#include "spritesheet.h"

#include <framework/config.h>
#include <framework/core/filestream.h>
#include <framework/core/resourcemanager.h>
#include <framework/graphics/image.h>

#include "lzma.h"

#include <algorithm>
#include <array>
#include <cstring>

namespace {

Size spriteSizeOf(const SpriteLayout layout)
{
    static const std::array<Size, 36> sizes = {
        Size(32, 32), Size(32, 64), Size(64, 32), Size(64, 64),
        Size(32, 96), Size(32, 128), Size(32, 192), Size(32, 384),
        Size(64, 96), Size(64, 128), Size(64, 192), Size(64, 384),
        Size(96, 32), Size(96, 64), Size(96, 96), Size(96, 128), Size(96, 192), Size(96, 384),
        Size(128, 32), Size(128, 64), Size(128, 96), Size(128, 128), Size(128, 192), Size(128, 384),
        Size(192, 32), Size(192, 64), Size(192, 96), Size(192, 128), Size(192, 192), Size(192, 384),
        Size(384, 32), Size(384, 64), Size(384, 96), Size(384, 128), Size(384, 192), Size(384, 384)
    };
    const auto index = static_cast<size_t>(layout);
    return index < sizes.size() ? sizes[index] : sizes[0];
}

}

Size SpriteSheet::getSpriteSize() const
{
    return spriteSizeOf(spriteLayout);
}

int SpriteSheet::getSpritesPerSheet() const
{
    const Size& size = getSpriteSize();
    return getColumns() * (SIZE / size.height());
}

