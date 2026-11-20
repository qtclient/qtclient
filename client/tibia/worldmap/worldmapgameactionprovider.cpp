#include "worldmapgameactionprovider.h"

#include "gamewindow/mousecursorcontroller.h"
#include "input/inputciptypes.h"
#include "input/genericobjectgameactionprovider.h"
#include "renderer/worldmaprenderprovider.h"
#include "worldmap/worldmappicker.h"
#include "worldmap/worldmapstorage.h"

#include "creatures/creaturestorage.h"
#include <tibia/appearances/appearancetype.h>
#include <tibia/game/gamesession.h>
#include <QCoreApplication>

namespace tibia::worldmap {

namespace {

using input::EGameAction;
using input::TGameActionList;
using input::IGameActionPtr;
using A = EGameAction;
using Provider = input::TGenericObjectGameActionProvider;
using Cursor = gamewindow::TMouseCursorController;

TGameActionList actionsOf(const TFieldObject& object, const TWorldMapField* field, const Position& position,
                          std::initializer_list<EGameAction> types)
{
    if (!object)
        return {};
    // The map menu takes a creature's actions from the creature provider (v4);
    // of its object actions only the move.
    if (object.creatureId)
        return input::filterActions(Provider::objectActions(object, field, position), { A::MOVE_OBJECT });
    return input::filterActions(Provider::objectActions(object, field, position), types);
}

std::shared_ptr<const input::TGameActionSmartClick> firstSmartClick(const TGameActionList& actions)
{
    for (const IGameActionPtr& action : actions) {
        if (auto smartClick = std::dynamic_pointer_cast<const input::TGameActionSmartClick>(action))
            return smartClick;
    }
    return nullptr;
}

int defaultActionOf(const TFieldObject& object, const TWorldMapField* field)
{
    if (object.creatureId)
        return 0;
    const appearances::TAppearanceInstancePtr appearance =
        field && object.stackPosition >= 0 ? field->appearanceAt(object.stackPosition) : object.appearance;
    TAppearanceType* type = appearance ? appearance->thingType() : nullptr;
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
        auto message = std::make_shared<input::TGameActionAddTextMessage>(A::ADD_BENEATH_MIDDLE_WORLDMAP_MESSAGE, qtTrId("you_see_nothing"), QColor(Qt::green));
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

    const TGameActionList lookActions =
        pick.look.creatureId ? TGameActionList{} : Provider::objectActions(pick.look, pick.field, pick.position);
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
    result.push_back(std::make_shared<input::TGameActionReportMapBug>(pick.position));

    const uint32_t pixelCreatureId = TWorldMapPicker::creatureAt(m_provider, m_viewport, mapPixel);
    const TGameActionList creatureActions = Provider::creatureActions(pixelCreatureId);
    result.push_back(input::makeGameAction(A::CONTEXT_MENU));

    const TGameActionList moveList = actionsOf(pick.move, pick.field, pick.position, { A::MOVE_OBJECT });

    std::shared_ptr<const input::TGameActionSmartClick> smartClick;
    std::shared_ptr<const input::TGameActionSmartClick> ownLook;
    if (pick.position.z != pick.playerFloorPosition.z) {
        smartClick = input::makeSmartClick(input::makeMovementIntent(pick.playerFloorPosition));
    } else {
        if (pixelCreatureId)
            smartClick = firstSmartClick(creatureActions);
        if (smartClick && smartClick->innerGameAction()->gameActionType() == A::LOOK_AT_CREATURE && fieldCreatureIsOwn) {
            ownLook = smartClick;
            smartClick = nullptr;
        }
        if (!smartClick)
            smartClick = firstSmartClick(useList);
        if (!smartClick)
            smartClick = firstSmartClick(lookList);
    }
    if (smartClick && smartClick->innerGameAction()->gameActionType() == A::LOOK) {
        const int useDefault = defaultActionOf(use, pick.field);
        const int lookDefault = defaultActionOf(pick.look, pick.field);
        if ((useDefault == 0 || useDefault == 4) && (lookDefault == 0 || lookDefault == 4) && pick.walkability != 1) {
            smartClick = input::makeSmartClick(input::makeMovementIntent(pick.position));
        }
    }
    if (ownLook && smartClick && pixelCreatureId && localId && pixelCreatureId == localId) {
        const auto* walk = input::gameActionAs<input::TGameActionPlayerMovementIntent>(smartClick->innerGameAction());
        if (walk && walk->target() == tibia::game::localPlayerPosition())
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

namespace {

using TMouseTarget = TWorldMapGameActionProvider::TMouseTarget;

/// The object at an action's location, when that is a world map field's stack.
TMouseTarget objectAt(const input::TObjectGameActionContext& object)
{
    if (object.locationType() != input::EGameActionContextType::LOCATION_WORLD_MAP_COORDINATE_WITH_STACK_POSITION)
        return {};
    TMouseTarget target;
    target.creatureId = object.creatureId();
    target.typeId = target.creatureId ? 0 : object.objectClientId();
    target.field = object.position();
    target.stackPos = object.stackPosition();
    return target;
}

/// The look object of the field at `coordinate`; with `autowalkOnly` only one
/// whose appearance's default action is the highlighting auto walk.
TMouseTarget lookObjectAt(const Position& coordinate, const bool autowalkOnly)
{
    TWorldMapStorage* storage = TWorldMapStorage::instance();
    TWorldMapField* field = storage ? storage->fieldAt(coordinate) : nullptr;
    if (!field)
        return {};
    const TFieldObject look = field->lookObject();
    if (!look)
        return {};
    if (autowalkOnly) {
        appearances::TAppearanceInstancePtr appearance = look.appearance;
        if (!appearance && look.creatureId) {
            if (creatures::TCreatureStorage* creatures = creatures::TCreatureStorage::instance())
                if (const std::shared_ptr<creatures::TCreature> row = creatures->creatureAt(look.creatureId))
                    appearance = row->outfitAppearance();
        }
        const TAppearanceType* type = appearance ? appearance->thingType() : nullptr;
        if (!type || type->getDefaultAction() != PLAYER_ACTION_AUTOWALK_HIGHLIGHT)
            return {};
    }
    TMouseTarget target;
    target.creatureId = look.creatureId;
    target.typeId = look.creatureId ? 0 : look.clientId();
    target.field = coordinate;
    target.stackPos = look.stackPosition;
    return target;
}

TMouseTarget creatureTarget(const uint32_t creatureId)
{
    TMouseTarget target;
    target.creatureId = creatureId;
    return target;
}

}

TWorldMapGameActionProvider::TMouseTarget TWorldMapGameActionProvider::mouseTargetOf(const input::IGameActionPtr& pGameAction)
{
    if (!pGameAction)
        return {};
    switch (pGameAction->gameActionType()) {
    case A::USE:
    case A::WRAP:
    case A::UNWRAP:
    case A::OPEN:
    case A::OPEN_IN_NEW_WINDOW: {
        const std::optional<input::TObjectGameActionContext> object = input::objectContextOf(*pGameAction);
        return object ? objectAt(*object) : TMouseTarget{};
    }
    case A::USE_WITH: {
        // The target once one is chosen, else the used object.
        const auto* use = input::gameActionAs<input::TGameActionUseWith>(pGameAction);
        if (!use)
            return {};
        return objectAt(use->hasTarget() ? *use->target() : use->context());
    }
    case A::USE_ON_CREATURE:
        if (const auto* use = input::gameActionAs<input::TGameActionUseOnCreature>(pGameAction))
            return creatureTarget(use->targetCreatureId());
        return {};
    case A::LOOK: {
        // The look object of the looked-at field.
        const std::optional<input::TObjectGameActionContext> object = input::objectContextOf(*pGameAction);
        if (!object
            || object->locationType() != input::EGameActionContextType::LOCATION_WORLD_MAP_COORDINATE_WITH_STACK_POSITION)
            return {};
        return lookObjectAt(object->position(), false);
    }
    case A::LOOK_AT_CREATURE:
    case A::ATTACK_CREATURE:
    case A::STOP_ATTACKING_CREATURE:
    case A::FOLLOW_CREATURE:
    case A::STOP_FOLLOWING_CREATURE:
    case A::TEMPORARY_SELECT_CREATURE:
    case A::COPY_CREATURE_WITH_ID_NAME_TO_CLIPBOARD:
        if (const auto* creature = input::gameActionAs<input::TGameActionCreature>(pGameAction))
            return creatureTarget(creature->creatureId());
        return {};
    case A::START_TALKING_TO_NPC:
        if (const auto* npc = input::gameActionAs<input::TGameActionStartTalkingToNPC>(pGameAction))
            return creatureTarget(npc->creatureId());
        return {};
    case A::SET_PLAYER_MOVEMENT_INTENT:
        // A walk to a field highlights its look object when that walks there itself.
        if (const auto* walk = input::gameActionAs<input::TGameActionPlayerMovementIntent>(pGameAction))
            return lookObjectAt(walk->target(), true);
        return {};
    case A::SMART_CLICK:
        if (const auto* smartClick = input::gameActionAs<input::TGameActionSmartClick>(pGameAction))
            return mouseTargetOf(smartClick->innerGameAction());
        return {};
    default:
        return {};
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
        if (const auto* smartClick = input::gameActionAs<input::TGameActionSmartClick>(pGameAction))
            return cursorShapeOf(smartClick->innerGameAction());
        return Cursor::Default;
    default:
        return Cursor::Default;
    }
}

}
