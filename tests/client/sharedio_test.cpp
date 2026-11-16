#include <gtest/gtest.h>

#include "bundle.h"
#include "compressionhelper.h"
#include "decompressionerror.h"
#include "filesystemhelper.h"
#include "hashhelper.h"
#include "localfileinfo.h"
#include "lzmacompressedreadonlymemoryblob.h"
#include "randomnumbergenerator.h"
#include "readonlymemoryblob.h"
#include "rsahelper.h"
#include "zlibdeflatewrapper.h"
#include "zlibinflatewrapper.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <stdexcept>
#include <string>
#include <typeinfo>

using namespace tibia::shared;

namespace {

std::vector<unsigned char> bytes(const QByteArray& data)
{
    return { data.begin(), data.end() };
}

QByteArray toByteArray(const std::vector<unsigned char>& data)
{
    return QByteArray(reinterpret_cast<const char*>(data.data()), qsizetype(data.size()));
}

std::string varint(size_t value)
{
    std::string out;
    do {
        unsigned char byte = value & 0x7f;
        value >>= 7;
        if (value)
            byte |= 0x80;
        out.push_back(char(byte));
    } while (value);
    return out;
}

// A bundle container around `payload` with codec `codec`.
std::string bundleOf(unsigned char codec, size_t unpackedSize, const std::string& payload)
{
    return std::string("p") + char(codec) + varint(unpackedSize) + varint(payload.size()) + payload;
}

std::string rawDeflate(const QByteArray& data)
{
    TZlibDeflateWrapper deflater;
    EXPECT_EQ(deflater.init(15, 8, 6, true), Z_OK);
    std::vector<unsigned char> out;
    EXPECT_EQ(deflater.deflate(reinterpret_cast<const unsigned char*>(data.constData()), unsigned(data.size()), out),
              Z_OK);
    return std::string(out.begin(), out.end());
}

void writeFile(const QString& path, const QByteArray& data)
{
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write(data);
}

}

TEST(SharedZlib, StreamingMessagesDropAndRestoreTheSyncFlushTrailer)
{
    TZlibDeflateWrapper deflater;
    ASSERT_EQ(deflater.init(15, 8, 6, false), Z_OK);
    EXPECT_TRUE(deflater.isInitialized());
    EXPECT_EQ(deflater.getWindowBits(), 15);
    EXPECT_EQ(deflater.getMemLevel(), 8);
    EXPECT_EQ(deflater.getLevel(), 6);
    TZlibInflateWrapper inflater;
    ASSERT_EQ(inflater.init(15, false), Z_OK);

    for (const QByteArray message : { QByteArray("hello hello hello"), QByteArray("second message, hello") }) {
        std::vector<unsigned char> packed;
        ASSERT_EQ(deflater.deflate(reinterpret_cast<const unsigned char*>(message.constData()),
                                   unsigned(message.size()), packed),
                  Z_OK);
        ASSERT_GE(packed.size(), 1u);
        // The SYNC_FLUSH trailer is not part of the message.
        const QByteArray tail = toByteArray(packed).right(4);
        EXPECT_NE(tail, QByteArray::fromHex("0000ffff"));
        std::vector<unsigned char> unpacked;
        ASSERT_EQ(inflater.inflate(packed.data(), unsigned(packed.size()), unpacked), Z_OK);
        EXPECT_EQ(toByteArray(unpacked), message);
    }
}

TEST(SharedZlib, OneShotRoundTripResetsBetweenMessages)
{
    TCompressionHelper helper;
    EXPECT_FALSE(helper.isInitialized());
    helper.init(true);
    EXPECT_TRUE(helper.isInitialized());
    const QByteArray first(100000, 'a');
    const QByteArray second("another, independent stream");
    const std::string a = rawDeflate(first);
    const std::string b = rawDeflate(second);
    EXPECT_EQ(helper.decompress(QByteArray::fromStdString(a)), first);
    EXPECT_EQ(helper.decompress(QByteArray::fromStdString(b)), second);
}

