# Sound System — Redesign Notes (in-progress)

> **For continuing agents:** This document tracks the in-flight changes being
> made to `code/client/snd_openal.c`.  Read it **before** opening the source
> file.  Every section below has a ✅ (done), 🔧 (partially done), or ❌ (not
> yet started) marker.  Complete the ❌ and 🔧 items in order before touching
> anything else.

---

## Why this document exists

The change set is large enough that it cannot be completed in a single agent
session without risk of losing context.  This file is the authoritative
checkpoint record.  Update the status markers as you complete each item.

---

## Background: the problems being fixed

### 1. Ambient loop volume outliers (ut4_turnpike et al.)

The old `S_AL_CalcNormGain` scanned only the **first 4 seconds** of each
ambient file.  Any loop with a quiet intro followed by a loud main body
received an inflated `normGain` (computed from the quiet intro), causing the
bulk of the playback to be far too loud.  ut4_turnpike's traffic loop is the
canonical example.

Additionally, the old normaliser was **bidirectional** — it both cut loud
sounds and boosted quiet ones toward a fixed target RMS (0.085 / −21 dBFS).
This collapsed the intended loudness differences between different ambient
sounds on the same map (a waterfall should still be louder than wind after
normalisation).

### 2. No map-level context in the acoustic classifier

The dynamic reverb classifier (`S_AL_UpdateDynamicReverb`) was purely
geometry-based.  It had no knowledge of what kind of map it was operating on.
As a result, a basement section of an outdoor urban map could be
misclassified as CAVE and receive stone-hall reverb.

### 3. Cold-start positional error for map speakers

Static `target_speaker` entities emit their first sound on the earliest frames
of a map load, before `S_UpdateEntityPosition` has been called for them.  The
engine fell back to world-origin (0, 0, 0) in that window, placing the sound
at the wrong position.

### 4. No persistent normalisation data

Every session required re-decoding audio files to recompute `normGain`.  There
was no way for a player or server admin to tune the per-map ambient balance
without recompiling.

### 5. MAP sounds not occluded through walls

BSP `target_speaker` loop sources (ambient machinery, fans, dripping water,
etc.) were **completely excluded from occlusion processing** — the
`S_Update` occlusion loop skipped every source with `loopSound == qtrue`.
This meant a player on the other side of a thick concrete wall still heard
the speaker at full volume and full brightness.

---

## Changes overview

### A. `S_AL_CalcNormGain` — strided scan + one-sided ceiling  ✅ DONE

**File:** `code/client/snd_openal.c`

Old behaviour:
```c
// scan first 4 s only
// normGain = TARGET_RMS / rms   (bidirectional: boosts quiet, cuts loud)
// clamp [0.10, 4.0]
```

New behaviour:
```c
#define S_AL_NORM_SCAN_SAMPLES  (44100 * 4)   // max frames to analyse
#define S_AL_NORM_CEILING       0.316f         // ≈ −10 dBFS

// stride = totalFrames / SCAN_SAMPLES  → evenly covers full file
// normGain = min(1.0, CEILING / rms)   // one-sided: only cut, never boost
// clamp floor 0.05                     // safety net for pathological files
```

Key design decisions:
- **Strided scan**: divides the full file into at most `S_AL_NORM_SCAN_SAMPLES`
  evenly-spaced frames, giving an accurate RMS for the entire loop including
  the loud main body after any quiet intro.
- **One-sided ceiling**: sounds with RMS ≤ 0.316 receive `normGain = 1.0`
  (natural level, no change).  Only sounds exceeding the ceiling are cut.
  This preserves the relative loudness intended by the map designer.
- **No boost**: `normGain` is capped at `1.0` — quiet sounds stay at their
  natural level; they are never amplified.

---

### B. BSP entity data structures  ✅ DONE

**File:** `code/client/snd_openal.c` — after `s_al_efx` static

New types and statics added:

