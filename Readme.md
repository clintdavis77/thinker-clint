
SMACX Thinker Mod
=================

Thinker is a remake of game engine features for Alpha Centauri: Alien Crossfire.
By patching the game to use an additional DLL, many new features can be developed in C++.

* Visual changes to map rendering and random map generation
* Additional game mechanics and more detailed config options for the game rules
* Numerous previously fixed settings in the game engine can be adjusted from the config
* More options for resolution settings, movie playback and other user interface features
* Entirely rewritten production and movement AI with minor changes to diplomacy
* Improved automation routines are also available for player factions
* Includes [Scient's patches](Details.md)

This mod is tested to work with the [GOG version](https://www.gog.com/game/sid_meiers_alpha_centauri) of Alpha Centauri.
Note that official Alien Crossfire patch version 2.0 must be installed for the launcher to work with terranx.exe.
Older game version 1.0 is NOT supported by Thinker. Installing Scient's patch v2 also works since this terranx.exe can be used by the launcher.
Thinker also includes changes to remove issues on Windows 11 that prevent playing the game expansion due to crashes after ending the turn.

[See more information](Details.md) about the features and recommended settings.
It's strongly recommended to read Details.md since many features are added not provided by the original game.
[Discuss here](https://github.com/induktio/thinker/discussions) about anything related to Thinker development.
Remember also to star and watch the repository to receive notifications about new updates.


Changes in this fork
--------------------
This fork picks up upstream Thinker at version 5.5 and continues its version numbering. The releases
below add the features listed here on top of the upstream mod. Full descriptions are in
[Details.md](Details.md) and release notes in the [Changelog](Changelog.md); this section summarises
everything the fork adds and names the release that introduced it.

### Special-ability slots in the Design Workshop (v5.6)
* Units can now be given a **third special ability**. Neural Grafting still unlocks the second slot; a
  third is unlocked by the technology set in `third_ability_tech` (Mind/Machine Interface by default),
  and both that technology and the rules-file two-ability prerequisite must be known.
* In the Design Workshop the special-ability panel now shows numbered rows **2** and **3**. Click row 3
  to pick or clear its ability with the normal picker, and right-click a row to see the technology it
  needs; a row that is not unlocked yet is drawn as *Locked*. The design summary also lists the third
  ability's abbreviation.
* The ability picker itself is unchanged: technology prerequisites, faction free abilities, component
  restrictions, the mutually exclusive Carrier and Deep Pressure Hull choices, missile restrictions and
  normal mineral costs all still apply to the third slot.
* Changing chassis, weapon, armor or reactor, or losing access to the third slot, removes abilities that
  no longer fit and moves remaining ones back into visible slots, so no ability is left hidden in the
  mask with no way to remove it.
* The extra ability uses the existing unit ability bitmask, so unit and savegame layouts are unchanged.
  Existing units are not given an ability for free, and existing saves gain access as soon as the
  technologies are known. Designs with three abilities must be edited with this version of Thinker:
  older builds and the original game may discard the extra ability. Use the same mod version and
  `third_ability_tech` setting on every multiplayer machine and when continuing a saved game.
* In single player, discovering or acquiring the third-slot technology shows a popup explaining the new
  slot.
* While the extension is enabled, the game also compares complete ability masks when matching an
  existing prototype, so a requested three-ability design is not silently replaced by an older one- or
  two-ability design.

### Three-ability AI designs (v5.6)
* With `design_units=1` the AI uses the same researched slot limit as the player, so it proposes
  three-ability designs only after the same technologies are known.
* AI garrisons may add a third defensive ability from the existing list, keeping the Police and Trained
  checks; long-range artillery ships that already carry AAA gain Deep Radar, radar-equipped interceptors
  gain Fuel Nanocells on distant or naval fronts, and Super or Fungicidal land formers gain Clean
  Reactor when support is not free.
* Production scoring weighs the extra abilities and range against their mineral cost, so the AI does not
  pay for a third ability it does not need.

### Facilities-only governor (v5.6)
* The base governor settings have a new **facilities only** priority. While it is selected the governor
  produces only ordinary base facilities and ignores units, secret projects and orbital satellites.
* Selecting it clears the four original priorities, since it replaces them instead of combining with
  them. It applies to player-owned bases managed by Thinker, not to AI bases.
* When no facility can be built the governor stockpiles energy and checks again every turn, so a facility
  unlocked by a later technology is started without manual intervention. A manual production queue is
  still respected, and a non-facility item is replaced rather than left in production.
* Base hurrying skips anything that is not a facility while this priority is set.

### Targeted forest and fungus former orders (v5.8)
* A former can be given one of four persistent orders to plant forest or fungus **near its home base**
  or **globally**, without replacing other improvements.
* Select one of your formers and press `Ctrl+Shift+P`, or choose **Former forest/fungus orders** from the
  `Alt+T` menu. The orders also work when Thinker management of other player units is disabled.
* Home-base orders stay within that base's 21-tile working radius and stop if the base is lost; the
  former then requests new orders instead of silently switching to global work. Global orders use
  known, reachable tiles in your own or unclaimed territory and never enter foreign territory.
* Planting may remove fungus before planting forest, and may replace forest with fungus, but never
  removes any other improvement. Technologies, terrain restrictions, unit movement type and tile safety
  are respected, and the former waits and checks again later when there is nothing eligible to do.
* Waking the former drops the planting order, so it is no longer in planting mode; automating it again
  selects normal full terraforming. The order is stored in the unit's existing automation field, so
  use the same mod version on all multiplayer machines and when continuing a saved game.

### Extra map zoom level with PRACX (v5.7, fixed in v5.7.1)
* When PRACX is active, Thinker inserts one additional map zoom level between PRACX levels 2 and 3.
  The other six levels keep the scales PRACX provides, and both the zoom keys and the mouse wheel use all
  seven levels.
* The extension checks the PRACX build before changing it and leaves an unknown version untouched, so a
  different PRACX release is never corrupted. Version 5.7.1 fixed this check, which had disabled the new
  level by misreading relocated PRACX addresses.
* The seventh level depends on the PRACX 1.11 build bundled with the GOG release (`prax.dll`). Thinker
  checks that build and extends only it, so any other PRACX release keeps its own zoom levels, unchanged.
* No message appears in normal play when the level is not added; the reason is written only to the
  debug log. Six zoom levels instead of seven therefore mean a PRACX version mismatch, not a Thinker bug.
* Updating PRACX takes precedence: the mod stays on the PRACX version shipped with the GOG release.

### Council and achievement screen scaling (v5.8.1)
* The Planetary Council vote window and the Planetary Monument achievement screen now scale correctly
  when a graphics patch such as Longevity Vaccine is installed, instead of opening undersized: Thinker
  follows the original call site rather than assuming its own internal function is the one the game
  calls, so the vote buttons, portraits, hitboxes and the achievement plaque scale together.

### New configuration and menu entries (v5.6, v5.8)
* `third_ability_tech=41` in thinker.ini selects the third-slot prerequisite. `41` is Mind/Machine
  Interface, any other value is a zero-based technology ID for custom tech trees, and `-1` disables the
  extension and restores the original two-slot limit and Workshop.
* The governor settings gain the facilities-only checkbox, and the `Alt+T` menu gains the former
  forest/fungus orders entry, a former-order dialog and a third-ability notification popup. Extra UI and
  popup labels are stored in `docs/modmenu.txt`, which must be deployed next to the DLL as usual.
* The current work text for these units shows the planting order (for example `[Forest near home]`),
  because the native automation labels have no entry for the new orders.

### Developer additions (v5.6, v5.8, v5.8.1)
* `tests/` adds standalone logic tests for the ability slots and the former planting orders (built as a
  separate CMake project, run with ctest), unit tests for the release script, and
  `tests/check_workshop_patches.py`, which verifies every new binary hook site against an unmodified
  supported game executable without modifying it. See `tests/README.md`.
* `tools/build_release.py` builds the DLL and the launcher from current sources with 32-bit MinGW,
  runs the tests, and writes a versioned release archive with checksums and a manifest describing the
  source commit, toolchain and validation status. `AGENTS.md` documents that packaging flow and the
  constraints of the third-ability slot for future contributors.


Download
--------
These links are the only original source to download binary releases. See also the [Changelog](Changelog.md) for useful release notes.

* [Release versions](https://www.dropbox.com/sh/qsps5bhz8v020o9/AAAp6ioWxdo7vnG6Ity5W3o1a?dl=0&lst=)
* [Develop builds](https://www.dropbox.com/sh/qsps5bhz8v020o9/AADv-0D0-bPq22pgoAIcDRC3a/develop?dl=0&lst=)


Installation
------------
1. Extract the files to Alpha Centauri game folder. Alphax.txt changes are optional.
2. Check Changelog.md and Details.md for useful information.
3. Change configuration from thinker.ini or just use the defaults.
4. Start the game from thinker.exe.
5. Press ALT+T to open Thinker's options menu. Other option is to check that mod version/build date is visible in the game version menu (CTRL+F4).
6. If neither of those options display mod version, Thinker is incorrectly installed and not loaded.


Troubleshooting
---------------
The launcher requires original Alien Crossfire v2.0 terranx.exe in the same folder but this file is not modified on disk.
In case of startup problems, make sure the official v2.0 patch is applied on the game, and the mod is actually started
from same folder with the game. Sometimes startup issues can be fixed by starting the launcher with administrator privileges.

When starting the game with screen scaling set to something else than 100% some portion of the window may not be visible due to being
clipped out of the screen. The game does not have proper support for screen scaling so as a workaround the scaling should be set to 100%.
Sometimes opening/secret project video playback errors might be fixed by installing [DDrawCompat](https://github.com/narzoul/DDrawCompat)
but otherwise it is not necessary.

The current GOG Alpha Centauri Planetary Pack (13 November 2024) may not work on Windows XP by default. When starting the game
there can be an error message "The application failed to start because dwmapi.dll was not found." To fix this simply remove ddraw.dll
from the game folder. This file was added on the latest GOG version and it is not necessary to run the game on Windows XP.

Alt-tabbing may sometimes not work by default in Alpha Centauri's GOG version. To re-enable alt-tab feature, follow these steps.
First open a command prompt with administrator privileges in Alpha Centauri's installation folder. After entering the commands
below alt-tabbing should now work when the game is restarted.

    sdbinst -u game.sdb
    sdbinst -u game_add.sdb

After installing the game on Windows there might be a notification that Windows Features can't complete the requested changes.
This might be caused by a failure to install DirectPlay. First open a command prompt with administrator privileges.
Then after entering the command below DirectPlay should be automatically installed.

    dism /online /Enable-Feature /FeatureName:DirectPlay /All


Other mods
----------
* [SMAC-in-SMACX mod](Details.md#smac-in-smacx-mod) can be installed to play a game similar to original SMAC while Thinker is enabled.
* [Scient's patch](https://github.com/DrazharLn/scient-unofficial-smacx-patch) combines many fixes for the original game.
* [OpenSMACX](https://github.com/b-casey/OpenSMACX) has been a long-term project to recreate SMACX game engine.
* Also see [compatibility with other mods such as PRACX](Details.md#compatibility-with-other-mods).


Compiling
---------
For information on how to compile Thinker, see [Technical.md](Technical.md).


License
-------
This software is licensed under the MIT License. Check [License.md](License.md) for detailed conditions.

The original game assets are not covered by this license and remain property of Firaxis Games Inc and Electronic Arts Inc.

Sid Meier's Alpha Centauri and Sid Meier's Alien Crossfire is Copyright © 1997, 1998 by Firaxis Games Inc and Electronic Arts Inc.
