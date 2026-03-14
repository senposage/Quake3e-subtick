/*
===========================================================================
FTWGL: Urban Terror — OpenAL Soft sound backend.
===========================================================================

This module replaces the dmaHD / base-DMA sound path with a clean OpenAL
Soft backend.  Every soundInterface_t function is implemented here; the
OpenAL library is loaded at run-time so that the engine can fall back to
the dmaHD backend transparently when OpenAL Soft is not installed.

Why OpenAL instead of continuing to patch dmaHD
================================================
  • OpenAL guarantees seamless buffer transitions — the HP/LP warm-up
    contamination that caused the audio clicks (PRs #70 #73 #74 #75) is
    structurally impossible: there is no custom filter state to prime.
  • HRTF is provided by OpenAL Soft from measured HRIR datasets (CIPIC /
    MIT KEMAR) without any hand-rolled convolution or resampling code.
  • Cross-platform: WASAPI on Windows, PipeWire/ALSA/PulseAudio on Linux,
    CoreAudio on macOS — all via a single code path.
  • s_doppler is handled natively (AL_VELOCITY + AL_DOPPLER_FACTOR).

Build flag: compile with USE_OPENAL=1 (set in Makefile).
===========================================================================
*/

#ifdef USE_OPENAL

#include "client.h"
#include "snd_codec.h"
#include "snd_local.h"
#include "snd_openal.h"

/* =========================================================================
 * Section 1 — Official OpenAL Soft 1.25.1 headers
 *
 * AL_NO_PROTOTYPES / ALC_NO_PROTOTYPES suppress the inline function
 * declarations so we can load every entry point dynamically at runtime
 * without a link-time dependency on libopenal.  The headers still supply
 * all type definitions, enumeration constants, and LPAL* function-pointer
 * typedefs that we use below.
 * =========================================================================
 */

#define AL_NO_PROTOTYPES
#define ALC_NO_PROTOTYPES
#include "../../libopenal/include/AL/al.h"
#include "../../libopenal/include/AL/alc.h"
#include "../../libopenal/include/AL/alext.h"

/* =========================================================================
 * Section 2 — Dynamic library loading
 * =========================================================================
 */

#ifdef _WIN32
# include <windows.h>
typedef HMODULE al_lib_t;
# define AL_LoadLib(n)    LoadLibraryA(n)
# define AL_UnloadLib(h)  FreeLibrary(h)
# define AL_GetProc(h,n)  GetProcAddress(h, n)
#else
# include <dlfcn.h>
typedef void *al_lib_t;
# define AL_LoadLib(n)    dlopen(n, RTLD_NOW | RTLD_LOCAL)
# define AL_UnloadLib(h)  dlclose(h)
# define AL_GetProc(h,n)  dlsym(h, n)
#endif

static al_lib_t al_handle = NULL;

static const char *al_libnames[] = {
#if defined(_WIN32)
    "OpenAL32.dll",
    "soft_oal.dll",
#elif defined(__APPLE__)
    "/System/Library/Frameworks/OpenAL.framework/OpenAL",
    "libopenal.dylib",
    "libopenal.1.dylib",
#else /* Linux / other Unix */
    "libopenal.so.1",
    "libopenal.so",
#endif
    NULL
};

/* =========================================================================
 * Section 3 — AL / ALC function pointers
 * =========================================================================
 */

static ALvoid    (AL_APIENTRY *qalEnable)(ALenum capability);
static ALvoid    (AL_APIENTRY *qalDisable)(ALenum capability);
static ALboolean (AL_APIENTRY *qalIsEnabled)(ALenum capability);
static const ALchar *(AL_APIENTRY *qalGetString)(ALenum param);
static ALvoid    (AL_APIENTRY *qalGetBooleanv)(ALenum param, ALboolean *values);
static ALvoid    (AL_APIENTRY *qalGetIntegerv)(ALenum param, ALint *values);
static ALvoid    (AL_APIENTRY *qalGetFloatv)(ALenum param, ALfloat *values);
static ALvoid    (AL_APIENTRY *qalGetDoublev)(ALenum param, ALdouble *values);
static ALboolean (AL_APIENTRY *qalGetBoolean)(ALenum param);
static ALint     (AL_APIENTRY *qalGetInteger)(ALenum param);
static ALfloat   (AL_APIENTRY *qalGetFloat)(ALenum param);
static ALdouble  (AL_APIENTRY *qalGetDouble)(ALenum param);
static ALenum    (AL_APIENTRY *qalGetError)(void);
static ALboolean (AL_APIENTRY *qalIsExtensionPresent)(const ALchar *extname);
static ALvoid   *(AL_APIENTRY *qalGetProcAddress)(const ALchar *fname);
static ALenum    (AL_APIENTRY *qalGetEnumValue)(const ALchar *ename);
static ALvoid    (AL_APIENTRY *qalListenerf)(ALenum param, ALfloat value);
static ALvoid    (AL_APIENTRY *qalListener3f)(ALenum param, ALfloat v1, ALfloat v2, ALfloat v3);
static ALvoid    (AL_APIENTRY *qalListenerfv)(ALenum param, const ALfloat *values);
static ALvoid    (AL_APIENTRY *qalListeneri)(ALenum param, ALint value);
static ALvoid    (AL_APIENTRY *qalGetListenerf)(ALenum param, ALfloat *value);
static ALvoid    (AL_APIENTRY *qalGetListenerfv)(ALenum param, ALfloat *values);
static ALvoid    (AL_APIENTRY *qalGenSources)(ALsizei n, ALuint *sources);
static ALvoid    (AL_APIENTRY *qalDeleteSources)(ALsizei n, const ALuint *sources);
static ALboolean (AL_APIENTRY *qalIsSource)(ALuint sid);
static ALvoid    (AL_APIENTRY *qalSourcef)(ALuint sid, ALenum param, ALfloat value);
static ALvoid    (AL_APIENTRY *qalSource3f)(ALuint sid, ALenum param, ALfloat v1, ALfloat v2, ALfloat v3);
static ALvoid    (AL_APIENTRY *qalSourcefv)(ALuint sid, ALenum param, const ALfloat *values);
static ALvoid    (AL_APIENTRY *qalSourcei)(ALuint sid, ALenum param, ALint value);
static ALvoid    (AL_APIENTRY *qalSource3i)(ALuint sid, ALenum param, ALint v1, ALint v2, ALint v3);
static ALvoid    (AL_APIENTRY *qalSourceiv)(ALuint sid, ALenum param, const ALint *values);
static ALvoid    (AL_APIENTRY *qalGetSourcef)(ALuint sid, ALenum param, ALfloat *value);
static ALvoid    (AL_APIENTRY *qalGetSource3f)(ALuint sid, ALenum param, ALfloat *v1, ALfloat *v2, ALfloat *v3);
static ALvoid    (AL_APIENTRY *qalGetSourcefv)(ALuint sid, ALenum param, ALfloat *values);
static ALvoid    (AL_APIENTRY *qalGetSourcei)(ALuint sid, ALenum param, ALint *value);
static ALvoid    (AL_APIENTRY *qalSourcePlay)(ALuint sid);
static ALvoid    (AL_APIENTRY *qalSourceStop)(ALuint sid);
static ALvoid    (AL_APIENTRY *qalSourceRewind)(ALuint sid);
static ALvoid    (AL_APIENTRY *qalSourcePause)(ALuint sid);
static ALvoid    (AL_APIENTRY *qalSourcePlayv)(ALsizei n, const ALuint *sids);
static ALvoid    (AL_APIENTRY *qalSourceStopv)(ALsizei n, const ALuint *sids);
static ALvoid    (AL_APIENTRY *qalSourceQueueBuffers)(ALuint sid, ALsizei numEntries, const ALuint *bids);
static ALvoid    (AL_APIENTRY *qalSourceUnqueueBuffers)(ALuint sid, ALsizei numEntries, ALuint *bids);
static ALvoid    (AL_APIENTRY *qalGenBuffers)(ALsizei n, ALuint *buffers);
static ALvoid    (AL_APIENTRY *qalDeleteBuffers)(ALsizei n, const ALuint *buffers);
static ALboolean (AL_APIENTRY *qalIsBuffer)(ALuint bid);
static ALvoid    (AL_APIENTRY *qalBufferData)(ALuint bid, ALenum format, const ALvoid *data, ALsizei size, ALsizei freq);
static ALvoid    (AL_APIENTRY *qalGetBufferi)(ALuint bid, ALenum param, ALint *value);
static ALvoid    (AL_APIENTRY *qalDistanceModel)(ALenum distanceModel);
static ALvoid    (AL_APIENTRY *qalDopplerFactor)(ALfloat value);
static ALvoid    (AL_APIENTRY *qalSpeedOfSound)(ALfloat value);

/* ALC */
static ALCcontext *(ALC_APIENTRY *qalcCreateContext)(ALCdevice *device, const ALCint *attrlist);
static ALCboolean  (ALC_APIENTRY *qalcMakeContextCurrent)(ALCcontext *context);
static ALCvoid     (ALC_APIENTRY *qalcProcessContext)(ALCcontext *context);
static ALCvoid     (ALC_APIENTRY *qalcSuspendContext)(ALCcontext *context);
static ALCvoid     (ALC_APIENTRY *qalcDestroyContext)(ALCcontext *context);
static ALCcontext *(ALC_APIENTRY *qalcGetCurrentContext)(void);
static ALCdevice  *(ALC_APIENTRY *qalcGetContextsDevice)(ALCcontext *context);
static ALCdevice  *(ALC_APIENTRY *qalcOpenDevice)(const ALCchar *devicename);
static ALCboolean  (ALC_APIENTRY *qalcCloseDevice)(ALCdevice *device);
static ALCenum     (ALC_APIENTRY *qalcGetError)(ALCdevice *device);
static ALCboolean  (ALC_APIENTRY *qalcIsExtensionPresent)(ALCdevice *device, const ALCchar *extname);
static ALCvoid    *(ALC_APIENTRY *qalcGetProcAddress)(ALCdevice *device, const ALCchar *funcname);
static ALCenum     (ALC_APIENTRY *qalcGetEnumValue)(ALCdevice *device, const ALCchar *enumname);
static const ALCchar *(ALC_APIENTRY *qalcGetString)(ALCdevice *device, ALCenum param);
static ALCvoid     (ALC_APIENTRY *qalcGetIntegerv)(ALCdevice *device, ALCenum param, ALCsizei size, ALCint *data);

/* ALC_SOFT_HRTF and ALC_SOFT_output_mode extension functions (optional) */
static LPALCRESETDEVICESOFT  qalcResetDeviceSoft  = NULL;  /* re-configure device */
static LPALCGETSTRINGISOFT   qalcGetStringiSOFT   = NULL;  /* enumerate HRTF datasets */

/* =========================================================================
 * ALC_EXT_EFX function pointers (all optional — graceful no-op if absent)
 * =========================================================================
 */
static LPALGENEFFECTS                 qalGenEffects;
static LPALDELETEEFFECTS              qalDeleteEffects;
static LPALEFFECTI                    qalEffecti;
static LPALEFFECTF                    qalEffectf;
static LPALEFFECTFV                   qalEffectfv;
static LPALGENFILTERS                 qalGenFilters;
static LPALDELETEFILTERS              qalDeleteFilters;
static LPALFILTERI                    qalFilteri;
static LPALFILTERF                    qalFilterf;
static LPALGENAUXILIARYEFFECTSLOTS    qalGenAuxiliaryEffectSlots;
static LPALDELETEAUXILIARYEFFECTSLOTS qalDeleteAuxiliaryEffectSlots;
static LPALAUXILIARYEFFECTSLOTI       qalAuxiliaryEffectSloti;
static LPALAUXILIARYEFFECTSLOTF       qalAuxiliaryEffectSlotf;

/* =========================================================================
 * Section 4 — Internal data structures
 * =========================================================================
 */

#define S_AL_MAX_SFX        4096
#define S_AL_SFX_HASHSIZE   1024
#define S_AL_MAX_SRC        512     /* upper bound for source creation; OpenAL Soft
                                     * caps actual sources to its mix-voice limit
                                     * (default 256, raise via alsoft.ini to serve
                                     * 32-38 players × ~5 sounds + world headroom) */
#define S_AL_MUSIC_BUFFERS  4                   /* ping-pong music stream */
#define S_AL_MUSIC_BUFSZ    (32 * 1024)         /* 32 KiB per streaming buf */
#define S_AL_RAW_BUFFERS    8                   /* raw-samples stream queue */
#define S_AL_RAW_BUFSZ      (16 * 1024)         /* 16 KiB per raw buf */

/* Q3/dmaHD-derived distance constants (replicated from snd_dma.c / snd_dmahd.c):
 *   SOUND_FULLVOLUME = 80  — within this radius gain = 1.0
 *   SOUND_ATTENUATE  = 0.0008 — linear falloff rate beyond full-volume radius
 * Max audible distance = SOUND_FULLVOLUME + 1/SOUND_ATTENUATE = 1330 game units. */
#define S_AL_SOUND_FULLVOLUME   80.0f
#define S_AL_SOUND_MAXDIST      1330.0f   /* 80 + 1/0.0008 */

/* Target RMS level for ambient-sound loudness normalisation.
 * -21 dBFS gives a comfortable background-ambiance level that doesn't
 * drown weapon, footstep, or voice sounds on maps with loud ambient
 * loops (e.g. ut4_turnpike traffic).  Individual sounds are scaled so
 * their measured RMS matches this reference before s_alVolEnv is applied.
 * Only ambient/looping sources use this — one-shot world and local sounds
 * are deliberately left un-normalised so weapon impacts, footstep ticks,
 * etc. retain their designed levels. */
#define S_AL_AMBIENT_TARGET_RMS  0.085f   /* ≈ -21 dBFS (was 0.125 / -18 dBFS) */

/* Looping-ambient fade timings.
 * FADEIN  — gain ramps 0→1 when a loop source first starts (prevents cold-start
 *           pop when you enter a fountain/ambient entity's range).
 * FADEOUT — gain ramps 1→0 before a loop source stops, triggered when the entity
 *           leaves the client snapshot (PVS cull) or is explicitly removed, so the
 *           sound tapers off instead of cutting to silence mid-cycle. */
#define S_AL_LOOP_FADEIN_MS   600   /* ms — ~36 frames at 60 fps (was 150) */
#define S_AL_LOOP_FADEOUT_MS  500   /* ms — ~30 frames at 60 fps (was 120) */

/* CHAN_WEAPON gun-muzzle proximity: sources on the weapon channel within this
 * distance of the listener are treated as "own gun barrel" and bypass occlusion
 * regardless of entity ownership.  This covers muzzle-flash origin offsets
 * (~50–100 u) with margin and prevents PLAYERCLIP slope geometry from muffling
 * the player's own weapon fire even when the sound was emitted as a world sound. */
#define S_AL_WEAPON_NOOCC_DIST  160.0f

/* Dedup window removed — vanilla URT has the same double-start behaviour
 * and nobody notices.  Culling is handled by distance rejection and by the
 * priority-ordered eviction in S_AL_GetFreeSource.
 * s_alDedupMs is registered as a cvar for potential future use; no code
 * currently reads it (value is ignored at runtime). */

/* Per-sound-file data */
typedef struct alSfxRec_s {
    ALuint              buffer;                 /* AL buffer handle */
    char                name[MAX_QPATH];
    int                 soundLength;            /* in samples (file-rate frames) */
    int                 fileRate;               /* original sample rate from the file (Hz) */
    qboolean            defaultSound;           /* buzz on load failure */
    qboolean            inMemory;
    int                 lastTimeUsed;           /* Com_Milliseconds() */
    float               normGain;              /* RMS loudness-normalisation factor for ambient use */
    struct alSfxRec_s  *next;                   /* hash chain */
} alSfxRec_t;

static alSfxRec_t   s_al_sfx[S_AL_MAX_SFX];
static int          s_al_numSfx   = 0;
static alSfxRec_t  *s_al_sfxHash[S_AL_SFX_HASHSIZE];

/* Volume category for per-group gain control */
typedef enum {
    SRC_CAT_WORLD   = 0,  /* other entity / player sounds (default) */
    SRC_CAT_LOCAL   = 1,  /* own player / weapon / breath sounds */
    SRC_CAT_AMBIENT = 2,  /* looping environmental / ambient sounds */
    SRC_CAT_UI      = 3,  /* non-positional local sounds: hit markers, kill
                           * confirmations, menu beeps (entnum == 0 + isLocal).
                           * Separate from SRC_CAT_LOCAL so hit/kill sounds
                           * can be tuned independently of weapon volume. */
} alSrcCat_t;

/* Per-active-sound source */
typedef struct {
    ALuint          source;
    sfxHandle_t     sfx;            /* index into s_al_sfx */
    int             entnum;
    int             entchannel;
    vec3_t          origin;
    qboolean        fixed_origin;
    qboolean        loopSound;
    int             allocTime;
    float           master_vol;     /* raw volume [0..255] before category scale */
    alSrcCat_t      category;       /* volume group for s_alVolSelf/Other/Env */
    qboolean        isPlaying;
    qboolean        isLocal;        /* non-spatialized */
    float           occlusionGain;      /* smoothed [0..1] — 1.0 = unoccluded, applied every frame */
    float           occlusionTarget;    /* raw trace result [0..1] — occlusionGain blends towards this */
    int             occlusionTick;      /* s_al_loopFrame when last traced */
    vec3_t          acousticOffset;     /* displacement from real origin towards gap (×posBlend) */
} alSrc_t;

static alSrc_t  s_al_src[S_AL_MAX_SRC];
static int      s_al_numSrc = 0;    /* number of sources created */

/* Per-entity looping sound */
typedef struct {
    sfxHandle_t sfx;
    int         srcIdx;             /* index into s_al_src (-1 = none) */
    vec3_t      origin;
    vec3_t      velocity;
    qboolean    active;
    qboolean    kill;
    int         framenum;
    float       dopplerScale;
    float       oldDopplerScale;
    int         fadeStartMs;        /* Com_Milliseconds() when fade-in began (0 = not fading in) */
    int         fadeOutStartMs;     /* Com_Milliseconds() when fade-out began (0 = not fading out) */
} alLoop_t;

static alLoop_t s_al_loops[MAX_GENTITIES];
static int      s_al_loopFrame = 0;

/* Background music stream */
typedef struct {
    ALuint          source;
    ALuint          buffers[S_AL_MUSIC_BUFFERS];
    snd_stream_t   *stream;
    char            introFile[MAX_QPATH];
    char            loopFile[MAX_QPATH];
    qboolean        looping;
    qboolean        active;
    qboolean        paused;
} alMusic_t;

static alMusic_t s_al_music;

/* Raw-samples stream (cinematics, VoIP) */
typedef struct {
    ALuint      source;
    ALuint      buffers[S_AL_RAW_BUFFERS];
    int         nextBuf;
    qboolean    active;
} alRaw_t;

static alRaw_t s_al_raw;

/* Global OpenAL context */
static ALCdevice  *s_al_device  = NULL;
static ALCcontext *s_al_context = NULL;
static ALCint      s_al_deviceFreq = 0;    /* device mixing frequency (Hz), queried at init */

static qboolean s_al_started  = qfalse;
static qboolean s_al_muted    = qfalse;
static qboolean s_al_hrtf     = qfalse; /* ALC_HRTF_SOFT active */
static qboolean s_al_inwater  = qfalse;

static vec3_t s_al_listener_origin;   /* updated in S_AL_Respatialize */
static int    s_al_listener_entnum = -1; /* entity number of the listener */

/* Per-entity origin cache — updated by S_AL_UpdateEntityPosition and
 * S_AL_Respatialize.  Used by S_AL_StartSound when origin is NULL so that
 * entity-following sounds start at the right place instead of the world
 * origin.  Indexed by entity number. */
static vec3_t s_al_entity_origins[MAX_GENTITIES];

typedef struct {
    qboolean available;       /* ALC_EXT_EFX present and procs loaded */
    ALuint   reverbEffect;    /* AL_EFFECT_EAXREVERB (or AL_EFFECT_REVERB fallback) */
    ALuint   reverbSlot;      /* auxiliary effect slot for reverb send */
    ALuint   underwaterSlot;  /* auxiliary slot for underwater low-pass */
    ALuint   underwaterEffect;/* AL_EFFECT_EAXREVERB for water acoustics */
    ALuint   occlusionFilter[S_AL_MAX_SRC];
    qboolean hasEAXReverb;    /* AL_EFFECT_EAXREVERB available (vs plain reverb) */
} alEfx_t;

static alEfx_t s_al_efx;

/* Cvars */
static cvar_t *s_alDevice;
static cvar_t *s_alHRTF;
static cvar_t *s_alEFX;          /* 0 = skip EFX init entirely (test/debug) */
static cvar_t *s_alDirectChannels; /* 0 = disable AL_SOFT_direct_channels; active only when s_alHRTF 1 */
static cvar_t *s_alMaxDist;
static cvar_t *s_alReverb;       /* master EFX reverb toggle (0/1) */
static cvar_t *s_alReverbGain;   /* wet-signal slot gain [0..1] */
static cvar_t *s_alReverbDecay;  /* reverb decay time in seconds (LATCH) */
static cvar_t *s_alOcclusion;    /* 0 = off, 1 = gain+direction correction */
static cvar_t *s_alDynamicReverb;/* 0 = static EFX, 1 = ray-traced acoustic env */
static cvar_t *s_alDebugReverb;  /* 0 = off, 1 = params per probe, 2 = + raw probe data */
static cvar_t *s_alVolSelf;      /* own player/weapon volume multiplier [0..1.5] */
static cvar_t *s_alVolOther;     /* other entity/player volume multiplier [0..1.0] */
static cvar_t *s_alVolEnv;       /* looping ambient volume multiplier [0..1.0] */
static cvar_t *s_alVolUI;        /* hit/kill/UI sound multiplier [0..1.0] */
static cvar_t *s_alLocalSelf;    /* force own-entity sounds local even with explicit origin */

/* Acoustic-environment tuning CVars (used by S_AL_UpdateDynamicReverb).
 * All CVAR_ARCHIVE_ND so user tweaks persist across sessions.
 * Change any of these at runtime and type /s_alReset to hear the effect
 * immediately without a map reload or snd_restart. */
static cvar_t *s_alEnvHorizDist;   /* horizontal probe ray max distance */
static cvar_t *s_alEnvVertDist;    /* diagonal-up / straight-up ray max distance */
static cvar_t *s_alEnvDownDist;    /* straight-down ray max distance */
static cvar_t *s_alEnvHorizWeight; /* fraction of openFrac from horizontal rays [0-1] */
static cvar_t *s_alEnvSmoothPole;  /* IIR blend pole (higher = slower transitions) */
static cvar_t *s_alEnvHistorySize; /* rolling history window size [1-S_AL_ENV_HISTORY_MAX] */
static cvar_t *s_alEnvVelThresh;   /* speed (units/probe-cycle) to enable look-ahead */
static cvar_t *s_alEnvVelWeight;   /* max look-ahead ray blend weight [0-0.5] */
static cvar_t *s_alOccWeaponDist;  /* CHAN_WEAPON no-occlusion radius (gun-muzzle zone) */
/* Occlusion filter tuning — live-tunable, use /s_alReset to hear changes. */
static cvar_t *s_alOccGainFloor;   /* floor of AL_LOWPASS_GAIN for fully-blocked sources */
static cvar_t *s_alOccHFFloor;     /* floor of AL_LOWPASS_GAINHF (HF content) through walls */
static cvar_t *s_alOccPosBlend;    /* fraction of HRTF redirect towards nearest gap [0-1] */
static cvar_t *s_alDebugOcc;       /* print per-source occlusion state each frame */
static cvar_t *s_alDebugPlayback;  /* 0=off 1=rate-mismatch+preemption 2=+natural-stop */
static cvar_t *s_alMaxSrc;         /* max source pool size (LATCH) */
static cvar_t *s_alDedupMs;        /* same-frame dedup window in ms (live) */
static cvar_t *s_alTrace;          /* 0=off 1=key lifecycle events 2=verbose */

