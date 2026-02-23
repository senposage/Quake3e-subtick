/*
===========================================================================
sv_antilag.c - Engine-side shadow antilag system for Quake3e / UrbanTerror

Architecture overview:
───────────────────────────────────────────────────────────────────────────
  GAME TICK (sv_fps Hz)          → QVM sees this. level.time advances here.
                                   QVM's own FIFO antilag runs here unchanged.

  SHADOW SUB-TICK (sv_fps * sv_physicsScale Hz)
                                 → Engine only. Records entity positions into
                                   svShadowHistory[]. QVM never sees this.

  HIT REGISTRY (on G_TRACE syscall)
                                 → Engine intercepts trap_Trace from QVM.
                                   Rewinds entity positions to shooter's
                                   clientFireTime using shadow history.
                                   Runs trace. Restores ALL positions.
                                   Returns result to QVM. QVM sees no diff.
───────────────────────────────────────────────────────────────────────────

QVM CONTRACT (never violated):
  - level.time advances exactly (1000/sv_fps) ms per game frame
  - Entity state is stable and correct at every GAME_RUN_FRAME entry
  - G_Damage is called with valid entity pointers, same signature
  - trap_Trace returns results consistent with current game state
    (from QVM perspective — we rewind/restore before QVM can observe)
  - nextthink / events fire at expected times
  - The QVM's own G_StoreFIFOTrail / G_TimeShiftAllClients are untouched

===========================================================================
*/

#include "server.h"
#include "sv_antilag.h"

// ---------------------------------------------------------------------------
// Internal types - engine only, never exposed to QVM
// ---------------------------------------------------------------------------

typedef struct {
    vec3_t  origin;
    vec3_t  absmin;
    vec3_t  absmax;
    int     serverTime;
    qboolean valid;
} svShadowPos_t;

typedef struct {
    svShadowPos_t   slots[SV_ANTILAG_MAX_HISTORY_SLOTS];
    int             head;
    int             count;
} svShadowHistory_t;

typedef struct {
    vec3_t      origin;
    vec3_t      absmin;
    vec3_t      absmax;
    qboolean    saved;
} svShadowSaved_t;

// ---------------------------------------------------------------------------
// Cvars
// ---------------------------------------------------------------------------

cvar_t *sv_antilagEnable;
cvar_t *sv_physicsScale;
cvar_t *sv_antilagMaxMs;

// ---------------------------------------------------------------------------
// Per-client shadow history state
// ---------------------------------------------------------------------------

static svShadowHistory_t    sv_shadowHistory[MAX_CLIENTS];
static svShadowSaved_t      sv_shadowSaved[MAX_CLIENTS];

// Shadow sub-tick accumulator — tracks fractional ms between engine ticks
static int                  sv_shadowAccumulator = 0;
static int                  sv_shadowTickMs = 0;        // computed each frame
static int                  sv_shadowHistorySlots = 0;  // computed on init/change

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

/*
SV_Antilag_ComputeConfig

Recomputes shadow tick rate and history depth from current cvars.
Called on init and whenever sv_fps or sv_physicsScale change.
*/
static void SV_Antilag_ComputeConfig( void ) {
    int fps   = sv_fps ? sv_fps->integer : 40;
    int scale = sv_physicsScale ? sv_physicsScale->integer : 3;
    int winMs = sv_antilagMaxMs ? sv_antilagMaxMs->integer : SV_ANTILAG_HISTORY_WINDOW_MS;

    // Clamp scale to sane integer range so sub-steps divide game tick evenly
    if ( scale < 1 )  scale = 1;
    if ( scale > 8 )  scale = 8;

    // Shadow Hz = sv_fps * scale (e.g. 40 * 3 = 120Hz)
    int shadowHz = fps * scale;
    sv_shadowTickMs = 1000 / shadowHz;
    if ( sv_shadowTickMs < 1 ) sv_shadowTickMs = 1;

    // History depth = enough slots to cover the window at shadow Hz
    sv_shadowHistorySlots = ( shadowHz * winMs ) / 1000;
    if ( sv_shadowHistorySlots < 4 )
        sv_shadowHistorySlots = 4;
    if ( sv_shadowHistorySlots > SV_ANTILAG_MAX_HISTORY_SLOTS )
        sv_shadowHistorySlots = SV_ANTILAG_MAX_HISTORY_SLOTS;
}


