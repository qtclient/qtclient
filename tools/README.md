# qtclient tools

In the **IdleOT monorepo**, `make extract` / `make tibia-sync` / RE helpers
resolve to umbrella [`tools/`](../../../tools/) first (see root
`apps/qtclient/Makefile` `TOOLS_DIR`). Prefer editing those copies.

This tree keeps:

- **qtclient-only** helpers (`publish/`, `fetch-assets/`, `qt/`, `cip/`,
  `extract/apng_to_webp.py`, …)
- **Stand-alone fallbacks** of extract / tibia-sync / re when this repo is
  checked out without the umbrella `tools/` tree — keep them in sync with
  umbrella when you change ingest behavior.
