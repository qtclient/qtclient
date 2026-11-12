/**
 * JSPI qtclient-fs post-js (SoT with portal patchJspiPromisingWrap):
 *
 * Qt app.exec() stays on promising(main); qtclientFs EM_ASYNC_JS suspends there.
 * NEVER promising-wrap via getWasmTableEntry / force isAsyncExport≡true —
 * invoke_* and Qt EH call table entries as sync wasm funcs; a cached
 * promising wrapper returns a Promise instead of an i32 → OOB / null function
 * after the login mask.
 *
 * This file: (1) force raw getWasmTableEntry, (2) isAsyncExport≡false,
 * (3) safePromising for export-time makeAsyncFunction only.
 */
(function () {
  function safePromising(fn) {
    if (!fn || fn.__idleotPromising) return fn;
    try {
      if (
        typeof WebAssembly.Function === 'function' &&
        !(fn instanceof WebAssembly.Function)
      ) {
        return fn;
      }
      var f = WebAssembly.promising(fn);
      try {
        f.__idleotPromising = 1;
      } catch (_) {
        /* ignore */
      }
      return f;
    } catch (_) {
      return fn;
    }
  }

  function rawTable() {
    if (typeof getWasmTableEntry !== 'function' || typeof wasmTable === 'undefined')
      return;
    if (getWasmTableEntry.__idleotRawTable) return;
    getWasmTableEntry = function (funcPtr) {
      var func = wasmTableMirror[funcPtr];
      if (!func) {
        func = wasmTable.get(funcPtr);
        wasmTableMirror[funcPtr] = func;
      }
      return func;
    };
    getWasmTableEntry.__idleotRawTable = 1;
  }

  function patch(Asyncify) {
    if (!Asyncify || Asyncify.__idleotJspiTableWrap) return;
    Asyncify.__idleotJspiTableWrap = 1;
    Asyncify.isAsyncExport = function () {
      return false;
    };
    Asyncify.makeAsyncFunction = function (original) {
      return safePromising(original);
    };
  }

  if (typeof Asyncify !== 'undefined') patch(Asyncify);
  rawTable();
  if (typeof Module !== 'undefined') {
    var prev = Module.preRun;
    Module.preRun = [].concat(prev || []).concat(function () {
      if (typeof Asyncify !== 'undefined') patch(Asyncify);
      rawTable();
    });
  }
})();
