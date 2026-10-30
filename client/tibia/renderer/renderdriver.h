#pragma once

#include <cip_gen.h>

namespace tibia::qmlcomponents {

/// Invisible item that keeps the scene graph rendering every frame (QML
/// RenderDriver): each swapped frame polishes it, and polishing requests the next.
class TRenderDriver : public cip::tibia::qmlcomponents::TRenderDriver
{
    Q_OBJECT
public:
    explicit TRenderDriver(QQuickItem* parent = nullptr);

protected:
    void updatePolish() override;
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) override;
    void itemChange(ItemChange change, const ItemChangeData& value) override;

private:
    QMetaObject::Connection m_frameSwapped;
};

}
