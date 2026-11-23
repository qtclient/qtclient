#include "uifocusmanager.h"

#include "input/eventfiltermanager.h"

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
    disconnect(m_otherFocusConnection);
    if (m_hasFocus) {
        qApp->removeEventFilter(&m_clickArea);
        if (input::TEventFilterManager* eventFilters = input::TEventFilterManager::instance())
            eventFilters->unblock();
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
    input::TEventFilterManager* eventFilters = input::TEventFilterManager::instance();
    if (m_hasFocus || !m_isActive || !eventFilters)
        return;
    m_hasFocus = true;
    Q_EMIT focusChanged();
    eventFilters->block();
    qApp->installEventFilter(&m_clickArea);
    // Another element asking for the focus takes it; the chat input getting it
    // back drops it.
    m_otherFocusConnection = connect(eventFilters, &input::TEventFilterManager::requestElementFocus, this, &TUiFocusManager::releaseFocus);
    m_keyboardConnection = connect(eventFilters, &input::TEventFilterManager::releaseElementFocus, this,
                                   &TUiFocusManager::onFocusTakenByEventFilterManager);
}

void TUiFocusManager::releaseFocus()
{
    // Unblocking gives the keyboard back to the chat input; its
    // releaseElementFocus signal drops the focus (onFocusTakenByEventFilterManager).
    if (!m_hasFocus)
        return;
    if (input::TEventFilterManager* eventFilters = input::TEventFilterManager::instance())
        eventFilters->unblock();
}

void TUiFocusManager::onFocusTakenByEventFilterManager()
{
    if (!m_hasFocus)
        return;
    disconnect(m_keyboardConnection);
    disconnect(m_otherFocusConnection);
    qApp->removeEventFilter(&m_clickArea);
    m_hasFocus = false;
    Q_EMIT focusChanged();
}

}
