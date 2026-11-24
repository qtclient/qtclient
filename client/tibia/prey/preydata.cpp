#include "prey/preydata.h"

#include <QDateTime>

#include <utility>

namespace tibia::prey {

namespace {

qint64 now() { return QDateTime::currentSecsSinceEpoch(); }

}

TPreyData::TPreyData(const uint8_t preyId, QObject* parent) : cip::tibia::prey::TPreyData(parent), m_preyId(preyId) {}

void TPreyData::setState(const int state)
{
    if (m_state == state)
        return;
    m_state = state;
    Q_EMIT stateChanged();
}

void TPreyData::setUnlockOption(const int unlockOption)
{
    if (m_unlockOption == unlockOption)
        return;
    m_unlockOption = unlockOption;
    Q_EMIT unlockOptionChanged();
}

void TPreyData::setBonus(const TPreyBonus& bonus)
{
    if (m_bonus.type == bonus.type && m_bonus.value == bonus.value && m_bonus.grade == bonus.grade)
        return;
    m_bonus = bonus;
    Q_EMIT bonusChanged();
}

void TPreyData::setMonster(const TPreyMonster& monster)
{
    if (m_monster == monster)
        return;
    m_monster = monster;
    Q_EMIT monsterInformationChanged();
}

void TPreyData::setMonsterList(QList<TPreyMonster> list)
{
    if (m_monsterList == list)
        return;
    m_monsterList = std::move(list);
    Q_EMIT monsterListChanged();
}

void TPreyData::setRaceList(QList<uint16_t> races)
{
    if (m_raceList == races)
        return;
    m_raceList = std::move(races);
    Q_EMIT monsterRaceListChanged();
}

/// Every state change starts from a cleared slot: unlock option 2, no bonus,
/// no creature, empty lists and no time left.
void TPreyData::clearStateData()
{
    setUnlockOption(2);
    setBonus(TPreyBonus{});
    setMonster(TPreyMonster{});
    setMonsterList({});
    setRaceList({});
    setTimeLeft(0);
}

/// Unlock option 2 means the slot is unlocked, so it shows as inactive.
void TPreyData::setLocked(const int unlockOption)
{
    clearStateData();
    if (unlockOption == 2) {
        setState(1);
        return;
    }
    setUnlockOption(unlockOption);
    setState(0);
}

void TPreyData::setInactive()
{
    clearStateData();
    setState(1);
}

void TPreyData::setActive(const TPreyBonus& bonus, const TPreyMonster& monster)
{
    clearStateData();
    if (bonus.type == 4) {
        setState(1);
    } else {
        setBonus(bonus);
        setMonster(monster);
        setState(2);
    }
}

void TPreyData::setSelection(QList<TPreyMonster> list)
{
    clearStateData();
    if (list.isEmpty()) {
        setState(1);
        return;
    }
    setMonsterList(std::move(list));
    setState(3);
}

void TPreyData::setSelectionChangeMonster(const TPreyBonus& bonus, QList<TPreyMonster> list)
{
    clearStateData();
    if (bonus.type == 4 || list.isEmpty()) {
        setState(1);
        return;
    }
    setBonus(bonus);
    setMonsterList(std::move(list));
    setState(4);
}

void TPreyData::setListSelection(QList<uint16_t> races)
{
    clearStateData();
    if (races.isEmpty()) {
        setState(1);
        return;
    }
    setRaceList(std::move(races));
    setState(5);
}

void TPreyData::setWildcardSelection(const TPreyBonus& bonus, QList<uint16_t> races)
{
    clearStateData();
    if (bonus.type == 4 || races.isEmpty()) {
        setState(1);
        return;
    }
    setBonus(bonus);
    setRaceList(std::move(races));
    setState(6);
}

void TPreyData::setTimeLeft(const int seconds)
{
    if (m_timeLeft == seconds)
        return;
    m_timeLeft = seconds;
    Q_EMIT timeLeftChanged();
}

void TPreyData::setNextFreeListReroll(const qint64 seconds)
{
    const qint64 at = seconds > 0 ? now() + seconds : 0;
    if (m_nextFreeListReroll == at)
        return;
    m_nextFreeListReroll = at;
    Q_EMIT timeUntilFreeListRerollChanged();
}

void TPreyData::setAutomaticExtendType(const int type)
{
    if (m_automaticExtendType == type)
        return;
    m_automaticExtendType = type;
    Q_EMIT automaticExtendTypeChanged();
}

}
