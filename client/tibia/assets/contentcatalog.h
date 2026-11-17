#pragma once

#include "assets/assetcatalog.h"

#include <memory>

namespace tibia::sprites {
class TSpritesheetsMetadataManager;
}

namespace tibia {
namespace assets {

/// The client's asset catalog: its sprite entries become the spritesheets
/// the sprites are read from (CIP: TContentCatalog).
class TContentCatalog final : public TAssetCatalog
{
public:
    TContentCatalog();

    /// The spritesheets of the catalog's sprite entries.
    const std::shared_ptr<sprites::TSpritesheetsMetadataManager>& getSpritesheetsMetadataManager() const
    {
        return m_spritesheetsMetadataManager;
    }

protected:
    void assetRead(const TAsset& asset, const QJsonObject& json) override;

private:
    /// A sprite entry with numeric first / last sprite id and sprite type becomes a spritesheet.
    void addSpritesheet(const QString& file, const QJsonObject& json);

    std::shared_ptr<sprites::TSpritesheetsMetadataManager> m_spritesheetsMetadataManager;
};

}
}
