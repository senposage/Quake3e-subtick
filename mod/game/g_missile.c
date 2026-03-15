// Copyright (C) 1999-2000 Id Software, Inc.
//
#include "g_local.h"

#define MISSILE_PRESTEP_TIME 50

/*
================
G_BounceMissile

================
*/
void G_BounceMissile( gentity_t *ent, trace_t *trace )
{
	vec3_t	   velocity;
	float	   dot;
	int 	   hitTime;
	gentity_t  *train;
	gentity_t  *traceEnt;
	vec3_t	   down = { 0, 0, -1};

	// Get the trace entity
	traceEnt = &g_entities[trace->entityNum];

	train	 = NULL;

	if((train = G_FindClassHash (train, HSH_func_ut_train)) != NULL)
	{
		if(train->s.number == traceEnt->s.number)
		{
			VectorNormalizeNR( down );
			VectorCopy( down, ent->s.pos.trDelta );

			VectorScale( ent->s.pos.trDelta, 0.25f, ent->s.pos.trDelta );

			VectorAdd( ent->r.currentOrigin, trace->plane.normal, ent->r.currentOrigin);
			VectorCopy( ent->r.currentOrigin, ent->s.pos.trBase );
			ent->s.pos.trTime = level.time;

			return;
		}
	}

	// reflect the velocity on the trace plane
	hitTime = level.previousTime + (level.time - level.previousTime) * trace->fraction;
	BG_EvaluateTrajectoryDelta( &ent->s.pos, hitTime, velocity );
	dot = DotProduct( velocity, trace->plane.normal );
	VectorMA( velocity, -2 * dot, trace->plane.normal, ent->s.pos.trDelta );

	if (ent->s.eFlags & EF_BOUNCE_HALF)
	{
		float  scale = 1;

		// Bounce depending on what it hits
		switch (SURFACE_GET_TYPE(trace->surfaceFlags))
		{
			case SURF_TYPE_TIN:
			case SURF_TYPE_ALUMINUM:
			case SURF_TYPE_IRON:
			case SURF_TYPE_TITANIUM:
			case SURF_TYPE_STEEL:
			case SURF_TYPE_BRASS:
			case SURF_TYPE_COPPER:
				scale = 0.4f;
				break;

			default:
				scale = 0.25f;
				break;
		}

		VectorScale( ent->s.pos.trDelta, scale, ent->s.pos.trDelta );

		// check for stop
		if ((trace->plane.normal[2] > 0.2) && (VectorLength( ent->s.pos.trDelta ) < 40))
		{
			G_SetOrigin( ent, trace->endpos );
			return;
		}
	}

	VectorAdd( ent->r.currentOrigin, trace->plane.normal, ent->r.currentOrigin);
	VectorCopy( ent->r.currentOrigin, ent->s.pos.trBase );
	ent->s.pos.trTime = level.time;
}