/*
SV_Antilag_RecordClient

Records the current position of one client into their shadow history ring.
*/
static void SV_Antilag_RecordClient( int clientNum, int time ) {
    svShadowHistory_t   *hist;
    svShadowPos_t       *slot;
    sharedEntity_t      *gent;
    int                 idx;

    if ( clientNum < 0 || clientNum >= sv_maxclients->integer )
        return;

    gent = SV_GentityNum( clientNum );
    if ( !gent )
        return;

    hist = &sv_shadowHistory[clientNum];
    idx  = hist->head % sv_shadowHistorySlots;

    slot = &hist->slots[idx];
    VectorCopy( gent->r.currentOrigin, slot->origin );
    VectorCopy( gent->r.absmin,        slot->absmin );
    VectorCopy( gent->r.absmax,        slot->absmax );
    slot->serverTime = time;
    slot->valid = qtrue;

    hist->head++;
    if ( hist->count < sv_shadowHistorySlots )
        hist->count++;
}


/*
SV_Antilag_GetPositionAtTime

Interpolates a client's shadow position at an arbitrary past time.
Returns qfalse if no history is available for that time range.
*/
static qboolean SV_Antilag_GetPositionAtTime(
    int         clientNum,
    int         targetTime,
    vec3_t      outOrigin,
    vec3_t      outAbsmin,
    vec3_t      outAbsmax
) {
    svShadowHistory_t   *hist;
    svShadowPos_t       *before = NULL, *after = NULL;
    int                 i, idx;
    float               frac;

    if ( clientNum < 0 || clientNum >= sv_maxclients->integer )
        return qfalse;

    hist = &sv_shadowHistory[clientNum];
    if ( hist->count == 0 )
        return qfalse;

    // Walk the valid portion of the ring buffer
    for ( i = 0; i < hist->count; i++ ) {
        idx = ( hist->head - 1 - i + SV_ANTILAG_MAX_HISTORY_SLOTS )
              % sv_shadowHistorySlots;

        svShadowPos_t *s = &hist->slots[idx];
        if ( !s->valid ) continue;

        if ( s->serverTime <= targetTime ) {
            if ( !before || s->serverTime > before->serverTime )
                before = s;
        } else {
            if ( !after || s->serverTime < after->serverTime )
                after = s;
        }
    }

    if ( !before ) {
        // Target time is older than our entire history — use oldest entry
        return qfalse;
    }

    if ( !after ) {
        // Target time is at or beyond most recent entry — use it directly
        VectorCopy( before->origin, outOrigin );
        VectorCopy( before->absmin, outAbsmin );
        VectorCopy( before->absmax, outAbsmax );
        return qtrue;
    }

    // Interpolate between before and after
    if ( after->serverTime == before->serverTime ) {
        frac = 0.0f;
    } else {
        frac = (float)( targetTime - before->serverTime )
             / (float)( after->serverTime - before->serverTime );
    }

    // Clamp to [0,1] — should always be true but be safe
    if ( frac < 0.0f ) frac = 0.0f;
    if ( frac > 1.0f ) frac = 1.0f;

    outOrigin[0] = before->origin[0] + frac * ( after->origin[0] - before->origin[0] );
    outOrigin[1] = before->origin[1] + frac * ( after->origin[1] - before->origin[1] );
    outOrigin[2] = before->origin[2] + frac * ( after->origin[2] - before->origin[2] );

    outAbsmin[0] = before->absmin[0] + frac * ( after->absmin[0] - before->absmin[0] );
    outAbsmin[1] = before->absmin[1] + frac * ( after->absmin[1] - before->absmin[1] );
    outAbsmin[2] = before->absmin[2] + frac * ( after->absmin[2] - before->absmin[2] );

    outAbsmax[0] = before->absmax[0] + frac * ( after->absmax[0] - before->absmax[0] );
    outAbsmax[1] = before->absmax[1] + frac * ( after->absmax[1] - before->absmax[1] );
    outAbsmax[2] = before->absmax[2] + frac * ( after->absmax[2] - before->absmax[2] );

    return qtrue;
}


