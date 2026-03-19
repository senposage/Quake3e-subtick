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
# include <pthread.h>
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
static LPALGETSTRINGISOFT    qalGetStringiSOFT    = NULL;  /* enumerate resamplers */

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
static LPALGETAUXILIARYEFFECTSLOTF    qalGetAuxiliaryEffectSlotf;

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
/* Maximum dedup window applied to the duration-based extension for non-weapon
 * channels.  Caps the int64→int narrowing and prevents absurdly long tracks
 * (streamed music registered as loop sfx) from locking out re-triggers for
 * an impractical length of time. */
#define S_AL_DEDUP_MAX_DUR_MS 600000  /* 10 minutes */

/* CHAN_WEAPON gun-muzzle proximity: sources on the weapon channel within this
 * distance of the listener are treated as "own gun barrel" and bypass occlusion
 * regardless of entity ownership.  This covers muzzle-flash origin offsets
 * (~50–100 u) with margin and prevents PLAYERCLIP slope geometry from muffling
 * the player's own weapon fire even when the sound was emitted as a world sound. */
#define S_AL_WEAPON_NOOCC_DIST  160.0f

/* One-shot dedup window (s_alDedupMs).
 *
 * Two-tier dedup applied in S_AL_StartSound:
 *
 *   CHAN_WEAPON:       minimum window only (default 20 ms ≈ 1 frame).
 *                     Blocks same-frame double-starts (e.g. cgame submitting
 *                     the same fire event twice) but lets every intentional
 *                     shot through at any fire rate.
 *
 *   All other channels: extend the window to the FULL SOUND DURATION when a
 *                     source with the same (entnum, sfx) is still playing.
 *                     Prevents trigger sounds (birds, ambient music,
 *                     environment cues) from re-stacking when the player
 *                     stands on overlapping trigger bounds or moves in/out
 *                     of a trigger area before the previous instance ends.
 *
 *   ENTITYNUM_WORLD:  hard 100 ms floor regardless of the cvar, matching
 *                     DMA's hardcoded world-entity throttle
 *                     (dma.speed * 100 / 1000) that limits impact/casing
 *                     accumulation at sv_fps rate. */

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
    SRC_CAT_LOCAL   = 1,  /* own player movement / breath / general sounds */
    SRC_CAT_AMBIENT = 2,  /* looping environmental / ambient sounds */
    SRC_CAT_UI      = 3,  /* non-positional local sounds: hit markers, kill
                           * confirmations, menu beeps (entnum == 0 + isLocal).
                           * Separate from SRC_CAT_LOCAL so hit/kill sounds
                           * can be tuned independently of weapon volume. */
    SRC_CAT_WEAPON  = 4,  /* own player CHAN_WEAPON fire — separated so weapon
                           * volume can be tuned without affecting footsteps. */
    SRC_CAT_IMPACT  = 5,  /* ENTITYNUM_WORLD one-shots: bullet impacts, brass,
                           * explosions, debris — often disproportionately loud. */
    SRC_CAT_WEAPON_SUPPRESSED = 6, /* own suppressed-attachment weapon fire
                                    * (e.g. M4/MP5 with silencer fitted).
                                    * Detected via gear-string 'U' flag or
                                    * sound file name pattern. */
    SRC_CAT_WORLD_SUPPRESSED  = 7, /* enemy/other-player suppressed weapon fire.
                                    * Suppressed fire is inherently quieter and
                                    * harder to locate — tunable via
                                    * s_alVolEnemySuppressedWeapon. */
    SRC_CAT_EXTRAVOL          = 8, /* user-defined per-slot pattern list
                                    * (s_alExtraVolEntry1–s_alExtraVolEntry8):
                                    * sounds matching any positive slot are
                                    * routed here so s_alExtraVol can scale
                                    * them independently.  A ":-N" dB suffix
                                    * on a slot applies a per-sample cut on
                                    * top of the global scalar. Defaults:
                                    * slot 1 = hit.wav:-2.5,
                                    * slot 2 = kill.wav:-2.5. */
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
    float           sampleVol;         /* per-sample linear scale from ":-N" dB suffix in the matching
                                         * s_alExtraVolEntryN slot; 1.0 for non-EXTRAVOL sources and
                                         * EXTRAVOL slots without a suffix */
    qboolean        isBspSpeaker;      /* qtrue when this is a non-global BSP target_speaker loop —
                                         * allows the occlusion pass to mute it through walls while
                                         * keeping AL_POSITION / AL_GAIN owned by S_AL_UpdateLoops */
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
    float       fadeOutStartGain;   /* fade-in progress factor [0..1] at the moment fade-out was
                                     * triggered — used so fade-out always ramps down from the
                                     * actual in-progress level rather than jumping to full gain
                                     * first (pop fix when fade-out interrupts a fade-in). */
} alLoop_t;

static alLoop_t s_al_loops[MAX_GENTITIES];
static int      s_al_loopFrame = 0;

/* Per-frame multi-hop trace budget.
 * Each multi-hop run casts up to 16 CM_BoxTrace calls (8 directions × 2 legs).
 * With 32 players all firing simultaneously, every new weapon sound starts with
 * occlusionTick=0 and traces on the same frame — potentially hundreds of traces
 * in one audio tick, starving the audio thread's buffer fill and causing
 * crackling that scales with the number of concurrent sound events.
 * Limiting multi-hop to S_AL_MULTIHOP_BUDGET sources per frame keeps the
 * worst-case trace count bounded.  Sources beyond the budget still run the
 * fast direct+near-source-probe path; only the two-hop loop is skipped. */
#define S_AL_MULTIHOP_BUDGET    8
static int s_al_multihop_frame  = -1; /* s_al_loopFrame when count was reset */
static int s_al_multihop_used   = 0;  /* multi-hop runs consumed this frame  */

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
static ALint    s_al_bestResampler = -1; /* AL_SOFT_source_resampler index, -1 = use default */

static vec3_t s_al_listener_origin;   /* updated in S_AL_Respatialize */
static int    s_al_listener_entnum = -1; /* entity number of the listener */
static vec3_t s_al_listener_forward;  /* axis[0] — direction listener is facing; updated in S_AL_Respatialize */

/* Fire-direction impact reverb: set in S_AL_StartSound on CHAN_WEAPON from
 * the listener entity; consumed by S_AL_UpdateDynamicReverb. */
static vec3_t s_al_fire_dir;          /* normalised aim direction at last weapon fire */
static int    s_al_fire_frame = -9999;/* s_al_loopFrame when last shot was fired */

/* Incoming-enemy-fire reverb: set in S_AL_StartSound for nearby non-friendly
 * CHAN_WEAPON sounds; consumed by S_AL_UpdateDynamicReverb. */
static int   s_al_incoming_fire_frame = -9999; /* s_al_loopFrame when last enemy shot detected */
static float s_al_incoming_fire_dist  =  1.0f; /* normalised dist from listener [0=close,1=at range] */

/* Grenade-concussion bloom state.
 * When an enemy grenade explodes near the listener, a brief EFX reverb boost
 * is applied — the reverb slot gain spikes then decays back over bloomMs.
 * No listener-gain duck: competitive audio clarity is preserved at all times.
 * bloomExpiry: Com_Milliseconds() when the bloom expires (0 = idle).
 * bloomPeak:   reverb slot gain at the moment of trigger. */
static int   s_al_bloomExpiry = 0;
static float s_al_bloomPeak   = 0.f;

/* Audio suppression state: a near-miss tracer briefly ducks the listener
 * gain to simulate the concussive disruption of incoming fire.
 * suppressExpiry: Com_Milliseconds() timestamp when the duck expires (0=off).
 * suppressHFPeak: the AL_LOWPASS_GAINHF floor for the active event.
 *   Multiple overlapping triggers (incoming fire + helmet hit, etc.) take
 *   the later expiry and the deeper (lower) HF floor.
 * suppress_dir / suppress_directional: direction TOWARD the incoming fire
 *   source (normalised).  When directional, only sources within the cone
 *   defined by s_alSuppressionConeAngle are fully muffled; sources outside
 *   receive no HF cut.  Head-hit triggers set suppress_directional = qfalse
 *   (attacker direction unknown) so the full HF cut is applied globally. */
static int      s_al_suppressExpiry      = 0;
static float    s_al_suppressHFPeak      = 1.0f;
static vec3_t   s_al_suppress_dir        = {0.f, 0.f, 0.f};
static qboolean s_al_suppress_directional = qfalse;
/* Precomputed cosine of the half-cone angle; updated each frame. */
static float    s_al_suppress_cone_halfcos = 0.f; /* cos(60°) = 0.5 at default 120° */

/* Reverb boost during suppression: spikes the reverb slot gain on trigger
 * then decays back over the suppression duration — creates the "ring/splash"
 * acoustic character of a nearby shot rather than just volume loss. */
static int   s_al_suppressReverbExpiry = 0;
static float s_al_suppressReverbPeak   = 0.f;

/* Per-frame combined hearing-disruption HF multipliers.
 * 1.0 = flat (no filter); lower values cut high frequencies, turning the
 * old "general duck" into a convincing muffled/concussed hearing effect.
 * Computed each frame in S_AL_Update from the active expiry timers and
 * player health, then applied per-source in the Phase-3 filter block. */
static float s_al_suppressHF = 1.0f;  /* incoming-fire / head-hit suppression */
static float s_al_grenadeHF  = 1.0f;  /* grenade-blast concussion            */
static float s_al_healthHF   = 1.0f;  /* near-death health fade               */

/* Procedurally synthesised tinnitus tone.
 * Generated once (or on cvar change) via S_AL_BuildTinnitusBuf(), stored
 * as a raw AL buffer.  Played as a non-positional local source on head hits.
 * -1 / 0 indicate "not yet built". */
static ALuint s_al_tinnitusBuf         = 0;
static int    s_al_tinnitusLastBuiltHz = 0;  /* freq the buffer was built at   */
static int    s_al_tinnitusLastBuiltMs = 0;  /* duration the buffer was built for */
static int    s_al_tinnitusLastPlay    = 0;  /* timestamp — spam-guard         */

/* Per-entity origin cache — updated by S_AL_UpdateEntityPosition and
 * S_AL_Respatialize.  Used by S_AL_StartSound when origin is NULL so that
 * entity-following sounds start at the right place instead of the world
 * origin.  Indexed by entity number. */
static vec3_t s_al_entity_origins[MAX_GENTITIES];

typedef struct {
    qboolean available;       /* ALC_EXT_EFX present and procs loaded */
    ALuint   reverbEffect;    /* AL_EFFECT_EAXREVERB (or AL_EFFECT_REVERB fallback) */
    ALuint   reverbSlot;      /* auxiliary effect slot for reverb (aux send 0) */
    ALuint   underwaterSlot;  /* auxiliary slot for underwater effect (aux send 0 in water) */
    ALuint   underwaterEffect;/* AL_EFFECT_EAXREVERB for water acoustics */
    ALuint   echoEffect;      /* AL_EFFECT_ECHO — geometry-driven discrete repeats (aux send 1) */
    ALuint   echoSlot;        /* auxiliary effect slot for the echo effect */
    ALuint   chorusEffect;    /* AL_EFFECT_CHORUS — underwater modulation wobble (aux send 1) */
    ALuint   chorusSlot;      /* auxiliary effect slot for the chorus effect */
    ALuint   eqEffect;        /* AL_EFFECT_EQUALIZER — global tone shaping */
    ALuint   eqSlot;          /* auxiliary effect slot for the EQ */
    int      eqSend;          /* aux send index used for EQ (-1 = not wired) */
    ALuint   eqNormFilter;    /* lowpass filter at eqCompGain: applied to both the direct
                               * path and the EQ auxiliary send to prevent the additive
                               * wet signal from pushing dry+wet above the source level */
    float    eqCompGain;      /* 1/(1+max_band*slot_gain): normalization factor so that
                               * direct*comp + EQ_wet*comp*band = source at the peak band */
    ALuint   occlusionFilter[S_AL_MAX_SRC];
    qboolean hasEAXReverb;    /* AL_EFFECT_EAXREVERB available (vs plain reverb) */
    ALint    maxSends;        /* ALC_MAX_AUXILIARY_SENDS reported by the device */
} alEfx_t;

static alEfx_t s_al_efx;

/* =========================================================================
 * BSP map entity data — extracted from the entity lump once per map load.
 *
 * alMapHints_t holds acoustic-relevant worldspawn keys used to bias the
 * dynamic reverb classifier (e.g. a map with a skybox is an outdoor/urban
 * layout, not an underground cave).
 *
 * alBspSpeaker_t stores every target_speaker entity with a "noise" key so
 * that S_AL_StartSound can resolve the correct world position for cold-start
 * sounds that arrive before S_UpdateEntityPosition has been called for the
 * entity (e.g. the very first frame of a new map).
 * ========================================================================= */
#define S_AL_BSP_SPEAKERS_MAX 256
/* Maximum squared distance (in game units) for matching a loop entity's
 * origin to a BSP speaker origin when tagging isBspSpeaker.  4 units gives
 * a 4-unit sphere that comfortably covers floating-point precision differences
 * between the BSP-parsed string origin and the cgame-supplied loop origin. */
#define S_AL_BSP_SPEAKER_ORIGIN_MATCH_SQ 16.0f

typedef struct {
    char   noise[MAX_QPATH]; /* "noise" key value — sound file path */
    vec3_t origin;           /* "origin" key parsed into world coords */
    int    spawnflags;       /* bit 0 = GLOBAL (plays everywhere)    */
} alBspSpeaker_t;

typedef struct {
    qboolean parsed;                  /* qtrue after S_AL_ParseMapEntities ran */
    qboolean hasSky;                  /* worldspawn "sky" key present */
    char     skyShader[64];           /* sky shader name (empty if none) */
    char     worldAmbient[MAX_QPATH]; /* worldspawn "ambient" key value */
    char     worldMusic[MAX_QPATH];   /* worldspawn "music" key value */
    int      globalSpeakerCount;      /* target_speaker entities with spawnflags & 1 */
    int      totalSpeakerCount;       /* all target_speaker entities with a noise key */
} alMapHints_t;

static alMapHints_t   s_al_mapHints;
static alBspSpeaker_t s_al_bspSpeakers[S_AL_BSP_SPEAKERS_MAX];
static int            s_al_numBspSpeakers;

/* Per-map normalization cache.
 *
 * Loaded from normcache/<mapname>.cfg at BeginRegistration so that
 * RegisterSound picks up the override before cgame registers its sounds.
 * Written to the same file after the first map load once every BSP sound has
 * been decoded and its normGain computed.
 *
 * Entries are matched case-insensitively against the sfx path.  The table is
 * cleared in StopAllSounds so stale entries from a previous map never bleed
 * into the next one. */
#define S_AL_NORMCACHE_MAX 512

typedef struct {
    char  path[MAX_QPATH]; /* sfx path key                            */
    float normGain;        /* override value written to r->normGain   */
} alNormCacheEntry_t;

static alNormCacheEntry_t s_al_normCache[S_AL_NORMCACHE_MAX];
static int                s_al_normCacheCount;
/* qtrue once the cache has been written for the current map so we don't
 * rewrite on every probe cycle. */
static qboolean           s_al_normCacheWritten;

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
static cvar_t *s_alEcho;              /* 0=off, 1=geometry-driven echo on aux send 1 */
static cvar_t *s_alEchoGain;          /* echo slot wet gain [0..0.3] */
static cvar_t *s_alFireImpactReverb;  /* 0=off, 1=transient reflections boost on weapon fire */
static cvar_t *s_alFireImpactMaxBoost;/* max reflections gain added by fire impact [0..0.4] */
static cvar_t *s_alSuppression;        /* 0=off, 1=near-miss tracer detection */
static cvar_t *s_alSuppressionRadius;  /* near-miss detection radius (units) */
static cvar_t *s_alSuppressionFloor;   /* min listener gain during suppression [0..0.8] */
static cvar_t *s_alSuppressionMs;      /* suppression duration in ms */
static cvar_t *s_alSuppressionHFFloor; /* min AL_LOWPASS_GAINHF during suppression [0..1] */
static cvar_t *s_alSuppressionConeAngle;  /* full cone angle (deg) for directional HF suppression */
static cvar_t *s_alSuppressionRearGain;   /* partial HF suppression fraction behind the listener */
static cvar_t *s_alSuppressionReverbBoost; /* reverb slot gain spike added on suppression trigger */
static cvar_t *s_alNearMissPattern;    /* sound-name substrings that identify a near-miss bullet */
/* Head-hit triggers (helmet and bare-head) — separate gate from s_alSuppression */
static cvar_t *s_alHeadHit;            /* 0=off, 1=head-hit disruption + tinnitus (standalone) */
static cvar_t *s_alHelmetHitPattern;   /* sound-name substrings that identify a helmet hit */
static cvar_t *s_alHelmetHitMs;        /* hearing-disruption duration after helmet hit (ms) */
static cvar_t *s_alHelmetHFFloor;      /* min HF gain for helmet hit [0..1] */
static cvar_t *s_alBareHeadHitPattern; /* sound-name substrings for a bare-head hit */
static cvar_t *s_alBareHeadHitMs;      /* hearing-disruption duration for bare-head hit (ms) */
static cvar_t *s_alBareHeadHFFloor;    /* min HF gain for bare-head hit [0..1] */
/* Synthesised tinnitus tone */
static cvar_t *s_alTinnitusFreq;       /* tone frequency in Hz (rebuilt on change) */
static cvar_t *s_alTinnitusDuration;   /* ring duration in ms (rebuilt on change) */
static cvar_t *s_alTinnitusVol;        /* playback volume [0..1] */
static cvar_t *s_alTinnitusCooldown;   /* min ms between successive plays */
/* Health-based HF fade */
static cvar_t *s_alHealthFade;           /* 0=off, 1=enable near-death HF fade */
static cvar_t *s_alHealthFadeThreshold;  /* HP below which fade activates */
static cvar_t *s_alHealthFadeFloor;      /* HF floor at 1 HP [0..1] */
/* Grenade-concussion EFX bloom — reverb + HF filter, competitive-safe */
static cvar_t *s_alGrenadeBloom;        /* 0=off, 1=EFX reverb bloom on nearby enemy grenade */
static cvar_t *s_alGrenadeBloomRadius;  /* blast radius that triggers the bloom effect (units) */
static cvar_t *s_alGrenadeBloomGain;    /* peak reverb slot gain boost [0..0.3] */
static cvar_t *s_alGrenadeBloomMs;      /* bloom decay duration in ms */
static cvar_t *s_alGrenadeBloomHFFloor; /* min HF gain during grenade blast [0..1] */
static cvar_t *s_alGrenadeBloomDuck;       /* 0=off, 1=also apply mild listener-gain duck with bloom */
static cvar_t *s_alGrenadeBloomDuckFloor;  /* min listener gain during grenade duck [0.5..0.95] */
/* Global EQ — compensate for occlusion processing making audio soft */
static cvar_t *s_alEqLow;         /* low shelf gain (linear, 1.0=flat, >1.0=boost) */
static cvar_t *s_alEqMid;         /* mid band gain */
static cvar_t *s_alEqHigh;        /* high shelf gain */
static cvar_t *s_alEqGain;        /* EQ wet mix level [0..1] */
/* Extra-vol slots: player-configurable sound-path filters with their own volume knob.
 * Each slot is one pattern (optionally prefixed with ! to exclude, suffixed with :-N dB).
 * All slots are CVAR_ARCHIVE so every slot always appears in the config for easy editing. */
#define S_AL_EXTRAVOL_SLOTS 8
static cvar_t *s_alExtraVolEntry[S_AL_EXTRAVOL_SLOTS]; /* one cvar per slot */
static cvar_t *s_alExtraVol;    /* global volume scalar for all matched sounds [0..1.0] */
/* Suppressed weapons: separate volume knob + exclusion from suppression/reverb effects */
static cvar_t *s_alSuppressedSoundPattern; /* comma-separated substrings matched against sfx name */
static cvar_t *s_alVolSuppressedWeapon;    /* own suppressed-weapon sounds [0..10] */
static cvar_t *s_alVolEnemySuppressedWeapon; /* enemy suppressed-weapon sounds [0..10] */
static cvar_t *s_alSuppressedEnemyRangeMax;  /* dist at which enemy suppressed boost drops to zero */
static cvar_t *s_alVolSelf;      /* own player/weapon volume multiplier [0..1.5] */
static cvar_t *s_alVolOther;     /* other entity/player volume multiplier [0..1.0] */
static cvar_t *s_alVolEnv;       /* looping ambient volume multiplier [0..1.0] */
static cvar_t *s_alVolUI;        /* hit/kill/UI sound multiplier [0..1.0] */
static cvar_t *s_alVolWeapon;    /* own weapon-fire volume multiplier [0..10] */
static cvar_t *s_alVolImpact;    /* world-entity impact sound multiplier [0..10] */
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
/* Loop-sound fade timing CVars (live-tunable, use /s_alReset to hear changes). */
static cvar_t *s_alLoopFadeInMs;    /* fade-in duration in ms for new loop sources */
static cvar_t *s_alLoopFadeOutMs;   /* fade-out duration in ms when loop source leaves PVS */
static cvar_t *s_alLoopFadeDist;    /* distance from maxDist at which new sources start with
                                     * a proportional initial gain (0 = always start from 0) */
/* Occlusion filter tuning — live-tunable, use /s_alReset to hear changes. */
static cvar_t *s_alOccGainFloor;   /* floor of AL_LOWPASS_GAIN for fully-blocked sources */
static cvar_t *s_alOccHFFloor;     /* floor of AL_LOWPASS_GAINHF (HF content) through walls */
static cvar_t *s_alOccPosBlend;    /* fraction of HRTF redirect towards nearest gap [0-1] */
static cvar_t *s_alOccSearchRadius; /* tangent-plane probe radius for gap finding [units] */
static cvar_t *s_alOccAreaFloor;   /* minimum occlusion for BSP-connected areas [0-1] */
static cvar_t *s_alDebugOcc;       /* print per-source occlusion state each frame */
static cvar_t *s_alDebugPlayback;  /* 0=off 1=rate-mismatch+preemption 2=+natural-stop */
static cvar_t *s_alDebugNorm;      /* 0=off 1=print normGain for all active ambient sources */
static cvar_t *s_alMaxSrc;         /* max source pool size (LATCH) */
static cvar_t *s_alDedupMs;        /* same-frame dedup window in ms (live) */
static cvar_t *s_alTrace;          /* 0=off 1=key lifecycle events 2=verbose */
static cvar_t *s_alLoadThreads;   /* async load worker count [1-8], default 4, LATCH */

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
    qalGetStringiSOFT   = (LPALGETSTRINGISOFT)   AL_GetProc(al_handle, "alGetStringiSOFT");

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

/* =========================================================================
 * Load-time PCM resampler
 *
 * URT source audio is typically 22 050 Hz 16-bit PCM.  When the device
 * runs at a different rate (48 000 Hz on virtually all modern hardware)
 * OpenAL Soft must resample every source on every playback call.  Even the
 * best internal resampler (BSinc24) cannot repair crackling artefacts that
 * originate in the source — it can only preserve them — and older or less
 * capable resamplers (Zero-Order Hold, linear) actively amplify artefacts
 * through aliasing and interpolation overshoot.
 *
 * The fix: convert each sound file to the device's native rate ONCE at
 * load time using a Kaiser-windowed sinc filter.  The AL buffer is then
 * submitted at the device's own rate, bypassing the internal resampler on
 * every subsequent playback.  The CPU cost is a few milliseconds per sound
 * at map load; the extra RAM is one copy of each sound at the higher rate.
 *
 * Algorithm
 * ---------
 * For each output sample i at continuous input position  t = i*(inRate/outRate):
 *
 *     y[i] = Σ_k  x[k] · h(t − k)            (where k ranges over nearby input frames)
 *
 * where the Kaiser-windowed sinc kernel h is
 *
 *     h(u) = 2·fc · sinc(2·fc·u) · Kaiser(|u|/W, β)
 *
 * with
 *     fc  = min(inRate, outRate) / (2·inRate)   anti-aliasing cutoff
 *     W   = S_AL_SINC_HALF_TAPS                  kernel half-width (input samples)
 *     β   = S_AL_KAISER_BETA                     window shape (~74 dB stopband)
 *
 * · Upsampling (inRate < outRate): fc = 0.5 → full source bandwidth preserved.
 * · Downsampling (inRate > outRate): fc < 0.5 → anti-aliasing at output Nyquist.
 * · Zero-padding at boundaries (samples outside [0, inSamples) are treated as 0).
 *   This creates a natural sub-millisecond fade at each edge, which eliminates
 *   the click artefacts common in URT's abruptly-starting 22 kHz assets.
 * · The output is divided by wsum (sum of applied weights) so that DC gain is
 *   exactly 1.0 everywhere, including near the buffer edges.
 *
 * Polyphase fast-path
 * -------------------
 * The original per-sample sin() call caused 15-20 second map-load stalls.
 * S_AL_ResamplePCM now builds a polyphase filter table (S_AL_RESAMPLE_PHASES
 * rows × nTaps columns) once at the start of the call using sin().  The main
 * resampling loop then does only float multiply-adds against the pre-computed
 * table — no transcendental functions — giving ~180× less sin() work for a
 * typical 2-second sound and reducing map-load resampling time from seconds
 * to milliseconds.
 * =========================================================================
 */
#define S_AL_SINC_HALF_TAPS   24    /* kernel half-width (input samples); 24 → ~74 dB */
#define S_AL_KAISER_BETA      6.0   /* Kaiser β for ~74 dB stopband attenuation       */
#define S_AL_KAISER_LUT_N     512   /* Kaiser window LUT resolution                   */
#define S_AL_RESAMPLE_PHASES  512   /* polyphase table phase count; 512 → <0.001-sample phase error */

/* Pre-computed Kaiser window lookup table.  Index 0 = centre (gain=1), index
 * S_AL_KAISER_LUT_N-1 = edge (gain≈0).  Populated once by S_AL_InitKaiserLUT. */
static float s_al_kaiserLUT[S_AL_KAISER_LUT_N];
static qboolean s_al_kaiserReady = qfalse;

/* Modified Bessel function of the first kind, order 0 (I₀).
 * Series expansion; converges in <30 iterations for β ≤ 12. */
static double S_AL_BesselI0( double x )
{
    double d = 0.0, ds = 1.0, s = 1.0;
    do {
        d  += 2.0;
        ds *= x * x / (d * d);
        s  += ds;
    } while (ds > s * 1e-12);
    return s;
}

/* Populate s_al_kaiserLUT.  Must be called once before S_AL_ResamplePCM. */
static void S_AL_InitKaiserLUT( void )
{
    double inv_i0beta;
    int    i;
    if (s_al_kaiserReady) return;
    inv_i0beta = 1.0 / S_AL_BesselI0(S_AL_KAISER_BETA);
    for (i = 0; i < S_AL_KAISER_LUT_N; i++) {
        double x   = (double)i / (double)(S_AL_KAISER_LUT_N - 1); /* 0 = centre, 1 = edge */
        double arg = 1.0 - x * x;
        s_al_kaiserLUT[i] = (arg > 0.0)
            ? (float)(S_AL_BesselI0(S_AL_KAISER_BETA * sqrt(arg)) * inv_i0beta)
            : 0.0f;
    }
    s_al_kaiserReady = qtrue;
}

/* Inline Kaiser window evaluation.  norm ∈ [0, 1): 0 = centre, 1 = edge. */
static float S_AL_KaiserWindow( double norm )
{
    int idx = (int)(norm * (S_AL_KAISER_LUT_N - 1) + 0.5);
    if (idx < 0)                  idx = 0;
    if (idx >= S_AL_KAISER_LUT_N) idx = S_AL_KAISER_LUT_N - 1;
    return s_al_kaiserLUT[idx];
}

/*
 * S_AL_ResamplePCM
 *
 * Resample `inSamples` frames of `inChannels`-channel native-endian 16-bit
 * signed PCM from `inRate` to `outRate` Hz using the Kaiser-windowed sinc
 * algorithm described above.
 *
 * Returns a Z_Malloc'd array of (*outSamplesOut × inChannels) shorts.
 * The caller must Z_Free the returned pointer.
 * Returns NULL on invalid arguments or allocation failure.
 *
 * Performance
 * -----------
 * The original naive implementation called sin() for every tap of every
 * output sample, causing 15-20 second map-load stalls on modern hardware.
 *
 * This implementation pre-computes a polyphase filter table of
 * S_AL_RESAMPLE_PHASES × nTaps float coefficients at the start of the call.
 * The table is built with sin() (one call per cell), but that cost is
 * negligible compared to the saved per-sample-per-tap sin() calls.  The
 * main resampling loop then does only float multiply-adds — no transcendental
 * functions.  For a 2-second 22 050 Hz → 48 000 Hz sound this reduces the
 * per-file work from ~4.6 M sin() calls to ~25 K (a 180× reduction), cutting
 * total map-load resampling time from seconds to milliseconds.
 */
