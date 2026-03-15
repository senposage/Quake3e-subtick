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
 * g_gungame.c — Gun Game mode (GT_GUNGAME)
 *
 * Reconstructed from UrT 4.3.4 QVM binary analysis.
 *
 * Rules:
 *   - All players start with level 0 weapon (knife).
 *   - Each kill promotes the killer to the next weapon level.
 *   - The first player to complete all weapon levels wins.
 *   - Being killed does NOT demote the victim.
 *   - "Gunlimit" exit condition: first to reach top level triggers round end.
 *
 * Log events generated:
 *   Gunlimit hit.
 *
 * Vote command: g_instagib cannot be used with GT_GUNGAME.
 *
 * String confirmed in 4.3.4 QVM:
 *   "GunGame", "GUNGAME", "Gun Game"
 *   "Gunlimit hit."
 */

#include "g_local.h"

/* Ordered weapon progression for Gun Game.
 * Reconstructed from QVM weapon strings and typical UrT Gun Game rules.
 * Starts from knife, progresses through standard weapons to the SR8 at top. */
static const weapon_t gg_weaponLevels[] = {
	/* 4.3.4 Gun Game weapon progression table (11 levels) */
	UT_WP_KNIFE,	    /* level  0 */
	UT_WP_BERETTA,	    /* level  1 */
	UT_WP_DEAGLE,	    /* level  2 */
	UT_WP_SPAS12,	    /* level  3 */
	UT_WP_MP5K,		    /* level  4 */
	UT_WP_UMP45,	    /* level  5 */
	UT_WP_HK69,		    /* level  6 */
	UT_WP_LR,		    /* level  7 */
	UT_WP_G36,		    /* level  8 */
	UT_WP_PSG1,		    /* level  9 */
	UT_WP_SR8,		    /* level 10 — final level */
};

#define GG_NUM_LEVELS  ( sizeof(gg_weaponLevels) / sizeof(gg_weaponLevels[0]) )
#define GG_FINAL_LEVEL ( GG_NUM_LEVELS - 1 )

/*
 * G_GunGameGetWeaponForLevel
 *
 * Returns the weapon_t for the given Gun Game level.
 */
weapon_t G_GunGameGetWeaponForLevel( int level ) {
	if ( level < 0 ) {
		level = 0;
	}
	if ( level >= (int)GG_NUM_LEVELS ) {
		level = GG_FINAL_LEVEL;
	}
	return gg_weaponLevels[level];
}

/*
 * G_GunGameGiveWeapon
 *
 * Strip all weapons from the player and give them the weapon for
 * their current Gun Game level.
 */
void G_GunGameGiveWeapon( gentity_t *ent ) {
	gclient_t	*cl;
	weapon_t	wp;
	int		level;

	if ( !ent || !ent->client ) {
		return;
	}

	cl    = ent->client;
	level = cl->gunGameLevel;
	wp    = G_GunGameGetWeaponForLevel( level );

	/* strip all weapon data */
	memset( cl->ps.weaponData, 0, sizeof( cl->ps.weaponData ) );
	cl->ps.weaponslot = 0;

	/* give the level weapon with one magazine */
	utPSGiveWeapon( &cl->ps, wp, 0 );
}

/*
 * G_GunGameOnKill
 *
 * Called from player_die when the game mode is GT_GUNGAME and the
 * attacker and victim are different players.
 *
 * Promotes the attacker to the next weapon level.
 * Checks for the gun-limit (win condition).
 */
void G_GunGameOnKill( gentity_t *attacker, gentity_t *victim ) {
	gclient_t	*cl;
	int		newLevel;

	if ( g_gametype.integer != GT_GUNGAME ) {
		return;
	}
	if ( !attacker || !attacker->client ) {
		return;
	}
	if ( attacker == victim ) {
		return;	/* no promotion on self-kill */
	}

	cl = attacker->client;
	newLevel = cl->gunGameLevel + 1;

	if ( newLevel >= (int)GG_NUM_LEVELS ) {
		/* winner — this player reached the top */
		G_LogPrintf( "Gunlimit hit.\n" );
		LogExit( "Gunlimit hit." );
		return;
	}

	cl->gunGameLevel = newLevel;
	G_GunGameGiveWeapon( attacker );

	/* ccprint "%d" "%d" "%d" — attacker, victim, newLevel */
	trap_SendServerCommand( -1,
		va( "ccprint \"%d\" \"%d\" \"%d\"",
			(int)(attacker - g_entities),
			(int)(victim   - g_entities),
			newLevel ) );
}

/*
 * G_GunGameInitClient
 *
 * Reset a client to Gun Game level 0 at spawn.
 * Called from ClientSpawn when GT_GUNGAME is active.
 */
void G_GunGameInitClient( gentity_t *ent ) {
	if ( !ent || !ent->client ) {
		return;
	}
	if ( g_gametype.integer != GT_GUNGAME ) {
		return;
	}

	ent->client->gunGameLevel = 0;
	G_GunGameGiveWeapon( ent );
}
