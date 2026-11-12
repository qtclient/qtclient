#pragma once

#include "gamewindow/gamewindowciptypes.h"
#include "input/gameaction.h"

#include <QObject>
#include <QPointer>

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

public Q_SLOTS:
    /// CIP moc — START_TARGET_SELECTION (0x50).
    void handleGameAction(const input::TGameAction& action);
    /// CIP moc — cancel selection when a modal dialog opens.
    void onDialogVisibleChanged(bool Visible) override;
    /// CIP moc — TARGET_SELECTION_FINISHED fan-out.
    void onTargetSelectionFinished();

Q_SIGNALS:
    /// CIP moc — emits TARGET_SELECTION_FINISHED after selection ends.
    void publishGameAction(const input::TGameActionPtr& action);

private:
    void setSelecting(bool selecting);
    void bindDialogStack();

    TTargetSelectionEventFilter* m_filter = nullptr;
    bool m_selecting = false;
    bool m_dialogBound = false;
    QMetaObject::Connection m_dialogConn;
    QMetaObject::Connection m_filterFinishedConn;
};

}
}