```c
#define S_AL_BSP_SPEAKERS_MAX 256

typedef struct {
    char   noise[MAX_QPATH]; // "noise" key — sound file path
    vec3_t origin;           // parsed world position
    int    spawnflags;       // bit 0 = GLOBAL (plays everywhere)
} alBspSpeaker_t;

typedef struct {
    qboolean parsed;
    qboolean hasSky;                  // worldspawn "sky" key present
    char     skyShader[64];
    char     worldAmbient[MAX_QPATH]; // worldspawn "ambient"
    char     worldMusic[MAX_QPATH];   // worldspawn "music"
    int      globalSpeakerCount;      // target_speaker with spawnflags & 1
    int      totalSpeakerCount;
} alMapHints_t;

#define S_AL_NORMCACHE_MAX 512

typedef struct {
    char  path[MAX_QPATH];
    float normGain;
} alNormCacheEntry_t;

static alMapHints_t        s_al_mapHints;
static alBspSpeaker_t      s_al_bspSpeakers[S_AL_BSP_SPEAKERS_MAX];
static int                 s_al_numBspSpeakers;
static alNormCacheEntry_t  s_al_normCache[S_AL_NORMCACHE_MAX];
static int                 s_al_normCacheCount;
static qboolean            s_al_normCacheWritten;
```

Also added: `static cvar_t *s_alDebugNorm;`

---

### C. Normcache override in `S_AL_RegisterSound`  ✅ DONE

After `r->normGain = S_AL_CalcNormGain(pcm, &info)`, a loop over
`s_al_normCache[]` applies any loaded per-map override before the PCM is
freed.

---

### D. BSP origin fallback in `S_AL_StartSound`  ✅ DONE

In the `origin == NULL` branch, after copying `s_al_entity_origins[entnum]`,
a new block checks if `sndOrigin` is still `vec3_origin` (cold-start).  If
exactly one `target_speaker` in `s_al_bspSpeakers[]` carries a `noise` field
that matches the sfx name, that speaker's BSP origin is used as the fallback.
Multiple speakers sharing the same noise file are not resolved (ambiguous) and
are left for the natural entity-update path.

---

### E. `S_AL_StopAllSounds` reset  ✅ DONE

Three lines added at the end:
```c
Com_Memset(&s_al_mapHints, 0, sizeof(s_al_mapHints));
s_al_numBspSpeakers   = 0;
s_al_normCacheCount   = 0;
s_al_normCacheWritten = qfalse;
```

---

### F. `S_AL_RegisterSound` debug output  ✅ DONE

`Com_DPrintf` line now includes `normGain=%.3f`.
Rate-mismatch `[alDbg]` line now includes `normGain=%.3f`.

---

### G. New functions — map audio init + normcache  ✅ DONE

All of the following functions need to be **inserted between**
`S_AL_StopAllSounds` and `S_AL_ClearLoopingSounds`
(currently around line 2863 in snd_openal.c).

Insert them in this order:

---

#### G.1 `S_AL_MapBaseName`

```c
/* Extract the bare map name from cl.mapname.
 * "maps/ut4_turnpike.bsp" → "ut4_turnpike"
 * Result is written into out[outLen]. */
static void S_AL_MapBaseName( char *out, int outLen )
{
    const char *p   = cl.mapname;
    const char *sl  = strrchr(p, '/');
    const char *dot;
    if (sl) p = sl + 1;
    Q_strncpyz(out, p, outLen);
    dot = strrchr(out, '.');
    if (dot) *(char *)dot = '\0';
}
```

---

#### G.2 `S_AL_ParseMapEntities`

Parse the BSP entity lump (accessible via `CM_EntityString()` once
`CM_NumInlineModels() > 0`) and fill `s_al_mapHints` and `s_al_bspSpeakers`.

