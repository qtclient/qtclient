#pragma once

#include "tutorial/sessiondumphintbase.h"

#include <QJsonObject>

#include <memory>

namespace tibia {
namespace tutorial {

/// CIP `tutorial::TSessiondumpHintCondition`: holds the tutorial replay at its
/// timestamp until the player does the action the hint shows. Each run
/// (execute) moves the hint on:
/// - the first run publishes its game actions (the GUI tutorial markers and
///   text hints) and starts waiting;
/// - while it waits, a run publishes nothing;
/// - once the player did the expected action (handleGameAction), the next run
///   removes every tutorial marker and the hint is done.
class TSessiondumpHintCondition : public TSessiondumpHintBase
{
public:
    enum EState
    {
        NotStarted = 0,
        WaitingForGameAction = 1,
        GameActionReceived = 2,
    };

    explicit TSessiondumpHintCondition(input::IGameActionPtr expectedGameAction);
    ~TSessiondumpHintCondition() override;

    input::TGameActionList execute() override;
    /// While the hint waits: the player's action, which fulfils the hint when it
    /// matches the expected one (gameActionsMatch).
    virtual void handleGameAction(const input::IGameActionPtr& gameAction);
    virtual input::IGameActionPtr expectedGameAction() const;
    /// The actions the first run publishes.
    virtual input::TGameActionList gameActions() const;
    virtual void addGameAction(const input::IGameActionPtr& gameAction);

    EState state() const { return m_state; }

    // The condition hints of a hints file, from their "conditiondata" object.
    // Each throws std::invalid_argument when a field it needs is missing.

    /// AUTOWALK: walk to "coordinate" (a field without a stack position).
    static std::shared_ptr<TSessiondumpHintCondition> createAutowalkHintFromJsonObject(const QJsonObject& data);
    /// MOVE: move "amount" objects of "objecttypeid" from "source" to "destination"
    /// (a container destination means any slot of it).
    static std::shared_ptr<TSessiondumpHintCondition> createMoveHintFromJsonObject(const QJsonObject& data);
    /// ATTACK or GREET (`type`): attack or greet "creatureid".
    static std::shared_ptr<TSessiondumpHintCondition> createAttackOrGreetHintFromJsonObject(const QJsonObject& data,
                                                                                            input::EGameAction type);
    /// USE or OPEN_IN_NEW_WINDOW (`type`): use or open "usetypeid" at "coordinate".
    static std::shared_ptr<TSessiondumpHintCondition> createUseHintFromJsonObject(const QJsonObject& data,
                                                                                  input::EGameAction type);
    /// TALK: say "text" to an NPC.
    static std::shared_ptr<TSessiondumpHintCondition> createTalkHintFromJsonObject(const QJsonObject& data);
    /// USE_WITH: use "objecttypeid" at "coordinate" with "targettypeid" at "multiusetarget".
    static std::shared_ptr<TSessiondumpHintCondition> createUseWithHintFromJsonObject(const QJsonObject& data);
    /// QUICKLOOT: quick loot "usetypeid" at "coordinate".
    static std::shared_ptr<TSessiondumpHintCondition> createQuickLootHintFromJsonObject(const QJsonObject& data);
    /// ACTION_BUTTON_TRIGGERED: trigger "actionbuttonid" of action bar "actionbarid".
    static std::shared_ptr<TSessiondumpHintCondition> createActionButtonTriggeredHintFromJsonObject(const QJsonObject& data);
    /// SHOW_TUTORIAL_HINT: show and confirm "showtutorialhintid"; null for an id
    /// outside 1..14.
    static std::shared_ptr<TSessiondumpHintCondition> createShowTutorialHintFromJsonObject(const QJsonObject& data);

    /// A location in a hints file: "x", "y", "z" and an optional "stackposition".
    /// x 0xffff is an inventory slot (y), or a container slot (z) of container
    /// y - 0x40 when y is at least 0x40; anything else is a field.
    static input::TGameActionContextPtr locationFromJsonObject(const QJsonObject& object);

protected:
    /// A GUI tutorial marker at `location` (and `object` there), with the next
    /// unique marker id.
    static std::shared_ptr<const input::TGameActionAddGUITutorialMarker> createTutorialMarker(
        input::TGameActionContextPtr location, input::TGameActionContextPtr object = nullptr);
    /// Adds the "texthint" at marker `markerId` and the "destinationtexthint" at
    /// marker `destinationMarkerId`, where the data has them and the marker is not 0.
    void addTextHintsFromJsonObject(const QJsonObject& data, int markerId, int destinationMarkerId);
    /// A text hint: the translated "text", its "textanchor" (default topLeft) and
    /// whether it has a "border" (default true).
    static input::TGameActionAddHintAtTutorialMarker::THint textHintFromJsonObject(const QJsonObject& object);

    EState m_state = NotStarted;
    input::IGameActionPtr m_expectedGameAction;
    input::TGameActionList m_gameActions;
};

/// CIP: whether the player's action is the one a condition hint expects. Both
/// must be of the same type; for these types the fields must also match:
/// - a walk: the same target;
/// - a move: the same source, destination, object type and whether everything
///   moves;
/// - an attack or a greeting: the same creature;
/// - a use or an open in a new window: the same location and object;
/// - a text to an NPC: the same text, ignoring case;
/// - a use with: whether a target is set, then the locations and the objects;
/// - a quick loot: the same location and object;
/// - an action button: the same bar and button;
/// - a confirmed tutorial hint: the same hint.
/// Any other type matches on the type alone. A null action never matches.
bool gameActionsMatch(const input::IGameActionPtr& expected, const input::IGameActionPtr& actual);

}
}
