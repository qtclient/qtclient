#!/usr/bin/env python3
"""Render portal outfit frames from the OTC things pack (CIP appearances + sprites).

Output layout (read by portal `/api/outfits`, published as the CDN
`assets/outfits/` slice by `make -C apps/qtclient publish-assets`):

  <dest>/<looktype>/<phase>_<mount>_<addon>_<direction>[_template].png
  <dest>/<looktype>/outfit.data.json   {"files": ["<looktype>/<name>", ...], "framesNumber": N}
  <dest>/.revision                     catalog-content.json sha256[:16] + format

Only the south direction (3) is rendered: `/api/outfits` never asks for others.

Usage:
  python3 tools/extract/outfits.py                   # newest things pack → apps/portal/apps/web/outfits_anim
  python3 tools/extract/outfits.py --things DIR --dest DIR
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

try:
    from .cip_sprites import (
        APPEARANCES_OUTFIT,
        FRAME_GROUP_IDLE,
        FRAME_GROUP_MOVING,
        ROOT,
        THINGS_ROOT,
        SpriteInfo,
        SpriteStore,
        find_things_dir,
        load_appearances,
        png,
        render_pack,
    )
except ImportError:
    from cip_sprites import (
        APPEARANCES_OUTFIT,
        FRAME_GROUP_IDLE,
        FRAME_GROUP_MOVING,
        ROOT,
        THINGS_ROOT,
        SpriteInfo,
        SpriteStore,
        find_things_dir,
        load_appearances,
        png,
        render_pack,
    )

DEFAULT_DEST = ROOT / "apps/portal/apps/web/outfits_anim"
FORMAT = "s1"  # bump when the on-disk layout changes
SOUTH = 2  # pattern_x index (N, E, S, W)


def load_outfits(dat: Path) -> dict[int, SpriteInfo]:
    """looktype → walk (moving) frame group, else idle."""
    return {
        look_id: app.group(FRAME_GROUP_MOVING, FRAME_GROUP_IDLE)
        for look_id, app in load_appearances(dat, APPEARANCES_OUTFIT).items()
    }


def render_outfit(store: SpriteStore, look_id: int, info: SpriteInfo, dest: Path) -> int:
    out_dir = dest / str(look_id)
    out_dir.mkdir(parents=True, exist_ok=True)
    x = SOUTH if info.pattern_x > SOUTH else 0
    files: list[str] = []
    for phase in range(info.phases):
        for z in range(info.pattern_z):
            for y in range(info.pattern_y):
                for layer in range(min(info.layers, 2)):
                    sprite = store.sprite(info.sprite_id(phase, x, y, z, layer))
                    if sprite is None:
                        continue
                    suffix = "_template" if layer == 1 else ""
                    name = f"{phase + 1}_{z + 1}_{y + 1}_3{suffix}.png"
                    (out_dir / name).write_bytes(png(*sprite))
                    files.append(f"{look_id}/{name}")
    data = {"files": files, "framesNumber": info.phases if files else 0, "mountFramesNumber": 0}
    (out_dir / "outfit.data.json").write_text(json.dumps(data, separators=(",", ":")))
    return len(files)


_worker: tuple[SpriteStore, dict[int, SpriteInfo], Path] | None = None


def _init_worker(things: str, dest: str) -> None:
    global _worker
    store = SpriteStore(Path(things))
    _worker = (store, load_outfits(store.appearances), Path(dest))


def _render_chunk(look_ids: list[int]) -> int:
    assert _worker is not None
    store, outfits, dest = _worker
    return sum(1 for look_id in look_ids if render_outfit(store, look_id, outfits[look_id], dest))


def render_all(things: Path, dest: Path = DEFAULT_DEST) -> str:
    """Render every outfit in `things` into a fresh `dest`; returns the revision."""
    look_ids = sorted(load_outfits(SpriteStore(things).appearances))
    return render_pack("outfits", things, dest, FORMAT, look_ids, _init_worker, _render_chunk)


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
