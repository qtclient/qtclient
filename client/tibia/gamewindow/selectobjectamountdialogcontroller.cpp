#include "selectobjectamountdialogcontroller.h"

#include "appearances/appearanceinstance.h"
#include "container/containerciptypes.h"
#include "container/qmlitemslotdata.h"
#include "input/gameactionhandlers.h"
#include "worldmap/worldmapfield.h"
#include "worldmap/worldmapstorage.h"

#include "protocol/protocolcodes.h"
#include <tibia/assets/thingtype.h>

#include <algorithm>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;

int storageSlotCount(const Position& position, const int stackPosition)
{
    if (position.x != UINT16_MAX) {
        if (worldmap::TWorldMapStorage* storage = worldmap::TWorldMapStorage::instance()) {
            if (worldmap::TWorldMapField* field = storage->fieldAt(position)) {
                if (const appearances::TAppearanceInstancePtr appearance = field->appearanceAt(stackPosition)) {
                    if (const auto* object =
                            dynamic_cast<const appearances::TObjectAppearanceInstance*>(appearance.get())) {
                        if (ThingType* type = object->thingType(); type && type->isStackable())
                            return std::max<int>(1, object->count());
                    }
                }
            }
        }
        return 1;
    }
    if (position.y & 0x40) {
        container::TContainerStorage* storage = container::TContainerStorage::instance();
        if (!storage)
            return 1;
        const container::TStoredContainer* row = storage->containerAt(position.y & 0x3F);
        if (!row)
            return 1;
        const container::TInventorySlot slot =
            storage->slotAt(row->id, static_cast<int>(row->firstIndex) + std::max(0, stackPosition));
        return slot.stackable ? std::max<int>(1, slot.count) : 1;
    }
    container::TInventoryContainer* inventory = container::TInventoryContainer::instance();
    if (!inventory)
        return 1;
    const container::TInventorySlot& slot = inventory->slotAt(position.y);
    return slot.stackable ? std::max<int>(1, slot.count) : 1;
}

}

using A = input::EGameAction;

TSelectObjectAmountDialogController::TSelectObjectAmountDialogController(QObject* parent)
    : TDialogController(QStringLiteral("SelectObjectAmountDialog.qml"), parent)
{
    input::setActionHandler(A::SHOW_SPLIT_STACK_DIALOG, [this](const input::IGameAction& action) { return handleSplitStack(action); });
    input::setActionHandler(A::STASH_OPEN_STOW_STACK_DIALOG, [this](const input::IGameAction& action) { return handleStash(action); });
    input::setActionHandler(A::STASH_OPEN_RETRIEVE_DIALOG, [this](const input::IGameAction& action) { return handleStash(action); });
}

TSelectObjectAmountDialogController::~TSelectObjectAmountDialogController()
{
    input::setActionHandler(A::SHOW_SPLIT_STACK_DIALOG, nullptr);
    input::setActionHandler(A::STASH_OPEN_STOW_STACK_DIALOG, nullptr);
    input::setActionHandler(A::STASH_OPEN_RETRIEVE_DIALOG, nullptr);
}

bool TSelectObjectAmountDialogController::handleSplitStack(const input::IGameAction& action)
{
    const auto* split = input::gameActionAs<input::TGameActionShowSplitStackDialog>(&action);
    const input::TMoveObjectPtr move = split ? split->moveObject() : nullptr;
    if (!move || !move->objectClientId())
        return false;
    // Creatures use wire id Proto::Creature — never a stackable item amount.
    if (input::creatureOfObject(move->context()))
        return false;
    const int count = move->amount() > 0 ? move->amount() : storageSlotCount(move->position(), move->stackPosition());
    if (count <= 1)
        return false;
    m_inner = move;
    open(static_cast<int>(move->objectClientId()), static_cast<uint>(count));
    return true;
}

bool TSelectObjectAmountDialogController::handleStash(const input::IGameAction& action)
{
    input::IGameActionPtr inner;
    int typeId = 0;
    int amount = 0;
    if (const auto* retrieve = input::gameActionAs<input::TGameActionStashOpenRetrieveDialog>(&action)) {
        if (!retrieve->retrieve())
            return false;
        inner = retrieve->retrieve();
        typeId = retrieve->retrieve()->typeId();
        amount = retrieve->retrieve()->amount();
    } else if (const auto* stow = input::gameActionAs<input::TGameActionStashOpenStowStackDialog>(&action)) {
        if (!stow->stowStack() || !stow->stowStack()->objectClientId())
            return false;
        inner = stow->stowStack();
        typeId = static_cast<int>(stow->stowStack()->objectClientId());
        amount = stow->stowStack()->amount();
    } else {
        return false;
    }
    if (amount == 1) {
        input::publish(inner);
        return true;
    }
    if (amount > 1) {
        m_inner = inner;
        open(typeId, static_cast<uint>(amount));
    }
    return true;
}

void TSelectObjectAmountDialogController::open(const int typeId, const uint count)
{
    dialog()->destroy();
    if (m_slot)
        m_slot->deleteLater();
    m_slot = new container::TQMLItemSlotData(typeId, count, {}, this);
    m_slotData = m_slot;
    m_maxAmount = count;
    // CIP.
    const A type = m_inner ? m_inner->gameActionType() : A::MOVE_OBJECT;
    const char* caption = type == A::STASH_STOW_STACK ? "selectamount_stow_caption"
                          : type == A::STASH_RETRIEVE ? "selectamount_retrieve_caption"
                                                      : "selectamount_move_caption";
    const char* verb = type == A::STASH_STOW_STACK ? "selectamount_stow" : type == A::STASH_RETRIEVE ? "selectamount_retrieve" : "selectamount_move";
    m_caption = qtTrId(caption);
    m_message = qtTrId("selectamount_set_desired_number").arg(qtTrId(verb));
    dialog()->create();
    dialog()->show();
}

void TSelectObjectAmountDialogController::okButtonClicked()
{
    if (m_inner && m_slot) {
        // The pending action again, with the chosen amount.
        const int amount = static_cast<int>(m_slot->count());
        if (const auto* move = input::gameActionAs<input::TGameActionMoveObject>(m_inner))
            input::publish(std::make_shared<input::TGameActionMoveObject>(move->context(), move->destination(), move->moveAll(), amount));
        else if (const auto* stow = input::gameActionAs<input::TGameActionStashStowStack>(m_inner))
            input::publish(std::make_shared<input::TGameActionStashStowStack>(stow->context(), amount));
        else if (const auto* retrieve = input::gameActionAs<input::TGameActionStashRetrieve>(m_inner))
            input::publish(std::make_shared<input::TGameActionStashRetrieve>(retrieve->typeId(), amount));
    }
    finish();
}

void TSelectObjectAmountDialogController::cancelButtonClicked()
{
    finish();
}

void TSelectObjectAmountDialogController::closeDialog()
{
    finish();
}

void TSelectObjectAmountDialogController::setCurrentAmount(const uint amount)
{
    if (m_slot)
        m_slot->setCount(amount);
}

void TSelectObjectAmountDialogController::finish()
{
    dialog()->close();
    m_inner.reset();
    m_slotData = nullptr;
    if (m_slot) {
        m_slot->deleteLater();
        m_slot = nullptr;
    }
}

}
