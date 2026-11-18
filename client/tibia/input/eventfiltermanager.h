#pragma once

#include <cip_gen.h>

#include <QMetaObject>

#include <memory>

namespace tibia {
namespace input {

class TDiscardAllKeyboardInputAndSignalEscapePressedEventFilter;
class TDiscardMouseWheelEventFilter;
class TDiscardUnhandledMouseButtonsEventFilter;
class TKeyEventHandler;
class TKeyboardShortcutHandler;
class TRecordKeyboardShortcutHandler;

/// Owns which application event filters are installed, from the game window's
/// state:
/// - the keyboard shortcuts' filter: shortcuts enabled (the game screen), no
///   context menu, no tutorial overlay, not recording a shortcut;
/// - the key event handler (walking keys): movement enabled, the keyboard not
///   blocked, and the same three conditions; removing it forgets the held keys;
/// - the shortcut recorder: recording, no context menu, no tutorial overlay;
/// - the discard-all-keys filter: a context menu or the tutorial overlay is
///   shown, not recording (Escape closes the context menu);
/// - the unhandled mouse buttons filter: while the game window exists;
/// - the mouse wheel filter: while the tutorial overlay is shown.
/// Blocking the keyboard (a dialog or a focused UI part) restricts the
/// shortcuts and asks the chat input to give up the focus (requestElementFocus);
/// unblocking gives it back (releaseElementFocus).
class TEventFilterManager final : public cip::tibia::input::TEventFilterManager
{
    Q_OBJECT
public:
    TEventFilterManager(std::shared_ptr<TKeyboardShortcutHandler> pKeyboardShortcutHandler,
                        std::shared_ptr<TKeyEventHandler> pKeyEventHandler,
                        std::shared_ptr<TRecordKeyboardShortcutHandler> pRecordKeyboardShortcutHandler);
    ~TEventFilterManager() override;

    static TEventFilterManager* instance();

    void enableKeyboardShortcuts();
    void disableKeyboardShortcuts();
    void enableMovementKeys();
    void disableMovementKeys();
    void startRecordingKeyboardShortcut();
    void stopRecordingKeyboardShortcut();
    /// A dialog or a focused UI part takes the keyboard.
    void block();
    void unblock();
    bool isBlocked() const { return m_isBlocked; }
    /// A dialog's element asks for the focus (requestElementFocus) or lets go.
    void requestFocus();
    void releaseFocus();
    void setFocusRequested(bool requested);
    /// A context menu is shown: keys are discarded and Escape closes it.
    void showContextMenu();
    void hideContextMenu();
    void showTutorialOverlay();
    void hideTutorialOverlay();
    void enableDiscardingUnhandledMouseButtons();
    void disableDiscardingUnhandledMouseButtons();

    bool isKeyboardShortcutHandlerWanted() const;
    bool isKeyEventHandlerWanted() const;
    bool isRecordKeyboardShortcutHandlerWanted() const;
    bool isDiscardAllKeyboardInputWanted() const;
    bool isDiscardUnhandledMouseButtonsWanted() const { return m_isDiscardingUnhandledMouseButtons; }
    bool isDiscardMouseWheelWanted() const { return m_isTutorialOverlayShown; }

private:
    /// Installs and removes the filters to match the state.
    void update();
    void installKeyboardShortcutHandler();
    void removeKeyboardShortcutHandler();
    void installKeyEventHandler();
    void removeKeyEventHandler();
    void installRecordKeyboardShortcutHandler();
    void removeRecordKeyboardShortcutHandler();
    void installDiscardAllKeyboardInput();
    void removeDiscardAllKeyboardInput();
    void installDiscardUnhandledMouseButtons();
    void removeDiscardUnhandledMouseButtons();
    void installDiscardMouseWheel();
    void removeDiscardMouseWheel();

    bool m_areKeyboardShortcutsEnabled = false;
    bool m_areMovementKeysEnabled = false;
    bool m_isRecordingKeyboardShortcut = false;
    bool m_isBlocked = false;
    bool m_isFocusRequested = false;
    bool m_isContextMenuShown = false;
    bool m_isTutorialOverlayShown = false;
    bool m_isDiscardingUnhandledMouseButtons = false;
    /// Escape of the discard-all-keys filter closes the context menu.
    QMetaObject::Connection m_contextMenuConnection;
    std::shared_ptr<TKeyboardShortcutHandler> m_pKeyboardShortcutHandler;
    bool m_isKeyboardShortcutHandlerInstalled = false;
    std::shared_ptr<TKeyEventHandler> m_pKeyEventHandler;
    bool m_isKeyEventHandlerInstalled = false;
    std::shared_ptr<TRecordKeyboardShortcutHandler> m_pRecordKeyboardShortcutHandler;
    bool m_isRecordKeyboardShortcutHandlerInstalled = false;
    TDiscardAllKeyboardInputAndSignalEscapePressedEventFilter* m_pDiscardAllKeyboardInput;
    bool m_isDiscardAllKeyboardInputInstalled = false;
    TDiscardUnhandledMouseButtonsEventFilter* m_pDiscardUnhandledMouseButtons;
    bool m_isDiscardUnhandledMouseButtonsInstalled = false;
    TDiscardMouseWheelEventFilter* m_pDiscardMouseWheel;
    bool m_isDiscardMouseWheelInstalled = false;
};

}
}
