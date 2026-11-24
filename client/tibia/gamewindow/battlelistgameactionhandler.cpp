#include "gamewindow/battlelistgameactionhandler.h"

#include "config/sidebaroptions.h"
#include "creatures/battlelistrenderprovider.h"
#include "creatures/creaturestorage.h"
#include "input/gameactioncontext.h"
#include "protocol/protocolcodes.h"

#include <algorithm>
#include <functional>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;
using TDrawCommand = creatures::TBattleListDrawCommand;

/// The sort keys of TBattleListOptions::sortOrder.
enum ESortKey
{
    BY_AGE_ASCENDING = 0,
    BY_AGE_DESCENDING,
    BY_DISTANCE_ASCENDING,
    BY_DISTANCE_DESCENDING,
    BY_HITPOINTS_ASCENDING,
    BY_HITPOINTS_DESCENDING,
    BY_NAME_ASCENDING,
    BY_NAME_DESCENDING,
};

/// The filter keys of TBattleListOptions::filters.
enum EFilterKey
{
    HIDE_PLAYERS = 0,
    HIDE_KNIGHTS,
    HIDE_PALADINS,
    HIDE_DRUIDS,
    HIDE_SORCERERS,
    HIDE_MONKS,
    HIDE_NPCS,
    HIDE_MONSTERS,
    HIDE_NON_SKULLED_PLAYERS,
    HIDE_PARTY_MEMBERS,
    HIDE_PLAYER_SUMMONS,
    HIDE_MEMBERS_OF_OWN_GUILD,
};

}

TBattleListGameActionHandler::TBattleListGameActionHandler(config::TSidebarOptions* sidebarOptions,
                                                           std::shared_ptr<creatures::TCreatureStorage> creatureStorage,
                                                           std::shared_ptr<creatures::TBattleListRenderProvider> battleListRenderProvider,
                                                           QObject* parent)
    : cip::tibia::gamewindow::TBattleListGameActionHandler(parent)
    , m_sidebarOptions(sidebarOptions)
    , m_creatureStorage(std::move(creatureStorage))
    , m_battleListRenderProvider(std::move(battleListRenderProvider))
{}

void TBattleListGameActionHandler::filterDrawCommands(std::vector<TDrawCommand>& DrawCommands,
                                                      const config::TBattleListOptions& BattleListWidgetOptions,
                                                      const creatures::TCreatureStorage& CreatureStorage)
{
    const std::function<bool(uint32_t)> hidden = [&BattleListWidgetOptions, &CreatureStorage](const uint32_t creatureId) {
        const std::shared_ptr<creatures::TCreature> creature = CreatureStorage.creatureAt(creatureId);
        if (!creature)
            return true;
        const uint8_t type = creature->creatureType();
        const bool partyMember = creature->isPartyMember();
        const uint8_t emblem = creature->emblem();
        const bool player = type == Proto::CreatureTypePlayer;
        // The first filter that applies hides the creature.
        for (const int filter : BattleListWidgetOptions.filters) {
            bool applies = false;
            switch (filter) {
                case HIDE_PLAYERS: applies = player; break;
                case HIDE_KNIGHTS: applies = player && creature->vocation() == 1; break;
                case HIDE_PALADINS: applies = player && creature->vocation() == 2; break;
                case HIDE_DRUIDS: applies = player && creature->vocation() == 4; break;
                case HIDE_SORCERERS: applies = player && creature->vocation() == 3; break;
                case HIDE_MONKS: applies = player && creature->vocation() == 5; break;
                case HIDE_NPCS: applies = type == Proto::CreatureTypeNpc; break;
                case HIDE_MONSTERS: applies = type == Proto::CreatureTypeMonster; break;
                case HIDE_NON_SKULLED_PLAYERS: applies = player && creature->skull() == 0; break;
                case HIDE_PARTY_MEMBERS: applies = partyMember; break;
                case HIDE_PLAYER_SUMMONS: applies = type == Proto::CreatureTypeSummonOwn; break;
                case HIDE_MEMBERS_OF_OWN_GUILD: applies = emblem == 4 || emblem == 1; break;
                default: break;
            }
            if (applies)
                return true;
        }
        return false;
    };
    std::erase_if(DrawCommands, [&hidden](const TDrawCommand& command) { return hidden(command.creatureId); });
}

