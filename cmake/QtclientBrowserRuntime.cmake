# Browser runtime shared by tibia.wasm and launcher.wasm (/play).
#
# Durable FS: WasmFS with /home/web_user mounted on the origin OPFS through the
# portal's qtclient-fs Worker (browser/qtclient_opfs_backend.cpp), so every
# native file API (QFile, QSaveFile, QSettings, QLockFile, std::filesystem)
# works unchanged. Emscripten's wasmfs_create_opfs_backend busy-waits main on
# OPFS promises, which Safari only settles while main is free: it deadlocks
# Safari, do not use it.
#
# Main-thread Qt + JSPI (no PROXY_TO_PTHREAD: it broke Emval DOM). Async OPFS
# work suspends main; Emscripten async callbacks run on main's activation
# through Qt's event queue. Contract: browser/jspi-runtime.js. Requires wasm
# EH (the Qt kit's -feature-wasm-exceptions), set by each target.

set(_QTCLIENT_BROWSER_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")

function(qtclient_browser_runtime target)
  set(_dir "${_QTCLIENT_BROWSER_ROOT}/client/tibia/browser")
  target_sources(${target} PRIVATE
    "${_dir}/qtclient_opfs_backend.cpp"
    "${_dir}/qtclient_opfs_backend.h"
    "${_dir}/jspi_main_calls.cpp"
    "${_dir}/wasm_heap_stats.cpp")
  # The WasmFS backend API (backend.h / file.h) is internal to Emscripten.
  get_filename_component(_emscripten_root "${CMAKE_C_COMPILER}" DIRECTORY)
  target_include_directories(${target} PRIVATE "${_emscripten_root}/system/lib/wasmfs")
  target_link_libraries(${target} PRIVATE Qt6::CorePrivate)
  target_link_options(${target} PRIVATE
    "SHELL:-sWASMFS"
    # Qt qwasmsocket needs classic SOCKFS callbacks (not pulled in by WasmFS).
    "SHELL:-lsockfs.js"
    # The Qt kit with -feature-wasm-jspi adds it too; the duplicate is harmless.
    "SHELL:-sJSPI"
    "SHELL:--post-js=${_QTCLIENT_BROWSER_ROOT}/browser/jspi-runtime.js"
    # Function names for /play crash stacks (<target>.js.symbols, not published).
    "SHELL:--emit-symbol-map"
    # QNetworkAccessManager uses emscripten_fetch without its IndexedDB cache;
    # opening that DB is a runDependency that can block instantiation.
    "SHELL:-sFETCH_SUPPORT_INDEXEDDB=0")
  # ENV: the portal passes QTCLIENT_* through qtLoad's environment (qtloader
  # throws when ENV is used but not exported). Pool of 1: createWasm
  # Atomics.waits on every pre-spawned Worker's script fetch, which stalls
  # /play; Qt spawns further threads on demand.
  set_target_properties(${target} PROPERTIES
    QT_WASM_EXTRA_EXPORTED_METHODS "ENV"
    QT_WASM_PTHREAD_POOL_SIZE "1")
endfunction()
