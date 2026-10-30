#include "worldmapgameactionprovider.h"

#include "gamewindow/mousecursorcontroller.h"
#include "input/inputciptypes.h"
#include "input/genericobjectgameactionprovider.h"
#include "renderer/worldmaprenderprovider.h"
#include "worldmap/worldmappicker.h"

#include <tibia/assets/thingtype.h>
#include <tibia/world/world.h>

#include <QCoreApplication>

namespace tibia::worldmap {

namespace {

using input::EGameAction;
using input::TGameActionList;
using input::TGameActionPtr;
using A = EGameAction;
using Provider = input::TGenericObjectGameActionProvider;
using Cursor = gamewindow::TMouseCursorController;

TGameActionList actionsOf(const TFieldObject& object, const Position& position, std::initializer_list<EGameAction> types)
{
    if (!object)
        return {};
    // CIP TLocationWorldMapCoordinateWithStackPositionContext for field objects.
    (void)input::TLocationWorldMapCoordinateWithStackPositionContext(
        position.x, position.y, position.z, object.stackPosition);
    if (object.object)
        (void)input::TObjectApperanceTypeGameActionContext(object.object->getClientId());
    return input::filterActions(Provider::objectActions(object.object, position, object.stackPosition), types);
}

TGameActionPtr firstSmartClick(const TGameActionList& actions)
{
    for (const TGameActionPtr& action : actions) {
        if (action->type == A::SMART_CLICK)
            return action;
    }
    return nullptr;
}

int defaultActionOf(const ThingPtr& object)
{
    ThingType* type = object && object->isItem() ? object->getThingType() : nullptr;
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
    if (pick.creature && input::isHireling(pick.creature.object->static_self_cast<Creature>()))
        use = pick.creature;
    if (!use)
        use = pick.look;
    const bool fieldCreatureIsOwn = pick.creature && pick.creature.object->isLocalPlayer();

    const TGameActionList lookActions = Provider::objectActions(pick.look.object, pick.position, pick.look.stackPosition);
    const TGameActionList lookList = input::filterActions(lookActions, { A::LOOK, A::INSPECT_OBJECT, A::OPEN_CYCLOPEDIA, A::OPEN_WEAPON_PROFICIENCY_DIALOG, A::LENSHELP_SHOW, A::SMART_CLICK, A::GET_TEXT_FOR_REPORT });
    const TGameActionList lootList = input::filterActions(lookActions, { A::LOOT_ADD_TO_BLACKWHITELIST, A::LOOT_REMOVE_FROM_BLACKWHITELIST });
    const TGameActionList useList = actionsOf(use, pick.position, { A::USE, A::USE_WITH, A::OPEN_IN_NEW_WINDOW, A::QUICK_LOOT, A::GATHER_ALL, A::WRAP, A::UNWRAP, A::ROTATE_OBJECT, A::CONFIGURE_SHOW_OFF_SOCKET, A::TRADE_ITEM, A::SMART_CLICK, A::GET_TEXT_FOR_REPORT });
    const TGameActionList quickLootList = actionsOf(pick.quickLoot, pick.position, { A::QUICK_LOOT });
    const TGameActionList targetList = actionsOf(pick.multiUse, pick.position, { A::TARGET_SELECTED });
    const TGameActionList moveList = actionsOf(pick.move, pick.position, { A::MOVE_OBJECT });

    result.push_back(input::makeObjectAction(A::BROWSE_FIELD, nullptr, pick.position, -1));
    result.push_back(input::makeObjectAction(A::REPORT_BUG_MAP, nullptr, pick.position, -1));

    const CreaturePtr pixelCreature = TWorldMapPicker::creatureAt(m_provider, mapPixel);
    const TGameActionList creatureActions = Provider::creatureActions(pixelCreature ? pixelCreature->getId() : 0);
    result.push_back(input::makeAction(A::CONTEXT_MENU));
    (void)input::TLocationWorldMapGameActionContext(pick.position.x, pick.position.y, pick.position.z);
    (void)input::TLocationWorldMapCoordinateContext(pick.position.x, pick.position.y, pick.position.z);
    if (pixelCreature)
        (void)input::TObjectCreatureGameActionContext(pixelCreature->getId());

    TGameActionPtr smartClick;
    TGameActionPtr ownLook;
    if (pick.position.z != pick.playerFloorPosition.z) {
        smartClick = input::makeSmartClick(input::makeMovementIntent(pick.playerFloorPosition));
    } else {
        if (pixelCreature)
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
        const int useDefault = defaultActionOf(use.object);
        const int lookDefault = defaultActionOf(pick.look.object);
        if ((useDefault == 0 || useDefault == 4) && (lookDefault == 0 || lookDefault == 4) && pick.walkability != 1) {
            const auto target = input::TMovementIntentTargetCoordinate(pick.position.x, pick.position.y, pick.position.z);
            (void)target;
            smartClick = input::makeSmartClick(input::makeMovementIntent(pick.position));
        }
    }
    if (ownLook && smartClick && pixelCreature && pixelCreature->isLocalPlayer()) {
        const TGameActionPtr& inner = smartClick->inner;
        if (inner->type == A::SET_PLAYER_MOVEMENT_INTENT && inner->position == pixelCreature->getPosition())
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

ThingPtr TWorldMapGameActionProvider::mouseTargetOf(const TGameActionPtr& action)
{
    if (!action)
        return nullptr;
    switch (action->type) {
    case A::USE:
    case A::USE_WITH:
    case A::WRAP:
    case A::UNWRAP:
    case A::OPEN:
    case A::OPEN_IN_NEW_WINDOW:
    case A::LOOK:
        return action->object;
    case A::USE_ON_CREATURE:
    case A::LOOK_AT_CREATURE:
    case A::ATTACK_CREATURE:
    case A::STOP_ATTACKING_CREATURE:
    case A::FOLLOW_CREATURE:
    case A::STOP_FOLLOWING_CREATURE:
    case A::TEMPORARY_SELECT_CREATURE:
    case A::COPY_CREATURE_WITH_ID_NAME_TO_CLIPBOARD:
    case A::START_TALKING_TO_NPC:
        return action->creatureId ? tibia::world::fields().getCreatureById(action->creatureId) : nullptr;
    case A::SET_PLAYER_MOVEMENT_INTENT: {
        const TilePtr& tile = tibia::world::fields().getTile(action->position);
        if (!tile)
            return nullptr;
        const ThingPtr look = field::lookObject(tile).object;
        return defaultActionOf(look) == PLAYER_ACTION_AUTOWALK_HIGHLIGHT ? look : nullptr;
    }
    case A::SMART_CLICK:
        return mouseTargetOf(action->inner);
    default:
        return nullptr;
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
