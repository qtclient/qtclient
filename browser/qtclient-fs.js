/**
 * qtclient-fs: the OPFS Worker behind browser/qtclient_opfs_backend.cpp.
 * Shared --post-js of tibia and launcher.
 *
 * The engine starts its own Worker (from workerMain below) when main() mounts
 * OPFS at /home/web_user, so the backend and the Worker always come from the
 * same build. The Worker is the engine's only OPFS client: it owns every
 * SyncAccessHandle and serves the backend's requests from a block in the wasm
 * heap (one slot per calling thread).
 *
 *  - sync requests (read / write / size / truncate on an open access handle,
 *    handle bookkeeping) are served inline: SyncAccessHandle I/O completes
 *    without the page's main thread, so the caller may block on it;
 *  - async requests (lookups, create, list, remove, move, open) are
 *    FileSystemHandle promises, which Safari only settles while the page's
 *    main thread is free, so a main-thread caller suspends (JSPI) instead.
 *
 * Wake-ups are messages, i.e. tasks, in both directions: the main thread posts
 * `ring`, a pthread rings a BroadcastChannel, and the Worker answers the main
 * thread with `main-done`. Safari leaves an Atomics.waitAsync resolution queued
 * until some unrelated task wakes the event loop (seconds on an idle page).
 * Atomics only carry the slot state and wake a pthread's blocking futex wait.
 *
 * Memory model (like a native fs): bytes stay in OPFS. Reads and writes go
 * straight between the access handle and the wasm heap; nothing is cached.
 * Access handles (OS handles, no file contents) stay open in a small LRU after
 * their last close so reopen is cheap.
 */
