#if defined(__EMSCRIPTEN__)

// Executes Emscripten async callbacks (emscripten_fetch, emscripten_async_call,
// SOCKFS) on main's JSPI activation. browser/jspi-runtime.js captures each
// callUserCallback table call and posts {fp, args, free} to this Qt
// suspend/resume handler, so the callback runs inside processEvents like a DOM
// event — where qtclient-fs may suspend — instead of as a raw JS→wasm entry
// on top of a suspended main.

#include <QtCore/qcoreapplication.h>
#include <QtCore/private/qwasmsuspendresumecontrol_p.h>

#include <emscripten.h>
#include <emscripten/fetch.h>
#include <emscripten/val.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

uint32_t argAt(const emscripten::val& args, int i)
{
    return static_cast<uint32_t>(args[i].as<double>());
}

// emscripten_fetch callback: restore the struct fields of the event that was
// captured (later events overwrote them), or drop it if the fetch was closed.
bool prepareFetchCall(const emscripten::val& call)
{
    if (call["fetchId"].isUndefined())
        return true;
    const emscripten::val jspi = emscripten::val::module_property("qtclientJspi");
    if (!jspi.call<bool>("fetchAlive", call))
        return false;
    auto* fetch = reinterpret_cast<emscripten_fetch_t*>(
        static_cast<uintptr_t>(call["args"][0].as<double>()));
    const std::vector<uint8_t> snapshot =
        emscripten::convertJSArrayToNumberVector<uint8_t>(call["snapshot"]);
    constexpr size_t begin = offsetof(emscripten_fetch_t, data);
    constexpr size_t end = offsetof(emscripten_fetch_t, __attributes);
    if (snapshot.size() != end - begin)
        EM_ASM({ abort('qtclient jspi: fetch snapshot size mismatch'); });
    std::memcpy(reinterpret_cast<char*>(fetch) + begin, snapshot.data(), snapshot.size());
    return true;
}

// Qt's QNetworkReplyWasmImpl copies the body out in its onsuccess / onerror
// callback, then forgets the fetch (m_fetch = nullptr) without
// emscripten_fetch_close — so the LOAD_TO_MEMORY body buffer of every request
// stays in the heap (the launcher's asset install alone held ~250 MB).
// Nothing touches a fetch after its terminal callback, so close it here.
void closeFinishedFetch(const emscripten::val& call)
{
    if (call["fetchId"].isUndefined() || !call["fetchTerminal"].as<bool>())
        return;
    const emscripten::val jspi = emscripten::val::module_property("qtclientJspi");
    if (!jspi.call<bool>("fetchAlive", call))
        return;
    emscripten_fetch_close(reinterpret_cast<emscripten_fetch_t*>(
        static_cast<uintptr_t>(call["args"][0].as<double>())));
}

void runMainCall(emscripten::val call)
{
    if (!prepareFetchCall(call))
        return;
    const uint32_t fp = static_cast<uint32_t>(call["fp"].as<double>());
    const emscripten::val args = call["args"];
    const int argc = args["length"].as<int>();
    // Every routed Emscripten callback returns void and takes i32 arguments
    // (pointers / ints); jspi-runtime.js aborts before posting anything else.
    switch (argc) {
    case 0:
        reinterpret_cast<void (*)()>(fp)();
        break;
    case 1:
        reinterpret_cast<void (*)(uint32_t)>(fp)(argAt(args, 0));
        break;
    case 2:
        reinterpret_cast<void (*)(uint32_t, uint32_t)>(fp)(argAt(args, 0), argAt(args, 1));
        break;
    case 3:
        reinterpret_cast<void (*)(uint32_t, uint32_t, uint32_t)>(fp)(
            argAt(args, 0), argAt(args, 1), argAt(args, 2));
        break;
    case 4:
        reinterpret_cast<void (*)(uint32_t, uint32_t, uint32_t, uint32_t)>(fp)(
            argAt(args, 0), argAt(args, 1), argAt(args, 2), argAt(args, 3));
        break;
    default:
        EM_ASM({ abort('qtclient jspi: unsupported callback arity ' + $0); }, argc);
        return;
    }
    closeFinishedFetch(call);
    const uint32_t freePtr = static_cast<uint32_t>(call["free"].as<double>());
    if (freePtr)
        std::free(reinterpret_cast<void*>(static_cast<uintptr_t>(freePtr)));
}

void installMainCalls()
{
    const uint32_t index = QWasmSuspendResumeControl::get()->registerEventHandler(&runMainCall);
    // One statement per value: commas inside EM_ASM code split macro arguments.
    EM_ASM({
        var jspi = Module.qtclientJspi;
        if (!jspi)
            abort('qtclient jspi: browser/jspi-runtime.js post-js is not linked');
        var L = {};
        L.data = $1;
        L.attr = $2;
        L.attrSize = $3;
        L.callbacks = [];
        L.callbacks.push($4);
        L.callbacks.push($5);
        L.callbacks.push($6);
        L.callbacks.push($7);
        jspi.fetchLayout = L;
        jspi.mainCallHandler = $0;
    },
        index,
        static_cast<int>(offsetof(emscripten_fetch_t, data)),
        static_cast<int>(offsetof(emscripten_fetch_t, __attributes)),
        static_cast<int>(sizeof(emscripten_fetch_attr_t)),
        static_cast<int>(offsetof(emscripten_fetch_attr_t, onsuccess)),
        static_cast<int>(offsetof(emscripten_fetch_attr_t, onerror)),
        static_cast<int>(offsetof(emscripten_fetch_attr_t, onprogress)),
        static_cast<int>(offsetof(emscripten_fetch_attr_t, onreadystatechange)));
}

} // namespace

Q_COREAPP_STARTUP_FUNCTION(installMainCalls)

#endif // __EMSCRIPTEN__