/*
SV_Antilag_GetClientFireTime

Extracts the client's fire time from the QVM entity state.
The QVM (UT4.2) stores this as AttackTime in the client struct,
which corresponds to ucmd->serverTime captured before the sanity clamp.

On the engine side we access it through the playerState ping offset —
the QVM writes ps.commandTime which is updated per-ucmd.
We use svs.time - client->ping as the conservative estimate if
we can't read the exact ucmd serverTime directly.
*/
static int SV_Antilag_GetClientFireTime( int shooterNum ) {
    client_t *cl;
    int fireTime;
    int maxRewind;

    if ( shooterNum < 0 || shooterNum >= sv_maxclients->integer )
        return svs.time;

    cl = &svs.clients[shooterNum];

    // Best estimate: current server time minus measured ping.
    // This mirrors what the QVM's AttackTime - ut_timenudge approximates.
    // The QVM's own FIFO system will also rewind, but our shadow rewind
    // operates on the higher-resolution shadow history independently.
    fireTime = svs.time - cl->ping;

    // Clamp: never rewind more than sv_antilagMaxMs
    maxRewind = sv_antilagMaxMs ? sv_antilagMaxMs->integer : SV_ANTILAG_MAX_REWIND_MS;
    if ( svs.time - fireTime > maxRewind )
        fireTime = svs.time - maxRewind;

    // Clamp: never go into the future
    if ( fireTime > svs.time )
        fireTime = svs.time;

    return fireTime;
}


/*
SV_Antilag_RewindAll

Saves and rewinds all active non-shooter clients to targetTime.
Returns number of clients rewound (used to decide if restore is needed).
MUST be paired with SV_Antilag_RestoreAll.
*/
static int SV_Antilag_RewindAll( int shooterNum, int targetTime ) {
    int             i, rewound = 0;
    sharedEntity_t  *gent;
    client_t        *cl;
    vec3_t          rwOrigin, rwAbsmin, rwAbsmax;

    for ( i = 0; i < sv_maxclients->integer; i++ ) {
        cl = &svs.clients[i];

        // Skip shooter, free slots, spectators
        if ( i == shooterNum )              continue;
        if ( cl->state != CS_ACTIVE )       continue;

        gent = SV_GentityNum( i );
        if ( !gent || !gent->r.linked )     continue;

        // Save current state unconditionally — restore must always fire
        sv_shadowSaved[i].saved = qtrue;
        VectorCopy( gent->r.currentOrigin, sv_shadowSaved[i].origin );
        VectorCopy( gent->r.absmin,        sv_shadowSaved[i].absmin );
        VectorCopy( gent->r.absmax,        sv_shadowSaved[i].absmax );

        // Attempt shadow rewind
        if ( SV_Antilag_GetPositionAtTime( i, targetTime,
                rwOrigin, rwAbsmin, rwAbsmax ) ) {

            VectorCopy( rwOrigin, gent->r.currentOrigin );
            VectorCopy( rwAbsmin, gent->r.absmin );
            VectorCopy( rwAbsmax, gent->r.absmax );
            SV_LinkEntity( gent );
            rewound++;
        }
        // If no history available, entity stays at current position
        // (still gets restored below — saved flag ensures that)
    }

    return rewound;
}


/*
SV_Antilag_RestoreAll

Restores all clients that were saved by SV_Antilag_RewindAll.
Called unconditionally after trace, regardless of errors.
This is the guarantee that makes the system QVM-safe.
*/
static void SV_Antilag_RestoreAll( int shooterNum ) {
    int             i;
    sharedEntity_t  *gent;
    client_t        *cl;

    for ( i = 0; i < sv_maxclients->integer; i++ ) {
        if ( i == shooterNum ) continue;

        if ( !sv_shadowSaved[i].saved ) continue;
        sv_shadowSaved[i].saved = qfalse;

        cl = &svs.clients[i];
        if ( cl->state != CS_ACTIVE ) continue;

        gent = SV_GentityNum( i );
        if ( !gent ) continue;

        VectorCopy( sv_shadowSaved[i].origin, gent->r.currentOrigin );
        VectorCopy( sv_shadowSaved[i].absmin, gent->r.absmin );
        VectorCopy( sv_shadowSaved[i].absmax, gent->r.absmax );
        SV_LinkEntity( gent );
    }
}


// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/*
SV_Antilag_Init

