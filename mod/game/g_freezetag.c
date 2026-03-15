/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2010-2018 FrozenSand

This file is part of Urban Terror 4.3.4 source code.

Urban Terror source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Urban Terror source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Urban Terror source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

/*
 * g_freezetag.c — Freeze Tag game mode (GT_FREEZE)
 *
 * Reconstructed from UrT 4.3.4 QVM binary analysis.
 *
 * Rules:
 *   - A player hit to zero health is FROZEN instead of killed.
 *   - Teammates can thaw frozen players by standing next to them for
 *     g_thawTime seconds.
 *   - A frozen player who is not thawed within g_meltdownTime seconds
 *     melts (counts as a kill for the enemy team).
 *   - Round ends when all players on one team are frozen or melted.
 *
 * Log events generated:
 *   Freeze: %i %i %i: %s froze %s by %s
 *   ThawOutStarted: %i %i: %s started thawing out %s
 *   ThawOutFinished: %i %i: %s thawed out %s
 *   ClientMelted: %i
 */

#include "g_local.h"

/*
 * G_FreezeTagCheckRoundOver
 *
 * Called each frame. Returns qtrue and records the winner if all
 * players on one side are frozen/dead.
 */
qboolean G_FreezeTagCheckRoundOver( void ) {
	int		i;
	int		redAlive = 0, blueAlive = 0;
	gclient_t	*cl;

	if ( g_gametype.integer != GT_FREEZE ) {
		return qfalse;
	}

	for ( i = 0; i < level.maxclients; i++ ) {
		cl = &level.clients[i];
		if ( cl->pers.connected != CON_CONNECTED ) {
			continue;
		}
		if ( cl->sess.sessionTeam != TEAM_RED &&
			 cl->sess.sessionTeam != TEAM_BLUE ) {
			continue;
		}
		if ( cl->ps.pm_type == PM_DEAD ) {
			continue;
		}
		/* alive and not frozen */
		if ( cl->freezeTime == 0 ) {
			if ( cl->sess.sessionTeam == TEAM_RED ) {
				redAlive++;
			} else {
				blueAlive++;
			}
		}
	}

	if ( redAlive == 0 && blueAlive == 0 ) {
		/* draw — both teams fully frozen simultaneously */
		G_BroadcastRoundWinner( TEAM_FREE );
		return qtrue;
	}
	if ( redAlive == 0 ) {
		G_BroadcastRoundWinner( TEAM_BLUE );
		return qtrue;
	}
	if ( blueAlive == 0 ) {
		G_BroadcastRoundWinner( TEAM_RED );
		return qtrue;
	}

	return qfalse;
}

/*
 * G_FreezePlayer
 *
 * Freeze a player. Called from G_Damage when health drops to zero
 * in GT_FREEZE mode instead of killing the player.
 *
 * attacker: entity that delivered the killing blow (may be NULL)
 * targ:     entity being frozen
 * mod:      means of death
 */
void G_FreezePlayer( gentity_t *targ, gentity_t *attacker, int mod ) {
	gclient_t	*cl;
	int		attackerNum;
	int		targNum;

	if ( !targ || !targ->client ) {
		return;
	}

	cl = targ->client;
	targNum = targ - g_entities;

	/* already frozen */
	if ( cl->freezeTime > 0 ) {
		return;
	}

	attackerNum = attacker ? (attacker - g_entities) : ENTITYNUM_WORLD;

	cl->freezeTime   = level.time;
	cl->meltTime     = level.time + g_meltdownTime.integer * 1000;
	cl->thawingBy    = -1;
	cl->thawStartTime = 0;

	/* set the frozen PMF flag so pmove stops the player */
	cl->ps.pm_flags |= UT_PMF_FROZEN;

	/* broadcast freeze event */
	G_AddEvent( targ, EV_UT_FREEZE, 0 );

	/* Log: Freeze: targNum attackerNum mod: targName froze via weapon */
	G_LogPrintf( "Freeze: %i %i %i: %s froze %s\n",
		targNum,
		attackerNum,
		mod,
		attacker ? attacker->client->pers.netname : "world",
		cl->pers.netname );

	/* ccprint for clients: ccprint "%d" "%d" */
	trap_SendServerCommand( -1, va( "ccprint \"%d\" \"%d\"",
		attackerNum, targNum ) );
}

/*
 * G_ThawPlayer
 *
 * Fully thaw a frozen player. Called when a teammate has stood
 * next to them for g_thawTime seconds.
 */
