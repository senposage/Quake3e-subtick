/**
 * Filename: cg_localents.c
 *
 * Copyright (C) 1999-2005 Id Software, Inc.
 * Copyright (C) 2000-2009 FrozenSand
 *
 * This file is part of Urban Terror source code.
 *
 */
#include "cg_local.h"
#include "../common/c_bmalloc.h"

//#define NEW_LE_MANAGER

#ifdef NEW_LE_MANAGER

 #define MAX_LOCAL_ENTITIES 2048

localEntity_t  *cg_freeLocalEntitiesList = 0;
localEntity_t  *cg_usedLocalEntitiesList = 0;
int	       cg_numLocalEntities	 = 0;

// Two LIFO list approach. Free list is filled at start.
// Entity allocation call moves one entity from the free list to the used list.
// Freeing an entity moves it from the used list to the free list.
// If free list is empty on allocation call, use the last used entity.

#else

 #define MAX_LOCAL_ENTITIES 800 // was 512 in UT

localEntity_t  *cg_freeLocalEntities	    = NULL;	// single linked list
localEntity_t  *cg_activeLocalEntitiesBegin = NULL;	// single linked list
localEntity_t  *cg_activeLocalEntitiesEnd   = NULL;	// single linked list

#endif

/*
==================
CG_FreeLocalEntity
==================
*/
#ifdef NEW_LE_MANAGER

void CG_FreeLocalEntity(localEntity_t *le)
{
	// If an invalid entity.
	if (!le)
	{
		CG_Error("CG_FreeLocalEntity argument is a null pointer.\n");
	}

	// If this is the first entity on the used list.
	if (le == cg_usedLocalEntitiesList)
	{
		if (cg_usedLocalEntitiesList->next)
		{
			cg_usedLocalEntitiesList       = cg_usedLocalEntitiesList->next;
			cg_usedLocalEntitiesList->prev = 0;
		}
		else
		{
			// Empty used entities list.
			cg_usedLocalEntitiesList = 0;
		}
	}
	else
	{
		// Remove entity from list.
		if (le->prev)
		{
			le->prev->next = le->next;
		}

		if (le->next)
		{
			le->next->prev = le->prev;
		}
	}

	// Clear entity.
	memset(le, 0, sizeof(localEntity_t));

	// Add entity to the free list.
	if (cg_freeLocalEntitiesList)
	{
		cg_freeLocalEntitiesList->prev = le;
		le->next		       = cg_freeLocalEntitiesList;
	}

	cg_freeLocalEntitiesList = le;
	cg_numLocalEntities--;
}

#else

/**
 * $(function)
 */
void CG_FreeLocalEntity(localEntity_t *le)
{
	// Make sure we move the active entity pointer if need be
	if (le == cg_activeLocalEntitiesBegin)
	{
		cg_activeLocalEntitiesBegin = cg_activeLocalEntitiesBegin->next;
	}

	if (le == cg_activeLocalEntitiesEnd)
	{
		cg_activeLocalEntitiesEnd = cg_activeLocalEntitiesEnd->prev;
	}

	// remove from the doubly linked active list
	if (le->prev)
	{
		le->prev->next = le->next;
	}

	if (le->next)
	{
		le->next->prev = le->prev;
	}

	// Add it to the free list
	le->next	     = cg_freeLocalEntities;
	le->prev	     = NULL;

	cg_freeLocalEntities = le;
}

#endif

/*
===================
CG_InitLocalEntities

This is called at startup and for tournement restarts
===================
*/
#ifdef NEW_LE_MANAGER

void CG_InitLocalEntities(void)
{
	localEntity_t  *prev;
	localEntity_t  *next;
	int	       i;

	cg_freeLocalEntitiesList = NULL;

	for (i = 0; i < MAX_LOCAL_ENTITIES; i++)
	{
		next = (localEntity_t *)bmalloc(sizeof(localEntity_t));

		// If an invalid pointer.
		if (!next)
		{
			CG_Error("Unable to allocate memory for local entity.\n");
		}

		memset(next, 0, sizeof(localEntity_t));

		// First loop.
		if (!cg_freeLocalEntitiesList)
		{
			cg_freeLocalEntitiesList = next;
		}
		else
		{
			next->prev = prev;
			prev->next = next;
		}

		prev = next;
	}

	cg_numLocalEntities = 0;
}

#else

/**
 * $(function)
 */
void CG_InitLocalEntities(void)
{
	int  i;

	// They have been allocated already, so free all the active ones
	if ((cg_freeLocalEntities != NULL) || (cg_activeLocalEntitiesBegin != NULL))
	{
		while (cg_activeLocalEntitiesBegin)
		{
			CG_FreeLocalEntity(cg_activeLocalEntitiesBegin);
		}

		return;
	}

	for (i = 0; i < MAX_LOCAL_ENTITIES; i++)
	{
		localEntity_t  *ent = (localEntity_t *)bmalloc(sizeof(localEntity_t));

		memset(ent, 0, sizeof(localEntity_t));
		ent->next	     = cg_freeLocalEntities;

		cg_freeLocalEntities = ent;
	}
}

#endif

/*
===================
CG_AllocLocalEntity

Will allways succeed, even if it requires freeing an old active entity
===================
*/
#ifdef NEW_LE_MANAGER

localEntity_t *CG_AllocLocalEntity(void)
{
	localEntity_t  *le;

	// If free entities list is empty.
	if (!cg_freeLocalEntitiesList)
	{
		// If used entities list is empty too.
		if (!cg_usedLocalEntitiesList)
		{
			CG_Error("Both used and free local entities lists are empty.\n");
		}

		// Free the first entity on the used list.
		CG_FreeLocalEntity(cg_usedLocalEntitiesList);

		// If free entities list is still empty.
		if (!cg_freeLocalEntitiesList)
		{
			CG_Error("Unable to free first entity from used local entities list.\n");
		}
	}

	// Get first entity from the free entities list.
	le = cg_freeLocalEntitiesList;

	if (cg_freeLocalEntitiesList->next)
	{
		cg_freeLocalEntitiesList       = cg_freeLocalEntitiesList->next;
		cg_freeLocalEntitiesList->prev = 0;
	}
	else
	{
		// Empty free entities list.
		cg_freeLocalEntitiesList = 0;
	}

	// Reset entity.
	le->prev = 0;
	le->next = 0;

	// Add entity to the used list.
	if (cg_usedLocalEntitiesList)
	{
		cg_usedLocalEntitiesList->prev = le;
		le->next		       = cg_usedLocalEntitiesList;
	}

	cg_usedLocalEntitiesList = le;
	cg_numLocalEntities++;

	return le;
}

#else

/**
 * $(function)
 */
localEntity_t *CG_AllocLocalEntity(void)
{
	localEntity_t  *le;

	// ENTITY LIST
	{
		//    int l;
//	localEntity_t* le;
//	for ( l = 0, le=cg_activeLocalEntitiesBegin; le; le=le->next, l++ );
//	   CG_Printf ( "    used	   = %i\n", l );
	}

	if (!cg_freeLocalEntities)
	{
		//if (!cg_activeLocalEntitiesEnd)
		//   CG_Error("local entities error\n");

		// no free entities, so free the one at the end of the chain
		// remove the oldest active entity
		CG_FreeLocalEntity(cg_activeLocalEntitiesEnd);
	}

	// Get the first free one
	le		     = cg_freeLocalEntities;

	cg_freeLocalEntities = cg_freeLocalEntities->next;

//   if (cg_freeLocalEntities->

	// Make sure its clear
	memset(le, 0, sizeof(*le));

	// First one ?
	if (NULL == cg_activeLocalEntitiesBegin)
	{
		cg_activeLocalEntitiesBegin = le;
		cg_activeLocalEntitiesEnd   = le;
	}
	else
	{
		cg_activeLocalEntitiesBegin->prev = le;
		le->next			  = cg_activeLocalEntitiesBegin;

		cg_activeLocalEntitiesBegin	  = le;
	}

	return le;
}

