# Storyland 1.3.6 for Android

The ARM64 APK targets phones such as the Galaxy S23 FE. It requires Android
8.0 or newer and OpenGL ES 3.0. Original game files are supplied separately.

## Install and open a game

Install `Storyland-1.3.6-Android-arm64-v8a.apk`, then use **Open → Game folder**
to select your LCS or VCS files. Preserve the original directory names when
selecting script, animation, audio and map archives. Use **Play → Play Game**
for missions or **Play → Play Test** for exploration.

The source distribution does **not** contain the private Reigns Studio release signing
key. Builds automatically select a local development key when no complete release
signing identity is configured, so double-clicking `build_android.bat` creates an
installable APK rather than failing after native compilation. Development builds
use `org.reignsstudio.storyland.crossplay` and install separately from builds using
the preview package ID. Their launcher label is **Storyland**, and their version
is **1.3.6** (`1030600`). This source release does not include a prebuilt APK.

## Co-op controls

The **More → Co-op** panel opens host/join controls. Both players need the
same game and compatible co-op protocol versions. The 1.3.6 version bump does
not change the existing co-op packet layout. The **Play** tab also contains
**Play Options → Co-op → Friendly Fire → On/Off**. The host controls this
setting.

Use **More → Chat** for the on-screen keyboard. A hardware keyboard can open
chat with **T**, send with **Enter**, and close with **Escape** or Android's
Back action. Typing blocks gameplay input. **R** reloads on foot and changes
radio stations while driving. The touch and gamepad controls remain available.

## Build

Required tools are a JDK, CMake, Ninja, Android API 35, Build-Tools 35.0.0 or
newer, and an Android NDK. The prior ARM64 build used NDK r28c and
Build-Tools 35.0.1; that is historical toolchain information, not a newly
executed 1.3.6 Android build. SDL2, the Android FFmpeg static libraries, their
source and licenses are included directly in this source package:

| Source directory | Android use |
| --- | --- |
| `vendors/SDL2/` | Native SDL2 library and Android Java classes. |
| `vendors/imgui/` | Dear ImGui and its SDL2/OpenGL ES backends. |
| `vendors/ffmpeg/source/` | FFmpeg source and license used for decoder rebuilds. |
| `vendors/ffmpeg/android/` | Included per-ABI decoder libraries and headers. |
| `src/re/reLCS2/`, `src/re/reVCS/` | Game modules shared with the desktop builds. |

The builder verifies the selected vendor files and uses them in place. It
does not extract dependency archives into the build directory. Generated
native libraries, APK staging files and signing keys remain under the
selected output directory.

```bash
python3 android/build_apk.py --sdk "$ANDROID_SDK_ROOT" \
  --abi arm64-v8a --debug-sign --jobs 4
```

`--ndk`, `--build-tools`, `--output`, and `--native-build` select explicit
toolchain and output directories. The builder checks `AndroidManifest.xml`
versionName and versionCode against `STORYLAND_VERSION.txt` before building,
including `--skip-native` runs. CMake derives the native version from the same
source file. APK names use that verified version. Native libraries and their uncompressed APK entries
use 16 KB alignment. The builder verifies signatures, alignment and archive
integrity before reporting success.

To rebuild the FFmpeg libraries from the included source, use a separate
output directory:

```bash
python3 android/build_ffmpeg.py --ndk "$ANDROID_NDK_ROOT" \
  --abi arm64-v8a --build build-android/ffmpeg-build-arm64-v8a \
  --output build-android/ffmpeg/arm64-v8a --jobs 4
```

For a direct CMake build, select that installation using
`-DSTORYLAND_ANDROID_FFMPEG=<installation directory>`. The
`--dependency-build-directory` argument remains available as a compatibility
option for selecting the default generated-library installation root; it
does not relocate the checked-in FFmpeg source.

The development signing identity is stored in `crossplay-debug.p12` under
the selected output directory. Retain that private file for subsequent
updates to the same installation. The builder picks release signing only when
`android/signing/reigns-studio-release.p12` exists **and** its password is set
through `REIGNS_STUDIO_KEYSTORE_PASSWORD` or in
`android/signing/reigns-studio-password.txt`. Use `--debug-sign` to force the
development identity, or `--release-sign` to require release signing and fail
immediately if the private key or password is missing. These two flags are
mutually exclusive. Do not ship the release keystore or its password in source
archives.

## Validation

The APK builder checks signatures, ZIP integrity, native-library presence
and 16 KB alignment. Those packaging checks do not establish rendering or
gameplay behavior on an Android device. The retained graphics and runtime
fixtures are documented in `tests/README.md` from the repository root.

This directory reorganization does not include a newly built APK or a new
physical-device validation result. Historical Android validation remains
separate from the current package's build and source-layout checks.