/* Set to qtrue by the s_alReset command; cleared on the next probe cycle. */
static qboolean s_al_reverbReset = qfalse;

/* =========================================================================
 * Trace-level debug logging.
 *
 * S_AL_TRACE(level, fmt, ...) — prints when s_alTrace >= level.
 *   level 1 : key lifecycle events (source alloc/free, music open/close,
 *             AL errors at critical call sites, init sequence).
 *   level 2 : verbose — every source setup, buffer queue/unqueue, and
 *             intermediate state change.
 *
 * S_AL_CheckALError(where) — queries and logs any pending AL error then
 *   clears the error state.  Only active when s_alTrace >= 1 so the
 *   extra qalGetError() calls are never visible in production builds.
 * =========================================================================
 */
#define S_AL_TRACE(lvl, fmt, ...) \
    do { if (s_alTrace && s_alTrace->integer >= (lvl)) \
        Com_Printf("[altrace] " fmt, ##__VA_ARGS__); } while(0)

static void S_AL_CheckALError( const char *where )
{
    ALenum err;
    if (!s_alTrace || s_alTrace->integer < 1) return;
    err = qalGetError();
    if (err != AL_NO_ERROR)
        Com_Printf("[altrace] ^1AL error 0x%04x at %s\n", (unsigned)err, where);
}

/* AL_SOFT_direct_channels: when HRTF is on (s_alHRTF 1), local (head-locked)
 * sources bypass the HRTF convolution pipeline and route PCM straight to the
 * stereo output.  Without this, even a source at position (0,0,0) relative
 * goes through the HRTF "center" HRIR, which smears the initial transient of
 * weapon sounds (e.g. de.wav).  Only active when s_alHRTF 1 — with HRTF off
 * there is no convolution to bypass, so this has no effect. */
static qboolean s_al_directChannelsExt = qfalse; /* AL_SOFT_direct_channels extension present */
static qboolean s_al_directChannels    = qfalse; /* active: extension present, cvar on, AND s_alHRTF 1 */

/* =========================================================================
 * Section 5 — Library loading helpers
 * =========================================================================
 */

#define AL_LOAD_FN(fn) \
    do { \
        q##fn = (void*)AL_GetProc(al_handle, #fn); \
        if (!q##fn) { \
            Com_Printf("S_AL: missing symbol: %s\n", #fn); \
            return qfalse; \
        } \
    } while(0)

#define ALC_LOAD_FN(fn) \
    do { \
        q##fn = (void*)AL_GetProc(al_handle, #fn); \
        if (!q##fn) { \
            Com_Printf("S_AL: missing symbol: %s\n", #fn); \
            return qfalse; \
        } \
    } while(0)

static qboolean S_AL_LoadLibrary( void )
{
    int i;
    for (i = 0; al_libnames[i]; i++) {
        al_handle = AL_LoadLib(al_libnames[i]);
        if (al_handle) {
            Com_Printf("S_AL: loaded %s\n", al_libnames[i]);
            break;
        }
    }
    if (!al_handle) {
        Com_Printf("S_AL: OpenAL library not found — falling back to dmaHD\n");
        return qfalse;
    }

    /* AL */
    AL_LOAD_FN(alEnable);
    AL_LOAD_FN(alDisable);
    AL_LOAD_FN(alIsEnabled);
    AL_LOAD_FN(alGetString);
    AL_LOAD_FN(alGetBooleanv);
    AL_LOAD_FN(alGetIntegerv);
    AL_LOAD_FN(alGetFloatv);
    AL_LOAD_FN(alGetDoublev);
    AL_LOAD_FN(alGetBoolean);
    AL_LOAD_FN(alGetInteger);
    AL_LOAD_FN(alGetFloat);
    AL_LOAD_FN(alGetDouble);
    AL_LOAD_FN(alGetError);
    AL_LOAD_FN(alIsExtensionPresent);
    AL_LOAD_FN(alGetProcAddress);
    AL_LOAD_FN(alGetEnumValue);
    AL_LOAD_FN(alListenerf);
    AL_LOAD_FN(alListener3f);
    AL_LOAD_FN(alListenerfv);
    AL_LOAD_FN(alListeneri);
    AL_LOAD_FN(alGetListenerf);
    AL_LOAD_FN(alGetListenerfv);
    AL_LOAD_FN(alGenSources);
    AL_LOAD_FN(alDeleteSources);
    AL_LOAD_FN(alIsSource);
    AL_LOAD_FN(alSourcef);
    AL_LOAD_FN(alSource3f);
    AL_LOAD_FN(alSourcefv);
    AL_LOAD_FN(alSourcei);
    AL_LOAD_FN(alSource3i);
    AL_LOAD_FN(alSourceiv);
    AL_LOAD_FN(alGetSourcef);
    AL_LOAD_FN(alGetSource3f);
    AL_LOAD_FN(alGetSourcefv);
    AL_LOAD_FN(alGetSourcei);
    AL_LOAD_FN(alSourcePlay);
    AL_LOAD_FN(alSourceStop);
    AL_LOAD_FN(alSourceRewind);
    AL_LOAD_FN(alSourcePause);
    AL_LOAD_FN(alSourcePlayv);
    AL_LOAD_FN(alSourceStopv);
    AL_LOAD_FN(alSourceQueueBuffers);
    AL_LOAD_FN(alSourceUnqueueBuffers);
    AL_LOAD_FN(alGenBuffers);
    AL_LOAD_FN(alDeleteBuffers);
    AL_LOAD_FN(alIsBuffer);
    AL_LOAD_FN(alBufferData);
    AL_LOAD_FN(alGetBufferi);
    AL_LOAD_FN(alDistanceModel);
    AL_LOAD_FN(alDopplerFactor);
    AL_LOAD_FN(alSpeedOfSound);

    /* ALC */
    ALC_LOAD_FN(alcCreateContext);
    ALC_LOAD_FN(alcMakeContextCurrent);
    ALC_LOAD_FN(alcProcessContext);
    ALC_LOAD_FN(alcSuspendContext);
    ALC_LOAD_FN(alcDestroyContext);
    ALC_LOAD_FN(alcGetCurrentContext);
    ALC_LOAD_FN(alcGetContextsDevice);
    ALC_LOAD_FN(alcOpenDevice);
    ALC_LOAD_FN(alcCloseDevice);
    ALC_LOAD_FN(alcGetError);
    ALC_LOAD_FN(alcIsExtensionPresent);
    ALC_LOAD_FN(alcGetProcAddress);
    ALC_LOAD_FN(alcGetEnumValue);
    ALC_LOAD_FN(alcGetString);
    ALC_LOAD_FN(alcGetIntegerv);

    /* Optional: ALC_SOFT_HRTF and ALC_SOFT_output_mode extensions.
     * These are only present in OpenAL Soft; graceful no-op on other stacks. */
    qalcResetDeviceSoft = (LPALCRESETDEVICESOFT) AL_GetProc(al_handle, "alcResetDeviceSoft");
    qalcGetStringiSOFT  = (LPALCGETSTRINGISOFT)  AL_GetProc(al_handle, "alcGetStringiSOFT");

    return qtrue;
}

static void S_AL_UnloadLibrary( void )
{
    if (al_handle) {
        AL_UnloadLib(al_handle);
        al_handle = NULL;
    }
}

/* =========================================================================
 * Section 6 — Helper utilities
 * =========================================================================
 */

/* Convert PCM format parameters to the matching AL_FORMAT_* constant. */
static ALenum S_AL_Format( int width, int channels )
{
    if (channels == 1)
        return (width == 2) ? AL_FORMAT_MONO16 : AL_FORMAT_MONO8;
    else
        return (width == 2) ? AL_FORMAT_STEREO16 : AL_FORMAT_STEREO8;
}

/* Clear the AL error state and return the previous error code. */
static ALenum S_AL_ClearError( const char *caller )
{
    ALenum err = qalGetError();
    if (err != AL_NO_ERROR && caller)
        Com_DPrintf("S_AL: %s: previous error 0x%x cleared\n", caller, err);
    return err;
}

/* Log and return the current AL error (non-zero = problem). */
static ALenum S_AL_CheckError( const char *caller )
{
    ALenum err = qalGetError();
    if (err != AL_NO_ERROR)
        Com_Printf("S_AL: %s: error 0x%x\n", caller, err);
    return err;
}

/* =========================================================================
 * Section 7 — Sound-file (sfx) management
 * =========================================================================
 */

static unsigned int S_AL_SfxHash( const char *name )
{
    unsigned int h = 0;
    while (*name)
        h = h * 31 + (unsigned char)*name++;
    return h & (S_AL_SFX_HASHSIZE - 1);
}

/* Find an existing sfx record by name, or NULL. */
static alSfxRec_t *S_AL_FindSfx( const char *name )
{
    unsigned int h = S_AL_SfxHash(name);
    alSfxRec_t *r;
    for (r = s_al_sfxHash[h]; r; r = r->next)
        if (!Q_stricmp(r->name, name))
            return r;
    return NULL;
}

/* Allocate a new sfx slot (LRU eviction when full). */
static alSfxRec_t *S_AL_AllocSfx( void )
{
    int         i;
    alSfxRec_t *r;

    /* Try an empty slot first. */
    for (i = 0; i < s_al_numSfx; i++) {
        if (!s_al_sfx[i].inMemory && !s_al_sfx[i].name[0])
            return &s_al_sfx[i];
    }

    /* Grow the array if space remains. */
    if (s_al_numSfx < S_AL_MAX_SFX)
        return &s_al_sfx[s_al_numSfx++];

    /* Evict the least-recently-used loaded sfx. */
    {
        int oldest = Com_Milliseconds();
        int used   = -1;
        for (i = 0; i < s_al_numSfx; i++) {
            if (s_al_sfx[i].inMemory && s_al_sfx[i].lastTimeUsed < oldest) {
                oldest = s_al_sfx[i].lastTimeUsed;
                used   = i;
            }
        }
        if (used < 0)
            return NULL;    /* completely full — should never happen */
        r = &s_al_sfx[used];
        /* Remove from hash chain */
        {
            unsigned int h = S_AL_SfxHash(r->name);
            alSfxRec_t **prev = &s_al_sfxHash[h];
            while (*prev && *prev != r) prev = &(*prev)->next;
            if (*prev) *prev = r->next;
        }
        if (r->buffer) {
            qalDeleteBuffers(1, &r->buffer);
            r->buffer = 0;
        }
        Com_Memset(r, 0, sizeof(*r));
        return r;
    }
}

/* Compute a per-sample RMS loudness-normalisation gain for ambient sources.
 *
 * Scans the decoded PCM data (up to S_AL_NORM_SCAN_SAMPLES per channel),
 * calculates the root-mean-square level, and returns the gain multiplier
 * required to bring that level to S_AL_AMBIENT_TARGET_RMS.
 *
 * Returns 1.0 (no adjustment) for:
 *   • empty or silent files
 *   • unsupported bit depths
 *
 * The result is clamped to [0.25, 4.0] (±12 dB) so very quiet or very loud
 * outliers are still audible / not ear-splitting after normalization. */
#define S_AL_NORM_SCAN_SAMPLES  (44100 * 4)   /* at most 4 s worth of samples */

static float S_AL_CalcNormGain( const void *pcm, const snd_info_t *info )
{
    double       sumSq = 0.0;
    long         count, i;
    float        rms, normGain;
    long         maxSamples;

    if (!pcm || info->samples <= 0 || info->size <= 0)
        return 1.0f;

    /* Cap scan length to keep load times short on very long ambient loops. */
    maxSamples = info->samples;
    if (maxSamples > S_AL_NORM_SCAN_SAMPLES)
        maxSamples = S_AL_NORM_SCAN_SAMPLES;

    if (info->width == 2) {
        /* 16-bit signed PCM */
        const short *s = (const short *)pcm;
        long n = maxSamples * info->channels;
        /* guard against size underflow */
        if (n > (long)(info->size / 2))
            n = (long)(info->size / 2);
        for (i = 0; i < n; i++) {
            double v = s[i] / 32768.0;
            sumSq += v * v;
        }
        count = n;
    } else if (info->width == 1) {
        /* 8-bit unsigned PCM (centre = 128) */
        const byte *s = (const byte *)pcm;
        long n = maxSamples * info->channels;
        if (n > (long)info->size)
            n = (long)info->size;
        for (i = 0; i < n; i++) {
            double v = (s[i] - 128) / 128.0;
            sumSq += v * v;
        }
        count = n;
    } else {
        return 1.0f;  /* unsupported depth */
    }

    if (count == 0) return 1.0f;

    rms = (float)sqrt(sumSq / (double)count);
    if (rms < 1e-6f) return 1.0f;   /* silent — don't amplify noise floor */

    normGain = S_AL_AMBIENT_TARGET_RMS / rms;
    /* Clamp: boost quiet sounds up to 4× (+12 dB), cut loud sounds by up to
     * 10× (−20 dB).  The wider reduction range prevents very loud ambient WAVs
     * (e.g. ut4_turnpike traffic) from overwhelming other sounds even after the
     * lower TARGET_RMS ceiling is applied. */
    if (normGain > 4.0f)   normGain = 4.0f;
    if (normGain < 0.10f)  normGain = 0.10f;

    return normGain;
}

/* Load a sound file into an AL buffer.  Returns the sfxHandle_t index
 * (>= 0) on success, 0 (the default-sound slot) on failure. */
static sfxHandle_t S_AL_RegisterSound( const char *sample, qboolean compressed )
{
    alSfxRec_t  *r;
    snd_info_t   info;
    void        *pcm;
    ALenum       fmt;
    unsigned int h;
    int          idx;

    if (!sample || !*sample)
        return 0;

    /* Look up in registry. */
    r = S_AL_FindSfx(sample);
    if (r) {
        r->lastTimeUsed = Com_Milliseconds();
        return (sfxHandle_t)(r - s_al_sfx);
    }

    /* Allocate new record. */
    r = S_AL_AllocSfx();
    if (!r)
        return 0;

    Q_strncpyz(r->name, sample, sizeof(r->name));
    h          = S_AL_SfxHash(sample);
    r->next    = s_al_sfxHash[h];
    s_al_sfxHash[h] = r;

    /* Decode via codec. */
    pcm = S_CodecLoad(sample, &info);
    if (!pcm) {
        Com_Printf(S_COLOR_YELLOW "S_AL: couldn't load %s\n", sample);
        r->defaultSound = qtrue;
        r->inMemory     = qtrue;
        r->normGain     = 1.0f;
        r->lastTimeUsed = Com_Milliseconds();
        return (sfxHandle_t)(r - s_al_sfx);
    }

    fmt = S_AL_Format(info.width, info.channels);

    /* B-format Ambisonics: 4-channel files (W+X+Y+Z) loaded as BFORMAT3D */
    if (info.channels == 4) {
        fmt = (info.width == 2) ? AL_FORMAT_BFORMAT3D_16 : AL_FORMAT_BFORMAT3D_8;
    }

    /* Analyse PCM while it is still in the temp allocation. */
    r->normGain = S_AL_CalcNormGain(pcm, &info);

    S_AL_ClearError("RegisterSound");
    qalGenBuffers(1, &r->buffer);
    if (S_AL_CheckError("alGenBuffers")) {
        Hunk_FreeTempMemory(pcm);
        return 0;
    }

    qalBufferData(r->buffer, fmt, pcm, info.size, info.rate);
    Hunk_FreeTempMemory(pcm);

    if (S_AL_CheckError("alBufferData")) {
        qalDeleteBuffers(1, &r->buffer);
        r->buffer = 0;
        r->defaultSound = qtrue;
    }

    r->soundLength  = info.samples;
    r->fileRate     = info.rate;
    r->inMemory     = qtrue;
    r->lastTimeUsed = Com_Milliseconds();

    idx = (int)(r - s_al_sfx);
    Com_DPrintf("S_AL: loaded %s (%d Hz, %d ch, %d smp)\n",
        sample, info.rate, info.channels, info.samples);

    /* Playback debug: warn when the file's sample rate doesn't match the
     * device's mixing rate — this is the condition that forces the internal
     * resampler to run and is the root cause of the Gaussian-filter quality
     * loss described in kcat/openal-soft#985. */
    if (s_alDebugPlayback && s_alDebugPlayback->integer >= 1 &&
            s_al_deviceFreq > 0 && info.rate != (int)s_al_deviceFreq) {
        Com_Printf(S_COLOR_YELLOW
            "[alDbg] rate mismatch: %s  file=%d Hz  device=%d Hz"
            "  — resampler will run\n",
            sample, info.rate, (int)s_al_deviceFreq);
    }

    return (sfxHandle_t)idx;
}

/* Free all loaded sfx. */
static void S_AL_FreeSfx( void )
{
    int i;
    for (i = 0; i < s_al_numSfx; i++) {
        if (s_al_sfx[i].buffer) {
            qalDeleteBuffers(1, &s_al_sfx[i].buffer);
            s_al_sfx[i].buffer = 0;
        }
    }
    Com_Memset(s_al_sfx,     0, sizeof(s_al_sfx));
    Com_Memset(s_al_sfxHash, 0, sizeof(s_al_sfxHash));
    s_al_numSfx = 0;
}

/* =========================================================================
 * Section 8 — Source pool management
 * =========================================================================
 */

/* Full-scale value for master_vol: source gain = (master_vol/255)*catVol,
 * listener gain = s_volume.  Combined: s_volume * catVol (no double-apply). */
#define S_AL_MASTER_VOL_FULL 255.0f

/* Find an existing source for (entnum, entchannel), or -1. */
static float S_AL_GetMasterVol( void )
{
    /* Return full-scale so that source gain = catVol (the listener AL_GAIN
     * is set to s_volume every frame — applying it here too would double it). */
    return S_AL_MASTER_VOL_FULL;
}

/* Return the per-category volume scalar for a source, clamped to its
 * allowed range.  Anti-cheat: s_alVolOther and s_alVolEnv are capped at
 * 1.0 (can only be turned DOWN); s_alVolSelf allows a modest boost (1.5). */
static float S_AL_GetCategoryVol( alSrcCat_t cat )
{
    float v;
    switch (cat) {
    case SRC_CAT_LOCAL:
        v = s_alVolSelf ? s_alVolSelf->value : 1.0f;
        if (v < 0.0f) v = 0.0f;
        if (v > 1.5f) v = 1.5f;
        return v;
    case SRC_CAT_UI:
        v = s_alVolUI ? s_alVolUI->value : 0.8f;
        if (v < 0.0f) v = 0.0f;
        if (v > 1.0f) v = 1.0f;
        return v;
    case SRC_CAT_AMBIENT:
        v = s_alVolEnv ? s_alVolEnv->value : 0.3f;
        if (v < 0.0f) v = 0.0f;
        if (v > 1.0f) v = 1.0f;
        return v;
    case SRC_CAT_WORLD:
    default:
        v = s_alVolOther ? s_alVolOther->value : 0.7f;
        if (v < 0.0f) v = 0.0f;
        if (v > 1.0f) v = 1.0f;
        return v;
    }
}

/* Find a free source slot.  Before stealing anything, tries to grow the pool
 * by asking OpenAL for a new source — slots are cheap on modern hardware.
 *
 * When the pool is genuinely full, eviction priority is:
 *
 *   Tier 1 — ENTITYNUM_WORLD (impacts, brass, debris): farthest first.
 *            Missing a casing 2000 u away is fine; dropping a player shot
 *            never is.
 *   Tier 2 — Other-player / entity non-local 3D sources: farthest first.
 *            Distant sounds have already faded; nearby teammates stay alive.
 *   Tier 3 — Any non-local non-loop source: oldest first.
 *   Tier 4 — Any non-loop source (own-player included): oldest first.
 *            Last resort — own-player sounds are almost never here.
 *
 * Loop (ambient) sources are never stolen; they have their own management.
 * Returns source index, or -1 only on a true hard AL failure. */
static int S_AL_GetFreeSource( void )
{
    int   i;
    int   maxSrc;

    /* ---- Pass 1: free slot -------------------------------------------- */
    for (i = 0; i < s_al_numSrc; i++) {
        if (!s_al_src[i].isPlaying)
            return i;
    }

    /* ---- Pass 2: grow the pool ----------------------------------------- */
    maxSrc = (s_alMaxSrc && s_alMaxSrc->integer > 0)
             ? s_alMaxSrc->integer : S_AL_MAX_SRC;
    if (maxSrc > S_AL_MAX_SRC) maxSrc = S_AL_MAX_SRC;

    if (s_al_numSrc < maxSrc) {
        ALuint newSource = 0;
        qalGetError();
        qalGenSources(1, &newSource);
        if (qalGetError() == AL_NO_ERROR) {
            i = s_al_numSrc;
            s_al_src[i].source  = newSource;
            s_al_src[i].sfx     = -1;
            /* The EFX filter for this slot was pre-allocated at init —
             * initialise it as pass-through for the first use. */
            if (s_al_efx.available && s_al_efx.occlusionFilter[i]) {
                qalFilteri(s_al_efx.occlusionFilter[i],
                           AL_FILTER_TYPE, AL_FILTER_LOWPASS);
                qalFilterf(s_al_efx.occlusionFilter[i],
                           AL_LOWPASS_GAIN,   1.0f);
                qalFilterf(s_al_efx.occlusionFilter[i],
                           AL_LOWPASS_GAINHF, 1.0f);
            }
            s_al_numSrc++;
            Com_DPrintf("S_AL: pool grew to %d sources\n", s_al_numSrc);
            return i;
        }
    }

    /* ---- Pass 3: steal by priority ------------------------------------- */
    {
        int   now          = Com_Milliseconds();
        /* Tier 1 — world-entity, farthest */
        float worldFarthest = -1.f;
        int   worldIdx      = -1;
        /* Tier 2 — other-entity non-local 3D, farthest */
        float otherFarthest = -1.f;
        int   otherIdx      = -1;
        /* Tier 3 — any non-local, oldest */
        int   nonLocalOldest = now;
        int   nonLocalIdx    = -1;
        /* Tier 4 — any non-loop, oldest (includes own-player as last resort) */
        int   anyOldest      = now;
        int   anyIdx         = -1;

        for (i = 0; i < s_al_numSrc; i++) {
            alSrc_t *src = &s_al_src[i];
            if (!src->isPlaying || src->loopSound) continue;

            /* Tier 4 baseline: oldest of everything */
            if (src->allocTime < anyOldest) {
                anyOldest = src->allocTime;
                anyIdx    = i;
            }

            if (src->isLocal) continue; /* own-player: skip tiers 1-3 */

            /* Tier 3: oldest non-local */
            if (src->allocTime < nonLocalOldest) {
                nonLocalOldest = src->allocTime;
                nonLocalIdx    = i;
            }

            /* Distance for tiers 1 & 2 */
            {
                vec3_t  d;
                float   distSq;
                VectorSubtract(src->origin, s_al_listener_origin, d);
                distSq = DotProduct(d, d);

                if (src->entnum == ENTITYNUM_WORLD) {
                    if (distSq > worldFarthest) {
                        worldFarthest = distSq;
                        worldIdx      = i;
                    }
                } else {
                    if (distSq > otherFarthest) {
                        otherFarthest = distSq;
                        otherIdx      = i;
                    }
                }
            }
        }

        i = (worldIdx    >= 0) ? worldIdx    :
            (otherIdx    >= 0) ? otherIdx    :
            (nonLocalIdx >= 0) ? nonLocalIdx : anyIdx;

        if (i >= 0) {
            S_AL_TRACE(2, "GetFreeSource: stealing slot %d (sfx %d) for new sound\n",
                       i, s_al_src[i].sfx);
            qalSourceStop(s_al_src[i].source);
            qalSourcei(s_al_src[i].source, AL_BUFFER, 0);
            s_al_src[i].isPlaying = qfalse;
            return i;
        }
    }

    Com_DPrintf(S_COLOR_YELLOW
        "S_AL_GetFreeSource: pool exhausted (all %d sources are looping ambients)\n",
        s_al_numSrc);
    return -1; /* all sources are looping ambients — should never happen */
}

/* Compute occlusion gain and find the best apparent source position for HRTF
 * directional accuracy when the direct path is blocked.
 *
 * Algorithm (first-order diffraction approximation):
 *   1. Cast one direct ray (listener → srcOrigin).
 *      Clear LOS  → acousticPos = srcOrigin, return 1.0.
 *   2. Blocked → build an orthonormal tangent frame perpendicular to the
 *      listener→source axis and cast up to 8 probe rays displaced
 *      ±SEARCH_RADIUS in that plane (cardinal + diagonal directions).
 *      Pick the probe with the highest fraction (nearest audible gap).
 *      Use that probe position as acousticPos so HRTF places the sound
 *      from the gap direction rather than through the wall.
 *   3. Fall back to srcOrigin if no better probe is found.
 *
 * Total traces: 1 (direct) + up to 8 (probes, early-break on clear path).
 * Called every 4 frames per playing non-local source.
 *
 * Returns gain [0.1..1.0].  acousticPos receives the virtual AL_POSITION. */
static float S_AL_ComputeAcousticPosition( const vec3_t srcOrigin, vec3_t acousticPos )
{
    trace_t tr;
    float   directFrac;
    float   bestFrac = 0.f; /* hoisted; assigned from directFrac before the probe loop */

    VectorCopy(srcOrigin, acousticPos);

    /* Guard: CM_BoxTrace with handle 0 will crash if no map is loaded. */
    if (CM_NumInlineModels() <= 0)
        return 1.0f;

    CM_BoxTrace(&tr, s_al_listener_origin, srcOrigin,
                vec3_origin, vec3_origin,
                0, CONTENTS_SOLID, qfalse);

    directFrac = tr.fraction;
    if (directFrac >= 1.0f)
        return 1.0f;   /* clear LOS — use the real position as-is */

    bestFrac = directFrac;

    /* Occluded: search for the nearest gap in 8 tangent-plane directions. */
    {
        /* (right-scale, up-scale) pairs — cardinal then diagonal */
        static const float dirs[8][2] = {
            { 1.f,  0.f}, {-1.f,  0.f}, { 0.f,  1.f}, { 0.f, -1.f},
            { 1.f,  1.f}, { 1.f, -1.f}, {-1.f,  1.f}, {-1.f, -1.f}
        };
        /* ~half a doorway width; large enough to reach around thin walls,
         * small enough not to sample through unrelated geometry */
        static const float SEARCH_RADIUS = 80.0f;

        vec3_t fwd, right, upv;
        int    k;

        /* Orthonormal frame: fwd = listener→source, right/up = tangent plane */
        VectorSubtract(srcOrigin, s_al_listener_origin, fwd);
        VectorNormalize(fwd);

        if (fabsf(fwd[2]) < 0.9f) {
            vec3_t worldUp = {0.f, 0.f, 1.f};
            CrossProduct(fwd, worldUp, right);
        } else {
            vec3_t worldFwd = {1.f, 0.f, 0.f};
            CrossProduct(fwd, worldFwd, right);
        }
        VectorNormalize(right);
        CrossProduct(right, fwd, upv);
        VectorNormalize(upv);

        for (k = 0; k < 8; k++) {
            trace_t pr;
            vec3_t  probe;

            VectorMA(srcOrigin, dirs[k][0] * SEARCH_RADIUS, right, probe);
            VectorMA(probe,     dirs[k][1] * SEARCH_RADIUS, upv,   probe);

            CM_BoxTrace(&pr, s_al_listener_origin, probe,
                        vec3_origin, vec3_origin,
                        0, CONTENTS_SOLID, qfalse);

            if (pr.fraction > bestFrac) {
                bestFrac = pr.fraction;
                VectorCopy(probe, acousticPos);
                if (bestFrac >= 1.0f)
                    break;   /* clear path found — stop searching */
            }
        }
    }

    return 0.1f + 0.9f * bestFrac;
}

/* Configure a source for 3D or local playback. */
static void S_AL_SrcSetup( int idx, sfxHandle_t sfx,
                            const vec3_t origin, qboolean fixed_origin,
                            int entnum, int entchannel,
                            float vol, qboolean isLocal, qboolean isAmbient )
{
    alSfxRec_t *r   = &s_al_sfx[sfx];
    alSrc_t    *src  = &s_al_src[idx];
    ALuint      sid  = src->source;
    float       catVol;

    src->sfx          = sfx;
    src->entnum       = entnum;
    src->entchannel   = entchannel;
    src->master_vol   = vol;
    src->isLocal      = isLocal;
    src->loopSound    = qfalse;
    src->allocTime    = Com_Milliseconds();
    src->isPlaying    = qtrue;
    src->occlusionGain   = 1.0f;
    src->occlusionTarget = 1.0f;
    src->occlusionTick   = 0;
    VectorClear(src->acousticOffset);

    /* Classify into volume category.
     * isLocal + entnum==0 means StartLocalSound (hit markers, kill confirmations,
     * menu audio) — give these their own SRC_CAT_UI knob so they can be tuned
     * independently of own-weapon / breath sounds (SRC_CAT_LOCAL).
     * Own-entity sounds (entnum == s_al_listener_entnum) are SRC_CAT_LOCAL even
     * when s_alLocalSelf=0 (3D spatialized mode) so that s_alVolSelf always
     * controls own-player audio regardless of the spatialization setting.
     * Ambient (loop) sources are classified before the own-entity check so that
     * a loop entity that happens to share the listener's entity number is still
     * correctly routed to SRC_CAT_AMBIENT → s_alVolEnv. */
    if (isLocal && entnum == 0)
        src->category = SRC_CAT_UI;
    else if (isLocal)
        src->category = SRC_CAT_LOCAL;
    else if (isAmbient)
        src->category = SRC_CAT_AMBIENT;
    else if (s_al_listener_entnum >= 0 && entnum == s_al_listener_entnum)
        src->category = SRC_CAT_LOCAL;   /* own entity, 3D mode */
    else
        src->category = SRC_CAT_WORLD;

    catVol = S_AL_GetCategoryVol(src->category);
    /* Ambient sources additionally apply per-sample RMS normalisation so
     * that different map environments have consistent perceived loudness
     * without the player having to adjust s_alVolEnv every map. */
    if (src->category == SRC_CAT_AMBIENT)
        catVol *= r->normGain;

    if (origin)
        VectorCopy(origin, src->origin);
    else
        VectorClear(src->origin);
    src->fixed_origin = fixed_origin;

    qalSourcei(sid,  AL_BUFFER,   r->buffer);
    qalSourcef(sid,  AL_GAIN,     (vol / 255.0f) * catVol);
    qalSourcei(sid,  AL_LOOPING,  AL_FALSE);

    if (isLocal) {
        qalSourcei(sid, AL_SOURCE_RELATIVE, AL_TRUE);
        qalSource3f(sid, AL_POSITION, 0.0f, 0.0f, 0.0f);
        qalSourcef(sid, AL_ROLLOFF_FACTOR, 0.0f);
        /* Explicitly clear any direct filter left from a previous 3-D use of
         * this source slot.  A stale low-pass (e.g. from a fully-occluded
         * world sound) would otherwise muffle own-player / weapon sounds for
         * the first few frames until the occlusion update tick resets it —
         * the sporadic "suppressor" effect heard on weapons like the DE-50. */
        if (s_al_efx.available) {
            qalSourcei(sid, AL_DIRECT_FILTER, (ALint)AL_FILTER_NULL);
            /* Also disconnect any stale reverb auxiliary send.  Without this,
             * a local source that reuses a slot previously used for a 3D
             * reverb-enabled sound inherits the reverb routing, adding an
             * unwanted wet tail to own-player weapon and footstep sounds. */
            qalSource3i(sid, AL_AUXILIARY_SEND_FILTER,
                        (ALint)AL_EFFECTSLOT_NULL, 0, (ALint)AL_FILTER_NULL);
            if (s_alDebugReverb && s_alDebugReverb->integer >= 2)
                Com_Printf(S_COLOR_CYAN "[alfilter] src#%d local: cleared stale filter\n", idx);
        }
        /* Bypass the HRTF convolution for head-locked (own-player) sounds
         * when HRTF is active (s_alHRTF 1).  Even at AL_POSITION (0,0,0)
         * relative, OpenAL Soft runs the signal through the "center" HRIR
         * when HRTF is enabled.  That convolution smears the initial transient
         * of weapon sounds.  AL_DIRECT_CHANNELS_SOFT routes the PCM directly
         * to the stereo output mix, bypassing the HRTF kernel.
         * s_al_directChannels is false when s_alHRTF 0 (default), so this
         * call is skipped unless the user has explicitly enabled HRTF. */
        if (s_al_directChannels)
            qalSourcei(sid, AL_DIRECT_CHANNELS_SOFT, AL_TRUE);
    } else {
        float maxDist = s_alMaxDist ? s_alMaxDist->value : S_AL_SOUND_MAXDIST;
        if (maxDist < S_AL_SOUND_MAXDIST) maxDist = S_AL_SOUND_MAXDIST;
        qalSourcei(sid, AL_SOURCE_RELATIVE, AL_FALSE);
        qalSource3f(sid, AL_POSITION, src->origin[0], src->origin[1], src->origin[2]);
        qalSourcef(sid, AL_ROLLOFF_FACTOR,      1.0f);
        qalSourcef(sid, AL_REFERENCE_DISTANCE,  S_AL_SOUND_FULLVOLUME);
        qalSourcef(sid, AL_MAX_DISTANCE,        maxDist);
        /* If this slot was previously used as a local source, it may have
         * AL_DIRECT_CHANNELS_SOFT = AL_TRUE left over.  A 3D source must
         * go through the HRTF/spatialization pipeline, so clear it now. */
        if (s_al_directChannels)
            qalSourcei(sid, AL_DIRECT_CHANNELS_SOFT, AL_FALSE);
        if (s_al_efx.available) {
            /* Reverb send (auxiliary send 0) — skip when reverb is disabled */
            if (s_alReverb && s_alReverb->integer) {
                qalSource3i(sid, AL_AUXILIARY_SEND_FILTER,
                            (ALint)s_al_efx.reverbSlot, 0, (ALint)AL_FILTER_NULL);
            } else {
                qalSource3i(sid, AL_AUXILIARY_SEND_FILTER,
                            (ALint)AL_EFFECTSLOT_NULL, 0, (ALint)AL_FILTER_NULL);
            }
            /* Pre-set the occlusion filter to pass-through values so the
             * filter object is ready for the first occlusion update tick.
             * Start the source with AL_FILTER_NULL (complete bypass) rather
             * than the filter object: even a biquad at GAIN=1/GAINHF=1
             * introduces group-delay coloration on the attack transient of
             * weapon sounds (de.wav) heard from other players, producing a
             * subtle "wet blanket" quality.  The occlusion update will swap
             * in the filter object only when actual attenuation is needed. */
            if (s_alDebugReverb && s_alDebugReverb->integer >= 2
                    && src->occlusionGain < 0.99f) {
                Com_Printf(S_COLOR_CYAN "[alfilter] src#%d 3D: reset stale filter"
                           " (prev occlusionGain=%.2f)\n",
                           idx, src->occlusionGain);
            }
            qalFilterf(s_al_efx.occlusionFilter[idx], AL_LOWPASS_GAIN,   1.0f);
            qalFilterf(s_al_efx.occlusionFilter[idx], AL_LOWPASS_GAINHF, 1.0f);
            qalSourcei(sid, AL_DIRECT_FILTER, (ALint)AL_FILTER_NULL);
            qalSourcei(sid, AL_AUXILIARY_SEND_FILTER_GAIN_AUTO,   AL_TRUE);
            qalSourcei(sid, AL_AUXILIARY_SEND_FILTER_GAINHF_AUTO, AL_TRUE);
            qalSourcei(sid, AL_DIRECT_FILTER_GAINHF_AUTO,         AL_FALSE);
        }
    }
}

/* Stop a source and mark it free.
 *
 * preempted: qtrue when a new sound on the same channel is cutting this one
 *   short; qfalse when called from StopAllSounds / ClearSoundBuffer.
 *   When s_alDebugPlayback >= 1, log how much of the buffer was consumed so
 *   it is visible whether the DE boom is being truncated by a later sound. */
static void S_AL_StopSource( int idx )
{
    if (!s_al_src[idx].isPlaying)
        return;

    if (s_alDebugPlayback && s_alDebugPlayback->integer >= 1) {
        alSrc_t    *src = &s_al_src[idx];
        ALint       offset = 0;
        const char *name   = "(unknown)";
        int         total  = 0;
        int         fileHz = 0;

        qalGetSourcei(src->source, AL_SAMPLE_OFFSET, &offset);

        if (src->sfx >= 0 && src->sfx < s_al_numSfx) {
            name   = s_al_sfx[src->sfx].name;
            total  = s_al_sfx[src->sfx].soundLength;
            fileHz = s_al_sfx[src->sfx].fileRate;
        }

        Com_Printf(S_COLOR_CYAN
            "[alDbg] stop  %-40s  played %d / %d smp (%.0f%%)  "
            "file=%d Hz  device=%d Hz\n",
            name, offset, total,
            (total > 0) ? (100.0f * offset / total) : 0.0f,
            fileHz, (int)s_al_deviceFreq);
    }

    qalSourceStop(s_al_src[idx].source);
    qalSourcei(s_al_src[idx].source, AL_LOOPING, AL_FALSE);
    qalSourcei(s_al_src[idx].source, AL_BUFFER, 0);
    s_al_src[idx].isPlaying  = qfalse;
    s_al_src[idx].loopSound  = qfalse;
}

/* =========================================================================
 * Section 9 — Background-music streaming
 * =========================================================================
 */

#define MUSIC_RATE  44100

static void S_AL_MusicStop( void )
{
    ALint  queued;
    ALuint buf;

    S_AL_TRACE(2, "MusicStop: active=%d source=%u stream=%p\n",
               s_al_music.active, s_al_music.source,
               (void *)s_al_music.stream);

    /* If no valid music source was created (e.g. voice pool was exhausted
     * at init), only clean up the codec stream and return. */
    if (!s_al_music.source) {
        if (s_al_music.stream) {
            S_CodecCloseStream(s_al_music.stream);
            s_al_music.stream = NULL;
        }
        s_al_music.active = qfalse;
        return;
    }

    /* Stop the source unconditionally so that all queued buffers transition
     * to the "processed" state and can be unqueued.  We do this even when
     * active==qfalse because MusicUpdate may have set active=qfalse on EOF
     * while the source still had buffers in its queue.  Leaving those buffers
     * attached prevents alBufferData from being called on them later, which
     * would make the next StartBackgroundTrack call fail silently. */
    qalSourceStop(s_al_music.source);

    /* Detach every buffer from the source queue so they can be reused. */
    qalGetSourcei(s_al_music.source, AL_BUFFERS_QUEUED, &queued);
    while (queued-- > 0)
        qalSourceUnqueueBuffers(s_al_music.source, 1, &buf);

    if (s_al_music.stream) {
        S_CodecCloseStream(s_al_music.stream);
        s_al_music.stream = NULL;
    }
    s_al_music.active = qfalse;
}

/* Fill one streaming buffer from the music codec.
 * Returns qtrue if data was written, qfalse at end-of-stream. */
static qboolean S_AL_MusicFillBuffer( ALuint buf )
{
    static byte musicBuf[S_AL_MUSIC_BUFSZ];
    snd_stream_t *stream = s_al_music.stream;
    snd_info_t   *info;
    int           read;
    ALenum        fmt;

    if (!stream) return qfalse;

    info = &stream->info;
    read = S_CodecReadStream(stream, S_AL_MUSIC_BUFSZ, musicBuf);
    if (read <= 0) {
        S_AL_TRACE(1, "MusicFillBuffer: stream read=%d (EOF or error) looping=%d\n",
                   read, s_al_music.looping);
        /* End of track — try to loop */
        S_CodecCloseStream(s_al_music.stream);
        s_al_music.stream = NULL;
        if (s_al_music.looping && s_al_music.loopFile[0]) {
            s_al_music.stream = S_CodecOpenStream(s_al_music.loopFile);
            if (!s_al_music.stream) {
                S_AL_TRACE(1, "MusicFillBuffer: loop reopen \"%s\" failed\n",
                           s_al_music.loopFile);
                return qfalse;
            }
            stream = s_al_music.stream;
            info   = &stream->info;
            read   = S_CodecReadStream(stream, S_AL_MUSIC_BUFSZ, musicBuf);
            if (read <= 0) return qfalse;
        } else {
            return qfalse;
        }
    }

    fmt = S_AL_Format(info->width, info->channels);
    qalBufferData(buf, fmt, musicBuf, read, info->rate);
    S_AL_CheckALError("MusicFillBuffer/qalBufferData");
    return qtrue;
}

static void S_AL_MusicUpdate( void )
{
    ALint processed, queued, state;
    ALuint buf;
    int    i;

    if (!s_al_music.active || s_al_music.paused) return;

    qalGetSourcei(s_al_music.source, AL_BUFFERS_PROCESSED, &processed);

    for (i = 0; i < processed; i++) {
        qalSourceUnqueueBuffers(s_al_music.source, 1, &buf);
        if (!S_AL_MusicFillBuffer(buf)) {
            S_AL_TRACE(1, "MusicUpdate: fill failed — stopping music\n");
            s_al_music.active = qfalse;
            return;
        }
        qalSourceQueueBuffers(s_al_music.source, 1, &buf);
        S_AL_CheckALError("MusicUpdate/qalSourceQueueBuffers");
    }

    /* Restart if the source stalled (buffer underrun) */
    qalGetSourcei(s_al_music.source, AL_BUFFERS_QUEUED, &queued);
    qalGetSourcei(s_al_music.source, AL_SOURCE_STATE, &state);
    if (queued > 0 && state != AL_PLAYING && !s_al_music.paused) {
        S_AL_TRACE(1, "MusicUpdate: source stalled — restarting playback\n");
        qalSourcePlay(s_al_music.source);
        S_AL_CheckALError("MusicUpdate/qalSourcePlay");
    }
}

/* =========================================================================
 * Section 10 — soundInterface_t implementations
 * =========================================================================
 */

/* Load EFX entry points via alcGetProcAddress (device-specific),
 * create the reverb + underwater auxiliary slots, and per-source
 * occlusion low-pass filters. */
static void S_AL_InitEFX( void )
{
#define EFX_PROC(T, n) q##n = (T)qalcGetProcAddress(s_al_device, #n)
    int i;
    ALint maxSends = 0;

    Com_Memset(&s_al_efx, 0, sizeof(s_al_efx));

    /* Allow the user to skip EFX entirely for testing/diagnostics. */
    if (s_alEFX && !s_alEFX->integer) {
        Com_Printf("S_AL: EFX disabled by s_alEFX 0\n");
        return;
    }

    if (!qalcIsExtensionPresent(s_al_device, "ALC_EXT_EFX"))
        return;

    EFX_PROC(LPALGENEFFECTS,                 alGenEffects);
    EFX_PROC(LPALDELETEEFFECTS,              alDeleteEffects);
    EFX_PROC(LPALEFFECTI,                    alEffecti);
    EFX_PROC(LPALEFFECTF,                    alEffectf);
    EFX_PROC(LPALEFFECTFV,                   alEffectfv);
    EFX_PROC(LPALGENFILTERS,                 alGenFilters);
    EFX_PROC(LPALDELETEFILTERS,              alDeleteFilters);
    EFX_PROC(LPALFILTERI,                    alFilteri);
    EFX_PROC(LPALFILTERF,                    alFilterf);
    EFX_PROC(LPALGENAUXILIARYEFFECTSLOTS,    alGenAuxiliaryEffectSlots);
    EFX_PROC(LPALDELETEAUXILIARYEFFECTSLOTS, alDeleteAuxiliaryEffectSlots);
    EFX_PROC(LPALAUXILIARYEFFECTSLOTI,       alAuxiliaryEffectSloti);
    EFX_PROC(LPALAUXILIARYEFFECTSLOTF,       alAuxiliaryEffectSlotf);
#undef EFX_PROC

    if (!qalGenEffects || !qalGenFilters || !qalGenAuxiliaryEffectSlots)
        return;

    qalGetError();  /* clear */

    /* Reverb effect — prefer EAX reverb, fall back to standard reverb */
    qalGenEffects(1, &s_al_efx.reverbEffect);
    qalEffecti(s_al_efx.reverbEffect, AL_EFFECT_TYPE, AL_EFFECT_EAXREVERB);
    if (qalGetError() == AL_NO_ERROR) {
        s_al_efx.hasEAXReverb = qtrue;
        /* Medium-sized indoor room — sensible default for URT maps.
         * Reflections gain is intentionally kept moderate so early
         * reflections are audible but the late reverb tail does not
         * drown out direct weapon / footstep sounds. */
        qalEffectf(s_al_efx.reverbEffect, AL_EAXREVERB_DENSITY,           0.5f);
        qalEffectf(s_al_efx.reverbEffect, AL_EAXREVERB_DIFFUSION,         0.85f);
        qalEffectf(s_al_efx.reverbEffect, AL_EAXREVERB_GAIN,              0.20f);
        qalEffectf(s_al_efx.reverbEffect, AL_EAXREVERB_GAINHF,            0.89f);
        qalEffectf(s_al_efx.reverbEffect, AL_EAXREVERB_GAINLF,            1.0f);
        qalEffectf(s_al_efx.reverbEffect, AL_EAXREVERB_DECAY_TIME,
                   s_alReverbDecay ? s_alReverbDecay->value : 1.49f);
        qalEffectf(s_al_efx.reverbEffect, AL_EAXREVERB_DECAY_HFRATIO,     0.83f);
        qalEffectf(s_al_efx.reverbEffect, AL_EAXREVERB_REFLECTIONS_GAIN,  0.15f);
        qalEffectf(s_al_efx.reverbEffect, AL_EAXREVERB_REFLECTIONS_DELAY, 0.007f);
        qalEffectf(s_al_efx.reverbEffect, AL_EAXREVERB_LATE_REVERB_GAIN,  0.35f);
        qalEffectf(s_al_efx.reverbEffect, AL_EAXREVERB_LATE_REVERB_DELAY, 0.011f);
        qalEffectf(s_al_efx.reverbEffect, AL_EAXREVERB_AIR_ABSORPTION_GAINHF, 0.994f);
        qalEffectf(s_al_efx.reverbEffect, AL_EAXREVERB_ROOM_ROLLOFF_FACTOR,   0.0f);
        qalEffecti(s_al_efx.reverbEffect, AL_EAXREVERB_DECAY_HFLIMIT,    AL_TRUE);
    } else {
        /* Fallback: plain reverb */
        qalEffecti(s_al_efx.reverbEffect, AL_EFFECT_TYPE, AL_EFFECT_REVERB);
        qalGetError();
        qalEffectf(s_al_efx.reverbEffect, AL_REVERB_DENSITY,    0.5f);
        qalEffectf(s_al_efx.reverbEffect, AL_REVERB_DIFFUSION,  0.85f);
        qalEffectf(s_al_efx.reverbEffect, AL_REVERB_GAIN,       0.20f);
        qalEffectf(s_al_efx.reverbEffect, AL_REVERB_GAINHF,     0.89f);
        qalEffectf(s_al_efx.reverbEffect, AL_REVERB_DECAY_TIME,
                   s_alReverbDecay ? s_alReverbDecay->value : 1.49f);
        qalEffectf(s_al_efx.reverbEffect, AL_REVERB_REFLECTIONS_GAIN,  0.15f);
        qalEffectf(s_al_efx.reverbEffect, AL_REVERB_REFLECTIONS_DELAY, 0.007f);
        qalEffectf(s_al_efx.reverbEffect, AL_REVERB_LATE_REVERB_GAIN,  0.35f);
        qalEffectf(s_al_efx.reverbEffect, AL_REVERB_LATE_REVERB_DELAY, 0.011f);
    }
    qalGenAuxiliaryEffectSlots(1, &s_al_efx.reverbSlot);
    qalAuxiliaryEffectSloti(s_al_efx.reverbSlot, AL_EFFECTSLOT_EFFECT,
                            (ALint)s_al_efx.reverbEffect);
    /* Apply initial reverb slot gain from cvar */
    {
        float slotGain = s_alReverbGain ? s_alReverbGain->value : 0.20f;
        if (slotGain < 0.0f) slotGain = 0.0f;
        if (slotGain > 1.0f) slotGain = 1.0f;
        qalAuxiliaryEffectSlotf(s_al_efx.reverbSlot, AL_EFFECTSLOT_GAIN, slotGain);
    }

    /* Underwater effect */
    qalGenEffects(1, &s_al_efx.underwaterEffect);
    if (s_al_efx.hasEAXReverb) {
        qalEffecti(s_al_efx.underwaterEffect, AL_EFFECT_TYPE, AL_EFFECT_EAXREVERB);
        qalEffectf(s_al_efx.underwaterEffect, AL_EAXREVERB_DENSITY,    0.1f);
        qalEffectf(s_al_efx.underwaterEffect, AL_EAXREVERB_DIFFUSION,  1.0f);
        qalEffectf(s_al_efx.underwaterEffect, AL_EAXREVERB_GAIN,       0.1f);
        qalEffectf(s_al_efx.underwaterEffect, AL_EAXREVERB_GAINHF,     0.1f);
        qalEffectf(s_al_efx.underwaterEffect, AL_EAXREVERB_DECAY_TIME, 1.5f);
        qalEffectf(s_al_efx.underwaterEffect, AL_EAXREVERB_DECAY_HFRATIO, 0.1f);
    } else {
        qalEffecti(s_al_efx.underwaterEffect, AL_EFFECT_TYPE, AL_EFFECT_REVERB);
        qalGetError();
        qalEffectf(s_al_efx.underwaterEffect, AL_REVERB_GAIN,   0.1f);
        qalEffectf(s_al_efx.underwaterEffect, AL_REVERB_GAINHF, 0.1f);
    }
    qalGenAuxiliaryEffectSlots(1, &s_al_efx.underwaterSlot);
    qalAuxiliaryEffectSloti(s_al_efx.underwaterSlot, AL_EFFECTSLOT_EFFECT,
                            (ALint)s_al_efx.underwaterEffect);

    /* Per-source occlusion low-pass filters */
    qalGenFilters(S_AL_MAX_SRC, s_al_efx.occlusionFilter);
    for (i = 0; i < S_AL_MAX_SRC; i++) {
        qalFilteri(s_al_efx.occlusionFilter[i], AL_FILTER_TYPE, AL_FILTER_LOWPASS);
        qalFilterf(s_al_efx.occlusionFilter[i], AL_LOWPASS_GAIN,   1.0f);
        qalFilterf(s_al_efx.occlusionFilter[i], AL_LOWPASS_GAINHF, 1.0f);
    }

    qalcGetIntegerv(s_al_device, ALC_MAX_AUXILIARY_SENDS, 1, &maxSends);
    s_al_efx.available = qtrue;
    Com_Printf("S_AL: EFX available (%s reverb, %d aux sends)\n",
        s_al_efx.hasEAXReverb ? "EAX" : "standard", maxSends);
}

static void S_AL_Shutdown( void )
{
    int i;

    if (!s_al_started) return;
    s_al_started = qfalse;

    Cmd_RemoveCommand("s_devices");
    Cmd_RemoveCommand("s_alReset");

    /* Stop all sources */
    for (i = 0; i < s_al_numSrc; i++)
        S_AL_StopSource(i);

    /* Delete sources */
    for (i = 0; i < s_al_numSrc; i++) {
        if (s_al_src[i].source)
            qalDeleteSources(1, &s_al_src[i].source);
    }
    s_al_numSrc = 0;

    /* Music — MusicStop already stops and unqueues all buffers.
     * The redundant qalSourceStop that was here has been removed.
     * Guard the delete against source==0 (init failure when voice pool
     * was exhausted before the music source could be created). */
    S_AL_MusicStop();
    if (s_al_music.source)
        qalDeleteSources(1, &s_al_music.source);
    qalDeleteBuffers(S_AL_MUSIC_BUFFERS, s_al_music.buffers);

    /* Raw samples */
    if (s_al_raw.active) {
        if (s_al_raw.source)
            qalSourceStop(s_al_raw.source);
        s_al_raw.active = qfalse;
    }
    if (s_al_raw.source)
        qalDeleteSources(1, &s_al_raw.source);
    qalDeleteBuffers(S_AL_RAW_BUFFERS, s_al_raw.buffers);

    /* EFX cleanup */
    if (s_al_efx.available) {
        int j;
        qalDeleteAuxiliaryEffectSlots(1, &s_al_efx.reverbSlot);
        qalDeleteAuxiliaryEffectSlots(1, &s_al_efx.underwaterSlot);
        qalDeleteEffects(1, &s_al_efx.reverbEffect);
        qalDeleteEffects(1, &s_al_efx.underwaterEffect);
        for (j = 0; j < S_AL_MAX_SRC; j++)
            if (s_al_efx.occlusionFilter[j])
                qalDeleteFilters(1, &s_al_efx.occlusionFilter[j]);
        Com_Memset(&s_al_efx, 0, sizeof(s_al_efx));
    }

    /* Sfx */
    S_AL_FreeSfx();

    /* Context / device */
    qalcMakeContextCurrent(NULL);
    if (s_al_context) { qalcDestroyContext(s_al_context); s_al_context = NULL; }
    if (s_al_device)  { qalcCloseDevice(s_al_device);     s_al_device  = NULL; }

    S_AL_UnloadLibrary();
    Com_Memset(&s_al_music, 0, sizeof(s_al_music));
    Com_Memset(&s_al_raw,   0, sizeof(s_al_raw));
}

static void S_AL_StartSound( const vec3_t origin, int entnum,
                              int entchannel, sfxHandle_t sfx )
{
    alSfxRec_t *r;
    int         srcIdx;
    float       vol;
    vec3_t      sndOrigin;
    qboolean    isLocal;

    if (!s_al_started || sfx < 0 || sfx >= s_al_numSfx) return;
    r = &s_al_sfx[sfx];
    if (!r->inMemory || r->defaultSound || !r->buffer) return;

    /* Determine the sound's world origin as early as possible so we can
     * apply a distance-based rejection before allocating any slot.
     * Sounds beyond s_alMaxDist are already inaudible in the distance
     * model — don't waste a slot on them. */
    if (entnum == s_al_listener_entnum &&
            (!s_alLocalSelf || s_alLocalSelf->integer)) {
        VectorClear(sndOrigin);
        isLocal = qtrue;
    } else if (origin) {
        VectorCopy(origin, sndOrigin);
        isLocal = qfalse;
    } else {
        if (entnum >= 0 && entnum < MAX_GENTITIES)
            VectorCopy(s_al_entity_origins[entnum], sndOrigin);
        else
            VectorClear(sndOrigin);
        isLocal = qfalse;
    }

    if (!isLocal) {
        float maxDist = s_alMaxDist ? s_alMaxDist->value : S_AL_SOUND_MAXDIST;
        vec3_t d;
        VectorSubtract(sndOrigin, s_al_listener_origin, d);
        if (DotProduct(d, d) > maxDist * maxDist)
            return;   /* beyond audible range — no slot needed */
    }

    srcIdx = S_AL_GetFreeSource();
    if (srcIdx < 0) return;

    vol = S_AL_GetMasterVol();
    /* fixed_origin: lock position only for explicit world-space origins that
     * belong to other entities.  Own-entity sounds are always local (above),
     * so fixed_origin is irrelevant for them. */
    S_AL_SrcSetup(srcIdx, sfx, isLocal ? NULL : sndOrigin,
                  (origin != NULL && !isLocal), entnum, entchannel, vol, isLocal,
                  qfalse /* one-shot entity sound, not ambient */);

    r->lastTimeUsed = Com_Milliseconds();
    qalSourcePlay(s_al_src[srcIdx].source);
    S_AL_CheckALError("StartSound/qalSourcePlay");
    S_AL_TRACE(2, "StartSound: sfx=%d ent=%d ch=%d slot=%d local=%d\n",
               sfx, entnum, entchannel, srcIdx, isLocal);
}

static void S_AL_StartLocalSound( sfxHandle_t sfx, int channelNum )
{
    alSfxRec_t *r;
    int         srcIdx;
    float       vol;

    if (!s_al_started || sfx < 0 || sfx >= s_al_numSfx) return;
    r = &s_al_sfx[sfx];
    if (!r->inMemory || r->defaultSound || !r->buffer) return;

    srcIdx = S_AL_GetFreeSource();
    if (srcIdx < 0) return;

    vol = S_AL_GetMasterVol();
    S_AL_SrcSetup(srcIdx, sfx, NULL, qfalse,
                  0, channelNum, vol, qtrue /* local */,
                  qfalse /* not ambient */);
    r->lastTimeUsed = Com_Milliseconds();
    qalSourcePlay(s_al_src[srcIdx].source);
    S_AL_CheckALError("StartLocalSound/qalSourcePlay");
    S_AL_TRACE(2, "StartLocalSound: sfx=%d ch=%d slot=%d\n",
               sfx, channelNum, srcIdx);
}

static void S_AL_StartBackgroundTrack( const char *intro, const char *loop )
{
    int      i;
    qboolean primed = qfalse;

    if (!s_al_started) return;

    S_AL_TRACE(1, "StartBackgroundTrack: intro=\"%s\" loop=\"%s\" source=%u\n",
               intro ? intro : "(null)", loop ? loop : "(null)",
               s_al_music.source);

    S_AL_MusicStop();

    if (!intro || !*intro) return;

    /* If no valid music source was created at init (e.g. voice pool was
     * exhausted by the game source pool — a PR #95 regression), warn and
     * bail out gracefully rather than sending AL_INVALID_NAME to every
     * subsequent qalSource* call, which causes OpenAL Soft's 64-bit GCC
     * runtime to throw 0x20474343 C++ exceptions and hard-crash. */
    if (!s_al_music.source) {
        Com_Printf(S_COLOR_YELLOW "S_AL: StartBackgroundTrack: no music source — "
                   "background music disabled (check s_alMaxSrc)\n");
        return;
    }

    s_al_music.stream = S_CodecOpenStream(intro);
    if (!s_al_music.stream) {
        Com_Printf(S_COLOR_YELLOW "S_AL: couldn't open music %s\n", intro);
        return;
    }
    S_AL_TRACE(1, "StartBackgroundTrack: stream opened for \"%s\"\n", intro);

    Q_strncpyz(s_al_music.introFile, intro, sizeof(s_al_music.introFile));
    if (loop && *loop)
        Q_strncpyz(s_al_music.loopFile, loop, sizeof(s_al_music.loopFile));
    else
        s_al_music.loopFile[0] = '\0';
    s_al_music.looping = (s_al_music.loopFile[0] != '\0');

    /* Prime streaming buffers */
    for (i = 0; i < S_AL_MUSIC_BUFFERS; i++) {
        if (!S_AL_MusicFillBuffer(s_al_music.buffers[i]))
            break;
        qalSourceQueueBuffers(s_al_music.source, 1, &s_al_music.buffers[i]);
        S_AL_CheckALError("StartBackgroundTrack/qalSourceQueueBuffers");
        primed = qtrue;
    }

    if (!primed) {
        S_AL_TRACE(1, "StartBackgroundTrack: priming failed for \"%s\"\n", intro);
        S_AL_MusicStop();
        return;
    }

    s_al_music.active = qtrue;
    s_al_music.paused = qfalse;

    qalSourcef(s_al_music.source, AL_GAIN,
               s_musicVolume ? s_musicVolume->value : 0.25f);
    qalSourcei(s_al_music.source, AL_SOURCE_RELATIVE, AL_TRUE);
    qalSource3f(s_al_music.source, AL_POSITION, 0.0f, 0.0f, 0.0f);
    qalSourcef(s_al_music.source, AL_ROLLOFF_FACTOR, 0.0f);
    qalSourcePlay(s_al_music.source);
    S_AL_CheckALError("StartBackgroundTrack/qalSourcePlay");
    S_AL_TRACE(1, "StartBackgroundTrack: playing (looping=%d)\n",
               s_al_music.looping);
}

static void S_AL_StopBackgroundTrack( void )
{
    S_AL_MusicStop();
}

static void S_AL_RawSamples( int samples, int rate, int width, int channels,
                              const byte *data, float volume )
{
    ALenum  fmt;
    ALuint  buf;
    ALint   state, processed;
    int     size;

    if (!s_al_started) return;

    /* If no valid raw source was created at init (voice pool exhausted by
     * the game source pool), silently drop cinematic/VoIP audio rather than
     * flooding AL with AL_INVALID_NAME errors that crash the 64-bit build. */
    if (!s_al_raw.source) {
        S_AL_TRACE(2, "RawSamples: no raw source — dropping %d samples\n", samples);
        return;
    }

    fmt  = S_AL_Format(width, channels);
    size = samples * width * channels;

    /* Reclaim any AL buffers that the source has already finished playing.
     * In OpenAL, a buffer stays "attached" to the source queue (and cannot
     * be written to with alBufferData) until it is explicitly unqueued, even
     * after it has been played.  Without this drain step the round-robin pool
     * would attempt alBufferData on still-attached buffers after 8 calls and
     * silently fail, causing raw-samples audio (cinematics, VoIP) to stop. */
    processed = 0;
    qalGetSourcei(s_al_raw.source, AL_BUFFERS_PROCESSED, &processed);
    while (processed-- > 0) {
        ALuint tmp;
        qalSourceUnqueueBuffers(s_al_raw.source, 1, &tmp);
    }

    /* Grab one buffer from our small pool (round-robin). */
    buf = s_al_raw.buffers[s_al_raw.nextBuf];
    s_al_raw.nextBuf = (s_al_raw.nextBuf + 1) % S_AL_RAW_BUFFERS;

    qalBufferData(buf, fmt, data, size, rate);
    S_AL_CheckALError("RawSamples/qalBufferData");
    qalSourceQueueBuffers(s_al_raw.source, 1, &buf);
    S_AL_CheckALError("RawSamples/qalSourceQueueBuffers");

    qalSourcef(s_al_raw.source, AL_GAIN, volume);
    state = AL_STOPPED;
    qalGetSourcei(s_al_raw.source, AL_SOURCE_STATE, &state);
    if (state != AL_PLAYING)
        qalSourcePlay(s_al_raw.source);

    s_al_raw.active = qtrue;
}

static void S_AL_StopAllSounds( void )
{
    int i;

    if (!s_al_started) return;

    for (i = 0; i < s_al_numSrc; i++)
        S_AL_StopSource(i);

    S_AL_MusicStop();

    if (s_al_raw.active) {
        if (s_al_raw.source)
            qalSourceStop(s_al_raw.source);
        s_al_raw.active = qfalse;
    }

    Com_Memset(s_al_loops, 0, sizeof(s_al_loops));
    /* srcIdx must start at -1 (not 0) so UpdateLoops knows no source is
     * assigned yet.  Com_Memset above zeroed everything to 0. */
    for (i = 0; i < MAX_GENTITIES; i++)
        s_al_loops[i].srcIdx = -1;
    s_al_loopFrame = 0;

    /* Reset entity-origin cache so stale positions don't bleed in. */
    Com_Memset(s_al_entity_origins, 0, sizeof(s_al_entity_origins));
    s_al_listener_entnum = -1;
}

static void S_AL_ClearLoopingSounds( qboolean killall )
{
    int i;
    for (i = 0; i < MAX_GENTITIES; i++) {
        s_al_loops[i].kill = qtrue;
    }
}

static void S_AL_AddLoopingSound( int entityNum, const vec3_t origin,
                                  const vec3_t velocity, sfxHandle_t sfx )
{
    alLoop_t *lp;
    if (!s_al_started || entityNum < 0 || entityNum >= MAX_GENTITIES) return;
    lp = &s_al_loops[entityNum];
    lp->sfx      = sfx;
    lp->kill     = qfalse;
    lp->active   = qtrue;
    lp->framenum = s_al_loopFrame;
    VectorCopy(origin,   lp->origin);
    VectorCopy(velocity, lp->velocity);
}

static void S_AL_AddRealLoopingSound( int entityNum, const vec3_t origin,
                                      const vec3_t velocity, sfxHandle_t sfx )
{
    S_AL_AddLoopingSound(entityNum, origin, velocity, sfx);
}

static void S_AL_StopLoopingSound( int entityNum )
{
    if (entityNum < 0 || entityNum >= MAX_GENTITIES) return;
    s_al_loops[entityNum].kill   = qtrue;
    s_al_loops[entityNum].active = qfalse;
}

/* Update looping sounds: start, stop, reposition.
 *
 * Fade-in  (S_AL_LOOP_FADEIN_MS):  new loop sources start at gain=0 and ramp
 *   to full over ~150 ms, preventing the cold-start pop when the listener
 *   enters an ambient entity's range.
 *
 * Fade-out (S_AL_LOOP_FADEOUT_MS): when a loop is killed (entity leaves the
 *   client snapshot / PVS cull) or explicitly removed, the source ramps to
 *   silence over ~120 ms before being stopped.  This prevents the hard cut
 *   heard when walking out of a fountain or ambient-sound entity's range.
 *
 * State machine per alLoop_t:
 *   active=true,  fadeOutStartMs=0  → normal playing (or fade-in still running)
 *   active=false, fadeOutStartMs>0  → fading out; source still playing
 *   active=false, fadeOutStartMs=0  → fully stopped / idle
 *
 * Re-activation during fade-out (entity pops back into snapshot mid-fade):
 *   fadeOutStartMs is cleared, a fresh fade-in begins from the current source
 *   so there is no audible gap or restart. */
static void S_AL_UpdateLoops( void )
{
    int       i, srcIdx;
    alLoop_t *lp;
    int       now = Com_Milliseconds();

    for (i = 0; i < MAX_GENTITIES; i++) {
        lp = &s_al_loops[i];

        /* ---- Step 1: Trigger fade-out when a loop goes stale / killed ---- */
        if (lp->active && (lp->kill || lp->framenum != s_al_loopFrame)) {
            lp->active = qfalse;
            lp->kill   = qfalse;
            if (lp->srcIdx >= 0 && lp->srcIdx < s_al_numSrc &&
                    s_al_src[lp->srcIdx].isPlaying) {
                /* Start fade-out instead of hard-stopping. */
                if (!lp->fadeOutStartMs)
                    lp->fadeOutStartMs = (now > 0) ? now : 1;
            } else {
                /* Source not playing — nothing to fade, clean up immediately. */
                lp->srcIdx         = -1;
                lp->fadeOutStartMs = 0;
                lp->fadeStartMs    = 0;
            }
        }

        /* ---- Step 2: Drive the fade-out ramp ---- */
        if (!lp->active && lp->fadeOutStartMs > 0) {
            if (lp->srcIdx >= 0 && lp->srcIdx < s_al_numSrc &&
                    s_al_src[lp->srcIdx].isPlaying) {
                int elapsed = now - lp->fadeOutStartMs;
                if (elapsed >= S_AL_LOOP_FADEOUT_MS) {
                    /* Fade complete — hard-stop the source. */
                    S_AL_StopSource(lp->srcIdx);
                    s_al_src[lp->srcIdx].loopSound = qfalse;
                    lp->srcIdx         = -1;
                    lp->fadeOutStartMs = 0;
                    lp->fadeStartMs    = 0;
                } else {
                    float    t      = 1.0f - (float)elapsed / (float)S_AL_LOOP_FADEOUT_MS;
                    alSrc_t *src    = &s_al_src[lp->srcIdx];
                    float    catVol = S_AL_GetCategoryVol(SRC_CAT_AMBIENT);
                    if (lp->sfx >= 0 && lp->sfx < s_al_numSfx)
                        catVol *= s_al_sfx[lp->sfx].normGain;
                    qalSourcef(src->source, AL_GAIN,
                               (src->master_vol / 255.f) * catVol * t);
                }
            } else {
                /* Source stopped externally (e.g. StopAllSounds). */
                lp->srcIdx         = -1;
                lp->fadeOutStartMs = 0;
                lp->fadeStartMs    = 0;
            }
            continue;   /* don't touch this loop further this frame */
        }

        if (!lp->active) continue;
        if (lp->sfx < 0 || lp->sfx >= s_al_numSfx) continue;
        if (!s_al_sfx[lp->sfx].buffer) continue;

        /* ---- Step 3: Cancel fade-out if the entity was re-added ---- *
         * (active=true while a previous fade-out is still running means the
         * entity re-entered the snapshot before the fade finished.) */
        if (lp->fadeOutStartMs > 0) {
            /* Don't restart the source — just cancel the fade-out and let
             * the fade-in ramp bring gain back up from here. */
            lp->fadeOutStartMs = 0;
            lp->fadeStartMs    = (now > 0) ? now : 1;
        }

        /* ---- Step 4: Start a new source if needed ---- */
        if (lp->srcIdx < 0 || !s_al_src[lp->srcIdx].isPlaying) {
            /* Dedup: mirror DMA S_AddLoopSounds() merge behaviour.
             * When two or more map entities reference the same sfx (e.g.
             * ut4_austria has three ambient entities playing the same Bach
             * prelude loop), only ONE AL source is ever running for that sfx.
             * We scan ALL loop slots (not just k < i) so that a higher-index
             * entity that already owns a source also blocks a lower-index
             * newcomer — whichever entity entered PVS first keeps the source.
             * When the owning entity leaves PVS its active flag clears (Step 1),
             * which immediately lets any remaining duplicate claim a source on
             * the next frame (smooth cross-fade rather than a hard cut).
             * Without this, N entities → N simultaneous sources → N× volume
             * ("double running", confirmed by 3 identical stop-offsets in
             *  ut4_austria logs). */
            if (lp->srcIdx < 0) {
                int      k;
                qboolean sfxDuped = qfalse;
                for (k = 0; k < MAX_GENTITIES; k++) {
                    alLoop_t *other;
                    if (k == i) continue;
                    other = &s_al_loops[k];
                    if (!other->active || other->srcIdx < 0) continue;
                    if (other->sfx != lp->sfx)              continue;
                    if (s_al_src[other->srcIdx].isPlaying)  { sfxDuped = qtrue; break; }
                }
                if (sfxDuped) continue;
            }
            srcIdx = S_AL_GetFreeSource();
            if (srcIdx < 0) continue;
            {
                float vol = S_AL_GetMasterVol();
                S_AL_SrcSetup(srcIdx, lp->sfx, lp->origin, qtrue,
                              i, CHAN_AUTO, vol, qfalse,
                              qtrue /* looping ambient */);
            }
            s_al_src[srcIdx].loopSound = qtrue;
            qalSourcei(s_al_src[srcIdx].source, AL_LOOPING, AL_TRUE);
            /* Start silent — gain driven up by the fade-in ramp below. */
            qalSourcef(s_al_src[srcIdx].source, AL_GAIN, 0.0f);
            qalSourcePlay(s_al_src[srcIdx].source);
            lp->srcIdx      = srcIdx;
            lp->fadeStartMs = (now > 0) ? now : 1;
        } else {
            /* Update position and velocity every frame. */
            qalSource3f(s_al_src[lp->srcIdx].source, AL_POSITION,
                        lp->origin[0], lp->origin[1], lp->origin[2]);
            qalSource3f(s_al_src[lp->srcIdx].source, AL_VELOCITY,
                        lp->velocity[0], lp->velocity[1], lp->velocity[2]);
        }

        /* ---- Step 5: Apply fade-in gain ramp ---- */
        if (lp->fadeStartMs > 0) {
            int     elapsed = now - lp->fadeStartMs;
            float   fadeGain;
            alSrc_t *src    = &s_al_src[lp->srcIdx];
            float    catVol = S_AL_GetCategoryVol(SRC_CAT_AMBIENT);
            if (lp->sfx >= 0 && lp->sfx < s_al_numSfx)
                catVol *= s_al_sfx[lp->sfx].normGain;
            if (elapsed >= S_AL_LOOP_FADEIN_MS) {
                fadeGain        = 1.0f;
                lp->fadeStartMs = 0;   /* fade complete — stop updating gain */
            } else {
                fadeGain = (float)elapsed / (float)S_AL_LOOP_FADEIN_MS;
            }
            qalSourcef(src->source, AL_GAIN,
                       (src->master_vol / 255.f) * catVol * fadeGain);
        }
    }
}

static void S_AL_Respatialize( int entityNum, const vec3_t origin,
                                vec3_t axis[3], int inwater )
{
    ALfloat orient[6];

    if (!s_al_started) return;

    /* Track which entity is the listener so StartSound can treat that
     * entity's sounds as non-spatialized (local). */
    s_al_listener_entnum = entityNum;

    /* Update the listener entity's cached origin so that any look-up
     * through s_al_entity_origins also returns the correct position. */
    if (entityNum >= 0 && entityNum < MAX_GENTITIES)
        VectorCopy(origin, s_al_entity_origins[entityNum]);

    /* Listener position */
    qalListener3f(AL_POSITION, origin[0], origin[1], origin[2]);
    VectorCopy(origin, s_al_listener_origin);
    s_al_inwater = (inwater != 0);

    /* Orientation: forward then up vectors */
    orient[0] =  axis[0][0];   /* forward X */
    orient[1] =  axis[0][1];   /* forward Y */
    orient[2] =  axis[0][2];   /* forward Z */
    orient[3] =  axis[2][0];   /* up X */
    orient[4] =  axis[2][1];   /* up Y */
    orient[5] =  axis[2][2];   /* up Z */
    qalListenerfv(AL_ORIENTATION, orient);
}

static void S_AL_UpdateEntityPosition( int entityNum, const vec3_t origin )
{
    int i;

    /* Keep the per-entity origin cache current so that StartSound can place
     * new one-shot sounds at the right position when origin is NULL. */
    if (entityNum >= 0 && entityNum < MAX_GENTITIES)
        VectorCopy(origin, s_al_entity_origins[entityNum]);

    /* Move any active source attached to this entity.
     * Skip local (listener-relative) sources: they use AL_SOURCE_RELATIVE=TRUE
     * and must stay at position (0,0,0).  Setting them to a world-space origin
     * while they are in relative mode causes the sound to appear off to the
     * side or behind the player (the world coordinate is treated as a relative
     * offset from the listener instead of an absolute position). */
    for (i = 0; i < s_al_numSrc; i++) {
        if (!s_al_src[i].isPlaying) continue;
        if (s_al_src[i].entnum == entityNum && !s_al_src[i].fixed_origin
                && !s_al_src[i].isLocal) {
            VectorCopy(origin, s_al_src[i].origin);
            qalSource3f(s_al_src[i].source, AL_POSITION,
                        origin[0], origin[1], origin[2]);
        }
    }
}

/* =========================================================================
 * Dynamic acoustic environment detection
 *
 * Called from S_AL_Update every S_AL_ENV_RATE frames when s_alDynamicReverb
 * is non-zero.  Casts 14 rays (8 horizontal, 4 diagonal-up, 1 up, 1 down)
 * from the listener position to measure the surrounding space, then derives
 * EFX reverb parameters that match the environment:
 *
 *   Small room        — short decay, modest late-gain, modest reflections
 *   Corridor          — medium decay, stronger early reflections
 *   Large hall / cave — longer decay, moderate late-reverb tail
 *   Open outdoor      — very short decay, near-dry (slot gain → low)
 *
 * Only CONTENTS_SOLID is tested (not PLAYERCLIP) to avoid player-clip
 * brushes registering as acoustic surfaces.  Max ray length equals the
 * sound attenuation distance so we never measure geometry beyond the
 * audible range.
 *
 * Each ray group uses a calibrated maximum distance (see s_al_envDists[]):
 *   Horizontal 0–7  : S_AL_SOUND_MAXDIST (1330 u) — room-scale openness
 *   Diagonal-up 8–11: S_AL_ENV_VERT_DIST (400 u)  — ceiling up to ~283 u
 *   Straight up   12: S_AL_ENV_VERT_DIST (400 u)  — ceiling up to 400 u
 *   Straight down 13: S_AL_ENV_DOWN_DIST (160 u)  — floor / pit context
 * Using a single global max for all rays made ceilings appear nearly open
 * (a 200-unit ceiling gave fr ≈ 0.15 at 1330-unit scale) and made overhangs
 * invisible to the vertical openness metric.
 *
 * A rolling history buffer (S_AL_ENV_HISTORY slots) averages the last N probe
 * sets.  A single "confused" reading — caused by complex geometry, thin walls,
 * windows, or straddling a doorway threshold — only shifts the average by 1/N,
 * preventing transient wrong classifications from audibly changing the reverb.
 *
 * A movement cache skips re-tracing when the listener hasn't moved more than
 * S_AL_ENV_MOVE_THRESH units, saving CM traces for stationary players.
 *
 * EFX parameter targets are intentionally conservative (late-gain max 0.35,
 * decay max 2.5 s) so the reverb colours the space without drowning the direct
 * signal or causing layered weapon sounds to sound muffled.
 *
 * Performance: 14 traces / S_AL_ENV_RATE frames ≈ 0.9 traces/frame average
 *              (zero traces when movement cache is active).
 * ========================================================================= */

#define S_AL_ENV_RAYS        14
#define S_AL_ENV_RATE         16    /* update every N frames */
#define S_AL_ENV_HISTORY       3    /* default rolling window (s_alEnvHistorySize default) */
#define S_AL_ENV_HISTORY_MAX   8    /* hard cap; array sizing for s_alEnvHistorySize */
#define S_AL_ENV_MOVE_THRESH  32.0f /* units; skip re-cast when barely moved */

/* World-space probe directions (normalised).
 * 8 horizontal (every 45°), 4 diagonal-up, 1 up, 1 down. */
static const float s_al_envDirs[S_AL_ENV_RAYS][3] = {
    /* horizontal */
    { 1.000f,  0.000f,  0.000f},
    { 0.707f,  0.707f,  0.000f},
    { 0.000f,  1.000f,  0.000f},
    {-0.707f,  0.707f,  0.000f},
    {-1.000f,  0.000f,  0.000f},
    {-0.707f, -0.707f,  0.000f},
    { 0.000f, -1.000f,  0.000f},
    { 0.707f, -0.707f,  0.000f},
    /* diagonal-up */
    { 0.707f,  0.000f,  0.707f},
    { 0.000f,  0.707f,  0.707f},
    {-0.707f,  0.000f,  0.707f},
    { 0.000f, -0.707f,  0.707f},
    /* vertical */
    { 0.000f,  0.000f,  1.000f},
    { 0.000f,  0.000f, -1.000f},
};

/* Per-ray trace distance (game units).
 *
 * Using a single global max-distance for all rays caused two problems:
 *   1. Vertical / diagonal-up rays at 1330 units made a normal 200-unit
 *      indoor ceiling appear nearly "open" (fr ≈ 0.15), so the classifier
 *      failed to distinguish low-ceiling corridors from open sky.
 *   2. An overhead overhang at, say, 120 units only pulled fr down to 0.09,
 *      barely affecting vertOpenFrac, so the 70/30 horizontal/vertical
 *      weighting could not fully compensate.
 *
 * Solution: calibrate each ray group to the geometry scale it is measuring.
 *
 *  Horizontal 0–7 : S_AL_SOUND_MAXDIST (1330 u)
 *      Full attenuation range — needed for room-scale openness and corridor
 *      detection.  A wall 600 units away gives fr = 0.45 (clearly enclosed);
 *      an open street gives fr ≈ 1.0 (clearly open).
 *
 *  Diagonal-up 8–11 : S_AL_ENV_VERT_DIST (400 u)
 *      A 45° diagonal ray of 400 units detects ceilings up to
 *      400 × sin 45° ≈ 283 units high.  This covers the full range of URT
 *      indoor ceilings (typically 160–280 u) while treating anything taller
 *      (large atria, outdoor sky) as fully open (fr = 1.0).
 *
 *  Straight up 12 : S_AL_ENV_VERT_DIST (400 u)
 *      Direct ceiling height up to 400 units.  A 200-unit ceiling gives
 *      fr = 0.50 (half blocked — correctly "indoor").  Sky → fr = 1.0.
 *
 *  Straight down 13 : S_AL_ENV_DOWN_DIST (160 u)
 *      Floor / pit context within a normal drop height.  Normal standing
 *      gives fr ≈ 0.25 (floor close below — as expected).  A large pit or
 *      open void below gives fr = 1.0 (contributes some openness). */
#define S_AL_ENV_VERT_DIST  400.0f   /* diagonal-up + straight-up max (units) */
#define S_AL_ENV_DOWN_DIST  160.0f   /* straight-down max (units) */

static const float s_al_envDists[S_AL_ENV_RAYS] = {
    /* horizontal (0–7): full sound-attenuation range */
    S_AL_SOUND_MAXDIST,   /* 0: +X */
    S_AL_SOUND_MAXDIST,   /* 1: +X+Y diagonal */
    S_AL_SOUND_MAXDIST,   /* 2: +Y */
    S_AL_SOUND_MAXDIST,   /* 3: -X+Y diagonal */
    S_AL_SOUND_MAXDIST,   /* 4: -X */
    S_AL_SOUND_MAXDIST,   /* 5: -X-Y diagonal */
    S_AL_SOUND_MAXDIST,   /* 6: -Y */
    S_AL_SOUND_MAXDIST,   /* 7: +X-Y diagonal */
    /* diagonal-up (8–11): ceiling/overhang sensitivity */
    S_AL_ENV_VERT_DIST,   /* 8:  +X up */
    S_AL_ENV_VERT_DIST,   /* 9:  +Y up */
    S_AL_ENV_VERT_DIST,   /* 10: -X up */
    S_AL_ENV_VERT_DIST,   /* 11: -Y up */
    /* straight up (12): direct ceiling height */
    S_AL_ENV_VERT_DIST,   /* 12: +Z */
    /* straight down (13): floor / pit context */
    S_AL_ENV_DOWN_DIST,   /* 13: -Z */
};
/* Compile-time guard: every entry in s_al_envDirs must have a matching
 * entry in s_al_envDists.  A mismatch causes a division-by-zero-size error. */
typedef char s_al_envDists_size_assert[
    (sizeof(s_al_envDists)/sizeof(s_al_envDists[0]) == S_AL_ENV_RAYS) ? 1 : -1];

static void S_AL_UpdateDynamicReverb( void )
{
    /* Smoothed current EFX values (negative = uninitialised, snap on first run) */
    static float curDecay  = -1.f;
    static float curLate   =  0.f;
    static float curRefl   =  0.f;
    static float curSlot   =  0.f;
    static int   lastFrame = -(S_AL_ENV_RATE);

    /* Rolling environment history — averages the last S_AL_ENV_HISTORY probe
     * sets so that a single "confused" reading (thin wall, window, doorway
     * straddling) only moves the average by 1/N rather than flipping the
     * whole classification. */
    static float histSize[S_AL_ENV_HISTORY_MAX];
    static float histOpen[S_AL_ENV_HISTORY_MAX];
    static float histCorr[S_AL_ENV_HISTORY_MAX];
    static int   histIdx   = 0;
    static int   histCount = 0;

    /* Movement cache — skip expensive traces when the listener hasn't moved
     * far enough for new geometry to produce meaningfully different results. */
    static vec3_t lastProbeOrigin;
    static float  cachedSize       = 0.f;
    static float  cachedOpen       = 0.f;
    static float  cachedCorr       = 0.f;
    static float  cachedHorizOpen  = 0.f;  /* for debug verbose split display */
    static float  cachedVertOpen   = 0.f;
    static qboolean haveCache = qfalse;

    /* Velocity tracking for direction-of-travel look-ahead probing.
     * cachedVelSpeed: movement units since last probe (0 when stationary).
     * cachedVelDir:   normalised horizontal direction of travel. */
    static float  cachedVelSpeed      = 0.f;
    static float  cachedVelDir[3]     = {1.f, 0.f, 0.f};

    float horizDists[8];            /* per-ray horizontal distances, original order */
    float horizTotalFrac, vertTotalFrac;
    int   i;
    float horizOpenFrac, vertOpenFrac, openFrac, sizeFactor, corrFactor;
    float targetDecay, targetLate, targetRefl, targetSlot;
    float baseGain;
    qboolean snap;

    if (!s_al_efx.available)                                  return;
    if (!s_alReverb        || !s_alReverb->integer)           return;
    if (!s_alDynamicReverb || !s_alDynamicReverb->integer)    return;
    if (s_al_inwater)                                         return;
    /* No map loaded yet (main menu, pre-game) — CM_BoxTrace with handle 0
     * would crash with "bad handle 0 < 0 < 256" when cm.numSubModels == 0. */
    if (CM_NumInlineModels() <= 0)                            return;

    /* Detect frame-counter reset (map load / StopAllSounds) or explicit reset
     * command (s_alReset — lets the user re-probe after changing tuning cvars). */
    snap = (s_al_loopFrame < lastFrame || curDecay < 0.f || s_al_reverbReset);
    if (snap) {
        lastFrame       = s_al_loopFrame - S_AL_ENV_RATE;
        s_al_reverbReset = qfalse;
        /* Invalidate history and movement cache so the first post-load probe
         * snaps immediately rather than blending stale data from the old map. */
        histCount  = 0;
        histIdx    = 0;
        haveCache  = qfalse;
        cachedVelSpeed = 0.f;
    }

    if ((s_al_loopFrame - lastFrame) < S_AL_ENV_RATE)
        return;
    lastFrame = s_al_loopFrame;

    /* --- Probe the environment (or reuse movement cache) ----------------- */
    {
        vec3_t  moveDelta;
        float   moveDist;
        VectorSubtract(s_al_listener_origin, lastProbeOrigin, moveDelta);
        moveDist = VectorLength(moveDelta);

        if (haveCache && moveDist < S_AL_ENV_MOVE_THRESH) {
            /* Listener hasn't moved enough to justify re-tracing; reuse the
             * cached metrics from the last probe run. */
            sizeFactor    = cachedSize;
            openFrac      = cachedOpen;
            corrFactor    = cachedCorr;
            horizOpenFrac = cachedHorizOpen;
            vertOpenFrac  = cachedVertOpen;
        } else {
            VectorCopy(s_al_listener_origin, lastProbeOrigin);

            horizTotalFrac = 0.f;
            vertTotalFrac  = 0.f;

            /* Per-ray max distances from tuning CVars (or compile-time defaults). */
            {
                float hd = s_alEnvHorizDist ? s_alEnvHorizDist->value : S_AL_SOUND_MAXDIST;
                float vd = s_alEnvVertDist  ? s_alEnvVertDist->value  : S_AL_ENV_VERT_DIST;
                float dd = s_alEnvDownDist  ? s_alEnvDownDist->value  : S_AL_ENV_DOWN_DIST;

                for (i = 0; i < S_AL_ENV_RAYS; i++) {
                    trace_t tr;
                    vec3_t  end;
                    float   rayDist = (i < 8) ? hd : ((i < 13) ? vd : dd);

                    end[0] = s_al_listener_origin[0] + s_al_envDirs[i][0] * rayDist;
                    end[1] = s_al_listener_origin[1] + s_al_envDirs[i][1] * rayDist;
                    end[2] = s_al_listener_origin[2] + s_al_envDirs[i][2] * rayDist;

                    CM_BoxTrace(&tr, s_al_listener_origin, end,
                                vec3_origin, vec3_origin, 0, CONTENTS_SOLID, qfalse);

                    if (i < 8) {
                        horizDists[i]   = tr.fraction * rayDist;
                        horizTotalFrac += tr.fraction;
                    } else {
                        /* Vertical / diagonal-up rays: shorter calibrated distances
                         * (s_alEnvVertDist / s_alEnvDownDist) make tr.fraction
                         * sensitive to realistic URT ceiling heights rather than
                         * comparing against the full 1330-unit horizontal range. */
                        vertTotalFrac += tr.fraction;
                    }
                }
            }

            /* ---- Horizontal metrics ----------------------------------------
             *
             * openFrac: plain average of all 8 ray fractions.  Gives a smooth
             * gradient between enclosed (0) and fully open (1).
             *
             * sizeFactor: average of the LONGEST 4 (upper-half) horizontal
             * distances.  This ignores nearby obstacles such as pillars —
             * even if the player is surrounded by pillars wider than the
             * player model, the diagonal rays that slip through the gaps
             * still reach the actual acoustic boundary (cavern walls, open
             * street) and correctly represent the size of the space.
             * Using the global mean would let 4 close-hitting pillar rays
             * drag the apparent room size down to "small room" level.
             *
             * corrFactor: axis-pair analysis on the 4 opposite-ray pairs
             * (rays 0↔4, 1↔5, 2↔6, 3↔7).  A TRUE corridor has exactly ONE
             * pair of opposite walls both close while all other pairs are
             * open.  Pillars create MULTIPLE close axis-pairs — explicitly
             * excluded so they do not trigger false corridor reverb.
             * -------------------------------------------------------------- */
            {
                float sorted[8];
                int   a, b;

                horizOpenFrac = horizTotalFrac / 8.f;

                /* Copy into sort buffer, insertion-sort ascending */
                for (a = 0; a < 8; a++) sorted[a] = horizDists[a];
                for (a = 1; a < 8; a++) {
                    float key = sorted[a];
                    for (b = a - 1; b >= 0 && sorted[b] > key; b--)
                        sorted[b + 1] = sorted[b];
                    sorted[b + 1] = key;
                }
                /* Top-4 (sorted[4..7]) = 4 longest rays */
                {
                    float top4Avg = (sorted[4] + sorted[5] + sorted[6] + sorted[7]) * 0.25f;
                    sizeFactor = (top4Avg - S_AL_SOUND_FULLVOLUME) /
                                 (600.f   - S_AL_SOUND_FULLVOLUME);
                    if (sizeFactor < 0.f) sizeFactor = 0.f;
                    if (sizeFactor > 1.f) sizeFactor = 1.f;
                }

                /* Axis-pair corridor detection */
                {
                    float axisDist[4];
                    int   closeAxes = 0;
                    float minClose  = S_AL_SOUND_MAXDIST;
                    float maxOpen   = 0.f;

                    /* Rays i and i+4 are exact geometric opposites */
                    for (a = 0; a < 4; a++) {
                        float d0 = horizDists[a];
                        float d1 = horizDists[a + 4];
                        axisDist[a] = (d0 < d1) ? d0 : d1;
                        if (axisDist[a] < 200.f) {
                            closeAxes++;
                            if (axisDist[a] < minClose) minClose = axisDist[a];
                        } else {
                            if (axisDist[a] > maxOpen) maxOpen = axisDist[a];
                        }
                    }
                    /* Corridor = exactly one close axis pair with open perpendicular */
                    if (closeAxes == 1 && maxOpen > minClose * 1.5f) {
                        corrFactor = (maxOpen - minClose) / maxOpen;
                        if (corrFactor > 1.f) corrFactor = 1.f;
                    } else {
                        corrFactor = 0.f;
                    }
                }
            }

            vertOpenFrac = vertTotalFrac / 6.f;  /* 14 − 8 = 6 vertical rays */

            /* ---- Horizontal/vertical openness combine ---------------------- */
            {
                float hw = s_alEnvHorizWeight ? s_alEnvHorizWeight->value : 0.70f;
                if (hw < 0.f) hw = 0.f;
                if (hw > 1.f) hw = 1.f;
                openFrac = horizOpenFrac * hw + vertOpenFrac * (1.f - hw);
            }

            /* ---- Velocity direction from this probe's movement ------------- */
            {
                float hdx = moveDelta[0], hdy = moveDelta[1];
                float hlen = hdx*hdx + hdy*hdy;
                if (hlen > 1.f) {
                    hlen = (float)sqrt((double)hlen);
                    cachedVelDir[0] = hdx / hlen;
                    cachedVelDir[1] = hdy / hlen;
                    cachedVelDir[2] = 0.f;
                    cachedVelSpeed  = moveDist;
                } else {
                    cachedVelSpeed = 0.f;
                }
            }

            cachedSize      = sizeFactor;
            cachedOpen      = openFrac;
            cachedCorr      = corrFactor;
            cachedHorizOpen = horizOpenFrac;
            cachedVertOpen  = vertOpenFrac;
            haveCache       = qtrue;
        }
    }

    /* --- Velocity look-ahead: bias openFrac toward the upcoming environment --
     *
     * When the player is moving, cast 2 extra horizontal rays in the direction
     * of travel and blend their openness reading into horizOpenFrac.  The blend
     * weight grows with speed (max s_alEnvVelWeight, default 0.30) so a fast
     * run toward an outdoor exit shifts the classifier earlier, preventing the
     * probe from stalling in the TRANSITION zone for several seconds.
     *
     * Only active on cache misses (moveDist ≥ S_AL_ENV_MOVE_THRESH), when the
     * player is actually moving.  cachedVelSpeed is 0 on cache hits or when
     * standing still, so the block is a no-op in those cases. */
    {
        float velThresh = s_alEnvVelThresh ? s_alEnvVelThresh->value : 48.f;
        float velWMax   = s_alEnvVelWeight ? s_alEnvVelWeight->value : 0.30f;

        if (cachedVelSpeed > velThresh && CM_NumInlineModels() > 0) {
            float biasSum = 0.f;
            float perpX   = -cachedVelDir[1];
            float perpY   =  cachedVelDir[0];
            float hd      = s_alEnvHorizDist ? s_alEnvHorizDist->value : S_AL_SOUND_MAXDIST;
            int   k;

            /* Ray 0: straight ahead in direction of travel.
             * Ray 1: 45° left of travel — wider spatial preview. */
            for (k = 0; k < 2; k++) {
                trace_t vtr;
                vec3_t  vend;
                float dx, dy;

                if (k == 0) {
                    dx = cachedVelDir[0];
                    dy = cachedVelDir[1];
                } else {
                    dx = cachedVelDir[0] * 0.707f + perpX * 0.707f;
                    dy = cachedVelDir[1] * 0.707f + perpY * 0.707f;
                }
                vend[0] = s_al_listener_origin[0] + dx * hd;
                vend[1] = s_al_listener_origin[1] + dy * hd;
                vend[2] = s_al_listener_origin[2];

                CM_BoxTrace(&vtr, s_al_listener_origin, vend,
                            vec3_origin, vec3_origin, 0, CONTENTS_SOLID, qfalse);
                biasSum += vtr.fraction;
            }

            {
                float biasOpen  = biasSum * 0.5f;
                float velFactor = (cachedVelSpeed - velThresh) / 200.f;
                if (velFactor < 0.f) velFactor = 0.f;
                if (velFactor > velWMax) velFactor = velWMax;

                {
                    float hw = s_alEnvHorizWeight ? s_alEnvHorizWeight->value : 0.70f;
                    if (hw < 0.f) hw = 0.f;
                    if (hw > 1.f) hw = 1.f;
                    horizOpenFrac = horizOpenFrac * (1.f - velFactor) + biasOpen * velFactor;
                    openFrac      = horizOpenFrac * hw + vertOpenFrac * (1.f - hw);
                }
            }
        }
    }

    /* --- Rolling history: average last N measurements -------------------- */
    {
        int historySize = s_alEnvHistorySize ? (int)s_alEnvHistorySize->value
                                             : S_AL_ENV_HISTORY;
        if (historySize < 1)                    historySize = 1;
        if (historySize > S_AL_ENV_HISTORY_MAX) historySize = S_AL_ENV_HISTORY_MAX;

        histSize[histIdx] = sizeFactor;
        histOpen[histIdx] = openFrac;
        histCorr[histIdx] = corrFactor;
        histIdx = (histIdx + 1) % historySize;
        if (histCount < historySize) histCount++;
        /* If the user shrank historySize, drop excess entries. */
        if (histCount > historySize) histCount = historySize;

        if (histCount > 1) {
            float sumSize = 0.f, sumOpen = 0.f, sumCorr = 0.f;
            for (i = 0; i < histCount; i++) {
                sumSize += histSize[i];
                sumOpen += histOpen[i];
                sumCorr += histCorr[i];
            }
            sizeFactor = sumSize / (float)histCount;
            openFrac   = sumOpen / (float)histCount;
            corrFactor = sumCorr / (float)histCount;
        }
    }

    /* --- Derive target EFX parameters ------------------------------------ */
    baseGain = (s_alReverbGain && s_alReverbGain->value > 0.f)
               ? s_alReverbGain->value : 0.20f;

    /* Decay: short room (0.4 s) up to medium-large space (2.5 s), killed
     * outdoors.  Max reduced from 4.0 s to 2.5 s — the longer decay was the
     * main contributor to the "muffled / smeared" complaints, especially in
     * the reverb send shared by all simultaneous layers of a compound sound. */
    targetDecay = 0.4f + sizeFactor * 2.1f;
    targetDecay *= (1.f - openFrac * 0.85f);
    if (targetDecay < 0.1f) targetDecay = 0.1f;

    /* Late reverb tail: scaled way back so it colours the space rather than
     * drowning the direct signal.  Max 0.35 (was 0.90).  Lower ceiling is
     * especially important when multiple WAV layers share the same reverb
     * slot — the tail compounds across all simultaneous sources. */
    targetLate = 0.05f + sizeFactor * 0.30f;
    targetLate *= (1.f - openFrac * 0.85f);
    if (targetLate < 0.f) targetLate = 0.f;
    if (targetLate > 1.f) targetLate = 1.f;

    /* Early reflections: corridor-boosted but overall reduced.
     * Max 0.28 (was 0.60) to avoid masking the direct signal.
     * Corridor threshold tightened (< 150 u, ratio ≥ 1.5×) so doorways
     * and arches no longer trigger full corridor reflection levels. */
    targetRefl = 0.03f + corrFactor * 0.25f;
    targetRefl *= (1.f - openFrac * 0.75f);
    if (targetRefl < 0.f) targetRefl = 0.f;
    if (targetRefl > 1.f) targetRefl = 1.f;

    /* Slot gain: scales with enclosure; outdoor → near-dry.
     * Kill factor reduced from 0.90 to 0.80 — less aggressive zero-out in
     * semi-open areas so the transition feels gradual, not switched. */
    targetSlot = baseGain * (1.f - openFrac * 0.80f);
    if (targetSlot < 0.f) targetSlot = 0.f;
    if (targetSlot > 1.f) targetSlot = 1.f;

    /* --- Smooth blend or snap -------------------------------------------- */
    if (snap) {
        curDecay = targetDecay;
        curLate  = targetLate;
        curRefl  = targetRefl;
        curSlot  = targetSlot;
    } else {
        /* Base pole from s_alEnvSmoothPole (default 0.92).
         * Reduced toward 0.85 when the player is moving — faster probe-to-probe
         * response during environment transitions.  At full walking speed the
         * half-transition time drops from ~3.5 s to ~2.2 s. */
        float basePole = s_alEnvSmoothPole ? s_alEnvSmoothPole->value : 0.92f;
        float blendPole;
        {
            float velThresh = s_alEnvVelThresh ? s_alEnvVelThresh->value : 48.f;
            float vf = (cachedVelSpeed > velThresh)
                       ? (cachedVelSpeed - velThresh) / 200.f : 0.f;
            if (vf > 1.f) vf = 1.f;
            blendPole = basePole - vf * 0.07f;
            if (blendPole < 0.5f) blendPole = 0.5f;
        }
        curDecay = curDecay * blendPole + targetDecay * (1.f - blendPole);
        curLate  = curLate  * blendPole + targetLate  * (1.f - blendPole);
        curRefl  = curRefl  * blendPole + targetRefl  * (1.f - blendPole);
        curSlot  = curSlot  * blendPole + targetSlot  * (1.f - blendPole);
    }

    /* --- Push to EFX effect and re-upload to slot ------------------------- */
    if (s_al_efx.hasEAXReverb) {
        qalEffectf(s_al_efx.reverbEffect, AL_EAXREVERB_DECAY_TIME,       curDecay);
        qalEffectf(s_al_efx.reverbEffect, AL_EAXREVERB_LATE_REVERB_GAIN, curLate);
        qalEffectf(s_al_efx.reverbEffect, AL_EAXREVERB_REFLECTIONS_GAIN, curRefl);
    } else {
        qalEffectf(s_al_efx.reverbEffect, AL_REVERB_DECAY_TIME,          curDecay);
        qalEffectf(s_al_efx.reverbEffect, AL_REVERB_LATE_REVERB_GAIN,    curLate);
        qalEffectf(s_al_efx.reverbEffect, AL_REVERB_REFLECTIONS_GAIN,    curRefl);
    }
    /* Re-attach effect to slot so parameter changes take effect */
    qalAuxiliaryEffectSloti(s_al_efx.reverbSlot, AL_EFFECTSLOT_EFFECT,
                            (ALint)s_al_efx.reverbEffect);
    qalAuxiliaryEffectSlotf(s_al_efx.reverbSlot, AL_EFFECTSLOT_GAIN, curSlot);

    /* --- Debug output ---------------------------------------------------- */
    if (s_alDebugReverb && s_alDebugReverb->integer >= 1) {
        /* Classify the averaged environment into a human-readable label. */
        const char *envLabel;
        if      (openFrac   > 0.55f)  envLabel = "OUTDOOR";
        else if (openFrac   > 0.30f)  envLabel = "TRANSITION";
        else if (corrFactor > 0.40f)  envLabel = "CORRIDOR";
        else if (sizeFactor > 0.65f)  envLabel = "LARGE_ROOM";
        else if (sizeFactor > 0.30f)  envLabel = "MEDIUM_ROOM";
        else                          envLabel = "SMALL_ROOM";

        Com_Printf(S_COLOR_CYAN "[dynreverb] env=%-11s"
                   "  size=%.2f  open=%.2f  corr=%.2f"
                   "  |  tgt: dec=%.2f lat=%.2f ref=%.2f slot=%.2f"
                   "  |  cur: dec=%.2f lat=%.2f ref=%.2f slot=%.2f\n",
                   envLabel,
                   sizeFactor, openFrac, corrFactor,
                   targetDecay, targetLate, targetRefl, targetSlot,
                   curDecay,   curLate,   curRefl,   curSlot);

        if (s_alDebugReverb->integer >= 2) {
            /* Extra line: show raw (pre-history) metrics, cache state, and
             * active tuning parameters so the user can verify cvar changes
             * took effect after an s_alReset. */
            int  historySize = s_alEnvHistorySize ? (int)s_alEnvHistorySize->value
                                                  : S_AL_ENV_HISTORY;
            if (historySize < 1) historySize = 1;
            if (historySize > S_AL_ENV_HISTORY_MAX) historySize = S_AL_ENV_HISTORY_MAX;
            Com_Printf(S_COLOR_CYAN "             raw: size=%.2f  horizOpen=%.2f  vertOpen=%.2f  corr=%.2f"
                       "  |  hist=%d/%d  vel=%.0f  cache=%s\n",
                       cachedSize, horizOpenFrac, vertOpenFrac, cachedCorr,
                       histCount, historySize, cachedVelSpeed,
                       haveCache ? "hit" : "miss");
        }
    }
}

/* Lightweight dynamic-reverb state reset, triggered by the s_alReset console
 * command.  Invalidates the history buffer, movement cache, velocity tracking,
 * and forces a "snap" on the next probe cycle so all s_alEnv* cvar changes
 * take effect immediately — no map reload or snd_restart needed. */
static void S_AL_ReverbReset_f( void )
{
    if (!s_al_started) {
        Com_Printf("S_AL: not running.\n");
        return;
    }
    s_al_reverbReset = qtrue;
    Com_Printf("S_AL: reverb state reset — will snap on next probe cycle.\n");
}

static void S_AL_Update( int msec )
{
    int   i;
    ALint state;
    float masterGain, musicGain;

    if (!s_al_started) return;

    /* Handle mute */
    masterGain = s_al_muted ? 0.0f : (s_volume ? s_volume->value : 0.8f);
    qalListenerf(AL_GAIN, masterGain);

    /* Update music volume */
    if (s_al_music.active) {
        musicGain = s_al_muted ? 0.0f :
                    (s_musicVolume ? s_musicVolume->value : 0.25f);
        qalSourcef(s_al_music.source, AL_GAIN, musicGain);
        S_AL_MusicUpdate();
    }

    /* Live-update reverb slot gain and water-transition routing */
    if (s_al_efx.available) {
        static qboolean lastInwater = qfalse;
        static float    lastReverbGain = -1.0f;

        /* s_alReverbGain can be changed at any time — push to AL slot.
         * Skip when dynamic reverb is active; it manages the slot gain. */
        if (s_alReverbGain && !(s_alDynamicReverb && s_alDynamicReverb->integer)) {
            float rg = s_alReverbGain->value;
            if (rg < 0.0f) rg = 0.0f;
            if (rg > 1.0f) rg = 1.0f;
            if (rg != lastReverbGain) {
                float slotGain = (s_alReverb && s_alReverb->integer) ? rg : 0.0f;
                qalAuxiliaryEffectSlotf(s_al_efx.reverbSlot,
                                        AL_EFFECTSLOT_GAIN, slotGain);
                lastReverbGain = rg;
            }
        }

        /* Dynamic acoustic environment: updates EFX params + slot gain */
        S_AL_UpdateDynamicReverb();

        /* Switch all non-local sources between normal reverb and underwater effect */
        if (s_al_inwater != lastInwater) {
            int j;
            ALuint slot = s_al_inwater ? s_al_efx.underwaterSlot : s_al_efx.reverbSlot;
            for (j = 0; j < s_al_numSrc; j++) {
                if (!s_al_src[j].isPlaying || s_al_src[j].isLocal) continue;
                if (s_alReverb && !s_alReverb->integer)
                    slot = (ALuint)AL_EFFECTSLOT_NULL;
                qalSource3i(s_al_src[j].source, AL_AUXILIARY_SEND_FILTER,
                            (ALint)slot, 0, (ALint)AL_FILTER_NULL);
            }
            lastInwater = s_al_inwater;
        }
    }

    /* Live-update per-category volume when s_alVolSelf/Other/Env/UI change.
     * Runs regardless of EFX availability so that own-player (local) source
     * gains respond to vol cvar changes even when s_alEFX=0.  When EFX is
     * off, Phase 3 of the occlusion block already writes gain every frame for
     * non-local sources, but local sources (isLocal=true) skip Phase 3 and
     * only get their gain set once in SrcSetup — without this block, changing
     * s_alVolSelf at runtime would have no effect on local sources. */
    {
        static float lastVolSelf  = -1.f;
        static float lastVolOther = -1.f;
        static float lastVolEnv   = -1.f;
        static float lastVolUI    = -1.f;
        float vSelf  = S_AL_GetCategoryVol(SRC_CAT_LOCAL);
        float vOther = S_AL_GetCategoryVol(SRC_CAT_WORLD);
        float vEnv   = S_AL_GetCategoryVol(SRC_CAT_AMBIENT);
        float vUI    = S_AL_GetCategoryVol(SRC_CAT_UI);

        if (vSelf != lastVolSelf || vOther != lastVolOther ||
            vEnv  != lastVolEnv  || vUI    != lastVolUI) {
            int j;
            for (j = 0; j < s_al_numSrc; j++) {
                float catGain;
                if (!s_al_src[j].isPlaying) continue;
                catGain = S_AL_GetCategoryVol(s_al_src[j].category);
                /* Re-apply per-sample normalisation for ambient sources */
                if (s_al_src[j].category == SRC_CAT_AMBIENT &&
                        s_al_src[j].sfx >= 0 && s_al_src[j].sfx < s_al_numSfx)
                    catGain *= s_al_sfx[s_al_src[j].sfx].normGain;
                qalSourcef(s_al_src[j].source, AL_GAIN,
                    (s_al_src[j].master_vol / 255.f) * catGain);
            }
            lastVolSelf  = vSelf;
            lastVolOther = vOther;
            lastVolEnv   = vEnv;
            lastVolUI    = vUI;
        }
    }

    /* Reap stopped sources — mark them free immediately so the next
     * S_AL_GetFreeSource pass 1 finds them without growing or stealing. */
    for (i = 0; i < s_al_numSrc; i++) {
        if (!s_al_src[i].isPlaying || s_al_src[i].loopSound) continue;

        qalGetSourcei(s_al_src[i].source, AL_SOURCE_STATE, &state);
        if (state == AL_STOPPED) {
            /* Level 2: log every natural completion so we can confirm the full
             * buffer was consumed (offset should equal soundLength). */
            if (s_alDebugPlayback && s_alDebugPlayback->integer >= 2) {
                ALint       offset = 0;
                const char *name   = "(unknown)";
                int         total  = 0;
                int         fileHz = 0;

                qalGetSourcei(s_al_src[i].source, AL_SAMPLE_OFFSET, &offset);
                if (s_al_src[i].sfx >= 0 && s_al_src[i].sfx < s_al_numSfx) {
                    name   = s_al_sfx[s_al_src[i].sfx].name;
                    total  = s_al_sfx[s_al_src[i].sfx].soundLength;
                    fileHz = s_al_sfx[s_al_src[i].sfx].fileRate;
                }
                Com_Printf(S_COLOR_GREEN
                    "[alDbg] done  %-40s  played %d / %d smp (%.0f%%)  "
                    "file=%d Hz  device=%d Hz\n",
                    name, offset, total,
                    (total > 0) ? (100.0f * offset / total) : 0.0f,
                    fileHz, (int)s_al_deviceFreq);
            }
            qalSourcei(s_al_src[i].source, AL_BUFFER, 0);
            s_al_src[i].isPlaying = qfalse;
            continue;
        }
        /* Occlusion: three-phase update for smooth wall↔clear transitions.
         *
         * Phase 1 (trace, on interval): measure occlusion and set target.
         *   Full-vol zone / weapon-muzzle zone / occlusion off → snap target=1.0.
         *   Otherwise: run CM_BoxTrace every interval frames (1/4/8 by distance),
         *   update occlusionTarget and acousticOffset (partial gap redirect).
         *
         * Phase 2 (smooth, every frame): IIR blend occlusionGain → occlusionTarget.
         *   Opening (gain rising): step=0.30 — fast snap on line-of-sight regain.
         *   Closing (gain falling): step=0.18 — gradual muffle, no abrupt pop.
         *
         * Phase 3 (apply, every frame): push position + filter using smoothed values.
         *   Position = origin + acousticOffset (tracks the entity; gap hint is
         *              relative so it follows moving players automatically).
         *   LOWPASS_GAIN   = s_alOccGainFloor + (1-floor)*occ
         *   LOWPASS_GAINHF = s_alOccHFFloor   + (1-floor)*occ
         *   Both tunable live; set s_alDebugOcc 1 to print per-source state.
         *
         * Trace intervals (same thresholds as before):
         *   < 80 u or CHAN_WEAPON < weapDist : interval=0 (full-vol / muzzle zone)
         *   80 – 300 u : every frame
         *   300 – 600 u: every 4 frames
         *   > 600 u    : every 8 frames
         */
        if (!s_al_src[i].isLocal) {
            vec3_t diff;
            float  distSq;
            int    interval;
            float  weapDist;

            VectorSubtract(s_al_src[i].origin, s_al_listener_origin, diff);
            distSq   = DotProduct(diff, diff);
            weapDist = s_alOccWeaponDist ? s_alOccWeaponDist->value
                                         : S_AL_WEAPON_NOOCC_DIST;

            if (distSq < (S_AL_SOUND_FULLVOLUME * S_AL_SOUND_FULLVOLUME)) {
                /* Full-volume zone: wall cannot separate source from listener. */
                interval = 0;
            } else if (distSq < (weapDist * weapDist) &&
                       s_al_src[i].entchannel == CHAN_WEAPON) {
                /* Gun-muzzle zone: bypass occlusion for CHAN_WEAPON sources.
                 * Covers muzzle-flash origin offsets (~50-100 u ahead of the
                 * player eye) that would otherwise trigger false filter
                 * attenuation from nearby slope/clip geometry. */
                interval = 0;
            } else if (distSq < (300.f * 300.f)) {
                /* Close range — trace every frame for nearby player accuracy. */
                interval = 1;
            } else if (distSq < (600.f * 600.f)) {
                /* Medium range — trace every 4 frames. */
                interval = 4;
            } else {
                /* Far range — distance model dominates; trace every 8 frames. */
                interval = 8;
            }

            /* ---- Phase 1: trace on interval ---- */
            if (interval == 0 || !(s_alOcclusion && s_alOcclusion->integer)) {
                /* Full-vol zone OR occlusion disabled: snap target to clear,
                 * zero out any position redirect. */
                s_al_src[i].occlusionTarget = 1.0f;
                if (interval == 0)
                    s_al_src[i].occlusionTick = s_al_loopFrame;
                VectorClear(s_al_src[i].acousticOffset);
            } else if ((s_al_loopFrame - s_al_src[i].occlusionTick) >= interval) {
                vec3_t acousticPos;
                float  occ    = S_AL_ComputeAcousticPosition(
                                    s_al_src[i].origin, acousticPos);
                /* Scale position blend by how much the path is blocked.
                 * s_alOccPosBlend=0.25: max 25% redirect when fully blocked,
                 * near-zero when nearly clear — preserves directional accuracy. */
                float  pBlend = (s_alOccPosBlend ? s_alOccPosBlend->value : 0.25f)
                                * (1.0f - occ);
                s_al_src[i].occlusionTick     = s_al_loopFrame;
                s_al_src[i].occlusionTarget   = occ;
                /* Store as offset from real origin so it tracks moving entities */
                s_al_src[i].acousticOffset[0] = pBlend *
                    (acousticPos[0] - s_al_src[i].origin[0]);
                s_al_src[i].acousticOffset[1] = pBlend *
                    (acousticPos[1] - s_al_src[i].origin[1]);
                s_al_src[i].acousticOffset[2] = pBlend *
                    (acousticPos[2] - s_al_src[i].origin[2]);
            }

            /* ---- Phase 2: IIR smooth occlusionGain toward target ---- */
            {
                float target  = s_al_src[i].occlusionTarget;
                float current = s_al_src[i].occlusionGain;
                /* Opening responds faster (snap on corner clear);
                 * closing is more gradual (no abrupt muffle pop). */
                float step    = (target > current) ? 0.30f : 0.18f;
                s_al_src[i].occlusionGain = current + (target - current) * step;
            }

            /* ---- Phase 3: apply position + filter every frame ---- */
            {
                float occ       = s_al_src[i].occlusionGain;
                float gainFloor = s_alOccGainFloor ? s_alOccGainFloor->value : 0.55f;
                float hfFloor   = s_alOccHFFloor   ? s_alOccHFFloor->value   : 0.50f;
                /* Linear floor→1.0 curves.  gainFloor controls how much overall
                 * volume remains when fully blocked; hfFloor controls how much
                 * high-frequency content survives (higher = less "muffled").
                 * Old hardcoded: GAIN=0.25+0.75*occ, GAINHF=occ*occ.
                 * Defaults (0.55 / 0.50) keep weapon-fire timbre recognisable
                 * and substantially reduce the wall-to-clear volume jump. */
                float gain   = gainFloor + (1.0f - gainFloor) * occ;
                float gainHF = hfFloor   + (1.0f - hfFloor)   * occ;
                vec3_t pos;

                /* Position: real origin + gap-hint offset (tracks moving entities) */
                pos[0] = s_al_src[i].origin[0] + s_al_src[i].acousticOffset[0];
                pos[1] = s_al_src[i].origin[1] + s_al_src[i].acousticOffset[1];
                pos[2] = s_al_src[i].origin[2] + s_al_src[i].acousticOffset[2];
                qalSource3f(s_al_src[i].source, AL_POSITION,
                            pos[0], pos[1], pos[2]);

                if (s_al_efx.available) {
                    if (occ > 0.98f) {
                        /* Fully clear: remove the lowpass from the signal path
                         * entirely.  Even a "passthrough" biquad filter object
                         * can introduce subtle phase-shift coloration that makes
                         * non-occluded weapon fire sound tinny / bass-light
                         * compared to vanilla DMA.  AL_FILTER_NULL bypasses the
                         * filter stage completely, restoring the raw signal. */
                        qalSourcei(s_al_src[i].source, AL_DIRECT_FILTER,
                                   (ALint)AL_FILTER_NULL);
                    } else {
                        qalFilterf(s_al_efx.occlusionFilter[i],
                                   AL_LOWPASS_GAIN,   gain);
                        qalFilterf(s_al_efx.occlusionFilter[i],
                                   AL_LOWPASS_GAINHF, gainHF);
                        qalSourcei(s_al_src[i].source, AL_DIRECT_FILTER,
                                   (ALint)s_al_efx.occlusionFilter[i]);
                    }
                } else {
                    /* No EFX: approximate occlusion by direct gain reduction.
                     * Do NOT multiply by masterGain here — the listener AL_GAIN
                     * already applies s_volume; including it again would double it. */
                    qalSourcef(s_al_src[i].source, AL_GAIN,
                        (s_al_src[i].master_vol / 255.0f) *
                        S_AL_GetCategoryVol(s_al_src[i].category) *
                        occ);
                }

                if (s_alDebugOcc && s_alDebugOcc->integer && occ < 0.99f) {
                    Com_Printf(S_COLOR_CYAN
                        "[alOcc] ent=%4d dist=%5.0f  "
                        "tgt=%.2f cur=%.2f  G=%.2f GHF=%.2f\n",
                        s_al_src[i].entnum,
                        sqrtf(distSq),
                        s_al_src[i].occlusionTarget,
                        occ, gain, gainHF);
                }
            }
        }
    }

    /* Update looping sounds.
     *
     * IMPORTANT: s_al_loopFrame is incremented AFTER UpdateLoops so that
     * the frame number stored by AddLoopingSound (called from cgame before
     * S_Update) matches the frame number checked inside UpdateLoops.
     * Incrementing before UpdateLoops caused every looping sound to fail
     * the "lp->framenum != s_al_loopFrame" staleness check and be killed
     * on every single frame. */
    S_AL_UpdateLoops();
    ++s_al_loopFrame;
}

static void S_AL_DisableSounds( void )
{
    S_AL_StopAllSounds();
    s_al_muted = qtrue;
    Com_DPrintf("S_AL: DisableSounds -- muted until next BeginRegistration\n");
}

static void S_AL_BeginRegistration( void )
{
    s_al_muted = qfalse;

    /* Slot 0 must be reserved as the default/failure placeholder on every
     * registration cycle — cold start, map→menu, map→map.  The QVM checks
     * (handle > 0) to detect a successful S_RegisterSound; if slot 0 is not
     * pre-occupied the first real sound lands there, the QVM discards it as
     * a failure, and the menu music / intro never starts, causing a hard lock.
     *
     * When s_al_numSfx == 0 (cold start) this registers the sound fresh.
     * When s_al_numSfx > 0 (returning from a map) S_AL_FindSfx finds the
     * existing record immediately and returns handle 0 — no reload, no cost. */
    S_AL_RegisterSound( "sound/feedback/hit.wav", qfalse );

    Com_DPrintf("S_AL: BeginRegistration -- slot 0 reserved\n");
}

static void S_AL_ClearSoundBuffer( void )
{
    /* Stop all transient sources, keep sfx buffers loaded. */
    int i;
    for (i = 0; i < s_al_numSrc; i++) {
        if (!s_al_src[i].loopSound)
            S_AL_StopSource(i);
    }
}

/* Print a clean numbered list of available OpenAL output devices so that
 * players can easily identify the value to put in s_alDevice.
 *
 *   >*  1.  Realtek High Definition Audio    <- system default, currently active
 *       2.  Razer Kraken 7.1 Chroma
 *       3.  HDMI Output (LG Monitor)
 *
 * Legend: '>' = currently active   '*' = system default
 *
 * Device names form a contiguous multi-string: each entry is NUL-terminated
 * and the list ends with an empty entry (double NUL). */
static void S_AL_PrintDevices( void )
{
    const ALCchar *devList;
    const ALCchar *defDev;
    const ALCchar *curDev;
    const ALCchar *p;
    int            total, n;
    qboolean       hasEnumAll;

    hasEnumAll = qalcIsExtensionPresent(NULL, "ALC_ENUMERATE_ALL_EXT");

    if (hasEnumAll) {
        devList = qalcGetString(NULL, ALC_ALL_DEVICES_SPECIFIER);
        defDev  = qalcGetString(NULL, ALC_DEFAULT_ALL_DEVICES_SPECIFIER);
    } else {
        devList = qalcGetString(NULL, ALC_DEVICE_SPECIFIER);
        defDev  = qalcGetString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER);
    }

    curDev = s_al_device
             ? qalcGetString(s_al_device, ALC_DEVICE_SPECIFIER) : NULL;

    if (!devList || !devList[0]) {
        Com_Printf("  S_AL: no audio devices enumerated by OpenAL.\n");
        return;
    }

    /* Count devices first for the header */
    total = 0;
    for (p = devList; *p; p += strlen(p) + 1)
        total++;

    Com_Printf("  Audio output devices  (> = active  * = system default):\n");

    for (p = devList, n = 1; *p; p += strlen(p) + 1, n++) {
        qboolean isDefault = (defDev && defDev[0] && !Q_stricmp(defDev, p));
        qboolean isActive  = (curDev && curDev[0] && !Q_stricmp(curDev, p));

        Com_Printf("    %c%c %2d.  %s\n",
            isActive  ? '>' : ' ',
            isDefault ? '*' : ' ',
            n, p);
    }

    Com_Printf("  Usage: set s_alDevice <name-or-number> ; vid_restart\n");
    Com_Printf("         (empty string restores the system default)\n");
}

/* Console command: /s_devices — prints the device list without any other
 * sndinfo output, making it easy to find the right s_alDevice value. */
static void S_AL_ListDevicesCmd( void )
{
    if (!s_al_started) {
        Com_Printf("S_AL: OpenAL backend not active\n");
        return;
    }
    Com_Printf("\n");
    S_AL_PrintDevices();
    Com_Printf("\n");
}

static void S_AL_SoundInfo( void )
{
    ALCint hrtfStatus = 0;
    const ALchar *vendor   = qalGetString(AL_VENDOR);
    const ALchar *version  = qalGetString(AL_VERSION);
    const ALchar *renderer = qalGetString(AL_RENDERER);

    /* Determine actual live HRTF state */
    if (qalcIsExtensionPresent(s_al_device, "ALC_SOFT_HRTF"))
        qalcGetIntegerv(s_al_device, ALC_HRTF_STATUS_SOFT, 1, &hrtfStatus);

    Com_Printf("\n");
    Com_Printf("OpenAL sound backend\n");
    Com_Printf("  Vendor:   %s\n", vendor   ? vendor   : "unknown");
    Com_Printf("  Version:  %s\n", version  ? version  : "unknown");
    Com_Printf("  Renderer: %s\n", renderer ? renderer : "unknown");
    Com_Printf("  Sources:  %d / %d active  |  Sfx: %d loaded\n",
        s_al_numSrc, S_AL_MAX_SRC, s_al_numSfx);
    Com_Printf("\n");

    /* -----------------------------------------------------------------------
     * Processing feature status — use this to isolate which feature is
     * causing audio issues.  Each line shows whether the feature is truly
     * active or bypassed at this moment.  Restart (vid_restart) is required
     * after changing any LATCH cvar before re-testing.
     * ----------------------------------------------------------------------- */
    Com_Printf("  Processing features (ON = active, OFF = bypassed):\n");

    /* HRTF — reports actual hardware/driver state, not just the cvar */
    if (qalcIsExtensionPresent(s_al_device, "ALC_SOFT_HRTF")) {
        qboolean hrtfOn = (hrtfStatus == ALC_HRTF_ENABLED_SOFT);
        Com_Printf("    HRTF              %s  (s_alHRTF %d, LATCH)%s\n",
            hrtfOn ? S_COLOR_RED "ON " S_COLOR_WHITE
                   : S_COLOR_GREEN "OFF" S_COLOR_WHITE,
            s_alHRTF ? s_alHRTF->integer : 0,
            hrtfOn && !(s_alHRTF && s_alHRTF->integer)
                ? S_COLOR_YELLOW " WARNING: on despite cvar=0" S_COLOR_WHITE : "");
    } else {
        Com_Printf("    HRTF              " S_COLOR_GREEN "OFF" S_COLOR_WHITE
                   "  (ALC_SOFT_HRTF not available)\n");
    }

    /* EFX — reports whether filter objects / reverb slots were created */
    Com_Printf("    EFX subsystem     %s  (s_alEFX %d, LATCH  — %s reverb)\n",
        s_al_efx.available ? S_COLOR_YELLOW "ON " S_COLOR_WHITE
                           : S_COLOR_GREEN "OFF" S_COLOR_WHITE,
        s_alEFX ? s_alEFX->integer : 1,
        s_al_efx.hasEAXReverb ? "EAX" : "standard");

    /* Reverb — only meaningful when EFX is on */
    Com_Printf("    EFX reverb        %s  (s_alReverb %d)\n",
        (s_al_efx.available && s_alReverb && s_alReverb->integer)
            ? S_COLOR_YELLOW "ON " S_COLOR_WHITE
            : S_COLOR_GREEN "OFF" S_COLOR_WHITE,
        s_alReverb ? s_alReverb->integer : 0);

    /* Dynamic reverb */
    Com_Printf("    Dynamic reverb    %s  (s_alDynamicReverb %d)\n",
        (s_al_efx.available && s_alReverb && s_alReverb->integer &&
         s_alDynamicReverb && s_alDynamicReverb->integer)
            ? S_COLOR_YELLOW "ON " S_COLOR_WHITE
            : S_COLOR_GREEN "OFF" S_COLOR_WHITE,
        s_alDynamicReverb ? s_alDynamicReverb->integer : 0);

    /* Occlusion — traces + lowpass filter or gain reduction per source */
    Com_Printf("    Occlusion         %s  (s_alOcclusion %d)\n",
        (s_alOcclusion && s_alOcclusion->integer)
            ? S_COLOR_YELLOW "ON " S_COLOR_WHITE
            : S_COLOR_GREEN "OFF" S_COLOR_WHITE,
        s_alOcclusion ? s_alOcclusion->integer : 0);

    /* AL_DIRECT_CHANNELS_SOFT — bypasses HRTF on local sources (active only when s_alHRTF 1) */
    if (!s_al_directChannelsExt) {
        Com_Printf("    DirectChannels    " S_COLOR_YELLOW "OFF" S_COLOR_WHITE
                   "  (AL_SOFT_direct_channels not available)\n");
    } else {
        const char *reason = "";
        if (!s_al_directChannels) {
            if (s_alDirectChannels && !s_alDirectChannels->integer)
                reason = "  (disabled by s_alDirectChannels 0)";
            else if (!s_alHRTF || !s_alHRTF->integer)
                reason = "  (inactive — s_alHRTF 0)";
        }
        Com_Printf("    DirectChannels    %s  (s_alDirectChannels %d, s_alHRTF %d, LATCH)%s\n",
            s_al_directChannels ? S_COLOR_GREEN "ON " S_COLOR_WHITE
                                : S_COLOR_YELLOW "OFF" S_COLOR_WHITE,
            s_alDirectChannels ? s_alDirectChannels->integer : 1,
            s_alHRTF          ? s_alHRTF->integer          : 0,
            reason);
    }

    Com_Printf("\n");

    /* Quick passthrough verdict */
    {
        qboolean hrtfOn  = (hrtfStatus == ALC_HRTF_ENABLED_SOFT);
        qboolean passthrough =
            !hrtfOn &&
            !s_al_efx.available &&
            !(s_alOcclusion && s_alOcclusion->integer);
        if (passthrough) {
            Com_Printf("  Passthrough check: %s\n",
                S_COLOR_GREEN "PASS — HRTF off, EFX off, occlusion off.  "
                "Signal path is: PCM → distance model → stereo pan → output." S_COLOR_WHITE);
            if (s_al_directChannels)
                Com_Printf("    (DirectChannels ON: own-player sources skip distance model too"
                           " — PCM → stereo output directly.)\n");
        } else {
            Com_Printf("  Passthrough check: %s\n",
                S_COLOR_YELLOW "PARTIAL — one or more features still active (see above)" S_COLOR_WHITE);
        }
    }

    Com_Printf("\n");
    Com_Printf("  Volume groups:\n");
    Com_Printf("    s_alVolSelf   %.2f  (own player/weapon/breath, max 1.5)\n",
        s_alVolSelf  ? s_alVolSelf->value  : 1.0f);
    Com_Printf("    s_alVolOther  %.2f  (other players/ents,       max 1.0)\n",
        s_alVolOther ? s_alVolOther->value : 0.7f);
    Com_Printf("    s_alVolEnv    %.2f  (ambient/looping,          max 1.0)\n",
        s_alVolEnv   ? s_alVolEnv->value   : 0.3f);
    Com_Printf("    s_alVolUI     %.2f  (hit/kill/UI sounds,       max 1.0)\n",
        s_alVolUI    ? s_alVolUI->value    : 0.8f);
    Com_Printf("    s_musicVolume %.2f  (map background music)\n",
        s_musicVolume ? s_musicVolume->value : 0.25f);
    Com_Printf("    s_alReverbGain    %.2f  (reverb wet level)\n",
        s_alReverbGain  ? s_alReverbGain->value  : 0.20f);
    Com_Printf("    s_alReverbDecay   %.2f s  (decay, LATCH)\n",
        s_alReverbDecay ? s_alReverbDecay->value : 1.49f);
    Com_Printf("    s_alMaxDist       %.0f  (attenuation distance)\n",
        s_alMaxDist ? s_alMaxDist->value : 1330.f);

    {
        ALCint ambiOrder = 0;
        if (qalcIsExtensionPresent(s_al_device, "ALC_SOFT_output_mode"))
            qalcGetIntegerv(s_al_device, ALC_MAX_AMBISONIC_ORDER_SOFT, 1, &ambiOrder);
        if (ambiOrder > 0)
            Com_Printf("  Ambisonics: max order %d supported (not used)\n", ambiOrder);
    }

    /* Device list — shows valid values for s_alDevice */
    S_AL_PrintDevices();
}

static void S_AL_SoundList( void )
{
    int i;
    int total = 0;

    for (i = 0; i < s_al_numSfx; i++) {
        if (!s_al_sfx[i].inMemory) continue;
        Com_Printf("%6i : %s\n", s_al_sfx[i].soundLength, s_al_sfx[i].name);
        total++;
    }
    Com_Printf("-----%d sounds------\n", total);
}

/* =========================================================================
 * Section 11 — Initialisation entry point
 * =========================================================================
 */

/*
 * S_AL_Init
 *
 * Load the OpenAL library, open a device, create a context (with HRTF when
 * ALC_SOFT_HRTF is available), allocate the source pool, and fill *si with
 * our function table.  Returns qtrue on success, qfalse if OpenAL is not
 * available (the caller should fall back to the dmaHD backend).
 */
qboolean S_AL_Init( soundInterface_t *si )
{
    int        i;
    const char *device;
    ALCint     ctxAttribs[16];
    int        nAttribs = 0;
    ALCint     hrtfStatus = 0;

    if (!si) return qfalse;

    /* Register cvars */
    s_alDevice  = Cvar_Get("s_alDevice",  "",    CVAR_ARCHIVE_ND | CVAR_LATCH);
    Cvar_SetDescription(s_alDevice, "OpenAL output device. Empty = system default. Type /s_devices for a numbered list of available devices. (LATCH — requires vid_restart to take effect)");
    s_alHRTF    = Cvar_Get("s_alHRTF",   "0",   CVAR_ARCHIVE_ND | CVAR_LATCH);
    Cvar_CheckRange(s_alHRTF, "0", "1", CV_INTEGER);
    Cvar_SetDescription(s_alHRTF, "Enable HRTF (Head-Related Transfer Function) 3D audio via OpenAL Soft. Default 0 (off). Only enable when using headphones — on speakers HRTF smears weapon transients into a muffled pop via the centre-HRIR convolution. Requires vid_restart (LATCH).");
    s_alEFX = Cvar_Get("s_alEFX", "1", CVAR_ARCHIVE_ND | CVAR_LATCH);
    Cvar_CheckRange(s_alEFX, "0", "1", CV_INTEGER);
    Cvar_SetDescription(s_alEFX, "Enable ALC_EXT_EFX (occlusion filters, reverb slots). "
        "0 = skip EFX init entirely; no filter objects or aux sends on any source. "
        "Use to isolate EFX as a cause of audio issues. Requires vid_restart (LATCH).");
    s_alDirectChannels = Cvar_Get("s_alDirectChannels", "1", CVAR_ARCHIVE_ND | CVAR_LATCH);
    Cvar_CheckRange(s_alDirectChannels, "0", "1", CV_INTEGER);
    Cvar_SetDescription(s_alDirectChannels,
        "Enable AL_SOFT_direct_channels for own-player (local) sources when s_alHRTF 1. "
        "When 1 (default), own-weapon and footstep sounds are routed directly to the "
        "stereo mix, bypassing the HRTF centre-HRIR convolution — preserving the "
        "transient punch of weapon sounds (e.g. de.wav) for headphone users. "
        "Has no effect when s_alHRTF 0 (default — HRTF off). "
        "Set to 0 to route local sources through the full HRTF pipeline. "
        "Requires vid_restart (LATCH).");
    /* s_alMaxDist: default matches the vanilla dma max-audible distance.
     * Values below 1330 are clamped to 1330 by the distance-model setup. */
    s_alMaxDist = Cvar_Get("s_alMaxDist", "1330", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alMaxDist, "1330", "4096", CV_FLOAT);
    Cvar_SetDescription(s_alMaxDist, "Maximum attenuation distance for OpenAL sound sources (game units). Matches vanilla dma range of 1330 at default.");
    /* Reverb is OFF by default to match vanilla dma (no reverb). */
    s_alReverb = Cvar_Get("s_alReverb", "0", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alReverb, "0", "1", CV_INTEGER);
    Cvar_SetDescription(s_alReverb, "Enable EFX environmental reverb (requires ALC_EXT_EFX). Default 0 matches vanilla dma behaviour.");
    s_alReverbGain = Cvar_Get("s_alReverbGain", "0.20", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alReverbGain, "0", "1", CV_FLOAT);
    Cvar_SetDescription(s_alReverbGain, "EFX reverb auxiliary slot gain (wet level). Lower values reduce echo relative to direct sound. Acts as the ceiling when s_alDynamicReverb is on.");
    s_alReverbDecay = Cvar_Get("s_alReverbDecay", "1.49", CVAR_ARCHIVE_ND | CVAR_LATCH);
    Cvar_CheckRange(s_alReverbDecay, "0.1", "10.0", CV_FLOAT);
    Cvar_SetDescription(s_alReverbDecay, "EFX reverb decay time in seconds. Used when s_alDynamicReverb 0. Requires map reload (LATCH).");
    /* Dynamic reverb is OFF by default (opt-in feature, not vanilla). */
    s_alDynamicReverb = Cvar_Get("s_alDynamicReverb", "0", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alDynamicReverb, "0", "1", CV_INTEGER);
    Cvar_SetDescription(s_alDynamicReverb, "Ray-traced acoustic environment detection. Casts 14 probes from the listener every 16 frames to measure room size, corridor shape, and openness, then adjusts EFX reverb decay/reflections accordingly. Default 0.");
    s_alDebugReverb = Cvar_Get("s_alDebugReverb", "0", CVAR_TEMP);
    Cvar_CheckRange(s_alDebugReverb, "0", "2", CV_INTEGER);
    Cvar_SetDescription(s_alDebugReverb, "Dynamic reverb debug output. 1 = print env label + smoothed EFX params every probe cycle. 2 = also print raw probe metrics and filter-reset events. Default 0 (off). Not archived.");
    s_alOcclusion = Cvar_Get("s_alOcclusion", "1", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alOcclusion, "0", "1", CV_INTEGER);
    Cvar_SetDescription(s_alOcclusion, "Per-source occlusion tracing. Sounds behind walls are gain-attenuated and their apparent HRTF direction is corrected to the nearest audible gap. Close sources (<300 u) update every frame; distant sources update less often. Default 1.");
    /* Volume group controls.  s_alVolOther and s_alVolEnv are hard-capped at
     * 1.0 (you can only turn them DOWN, not amplify other players/ambience
     * beyond their default level — anti-cheat / anti-wallhack measure).
     * s_alVolSelf allows a modest boost to 1.5 since that only affects the
     * local player's own sounds. */
    s_alVolSelf = Cvar_Get("s_alVolSelf", "1.0", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alVolSelf, "0", "1.5", CV_FLOAT);
    Cvar_SetDescription(s_alVolSelf, "Own player / weapon / breath volume multiplier [0.0-1.5]. Default 1.0. Modest boost allowed since this only affects your own sounds.");
    s_alVolOther = Cvar_Get("s_alVolOther", "0.7", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alVolOther, "0", "1.0", CV_FLOAT);
    Cvar_SetDescription(s_alVolOther, "Other player / entity one-shot volume multiplier [0.0-1.0]. Default 0.7. Capped at 1.0 — can turn down but not amplify (anti-cheat). Reduce further on maps with very loud triggered sounds.");
    s_alVolEnv = Cvar_Get("s_alVolEnv", "0.3", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alVolEnv, "0", "1.0", CV_FLOAT);
    Cvar_SetDescription(s_alVolEnv, "Looping ambient / environmental sound volume multiplier [0.0-1.0]. Default 0.3. Per-sample RMS normalisation is applied automatically. Reduce further on maps with particularly loud ambient loops (e.g. ut4_turnpike).");
    s_alVolUI = Cvar_Get("s_alVolUI", "0.8", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alVolUI, "0", "1.0", CV_FLOAT);
    Cvar_SetDescription(s_alVolUI, "Hit-marker / kill-confirmation / UI sound volume multiplier [0.0-1.0]. Default 0.8. Controls non-positional local sounds (StartLocalSound) independently of own-weapon volume (s_alVolSelf).");
    s_alLocalSelf = Cvar_Get("s_alLocalSelf", "0", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alLocalSelf, "0", "1", CV_INTEGER);
    Cvar_SetDescription(s_alLocalSelf, "Force own-player sounds (footsteps, weapon, breath) to be non-spatialized regardless of any world-space origin supplied by the game. Prevents stale-position artefacts caused by URT jumping/sliding physics. Default 0.");

    /* Acoustic-environment tuning — all live-tunable; type /s_alReset after
     * changing to hear the effect without a map reload. */
    s_alEnvHorizDist = Cvar_Get("s_alEnvHorizDist", "1330", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alEnvHorizDist, "200", "4096", CV_FLOAT);
    Cvar_SetDescription(s_alEnvHorizDist, "Dynamic reverb: max distance for the 8 horizontal probe rays (game units). Larger values make open spaces sound more open. Default 1330 = full attenuation range.");
    s_alEnvVertDist = Cvar_Get("s_alEnvVertDist", "400", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alEnvVertDist, "50", "2000", CV_FLOAT);
    Cvar_SetDescription(s_alEnvVertDist, "Dynamic reverb: max distance for diagonal-up and straight-up probe rays (game units). Calibrated for URT indoor ceiling heights (160-280 u). Default 400.");
    s_alEnvDownDist = Cvar_Get("s_alEnvDownDist", "160", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alEnvDownDist, "50", "1000", CV_FLOAT);
    Cvar_SetDescription(s_alEnvDownDist, "Dynamic reverb: max distance for the straight-down probe ray (game units). Detects pits and voids below the player. Default 160.");
    s_alEnvHorizWeight = Cvar_Get("s_alEnvHorizWeight", "0.70", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alEnvHorizWeight, "0", "1", CV_FLOAT);
    Cvar_SetDescription(s_alEnvHorizWeight, "Dynamic reverb: fraction of openFrac contributed by the 8 horizontal rays. Remaining (1-value) comes from vertical rays. Default 0.70 — overhead obstructions are less acoustically dominant than surrounding walls.");
    s_alEnvSmoothPole = Cvar_Get("s_alEnvSmoothPole", "0.92", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alEnvSmoothPole, "0.5", "0.99", CV_FLOAT);
    Cvar_SetDescription(s_alEnvSmoothPole, "Dynamic reverb: IIR smoothing pole for EFX parameter blending [0.5-0.99]. Higher = slower, smoother transitions; lower = snappier. 0.92 ~= 3.5 s half-life at 60 fps. Automatically reduced up to 0.07 below this when the player is moving fast.");
    s_alEnvHistorySize = Cvar_Get("s_alEnvHistorySize", "3", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alEnvHistorySize, "1", "8", CV_INTEGER);
    Cvar_SetDescription(s_alEnvHistorySize, "Dynamic reverb: rolling history window size [1-8]. Larger values smooth out doorway/threshold flicker at the cost of slower environment transitions. Default 3.");
    s_alEnvVelThresh = Cvar_Get("s_alEnvVelThresh", "48", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alEnvVelThresh, "8", "400", CV_FLOAT);
    Cvar_SetDescription(s_alEnvVelThresh, "Dynamic reverb: player speed threshold (game units per 16-frame probe cycle, ~180 u/s at 60fps) to activate velocity look-ahead rays. Below this, look-ahead is disabled. Default 48.");
    s_alEnvVelWeight = Cvar_Get("s_alEnvVelWeight", "0.30", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alEnvVelWeight, "0", "0.5", CV_FLOAT);
    Cvar_SetDescription(s_alEnvVelWeight, "Dynamic reverb: maximum blend weight of the 2 look-ahead rays (in direction of travel) into openFrac when the player is moving. Higher values make environment transitions respond faster to movement direction. Default 0.30.");
    s_alOccWeaponDist = Cvar_Get("s_alOccWeaponDist", "160", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alOccWeaponDist, "80", "400", CV_FLOAT);
    Cvar_SetDescription(s_alOccWeaponDist, "Occlusion: CHAN_WEAPON sources within this distance of the listener bypass occlusion tracing entirely. Covers gun-muzzle origin offsets (~50-100 u) and prevents PLAYERCLIP slope geometry from muffling your own weapon fire. Default 160.");

    /* Occlusion filter tuning CVars.
     * All live-tunable — type /s_alReset after changing to hear the effect.
     * Use s_alDebugOcc 1 to print per-source state each frame while testing. */
    s_alOccGainFloor = Cvar_Get("s_alOccGainFloor", "0.55", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alOccGainFloor, "0", "1", CV_FLOAT);
    Cvar_SetDescription(s_alOccGainFloor,
        "Occlusion: floor of AL_LOWPASS_GAIN (overall volume) for fully-blocked sources [0-1]. "
        "0 = can go fully silent through thick walls; 1 = no volume change (vanilla-like). "
        "Default 0.55 keeps occluded sounds audible while halving the wall-to-clear volume jump. "
        "Old hardcoded floor was 0.25.");
    s_alOccHFFloor = Cvar_Get("s_alOccHFFloor", "0.50", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alOccHFFloor, "0", "1", CV_FLOAT);
    Cvar_SetDescription(s_alOccHFFloor,
        "Occlusion: floor of AL_LOWPASS_GAINHF (high-frequency content) for fully-blocked sources [0-1]. "
        "0 = strip all HF (bass-only thump through walls); 1 = no HF filtering (only volume changes). "
        "Default 0.50 preserves enough weapon-fire crack to remain recognisable. "
        "Old formula was occ\xc2\xb2 which reached near-zero HF even at moderate occlusion — "
        "this caused the 'tinny then boom' corner-pop effect.");
    s_alOccPosBlend = Cvar_Get("s_alOccPosBlend", "0.25", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alOccPosBlend, "0", "1", CV_FLOAT);
    Cvar_SetDescription(s_alOccPosBlend,
        "Occlusion: how far to redirect the HRTF apparent-source position towards the nearest "
        "acoustic gap when a source is blocked [0-1]. "
        "0 = always use true source position (best direction accuracy, no gap hint); "
        "1 = full redirect to gap (old behaviour — confuses direction of fire). "
        "Default 0.25: subtle gap hint that helps 'around the corner' perception "
        "without sacrificing the ability to locate shooting players.");
    s_alDebugOcc = Cvar_Get("s_alDebugOcc", "0", CVAR_TEMP);
    Cvar_CheckRange(s_alDebugOcc, "0", "1", CV_INTEGER);
    Cvar_SetDescription(s_alDebugOcc,
        "Occlusion debug output [0/1]. When 1, prints one line per occluded source each frame: "
        "entity number, listener distance, trace target, smoothed gain, LOWPASS_GAIN, LOWPASS_GAINHF. "
        "Use this to isolate whether a specific sound is being filtered and by how much. "
        "Not archived.");
    s_alDebugPlayback = Cvar_Get("s_alDebugPlayback", "0", CVAR_TEMP);
    Cvar_CheckRange(s_alDebugPlayback, "0", "2", CV_INTEGER);
    Cvar_SetDescription(s_alDebugPlayback,
        "Playback diagnostics. "
        "1 = log every PREEMPT event (sound cut short by a new sound on the same channel) "
        "and every rate-mismatch at load time (file Hz != device Hz, forces the resampler). "
        "2 = also log every natural completion. "
        "Each line shows: sound name, samples played vs total, % consumed, file rate, device rate. "
        "Use this to determine whether the DE-50 boom is being truncated (preempt) "
        "or degraded by the resampler (rate mismatch). "
        "Not archived. Set before loading a map to catch registration warnings.");

    /* Source pool size.  The pool starts at this size and grows dynamically
     * on demand up to S_AL_MAX_SRC.  Reduce if your audio driver struggles
     * (extremely rare on modern hardware).  Requires vid_restart (LATCH). */
    s_alMaxSrc = Cvar_Get("s_alMaxSrc", "512", CVAR_ARCHIVE_ND | CVAR_LATCH);
    Cvar_CheckRange(s_alMaxSrc, "16", va("%d", S_AL_MAX_SRC), CV_INTEGER);
    Cvar_SetDescription(s_alMaxSrc,
        "Maximum OpenAL source slots.  Default 512 — slots are cheap on modern "
        "hardware; reduce only if the audio driver reports resource errors.  "
        "Requires vid_restart (LATCH).");

    /* Dedup window — disabled by default (0).  Set to e.g. 20 to suppress
     * identical (entity, sfx) re-submissions within that many milliseconds.
     * Vanilla URT doesn't bother and nobody notices; kept as a cvar in case
     * it is ever needed for unusual sound designs. */
    s_alDedupMs = Cvar_Get("s_alDedupMs", "0", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alDedupMs, "0", "200", CV_INTEGER);
    Cvar_SetDescription(s_alDedupMs,
        "Same-entity same-sfx dedup window in milliseconds.  0 = disabled (default).  "
        "When > 0, suppresses a second start of the exact same sound from the same "
        "entity within this window — prevents audible doubling on rapid re-triggers.");

    s_alTrace = Cvar_Get("s_alTrace", "0", CVAR_TEMP);
    Cvar_CheckRange(s_alTrace, "0", "2", CV_INTEGER);
    Cvar_SetDescription(s_alTrace,
        "OpenAL trace logging. "
        "0 = off (default). "
        "1 = key lifecycle events: source alloc/free, music open/close/loop, "
        "video audio drops, AL errors at critical call sites, init sequence. "
        "2 = verbose: every source setup, buffer queue/unqueue, and state change. "
        "Not archived. Set before starting the game to capture the init sequence.");

    /* Load library */
    if (!S_AL_LoadLibrary())
        return qfalse;

    /* Now the library is loaded — query the real default device name and
     * embed it in the s_alDevice cvar description so players see it when
     * they tab-complete or type the cvar name in the console. */
    {
        char           desc[512];
        const ALCchar *defDev;

        if (qalcIsExtensionPresent(NULL, "ALC_ENUMERATE_ALL_EXT"))
            defDev = qalcGetString(NULL, ALC_DEFAULT_ALL_DEVICES_SPECIFIER);
        else
            defDev = qalcGetString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER);

        Com_sprintf(desc, sizeof(desc),
            "OpenAL output device. Empty = system default (\"%s\"). "
            "Type /s_devices for a numbered list. (LATCH — requires vid_restart)",
            (defDev && defDev[0]) ? defDev : "unknown");
        Cvar_SetDescription(s_alDevice, desc);
    }

    /* Open device */
    device = s_alDevice->string[0] ? s_alDevice->string : NULL;
    s_al_device = qalcOpenDevice(device);
    if (!s_al_device) {
        Com_Printf("S_AL: alcOpenDevice(%s) failed\n", device ? device : "default");
        S_AL_UnloadLibrary();
        return qfalse;
    }
    Com_Printf("S_AL: device \"%s\"\n",
        qalcGetString(s_al_device, ALC_DEVICE_SPECIFIER));

    /* -----------------------------------------------------------------------
     * HRTF / headphone stereo setup
     *
     * s_alHRTF 1 (opt-in): request HRTF convolution for headphone users.
     *   Layer 1 — ALC_SOFT_output_mode: ALC_STEREO_HRTF_SOFT
     *   Layer 2 — ALC_SOFT_HRTF:        ALC_HRTF_SOFT = ALC_TRUE
     *   Layer 3 — alcResetDeviceSoft:   belt-and-suspenders if still disabled
     *
     * s_alHRTF 0 (default): explicitly DISABLE HRTF so that alsoft.conf or
     *   driver defaults cannot silently re-enable it.  Without the explicit
     *   ALC_HRTF_SOFT=ALC_FALSE request, OpenAL Soft may honour a system-wide
     *   "hrtf = true" in alsoft.conf and run the centre-HRIR convolution on
     *   own-player weapon sources even when the user sets s_alHRTF 0.  That
     *   convolution is what produces the "muffled pop" on speaker setups.
     *   ALC_STEREO_BASIC_SOFT requests plain stereo panning with no HRTF and
     *   no surround upmix — the correct output mode for a speaker game session.
     * ----------------------------------------------------------------------- */
    nAttribs = 0;
    if (s_alHRTF->integer) {
        /* Layer 1: prefer ALC_SOFT_output_mode + ALC_STEREO_HRTF_SOFT */
        if (qalcIsExtensionPresent(s_al_device, "ALC_SOFT_output_mode")) {
            ctxAttribs[nAttribs++] = ALC_OUTPUT_MODE_SOFT;
            ctxAttribs[nAttribs++] = ALC_STEREO_HRTF_SOFT;
        }
        /* Layer 2: ALC_SOFT_HRTF — always include when extension present */
        if (qalcIsExtensionPresent(s_al_device, "ALC_SOFT_HRTF")) {
            ctxAttribs[nAttribs++] = ALC_HRTF_SOFT;
            ctxAttribs[nAttribs++] = ALC_TRUE;
        }
    } else {
        /* Explicitly disable HRTF and request plain stereo output.
         * This overrides alsoft.conf and device defaults so the user can
         * be certain HRTF is truly off after a vid_restart. */
        if (qalcIsExtensionPresent(s_al_device, "ALC_SOFT_output_mode")) {
            ctxAttribs[nAttribs++] = ALC_OUTPUT_MODE_SOFT;
            ctxAttribs[nAttribs++] = ALC_STEREO_BASIC_SOFT;
        }
        if (qalcIsExtensionPresent(s_al_device, "ALC_SOFT_HRTF")) {
            ctxAttribs[nAttribs++] = ALC_HRTF_SOFT;
            ctxAttribs[nAttribs++] = ALC_FALSE;
        }
    }
    ctxAttribs[nAttribs] = 0; /* sentinel */

    s_al_context = qalcCreateContext(s_al_device, ctxAttribs);
    if (!s_al_context) {
        Com_Printf("S_AL: alcCreateContext failed (error 0x%x)\n",
            qalcGetError(s_al_device));
        qalcCloseDevice(s_al_device); s_al_device = NULL;
        S_AL_UnloadLibrary();
        return qfalse;
    }
    qalcMakeContextCurrent(s_al_context);

    /* Query the device's actual mixing frequency.  This is the rate OpenAL
     * will resample every buffer to internally.  Any sound file whose rate
     * differs from this value will go through the internal resampler.
     * We log this at startup and use it in the playback-debug output. */
    qalcGetIntegerv(s_al_device, ALC_FREQUENCY, 1, &s_al_deviceFreq);
    Com_Printf("S_AL: device mixing frequency: %d Hz\n", (int)s_al_deviceFreq);

    /* Query actual HRTF status */
    if (qalcIsExtensionPresent(s_al_device, "ALC_SOFT_HRTF")) {
        qalcGetIntegerv(s_al_device, ALC_HRTF_STATUS_SOFT, 1, &hrtfStatus);
        s_al_hrtf = (hrtfStatus == ALC_HRTF_ENABLED_SOFT);

        /* Layer 3 enable: if HRTF wasn't granted at context creation
         * (e.g. OpenAL Soft auto-disabled it for a non-headphone device),
         * re-request it explicitly when the user wants HRTF on. */
        if (!s_al_hrtf && s_alHRTF->integer && qalcResetDeviceSoft) {
            ALCint resetAttribs[] = {
                ALC_HRTF_SOFT, ALC_TRUE,
                0
            };
            if (qalcResetDeviceSoft(s_al_device, resetAttribs)) {
                qalcGetIntegerv(s_al_device, ALC_HRTF_STATUS_SOFT, 1, &hrtfStatus);
                s_al_hrtf = (hrtfStatus == ALC_HRTF_ENABLED_SOFT);
                if (s_al_hrtf)
                    Com_DPrintf("S_AL: HRTF enabled via alcResetDeviceSoft\n");
            }
        }

        /* Layer 3 disable: if HRTF is still active despite s_alHRTF=0 and the
         * explicit ALC_HRTF_SOFT=ALC_FALSE context attr (e.g. a system-wide
         * alsoft.conf override), force it off via alcResetDeviceSoft.
         * Without this, the user has no way to guarantee HRTF is truly off
         * short of editing alsoft.conf manually. */
        if (s_al_hrtf && !s_alHRTF->integer && qalcResetDeviceSoft) {
            ALCint resetAttribs[] = {
                ALC_HRTF_SOFT, ALC_FALSE,
                0
            };
            if (qalcResetDeviceSoft(s_al_device, resetAttribs)) {
                qalcGetIntegerv(s_al_device, ALC_HRTF_STATUS_SOFT, 1, &hrtfStatus);
                s_al_hrtf = (hrtfStatus == ALC_HRTF_ENABLED_SOFT);
            }
            if (s_al_hrtf)
                Com_Printf(S_COLOR_YELLOW
                    "S_AL: WARNING: HRTF still active despite s_alHRTF 0 — "
                    "check alsoft.conf or driver settings\n");
            else
                Com_DPrintf("S_AL: HRTF forced off via alcResetDeviceSoft\n");
        }
    }

    /* Log HRTF status and which dataset OpenAL Soft selected */
    if (s_al_hrtf) {
        const ALCchar *hrtfName = NULL;
        if (qalcGetStringiSOFT)
            hrtfName = qalcGetStringiSOFT(s_al_device, ALC_HRTF_SPECIFIER_SOFT, 0);
        Com_Printf("S_AL: HRTF enabled (%s)\n",
            (hrtfName && hrtfName[0]) ? hrtfName : "built-in dataset");
    } else {
        Com_Printf(s_alHRTF->integer
            ? "S_AL: HRTF requested but not supported by this device/driver\n"
            : "S_AL: HRTF disabled (s_alHRTF 0)\n");
    }

    /* Distance model matching Q3 linear attenuation */
    qalDistanceModel(AL_LINEAR_DISTANCE_CLAMPED);

    /* Doppler — OpenAL Soft models this natively when AL_VELOCITY is set */
    if (s_doppler && s_doppler->integer)
        qalDopplerFactor(1.0f);
    else
        qalDopplerFactor(0.0f);
    qalSpeedOfSound(1700.0f);   /* game units/sec, tuned to Q3 scale */

    S_AL_InitEFX();

    /* Detect AL_SOFT_direct_channels.  Available in OpenAL Soft since v1.13.
     * Only active when HRTF is on (s_alHRTF 1): routing PCM directly to the
     * stereo output only makes sense when there is a convolution kernel to
     * bypass.  With s_alHRTF 0 (default) HRTF is already off, so this is a
     * no-op and we skip setting it to reflect the actual intent. */
    s_al_directChannelsExt = qalIsExtensionPresent("AL_SOFT_direct_channels");
    s_al_directChannels    = s_al_directChannelsExt &&
                             s_alDirectChannels && s_alDirectChannels->integer &&
                             s_alHRTF && s_alHRTF->integer;
    Com_Printf("S_AL: AL_SOFT_direct_channels: %s%s\n",
               s_al_directChannelsExt ? "available" : "not available",
               (s_al_directChannelsExt && !s_al_directChannels)
                   ? (s_alDirectChannels && !s_alDirectChannels->integer)
                       ? " (disabled by s_alDirectChannels 0)"
                       : " (inactive — s_alHRTF 0)"
                   : "");

    /* -----------------------------------------------------------------------
     * Streaming sources — allocated BEFORE the game source pool so they are
     * guaranteed a valid OpenAL voice regardless of the driver's voice limit.
     *
     * PR #95 raised s_alMaxSrc to 512.  OpenAL Soft's default mix-voice cap
     * is 256.  When the game pool was allocated first it consumed all 256
     * voices, leaving s_al_music.source = 0 and s_al_raw.source = 0.
     * Every subsequent qalSource* call on source 0 generates AL_INVALID_NAME,
     * which causes OpenAL Soft's 64-bit GCC runtime to throw 0x20474343 C++
     * exceptions — the hard crash reported after the intro video finishes and
     * the menu music tries to start.  On 32-bit the same root cause silently
     * drops all video audio and menu music without crashing.
     * ----------------------------------------------------------------------- */

    /* Music streaming source and buffers */
    Com_Memset(&s_al_music, 0, sizeof(s_al_music));
    {
        ALenum preErr = qalGetError(); /* clear any stale error before the alloc */
        if (preErr != AL_NO_ERROR)
            S_AL_TRACE(1, "init: stale AL error 0x%04x cleared before music source alloc\n",
                       (unsigned)preErr);
    }
    qalGenSources(1, &s_al_music.source);
    if (qalGetError() != AL_NO_ERROR) {
        Com_Printf(S_COLOR_YELLOW
            "S_AL: WARNING: failed to create music streaming source — "
            "background music will be silent\n");
        s_al_music.source = 0;
    } else {
        S_AL_TRACE(1, "init: music source created (AL id %u)\n",
                   s_al_music.source);
    }
    qalGenBuffers(S_AL_MUSIC_BUFFERS, s_al_music.buffers);

    /* Raw-samples source and buffers (cinematics / VoIP) */
    Com_Memset(&s_al_raw, 0, sizeof(s_al_raw));
    {
        ALenum preErr = qalGetError(); /* clear any stale error before the alloc */
        if (preErr != AL_NO_ERROR)
            S_AL_TRACE(1, "init: stale AL error 0x%04x cleared before raw source alloc\n",
                       (unsigned)preErr);
    }
    qalGenSources(1, &s_al_raw.source);
    if (qalGetError() != AL_NO_ERROR) {
        Com_Printf(S_COLOR_YELLOW
            "S_AL: WARNING: failed to create raw streaming source — "
            "cinematic and VoIP audio will be silent\n");
        s_al_raw.source = 0;
    } else {
        S_AL_TRACE(1, "init: raw source created (AL id %u)\n",
                   s_al_raw.source);
        qalSourcei(s_al_raw.source, AL_SOURCE_RELATIVE, AL_TRUE);
        qalSource3f(s_al_raw.source, AL_POSITION, 0.0f, 0.0f, 0.0f);
        qalSourcef(s_al_raw.source, AL_ROLLOFF_FACTOR, 0.0f);
    }
    qalGenBuffers(S_AL_RAW_BUFFERS, s_al_raw.buffers);

    /* Game source pool — up to s_alMaxSrc sources (capped at S_AL_MAX_SRC).
     * OpenAL Soft will stop early if it hits its own mix-voice limit; the
     * streaming sources above are already allocated so they are unaffected. */
    {
        int wantSrc = (s_alMaxSrc && s_alMaxSrc->integer > 0)
                      ? s_alMaxSrc->integer : S_AL_MAX_SRC;
        if (wantSrc > S_AL_MAX_SRC) wantSrc = S_AL_MAX_SRC;

        s_al_numSrc = 0;
        Com_Memset(s_al_src, 0, sizeof(s_al_src));
        for (i = 0; i < wantSrc; i++) {
            qalGenSources(1, &s_al_src[i].source);
            if (qalGetError() != AL_NO_ERROR) {
                Com_DPrintf("S_AL: created %d sources (AL_OUT_OF_RESOURCES)\n", i);
                break;
            }
            s_al_src[i].sfx = -1;
            s_al_numSrc++;
        }
        Com_Printf("S_AL: %d game sources allocated (wanted %d)\n",
                   s_al_numSrc, wantSrc);
        S_AL_TRACE(1, "init: game pool %d/%d voices\n", s_al_numSrc, wantSrc);
    }

    /* Looping state */
    Com_Memset(s_al_loops, 0, sizeof(s_al_loops));
    for (i = 0; i < MAX_GENTITIES; i++)
        s_al_loops[i].srcIdx = -1;
    s_al_loopFrame = 0;

    /* Entity origin cache */
    Com_Memset(s_al_entity_origins, 0, sizeof(s_al_entity_origins));
    s_al_listener_entnum = -1;

    s_al_started = qtrue;
    s_al_muted   = qfalse;

    Cmd_AddCommand("s_devices", S_AL_ListDevicesCmd);
    Cmd_AddCommand("s_alReset",  S_AL_ReverbReset_f);

    /* Populate the soundInterface_t */
    si->Shutdown             = S_AL_Shutdown;
    si->StartSound           = S_AL_StartSound;
    si->StartLocalSound      = S_AL_StartLocalSound;
    si->StartBackgroundTrack = S_AL_StartBackgroundTrack;
    si->StopBackgroundTrack  = S_AL_StopBackgroundTrack;
    si->RawSamples           = S_AL_RawSamples;
    si->StopAllSounds        = S_AL_StopAllSounds;
    si->ClearLoopingSounds   = S_AL_ClearLoopingSounds;
    si->AddLoopingSound      = S_AL_AddLoopingSound;
    si->AddRealLoopingSound  = S_AL_AddRealLoopingSound;
    si->StopLoopingSound     = S_AL_StopLoopingSound;
    si->Respatialize         = S_AL_Respatialize;
    si->UpdateEntityPosition = S_AL_UpdateEntityPosition;
    si->Update               = S_AL_Update;
    si->DisableSounds        = S_AL_DisableSounds;
    si->BeginRegistration    = S_AL_BeginRegistration;
    si->RegisterSound        = S_AL_RegisterSound;
    si->ClearSoundBuffer     = S_AL_ClearSoundBuffer;
    si->SoundInfo            = S_AL_SoundInfo;
    si->SoundList            = S_AL_SoundList;

    return qtrue;
}

#endif /* USE_OPENAL */