#endif

/*
===================
CG_CloneLocalEntity
===================
*/
localEntity_t *CG_CloneLocalEntity(localEntity_t *le)
{
	localEntity_t  *prev;
	localEntity_t  *next;
	localEntity_t  buf;

	// If an invalid entity.
	if (!le)
	{
		CG_Error("CG_CloneLocalEntity argument is a null pointer.\n");
	}

	// Copy contents.
	memcpy(&buf, le, sizeof(localEntity_t));

	// Allocate new entity.
	le   = CG_AllocLocalEntity();

	prev = le->prev;
	next = le->next;

	// Copy over contents and restore linked list position.
	memcpy(le, &buf, sizeof(localEntity_t));

	le->prev = prev;
	le->next = next;

	return le;
}

/*
====================================================================================

FRAGMENT PROCESSING

A fragment localentity interacts with the environment in some way (hitting walls),
or generates more localentities along a trail.

====================================================================================
*/

/*
================
CG_BloodTrail

Leave expanding blood puffs behind gibs
================
*/
void CG_BloodTrail( localEntity_t *le )
{
	int	t;
	int	t2;
	int	step;
	vec3_t	newOrigin;
	vec3_t	normal;
	vec3_t	from = { 0, 0, 10};
	float	angle;

	angle	  = (rand() % 100);

	normal[0] = sin( angle ) / 100;
	normal[1] = cos( angle ) / 100;
	normal[2] = -1;

	step	  = 50;
	t	  = step * ((cg.time - cg.frametime + step) / step);
	t2	  = step * (cg.time / step);

	for ( ; t <= t2; t += step)
	{
		BG_EvaluateTrajectory( &le->pos, t, newOrigin );

		le = CG_ParticleCreate( );
		CG_ParticleSetAttributes ( le, PART_ATTR_BLOOD, cgs.media.smokePuffShader, cgs.media.bloodMarkShader[rand() % 4], 0 );
//		CG_ParticleSetAttributes ( le, PART_ATTR_BLOOD, cgs.media.bloodMarkShader[rand()%4], cgs.media.bloodMarkShader[rand()%4], 0 );

		//CG_ParticleExplode(newOrigin, from, normal, 1+(rand()%1), 40.0f, le );
		CG_ParticleExplode(newOrigin, from, normal, 1, 40.0f, le );

/*		blood = CG_SmokePuff( newOrigin, vec3_origin,
					  0.1,		// radius
					  1, 1, 1, 1,	// color
					  1000, 	// trailTime
					  t,		// startTime
					  0,		// fadeInTime
					  0,		// flags
					  cgs.media.bloodTrailShader );
		// use the optimized version
		blood->leType = LE_FALL_SCALE_FADE;
		// drop a total of 40 units over its lifetime
		blood->pos.trDelta[2] = 40;*/
	}
}

/*
================
CG_FragmentBounceMark
================
*/
void CG_FragmentBounceMark( localEntity_t *le, trace_t *trace )
{
	float	    radius;
	vec3_t	    finalcolor;
	particle_t  *part = &p_attr[le->partType];

	if (part->flags & PART_FLAG_MARK)
	{
		if (part->markradius == 0.0f)
		{
			radius = le->radius;
		}
		else if (part->markradius < 0.0f)
		{
			radius = le->radius * -part->markradius;
		}
		else
		{
			radius = part->markradius;
		}

		BuildLightFloat(trace->endpos, part->markcolor, finalcolor);
		CG_ImpactMark ( le->partMarkShader,
				trace->endpos,
				trace->plane.normal,
				rand() % 360,
				finalcolor[0],
				finalcolor[1],
				finalcolor[2],
				part->markcolor[3],
				qtrue,
				radius,
				qfalse, NULL );

		//part->flags &= ~PART_FLAG_MARK;
		return;
	}

	if (le->leMarkType == LEMT_BLOOD)
	{
		// don't know if we still need this here
		radius = 16 + (rand() & 31);

		BuildLightFloat(trace->endpos, part->markcolor, finalcolor);

		CG_ImpactMark( cgs.media.bloodMarkShader[0], trace->endpos, trace->plane.normal, random() * 360,
			       finalcolor[0], finalcolor[1], finalcolor[2], 1, qtrue, radius, qfalse, NULL );
	}
	else if (le->leMarkType == LEMT_BURN)
	{
		radius = 8 + (rand() & 15);
		BuildLightFloat(trace->endpos, part->markcolor, finalcolor);

		CG_ImpactMark( cgs.media.burnMarkShader, trace->endpos, trace->plane.normal, random() * 360,
			       finalcolor[0], finalcolor[1], finalcolor[2], 1, qtrue, radius, qfalse, NULL );
	}
	else if (le->leMarkType == LEMT_SMALLBLOOD)
	{
		radius		   = 8 + (rand() & 15);
		part->markcolor[0] = 1;
		part->markcolor[1] = 0;
		part->markcolor[2] = 0;
		BuildLightFloat(trace->endpos, part->markcolor, finalcolor);
		CG_ImpactMark( cgs.media.bloodMarkShader[1], trace->endpos, trace->plane.normal, random() * 360,
			       finalcolor[0], finalcolor[1], finalcolor[2], 1, qtrue, radius, qfalse, NULL );
	}

	// don't allow a fragment to make multiple marks, or they
	// pile up while settling
	le->leMarkType = LEMT_NONE;
}

/*
================
CG_FragmentBounceSound
================
*/
void CG_FragmentBounceSound( localEntity_t *le, trace_t *trace )
{
	if (le->leBounceSoundType == LEBS_BLOOD)
	{
		// half the gibs will make splat sounds
		if (rand() & 1)
		{
			int	     r = rand() & 3;
			sfxHandle_t  s;

			if (r < 2)
			{
				s = cgs.media.gibBounce1Sound;
			}
			else if (r == 2)
			{
				s = cgs.media.gibBounce2Sound;
			}
			else
			{
				s = cgs.media.gibBounce3Sound;
			}
			trap_S_StartSound( trace->endpos, ENTITYNUM_WORLD, CHAN_AUTO, s );
		}
	}
	else if (le->leBounceSoundType == LEBS_GLASS)
	{
		// Dokta8 - a kind of tinkle effect
		if (rand() & 7)
			trap_S_StartSound ( trace->endpos, ENTITYNUM_WORLD, CHAN_AUTO, cgs.media.UT_glassBounceSound );
	}
	else if (le->leBounceSoundType == LEBS_WOOD)
	{
		trap_S_StartSound ( trace->endpos, ENTITYNUM_WORLD, CHAN_AUTO, cgs.media.UT_woodBounceSound );
	}
	else if (le->leBounceSoundType == LEBS_CERAMIC)
	{
		trap_S_StartSound ( trace->endpos, ENTITYNUM_WORLD, CHAN_AUTO, cgs.media.UT_ceramicBounceSound );
	}
	else if (le->leBounceSoundType == LEBS_PLASTIC)
	{
		trap_S_StartSound ( trace->endpos, ENTITYNUM_WORLD, CHAN_AUTO, cgs.media.UT_plasticBounceSound );
	}
	else if (le->bounceSound)
	{
		trap_S_StartSound ( trace->endpos, ENTITYNUM_WORLD, CHAN_FX, le->bounceSound );
	}

	// don't allow a fragment to make multiple bounce sounds,
	// or it gets too noisy as they settle
	le->leBounceSoundType = LEBS_NONE;
	le->bounceSound       = 0;
}

