#include "spritesheet.h"

#include "shared/tibiafilesystemhelper.h"

#include "lzma.h"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <cstring>

namespace {

// A sheet is 384x384 RGBA; the LZMA stream holds a BMP of it (122-byte header).
constexpr int kBytesInSpriteSheet = 384 * 384 * 4;
constexpr int kLzmaUncompressedSize = kBytesInSpriteSheet + 122;
constexpr int kSpriteSheetWidthBytes = 384 * 4;

}

namespace {

QSize spriteSizeOf(const SpriteLayout layout)
{
    static const std::array<QSize, 36> sizes = {
        QSize(32, 32), QSize(32, 64), QSize(64, 32), QSize(64, 64),
        QSize(32, 96), QSize(32, 128), QSize(32, 192), QSize(32, 384),
        QSize(64, 96), QSize(64, 128), QSize(64, 192), QSize(64, 384),
        QSize(96, 32), QSize(96, 64), QSize(96, 96), QSize(96, 128), QSize(96, 192), QSize(96, 384),
        QSize(128, 32), QSize(128, 64), QSize(128, 96), QSize(128, 128), QSize(128, 192), QSize(128, 384),
        QSize(192, 32), QSize(192, 64), QSize(192, 96), QSize(192, 128), QSize(192, 192), QSize(192, 384),
        QSize(384, 32), QSize(384, 64), QSize(384, 96), QSize(384, 128), QSize(384, 192), QSize(384, 384)
    };
    const auto index = static_cast<size_t>(layout);
    return index < sizes.size() ? sizes[index] : sizes[0];
}

}

QSize SpriteSheet::getSpriteSize() const
{
    return spriteSizeOf(spriteLayout);
}

int SpriteSheet::getSpritesPerSheet() const
{
    const QSize& size = getSpriteSize();
    return getColumns() * (SIZE / size.height());
}