static short *S_AL_ResamplePCM( const short *in, int inSamples, int inChannels,
                                 int inRate,  int outRate,  int *outSamplesOut )
{
    double  step, fc, winHalf;
    int     outN, nTaps, tapMin, i, j, ch;
    short  *out;
    float  *phaseCoeffs; /* [S_AL_RESAMPLE_PHASES * nTaps] pre-computed kernel  */
    float  *phaseWsum;   /* [S_AL_RESAMPLE_PHASES]          sum of each phase    */

    if (!in || inSamples <= 0 || inChannels <= 0 ||
            inRate <= 0 || outRate <= 0 || !outSamplesOut)
        return NULL;

    outN = (int)((double)inSamples * outRate / inRate + 0.5);
    if (outN <= 0) return NULL;

    /* Advance in the input domain per output sample. */
    step = (double)inRate / outRate;

    /* Anti-aliasing cutoff in normalised-input-rate units.
     * Upsampling: fc = 0.5 (full source bandwidth preserved).
     * Downsampling: fc = outRate/(2·inRate) (cut at output Nyquist). */
    fc = (step > 1.0) ? 0.5 / step : 0.5;

    /* Kernel half-width in input-sample units.
     * Scaled up for downsampling so we still span SINC_HALF_TAPS output taps. */
    winHalf = (step > 1.0)
        ? (double)S_AL_SINC_HALF_TAPS * step
        : (double)S_AL_SINC_HALF_TAPS;

    /* Polyphase table layout
     * ----------------------
     * For each output sample at input position t = i*step, let
     *   iFloor = (int)t,  frac = t - iFloor.
     * Tap j (0 … nTaps-1) reads input sample iFloor + tapMin + j, where
     *   tapMin = -ceil(winHalf).
     * The phase index p = round(frac * S_AL_RESAMPLE_PHASES) selects the
     * pre-computed row phaseCoeffs[p * nTaps … +nTaps-1].
     * phaseWsum[p] = sum of that row (used for DC-gain normalisation).       */
    tapMin = -(int)ceil(winHalf);
    nTaps  = 2 * (int)ceil(winHalf) + 1;

    phaseCoeffs = (float *)malloc(S_AL_RESAMPLE_PHASES * nTaps * sizeof(float));
    phaseWsum   = (float *)malloc(S_AL_RESAMPLE_PHASES * sizeof(float));
    if (!phaseCoeffs || !phaseWsum) {
        if (phaseCoeffs) free(phaseCoeffs);
        if (phaseWsum)   free(phaseWsum);
        return NULL;
    }

    /* Build the polyphase table — sin() calls happen only here, once. */
    for (i = 0; i < S_AL_RESAMPLE_PHASES; i++) {
        double frac = (double)i / S_AL_RESAMPLE_PHASES;
        float  wsum = 0.0f;
        for (j = 0; j < nTaps; j++) {
            /* dt: distance from the fractional input position to tap j's sample */
            double dt      = frac - (double)(tapMin + j);
            double normAbs = (dt < 0.0 ? -dt : dt) / winHalf;
            float  coeff   = 0.0f;
            if (normAbs < 1.0) {
                double w        = (double)S_AL_KaiserWindow(normAbs);
                double sinc_arg = dt * (2.0 * fc);
                double sinc_v   = (fabs(sinc_arg) < 1e-10)
                    ? 1.0
                    : sin(M_PI * sinc_arg) / (M_PI * sinc_arg);
                coeff = (float)(sinc_v * w * (2.0 * fc));
            }
            phaseCoeffs[i * nTaps + j] = coeff;
            wsum += coeff;
        }
        phaseWsum[i] = wsum;
    }

    out = (short *)malloc(outN * inChannels * (int)sizeof(short));
    if (!out) {
        free(phaseCoeffs);
        free(phaseWsum);
        return NULL;
    }

    /* Fast resampling loop — float multiply-add only, no sin() calls. */
    for (i = 0; i < outN; i++) {
        double        t      = (double)i * step;
        int           iFloor = (int)t;        /* floor(t); t is always >= 0 */
        double        frac   = t - (double)iFloor;
        int           pIdx   = (int)(frac * (double)S_AL_RESAMPLE_PHASES + 0.5);
        int           kBase  = iFloor + tapMin;
        int           jLo, jHi;
        const float  *coeffs;
        float         wsumNorm;

        if (pIdx >= S_AL_RESAMPLE_PHASES) pIdx = S_AL_RESAMPLE_PHASES - 1;
        coeffs = phaseCoeffs + pIdx * nTaps;

        /* Clamp tap range to valid input indices (zero-padding outside). */
        jLo = (kBase < 0)          ? -kBase              : 0;
        jHi = (kBase + nTaps > inSamples) ? inSamples - kBase : nTaps;
        if (jHi < jLo) jHi = jLo;   /* entirely outside buffer → silence */

        /* DC-gain normalisation weight for this output sample.
         * Interior samples reuse the pre-summed value; edge samples
         * recompute the partial sum for only the in-bounds taps. */
        if (jLo == 0 && jHi == nTaps) {
            wsumNorm = phaseWsum[pIdx];   /* fast path: all taps in bounds */
        } else {
            wsumNorm = 0.0f;
            for (j = jLo; j < jHi; j++)
                wsumNorm += coeffs[j];
        }

        for (ch = 0; ch < inChannels; ch++) {
            float sum = 0.0f;
            int   s;

            for (j = jLo; j < jHi; j++)
                sum += (float)in[(kBase + j) * inChannels + ch] * coeffs[j];

            if (wsumNorm > 1e-10f) sum /= wsumNorm;

            s = (int)(sum + (sum < 0.0f ? -0.5f : 0.5f));
            out[i * inChannels + ch] =
                (short)(s < -32768 ? -32768 : s > 32767 ? 32767 : s);
        }
    }

    free(phaseCoeffs);
    free(phaseWsum);
    *outSamplesOut = outN;
    return out;
}

/* Forward declaration — used by S_AL_LoadWorker before the full definition. */
static float S_AL_CalcNormGain( const void *pcm, const snd_info_t *info );

/* =========================================================================
 * Async sound-loader thread pool
 * =========================================================================
 *
 * Problem: S_AL_RegisterSound blocked the game thread for every new sound
 * (~4 ms each: 0.5 ms file I/O + 0.5 ms CalcNormGain + 3 ms ResamplePCM).
 * With 300+ sounds at map load AND lazy registration of radio calls during
 * gameplay, this caused both a long map-load stall AND mid-game stutters
 * every time an unregistered sound (radio call, new weapon variant, etc.)
 * was played for the first time.
 *
 * Solution: a persistent worker thread pool (created at S_AL_Init, destroyed
 * at S_AL_Shutdown).  S_AL_RegisterSound now:
 *   1. Calls S_CodecLoad on the game thread (~0.5 ms — keeps FS access serial).
 *   2. Copies the decoded PCM to a malloc'd buffer, frees the Hunk block.
 *   3. Submits a load job to the pool and returns the sfxHandle immediately.
 *      (r->inMemory = qfalse, r->buffer = 0 until committed.)
 *   4. Worker threads run CalcNormGain + ResamplePCM in parallel (~3.5 ms).
 *   5. S_AL_CommitCompletedJobs() (called from S_AL_Update every frame) drains
 *      the completed-jobs queue and calls qalGenBuffers + qalBufferData on the
 *      game/OpenAL thread.  After this, r->inMemory = qtrue.
 *
 * Playback guard: S_AL_StartSound already has "if (!r->inMemory || !r->buffer)
 * return;" so sounds that haven't committed yet are silently skipped for that
 * frame and play correctly on the next frame (typically < 1 frame later).
 *
 * S_AL_FlushAllJobs() (blocking) is called before any operation that would
 * invalidate sfx records (S_AL_FreeSfx, S_AL_BeginRegistration, S_AL_Shutdown).
 *
 * Number of threads: s_alLoadThreads cvar, default 4, clamped to [1, 8].
 * =========================================================================
 */

#ifndef _WIN32   /* pthreads — Linux / macOS / MinGW */

/* One async load job. */
typedef struct S_AL_LoadJob_s {
    struct S_AL_LoadJob_s *next;            /* intrusive queue link              */
    alSfxRec_t            *r;               /* sfx record (in hash, !inMemory)   */
    char                   path[MAX_QPATH]; /* for diagnostics                   */
    /* Decode output — filled on the game thread by S_AL_RegisterSound. */
    void                  *pcm;             /* malloc'd decoded PCM              */
    snd_info_t             info;            /* sample width/channels/rate/size   */
    ALenum                 fmt;             /* AL_FORMAT_* matching pcm          */
    float                  normCacheGain;   /* ≥ 0 → override; < 0 → none       */
    /* Worker output — filled by S_AL_LoadWorker. */
    void                  *submitPcm;       /* pcm or resampled (to submit)      */
    int                    submitSize;      /* byte count for qalBufferData      */
    ALenum                 submitFmt;       /* AL format for submission          */
    int                    submitRate;      /* rate for submission               */
    float                  normGain;        /* CalcNormGain result               */
    qboolean               decodeOk;        /* qfalse → defaultSound             */
} S_AL_LoadJob_t;

#define S_AL_POOL_MAX_THREADS  8

/* Shared state — all protected by s_al_jobMutex except s_al_done{Head,Tail}
 * which have their own mutex. */
static pthread_mutex_t   s_al_jobMutex   = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t    s_al_jobCond    = PTHREAD_COND_INITIALIZER;
static pthread_cond_t    s_al_allDone    = PTHREAD_COND_INITIALIZER;
static S_AL_LoadJob_t   *s_al_jobHead    = NULL; /* pending queue head          */
static S_AL_LoadJob_t   *s_al_jobTail    = NULL; /* pending queue tail          */
static int               s_al_jobsTotal  = 0;    /* pending + in-progress count */
static int               s_al_poolRunning = 0;   /* 1 while threads are alive   */
static pthread_t         s_al_workers[S_AL_POOL_MAX_THREADS];
static int               s_al_numWorkers = 0;

static pthread_mutex_t   s_al_doneMutex  = PTHREAD_MUTEX_INITIALIZER;
static S_AL_LoadJob_t   *s_al_doneHead   = NULL; /* completed queue head        */
static S_AL_LoadJob_t   *s_al_doneTail   = NULL; /* completed queue tail        */

/* Worker thread: dequeue → CalcNormGain → ResamplePCM → push to done queue. */
static void *S_AL_LoadWorker( void *unused )
{
    (void)unused;

    pthread_mutex_lock(&s_al_jobMutex);

    for (;;) {
        S_AL_LoadJob_t *job;
        short          *src16;
        short          *tmp16;

        /* Wait until there is work or the pool is shutting down. */
        while (s_al_poolRunning && !s_al_jobHead)
            pthread_cond_wait(&s_al_jobCond, &s_al_jobMutex);

        if (!s_al_jobHead) {
            /* Pool stopping with no remaining jobs — exit. */
            pthread_mutex_unlock(&s_al_jobMutex);
            return NULL;
        }

        /* Dequeue one job. */
        job          = s_al_jobHead;
        s_al_jobHead = job->next;
        if (!s_al_jobHead) s_al_jobTail = NULL;
        job->next    = NULL;
        pthread_mutex_unlock(&s_al_jobMutex);

        /* ---- CPU work (no locks held) ------------------------------------ */
        src16 = NULL;
        tmp16 = NULL;

        if (job->decodeOk) {
            /* Normalisation gain scan. */
            job->normGain = S_AL_CalcNormGain(job->pcm, &job->info);

            /* Prepare 16-bit source pointer for the resampler. */
            if (s_al_deviceFreq > 0
                    && job->info.rate != (int)s_al_deviceFreq
                    && job->info.channels <= 2) {

                if (job->info.width == 2) {
                    src16 = (short *)job->pcm;
                } else if (job->info.width == 1) {
                    /* Expand 8-bit unsigned → 16-bit signed (malloc; no Q3 zone). */
                    int  n  = job->info.samples * job->info.channels;
                    int  ii;
                    tmp16 = (short *)malloc((size_t)n * sizeof(short));
                    if (tmp16) {
                        const byte *p = (const byte *)job->pcm;
                        for (ii = 0; ii < n; ii++)
                            tmp16[ii] = (short)(((int)p[ii] - 128) << 8);
                        src16 = tmp16;
                    }
                }

                if (src16) {
                    int    outSamples = 0;
                    short *resampled  = S_AL_ResamplePCM(
                                            src16,
                                            job->info.samples,
                                            job->info.channels,
                                            job->info.rate,
                                            (int)s_al_deviceFreq,
                                            &outSamples);
                    if (tmp16) { free(tmp16); tmp16 = NULL; }

                    if (resampled) {
                        /* Resampled buffer becomes the submission target. */
                        job->submitPcm  = resampled;
                        job->submitFmt  = (job->info.channels == 1)
                                          ? AL_FORMAT_MONO16
                                          : AL_FORMAT_STEREO16;
                        job->submitRate = (int)s_al_deviceFreq;
                        job->submitSize = outSamples * job->info.channels
                                          * (int)sizeof(short);
                        /* Original PCM is no longer needed. */
                        free(job->pcm);
                        job->pcm = NULL;
                    }
                }
            }

            /* If no resampling was done, submit the original PCM as-is. */
            if (!job->submitPcm) {
                job->submitPcm  = job->pcm;
                job->submitFmt  = job->fmt;
                job->submitRate = job->info.rate;
                job->submitSize = job->info.size;
            }
        }

        /* ---- Push to done queue (done mutex) ----------------------------- */
        pthread_mutex_lock(&s_al_doneMutex);
        if (s_al_doneTail)
            s_al_doneTail->next = job;
        else
            s_al_doneHead = job;
        s_al_doneTail = job;
        pthread_mutex_unlock(&s_al_doneMutex);

        /* Decrement total counter under job mutex; signal "all done" if 0. */
        pthread_mutex_lock(&s_al_jobMutex);
        s_al_jobsTotal--;
        if (s_al_jobsTotal == 0)
            pthread_cond_broadcast(&s_al_allDone);
    }
    /* unreachable */
}

/* Submit a job to the pending queue.  Called on the game thread. */
static void S_AL_SubmitLoadJob( S_AL_LoadJob_t *job )
{
    pthread_mutex_lock(&s_al_jobMutex);
    job->next = NULL;
    if (s_al_jobTail)
        s_al_jobTail->next = job;
    else
        s_al_jobHead = job;
    s_al_jobTail = job;
    s_al_jobsTotal++;
    pthread_cond_signal(&s_al_jobCond);
    pthread_mutex_unlock(&s_al_jobMutex);
}

/* Drain the completed-jobs queue and call qalBufferData for each.
 * Must be called on the game/OpenAL thread (typically from S_AL_Update). */
static void S_AL_CommitCompletedJobs( void )
{
    S_AL_LoadJob_t *list, *job;

    /* Atomically detach the entire done list. */
    pthread_mutex_lock(&s_al_doneMutex);
    list          = s_al_doneHead;
    s_al_doneHead = NULL;
    s_al_doneTail = NULL;
    pthread_mutex_unlock(&s_al_doneMutex);

    while (list) {
        alSfxRec_t *r;
        float       ng;

        job  = list;
        list = job->next;

        r = job->r;

        if (!job->decodeOk || !job->submitPcm) {
            /* Decode failed — mark as default sound so playback skips it. */
            r->defaultSound = qtrue;
            r->inMemory     = qtrue;
            r->normGain     = 1.0f;
            r->lastTimeUsed = Com_Milliseconds();
            if (job->pcm)       { free(job->pcm);       job->pcm       = NULL; }
            if (job->submitPcm) { free(job->submitPcm); job->submitPcm = NULL; }
            free(job);
            continue;
        }

        /* Apply normcache override if one was stored at register-time. */
        ng = (job->normCacheGain >= 0.0f) ? job->normCacheGain : job->normGain;
        r->normGain = ng;

        /* Upload to OpenAL on this (game) thread. */
        S_AL_ClearError("CommitJob");
        qalGenBuffers(1, &r->buffer);
        if (!S_AL_CheckError("alGenBuffers(async)")) {
            qalBufferData(r->buffer, job->submitFmt,
                          job->submitPcm, job->submitSize, job->submitRate);
            if (S_AL_CheckError("alBufferData(async)")) {
                qalDeleteBuffers(1, &r->buffer);
                r->buffer      = 0;
                r->defaultSound = qtrue;
            }
        } else {
            r->defaultSound = qtrue;
        }

        r->soundLength  = job->info.samples;
        r->fileRate     = job->info.rate;
        r->inMemory     = qtrue;
        r->lastTimeUsed = Com_Milliseconds();

        Com_DPrintf("S_AL: committed %s (%d Hz→%d Hz, %d ch, normGain=%.3f)\n",
            job->path, job->info.rate, job->submitRate,
            job->info.channels, ng);

        /* Free buffers. submitPcm is either the resampled buffer (malloc'd)
         * or points into pcm.  Only free each pointer once. */
        if (job->submitPcm && job->submitPcm != job->pcm)
            free(job->submitPcm);
        if (job->pcm)
            free(job->pcm);
        free(job);
    }
}

/* Block until all submitted jobs have been processed and committed.
 * Must be called before invalidating sfx records (FreeSfx, BeginRegistration,
 * Shutdown).  Also drains the done queue so OpenAL buffers are created. */
static void S_AL_FlushAllJobs( void )
{
    if (!s_al_numWorkers) return;

    /* Wait until every in-flight job has been processed by a worker. */
    pthread_mutex_lock(&s_al_jobMutex);
    while (s_al_jobsTotal > 0)
        pthread_cond_wait(&s_al_allDone, &s_al_jobMutex);
    pthread_mutex_unlock(&s_al_jobMutex);

    /* Commit all completed jobs now (caller is on game thread). */
    S_AL_CommitCompletedJobs();
}

/* Start N worker threads.  Called from S_AL_Init. */
static void S_AL_StartThreadPool( int n )
{
    int i;
    if (n < 1) n = 1;
    if (n > S_AL_POOL_MAX_THREADS) n = S_AL_POOL_MAX_THREADS;

    s_al_poolRunning = 1;
    s_al_numWorkers  = 0;

    for (i = 0; i < n; i++) {
        if (pthread_create(&s_al_workers[i], NULL, S_AL_LoadWorker, NULL) != 0) {
            Com_Printf(S_COLOR_YELLOW "S_AL: failed to create load worker %d\n", i);
            break;
        }
        s_al_numWorkers++;
    }

    Com_Printf("S_AL: async load pool: %d worker thread(s)\n", s_al_numWorkers);
}

/* Drain queue, join all worker threads.  Called from S_AL_Shutdown. */
static void S_AL_StopThreadPool( void )
{
    int i;
    if (!s_al_numWorkers) return;

    /* Signal workers to exit after draining remaining work. */
    pthread_mutex_lock(&s_al_jobMutex);
    s_al_poolRunning = 0;
    pthread_cond_broadcast(&s_al_jobCond);
    pthread_mutex_unlock(&s_al_jobMutex);

    for (i = 0; i < s_al_numWorkers; i++)
        pthread_join(s_al_workers[i], NULL);

    s_al_numWorkers = 0;

    /* Commit anything that was completed but not yet drained. */
    S_AL_CommitCompletedJobs();

    /* Discard any jobs still on the pending queue (shouldn't happen after
     * flush, but be defensive). */
    pthread_mutex_lock(&s_al_jobMutex);
    while (s_al_jobHead) {
        S_AL_LoadJob_t *j = s_al_jobHead;
        s_al_jobHead = j->next;
        if (j->submitPcm && j->submitPcm != j->pcm) free(j->submitPcm);
        if (j->pcm) free(j->pcm);
        free(j);
    }
    s_al_jobTail   = NULL;
    s_al_jobsTotal = 0;
    pthread_mutex_unlock(&s_al_jobMutex);
}

#else  /* _WIN32 — no pthreads; async loading disabled, sync fallback used */

typedef struct S_AL_LoadJob_s { int unused; } S_AL_LoadJob_t;
static void S_AL_SubmitLoadJob(S_AL_LoadJob_t *j)   { (void)j; }
static void S_AL_CommitCompletedJobs( void )         {}
static void S_AL_FlushAllJobs( void )                {}
static void S_AL_StartThreadPool( int n )            { (void)n; }
static void S_AL_StopThreadPool( void )              {}
static int  s_al_numWorkers = 0;

#endif  /* _WIN32 */


/* Compute a per-sample RMS ceiling limiter gain for ambient sources.
 *
 * Samples the decoded PCM file at a regular stride so that the ENTIRE loop
 * body is represented, not just the opening seconds.  The old approach (scan
 * only the first S_AL_NORM_SCAN_SAMPLES frames) produced incorrect results
 * for ambient tracks with a quiet intro followed by a loud main body — the
 * quiet intro inflated normGain and the rest of the loop played too loud.
 *
 * The stride is chosen so that at most S_AL_NORM_SCAN_SAMPLES frames are
 * examined.  For files shorter than the cap, stride = 1 (every frame).
 *
 * DESIGN: one-sided ceiling, not a bidirectional normalizer.
 *
 *   normGain = min( 1.0,  S_AL_NORM_CEILING / RMS )
 *
 *   • Sounds with RMS ≤ S_AL_NORM_CEILING  → normGain = 1.0  (natural level,
 *     no boost, no cut).  A quiet breeze stays quiet; a moderately loud
 *     waterfall stays at its intended loudness.
 *   • Sounds with RMS  > S_AL_NORM_CEILING  → normGain < 1.0  (proportional
 *     cut that brings the sound DOWN to the ceiling level).  Only genuine
 *     outliers — heavily-compressed, broadcast-hot, or clipped ambient WAVs
 *     — receive any attenuation.
 *
 * This preserves the relative loudness relationship between different ambient
 * sounds on the same map (intended-to-be-louder sounds stay louder) while
 * preventing any single excessively-recorded loop from dominating the mix.
 *
 * Returns 1.0 (no adjustment) for:
 *   • empty or silent files
 *   • unsupported bit depths */
#define S_AL_NORM_SCAN_SAMPLES  (44100 * 4)   /* max sample frames to analyse */
#define S_AL_NORM_CEILING       0.316f         /* ≈ −10 dBFS; sounds above cut to here */

static float S_AL_CalcNormGain( const void *pcm, const snd_info_t *info )
{
    double   sumSq = 0.0;
    long     count = 0;
    float    rms, normGain;
    long     totalFrames, stride, scanFrames, i;

    if (!pcm || info->samples <= 0 || info->size <= 0)
        return 1.0f;

    /* Clamp totalFrames to what info->size actually allows — guards against
     * codec-reported sample counts that exceed the allocated buffer. */
    {
        long maxFromSize = (long)(info->size / ((long)info->width * info->channels));
        totalFrames = (info->samples < maxFromSize) ? info->samples : maxFromSize;
    }
    if (totalFrames <= 0) return 1.0f;

    /* Stride: evenly spaced across the full file.  stride=1 when the file
     * fits within the scan cap so no precision is lost on short sounds. */
    stride     = (totalFrames > S_AL_NORM_SCAN_SAMPLES)
                 ? (totalFrames / S_AL_NORM_SCAN_SAMPLES) : 1;
    scanFrames = totalFrames / stride;
    if (scanFrames < 1) scanFrames = 1;

    if (info->width == 2) {
        /* 16-bit signed PCM */
        const short *s = (const short *)pcm;
        long ch;
        for (i = 0; i < scanFrames; i++) {
            long base = (i * stride) * info->channels;
            for (ch = 0; ch < info->channels; ch++) {
                double v = s[base + ch] / 32768.0;
                sumSq += v * v;
            }
        }
        count = scanFrames * info->channels;
    } else if (info->width == 1) {
        /* 8-bit unsigned PCM (centre = 128) */
        const byte *s = (const byte *)pcm;
        long ch;
        for (i = 0; i < scanFrames; i++) {
            long base = (i * stride) * info->channels;
            for (ch = 0; ch < info->channels; ch++) {
                double v = (s[base + ch] - 128) / 128.0;
                sumSq += v * v;
            }
        }
        count = scanFrames * info->channels;
    } else {
        return 1.0f;  /* unsupported depth */
    }

    if (count == 0) return 1.0f;

    rms = (float)sqrt(sumSq / (double)count);
    if (rms < 1e-6f) return 1.0f;   /* silent — don't amplify noise floor */

    /* One-sided ceiling: only cut sounds that exceed the ceiling.
     * Sounds at or below the ceiling play at their natural level (1.0).
     * The hard floor at 0.05 is a safety net for truly pathological files
     * (e.g. fully-clipped square waves); it is never reached by normally
     * encoded audio since ceiling/1.0 = 0.316 > 0.05 at maximum RMS. */
    normGain = S_AL_NORM_CEILING / rms;
    if (normGain > 1.0f)  normGain = 1.0f;   /* below ceiling — no change */
    if (normGain < 0.05f) normGain = 0.05f;  /* safety floor */

    return normGain;
}

/* Load a sound file into an AL buffer.  Returns the sfxHandle_t index
 * (>= 0) on success, 0 (the default-sound slot) on failure.
 *
 * The async path (non-Windows, pool running) submits CPU-intensive work
 * (CalcNormGain + ResamplePCM) to the thread pool and returns immediately
 * with a valid handle.  r->inMemory = qfalse until S_AL_CommitCompletedJobs
 * (called from S_AL_Update) uploads the buffer.  Playback guards already in
 * S_AL_StartSound / S_AL_AddLoopingSound skip sounds that are not yet ready;
 * they play correctly from the very next frame. */
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

    /* Allocate new record and insert into hash. */
    r = S_AL_AllocSfx();
    if (!r)
        return 0;

    Q_strncpyz(r->name, sample, sizeof(r->name));
    h               = S_AL_SfxHash(sample);
    r->next         = s_al_sfxHash[h];
    s_al_sfxHash[h] = r;

    /* Decode via codec (always on the game thread — FS is not thread-safe). */
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

#ifndef _WIN32
    /* -----------------------------------------------------------------------
     * Async path: hand CPU work off to the thread pool.
     * The game thread is only blocked for the S_CodecLoad above (~0.5 ms).
     * CalcNormGain + ResamplePCM (~3.5 ms combined) run in a worker thread.
     * S_AL_CommitCompletedJobs (called from S_AL_Update every frame) uploads
     * the result to OpenAL and sets r->inMemory = qtrue.
     * ----------------------------------------------------------------------- */
    if (s_al_numWorkers > 0) {
        S_AL_LoadJob_t *job;
        void           *pcmMalloc;
        int             ci;

        /* Copy PCM from the Hunk stack allocator to a free-standing malloc
         * buffer so the worker thread can hold it after we return and the
         * Hunk block can be recycled immediately. */
        pcmMalloc = malloc((size_t)info.size);
        if (!pcmMalloc) {
            Hunk_FreeTempMemory(pcm);
            r->defaultSound = qtrue;
            r->inMemory     = qtrue;
            r->normGain     = 1.0f;
            r->lastTimeUsed = Com_Milliseconds();
            return (sfxHandle_t)(r - s_al_sfx);
        }
        memcpy(pcmMalloc, pcm, (size_t)info.size);
        Hunk_FreeTempMemory(pcm);
        pcm = NULL;

        job = (S_AL_LoadJob_t *)malloc(sizeof(S_AL_LoadJob_t));
        if (!job) {
            free(pcmMalloc);
            r->defaultSound = qtrue;
            r->inMemory     = qtrue;
            r->normGain     = 1.0f;
            r->lastTimeUsed = Com_Milliseconds();
            return (sfxHandle_t)(r - s_al_sfx);
        }
        memset(job, 0, sizeof(*job));
        job->r             = r;
        Q_strncpyz(job->path, sample, sizeof(job->path));
        job->pcm           = pcmMalloc;
        job->info          = info;
        job->fmt           = fmt;
        job->normCacheGain = -1.0f;   /* -1 = no override */
        job->decodeOk      = qtrue;

        /* Capture any normcache override so the worker can apply it without
         * touching the shared normcache table. */
        for (ci = 0; ci < s_al_normCacheCount; ci++) {
            if (!Q_stricmp(s_al_normCache[ci].path, sample)) {
                job->normCacheGain = s_al_normCache[ci].normGain;
                break;
            }
        }

        S_AL_SubmitLoadJob(job);
        r->lastTimeUsed = Com_Milliseconds();
        /* r->inMemory = qfalse, r->buffer = 0 until CommitCompletedJobs. */
        return (sfxHandle_t)(r - s_al_sfx);
    }
#endif /* !_WIN32 */

    /* -----------------------------------------------------------------------
     * Synchronous path (Windows, or pool not yet started).
     * Identical to the pre-threading code: CalcNormGain → normcache override
     * → qalGenBuffers → ResamplePCM → qalBufferData, all inline.
     * ----------------------------------------------------------------------- */

    /* Analyse PCM while it is still in the temp allocation. */
    r->normGain = S_AL_CalcNormGain(pcm, &info);

    /* Per-map normcache override: if the player has an existing cache entry
     * for this sound (loaded by S_AL_BeginRegistration before cgame registers
     * its sounds), prefer it over the freshly computed gain.  This lets the
     * cache drive normGain even when the sfx record is reused from a previous
     * map session (S_AL_FindSfx returned an existing record above and skipped
     * this code path — those records are updated in S_AL_BeginRegistration
     * directly, so this branch only fires for genuinely new registrations). */
    {
        int ci;
        for (ci = 0; ci < s_al_normCacheCount; ci++) {
            if (!Q_stricmp(s_al_normCache[ci].path, sample)) {
                r->normGain = s_al_normCache[ci].normGain;
                break;
            }
        }
    }

    S_AL_ClearError("RegisterSound");
    qalGenBuffers(1, &r->buffer);
    if (S_AL_CheckError("alGenBuffers")) {
        Hunk_FreeTempMemory(pcm);
        return 0;
    }

    /* Pre-resample to the device's native rate when rates differ.
     * This converts the PCM exactly once at load time, so OpenAL receives
     * native-rate audio and its internal per-playback resampler never runs.
     * Eliminating that resampler prevents aliasing, interpolation overshoot,
     * and the crackling amplification inherent in all lower-quality resamplers
     * (ZOH, Linear) that may be the only option on older OpenAL Soft builds.
     * For 8-bit sources we expand to 16-bit first so the sinc filter can
     * work at full precision. */
    {
        void   *pcmToSubmit  = pcm;
        ALenum  fmtToSubmit  = fmt;
        int     rateToSubmit = info.rate;
        int     sizeToSubmit = info.size;
        short  *resampled    = NULL;
        short  *tmp16        = NULL;

        if (s_al_deviceFreq > 0 && info.rate != (int)s_al_deviceFreq
                && info.channels <= 2) {
            const short *src16 = NULL;

            if (info.width == 2) {
                src16 = (const short *)pcm;
            } else if (info.width == 1) {
                /* Expand 8-bit unsigned PCM to 16-bit signed. */
                int n = info.samples * info.channels, ii;
                tmp16 = (short *)malloc((size_t)n * sizeof(short));
                if (tmp16) {
                    const byte *p = (const byte *)pcm;
                    for (ii = 0; ii < n; ii++)
                        tmp16[ii] = (short)(((int)p[ii] - 128) << 8);
                    src16 = tmp16;
                }
            }

            if (src16) {
                int outSamples = 0;
                resampled = S_AL_ResamplePCM(src16, info.samples, info.channels,
                                              info.rate, (int)s_al_deviceFreq,
                                              &outSamples);
                if (resampled) {
                    pcmToSubmit  = resampled;
                    fmtToSubmit  = (info.channels == 1)
                                    ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
                    rateToSubmit = (int)s_al_deviceFreq;
                    sizeToSubmit = outSamples * info.channels * (int)sizeof(short);

                    if (s_alDebugPlayback && s_alDebugPlayback->integer >= 1) {
                        Com_Printf("[alDbg] pre-resampled: %s  %d Hz → %d Hz"
                                   "  (%d → %d smp)\n",
                                   sample, info.rate, rateToSubmit,
                                   info.samples, outSamples);
                    }
                }
            }
        }

        qalBufferData(r->buffer, fmtToSubmit, pcmToSubmit, sizeToSubmit, rateToSubmit);

        if (tmp16)     { free(tmp16);     tmp16     = NULL; }
        if (resampled) { free(resampled); resampled = NULL; }
        Hunk_FreeTempMemory(pcm);

        /* Debug: if rate still differs (device freq unknown or alloc failed),
         * log so the operator knows the internal resampler will be used. */
        if (s_alDebugPlayback && s_alDebugPlayback->integer >= 1
                && rateToSubmit != (int)s_al_deviceFreq && s_al_deviceFreq > 0) {
            Com_Printf(S_COLOR_YELLOW
                "[alDbg] rate mismatch (pre-resample skipped): %s"
                "  file=%d Hz  device=%d Hz  normGain=%.3f\n",
                sample, info.rate, (int)s_al_deviceFreq, r->normGain);
        }
    }

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
    Com_DPrintf("S_AL: loaded %s (%d Hz, %d ch, %d smp, normGain=%.3f)\n",
        sample, info.rate, info.channels, info.samples, r->normGain);

    return (sfxHandle_t)idx;
}