/*
================
CG_ReflectVelocity
================
*/
void CG_ReflectVelocity( localEntity_t *le, trace_t *trace )
{
	vec3_t	velocity;
	float	dot;
	int	hitTime;

	// reflect the velocity on the trace plane
	hitTime = cg.time - cg.frametime + cg.frametime * trace->fraction;
	BG_EvaluateTrajectoryDelta( &le->pos, hitTime, velocity );
	dot	= DotProduct( velocity, trace->plane.normal );
	VectorMA( velocity, -2 * dot, trace->plane.normal, le->pos.trDelta );

	VectorScale( le->pos.trDelta, le->bounceFactor, le->pos.trDelta );

	VectorCopy( trace->endpos, le->pos.trBase );
	le->pos.trTime = cg.time;

	// check for stop, making sure that even on low FPS systems it doesn't bobble
	if (trace->allsolid ||
	    ((trace->plane.normal[2] > 0) &&
	     ((le->pos.trDelta[2] < 40) || (le->pos.trDelta[2] < -cg.frametime * le->pos.trDelta[2]))))
	{
		le->pos.trType = TR_STATIONARY;
	}
	else
	{
	}
}

/*
================
CG_AddFragment
General purpose way to add
local entities to a scene
================
*/
void CG_AddFragment( localEntity_t *le )
{
	vec3_t	    ambientLight, dirLight, Lightdir;
	vec3_t	    newOrigin;
	trace_t     trace;
	vec3_t	    vel    = { 0, 0, 1};
	float	    c;
	qboolean    noShow = qfalse;

	particle_t  *part  = &p_attr[le->partType];

	trap_R_LightForPoint(le->refEntity.origin, ambientLight, dirLight, Lightdir );
	le->refEntity.shaderRGBA[0] = dirLight[0];
	le->refEntity.shaderRGBA[1] = dirLight[1];
	le->refEntity.shaderRGBA[2] = dirLight[2];
	le->refEntity.shaderRGBA[3] = 255;

	// This is a hack to ensure that the ejected bullets operate correctly
	if (le->refEntity.renderfx & RF_DEPTHHACK)
	{
		if (cg.zoomed)
		{
			noShow = qtrue;
		}

		if (cg.time - le->startTime > 200)
		{
			le->refEntity.renderfx &= (~RF_DEPTHHACK);
		}
	}

	// Change our alpha value if fading is on
	if(part->fade)
	{
		c			    = (float)( le->endTime - cg.time ) * le->lifeRate;
		le->refEntity.shaderRGBA[3] = 255.0f * c * le->color[3];
	}

	if (le->pos.trType == TR_STATIONARY)
	{
		if (!le->refEntity.hModel)
		{
			return;
		}

		// If we are stopped and this particle dies when stopped then free it
		if(part->flags & PART_FLAG_DIEONSTOP)
		{
			CG_FreeLocalEntity( le );
			return;
		}

		if (le->scale[0] || le->scale[1] || le->scale[2])
		{
			vec3_t	angles;

			if (le->leBounceSoundType != LEBS_GLASS)
			{
				// flatten fragment against ground
				if (le->scale[0] == 0.1f)
				{
					le->scale[0] = le->scale[2];
					le->scale[2] = 0.1f;
				}
				else if (le->scale[1] == 0.1f)
				{
					le->scale[1] = le->scale[2];
					le->scale[2] = 0.1f;
				}

				angles[0] = 0.0f;
				angles[1] = (le->scale[0] + le->scale[1] * 100.0f);
				angles[2] = 0.0f;

				AnglesToAxis( angles, le->refEntity.axis );

				VectorScale( le->refEntity.axis[0], le->scale[0], le->refEntity.axis[0] );
				VectorScale( le->refEntity.axis[1], le->scale[1], le->refEntity.axis[1] );
				VectorScale( le->refEntity.axis[2], le->scale[2], le->refEntity.axis[2] );
				le->refEntity.nonNormalizedAxes = qtrue;
			}
			else
			{
				vec3_t	angles = { 0, 0, 0};
				angles[2] = 90;
				angles[1] = le->startTime % 360;
				AnglesToAxis( angles, le->refEntity.axis );

				VectorScale( le->refEntity.axis[0], le->scale[0], le->refEntity.axis[0] );
				VectorScale( le->refEntity.axis[1], le->scale[1], le->refEntity.axis[1] );
				VectorScale( le->refEntity.axis[2], le->scale[2], le->refEntity.axis[2] );
				le->refEntity.nonNormalizedAxes = qtrue;
			}
		}

		if (le->leFlags & LEF_DONTRENDER)
		{
			noShow = qtrue;
		}

		if (!noShow)
		{
			trap_R_AddRefEntityToSceneX( &le->refEntity );
		}

		return;
	}

	// Hack for custom gravity
	if((le->pos.trType == TR_GRAVITY2) || (le->pos.trType == TR_FEATHER))
	{
		trGravity = le->gravity;
	}

	// calculate new position
	BG_EvaluateTrajectory( &le->pos, cg.time, newOrigin );

	trGravity = 1.0f;

	// trace a line from previous position to new position
	CG_Trace( &trace, le->refEntity.origin, NULL, NULL, newOrigin, -1, CONTENTS_SOLID );

	if ((trace.fraction == 1.0) || (trace.contents != 1) || (cg.time - le->startTime < 100))
	{
		// still in free fall
		VectorCopy( newOrigin, le->refEntity.origin );

		if ((part->flags & PART_FLAG_FLOATS) && (trap_CM_PointContents( trace.endpos, 0 ) & CONTENTS_WATER))
		{
			CG_FragmentBounceMark( le, &trace );
			CG_FreeLocalEntity( le );
			return;
		}

		if (le->leFlags & LEF_TUMBLE)
		{
			vec3_t	angles;

			BG_EvaluateTrajectory( &le->angles, cg.time, angles );
			AnglesToAxis( angles, le->refEntity.axis );

			// Dokta8: scale up along axes if re is a non-window breakable
			if (le->scale[0] || le->scale[1] || le->scale[2])
			{
				VectorScale( le->refEntity.axis[0], le->scale[0], le->refEntity.axis[0] );
				VectorScale( le->refEntity.axis[1], le->scale[1], le->refEntity.axis[1] );
				VectorScale( le->refEntity.axis[2], le->scale[2], le->refEntity.axis[2] );
				le->refEntity.nonNormalizedAxes = qtrue;
			}
		}

		if (part->flags & PART_FLAG_TRAIL)
		{
			CG_ParticleSmoke ( le, cgs.media.shotgunSmokePuffShader );
		}

		if (le->leFlags & LEF_PARTICLE)
		{
			if (le->refEntity.hModel)
			{
				if (le->leFlags & LEF_DONTRENDER)
				{
					noShow = qtrue;
				}

				trap_R_AddRefEntityToSceneX( &le->refEntity );
			}
			else
			{
				CG_AddParticleToScene ( le );
			}
		}
		else if (le->leFlags & LEF_SHARD)
		{
			CG_AddShardToScene( le );
		}
		else
		{
			if (le->leFlags & LEF_DONTRENDER)
			{
				noShow = qtrue;
			}

			if (!noShow)
			{
				trap_R_AddRefEntityToSceneX( &le->refEntity );
			}
		}

		// add a blood trail
		if (le->leFlags & LEF_BLOODTRAIL)
		{
			if (cg.time > le->timeTilNextChild)
			{
				CG_SmokePuff(le->refEntity.origin, vel, 1 + rand() % 1, 1, 1, 1, 0.66f, 200 + rand() % 100, cg.time, cg.time, LEF_DIEONIMPACT, cgs.media.bloodMarkShader[rand() % 4] );
				le->timeTilNextChild = cg.time + 50;
			}
		}

		return;
	}	// end of "still in free fall"

	// get to here, and le is not in freefall, so it hit something

	// if it is in a nodrop zone, remove it
	// this keeps gibs from waiting at the bottom of pits of death
	// and floating levels
/*	if ( trap_CM_PointContents( trace.endpos, 0 ) & CONTENTS_NODROP )
	{
		CG_FreeLocalEntity( le );
		return;
	}*/

	// leave a mark
	if (!trace.allsolid && !trace.startsolid)
	{
		CG_FragmentBounceMark( le, &trace ); //fps bug fix
	}

	// #27 - for the blood jet gibs
	if (le->leFlags & LEF_DIEONIMPACT)
	{
		CG_FreeLocalEntity(le);
		return;
	}

	// No bounce?  then dont bounce it
	if((le->partType == PART_ATTR_NONE) || (part->flags & PART_FLAG_BOUNCE))
	{
		// do a bouncy sound
		CG_FragmentBounceSound( le, &trace );

		// reflect the velocity on the trace plane
		CG_ReflectVelocity( le, &trace );
	}
	else
	{
		le->pos.trType	  = TR_STATIONARY;
		le->angles.trType = TR_STATIONARY;
	}

	if (le->leFlags & LEF_PARTICLE)
	{
		if (le->refEntity.hModel)
		{
			trap_R_AddRefEntityToSceneX( &le->refEntity );
		}
		else
		{
			CG_AddParticleToScene ( le );
		}
	}
	else if (le->leFlags & LEF_SHARD)
	{
		CG_AddShardToScene( le );
	}
	else
	{
		if (le->scale[0] || le->scale[1] || le->scale[2])
		{
			vec3_t	angles;

			BG_EvaluateTrajectory( &le->angles, cg.time, angles );
			AnglesToAxis( angles, le->refEntity.axis );

			VectorScale( le->refEntity.axis[0], le->scale[0], le->refEntity.axis[0] );
			VectorScale( le->refEntity.axis[1], le->scale[1], le->refEntity.axis[1] );
			VectorScale( le->refEntity.axis[2], le->scale[2], le->refEntity.axis[2] );
			le->refEntity.nonNormalizedAxes = qtrue;
		}

		if (le->leFlags & LEF_DONTRENDER)
		{
			noShow = qtrue;
		}

		if (!noShow)
		{
			trap_R_AddRefEntityToSceneX( &le->refEntity );
		}
	}
}

