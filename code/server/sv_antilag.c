/*
===========================================================================
sv_antilag.c - Engine-side shadow antilag system for Quake3e / UrbanTerror

Shadow recording at sv_fps Hz, hit registry via G_TRACE syscall intercept.
QVM contract never violated: level.time untouched, entity state restored
before control returns to VM.
===========================================================================
*/

#include "server.h"
#include "sv_antilag.h"

// ---------------------------------------------------------------------------
// Internal types
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
// Rate tracking (sv_antilagRateDebug)
// ---------------------------------------------------------------------------

#define SV_RATE_WINDOW_MS   2000
#define SV_RATE_SAMPLES     32

typedef struct {
    int     snapTimes[SV_RATE_SAMPLES];
    int     snapHead;
    int     snapCount;
    int     lastSeq;
    int     lastSeqTime;
    int     reportTimer;
} svRateTrack_t;

static svRateTrack_t sv_rateTrack[MAX_CLIENTS];

// ---------------------------------------------------------------------------
// Cvars
// ---------------------------------------------------------------------------

cvar_t *sv_antilag;
cvar_t *sv_antilagMaxMs;
cvar_t *sv_antilagDebug;
cvar_t *sv_antilagRateDebug;

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

static svShadowHistory_t    sv_shadowHistory[MAX_CLIENTS];
static svShadowSaved_t      sv_shadowSaved[MAX_CLIENTS];

static int                  sv_shadowTickMs = 0;
static int                  sv_shadowHistorySlots = 0;
static int                  sv_antilag_lastFpsValue = 0;


// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static void SV_Antilag_ComputeConfig( void ) {
    int fps   = sv_fps ? sv_fps->integer : 40;
    int winMs = sv_antilagMaxMs ? sv_antilagMaxMs->integer : SV_ANTILAG_HISTORY_WINDOW_MS;

    if ( fps < 1 ) fps = 1;

    // Shadow tick rate equals sv_fps.
    //
    // SV_Antilag_RecordPositions() is called exactly once per engine tick,
    // so positions are captured at sv_fps Hz.  Clients also only receive
    // snapshots at sv_fps Hz, meaning the finest position granularity a
    // shooter ever saw is one sv_fps frame.  At sv_fps 60 (16.7 ms/tick)
    // or sv_fps 125 (8 ms/tick) the sampling is already fine enough that
    // interpolation error is well under one frame.
    sv_shadowTickMs = 1000 / fps;
    if ( sv_shadowTickMs < 1 ) sv_shadowTickMs = 1;

    sv_shadowHistorySlots = ( fps * winMs ) / 1000 + 1;
    if ( sv_shadowHistorySlots < 4 )
        sv_shadowHistorySlots = 4;
    if ( sv_shadowHistorySlots > SV_ANTILAG_MAX_HISTORY_SLOTS )
        sv_shadowHistorySlots = SV_ANTILAG_MAX_HISTORY_SLOTS;
}


