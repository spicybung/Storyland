"""Remove stale optional test/proof files from an existing Storyland source tree."""

from pathlib import Path, PurePosixPath


def clean_source():
    root = Path(__file__).resolve().parent
    removed_file = root / "cleanup_removed_paths.txt"
    paths = removed_file.read_text(encoding="utf-8").splitlines()
    directories = set()
    deleted_count = 0
    missing_count = 0

    for filename in paths:
        path = PurePosixPath(filename)
        if not path.parts or path.is_absolute() or ".." in path.parts or "\\" in filename:
            raise RuntimeError("Invalid source path: " + repr(filename))
        target = root.joinpath(*path.parts)
        if not target.resolve().is_relative_to(root):
            raise RuntimeError("Cleanup path escapes source tree: " + filename)
        if target.is_file() or target.is_symlink():
            target.unlink()
            deleted_count += 1
        else:
            missing_count += 1
        parent = target.parent
        while parent != root and parent.is_relative_to(root):
            directories.add(parent)
            parent = parent.parent

    for directory in sorted(directories, key=lambda path: len(path.parts), reverse=True):
        try:
            directory.rmdir()
        except OSError:
            pass

    print("Storyland source cleanup complete.")
    print("Deleted optional files:", deleted_count)
    print("Already absent:", missing_count)
    print("Play Test, Test Model, game runtime and built-in validators were preserved.")


if __name__ == "__main__":
    clean_source()
