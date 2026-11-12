# Open-sourcing qtclient is a thin CipSoft-compatible Qt/QML client. Product features ship as
runtime packs under `client/mods/<id>/` and `launcher/mods/<id>/` — they are not
woven into stock `client/tibia`.

| Layer | Stock (this tree) | Product pack |
|-------|-------------------|--------------|
| CipSoft engine | `client/tibia` (+ `upstream/` from sync) | — |
| Launcher | `launcher/src` | Brand/art via `launcher/mods/<id>` |
| `upstream/*` / CIP assets | Never commit — `make sync` + `make extract` | Same |
| Mods API + default packs (`myaac`, `otclient`) | Yes | `mod.json` + linked C++ / FS QML + `mod_api_v1` |
| Product packs | Not required to build stock | Drop in `mods/<id>/` or umbrella `make qtclient install` (`QTCLIENT_PACK`) |

## Inplace extension

```bash
git clone https://github.com/qtclient/qtclient.git
cd qtclient
ln -s ~/my-product-mods client/mods/my_product
cmake -B build -DQTCLIENT_ENABLE_MODS=ON
cmake --build build --target tibia
```

Stock-only: `-DQTCLIENT_ENABLE_MODS=OFF`.

## Env prefixes

| Prefix | Use |
|--------|-----|
| `QTCLIENT_*` | Stock qtclient / launcher process env and WASM hooks |
| `product pack env knobs` | product pack-only knobs (lighting, raycast/weather perf) |
| `TIBIA_*` | CipSoft-shaped CMake/target names (`TIBIA_WITH_GAMECORE`, brand.cmake keys) — not invented process env |

## First-time consumers

Binary: demo Release → `./launcher` (myAAC `http://127.0.0.1/login.php` +
CipSoft `assetsurl`; see [public/demo-tip.md](public/demo-tip.md)) **or**
`./qtclient-fetch-assets` → `./client` (BYO Tibia).

Source: `make setup` (resolves the local Tibia install; optional `TIBIA=` /
`MSGMAP=`).

## Related

- [mods.md](mods.md) — pack format, ModLoader, WorldView customization
- [public/demo-tip.md](public/demo-tip.md) — CIP assetsurl + package tip
- [public/CONTRIBUTING.md](public/CONTRIBUTING.md) — contributor guide
