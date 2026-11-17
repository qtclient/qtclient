// Serve the built contract test with cross-origin isolation (pthreads need
// SharedArrayBuffer), open it in headless Chromium and report the result.
// Usage: node run.mjs [outDir]   (needs the `playwright` package + Chromium)
import { createServer } from 'node:http';
import { readFile } from 'node:fs/promises';
import { extname, join, resolve } from 'node:path';
import { chromium } from 'playwright';

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

// JSPI is on by default from Chromium 137; the flag covers older builds.
const browser = await chromium.launch({ args: ['--js-flags=--experimental-wasm-jspi'] });
let failures = 1;
try {
  const page = await browser.newPage();
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