void TBattleListGameActionHandler::sortDrawCommands(std::vector<TDrawCommand>& DrawCommands,
                                                    const config::TBattleListOptions& BattleListWidgetOptions)
{
    const std::vector<int> sortOrder = BattleListWidgetOptions.sortOrder;
    for (auto key = sortOrder.rbegin(); key != sortOrder.rend(); ++key) {
        switch (*key) {
            case BY_AGE_ASCENDING:
                std::sort(DrawCommands.begin(), DrawCommands.end(),
                          [](const TDrawCommand& a, const TDrawCommand& b) { return a.visibleOrder < b.visibleOrder; });
                break;
            case BY_AGE_DESCENDING:
                std::sort(DrawCommands.begin(), DrawCommands.end(),
                          [](const TDrawCommand& a, const TDrawCommand& b) { return b.visibleOrder < a.visibleOrder; });
                break;
            case BY_DISTANCE_ASCENDING:
                std::sort(DrawCommands.begin(), DrawCommands.end(),
                          [](const TDrawCommand& a, const TDrawCommand& b) { return a.distance < b.distance; });
                break;
            case BY_DISTANCE_DESCENDING:
                std::sort(DrawCommands.begin(), DrawCommands.end(),
                          [](const TDrawCommand& a, const TDrawCommand& b) { return b.distance < a.distance; });
                break;
            case BY_HITPOINTS_ASCENDING:
                std::sort(DrawCommands.begin(), DrawCommands.end(),
                          [](const TDrawCommand& a, const TDrawCommand& b) { return a.healthPercent < b.healthPercent; });
                break;
            case BY_HITPOINTS_DESCENDING:
                std::sort(DrawCommands.begin(), DrawCommands.end(),
                          [](const TDrawCommand& a, const TDrawCommand& b) { return b.healthPercent < a.healthPercent; });
                break;
            case BY_NAME_ASCENDING:
                std::sort(DrawCommands.begin(), DrawCommands.end(), [](const TDrawCommand& a, const TDrawCommand& b) {
                    return QString::compare(a.name, b.name, Qt::CaseSensitive) < 0;
                });
                break;
            case BY_NAME_DESCENDING:
                std::sort(DrawCommands.begin(), DrawCommands.end(), [](const TDrawCommand& a, const TDrawCommand& b) {
                    return QString::compare(a.name, b.name, Qt::CaseSensitive) > 0;
                });
                break;
            default: break;
        }
    }
}

void TBattleListGameActionHandler::handleGameAction(const input::IGameActionPtr& pGameAction)
{
    if (!pGameAction)
        return;
    switch (pGameAction->gameActionType()) {
        case A::ATTACK_FIRST_TARGET: attackFirstTarget(); break;
        case A::ATTACK_NEXT_TARGET: attackNextOrPreviousTarget(false); break;
        case A::ATTACK_PREVIOUS_TARGET: attackNextOrPreviousTarget(true); break;
        case A::ADD_GUI_TUTORIAL_MARKER:
            if (const auto* marker = input::gameActionAs<input::TGameActionAddGUITutorialMarker>(pGameAction))
                handleAddGUITutorialMarkerAction(*marker);
            break;
        case A::REMOVE_ALL_TUTORIAL_MARKERS:
            if (m_battleListRenderProvider)
                m_battleListRenderProvider->removeAllTutorialMarkers();
            break;
        default: break;
    }
}

void TBattleListGameActionHandler::handleAttackFirstTargetGameAction() { attackFirstTarget(); }

