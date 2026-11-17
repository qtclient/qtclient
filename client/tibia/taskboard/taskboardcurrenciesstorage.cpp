#include "taskboard/taskboardcurrenciesstorage.h"

#include <QCoreApplication>

namespace tibia::taskboard {

TTaskboardCurrenciesStorage::TTaskboardCurrenciesStorage(QObject* parent)
    : cip::tibia::taskboard::TTaskboardCurrenciesStorage(parent)
{
}

TTaskboardCurrenciesStorage* TTaskboardCurrenciesStorage::instance()
{
    static TTaskboardCurrenciesStorage* s_instance = new TTaskboardCurrenciesStorage(QCoreApplication::instance());
    return s_instance;
}

/// Bounty points report every balance message, also an unchanged one.
void TTaskboardCurrenciesStorage::setBountyPoints(const int value)
{
    m_bountyPoints = value;
    Q_EMIT bountyPointsChanged();
}

void TTaskboardCurrenciesStorage::setPreyHuntingTaskTokensAmount(const int value)
{
    if (m_preyHuntingTaskTokensAmount == value)
        return;
    m_preyHuntingTaskTokensAmount = value;
    Q_EMIT preyHuntingTaskTokensAmountChanged();
}

void TTaskboardCurrenciesStorage::setSoulSealsAmount(const int value)
{
    if (m_soulSealsAmount == value)
        return;
    m_soulSealsAmount = value;
    Q_EMIT soulSealsAmountChanged();
}

void TTaskboardCurrenciesStorage::setBountyTaskRerollTokens(const int value)
{
    if (m_bountyTaskRerollTokens == value)
        return;
    m_bountyTaskRerollTokens = value;
    Q_EMIT bountyTaskRerollTokensChanged();
}

}
