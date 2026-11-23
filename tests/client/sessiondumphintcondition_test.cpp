#include <gtest/gtest.h>

#include "input/gameaction.h"
#include "input/gameactioncontext.h"
#include "qmlcomponents/tutorialhelper.h"
#include "tutorial/sessiondumphintcondition.h"
#include "tutorial/sessiondumphintconditionshowtutorialhint.h"
#include "tutorial/sessiondumphints.h"
#include "tutorial/sessiondumphintsprocessor.h"

#include <QJsonDocument>
#include <QJsonObject>

#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

using namespace tibia::tutorial;
namespace input = tibia::input;
using input::EGameAction;
using input::gameActionAs;

// The test builds the hints without qmlcomponents/tutorialhelper.cpp; these are the
// marker registry's id counter.
namespace tibia::qmlcomponents {
TTutorialMarkerRegistry& TTutorialMarkerRegistry::instance()
{
    static TTutorialMarkerRegistry registry;
    return registry;
}
int TTutorialMarkerRegistry::nextUniqueMarkerID() { return s_nextMarkerID++; }
}

namespace {

QJsonObject object(const char* json) { return QJsonDocument::fromJson(json).object(); }

input::TObjectGameActionContext context(input::TGameActionContextPtr location, uint16_t typeId)
{
    return input::TObjectGameActionContext(std::move(location),
                                           std::make_shared<input::TObjectApperanceTypeGameActionContext>(typeId));
}

input::TGameActionContextPtr field(int x, int y, int z)
{
    return std::make_shared<input::TLocationWorldMapCoordinateContext>(Position(x, y, uint8_t(z)));
}

input::TGameActionContextPtr fieldAt(int x, int y, int z, int stackPosition)
{
    return std::make_shared<input::TLocationWorldMapCoordinateWithStackPositionContext>(Position(x, y, uint8_t(z)),
                                                                                         uint8_t(stackPosition));
}

std::vector<EGameAction> typesOf(const input::TGameActionList& actions)
{
    std::vector<EGameAction> types;
    for (const auto& action : actions)
        types.push_back(action->gameActionType());
    return types;
}

}

TEST(SessiondumpHintCondition, PublishesItsActionsThenWaitsForTheExpectedOne)
{
    const auto use = std::make_shared<input::TGameActionUse>(context(fieldAt(100, 200, 7, 1), 2480));
    TSessiondumpHintCondition condition(use);
    EXPECT_EQ(condition.type(), TSessiondumpHintBase::Condition);
    condition.addGameAction(std::make_shared<input::TGameActionClearOldestMessageFromMapwindow>());

    // Not waiting yet: an action does not count.
    condition.handleGameAction(use);
    EXPECT_EQ(condition.state(), TSessiondumpHintCondition::NotStarted);

    const auto first = condition.execute();
    ASSERT_EQ(first.size(), 1u);
    EXPECT_EQ(first[0]->gameActionType(), EGameAction::CLEAR_OLDEST_MESSAGE_FROM_MAPWINDOW);
    EXPECT_EQ(condition.state(), TSessiondumpHintCondition::WaitingForGameAction);
    EXPECT_TRUE(condition.execute().empty());
    EXPECT_FALSE(condition.isDone());

    // Another object type, then the same use on the field without a stack position (USE needs equal contexts).
    condition.handleGameAction(std::make_shared<input::TGameActionUse>(context(fieldAt(100, 200, 7, 1), 2481)));
    condition.handleGameAction(std::make_shared<input::TGameActionUse>(context(field(100, 200, 7), 2480)));
    EXPECT_EQ(condition.state(), TSessiondumpHintCondition::WaitingForGameAction);
    condition.handleGameAction(std::make_shared<input::TGameActionUse>(context(fieldAt(100, 200, 7, 1), 2480)));
    EXPECT_EQ(condition.state(), TSessiondumpHintCondition::GameActionReceived);

    const auto last = condition.execute();
    ASSERT_EQ(last.size(), 1u);
    EXPECT_EQ(last[0]->gameActionType(), EGameAction::REMOVE_ALL_TUTORIAL_MARKERS);
    EXPECT_TRUE(condition.isDone());
}

