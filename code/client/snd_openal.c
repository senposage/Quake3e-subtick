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
#define S_AL_MAX_SRC        MAX_CHANNELS        /* 96 active sources */
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

/* Per-sound-file data */
typedef struct alSfxRec_s {
    ALuint              buffer;                 /* AL buffer handle */
    char                name[MAX_QPATH];
    int                 soundLength;            /* in samples */
    qboolean            defaultSound;           /* buzz on load failure */
    qboolean            inMemory;
    int                 lastTimeUsed;           /* Com_Milliseconds() */
    struct alSfxRec_s  *next;                   /* hash chain */
} alSfxRec_t;

static alSfxRec_t   s_al_sfx[S_AL_MAX_SFX];
static int          s_al_numSfx   = 0;
static alSfxRec_t  *s_al_sfxHash[S_AL_SFX_HASHSIZE];

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
    float           master_vol;
    qboolean        isPlaying;
    qboolean        isLocal;        /* non-spatialized */
    float           occlusionGain;  /* [0..1] — 1.0 = unoccluded, 0.1 = fully blocked */
    int             occlusionTick;  /* s_al_loopFrame when last computed */
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
static cvar_t *s_alMaxDist;

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
        r->lastTimeUsed = Com_Milliseconds();
        return (sfxHandle_t)(r - s_al_sfx);
    }

    fmt = S_AL_Format(info.width, info.channels);

    /* B-format Ambisonics: 4-channel files (W+X+Y+Z) loaded as BFORMAT3D */
    if (info.channels == 4) {
        fmt = (info.width == 2) ? AL_FORMAT_BFORMAT3D_16 : AL_FORMAT_BFORMAT3D_8;
    }

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
    r->inMemory     = qtrue;
    r->lastTimeUsed = Com_Milliseconds();

    idx = (int)(r - s_al_sfx);
    Com_DPrintf("S_AL: loaded %s (%d Hz, %d ch, %d smp)\n",
        sample, info.rate, info.channels, info.samples);

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

/* Find an existing source for (entnum, entchannel), or -1. */
static float S_AL_GetMasterVol( void )
{
    return s_volume ? s_volume->value * 255.0f : 200.0f;
}

static int S_AL_FindSourceByChannel( int entnum, int entchannel )
{
    int i;
    for (i = 0; i < s_al_numSrc; i++) {
        if (!s_al_src[i].isPlaying) continue;
        if (s_al_src[i].entnum == entnum &&
            s_al_src[i].entchannel == entchannel &&
            entchannel != CHAN_AUTO)
            return i;
    }
    return -1;
}

/* Find a free source slot, stealing the oldest non-loop one if needed.
 * Returns source index or -1 on hard failure. */
static int S_AL_GetFreeSource( void )
{
    int i;
    int oldest      = Com_Milliseconds();
    int oldestIdx   = -1;

    for (i = 0; i < s_al_numSrc; i++) {
        if (!s_al_src[i].isPlaying)
            return i;
        if (!s_al_src[i].loopSound && s_al_src[i].allocTime < oldest) {
            oldest    = s_al_src[i].allocTime;
            oldestIdx = i;
        }
    }

    /* Steal oldest non-loop source */
    if (oldestIdx >= 0) {
        qalSourceStop(s_al_src[oldestIdx].source);
        s_al_src[oldestIdx].isPlaying = qfalse;
        return oldestIdx;
    }
    return -1;
}

/* Trace a ray from the listener to the source through solid BSP geometry.
 * Returns a gain multiplier: 1.0 = line-of-sight, 0.1 = fully occluded.
 * We include CONTENTS_PLAYERCLIP so that player-solid geometry (stairs,
 * clip brushes) also attenuates sound realistically. */
