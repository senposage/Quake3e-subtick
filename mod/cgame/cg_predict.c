// Copyright (C) 1999-2000 Id Software, Inc.
//
// cg_predict.c -- this file generates cg.predictedPlayerState by either
// interpolating between snapshots from the server or locally predicting
// ahead the client's movement.
// It also handles local physics interaction, like fragments bouncing off walls

#include "cg_local.h"

static pmove_t	  cg_pmove;

static int	  cg_numSolidEntities; //@r00t: Hacks hook this, let's xor it
//#define NSE_XOR 0xDE
static centity_t  *cg_solidEntities[MAX_ENTITIES_IN_SNAPSHOT];
//#define SE_XOR 0xAD  // assumes MAX_ENTITIES_IN_SNAPSHOT==256
static int	  cg_numTriggerEntities;
static centity_t  *cg_triggerEntities[MAX_ENTITIES_IN_SNAPSHOT];

// use different values every time QVM is run
#define NSE_XOR predict_xor1
#define SE_XOR  predict_xor2

/*
====================
CG_BuildSolidList

When a new cg.snap has been set, this function builds a sublist
of the entities that are actually solid, to make for more
efficient collision detection
====================
*/
void CG_BuildSolidList( void )
{
	int	       i;
	centity_t      *cent;
	snapshot_t     *snap;
	entityState_t  *ent;

	cg_numSolidEntities   = 0 ^ NSE_XOR;
	cg_numTriggerEntities = 0;

	if (cg.nextSnap && !cg.nextFrameTeleport && !cg.thisFrameTeleport)
	{
		snap = cg.nextSnap;
	}
	else
	{
		snap = cg.snap;
	}

	for (i = 0 ; i < snap->numEntities ; i++)
	{
		cent = CG_ENTITIES(snap->entities[i].number);
		ent  = &cent->currentState;

		if ((ent->eType == ET_ITEM) || (ent->eType == ET_PUSH_TRIGGER) || (ent->eType == ET_TELEPORT_TRIGGER))
		{
			cg_triggerEntities[cg_numTriggerEntities] = cent;
			cg_numTriggerEntities++;
			continue;
		}

		if (cent->nextState.solid)
		{
			if (cent->currentValid)
			{
				int nse = cg_numSolidEntities^(NSE_XOR^SE_XOR);
				cg_solidEntities[nse] = cent;

				cg_numSolidEntities=((nse^SE_XOR)+1)^NSE_XOR;
			}
			continue;
		}
	}
}

// Builds a symetrical AABB of a non-symetrical transformed BB
static void EvalMinsMaxs(vec3_t centervec, vec3_t axis[3], vec3_t bmins, vec3_t bmaxs, vec3_t neworg)
{
	vec3_t	top1, top2, top3, top4;
	vec3_t	bop1, bop2, bop3, bop4;
	int	j;

	top1[0] = bmins[0];
	top1[1] = bmins[1];
	top1[2] = bmins[2];

	top2[0] = bmaxs[0];
	top2[1] = bmins[1];
	top2[2] = bmins[2];

	top3[0] = bmaxs[0];
	top3[1] = bmaxs[1];
	top3[2] = bmins[2];

	top4[0] = bmins[0];
	top4[1] = bmaxs[1];
	top4[2] = bmins[2];

	ApplyRotationToPoint(axis, top1);
	ApplyRotationToPoint(axis, top2);
	ApplyRotationToPoint(axis, top3);
	ApplyRotationToPoint(axis, top4);

/*   top1[0]+=centervec[0];
   top1[1]+=centervec[1];
   top1[2]+=centervec[2];

   top2[0]+=centervec[0];
   top2[1]+=centervec[1];
   top2[2]+=centervec[2];

   top3[0]+=centervec[0];
   top3[1]+=centervec[1];
   top3[2]+=centervec[2];

   top4[0]+=centervec[0];
   top4[1]+=centervec[1];
   top4[2]+=centervec[2];  */

	bop1[0] = bmins[0];
	bop1[1] = bmins[1];
	bop1[2] = bmaxs[2];

	bop2[0] = bmaxs[0];
	bop2[1] = bmins[1];
	bop2[2] = bmaxs[2];

	bop3[0] = bmaxs[0];
	bop3[1] = bmaxs[1];
	bop3[2] = bmaxs[2];

	bop4[0] = bmins[0];
	bop4[1] = bmaxs[1];
	bop4[2] = bmaxs[2];

	ApplyRotationToPoint(axis, bop1);
	ApplyRotationToPoint(axis, bop2);
	ApplyRotationToPoint(axis, bop3);
	ApplyRotationToPoint(axis, bop4);

/*   bop1[0]+=centervec[0];
   bop1[1]+=centervec[1];
   bop1[2]+=centervec[2];

   bop2[0]+=centervec[0];
   bop2[1]+=centervec[1];
   bop2[2]+=centervec[2];

   bop3[0]+=centervec[0];
   bop3[1]+=centervec[1];
   bop3[2]+=centervec[2];

   bop4[0]+=centervec[0];
   bop4[1]+=centervec[1];
   bop4[2]+=centervec[2];  */

	bmins[0] = bmins[1] = bmins[2] = 32000;
	bmaxs[0] = bmaxs[1] = bmaxs[2] = -32000;

	for (j = 0; j < 3; j++)
	{
		if (bop1[j] < bmins[j])
		{
			bmins[j] = bop1[j];
		}

		if (bop1[j] > bmaxs[j])
		{
			bmaxs[j] = bop1[j];
		}

		if (bop2[j] < bmins[j])
		{
			bmins[j] = bop2[j];
		}

		if (bop2[j] > bmaxs[j])
		{
			bmaxs[j] = bop2[j];
		}

		if (bop3[j] < bmins[j])
		{
			bmins[j] = bop3[j];
		}

		if (bop3[j] > bmaxs[j])
		{
			bmaxs[j] = bop3[j];
		}

		if (bop4[j] < bmins[j])
		{
			bmins[j] = bop4[j];
		}

		if (bop4[j] > bmaxs[j])
		{
			bmaxs[j] = bop4[j];
		}
	}

	for (j = 0; j < 3; j++)
	{
		if (top1[j] < bmins[j])
		{
			bmins[j] = top1[j];
		}

		if (top1[j] > bmaxs[j])
		{
			bmaxs[j] = top1[j];
		}

		if (top2[j] < bmins[j])
		{
			bmins[j] = top2[j];
		}

		if (top2[j] > bmaxs[j])
		{
			bmaxs[j] = top2[j];
		}

		if (top3[j] < bmins[j])
		{
			bmins[j] = top3[j];
		}

		if (top3[j] > bmaxs[j])
		{
			bmaxs[j] = top3[j];
		}

		if (top4[j] < bmins[j])
		{
			bmins[j] = top4[j];
		}

		if (top4[j] > bmaxs[j])
		{
			bmaxs[j] = top4[j];
		}
	}

	neworg[0]  = (bmins[0] + bmaxs[0]) * 0.5;
	neworg[1]  = (bmins[1] + bmaxs[1]) * 0.5;
	neworg[2]  = (bmins[2] + bmaxs[2]) * 0.5;

	bmaxs[0]  -= neworg[0];
	bmaxs[1]  -= neworg[1];
	bmaxs[2]  -= neworg[2];

	neworg[0] += centervec[0];
	neworg[1] += centervec[1];
	neworg[2] += centervec[2];
}

