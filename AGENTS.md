# Thinker project guide

This repository is a fork of Thinker, a C++ mod for Sid Meier's Alpha Centauri:
Alien Crossfire (SMACX). It replaces or extends game-engine behavior, including
AI, game rules, map generation, and UI. `thinker.exe` launches the original
game with `thinker.dll`; patches are applied to the game image in memory, not
to `terranx.exe` on disk. The supported game is Alien Crossfire 2.0; see
`Readme.md` and `Technical.md` for installation and binary compatibility.

## Repository map

- `src/`: C++11 implementation, mostly paired `.cpp` and `.h` files.
  - `launch.*`: launcher and Windows resources; `main.*`: DLL setup and INI
    option handling; `patch.*`, `patchdata.h`, and `patchveh.h`: patching and
    hooks. `patchdata.h` includes Scient's patch data.
  - `engine*.h` and `engine.cpp`: interfaces to original game structures,
    functions, and memory. These are sensitive to binary layout, addresses,
    calling conventions, and the supported executable.
  - `base*`, `build`, `plan`, `goal`, `move`, `path`, and `veh*`: base
    management, production, planning, movement, and unit behavior.
  - `map*`, `faction*`, `game*`, `tech`, `probe`, `savegame`, `gui*`, and
    related files: corresponding game systems and UI.
  - `config.*`: game text/config support; `lib/`: bundled dependencies.
  - `ability.cpp`: shared ability availability, slot limits, and Workshop
    selection/validation logic; `workshop.*`: Workshop rendering and binary hooks.
- `docs/thinker.ini`: distributed configuration; `docs/modmenu.txt` and other
  files under `docs/` provide game text and optional mod data.
- `tools/`: release packaging and development utilities, including the tech
  tree visualization script.
- `tests/`: standalone ability tests, read-only Workshop patch-site checks, and
  an in-game regression checklist. See `tests/README.md`.
- `Details.md`: features and options; `Technical.md`: build, debugging, and
  patching details; `Changelog.md`: release history.
- `thinker.cbp` and `thinkerlaunch.cbp`: Code::Blocks projects for the DLL and
  launcher. `CMakeLists.txt` and `CMakePresets.json` provide an alternative build.

## Building and validation

- The target is 32-bit Windows. The documented Windows setup uses Code::Blocks
  and i686 MinGW; CMake requires version 3.31 or newer and references the
  `i686-w64-mingw32` GCC toolchain. See `Technical.md` for prerequisites.
- Example CMake build: `cmake --preset debug` followed by
  `cmake --build --preset debug`. Presets also exist for develop/release,
  Ninja, and Code::Blocks generators. Outputs go under `build/<config>`.
  Targets `thinkerlib` and `thinker` build the DLL and launcher respectively.
  Presets with the same configuration share a build directory, so do not mix
  generators there without a fresh build directory.
- The main CMake project does not register tests; `src/test.cpp` is a debug
  setup hook, not a test runner. Ability tests have a separate CMake project:
  `cmake -S tests -B build/ability-tests -G "MinGW Makefiles" -DCMAKE_CXX_COMPILER=i686-w64-mingw32-g++`,
  then `cmake --build build/ability-tests` and
  `ctest --test-dir build/ability-tests --output-on-failure`.
- Verify new Workshop hook sites without modifying the executable:
  `python tests/check_workshop_patches.py "C:/Games/SMAC/terranx.exe"`.
  The supported GOG executable is 3,084,288 bytes, SHA-1
  `4b19c1fe3266b5ebc4305cd182ed6e864e3a1c4a`.
- Windows toolchains may provide `windres.exe` without the prefixed name
  expected by the root CMake file. During development, reconfiguration also
  repeatedly invalidated the cache over `CMAKE_RC_COMPILER`; do not keep
  retrying a configure loop. The full DLL and launcher were successfully built
  directly with 32-bit MinGW GCC 14.2 and `windres`. This build-system issue
  has not been fixed. A successful DLL build alone does not verify the launcher.
- Validate gameplay in a separate compatible game copy, never by overwriting
  the installed game or original saves. `Technical.md` describes debug builds,
  shortcuts, and `debug.txt`. Report validation that could not be run.

## Building and packaging releases

Use `tools/build_release.py` for new release archives, not the legacy
`makerelzip.sh`/`makedevzip.sh` scripts (those only package existing `patch/`
binaries). It requires Python 3.8+, Git, and 32-bit MinGW GCC 8.1+ (14.2 is the
documented toolchain). No CMake, Code::Blocks, 7-Zip, or Python packages are
needed. It invokes MinGW directly to avoid the CMake resource-compiler issue.

1. Review/update the release `MOD_VERSION` in `src/main.h`, `Changelog.md`,
   user-facing docs, `docs/thinker.ini`, and `docs/modmenu.txt` as appropriate.
   The script does not bump versions, commit, tag, upload, or install anything.
2. Normally commit the intended changes first. From the repository root:

   ```powershell
   python tools/build_release.py --toolchain-bin "C:\path\to\mingw32\bin"
   ```

   If `i686-w64-mingw32-g++` is already on `PATH`, omit `--toolchain-bin`.
   An explicit directory also supports unprefixed `g++`/`gcc`/`windres`;
   compilers are checked for a 32-bit MinGW target.
   On the current workstation, the verified GCC 14.2 toolchain is at
   `C:\src\alphacentauriclintmod\build\deps\mingw32\bin`; pass that directory
   rather than assuming the tools are on `PATH`. This is a local installation,
   not a dependency to download automatically or assume on other machines.