```c
static void S_AL_ParseMapEntities( void )
{
    const char *p;
    const char *tok;
    char key[256], val[256];
    char blkClass[64], blkSky[64];
    char blkAmbient[MAX_QPATH], blkMusic[MAX_QPATH], blkNoise[MAX_QPATH];
    char blkOriginStr[128];
    int  blkFlags;

    Com_Memset(&s_al_mapHints, 0, sizeof(s_al_mapHints));
    s_al_numBspSpeakers = 0;
    s_al_mapHints.parsed = qtrue;

    p = CM_EntityString();
    if (!p || !*p) return;

    for (;;) {
        tok = COM_ParseExt(&p, qtrue);
        if (!tok || !tok[0]) break;
        if (tok[0] != '{') continue;

        /* clear per-block state */
        blkClass[0] = blkSky[0] = blkAmbient[0] = '\0';
        blkMusic[0] = blkNoise[0] = blkOriginStr[0] = '\0';
        blkFlags = 0;

        for (;;) {
            tok = COM_ParseExt(&p, qtrue);
            if (!tok || !tok[0] || tok[0] == '}') break;
            Q_strncpyz(key, tok, sizeof(key));
            tok = COM_ParseExt(&p, qtrue);
            if (!tok || !tok[0]) break;
            Q_strncpyz(val, tok, sizeof(val));

            if      (!Q_stricmp(key, "classname"))  Q_strncpyz(blkClass,     val, sizeof(blkClass));
            else if (!Q_stricmp(key, "spawnflags"))  blkFlags = atoi(val);
            else if (!Q_stricmp(key, "sky"))         Q_strncpyz(blkSky,       val, sizeof(blkSky));
            else if (!Q_stricmp(key, "ambient"))     Q_strncpyz(blkAmbient,   val, sizeof(blkAmbient));
            else if (!Q_stricmp(key, "music"))       Q_strncpyz(blkMusic,     val, sizeof(blkMusic));
            else if (!Q_stricmp(key, "noise"))       Q_strncpyz(blkNoise,     val, sizeof(blkNoise));
            else if (!Q_stricmp(key, "origin"))      Q_strncpyz(blkOriginStr, val, sizeof(blkOriginStr));
```