/**
 * $(function)
 */
void CG_BuildBoundsOptimizer(void )
{
	vec3_t	       mins ;
	centity_t      *cent;
	entityState_t  *ent;
	int	       i;
	vec3_t	       axis[3];
	vec3_t	       origin;
	int	          nse = cg_numSolidEntities^NSE_XOR;

	for (i = 0 ; i < nse ; i++)
	{
		cent = cg_solidEntities[i^SE_XOR];
		ent  = &cent->currentState;

		if (ent->solid == SOLID_BMODEL)
		{
			// Precalc AABB Bounds.
			BG_EvaluateTrajectory( &cent->currentState.pos, cg.physicsTime, origin );

			AnglesToAxis(cent->lerpAngles, axis);
			trap_R_ModelBounds( cgs.inlineDrawModel[ent->modelindex], mins, cent->currentState.angles2);
			// origin2 = new center of the symetrical BB
			// angles2 = new max extents of the symetrical BB
			EvalMinsMaxs(origin, axis, mins, cent->currentState.angles2, cent->currentState.origin2);
			mins[0] = -cent->currentState.angles2[0];
			mins[1] = -cent->currentState.angles2[1];
			mins[2] = -cent->currentState.angles2[2];

//	     if (cg_boundsoptimize.integer==3) CG_DrawBB(cent->currentState.origin2,mins,cent->currentState.angles2,3,0,255,0);
		}
	}
}

//ONLY works for symetrical AABB checks.
//The bounds returned for both players and BMODELS are not symetrical so you'll need to fix that
static int OverlapBounds(const vec3_t orga, const vec3_t max_a, const vec3_t orgb, const vec3_t max_b )
{
	vec3_t	t;

	t[0] = orgb[0] - orga[0];
	t[1] = orgb[1] - orga[1];
	t[2] = orgb[2] - orga[2];

	return fabs(t[0]) <= (max_a[0] + max_b[0]) &&
	       fabs(t[1]) <= (max_a[1] + max_b[1]) &&
	       fabs(t[2]) <= (max_a[2] + max_b[2]);
}
/*
====================
CG_ClipMoveToEntities

====================
*/
static void CG_ClipMoveToEntities ( const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end,
				    int skipNumber, int mask, trace_t *tr )
{
	int	       i, x, zd, zu;
	trace_t        trace;
	entityState_t  *ent;
	clipHandle_t   cmodel;
	vec3_t	       bmins, bmaxs;
	vec3_t	       tmins, tmaxs;
	vec3_t	       origin, angles;
	centity_t      *cent;
	vec3_t	       torg = { 0, 0, 0};

	int	       j;
	qboolean       dooptimize;
	int	       nse = cg_numSolidEntities^NSE_XOR;

	//build the AABB of this trace.
	tmins[0] = tmins[1] = tmins[2] = 32700;
	tmaxs[0] = tmaxs[1] = tmaxs[2] = -32700;

	if (!mins || !maxs)
	{
		//build AABB of the ray
		//  for (j=0;j<3;j++)
		//{
		//  if (start[j]<tmins[j]) tmins[j]=start[j];
		//  if (start[j]>tmaxs[j]) tmaxs[j]=start[j];
		//   if (end[j]<tmins[j]) tmins[j]=end[j];
		//   if (end[j]>tmaxs[j]) tmaxs[j]=end[j];
		//}
		//  CG_DrawBB(torg,tmins,tmaxs,3);
		dooptimize = qfalse;
	}
	else
	{
		//build AABB of the swept box
		for (j = 0; j < 3; j++)
		{
			if (start[j] + mins[j] < tmins[j])
			{
				tmins[j] = start[j] + (mins[j]);
			}

			if (start[j] + maxs[j] > tmaxs[j])
			{
				tmaxs[j] = start[j] + (maxs[j]);
			}

			if (end[j] + mins[j] < tmins[j])
			{
				tmins[j] = end[j] + (mins[j]);
			}

			if (end[j] + maxs[j] > tmaxs[j])
			{
				tmaxs[j] = end[j] + (maxs[j]);
			}
		}

		tmins[0] -= 32;
		tmins[1] -= 32;
		tmins[2] -= 32;
		tmaxs[0] += 32;
		tmaxs[1] += 32;
		tmaxs[2] += 32;

		torg[0]   = (tmins[0] + tmaxs[0]) * 0.5;
		torg[1]   = (tmins[1] + tmaxs[1]) * 0.5;
		torg[2]   = (tmins[2] + tmaxs[2]) * 0.5;

		tmins[0] -= torg[0];
		tmins[1] -= torg[1];
		tmins[2] -= torg[2];
		tmaxs[0] -= torg[0];
		tmaxs[1] -= torg[1];
		tmaxs[2] -= torg[2];

//	if (cg_boundsoptimize.integer>1) CG_DrawBB(torg,tmins,tmaxs,1,255,0,0);
		dooptimize = qtrue;
	}

//   if (cg_boundsoptimize.integer==0) dooptimize=qfalse;

	// CG_Printf("NSE: %d\n",nse);
	for (i = 0 ; i < nse ; i++)
	{
		cent = cg_solidEntities[i^SE_XOR];
		ent  = &cent->currentState;

		if (ent->number == skipNumber)
		{
			continue;
		}

		if (ent->solid == SOLID_BMODEL)
		{
			if (dooptimize == qtrue)
			{
				if (OverlapBounds(cent->currentState.origin2, cent->currentState.angles2, torg, tmaxs))
				{
/*		 if (cg_boundsoptimize.integer>1)
	       {
		  org[0]=-cent->currentState.angles2[0];
		  org[1]=-cent->currentState.angles2[1];
		  org[2]=-cent->currentState.angles2[2];

		  CG_DrawBB(cent->currentState.origin2,org,cent->currentState.angles2,2,0,0,255);
	       } */
				}
				else
				{
					continue;
				}
			}

			// special value for bmodel
			cmodel = trap_CM_InlineModel( ent->modelindex );
			VectorCopy( cent->lerpAngles, angles );
			BG_EvaluateTrajectory( &cent->currentState.pos, cg.physicsTime, origin );
		}
		else
		{
			// encoded bbox
			x	 = (ent->solid & 255);
			zd	 = ((ent->solid >> 8) & 255);
			zu	 = ((ent->solid >> 16) & 255) - 32;

			bmins[0] = bmins[1] = -x;
			bmaxs[0] = bmaxs[1] = x;
			bmins[2] = -zd;
			bmaxs[2] = zu;

			cmodel	 = trap_CM_TempBoxModel( bmins, bmaxs );
			//BG_EvaluateTrajectory( &cent->currentState.pos, cg.physicsTime, origin );
			//BG_EvaluateTrajectory( &cent->currentState.apos, cg.physicsTime, angles );
			/// VectorCopy( cent->lerpAngles,angles);
			//VectorCopy( vec3_origin, angles );
			VectorCopy( vec3_origin, angles );
			VectorCopy( cent->lerpOrigin, origin );
			//CG_DrawBB(origin,bmins,bmaxs,1,255,0,255);
		}

		trap_CM_TransformedBoxTrace ( &trace, start, end,
					      mins, maxs, cmodel, mask, origin, angles);

		if (trace.allsolid || (trace.fraction < tr->fraction))
		{
			trace.entityNum = ent->number;
			*tr		= trace;
		}
		else
		{
			if (trace.startsolid)
			{
				tr->startsolid = qtrue;
			}
		}

		if (tr->allsolid)
		{
			return;
		}
	}
}