#ifdef FLASHNADES
/*
================
G_BlindClients
================
*/
static void G_BlindClients( vec3_t origin, gentity_t *inflictor, gentity_t *attacker, float radius )
{
	float		boxradius;
	vec3_t		mins, maxs;
	int 		entityList[MAX_GENTITIES];
	int 		numListedEntities;
	int 		i, e;
	gentity_t	*ent;
	vec3_t		v;
	float		dist, fdot;
	vec3_t		relative, forward;

	if( radius < 1 )
	{
		radius = 1;
	}

	boxradius = 1.41421356 * radius; // radius * sqrt(2) for bounding box enlargement --
	// bounding box was checking against radius / sqrt(2) if collision is along box plane
	for( i = 0 ; i < 3 ; i++ )
	{
		mins[i] = origin[i] - boxradius;
		maxs[i] = origin[i] + boxradius;
	}

	numListedEntities = trap_EntitiesInBox( mins, maxs, entityList, MAX_GENTITIES );

	for( e = 0 ; e < numListedEntities ; e++ )
	{
		ent = &g_entities[entityList[ e ]];

		if( !ent->client )
		{
			continue;
		}

		for ( i = 0 ; i < 3 ; i++ )
		{
			if ( origin[i] < ent->r.absmin[i] )
			{
				v[i] = ent->r.absmin[i] - origin[i];
			}
			else if ( origin[i] > ent->r.absmax[i] )
			{
				v[i] = origin[i] - ent->r.absmax[i];
			}
			else
			{
				v[i] = 0;
			}
		}

		dist = VectorLength( v );
		if ( dist >= radius )
		{
			continue;
		}

		if( CanDamage( ent, origin ) )
		{
			int 		blind, sound;
			float		points;
			vec3_t		viewpoint;
			trace_t 	trace;

			points = 1.0 - dist*dist / (radius*radius);

			blind = ent->client->ps.stats[STAT_BLIND] & 0x0000FFFF;
			sound = (ent->client->ps.stats[STAT_BLIND] & 0xFFFF0000) >> 16;

			// for now, even only the feet are hit, the sound's full volume
			sound += (int)( 10000.f * points );
			if( sound > 10000 )
			{
				sound = 10000;
			}

			// now check if it can hit our eyes (even from behind - we don't care yet)
			VectorCopy(ent->r.currentOrigin, viewpoint);
			viewpoint[2] += ent->client->ps.viewheight;
			trap_Trace(&trace, origin, NULL, NULL, viewpoint, inflictor->s.number, MASK_SOLID);
			if (trace.fraction == 1.0)
			{
				// now also check whether we look in that direction and adjust accordingly
				VectorSubtract(origin, ent->r.currentOrigin, relative);
				VectorNormalizeNR(relative);
				AngleVectorsF (ent->client->ps.viewangles, forward, NULL, NULL);
				VectorNormalizeNR(forward);

				fdot = DotProduct(forward, relative);

				if ( dist*dist < 64.0*64.0 )
				{
					fdot=1.0;
				}

				if(fdot > 0.0f)
				{
					if( utPSIsItemOn ( &ent->client->ps, UT_ITEM_NVG ) )		// nvgs
					{
						blind += (int)( 12000.0f * fdot * points );
						if( blind > 12000 )
						{
							blind = 12000;
						}
					}
					else
					{
						blind += (int)( 7000.f * fdot * points );
						if( blind > 7000 )
						{
							blind = 7000;
						}
					}
				}
			}

			ent->client->ps.stats[STAT_BLIND] = blind | (sound << 16);
		}
	}
}
#endif


/*
================
G_ExplodeMissile

Explode a missile without an impact
================
*/
void G_ExplodeMissile( gentity_t *ent )
{
	vec3_t	dir;
	vec3_t	origin;

	BG_EvaluateTrajectory( &ent->s.pos, level.time, origin );
	SnapVector( origin );
	G_SetOrigin( ent, origin );

	// we don't have a valid direction, so just point straight up
	dir[0]		= dir[1] = 0;
	dir[2]		= 1;

	ent->s.eType	= ET_GENERAL;
	ent->r.svFlags |= SVF_BROADCAST;

	G_AddEvent( ent, EV_MISSILE_MISS, DirToByte( dir ));

	ent->freeAfterEvent = qtrue;

	// Grenade's gone off in the hand. Hence, 'Sploded.
	// (in other words, make sure the player dies and is properly humiliated)
	if (UT_MOD_SPLODED == ent->methodOfDeath)
	{
		G_Damage(ent->parent, NULL, ent->parent, dir, ent->r.currentOrigin, 200, DAMAGE_RADIUS,
			 UT_MOD_SPLODED, HL_UNKNOWN);
		trap_SendServerCommand( ent->parent->s.clientNum, "sploded");
	}
#ifdef FLASHNADES
	if( ent->s.weapon == UT_WP_GRENADE_FLASH )
	{
		//@Barbatos: radius was 1500
		G_BlindClients( ent->r.currentOrigin, ent, ent->parent, 1000 );
	}
#endif
	// splash damage
	if (ent->splashDamage)
	{
		if (G_RadiusDamage(ent->r.currentOrigin, ent->parent, ent->splashDamage, ent->splashRadius,
							 ent, ent->splashMethodOfDeath))
		{
			g_entities[ent->r.ownerNum].client->accuracy_hits++;
		}
	}

	trap_LinkEntity( ent );
}

