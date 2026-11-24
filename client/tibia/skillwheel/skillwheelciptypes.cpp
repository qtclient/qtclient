#include "skillwheel/skillwheelciptypes.h"

#include "cyclopedia/cyclopediaciptypes.h"
#include "game/gameciptypes.h"
#include <vector>
#include <map>
#include <client/staticdata.h>

#include "protocol/protocolciptypes.h"

#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "skillwheel/skillwheeldata.h"
#include "skillwheel/skillwheelgameactions.h"
#include "skillwheel/skillwheelstorage.h"

#include <creatures/creaturestorage.h>
#include <tibia/game/tgamenotify.h>

#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>

#include <protocol.pb.h>
#include <tibia/game/gamesession_forward.h>

#include <stdexcept>
#include <utility>

namespace tibia::skillwheel {

namespace {

using ::tibia::protobuf::protocol::GameserverMessageGemAtelierGemResponse;
using ::tibia::protobuf::protocol::GameserverMessageSkillWheel;
} // namespace

void TSkillWheelProtocolMessageHandler::requestSkillWheel(const uint32_t playerId)
{
    protobuf::protocol::GameclientMessageRequestSkillWheel message;
    message.set_character_id(playerId);
    Q_EMIT sendRequestSkillWheel(message);
}

void TSkillWheelProtocolMessageHandler::saveSkillWheel(const TWheel& wheel)
{
    protobuf::protocol::GameclientMessageApplySkillWheel message;
    for (const int points : wheel.points)
        message.add_slot_points(static_cast<uint32_t>(points));
    for (const std::optional<TGem>& gem : wheel.vessels)
        message.add_vessel_gems(gem ? static_cast<uint32_t>(gem->id & 0xffff) : UINT32_MAX);
    Q_EMIT sendApplySkillWheel(message);
}

void TSkillWheelProtocolMessageHandler::requestCharacterSkillWheel(const uint32_t characterId)
{
    protobuf::protocol::GameclientMessageOpenCyclopediaCharacterInfo message;
    message.set_character_id(characterId);
    message.set_info_type(static_cast<uint32_t>(tibia::client::CYCLOPEDIA_CHARACTERINFO_WHEEL));
    Q_EMIT sendRequestSkillWheelViaCyclopedia(message);
}

void TSkillWheelProtocolMessageHandler::sendGemAtelier(const uint32_t action, const uint16_t value, const uint8_t secondValue)
{
    // Dismantle, reshape, lock and attempted reshape name a gem; reveal names a
    // quality and the mod grade enhancement a mod type.
    protobuf::protocol::GameclientMessageGemAtelier message;
    message.set_action(action);
    if (action == 0 || action == 2 || action == 3 || action == 5)
        message.set_gem_index(value);
    else
        message.set_first_value(static_cast<uint8_t>(value));
    message.set_second_value(secondValue);
    Q_EMIT sendGemAtelierAction(message);
}

void TSkillWheelProtocolMessageHandler::dismantleGem(const int gemId)
{
    sendGemAtelier(0, static_cast<uint16_t>(gemId), 0);
}

void TSkillWheelProtocolMessageHandler::revealGem(const int quality)
{
    if (static_cast<unsigned>(quality) >= 3)
        throw std::runtime_error("unhandled enum value");
    sendGemAtelier(1, static_cast<uint16_t>(quality), 0);
}

void TSkillWheelProtocolMessageHandler::reshapeGem(const int gemId, const int modIndex)
{
    sendGemAtelier(2, static_cast<uint16_t>(gemId), static_cast<uint8_t>(modIndex));
}

void TSkillWheelProtocolMessageHandler::attemptReshapeGem(const int gemId, const int modIndex)
{
    sendGemAtelier(5, static_cast<uint16_t>(gemId), static_cast<uint8_t>(modIndex));
}

void TSkillWheelProtocolMessageHandler::toggleGemLock(const int gemId)
{
    sendGemAtelier(3, static_cast<uint16_t>(gemId), 0);
}

void TSkillWheelProtocolMessageHandler::enhanceModGrade(const int modType, const int modId)
{
    sendGemAtelier(4, static_cast<uint16_t>(static_cast<uint8_t>(modType)), static_cast<uint8_t>(modId));
}

void TSkillWheelProtocolMessageHandler::setSkillWheelStorage(TSkillWheelStorage* storage)
{
    m_storage = storage;
}

TSkillWheelProtocolMessageHandler::TSkillWheelProtocolMessageHandler(
    TSkillWheelStorage* skillWheelStorage, std::shared_ptr<creatures::TCreatureStorage> creatureStorage,
    std::shared_ptr<cyclopedia::TCharacterInfoStorage> characterInfoStorage, std::shared_ptr<game::TPlayerData> playerData)
    : m_storage(skillWheelStorage)
    , m_creatureStorage(std::move(creatureStorage))
    , m_characterInfoStorage(std::move(characterInfoStorage))
    , m_playerData(std::move(playerData))
{
    // Host: the resource balances reach the handler through the engine's notify.
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::resourcesBalanceChanged, this,
            [this](const qint64 value, qint64, const int type) { handleResourceBalanceMessage(type, value); });
}

