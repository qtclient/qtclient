#!/usr/bin/env python3
"""Render portal item sprites from the OTC things pack (CIP appearances + sprites).

Every pickupable object (`take` flag) becomes `<dest>/<client id>.png`: the
first pattern (count 1 / default fluid), all animation phases. Animated items
are looping APNGs using the client's phase durations.

Output (read by portal `/api/items`, published as the CDN `assets/items/`
slice by `make -C apps/qtclient publish-assets`):

  <dest>/<client id>.png
  <dest>/.revision        catalog-content.json sha256[:16] + format

Usage:
  python3 tools/extract/items.py                     # newest things pack → apps/portal/apps/web/items
  python3 tools/extract/items.py --things DIR --dest DIR
"""
from __future__ import annotations

import argparse
from pathlib import Path

try:
    from .cip_sprites import (
        APPEARANCES_OBJECT,
        FLAG_TAKE,
        ROOT,
        THINGS_ROOT,
        SpriteInfo,
        SpriteStore,
        apng,
        find_things_dir,
        load_appearances,
        render_pack,
    )
except ImportError:
    from cip_sprites import (
        APPEARANCES_OBJECT,
        FLAG_TAKE,
        ROOT,
        THINGS_ROOT,
        SpriteInfo,
        SpriteStore,
        apng,
        find_things_dir,
        load_appearances,
        render_pack,
    )

DEFAULT_DEST = ROOT / "apps/portal/apps/web/items"
FORMAT = "i1"  # bump when the on-disk layout changes


def load_items(dat: Path) -> dict[int, SpriteInfo]:
    return {
        item_id: app.group()
        for item_id, app in load_appearances(dat, APPEARANCES_OBJECT).items()
        if FLAG_TAKE in app.flags
    }


def render_item(store: SpriteStore, item_id: int, info: SpriteInfo, dest: Path) -> bool:
    frames: list[tuple[bytes, int]] = []
    size: tuple[int, int] | None = None
    for phase, delay in enumerate(info.durations):
        sprite = store.sprite(info.sprite_id(phase))
        if sprite is None:
            continue
        w, h, rgba = sprite
        if size is None:
            size = (w, h)
        elif size != (w, h):
            continue
        frames.append((rgba, delay))
    if size is None:
        return False
    (dest / f"{item_id}.png").write_bytes(apng(*size, frames))
    return True


_worker: tuple[SpriteStore, dict[int, SpriteInfo], Path] | None = None


def _init_worker(things: str, dest: str) -> None:
    global _worker
    store = SpriteStore(Path(things))
    _worker = (store, load_items(store.appearances), Path(dest))


def _render_chunk(item_ids: list[int]) -> int:
    assert _worker is not None
    store, items, dest = _worker
    return sum(1 for item_id in item_ids if render_item(store, item_id, items[item_id], dest))


def render_all(things: Path, dest: Path = DEFAULT_DEST) -> str:
    """Render every pickupable item in `things` into a fresh `dest`; returns the revision."""
    item_ids = sorted(load_items(SpriteStore(things).appearances))
    return render_pack("items", things, dest, FORMAT, item_ids, _init_worker, _render_chunk)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--things", type=Path, default=None, help="things pack dir (default: newest apps/qtclient/data/things/<ver>)")
    parser.add_argument("--dest", type=Path, default=DEFAULT_DEST)
    args = parser.parse_args()
    things = args.things or find_things_dir()
    if things is None:
        raise SystemExit(f"no things pack under {THINGS_ROOT} — run: make extract")
    render_all(things.resolve(), args.dest.resolve())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
