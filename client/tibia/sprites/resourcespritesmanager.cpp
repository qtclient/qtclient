#include "sprites/resourcespritesmanager.h"

namespace tibia::sprites {

TResourceSpritesManager::TResourceSpritesManager()
    : QObject(nullptr)
{
    addResourceSprite(QStringLiteral(":/images/minimap/markers.png"), 2);
    addResourceSprite(QStringLiteral(":/images/icon-map-npc.png"), 3);
    addResourceSprite(QStringLiteral(":/images/icon-map-passage-idle.png"), 4);
    addResourceSprite(QStringLiteral(":/images/icon-map-passage-selected.png"), 5);
    addResourceSprite(QStringLiteral(":/images/icon-map-pointofinterest.png"), 6);
    addResourceSprite(QStringLiteral(":/images/icon-raid.png"), 7);
    addResourceSprite(QStringLiteral(":/images/icon-map-player.png"), 8);
    // CipSoft's path lacks the slash after the colon, so this image does not load.
    addResourceSprite(QStringLiteral(":images/icon-map-house-status.png"), 9);
}

void TResourceSpritesManager::addResourceSprite(const QString& path, const int id)
{
    TResourceSprite sprite;
    sprite.image = QImage(path);
    const int height = sprite.image.size().height();
    sprite.size = QSize(height, height);
    sprite.path = path;
    m_resourceSprites[id] = sprite;
}

QRect TResourceSpritesManager::resourceRectangle(const int id, const QPointF& position, const bool centered) const
{
    const TResourceSprite& sprite = m_resourceSprites.at(id);
    const int width = sprite.size.width();
    const int height = sprite.size.height();
    double x = position.x();
    double y = position.y();
    if (centered) {
        x -= static_cast<double>(width / 2);
        y -= static_cast<double>(height / 2);
    }
    const int left = static_cast<int>(x);
    const int top = static_cast<int>(y);
    return QRect(QPoint(left, top), QPoint(left + width - 1, top + height - 1));
}

}
