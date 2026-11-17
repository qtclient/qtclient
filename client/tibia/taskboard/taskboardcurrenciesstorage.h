#pragma once

#include <cip_gen.h>

#include <QObject>

namespace tibia {
namespace taskboard {

/// The task board currencies, fed from the resource balance messages.
class TTaskboardCurrenciesStorage final : public cip::tibia::taskboard::TTaskboardCurrenciesStorage
{
    Q_OBJECT
public:
    explicit TTaskboardCurrenciesStorage(QObject* parent = nullptr);

    static TTaskboardCurrenciesStorage* instance();

    void setBountyPoints(int value);
    void setPreyHuntingTaskTokensAmount(int value);
    void setSoulSealsAmount(int value);
    void setBountyTaskRerollTokens(int value);

    int bountyPoints() const { return m_bountyPoints; }
    int preyHuntingTaskTokensAmount() const { return m_preyHuntingTaskTokensAmount; }
    int soulSealsAmount() const { return m_soulSealsAmount; }
    int bountyTaskRerollTokens() const { return m_bountyTaskRerollTokens; }

private:
    int m_bountyPoints = 0;
    int m_preyHuntingTaskTokensAmount = 0;
    int m_soulSealsAmount = 0;
    int m_bountyTaskRerollTokens = 0;
};

}
}