Registers cvars and resets state. Called from SV_Init.
*/
void SV_Antilag_Init( void ) {
    sv_antilagEnable = Cvar_Get( "sv_antilagEnable", "1",   CVAR_SERVERINFO );
    sv_physicsScale  = Cvar_Get( "sv_physicsScale",  "3",   CVAR_SERVERINFO );
    sv_antilagMaxMs  = Cvar_Get( "sv_antilagMaxMs",  "200", CVAR_SERVERINFO );

    Com_Memset( sv_shadowHistory, 0, sizeof( sv_shadowHistory ) );
    Com_Memset( sv_shadowSaved,   0, sizeof( sv_shadowSaved ) );

    sv_shadowAccumulator = 0;

    SV_Antilag_ComputeConfig();

    Com_Printf( "SV_Antilag: shadow Hz=%d, historySlots=%d, maxRewindMs=%d\n",
        1000 / sv_shadowTickMs,
        sv_shadowHistorySlots,
        sv_antilagMaxMs->integer );
}


/*
SV_Antilag_RecordPositions

Called from SV_Frame() BEFORE GAME_RUN_FRAME, once per shadow sub-tick.
Decoupled from game tick — runs at sv_fps * sv_physicsScale Hz.
Recomputes config if cvars changed.
*/
void SV_Antilag_RecordPositions( void ) {
    int i;

    // Recompute config if relevant cvars changed
    if ( sv_physicsScale->modified || sv_antilagMaxMs->modified ) {
        SV_Antilag_ComputeConfig();
        sv_physicsScale->modified = qfalse;
        sv_antilagMaxMs->modified = qfalse;
        Com_Printf( "SV_Antilag: reconfigured — shadow Hz=%d, historySlots=%d\n",
            1000 / sv_shadowTickMs, sv_shadowHistorySlots );
    }

    // Record all active clients into shadow history
    for ( i = 0; i < sv_maxclients->integer; i++ ) {
        client_t *cl = &svs.clients[i];
        if ( cl->state != CS_ACTIVE ) continue;
        if ( cl->netchan.remoteAddress.type == NA_BOT ) continue;
        SV_Antilag_RecordClient( i, svs.time );
    }
}


/*
SV_Antilag_InterceptTrace

Called from the G_TRACE / G_TRACECAPSULE syscall handler in sv_game.c,
replacing the normal SV_Trace when the shooter has antilag enabled.

The intercept:
  1. Determines rewind target time from shooter's ping
  2. Saves + rewinds all other clients in shadow history
  3. Runs the actual SV_Trace at the rewound world state
  4. Restores ALL clients unconditionally
  5. Returns result to QVM

The QVM never sees entities in a rewound state — the restore happens
before control returns to the VM. level.time is never touched.
*/
qboolean SV_Antilag_InterceptTrace(
    trace_t         *results,
    const vec3_t    start,
    const vec3_t    mins,
    const vec3_t    maxs,
    const vec3_t    end,
    int             passEntityNum,
    int             contentmask,
    qboolean        capsule
) {
    int fireTime;
    int rewound;

    // Master switch
    if ( !sv_antilagEnable || !sv_antilagEnable->integer )
        return qfalse;

    // Only intercept if passEntityNum is a valid active player
    // (shooters pass their own entity num as passEntityNum in utTrace)
    if ( passEntityNum < 0 || passEntityNum >= sv_maxclients->integer )
        return qfalse;

    {
        client_t *shooter = &svs.clients[passEntityNum];
        if ( shooter->state != CS_ACTIVE )
            return qfalse;
        // Don't apply to bots — they have no lag to compensate
        if ( shooter->netchan.remoteAddress.type == NA_BOT )
            return qfalse;
    }

    // Determine the rewind target time
    fireTime = SV_Antilag_GetClientFireTime( passEntityNum );

    // Rewind all other clients into shadow positions
    rewound = SV_Antilag_RewindAll( passEntityNum, fireTime );

    // Run the actual trace at the rewound world state
    // This is identical to what sv_game.c would have called
    SV_Trace( results, start, mins, maxs, end, passEntityNum, contentmask, capsule );

    // Unconditional restore — QVM sees a clean world after this returns
    SV_Antilag_RestoreAll( passEntityNum );

    (void)rewound; // available for debug logging if needed

    return qtrue; // we handled it, caller should not call SV_Trace again
}
