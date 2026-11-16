#pragma once

#include "qtconnectionmanager.h"

#include <QObject>
#include <QTimer>

#include <memory>

namespace tibia {
namespace shared {

/// CIP `shared::TQtConnectionWrapper`: funnels source signals into `signal`,
/// either at once or after an asynchronous delay (a burst of source signals
/// within the delay gives one `signal`), optionally only for the first source
/// signal since the last resetEmitCount.
class TQtConnectionWrapper : public QObject
{
    Q_OBJECT

public:
    TQtConnectionWrapper(int AsyncDelay, bool OnlyFirstSignal);
    ~TQtConnectionWrapper() override;

    /// Connects a source signal to the wrapper.
    template<typename Sender, typename Signal>
    void connectTo(const Sender* sender, Signal sourceSignal)
    {
        m_connections.add(connect(sender, sourceSignal, this, &TQtConnectionWrapper::onSignalEmitted));
    }
    void disconnectAll();
    /// 0 emits synchronously; otherwise source signals start a single-shot timer.
    void setAsyncDelay(int AsyncDelay);
    void setOnlyFirstSignal(bool OnlyFirstSignal);
    void resetEmitCount();

Q_SIGNALS:
    void signal();

public Q_SLOTS:
    void onSignalEmitted();
    void onAsyncTimerTimeout();

private:
    std::unique_ptr<QTimer> m_asyncTimer;
    qint64 m_emitCount = 0;
    bool m_onlyFirstSignal = false;
    TQtConnectionManager m_connections;
};

}
}
