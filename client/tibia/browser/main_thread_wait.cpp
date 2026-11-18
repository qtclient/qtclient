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
// Spin slice between two yields (also serves calls proxied to main meanwhile).
constexpr double kSpinSliceMs = 1;

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
    int r = __real_emscripten_futex_wait(addr, val, std::min(maxWaitMs, kSpinBeforeYieldMs));
    for (;;) {
        if (r != -ETIMEDOUT)
            return r;
        const double now = emscripten_get_now();
        if (now >= end)
            return -ETIMEDOUT;
        if (!qtclientMainCanSuspend())
            return __real_emscripten_futex_wait(addr, val, end - now);
        qtclientMainYieldTask();
        // Woken meanwhile (or the value changed): callers re-check their
        // condition after any return, like after a spurious futex wake-up.
        if (__atomic_load_n(static_cast<volatile uint32_t*>(addr), __ATOMIC_SEQ_CST) != val)
            return 0;
        const double left = end - emscripten_get_now();
        if (left <= 0)
            return -ETIMEDOUT;
        r = __real_emscripten_futex_wait(addr, val, std::min(left, kSpinSliceMs));
    }
}

#endif // __EMSCRIPTEN__
