#pragma once

#include <cip_gen.h>

namespace tibia {
namespace input {

/// The application key filter while a keyboard shortcut is recorded (installed
/// by TEventFilterManager): every key press is eaten; a press that is not a
/// repeat emits the shortcut (only the modifiers for a modifier key).
class TRecordKeyboardShortcutHandler final : public cip::tibia::input::TRecordKeyboardShortcutHandler
{
    Q_OBJECT
public:
    using cip::tibia::input::TRecordKeyboardShortcutHandler::TRecordKeyboardShortcutHandler;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
};

}
}
