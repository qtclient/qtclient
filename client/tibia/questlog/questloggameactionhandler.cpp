#include "questlog/questloggameactionhandler.h"

#include <tibia/game/gamesession.h>

#include <utility>
#include <vector>

namespace tibia::questlog {

using input::EGameAction;

TQuestLogGameActionHandler::TQuestLogGameActionHandler(std::shared_ptr<TQuestLogStorage> questLogStorage)
    : cip::tibia::questlog::TQuestLogGameActionHandler(nullptr)
    , m_questLogStorage(std::move(questLogStorage))
{
}

void TQuestLogGameActionHandler::handleGameAction(const input::IGameActionPtr& pGameAction)
{
    if (!pGameAction)
        return;
    switch (pGameAction->gameActionType()) {
    case EGameAction::CHANGE_QUESTTRACKER_OPTIONS:
        if (const auto* action = input::gameActionAs<input::TGameActionChangeQuesttrackerOptions>(pGameAction))
            handleChangeQuesttrackerOptionsGameAction(*action);
        return;
    case EGameAction::CHANGE_TRACKED_QUESTFLAGS:
        if (const auto* action = input::gameActionAs<input::TGameActionChangeTrackedQuestFlags>(pGameAction))
            handleChangeTrackedQuestFlagsGameAction(*action);
        return;
    default:
        return;
    }
}

void TQuestLogGameActionHandler::handleChangeQuesttrackerOptionsGameAction(const input::TGameActionChangeQuesttrackerOptions& pGameAction)
{
    if (!m_questLogStorage)
        return;
    if (pGameAction.option() == 0)
        m_questLogStorage->setAutoTrackNewQuests(pGameAction.enabled());
    else if (pGameAction.option() == 1)
        m_questLogStorage->setAutoUntrackCompletedQuests(pGameAction.enabled());
    trackQuestFlags(trackedQuestFlagIds());
}

void TQuestLogGameActionHandler::handleChangeTrackedQuestFlagsGameAction(const input::TGameActionChangeTrackedQuestFlags& pGameAction)
{
    if (!m_questLogStorage)
        return;
    std::set<TQuestFlagID> questFlagIds = trackedQuestFlagIds();
    // CIP: mode 0 adds the ids, mode 1 removes them (qtclient: track()).
    for (const int questId : pGameAction.questIds()) {
        const auto questFlagId = static_cast<TQuestFlagID>(questId);
        if (pGameAction.track())
            questFlagIds.insert(questFlagId);
        else
            questFlagIds.erase(questFlagId);
    }
    trackQuestFlags(questFlagIds);
}

std::set<TQuestFlagID> TQuestLogGameActionHandler::trackedQuestFlagIds() const
{
    std::set<TQuestFlagID> questFlagIds;
    for (const TQuestFlag& questFlag : m_questLogStorage->trackedQuestFlags())
        questFlagIds.insert(questFlag.id);
    return questFlagIds;
}

void TQuestLogGameActionHandler::trackQuestFlags(const std::set<TQuestFlagID>& questFlagIds)
{
    ::tibia::protobuf::protocol::GameclientMessageTrackQuestFlags message;
    for (const TQuestFlagID questFlagId : questFlagIds)
        message.add_mission_ids(questFlagId);
    message.set_auto_track_new_quests(m_questLogStorage->autoTrackNewQuests());
    message.set_auto_untrack_completed_quests(m_questLogStorage->autoUntrackCompletedQuests());
    Q_EMIT sendTrackQuestFlags(message);
}

}
