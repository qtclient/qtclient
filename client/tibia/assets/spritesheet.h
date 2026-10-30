#pragma once

#include <framework/core/timer.h>
#include <framework/graphics/declarations.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

enum class SpriteLayout
{
    SIZE_32_32 = 0,
    SIZE_32_64 = 1,
    SIZE_64_32 = 2,
    SIZE_64_64 = 3,
    SIZE_32_96 = 4,
    SIZE_32_128 = 5,
    SIZE_32_192 = 6,
    SIZE_32_384 = 7,
    SIZE_64_96 = 8,
    SIZE_64_128 = 9,
    SIZE_64_192 = 10,
    SIZE_64_384 = 11,
    SIZE_96_32 = 12,
    SIZE_96_64 = 13,
    SIZE_96_96 = 14,
    SIZE_96_128 = 15,
    SIZE_96_192 = 16,
    SIZE_96_384 = 17,
    SIZE_128_32 = 18,
    SIZE_128_64 = 19,
    SIZE_128_96 = 20,
    SIZE_128_128 = 21,
    SIZE_128_192 = 22,
    SIZE_128_384 = 23,
    SIZE_192_32 = 24,
    SIZE_192_64 = 25,
    SIZE_192_96 = 26,
    SIZE_192_128 = 27,
    SIZE_192_192 = 28,
    SIZE_192_384 = 29,
    SIZE_384_32 = 30,
    SIZE_384_64 = 31,
    SIZE_384_96 = 32,
    SIZE_384_128 = 33,
    SIZE_384_192 = 34,
    SIZE_384_384 = 35
};

enum class SpriteLoadState
{
    NONE,
    LOADING,
    LOADED
};

class SpriteSheet
{
public:
    static constexpr uint16_t SIZE = 384;

    SpriteSheet(int firstId, int lastId, SpriteLayout spriteLayout, std::string file)
        : firstId(firstId), lastId(lastId), spriteLayout(spriteLayout), file(std::move(file))
    {
    }

    Size getSpriteSize() const;
    int getSpritesPerSheet() const;
    int getColumns() const { return SIZE / getSpriteSize().width(); }

    int firstId = 0;
    int lastId = 0;
    SpriteLayout spriteLayout = SpriteLayout::SIZE_32_32;
    std::atomic<SpriteLoadState> m_loadingState = SpriteLoadState::NONE;
    std::unique_ptr<uint8_t[]> data;
    std::string file;
    Timer m_lastTimeUsage;
};

namespace tibia::assets {

class SpriteStore
{
public:
    void clear();
    void setPath(std::string path) { m_path = std::move(path); }
    const std::string& path() const { return m_path; }
    void addSheet(const SpriteSheetPtr& sheet);
    void setSpritesCount(int count) { m_spritesCount = count; }
    int spritesCount() const { return m_spritesCount; }

    bool loadSheet(const SpriteSheetPtr& sheet) const;
    SpriteSheetPtr sheetBySpriteId(int id, bool& loading, bool load = true);
    ImagePtr spriteImage(int id, bool& loading);

private:
    int m_spritesCount = 0;
    std::vector<SpriteSheetPtr> m_sheets;
    std::string m_path;
};

SpriteStore& sprites();

}
