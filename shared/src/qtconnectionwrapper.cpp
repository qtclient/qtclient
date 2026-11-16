#include "qtconnectionwrapper.h"

namespace tibia::shared {

TQtConnectionWrapper::TQtConnectionWrapper(const int AsyncDelay, const bool OnlyFirstSignal)
    : QObject(nullptr)
{
    setAsyncDelay(AsyncDelay);
    m_onlyFirstSignal = OnlyFirstSignal;
}

TQtConnectionWrapper::~TQtConnectionWrapper() = default;

void TQtConnectionWrapper::disconnectAll()
{
    m_connections.disconnectAll();
}

void TQtConnectionWrapper::setAsyncDelay(const int AsyncDelay)
{
    if (AsyncDelay == 0) {
        if (m_asyncTimer) {
            disconnect(m_asyncTimer.get(), &QTimer::timeout, this, &TQtConnectionWrapper::onAsyncTimerTimeout);
            m_asyncTimer.reset();
        }
        return;
    }
    if (m_asyncTimer)
        return;
    m_asyncTimer = std::make_unique<QTimer>();
    m_asyncTimer->setSingleShot(true);
    m_asyncTimer->setInterval(AsyncDelay);
    connect(m_asyncTimer.get(), &QTimer::timeout, this, &TQtConnectionWrapper::onAsyncTimerTimeout);
}

void TQtConnectionWrapper::setOnlyFirstSignal(const bool OnlyFirstSignal)
{
    m_onlyFirstSignal = OnlyFirstSignal;
}

void TQtConnectionWrapper::resetEmitCount()
{
    m_emitCount = 0;
}

void TQtConnectionWrapper::onSignalEmitted()
{
    const qint64 previousCount = m_emitCount++;
    if (m_onlyFirstSignal && previousCount != 0)
        return;
    if (!m_asyncTimer) {
        Q_EMIT signal();
        return;
    }
    m_asyncTimer->start();
}

void TQtConnectionWrapper::onAsyncTimerTimeout()
{
    Q_EMIT signal();
}

}
