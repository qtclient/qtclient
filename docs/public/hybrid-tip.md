# Hybrid tip (qtclient)

CIP launcher protocol with **our** client package and **CipSoft** assets:

| Tip field | Points at |
|-----------|-----------|
| `url` / `versionurl` | qtclient GitHub Release `package.json` (+ `.version`) |
| `assetsurl` / `assetsversionurl` | `https://static.tibia.com/launcher/assets-current/…` |
| `hints` | optional CIP hints |

Example tip: [`tibiametadata.hybrid.json`](tibiametadata.hybrid.json).

## Running

```bash
export QTCLIENT_TIP_URL="https://github.com/qtclient/qtclient/releases/latest/download/tibiametadata.json"
# or a self-hosted copy of the hybrid tip
./launcher
```

`PackageClient` uses tip-relative downloads for foreign hosts (CIP). A product
pack’s `/content/<hash>` store applies only when `QTCLIENT_CDN_URL` /
`QTCLIENT_CDN_URL` points at that product CDN (or `QTCLIENT_CDN_CONTENT_STORE=1`).

**ToS:** Fetching CipSoft `assetsurl` content is subject to CipSoft Terms of
Service and your licensed Tibia install. Do not redistribute CipSoft blobs.

## Packaging a Release

1. Build `launcher` + `client` (tibia).
2. Emit CIP-shaped `package.json` for the client binary (lzma + hashes).
3. Publish hybrid `tibiametadata.json` with Release asset URLs for the package
   and CIP `assetsurl`s unchanged.
4. Attach launcher binary, package blobs, and tip to the GitHub Release.

Do **not** attach CipSoft asset blobs to the Release.
