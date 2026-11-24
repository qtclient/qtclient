#include "prey/preystorage.h"

#include <tibia/game/thingsession_forward.h>

#include <utility>

namespace tibia::prey {

namespace {

/// The race of the monster named `name` in the static race table (names
/// compare case-insensitively); 0 when there is none.
uint16_t raceIdOfMonsterName(const QString& name)
{
    for (const auto& race : tibia::game::getAllRaces()) {
        if (QString::compare(QString::fromStdString(race.name), name, Qt::CaseInsensitive) == 0)
            return static_cast<uint16_t>(race.raceId);
    }
    return 0;
}

}

TPreyStorage::TPreyStorage(QObject* parent) : cip::tibia::prey::TPreyStorage(parent)
{
    for (int i = 0; i < SlotCount; ++i)
        m_slots[i] = new TPreyData(static_cast<uint8_t>(i), this);
    // Every slot waits for the server's first state.
    for (int i = 0; i < SlotCount; ++i)
        m_pendingSlots.insert(i);
}

TPreyData* TPreyStorage::slot(const int preyId) const
{
    return preyId >= 0 && preyId < SlotCount ? m_slots[preyId] : nullptr;
}

void TPreyStorage::markReceived(const int preyId) { m_pendingSlots.erase(preyId); }

void TPreyStorage::setSlotNextFreeListReroll(const int preyId, const qint64 seconds)
{
    if (TPreyData* data = slot(preyId))
        data->setNextFreeListReroll(seconds);
}

void TPreyStorage::setSlotTimeLeft(const int preyId, const int seconds)
{
    if (TPreyData* data = slot(preyId))
        data->setTimeLeft(seconds);
}

void TPreyStorage::setSlotAutomaticExtendType(const int preyId, const int type)
{
    if (TPreyData* data = slot(preyId))
        data->setAutomaticExtendType(type);
}

void TPreyStorage::setSlotLocked(const int preyId, const int unlockOption)
{
    if (TPreyData* data = slot(preyId)) {
        data->setLocked(unlockOption);
        markReceived(preyId);
    }
}

void TPreyStorage::setSlotInactive(const int preyId)
{
    if (TPreyData* data = slot(preyId)) {
        data->setInactive();
        markReceived(preyId);
    }
}

void TPreyStorage::setSlotActive(const int preyId, const TPreyBonus& bonus, const TPreyMonster& monster)
{
    if (TPreyData* data = slot(preyId)) {
        data->setActive(bonus, monster);
        markReceived(preyId);
    }
}

void TPreyStorage::setSlotSelection(const int preyId, QList<TPreyMonster> list)
{
    if (TPreyData* data = slot(preyId)) {
        data->setSelection(std::move(list));
        markReceived(preyId);
    }
}

void TPreyStorage::setSlotSelectionChangeMonster(const int preyId, const TPreyBonus& bonus, QList<TPreyMonster> list)
{
    if (TPreyData* data = slot(preyId)) {
        data->setSelectionChangeMonster(bonus, std::move(list));
        markReceived(preyId);
    }
}

void TPreyStorage::setSlotListSelection(const int preyId, QList<uint16_t> races)
{
    if (TPreyData* data = slot(preyId)) {
        data->setListSelection(std::move(races));
        markReceived(preyId);
    }
}

void TPreyStorage::setSlotWildcardSelection(const int preyId, const TPreyBonus& bonus, QList<uint16_t> races)
{
    if (TPreyData* data = slot(preyId)) {
        data->setWildcardSelection(bonus, std::move(races));
        markReceived(preyId);
    }
}

void TPreyStorage::setPrices(const int listReroll, const int bonusReroll, const int choosePrey)
{
    setListRerollPrice(listReroll);
    setBonusRerollPrice(bonusReroll);
    setChoosePreyPrice(choosePrey);
}

void TPreyStorage::setListRerollPrice(const int price)
{
    if (m_listRerollPrice == price)
        return;
    m_listRerollPrice = price;
    Q_EMIT listRerollPriceChanged();
}

void TPreyStorage::setBonusRerollPrice(const int price)
{
    if (m_bonusRerollPrice == price)
        return;
    m_bonusRerollPrice = price;
    Q_EMIT bonusRerollPriceChanged();
    Q_EMIT bonusRerollPossibleChanged();
}

void TPreyStorage::setChoosePreyPrice(const int price)
{
    if (m_choosePreyPrice == price)
        return;
    m_choosePreyPrice = price;
    Q_EMIT choosePreyPriceChanged();
    Q_EMIT choosePreyPossibleChanged();
}

void TPreyStorage::setPreyWildcards(const int amount)
{
    if (amount == m_preyWildcards)
        return;
    m_preyWildcards = amount;
    Q_EMIT preyWildcardAmountChanged();
    Q_EMIT bonusRerollPossibleChanged();
    Q_EMIT choosePreyPossibleChanged();
}

std::unordered_set<uint16_t> TPreyStorage::activePreyRaceIds() const
{
    std::unordered_set<uint16_t> races;
    for (const TPreyData* data : m_slots) {
        if (data->state() == 2)
            races.insert(raceIdOfMonsterName(data->monster().name));
    }
    return races;
}

}
