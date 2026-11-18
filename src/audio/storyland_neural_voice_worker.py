from __future__ import annotations

import argparse
import contextlib
import hashlib
import json
import math
import os
import random
import sys
import wave
import zlib
from pathlib import Path

sys.dont_write_bytecode = True

MAX_REQUEST_BYTES = 32768
MAX_AUDIO_SECONDS = 120
MAX_REFERENCE_BYTES = 6 * 1024 * 1024
SYNTHESIS_LIMITS = {
    "cfg_weight": (0.0, 1.0),
    "temperature": (0.1, 1.0),
    "exaggeration": (0.0, 1.0),
}


def resolve_path(directory: Path, value: str, label: str) -> Path:
    if not isinstance(value, str) or not value:
        raise ValueError(f"The neural voice configuration has no {label}.")
    path = Path(value).expanduser()
    return (path if path.is_absolute() else directory / path).resolve()


def validate_reference_audio(path: Path, profile_id: str) -> None:
    if not path.is_file() or path.suffix.lower() != ".wav":
        raise ValueError(f"The {profile_id} reference must be a PCM WAV file.")
    if path.stat().st_size > MAX_REFERENCE_BYTES:
        raise ValueError(f"The {profile_id} reference exceeds the six MiB file limit.")
    try:
        with wave.open(str(path), "rb") as audio:
            rate = audio.getframerate()
            if audio.getcomptype() != "NONE" or audio.getsampwidth() != 2 or audio.getnchannels() != 1:
                raise ValueError(f"The {profile_id} reference must be uncompressed, mono, 16-bit WAV audio.")
            if not 8000 <= rate <= 96000 or not 1 <= audio.getnframes() / rate <= 30:
                raise ValueError(f"The {profile_id} reference must contain 1 to 30 seconds at 8 to 96 kHz.")
    except (wave.Error, EOFError) as error:
        raise ValueError(f"The {profile_id} reference has an invalid WAV header.") from error


def reference_sha256(path: Path) -> str:
    with path.open("rb") as source:
        data = source.read(MAX_REFERENCE_BYTES + 1)
    if len(data) > MAX_REFERENCE_BYTES:
        raise ValueError("The voice reference exceeds the six MiB file limit.")
    return hashlib.sha256(data).hexdigest()


def validate_synthesis_overrides(profile_id: str, overrides: dict) -> dict:
    if not isinstance(overrides, dict) or any(key not in SYNTHESIS_LIMITS for key in overrides):
        raise ValueError(f"The {profile_id} synthesis overrides must use cfg_weight, temperature, or exaggeration.")
    for key, value in overrides.items():
        low, high = SYNTHESIS_LIMITS[key]
        if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value) or not low <= value <= high:
            raise ValueError(f"The {profile_id} {key} override must be a finite number from {low} to {high}.")
    return dict(overrides)


def synthesis_parameters(profile_id: str, profile: dict, language: str, emotion: str) -> dict:
    parameters = {
        "cfg_weight": 0.0 if language == "ru" else 0.5,
        "temperature": 0.75,
        "exaggeration": {
            "neutral": 0.45,
            "urgent": 0.60,
            "angry": 0.65,
            "quiet": 0.25,
            "reassuring": 0.35,
        }[emotion],
    }
    parameters.update(validate_synthesis_overrides(profile_id, profile.get("synthesis", {})))
    return parameters


