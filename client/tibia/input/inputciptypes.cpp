#include "input/inputciptypes.h"

#include <QEvent>

namespace tibia::input {

namespace {
struct CipTypedKeep {
    TLocationDepotSearchGameActionContext tLocationDepotSearchGameActionContext;
    TLocationNPCChatKeywordGameActionContext tLocationNPCChatKeywordGameActionContext;
    TUpgradeableObjectApperanceTypeGameActionContext tUpgradeableObjectApperanceTypeGameActionContext;
};
CipTypedKeep& typedKeep() { static CipTypedKeep keep; return keep; }
} // namespace

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    (void)typedKeep();
    (void)parent;
}

bool TKeyEventHandler::eventFilter(QObject* watched, QEvent* event)
{
    return cip::tibia::input::TKeyEventHandler::eventFilter(watched, event);
}

bool TKeyboardShortcutHandlerEventFilter::eventFilter(QObject* watched, QEvent* event)
{
    return cip::tibia::input::TKeyboardShortcutHandlerEventFilter::eventFilter(watched, event);
}

bool TRecordKeyboardShortcutHandler::eventFilter(QObject* watched, QEvent* event)
{
    return cip::tibia::input::TRecordKeyboardShortcutHandler::eventFilter(watched, event);
}

} // namespace tibia::input
