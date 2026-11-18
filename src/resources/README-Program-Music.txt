Storyland program intro and error music
=======================================

These fallback_* WAV files are bundled to prevent silent intro screens and
error dialogs in clean Windows builds. Each contains a new, original
5.45-second, 44.1-kHz stereo PCM composition.
There are six separate compositions: one for the Storyland intro, and one
each for the five existing application error/warning themes.

The five original recordings used in previous configurations were absent
from the Storyland source archives supplied for this update. None of the
fallback cues is presented as one of those original recordings.

To build the executable with the original tracks instead, place the matching
WAV recordings in this directory using the filenames below:

    storyland_intro.wav
    reigns_error_theme.wav
    vcs_error_warning_theme.wav
    gta3_error_theme.wav
    gtavc_error_theme.wav
    gtasa_error_theme.wav

CMake and Storyland.rc automatically select the originals when present.
The fallback files can be kept; they will not take priority over originals.

For an already-built executable, put recordings beside Storyland.exe or in
its resources, music or audio folder. Both WAV and MP3/OGG/FLAC are accepted.
This approach does not require rebuilding the executable.

The startup splash also accepts storyland_intro.wav / .mp3 / .ogg / .flac
as a dedicated introduction, overriding the built-in intro music. The
Reigns application error theme is a separate cue and remains unaffected.

Use the bundled generator to reproduce the PCM fallback files:

    generate_program_themes <output-directory>

These are Storyland application interface cues. They do not replace, remove,
or disable in-game vehicle radio tracks, music streams or sound effects.