/*
================
G_MissileImpact
================
*/
void G_MissileImpact( gentity_t *ent, trace_t *trace )
{
	gentity_t  *other;
	qboolean   hitClient = qfalse;
	vec3_t	   forward, dir;

	other = &g_entities[trace->entityNum];


	// check for bounce
	if ( //!other->takedamage &&
		(ent->s.eFlags & (EF_BOUNCE | EF_BOUNCE_HALF)))
	{
		// If this missle hit something breakable then break it and continue going
//		  if ((Q_stricmp ( other->classname, "func_breakable" ) == 0) && (other->breaktype < 5) && !other->bombable)
		if (other->classhash==HSH_func_breakable && (other->breaktype < 5) && !other->bombable)
		{
			if (other->health > 100)
			{
				other->health -= 100;
			}
			else
			{
				G_Damage( other, ent, ent, forward, trace->endpos, 100, 0, ent->methodOfDeath, HL_UNKNOWN);
				return;
			}
		}

		G_BounceMissile( ent, trace );

		// If it hits a client then dramatically reduce its velocity
		if (other->client)
		{
			// Damage client.
			if (ent->s.weapon == UT_WP_HK69)
			{
				VectorNormalize2NR(ent->s.pos.trDelta, dir);

				dir[0] *= -1.0f;
				dir[1] *= -1.0f;
				dir[2] *= -1.0f;

				G_Damage(other, ent->parent ? ent->parent : ent, ent->parent ? ent->parent : ent, dir, trace->endpos, 25, 0, UT_MOD_HK69_HIT, HL_UNKNOWN);
			}

			VectorScale ( ent->s.pos.trDelta, 0.2f, ent->s.pos.trDelta );
		}

		// If this is not a bomb.
		if (ent->s.weapon != UT_WP_BOMB)
		{
			G_AddEvent(ent, EV_GRENADE_BOUNCE, SURFACE_GET_TYPE(trace->surfaceFlags));
		}

		return;
	}

	if (other->takedamage)
	{
		if (other->client && (other->client->invulnerabilityTime > level.time))
		{
			VectorCopy( ent->s.pos.trDelta, forward );
			VectorNormalizeNR( forward );
			ent->target_ent = other;
			return;
		}
	}

	// impact damage
	if (other->takedamage)
	{
		// FIXME: wrong damage direction?
		if (ent->damage)
		{
			vec3_t	velocity;

			if(LogAccuracyHit( other, &g_entities[ent->r.ownerNum] ))
			{
				g_entities[ent->r.ownerNum].client->accuracy_hits++;
				hitClient = qtrue;
			}
			BG_EvaluateTrajectoryDelta( &ent->s.pos, level.time, velocity );

			if (VectorLength( velocity ) == 0)
			{
				velocity[2] = 1;	// stepped on a grenade
			}
			G_Damage (other, ent, &g_entities[ent->r.ownerNum], velocity,
				  ent->s.origin, ent->damage,
				  0, ent->methodOfDeath, HL_UNKNOWN);

		}
	}

	// is it cheaper in bandwidth to just remove this ent and create a new
	// one, rather than changing the missile into the explosion?

	if (other->takedamage && other->client)
	{
		G_AddEvent( ent, EV_MISSILE_HIT, DirToByte( trace->plane.normal ));
		ent->s.otherEntityNum = other->s.number;
	}
	else if(trace->surfaceFlags & SURF_METALSTEPS)
	{
		G_AddEvent( ent, EV_MISSILE_MISS_METAL, DirToByte( trace->plane.normal ));
	}
	else
	{
		G_AddEvent( ent, EV_MISSILE_MISS, DirToByte( trace->plane.normal ));
	}

	ent->freeAfterEvent = qtrue;

	// change over to a normal entity right at the point of impact
	ent->s.eType = ET_GENERAL;

	SnapVectorTowards( trace->endpos, ent->s.pos.trBase );	// save net bandwidth

	G_SetOrigin( ent, trace->endpos );

	// splash damage (doesn't apply to person directly hit)
	if (ent->splashDamage)
	{
		if(G_RadiusDamage( trace->endpos, ent->parent, ent->splashDamage, ent->splashRadius,
				   NULL, ent->splashMethodOfDeath )) // old value : NULL = other
		{
			if(!hitClient)
			{
				g_entities[ent->r.ownerNum].client->accuracy_hits++;
			}
		}
	}

	trap_LinkEntity( ent );
}

