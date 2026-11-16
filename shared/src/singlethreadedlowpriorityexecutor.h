#pragma once

#include "executor.h"

#include <QThread>

namespace tibia {
namespace shared {

namespace executors_private {

/// CIP `shared::executors_private::TaskExecutorObject`: lives on the worker
/// thread; runs a task there and has the executor report it on its own thread.
class TaskExecutorObject : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

public Q_SLOTS:
    void execute(QRunnable* Task, TExecutor* ExecutedSignalEmitter);
};

}

/// CIP `shared::TSingleThreadedLowPriorityExecutor`: runs tasks one after the
/// other on one low-priority worker thread.
class TSingleThreadedLowPriorityExecutor : public TExecutor
{
    Q_OBJECT

public:
    TSingleThreadedLowPriorityExecutor();
    ~TSingleThreadedLowPriorityExecutor() override;

public Q_SLOTS:
    void start(QRunnable* Task) override;

private:
    QThread m_thread;
    executors_private::TaskExecutorObject* m_taskExecutorObject = nullptr;
};

}
}