/*
	CG_Line
	Draws a line in 3D space
	Used for rain, but can also be a useful debugging tool
*/
void CG_Line( vec3_t source, vec3_t dest, float radius, qhandle_t shader, vec4_t srccol, vec4_t destcol )
{
	vec3_t	       forward, right;
	polyVert_t     verts[4];
	vec3_t	       line;
	float	       len;
	unsigned char  c1[4], c2[4];
	int	       i;

	// tracer
	VectorSubtract( dest, source, forward );
	VectorNormalize( forward,len );

	// don't draw really short lines
	if (len < 1)
	{
		return;
	}

	// set colour of line
	for (i = 0; i < 4; i++)
	{
		c1[i] =  (unsigned char)(destcol[i] * 255.0f);
		c2[i] =  (unsigned char)(srccol[i] * 255.0f);
	}

	// constuct line building coords
	line[0] = DotProduct( forward, cg.refdef.viewaxis[1] );
	line[1] = DotProduct( forward, cg.refdef.viewaxis[2] );

	VectorScale( cg.refdef.viewaxis[1], line[1], right );
	VectorMA( right, -line[0], cg.refdef.viewaxis[2], right );
	VectorNormalizeNR( right );

	VectorMA( dest, radius / 2, right, verts[0].xyz );
	verts[0].st[0]	     = 0;
	verts[0].st[1]	     = 0;
	verts[0].modulate[0] = c2[0];
	verts[0].modulate[1] = c2[1];
	verts[0].modulate[2] = c2[2];
	verts[0].modulate[3] = c2[3];

	VectorMA( dest, -radius / 2, right, verts[1].xyz );
	verts[1].st[0]	     = 0;
	verts[1].st[1]	     = 0.2f;
	verts[1].modulate[0] = c2[0];
	verts[1].modulate[1] = c2[1];
	verts[1].modulate[2] = c2[2];
	verts[1].modulate[3] = c2[3];

	VectorMA( source, -radius / 2, right, verts[2].xyz );
	verts[2].st[0]	     = len / 40;
	verts[2].st[1]	     = 0;
	verts[2].modulate[0] = c1[0];
	verts[2].modulate[1] = c1[1];
	verts[2].modulate[2] = c1[2];
	verts[2].modulate[3] = c1[3];

	VectorMA( source, radius / 2, right, verts[3].xyz );
	verts[3].st[0]	     = len / 40;
	verts[3].st[1]	     = 0.2f;
	verts[3].modulate[0] = c1[0];
	verts[3].modulate[1] = c1[1];
	verts[3].modulate[2] = c1[2];
	verts[3].modulate[3] = c1[3];

	if (shader == 0)
	{
		shader = cgs.media.whiteShader ;
	}
	trap_R_AddPolyToScene( shader, 4, verts );
}

// emitter: a particle generator
/*void CG_AddEmitter ( localEntity_t *le ) {
	vec3_t	origin;
	trace_t trace;
	int interval;
	int deviation = 0;

	// emitters do not actually render, they just create
	// particles.  The origin in refentity is used to
	// store the position of the entity the last time
	// cg_addemitter was called for collision detection
	// using the trace (bouncing)

	// immediately free emitter if it has finished its job
	if (!le->emit.total_particles) {
		// I DONT KNOW WHY, BUT DO THIS FOR NOW
		CG_FreeLocalEntity( le );
		return;
	}

	trGravity = 1.0f;	// set gravity of emitter first!! (it may be using a dependent trtype)
	BG_EvaluateTrajectory( &le->pos, cg.time, origin );

	// emit a particle if time
	if ( cg.time > le->emit.nextEmitTime ) {
		//BG_EvaluateTrajectory( &le->pos, cg.time, origin );
		CG_ParticleLaunch( origin, le->emit.velocity, &le );

		interval = (le->endTime - le->startTime) / le->emit.total_particles;
		if ( interval ) {
			deviation = (rand() % interval) * le->emit.deviation;
		}

		le->emit.nextEmitTime = le->startTime + (interval * le->emit.current_particle) + deviation;
		le->emit.current_particle++;
	}

	// immediately free emitter if it has finished its job
	if (le->emit.current_particle >= le->emit.total_particles) {
		CG_FreeLocalEntity( le );
		return;
	}

	// did the move intersect with anything since last move?
	CG_Trace( &trace, le->refEntity.origin, NULL, NULL, origin, -1, CONTENTS_SOLID );

	// no? set origin and return
	if ( trace.fraction >= 1.0f ) {
		VectorCopy ( origin, le->refEntity.origin );
		return;
	}

	// yes -> bounce
	CG_ReflectVelocity( le, &trace );

}*/

/*
=====================================================================

TRIVIAL LOCAL ENTITIES

These only do simple scaling or modulation before passing to the renderer
=====================================================================
*/

/*
====================
CG_AddFadeRGB
====================
*/
void CG_AddFadeRGB( localEntity_t *le )
{
	refEntity_t  *re;
	float	     c;

	re		  = &le->refEntity;

	c		  = (le->endTime - cg.time) * le->lifeRate;
	c		 *= 0xff;

	re->shaderRGBA[0] = le->color[0] * c;
	re->shaderRGBA[1] = le->color[1] * c;
	re->shaderRGBA[2] = le->color[2] * c;
	re->shaderRGBA[3] = le->color[3] * c;

	trap_R_AddRefEntityToSceneX( re );
}

