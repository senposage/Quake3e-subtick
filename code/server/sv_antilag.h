/*
===========================================================================
sv_antilag.h - Engine-side shadow antilag system

Included by sv_main.c, sv_game.c, sv_init.c.
server.h must be included before this header in each .c file.
===========================================================================
*/

#ifndef SV_ANTILAG_H
#define SV_ANTILAG_H

#define SV_ANTILAG_MAX_HISTORY_SLOTS    256  // 256 slots: covers 200ms at up to 1000Hz shadow rate
#define SV_ANTILAG_HISTORY_WINDOW_MS    300
#define SV_ANTILAG_MAX_REWIND_MS        200

// Cvars — registered by SV_Antilag_Init(), accessible after that call
extern cvar_t *sv_antilagEnable;
extern cvar_t *sv_physicsScale;
extern cvar_t *sv_antilagMaxMs;
extern cvar_t *sv_antilagTraceLog;
extern cvar_t *sv_snapRateLog;

// Called once at server init — registers cvars, resets state
void        SV_Antilag_Init( void );

// Called from SV_Frame() once per engine tick, before GAME_RUN_FRAME.
// Records all active client positions into engine-side shadow history.
// Completely decoupled from level.time and QVM.
void        SV_Antilag_RecordPositions( void );
void        SV_Antilag_NoteSnapshot( int clientNum );

// Called from G_TRACE / G_TRACECAPSULE syscall handler in sv_game.c.
// Transparently rewinds clients to shooter's fire time, runs trace, restores all.
// Returns qtrue if the intercept ran (caller should NOT call SV_Trace again).
// Returns qfalse if conditions not met (caller calls SV_Trace normally).
qboolean    SV_Antilag_InterceptTrace(
                trace_t     *results,
                const vec3_t start,
                const vec3_t mins,
                const vec3_t maxs,
                const vec3_t end,
                int         passEntityNum,
                int         contentmask,
                qboolean    capsule
            );

#endif // SV_ANTILAG_H
