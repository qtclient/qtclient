#include "skillwheel/skillwheelciptypes.h"
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

namespace tibia::skillwheel {

namespace {
TSkillWheelProtocolMessageHandler* g_liveSkillWheelProtocolHandler = nullptr;

using ::tibia::protobuf::protocol::GameserverMessageGemAtelierGemRevealed;
using ::tibia::protobuf::protocol::GameserverMessageSkillWheel;
} // namespace

void TSkillWheelProtocolMessageHandler::requestSkillWheel(const uint32_t playerId)
{
    auto message = protocol::newGameclientMessage(97);
    message->MutableExtension(protobuf::protocol::GameclientMessageRequestSkillWheel::gameclient_message_request_skill_wheel)
        ->set_character_id(playerId);
    protocol::sendGameclientMessage(std::move(message));
}

void TSkillWheelProtocolMessageHandler::saveSkillWheel(const TWheel& wheel)
{
    auto message = protocol::newGameclientMessage(98);
    auto* m = message->MutableExtension(protobuf::protocol::GameclientMessageApplySkillWheel::gameclient_message_apply_skill_wheel);
    for (const int points : wheel.points)
        m->add_slot_points(static_cast<uint32_t>(points));
    for (const std::optional<TGem>& gem : wheel.vessels)
        m->add_vessel_gems(gem ? static_cast<uint32_t>(gem->id & 0xffff) : UINT32_MAX);
    protocol::sendGameclientMessage(std::move(message));
}

void TSkillWheelProtocolMessageHandler::dismantleGem(const int gemId)
{
    tibia::game::gemAction(0, static_cast<uint16_t>(gemId), 0);
}

void TSkillWheelProtocolMessageHandler::revealGem(const int quality)
{
    if (static_cast<unsigned>(quality) >= 3)
        throw std::runtime_error("unhandled enum value");
    tibia::game::gemAction(1, static_cast<uint16_t>(quality), 0);
}

void TSkillWheelProtocolMessageHandler::reshapeGem(const int gemId, const int modIndex)
{
    tibia::game::gemAction(2, static_cast<uint16_t>(gemId), static_cast<uint8_t>(modIndex));
}

void TSkillWheelProtocolMessageHandler::toggleGemLock(const int gemId)
{
    tibia::game::gemAction(3, static_cast<uint16_t>(gemId), 0);
}

void TSkillWheelProtocolMessageHandler::enhanceModGrade(const int modType, const int modId)
{
    tibia::game::gemAction(4, static_cast<uint16_t>(static_cast<uint8_t>(modType)), static_cast<uint8_t>(modId));
}

TSkillWheelProtocolMessageHandler::TSkillWheelProtocolMessageHandler(QObject* parent)
    : cip::tibia::skillwheel::TSkillWheelProtocolMessageHandler(parent)
{
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::resourcesBalanceChanged, this,
            [this](const qint64 value, qint64, const int type) { handleResourceBalanceMessage(type, value); });
}

TSkillWheelProtocolMessageHandler* TSkillWheelProtocolMessageHandler::liveInstance()
{
    return g_liveSkillWheelProtocolHandler;
}

void TSkillWheelProtocolMessageHandler::setLiveInstance(TSkillWheelProtocolMessageHandler* handler)
{
    g_liveSkillWheelProtocolHandler = handler;
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
        input::publish(input::makeAction(hasVocation ? input::EGameAction::OPEN_SKILL_WHEEL_DIALOG_PREVIEW_POSSIBLE_MESSAGE
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
        creatures::TCreatureStorage::instance() ? creatures::TCreatureStorage::instance()->creatureAt(m.player_id()) : nullptr;
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
    input::publish(input::makeAction(input::EGameAction::OPEN_OR_UPDATE_SKILL_WHEEL_DIALOG));
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

void TSkillWheelProtocolMessageHandler::handleGemAtelierGemRevealedMessage(
    const GameserverMessageGemAtelierGemRevealed& Message)
{
    auto action = input::mutableClone(*input::makeAction(input::EGameAction::GEM_ATELIER_SELECT_GEM));
    action->parameter = static_cast<uint16_t>(Message.gem_id());
    input::publish(action);
}

void TSkillWheelGameActionHandler::handleGameAction(const input::TGameAction& action)
{
    switch (action.type) {
        case input::EGameAction::SAVE_SKILL_WHEEL:
            if (const auto* save = dynamic_cast<const TGameActionSaveSkillWheelConfiguration*>(&action))
                TSkillWheelProtocolMessageHandler::saveSkillWheel(save->wheel);
            return;
        case input::EGameAction::REQUEST_OWN_SKILL_WHEEL: handleRequestOnwSkillWheelGameAction(); return;
        case input::EGameAction::REQUEST_SKILL_WHEEL: handleCreatureGameAction(action.creatureId); return;
        default: return;
    }
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
    TSkillWheelProtocolMessageHandler::requestSkillWheel(creatureId);
}

void TSkillWheelGameActionHandler::handleSaveSkillWheelGameAction(const TWheel& wheel)
{
    TSkillWheelProtocolMessageHandler::saveSkillWheel(wheel);
}

void TSkillWheelGameActionHandler::handleRequestOnwSkillWheelGameAction()
{
    if (!tibia::game::hasLocalPlayer() || tibia::game::localPlayerId() == 0)
        return;
    if (tibia::game::localPlayerLevel() > RequiredLevel - 1 && (tibia::game::localPlayerIsPremium() || !m_sessionPremium)) {
        TSkillWheelProtocolMessageHandler::requestSkillWheel(tibia::game::localPlayerId());
        return;
    }
    const int profession = tibia::game::localPlayerVocation() % 10;
    const bool hasVocation = profession >= 1 && profession <= VocationCount;
    input::publish(input::makeAction(hasVocation ? input::EGameAction::OPEN_SKILL_WHEEL_DIALOG_PREVIEW_POSSIBLE_MESSAGE
                                                 : input::EGameAction::OPEN_SKILL_WHEEL_DIALOG_NO_WHEEL_MESSAGE));
}

void TSkillWheelProtocolMessageHandler::onServerMessageEnqueued(int opcode)
{

    switch (opcode) {
    case 95: {
        const auto* shadow = protocol::lastShadowServerMessage();
        if (!shadow || !shadow->HasExtension(GameserverMessageSkillWheel::gameserver_message_skill_wheel))
            return;
        handleSkillWheelMessage(shadow->GetExtension(GameserverMessageSkillWheel::gameserver_message_skill_wheel));
        break;
    }
    case 197: {
        const auto* shadow = protocol::lastShadowServerMessage();
        if (!shadow
            || !shadow->HasExtension(
                GameserverMessageGemAtelierGemRevealed::gameserver_message_gem_atelier_gem_revealed))
            return;
        handleGemAtelierGemRevealedMessage(shadow->GetExtension(
            GameserverMessageGemAtelierGemRevealed::gameserver_message_gem_atelier_gem_revealed));
        break;
    }
    default:
        break;
    }
}

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    auto* handler = new TSkillWheelProtocolMessageHandler(parent);
    handler->setSkillWheelStorage(TSkillWheelStorage::liveInstance());
    TSkillWheelProtocolMessageHandler::setLiveInstance(handler);
}

} // namespace tibia::skillwheel