Note: `blkOriginStr` is declared as `char blkOriginStr[96]` in the implementation.
        }

        if (!Q_stricmp(blkClass, "worldspawn")) {
            if (blkSky[0]) {
                Q_strncpyz(s_al_mapHints.skyShader, blkSky, sizeof(s_al_mapHints.skyShader));
                s_al_mapHints.hasSky = qtrue;
            }
            if (blkAmbient[0])
                Q_strncpyz(s_al_mapHints.worldAmbient, blkAmbient, sizeof(s_al_mapHints.worldAmbient));
            if (blkMusic[0])
                Q_strncpyz(s_al_mapHints.worldMusic, blkMusic, sizeof(s_al_mapHints.worldMusic));
        } else if (!Q_stricmp(blkClass, "target_speaker") && blkNoise[0]) {
            if (s_al_numBspSpeakers < S_AL_BSP_SPEAKERS_MAX) {
                alBspSpeaker_t *sp = &s_al_bspSpeakers[s_al_numBspSpeakers++];
                Q_strncpyz(sp->noise, blkNoise, sizeof(sp->noise));
                sp->spawnflags = blkFlags;
                /* Parse "x y z" origin string */
                if (blkOriginStr[0]) {
                    sscanf(blkOriginStr, "%f %f %f",
                           &sp->origin[0], &sp->origin[1], &sp->origin[2]);
                }
            }
            s_al_mapHints.totalSpeakerCount++;
            if (blkFlags & 1) s_al_mapHints.globalSpeakerCount++;
        }
    }

    Com_DPrintf("S_AL: map hints: sky=%s ambient=[%s] music=[%s] "
                "speakers=%d (%d global)\n",
        s_al_mapHints.hasSky ? s_al_mapHints.skyShader : "none",
        s_al_mapHints.worldAmbient, s_al_mapHints.worldMusic,
        s_al_mapHints.totalSpeakerCount, s_al_mapHints.globalSpeakerCount);
}
```

---

#### G.3 `S_AL_PreRegisterMapSounds`

Force-register all sounds referenced in the BSP so their `normGain` is
computed before `S_AL_WriteMapNormCache` runs.

```c
static void S_AL_PreRegisterMapSounds( void )
{
    int i;

    if (s_al_mapHints.worldAmbient[0])
        S_AL_RegisterSound(s_al_mapHints.worldAmbient, qfalse);
    if (s_al_mapHints.worldMusic[0])
        S_AL_RegisterSound(s_al_mapHints.worldMusic, qfalse);

    for (i = 0; i < s_al_numBspSpeakers; i++) {
        if (s_al_bspSpeakers[i].noise[0])
            S_AL_RegisterSound(s_al_bspSpeakers[i].noise, qfalse);
    }
}
```

---

#### G.4 `S_AL_WriteMapNormCache`

Write `normcache/<mapbase>.cfg`.  Called once per map after
`S_AL_PreRegisterMapSounds`.

```c
static void S_AL_WriteMapNormCache( const char *mapbase )
{
    char        path[MAX_QPATH];
    char       *buf;
    int         bufLen, written, i;
    const char *sndPaths[S_AL_BSP_SPEAKERS_MAX + 2];
    int         numPaths = 0;

    if (!mapbase || !mapbase[0]) return;

    /* Collect unique sound paths referenced in the BSP */
    if (s_al_mapHints.worldAmbient[0])
        sndPaths[numPaths++] = s_al_mapHints.worldAmbient;
    if (s_al_mapHints.worldMusic[0])
        sndPaths[numPaths++] = s_al_mapHints.worldMusic;
    for (i = 0; i < s_al_numBspSpeakers; i++) {
        if (s_al_bspSpeakers[i].noise[0])
            sndPaths[numPaths++] = s_al_bspSpeakers[i].noise;
    }
    if (!numPaths) return;   /* nothing to write */

    Com_sprintf(path, sizeof(path), "normcache/%s.cfg", mapbase);

    bufLen  = 512 + numPaths * 96;
    buf     = (char *)Z_Malloc(bufLen);
    written = 0;

    written += Com_sprintf(buf + written, bufLen - written,
        "// Quake3e-urt per-map ambient normalisation cache\n"
        "// map: %s\n"
        "// normGain range 0.05-1.00  (1.0 = natural level, 0.05 = −26 dB cut)\n"
        "// Edit values to tune loudness then type: snd_normcache_rebuild\n"
        "// Delete this file to force regeneration on the next map load.\n",
        cl.mapname[0] ? cl.mapname : mapbase);

    for (i = 0; i < numPaths; i++) {
        alSfxRec_t *r = S_AL_FindSfx(sndPaths[i]);
        float ng = (r && r->inMemory && !r->defaultSound) ? r->normGain : 1.0f;
        written += Com_sprintf(buf + written, bufLen - written,
            "%s %.4f\n", sndPaths[i], ng);
    }

    FS_WriteFile(path, buf, written);
    Z_Free(buf);

    s_al_normCacheWritten = qtrue;
    Com_Printf("S_AL: wrote normcache %s  (%d sounds)\n", path, numPaths);
}
```

---

#### G.5 `S_AL_LoadMapNormCache`

Read `normcache/<mapbase>.cfg`, populate `s_al_normCache[]` for future
`RegisterSound` overrides, and apply immediately to already-registered sfx.

```c
static qboolean S_AL_LoadMapNormCache( const char *mapbase )
{
    char        path[MAX_QPATH];
    char       *buf;
    const char *p;
    const char *tok;
    int         fileLen, applied = 0;

    if (!mapbase || !mapbase[0]) return qfalse;

    Com_sprintf(path, sizeof(path), "normcache/%s.cfg", mapbase);
    fileLen = FS_ReadFile(path, (void **)&buf);
    if (fileLen <= 0 || !buf) return qfalse;

    s_al_normCacheCount = 0;
    p = buf;

    while (*p) {
        char       sfxPath[MAX_QPATH];
        float      ng;
        alSfxRec_t *r;

        tok = COM_ParseExt(&p, qtrue);
        if (!tok || !tok[0]) break;

        /* skip comment lines (COM_ParseExt returns '//' as a single token) */
        if (tok[0] == '/' && tok[1] == '/') {
            while (*p && *p != '\n') p++;
            continue;
        }

        Q_strncpyz(sfxPath, tok, sizeof(sfxPath));

        tok = COM_ParseExt(&p, qfalse);   /* normGain on same line */
        if (!tok || !tok[0]) continue;
        ng = (float)atof(tok);
        if (ng < 0.05f) ng = 0.05f;
        if (ng > 1.0f)  ng = 1.0f;

        /* store in pending table so RegisterSound can pick it up */
        if (s_al_normCacheCount < S_AL_NORMCACHE_MAX) {
            Q_strncpyz(s_al_normCache[s_al_normCacheCount].path,
                       sfxPath, MAX_QPATH);
            s_al_normCache[s_al_normCacheCount].normGain = ng;
            s_al_normCacheCount++;
        }

        /* also apply to already-registered sfx (sounds cached from a
         * previous map session that RegisterSound returned early for) */
        r = S_AL_FindSfx(sfxPath);
        if (r && r->inMemory) {
            r->normGain = ng;
            applied++;
        }
    }

    FS_FreeFile(buf);

    s_al_normCacheWritten = qtrue;   /* don't overwrite user-edited cache */
    Com_Printf("S_AL: loaded normcache %s  (%d entries, %d applied)\n",
               path, s_al_normCacheCount, applied);
    return qtrue;
}
```

---

#### G.6 `S_AL_InitMapAudio`

Called lazily from the snap branch of `S_AL_UpdateDynamicReverb` once
`CM_NumInlineModels() > 0` and `!s_al_mapHints.parsed`.

```c
static void S_AL_InitMapAudio( void )
{
    char mapbase[MAX_QPATH];

    S_AL_ParseMapEntities();   /* fills mapHints + bspSpeakers */

    S_AL_MapBaseName(mapbase, sizeof(mapbase));
    if (!mapbase[0]) return;

    /* Try loading an existing cache first.  If found, it takes precedence
     * (user may have hand-tuned it).  If not found, decode all BSP sounds,
     * compute normGain, and write the cache for next time. */
    if (!S_AL_LoadMapNormCache(mapbase)) {
        S_AL_PreRegisterMapSounds();
        S_AL_WriteMapNormCache(mapbase);
    }
}
```

---

#### G.7 `S_AL_RebuildNormCache_f`

Console command `snd_normcache_rebuild` — regenerates the cache for the
currently loaded map (discards any hand-edits).

```c
static void S_AL_RebuildNormCache_f( void )
{
    char mapbase[MAX_QPATH];

    if (!s_al_started || CM_NumInlineModels() <= 0) {
        Com_Printf("S_AL: no map loaded\n");
        return;
    }

    s_al_normCacheCount   = 0;
    s_al_normCacheWritten = qfalse;

    S_AL_MapBaseName(mapbase, sizeof(mapbase));
    if (!mapbase[0]) { Com_Printf("S_AL: cannot determine map name\n"); return; }

    S_AL_PreRegisterMapSounds();
    S_AL_WriteMapNormCache(mapbase);
}
```

---

### H. `S_AL_BeginRegistration` — load normcache early  ✅ DONE

`BeginRegistration` is called **before** sounds are registered for a new map.
Loading the normcache here means every subsequent `RegisterSound` call
receives the cached `normGain` even for sounds that are already in the sfx
cache from a previous session.

```c
static void S_AL_BeginRegistration( void )
{
    char mapbase[MAX_QPATH];

    s_al_muted = qfalse;

    /* Slot 0 reservation — unchanged */
    S_AL_RegisterSound( "sound/feedback/hit.wav", qfalse );
    Com_DPrintf("S_AL: BeginRegistration -- slot 0 reserved\n");

    /* Load any existing per-map normcache so RegisterSound calls during
     * this registration phase pick up the override values immediately. */
    S_AL_MapBaseName(mapbase, sizeof(mapbase));
    if (mapbase[0])
        S_AL_LoadMapNormCache(mapbase);
}
```

---

### I. `S_AL_UpdateDynamicReverb` — snap branch + `hasSky` hint  ✅ DONE

#### I.1 Snap branch — call `S_AL_InitMapAudio`

In the existing snap block (around line 3300 after changes):

```c
snap = (s_al_loopFrame < lastFrame || curDecay < 0.f || s_al_reverbReset);
if (snap) {
    lastFrame        = s_al_loopFrame - S_AL_ENV_RATE;
    s_al_reverbReset = qfalse;
    histCount  = 0;
    histIdx    = 0;
    haveCache  = qfalse;
    cachedVelSpeed = 0.f;

    /* Parse BSP entities and load/write the per-map normcache the first
     * time we probe after a map load (CM_NumInlineModels() is already > 0
     * so CM_EntityString() is safe to call). */
    if (!s_al_mapHints.parsed)
        S_AL_InitMapAudio();
}
```

#### I.2 `hasSky` classifier hint

After the cave-bonus block, before the open-box block:

```c
/* BSP sky hint: maps with a skybox are outdoor/urban layouts.  A true
 * underground cave will never have a sky shader in worldspawn.  Dampen
 * the cave bonus so that basement sections of sky-lit URT maps don't
 * receive the full stone-tunnel reverb character. */
