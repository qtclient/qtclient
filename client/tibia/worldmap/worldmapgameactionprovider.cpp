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
using input::IGameActionPtr;
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
    for (const IGameActionPtr& action : actions) {
        if (action->gameActionType() == A::SMART_CLICK)
            return input::unportedGameAction(action);
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
    const TWorldMapPick pick = TWorldMapPicker::pick(m_provider, m_viewport, mapPixel);
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
    // On the map a container opens in a new window only: OPEN is not offered.
    const TGameActionList useList = actionsOf(use, pick.field, pick.position,
                                              { A::USE, A::USE_WITH, A::OPEN_IN_NEW_WINDOW, A::QUICK_LOOT,
                                                A::GATHER_ALL, A::WRAP, A::UNWRAP, A::ROTATE_OBJECT,
                                                A::CONFIGURE_SHOW_OFF_SOCKET, A::TRADE_ITEM, A::SMART_CLICK,
                                                A::GET_TEXT_FOR_REPORT });
    // The quick-loot object adds its own quick loot unless it is the use object.
    const TGameActionList quickLootList = pick.quickLoot.appearance != use.appearance
                                              ? actionsOf(pick.quickLoot, pick.field, pick.position, { A::QUICK_LOOT })
                                              : TGameActionList{};
    const TGameActionList targetList = actionsOf(pick.multiUse, pick.field, pick.position, { A::TARGET_SELECTED });

    result.push_back(input::makeObjectGameAction(A::BROWSE_FIELD, pick.position, -1));
    result.push_back(input::makeObjectAction(A::REPORT_BUG_MAP, pick.position, -1));

    const uint32_t pixelCreatureId = TWorldMapPicker::creatureAt(m_provider, m_viewport, mapPixel);
    const TGameActionList creatureActions = Provider::creatureActions(pixelCreatureId);
    result.push_back(input::makeGameAction(A::CONTEXT_MENU));
    (void)input::TLocationWorldMapGameActionContext(pick.position.x, pick.position.y, pick.position.z);
    (void)input::TLocationWorldMapCoordinateContext(pick.position.x, pick.position.y, pick.position.z);
    if (pixelCreatureId)
        (void)input::TObjectCreatureGameActionContext(pixelCreatureId);

    const TGameActionList moveList = actionsOf(pick.move, pick.field, pick.position, { A::MOVE_OBJECT });

    TGameActionPtr smartClick;
    TGameActionPtr ownLook;
    if (pick.position.z != pick.playerFloorPosition.z) {
        smartClick = input::makeSmartClick(input::makeMovementIntent(pick.playerFloorPosition));
    } else {
        if (pixelCreatureId)
            smartClick = firstSmartClick(creatureActions);
        if (smartClick && smartClick->inner->gameActionType() == A::LOOK_AT_CREATURE && fieldCreatureIsOwn) {
            ownLook = smartClick;
            smartClick = nullptr;
        }
        if (!smartClick)
            smartClick = firstSmartClick(useList);
        if (!smartClick)
            smartClick = firstSmartClick(lookList);
    }
    if (smartClick && smartClick->inner->gameActionType() == A::LOOK) {
        const int useDefault = defaultActionOf(use, pick.field);
        const int lookDefault = defaultActionOf(pick.look, pick.field);
        if ((useDefault == 0 || useDefault == 4) && (lookDefault == 0 || lookDefault == 4) && pick.walkability != 1) {
            smartClick = input::makeSmartClick(input::makeMovementIntent(pick.position));
        }
    }
    if (ownLook && smartClick && pixelCreatureId && localId && pixelCreatureId == localId) {
        const TGameActionPtr inner = input::unportedGameAction(smartClick->inner);
        if (inner && inner->type == A::SET_PLAYER_MOVEMENT_INTENT && inner->position == tibia::game::localPlayerPosition())
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
    const input::IGameActionPtr& pGameAction)
{
    if (!pGameAction)
        return {};
    switch (pGameAction->gameActionType()) {
    case A::USE:
    case A::USE_WITH:
    case A::WRAP:
    case A::UNWRAP:
    case A::OPEN:
    case A::OPEN_IN_NEW_WINDOW:
    case A::LOOK: {
        const std::optional<input::TObjectGameActionContext> object = input::objectContextOf(*pGameAction);
        TMouseTargetObject target;
        target.typeId = object ? object->objectClientId : 0;
        if (!target.typeId)
            return {};
        target.field = object->position;
        target.stackPos = object->stackPosition;
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
        if (const TGameActionPtr smartClick = input::unportedGameAction(pGameAction))
            return mouseTargetObjectOf(smartClick->inner);
        return {};
    default:
        return {};
    }
}

uint32_t TWorldMapGameActionProvider::mouseTargetCreatureIdOf(const input::IGameActionPtr& pGameAction)
{
    if (!pGameAction)
        return 0;
    if (const auto* creature = input::gameActionAs<input::TGameActionCreature>(pGameAction))
        return creature->gameActionType() == A::TEMPORARY_SELECT_CREATURE || creature->gameActionType() == A::LOOK_AT_CREATURE
                || creature->gameActionType() == A::ATTACK_CREATURE || creature->gameActionType() == A::STOP_ATTACKING_CREATURE
                || creature->gameActionType() == A::FOLLOW_CREATURE || creature->gameActionType() == A::STOP_FOLLOWING_CREATURE
                || creature->gameActionType() == A::COPY_CREATURE_WITH_ID_NAME_TO_CLIPBOARD
            ? creature->creatureId()
            : 0;
    if (const auto* npc = input::gameActionAs<input::TGameActionStartTalkingToNPC>(pGameAction))
        return npc->creatureId();
    const TGameActionPtr action = input::unportedGameAction(pGameAction);
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
        const TFieldObject look = field->lookObject();
        return defaultActionOf(look, field) == PLAYER_ACTION_AUTOWALK_HIGHLIGHT ? look.creatureId : 0;
    }
    case A::SMART_CLICK:
        return mouseTargetCreatureIdOf(action->inner);
    default:
        return 0;
    }
}

int TWorldMapGameActionProvider::cursorShapeOf(const input::IGameActionPtr& pGameAction)
{
    if (!pGameAction)
        return Cursor::Default;
    switch (pGameAction->gameActionType()) {
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
        if (const TGameActionPtr smartClick = input::unportedGameAction(pGameAction))
            return cursorShapeOf(smartClick->inner);
        return Cursor::Default;
    default:
        return Cursor::Default;
    }
}

}
