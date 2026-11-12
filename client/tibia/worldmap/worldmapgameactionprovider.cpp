#include "worldmapgameactionprovider.h"

#include "gamewindow/mousecursorcontroller.h"
#include "input/inputciptypes.h"
#include "input/genericobjectgameactionprovider.h"
#include "renderer/worldmaprenderprovider.h"
#include "worldmap/worldmappicker.h"
#include "worldmap/worldmapstorage.h"

#include "creatures/creaturestorage.h"
#include <tibia/assets/thingtype.h>
#include <tibia/game/gamesession.h>
#include <QCoreApplication>

namespace tibia::worldmap {

namespace {

using input::EGameAction;
using input::TGameActionList;
using input::TGameActionPtr;
using A = EGameAction;
using Provider = input::TGenericObjectGameActionProvider;
using Cursor = gamewindow::TMouseCursorController;

TGameActionList actionsOf(const TFieldObject& object, const TWorldMapField* field, const Position& position,
                          std::initializer_list<EGameAction> types)
{
    if (!object)
        return {};
    // CIP TLocationWorldMapCoordinateWithStackPositionContext for field objects.
    (void)input::TLocationWorldMapCoordinateWithStackPositionContext(
        position.x, position.y, position.z, object.stackPosition);
    if (const uint16_t clientId = object.clientId())
        (void)input::TObjectApperanceTypeGameActionContext(clientId);
    return input::filterActions(Provider::objectActions(object, field, position), types);
}

TGameActionPtr firstSmartClick(const TGameActionList& actions)
{
    for (const TGameActionPtr& action : actions) {
        if (action->type == A::SMART_CLICK)
            return action;
    }
    return nullptr;
}

int defaultActionOf(const TFieldObject& object, const TWorldMapField* field)
{
    if (object.creatureId)
        return 0;
    const appearances::TAppearanceInstancePtr appearance =
        field && object.stackPosition >= 0 ? field->appearanceAt(object.stackPosition) : object.appearance;
    ThingType* type = appearance ? appearance->thingType() : nullptr;
    return type && type->hasAction() ? type->getDefaultAction() : 0;
}

void append(TGameActionList& to, const TGameActionList& from)
{
    to.insert(to.end(), from.begin(), from.end());
}

}

TGameActionList TWorldMapGameActionProvider::actionsAt(const QPoint& mapPixel) const
{
    const TWorldMapPick pick = TWorldMapPicker::pick(m_provider, mapPixel);
    if (!pick.isValid())
        return Provider::creatureActions(0);

    TGameActionList result;
    result.push_back(input::makeMovementIntent(pick.playerFloorPosition));

    if (pick.lightness == 0) {
        auto message = input::mutableClone(*input::makeAction(A::ADD_BENEATH_MIDDLE_WORLDMAP_MESSAGE));
        message->text = qtTrId("you_see_nothing");
        message->color = QColor(Qt::green);
        result.push_back(message);
        result.push_back(input::makeSmartClick(message));
        append(result, Provider::creatureActions(0));
        return result;
    }
    if (!pick.look)
        return result;

    TFieldObject use = pick.use;
    const uint32_t fieldCreatureId = pick.creature.creatureId;
    if (fieldCreatureId) {
        if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
            if (const std::shared_ptr<tibia::creatures::TCreature> row = storage->creatureAt(fieldCreatureId);
                row && input::isHireling(*row))
                use = pick.creature;
        }
    }
    if (!use)
        use = pick.look;
    const uint32_t localId = tibia::game::localPlayerId();
    const bool fieldCreatureIsOwn = fieldCreatureId && localId && fieldCreatureId == localId;

    const TGameActionList lookActions = Provider::objectActions(pick.look, pick.field, pick.position);
    const TGameActionList lookList = input::filterActions(lookActions, { A::LOOK, A::INSPECT_OBJECT, A::OPEN_CYCLOPEDIA, A::OPEN_WEAPON_PROFICIENCY_DIALOG, A::LENSHELP_SHOW, A::SMART_CLICK, A::GET_TEXT_FOR_REPORT });
    const TGameActionList lootList = input::filterActions(lookActions, { A::LOOT_ADD_TO_BLACKWHITELIST, A::LOOT_REMOVE_FROM_BLACKWHITELIST });
    // CIP use-list filter (provider): must include OPEN —
    // Classic right-click rules prefer OPEN before OPEN_IN_NEW_WINDOW; containers only
    // emit OPEN (plus OPEN_IN_NEW_WINDOW except supply stash).
    const TGameActionList useList = actionsOf(use, pick.field, pick.position,
                                              { A::USE, A::USE_WITH, A::OPEN, A::OPEN_IN_NEW_WINDOW, A::QUICK_LOOT,
                                                A::GATHER_ALL, A::WRAP, A::UNWRAP, A::ROTATE_OBJECT,
                                                A::CONFIGURE_SHOW_OFF_SOCKET, A::TRADE_ITEM, A::SMART_CLICK,
                                                A::GET_TEXT_FOR_REPORT });
    const TGameActionList quickLootList = actionsOf(pick.quickLoot, pick.field, pick.position, { A::QUICK_LOOT });
    const TGameActionList targetList = actionsOf(pick.multiUse, pick.field, pick.position, { A::TARGET_SELECTED });

