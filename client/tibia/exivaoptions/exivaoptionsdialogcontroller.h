#pragma once

#include "input/gameactiondispatch.h"
#include "gamewindow/dialogmanager.h"
#include "gamewindow/messagedialogcontroller.h"
#include "input/gameaction.h"

#include <cip_gen.h>

#include <QPointer>

#include <functional>
#include <memory>
#include <vector>

namespace tibia {
namespace game {
class IGameSession;
}
namespace exivaoptions {

class TExivaOptionsStorage;

/// The exiva options dialog, ExivaOptionsDialog.qml, opened by
/// OPEN_EXIVA_OPTIONS_DIALOG. It shows the TExivaOptionsStorage's flags and whitelists
/// (one name per line); Apply publishes the edited options as EXIVA_UPADATE_OPTIONS.
class TExivaOptionsDialogController final : public gamewindow::TDialogController<cip::tibia::gamewindow::TExivaOptionsDialogController>
{
    Q_OBJECT
public:
    /// CIP: made in the engine under the game window's dialog parent item
    /// (without them the dialog uses the dialog stack's).
    explicit TExivaOptionsDialogController(QQmlEngine* Engine = nullptr, QQuickItem* DialogParentItem = nullptr,
                                           QObject* parent = nullptr);
    ~TExivaOptionsDialogController() override;

    /// The session's world type (the dialog needs an Optional PvP world).
    void setWorldType(std::function<int()> worldType) { m_worldType = std::move(worldType); }
    /// CIP connector: the dialog closes and edits `GameSession`'s exiva options.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

    QString characterWhitelist() const override;
    QString guildWhitelist() const override;

public Q_SLOTS:
    /// Outside an Optional PvP world a message box instead.
    void showDialog() override;
    void closeDialog() override;
    void closeButtonClicked() override;
    void applyButtonClicked(const QString& CharacterWhitelistString, const QString& GuildWhitelistString) override;
    void onAllowEveryoneChanged() override;
    void onAllowedGroupsChanged() override;

private:
    bool handleAction(const input::IGameAction& action);

    /// CIP +0xc8: the current session; +0xd8: its exiva options.
    std::shared_ptr<game::IGameSession> m_gameSession;
    QPointer<TExivaOptionsStorage> m_storage;
    /// CIP +0x40.
    std::vector<QMetaObject::Connection> m_gameSessionConnections;
    std::function<int()> m_worldType;
    gamewindow::TMessageDialogController m_onlyOptionalPvp;

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