/*
================
CG_Trace
================
*/
int  cg_traceCount = 0;
/**
 * $(function)
 */
void	CG_Trace( trace_t *result, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end,
		  int skipNumber, int mask )
{
	trace_t  t;

	cg_traceCount++;

	trap_CM_BoxTrace ( &t, start, end, mins, maxs, 0, mask);

	t.entityNum = t.fraction != 1.0 ? ENTITYNUM_WORLD : ENTITYNUM_NONE;
	// check all other solid models
	CG_ClipMoveToEntities (start, mins, maxs, end, skipNumber, mask, &t);

	*result = t;
}

/*
================
CG_PointContents
================
*/
int		CG_PointContents( const vec3_t point, int passEntityNum )
{
	int	       i;
	entityState_t  *ent;
	centity_t      *cent;
	clipHandle_t   cmodel;
	int	       contents;

	int	       nse = cg_numSolidEntities^NSE_XOR;
	contents = trap_CM_PointContents (point, 0);

	for (i = 0 ; i < nse ; i++)
	{
		cent = cg_solidEntities[i^SE_XOR];

		if (!cent)
		{
			continue;
		}

		ent = &cent->currentState;

		if (ent->number == passEntityNum)
		{
			continue;
		}

		if (ent->solid != SOLID_BMODEL)   // special value for bmodel
		{
			continue;
		}

		cmodel = trap_CM_InlineModel( ent->modelindex );

		if (!cmodel)
		{
			continue;
		}

		contents |= trap_CM_TransformedPointContents( point, cmodel, ent->origin, ent->angles );
	}

	return contents;
}

/*
========================
CG_InterpolatePlayerState

Generates cg.predictedPlayerState by interpolating between
cg.snap->player_state and cg.nextFrame->player_state
========================
*/
static void CG_InterpolatePlayerState( qboolean grabAngles )
{
	float	       f;
	int	       i;
	playerState_t  *out;
	snapshot_t     *prev, *next;

	out  = &cg.predictedPlayerState;
	prev = cg.snap;
	next = cg.nextSnap;

	*out = cg.snap->ps;

	// if we are still allowing local input, short circuit the view angles
	if (grabAngles)
	{
		usercmd_t  cmd;
		int	   cmdNum;

		cmdNum = trap_GetCurrentCmdNumber();
		trap_GetUserCmd( cmdNum, &cmd );

		PM_UpdateViewAngles( out, &cmd );
	}

	// if the next frame is a teleport, we can't lerp to it
	if (cg.nextFrameTeleport)
	{
		return;
	}

	if (!next || (next->serverTime <= prev->serverTime))
	{
		return;
	}

	f = (float)( cg.time - prev->serverTime ) / (next->serverTime - prev->serverTime);

	i = next->ps.bobCycle;

	if (i < prev->ps.bobCycle)
	{
		i += 256;		// handle wraparound
	}
	out->bobCycle = prev->ps.bobCycle + f * (i - prev->ps.bobCycle);

	for (i = 0 ; i < 3 ; i++)
	{
		out->origin[i] = prev->ps.origin[i] + f * (next->ps.origin[i] - prev->ps.origin[i]);

		if (!grabAngles)
		{
			out->viewangles[i] = LerpAngle(
				prev->ps.viewangles[i], next->ps.viewangles[i], f );
		}
		out->velocity[i] = prev->ps.velocity[i] +
				   f * (next->ps.velocity[i] - prev->ps.velocity[i]);
	}
}


