# Console pipeline and native timecycle status

The PS2 and PSP GLSL files are desktop compatibility implementations, not shader
binaries extracted from the games. The same GLSL 1.20 sources are embedded for
Linux and Windows. Native textures and DTZ timecycle data remain the primary input.

Console fog uses linear eye-space depth with the decoded fog start and far clip.
Sunlight is supplied in eye space (`uLightEye`); normals receive the object's
normal matrix, while the light must not. Applying the object normal matrix to
sunlight causes the illumination to turn with each vehicle or prop. The console
fragment programs do not add a saturation adjustment or a tone curve to the
native timecycle colors.

The PS2 building/vegetation path scales 128-neutral prelight before ambient and
emissive modulation, clamps at the vertex stage, then modulates the texture. The
checked-in `vendors/relcs_mobile/leedsBuilding.vert` documents the building
pipeline's multiply/add/clamp ordering; it is a reconstruction reference, not
proof of every PS2 microcode coefficient. Texture/material effects and the
remaining PSP/dynamic-light coefficients still need platform-specific retail
captures. These changes do not claim complete pixel parity or recover unprovided
PSP executable data.

## Native data evidence

`StorylandNativeTimecycle` reads 24 hours by 8 weather samples from unpacked DTZ.
The pointer is at 0xB4 for VCS and 0xB0 for LCS; channel data starts 0x280 bytes
into the pointed block. Samples are indexed `hour * 8 + weather`. Colors use
separate byte arrays; the far clip and fog start use signed 16-bit arrays.
Cloud channels begin at byte-array slot 40 for VCS and 38 for LCS.

The supplied PS2 data was checked directly. These values identify those files,
not universal values across revisions or mods:

| Sample | VCS | LCS |
| --- | --- | --- |
| Unpacked table pointer | 0x5931E0 | 0x8C0 |
| Sunny midnight ambient | 119, 119, 119 | 156, 163, 171 |
| Sunny midnight sky top | 9, 11, 15 | 0, 0, 0 |
| Sunny midnight far clip / fog start | 1999 / 22 | 859 / 30 |
| Weather 3 noon far clip / fog start | 919 / -31 | 116 / -46 |

The loose `src/shaders/timecyc.dat` has a Sunny midnight far clip of 2200, whereas
the supplied VCS DTZ has 1999. A loaded native table therefore takes precedence
over loose text and editor fallback values. The native decoder deliberately
exposes only verified fields currently consumed by the sky/lighting renderer;
it does not claim recovered radiosity, postprocessing, or water effects.

## Checks

`python tests/console_shader_checks.py` creates a surfaceless Mesa desktop OpenGL
context, compiles/links every PS2/PSP material program, and reads pixels to check
fog, stationary sunlight under object rotation, and PS2 prelight saturation.
It requires Linux EGL/OpenGL libraries, without a desktop or external Python packages.

The portable C++ data checks can be run separately:

```sh
c++ -std=c++17 -Isrc/graphics tests/native_timecycle_checks.cpp src/graphics/storyland_native_graphics.cpp -o native_timecycle_checks
./native_timecycle_checks
```

Optionally pass unpacked VCS and LCS DTZ paths, in that order, to check the captured
retail sample values. The tests do not bundle game data.
