#pragma once

#include <cip_gen.h>

#include <QKeySequence>

#include <set>

namespace tibia {
namespace input {

class TKeyboardShortcutHandler;

/// The application key filter of the keyboard shortcuts (installed by
/// TEventFilterManager): a key press whose sequence is bound goes to the
/// handler, which decides whether it is eaten. Modifier keys alone pass.
class TKeyboardShortcutHandlerEventFilter final : public cip::tibia::input::TKeyboardShortcutHandlerEventFilter
{
    Q_OBJECT
public:
    TKeyboardShortcutHandlerEventFilter(TKeyboardShortcutHandler* keyboardShortcutHandler, QObject* parent = nullptr);

    /// The bound key sequences (rebuilt by the handler on a hotkey change).
    void setKeySequences(std::set<QKeySequence> keySequences) { m_keySequences = std::move(keySequences); }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    TKeyboardShortcutHandler* m_keyboardShortcutHandler;
    std::set<QKeySequence> m_keySequences;
};

}
}