/*
==================
CG_AddMoveScaleFade
==================
*/
static void CG_AddMoveScaleFade( localEntity_t *le )
{
	refEntity_t  *re;
	float	     c;
	vec3_t	     delta;
	vec3_t	     oldcolor;
	float	     len;
	int	     alpha;

	re = &le->refEntity;

	if ((le->fadeInTime > le->startTime) && (cg.time < le->fadeInTime))
	{
		// fade / grow time
		c = 1.0 - (float)( le->fadeInTime - cg.time ) / (le->fadeInTime - le->startTime);
	}
	else
	{
		// fade / grow time
		c = (le->endTime - cg.time) * le->lifeRate;
	}

	re->shaderRGBA[3] = 0xff * c * le->color[3];

	if (!(le->leFlags & LEF_PUFF_DONT_SCALE))
	{
		re->radius = le->radius * (1.0f - c) + 4.0f;
	}

	if (le->leFlags & LEF_PARTICLE)
	{
		re->radius = le->radius + (3 * le->radius * (1.0f - c));

		if (re->radius < 0.01f)
		{
			re->radius = 0.01f;
		}
	}

	BG_EvaluateTrajectory( &le->pos, cg.time, re->origin );

	if (le->leFlags & LEF_CLIP)
	{
//		CG_Printf("3: Eval clipping\n");
		VectorSubtract(re->origin, le->angles.trBase, delta);

		if (VectorLength(delta) > le->angles.trTime)
		{
			CG_FreeLocalEntity( le );
			return;
		}
	}

/*   if (le->leFlags & LEF_DIEONIMPACT )
   {
      CG_Trace( &trace, le->pos.trBase, NULL, NULL, re->origin, -1, CONTENTS_SOLID );
	   if ((trace.allsolid)||(trace.fraction < 1.0 ))
      {
	 CG_FragmentBounceMark(le,&trace);
	 CG_FreeLocalEntity( le );

      }





   }*/

	// if the view would be "inside" the sprite, kill the sprite
	// so it doesn't add too much overdraw
	VectorSubtract( re->origin, cg.refdef.vieworg, delta );
	len = VectorLength( delta );

	//Different style of overdraw
	if (le->leFlags & LEF_ALLOWOVERDRAW)
	{
		if ((rand() % 100 > 70) && (re->shaderRGBA[3] < 32)) // Kill off the ones close to transparency
		{
			CG_FreeLocalEntity( le );
			return;
		}

		if ((len < 300) && (le->endTime - cg.time < 100))  //kill really big almost transparent ones anyway
		{
			CG_FreeLocalEntity( le );
			return;
		}

		//view overlay
		if ((len < 150) && (cg.grensmokecolor[3] > 0.8))
		{
			return;
		}
	}
	else
	if (len < le->radius)
	{
		CG_FreeLocalEntity( le );
		return;
	}

	if (le->leFlags & LEF_SHOWLASER)
	{
		CG_MarkSmokeForLaser(le);

		//up the opacity
		alpha = re->shaderRGBA[3] * 2;

		if (alpha > 255)
		{
			alpha = 255;
		}
		re->shaderRGBA[3]    = alpha;

		le->lightColor[0]    = re->shaderRGBA[3];
		le->angles.trBase[0] = re->origin[0];
		le->angles.trBase[1] = re->origin[1];
		le->angles.trBase[2] = re->origin[2];
		le->lightColor[1]    = re->radius;
	}

	oldcolor[0] = re->shaderRGBA[0];
	oldcolor[1] = re->shaderRGBA[1];
	oldcolor[2] = re->shaderRGBA[2];

	BuildLightByte(re->origin, oldcolor, (char *)re->shaderRGBA);
	//trap_R_LightForPoint( re->origin,ambientLight, dirLight, Lightdir );

	//re->shaderRGBA[0]=((float)(dirLight[0]/255)*(float)re->shaderRGBA[0]/255)*255;
	//re->shaderRGBA[1]=((float)(dirLight[1]/255)*(float)re->shaderRGBA[1]/255)*255;
	//re->shaderRGBA[2]=((float)(dirLight[2]/255)*(float)re->shaderRGBA[2]/255)*255;

////////////////
	trap_R_AddRefEntityToSceneX( re );
/////////
	re->shaderRGBA[0] = oldcolor[0];
	re->shaderRGBA[1] = oldcolor[1];
	re->shaderRGBA[2] = oldcolor[2];
}

/**
 * $(function)
 */
void CG_DrawBBAxis( vec3_t centervec, vec3_t mins, vec3_t maxs, float thickness, vec3_t axis[3])
{
	vec3_t	top1, top2, top3, top4;
	vec3_t	bop1, bop2, bop3, bop4;

//   int j;
	vec4_t	white = { 0.7f, 0.8f, 1.0f, 0.5f };

	/*top1[0]=centervec[0]+mins[0];
	  top1[1]=centervec[1]+mins[1];
	  top1[2]=centervec[2]+mins[2];

	  top2[0]=centervec[0]+maxs[0];
	  top2[1]=centervec[1]+mins[1];
	  top2[2]=centervec[2]+mins[2];

	  top3[0]=centervec[0]+maxs[0];
	  top3[1]=centervec[1]+maxs[1];
	  top3[2]=centervec[2]+mins[2];

	  top4[0]=centervec[0]+mins[0];
	  top4[1]=centervec[1]+maxs[1];
	  top4[2]=centervec[2]+mins[2]; */

	top1[0] = mins[0];
	top1[1] = mins[1];
	top1[2] = mins[2];

	top2[0] = maxs[0];
	top2[1] = mins[1];
	top2[2] = mins[2];

	top3[0] = maxs[0];
	top3[1] = maxs[1];
	top3[2] = mins[2];

	top4[0] = mins[0];
	top4[1] = maxs[1];
	top4[2] = mins[2];

	ApplyRotationToPoint(axis, top1);
	ApplyRotationToPoint(axis, top2);
	ApplyRotationToPoint(axis, top3);
	ApplyRotationToPoint(axis, top4);

	top1[0] += centervec[0];
	top1[1] += centervec[1];
	top1[2] += centervec[2];

	top2[0] += centervec[0];
	top2[1] += centervec[1];
	top2[2] += centervec[2];

	top3[0] += centervec[0];
	top3[1] += centervec[1];
	top3[2] += centervec[2];

	top4[0] += centervec[0];
	top4[1] += centervec[1];
	top4[2] += centervec[2];

	bop1[0]  = mins[0];
	bop1[1]  = mins[1];
	bop1[2]  = maxs[2];

	bop2[0]  = maxs[0];
	bop2[1]  = mins[1];
	bop2[2]  = maxs[2];

	bop3[0]  = maxs[0];
	bop3[1]  = maxs[1];
	bop3[2]  = maxs[2];

	bop4[0]  = mins[0];
	bop4[1]  = maxs[1];
	bop4[2]  = maxs[2];

	ApplyRotationToPoint(axis, bop1);
	ApplyRotationToPoint(axis, bop2);
	ApplyRotationToPoint(axis, bop3);
	ApplyRotationToPoint(axis, bop4);

	bop1[0] += centervec[0];
	bop1[1] += centervec[1];
	bop1[2] += centervec[2];

	bop2[0] += centervec[0];
	bop2[1] += centervec[1];
	bop2[2] += centervec[2];

	bop3[0] += centervec[0];
	bop3[1] += centervec[1];
	bop3[2] += centervec[2];

	bop4[0] += centervec[0];
	bop4[1] += centervec[1];
	bop4[2] += centervec[2];

	CG_Line(bop1, bop2, thickness, cgs.media.whiteShader, white, white );
	CG_Line(bop2, bop3, thickness, cgs.media.whiteShader, white, white );
	CG_Line(bop3, bop4, thickness, cgs.media.whiteShader, white, white );
	CG_Line(bop4, bop1, thickness, cgs.media.whiteShader, white, white );
	////
	CG_Line(top1, top2, thickness, cgs.media.whiteShader, white, white );
	CG_Line(top2, top3, thickness, cgs.media.whiteShader, white, white );
	CG_Line(top3, top4, thickness, cgs.media.whiteShader, white, white );
	CG_Line(top4, top1, thickness, cgs.media.whiteShader, white, white );
/////////////////
	CG_Line(top1, bop1, thickness, cgs.media.whiteShader, white, white );
	CG_Line(top2, bop2, thickness, cgs.media.whiteShader, white, white );
	CG_Line(top3, bop3, thickness, cgs.media.whiteShader, white, white );
	CG_Line(top4, bop4, thickness, cgs.media.whiteShader, white, white );
}

