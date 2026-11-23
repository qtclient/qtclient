#include "client/preventcloseeventfilter.h"

#include <QEvent>

namespace tibia::client {

TPreventCloseEventFilter::TPreventCloseEventFilter(QObject* parent)
    : cip::tibia::client::TPreventCloseEventFilter(parent)
{
}

TPreventCloseEventFilter::~TPreventCloseEventFilter() = default;

bool TPreventCloseEventFilter::eventFilter(QObject* watched, QEvent* event)
{
    Q_UNUSED(watched);
    const bool prevent = m_preventClose && event->type() == QEvent::Close;
    if (prevent) {
        // Host: the platform reads the close event's accept flag; an accepted
        // one lets AppKit close the native window although Qt kept it.
        event->ignore();
        Q_EMIT preventedClose();
    }
    return prevent;
}

}
