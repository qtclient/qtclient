#pragma once

#include "prey/preydata.h"

#include <cip_gen.h>

#include <QList>

#include <array>
#include <cstdint>
#include <set>
#include <unordered_set>

namespace tibia {
namespace prey {

/// The three prey slots and the prices.
/// The dialog opens once the server sent every slot.
class TPreyStorage final : public cip::tibia::prey::TPreyStorage
{
    Q_OBJECT
public:
    static constexpr int SlotCount = 3;

    explicit TPreyStorage(QObject* parent = nullptr);

    TPreyData* slot(int preyId) const;
    const std::array<TPreyData*, SlotCount>& slots() const { return m_slots; }
    /// Every slot arrived (the pending set is empty).
    bool allDataReceived() const { return m_pendingSlots.empty(); }
    /// A slot arrived.
    void markReceived(int preyId);
    /// One slot's data; out-of-range slots are ignored. The state setters
    /// count the slot as received.
    void setSlotNextFreeListReroll(int preyId, qint64 seconds);
    void setSlotTimeLeft(int preyId, int seconds);
    void setSlotAutomaticExtendType(int preyId, int type);
    void setSlotLocked(int preyId, int unlockOption);
    void setSlotInactive(int preyId);
    void setSlotActive(int preyId, const TPreyBonus& bonus, const TPreyMonster& monster, int timeLeft);
    void setSlotSelection(int preyId, QList<TPreyMonster> list);
    void setSlotSelectionChangeMonster(int preyId, const TPreyBonus& bonus, QList<TPreyMonster> list);
    void setSlotListSelection(int preyId, QList<uint16_t> races);
    void setSlotWildcardSelection(int preyId, const TPreyBonus& bonus, QList<uint16_t> races);

    int listRerollPrice() const { return m_listRerollPrice; }
    int bonusRerollPrice() const { return m_bonusRerollPrice; }
    int choosePreyPrice() const { return m_choosePreyPrice; }
    int preyWildcards() const { return m_preyWildcards; }
    bool bonusRerollPossible() const { return static_cast<uint>(m_bonusRerollPrice) <= static_cast<uint>(m_preyWildcards); }
    bool choosePreyPossible() const { return static_cast<uint>(m_choosePreyPrice) <= static_cast<uint>(m_preyWildcards); }
    /// The PreyPrices message: list reroll, bonus reroll and choose prey prices.
    void setPrices(int listReroll, int bonusReroll, int choosePrey);
    void setListRerollPrice(int price);
    void setBonusRerollPrice(int price);
    void setChoosePreyPrice(int price);
    /// The prey wildcard resource balance.
    void setPreyWildcards(int amount);
    /// The races of the slots with an active prey (looked up by the monster's name).
    std::unordered_set<uint16_t> activePreyRaceIds() const;

private:
    std::array<TPreyData*, SlotCount> m_slots{};
    std::set<int> m_pendingSlots;
    int m_listRerollPrice = 0;
    int m_bonusRerollPrice = 0;
    int m_choosePreyPrice = 0;
    int m_preyWildcards = 0;
};

}
}
