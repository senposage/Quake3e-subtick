// Copyright (C) 1999-2000 Id Software, Inc.
//

#include "g_local.h"
#include "../common/c_bmalloc.h"

/*
===============================================================================

PUSHMOVE

===============================================================================
*/

void MatchTeam( gentity_t *teamLeader, int moverState, int time );
int  CalcRDoorDirection 	 ( gentity_t *ent, gentity_t *activator);

void Use_BinaryMoverSpecial( gentity_t *ent, gentity_t *other, gentity_t *activator );

typedef struct
{
	gentity_t  *ent;
	vec3_t	   origin;
	vec3_t	   angles;
	float	   deltayaw;
} pushed_t;
pushed_t  pushed[MAX_GENTITIES], *pushed_p;

/**
 * $(function)
 */
void SP_InitMover ( gentity_t *self )
{
	if (self->mover)
	{
		return;
	}

	// Allocate the mover object
	self->mover = bmalloc ( sizeof(struct gmover_s));
}

/*
============
G_TestEntityPosition

============
*/
gentity_t *G_TestEntityPosition( gentity_t *ent )
{
	trace_t  tr;
	int  mask;

	if (ent->clipmask)
	{
		mask = ent->clipmask;
	}
	else
	{
		mask = MASK_SOLID;
	}

	if (ent->client)
	{
		trap_Trace( &tr, ent->client->ps.origin, ent->r.mins, ent->r.maxs, ent->client->ps.origin, ent->s.number, mask );
		/*
		if (tr.allsolid)
		{
			G_Damage( ent, NULL, NULL, NULL, NULL, 99999, 0, MOD_CRUSH, HL_UNKNOWN );
		}
		*/
	}
	else
	{
		trap_Trace( &tr, ent->s.pos.trBase, ent->r.mins, ent->r.maxs, ent->s.pos.trBase, ent->s.number, mask );
	}

	if (tr.startsolid)
	{
		return &g_entities[tr.entityNum];
	}

	return NULL;
}

/*
================
G_CreateRotationMatrix
================
*/
void G_CreateRotationMatrix(vec3_t angles, vec3_t matrix[3])
{
	AngleVectors(angles, matrix[0], matrix[1], matrix[2]);
	VectorInverse(matrix[1]);
}

/*
================
G_TransposeMatrix
================
*/
void G_TransposeMatrix(vec3_t matrix[3], vec3_t transpose[3])
{
	int  i, j;

	for (i = 0; i < 3; i++)
	{
		for (j = 0; j < 3; j++)
		{
			transpose[i][j] = matrix[j][i];
		}
	}
}

/*
================
G_RotatePoint
================
*/
void G_RotatePoint(vec3_t point, vec3_t matrix[3])
{
	vec3_t	tvec;

	VectorCopy(point, tvec);
	point[0] = DotProduct(matrix[0], tvec);
	point[1] = DotProduct(matrix[1], tvec);
	point[2] = DotProduct(matrix[2], tvec);
}

/*
==================
G_TryPushingEntity

Returns qfalse if the move is blocked
==================
*/
qboolean G_TryPushingEntity( gentity_t *check, gentity_t *pusher, vec3_t move, vec3_t amove )
{
	vec3_t	   matrix[3], transpose[3];
	vec3_t	   org, org2, move2, entpos;
	gentity_t  *block;

	// EF_MOVER_STOP will just stop when contacting another entity
	// instead of pushing it, but entities can still ride on top of it
	if ((pusher->s.eFlags & EF_MOVER_STOP) &&
		(check->s.groundEntityNum != pusher->s.number))
	{
		return qfalse;
	}

	if(pusher->classname)
	{
//		  if(!(Q_stricmp(pusher->classname, "func_ut_train")))
		if(pusher->classhash==HSH_func_ut_train)
		{
			if ((move[0] == 0) && (move[1] == 0) && (move[2] < 0))
			{
				if((check->s.groundEntityNum != pusher->s.number) && (check->s.groundEntityNum != ENTITYNUM_NONE))
				{
					G_Damage( check, pusher, pusher, NULL, NULL, 99999, 0, MOD_CRUSH, HL_UNKNOWN );
				}
			}
		}
	}

	// save off the old position
	if (pushed_p > &pushed[MAX_GENTITIES])
	{
		G_Error( "pushed_p > &pushed[MAX_GENTITIES]" );
	}
	pushed_p->ent = check;
	VectorCopy (check->s.pos.trBase, pushed_p->origin);
	VectorCopy (check->s.apos.trBase, pushed_p->angles);

	if (check->client)
	{
		pushed_p->deltayaw = check->client->ps.delta_angles[YAW];
		VectorCopy (check->client->ps.origin, pushed_p->origin);
	}
	pushed_p++;

	// try moving the contacted entity
	// figure movement due to the pusher's amove
	G_CreateRotationMatrix( amove, transpose );
	G_TransposeMatrix( transpose, matrix );

	if (check->client)
	{
		VectorAdd (check->client->ps.origin, move, check->client->ps.origin);
		VectorCopy (check->client->ps.origin, entpos);
	}
	else
	{
		VectorCopy (check->s.pos.trBase, entpos);
	}

	VectorSubtract(entpos, pusher->r.currentOrigin, org);

//	org[0]*=1.15f;
//	org[1]*=1.15f;
//	org[2]*=1.15f;

	VectorCopy( org, org2 );
	G_RotatePoint( org2, matrix );
	VectorSubtract (org2, org, move2);	// move2 is linear movement caused by rotation

	// add movement
	//VectorAdd (check->s.pos.trBase, move, check->s.pos.trBase);
	//VectorAdd (check->s.pos.trBase, move2, check->s.pos.trBase);
	if (check->client)
	{
		VectorAdd (check->client->ps.origin, move2, check->client->ps.origin);	// this is not correcting origin correctly, causing sliding on chopper (jitter)
		//	make sure the client's view rotates when on a rotating mover

		//27 send this info to the client and let them do the yaw - this is actually okay
		check->client->ps.delta_angles[YAW] += ANGLE2SHORT(amove[YAW]);
	}

	// may have pushed them off an edge
	if (check->s.groundEntityNum != pusher->s.number)
	{
		check->s.groundEntityNum = -1;
	}

	block = G_TestEntityPosition( check );

	// temp hack to stop ut_trains from being blocked by players
	// side effect: jerky player movement and possibly getting stuck (but not badly)
	/*
//	  if (check->client && !Q_stricmp( pusher->classname, "func_ut_train" ))
	if (check->client && pusher->classhash==HSH_func_ut_train)
	{
		//	trap_LinkEntity (check);
		return qtrue;
	}
	*/

	if (!block)
	{
		// pushed ok
		if (check->client)
		{
			VectorCopy( check->client->ps.origin, check->r.currentOrigin );
		}
		else
		{
			VectorCopy( check->s.pos.trBase, check->r.currentOrigin );
		}
		trap_LinkEntity (check);
		return qtrue;
	}
	else
	{
		if(pusher->classname)
		{
//			  if(!(Q_stricmp(pusher->classname, "func_ut_train")))
			if(pusher->classhash==HSH_func_ut_train)
				G_Damage( check, pusher, pusher, NULL, NULL, 99999, 0, MOD_CRUSH, HL_UNKNOWN );
		}
	}

	// if it is ok to leave in the old position, do it
	// this is only relevent for riding entities, not pushed
	// Sliding trapdoors can cause this.
	VectorCopy((pushed_p - 1)->origin, check->s.pos.trBase);

	if (check->client)
	{
		VectorCopy((pushed_p - 1)->origin, check->client->ps.origin);
	}
	VectorCopy((pushed_p - 1)->angles, check->s.apos.trBase );
	block = G_TestEntityPosition (check);

	if (!block)
	{
		check->s.groundEntityNum = -1;
		pushed_p--;
		return qtrue;
	}

	// blocked
	return qtrue;
//	return qfalse;	//27's hack - gotta test it.
}

void G_ExplodeMissile( gentity_t *ent );

/*
============
G_MoverPush

Objects need to be moved back on a failed push,
otherwise riders would continue to slide.
If qfalse is returned, *obstacle will be the blocking entity
============
*/
qboolean G_MoverPush( gentity_t *pusher, vec3_t move, vec3_t amove, gentity_t * *obstacle )
{
	int    i, e;
	gentity_t  *check;
	vec3_t	   mins, maxs;
	pushed_t   *p;
	int    entityList[MAX_GENTITIES];
	int    listedEntities;
	vec3_t	   totalMins, totalMaxs;

	*obstacle = NULL;

	// mins/maxs are the bounds at the destination
	// totalMins / totalMaxs are the bounds for the entire move
	if (pusher->r.currentAngles[0] || pusher->r.currentAngles[1] || pusher->r.currentAngles[2]
		|| amove[0] || amove[1] || amove[2])
	{
		float  radius;

		radius = RadiusFromBounds( pusher->r.mins, pusher->r.maxs );

		for (i = 0 ; i < 3 ; i++)
		{
			mins[i] 	 = pusher->r.currentOrigin[i] + move[i] - radius;
			maxs[i] 	 = pusher->r.currentOrigin[i] + move[i] + radius;
			totalMins[i] = mins[i] - move[i];
			totalMaxs[i] = maxs[i] - move[i];
		}
	}
	else
	{
		for (i = 0 ; i < 3 ; i++)
		{
			mins[i] = pusher->r.absmin[i] + move[i];
			maxs[i] = pusher->r.absmax[i] + move[i];
		}

		VectorCopy( pusher->r.absmin, totalMins );
		VectorCopy( pusher->r.absmax, totalMaxs );

		for (i = 0 ; i < 3 ; i++)
		{
			if (move[i] > 0)
			{
				totalMaxs[i] += move[i];
			}
			else
			{
				totalMins[i] += move[i];
			}
		}
	}

	// unlink the pusher so we don't get it in the entityList
	trap_UnlinkEntity( pusher );

	listedEntities = trap_EntitiesInBox( totalMins, totalMaxs, entityList, MAX_GENTITIES );

	// move the pusher to its final position
	VectorAdd( pusher->r.currentOrigin, move, pusher->r.currentOrigin );
	VectorAdd( pusher->r.currentAngles, amove, pusher->r.currentAngles );
	trap_LinkEntity( pusher );

	// see if any solid entities are inside the final position
	for (e = 0 ; e < listedEntities ; e++)
	{
		check = &g_entities[entityList[e]];

		// only push items and players
		if ((check->s.eType != ET_ITEM) && (check->s.eType != ET_PLAYER) && (check->s.eType != ET_MISSILE) && !check->physicsObject)
		{
			continue;
		}

		// if the entity is standing on the pusher, it will definitely be moved
		if (check->s.groundEntityNum != pusher->s.number)
		{
			// see if the ent needs to be tested
			if ((check->r.absmin[0] >= maxs[0])
				|| (check->r.absmin[1] >= maxs[1])
				|| (check->r.absmin[2] >= maxs[2])
				|| (check->r.absmax[0] <= mins[0])
				|| (check->r.absmax[1] <= mins[1])
				|| (check->r.absmax[2] <= mins[2]))
			{
				continue;
			}

			// see if the ent's bbox is inside the pusher's final position
			// this does allow a fast moving object to pass through a thin entity...
			if (!G_TestEntityPosition (check))
			{
				continue;
			}
		}

		// the entity needs to be pushed
		if (G_TryPushingEntity( check, pusher, move, amove ))
		{
			continue;
		}

		// the move was blocked an entity

		// bobbing entities are instant-kill and never get blocked
		if ((pusher->s.pos.trType == TR_SINE) || (pusher->s.apos.trType == TR_SINE))
		{
			G_Damage( check, pusher, pusher, NULL, NULL, 99999, 0, MOD_CRUSH, HL_UNKNOWN );
			continue;
		}

		// save off the obstacle so we can call the block function (crush, etc)
		*obstacle = check;

		// move back any entities we already moved
		// go backwards, so if the same entity was pushed
		// twice, it goes back to the original position
		for (p = pushed_p - 1 ; p >= pushed ; p--)
		{
			VectorCopy (p->origin, p->ent->s.pos.trBase);
			VectorCopy (p->angles, p->ent->s.apos.trBase);

			if (p->ent->client)
			{
				p->ent->client->ps.delta_angles[YAW] = p->deltayaw;
				VectorCopy (p->origin, p->ent->client->ps.origin);
			}
			trap_LinkEntity (p->ent);
		}
		return qfalse;
	}

	return qtrue;
}

/*
=================
G_MoverTeam
=================
*/
void G_MoverTeam( gentity_t *ent )
{
	vec3_t	   move, amove;
	gentity_t  *part, *obstacle;
	vec3_t	   origin, angles;

	obstacle = NULL;

	// make sure all team slaves can move before commiting
	// any moves or calling any think functions
	// if the move is blocked, all moved objects will be backed out
	pushed_p = pushed;

	for (part = ent ; part ; part = part->teamchain)
	{
		// get current position
		BG_EvaluateTrajectory( &part->s.pos, level.time, origin );
		BG_EvaluateTrajectory( &part->s.apos, level.time, angles );
		VectorSubtract( origin, part->r.currentOrigin, move );
		VectorSubtract( angles, part->r.currentAngles, amove );

		if (!G_MoverPush( part, move, amove, &obstacle ))
		{
			break;	// move was blocked
		}
	}

	if (part)	// move was blocked
	{	// go back to the previous position
		for (part = ent ; part ; part = part->teamchain)
		{
			part->s.pos.trTime	+= level.time - level.previousTime;
			part->s.apos.trTime += level.time - level.previousTime;
			BG_EvaluateTrajectory( &part->s.pos, level.time, part->r.currentOrigin );
			BG_EvaluateTrajectory( &part->s.apos, level.time, part->r.currentAngles );
			trap_LinkEntity( part );
		}

		// if the pusher has a "blocked" function, call it
		if (ent->mover->blocked)
		{
			ent->mover->blocked( ent, obstacle );
		}
		return;
	}

	// the move succeeded
	for (part = ent ; part ; part = part->teamchain)
	{
		// call the reached function if time is at or past end point
//		  if ((Q_stricmp (ent->classname, "func_rotating_door") == 0))
		if (ent->classhash==HSH_func_rotating_door)
		{
			if (part->s.apos.trType == TR_LINEAR_STOP)
			{
				if (level.time >= part->s.apos.trTime + part->s.apos.trDuration)
				{
					if (part->mover->reached)
					{
						part->mover->reached( part, ent );
					}
				}
			}
		}
		else
		{
			if (part->s.pos.trType == TR_LINEAR_STOP)
			{
				if (level.time >= part->s.pos.trTime + part->s.pos.trDuration)
				{
					if (part->mover->reached)
					{
						part->mover->reached( part, ent );
					}
				}
			}
		}
	}
}

/*
================
G_RunMover

================
*/
void G_RunMover( gentity_t *ent )
{
	// if not a team captain, don't do anything, because
	// the captain will handle everything
	if (ent->flags & FL_TEAMSLAVE)
	{
		return;
	}

	// if stationary at one of the positions, don't move anything
	if ((ent->s.pos.trType != TR_STATIONARY) || (ent->s.apos.trType != TR_STATIONARY))
	{
		G_MoverTeam( ent );
	}
	// check think function
	G_RunThink( ent );

	//Check to see if its one of those special doors that needs autoclosing after a while
	//27work
	if (ent->s.time2 > 0)
	{
		//If we have not been triggered in
		if (level.time > ent->count)
		{
			if (ent->mover->moverState == MOVER_POS2)
			{
				ent->nextthink = level.time;

				G_RunThink(ent);
			}
		}
	}
}

/*
============================================================================

GENERAL MOVERS

Doors, plats, and buttons are all binary (two position) movers
Pos1 is "at rest", pos2 is "activated"
============================================================================
*/

