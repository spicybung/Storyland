import argparse
import os
from pathlib import Path
import shutil
import sys
import subprocess
import zipfile
import xml.etree.ElementTree as ET


def prepare_dependencies(root, output):
    sys.dont_write_bytecode = True
    sys.path.insert(0, str(root / "cmake"))
    from storyland_vendor_sources import stage_dependencies
    return stage_dependencies(root, output, ["sdl2", "ffmpeg-source", "ffmpeg-android"])


def vcs_loading_artwork_files(root):
    directory = root / "assets/loading/vcs"
    names = ["loadsc" + str(index) for index in range(12)]
    names.extend(("outro", "title_pc_EN", "title_pc_ES", "title_pc_PL"))
    files = [directory / (name + ".xtx") for name in names]
    for path in files:
        if not path.is_file():
            raise RuntimeError("Required VCS loading artwork is missing: " + str(path))
    return files


def stage_vcs_loading_artwork(root, asset_directory):
    files = vcs_loading_artwork_files(root)
    destination = asset_directory / "loading/vcs"
    destination.mkdir(parents=True, exist_ok=True)
    (destination / "intro.xtx").unlink(missing_ok=True)
    for path in files:
        shutil.copy2(path, destination / path.name)


def verify_vcs_loading_artwork(root, archive):
    for path in vcs_loading_artwork_files(root):
        name = "assets/loading/vcs/" + path.name
        if name not in archive.namelist() or archive.read(name) != path.read_bytes():
            raise RuntimeError("Missing or altered VCS loading artwork in the APK: " + path.name)


def pc_artwork_files(root):
    directory = root / "assets/pc"
    manifest = directory / "manifest.txt"
    if not manifest.is_file():
        raise RuntimeError("Required PC artwork manifest is missing: " + str(manifest))
    names = manifest.read_text(encoding="utf-8").splitlines()
    if not names or len(names) != len(set(name.casefold() for name in names)):
        raise RuntimeError("The PC artwork manifest is empty or contains duplicate paths.")
    files = [manifest]
    for name in names:
        relative = Path(name)
        if not name or relative.is_absolute() or ".." in relative.parts or ":" in name or "\\" in name or relative.suffix != ".xtx":
            raise RuntimeError("Invalid PC artwork path: " + name)
        path = directory / relative
        if not path.is_file():
            raise RuntimeError("Required PC artwork is missing: " + str(path))
        files.append(path)
    return files


def stage_pc_artwork(root, asset_directory):
    directory = root / "assets/pc"
    for path in pc_artwork_files(root):
        destination = asset_directory / "pc" / path.relative_to(directory)
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, destination)


def verify_pc_artwork(root, archive):
    directory = root / "assets/pc"
    for path in pc_artwork_files(root):
        name = "assets/pc/" + path.relative_to(directory).as_posix()
        if name not in archive.namelist() or archive.read(name) != path.read_bytes():
            raise RuntimeError("Missing or altered PC artwork in the APK: " + name)


def movie_asset_files(root):
    directory = root / "assets/movies"
    files = [directory / name for name in ("intro_pc.pss", "reigns_studio.pss", "manifest.json")]
    for path in files:
        if not path.is_file():
            raise RuntimeError("Required Storyland movie asset is missing: " + str(path))
    readme = directory / "README.md"
    if readme.is_file():
        files.append(readme)
    return files


def stage_movie_assets(root, asset_directory):
    files = movie_asset_files(root)
    destination = asset_directory / "movies"
    destination.mkdir(parents=True, exist_ok=True)
    for path in files:
        shutil.copy2(path, destination / path.name)


def verify_movie_assets(root, archive):
    entries = archive.infolist()
    for path in movie_asset_files(root):
        name = "assets/movies/" + path.name
        matches = [entry for entry in entries if entry.filename == name]
        if len(matches) != 1:
            raise RuntimeError("Missing or duplicate Storyland movie asset in the APK: " + name)
        entry = matches[0]
        if entry.compress_type not in (zipfile.ZIP_STORED, zipfile.ZIP_DEFLATED):
            raise RuntimeError("Unsupported compression for a Storyland movie asset in the APK: " + name)
        if entry.file_size != path.stat().st_size:
            raise RuntimeError("Altered Storyland movie asset in the APK: " + name)
        with path.open("rb") as source, archive.open(entry) as packaged:
            while True:
                original = source.read(1024 * 1024)
                copied = packaged.read(1024 * 1024)
                if original != copied:
                    raise RuntimeError("Altered Storyland movie asset in the APK: " + name)
                if not original:
                    break


