from __future__ import annotations

import argparse
import concurrent.futures
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import subprocess
import sys
import tarfile
import urllib.request
import wave


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def copy_file(source: Path, destination: Path) -> None:
    if source.resolve() != destination.resolve():
        shutil.copy2(source, destination)


def read_python_details(interpreter: str) -> dict:
    probe = (
        "import json,platform,struct,sys; "
        "print(json.dumps({'version':list(sys.version_info[:3]),"
        "'platform':sys.platform,'machine':platform.machine(),"
        "'bits':struct.calcsize('P')*8,'executable':sys.executable}))"
    )
    completed = subprocess.run([interpreter, "-I", "-c", probe], check=True, capture_output=True, text=True)
    details = json.loads(completed.stdout)
    if not (3, 10) <= tuple(details["version"][:2]) < (3, 14):
        raise ValueError("Select Python 3.10, 3.11, 3.12, or 3.13. Python 3.12 is the tested configuration.")
    if details["bits"] != 64 or details["machine"].lower() not in {"amd64", "x86_64"}:
        raise ValueError("This CPU installer currently targets 64-bit x86 Windows and Linux.")
    if details["platform"] not in {"win32", "linux"}:
        raise ValueError("This installer currently targets Windows and Linux.")
    return details


def validate_catalogue(path: Path) -> dict:
    catalogue = json.loads(path.read_text(encoding="utf-8"))
    if catalogue.get("policy_id") != "storyland.generated_multilingual.strict.v2":
        raise ValueError("The installer requires the reviewed multilingual Storyland voice catalogue.")
    if not isinstance(catalogue.get("lines"), list) or not catalogue["lines"]:
        raise ValueError("The reviewed voice catalogue has no lines.")
    return catalogue


def read_references(values: list[str], catalogue: dict) -> dict:
    profile_ids = {line["profile_id"] for line in catalogue["lines"]}
    references = {}
    for value in values:
        profile_id, separator, filename = value.partition("=")
        if not separator or profile_id not in profile_ids or not filename:
            raise ValueError("Each --reference must be an approved profile_id=audio.wav binding.")
        if profile_id in references:
            raise ValueError(f"The reference profile {profile_id} was supplied more than once.")
        path = Path(filename).expanduser().resolve(strict=True)
        if not path.is_file() or path.suffix.lower() != ".wav":
            raise ValueError(f"The {profile_id} reference must be a PCM WAV file.")
        with wave.open(str(path), "rb") as audio:
            duration = audio.getnframes() / audio.getframerate()
            if audio.getcomptype() != "NONE" or audio.getsampwidth() != 2 or audio.getnchannels() != 1:
                raise ValueError(f"The {profile_id} reference must be uncompressed, mono, 16-bit WAV audio.")
            if not 8000 <= audio.getframerate() <= 96000 or not 1 <= duration <= 30:
                raise ValueError(f"The {profile_id} reference must contain 1 to 30 seconds at 8 to 96 kHz.")
        references[profile_id] = {"path": path, "duration_seconds": duration, "sha256": file_sha256(path)}
    return references


def download_checked(record: dict, destination: Path) -> dict:
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_name(destination.name + ".part")
    digest = hashlib.sha256()
    written = 0
    next_progress = 256 * 1024 * 1024
    request = urllib.request.Request(record["url"], headers={"User-Agent": "Storyland-neural-voice-setup/1"})
    with urllib.request.urlopen(request, timeout=120) as response, temporary.open("wb") as stream:
        while chunk := response.read(1024 * 1024):
            stream.write(chunk)
            digest.update(chunk)
            written += len(chunk)
            if written >= next_progress:
                print(f"{destination.name}: {written:,} of {record['bytes']:,} bytes", flush=True)
                next_progress += 256 * 1024 * 1024
    if written != record["bytes"] or digest.hexdigest() != record["sha256"]:
        temporary.unlink(missing_ok=True)
        raise ValueError(f"The pinned download failed its size or SHA256 check: {destination.name}")
    temporary.replace(destination)
    print(f"Verified {destination.name}", flush=True)
    return {"file": destination.name, "bytes": written, "sha256": digest.hexdigest(), "url": record["url"]}


def source_relative_path(value: str) -> PurePosixPath:
    path = PurePosixPath(value)
    if (not value or path.is_absolute() or "\\" in value or ":" in value or
            any(part in {"", ".", ".."} for part in value.split("/"))):
        raise ValueError("A pinned source manifest contains an invalid relative path.")
    return path


