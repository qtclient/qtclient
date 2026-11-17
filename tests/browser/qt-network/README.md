# Qt network contract (browser)

Checks the pinned Qt kit's network layer on the web — `QNetworkAccessManager`
over `fetch` and `QWebSocket` over the browser WebSocket (the qtclient/Qt5 fork,
`third_party/qt5-cip`), with JSPI event delivery — in headless Chromium against
a local server (`run.mjs`): streamed GET, POST, HTTP errors, redirects, abort, a
nested event loop inside a reply handler, and a WebSocket session.

```bash
bash tests/browser/qt-network/build.sh        # Qt wasm + desktop kits (make qt WASM=1)
cd tests/browser/qt-network
npm install --no-save --no-package-lock playwright@1.64.1 ws@8.18.3
npx playwright install chromium
node run.mjs out
```

CI runs it in the `wasm` job of `ci-client.yml`.
