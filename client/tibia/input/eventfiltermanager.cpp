#include "eventfiltermanager.h"

#include "gamewindow/contextmenucontroller.h"
#include "input/eventfilters.h"
#include "input/keyboardshortcuthandler.h"
#include "input/keyboardshortcuthandlereventfilter.h"
#include "input/keyboardstatusstorage.h"
#include "input/keyeventhandler.h"
#include "input/recordkeyboardshortcuthandler.h"

#include <QCoreApplication>

namespace tibia::input {

namespace {

TEventFilterManager* g_eventFilterManager = nullptr;

}

TEventFilterManager::TEventFilterManager(std::shared_ptr<TKeyboardShortcutHandler> pKeyboardShortcutHandler,
                                         std::shared_ptr<TKeyEventHandler> pKeyEventHandler,
                                         std::shared_ptr<TRecordKeyboardShortcutHandler> pRecordKeyboardShortcutHandler)
    : cip::tibia::input::TEventFilterManager(nullptr)
    , m_pKeyboardShortcutHandler(std::move(pKeyboardShortcutHandler))
    , m_pKeyEventHandler(std::move(pKeyEventHandler))
    , m_pRecordKeyboardShortcutHandler(std::move(pRecordKeyboardShortcutHandler))
    , m_pDiscardAllKeyboardInput(new TDiscardAllKeyboardInputAndSignalEscapePressedEventFilter(this))
    , m_pDiscardUnhandledMouseButtons(new TDiscardUnhandledMouseButtonsEventFilter(this))
    , m_pDiscardMouseWheel(new TDiscardMouseWheelEventFilter(this))
{
    connect(m_pRecordKeyboardShortcutHandler.get(), &TRecordKeyboardShortcutHandler::shortcutPressed, this,
            &TEventFilterManager::keyboardShortcutRecorded);
    g_eventFilterManager = this;
}

TEventFilterManager::~TEventFilterManager()
{
    if (g_eventFilterManager == this)
        g_eventFilterManager = nullptr;
    removeKeyboardShortcutHandler();
    removeKeyEventHandler();
    removeRecordKeyboardShortcutHandler();
    removeDiscardAllKeyboardInput();
    removeDiscardUnhandledMouseButtons();
    removeDiscardMouseWheel();
}

TEventFilterManager* TEventFilterManager::instance()
{
    return g_eventFilterManager;
}

bool TEventFilterManager::isKeyboardShortcutHandlerWanted() const
{
    return m_areKeyboardShortcutsEnabled && !m_isContextMenuShown && !m_isTutorialOverlayShown && !m_isRecordingKeyboardShortcut;
}

bool TEventFilterManager::isKeyEventHandlerWanted() const
{
    return m_areMovementKeysEnabled && !m_isBlocked && !m_isContextMenuShown && !m_isTutorialOverlayShown
        && !m_isRecordingKeyboardShortcut;
}

bool TEventFilterManager::isRecordKeyboardShortcutHandlerWanted() const
{
    return m_isRecordingKeyboardShortcut && !m_isContextMenuShown && !m_isTutorialOverlayShown;
}

bool TEventFilterManager::isDiscardAllKeyboardInputWanted() const
{
    return (m_isContextMenuShown || m_isTutorialOverlayShown) && !m_isRecordingKeyboardShortcut;
}

void TEventFilterManager::update()
{
    if (isKeyboardShortcutHandlerWanted())
        installKeyboardShortcutHandler();
    else
        removeKeyboardShortcutHandler();
    if (m_pKeyboardShortcutHandler)
        m_pKeyboardShortcutHandler->setShortcutsRestricted(m_isBlocked);
    if (isKeyEventHandlerWanted())
        installKeyEventHandler();
    else
        removeKeyEventHandler();
    if (isRecordKeyboardShortcutHandlerWanted())
        installRecordKeyboardShortcutHandler();
    else
        removeRecordKeyboardShortcutHandler();
    if (isDiscardAllKeyboardInputWanted())
        installDiscardAllKeyboardInput();
    else
        removeDiscardAllKeyboardInput();
    if (isDiscardUnhandledMouseButtonsWanted())
        installDiscardUnhandledMouseButtons();
    else
        removeDiscardUnhandledMouseButtons();
    if (isDiscardMouseWheelWanted())
        installDiscardMouseWheel();
    else
        removeDiscardMouseWheel();
}

void TEventFilterManager::enableKeyboardShortcuts()
{
    m_areKeyboardShortcutsEnabled = true;
    update();
}

void TEventFilterManager::disableKeyboardShortcuts()
{
    m_areKeyboardShortcutsEnabled = false;
    update();
}

void TEventFilterManager::enableMovementKeys()
{
    m_areMovementKeysEnabled = true;
    update();
}

void TEventFilterManager::disableMovementKeys()
{
    m_areMovementKeysEnabled = false;
    update();
}

void TEventFilterManager::startRecordingKeyboardShortcut()
{
    m_isRecordingKeyboardShortcut = true;
    update();
}

void TEventFilterManager::stopRecordingKeyboardShortcut()
{
    m_isRecordingKeyboardShortcut = false;
    update();
}

void TEventFilterManager::block()
{
    m_isBlocked = true;
    update();
    requestFocus();
}

void TEventFilterManager::unblock()
{
    m_isBlocked = false;
    update();
    releaseFocus();
}

void TEventFilterManager::requestFocus()
{
    if (m_isFocusRequested)
        return;
    m_isFocusRequested = true;
    Q_EMIT requestElementFocus();
}

void TEventFilterManager::releaseFocus()
{
    if (!m_isFocusRequested)
        return;
    m_isFocusRequested = false;
    Q_EMIT releaseElementFocus();
}

void TEventFilterManager::setFocusRequested(const bool requested)
{
    if (requested)
        requestFocus();
    else
        releaseFocus();
}

void TEventFilterManager::showContextMenu()
{
    m_isContextMenuShown = true;
    update();
    gamewindow::TContextMenuController* contextMenu = gamewindow::TContextMenuController::instance();
    if (!contextMenu)
        return;
    m_contextMenuConnection = connect(m_pDiscardAllKeyboardInput,
                                      &TDiscardAllKeyboardInputAndSignalEscapePressedEventFilter::escapePressed, contextMenu,
                                      &gamewindow::TContextMenuController::onRequestClose);
}

void TEventFilterManager::hideContextMenu()
{
    m_isContextMenuShown = false;
    update();
    disconnect(m_contextMenuConnection);
    m_contextMenuConnection = {};
}

void TEventFilterManager::showTutorialOverlay()
{
    m_isTutorialOverlayShown = true;
    update();
}

void TEventFilterManager::hideTutorialOverlay()
{
    m_isTutorialOverlayShown = false;
    update();
}

void TEventFilterManager::enableDiscardingUnhandledMouseButtons()
{
    m_isDiscardingUnhandledMouseButtons = true;
    update();
}

void TEventFilterManager::disableDiscardingUnhandledMouseButtons()
{
    m_isDiscardingUnhandledMouseButtons = false;
    update();
}

void TEventFilterManager::installKeyboardShortcutHandler()
{
    if (m_isKeyboardShortcutHandlerInstalled || !m_pKeyboardShortcutHandler)
        return;
    qApp->installEventFilter(m_pKeyboardShortcutHandler->keyboardShortcutEventFilter());
    m_isKeyboardShortcutHandlerInstalled = true;
}

void TEventFilterManager::removeKeyboardShortcutHandler()
{
    if (!m_isKeyboardShortcutHandlerInstalled)
        return;
    if (qApp)
        qApp->removeEventFilter(m_pKeyboardShortcutHandler->keyboardShortcutEventFilter());
    m_isKeyboardShortcutHandlerInstalled = false;
}

void TEventFilterManager::installKeyEventHandler()
{
    if (m_isKeyEventHandlerInstalled || !m_pKeyEventHandler)
        return;
    qApp->installEventFilter(m_pKeyEventHandler.get());
    m_isKeyEventHandlerInstalled = true;
}

void TEventFilterManager::removeKeyEventHandler()
{
    if (!m_isKeyEventHandlerInstalled)
        return;
    if (qApp)
        qApp->removeEventFilter(m_pKeyEventHandler.get());
    m_pKeyEventHandler->keyboardStatusStorage()->releaseKeys(false);
    m_isKeyEventHandlerInstalled = false;
}

void TEventFilterManager::installRecordKeyboardShortcutHandler()
{
    if (m_isRecordKeyboardShortcutHandlerInstalled || !m_pRecordKeyboardShortcutHandler)
        return;
    qApp->installEventFilter(m_pRecordKeyboardShortcutHandler.get());
    m_isRecordKeyboardShortcutHandlerInstalled = true;
}

void TEventFilterManager::removeRecordKeyboardShortcutHandler()
{
    if (!m_isRecordKeyboardShortcutHandlerInstalled)
        return;
    if (qApp)
        qApp->removeEventFilter(m_pRecordKeyboardShortcutHandler.get());
    m_isRecordKeyboardShortcutHandlerInstalled = false;
}

void TEventFilterManager::installDiscardAllKeyboardInput()
{
    if (m_isDiscardAllKeyboardInputInstalled)
        return;
    qApp->installEventFilter(m_pDiscardAllKeyboardInput);
    m_isDiscardAllKeyboardInputInstalled = true;
}

void TEventFilterManager::removeDiscardAllKeyboardInput()
{
    if (!m_isDiscardAllKeyboardInputInstalled)
        return;
    if (qApp)
        qApp->removeEventFilter(m_pDiscardAllKeyboardInput);
    m_isDiscardAllKeyboardInputInstalled = false;
}

void TEventFilterManager::installDiscardUnhandledMouseButtons()
{
    if (m_isDiscardUnhandledMouseButtonsInstalled)
        return;
    qApp->installEventFilter(m_pDiscardUnhandledMouseButtons);
    m_isDiscardUnhandledMouseButtonsInstalled = true;
}

void TEventFilterManager::removeDiscardUnhandledMouseButtons()
{
    if (!m_isDiscardUnhandledMouseButtonsInstalled)
        return;
    if (qApp)
        qApp->removeEventFilter(m_pDiscardUnhandledMouseButtons);
    m_isDiscardUnhandledMouseButtonsInstalled = false;
}

void TEventFilterManager::installDiscardMouseWheel()
{
    if (m_isDiscardMouseWheelInstalled)
        return;
    qApp->installEventFilter(m_pDiscardMouseWheel);
    m_isDiscardMouseWheelInstalled = true;
}

void TEventFilterManager::removeDiscardMouseWheel()
{
    if (!m_isDiscardMouseWheelInstalled)
        return;
    if (qApp)
        qApp->removeEventFilter(m_pDiscardMouseWheel);
    m_isDiscardMouseWheelInstalled = false;
}

}