(function () {
  if (typeof ENVIRONMENT_IS_PTHREAD !== 'undefined' && ENVIRONMENT_IS_PTHREAD) return;

  // Runs in the Worker, from its own source text (no file to fetch).
  function workerMain() {
    'use strict';
    var MAIN_SLOT = 0;
    // Every SyncAccessHandle is an OS file handle in the browser's storage
    // process. Safari fails createSyncAccessHandle with "Invalid platform file
    // handle" past ~250 open handles (measured), so the Worker caps them like a
    // process fd limit: idle ones are closed first, and an open fails with
    // EMFILE when all are in use.
    var MAX_OPEN_HANDLES = 128;
    // Of those, handles kept open after their last close (cheap reopen for wasm).
    var IDLE_HANDLES_MAX = 16;
    // A just-closed /play document's Worker releases its access handles
    // asynchronously; wait that long for a lock before reporting a conflict.
    var LOCK_WAIT_MS = 4000;

    // Wasm op codes (qtclient_opfs_backend.cpp enum Op).
    var OP = {
      CHILD: 1, INSERT_FILE: 2, INSERT_DIR: 3, REMOVE: 4, ENTRIES: 5, MOVE: 6,
      OPEN: 7, CLOSE: 8, READ: 9, WRITE: 10, SIZE_ACCESS: 11, TRUNC_ACCESS: 12,
      SIZE_FILE: 14, TRUNC_FILE: 15, FREE_NODE: 16,
    };
    var OP_NAME = {};
    Object.keys(OP).forEach(function (k) { OP_NAME[OP[k]] = k; });
    var SYNC_OPS = new Set([OP.CLOSE, OP.READ, OP.WRITE, OP.SIZE_ACCESS, OP.TRUNC_ACCESS, OP.FREE_NODE]);

    var root = null;

    // ---- wasm channel -----------------------------------------------------------
    var memory = null;   // shared WebAssembly.Memory (may grow: launcher)
    var L = null;        // byte offsets from attach
    var ERR = null;      // errno values from attach
    var I32 = null, F64 = null, U8 = null;

    function views() {
      var buf = memory.buffer;
      if (U8 && U8.length === buf.byteLength) return;
      I32 = new Int32Array(buf);
      F64 = new Float64Array(buf);
      U8 = new Uint8Array(buf);
    }
    function slotState(s) { return (L.slots + s * L.stride + L.state) >> 2; }
    function slotOp(s) { return (L.slots + s * L.stride + L.op) >> 2; }
    function arg(s, i) { return F64[((L.slots + s * L.stride + L.args) >> 3) + i]; }
    function readName(s, iPtr, iLen) {
      var p = arg(s, iPtr) >>> 0, n = arg(s, iLen) >>> 0;
      return new TextDecoder().decode(U8.slice(p, p + n));
    }
    function finish(s, r0, r1) {
      views();
      var base = L.slots + s * L.stride;
      F64[(base + L.r0) >> 3] = r0;
      F64[(base + L.r1) >> 3] = r1 || 0;
      Atomics.store(I32, slotState(s), 3);
      // Slot 0 is the wasm main thread: it waits for this message (a task, so
      // Safari runs it at once). Pthreads block in futex_wait on the state word.
      if (s === MAIN_SLOT) self.postMessage({ type: 'main-done' });
      else Atomics.notify(I32, slotState(s));
    }

    // ---- node / handle tables ---------------------------------------------------
    /** id → { kind: 'file'|'dir', handle, path } ; path is OPFS-relative ('' = root). */
    var nodes = new Map();
    var nextNode = 2;
    /** OPFS-relative path → { id, path, sah, refs } */
    var accessByPath = new Map();
    var accessById = new Map();
    var nextAccess = 1;
    /** paths whose access handle has refs == 0, least recently used first */
    var idle = [];
    /** path → Promise<access rec> while createSyncAccessHandle is in flight */
    var opening = new Map();
    /**
     * POSIX unlink of an open file: the name goes at once, open descriptors keep
     * working and the data goes with the last close. OPFS cannot remove an entry
     * while its access handle is open, so the entry is hidden until then.
     * path → { dir, name } (unlinked, still open); path → Promise (removing).
     */
    var unlinked = new Map();
    var removing = new Map();
    function removalsSettled() {
      return removing.size ? Promise.all(Array.from(removing.values())) : null;
    }

    function join(dir, name) { return dir ? dir + '/' + name : name; }
    function under(path, prefix) { return path === prefix || path.indexOf(prefix + '/') === 0; }
    function node(id) {
      var n = nodes.get(id);
      if (!n) throw fsError('BADF', 'stale node ' + id);
      return n;
    }
    /** OPFS-relative path → live node count (WasmFS holds a File for it). */
    var nodesAtPath = new Map();
    function countPath(path, delta) {
      var n = (nodesAtPath.get(path) || 0) + delta;
      if (n > 0) nodesAtPath.set(path, n); else nodesAtPath.delete(path);
    }
    function addNode(kind, handle, path) {
      var id = nextNode++;
      nodes.set(id, { kind: kind, handle: handle, path: path });
      countPath(path, 1);
      return id;
    }
    function freeNode(id) {
      var n = nodes.get(id);
      if (!n) return;
      nodes.delete(id);
      countPath(n.path, -1);
    }

    function fsError(code, message) {
      var e = new Error(message || code);
      e.code = code;
      return e;
    }
    /** Our own fsError code (string); DOMException.code is a legacy number. */
    function ownCode(e) {
      return e && typeof e.code === 'string' ? e.code : null;
    }
    function errnoOf(e) {
      var code = ownCode(e);
      if (!code) {
        switch (e && e.name) {
        case 'NotFoundError': code = 'NOENT'; break;
        case 'TypeMismatchError': code = 'NOTDIR'; break;
        case 'NoModificationAllowedError': code = 'BUSY'; break;
        case 'InvalidStateError': code = 'BUSY'; break;
        case 'InvalidModificationError': code = 'NOTEMPTY'; break;
        case 'QuotaExceededError': code = 'NOSPC'; break;
        case 'TypeError': code = 'INVAL'; break;
        default: code = 'IO';
        }
      }
      if (code === 'IO') log('unexpected OPFS error', e);
      if (!ERR) return -1;
      return -(ERR[code] || ERR.IO);
    }
    function log(what, e) {
      self.postMessage({ type: 'log', message: what + ': ' + (e && e.name ? e.name + ': ' + e.message : String(e)) });
    }

    function untouch(path) {
      var i = idle.indexOf(path);
      if (i >= 0) idle.splice(i, 1);
    }
    function closeRec(rec) {
      accessByPath.delete(rec.path);
      accessById.delete(rec.id);
      untouch(rec.path);
      // close() persists written data (no separate flush: see OpfsFile::flush).
      rec.sah.close();
      releaseHandle();
    }

    /** Access handles open or being opened; see MAX_OPEN_HANDLES. */
    var handlesInUse = 0;
    function releaseHandle() {
      handlesInUse--;
    }
    /**
     * Take one access-handle slot, closing idle handles first. All in use:
     * EMFILE, like a process at its fd limit (waiting could deadlock a caller
     * that holds the handles itself).
     */
    function reserveHandle() {
      while (handlesInUse >= MAX_OPEN_HANDLES && idle.length) {
        var rec = accessByPath.get(idle[0]);
        if (rec) closeRec(rec); else idle.shift();
      }
      if (handlesInUse >= MAX_OPEN_HANDLES)
        throw fsError('MFILE', MAX_OPEN_HANDLES + ' access handles open');
      handlesInUse++;
    }
    function evictIdle(max) {
      while (idle.length > max) {
        var rec = accessByPath.get(idle[0]);
        if (rec) closeRec(rec); else idle.shift();
      }
    }
    /** Close idle handles under path; EBUSY if one is open by a caller. */
    function releaseHandlesUnder(path) {
      Array.from(accessByPath.values()).forEach(function (rec) {
        if (!under(rec.path, path)) return;
        if (rec.refs > 0) throw fsError('BUSY', rec.path + ' is open');
        closeRec(rec);
      });
    }
    function renamePaths(from, to) {
      nodes.forEach(function (n) {
        if (!under(n.path, from)) return;
        countPath(n.path, -1);
        n.path = to + n.path.slice(from.length);
        countPath(n.path, 1);
      });
    }

    function parentOf(path) { var i = path.lastIndexOf('/'); return i < 0 ? '' : path.slice(0, i); }
    function baseName(path) { return path.slice(path.lastIndexOf('/') + 1); }
    /** Directory handle for an OPFS-relative path ('' = root). */
    async function dirAt(path) {
      var dir = root;
      var parts = path ? path.split('/') : [];
      for (var i = 0; i < parts.length; i++) dir = await dir.getDirectoryHandle(parts[i]);
      return dir;
    }
    async function moveDirectory(srcParent, srcName, dstParent, dstName) {
      var src = await srcParent.getDirectoryHandle(srcName);
      var dst = await dstParent.getDirectoryHandle(dstName, { create: true });
      var entries = [];
      for await (var entry of src.entries()) entries.push(entry);
      for (var i = 0; i < entries.length; i++) {
        if (entries[i][1].kind === 'file') await entries[i][1].move(dst, entries[i][0]);
        else await moveDirectory(src, entries[i][0], dst, entries[i][0]);
      }
      await srcParent.removeEntry(srcName);
    }

    async function openAccess(path, fileHandle) {
      var rec = accessByPath.get(path);
      if (rec) {
        untouch(path);
        rec.refs++;
        return rec;
      }
      var pending = opening.get(path);
      if (!pending) {
        reserveHandle();
        // Another caller may have opened or started opening it meanwhile.
        rec = accessByPath.get(path) || null;
        pending = opening.get(path);
        if (rec || pending) releaseHandle();
        if (rec) {
          untouch(path);
          rec.refs++;
          return rec;
        }
      }
      if (!pending) {
        pending = (async function () {
          var sah = await createAccessHandle(path, fileHandle);
          var r = { id: nextAccess++, path: path, sah: sah, refs: 0 };
          accessByPath.set(path, r);
          accessById.set(r.id, r);
          return r;
        })();
        opening.set(path, pending);
        pending.then(function () { opening.delete(path); }, function () {
          opening.delete(path);
          releaseHandle();
        });
      }
      rec = await pending;
      untouch(path);
      rec.refs++;
      return rec;
    }
    function isLockConflict(e) {
      return e && (e.name === 'NoModificationAllowedError' || e.name === 'InvalidStateError');
    }
    async function createAccessHandle(path, fileHandle) {
      var t0 = performance.now();
      for (var delay = 10;; delay = Math.min(delay * 2, 250)) {
        try {
          return await fileHandle.createSyncAccessHandle();
        } catch (e) {
          if (!isLockConflict(e)) throw e;
          if (performance.now() - t0 > LOCK_WAIT_MS) {
            throw fsError('BUSY', path + ' is locked by another /play tab or window (' + e.name + ')');
          }
        }
        await new Promise(function (r) { setTimeout(r, delay); });
      }
    }
    /** keepIdle: leave the handle open for a cheap reopen (wasm files). */
    function closeAccess(rec, keepIdle) {
      if (--rec.refs > 0) return;
      var gone = unlinked.get(rec.path);
      if (gone) {
        // Last close of an unlinked file: now the entry can go.
        closeRec(rec);
        unlinked.delete(rec.path);
        var path = rec.path, removal = gone.dir.removeEntry(gone.name).catch(function () {});
        removing.set(path, removal);
        removal.then(function () { if (removing.get(path) === removal) removing.delete(path); });
        return;
      }
      if (!keepIdle) {
        closeRec(rec);
        return;
      }
      idle.push(rec.path);
      evictIdle(IDLE_HANDLES_MAX);
    }
    function accessOf(id) {
      var rec = accessById.get(id);
      if (!rec) throw fsError('BADF', 'access ' + id);
      return rec;
    }


    // ---- wasm ops ---------------------------------------------------------------
    var stats = { calls: 0, ms: 0, bytesRead: 0, bytesWritten: 0, byOp: {} };
    function count(op, ms) {
      stats.calls++;
      stats.ms += ms;
      var o = stats.byOp[op] || (stats.byOp[op] = { calls: 0, ms: 0 });
      o.calls++;
      o.ms += ms;
    }

    function runSync(op, s) {
      switch (op) {
      case OP.CLOSE: closeAccess(accessOf(arg(s, 0)), true); return [0];
      case OP.READ: {
        var rec = accessOf(arg(s, 0)), p = arg(s, 1) >>> 0, n = arg(s, 2);
        var got = n ? rec.sah.read(U8.subarray(p, p + n), { at: arg(s, 3) }) : 0;
        stats.bytesRead += got;
        return [got];
      }
      case OP.WRITE: {
        var wrec = accessOf(arg(s, 0)), wp = arg(s, 1) >>> 0, wn = arg(s, 2);
        var put = wn ? wrec.sah.write(U8.subarray(wp, wp + wn), { at: arg(s, 3) }) : 0;
        stats.bytesWritten += put;
        return [put];
      }
      case OP.SIZE_ACCESS: return [accessOf(arg(s, 0)).sah.getSize()];
      case OP.TRUNC_ACCESS: accessOf(arg(s, 0)).sah.truncate(arg(s, 1)); return [0];
      case OP.FREE_NODE: freeNode(arg(s, 0)); return [0];
      }
      throw fsError('NOSYS', 'op ' + op);
    }

    async function runAsync(op, s) {
      switch (op) {
      case OP.CHILD: {
        var dir = node(arg(s, 0)), name = readName(s, 1, 2), path = join(dir.path, name);
        if (unlinked.has(path)) throw fsError('NOENT', path + ' is unlinked');
        await removalsSettled();
        try {
          return [1, addNode('file', await dir.handle.getFileHandle(name), path)];
        } catch (e) {
          if (e.name !== 'TypeMismatchError') throw e;
        }
        return [2, addNode('dir', await dir.handle.getDirectoryHandle(name), path)];
      }
      case OP.INSERT_FILE: {
        var fdir = node(arg(s, 0)), fname = readName(s, 1, 2);
        var fpath = join(fdir.path, fname);
        // The unlinked file still owns that entry until its last close.
        if (unlinked.has(fpath)) throw fsError('BUSY', fpath + ' is unlinked but still open');
        await removalsSettled();
        var fh = await fdir.handle.getFileHandle(fname, { create: true });
        var fid = addNode('file', fh, fpath);
        // WasmFS opens a file right after creating it: hand back an open access
        // handle with the create so that open needs no second round trip. At
        // the handle cap the open itself reports EMFILE.
        try {
          return [fid, (await openAccess(fpath, fh)).id];
        } catch (e) {
          if (ownCode(e) !== 'MFILE') throw e;
          return [fid, 0];
        }
      }
      case OP.INSERT_DIR: {
        var ddir = node(arg(s, 0)), dname = readName(s, 1, 2);
        var dh = await ddir.handle.getDirectoryHandle(dname, { create: true });
        return [addNode('dir', dh, join(ddir.path, dname))];
      }
      case OP.REMOVE: {
        var rdir = node(arg(s, 0)), rname = readName(s, 1, 2), rpath = join(rdir.path, rname);
        var openRec = accessByPath.get(rpath);
        if (openRec && openRec.refs > 0) {
          unlinked.set(rpath, { dir: rdir.handle, name: rname });
          return [0];
        }
        releaseHandlesUnder(rpath);
        await rdir.handle.removeEntry(rname);
        return [0];
      }
      case OP.ENTRIES: {
        var edir = node(arg(s, 0)), buf = arg(s, 1) >>> 0, cap = arg(s, 2);
        var enc = new TextEncoder(), parts = [], bytes = 0;
        await removalsSettled();
        for await (var entry of edir.handle.entries()) {
          if (unlinked.has(join(edir.path, entry[0]))) continue;
          var nb = enc.encode(entry[0]);
          parts.push([entry[1].kind === 'directory' ? 2 : 1, nb]);
          bytes += 3 + nb.length;
        }
        if (bytes > cap) return [-ERR.RANGE, bytes];
        views();
        var at = buf;
        parts.forEach(function (p) {
          U8[at] = p[0];
          U8[at + 1] = p[1].length & 255;
          U8[at + 2] = p[1].length >> 8;
          U8.set(p[1], at + 3);
          at += 3 + p[1].length;
        });
        return [parts.length, bytes];
      }
      case OP.MOVE: {
        var moved = node(arg(s, 0)), target = node(arg(s, 1)), mname = readName(s, 2, 3);
        var from = moved.path, to = join(target.path, mname);
        if (from === to) return [0];
        releaseHandlesUnder(from);
        // WasmFS resolves the target before a rename, so an existing target always
        // has a live node here; no node means nothing to replace.
        if (nodesAtPath.has(to)) {
          releaseHandlesUnder(to);
          // WasmFS already checked the replace rules (type match, empty dir).
          try {
            await target.handle.removeEntry(mname);
          } catch (e) {
            if (e.name !== 'NotFoundError') throw e;
          }
        }
        if (moved.kind === 'file') {
          await moved.handle.move(target.handle, mname);
          renamePaths(from, to);
          return [0];
        }
        // Directory handles have no move(): move the tree file by file.
        await moveDirectory(await dirAt(parentOf(from)), baseName(from), target.handle, mname);
        renamePaths(from, to);
        // Handles still name the old location: re-resolve the moved tree.
        var stale = [];
        nodes.forEach(function (n) { if (under(n.path, to)) stale.push(n); });
        for (var i = 0; i < stale.length; i++) {
          var p = stale[i].path, parent = await dirAt(parentOf(p));
          stale[i].handle = stale[i].kind === 'dir'
            ? await parent.getDirectoryHandle(baseName(p))
            : await parent.getFileHandle(baseName(p));
        }
        return [0];
      }
      case OP.OPEN: {
        var file = node(arg(s, 0));
        return [(await openAccess(file.path, file.handle)).id];
      }
      case OP.SIZE_FILE: {
        var sfile = node(arg(s, 0)), open = accessByPath.get(sfile.path);
        if (open) return [open.sah.getSize()];
        return [(await sfile.handle.getFile()).size];
      }
      case OP.TRUNC_FILE: {
        var tfile = node(arg(s, 0)), trec = await openAccess(tfile.path, tfile.handle);
        try {
          trec.sah.truncate(arg(s, 1));
        } finally {
          closeAccess(trec, true);
        }
        return [0];
      }
      }
      throw fsError('NOSYS', 'op ' + op);
    }

    function serveSlots() {
      views();
      for (var s = 0; s < L.count; s++) {
        if (Atomics.load(I32, slotState(s)) !== 1) continue;
        Atomics.store(I32, slotState(s), 2);
        var op = I32[slotOp(s)];
        if (SYNC_OPS.has(op)) {
          var t0 = performance.now();
          var r;
          try {
            r = runSync(op, s);
          } catch (e) {
            r = [errnoOf(e)];
          }
          count(OP_NAME[op], performance.now() - t0);
          finish(s, r[0], r[1]);
        } else {
          startAsync(op, s);
        }
      }
    }
    function startAsync(op, s) {
      var t0 = performance.now();
      runAsync(op, s).catch(function (e) {
        return [errnoOf(e)];
      }).then(function (r) {
        count(OP_NAME[op], performance.now() - t0);
        finish(s, r[0], r[1]);
      });
    }

    // ---- messages -------------------------------------------------------------------
    // Every wake-up is a message, i.e. a task: `ring` from the wasm main thread,
    // the ring BroadcastChannel from pthreads. Safari leaves an Atomics.waitAsync
    // resolution queued until some unrelated task wakes the event loop, so none is
    // used; Atomics only carry the slot state (and wake a pthread's blocking wait).
    self.onmessage = function (ev) {
      var m = ev.data || {};
      if (m.type === 'ring') {
        if (L) serveSlots();
        return;
      }
      if (m.type === 'start') {
        navigator.storage.getDirectory().then(function (dir) {
          root = dir;
          memory = m.memory;
          L = m.layout;
          ERR = m.errno;
          views();
          nodes.set(1, { kind: 'dir', handle: root, path: '' });
          countPath('', 1);
          var ring = new BroadcastChannel(m.channel);
          ring.onmessage = function () { serveSlots(); };
          self.postMessage({ type: 'started' });
        }, function (e) {
          self.postMessage({ type: 'start-failed', message: String(e && e.message || e) });
        });
        return;
      }
      if (m.type === 'stats') {
        self.postMessage({ type: 'stats', id: m.id, stats: stats, openHandles: accessByPath.size,
          idleHandles: idle.length, nodes: nodes.size });
      }
    };
  }

  var worker = null;
  var mainWake = null;
  var statsWaiters = new Map();
  var nextStats = 1;

  Module.qtclientFs = {
    /** Start the Worker; called once by qtclientMountOpfs (EM_ASYNC_JS). */
    start: function (layout, errno, channel) {
      return new Promise(function (resolve, reject) {
        var source = '(' + workerMain.toString() + ')();';
        var url = URL.createObjectURL(new Blob([source], { type: 'text/javascript' }));
        worker = new Worker(url, { name: 'qtclient-fs' });
        URL.revokeObjectURL(url);
        worker.onerror = function (e) {
          reject(new Error('qtclient-fs worker: ' + (e && e.message || 'failed to start')));
        };
        worker.onmessage = function (ev) {
          var m = ev.data;
          if (m.type === 'main-done') {
            var wake = mainWake;
            mainWake = null;
            if (wake) wake();
          } else if (m.type === 'started') {
            resolve();
          } else if (m.type === 'start-failed') {
            reject(new Error('qtclient-fs worker: ' + m.message));
          } else if (m.type === 'log') {
            err('qtclient-fs: ' + m.message);
          } else if (m.type === 'stats') {
            var done = statsWaiters.get(m.id);
            statsWaiters.delete(m.id);
            if (done) done(m);
          }
        };
        worker.postMessage({ type: 'start', memory: wasmMemory, layout: layout, errno: errno, channel: channel });
      });
    },
    /** Wake the Worker for a request the main thread just posted. */
    ring: function () {
      worker.postMessage({ type: 'ring' });
    },
    /** Resolves on the Worker's next `main-done` (main-slot request finished). */
    mainDone: function () {
      return new Promise(function (resolve) { mainWake = resolve; });
    },
    /** The Worker's counters, for /play diagnostics. */
    stats: function () {
      return new Promise(function (resolve) {
        var id = nextStats++;
        statsWaiters.set(id, resolve);
        worker.postMessage({ type: 'stats', id: id });
      });
    },
  };

  // A Worker blocked on OPFS keeps its access handles past a reload; end it
  // with the page so the next document can open them.
  addEventListener('pagehide', function (ev) {
    if (!ev.persisted && worker) worker.terminate();
  });
})();
