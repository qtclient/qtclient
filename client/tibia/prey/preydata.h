#pragma once

#include <cip_gen.h>

#include <QList>
#include <QObject>
#include <QString>

#include <cstdint>

namespace tibia {
namespace prey {

/// A creature of a prey slot (CIP: name and outfit of TPreyData's monster).
struct TPreyMonster
{
    QString name;
    uint outfitId = 0;
    int head = 0;
    int torso = 0;
    int legs = 0;
    int detail = 0;
    bool firstAddOn = false;
    bool secondAddOn = false;

    bool operator==(const TPreyMonster&) const = default;
};

/// The protocol's prey bonus (type 4: none).
struct TPreyBonus
{
    int type = 4;
    int value = 0;
    int grade = 0;
};

/// One prey slot. States:
/// 0 locked, 1 inactive, 2 active, 3 selection, 4 selection with a bonus,
/// 5 bestiary selection, 6 bestiary selection with a bonus (wildcard).
class TPreyData final : public cip::tibia::prey::TPreyData
{
    Q_OBJECT
public:
    explicit TPreyData(uint8_t preyId, QObject* parent = nullptr);

    uint8_t preyId() const { return m_preyId; }
    int state() const { return m_state; }
    int unlockOption() const { return m_unlockOption; }
    int timeLeft() const { return m_timeLeft; }
    /// The free list reroll, in seconds since the epoch (0: now).
    qint64 nextFreeListReroll() const { return m_nextFreeListReroll; }
    int automaticExtendType() const { return m_automaticExtendType; }
    const TPreyBonus& bonus() const { return m_bonus; }
    const TPreyMonster& monster() const { return m_monster; }
    const QList<TPreyMonster>& monsterList() const { return m_monsterList; }
    const QList<uint16_t>& raceList() const { return m_raceList; }

    /// The state setters. Each clears the slot first (clearStateData) and
    /// falls back to inactive when the server sent no bonus or no creatures.
    void setLocked(int unlockOption);
    void setInactive();
    void setActive(const TPreyBonus& bonus, const TPreyMonster& monster, int timeLeft);
    void setSelection(QList<TPreyMonster> list);
    void setSelectionChangeMonster(const TPreyBonus& bonus, QList<TPreyMonster> list);
    void setListSelection(QList<uint16_t> races);
    void setWildcardSelection(const TPreyBonus& bonus, QList<uint16_t> races);
    void setTimeLeft(int seconds);
    /// `seconds` from now, as the server sends it (0: free now).
    void setNextFreeListReroll(qint64 seconds);
    void setAutomaticExtendType(int type);

private:
    void clearStateData();
    void setState(int state);
    void setUnlockOption(int unlockOption);
    void setBonus(const TPreyBonus& bonus);
    void setMonster(const TPreyMonster& monster);
    void setMonsterList(QList<TPreyMonster> list);
    void setRaceList(QList<uint16_t> races);

    uint8_t m_preyId;
    /// A new slot is inactive and unlocked until the server says otherwise.
    int m_state = 1;
    int m_unlockOption = 2;
    int m_timeLeft = 0;
    qint64 m_nextFreeListReroll = 0;
    int m_automaticExtendType = 0;
    TPreyBonus m_bonus;
    TPreyMonster m_monster;
    QList<TPreyMonster> m_monsterList;
    QList<uint16_t> m_raceList;
};

}
}