def run(arguments, **options):
    print(" ".join(str(argument) for argument in arguments), flush=True)
    subprocess.run([str(argument) for argument in arguments], check=True, **options)


def executable(name, directory=None):
    candidates = [name + suffix for suffix in (".exe", ".bat", ".cmd", "")] if os.name == "nt" else [name]
    for candidate in candidates:
        if directory is not None:
            path = directory / candidate
            if path.is_file():
                return path
        else:
            found = shutil.which(candidate)
            if found:
                return Path(found)
    raise RuntimeError("Required build command is missing: " + name)


def newest(directory):
    if not directory.is_dir():
        raise RuntimeError("Required Android SDK directory is missing: " + str(directory))
    folders = [path for path in directory.iterdir() if path.is_dir() and all(part.isdigit() for part in path.name.split("."))]
    if not folders:
        raise RuntimeError("No installed version found in " + str(directory))
    return max(folders, key=lambda path: tuple(int(part) for part in path.name.split(".")))


def find_sdk(provided):
    paths = []
    if provided:
        paths.append(Path(provided).expanduser())
    for name in ("ANDROID_SDK_ROOT", "ANDROID_HOME"):
        value = os.environ.get(name)
        if value:
            paths.append(Path(value).expanduser())
    if os.name == "nt":
        local_app_data = os.environ.get("LOCALAPPDATA")
        if local_app_data:
            paths.append(Path(local_app_data) / "Android" / "Sdk")
    else:
        paths.append(Path.home() / "Android" / "Sdk")

    checked = set()
    for path in paths:
        resolved = path.resolve()
        if resolved in checked:
            continue
        checked.add(resolved)
        if (resolved / "platforms").is_dir() or (resolved / "ndk").is_dir():
            return resolved

    attempted = ", ".join(str(path) for path in paths) if paths else "no SDK paths known"
    raise RuntimeError(
        "Android SDK not found. Install the Android SDK using Android Studio's "
        "SDK Manager, or run build_android.bat --sdk <SDK_DIRECTORY>. "
        "Locations checked: " + attempted
    )


def configure_tool_paths(sdk):
    executable_paths = []
    cmake_directory = sdk / "cmake"
    if cmake_directory.is_dir():
        try:
            executable_paths.append(newest(cmake_directory) / "bin")
        except RuntimeError:
            pass

    java_home = os.environ.get("JAVA_HOME")
    if java_home:
        executable_paths.append(Path(java_home) / "bin")

    if os.name == "nt":
        program_files = os.environ.get("ProgramFiles")
        if program_files:
            executable_paths.append(Path(program_files) / "Android" / "Android Studio" / "jbr" / "bin")

    existing = [str(path) for path in executable_paths if path.is_dir()]
    if existing:
        os.environ["PATH"] = os.pathsep.join(existing) + os.pathsep + os.environ.get("PATH", "")


def check_build_requirements(sdk, ndk, tools, android, skip_native):
    requirements = [
        (tools, 'Android SDK Build-Tools 35.0.0 or later'),
        (android, 'Android 15 / API 35 platform (SDK Manager: "platforms;android-35")'),
        (ndk / "build/cmake/android.toolchain.cmake", 'Android NDK (SDK Manager: "ndk;27.2.12479018")'),
    ]
    missing = []
    for path, description in requirements:
        if not path.exists():
            missing.append("  - " + description + "\n    Expected: " + str(path))
    if missing:
        raise RuntimeError("Missing required Android SDK components:\n" + "\n".join(missing))

    for command in ("javac", "cmake" if not skip_native else None, "ninja" if not skip_native else None):
        if command is None:
            continue
        try:
            executable(command)
        except RuntimeError:
            missing.append("  - " + command + " executable (install Java JDK and CMake/Ninja through Android Studio)")
    if len(missing) > 0:
        raise RuntimeError("Missing tools for the Android build:\n" + "\n".join(missing))


def print_build_info(root, sdk, ndk, tools, output, abi):
    print("Storyland Android build configuration:", flush=True)
    print("  Source:      " + str(root), flush=True)
    print("  SDK:         " + str(sdk), flush=True)
    print("  NDK:         " + str(ndk), flush=True)
    print("  Build tools: " + str(tools), flush=True)
    print("  ABI:         " + abi, flush=True)
    print("  Output:      " + str(output), flush=True)
    print("", flush=True)


