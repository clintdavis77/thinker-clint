#!/usr/bin/env python3
"""Build and package Thinker with 32-bit MinGW (Python 3.8+, standard library only)."""

import argparse
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import zipfile


ROOT = Path(__file__).resolve().parents[1]
WARNINGS = [
    "-pedantic", "-Wall", "-Wextra", "-Wshadow", "-Wundef", "-Wuseless-cast",
    "-Wpointer-arith", "-Wfloat-conversion",
]
SAFETY = [
    "-fno-common", "-fno-strict-overflow", "-fno-strict-aliasing",
    "-fno-delete-null-pointer-checks", "-fno-optimize-sibling-calls",
]
RELEASE_DOCS = ["Readme.md", "Details.md", "Changelog.md", "Technical.md", "License.md"]


def run(command, cwd, env):
    """Never use a shell; propagate every compiler/test failure."""
    command = [str(arg) for arg in command]
    print("+ " + subprocess.list2cmdline(command), flush=True)
    subprocess.run(command, cwd=str(cwd), env=env, check=True)


def capture(command, cwd, env):
    return subprocess.check_output(
        [str(arg) for arg in command], cwd=str(cwd), env=env,
        encoding="utf-8", errors="replace",
    ).rstrip("\r\n")


def release_version(root):
    text = (root / "src/main.h").read_text(encoding="utf-8")
    matches = re.findall(
        r'^\s*#define\s+MOD_VERSION\s+"Thinker Mod (v[0-9]+(?:\.[0-9]+)*)"\s*$',
        text, re.MULTILINE,
    )
    if len(matches) != 1:
        raise ValueError("Expected one numeric release MOD_VERSION in src/main.h")
    return matches[0]


def find_tool(directory, names):
    search_path = str(directory) if directory else None
    for name in names:
        result = shutil.which(name, path=search_path)
        if result:
            return Path(result).resolve()
    raise ValueError(
        "Cannot find {}. Put 32-bit MinGW on PATH or pass --toolchain-bin.".format(
            " / ".join(names)
        )
    )


def check_pe(path, is_dll):
    """Reject wrong architecture, truncated output, or a DLL/EXE mixup."""
    with path.open("rb") as stream:
        if stream.read(2) != b"MZ":
            raise ValueError("Not a Windows binary: " + str(path))
        stream.seek(0x3c)
        offset = struct.unpack("<I", stream.read(4))[0]
        stream.seek(offset)
        header = stream.read(26)
    if len(header) != 26 or header[:4] != b"PE\0\0":
        raise ValueError("Invalid PE header: " + str(path))
    machine = struct.unpack_from("<H", header, 4)[0]
    characteristics = struct.unpack_from("<H", header, 22)[0]
    magic = struct.unpack_from("<H", header, 24)[0]
    if machine != 0x14c or magic != 0x10b or bool(characteristics & 0x2000) != is_dll:
        raise ValueError("Expected 32-bit x86 {}: {}".format(
            "DLL" if is_dll else "EXE", path
        ))


def package_files(root, build):
    files = {name: build / name for name in ("thinker.dll", "thinker.exe")}
    files.update({name: root / name for name in RELEASE_DOCS})
    # Keep the legacy release layout: docs/* is installed at the ZIP root.
    for path in sorted((root / "docs").rglob("*")):
        if path.is_file():
            name = path.relative_to(root / "docs").as_posix()
            if name in files or name == "release-manifest.json":
                raise ValueError("Duplicate package path: " + name)
            files[name] = path
    for name in ("thinker.ini", "modmenu.txt"):
        if name not in files:
            raise ValueError("Missing required docs/" + name)
    for path in files.values():
        if not path.is_file():
            raise ValueError("Missing package input: " + str(path))
    return files


