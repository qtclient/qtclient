#pragma once

#include <QMetaType>
#include <QString>

#include <cstdint>
#include <utility>
#include <vector>

namespace tibia {
namespace game {

/// What the blessings dialog shows, as the blessings handler reads it from the
/// server's blessings dialog message.
class TBlessingDialogData
{
public:
    /// One blessing: its bit (1, 2, 4, … 256), the charges the player has and
    /// how many of them came from the store.
    struct TBlessing
    {
        TBlessing(int Id, uint8_t Charges, uint8_t ChargesFromStore);

        int Id = 0;
        uint8_t Charges = 0;
        uint8_t ChargesFromStore = 0;
    };

    /// One line of the blessing history: when, its colour (0 and 5 mark a lost
    /// blessing) and the server's text.
    struct THistoryEntry
    {
        THistoryEntry(uint32_t Timestamp, int Color, const QString& Text);

        uint32_t Timestamp = 0;
        int Color = 0;
        QString Text;
    };

    /// The death penalty in percent: the promotion's loss reduction, the
    /// experience lost on a PvP death (from / to) and on a PvE death, then the
    /// backpack and item loss of a PvP and of a PvE death.
    struct TDeathPenalty
    {
        TDeathPenalty() = default;
        TDeathPenalty(uint8_t PromotionLossReduction, uint8_t PvpMinimumExperienceLoss, uint8_t PvpMaximumExperienceLoss,
                      uint8_t PveExperienceLoss, uint8_t PvpBackpackLoss, uint8_t PvpItemsLoss, uint8_t PveBackpackLoss,
                      uint8_t PveItemsLoss);

        uint8_t PromotionLossReduction = 0;
        uint8_t PvpMinimumExperienceLoss = 0;
        uint8_t PvpMaximumExperienceLoss = 0;
        uint8_t PveExperienceLoss = 0;
        uint8_t PvpBackpackLoss = 0;
        uint8_t PvpItemsLoss = 0;
        uint8_t PveBackpackLoss = 0;
        uint8_t PveItemsLoss = 0;
    };

    const std::vector<TBlessing>& blessings() const { return m_blessings; }
    void setBlessings(std::vector<TBlessing>&& Blessings) { m_blessings = std::move(Blessings); }

    const std::vector<THistoryEntry>& history() const { return m_history; }
    void setHistory(std::vector<THistoryEntry>&& History);

    const TDeathPenalty& deathPenalty() const { return m_deathPenalty; }
    void setDeathPenalty(const TDeathPenalty& DeathPenalty);

    bool isPromoted() const { return m_promoted; }
    void setPromoted(bool Promoted);

    bool hasSkull() const { return m_hasSkull; }
    void setHasSkull(bool HasSkull);

    bool hasAmuletOfLoss() const { return m_hasAmuletOfLoss; }
    void setHasAmuletOfLoss(bool HasAmuletOfLoss);

private:
    std::vector<TBlessing> m_blessings;
    std::vector<THistoryEntry> m_history;
    TDeathPenalty m_deathPenalty;
    bool m_promoted = false;
    bool m_hasSkull = false;
    bool m_hasAmuletOfLoss = false;
};

}
}

Q_DECLARE_METATYPE(tibia::game::TBlessingDialogData)