TEST(SessiondumpHintCondition, MatchesTheFieldsOfEachActionType)
{
    // Quick loot matches a field with and without a stack position.
    const auto loot = std::make_shared<input::TGameActionQuickLoot>(context(fieldAt(1, 2, 7, 3), 10));
    EXPECT_TRUE(gameActionsMatch(loot, std::make_shared<input::TGameActionQuickLoot>(context(field(1, 2, 7), 10))));
    EXPECT_FALSE(gameActionsMatch(loot, std::make_shared<input::TGameActionQuickLoot>(context(field(1, 3, 7), 10))));

    const auto attack = std::make_shared<input::TGameActionCreature>(EGameAction::ATTACK_CREATURE, 42);
    EXPECT_TRUE(gameActionsMatch(attack, std::make_shared<input::TGameActionCreature>(EGameAction::ATTACK_CREATURE, 42)));
    EXPECT_FALSE(gameActionsMatch(attack, std::make_shared<input::TGameActionCreature>(EGameAction::ATTACK_CREATURE, 43)));

    const auto talk = std::make_shared<input::TGameActionSendChatMessageToNpc>(QStringLiteral("yes"), QString(), 0);
    EXPECT_TRUE(gameActionsMatch(talk, std::make_shared<input::TGameActionSendChatMessageToNpc>(QStringLiteral("YeS"),
                                                                                               QStringLiteral("Npc"), 5)));
    EXPECT_FALSE(gameActionsMatch(talk, std::make_shared<input::TGameActionSendChatMessageToNpc>(QStringLiteral("no"),
                                                                                                QString(), 0)));

    const auto button = std::make_shared<input::TGameActionActionButtonTriggered>(uint8_t(1), 3);
    EXPECT_TRUE(gameActionsMatch(button, std::make_shared<input::TGameActionActionButtonTriggered>(uint8_t(1), 3, true)));
    EXPECT_FALSE(gameActionsMatch(button, std::make_shared<input::TGameActionActionButtonTriggered>(uint8_t(2), 3)));

    const auto walk = std::make_shared<input::TGameActionPlayerMovementIntent>(Position(5, 6, 7), 0, true);
    EXPECT_TRUE(gameActionsMatch(walk, std::make_shared<input::TGameActionPlayerMovementIntent>(Position(5, 6, 7), 2, true)));
    EXPECT_FALSE(gameActionsMatch(walk, std::make_shared<input::TGameActionPlayerMovementIntent>(Position(5, 6, 7), 0, false)));

    // A move compares the moved object's type only, not its kind.
    const auto source = std::make_shared<input::TLocationContainerWithSlotGameActionContext>(uint8_t(0), 1);
    const auto move = std::make_shared<input::TGameActionMoveObject>(context(source, 3267), Position(0xffff, 6, 0), false, 1);
    const auto movedAll = std::make_shared<input::TGameActionMoveObject>(
        input::TObjectGameActionContext(source, std::make_shared<input::TUpgradeableObjectApperanceTypeGameActionContext>(3267, 2)),
        Position(0xffff, 6, 0), false, 5);
    EXPECT_TRUE(gameActionsMatch(move, movedAll));
    EXPECT_FALSE(gameActionsMatch(move, std::make_shared<input::TGameActionMoveObject>(context(source, 3267), Position(0xffff, 5, 0))));
    EXPECT_FALSE(gameActionsMatch(move, std::make_shared<input::TGameActionMoveObject>(context(source, 3267), Position(0xffff, 6, 0), true)));

    // Different types never match; any other type matches on the type alone.
    EXPECT_FALSE(gameActionsMatch(attack, loot));
    EXPECT_FALSE(gameActionsMatch(attack, nullptr));
    EXPECT_TRUE(gameActionsMatch(std::make_shared<input::TGameActionRemoveAllTutorialMarkers>(),
                                 std::make_shared<input::TGameActionRemoveAllTutorialMarkers>()));
}

TEST(SessiondumpHintCondition, ReadsLocations)
{
    using C = TSessiondumpHintCondition;
    const auto container = C::locationFromJsonObject(object(R"({"x": 65535, "y": 65, "z": 3})"));
    const auto* slot = dynamic_cast<const input::TLocationContainerWithSlotGameActionContext*>(container.get());
    ASSERT_TRUE(slot);
    EXPECT_EQ(slot->containerId(), 1);
    EXPECT_EQ(slot->slot(), 3);
    const auto inventory = C::locationFromJsonObject(object(R"({"x": 65535, "y": 10, "z": 0, "stackposition": 10})"));
    ASSERT_EQ(inventory->gameActionContextType(), input::EGameActionContextType::LOCATION_INVENTORY);
    EXPECT_EQ(static_cast<const input::TLocationInventoryGameActionContext&>(*inventory).slot(), 10);
    const auto stacked = C::locationFromJsonObject(object(R"({"x": 32518, "y": 32510, "z": 6, "stackposition": 1})"));
    ASSERT_EQ(stacked->gameActionContextType(), input::EGameActionContextType::LOCATION_WORLD_MAP_COORDINATE_WITH_STACK_POSITION);
    EXPECT_EQ(static_cast<const input::TLocationWorldMapCoordinateWithStackPositionContext&>(*stacked).stackPosition(), 1);
    EXPECT_EQ(C::locationFromJsonObject(object(R"({"x": 1, "y": 2, "z": 7})"))->gameActionContextType(),
              input::EGameActionContextType::LOCATION_WORLD_MAP_COORDINATE);
    EXPECT_THROW(C::locationFromJsonObject(object(R"({"x": 1, "y": 2})")), std::invalid_argument);
}