void G_ThawPlayer( gentity_t *targ, gentity_t *thawer ) {
	gclient_t	*cl;
	int		targNum, thawerNum;

	if ( !targ || !targ->client ) {
		return;
	}

	cl = targ->client;
	targNum  = targ   - g_entities;
	thawerNum = thawer ? (thawer - g_entities) : ENTITYNUM_WORLD;

	cl->freezeTime    = 0;
	cl->meltTime      = 0;
	cl->thawingBy     = -1;
	cl->thawStartTime = 0;

	/* restore health to 1 so they are alive */
	cl->ps.stats[STAT_HEALTH] = 1;
	targ->health = 1;

	/* clear frozen pmove flag */
	cl->ps.pm_flags &= ~UT_PMF_FROZEN;

	G_AddEvent( targ, EV_UT_THAW, 0 );

	G_LogPrintf( "ThawOutFinished: %i %i: %s thawed out %s\n",
		thawerNum, targNum,
		thawer ? thawer->client->pers.netname : "world",
		cl->pers.netname );

	/* ccprint for clients: ccprint "%d" "%d" "1" "%s" */
	trap_SendServerCommand( -1, va( "ccprint \"%d\" \"%d\" \"1\" \"%s\"",
		thawerNum, targNum, cl->pers.netname ) );
}

/*
 * G_FreezeTagThinkClient
 *
 * Called every frame for each connected client to:
 *   1. Process meltdown timer for frozen players.
 *   2. Process thaw progress for players standing near frozen teammates.
 */
void G_FreezeTagThinkClient( gentity_t *ent ) {
	int		i;
	gclient_t	*cl, *other;
	gentity_t	*otherEnt;
	float		dist;
	vec3_t		diff;
	int		entNum;

	if ( !ent->client ) {
		return;
	}

	cl     = ent->client;
	entNum = ent - g_entities;

	/* ---- Meltdown: frozen player timer ---- */
	if ( cl->freezeTime > 0 ) {
		if ( level.time >= cl->meltTime ) {
			/* melt — count as death */
			cl->freezeTime    = 0;
			cl->meltTime      = 0;
			cl->thawingBy     = -1;
			cl->thawStartTime = 0;
			cl->ps.pm_flags  &= ~UT_PMF_FROZEN;

			G_AddEvent( ent, EV_UT_MELT, 0 );
			G_LogPrintf( "ClientMelted: %i\n", entNum );

			/* award kill to no one (meltdown kill) */
			G_Damage( ent, NULL, NULL, NULL, NULL,
				9999, DAMAGE_NO_PROTECTION, UT_MOD_MELT, HL_UNKNOWN );
		}
		return;
	}

	/* ---- Thaw: look for frozen teammates nearby ---- */
	if ( cl->ps.pm_type == PM_DEAD ||
		 cl->ps.stats[STAT_HEALTH] <= 0 ||
		 cl->freezeTime > 0 ) {
		return;
	}

	for ( i = 0; i < level.maxclients; i++ ) {
		otherEnt = &g_entities[i];
		other    = otherEnt->client;

		if ( !other || other == cl ) {
			continue;
		}
		/* must be same team */
		if ( other->sess.sessionTeam != cl->sess.sessionTeam ) {
			continue;
		}
		/* target must be frozen */
		if ( other->freezeTime == 0 ) {
			continue;
		}

		/* must be within thaw range (~64 units) */
		VectorSubtract( otherEnt->r.currentOrigin,
			ent->r.currentOrigin, diff );
		dist = VectorLength( diff );
		if ( dist > 64.0f ) {
			/* if we were thawing this one, cancel */
			if ( other->thawingBy == entNum ) {
				other->thawingBy     = -1;
				other->thawStartTime = 0;

				G_LogPrintf( "ThawOutStarted: %i %i: %s started thawing out %s\n",
					entNum, i,
					cl->pers.netname,
					other->pers.netname );
			}
			continue;
		}

		/* start or continue thawing */
		if ( other->thawingBy != entNum ) {
			other->thawingBy     = entNum;
			other->thawStartTime = level.time;

			G_LogPrintf( "ThawOutStarted: %i %i: %s started thawing out %s\n",
				entNum, i,
				cl->pers.netname,
				other->pers.netname );

			/* ccprint "%d" "%d" "%s" */
			trap_SendServerCommand( -1,
				va( "ccprint \"%d\" \"%d\" \"%s\"",
					entNum, i,
					other->pers.netname ) );
		}

		/* check if thaw complete */
		if ( level.time - other->thawStartTime >=
			 g_thawTime.integer * 1000 ) {
			G_ThawPlayer( otherEnt, ent );
		}

		/* only thaw one person at a time */
		break;
	}
}

/*
 * G_FreezeTagFrame
 *
 * Called once per server frame (from G_RunFrame) for GT_FREEZE.
 * Checks round-over condition.
 */
void G_FreezeTagFrame( void ) {
	if ( g_gametype.integer != GT_FREEZE ) {
		return;
	}

	G_FreezeTagCheckRoundOver();
}
