#include "asyncdispatcher.h"

#include <algorithm>
#include <thread>

namespace {

int threadCount()
{
    static constexpr int MinThreads = 2;
    static constexpr int MaxThreads = 12;
    const int hw = static_cast<int>(std::thread::hardware_concurrency());
    return std::clamp(hw > 0 ? hw - 1 : MinThreads, MinThreads, MaxThreads);
}

} // namespace

std::unique_ptr<AsyncDispatcher> g_asyncDispatcher = std::make_unique<AsyncDispatcher>(threadCount());
