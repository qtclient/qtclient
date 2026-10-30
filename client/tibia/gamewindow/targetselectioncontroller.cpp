#include "targetselectioncontroller.h"

#include "input/gameactionhandlers.h"

namespace tibia::gamewindow {

TTargetSelectionController::TTargetSelectionController(QQuickItem* root, QObject* parent)
    : QObject(parent)
    , m_filter(new TTargetSelectionEventFilter(root, this))
{
    input::setTargetSelectionListener([this](const bool selecting) {
        if (m_filter)
            m_filter->setSelecting(selecting);
    });
}

TTargetSelectionController::~TTargetSelectionController()
{
    input::setTargetSelectionListener(nullptr);
}

}
