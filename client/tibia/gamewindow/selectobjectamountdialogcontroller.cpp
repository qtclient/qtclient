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
    input::setActionHandler(A::SHOW_SPLIT_STACK_DIALOG, [this](const input::TGameAction& action) { return handleSplitStack(action); });
    input::setActionHandler(A::STASH_OPEN_STOW_STACK_DIALOG, [this](const input::TGameAction& action) { return handleStash(action); });
    input::setActionHandler(A::STASH_OPEN_RETRIEVE_DIALOG, [this](const input::TGameAction& action) { return handleStash(action); });
}

TSelectObjectAmountDialogController::~TSelectObjectAmountDialogController()
{
    input::setActionHandler(A::SHOW_SPLIT_STACK_DIALOG, nullptr);
    input::setActionHandler(A::STASH_OPEN_STOW_STACK_DIALOG, nullptr);
    input::setActionHandler(A::STASH_OPEN_RETRIEVE_DIALOG, nullptr);
}

bool TSelectObjectAmountDialogController::handleSplitStack(const input::TGameAction& action)
{
    const input::TGameActionPtr& move = action.inner;
    if (!move || !move->objectClientId)
        return false;
    // Creatures use wire id Proto::Creature — never a stackable item amount.
    if (move->creatureId || move->objectClientId == Proto::Creature)
        return false;
    const int count = move->amount > 0 ? move->amount : storageSlotCount(move->position, move->stackPosition);
    if (count <= 1)
        return false;
    m_inner = move;
    open(static_cast<int>(move->objectClientId), static_cast<uint>(count));
    return true;
}

bool TSelectObjectAmountDialogController::handleStash(const input::TGameAction& action)
{
    const input::TGameActionPtr& inner = action.inner;
    if (!inner)
        return false;
    int typeId = 0;
    if (action.type == A::STASH_OPEN_RETRIEVE_DIALOG) {
        typeId = inner->parameter;
    } else {
        if (!inner->objectClientId)
            return false;
        typeId = static_cast<int>(inner->objectClientId);
    }
    if (inner->amount == 1) {
        input::publish(inner);
        return true;
    }
    if (inner->amount > 1) {
        m_inner = inner;
        open(typeId, static_cast<uint>(inner->amount));
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
    const A type = m_inner ? m_inner->type : A::MOVE_OBJECT;
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
        auto inner = input::mutableClone(*m_inner);
        inner->amount = static_cast<int>(m_slot->count());
        input::publish(inner);
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
