import argparse
from pathlib import Path
import platform
import shutil
import subprocess
import sys


FFMPEG_BUILD_SCRIPTS = (
    "configure",
    "ffbuild/version.sh",
    "ffbuild/libversion.sh",
    "ffbuild/pkgconfig_generate.sh",
)


def prepare_ffmpeg_build_scripts(source, bash):
    scripts = [source / name for name in FFMPEG_BUILD_SCRIPTS]
    for script in scripts:
        if not script.is_file():
            raise RuntimeError("Required FFmpeg build script is missing: " + str(script))
    try:
        subprocess.run([bash, "-c", 'chmod a+x -- "$@"', "Storyland FFmpeg permissions",
                        *(script.as_posix() for script in scripts)], check=True)
    except (OSError, subprocess.CalledProcessError) as error:
        raise RuntimeError("Cannot make the required FFmpeg build scripts executable: " + str(error)) from error


def build():
    parser = argparse.ArgumentParser(description="Build Storyland's Android movie and audio decoder libraries.")
    parser.add_argument("--ndk", type=Path, required=True)
    parser.add_argument("--abi", choices=["arm64-v8a", "x86_64"], default="arm64-v8a")
    parser.add_argument("--jobs", type=int, default=2)
    parser.add_argument("--build", type=Path)
    parser.add_argument("--output", type=Path,
                        help="Library installation directory; defaults to build-android/ffmpeg/<ABI>.")
    parser.add_argument("--dependency-build-directory", type=Path,
                        help="Root for generated FFmpeg libraries; sources are read directly from vendors.")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    sys.dont_write_bytecode = True
    sys.path.insert(0, str(root / "cmake"))
    from storyland_vendor_sources import stage_dependencies
    dependency_build = (args.dependency_build_directory or root / "build-android").resolve()
    dependencies = stage_dependencies(root, dependency_build, ["ffmpeg-source"])
    host = {"Linux": "linux-x86_64", "Darwin": "darwin-x86_64", "Windows": "windows-x86_64"}[platform.system()]
    binaries = args.ndk.resolve() / "toolchains/llvm/prebuilt" / host / "bin"
    suffix = ".exe" if platform.system() == "Windows" else ""
    compiler = binaries / ("clang" + suffix)
    target = "aarch64-linux-android26" if args.abi == "arm64-v8a" else "x86_64-linux-android26"
    source = dependencies["ffmpeg-source"]
    output = (args.output or dependency_build / "ffmpeg" / args.abi).resolve()
    build_directory = (args.build or root / "build-android" / ("ffmpeg-" + args.abi)).resolve()
    build_directory.mkdir(parents=True, exist_ok=True)
    bash = shutil.which("bash")
    make = shutil.which("make")
    if not bash or not make:
        raise RuntimeError("Building FFmpeg requires bash and make (Linux, macOS, WSL or MSYS2).")
    prepare_ffmpeg_build_scripts(source, bash)
    arguments = [bash, str(source / "configure"), "--prefix=" + str(output),
        "--target-os=android", "--arch=" + ("aarch64" if args.abi == "arm64-v8a" else "x86_64"),
        "--enable-cross-compile", "--cc=" + str(compiler), "--extra-cflags=--target=" + target,
        "--extra-ldflags=--target=" + target, "--ar=" + str(binaries / ("llvm-ar" + suffix)),
        "--ranlib=" + str(binaries / ("llvm-ranlib" + suffix)), "--strip=" + str(binaries / ("llvm-strip" + suffix)),
        "--disable-everything", "--disable-programs", "--disable-doc", "--disable-network",
        "--disable-autodetect", "--disable-shared", "--enable-static", "--enable-pic",
        "--disable-avdevice", "--disable-avfilter", "--disable-swresample", "--disable-debug",
        "--enable-avcodec", "--enable-avformat", "--enable-avutil", "--enable-swscale",
        "--enable-protocol=file", "--enable-demuxer=mpegps,wav,oma,mp3,ogg,flac",
        "--enable-parser=mpegvideo,mpegaudio,flac,vorbis",
        "--enable-decoder=mpeg1video,mpeg2video,atrac3,atrac3p,mp3,mp3float,vorbis,flac", "--disable-x86asm"]
    subprocess.run(arguments, cwd=build_directory, check=True)
    subprocess.run([make, "-j" + str(max(1, args.jobs))], cwd=build_directory, check=True)
    subprocess.run([make, "install"], cwd=build_directory, check=True)
    shutil.rmtree(output / "share", ignore_errors=True)
    shutil.rmtree(output / "lib/pkgconfig", ignore_errors=True)


if __name__ == "__main__":
    build()
