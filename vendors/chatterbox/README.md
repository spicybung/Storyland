# Storyland neural voice runtime

This directory contains the reproducible installation recipe and pinned Chatterbox source for Multilingual V3 with reference audio, Russian text, and the original Perth watermark. Chatterbox code is in `source/`; Perth code and its required watermark checkpoint are in the sibling `vendors/perth/` directory. The large Chatterbox model files are downloaded when the user runs the setup tool. They are not part of the Storyland source archive.

## Install beside Storyland

Use 64-bit CPython 3.10, 3.11, 3.12, or 3.13 on x86-64 Windows or Linux. Python 3.12 is the configuration exercised in the Linux synthesis test. Run the setup from the Storyland source directory after building the application. The setup creates its own `venv` and does not install packages into the selected interpreter's global environment.

The opening of Domo Arigato Domestoboto uses Victor Vance, Lance Vance, and Ricardo Diaz. Bind each profile to its own reference recording. References must be mono, uncompressed, 16-bit PCM WAV files, 1 to 30 seconds long, sampled at 8 to 96 kHz. A clean speech recording is useful because the model also hears music, effects, and other speakers present in the reference.

PowerShell example; replace the executable and reference file locations with the files on your machine:

```powershell
py -3.12 src/audio/setup_neural_voice.py `
  --storyland-executable .\build\x64\Release\Storyland.exe `
  --reference "vcs.victor_vance=.\references\victor_vance_reference_24k.wav" `
  --reference "vcs.lance_vance=.\references\lance_vance_reference_24k.wav" `
  --reference "vcs.ricardo_diaz=.\references\ricardo_diaz_reference_24k.wav"
```

Linux example:

```sh
python3.12 src/audio/setup_neural_voice.py \
  --storyland-executable ./build/Storyland \
  --reference vcs.victor_vance=./references/victor_vance_reference_24k.wav \
  --reference vcs.lance_vance=./references/lance_vance_reference_24k.wav \
  --reference vcs.ricardo_diaz=./references/ricardo_diaz_reference_24k.wav
```

`--storyland-executable` selects `speech/neural` beside that executable. Alternatively, supply the final installation directory with `--output`. The native backend finds `venv/Scripts/python.exe` on Windows and `venv/bin/python` on Linux. The installed `voice_config.json` uses relative paths for the source, model directory, catalogue, and references. Install into the final location because Python virtual environments contain paths to their original location.

At least one reference binding is required. A character without a binding remains unavailable for neural synthesis; a reference is never substituted from another character. `--reference` can be repeated for any profile already present in the reviewed catalogue.

Add `--dry-run` to either command to validate the inputs and bundled source hashes and print every model URL, checksum, source revision, source location, and reference binding without downloading, creating output files, or installing anything. A previous installed runtime is never overwritten. A failed installation can be inspected through its `provenance` files; remove that incomplete directory or choose another destination before running setup again.

Packaged applications include the setup script, worker, catalogue, and a `setup_resources` directory together in `speech/neural`. CMake copies the bundled Chatterbox source to `setup_resources/source/` and Perth to `setup_resources/perth/`. When the script finds those adjacent resources, it defaults the installation to that same directory and preserves the bootstrap files. The source-tree tool requires `--output` or `--storyland-executable`. `--resources`, `--worker`, and `--catalogue` also accept explicit locations for custom packaging.

Setup verifies and copies the bundled code into the selected runtime's `vendor/` directory before applying its reviewed patches. It leaves the checkout and packaged bootstrap sources unchanged. A source directory that is present but incomplete or modified fails verification instead of being silently replaced. A separately distributed setup bundle without source directories retains the exact pinned archive download fallback. `provenance/sources.json` records which route supplied each component.

To verify an installed runtime without generating audio:

```sh
python src/audio/setup_neural_voice.py --output ./build/speech/neural --verify-only
```

Verification checks model and patched-source SHA256 values, relative reference paths, Python imports, Russian support, and availability of the actual Perth watermarker. It does not load the large neural model or claim an auditory quality test.

## Pinned sources and models

