#!/usr/bin/env python3
"""Unit tests for local play-tip helpers (CIP packages[] shape)."""
from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
PUBLISH = HERE.parent
sys.path.insert(0, str(PUBLISH))

import emit_play_tip as play  # noqa: E402


class SanitizeDirectoryNameTest(unittest.TestCase):
    def test_bare_like_cip(self) -> None:
        self.assertEqual(play.sanitize_directory_name("Tibia"), "IdleOT-Tibia")
        self.assertEqual(play.sanitize_directory_name("IdleOT"), "IdleOT")
        self.assertEqual(play.sanitize_directory_name("session/all"), "IdleOT-session-all")

    def test_strips_unsafe(self) -> None:
        name = play.sanitize_directory_name("feat: foo bar!")
        self.assertTrue(name.startswith("IdleOT-"))
        self.assertNotIn(":", name)
        self.assertNotIn(" ", name)


class GitBranchParseTest(unittest.TestCase):
    def test_nested_branch_keeps_path(self) -> None:
        # Mirrors git worktree list --porcelain "branch refs/heads/session/all".
        ref = "refs/heads/session/all"
        branch = ref[len("refs/heads/") :] if ref.startswith("refs/heads/") else ref
        self.assertEqual(branch, "session/all")
        self.assertEqual(play.sanitize_directory_name(branch), "IdleOT-session-all")


class EmitPackageFilesTest(unittest.TestCase):
    def test_unpack_false_and_shared_version_hash(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            tree = root / "tree" / "bin"
            tree.mkdir(parents=True)
            client = tree / "client"
            client.write_bytes(b"fake-client")
            client.chmod(0o755)
            (tree / "note.txt").write_text("x\n")
            pkg_dir = root / "pkg"
            records, version, exe = play.emit_package_files(pkg_dir, root / "tree", "linux")
            self.assertEqual(exe, "bin/client")
            self.assertTrue(version.startswith("15.00."))
            self.assertTrue((pkg_dir / "package.json").is_file())
            pkg = json.loads((pkg_dir / "package.json").read_text())
            self.assertEqual(pkg["executable"], "bin/client")
            self.assertTrue(any(r["localfile"] == "bin/client" and r.get("executable") for r in records))
            self.assertTrue(all(r.get("unpack") is False for r in records))
            # Code-only: same tree → same version stamp (assets stay separate).
            _, version2, _ = play.emit_package_files(root / "pkg2", root / "tree", "linux")
            self.assertEqual(version, version2)


class ResolveAssetsTest(unittest.TestCase):
    def test_falls_back_to_cip(self) -> None:
        url, ver = play.resolve_assets("linux", None)
        self.assertIn("static.tibia.com", url)
        self.assertIn("assets.json.sha256", ver)


if __name__ == "__main__":
    unittest.main()