static float S_AL_ComputeOcclusion( const vec3_t srcOrigin )
{
    trace_t tr;
    vec3_t  listenerOrg;

    VectorCopy(s_al_listener_origin, listenerOrg);
    CM_BoxTrace(&tr, listenerOrg, srcOrigin,
                vec3_origin, vec3_origin,
                0, CONTENTS_SOLID | CONTENTS_PLAYERCLIP, qfalse);

    if (tr.fraction >= 1.0f)
        return 1.0f;           /* clear line of sight */

    /* Linear blend: solid wall = 0.1 (muffled but audible), partial = blend */
    return 0.1f + 0.9f * tr.fraction;
}

/* Configure a source for 3D or local playback. */
static void S_AL_SrcSetup( int idx, sfxHandle_t sfx,
                            const vec3_t origin, qboolean fixed_origin,
                            int entnum, int entchannel,
                            float vol, qboolean isLocal )
{
    alSfxRec_t *r  = &s_al_sfx[sfx];
    alSrc_t    *src = &s_al_src[idx];
    ALuint      sid = src->source;

    src->sfx          = sfx;
    src->entnum       = entnum;
    src->entchannel   = entchannel;
    src->master_vol   = vol;
    src->isLocal      = isLocal;
    src->loopSound    = qfalse;
    src->allocTime    = Com_Milliseconds();
    src->isPlaying    = qtrue;
    src->occlusionGain = 1.0f;
    src->occlusionTick = 0;

    if (origin)
        VectorCopy(origin, src->origin);
    else
        VectorClear(src->origin);
    src->fixed_origin = fixed_origin;

    qalSourcei(sid,  AL_BUFFER,   r->buffer);
    qalSourcef(sid,  AL_GAIN,     vol / 255.0f);
    qalSourcei(sid,  AL_LOOPING,  AL_FALSE);

    if (isLocal) {
        qalSourcei(sid, AL_SOURCE_RELATIVE, AL_TRUE);
        qalSource3f(sid, AL_POSITION, 0.0f, 0.0f, 0.0f);
        qalSourcef(sid, AL_ROLLOFF_FACTOR, 0.0f);
    } else {
        float maxDist = s_alMaxDist ? s_alMaxDist->value : S_AL_SOUND_MAXDIST;
        if (maxDist < S_AL_SOUND_MAXDIST) maxDist = S_AL_SOUND_MAXDIST;
        qalSourcei(sid, AL_SOURCE_RELATIVE, AL_FALSE);
        qalSource3f(sid, AL_POSITION, src->origin[0], src->origin[1], src->origin[2]);
        qalSourcef(sid, AL_ROLLOFF_FACTOR,      1.0f);
        qalSourcef(sid, AL_REFERENCE_DISTANCE,  S_AL_SOUND_FULLVOLUME);
        qalSourcef(sid, AL_MAX_DISTANCE,        maxDist);
        if (s_al_efx.available) {
            /* Reverb send (auxiliary send 0) */
            qalSource3i(sid, AL_AUXILIARY_SEND_FILTER,
                        (ALint)s_al_efx.reverbSlot, 0, (ALint)AL_FILTER_NULL);
            /* Occlusion direct filter (initially pass-through) */
            qalSourcei(sid, AL_DIRECT_FILTER,
                       (ALint)s_al_efx.occlusionFilter[idx]);
            qalSourcei(sid, AL_AUXILIARY_SEND_FILTER_GAIN_AUTO,   AL_TRUE);
            qalSourcei(sid, AL_AUXILIARY_SEND_FILTER_GAINHF_AUTO, AL_TRUE);
            qalSourcei(sid, AL_DIRECT_FILTER_GAINHF_AUTO,         AL_FALSE);
        }
    }
}