/**
 * $(function)
 */
void CG_DrawBB( vec3_t centervec, vec3_t mins, vec3_t maxs, float thickness, int red, int green, int blue)
{
	vec3_t	line1, line2;
	vec4_t	white;

	white[0] = red / 255;
	white[1] = green / 255;
	white[2] = blue / 255;
	white[3] = 0.5;

	// Top left
	line1[0] = centervec[0] + mins[0];
	line1[1] = centervec[1] + mins[1];
	line1[2] = centervec[2] + mins[2];

	line2[0] = centervec[0] + maxs[0];
	line2[1] = centervec[1] + mins[1];
	line2[2] = centervec[2] + mins[2];
	CG_Line(line1, line2, thickness, cgs.media.whiteShader, white, white );

	line2[0] = centervec[0] + mins[0];
	line2[1] = centervec[1] + maxs[1];
	line2[2] = centervec[2] + mins[2];
	CG_Line(line1, line2, thickness, cgs.media.whiteShader, white, white );
	line2[0] = centervec[0] + mins[0];
	line2[1] = centervec[1] + mins[1];
	line2[2] = centervec[2] + maxs[2];
	CG_Line(line1, line2, thickness, cgs.media.whiteShader, white, white );

	// Top left
	line1[0] = centervec[0] + maxs[0];
	line1[1] = centervec[1] + maxs[1];
	line1[2] = centervec[2] + maxs[2];

	line2[0] = centervec[0] + mins[0];
	line2[1] = centervec[1] + maxs[1];
	line2[2] = centervec[2] + maxs[2];
	CG_Line(line1, line2, thickness, cgs.media.whiteShader, white, white );
	line2[0] = centervec[0] + maxs[0];
	line2[1] = centervec[1] + mins[1];
	line2[2] = centervec[2] + maxs[2];
	CG_Line(line1, line2, thickness, cgs.media.whiteShader, white, white );
	line2[0] = centervec[0] + maxs[0];
	line2[1] = centervec[1] + maxs[1];
	line2[2] = centervec[2] + mins[2];
	CG_Line(line1, line2, thickness, cgs.media.whiteShader, white, white );

	// bot left
	line1[0] = centervec[0] + mins[0];
	line1[1] = centervec[1] + mins[1];
	line1[2] = centervec[2] + maxs[2];

	line2[0] = centervec[0] + maxs[0];
	line2[1] = centervec[1] + mins[1];
	line2[2] = centervec[2] + maxs[2];
	CG_Line(line1, line2, thickness, cgs.media.whiteShader, white, white );

	line2[0] = centervec[0] + mins[0];
	line2[1] = centervec[1] + maxs[1];
	line2[2] = centervec[2] + maxs[2];
	CG_Line(line1, line2, thickness, cgs.media.whiteShader, white, white );

	// bot left
	line1[0] = centervec[0] + mins[0];
	line1[1] = centervec[1] + maxs[1];
	line1[2] = centervec[2] + mins[2];

	line2[0] = centervec[0] + mins[0];
	line2[1] = centervec[1] + maxs[1];
	line2[2] = centervec[2] + maxs[2];
	CG_Line(line1, line2, thickness, cgs.media.whiteShader, white, white );

	line2[0] = centervec[0] + maxs[0];
	line2[1] = centervec[1] + maxs[1];
	line2[2] = centervec[2] + mins[2];
	CG_Line(line1, line2, thickness, cgs.media.whiteShader, white, white );

	// bot left
	line1[0] = centervec[0] + maxs[0];
	line1[1] = centervec[1] + mins[1];
	line1[2] = centervec[2] + mins[2];

	line2[0] = centervec[0] + maxs[0];
	line2[1] = centervec[1] + maxs[1];
	line2[2] = centervec[2] + mins[2];
	CG_Line(line1, line2, thickness, cgs.media.whiteShader, white, white );

	line2[0] = centervec[0] + maxs[0];
	line2[1] = centervec[1] + mins[1];
	line2[2] = centervec[2] + maxs[2];
	CG_Line(line1, line2, thickness, cgs.media.whiteShader, white, white );
}

/*
===================
CG_AddScaleFade

For rocket smokes that hang in place, fade out, and are
removed if the view passes through them.
There are often many of these, so it needs to be simple.
===================
*/
static void CG_AddScaleFade( localEntity_t *le )
{
	refEntity_t  *re;
	float	     c;
	vec3_t	     delta;
	float	     len;

	re = &le->refEntity;

	// fade / grow time
	c		  = (le->endTime - cg.time) * le->lifeRate;

	re->shaderRGBA[3] = 0xff * c * le->color[3];
	re->radius	  = le->radius * (1.0 - c) + 8;

	// if the view would be "inside" the sprite, kill the sprite
	// so it doesn't add too much overdraw
	VectorSubtract( re->origin, cg.refdef.vieworg, delta );
	len = VectorLength( delta );

	if (len < le->radius)
	{
		CG_FreeLocalEntity( le );
		return;
	}

	trap_R_AddRefEntityToSceneX( re );
}

/*
=================
CG_AddFallScaleFade

This is just an optimized CG_AddMoveScaleFade
For blood mists that drift down, fade out, and are
removed if the view passes through them.
There are often 100+ of these, so it needs to be simple.
=================
*/
static void CG_AddFallScaleFade( localEntity_t *le )
{
	refEntity_t  *re;
	float	     c;
	vec3_t	     delta;
	float	     len;

	re = &le->refEntity;

	// fade time
	c		  = (le->endTime - cg.time) * le->lifeRate;

	re->shaderRGBA[3] = 0xff * c * le->color[3];

	re->origin[2]	  = le->pos.trBase[2] - (1.0 - c) * le->pos.trDelta[2];

	re->radius	  = le->radius * (1.0 - c) + 16;

	// if the view would be "inside" the sprite, kill the sprite
	// so it doesn't add too much overdraw
	VectorSubtract( re->origin, cg.refdef.vieworg, delta );
	len = VectorLength( delta );

	if (len < le->radius)
	{
		CG_FreeLocalEntity( le );
		return;
	}

	trap_R_AddRefEntityToSceneX( re );
}

/*
================
CG_AddExplosion
================
*/
static void CG_AddExplosion( localEntity_t *ex )
{
	refEntity_t  *ent;

	ent = &ex->refEntity;

	// add the entity
	trap_R_AddRefEntityToSceneX(ent);

	// add the dlight
	if (ex->light)
	{
		float  light;

		light = (float)( cg.time - ex->startTime ) / (ex->endTime - ex->startTime);

		if (light < 0.5)
		{
			light = 1.0;
		}
		else
		{
			light = 1.0 - (light - 0.5) * 2;
		}
		light = ex->light * light;
		trap_R_AddLightToScene(ent->origin, light, ex->lightColor[0], ex->lightColor[1], ex->lightColor[2] );
	}
}