    result.push_back(input::makeObjectAction(A::BROWSE_FIELD, pick.position, -1));
    result.push_back(input::makeObjectAction(A::REPORT_BUG_MAP, pick.position, -1));

    const uint32_t pixelCreatureId = TWorldMapPicker::creatureAt(m_provider, mapPixel);
    const TGameActionList creatureActions = Provider::creatureActions(pixelCreatureId);
    result.push_back(input::makeAction(A::CONTEXT_MENU));
    (void)input::TLocationWorldMapGameActionContext(pick.position.x, pick.position.y, pick.position.z);
    (void)input::TLocationWorldMapCoordinateContext(pick.position.x, pick.position.y, pick.position.z);
    if (pixelCreatureId)
        (void)input::TObjectCreatureGameActionContext(pixelCreatureId);

    // CipSoft creature hit-test (v7 / provider v2): when the cursor is on a creature
    // sprite, MOVE_OBJECT is that creature — field::moveObject prefers items under it.
    // Always canonicalize from TCreatureStorage::position (same as lookCreature);
    // walk-offset sprites and pick.creature on the cursor tile must not keep pick.position
    // as the wire `from` (that yields from==to no-ops / NOTPOSSIBLE).
    TFieldObject moveTarget = pick.move;
    Position movePosition = pick.position;
    const TWorldMapField* moveField = pick.field;
    if (pixelCreatureId) {
        moveTarget = {};
        if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
            if (const std::shared_ptr<tibia::creatures::TCreature> row = storage->creatureAt(pixelCreatureId)) {
                movePosition = row->position();
                if (TWorldMapStorage* map = TWorldMapStorage::instance()) {
                    if (TWorldMapField* field = map->fieldAt(movePosition)) {
                        const int stackPos = field->creatureStackPos(pixelCreatureId);
                        if (stackPos >= 0) {
                            moveTarget.stackPosition = stackPos;
                            moveTarget.creatureId = pixelCreatureId;
                            moveTarget.appearance = field->appearanceAt(stackPos);
                            moveField = field;
                        }
                    }
                }
            }
        }
        // Do not fall back to pick.move (item under the creature) when resolve fails.
    }
    const TGameActionList moveList = actionsOf(moveTarget, moveField, movePosition, { A::MOVE_OBJECT });

    TGameActionPtr smartClick;
    TGameActionPtr ownLook;
    if (pick.position.z != pick.playerFloorPosition.z) {
        smartClick = input::makeSmartClick(input::makeMovementIntent(pick.playerFloorPosition));
    } else {
        if (pixelCreatureId)
            smartClick = firstSmartClick(creatureActions);
        if (smartClick && smartClick->inner->type == A::LOOK_AT_CREATURE && fieldCreatureIsOwn) {
            ownLook = smartClick;
            smartClick = nullptr;
        }
        if (!smartClick)
            smartClick = firstSmartClick(useList);
        if (!smartClick)
            smartClick = firstSmartClick(lookList);
    }
    if (smartClick && smartClick->inner->type == A::LOOK) {
        const int useDefault = defaultActionOf(use, pick.field);
        const int lookDefault = defaultActionOf(pick.look, pick.field);
        if ((useDefault == 0 || useDefault == 4) && (lookDefault == 0 || lookDefault == 4) && pick.walkability != 1) {
            const auto target = input::TMovementIntentTargetCoordinate(pick.position.x, pick.position.y, pick.position.z);
            (void)target;
            smartClick = input::makeSmartClick(input::makeMovementIntent(pick.position));
        }
    }
    if (ownLook && smartClick && pixelCreatureId && localId && pixelCreatureId == localId) {
        const TGameActionPtr& inner = smartClick->inner;
        if (inner->type == A::SET_PLAYER_MOVEMENT_INTENT && inner->position == tibia::game::localPlayerPosition())
            smartClick = ownLook;
    }
    if (smartClick)
        result.push_back(smartClick);

    append(result, lookList);
    append(result, useList);
    append(result, moveList);
    append(result, targetList);
    append(result, creatureActions);
    append(result, lootList);
    append(result, quickLootList);
    return result;
}

