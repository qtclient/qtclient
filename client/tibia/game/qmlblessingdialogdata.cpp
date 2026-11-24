#include "game/qmlblessingdialogdata.h"

#include "game/qmlblessinghistoryentry.h"
#include "game/qmlblessingwithcharges.h"

#include <algorithm>
#include <utility>

namespace tibia::game {

TQmlBlessingDialogData::TQmlBlessingDialogData(TBlessingDialogData&& BlessingDialogData, const bool IsPremium)
    : cip::tibia::game::TQmlBlessingDialogData(nullptr)
    , m_data(std::move(BlessingDialogData))
    , m_premium(IsPremium)
{
    // The adventurer's blessing has no row of its own: the death penalty text
    // mentions it.
    for (const TBlessingDialogData::TBlessing& blessing : m_data.blessings()) {
        if (blessing.Id != 1)
            m_blessingObjects.append(new TQmlBlessingWithCharges(blessing));
    }
    for (const TBlessingDialogData::THistoryEntry& entry : m_data.history())
        m_historyObjects.append(new TQmlBlessingHistoryEntry(TBlessingDialogData::THistoryEntry(entry)));
}

TQmlBlessingDialogData::~TQmlBlessingDialogData()
{
    qDeleteAll(m_blessingObjects);
    qDeleteAll(m_historyObjects);
}

QList<QObject*> TQmlBlessingDialogData::blessingsWithCharges() const { return m_blessingObjects; }

QList<QObject*> TQmlBlessingDialogData::blessingHistory() const { return m_historyObjects; }

bool TQmlBlessingDialogData::isPremium() const { return m_premium; }

QString TQmlBlessingDialogData::promotionLossReductionText() const
{
    const char* id = m_premium ? (m_data.isPromoted() ? "blessings_dialog_premium_promoted" : "blessings_dialog_premium_unpromoted")
                               : (m_data.isPromoted() ? "blessings_dialog_nopremium_promoted" : "blessings_dialog_nopremium_unpromoted");
    return qtTrId(id).arg(int(m_data.deathPenalty().PromotionLossReduction));
}

QStringList TQmlBlessingDialogData::deathPenaltyText() const
{
    const TBlessingDialogData::TDeathPenalty& penalty = m_data.deathPenalty();
    QStringList text;
    if (penalty.PvpMinimumExperienceLoss == penalty.PvpMaximumExperienceLoss)
        text.append(qtTrId("blessings_dialog_pvp_death_single_chance").arg(int(penalty.PvpMinimumExperienceLoss)));
    else
        text.append(qtTrId("blessings_dialog_pvp_death_range_chance").arg(int(penalty.PvpMinimumExperienceLoss)).arg(int(penalty.PvpMaximumExperienceLoss)));
    text.append(qtTrId("blessings_dialog_pve_death").arg(int(penalty.PveExperienceLoss)));
    if (penalty.PvpBackpackLoss == penalty.PveBackpackLoss)
        text.append(qtTrId("blessings_dialog_backpack_loss").arg(int(penalty.PvpBackpackLoss)));
    else
        text.append(qtTrId("blessings_dialog_backpack_loss_pvp_pve").arg(int(penalty.PvpBackpackLoss)).arg(int(penalty.PveBackpackLoss)));
    if (penalty.PvpItemsLoss == penalty.PveItemsLoss)
        text.append(qtTrId("blessings_dialog_items_loss").arg(int(penalty.PvpItemsLoss)));
    else
        text.append(qtTrId("blessings_dialog_items_loss_pvp_pve").arg(int(penalty.PvpItemsLoss)).arg(int(penalty.PveItemsLoss)));
    const auto& blessings = m_data.blessings();
    if (std::any_of(blessings.begin(), blessings.end(),
                    [](const TBlessingDialogData::TBlessing& blessing) { return blessing.Id == 1 && blessing.Charges != 0; }))
        text.append(qtTrId("blessings_dialog_adventurers_blessing"));
    if (m_data.hasAmuletOfLoss())
        text.append(qtTrId(m_data.hasSkull() ? "blessings_dialog_amulet_of_loss_disabled_by_skull" : "blessings_dialog_amulet_of_loss"));
    if (m_data.hasSkull())
        text.append(qtTrId("blessings_dialog_has_skull"));
    return text;
}

void TQmlBlessingDialogData::setPremium(const bool IsPremium)
{
    if (m_premium == IsPremium)
        return;
    m_premium = IsPremium;
    Q_EMIT premiumStatusChanged();
}

}
