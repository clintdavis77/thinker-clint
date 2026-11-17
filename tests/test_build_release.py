"""Release tooling tests; no MinGW compiler or installed game required."""

import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest
import zipfile
from unittest.mock import patch


SPEC = importlib.util.spec_from_file_location(
    "build_release", Path(__file__).resolve().parents[1] / "tools/build_release.py"
)
release = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(release)


class ReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="thinker-release-test-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / "src").mkdir()
        (self.root / "src/main.h").write_text(
            '#define MOD_VERSION "Thinker Mod v5.5"\n'
            '#define MOD_VERSION "Thinker Mod develop build"\n', encoding="utf-8"
        )
        self.build = self.root / "build"
        self.build.mkdir()
        for name in ("thinker.exe", "thinker.dll"):
            (self.build / name).write_bytes(b"test binary")
        for name in release.RELEASE_DOCS:
            (self.root / name).write_text(name, encoding="utf-8")
        (self.root / "docs/smac_mod").mkdir(parents=True)
        for name in ("thinker.ini", "modmenu.txt", "smac_mod/alphax.txt"):
            (self.root / "docs" / name).write_text(name, encoding="utf-8")

    def test_version_uses_release_not_develop_label(self):
        self.assertEqual(release.release_version(self.root), "v5.5")

    def test_missing_version_fails(self):
        (self.root / "src/main.h").write_text("", encoding="utf-8")
        with self.assertRaises(ValueError):
            release.release_version(self.root)

    def test_package_layout_and_manifest(self):
        archive = self.root / "release.zip"
        digest = release.write_package(
            archive, release.package_files(self.root, self.build), {"version": "v5.5"}
        )
        self.assertEqual(len(digest), 64)
        with zipfile.ZipFile(archive) as zf:
            self.assertIsNone(zf.testzip())
            self.assertIn("smac_mod/alphax.txt", zf.namelist())
            self.assertIn("modmenu.txt", zf.namelist())
            self.assertIn("License.md", zf.namelist())
            self.assertNotIn("docs/thinker.ini", zf.namelist())
            manifest = json.loads(zf.read("release-manifest.json"))
            self.assertEqual(manifest["version"], "v5.5")
            expected = release.hashlib.sha256(b"test binary").hexdigest()
            self.assertEqual(manifest["files"]["thinker.dll"]["sha256"], expected)
            self.assertEqual(manifest["files"]["thinker.exe"]["bytes"], 11)

    def test_existing_archive_is_never_overwritten(self):
        archive = self.root / "release.zip"
        archive.write_bytes(b"keep this release")
        with self.assertRaises(FileExistsError):
            release.write_package(archive, {}, {})
        self.assertEqual(archive.read_bytes(), b"keep this release")

    def test_partial_archive_is_removed_on_failure(self):
        archive = self.root / "release.zip"
        with patch.object(zipfile.ZipFile, "write", side_effect=OSError("disk full")):
            with self.assertRaises(OSError):
                release.write_package(archive, {"thinker.dll": self.build / "thinker.dll"}, {})
        self.assertFalse(archive.exists())

    def test_required_runtime_assets(self):
        (self.root / "docs/modmenu.txt").unlink()
        with self.assertRaisesRegex(ValueError, "modmenu"):
            release.package_files(self.root, self.build)

    def test_missing_launcher_fails(self):
        (self.build / "thinker.exe").unlink()
        with self.assertRaisesRegex(ValueError, "thinker.exe"):
            release.package_files(self.root, self.build)

    def test_assets_cannot_replace_binary(self):
        (self.root / "docs/thinker.dll").write_bytes(b"stale")
        with self.assertRaisesRegex(ValueError, "Duplicate"):
            release.package_files(self.root, self.build)

    def test_32_bit_pe_type_validation(self):
        path = self.build / "test.exe"
        for dll in (False, True):
            data = bytearray(154)
            data[:2] = b"MZ"
            struct.pack_into("<I", data, 0x3c, 128)
            data[128:132] = b"PE\0\0"
            struct.pack_into("<H", data, 132, 0x14c)
            struct.pack_into("<H", data, 150, 0x2000 if dll else 0)
            struct.pack_into("<H", data, 152, 0x10b)
            path.write_bytes(data)
            release.check_pe(path, is_dll=dll)
            with self.assertRaises(ValueError):
                release.check_pe(path, is_dll=not dll)
        struct.pack_into("<H", data, 132, 0x8664)
        path.write_bytes(data)
        with self.assertRaises(ValueError):
            release.check_pe(path, is_dll=True)

    def test_invalid_binary_fails(self):
        with self.assertRaises(ValueError):
            release.check_pe(self.build / "thinker.exe", is_dll=False)

    def build_args(self, **overrides):
        values = dict(
            name=None, output_dir=self.root / "rel", allow_dirty=False,
            toolchain_bin=None, game_exe=None, skip_tests=False, jobs=1,
        )
        values.update(overrides)
        return release.argparse.Namespace(**values)

    def test_dirty_tree_requires_explicit_permission(self):
        with patch.object(release, "ROOT", self.root), \
                patch.object(release, "find_tool", return_value=Path("git")), \
                patch.object(release, "capture", side_effect=["commit", " M src/main.h"]):
            with self.assertRaisesRegex(ValueError, "dirty"):
                release.build_release(self.build_args())
        self.assertFalse((self.root / "rel").exists())

    def test_compiler_failure_never_packages(self):
        (self.root / "src/main.cpp").write_text("// test", encoding="utf-8")
        responses = ["commit", "", "i686-w64-mingw32", "14.2.0",
                     "i686-w64-mingw32", "14.2.0", "GCC 14.2.0"]
        with patch.object(release, "ROOT", self.root), \
                patch.object(release, "find_tool", return_value=Path("compiler")), \
                patch.object(release, "capture", side_effect=responses), \
                patch.object(release, "run", side_effect=release.subprocess.CalledProcessError(
                    1, ["compiler"]
                )), patch.object(release, "write_package") as package:
            with self.assertRaises(release.subprocess.CalledProcessError):
                release.build_release(self.build_args())
            package.assert_not_called()
        self.assertFalse((self.root / "rel").exists())

    def test_64_bit_toolchain_rejected_before_build(self):
        with patch.object(release, "ROOT", self.root), \
                patch.object(release, "find_tool", return_value=Path("compiler")), \
                patch.object(release, "capture",
                             side_effect=["commit", "", "x86_64-w64-mingw32"]):
            with self.assertRaisesRegex(ValueError, "32-bit"):
                release.build_release(self.build_args())
        self.assertFalse((self.root / "build/release-packages").exists())


if __name__ == "__main__":
    unittest.main()
