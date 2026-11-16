#pragma once

// CIP-named shared surfaces (promoted from cipwire).

#include "shared/cip_typed.h"
#include "clockservice.h"
#include "coordinate.h"

#include <cip_gen.h>

#include <QAuthenticator>
#include <QByteArray>
#include <QElapsedTimer>
#include <QNetworkReply>
#include <QObject>

#include <string_view>
#include <QString>
#include <QtGlobal>
#include <QUrl>

#include <array>
#include <cstdint>
#include <vector>

namespace tibia {
namespace shared {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);

class TDynamicResourcesHelper final : public cip::tibia::shared::TDynamicResourcesHelper
{
    Q_OBJECT
public:
    using cip::tibia::shared::TDynamicResourcesHelper::TDynamicResourcesHelper;
};

class TExperienceGainChangeTracker final : public ::tibia::CipTyped {
public:
    TExperienceGainChangeTracker() = default;
};

class TImagestripResponse final : public ::tibia::CipTyped {
public:
    TImagestripResponse() = default;
};

class TNullExperienceGainChangeTracker final : public ::tibia::CipTyped {
public:
    TNullExperienceGainChangeTracker() = default;
};

class TNullExperiencePerHourProvider final : public ::tibia::CipTyped {
public:
    TNullExperiencePerHourProvider() = default;
};

// TOptimized1PixelBorderImageProvider — live QQuickImageProvider in
// imageproviders/optimizedborderimageprovider.h (CipSoft requestImage).

class TTibiaClipboardHelper final : public ::tibia::CipTyped {
public:
    TTibiaClipboardHelper() = default;
};

/// CIP `shared::THashHelper` — CryptoPP SHA256 in the binary; product uses Qt.
class THashHelper final : public ::tibia::CipTyped
{
public:
    THashHelper() = default;
    static QByteArray sha256(const QByteArray& data);
};

/// CIP `shared::TRsaHelper` — RSA with the game server's public key (rsahelper.cpp).
class TRsaHelper final : public ::tibia::CipTyped
{
public:
    TRsaHelper() = default;
    /// Installs the login key (Crystal's modulus, exponent 65537).
    static void installLoginKey();
    static void setPublicKey(std::string_view modulusDecimal, std::string_view exponentDecimal);
    /// Bytes per RSA block (0 before a key is set).
    static int blockSize();
    /// Encrypts one block in place; false when `size` is not blockSize().
    static bool encrypt(uint8_t* block, int size);
};

/// CIP `shared::TRandomNumberGenerator` (uniform int in [min,max]).
class TRandomNumberGenerator final : public ::tibia::CipTyped
{
public:
    TRandomNumberGenerator() = default;
    /// CIP asserts Min <= Max (randomnumbergenerator.h).
    static quint32 bounded(quint32 minInclusive, quint32 maxInclusive);
};

/// CIP `shared::TLocalFileInfo`: absolute path (+0x8) and size (+0x20).
class TLocalFileInfo final : public ::tibia::CipTyped
{
public:
    TLocalFileInfo() = default;
    explicit TLocalFileInfo(QString path, qint64 size = 0)
        : m_path(std::move(path))
        , m_size(size)
    {
    }
    const QString& path() const { return m_path; }
    qint64 size() const { return m_size; }
    bool exists() const;

private:
    QString m_path;
    qint64 m_size = 0;
};

/// CIP `shared::TDecompressionError`.
class TDecompressionError final : public ::tibia::CipTyped
{
public:
    explicit TDecompressionError(QString message = {})
        : m_message(std::move(message))
    {
    }
    const QString& message() const { return m_message; }

private:
    QString m_message;
};

/// CIP `shared::TReadOnlyMemoryBlob`.
class TReadOnlyMemoryBlob final : public ::tibia::CipTyped
{
public:
    TReadOnlyMemoryBlob() = default;
    explicit TReadOnlyMemoryBlob(QByteArray data)
        : m_data(std::move(data))
    {
    }
    const QByteArray& data() const { return m_data; }
    qsizetype size() const { return m_data.size(); }

private:
    QByteArray m_data;
};

/// CIP `shared::TLZMACompressedReadOnlyMemoryBlob`.
class TLZMACompressedReadOnlyMemoryBlob final : public ::tibia::CipTyped
{
public:
    TLZMACompressedReadOnlyMemoryBlob() = default;
    explicit TLZMACompressedReadOnlyMemoryBlob(QByteArray compressed)
        : m_compressed(std::move(compressed))
    {
    }
    const QByteArray& compressed() const { return m_compressed; }

private:
    QByteArray m_compressed;
};

/// CIP zlib wrappers — game wire inflate lives on `TGameserverNetworkPacketConnection`.
class TZlibBaseWrapper final : public ::tibia::CipTyped
{
public:
    TZlibBaseWrapper() = default;
};
class TZlibInflateWrapper final : public ::tibia::CipTyped
{
public:
    TZlibInflateWrapper() = default;
};
class TZlibDeflateWrapper final : public ::tibia::CipTyped
{
public:
    TZlibDeflateWrapper() = default;
};
class TCompressionHelper final : public ::tibia::CipTyped
{
public:
    TCompressionHelper() = default;
};

}
}


