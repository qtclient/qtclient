#pragma once

#include "game/gameciptypes.h"
#include "gamewindow/dialogmanager.h"

#include <cip_gen.h>

#include <memory>
#include <vector>

namespace tibia::game {
class IGameSession;
}

namespace tibia {
namespace gamewindow {

/// A dialog the server opens (ServerModalDialog.qml; CIP:
/// TServerModalDialogController).
class TServerModalDialogController final : public TDialogController<cip::tibia::gamewindow::TServerModalDialogController>
{
    Q_OBJECT
public:
    TServerModalDialogController(QQmlEngine* Engine, QQuickItem* DialogParentItem, QObject* parent = nullptr);

    /// CIP: closes the dialog (a priority dialog stays when the session goes),
    /// keeps the session and shows the dialogs its server modal dialog
    /// protocol handler asks for.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

    void answerButtonClicked(int ButtonIndex, int SelectionIndex) override;

public Q_SLOTS:
    /// CIP: replaces the shown dialog with `RequestedDialog`.
    void showDialog(const ::tibia::game::TServerModalDialogData& RequestedDialog);

private:
    std::shared_ptr<game::IGameSession> m_gameSession;
    std::vector<QMetaObject::Connection> m_gameSessionConnections;
    game::TServerModalDialogData m_dialogData;
};

}
}