/*
===============
SetMoverState
===============
*/
void SetMoverState( gentity_t *ent, moverState_t moverState, int time )
{
	vec3_t	delta;
	float	f;

	ent->mover->moverState = moverState;

	ent->s.pos.trTime	   = time;
	ent->s.apos.trTime	   = time;

	switch(moverState)
	{
		case MOVER_POS1:

//			  if (Q_stricmp (ent->classname, "func_rotating_door") == 0)
			if (ent->classhash==HSH_func_rotating_door)
			{
				VectorCopy( ent->mover->apos1, ent->s.apos.trBase );
				ent->s.apos.trType = TR_STATIONARY;
			}
			else
			{
				VectorCopy( ent->mover->pos1, ent->s.pos.trBase );
				ent->s.pos.trType = TR_STATIONARY;
			}
			break;

		case MOVER_POS2:

//			  if (Q_stricmp (ent->classname, "func_rotating_door") == 0)
			if (ent->classhash==HSH_func_rotating_door)
			{
				VectorCopy( ent->mover->apos2, ent->s.apos.trBase );
				ent->s.apos.trType = TR_STATIONARY;
			}
			else
			{
				VectorCopy( ent->mover->pos2, ent->s.pos.trBase );
				ent->s.pos.trType = TR_STATIONARY;
			}
			break;

		case MOVER_1TO2:

//			  if (Q_stricmp (ent->classname, "func_rotating_door") == 0)
			if (ent->classhash==HSH_func_rotating_door)
			{
				VectorCopy( ent->mover->apos1, ent->s.apos.trBase );			// set trBase to apos1 (start angle)
				VectorSubtract ( ent->mover->apos2, ent->mover->apos1, delta ); 	// get difference between start and end angle vectors
				VectorCopy( delta, ent->s.apos.trDelta );				// set trDelta
				ent->s.apos.trType = TR_LINEAR_STOP;
			}
			else
			{
				VectorCopy( ent->mover->pos1, ent->s.pos.trBase );
				VectorSubtract( ent->mover->pos2, ent->mover->pos1, delta );
				f		  = 1000.0 / ent->s.pos.trDuration;
				VectorScale( delta, f, ent->s.pos.trDelta );
				ent->s.pos.trType = TR_LINEAR_STOP;
			}
			break;

		case MOVER_2TO1:

//			  if (Q_stricmp (ent->classname, "func_rotating_door") == 0)
			if (ent->classhash==HSH_func_rotating_door)
			{
				VectorCopy( ent->mover->apos2, ent->s.apos.trBase );
				VectorSubtract ( ent->mover->apos1, ent->mover->apos2, delta );
				VectorCopy( delta, ent->s.apos.trDelta );
				ent->s.apos.trType = TR_LINEAR_STOP;
			}
			else
			{
				VectorCopy( ent->mover->pos2, ent->s.pos.trBase );
				VectorSubtract( ent->mover->pos1, ent->mover->pos2, delta );
				f		  = 1000.0 / ent->s.pos.trDuration;
				VectorScale( delta, f, ent->s.pos.trDelta );
				ent->s.pos.trType = TR_LINEAR_STOP;
			}
			break;

		default:
			return;
	}

//	  if (Q_stricmp (ent->classname, "func_rotating_door") == 0)
	if (ent->classhash==HSH_func_rotating_door)
	{
		BG_EvaluateTrajectory( &ent->s.apos, level.time, ent->r.currentAngles );
	}
	else
	{
		BG_EvaluateTrajectory( &ent->s.pos, level.time, ent->r.currentOrigin );
	}
	trap_LinkEntity( ent );
}

/*
================
MatchTeam

All entities in a mover team will move from pos1 to pos2
in the same amount of time
================
*/
void MatchTeam( gentity_t *teamLeader, int moverState, int time )
{
	gentity_t  *slave;

	for (slave = teamLeader ; slave ; slave = slave->teamchain)
	{
		SetMoverState( slave, moverState, time );
	}
}

/*
================
ReturnToPos1
================
*/
void ReturnToPos1( gentity_t *ent )
{
	MatchTeam( ent, MOVER_2TO1, level.time );

	// looping sound
	ent->s.loopSound = ent->mover->soundLoop;

	// starting sound
	if (ent->mover->sound2to1)
	{
		G_Sound ( ent, 0, ent->mover->sound2to1 );
	}
}

/*
================
Reached_BinaryMover
================
*/
void Reached_BinaryMover( gentity_t *ent, gentity_t *other )
{
	// stop the looping sound
	ent->s.loopSound = 0; // ent->mover->soundLoop;

	if (ent->mover->moverState == MOVER_1TO2)
	{
		// reached pos2
		SetMoverState( ent, MOVER_POS2, level.time );

		// play sound
		if (ent->mover->soundPos2)
		{
			G_Sound ( ent, 0, ent->mover->soundPos2 );
		}

		ent->think = ReturnToPos1;

//		  if ((Q_stricmp (ent->classname, "func_rotating_door") == 0) || (Q_stricmp (ent->classname, "func_door") == 0))
		if (ent->classhash==HSH_func_rotating_door || ent->classhash==HSH_func_door)
		{
			// don't ever return until binary mover asks
			ent->nextthink = INT_MAX;
		}
		else
		{
			ent->nextthink = level.time + ent->wait;
		}

		// fire targets
		if (!ent->activator)
		{
			ent->activator = ent;
		}

		if(!(ent->isNPC && !(ent->activator->isNPC)))
		{
			G_UseTargets( ent, ent->activator );
		}
	}
	else if (ent->mover->moverState == MOVER_2TO1)
	{
		// reached pos1
		SetMoverState( ent, MOVER_POS1, level.time );

		// play sound
		if (ent->mover->soundPos1)
		{
			G_Sound ( ent, 0, ent->mover->soundPos1 );
		}

		// close areaportals
		if ((ent->teammaster == ent) || !ent->teammaster)
		{
			trap_AdjustAreaPortalState( ent, qfalse );
		}
	}
	else
	{
		G_Error( "Reached_BinaryMover: bad moverState" );
	}
}

/*
================
Use_BinaryMover
================
*/
void Use_BinaryMover( gentity_t *ent, gentity_t *other, gentity_t *activator )
{
	int  total;
	int  partial;
	int  dir;		// for rotating door direction

	if (ent->s.time2 > 0)
	{
		Use_BinaryMoverSpecial(ent, other, activator);
		return;
	}

	// only the master should be used
	if (ent->flags & FL_TEAMSLAVE)
	{
		Use_BinaryMover( ent->teammaster, other, activator );
		return;
	}

//	 G_Printf("USING!\n");
	ent->activator = activator;

	if (ent->mover->moverState == MOVER_POS1)
	{
		// GottaBeKD: return if button is close only
//		  if (!Q_stricmp (ent->classname, "func_door") && (other->switchType == 2))
		if (ent->classhash==HSH_func_door && (other->switchType == 2))
		{
			return;
		}

//		  if(!Q_stricmp( ent->classname, "func_door" ) &&
		if(ent->classhash==HSH_func_door &&
		   (ent->mover->team != TEAM_SPECTATOR) &&
		   (ent->mover->team != activator->client->sess.sessionTeam))
		{
			return;
		}

		// Dokta8: work out apos2 so door opens away from player
//		  if (Q_stricmp (ent->classname, "func_rotating_door") == 0)
		if (ent->classhash==HSH_func_rotating_door)
		{
			if((ent->mover->team != TEAM_SPECTATOR) &&
			   (ent->mover->team != activator->client->sess.sessionTeam))
			{
				return;
			}

			dir = CalcRDoorDirection(ent, activator);

			if (ent->count != 0)
			{
				dir = ent->mover->direction;
			}						//Door only opens one way

			if (dir != 0)
			{
				ent->mover->apos2[0] = (ent->mover->apos1[0] + (dir * 90)) * ent->movedir[0];
				ent->mover->apos2[1] = (ent->mover->apos1[1] + (dir * 90)) * ent->movedir[1];
				ent->mover->apos2[2] = (ent->mover->apos1[2] + (dir * 90)) * ent->movedir[2];
			}
			else
			{
				// only happens if door is a trapdoor
				// make 'em always open anti-clockwise
				ent->mover->apos2[0] = (ent->mover->apos1[0] + (90)) * ent->movedir[0];
				ent->mover->apos2[1] = (ent->mover->apos1[1] + (90)) * ent->movedir[1];
				ent->mover->apos2[2] = (ent->mover->apos1[2] + (90)) * ent->movedir[2];
			}
		}

		// start moving 50 msec later, becase if this was player
		// triggered, level.time hasn't been advanced yet
		MatchTeam( ent, MOVER_1TO2, level.time	);

		// starting sound
		if (ent->mover->sound1to2)
		{
			G_Sound ( ent, 0, ent->mover->sound1to2 );
		}

		// looping sound
		ent->s.loopSound = ent->mover->soundLoop;

		// open areaportal
		if ((ent->teammaster == ent) || !ent->teammaster)
		{
			trap_AdjustAreaPortalState( ent, qtrue );
		}
		return;
	}

	// if all the way up, just delay before coming down
	if (ent->mover->moverState == MOVER_POS2)
	{
		//G_Printf("MoverPos2\n");
//		  if (!Q_stricmp (ent->classname, "func_door") && (other->switchType == 1))
		if (ent->classhash==HSH_func_door && (other->switchType == 1))
		{
			return;
		}

//		  if(!Q_stricmp( ent->classname, "func_door" ) &&
		if(ent->classhash==HSH_func_door &&
		   (ent->mover->team != TEAM_SPECTATOR) &&
		   (ent->mover->team != activator->client->sess.sessionTeam))
		{
			return;
		}

		// looping sound
		ent->s.loopSound = ent->mover->soundLoop;

//		  if (Q_stricmp (ent->classname, "func_rotating_door") == 0)
		if (ent->classhash==HSH_func_rotating_door)
		{
			if((ent->mover->team != TEAM_SPECTATOR) &&
			   (ent->mover->team != activator->client->sess.sessionTeam))
			{
				return;
			}
			// return rdoor immediately if player asks
			ent->nextthink = level.time + 50;
		}
		else
		{
			ent->nextthink = level.time + ent->wait;
		}
		return;
	}

	// only partway down before reversing
	if (ent->mover->moverState == MOVER_2TO1)
	{
		// G_Printf("MoverPos2to1\n");
//		  if(!Q_stricmp( ent->classname, "func_door" ) &&
		if(ent->classhash==HSH_func_door &&
		   (ent->mover->team != TEAM_SPECTATOR) &&
		   (ent->mover->team != activator->client->sess.sessionTeam))
		{
			return;
		}

//		  if (Q_stricmp (ent->classname, "func_rotating_door") == 0)
		if (ent->classhash==HSH_func_rotating_door)
		{
			if((ent->mover->team != TEAM_SPECTATOR) &&
			   (ent->mover->team != activator->client->sess.sessionTeam))
			{
				return;
			}

			total	= ent->s.apos.trDuration;

			partial = level.time - ent->s.apos.trTime;

			if (partial > total)
			{
				partial = total;
			}

			MatchTeam( ent, MOVER_1TO2, level.time - (total - partial));
		}
		else
		{
			total	= ent->s.pos.trDuration;

			partial = level.time - ent->s.pos.trTime;

			if (partial > total)
			{
				partial = total;
			}

			MatchTeam( ent, MOVER_1TO2, level.time - (total - partial));
		}

		// looping sound
		ent->s.loopSound = ent->mover->soundLoop;

		if (ent->mover->sound1to2)
		{
			G_Sound ( ent, 0, ent->mover->sound1to2 );
		}

		return;
	}

	// only partway up before reversing
	if (ent->mover->moverState == MOVER_1TO2)
	{
//		  if(!Q_stricmp( ent->classname, "func_door" ) &&
		if(ent->classhash==HSH_func_door &&
		   (ent->mover->team != TEAM_SPECTATOR) &&
		   (ent->mover->team != activator->client->sess.sessionTeam))
		{
			return;
		}

//		  if (Q_stricmp (ent->classname, "func_rotating_door") == 0)
		if (ent->classhash==HSH_func_rotating_door)
		{
			if((ent->mover->team != TEAM_SPECTATOR) &&
			   (ent->mover->team != activator->client->sess.sessionTeam))
			{
				return;
			}

			total	= ent->s.apos.trDuration;

			partial = level.time - ent->s.apos.trTime;

			if (partial > total)
			{
				partial = total;
			}

			MatchTeam( ent, MOVER_2TO1, level.time - (total - partial));
		}
		else
		{
			total	= ent->s.pos.trDuration;

			partial = level.time - ent->s.pos.trTime;

			if (partial > total)
			{
				partial = total;
			}

			MatchTeam( ent, MOVER_2TO1, level.time - (total - partial));
		}

		if (ent->mover->sound2to1)
		{
			G_Sound ( ent, 0, ent->mover->sound2to1 );
		}
		return;
	}
}

/**
 * $(function)
 */
void Use_BinaryMoverSpecial( gentity_t *ent, gentity_t *other, gentity_t *activator )
{
	int  dir;		// for rotating door direction

	// only the master should be used
	if (ent->flags & FL_TEAMSLAVE)
	{
		Use_BinaryMover( ent->teammaster, other, activator );
		return;
	}

//	 G_Printf("USING!\n");
	ent->activator = activator;
	ent->count	   = level.time + 1000;

	//If the door was FULLY CLOSED
	if (ent->mover->moverState == MOVER_POS1)
	{
		if (ent->splashDamage == 0)
		{
//	   G_Printf("Mover_POS1\n");
			// GottaBeKD: return if button is close only
//			  if (!Q_stricmp (ent->classname, "func_door") && (other->switchType == 2))
			if (ent->classhash == HSH_func_door && (other->switchType == 2))
			{
				return;
			}

//			  if(!Q_stricmp( ent->classname, "func_door" ) &&
			if(ent->classhash==HSH_func_door &&
			   (ent->mover->team != TEAM_SPECTATOR) &&
			   (ent->mover->team != activator->client->sess.sessionTeam))
			{
				return;
			}

			// Dokta8: work out apos2 so door opens away from player
//			  if (Q_stricmp (ent->classname, "func_rotating_door") == 0)
			if (ent->classhash==HSH_func_rotating_door)
			{
				if((ent->mover->team != TEAM_SPECTATOR) &&
				   (ent->mover->team != activator->client->sess.sessionTeam))
				{
					return;
				}

				dir = CalcRDoorDirection(ent, activator);

				if (ent->count != 0)
				{
					dir = ent->mover->direction;
				}					//Door only opens one way

				if (dir != 0)
				{
					ent->mover->apos2[0] = (ent->mover->apos1[0] + (dir * 90)) * ent->movedir[0];
					ent->mover->apos2[1] = (ent->mover->apos1[1] + (dir * 90)) * ent->movedir[1];
					ent->mover->apos2[2] = (ent->mover->apos1[2] + (dir * 90)) * ent->movedir[2];
				}
				else
				{
					// only happens if door is a trapdoor
					// make 'em always open anti-clockwise
					ent->mover->apos2[0] = (ent->mover->apos1[0] + (90)) * ent->movedir[0];
					ent->mover->apos2[1] = (ent->mover->apos1[1] + (90)) * ent->movedir[1];
					ent->mover->apos2[2] = (ent->mover->apos1[2] + (90)) * ent->movedir[2];
				}
			}

			// start moving 50 msec later, becase if this was player
			// triggered, level.time hasn't been advanced yet
			MatchTeam( ent, MOVER_1TO2, level.time	);

			// starting sound
			if (ent->mover->sound1to2)
			{
				G_Sound ( ent, 0, ent->mover->sound1to2 );
			}

			// looping sound
			ent->s.loopSound = ent->mover->soundLoop;

			// open areaportal
			if ((ent->teammaster == ent) || !ent->teammaster)
			{
				trap_AdjustAreaPortalState( ent, qtrue );
			}
			return;
		}
		ent->splashDamage = 0;
	}

	//DOOR WAS OPENING/OPENED, so
	//If we weren't triggered recently then close it.

	// if all the way up, just delay before coming down
	if (ent->mover->moverState == MOVER_POS2)
	{
		if (ent->splashDamage == 1)
		{
			ent->splashDamage = 0;
			G_Printf("MoverPos2\n");

//			  if (!Q_stricmp (ent->classname, "func_door") && (other->switchType == 1))
			if (ent->classhash==HSH_func_door && (other->switchType == 1))
			{
				return;
			}

//			  if(!Q_stricmp( ent->classname, "func_door" ) &&
			if(ent->classhash==HSH_func_door &&
			   (ent->mover->team != TEAM_SPECTATOR) &&
			   (ent->mover->team != activator->client->sess.sessionTeam))
			{
				return;
			}

			// looping sound
			ent->s.loopSound = ent->mover->soundLoop;

//			  if (Q_stricmp (ent->classname, "func_rotating_door") == 0)
			if (ent->classhash==HSH_func_rotating_door)
			{
				if((ent->mover->team != TEAM_SPECTATOR) &&
				   (ent->mover->team != activator->client->sess.sessionTeam))
				{
					return;
				}
				// return rdoor immediately if player asks
				ent->nextthink = level.time + 50;
			}
			else
			{
				ent->nextthink = level.time + ent->wait;
			}
			return;
		}
	}

	// only partway down before reversing
/*	if ( ent->mover->moverState == MOVER_2TO1 )
   {

	  G_Printf("MoverPos2to1\n");
//		  if( !Q_stricmp( ent->classname, "func_door" ) &&
		if( ent->classhash==HSH_func_door &&
			ent->mover->team != TEAM_SPECTATOR &&
			ent->mover->team != activator->client->sess.sessionTeam )
			return;

//		  if (Q_stricmp (ent->classname, "func_rotating_door")==0) {
		if (ent->classhash==HSH_func_rotating_door) {
			if( ent->mover->team != TEAM_SPECTATOR &&
				ent->mover->team != activator->client->sess.sessionTeam )
				return;

			total = ent->s.apos.trDuration;

			partial = level.time - ent->s.apos.trTime;

			if ( partial > total ) {
				partial = total;
			}

			MatchTeam( ent, MOVER_1TO2, level.time - ( total - partial ) );

		} else {
			total = ent->s.pos.trDuration;

			partial = level.time - ent->s.pos.trTime;

			if ( partial > total ) {
				partial = total;
			}

			MatchTeam( ent, MOVER_1TO2, level.time - ( total - partial ) );

		}

		// looping sound
		ent->s.loopSound = ent->mover->soundLoop;

		if ( ent->mover->sound1to2 ) {
			G_Sound ( ent, 0, ent->mover->sound1to2 );
		}

		return;
	}

	// only partway up before reversing
	if ( ent->mover->moverState == MOVER_1TO2 )
   {

	  G_Printf("MoverPos1to2\n");
//		 if( !Q_stricmp( ent->classname, "func_door" ) &&
		if( ent->classhash==HSH_func_door &&
			ent->mover->team != TEAM_SPECTATOR &&
			ent->mover->team != activator->client->sess.sessionTeam )
			return;

//		  if (Q_stricmp (ent->classname, "func_rotating_door")==0) {
		if (ent->classhash==HSH_func_rotating_door) {

			if( ent->mover->team != TEAM_SPECTATOR &&
				ent->mover->team != activator->client->sess.sessionTeam )
				return;

			total = ent->s.apos.trDuration;

			partial = level.time - ent->s.apos.trTime;

			if ( partial > total ) {
				partial = total;
			}


			MatchTeam( ent, MOVER_2TO1, level.time - ( total - partial ) );

		} else {
			total = ent->s.pos.trDuration;

			partial = level.time - ent->s.pos.trTime;

			if ( partial > total ) {
				partial = total;
			}

			MatchTeam( ent, MOVER_2TO1, level.time - ( total - partial ) );
		}

		if ( ent->mover->sound2to1 ) {
			G_Sound ( ent, 0, ent->mover->sound2to1 );
		}
		return;
	}
   */
}

