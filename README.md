# Storyland

This v28 source revision adds shared desktop RenderWare DFF/TXD viewing for GTA III, Vice City and San Andreas, navy accents for LCS, and each game's supplied error theme. It includes the v27 standalone depth fix and previous LCS/VCS parser repairs. See [RESEARCH_AND_VALIDATION.md](RESEARCH_AND_VALIDATION.md) for supported layouts and validation limits.

Storyland is a Windows workbench for Rockstar Leeds and RenderWare formats. It inspects and edits CHK, XTX, TEX, MDL, DFF, WBL, ANIM, IMG, LVZ, GAME.DTZ, and Vice City Stories SCM mission scripts. Desktop PC TXDs are supported for viewing and extraction; their authoring path remains read-only. It also previews SDT/RAW/VAG/WAV audio and PSS/common video files.

Release builds are portable: the active GLSL shaders and VCS timecycle are embedded in `Storyland.exe`, and zlib is linked statically. The executable does not require a `shaders` folder, `timecyc.dat`, or `zlib1.dll` beside it.

Use `build_msvc.bat` to create `build-portable\Release\Storyland.exe`. The separate build directory prevents an older dynamic-zlib CMake cache from being reused, and the script verifies the final PE dependency table before reporting success.

The build script locates vcpkg through `VCPKG_ROOT`, `C:\Projects\vcpkg`, or `vcpkg.exe` on `PATH`, and passes its CMake toolchain explicitly. The included manifest installs the static zlib package for the portable build.

This build adds:

- A VCS `main.scm` mission editor/browser with native Stories SCM header + mission-offset parsing. Individual missions are visible as raw binary ranges even without Sanny Builder. With Sanny configured, Storyland supports separate `vcs_ps2` and `vcs_psp` decompile/compile modes, editable per-mission source, editable full SCM source, mission-name lookup, and compilation back to `.scm`. See `README_VCS_SCM_MISSION_EDITOR.md`.
- A camera-centred LCS/VCS sky with time-of-day gradients, sun, moon, stars, moving cloud layers, fog, and four weather presets.
- VCS `timecyc.dat` parsing. The supplied PC-port timecycle is embedded directly in `Storyland.exe` as a Windows resource.
- Proper Gouraud shading using area-weighted shared-vertex normals and timecycle ambient/directional light. The OpenGL 1.x fallback uses the same smooth normals.
- Correct GAME.DTZ streaming-table boundaries. The embedded `gta3*.img` directory no longer loses its XTX names after `plr.xtx` / `plr.mdl`.
- PSP-native MDL vertex decoding with an aligned, fixed per-vertex stride derived from the vertex flags.
- A structured MDL inspector for model-family headers, clumps, atomics, RslGeometry, RslMaterial lists, RslTAnim/HAnim hierarchy entries, frame extensions, and PS2/PSP geometry stream headers.

Model and texture layouts are selected from the file contents. The normal MDL/DFF and texture workflows stay format-neutral.

## Build

Requirements:

- Windows 10 or newer
- Visual Studio 2022 with Desktop development with C++
- CMake 3.24 or newer (`build_msvc.bat` uses `cmake --fresh`)
- vcpkg with its Visual Studio/CMake toolchain available

Run `build_msvc.bat`, or configure manually:

```bat
cmake --fresh -S . -B build-portable -A x64 -DCMAKE_TOOLCHAIN_FILE=C:\Projects\vcpkg\scripts\buildsystems\vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static
cmake --build build-portable --config Release
```

The active shader and VCS timecycle are compiled into the executable. No runtime shader folder is copied.

## LVZ + IMG browser

Standalone viewing opens an LVZ with its same-stem IMG. It does not require GAME.DTZ.

- LCS uses WRLD resources; VCS additionally has AREA/AERA resource pools. The parser selects the game from the sector-directory layout.
- Master Resource[] rows are 8 bytes in LCS and 12 in VCS. Sector OverlayResource rows are 8 bytes in both games.
- Slave pointers include a 0x20-byte chunk header omitted from IMG. Resource payloads are bounded by the next distinct allocation.
- LCS PS2 raster descriptors are 8 bytes; VCS descriptors are 16 bytes. Indexed palettes follow the entire mip chain, and valid solid textures are retained.
- Model IDs come from resource tables. Texture material IDs and guessed absolute pointer forms are not used to invent model bindings.
- Geometry resolves through its owning sector, VCS streaming region, master resource, or validated linked WRLD table. Texture selection uses the selected geometry's container; identical shared copies can be reused safely.
- Models, LODs and SUPERLODs have separate tree branches. The overview displays detailed geometry; select an LOD/SUPERLOD placement or open its model alone to inspect it.
- Deliberate zero-mesh records are recognized as empty geometry rather than failed decodes or neighbouring models.
- GAME.DTZ discovers LVZ/IMG pairs in its own folder and its local MODELS, LEVELS and PS2 folders, including mixed-case directory names. Area loading is independent of a gta3 IMG companion.
- Resource export, placement editing and rollback-capable pair saving remain available. PS2 model and texture insertion now selects the LCS/VCS resource layout.
- In-place indexed texture replacement currently accepts single-level textures. Multi-level replacement is rejected rather than overwriting the mip chain or palette.

