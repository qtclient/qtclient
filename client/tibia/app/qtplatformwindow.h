#pragma once

#include <QPointer>
#include <QWindow>

namespace tibia::app {

/// Qt already owns the native window. This only remembers which window the
/// process is using so later code can ask for it.
class QtPlatformWindow
{
public:
    void attach(QWindow* window);
    QWindow* window() const { return m_window; }

private:
    QPointer<QWindow> m_window;
};

QtPlatformWindow& qtWindow();

}