TEST(SharedZlib, ErrorsAreRecordedWithTheirCode)
{
    TZlibDeflateWrapper deflater;
    std::vector<unsigned char> out;
    const unsigned char input[1] = { 1 };
    EXPECT_EQ(deflater.deflate(input, 1, out), Z_STREAM_ERROR);
    EXPECT_TRUE(out.empty());
    EXPECT_EQ(deflater.getLastErrorMessage(), "Stream not initialized or already deleted (-2)");

    TZlibInflateWrapper inflater;
    ASSERT_EQ(inflater.init(15, false), Z_OK);
    EXPECT_EQ(inflater.init(15, false), Z_OK);
    EXPECT_EQ(inflater.getLastErrorMessage(), "TZlibInflateWrapper::init: already initialized (-2)");
}

TEST(SharedBundle, UnpacksRawAndDeflateAndPassesOtherDataThrough)
{
    const std::string plain = "plain bytes";
    std::string out;
    EXPECT_FALSE(bundle::isPacked(plain));
    EXPECT_FALSE(bundle::unpack(out, plain));
    EXPECT_EQ(out, plain);

    const std::string raw = std::string(3, '\0') + bundleOf(0, plain.size(), plain);
    EXPECT_TRUE(bundle::isPacked(raw));
    EXPECT_TRUE(bundle::unpack(out, raw));
    EXPECT_EQ(out, plain);

    const QByteArray text(5000, 'z');
    const std::string deflated = bundleOf(3, size_t(text.size()), rawDeflate(text));
    EXPECT_TRUE(bundle::unpack(out, deflated));
    EXPECT_EQ(QByteArray::fromStdString(out), text);

    // LZMA20: an lzma-alone stream (props, size, data).
    const QByteArray lzma = QByteArray::fromHex(
        "5d00008000ffffffffffffffff00361e89dd7d4919e5b132193ebf18255d8c6c9a53fa6f36ef788c951bfffff1ae4000");
    EXPECT_TRUE(bundle::unpack(out, bundleOf(5, 400, lzma.toStdString())));
    EXPECT_EQ(QByteArray::fromStdString(out), QByteArray("lzma bundle payload ").repeated(20));

    // A codec this build does not decode (LZIP) fails and returns the input.
    const std::string lzip = bundleOf(4, 4, "abcd");
    EXPECT_FALSE(bundle::unpack(out, lzip));
    EXPECT_EQ(out, lzip);
}

TEST(SharedBundle, DecompressBundleThrowsOnBrokenPackedData)
{
    EXPECT_EQ(decompressBundle("not packed"), QByteArray("not packed"));
    const std::string raw = bundleOf(0, 3, "abc");
    EXPECT_EQ(decompressBundle(QByteArray::fromStdString(raw)), QByteArray("abc"));
    const std::string broken = bundleOf(4, 4, "abcd");
    EXPECT_THROW(decompressBundle(QByteArray::fromStdString(broken)), TDecompressionError);
    try {
        decompressBundle(QByteArray::fromStdString(broken));
    } catch (const TDecompressionError& error) {
        EXPECT_STREQ(error.what(), "Invalid input data could not be decompressed");
    }
}