/* Free all loaded sfx. */
static void S_AL_FreeSfx( void )
{
    int i;

    /* Drain all in-flight async jobs before touching the sfx table.
     * Any jobs still running hold pointers into s_al_sfx[]; wiping the
     * table while they are live would cause a use-after-free. */
    S_AL_FlushAllJobs();

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

/* Return the per-category volume scalar for a source.
 *
 * Scale: 0–10, reference unity at 1.0.
 *   cvar = 1.0  → same loudness as the old hardcoded defaults.
 *   cvar = 0.5  → ~0.25 × ref  (−12 dB via square curve, clearly quieter).
 *   cvar = 2.0  → 2.0 × ref   (+6 dB, noticeably louder).
 *   cvar = 0.0  → silence.
 *
 * A power-2 curve (v²) is applied in the 0–1 attenuation region so that
 * small reductions feel proportional to loudness perception.  Above 1.0
 * the value is used linearly (modest boost zone).
 *
 * Reference gains (baked in, not in the default cvar value):
 *   WORLD   0.70  · WEAPON  1.00  · IMPACT  0.55
 *   LOCAL   1.00  · AMBIENT 0.30  · UI      0.80
 */

/* Return qtrue when the sound should be treated as suppressed/silenced.
 *
 * Both checks always run independently and the result is OR'd together.
 * Both are cheap string operations (strchr + Q_stristr), so there is no
 * reason to short-circuit — running both catches edge cases where only
 * one signal is available.
 *
 * GEAR CHECK — g_gear configstring 'U' flag:
 *   The server writes each player's equipped gear into CS_PLAYERS under key
 *   "g". Character 'U' = suppressor attachment fitted.  Most reliable: directly
 *   reflects game state regardless of audio asset naming.  Valid for player
 *   entities only (entnum 0..MAX_CLIENTS-1).  In practice suppressors are
 *   rarely swapped mid-round, but checking both signals every call costs only
 *   a strchr and is the correct defensive approach.
 *
 * FILENAME CHECK — s_alSuppressedSoundPattern substring match:
 *   URT suppressed file names from pk3 audit:
 *     *_sil.wav  — dominant (de_sil, m4_sil, ak103_sil, beretta_sil, …)
 *     *-sil.wav  — mac11 (mac11-sil.wav)
 *     *_silenced.wav — p90 (p90_silenced.wav)
 *   Covered by default tokens: _sil, -sil, silenced.
 *   Also catches non-player entities that play suppressed audio. */
static qboolean S_AL_IsSoundSuppressed( sfxHandle_t sfx, int entnum )
{
    qboolean gearSuppressed = qfalse;
    qboolean nameSuppressed = qfalse;

    /* --- Gear string check (always run for valid player entities) ------- */
    if (entnum >= 0 && entnum < MAX_CLIENTS) {
        const char *csStr = cl.gameState.stringData
                            + cl.gameState.stringOffsets[CS_PLAYERS + entnum];
        if (csStr && csStr[0]) {
            const char *gearStr = Info_ValueForKey(csStr, "g");
            if (gearStr && strchr(gearStr, 'U'))
                gearSuppressed = qtrue;
        }
    }

    /* --- Filename pattern check (always run) ---------------------------- */
    if (s_alSuppressedSoundPattern) {
        const char *pat  = s_alSuppressedSoundPattern->string;
        const char *name = (sfx >= 0 && sfx < s_al_numSfx)
                           ? s_al_sfx[sfx].name : NULL;
        if (pat && pat[0] && name && name[0]) {
            char        tok[64];
            const char *p = pat;
            while (*p && !nameSuppressed) {
                int i = 0;
                while (*p && *p != ',' && i < (int)sizeof(tok) - 1)
                    tok[i++] = *p++;
                tok[i] = '\0';
                if (*p == ',') p++;
                {
                    char *s = tok, *e;
                    while (*s == ' ') s++;
                    e = s + strlen(s);
                    while (e > s && *(e-1) == ' ') *--e = '\0';
                    if (s[0] && Q_stristr(name, s))
                        nameSuppressed = qtrue;
                }
            }
        }
    }

    return gearSuppressed || nameSuppressed;
}

/* Return qtrue when the sound file name matches one of the s_alExtraVolEntryN
 * slots, and write the per-sample linear gain scale into *outSampleScale (1.0
 * when the matched slot carries no ":-N" dB suffix).
 *
 * Each slot holds exactly one pattern string with the following syntax:
 *   token           — case-insensitive substring match against the sound path.
 *                     e.g. "sound/feedback" matches every file under that dir.
 *                     e.g. "sound/feedback/hit.wav" matches only that file.
 *   token:-N        — same match, but apply an N dB cut (N ≤ 0; floor −40 dB;
 *                     fractional values accepted; values > 0 clamped to 0).
 *                     e.g. "sound/feedback/hit.wav:-2.5" applies −2.5 dB.
 *   !token          — exclusion: if the name contains this substring the sound
 *                     is removed from the group even if a positive slot matched.
 *   !token:-N       — exclusion with ignored dB suffix (parsed but discarded).
 *
 * A sound is routed to SRC_CAT_EXTRAVOL when at least one positive slot
 * matches AND no exclusion slot matches.  Empty slots are skipped. */
static qboolean S_AL_IsExtraVolSound( sfxHandle_t sfx, float *outSampleScale )
{
    const char *name;
    qboolean    included  = qfalse;
    qboolean    excluded  = qfalse;
    float       bestScale = 1.0f;
    int         i;

    if (outSampleScale) *outSampleScale = 1.0f;

    name = (sfx >= 0 && sfx < s_al_numSfx) ? s_al_sfx[sfx].name : NULL;
    if (!name || !name[0])
        return qfalse;

    for (i = 0; i < S_AL_EXTRAVOL_SLOTS; i++) {
        char      tok[MAX_QPATH];
        char     *s, *e, *colon;
        float     sampleScale = 1.0f;
        qboolean  isExclusion = qfalse;

        if (!s_alExtraVolEntry[i] || !s_alExtraVolEntry[i]->string[0])
            continue;

        /* copy into a mutable buffer for in-place trimming/truncation */
        Q_strncpyz(tok, s_alExtraVolEntry[i]->string, sizeof(tok));
        s = tok;

        /* trim leading whitespace */
        while (*s == ' ') s++;
        e = s + strlen(s);
        /* trim trailing whitespace */
        while (e > s && *(e-1) == ' ') *--e = '\0';
        if (!s[0]) continue;

        /* strip optional ":-N" dB suffix */
        colon = strrchr(s, ':');
        if (colon && colon != s) {
            float db = Q_atof(colon + 1);
            if (db > 0.0f)   db = 0.0f;    /* cuts only, no boosts */
            if (db < -40.0f) db = -40.0f;  /* floor at -40 dB */
            sampleScale = powf(10.0f, db / 20.0f);
            *colon = '\0'; /* truncate before the colon */
        }

        /* check for exclusion prefix */
        if (s[0] == '!') {
            isExclusion = qtrue;
            s++; /* skip '!' */
        }
        if (!s[0]) continue;

        if (isExclusion) {
            if (Q_stristr(name, s))
                excluded = qtrue;
        } else {
            if (Q_stristr(name, s)) {
                included  = qtrue;
                bestScale = sampleScale;
            }
        }
    }

    if (included && !excluded) {
        if (outSampleScale) *outSampleScale = bestScale;
        return qtrue;
    }
    return qfalse;
}

/* Anti-cheat: SRC_CAT_WORLD and SRC_CAT_IMPACT are capped at 2.0 so that
 * other players/impacts cannot be amplified to wallhack-level volume. */
static float S_AL_GetCategoryVol( alSrcCat_t cat )
{
    float v, ref, maxV;
    switch (cat) {
    case SRC_CAT_LOCAL:
        v    = s_alVolSelf   ? s_alVolSelf->value   : 1.0f;
        ref  = 1.00f;  maxV = 10.0f;
        break;
    case SRC_CAT_WEAPON:
        v    = s_alVolWeapon ? s_alVolWeapon->value : 1.0f;
        ref  = 1.00f;  maxV = 10.0f;
        break;
    case SRC_CAT_WEAPON_SUPPRESSED:
        v    = s_alVolSuppressedWeapon ? s_alVolSuppressedWeapon->value : 1.0f;
        ref  = 0.55f;  maxV = 10.0f;   /* own suppressed: inherently quieter */
        break;
    case SRC_CAT_WORLD_SUPPRESSED:
        v    = s_alVolEnemySuppressedWeapon ? s_alVolEnemySuppressedWeapon->value : 1.0f;
        ref  = 0.45f;  maxV = 2.0f;    /* enemy suppressed: quieter + anti-cheat cap */
        break;
    case SRC_CAT_UI:
        v    = s_alVolUI     ? s_alVolUI->value     : 1.0f;
        ref  = 0.80f;  maxV = 10.0f;
        break;
    case SRC_CAT_AMBIENT:
        v    = s_alVolEnv    ? s_alVolEnv->value    : 1.0f;
        ref  = 0.30f;  maxV = 10.0f;
        break;
    case SRC_CAT_IMPACT:
        v    = s_alVolImpact ? s_alVolImpact->value : 1.0f;
        ref  = 0.55f;  maxV = 2.0f;   /* anti-cheat cap */
        break;
    case SRC_CAT_WORLD:
    default:
        v    = s_alVolOther  ? s_alVolOther->value  : 1.0f;
        ref  = 0.70f;  maxV = 2.0f;   /* anti-cheat cap */
        break;
    case SRC_CAT_EXTRAVOL:
        v    = s_alExtraVol  ? s_alExtraVol->value  : 1.0f;
        ref  = 0.70f;  maxV = 1.0f;   /* reduce-only: 30% reduction at cvar=1.0 */
        break;
    }
    if (v < 0.0f) v = 0.0f;
    if (v > maxV) v = maxV;
    /* Power-2 curve in the attenuation zone [0,1] for perceptual linearity.
     * Above 1.0, use linear (boost zone). */
    if (v <= 1.0f)
        v = v * v;
    return ref * v;
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
        /* Doorway-scale search radius — default 120 u (≈ URT door half-width).
         * Tunable via s_alOccSearchRadius: increase to reach further around
         * thick walls or wide columns, decrease for tighter gap sensitivity. */
        float SEARCH_RADIUS = s_alOccSearchRadius
                              ? s_alOccSearchRadius->value : 120.0f;
        if (SEARCH_RADIUS < 20.f) SEARCH_RADIUS = 20.f;

        vec3_t fwd, right, upv;
        float  srcDist;
        int    k;

        /* Orthonormal frame: fwd = listener→source, right/up = tangent plane */
        VectorSubtract(srcOrigin, s_al_listener_origin, fwd);
        srcDist = VectorNormalize(fwd);

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

        /* ---- Multi-hop corridor probing ----
         * When near-source probes can't find a gap (thick wall, corridor far
         * from source), try two-hop paths: listener → waypoint → source.
         * Casts rays from the listener in 8 world-space horizontal directions
         * to find corridor openings, then checks if those openings can reach
         * the source.  Approximates sound traveling through corridors without
         * a full pathfinding graph.
         *
         * Each hop direction is skipped if it points within ~30° of the
         * direct listener→source line (already covered by the direct trace).
         * The hop score is penalised by the path-length ratio so longer
         * corridors attenuate more than short doorways. */
        /* Budget check: reset counter at the start of each new audio frame,
         * then skip multi-hop once the per-frame limit is reached.
         * This prevents a burst of simultaneous sound events (e.g. 32 players
         * firing at once) from piling up hundreds of CM_BoxTrace calls in a
         * single tick and starving the audio buffer fill thread. */
        if (s_al_multihop_frame != s_al_loopFrame) {
            s_al_multihop_frame = s_al_loopFrame;
            s_al_multihop_used  = 0;
        }
        if (bestFrac < 0.5f && srcDist > 200.f
                && s_al_multihop_used < S_AL_MULTIHOP_BUDGET) {
            s_al_multihop_used++;
            static const float hopDirs[8][3] = {
                { 1.f, 0.f, 0.f}, {-1.f,  0.f, 0.f},
                { 0.f, 1.f, 0.f}, { 0.f, -1.f, 0.f},
                { 0.707f,  0.707f, 0.f}, { 0.707f, -0.707f, 0.f},
                {-0.707f,  0.707f, 0.f}, {-0.707f, -0.707f, 0.f}
            };
            float hopReach = srcDist * 0.6f;
            if (hopReach > 1200.f) hopReach = 1200.f;
            if (hopReach < 200.f)  hopReach = 200.f;

            for (k = 0; k < 8; k++) {
                trace_t hop1, hop2;
                vec3_t  waypoint, hopEnd;
                float   leg1Dist, leg2Dist, pathLen, hopScore, dot;

                /* Skip directions too close to the direct path —
                 * already covered by the direct trace + near-source probes. */
                dot = fwd[0]*hopDirs[k][0] + fwd[1]*hopDirs[k][1]
                    + fwd[2]*hopDirs[k][2];
                if (dot > 0.85f) continue;

                /* Leg 1: listener → corridor direction */
                VectorMA(s_al_listener_origin, hopReach, hopDirs[k], hopEnd);
                CM_BoxTrace(&hop1, s_al_listener_origin, hopEnd,
                            vec3_origin, vec3_origin,
                            0, CONTENTS_SOLID, qfalse);
                if (hop1.fraction < 0.15f) continue; /* immediate wall */

                /* Waypoint = slightly before the wall hit */
                leg1Dist = hopReach * hop1.fraction;
                VectorMA(s_al_listener_origin, leg1Dist * 0.95f,
                         hopDirs[k], waypoint);

                /* Leg 2: waypoint → source */
                CM_BoxTrace(&hop2, waypoint, srcOrigin,
                            vec3_origin, vec3_origin,
                            0, CONTENTS_SOLID, qfalse);

                /* Score = clearness of both legs × distance penalty.
                 * Longer corridor paths get more attenuation than short
                 * doorway paths (triangle inequality: pathLen ≥ srcDist).
                 *
                 * Hard-cap: multi-hop corridor sound should never exceed
                 * ~40% of direct LOS.  Sound bending around corners loses
                 * significant energy; without this cap the wet sends
                 * (reverb+echo+EQ) stack on top and cause clipping. */
                hopScore = hop1.fraction * hop2.fraction;
                {
                    vec3_t leg2Vec;
                    VectorSubtract(srcOrigin, waypoint, leg2Vec);
                    leg2Dist = VectorLength(leg2Vec);
                }
                pathLen = leg1Dist + leg2Dist;
                if (pathLen > srcDist && srcDist > 1.f)
                    hopScore *= srcDist / pathLen;
                if (hopScore > 0.40f)
                    hopScore = 0.40f;

                if (hopScore > bestFrac) {
                    bestFrac = hopScore;
                    /* Do NOT update acousticPos from multi-hop waypoints.
                     * The 8 fixed world-space hop directions produce
                     * different "winners" as the listener moves, causing
                     * the acoustic position to oscillate between corridors.
                     * With HRTF this oscillation is audible as crackling;
                     * without HRTF it causes panning wobble.
                     * Only near-source probes (above) update direction. */
                }
            }
        }
    }

    /* ---- BSP area connectivity floor (distance-scaled) ----
     * Even when all traces hit solid walls, BSP areas connected through
     * portal chains (corridors, tunnels, doorways) should still allow
     * sound through — muffled but audible.  This catches long winding
     * corridors that neither near-source nor multi-hop probes can span.
     *
     * The floor fades linearly with distance: full strength at refDist
     * (80 u), zero at maxDist (1330 u).  Close sources stay prominent;
     * distant ones fade naturally with the distance model.
     *
     * Same area    → strong floor (0.50 × distScale)
     * Connected    → tunable floor (s_alOccAreaFloor × distScale)
     * Disconnected → no floor: sealed areas (closed doors) stay silent. */
    {
        int srcLeaf = CM_PointLeafnum( srcOrigin );
        int lstLeaf = CM_PointLeafnum( s_al_listener_origin );
        int srcArea = CM_LeafArea( srcLeaf );
        int lstArea = CM_LeafArea( lstLeaf );

        if (srcArea == lstArea || CM_AreasConnected( srcArea, lstArea )) {
            vec3_t toSrc;
            float  dist, maxDist, refDist, distScale, floor;

            VectorSubtract( srcOrigin, s_al_listener_origin, toSrc );
            dist    = VectorLength( toSrc );
            maxDist = s_alMaxDist ? s_alMaxDist->value : S_AL_SOUND_MAXDIST;
            refDist = S_AL_SOUND_FULLVOLUME;

            if (dist <= refDist)
                distScale = 1.0f;
            else if (dist >= maxDist)
                distScale = 0.0f;
            else
                distScale = 1.0f - (dist - refDist) / (maxDist - refDist);

            if (srcArea == lstArea)
                floor = 0.5f * distScale;
            else {
                float base = s_alOccAreaFloor ? s_alOccAreaFloor->value : 0.30f;
                floor = base * distScale;
            }

            if (bestFrac < floor) {
                bestFrac = floor;
                /* Floor wins: the BSP graph says areas are connected, so let
                 * the sound through at the floor level — but there is no
                 * meaningful "gap position" to redirect toward.  Keep the
                 * acoustic position at the real source so the projection step
                 * below does not amplify a stale multi-hop waypoint (which is
                 * near the listener, not the source) into a completely wrong
                 * apparent direction that changes every trace tick. */
                VectorCopy(srcOrigin, acousticPos);
            }
        }
    }

    /* Project the best-gap position onto the listener→source distance.
     *
     * Without projection acousticPos sits only SEARCH_RADIUS units from the
     * real source.  The listener→acousticPos angle is nearly identical to
     * listener→source, giving < 1° apparent HRTF shift even at high pBlend
     * values (a 120 u gap offset on a 500 u source moves the angle ≈ 5°).
     *
     * With projection we normalise the listener→gap direction and rescale
     * it to the source distance.  At pBlend=0.40 a doorway 45° to the right
     * of the direct line now gives a clearly audible ~16° HRTF direction
     * shift — the sound appears to come from around the corner. */
    if (bestFrac > directFrac) {
        vec3_t gapVec;
        float  gapLen;
        VectorSubtract(acousticPos, s_al_listener_origin, gapVec);
        gapLen = VectorLength(gapVec);
        if (gapLen > 1.0f) {
            vec3_t srcVec;
            float  srcLen;
            VectorSubtract(srcOrigin, s_al_listener_origin, srcVec);
            srcLen = VectorLength(srcVec);
            if (srcLen > 1.0f) {
                float rcpGap = 1.0f / gapLen;
                acousticPos[0] = s_al_listener_origin[0]
                                 + gapVec[0] * rcpGap * srcLen;
                acousticPos[1] = s_al_listener_origin[1]
                                 + gapVec[1] * rcpGap * srcLen;
                acousticPos[2] = s_al_listener_origin[2]
                                 + gapVec[2] * rcpGap * srcLen;
            }
        }
    }

    if (bestFrac > 1.0f) bestFrac = 1.0f;
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
    src->sampleVol       = 1.0f;
    src->isBspSpeaker    = qfalse;  /* set by UpdateLoops for non-global BSP loops */

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
    else if (isLocal && entchannel == CHAN_WEAPON)
        src->category = S_AL_IsSoundSuppressed(sfx, entnum)
                        ? SRC_CAT_WEAPON_SUPPRESSED : SRC_CAT_WEAPON;
    else if (isLocal)
        src->category = SRC_CAT_LOCAL;
    else if (isAmbient)
        src->category = SRC_CAT_AMBIENT;
    else if (s_al_listener_entnum >= 0 && entnum == s_al_listener_entnum) {
        /* Own entity in 3D mode — split weapon fire from movement */
        if (entchannel == CHAN_WEAPON)
            src->category = S_AL_IsSoundSuppressed(sfx, entnum)
                            ? SRC_CAT_WEAPON_SUPPRESSED : SRC_CAT_WEAPON;
        else
            src->category = SRC_CAT_LOCAL;
    } else if (entnum == ENTITYNUM_WORLD)
        src->category = SRC_CAT_IMPACT;  /* bullet impacts, brass, debris */
    else {
        /* Other player/entity: check for suppressed weapon fire and route
         * to SRC_CAT_WORLD_SUPPRESSED so the quieter ref gain applies.
         * Only CHAN_WEAPON is suppressed; movement/other sounds stay WORLD. */
        src->category = (entchannel == CHAN_WEAPON
                         && S_AL_IsSoundSuppressed(sfx, entnum))
                        ? SRC_CAT_WORLD_SUPPRESSED : SRC_CAT_WORLD;
    }

    /* Extra-vol override: if the sound file name matches one of the
     * s_alExtraVolEntryN slots, route it to SRC_CAT_EXTRAVOL regardless of
     * entity/channel classification.  Spatial properties (local, rolloff,
     * origin) are unaffected — only the gain group changes so s_alExtraVol
     * controls the loudness independently.  A ":-N" dB suffix in the matching
     * slot is converted to a linear scale stored in src->sampleVol and
     * multiplied into the gain on top of the global s_alExtraVol scalar. */
    {
        float sampleScale = 1.0f;
        if (S_AL_IsExtraVolSound(sfx, &sampleScale)) {
            src->category  = SRC_CAT_EXTRAVOL;
            src->sampleVol = sampleScale;
        }
    }

    catVol = S_AL_GetCategoryVol(src->category);
    /* Ambient sources additionally apply per-sample RMS normalisation so
     * that different map environments have consistent perceived loudness
     * without the player having to adjust s_alVolEnv every map. */
    if (src->category == SRC_CAT_AMBIENT)
        catVol *= r->normGain;

    /* Range-based proximity boost for enemy suppressed weapons.
     * At point-blank range the volume scales up toward normal enemy weapon
     * level (ref 0.70); beyond s_alSuppressedEnemyRangeMax it stays at the
     * suppressed floor (ref 0.45). */
    if (src->category == SRC_CAT_WORLD_SUPPRESSED && origin) {
        float rangeMax  = s_alSuppressedEnemyRangeMax
                          ? s_alSuppressedEnemyRangeMax->value : 400.f;
        if (rangeMax > 0.f) {
            float dx   = origin[0] - s_al_listener_origin[0];
            float dy   = origin[1] - s_al_listener_origin[1];
            float dz   = origin[2] - s_al_listener_origin[2];
            float dist = sqrtf(dx*dx + dy*dy + dz*dz);
            if (dist < rangeMax) {
                float proximity  = 1.0f - dist / rangeMax;
                /* Blend suppressed ref (0.45) up toward normal enemy ref (0.70). */
                float rangeBoost = 1.0f + (0.70f / 0.45f - 1.0f) * proximity;
                catVol *= rangeBoost;
            }
        }
    }

    if (origin)
        VectorCopy(origin, src->origin);
    else
        VectorClear(src->origin);
    src->fixed_origin = fixed_origin;

    qalSourcei(sid,  AL_BUFFER,   r->buffer);
    qalSourcef(sid,  AL_GAIN,     (vol / 255.0f) * catVol * src->sampleVol);
    qalSourcei(sid,  AL_LOOPING,  AL_FALSE);
    if (s_al_bestResampler >= 0)
        qalSourcei(sid, AL_SOURCE_RESAMPLER_SOFT, s_al_bestResampler);

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
            if (s_al_efx.maxSends >= 2)
                qalSource3i(sid, AL_AUXILIARY_SEND_FILTER,
                            (ALint)AL_EFFECTSLOT_NULL, 1, (ALint)AL_FILTER_NULL);
            /* EQ send — local sources (own weapons/footsteps) get the same
             * bass/mid boost as world sources for consistent tonal balance.
             * Use eqNormFilter on the send to compensate for the additive
             * wet path (prevents dry+wet from boosting above source level). */
            if (s_al_efx.eqSend >= 0 && s_al_efx.eqSlot) {
                qalSource3i(sid, AL_AUXILIARY_SEND_FILTER,
                            (ALint)s_al_efx.eqSlot, s_al_efx.eqSend,
                            (ALint)(s_al_efx.eqNormFilter
                                    ? s_al_efx.eqNormFilter
                                    : AL_FILTER_NULL));
            }
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
            /* Echo / chorus send (auxiliary send 1).
             * Underwater: route to chorus slot (pitch wobble).
             * Normal: route to echo slot when s_alEcho is on.
             * Always null the send when neither applies so a stale
             * send from a previous use of this slot is cleared. */
            if (s_al_efx.maxSends >= 2) {
                ALuint send1 = (ALuint)AL_EFFECTSLOT_NULL;
                if (s_al_inwater && s_al_efx.chorusSlot)
                    send1 = s_al_efx.chorusSlot;
                else if (s_alEcho && s_alEcho->integer && s_al_efx.echoSlot)
                    send1 = s_al_efx.echoSlot;
                qalSource3i(sid, AL_AUXILIARY_SEND_FILTER,
                            (ALint)send1, 1, (ALint)AL_FILTER_NULL);
            }
            /* EQ send — always-on tone shaping for bass/mid presence.
             * Use eqNormFilter on the send to normalize the wet path so
             * dry+wet stays at the original source level without distortion. */
            if (s_al_efx.eqSend >= 0 && s_al_efx.eqSlot) {
                qalSource3i(sid, AL_AUXILIARY_SEND_FILTER,
                            (ALint)s_al_efx.eqSlot, s_al_efx.eqSend,
                            (ALint)(s_al_efx.eqNormFilter
                                    ? s_al_efx.eqNormFilter
                                    : AL_FILTER_NULL));
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
    // Clear EFX routing so the next sound on this slot doesn't inherit
    // stale reverb/filter state (e.g. tinnitus routed through reverb).
    if (s_al_efx.available) {
        qalSource3i(s_al_src[idx].source, AL_AUXILIARY_SEND_FILTER, AL_EFFECTSLOT_NULL, 0, AL_FILTER_NULL);
        qalSource3i(s_al_src[idx].source, AL_AUXILIARY_SEND_FILTER, AL_EFFECTSLOT_NULL, 1, AL_FILTER_NULL);
        if (s_al_efx.eqSend >= 0)
            qalSource3i(s_al_src[idx].source, AL_AUXILIARY_SEND_FILTER, AL_EFFECTSLOT_NULL, s_al_efx.eqSend, AL_FILTER_NULL);
        qalSourcei(s_al_src[idx].source, AL_DIRECT_FILTER, AL_FILTER_NULL);
    }
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
    EFX_PROC(LPALGETAUXILIARYEFFECTSLOTF,    alGetAuxiliaryEffectSlotf);
#undef EFX_PROC

    if (!qalGenEffects || !qalGenFilters || !qalGenAuxiliaryEffectSlots)
        return;

    qalGetError();  /* clear */

    /* Reverb effect — prefer EAX reverb, fall back to standard reverb */
    qalGenEffects(1, &s_al_efx.reverbEffect);
    qalEffecti(s_al_efx.reverbEffect, AL_EFFECT_TYPE, AL_EFFECT_EAXREVERB);
    if (qalGetError() == AL_NO_ERROR) {
        s_al_efx.hasEAXReverb = qtrue;
        /* Medium-sized indoor room — sensible default for URT maps. */
        qalEffectf(s_al_efx.reverbEffect, AL_EAXREVERB_DENSITY,           0.5f);
        qalEffectf(s_al_efx.reverbEffect, AL_EAXREVERB_DIFFUSION,         0.85f);
        qalEffectf(s_al_efx.reverbEffect, AL_EAXREVERB_GAIN,              0.32f);
        qalEffectf(s_al_efx.reverbEffect, AL_EAXREVERB_GAINHF,            0.89f);
        qalEffectf(s_al_efx.reverbEffect, AL_EAXREVERB_GAINLF,            1.0f);
        qalEffectf(s_al_efx.reverbEffect, AL_EAXREVERB_DECAY_TIME,
                   s_alReverbDecay ? s_alReverbDecay->value : 1.49f);
        qalEffectf(s_al_efx.reverbEffect, AL_EAXREVERB_DECAY_HFRATIO,     0.83f);
        qalEffectf(s_al_efx.reverbEffect, AL_EAXREVERB_REFLECTIONS_GAIN,  0.25f);
        qalEffectf(s_al_efx.reverbEffect, AL_EAXREVERB_REFLECTIONS_DELAY, 0.007f);
        qalEffectf(s_al_efx.reverbEffect, AL_EAXREVERB_LATE_REVERB_GAIN,  0.50f);
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
        qalEffectf(s_al_efx.reverbEffect, AL_REVERB_GAIN,       0.32f);
        qalEffectf(s_al_efx.reverbEffect, AL_REVERB_GAINHF,     0.89f);
        qalEffectf(s_al_efx.reverbEffect, AL_REVERB_DECAY_TIME,
                   s_alReverbDecay ? s_alReverbDecay->value : 1.49f);
        qalEffectf(s_al_efx.reverbEffect, AL_REVERB_REFLECTIONS_GAIN,  0.25f);
        qalEffectf(s_al_efx.reverbEffect, AL_REVERB_REFLECTIONS_DELAY, 0.007f);
        qalEffectf(s_al_efx.reverbEffect, AL_REVERB_LATE_REVERB_GAIN,  0.50f);
        qalEffectf(s_al_efx.reverbEffect, AL_REVERB_LATE_REVERB_DELAY, 0.011f);
    }
    qalGenAuxiliaryEffectSlots(1, &s_al_efx.reverbSlot);
    qalAuxiliaryEffectSloti(s_al_efx.reverbSlot, AL_EFFECTSLOT_EFFECT,
                            (ALint)s_al_efx.reverbEffect);
    /* Apply initial reverb slot gain from cvar */
    {
        float slotGain = s_alReverbGain ? s_alReverbGain->value : 0.35f;
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
    s_al_efx.maxSends = maxSends;

    /* Echo effect (aux send 1): geometry-driven discrete repeats.
     * Only created when at least 2 aux sends are available. */
    if (maxSends >= 2) {
        qalGetError();
        qalGenEffects(1, &s_al_efx.echoEffect);
        qalEffecti(s_al_efx.echoEffect, AL_EFFECT_TYPE, AL_EFFECT_ECHO);
        if (qalGetError() == AL_NO_ERROR) {
            /* Conservative defaults — will be driven dynamically by
             * S_AL_UpdateDynamicReverb based on room geometry.
             * Short delay + low feedback → a single subtle reflection,
             * not a long repeating trail. */
            qalEffectf(s_al_efx.echoEffect, AL_ECHO_DELAY,    0.04f);
            qalEffectf(s_al_efx.echoEffect, AL_ECHO_LRDELAY,  0.02f);
            qalEffectf(s_al_efx.echoEffect, AL_ECHO_DAMPING,  0.70f);
            qalEffectf(s_al_efx.echoEffect, AL_ECHO_FEEDBACK, 0.08f);
            qalEffectf(s_al_efx.echoEffect, AL_ECHO_SPREAD,   0.50f);
            qalGenAuxiliaryEffectSlots(1, &s_al_efx.echoSlot);
            qalAuxiliaryEffectSloti(s_al_efx.echoSlot, AL_EFFECTSLOT_EFFECT,
                                    (ALint)s_al_efx.echoEffect);
            qalAuxiliaryEffectSlotf(s_al_efx.echoSlot, AL_EFFECTSLOT_GAIN, 0.0f);
        } else {
            /* AL_EFFECT_ECHO not supported — clear so wiring code skips it. */
            qalDeleteEffects(1, &s_al_efx.echoEffect);
            s_al_efx.echoEffect = 0;
        }

        /* Chorus effect (aux send 1 while underwater): pitch-modulation wobble. */
        qalGetError();
        qalGenEffects(1, &s_al_efx.chorusEffect);
        qalEffecti(s_al_efx.chorusEffect, AL_EFFECT_TYPE, AL_EFFECT_CHORUS);
        if (qalGetError() == AL_NO_ERROR) {
            qalEffecti(s_al_efx.chorusEffect, AL_CHORUS_WAVEFORM, AL_CHORUS_WAVEFORM_SINUSOID);
            qalEffecti(s_al_efx.chorusEffect, AL_CHORUS_PHASE,    90);
            qalEffectf(s_al_efx.chorusEffect, AL_CHORUS_RATE,     0.5f);
            qalEffectf(s_al_efx.chorusEffect, AL_CHORUS_DEPTH,    0.12f);
            qalEffectf(s_al_efx.chorusEffect, AL_CHORUS_FEEDBACK, 0.0f);
            qalEffectf(s_al_efx.chorusEffect, AL_CHORUS_DELAY,    0.016f);
            qalGenAuxiliaryEffectSlots(1, &s_al_efx.chorusSlot);
            qalAuxiliaryEffectSloti(s_al_efx.chorusSlot, AL_EFFECTSLOT_EFFECT,
                                    (ALint)s_al_efx.chorusEffect);
            qalAuxiliaryEffectSlotf(s_al_efx.chorusSlot, AL_EFFECTSLOT_GAIN, 0.0f);
        } else {
            qalDeleteEffects(1, &s_al_efx.chorusEffect);
            s_al_efx.chorusEffect = 0;
        }
    }

    /* Global EQ effect — tone shaping to compensate for occlusion softening.
     * Uses its own aux send (2 if available, else shares send 1 with echo).
     * The EQ auxiliary send is inherently additive (wet adds to dry), which
     * would boost the signal and cause distortion.  A normalization filter
     * (eqNormFilter at gain 1/(1+max_band*slot_gain)) is applied to both the
     * direct path and the EQ send so the combined dry+wet output stays at the
     * original source level.  Set s_alEqGain 0 to disable without re-init. */
    s_al_efx.eqSend = -1;
    if (s_alEqGain && s_alEqGain->value > 0.0f) {
        int eqSendIdx = (maxSends >= 3) ? 2 : ((maxSends >= 2) ? 1 : -1);
        if (eqSendIdx >= 0) {
            qalGetError();
            qalGenEffects(1, &s_al_efx.eqEffect);
            qalEffecti(s_al_efx.eqEffect, AL_EFFECT_TYPE, AL_EFFECT_EQUALIZER);
            if (qalGetError() == AL_NO_ERROR) {
                float low  = s_alEqLow  ? s_alEqLow->value  : 1.26f;
                float mid  = s_alEqMid  ? s_alEqMid->value  : 1.26f;
                float high = s_alEqHigh ? s_alEqHigh->value : 1.0f;
                qalEffectf(s_al_efx.eqEffect, AL_EQUALIZER_LOW_GAIN,    low);
                qalEffectf(s_al_efx.eqEffect, AL_EQUALIZER_LOW_CUTOFF,  200.0f);
                qalEffectf(s_al_efx.eqEffect, AL_EQUALIZER_MID1_GAIN,   mid);
                qalEffectf(s_al_efx.eqEffect, AL_EQUALIZER_MID1_CENTER, 800.0f);
                qalEffectf(s_al_efx.eqEffect, AL_EQUALIZER_MID1_WIDTH,  1.0f);
                qalEffectf(s_al_efx.eqEffect, AL_EQUALIZER_MID2_GAIN,   mid);
                qalEffectf(s_al_efx.eqEffect, AL_EQUALIZER_MID2_CENTER, 2500.0f);
                qalEffectf(s_al_efx.eqEffect, AL_EQUALIZER_MID2_WIDTH,  1.0f);
                qalEffectf(s_al_efx.eqEffect, AL_EQUALIZER_HIGH_GAIN,   high);
                qalEffectf(s_al_efx.eqEffect, AL_EQUALIZER_HIGH_CUTOFF, 6000.0f);
                qalGenAuxiliaryEffectSlots(1, &s_al_efx.eqSlot);
                qalAuxiliaryEffectSloti(s_al_efx.eqSlot, AL_EFFECTSLOT_EFFECT,
                                        (ALint)s_al_efx.eqEffect);
                qalAuxiliaryEffectSlotf(s_al_efx.eqSlot, AL_EFFECTSLOT_GAIN,
                                        s_alEqGain->value);
                s_al_efx.eqSend = eqSendIdx;
                /* Compute normalization gain so dry + EQ-wet stays at input level.
                 * When the same gain is applied to both the direct path and the EQ
                 * auxiliary send, the total at the peak-boosted band equals the
                 * original source signal: comp*(1) + comp*band*slot = source.
                 * Without this, the additive wet path boosts the signal by up to
                 * (1 + max_band * slot_gain)x, which causes clipping / distortion. */
                {
                    float maxBand = MAX(MAX(low, mid), high);
                    s_al_efx.eqCompGain = 1.0f / (1.0f + maxBand * s_alEqGain->value);
                    qalGenFilters(1, &s_al_efx.eqNormFilter);
                    qalFilteri(s_al_efx.eqNormFilter, AL_FILTER_TYPE, AL_FILTER_LOWPASS);
                    qalFilterf(s_al_efx.eqNormFilter, AL_LOWPASS_GAIN,   s_al_efx.eqCompGain);
                    qalFilterf(s_al_efx.eqNormFilter, AL_LOWPASS_GAINHF, 1.0f);
                }
            } else {
                qalDeleteEffects(1, &s_al_efx.eqEffect);
                s_al_efx.eqEffect = 0;
            }
        }
    }

    s_al_efx.available = qtrue;
    Com_Printf("S_AL: EFX available (%s reverb, %d aux sends%s%s%s)\n",
        s_al_efx.hasEAXReverb ? "EAX" : "standard", maxSends,
        s_al_efx.echoSlot   ? ", echo"   : "",
        s_al_efx.chorusSlot ? ", chorus" : "",
        s_al_efx.eqSlot     ? ", EQ"     : "");
}

static void S_AL_Shutdown( void )
{
    int i;

    if (!s_al_started) return;
    s_al_started = qfalse;

    Cmd_RemoveCommand("s_devices");
    Cmd_RemoveCommand("s_alReset");
    Cmd_RemoveCommand("snd_normcache_rebuild");

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
        if (s_al_efx.echoSlot)
            qalDeleteAuxiliaryEffectSlots(1, &s_al_efx.echoSlot);
        if (s_al_efx.echoEffect)
            qalDeleteEffects(1, &s_al_efx.echoEffect);
        if (s_al_efx.chorusSlot)
            qalDeleteAuxiliaryEffectSlots(1, &s_al_efx.chorusSlot);
        if (s_al_efx.chorusEffect)
            qalDeleteEffects(1, &s_al_efx.chorusEffect);
        if (s_al_efx.eqSlot)
            qalDeleteAuxiliaryEffectSlots(1, &s_al_efx.eqSlot);
        if (s_al_efx.eqEffect)
            qalDeleteEffects(1, &s_al_efx.eqEffect);
        if (s_al_efx.eqNormFilter)
            qalDeleteFilters(1, &s_al_efx.eqNormFilter);
        for (j = 0; j < S_AL_MAX_SRC; j++)
            if (s_al_efx.occlusionFilter[j])
                qalDeleteFilters(1, &s_al_efx.occlusionFilter[j]);
        Com_Memset(&s_al_efx, 0, sizeof(s_al_efx));
    }

    /* Tinnitus PCM buffer (independent of EFX) */
    if (s_al_tinnitusBuf) {
        qalDeleteBuffers(1, &s_al_tinnitusBuf);
        s_al_tinnitusBuf         = 0;
        s_al_tinnitusLastBuiltHz = 0;
        s_al_tinnitusLastBuiltMs = 0;
    }

    /* Sfx — drain the async loader pool first so no workers are accessing
     * sfx records when we wipe them. */
    S_AL_StopThreadPool();
    S_AL_FreeSfx();

    /* Context / device */
    qalcMakeContextCurrent(NULL);
    if (s_al_context) { qalcDestroyContext(s_al_context); s_al_context = NULL; }
    if (s_al_device)  { qalcCloseDevice(s_al_device);     s_al_device  = NULL; }

    S_AL_UnloadLibrary();
    Com_Memset(&s_al_music, 0, sizeof(s_al_music));
    Com_Memset(&s_al_raw,   0, sizeof(s_al_raw));
}

/* =========================================================================
 * Hearing-disruption helpers
 * =========================================================================
 */

/* Arm (or extend/deepen) the suppression event.
 * fireDir: direction TOWARD the incoming fire source (need not be normalised —
 *   the function normalises it internally), or NULL for omnidirectional (head
 *   hits — attacker position unknown).  Directional events gate per-source HF
 *   suppression to a cone; omnidirectional events apply the full cut globally.
 * Takes the later expiry and the lower (more muffling) HF floor so that
 * overlapping triggers always favour the worse outcome. */
static void S_AL_TriggerSuppression( int durationMs, float hfFloor,
                                     const float *fireDir )
{
    int   expiry;
    float reverbBoost;
    if (durationMs < 50)   durationMs = 50;
    if (hfFloor   < 0.0f)  hfFloor    = 0.0f;
    if (hfFloor   > 1.0f)  hfFloor    = 1.0f;
    expiry = Com_Milliseconds() + durationMs;
    if (expiry >= s_al_suppressExpiry) {
        s_al_suppressExpiry = expiry;
        s_al_suppressHFPeak = hfFloor;
    } else if (hfFloor < s_al_suppressHFPeak) {
        /* Shorter but deeper — keep existing expiry, deepen the cut */
        s_al_suppressHFPeak = hfFloor;
    }

    /* Update fire direction: a new directional event overrides the stored
     * direction; an omnidirectional event clears directionality unless a
     * stronger directional event is already active. */
    if (fireDir) {
        float len = sqrtf(fireDir[0]*fireDir[0] + fireDir[1]*fireDir[1]
                          + fireDir[2]*fireDir[2]);
        if (len > 0.001f) {
            s_al_suppress_dir[0] = fireDir[0] / len;
            s_al_suppress_dir[1] = fireDir[1] / len;
            s_al_suppress_dir[2] = fireDir[2] / len;
        }
        s_al_suppress_directional = qtrue;
    } else {
        s_al_suppress_directional = qfalse;
    }

    /* Reverb spike: makes the room acoustics "bloom" on impact — the
     * wet reverb tail briefly dominates, then decays back, selling the
     * acoustic weight of nearby incoming fire. */
    reverbBoost = s_alSuppressionReverbBoost
                  ? s_alSuppressionReverbBoost->value : 0.18f;
    if (reverbBoost > 0.f && s_al_efx.reverbSlot
            && s_alReverb && s_alReverb->integer) {
        float curGain = s_alReverbGain ? s_alReverbGain->value : 0.35f;
        float peak    = curGain + reverbBoost;
        if (peak > 1.0f) peak = 1.0f;
        if (s_al_suppressReverbExpiry == 0 || peak > s_al_suppressReverbPeak)
            s_al_suppressReverbPeak = peak;
        s_al_suppressReverbExpiry = expiry;
    }
}

/* Build (or rebuild) the procedural tinnitus PCM buffer.
 * Generates a pure sine tone at s_alTinnitusFreq Hz with a short
 * linear attack and a quadratic decay envelope — no external asset needed.
 * Called lazily before the first play and whenever freq/duration change. */
static void S_AL_BuildTinnitusBuf( void )
{
    int    freqHz, durationMs, nSamples, i;
    short *pcm;

    if (!qalGenBuffers || !qalBufferData) return;

    freqHz     = s_alTinnitusFreq     ? (int)s_alTinnitusFreq->value     : 3500;
    durationMs = s_alTinnitusDuration ? (int)s_alTinnitusDuration->value : 700;
    if (freqHz     <  200) freqHz     =  200;
    if (freqHz     > 8000) freqHz     = 8000;
    if (durationMs <   50) durationMs =   50;
    if (durationMs > 3000) durationMs = 3000;

    /* Nothing changed — skip rebuild */
    if (s_al_tinnitusBuf
            && freqHz     == s_al_tinnitusLastBuiltHz
            && durationMs == s_al_tinnitusLastBuiltMs)
        return;

    /* Delete stale buffer */
    if (s_al_tinnitusBuf) {
        qalDeleteBuffers(1, &s_al_tinnitusBuf);
        s_al_tinnitusBuf = 0;
    }

    /* 22050 Hz mono — plenty of bandwidth for up to 8 kHz */
#define TIN_RATE 22050
    nSamples = (TIN_RATE * durationMs) / 1000;
    if (nSamples < 1) return;

    pcm = (short *)Z_Malloc(nSamples * (int)sizeof(short));

    {
        /* Attack: first 20 ms linear ramp up */
        int attackSmp  = (TIN_RATE * 20) / 1000;
        /* Decay starts right after attack and runs to end */
        float decayLen = (float)(nSamples - attackSmp);

        for (i = 0; i < nSamples; i++) {
            float t      = (float)i / TIN_RATE;
            float sample = sinf(2.0f * M_PI * (float)freqHz * t);
            float env;

            if (i < attackSmp) {
                env = (float)i / (float)attackSmp;
            } else {
                float d = (float)(i - attackSmp) / decayLen;
                /* Quadratic decay: 1 → 0, sounds natural */
                float inv = 1.0f - d;
                env = inv * inv;
            }
            pcm[i] = (short)(sample * env * 26000.0f);
        }
    }
#undef TIN_RATE

    qalGenBuffers(1, &s_al_tinnitusBuf);
    qalBufferData(s_al_tinnitusBuf, AL_FORMAT_MONO16,
                  pcm, nSamples * (ALsizei)sizeof(short), 22050);
    Z_Free(pcm);

    s_al_tinnitusLastBuiltHz = freqHz;
    s_al_tinnitusLastBuiltMs = durationMs;
}

/* Play the synthesised tinnitus tone as a non-positional local source.
 * Rebuilds the PCM buffer if the frequency or duration CVars changed.
 * The cooldown guard prevents rapid-fire repetition. */
static void S_AL_TriggerTinnitus( void )
{
    int    srcIdx, now, cooldown;
    float  tinVol;
    ALuint sid;

    if (!s_al_started) return;

    now      = Com_Milliseconds();
    cooldown = s_alTinnitusCooldown ? (int)s_alTinnitusCooldown->value : 800;
    if (cooldown < 0) cooldown = 0;
    if (now - s_al_tinnitusLastPlay < cooldown) return;

    S_AL_BuildTinnitusBuf();
    if (!s_al_tinnitusBuf) return;

    srcIdx = S_AL_GetFreeSource();
    if (srcIdx < 0) return;

    sid    = s_al_src[srcIdx].source;
    tinVol = s_alTinnitusVol ? s_alTinnitusVol->value : 0.45f;
    if (tinVol < 0.0f) tinVol = 0.0f;
    if (tinVol > 1.0f) tinVol = 1.0f;

    /* Wire up the source manually — no sfx record, no category vol,
     * just the raw buffer at the user-configured volume scaled by the
     * global listener gain (s_volume is applied via AL_GAIN on the
     * listener every frame, so we set source gain = tinVol directly). */
    s_al_src[srcIdx].sfx         = -1;
    s_al_src[srcIdx].entnum      =  0;
    s_al_src[srcIdx].entchannel  = CHAN_LOCAL_SOUND;
    s_al_src[srcIdx].master_vol  = S_AL_MASTER_VOL_FULL;
    s_al_src[srcIdx].isLocal     = qtrue;
    s_al_src[srcIdx].loopSound   = qfalse;
    s_al_src[srcIdx].allocTime   = now;
    s_al_src[srcIdx].isPlaying   = qtrue;
    s_al_src[srcIdx].category    = SRC_CAT_UI; /* excluded from HF filter */
    s_al_src[srcIdx].occlusionGain   = 1.0f;
    s_al_src[srcIdx].occlusionTarget = 1.0f;
    s_al_src[srcIdx].occlusionTick   = 0;
    s_al_src[srcIdx].sampleVol       = 1.0f;
    VectorClear(s_al_src[srcIdx].origin);
    s_al_src[srcIdx].fixed_origin    = qfalse;

    qalSourcei(sid, AL_BUFFER,          (ALint)s_al_tinnitusBuf);
    qalSourcef(sid, AL_GAIN,            tinVol);
    qalSourcei(sid, AL_LOOPING,         AL_FALSE);
    qalSourcei(sid, AL_SOURCE_RELATIVE, AL_TRUE);
    qalSource3f(sid, AL_POSITION,       0.0f, 0.0f, 0.0f);
    qalSourcef(sid, AL_ROLLOFF_FACTOR,  0.0f);
    if (s_al_efx.available)
        qalSourcei(sid, AL_DIRECT_FILTER, (ALint)AL_FILTER_NULL);
    qalSourcePlay(sid);

    s_al_tinnitusLastPlay = now;
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
        /* When the caller supplies an explicit world-space origin for the
         * listener entity, the cgame may have evaluated it from the network
         * trajectory (TR_LINEAR / TR_INTERPOLATE), which can lag behind the
         * engine's client-side prediction — especially when sv_smoothClients
         * delays the snapshot trBase by sv_bufferMs ms.  At 300 u/s and
         * 50 ms buffer delay that is a ~15-unit positional error that scales
         * with velocity, causing own-player sounds (footsteps, weapon fire,
         * breath) to appear offset behind the listener.
         *
         * S_AL_Respatialize is always called with the client's predicted eye
         * position (from Pmove) and stores it in s_al_listener_origin.  That
         * is the authoritative ground truth for the local player regardless of
         * server configuration (vanilla, sv_smoothClients, sv_bufferMs) or
         * active QVM patch level.  Substitute it here so own-player sounds are
         * always placed at the correct position.
         *
         * Scope: strictly limited to entnum == s_al_listener_entnum.
         * All other entities — other players, world, bots — use the origin
         * supplied by the cgame, which is correct for them. */
        if (entnum == s_al_listener_entnum && s_al_listener_entnum >= 0)
            VectorCopy(s_al_listener_origin, sndOrigin);
        else
            VectorCopy(origin, sndOrigin);
        isLocal = qfalse;
    } else {
        /* origin == NULL: use the entity-origin cache.  For the listener
         * entity additionally prefer s_al_listener_origin over the cache
         * so that even if S_UpdateEntityPosition hasn't run yet this frame
         * (early-in-frame sound) we still use the Pmove-predicted position. */
        if (entnum == s_al_listener_entnum && s_al_listener_entnum >= 0)
            VectorCopy(s_al_listener_origin, sndOrigin);
        else if (entnum >= 0 && entnum < MAX_GENTITIES)
            VectorCopy(s_al_entity_origins[entnum], sndOrigin);
        else
            VectorClear(sndOrigin);

        /* BSP cold-start fallback: if the entity origin cache is still at
         * world-origin (0,0,0) — which happens on the very first frame before
         * S_UpdateEntityPosition has been called for static map entities —
         * look for a target_speaker in the parsed BSP list whose noise file
         * matches this sound.  Only resolve when exactly ONE speaker uses this
         * file; if multiple speakers share the same noise we cannot know which
         * entity number maps to which BSP entity, so we leave the origin alone
         * and let it update naturally on the next frame. */
        if (VectorCompare(sndOrigin, vec3_origin)
                && s_al_numBspSpeakers > 0
                && sfx >= 0 && sfx < s_al_numSfx) {
            const char *sfxName = s_al_sfx[sfx].name;
            int  matchIdx   = -1;
            int  matchCount = 0;
            int  bk;
            for (bk = 0; bk < s_al_numBspSpeakers; bk++) {
                /* Match on the base filename portion so that path differences
                 * between the BSP noise value and the registered sfx path
                 * (with or without leading "sound/") still resolve. */
                if (Q_stristr(sfxName, s_al_bspSpeakers[bk].noise) ||
                        Q_stristr(s_al_bspSpeakers[bk].noise, sfxName)) {
                    matchCount++;
                    matchIdx = bk;
                }
            }
            if (matchCount == 1)
                VectorCopy(s_al_bspSpeakers[matchIdx].origin, sndOrigin);
        }
        isLocal = qfalse;
    }

    if (!isLocal) {
        float maxDist = s_alMaxDist ? s_alMaxDist->value : S_AL_SOUND_MAXDIST;
        vec3_t d;
        VectorSubtract(sndOrigin, s_al_listener_origin, d);
        if (DotProduct(d, d) > maxDist * maxDist)
            return;   /* beyond audible range — no slot needed */
    }

    /* One-shot dedup — see comment block above S_AL_StartSound definition. */
    if (s_alDedupMs && s_alDedupMs->integer > 0) {
        int dedupMs = s_alDedupMs->integer;
        int nowMs   = Com_Milliseconds();
        int i;

        /* ENTITYNUM_WORLD hard floor: match DMA's 100 ms world-entity throttle. */
        if (entnum == ENTITYNUM_WORLD && dedupMs < 100)
            dedupMs = 100;

        for (i = 0; i < s_al_numSrc; i++) {
            alSrc_t *src = &s_al_src[i];
            int      elapsed, window;

            if (!src->isPlaying || src->loopSound) continue;
            if (src->entnum != entnum || src->sfx != sfx) continue;

            elapsed = nowMs - src->allocTime;
            window  = dedupMs;

            /* Non-weapon channels: extend the window to the full sound
             * duration so trigger-bound sounds cannot re-stack while the
             * previous instance is still playing — regardless of how
             * s_alDedupMs is configured.  Weapon channels keep the minimum
             * window so rapid fire is never suppressed.
             * ENTITYNUM_WORLD is excluded: it has its own hard 100 ms floor
             * (set above) and should never suppress rapid world impacts from
             * different locations for the full sample duration. */
            if (entchannel != CHAN_WEAPON && entnum != ENTITYNUM_WORLD &&
                    sfx >= 0 && sfx < s_al_numSfx) {
                alSfxRec_t *sr = &s_al_sfx[sfx];
                if (sr->fileRate > 0 && sr->soundLength > 0) {
                    /* Cast to int64 before multiplying to avoid signed 32-bit
                     * overflow: at 44100 Hz any sound > ~49 s causes
                     * soundLength * 1000 to wrap negative, silently disabling
                     * the duration guard for the longest ambient tracks. */
                    int durMs = (int)(((int64_t)sr->soundLength * 1000)
                                      / sr->fileRate);
                    if (durMs < 0 || durMs > S_AL_DEDUP_MAX_DUR_MS)
                        durMs = S_AL_DEDUP_MAX_DUR_MS;
                    if (durMs > window) window = durMs;
                }
            }

            if (elapsed < window) {
                S_AL_TRACE(2, "StartSound: dedup blocked sfx=%d ent=%d ch=%d "
                              "(%d ms elapsed < %d ms window)\n",
                           sfx, entnum, entchannel, elapsed, window);
                return;
            }
        }
    }

    srcIdx = S_AL_GetFreeSource();
    if (srcIdx < 0) return;

    /* Feature D — Audio suppression: near-miss tracer detection.
     * When an ENEMY fires a CHAN_WEAPON sound very close to the listener,
     * briefly duck the listener gain to simulate the concussive disruption of
     * incoming fire ("suppression effect").
     *
     * Own sounds and TEAMMATE sounds are both excluded:
     *   • Own sounds: entnum == s_al_listener_entnum (already caught by !isLocal
     *     but guarded explicitly for clarity).
     *   • Teammates: determined by comparing the shooter's player configstring
     *     team field ("t") against the local player's persistant[PERS_TEAM].
     *     Both must be on a real team (TEAM_RED/TEAM_BLUE, not TEAM_FREE / FFA).
     *     Configstrings are in cl.gameState which is always up-to-date — no
     *     cgame access required. */
    if (s_alSuppression && s_alSuppression->integer && !isLocal
            && entchannel == CHAN_WEAPON
            && entnum != s_al_listener_entnum) {
        /* --- Friendly-fire guard ------------------------------------------ */
        qboolean isFriendly = qfalse;
        {
            int ourTeam = (int)cl.snap.ps.persistant[PERS_TEAM];
            /* Only meaningful in a team game (RED or BLUE); skip in FFA. */
            if (ourTeam != TEAM_FREE && entnum >= 0 && entnum < MAX_CLIENTS) {
                const char *csStr = cl.gameState.stringData
                                    + cl.gameState.stringOffsets[CS_PLAYERS + entnum];
                int shooterTeam = atoi(Info_ValueForKey(csStr, "t"));
                if (shooterTeam == ourTeam)
                    isFriendly = qtrue;
            }
        }
        if (!isFriendly && !S_AL_IsSoundSuppressed(sfx, entnum)) {
            float radius = s_alSuppressionRadius ? s_alSuppressionRadius->value : 180.f;
            vec3_t d;
            VectorSubtract(sndOrigin, s_al_listener_origin, d);
            if (DotProduct(d, d) < radius * radius) {
                int   durationMs = s_alSuppressionMs    ? (int)s_alSuppressionMs->value    : 220;
                float hfFloor    = s_alSuppressionHFFloor ? s_alSuppressionHFFloor->value : 0.08f;
                /* Direction toward shooter (unnormalised — function normalises internally) */
                S_AL_TriggerSuppression(durationMs, hfFloor, d);
            }
        }
    }

    /* Feature D1 — Near-miss whiz trigger.
     * URT plays sound/weapons/whiz1.wav and whiz2.wav when a bullet passes
     * close to the listener.  These are the definitive "near miss" audio cues
     * and the most reliable suppression trigger — more precise than the
     * CHAN_WEAPON radius fallback above.  The sound may arrive as a local
     * (head-locked) source OR as a positional world source, so we check on
     * any sound whose name matches s_alNearMissPattern regardless of isLocal.
     * Own weapon fire is excluded implicitly: whiz sounds are never played
     * for the shooter's own projectile. */
    if (s_alSuppression && s_alSuppression->integer
            && sfx >= 0 && sfx < s_al_numSfx) {
        const char *nmPat = s_alNearMissPattern
                            ? s_alNearMissPattern->string : "whiz1,whiz2";
        const char *sndName = s_al_sfx[sfx].name;
        /* Walk the comma-separated pattern list */
        if (nmPat && nmPat[0]) {
            const char *p = nmPat;
            while (*p) {
                char tok[64];
                int  ti = 0;
                char *s, *e;
                while (*p && *p != ',' && ti < (int)sizeof(tok) - 1)
                    tok[ti++] = *p++;
                tok[ti] = '\0';
                if (*p == ',') p++;
                s = tok;
                while (*s == ' ') s++;
                e = s + strlen(s);
                while (e > s && *(e-1) == ' ') *--e = '\0';
                if (*s && Q_stristr(sndName, s)) {
                    int   durMs   = s_alSuppressionMs      ? (int)s_alSuppressionMs->value    : 220;
                    float hfFloor = s_alSuppressionHFFloor ? s_alSuppressionHFFloor->value    : 0.08f;
                    /* Use the whiz sound's origin for directionality when
                     * it is a positional (non-local) source. */
                    if (!isLocal) {
                        vec3_t whizDir;
                        VectorSubtract(sndOrigin, s_al_listener_origin, whizDir);
                        S_AL_TriggerSuppression(durMs, hfFloor, whizDir);
                    } else {
                        /* Local/head-locked whiz — direction unknown */
                        S_AL_TriggerSuppression(durMs, hfFloor, NULL);
                    }
                    break;
                }
            }
        }
    }

    /* Feature D2 — Head-hit triggers (helmet and bare head).
     * When the LOCAL player's entity plays a CHAN_BODY sound whose name
     * matches s_alHelmetHitPattern or s_alBareHeadHitPattern, fire a
     * hearing-disruption event with its configured HF floor and duration,
     * then play the synthesised tinnitus ring.
     *
     * Gated by s_alHeadHit (standalone toggle, default 1) OR s_alSuppression.
     * This allows head-hit feedback to be active without enabling the full
     * near-miss / incoming-fire suppression system. */
    if ((   (s_alHeadHit   && s_alHeadHit->integer)
         || (s_alSuppression && s_alSuppression->integer))
            && isLocal
            && entchannel == CHAN_BODY
            && sfx >= 0 && sfx < s_al_numSfx) {
        const char *sndName = s_al_sfx[sfx].name;

        /* --- Helmet hit -------------------------------------------------- */
        {
            const char *pat = s_alHelmetHitPattern
                              ? s_alHelmetHitPattern->string : "helmethit";
            if (pat && pat[0] && Q_stristr(sndName, pat)) {
                int   durMs   = s_alHelmetHitMs    ? (int)s_alHelmetHitMs->value    : 350;
                float hfFloor = s_alHelmetHFFloor  ? s_alHelmetHFFloor->value       : 0.10f;
                S_AL_TriggerSuppression(durMs, hfFloor, NULL); /* omnidirectional */
                S_AL_TriggerTinnitus();
            }
        }

        /* --- Bare-head hit (no helmet) ------------------------------------ */
        {
            const char *pat = s_alBareHeadHitPattern
                              ? s_alBareHeadHitPattern->string : "headshot";
            if (pat && pat[0] && Q_stristr(sndName, pat)) {
                int   durMs   = s_alBareHeadHitMs   ? (int)s_alBareHeadHitMs->value  : 500;
                float hfFloor = s_alBareHeadHFFloor ? s_alBareHeadHFFloor->value      : 0.03f;
                S_AL_TriggerSuppression(durMs, hfFloor, NULL); /* omnidirectional */
                S_AL_TriggerTinnitus();
            }
        }
    }

    /* Feature C — fire-direction impact reverb.
     * Record aim direction + frame number when the local player fires a weapon
     * so that S_AL_UpdateDynamicReverb can cast a short forward cone on its
     * next probe cycle and transiently boost early reflections. */
    if (s_alFireImpactReverb && s_alFireImpactReverb->integer
            && entnum == s_al_listener_entnum
            && entchannel == CHAN_WEAPON) {
        VectorCopy(s_al_listener_forward, s_al_fire_dir);
        s_al_fire_frame = s_al_loopFrame;
    }

    /* Incoming-enemy-fire reverb: when an enemy fires CHAN_WEAPON within range,
     * record the normalised distance so S_AL_UpdateDynamicReverb can apply a
     * proportional early-reflection boost — giving their muzzle blast acoustic
     * weight in the listener's local environment.
     * Suppressed weapons are excluded (they don't produce a significant blast). */
    if (s_alFireImpactReverb && s_alFireImpactReverb->integer
            && !isLocal
            && entchannel == CHAN_WEAPON
            && entnum != s_al_listener_entnum
            && !S_AL_IsSoundSuppressed(sfx, entnum)) {
        qboolean friendly = qfalse;
        {
            int ourTeam = (int)cl.snap.ps.persistant[PERS_TEAM];
            if (ourTeam != TEAM_FREE && entnum >= 0 && entnum < MAX_CLIENTS) {
                const char *csStr = cl.gameState.stringData
                    + cl.gameState.stringOffsets[CS_PLAYERS + entnum];
                if (atoi(Info_ValueForKey(csStr, "t")) == ourTeam)
                    friendly = qtrue;
            }
        }
        if (!friendly) {
            float  inRange = 700.f;
            vec3_t d;
            float  dist;
            VectorSubtract(sndOrigin, s_al_listener_origin, d);
            dist = sqrtf(DotProduct(d, d));
            if (dist < inRange) {
                s_al_incoming_fire_dist  = dist / inRange;
                s_al_incoming_fire_frame = s_al_loopFrame;
            }
        }
    }

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

    /* Grab one buffer from our small pool (round-robin).
     * If the target buffer is still queued on the source (caller is
     * pushing faster than playback), force-drain it so qalBufferData
     * doesn't silently fail with AL_INVALID_OPERATION. */
    {
        ALint queued = 0;
        qalGetSourcei(s_al_raw.source, AL_BUFFERS_QUEUED, &queued);
        if ( queued >= S_AL_RAW_BUFFERS ) {
            /* All buffers are attached — stop, drain everything, restart. */
            qalSourceStop(s_al_raw.source);
            queued = 0;
            qalGetSourcei(s_al_raw.source, AL_BUFFERS_PROCESSED, &queued);
            while (queued-- > 0) {
                ALuint tmp;
                qalSourceUnqueueBuffers(s_al_raw.source, 1, &tmp);
            }
        }
    }
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

    /* Reset per-map BSP state so the next map load re-parses and re-caches. */
    Com_Memset(&s_al_mapHints, 0, sizeof(s_al_mapHints));
    s_al_numBspSpeakers  = 0;
    s_al_normCacheCount  = 0;
    s_al_normCacheWritten = qfalse;
}


