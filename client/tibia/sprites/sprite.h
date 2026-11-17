#pragma once

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <cstdint>
#include <memory>
#include <string>

class SpriteSheet;

namespace tibia {
namespace sprites {

/// CIP `sprites::IResourceSpriteIdentifier`.
class IResourceSpriteIdentifier : public ::tibia::CipTyped
{
public:
    virtual ~IResourceSpriteIdentifier() = default;
    virtual uint32_t spriteId() const = 0;
};

/// CIP sprites::TSprite — one sprite id (possibly in a sheet).
class TSprite : public IResourceSpriteIdentifier
{
public:
    explicit TSprite(uint32_t spriteId = 0);

    uint32_t spriteId() const override { return m_spriteId; }
    bool isValid() const { return m_spriteId != 0; }

private:
    uint32_t m_spriteId = 0;
};

class TSingleSprite final : public TSprite
{
public:
    using TSprite::TSprite;
};

class TSpriteInSpritesheet final : public TSprite
{
public:
    TSpriteInSpritesheet(uint32_t spriteId, SpriteSheet* sheet);

    SpriteSheet* sheet() const { return m_sheet; }

private:
    SpriteSheet* m_sheet = nullptr;
};

/// CIP sprites::TSpritesheetCache — decode cache over assets::SpriteStore.
class TSpritesheetCache final : public ::tibia::CipTyped
{
public:
    static TSpritesheetCache& instance();

    SpriteSheet* sheetForSprite(uint32_t spriteId, bool load = true);
    TSpriteInSpritesheet resolve(uint32_t spriteId, bool load = true);
};

class TSpritesheetDecompressorSynchronous final : public ::tibia::CipTyped
{
public:
    bool decompress(SpriteSheet* sheet) const;
};

/// Promoted cip:: moc managers.
class TResourceSpritesManager final : public cip::tibia::sprites::TResourceSpritesManager
{
public:
    using cip::tibia::sprites::TResourceSpritesManager::TResourceSpritesManager;
};

class TSpriteResolver final : public cip::tibia::sprites::TSpriteResolver
{
public:
    using cip::tibia::sprites::TSpriteResolver::TSpriteResolver;
};

class TSpritesheetDecompressor final : public cip::tibia::sprites::TSpritesheetDecompressor
{
public:
    using cip::tibia::sprites::TSpritesheetDecompressor::TSpritesheetDecompressor;
};

class TSpritesheetDecompressorThreadpool final : public cip::tibia::sprites::TSpritesheetDecompressorThreadpool
{
public:
    using cip::tibia::sprites::TSpritesheetDecompressorThreadpool::TSpritesheetDecompressorThreadpool;
};

class TSpritesheetManager final : public cip::tibia::sprites::TSpritesheetManager
{
public:
    using cip::tibia::sprites::TSpritesheetManager::TSpritesheetManager;
};

}
}
