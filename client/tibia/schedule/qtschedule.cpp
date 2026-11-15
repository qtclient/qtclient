#include <tibia/schedule/schedule.h>

#include <QCoreApplication>
#include <QMetaObject>
#include <QThread>
#include <QTimer>

#include <algorithm>


namespace tibia::schedule {

TScheduledTask::TScheduledTask(std::function<void()> callback)
    : m_callback(std::move(callback))
{
}

TScheduledTask::~TScheduledTask()
{
    stopTimer();
}

void TScheduledTask::stopTimer()
{
    if (!m_timer)
        return;
    m_timer->stop();
    m_timer->deleteLater();
    m_timer = nullptr;
}

void TScheduledTask::runOnce()
{
    stopTimer();
    if (!m_canceled && !m_executed && m_callback) {
        auto callback = std::move(m_callback);
        m_executed = true;
        callback();
    }
    m_callback = nullptr;
}

void TScheduledTask::cancel()
{
    m_canceled = true;
    stopTimer();
    m_callback = nullptr;
}

void TScheduledTask::armQueued()
{
    const auto self = shared_from_this();
    auto* app = QCoreApplication::instance();
    if (!app) {
        self->runOnce();
        return;
    }
    QMetaObject::invokeMethod(app, [self] { self->runOnce(); }, Qt::QueuedConnection);
}

void TScheduledTask::armTimer(const int delayMs, const bool repeat)
{
    const auto self = shared_from_this();
    auto start = [self, delayMs, repeat] {
        if (self->m_canceled)
            return;
        auto* timer = new QTimer(QCoreApplication::instance());
        self->m_timer = timer;
        timer->setSingleShot(!repeat);
        timer->setInterval(std::max(0, delayMs));
        QObject::connect(timer, &QTimer::timeout, timer, [self, repeat] {
            if (self->m_canceled)
                return;
            if (!repeat) {
                self->runOnce();
                return;
            }
            if (self->m_callback) {
                // The callback may cancel the task; keep it alive for the call.
                auto callback = self->m_callback;
                callback();
                self->m_executed = true;
            }
        });
        timer->start();
    };
    auto* app = QCoreApplication::instance();
    if (!app || QThread::currentThread() == app->thread())
        start();
    else
        QMetaObject::invokeMethod(app, std::move(start), Qt::QueuedConnection);
}

TScheduledTaskPtr post(std::function<void()> callback)
{
    auto task = std::make_shared<TScheduledTask>(std::move(callback));
    task->armQueued();
    return task;
}

TScheduledTaskPtr after(std::function<void()> callback, const int delay)
{
    auto task = std::make_shared<TScheduledTask>(std::move(callback));
    task->armTimer(delay, false);
    return task;
}

TScheduledTaskPtr every(std::function<void()> callback, const int delay)
{
    auto task = std::make_shared<TScheduledTask>(std::move(callback));
    task->armTimer(delay, true);
    return task;
}

}