TEST(SessiondumpHintCondition, BuildsAMoveHint)
{
    const auto condition = TSessiondumpHintCondition::createMoveHintFromJsonObject(object(R"({
        "amount": 1, "objecttypeid": 3267, "destination": {"x": 65535, "y": 64, "z": 4}, "source": {"x": 65535, "y": 6, "z": 0},
        "texthint": {"text": "source_hint"},
        "destinationtexthint": {"text": "destination_hint", "textanchor": "topCenter", "border": false}})"));
    const auto* move = gameActionAs<input::TGameActionMoveObject>(condition->expectedGameAction());
    ASSERT_TRUE(move);
    // Into the container: any slot of it.
    EXPECT_EQ(move->destination(), Position(0xffff, 0x40, 0xff));
    EXPECT_EQ(move->amount(), 1);
    EXPECT_EQ(move->objectClientId(), 3267);

    const auto actions = condition->execute();
    EXPECT_EQ(typesOf(actions),
              (std::vector<EGameAction>{ EGameAction::ADD_GUI_TUTORIAL_MARKER, EGameAction::ADD_GUI_TUTORIAL_MARKER,
                                         EGameAction::ADD_MOUSE_TRAIL_HINT_AT_TUTORIAL_MARKER,
                                         EGameAction::ADD_HINT_AT_TUTORIAL_MARKER, EGameAction::ADD_HINT_AT_TUTORIAL_MARKER }));
    const auto* sourceMarker = gameActionAs<input::TGameActionAddGUITutorialMarker>(actions[0]);
    const auto* destinationMarker = gameActionAs<input::TGameActionAddGUITutorialMarker>(actions[1]);
    ASSERT_TRUE(sourceMarker && destinationMarker);
    EXPECT_NE(sourceMarker->markerId(), destinationMarker->markerId());
    EXPECT_EQ(sourceMarker->location()->gameActionContextType(), input::EGameActionContextType::LOCATION_INVENTORY);
    EXPECT_FALSE(sourceMarker->object());
    const auto& sourceHint = gameActionAs<input::TGameActionAddHintAtTutorialMarker>(actions[3])->hint();
    EXPECT_EQ(sourceHint.markerId, sourceMarker->markerId());
    EXPECT_EQ(sourceHint.text, QStringLiteral("source_hint"));
    EXPECT_EQ(sourceHint.textAnchor, QStringLiteral("topLeft"));
    EXPECT_TRUE(sourceHint.border);
    const auto& destinationHint = gameActionAs<input::TGameActionAddHintAtTutorialMarker>(actions[4])->hint();
    EXPECT_EQ(destinationHint.markerId, destinationMarker->markerId());
    EXPECT_EQ(destinationHint.textAnchor, QStringLiteral("topCenter"));
    EXPECT_FALSE(destinationHint.border);

    EXPECT_THROW(TSessiondumpHintCondition::createMoveHintFromJsonObject(object(R"({"objecttypeid": 1})")),
                 std::invalid_argument);
}

TEST(SessiondumpHintCondition, BuildsAnAttackHintWithAMarkerOnTheMapAndInTheBattleList)
{
    const auto condition = TSessiondumpHintCondition::createAttackOrGreetHintFromJsonObject(
        object(R"({"creatureid": 77, "texthint": {"text": "attack_hint"}})"), EGameAction::ATTACK_CREATURE);
    const auto* attack = gameActionAs<input::TGameActionCreature>(condition->expectedGameAction());
    ASSERT_TRUE(attack);
    EXPECT_EQ(attack->creatureId(), 77u);
    const auto actions = condition->execute();
    ASSERT_EQ(actions.size(), 4u);
    const auto* worldMap = gameActionAs<input::TGameActionAddGUITutorialMarker>(actions[0]);
    const auto* battleList = gameActionAs<input::TGameActionAddGUITutorialMarker>(actions[1]);
    ASSERT_TRUE(worldMap && battleList);
    EXPECT_EQ(worldMap->location()->gameActionContextType(), input::EGameActionContextType::LOCATION_WORLD_MAP);
    EXPECT_EQ(battleList->location()->gameActionContextType(), input::EGameActionContextType::LOCATION_BATTLE_LIST);
    EXPECT_EQ(input::creatureIdOf(battleList->object().get()), 77u);
    // An empty hint at the battle list marker, then the text hint on the map.
    EXPECT_EQ(gameActionAs<input::TGameActionAddHintAtTutorialMarker>(actions[2])->hint().markerId, battleList->markerId());
    EXPECT_TRUE(gameActionAs<input::TGameActionAddHintAtTutorialMarker>(actions[2])->hint().text.isEmpty());
    EXPECT_EQ(gameActionAs<input::TGameActionAddHintAtTutorialMarker>(actions[3])->hint().markerId, worldMap->markerId());

    EXPECT_THROW(TSessiondumpHintCondition::createAttackOrGreetHintFromJsonObject(object(R"({"creatureid": 1})"),
                                                                                  EGameAction::USE),
                 std::invalid_argument);
}

