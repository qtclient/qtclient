"""client_sha256 changes require --msgmap so protocol/ cannot silently freeze."""

from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import sync  # noqa: E402


class MsgmapGateTest(unittest.TestCase):
    def test_same_sha_allows_omitting_msgmap(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp)
            (out / "VERSION.json").write_text(json.dumps({"client_sha256": "abc"}))
            sync.require_msgmap_for_new_client(out, "abc", None)

    def test_new_sha_without_msgmap_fails(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp)
            (out / "VERSION.json").write_text(json.dumps({"client_sha256": "oldsha"}))
            with self.assertRaises(SystemExit) as ctx:
                sync.require_msgmap_for_new_client(out, "newsha", None)
            self.assertIn("--msgmap", str(ctx.exception))

    def test_new_sha_with_msgmap_ok(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp)
            (out / "VERSION.json").write_text(json.dumps({"client_sha256": "oldsha"}))
            sync.require_msgmap_for_new_client(out, "newsha", Path("/tmp/msgmap.jsonl"))

    def test_missing_version_without_msgmap_fails(self):
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaises(SystemExit):
                sync.require_msgmap_for_new_client(Path(tmp), "newsha", None)

    def test_previous_client_sha(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp)
            self.assertIsNone(sync.previous_client_sha(out))
            (out / "VERSION.json").write_text(json.dumps({"client_sha256": "deadbeef"}))
            self.assertEqual(sync.previous_client_sha(out), "deadbeef")


if __name__ == "__main__":
    unittest.main()
