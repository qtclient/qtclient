#pragma once

#include "gamewindow/dialogmanager.h"
#include "taskboard/taskboardshopstorage.h"

#include <cip_gen.h>

#include <QColor>
#include <QString>
#include <QVariant>

class QQmlEngine;
class QQuickItem;

namespace tibia {
namespace gamewindow {

/// HuntingTaskRewardPurchaseConfirmationDialog.qml (CIP:
/// TTaskboardHuntingTaskRewardPurchaseConfirmationDialogController): asks
/// before a hunting task shop offer is bought. The task board shop holds it
/// and sends the purchase on confirmPurchase.
class TTaskboardHuntingTaskRewardPurchaseConfirmationDialogController final
    : public TDialogController<cip::tibia::gamewindow::TTaskboardHuntingTaskRewardPurchaseConfirmationDialogController>
{
    Q_OBJECT
public:
    TTaskboardHuntingTaskRewardPurchaseConfirmationDialogController(QQmlEngine* engine, QQuickItem* dialogParentItem,
                                                                   QObject* parent = nullptr);

    int appearanceId() const override;
    int optionalId() const override;
    QString title() const override;
    /// The price in English number format.
    QString cost() const override;
    /// The reward's type when it is one of the four known ones, otherwise 0.
    QVariant rewardType() const override;
    bool addOn1() const override;
    bool addOn2() const override;
    QColor headColor() const override;
    QColor torsoColor() const override;
    QColor legsColor() const override;
    QColor detailColor() const override;

    /// Closes the dialog, then confirms the purchase.
    Q_INVOKABLE void purchaseRequested() override;
    Q_INVOKABLE void closeRequested() override;

public Q_SLOTS:
    /// Reopens the dialog on `RewardToBuy`.
    void showDialog(const taskboard::TShopReward& RewardToBuy);
    /// An outfit reward: reopens the dialog with the player's outfit colours.
    void showDialog(const taskboard::TShopReward& RewardToBuy, const QColor& headColor, const QColor& torsoColor,
                    const QColor& legsColor, const QColor& detailColor);

private:
    taskboard::TShopReward m_rewardToBuy;
    QColor m_outfitHeadColor = Qt::white;
    QColor m_outfitTorsoColor = Qt::white;
    QColor m_outfitLegsColor = Qt::white;
    QColor m_outfitDetailColor = Qt::white;
};

}
}
