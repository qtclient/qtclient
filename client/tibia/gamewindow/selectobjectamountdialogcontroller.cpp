#include "selectobjectamountdialogcontroller.h"

#include "container/qmlitemslotdata.h"
#include "input/gameactionhandlers.h"

#include <client/item.h>

namespace tibia::gamewindow {

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
    if (!move || !move->object || !move->object->isItem())
        return false;
    const ItemPtr item = move->object->static_self_cast<Item>();
    const int count = item->isStackable() ? item->getCountOrSubType() : 1;
    if (count <= 1)
        return false;
    m_inner = move;
    open(static_cast<int>(item->getId()), static_cast<uint>(count));
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
        if (!inner->object || !inner->object->isItem())
            return false;
        typeId = static_cast<int>(inner->object->static_self_cast<Item>()->getId());
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
    // CIP 0x100d49320 / 0x100d49380.
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
