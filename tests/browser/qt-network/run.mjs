// Serve the built Qt network contract test and its endpoints on one origin
// (cross-origin isolated: pthreads need SharedArrayBuffer), open it in headless
// Chromium and report the result.
// Usage: node run.mjs [outDir]   (needs the `playwright` and `ws` packages + Chromium)
import { createServer } from 'node:http';
import { readFile } from 'node:fs/promises';
import { extname, join, resolve } from 'node:path';
import { chromium } from 'playwright';
import { WebSocketServer } from 'ws';

const dir = resolve(process.argv[2] ?? new URL('./out', import.meta.url).pathname);
const types = { '.html': 'text/html', '.js': 'text/javascript', '.wasm': 'application/wasm' };
const isolation = {
  'Cross-Origin-Opener-Policy': 'same-origin',
  'Cross-Origin-Embedder-Policy': 'require-corp',
  'Cache-Control': 'no-store',
};

function pattern(size) {
  const bytes = Buffer.alloc(size);
  for (let i = 0; i < size; ++i) bytes[i] = (i * 31 + 7) & 0xff;
  return bytes;
}
const big = pattern(3 << 20);

async function endpoint(req, res, path) {
  switch (path) {
    case '/text':
      res.writeHead(200, { ...isolation, 'Content-Type': 'text/plain', 'X-Test': '1' });
      return res.end('hello');
    case '/echo': {
      const chunks = [];
      for await (const c of req) chunks.push(c);
      res.writeHead(200, { ...isolation, 'Content-Type': req.headers['content-type'] ?? 'application/octet-stream' });
      return res.end(Buffer.concat(chunks));
    }
    case '/big': {
      // Several writes with pauses: the client sees the body in chunks.
      res.writeHead(200, { ...isolation, 'Content-Type': 'application/octet-stream', 'Content-Length': big.length });
      const step = 256 << 10;
      for (let at = 0; at < big.length; at += step) {
        res.write(big.subarray(at, at + step));
        await new Promise((r) => setTimeout(r, 5));
      }
      return res.end();
    }
    case '/missing':
      res.writeHead(404, { ...isolation, 'Content-Type': 'text/plain' });
      return res.end('nope');
    case '/redirect':
      res.writeHead(302, { ...isolation, Location: '/text' });
      return res.end();
    case '/slow':
      res.writeHead(200, { ...isolation, 'Content-Type': 'text/plain' });
      res.write('x');
      setTimeout(() => res.end('y'), 10_000).unref();
      return;
    default:
      return false;
  }
}

const server = createServer(async (req, res) => {
  const path = new URL(req.url, 'http://x').pathname;
  try {
    if ((await endpoint(req, res, path)) !== false) return;
    const file = join(dir, path === '/' ? '/index.html' : path);
    const body = await readFile(file);
    res.writeHead(200, { ...isolation, 'Content-Type': types[extname(file)] ?? 'application/octet-stream' });
    res.end(body);
  } catch {
    if (!res.headersSent) res.writeHead(404);
    res.end();
  }
});

// /ws: echo every frame; "close" closes with 4000 "bye".
const wss = new WebSocketServer({ server, path: '/ws' });
wss.on('connection', (ws) => {
  ws.on('message', (data, isBinary) => {
    if (!isBinary && data.toString() === 'close') ws.close(4000, 'bye');
    else ws.send(data, { binary: isBinary });
  });
});

await new Promise((r) => server.listen(0, '127.0.0.1', r));
const url = `http://127.0.0.1:${server.address().port}/`;

const browser = await chromium.launch();
let failures = 1;
try {
  const page = await browser.newPage();
  page.on('console', (m) => console.log(`[page] ${m.text()}`));
  page.on('pageerror', (e) => console.log(`[pageerror] ${e.message}`));
  await page.goto(url);
  const result = await page.waitForFunction(() => globalThis.qtNetworkContract, null, { timeout: 120_000 });
  const { failures: f, log } = await result.jsonValue();
  failures = f;
  console.log(log);
} finally {
  await browser.close();
  wss.close();
  server.close();
  server.closeAllConnections();
}
process.exit(failures ? 1 : 0);
