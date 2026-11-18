"""Locate bundled Storyland dependencies and check their required build inputs."""

import argparse
import json
from pathlib import Path, PurePosixPath


def relative_path(value):
    if not isinstance(value, str) or not value or "\\" in value or ":" in value:
        raise RuntimeError("Invalid vendor path: " + repr(value))
    path = PurePosixPath(value)
    if path.is_absolute() or any(part in ("", ".", "..") for part in value.split("/")):
        raise RuntimeError("Invalid vendor path: " + repr(value))
    return path


def dependency_manifest(source_root):
    path = Path(source_root) / "vendors" / "manifest.json"
    if not path.is_file():
        raise RuntimeError("Missing Storyland vendor manifest: " + str(path))
    manifest = json.loads(path.read_text(encoding="utf-8"))
    if manifest.get("format") != 2 or not isinstance(manifest.get("bundles"), dict):
        raise RuntimeError("Unsupported Storyland vendor manifest: " + str(path))
    return manifest


def stage_dependencies(source_root, build_directory, names):
    source_root = Path(source_root).resolve()
    vendor_directory = (source_root / "vendors").resolve()
    bundles = dependency_manifest(source_root)["bundles"]
    roots = {}

    for name in dict.fromkeys(names):
        if name not in bundles:
            raise RuntimeError("Unknown Storyland dependency: " + name)
        bundle = bundles[name]
        relative_root = bundle["root"]
        if relative_root == ".":
            if name != "integration":
                raise RuntimeError("Invalid vendor root for " + name)
            directory = vendor_directory
        else:
            subdirectory = relative_path(relative_root)
            directory = vendor_directory.joinpath(*subdirectory.parts).resolve()
            if not directory.is_relative_to(vendor_directory):
                raise RuntimeError("Vendor root escapes vendors/: " + name)

        if not directory.is_dir():
            raise RuntimeError("Missing dependency directory: " + str(directory))
        required_files = bundle.get("required_files")
        if not isinstance(required_files, list) or not required_files:
            raise RuntimeError("Missing required-file inventory: " + name)

        for filename in required_files:
            member = relative_path(filename)
            file = directory.joinpath(*member.parts)
            if not file.is_file() or not file.resolve().is_relative_to(directory):
                raise RuntimeError("Missing required build input for " + name + ": " + str(file))
        roots[name] = directory

    return roots


def verify_dependencies(source_root, names):
    """Validate the presence of essential vendor inputs, without release hashes."""
    roots = stage_dependencies(source_root, None, names)
    return {"dependencies": len(roots), "required_inputs": sum(
        len(dependency_manifest(source_root)["bundles"][name]["required_files"])
        for name in roots
    )}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--build-directory", type=Path, help="Compatibility option, no staging performed")
    parser.add_argument("--bundle", action="append", dest="bundles", help="Select an individual dependency")
    parser.add_argument("--verify", action="store_true", help="Check that essential build files exist")
    arguments = parser.parse_args()
    names = arguments.bundles or list(dependency_manifest(arguments.source)["bundles"])
    roots = stage_dependencies(arguments.source, arguments.build_directory, names)
    if arguments.verify:
        print(json.dumps(verify_dependencies(arguments.source, names), indent=2))
    else:
        print(json.dumps({name: str(directory) for name, directory in roots.items()}, indent=2))


if __name__ == "__main__":
    main()
