/**
 * JSPI: await __wasm_call_ctors under WebAssembly.promising.
 *
 * Stock glue calls the export bare from initRuntime(). With EM_ASYNC_JS
 * tip-fs, a suspending ctor makes that export suspending → Safari
 * SuspendError. Table wraps do not cover wasmExports ctors.
 *
 * Portal patchJspiPromisingWrap rewrites the same sites for /play. This
 * post-js keeps tip tibia.js self-contained after a clean wasm configure
 * that adds --post-js=jspi-await-ctors.js.
 */
(function () {
  if (typeof initRuntime !== 'function' || initRuntime.__idleotJspiCtors) return;
  if (typeof run !== 'function') return;

  function promisingCtors(c) {
    if (!c) return async function () {};
    if (c.__idleotPromising) return c;
    try {
      var wrapped = WebAssembly.promising(c);
      try {
        wrapped.__idleotPromising = 1;
      } catch (_) {
        /* ignore */
      }
      return wrapped;
    } catch (_) {
      var fallback = async function () {
        return await c();
      };
      fallback.__idleotPromising = 1;
      return fallback;
    }
  }

  initRuntime = async function () {
    runtimeInitialized = true;
    if (typeof ENVIRONMENT_IS_PTHREAD !== 'undefined' && ENVIRONMENT_IS_PTHREAD)
      return startWorker(Module);
    var c =
      typeof wasmExports !== 'undefined' && wasmExports
        ? wasmExports['__wasm_call_ctors']
        : null;
    var p = promisingCtors(c);
    if (c && wasmExports) wasmExports['__wasm_call_ctors'] = p;
    await p();
  };
  initRuntime.__idleotJspiCtors = 1;

  run = function (args) {
    if (args === undefined) args = arguments_;
    if (runDependencies > 0) {
      dependenciesFulfilled = run;
      return;
    }
    if (typeof ENVIRONMENT_IS_PTHREAD !== 'undefined' && ENVIRONMENT_IS_PTHREAD) {
      readyPromiseResolve(Module);
      void initRuntime();
      return;
    }
    preRun();
    if (runDependencies > 0) {
      dependenciesFulfilled = run;
      return;
    }
    async function doRun() {
      Module['calledRun'] = true;
      if (ABORT) return;
      await initRuntime();
      preMain();
      readyPromiseResolve(Module);
      if (Module['onRuntimeInitialized']) Module['onRuntimeInitialized']();
      var noInitialRun = Module['noInitialRun'] || false;
      if (!noInitialRun) await callMain(args);
      postRun();
    }
    if (Module['setStatus']) {
      Module['setStatus']('Running...');
      setTimeout(function () {
        setTimeout(function () {
          Module['setStatus']('');
        }, 1);
        void doRun();
      }, 1);
    } else {
      void doRun();
    }
  };
  run.__idleotJspiCtors = 1;
})();
