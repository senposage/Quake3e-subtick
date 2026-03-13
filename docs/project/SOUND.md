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

Controlled by the `s_alHRTF` cvar (default `1`).

### Distance Model — Q3/dmaHD-Matching Linear Attenuation

Uses `AL_LINEAR_DISTANCE_CLAMPED` with parameters derived from Q3/dmaHD constants:

| Parameter | Value | Source |
|-----------|-------|--------|
| `AL_REFERENCE_DISTANCE` | 80 | `SOUND_FULLVOLUME` from snd_dma.c/snd_dmahd.c |
| `AL_MAX_DISTANCE` | 1330 | `80 + 1/SOUND_ATTENUATE(0.0008)` |
| `AL_ROLLOFF_FACTOR` | 1.0 | matches Q3 linear slope |

Formula: `gain = 1 - 1.0 * (dist - 80) / (1330 - 80)` — identical to Q3 base.

### Occlusion — `CM_BoxTrace` + EFX Low-Pass Filter

Every 4 frames, for each playing 3D source, a ray is traced from the listener through solid BSP geometry (`CONTENTS_SOLID | CONTENTS_PLAYERCLIP`):

```c
CM_BoxTrace(&tr, listenerOrigin, srcOrigin, vec3_origin, vec3_origin,
            0, CONTENTS_SOLID | CONTENTS_PLAYERCLIP, qfalse);
occlusion = 0.1 + 0.9 * tr.fraction;  // 1.0 = clear, 0.1 = fully blocked
```

**With EFX** (`ALC_EXT_EFX`): Applied as a per-source `AL_DIRECT_FILTER` low-pass filter:
- `AL_LOWPASS_GAIN = 0.25 + 0.75 * occlusion` — overall attenuation
- `AL_LOWPASS_GAINHF = occlusion²` — quadratic high-frequency roll-off

This makes occluded sounds lose treble energy (muffled through walls) rather than just becoming quieter — matching real-world acoustics.

**Without EFX**: Simple `AL_GAIN` scale applied directly to the source.

### EFX Environmental Effects (`ALC_EXT_EFX`)

All EFX entry points are loaded via `alcGetProcAddress(device, ...)` at runtime — no link-time EFX dependency.

**Two auxiliary effect slots** are created:

| Slot | Effect | Used when |
|------|--------|-----------|
| Reverb slot | EAX reverb (or plain AL_EFFECT_REVERB fallback) | always — medium indoor room preset |
| Underwater slot | EAX reverb, heavy HF cut (GAINHF=0.1) | listener inside water volume |