/*
================
G_RunMissile
================
*/
void G_RunMissile( gentity_t *ent )
{
	vec3_t	 origin;
	trace_t  tr;
	int 	 passent, contents;

	if (ent->s.weapon == UT_WP_KNIFE)
	{
		// use our own run function for knives
		G_RunKnife( ent );
		return;
	}

	if (ent->s.pos.trType == TR_STATIONARY)
	{
		trap_LinkEntity( ent );
		G_RunThink( ent );
		return;
	}

	BG_EvaluateTrajectory( &ent->s.pos, level.time, origin );

	// if this missile bounced off an invulnerability sphere
	if (ent->target_ent)
	{
		passent = ent->target_ent->s.number;
	}
	else
	{
		// ignore interactions with the missile owner
		passent = ent->r.ownerNum;
	}
	// trace a line from the previous position to the current position
	trap_Trace( &tr, ent->r.currentOrigin, ent->r.mins, ent->r.maxs, origin, passent, ent->clipmask );

	if (tr.startsolid || tr.allsolid)
	{
		// make sure the tr.entityNum is set to the entity we're stuck in
		trap_Trace( &tr, ent->r.currentOrigin, ent->r.mins, ent->r.maxs, ent->r.currentOrigin, passent, ent->clipmask );
		tr.fraction = 0;
	}
	else
	{
		VectorCopy( tr.endpos, ent->r.currentOrigin );
	}

	trap_LinkEntity( ent );

	// check if in water
	contents = trap_PointContents( tr.endpos, ent->s.number );

	if ((contents & CONTENTS_WATER) && (ent->s.pos.trType != TR_WATER))
	{
		VectorCopy ( tr.endpos, ent->s.pos.trBase );
		VectorScale ( ent->s.pos.trDelta, 0.25f, ent->s.pos.trDelta );
		ent->s.pos.trTime  = level.time;
		ent->s.pos.trType  = TR_WATER;
		ent->s.apos.trTime = level.time;
	}

	if (tr.fraction != 1)
	{
		// never explode or bounce on sky
		if (tr.surfaceFlags & SURF_NOIMPACT)
		{
			G_RunThink( ent );
			return;
		}
		G_MissileImpact( ent, &tr );

		if (ent->s.eType != ET_MISSILE)
		{
			return; 	// exploded (or was dropped or embedded in player if knife)
		}
	}

	// check think function after bouncing
	G_RunThink( ent );
}

/*
================
G_RunCorpse
================
*/
// FIXME: this is just a bare bones implementation, so that someone else can come and do some
// "keep me out of walls" stuff or whatever anyone wants to do.
void G_FindSafeAngle(gentity_t *ent)
{
	//Find out if the angle the corpse is going to fall on is going to look wanky..
	//Wanky is defined as, trace a line in the fall direction, if we hit a wall, then it's wanky.
	//start rotating CCW until we find a safe direction
	vec3_t	 origin, down;
	vec3_t	 mins = { -5, -5, -5};
	vec3_t	 maxs = { 5, 5, 5};

	trace_t  tr;
	float	 angle;
	int 	 j;

	for (j = ent->s.apos.trBase[1]; j < ent->s.apos.trBase[1] + 359; j += 5)
	{
//	Com_Printf("hmm: %d\n",j);
		angle	   = DEG2RAD(j);
		VectorCopy(ent->s.pos.trBase, origin);
		origin[0] += (cos(angle) * 60);
		origin[1] += (sin(angle) * 60);

		trap_Trace( &tr, ent->s.pos.trBase, mins, maxs, origin, ent->s.number, ent->clipmask );

		if (tr.startsolid || tr.allsolid)
		{
		}
		else
		{
			if (tr.fraction == 1)
			{
				//	 Com_Printf("Ok - might be a nice angle.\n");

				//Now check to see if its solid ground
				VectorCopy(ent->s.pos.trBase, origin);
				origin[0] += (cos(angle) * 40);
				origin[1] += (sin(angle) * 40);

				VectorCopy(origin, down);
				down[2] -= 30;
				trap_Trace( &tr, origin, mins, maxs, down, ent->s.number, ent->clipmask );

				if ((tr.fraction == 1) && !tr.startsolid && !tr.allsolid)
				{
					//	Com_Printf("no ground\n");
				}
				else
				{
					ent->s.apos.trBase[1] = j;
					return;
				}
			}
			else
			{
				//Com_Printf("HIT\n");
			}
		}
	}
	//Com_Printf("Could not find a nice angle.\n");
}

