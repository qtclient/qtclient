#include "taskboardhuntingtaskrewardpurchaseconfirmationdialogcontroller.h"

#include <QLocale>

namespace tibia::gamewindow {

TTaskboardHuntingTaskRewardPurchaseConfirmationDialogController::TTaskboardHuntingTaskRewardPurchaseConfirmationDialogController(
    QQmlEngine* engine, QQuickItem* dialogParentItem, QObject* parent)
    : TDialogController(QStringLiteral("taskboard/HuntingTaskRewardPurchaseConfirmationDialog.qml"), engine, dialogParentItem,
                        parent)
{}

int TTaskboardHuntingTaskRewardPurchaseConfirmationDialogController::appearanceId() const
{
    return static_cast<int>(m_rewardToBuy.appearanceId);
}

int TTaskboardHuntingTaskRewardPurchaseConfirmationDialogController::optionalId() const
{
    return static_cast<int>(m_rewardToBuy.secondObjectTypeId);
}

QString TTaskboardHuntingTaskRewardPurchaseConfirmationDialogController::title() const { return m_rewardToBuy.name; }

QString TTaskboardHuntingTaskRewardPurchaseConfirmationDialogController::cost() const
{
    return QLocale(QLocale::English).toString(static_cast<qulonglong>(m_rewardToBuy.price));
}

QVariant TTaskboardHuntingTaskRewardPurchaseConfirmationDialogController::rewardType() const
{
    const int type = static_cast<int>(m_rewardToBuy.type);
    return QVariant(static_cast<unsigned>(type - 1) < 4u ? type : 0);
}

bool TTaskboardHuntingTaskRewardPurchaseConfirmationDialogController::addOn1() const { return m_rewardToBuy.firstAddon; }

bool TTaskboardHuntingTaskRewardPurchaseConfirmationDialogController::addOn2() const { return m_rewardToBuy.secondAddon; }

QColor TTaskboardHuntingTaskRewardPurchaseConfirmationDialogController::headColor() const { return m_outfitHeadColor; }

QColor TTaskboardHuntingTaskRewardPurchaseConfirmationDialogController::torsoColor() const { return m_outfitTorsoColor; }

QColor TTaskboardHuntingTaskRewardPurchaseConfirmationDialogController::legsColor() const { return m_outfitLegsColor; }

QColor TTaskboardHuntingTaskRewardPurchaseConfirmationDialogController::detailColor() const { return m_outfitDetailColor; }

void TTaskboardHuntingTaskRewardPurchaseConfirmationDialogController::purchaseRequested()
{
    dialog()->close();
    Q_EMIT confirmPurchase();
}

void TTaskboardHuntingTaskRewardPurchaseConfirmationDialogController::closeRequested() { dialog()->close(); }

void TTaskboardHuntingTaskRewardPurchaseConfirmationDialogController::showDialog(const taskboard::TShopReward& RewardToBuy)
{
    dialog()->close();
    m_rewardToBuy = RewardToBuy;
    dialog()->show();
}

void TTaskboardHuntingTaskRewardPurchaseConfirmationDialogController::showDialog(const taskboard::TShopReward& RewardToBuy,
                                                                                 const QColor& headColor,
                                                                                 const QColor& torsoColor,
                                                                                 const QColor& legsColor,
                                                                                 const QColor& detailColor)
{
    m_outfitHeadColor = headColor;
    m_outfitTorsoColor = torsoColor;
    m_outfitLegsColor = legsColor;
    m_outfitDetailColor = detailColor;
    showDialog(RewardToBuy);
}

}