def verify_source_tree(directory: Path, source: dict) -> dict:
    bundled = source["bundled_source"]
    records = bundled["files"]
    if not records or len(records) != bundled["file_count"]:
        raise ValueError("The bundled source inventory is incomplete.")
    seen = set()
    total_bytes = 0
    for record in records:
        relative = source_relative_path(record["path"])
        if relative in seen:
            raise ValueError("The bundled source inventory has duplicate paths.")
        seen.add(relative)
        path = directory.joinpath(*relative.parts)
        if not path.is_file() or not path.resolve().is_relative_to(directory.resolve()):
            raise ValueError(f"The bundled source is incomplete: {path}")
        if path.stat().st_size != record["bytes"] or file_sha256(path) != record["sha256"]:
            raise ValueError(f"The bundled source differs from the pinned revision: {path}")
        total_bytes += record["bytes"]
    return {"files": len(records), "bytes": total_bytes, "sha256_verified": True}


def find_bundled_source(bundle: Path, source: dict) -> Path | None:
    bundled = source.get("bundled_source")
    if bundled is None:
        return None
    relative = source_relative_path(bundled["directory"])
    candidates = [bundle.joinpath(*relative.parts)]
    if "sibling_directory" in bundled:
        sibling = source_relative_path(bundled["sibling_directory"])
        candidates.append(bundle.parent.joinpath(*sibling.parts))
    for candidate in candidates:
        if candidate.exists():
            if not candidate.is_dir():
                raise ValueError(f"The bundled source path is not a directory: {candidate}")
            verify_source_tree(candidate, source)
            return candidate
    return None


def copy_source_tree(source_directory: Path, destination: Path, source: dict) -> dict:
    verification = verify_source_tree(source_directory, source)
    if destination.resolve().is_relative_to(source_directory.resolve()):
        raise ValueError("Install the neural runtime outside its bundled source directory.")
    destination.mkdir(parents=True, exist_ok=False)
    for record in source["bundled_source"]["files"]:
        relative = source_relative_path(record["path"])
        target = destination.joinpath(*relative.parts)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source_directory.joinpath(*relative.parts), target)
    verify_source_tree(destination, source)
    return verification


def extract_source(archive: Path, destination: Path, renamed_paths: dict | None = None) -> None:
    destination.mkdir(parents=True, exist_ok=True)
    with tarfile.open(archive, "r:gz") as package:
        for member in package.getmembers():
            parts = PurePosixPath(member.name).parts
            if len(parts) < 2:
                continue
            original = PurePosixPath(*parts[1:]).as_posix()
            relative = source_relative_path((renamed_paths or {}).get(original, original))
            target = destination.joinpath(*relative.parts)
            if member.isdir():
                target.mkdir(parents=True, exist_ok=True)
            elif member.isfile():
                target.parent.mkdir(parents=True, exist_ok=True)
                source = package.extractfile(member)
                if source is None:
                    raise ValueError("A source archive member could not be read.")
                with source, target.open("wb") as output:
                    shutil.copyfileobj(source, output)
                target.chmod(member.mode & 0o777)
            else:
                raise ValueError("The pinned source archive unexpectedly contains a link or special file.")


def install_source(name: str, source: dict, bundled_directory: Path | None, output: Path) -> dict:
    destination = output / "vendor" / name
    if bundled_directory is not None:
        verification = copy_source_tree(bundled_directory, destination, source)
        return {"component": name, "origin": "bundled", "revision": source["revision"],
                "verification": verification}
    archive = output / "sources" / (name + ".tar.gz")
    downloaded = download_checked(source, archive)
    bundled = source.get("bundled_source", {})
    extract_source(archive, destination, bundled.get("renamed_paths"))
    verification = verify_source_tree(destination, source) if bundled else {"archive_sha256_verified": True}
    return {"component": name, "origin": "download", "revision": source["revision"],
            "download": downloaded, "verification": verification}


def patch_source(directory: Path, patches: list[dict]) -> None:
    for patch in patches:
        target = directory / patch["path"]
        if file_sha256(target) != patch["original_sha256"]:
            raise ValueError(f"The source does not match the reviewed patch input: {patch['path']}")
        text = target.read_text(encoding="utf-8")
        for replacement in patch["replacements"]:
            before, after = replacement["before"], replacement["after"]
            if text.count(before) != 1:
                raise ValueError(f"A reviewed patch does not match exactly once: {patch['path']}")
            text = text.replace(before, after, 1)
        target.write_bytes(text.encode("utf-8"))
        if file_sha256(target) != patch["patched_sha256"]:
            raise ValueError(f"The patched source failed its SHA256 check: {patch['path']}")