/**
 * $(function)
 */
void G_RunCorpse( gentity_t *ent )
{
	vec3_t	 origin;
	trace_t  tr;
	vec3_t	 mins, maxs;

	//We want to make sure we don't fall into a wall
	// BUT we want to make sure we don't adjust the angle on the inital frame
	// because that way we get a nice lerp.
	ent->switchType++;

	if (ent->switchType == 1)
	{
		G_FindSafeAngle(ent);
	}

	//Copy the old origin
	VectorCopy(ent->s.pos.trBase, origin);

	//Add the gravity
	ent->s.pos.trDelta[2] -= 4; //800 units per second, 20 updates per second, 16 units per update

	//Attempt to Run the Physics
	VectorAdd(ent->s.pos.trBase, ent->s.pos.trDelta, ent->s.pos.trBase);

// trace a line from the previous position to the current position
	mins[0] = -2;
	mins[1] = -2;
	mins[2] = 1;
	maxs[0] = 2;
	maxs[1] = 2;
	maxs[2] = 2;

	trap_Trace( &tr, ent->s.pos.trBase, ent->r.mins, ent->r.maxs, origin, 0, ent->clipmask );

	if (tr.startsolid || tr.allsolid)
	{
		// make sure the tr.entityNum is set to the entity we're stuck in
		//trap_Trace( &tr, ent->r.currentOrigin, ent->r.mins, ent->r.maxs, ent->r.currentOrigin, 0, ent->clipmask );
		//tr.fraction = 0;

		VectorCopy( origin, ent->s.pos.trBase);

		ent->s.pos.trDelta[0] = 0;
		ent->s.pos.trDelta[1] = 0;
		ent->s.pos.trDelta[2] = 0;
	}

	VectorCopy( ent->s.pos.trBase, ent->r.currentOrigin );
	trap_LinkEntity( ent );

	// check if in water
/*	contents = trap_PointContents( tr.endpos, ent->s.number );
	if ( (contents & CONTENTS_WATER) && (ent->s.pos.trType != TR_WATER) )
   {
		VectorCopy ( tr.endpos, ent->s.pos.trBase );
		VectorScale ( ent->s.pos.trDelta, 0.25f, ent->s.pos.trDelta );
		ent->s.pos.trTime = level.time;
		ent->s.pos.trType = TR_WATER;
		ent->s.apos.trTime = level.time;
	} */
}

