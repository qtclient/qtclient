#include "game/blessingdialogdata.h"

#include <utility>

namespace tibia::game {

TBlessingDialogData::TBlessing::TBlessing(const int Id, const uint8_t Charges, const uint8_t ChargesFromStore)
    : Id(Id)
    , Charges(Charges)
    , ChargesFromStore(ChargesFromStore)
{
}

TBlessingDialogData::THistoryEntry::THistoryEntry(const uint32_t Timestamp, const int Color, const QString& Text)
    : Timestamp(Timestamp)
    , Color(Color)
    , Text(Text)
{
}

TBlessingDialogData::TDeathPenalty::TDeathPenalty(const uint8_t PromotionLossReduction, const uint8_t PvpMinimumExperienceLoss,
                                                  const uint8_t PvpMaximumExperienceLoss, const uint8_t PveExperienceLoss,
                                                  const uint8_t PvpBackpackLoss, const uint8_t PvpItemsLoss,
                                                  const uint8_t PveBackpackLoss, const uint8_t PveItemsLoss)
    : PromotionLossReduction(PromotionLossReduction)
    , PvpMinimumExperienceLoss(PvpMinimumExperienceLoss)
    , PvpMaximumExperienceLoss(PvpMaximumExperienceLoss)
    , PveExperienceLoss(PveExperienceLoss)
    , PvpBackpackLoss(PvpBackpackLoss)
    , PvpItemsLoss(PvpItemsLoss)
    , PveBackpackLoss(PveBackpackLoss)
    , PveItemsLoss(PveItemsLoss)
{
}

void TBlessingDialogData::setHistory(std::vector<THistoryEntry>&& History) { m_history = std::move(History); }

void TBlessingDialogData::setDeathPenalty(const TDeathPenalty& DeathPenalty) { m_deathPenalty = DeathPenalty; }

void TBlessingDialogData::setPromoted(const bool Promoted) { m_promoted = Promoted; }

void TBlessingDialogData::setHasSkull(const bool HasSkull) { m_hasSkull = HasSkull; }

void TBlessingDialogData::setHasAmuletOfLoss(const bool HasAmuletOfLoss) { m_hasAmuletOfLoss = HasAmuletOfLoss; }

}
