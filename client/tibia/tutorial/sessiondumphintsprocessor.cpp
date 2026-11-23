#include "tutorial/sessiondumphintsprocessor.h"

#include "tutorial/sessiondumphintcondition.h"
#include "tutorial/sessiondumphints.h"

#include <QFileInfo>

#include <limits>

namespace tibia::tutorial {

QString hintsPathForDump(const QString& dumpPath)
{
    QFileInfo info(dumpPath);
    if (!info.exists())
        return {};
    const QString sibling = info.path() + QLatin1Char('/') + info.completeBaseName() + QStringLiteral(".hints");
    return QFileInfo::exists(sibling) ? sibling : QString{};
}

TSessiondumpHintsProcessor::TSessiondumpHintsProcessor(std::shared_ptr<TSessiondumpHints> hints)
    : cip::tibia::tutorial::TSessiondumpHintsProcessor(nullptr)
    , m_hints(std::move(hints))
{
    updateCurrentHint();
}

TSessiondumpHintsProcessor::~TSessiondumpHintsProcessor() = default;

void TSessiondumpHintsProcessor::updateCurrentHint() { m_currentHint = m_hints->firstUnfinishedHint(); }

int64_t TSessiondumpHintsProcessor::nextHintTimestamp() const
{
    if (m_currentHint)
        return m_currentHint->timestamp();
    return std::numeric_limits<int64_t>::max();
}

bool TSessiondumpHintsProcessor::hasHintToProcess(const int64_t& timestamp) const
{
    return m_currentHint && m_currentHint->timestamp() <= timestamp;
}

void TSessiondumpHintsProcessor::processHints(const int64_t& timestamp)
{
    updateCurrentHint();
    while (m_currentHint && m_currentHint->timestamp() <= timestamp) {
        for (const input::IGameActionPtr& action : m_currentHint->execute())
            Q_EMIT publishGameAction(action);
        if (!m_currentHint || !m_currentHint->isDone())
            return;
        updateCurrentHint();
    }
}

void TSessiondumpHintsProcessor::handleGameAction(const tibia::input::IGameActionPtr& pGameAction)
{
    if (pGameAction->gameActionType() == input::EGameAction::SESSIONDUMP_SKIP_TO) {
        const auto* skipTo = input::gameActionAs<input::TGameActionSessiondumpSkipTo>(pGameAction);
        if (!skipTo)
            return;
        m_hints->skipHintsBefore(skipTo->timestamp());
        updateCurrentHint();
        return;
    }
    if (!m_currentHint || m_currentHint->type() != TSessiondumpHintBase::Condition)
        return;
    if (auto condition = std::dynamic_pointer_cast<TSessiondumpHintCondition>(m_currentHint))
        condition->handleGameAction(pGameAction);
}

}