3. Optionally verify Workshop patch sites read-only during the same run:

   ```powershell
   python tools/build_release.py --toolchain-bin "C:\path\to\mingw32\bin" --game-exe "C:\Games\SMAC\terranx.exe"
   ```

The script builds **both** the DLL and resource-bearing launcher from current
sources with release flags and static MinGW runtimes, then compiles/runs the
standalone ability tests. Any command failure stops packaging. Each run uses a
fresh retained directory under `build/release-packages/` (including
`commands.json`); it never copies stale binaries from `patch/` or modifies a
game installation. Do not edit sources/assets while it is running.

Outputs default to `rel/Thinker_v<version>.zip` and `.zip.sha256`, with the version
read from `src/main.h`. The ZIP contains `thinker.exe`, `thinker.dll`, release
docs/license, and **all** `docs/` contents at the ZIP root, preserving subfolders.
`release-manifest.json` records the source commit, dirty status, compiler,
validation status, and packaged-file SHA-256 hashes.

- Existing archives/checksums are never overwritten. For a new candidate use
  `--name Thinker_v5.5-rc1`; this changes the filename, **not** the in-game version.
  `--output-dir <directory>` changes the archive location; `--jobs 2` limits
  parallel compilation. Relative argument paths are relative to your shell.
- Dirty/untracked source changes are rejected unless `--allow-dirty` is explicit.
  This is useful for local candidates; the manifest records that the commit alone
  does not describe their sources. Do not present these as clean tagged releases.
- `--skip-tests` explicitly skips the Windows ability tests (for example, when
  cross-compiling on Linux). The manifest marks them skipped. Packaging is not
  proof of gameplay validation: smoke-test the extracted archive in a separate
  compatible game copy and report unrun campaign/multiplayer checks.
- Test the packaging helpers without MinGW or game files:
  `python -B -m unittest discover -s tests -p test_build_release.py`.
  Use `python tools/build_release.py --help` for all options.

## Working on changes

- Follow existing local conventions and trace the relevant hook and original
  engine interface before changing gameplay. Avoid casually changing fixed
  addresses, structure layouts, calling conventions, or patch bytes.
- For user-facing options, check the `Config` definition, the INI parser in
  `src/main.cpp`, `docs/thinker.ini`, and relevant documentation together.
  Option names are case-sensitive (`DisableOpeningMovie`, not
  `disable_opening_movie`). New popup labels also require the updated
  `docs/modmenu.txt` to be deployed alongside the DLL.
- Preserve compatibility with the existing game and save formats; assess
  multiplayer or AI effects where relevant. Keep fork-specific changes
  focused and document user-visible behavior.

## Third special-ability slot: important constraints

- `third_ability_tech=41` defaults to Mind/Machine Interface (`TECH_MindMac`).
  Both this technology and `Rules->tech_preq_allow_2_spec_abil` (normally Neural
  Grafting) are required for three slots. `-1` disables the extension, including
  its Workshop/prototype-matching patches. Use `ability_slot_limit()` rather
  than duplicating the research checks. Existing units are not automatically
  granted a third ability.
- `UNIT::ability_flags` already saves the complete bitmask; no unit/save layout
  expansion is needed. **Do not add or index a third selection field in
  `DesignWindow`: the two native ability IDs are immediately followed by
  `custom_name`.** Slot three is derived from the remaining mask bit. Test
  replacing/removing either earlier slot while the third is occupied.
- Native `propose_proto` can return an existing design while ignoring ability
  differences, notably Clean Reactor and Fuel Nanocells. Logging a requested
  three-bit mask is not proof that a three-ability prototype was created.
  With the extension enabled, `patch_workshop()` changes both native
  comparison masks to compare all ability bits. Inspect the returned
  `Units[unit_id].ability_flags` when testing AI changes.
- Preserve native picker eligibility checks, faction free-ability exclusions,
  Cancel behavior, missile restrictions, Carrier/Deep Pressure Hull exclusion,
  and normal mineral-cost calculations. Validation must not leave abilities
  hidden in unavailable slots when the cap decreases.
- Workshop hooks include checked x86 byte adapters and read-only vtable entries.
  Code-page write access does not cover the vtable: explicitly change and
  restore its page protection. Maintain calling conventions, adapter stack
  balance, resume addresses, and the patch-site verification script together.
- `bit_count()` accepts integral types, not `VehAblFlag` enums; cast enum masks
  to `uint32_t` before calling it.
- AI design creation is in `src/plan.cpp`; cost/role-based production scoring is
  in `src/build.cpp`. Both matter: proposing a new combination alone does not
  ensure the AI will choose to build it. Avoid paying for an unnecessary third
  ability (e.g. free support or short-range interception).
- The Design Workshop shortcut is **U**, not F3/F4. For debugger-driven tests,
  direct GDB calls to `__thiscall` functions did not pass `This` in ECX and
  crashed the test process. Use a verified ABI-correct adapter or normal UI
  input; do not mistake debugger invocation failures for gameplay defects.
- Confirmed in a separate game copy: third-slot native picker, commit/reopen,
  save/reload, ability checks on a vehicle created from the design, and AI
  three-ability garrisons, distant-front interceptors, and supported formers.
  Full campaign and multiplayer behavior remain untested.
- Save structure compatibility does not mean older Workshop versions can edit
  these designs safely: they may discard the third ability. Keep the same mod
  version and `third_ability_tech` setting when continuing games and on every
  multiplayer participant.