/* =========================================================================
 * Per-map BSP entity parsing and normalization cache
 * =========================================================================
 *
 * Order of operations at map load:
 *   1. S_AL_StopAllSounds()     — clears mapHints, bspSpeakers, normCache
 *   2. S_AL_BeginRegistration() — loads existing normcache if present so
 *                                  RegisterSound calls pick up overrides
 *   3. cgame registers sounds   — normcache override applied per-sound
 *   4. First UpdateDynamicReverb snap cycle:
 *        → S_AL_InitMapAudio()  — parses BSP, pre-registers remaining
 *                                  sounds, writes cache if none existed
 */

/* Extract the bare map name from cl.mapname.
 * "maps/ut4_turnpike.bsp" → "ut4_turnpike"                              */
static void S_AL_MapBaseName( char *out, int outLen )
{
    const char *p  = cl.mapname;
    const char *sl = strrchr(p, '/');
    const char *dot;
    if (sl) p = sl + 1;
    Q_strncpyz(out, p, outLen);
    dot = strrchr(out, '.');
    if (dot) *(char *)dot = '\0';
}

/* Parse the BSP entity lump (requires CM_NumInlineModels() > 0) and fill
 * s_al_mapHints and s_al_bspSpeakers[].                                  */
static void S_AL_ParseMapEntities( void )
{
    const char *p;
    const char *tok;
    char key[256], val[256];
    char blkClass[64], blkSky[64];
    char blkAmbient[MAX_QPATH], blkMusic[MAX_QPATH];
    char blkNoise[MAX_QPATH], blkOriginStr[96];
    int  blkFlags;

    Com_Memset(&s_al_mapHints, 0, sizeof(s_al_mapHints));
    s_al_numBspSpeakers  = 0;
    s_al_mapHints.parsed = qtrue;

    p = CM_EntityString();
    if (!p || !*p) return;

    for (;;) {
        tok = COM_ParseExt(&p, qtrue);
        if (!tok || !tok[0]) break;
        if (tok[0] != '{') continue;

        blkClass[0] = blkSky[0] = blkAmbient[0] = blkMusic[0] = '\0';
        blkNoise[0] = blkOriginStr[0] = '\0';
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
        }

        if (!Q_stricmp(blkClass, "worldspawn")) {
            if (blkSky[0]) {
                Q_strncpyz(s_al_mapHints.skyShader, blkSky,
                           sizeof(s_al_mapHints.skyShader));
                s_al_mapHints.hasSky = qtrue;
            }
            if (blkAmbient[0])
                Q_strncpyz(s_al_mapHints.worldAmbient, blkAmbient,
                           sizeof(s_al_mapHints.worldAmbient));
            if (blkMusic[0])
                Q_strncpyz(s_al_mapHints.worldMusic, blkMusic,
                           sizeof(s_al_mapHints.worldMusic));
        } else if (!Q_stricmp(blkClass, "target_speaker") && blkNoise[0]) {
            s_al_mapHints.totalSpeakerCount++;
            if (blkFlags & 1) s_al_mapHints.globalSpeakerCount++;

            if (s_al_numBspSpeakers < S_AL_BSP_SPEAKERS_MAX) {
                alBspSpeaker_t *sp = &s_al_bspSpeakers[s_al_numBspSpeakers++];
                Q_strncpyz(sp->noise, blkNoise, sizeof(sp->noise));
                sp->spawnflags = blkFlags;
                if (blkOriginStr[0])
                    sscanf(blkOriginStr, "%f %f %f",
                           &sp->origin[0], &sp->origin[1], &sp->origin[2]);
            }
        }
    }

    Com_DPrintf("S_AL: map hints: sky=%s ambient=[%s] music=[%s] "
                "speakers=%d (%d global)\n",
        s_al_mapHints.hasSky ? s_al_mapHints.skyShader : "none",
        s_al_mapHints.worldAmbient, s_al_mapHints.worldMusic,
        s_al_mapHints.totalSpeakerCount, s_al_mapHints.globalSpeakerCount);
}

