// Copyright (C) 1999-2000 Id Software, Inc.
//
// bg_slidemove.c -- part of bg_pmove functionality

#include "q_shared.h"
#include "bg_public.h"
#include "bg_local.h"

/*

input: origin, velocity, bounds, groundPlane, trace function

output: origin, velocity, impacts, stairup boolean

*/

/*
==================


Returns qtrue if the velocity was clipped in some way
==================
*/
#define MAX_CLIP_PLANES 5
/**
 * $(function)
 */
qboolean PM_SlideMove( qboolean gravity )
{
	int 	  bumpcount;
	int 	  numbumps;
	vec3_t	  dir;
	float	  d;
	int 	  numplanes;
	vec3_t	  planes[MAX_CLIP_PLANES];
	vec3_t	  primal_velocity;
	vec3_t	  clipVelocity;
	int 	  i;
	int 	  j;
	int 	  k;
	trace_t   trace;
	vec3_t	  end;
	float	  time_left;
	float	  into;
	vec3_t	  endVelocity;
	vec3_t	  endClipVelocity;
	qboolean  sliding = qfalse;

//	 pm->walljump=0;
	endVelocity[0] = 0;
	endVelocity[1] = 0;
	endVelocity[2] = 0;
	numbumps	   = 4;

	VectorCopy (pm->ps->velocity, primal_velocity);

	if (gravity)
	{
		VectorCopy( pm->ps->velocity, endVelocity );

		endVelocity[2]	   -= pm->ps->gravity * pml.frametime;

		pm->ps->velocity[2] = (pm->ps->velocity[2] + endVelocity[2]) * 0.5;
		primal_velocity[2]	= endVelocity[2];

		if (pml.groundPlane)
		{
			// slide along the ground plane
			PM_ClipVelocity (pm->ps->velocity, pml.groundTrace.plane.normal, pm->ps->velocity, OVERCLIP );
		}
	}

	time_left = pml.frametime;

	// never turn against the ground plane
	if (pml.groundPlane)
	{
		numplanes = 1;
		VectorCopy( pml.groundTrace.plane.normal, planes[0] );
	}
	else
	{
		numplanes = 0;
	}

	// never turn against original velocity
	VectorNormalize2NR( pm->ps->velocity, planes[numplanes] );
	numplanes++;

	for (bumpcount = 0 ; bumpcount < numbumps ; bumpcount++)
	{
		// calculate position we are trying to move to
		VectorMA( pm->ps->origin, time_left, pm->ps->velocity, end );

		// see if we can make it there
		pm->trace ( &trace, pm->ps->origin, pm->mins, pm->maxs, end, pm->ps->clientNum, pm->tracemask);

		if (trace.allsolid)
		{
			// entity is completely trapped in another solid
			pm->ps->velocity[2] = 0;	// don't build up falling damage, but allow sideways acceleration
			return qtrue;
		}

		if (trace.fraction > 0)
		{
			// actually covered some distance
			VectorCopy (trace.endpos, pm->ps->origin);
		}

		if (trace.fraction == 1)
		{
			break;		// moved the entire distance
		}

		//check for rubbing on a near vert surface for wall sliding
		if ((trace.plane.normal[2] < 0.3f) && (trace.plane.normal[2] > -0.3f) && gravity)
		{
			if (!(trace.surfaceFlags & SURF_SKY) &&
				!(trace.surfaceFlags & SURF_SLICK) &&
				!(trace.contents & CONTENTS_BODY))
			{
				//	if (pm->ps->grapplePoint[0]==0)
				//	 pm->ps->grapplePoint[0] = pm->cmd.serverTime;
				pm->ps->pm_flags |= PMF_WALL_JUMP;
				sliding 	  = qtrue;

				//Accumulate the direction we'd jump off
				pm->ps->grapplePoint[1] = trace.plane.normal[0];
				pm->ps->grapplePoint[2] = trace.plane.normal[1];
			}
		}

		// save entity for contact
		PM_AddTouchEnt( trace.entityNum );

		time_left -= time_left * trace.fraction;

		if (numplanes >= MAX_CLIP_PLANES)
		{
			// this shouldn't really happen
			VectorClear( pm->ps->velocity );
			return qtrue;
		}

		//
		// if this is the same plane we hit before, nudge velocity
		// out along it, which fixes some epsilon issues with
		// non-axial planes
		//
		for (i = 0 ; i < numplanes ; i++)
		{
			if (DotProduct( trace.plane.normal, planes[i] ) > 0.99)
			{
				VectorAdd( trace.plane.normal, pm->ps->velocity, pm->ps->velocity );
				break;
			}
		}

		if (i < numplanes)
		{
			continue;
		}
		VectorCopy (trace.plane.normal, planes[numplanes]);
		numplanes++;

		//
		// modify velocity so it parallels all of the clip planes
		//

		// find a plane that it enters
		for (i = 0 ; i < numplanes ; i++)
		{
			into = DotProduct( pm->ps->velocity, planes[i] );

			if (into >= 0.1)
			{
				continue;		// move doesn't interact with the plane
			}

			// see how hard we are hitting things
			if (-into > pml.impactSpeed)
			{
				pml.impactSpeed = -into;
			}

			// slide along the plane
			PM_ClipVelocity (pm->ps->velocity, planes[i], clipVelocity, OVERCLIP );

			// slide along the plane
			PM_ClipVelocity (endVelocity, planes[i], endClipVelocity, OVERCLIP );

			// see if there is a second plane that the new move enters
			for (j = 0 ; j < numplanes ; j++)
			{
				if (j == i)
				{
					continue;
				}

				if (DotProduct( clipVelocity, planes[j] ) >= 0.1)
				{
					continue;		// move doesn't interact with the plane
				}

				// try clipping the move to the plane
				PM_ClipVelocity( clipVelocity, planes[j], clipVelocity, OVERCLIP );
				PM_ClipVelocity( endClipVelocity, planes[j], endClipVelocity, OVERCLIP );

				// see if it goes back into the first clip plane
				if (DotProduct( clipVelocity, planes[i] ) >= 0)
				{
					continue;
				}

				// slide the original velocity along the crease
				CrossProduct (planes[i], planes[j], dir);
				VectorNormalizeNR( dir );
				d = DotProduct( dir, pm->ps->velocity );
				VectorScale( dir, d, clipVelocity );

				CrossProduct (planes[i], planes[j], dir);
				VectorNormalizeNR( dir );
				d = DotProduct( dir, endVelocity );
				VectorScale( dir, d, endClipVelocity );

				// see if there is a third plane the the new move enters
				for (k = 0 ; k < numplanes ; k++)
				{
					if ((k == i) || (k == j))
					{
						continue;
					}

					if (DotProduct( clipVelocity, planes[k] ) >= 0.1)
					{
						continue;		// move doesn't interact with the plane
					}

					// stop dead at a tripple plane interaction
					VectorClear( pm->ps->velocity );
					return qtrue;
				}
			}

			// if we have fixed all interactions, try another move
			VectorCopy( clipVelocity, pm->ps->velocity );
			VectorCopy( endClipVelocity, endVelocity );
			break;
		}
	}

	if (gravity)
	{
		VectorCopy( endVelocity, pm->ps->velocity );
	}

	// don't change velocity if in a timer (FIXME: is this correct?)
	if (pm->ps->pm_time)
	{
		VectorCopy( primal_velocity, pm->ps->velocity );
	}

	if (!sliding || (pm->ps->groundEntityNum != ENTITYNUM_NONE))
	{
		//pm->walljump=0;
		//pm->ps->grapplePoint[0]=0;
		pm->ps->pm_flags &= ~PMF_WALL_JUMP;
	}

	return bumpcount != 0 ;
}

