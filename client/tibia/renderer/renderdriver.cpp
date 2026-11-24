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
    // The game client polishes the driver every frame (TGameWindowController::requestRenderDriverUpdate);
    // host: the first polish when the driver enters a window.
    if (change == ItemSceneChange && value.window)
        polish();
    cip::tibia::qmlcomponents::TRenderDriver::itemChange(change, value);
}

}
