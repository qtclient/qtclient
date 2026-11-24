#pragma once

#include "input/gameactiondispatch.h"
#include "gamewindow/gamewindowciptypes.h"
#include "input/gameaction.h"

#include <QObject>
#include <QPointer>

class QQmlContext;
class QQuickItem;

namespace tibia {
namespace gamewindow {

/// CIP's TTargetSelectionController (slots start / 
/// finish / dialog) owning TTargetSelectionEventFilter .
class TTargetSelectionController final : public cip::tibia::gamewindow::TTargetSelectionController
{
    Q_OBJECT
public:
    explicit TTargetSelectionController(QQuickItem* root, QObject* parent = nullptr);
    ~TTargetSelectionController() override;

    TTargetSelectionEventFilter* eventFilterObject() const { return m_filter; }
    /// Publishes this controller in the root context as
    /// tibiaTargetSelectionController.
    void registerInQmlContext(QQmlContext* RootContext);
    void setSelecting(bool selecting);

public Q_SLOTS:
    /// CIP moc — START_TARGET_SELECTION (0x50).
    void handleGameAction(const input::IGameAction& action);
    /// CIP moc — cancel selection when a modal dialog opens.
    void onDialogVisibleChanged(bool Visible) override;
    /// CIP moc — TARGET_SELECTION_FINISHED fan-out.
    void onTargetSelectionFinished();

Q_SIGNALS:
    /// CIP moc — emits TARGET_SELECTION_FINISHED after selection ends.
    void publishGameAction(const input::IGameActionPtr& action);

private:
    void bindDialogStack();

    TTargetSelectionEventFilter* m_filter = nullptr;
    bool m_selecting = false;
    bool m_dialogBound = false;
    QMetaObject::Connection m_dialogConn;
    QMetaObject::Connection m_filterFinishedConn;

public Q_SLOTS:
    /// The router's game actions (CIP: handleGameAction; connected to
    /// publishGameActionInternal).
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction)
    {
        m_gameActions.handle(pGameAction);
    }

private:
    ::tibia::input::TGameActionDispatch m_gameActions;
};

}
}
