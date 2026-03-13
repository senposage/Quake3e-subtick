OpenAL Soft — bundled headers
==============================

Version:  1.25.1
Source:   https://github.com/kcat/openal-soft/releases/tag/1.25.1
License:  Public domain (Unlicense) — see file headers

Files included
--------------
include/AL/al.h     — Core OpenAL 1.1 API
include/AL/alc.h    — ALC (context/device management) API
include/AL/alext.h  — Extension constants (ALC_SOFT_HRTF etc.)

These headers are used by code/client/snd_openal.c for type definitions
and constants only.  The OpenAL Soft library itself is loaded at runtime
via dlopen/LoadLibrary — no link-time dependency is required.

To update: replace the files in include/AL/ with the corresponding files
from the desired release tag on https://github.com/kcat/openal-soft.