void TBattleListGameActionHandler::handleAttackNextTargetGameAction() { attackNextOrPreviousTarget(false); }

void TBattleListGameActionHandler::handleAttackPreviousTargetGameAction() { attackNextOrPreviousTarget(true); }

void TBattleListGameActionHandler::handleAddGUITutorialMarkerAction(const input::TGameActionAddGUITutorialMarker& pGameAction)
{
    const input::IGameActionContext* location = pGameAction.location().get();
    const input::IGameActionContext* object = pGameAction.object().get();
    if (!location || location->gameActionContextType() != input::EGameActionContextType::LOCATION_BATTLE_LIST || !object
        || object->gameActionContextType() != input::EGameActionContextType::OBJECT_CREATURE)
        return;
    const auto* creature = dynamic_cast<const input::TObjectCreatureGameActionContext*>(object);
    if (creature && m_battleListRenderProvider)
        m_battleListRenderProvider->setTutorialMarker(creature->creatureId(), pGameAction.markerId());
}

void TBattleListGameActionHandler::handleRemoveAllTutorialMarkersGameAction()
{
    if (m_battleListRenderProvider)
        m_battleListRenderProvider->removeAllTutorialMarkers();
}

config::TBattleListOptions TBattleListGameActionHandler::battleListWidgetOptions() const
{
    // Host: CipSoft's sidebar options always hold a primary battle list.
    if (m_sidebarOptions) {
        for (const auto& [id, options] : m_sidebarOptions->battleListsOptions()) {
            if (options.isPrimary)
                return m_sidebarOptions->primaryBattleListOptions();
        }
    }
    return {};
}

bool TBattleListGameActionHandler::attackIfPossible(const TDrawCommand& command, const uint32_t attackTargetId)
{
    const std::shared_ptr<creatures::TCreature> creature = m_creatureStorage->creatureAt(command.creatureId);
    if (!creature || creature->creatureType() == Proto::CreatureTypeNpc || creature->creatureId() == attackTargetId
        || !creature->isPlacedOnField())
        return false;
    Q_EMIT publishGameAction(input::makeCreatureAction(A::ATTACK_CREATURE, creature->creatureId()));
    return true;
}

void TBattleListGameActionHandler::attackFirstTarget()
{
    if (!m_battleListRenderProvider || !m_creatureStorage)
        return;
    std::vector<TDrawCommand> commands = m_battleListRenderProvider->drawCommands();
    const config::TBattleListOptions options = battleListWidgetOptions();
    filterDrawCommands(commands, options, *m_creatureStorage);
    sortDrawCommands(commands, options);
    if (commands.empty())
        return;
    attackIfPossible(commands.front(), m_creatureStorage->attackID());
}

void TBattleListGameActionHandler::attackNextOrPreviousTarget(const bool Previous)
{
    if (!m_battleListRenderProvider || !m_creatureStorage)
        return;
    std::vector<TDrawCommand> commands = m_battleListRenderProvider->drawCommands();
    const config::TBattleListOptions options = battleListWidgetOptions();
    filterDrawCommands(commands, options, *m_creatureStorage);
    sortDrawCommands(commands, options);
    if (commands.empty())
        return;
    if (Previous)
        std::reverse(commands.begin(), commands.end());
    const uint32_t attackTargetId = m_creatureStorage->attackID();
    if (attackTargetId != 0) {
        // The list starts after the attack target (from the top when it is
        // the last row or not in the list).
        size_t start = commands.size();
        for (size_t i = 0; i < commands.size(); ++i) {
            if (commands[i].creatureId == attackTargetId) {
                start = i + 1;
                break;
            }
        }
        if (start >= commands.size())
            start %= commands.size();
        if (start != 0)
            std::rotate(commands.begin(), commands.begin() + static_cast<std::ptrdiff_t>(start), commands.end());
    }
    for (const TDrawCommand& command : commands) {
        if (attackIfPossible(command, attackTargetId))
            break;
    }
}

}