/*
================
InitMover

"pos1", "pos2", and "speed" should be set before calling,
so the movement delta can be calculated
================
*/
void InitMover( gentity_t *ent )
{
	vec3_t	  move;
	float	  distance;
	float	  light;
	vec3_t	  color;
	qboolean  lightSet, colorSet;
	char	  *sound;
	int 	  trigonly = 0;

	G_SpawnInt("trigger_only", "0", &trigonly);

	if (trigonly)
	{
		ent->mover->trigger_only = qtrue;
	}
	else
	{
		ent->mover->trigger_only = qfalse;
	}

	// if the "model2" key is set, use a seperate model
	// for drawing, but clip against the brushes
	if (ent->model2)
	{
		ent->s.modelindex2 = G_ModelIndex( ent->model2 );
	}

	// if the "loopsound" key is set, use a constant looping sound when moving
	if (G_SpawnString( "noise", "100", &sound ))
	{
//		ent->s.loopSound = G_SoundIndex( sound );
		ent->mover->soundLoop = G_SoundIndex( sound );
//		G_AddEvent( ent, EV_GLOBAL_SOUND, ent->mover->soundLoop );
	}

	// if the "color" or "light" keys are set, setup constantLight
	lightSet = G_SpawnFloat( "light", "100", &light );
	colorSet = G_SpawnVector( "color", "1 1 1", color );

	if (lightSet || colorSet)
	{
		int  r, g, b, i;

		r = color[0] * 255;

		if (r > 255)
		{
			r = 255;
		}
		g = color[1] * 255;

		if (g > 255)
		{
			g = 255;
		}
		b = color[2] * 255;

		if (b > 255)
		{
			b = 255;
		}
		i = light / 4;

		if (i > 255)
		{
			i = 255;
		}
		ent->s.constantLight = r | (g << 8) | (b << 16) | (i << 24);
	}

	ent->use		   = Use_BinaryMover;
	ent->mover->reached    = Reached_BinaryMover;

	ent->mover->moverState = MOVER_POS1;
	ent->r.svFlags		   = SVF_USE_CURRENT_ORIGIN;
	ent->s.eType		   = ET_MOVER;
	VectorCopy (ent->mover->pos1, ent->r.currentOrigin);

	// Begin Rdoor modification
//	  if (Q_stricmp (ent->classname, "func_rotating_door") == 0)	   //	  dokta8 - setup currentAngles and trBase
	if (ent->classhash==HSH_func_rotating_door)   //	 dokta8 - setup currentAngles and trBase
	{
		VectorCopy ( ent->mover->apos1, ent->r.currentAngles ); 	//	  dokta8 - if entity is a rotating door
		ent->s.apos.trType = TR_STATIONARY;
		VectorCopy ( ent->mover->apos1, ent->s.apos.trBase );
	}
	// End Rdoor

	ent->s.pos.trType = TR_STATIONARY;
	VectorCopy( ent->mover->pos1, ent->s.pos.trBase );

	// calculate time to reach second position from speed
	VectorSubtract( ent->mover->pos2, ent->mover->pos1, move );
	distance = VectorLength( move );

	if (!ent->speed)
	{
		ent->speed = 100;
	}
	VectorScale( move, ent->speed, ent->s.pos.trDelta );
	ent->s.pos.trDuration = distance * 1000 / ent->speed;

	if (ent->s.pos.trDuration <= 0)
	{
		ent->s.pos.trDuration = 1;
	}

	// Begin RDoor Modificiation
	// calculate s.apos.trDuration as time to reach
	// destination, based on speed
//	  if (Q_stricmp (ent->classname, "func_rotating_door") == 0)
	if (ent->classhash==HSH_func_rotating_door)
	{
		distance = 90;	// doors rotate thru 90 degrees

		if (ent->speed == 0)
		{
			ent->speed = 100;
		}
		ent->s.apos.trDuration = (distance * 1000 / ent->speed) + 100; //27 fixed the snappy door problem

		if (ent->s.apos.trDuration < 0)
		{
			ent->s.apos.trDuration *= -1;
		}

		if (ent->s.apos.trDuration <= 0)
		{
			ent->s.apos.trDuration = 1;
		}
	}
	// End RDoor Mod
	trap_LinkEntity (ent);
}

/*
===============================================================================

DOOR

A use can be triggered either by a touch function, by being shot, or by being
targeted by another entity.

===============================================================================
*/

/*
================
Blocked_Door
================
*/
void Blocked_Door( gentity_t *ent, gentity_t *other )
{
	// remove anything other than a client
	if (!other->client)
	{
//		// except CTF flags and grenaded!!!!
//		if( other->s.eType == ET_ITEM && other->item->giType == IT_TEAM ) {
//			Team_DroppedFlagThink( other );
//			return;
//		}
//		G_FreeEntity( other );
		return;
	}

	if (ent->damage)
	{
		G_Damage( other, ent, ent, NULL, NULL, ent->damage, 0, MOD_CRUSH, HL_UNKNOWN );
	}

	if (ent->spawnflags & 4)
	{
		return; 	// crushers don't reverse
	}

//	  if (Q_stricmp (ent->classname, "func_rotating_door") == 0)
	if (ent->classhash==HSH_func_rotating_door)
	{
		return;
	}

	// reverse direction
	Use_BinaryMover( ent, ent, other );
}

/*
================
Touch_DoorTrigger
================
*/
void Touch_DoorTrigger( gentity_t *ent, gentity_t *other, trace_t *trace )
{
	return;

	/*if ( other->client && (other->client->sess.sessionTeam == TEAM_SPECTATOR || other->client->ghost )) {
		// if the door is not open and not opening
		if ( ent->parent->moverState != MOVER_1TO2 &&
			ent->parent->moverState != MOVER_POS2) {
			Touch_DoorTriggerSpectator( ent, other, trace );
		}
	}
	else if ( ent->parent->moverState != MOVER_1TO2 ) {
		Use_BinaryMover( ent->parent, ent, other );
	}*/
}

/*
======================
Think_SpawnNewDoorTrigger

All of the parts of a door have been spawned, so create
a trigger that encloses all of them
======================
*/
void Think_SpawnNewDoorTrigger( gentity_t *ent )
{
	gentity_t  *other;
	vec3_t	   mins, maxs;
	int    i, best;

/* URBAN TERROR - Doors arent shootable
	// set all of the slaves as shootable
	for ( other = ent ; other ; other = other->teamchain ) {
		other->takedamage = qtrue;
	}
*/

	// find the bounds of everything on the team
	VectorCopy (ent->r.absmin, mins);
	VectorCopy (ent->r.absmax, maxs);

	for (other = ent->teamchain ; other ; other = other->teamchain)
	{
		AddPointToBounds (other->r.absmin, mins, maxs);
		AddPointToBounds (other->r.absmax, mins, maxs);
	}

	// find the thinnest axis, which will be the one we expand
	best = 0;

	for (i = 1 ; i < 3 ; i++)
	{
		if (maxs[i] - mins[i] < maxs[best] - mins[best])
		{
			best = i;
		}
	}

/* URBAN TERROR - expand them all
	maxs[best] += 120;
	mins[best] -= 120;
*/

//	  if (Q_stricmp (ent->classname, "func_door") == 0)
	if (ent->classhash==HSH_func_door)
	{
		//	dokta8 -  increase the bounding box by 120 units in all dimensions
		//	dokta8 -  so doors can be opened if players aren't standing directly in
		//	dokta8 -  front of them. (should work to open doors from beneath)
		maxs[0] += 120;
		mins[0] -= 120;
		maxs[1] += 120;
		mins[1] -= 120;
		maxs[2] += 120;
		mins[2] -= 120;
	}

/* URBAN TERROR - Dont need a door trigger, players must trigger it
	// create a trigger with this size
	other = G_Spawn ();
	other->classname = "door_trigger";
	other->classhash = HSH_door_trigger;
	VectorCopy (mins, other->r.mins);
	VectorCopy (maxs, other->r.maxs);
	other->parent = ent;
	other->r.contents = CONTENTS_TRIGGER;
	other->touch = Touch_DoorTrigger;
	// remember the thinnest axis
	other->count = best;
	trap_LinkEntity (other);
*/

	MatchTeam( ent, ent->mover->moverState, level.time );
}

/**
 * $(function)
 */
void Think_MatchTeam( gentity_t *ent )
{
	MatchTeam( ent, ent->mover->moverState, level.time );
}

/*QUAKED func_door (0 .5 .8) ? START_OPEN x CRUSHER
TOGGLE		wait in both the start and end states for a trigger event.
START_OPEN	the door to moves to its destination when spawned, and operate in reverse.	It is used to temporarily or permanently close off an area when triggered (not useful for touch or takedamage doors).
NOMONSTER	monsters will not trigger this door

"model2"	.md3 model to also draw
"angle" 	determines the opening direction
"targetname" if set, no touch field will be spawned and a remote button or trigger field activates the door.
"speed" 	movement speed (100 default)
"wait"		wait before returning (3 default, -1 = never return)
"lip"		lip remaining at end of move (8 default)
"dmg"		damage to inflict when blocked (2 default)
"color" 	constantLight color
"light" 	constantLight radius
"health"	if set, the door must be shot open
*/
void SP_func_door (gentity_t *ent)
{
	vec3_t	abs_movedir;
	float	distance;
	vec3_t	size;
	float	lip;

	char	*sound;
	char	*team;

	SP_InitMover ( ent );

	ent->mover->sound1to2 = ent->mover->sound2to1 = G_SoundIndex("sound/movers/doors/dr1_strt.wav");
	ent->mover->soundPos1 = ent->mover->soundPos2 = G_SoundIndex("sound/movers/doors/dr1_end.wav");

	// "pos1" is start opening sound (play when reached POS1 or POS2)
	// "pos2" is stop opening sound (play when leaving POS1 or POS2)
	// "noise" is looping sound to play while moving

	G_SpawnInt( "closewhenidle", "0", &ent->s.time2);

	if (G_SpawnString( "pos1", "", &sound ))
	{
		if (Q_stricmp( sound, "none" ) == 0)
		{
			ent->mover->sound1to2 = ent->mover->sound2to1 = 0;
		}
		else
		{
			ent->mover->sound1to2 = ent->mover->sound2to1 = G_SoundIndex( sound );
		}
	}

	if (G_SpawnString( "pos2", "", &sound ))
	{
		if (Q_stricmp( sound, "none" ) == 0)
		{
			ent->mover->soundPos1 = ent->mover->soundPos2 = 0;
		}
		else
		{
			ent->mover->soundPos1 = ent->mover->soundPos2 = G_SoundIndex( sound );
		}
	}

	// Team based opening - DensitY
	if(G_SpawnString( "only", "", &team ) && (g_gametype.integer >= GT_TEAM) && (g_gametype.integer != GT_JUMP))
	{
		if (ent->mover)
		{
			if(!Q_stricmp( team, "red" ))
			{
				ent->mover->team = TEAM_RED;
			}
			else if(!Q_stricmp( team, "blue" ))
			{
				ent->mover->team = TEAM_BLUE;
			}
			else
			{
				ent->mover->team = TEAM_SPECTATOR;
			}
		}
	}
	else
	{
		if (ent->mover)
		{
			ent->mover->team = TEAM_SPECTATOR;
		}
	}

	ent->mover->blocked = Blocked_Door;

	// default speed of 400
	if (!ent->speed)
	{
		ent->speed = 400;
	}

	// default wait of 2 seconds
	if (!ent->wait)
	{
		ent->wait = 2;
	}

	ent->wait *= 1000;

	// default lip of 8 units
	G_SpawnFloat( "lip", "8", &lip );

	// default damage of 2 points
	G_SpawnInt( "dmg", "2", &ent->damage );

	// default damage of 2 points
	G_SpawnInt( "dmg", "2", &ent->damage );

	// first position at start
	VectorCopy( ent->s.origin, ent->mover->pos1 );

	// calculate second position
	trap_SetBrushModel( ent, ent->model );
	G_SetMovedir (ent->s.angles, ent->movedir);
	abs_movedir[0] = fabs(ent->movedir[0]);
	abs_movedir[1] = fabs(ent->movedir[1]);
	abs_movedir[2] = fabs(ent->movedir[2]);
	VectorSubtract( ent->r.maxs, ent->r.mins, size );
	distance	   = DotProduct( abs_movedir, size ) - lip;
	VectorMA( ent->mover->pos1, distance, ent->movedir, ent->mover->pos2 );

	// if "start_open", reverse position 1 and 2
	if (ent->spawnflags & 1)
	{
		vec3_t	temp;

		VectorCopy( ent->mover->pos2, temp );
		VectorCopy( ent->s.origin, ent->mover->pos2 );
		VectorCopy( temp, ent->mover->pos1 );
	}

	InitMover( ent );

	ent->nextthink = level.time + FRAMETIME;

	if (!(ent->flags & FL_TEAMSLAVE))
	{
		int  health;

		G_SpawnInt( "health", "0", &health );

		if (health)
		{
			ent->takedamage = qtrue;
		}

		if (ent->targetname || health)
		{
			// non touch/shoot doors
			ent->think = Think_MatchTeam;
		}
		else
		{
			ent->think = Think_SpawnNewDoorTrigger;
		}
	}
}

/*
===============================================================================

PLAT

===============================================================================
*/

/*
==============
Touch_Plat

Don't allow decent if a living player is on it
===============
*/
void Touch_Plat( gentity_t *ent, gentity_t *other, trace_t *trace )
{
	if (!other->client || (other->client->ps.stats[STAT_HEALTH] <= 0))
	{
		return;
	}

	// delay return-to-pos1 by one second
	if (ent->mover->moverState == MOVER_POS2)
	{
		ent->nextthink = level.time + 1000;
	}
}

/*
==============
Touch_PlatCenterTrigger

If the plat is at the bottom position, start it going up
===============
*/
void Touch_PlatCenterTrigger(gentity_t *ent, gentity_t *other, trace_t *trace )
{
	if (!other->client)
	{
		return;
	}

	if (ent->parent->mover->moverState == MOVER_POS1)
	{
		Use_BinaryMover( ent->parent, ent, other );
	}
}

/*
================
SpawnPlatTrigger

Spawn a trigger in the middle of the plat's low position
Elevator cars require that the trigger extend through the entire low position,
not just sit on top of it.
================
*/
void SpawnPlatTrigger( gentity_t *ent )
{
	gentity_t  *trigger;
	vec3_t	   tmin, tmax;

	// the middle trigger will be a thin trigger just
	// above the starting position
	trigger 		= G_Spawn();
	trigger->classname	= "plat_trigger";
	trigger->classhash	= HSH_plat_trigger;
	trigger->touch		= Touch_PlatCenterTrigger;
	trigger->r.contents = CONTENTS_TRIGGER;
	trigger->parent 	= ent;

	tmin[0] 		= ent->mover->pos1[0] + ent->r.mins[0] + 33;
	tmin[1] 		= ent->mover->pos1[1] + ent->r.mins[1] + 33;
	tmin[2] 		= ent->mover->pos1[2] + ent->r.mins[2];

	tmax[0] 		= ent->mover->pos1[0] + ent->r.maxs[0] - 33;
	tmax[1] 		= ent->mover->pos1[1] + ent->r.maxs[1] - 33;
	tmax[2] 		= ent->mover->pos1[2] + ent->r.maxs[2] + 8;

	if (tmax[0] <= tmin[0])
	{
		tmin[0] = ent->mover->pos1[0] + (ent->r.mins[0] + ent->r.maxs[0]) * 0.5;
		tmax[0] = tmin[0] + 1;
	}

	if (tmax[1] <= tmin[1])
	{
		tmin[1] = ent->mover->pos1[1] + (ent->r.mins[1] + ent->r.maxs[1]) * 0.5;
		tmax[1] = tmin[1] + 1;
	}

	VectorCopy (tmin, trigger->r.mins);
	VectorCopy (tmax, trigger->r.maxs);

	trap_LinkEntity (trigger);
}