/*
================
CG_AddSpriteExplosion
================
*/
static void CG_AddSpriteExplosion( localEntity_t *le )
{
	refEntity_t  re;
	float	     c;

	re = le->refEntity;

	c  = (le->endTime - cg.time) / (float)( le->endTime - le->startTime );

	if (c > 1)
	{
		c = 1.0;	// can happen during connection problems
	}

	re.shaderRGBA[0] = 0xff;
	re.shaderRGBA[1] = 0xff;
	re.shaderRGBA[2] = 0xff;
	re.shaderRGBA[3] = 0xff * c * 0.33;

	re.reType	 = RT_SPRITE;
	re.radius	 = re.radius * (1.0 - c) ;

	trap_R_AddRefEntityToSceneX( &re );

	// add the dlight
	if (le->light)
	{
		float  light;

		light = (float)( cg.time - le->startTime ) / (le->endTime - le->startTime);

		if (light < 0.5)
		{
			light = 1.0;
		}
		else
		{
			light = 1.0 - (light - 0.5) * 2;
		}
		light = le->light * light;
		trap_R_AddLightToScene(re.origin, light, le->lightColor[0], le->lightColor[1], le->lightColor[2] );
	}
}

/*
===================
CG_AddRefEntity
===================
*/
void CG_AddRefEntity( localEntity_t *le )
{
	if (le->endTime < cg.time)
	{
		CG_FreeLocalEntity( le );
		return;
	}
	trap_R_AddRefEntityToSceneX( &le->refEntity );
}

/*
===================
CG_AddScorePlum
===================
*/
#define NUMBER_SIZE 8

/**
 * $(function)
 */
void CG_AddScorePlum( localEntity_t *le )
{
	refEntity_t  *re;
	vec3_t	     origin, delta, dir, vec, up = { 0, 0, 1};
	float	     c, len;
	int	     i, score, digits[10], numdigits, negative;

	re    = &le->refEntity;

	c     = (le->endTime - cg.time) * le->lifeRate;

	score = le->radius;

	if (score < 0)
	{
		re->shaderRGBA[0] = 0xff;
		re->shaderRGBA[1] = 0x11;
		re->shaderRGBA[2] = 0x11;
	}
	else
	{
		re->shaderRGBA[0] = 0xff;
		re->shaderRGBA[1] = 0xff;
		re->shaderRGBA[2] = 0xff;

		if (score >= 50)
		{
			re->shaderRGBA[1] = 0;
		}
		else if (score >= 20)
		{
			re->shaderRGBA[0] = re->shaderRGBA[1] = 0;
		}
		else if (score >= 10)
		{
			re->shaderRGBA[2] = 0;
		}
		else if (score >= 2)
		{
			re->shaderRGBA[0] = re->shaderRGBA[2] = 0;
		}
	}

	if (c < 0.25)
	{
		re->shaderRGBA[3] = 0xff * 4 * c;
	}
	else
	{
		re->shaderRGBA[3] = 0xff;
	}

	re->radius = NUMBER_SIZE / 2;

	VectorCopy(le->pos.trBase, origin);
	origin[2] += 110 - c * 100;

	VectorSubtract(cg.refdef.vieworg, origin, dir);
	CrossProduct(dir, up, vec);
	VectorNormalizeNR(vec);

	VectorMA(origin, -10 + 20 * sin(c * 2 * M_PI), vec, origin);

	// if the view would be "inside" the sprite, kill the sprite
	// so it doesn't add too much overdraw
	VectorSubtract( origin, cg.refdef.vieworg, delta );
	len = VectorLength( delta );

	if (len < 20)
	{
		CG_FreeLocalEntity( le );
		return;
	}

	negative = qfalse;

	if (score < 0)
	{
		negative = qtrue;
		score	 = -score;
	}

	for (numdigits = 0; !(numdigits && !score); numdigits++)
	{
		digits[numdigits] = score % 10;
		score		  = score / 10;
	}

	if (negative)
	{
		digits[numdigits] = 10;
		numdigits++;
	}

	for (i = 0; i < numdigits; i++)
	{
		VectorMA(origin, (float)(((float)numdigits / 2) - i) * NUMBER_SIZE, vec, re->origin);
		re->customShader = cgs.media.numberShaders[digits[numdigits - 1 - i]];
		trap_R_AddRefEntityToSceneX( re );
	}
}

//==============================================================================

/*
===================
CG_AddLocalEntities

===================
*/

/*
void CG_AddLocalEntities( void )
{
	localEntity_t*	le;
	localEntity_t*	prev;

	// walk the list backwards, so any new local entities generated
	// (trails, marks, etc) will be present this frame
	le = cg_activeLocalEntitiesEnd;

	if (le==NULL) return;

	for ( ; le; le = prev )
	{
		// grab next now, so if the local entity is freed we
		// still have it
		prev = le->prev;

		// If we're paused.
		if (cg.pauseState & UT_PAUSE_ON)
		{
			// Adjust times for pause.
			le->startTime += cg.frametime;
			le->fadeInTime += cg.frametime;
			le->endTime += cg.frametime;
			le->pos.trTime += cg.frametime;
			le->angles.trTime += cg.frametime;
		}

		if ( cg.time >= le->endTime ) {
			CG_FreeLocalEntity( le );
			continue;
		}
		switch ( le->leType ) {
		default:
			CG_Error( "Bad leType: %i", le->leType );
			break;

		case LE_MARK:
			break;

		case LE_SPRITE_EXPLOSION:
			CG_AddSpriteExplosion( le );
			break;

		case LE_EXPLOSION:
			CG_AddExplosion( le );
			break;

		case LE_FRAGMENT:			// gibs and brass
			CG_AddFragment( le );
			break;

		case LE_MOVE_SCALE_FADE:		// water bubbles
			CG_AddMoveScaleFade( le );
			break;

		case LE_FADE_RGB:				// teleporters, railtrails
			CG_AddFadeRGB( le );
			break;

		case LE_FALL_SCALE_FADE: // gib blood trails
			CG_AddFallScaleFade( le );
			break;

		case LE_SCALE_FADE:		// rocket trails
			CG_AddScaleFade( le );
			break;

		case LE_SCOREPLUM:
			CG_AddScorePlum( le );
			break;

		//case LE_EMITTER:
		//	CG_AddEmitter( le );
			break;
		}
	}
}*/

void CG_AddLocalEntities(void)
{
	localEntity_t  *le;

#ifdef NEW_LE_MANAGER
	localEntity_t  *next;
#else
	localEntity_t  *prev;
#endif

#ifdef NEW_LE_MANAGER
	//CG_Printf("%d\n", cg_numLocalEntities);

	le = cg_usedLocalEntitiesList;
#else
	// walk the list backwards, so any new local entities generated
	// (trails, marks, etc) will be present this frame
	le = cg_activeLocalEntitiesEnd;
#endif

	if (le == NULL)
	{
		return;
	}

#ifdef NEW_LE_MANAGER
	for (; le; le = next)
#else
	for (; le; le = prev)
#endif
	{
		// grab next now, so if the local entity is freed we
		// still have it

#ifdef NEW_LE_MANAGER
		next = le->next;
#else
		prev = le->prev;
#endif

		// If we're paused.
		if (cg.pauseState & UT_PAUSE_ON)
		{
			// Adjust times for pause.
			le->startTime	  += cg.frametime;
			le->fadeInTime	  += cg.frametime;
			le->endTime	  += cg.frametime;
			le->pos.trTime	  += cg.frametime;
			le->angles.trTime += cg.frametime;
		}

		if (cg.time >= le->endTime)
		{
			CG_FreeLocalEntity( le );
			continue;
		}

		switch (le->leType)
		{
			default:
				CG_Error("Bad leType: %i", le->leType);
				break;

			case LE_MARK:
				break;

			case LE_SPRITE_EXPLOSION:
				CG_AddSpriteExplosion(le);
				break;

			case LE_EXPLOSION:
				CG_AddExplosion(le);
				break;

			case LE_FRAGMENT: // gibs and brass
				CG_AddFragment(le);
				break;

			case LE_MOVE_SCALE_FADE: // water bubbles
				CG_AddMoveScaleFade(le);
				break;

			case LE_FADE_RGB: // teleporters, railtrails
				CG_AddFadeRGB(le);
				break;

			case LE_FALL_SCALE_FADE: // gib blood trails
				CG_AddFallScaleFade(le);
				break;

			case LE_SCALE_FADE: // rocket trails
				CG_AddScaleFade(le);
				break;

			case LE_SCOREPLUM:
				CG_AddScorePlum(le);
				break;
		}
	}
}

