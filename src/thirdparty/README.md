# Third-party source

Vendored third-party source code belongs in this directory.

Storyland-owned source remains under `src/storyland`, `src/leeds`, `src/shaders`, `src/validation`, and `src/platform`. Dependencies supplied by vcpkg are not copied here.

When a dependency must be vendored, place it in its own subdirectory under `src/thirdparty` and retain its upstream license/notice files beside it.
