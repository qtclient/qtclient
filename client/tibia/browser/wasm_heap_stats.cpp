#if defined(__EMSCRIPTEN__)

// Heap telemetry for the portal's /play diagnostics. The wasm heap is a fixed
// reservation (ALLOW_MEMORY_GROWTH=0); these numbers size INITIAL_MEMORY from
// measured usage. Calling it from JS is a plain synchronous wasm call (no
// suspension, no fs), so it is safe while main is parked.

#include <emscripten.h>
#include <emscripten/heap.h>
#include <emscripten/stack.h>
#include <malloc.h>
#include <unistd.h>

#include <cstdint>

extern "C" EMSCRIPTEN_KEEPALIVE void qtclient_heap_stats_publish()
{
    const struct mallinfo mi = mallinfo();
    // One statement per field: a comma inside the code would split EM_ASM's
    // macro arguments.
    EM_ASM({
        var h = Module.qtclientHeap = {};
        h.reserved = $0;
        h.sbrkTop = $1;
        h.mallocInUse = $2;
        h.mallocFootprint = $3;
        h.mallocMaxFootprint = $4;
        h.stackSize = $5;
    },
        static_cast<double>(emscripten_get_heap_size()),
        static_cast<double>(reinterpret_cast<uintptr_t>(sbrk(0))),
        static_cast<double>(mi.uordblks),
        static_cast<double>(mi.arena),
        static_cast<double>(mi.usmblks),
        static_cast<double>(emscripten_stack_get_base() - emscripten_stack_get_end()));
}

#endif // __EMSCRIPTEN__
