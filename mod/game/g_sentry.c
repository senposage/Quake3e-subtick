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
 * g_sentry.c — Mr. Sentry autonomous turret entity (new in 4.3)
 *
 * Reconstructed from UrT 4.3.4 QVM binary analysis.
 *
 * Cvar: ut_mrsentry — enables/disables the sentry in maps that use it.
 *
 * Assets confirmed in 4.3.4 QVM strings:
 *   models/mrsentry/mrsentryspindown.wav
 *   models/mrsentry/mrsentryspinup.wav
 *   models/mrsentry/mrsentryfire.wav
 *   models/mrsentry/mrsentry_blue  (skin)
 *   models/mrsentry/mrsentry_red   (skin)
 *   models/mrsentry/mrsentrybarrel.md3
 *   models/mrsentry/mrsentrybase.md3
 *
 * Entity type: ET_MR_SENTRY (defined in bg_public.h)
 *
 * The sentry scans for enemies within range, rotates to track them,
 * and fires. It is team-coloured and cannot be damaged by its own team.
 */

#include "g_local.h"

#define SENTRY_RANGE		768		/* max targeting range (units) */
#define SENTRY_FOV			120		/* field of view (degrees) */
#define SENTRY_FIRE_RATE	100		/* ms between shots */
#define SENTRY_DAMAGE		15		/* damage per bullet */
#define SENTRY_SPIN_UP_TIME	1500	/* ms to spin up before firing */

/*
 * MrSentry_FindTarget
 *
 * Scan for the nearest enemy within SENTRY_RANGE that is in line of sight.
 * Returns the target entity, or NULL if none found.
 */
static gentity_t *MrSentry_FindTarget( gentity_t *sentry ) {
	int		i;
	gentity_t	*ent, *best;
	float		dist, bestDist;
	vec3_t		dir;
	trace_t		tr;

	best     = NULL;
	bestDist = SENTRY_RANGE;

	for ( i = 0; i < level.maxclients; i++ ) {
		ent = &g_entities[i];
		if ( !ent->client ) {
			continue;
		}
		if ( ent->client->ps.stats[STAT_HEALTH] <= 0 ) {
			continue;
		}
		if ( ent->client->ps.pm_type == PM_DEAD ) {
			continue;
		}
		/* skip same team (team stored in s.generic1 for non-client entities) */
		if ( ent->client->sess.sessionTeam == sentry->s.generic1 ) {
			continue;
		}

		VectorSubtract( ent->r.currentOrigin, sentry->r.currentOrigin, dir );
		dist = VectorLength( dir );
		if ( dist >= bestDist ) {
			continue;
		}

		/* line of sight check */
		trap_Trace( &tr, sentry->r.currentOrigin, NULL, NULL,
			ent->r.currentOrigin, sentry->s.number, MASK_SHOT );
		if ( tr.entityNum != ent->s.number ) {
			continue;
		}

		best     = ent;
		bestDist = dist;
	}

	return best;
}

/*
 * MrSentry_Think
 *
 * Called every SENTRY_FIRE_RATE ms (or slower) when the sentry is active.
 * Find target, rotate toward it, fire.
 */
static void MrSentry_Think( gentity_t *self ) {
	gentity_t	*target;
	vec3_t		dir;
	vec3_t		angles;

	self->nextthink = level.time + SENTRY_FIRE_RATE;

	if ( !g_mrsentry.integer ) {
		return;
	}

	target = MrSentry_FindTarget( self );
	if ( !target ) {
		return;
	}

	/* rotate to face target */
	VectorSubtract( target->r.currentOrigin, self->r.currentOrigin, dir );
	{
		float _len;
		VectorNormalize( dir, _len );
		(void)_len;
	}
	vectoangles( dir, angles );
	VectorCopy( angles, self->s.apos.trBase );

	/* fire at target */
	G_Damage( target, self, self, dir, target->r.currentOrigin,
		SENTRY_DAMAGE, 0, UT_MOD_LR, HL_UNKNOWN );
}

/*
 * SP_ut_mrsentry
 *
 * Spawn function for "ut_mrsentry" map entity.
 * Place a Mr. Sentry turret at the entity's origin.
 */
void SP_ut_mrsentry( gentity_t *ent ) {
	if ( !g_mrsentry.integer ) {
		G_FreeEntity( ent );
		return;
	}

	ent->s.eType    = ET_MR_SENTRY;
	ent->s.generic1 = ent->spawnflags & 1 ? TEAM_BLUE : TEAM_RED;  /* team stored in generic1 */
	ent->takedamage  = qfalse;	/* sentries are indestructible in base 4.3 */
	ent->think       = MrSentry_Think;
	ent->nextthink   = level.time + SENTRY_SPIN_UP_TIME;

	VectorCopy( ent->s.origin, ent->r.currentOrigin );
	trap_LinkEntity( ent );
}