/* Stop a source and mark it free. */
static void S_AL_StopSource( int idx )
{
    if (s_al_src[idx].isPlaying) {
        qalSourceStop(s_al_src[idx].source);
        qalSourcei(s_al_src[idx].source, AL_BUFFER, 0);
        s_al_src[idx].isPlaying  = qfalse;
        s_al_src[idx].loopSound  = qfalse;
    }
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
        /* End of track — try to loop */
        S_CodecCloseStream(s_al_music.stream);
        s_al_music.stream = NULL;
        if (s_al_music.looping && s_al_music.loopFile[0]) {
            s_al_music.stream = S_CodecOpenStream(s_al_music.loopFile);
            if (!s_al_music.stream) return qfalse;
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
            s_al_music.active = qfalse;
            return;
        }
        qalSourceQueueBuffers(s_al_music.source, 1, &buf);
    }

    /* Restart if the source stalled (buffer underrun) */
    qalGetSourcei(s_al_music.source, AL_BUFFERS_QUEUED, &queued);
    qalGetSourcei(s_al_music.source, AL_SOURCE_STATE, &state);
    if (queued > 0 && state != AL_PLAYING && !s_al_music.paused)
        qalSourcePlay(s_al_music.source);
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
        /* Medium-sized indoor room — sensible default for URT maps */
        qalEffectf(s_al_efx.reverbEffect, AL_EAXREVERB_DENSITY,           0.5f);
        qalEffectf(s_al_efx.reverbEffect, AL_EAXREVERB_DIFFUSION,         0.85f);
        qalEffectf(s_al_efx.reverbEffect, AL_EAXREVERB_GAIN,              0.32f);
        qalEffectf(s_al_efx.reverbEffect, AL_EAXREVERB_GAINHF,            0.89f);
        qalEffectf(s_al_efx.reverbEffect, AL_EAXREVERB_GAINLF,            1.0f);
        qalEffectf(s_al_efx.reverbEffect, AL_EAXREVERB_DECAY_TIME,        1.49f);
        qalEffectf(s_al_efx.reverbEffect, AL_EAXREVERB_DECAY_HFRATIO,     0.83f);
        qalEffectf(s_al_efx.reverbEffect, AL_EAXREVERB_REFLECTIONS_GAIN,  0.05f);
        qalEffectf(s_al_efx.reverbEffect, AL_EAXREVERB_REFLECTIONS_DELAY, 0.007f);
        qalEffectf(s_al_efx.reverbEffect, AL_EAXREVERB_LATE_REVERB_GAIN,  1.26f);
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
        qalEffectf(s_al_efx.reverbEffect, AL_REVERB_DECAY_TIME, 1.49f);
        qalEffectf(s_al_efx.reverbEffect, AL_REVERB_REFLECTIONS_GAIN,  0.05f);
        qalEffectf(s_al_efx.reverbEffect, AL_REVERB_REFLECTIONS_DELAY, 0.007f);
        qalEffectf(s_al_efx.reverbEffect, AL_REVERB_LATE_REVERB_GAIN,  1.26f);
        qalEffectf(s_al_efx.reverbEffect, AL_REVERB_LATE_REVERB_DELAY, 0.011f);
    }
    qalGenAuxiliaryEffectSlots(1, &s_al_efx.reverbSlot);
    qalAuxiliaryEffectSloti(s_al_efx.reverbSlot, AL_EFFECTSLOT_EFFECT,
                            (ALint)s_al_efx.reverbEffect);

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

    /* Stop all sources */
    for (i = 0; i < s_al_numSrc; i++)
        S_AL_StopSource(i);

    /* Delete sources */
    for (i = 0; i < s_al_numSrc; i++) {
        if (s_al_src[i].source)
            qalDeleteSources(1, &s_al_src[i].source);
    }
    s_al_numSrc = 0;

    /* Music */
    S_AL_MusicStop();
    qalSourceStop(s_al_music.source);
    qalDeleteSources(1, &s_al_music.source);
    qalDeleteBuffers(S_AL_MUSIC_BUFFERS, s_al_music.buffers);

    /* Raw samples */
    if (s_al_raw.active) {
        qalSourceStop(s_al_raw.source);
        s_al_raw.active = qfalse;
    }
    qalDeleteSources(1, &s_al_raw.source);
    qalDeleteBuffers(S_AL_RAW_BUFFERS, s_al_raw.buffers);

    /* EFX cleanup */
    if (s_al_efx.available) {
        int j;
        qalDeleteAuxiliaryEffectSlots(1, &s_al_efx.reverbSlot);
        qalDeleteAuxiliaryEffectSlots(1, &s_al_efx.underwaterSlot);
        qalDeleteEffects(1, &s_al_efx.reverbEffect);
        qalDeleteEffects(1, &s_al_efx.underwaterEffect);
        for (j = 0; j < s_al_numSrc; j++)
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

    /* Reuse/override same (entnum, entchannel) if not CHAN_AUTO */
    if (entchannel != CHAN_AUTO) {
        srcIdx = S_AL_FindSourceByChannel(entnum, entchannel);
        if (srcIdx >= 0)
            S_AL_StopSource(srcIdx);
    }

    srcIdx = S_AL_GetFreeSource();
    if (srcIdx < 0) return;

    if (origin) {
        /* Caller supplied a world-space origin — use it as-is. */
        VectorCopy(origin, sndOrigin);
        isLocal = qfalse;
    } else if (entnum == s_al_listener_entnum) {
        /* Sound from the local player's own entity: non-spatialized so that
         * weapon fire, footsteps, etc. are heard at full volume regardless of
         * HRTF positioning.  This mirrors the base snd_dma.c behaviour where
         * listener-entity sounds are treated as local. */
        VectorClear(sndOrigin);
        isLocal = qtrue;
    } else {
        /* No origin given but it is a world entity: look up its last known
         * position from the per-entity cache (filled by UpdateEntityPosition
         * and Respatialize).  If the cache is still zero the sound will be
         * placed at the world origin; UpdateEntityPosition will correct the
         * AL source position on the very next frame. */
        if (entnum >= 0 && entnum < MAX_GENTITIES)
            VectorCopy(s_al_entity_origins[entnum], sndOrigin);
        else
            VectorClear(sndOrigin);
        isLocal = qfalse;
    }

    vol = S_AL_GetMasterVol();
    /* fixed_origin=true only when the caller supplied an explicit world-space
     * position.  Entity-cache positions (origin==NULL, non-listener entity)
     * use fixed_origin=false so that S_AL_UpdateEntityPosition can keep
     * moving the source as the entity moves each frame. */
    S_AL_SrcSetup(srcIdx, sfx, isLocal ? NULL : sndOrigin,
                  (origin != NULL), entnum, entchannel, vol, isLocal);

    r->lastTimeUsed = Com_Milliseconds();
    qalSourcePlay(s_al_src[srcIdx].source);
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
                  0, channelNum, vol, qtrue /* local */);
    r->lastTimeUsed = Com_Milliseconds();
    qalSourcePlay(s_al_src[srcIdx].source);
}

