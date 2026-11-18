# VCS PS2 Lua Modloader

Storyland bundles these files in `assets/modloader`. A mod folder installed in
the VCS PS2 game directory lives at `modloader/<mod name>`; the application
copy remains available for other game installations.

Copy the `modloader` directory into the game directory containing `GAME.DTZ`
(or the parent of its `PS2`, `DATA` or `MODELS` directory). Storyland mounts it
when it opens a verified VCS PS2 archive. LCS and PSP files do not activate it.
The original game files are never rewritten by mounting a mod.

Each child directory is one mod. Put replacement files directly inside it or
mirror their retail subdirectories, for example:

```
modloader/my-assets/GAME.DTZ
modloader/my-assets/MODELS/gta3PS2.img
modloader/my-assets/MODELS/gta3PS2.dir
modloader/my-assets/MOVIES/intro.pss
modloader/my-assets/MOVIES/stings.pss
modloader/my-assets/mod.lua
```

File matching ignores case, including on Linux. A basename request also searches
subdirectories of each mod. A request with a retail directory uses that exact
relative path or a replacement placed at the mod's root; a file in another
subdirectory cannot override it just because the names match. Within one mod,
the exact relative path wins over the root-level fallback. Explicit
`storyland.replace` declarations take precedence over automatic placement, with
an exact relative declaration winning over a basename declaration. Supply a
complete matching IMG when replacing GAME.DTZ with a sector map from a different
build.

`gta3PS2.img` and `gta3PS2.dir` are the retail PS2 names. The shorthand filenames
`gtaps2.img` and `gtaps2.dir` also work inside mods as fallback aliases. Within
one mod, the exact retail filename wins over its shorthand alias. Mod priority
still applies between mods. A PSP image is never substituted for a PS2 image.

Enabling Extended + SA Features in the Windows app offers to install the bundled
folder into the detected VCS PS2 game directory. Installation stages the bundle
before moving it into place. Existing user mod folders and files are preserved;
only missing bundled entries are added. Reopen GAME.DTZ after installation to
mount the new data. The installer does not modify retail archives.

Mods are ordered by ascending `priority`, then case-insensitive directory name
(original directory spelling breaks equal-name ties). The last mod wins file
conflicts. Lua byte patches run in that same order. Later gang data replaces
the earlier gang definition as a whole. Place a `.disabled` file inside a mod
to disable it before Lua is read, or use `enabled = false` in `storyland.mod`.
Reload the archive after changing files or Lua; each load rereads the source
and advances the archive content revision used by model/texture consumers.
Remounting the loader validates every mod before replacing the previous mount.
A failed manifest therefore cannot leave a partially updated override list.

Lua 5.4.8 executes `mod.lua`, with real functions, loops, tables, arithmetic,
and base/table/string/math/utf8 libraries. File IO, package loading, OS access,
debug access and dynamic code loading are unavailable. Manifests have a 1 MiB
source limit, 16 MiB Lua memory budget and 1,000,000 instruction loading budget.
Load failures report the script filename/line or the failing patch offset.

```lua
storyland.mod { name = "My VCS PS2 edit", priority = 200, enabled = true }
assert(storyland.target == "vcs-ps2")
storyland.replace("gta3PS2.img", "replacement/world.img")
```

The DTZ patch API addresses **decompressed GAME.DTZ byte offsets**, using
little-endian values. It never patches the compressed zlib wrapper. Supported
calls are `patch_bytes(offset, bytes, expected_bytes)`, `patch_u8`, `patch_u16`,
`patch_u32` and `patch_f32`; numeric calls accept `(offset, value, expected)`.
Byte arrays or binary Lua strings are accepted by `patch_bytes`. The expected
argument is optional, but should be supplied for build-specific edits.
All patches are checked and applied transactionally before native data is
parsed again. A range or expected-value failure leaves the supplied DTZ buffer
unchanged and prevents Play Game from using the failed load.

`mainland-gangs/mod.lua` is the bundled additional mainland gang modification.
It supplies Haitian models A/B as **BMYCR/BMYBB**, native pistol/SMG weapon
slots 3/5, health/armour, 20 initial ownership cells, income and zone exclusions.
The engine resolves its normalized rectangles against the loaded retail
HAITI/HAVANA/DTOWN/DOCKS zones; it excludes complete cells intersecting protected
airport, army-base or Starfish zones. It requires both model assets to exist in
the effective IMG, such as an IMG replacement supplied by another mod.
This is additional mod content. It preserves the native Cholo/Vance identities,
the nine native SCM gang slots and the police population kind; the extra
Haitian population uses kind 10 and pedestrian type 32.

`storyland.gang_turfs` accepts `population_kind`, `pedestrian_type`, `models`,
`health`, `armour`, `weapon_slots`, `excluded_zones`, and `territories` as shown
in the supplied script. Each territory has `id`, `name`, `district_zone`,
`owner` (`cholos`, `haitians` or `vance`), normalized `bounds`
`{xmin,ymin,xmax,ymax}` and `daily_income`. IDs are unique, bounds must have
positive area within 0..1, at most 128 territories are accepted, and normalized
rectangles must not overlap after native zone resolution. Without an enabled
gang data mod, the additional faction and territory system are absent.