namespace tibia::assets {

namespace {
/// TU-private catalog — not a qtclient `g_*` hub; callers use `sprites()`.
SpriteStore spriteStore;
}

SpriteStore& sprites()
{
    return spriteStore;
}

void SpriteStore::clear()
{
    m_spritesCount = 0;
    m_sheets.clear();
    {
        const std::lock_guard lock(m_cacheMutex);
        m_cachedSheet.reset();
    }
    m_path.clear();
}

void SpriteStore::addSheet(const SpriteSheetPtr& sheet)
{
    if (std::ranges::any_of(m_sheets, [&sheet](const SpriteSheetPtr& stored) { return stored->file == sheet->file; }))
        throw std::invalid_argument("Spritesheet already stored");
    const auto at = std::ranges::upper_bound(m_sheets, sheet->firstId, {}, [](const SpriteSheetPtr& stored) { return stored->firstId; });
    m_sheets.insert(at, sheet);
}

SpriteSheetPtr SpriteStore::findSheet(const int id)
{
    const std::lock_guard lock(m_cacheMutex);
    if (m_cachedSheet && m_cachedSheet->firstId <= id && id <= m_cachedSheet->lastId)
        return m_cachedSheet;
    const auto it = std::ranges::lower_bound(m_sheets, id, {}, [](const SpriteSheetPtr& stored) { return stored->lastId; });
    m_cachedSheet = it != m_sheets.end() ? *it : nullptr;
    return m_cachedSheet;
}

bool SpriteStore::loadSheet(const SpriteSheetPtr& sheet) const
{
    if (!sheet)
        return false;
    if (sheet->m_loadingState.load(std::memory_order_acquire) == SpriteLoadState::LOADING)
        return false;
    if (sheet->data)
        return true;
    if (sheet->m_loadingState.exchange(SpriteLoadState::LOADING, std::memory_order_acq_rel) == SpriteLoadState::LOADING)
        return false;

    try {
        const auto path = std::format("{}{}", m_path, sheet->file);
        if (!tibia::files::data().fileExists(path)) {
            sheet->m_loadingState.store(SpriteLoadState::NONE, std::memory_order_release);
            return false;
        }

        const std::string file = tibia::files::data().readFileContents(path);
        const auto* bytes = reinterpret_cast<const uint8_t*>(file.data());
        const size_t size = file.size();
        size_t at = 0;
        const auto next = [&]() -> uint8_t {
            if (at >= size)
                throw std::runtime_error("truncated sprite sheet");
            return bytes[at++];
        };

        thread_local static std::array<uint8_t, kLzmaUncompressedSize> decompressBuffer;

        // CIP header: zero padding, u32 constant, a 7-bit varint size, then the LZMA properties.
        while (next() == 0x00)
            ;
        at += 4;
        while ((next() & 0x80) == 0x80)
            ;

        const uint8_t lclppb = next();
        lzma_options_lzma options{};
        options.lc = lclppb % 9;
        const int remainder = lclppb / 9;
        options.lp = remainder % 5;
        options.pb = remainder / 5;

        uint32_t dictionarySize = 0;
        for (uint8_t i = 0; i < 4; ++i)
            dictionarySize += next() << (i * 8);

        constexpr uint32_t maxDictionarySize = std::max<uint32_t>(kLzmaUncompressedSize, 1024 * 1024);
        options.dict_size = std::min(dictionarySize, maxDictionarySize);
        at += 8;
        if (at > size)
            throw std::runtime_error("truncated sprite sheet");

        lzma_stream stream = LZMA_STREAM_INIT;
        const lzma_filter filters[2] = {
            lzma_filter{ LZMA_FILTER_LZMA1, &options },
            lzma_filter{ LZMA_VLI_UNKNOWN, nullptr }
        };
        if (lzma_raw_decoder(&stream, filters) != LZMA_OK) {
            sheet->m_loadingState.store(SpriteLoadState::NONE, std::memory_order_release);
            return false;
        }

        stream.next_in = bytes + at;
        stream.avail_in = size - at;
        stream.next_out = decompressBuffer.data();
        stream.avail_out = decompressBuffer.size();
        const auto result = lzma_code(&stream, LZMA_RUN);
        lzma_end(&stream);
        if (result != LZMA_STREAM_END)
            throw std::runtime_error("LZMA decompression failed");

        const uint8_t* bmpOffsetPtr = decompressBuffer.data() + 10;
        const uint32_t bmpDataOffset = bmpOffsetPtr[0] | (bmpOffsetPtr[1] << 8) | (bmpOffsetPtr[2] << 16) | (bmpOffsetPtr[3] << 24);
        if (bmpDataOffset + kBytesInSpriteSheet > kLzmaUncompressedSize)
            throw std::runtime_error("sprite sheet image offset out of bounds");

        uint8_t* bufferStart = decompressBuffer.data() + bmpDataOffset;
        for (int i = 0; i < kBytesInSpriteSheet; i += 4) {
            std::swap(bufferStart[i], bufferStart[i + 2]);
            const uint32_t rgb = bufferStart[i] | (bufferStart[i + 1] << 8) | (bufferStart[i + 2] << 16);
            if (rgb == 0xFF00FF) {
                bufferStart[i] = 0;
                bufferStart[i + 1] = 0;
                bufferStart[i + 2] = 0;
                bufferStart[i + 3] = 0;
            }
        }

        constexpr int halfHeight = SpriteSheet::SIZE / 2;
        uint8_t tempLine[kSpriteSheetWidthBytes];
        for (int y = 0; y < halfHeight; ++y) {
            uint8_t* top = bufferStart + y * kSpriteSheetWidthBytes;
            uint8_t* bottom = bufferStart + (SpriteSheet::SIZE - 1 - y) * kSpriteSheetWidthBytes;
            std::memcpy(tempLine, top, kSpriteSheetWidthBytes);
            std::memcpy(top, bottom, kSpriteSheetWidthBytes);
            std::memcpy(bottom, tempLine, kSpriteSheetWidthBytes);
        }

        sheet->data = std::make_unique<uint8_t[]>(kBytesInSpriteSheet);
        std::memcpy(sheet->data.get(), bufferStart, kBytesInSpriteSheet);
        sheet->m_loadingState.store(SpriteLoadState::LOADED, std::memory_order_release);
        return true;
    } catch (const std::exception& e) {
        sheet->m_loadingState.store(SpriteLoadState::NONE, std::memory_order_release);
        qCritical("%s", std::format("Failed to load sprite sheet '{}': {}", sheet->file, e.what()).c_str());
        return false;
    }
}

SpriteSheetPtr SpriteStore::sheetBySpriteId(const int id, bool& loading, const bool load)
{
    loading = false;
    if (id == 0)
        return nullptr;

    const SpriteSheetPtr sheet = findSheet(id);
    if (!sheet)
        return nullptr;
    if (!load) {
        loading = sheet->m_loadingState.load(std::memory_order_acquire) == SpriteLoadState::LOADING;
        return sheet;
    }
    if (!loadSheet(sheet)) {
        loading = sheet->m_loadingState.load(std::memory_order_acquire) == SpriteLoadState::LOADING;
        return nullptr;
    }
    if (sheet->data)
        sheet->m_lastTimeUsage.restart();
    return sheet;
}

QImage SpriteStore::spriteImage(const int id, bool& loading)
{
    try {
        const SpriteSheetPtr sheet = sheetBySpriteId(id, loading, true);
        if (!sheet)
            return {};

        const QSize size = sheet->getSpriteSize();
        QImage image(size.width(), size.height(), QImage::Format_RGBA8888);
        uint8_t* pixelData = image.bits();

        const int spriteOffset = id - sheet->firstId;
        const int columns = sheet->getColumns();
        const int spritesPerSheet = sheet->getSpritesPerSheet();
        if (spriteOffset < 0 || spriteOffset >= spritesPerSheet)
            return {};

        const int spriteRow = spriteOffset / columns;
        const int spriteColumn = spriteOffset % columns;
        const int spriteWidthBytes = size.width() * 4;
        for (int height = size.height() * spriteRow, offset = 0; height < size.height() + spriteRow * size.height(); ++height, ++offset) {
            std::memcpy(&pixelData[offset * spriteWidthBytes],
                        &sheet->data[(height * kSpriteSheetWidthBytes) + (spriteColumn * spriteWidthBytes)],
                        spriteWidthBytes);
        }
        return image;
    } catch (const std::exception& e) {
        qCritical("%s", std::format("Failed to get sprite id {}: {}", id, e.what()).c_str());
        return {};
    }
}

}
