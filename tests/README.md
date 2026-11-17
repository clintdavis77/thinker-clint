# Third-ability checks

## Release tooling tests

```powershell
python -B -m unittest discover -s tests -p test_build_release.py
```

These standard-library-only tests need neither MinGW nor the game. They check
release layout/metadata, required assets, hashes, refusal to overwrite archives,
cleanup after packaging failure, 32-bit binary/toolchain validation, and that
dirty trees or failed compiler commands cannot silently produce a release.
See `AGENTS.md` for the end-to-end build/package command.

## Automated logic tests

On Windows with the 32-bit MinGW tools on `PATH`:

```powershell
cmake -S tests -B build/ability-tests -G "MinGW Makefiles" -DCMAKE_CXX_COMPILER=i686-w64-mingw32-g++
cmake --build build/ability-tests
ctest --test-dir build/ability-tests --output-on-failure
```

These compile the production selection/validation code against mock technology
and component tables. They check the 1/2/3-slot progression, custom/disabled
unlock tech, faction isolation, add/replace/remove, duplicates, hidden slots,
missiles, incompatible abilities, and preservation of the custom-name field.

Verify binary patch sites against a supported, unmodified game executable:

```powershell
python tests/check_workshop_patches.py "C:/Games/SMAC/terranx.exe"
```

This uses only Python's standard library and does not modify the executable.

## Validation performed (2026-11-17)

* Full debug DLL and launcher built with 32-bit MinGW GCC 14.2, including the
  project's warning flags for the DLL.
* Standalone tests passed; all 21 new binary hook sites verified against the
  supported GOG executable.
* In a separate game copy: opened the third-slot native picker, added Deep
  Radar to an Air Superiority/Clean design, committed it, reopened it, saved,
  and reloaded. The mask stayed `0x10022`. A vehicle created from that prototype
  reported all three abilities through the game's ability-check hook.
* AI design checks: no new three-ability prototypes before unlocking; after
  unlocking, verified garrison `0x400300`, distant-front interceptor `0x8000022`,
  and supported former `0x12001`. Native/Thinker mineral-cost assertions passed.
* Multiplayer and a full campaign playthrough were not tested. The checklist
  below includes additional regressions still worth exercising.

## In-game regression checklist

Use a separate game installation and a debug build.

1. Before Neural Grafting: only slot 1 is usable. After Neural Grafting:
   slots 1 and 2. After Mind/Machine Interface: all three. In a new game,
   verify that the third-slot notification appears on discovery/acquisition.
2. Click row 3 to open the normal picker, then cancel. Existing selections,
   custom name, and mineral cost must remain unchanged.
3. Add, replace, and remove slot 3. Try changing slots 1 and 2 while slot 3
   is occupied. All three must be distinct. Carrier and Deep Pressure Hull
   must remain mutually exclusive.
4. Change chassis, weapon, armor, and reactor; verify invalid abilities are
   removed, missiles have none, and the displayed mineral cost stays correct.
5. Commit a named three-ability design, switch designs, reopen the Workshop,
   save, reload, build a unit, and upgrade an older unit to it. Check all three
   abilities and the name throughout. Repeat for a former and an aircraft.
6. Run AI turns with `design_units=1`, before and after the unlock. Inspect
   `create_proto` lines in `debug.txt` for three-bit masks on appropriate
   garrisons, long-range artillery ships, radar interceptors, and supported
   Super/Fungicidal formers. No three-slot designs should be newly proposed
   before the prerequisite technologies.
7. With `third_ability_tech=-1`, the original Workshop and 1/2-slot AI limits
   must return. Use a save without three-ability designs for this check.
8. For multiplayer, use identical binaries and INI settings on all machines;
   exercise designing, upgrading, exchanging designs, and save/reload.