///////////////////////////////////////////////////////
//	Alternate "Knife as missile" functions: run
//	Duplicates some existing missile stuff, but performs
//	things better for knives; simplified.
///////////////////////////////////////////////////////
void G_RunKnife ( gentity_t *ent )
{
	vec3_t	   newOrigin;
	trace_t    tr;
	gentity_t  *thrower;
	gentity_t  *other;
	int 	   contents;
	int 	   ignore;

	thrower = &g_entities[ent->r.ownerNum];

	// find new position to move knife to
	BG_EvaluateTrajectory( &ent->s.pos, level.time, newOrigin );

	if (ent->s.time2 >= 0)
	{
		ignore = ent->s.time2;
	}
	else
	{
		ignore = ent->r.ownerNum;
	}

	// see if we hit anything
	trap_Trace( &tr, ent->s.pos.trBase, NULL, NULL, newOrigin, ignore, MASK_SHOT);

	if (tr.startsolid || tr.allsolid)
	{
		// should not ever happen
		// G_Printf("G_RunKnife: knife trapped in solid; removed\n");
		G_FreeEntity( ent );
		return;
	}

	contents = trap_PointContents( newOrigin, ent->s.number );

	// check if in water
	if (contents & CONTENTS_WATER)
	{
		// knife is in water: slow it down then give it water physics!
		if (ent->watertype != CONTENTS_WATER)
		{
			ent->s.pos.trTime = level.time;
			VectorCopy( newOrigin, ent->s.pos.trBase );
			VectorScale( ent->s.pos.trDelta, 0.25f, ent->s.pos.trDelta);
			ent->s.apos.trTime = level.time;
			VectorScale( ent->s.apos.trDelta, 0.25f, ent->s.apos.trDelta);

			ent->s.pos.trType = TR_WATER;
		}
		//else // already in water... decelerate
		//{
		//	ent->s.pos.trTime = level.time;
		//	VectorCopy( newOrigin, ent->s.pos.trBase );
		//	VectorScale( ent->s.pos.trDelta, 0.95f, ent->s.pos.trDelta);
		//	ent->s.apos.trTime = level.time;
		//	VectorScale( ent->s.apos.trDelta, 0.95f, ent->s.apos.trDelta);
		//}
		ent->watertype = CONTENTS_WATER;
	}
	else
	{
		ent->watertype = 0;
	}

	if (tr.fraction == 1.0f)
	{
		// didn't hit anything, so set origin and exit
		VectorCopy( newOrigin, ent->r.currentOrigin );
		VectorCopy( newOrigin, ent->s.pos.trBase);
		// decelerate
		//ent->s.pos.trTime = level.time;
		//VectorScale( ent->s.pos.trDelta, 0.98f, ent->s.pos.trDelta);
		//ent->s.apos.trTime = level.time;
		//VectorScale( ent->s.apos.trDelta, 0.98f, ent->s.apos.trDelta);

		trap_LinkEntity( ent ); // make it visible
		return;
	}

	if (tr.surfaceFlags & SURF_NOIMPACT)
	{
//		G_FreeEntity( ent );
		// hit a no-impact surface: remove
		return;
	}

	other = &g_entities[tr.entityNum];

	if (other->takedamage)
	{
		ariesTraceResult_t result;
		//float		   dist;

		ent->s.time2 = tr.entityNum;

		if (ent->watertype == CONTENTS_WATER)
		{
			trap_LinkEntity( ent );
			return;
		}

		//If we hit a breakable thats not knifeable
//		  if (Q_stricmp ( other->classname, "func_breakable" ) == 0)
		if (other->classhash==HSH_func_breakable)
		{
			if ((other->bombable) || (other->breaktype > 4))
			{
				G_EmbedKnife ( ent, &tr, ent->s.pos.trBase );
				utBulletHit ( thrower, &tr, UT_WP_KNIFE_THROWN, NULL, qfalse, NULL);
				return;
			}
		}
		// Save the entity it hit so we can skip it later
		/*VectorCopy ( ent->s.pos.trDelta, dir );
		VectorNormalizeNR ( dir );
		VectorCopy ( ent->r.currentOrigin, newOrigin );
		VectorMA ( newOrigin, 60, dir, newOrigin );

		// Little hack so it will test angled down into the model
		newOrigin[2] -= 5;*/

		// Figure out where the knife hit on the body
		// Distance increased from 1 - DensitY
		if (!utTrace ( thrower, &tr, ent->s.pos.trBase, newOrigin, 16, &result, qfalse ))
		{
			// hit the hitbox, but missed an aries location
			VectorCopy( newOrigin, ent->r.currentOrigin );
			trap_LinkEntity( ent );
			return;
		}

		// Do the damage
		utBulletHit ( thrower, &tr, UT_WP_KNIFE_THROWN, &result, qtrue, NULL );

		// TODO: FIX FIX FIX - use triangle returned from aries result to keep knife embedded
		G_FreeEntity( ent );	// remove knife or it will continue to hurt
	}
	else
	{
		G_EmbedKnife ( ent, &tr, ent->s.pos.trBase );

		// something that doesn't take damage: knife gets stuck in it
		// use ent->r.currentOrigin so we know where knife was last level.time
		// for calculation of incidence angle
		utBulletHit ( thrower, &tr, UT_WP_KNIFE_THROWN, NULL, qtrue, NULL );
	}
}