def load_configuration(path: Path) -> dict:
    configuration = json.loads(path.read_text(encoding="utf-8"))
    if configuration.get("format") != "storyland_neural_voice_config" or configuration.get("format_version") != 1:
        raise ValueError("Unsupported neural voice configuration.")
    if configuration.get("t3_model", "v3") != "v3":
        raise ValueError("This worker requires Chatterbox Multilingual V3 with t3_model set to v3.")
    directory = path.parent.resolve()
    configuration["model_path"] = resolve_path(directory, configuration.get("model_directory"), "model directory")
    catalogue_path = resolve_path(directory, configuration.get("catalogue"), "dialogue catalogue")
    if not configuration["model_path"].is_dir():
        raise ValueError("The configured Chatterbox model directory does not exist.")
    catalogue = json.loads(catalogue_path.read_text(encoding="utf-8"))
    if catalogue.get("policy_id") != "storyland.generated_multilingual.strict.v2":
        raise ValueError("The neural worker requires Storyland's reviewed multilingual catalogue.")
    lines = catalogue.get("lines")
    if not isinstance(lines, list):
        raise ValueError("The approved dialogue catalogue is invalid.")
    configuration["lines"] = {}
    for line in lines:
        identifier = line.get("id")
        if not isinstance(identifier, str) or identifier in configuration["lines"]:
            raise ValueError("The dialogue catalogue contains an invalid or duplicate line ID.")
        configuration["lines"][identifier] = line
    profiles = configuration.get("profiles")
    if not isinstance(profiles, dict):
        raise ValueError("The neural worker requires character reference bindings.")
    configuration["references"] = {}
    for identifier, profile in profiles.items():
        if not isinstance(identifier, str) or not identifier or not isinstance(profile, dict):
            raise ValueError("Every character reference binding must be a profile object with a nonempty ID.")
        validate_synthesis_overrides(identifier, profile.get("synthesis", {}))
        reference = resolve_path(directory, profile.get("reference_audio"), f"{identifier} reference audio")
        validate_reference_audio(reference, identifier)
        configuration["references"][identifier] = reference
    if configuration.get("code_directory"):
        code = resolve_path(directory, configuration["code_directory"], "Chatterbox source directory")
        if not code.is_dir():
            raise ValueError("The configured Chatterbox source directory does not exist.")
        sys.path.insert(0, str(code / "src" if (code / "src").is_dir() else code))
    return configuration


def initialize_model(state: dict) -> None:
    if state.get("model") is not None:
        return
    configuration = state["configuration"]
    threads = configuration.get("cpu_threads", 4)
    if type(threads) is not int or not 1 <= threads <= 32:
        raise ValueError("cpu_threads must be an integer from one to thirty-two.")
    os.environ["OMP_NUM_THREADS"] = str(threads)
    os.environ["MKL_NUM_THREADS"] = str(threads)
    os.environ["TOKENIZERS_PARALLELISM"] = "false"
    os.environ["HF_HUB_OFFLINE"] = "1"
    os.environ["TRANSFORMERS_OFFLINE"] = "1"
    with contextlib.redirect_stdout(sys.stderr):
        import numpy
        import torch

        if not state.get("thread_pools_initialized", False):
            torch.set_num_threads(threads)
            torch.set_num_interop_threads(min(threads, 2))
            state["thread_pools_initialized"] = True
        from chatterbox.mtl_tts import ChatterboxMultilingualTTS
        requested_device = configuration.get("device", "cpu")
        if requested_device == "auto":
            requested_device = "cuda" if torch.cuda.is_available() else "cpu"
        if requested_device not in {"cpu", "cuda", "mps"}:
            raise ValueError("The configured synthesis device is unsupported.")
        model = ChatterboxMultilingualTTS.from_local(
            configuration["model_path"],
            device=requested_device,
            t3_model="v3",
        )
    state["numpy"] = numpy
    state["torch"] = torch
    state["model"] = model
    state["device"] = requested_device


def verify_request(request: dict, configuration: dict) -> tuple[dict, Path]:
    identifier = request.get("request_id")
    if type(identifier) is not int or identifier <= 0:
        raise ValueError("A positive integer request_id is required.")
    line = configuration["lines"].get(request.get("line_id"))
    if line is None or line.get("catalogue_status") != "included":
        raise ValueError("The requested line is not in the approved catalogue.")
    language = line.get("language", "en")
    if language not in {"en", "ru"} or request.get("language") != language:
        raise ValueError("The requested language does not match the approved line.")
    if request.get("profile_id") != line.get("profile_id"):
        raise ValueError("The requested character does not match the approved line.")
    if request.get("text") != line.get("text"):
        raise ValueError("The requested text differs from the approved dialogue.")
    if request.get("emotion") not in {"neutral", "urgent", "angry", "quiet", "reassuring"}:
        raise ValueError("The requested emotion is unsupported.")
    text = line["text"]
    if not isinstance(text, str) or not text or len(text.encode("utf-8")) > 1024:
        raise ValueError("The approved line has invalid text dimensions.")
    reference = configuration["references"].get(line["profile_id"])
    if reference is None:
        raise ValueError(f"No character reference is configured for {line['profile_id']}.")
    validate_reference_audio(reference, line["profile_id"])
    controls = request.get("controls")
    if not isinstance(controls, dict):
        raise ValueError("Prepared voice controls are required.")
    for key, low, high in [
        ("rate_multiplier", 0.75, 1.30),
        ("pitch_offset_semitones", -4.0, 4.0),
        ("energy", 0.05, 1.0),
        ("roughness", 0.0, 1.0),
    ]:
        value = controls.get(key)
        if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value) or not low <= value <= high:
            raise ValueError(f"The prepared {key} control is invalid.")
    return line, reference