/**
 * $(function)
 */
qboolean PM_StepUp ( qboolean gravity, vec3_t start_o, vec3_t start_v, int height )
{
	trace_t   trace;
	vec3_t	  down;
	qboolean  result = qtrue;

	// First lets see if there is any hope of steping up
	pm->maxs[2] -= STEPSIZE_LARGE;

	VectorCopy ( start_o, pm->ps->origin );
	VectorCopy ( start_v, pm->ps->velocity );

	pm->ps->origin[2] += STEPSIZE_LARGE;

	if (PM_SlideMove( gravity ))
	{
		result		  = qfalse;
		pm->ps->pm_flags |= PMF_WALL;
	}

	// See how far down now
	VectorCopy ( pm->ps->origin, down );
	down[2] -= STEPSIZE_LARGE;

	// Trace it
	pm->trace (&trace, pm->ps->origin, pm->mins, pm->maxs, down, pm->ps->clientNum, pm->tracemask);

	if (trace.allsolid || trace.startsolid)
	{
		pm->maxs[2] += STEPSIZE_LARGE;
		return qfalse;
	}

	if (trace.fraction < 1.0)
	{
		PM_ClipVelocity( pm->ps->velocity, trace.plane.normal, pm->ps->velocity, OVERCLIP );
	}

	pm->maxs[2] += STEPSIZE_LARGE;

	// Now double check not stuck
	VectorCopy ( trace.endpos, pm->ps->origin );
	pm->trace (&trace, pm->ps->origin, pm->mins, pm->maxs, pm->ps->origin, pm->ps->clientNum, pm->tracemask);

	if (trace.allsolid || trace.startsolid)
	{
		return qfalse;
	}

/*
	if ( trace.fraction < 1.0 ) {
		PM_ClipVelocity( pm->ps->velocity, trace.plane.normal, pm->ps->velocity, OVERCLIP );
	}

	pm->maxs[2] += STEPSIZE_LARGE;

	pm->trace (&trace, pm->ps->origin, pm->mins, pm->maxs, pm->ps->origin, pm->ps->clientNum, pm->tracemask);

	if ( trace.fraction < 1.0 ) {
		PM_ClipVelocity( pm->ps->velocity, trace.plane.normal, pm->ps->velocity, OVERCLIP );
	}

	if ( trace.allsolid )
	{
		pm->ps->pm_flags |= PMF_WALL;
		VectorCopy ( start_o, pm->ps->origin );
	}
*/

/*

	VectorCopy(start_o, down);
	down[2] -= stepHeight;
	pm->trace (&trace, start_o, pm->mins, pm->maxs, down, pm->ps->clientNum, pm->tracemask);
	VectorSet(up, 0, 0, 1);
	// never step up when you still have up velocity
	if ( pm->ps->velocity[2] > 0 && (trace.fraction == 1.0 ||
										DotProduct(trace.plane.normal, up) < 0.7)) {
		pm->ps->pm_flags |= PMF_WALL;
		return qfalse;
	}

	VectorCopy (pm->ps->origin, down_o);
	VectorCopy (pm->ps->velocity, down_v);

	VectorCopy (start_o, up);
	up[2] += stepHeight;

	// test the player position if they were a stepheight higher
	pm->trace (&trace, up, pm->mins, pm->maxs, up, pm->ps->clientNum, pm->tracemask);
	if ( trace.allsolid ) {
		if ( pm->debugLevel ) {
			Com_Printf("%i:bend can't step\n", c_pmove);
		}
		pm->ps->pm_flags |= PMF_WALL;
		return qfalse;
	}

	// try slidemove from this position
	VectorCopy (up, pm->ps->origin);
	VectorCopy (start_v, pm->ps->velocity);

	if ( PM_SlideMove( gravity ))
	{
		result = qfalse;
		pm->ps->pm_flags |= PMF_WALL;
	}

	// push down the final amount
	VectorCopy (pm->ps->origin, down);
	down[2] -= stepHeight;
	pm->trace (&trace, pm->ps->origin, pm->mins, pm->maxs, down, pm->ps->clientNum, pm->tracemask);
	if ( !trace.allsolid ) {
		VectorCopy (trace.endpos, pm->ps->origin);
	}
	else
	{
		pm->ps->pm_flags |= PMF_WALL;
		return qfalse;
	}

	if ( trace.fraction < 1.0 ) {
		PM_ClipVelocity( pm->ps->velocity, trace.plane.normal, pm->ps->velocity, OVERCLIP );
	}
*/
	return qtrue;
}