#if 0
/*
===================
CG_TouchItem
===================
*/
static void CG_TouchItem( centity_t *cent )
{
	gitem_t  *item;

	if (!BG_PlayerTouchesItem( &cg.predictedPlayerState, &cent->currentState, cg.time ))
	{
		return;
	}

	// never pick an item up twice in a prediction
	if (cent->miscTime == cg.time)
	{
		return;
	}

	if (!BG_CanItemBeGrabbed( cgs.gametype, &cent->currentState, &cg.predictedPlayerState ))
	{
		return; 	// can't hold it
	}

	item = &bg_itemlist[cent->currentState.modelindex];

	// Special case for flags.
	// We don't predict touching our own flag
	if(cgs.gametype == GT_CTF)
	{
		if ((cg.predictedPlayerState.persistant[PERS_TEAM] == TEAM_RED) &&
		    (item->giTag == PW_REDFLAG))
		{
			return;
		}

		if ((cg.predictedPlayerState.persistant[PERS_TEAM] == TEAM_BLUE) &&
		    (item->giTag == PW_BLUEFLAG))
		{
			return;
		}
	}

	// Special For bomb mode
	if(cgs.gametype == GT_UT_BOMB)
	{
		if((cg.predictedPlayerState.persistant[PERS_TEAM] != TEAM_RED) && (item->giTag == UT_WP_BOMB))
		{
			return;
		}
	}
	// grab it
	BG_AddPredictableEventToPlayerstate( EV_ITEM_PICKUP, cent->currentState.modelindex, &cg.predictedPlayerState);

	// remove it from the frame so it won't be drawn
	cent->currentState.eFlags |= EF_NODRAW;

	// don't touch it again this prediction
	cent->miscTime = cg.time;

/* URBAN TERROR
	// if its a weapon, give them some predicted ammo so the autoswitch will work
	if ( item->giType == IT_WEAPON ) {
		cg.predictedPlayerState.stats[ STAT_WEAPONS ] |= 1 << item->giTag;
		if ( !cg.predictedPlayerState.ammo[ item->giTag ] ) {
			cg.predictedPlayerState.ammo[ item->giTag ] = 1;
		}
	}
*/
}


/*
=========================
CG_TouchTriggerPrediction

Predict push triggers and items
=========================
*/
static void CG_TouchTriggerPrediction( void )
{
	int	       i;
	trace_t        trace;
	entityState_t  *ent;
	clipHandle_t   cmodel;
	centity_t      *cent;
	qboolean       spectator;

	// dead clients don't activate triggers
	if (cg.predictedPlayerState.stats[STAT_HEALTH] <= 0)
	{
		return;
	}

	spectator = (cg.predictedPlayerState.pm_type == PM_SPECTATOR || (cg.predictedPlayerState.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_GHOST));

	if ((cg.predictedPlayerState.pm_type != PM_NORMAL) && !spectator)
	{
		return;
	}

	for (i = 0 ; i < cg_numTriggerEntities ; i++)
	{
		cent = cg_triggerEntities[i];
		ent  = &cent->currentState;

		if ((ent->eType == ET_ITEM) && !spectator)
		{
			CG_TouchItem( cent );
			continue;
		}

		if (ent->solid != SOLID_BMODEL)
		{
			continue;
		}

		cmodel = trap_CM_InlineModel( ent->modelindex );

		if (!cmodel)
		{
			continue;
		}

		trap_CM_BoxTrace( &trace, cg.predictedPlayerState.origin, cg.predictedPlayerState.origin,
				  cg_pmove.mins, cg_pmove.maxs, cmodel, -1 );

		if (!trace.startsolid)
		{
			continue;
		}

		if (ent->eType == ET_TELEPORT_TRIGGER)
		{
			cg.hyperspace = qtrue;
		}
	}

	// if we didn't touch a jump pad this pmove frame
	if (cg.predictedPlayerState.jumppad_frame != cg.predictedPlayerState.pmove_framecount)
	{
		cg.predictedPlayerState.jumppad_frame = 0;
		cg.predictedPlayerState.jumppad_ent   = 0;
	}
}
#endif


//arq0n

void CG_StepAdjust(void)
{
	if (!cg_pmove.evstepHack)
	{
		return;
	}

	BG_AddPredictableEventToPlayerstate( EV_STEP, cg_pmove.evstepHack, cg_pmove.ps );
	cg_pmove.evstepHack = 0;
}

/*
=================
CG_PredictPlayerState

Generates cg.predictedPlayerState for the current cg.time
cg.predictedPlayerState is guaranteed to be valid after exiting.

For demo playback, this will be an interpolation between two valid
playerState_t.

For normal gameplay, it will be the result of predicted usercmd_t on
top of the most recent playerState_t received from the server.

Each new snapshot will usually have one or more new usercmd over the last,
but we simulate all unacknowledged commands each time, not just the new ones.
This means that on an internet connection, quite a few pmoves may be issued
each frame.

OPTIMIZE: don't re-simulate unless the newly arrived snapshot playerState_t
differs from the predicted one.  Would require saving all intermediate
playerState_t during prediction.

We detect prediction errors and allow them to be decayed off over several frames
to ease the jerk.
=================
*/

