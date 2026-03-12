# Sound System

> Covers `code/client/snd_main.c`, `snd_dma.c`, `snd_dmahd.c`, `snd_mem.c`, `snd_mix.c`, `snd_adpcm.c`, `snd_codec.c`, `snd_codec_wav.c`, `snd_wavelet.c`, and `snd_local.h`.
> This branch adds dmaHD HRTF/spatial sound integration via `snd_dmahd.c` (guarded by `#ifndef NO_DMAHD`).

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [snd_main.c — Sound Dispatcher](#snd_mainc--sound-dispatcher)
3. [snd_dma.c — Base DMA Backend](#snd_dmac--base-dma-backend)
4. [snd_dmahd.c — High-Definition Backend](#snd_dmahdcc--high-definition-backend)
5. [snd_mem.c — Sound Data Loading](#snd_memc--sound-data-loading)
6. [snd_mix.c — Mixing Engine](#snd_mixc--mixing-engine)
7. [Codecs](#codecs)
8. [Key Data Structures](#key-data-structures)
9. [Key Sound Cvars](#key-sound-cvars)

---

## Architecture Overview

```
Game calls S_StartSound(origin, entnum, channel, sfxHandle)
                │
                ▼
         snd_main.c dispatcher
         (si.StartSound / si.StartLocalSound)
                │
         ┌──────┴────────┐
         │               │
    snd_dma.c       snd_dmahd.c
  (base backend)   (HD backend)
         │               │
         └──────┬────────┘
                │
          snd_mix.c
       (channel mixing)
                │
                ▼
         platform audio driver
     (sdl_snd.c / win_snd.c / linux_snd.c)
                │
                ▼
           OS audio device
```

The sound system runs in the game thread (not a separate audio thread). `S_Update(msec)` is called from `CL_Frame` once per frame. The platform driver provides a DMA buffer; the engine writes mixed samples into it.

---

## snd_main.c — Sound Dispatcher

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

`S_Base_StartSound` enforces two independent guards before allocating a channel:

| Entity | Per-sound concurrent limit (`allowed`) | Rationale |
|--------|----------------------------------------|-----------|
| `listener_number` (local player) | 16 | Many distinct weapon/movement sounds |
| `ENTITYNUM_WORLD` (1022) | **3** | Impact/surface/casing sounds; 3 concurrent instances per clip is perceptually indistinguishable from more |
| Any other entity | 8 | Default |

Additionally, `ENTITYNUM_WORLD` is subject to a **total channel cap** of `WORLD_ENTITY_MAX_CHANNELS` (`MAX_CHANNELS / 6` = 16 out of 96). In multiplayer, all bullet impacts, ricochets, and brass casings share the world entity number. Without this cap, multiple players firing automatic weapons simultaneously can fill the entire 96-channel pool with impact sounds, forcing channel stealing for weapon and footstep sounds.

If either guard triggers, the sound is silently dropped (logged via `Com_DPrintf`).

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

### Build System

`Makefile` additions:
- `NO_DMAHD=0` (default) — dmaHD enabled
- `-DNO_DMAHD` conditional compile flag
- `snd_dmahd.o` included in client build when `NO_DMAHD=0`

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
| `s_doppler` | 1 | Enable Doppler effect (HD backend only) |
| `s_khz` | 44 | Playback frequency (11/22/44/48 kHz) |
| `s_mixahead` | 0.2 | How far ahead to mix (seconds) |
| `s_mixPreStep` | 0.05 | Mix step size |
| `s_compression` | 0 | Sound compression: 0=none, 1=wavelet, 2=ADPCM |
| `s_useHD` | 1 | Use HD backend (higher quality mixing) |

### dmaHD-Specific Behavior [URT]

When dmaHD is active (`NO_DMAHD` not set):
- `s_khz` defaults to **48** (used as fallback; on Windows WASAPI the native device rate is used regardless)
- `s_doppler` enables HRTF Doppler simulation
- On Windows, WASAPI queries `GetMixFormat` and matches the native device rate + format exactly — **no resampling, no APO disruption** (typically 48 000 Hz / 32-bit float on modern hardware)
- DirectSound fallback: 48 000 Hz / stereo / 16-bit PCM
- Sound loading goes through `dmaHD_LoadSound` for HRTF pre-processing

Build flag: compile with `NO_DMAHD=1` to disable dmaHD and use the standard DMA backend.