def venv_interpreter(output: Path, platform: str) -> Path:
    if platform == "win32":
        return output / "venv" / "Scripts" / "python.exe"
    return output / "venv" / "bin" / "python"


def run_command(arguments: list[str], environment: dict) -> None:
    subprocess.run(arguments, env=environment, check=True)


def install_python(output: Path, details: dict, bundle: Path) -> None:
    environment = {name:value for name, value in os.environ.items() if not name.startswith("PIP_")}
    environment.update({"PIP_NO_CACHE_DIR": "1", "PIP_DISABLE_PIP_VERSION_CHECK": "1", "PIP_CONFIG_FILE": os.devnull, "PIP_EXTRA_INDEX_URL": "", "PYTHONDONTWRITEBYTECODE": "1"})
    run_command([details["executable"], "-I", "-m", "venv", str(output / "venv")], environment)
    interpreter = str(venv_interpreter(output, details["platform"]))
    common = [interpreter, "-I", "-m", "pip", "install", "--no-cache-dir"]
    constraints = str(bundle / "constraints.txt")
    run_command(common + ["--index-url", "https://pypi.org/simple", "--requirement", str(bundle / "requirements-build.txt"), "--report", str(output / "provenance/build-install.json")], environment)
    run_command(common + ["--index-url", "https://download.pytorch.org/whl/cpu", "--constraint", constraints, "torch==2.6.0", "torchaudio==2.6.0", "--report", str(output / "provenance/torch-install.json")], environment)
    run_command(common + ["--index-url", "https://pypi.org/simple", "--constraint", constraints, "--requirement", str(bundle / "requirements-runtime.txt"), "--report", str(output / "provenance/runtime-install.json")], environment)
    run_command(common + ["--no-build-isolation", "--no-deps", str(output / "vendor/perth"), str(output / "vendor/chatterbox"), "--report", str(output / "provenance/vendor-install.json")], environment)
    run_command([interpreter, "-I", "-m", "pip", "check"], environment)
    packages = subprocess.check_output([interpreter, "-I", "-m", "pip", "list", "--format=json"], env=environment, text=True)
    write_json(output / "provenance/installed-packages.json", json.loads(packages))


def create_configuration(output: Path, worker: Path, catalogue: Path, references: dict, manifest: dict, threads: int) -> dict:
    copy_file(worker, output / "storyland_neural_voice_worker.py")
    copy_file(catalogue, output / "vcs_approved_voice_lines.json")
    profiles = {}
    reference_receipt = {}
    (output / "references").mkdir(exist_ok=True)
    for profile_id, reference in references.items():
        relative = Path("references") / (profile_id + ".wav")
        copy_file(reference["path"], output / relative)
        profiles[profile_id] = {"reference_audio": relative.as_posix()}
        reference_receipt[profile_id] = {"file": relative.as_posix(), "sha256": reference["sha256"], "duration_seconds": reference["duration_seconds"]}
    configuration = {
        "format": "storyland_neural_voice_config",
        "format_version": 1,
        "code_directory": "vendor/chatterbox",
        "model_directory": "models/chatterbox-multilingual-v3",
        "catalogue": "vcs_approved_voice_lines.json",
        "profiles": profiles,
        "device": "cpu",
        "cpu_threads": threads,
        "seed": 20261109,
        "t3_model": "v3",
        "code_revision": manifest["sources"]["chatterbox"]["revision"],
    }
    write_json(output / "voice_config.json", configuration)
    write_json(output / "provenance/reference-audio.json", reference_receipt)
    return configuration


