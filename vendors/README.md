# Bundled dependencies

Storyland includes editable build inputs for Lua, OpenSubdiv, SDL2, FFmpeg,
eSpeak NG, ImGui, zlib, MikkTSpace, and the optional Chatterbox/Perth voice tools.
These are application dependencies, not thousands of Storyland test programs.

The required-file manifest at `manifest.json` contains the source root and
minimum required files for each dependency. It does not contain a per-file
SHA-256 proof inventory. CMake validates required paths during configuration.

Third-party demo applications, upstream test suites, CI artifacts and expendable
documentation were excluded from this application source distribution. Required
headers, platform integrations, tool scripts, model weights, runtime dictionaries
and audio/video codecs are retained.

## Licensing

The original license notices remain beside each dependency:

- SDL2: `SDL2/LICENSE.txt`
- FFmpeg: `ffmpeg/source/LICENSE.md` and `ffmpeg/source/COPYING.*`
- eSpeak NG: `espeak-ng/licenses/` and `espeak-ng/source/COPYING`
- Lua: `lua/doc/readme.html` and `lua/src/lua.h`
- OpenSubdiv: `opensubdiv/LICENSE.txt`, `opensubdiv/NOTICE.txt`
- ImGui: `imgui/LICENSE.txt`
- zlib: `zlib/LICENSE`
- Chatterbox: `chatterbox/source/LICENSE` and `chatterbox/LICENSE.chatterbox`
- Perth: `perth/LICENSE`
- nlohmann JSON: `nlohmann/LICENSE.MIT`

The LCS/VCS opcode definitions, reference shaders, and fonts retain their
bundled license or attribution files. The provided Android FFmpeg libraries
target `arm64-v8a`, and its source is retained for further ABI builds.

To check that required build inputs are present, run:

```sh
python3 -B cmake/storyland_vendor_sources.py --verify
```

The helper verifies required files only. It does not hash every dependency.
