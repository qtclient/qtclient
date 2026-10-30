#pragma once

#include <cip_gen.h>

#include <map>
#include <utility>

class QSGImageNode;

namespace tibia::minimap {
class TMinimapRenderProvider;
}

namespace tibia::qmlcomponents {

/// The minimap (QML Minimap; CIP: TMinimapQuickItem, 0x1010d63b0). Every
/// frame it rebuilds a transform node with the visible area's matrix and one
/// image node per visible tile, reusing nodes by the tile image's cache key;
/// nodes no longer shown are deleted. On OpenGL the tile is prescaled
/// (CIP 0x100f76680) and the matrix only translates.
class TMinimapQuickItem : public cip::tibia::qmlcomponents::TMinimapQuickItem
{
    Q_OBJECT
public:
    explicit TMinimapQuickItem(QQuickItem* parent = nullptr);
    ~TMinimapQuickItem() override;

    /// What the item draws (CIP 0x1010d6550); null draws nothing. Must outlive
    /// the item or be unset first.
    void setRenderProvider(const minimap::TMinimapRenderProvider* provider);

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) override;
    void hoverEnterEvent(QHoverEvent* event) override;
    void hoverLeaveEvent(QHoverEvent* event) override;
    void itemChange(ItemChange change, const ItemChangeData& value) override;

private:
    QSGImageNode* nodeFor(const QImage& colors, int x, int y, float pixelScale);
    void deleteDetachedNodes();

    const minimap::TMinimapRenderProvider* m_provider = nullptr;
    /// Tile nodes by image cache key and the OpenGL prescale (1000 = 1×). Owned here.
    std::map<std::pair<qint64, int>, QSGImageNode*> m_nodes;
    QMetaObject::Connection m_invalidatedConnection;
};

}
