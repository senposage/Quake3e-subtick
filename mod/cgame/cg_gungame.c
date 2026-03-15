/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2010-2018 FrozenSand

This file is part of Urban Terror 4.3.4 source code.
Reconstructed from QVM binary analysis.
===========================================================================
*/

/*
 * cg_gungame.c — Gun Game HUD elements
 *
 * Displays the current weapon level progress to the local player
 * in GT_GUNGAME mode.
 *
 * Log events / server cmds parsed:
 *   ccprint "%d" "%d" "%d"   — player promoted to level (server->client)
 */

#include "cg_local.h"

/*
 * CG_GunGameDrawProgress
 *
 * Draw the current weapon level indicator in the bottom of the HUD.
 * Called from CG_DrawActiveFrame when gametype == GT_GUNGAME.
 */
void CG_GunGameDrawProgress( void ) {
	int		level;
	const char	*weapName;

	if ( cgs.gametype != GT_GUNGAME ) {
		return;
	}

	/* level is stored in persistant[] PERS_ATTACKER slot
	 * until we have a dedicated client-side variable */
	level = cg.snap->ps.persistant[PERS_SCORE]; /* placeholder */

	weapName = "Unknown";
	/* TODO: map level to weapon name via bg_weaponlist */

	CG_DrawSmallStringColor( 320 - 50, 440,
		va( "Level %d: ^3%s", level, weapName ),
		colorWhite );
}

/*
 * CG_GunGameProcessServerCmd
 *
 * Parse ccprint promotion command from server.
 * Format: ccprint "%d" "%d" "%d" — attacker, victim, newLevel
 */
void CG_GunGameProcessServerCmd( void ) {
	int	attacker, victim, newLevel;

	attacker = atoi( CG_Argv( 1 ) );
	victim   = atoi( CG_Argv( 2 ) );
	newLevel = atoi( CG_Argv( 3 ) );

	(void)victim;

	if ( attacker == cg.clientNum ) {
		CG_CenterPrint(
			va( "^2Level Up!^7 Level %d", newLevel ),
			200, BIGCHAR_WIDTH );
	}
}