static void S_AL_StartBackgroundTrack( const char *intro, const char *loop )
{
    int      i;
    qboolean primed = qfalse;

    if (!s_al_started) return;

    S_AL_MusicStop();

    if (!intro || !*intro) return;

    s_al_music.stream = S_CodecOpenStream(intro);
    if (!s_al_music.stream) {
        Com_Printf(S_COLOR_YELLOW "S_AL: couldn't open music %s\n", intro);
        return;
    }

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
        primed = qtrue;
    }

    if (!primed) {
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

    fmt  = S_AL_Format(width, channels);
    size = samples * width * channels;

    /* Reclaim any AL buffers that the source has already finished playing.
     * In OpenAL, a buffer stays "attached" to the source queue (and cannot
     * be written to with alBufferData) until it is explicitly unqueued, even
     * after it has been played.  Without this drain step the round-robin pool
     * would attempt alBufferData on still-attached buffers after 8 calls and
     * silently fail, causing raw-samples audio (cinematics, VoIP) to stop. */
    qalGetSourcei(s_al_raw.source, AL_BUFFERS_PROCESSED, &processed);
    while (processed-- > 0) {
        ALuint tmp;
        qalSourceUnqueueBuffers(s_al_raw.source, 1, &tmp);
    }

    /* Grab one buffer from our small pool (round-robin). */
    buf = s_al_raw.buffers[s_al_raw.nextBuf];
    s_al_raw.nextBuf = (s_al_raw.nextBuf + 1) % S_AL_RAW_BUFFERS;

    qalBufferData(buf, fmt, data, size, rate);
    qalSourceQueueBuffers(s_al_raw.source, 1, &buf);

    qalSourcef(s_al_raw.source, AL_GAIN, volume);
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

/* Update looping sounds: start, stop, reposition. */
static void S_AL_UpdateLoops( void )
{
    int       i, srcIdx;
    alLoop_t *lp;

    for (i = 0; i < MAX_GENTITIES; i++) {
        lp = &s_al_loops[i];

        /* Kill sounds that were not renewed this frame */
        if (lp->active && (lp->kill || lp->framenum != s_al_loopFrame)) {
            lp->active = qfalse;
            lp->kill   = qfalse;
            if (lp->srcIdx >= 0 && lp->srcIdx < s_al_numSrc) {
                S_AL_StopSource(lp->srcIdx);
                s_al_src[lp->srcIdx].loopSound = qfalse;
            }
            lp->srcIdx = -1;
            continue;
        }

        if (!lp->active) continue;

        if (lp->sfx < 0 || lp->sfx >= s_al_numSfx) continue;
        if (!s_al_sfx[lp->sfx].buffer) continue;

        /* Start new loop source if needed */
        if (lp->srcIdx < 0 || !s_al_src[lp->srcIdx].isPlaying) {
            srcIdx = S_AL_GetFreeSource();
            if (srcIdx < 0) continue;
            float vol = S_AL_GetMasterVol();
            S_AL_SrcSetup(srcIdx, lp->sfx, lp->origin, qtrue,
                          i, CHAN_AUTO, vol, qfalse);
            s_al_src[srcIdx].loopSound = qtrue;
            qalSourcei(s_al_src[srcIdx].source, AL_LOOPING, AL_TRUE);
            qalSourcePlay(s_al_src[srcIdx].source);
            lp->srcIdx = srcIdx;
        } else {
            /* Update position */
            qalSource3f(s_al_src[lp->srcIdx].source, AL_POSITION,
                        lp->origin[0], lp->origin[1], lp->origin[2]);
            qalSource3f(s_al_src[lp->srcIdx].source, AL_VELOCITY,
                        lp->velocity[0], lp->velocity[1], lp->velocity[2]);
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

    /* Move any active source attached to this entity */
    for (i = 0; i < s_al_numSrc; i++) {
        if (!s_al_src[i].isPlaying) continue;
        if (s_al_src[i].entnum == entityNum && !s_al_src[i].fixed_origin) {
            VectorCopy(origin, s_al_src[i].origin);
            qalSource3f(s_al_src[i].source, AL_POSITION,
                        origin[0], origin[1], origin[2]);
        }
    }
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

    /* Switch all non-local sources between normal reverb and underwater effect */
    if (s_al_efx.available) {
        static qboolean lastInwater = qfalse;
        if (s_al_inwater != lastInwater) {
            int j;
            ALuint slot = s_al_inwater ? s_al_efx.underwaterSlot : s_al_efx.reverbSlot;
            for (j = 0; j < s_al_numSrc; j++) {
                if (!s_al_src[j].isPlaying || s_al_src[j].isLocal) continue;
                qalSource3i(s_al_src[j].source, AL_AUXILIARY_SEND_FILTER,
                            (ALint)slot, 0, (ALint)AL_FILTER_NULL);
            }
            lastInwater = s_al_inwater;
        }
    }

    /* Reap stopped sources */
    for (i = 0; i < s_al_numSrc; i++) {
        if (!s_al_src[i].isPlaying || s_al_src[i].loopSound) continue;
        qalGetSourcei(s_al_src[i].source, AL_SOURCE_STATE, &state);
        if (state == AL_STOPPED) {
            qalSourcei(s_al_src[i].source, AL_BUFFER, 0);
            s_al_src[i].isPlaying = qfalse;
            continue;
        }
        /* Occlusion: update every 4 frames to amortise CM_BoxTrace cost */
        if (!s_al_src[i].isLocal) {
            if ((s_al_loopFrame - s_al_src[i].occlusionTick) >= 4) {
                float occ = S_AL_ComputeOcclusion(s_al_src[i].origin);
                s_al_src[i].occlusionGain = occ;
                s_al_src[i].occlusionTick = s_al_loopFrame;

                if (s_al_efx.available) {
                    qalFilterf(s_al_efx.occlusionFilter[i],
                               AL_LOWPASS_GAIN,   0.25f + 0.75f * occ);
                    qalFilterf(s_al_efx.occlusionFilter[i],
                               AL_LOWPASS_GAINHF, occ * occ);
                    qalSourcei(s_al_src[i].source, AL_DIRECT_FILTER,
                               (ALint)s_al_efx.occlusionFilter[i]);
                } else {
                    qalSourcef(s_al_src[i].source, AL_GAIN,
                        (s_al_src[i].master_vol / 255.0f) * occ * masterGain);
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
}

static void S_AL_BeginRegistration( void )
{
    /* Nothing special needed — sounds are loaded on demand. */
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

static void S_AL_SoundInfo( void )
{
    ALCint hrtfStatus = 0;
    const ALchar *vendor  = qalGetString(AL_VENDOR);
    const ALchar *version = qalGetString(AL_VERSION);
    const ALchar *renderer = qalGetString(AL_RENDERER);

    Com_Printf("\n");
    Com_Printf("OpenAL sound backend\n");
    Com_Printf("  Vendor:   %s\n", vendor  ? vendor  : "unknown");
    Com_Printf("  Version:  %s\n", version ? version : "unknown");
    Com_Printf("  Renderer: %s\n", renderer ? renderer : "unknown");

    if (qalcIsExtensionPresent(s_al_device, "ALC_SOFT_HRTF")) {
        qalcGetIntegerv(s_al_device, ALC_HRTF_STATUS_SOFT, 1, &hrtfStatus);
        Com_Printf("  HRTF:     %s\n",
            (hrtfStatus == ALC_HRTF_ENABLED_SOFT) ? "enabled" : "disabled");
    } else {
        Com_Printf("  HRTF:     not available (ALC_SOFT_HRTF extension absent)\n");
    }

    Com_Printf("  Sources:  %d / %d active\n", s_al_numSrc, S_AL_MAX_SRC);
    Com_Printf("  Sfx:      %d loaded\n", s_al_numSfx);

    Com_Printf("  EFX:      %s (%s reverb)\n",
        s_al_efx.available ? "enabled" : "not available",
        s_al_efx.hasEAXReverb ? "EAX" : "standard");

    {
        ALCint ambiOrder = 0;
        if (qalcIsExtensionPresent(s_al_device, "ALC_SOFT_output_mode"))
            qalcGetIntegerv(s_al_device, ALC_MAX_AMBISONIC_ORDER_SOFT, 1, &ambiOrder);
        if (ambiOrder > 0)
            Com_Printf("  Ambisonics: max order %d supported\n", ambiOrder);
    }
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
    Cvar_SetDescription(s_alDevice, "OpenAL output device name. Empty = system default.");
    s_alHRTF    = Cvar_Get("s_alHRTF",   "1",   CVAR_ARCHIVE_ND | CVAR_LATCH);
    Cvar_CheckRange(s_alHRTF, "0", "1", CV_INTEGER);
    Cvar_SetDescription(s_alHRTF, "Enable HRTF (Head-Related Transfer Function) 3D audio via OpenAL Soft. Requires headphones for best effect.");
    s_alMaxDist = Cvar_Get("s_alMaxDist", "1024", CVAR_ARCHIVE_ND);
    Cvar_CheckRange(s_alMaxDist, "128", "4096", CV_FLOAT);
    Cvar_SetDescription(s_alMaxDist, "Maximum attenuation distance for OpenAL sound sources.");

    /* Load library */
    if (!S_AL_LoadLibrary())
        return qfalse;

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
     * We apply three layers to ensure HRTF is active without requiring the
     * user to run alsoft-config.exe or edit alsoft.conf manually.
     *
     * Layer 1 — ALC_SOFT_output_mode (OpenAL Soft 1.20+, most forceful):
     *   ALC_STEREO_HRTF_SOFT requests stereo headphone rendering.  OpenAL
     *   Soft will always honour this value; it overrides any device default.
     *
     * Layer 2 — ALC_SOFT_HRTF (classic request, all OpenAL Soft versions):
     *   ALC_HRTF_SOFT = ALC_TRUE in the context creation attributes.
     *
     * Layer 3 — alcResetDeviceSoft (belt-and-suspenders):
     *   If HRTF still reports disabled after context creation (e.g. OpenAL
     *   implementation switched it off based on device type), call
     *   alcResetDeviceSoft with ALC_HRTF_SOFT = ALC_TRUE to force it.
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
    }
    /* Ambisonic panning: request highest Ambisonic order the device supports. */
    if (s_alHRTF->integer &&
        qalcIsExtensionPresent(s_al_device, "ALC_SOFT_output_mode")) {
        ALCint maxOrder = 1;
        qalcGetIntegerv(s_al_device, ALC_MAX_AMBISONIC_ORDER_SOFT, 1, &maxOrder);
        if (maxOrder > 3) maxOrder = 3;
        if (maxOrder >= 1) {
            ctxAttribs[nAttribs++] = ALC_AMBISONIC_ORDER_SOFT;
            ctxAttribs[nAttribs++] = maxOrder;
            Com_DPrintf("S_AL: requesting Ambisonic order %d for HRTF rendering\n", maxOrder);
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

    /* Query actual HRTF status */
    if (qalcIsExtensionPresent(s_al_device, "ALC_SOFT_HRTF")) {
        qalcGetIntegerv(s_al_device, ALC_HRTF_STATUS_SOFT, 1, &hrtfStatus);
        s_al_hrtf = (hrtfStatus == ALC_HRTF_ENABLED_SOFT);

        /* Layer 3: alcResetDeviceSoft — if HRTF wasn't granted at creation
         * (e.g. OpenAL Soft auto-disabled it for a non-headphone device),
         * re-request it explicitly.  This mirrors what alsoft-config.exe does
         * when "Enable HRTF" is checked. */
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

    /* Allocate source pool */
    s_al_numSrc = 0;
    Com_Memset(s_al_src, 0, sizeof(s_al_src));
    for (i = 0; i < S_AL_MAX_SRC; i++) {
        qalGenSources(1, &s_al_src[i].source);
        if (qalGetError() != AL_NO_ERROR) {
            Com_DPrintf("S_AL: created %d sources (AL_OUT_OF_RESOURCES)\n", i);
            break;
        }
        s_al_src[i].sfx = -1;
        s_al_numSrc++;
    }
    Com_Printf("S_AL: %d sources allocated\n", s_al_numSrc);

    /* Music streaming source and buffers */
    Com_Memset(&s_al_music, 0, sizeof(s_al_music));
    qalGenSources(1, &s_al_music.source);
    qalGenBuffers(S_AL_MUSIC_BUFFERS, s_al_music.buffers);

    /* Raw-samples source and buffers */
    Com_Memset(&s_al_raw, 0, sizeof(s_al_raw));
    qalGenSources(1, &s_al_raw.source);
    qalGenBuffers(S_AL_RAW_BUFFERS, s_al_raw.buffers);
    qalSourcei(s_al_raw.source, AL_SOURCE_RELATIVE, AL_TRUE);
    qalSource3f(s_al_raw.source, AL_POSITION, 0.0f, 0.0f, 0.0f);
    qalSourcef(s_al_raw.source, AL_ROLLOFF_FACTOR, 0.0f);

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
