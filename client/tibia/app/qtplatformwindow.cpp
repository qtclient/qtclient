#include "qtplatformwindow.h"

namespace tibia::app {

namespace {
QtPlatformWindow qtPlatformWindow;
}

QtPlatformWindow& qtWindow()
{
    return qtPlatformWindow;
}

void QtPlatformWindow::attach(QWindow* window)
{
    m_window = window;
}

}