#if 1
void CG_PredictPlayerState(void)
{
	playerState_t  oldPlayerState;
	int	       newestCommandNum;
	int	       oldestCommandNum;
	int	       commandNum;
	usercmd_t      newestCommand;
	usercmd_t      oldestCommand;
	vec3_t	       delta, adjusted, mt;
	vec3_t	       backupOrigin, backupAngles;
	float	       length, f;
	int	       t;
	int	       ppscount;

	// If sprint key combo is pressed.
	if (trap_Key_IsDown(trap_Key_GetKey("+moveleft")) && trap_Key_IsDown(trap_Key_GetKey("+moveright")))
	{
		// If we're not sprinting.
		if (!cg.sprintKey)
		{
			trap_SendConsoleCommand("+button8; ");
		}

		// Set sprint flag.
		cg.sprintKey = qtrue;
	}
	else
	{
		// If we're sprinting.
		if (cg.sprintKey)
		{
			trap_SendConsoleCommand("-button8; ");
		}

		// Clear sprint flag.
		cg.sprintKey = qfalse;
	}

	// Reset ET_TELEPORT_TRIGGER flag.
	cg.hyperspace = qfalse;

	// If this is the first frame.
	if (!cg.validPPS)
	{
		// Reset predicted playerstate.
		cg.predictedPlayerState = cg.snap->ps;
	}

	// If this is a demo playback or we're following someone.
	if (cg.demoPlayback || (cg.snap->ps.pm_flags & PMF_FOLLOW))
	{
		// Interpolate.
		CG_InterpolatePlayerState(qfalse);

		return;
	}

	// If we shouln't predict playerstate.
/*	if (cg_nopredict.integer || cg_synchronousClients.integer  )
	{
		// Interpolate with view angles.
		CG_InterpolatePlayerState(qtrue);

		return;
	}*/

	// Configure pmove parameters.
	cg_pmove.ps	           = &cg.predictedPlayerState;
	cg_pmove.trace	       = CG_Trace;
	cg_pmove.pointcontents = CG_PointContents;
	cg_pmove.warmup        = cg.warmup == 0 ? 0 : 1;
	cg_pmove.noFiring      = 0; //cg_noweaponpredict.integer ? qtrue : qfalse;
	cg_pmove.physics       = cg_physics.integer ? 1 : 0;
	cg_pmove.ariesModel    = cgs.clientinfo[cg.predictedPlayerState.clientNum].gender == GENDER_FEMALE;

	if ((cgs.gametype >= GT_TEAM) && (cgs.gametype != GT_JUMP) && !cgs.survivor && !cgs.followenemy && (cg.predictedPlayerState.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_GHOST) && ((cgs.clientinfo[cg.predictedPlayerState.clientNum].team == TEAM_RED) || (cgs.clientinfo[cg.predictedPlayerState.clientNum].team == TEAM_BLUE)))
	{
		cg_pmove.freeze = qtrue;
	}
	else
	{
		cg_pmove.freeze = qfalse;
	}
	//if (cg.pauseState & UT_PAUSE_ON)  cg_pmove.freeze = qtrue;

    // If we're dead.
    if (cg_pmove.ps->pm_type == PM_DEAD) {
        cg_pmove.tracemask = MASK_PLAYERSOLID & ~CONTENTS_BODY;
    } else if ((cgs.gametype == GT_JUMP) && (cg_ghost.integer > 0)) {
        cg_pmove.tracemask = (MASK_PLAYERSOLID & ~CONTENTS_BODY) | CONTENTS_CORPSE;
    } else {
        cg_pmove.tracemask = MASK_PLAYERSOLID;
    }

	// If we're a spectator or a ghost.
	if ((cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR) || (cg.snap->ps.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_GHOST)) {
		cg_pmove.tracemask &= ~CONTENTS_BODY;
	}

	// Save current playerstate.
	oldPlayerState = cg.predictedPlayerState;

	// Get newest command number.
	newestCommandNum = trap_GetCurrentCmdNumber();

	// Calculate oldest command number.
	oldestCommandNum = (newestCommandNum - CMD_BACKUP) + 1;

	// Get oldest command.
	trap_GetUserCmd(oldestCommandNum, &oldestCommand);

	// If there's a commands buffer overflow; make sure it's not caused by map_restart.
	if ((oldestCommand.serverTime > cg.snap->ps.commandTime) && (oldestCommand.serverTime < cg.time))
	{
		// If we need to display debug info.
		if (cg_showmiss.integer)
		{
			CG_Printf("exceeded PACKET_BACKUP on commands\n");
		}

		return;
	}

	// Get newest command.
	trap_GetUserCmd(newestCommandNum, &newestCommand);

	// If a new snapshot arrived.
	if (cg.nextSnap && !cg.nextFrameTeleport && !cg.thisFrameTeleport && (!cg_optimize.integer || (cg.nextSnap->serverTime != cg.lastPredictedNextSnapTime)))
	{
		// Reinitialize with next snapshot's playerstate.
		cg.predictedPlayerState = cg.nextSnap->ps;

		// Set index to oldest command in buffer.
		commandNum = oldestCommandNum;

		// Set physics time.
		cg.physicsTime = cg.nextSnap->serverTime;

		// Backup snap time.
		cg.lastPredictedNextSnapTime = cg.nextSnap->serverTime;
	}
	else
	{
		// If this is the first time we're predicting or we're not optimizing.
		if (!cg.validPPS || !cg_optimize.integer)
		{
			// Reinitialize with current snapshot's playerstate.
			cg.predictedPlayerState = cg.snap->ps;

			// Set index to oldest command in buffer.
			commandNum = oldestCommandNum;

			// Set physics time.
			cg.physicsTime = cg.snap->serverTime;
		}
		else
		{
			// Set index to the newest command.
			commandNum = newestCommandNum;

			// Set physics time.
			cg.physicsTime = cg.oldTime;
		}
	}

	// If this is the first time we're predicting.
	if (!cg.validPPS)
	{
		cg.validPPS = qtrue;
	}

	ppscount = 0;

	// Prediction loop.
	for (; commandNum <= newestCommandNum; commandNum++)
	{
		// Get command.
		trap_GetUserCmd(commandNum, &cg_pmove.cmd);

		//#ifdef PMOVE_FIXEDY
		if (cg_pmove.physics && !cg_pmove.freeze)
		{
			// Update angles.
			if (!(cg.pauseState & UT_PAUSE_ON))
			{
				PM_UpdateViewAngles(cg_pmove.ps, &cg_pmove.cmd);
			}
		}
//#endif

		// If this command is too old.
		if (cg_pmove.cmd.serverTime <= cg.predictedPlayerState.commandTime)
		{
			continue;
		}

		// If this command was before a map_restart.
		if (cg_pmove.cmd.serverTime > newestCommand.serverTime)
		{
			continue;
		}

		// Check for prediction errors.
		if (cg.predictedPlayerState.commandTime == oldPlayerState.commandTime)
		{
			// A teleport will not cause an error decay.
			if (cg.thisFrameTeleport)
			{
				VectorClear(cg.predictedError);

				// If we need to display debug info.
				if (cg_showmiss.integer)
				{
					CG_Printf("PredictionTeleport\n");
				}

				cg.thisFrameTeleport = qfalse;
			}
			else
			{
				CG_AdjustPositionForMover(cg.predictedPlayerState.origin, cg.predictedPlayerState.viewangles,
							  cg.predictedPlayerState.groundEntityNum, cg.physicsTime, cg.oldTime, adjusted, mt);

				// If we need to display debug info.
				if (cg_showmiss.integer)
				{
					if (!VectorCompare(oldPlayerState.origin, adjusted))
					{
						CG_Printf("prediction error\n");
					}
				}

				VectorSubtract(oldPlayerState.origin, adjusted, delta);
				length = VectorLength(delta);

				if (length > 0.1f)
				{
					// If we need to display debug info.
					if (cg_showmiss.integer)
					{
						CG_Printf("Prediction miss: %f\n", length);
					}

					if (cg_errorDecay.integer)
					{
						t = cg.time - cg.predictedErrorTime;
						f = (float)(cg_errorDecay.value - t) / cg_errorDecay.value;

						if (f < 0.0f)
						{
							f = 0.0f;
						}

						// If we need to display debug info.
						if ((f > 0.0f) && cg_showmiss.integer)
						{
							CG_Printf("Double prediction decay: %f\n", f);
						}

						VectorScale(cg.predictedError, f, cg.predictedError);
					}
					else
					{
						VectorClear(cg.predictedError);
					}

					VectorAdd(delta, cg.predictedError, cg.predictedError);
					cg.predictedErrorTime = cg.oldTime;
				}
			}
		}

		// No prediction for gauntlet.
		cg_pmove.gauntletHit = qfalse;

//#ifdef PMOVE_FIXEDY
		if (cg_pmove.physics)
		{
			// Voodoo calculation.
			cg_pmove.cmd.serverTime = ((cg_pmove.cmd.serverTime + PMOVE_MSEC - 1) / PMOVE_MSEC) * PMOVE_MSEC;
		}
//#endif

		// If we're paused and we're not a spectator.
		if ((cg.pauseState & UT_PAUSE_ON) && (cgs.clientinfo[cg.predictedPlayerState.clientNum].team != TEAM_SPECTATOR))
		{
			// Restore angles.
			cg_pmove.cmd.angles[0] = cg.oldAngles[0];
			cg_pmove.cmd.angles[1] = cg.oldAngles[1];
			cg_pmove.cmd.angles[2] = cg.oldAngles[2];

			// Clear motion.
			cg_pmove.cmd.forwardmove = 0;
			cg_pmove.cmd.rightmove	 = 0;
			cg_pmove.cmd.upmove	 = 0;

			// Clear buttons.
			cg_pmove.cmd.buttons = 0;

			//@Barbatos
			cg_pmove.paused = qtrue;

			// Backup origin and view angles.
			VectorCopy(cg.predictedPlayerState.origin, backupOrigin);
			VectorCopy(cg.predictedPlayerState.viewangles, backupAngles);
		}
		else
		{
			// Backup angles.
			cg.oldAngles[0] = cg_pmove.cmd.angles[0];
			cg.oldAngles[1] = cg_pmove.cmd.angles[1];
			cg.oldAngles[2] = cg_pmove.cmd.angles[2];

			//@Barbatos
			cg_pmove.paused = qfalse;
		}

		// If we're alive.
		if (cg.snap->ps.stats[STAT_HEALTH] > 0)
		{
			// If we're planting.
			if (cg.snap->ps.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_PLANTING)
			{
				// Lock player in place.
				cg_pmove.cmd.forwardmove = 0;
				cg_pmove.cmd.rightmove	 = 0;

				if (cg_pmove.cmd.upmove > 0)
				{
					cg_pmove.cmd.upmove = 0;
				}
			}
		}

		// Predict.
		Pmove(&cg_pmove);

		// If we're paused and we're not a spectator.
		if ((cg.pauseState & UT_PAUSE_ON) && (cgs.clientinfo[cg.predictedPlayerState.clientNum].team != TEAM_SPECTATOR))
		{
			// Restore origin and view angles.
			VectorCopy(backupOrigin, cg.predictedPlayerState.origin);
			VectorCopy(backupAngles, cg.predictedPlayerState.viewangles);
		}

		// Predict push triggers and items.
		//	CG_TouchTriggerPrediction();

		// If we need to display debug info.
		if (cg_showmiss.integer)
		{
			// Count number of prediction loops.
			cg.ppsCount++;
			ppscount++;
		}
	}

	if (cg_showmiss.integer == 2)
	{
		CG_Printf("pps: %i\n", ppscount);
	}

	// If we need to display extra debug info.
	if (cg_showmiss.integer > 1)
	{
		CG_Printf("[%i : %i] ", cg_pmove.cmd.serverTime, cg.time);
	}

	// Adjust for the movement of the ground entity.
	CG_AdjustPositionForMover(cg.predictedPlayerState.origin, cg.predictedPlayerState.viewangles, cg.predictedPlayerState.groundEntityNum,
				  cg.physicsTime, cg.time, cg.predictedPlayerState.origin, cg.predictedPlayerState.viewangles);

	// If we need to display debug info.
	if (cg_showmiss.integer)
	{
		// If event(s) were dropped.
		if (cg.predictedPlayerState.eventSequence > (oldPlayerState.eventSequence + MAX_PS_EVENTS))
		{
			CG_Printf("WARNING: dropped event\n");
		}
	}

	// Fire events and other transition stuff.
	CG_TransitionPlayerState(&cg.predictedPlayerState, &oldPlayerState);

	// If we need to display debug info.
	if (cg_showmiss.integer)
	{
		if (cg.eventSequence > cg.predictedPlayerState.eventSequence)
		{
			CG_Printf("WARNING: double event\n");

			cg.eventSequence = cg.predictedPlayerState.eventSequence;
		}
	}
}
#else
int  sprint = 0;
/**
 * $(function)
 */
