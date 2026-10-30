#include "renderer/scenegraphnodeprovider.h"

namespace tibia::renderer {

QColor TScenegraphNodeProvider::highlightColor(const qmlcomponents::SpriteRecord::Highlight highlight)
{
    switch (highlight) {
    case qmlcomponents::SpriteRecord::AttackTarget:
        return QColor(0xff, 0x00, 0x00);
    case qmlcomponents::SpriteRecord::FollowTarget:
        return QColor(0x00, 0xff, 0x00);
    default:
        return QColor(0xff, 0xe1, 0x37);
    }
}

}