def verify_installation(output: Path, manifest: dict) -> dict:
    configuration = json.loads((output / "voice_config.json").read_text(encoding="utf-8"))
    for key in ("model_directory", "code_directory", "catalogue"):
        if Path(configuration[key]).is_absolute():
            raise ValueError(f"The installed {key} must use a relative path.")
    for record in manifest["models"]:
        path = output / configuration["model_directory"] / record["name"]
        if path.stat().st_size != record["bytes"] or file_sha256(path) != record["sha256"]:
            raise ValueError(f"The installed model file failed verification: {record['name']}")
    for patch in manifest["patches"]:
        if file_sha256(output / configuration["code_directory"] / patch["path"]) != patch["patched_sha256"]:
            raise ValueError(f"The installed source patch failed verification: {patch['path']}")
    watermark = output / manifest["watermark"]["relative_checkpoint"]
    if file_sha256(watermark) != manifest["watermark"]["sha256"]:
        raise ValueError("The installed Perth watermark checkpoint failed verification.")
    validate_catalogue(output / configuration["catalogue"])
    reference_receipt = json.loads((output / "provenance/reference-audio.json").read_text(encoding="utf-8"))
    for profile_id, profile in configuration["profiles"].items():
        if Path(profile["reference_audio"]).is_absolute() or not (output / profile["reference_audio"]).is_file():
            raise ValueError("An installed reference binding is not a valid relative path.")
        if file_sha256(output / profile["reference_audio"]) != reference_receipt[profile_id]["sha256"]:
            raise ValueError(f"The installed reference audio failed verification: {profile_id}")
    interpreter = venv_interpreter(output, sys.platform)
    environment = os.environ.copy()
    environment.update({"HF_HUB_OFFLINE": "1", "TRANSFORMERS_OFFLINE": "1", "PYTHONDONTWRITEBYTECODE": "1"})
    probe = (
        "import importlib.metadata,json; "
        "from chatterbox.mtl_tts import ChatterboxMultilingualTTS; "
        "import perth; "
        "assert 'ru' in ChatterboxMultilingualTTS.get_supported_languages(); "
        "assert perth.PerthImplicitWatermarker is not None; "
        "print(json.dumps({name:importlib.metadata.version(name) for name in "
        "['torch','torchaudio','chatterbox-tts','resemble-perth']}))"
    )
    result = subprocess.run([str(interpreter), "-I", "-B", "-c", probe], env=environment, check=True, capture_output=True, text=True)
    versions = json.loads(result.stdout)
    return {"status": "verified", "versions": versions, "models_sha256_verified": True, "patches_sha256_verified": True, "relative_configuration": True, "model_inference_performed": False, "generated_audio_cache": False}


