#pragma once

#include "gamewindow/dialogmanager.h"
#include "game/blessingdialogdata.h"
#include "qtconnectionmanager.h"

#include <cip_gen.h>

#include <QPointer>

#include <memory>
#include <vector>

namespace tibia {
namespace game {
class IGameSession;
class TPremiumTimeStorage;
}
namespace gamewindow {

/// BlessingsDialog.qml (CIP: gamewindow::TBlessingsDialogController, ctor
/// ). Opens when the server sends the blessing dialog (answer to
/// the inventory's blessings button).
class TBlessingsDialogController final : public TDialogController<cip::tibia::gamewindow::TBlessingsDialogController>
{
    Q_OBJECT
public:
    TBlessingsDialogController(QQmlEngine* engine, QQuickItem* dialogParentItem, QObject* parent = nullptr);

    /// Closes the dialog; the session's blessings dialog message opens it.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);
    void connectToPremiumTimeStorage(game::TPremiumTimeStorage* PremiumTimeStorage);

public Q_SLOTS:
    void closeDialog() override;
    /// CIP .
    void onPremiumStatusChanged() override;
    /// CIP : store request type 1 (premium category).
    void requestOpenStoreWithPremiumCategory() override;
    /// CIP : store request type 3 with the blessing's offer.
    void requestOpenStoreWithBlessingOffer(int BlessingId) override;
    void requestClose() override;

private:
    /// Replaces the data and shows the dialog. CipSoft only
    /// creates the item when there is none; the QML reads blessingsData once,
    /// so an open dialog is rebuilt instead.
    void showDialog(const game::TBlessingDialogData& BlessingDialogData);

    shared::TQtConnectionManager m_gameSessionConnections;
    std::shared_ptr<game::IGameSession> m_gameSession;
    QPointer<game::TPremiumTimeStorage> m_premiumTimeStorage;
};

}
}
