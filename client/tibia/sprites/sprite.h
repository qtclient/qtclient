#pragma once

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <cstdint>
#include <memory>
#include <string>

class SpriteSheet;

namespace tibia::sprites {

/// CIP sprites::TSprite — one sprite id (possibly in a sheet).
class TSprite : public ::tibia::CipTyped
{
public:
    explicit TSprite(uint32_t spriteId = 0);

    uint32_t spriteId() const { return m_spriteId; }
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

/// CIP sprites::TQtTextureAtlasTextureCreator — atlas insert path for TSpriteTextureCache.
class TQtTextureAtlasTextureCreator final : public ::tibia::CipTyped
{
public:
    explicit TQtTextureAtlasTextureCreator(bool allowEvict = true);

    bool allowEvict() const { return m_allowEvict; }

private:
    bool m_allowEvict = true;
};

/// Sheet / resource identifiers used by catalog + minimap + effects wrappers.
class TAppearanceTypeSpritesheetMapping final : public ::tibia::CipTyped
{
public:
    TAppearanceTypeSpritesheetMapping(uint16_t appearanceId, uint32_t firstSpriteId);

    uint16_t appearanceId() const { return m_appearanceId; }
    uint32_t firstSpriteId() const { return m_firstSpriteId; }

private:
    uint16_t m_appearanceId = 0;
    uint32_t m_firstSpriteId = 0;
};

class TAssetSpritesheetIdentifier final : public ::tibia::CipTyped
{
public:
    explicit TAssetSpritesheetIdentifier(std::string file);

    const std::string& file() const { return m_file; }

private:
    std::string m_file;
};

class TEffectWrapperSpritesheetIdentifier final : public ::tibia::CipTyped
{
public:
    explicit TEffectWrapperSpritesheetIdentifier(uint32_t effectSpriteId);

    uint32_t effectSpriteId() const { return m_effectSpriteId; }

private:
    uint32_t m_effectSpriteId = 0;
};

class TImageResourceSpriteIdentifier final : public ::tibia::CipTyped
{
public:
    explicit TImageResourceSpriteIdentifier(std::string resourcePath);

    const std::string& resourcePath() const { return m_resourcePath; }

private:
    std::string m_resourcePath;
};

class TMinimapTileSpritesheetIdentifier final : public ::tibia::CipTyped
{
public:
    explicit TMinimapTileSpritesheetIdentifier(uint32_t tileSpriteId);

    uint32_t tileSpriteId() const { return m_tileSpriteId; }

private:
    uint32_t m_tileSpriteId = 0;
};

class TStaticMinimapMarkerSymbolResourceSpriteIdentifier final : public ::tibia::CipTyped
{
public:
    explicit TStaticMinimapMarkerSymbolResourceSpriteIdentifier(std::string resourcePath);

    const std::string& resourcePath() const { return m_resourcePath; }

private:
    std::string m_resourcePath;
};

class TSpritesheetsMetadataManager final : public ::tibia::CipTyped
{
public:
    static TSpritesheetsMetadataManager& instance();

    int spritesCount() const;
};

class TSpritesheetDecompressorSynchronous final : public ::tibia::CipTyped
{
public:
    bool decompress(SpriteSheet* sheet) const;
};

class TSpritesheetDecompressorConcurrentException final : public ::tibia::CipTyped
{
public:
    explicit TSpritesheetDecompressorConcurrentException(std::string message);

    const std::string& message() const { return m_message; }

private:
    std::string m_message;
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
