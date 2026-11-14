#include "renderdriver.h"

#include <QQuickWindow>

namespace tibia::qmlcomponents {

TRenderDriver::TRenderDriver(QQuickItem* parent)
    : cip::tibia::qmlcomponents::TRenderDriver(parent)
{
    setFlag(ItemHasContents);
}

void TRenderDriver::updatePolish()
{
    update();
}

QSGNode* TRenderDriver::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*)
{
    delete oldNode;
    return nullptr;
}

void TRenderDriver::itemChange(const ItemChange change, const ItemChangeData& value)
{
    if (change == ItemSceneChange) {
        disconnect(m_frameSwapped);
        if (QQuickWindow* window = value.window) {
            m_frameSwapped = connect(window, &QQuickWindow::frameSwapped, this, [this] {
                if (isVisible())
                    polish();
            }, Qt::QueuedConnection);
            polish();
        }
    }
    cip::tibia::qmlcomponents::TRenderDriver::itemChange(change, value);
}

}
