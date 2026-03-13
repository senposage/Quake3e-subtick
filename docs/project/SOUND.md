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

**Underwater mode**: When `S_AL_Respatialize` reports `inwater != 0`, all active 3D sources are switched from the reverb slot to the underwater slot in the next `S_AL_Update` tick.

**EFX source wiring** (set in `S_AL_SrcSetup`):
```c
/* Non-local (3-D) sources: */
alFilterf(occlusionFilter[idx], AL_LOWPASS_GAIN,   1.0f);  /* reset to pass-through */
alFilterf(occlusionFilter[idx], AL_LOWPASS_GAINHF, 1.0f);
alSource3i(src, AL_AUXILIARY_SEND_FILTER, reverbSlot, 0, AL_FILTER_NULL);
alSourcei(src,  AL_DIRECT_FILTER, occlusionFilter[idx]);
alSourcei(src,  AL_AUXILIARY_SEND_FILTER_GAIN_AUTO,   AL_TRUE);
alSourcei(src,  AL_AUXILIARY_SEND_FILTER_GAINHF_AUTO, AL_TRUE);
alSourcei(src,  AL_DIRECT_FILTER_GAINHF_AUTO,         AL_FALSE);

/* Local (own-player / weapon) sources: */
alSourcei(src, AL_DIRECT_FILTER, AL_FILTER_NULL);  /* clear any inherited filter */
```

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
| `s_alHRTF` | `1` | Enable HRTF binaural rendering via OpenAL Soft (LATCH) |
| `s_alMaxDist` | `1330` | Max attenuation distance in game units; matches vanilla dma range |
| `s_alReverb` | `0` | Master EFX reverb toggle. **Default 0 = vanilla dma behaviour** |
| `s_alReverbGain` | `0.20` | EFX wet slot gain [0–1]. Ceiling when `s_alDynamicReverb 1` |
| `s_alReverbDecay` | `1.49` | Reverb decay in seconds; used when `s_alDynamicReverb 0` (LATCH) |
| `s_alDynamicReverb` | `0` | Ray-traced acoustic environment. **Default 0 = opt-in** |
| `s_alDebugReverb` | `0` | Debug output for dynamic reverb. `1` = env label + smoothed params every probe cycle. `2` = also raw probe data and filter-reset events. Not archived. |
| `s_alOcclusion` | `1` | Wall-occlusion tracing with HRTF direction correction |
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
- **`s_alOcclusion 1` (default)**: sources behind walls are muffled and their
  apparent HRTF direction is redirected to the nearest audible gap.  This is
  _less_ of a positional wallhack than raw HRTF (which reveals exact 3-D source
  direction through walls) while keeping the "you can hear them if close enough"
  vanilla expectation via the standard distance-attenuation model.
- `s_alMaxDist 1330` is the vanilla dma maximum audible radius; sounds beyond
  this distance are inaudible regardless of wall status.

#### Occlusion update cadence (distance-adaptive)

| Source distance | Update interval | Rationale |
|---|---|---|
| < 80 u (full-vol zone) | every frame — no trace | Wall impossible at this range; real position used |
| 80 – 300 u | every frame | Nearby players — maximum HRTF precision |
| 300 – 600 u | every 4 frames | Medium range |
| > 600 u | every 8 frames | Far — distance model dominates |

#### Dynamic acoustic environment (`s_alDynamicReverb 1`)

Casts **14 world-space rays** from the listener every **16 frames** (≈ 0.9 traces/frame average, zero when the movement cache is active):
- 8 horizontal (every 45°) — max **1330 u** (full attenuation range)
- 4 diagonal-up (45° elevation, N/E/S/W) — max **400 u** (ceiling sensitivity up to ~283 u height)
- 1 straight up — max **400 u** (ceiling up to 400 u)
- 1 straight down — max **160 u** (floor / pit context)

Per-ray distances are calibrated to the geometry scale each group is measuring. Using a single global max (1330 u) for all rays made a 200-unit indoor ceiling appear nearly open (fr ≈ 0.15) and made overhangs invisible to the vertical metric. Shorter vertical distances give the classifier appropriate sensitivity to real URT ceiling heights.