def build():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description="Build the Storyland Android preview APK.")
    parser.add_argument("--sdk", default=os.environ.get("ANDROID_SDK_ROOT") or os.environ.get("ANDROID_HOME"))
    parser.add_argument("--ndk", type=Path)
    parser.add_argument("--build-tools", help="Installed SDK Build-Tools version; defaults to the newest version.")
    parser.add_argument("--abi", choices=["arm64-v8a", "x86_64"], default="arm64-v8a")
    parser.add_argument("--output", type=Path, default=root / "build-android")
    parser.add_argument("--jobs", type=int, default=2)
    parser.add_argument("--native-build", type=Path)
    parser.add_argument("--skip-native", action="store_true")
    signing_group = parser.add_mutually_exclusive_group()
    signing_group.add_argument("--debug-sign", action="store_true",
                               help="Use the local development signing key and a separate package ID.")
    signing_group.add_argument("--release-sign", action="store_true",
                               help="Require the private Reigns Studio release signing identity.")
    args = parser.parse_args()
    release_key = root / "android/signing/reigns-studio-release.p12"
    release_password_file = root / "android/signing/reigns-studio-password.txt"
    release_password = os.environ.get("REIGNS_STUDIO_KEYSTORE_PASSWORD", "")
    if not release_password and release_password_file.is_file():
        release_password = release_password_file.read_text(encoding="utf-8").strip()

    if args.release_sign:
        if not release_key.is_file():
            raise RuntimeError("Reigns Studio signing key is missing: " + str(release_key))
        if not release_password:
            raise RuntimeError("Reigns Studio signing password is missing. Set REIGNS_STUDIO_KEYSTORE_PASSWORD.")
    elif not args.debug_sign:
        args.debug_sign = not (release_key.is_file() and bool(release_password))

    if args.debug_sign:
        print("Signing: local development key (independent package: org.reignsstudio.storyland.crossplay)", flush=True)
        print("This source distribution has no Reigns Studio release signing key; a development key will be generated if needed.", flush=True)
    else:
        print("Signing: private Reigns Studio release identity", flush=True)
    android_namespace = "http://schemas.android.com/apk/res/android"
    manifest_source = root / "android/AndroidManifest.xml"
    manifest_document = ET.parse(manifest_source)
    version = manifest_document.getroot().get("{" + android_namespace + "}versionName")
    sourceVersion = (root / "STORYLAND_VERSION.txt").read_text(encoding="utf-8").strip()
    versionParts = sourceVersion.split(".")
    if len(versionParts) != 3 or not all(part.isascii() and part.isdigit() for part in versionParts):
        raise RuntimeError("STORYLAND_VERSION.txt must contain a numeric major.minor.patch version.")
    major, minor, patch = (int(part) for part in versionParts)
    if major > 2146 or minor > 99 or patch > 99:
        raise RuntimeError("The version exceeds Storyland's Android versionCode component ranges.")
    versionCode = str(major * 1000000 + minor * 10000 + patch * 100)
    manifestVersionCode = manifest_document.getroot().get("{" + android_namespace + "}versionCode")
    if version != sourceVersion or manifestVersionCode != versionCode:
        raise RuntimeError("AndroidManifest.xml versionName/versionCode must match STORYLAND_VERSION.txt.")
    sdk = find_sdk(args.sdk)
    ndk = args.ndk.resolve() if args.ndk else newest(sdk / "ndk")
    tools = sdk / "build-tools" / args.build_tools if args.build_tools else newest(sdk / "build-tools")
    if tuple(int(part) for part in tools.name.split(".")) < (35, 0, 0):
        raise RuntimeError("Android SDK Build-Tools 35.0.0 or later is required for 16 KB APK alignment.")
    android = sdk / "platforms" / "android-35" / "android.jar"
    configure_tool_paths(sdk)
    check_build_requirements(sdk, ndk, tools, android, args.skip_native)
    output = args.output.resolve()
    print_build_info(root, sdk, ndk, tools, output, args.abi)
    output.mkdir(parents=True, exist_ok=True)
    native = args.native_build.resolve() if args.native_build else output / ("native-" + args.abi)
    dependencies = prepare_dependencies(root, output)
    ffmpeg = dependencies["ffmpeg-android"] / args.abi
    if not (ffmpeg / "lib/libavcodec.a").is_file():
        ffmpeg = output / "ffmpeg" / args.abi
    if not args.skip_native:
        if not (ffmpeg / "lib/libavcodec.a").is_file():
            run([executable("python3" if os.name != "nt" else "python"), root / "android/build_ffmpeg.py",
                 "--ndk", ndk, "--abi", args.abi, "--jobs", max(1, args.jobs),
                 "--dependency-build-directory", output, "--output", ffmpeg,
                 "--build", output / ("ffmpeg-build-" + args.abi)])
        run([executable("cmake"), "-S", root, "-B", native, "-G", "Ninja",
            "-DCMAKE_TOOLCHAIN_FILE=" + str(ndk / "build/cmake/android.toolchain.cmake"),
            "-DCMAKE_SYSTEM_NAME=Android",
            "-DCMAKE_SUPPRESS_REGENERATION=ON",
            "-DANDROID_ABI=" + args.abi, "-DANDROID_PLATFORM=android-26", "-DANDROID_STL=c++_static",
            "-DSTORYLAND_ANDROID_FFMPEG=" + str(ffmpeg),
            "-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON", "-DCMAKE_BUILD_TYPE=Release"])
        run([executable("cmake"), "--build", native, "--parallel", max(1, args.jobs)])
    native_library = native / "libmain.so"
    if not native_library.is_file():
        raise RuntimeError("Native Android library missing. Run the full build, not --skip-native.")
    with native_library.open("rb") as compiled_library:
        if b"Host IPv4" not in compiled_library.read():
            raise RuntimeError("The native library predates crossplay. Run the full build again.")
    stage = output / ("package-" + args.abi)
    if stage.exists():
        shutil.rmtree(stage)
    classes = stage / "classes"
    dex = stage / "dex"
    resources = stage / "res" / "drawable"
    assets = stage / "assets" / "sannybuilder"
    for directory in (classes, dex, resources, assets):
        directory.mkdir(parents=True)
    shutil.copy2(root / "android/res/drawable/icon.png", resources / "icon.png")
    for name in ("LCSSCM.INI", "VCSSCM.INI"):
        shutil.copy2(root / "vendors/sannybuilder" / name, assets / name)
    shutil.copy2(root / "LICENSE", stage / "assets/LICENSE-Storyland")
    shutil.copy2(dependencies["sdl2"] / "LICENSE.txt", stage / "assets/LICENSE-SDL2")
    shutil.copy2(root / "vendors/imgui/LICENSE.txt", stage / "assets/LICENSE-ImGui")
    shutil.copy2(dependencies["ffmpeg-source"] / "COPYING.LGPLv2.1", stage / "assets/LICENSE-FFmpeg")
    for asset in (root / "android/assets").iterdir():
        if asset.is_file(): shutil.copy2(asset, stage / "assets" / asset.name)
    radio = stage / "assets" / "radio"
    radio.mkdir()
    shutil.copy2(root / "assets/radio/usertracks.xtx", radio / "usertracks.xtx")
    stage_vcs_loading_artwork(root, stage / "assets")
    stage_pc_artwork(root, stage / "assets")
    stage_movie_assets(root, stage / "assets")
    music = stage / "assets" / "music"
    music.mkdir()
    shutil.copy2(root / "src/resources/reigns_intro.mp3", music / "reigns_intro.mp3")
    shutil.copy2(root / "src/resources/Strike It Up [8 Bit Tribute to Black Box] - 8 Bit Universe.mp3", music / "special_thanks.mp3")
    sources = sorted((root / "android/java").rglob("*.java"))
    sources += sorted((dependencies["sdl2"] / "android-project/app/src/main/java").rglob("*.java"))
    source_list = stage / "java-sources.txt"
    source_list.write_text("\n".join('"' + path.as_posix() + '"' for path in sources), encoding="utf-8")
    run([executable("javac"), "-encoding", "UTF-8", "-source", "8", "-target", "8", "-bootclasspath", str(android) + os.pathsep + str(tools / "core-lambda-stubs.jar"),
         "-d", classes, "@" + str(source_list)])
    jar = stage / "classes.jar"
    with zipfile.ZipFile(jar, "w") as archive:
        for path in classes.rglob("*.class"):
            archive.write(path, path.relative_to(classes).as_posix())
    run([executable("d8", tools), "--lib", android, "--min-api", "26", "--output", dex, jar])
    unsigned = stage / "unsigned.apk"
    manifest = manifest_source
    if args.debug_sign:
        ET.register_namespace("android", android_namespace)
        document = ET.parse(manifest)
        document.getroot().set("package", "org.reignsstudio.storyland.crossplay")
        for element in document.findall(".//application") + document.findall(".//activity"):
            element.set("{" + android_namespace + "}label", "Storyland")
        manifest = stage / "AndroidManifest.xml"
        document.write(manifest, encoding="utf-8", xml_declaration=True)
    run([executable("aapt", tools), "package", "-f", "-0", "mp3", "-M", manifest, "-I", android,
         "-S", stage / "res", "-A", stage / "assets", "-F", unsigned])
    host_tools = ndk / "toolchains/llvm/prebuilt"
    strip_tools = [path for path in host_tools.glob("*/bin/llvm-strip*") if path.name in ("llvm-strip", "llvm-strip.exe")]
    if len(strip_tools) != 1:
        raise RuntimeError("Cannot locate the NDK llvm-strip tool.")
    with zipfile.ZipFile(unsigned, "a", compression=zipfile.ZIP_DEFLATED) as archive:
        for path in dex.glob("*.dex"):
            archive.write(path, path.name)
        for name, path in (("libmain.so", native / "libmain.so"), ("libSDL2.so", native / "vendors/SDL2/libSDL2.so")):
            if not path.is_file():
                raise RuntimeError("Native library was not built: " + str(path))
            packaged = stage / name
            shutil.copy2(path, packaged)
            run([strip_tools[0], "--strip-unneeded", packaged])
            archive.write(packaged, "lib/" + args.abi + "/" + name, compress_type=zipfile.ZIP_STORED)
    aligned = stage / "aligned.apk"
    run([executable("zipalign", tools), "-P", "16", "-f", "4", unsigned, aligned])
    if args.debug_sign:
        key = output / "crossplay-debug.p12"
        alias = "androiddebugkey"
        password = "android"
        if not key.is_file():
            run([executable("keytool"), "-genkeypair", "-keystore", key, "-storetype", "PKCS12",
                 "-alias", alias, "-storepass", password, "-keypass", password,
                 "-dname", "CN=Storyland Android", "-keyalg", "RSA", "-keysize", "2048", "-validity", "3650"])
        apk = output / ("Storyland-" + version + "-Android-" + args.abi + ".apk")
    else:
        key = release_key
        alias = "reigns-studio"
        password = release_password
        apk = output / ("Storyland-" + version + "-ReignsStudio-Android-" + args.abi + ".apk")
    signing_environment = dict(os.environ, REIGNS_STUDIO_KEYSTORE_PASSWORD=password)
    run([executable("apksigner", tools), "sign", "--ks", key, "--ks-type", "PKCS12",
         "--ks-key-alias", alias,
         "--ks-pass", "env:REIGNS_STUDIO_KEYSTORE_PASSWORD",
         "--key-pass", "env:REIGNS_STUDIO_KEYSTORE_PASSWORD",
         "--v1-signing-enabled", "true", "--v2-signing-enabled", "true",
         "--v3-signing-enabled", "true", "--out", apk, aligned], env=signing_environment)
    run([executable("apksigner", tools), "verify", "--verbose", "--print-certs", apk])
    run([executable("zipalign", tools), "-c", "-P", "16", "4", apk])
    with zipfile.ZipFile(apk) as archive:
        if archive.testzip() is not None:
            raise RuntimeError("APK integrity check failed.")
        verify_vcs_loading_artwork(root, archive)
        verify_pc_artwork(root, archive)
        verify_movie_assets(root, archive)
        built_music = archive.read("assets/music/reigns_intro.mp3")
        if built_music != (root / "src/resources/reigns_intro.mp3").read_bytes():
            raise RuntimeError("Wrong Reigns Studio intro music in the APK.")
        built_native = archive.read("lib/" + args.abi + "/libmain.so")
        if b"Can't unpack LVZ at byte " not in built_native:
            raise RuntimeError("The APK contains the old LVZ reader.")
        if b"Unknown IMG layout" not in built_native:
            raise RuntimeError("The APK is missing the gta3PS IMG file browser.")
        java_strings = archive.read("classes.dex")
        if b"Find folder" not in java_strings:
            raise RuntimeError("The APK contains the old Android file picker.")
        if b"GAME.DTZ not found" not in java_strings and b"Other folder" not in java_strings:
            raise RuntimeError("The APK is missing the gta3PS IMG/DTZ file picker.")
    print("Built: " + str(apk))


if __name__ == "__main__":
    try:
        build()
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print("BUILD ERROR: " + str(error), file=sys.stderr, flush=True)
        raise SystemExit(1)
