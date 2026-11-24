#pragma once

#include "gamewindow/taskboardhuntingtaskrewardpurchaseconfirmationdialogcontroller.h"
#include "qtconnectionmanager.h"
#include "taskboard/taskboardrewardlistmodel.h"

#include <cip_gen.h>

#include <memory>

class QQmlEngine;
class QQuickItem;

namespace tibia {
namespace game {
class IGameSession;
}
namespace gamewindow {

/// The task board's shop tab (CIP: TTaskboardShopController). Lists the
/// session's hunting task shop offers; buying one asks for confirmation
/// first, then sends the offer's index.
class TTaskboardShopController final : public cip::tibia::gamewindow::TTaskboardShopController
{
    Q_OBJECT
public:
    TTaskboardShopController(QQmlEngine* engine, QQuickItem* dialogParentItem, QObject* parent = nullptr);

    /// Takes the session: its shop offers refill the list, its hunting task
    /// points notify huntingTaskPoints.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

    /// The session's hunting task points; 0 without a session.
    int huntingTaskPoints() const override;
    QObject* rewardList() const override;
    /// The player's outfit colours; white without a player outfit.
    QColor headColor() const override;
    QColor torsoColor() const override;
    QColor legsColor() const override;
    QColor detailColor() const override;

public Q_SLOTS:
    /// Asks the server for the shop offers.
    void showDialogTab() override;
    /// Closes the purchase confirmation.
    void hideDialogTab() override;
    void updateRewardList() override;
    /// Remembers the offer and asks to confirm it (with the player's colours
    /// for an outfit).
    void requestBuyOffer(int Index) override;

private:
    /// The player's outfit colour of `part` (0 head, 1 torso, 2 legs, 3 detail).
    QColor playerOutfitColor(int part) const;
    /// The purchase was confirmed: buys the remembered offer.
    void onPurchaseConfirmed();

    std::shared_ptr<game::IGameSession> m_gameSession;
    shared::TQtConnectionManager m_gameSessionConnections;
    taskboard::TTaskboardRewardListModel* m_rewardsModel = nullptr;
    TTaskboardHuntingTaskRewardPurchaseConfirmationDialogController m_purchaseConfirmationDialog;
    /// The offer waiting for the confirmation; -1 none.
    int m_offerToBuyIndex = -1;
};

}
}
