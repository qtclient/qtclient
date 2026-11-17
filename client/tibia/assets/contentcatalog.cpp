#include "assets/contentcatalog.h"

#include "assets/spritesheet.h"
#include "sprites/sprite.h"
#include "sprites/spritesession.h"

#include <algorithm>

namespace tibia::assets {

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
    int lastSpriteId = last.toInt();
    auto sheet = std::make_shared<SpriteSheet>(first.toInt(), lastSpriteId, static_cast<SpriteLayout>(type.toInt()), file.toStdString());
    const int maxSpriteId = sheet->firstId + sheet->getSpritesPerSheet() - 1;
    if (lastSpriteId > maxSpriteId) {
        lastSpriteId = maxSpriteId;
        sheet->lastId = maxSpriteId;
    }
    tibia::assets::sprites().addSheet(sheet);
    m_lastSpriteId = std::max(m_lastSpriteId, lastSpriteId);
}

}