// model a particle
void CG_AddParticleToScene ( localEntity_t *le )
{
	vec3_t	    head, tail, line, right, forward;
	float	    stretch, tailsplay, tailcol;
	polyVert_t  xal[4];
	int	    backtrail;
	qboolean    noShow = qfalse;

	particle_t  *part  = &p_attr[le->partType];

	if (le->radius == 0)
	{
		return;
	}

	if (part->flags & PART_FLAG_INVISIBLE)
	{
		noShow = qtrue;
	}

	// calculate particle as a blur - used for sparks and water drops
	if (part->trail)
	{
		backtrail = cg.time - le->pos.trTime;

		if (backtrail > 50)
		{
			backtrail = 50;
		}

		// gravity hack
		if((le->pos.trType == TR_GRAVITY2) || (le->pos.trType == TR_FEATHER))
		{
			trGravity = le->gravity;
		}
		trGravity = 1.0f;
		// find particle's head and tail: head is present pos, tail is 500ms behind it
		BG_EvaluateTrajectory( &le->pos, cg.time, head );
		BG_EvaluateTrajectory( &le->pos, cg.time - backtrail, tail );
		//VectorSet( mid, (head[0] + tail[0]) * 0.5, (head[1] + tail[1]) * 0.5, (head[2] + tail[2]) * 0.5 );
		tailsplay = 3.0f;
		tailcol   = 0.0f;
	}
	else
	{
		// particle is a "chunk", not a fast-moving projectile like a spark
		tailsplay = 1.0f;

		// gravity hack
		if((le->pos.trType == TR_GRAVITY2) || (le->pos.trType == TR_FEATHER))
		{
			trGravity = le->gravity;
		}
		BG_EvaluateTrajectory( &le->pos, cg.time, head );
		trGravity = 1.0f;
		VectorMA( head, le->radius, cg.refdef.viewaxis[1], tail );
		//VectorSet( tail, head[0], head[1], (head[2] - le->part.radius) );
		tailcol = le->color[3];
	}

	if (noShow)
	{
		return;
	}

	VectorSubtract( tail, head, forward );
	VectorNormalize( forward,stretch );

	//if (!stretch) return; // don't draw stopped particles

	line[0] = DotProduct( forward, cg.refdef.viewaxis[1] );
	line[1] = DotProduct( forward, cg.refdef.viewaxis[2] );

	VectorScale( cg.refdef.viewaxis[1], line[1], right );
	VectorMA( right, -line[0], cg.refdef.viewaxis[2], right );
	VectorNormalizeNR( right );

	//CrossProduct( forward, right, up );

	VectorMA( head, (le->radius / 2), right, xal[3].xyz );
	xal[0].st[0] = 1.0f;
	xal[0].st[1] = 1.0f;

	VectorMA( head, -(le->radius / 2), right, xal[2].xyz );
	xal[1].st[0] = 1.0f;
	xal[1].st[1] = 0.0f;

	VectorMA( tail, -(le->radius / 2) * tailsplay, right, xal[1].xyz );	// *3 to widen
	xal[2].st[0] = 0.0f;
	xal[2].st[1] = 0.0f;

	VectorMA( tail, (le->radius / 2) * tailsplay, right, xal[0].xyz );
	xal[3].st[0] = 0.0f;
	xal[3].st[1] = 1.0f;

	// apply colours and alpha
	xal[0].modulate[0] =  (unsigned char)(255.0f * le->color[0]);
	xal[0].modulate[1] =  (unsigned char)(255.0f * le->color[1]);
	xal[0].modulate[2] =  (unsigned char)(255.0f * le->color[2]);
	xal[0].modulate[3] =  (unsigned char)(255.0f * tailcol);

	xal[1].modulate[0] =  (unsigned char)(255.0f * le->color[0]);
	xal[1].modulate[1] =  (unsigned char)(255.0f * le->color[1]);
	xal[1].modulate[2] =  (unsigned char)(255.0f * le->color[2]);
	xal[1].modulate[3] =  (unsigned char)(255.0f * tailcol);

	xal[2].modulate[0] =  (unsigned char)(255.0f * le->color[0]);
	xal[2].modulate[1] =  (unsigned char)(255.0f * le->color[1]);
	xal[2].modulate[2] =  (unsigned char)(255.0f * le->color[2]);
	xal[2].modulate[3] =  (unsigned char)(255.0f * le->color[3]);

	xal[3].modulate[0] =  (unsigned char)(255.0f * le->color[0]);
	xal[3].modulate[1] =  (unsigned char)(255.0f * le->color[1]);
	xal[3].modulate[2] =  (unsigned char)(255.0f * le->color[2]);
	xal[3].modulate[3] =  (unsigned char)(255.0f * le->color[3]);

	if (le->refEntity.customShader == 0)
	{
		le->refEntity.customShader = cgs.media.whiteShader ;
	}
	trap_R_AddPolyToScene( le->refEntity.customShader, 4, xal );
}

// model a shard of glass
// this thing needs to be able to rotate
void CG_AddShardToScene ( localEntity_t *le )
{
	polyVert_t  pv[3];
	int	    i;

	// still need to rotate to align with the axis!!

	// translate triangle vertices into position
	VectorAdd ( le->triverts[0], le->refEntity.origin, pv[0].xyz );
	VectorAdd ( le->triverts[1], le->refEntity.origin, pv[1].xyz );
	VectorAdd ( le->triverts[2], le->refEntity.origin, pv[2].xyz );

	// no custom trans or colour in shards
	for (i = 0; i < 3; i++)
	{
		pv[i].modulate[0] = 0xff;
		pv[i].modulate[1] = 0xff;
		pv[i].modulate[2] = 0xff;
		pv[i].modulate[3] = 0xff;
	}

	// set texture coordinates for tri
	pv[0].st[0] = 0.0f;
	pv[0].st[1] = 1.0f;
	pv[1].st[0] = 1.0f;
	pv[1].st[1] = 1.0f;
	pv[2].st[0] = 0.0f;
	pv[2].st[1] = 0.0f;

	//CG_Printf("Drawing shard at <%.2f, %.2f, %.2f>\n", le->refEntity.origin[0], le->refEntity.origin[1], le->refEntity.origin[2]);

	trap_R_AddPolyToScene( cgs.media.whiteShader, 3, pv );
}

/**
 * $(function)
 */
void ApplyRotationToPoint(vec3_t axis[3], vec3_t in)
{
	vec3_t	out;

	out[0] = ((in[0] * axis[0][0]) + (in[1] * axis[1][0]) + (in[2] * axis[2][0]));
	out[1] = ((in[0] * axis[0][1]) + (in[1] * axis[1][1]) + (in[2] * axis[2][1]));
	out[2] = ((in[0] * axis[0][2]) + (in[1] * axis[1][2]) + (in[2] * axis[2][2]));

	in[0]  = out[0];
	in[1]  = out[1];
	in[2]  = out[2];
}