def render_request(request: dict, state: dict) -> tuple[dict, bytes]:
    configuration = state["configuration"]
    line, reference = verify_request(request, configuration)
    language = line.get("language", "en")
    parameters = synthesis_parameters(line["profile_id"], configuration["profiles"][line["profile_id"]], language, request["emotion"])
    seed = configuration.get("seed", 20261109)
    if type(seed) is not int:
        raise ValueError("The configured synthesis seed must be an integer.")
    initialize_model(state)
    numpy = state["numpy"]
    torch = state["torch"]
    model = state["model"]
    seed = (seed + zlib.crc32(line["id"].encode("utf-8"))) & 0x7fffffff
    random.seed(seed)
    numpy.random.seed(seed)
    torch.manual_seed(seed)
    if state["device"] == "cuda":
        torch.cuda.manual_seed_all(seed)
    reference_digest = reference_sha256(reference)
    try:
        with contextlib.redirect_stdout(sys.stderr), torch.inference_mode():
            generated = model.generate(
                line["text"],
                language_id=language,
                audio_prompt_path=str(reference),
                exaggeration=parameters["exaggeration"],
                cfg_weight=parameters["cfg_weight"],
                temperature=parameters["temperature"],
                repetition_penalty=1.4,
                min_p=0.05,
                top_p=0.95,
            )
        samples = generated.detach().cpu().numpy().reshape(-1)
    finally:
        model.conds = None
    if reference_sha256(reference) != reference_digest:
        raise ValueError("The character reference changed while synthesis was in progress.")
    rate = int(model.sr)
    if not 8000 <= rate <= 48000 or samples.size == 0 or samples.size > rate * MAX_AUDIO_SECONDS:
        raise ValueError("The neural voice output has invalid dimensions.")
    if not numpy.isfinite(samples).all():
        raise ValueError("The neural voice output contains invalid samples.")
    peak = float(numpy.max(numpy.abs(samples)))
    if peak < 0.0001:
        raise ValueError("The neural voice output contains no audible speech.")
    gain = min(1.0, 0.98 / peak)
    pcm = numpy.rint(numpy.clip(samples * gain, -1.0, 1.0) * 32767.0).astype("<i2").tobytes()
    response = {
        "request_id": request["request_id"],
        "status": "ready",
        "sample_rate": rate,
        "sample_count": int(samples.size),
        "pcm_bytes": len(pcm),
        "voice_id": "chatterbox:" + line["profile_id"],
        "backend_version": "Chatterbox Multilingual V3 / " + str(configuration.get("code_revision", "local")),
        "watermark": "default_perth",
        "device": state["device"],
        "seed": seed,
        "exaggeration": parameters["exaggeration"],
        "cfg_weight": parameters["cfg_weight"],
        "temperature": parameters["temperature"],
        "reference_sha256": reference_digest,
        "pitch_shift_applied": False,
        "rate_shift_applied": False,
        "roughness_applied": False,
    }
    return response, pcm


def send_response(response: dict, pcm: bytes = b"") -> None:
    header = json.dumps(response, ensure_ascii=False, separators=(",", ":")).encode("utf-8") + b"\n"
    if len(header) > 16384:
        raise ValueError("The synthesis response header is too long.")
    sys.stdout.buffer.write(header)
    if pcm:
        sys.stdout.buffer.write(pcm)
    sys.stdout.buffer.flush()


def main() -> int:
    parser = argparse.ArgumentParser(description="Storyland's approved-dialogue neural voice worker.")
    parser.add_argument("--config", type=Path, required=True)
    arguments = parser.parse_args()
    state = {"configuration": load_configuration(arguments.config.resolve())}
    while True:
        raw = sys.stdin.buffer.readline(MAX_REQUEST_BYTES + 1)
        if not raw:
            return 0
        if len(raw) > MAX_REQUEST_BYTES or not raw.endswith(b"\n"):
            send_response({"request_id": 0, "status": "error", "message": "The synthesis request is oversized or incomplete."})
            return 1
        request_id = 0
        try:
            request = json.loads(raw.decode("utf-8", errors="strict"))
            if not isinstance(request, dict):
                raise ValueError("The synthesis request must be an object.")
            candidate_id = request.get("request_id")
            if type(candidate_id) is int:
                request_id = candidate_id
            response, pcm = render_request(request, state)
        except Exception as error:
            response = {"request_id": request_id, "status": "error", "message": str(error)}
            pcm = b""
        send_response(response, pcm)


if __name__ == "__main__":
    raise SystemExit(main())
