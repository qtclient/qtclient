/**
 * After C roundtrip, dump OPFS layout for portal writer alignment.
 * Expected: mount /home/web_user ⇒ OPFS paths are relative to origin root
 * (e.g. `.local/share/IdleOT/...`), NOT under idleot-cip-home/.
 */
Module['onExit'] = function (code) {
  (async function () {
    async function listOpfs(dir, prefix) {
      const out = [];
      for await (const [name, handle] of dir.entries()) {
        const path = prefix ? prefix + '/' + name : name;
        if (handle.kind === 'directory') {
          out.push({ path: path, kind: 'dir' });
          out.push.apply(out, await listOpfs(handle, path));
        } else {
          const file = await handle.getFile();
          out.push({ path: path, kind: 'file', size: file.size });
        }
      }
      return out;
    }
    try {
      const root = await navigator.storage.getDirectory();
      const entries = await listOpfs(root, '');
      const expected =
        '.local/share/IdleOT/IdleOT/packages/IdleOT.app/wasmfs-opfs-harness.txt';
      const hit = entries.find(function (e) {
        return e.path === expected;
      });
      const report = {
        exitCode: code,
        layout:
          'WasmFS OPFS backend root = navigator.storage.getDirectory(); ' +
          'VFS /home/web_user/<rel> ↔ OPFS <rel> (no idleot-cip-home prefix).',
        expectedRel: expected,
        found: !!hit,
        entries: entries,
      };
      console.log('[wasmfs-opfs-harness]', JSON.stringify(report, null, 2));
      if (typeof window !== 'undefined') window.__WASMFS_OPFS_LAYOUT__ = report;
    } catch (err) {
      console.error('[wasmfs-opfs-harness] OPFS list failed', err);
    }
  })();
};
