#include "singlethreadedlowpriorityexecutor.h"

namespace tibia::shared {

void executors_private::TaskExecutorObject::execute(QRunnable* Task, TExecutor* ExecutedSignalEmitter)
{
    Task->run();
    QMetaObject::invokeMethod(ExecutedSignalEmitter, "emitExecutedSignal", Qt::QueuedConnection, Q_ARG(QRunnable*, Task));
}

TSingleThreadedLowPriorityExecutor::TSingleThreadedLowPriorityExecutor()
    : TExecutor(nullptr)
{
    qRegisterMetaType<QRunnable*>("QRunnable*");
    qRegisterMetaType<TExecutor*>("TExecutor*");
    m_thread.start();
    m_thread.setPriority(QThread::LowPriority);
    m_taskExecutorObject = new executors_private::TaskExecutorObject();
    m_taskExecutorObject->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_taskExecutorObject, &QObject::deleteLater);
}

TSingleThreadedLowPriorityExecutor::~TSingleThreadedLowPriorityExecutor()
{
    m_thread.quit();
    m_thread.wait();
}

void TSingleThreadedLowPriorityExecutor::start(QRunnable* Task)
{
    QMetaObject::invokeMethod(m_taskExecutorObject, "execute", Qt::QueuedConnection, Q_ARG(QRunnable*, Task),
        Q_ARG(TExecutor*, this));
}

}