if (s_al_mapHints.hasSky && caveBonus > 0.f)
    caveBonus *= 0.35f;
```

---

### J. `s_alDebugNorm` — per-frame ambient normGain dump  ✅ DONE

#### J.1 Register the cvar in `S_AL_Init`

After the `s_alDebugPlayback` cvar registration:

```c
s_alDebugNorm = Cvar_Get("s_alDebugNorm", "0", CVAR_TEMP);
Cvar_CheckRange(s_alDebugNorm, "0", "1", CV_INTEGER);
Cvar_SetDescription(s_alDebugNorm,
    "Print normGain for every active ambient loop source once per frame. "
    "Use on a specific map (e.g. ut4_turnpike) to identify which sounds "
    "are too loud and what gain is being applied.  Not archived.");
```

#### J.2 Register `snd_normcache_rebuild` command in `S_AL_Init`

```c
Cmd_AddCommand("snd_normcache_rebuild", S_AL_RebuildNormCache_f);
```

Also add `Cmd_RemoveCommand("snd_normcache_rebuild")` in `S_AL_Shutdown`.

#### J.3 Per-frame output in `S_AL_Update`

After the `S_AL_UpdateDynamicReverb()` call:

```c
/* Debug: print normGain for all active ambient loop sources */
if (s_alDebugNorm && s_alDebugNorm->integer) {
    int j;
    for (j = 0; j < s_al_numSrc; j++) {
        const alSrc_t *src = &s_al_src[j];
        if (src->isPlaying && src->loopSound
                && src->sfx >= 0 && src->sfx < s_al_numSfx) {
            const alSfxRec_t *r = &s_al_sfx[src->sfx];
            Com_Printf("[alNorm] ent=%4d  loop  %-48s  normGain=%.4f\n",
                src->entnum, r->name, r->normGain);
        }
    }
}
```

---

### K. SOUND.md — documentation update  ✅ DONE

Add a new section after the existing **"Key Sound Cvars"** table:

#### Sections to add:

1. **Per-map ambient normalisation cache** — explain the normcache/ directory,
   the one-sided ceiling design, the strided scan fix, how to edit the cache
   manually, and the `snd_normcache_rebuild` command.

2. **BSP entity acoustic hints** — explain what `alMapHints_t` extracts from
   the BSP, what `hasSky` does to the classifier, what `alBspSpeaker_t` stores
   and how it is used for cold-start origin resolution.

3. Update the **debug/diagnostic cvars** table to include `s_alDebugNorm`.

4. Update the **`S_AL_CalcNormGain`** entry under `snd_openal.c` to describe
   the strided scan and one-sided ceiling.

---

### L. BSP speaker occlusion filtering  ✅ DONE

**File:** `code/client/snd_openal.c`

**Problem:** Non-global `target_speaker` loop sources were excluded from the
`S_Update` occlusion pass (`loopSound == qtrue` caused an unconditional
`continue`).  Players heard map ambient sounds (fans, water, machinery) at
full volume and brightness regardless of intervening walls.

**Solution — three coordinated changes:**

#### L.1 `isBspSpeaker` flag in `alSrc_t`

```c
qboolean isBspSpeaker;  /* qtrue: non-global BSP target_speaker loop */
```

Initialised to `qfalse` in `S_AL_SrcSetup`.

#### L.2 Tagging in `S_AL_UpdateLoops`

Immediately after `s_al_src[srcIdx].loopSound = qtrue`, a lookup against
`s_al_bspSpeakers[]` tags the new source:

```c
for (bk = 0; bk < s_al_numBspSpeakers; bk++) {
    if (s_al_bspSpeakers[bk].spawnflags & 1) continue; /* global → skip */
    if (Q_stristr(sfxName, s_al_bspSpeakers[bk].noise) ||
            Q_stristr(s_al_bspSpeakers[bk].noise, sfxName)) {
        s_al_src[srcIdx].isBspSpeaker = qtrue;
        break;
    }
}
```

Global speakers (`spawnflags & 1`) are intentionally excluded — they are
designed to be heard everywhere and should never be attenuated by walls.

#### L.3 Occlusion loop skip guard

Old:
```c
if (!s_al_src[i].isPlaying || s_al_src[i].loopSound) continue;
```

New:
```c
if (!s_al_src[i].isPlaying) continue;
if (s_al_src[i].loopSound && !s_al_src[i].isBspSpeaker) continue;
```

Non-BSP loop sources still skip entirely (they are managed by
`S_AL_UpdateLoops` which owns their `AL_POSITION` and `AL_GAIN`).
BSP speaker loops fall through and receive all three occlusion phases.

#### L.4 Phase guards — don't race with `S_AL_UpdateLoops`

`S_AL_UpdateLoops` runs **after** the occlusion loop and writes the
authoritative `AL_POSITION` and (in non-EFX mode) `AL_GAIN` for every
loop source.  To avoid overwriting those:

- **Phase 1** — `acousticOffset` IIR blend is skipped for `loopSound`
  sources.  The EFX filter (gain + HF muffle) is still updated.
- **Phase 3** — `qalSource3f(AL_POSITION, …)` is skipped for `loopSound`.
- **Phase 3 (no EFX)** — `qalSourcef(AL_GAIN, …)` is skipped for
  `loopSound`; `S_AL_UpdateLoops` applies gain after this loop anyway.

With EFX available the `AL_DIRECT_FILTER` (lowpass gain + HF) is written
by the occlusion pass — that filter slot is not touched by `UpdateLoops`,
so there is no race.

#### Design: no `acousticOffset` for BSP speakers

Map speakers are fixed-position entities.  The gap-finding redirect
(`acousticOffset`) is useful for moving players (sounds "leak" around
corners toward the listener) but is not applied to speakers since their
direction relative to the player is stable and `UpdateLoops` owns the
position write.  Occlusion through walls is achieved purely via the
lowpass gain/HF attenuation — which is the primary perceptual cue.

---

## File locations

| File | Role |
|---|---|
| `code/client/snd_openal.c` | All audio engine changes |
| `normcache/<mapbase>.cfg`  | Per-map normGain cache (written to FS home path) |
| `docs/project/SOUND.md`    | Existing audio reference doc |
| `docs/project/SOUND2.md`   | **This file** — in-progress redesign notes |

---

## Build & test notes

- Build: `make release` from repository root (Linux), or open the VS solution on Windows.
- There is no automated test suite for the audio system.  Manual verification:
  1. Load `ut4_turnpike` in-game with `s_alDebugNorm 1`.  Every active loop
     source should print its normGain.  Traffic sounds should have normGain
     well below 1.0 (the old bug produced ~1.7; the fix should produce ~0.3–0.5
     depending on the actual file RMS).
  2. Check that `normcache/ut4_turnpike.cfg` is created in the home path after
     the first load.
  3. Edit a normGain value in the file, type `snd_normcache_rebuild` → the
     console should print "loaded normcache" and the edited value should be used.
  4. Load a second map — verify `normcache/<mapname>.cfg` is created for it too.
  5. Load ut4_turnpike again — verify "loaded normcache" is printed (not "wrote").
  6. Stand behind a solid wall near a `target_speaker` with `s_alOcclusion 1`.
     Enable `s_alDebugOcc 1` — the BSP speaker source should show `occ < 1.0`
     and the sound should be muffled.  Walk around the corner into line-of-sight
     — the filter should clear and the sound restore within a few frames.

---

## Symbols quick-reference

| Symbol | Type | Status |
|---|---|---|
| `S_AL_NORM_SCAN_SAMPLES` | `#define` | ✅ done |
| `S_AL_NORM_CEILING` | `#define` | ✅ done |
| `S_AL_BSP_SPEAKERS_MAX` | `#define` | ✅ done |
| `S_AL_NORMCACHE_MAX` | `#define` | ✅ done |
| `alBspSpeaker_t` | struct | ✅ done |
| `alMapHints_t` | struct | ✅ done |
| `alNormCacheEntry_t` | struct | ✅ done |
| `alSrc_t.isBspSpeaker` | field | ✅ done |
| `s_al_mapHints` | static | ✅ done |
| `s_al_bspSpeakers[]` | static | ✅ done |
| `s_al_numBspSpeakers` | static | ✅ done |
| `s_al_normCache[]` | static | ✅ done |
| `s_al_normCacheCount` | static | ✅ done |
| `s_al_normCacheWritten` | static | ✅ done |
| `s_alDebugNorm` cvar ptr | static | ✅ done |
| `S_AL_CalcNormGain` | function | ✅ rewritten |
| normcache override in `RegisterSound` | code block | ✅ done |
| BSP origin fallback in `StartSound` | code block | ✅ done |
| `StopAllSounds` reset additions | code block | ✅ done |
| `S_AL_MapBaseName` | function | ✅ done |
| `S_AL_ParseMapEntities` | function | ✅ done |
| `S_AL_PreRegisterMapSounds` | function | ✅ done |
| `S_AL_WriteMapNormCache` | function | ✅ done |
| `S_AL_LoadMapNormCache` | function | ✅ done |
| `S_AL_InitMapAudio` | function | ✅ done |
| `S_AL_RebuildNormCache_f` | function | ✅ done |
| `BeginRegistration` normcache load | code block | ✅ done |
| `UpdateDynamicReverb` snap + hasSky | code block | ✅ done |
| `s_alDebugNorm` cvar registration | code block | ✅ done |
| `snd_normcache_rebuild` cmd | code block | ✅ done |
| per-frame `s_alDebugNorm` output | code block | ✅ done |
| `isBspSpeaker` tagging in `UpdateLoops` | code block | ✅ done |
| occlusion loop BSP speaker guard | code block | ✅ done |
| phase guards (pos / gain / offset) | code block | ✅ done |
| `SOUND.md` updates | doc | ✅ done |
| `SOUND2.md` updates | doc | ✅ done |

---

## Continuing from here

**All items are complete (✅).**  Run `code_review` then `codeql_checker`
to finalise the PR.