For portable parser checks without the Windows UI:

```sh
cmake -S . -B build-archive-tests -DSTORYLAND_ARCHIVE_TESTS_ONLY=ON
cmake --build build-archive-tests
ctest --test-dir build-archive-tests --output-on-failure
build-archive-tests/archive_inspect path/to/INDUST.LVZ path/to/mainla.lvz
build-archive-tests/loose_inspect path/to/platform-samples
```

The portable tools require C++17 and zlib. Supplied game assets are not redistributed in this source archive.

## Stories viewport

For 3D content, **View > Viewport** contains render mode and overlay controls, while **View > Sky** contains the LCS/VCS sky, time, and weather controls. Texture-only, media, and script views hide unrelated 3D options. The model renderer calculates lighting at each vertex and interpolates it across the triangle (Gouraud shading).

Controls:

- Left drag: orbit
- Right drag: pan
- Mouse wheel, `+`, `-`, Page Up, Page Down: zoom
- `W`, `S`, `A`, `D`, `Q`, `E`: move through world/archive views
- `[`: move the Stories sky back one hour
- `]`: move the Stories sky forward one hour
- `F`: fit closer
- `R` or `0`: reset view

## Model test

When an MDL or DFF is open, **Test Model** appears directly below the preview. It checks the model structure, geometry, hierarchy, skin data, relocation/pointer fields, companion textures, and PS2 DMA/VIF/GIF streams. Missing streams are reported as warnings; definite malformed ranges, invalid pointers, truncated packets, or unsafe geometry are errors. PSP-native geometry also receives its own bounded GE stream-range check.

Runtime-safe LVZ/IMG mesh replacement uses the same low-level range checks internally. GAME.DTZ and LVZ/IMG rebuild commands live in the relevant tree right-click menus rather than the general File menu.

When GAME.DTZ is open, right-click the tree and choose **Find...**. The search covers decoded DTZ resource names, fields, stream records, and—when the companion IMG is loaded—texture names parsed from internal XTX/CHK/TEX/TXD archives. Search results appear in a compact branch near the top of the tree. Double-clicking a texture-name result opens its containing archive and selects the matching texture.

Models and texture archives opened from GAME.DTZ keep a **Back to GAME.DTZ** action below the preview so the archive browser, Find results, and prior selection can be restored without reopening GAME.DTZ. The large raw sector-map branch starts collapsed, and its rows use shorter sector/size summaries; detailed offsets remain in the details pane.

## Media preview

Open `.sdt`, `.raw`, `.vag`, or `.wav` to inspect audio. SDT banks list their sounds in the tree; selecting a sound plays it and displays its waveform in the preview. SDT entries are range-checked against the matching RAW bank before decoding.

Open `.pss`, `.pmf`, `.mpg`, `.mpeg`, `.mp4`, `.m4v`, `.wmv`, `.avi`, `.mov`, or `.mkv` to preview video through the Windows media stack. PSS files are treated as MPEG program streams for playback. PSP PMF/PSMF files are validated, their embedded MPEG program-stream payload is exposed to the decoder, and both PSS and PMF can be paused and inspected one decoded frame at a time with Previous Frame / Next Frame. Media size, frame-size, sample-rate, chunk-range, and decoded-buffer limits are checked before allocation.

## GAME.DTZ + gta3 IMG

Open `GAME.DTZ`, then select its matching `gta3*.img` when prompted. Storyland reads the model, texture, and total streaming slot limits from the DTZ streaming-info header and applies each sentinel name table only to its own range.

Rebuild output is staged, verified, and committed transactionally with the companion IMG. Retail LCS/VCS do not use a `.dir`; optional `.dir` reading/writing is retained only for beta-build archives where one was explicitly loaded.

When an IMG resource changes size, Storyland updates both the main `CStreamingInfo` sector table and the compact named-resource directory used by special/cutscene assets. Later ranges are shifted by the same 2048-byte-sector delta. Rebuild is refused if the resulting internal ranges overlap or extend past the companion IMG, preventing a partially shifted GAME.DTZ from being written.

## Credits

Original Storyland by spicybung: <https://github.com/spicybung>

The PSP vertex-layout cleanup was adapted from the supplied BLeeds reference implementation.