//
// G_EmbedKnife
// Knife has hit a surface NOT a player
//
gentity_t *G_EmbedKnife ( gentity_t *knife, trace_t *trace, vec3_t from )
{
	vec3_t	   origin, incidence;
	gentity_t  *dropped;
	gitem_t    *item;

	// spawn a new item entity
	item			   = BG_FindItem( "Knife" );

	dropped 		   = G_Spawn();

	dropped->s.eType	   = ET_ITEM;
	dropped->s.modelindex  = item - bg_itemlist;	// store item number in modelindex
	dropped->s.modelindex2 = 1; 		// This is non-zero if it's a dropped item
	dropped->s.weapon	   = UT_WP_KNIFE;

	dropped->classname	   = item->classname;
	HASHSTR(item->classname,dropped->classhash);
	dropped->item		   = item;
	VectorSet (dropped->r.mins, -ITEM_RADIUS, -ITEM_RADIUS, -ITEM_RADIUS);
	VectorSet (dropped->r.maxs, ITEM_RADIUS, ITEM_RADIUS, ITEM_RADIUS);
	dropped->r.contents = CONTENTS_TRIGGER;

	dropped->touch		= Touch_Item;

	//Make sure that q3 knows to clear them if the round restarts
	dropped->flags |= FL_KILLNEXTROUND;

	// Make sure they go away after a while
	dropped->think	   = G_FreeEntity;
	dropped->nextthink = level.time + 15 * 1000;
	// Unlink after being picked up
	dropped->wait	   = -1;

	// okay, now we need to work out the various angles and stuff
	// so we can embed the knife in the correct way

	if (knife->watertype == CONTENTS_WATER)
	{
		// lie knife on ground
		VectorCopy ( trace->plane.normal, from );
		PerpendicularVector( incidence, from );
	}
	else
	{
		// stick knife in at incidence angle
		VectorSubtract( from, trace->endpos, incidence );
	}

	vectoangles( incidence, dropped->s.angles );
	dropped->s.angles[2]  = 0;

	dropped->s.angles[0] -= (90.0f + 45);	// rotate to make front first

	// set origin of dropped knife to the trace endpos, shifted a few
	// units in the direction of the incidence angle so knife is in wall
	// VectorMA( trace->endpos, 2, trace->plane.normal, origin );
	VectorCopy ( trace->endpos, origin ); // let cgame move the knife to proper pos in wall
	G_SetOrigin( dropped, origin );

	dropped->s.pos.trType = 0;
	dropped->s.pos.trTime = 0;
	VectorClear( dropped->s.pos.trDelta );

	//dropped->flags = FL_DROPPED_ITEM;
	dropped->count = 1; // picking up knife adds one extra ammo

	trap_LinkEntity (dropped);
	G_FreeEntity( knife );

	return dropped;
}

/**
 * $(function)
 */
void G_GrenadeShutdown( gentity_t *ent )
{
	ent->freeAfterEvent = qtrue;

//	G_AddEvent( ent, EV_MISSILE_MISS, 0);
//	trap_LinkEntity( ent );
}

/**
 * $(function)
 */
