/**
 * JSPI runtime contract for qtclient wasm (tibia + launcher). Shared --post-js.
 *
 * One wasm activation: `main`, entered through WebAssembly.promising. Only main
 * suspends — in Qt's idle wait (QWasmSuspendResumeControl::suspend) and in
 * qtclient-fs I/O (EM_ASYNC_JS). Emscripten's JSPI does not save the C shadow
 * stack across a suspend, so any other JS→wasm entry while main is suspended
 * runs on main's stack region and must neither suspend nor re-enter Qt halfway
 * through an operation.
 *
 * 1. Raw table entries. getWasmTableEntry returns the plain WebAssembly
 *    function (never a promising wrapper): invoke_* / Qt EH call table slots
 *    synchronously and must get an i32 back, not a Promise.
 * 2. Async callbacks run on main's stack. Every Emscripten callback routed
 *    through callUserCallback (emscripten_fetch, emscripten_async_call /
 *    set_timeout, SOCKFS) is posted to Qt's QWasmSuspendResumeControl queue
 *    and executed by browser/jspi_main_calls.cpp inside processEvents — the
 *    same path Qt uses for DOM events and timers. Those callbacks may then do
 *    qtclient-fs I/O like any other Qt event handler.
 * 3. `Module.qtclientJspi.parked` counts suspended activations. qtclient-fs
 *    aborts (with the JS + wasm stack) when called while main is parked: that
 *    is a raw JS→wasm re-entry the runtime does not route through Qt yet.
 */
(function () {
  if (typeof ENVIRONMENT_IS_PTHREAD !== 'undefined' && ENVIRONMENT_IS_PTHREAD) return;

  var state = { parked: 0, mainCallHandler: 0, posted: 0 };
  Module.qtclientJspi = state;

  // --- 1. raw table entries ------------------------------------------------
  var capture = null;
  getWasmTableEntry = function (funcPtr) {
    if (capture) {
      return function () {
        capture.push({ fp: funcPtr, args: Array.prototype.slice.call(arguments) });
      };
    }
    var func = wasmTableMirror[funcPtr];
    if (!func) {
      func = wasmTable.get(funcPtr);
      wasmTableMirror[funcPtr] = func;
    }
    return func;
  };

  // --- 3. suspension accounting -------------------------------------------
  // Every EM_ASYNC_JS / emscripten_sleep import goes through handleAsync.
  var handleAsync = Asyncify.handleAsync;
  Asyncify.handleAsync = async function (startAsync) {
    state.parked++;
    try {
      return await handleAsync.call(Asyncify, startAsync);
    } finally {
      state.parked--;
    }
  };

  // --- 2. async callbacks → Qt event queue --------------------------------
  // emscripten_fetch writes readyState / status / byte counts / data into the
  // shared emscripten_fetch_t right before each callback. A deferred callback
  // must see the values of its own event, and must not run once the fetch is
  // closed (struct freed). jspi_main_calls.cpp publishes the struct layout.
  function tagFetchCall(call) {
    var L = state.fetchLayout;
    if (!L || call.args.length !== 1 || typeof Fetch === 'undefined' || !Fetch.xhrs) return;
    var p = call.args[0];
    if (!p || p % 4 !== 0 || p + L.attr + L.attrSize > HEAPU8.length) return;
    var id = HEAPU32[p >> 2];
    if (!Fetch.xhrs.has(id)) return;
    var attr = p + L.attr;
    var slot = -1;
    L.callbacks.forEach(function (off, k) { if (HEAPU32[(attr + off) >> 2] === call.fp) slot = k; });
    if (slot < 0) return;
    call.fetchId = id;
    // onsuccess / onerror (L.callbacks order) end the fetch: see jspi_main_calls.cpp.
    call.fetchTerminal = slot < 2;
    call.xhr = Fetch.xhrs.get(id);
    call.snapshot = HEAPU8.slice(p + L.data, p + L.attr);
  }
  state.fetchAlive = function (call) {
    return Fetch.xhrs.has(call.fetchId) && Fetch.xhrs.get(call.fetchId) === call.xhr;
  };

  function postMainCall(call, freePtr) {
    for (var i = 0; i < call.args.length; i++) {
      var a = call.args[i];
      if (typeof a !== 'number' || a !== Math.floor(a)) {
        abort('qtclient jspi: callback ' + call.fp + ' has a non-i32 argument; ' +
              'add its signature to browser/jspi_main_calls.cpp');
      }
    }
    if (call.args.length > 4) {
      abort('qtclient jspi: callback ' + call.fp + ' takes ' + call.args.length +
            ' arguments; browser/jspi_main_calls.cpp supports up to 4');
    }
    var control = Module.qtSuspendResumeControl;
    var handler = control && control.eventHandlers[state.mainCallHandler];
    if (!handler) abort('qtclient jspi: main-call handler is not registered');
    tagFetchCall(call);
    call.free = freePtr | 0;
    state.posted++;
    handler(call);
  }

  var callUserCallbackDirect = callUserCallback;
  callUserCallback = function (func) {
    // Before QCoreApplication exists there is no Qt event loop to post to and
    // main is not parked in it; Emscripten's direct call is the only flow.
    // The pthread mailbox export is not a table callback (proxied calls).
    if (!state.mainCallHandler || func === __emscripten_check_mailbox) {
      callUserCallbackDirect(func);
      return;
    }
    callUserCallbackDirect(function () {
      var calls = [];
      capture = calls;
      try {
        func();
      } finally {
        capture = null;
      }
      for (var i = 0; i < calls.length; i++) postMainCall(calls[i], 0);
    });
  };

  // SOCKFS error callbacks pass a stack-allocated message that is popped as
  // soon as the JS closure returns. Hand the C++ side a heap copy it frees.
  // (tibia's WasmFS build has no SOCKFS; only the launcher links it.)
  if (typeof SOCKFS === 'undefined') return;
  _setNetworkCallback = function (event, userData, callback) {
    runtimeKeepalivePush();
    if (!callback) {
      SOCKFS.on(event, null);
      return;
    }
    SOCKFS.on(event, function (data) {
      if (!state.mainCallHandler) {
        abort('qtclient jspi: SOCKFS ' + event + ' before the Qt event loop exists');
      }
      if (event === 'error') {
        var msg = stringToNewUTF8(String(data[2]));
        postMainCall({ fp: callback, args: [data[0], data[1], msg, userData] }, msg);
      } else {
        postMainCall({ fp: callback, args: [data, userData] }, 0);
      }
    });
  };
})();