namespace {

/// The vocation of a wheel message (1 knight .. 5 monk).
int wheelVocation(const uint32_t vocation)
{
    if (vocation < 1 || vocation > static_cast<uint32_t>(VocationCount))
        throw std::runtime_error("unhandled enum value");
    return static_cast<int>(vocation);
}

}

namespace {

/// CIP: a mod's grade; grade 0 drops the mod.
void setModGrade(TGrades& grades, const int mod, const int grade)
{
    if (grade == 0)
        grades.erase(mod);
    else
        grades[mod] = grade;
}

}

void TSkillWheelProtocolMessageHandler::handleSkillWheelMessage(const GameserverMessageSkillWheel& m)
{
    if (!m.can_view()) {
        const int profession = tibia::game::hasLocalPlayer() ? tibia::game::localPlayerVocation() % 10 : 0;
        const bool hasVocation = profession >= 1 && profession <= VocationCount;
        Q_EMIT publishGameAction(input::makeGameAction(hasVocation ? input::EGameAction::OPEN_SKILL_WHEEL_DIALOG_PREVIEW_POSSIBLE_MESSAGE
                                                     : input::EGameAction::OPEN_SKILL_WHEEL_DIALOG_NO_WHEEL_MESSAGE));
        return;
    }
    if (!m_storage)
        return;
    TWheel wheel;
    wheel.vocation = wheelVocation(m.vocation());
    for (int slot = 0; slot < SlotCount && slot < m.slot_points_size(); ++slot)
        wheel.points[slot] = static_cast<int>(m.slot_points(slot));

    std::vector<std::pair<int, int>> scrolls;
    for (const auto& scroll : m.scrolls())
        scrolls.emplace_back(static_cast<uint16_t>(scroll.object_id()), static_cast<uint8_t>(scroll.points()));

    // The name comes from the known creature, else from the own character; the own flag with it.
    QString name = qtTrId("skill_wheel_dialog_name_dummy");
    bool ownCharacter = false;
    const std::shared_ptr<creatures::TCreature> creature =
        m_creatureStorage ? m_creatureStorage->creatureAt(m.player_id()) : nullptr;
    if (creature) {
        ownCharacter = creature->isLocalPlayer();
        name = creature->name();
    } else if (tibia::game::hasLocalPlayer() && tibia::game::localPlayerId() == m.player_id()) {
        name = QString::fromStdString(tibia::game::localPlayerName());
        ownCharacter = true;
    }

    std::vector<TGem> gems;
    for (const auto& g : m.gems())
        gems.push_back({ static_cast<int>(static_cast<uint16_t>(g.gem_id())), g.locked(), static_cast<int>(g.domain()),
                         static_cast<int>(g.quality()), static_cast<int>(g.first_modifier()), static_cast<int>(g.second_modifier()),
                         static_cast<int>(g.supreme_modifier()) });
    for (int vessel = 0; vessel < VesselCount && vessel < m.equipped_gems_size(); ++vessel) {
        const int32_t id = m.equipped_gems(vessel);
        if (id == -1)
            continue;
        const auto it = std::ranges::find_if(gems, [id](const TGem& g) { return static_cast<uint16_t>(g.id) == static_cast<uint16_t>(id); });
        if (it != gems.end())
            socketGem(wheel, *it, vessel);
    }

    TGrades basicGrades, supremeGrades;
    for (const auto& g : m.basic_grades())
        setModGrade(basicGrades, static_cast<int>(g.modifier()), static_cast<int>(g.grade()));
    for (const auto& g : m.supreme_grades())
        setModGrade(supremeGrades, static_cast<int>(g.modifier()), static_cast<int>(g.grade()));

    m_storage->setGemInventory(std::move(gems));
    m_storage->setModGrades(std::move(basicGrades), std::move(supremeGrades));
    if (m.change_state() > 2)
        throw std::runtime_error("unhandled enum value");
    m_storage->setServerData(static_cast<int>(m.change_state()), name, ownCharacter, static_cast<int>(m.points()),
                             static_cast<int>(m.extra_points()), scrolls, m.monk_quest(), static_cast<int>(m.hunting_task_points()), wheel);
    m_storage->updateModList();
    Q_EMIT publishGameAction(input::makeGameAction(input::EGameAction::OPEN_OR_UPDATE_SKILL_WHEEL_DIALOG));
}