namespace tibia::assets {

namespace {
SpriteStore g_spritesStore;
}

SpriteStore& sprites()
{
    return g_spritesStore;
}

void SpriteStore::clear()
{
    m_spritesCount = 0;
    m_sheets.clear();
    m_path.clear();
}

void SpriteStore::addSheet(const SpriteSheetPtr& sheet)
{
    m_sheets.push_back(sheet);
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
        const auto path = fmt::format("{}{}", m_path, sheet->file);
        if (!tibia::files::data().fileExists(path)) {
            sheet->m_loadingState.store(SpriteLoadState::NONE, std::memory_order_release);
            return false;
        }

        const auto fin = tibia::files::data().openFile(path);
        fin->cache(true);

        thread_local static std::array<uint8_t, LZMA_UNCOMPRESSED_SIZE> decompressBuffer;

        while (fin->getU8() == 0x00)
            ;
        fin->skip(4);
        while ((fin->getU8() & 0x80) == 0x80)
            ;

        const uint8_t lclppb = fin->getU8();
        lzma_options_lzma options{};
        options.lc = lclppb % 9;
        const int remainder = lclppb / 9;
        options.lp = remainder % 5;
        options.pb = remainder / 5;

        uint32_t dictionarySize = 0;
        for (uint8_t i = 0; i < 4; ++i)
            dictionarySize += fin->getU8() << (i * 8);

        constexpr uint32_t maxDictionarySize = std::max<uint32_t>(LZMA_UNCOMPRESSED_SIZE, 1024 * 1024);
        options.dict_size = std::min(dictionarySize, maxDictionarySize);
        fin->skip(8);

        lzma_stream stream = LZMA_STREAM_INIT;
        const lzma_filter filters[2] = {
            lzma_filter{ LZMA_FILTER_LZMA1, &options },
            lzma_filter{ LZMA_VLI_UNKNOWN, nullptr }
        };
        if (lzma_raw_decoder(&stream, filters) != LZMA_OK) {
            sheet->m_loadingState.store(SpriteLoadState::NONE, std::memory_order_release);
            return false;
        }

        stream.next_in = &fin->m_data[fin->tell()];
        stream.avail_in = fin->size() - fin->tell();
        stream.next_out = decompressBuffer.data();
        stream.avail_out = decompressBuffer.size();
        const auto result = lzma_code(&stream, LZMA_RUN);
        lzma_end(&stream);
        if (result != LZMA_STREAM_END)
            throw stdext::exception("LZMA decompression failed");

        const uint8_t* bmpOffsetPtr = decompressBuffer.data() + 10;
        const uint32_t bmpDataOffset = bmpOffsetPtr[0] | (bmpOffsetPtr[1] << 8) | (bmpOffsetPtr[2] << 16) | (bmpOffsetPtr[3] << 24);
        if (bmpDataOffset + BYTES_IN_SPRITE_SHEET > LZMA_UNCOMPRESSED_SIZE)
            throw stdext::exception("sprite sheet image offset out of bounds");

        uint8_t* bufferStart = decompressBuffer.data() + bmpDataOffset;
        for (int i = 0; i < BYTES_IN_SPRITE_SHEET; i += 4) {
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
        uint8_t tempLine[SPRITE_SHEET_WIDTH_BYTES];
        for (int y = 0; y < halfHeight; ++y) {
            uint8_t* top = bufferStart + y * SPRITE_SHEET_WIDTH_BYTES;
            uint8_t* bottom = bufferStart + (SpriteSheet::SIZE - 1 - y) * SPRITE_SHEET_WIDTH_BYTES;
            std::memcpy(tempLine, top, SPRITE_SHEET_WIDTH_BYTES);
            std::memcpy(top, bottom, SPRITE_SHEET_WIDTH_BYTES);
            std::memcpy(bottom, tempLine, SPRITE_SHEET_WIDTH_BYTES);
        }

        sheet->data = std::make_unique<uint8_t[]>(BYTES_IN_SPRITE_SHEET);
        std::memcpy(sheet->data.get(), bufferStart, BYTES_IN_SPRITE_SHEET);
        sheet->m_loadingState.store(SpriteLoadState::LOADED, std::memory_order_release);
        return true;
    } catch (const std::exception& e) {
        sheet->m_loadingState.store(SpriteLoadState::NONE, std::memory_order_release);
        g_logger.error("Failed to load sprite sheet '{}': {}", sheet->file, e.what());
        return false;
    }
}

SpriteSheetPtr SpriteStore::sheetBySpriteId(const int id, bool& loading, const bool load)
{
    loading = false;
    if (id == 0)
        return nullptr;

    const auto it = std::ranges::find_if(m_sheets, [id](const SpriteSheetPtr& sheet) {
        return sheet && id >= sheet->firstId && id <= sheet->lastId;
    });
    if (it == m_sheets.end())
        return nullptr;

    const SpriteSheetPtr sheet = *it;
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

ImagePtr SpriteStore::spriteImage(const int id, bool& loading)
{
    try {
        const SpriteSheetPtr sheet = sheetBySpriteId(id, loading, true);
        if (!sheet)
            return nullptr;

        const Size size = sheet->getSpriteSize();
        const auto image = std::make_shared<Image>(size);
        uint8_t* pixelData = image->getPixelData();

        const int spriteOffset = id - sheet->firstId;
        const int columns = sheet->getColumns();
        const int spritesPerSheet = sheet->getSpritesPerSheet();
        if (spriteOffset < 0 || spriteOffset >= spritesPerSheet)
            return nullptr;

        const int spriteRow = spriteOffset / columns;
        const int spriteColumn = spriteOffset % columns;
        const int spriteWidthBytes = size.width() * 4;
        for (int height = size.height() * spriteRow, offset = 0; height < size.height() + spriteRow * size.height(); ++height, ++offset) {
            std::memcpy(&pixelData[offset * spriteWidthBytes],
                        &sheet->data[(height * SPRITE_SHEET_WIDTH_BYTES) + (spriteColumn * spriteWidthBytes)],
                        spriteWidthBytes);
        }

        if (!image->hasTransparentPixel()) {
            uint8_t transparent = 0;
            const auto& pixels = image->getPixels();
            for (size_t i = 3; i < pixels.size(); i += 4) {
                if (pixels[i] == 0 && ++transparent > 4) {
                    image->setTransparentPixel(true);
                    break;
                }
            }
        }
        return image;
    } catch (const std::exception& e) {
        g_logger.error("Failed to get sprite id {}: {}", id, e.what());
        return nullptr;
    }
}

}
