#if defined(__EMSCRIPTEN__)

// Safari-safe blocking on the web's main thread, below all app code.
//
// Every blocking wait (QThreadPool::waitForDone, QThread::wait, QMutex,
// QWaitCondition, std::mutex, …) ends in emscripten_futex_wait. The main
// browser thread cannot Atomics.wait, so Emscripten busy-spins there. Safari
// settles a Worker's async OPFS requests (lookups, create, open, getFile) only
// while the page's main thread is free: a pool thread doing file I/O that the
// GUI thread waits for never finishes (measured: 0 of 200 requests settle while
// main spins; tools/safari-poc section D). CipSoft-parity code waits like on
// desktop, so the fix lives here: the link wraps emscripten_futex_wait
// (QtclientBrowserRuntime.cmake, --wrap) and a long main-thread wait yields one
// browser task at a time (a JSPI suspension) until it is woken.
//
// Short waits (lock hand-offs) spin exactly as before. A wait never suspends
// where JSPI cannot: before main() (static constructors), on pthreads, or in a
// wasm call nested in a main-thread suspension (an OPFS await, Qt waiting for
// events in processEvents(), or this yield itself) — those keep spinning.
// While main is suspended here, DOM events are queued by Qt's
// QWasmSuspendResumeControl like during any other suspension.

#include <emscripten.h>
#include <emscripten/threading.h>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdint>

bool qtclientMainWaitMayYield = false;

namespace {

// Spin this long before the first yield: lock hand-offs finish well within it.
constexpr double kSpinBeforeYieldMs = 10;
// Between two yields there is no spin, only a re-check: measured in Safari
// (tools/safari-poc D5), any spin slice costs ~1 ms per yield (coarse timers)
// and slows the Worker's OPFS work 4-5x; yield-only waits finish in ~1.5x
// the idle time.

} // namespace

// True when the current main-thread stack is the JSPI activation and nothing
// suspended it: Qt's processEvents() wait (control.resume set) and the OPFS /
// futex awaits (Module.qtclientMainSuspended) mean we are a nested call.
EM_JS(int, qtclientMainCanSuspend, (), {
    if (Module.qtclientMainSuspended > 0)
        return 0;
    var control = Module.qtSuspendResumeControl;
    return control && control.resume ? 0 : 1;
});

// One browser task (MessageChannel: no setTimeout clamping), main suspended.
EM_ASYNC_JS(void, qtclientMainYieldTask, (), {
    Module.qtclientMainSuspended = (Module.qtclientMainSuspended | 0) + 1;
    try {
        await new Promise(function (resolve) {
            var channel = new MessageChannel();
            channel.port1.onmessage = function () { resolve(); };
            channel.port2.postMessage(0);
        });
    } finally {
        Module.qtclientMainSuspended--;
    }
});

extern "C" int __real_emscripten_futex_wait(volatile void* addr, uint32_t val, double maxWaitMs);

extern "C" int __wrap_emscripten_futex_wait(volatile void* addr, uint32_t val, double maxWaitMs)
{
    if (!qtclientMainWaitMayYield || !emscripten_is_main_browser_thread())
        return __real_emscripten_futex_wait(addr, val, maxWaitMs);

    const double end = std::isinf(maxWaitMs) ? INFINITY : emscripten_get_now() + maxWaitMs;
    const int r = __real_emscripten_futex_wait(addr, val, std::min(maxWaitMs, kSpinBeforeYieldMs));
    if (r != -ETIMEDOUT)
        return r;
    for (;;) {
        const double now = emscripten_get_now();
        if (now >= end)
            return -ETIMEDOUT;
        if (!qtclientMainCanSuspend())
            return __real_emscripten_futex_wait(addr, val, end - now);
        qtclientMainYieldTask();
        // What Emscripten's main-thread spin does each pass: run calls that
        // pthreads proxied to main, then re-check the value. A changed value
        // is a wake-up; callers re-check their condition after any return,
        // like after a spurious futex wake-up.
        emscripten_main_thread_process_queued_calls();
        if (__atomic_load_n(static_cast<volatile uint32_t*>(addr), __ATOMIC_SEQ_CST) != val)
            return 0;
    }
}

#endif // __EMSCRIPTEN__