TEST(SessiondumpHintCondition, ShowsATutorialHintUntilItIsConfirmed)
{
    EXPECT_FALSE(TSessiondumpHintCondition::createShowTutorialHintFromJsonObject(object(R"({"showtutorialhintid": 15})")));
    const auto condition = TSessiondumpHintCondition::createShowTutorialHintFromJsonObject(object(R"({"showtutorialhintid": 14})"));
    ASSERT_TRUE(condition);
    const auto actions = condition->execute();
    ASSERT_EQ(actions.size(), 2u);
    ASSERT_TRUE(gameActionAs<input::TGameActionShowTutorialHint>(actions[0]));
    EXPECT_EQ(gameActionAs<input::TGameActionShowTutorialHint>(actions[0])->tutorialHintId(), 14);
    EXPECT_EQ(actions[1]->gameActionType(), EGameAction::ADD_GUI_TUTORIAL_MARKER);
    condition->handleGameAction(std::make_shared<input::TGameActionTutorialHintConfirmed>(13));
    EXPECT_TRUE(condition->execute().empty());
    condition->handleGameAction(std::make_shared<input::TGameActionTutorialHintConfirmed>(14));
    EXPECT_EQ(typesOf(condition->execute()), (std::vector<EGameAction>{ EGameAction::REMOVE_ALL_TUTORIAL_MARKERS }));
    EXPECT_TRUE(condition->isDone());
}

TEST(SessiondumpHintsProcessor, HoldsAtAConditionUntilThePlayerDoesIt)
{
    const QByteArray json = R"({"outfit": {"male": {"id": 1, "head": 0, "torso": 0, "legs": 0, "detail": 0, "addons": 0},
                                           "female": {"id": 2, "head": 0, "torso": 0, "legs": 0, "detail": 0, "addons": 0}},
        "actions": {
            "10": [{"type": "TUTORIAL_PROGRESS", "tutorialprogress": "Start"}],
            "20": [{"type": "CONDITION", "conditiontype": "ACTION_BUTTON_TRIGGERED", "conditiondata": {"actionbarid": 1, "actionbuttonid": 3}}],
            "30": [{"type": "CLEAR_OLDEST_MESSAGE"}],
            "90": [{"type": "CLEAR_OLDEST_MESSAGE"}]}})";
    auto hints = std::make_shared<TSessiondumpHints>(QJsonDocument::fromJson(json));
    TSessiondumpHintsProcessor processor(hints);
    std::vector<EGameAction> published;
    QObject::connect(&processor, &TSessiondumpHintsProcessor::publishGameAction,
                     [&published](const input::IGameActionPtr& action) { published.push_back(action->gameActionType()); });

    EXPECT_EQ(processor.nextHintTimestamp(), 10);
    EXPECT_FALSE(processor.hasHintToProcess(9));
    processor.processHints(50);
    // The progress, then the condition's marker; the condition waits.
    EXPECT_EQ(published, (std::vector<EGameAction>{ EGameAction::TUTORIAL_PROGRESS, EGameAction::ADD_GUI_TUTORIAL_MARKER }));
    EXPECT_EQ(processor.nextHintTimestamp(), 20);
    EXPECT_TRUE(processor.hasHintToProcess(50));

    published.clear();
    processor.handleGameAction(std::make_shared<input::TGameActionActionButtonTriggered>(uint8_t(1), 3));
    processor.processHints(50);
    EXPECT_EQ(published, (std::vector<EGameAction>{ EGameAction::REMOVE_ALL_TUTORIAL_MARKERS,
                                                    EGameAction::CLEAR_OLDEST_MESSAGE_FROM_MAPWINDOW }));
    EXPECT_EQ(processor.nextHintTimestamp(), 90);

    // A skip drops the hints before its target.
    processor.handleGameAction(std::make_shared<input::TGameActionSessiondumpSkipTo>(100));
    EXPECT_EQ(processor.nextHintTimestamp(), std::numeric_limits<int64_t>::max());
}