TEST(SharedBlob, FileBlobsReadOnEveryCallAndLzmaBlobsUnpack)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString plainPath = dir.filePath("asset.dat");
    const QString packedPath = dir.filePath("other.dat.lzma");
    writeFile(plainPath, "first");
    writeFile(packedPath, QByteArray::fromStdString(bundleOf(0, 6, "packed")));

    TFileSystemHelper fileSystem;
    const auto plain = fileSystem.openReadOnlyMemoryBlob(plainPath);
    const TReadOnlyMemoryBlob& plainBlob = *plain;
    EXPECT_EQ(typeid(plainBlob), typeid(TReadOnlyMemoryBlob));
    EXPECT_EQ(plain->getData(), QByteArray("first"));
    writeFile(plainPath, "second");
    EXPECT_EQ(plain->getData(), QByteArray("second"));
    EXPECT_EQ(plain->getFilePath(), plainPath);

    // "<path>.lzma" is used when "<path>" does not exist.
    const auto packed = fileSystem.openReadOnlyMemoryBlob(dir.filePath("other.dat"));
    const TReadOnlyMemoryBlob& packedBlob = *packed;
    EXPECT_EQ(typeid(packedBlob), typeid(TLZMACompressedReadOnlyMemoryBlob));
    EXPECT_EQ(packed->getFilePath(), packedPath);
    EXPECT_EQ(packed->getData(), QByteArray("packed"));

    EXPECT_THROW(fileSystem.openReadOnlyMemoryBlob(dir.filePath("missing")), std::runtime_error);

    const TReadOnlyMemoryBlob memory(QByteArray("in memory"));
    EXPECT_EQ(memory.getData(), QByteArray("in memory"));
}

TEST(SharedLocalFileInfo, ReportsPathNameAndSize)
{
    const TLocalFileInfo info(QStringLiteral("/tmp/dir/file.txt"), 42);
    EXPECT_EQ(info.getFilePath(), QStringLiteral("/tmp/dir/file.txt"));
    EXPECT_EQ(info.getFileName(), QStringLiteral("file.txt"));
    EXPECT_EQ(info.getSize(), 42);
}

TEST(SharedHash, Sha256AndSha1)
{
    const THashHelper hash;
    const QByteArray sha256 = hash.getSha256Hash("abc");
    EXPECT_EQ(sha256.toHex(), QByteArray("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    EXPECT_EQ(hash.getSha1Hash("abc").toHex(), QByteArray("a9993e364706816aba3e25717850c26c9cd0d89d"));
    EXPECT_TRUE(hash.isSha256Hash(sha256, "abc"));
    EXPECT_FALSE(hash.isSha256Hash(sha256, "abd"));
}

TEST(SharedRsa, EncryptsOneBigEndianBlockAndPadsShortData)
{
    TRsaHelper rsa;
    // n = 4294967291 * 4294967279, e = 65537.
    rsa.setPublicKey(QByteArray::fromHex("ffffffea00000055"));
    EXPECT_EQ(rsa.encrypt(QByteArray::fromHex("0011223344556677")).toHex(), QByteArray("6ed809d513bffa55"));
    const QByteArray a = rsa.encrypt(QByteArray::fromHex("00aa"));
    const QByteArray b = rsa.encrypt(QByteArray::fromHex("00aa"));
    EXPECT_EQ(a.size(), 8);
    EXPECT_NE(a, b);

    EXPECT_THROW(rsa.setPublicKey(QByteArray::fromHex("0100")), std::runtime_error);
}

TEST(SharedRandom, InstanceIsShared)
{
    EXPECT_NE(TRandomNumberGenerator::instance(), nullptr);
    EXPECT_EQ(TRandomNumberGenerator::instance(), TRandomNumberGenerator::instance());
}

TEST(SharedRandom, StaysInsideTheRange)
{
    TRandomNumberGenerator random;
    for (int i = 0; i < 1000; ++i) {
        const int value = random.getRandomInt(-3, 3);
        EXPECT_GE(value, -3);
        EXPECT_LE(value, 3);
        const float real = random.getRandomFloat(1.0f, 2.0f);
        EXPECT_GE(real, 1.0f);
        EXPECT_LE(real, 2.0f);
        EXPECT_EQ(random.getRandomLongLong(7, 7), 7);
        EXPECT_LE(random.getRandomUnsignedLong(0, 9), 9u);
        EXPECT_LE(random.getRandomNumber(0, 1), 1);
        EXPECT_GE(random.getRandomLong(5, 6), 5);
    }
}
