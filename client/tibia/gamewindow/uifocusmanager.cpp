#include "uifocusmanager.h"

#include "gamewindow/dialogmanager.h"

#include <QGuiApplication>

namespace tibia::gamewindow {

TUiFocusManager::TUiFocusManager(QObject* parent)
    : cip::tibia::gamewindow::TUiFocusManager(parent)
    , m_clickArea(this)
{
    m_isActive = true;
    connect(&m_clickArea, &input::TClickAreaEventFilter::clickedOutside, this, &TUiFocusManager::releaseFocus);
    connect(&m_clickArea, &input::TClickAreaEventFilter::clickedInside, this, &TUiFocusManager::takeFocus);
}

TUiFocusManager::~TUiFocusManager()
{
    disconnect(m_keyboardConnection);
    if (m_hasFocus) {
        qApp->removeEventFilter(&m_clickArea);
        if (TDialogStackController* stack = TDialogStackController::instance())
            stack->unblockKeyboard();
    }
}

void TUiFocusManager::setClickArea(QQuickItem* item)
{
    m_clickArea.setClickArea(item);
}

void TUiFocusManager::setActive(const bool active)
{
    if (active == m_isActive)
        return;
    if (!active)
        releaseFocus();
    m_isActive = active;
    Q_EMIT activeChanged();
}

void TUiFocusManager::takeFocus()
{
    TDialogStackController* stack = TDialogStackController::instance();
    if (m_hasFocus || !m_isActive || !stack)
        return;
    m_hasFocus = true;
    Q_EMIT focusChanged();
    stack->blockKeyboard();
    qApp->installEventFilter(&m_clickArea);
    m_keyboardConnection = connect(stack, &TDialogStackController::chatInputFocusChanged, this, [this](const bool bound) {
        if (bound)
            onFocusTakenByEventFilterManager();
    });
}

void TUiFocusManager::releaseFocus()
{
    if (!m_hasFocus)
        return;
    if (TDialogStackController* stack = TDialogStackController::instance())
        stack->unblockKeyboard();
    onFocusTakenByEventFilterManager();
}

void TUiFocusManager::onFocusTakenByEventFilterManager()
{
    if (!m_hasFocus)
        return;
    disconnect(m_keyboardConnection);
    qApp->removeEventFilter(&m_clickArea);
    m_hasFocus = false;
    Q_EMIT focusChanged();
}

}
