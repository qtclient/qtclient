#include "tutorial/sessiondumphintcondition.h"

#include "input/gameaction.h"
#include "input/gameactioncontext.h"
#include "input/playermovementintent.h"
#include "jsonvalidator.h"
#include "qmlcomponents/tutorialhelper.h"
#include "tutorial/sessiondumphintconditionshowtutorialhint.h"

#include <QCoreApplication>
#include <QJsonValue>
#include <QString>

#include <stdexcept>

namespace tibia::tutorial {

using input::EGameAction;
using input::gameActionAs;

namespace {

constexpr const char* InvalidHint = "Invalid sessiondump hint json object";

using TConditionPtr = std::shared_ptr<TSessiondumpHintCondition>;

/// The object's `key` passes `check` (a TJsonValueValidator member).
bool has(const QJsonObject& object, const char* key, shared::TJsonValueValidator& (shared::TJsonValueValidator::*check)())
{
    shared::TJsonObjectValidator validator(object);
    shared::TJsonValueValidator value = validator.value(QString::fromLatin1(key));
    return (value.*check)().isValid();
}

void require(const QJsonObject& object, const char* key, shared::TJsonValueValidator& (shared::TJsonValueValidator::*check)())
{
    if (!has(object, key, check))
        throw std::invalid_argument(InvalidHint);
}

int intAt(const QJsonObject& object, const char* key) { return object[QString::fromLatin1(key)].toInt(); }

QJsonObject objectAt(const QJsonObject& object, const char* key) { return object[QString::fromLatin1(key)].toObject(); }

}

TSessiondumpHintCondition::TSessiondumpHintCondition(input::IGameActionPtr expectedGameAction)
    : TSessiondumpHintBase(Condition)
    , m_expectedGameAction(std::move(expectedGameAction))
{
}

TSessiondumpHintCondition::~TSessiondumpHintCondition() = default;

input::TGameActionList TSessiondumpHintCondition::execute()
{
    input::TGameActionList actions;
    if (m_state == GameActionReceived) {
        actions.push_back(std::make_shared<input::TGameActionRemoveAllTutorialMarkers>());
        m_done = true;
    } else if (m_state == NotStarted) {
        actions = gameActions();
        m_state = WaitingForGameAction;
    }
    return actions;
}

void TSessiondumpHintCondition::handleGameAction(const input::IGameActionPtr& gameAction)
{
    if (m_state != WaitingForGameAction)
        return;
    if (gameActionsMatch(m_expectedGameAction, gameAction))
        m_state = GameActionReceived;
}

input::IGameActionPtr TSessiondumpHintCondition::expectedGameAction() const { return m_expectedGameAction; }

input::TGameActionList TSessiondumpHintCondition::gameActions() const { return m_gameActions; }

void TSessiondumpHintCondition::addGameAction(const input::IGameActionPtr& gameAction) { m_gameActions.push_back(gameAction); }

namespace {

/// Both contexts are set and equal.
bool contextsEqual(const input::TGameActionContextPtr& first, const input::TGameActionContextPtr& second)
{
    return first && second && input::gameActionContextsEqual(*first, *second);
}

/// Both contexts are set and mean the same place or object.
bool contextsMatch(const input::TGameActionContextPtr& first, const input::TGameActionContextPtr& second)
{
    return first && second && input::gameActionContextsMatch(*first, *second);
}

/// The expected object's appearance type is the one of the moved object, when
/// that is an appearance type object.
bool sameMovedObjectType(const input::TGameActionContextPtr& expected, const input::TGameActionContextPtr& actual)
{
    if (!expected || !actual || actual->gameActionContextType() != input::EGameActionContextType::OBJECT_APPEARANCE_TYPE)
        return false;
    return input::appearanceTypeIdOf(expected.get()) == input::appearanceTypeIdOf(actual.get());
}

/// A move's destination as a location (qtclient keeps it as a classic position).
input::TGameActionContextPtr destinationOf(const input::TGameActionMoveObject& move)
{
    if (!move.destination())
        return nullptr;
    return input::locationContextFromPosition(*move.destination(), -1);
}

/// The target of a walk as CipSoft's movement intent.
input::TMovementIntentTargetCoordinate movementIntentOf(const input::TGameActionPlayerMovementIntent& walk)
{
    return input::TMovementIntentTargetCoordinate(walk.target().x, walk.target().y, walk.target().z,
                                                  walk.enterUnwalkableGoal());
}

template<typename T>
bool sameCreature(const input::IGameActionPtr& expected, const input::IGameActionPtr& actual)
{
    const auto* left = gameActionAs<T>(expected);
    const auto* right = gameActionAs<T>(actual);
    return left && right && left->gameActionType() == right->gameActionType() && left->creatureId() == right->creatureId();
}

bool sameLocationAndObject(const input::IGameActionPtr& expected, const input::IGameActionPtr& actual)
{
    const auto* left = gameActionAs<input::TObjectGameAction>(expected);
    const auto* right = gameActionAs<input::TObjectGameAction>(actual);
    return left && right && contextsEqual(left->location(), right->location())
        && contextsEqual(left->object(), right->object());
}

}

bool gameActionsMatch(const input::IGameActionPtr& expected, const input::IGameActionPtr& actual)
{
    if (!expected || !actual)
        return false;
    if (expected->gameActionType() != actual->gameActionType())
        return false;
    switch (expected->gameActionType()) {
    case EGameAction::SET_PLAYER_MOVEMENT_INTENT: {
        const auto* left = gameActionAs<input::TGameActionPlayerMovementIntent>(expected);
        const auto* right = gameActionAs<input::TGameActionPlayerMovementIntent>(actual);
        return left && right && movementIntentOf(*left) == movementIntentOf(*right);
    }
    case EGameAction::MOVE_OBJECT: {
        const auto* left = gameActionAs<input::TGameActionMoveObject>(expected);
        const auto* right = gameActionAs<input::TGameActionMoveObject>(actual);
        if (!left || !right)
            return false;
        if (!contextsMatch(left->location(), right->location()))
            return false;
        if (!contextsMatch(destinationOf(*left), destinationOf(*right)))
            return false;
        if (!sameMovedObjectType(left->object(), right->object()))
            return false;
        return left->moveAll() == right->moveAll();
    }
    case EGameAction::ATTACK_CREATURE:
        return sameCreature<input::TGameActionCreature>(expected, actual);
    case EGameAction::OPEN_IN_NEW_WINDOW:
    case EGameAction::USE:
        return sameLocationAndObject(expected, actual);
    case EGameAction::START_TALKING_TO_NPC:
        return sameCreature<input::TGameActionStartTalkingToNPC>(expected, actual);
    case EGameAction::SEND_CHAT_MESSAGE_TO_NPC: {
        const auto* left = gameActionAs<input::TGameActionSendChatMessage>(expected);
        const auto* right = gameActionAs<input::TGameActionSendChatMessage>(actual);
        return left && right && left->text().toLower() == right->text().toLower();
    }
    case EGameAction::USE_WITH: {
        const auto* left = gameActionAs<input::TGameActionUseWith>(expected);
        const auto* right = gameActionAs<input::TGameActionUseWith>(actual);
        if (!left || !right || left->hasTarget() != right->hasTarget())
            return false;
        if (!contextsMatch(left->location(), right->location()))
            return false;
        const input::TGameActionContextPtr leftTargetLocation = left->target() ? left->target()->location() : nullptr;
        const input::TGameActionContextPtr rightTargetLocation = right->target() ? right->target()->location() : nullptr;
        if (!contextsMatch(leftTargetLocation, rightTargetLocation))
            return false;
        const auto* leftObject = dynamic_cast<const input::TObjectApperanceTypeGameActionContext*>(left->object().get());
        const auto* rightObject = dynamic_cast<const input::TObjectApperanceTypeGameActionContext*>(right->object().get());
        if (!leftObject || !rightObject || !input::objectApperanceTypeContextsEqual(*leftObject, *rightObject))
            return false;
        const input::TGameActionContextPtr leftTargetObject = left->target() ? left->target()->object() : nullptr;
        const input::TGameActionContextPtr rightTargetObject = right->target() ? right->target()->object() : nullptr;
        return contextsEqual(leftTargetObject, rightTargetObject);
    }
    case EGameAction::QUICK_LOOT: {
        const auto* left = gameActionAs<input::TGameActionQuickLoot>(expected);
        const auto* right = gameActionAs<input::TGameActionQuickLoot>(actual);
        return left && right && contextsMatch(left->location(), right->location())
            && contextsMatch(left->object(), right->object());
    }
    case EGameAction::ACTION_BUTTON_TRIGGERED: {
        const auto* left = gameActionAs<input::TGameActionActionButtonTriggered>(expected);
        const auto* right = gameActionAs<input::TGameActionActionButtonTriggered>(actual);
        return left && right && left->actionBarId() == right->actionBarId()
            && left->actionButtonId() == right->actionButtonId();
    }
    case EGameAction::CONFIRM_TUTORIAL_HINT: {
        const auto* left = gameActionAs<input::TGameActionTutorialHintConfirmed>(expected);
        const auto* right = gameActionAs<input::TGameActionTutorialHintConfirmed>(actual);
        return left && right && left->tutorialHintId() == right->tutorialHintId();
    }
    default:
        return true;
    }
}

std::shared_ptr<const input::TGameActionAddGUITutorialMarker> TSessiondumpHintCondition::createTutorialMarker(
    input::TGameActionContextPtr location, input::TGameActionContextPtr object)
{
    const int markerId = qmlcomponents::TTutorialMarkerRegistry::instance().nextUniqueMarkerID();
    return std::make_shared<input::TGameActionAddGUITutorialMarker>(std::move(location), std::move(object), markerId);
}

input::TGameActionContextPtr TSessiondumpHintCondition::locationFromJsonObject(const QJsonObject& object)
{
    using V = shared::TJsonValueValidator;
    const bool valid = has(object, "x", &V::isInt) && has(object, "y", &V::isInt) && has(object, "z", &V::isInt);
    const bool hasStackPosition = has(object, "stackposition", &V::isInt);
    if (!valid)
        throw std::invalid_argument(InvalidHint);
    const int x = intAt(object, "x");
    const int y = intAt(object, "y");
    const int z = intAt(object, "z");
    if (x == 0xffff) {
        if (y > 0x3f)
            return std::make_shared<input::TLocationContainerWithSlotGameActionContext>(uint8_t(y - 0x40), z);
        return std::make_shared<input::TLocationInventoryGameActionContext>(y);
    }
    const Position coordinate(x, y, uint8_t(z));
    if (hasStackPosition)
        return std::make_shared<input::TLocationWorldMapCoordinateWithStackPositionContext>(
            coordinate, uint8_t(intAt(object, "stackposition")));
    return std::make_shared<input::TLocationWorldMapCoordinateContext>(coordinate);
}

input::TGameActionAddHintAtTutorialMarker::THint TSessiondumpHintCondition::textHintFromJsonObject(const QJsonObject& object)
{
    using V = shared::TJsonValueValidator;
    require(object, "text", &V::isString);
    input::TGameActionAddHintAtTutorialMarker::THint hint;
    hint.text = qtTrId(object[QStringLiteral("text")].toString().toStdString().c_str());
    hint.border = has(object, "border", &V::isBool) ? object[QStringLiteral("border")].toBool() : true;
    hint.textAnchor = has(object, "textanchor", &V::isString) ? object[QStringLiteral("textanchor")].toString()
                                                              : QStringLiteral("topLeft");
    return hint;
}

void TSessiondumpHintCondition::addTextHintsFromJsonObject(const QJsonObject& data, const int markerId,
                                                           const int destinationMarkerId)
{
    using V = shared::TJsonValueValidator;
    if (markerId != 0 && has(data, "texthint", &V::isObject)) {
        input::TGameActionAddHintAtTutorialMarker::THint hint = textHintFromJsonObject(objectAt(data, "texthint"));
        hint.markerId = markerId;
        addGameAction(std::make_shared<input::TGameActionAddHintAtTutorialMarker>(std::move(hint)));
    }
    if (destinationMarkerId != 0 && has(data, "destinationtexthint", &V::isObject)) {
        input::TGameActionAddHintAtTutorialMarker::THint hint = textHintFromJsonObject(objectAt(data, "destinationtexthint"));
        hint.markerId = destinationMarkerId;
        addGameAction(std::make_shared<input::TGameActionAddHintAtTutorialMarker>(std::move(hint)));
    }
}

TConditionPtr TSessiondumpHintCondition::createAutowalkHintFromJsonObject(const QJsonObject& data)
{
    const input::TGameActionContextPtr location = locationFromJsonObject(objectAt(data, "coordinate"));
    const auto* field = dynamic_cast<const input::TLocationWorldMapCoordinateContext*>(location.get());
    if (!field || location->gameActionContextType() != input::EGameActionContextType::LOCATION_WORLD_MAP_COORDINATE)
        throw std::invalid_argument(InvalidHint);
    // Like a click on the map: the walk may end on an unwalkable goal.
    auto condition = std::make_shared<TSessiondumpHintCondition>(
        std::make_shared<input::TGameActionPlayerMovementIntent>(field->coordinate(), 0, true));
    const auto marker = createTutorialMarker(location);
    condition->addGameAction(marker);
    condition->addTextHintsFromJsonObject(data, marker->markerId(), 0);
    return condition;
}

TConditionPtr TSessiondumpHintCondition::createMoveHintFromJsonObject(const QJsonObject& data)
{
    using V = shared::TJsonValueValidator;
    if (!has(data, "objecttypeid", &V::isInt) || !has(data, "amount", &V::isInt))
        throw std::invalid_argument(InvalidHint);
    const int objectTypeId = intAt(data, "objecttypeid");
    const int amount = intAt(data, "amount");
    QJsonObject destinationObject = objectAt(data, "destination");
    input::TGameActionContextPtr destination = locationFromJsonObject(destinationObject);
    if (dynamic_cast<const input::TLocationContainerWithSlotGameActionContext*>(destination.get())) {
        // Into a container: any slot of it.
        destinationObject[QStringLiteral("z")] = QJsonValue(0xff);
        destination = locationFromJsonObject(destinationObject);
    }
    const input::TGameActionContextPtr source = locationFromJsonObject(objectAt(data, "source"));
    const input::TGameActionContextPtr object
        = std::make_shared<input::TObjectApperanceTypeGameActionContext>(uint16_t(objectTypeId));
    // qtclient keeps a move's destination as a classic position.
    auto move = std::make_shared<input::TGameActionMoveObject>(input::TObjectGameActionContext(source, object),
                                                               input::protocolPosition(destination.get()), false, amount);
    auto condition = std::make_shared<TSessiondumpHintCondition>(move);
    const auto sourceMarker = createTutorialMarker(source);
    condition->addGameAction(sourceMarker);
    const auto destinationMarker = createTutorialMarker(destination);
    condition->addGameAction(destinationMarker);
    condition->addGameAction(std::make_shared<input::TGameActionAddMouseTrailHintAtTutorialMarker>(
        1, sourceMarker->markerId(), destinationMarker->markerId()));
    condition->addTextHintsFromJsonObject(data, sourceMarker->markerId(), destinationMarker->markerId());
    return condition;
}

TConditionPtr TSessiondumpHintCondition::createAttackOrGreetHintFromJsonObject(const QJsonObject& data,
                                                                               const input::EGameAction type)
{
    using V = shared::TJsonValueValidator;
    require(data, "creatureid", &V::isInt);
    const uint32_t creatureId = uint32_t(intAt(data, "creatureid"));
    const input::TGameActionContextPtr creature = std::make_shared<input::TObjectCreatureGameActionContext>(creatureId);
    input::IGameActionPtr expected;
    if (type == EGameAction::START_TALKING_TO_NPC)
        expected = std::make_shared<input::TGameActionStartTalkingToNPC>(creatureId);
    else if (type == EGameAction::ATTACK_CREATURE)
        expected = std::make_shared<input::TGameActionCreature>(EGameAction::ATTACK_CREATURE, creatureId);
    else
        throw std::invalid_argument(
            "TSessiondumpHintCondition::createAttackOrGreetHintFromJsonObject: Invalid game action type");
    auto condition = std::make_shared<TSessiondumpHintCondition>(expected);
    // The creature on the map and in the battle list.
    const auto worldMapMarker = createTutorialMarker(std::make_shared<input::TLocationWorldMapGameActionContext>(), creature);
    condition->addGameAction(worldMapMarker);
    const auto battleListMarker = createTutorialMarker(std::make_shared<input::TLocationBattleListGameActionContext>(), creature);
    condition->addGameAction(battleListMarker);
    input::TGameActionAddHintAtTutorialMarker::THint battleListHint;
    battleListHint.markerId = battleListMarker->markerId();
    battleListHint.text = QString();
    battleListHint.border = true;
    battleListHint.textAnchor = QStringLiteral("topLeft");
    condition->addGameAction(std::make_shared<input::TGameActionAddHintAtTutorialMarker>(std::move(battleListHint)));
    condition->addTextHintsFromJsonObject(data, worldMapMarker->markerId(), battleListMarker->markerId());
    return condition;
}

TConditionPtr TSessiondumpHintCondition::createUseHintFromJsonObject(const QJsonObject& data, const input::EGameAction type)
{
    using V = shared::TJsonValueValidator;
    require(data, "usetypeid", &V::isInt);
    const int useTypeId = intAt(data, "usetypeid");
    const input::TGameActionContextPtr location = locationFromJsonObject(objectAt(data, "coordinate"));
    const input::TGameActionContextPtr object = std::make_shared<input::TObjectApperanceTypeGameActionContext>(uint16_t(useTypeId));
    const input::TObjectGameActionContext context(location, object);
    input::IGameActionPtr expected;
    if (type == EGameAction::OPEN_IN_NEW_WINDOW)
        expected = std::make_shared<input::TGameActionOpenInNewWindow>(context);
    else if (type == EGameAction::USE)
        expected = std::make_shared<input::TGameActionUse>(context);
    else
        throw std::invalid_argument("TSessiondumpHintCondition::createUseHintFromJsonObject: Invalid game action type");
    auto condition = std::make_shared<TSessiondumpHintCondition>(expected);
    const auto marker = createTutorialMarker(location);
    condition->addGameAction(marker);
    condition->addTextHintsFromJsonObject(data, marker->markerId(), 0);
    return condition;
}

TConditionPtr TSessiondumpHintCondition::createTalkHintFromJsonObject(const QJsonObject& data)
{
    using V = shared::TJsonValueValidator;
    require(data, "text", &V::isString);
    const QString text = data[QStringLiteral("text")].toString();
    auto condition = std::make_shared<TSessiondumpHintCondition>(
        std::make_shared<input::TGameActionSendChatMessageToNpc>(text, QString(), uint16_t(0)));
    // The keyword in the NPC's chat.
    const auto marker = createTutorialMarker(std::make_shared<input::TLocationNPCChatKeywordGameActionContext>(text));
    condition->addGameAction(marker);
    condition->addTextHintsFromJsonObject(data, marker->markerId(), 0);
    return condition;
}

TConditionPtr TSessiondumpHintCondition::createUseWithHintFromJsonObject(const QJsonObject& data)
{
    using V = shared::TJsonValueValidator;
    if (!has(data, "objecttypeid", &V::isInt) || !has(data, "targettypeid", &V::isInt))
        throw std::invalid_argument(InvalidHint);
    const int objectTypeId = intAt(data, "objecttypeid");
    const int targetTypeId = intAt(data, "targettypeid");
    const input::TGameActionContextPtr targetLocation = locationFromJsonObject(objectAt(data, "multiusetarget"));
    const input::TGameActionContextPtr location = locationFromJsonObject(objectAt(data, "coordinate"));
    const input::TGameActionContextPtr object = std::make_shared<input::TObjectApperanceTypeGameActionContext>(uint16_t(objectTypeId));
    const input::TGameActionContextPtr target = std::make_shared<input::TObjectApperanceTypeGameActionContext>(uint16_t(targetTypeId));
    auto condition = std::make_shared<TSessiondumpHintCondition>(std::make_shared<input::TGameActionUseWith>(
        input::TObjectGameActionContext(location, object), input::TObjectGameActionContext(targetLocation, target)));
    const auto marker = createTutorialMarker(location);
    condition->addGameAction(marker);
    const auto targetMarker = createTutorialMarker(targetLocation);
    condition->addGameAction(targetMarker);
    condition->addTextHintsFromJsonObject(data, marker->markerId(), 0);
    condition->addGameAction(std::make_shared<input::TGameActionAddMouseTrailHintAtTutorialMarker>(
        2, marker->markerId(), targetMarker->markerId()));
    return condition;
}

TConditionPtr TSessiondumpHintCondition::createQuickLootHintFromJsonObject(const QJsonObject& data)
{
    using V = shared::TJsonValueValidator;
    require(data, "usetypeid", &V::isInt);
    const int useTypeId = intAt(data, "usetypeid");
    const input::TGameActionContextPtr location = locationFromJsonObject(objectAt(data, "coordinate"));
    const input::TGameActionContextPtr object = std::make_shared<input::TObjectApperanceTypeGameActionContext>(uint16_t(useTypeId));
    auto condition = std::make_shared<TSessiondumpHintCondition>(
        std::make_shared<input::TGameActionQuickLoot>(input::TObjectGameActionContext(location, object)));
    const auto marker = createTutorialMarker(location);
    condition->addGameAction(marker);
    condition->addTextHintsFromJsonObject(data, marker->markerId(), 0);
    return condition;
}

TConditionPtr TSessiondumpHintCondition::createActionButtonTriggeredHintFromJsonObject(const QJsonObject& data)
{
    using V = shared::TJsonValueValidator;
    if (!has(data, "actionbarid", &V::isDouble) || !has(data, "actionbuttonid", &V::isDouble))
        throw std::invalid_argument(InvalidHint);
    const uint8_t actionBarId = uint8_t(intAt(data, "actionbarid"));
    const int actionButtonId = intAt(data, "actionbuttonid");
    auto condition = std::make_shared<TSessiondumpHintCondition>(
        std::make_shared<input::TGameActionActionButtonTriggered>(actionBarId, actionButtonId, false));
    const auto marker = createTutorialMarker(
        std::make_shared<input::TLocationActionButtonGameActionContext>(actionBarId, actionButtonId, -1));
    condition->addGameAction(marker);
    condition->addTextHintsFromJsonObject(data, marker->markerId(), 0);
    return condition;
}

TConditionPtr TSessiondumpHintCondition::createShowTutorialHintFromJsonObject(const QJsonObject& data)
{
    using V = shared::TJsonValueValidator;
    require(data, "showtutorialhintid", &V::isInt);
    const int tutorialHintId = intAt(data, "showtutorialhintid");
    if (unsigned(tutorialHintId - 1) >= 14u)
        return nullptr;
    auto condition = std::make_shared<TSessiondumpHintConditionShowTutorialHint>(tutorialHintId);
    // The hint dialog.
    condition->addGameAction(createTutorialMarker(std::make_shared<input::TLocationTutorialHintDialogGameActionContext>()));
    return condition;
}

}