static void SV_Antilag_RecordClient( int clientNum, int time ) {
    svShadowHistory_t   *hist;
    svShadowPos_t       *slot;
    sharedEntity_t      *gent;
    int                 idx;

    if ( clientNum < 0 || clientNum >= sv.maxclients )
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


static qboolean SV_Antilag_GetPositionAtTime(
    int         clientNum,
    int         targetTime,
    vec3_t      outOrigin,
    vec3_t      outAbsmin,
    vec3_t      outAbsmax
) {
    svShadowHistory_t   *hist;
    svShadowPos_t       *before = NULL, *after = NULL;
    svShadowPos_t       *s;
    int                 i, idx;
    float               frac;

    if ( clientNum < 0 || clientNum >= sv.maxclients )
        return qfalse;

    hist = &sv_shadowHistory[clientNum];
    if ( hist->count == 0 )
        return qfalse;

    for ( i = 0; i < hist->count; i++ ) {
        idx = ( hist->head - 1 - i + sv_shadowHistorySlots )
              % sv_shadowHistorySlots;

        s = &hist->slots[idx];
        if ( !s->valid ) continue;

        if ( s->serverTime <= targetTime ) {
            if ( !before || s->serverTime > before->serverTime )
                before = s;
        } else {
            if ( !after || s->serverTime < after->serverTime )
                after = s;
        }

        // Early exit: we iterate newest-first (backward from head).
        // Once we have both brackets and are now below before's
        // timestamp, no further entry can improve either bracket.
        if ( before && after && s->serverTime < before->serverTime )
            break;
    }

    if ( !before )
        return qfalse;

    if ( !after ) {
        VectorCopy( before->origin, outOrigin );
        VectorCopy( before->absmin, outAbsmin );
        VectorCopy( before->absmax, outAbsmax );
        return qtrue;
    }

    if ( after->serverTime == before->serverTime ) {
        frac = 0.0f;
    } else {
        frac = (float)( targetTime - before->serverTime )
             / (float)( after->serverTime - before->serverTime );
    }
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


static int SV_Antilag_GetClientFireTime( int shooterNum ) {
    client_t *cl;
    int fireTime;
    int maxRewind;

    if ( shooterNum < 0 || shooterNum >= sv.maxclients )
        return sv.time;

    cl = &svs.clients[shooterNum];

    // Full timing path (recording-after-game-frame ensures shadow[T] == snapshot[T]):
    //
    //   T0        positions recorded into shadow history (after game frame at tick T0)
    //   T0        snapshot built and sent; messageSent = Sys_Milliseconds()
    //             shadow[T0] == snapshot[T0]  <- guaranteed by recording order
    //   T0+RTT/2  client receives snapshot; latest server time in client = T0
    //   T0+RTT/2  client renders target at T0 - snapshotMsec because the Q3/URT
    //             client interpolates one snapshot interval behind its latest
    //             received snapshot (cl_interp ~ snapshotMsec)
    //   T0+RTT/2  client fires; sends usercmd
    //   T0+RTT    server receives usercmd; messageAcked = Sys_Milliseconds()
    //             cl->ping  = messageAcked - messageSent = RTT  (server-measured)
    //             cl->snapshotMsec = 1000/snapHz  (per-client, from sv_fps/sv_snapshotFps)
    //             sv.time  ~ T0 + RTT
    //
    //   Target position the client was aiming at: shadow[T0 - snapshotMsec]
    //   fireTime = sv.time - ping - snapshotMsec
    //            = (T0 + RTT) - RTT - snapshotMsec
    //            = T0 - snapshotMsec  [ok]
    //
    // Limitation -- dropped/late snapshots:
    //   If a snapshot was dropped in transit the client interpolates across a
    //   two-snapshot gap, effectively increasing cl_interp by snapshotMsec.
    //   The server cannot detect this without client-reported data, so the
    //   rewind will be snapshotMsec too shallow and the shot may miss.
    //   This is unavoidable without a client-side interp report.
    fireTime = sv.time - cl->ping - cl->snapshotMsec;

    maxRewind = sv_antilagMaxMs ? sv_antilagMaxMs->integer : SV_ANTILAG_MAX_REWIND_MS;
    if ( sv.time - fireTime > maxRewind )
        fireTime = sv.time - maxRewind;
    if ( fireTime > sv.time )
        fireTime = sv.time;

    return fireTime;
}


static int SV_Antilag_RewindAll( int shooterNum, int targetTime ) {
    int             i, rewound = 0;
    sharedEntity_t  *gent;
    client_t        *cl;
    vec3_t          rwOrigin, rwAbsmin, rwAbsmax;

    for ( i = 0; i < sv.maxclients; i++ ) {
        cl = &svs.clients[i];

        if ( i == shooterNum )                              continue;
        if ( cl->state != CS_ACTIVE )                       continue;
        // Bots ARE included in the shadow rewind.
        // The human shooter sees all entities -- including bots -- through the same
        // snapshot/interpolation pipeline.  A shooter with 100 ms ping is aiming
        // at the bot's position from ~(ping + snapshotMsec) ms ago, just as they
        // would be for a human target.  Excluding bots produced a systematic miss
        // for any human vs. bot engagement at non-trivial ping.
        //
        // The original "double-rewind conflict" concern: when g_antilag 1, the QVM
        // FIFO antilag rewinds entities before calling trap_Trace.  The engine
        // intercepts that syscall and applies its OWN rewind on top, which was
        // assumed to conflict.  In practice both save/restore cycles are independent
        // and commute correctly:
        //   1. QVM FIFO may move entities (including any bot if FIFO touches it)
        //   2. Engine saves the post-FIFO state
        //   3. Engine applies shadow rewind -> trace -> restores to post-FIFO state
        //   4. QVM FIFO restores to original
        // The trace always sees shadow-recorded positions; entities end up back at
        // original.  No conflict.
        // sv_antilag 1 also auto-forces g_antilag 0 (SV_TrackCvarChanges) which
        // eliminates any FIFO pre-setup entirely, making the ordering unambiguous.

        gent = SV_GentityNum( i );
        if ( !gent || !gent->r.linked ) continue;

        sv_shadowSaved[i].saved = qtrue;
        VectorCopy( gent->r.currentOrigin, sv_shadowSaved[i].origin );
        VectorCopy( gent->r.absmin,        sv_shadowSaved[i].absmin );
        VectorCopy( gent->r.absmax,        sv_shadowSaved[i].absmax );

        if ( SV_Antilag_GetPositionAtTime( i, targetTime,
                rwOrigin, rwAbsmin, rwAbsmax ) ) {

            if ( sv_antilagDebug && sv_antilagDebug->integer >= 2 ) {
                vec3_t delta;
                float dist;
                VectorSubtract( rwOrigin, sv_shadowSaved[i].origin, delta );
                dist = VectorLength( delta );
                Com_Printf( "  AL rewind cl[%d]: %.1f %.1f %.1f -> %.1f %.1f %.1f (moved %.1f units back)\n",
                    i,
                    sv_shadowSaved[i].origin[0], sv_shadowSaved[i].origin[1], sv_shadowSaved[i].origin[2],
                    rwOrigin[0], rwOrigin[1], rwOrigin[2],
                    dist );
            }

            VectorCopy( rwOrigin, gent->r.currentOrigin );
            VectorCopy( rwAbsmin, gent->r.absmin );
            VectorCopy( rwAbsmax, gent->r.absmax );
            SV_LinkEntity( gent );
            rewound++;
        }
    }

    return rewound;
}


static void SV_Antilag_RestoreAll( int shooterNum ) {
    int             i;
    sharedEntity_t  *gent;
    client_t        *cl;

    for ( i = 0; i < sv.maxclients; i++ ) {
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
SV_Antilag_ClearClient

Zeroes the shadow history ring buffer for a single client slot.
Called when a client disconnects so that a new occupant (bot or human)
does not inherit stale position entries recorded for the previous player.
*/
void SV_Antilag_ClearClient( int clientNum ) {
    if ( clientNum < 0 || clientNum >= MAX_CLIENTS )
        return;
    Com_Memset( &sv_shadowHistory[clientNum], 0, sizeof( sv_shadowHistory[clientNum] ) );
}


void SV_Antilag_Init( void ) {
    sv_antilag       = Cvar_Get( "sv_antilag",       "0",   CVAR_SERVERINFO );
    sv_antilagMaxMs  = Cvar_Get( "sv_antilagMaxMs",  "200", CVAR_SERVERINFO );
    sv_antilagDebug      = Cvar_Get( "sv_antilagDebug",      "0", CVAR_TEMP );
    sv_antilagRateDebug  = Cvar_Get( "sv_antilagRateDebug",  "0", CVAR_TEMP );

    Com_Memset( sv_shadowHistory, 0, sizeof( sv_shadowHistory ) );
    Com_Memset( sv_shadowSaved,   0, sizeof( sv_shadowSaved ) );
    Com_Memset( sv_rateTrack,     0, sizeof( sv_rateTrack ) );

    sv_antilag_lastFpsValue = sv_fps ? sv_fps->integer : 40;

    SV_Antilag_ComputeConfig();

    Com_Printf( "SV_Antilag: shadow=%dHz slots=%d covers~%dms maxRewind=%dms\n",
        sv_fps ? sv_fps->integer : 40,
        sv_shadowHistorySlots,
        sv_shadowHistorySlots * sv_shadowTickMs,
        sv_antilagMaxMs->integer );
}


void SV_Antilag_RecordPositions( void ) {
    int i;

    {
        int currentFps = sv_fps ? sv_fps->integer : 40;
        qboolean fpsChanged = ( currentFps != sv_antilag_lastFpsValue );

        if ( sv_antilagMaxMs->modified || fpsChanged ) {
            SV_Antilag_ComputeConfig();
            sv_antilagMaxMs->modified = qfalse;
            sv_antilag_lastFpsValue = currentFps;
            Com_Memset( sv_shadowHistory, 0, sizeof( sv_shadowHistory ) );
            Com_Printf( "SV_Antilag: reconfigured -- shadow Hz=%d, historySlots=%d (history flushed)\n",
                currentFps, sv_shadowHistorySlots );
        }
    }

    for ( i = 0; i < sv.maxclients; i++ ) {
        client_t *cl = &svs.clients[i];
        if ( cl->state != CS_ACTIVE ) continue;
        // Bots ARE recorded.  Their positions must be in shadow history so that
        // human-vs-bot traces can be rewound to the shooter's fireTime, giving
        // fair hit registration at non-trivial ping (see SV_Antilag_RewindAll).
        SV_Antilag_RecordClient( i, sv.time );
    }
}


void SV_Antilag_NoteSnapshot( int clientNum ) {
    svRateTrack_t   *rt;
    client_t        *cl;
    int             idx;

    if ( !sv_antilagRateDebug || !sv_antilagRateDebug->integer )
        return;

    if ( clientNum < 0 || clientNum >= sv.maxclients )
        return;

    rt = &sv_rateTrack[clientNum];
    cl = &svs.clients[clientNum];

    idx = rt->snapHead % SV_RATE_SAMPLES;
    rt->snapTimes[idx] = svs.time;
    rt->snapHead++;
    if ( rt->snapCount < SV_RATE_SAMPLES )
        rt->snapCount++;

    if ( svs.time < rt->reportTimer )
        return;
    rt->reportTimer = svs.time + 5000;

    {
        int windowStart = svs.time - SV_RATE_WINDOW_MS;
        int snapsSeen   = 0;
        int oldest = svs.time, newest = 0;
        int i;
        float obsHz = 0.0f;
        float pktHz = 0.0f;

        for ( i = 0; i < rt->snapCount; i++ ) {
            int t = rt->snapTimes[( rt->snapHead - 1 - i + SV_RATE_SAMPLES ) % SV_RATE_SAMPLES];
            if ( t >= windowStart ) {
                snapsSeen++;
                if ( t < oldest ) oldest = t;
                if ( t > newest ) newest = t;
            }
        }

        if ( snapsSeen > 1 && newest > oldest )
            obsHz = ( snapsSeen - 1 ) * 1000.0f / (float)( newest - oldest );

        if ( rt->lastSeqTime > 0 && svs.time > rt->lastSeqTime ) {
            int seqDelta  = cl->netchan.outgoingSequence - rt->lastSeq;
            int timeDelta = svs.time - rt->lastSeqTime;
            pktHz = seqDelta * 1000.0f / (float)timeDelta;
        }
        rt->lastSeq     = cl->netchan.outgoingSequence;
        rt->lastSeqTime = svs.time;

        Com_Printf( "RATE cl[%2d] %-16s | snap: obs=%.1fHz target=%dHz | pkt: %.1fHz | ping=%dms\n",
            clientNum, cl->name, obsHz,
            1000 / ( cl->snapshotMsec > 0 ? cl->snapshotMsec : 1 ),
            pktHz, cl->ping );
    }
}


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

    if ( !sv_antilag || !sv_antilag->integer )
        return qfalse;

    if ( passEntityNum < 0 || passEntityNum >= sv.maxclients )
        return qfalse;

    // Skip movement traces -- only intercept weapon traces (no CONTENTS_PLAYERCLIP)
    if ( contentmask & CONTENTS_PLAYERCLIP )
        return qfalse;

    {
        client_t *shooter = &svs.clients[passEntityNum];
        if ( shooter->state != CS_ACTIVE )
            return qfalse;
        if ( shooter->netchan.remoteAddress.type == NA_BOT )
            return qfalse;
    }

    fireTime = SV_Antilag_GetClientFireTime( passEntityNum );

    if ( sv_antilagDebug && sv_antilagDebug->integer >= 1 ) {
        client_t *shooter = &svs.clients[ passEntityNum ];
        Com_Printf( "AL shot cl[%d] ping=%d snapMsec=%d rewind=%dms (sv.time=%d fireTime=%d)\n",
            passEntityNum, shooter->ping, shooter->snapshotMsec,
            sv.time - fireTime, sv.time, fireTime );
    }

    rewound = SV_Antilag_RewindAll( passEntityNum, fireTime );

    SV_Trace( results, start, mins, maxs, end, passEntityNum, contentmask, capsule );

    SV_Antilag_RestoreAll( passEntityNum );

    if ( sv_antilagDebug && sv_antilagDebug->integer >= 1 ) {
        const char *hitStr = ( results->entityNum < MAX_CLIENTS ) ? "HIT PLAYER" :
                             ( results->entityNum == ENTITYNUM_WORLD ) ? "hit world" :
                             ( results->fraction >= 1.0f ) ? "miss" : "hit entity";
        Com_Printf( "AL result: %s (ent=%d frac=%.3f) rewound=%d clients\n",
            hitStr, results->entityNum, results->fraction, rewound );
    }

    return qtrue;
}