void TSkillWheelProtocolMessageHandler::handleResourceBalanceMessage(const int type, const qint64 balance)
{
    if (!m_storage)
        return;
    const int value = static_cast<int>(balance);
    switch (type) {
        case tibia::client::RESOURCE_WHEEL_POINTS: m_storage->setUnspentSkillPoints(value); break;
        case tibia::client::RESOURCE_LESSER_GEMS: m_storage->setUnrevealedGems(0, value); break;
        case tibia::client::RESOURCE_REGULAR_GEMS: m_storage->setUnrevealedGems(1, value); break;
        case tibia::client::RESOURCE_GREATER_GEMS: m_storage->setUnrevealedGems(2, value); break;
        case tibia::client::RESOURCE_LESSER_FRAGMENT: m_storage->setLesserFragments(value); break;
        case tibia::client::RESOURCE_GREATER_FRAGMENT: m_storage->setGreaterFragments(value); break;
        default: break;
    }
}

void TSkillWheelProtocolMessageHandler::handleGemAtelierGemResponseMessage(const GameserverMessageGemAtelierGemResponse& Message)
{
    if (Message.type() == 1) {
        if (!m_storage)
            return;
        const auto& gem = Message.gem();
        TGemMods mods;
        mods.basic1 = static_cast<int>(gem.first_modifier());
        if (gem.has_second_modifier())
            mods.basic2 = static_cast<int>(gem.second_modifier());
        if (gem.has_supreme_modifier())
            mods.supreme = static_cast<int>(gem.supreme_modifier());
        m_storage->setTemporaryReshapedGem(mods);
    } else if (Message.type() == 0) {
        Q_EMIT publishGameAction(std::make_shared<input::gameactions::TSelectAtelierGemGameAction>(static_cast<uint16_t>(Message.gem_id())));
    }
}

TSkillWheelGameActionHandler::TSkillWheelGameActionHandler(std::shared_ptr<TSkillWheelProtocolMessageHandler> pProtocolMessageHandler,
                                                           const std::weak_ptr<creatures::TCreature>& pPlayerWeak)
    : m_pProtocolMessageHandler(std::move(pProtocolMessageHandler))
{
    onPlayerCreatureAddedToCreatureStorage(pPlayerWeak);
}

void TSkillWheelGameActionHandler::handleGameAction(const input::IGameAction& action)
{
    switch (action.gameActionType()) {
        case input::EGameAction::SAVE_SKILL_WHEEL:
            if (const auto* save = input::gameActionAs<input::gameactions::TGameActionSaveSkillWheel>(&action))
                handleSaveSkillWheelGameAction(save->wheel());
            return;
        case input::EGameAction::REQUEST_OWN_SKILL_WHEEL: handleRequestOnwSkillWheelGameAction(); return;
        case input::EGameAction::REQUEST_SKILL_WHEEL: handleCreatureGameAction(input::creatureIdOf(action)); return;
        default: return;
    }
}

void TSkillWheelGameActionHandler::handleGameAction(const input::IGameActionPtr& pGameAction)
{
    if (pGameAction)
        handleGameAction(*pGameAction);
}

void TSkillWheelGameActionHandler::onPlayerCreatureAddedToCreatureStorage(std::weak_ptr<creatures::TCreature> pPlayerWeak)
{
    const std::shared_ptr<creatures::TCreature> player = pPlayerWeak.lock();
    m_playerId = player ? player->creatureId() : 0;
}

void TSkillWheelGameActionHandler::handleCreatureGameAction(const uint32_t creatureId)
{
    if (creatureId == m_playerId) {
        handleRequestOnwSkillWheelGameAction();
        return;
    }
    m_pProtocolMessageHandler->requestSkillWheel(creatureId);
}

void TSkillWheelGameActionHandler::setPremiumTimeStorage(game::TPremiumTimeStorage* premiumTimeStorage)
{
    m_premiumTimeStorage = premiumTimeStorage;
}

void TSkillWheelGameActionHandler::handleSaveSkillWheelGameAction(const TWheel& wheel)
{
    m_pProtocolMessageHandler->saveSkillWheel(wheel);
}

void TSkillWheelGameActionHandler::handleRequestOnwSkillWheelGameAction()
{
    if (m_playerId == 0 || !tibia::game::hasLocalPlayer())
        return;
    if (tibia::game::localPlayerLevel() > RequiredLevel - 1 && (tibia::game::localPlayerIsPremium() || (m_premiumTimeStorage && !m_premiumTimeStorage->isPremium()))) {
        m_pProtocolMessageHandler->requestSkillWheel(m_playerId);
        return;
    }
    const int profession = tibia::game::localPlayerVocation() % 10;
    const bool hasVocation = profession >= 1 && profession <= VocationCount;
    Q_EMIT publishGameAction(input::makeGameAction(hasVocation ? input::EGameAction::OPEN_SKILL_WHEEL_DIALOG_PREVIEW_POSSIBLE_MESSAGE
                                                 : input::EGameAction::OPEN_SKILL_WHEEL_DIALOG_NO_WHEEL_MESSAGE));
}

} // namespace tibia::skillwheel