def main() -> int:
    parser = argparse.ArgumentParser(description="Install Storyland's pinned local neural voice runtime for Windows or Linux.")
    destination = parser.add_mutually_exclusive_group()
    destination.add_argument("--output", type=Path, help="Final speech/neural installation directory; an existing runtime is not overwritten.")
    destination.add_argument("--storyland-executable", type=Path, help="Install into speech/neural beside this Storyland executable.")
    parser.add_argument("--python", default=sys.executable, help="64-bit Python 3.10 to 3.13 interpreter; Python 3.12 is recommended.")
    parser.add_argument("--reference", action="append", default=[], metavar="PROFILE_ID=AUDIO.WAV")
    parser.add_argument("--resources", type=Path, help="Directory containing runtime_manifest.json, pinned requirements, and licenses.")
    parser.add_argument("--worker", type=Path, help="Override the source or adjacent neural worker script.")
    parser.add_argument("--catalogue", type=Path, help="Override the source or adjacent reviewed dialogue catalogue.")
    parser.add_argument("--cpu-threads", type=int, default=4)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--dry-run", action="store_true", help="Validate inputs and print the exact plan without downloads, installation, or output files.")
    mode.add_argument("--verify-only", action="store_true", help="Verify an existing installation without downloading or loading the neural model.")
    arguments = parser.parse_args()
    if not 1 <= arguments.cpu_threads <= 32:
        parser.error("--cpu-threads must be from one to thirty-two")
    script_directory = Path(__file__).resolve().parent
    project = next((directory for directory in (script_directory, *script_directory.parents)
                    if (directory / "STORYLAND_VERSION.txt").is_file()
                    and (directory / "vendors/chatterbox/runtime_manifest.json").is_file()), None)
    packaged_resources = script_directory / "setup_resources"
    bundle = arguments.resources
    if bundle is None:
        if packaged_resources.is_dir():
            bundle = packaged_resources
        elif project is not None:
            bundle = project / "vendors/chatterbox"
        else:
            parser.error("Could not find the voice setup resources. Supply --resources or run the source or packaged setup script.")
    bundle = bundle.expanduser().resolve(strict=True)
    worker = arguments.worker or script_directory / "storyland_neural_voice_worker.py"
    catalogue_path = arguments.catalogue
    if catalogue_path is None:
        adjacent_catalogue = script_directory / "vcs_approved_voice_lines.json"
        if adjacent_catalogue.is_file():
            catalogue_path = adjacent_catalogue
        elif project is not None:
            catalogue_path = project / "data/vcs_approved_voice_lines.json"
        else:
            parser.error("Could not find the reviewed dialogue catalogue. Supply --catalogue or keep it beside the packaged setup script.")
    worker = worker.expanduser().resolve(strict=True)
    catalogue_path = catalogue_path.expanduser().resolve(strict=True)
    manifest = json.loads((bundle / "runtime_manifest.json").read_text(encoding="utf-8"))
    output = arguments.output
    if output is None and arguments.storyland_executable is not None:
        executable = arguments.storyland_executable.expanduser().resolve(strict=True)
        if not executable.is_file():
            parser.error("--storyland-executable must identify a built executable")
        output = executable.parent / "speech/neural"
    if output is None:
        if packaged_resources.is_dir():
            output = script_directory
        else:
            parser.error("Choose --output or --storyland-executable when running setup from the source tree.")
    output = output.expanduser().resolve()
    if arguments.verify_only:
        print(json.dumps(verify_installation(output, manifest), indent=2))
        return 0
    details = read_python_details(arguments.python)
    catalogue = validate_catalogue(catalogue_path)
    references = read_references(arguments.reference, catalogue)
    if not references:
        parser.error("Provide at least one --reference profile_id=audio.wav binding.")
    local_sources = {name:find_bundled_source(bundle, record)
                     for name, record in manifest["sources"].items()}
    plan = {
        "output": str(output),
        "python": details,
        "model_revision": manifest["model_revision"],
        "model_download_bytes": sum(record["bytes"] for record in manifest["models"]),
        "model_files": [{key:record[key] for key in ("name", "bytes", "sha256", "url")} for record in manifest["models"]],
        "source_revisions": {name:record["revision"] for name, record in manifest["sources"].items()},
        "source_inputs": {name:{"origin": "bundled" if local_sources[name] is not None else "download",
                                "directory": str(local_sources[name]) if local_sources[name] is not None else None,
                                "download_bytes": 0 if local_sources[name] is not None else record["bytes"]}
                          for name, record in manifest["sources"].items()},
        "references": {name:{"source":str(record["path"]), "sha256":record["sha256"]} for name, record in references.items()},
        "watermark_enabled": True,
        "persistent_generated_audio_cache": False,
        "configuration_paths": "relative to voice_config.json",
    }
    if arguments.dry_run:
        print(json.dumps(plan, indent=2, ensure_ascii=False))
        return 0
    for name in ("venv", "models", "vendor", "sources", "provenance", "voice_config.json"):
        if (output / name).exists():
            raise ValueError(f"The destination already contains {name}. Choose a new runtime directory, or use --verify-only for an existing installation.")
    output.mkdir(parents=True, exist_ok=True)
    (output / "provenance").mkdir(exist_ok=True)
    write_json(output / "provenance/setup-plan.json", plan)
    downloads = []
    source_receipts = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
        futures = {}
        for name, record in manifest["sources"].items():
            futures[pool.submit(install_source, name, record, local_sources[name], output)] = "source"
        for record in manifest["models"]:
            futures[pool.submit(download_checked, record, output / "models/chatterbox-multilingual-v3" / record["name"])] = "model"
        for future in concurrent.futures.as_completed(futures):
            receipt = future.result()
            if futures[future] == "source":
                source_receipts.append(receipt)
                if "download" in receipt:
                    downloads.append(receipt["download"])
            else:
                downloads.append(receipt)
    write_json(output / "provenance/downloads.json", downloads)
    write_json(output / "provenance/sources.json", sorted(source_receipts, key=lambda item:item["component"]))
    patch_source(output / "vendor/chatterbox", manifest["patches"])
    for filename in ("runtime_manifest.json", "local-runtime.patch", "constraints.txt", "requirements-runtime.txt", "requirements-build.txt", "LICENSE.chatterbox", "LICENSE.perth", "README.md"):
        shutil.copy2(bundle / filename, output / "provenance" / filename)
    install_python(output, details, bundle)
    create_configuration(output, worker, catalogue_path, references, manifest, arguments.cpu_threads)
    verification = verify_installation(output, manifest)
    write_json(output / "provenance/verification.json", verification)
    print(json.dumps({"installed": str(output), "configuration": str(output / "voice_config.json"), "verification": verification}, indent=2))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"Neural voice setup failed: {error}", file=sys.stderr)
        raise SystemExit(1)
