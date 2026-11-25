"""FUNCMAP → cip_stamps headers + h vs cpp stamp_check."""

from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(HERE))
import funcmap  # noqa: E402
import stamp_check  # noqa: E402
import sync  # noqa: E402


SAMPLE_FN = (
    'FN {"symbol":"gamewindow::TNPCTradeController::setFilter",'
    '"addr":"0x100abc","size":32,'
    '"sha256":"a3f91c0e7b2d4e88111111111111111111111111111111111111111111111111"}\n'
    'FN {"symbol":"gamewindow::TNPCTradeController::metaObject",'
    '"addr":"0x100aaa","size":8,'
    '"sha256":"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"}\n'
    'FN {"symbol":"tibia::trade::TNPCTradeController::clearFilter",'
    '"addr":"0x100def","size":16,'
    '"sha256":"c0ffee00deadbeef222222222222222222222222222222222222222222222222"}\n'
    'FN {"symbol":"FUN_100bf2b30",'
    '"addr":"0x100bf2b30","size":40,'
    '"sha256":"dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd"}\n'
)


class FuncmapStampsTest(unittest.TestCase):
    def test_noise_and_tibia_prefix(self):
        with tempfile.TemporaryDirectory() as tmp:
            raw = Path(tmp) / "f.jsonl"
            raw.write_text(SAMPLE_FN)
            rows = funcmap.load_funcmap(raw)
        tree = funcmap.group_rows(rows)
        # metaObject dropped; gamewindow → tibia/gamewindow
        self.assertIn(("tibia", "gamewindow"), tree)
        methods = {m["method"] for m in tree[("tibia", "gamewindow")]["TNPCTradeController"]}
        self.assertIn("setFilter", methods)
        self.assertNotIn("metaObject", methods)

    def test_play_path_vN_stamped_when_sized(self):
        raw = (
            'FN {"symbol":"creatures::TCreatureMovementTween::v9",'
            '"addr":"100a3dcd0","size":424,'
            '"sha256":"6a40e4aff7682e7e157a70f77f789492c82175dfcb5ec808092f930c5ce093ab"}\n'
            'FN {"symbol":"creatures::TCreatureMovementTween::v10",'
            '"addr":"100a3e020","size":10,'
            '"sha256":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}\n'
            'FN {"symbol":"shared::TRandomNumberGenerator::v5",'
            '"addr":"1007bb160","size":114,'
            '"sha256":"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"}\n'
        )
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "f.jsonl"
            path.write_text(raw)
            rows = funcmap.load_funcmap(path)
            tree = funcmap.group_rows(rows)
        self.assertIn(("tibia", "creatures"), tree)
        tween = {m["method"] for m in tree[("tibia", "creatures")]["TCreatureMovementTween"]}
        self.assertIn("v9", tween)
        self.assertNotIn("v10", tween)  # below size floor
        self.assertNotIn(("tibia", "shared"), tree)  # non-allowlist vN stays noise

    def test_play_path_destructor_ghosts_dropped(self):
        raw = (
            'FN {"symbol":"creatures::TCreatureStorage::v4",'
            '"addr":"10086e990","size":54,'
            '"sha256":"f09eaaf94721c232157a70f77f789492c82175dfcb5ec808092f930c5ce093ab"}\n'
            'FN {"symbol":"creatures::TCreatureMovementTween::v9",'
            '"addr":"100a3dcd0","size":424,'
            '"sha256":"6a40e4aff7682e7e157a70f77f789492c82175dfcb5ec808092f930c5ce093ab"}\n'
        )
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "f.jsonl"
            path.write_text(raw)
            rows = funcmap.load_funcmap(path)
            tree = funcmap.group_rows(rows)
        self.assertNotIn("TCreatureStorage", tree.get(("tibia", "creatures"), {}))
        self.assertIn("v9", {m["method"] for m in tree[("tibia", "creatures")]["TCreatureMovementTween"]})

    def test_load_and_emit_headers_with_moc(self):
        with tempfile.TemporaryDirectory() as tmp:
            raw = Path(tmp) / "funcmap.jsonl"
            raw.write_text(SAMPLE_FN)
            rows = funcmap.load_funcmap(raw)
            contract = Path(tmp) / "contract"
            contract.mkdir()
            (contract / "moc.json").write_text(json.dumps([{
                "class": "tibia::gamewindow::TNPCTradeController",
                "superclass": "QObject",
                "slots": [],
                "invokables": [{
                    "name": "setFilter",
                    "return": "void",
                    "access": "public",
                    "args": [{"type": "QString", "name": "NameFilter"}],
                }],
                "properties": [],
                "signals": [],
                "enums": [],
            }]))
            upstream = Path(tmp) / "upstream"
            upstream.mkdir()
            root, n = funcmap.write_stamp_headers(rows, upstream, contract_dir=contract)
            self.assertGreaterEqual(n, 1)
            hdr = root / "tibia" / "gamewindow" / "TNPCTradeController.h"
            self.assertTrue(hdr.is_file(), hdr)
            text = hdr.read_text()
            self.assertIn("// b1/a3f91c0e7b2d4e88 @ 0x100abc", text)
            self.assertIn("void setFilter(QString NameFilter);", text)
            self.assertNotIn("metaObject", text)

    def test_attach_names_from_tip(self):
        with tempfile.TemporaryDirectory() as tmp:
            tip = Path(tmp) / "tip"
            tip.mkdir()
            (tip / "c.h").write_text(
                "namespace tibia {\nnamespace gamewindow {\n"
                "class TNPCTradeController final : public cip::X {\n"
                "public:\n"
                "    /// CIP 0x100bf2b30.\n"
                "    Q_INVOKABLE void requestFilterByName(const QString& NameFilter) override;\n"
                "};\n}\n}\n"
            )
            rows = [{
                "symbol": "FUN_100bf2b30",
                "addr": "0x100bf2b30",
                "size": 40,
                "sha256": "d" * 64,
                "stamp": "b1/" + "d" * 16,
            }]
            named = funcmap.attach_names_from_tip(rows, [tip])
            self.assertIn("requestFilterByName", named[0]["symbol"])

    def test_stamp_check_matched_and_diverged(self):
        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp)
            stamps = base / "cip_stamps" / "tibia" / "trade"
            stamps.mkdir(parents=True)
            (stamps / "TNPCTradeController.h").write_text(
                "// Generated\n#pragma once\n"
                "namespace tibia { namespace trade {\n"
                "class TNPCTradeController {\npublic:\n"
                "  // b1/a3f91c0e7b2d4e88 @ 0x100abc\n"
                "  void setFilter();\n"
                "  // b1/c0ffee00deadbeef @ 0x100def\n"
                "  void clearFilter();\n"
                "};\n}} // namespace\n"
            )
            tip = base / "tip"
            tip.mkdir()
            (tip / "npctrade.cpp").write_text(
                "// b1/a3f91c0e7b2d4e88\n"
                "void TNPCTradeController::setFilter(const QString &text)\n{\n}\n"
                "\n"
                "// b1/deadbeefdeadbeef\n"
                "void TNPCTradeController::clearFilter()\n{\n}\n"
            )
            catalog = stamp_check.parse_catalog(base / "cip_stamps")
            impl = stamp_check.parse_impl_stamps([tip])
            matched, diverged, missing, unknown = stamp_check.compare(catalog, impl)
            self.assertEqual(len(matched), 1)
            self.assertEqual(len(diverged), 1)
            self.assertEqual(missing, [])
            self.assertEqual(unknown, [])

    def test_funcmap_gate_bootstrap_allows_omit(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp)
            (out / "VERSION.json").write_text(json.dumps({"client_sha256": "old"}))
            sync.require_funcmap_for_new_client(out, "new", None)

    def test_funcmap_gate_requires_when_stamps_exist(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp)
            (out / "VERSION.json").write_text(json.dumps({"client_sha256": "old"}))
            (out / "cip_stamps").mkdir()
            with self.assertRaises(SystemExit) as ctx:
                sync.require_funcmap_for_new_client(out, "new", None)
            self.assertIn("--funcmap", str(ctx.exception))


if __name__ == "__main__":
    unittest.main()