void CG_PredictPlayerState( void )
{
	int	       cmdNum, current;
	playerState_t  oldPlayerState;
	qboolean       moved;
	usercmd_t      oldestCmd;
	usercmd_t      latestCmd;
	int	       pmoveCount = 0;

	// Sprint
	if (trap_Key_IsDown ( trap_Key_GetKey ( "+moveleft" )) &&
	    trap_Key_IsDown ( trap_Key_GetKey ( "+moveright" )))
	{
		if (!sprint)
		{
			trap_SendConsoleCommand ( "+button8; " );
		}
		sprint = 1;
	}
	else
	{
		if (sprint)
		{
			trap_SendConsoleCommand ( "-button8; " );
		}
		sprint = 0;
	}

	cg.hyperspace = qfalse; // will be set if touching a trigger_teleport

	// if this is the first frame we must guarantee
	// predictedPlayerState is valid even if there is some
	// other error condition
	if (!cg.validPPS)
	{
		cg.validPPS		= qtrue;
		cg.predictedPlayerState = cg.snap->ps;
	}

	// demo playback just copies the moves
	if (cg.demoPlayback || (cg.snap->ps.pm_flags & PMF_FOLLOW))
	{
		CG_InterpolatePlayerState( qfalse );

/*
		{
			usercmd_t	cmd;
			int			cmdNum;

			cmdNum = trap_GetCurrentCmdNumber();
			trap_GetUserCmd( cmdNum, &cmd );

//			PM_UpdateViewAngles( out, &cmd );

			cmdNum = SHORT2ANGLE(cmd.angles[YAW]);
			CG_Printf ( "%i\n", cmdNum );
			trap_Cvar_Set ( "cg_thirdPersonAngle", va("%i",cmdNum) );
		}
*/

		return;
	}

	// non-predicting local movement will grab the latest angles
	if (cg_nopredict.integer || cg_synchronousClients.integer)
	{
		CG_InterpolatePlayerState( qtrue );
		return;
	}

	// prepare for pmove
	cg_pmove.ps	       = &cg.predictedPlayerState;
	cg_pmove.trace	       = CG_Trace;
	cg_pmove.pointcontents = CG_PointContents;
	cg_pmove.warmup        = cg.warmup == 0 ? 0 : 1;
	//cg_pmove.noFiring = (!cg_antilag.integer && cg_noweaponpredict.integer)?qtrue:qfalse;
	cg_pmove.noFiring      = (cg_noweaponpredict.integer) ? qtrue : qfalse;
	cg_pmove.ariesModel    = cgs.clientinfo[cg.predictedPlayerState.clientNum].gender == GENDER_FEMALE;

	if (cg_pmove.ps->pm_type == PM_DEAD)
	{
		cg_pmove.tracemask = MASK_PLAYERSOLID & ~CONTENTS_BODY;
	}
	else
	{
		cg_pmove.tracemask = MASK_PLAYERSOLID;
	}

	if ((cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR) || (cg.snap->ps.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_GHOST))
	{
		cg_pmove.tracemask &= ~CONTENTS_BODY;	// spectators can fly through bodies
	}
//   cg_pmove.noFootsteps = ( cgs.dmflags & DF_NO_FOOTSTEPS ) > 0;

	// save the state before the pmove so we can detect transitions
	oldPlayerState = cg.predictedPlayerState;

	current        = trap_GetCurrentCmdNumber();

	// if we don't have the commands right after the snapshot, we
	// can't accurately predict a current position, so just freeze at
	// the last good position we had
	cmdNum = current - CMD_BACKUP + 1;
	trap_GetUserCmd( cmdNum, &oldestCmd );

	if ((oldestCmd.serverTime > cg.snap->ps.commandTime)
	    && (oldestCmd.serverTime < cg.time))	// special check for map_restart
	{
		if (cg_showmiss.integer)
		{
			CG_Printf ("exceeded PACKET_BACKUP on commands\n");
		}
		return;
	}

	// get the latest command so we can know which commands are from previous map_restarts
	trap_GetUserCmd( current, &latestCmd );

//latestCmd.angles[0]=0;//27 hack
	// get the most recent information we have, even if
	// the server time is beyond our current cg.time,
	// because predicted player positions are going to
	// be ahead of everything else anyway
	if (cg.nextSnap && !cg.nextFrameTeleport && !cg.thisFrameTeleport)
	{
		cg.predictedPlayerState = cg.nextSnap->ps;
		cg.physicsTime		= cg.nextSnap->serverTime;
	}
	else
	{
		cg.predictedPlayerState = cg.snap->ps;
		cg.physicsTime		= cg.snap->serverTime;
	}

//	if ( pmove_msec.integer < 8 ) {
//		trap_Cvar_Set("pmove_msec", "8");
//	}
//	else if (pmove_msec.integer > 33) {
//		trap_Cvar_Set("pmove_msec", "33");
//	}

//	cg_pmove.pmove_fixed = pmove_fixed.integer;// | cg_pmove_fixed.integer;
//	cg_pmove.pmove_msec = pmove_msec.integer;

	// run cmds
	moved = qfalse;

	for (cmdNum = current - CMD_BACKUP + 1 ; cmdNum <= current ; cmdNum++)
	{
		// get the command
		trap_GetUserCmd( cmdNum, &cg_pmove.cmd );
		//	if ( cg_pmove.pmove_fixed )
		// {
 #ifdef PMOVE_FIXEDY
		PM_UpdateViewAngles( cg_pmove.ps, &cg_pmove.cmd );
 #endif
		//	}

		// don't do anything if the time is before the snapshot player time
		if (cg_pmove.cmd.serverTime <= cg.predictedPlayerState.commandTime)
		{
			continue;
		}

		// don't do anything if the command was from a previous map_restart
		if (cg_pmove.cmd.serverTime > latestCmd.serverTime)
		{
			continue;
		}

		// check for a prediction error from last frame
		// on a lan, this will often be the exact value
		// from the snapshot, but on a wan we will have
		// to predict several commands to get to the point
		// we want to compare
		if (cg.predictedPlayerState.commandTime == oldPlayerState.commandTime)
		{
			vec3_t	delta;
			float	len;

			if (cg.thisFrameTeleport)
			{
				// a teleport will not cause an error decay
				VectorClear( cg.predictedError );

				if (cg_showmiss.integer)
				{
					CG_Printf( "PredictionTeleport\n" );
				}
				cg.thisFrameTeleport = qfalse;
			}
			else
			{
				vec3_t	adjusted, mt;

				CG_AdjustPositionForMover( cg.predictedPlayerState.origin, cg.predictedPlayerState.viewangles,
							   cg.predictedPlayerState.groundEntityNum, cg.physicsTime, cg.oldTime, adjusted, mt );

				if (cg_showmiss.integer)
				{
					if (!VectorCompare( oldPlayerState.origin, adjusted ))
					{
						CG_Printf("prediction error\n");
					}
				}
				VectorSubtract( oldPlayerState.origin, adjusted, delta );
				len = VectorLength( delta );

				if (len > 0.1)
				{
					if (cg_showmiss.integer)
					{
						CG_Printf("Prediction miss: %f\n", len);
					}

					if (cg_errorDecay.integer)
					{
						int    t;
						float  f;

						t = cg.time - cg.predictedErrorTime;
						f = (cg_errorDecay.value - t) / cg_errorDecay.value;

						if (f < 0)
						{
							f = 0;
						}

						if ((f > 0) && cg_showmiss.integer)
						{
							CG_Printf("Double prediction decay: %f\n", f);
						}
						VectorScale( cg.predictedError, f, cg.predictedError );
					}
					else
					{
						VectorClear( cg.predictedError );
					}
					VectorAdd( delta, cg.predictedError, cg.predictedError );
					cg.predictedErrorTime = cg.oldTime;
				}
			}
		}

		// don't predict gauntlet firing, which is only supposed to happen
		// when it actually inflicts damage
		cg_pmove.gauntletHit = qfalse;

//		if ( cg_pmove.pmove_fixed )
 #ifdef PMOVE_FIXEDY
		{
			cg_pmove.cmd.serverTime = ((cg_pmove.cmd.serverTime + PMOVE_MSEC - 1) / PMOVE_MSEC) * PMOVE_MSEC;
		}
 #endif
		{
			vec3_t	    oldOrigin;
			vec3_t	    oldViewAngles;
			static int  traces = 0;
			cg_traceCount = 0;

			// If we're paused and we're not a spectator.
			if (cg.pauseState & UT_PAUSE_ON && (cgs.clientinfo[cg.predictedPlayerState.clientNum].team != TEAM_SPECTATOR))
			{
				// Restore angles.
				cg_pmove.cmd.angles[0] = cg.oldAngles[0];
				cg_pmove.cmd.angles[1] = cg.oldAngles[1];
				cg_pmove.cmd.angles[2] = cg.oldAngles[2];

				// Clear motion.
				cg_pmove.cmd.forwardmove = 0;
				cg_pmove.cmd.rightmove	 = 0;
				cg_pmove.cmd.upmove	 = 0;

				// Clear buttons.
				cg_pmove.cmd.buttons = 0;

				// Backup origin and view angles.
				VectorCopy(cg.predictedPlayerState.origin, oldOrigin);
				VectorCopy(cg.predictedPlayerState.viewangles, oldViewAngles);
			}
			else
			{
				// Backup angles.
				cg.oldAngles[0] = cg_pmove.cmd.angles[0];
				cg.oldAngles[1] = cg_pmove.cmd.angles[1];
				cg.oldAngles[2] = cg_pmove.cmd.angles[2];
			}

			// Bomb Planting & defusing only!
			/*
			if( cg.snap->ps.stats[ STAT_UTPMOVE_FLAGS ] & UT_PMF_PLANTING ||
				cg.snap->ps.stats[ STAT_UTPMOVE_FLAGS ] & UT_PMF_DEFUSING ) { // No moving while defusing or planting!
				cg_pmove.cmd.forwardmove = 0;
				cg_pmove.cmd.rightmove = 0;
				cg_pmove.cmd.upmove = -127;
			}
			*/

			/*
			// If we're planting.
			if (cg.snap->ps.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_PLANTING)
			{
				// If we're moving.
				if (VectorLength(cg.snap->ps.velocity) > 0.0f)
				{
					// Cancel planting.
					cg.snap->ps.stats[STAT_UTPMOVE_FLAGS] &= ~UT_PMF_PLANTING;
					cg.snap->ps.stats[STAT_UTPMOVE_FLAGS] &= ~UT_PMF_FIRE_HELD;
					cg.snap->ps.stats[STAT_UTPMOVE_FLAGS] &= ~UT_PMF_READY_FIRE;
					cg.snap->ps.weaponTime = 0;
					cg.snap->ps.weaponstate = WEAPON_READY;
				}
			}
			*/

			/*
			// If we're difusing.
			if (cg.snap->ps.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_DEFUSING)
			{
				// If we're moving.
				if (VectorLength(cg.snap->ps.velocity) > 0.0f)
				{
					// Cancel difusing.
					cg.snap->ps.stats[STAT_UTPMOVE_FLAGS] &= ~UT_PMF_DEFUSING;
				}
			}
			*/

			// If we're alive.
			if (cg.snap->ps.stats[STAT_HEALTH] > 0)
			{
				// If we're planting.
				if (cg.snap->ps.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_PLANTING)
				{
					// Lock player in place.
					cg_pmove.cmd.forwardmove = 0;
					cg_pmove.cmd.rightmove	 = 0;

					if (cg_pmove.cmd.upmove > 0)
					{
						cg_pmove.cmd.upmove = 0;
					}
				}
			}

			Pmove (&cg_pmove);
			// Now EV_STEP is handled in PmoveSingle.
			//CG_StepAdjust();

			// If we're paused and we're not a spectator.
			if (cg.pauseState & UT_PAUSE_ON && (cgs.clientinfo[cg.predictedPlayerState.clientNum].team != TEAM_SPECTATOR))
			{
				// Restore origin and view angles.
				VectorCopy(oldOrigin, cg.predictedPlayerState.origin);
				VectorCopy(oldViewAngles, cg.predictedPlayerState.viewangles);
			}

			if (traces != cg_traceCount)
			{
				traces = cg_traceCount;
//				Com_Printf ( "TRACES:  %i\n", traces );
			}
		}

		moved = qtrue;

		// add push trigger movement effects
		CG_TouchTriggerPrediction();

		// check for predictable events that changed from previous predictions
		//CG_CheckChangedPredictableEvents(&cg.predictedPlayerState);
	}

	if (cg_showmiss.integer > 1)
	{
		CG_Printf( "[%i : %i] ", cg_pmove.cmd.serverTime, cg.time );
	}

	/*if ( !moved ) {
		if ( cg_showmiss.integer ) {
			CG_Printf( "not moved\n" );
		}
		return;
	}*/

	// adjust for the movement of the groundentity
	CG_AdjustPositionForMover( cg.predictedPlayerState.origin, cg.predictedPlayerState.viewangles,
				   cg.predictedPlayerState.groundEntityNum,
				   cg.physicsTime, cg.time, cg.predictedPlayerState.origin, cg.predictedPlayerState.viewangles );

	if (cg_showmiss.integer)
	{
		if (cg.predictedPlayerState.eventSequence > oldPlayerState.eventSequence + MAX_PS_EVENTS)
		{
			CG_Printf("WARNING: dropped event\n");
		}
	}

	// fire events and other transition triggered things
	CG_TransitionPlayerState( &cg.predictedPlayerState, &oldPlayerState );

	if (cg_showmiss.integer)
	{
		if (cg.eventSequence > cg.predictedPlayerState.eventSequence)
		{
			CG_Printf("WARNING: double event\n");
			cg.eventSequence = cg.predictedPlayerState.eventSequence;
		}
	}
}
#endif