| Component | Official revision or version |
| --- | --- |
| Chatterbox code | `ab55923c27757c90cfcc407baa57dc25a7d25217`, package `0.1.7` |
| ResembleAI/chatterbox model repository | `5bb1f6ee58e50c3b8d408bc82a6d3740c2db6e18` |
| Multilingual T3 model | `t3_mtl23ls_v3.safetensors`; the worker explicitly selects `v3` |
| Perth source and watermark | `ff1c8ac55a976971245cdd53c18d6131ca00d993`, package `1.1.0` |
| CPU PyTorch and torchaudio | `2.6.0+cpu`, official PyTorch CPU wheel index |
| Transformers | `5.2.0` |
| Diffusers | `0.29.0` |

The five model assets total **3,208,844,550 bytes**, excluding Python packages. Source archives are not downloaded when the bundled code is present:

- `t3_mtl23ls_v3.safetensors`
- `s3gen.pt`
- `ve.pt`
- `grapheme_mtl_merged_expanded_v1.json`
- `Cangjie5_TC.json`

The pinned `from_local` implementation uses `s3gen.pt`, including when the V3 T3 model is selected. The installer follows that implementation. The exact URLs, byte lengths, SHA256 hashes, source revisions, bundled source inventories, and watermark checkpoint checksum are in [runtime_manifest.json](runtime_manifest.json). Every bundled source file is checked before copying; fallback archives and model downloads are checked before use. The download records identify public, ungated official sources.

[requirements-runtime.txt](requirements-runtime.txt) pins the upstream runtime dependencies. [constraints.txt](constraints.txt) pins their transitive dependencies and includes Python-version markers where newer package releases dropped Python 3.10 or 3.11 support. Python 3.13 uses NumPy 2.2.6; Python 3.10 to 3.12 use NumPy 1.26.4. [requirements-build.txt](requirements-build.txt) pins the installer and build tools. The exact successful Linux Python 3.12 environment is recorded in [requirements-linux-py312-tested.lock.txt](requirements-linux-py312-tested.lock.txt).

Each installation records pip's actual downloaded package URLs and hashes in `provenance/*-install.json`, the installed versions in `provenance/installed-packages.json`, and reference hashes in `provenance/reference-audio.json`. The script uses pip's `--no-cache-dir`. It neither creates nor consults a generated-audio cache.

## Reviewed local changes

[local-runtime.patch](local-runtime.patch) shows the changes to the official Chatterbox source. The committed source remains unpatched. Setup applies equivalent exact-text transformations to the runtime's copied source only when the source file hashes match the pinned originals, then checks the resulting hashes. The upstream Chatterbox and Perth `.gitignore` files are preserved as `.gitignore.upstream`; this keeps their original policy text without allowing it to hide files in the Storyland checkout. These filename mappings are recorded in the source manifest and also applied to fallback archive extraction.

1. Read `Cangjie5_TC.json` from the explicit model directory instead of invoking a Hugging Face download from the tokenizer constructor.
2. Initialize the Chinese converter only when Chinese text is encoded. Russian and English therefore do not initialize or download the unrelated Chinese segmenter model.
3. Release the copied T3 checkpoint dictionary before constructing S3Gen to reduce peak memory while loading on a CPU.

The synthesis algorithms, trained weights, speaker conditioning, and Perth watermark remain intact. The worker sets Hugging Face and Transformers offline mode and calls `from_local` with the explicit model directory. The optional Russian stress-labeling package is not installed; the official tokenizer accepts Russian without it.

## Validation and licensing

The earlier runtime validation exercised Linux x86-64 Python 3.12 CPU installation and imports. The production worker generated the reviewed Victor line `Какие ещё деньги? О чём он говорит?` from the supplied Victor reference. The output was 3.68 seconds of 24 kHz mono audio; an independent Russian transcription recovered the expected words. Those historical results do not claim a new model installation or synthesis run for this source-layout update. The layout update checks local source copying, exact patch hashes, source immutability, packaged resource lookup and fallback extraction without downloading models or installing Python packages. Windows paths, interpreter discovery, package metadata, and setup dry runs are reviewed separately from an actual Windows installation.

Chatterbox code and the published model card declare MIT licensing; the source license is preserved as [LICENSE.chatterbox](LICENSE.chatterbox). The pinned Perth source is MIT licensed, preserved as [LICENSE.perth](LICENSE.perth). Python dependencies retain their own licenses in the installed distributions. The setup does not download reference voices; it copies the recordings explicitly supplied with `--reference`.

Official primary sources:

- <https://github.com/resemble-ai/chatterbox>
- <https://huggingface.co/ResembleAI/chatterbox>
- <https://github.com/resemble-ai/Perth>
- <https://download.pytorch.org/whl/cpu>