void G_MrSentryThink( gentity_t *ent)
{
	int 		   i;
	gentity_t	   *pent;
	vec3_t		   viewvec;
	vec3_t		   aimvec;
	vec3_t		   aimangle;
	vec3_t		   right;
	vec3_t		   up;

	vec3_t		   muzzle;
	float		   dot;
	vec3_t		   end;
	ariesTraceResult_t result;
	trace_t 	   tr;
	qboolean	   redflag;
	qboolean	   blueflag;

	ent->nextthink	   = level.time + 50;
	ent->s.apos.trType = TR_INTERPOLATE;

	//ent->s.frame	   The mode of the sentry gun  0 searching, 1 killing
	//ent->angle	   The mapper specified target angle
	//ent->s.powerups  Team

	//Idle, aquire new targets
	if (ent->s.frame == 0)
	{
		// Aim Towards the target
		{
			//Aimvec is where we want to be
			aimangle[0] = sin((float)level.time / 3000) * 5 ;
			aimangle[1] = ent->angle + (sin((float)level.time / 1000) * 45);
			aimangle[2] = 0;

			//So, lerp 10% of it
			ent->s.apos.trBase[0] = LerpAngle(aimangle[0], ent->s.apos.trBase[0], 0.9f);
			ent->s.apos.trBase[1] = LerpAngle(aimangle[1], ent->s.apos.trBase[1], 0.9f);
			ent->s.apos.trBase[2] = 0;
		}

		pent = &g_entities[0];

		for(i = 0; i < MAX_CLIENTS; i++, pent++)
		{
			//Find a suitable victim
			if(pent->client && pent->inuse && (pent->client->sess.sessionTeam < TEAM_SPECTATOR) && !(pent->s.eFlags & EF_DEAD))
			{
				redflag  = (pent->client ? utPSHasItem(&pent->client->ps, UT_ITEM_REDFLAG) : qfalse);
				blueflag = (pent->client ? utPSHasItem(&pent->client->ps, UT_ITEM_BLUEFLAG) : qfalse);

				//We want the enemy only, or some shmuck with a flag
				if (!redflag && !blueflag && (pent->client->sess.sessionTeam == ent->s.powerups))
				{
					continue;
				}

				aimvec[0] = pent->s.pos.trBase[0] - ent->s.pos.trBase[0];
				aimvec[1] = pent->s.pos.trBase[1] - ent->s.pos.trBase[1];
				aimvec[2] = pent->s.pos.trBase[2] - ent->s.pos.trBase[2];

				VectorNormalizeNR(aimvec);
				//Loop through the clients and find any naughty ones.
				AngleVectors(ent->s.apos.trBase, viewvec, right, up);

				dot = DotProduct(aimvec, viewvec);
				//Com_Printf("%f\n",dot);
				//Aquired target
				aimvec[2] = 0;

				if ((dot > 0.9) || (VectorLength(aimvec) < 64))
				{
					trap_Trace(&tr, ent->s.pos.trBase, NULL, NULL, pent->s.pos.trBase, ent->s.number, CONTENTS_SOLID);

					if (tr.fraction == 1)
					{
						ent->s.frame	= 1; //Sounds the spinning noise
						ent->s.time2++; 	 //For sounding the alarm
						ent->target_ent = pent;
					}
				}
			}
		}
	}
	else
	if (ent->s.frame == 1)
	{
		pent = ent->target_ent;

		//Turn the sentry off if our target goes away
		if (!pent || !pent->inuse || (pent->s.eFlags & EF_DEAD))
		{
			ent->s.frame = 0;
			ent->s.torsoAnim++; //for sounding the spin down
			return;
		}

		//Line of Sight
		trap_Trace(&tr, ent->s.pos.trBase, NULL, NULL, pent->s.pos.trBase, ent->s.number, CONTENTS_SOLID);

		if (tr.fraction != 1)
		{
			ent->s.frame = 0;
			ent->s.torsoAnim++; //for sounding the spin down
			return;
		}

		// Aim Towards the target
		{
			//Aimvec is where we want to be
			aimvec[0] = pent->s.pos.trBase[0] - ent->s.pos.trBase[0];
			aimvec[1] = pent->s.pos.trBase[1] - ent->s.pos.trBase[1];
			aimvec[2] = (pent->s.pos.trBase[2] + 10) - ent->s.pos.trBase[2];

			VectorNormalizeNR(aimvec);
			vectoangles(aimvec, aimangle); //face the shot direction

			//So, lerp 30% of it
			ent->s.apos.trBase[0] = LerpAngle(aimangle[0], ent->s.apos.trBase[0], 0.6f);
			ent->s.apos.trBase[1] = LerpAngle(aimangle[1], ent->s.apos.trBase[1], 0.6f);
			ent->s.apos.trBase[2] = 0;
		}

		//Fire the Sentry Gun
		if (level.time > ent->pain_bleed_time)
		{
			muzzle[0] = ent->s.pos.trBase[0];
			muzzle[1] = ent->s.pos.trBase[1];
			muzzle[2] = ent->s.pos.trBase[2];

			VectorCopy(ent->s.apos.trBase, aimangle);

			aimangle[0] += ((float)((rand() % 100) - 50) / 30.0f);
			aimangle[1] += ((float)((rand() % 100) - 50) / 30.0f);

			AngleVectors(aimangle, aimvec, right, up);

			// Traces a bullet's path
			end[0] = muzzle[0] + (aimvec[0] * 30000);
			end[1] = muzzle[1] + (aimvec[1] * 30000);
			end[2] = muzzle[2] + (aimvec[2] * 30000);

			utTrace ( ent, &tr, muzzle, end, 1, &result, qfalse );

			// Make sure something was hit
			if (!(tr.surfaceFlags & SURF_NOIMPACT))
			{
				// Perform the hit
				utBulletHit ( ent, &tr, UT_WP_BERETTA, &result, qtrue, NULL);
			}
			ent->pain_bleed_time = level.time + 50;
			ent->s.time 	 = level.time;
			ent->s.generic1++;
		}
	}
}
