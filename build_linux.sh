#!/usr/bin/env bash
set -euo pipefail

source_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
build_directory="${STORYLAND_BUILD_DIRECTORY:-${source_root}/build-linux}"
build_configuration="${STORYLAND_BUILD_CONFIGURATION:-Release}"
build_jobs="${STORYLAND_BUILD_JOBS:-2}"

if ! command -v cmake >/dev/null 2>&1; then
    printf '%s\n' 'CMake is required to build Storyland.' >&2
    printf '%s\n' 'Debian/Ubuntu: sudo apt install build-essential cmake pkg-config libsdl2-dev libgl1-mesa-dev libpng-dev libssl-dev zlib1g-dev libavformat-dev libavcodec-dev libavutil-dev' >&2
    exit 1
fi

cmake -S "${source_root}" -B "${build_directory}" \
    -DCMAKE_BUILD_TYPE="${build_configuration}" \
    -DSTORYLAND_ENABLE_DIRECTX=OFF
cmake --build "${build_directory}" --parallel "${build_jobs}"
printf '\nStoryland built: %s/Storyland\n' "${build_directory}"
printf '%s\n' 'Run Storyland, enter the game folder or GAME.DTZ path, then choose Play Game.'
