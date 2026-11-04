#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include <QThreadPool>

/// CIP async work queue — Qt `QThreadPool` (replaces BS::thread_pool).
class AsyncDispatcher
{
public:
    explicit AsyncDispatcher(int threadCount)
    {
        m_pool.setMaxThreadCount(std::max(1, threadCount));
    }

    template<typename F>
    auto submit_task(F&& fn) -> std::future<std::invoke_result_t<std::decay_t<F>>>
    {
        using R = std::invoke_result_t<std::decay_t<F>>;
        auto promise = std::make_shared<std::promise<R>>();
        auto future = promise->get_future();
        m_pool.start([promise, task = std::function<R()>(std::forward<F>(fn))]() mutable {
            try {
                if constexpr (std::is_void_v<R>) {
                    task();
                    promise->set_value();
                } else {
                    promise->set_value(task());
                }
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
        });
        return future;
    }

    template<typename F>
    void detach_task(F&& fn)
    {
        m_pool.start(std::function<void()>(std::forward<F>(fn)));
    }

    [[nodiscard]] size_t get_thread_count() const
    {
        return static_cast<size_t>(std::max(1, m_pool.maxThreadCount()));
    }

private:
    QThreadPool m_pool;
};

/// Collects `submit_task` futures and waits (BS::multi_future stand-in).
template<typename T = void>
class AsyncMultiFuture
{
public:
    void emplace_back(std::future<T> future) { m_futures.push_back(std::move(future)); }
    void wait()
    {
        for (auto& future : m_futures)
            future.wait();
    }
    void clear() { m_futures.clear(); }

private:
    std::vector<std::future<T>> m_futures;
};

extern std::unique_ptr<AsyncDispatcher> g_asyncDispatcher;