/* Force-register every sound referenced in the BSP entity lump so that
 * normGain is computed for all of them before WriteMapNormCache runs.    */
static void S_AL_PreRegisterMapSounds( void )
{
    int i;
    if (s_al_mapHints.worldAmbient[0])
        S_AL_RegisterSound(s_al_mapHints.worldAmbient, qfalse);
    if (s_al_mapHints.worldMusic[0])
        S_AL_RegisterSound(s_al_mapHints.worldMusic,   qfalse);
    for (i = 0; i < s_al_numBspSpeakers; i++) {
        if (s_al_bspSpeakers[i].noise[0])
            S_AL_RegisterSound(s_al_bspSpeakers[i].noise, qfalse);
    }
}

/* Write normcache/<mapbase>.cfg with the normGain for every BSP sound.
 * The file is human-editable; players/admins can hand-tune per-map levels
 * then type snd_normcache_rebuild to push the new values live.            */
static void S_AL_WriteMapNormCache( const char *mapbase )
{
    char        path[MAX_QPATH];
    char       *buf;
    int         bufLen, written, i;
    /* Collect unique paths: worldAmbient + worldMusic + all speaker noises */
    const char *sndPaths[S_AL_BSP_SPEAKERS_MAX + 2];
    int         numPaths = 0;

    if (!mapbase || !mapbase[0]) return;

    if (s_al_mapHints.worldAmbient[0])
        sndPaths[numPaths++] = s_al_mapHints.worldAmbient;
    if (s_al_mapHints.worldMusic[0])
        sndPaths[numPaths++] = s_al_mapHints.worldMusic;
    for (i = 0; i < s_al_numBspSpeakers; i++)
        if (s_al_bspSpeakers[i].noise[0])
            sndPaths[numPaths++] = s_al_bspSpeakers[i].noise;

    if (!numPaths) return;

    Com_sprintf(path, sizeof(path), "normcache/%s.cfg", mapbase);

    bufLen  = 512 + numPaths * 96;
    buf     = (char *)Z_Malloc(bufLen);
    written = 0;

    written += Com_sprintf(buf + written, bufLen - written,
        "// Quake3e-urt per-map ambient normalisation cache\n"
        "// map: %s\n"
        "// normGain 0.05–1.00  (1.0 = natural level; lower = quieter)\n"
        "// Edit values to tune loudness. Delete file to regenerate.\n"
        "// Reload with: snd_normcache_rebuild\n",
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

/* Load normcache/<mapbase>.cfg.
 * Populates s_al_normCache[] for RegisterSound overrides and immediately
 * applies entries to sfx records already in the cache from a previous map. */
static qboolean S_AL_LoadMapNormCache( const char *mapbase )
{
    char        path[MAX_QPATH];
    char       *buf;
    const char *p;
    const char *tok;
    int         fileLen, applied;

    if (!mapbase || !mapbase[0]) return qfalse;

    Com_sprintf(path, sizeof(path), "normcache/%s.cfg", mapbase);
    fileLen = FS_ReadFile(path, (void **)&buf);
    if (fileLen <= 0 || !buf) return qfalse;

    s_al_normCacheCount = 0;
    applied = 0;
    p = buf;

    while (*p) {
        char        sfxPath[MAX_QPATH];
        float       ng;
        alSfxRec_t *r;

        /* COM_ParseExt already skips // comments and blank lines.
         * The first token on each data line is the sfx path. */
        tok = COM_ParseExt(&p, qtrue);
        if (!tok || !tok[0]) break;

        Q_strncpyz(sfxPath, tok, sizeof(sfxPath));

        tok = COM_ParseExt(&p, qfalse);   /* normGain on same line */
        if (!tok || !tok[0]) continue;
        ng = (float)atof(tok);
        if (ng < 0.05f) ng = 0.05f;
        if (ng > 1.0f)  ng = 1.0f;

        /* Store in pending table — RegisterSound checks this for new loads */
        if (s_al_normCacheCount < S_AL_NORMCACHE_MAX) {
            Q_strncpyz(s_al_normCache[s_al_normCacheCount].path,
                       sfxPath, MAX_QPATH);
            s_al_normCache[s_al_normCacheCount].normGain = ng;
            s_al_normCacheCount++;
        }

        /* Apply to sfx records already in memory from a previous session */
        r = S_AL_FindSfx(sfxPath);
        if (r && r->inMemory) {
            r->normGain = ng;
            applied++;
        }
    }

    FS_FreeFile(buf);

    /* Mark written so we don't accidentally overwrite a user-edited cache */
    s_al_normCacheWritten = qtrue;
    Com_Printf("S_AL: loaded normcache %s  (%d entries, %d applied)\n",
               path, s_al_normCacheCount, applied);
    return qtrue;
}

/* Called once per map on the first dynamic-reverb probe cycle after the BSP
 * is loaded.  Parses BSP entities, then either loads the existing normcache
 * (if present) or decodes all BSP sounds and writes a fresh one.         */
static void S_AL_InitMapAudio( void )
{
    char mapbase[MAX_QPATH];

    S_AL_ParseMapEntities();

    S_AL_MapBaseName(mapbase, sizeof(mapbase));
    if (!mapbase[0]) return;

    if (!S_AL_LoadMapNormCache(mapbase)) {
        S_AL_PreRegisterMapSounds();
        S_AL_WriteMapNormCache(mapbase);
    }
}

/* Console command: regenerate the normcache for the current map.
 * Use after manually editing a cache file to push the values live, or to
 * discard a bad cache and let the engine recompute from the audio files.  */
static void S_AL_RebuildNormCache_f( void )
{
    char mapbase[MAX_QPATH];

    if (!s_al_started || CM_NumInlineModels() <= 0) {
        Com_Printf("S_AL: snd_normcache_rebuild — no map loaded\n");
        return;
    }
    s_al_normCacheCount   = 0;
    s_al_normCacheWritten = qfalse;

    S_AL_MapBaseName(mapbase, sizeof(mapbase));
    if (!mapbase[0]) {
        Com_Printf("S_AL: snd_normcache_rebuild — cannot determine map name\n");
        return;
    }
    S_AL_PreRegisterMapSounds();
    S_AL_WriteMapNormCache(mapbase);
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
    /* For the listener entity use the engine's predicted position so that
     * lp->origin is already correct if anything reads it before UpdateLoops. */
    if (entityNum == s_al_listener_entnum && s_al_listener_entnum >= 0)
        VectorCopy(s_al_listener_origin, lp->origin);
    else
        VectorCopy(origin, lp->origin);
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
                /* Start fade-out instead of hard-stopping.
                 * Record the current normalised fade-in level so the fade-out
                 * always ramps down from where we actually are, not from 1.0
                 * (which would cause a gain jump if fade-in was incomplete). */
                if (!lp->fadeOutStartMs) {
                    lp->fadeOutStartMs = (now > 0) ? now : 1;
                    if (lp->fadeStartMs > 0) {
                        int fadeInMs = s_alLoopFadeInMs
                                       ? (int)s_alLoopFadeInMs->value
                                       : S_AL_LOOP_FADEIN_MS;
                        if (fadeInMs < 1) fadeInMs = 1;
                        {
                            int el = now - lp->fadeStartMs;
                            lp->fadeOutStartGain = (el >= fadeInMs) ? 1.0f
                                : (float)el / (float)fadeInMs;
                        }
                    } else {
                        lp->fadeOutStartGain = 1.0f;  /* fade-in complete */
                    }
                }
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
                int foMs    = s_alLoopFadeOutMs ? (int)s_alLoopFadeOutMs->value
                                                : S_AL_LOOP_FADEOUT_MS;
                int elapsed = now - lp->fadeOutStartMs;
                if (foMs < 1) foMs = 1;
                if (elapsed >= foMs) {
                    /* Fade complete — hard-stop the source. */
                    S_AL_StopSource(lp->srcIdx);
                    s_al_src[lp->srcIdx].loopSound = qfalse;
                    lp->srcIdx         = -1;
                    lp->fadeOutStartMs = 0;
                    lp->fadeStartMs    = 0;
                } else {
                    float t;
                    alSrc_t *src    = &s_al_src[lp->srcIdx];
                    float    catVol = S_AL_GetCategoryVol(SRC_CAT_AMBIENT);
                    /* foMs already computed above — reuse it. */
                    t = lp->fadeOutStartGain *
                        (1.0f - (float)elapsed / (float)foMs);
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
         * entity re-entered the snapshot before the fade finished.)
         *
         * We compute the current fade-out gain so the fade-in begins from
         * exactly that level — preventing a gain jump (pop) at re-activation.
         * fadeStartMs is offset into the past so that Step 5's (elapsed/fiMs)
         * equals the current gain on its very first application. */
        if (lp->fadeOutStartMs > 0) {
            int foMs = s_alLoopFadeOutMs ? (int)s_alLoopFadeOutMs->value
                                         : S_AL_LOOP_FADEOUT_MS;
            int fiMs = s_alLoopFadeInMs  ? (int)s_alLoopFadeInMs->value
                                         : S_AL_LOOP_FADEIN_MS;
            if (foMs < 1) foMs = 1;
            if (fiMs < 1) fiMs = 1;
            {
                int   elapsedOut = now - lp->fadeOutStartMs;
                /* Guard against clock skew / NTP jumps: a negative elapsed
                 * would produce a curGain > fadeOutStartGain (> 1.0).  The
                 * clamps below would catch it, but clamping elapsedOut here
                 * is cleaner and avoids a spurious fadeStartMs offset. */
                if (elapsedOut < 0) elapsedOut = 0;
                float curGain = (elapsedOut >= foMs) ? 0.0f
                    : lp->fadeOutStartGain * (1.0f - (float)elapsedOut / (float)foMs);
                if (curGain < 0.0f) curGain = 0.0f;
                if (curGain > 1.0f) curGain = 1.0f;
                lp->fadeStartMs = now - (int)(curGain * (float)fiMs);
            }
            lp->fadeOutStartMs = 0;
        }

        /* ---- Step 4: Start a new source if needed ---- */
        /* S_AL_UpdateLoops runs from S_Update, which is called after the full
         * cgame frame (SCR_UpdateScreen) — meaning S_Respatialize has already
         * refreshed s_al_listener_origin to the Pmove-predicted position this
         * frame.  For the listener entity, prefer that authoritative origin
         * over lp->origin (which is the cgame-supplied network position, lagging
         * by sv_bufferMs when sv_smoothClients is active). */
        {
        const float *loopOrigin = (i == s_al_listener_entnum && s_al_listener_entnum >= 0)
                                  ? s_al_listener_origin : lp->origin;

        /* Normalise a stale srcIdx before the dedup check so that the dedup
         * scan always runs when we truly need a new source.
         *
         * Two cases make an existing srcIdx stale on an active entity:
         *   (a) The source stopped without going through our fade-out path
         *       (driver reset, or StopAllSounds without clearing loop state).
         *       isPlaying (our software flag) is still qtrue; we detect
         *       staleness only when srcIdx >= 0 and !isPlaying.
         *   (b) The entity's desired sfx changed while the source was playing
         *       (e.g. weapon swap — must restart immediately on the new buffer).
         *
         * In case (b) we hard-stop the old source.  The new source will begin
         * a normal fade-in via the freshly-cleared fadeStartMs below. */
        if (lp->srcIdx >= 0 && lp->srcIdx < s_al_numSrc) {
            qboolean stale = !s_al_src[lp->srcIdx].isPlaying;
            if (!stale && s_al_src[lp->srcIdx].sfx != lp->sfx)
                stale = qtrue;
            if (stale) {
                if (s_al_src[lp->srcIdx].isPlaying)
                    S_AL_StopSource(lp->srcIdx);
                s_al_src[lp->srcIdx].loopSound = qfalse;
                lp->srcIdx      = -1;
                lp->fadeStartMs = 0;
            }
        }

        if (lp->srcIdx < 0) {
            /* Dedup: only ONE AL source per sfx may run at a time.
             *
             * Scan ALL loop slots — active AND fading-out — for any slot
             * that has a source still playing on the same sfx.  The old
             * guard `!other->active` was removed intentionally: a slot that
             * is fading out (active=false, srcIdx valid, isPlaying=true) is
             * still driving audio from an independent playback cursor.  If we
             * started a second source now, both cursors would drift apart with
             * every loop cycle, producing constructive/destructive interference
             * heard as wavering distortion at the loop boundary.
             *
             * The newcomer waits until the fade-out fully stops the existing
             * source (srcIdx → -1), then claims a source on the next frame.
             * The brief holdoff (≤ fade-out duration) is inaudible; the
             * phase-divergence distortion it prevents is very much audible. */
            {
                int      k;
                qboolean sfxDuped = qfalse;
                for (k = 0; k < MAX_GENTITIES; k++) {
                    alLoop_t *other;
                    if (k == i) continue;
                    other = &s_al_loops[k];
                    if (other->srcIdx < 0) continue;        /* no source */
                    if (other->sfx != lp->sfx) continue;    /* different sfx */
                    if (s_al_src[other->srcIdx].isPlaying) { sfxDuped = qtrue; break; }
                }
                if (sfxDuped) continue;
            }
            srcIdx = S_AL_GetFreeSource();
            if (srcIdx < 0) continue;
            {
                float vol = S_AL_GetMasterVol();
                S_AL_SrcSetup(srcIdx, lp->sfx, loopOrigin, qtrue,
                              i, CHAN_AUTO, vol, qfalse,
                              qtrue /* looping ambient */);
            }
            s_al_src[srcIdx].loopSound = qtrue;
            qalSourcei(s_al_src[srcIdx].source, AL_LOOPING, AL_TRUE);
            /* Tag non-global BSP target_speaker loops so the occlusion
             * pass can apply wall-muffling to them.  Global speakers
             * (spawnflags & 1) are intentionally heard everywhere; they
             * are excluded so they are never attenuated through walls. */
            {
                int bk;
                const char *sfxName = (lp->sfx >= 0 && lp->sfx < s_al_numSfx)
                                      ? s_al_sfx[lp->sfx].name : NULL;
                if (sfxName) {
                    for (bk = 0; bk < s_al_numBspSpeakers; bk++) {
                        float dx, dy, dz;
                        if (s_al_bspSpeakers[bk].spawnflags & 1) continue;
                        if (!(Q_stristr(sfxName, s_al_bspSpeakers[bk].noise) ||
                                Q_stristr(s_al_bspSpeakers[bk].noise, sfxName))) continue;
                        /* Confirm by position: only tag if this loop entity is
                         * actually at the BSP speaker's origin (within 4 units).
                         * This prevents misclassifying a non-speaker loop that
                         * happens to share the same sound file with a BSP speaker
                         * at a different location (e.g. a global speaker playing
                         * the same noise as a non-global one). */
                        dx = loopOrigin[0] - s_al_bspSpeakers[bk].origin[0];
                        dy = loopOrigin[1] - s_al_bspSpeakers[bk].origin[1];
                        dz = loopOrigin[2] - s_al_bspSpeakers[bk].origin[2];
                        if (dx*dx + dy*dy + dz*dz > S_AL_BSP_SPEAKER_ORIGIN_MATCH_SQ) continue;
                        s_al_src[srcIdx].isBspSpeaker = qtrue;
                        break;
                    }
                }
            }
            /* Start gain: normally 0 (time-based fade-in prevents pop).
             * When s_alLoopFadeDist > 0, compute the distance-proportional
             * gain so a source that appears far from the listener starts
             * proportionally quiet — matches what OpenAL distance rolloff
             * gives at that point — preventing the "ramp-in from silence"
             * for sources that are audible but distant. */
            {
                float startGain = 0.0f;
                float fadeDist  = s_alLoopFadeDist ? s_alLoopFadeDist->value : 0.0f;
                if (fadeDist > 0.0f) {
                    float maxDist = s_alMaxDist ? s_alMaxDist->value : S_AL_SOUND_MAXDIST;
                    float ref     = S_AL_SOUND_FULLVOLUME;
                    vec3_t d;
                    float  dist;
                    VectorSubtract(loopOrigin, s_al_listener_origin, d);
                    dist = VectorLength(d);
                    if (dist >= maxDist)
                        startGain = 0.0f;
                    else if (dist <= ref)
                        startGain = 1.0f;   /* in full-volume zone — skip time fade */
                    else {
                        float zoneStart = maxDist - fadeDist;
                        if (dist <= zoneStart)
                            startGain = 1.0f;  /* inside fade zone but above ref — full */
                        else
                            startGain = (maxDist - dist) / fadeDist;
                    }
                    if (startGain < 0.0f) startGain = 0.0f;
                    if (startGain > 1.0f) startGain = 1.0f;
                }
                qalSourcef(s_al_src[srcIdx].source, AL_GAIN, startGain *
                           (s_al_src[srcIdx].master_vol / 255.f) *
                           S_AL_GetCategoryVol(SRC_CAT_AMBIENT) *
                           (lp->sfx >= 0 && lp->sfx < s_al_numSfx
                            ? s_al_sfx[lp->sfx].normGain : 1.0f));
            }
            qalSourcePlay(s_al_src[srcIdx].source);
            lp->srcIdx      = srcIdx;
            lp->fadeStartMs = (now > 0) ? now : 1;
        } else {
            /* Update position and velocity every frame. */
            qalSource3f(s_al_src[lp->srcIdx].source, AL_POSITION,
                        loopOrigin[0], loopOrigin[1], loopOrigin[2]);
            qalSource3f(s_al_src[lp->srcIdx].source, AL_VELOCITY,
                        lp->velocity[0], lp->velocity[1], lp->velocity[2]);
        }
        } /* end loopOrigin scope */

        /* ---- Step 5: Apply gain (fade-in ramp, or settled level) ---- *
         * Runs every frame so that s_alVolEnv cvar changes take effect on
         * active ambient loops regardless of fade state.  The vol-update block
         * in S_AL_Update skips loop sources precisely because this step is the
         * authoritative gain writer for all loop sources — both mid-fade and
         * settled.
         *
         * For BSP speaker loops without EFX, the occlusion pass cannot write
         * AL_GAIN itself (this step would overwrite it), so we read the
         * smoothed occlusionGain back here and fold it into the final value. */
        {
            alSrc_t *src    = &s_al_src[lp->srcIdx];
            float    catVol = S_AL_GetCategoryVol(SRC_CAT_AMBIENT);
            /* Non-EFX BSP speaker occlusion: occlusionGain is maintained by
             * the S_Update occlusion pass; fold it in here so S_AL_UpdateLoops
             * doesn't overwrite the muffled level with the unoccluded value. */
            float    occFactor = (src->isBspSpeaker && !s_al_efx.available)
                                 ? src->occlusionGain : 1.0f;
            if (lp->sfx >= 0 && lp->sfx < s_al_numSfx)
                catVol *= s_al_sfx[lp->sfx].normGain;

            if (lp->fadeStartMs > 0) {
                /* Fade-in ramp: gain 0 → 1 over fadeInMs. */
                float fadeGain;
                int   elapsed = now - lp->fadeStartMs;
                int   fiMs    = s_alLoopFadeInMs ? (int)s_alLoopFadeInMs->value
                                                  : S_AL_LOOP_FADEIN_MS;
                if (fiMs < 1) fiMs = 1;
                if (elapsed >= fiMs) {
                    fadeGain        = 1.0f;
                    lp->fadeStartMs = 0;   /* fade complete */
                } else {
                    fadeGain = (float)elapsed / (float)fiMs;
                }
                qalSourcef(src->source, AL_GAIN,
                           (src->master_vol / 255.f) * catVol * fadeGain * occFactor);
            } else {
                /* Settled level — write every frame so live cvar adjustments
                 * (s_alVolEnv, s_alVolSelf, etc.) are immediately reflected.
                 * Cost is negligible: one qalSourcef per active ambient loop. */
                qalSourcef(src->source, AL_GAIN,
                           (src->master_vol / 255.f) * catVol * occFactor);
            }
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
    VectorCopy(axis[0], s_al_listener_forward);
    qalListenerfv(AL_ORIENTATION, orient);
}

static void S_AL_UpdateEntityPosition( int entityNum, const vec3_t origin )
{
    int i;
    const float *effectiveOrigin;

    /* For the listener entity, always use the engine's authoritative predicted
     * position maintained by S_AL_Respatialize rather than the cgame-supplied
     * origin.  The cgame derives entity positions from the network trajectory
     * (TR_LINEAR / TR_INTERPOLATE) which can lag client-side prediction when
     * sv_smoothClients / sv_bufferMs is active, producing a stale offset that
     * scales with velocity.  Respatialize is always called with the Pmove-
     * predicted eye position and is the single ground truth for the local
     * player regardless of server configuration or QVM patch level.
     *
     * Two things this guards:
     *   1. The entity-origin cache write — cgame calls UpdateEntityPosition
     *      for all entities every frame, including the local player; without
     *      this guard the cache write could overwrite the correct value set
     *      by Respatialize earlier in the same frame.
     *   2. Active-source position updates — any own-player sound started this
     *      frame (e.g. footsteps) should track the predicted position, not the
     *      stale network position.
     *
     * Scope: strictly limited to entityNum == s_al_listener_entnum.
     * All other entities — other players, world, bots — use the cgame-supplied
     * origin, which is correct for them. */
    if (entityNum == s_al_listener_entnum && s_al_listener_entnum >= 0)
        effectiveOrigin = s_al_listener_origin;
    else
        effectiveOrigin = origin;

    /* Keep the per-entity origin cache current so that StartSound can place
     * new one-shot sounds at the right position when origin is NULL. */
    if (entityNum >= 0 && entityNum < MAX_GENTITIES)
        VectorCopy(effectiveOrigin, s_al_entity_origins[entityNum]);

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
            VectorCopy(effectiveOrigin, s_al_src[i].origin);
            qalSource3f(s_al_src[i].source, AL_POSITION,
                        effectiveOrigin[0], effectiveOrigin[1], effectiveOrigin[2]);
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
 * EFX parameter targets (late-gain max 0.53, decay max 3.0 s) are tuned
 * to give URT maps a clearly audible acoustic character while leaving the
 * direct signal intelligible.
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
    static float curDecay    = -1.f;
    static float curLate     =  0.f;
    static float curRefl     =  0.f;
    static float curSlot     =  0.f;
    static float curHFRatio  = -1.f;  /* decay HF ratio — surface hardness   */
    static float curDensity  = -1.f;  /* echo density                         */
    static float curDiffusion = -1.f; /* wall diffusion (flutter vs scatter)  */
    static float curGainHF   = -1.f;  /* reverb tail treble level (EQ)        */
    static float curGainLF   = -1.f;  /* reverb tail bass level (EQ)          */
    /* All five start at -1 so the curDecay < 0 snap check (line below) also
     * initialises them on the very first probe cycle, just like curDecay. */
    static int   lastFrame = -(S_AL_ENV_RATE);

    /* Rolling environment history — averages the last S_AL_ENV_HISTORY probe
     * sets so that a single "confused" reading (thin wall, window, doorway
     * straddling) only moves the average by 1/N rather than flipping the
     * whole classification. */
    static float histSize[S_AL_ENV_HISTORY_MAX];
    static float histOpen[S_AL_ENV_HISTORY_MAX];
    static float histCorr[S_AL_ENV_HISTORY_MAX];
    static float histVert[S_AL_ENV_HISTORY_MAX];  /* vertical openness history */
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
    float targetHFRatio, targetDensity, targetDiffusion;
    float targetGainHF, targetGainLF;
    float caveBonus, openBoxFrac;
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

        /* Parse BSP entities and load/write the per-map normcache the first
         * time we probe after a new map is loaded.  CM_NumInlineModels() > 0
         * at this point so CM_EntityString() is safe to call. */
        if (!s_al_mapHints.parsed)
            S_AL_InitMapAudio();
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
        histVert[histIdx] = vertOpenFrac;
        histIdx = (histIdx + 1) % historySize;
        if (histCount < historySize) histCount++;
        /* If the user shrank historySize, drop excess entries. */
        if (histCount > historySize) histCount = historySize;

        if (histCount > 1) {
            float sumSize = 0.f, sumOpen = 0.f, sumCorr = 0.f, sumVert = 0.f;
            for (i = 0; i < histCount; i++) {
                sumSize += histSize[i];
                sumOpen += histOpen[i];
                sumCorr += histCorr[i];
                sumVert += histVert[i];
            }
            sizeFactor   = sumSize / (float)histCount;
            openFrac     = sumOpen / (float)histCount;
            corrFactor   = sumCorr / (float)histCount;
            vertOpenFrac = sumVert / (float)histCount;
        }
    }

    /* --- Derive target EFX parameters ------------------------------------ */
    baseGain = (s_alReverbGain && s_alReverbGain->value > 0.f)
               ? s_alReverbGain->value : 0.35f;

    /* ---- Cave bonus: large horizontal space with enclosed overhead --------
     * Detects caves, mine tunnels, and underground passages.  The listener
     * sees far horizontally (sizeFactor high) but solid rock overhead keeps
     * vertOpenFrac below 0.25.  Boosts decay, HF-ratio (stone surfaces), echo
     * density, and late reverb for the thick, long-tailed underground sound. */
    {
        float coveredCeiling = (0.25f - vertOpenFrac) / 0.25f;
        if (coveredCeiling < 0.f) coveredCeiling = 0.f;
        caveBonus = sizeFactor * coveredCeiling;
        if (caveBonus > 1.f) caveBonus = 1.f;
    }

    /* ---- BSP sky hint: dampen cave bonus for outdoor/urban maps -----------
     * A map with a worldspawn "sky" key is an outdoor or urban layout —
     * it will never be a true underground cave regardless of what the local
     * geometry looks like.  If the probe fires inside a basement or narrow
     * stairwell the caveBonus would otherwise trigger stone-tunnel reverb,
     * which sounds wrong in a building that opens to sky.  Reduce it to 35%
     * so enclosed sub-areas still get some reverb body without the full
     * mine-shaft character. */
    if (s_al_mapHints.hasSky && caveBonus > 0.f)
        caveBonus *= 0.35f;

    /* ---- Open-box fraction: walls all around but open sky above ----------
     * Detects URT "open box" designs: rooftops, open-top courtyards, walled
     * arenas with no ceiling.  Signature: vertOpenFrac >> horizOpenFrac.
     * Gives shorter decay (no ceiling to trap reverb), strong early
     * reflections (building walls), and moderate slot gain. */
    {
        float vertExcess = vertOpenFrac - horizOpenFrac - 0.20f;
        if (vertExcess < 0.f) vertExcess = 0.f;
        openBoxFrac = (horizOpenFrac < 0.50f) ? (vertExcess / 0.60f) : 0.f;
        if (openBoxFrac > 1.f) openBoxFrac = 1.f;
    }

    /* Decay: short room (0.5 s) up to large space (3.0 s), killed outdoors.
     * caveBonus adds up to 1.0 s extra tail for hard stone reflections;
     * openBoxFrac trims the tail since the open sky kills long reverberation. */
    targetDecay = 0.5f + sizeFactor * 2.5f;
    targetDecay *= (1.f - openFrac * 0.85f);
    targetDecay += caveBonus  * 1.0f;
    targetDecay -= openBoxFrac * 0.30f;
    if (targetDecay < 0.1f) targetDecay = 0.1f;

    /* Late reverb tail: max 0.53 — clearly audible in enclosed spaces. */
    targetLate = 0.08f + sizeFactor * 0.45f;
    targetLate *= (1.f - openFrac * 0.85f);
    targetLate += caveBonus * 0.12f;
    if (targetLate < 0.f) targetLate = 0.f;
    if (targetLate > 1.f) targetLate = 1.f;

    /* Early reflections: corridor-boosted, urban semi-open boost, open-box boost.
     *
     * Semi-open urban boost (0.20–0.55 openFrac, non-corridor): courtyards,
     * plazas, short hallways with doorways and walled alleys all reflect sound
     * off surrounding faces without forming a single dominant corridor axis.
     * A parabolic lift over that range gives them a characteristic early-
     * reflection signature instead of flat near-dry reverb.
     *
     * Open-box boost: vertical walls produce strong first-order reflections
     * even though the absence of a ceiling means no long late-reverb tail. */
    targetRefl = 0.05f + corrFactor * 0.35f;
    if (openFrac > 0.20f && openFrac < 0.55f && corrFactor < 0.30f) {
        float x = (openFrac - 0.20f) / 0.35f;          /* 0→1 over 0.20→0.55 */
        targetRefl += 4.f * x * (1.f - x) * 0.20f;    /* parabola, peak 0.20 */
    }
    targetRefl += openBoxFrac * 0.18f;
    targetRefl *= (1.f - openFrac * 0.75f);
    if (targetRefl < 0.f) targetRefl = 0.f;
    if (targetRefl > 1.f) targetRefl = 1.f;

    /* Slot gain: scales with enclosure; outdoor → near-dry.
     * Open-box gets a small bonus: wall reflections add wetness even without
     * a ceiling to trap the tail. */
    targetSlot = baseGain * (1.f - openFrac * 0.70f);
    targetSlot += openBoxFrac * baseGain * 0.15f;
    if (targetSlot < 0.f) targetSlot = 0.f;
    if (targetSlot > 1.f) targetSlot = 1.f;

    /* Decay HF ratio: characterises surface hardness / material absorption.
     *   Outdoor / forest  — ~0.55: air and foliage absorb HF quickly.
     *   Concrete indoor   — ~0.83: default plaster/concrete.
     *   Corridor          — ~1.05: flat parallel walls sustain HF (flutter).
     *   Stone cave        — ~1.25: hard reflective surfaces; HF persists. */
    targetHFRatio = 0.55f + (1.f - openFrac) * 0.55f
                   + caveBonus  * 0.45f
                   + corrFactor * 0.20f;
    if (targetHFRatio < 0.1f) targetHFRatio = 0.1f;
    if (targetHFRatio > 2.0f) targetHFRatio = 2.0f;

    /* Echo density: sparse outdoors, increasingly rich indoors and in caves. */
    targetDensity = 0.20f + (1.f - openFrac) * 0.50f + caveBonus * 0.30f;
    if (targetDensity < 0.0f) targetDensity = 0.0f;
    if (targetDensity > 1.0f) targetDensity = 1.0f;

    /* Diffusion: wall irregularity.
     * Corridors are less diffuse — parallel flat walls create flutter echo,
     * which is itself an audible cue that the player is in a tight passage.
     * Caves are highly diffuse — irregular rock surfaces scatter reflections.
     * Outdoors and open-box: low (few surfaces to scatter off). */
    targetDiffusion = 0.20f + (1.f - openFrac) * 0.55f
                     + caveBonus  * 0.25f
                     - corrFactor * 0.30f;
    if (targetDiffusion < 0.0f) targetDiffusion = 0.0f;
    if (targetDiffusion > 1.0f) targetDiffusion = 1.0f;

    /* GainHF: high-frequency content of the reverb tail (EQ brightness).
     * Outdoor: dim (air absorption thins HF over distance).
     * Corridor: bright (concrete flush-wall flutter is rich in treble).
     * Cave: strong (stone surfaces reflect HF with little absorption). */
    targetGainHF = 0.60f + (1.f - openFrac) * 0.35f
                  + corrFactor * 0.12f
                  + caveBonus  * 0.10f
                  - openBoxFrac * 0.08f;
    if (targetGainHF < 0.0f) targetGainHF = 0.0f;
    if (targetGainHF > 1.0f) targetGainHF = 1.0f;

    /* GainLF: low-frequency content of the reverb tail (EQ bass weight).
     * Outdoor: thin (little LF reinforcement in open air).
     * Large indoor / cave: resonant (volume of enclosed air boosts bass).
     * Open-box: trimmed (no ceiling to build standing low-frequency waves). */
    targetGainLF = 0.50f + (1.f - openFrac) * 0.40f
                  + caveBonus  * 0.20f
                  - openBoxFrac * 0.15f;
    if (targetGainLF < 0.0f) targetGainLF = 0.0f;
    if (targetGainLF > 1.0f) targetGainLF = 1.0f;

    /* --- Smooth blend or snap -------------------------------------------- */
    if (snap) {
        curDecay     = targetDecay;
        curLate      = targetLate;
        curRefl      = targetRefl;
        curSlot      = targetSlot;
        curHFRatio   = targetHFRatio;
        curDensity   = targetDensity;
        curDiffusion = targetDiffusion;
        curGainHF    = targetGainHF;
        curGainLF    = targetGainLF;
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
        curDecay     = curDecay     * blendPole + targetDecay     * (1.f - blendPole);
        curLate      = curLate      * blendPole + targetLate      * (1.f - blendPole);
        curRefl      = curRefl      * blendPole + targetRefl      * (1.f - blendPole);
        curSlot      = curSlot      * blendPole + targetSlot      * (1.f - blendPole);
        curHFRatio   = curHFRatio   * blendPole + targetHFRatio   * (1.f - blendPole);
        curDensity   = curDensity   * blendPole + targetDensity   * (1.f - blendPole);
        curDiffusion = curDiffusion * blendPole + targetDiffusion * (1.f - blendPole);
        curGainHF    = curGainHF    * blendPole + targetGainHF    * (1.f - blendPole);
        curGainLF    = curGainLF    * blendPole + targetGainLF    * (1.f - blendPole);
    }

    /* --- Fire-direction impact reverb boost (Feature C) -------------------
     * When the local player just fired and s_alFireImpactReverb is on, cast
     * 3 short rays in a narrow forward cone in the aim direction.  If any ray
     * hits solid geometry within 400 units, boost the smoothed cur* values
     * directly so the effect is immediate and independent of what environment
     * the classifier is currently reporting — a weapon fired at a wall always
     * produces a sharp early reflection regardless of whether the probe says
     * CORRIDOR, TRANSITION, or OUTDOOR.  The IIR smoother then naturally
     * decays the boost back toward the environment target over the next cycles. */
    if (s_alFireImpactReverb && s_alFireImpactReverb->integer
            && (s_al_loopFrame - s_al_fire_frame) <= S_AL_ENV_RATE * 2
            && CM_NumInlineModels() > 0) {
        static const float coneOffsets[3][2] = {
            /* [0] = right-blend fraction, [1] = reserved (vertical, unused) */
            { 0.f,   0.f  },   /* straight ahead */
            { 0.12f, 0.f  },   /* slight right */
            {-0.12f, 0.f  },   /* slight left  */
        };
        float   bestHit   = 1.0f;         /* fraction (1 = miss) */
        float   fireRange = 400.0f;
        float   maxBoost  = s_alFireImpactMaxBoost
                            ? s_alFireImpactMaxBoost->value : 0.25f;
        int     fi;
        /* Build a rough right-vector from the forward direction */
        float fwdX = s_al_fire_dir[0], fwdY = s_al_fire_dir[1];
        float rgtX = -fwdY, rgtY = fwdX; /* 90° CCW in XY plane */
        if (maxBoost < 0.f) maxBoost = 0.f;
        if (maxBoost > 0.5f) maxBoost = 0.5f;

        for (fi = 0; fi < 3; fi++) {
            trace_t ftr;
            vec3_t  fend;
            float   dx = fwdX + coneOffsets[fi][0] * rgtX;
            float   dy = fwdY + coneOffsets[fi][0] * rgtY;
            float   len = sqrtf(dx*dx + dy*dy);
            if (len > 0.001f) { dx /= len; dy /= len; }
            fend[0] = s_al_listener_origin[0] + dx * fireRange;
            fend[1] = s_al_listener_origin[1] + dy * fireRange;
            fend[2] = s_al_listener_origin[2] + s_al_fire_dir[2] * fireRange;
            CM_BoxTrace(&ftr, s_al_listener_origin, fend,
                        vec3_origin, vec3_origin, 0, CONTENTS_SOLID, qfalse);
            if (ftr.fraction < bestHit)
                bestHit = ftr.fraction;
        }

        if (bestHit < 1.0f) {
            /* Closer hit → stronger reflection boost.  Applied directly to the
             * smoothed output values so the spike is consistent across all
             * environment types — classifier confusion cannot suppress it. */
            float proximity = 1.0f - bestHit;
            float boost     = maxBoost * proximity;
            curRefl += boost;
            if (curRefl > 1.f) curRefl = 1.f;
            /* Short decay bump — muzzle blast echo decays fast.
             * 3.0 s is the absolute ceiling for fire-boosted decay — it exceeds
             * the typical environment-driven maximum (~2.5 s for large caves) to
             * ensure a perceptible spike even on top of an already-long tail. */
            curDecay += boost * 0.3f;
            if (curDecay > 3.0f) curDecay = 3.0f;
            /* Slot gain spike so the muzzle report is audible even when the
             * environment probe has set a dry slot (outdoor, TRANSITION). */
            curSlot += boost * baseGain * 0.60f;
            if (curSlot > 1.f) curSlot = 1.f;
        }
    }

    /* --- Incoming-enemy-fire reverb boost -----------------------------------
     * Complements Feature C by applying a proportional boost when an enemy
     * fires near the listener.  Half the own-fire max boost since the shooter
     * is typically further away; no ray cast needed because we already have
     * the distance-normalised proximity from the trigger in StartSound.
     * Excluded for suppressed weapons and teammates (both filtered at capture).
     * Applied to cur* for the same consistency reason as Feature C above. */
    if (s_alFireImpactReverb && s_alFireImpactReverb->integer
            && (s_al_loopFrame - s_al_incoming_fire_frame) <= S_AL_ENV_RATE * 2
            && CM_NumInlineModels() > 0) {
        float maxBoost  = s_alFireImpactMaxBoost
                          ? s_alFireImpactMaxBoost->value * 0.5f : 0.125f;
        float proximity = 1.0f - s_al_incoming_fire_dist;
        float boost;
        if (maxBoost > 0.25f) maxBoost = 0.25f;
        boost = maxBoost * proximity;
        curRefl += boost;
        if (curRefl > 1.f) curRefl = 1.f;
        curDecay += boost * 0.2f;
        if (curDecay > 3.0f) curDecay = 3.0f;
        curSlot += boost * baseGain * 0.40f;
        if (curSlot > 1.f) curSlot = 1.f;
    }

    /* --- Push to EFX effect and re-upload to slot ------------------------- */
    if (s_al_efx.hasEAXReverb) {
        qalEffectf(s_al_efx.reverbEffect, AL_EAXREVERB_DECAY_TIME,       curDecay);
        qalEffectf(s_al_efx.reverbEffect, AL_EAXREVERB_LATE_REVERB_GAIN, curLate);
        qalEffectf(s_al_efx.reverbEffect, AL_EAXREVERB_REFLECTIONS_GAIN, curRefl);
        qalEffectf(s_al_efx.reverbEffect, AL_EAXREVERB_DECAY_HFRATIO,    curHFRatio);
        qalEffectf(s_al_efx.reverbEffect, AL_EAXREVERB_DENSITY,          curDensity);
        qalEffectf(s_al_efx.reverbEffect, AL_EAXREVERB_DIFFUSION,        curDiffusion);
        qalEffectf(s_al_efx.reverbEffect, AL_EAXREVERB_GAINHF,           curGainHF);
        qalEffectf(s_al_efx.reverbEffect, AL_EAXREVERB_GAINLF,           curGainLF);
    } else {
        qalEffectf(s_al_efx.reverbEffect, AL_REVERB_DECAY_TIME,          curDecay);
        qalEffectf(s_al_efx.reverbEffect, AL_REVERB_LATE_REVERB_GAIN,    curLate);
        qalEffectf(s_al_efx.reverbEffect, AL_REVERB_REFLECTIONS_GAIN,    curRefl);
        qalEffectf(s_al_efx.reverbEffect, AL_REVERB_DECAY_HFRATIO,       curHFRatio);
        qalEffectf(s_al_efx.reverbEffect, AL_REVERB_DENSITY,             curDensity);
        qalEffectf(s_al_efx.reverbEffect, AL_REVERB_DIFFUSION,           curDiffusion);
        qalEffectf(s_al_efx.reverbEffect, AL_REVERB_GAINHF,              curGainHF);
    }
    /* Re-attach effect to slot so parameter changes take effect */
    qalAuxiliaryEffectSloti(s_al_efx.reverbSlot, AL_EFFECTSLOT_EFFECT,
                            (ALint)s_al_efx.reverbEffect);
    qalAuxiliaryEffectSlotf(s_al_efx.reverbSlot, AL_EFFECTSLOT_GAIN, curSlot);

    /* --- Debug output ---------------------------------------------------- */
    if (s_alDebugReverb && s_alDebugReverb->integer >= 1) {
        /* Classify into a human-readable environment label.
         * OPEN_BOX  — walls all around, open sky (rooftop / open-top courtyard)
         * SEMI_OPEN — clearly transitional, leaning outdoor
         * CAVE      — large space with solid overhead (tunnel / mine)
         * CORRIDOR  — single tight axis, closed (pure passage or alley)
         * HALLWAY   — corridor-like but with doorways / openings on sides
         * LARGE_HALL — big enclosed space
         * MEDIUM_ROOM / SMALL_ROOM — standard indoor fallbacks */
        const char *envLabel;
        if      (openFrac    > 0.60f)                       envLabel = "OUTDOOR";
        else if (openBoxFrac > 0.35f)                       envLabel = "OPEN_BOX";
        else if (openFrac    > 0.42f)                       envLabel = "SEMI_OPEN";
        else if (caveBonus   > 0.30f)                       envLabel = "CAVE";
        else if (corrFactor  > 0.40f && openFrac < 0.25f)  envLabel = "CORRIDOR";
        else if (corrFactor  > 0.20f && openFrac < 0.45f)  envLabel = "HALLWAY";
        else if (sizeFactor  > 0.65f)                       envLabel = "LARGE_HALL";
        else if (sizeFactor  > 0.30f)                       envLabel = "MEDIUM_ROOM";
        else                                                envLabel = "SMALL_ROOM";

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
            Com_Printf(S_COLOR_CYAN "             raw: size=%.2f  horizOpen=%.2f  vertOpen=%.2f  corr=%.2f  cave=%.2f  box=%.2f"
                       "  |  hist=%d/%d  vel=%.0f  cache=%s\n",
                       cachedSize, horizOpenFrac, vertOpenFrac, cachedCorr,
                       caveBonus, openBoxFrac,
                       histCount, historySize, cachedVelSpeed,
                       haveCache ? "hit" : "miss");
        }
    }
}

/* S_AL_UpdateStaticFireBoost — fire-impact reverb enhancement when
 * s_alDynamicReverb is OFF.  In static reverb mode the environment probe
 * never runs, so the fire-boost logic inside S_AL_UpdateDynamicReverb is
 * never reached.  This lightweight function provides the same muzzle-report
 * and incoming-fire reverb spike using a one-shot spike + timed restore:
 *   1. Detect fire event via s_al_fire_frame / s_al_incoming_fire_frame.
 *   2. Cast the 3-ray forward cone (own fire) and check proximity (incoming).
 *   3. Temporarily raise EFX reflections, decay, and slot gain.
 *   4. After S_AL_ENV_RATE*2 frames restore the static defaults.
 *
 * Called from S_AL_Update when s_alReverb 1, s_alFireImpactReverb 1,
 * s_alDynamicReverb 0, EFX available. */
static void S_AL_UpdateStaticFireBoost( void )
{
    static int      lastBoostFire   = -99999; /* s_al_fire_frame when last boosted */
    static int      lastBoostInFire = -99999; /* s_al_incoming_fire_frame ditto */
    static int      boostStartFrame = -99999; /* frame the boost was applied */
    static qboolean boostActive     = qfalse;

    float baseDecay, baseRefl, baseSlot, maxBoost, boost;

    if (!s_al_efx.available)                              return;
    if (!s_alReverb        || !s_alReverb->integer)       return;
    if (!s_alFireImpactReverb || !s_alFireImpactReverb->integer) return;
    if (CM_NumInlineModels() <= 0)                        return;

    baseDecay = s_alReverbDecay ? s_alReverbDecay->value : 1.49f;
    baseRefl  = 0.25f;
    baseSlot  = s_alReverbGain  ? s_alReverbGain->value  : 0.35f;
    maxBoost  = s_alFireImpactMaxBoost
                ? s_alFireImpactMaxBoost->value : 0.25f;
    if (maxBoost < 0.f)  maxBoost = 0.f;
    if (maxBoost > 0.5f) maxBoost = 0.5f;

    /* Expire a running boost and restore static defaults */
    if (boostActive &&
            (s_al_loopFrame - boostStartFrame) >= S_AL_ENV_RATE * 2) {
        if (s_al_efx.hasEAXReverb) {
            qalEffectf(s_al_efx.reverbEffect, AL_EAXREVERB_REFLECTIONS_GAIN, baseRefl);
            qalEffectf(s_al_efx.reverbEffect, AL_EAXREVERB_DECAY_TIME,       baseDecay);
        } else {
            qalEffectf(s_al_efx.reverbEffect, AL_REVERB_REFLECTIONS_GAIN, baseRefl);
            qalEffectf(s_al_efx.reverbEffect, AL_REVERB_DECAY_TIME,       baseDecay);
        }
        qalAuxiliaryEffectSloti(s_al_efx.reverbSlot, AL_EFFECTSLOT_EFFECT,
                                (ALint)s_al_efx.reverbEffect);
        qalAuxiliaryEffectSlotf(s_al_efx.reverbSlot, AL_EFFECTSLOT_GAIN, baseSlot);
        boostActive = qfalse;
    }

    boost = 0.f;

    /* Own-fire: 3-ray muzzle cone — only trigger once per fire event */
    if ((s_al_loopFrame - s_al_fire_frame) <= S_AL_ENV_RATE * 2
            && s_al_fire_frame != lastBoostFire) {
        float bestHit  = 1.0f;
        float fireRange = 400.0f;
        float fwdX = s_al_fire_dir[0], fwdY = s_al_fire_dir[1];
        float rgtX = -fwdY, rgtY = fwdX;
        int   fi;
        static const float coneOff[3] = { 0.f, 0.12f, -0.12f };

        for (fi = 0; fi < 3; fi++) {
            trace_t ftr;
            vec3_t  fend;
            float   dx  = fwdX + coneOff[fi] * rgtX;
            float   dy  = fwdY + coneOff[fi] * rgtY;
            float   len = sqrtf(dx*dx + dy*dy);
            if (len > 0.001f) { dx /= len; dy /= len; }
            fend[0] = s_al_listener_origin[0] + dx * fireRange;
            fend[1] = s_al_listener_origin[1] + dy * fireRange;
            fend[2] = s_al_listener_origin[2] + s_al_fire_dir[2] * fireRange;
            CM_BoxTrace(&ftr, s_al_listener_origin, fend,
                        vec3_origin, vec3_origin, 0, CONTENTS_SOLID, qfalse);
            if (ftr.fraction < bestHit) bestHit = ftr.fraction;
        }
        if (bestHit < 1.0f) {
            boost = maxBoost * (1.0f - bestHit);
            lastBoostFire = s_al_fire_frame;
        }
    }

    /* Incoming enemy fire */
    if ((s_al_loopFrame - s_al_incoming_fire_frame) <= S_AL_ENV_RATE * 2
            && s_al_incoming_fire_frame != lastBoostInFire) {
        float inBoostMax = maxBoost * 0.5f;
        float inBoost;
        if (inBoostMax > 0.25f) inBoostMax = 0.25f;   /* cap before use */
        inBoost = inBoostMax * (1.0f - s_al_incoming_fire_dist);
        if (inBoost > boost) {
            boost = inBoost;
            lastBoostInFire = s_al_incoming_fire_frame;
        }
    }

    if (boost > 0.01f && !boostActive) {
        if (s_al_efx.hasEAXReverb) {
            qalEffectf(s_al_efx.reverbEffect, AL_EAXREVERB_REFLECTIONS_GAIN,
                       baseRefl + boost);
            qalEffectf(s_al_efx.reverbEffect, AL_EAXREVERB_DECAY_TIME,
                       baseDecay + boost * 0.3f);
        } else {
            qalEffectf(s_al_efx.reverbEffect, AL_REVERB_REFLECTIONS_GAIN,
                       baseRefl + boost);
            qalEffectf(s_al_efx.reverbEffect, AL_REVERB_DECAY_TIME,
                       baseDecay + boost * 0.3f);
        }
        qalAuxiliaryEffectSloti(s_al_efx.reverbSlot, AL_EFFECTSLOT_EFFECT,
                                (ALint)s_al_efx.reverbEffect);
        qalAuxiliaryEffectSlotf(s_al_efx.reverbSlot, AL_EFFECTSLOT_GAIN,
                                baseSlot + boost * baseSlot * 0.60f);
        boostStartFrame = s_al_loopFrame;
        boostActive     = qtrue;
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

    /* Upload any buffers that were decoded + resampled by worker threads
     * since the last frame.  This is cheap when the queue is empty. */
    S_AL_CommitCompletedJobs();

    /* Handle mute */
    masterGain = s_al_muted ? 0.0f : (s_volume ? s_volume->value : 0.8f);

    /* Reset per-frame HF multipliers; each block below may lower them. */
    s_al_suppressHF = 1.0f;
    s_al_grenadeHF  = 1.0f;
    s_al_healthHF   = 1.0f;

    /* -----------------------------------------------------------------------
     * Grenade-blast concussion effect.
     * Two components that work together:
     *   1. masterGain duck (s_alGrenadeBloomDuck, optional, very mild) —
     *      kept as-is for the physical "pressure" jolt feel.
     *   2. Per-source HF lowpass (s_alGrenadeBloomHFFloor, always active
     *      when s_alGrenadeBloom 1) — the dominant cue that transforms
     *      "volume went down" into "something just exploded near me".
     * ----------------------------------------------------------------------- */
    if (!s_al_muted && s_alGrenadeBloom && s_alGrenadeBloom->integer
            && s_al_bloomExpiry > 0) {
        int   nowGD  = Com_Milliseconds();
        int   bMsGD  = s_alGrenadeBloomMs ? (int)s_alGrenadeBloomMs->value : 180;
        if (bMsGD < 1) bMsGD = 1;

        if (nowGD < s_al_bloomExpiry) {
            int   remain = s_al_bloomExpiry - nowGD;
            float t      = (float)remain / (float)bMsGD;
            if (t > 1.f) t = 1.f;

            /* HF filter: t=1 at trigger → deep cut; t=0 at expiry → flat */
            {
                float hfFloor = s_alGrenadeBloomHFFloor
                                ? s_alGrenadeBloomHFFloor->value : 0.05f;
                if (hfFloor < 0.0f) hfFloor = 0.0f;
                if (hfFloor > 1.0f) hfFloor = 1.0f;
                s_al_grenadeHF = hfFloor + (1.0f - hfFloor) * (1.0f - t);
            }

            /* Optional mild gain duck (unchanged behaviour) */
            if (s_alGrenadeBloomDuck && s_alGrenadeBloomDuck->integer) {
                float dFloor = s_alGrenadeBloomDuckFloor
                               ? s_alGrenadeBloomDuckFloor->value : 0.82f;
                if (dFloor < 0.5f)  dFloor = 0.5f;
                if (dFloor > 0.98f) dFloor = 0.98f;
                masterGain *= (dFloor + (1.0f - dFloor) * (1.0f - t));
            }
        }
    }

    /* -----------------------------------------------------------------------
     * Suppression: incoming-fire / head-hit hearing disruption.
     * masterGain duck is preserved as the "shock" component.
     * The per-source HF filter (s_al_suppressHF) is now the primary cue —
     * it replaces the "general duck" feel with convincing muffled hearing.
     * ----------------------------------------------------------------------- */

    /* Precompute cone half-cosine for directional per-source scaling. */
    {
        float coneAngle = s_alSuppressionConeAngle
                          ? s_alSuppressionConeAngle->value : 120.f;
        if (coneAngle < 10.f)  coneAngle = 10.f;
        if (coneAngle > 360.f) coneAngle = 360.f;
        s_al_suppress_cone_halfcos = cosf(coneAngle * 0.5f * ((float)M_PI / 180.0f));
    }

    if (s_al_suppressExpiry > 0) {
        int   now2  = Com_Milliseconds();
        int   durMs = s_alSuppressionMs ? (int)s_alSuppressionMs->value : 220;
        float t;
        if (durMs < 50) durMs = 50;
        if (now2 >= s_al_suppressExpiry) {
            s_al_suppressExpiry       = 0;
            s_al_suppressHFPeak       = 1.0f;
            s_al_suppress_directional = qfalse;
            t = 1.0f;
        } else {
            int remain = s_al_suppressExpiry - now2;
            t = (float)remain / (float)durMs;
            if (t > 1.f) t = 1.f;
        }

        /* Volume duck (secondary — kept for the physical "jolt" sensation) */
        if (!s_al_muted) {
            float volFloor = s_alSuppressionFloor
                             ? s_alSuppressionFloor->value : 0.45f;
            if (volFloor < 0.0f)  volFloor = 0.0f;
            if (volFloor > 0.95f) volFloor = 0.95f;
            masterGain *= (volFloor + (1.0f - volFloor) * (1.0f - t));
        }

        /* HF filter (primary — creates the muffled/disrupted hearing feel) */
        {
            float hfFloor = s_al_suppressHFPeak; /* set by the trigger */
            if (hfFloor < 0.0f) hfFloor = 0.0f;
            if (hfFloor > 1.0f) hfFloor = 1.0f;
            s_al_suppressHF = hfFloor + (1.0f - hfFloor) * (1.0f - t);
        }
    }

    /* -----------------------------------------------------------------------
     * Health-based HF fade (opt-in, s_alHealthFade 1).
     * Below s_alHealthFadeThreshold HP the world gradually grows muffled —
     * a subtle "fading away" effect that scales linearly with how close the
     * player is to death.  Never affects health above the threshold so it
     * has zero impact during normal gameplay.
     * ----------------------------------------------------------------------- */
    if (!s_al_muted && s_alHealthFade && s_alHealthFade->integer) {
        int   hp        = (int)cl.snap.ps.stats[STAT_HEALTH];
        int   threshold = s_alHealthFadeThreshold
                          ? (int)s_alHealthFadeThreshold->value : 30;
        if (threshold < 2)  threshold = 2; /* guard against div-by-zero below */
        if (hp < threshold && hp > 0) {
            float hfFloor = s_alHealthFadeFloor
                            ? s_alHealthFadeFloor->value : 0.35f;
            /* frac = 0 at threshold HP, = 1 at 1 HP.
             * (threshold - 1) >= 1 because threshold >= 2. */
            float frac = 1.0f - (float)(hp - 1) / (float)(threshold - 1);
            if (hfFloor < 0.0f) hfFloor = 0.0f;
            if (hfFloor > 1.0f) hfFloor = 1.0f;
            s_al_healthHF = hfFloor + (1.0f - hfFloor) * (1.0f - frac);
        }
    }

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

        /* Ambient normalisation diagnostics: one line per active loop source.
         * Helps identify which map sounds are outliers and verify the normcache
         * is having the intended effect.  Enable with s_alDebugNorm 1. */
        if (s_alDebugNorm && s_alDebugNorm->integer) {
            int j;
            for (j = 0; j < s_al_numSrc; j++) {
                const alSrc_t    *src = &s_al_src[j];
                const alSfxRec_t *r;
                if (!src->isPlaying || !src->loopSound) continue;
                if (src->sfx < 0 || src->sfx >= s_al_numSfx) continue;
                r = &s_al_sfx[src->sfx];
                Com_Printf("[alNorm] ent=%4d  %-48s  normGain=%.4f\n",
                    src->entnum, r->name, r->normGain);
            }
        }

        /* Fire-impact reverb in static mode (s_alDynamicReverb 0).
         * When the dynamic probe is active it handles fire boosts internally;
         * this call is a no-op in that case (guarded inside the function). */
        if (!s_alDynamicReverb || !s_alDynamicReverb->integer)
            S_AL_UpdateStaticFireBoost();

        /* Grenade-concussion EFX bloom: scan snapshot entities once per frame.
         *
         * Design goal — competitive-safe:
         *   The effect boosts the reverb slot gain briefly (making the room sound
         *   momentarily "bigger/boomer") then decays.  It does NOT reduce listener
         *   gain, so weapon fire, footsteps and callouts remain at full clarity.
         *   Default off; useful as a subtle immersion layer for non-competitive play.
         *
         * Detection:
         *   Iterate cl.snap.entities, looking for ET_MISSILE entities that are
         *   carrying an explosion event (EV_MISSILE_HIT or EV_MISSILE_MISS*) within
         *   s_alGrenadeBloomRadius units of the listener.  Team attribution uses the
         *   same configstring path as the near-miss suppression: the entity's
         *   clientNum maps to CS_PLAYERS + clientNum → "t" field.  Only enemy
         *   grenades trigger the bloom — teammate grenades are silently ignored. */
        if (s_alGrenadeBloom && s_alGrenadeBloom->integer
                && s_al_efx.reverbSlot
                && s_alReverb && s_alReverb->integer) {
            float  bRadius = s_alGrenadeBloomRadius
                             ? s_alGrenadeBloomRadius->value : 400.f;
            float  bGain   = s_alGrenadeBloomGain
                             ? s_alGrenadeBloomGain->value   : 0.12f;
            int    bMs     = s_alGrenadeBloomMs
                             ? (int)s_alGrenadeBloomMs->value : 180;
            int    ourTeam = (int)cl.snap.ps.persistant[PERS_TEAM];
            int    k;

            if (bGain  > 0.3f) bGain  = 0.3f;
            if (bGain  < 0.0f) bGain  = 0.0f;
            if (bMs    < 1)    bMs    = 1;
            if (bRadius < 1.f) bRadius = 1.f;

            for (k = 0; k < cl.snap.numEntities; k++) {
                entityState_t *es;
                int            ev;
                vec3_t         d;
                es = &cl.parseEntities[
                    (cl.snap.parseEntitiesNum + k) & (MAX_PARSE_ENTITIES - 1)];

                /* Only care about missile entities */
                if (es->eType != ET_MISSILE) continue;

                /* Only explosion events */
                ev = es->event & ~EV_EVENT_BITS;
                if (ev != EV_MISSILE_HIT && ev != EV_MISSILE_MISS
                        && ev != EV_MISSILE_MISS_METAL) continue;

                /* Distance check */
                VectorSubtract(es->pos.trBase, s_al_listener_origin, d);
                if (DotProduct(d, d) > bRadius * bRadius) continue;

                /* Friendly-fire exclusion: same configstring path as suppression */
                if (ourTeam != TEAM_FREE
                        && es->clientNum >= 0 && es->clientNum < MAX_CLIENTS) {
                    const char *csStr = cl.gameState.stringData
                        + cl.gameState.stringOffsets[CS_PLAYERS + es->clientNum];
                    int throwerTeam = atoi(Info_ValueForKey(csStr, "t"));
                    if (throwerTeam == ourTeam) continue;   /* teammate — skip */
                }

                /* Trigger bloom: record current slot gain as the base to spike from */
                if (s_al_bloomExpiry == 0) {
                    /* Read current slot gain so we spike ABOVE it, not to an
                     * absolute value — prevents fighting with dynamic reverb. */
                    float curGain = s_alReverbGain ? s_alReverbGain->value : 0.35f;
                    s_al_bloomPeak   = curGain + bGain;
                    if (s_al_bloomPeak > 1.0f) s_al_bloomPeak = 1.0f;
                }
                /* Extend/restart the expiry on each additional hit this frame */
                s_al_bloomExpiry = Com_Milliseconds() + bMs;
                break;   /* one bloom trigger per frame is enough */
            }

            /* Drive the bloom decay: interpolate slot gain from peak back to
             * the normal target gain over the remaining duration.
             * Query the actual current slot gain rather than the cvar so the
             * base is correct even when s_alDynamicReverb has modified it. */
            if (s_al_bloomExpiry > 0) {
                int   now3     = Com_Milliseconds();
                float baseGain = 0.0f;
                float slotGain;
                qalGetAuxiliaryEffectSlotf(s_al_efx.reverbSlot,
                                           AL_EFFECTSLOT_GAIN, &baseGain);
                if (now3 >= s_al_bloomExpiry) {
                    s_al_bloomExpiry = 0;
                    slotGain = baseGain;
                } else {
                    int   remain = s_al_bloomExpiry - now3;
                    /* Reuse bMs already computed above for the trigger check */
                    float t = (bMs > 0) ? (float)remain / (float)bMs : 0.f;
                    if (t > 1.f) t = 1.f;
                    /* t=1 at trigger → peak gain; t=0 at expiry → base gain */
                    slotGain = baseGain + (s_al_bloomPeak - baseGain) * t;
                }
                qalAuxiliaryEffectSlotf(s_al_efx.reverbSlot,
                                        AL_EFFECTSLOT_GAIN, slotGain);
            }
        }

        /* Suppression reverb boost decay.
         * When incoming fire triggers suppression the reverb slot was spiked
         * in S_AL_TriggerSuppression; decay it back over the suppression
         * duration.  Grenade bloom takes priority if both are active. */
        if (s_al_suppressReverbExpiry > 0 && s_al_bloomExpiry == 0
                && s_alReverb && s_alReverb->integer) {
            int   now4  = Com_Milliseconds();
            int   durMs = s_alSuppressionMs ? (int)s_alSuppressionMs->value : 220;
            if (durMs < 1) durMs = 1;
            if (now4 >= s_al_suppressReverbExpiry) {
                s_al_suppressReverbExpiry = 0;
                s_al_suppressReverbPeak   = 0.f;
            } else {
                int   remain   = s_al_suppressReverbExpiry - now4;
                float t        = (float)remain / (float)durMs;
                float baseGain = s_alReverbGain ? s_alReverbGain->value : 0.35f;
                float slotGain;
                if (t > 1.f) t = 1.f;
                slotGain = baseGain + (s_al_suppressReverbPeak - baseGain) * t;
                if (slotGain > 1.f) slotGain = 1.f;
                qalAuxiliaryEffectSlotf(s_al_efx.reverbSlot,
                                        AL_EFFECTSLOT_GAIN, slotGain);
            }
        }


        /* Echo is audible only in enclosed spaces (low openFrac).
         * In open/outdoor environments the slot gain is held near-zero.
         * We don't have openFrac directly here, so approximate from the
         * current reverb decay: longer decay → more enclosed → more echo. */
        if (s_alEcho && s_alEcho->integer && s_al_efx.echoSlot) {
            static float lastEchoGain = -1.f;
            float echoGainTarget = 0.f;
            float echoGainMax = s_alEchoGain ? s_alEchoGain->value : 0.10f;
            if (echoGainMax < 0.f) echoGainMax = 0.f;
            if (echoGainMax > 0.3f) echoGainMax = 0.3f;
            if (!s_al_inwater
                    && s_alReverb && s_alReverb->integer
                    && s_alDynamicReverb && s_alDynamicReverb->integer) {
                /* Use current slot gain as a proxy for enclosure level */
                float slotGain = 0.f;
                float reverbGainRef = s_alReverbGain ? s_alReverbGain->value : 0.35f;
                if (reverbGainRef > 0.001f && qalGetAuxiliaryEffectSlotf) {
                    qalGetAuxiliaryEffectSlotf(s_al_efx.reverbSlot,
                                               AL_EFFECTSLOT_GAIN, &slotGain);
                    echoGainTarget = echoGainMax * slotGain / reverbGainRef;
                } else if (reverbGainRef > 0.001f) {
                    /* Fallback: no getter available — use a modest static fraction */
                    echoGainTarget = echoGainMax * 0.3f;
                }
                if (echoGainTarget > echoGainMax) echoGainTarget = echoGainMax;
                if (echoGainTarget < 0.f)         echoGainTarget = 0.f;
            }
            if (echoGainTarget != lastEchoGain) {
                qalAuxiliaryEffectSlotf(s_al_efx.echoSlot,
                                        AL_EFFECTSLOT_GAIN, echoGainTarget);
                lastEchoGain = echoGainTarget;
            }
        }

        /* Drive chorus slot gain: full when underwater, zero when not. */
        if (s_al_efx.chorusSlot) {
            static qboolean lastChorusActive = qfalse;
            qboolean chorusActive = (s_al_inwater && s_al_efx.chorusEffect != 0);
            if (chorusActive != lastChorusActive) {
                qalAuxiliaryEffectSlotf(s_al_efx.chorusSlot, AL_EFFECTSLOT_GAIN,
                                        chorusActive ? 0.25f : 0.0f);
                lastChorusActive = chorusActive;
            }
        }

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

        /* Live-update EQ parameters when cvars change at runtime. */
        if (s_al_efx.eqSend >= 0 && s_al_efx.eqEffect) {
            static float lastEqLow  = -1.f;
            static float lastEqMid  = -1.f;
            static float lastEqHigh = -1.f;
            static float lastEqGain = -1.f;
            float curLow  = s_alEqLow  ? s_alEqLow->value  : 1.26f;
            float curMid  = s_alEqMid  ? s_alEqMid->value  : 1.26f;
            float curHigh = s_alEqHigh ? s_alEqHigh->value : 1.0f;
            float curGain = s_alEqGain ? s_alEqGain->value : 1.0f;
            if (curLow != lastEqLow || curMid != lastEqMid ||
                curHigh != lastEqHigh || curGain != lastEqGain) {
                qalEffectf(s_al_efx.eqEffect, AL_EQUALIZER_LOW_GAIN,    curLow);
                qalEffectf(s_al_efx.eqEffect, AL_EQUALIZER_MID1_GAIN,   curMid);
                qalEffectf(s_al_efx.eqEffect, AL_EQUALIZER_MID2_GAIN,   curMid);
                qalEffectf(s_al_efx.eqEffect, AL_EQUALIZER_HIGH_GAIN,   curHigh);
                /* Re-commit the modified effect into the slot so OpenAL
                 * picks up the parameter changes immediately. */
                qalAuxiliaryEffectSloti(s_al_efx.eqSlot, AL_EFFECTSLOT_EFFECT,
                                        (ALint)s_al_efx.eqEffect);
                qalAuxiliaryEffectSlotf(s_al_efx.eqSlot, AL_EFFECTSLOT_GAIN,
                                        curGain);
                /* Recompute the normalization filter gain so it tracks the
                 * updated band gains and slot gain.  Per-source direct filters
                 * and EQ send filters are updated by the per-frame occlusion
                 * loop — only the global filter object gain changes here. */
                if (s_al_efx.eqNormFilter) {
                    s_al_efx.eqCompGain = 1.0f
                        / (1.0f + MAX(MAX(curLow, curMid), curHigh) * curGain);
                    qalFilterf(s_al_efx.eqNormFilter, AL_LOWPASS_GAIN,
                               s_al_efx.eqCompGain);
                }
                lastEqLow  = curLow;
                lastEqMid  = curMid;
                lastEqHigh = curHigh;
                lastEqGain = curGain;
            }
        }
    }

    /* Live-update per-category volume when any vol cvar changes.
     * Runs regardless of EFX availability so that own-player (local) source
     * gains respond to vol cvar changes even when s_alEFX=0.  When EFX is
     * off, Phase 3 of the occlusion block already writes gain every frame for
     * non-local sources, but local sources (isLocal=true) skip Phase 3 and
     * only get their gain set once in SrcSetup — without this block, changing
     * s_alVolSelf at runtime would have no effect on local sources.
     * NOTE: loop sources in the middle of a fade-in are NOT touched here;
     * S_AL_UpdateLoops always writes their gain last. */
    {
        static float lastVolSelf    = -1.f;
        static float lastVolOther   = -1.f;
        static float lastVolEnv     = -1.f;
        static float lastVolUI      = -1.f;
        static float lastVolWeapon  = -1.f;
        static float lastVolImpact  = -1.f;
        static float lastVolSupWpn  = -1.f;
        static float lastVolESupWpn = -1.f;
        static float lastVolExtraVol = -1.f;
        float vSelf     = S_AL_GetCategoryVol(SRC_CAT_LOCAL);
        float vOther    = S_AL_GetCategoryVol(SRC_CAT_WORLD);
        float vEnv      = S_AL_GetCategoryVol(SRC_CAT_AMBIENT);
        float vUI       = S_AL_GetCategoryVol(SRC_CAT_UI);
        float vWeapon   = S_AL_GetCategoryVol(SRC_CAT_WEAPON);
        float vImpact   = S_AL_GetCategoryVol(SRC_CAT_IMPACT);
        float vSupWpn   = S_AL_GetCategoryVol(SRC_CAT_WEAPON_SUPPRESSED);
        float vESupWpn  = S_AL_GetCategoryVol(SRC_CAT_WORLD_SUPPRESSED);
        float vExtraVol = S_AL_GetCategoryVol(SRC_CAT_EXTRAVOL);

        if (vSelf      != lastVolSelf     || vOther    != lastVolOther    ||
            vEnv       != lastVolEnv      || vUI       != lastVolUI       ||
            vWeapon    != lastVolWeapon   || vImpact   != lastVolImpact   ||
            vSupWpn    != lastVolSupWpn   || vESupWpn  != lastVolESupWpn  ||
            vExtraVol  != lastVolExtraVol) {
            int j;
            for (j = 0; j < s_al_numSrc; j++) {
                float catGain;
                if (!s_al_src[j].isPlaying) continue;
                /* Skip ALL loop sources — S_AL_UpdateLoops Step 5 runs after
                 * this block and is the authoritative gain writer for every
                 * active loop source (both mid-fade and settled).  Touching
                 * them here would overwrite a mid-fade ramp with the full
                 * target gain, popping the volume instead of ramping. */
                if (s_al_src[j].loopSound) continue;
                catGain = S_AL_GetCategoryVol(s_al_src[j].category);
                /* Re-apply per-sample normalisation for ambient sources */
                if (s_al_src[j].category == SRC_CAT_AMBIENT &&
                        s_al_src[j].sfx >= 0 && s_al_src[j].sfx < s_al_numSfx)
                    catGain *= s_al_sfx[s_al_src[j].sfx].normGain;
                /* Re-apply per-sample dB offset for extra-vol sources */
                if (s_al_src[j].category == SRC_CAT_EXTRAVOL)
                    catGain *= s_al_src[j].sampleVol;
                qalSourcef(s_al_src[j].source, AL_GAIN,
                    (s_al_src[j].master_vol / 255.f) * catGain);
            }
            lastVolSelf     = vSelf;
            lastVolOther    = vOther;
            lastVolEnv      = vEnv;
            lastVolUI       = vUI;
            lastVolWeapon   = vWeapon;
            lastVolImpact   = vImpact;
            lastVolSupWpn   = vSupWpn;
            lastVolESupWpn  = vESupWpn;
            lastVolExtraVol = vExtraVol;
        }
    }

    /* Reap stopped sources — mark them free immediately so the next
     * S_AL_GetFreeSource pass 1 finds them without growing or stealing.
     * Also runs occlusion for non-global BSP target_speaker loop sources —
     * those are always-on AL_LOOPING sources at fixed world positions that
     * should realistically be muffled through walls, just like one-shot
     * world sounds.  Generic (non-BSP) loop sources are still skipped. */
    for (i = 0; i < s_al_numSrc; i++) {
        if (!s_al_src[i].isPlaying) continue;
        /* Non-BSP loop sources: skip entirely (reap doesn't apply; occlusion
         * is not safe because AL_POSITION and AL_GAIN are owned by
         * S_AL_UpdateLoops which runs after this loop). */
        if (s_al_src[i].loopSound && !s_al_src[i].isBspSpeaker) continue;

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
                /* IIR-blend offset toward the new target instead of snapping.
                 * Direct assignment causes the apparent source direction to jump
                 * abruptly at every trace tick (every 4-8 frames for medium/far
                 * sources), which is audible as directional crackling with HRTF.
                 * Blending at step=0.35 matches the gain-opening speed so the
                 * position and gain fade together without pops.
                 *
                 * BSP speaker loop sources: skip the position redirect.
                 * S_AL_UpdateLoops writes AL_POSITION each frame from the
                 * entity origin; applying an acousticOffset here would race
                 * with that write and produce no net benefit.  The filter
                 * (gain + HF) is still applied — that is the primary cue. */
                if (!s_al_src[i].loopSound) {
                    float off0 = pBlend * (acousticPos[0] - s_al_src[i].origin[0]);
                    float off1 = pBlend * (acousticPos[1] - s_al_src[i].origin[1]);
                    float off2 = pBlend * (acousticPos[2] - s_al_src[i].origin[2]);
                    float offStep = 0.35f;
                    s_al_src[i].acousticOffset[0] +=
                        (off0 - s_al_src[i].acousticOffset[0]) * offStep;
                    s_al_src[i].acousticOffset[1] +=
                        (off1 - s_al_src[i].acousticOffset[1]) * offStep;
                    s_al_src[i].acousticOffset[2] +=
                        (off2 - s_al_src[i].acousticOffset[2]) * offStep;
                }
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

                /* Bake hearing-disruption HF multipliers into gainHF.
                 * For directional suppression (incoming fire) the HF cut is
                 * cone-weighted: full at the center of the fire cone, tapering
                 * to zero at the cone edge and in the side quadrants, with a
                 * secondary partial effect directly behind the listener.
                 * Omnidirectional events (head hits) apply globally. */
                {
                    float srcSuppressHF = s_al_suppressHF;
                    if (srcSuppressHF < 1.0f && s_al_suppress_directional) {
                        float dx   = pos[0] - s_al_listener_origin[0];
                        float dy   = pos[1] - s_al_listener_origin[1];
                        float dz   = pos[2] - s_al_listener_origin[2];
                        float dSq  = dx*dx + dy*dy + dz*dz;
                        /* Skip sources within 1 unit of the listener — at that
                         * distance the direction vector is numerically unreliable
                         * (almost always own-entity sounds that shouldn't be here). */
                        if (dSq > 1.f) {
                            float rcp      = 1.f / sqrtf(dSq);
                            float dot      = (dx * s_al_suppress_dir[0]
                                            + dy * s_al_suppress_dir[1]
                                            + dz * s_al_suppress_dir[2]) * rcp;
                            float halfCos  = s_al_suppress_cone_halfcos;
                            float rearGain = s_alSuppressionRearGain
                                             ? s_alSuppressionRearGain->value : 0.35f;
                            float cone_t;
                            if (dot >= halfCos) {
                                /* Inside forward cone: 0 at edge, 1 at centre */
                                cone_t = (halfCos < 0.9999f)
                                         ? (dot - halfCos) / (1.f - halfCos)
                                         : 1.f;
                            } else if (dot < 0.f) {
                                /* Behind listener: partial effect, peaks at dot=-1 */
                                cone_t = rearGain * (-dot);
                            } else {
                                /* Side quadrant: no suppression */
                                cone_t = 0.f;
                            }
                            if (cone_t > 1.f) cone_t = 1.f;
                            /* Blend toward 1.0 (flat) outside the effect zones */
                            srcSuppressHF = 1.0f
                                            + (s_al_suppressHF - 1.0f) * cone_t;
                        }
                    }
                    gainHF *= srcSuppressHF * s_al_grenadeHF * s_al_healthHF;
                }

                /* BSP speaker loop sources: skip the AL_POSITION write.
                 * S_AL_UpdateLoops writes the authoritative world position
                 * each frame (it runs after this loop); overwriting it here
                 * with the gap-redirected pos would race and gain nothing —
                 * acousticOffset is always kept zero for loop sources anyway. */
                if (!s_al_src[i].loopSound)
                    qalSource3f(s_al_src[i].source, AL_POSITION,
                                pos[0], pos[1], pos[2]);

                if (s_al_efx.available) {
                    if (occ > 0.98f && gainHF > 0.97f) {
                        /* Fully clear and no disruption: use the EQ normalization
                         * filter if EQ is active (to keep dry+wet at source level),
                         * otherwise bypass filter entirely to avoid phase-shift coloration.
                         * 0.98 / 0.97: small epsilon below 1.0 to absorb IIR
                         * rounding so we don't oscillate between NULL and filter
                         * on every frame when all values are nominally 1.0. */
                        if (s_al_efx.eqNormFilter && s_al_efx.eqSend >= 0)
                            qalSourcei(s_al_src[i].source, AL_DIRECT_FILTER,
                                       (ALint)s_al_efx.eqNormFilter);
                        else
                            qalSourcei(s_al_src[i].source, AL_DIRECT_FILTER,
                                       (ALint)AL_FILTER_NULL);
                    } else {
                        /* Multiply occlusion gain by EQ comp factor so the
                         * attenuated direct path stays in correct proportion
                         * with the normalized EQ wet send. */
                        float normGain = (s_al_efx.eqNormFilter && s_al_efx.eqSend >= 0)
                                         ? s_al_efx.eqCompGain : 1.0f;
                        qalFilterf(s_al_efx.occlusionFilter[i],
                                   AL_LOWPASS_GAIN,   gain * normGain);
                        qalFilterf(s_al_efx.occlusionFilter[i],
                                   AL_LOWPASS_GAINHF, gainHF);
                        qalSourcei(s_al_src[i].source, AL_DIRECT_FILTER,
                                   (ALint)s_al_efx.occlusionFilter[i]);
                    }
                } else if (!s_al_src[i].loopSound) {
                    /* No EFX: approximate occlusion by direct gain reduction.
                     * Do NOT multiply by masterGain here — the listener AL_GAIN
                     * already applies s_volume; including it again would double it.
                     * Loop sources (including BSP speaker loops) are skipped: the
                     * occlusion factor is folded into the gain by S_AL_UpdateLoops
                     * Step 5 via src->occlusionGain, which is maintained by the
                     * smoothing logic above. */
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
        } else if (s_al_efx.available) {
            /* Local (own-player) source: no occlusion, but apply the
             * hearing-disruption HF filter when active.  SRC_CAT_UI sources
             * (tinnitus ring, kill/hit markers) are intentionally excluded —
             * they are "mental" feedback that should stay crisp.
             * During directional suppression, own-player sounds get the rear
             * fraction of the effect — they have no world direction but the
             * concussion still partially affects the shooter's own hearing. */
            if (s_al_src[i].category != SRC_CAT_UI) {
                float localSuppressHF = s_al_suppressHF;
                if (localSuppressHF < 1.0f && s_al_suppress_directional) {
                    float rearGain = s_alSuppressionRearGain
                                     ? s_alSuppressionRearGain->value : 0.35f;
                    localSuppressHF = 1.0f
                                      + (s_al_suppressHF - 1.0f) * rearGain;
                }
                {
                    float combHF   = localSuppressHF * s_al_grenadeHF * s_al_healthHF;
                    float normGain = (s_al_efx.eqNormFilter && s_al_efx.eqSend >= 0)
                                     ? s_al_efx.eqCompGain : 1.0f;
                    /* Apply the filter when either condition requires attenuation.
                     * combHF < 0.97: HF suppression threshold (matches world source).
                     * normGain < 0.98: EQ compensation reduces direct path by > 2%;
                     *   use occlusionFilter rather than AL_FILTER_NULL so both
                     *   gain and gainHF can be set independently on the same object. */
                    if (combHF < 0.97f || normGain < 0.98f) {
                        qalFilterf(s_al_efx.occlusionFilter[i],
                                   AL_LOWPASS_GAIN,   normGain);
                        qalFilterf(s_al_efx.occlusionFilter[i],
                                   AL_LOWPASS_GAINHF, combHF);
                        qalSourcei(s_al_src[i].source, AL_DIRECT_FILTER,
                                   (ALint)s_al_efx.occlusionFilter[i]);
                    } else {
                        qalSourcei(s_al_src[i].source, AL_DIRECT_FILTER,
                                   (ALint)AL_FILTER_NULL);
                    }
                }
            }
        }

    }   /* end for each source */

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
    char mapbase[MAX_QPATH];

    s_al_muted = qfalse;

    /* Commit any in-flight loads from the previous map before resetting the
     * normcache.  Without this, a job queued for the old map might commit
     * after the new map's cache has been loaded and overwrite the normGain
     * that was just applied. */
    S_AL_FlushAllJobs();

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

    /* Load any existing per-map normcache now, before cgame registers its
     * sounds, so every subsequent RegisterSound call picks up the override
     * values immediately.  Also applies to sfx records still in memory from
     * a previous map session (FindSfx returns them early, bypassing the
     * CalcNormGain path — we fix those normGain values here instead). */
    s_al_normCacheCount   = 0;
    s_al_normCacheWritten = qfalse;
    S_AL_MapBaseName(mapbase, sizeof(mapbase));
    if (mapbase[0])
        S_AL_LoadMapNormCache(mapbase);
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
    Com_Printf("    s_alVolSelf   %.2f  (own movement/breath,        cvar ref 1.0 = gain %.2f)\n",
        s_alVolSelf   ? s_alVolSelf->value   : 1.0f, 1.00f);
    Com_Printf("    s_alVolWeapon %.2f  (own weapon fire,            cvar ref 1.0 = gain %.2f)\n",
        s_alVolWeapon ? s_alVolWeapon->value : 1.0f, 1.00f);
    Com_Printf("    s_alVolOther  %.2f  (other players/ents,         cvar ref 1.0 = gain %.2f, max 2)\n",
        s_alVolOther  ? s_alVolOther->value  : 1.0f, 0.70f);
    Com_Printf("    s_alVolImpact %.2f  (world impacts/brass/expl,   cvar ref 1.0 = gain %.2f, max 2)\n",
        s_alVolImpact ? s_alVolImpact->value : 1.0f, 0.55f);
    Com_Printf("    s_alVolEnv    %.2f  (ambient/looping,            cvar ref 1.0 = gain %.2f)\n",
        s_alVolEnv    ? s_alVolEnv->value    : 1.0f, 0.30f);
    Com_Printf("    s_alVolUI     %.2f  (hit/kill/UI sounds,         cvar ref 1.0 = gain %.2f)\n",
        s_alVolUI     ? s_alVolUI->value     : 1.0f, 0.80f);
    Com_Printf("    s_musicVolume %.2f  (map background music)\n",
        s_musicVolume ? s_musicVolume->value : 0.25f);
    Com_Printf("    s_alReverbGain    %.2f  (reverb wet level)\n",
        s_alReverbGain  ? s_alReverbGain->value  : 0.35f);
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
    /* Reverb is ON by default — gives URT maps clear acoustic character. */
    s_alReverb = Cvar_Get("s_alReverb", "1", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alReverb, "0", "1", CV_INTEGER);
    Cvar_SetDescription(s_alReverb, "Enable EFX environmental reverb (requires ALC_EXT_EFX). Default 1.");
    s_alReverbGain = Cvar_Get("s_alReverbGain", "0.35", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alReverbGain, "0", "1", CV_FLOAT);
    Cvar_SetDescription(s_alReverbGain, "EFX reverb auxiliary slot gain (wet level). Lower values reduce echo relative to direct sound. Acts as the ceiling when s_alDynamicReverb is on.");
    s_alReverbDecay = Cvar_Get("s_alReverbDecay", "1.49", CVAR_ARCHIVE_ND | CVAR_LATCH);
    Cvar_CheckRange(s_alReverbDecay, "0.1", "10.0", CV_FLOAT);
    Cvar_SetDescription(s_alReverbDecay, "EFX reverb decay time in seconds. Used when s_alDynamicReverb 0. Requires map reload (LATCH).");
    /* Dynamic reverb is ON by default. */
    s_alDynamicReverb = Cvar_Get("s_alDynamicReverb", "1", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alDynamicReverb, "0", "1", CV_INTEGER);
    Cvar_SetDescription(s_alDynamicReverb, "Ray-traced acoustic environment detection. Casts 14 probes from the listener every 16 frames to measure room size, corridor shape, and openness, then adjusts EFX reverb decay/reflections accordingly. Default 1.");
    s_alDebugReverb = Cvar_Get("s_alDebugReverb", "0", CVAR_TEMP);
    Cvar_CheckRange(s_alDebugReverb, "0", "2", CV_INTEGER);
    Cvar_SetDescription(s_alDebugReverb, "Dynamic reverb debug output. 1 = print env label + smoothed EFX params every probe cycle. 2 = also print raw probe metrics and filter-reset events. Default 0 (off). Not archived.");
    s_alEcho = Cvar_Get("s_alEcho", "1", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alEcho, "0", "1", CV_INTEGER);
    Cvar_SetDescription(s_alEcho,
        "Geometry-driven echo effect on EFX auxiliary send 1. "
        "When on, adds a discrete reflection layer to 3D sources in "
        "enclosed spaces. Slot gain is driven automatically by room size "
        "from s_alDynamicReverb. Default 1 (on).");
    s_alEchoGain = Cvar_Get("s_alEchoGain", "0.10", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alEchoGain, "0", "0.3", CV_FLOAT);
    Cvar_SetDescription(s_alEchoGain,
        "Maximum wet gain for the echo aux slot [0–0.3]. "
        "Only applies when s_alEcho 1. Actual gain is scaled by room enclosure. "
        "Default 0.10.");
    s_alFireImpactReverb = Cvar_Get("s_alFireImpactReverb", "1", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alFireImpactReverb, "0", "1", CV_INTEGER);
    Cvar_SetDescription(s_alFireImpactReverb,
        "Fire-direction impact reverb boost. When the local player fires a weapon, "
        "3 short rays are cast in the aim direction. A nearby wall within 400 units "
        "transiently boosts early reflections and decay in the EFX reverb — simulating "
        "the sharp acoustic echo of a muzzle blast off a hard surface. Requires "
        "s_alDynamicReverb 1. Default 1 (on).");
    s_alFireImpactMaxBoost = Cvar_Get("s_alFireImpactMaxBoost", "0.25", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alFireImpactMaxBoost, "0", "0.5", CV_FLOAT);
    Cvar_SetDescription(s_alFireImpactMaxBoost,
        "Maximum reflections gain added by the fire-direction impact boost [0–0.5]. "
        "Only applies when s_alFireImpactReverb 1. Default 0.25.");
    s_alSuppression = Cvar_Get("s_alSuppression", "0", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alSuppression, "0", "1", CV_INTEGER);
    Cvar_SetDescription(s_alSuppression,
        "Master toggle for near-miss and incoming-fire hearing disruption. When 1, enables: "
        "(A) near-miss HF muffling when whiz1/whiz2 sounds play, "
        "(B) additional HF muffling from nearby enemy CHAN_WEAPON fire. "
        "Effects apply a per-source AL_LOWPASS_GAINHF cut that creates a "
        "convincing muffled/concussed hearing feel rather than a simple volume duck. "
        "Teammates and suppressed weapons are excluded automatically. "
        "Head-hit disruption (helmet/bare-head) has its own toggle: s_alHeadHit. "
        "Default 0 (opt-in).");
    s_alSuppressionRadius = Cvar_Get("s_alSuppressionRadius", "180", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alSuppressionRadius, "50", "400", CV_FLOAT);
    Cvar_SetDescription(s_alSuppressionRadius,
        "Fallback suppression trigger: radius (game units) within which an enemy "
        "CHAN_WEAPON sound triggers the disruption effect. The primary trigger is "
        "the whiz-sound name match (s_alNearMissPattern), which fires reliably for "
        "any bullet that actually passes close by. Default 180 ≈ one room width.");
    s_alSuppressionFloor = Cvar_Get("s_alSuppressionFloor", "0.45", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alSuppressionFloor, "0", "0.95", CV_FLOAT);
    Cvar_SetDescription(s_alSuppressionFloor,
        "Minimum listener gain (volume) during suppression [0–0.95]. "
        "Secondary to the HF filter — this provides the physical 'jolt' while "
        "s_alSuppressionHFFloor provides the muffled-hearing character. "
        "Default 0.45 (≈ −7 dB).");
    s_alSuppressionMs = Cvar_Get("s_alSuppressionMs", "220", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alSuppressionMs, "50", "800", CV_INTEGER);
    Cvar_SetDescription(s_alSuppressionMs,
        "Duration of the near-miss / incoming-fire hearing disruption in ms. "
        "Both the volume duck and the HF muffling recover linearly over this time. "
        "Default 220 ms — snappy enough to not impede situational awareness.");
    s_alSuppressionHFFloor = Cvar_Get("s_alSuppressionHFFloor", "0.08", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alSuppressionHFFloor, "0", "1", CV_FLOAT);
    Cvar_SetDescription(s_alSuppressionHFFloor,
        "Minimum AL_LOWPASS_GAINHF during near-miss / incoming-fire suppression [0–1]. "
        "Primary cue: 0.08 ≈ −22 dB HF at peak — sounds in the fire direction go "
        "noticeably bassy/distorted. Applies only within the directional cone "
        "(s_alSuppressionConeAngle) plus a rear partial effect (s_alSuppressionRearGain). "
        "0 = fully muffled; 1 = flat. Default 0.08.");
    s_alSuppressionConeAngle = Cvar_Get("s_alSuppressionConeAngle", "120", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alSuppressionConeAngle, "10", "360", CV_FLOAT);
    Cvar_SetDescription(s_alSuppressionConeAngle,
        "Full cone angle (degrees) of the directional HF suppression effect [10–360]. "
        "Sources within this cone around the incoming fire direction receive the full "
        "HF cut; sources outside the cone are unaffected (except the rear partial). "
        "120 = ±60° half-angle — covers a generous forward wedge. "
        "360 = omnidirectional (equivalent to old behaviour). Default 120.");
    s_alSuppressionRearGain = Cvar_Get("s_alSuppressionRearGain", "0.35", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alSuppressionRearGain, "0", "1", CV_FLOAT);
    Cvar_SetDescription(s_alSuppressionRearGain,
        "Fraction of the HF suppression applied to sources directly behind the listener "
        "(opposite the incoming fire) [0–1]. Creates a secondary disruption cue that "
        "reinforces the fire direction without muffling side-facing sounds. "
        "0 = no rear effect (strict cone only). Default 0.35.");
    s_alSuppressionReverbBoost = Cvar_Get("s_alSuppressionReverbBoost", "0.18", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alSuppressionReverbBoost, "0", "0.5", CV_FLOAT);
    Cvar_SetDescription(s_alSuppressionReverbBoost,
        "Reverb slot gain spike added when suppression triggers [0–0.5]. "
        "Briefly boosts the wet reverb tail so the acoustic character of the "
        "environment 'splashes' with the concussion, then decays back over "
        "s_alSuppressionMs. Stacks with dynamic reverb — actual peak is "
        "current slot gain + this value, capped at 1.0. Default 0.18.");
    s_alNearMissPattern = Cvar_Get("s_alNearMissPattern", "whiz1,whiz2", CVAR_ARCHIVE_ND);
    Cvar_SetDescription(s_alNearMissPattern,
        "Comma-separated substrings matched case-insensitively against the sound "
        "file name to identify near-miss bullet whiz sounds. When a matching sound "
        "plays, the hearing-disruption effect fires immediately — this is the precise "
        "trigger path (vs the radius fallback). URT uses sound/weapons/whiz1.wav and "
        "whiz2.wav. Default 'whiz1,whiz2'. Empty = disable name-based trigger.");
    /* Head-hit triggers — standalone toggle, independent of s_alSuppression */
    s_alHeadHit = Cvar_Get("s_alHeadHit", "1", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alHeadHit, "0", "1", CV_INTEGER);
    Cvar_SetDescription(s_alHeadHit,
        "Enable head-hit hearing disruption + tinnitus independently of s_alSuppression. "
        "When 1, a CHAN_BODY sound matching s_alHelmetHitPattern or s_alBareHeadHitPattern "
        "on the local player entity triggers HF muffling and the tinnitus ring. "
        "s_alSuppression 1 also enables this feature; s_alHeadHit lets you enable "
        "head-hit feedback alone without the near-miss/incoming-fire system. Default 1.");
    s_alHelmetHitPattern = Cvar_Get("s_alHelmetHitPattern", "helmethit", CVAR_ARCHIVE_ND);
    Cvar_SetDescription(s_alHelmetHitPattern,
        "Comma-separated sound-name substrings for helmet-hit detection. "
        "When the local player's entity plays a CHAN_BODY sound matching this pattern, "
        "a hearing-disruption event fires (HF muffling + tinnitus ring). "
        "URT uses sound/helmethit.wav. Default 'helmethit'. "
        "Active when s_alHeadHit 1 or s_alSuppression 1.");
    s_alHelmetHitMs = Cvar_Get("s_alHelmetHitMs", "350", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alHelmetHitMs, "50", "1000", CV_INTEGER);
    Cvar_SetDescription(s_alHelmetHitMs,
        "Duration of hearing disruption after a helmet hit (ms). Longer than "
        "a near-miss (350 vs 220) to reflect the physical impact. Default 350.");
    s_alHelmetHFFloor = Cvar_Get("s_alHelmetHFFloor", "0.10", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alHelmetHFFloor, "0", "1", CV_FLOAT);
    Cvar_SetDescription(s_alHelmetHFFloor,
        "Minimum HF gain for the helmet-hit disruption [0–1]. "
        "Deeper cut than incoming fire (0.10 vs 0.15) — you were actually hit. "
        "Default 0.10 (≈ −20 dB HF at peak).");
    s_alBareHeadHitPattern = Cvar_Get("s_alBareHeadHitPattern", "headshot", CVAR_ARCHIVE_ND);
    Cvar_SetDescription(s_alBareHeadHitPattern,
        "Comma-separated sound-name substrings for bare-head (no helmet) hit detection. "
        "When matched on the local player entity CHAN_BODY, fires the strongest "
        "disruption tier. URT uses sound/headshot.wav for unprotected headshots. "
        "Default 'headshot'. Active when s_alHeadHit 1 or s_alSuppression 1.");
    s_alBareHeadHitMs = Cvar_Get("s_alBareHeadHitMs", "500", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alBareHeadHitMs, "50", "1500", CV_INTEGER);
    Cvar_SetDescription(s_alBareHeadHitMs,
        "Duration of hearing disruption after a bare-head hit (ms). "
        "Longest disruption tier — in URT a bare headshot is usually fatal, "
        "so 500 ms represents the last moments of awareness. Default 500.");
    s_alBareHeadHFFloor = Cvar_Get("s_alBareHeadHFFloor", "0.03", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alBareHeadHFFloor, "0", "1", CV_FLOAT);
    Cvar_SetDescription(s_alBareHeadHFFloor,
        "Minimum HF gain for the bare-head hit disruption [0–1]. "
        "Most severe cut: 0.03 ≈ −30 dB HF, nearly bass-only at peak. "
        "Default 0.03.");
    /* Tinnitus */
    s_alTinnitusFreq = Cvar_Get("s_alTinnitusFreq", "3500", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alTinnitusFreq, "200", "8000", CV_INTEGER);
    Cvar_SetDescription(s_alTinnitusFreq,
        "Frequency of the synthesised tinnitus tone in Hz [200–8000]. "
        "3500 Hz sits in the most sensitive region of human hearing — clearly "
        "audible without being ear-piercing. Change takes effect on next "
        "snd_restart or first play after change. Default 3500.");
    s_alTinnitusDuration = Cvar_Get("s_alTinnitusDuration", "700", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alTinnitusDuration, "50", "3000", CV_INTEGER);
    Cvar_SetDescription(s_alTinnitusDuration,
        "Duration of the synthesised tinnitus ring in ms [50–3000]. "
        "The tone has a 20 ms linear attack then a quadratic decay to silence. "
        "Change takes effect on next snd_restart or first play after change. "
        "Default 700 ms.");
    s_alTinnitusVol = Cvar_Get("s_alTinnitusVol", "0.45", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alTinnitusVol, "0", "1", CV_FLOAT);
    Cvar_SetDescription(s_alTinnitusVol,
        "Volume of the synthesised tinnitus ring [0–1]. Applied directly as "
        "the OpenAL source AL_GAIN so it is independent of s_alVolUI and other "
        "category knobs. 0 = silent (disables tinnitus without changing other "
        "disruption effects). Default 0.45.");
    s_alTinnitusCooldown = Cvar_Get("s_alTinnitusCooldown", "800", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alTinnitusCooldown, "0", "5000", CV_INTEGER);
    Cvar_SetDescription(s_alTinnitusCooldown,
        "Minimum gap between successive tinnitus plays in ms. Prevents rapid "
        "helmet hits from stacking many simultaneous ring sources. Default 800.");
    /* Health-based fade */
    s_alHealthFade = Cvar_Get("s_alHealthFade", "0", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alHealthFade, "0", "1", CV_INTEGER);
    Cvar_SetDescription(s_alHealthFade,
        "Health-based HF audio fade (opt-in). When enabled, the per-source "
        "HF filter gradually reduces below s_alHealthFadeThreshold HP — "
        "the world grows muffled as the player nears death. Zero effect at "
        "or above the threshold so normal gameplay is unaffected. Default 0.");
    s_alHealthFadeThreshold = Cvar_Get("s_alHealthFadeThreshold", "10", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alHealthFadeThreshold, "5", "100", CV_INTEGER);
    Cvar_SetDescription(s_alHealthFadeThreshold,
        "HP level below which the health-based HF fade activates [5–100]. "
        "At exactly this HP the filter is flat; at 1 HP it reaches "
        "s_alHealthFadeFloor. Default 30.");
    s_alHealthFadeFloor = Cvar_Get("s_alHealthFadeFloor", "0.35", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alHealthFadeFloor, "0", "1", CV_FLOAT);
    Cvar_SetDescription(s_alHealthFadeFloor,
        "Minimum HF gain at 1 HP when health-fade is active [0–1]. "
        "0.35 ≈ −9 dB HF at death's door — noticeably muffled but footsteps "
        "and shots remain identifiable. Default 0.35.");
    s_alGrenadeBloom = Cvar_Get("s_alGrenadeBloom", "1", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alGrenadeBloom, "0", "1", CV_INTEGER);
    Cvar_SetDescription(s_alGrenadeBloom,
        "Grenade-blast concussion effect. Combines two components: "
        "(1) EFX reverb slot gain spike (reverb bloom) that makes the room "
        "sound momentarily bigger/boomer; "
        "(2) per-source HF lowpass cut (s_alGrenadeBloomHFFloor) that turns "
        "'volume went down' into 'something just exploded 15 feet from me'. "
        "Enemy grenades only — teammate grenades excluded via configstring check. "
        "Requires s_alReverb 1 for the reverb component. Default 0 (opt-in).");
    s_alGrenadeBloomRadius = Cvar_Get("s_alGrenadeBloomRadius", "400", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alGrenadeBloomRadius, "50", "1200", CV_FLOAT);
    Cvar_SetDescription(s_alGrenadeBloomRadius,
        "Blast radius (game units) within which a grenade explosion triggers the "
        "bloom + HF effect. Default 400 ≈ medium room width.");
    s_alGrenadeBloomGain = Cvar_Get("s_alGrenadeBloomGain", "0.12", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alGrenadeBloomGain, "0", "0.3", CV_FLOAT);
    Cvar_SetDescription(s_alGrenadeBloomGain,
        "Peak reverb slot gain boost added by the grenade bloom [0–0.3]. "
        "Stacks on top of the current slot gain — actual peak = base + this value. "
        "Default 0.12 (subtle but audible reverb surge).");
    s_alGrenadeBloomMs = Cvar_Get("s_alGrenadeBloomMs", "350", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alGrenadeBloomMs, "50", "1000", CV_INTEGER);
    Cvar_SetDescription(s_alGrenadeBloomMs,
        "Duration of both the reverb bloom decay and HF muffling recovery in ms. "
        "Raised from 180 to 350 to give the blast a more convincing physical weight. "
        "Default 350.");
    s_alGrenadeBloomHFFloor = Cvar_Get("s_alGrenadeBloomHFFloor", "0.05", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alGrenadeBloomHFFloor, "0", "1", CV_FLOAT);
    Cvar_SetDescription(s_alGrenadeBloomHFFloor,
        "Minimum HF gain at the peak of a grenade-blast concussion [0–1]. "
        "0.05 ≈ −26 dB HF — an explosion 15 feet away strips nearly all high "
        "frequencies momentarily. Applies per-source (own weapon, footsteps, "
        "enemy fire all go bassy). Recovers linearly over s_alGrenadeBloomMs. "
        "Default 0.05. Requires s_alGrenadeBloom 1.");
    s_alGrenadeBloomDuck = Cvar_Get("s_alGrenadeBloomDuck", "0", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alGrenadeBloomDuck, "0", "1", CV_INTEGER);
    Cvar_SetDescription(s_alGrenadeBloomDuck,
        "Add a mild listener-gain duck alongside the grenade reverb bloom. "
        "URT grenades are already loud enough that they produce natural perceptual "
        "ducking — this is a very subtle optional supplement. Hard-floored at 0.5 "
        "so gameplay-critical audio (footsteps, shots) is never silenced. "
        "Requires s_alGrenadeBloom 1. Default 0 (opt-in).");
    s_alGrenadeBloomDuckFloor = Cvar_Get("s_alGrenadeBloomDuckFloor", "0.82", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alGrenadeBloomDuckFloor, "0.5", "0.95", CV_FLOAT);
    Cvar_SetDescription(s_alGrenadeBloomDuckFloor,
        "Minimum listener gain during the optional grenade duck [0.5–0.95]. "
        "0.82 ≈ −1.7 dB — barely perceptible but adds physical impact. "
        "Hard minimum 0.5 for competitive safety. Default 0.82.");
    s_alSuppressedSoundPattern = Cvar_Get("s_alSuppressedSoundPattern",
                                          "silenced,-sil,_sil,_sd_,_sd.,suppressed,suppressor",
                                          CVAR_ARCHIVE_ND);
    Cvar_SetDescription(s_alSuppressedSoundPattern,
        "Comma-separated substrings matched (case-insensitive) against the sound "
        "file name — used as FALLBACK when the gear-string check is unavailable. "
        "Primary detection reads the 'g' key from the player's CS_PLAYERS configstring; "
        "if the gear string contains 'U', the suppressor is equipped regardless of filename. "
        "URT suppressed file names from pk3 audit: de_sil, m4_sil, ak103_sil, beretta_sil, "
        "glock_sil, g36_sil, psg1_sil, ump45_sil, lr_sil (_sil token), "
        "mac11-sil (-sil token), p90_silenced (silenced token). "
        "Matched sounds: (1) use s_alVolSuppressedWeapon volume, "
        "(2) skip near-miss suppression duck, "
        "(3) skip incoming-fire reverb boost. "
        "Empty = all unsuppressed.");
    s_alVolSuppressedWeapon = Cvar_Get("s_alVolSuppressedWeapon", "1.0", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alVolSuppressedWeapon, "0", "10.0", CV_FLOAT);
    Cvar_SetDescription(s_alVolSuppressedWeapon,
        "Own suppressed weapon fire volume [0–10, ref 0.55]. "
        "Reference gain 0.55 reflects that suppressed weapons are inherently quieter. "
        "Below 1.0 uses power-2 curve for perceptual linearity. Default 1.0.");
    s_alVolEnemySuppressedWeapon = Cvar_Get("s_alVolEnemySuppressedWeapon", "1.0", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alVolEnemySuppressedWeapon, "0", "2.0", CV_FLOAT);
    Cvar_SetDescription(s_alVolEnemySuppressedWeapon,
        "Enemy / other-player suppressed weapon fire volume [0–2, ref 0.45]. "
        "Suppressed fire is by design harder to locate — the 0.45 reference gain "
        "reflects that inherent quietness. Capped at 2× (anti-cheat). "
        "Default 1.0 (reference level). Reduce to make suppressed enemies even "
        "harder to detect; raise to compensate if suppressors feel inaudible.");
    s_alSuppressedEnemyRangeMax = Cvar_Get("s_alSuppressedEnemyRangeMax", "400", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alSuppressedEnemyRangeMax, "0", "1330", CV_FLOAT);
    Cvar_SetDescription(s_alSuppressedEnemyRangeMax,
        "Distance (game units) within which enemy suppressed weapon volume scales up "
        "toward normal enemy weapon level. At 0 units the volume equals the normal "
        "enemy reference (ref 0.70); at this distance and beyond it stays at the "
        "suppressed floor (ref 0.45). Set to 0 to disable range scaling entirely. "
        "Default 400.");
    /* Extra-vol slots — one cvar per entry, always written to config (CVAR_ARCHIVE,
     * no CVAR_NODEFAULT) so every slot appears in the config file for easy editing.
     * Each slot holds one pattern: optional "!" prefix to exclude, optional ":-N"
     * dB suffix for a per-sample cut (N ≤ 0, floor −40 dB, fractional OK).
     * Empty slots are silently skipped during matching.
     * Slot 1 and 2 default to the two disproportionately loud URT feedback sounds
     * with a −0.4 dB cut each. */
    {
        static const char * const slotDesc =
            "One sound path pattern for the s_alExtraVol volume group "
            "(case-insensitive substring match against the sound file path). "
            "Syntax: \"path/pattern\" to include, \"!path/pattern\" to exclude, "
            "\"path/pattern:-N\" to include with an extra N dB cut (N ≤ 0; "
            "fractional values accepted; floor −40 dB). "
            "Empty = slot unused. "
            "All eight slots are evaluated together: a sound is matched when "
            "at least one positive slot matches and no exclusion slot matches.";
        static const char * const slotDefaults[S_AL_EXTRAVOL_SLOTS] = {
            "sound/feedback/hit.wav:-0.4",
            "sound/feedback/kill.wav:-0.4",
            "", "", "", "", "", ""
        };
        int si;
        for (si = 0; si < S_AL_EXTRAVOL_SLOTS; si++) {
            char cvName[32];
            Com_sprintf(cvName, sizeof(cvName), "s_alExtraVolEntry%d", si + 1);
            s_alExtraVolEntry[si] = Cvar_Get(cvName, slotDefaults[si], CVAR_ARCHIVE);
            Cvar_SetDescription(s_alExtraVolEntry[si], slotDesc);
        }
    }
    s_alExtraVol = Cvar_Get("s_alExtraVol", "1.0", CVAR_ARCHIVE);
    Cvar_CheckRange(s_alExtraVol, "0.25", "1.0", CV_FLOAT);
    Cvar_SetDescription(s_alExtraVol,
        "Global volume scalar for all sounds matched by the s_alExtraVolEntryN slots "
        "[0.25–1.0]. "
        "cvar=1.0 (default) applies no additional reduction beyond the per-slot "
        "\":-N\" dB cuts. "
        "Reduce below 1.0 to cut all matched sounds further (power-2 curve; "
        "0.5 ≈ −12 dB). "
        "Floored at 0.25 so matched sounds are never completely silenced. "
        "Default 1.0.");
    s_alOcclusion = Cvar_Get("s_alOcclusion", "1", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alOcclusion, "0", "1", CV_INTEGER);
    Cvar_SetDescription(s_alOcclusion, "Per-source occlusion tracing. Sounds behind walls are gain-attenuated and their apparent HRTF direction is corrected to the nearest audible gap. Close sources (<300 u) update every frame; distant sources update less often. Default 1.");
    /* Volume group controls — 0–10 scale, reference unity at 1.0.
     * WORLD and IMPACT are capped at 2.0 (anti-cheat: cannot amplify other
     * players or world impacts beyond 2× reference).
     * Reference gains are baked into S_AL_GetCategoryVol so that cvar=1.0
     * reproduces the same loudness as the old hardcoded defaults. */
    s_alVolSelf = Cvar_Get("s_alVolSelf", "1.0", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alVolSelf, "0", "10.0", CV_FLOAT);
    Cvar_SetDescription(s_alVolSelf,
        "Own player movement / breath / general volume [0–10, ref 1.0]. "
        "Below 1.0 uses a power-2 curve (0.5 ≈ −12 dB). Default 1.0.");
    s_alVolOther = Cvar_Get("s_alVolOther", "1.0", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alVolOther, "0", "2.0", CV_FLOAT);
    Cvar_SetDescription(s_alVolOther,
        "Other player / entity one-shot volume [0–2, ref 1.0 = applied gain 0.70]. "
        "Capped at 2.0 (anti-cheat). Below 1.0 uses power-2 curve. Default 1.0.");
    s_alVolEnv = Cvar_Get("s_alVolEnv", "1.0", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alVolEnv, "0", "10.0", CV_FLOAT);
    Cvar_SetDescription(s_alVolEnv,
        "Looping ambient / environmental sound volume [0–10, ref 1.0]. "
        "Per-sample RMS normalisation is applied automatically. "
        "Below 1.0 uses power-2 curve. Default 1.0.");
    s_alVolUI = Cvar_Get("s_alVolUI", "1.0", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alVolUI, "0", "10.0", CV_FLOAT);
    Cvar_SetDescription(s_alVolUI,
        "Hit-marker / kill-confirmation / UI sound volume [0–10, ref 1.0]. "
        "Controls non-positional local sounds independently of weapon volume. "
        "Below 1.0 uses power-2 curve. Default 1.0.");
    s_alVolWeapon = Cvar_Get("s_alVolWeapon", "1.0", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alVolWeapon, "0", "10.0", CV_FLOAT);
    Cvar_SetDescription(s_alVolWeapon,
        "Own weapon fire volume [0–10, ref 1.0]. "
        "Separated from s_alVolSelf so weapon loudness can be tuned without "
        "affecting footstep / breath / movement volume. "
        "Below 1.0 uses a power-2 curve (0.5 ≈ −12 dB). Default 1.0.");
    s_alVolImpact = Cvar_Get("s_alVolImpact", "1.0", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alVolImpact, "0", "2.0", CV_FLOAT);
    Cvar_SetDescription(s_alVolImpact,
        "World-entity impact sound volume (bullet hits, shell brass, explosions) [0–2, ref 1.0]. "
        "Capped at 2.0 (anti-cheat). Below 1.0 uses power-2 curve. Default 1.0.");
    s_alLocalSelf = Cvar_Get("s_alLocalSelf", "1", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alLocalSelf, "0", "1", CV_INTEGER);
    Cvar_SetDescription(s_alLocalSelf,
        "Force own-player sounds (footsteps, weapon, breath) to be non-spatialized "
        "(head-locked at the listener position) regardless of any world-space origin "
        "supplied by the game. "
        "Note: position-staleness artefacts caused by TR_LINEAR trajectory lag or "
        "sv_bufferMs position delay are now corrected automatically in the engine. "
        "Default 1 — own-player sounds are head-locked by default. "
        "Set to 0 to use full 3D spatialization for own-player sounds.");

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

    /* Loop-sound fade timing.  All live-tunable; type /s_alReset to hear changes. */
    s_alLoopFadeInMs = Cvar_Get("s_alLoopFadeInMs", "800", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alLoopFadeInMs, "0", "3000", CV_INTEGER);
    Cvar_SetDescription(s_alLoopFadeInMs,
        "Loop-sound fade-in duration in milliseconds when an ambient source first "
        "enters range (entity enters PVS). 0 = instant (no fade). Default 800.");
    s_alLoopFadeOutMs = Cvar_Get("s_alLoopFadeOutMs", "800", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alLoopFadeOutMs, "0", "3000", CV_INTEGER);
    Cvar_SetDescription(s_alLoopFadeOutMs,
        "Loop-sound fade-out duration in milliseconds when an ambient source leaves "
        "range (entity leaves PVS). 0 = instant cut. Default 800. "
        "The fade-out always starts from the source's current gain level — if the "
        "source was still fading in when it was removed, the fade-out begins from "
        "that partial level, never jumping to full volume first.");
    s_alLoopFadeDist = Cvar_Get("s_alLoopFadeDist", "400", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alLoopFadeDist, "0", "1000", CV_FLOAT);
    Cvar_SetDescription(s_alLoopFadeDist,
        "Distance zone (game units) inside the maximum audible range within which "
        "new loop sources start at a distance-proportional gain instead of silence. "
        "Eliminates the 'fade-in from zero' artefact for sources that appear while "
        "already audible: a source appearing at half the fade zone gets half gain "
        "immediately. 0 = always start from silence (old behaviour). Default 400.");

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
    s_alOccPosBlend = Cvar_Get("s_alOccPosBlend", "0.40", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alOccPosBlend, "0", "1", CV_FLOAT);
    Cvar_SetDescription(s_alOccPosBlend,
        "Occlusion: how far to redirect the HRTF apparent-source position towards the nearest "
        "acoustic gap when a source is blocked [0-1]. "
        "The gap direction is projected to the source's actual distance so even a modest "
        "value produces a clear angular shift: at 0.40 a doorway 45° off the direct line "
        "gives ~16° apparent HRTF direction change. "
        "0 = always use true source position (no gap hint); "
        "1 = full redirect to gap direction. "
        "Default 0.40.");
    s_alOccSearchRadius = Cvar_Get("s_alOccSearchRadius", "120", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alOccSearchRadius, "20", "400", CV_FLOAT);
    Cvar_SetDescription(s_alOccSearchRadius,
        "Occlusion: radius (game units) of the 8 tangent-plane probe rays used to find the "
        "nearest acoustic gap when a source is occluded. "
        "Default 120 (~URT standard door half-width). "
        "Increase to 160-200 if sounds behind wide doorways or around thick pillars still "
        "appear to come from the wrong direction; decrease to 60 for tighter gap sensitivity.");
    s_alOccAreaFloor = Cvar_Get("s_alOccAreaFloor", "0.30", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alOccAreaFloor, "0", "1", CV_FLOAT);
    Cvar_SetDescription(s_alOccAreaFloor,
        "Occlusion: minimum audibility for sounds in BSP-connected areas. "
        "When ray traces are fully blocked but the BSP area system reports the "
        "listener and source are connected through portals (corridors, tunnels), "
        "this value sets the minimum occlusion fraction, scaled by distance. "
        "0.30 means a nearby occluded-but-connected source stays at least 30% "
        "audible; the floor fades to zero at s_alMaxDist. "
        "0 = disable area floor (traces only). Default 0.30.");
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

    s_alDebugNorm = Cvar_Get("s_alDebugNorm", "0", CVAR_TEMP);
    Cvar_CheckRange(s_alDebugNorm, "0", "1", CV_INTEGER);
    Cvar_SetDescription(s_alDebugNorm,
        "Per-map ambient normalisation diagnostics [0/1]. "
        "When 1, prints one line per active looping ambient source each frame: "
        "entity number, sfx path, and the normGain being applied. "
        "Use on a specific map (e.g. ut4_turnpike) to identify which sounds are "
        "too loud and verify that the normcache is having the intended effect. "
        "Not archived.");

    /* Source pool size.  The pool starts at this size and grows dynamically
     * on demand up to S_AL_MAX_SRC.  Reduce if your audio driver struggles
     * (extremely rare on modern hardware).  Requires vid_restart (LATCH). */
    s_alMaxSrc = Cvar_Get("s_alMaxSrc", "512", CVAR_ARCHIVE_ND | CVAR_LATCH);
    Cvar_CheckRange(s_alMaxSrc, "16", va("%d", S_AL_MAX_SRC), CV_INTEGER);
    Cvar_SetDescription(s_alMaxSrc,
        "Maximum OpenAL source slots.  Default 512 — slots are cheap on modern "
        "hardware; reduce only if the audio driver reports resource errors.  "
        "Requires vid_restart (LATCH).");

    /* Dedup window — default 20 ms to match DMA's hardcoded per-entity window.
     * ENTITYNUM_WORLD always uses max(this, 100 ms) to match DMA's world throttle.
     * Non-weapon channels additionally extend the window to the sound duration
     * so trigger sounds cannot re-stack while the previous instance plays. */
    s_alDedupMs = Cvar_Get("s_alDedupMs", "20", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alDedupMs, "0", "10000", CV_INTEGER);
    Cvar_SetDescription(s_alDedupMs,
        "Same-entity same-sfx dedup window in milliseconds (default 20, matching the DMA backend). "
        "0 = disabled. "
        "Blocks a second S_StartSound for the same (entity, sfx) pair within this window. "
        "CHAN_WEAPON uses only this fixed window — rapid fire is never suppressed. "
        "All other channels additionally extend the window to the full sound duration so "
        "trigger-bound sounds (birds, ambient cues, map music) cannot re-stack while the "
        "previous instance is still playing, regardless of how quickly the trigger is re-tripped. "
        "ENTITYNUM_WORLD always uses max(s_alDedupMs, 100) to throttle impact/casing "
        "accumulation at sv_fps rate.");

    s_alTrace = Cvar_Get("s_alTrace", "0", CVAR_TEMP);
    Cvar_CheckRange(s_alTrace, "0", "2", CV_INTEGER);
    Cvar_SetDescription(s_alTrace,
        "OpenAL trace logging. "
        "0 = off (default). "
        "1 = key lifecycle events: source alloc/free, music open/close/loop, "
        "video audio drops, AL errors at critical call sites, init sequence. "
        "2 = verbose: every source setup, buffer queue/unqueue, and state change. "
        "Not archived. Set before starting the game to capture the init sequence.");

    s_alEqLow = Cvar_Get("s_alEqLow", "1.26", CVAR_ARCHIVE_ND);
    Cvar_SetDescription(s_alEqLow,
        "EQ low-shelf gain (linear). 1.0 = flat, 1.26 = +2 dB (default). "
        "Range 0.126 – 7.943 (±18 dB). The EQ uses level normalization: both "
        "the direct path and the EQ send are attenuated by 1/(1+max_band*slot_gain) "
        "so dry+wet stays at source level regardless of the band boost applied.");
    s_alEqMid = Cvar_Get("s_alEqMid", "1.26", CVAR_ARCHIVE_ND);
    Cvar_SetDescription(s_alEqMid,
        "EQ mid-band gain (linear). 1.0 = flat, 1.26 = +2 dB (default). "
        "Applied to both mid1 (800 Hz) and mid2 (2500 Hz) bands. "
        "See s_alEqLow for normalization details.");
    s_alEqHigh = Cvar_Get("s_alEqHigh", "1.0", CVAR_ARCHIVE_ND);
    Cvar_SetDescription(s_alEqHigh,
        "EQ high-shelf gain (linear). 1.0 = flat (default). "
        "Boost above 6 kHz — leave at 1.0 unless you want extra brightness. "
        "See s_alEqLow for normalization details.");
    s_alEqGain = Cvar_Get("s_alEqGain", "1.0", CVAR_ARCHIVE_ND);
    Cvar_SetDescription(s_alEqGain,
        "EQ effect slot wet-mix level [0..1]. 1.0 = full effect (default), "
        "0.0 = disabled. The normalization filter gain is recalculated automatically "
        "when this changes, so the total signal level stays consistent.");

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
    S_AL_InitKaiserLUT();

    /* Pick the highest-quality resampler offered by OpenAL Soft.
     * Priority: BSinc24 > BSinc12 > Cubic > (default).
     * This eliminates the tinny/thin quality of the default Linear
     * resampler on sounds whose sample rate differs from the device rate. */
    s_al_bestResampler = -1;
    if (qalGetStringiSOFT && qalIsExtensionPresent("AL_SOFT_source_resampler")) {
        ALint numResamplers = qalGetInteger(AL_NUM_RESAMPLERS_SOFT);
        ALint defResampler  = qalGetInteger(AL_DEFAULT_RESAMPLER_SOFT);
        ALint bestIdx = defResampler;
        int   bestRank = 0;   /* 1=Cubic, 2=BSinc12, 3=BSinc24 */
        int   ri;
        for (ri = 0; ri < numResamplers; ri++) {
            const char *name = qalGetStringiSOFT(AL_RESAMPLER_NAME_SOFT, ri);
            if (!name) continue;
            if (strstr(name, "BSinc24") || strstr(name, "bsinc24")) {
                if (bestRank < 3) { bestRank = 3; bestIdx = ri; }
            } else if (strstr(name, "BSinc12") || strstr(name, "bsinc12")) {
                if (bestRank < 2) { bestRank = 2; bestIdx = ri; }
            } else if (Q_stricmp(name, "Cubic") == 0) {
                if (bestRank < 1) { bestRank = 1; bestIdx = ri; }
            }
        }
        s_al_bestResampler = bestIdx;
        Com_Printf("S_AL: resampler: %s (index %d of %d, default was %d)\n",
            qalGetStringiSOFT(AL_RESAMPLER_NAME_SOFT, bestIdx),
            bestIdx, numResamplers, defResampler);
    }

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
        /* Route music through EQ for bass/mid presence.
         * Apply eqNormFilter on both the EQ send and the direct path so the
         * additive wet signal does not push total output above the source level. */
        if (s_al_efx.available && s_al_efx.eqSend >= 0 && s_al_efx.eqSlot) {
            qalSource3i(s_al_music.source, AL_AUXILIARY_SEND_FILTER,
                        (ALint)s_al_efx.eqSlot, s_al_efx.eqSend,
                        (ALint)(s_al_efx.eqNormFilter
                                ? s_al_efx.eqNormFilter
                                : AL_FILTER_NULL));
            if (s_al_efx.eqNormFilter)
                qalSourcei(s_al_music.source, AL_DIRECT_FILTER,
                           (ALint)s_al_efx.eqNormFilter);
        }
        if (s_al_bestResampler >= 0)
            qalSourcei(s_al_music.source, AL_SOURCE_RESAMPLER_SOFT,
                       s_al_bestResampler);
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

    /* Async loader thread pool.
     * Start workers now, before cgame registers any sounds, so the pool is
     * ready from the very first S_AL_RegisterSound call.
     * Default 4 threads — enough to parallelize all weapons+radio calls on a
     * quad-core.  Clamped to [1, 8] internally by S_AL_StartThreadPool. */
    s_alLoadThreads = Cvar_Get("s_alLoadThreads", "4", CVAR_ARCHIVE_ND | CVAR_LATCH);
    Cvar_CheckRange(s_alLoadThreads, "1", "8", CV_INTEGER);
    Cvar_SetDescription(s_alLoadThreads,
        "Number of background worker threads for async sound loading. "
        "Each worker runs CalcNormGain + ResamplePCM in parallel so the game "
        "thread is only stalled for the ~0.5 ms file-decode step per sound. "
        "Default 4. Range 1–8. Requires vid_restart (LATCH). "
        "Not supported on Windows (sync loading used instead).");
    S_AL_StartThreadPool(s_alLoadThreads->integer);

    Cmd_AddCommand("s_devices",              S_AL_ListDevicesCmd);
    Cmd_AddCommand("s_alReset",              S_AL_ReverbReset_f);
    Cmd_AddCommand("snd_normcache_rebuild",  S_AL_RebuildNormCache_f);

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