def write_package(archive, files, metadata):
    metadata = dict(metadata)
    metadata["files"] = {
        name: {"sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
               "bytes": path.stat().st_size}
        for name, path in sorted(files.items())
    }
    # Exclusive creation is intentional: no --force and no stale archive updates.
    created = False
    try:
        with archive.open("xb") as stream:
            created = True
            with zipfile.ZipFile(stream, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
                for name, path in sorted(files.items()):
                    zf.write(path, name)
                zf.writestr("release-manifest.json", json.dumps(metadata, indent=2) + "\n")
        with zipfile.ZipFile(archive) as zf:
            bad = zf.testzip()
            if bad:
                raise ValueError("ZIP CRC check failed: " + bad)
    except BaseException:
        if created:
            archive.unlink()
        raise
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    return digest


def build_release(args):
    root = ROOT
    env = os.environ.copy()
    version = release_version(root)
    name = args.name or "Thinker_" + version
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]*", name):
        raise ValueError("--name must be a filename stem using letters, digits, '.', '_' or '-'")
    output = args.output_dir.resolve()
    archive = output / (name + ".zip")
    checksum = output / (name + ".zip.sha256")
    if archive.exists() or checksum.exists():
        raise ValueError("Release already exists; choose a new --name: " + str(archive))
    git = find_tool(None, ["git"])
    commit = capture([git, "rev-parse", "HEAD"], root, env)
    status = capture([git, "status", "--porcelain", "--untracked-files=all"], root, env)
    if status and not args.allow_dirty:
        raise ValueError("Working tree is dirty. Commit it or explicitly pass --allow-dirty.")
    if status:
        print("WARNING: packaging a dirty working tree; recorded in release-manifest.json.")

    directory = args.toolchain_bin.resolve() if args.toolchain_bin else None
    cxx = find_tool(directory, ["i686-w64-mingw32-g++"] + (["g++"] if directory else []))
    # Prefer tools from the same installation, including unprefixed windres on Windows.
    cc = find_tool(cxx.parent, ["i686-w64-mingw32-gcc", "gcc"])
    windres = find_tool(cxx.parent, ["i686-w64-mingw32-windres", "windres"])
    env["PATH"] = str(cxx.parent) + os.pathsep + env.get("PATH", "")
    for compiler in (cxx, cc):
        target = capture([compiler, "-dumpmachine"], root, env)
        if not re.fullmatch(r"i[3-6]86-w64-mingw32", target):
            raise ValueError("Expected 32-bit MinGW, got {} from {}".format(target, compiler))
        gcc_version = capture([compiler, "-dumpfullversion"], root, env).split(".")
        if tuple(int(part) for part in gcc_version[:2]) < (8, 1):
            raise ValueError("MinGW GCC 8.1 or newer is required")
    compiler_version = capture([cxx, "--version"], root, env).splitlines()[0]
    if args.game_exe and not args.game_exe.is_file():
        raise ValueError("--game-exe does not exist: " + str(args.game_exe))

    base = root / "build/release-packages"
    base.mkdir(parents=True, exist_ok=True)
    build = Path(tempfile.mkdtemp(prefix=name + "-", dir=str(base)))
    print("Fresh build directory: " + str(build), flush=True)
    # Validate distributable inputs before doing the expensive compilation.
    for path in [root / name for name in RELEASE_DOCS] + [
        root / "docs/thinker.ini", root / "docs/modmenu.txt",
    ]:
        if not path.is_file():
            raise ValueError("Missing package input: " + str(path))

    commands = []
    objects = []
    sources = sorted(path for path in (root / "src").glob("*.cpp") if path.name != "launch.cpp")
    sources += sorted((root / "src/lib").glob("*.c"))
    for source in sources:
        obj = build / (source.relative_to(root).as_posix().replace("/", "_") + ".o")
        objects.append(obj)
        flags = (["-std=c++11"] if source.suffix == ".cpp" else [])
        flags += WARNINGS + SAFETY + [
            "-O2", "-march=pentium4", "-mtune=generic", "-DBUILD_DLL", "-DBUILD_REL",
        ]
        commands.append([cxx if source.suffix == ".cpp" else cc, *flags,
                         "-c", source, "-o", obj])
    # A response file avoids Windows command-line length limits during linking.
    response = build / "dll-objects.rsp"
    response.write_text(
        "\n".join('"' + obj.as_posix() + '"' for obj in objects) + "\n", encoding="utf-8"
    )
    dll_link = [cxx, "-shared", "-s", "-static", "-o", build / "thinker.dll",
                "@" + str(response), "-luser32", "-lgdi32", "-lwinmm", "-lpsapi"]
    resource = [windres, "-I", root / "src", "-O", "coff", "-i", root / "src/launch.rc",
                "-o", build / "launch.rc.o"]
    launcher = [cxx, "-std=c++11", *WARNINGS, "-O2", "-flto", "-fno-rtti",
                "-fno-strict-aliasing", "-fno-delete-null-pointer-checks", "-DBUILD_REL",
                "-s", "-static", "-mwindows", root / "src/launch.cpp",
                build / "launch.rc.o", "-o", build / "thinker.exe", "-luser32"]
    test_build = [cxx, "-std=c++11", "-Wall", "-Wextra", "-static",
                  root / "tests/workshop_abilities.cpp", root / "src/ability.cpp",
                  "-o", build / "workshop_abilities.exe"]
    all_commands = commands + [dll_link, resource, launcher]
    if not args.skip_tests:
        all_commands += [test_build, [build / "workshop_abilities.exe"]]
    if args.game_exe:
        all_commands += [[sys.executable, root / "tests/check_workshop_patches.py",
                          args.game_exe.resolve()]]
    (build / "commands.json").write_text(
        json.dumps([[str(arg) for arg in cmd] for cmd in all_commands], indent=2) + "\n",
        encoding="utf-8",
    )
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        list(pool.map(lambda command: run(command, root, env), commands))
    for command in all_commands[len(commands):]:
        run(command, root, env)
    check_pe(build / "thinker.dll", is_dll=True)
    check_pe(build / "thinker.exe", is_dll=False)
    # Fail rather than mislabel an archive if the header changed during the build.
    if release_version(root) != version:
        raise ValueError("MOD_VERSION changed during build; rerun after source edits finish")
    metadata = {
        "version": version,
        "built_utc": datetime.now(timezone.utc).isoformat(),
        "git_commit": commit,
        "dirty": bool(status),
        "git_status": status.splitlines(),
        "compiler": compiler_version,
        "target": "i686-w64-mingw32",
        "configuration": "Release (BUILD_DLL / BUILD_REL, static MinGW runtime)",
        "ability_tests": "skipped" if args.skip_tests else "passed",
        "workshop_patch_check": "passed" if args.game_exe else "not run",
        "gameplay_validation": "not run by this script",
    }
    output.mkdir(parents=True, exist_ok=True)
    digest = write_package(archive, package_files(root, build), metadata)
    with checksum.open("x", encoding="ascii") as stream:
        stream.write(digest + "  " + archive.name + "\n")
    print("\nRelease: {}\nSHA-256: {}\nBuild files: {}".format(archive, digest, build))
    print("Gameplay validation is still required in a separate compatible game copy.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--toolchain-bin", type=Path, help="32-bit MinGW bin directory")
    parser.add_argument("--output-dir", type=Path, default=ROOT / "rel",
                        help="archive directory (default: repository rel/)")
    parser.add_argument("--name", help="archive stem (default: Thinker_v<MOD_VERSION>)")
    parser.add_argument("--jobs", type=int, default=min(4, os.cpu_count() or 1),
                        help="parallel compilation jobs (default: up to 4)")
    parser.add_argument("--allow-dirty", action="store_true",
                        help="explicitly allow and record uncommitted/untracked changes")
    parser.add_argument("--skip-tests", action="store_true",
                        help="skip Windows ability tests (e.g. cross-compiling on Linux)")
    parser.add_argument("--game-exe", type=Path,
                        help="read-only Workshop patch verification against this terranx.exe")
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    try:
        build_release(args)
    except (ValueError, OSError, subprocess.CalledProcessError, struct.error) as error:
        print("Release failed: " + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
