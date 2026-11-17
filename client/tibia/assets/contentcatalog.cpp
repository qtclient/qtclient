#include "assets/contentcatalog.h"

#include "sprites/spritesheetsmetadatamanager.h"

namespace tibia::assets {

TContentCatalog::TContentCatalog()
    : m_spritesheetsMetadataManager(std::make_shared<sprites::TSpritesheetsMetadataManager>())
{
}

void TContentCatalog::assetRead(const TAsset& asset, const QJsonObject& json)
{
    if (asset.type() == TAsset::Sprite)
        addSpritesheet(asset.file(), json);
}

void TContentCatalog::addSpritesheet(const QString& file, const QJsonObject& json)
{
    const QJsonValue type = json.value(QLatin1String("spritetype"));
    const QJsonValue first = json.value(QLatin1String("firstspriteid"));
    const QJsonValue last = json.value(QLatin1String("lastspriteid"));
    if (!type.isDouble() || !first.isDouble() || !last.isDouble())
        return;
    m_spritesheetsMetadataManager->addSpritesheet(file, static_cast<uint32_t>(first.toInt()),
                                                  static_cast<uint32_t>(last.toInt()),
                                                  static_cast<uint32_t>(type.toInt()));
}

}