**openFrac** is a *weighted average of horizontal and vertical ray escape fractions*: **70 % horizontal (rays 0–7) + 30 % vertical (rays 8–13)**. Horizontal rays are the primary acoustic indicator — walls around you drive reverb far more than a ceiling above you. Giving vertical rays only 30 % weight prevents overhead obstructions — a leaning pillar, a low bridge, a covered colonnade, an archway ceiling — from falsely reclassifying an open outdoor area as enclosed and triggering heavy reverb as the player walks beneath them.

**sizeFactor** likewise uses horizontal-only average distance so a low ceiling or overhang cannot shrink the perceived acoustic room size.

Both metrics still use continuous ray fractions (not binary hit/miss) — a ray travelling 1 000 / 1 330 units contributes 0.75 rather than 0, giving a smooth spatial gradient instead of a hard classification jump at doorway and arch thresholds.

A **rolling history buffer** (3 slots) averages the last three probe sets. A single confused reading — caused by doorway geometry, windows, or thin walls — only shifts the average by ⅓, preventing transient wrong classifications from audibly changing reverb. This also suppresses the "instant-apply / un-apply / re-apply" flicker observed at doorway thresholds on maps like ut4_sanc.

A **movement cache** reuses metrics when the listener has moved < 32 units since the last probe, saving CM traces for stationary players.

Metrics derived: `avgDist` (room size), `openFrac` (openness, continuous), `corrFactor` (corridor asymmetry).

| Environment | `avgDist` | `openFrac` | Target `decayTime` | Target late-gain |
|---|---|---|---|---|
| Tight room | < 80 u | low | ~0.4 s | very low |
| Normal indoor | ~200 u | low | ~1.2 s | low |
| Large hall/cave | > 400 u | low | ~2.0–2.5 s | moderate |
| Corridor | asymmetric (< 150 u short side, ≥ 1.5× ratio) | low | medium | low; moderate reflections |
| Semi-open / urban | any | ~0.4–0.6 | ~0.6–1.0 s | low |
| Open outdoor | any | > 0.7 | < 0.4 s | near-zero |

Parameters blend smoothly (pole 0.92, ~3.5 s half-transition at 60 fps) so room changes sound natural. EFX targets are intentionally conservative (late-gain max 0.35, decay max 2.5 s) so reverb colours the space without drowning layered weapon sounds.

#### Comparison with dmaHD

`snd_dmahd.c` uses no environmental detection or reverb at all — it applies only per-frame binaural HRTF panning (distance + ear-delay model). Transitions between areas are "smooth" in dmaHD purely because the PCM mixer runs every frame and continuously recomputes left/right volumes from the listener position with no state machine involved. The lesson applied here: keep the EFX state machine lightweight, bias heavily toward stability over accuracy, and ensure the IIR smoother (pole 0.92) always prevents any single confused probe from producing an audible jump.

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
Also prints raw pre-history probe values (with the horizontal/vertical openness split) and filter-reset events:
```
             raw: size=0.44  horizOpen=0.21  vertOpen=0.08  corr=0.15  |  hist=3/3  cache=miss
[alfilter] src#7 3D: reset stale filter (prev occ=0.23  GAIN≈0.42  GAINHF≈0.05)
[alfilter] src#2 local: cleared stale filter
```
The `horizOpen` / `vertOpen` split is especially useful for diagnosing the **overhang edge case**: a large `horizOpen` with a small `vertOpen` means an overhead obstruction (pillar, bridge, archway) is blocking vertical rays — the 70/30 weighting ensures this does not inflate reverb. Use these lines to diagnose doorway flicker (rapid `env=` label changes), over-aggressive corridor detection (`corr` spikes), and stale-filter weapon muting (filter lines appear when a weapon fires).

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
| `s_alHRTF` | `1` | Enable three-layer HRTF + higher-order Ambisonic rendering |
| `s_alMaxDist` | `1024` | Override max attenuation distance (floor: 1330 Q3 units) |

- OpenAL Soft selects the native device rate automatically (no `s_khz` needed).
- `s_doppler` controls `AL_DOPPLER_FACTOR` (1.0 = enabled, 0.0 = disabled).
- HRTF uses OpenAL Soft's built-in CIPIC / MIT KEMAR datasets — no custom DSP code.
- EFX (`ALC_EXT_EFX`) is detected and enabled automatically; no cvar needed.
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