/*
==================
PM_StepSlideMove

==================
*/
qboolean PM_StepSlideMove( qboolean gravity )
{
	vec3_t	start_o;
	vec3_t	start_v;

	VectorCopy (pm->ps->origin, start_o);
	VectorCopy (pm->ps->velocity, start_v);

	if (PM_SlideMove( gravity ) == 0)
	{
		return qtrue;		// we got exactly where we wanted to go first try
	}

	{
		vec3_t	save_o;
		vec3_t	save_v;

		VectorCopy ( pm->ps->origin, save_o );
		VectorCopy ( pm->ps->velocity, save_v );

		if (!PM_StepUp ( gravity, start_o, start_v, STEPSIZE_LARGE	))
		{
			VectorCopy ( save_o, pm->ps->origin );
			VectorCopy ( save_v, pm->ps->velocity );
			pm->ps->pm_flags |= PMF_WALL;
		}
	}

/*
	if ( !PM_StepUp ( gravity, start_o, start_v, STEPSIZE_LARGE  ) )
		if ( !PM_StepUp ( gravity, start_o, start_v, STEPSIZE_SMALL ) )
			if ( !PM_StepUp ( gravity, start_o, start_v, STEPSIZE_LARGE / 2 ) )
				return qfalse;
*/

// the EV_STEP events are meaningless on the server
// but we need to preserve them for predicting clients, or walking up stairs will be VERY ugly
	if ((pm->ps->origin[2] - start_o[2]) > 2)
	{
		pm->evstepHack = pm->ps->origin[2] - start_o[2];
	}
	return qtrue;
	// !CPMA
}
/*
#if 0
	// if the down trace can trace back to the original position directly, don't step
	pm->trace( &trace, pm->ps->origin, pm->mins, pm->maxs, start_o, pm->ps->clientNum, pm->tracemask);
	if ( trace.fraction == 1.0 ) {
		// use the original move
		VectorCopy (down_o, pm->ps->origin);
		VectorCopy (down_v, pm->ps->velocity);
		if ( pm->debugLevel ) {
			Com_Printf("%i:bend\n", c_pmove);
		}
	} else
#endif
	{
		// use the step move
		float	delta;

		delta = pm->ps->origin[2] - start_o[2];
		if ( delta > 2 ) {
			if ( delta < 7 ) {
				PM_AddEvent( EV_STEP_4 );
			} else if ( delta < 11 ) {
				PM_AddEvent( EV_STEP_8 );
			} else if ( delta < 15 ) {
				PM_AddEvent( EV_STEP_12 );
			} else {
				PM_AddEvent( EV_STEP_16 );
			}
		}
		if ( pm->debugLevel ) {
			Com_Printf("%i:stepped\n", c_pmove);
		}
	}

	return qtrue;
}
*/
