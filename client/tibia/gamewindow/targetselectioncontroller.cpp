#include "targetselectioncontroller.h"

#include "gamewindow/dialogmanager.h"
#include "input/gameactionhandlers.h"

namespace tibia::gamewindow {

TTargetSelectionController::TTargetSelectionController(QQuickItem* root, QObject* parent)
    : cip::tibia::gamewindow::TTargetSelectionController(parent)
    , m_filter(new TTargetSelectionEventFilter(root, this))
{
    m_filterFinishedConn = connect(m_filter, &cip::tibia::gamewindow::TTargetSelectionEventFilter::targetSelectionFinished, this,
                                   &TTargetSelectionController::onTargetSelectionFinished);
    // CipSoft: START_TARGET_SELECTION / TARGET_SELECTION_FINISHED reach this
    // controller through the router; input::startTargetSelection tells it too.
    input::setTargetSelectionListener([this](const bool selecting) { setSelecting(selecting); });
    m_gameActions.on(input::EGameAction::START_TARGET_SELECTION, [this](const input::IGameAction&) { setSelecting(true); });
    m_gameActions.on(input::EGameAction::TARGET_SELECTION_FINISHED, [this](const input::IGameAction&) { setSelecting(false); });
    bindDialogStack();
}

TTargetSelectionController::~TTargetSelectionController()
{
    input::setTargetSelectionListener(nullptr);
    if (m_dialogConn)
        QObject::disconnect(m_dialogConn);
    if (m_filterFinishedConn)
        QObject::disconnect(m_filterFinishedConn);
}

void TTargetSelectionController::bindDialogStack()
{
    if (m_dialogBound)
        return;
    TDialogStackController* stack = TDialogStackController::instance();
    if (!stack)
        return;
    m_dialogConn = connect(stack, &cip::tibia::gamewindow::TDialogStackController::dialogVisible, this,
                           &TTargetSelectionController::onDialogVisibleChanged);
    m_dialogBound = true;
}

void TTargetSelectionController::setSelecting(const bool selecting)
{
    bindDialogStack();
    if (selecting) {
        // CIP : dialog already up → finish instead of arming.
        if (TDialogStackController* dialogs = TDialogStackController::instance(); dialogs && !dialogs->isEmpty()) {
            onTargetSelectionFinished();
            return;
        }
    }
    m_selecting = selecting;
    if (m_filter)
        m_filter->setSelecting(selecting);
}

void TTargetSelectionController::handleGameAction(const input::IGameAction& action)
{
    if (action.gameActionType() == input::EGameAction::START_TARGET_SELECTION)
        setSelecting(true);
}

void TTargetSelectionController::onDialogVisibleChanged(const bool Visible)
{
    // CIP : selecting + dialog visible → finish.
    if (m_selecting && Visible)
        onTargetSelectionFinished();
}

void TTargetSelectionController::onTargetSelectionFinished()
{
    // CIP : clear selecting, publish TARGET_SELECTION_FINISHED.
    m_selecting = false;
    if (m_filter)
        m_filter->setSelecting(false);
    Q_EMIT publishGameAction(input::makeGameAction(input::EGameAction::TARGET_SELECTION_FINISHED));
}

}
