# Validation checks

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
The former automation tests also cover all four order IDs, the 21-tile home
radius, opposite-terrain replacement, technology and terrain restrictions,
and preservation of incompatible improvements. Movement tests use a simulated
map to exercise home/global targets, neutral and foreign ownership, unsafe
routes, reserved tiles, interrupted work, loss of the home base, and the
fallback search when the normal fixed-size tile search misses a target.

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

For event-window scaling with PRACX and Longevity Vaccine enabled, test at a
resolution above 1024×768. Convene the Planetary Council and check that the vote
buttons, portraits, and hitboxes scale together. Complete your first Secret
Project with Planetary Monuments enabled and check that the achievement plaque
fills a centered 4:3 area; also open the monument from the menu. Repeat without
Longevity Vaccine to confirm both screens still use their original layouts.
At 3840×2160 with Longevity Vaccine 1.0.0, triggering these windows in a copied
save confirmed the Council scaling wrapper ran and the achievement window
expanded from 1024 pixels to the full screen width, with the 4:3 art scaled
separately. These checks exercised the windows directly, not a full turn.
Both windows also opened with the unmodified GOG executable at 1024×768.

For the former planting orders, select a former and use `Ctrl+Shift+P` (also
available in the `Alt+T` menu). Test each of the four choices with player unit
management both on and off. Confirm home orders remain within that base's
21-tile working radius, including across a wrapping map edge; global orders
reach known owned and neutral tiles outside all base radii but not foreign
territory. Check land and sea formers, missing technologies, blocked/dangerous
routes, no available work, several formers, and a lost home base. Verify fungus
is removed before forest is planted and forest can become fungus without
removing other improvements. Test cancellation and changes while moving and
working, save/reload, and multiplayer synchronization with matching binaries.

The four menu choices were smoke-tested in a separate game copy with
`manage_player_units=0`: each stored the intended automation mode and started
the corresponding forest/fungus work. In an in-memory fixture with a reduced
terraforming rate, global fungus completed over a forest while preserving a
road and magtube. Normal-duration completion across turns, save/reload, and
multiplayer synchronization have not yet been verified in-game. One
exploratory run using rapid synthetic keystrokes exited unexpectedly; the
cause is unknown. A paced repeat on the same former opened and canceled the
menu without reproducing the exit.

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
