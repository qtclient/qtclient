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

// b1/65acd8b4b5380319
bool TKeyEventHandler::eventFilter(QObject* watched, QEvent* event)
{
    return cip::tibia::input::TKeyEventHandler::eventFilter(watched, event);
}

// b1/417851cf6a2fe8b1
bool TKeyboardShortcutHandlerEventFilter::eventFilter(QObject* watched, QEvent* event)
{
    return cip::tibia::input::TKeyboardShortcutHandlerEventFilter::eventFilter(watched, event);
}

// b1/469ca00359646b13
bool TRecordKeyboardShortcutHandler::eventFilter(QObject* watched, QEvent* event)
{
    return cip::tibia::input::TRecordKeyboardShortcutHandler::eventFilter(watched, event);
}

} // namespace tibia::input