/*QUAKED func_plat (0 .5 .8) ?
Plats are always drawn in the extended position so they will light correctly.

"lip"		default 8, protrusion above rest position
"height"	total height of movement, defaults to model height
"speed" 	overrides default 200.
"dmg"		overrides default 2
"model2"	.md3 model to also draw
"color" 	constantLight color
"light" 	constantLight radius
*/
void SP_func_plat (gentity_t *ent)
{
	float  lip, height;

	SP_InitMover ( ent );

	ent->mover->sound1to2 = ent->mover->sound2to1 = G_SoundIndex("sound/movers/plats/pt1_strt.wav");
	ent->mover->soundPos1 = ent->mover->soundPos2 = G_SoundIndex("sound/movers/plats/pt1_end.wav");

	VectorClear (ent->s.angles);

	G_SpawnFloat( "speed", "200", &ent->speed );
	G_SpawnInt( "dmg", "2", &ent->damage );
	G_SpawnFloat( "wait", "1", &ent->wait );
	G_SpawnFloat( "lip", "8", &lip );

	ent->wait = 1000;

	// create second position
	trap_SetBrushModel( ent, ent->model );

	if (!G_SpawnFloat( "height", "0", &height ))
	{
		height = (ent->r.maxs[2] - ent->r.mins[2]) - lip;
	}

	// pos1 is the rest (bottom) position, pos2 is the top
	VectorCopy( ent->s.origin, ent->mover->pos2 );
	VectorCopy( ent->mover->pos2, ent->mover->pos1 );
	ent->mover->pos1[2] -= height;

	InitMover( ent );

	// touch function keeps the plat from returning while
	// a live player is standing on it
	ent->touch		= Touch_Plat;

	ent->mover->blocked = Blocked_Door;

	ent->parent 	= ent; // so it can be treated as a door

	// spawn the trigger if one hasn't been custom made
	if (!ent->targetname)
	{
		SpawnPlatTrigger(ent);
	}
}

/*
===============================================================================

BUTTON

===============================================================================
*/

/*
==============
Touch_Button

===============
*/
void Touch_Button(gentity_t *ent, gentity_t *other, trace_t *trace )
{
	return;

	// not in UT
	/*if ( !other->client ) {
		return;
	}

	if ( ent->mover->moverState == MOVER_POS1 ) {
		Use_BinaryMover( ent, other, other );
	}*/
}

/*QUAKED func_button (0 .5 .8) ?
When a button is touched, it moves some distance in the direction of it's angle, triggers all of it's targets, waits some time, then returns to it's original position where it can be triggered again.

"model2"	.md3 model to also draw
"angle" 	determines the opening direction
"target"	all entities with a matching targetname will be used
"speed" 	override the default 40 speed
"wait"		override the default 1 second wait (-1 = never return)
"lip"		override the default 4 pixel lip remaining at end of move
"health"	if set, the button must be killed instead of touched
"color" 	constantLight color
"light" 	constantLight radius
*/
void SP_func_button( gentity_t *ent )
{
	vec3_t	abs_movedir;
	float	distance;
	vec3_t	size;
	float	lip;
	int npcOnly;
	char	*sound;

	SP_InitMover ( ent );

	ent->mover->sound1to2 = G_SoundIndex("sound/movers/switches/butn2.wav");

	if (G_SpawnString( "pushsound", "", &sound ))
	{
		if (Q_stricmp( sound, "none" ) == 0)
		{
			ent->mover->sound1to2 = 0;
		}
		else
		{
			ent->mover->sound1to2 = G_SoundIndex( sound );
		}
	}

	if (!ent->speed)
	{
		ent->speed = 40;
	}

	if (!ent->wait)
	{
		ent->wait = 1;
	}
	ent->wait *= 1000;

	G_SpawnInt( "npcOnly", "0", &npcOnly );

	if(npcOnly == 1)
	{
		ent->isNPC = qtrue;
	}

	G_SpawnInt( "value", "0", &ent->switchType );

	// first position
	VectorCopy( ent->s.origin, ent->mover->pos1 );

	// calculate second position
	trap_SetBrushModel( ent, ent->model );

	G_SpawnFloat( "lip", "4", &lip );

	G_SetMovedir( ent->s.angles, ent->movedir );
	abs_movedir[0] = fabs(ent->movedir[0]);
	abs_movedir[1] = fabs(ent->movedir[1]);
	abs_movedir[2] = fabs(ent->movedir[2]);
	VectorSubtract( ent->r.maxs, ent->r.mins, size );
	distance	   = abs_movedir[0] * size[0] + abs_movedir[1] * size[1] + abs_movedir[2] * size[2] - lip;
	VectorMA (ent->mover->pos1, distance, ent->movedir, ent->mover->pos2);

	if (ent->health)
	{
		// shootable button
		ent->takedamage = qtrue;
	} /*else { NOT IN UT
		// touchable button
		ent->touch = Touch_Button;
	}*/

	InitMover( ent );
}

/*
===============================================================================

TRAIN

===============================================================================
*/

#define TRAIN_START_ON	  1
#define TRAIN_TOGGLE	  2
#define TRAIN_BLOCK_STOPS 4

/*
===============
Think_BeginMoving

The wait time at a corner has completed, so start moving again
===============
*/
void Think_BeginMoving( gentity_t *ent )
{
	ent->s.pos.trTime = level.time;
	ent->s.pos.trType = TR_LINEAR_STOP;
}

/*
===============
Reached_Train
===============
*/
void Reached_Train( gentity_t *ent, gentity_t *other )
{
	gentity_t  *next;
	float	   speed;
	vec3_t	   move;
	float	   length;

	// copy the apropriate values
	next = ent->mover->nextTrain;

	if (!next || !next->mover->nextTrain)
	{
		return; 	// just stop
	}

	// fire all other targets
	G_UseTargets( next, NULL );

	// set the new trajectory
	ent->mover->nextTrain = next->mover->nextTrain;
	VectorCopy( next->s.origin, ent->mover->pos1 );
	VectorCopy( next->mover->nextTrain->s.origin, ent->mover->pos2 );

	// if the path_corner has a speed, use that
	if (next->speed)
	{
		speed = next->speed;
	}
	else
	{
		// otherwise use the train's speed
		speed = ent->speed;
	}

	if (speed < 1)
	{
		speed = 1;
	}

	// calculate duration
	VectorSubtract( ent->mover->pos2, ent->mover->pos1, move );
	length			  = VectorLength( move );

	ent->s.pos.trDuration = length * 1000 / speed;

	// looping sound
	ent->s.loopSound = next->mover->soundLoop;

	// start it goingn
	SetMoverState( ent, MOVER_1TO2, level.time );

	// if there is a "wait" value on the target, don't start moving yet
	if (next->wait)
	{
		ent->nextthink	  = level.time + next->wait * 1000;
		ent->think	  = Think_BeginMoving;
		ent->s.pos.trType = TR_STATIONARY;
	}
}

/*
===============
Think_SetupTrainTargets

Link all the corners together
===============
*/
void Think_SetupTrainTargets( gentity_t *ent )
{
	gentity_t  *path, *next = NULL, *start;

	ent->mover->nextTrain = G_Find( NULL, FOFS(targetname), ent->target );

	if (!ent->mover->nextTrain)
	{
		vtos(ent->r.absmin) ;
		return;
	}

	start = NULL;

	for (path = ent->mover->nextTrain ; path != start ; path = next)
	{
		if (!start)
		{
			start = path;
		}

		if (!path->target)
		{
			vtos(path->s.origin) ;
			return;
		}

		// find a path_corner among the targets
		// there may also be other targets that get fired when the corner
		// is reached
		next = NULL;

		do
		{
			next = G_Find( next, FOFS(targetname), path->target );

			if (!next)
			{
				vtos(path->s.origin) ;
				return;
			}
		}
//		  while (strcmp( next->classname, "path_corner" ));
		while (next->classhash!=HSH_path_corner);

		path->mover->nextTrain = next;
	}

	// start the train moving from the first corner
	Reached_Train( ent, next );
}

/*QUAKED path_corner (.5 .3 0) (-8 -8 -8) (8 8 8)
Train path corners.
Target: next path corner and other targets to fire
"speed" speed to move to the next corner
"wait" seconds to wait before behining move to next corner
*/
void SP_path_corner( gentity_t *self )
{
	if (!self->targetname)
	{
		G_FreeEntity( self );
		return;
	}
	// path corners don't need to be linked in
}

/*QUAKED func_train (0 .5 .8) ? START_ON TOGGLE BLOCK_STOPS
A train is a mover that moves between path_corner target points.
Trains MUST HAVE AN ORIGIN BRUSH.
The train spawns at the first target it is pointing at.
"model2"	.md3 model to also draw
"speed" 	default 100
"dmg"		default 2
"noise" 	looping sound to play when the train is in motion
"target"	next path corner
"color" 	constantLight color
"light" 	constantLight radius
*/
void SP_func_train (gentity_t *self)
{
	SP_InitMover ( self );

	VectorClear (self->s.angles);

	if (self->spawnflags & TRAIN_BLOCK_STOPS)
	{
		self->damage = 0;
	}
	else
	{
		if (!self->damage)
		{
			self->damage = 2;
		}
	}

	if (!self->speed)
	{
		self->speed = 100;
	}

	if (!self->target)
	{
		G_FreeEntity( self );
		return;
	}

	trap_SetBrushModel( self, self->model );
	InitMover( self );

	self->mover->reached = Reached_Train;

	// start trains on the second frame, to make sure their targets have had
	// a chance to spawn
	self->nextthink = level.time + FRAMETIME;
	self->think = Think_SetupTrainTargets;
}

/*
===============
Think_UTBeginMoving
GottaBeKD, January 14, 2001

A request to move on has come, so start moving again
===============
*/
void Think_UTBeginMoving( gentity_t *ent )
{
	// start it going
	SetMoverState( ent, MOVER_1TO2, level.time );

	ent->s.pos.trTime = level.time;
	ent->s.pos.trType = TR_LINEAR_STOP;
}

/*
===============
Reached_UTTrain
GottaBeKD, January 14, 2001

Arrived at stop
===============
*/
void Reached_UTTrain( gentity_t *ent, gentity_t *other )
{
	int    deltaTime;
	gentity_t  *next;
//	gentity_t  *curr;
	gentity_t  *interfaceEnt;

	if(ent->mover->rotationTime)
	{
		deltaTime = (ent->mover->rotationTime) * 0.001; 	// milliseconds to seconds

		VectorMA(	ent->s.apos.trBase, deltaTime, ent->s.apos.trDelta, ent->s.apos.trBase );
	}

	ent->s.pos.trType  = TR_STATIONARY;
	ent->s.apos.trType = TR_STATIONARY;

	next		   = ent->mover->nextTrain;

	// fire all other targets
	G_UseTargets( next, NULL );

	if(!next->mover->nextTrain)
	{
		VectorCopy( next->s.origin, ent->mover->pos1 );
		VectorCopy( next->s.origin, ent->mover->pos2 );
		SetMoverState( ent, MOVER_1TO2, level.time );
		ent->s.pos.trType	  = TR_STATIONARY;
		ent->s.apos.trType	  = TR_STATIONARY;
		ent->mover->currTrain = next;
		ent->think		  = Think_UTBeginMoving;

		if (ent->mover->stopSound && ent->mover->moving)
		{
			G_Sound ( ent, 0, ent->mover->stopSound );
		}
		ent->mover->moving = qfalse;

		//clear looping sound
		ent->s.loopSound = 0;

		if(ent->mover->soundLoop)
		{
			ent->s.loopSound = ent->mover->soundLoop;
		}

		return;
	}

	if ((interfaceEnt = G_Find (NULL, FOFS(targetname), ent->interfaceEnt)) != NULL)
	{
		if(interfaceEnt->mover->destination)
		{
			if(next == interfaceEnt->mover->destination)
			{
				VectorCopy( next->s.origin, ent->mover->pos1 );
				VectorCopy( next->s.origin, ent->mover->pos2 );
				SetMoverState( ent, MOVER_1TO2, level.time );
				ent->s.pos.trType	  = TR_STATIONARY;
				ent->s.apos.trType	  = TR_STATIONARY;
				ent->mover->currTrain = next;
				ent->think		  = Think_UTBeginMoving;

				if (ent->mover->stopSound && ent->mover->moving)
				{
					G_Sound ( ent, 0, ent->mover->stopSound );
				}
				ent->mover->moving = qfalse;

				//clear looping sound
				ent->s.loopSound = 0;

				if(ent->mover->soundLoop)
				{
					ent->s.loopSound = ent->mover->soundLoop;
				}

				return;
			}
		}
	}
}