**Indoor reverb preset** (EAX parameters, tuned for URT urban maps):
- Density 0.5, Diffusion 0.85, Gain 0.32, Decay 1.49s, HF ratio 0.83
- Reflections delay 7ms, Late reverb delay 11ms, Air absorption 0.994

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
alSourcei(src, AL_DIRECT_FILTER,       AL_FILTER_NULL);  /* clear any inherited filter */
alSourcei(src, AL_DIRECT_CHANNELS_SOFT, AL_TRUE);        /* bypass HRTF — see below */
```

> **Why `AL_DIRECT_CHANNELS_SOFT` for local sources**: even a source at `AL_POSITION (0,0,0)` relative to the listener is processed through the HRTF "center" HRIR when HRTF mode is active.  That convolution smears the initial transient attack of weapon sounds (e.g. `de.wav`) and produces a characteristic "wet blanket / lacking punch" coloration.  `AL_DIRECT_CHANNELS_SOFT = AL_TRUE` routes the PCM directly to the stereo output, completely bypassing the HRTF kernel for head-locked sounds.  3D sources (other players' weapons) are not affected — they go through the normal HRTF path for correct directional rendering.

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

Up to `MAX_CHANNELS` (96) OpenAL sources are pre-allocated at init. Source stealing (oldest non-looping source) handles overflow gracefully.

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
| `s_alDirectChannels` | `1` | Route own-player (local) sources directly to the stereo output, bypassing the HRTF centre-HRIR convolution. **Default 1 (on)** — preserves the transient punch of weapon sounds (e.g. de.wav). Set to 0 to route all sources through the full OpenAL pipeline. Requires `AL_SOFT_direct_channels` extension. (LATCH) |
| `s_alMaxDist` | `1330` | Max attenuation distance in game units; matches vanilla dma range |
| `s_alReverb` | `0` | Master EFX reverb toggle. **Default 0 = vanilla dma behaviour** |
| `s_alReverbGain` | `0.20` | EFX wet slot gain [0–1]. Ceiling when `s_alDynamicReverb 1` |
| `s_alReverbDecay` | `1.49` | Reverb decay in seconds; used when `s_alDynamicReverb 0` (LATCH) |
| `s_alDynamicReverb` | `0` | Ray-traced acoustic environment. **Default 0 = opt-in** |
| `s_alDebugReverb` | `0` | Debug output for dynamic reverb. `1` = env label + smoothed params every probe cycle. `2` = also raw probe data and filter-reset events. Not archived. |
| `s_alOcclusion` | `1` | Wall-occlusion tracing with HRTF direction correction |
| `s_alOccGainFloor` | `0.55` | Floor of `AL_LOWPASS_GAIN` for fully-blocked sources [0–1]. `1.0` = no volume change through walls (vanilla-like). `0.0` = can go fully silent. Default 0.55 keeps occluded sounds audible with a modest volume jump at line-of-sight. |
| `s_alOccHFFloor` | `0.50` | Floor of `AL_LOWPASS_GAINHF` (high-frequency content) for fully-blocked sources [0–1]. `1.0` = no HF filtering (only volume changes). `0.0` = bass-only thud through walls. Default 0.50 preserves enough weapon crack to remain recognisable. Old formula `occ²` reached near-zero HF even at moderate occlusion — this was the main cause of the "tinny pop" corner transition. |
| `s_alOccPosBlend` | `0.25` | How far to redirect HRTF apparent-source position towards the nearest acoustic gap [0–1]. `0.0` = always true source position (best direction accuracy). `1.0` = full gap redirect (old behaviour — confused weapon direction). Default 0.25: subtle gap hint that aids "around the corner" perception without sacrificing localizability. |
| `s_alDebugOcc` | `0` | Print per-source occlusion state each frame. Each occluded source shows entity, distance, trace target, smoothed gain, GAIN and GAINHF values. Use to identify which sounds are being filtered and by how much. Not archived. |
| `s_alVolSelf` | `1.0` | Own player/weapon/breath volume multiplier **[0–1.5]** |
| `s_alVolOther` | `0.7` | Other player/entity volume multiplier **[0–1.0]** — capped, anti-cheat |
| `s_alVolEnv` | `0.3` | Looping ambient/environmental volume multiplier **[0–1.0]** — capped |
| `s_alVolUI` | `0.8` | Hit-marker/kill-confirmation/UI sound multiplier **[0–1.0]** — `StartLocalSound` (entnum=0) only, independent of weapon volume |

#### Selecting an output device (`/s_devices`)

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
s_alReverb 0  s_alOcclusion 0  →  vanilla dma: distance-only, no wall effects
s_alReverb 0  s_alOcclusion 1  →  occlusion + HRTF direction fix (default)
s_alReverb 1  s_alOcclusion 1  →  + static EFX reverb room
s_alReverb 1  s_alDynamicReverb 1  →  + dynamic ray-traced reverb environment
```

#### Vanilla-behaviour notes

- **`s_alReverb 0` (default)**: no EFX reverb, matches plain dma/Q3 audio.
- **`s_alOcclusion 1` (default)**: sources behind walls are gain-attenuated and their
  apparent HRTF direction is partially redirected towards the nearest audible gap
  (controlled by `s_alOccPosBlend`).  Only `CONTENTS_SOLID` brushes are tested —
  `CONTENTS_PLAYERCLIP` is excluded so invisible movement-constraint geometry on
  slopes and ledges does not cause false muffling of nearby weapon fire.
- **CHAN_WEAPON proximity bypass**: weapon-channel sources and any `sound/weapons/`
  sound within `s_alOccWeaponDist` (default 160 u) of the listener skip occlusion
  entirely, covering the gun-muzzle offset (~50–100 u ahead of the player eye) that
  would otherwise trigger false filter attenuation from nearby slope/clip geometry.
  The path-based check ensures secondary weapon layers on `CHAN_AUTO` also receive the
  bypass (e.g. a tail layer played alongside a primary `CHAN_WEAPON` crack).
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
| CHAN_WEAPON or `sound/weapons/` < `s_alOccWeaponDist` u | every frame — no trace | Gun-muzzle zone; always pass-through; covers CHAN_AUTO weapon layers |
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

