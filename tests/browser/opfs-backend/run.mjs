// Serve the built contract test with cross-origin isolation (pthreads need
// SharedArrayBuffer), open it in a headless browser and report the result.
// Usage: node run.mjs [outDir] [chromium|webkit]
//   (needs the `playwright` package + that browser). WebKit is Safari's engine:
//   it settles a Worker's async OPFS requests only while the page's main thread
//   is free, which the blocking-join test depends on.
import { createServer } from 'node:http';
import { mkdtemp, readFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { extname, join, resolve } from 'node:path';
import { chromium, webkit } from 'playwright';

const dir = resolve(process.argv[2] ?? new URL('./out', import.meta.url).pathname);
const types = { '.html': 'text/html', '.js': 'text/javascript', '.wasm': 'application/wasm' };
const server = createServer(async (req, res) => {
  try {
    const file = join(dir, new URL(req.url, 'http://x').pathname.replace(/^\/$/, '/index.html'));
    const body = await readFile(file);
    res.writeHead(200, {
      'Content-Type': types[extname(file)] ?? 'application/octet-stream',
      'Cross-Origin-Opener-Policy': 'same-origin',
      'Cross-Origin-Embedder-Policy': 'require-corp',
      'Cache-Control': 'no-store',
    });
    res.end(body);
  } catch {
    res.writeHead(404).end();
  }
});
await new Promise((r) => server.listen(0, '127.0.0.1', r));
const url = `http://127.0.0.1:${server.address().port}/`;

const engine = process.argv[3] ?? 'chromium';
// JSPI is on by default from Chromium 137; the flag covers older builds.
// WebKit's default ephemeral context has no OPFS: use a fresh persistent one.
const browser =
  engine === 'webkit'
    ? await webkit.launchPersistentContext(await mkdtemp(join(tmpdir(), 'opfs-contract-')))
    : await chromium.launch({ args: ['--js-flags=--experimental-wasm-jspi'] });
console.log(`[opfs-contract] ${engine}`);
let failures = 1;
try {
  const page = (engine === 'webkit' && browser.pages()[0]) || (await browser.newPage());
  page.on('console', (m) => console.log(`[page] ${m.text()}`));
  page.on('pageerror', (e) => console.log(`[pageerror] ${e.message}`));
  await page.goto(url);
  const result = await page.waitForFunction(() => globalThis.qtclientFsContract, null, { timeout: 120_000 });
  const { failures: f, log } = await result.jsonValue();
  failures = f;
  console.log(log);
} finally {
  await browser.close();
  server.close();
}
process.exit(failures ? 1 : 0);
