/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2010-2018 FrozenSand

This file is part of Urban Terror 4.3.4 source code.
Reconstructed from QVM binary analysis.
===========================================================================
*/

/*
 * cg_freezetag.c — Freeze Tag client-game visuals and HUD
 *
 * Handles:
 *   - Drawing the thaw progress arc around frozen players
 *   - Drawing the "FROZEN" overlay on the local player's screen
 *   - Parsing ccprint commands from the server for freeze/thaw notifications
 *
 * Asset paths confirmed from cgame.qvm strings:
 *   gfx/2d/bigchars   (for on-screen FROZEN text)
 */

#include "cg_local.h"

/*
 * CG_FreezeTagDrawFrozenOverlay
 *
 * Draw a semi-transparent blue overlay and "FROZEN" text when
 * the local player is currently frozen.
 * Called from CG_Draw2D.
 */
void CG_FreezeTagDrawFrozenOverlay( void ) {
	float	alpha;
	int		msRemaining;

	if ( !( cg.snap->ps.pm_flags & UT_PMF_FROZEN ) ) {
		return;
	}

	/* pulse alpha between 0.3 and 0.7 */
	alpha = 0.5f + 0.2f * sin( cg.time * 0.005f );

	/* blue tint overlay */
	{
		vec4_t freezeColor = { 0.0f, 0.2f, 0.8f, 0.0f };
		freezeColor[3] = alpha * 0.35f;
		trap_R_SetColor( NULL );
		CG_FillRect( 0, 0, 640, 480, freezeColor );
	}

	/* "FROZEN" centred text */
	CG_CenterPrint( "FROZEN", 180, BIGCHAR_WIDTH );
}

/*
 * CG_FreezeTagDrawThawProgress
 *
 * Draw a radial progress indicator around the frozen player's world position
 * to show thaw progress. Called from CG_DrawActiveFrame.
 */
void CG_FreezeTagDrawThawProgress( centity_t *cent ) {
	/* Thaw progress is communicated via entityState flags.
	 * Draw a simple arc centred on the frozen player billboard. */
	(void)cent;
	/* TODO: implement arc shader draw once shader list is finalised */
}

/*
 * CG_FreezeTagProcessServerCmd
 *
 * Parse a ccprint command sent from g_freezetag.c.
 *
 * Formats (from QVM string analysis):
 *   ccprint "%d" "%d"           — player A froze player B
 *   ccprint "%d" "%d" "%s"      — player A started thawing player B (name)
 *   ccprint "%d" "%d" "1" "%s"  — player A finished thawing player B (name)
 *   ccprint "%d" "%d" "-1"      — thaw cancelled
 */
void CG_FreezeTagProcessServerCmd( const char *cmd ) {
	int		attacker, victim;
	const char	*s;

	attacker = atoi( CG_Argv( 1 ) );
	victim   = atoi( CG_Argv( 2 ) );
	s        = CG_Argv( 3 );

	(void)attacker;
	(void)victim;

	if ( s[0] == '1' ) {
		/* thaw complete — show notification */
		const char *name = CG_Argv( 4 );
		CG_CenterPrint( va( "^2%s^7 thawed out", name ), 200, SMALLCHAR_WIDTH );
	} else if ( s[0] == '-' ) {
		/* thaw cancelled — no notification needed */
	} else if ( s[0] != '\0' ) {
		/* thaw started — show name */
		CG_CenterPrint( va( "Thawing ^2%s^7...", s ), 200, SMALLCHAR_WIDTH );
	}
}
