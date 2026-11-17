#pragma once

#include "assets/assetcatalog.h"

namespace tibia {
namespace assets {

/// The client's asset catalog: its sprite entries become the spritesheets
/// the sprites are read from (CIP: TContentCatalog).
class TContentCatalog final : public TAssetCatalog
{
public:
    /// The highest sprite id of the spritesheets.
    int lastSpriteId() const { return m_lastSpriteId; }

protected:
    void assetRead(const TAsset& asset, const QJsonObject& json) override;

private:
    /// A sprite entry with numeric first / last sprite id and sprite type becomes a spritesheet.
    void addSpritesheet(const QString& file, const QJsonObject& json);

    int m_lastSpriteId = 0;
};

}
}
