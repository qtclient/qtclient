#pragma once

#include "game/blessingdialogdata.h"

#include <cip_gen.h>

namespace tibia {
namespace game {

/// The blessings dialog's QML data: one row per blessing but the
/// adventurer's blessing, the history, and the texts of the promotion's loss
/// reduction and of the death penalty.
class TQmlBlessingDialogData final : public cip::tibia::game::TQmlBlessingDialogData
{
    Q_OBJECT
public:
    TQmlBlessingDialogData(TBlessingDialogData&& BlessingDialogData, bool IsPremium);
    ~TQmlBlessingDialogData() override;

    QList<QObject*> blessingsWithCharges() const override;
    QList<QObject*> blessingHistory() const override;
    bool isPremium() const override;
    /// The loss reduction of the promotion, worded for premium / free account
    /// and promoted / not promoted.
    QString promotionLossReductionText() const override;
    /// One line each: PvP and PvE experience loss, backpack loss, items loss,
    /// then the adventurer's blessing, the amulet of loss and the skull.
    QStringList deathPenaltyText() const override;

    /// Stores the premium status; a change notifies premiumStatusChanged.
    void setPremium(bool IsPremium);

private:
    TBlessingDialogData m_data;
    QList<QObject*> m_blessingObjects;
    QList<QObject*> m_historyObjects;
    bool m_premium = false;
};

}
}