TWorldMapGameActionProvider::TMouseTargetObject TWorldMapGameActionProvider::mouseTargetObjectOf(
    const TGameActionPtr& action)
{
    if (!action)
        return {};
    switch (action->type) {
    case A::USE:
    case A::USE_WITH:
    case A::WRAP:
    case A::UNWRAP:
    case A::OPEN:
    case A::OPEN_IN_NEW_WINDOW:
    case A::LOOK: {
        TMouseTargetObject target;
        target.typeId = action->objectClientId;
        if (!target.typeId)
            return {};
        target.field = action->position;
        target.stackPos = action->stackPosition;
        return target;
    }
    case A::USE_ON_CREATURE:
    case A::LOOK_AT_CREATURE:
    case A::ATTACK_CREATURE:
    case A::STOP_ATTACKING_CREATURE:
    case A::FOLLOW_CREATURE:
    case A::STOP_FOLLOWING_CREATURE:
    case A::TEMPORARY_SELECT_CREATURE:
    case A::COPY_CREATURE_WITH_ID_NAME_TO_CLIPBOARD:
    case A::START_TALKING_TO_NPC:
    case A::SET_PLAYER_MOVEMENT_INTENT:
        // Creature hover is id-based (see mouseTargetCreatureIdOf).
        return {};
    case A::SMART_CLICK:
        return mouseTargetObjectOf(action->inner);
    default:
        return {};
    }
}

uint32_t TWorldMapGameActionProvider::mouseTargetCreatureIdOf(const TGameActionPtr& action)
{
    if (!action)
        return 0;
    switch (action->type) {
    case A::USE_ON_CREATURE:
    case A::LOOK_AT_CREATURE:
    case A::ATTACK_CREATURE:
    case A::STOP_ATTACKING_CREATURE:
    case A::FOLLOW_CREATURE:
    case A::STOP_FOLLOWING_CREATURE:
    case A::TEMPORARY_SELECT_CREATURE:
    case A::COPY_CREATURE_WITH_ID_NAME_TO_CLIPBOARD:
    case A::START_TALKING_TO_NPC:
        return action->creatureId;
    case A::SET_PLAYER_MOVEMENT_INTENT: {
        TWorldMapStorage* storage = TWorldMapStorage::instance();
        TWorldMapField* field = storage ? storage->fieldAt(action->position) : nullptr;
        if (!field || field->isEmpty())
            return 0;
        const TFieldObject look = field::lookObject(field);
        return defaultActionOf(look, field) == PLAYER_ACTION_AUTOWALK_HIGHLIGHT ? look.creatureId : 0;
    }
    case A::SMART_CLICK:
        return mouseTargetCreatureIdOf(action->inner);
    default:
        return 0;
    }
}

int TWorldMapGameActionProvider::cursorShapeOf(const TGameActionPtr& action)
{
    if (!action)
        return Cursor::Default;
    switch (action->type) {
    case A::USE:
    case A::USE_WITH:
    case A::USE_ON_CREATURE:
    case A::USE_ON_TARGET:
    case A::USE_ON_YOURSELF:
    case A::TRY_TO_EQUIP:
    case A::WRAP:
    case A::UNWRAP:
        return Cursor::Use;
    case A::LOOK:
    case A::LOOK_AT_CREATURE:
        return Cursor::Look;
    case A::QUICK_LOOT:
        return Cursor::QuickLoot;
    case A::OPEN:
    case A::OPEN_IN_NEW_WINDOW:
        return Cursor::Open;
    case A::ATTACK_CREATURE:
    case A::STOP_ATTACKING_CREATURE:
        return Cursor::Attack;
    case A::MOVE_OBJECT:
    case A::START_TARGET_SELECTION:
    case A::TARGET_SELECTED:
    case A::TARGET_SELECTION_FINISHED:
        return Cursor::Crosshair;
    case A::SET_PLAYER_MOVEMENT_INTENT:
    case A::SHOW_MESSAGE_YOU_SEE_NOTHING:
        return Cursor::Walk;
    case A::START_TALKING_TO_NPC:
        return Cursor::Talk;
    case A::SMART_CLICK:
        return cursorShapeOf(action->inner);
    default:
        return Cursor::Default;
    }
}

}
