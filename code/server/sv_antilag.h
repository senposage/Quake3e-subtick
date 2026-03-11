/*
===========================================================================
sv_antilag.h - Engine-side shadow antilag system

Included by sv_main.c, sv_game.c, sv_init.c.
server.h must be included before this header in each .c file.
===========================================================================
*/

#ifndef SV_ANTILAG_H
#define SV_ANTILAG_H

#define SV_ANTILAG_MAX_HISTORY_SLOTS    256
#define SV_ANTILAG_HISTORY_WINDOW_MS    300
#define SV_ANTILAG_MAX_REWIND_MS        200

extern cvar_t *sv_antilag;
extern cvar_t *sv_antilagMaxMs;
extern cvar_t *sv_antilagDebug;
extern cvar_t *sv_antilagRateDebug;

void        SV_Antilag_Init( void );
void        SV_Antilag_ClearClient( int clientNum );
void        SV_Antilag_RecordPositions( void );
void        SV_Antilag_NoteSnapshot( int clientNum );

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