- `s_alLocalSelf 1` (default): own-entity sounds → `AL_SOURCE_RELATIVE=TRUE`,
  position (0,0,0), no rolloff, `AL_DIRECT_FILTER = AL_FILTER_NULL`.
- **Stale reverb fix**: when a local source reuses a slot previously used for a
  3D sound, the auxiliary send is now explicitly disconnected — preventing an
  unwanted reverb tail on own-player weapons and footsteps.

#### Dynamic acoustic environment (`s_alDynamicReverb 1`)

Casts **14 world-space rays** from the listener every **16 frames** plus up to **2 look-ahead rays** when the player is moving (≈ 1.0 traces/frame average, zero when the movement cache is active):
- 8 horizontal (every 45°) — max `s_alEnvHorizDist` u (default 1330)
- 4 diagonal-up (45° elevation, N/E/S/W) — max `s_alEnvVertDist` u (default 400)
- 1 straight up — max `s_alEnvVertDist` u (default 400)
- 1 straight down — max `s_alEnvDownDist` u (default 160)
- 0–2 look-ahead (direction of travel) — max `s_alEnvHorizDist` u, only when `moveDist > s_alEnvVelThresh`

**openFrac** is a weighted average: `s_alEnvHorizWeight` × horizontal + (1 − `s_alEnvHorizWeight`) × vertical (defaults 70 / 30 %).  When the player is moving faster than `s_alEnvVelThresh` units per probe cycle, up to `s_alEnvVelWeight` (default 30 %) of openFrac is blended from 2 look-ahead rays in the direction of travel — this prevents the classifier from stalling in the TRANSITION zone when walking from indoors to outdoors.

A **rolling history buffer** (`s_alEnvHistorySize` slots, default 3, max 8) averages the last N probe sets. A **movement cache** reuses metrics when the listener has moved < 32 units since the last probe.

Parameters blend smoothly via IIR pole `s_alEnvSmoothPole` (default 0.92, ≈ 3.5 s half-transition at 60 fps).  When the player is moving, the pole is automatically reduced by up to 0.07 (toward 0.85) so transitions respond faster during environment changes.

| Environment | `openFrac` | Target `decayTime` | Target late-gain |
|---|---|---|---|
| Tight room | low | ~0.4 s | very low |
| Normal indoor | low | ~1.2 s | low |
| Large hall/cave | low | ~2.0–2.5 s | moderate |
| Corridor | low + asymmetric | medium | low + moderate reflections |
| Semi-open / urban | ~0.30–0.55 | ~0.6–1.0 s | low |
| Open outdoor | > 0.55 | < 0.4 s | near-zero |

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

#### Debug workflow (`s_alDebugReverb`)

```
s_alReverb 1 ; s_alDynamicReverb 1 ; s_alDebugReverb 1
```
Prints one line per probe cycle (every 16 frames):
```
[dynreverb] env=MEDIUM_ROOM  size=0.42  open=0.18  corr=0.12  |  tgt: dec=1.24 lat=0.11 ref=0.04 slot=0.16  |  cur: dec=1.18 lat=0.10 ref=0.04 slot=0.16
```

```
s_alDebugReverb 2
```
Also prints raw pre-history probe values with the horizontal/vertical split, current history window, and player velocity:
```
             raw: size=0.44  horizOpen=0.21  vertOpen=0.08  corr=0.15  |  hist=3/3  vel=62  cache=miss
[alfilter] src#7 3D: reset stale filter (prev occ=0.23  GAIN≈0.42  GAINHF≈0.05)
[alfilter] src#2 local: cleared stale filter
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
| `s_alDirectChannels` | `1` | Route own-player sources directly to stereo output, bypassing HRTF centre-HRIR. (LATCH) |
| `s_alMaxDist` | `1330` | Override max attenuation distance (floor: 1330 Q3 units) |

- OpenAL Soft selects the native device rate automatically (no `s_khz` needed).
- `s_doppler` controls `AL_DOPPLER_FACTOR` (1.0 = enabled, 0.0 = disabled).
- HRTF uses OpenAL Soft's built-in CIPIC / MIT KEMAR datasets — no custom DSP code.
- EFX (`ALC_EXT_EFX`) is detected and enabled when `s_alEFX 1`; set to 0 to bypass entirely.
- Occlusion traced via `CM_BoxTrace` every 4 frames per source; applied as EFX low-pass filter if available, else gain scale.
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