/*
===============
Think_SetupUTTrainTargets
GottaBeKD: January 12, 2001

Link all the stops together
===============
*/
void Think_SetupUTTrainTargets( gentity_t *ent )
{
	gentity_t  *other;

	ent->mover->nextTrain = G_Find( NULL, FOFS(targetname), ent->target );

	if (!ent->mover->nextTrain)
	{
		vtos(ent->r.absmin) ;
		return;
	}
	other = ent;

	// start the train moving from the first stop
//	Reached_UTTrain( ent, other );

	ent->mover->currTrain = ent->mover->nextTrain;

	VectorCopy( ent->mover->nextTrain->s.origin, ent->mover->pos1 );
	VectorCopy( ent->mover->nextTrain->s.origin, ent->mover->pos2 );
	SetMoverState( ent, MOVER_1TO2, level.time );

	ent->mover->goTime = level.time + ent->mover->nextTrain->wait;

	ent->think	   = Think_UTBeginMoving;
	ent->s.pos.trType  = TR_STATIONARY;

	G_Sound ( ent, 0, ent->mover->soundLoop );
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : UTFindDesiredStop
// Description: Returns a path_ut_stop with matching targetname to parameter name
// Changlist  : Gotta 14 Jan 2001 created
///////////////////////////////////////////////////////////////////////////////////////////
gentity_t *UTFindDesiredStop(char *name )
{
	gentity_t  *next;

	// find a path_ut_stop that matches the name
	next = NULL;

	do
	{
		next = G_Find( next, FOFS(targetname), name );

		if (!next)
		{
			return next;
		}
	}
//	  while (strcmp( next->classname, "path_ut_stop" ));
	while (next->classhash!=HSH_path_ut_stop);

	return next;
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : UTSetDesiredStop
// Description: Sets up movement to the next stop
// Changlist  : Gotta 14 Jan 2001 created
///////////////////////////////////////////////////////////////////////////////////////////
void UTSetDesiredStop(gentity_t *ent, gentity_t *stop)
{
	gentity_t  *next;
	float	   speed;
	vec3_t	   move;
	float	   length;
	qboolean   needRotation;

	needRotation				= qfalse;

	ent->mover->currTrain			= ent->mover->nextTrain;

	ent->mover->nextTrain->mover->nextTrain = stop;

	next					= ent->mover->nextTrain;

	// looping sound
	ent->s.loopSound = ent->mover->soundLoop;

	if (!next || !next->mover->nextTrain)
	{
		return; 	// just stop
	}

	VectorCopy( ent->mover->nextTrain->s.origin, ent->mover->pos1 );
	// set the new trajectory
	ent->mover->nextTrain = next->mover->nextTrain;

	VectorCopy( next->mover->nextTrain->s.origin, ent->mover->pos2 );

	// if the path_ut_stop has a speed, use that
	if (next->speed)
	{
		speed = next->speed;
	}
	else
	{
		// otherwise use the train's speed
		speed = ent->speed;
	}

	if (speed < 1)
	{
		speed = 1;
	}

	// calculate duration
	VectorSubtract( ent->mover->pos2, ent->mover->pos1, move );
	length = VectorLength( move );

	// handle rotation
	if(next->mover->rotation[0])
	{
		ent->s.apos.trDelta[0] = next->mover->rotation[0];
		needRotation		   = qtrue;
	}
	else
	{
		ent->s.apos.trDelta[0] = 0;
	}

	if(next->mover->rotation[1])
	{
		ent->s.apos.trDelta[1] = next->mover->rotation[1];
		needRotation		   = qtrue;
	}
	else
	{
		ent->s.apos.trDelta[1] = 0;
	}

	if(next->mover->rotation[2])
	{
		ent->s.apos.trDelta[2] = next->mover->rotation[2];
		needRotation		   = qtrue;
	}
	else
	{
		ent->s.apos.trDelta[2] = 0;
	}

	if(needRotation)
	{
		// set the axis and time span of rotation
		ent->s.apos.trType = TR_LINEAR_STOP;
		ent->s.apos.trTime = level.time;

		if(next->mover->rotationTime)
		{
			ent->s.apos.trDuration = next->mover->rotationTime;
		}
		else
		{
			ent->s.apos.trDuration = 10000;
		}

		ent->mover->rotationTime = ent->s.apos.trDuration;

		VectorCopy( ent->s.apos.trBase, ent->r.currentAngles );
	}
	else
	{
		ent->s.apos.trType	 = TR_STATIONARY;
		ent->mover->rotationTime = 0;
//		ent->s.apos.trDelta[0] = 0;
//		ent->s.apos.trDelta[1] = 0;
//		ent->s.apos.trDelta[2] = 0;
	}

	ent->s.pos.trDuration = length * 1000 / speed;
}

/*
================
GottaBeKD: Used when a train button is pushed
Use_TrainButton
================
*/
void Use_TrainButton( gentity_t* nextStop, gentity_t *other, gentity_t *activator ) {

	int 		j;
	gentity_t	*train;
	gentity_t	*interfaceEnt;
	gentity_t	*dest;
	gentity_t	*test;

	// Loop through the entities looking for the train thats being used.
	for (j = 0; j < MAX_GENTITIES; j++) {

		train = &g_entities[j];

		// If it doesnt have a classname or isnt a mover, skip it
		if( !train->classname || !train->mover ) {
			continue;
		}

		// If its not a train, skip it
//		  if( Q_stricmp(train->classname, "func_ut_train") ) {
		if( train->classhash!=HSH_func_ut_train) {
			continue;
		}

		// If this isnt the right train then skip it
		if ( train->mover->trainID != nextStop->mover->trainID) {
			continue;
		}

		// Can't change the train while it's moving
		if(train->s.pos.trType != TR_STATIONARY){
			return;
		}

		// Make sure the next stop isnt the current stop
		if(!Q_stricmp(nextStop->targetname, train->mover->currTrain->targetname)) {
			return;
		}

		// Play the start sound if we are just starting to move
		if ( train->mover->startSound && !train->mover->moving ) {
			G_Sound ( train, 0, train->mover->startSound );
		}

		// trains now trigger stops as they get up to leave
		if(train->mover->moving == qfalse) {
			train->mover->moving = qtrue;
			G_UseTargets(train->mover->nextTrain, NULL);
		}

		UTSetDesiredStop(train, nextStop);
		train->nextthink = train->mover->goTime;

		break;
	}

	dest = NULL;
	interfaceEnt = G_FindClassHash(NULL, HSH_func_keyboard_interface);
	if(interfaceEnt){
		test = G_Find (NULL, FOFS(targetname), interfaceEnt->mover->stop1);
		if(nextStop == test){
			dest = G_Find (NULL, FOFS(targetname), interfaceEnt->mover->stop1);
		}
		test = G_Find (NULL, FOFS(targetname), interfaceEnt->mover->stop2);
		if(nextStop == test){
			dest = G_Find (NULL, FOFS(targetname), interfaceEnt->mover->stop2);
		}
		test = G_Find (NULL, FOFS(targetname), interfaceEnt->mover->stop3);
		if(nextStop == test){
			dest = G_Find (NULL, FOFS(targetname), interfaceEnt->mover->stop3);
		}
		test = G_Find (NULL, FOFS(targetname), interfaceEnt->mover->stop4);
		if(nextStop == test){
			dest = G_Find (NULL, FOFS(targetname), interfaceEnt->mover->stop4);
		}
		test = G_Find (NULL, FOFS(targetname), interfaceEnt->mover->stop5);
		if(nextStop == test){
			dest = G_Find (NULL, FOFS(targetname), interfaceEnt->mover->stop5);
		}

		if(dest && interfaceEnt){
			interfaceEnt->mover->destination = dest;
		}
	}

}

// GottaBeKD
/*QUAKED path_ut_stop (.5 .3 0) (-8 -8 -8) (8 8 8)
Train path stops.
Target: next path stop and other targets to fire
"speed" speed to move to the next stop
"wait" seconds to wait before behining move to next stop
*/
void SP_path_ut_stop( gentity_t *self )
{
	SP_InitMover ( self );

	if (!self->targetname)
	{
		G_FreeEntity( self );
		return;
	}

	G_SpawnInt( "trainID", "1", &self->mover->trainID );

	G_SpawnFloat( "pitchSpeed", "0", &self->mover->rotation[2] );
	G_SpawnFloat( "yawSpeed", "0", &self->mover->rotation[1] );
	G_SpawnFloat( "rollSpeed", "0", &self->mover->rotation[0] );
	G_SpawnInt( "rotationTime", "0", &self->mover->rotationTime );

	G_SpawnFloat( "wait", "0", &self->wait );  // default 0 seconds wait at stop

	self->use = Use_TrainButton;
}

/*
================
blocked_ut_train

handles what happens
if an entity blocks
a ut_train's movement

eg: in ut_sands, someone
drops an item in the
chopper or throws a
knife into it.

================
*/
void blocked_ut_train( gentity_t *ent, gentity_t *other )
{
	// remove anything other than a client
	if (!other->client)
	{
		// except CTF flags!!!!
		if((other->s.eType == ET_ITEM) && (other->item->giType == IT_TEAM))
		{
			Team_DroppedFlagThink( other );
			return;
		}
		else if(other->s.eType == ET_MISSILE)	 // and missiles
		{
			return;
		}
		G_FreeEntity( other );
		return;
	}

	if (ent->damage && (ent->s.apos.trType == TR_STATIONARY))
	{
		G_Damage( other, ent, ent, NULL, NULL, ent->damage, 0, MOD_CRUSH, HL_UNKNOWN );
	}
}

// GottaBeKD
/*QUAKED func_ut_train (0 .5 .8) ? START_ON TOGGLE BLOCK_STOPS
A train is a mover that moves between path_ut_stop target points.
Trains MUST HAVE AN ORIGIN BRUSH.
The train spawns at the first target it is pointing at.
"model2"	.md3 model to also draw
"name"		name the train. used to associate an elevator with it's buttons
"speed" 	default 100
"dmg"		default 2
"noise" 	looping sound to play when the train is in motion
"target"	next path corner
"color" 	constantLight color
"light" 	constantLight radius
*/
void SP_func_ut_train (gentity_t *self)
{
	char  *interfaceStr;
	char  *startSound;
	char  *stopSound;
	char  *noise;
	int   soundIndex;

	SP_InitMover ( self );

	G_SpawnInt( "id", "0", &self->mover->trainID );

	VectorClear (self->s.angles);

	if (self->spawnflags & TRAIN_BLOCK_STOPS)
	{
		self->damage = 0;
	}
	else
	{
		if (!self->damage)
		{
			self->damage = 0;
		}
	}

	if (!self->speed)
	{
		self->speed = 350;
	}

	if (!self->target)
	{
		G_FreeEntity( self );
		return;
	}

	G_SpawnString( "interface", "", &interfaceStr );

	if (interfaceStr[0])
	{
		self->interfaceEnt = (char *)bmalloc ( strlen ( interfaceStr ) + 1 );
		strcpy ( self->interfaceEnt, interfaceStr );
	}

	trap_SetBrushModel( self, self->model );
	InitMover( self );

	self->mover->reached = Reached_UTTrain;

	// start trains on the second frame, to make sure their targets have had
	// a chance to spawn
	self->nextthink = level.time + FRAMETIME;
	self->think = Think_SetupUTTrainTargets;

	G_SpawnString( "stopSound", "", &stopSound );

	if(stopSound && stopSound[0])
	{
		soundIndex		   = G_SoundIndex( stopSound );
		self->mover->stopSound = soundIndex;
	}

	G_SpawnString( "startSound", "", &startSound );

	if(startSound && startSound[0])
	{
		soundIndex		= G_SoundIndex( startSound );
		self->mover->startSound = soundIndex;
	}

	G_SpawnString( "noise", "", &noise );

	if(noise && noise[0])
	{
		soundIndex		   = G_SoundIndex( noise );
		self->mover->soundLoop = soundIndex;
		self->s.loopSound	   = soundIndex;
	}

	self->mover->moving  = qfalse;

	self->mover->blocked = blocked_ut_train;
}

/*
================
GottaBeKD: Used when a keyboard interface is triggered
Use_KeyboardInterface
================
*/
void Use_KeyboardInterface( gentity_t *ent, gentity_t *other, gentity_t *activator ) {

	gentity_t *train;

	train = NULL;
	if(ent->mover->display){

		while((train = G_FindClassHash (train, HSH_func_ut_train)) != NULL){

			if(!Q_stricmp(train->interfaceEnt, ent->targetname)){
				if(!Q_stricmp(train->mover->currTrain->targetname, ent->mover->stop1)){
					trap_SendServerCommand( activator->client->ps.clientNum, va("ui_keyboardinterface %d %s %s %s %s %s %s\n", 0, ent->mover->option1, ent->mover->option2, ent->mover->option3, ent->mover->option4, ent->mover->option5) );
				}
				else if(!Q_stricmp(train->mover->currTrain->targetname, ent->mover->stop2)){
					trap_SendServerCommand( activator->client->ps.clientNum, va("ui_keyboardinterface %d %s %s %s %s %s %s\n", 1, ent->mover->option1, ent->mover->option2, ent->mover->option3, ent->mover->option4, ent->mover->option5) );
				}
				else if(!Q_stricmp(train->mover->currTrain->targetname, ent->mover->stop3)){
					trap_SendServerCommand( activator->client->ps.clientNum, va("ui_keyboardinterface %d %s %s %s %s %s %s\n", 2, ent->mover->option1, ent->mover->option2, ent->mover->option3, ent->mover->option4, ent->mover->option5) );
				}
				else if(!Q_stricmp(train->mover->currTrain->targetname, ent->mover->stop4)){
					trap_SendServerCommand( activator->client->ps.clientNum, va("ui_keyboardinterface %d %s %s %s %s %s %s\n", 3, ent->mover->option1, ent->mover->option2, ent->mover->option3, ent->mover->option4, ent->mover->option5) );
				}
				else if(!Q_stricmp(train->mover->currTrain->targetname, ent->mover->stop5)){
					trap_SendServerCommand( activator->client->ps.clientNum, va("ui_keyboardinterface %d %s %s %s %s %s %s\n", 4, ent->mover->option1, ent->mover->option2, ent->mover->option3, ent->mover->option4, ent->mover->option5) );
				}
			}
		}
	}
	else {

		if(!ent->mover->option3 && !ent->mover->option4 && !ent->mover->option5){
			Switch_Train_Stops ( activator, ent->mover->trainID );
		}
	}
}

// GottaBeKD
/*QUAKED func_keyboard_interface (.5 .3 0) (-8 -8 -8) (8 8 8)
keyboard menu interface
"option#" text for the given option
"target#" target entity for the given option
*/
void SP_func_keyboard_interface( gentity_t *self ) {

	char			*option1;
	char			*option2;
	char			*option3;
	char			*option4;
	char			*option5;

	char			*stop1;
	char			*stop2;
	char			*stop3;
	char			*stop4;
	char			*stop5;

	char			*stop1from2;
	char			*stop1from3;
	char			*stop1from4;
	char			*stop1from5;

	char			*stop2from1;
	char			*stop2from3;
	char			*stop2from4;
	char			*stop2from5;

	char			*stop3from1;
	char			*stop3from2;
	char			*stop3from4;
	char			*stop3from5;

	char			*stop4from1;
	char			*stop4from2;
	char			*stop4from3;
	char			*stop4from5;

	char			*stop5from1;
	char			*stop5from2;
	char			*stop5from3;
	char			*stop5from4;

	char			*s;

	SP_InitMover ( self );
	G_SpawnInt( "display", "1", &self->mover->display );
	G_SpawnInt( "id", "0", &self->mover->trainID );

	G_SpawnString( "option1", "", &option1 );
	if(option1[0]) {
		self->mover->option1 = bmalloc ( strlen ( option1 ) + 1 );
		strcpy ( self->mover->option1, option1 );
		for ( s = self->mover->option1; *s; s++ )
			 if ( *s == ' ' ) *s = '$';
	}

	G_SpawnString( "stop1", "stop1", &stop1 );
	self->mover->stop1 = bmalloc ( strlen ( stop1 ) + 1 );
	strcpy( self->mover->stop1, stop1 );

	G_SpawnString( "option2", "", &option2 );
	if(option2[0]){
		self->mover->option2 = bmalloc ( strlen ( option2 ) + 1 );
		strcpy ( self->mover->option2, option2 );
		for ( s = self->mover->option2; *s; s++ )
			 if ( *s == ' ' ) *s = '$';
	}

	G_SpawnString( "stop2", "stop2", &stop2 );
	self->mover->stop2 = bmalloc ( strlen ( stop2 ) + 1 );
	strcpy( self->mover->stop2, stop2 );

	G_SpawnString( "option3", "", &option3 );
	if(option3[0]){
		self->mover->option3 = bmalloc ( strlen ( option3 ) + 1 );
		strcpy ( self->mover->option3, option3 );
		for ( s = self->mover->option3; *s; s++ )
			 if ( *s == ' ' ) *s = '$';
	}

	G_SpawnString( "stop3", "stop3", &stop3 );
	self->mover->stop3 = bmalloc ( strlen ( stop3 ) + 1 );
	strcpy( self->mover->stop3, stop3 );

	G_SpawnString( "option4", "", &option4 );
		if(option4[0]){
		self->mover->option4 = bmalloc ( strlen ( option4 ) + 1 );
		strcpy ( self->mover->option4, option4 );
		for ( s = self->mover->option4; *s; s++ )
			 if ( *s == ' ' ) *s = '$';
	}

	G_SpawnString( "stop4", "stop4", &stop4 );
	self->mover->stop4 = bmalloc ( strlen ( stop4 ) + 1 );
	strcpy( self->mover->stop4, stop4 );

	G_SpawnString( "option5", "", &option5 );
	if(option5[0]){
	self->mover->option5 = bmalloc ( strlen ( option5 ) + 1 );
		strcpy ( self->mover->option5, option5 );
		for ( s = self->mover->option5; *s; s++ )
			 if ( *s == ' ' ) *s = '$';
	}

	G_SpawnString( "stop5", "stop5", &stop5 );
	self->mover->stop5 = bmalloc ( strlen ( stop5 ) + 1 );
	strcpy( self->mover->stop5, stop5 );

	G_SpawnString( "stop1from2", "stop1from2", &stop1from2 );
	self->mover->stop1from2 = bmalloc ( strlen ( stop1from2 ) + 1 );
	strcpy( self->mover->stop1from2, stop1from2 );

	G_SpawnString( "stop1from3", "stop1from3", &stop1from3 );
	self->mover->stop1from3 = bmalloc ( strlen ( stop1from3 ) + 1 );
	strcpy( self->mover->stop1from3, stop1from3 );

	G_SpawnString( "stop1from4", "stop1from4", &stop1from4 );
	self->mover->stop1from4 = bmalloc ( strlen ( stop1from4 ) + 1 );
	strcpy( self->mover->stop1from4, stop1from4 );

	G_SpawnString( "stop1from5", "stop1from5", &stop1from5 );
	self->mover->stop1from5 = bmalloc ( strlen ( stop1from5 ) + 1 );
	strcpy( self->mover->stop1from5, stop1from5 );

	G_SpawnString( "stop2from1", "stop2from1", &stop2from1 );
	self->mover->stop2from1 = bmalloc ( strlen ( stop2from1 ) + 1 );
	strcpy( self->mover->stop2from1, stop2from1 );

	G_SpawnString( "stop2from3", "stop2from3", &stop2from3 );
	self->mover->stop2from3 = bmalloc ( strlen ( stop2from3 ) + 1 );
	strcpy( self->mover->stop2from3, stop2from3 );

	G_SpawnString( "stop2from4", "stop2from4", &stop2from4 );
	self->mover->stop2from4 = bmalloc ( strlen ( stop2from4 ) + 1 );
	strcpy( self->mover->stop2from4, stop2from4 );

	G_SpawnString( "stop2from5", "stop2from5", &stop2from5 );
	self->mover->stop2from5 = bmalloc ( strlen ( stop2from5 ) + 1 );
	strcpy( self->mover->stop2from5, stop2from5 );

	G_SpawnString( "stop3from1", "stop3from1", &stop3from1 );
	self->mover->stop3from1 = bmalloc ( strlen ( stop3from1 ) + 1 );
	strcpy( self->mover->stop3from1, stop3from1 );

	G_SpawnString( "stop3from2", "stop3from2", &stop3from2 );
	self->mover->stop3from2 = bmalloc ( strlen ( stop3from2 ) + 1 );
	strcpy( self->mover->stop3from2, stop3from2 );

	G_SpawnString( "stop3from4", "stop3from4", &stop3from4 );
	self->mover->stop3from4 = bmalloc ( strlen ( stop3from4 ) + 1 );
	strcpy( self->mover->stop3from4, stop3from4 );

	G_SpawnString( "stop3from5", "stop3from5", &stop3from5 );
	self->mover->stop3from5 = bmalloc ( strlen ( stop3from5 ) + 1 );
	strcpy( self->mover->stop3from5, stop3from5 );

	G_SpawnString( "stop4from1", "stop4from1", &stop4from1 );
	self->mover->stop4from1 = bmalloc ( strlen ( stop4from1 ) + 1 );
	strcpy( self->mover->stop4from1, stop4from1 );

	G_SpawnString( "stop4from2", "stop4from2", &stop4from2 );
	self->mover->stop4from2 = bmalloc ( strlen ( stop4from2 ) + 1 );
	strcpy( self->mover->stop4from2, stop4from2 );

	G_SpawnString( "stop4from3", "stop4from3", &stop4from3 );
	self->mover->stop4from3 = bmalloc ( strlen ( stop4from3 ) + 1 );
	strcpy( self->mover->stop4from3, stop4from3 );

	G_SpawnString( "stop4from5", "stop4from5", &stop4from5 );
	self->mover->stop4from5 = bmalloc ( strlen ( stop4from5 ) + 1 );
	strcpy( self->mover->stop4from5, stop4from5 );

	G_SpawnString( "stop5from1", "stop5from1", &stop5from1 );
	self->mover->stop5from1 = bmalloc ( strlen ( stop5from1 ) + 1 );
	strcpy( self->mover->stop5from1, stop5from1 );

	G_SpawnString( "stop5from2", "stop5from2", &stop5from2 );
	self->mover->stop5from2 = bmalloc ( strlen ( stop5from2 ) + 1 );
	strcpy( self->mover->stop5from2, stop5from2 );

	G_SpawnString( "stop5from3", "stop5from3", &stop5from3 );
	self->mover->stop5from3 = bmalloc ( strlen ( stop5from3 ) + 1 );
	strcpy( self->mover->stop5from3, stop5from3 );

	G_SpawnString( "stop5from4", "stop5from4", &stop5from4 );
	self->mover->stop5from4 = bmalloc ( strlen ( stop5from4 ) + 1 );
	strcpy( self->mover->stop5from4, stop5from4 );

	trap_SetBrushModel( self, self->model );
	self->use = Use_KeyboardInterface;

	trap_LinkEntity( self );
}

/*
===============================================================================

STATIC

===============================================================================
*/

/*QUAKED func_static (0 .5 .8) ?
A bmodel that just sits there, doing nothing.  Can be used for conditional walls and models.
"model2"	.md3 model to also draw
"color" 	constantLight color
"light" 	constantLight radius
*/
void SP_func_static( gentity_t *ent )
{
	SP_InitMover ( ent );

	trap_SetBrushModel( ent, ent->model );
	InitMover( ent );
	VectorCopy( ent->s.origin, ent->s.pos.trBase );
	VectorCopy( ent->s.origin, ent->r.currentOrigin );
}

/*
===============================================================================

ROTATING

===============================================================================
*/

/*QUAKED func_rotating (0 .5 .8) ? START_ON - X_AXIS Y_AXIS
You need to have an origin brush as part of this entity.  The center of that brush will be
the point around which it is rotated. It will rotate around the Z axis by default.	You can
check either the X_AXIS or Y_AXIS box to change that.

"model2"	.md3 model to also draw
"speed" 	determines how fast it moves; default value is 100.
"dmg"		damage to inflict when blocked (2 default)
"color" 	constantLight color
"light" 	constantLight radius
*/
void SP_func_rotating (gentity_t *ent)
{
	SP_InitMover ( ent );

	if (!ent->speed)
	{
		ent->speed = 100;
	}

	// set the axis of rotation
	ent->s.apos.trType = TR_LINEAR;

	if (ent->spawnflags & 4)
	{
		ent->s.apos.trDelta[2] = ent->speed;
	}
	else if (ent->spawnflags & 8)
	{
		ent->s.apos.trDelta[0] = ent->speed;
	}
	else
	{
		ent->s.apos.trDelta[1] = ent->speed;
	}

	if (!ent->damage)
	{
		ent->damage = 2;
	}

	trap_SetBrushModel( ent, ent->model );
	InitMover( ent );

	VectorCopy( ent->s.origin, ent->s.pos.trBase );
	VectorCopy( ent->s.pos.trBase, ent->r.currentOrigin );
	VectorCopy( ent->s.apos.trBase, ent->r.currentAngles );

	trap_LinkEntity( ent );
}

/*
===============================================================================

BOBBING

===============================================================================
*/

/*QUAKED func_bobbing (0 .5 .8) ? X_AXIS Y_AXIS
Normally bobs on the Z axis
"model2"	.md3 model to also draw
"height"	amplitude of bob (32 default)
"speed" 	seconds to complete a bob cycle (4 default)
"phase" 	the 0.0 to 1.0 offset in the cycle to start at
"dmg"		damage to inflict when blocked (2 default)
"color" 	constantLight color
"light" 	constantLight radius
*/
void SP_func_bobbing (gentity_t *ent)
{
	float  height;
	float  phase;

	SP_InitMover ( ent );

	G_SpawnFloat( "speed", "4", &ent->speed );
	G_SpawnFloat( "height", "32", &height );
	G_SpawnInt( "dmg", "2", &ent->damage );
	G_SpawnFloat( "phase", "0", &phase );

	trap_SetBrushModel( ent, ent->model );
	InitMover( ent );

	VectorCopy( ent->s.origin, ent->s.pos.trBase );
	VectorCopy( ent->s.origin, ent->r.currentOrigin );

	ent->s.pos.trDuration = ent->speed * 1000;
	ent->s.pos.trTime	  = ent->s.pos.trDuration * phase;
	ent->s.pos.trType	  = TR_SINE;

	// set the axis of bobbing
	if (ent->spawnflags & 1)
	{
		ent->s.pos.trDelta[0] = height;
	}
	else if (ent->spawnflags & 2)
	{
		ent->s.pos.trDelta[1] = height;
	}
	else
	{
		ent->s.pos.trDelta[2] = height;
	}
}

/*
===============================================================================

PENDULUM

===============================================================================
*/

/*QUAKED func_pendulum (0 .5 .8) ?
You need to have an origin brush as part of this entity.
Pendulums always swing north / south on unrotated models.  Add an angles field to the model to allow rotation in other directions.
Pendulum frequency is a physical constant based on the length of the beam and gravity.
"model2"	.md3 model to also draw
"speed" 	the number of degrees each way the pendulum swings, (30 default)
"phase" 	the 0.0 to 1.0 offset in the cycle to start at
"dmg"		damage to inflict when blocked (2 default)
"color" 	constantLight color
"light" 	constantLight radius
*/
void SP_func_pendulum(gentity_t *ent)
{
	char	  *key;
	char	  *num;
	float	  freq;
	float	  length;
	float	  phase;
	float	  speed;
	int   i;
	qboolean  active = qtrue;

	if (G_SpawnString("gametype", "", &key))
	{
		active = qfalse;

		for (i = 0; i < strlen(key); i++)
		{
			num = va("%d", g_gametype.integer);

			if (key[i] == num[0])
			{
				active = qtrue;

				break;
			}
		}
	}

	// If not active in this gametype.
	if (!active)
	{
		return;
	}

	SP_InitMover ( ent );

	G_SpawnFloat( "speed", "30", &speed );
	G_SpawnInt( "dmg", "2", &ent->damage );
	G_SpawnFloat( "phase", "0", &phase );

	trap_SetBrushModel( ent, ent->model );

	// find pendulum length
	length = fabs( ent->r.mins[2] );

	if (length < 8)
	{
		length = 8;
	}

	freq			  = 1 / (M_PI * 2) * sqrt( g_gravity.value / (3 * length));

	ent->s.pos.trDuration = (1000 / freq);

	InitMover( ent );

	VectorCopy( ent->s.origin, ent->s.pos.trBase );
	VectorCopy( ent->s.origin, ent->r.currentOrigin );

	VectorCopy( ent->s.angles, ent->s.apos.trBase );

	ent->s.apos.trDuration = 1000 / freq;
	ent->s.apos.trTime	   = ent->s.apos.trDuration * phase;
	ent->s.apos.trType	   = TR_SINE;
	ent->s.apos.trDelta[2] = speed;
}

///////////////////////////////////////////////////////////////////////////////////////
// Urban Terror Movers
///////////////////////////////////////////////////////////////////////////////////////

/*
=======================
ROTATING DOOR
=======================

The values this guy cares about are:


*/

void SP_func_rotating_door ( gentity_t *ent )
{
	int   direction;
	char  *sound;
	char  *chr;
	char  *team;
	int   j;

	SP_InitMover ( ent );

	ent->mover->sound1to2 = ent->mover->sound2to1 = G_SoundIndex("sound/movers/doors/dr1_strt.wav");
	ent->mover->soundPos1 = ent->mover->soundPos2 = G_SoundIndex("sound/movers/doors/dr1_end.wav");

	if (G_SpawnString( "pos1", "", &sound ))
	{
		if (Q_stricmp( sound, "none" ) == 0)
		{
			ent->mover->sound1to2 = ent->mover->sound2to1 = 0;
		}
		else
		{
			ent->mover->sound1to2 = ent->mover->sound2to1 = G_SoundIndex( sound );
		}
	}

	if (G_SpawnString( "pos2", "", &sound ))
	{
		if (Q_stricmp( sound, "none" ) == 0)
		{
			ent->mover->soundPos1 = ent->mover->soundPos2 = 0;
		}
		else
		{
			ent->mover->soundPos1 = ent->mover->soundPos2 = G_SoundIndex( sound );
		}
	}

	// Team based opening - DensitY
	if(G_SpawnString( "only", "", &team ) && (g_gametype.integer >= GT_TEAM) && (g_gametype.integer != GT_JUMP))
	{
		if (ent->mover)
		{
			if(!Q_stricmp( team, "red" ))
			{
				ent->mover->team = TEAM_RED;
			}
			else if(!Q_stricmp( team, "blue" ))
			{
				ent->mover->team = TEAM_BLUE;
			}
			else
			{
				ent->mover->team = TEAM_SPECTATOR;
			}
		}
	}
	else
	{
		if (ent->mover)
		{
			ent->mover->team = TEAM_SPECTATOR;
		}
	}

	ent->mover->blocked = Blocked_Door;

	ent->angle		= 0;

	if (ent->speed >= 0)				// set opening direction based on speed
	{
		direction = 1;
	}
	else
	{
		direction = -1;
	}

	G_SpawnInt( "direction", "0", &ent->mover->direction ); // so mapper can force doors to open only one way

	if (ent->mover->direction) // make sure direction is only 1 or -1
	{
		ent->mover->direction /= abs(ent->mover->direction);
		direction		   = ent->mover->direction;
	}

	if (ent->speed == 0)
	{
		ent->speed = 100;
	}

	if (!ent->wait)
	{
		ent->wait = 0;			// doors with no explicit wait value do not shut unless asked to (normal doors)
	}
	ent->wait *= 1000;

	// set the axis of rotation
	ent->s.apos.trType	   = TR_INTERPOLATE;

	ent->s.apos.trDelta[0] = 0;
	ent->s.apos.trDelta[1] = ent->speed;
	ent->s.apos.trDelta[2] = 0;

	ent->movedir[0] 	   = 0;
	ent->movedir[1] 	   = 1;
	ent->movedir[2] 	   = 0;

	if (G_SpawnString( "axis", "", &sound ))
	{
		if (sound[0] == '0')
		{
			ent->s.apos.trDelta[0] = ent->speed;
			ent->s.apos.trDelta[1] = 0;
			ent->s.apos.trDelta[2] = 0;
			ent->movedir[0] 	   = 1;
			ent->movedir[1] 	   = 0;
			ent->movedir[2] 	   = 0;
		}

		if (sound[0] == '1')
		{
			ent->s.apos.trDelta[0] = 0;
			ent->s.apos.trDelta[1] = ent->speed;
			ent->s.apos.trDelta[2] = 0;
			ent->movedir[0] 	   = 0;
			ent->movedir[1] 	   = 1;
			ent->movedir[2] 	   = 0;
		}

		if (sound[0] == '2')
		{
			ent->s.apos.trDelta[0] = 0;
			ent->s.apos.trDelta[1] = 0;
			ent->s.apos.trDelta[2] = ent->speed;
			ent->movedir[0] 	   = 0;
			ent->movedir[1] 	   = 0;
			ent->movedir[2] 	   = 1;
		}
	}

	if (!ent->damage)
	{
		ent->damage = 0;
	}

	trap_SetBrushModel( ent, ent->model );
	InitMover( ent );

//	ent->s.eFlags |= EF_MOVER_STOP; // stop if it hits someone - means door won't push people

	VectorCopy( ent->s.origin, ent->s.pos.trBase );
	VectorCopy( ent->s.pos.trBase, ent->r.currentOrigin );
	VectorCopy( ent->s.apos.trBase, ent->r.currentAngles );

	VectorCopy ( ent->s.angles, ent->mover->apos1 );
	VectorCopy ( ent->s.angles, ent->mover->apos2 );

	ent->nextthink = level.time + FRAMETIME;
	ent->think	   = Think_MatchTeam;

	ent->count	   = 0;

	if (G_SpawnString( "OneWay", "", &sound ))
	{
		ent->count = 1;
	}

	// 27 let certain gamemodes lock doors open
	if (G_SpawnString( "GameMode_Open", "", &sound ))
	{
		for (j = 0; j < strlen(sound); j++)
		{
			if ((g_gametype.integer == GT_JUMP) || (g_gametype.integer == GT_LASTMAN))
			{
				chr = va("%d", GT_FFA);
			}
			else
			{
				chr = va("%d", g_gametype.integer);
			}

			if (sound[j] == chr[0]) //Lock the door OPEN in this gamemode
			{
				ent->s.apos.trBase[0]  = (90 * direction) * ent->movedir[0];
				ent->s.apos.trBase[1]  = (90 * direction) * ent->movedir[1];
				ent->s.apos.trBase[2]  = (90 * direction) * ent->movedir[2];

				ent->mover->moverState = MOVER_LOCKED; // door wont go anywhere
			}
		}
	}

	// 27 let certain gamemodes lock doors shut
	if (G_SpawnString( "GameMode_Shut", "", &sound ))
	{
		for (j = 0; j < strlen(sound); j++)
		{
			if ((g_gametype.integer == GT_JUMP) || (g_gametype.integer == GT_LASTMAN))
			{
				chr = va("%d", GT_FFA);
			}
			else
			{
				chr = va("%d", g_gametype.integer);
			}

			if (sound[j] == chr[0]) //Lock the door SHUT in this gamemode
			{
				ent->mover->moverState = MOVER_LOCKED; // door wont go anywhere
			}
		}
	}
}

/* ======== func_breakable stuff ========= */

/*QUAKED func_breakable (0 .5 .8) ?
A breakable thing just sits there, doing nothing,
but will break if damaged or collided with.

"model2"	.md3 model to also draw
"color" 	constantLight color
"light" 	constantLight radius
"type"		0: glass (not windows!!) 1: wood 2: ceramic 3: plastic 4: window
"axis"		1 - x, 2 - y, 4 - z
"health"	health of object
"shards"	number of shards object should shatter into
"thickness" how thick this window is for type 4 breakables only

See Urban Terror readme file for details about
this entity.

*/

/*
=========================
	Use_Breakable
	Use_Breakable is called when the object is damaged
=========================
*/

void Use_Breakable( gentity_t *ent, gentity_t *other, gentity_t *activator )
{
	// this function is entered by:
	//	a hit in G_Combat (targ->use (targ, NULL, NULL))
	//	a touch
	//	a think breakable weight

	vec3_t	dir;

	// don't shatter if health has not been exceeded
	if (ent->health > 0)
	{
		return;
	}

	// store the breakable	object's bounds - used in shatter effect
	// have to use origin since there is no way to get bounds info
	// on the client :(
	if (ent->model2)
	{
		VectorCopy(ent->r.absmin, ent->s.origin);
		VectorCopy(ent->r.absmax, ent->s.origin2);
	}
	else
	{
		VectorCopy(ent->r.mins, ent->s.origin);
		VectorCopy(ent->r.maxs, ent->s.origin2);
	}

	// work out a direction vector for where the damage is coming from
	// if other is a client, then it will set it in G_Damage or Touch_ below
	if (other && !other->client)
	{
		VectorSet(ent->s.angles2, 0, 0, 0); // if other is not a client, just shatter evenly
	}

	// send the trace normal
	VectorSet( dir, 0.0f, 0.0f, 1.0f ); // for now; later this needs to be a trace
	ent->s.time2 = DirToByte( dir );

	// remove the entity so that people won't collide with it
	// but don't unlink it yet or the event won't work
	ent->s.eType	= ET_INVISIBLE;
	ent->r.contents = 0;
	ent->s.solid	= 0;

	// only unlink the ent after the EV_BREAK event
	ent->unlinkAfterEvent = qtrue;

	// Bombale parameter.
	ent->s.frame = ent->bombable;

	// send the break object event
	// pack breakshards into bits 0 to 5 and breaktype into bits 6, 7 and 8
	//parm = 0;
	//parm = (ent->breaktype << 6) + (ent->breakshards & 0x3F);
	//G_AddEvent( ent, EV_UT_BREAK, parm );

	// Set break type.
	ent->s.generic1 = ent->breaktype;

	// Send the event.
	G_AddEvent(ent, EV_UT_BREAK, ent->breakshards);
}

/*
=========================
	Think_BreakableWeight
	called after someone stands on weak glass
=========================
*/

void Think_BreakableWeight ( gentity_t *ent )
{
	ent->nextthink = -1;		// no more thinks until reactivated
	ent->health--;			// damage the object
	Use_Breakable(ent, ent, ent);	// see if it breaks
}

/*
=========================
	Touch_Breakable
	Called if a player touches a breakable object
=========================
*/
void Touch_Breakable(gentity_t *ent, gentity_t *other, trace_t *trace )
{
	float	  damage;
	vec3_t	  maxs, mins;
	int   best = 0;
	int   i, legs;
	qboolean  axisoverride;

	if (!other->client)
	{
		return;
	}

	// If bombable.
	if (ent->bombable)
	{
		return;
	}

	// If metal - explosives, hk69.
	if ((ent->breaktype == 5) || (ent->breaktype == 6))
	{
		return;
	}

	// If stone - explosives, hk69.
	if ((ent->breaktype == 7) || (ent->breaktype == 8))
	{
		return;
	}

	// If wood - explosives, hk69.
	if ((ent->breaktype == 9) || (ent->breaktype == 10))
	{
		return;
	}

	legs = other->client->ps.legsAnim & 127;	// remove toggle bit

	// find thinnest axis, and assume that axis is the one
	// that the player will be smashing through.
	// algorithm fails if the brush is angled like this: \ or like this: /
	VectorCopy (ent->r.absmin, mins);
	VectorCopy (ent->r.absmax, maxs);

	axisoverride = qfalse;

	// find the thinnest axis
	if ((!ent->breakaxis) || (ent->breakaxis > 4))
	{
		// if the mapper has not overriden, try to guess
		// thin axis
		best = 0;

		for (i = 1 ; i < 3 ; i++)
		{
			if (maxs[i] - mins[i] < maxs[best] - mins[best])
			{
				best = i;
			}
		}
	}
	else
	{
		// mapper has told us which axis to consider "thin"
		axisoverride = qtrue;

		switch (ent->breakaxis)
		{
			case 1:
				best = 1;
				break;

			case 2:
				best = 0;
				break;

			case 4:
				best = 2;
				break;
		}
	}

	// as an extra hack to determine if the player is standing on a sheet
	// of glass we can use the groundentitynum
	if (other->client->ps.groundEntityNum == ent->s.number)
	{
		axisoverride = qtrue;
		best		 = 2;
	}

	// determine if we should damage the glass
	damage = 0;

	if (((maxs[best] - mins[best]) > 10) && (!axisoverride))
	{
		// the object is thick (thick glass, box) or an object (eg: vase)
		// always damage objects of this type
		// only take damage if player is airbourne
		if (other->client->ps.groundEntityNum == ENTITYNUM_NONE)
		{
			damage = 1;
		}
	}
	else
	{
		// the object has a thin axis, so only damage if
		// the movement component of the player is in
		// the direction of the thinnest axis... this should
		// allow people to walk about on glass roofs and only
		// break them if they jump on them
		switch(best)
		{
			case 0: // note: x and y are swapped

				if	(other->client->ps.velocity[1] &&
					 (other->client->ps.groundEntityNum == ENTITYNUM_NONE))
				{
					damage = 1;
				}
				// only smash if client is airborne and moving in right direction
				break;

			case 1:

				if	(other->client->ps.velocity[0] &&
					 (other->client->ps.groundEntityNum == ENTITYNUM_NONE))
				{
					damage = 1;
				}
				VectorSet(ent->s.angles2, 0, 1, 0); // shatter with a y component
				break;

			case 2:

				// if (other->client->ps.velocity[2]) damage = 1;
				// velocity in z may be 0 for a jump onto glass, so if legs are in land
				// anim mode, then the glass should take damage
				if ((legs == LEGS_LAND) || (legs == LEGS_BACKLAND))
				{
					damage = 1; // player was landing on the glass
					break;
				}

				// if player is standing on glass and its health is low
				// then it should crumble after a few seconds
				if ((ent->health < 10) && (ent->nextthink == -1))
				{
					// if health is already 1, just smash it otherwise
					// players can wander off the glass and it will
					// smash after they're not on it anymore
					if (ent->health == 1)
					{
						damage = 1;
						break;
					}
					ent->think	   = Think_BreakableWeight;
					ent->nextthink = level.time + 1000; // damage it a second from now
				}
				break;
		}
	}

	if (damage >= 1)
	{
		// set impact vector to client's velocity vector
		VectorCopy (other->client->ps.velocity, ent->s.angles2);
		ent->health--;
		Use_Breakable(ent, other, other);  // try to break it
	}
}

/*
=========================
	SP_func_breakable
	The basic spawn function for fun_breakables
=========================
*/

void SP_func_breakable (gentity_t *ent)
{
	float	  light;
	vec3_t	  color;
	qboolean  lightSet, colorSet;

	SP_InitMover ( ent );

	ent->takedamage = qtrue;

	trap_SetBrushModel( ent, ent->model );

	// if the "model2" key is set, use a seperate model
	// for drawing, but clip against the brushes
	if (ent->model2)
	{
		ent->s.modelindex2 = G_ModelIndex( ent->model2 );
	}

	// if the "color" or "light" keys are set, setup constantLight
	lightSet = G_SpawnFloat( "light", "100", &light );
	colorSet = G_SpawnVector( "color", "1 1 1", color );

	if (lightSet || colorSet)
	{
		int  r, g, b, i;

		r = color[0] * 255;

		if (r > 255)
		{
			r = 255;
		}
		g = color[1] * 255;

		if (g > 255)
		{
			g = 255;
		}
		b = color[2] * 255;

		if (b > 255)
		{
			b = 255;
		}
		i = light / 4;

		if (i > 255)
		{
			i = 255;
		}
		ent->s.constantLight = r | (g << 8) | (b << 16) | (i << 24);
	}

	ent->use		   = Use_Breakable;
	ent->touch		   = Touch_Breakable;
	ent->nextthink		   = -1;
	ent->mover->moverState = MOVER_POS1;
	ent->r.svFlags		   = SVF_USE_CURRENT_ORIGIN; // Had broadcast as well, not anymore.
	ent->s.eType		   = ET_MOVER;

	// get type of breakable object
	// 0 - window
	// 1 - wood
	// 2 - ceramic
	// 3 - plastic
	// 4 - metal - normal
	// 5 - metal - explosives
	// 6 - metal - hk69
	// 7 - stone - explosives
	// 8 - stone - hk69
	// 9 - wood - explosives
	// 10 - wood - hk69
	G_SpawnInt( "type", "0", &ent->breaktype );

	// so mappers can force a 'thin' axis
	// 1 = x, 2 = y, 4 = z
	G_SpawnInt( "axis", "0", &ent->breakaxis );

	// handle amount of shards (will be passed to the EV_BREAK event)
	G_SpawnInt( "shards", "35", &ent->breakshards );

	if (ent->breakshards < 4)
	{
		ent->breakshards = 4;	// default to 4
	}
	else if (ent->breakshards > 255)
	{
		ent->breakshards = 255;
	}

	G_SpawnInt( "thickness", "2", &ent->breakthickness );

	if (ent->breakthickness > 16)
	{
		ent->breakthickness = 0;				// if too thick, this is a mistake
	}
	// If entity can be bombed.
	G_SpawnInt("bombable", "0", &ent->bombable);

	trap_LinkEntity (ent);

	ent->s.pos.trType = TR_STATIONARY;

	VectorCopy( ent->s.origin, ent->s.pos.trBase );
	VectorCopy( ent->s.origin, ent->r.currentOrigin );

	if (!ent->health)	// set default health to 1 which means always break
	{
		ent->health = 1;
	}

	// cap health of glass at 11... 11 means it won't break if someone stands on it
	// unless it has been damaged.
	if ((ent->breaktype <= 4) && (ent->health > 11))
	{
		if (ent->breaktype != 2)
		{
			ent->health = 5 + (rand() % 6);
		}
		else
		{
			ent->health = 1;
		}
	}
	ent->watertype = ent->health;
}

//////////////////////////////////////////////////////////////
// CalcRDoorDirection										//
// Returns: 1 or -1 for clockwise/counterclockwise movement //
// or 0 if the door is aligned along the z axis (trapdoors) //
// or -1 if the activator was not a client					//
// Used to work out which way a door should open so that it //
// opens away from a player 								//
// Gotchas: won't work on rdoors that are in the ground 	//
//////////////////////////////////////////////////////////////

int CalcRDoorDirection(gentity_t *ent, gentity_t *activator)
{
	float	xd, yd, hinge, handle;
	int quadrant = 0;
	int otheraxis, thinaxis, i, direction;
	vec3_t	mins, maxs;
	int special = 1;	// set to -1 if player is not in front of door when opening it

	// not a player opening (maybe a trigger), so just open any old direction
	if (!activator->client)
	{
		return -1;
	}

	// allow mapper to override the door's opening direction
	if (ent->mover->direction)
	{
		return ent->mover->direction;
	}

	// work out where the player is in relation to the door's hinge
	xd = ent->s.pos.trBase[0] - activator->client->ps.origin[0];
	yd = ent->s.pos.trBase[1] - activator->client->ps.origin[1];

	// quadrant 0 is upper right, and quads progress clockwise from there
	if ((xd >= 0) && (yd >= 0))
	{
		quadrant = 0;
	}
	else if ((xd >= 0) && (yd < 0))
	{
		quadrant = 1;
	}
	else if ((xd < 0) && (yd < 0))
	{
		quadrant = 2;
	}
	else if ((xd < 0) && (yd >= 0))
	{
		quadrant = 3;
	}

	// work out which is the thinnest axis
	VectorCopy (ent->r.absmin, mins);
	VectorCopy (ent->r.absmax, maxs);

	// find the thinnest axis
	thinaxis = 0;

	for (i = 1 ; i < 3 ; i++)
	{
		if (maxs[i] - mins[i] < maxs[thinaxis] - mins[thinaxis])
		{
			thinaxis = i;
		}
	}

	if (thinaxis == 2)
	{
		return 0;		 // if it's aligned z, we don't fix it
	}
	// determine opposite axis
	otheraxis = thinaxis ? 0 : 1;

	/*if (thinaxis) {
		otheraxis = 0;
	} else {
		otheraxis = 1;
	}*/

	// work out where the hinge and handle is to allow us to catch the special case
	// see if the hinge is closer to max or min for the door
	if (fabs(ent->s.pos.trBase[otheraxis] - mins[otheraxis]) > fabs(ent->s.pos.trBase[otheraxis] - maxs[otheraxis]))
	{
		hinge  = maxs[otheraxis]; // hinge is closer to max
		handle = mins[otheraxis];
	}
	else
	{
		hinge  = mins[otheraxis]; // hinge is closer to mins
		handle = maxs[otheraxis];
	}

	// catch the special case: player is opening the door from the side
	// which is nearest the hinge but is not in front of the door
	if ((activator->client->ps.origin[otheraxis] > maxs[otheraxis]) || (activator->client->ps.origin[otheraxis] < mins[otheraxis]))
	{
		// player is not in front of the door so
		// work out if player is closer to the hinge or the handle
		if (fabs(hinge - activator->client->ps.origin[otheraxis]) < fabs(handle - activator->client->ps.origin[otheraxis]))
		{
			// player is close to the hinge, so because they're not in front of the door we
			// have a special case
			special = -1;
		}
	}

	// now we know the position of the player relative to the door
	// and we know the orientation of the door
	// we can work out which way it needs to rotate (clockwise or counter)
	// to open away from the player

	direction = 1;						// assume counter-clockwise rotation

	if (quadrant & 1)
	{
		direction = -1; 		// odd quadrants open clockwise
	}

	if (thinaxis)
	{
		direction *= -1;		// y-axis quadrants open opposite to x-axis
	}
	direction *= special;		// invert with special if the player is hiding

	// if player is holding down backwards key, open door towards player
	if (activator->client->pers.cmd.forwardmove < 0)
	{
		direction *= -1;
	}

	return direction;
}

/* ======== func_wall stuff ========= */
//Gamemode 0123456789
void SP_func_wall( gentity_t *ent )
{
	//If theres no GameMode, we exist.
	//If theres no US in this gamemode, we dont.
	char	  *sound;
	qboolean  found = qtrue;
	int   j;
	char	  *chr;

	if (G_SpawnString( "gametype", "", &sound ))
	{
		found = qfalse;

		for (j = 0; j < strlen(sound); j++)
		{
			if ((g_gametype.integer == GT_JUMP) || (g_gametype.integer == GT_LASTMAN))
			{
				chr = va("%d", GT_FFA);
			}
			else
			{
				chr = va("%d", g_gametype.integer);
			}

			if (sound[j] == chr[0]) //we exist in this mode
			{
				found = qtrue;
			}
		}
	}

	if (found == qtrue)
	{
		SP_InitMover ( ent );
		trap_SetBrushModel( ent, ent->model );
		InitMover( ent );
		VectorCopy( ent->s.origin, ent->s.pos.trBase );
		VectorCopy( ent->s.origin, ent->r.currentOrigin );
	}
}

/* ======== mr_sentry stuff ========= */
void SP_mr_sentry( gentity_t *ent )
{
	char	   *sound;
	gentity_t  *mrsentry;
	int    team;
	qboolean   found = qtrue;
	int    j;
	char	   *chr;
	int    theint;

	team = 1;

	if (G_SpawnString( "gametype", "", &sound ))
	{
		found = qfalse;

		for (j = 0; j < strlen(sound); j++)
		{
			chr = va("%d", g_gametype.integer);

			if (sound[j] == chr[0]) //we exist in this mode
			{
				found = qtrue;
			}
		}
	}

	if (found == qfalse)
	{
		return;
	}

	G_SpawnInt("angle", "0", &theint);

	if (G_SpawnString( "team", "red", &sound ))
	{
		Q_strlwr(sound);

		if ((sound[0] == 'r') || (sound[0] == '1'))
		{
			team = 1;
		}

		if ((sound[0] == 'b') || (sound[0] == '2'))
		{
			team = 2;
		}
	}

	mrsentry = G_Spawn();

	//memset(corpse,0,sizeof(gentity_t));
	if (!mrsentry)
	{
		return;
	}

	mrsentry->classname  = "mrsentry";
	mrsentry->classhash  = HSH_mrsentry;

	mrsentry->nextthink  = level.time + 1000;
	mrsentry->angle 	 = theint;
	mrsentry->timestamp  = level.time;
	mrsentry->think 	 = G_MrSentryThink;

	mrsentry->clipmask	 = 0;
	mrsentry->s.powerups = team;
	mrsentry->r.contents = 0;
	mrsentry->s.eType	 = ET_MR_SENTRY;
	mrsentry->r.mins[0]  = -20;
	mrsentry->r.mins[1]  = -20;
	mrsentry->r.mins[2]  = -20;
	mrsentry->r.maxs[0]  = 20;
	mrsentry->r.maxs[1]  = 20;
	mrsentry->r.maxs[2]  = 20;

	G_SetOrigin(mrsentry, ent->s.pos.trBase );

	VectorCopy(ent->r.currentOrigin, mrsentry->s.pos.trBase);
	VectorCopy(ent->r.currentOrigin, mrsentry->r.currentOrigin);

	mrsentry->s.apos.trType = TR_INTERPOLATE;
	mrsentry->s.pos.trType	= TR_INTERPOLATE;
	mrsentry->s.pos.trTime	= level.time;

	trap_LinkEntity(mrsentry);
}

/**
 * $(function)
 */
void goal_touch( gentity_t *self, gentity_t *other, trace_t *trace )
{
	//gentity_t  *SoundEnt;
	//int    j;
	//int    bit	 = 1 << (int)self->angle;
	//int    goals = 0;

	//@Barbatos
	return;

	// TTimo - silence warning, unreachable code
#if 0
	if (!other->client)
	{
		return;
	}

	if (other->client->goalbits & bit)
	{
		return;
	}

	//we're touching it for the first time
	other->client->goalbits |= bit;

	AddScore(other, other->r.currentOrigin, 1);

	SoundEnt		  = G_TempEntity( self->r.currentOrigin, EV_UT_BOMBBEEP );
	VectorCopy( self->r.currentOrigin, SoundEnt->s.pos.trBase );
	SoundEnt->s.eventParm = 0;

	goals			  = level.goalindex;
	trap_SendServerCommand( -1, va("print \"%s" S_COLOR_WHITE " collected a goal (%s).\n\"", other->client->pers.netname, self->model ));

	for (j = 0; j < 32; j++)
	{
		if (other->client->goalbits & (1 << j))
		{
			goals--;
		}
	}

	if (goals > 1)
	{
		trap_SendServerCommand( other->client->ps.clientNum, va("cp \"(%s) %d Goals To Go!\n\"", self->model, goals));
		return;
	}

	if (goals == 1)
	{
		trap_SendServerCommand( other->client->ps.clientNum, va("cp \"(%s) 1 Goal To Go!\n\"", self->model));
		return;
	}

	if (goals == 0)
	{
		AddTeamScore(other->client->sess.sessionTeam, 1);
		trap_SendServerCommand( -1, va("print \"%s" S_COLOR_WHITE " collected all the goals.\n\"", other->client->pers.netname ));

		trap_SendServerCommand( other->client->ps.clientNum, va("cp \"All Done. Grats!\n\""));

		LogExit("All Goals Hit.");
	}
#endif
}

/* ======== jumpgoal stuff ========= */
void SP_ut_jumpgoal( gentity_t *ent )
{
	//char	   *sound;
	//gentity_t  *goal;
	//int    team = 0;

	//if (g_gametype.integer != GT_JUMP)
	{
		return;
	}

	// TTimo - silence warning, unreachable code
#if 0
	if (level.goalindex == 32)
	{
		return;
	}

	goal = G_Spawn();

	//memset(corpse,0,sizeof(gentity_t));
	if (!goal)
	{
		return;
	}

	goal->classname   = "jumpgoal";
	goal->classhash   = HSH_jumpgoal;

	goal->nextthink   = level.time + 1000;
	goal->angle   = level.goalindex++; ///increment this every time we spawn a goal in
	goal->s.torsoAnim = 1;
	goal->timestamp   = level.time;

	if (G_SpawnString( "name", "", &sound ))
	{
		goal->model = G_NewString(sound);
	}
	else
	{
		goal->model = G_NewString(va("%s", level.goalindex));
	}
	//	 goal->think = G_JumpGoalThink;

	goal->r.contents   = CONTENTS_TRIGGER;
	goal->s.powerups   = team;

	goal->s.eType	   = ET_GENERAL;
	goal->s.modelindex = -1;
	goal->r.mins[0]    = -64;
	goal->r.mins[1]    = -64;
	goal->r.mins[2]    = -64;
	goal->r.maxs[0]    = 64;
	goal->r.maxs[1]    = 64;
	goal->r.maxs[2]    = 64;
	goal->touch    = goal_touch;

	G_SetOrigin(goal, ent->s.pos.trBase );

	VectorCopy(ent->r.currentOrigin, goal->s.pos.trBase);
	VectorCopy(ent->r.currentOrigin, goal->r.currentOrigin);

	goal->s.apos.trType = TR_INTERPOLATE;
	goal->s.pos.trType	= TR_INTERPOLATE;
	goal->s.pos.trTime	= level.time;

	trap_LinkEntity(goal);
#endif
}


/*
* When a jump start line entity gets touched
*
* @author	Barbatos, Fenix
* @date 	12/02/2012
* @param	ent 	  -> the entity to be used (ut_jumpstart)
* @param	other	  -> the entity which activated ut_jumpstart (trigger_multiple)
* @param	activator -> the entity containing the client who activated the trigger_multiple
*/
void Use_JumpStartLine( gentity_t *ent, gentity_t *other, gentity_t *activator ) {

	gentity_t  *tempEntity;
	gclient_t  *client;
	int        seconds, time;

	// If we do not have a valid pointer
	if (!activator->client) {
		return;
	}

	// Copying the client reference
	client = activator->client;

	// If the player is not "ready"
	if (!client->sess.jumpRun) {
		return;
	}

	// Block if the client is not moving
	if ((client->ps.velocity[0] == 0) &&
		(client->ps.velocity[1] == 0) &&
		(client->ps.velocity[2] == 0)) {
		return;
	}

	time = level.time;
	seconds = client->ps.persistant[PERS_JUMPSTARTTIME];
	seconds = (time - seconds) / 1000;

	// We need to check if we touched the start line less than 5 seconds ago.
	// We will allow the touch at the beginning of time and more over
	// we will not block in case we touched a different line
	if ((client->ps.persistant[PERS_JUMPSTARTTIME] > 0) &&
		(client->sess.jumpWay == ent->jumpWayNum) &&
		(seconds < 5)) {
		return;
	}

	// Set the jumpWay and the stop watch
	client->sess.jumpWay = ent->jumpWayNum;
	client->ps.persistant[PERS_JUMPSTARTTIME] = time;
    client->ps.persistant[PERS_JUMPWAYS] = (client->ps.persistant[PERS_JUMPWAYS] & 0xFFFFFF00) | (ent->jumpWayColor + 1); // @r00t:JumpWayColors

	// Create a temporary entity to be sent to the interested client (no SVF_BROADCAST) so he
	// will notice and update his timer client side in order to display a correct value in the HUD.
	tempEntity			         = G_TempEntity(ent->s.pos.trBase, EV_JUMP_START);
	tempEntity->s.otherEntityNum = activator->s.number;
	tempEntity->r.singleClient	 = activator->s.number;
	tempEntity->s.time		     = time;
    tempEntity->s.loopSound      = 1; // generate sound on client
	tempEntity->r.svFlags		|= SVF_SINGLECLIENT;


	// If matchmode is activated and we have a set of attempts specified
	if ((g_matchMode.integer > 0) && (g_jumpruns.integer > 0)) {

		// Log everything for external bots
		G_LogPrintf("ClientJumpRunStarted: %i - way: %i - attempt: %i of %i\n", client->ps.clientNum, client->sess.jumpWay, client->sess.jumpRunCount + 1, g_jumpruns.integer);

		if (!strcmp(ent->model, "unknown")) {
			// We do not have a jumpWay name to display
			trap_SendServerCommand(-1, va("print \"%s" S_COLOR_WHITE " started a jump run (Run: %i of %i)\n\"",
									   client->pers.netname,
									   client->sess.jumpRunCount + 1,
									   g_jumpruns.integer));

		} else {
			// We got a proper jumpWay name
			trap_SendServerCommand(-1, va("print \"%s" S_COLOR_WHITE " started a jump run (Way: ^%i%s" S_COLOR_WHITE ", Run: %i of %i)\n\"",
									   client->pers.netname,
									   ent->jumpWayColor,
									   ent->model,
									   client->sess.jumpRunCount + 1,
									   g_jumpruns.integer));

		}

	} else {

		// Log everything for external bots
		G_LogPrintf("ClientJumpRunStarted: %i - way: %i\n", client->ps.clientNum, client->sess.jumpWay);

		if (!strcmp(ent->model, "unknown")) {
			// We do not have a jumpWay name to display
			trap_SendServerCommand(-1, va("print \"%s" S_COLOR_WHITE " started a jump run\n\"", client->pers.netname));

		} else {
			// We got a proper jumpWay name
			trap_SendServerCommand(-1, va("print \"%s" S_COLOR_WHITE " started a jump run (Way: ^%i%s" S_COLOR_WHITE ")\n\"",
									   client->pers.netname,
									   ent->jumpWayColor,
									   ent->model));

		}

	}


	// Send updated values to all the clients
	SendScoreboardSingleMessageToAllClients(activator);

}


/*
* When a jump stop line entity gets touched
*
* @author	Barbatos, Fenix
* @date 	12/02/2012
* @param	ent 	  -> the entity to be used (ut_jumpstop)
* @param	other	  -> the entity which activated ut_jumpstart (trigger_multiple)
* @param	activator -> the entity containing the client who activated the trigger_multiple
*/
void Use_JumpStopLine( gentity_t *ent, gentity_t *other, gentity_t *activator ) {

	gentity_t  *tempEntity;
	gclient_t  *client;
	char	   *s;
	int        msec, seconds, mins, hours, time, logTimeMillis;
	int        waycolor;

	// If we do not have a valid pointer
	if (!activator->client) {
		return;
	}

	// Copying the client reference
	client = activator->client;

	// If the player is not "ready"
	if (!client->sess.jumpRun) {
		return;
	}

	// If we haven't touched a start line
	if (!client->ps.persistant[PERS_JUMPSTARTTIME]) {
		return;
	}

	// If we are touching the line of another way
	if (client->sess.jumpWay != ent->jumpWayNum) {
		return;
	}

	time = level.time;

	// Create a temporary entity to be sent to the interested client (no SVF_BROADCAST) so he
	// will notice and update his timer client side in order to display a correct value in the HUD.
	tempEntity			         = G_TempEntity(ent->s.pos.trBase, EV_JUMP_STOP);
	tempEntity->s.otherEntityNum = activator->s.number;
	tempEntity->r.singleClient	 = activator->s.number;
	tempEntity->s.time		     = time;
	tempEntity->r.svFlags		|= SVF_SINGLECLIENT;

    waycolor = client->ps.persistant[PERS_JUMPWAYS] & 0xFF; // Save current way color

	// Update the best timer just if we did a better time than our last run
	// or in case we do not have a best time established (it's our 1st run)
	if ((client->ps.persistant[PERS_JUMPBESTTIME] == 0) || ((time - client->ps.persistant[PERS_JUMPSTARTTIME]) < client->ps.persistant[PERS_JUMPBESTTIME])) {
		client->ps.persistant[PERS_JUMPBESTTIME] = time - client->ps.persistant[PERS_JUMPSTARTTIME];
        client->ps.persistant[PERS_JUMPWAYS] = client->ps.persistant[PERS_JUMPWAYS] << 8; // @r00t: Make current best
	} else {
        client->ps.persistant[PERS_JUMPWAYS] &= 0xFFFFFF00; // @r00t:JumpWayColors - Clear active way
	}


	msec = time - client->ps.persistant[PERS_JUMPSTARTTIME];
	logTimeMillis = msec; // Store it in a separate variable for the log message

	seconds = msec / 1000;
	msec -= seconds * 1000;
	mins = seconds / 60;
	seconds -= mins * 60;
	hours = mins / 60;
	mins -= hours * 60;

	// Building the time string
	s = va("%01d:%02d:%02d.%03d", hours, mins, seconds, msec);


	// If matchmode is activated and we have a set of attempts specified
	if ((g_matchMode.integer > 0) && (g_jumpruns.integer > 0)) {

		client->sess.jumpRunCount++;

		// Log everything for external bots
		G_LogPrintf("ClientJumpRunStopped: %i - way: %i - time: %i - attempt: %i of %i\n", client->ps.clientNum, client->sess.jumpWay, logTimeMillis, client->sess.jumpRunCount, g_jumpruns.integer);

		if (!strcmp(ent->model, "unknown")) {
			// We do not have a jumpWay name to display
			trap_SendServerCommand(-1, va("print \"Time performed by %s" S_COLOR_WHITE ": " S_COLOR_GREEN "%s" S_COLOR_WHITE " (Run: %i of %i)\n\"",
									   client->pers.netname,
									   s,
									   client->sess.jumpRunCount,
									   g_jumpruns.integer));

		} else {
			// We got a proper jumpWay name
			trap_SendServerCommand(-1, va("print \"Time performed by %s" S_COLOR_WHITE ": " S_COLOR_GREEN "%s" S_COLOR_WHITE " (Way: ^%i%s" S_COLOR_WHITE ", Run: %i of %i)\n\"",
									   client->pers.netname,
									   s,
									   waycolor - 1,
									   ent->model,
									   client->sess.jumpRunCount,
									   g_jumpruns.integer));

		}

		// Check if it's time to switch off jump timers
		if (client->sess.jumpRunCount >= g_jumpruns.integer)
			client->sess.jumpRun = 0;

	} else {

		// Log everything for external bots
		G_LogPrintf("ClientJumpRunStopped: %i - way: %i - time: %i\n", client->ps.clientNum, client->sess.jumpWay, logTimeMillis);

		if (!strcmp(ent->model, "unknown")) {
			// We do not have a jumpWay name to display
			trap_SendServerCommand(-1, va("print \"Time performed by %s" S_COLOR_WHITE ": " S_COLOR_GREEN "%s" S_COLOR_WHITE "\n\"",
									   client->pers.netname,
									   s));

		} else {
			// We got a proper jumpWay name
			trap_SendServerCommand(-1, va("print \"Time performed by %s" S_COLOR_WHITE ": " S_COLOR_GREEN "%s" S_COLOR_WHITE " (Way: ^%i%s" S_COLOR_WHITE ")\n\"",
									   client->pers.netname,
									   s,
									   waycolor - 1,
									   ent->model));

		}

	}

	// Reset the timer
	client->sess.jumpWay = 0;							  // Clear the jumpWay
	client->ps.persistant[PERS_JUMPSTARTTIME] = 0;		  // Clear the jumpStartTime
	client->ps.persistant[PERS_JUMPWAYS] &= 0xFFFFFF00;   // Clear the jumpWayColor

	// Calculate ranks
	CalculateRanks();

	//Send updated values to all the clients
	SendScoreboardSingleMessageToAllClients(activator);

}


/*
* When a jump cancel entity gets touched
*
* @author	Fenix
* @date 	12/02/2012
* @param	ent 	  -> the entity to be used (ut_jumpcancel)
* @param	other	  -> the entity which activated ut_jumpstart (trigger_multiple)
* @param	activator -> the entity containing the client who activated the trigger_multiple
*/
void Use_JumpTimerCancel( gentity_t *ent, gentity_t *other, gentity_t *activator ) {

	gentity_t  *tempEntity;
	gclient_t  *client;
	int        waycolor;

	// If we do not have a valid pointer
	if (!activator->client) {
		return;
	}

	// Copying the client reference
	client = activator->client;

	// If the player is not "ready"
	if (!client->sess.jumpRun) {
		return;
	}

	// If we haven't touched a start line
	if(!client->ps.persistant[PERS_JUMPSTARTTIME]) {
		return;
	}

	// If we are touching a line of another way or if 0 is specified as entity type
	if ((ent->jumpWayNum > 0) && (client->sess.jumpWay != ent->jumpWayNum)) {
		return;
	}


	// Create a temporary entity to be sent to the interested client (no SVF_BROADCAST) so he
	// will notice and reset his timer client side in order to display a correct value in the HUD.
	tempEntity			         = G_TempEntity(ent->s.pos.trBase, EV_JUMP_CANCEL);
	tempEntity->s.otherEntityNum = activator->s.number;
	tempEntity->r.singleClient	 = activator->s.number;
	tempEntity->s.time		     = level.time;
	tempEntity->r.svFlags		|= SVF_SINGLECLIENT;

	waycolor = client->ps.persistant[PERS_JUMPWAYS] & 0xFF;  // Save the way color

    // If matchmode is activated and we have a set of attempts specified
    if ((g_matchMode.integer > 0) && (g_jumpruns.integer > 0)) {

    	client->sess.jumpRunCount++;

    	// Log everything for external bots
    	G_LogPrintf("ClientJumpRunCanceled: %i - way: %i - attempt: %i of %i\n", client->ps.clientNum, client->sess.jumpWay, client->sess.jumpRunCount, g_jumpruns.integer);

    	if (!strcmp(ent->model, "unknown")) {
			// We do not have a jumpWay name to display
			trap_SendServerCommand(-1, va("print \"%s's run has been " S_COLOR_RED "stopped" S_COLOR_WHITE " (Run: %i of %i)\n\"",
					                   client->pers.netname,
					                   client->sess.jumpRunCount,
					                   g_jumpruns.integer));
		} else {
			// We got a proper jumpWay name
			trap_SendServerCommand(-1, va("print \"%s's run has been " S_COLOR_RED "stopped" S_COLOR_WHITE " (Way: ^%i%s" S_COLOR_WHITE ", Run: %i of %i)\n\"",
									   client->pers.netname,
									   waycolor - 1,
									   ent->model,
									   client->sess.jumpRunCount,
									   g_jumpruns.integer));
		}

    	// Check if it's time to switch off jump timers
		if (client->sess.jumpRunCount >= g_jumpruns.integer)
			client->sess.jumpRun = 0;

    } else {

    	// Log everything for external bots
    	G_LogPrintf("ClientJumpRunCanceled: %i - way: %i\n", client->ps.clientNum, client->sess.jumpWay);

    	if (!strcmp(ent->model, "unknown")) {
			// We do not have a jumpWay name to display
			trap_SendServerCommand(-1, va("print \"%s's run has been " S_COLOR_RED "stopped" S_COLOR_WHITE ".\n\"", client->pers.netname));
		} else {
			// We got a proper jumpWay name
			trap_SendServerCommand(-1, va("print \"%s's run has been " S_COLOR_RED "stopped" S_COLOR_WHITE ": (Way: ^%i%s" S_COLOR_WHITE ")\n\"",
					                   client->pers.netname,
					                   waycolor - 1,
					                   ent->model));
		}

    }

    // Reset the timer
    client->sess.jumpWay = 0;							  // Clear the jumpWay
    client->ps.persistant[PERS_JUMPSTARTTIME] = 0;		  // Clear the jumpStartTime
    client->ps.persistant[PERS_JUMPWAYS] &= 0xFFFFFF00;   // Clear the jumpWayColor

	// Send updated values to all the clients
	SendScoreboardSingleMessageToAllClients(activator);

}
