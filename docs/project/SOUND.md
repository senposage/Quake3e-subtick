# Sound System

> Covers `code/client/snd_main.c`, `snd_dma.c`, `snd_dmahd.c`, `snd_openal.c`, `snd_mem.c`, `snd_mix.c`, `snd_adpcm.c`, `snd_codec.c`, `snd_codec_wav.c`, `snd_wavelet.c`, and `snd_local.h`.
> This branch adds:
> - **OpenAL Soft backend** (`snd_openal.c`, guarded by `#ifdef USE_OPENAL`) — the **preferred** backend for click-free HRTF audio.
> - dmaHD HRTF/spatial sound fallback via `snd_dmahd.c` (guarded by `#ifndef NO_DMAHD`).

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [snd_main.c — Sound Dispatcher](#snd_mainc--sound-dispatcher)
3. [snd_openal.c — OpenAL Soft Backend [URT]](#snd_openalc--openal-soft-backend-urt)
4. [snd_dma.c — Base DMA Backend](#snd_dmac--base-dma-backend)
5. [snd_dmahd.c — High-Definition dmaHD Backend](#snd_dmahdcc--high-definition-backend)
6. [snd_mem.c — Sound Data Loading](#snd_memc--sound-data-loading)
7. [snd_mix.c — Mixing Engine](#snd_mixc--mixing-engine)
8. [Codecs](#codecs)
9. [Key Data Structures](#key-data-structures)
10. [Key Sound Cvars](#key-sound-cvars)

---

## Architecture Overview

```
Game calls S_StartSound(origin, entnum, channel, sfxHandle)
                │
                ▼
         snd_main.c  S_Init()
         ┌─────────────────────────────────┐
         │  1. Try S_AL_Init()  (USE_OPENAL)│  ← preferred: click-free HRTF
         │  2. Try S_Base_Init() → dmaHD    │  ← fallback: legacy DMA+HRTF
         └─────────────────────────────────┘
                │
         soundInterface_t *si (active backend)
                │
         ┌──────┴──────────────────────┐
         │                             │
    snd_openal.c               snd_dma.c + snd_dmahd.c
  (OpenAL Soft backend)        (DMA base + HD override)
         │                             │
    OpenAL Soft                  snd_mix.c
    (HRTF, mixing,               (channel mixing)
     resampling all                    │
     handled by OpenAL)               ▼
         │                      platform driver
         │                (sdl_snd / win_snd / linux_snd)
         │                             │
         └─────────────────────────────┘
                          │
                    OS audio device
```

**Backend selection priority (compile-time `USE_OPENAL=1`):**

| Priority | Backend | Guard | When used |
|----------|---------|-------|-----------|
| 1 | OpenAL Soft (`snd_openal.c`) | `USE_OPENAL` | If OpenAL library loads at runtime |
| 2 | dmaHD (`snd_dmahd.c`) | `NO_DMAHD=0` | If OpenAL unavailable and dmaHD enabled |
| 3 | Base DMA (`snd_dma.c`) | always | Final fallback |

The OpenAL backend is entirely self-contained — it does **not** use `snd_mix.c`, `snd_mem.c`, or the DMA buffer.  The DMA backends share the `snd_mix.c` paint path.

The sound system runs in the game thread (not a separate audio thread). `S_Update(msec)` is called from `CL_Frame` once per frame.

---

## snd_openal.c — OpenAL Soft Backend [URT]

**File:** `code/client/snd_openal.c`  
**Guard:** `#ifdef USE_OPENAL` (enabled by default; build with `USE_OPENAL=0` to disable)  
**New file** added in this branch.

### Why OpenAL Soft instead of dmaHD

The dmaHD backend suffered from recurring audio click/pop artefacts (PRs #70 #73 #74 #75) caused by:

1. **HP/LP filter warm-up contamination** — the resampling loop pre-fills a 4-sample warm-up from negative positions that wrap near the end of the source buffer; Hermite `x+2` lookahead then exceeded `soundLength` and wrapped to `sample[0]` (the attack peak), injecting spurious energy into `buffer[0..2]` — **a step discontinuity every time any sound started**.
2. **End-of-buffer spike** — the same wraparound at the last resampling iteration contaminated `buffer[outcount-1]` before the channel ended.

Both root causes were addressed in snd_dmahd.c (PR #75), but the fundamental complexity of managing custom IIR filter state, manual resampling, and hand-coded HP/LP filters creates ongoing maintenance risk.

**OpenAL Soft eliminates these artefacts structurally:**

- Buffer transitions are guaranteed click-free by the OpenAL specification — no filter warm-up state to manage.
- HRTF is provided by OpenAL Soft using measured HRIR datasets (CIPIC / MIT KEMAR) — no custom convolution code needed.
- Resampling is handled internally by OpenAL Soft at the device's native rate.
- Cross-platform: WASAPI (Windows), PipeWire/ALSA/PulseAudio (Linux), CoreAudio (macOS) — single code path.

### Dynamic Library Loading

The OpenAL library is loaded at **run-time** via `dlopen` / `LoadLibrary` so that:
- The executable has no mandatory OpenAL dependency.
- If OpenAL Soft is not installed the engine silently falls back to dmaHD.
- No link-time `-lopenal` flag required.

Library names tried (in order):

| Platform | Libraries |
|----------|-----------|
| Windows  | `OpenAL32.dll`, `soft_oal.dll` |
| Linux    | `libopenal.so.1`, `libopenal.so` |
| macOS    | `/System/Library/Frameworks/OpenAL.framework/OpenAL`, `libopenal.dylib` |

### HRTF Activation — Three Layers

HRTF is forced programmatically in three layers so users never need to run `alsoft-config.exe`:

**Layer 1 — `ALC_SOFT_output_mode` (OpenAL Soft 1.20+, most forceful):**
```c
ctxAttribs[] = { ALC_OUTPUT_MODE_SOFT, ALC_STEREO_HRTF_SOFT, ... }
```
`ALC_STEREO_HRTF_SOFT` tells OpenAL Soft to render to stereo headphones via HRTF regardless of device type.

**Layer 2 — `ALC_SOFT_HRTF` (all OpenAL Soft versions):**
```c
ctxAttribs[] = { ..., ALC_HRTF_SOFT, ALC_TRUE, ... }
```

**Layer 3 — `alcResetDeviceSoft` (belt-and-suspenders):**
If HRTF reports disabled after context creation, calls `alcResetDeviceSoft(device, {ALC_HRTF_SOFT, ALC_TRUE, 0})` to re-request it.

Status reported at startup: `s_info` shows `HRTF: enabled (built-in dataset)` or the specific dataset name from `alcGetStringiSOFT`.

Controlled by the `s_alHRTF` cvar (default `0` — off; enable with `s_alHRTF 1` for headphone users).

### Distance Model — Q3/dmaHD-Matching Linear Attenuation

Uses `AL_LINEAR_DISTANCE_CLAMPED` with parameters derived from Q3/dmaHD constants:

| Parameter | Value | Source |
|-----------|-------|--------|
| `AL_REFERENCE_DISTANCE` | 80 | `SOUND_FULLVOLUME` from snd_dma.c/snd_dmahd.c |
| `AL_MAX_DISTANCE` | 1330 | `80 + 1/SOUND_ATTENUATE(0.0008)` |
| `AL_ROLLOFF_FACTOR` | 1.0 | matches Q3 linear slope |

Formula: `gain = 1 - 1.0 * (dist - 80) / (1330 - 80)` — identical to Q3 base.

### Occlusion — `CM_BoxTrace` + EFX Low-Pass Filter + HRTF Gap Projection

At distance-adaptive intervals, for each playing 3D source, a ray is traced from
the listener through solid BSP geometry (`CONTENTS_SOLID` only —
`CONTENTS_PLAYERCLIP` is excluded so invisible movement-constraint geometry on
slopes and ledges does not cause false muffling):

```c
CM_BoxTrace(&tr, listenerOrigin, srcOrigin, vec3_origin, vec3_origin,
            0, CONTENTS_SOLID, qfalse);
occlusion = tr.fraction;  // 1.0 = clear, 0.0 = fully blocked
```

**With EFX** (`ALC_EXT_EFX`): Applied as a per-source `AL_DIRECT_FILTER` low-pass filter
using configurable floor curves (see [Occlusion filter tuning](#occlusion-filter-tuning-s_aloccgainfloor-s_alocchffloor)):
- `AL_LOWPASS_GAIN   = s_alOccGainFloor + (1 - s_alOccGainFloor) * occ`
- `AL_LOWPASS_GAINHF = s_alOccHFFloor   + (1 - s_alOccHFFloor)   * occ`

This makes occluded sounds lose treble energy (muffled through walls) rather than
just becoming quieter — matching real-world acoustics.  The filter is detached
entirely (`AL_FILTER_NULL`) when `occ > 0.98` to eliminate biquad phase-shift
coloration on fully-clear sources.

**Without EFX**: Simple `AL_GAIN` scale applied directly to the source.

**HRTF gap direction projection** (`s_alOccPosBlend`, `s_alOccSearchRadius`):
When the direct path is blocked, `S_AL_ComputeAcousticPosition` searches for the
nearest acoustic gap in 8 tangent-plane directions displaced `s_alOccSearchRadius`
units (default 120 u — approximately one URT doorway half-width) from the source.

The old approach placed the virtual `AL_POSITION` only `SEARCH_RADIUS` units from
the real source, which gave < 5° apparent HRTF direction shift even at full blend
— acoustically imperceptible.  The new approach **projects the gap direction onto
the source's actual distance** from the listener:

```
gapDir = normalize(acousticGap − listenerOrigin)
virtualPos = listenerOrigin + gapDir × distance(listener → source)
```

This means `s_alOccPosBlend` now directly controls an angular shift.  At the
default 0.40, a doorway 45° off the direct line produces a ~16° perceptible HRTF
direction change — the player hears the sound arriving *around the corner* rather
than straight through the wall.  The old hard-coded radius of 80 u has been
replaced with the tunable `s_alOccSearchRadius` (default 120 u).

### EFX Environmental Effects (`ALC_EXT_EFX`)

All EFX entry points are loaded via `alcGetProcAddress(device, ...)` at runtime — no link-time EFX dependency.

**Two auxiliary effect slots** are created:

| Slot | Effect | Used when |
|------|--------|-----------|
| Reverb slot | EAX reverb (or plain AL_EFFECT_REVERB fallback) | always — medium indoor room preset |
| Underwater slot | EAX reverb, heavy HF cut (GAINHF=0.1) | listener inside water volume |

**Indoor reverb initial preset** (EAX parameters — used as starting values and as the static baseline when `s_alDynamicReverb 0`):
- Density 0.5, Diffusion 0.85, Gain 0.32, Decay 1.49 s, HF ratio 0.83
- Reflections gain 0.25, Reflections delay 7 ms, Late reverb gain 0.50, Late reverb delay 11 ms, Air absorption 0.994

> When `s_alDynamicReverb 1` (default), **all nine EFX parameters are overridden every 16 frames** by the probe-derived environment classifier — the initial preset above is only a reasonable starting state before the first probe fires.  See [Dynamic acoustic environment](#dynamic-acoustic-environment-s_aldynamicreverb-1) for the full parameter list and formulas.

**Per-source occlusion filters**: One `AL_FILTER_LOWPASS` is allocated per source slot at init. The filter is updated by the occlusion trace and attached via `AL_DIRECT_FILTER`.  When a source slot is reused, the filter is **explicitly reset to pass-through** before being attached so that stale attenuation from a previous (possibly occluded) sound never bleeds into the new sound — this was the cause of the sporadic "suppressor" effect on multi-layer weapon events.

**EFX source wiring** (set in `S_AL_SrcSetup`):
```c
/* Non-local (3-D) sources: */
alFilterf(occlusionFilter[idx], AL_LOWPASS_GAIN,   1.0f);  /* pre-set to pass-through */
alFilterf(occlusionFilter[idx], AL_LOWPASS_GAINHF, 1.0f);
alSource3i(src, AL_AUXILIARY_SEND_FILTER, reverbSlot, 0, AL_FILTER_NULL);
alSourcei(src,  AL_DIRECT_FILTER, AL_FILTER_NULL);          /* bypass until first trace */
alSourcei(src,  AL_AUXILIARY_SEND_FILTER_GAIN_AUTO,   AL_TRUE);
alSourcei(src,  AL_AUXILIARY_SEND_FILTER_GAINHF_AUTO, AL_TRUE);
alSourcei(src,  AL_DIRECT_FILTER_GAINHF_AUTO,         AL_FALSE);

/* Local (own-player / weapon) sources: */
alSourcei(src, AL_DIRECT_FILTER, AL_FILTER_NULL);  /* clear any inherited filter */
/* AL_DIRECT_CHANNELS_SOFT only set when s_alHRTF 1 — see below */
```

> **Why `AL_DIRECT_CHANNELS_SOFT` for local sources (when `s_alHRTF 1`)**: even a
> source at `AL_POSITION (0,0,0)` relative to the listener is processed through the
> HRTF "center" HRIR when HRTF is enabled.  That convolution smears the initial
> transient attack of weapon sounds (e.g. `de.wav`).  `AL_DIRECT_CHANNELS_SOFT = AL_TRUE`
> routes the PCM directly to the stereo output, completely bypassing the HRTF kernel
> for head-locked sounds.  This is only applied when `s_alHRTF 1` — with HRTF off
> there is no convolution to bypass so the call is skipped.

> **Why start 3D sources with `AL_FILTER_NULL`**: attaching the occlusion filter object even at GAIN=1/GAINHF=1 (passthrough) still routes the signal through a biquad, which introduces group-delay coloration on the transient attack.  Starting with `AL_FILTER_NULL` (complete signal bypass) lets the occlusion update tick connect the filter only when actual attenuation is needed.

### Ambisonic Panning

Two aspects of Ambisonic integration:

**Internal HRTF rendering order** — At context creation, the engine queries `ALC_MAX_AMBISONIC_ORDER_SOFT` and requests the highest order ≤ 3:
```c
ctxAttribs[] = { ..., ALC_AMBISONIC_ORDER_SOFT, maxOrder, ... }
```
OpenAL Soft uses this order for its internal Ambisonic encoder when mixing 3D sources before applying HRTF — higher order gives better spatial resolution (order 3 = 16 B-format coefficients).

**B-format source loading** — 4-channel audio files are detected and loaded with the correct B-format enum so OpenAL Soft decodes them as first-order Ambisonics:
```c
if (info.channels == 4)
    fmt = (info.width == 2) ? AL_FORMAT_BFORMAT3D_16 : AL_FORMAT_BFORMAT3D_8;
```
This allows map ambient sounds encoded as WXYZ B-format to provide full 360° HRTF audio with no extra engine code.

### Source Pool

Sources are allocated on demand up to `s_alMaxSrc` (default 512).  `S_AL_GetFreeSource`
uses a three-pass strategy: find a stopped (free) slot, grow the pool dynamically, or
steal by priority (world-entity farthest first, then other-entity farthest, then oldest).
See [Current implementation — slot-based model](#current-implementation--slot-based-model)
for the full details.

### Music Streaming

Background music is streamed via a dedicated AL source and `S_AL_MUSIC_BUFFERS` (4) ping-pong buffers of 32 KiB each, refilled from the codec in `S_AL_Update`.

### Raw Samples

`S_RawSamples` (cinematics, VoIP) queues PCM data through a dedicated streaming source with `S_AL_RAW_BUFFERS` (8) rotating buffers.

### Key CVars

| Cvar | Default | Description |
|------|---------|-------------|
| `s_alDevice` | `""` | OpenAL output device; empty = system default. Type `/s_devices` for a numbered pick-list. (LATCH) |
| `s_alHRTF` | `0` | Enable HRTF binaural rendering via OpenAL Soft. **Default 0 (off)** — only enable when using headphones; on speakers HRTF smears weapon transients. (LATCH) |
| `s_alEFX` | `1` | Enable ALC_EXT_EFX (occlusion filters, reverb aux slots). Set to 0 to skip EFX entirely for diagnostic isolation. (LATCH) |
| `s_alDirectChannels` | `1` | Route own-player (local) sources directly to the stereo output, bypassing the HRTF centre-HRIR convolution. Only active when `s_alHRTF 1` — has no effect when HRTF is off (default). Requires `AL_SOFT_direct_channels` extension. (LATCH) |
| `s_alMaxDist` | `1330` | Max attenuation distance in game units; matches vanilla dma range |
| `s_alReverb` | `1` | Master EFX reverb toggle. **Default 1 = on** |
| `s_alReverbGain` | `0.35` | EFX wet slot gain [0–1]. Ceiling when `s_alDynamicReverb 1` |
| `s_alReverbDecay` | `1.49` | Reverb decay in seconds; used when `s_alDynamicReverb 0` (LATCH) |
| `s_alDynamicReverb` | `1` | Ray-traced acoustic environment. **Default 1 = on** |
| `s_alDebugReverb` | `0` | Debug output for dynamic reverb. `1` = env label + smoothed params every probe cycle. `2` = also raw probe data and filter-reset events. Not archived. |
| `s_alOcclusion` | `1` | Wall-occlusion tracing with HRTF direction correction |
| `s_alOccGainFloor` | `0.55` | Floor of `AL_LOWPASS_GAIN` for fully-blocked sources [0–1]. `1.0` = no volume change through walls (vanilla-like). `0.0` = can go fully silent. Default 0.55 keeps occluded sounds audible with a modest volume jump at line-of-sight. |
| `s_alOccHFFloor` | `0.50` | Floor of `AL_LOWPASS_GAINHF` (high-frequency content) for fully-blocked sources [0–1]. `1.0` = no HF filtering (only volume changes). `0.0` = bass-only thud through walls. Default 0.50 preserves enough weapon crack to remain recognisable. Old formula `occ²` reached near-zero HF even at moderate occlusion — this was the main cause of the "tinny pop" corner transition. |
| `s_alOccPosBlend` | `0.40` | How far to redirect the HRTF apparent-source position towards the nearest acoustic gap when a source is occluded [0–1]. The gap position is **projected to the source's actual distance** so the value directly controls an angular shift: at 0.40 a doorway 45° off the direct line gives ~16° apparent HRTF direction change. `0.0` = always true source position (no gap hint). `1.0` = full redirect to gap direction. Default 0.40 (raised from 0.25 — the projection makes larger values perceptually meaningful). |
| `s_alOccSearchRadius` | `120` | Tangent-plane probe radius (game units) used to find the nearest acoustic gap when a source is occluded. Default 120 ≈ URT door half-width. Increase to 160–200 for wide doorways or thick pillars; decrease for tighter gap sensitivity. |
| `s_alDebugOcc` | `0` | Print per-source occlusion state each frame. Each occluded source shows entity, distance, trace target, smoothed gain, GAIN and GAINHF values. Use to identify which sounds are being filtered and by how much. Not archived. |
| `s_alDebugPlayback` | `0` | Playback diagnostics for isolating audio quality issues. **`1`** = log every **PREEMPT** (sound cut short by a new sound on the same channel) and every **rate-mismatch** at load time (file Hz ≠ device Hz, which forces the internal resampler). **`2`** = also log every natural completion. Each line shows sound name, samples played / total, % consumed, file rate, and device rate. Use `1` to determine whether the DE-50 boom is being **truncated** (preempt line, low %) or **degraded** by the resampler (rate-mismatch lines at registration). Not archived — set before loading a map to catch registration warnings. |

#### Volume mixer — per-category controls (0–10 scale)

All volume cvars use a **0–10 scale** where **1.0 = reference** (the historically tuned default level for that category). A **power-2 curve** is applied below 1.0 so that small reductions feel proportional to loudness perception (0.5 input → ≈ −12 dB, clearly audible; 0.1 input → ≈ −40 dB, nearly silent). Above 1.0 the scale is linear.

| Cvar | Default | Ref gain | Max | Description |
|------|---------|----------|-----|-------------|
| `s_alVolSelf` | `1.0` | 1.00 | 10 | Own player movement, breath, general entity sounds |
| `s_alVolWeapon` | `1.0` | 1.00 | 10 | Own weapon fire (CHAN_WEAPON from listener entity) — split from movement so weapon loudness is tunable independently |
| `s_alVolOther` | `1.0` | 0.70 | 2 | Other players/entities. Capped at 2× (anti-cheat — prevents wallhack-level amplification) |
| `s_alVolImpact` | `1.0` | 0.55 | 2 | World entity impacts: bullet hits, brass casings, explosions. Often disproportionately loud; capped at 2× (anti-cheat) |
| `s_alVolEnv` | `1.0` | 0.30 | 10 | Looping ambient/environmental sounds |
| `s_alVolUI` | `1.0` | 0.80 | 10 | Hit-markers, kill-confirmations, menu sounds (`StartLocalSound` with entnum=0) |
| `s_alExtraVol` | `1.0` | 0.70 | 1 | Volume for sounds matched by `s_alExtraVolList`. **Reduce-only** (max 1.0, floor 0.25). Ref 0.70 means cvar=1.0 already applies a 30% reduction. Power-2 curve below 1.0. |

**Example**: `s_alVolEnv 0.5` reduces ambient volume to 25% of reference (−12 dB); `s_alVolEnv 2.0` doubles it (+6 dB).

#### Extra-vol slots — per-player custom loud-sound group

Eight individual cvars — `s_alExtraVolEntry1` through `s_alExtraVolEntry8` — each hold one sound path pattern. Because they use plain `CVAR_ARCHIVE` (no `CVAR_NODEFAULT`), **all eight slots always appear in the config file**, one line each, making them trivial to find and edit without touching the console.

**Slot syntax** (one pattern per cvar):

| Value | Effect |
|-------|--------|
| `sound/feedback/hit.wav` | Include only that specific file |
| `sound/feedback` | Include every sound under that directory |
| `sound/feedback/hit.wav:-2.5` | Include and apply an extra **−2.5 dB** (≈25% amplitude) cut on top of `s_alExtraVol` |
| `!sound/feedback/quiet.wav` | **Exclude** that file even if a positive slot matched it |
| *(empty)* | Slot unused — skipped |

A sound is routed to the extra-vol group when **at least one positive slot matches** and **no exclusion slot matches**. The `:-N` suffix is a per-sample dB cut (≤ 0; floor −40 dB; fractional values OK) applied on top of the global `s_alExtraVol` scalar.

**Config file appearance** (default):
```
seta s_alExtraVolEntry1 "sound/feedback/hit.wav:-2.5"
seta s_alExtraVolEntry2 "sound/feedback/kill.wav:-2.5"
seta s_alExtraVolEntry3 ""
seta s_alExtraVolEntry4 ""
seta s_alExtraVolEntry5 ""
seta s_alExtraVolEntry6 ""
seta s_alExtraVolEntry7 ""
seta s_alExtraVolEntry8 ""
seta s_alExtraVol "1.0"
```

| Cvar | Default | Description |
|------|---------|-------------|
| `s_alExtraVolEntry1`–`8` | slots 1–2 pre-filled, 3–8 empty | One pattern per slot. Prefix `!` to exclude. Append `:-N` (dB ≤ 0) for a per-sample cut on top of `s_alExtraVol`. |
| `s_alExtraVol` | `1.0` | Global scalar for all matched sounds [0.25–1.0]. At default (1.0) no extra reduction beyond per-slot `:-N` cuts. Floored at 0.25 so matched sounds are never fully silenced. |

**Examples**:
- Edit `s_alExtraVolEntry1` to `sound/feedback/hit.wav:-5` for a deeper cut on hit sounds
- Set `s_alExtraVolEntry3` to `sound/feedback` to catch any other feedback sounds
- Set `s_alExtraVolEntry4` to `!sound/feedback/quiet.wav` to exclude a specific file

#### Loop-sound fade controls

Looping ambient sources fade in when an entity enters the player's PVS and fade out when it leaves, preventing sudden audio pops.

| Cvar | Default | Description |
|------|---------|-------------|
| `s_alLoopFadeInMs` | `800` | Fade-in duration in ms for new loop sources. `0` = instant (no fade). |
| `s_alLoopFadeOutMs` | `800` | Fade-out duration in ms when a source leaves PVS. `0` = instant cut. The fade always starts from the source's **current gain level** — if it was still fading in when removed, it fades out from there (not from full volume), preventing pops. |
| `s_alLoopFadeDist` | `400` | Distance zone (game units) inside the hearing boundary within which new sources start at a **distance-proportional gain** rather than silence. Eliminates the "ramp-in from zero" artefact for sources appearing while already audible. `0` = always start from silence (old behaviour). |

#### EFX additions: echo, chorus, fire-impact reverb

| Cvar | Default | Description |
|------|---------|-------------|
| `s_alEcho` | `1` | Geometry-driven echo on EFX auxiliary send 1. Adds a discrete reflection layer to 3D sources in enclosed spaces. Slot gain is driven automatically by `s_alDynamicReverb` room classification. Requires `maxSends >= 2`. Default 1 (on). |
| `s_alEchoGain` | `0.10` | Maximum wet gain for the echo slot [0–0.3]. Actual gain is scaled by enclosure. Default 0.10. |
| `s_alFireImpactReverb` | `1` | Transient early-reflection boost on weapon fire — **own fire and incoming enemy fire**. Own fire: casts 3 rays in aim direction; a wall within 400 units boosts `curRefl`/`curDecay`/`curSlot` **directly on the smoothed output** so the spike is immediate and classifier-independent (a weapon fired at a wall always produces an audible muzzle report regardless of what environment the probe is reporting). Incoming enemy fire: uses normalised distance for a proportional boost (half the own-fire max). Suppressed weapons excluded from both paths. **When `s_alDynamicReverb 0`** a dedicated static-mode path (`S_AL_UpdateStaticFireBoost`) provides the same muzzle-report spike directly against the EFX baseline — so the effect works in any reverb mode. Default 1 (on). |
| `s_alFireImpactMaxBoost` | `0.25` | Maximum reflections gain for own-fire boost [0–0.5]. Incoming-enemy boost is capped at half this value. Default 0.25. |

#### Grenade-concussion EFX bloom

URT grenades are already loud enough to produce natural perceptual ducking via OpenAL's gain model. The bloom effect adds **acoustic character** (a brief reverb surge) without reducing audio clarity.

| Cvar | Default | Description |
|------|---------|-------------|
| `s_alGrenadeBloom` | `1` | Grenade-blast concussion effect: (1) EFX reverb slot gain spike that makes the room sound momentarily bigger; (2) per-source HF lowpass cut (`s_alGrenadeBloomHFFloor`) that turns the event from "volume went down" into "something just exploded 15 feet from me". Enemy grenades only. Requires `s_alReverb 1` for reverb component. Default 1 (on). |
| `s_alGrenadeBloomRadius` | `400` | Blast radius (game units) that triggers the bloom + HF effect. Default 400. |
| `s_alGrenadeBloomGain` | `0.12` | Peak reverb slot gain boost above current level [0–0.3]. Default 0.12. |
| `s_alGrenadeBloomMs` | `350` | Recovery time for both the reverb bloom decay and the HF muffling in ms. Default 350. |
| `s_alGrenadeBloomHFFloor` | `0.05` | Minimum `AL_LOWPASS_GAINHF` at peak of a grenade blast [0–1]. 0.05 ≈ −26 dB HF — an explosion nearby strips nearly all high frequencies momentarily, leaving a bassy rumble. Applies per-source to every playing sound. Default 0.05. |
| `s_alGrenadeBloomDuck` | `0` | **Opt-in** mild listener-gain duck alongside the HF effect. Usually not needed since the HF filter already sells the impact. Default 0. |
| `s_alGrenadeBloomDuckFloor` | `0.82` | Minimum listener gain during optional grenade duck [0.5–0.95]. Default 0.82. |

#### Hearing disruption — incoming fire, head hits, and health fade

All effects share a single infrastructure: per-source `AL_LOWPASS_GAINHF` cuts applied every frame through the existing occlusion filter path. **This replaces the old "general duck" feel** (listener `AL_GAIN` reduction only) with a convincing muffled/concussed hearing simulation:

- Own weapon fire sounds bassy and clipped
- Enemy footsteps and callouts become hard to read
- Recovery is gradual — you hear the world "coming back"
- `SRC_CAT_UI` sources (kill markers, the tinnitus ring itself) are intentionally excluded — they are mental/HUD feedback that should stay crisp

Multiple triggers can overlap; the system always takes the later expiry and the deeper HF cut.

| Cvar | Default | Description |
|------|---------|-------------|
| `s_alSuppression` | `0` | Toggle for near-miss and incoming-fire hearing disruption. Enables: (A) near-miss HF muffling triggered by whiz sounds (`s_alNearMissPattern`); (B) radius-fallback from nearby enemy `CHAN_WEAPON` fire. The HF cut is **directional** — applied only within the cone toward the fire source (front) plus a softer rear contribution, so you retain clarity from sources beside you. Teammates and suppressed weapons excluded automatically. Head-hit disruption has its own toggle: `s_alHeadHit`. Default 0 (opt-in). |
| `s_alSuppressionRadius` | `180` | Fallback trigger radius for enemy weapon fire (game units). The primary trigger is the whiz-sound name match which is more precise. Default 180 ≈ one room width. |
| `s_alSuppressionFloor` | `0.45` | Minimum listener `AL_GAIN` (volume) during suppression [0–0.95]. Secondary to the HF filter — provides the physical "jolt". Default 0.45 (≈ −7 dB). |
| `s_alSuppressionMs` | `220` | Duration of near-miss / incoming-fire hearing disruption in ms. Both the volume duck and the HF muffling recover linearly. Default 220. |
| `s_alSuppressionHFFloor` | `0.08` | Minimum `AL_LOWPASS_GAINHF` at peak suppression [0–1]. **Primary cue**: 0.08 ≈ −22 dB HF — sources in the fire direction go noticeably bassy/distorted. Applied per-source within the directional cone. Default 0.08. |
| `s_alSuppressionConeAngle` | `120` | Full cone angle (degrees) of the directional HF suppression [10–360]. Sources within ±half-angle of the incoming fire direction get the full cut; sources outside get none (except the rear partial). 360 = omnidirectional. Default 120 (±60°). Pairs naturally with `s_alHRTF 1` for maximum directional accuracy. |
| `s_alSuppressionRearGain` | `0.35` | Fraction of the HF suppression applied behind the listener (opposite the fire direction) [0–1]. Secondary cue: reinforces the direction of fire without muffling side-facing sounds. 0 = strict cone only. Default 0.35. |
| `s_alSuppressionReverbBoost` | `0.18` | Reverb slot gain spike added when suppression triggers [0–0.5]. Briefly boosts the wet reverb tail so the acoustic space "splashes" with the concussion, then decays back over `s_alSuppressionMs`. Default 0.18. |
| `s_alNearMissPattern` | `whiz1,whiz2` | Comma-separated sound-name substrings identifying near-miss bullet whiz sounds. Matched case-insensitively. URT uses `sound/weapons/whiz1.wav` and `whiz2.wav`. When matched, immediately triggers the suppression event — more precise than the radius fallback. |

#### Head-hit triggers

| Cvar | Default | Description |
|------|---------|-------------|
| `s_alHeadHit` | `1` | Standalone toggle for head-hit hearing disruption + tinnitus. Active independently of `s_alSuppression` — enables helmet/bare-head hit feedback without enabling the near-miss/incoming-fire system. Default 1 (on). |
| `s_alHelmetHitPattern` | `helmethit` | Sound-name substrings for helmet-hit detection. When the local player's entity plays a `CHAN_BODY` sound matching this, fires a hearing-disruption event + tinnitus ring. URT uses `sound/helmethit.wav`. Active when `s_alHeadHit 1` or `s_alSuppression 1`. |
| `s_alHelmetHitMs` | `350` | Duration of helmet-hit hearing disruption (ms). Longer than a near-miss — you were actually struck. Default 350. |
| `s_alHelmetHFFloor` | `0.10` | Minimum HF gain for helmet-hit disruption [0–1]. Deeper than incoming-fire (0.10 vs 0.15). Default 0.10 (≈ −20 dB HF at peak). |
| `s_alBareHeadHitPattern` | `headshot` | Sound-name substrings for bare-head (no helmet) hit detection. URT uses `sound/headshot.wav` for unprotected headshots. Fires the strongest disruption tier. Active when `s_alHeadHit 1` or `s_alSuppression 1`. |
| `s_alBareHeadHitMs` | `500` | Duration of bare-head hit disruption (ms). Longest tier. Default 500. |
| `s_alBareHeadHFFloor` | `0.03` | Minimum HF gain for bare-head hit [0–1]. Most severe: 0.03 ≈ −30 dB HF, nearly bass-only at peak. Default 0.03. |

#### Synthesised tinnitus tone

A pure sine tone generated entirely from PCM at init — **no sound file required**. Played as a non-positional local source (`SRC_CAT_UI`, excluded from HF filter so the ring cuts through the muffling). Rebuilt automatically when frequency or duration CVars change.

| Cvar | Default | Description |
|------|---------|-------------|
| `s_alTinnitusFreq` | `3500` | Tone frequency in Hz [200–8000]. 3500 Hz sits in the most sensitive region of human hearing. Default 3500. |
| `s_alTinnitusDuration` | `700` | Ring duration in ms [50–3000]. 20 ms linear attack then quadratic decay to silence. Default 700. |
| `s_alTinnitusVol` | `0.45` | Playback volume [0–1], applied as raw `AL_GAIN` independent of category vol knobs. 0 = silent (disables tinnitus without affecting other disruption effects). Default 0.45. |
| `s_alTinnitusCooldown` | `800` | Minimum gap between successive tinnitus plays in ms. Prevents stacking from rapid hits. Default 800. |

#### Health-based HF fade

| Cvar | Default | Description |
|------|---------|-------------|
| `s_alHealthFade` | `0` | Opt-in health-based audio fade. Below `s_alHealthFadeThreshold` HP, the per-source HF filter gradually reduces — the world grows muffled as the player nears death. Zero effect at or above the threshold. Default 0. |
| `s_alHealthFadeThreshold` | `10` | HP below which the fade activates [5–100]. Filter is flat at this HP and reaches `s_alHealthFadeFloor` at 1 HP. Default 10. |
| `s_alHealthFadeFloor` | `0.35` | Minimum HF gain at 1 HP [0–1]. 0.35 ≈ −9 dB HF at death's door — noticeably muffled but footsteps and shots remain identifiable. Default 0.35. |


#### Suppressed-weapon audio tuning

Suppressors in URT are weapon **attachments** — the weapon ID is unchanged. Detection uses two independent signals OR'd together (both are cheap string lookups):

1. **Gear string** (`g` key in `CS_PLAYERS` configstring, character `'U'` = suppressor fitted) — authoritative game state, always checked for player entities
2. **Sound file name** pattern match (`s_alSuppressedSoundPattern`) — catches non-player entities and cases where the gear string is absent

Both checks always run. URT suppressed file names confirmed from pk3 audit: `de_sil`, `m4_sil`, `ak103_sil`, `beretta_sil`, `g36_sil`, `glock_sil`, `psg1_sil`, `ump45_sil`, `lr_sil` (`_sil`), `mac11-sil` (`-sil`), `p90_silenced` (`silenced`).

| Cvar | Default | Description |
|------|---------|-------------|
| `s_alSuppressedSoundPattern` | `silenced,-sil,_sil,_sd_,_sd.,suppressed,suppressor` | Filename substrings for the name-based fallback check. Both `-sil` (mac11) and `_sil` (all others) are included to cover every URT suppressed weapon. Matched sounds: route through `s_alVolSuppressedWeapon` (own) or `s_alVolEnemySuppressedWeapon` (enemy), skip near-miss suppression duck, skip incoming-fire reverb boost. |
| `s_alVolSuppressedWeapon` | `1.0` | Own suppressed weapon fire volume [0–10, ref 0.55]. Suppressed weapons are inherently quieter — ref 0.55 reflects this. Power-2 curve below 1.0. Default 1.0. |
| `s_alVolEnemySuppressedWeapon` | `1.0` | Enemy suppressed weapon fire volume [0–2, ref 0.45]. Lower reference (0.45) than own suppressed (0.55) — enemy suppressed fire is designed to be harder to locate. Capped at 2× (anti-cheat). Default 1.0. |


#### Diagnosing weapon-sound quality with `s_alDebugPlayback`

`s_alDebugPlayback` produces three kinds of console lines — use them together to
pinpoint whether a broken weapon sound is a **truncation** problem or a
**resampler** problem:

| Prefix | Colour | What it means |
|--------|--------|---------------|
| `[alDbg] rate mismatch` | Yellow | Printed when a file is loaded and its Hz ≠ device Hz. This is the condition that forces OpenAL's internal resampler to run. If the DE-50 shot file appears here, the resampler is active on it. |
| `[alDbg] PREEMPT` | Red | A playing sound was cut short because a new sound started on the same channel (`chan=N`) for the same entity. `played X / Y smp (Z%)` shows how far through the sample it got. A low % on the DE boom means it is being truncated — look at the "by:" field to see which sound caused it. |
| `[alDbg] done` | Green | Natural completion (level 2 only). Should show ~100% for correctly played one-shot sounds. |

**Workflow:**

```
// 1. Before loading any map (catches registration warnings):
s_alDebugPlayback 1

// 2. Load the map; watch for yellow "rate mismatch" lines on DE-50 sounds.
//    The startup line "S_AL: device mixing frequency: NNNNN Hz" tells you
//    what rate the device is running at.

// 3. Fire the DE-50.  Watch for:
//      RED   PREEMPT lines  → truncation/channel-preemption is the bug
//      no PREEMPT + yellow rate-mismatch at load → resampler is the bug

// 4. For full natural-completion audit:
s_alDebugPlayback 2
```



Type `/s_devices` in the console for a numbered pick-list:

```
Audio output devices  (> = active  * = system default):
  >*  1.  Realtek High Definition Audio
      2.  Razer Kraken 7.1 Chroma
      3.  HDMI Output (LG Monitor)

Usage: set s_alDevice "Razer Kraken 7.1 Chroma" ; vid_restart
       (empty string restores the system default)
```

The cvar description shown when you query `s_alDevice` also embeds the actual
system-default device name (resolved at runtime after the library loads).
`sndinfo` includes the same device list at the bottom of its output.

#### Audio enhancement ladder

```
s_alReverb 0  s_alOcclusion 0                        →  vanilla dma: distance-only, no wall effects
s_alReverb 0  s_alOcclusion 1                        →  occlusion + HRTF gap-direction correction
s_alReverb 1  s_alOcclusion 1  s_alDynamicReverb 0  →  static EFX reverb + fire-impact boost (all environments)
s_alReverb 1  s_alDynamicReverb 1                    →  dynamic ray-traced reverb, 9 env profiles, 9 EFX params (default)
```

#### Channel preemption / slot model

See [Global Audio Preemption — Design and Implementation](#global-audio-preemption--design-and-implementation)
for the full description of how `S_AL_StartSound` allocates independent slots
for every sound submission — no preemption, no guards, each sound plays to
completion exactly as in the DMA backend.

#### Vanilla-behaviour notes

- **`s_alReverb 1` (default)**: EFX reverb enabled with dynamic ray-traced environment (9 profiles, 9 EFX params).
- **`s_alOcclusion 1` (default)**: sources behind walls are gain-attenuated and their
  apparent HRTF direction is corrected toward the nearest acoustic gap
  (`s_alOccPosBlend`, default 0.40; `s_alOccSearchRadius`, default 120 u).
  The gap position is projected to the source's actual distance before blending,
  producing a clear angular shift rather than a small positional nudge.
  Only `CONTENTS_SOLID` brushes are tested —
  `CONTENTS_PLAYERCLIP` is excluded so invisible movement-constraint geometry on
  slopes and ledges does not cause false muffling of nearby weapon fire.
- **CHAN_WEAPON proximity bypass**: weapon-channel sources within
  `s_alOccWeaponDist` (default 160 u) of the listener skip occlusion
  entirely, covering the gun-muzzle offset (~50–100 u ahead of the player eye) that
  would otherwise trigger false filter attenuation from nearby slope/clip geometry.
- `s_alMaxDist 1330` is the vanilla dma maximum audible radius; sounds beyond
  this distance are inaudible regardless of wall status.

#### Ambient looping sound fades

Looping ambient sources (fountains, machinery, wind, etc.) fade in and out
instead of starting and stopping abruptly:

| Event | Duration | Effect |
|---|---|---|
| Source starts / re-enters range | `S_AL_LOOP_FADEIN_MS` = 600 ms | Gain 0 → 1 (cold-start pop eliminated) |
| Entity leaves PVS / out of range | `S_AL_LOOP_FADEOUT_MS` = 500 ms | Gain 1 → 0 (hard cut eliminated) |
| Entity re-enters during fade-out | Immediate | Fade-out cancelled; fade-in from current level |

#### Ambient loop deduplication (same-sfx entities)

DMA's `S_AddLoopSounds()` merges all map entities sharing the same sfx into a
**single mixer channel**, summing their spatial volumes.  Without equivalent
deduplication, the OpenAL slot-based model would start one AL source per entity,
giving N× volume for maps that place N ambient entities using the same file
(confirmed on `ut4_austria`: three entities all use `Bach-Prelude_No_30_D_minor.wav`,
producing triple-volume output and three stop-log entries at identical sample
offsets).

`S_AL_UpdateLoops` now mirrors this behaviour: before allocating a new AL source
for entity `i`, it scans **all** other loop slots — both lower- and higher-index.
If any other *active* entity already has a playing source for the same sfx, entity
`i` is skipped (no new source allocated).  This prevents double-start regardless
of which entity entered PVS first.  When the owning entity leaves PVS its `active`
flag clears immediately, allowing any remaining duplicate to claim a source on the
next frame (smooth cross-fade rather than a hard cut).

#### Occlusion update cadence (distance-adaptive)

The occlusion system uses a **three-phase design** to give smooth wall↔clear
transitions without per-frame CM_BoxTrace overhead:

| Phase | Runs | What it does |
|---|---|---|
| **1 — Trace** | On interval (see table) | Runs `CM_BoxTrace`, updates `occlusionTarget` + `acousticOffset` |
| **2 — Smooth** | Every frame | IIR-blends `occlusionGain` toward target. Opening: step 0.30 (fast snap on LOS). Closing: step 0.18 (gradual muffle, no pop) |
| **3 — Apply** | Every frame | Writes position + filter using smoothed values. Detaches filter entirely (`AL_FILTER_NULL`) when `occ > 0.98` to eliminate biquad phase-shift coloration on clear sources |

| Source distance | Trace interval | Rationale |
|---|---|---|
| < 80 u (full-vol zone) | every frame — no trace | Wall impossible at this range; real position used |
| CHAN_WEAPON < `s_alOccWeaponDist` u | every frame — no trace | Gun-muzzle zone; always pass-through |
| 80 – 300 u | every frame | Nearby players — maximum HRTF precision |
| 300 – 600 u | every 4 frames | Medium range |
| > 600 u | every 8 frames | Far — distance model dominates |

#### Occlusion filter tuning (`s_alOccGainFloor`, `s_alOccHFFloor`)

The filter applied to occluded sources uses linear floor→1.0 curves:

```
AL_LOWPASS_GAIN   = s_alOccGainFloor + (1 - s_alOccGainFloor) * occ
AL_LOWPASS_GAINHF = s_alOccHFFloor   + (1 - s_alOccHFFloor)   * occ
```

| occ | GAIN (floor=0.55) | GAINHF (floor=0.50) | Old GAIN | Old GAINHF (occ²) |
|-----|-------------------|----------------------|----------|-------------------|
| 0.1 (thick wall) | 0.595 | 0.545 | 0.325 | 0.010 |
| 0.3 (partial)    | 0.685 | 0.650 | 0.475 | 0.090 |
| 0.5 (moderate)   | 0.775 | 0.750 | 0.625 | 0.250 |
| 0.8 (slight)     | 0.910 | 0.900 | 0.850 | 0.640 |
| 1.0 (clear)      | 1.000 | 1.000 | 1.000 | 1.000 |

Old `occ²` for GAINHF reached 0.01 through any solid wall — stripping ~99% of
high-frequency weapon-fire content and causing the "tinny pop" when rounding a
corner (extreme bass → sudden full crack).  The new defaults keep the timbre
recognisable at all occlusion levels.

#### Occlusion debugging workflow

Use `s_alDebugOcc 1` to print per-source filter state each frame, then isolate
the cause of a specific sound issue:

```
[alOcc] ent=   5 dist=  350  tgt=0.34 cur=0.56  G=0.86 GHF=0.78
         │            │         │         │       │       └─ LOWPASS_GAINHF applied
         │            │         │         │       └─ LOWPASS_GAIN applied
         │            │         │         └─ smoothed occlusionGain (IIR)
         │            │         └─ raw trace result (occlusionTarget)
         │            └─ distance to listener
         └─ entity number
```

**Isolating the filter vs. position-redirect vs. other:**

| Command | Effect | What it tests |
|---|---|---|
| `s_alOcclusion 0` | No filter, no position redirect, real pos | Baseline: is HRTF/distance model the issue? |
| `s_alOccGainFloor 1; s_alOccHFFloor 1` | No frequency filtering, position redirect only | Is it the lowpass filter causing the problem? |
| `s_alOccPosBlend 0` | No position redirect, filter only | Is the HRTF direction jump causing the issue? |
| `s_alOccGainFloor 1` | No volume change, HF-only filter | Is volume attenuation causing the pop? |
| `s_alOccHFFloor 1` | No HF change, volume-only filter | Is HF stripping causing the tinny/pop effect? |

#### Source pool management (30+ player overload)

With 30+ players firing simultaneously the 96-source pool can exhaust. Three
DMA-matching mechanisms prevent degradation:

1. **Distance-based eviction**: `S_AL_GetFreeSource` evicts the *farthest* non-loop
   3D source rather than the oldest — nearby (high-impact) sounds survive.
2. **World-entity cap**: `ENTITYNUM_WORLD` (bullet impacts, casings) is capped at
   `numSrc / 8` = 12 sources, matching DMA's `WORLD_ENTITY_MAX_CHANNELS`.
3. **Per-entity dedup**: same sfx on the same entity within 20 ms (100 ms for world
   impacts) is suppressed, matching DMA's dedup window.

#### Own-player audio (DMA comparison)

DMA plays own-entity sounds at `leftvol = rightvol = master_vol` — pure stereo
mix, no filter, no spatialization.  OpenAL matches this via:

- `s_alLocalSelf 0` (default): own-entity sounds are placed at the engine's
  authoritative predicted listener position (`s_al_listener_origin`, set by
  `S_AL_Respatialize` from Pmove each frame) for full 3D spatialization.
  `s_alVolSelf` **always** controls own-entity volume regardless of this
  setting — in 3D mode the source is classified `SRC_CAT_LOCAL` based on
  entity-number match with the listener, not on the `isLocal` flag.
- `s_alLocalSelf 1`: own-entity sounds are forced non-spatialized →
  `AL_SOURCE_RELATIVE=TRUE`, position (0,0,0), no rolloff,
  `AL_DIRECT_FILTER = AL_FILTER_NULL`.  No longer needed to work around
  position staleness (that is now fixed in the engine — see below).  Exists
  for users who prefer fully head-locked own-player sounds.
- **Stale reverb fix**: when a local source reuses a slot previously used for a
  3D sound, the auxiliary send is now explicitly disconnected — preventing an
  unwanted reverb tail on own-player weapons and footsteps.

#### Own-player sound position — `S_AL_StartSound` + `S_AL_UpdateEntityPosition` [CUSTOM]

When the cgame calls `trap_S_StartSound(cent->lerpOrigin, cg.clientNum, ...)` or
`trap_S_UpdateEntityPosition(cg.clientNum, cent->lerpOrigin)` for the local
player entity, the supplied origin comes from the network trajectory evaluated at
`cg.time`.  With **Patch 1** (TR_LINEAR) active or `sv_bufferMs` delaying the
snapshot `trBase`, this position lags the client-predicted eye position by an
amount proportional to velocity — typically 5–20 game units at normal running
speed with a 50 ms buffer.

The engine corrects this in two places, strictly for the listener entity only
(all other entities — other players, world, bots — continue to use the
cgame-supplied origin):

1. **`S_AL_StartSound`** (`else if (origin)` branch): when
   `entnum == s_al_listener_entnum` and an explicit origin is provided,
   substitute `s_al_listener_origin` instead of the cgame-supplied origin.
   `s_al_listener_origin` is set by `S_AL_Respatialize` from the Pmove-predicted
   eye position every frame and is always current.

2. **`S_AL_UpdateEntityPosition`**: when called for the listener entity,
   `effectiveOrigin` is set to `s_al_listener_origin` before both the entity-
   origin cache write (`s_al_entity_origins[entityNum]`) and the active-source
   position update loop.  This prevents a late-frame `UpdateEntityPosition` call
   from overwriting the cache value written earlier by `Respatialize`.

This fix is config-independent: it applies equally on vanilla servers (no
`sv_bufferMs`), custom servers with `sv_smoothClients`, and any QVM patch
level, because `s_al_listener_origin` is derived from client-side prediction,
not from the network.

#### Dynamic acoustic environment (`s_alDynamicReverb 1`)

##### Design rationale — why URT needed more than 4 EFX parameters

Urban Terror maps cover an unusually wide range of acoustic environments:
rooftops with no ceiling, cave-like underground passages, short hallways with
multiple doorways into wider spaces, narrow alleys, large atria, and dense urban
courtyards.  The original dynamic reverb drove only four parameters (decay, late
gain, reflections, slot gain), which produced a recognisable but **flat** acoustic
texture because:

- Every environment shared the same HF ratio, density, and diffusion — the reverb
  tail always had the same spectral colour and echo density regardless of whether
  the player was in a stone tunnel or on a tiled rooftop.
- `corrFactor > 0.40` was the only trigger for corridor-mode reflections, so
  short hallways with doorways on both sides — where no single axis pair is
  dominant — fell through to generic MEDIUM_ROOM behaviour.
- Open-top walled spaces ("open box") — where `vertOpenFrac` is high but
  `horizOpenFrac` is low — were misclassified as TRANSITION or SEMI_OPEN because
  no metric distinguished sky-open from wall-open.
- LARGE_ROOM, MEDIUM_ROOM, and SMALL_ROOM were the only purely indoor labels; there
  was no concept of a HALLWAY, LARGE_HALL, CAVE, or OPEN_BOX.

The new classifier addresses each issue specifically:

| Problem | Solution |
|---|---|
| Short hallways with doorways fall to MEDIUM_ROOM | New `HALLWAY` label: corrFactor > 0.20 & openFrac < 0.45 — lower threshold catches multi-opening passages |
| Open-box rooftops misclassified | New `openBoxFrac` metric: vertOpenFrac − horizOpenFrac excess; new `OPEN_BOX` label |
| Caves/tunnels identical to large halls | New `caveBonus` metric: sizeFactor × solid-overhead fraction; new `CAVE` label |
| Flat spectral quality everywhere | 5 new EFX params driven by probe metrics: HF ratio, density, diffusion, gainHF, gainLF |
| Fire-impact inconsistent across env types | Boost now written to `cur*` (output) not `target*` (pre-blend dead-write) |
| Fire-impact absent in static reverb mode | New `S_AL_UpdateStaticFireBoost` function — runs when `s_alDynamicReverb 0` |
| HRTF gap hint < 5° due to short probe | Gap position now projected to source distance before blending |

Casts **14 world-space rays** from the listener every **16 frames** plus up to **2 look-ahead rays** when the player is moving (≈ 1.0 traces/frame average, zero when the movement cache is active):
- 8 horizontal (every 45°) — max `s_alEnvHorizDist` u (default 1330)
- 4 diagonal-up (45° elevation, N/E/S/W) — max `s_alEnvVertDist` u (default 400)
- 1 straight up — max `s_alEnvVertDist` u (default 400)
- 1 straight down — max `s_alEnvDownDist` u (default 160)
- 0–2 look-ahead (direction of travel) — max `s_alEnvHorizDist` u, only when `moveDist > s_alEnvVelThresh`

**openFrac** is a weighted average: `s_alEnvHorizWeight` × horizontal + (1 − `s_alEnvHorizWeight`) × vertical (defaults 70 / 30 %).  When the player is moving faster than `s_alEnvVelThresh` units per probe cycle, up to `s_alEnvVelWeight` (default 30 %) of openFrac is blended from 2 look-ahead rays in the direction of travel — this prevents the classifier from stalling in semi-open zones when walking from indoors to outdoors.

**vertOpenFrac** is now history-averaged alongside the other metrics, enabling two additional derived signals:

- **caveBonus** (`sizeFactor × clamp(0.25 − vertOpenFrac, 0, 0.25) / 0.25`): fires when the listener is in a large horizontal space with solid overhead (cave, mine tunnel, underground passage). Adds up to 1.0 s extra decay, boosts HF ratio (stone surfaces), echo density, and late reverb.
- **openBoxFrac** (`clamp((vertOpenFrac − horizOpenFrac − 0.20) / 0.60, 0, 1)` when `horizOpenFrac < 0.50`): fires when sky is clearly visible above but walls surround the listener — URT "open box" designs (rooftops, walled courtyards, arenas with no ceiling). Trims decay (no ceiling to trap reverb), adds early reflections off the surrounding walls, and slightly reduces bass in the reverb tail.

A **rolling history buffer** (`s_alEnvHistorySize` slots, default 3, max 8) averages the last N probe sets for all four metrics (size, open, corr, vert). A **movement cache** reuses metrics when the listener has moved < 32 units since the last probe.

Parameters blend smoothly via IIR pole `s_alEnvSmoothPole` (default 0.92, ≈ 3.5 s half-transition at 60 fps).  When the player is moving, the pole is automatically reduced by up to 0.07 (toward 0.85) so transitions respond faster during environment changes.

#### Environment classification (9 types)

| Label | Condition | Acoustic character |
|---|---|---|
| `OUTDOOR` | openFrac > 0.60 | Very short decay, near-dry, dim reverb tail |
| `OPEN_BOX` | openBoxFrac > 0.35 | Walls around, sky above: short decay, strong early reflections, moderate wet |
| `SEMI_OPEN` | openFrac > 0.42 | Forest edge, large plaza: transitional with some reverb |
| `CAVE` | caveBonus > 0.30 | Long decay, stone HF ratio, dense echo, bassy tail |
| `CORRIDOR` | corrFactor > 0.40 & openFrac < 0.25 | Flutter echo, bright HF, strong reflections, low diffusion |
| `HALLWAY` | corrFactor > 0.20 & openFrac < 0.45 | Multi-doorway passage: corridor + urban early-reflection blend |
| `LARGE_HALL` | sizeFactor > 0.65 | Large enclosed space, long decay, moderate density |
| `MEDIUM_ROOM` | sizeFactor > 0.30 | Standard indoor room |
| `SMALL_ROOM` | fallback | Short decay, modest reverb |

#### EFX parameters dynamically modulated (9 total)

The following EFX parameters are now all driven by the probe metrics every 16 frames, giving each environment a distinct spectral and acoustic signature:

| Parameter | Range driven | Effect |
|---|---|---|
| `DECAY_TIME` | openFrac, sizeFactor, caveBonus, openBoxFrac | Room size / tail length |
| `LATE_REVERB_GAIN` | sizeFactor, openFrac, caveBonus | Reverb tail loudness |
| `REFLECTIONS_GAIN` | corrFactor, openFrac, urban semi-open boost, openBoxFrac | Early reflection strength |
| `EFFECTSLOT_GAIN` | openFrac, openBoxFrac | Overall wet/dry mix |
| `DECAY_HFRATIO` | openFrac, caveBonus, corrFactor | Surface hardness: 0.55 (outdoor/foliage) → 1.25+ (stone/cave) |
| `DENSITY` | openFrac, caveBonus | Echo density: sparse outdoor → dense cave |
| `DIFFUSION` | openFrac, caveBonus, corrFactor | Wall irregularity: corridor flutter vs cave scatter |
| `GAINHF` | openFrac, corrFactor, caveBonus, openBoxFrac | Reverb tail treble EQ: dim outdoor → bright corridor |
| `GAINLF` | openFrac, caveBonus, openBoxFrac | Reverb tail bass EQ: thin outdoor → resonant cave |

#### Fire-impact reverb (consistency fix)

The muzzle-report and incoming-fire reverb boosts (`s_alFireImpactReverb`) are now applied directly to the **smoothed output values** (`cur*`) rather than to the intermediate targets after the IIR blend.  In the old code the fire-boost sections ran after the blend had already computed `curRefl`/`curDecay` from the environment targets, making the boost modifications to those target variables a dead-write — `cur*` was already set and was what actually got pushed to EFX.  The new code writes directly to `cur*` after the blend, so the effect is:
- **Immediate** — takes effect in the same probe cycle it is detected.
- **Classifier-independent** — a weapon fired at a wall produces an audible spike regardless of whether the environment probe reports CORRIDOR, OUTDOOR, or any other type.
- **Includes a slot gain spike** so the muzzle report is audible even in dry outdoor or semi-open environments where the base slot gain is low.
- When `s_alDynamicReverb 0` (static reverb mode), a dedicated `S_AL_UpdateStaticFireBoost` function provides the same muzzle-report and incoming-fire enhancement directly against the static EFX baseline.

#### Live tuning with `/s_alReset`

All `s_alEnv*` and `s_alOccWeaponDist` cvars can be changed at the console and take effect immediately after typing `/s_alReset` — no map reload or `snd_restart` needed.  `s_alReset` invalidates the history buffer, movement cache, and velocity state so the next probe cycle snaps to the current environment with the new parameters.

```
// Example tuning session:
s_alEnvSmoothPole 0.88   // faster transitions
s_alEnvHistorySize 2     // less averaging (more responsive, more flicker)
s_alEnvVelWeight 0.40    // stronger look-ahead bias when running
s_alReset                // snap to new params now
s_alDebugReverb 2        // watch the output
```

#### New tuning CVars (all `CVAR_ARCHIVE_ND` — persist between sessions)

| CVar | Default | Range | Purpose |
|---|---|---|---|
| `s_alEnvHorizDist` | 1330 | 200–4096 | Horizontal probe ray max distance (u) |
| `s_alEnvVertDist` | 400 | 50–2000 | Diagonal-up / straight-up ray max distance (u) |
| `s_alEnvDownDist` | 160 | 50–1000 | Straight-down ray max distance (u) |
| `s_alEnvHorizWeight` | 0.70 | 0–1 | Fraction of openFrac from horizontal rays |
| `s_alEnvSmoothPole` | 0.92 | 0.5–0.99 | IIR blend pole; higher = slower transitions |
| `s_alEnvHistorySize` | 3 | 1–8 | Rolling history window size |
| `s_alEnvVelThresh` | 48 | 8–400 | Speed (u/probe-cycle) to activate look-ahead |
| `s_alEnvVelWeight` | 0.30 | 0–0.5 | Max look-ahead blend weight |
| `s_alOccWeaponDist` | 160 | 80–400 | CHAN_WEAPON no-occlusion radius (u) |

#### HRTF directional accuracy (`s_alOcclusion 1`)

When a source is occluded, the engine searches for the nearest acoustic gap using 8 tangent-plane probe rays displaced `s_alOccSearchRadius` units from the source.  The found gap position is **projected onto the source's actual distance** from the listener before blending, so `s_alOccPosBlend` now directly controls an angular direction shift rather than a small positional nudge:

| `s_alOccPosBlend` | Effect |
|---|---|
| 0.0 | Always use true source position — no gap hint |
| 0.40 (default) | ~16° apparent shift for a doorway 45° off the direct line |
| 1.0 | Full redirect to gap direction — maximum corner-peek effect |

| CVar | Default | Range | Purpose |
|---|---|---|---|
| `s_alOccPosBlend` | 0.40 | 0–1 | Fraction of HRTF redirect toward nearest acoustic gap |
| `s_alOccSearchRadius` | 120 | 20–400 | Probe radius (u) for gap search — increase for wide doorways/pillars |

#### Debug workflow (`s_alDebugReverb`)

```
s_alReverb 1 ; s_alDynamicReverb 1 ; s_alDebugReverb 1
```
Prints one line per probe cycle (every 16 frames):
```
[dynreverb] env=OPEN_BOX    size=0.38  open=0.33  corr=0.08  |  tgt: dec=0.82 lat=0.14 ref=0.22 slot=0.28  |  cur: dec=0.79 lat=0.13 ref=0.21 slot=0.27
```

```
s_alDebugReverb 2
```
Also prints raw pre-history probe values including the new `cave=` and `box=` metrics:
```
             raw: size=0.40  horizOpen=0.14  vertOpen=0.88  corr=0.09  cave=0.00  box=0.71  |  hist=3/3  vel=58  cache=miss
```
The `vel=` field shows movement units per probe cycle — use it to verify `s_alEnvVelThresh` is appropriate for your frame rate.

### snd_openal.h — Public API

```c
#ifdef USE_OPENAL
qboolean S_AL_Init(soundInterface_t *si);  // init OpenAL + populate si table
#endif
```

### snd_main.c Changes [URT]

```c
// snd_main.c: S_Init — try OpenAL first, fall back to base
#ifdef USE_OPENAL
if (!started)
    started = S_AL_Init(&si);
#endif
if (!started)
    started = S_Base_Init(&si);
```

### Build System

```makefile
USE_OPENAL = 1          # default: enabled
# USE_OPENAL = 0        # disable: use dmaHD / base DMA only
```

```makefile
ifeq ($(USE_OPENAL),1)
  BASE_CFLAGS += -DUSE_OPENAL
endif
# snd_openal.o added to Q3OBJ when USE_OPENAL=1
```

No link-time library flag needed — OpenAL is loaded dynamically at runtime.

---

**File:** `code/client/snd_main.c`

Dispatches all sound calls to whichever backend is loaded (`si` — the active `soundInterface_t`).

### S_Init()

```
S_Init:
  if s_initsound == 0: disable sound
  if s_useHD or s_doppler: use HD backend (S_Base_HD_Init)
  else: use base backend (S_Base_Init)
  S_BeginRegistration()
```

### soundInterface_t si

The active backend's function table:
```c
typedef struct {
    void (*Shutdown)(void);
    void (*StartSound)(vec3_t origin, int entityNum, int entchannel, sfxHandle_t sfx);
    void (*StartLocalSound)(sfxHandle_t sfx, int channelNum);
    void (*StartBackgroundTrack)(const char *intro, const char *loop);
    void (*StopBackgroundTrack)(void);
    void (*RawSamples)(int samples, int rate, int width, int channels, const byte *data, float vol);
    void (*StopAllSounds)(void);
    void (*ClearLoopingSounds)(qboolean killall);
    void (*AddLoopingSound)(int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx);
    void (*AddRealLoopingSound)(...);
    void (*StopLoopingSound)(int entityNum);
    void (*Respatialize)(int entityNum, const vec3_t origin, vec3_t axis[3], int inwater);
    void (*UpdateEntityPosition)(int entityNum, const vec3_t origin);
    void (*Update)(int msec);
    void (*DisableSounds)(void);
    void (*BeginRegistration)(void);
    sfxHandle_t (*RegisterSound)(const char *sample, qboolean compressed);
    void (*ClearSoundBuffer)(void);
    void (*SoundInfo)(void);
    void (*SoundList)(void);
} soundInterface_t;
```

### Key Dispatcher Functions

| Function | What It Does |
|---|---|
| `S_StartSound(origin, ent, ch, sfx)` | Play positional sound |
| `S_StartLocalSound(sfx, ch)` | Play non-positional (UI/weapon) sound |
| `S_StartBackgroundTrack(intro, loop)` | Start music stream |
| `S_StopAllSounds()` | Stop everything immediately |
| `S_Update(msec)` | Per-frame: mix + push to device |
| `S_RegisterSound(name, compressed)` | Load or find cached sfx |
| `S_Respatialize(ent, origin, axis, inwater)` | Update listener position/orientation |
| `S_AddLoopingSound(ent, origin, vel, sfx)` | Register looping ambient sound |
| `S_ClearLoopingSounds(killall)` | Clear looping sounds (each frame) |

---

## snd_dma.c — Base DMA Backend

**File:** `code/client/snd_dma.c`  
**Size:** ~1605 lines

### DMA Model

The OS provides a circular DMA buffer (`dma.buffer`). The sound system fills it by calling `S_PaintChannels` which mixes all active channels into `paintbuffer`, then `S_TransferPaintBuffer` converts to the device's sample format and copies to `dma.buffer`.

### Channel Management

`s_channels[MAX_CHANNELS]` — up to 96 active sound channels:
```c
typedef struct {
    sfx_t       *thesfx;        // loaded sound
    vec3_t       origin;
    int          entnum;
    int          entchannel;    // CHAN_AUTO, CHAN_BODY, CHAN_VOICE, etc.
    vec3_t       origin_velocity;
    int          master_vol;    // 0-255
    int          leftvol;       // spatialized per-sample
    int          rightvol;
    int          startSample;   // sample offset to start at
    int          loopSound;     // is this a loop?
    int          loopnum;
    qboolean     doppler;
    float        dopplerScale;
    qboolean     fullVolume;    // is this a local sound?
} channel_t;
```

#### Channel budget policy

`S_Base_StartSound` enforces three independent guards before allocating a channel:

| Entity | Per-sound dedup window | Per-sound concurrent limit (`allowed`) | Rationale |
|--------|------------------------|----------------------------------------|-----------|
| `listener_number` (local player) | 20 ms | 16 | Many distinct weapon/movement sounds |
| `ENTITYNUM_WORLD` (1022) | **100 ms** | **2** | Impact/surface/casing sounds — see rate-flood analysis below |
| Any other entity | 20 ms | 8 | Default |

Additionally, `ENTITYNUM_WORLD` is subject to a **total channel cap** of `WORLD_ENTITY_MAX_CHANNELS` (`MAX_CHANNELS / 8` = 12 out of 96).

#### World-entity rate-flood analysis

In multiplayer, **all** bullet impacts, ricochets, and brass casings share `ENTITYNUM_WORLD` (ent 1022). The server generates these sound events proportionally to `sv_fps`:

| sv_fps | ms/frame | ENTITYNUM_WORLD events/sec (28 players, auto-fire) | Issue frequency (before fix) |
|--------|----------|------------------------------------------------------|------------------------------|
| 20 | 50 ms | ~baseline | every ~1 min |
| 60 | 16.7 ms | ~3× baseline | every few seconds |

Without targeted throttling the world-entity pool saturates at a rate proportional to `sv_fps × player_count × sounds_per_action`.

Three complementary guards address this:

1. **100 ms per-sound dedup window** — the same sfx (e.g. `concrete1.wav`) cannot re-trigger on `ENTITYNUM_WORLD` within 100 ms. At sv_fps 60 this blocks re-triggers for 6 consecutive frames; at sv_fps 20 it blocks every other frame. Automatic weapons fire at ~10 rounds/sec (100 ms/round), so every impact is still audible.

2. **Per-sound concurrent limit of 2** — at most 2 "old" (> 100 ms) instances of the same sfx may be playing before new ones are rejected.

3. **Total cap of 12 channels** — the hard ceiling prevents any single server-fps spike from exhausting the 96-channel pool; leaves ≥ 77 channels for player, weapon, and ambient sounds.

If any guard triggers, the sound is silently dropped (logged via `Com_DPrintf`).

### S_Base_StartSound(origin, entityNum, entchannel, sfxHandle)

1. Load sound if not in memory: `S_memoryLoad(sfx)`
2. Find a free channel (or steal least important)
3. Set `ch->master_vol = s_volume * 255`
4. `S_SpatializeOrigin` — compute left/right volumes based on distance and listener orientation
5. Set `ch->startSample`

### S_SpatializeOrigin(origin, master_vol, left_vol, right_vol)

Converts 3D position to stereo panning:
1. `VectorSubtract(origin, listener_origin, dir)` — direction vector
2. Compute distance; attenuate by `1.0 - dist/1024`
3. Project direction onto listener right axis → left/right balance
4. Clamp to [0, 255]

### S_Base_Update(msec)

```
S_Base_Update:
  S_AddLoopSounds()         ← activate looping channels for this frame
  S_Spatialize all active channels (positions may have changed)
  S_PaintChannels(endtime)  ← fill paintbuffer with mixed samples
  S_TransferPaintBuffer()   ← write to DMA buffer
  S_UpdateBackgroundTrack() ← decode next chunk of music stream
```

---

## snd_dmahd.c — High-Definition HRTF Backend [URT]

**File:** `code/client/snd_dmahd.c`  
**Size:** ~1467 lines  
**Guard:** `#ifndef NO_DMAHD` (enabled by default; build with `NO_DMAHD=1` to disable)  
**New file** added in this branch.

Full spatial/HRTF sound system for UrbanTerror. Features:
- HRTF (Head-Related Transfer Function) positional audio
- Higher quality mixing (float paintbuffer instead of int)
- Doppler effect simulation
- Water underwater effect (low-pass filter)
- Separate volume controls for effects vs music
- Per-channel side parameters (bass, reverb, offset)

### Integration Architecture

Unlike the master branch description, dmaHD does **not** replace `snd_dma.c` — it is initialized as an **extension inside `S_Base_Init`**:

```c
// snd_dma.c: S_Base_Init (end)
#ifndef NO_DMAHD
    if ( dmaHD_Enabled() )
        return dmaHD_Init( si );   // HD replaces the si function table
#endif
```

`dmaHD_Init(si)` takes the already-initialized `soundInterface_t` and overrides specific function pointers with HD versions. This allows the base DMA infrastructure (device open, DMA buffer) to remain in `snd_dma.c` while spatial processing is handled by dmaHD.

### snd_dmahd.h — Public API

**New file.** Three public symbols:

```c
sfxHandle_t dmaHD_LoadSound( sfx_t *sfx );  // intercept S_LoadSound
qboolean    dmaHD_Enabled( void );           // query if dmaHD is active
void        dmaHD_Init( soundInterface_t *si ); // init + override si table
```

### snd_local.h Changes [URT]

`snd_local.h` adds dmaHD-specific fields to shared structs:

```c
// ch_side_t — per-channel side parameters
typedef struct {
    int vol;     // volume
    int offset;  // delay offset
    int bass;    // bass level
    int reverb;  // reverb level
} ch_side_t;

// channel_t — union adds ch_side_t alongside leftvol/rightvol
typedef struct channel_s {
    ...
    union { int leftvol;  ch_side_t left;  };
    union { int rightvol; ch_side_t right; };
    vec3_t sodrot;   // spatial orientation
    ...
} channel_t;

// sfx_t — weaponsound flag
typedef struct sfx_s {
    ...
    qboolean weaponsound;  // controls HRTF processing type
} sfx_t;
```

### snd_mem.c Changes [URT]

```c
// S_LoadSound: intercept for dmaHD memory allocation
#ifndef NO_DMAHD
    // Fixed memory allocation: 2*1536 bytes for dmaHD processing buffers
    if ( dmaHD_Enabled() )
        return dmaHD_LoadSound( sfx );
#endif
```

### win_snd.c Changes [URT]

On Windows, when dmaHD is active with WASAPI:
- **`GetMixFormat`** is called to discover the device's own native rate and sample format.  This makes WASAPI operate **bit-perfect** — no internal resampling, no Windows Audio Processing Object (APO) disruption, and no added latency.
- If the native format is 32-bit float (the default on virtually all modern Windows systems), dmaHD uses that directly via the new float write path in `dmaHD_TransferPaintBuffer`.
- If `GetMixFormat` fails the safe fallback is 48 000 Hz / 32-bit float.
- DirectSound (legacy fallback) uses 48 000 Hz / stereo / 16-bit PCM (DirectSound is PCM-only).

```c
// win32/win_snd.c  — SNDDMA_InitWASAPI (dmaHD path)
#ifndef NO_DMAHD
if ( dmaHD_Enabled() )
{
    // Query native mix format: avoids WASAPI resampling and APO disruption.
    if ( iAudioClient->lpVtbl->GetMixFormat( iAudioClient, &mixFormat ) == S_OK && mixFormat )
    {
        dma.speed = (int)mixFormat->nSamplesPerSec;
        if ( /* float32 subformat */ )
            dma.samplebits = 32;   // → dma.isfloat = qtrue after negotiation
        CoTaskMemFree( mixFormat );
    }
    else { dma.speed = 48000; dma.samplebits = 32; } // safe fallback
}
else
#endif
switch ( s_khz->integer ) { ... }
```

`initFormat` was updated to emit `WAVE_FORMAT_IEEE_FLOAT` (instead of `WAVE_FORMAT_PCM`) when `nBits == 32`, so the format request is accepted by WASAPI without negotiation.

`dmaHD_TransferPaintBuffer` was updated with a 32-bit float write path that normalises the 24.8 fixed-point paint accumulator to `[-1.0, 1.0]`, matching the formula used by the base `S_TransferPaintBuffer` in `snd_mix.c`.

**24-bit display fix:** `dma.validbits` (a new field in `dma_t`) is set from `wValidBitsPerSample` when the device reports a WAVEFORMATEXTENSIBLE format whose container width (`wBitsPerSample = 32`) differs from the valid bit depth (e.g. `wValidBitsPerSample = 24` for a 24-bit device). `dmaHD_SoundInfo` (`s_info`) now displays `dma.validbits` when it differs from `dma.samplebits` so that a 48 kHz / 24-bit device correctly shows **24 bps** instead of the container width of 32. The transfer path continues to use `dma.samplebits` (32) to select the correct output branch.

### Build System

`Makefile` additions:
- `NO_DMAHD=0` (default) — dmaHD enabled
- `-DNO_DMAHD` conditional compile flag
- `snd_dmahd.o` included in client build when `NO_DMAHD=0`

### Peak Limiter [URT]

`dmaHD_PaintChannels` now applies a **fast-attack / slow-release peak limiter** to the accumulated paintbuffer before calling `dmaHD_TransferPaintBuffer`. This prevents the mix-overload distortion (`paint buffer clipped N/16384 samples`) that occurs when many channels are simultaneously active during heavy combat.

**Algorithm:**

1. After mixing all channels into `dmaHD_paintbuffer`, scan the buffer for the peak absolute value.
2. If `peak > DMAHD_FLOAT_MAX` (the 16-bit output ceiling), immediately reduce `s_dmaHD_limiterGain` to `DMAHD_FLOAT_MAX / peak` — **fast attack** to prevent any clipping this chunk.
3. Apply `s_dmaHD_limiterGain` to every sample in the buffer (fixed-point: `(sample * scale256) >> 8`).
4. Each chunk, recover 15% of the remaining distance to unity gain — **slow release** (`gain += (1.0 - gain) * 0.15`), giving ~100–200 ms recovery at 60 fps to avoid audible volume pumping.

The limiter only engages when the mix would otherwise clip; at normal load `s_dmaHD_limiterGain == 1.0` and no scaling is applied.

Limiter activity is logged at `dmaHD_debugLevel 1`:
```
dmaHD: peak limiter active — gain 0.714 (peak 11747328, 52 active ch)
```

### HRTF at Increased Sample Rates — Inspection [URT]

dmaHD was audited for correctness at sample rates above the original 44 100 Hz target (in particular 48 000 Hz WASAPI native on Windows). All sample-rate-dependent calculations were verified:

| Component | Formula | Rate-correct? | Notes |
|-----------|---------|---------------|-------|
| ITD delay (`CALCSMPOFF`) | `(dist_units × dma.speed) >> 17` | ✅ | Delay in *samples* scales with `dma.speed`; same physical ms delay at all rates |
| Behind-viewer ITD offset | `t = dma.speed × 0.001f` | ✅ | Produces a constant 1 ms ITD regardless of rate |
| Resampling step | `stepscale = inrate / dma.speed` | ✅ | Output length = `soundLength / stepscale` → correct upsample/downsample |
| HP filter coefficient | `hp_a = 1 − (1 − 0.95) × (44100 / dma.speed)` | ✅ | Maintains ~360 Hz cutoff at all rates |
| LP (bass) filter coefficient | `lp_a = 0.03 × (44100 / dma.speed)` | ✅ | Maintains ~211 Hz cutoff at all rates |
| Sound storage layout | High-freq at `[0..soundLength]`, bass at `[soundLength..2×soundLength]` | ✅ | Both halves use `outcount` samples at `dma.speed` |

The filter coefficient scaling (`44100.0f / dma.speed`) introduced in PR 67 is mathematically correct for first-order IIR filters at the small-coefficient approximation used here. Overflow in `CALCSMPOFF` is not possible at supported sample rates (≤ 48 000 Hz) for any in-map distance.

### Audio Click/Pop — Full Root Cause Analysis & Fix [URT]

**Symptoms (PRs #69, #70, #73, #74, issue continuation):**
A click/pop is audible whenever sounds play during a kill event on Windows with WASAPI. The click is reproducible: every sound that starts (or ends) during the kill window contributes one click. If multiple sounds are active at the same time, multiple clicks are heard. The specific kill-ding sound is 22 050 Hz stereo, ~4 s long: ~1.5 s of audio followed by ~2.5 s of digital silence.

---

**Root cause — two separate click sources, both from `dmaHD_GetSampleRaw` wrapping**

The `dmaHD_GetSampleRaw_*` functions previously used **modulo wrapping** for indices outside `[0, samples)`:

```c
if (index < 0) index += samples;
else if (index >= samples) index -= samples;   // wraps to sample[0] (attack peak)
```

This was correct for seamless-loop playback but wrong for the **resampling** context where it runs. During `dmaHD_ResampleSfx`, the wraparound contaminated the output buffer at **two distinct locations**:

#### Click 1 — Start of buffer (`buffer[0..2]`): click at the kill event

`dmaHD_ResampleSfx` initialises the HP filter by running it on a 4-sample **warm-up** from `idx_smp = −4×stepscale`. `dmaHD_NormalizeSamplePosition` wraps these negative positions to near the **end** of the source sound (e.g. `88198.16` for 22 050 → 48 000 Hz). The Hermite kernel then reads `x+1` and `x+2`, which exceed `soundLength` and — in the old code — **wrapped back to `sample[0]` (the attack peak)**. This incorrectly primed the HP filter with a large negative contamination:

```
OLD buffer[0] = −1046  ← HP filter response to spurious attack-peak injection
OLD buffer[1] =   806
OLD buffer[2] =  8852
OLD buffer[3] = 13915  ← first real audio sample
```

When the mixer reads from `buffer[0]` the moment a sound starts, the paint buffer jumps from 0 (silence) to −1046 — an instantaneous step discontinuity → **click at the exact moment the kill event fires**, for every sound that starts. With multiple sounds starting simultaneously (kill ding, hit sound, etc.), each contributes one click.

> PR #74's fade-out only touched the **end** of the buffer (`buffer[outcount−n_pad..outcount−1]`). `buffer[0]` was never modified — **this click was not fixed by PR #74**.

#### Click 2 — End of buffer (`buffer[outcount−1]`): click at sound end

At the **last** iteration of the resampling loop, `idx_smp ≈ soundLength − 4×stepscale`, giving `x = soundLength−2` and `x+2 = soundLength` (equals `samples`), which the old code wrapped to 0 (attack peak). The HP filter output at `buffer[outcount−1]` therefore contained a small spike (−156 in the simulated case) rather than true silence, producing a step from near-silence to 0 when the channel ended.

PR #74's adaptive end-fade zeroed `buffer[outcount−1]` and addressed this click. The fade remains in place as a belt-and-suspenders guard for any HP/LP filter residual energy at the buffer boundary.

---

**Primary fix — `dmaHD_GetSampleRaw_*`: return 0 for out-of-bounds indices**

Changed from:
```c
if (index < 0) index += samples; else if (index >= samples) index -= samples;
```
to:
```c
if (index < 0) index += samples;
if (index < 0 || index >= samples) return 0;   // out-of-bounds → silence
```

- The **negative wrap** (`index += samples`) is preserved: it is used by the warm-up pre-loop, which reads from near the end of the source sound. For silence-padded sounds this correctly primes the filter with zeros.
- Indices that reach or exceed `samples` — from Hermite/cubic `x+1`, `x+2` lookahead or the no-interpolation round-up — now return **silence** (0) instead of the attack peak.

**Verified result (22 050 Hz → 48 000 Hz, ATTACK_PEAK = 15 000):**

```
NEW buffer[0] =     0  ← clean (no contamination)
NEW buffer[1] =     0
NEW buffer[2] =     0
NEW buffer[3] = 14310  ← first real audio sample, clean onset
NEW buffer[outcount−1] = 0  ← also clean (fix + fade both confirm)
```

This eliminates both click sources for all sample rates and all `dmaHD_interpolation` modes.

---

**Secondary fix — adaptive end-fade (`dmaHD_ResampleSfx`)** *(PR #74, retained)*

After the resampling loop, a linear fade-out is still applied to the last `n_pad` output samples of both sub-buffers. With the primary fix in place this is now a belt-and-suspenders guard against any residual HP/LP filter tail rather than the primary click fix. `n_pad` is computed adaptively from `stepscale` to handle any WASAPI output rate:

**Secondary fix — `dmaHD_lastsoundtime` reset guard (`dmaHD_Update_Mix`):**

The previous `static int lastsoundtime = -1;` was function-local, so it was never reset when `s_soundtime` was reinitialised to 0 by `S_Base_Init` / `S_Base_StopAllSounds` (e.g., `vid_restart`, level change, disconnect/reconnect). After such a reset, `s_soundtime (0) <= lastsoundtime (large)` caused `dmaHD_Update_Mix` to silently skip **all** painting until `s_soundtime` climbed back up to the old value — potentially several seconds of complete silence.

The variable was promoted to module scope as `dmaHD_lastsoundtime`. A guard resets it (and the peak-limiter gain) if `s_soundtime` drops more than one second below the last seen value:

```c
if ((dmaHD_lastsoundtime - s_soundtime) > (int)dma.speed) {
    dmaHD_lastsoundtime = s_soundtime - 1;
    s_dmaHD_limiterGain = 1.0f;
}
```

---

## snd_mem.c — Sound Data Loading

**File:** `code/client/snd_mem.c`

### S_LoadSound(sfx)

Loads a sound from disk:
1. Try `FS_ReadFile(sfx->soundName)`
2. Detect format (WAV, ADPCM, wavelet)
3. Decode to PCM via codec
4. Store in `sndBuffer` linked list

### Sound Compression

| Method | Flag | Notes |
|---|---|---|
| PCM 16-bit | 0 | Raw signed 16-bit samples |
| ADPCM | `SOUND_COMPRESSED_ADPCM` | 4:1 compression ratio, small step tables |
| Wavelet | `SOUND_COMPRESSED` | Custom wavelet codec (Quake 3-specific) |
| Mulaw | `SOUND_COMPRESSED_MULAW` | µ-law encoding |

`s_compression` cvar controls which codec is used for new loads.

### SndBuffer Memory Management

Sounds are stored in a pool: `s_soundPool[MAX_SFX]`. Least-recently-used sounds are evicted by `S_FreeOldestSound()` when pool is full. Time tracked by `sfx->lastTimeUsed = Sys_Milliseconds()`.

---

## snd_mix.c — Mixing Engine

**File:** `code/client/snd_mix.c`

### S_PaintChannels(endtime)

Core inner loop. For each active channel:
- Call appropriate paint function based on codec:
  - `S_PaintChannelFrom16` — PCM 16-bit (uses SIMD if available: MMX or SSE)
  - `S_PaintChannelFromADPCM` — decode ADPCM on-the-fly
  - `S_PaintChannelFromWavelet` — decode wavelet on-the-fly
  - `S_PaintChannelFromMuLaw` — decode µ-law on-the-fly

### S_PaintChannelFrom16 (with SIMD)

The hot path for most sounds. Mixes N samples into `paintbuffer[i].left/right` with volume scaling. Uses:
- Scalar fallback: always available
- `S_WriteLinearBlastStereo16_MMX` — MMX version
- `S_WriteLinearBlastStereo16_SSE` — SSE2 version (faster on modern CPUs)

### S_TransferStereo16 / S_TransferPaintBuffer

Converts `portable_samplepair_t paintbuffer[]` (32-bit int accumulator) to device format:
- Clip to ±32767
- Write 16-bit little-endian stereo to `dma.buffer`

---

## Codecs

### snd_codec.c — Codec Dispatcher

Dispatches to the appropriate codec based on file extension:
```
RegisterSound("fire/smg1_impact3.wav") → snd_codec_wav.c
RegisterSound("music/intro.ogg")       → (external: OpenAL or not supported in base Q3)
```

### snd_codec_wav.c

Standard PCM WAV loader. Reads RIFF/WAVE header, validates format, decompresses if needed.

### snd_adpcm.c

ADPCM (Adaptive Differential PCM) codec:
- 4 bits per sample (4:1 compression)
- Adaptive step size — larger steps for fast signals, smaller for quiet
- `DecodeADPCM(in, out, len, state)` / `EncodeADPCM(in, out, len, state)`

### snd_wavelet.c

Quake 3 proprietary wavelet codec:
- Used for highly compressed ambient sounds
- Better quality than ADPCM at same ratio for some sound types
- `decodeWavelet(sfx, chunk)` / `encodeWavelet(sfx, samples)`

---

## Key Data Structures

### dma_t — Device Audio Format

```c
typedef struct {
    int     channels;    // 1 (mono) or 2 (stereo)
    int     samples;     // total samples in ring buffer
    int     submission_chunk; // mix in units of this size
    int     samplebits;  // 8 or 16
    int     isfloat;     // 1 if float samples
    int     speed;       // Hz: 11025, 22050, 44100, 48000
    byte    *buffer;     // DMA ring buffer
} dma_t;
```

### sfx_t — Loaded Sound Effect

```c
typedef struct sfx_s {
    sndBuffer   *soundData;      // linked list of sample chunks
    qboolean     defaultSound;   // true if load failed (buzz sound)
    qboolean     inMemory;
    int          soundCompressionMethod;
    int          soundLength;    // total samples
    int          soundChannels;  // 1 or 2
    char         soundName[MAX_QPATH];
    int          lastTimeUsed;   // for LRU eviction
} sfx_t;
```

### channel_t — Active Sound Instance

```c
typedef struct {
    sfx_t   *thesfx;
    vec3_t   origin;
    int      entnum;
    int      entchannel;   // CHAN_AUTO, CHAN_BODY, CHAN_VOICE, CHAN_WEAPON, CHAN_ITEM, CHAN_MUSIC
    int      master_vol;
    int      leftvol;     // computed by S_SpatializeOrigin
    int      rightvol;
    int      startSample;
    qboolean loopSound;
    qboolean fullVolume;  // local (non-spatialized) sound
} channel_t;
```

---

## Key Sound Cvars

| Cvar | Default | Notes |
|---|---|---|
| `s_initsound` | 1 | Enable sound system |
| `s_volume` | 0.8 | Master volume |
| `s_musicvolume` | 0.25 | Music volume |
| `s_doppler` | 1 | Enable Doppler effect (all backends) |
| `s_khz` | 44 | Playback frequency (DMA backends only; 11/22/44/48 kHz) |
| `s_mixahead` | 0.2 | How far ahead to mix (DMA backends only) |
| `s_mixPreStep` | 0.05 | Mix step size (DMA backends only) |
| `s_compression` | 0 | Sound compression: 0=none, 1=wavelet, 2=ADPCM |
| `s_useHD` | 1 | Use HD backend (dmaHD; no effect when OpenAL is active) |

### OpenAL Soft Backend Cvars [URT]

When the OpenAL backend is active (`USE_OPENAL=1` and `libopenal.so.1` present):

| Cvar | Default | Notes |
|---|---|---|
| `s_alDevice` | `""` | OpenAL output device name; empty = OS default |
| `s_alHRTF` | `0` | Enable three-layer HRTF + higher-order Ambisonic rendering. Off by default (headphones only). (LATCH) |
| `s_alEFX` | `1` | Enable ALC_EXT_EFX. Set to 0 for diagnostic isolation. (LATCH) |
| `s_alDirectChannels` | `1` | Bypass HRTF centre-HRIR for own-player sources. Only active when `s_alHRTF 1`. (LATCH) |
| `s_alLocalSelf` | `0` | Force own-entity sounds non-spatialized (position 0,0,0). Default 0 — own-player sounds use their world-space origin. Set to 1 if stale-position artefacts occur. |
| `s_alOccPosBlend` | `0.40` | Gap-hint blend weight for HRTF direction correction when a source is occluded [0–1]. Default 0.40; see [HRTF directional accuracy](#hrtf-directional-accuracy-s_alocclusion-1). |
| `s_alOccSearchRadius` | `120` | Tangent-plane probe radius (units) for gap search. Default 120 ≈ URT door half-width. |
| `s_alMaxDist` | `1330` | Override max attenuation distance (floor: 1330 Q3 units) |
| `s_alMaxSrc` | `512` | Maximum OpenAL source pool size. Grows dynamically on demand up to this cap. (LATCH) |
| `s_alDedupMs` | `0` | Same-frame dedup window in ms (0 = disabled). Light guard against double-start artefacts. |

#### Volume mixer — per-category controls

| Cvar | Default | Notes |
|---|---|---|
| `s_alVolSelf` | `1.0` | Own player movement, breath, general entity sounds [0–10] |
| `s_alVolWeapon` | `1.0` | Own weapon fire (CHAN_WEAPON from listener entity) [0–10] |
| `s_alVolOther` | `1.0` | Other players/entities. Anti-cheat cap 2×. [0–2] |
| `s_alVolImpact` | `1.0` | World entity impacts (brass, explosions). Anti-cheat cap 2×. [0–2] |
| `s_alVolEnv` | `1.0` | Looping ambient/environmental sounds [0–10] |
| `s_alVolUI` | `1.0` | Hit-markers, kill-confirmations, menu sounds [0–10] |
| `s_alVolSuppressedWeapon` | `1.0` | Own suppressed-weapon fire volume [0–10] |
| `s_alVolEnemySuppressedWeapon` | `1.0` | Enemy suppressed-weapon fire volume. Anti-cheat cap 2×. [0–2] |

> **See also:** `s_alExtraVolEntry1`–`8` and `s_alExtraVol` in the [Per-Category Volume Mixer](#volume-mixer--per-category-controls-0-10-scale) section above.

#### Suppression & combat-feedback cvars

| Cvar | Default | Notes |
|---|---|---|
| `s_alSuppression` | `0` | Near-miss and incoming-fire hearing disruption toggle |
| `s_alSuppressionRadius` | `180` | Enemy weapon fire radius that triggers suppression fallback (units) |
| `s_alSuppressionFloor` | `0.45` | Min listener gain during suppression [0–0.95] |
| `s_alSuppressionMs` | `220` | Duration of near-miss / incoming-fire disruption (ms) |
| `s_alSuppressionHFFloor` | `0.08` | Min AL_LOWPASS_GAINHF at peak suppression (directional cone only) [0–1] |
| `s_alSuppressionConeAngle` | `120` | Full cone angle (deg) of directional HF suppression around fire direction |
| `s_alSuppressionRearGain` | `0.35` | Fraction of HF suppression applied behind listener (opposite fire) [0–1] |
| `s_alSuppressionReverbBoost` | `0.18` | Reverb slot gain spike on suppression trigger [0–0.5] |
| `s_alNearMissPattern` | `whiz1,whiz2` | Sound-name substrings that trigger a near-miss suppression event |
| `s_alHelmetHitPattern` | `helmethit` | CHAN_BODY sound name triggering helmet-hit disruption + tinnitus |
| `s_alHelmetHitMs` | `350` | Helmet-hit disruption duration (ms) |
| `s_alHelmetHFFloor` | `0.10` | Min HF gain for helmet-hit disruption [0–1] |
| `s_alBareHeadHitPattern` | `headshot` | Sound name triggering bare-head disruption (strongest tier) |
| `s_alBareHeadHitMs` | `500` | Bare-head disruption duration (ms) |
| `s_alBareHeadHFFloor` | `0.03` | Min HF gain for bare-head disruption [0–1] |
| `s_alTinnitusFreq` | `3500` | Tinnitus tone frequency in Hz |
| `s_alTinnitusDuration` | `700` | Ring duration in ms |
| `s_alTinnitusVol` | `0.45` | Tinnitus playback volume [0–1] |
| `s_alTinnitusCooldown` | `800` | Min gap between successive tinnitus plays (ms) |
| `s_alHealthFade` | `0` | Enable health-based HF fade (audio degrades as HP drops). Default off. |
| `s_alHealthFadeThreshold` | `10` | HP below which health-fade activates |
| `s_alHealthFadeFloor` | `0.35` | Min HF gain at 1 HP [0–1] |
| `s_alGrenadeBloom` | `1` | Grenade-blast concussion effect (reverb spike + HF muffling). Default on. |
| `s_alGrenadeBloomRadius` | `400` | Blast radius that triggers the bloom effect (units) |
| `s_alHeadHit` | `1` | Standalone helmet/bare-head hit disruption + tinnitus. Default on. |
| `s_alSuppressedSoundPattern` | `silenced,-sil,_sil,...` | Filename substrings identifying suppressed weapons |
| `s_alFireImpactReverb` | `1` | Muzzle-report reverb boost on own/enemy weapon fire. Applied to smoothed output — classifier-independent. Also active in static reverb mode (`s_alDynamicReverb 0`). |
| `s_alFireImpactMaxBoost` | `0.25` | Max reflections gain boost from fire-impact [0–0.5]. Incoming-enemy boost capped at half. |
| `s_alEnvHorizDist` | `1330` | Horizontal probe ray max distance (u). |
| `s_alEnvVertDist` | `400` | Diagonal-up / straight-up probe ray max distance (u). |
| `s_alEnvDownDist` | `160` | Straight-down probe ray max distance (u). |
| `s_alEnvHorizWeight` | `0.70` | Fraction of openFrac from horizontal rays (vs. vertical). |
| `s_alEnvSmoothPole` | `0.92` | IIR blend pole for EFX param transitions; lower = snappier. |
| `s_alEnvHistorySize` | `3` | Rolling history window size [1–8]; higher = less flicker but slower response. |
| `s_alEnvVelThresh` | `48` | Movement threshold (u/probe-cycle) to activate look-ahead blending. |
| `s_alEnvVelWeight` | `0.30` | Max look-ahead blend weight when moving fast [0–0.5]. |

> **Full suppression documentation:** See the [Hearing Disruption](#hearing-disruption--incoming-fire-head-hits-and-health-fade) section above.

#### Debug / diagnostic cvars

| Cvar | Default | Notes |
|---|---|---|
| `s_alDebugPlayback` | `0` | Log preemptions and rate-mismatches (`1`) or also natural completions (`2`). Not archived. |
| `s_alDebugOcc` | `0` | Print per-source occlusion state each frame. Not archived. |
| `s_alDebugReverb` | `0` | Log dynamic reverb probe results (`1`) or raw data (`2`). Not archived. |
| `s_alTrace` | `0` | Key source lifecycle events (`1`) or verbose (`2`). Not archived. |
| `s_alDebugNorm` | `0` | Print `normGain` for every active looping ambient source once per frame. Use on a specific map (e.g. `ut4_turnpike`) to identify which sounds are too loud and verify that the normcache is taking effect. Not archived. |

- OpenAL Soft selects the native device rate automatically (no `s_khz` needed).
- `s_doppler` controls `AL_DOPPLER_FACTOR` (1.0 = enabled, 0.0 = disabled).
- HRTF uses OpenAL Soft's built-in CIPIC / MIT KEMAR datasets — no custom DSP code.
- EFX (`ALC_EXT_EFX`) is detected and enabled when `s_alEFX 1`; set to 0 to bypass entirely.
- Occlusion traced via `CM_BoxTrace` at distance-adaptive intervals (every frame < 300 u, every 4 frames 300–600 u, every 8 frames > 600 u) per source; applied as EFX low-pass filter if available, else gain scale.  Gap direction projected to source distance (`s_alOccSearchRadius 120` u probe, `s_alOccPosBlend 0.40` angular blend).
- Ambisonic order for HRTF rendering auto-detected from `ALC_MAX_AMBISONIC_ORDER_SOFT` (capped at 3rd order).

Build flags:
- `USE_OPENAL=0` — disable OpenAL, force dmaHD / base DMA.
- `NO_DMAHD=1` — disable dmaHD; only relevant when OpenAL is also disabled.

### dmaHD-Specific Behavior [URT]

When dmaHD is active and OpenAL is unavailable (`USE_OPENAL=0` or OpenAL library not found):
- `s_khz` defaults to **48** (used as fallback; on Windows WASAPI the native device rate is used regardless)
- `s_doppler` enables HRTF Doppler simulation
- On Windows, WASAPI queries `GetMixFormat` and matches the native device rate + format exactly — **no resampling, no APO disruption** (typically 48 000 Hz / 32-bit float on modern hardware)
- DirectSound fallback: 48 000 Hz / stereo / 16-bit PCM
- Sound loading goes through `dmaHD_LoadSound` for HRTF pre-processing

Build flag: compile with `NO_DMAHD=1` to disable dmaHD and use the standard DMA backend.

---

## Global Audio Preemption — Design and Implementation

### How DMA handles it ("vanilla")

`snd_dma.c` uses no per-sound guards at all.  Every new `S_StartSound` call
allocates from a free-list; when the pool is full it steals the **oldest**
channel — same entity first, then any entity, then the listener as last resort.
`CHAN_ANNOUNCER` is the only channel with steal-protection.  CHAN_WEAPON has
no special treatment whatsoever.

Three mechanisms keep DMA from drowning in its own events:

| Mechanism | Value | Purpose |
|-----------|-------|---------|
| Per-entity dedup window | 20 ms (100 ms for world) | Same sfx from same entity can't fire again within the window — prevents double-start in a single frame |
| Per-entity concurrency cap | 16 (listener), 8 (other), 2 (world) | Limits how many concurrent copies of the same sound any one entity can hold |
| `ENTITYNUM_WORLD` hard cap | `MAX_CHANNELS / 8` | Brass, impacts, and ricochets all share the world entity — without this cap they exhaust the pool under heavy fire |

That's it.  No content awareness, no looping, no fade.  It works because
weapon sounds are **different files** (`de_out.wav`, `back.wav`, `cock.wav`),
so each can play concurrently rather than fighting for the same slot.  Vanilla
URT has double-start issues all the time and nobody notices.

### Why the previous OpenAL implementation needed guards

OpenAL originally mapped each `(entnum, entchannel)` pair to a **single source**.
When a new sound arrived on an occupied slot the existing source was hard-stopped.
Under URT's weapon-animation event burst (all three DE-50 sounds arrive within
~30 ms) every sound immediately evicted its predecessor, making the fire boom
completely inaudible.

The complex preemption guard (content-samples scan, weapon-loop looping model,
fade-in ramp) was built to paper over this constraint.  It introduced its own
bugs: loop restarts mid-fire-sequence, cycle sounds blocked by long guards.

### Current implementation — slot-based model

**Core principle**: source slots are cheap on modern hardware.  A 32–38 player
URT server needs at most ~250 simultaneous sources (38 players × ~5–6 sounds
each + world impacts).  With OpenAL Soft's default 256 mix voices, and a 512-
slot upper bound, there is no reason to refuse any sound request.

**`S_AL_StartSound`** applies three checks, in order:

1. **Distance rejection**: if the sound origin is beyond `s_alMaxDist` from the
   listener (default 1330 u — already inaudible in the distance model), skip
   it.  No slot wasted on a sound nobody can hear.

2. **Get slot**: call `S_AL_GetFreeSource` — three passes:
   - Pass 1: find a stopped (free) slot immediately.  `S_AL_Update` reaps
     finished sources every frame, so free slots are always available first.
   - Pass 2: grow the pool dynamically (`qalGenSources`) up to `s_alMaxSrc`.
   - Pass 3: steal by priority (see below).

3. **Setup and play**: no channel-based lookup, no preemption guard, no
   looping model.  Each submission gets its own independent slot, exactly like
   DMA.  Voice, body, and footstep sounds naturally overlap (multiple slots per
   entity/channel) — this is correct behaviour and matches DMA.

**`S_AL_GetFreeSource` steal priority** (used only when pool is truly full):

| Tier | Target | Rationale |
|------|--------|-----------|
| 1 | ENTITYNUM_WORLD sources — farthest first | Brass casings 2000 u away are expendable; player shots never are |
| 2 | Non-local other-entity 3D sources — farthest first | Distant sounds already faded; nearby teammates stay alive |
| 3 | Any non-local non-loop source — oldest first | Age-based fallback |
| 4 | Any non-loop source — oldest first | Last resort; own-player sounds almost never reach here |

Loop (ambient) sources are never stolen — they have their own lifecycle.

**`S_AL_Update`** (every frame):
- **Reap stopped sources**: poll `AL_SOURCE_STATE`; any `AL_STOPPED` source
  immediately has its buffer detached and `isPlaying` cleared so Pass 1 of
  `S_AL_GetFreeSource` finds it on the next frame.

**Culling philosophy**: sounds are dropped by **distance** (rejection at start)
and by **age/distance** (eviction when stealing) — never by hitting an arbitrary
slot count.  If a surplus of sources ever causes issues, reduce `s_alMaxSrc`
(default 512) or lower `s_alDedupMs` from its default of 0 (disabled) to add
a light same-entity dedup window.

### Behaviour by scenario

| Scenario | Before (PR #93) | Now |
|----------|-----------------|-----|
| DE-50 — fire once, wait | ✓ | ✓ — own independent slot |
| DE-50 — spam trigger | 150 ms cap | ✓ — every shot gets a slot |
| DE-50 + back.wav + cock.wav burst | Two sounds killed | ✓ — three separate slots |
| LR-300 — hold trigger | Silent after ~3 shots | ✓ — each re-fire its own slot |
| MAC-11 — hold trigger | Seeked to 0 on each event | ✓ — each event its own slot |
| Voice / body / footstep overlap | Single slot per channel | ✓ — naturally concurrent |
| Brass / impacts — nearby | Pool exhaustion | ✓ — always heard within range |
| Brass / impacts — far away | Counted against pool cap | Rejected before allocation |
| 38-player server peak | Guards & caps active | Pool grows dynamically; steal world-entity first |

---

## Cleanup Audit — What Was Wrong and Why

### The root problem, stated plainly

`S_AL_StartSound` had a "newest sound wins" preemption model — any new sound
on the same `(entnum, entchannel)` slot immediately hard-stopped whatever was
playing.  URT's weapon animation system submits all three DE-50 sounds
(`de_out.wav`, `back.wav`, `cock.wav`) on `CHAN_WEAPON` within the same server
frame burst (~30 ms).  Each one killed its predecessor before a single sample
of the crack was audible.  This was confirmed by `s_alDebugPlayback 1` in
PR #89 — `de_out.wav` was being cut at **1 % completion** by `back.wav`.

**Every audio quality complaint prior to PR #90 — "wet blanket", "no punch",
"tinny", "muffled", "lacking crack" — was this bug.  Not HRTF.  Not the
occlusion filter.  Not the resampler.  Preemption cutting the attack transient
at 30 ms.**

PRs #84 through #88 were written without knowing this.  Some of them contain
genuine fixes to real independent problems.  Others are pure workarounds for a
symptom whose cause was not yet understood.  The table below separates them.

---

### PR-by-PR verdict

#### PR #84 — "Fix audio issues with desert eagle and map sounds"

| Change | Verdict | Reason |
|--------|---------|--------|
| Remove `CONTENTS_PLAYERCLIP` from occlusion LOS traces | ✅ **GENUINE** | Playerclip geometry was genuinely producing false occlusion on slopes/ledges regardless of preemption |
| `CHAN_WEAPON` 160 u no-occlusion zone (`s_alOccWeaponDist`) | ✅ **GENUINE** | Gun-muzzle world-origin offset (~50–100 u ahead of player eye) is a real geometry issue |
| Ambient loop fade-in / fade-out | ✅ **GENUINE** | Ambient pop-in/pop-out is a real problem unrelated to preemption |
| Dynamic reverb look-ahead rays + velocity-adaptive IIR | ✅ **GENUINE** | Spatial enhancement; no relation to weapon sound |
| Tuning CVars (`s_alEnv*`, `s_alOccWeaponDist`) | ✅ **GENUINE** | User control for the above |

No hacks introduced.  All changes in this PR stand.

---

#### PR #85 — "Occlusion smoothing, filter tuning, debug CVars, source pool hardening"

| Change | Verdict | Reason |
|--------|---------|--------|
| Replace `occ²` GAINHF with tunable floor curves | ✅ **GENUINE** | `occ²` was reaching 0.01 through any wall, stripping 99 % of HF content — independently bad regardless of preemption |
| Three-phase occlusion (trace / smooth / apply separately) | ✅ **GENUINE** | Eliminates boundary snaps; IIR smoothing is correct regardless |
| `AL_FILTER_NULL` hard-detach when `occ > 0.98` | ✅ **GENUINE** | No reason to keep a biquad in the signal path when the source is fully clear |
| Source pool hardening (distance eviction, world-entity cap, dedup window) | ✅ **GENUINE** | Real pool exhaustion under 30+ players; matches DMA's own mechanisms |
| "Tinny sound" complaint used to motivate filter changes | ⚠️ **MISDIAGNOSIS** | The tinny/popping perception was preemption cutting the crack.  The filter changes were correct for independent reasons, but the stated motivation was wrong |

No hacks to remove.  All changes stand.

---

#### PR #86 — "Fix DE-50 weapon fire HRTF coloration ('wet blanket')"

**The entire PR was written against a wrong diagnosis.**  Every symptom
described — "wet blanket", "no punch", "lacking crack" — was preemption cutting
`de_out.wav` at 30 ms, not HRTF convolution.

| Change | Verdict | Reason |
|--------|---------|--------|
| `AL_DIRECT_CHANNELS_SOFT = AL_TRUE` for local sources | ✅ **FIXED** | Now gated to `s_alHRTF 1` only — no-op when HRTF is off (default), and correctly bypasses centre-HRIR smearing for headphone users when HRTF is on. |
| Path-based occlusion bypass — `sound/weapons/` files on `CHAN_AUTO` | ✅ **REMOVED** | Added because "secondary weapon layers were muffled".  They were muffled because preemption killed them before they played.  With preemption fixed, secondary layers play on their own slots exactly like DMA.  Path-prefix check removed; bypass is now CHAN_WEAPON-only. |
| 3D sources start with `AL_FILTER_NULL` instead of attaching filter object | ⚠️ **AUDIT** | Motivation: "group-delay coloration on the transient of other players' weapon sounds".  That coloration was preemption.  However, the `AL_FILTER_NULL`-until-first-trace pattern does have independent merit: don't put a biquad in the signal path before you know it's needed.  Leave it for now but note the motivation was wrong. |

**`s_alDirectChannels` cvar (PR #88): scope now narrowed** — only applies when
`s_alHRTF 1` is set.  The cvar is retained as a useful knob for headphone users.

---

#### PR #87 — "Guarantee true passthrough when all processing cvars are 0"

This PR was written to construct a "known-clean" baseline to isolate what was
causing the "wet blanket".  The baseline was never needed because the cause
was always preemption.  However, several real bugs were found and fixed as
collateral:

| Change | Verdict | Reason |
|--------|---------|--------|
| `s_volume` double-apply fix (`S_AL_GetMasterVol` returning `255` not `s_volume×255`) | ✅ **GENUINE BUG FIX** | Was applying `s_volume²` to all sources — at the default 0.8, weapons were at 64 % amplitude |
| Live category-vol update (`s_alVolSelf` etc.) moved outside EFX guard | ✅ **GENUINE BUG FIX** | When `s_alEFX 0`, changing volume cvars at runtime had no effect on local sources |
| No-EFX occlusion gain path double-applied `masterGain` | ✅ **GENUINE BUG FIX** | Real double-multiply in the non-EFX path |
| `/sndinfo` processing-feature diagnostic table | ✅ **GENUINE** | Permanently useful diagnostic tool |
| `s_alEFX 0` kill switch | ✅ **KEEP FOR DIAGNOSTICS** | Still useful for isolating future issues |
| `s_alHRTF` default changed from `1` → `0` | ⚠️ **MOTIVATED BY WRONG DIAGNOSIS** | Changed because HRTF was blamed for "wet blanket".  `0` is a safe default (HRTF is headphone-only), but the reasoning was wrong.  Leave as 0. |
| `alcResetDeviceSoft` Layer-3 belt-and-suspenders HRTF-off | ⚠️ **MOTIVATED BY WRONG DIAGNOSIS** | Added to be certain HRTF was truly off when `s_alHRTF 0`.  Defensively correct as belt-and-suspenders.  Leave it but note the reason it was added was wrong. |
| Explicit `ALC_HRTF_SOFT = ALC_FALSE` when `s_alHRTF 0` | ✅ **CORRECT MECHANISM** | Ensures the cvar actually disables HRTF even when alsoft.conf overrides it.  The mechanism is correct regardless of what motivated it. |

---

#### PR #88 — "`s_alDirectChannels` cvar"

| Change | Verdict | Reason |
|--------|---------|--------|
| `s_alDirectChannels` cvar + split extension tracking | ✅ **RETAINED (narrowed)** | Added to make `AL_DIRECT_CHANNELS_SOFT` user-toggleable.  Now that the usage is gated to `s_alHRTF 1` only, the cvar's scope is narrowed accordingly — it controls HRTF bypass for headphone users only.  Cvar kept as a useful diagnostic and opt-out knob. |

---

#### PR #89 — "`s_alDebugPlayback` diagnostic cvar"

| Change | Verdict | Reason |
|--------|---------|--------|
| `s_alDebugPlayback` cvar + `fileRate` persistence + `s_al_deviceFreq` | ✅ **PERMANENTLY VALUABLE** | This PR found the actual root cause.  Keep forever. |

---

#### PR #90 — "CHAN_WEAPON minimum play-time guard (100 ms)"

| Change | Verdict | Reason |
|--------|---------|--------|
| `CHAN_WEAPON_MIN_PLAY_MS = 100` time-based guard | ❌ **BANDAID — SUPERSEDED** | Correctly identified preemption as the problem but used wall-clock time instead of audio content.  A 100 ms wall-clock guard blocked rapid-fire at any cadence faster than 100 ms, and did nothing for non-CHAN_WEAPON sounds.  Replaced by the content-based guard in #91 and the capped looping model in the current PR. |

---

#### PRs #91 / #92 — "Adaptive content-based guard"

| Change | Verdict | Reason |
|--------|---------|--------|
| `contentSamples` PCM scan at load time | ✅ **KEEP** | The machinery is correct and still used by the current implementation |
| Full-`contentSamples` guard on `CHAN_WEAPON` | ❌ **REPLACED** | Blocking `de_out.wav`'s full 2.5 s content caused spam-trigger silence.  Replaced by the 150 ms cap in the current PR. |
| `AL_SOURCE_STATE` check before reading `AL_SAMPLE_OFFSET` (#92) | ✅ **GENUINE FIX** | Real bug: offset resets to 0 on `AL_STOPPED`; guard would falsely re-engage.  Still present in the current implementation. |

---

### Summary — what to clean up next session

| Item | File | Action |
|------|------|--------|
| `S_AL_WEAPON_SOUND_PREFIX` path-based occlusion bypass | `snd_openal.c` | ✅ **Done** — removed; occlusion bypass now CHAN_WEAPON-only |
| `AL_DIRECT_CHANNELS_SOFT` unconditional on all local sources | `snd_openal.c` | ✅ **Done** — gated to `s_alHRTF 1` only; no-op when HRTF is off (default) |
| `s_alDirectChannels` cvar (PR #88) | `snd_openal.c` | ✅ **Done** — description updated; cvar kept for headphone users who enable `s_alHRTF 1` |
| 3D sources starting with `AL_FILTER_NULL` instead of filter object | `snd_openal.c` | **Leave for now** — independent merit; re-evaluate after other cleanup |
| `alcResetDeviceSoft` Layer-3 HRTF disable | `snd_openal.c` | **Leave** — defensively correct even if motivated by wrong diagnosis |
| Stale `SOUND.md` references to old 100 ms guard | `docs/project/SOUND.md` | ✅ **Done in prior PR** |

---

## Per-Map Ambient Normalisation Cache

### Problem: outlier-loud map loops

Some maps (notably `ut4_turnpike`) have ambient loop sounds that are far too
loud even after volume cvars are tuned.  The root cause was a bug in
`S_AL_CalcNormGain`: it scanned only the **first 4 seconds** of each audio
file.  A loop with a quiet intro followed by a loud main body received an
inflated `normGain` (based on the quiet intro), causing the bulk of playback
to be far too loud.

The second problem was that the old normaliser was **bidirectional** — it both
cut loud sounds *and* boosted quiet sounds toward a fixed target RMS of 0.085
(−21 dBFS).  This collapsed the relative loudness differences between ambient
sounds on the same map: a waterfall would end up at the same perceived volume
as a gentle breeze, which is not the map designer's intent.

### Solution: strided scan + one-sided ceiling limiter

`S_AL_CalcNormGain` now uses a **strided full-file scan**:

```
stride = totalFrames / S_AL_NORM_SCAN_SAMPLES   (at most 4 s worth of frames)
```

Frames are sampled evenly across the entire file so that the RMS accurately
represents the loop's bulk energy, not just its intro.

The normalisation formula is now a **one-sided ceiling limiter**:

```
normGain = min(1.0,  S_AL_NORM_CEILING / RMS)

S_AL_NORM_CEILING = 0.316   (≈ −10 dBFS)
```

- Sounds with RMS ≤ 0.316: `normGain = 1.0` — play at natural level, no cut,
  no boost.  A quiet breeze stays quiet; a moderately loud waterfall stays at
  its intended loudness.
- Sounds with RMS > 0.316: `normGain < 1.0` — proportional cut bringing the
  sound down to the ceiling.  Only genuine outliers (broadcast-hot, heavily
  compressed, or clipped ambient WAVs) are affected.
- Safety floor: `normGain` is clamped to a minimum of `0.05` for pathological
  files (e.g. square-wave clipping at RMS 1.0).

This preserves the **relative loudness** between ambient sounds on the same
map while preventing any single over-recorded loop from dominating.

### Per-map normcache file

On the **first load** of each map, the engine:

1. Parses the BSP entity lump (`CM_EntityString()`) to discover every sound
   file referenced in `worldspawn` (`ambient`, `music`) and every
   `target_speaker` entity (`noise`).
2. Force-registers all of those sounds so their `normGain` is computed via the
   strided scan.
3. Writes a plain-text cache file:
   ```
   normcache/ut4_turnpike.cfg
   ```

On **subsequent loads** the cache is read during `S_AL_BeginRegistration`
(before cgame registers its sounds) so the cached values take effect for every
`RegisterSound` call.

#### Cache file format

```
// Quake3e-urt per-map ambient normalisation cache
// map: maps/ut4_turnpike.bsp
// normGain 0.05–1.00  (1.0 = natural level; lower = quieter)
// Edit values to tune loudness. Delete file to regenerate.
// Reload with: snd_normcache_rebuild
sound/urban_terror/traffic_loop.wav 0.2914
sound/env/wind.wav 1.0000
music/ut4_turnpike.ogg 0.4531
```

The file lives in the game's write directory (the same location as `q3config.cfg`).

#### Tuning procedure

1. Load the map.
2. Set `s_alDebugNorm 1` — the console will print one line per active loop
   source each frame showing the current `normGain`.
3. Identify the loud outlier.  Its `normGain` will be well below `1.0`.
4. Open `normcache/<mapname>.cfg` in a text editor and lower the value
   further if needed, or raise it to allow a sound to be louder.
5. Type `snd_normcache_rebuild` in the console to reload and apply the values.
6. Repeat until the balance sounds correct.

#### Console command

```
snd_normcache_rebuild
```

Regenerates the cache for the currently loaded map from the decoded audio
files (discarding any hand-edits).  Use this after a map update that changes
audio content, or to reset a badly-edited cache.

---

## BSP Entity Acoustic Hints

### What is parsed

On the first dynamic-reverb probe cycle after a new map loads, the engine
calls `S_AL_ParseMapEntities()` which walks the BSP entity lump and extracts:

**From `worldspawn`:**

| Key | Stored in | Purpose |
|---|---|---|
| `sky` | `s_al_mapHints.skyShader` / `hasSky` | Whether the map has outdoor sky exposure |
| `ambient` | `s_al_mapHints.worldAmbient` | Global ambient sound path |
| `music` | `s_al_mapHints.worldMusic` | Background music track path |

**From every `target_speaker` with a `noise` key:**

| Key | Stored in | Purpose |
|---|---|---|
| `noise` | `s_al_bspSpeakers[i].noise` | Sound file path |
| `origin` | `s_al_bspSpeakers[i].origin` | World position |
| `spawnflags` | `s_al_bspSpeakers[i].spawnflags` | Bit 0 = GLOBAL (plays everywhere) |

Entity example:
```
{
"classname" "target_speaker"
"spawnflags" "1"
"noise" "sound/urban_terror/traffic_loop.wav"
"origin" "-1306.4 660 293.6"
}
```

### Uses

#### 1. `hasSky` → classifier cave-bonus dampening

If `worldspawn` has a `sky` key, the dynamic reverb classifier dampens the
`caveBonus` to 35% of its computed value.  This prevents basement sections and
stairwells of outdoor/urban maps from triggering the stone-tunnel reverb
character that is appropriate only for true underground caves.

#### 2. Cold-start speaker origin fallback

Static `target_speaker` entities emit their first sounds on the earliest frames
of a map load, before `S_UpdateEntityPosition` has been called for them.
Without a fallback, those sounds play from world-origin (0, 0, 0).

`S_AL_StartSound` now checks the BSP speaker list: if the entity origin cache
is still at (0, 0, 0) **and** exactly one BSP speaker uses the same noise file,
that speaker's BSP origin is used instead.  Ambiguous cases (multiple speakers
with the same file) are left for the natural entity-update path.

#### 3. Normcache population

All `noise` paths collected from `target_speaker` entities, plus `worldAmbient`
and `worldMusic`, are pre-registered at map load via `S_AL_PreRegisterMapSounds`
so their `normGain` is computed before the normcache is written.
