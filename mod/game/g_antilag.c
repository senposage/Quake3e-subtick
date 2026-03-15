//--------------------------------------------------------------------------------
// Urban Terror
// By SID
// Coder: DensitY and others
//--------------------------------------------------------------------------------
// g_antilag.c
//--------------------------------------------------------------------------------

/*
	Shifty anitlag. First Modified by Dokta8

	Added:
		FIFO Trail buffer System - DensitY
*/

//--------------------------------------------------------------------------------
// Includes
//--------------------------------------------------------------------------------

#include "g_local.h"

//--------------------------------------------------------------------------------
// The Code
//--------------------------------------------------------------------------------

/*
============
G_ResetTrail

Clear out the given client's origin trails (should be called from ClientBegin and when
the teleport bit is toggled)
============
*/
void G_ResetTrail( gentity_t *ent )
{
	int  i, time;

	time = level.time;

	for (i = 0;  i < NUM_CLIENT_TRAILS;  i++)
	{
		VectorCopy( ent->s.pos.trBase, ent->client->FIFOTrail[i].currentOrigin );

		// extra client data needed for ARIES
		VectorCopy( ent->s.apos.trBase, ent->client->FIFOTrail[i].currentAngles );
		ent->client->FIFOTrail[i].torsoanim = ent->s.torsoAnim;
		ent->client->FIFOTrail[i].legsanim  = ent->s.legsAnim;

		VectorCopy( ent->s.pos.trDelta, ent->client->FIFOTrail[i].sPosTrDelta );
		ent->client->FIFOTrail[i].legPitchAngle   = ent->client->legsPitchAngle;
		ent->client->FIFOTrail[i].legYawAngle	  = ent->client->legsYawAngle;
		ent->client->FIFOTrail[i].torsoPitchAngle = ent->client->torsoPitchAngle;
		ent->client->FIFOTrail[i].torsoYawAngle   = ent->client->torsoYawAngle;

		//ent->client->FIFOTrail[i].leveltime = time;
		ent->client->FIFOTrail[i].time = time;
		ent->client->isSaved	       = qfalse;
	}
}

/*=---- FIFO Buffer Stored Trail ----=*/

void G_StoreFIFOTrail( gentity_t *ent )
{
	int  newtime;

//`	int				i;
	clientTrail_t  buffer[NUM_CLIENT_TRAILS];

	if(!ent->client)
	{
		return;
	}
//	if( ent->client->FIFOTrail[ 0 ].leveltime < level.time )
	{
//		ent->client->FIFOTrail[ 0 ].time = level.previousTime;
	}
	memcpy( &buffer[0], &ent->client->FIFOTrail[0], sizeof(clientTrail_t) * (NUM_CLIENT_TRAILS));
	memcpy( &ent->client->FIFOTrail[1], &buffer[0], sizeof(clientTrail_t) * (NUM_CLIENT_TRAILS - 1));
/*	if( ent->r.svFlags & SVF_BOT ) {
		newtime = level.time;
	}
	else {
		TrapTime = trap_Milliseconds();
	newtime =  level.previousTime + ((TrapTime - level.frameStartTime)); // 50); // CHANGED BY HIGHSEA.

//    Com_Printf( "p: %i    t: %i \n",level.previousTime,TrapTime);
*/

      /*		if( newtime > level.time ) {
			newtime = level.time;
		}
		else if( newtime <= level.previousTime ) {
			newtime = level.previousTime + 1;
		}  */

//		newtime = level.time;// + (1000/g_fps.integer)	;
//}

	newtime = level.time;

	VectorCopy( ent->s.pos.trBase, ent->client->FIFOTrail[0].currentOrigin );
	VectorCopy( ent->s.apos.trBase, ent->client->FIFOTrail[0].currentAngles );
	VectorCopy( ent->s.pos.trDelta, ent->client->FIFOTrail[0].sPosTrDelta );
	ent->client->FIFOTrail[0].torsoanim	  = ent->s.torsoAnim;
	ent->client->FIFOTrail[0].legsanim	  = ent->s.legsAnim;

	ent->client->FIFOTrail[0].legsFrame	  = ent->client->animationdata.legs.frame;
	ent->client->FIFOTrail[0].torsoFrame	  = ent->client->animationdata.torso.frame;

	ent->client->FIFOTrail[0].legPitchAngle   = ent->client->legsPitchAngle;
	ent->client->FIFOTrail[0].legYawAngle	  = ent->client->legsYawAngle;
	ent->client->FIFOTrail[0].torsoPitchAngle = ent->client->torsoPitchAngle;
	ent->client->FIFOTrail[0].torsoYawAngle   = ent->client->torsoYawAngle;
	//ent->client->FIFOTrail[ 0 ].leveltime = level.time;
	ent->client->FIFOTrail[0].time		  = newtime; //+ ( 1000 / 20 );
}

/*
=============
TimeShiftLerp

Used below to interpolate between two previous vectors
Returns a vector "frac" times the distance between "start" and "end"
=============
*/
static void TimeShiftLerp( double frac, vec3_t start, vec3_t end, vec3_t result )
{
	double	comp = 1.0 - frac;

	result[0] = (float)((comp * (double)start[0]) + (frac * (double)end[0]));
	result[1] = (float)((comp * (double)start[1]) + (frac * (double)end[1]));
	result[2] = (float)((comp * (double)start[2]) + (frac * (double)end[2]));
}

/*=---- FIFO TimeShift ----=*/

void G_FIFOTimeShiftClient( gentity_t *ent, int time )
{
	int	i;
	int	FIFOFar;
	int	FIFONear;
	double	frac;

	if(!ent->client)
	{
		return;
	}
	ent->client->isSaved = qtrue;

	VectorCopy( ent->r.currentOrigin, ent->client->saved.saved_rOrigin );
	VectorCopy( ent->s.pos.trBase, ent->client->saved.currentOrigin );

	VectorCopy( ent->s.apos.trBase, ent->client->saved.currentAngles );
	ent->client->saved.torsoanim	   = ent->s.torsoAnim;
	ent->client->saved.legsanim	   = ent->s.legsAnim;
	VectorCopy( ent->s.pos.trDelta, ent->client->saved.sPosTrDelta );
	ent->client->saved.legPitchAngle   = ent->client->legsPitchAngle;
	ent->client->saved.legYawAngle	   = ent->client->legsYawAngle;
	ent->client->saved.torsoPitchAngle = ent->client->torsoPitchAngle;
	ent->client->saved.torsoYawAngle   = ent->client->torsoYawAngle;

	ent->client->saved.legsFrame	   = ent->client->animationdata.legs.frame;
	ent->client->saved.torsoFrame	   = ent->client->animationdata.torso.frame;

	//ent->client->saved.leveltime = level.time;
	FIFOFar  = 0;
	FIFONear = 0;

	for(i = 1; i < NUM_CLIENT_TRAILS; i++)
	{
		if(ent->client->FIFOTrail[i].time <= time) // Must be less then!
		{
			FIFONear = i - 1;
			FIFOFar  = i;
			break;
		}
	}

	if(FIFOFar == 0)
	{
		FIFONear = 0;
		FIFOFar  = 0;

		if((float)(ent->client->FIFOTrail[0].time - ent->client->FIFOTrail[1].time != 0 ))
		{
			frac  = (double)((time - ent->client->FIFOTrail[0].time) /
					 (double)( ent->client->FIFOTrail[0].time - ent->client->FIFOTrail[1].time  ));
			frac += 1.0;
		}
		else
		{
			frac = 1;
		}
	}
	else
	{
		if (ent->client->FIFOTrail[FIFONear].time - ent->client->FIFOTrail[FIFOFar].time != 0)
		{
			frac = (double)((time - ent->client->FIFOTrail[FIFOFar].time) /
					(double)( ent->client->FIFOTrail[FIFONear].time - ent->client->FIFOTrail[FIFOFar].time ));
		}
		else
		{
			frac = 0;
		}
	}
	TimeShiftLerp( frac, ent->client->FIFOTrail[FIFOFar].sPosTrDelta, ent->client->FIFOTrail[FIFONear].sPosTrDelta, ent->s.pos.trDelta );
	TimeShiftLerp( frac, ent->client->FIFOTrail[FIFOFar].currentOrigin, ent->client->FIFOTrail[FIFONear].currentOrigin, ent->s.pos.trBase );

	ent->r.currentOrigin[0] 	       = ent->s.pos.trBase[0];
	ent->r.currentOrigin[1] 	       = ent->s.pos.trBase[1];
	ent->r.currentOrigin[2] 	       = ent->s.pos.trBase[2];

	ent->s.apos.trBase[0]		       = LerpAngle( ent->client->FIFOTrail[FIFOFar].currentAngles[0], ent->client->FIFOTrail[FIFONear].currentAngles[0], frac );
	ent->s.apos.trBase[1]		       = LerpAngle( ent->client->FIFOTrail[FIFOFar].currentAngles[1], ent->client->FIFOTrail[FIFONear].currentAngles[1], frac );
	ent->s.apos.trBase[2]		       = LerpAngle( ent->client->FIFOTrail[FIFOFar].currentAngles[2], ent->client->FIFOTrail[FIFONear].currentAngles[2], frac );

	ent->client->legsPitchAngle	       = ent->client->FIFOTrail[FIFONear].legPitchAngle;
	ent->client->legsYawAngle	       = ent->client->FIFOTrail[FIFONear].legYawAngle;
	ent->client->torsoPitchAngle	       = ent->client->FIFOTrail[FIFONear].torsoPitchAngle;
	ent->client->torsoYawAngle	       = ent->client->FIFOTrail[FIFONear].torsoYawAngle;
	ent->s.torsoAnim		       = ent->client->FIFOTrail[FIFONear].torsoanim;
	ent->s.legsAnim 		       = ent->client->FIFOTrail[FIFONear].legsanim;

	ent->client->animationdata.legs.frame  = ent->client->FIFOTrail[FIFONear].legsFrame;
	ent->client->animationdata.torso.frame = ent->client->FIFOTrail[FIFONear].torsoFrame;

	trap_LinkEntity( ent );

	// Debug print remove for Public release
#if 0	// 1 to enable
	Com_Printf( "\nFIFONear: %i, FIFOFar: %i. ShotTime: %i, Level Time: %i\n"
		    "FIFONear Time: %i, FIFOFar Time: %i\nFrac: %2.2f Diff: %i \n0: %i\n1: %i\n2: %i\n3: %i\n4: %i\n",
		    FIFONear,
		    FIFOFar,
		    time,
		    level.time,
		    ent->client->FIFOTrail[FIFONear].time,
		    ent->client->FIFOTrail[FIFOFar].time,
		    frac,
		    ent->client->FIFOTrail[FIFONear].time - ent->client->FIFOTrail[FIFOFar].time,
		    ent->client->FIFOTrail[0].time,
		    ent->client->FIFOTrail[1].time,
		    ent->client->FIFOTrail[2].time,
		    ent->client->FIFOTrail[3].time,
		    ent->client->FIFOTrail[4].time

		    );
#endif
}

/*
=====================
G_TimeShiftAllClients

Move ALL clients back to where they were at the specified "time",
except for "skip"
=====================
*/
void G_TimeShiftAllClients( int time, gentity_t *skip )
{
#if 0	// old system
	int	   i;
	gentity_t  *ent;

	if (time > level.time)
	{
		time = level.time;
	}

	// for every client
	ent = &g_entities[0];

	for (i = 0; i < MAX_CLIENTS; i++, ent++)
	{
		if (ent->client && ent->inuse && (ent->client->sess.sessionTeam < TEAM_SPECTATOR) && (ent != skip))
		{
			G_TimeShiftClient( ent, time );
		}
	}
#else	// FIFO system
	int	   i;
	gentity_t  *ent;

	ent = &g_entities[0];

	for(i = 0; i < MAX_CLIENTS; i++, ent++)
	{
		if(ent->client && ent->inuse && (ent->client->sess.sessionTeam < TEAM_SPECTATOR) && (ent != skip) && !ent->client->pers.substitute && !ent->client->ghost)
		{
			G_FIFOTimeShiftClient( ent, time );
		}
	}
#endif
}

/*
===================
G_UnTimeShiftClient

Move a client back to where he was before the time shift
===================
*/
void G_UnTimeShiftClient( gentity_t *ent )
{
	// if it was saved
	if (ent->client->isSaved == qfalse)
	{
		return;
	}

	ent->client->isSaved = qfalse;

	//  if ( ent->client->saved.leveltime == level.time ) {
	// move it back
	VectorCopy( ent->client->saved.currentOrigin, ent->s.pos.trBase );

	// Untimeshift ARIES extra data
	VectorCopy( ent->client->saved.currentAngles, ent->s.apos.trBase );
	VectorCopy( ent->client->saved.saved_rOrigin, ent->r.currentOrigin);
	ent->s.torsoAnim = ent->client->saved.torsoanim;
	ent->s.legsAnim  = ent->client->saved.legsanim;

	VectorCopy( ent->client->saved.sPosTrDelta, ent->s.pos.trDelta );
	ent->client->legsPitchAngle	       = ent->client->saved.legPitchAngle;
	ent->client->legsYawAngle	       = ent->client->saved.legYawAngle;
	ent->client->torsoPitchAngle	       = ent->client->saved.torsoPitchAngle;
	ent->client->torsoYawAngle	       = ent->client->saved.torsoYawAngle;

	ent->client->animationdata.legs.frame  = ent->client->saved.legsFrame;
	ent->client->animationdata.torso.frame = ent->client->saved.torsoFrame ;

	//	ent->client->saved.leveltime = 0;

	// this will recalculate absmin and absmax
	trap_LinkEntity( ent );
//	}
}

/*
=======================
G_UnTimeShiftAllClients

Move ALL the clients back to where they were before the time shift,
except for "skip"
=======================
*/
void G_UnTimeShiftAllClients( gentity_t *skip )
{
	int	   i;
	gentity_t  *ent;

	ent = &g_entities[0];

	for (i = 0; i < MAX_CLIENTS; i++, ent++)
	{
		if (ent->client && ent->inuse && (ent->client->sess.sessionTeam < TEAM_SPECTATOR) && (ent != skip) && !ent->client->pers.substitute && !ent->client->ghost)
		{
			G_UnTimeShiftClient( ent );
		}
	}
}

/* -------------- APOXOL'S ORIGINAL ANTILAG CODE PAST THIS POINT (FOR REFERENCE ) -------------- */

/*
////////////////////////////////////////////////////////////////////////////////
//
// Name       : g_antilag.c
// Description: Implements server side lag compensation
// Author     : Apoxol
//
////////////////////////////////////////////////////////////////////////////////
//#include "g_local.h"

#define MAX_HISTORYPOOL (MAX_CLIENTS*MAX_SERVER_FPS)

ghistory_t	g_historypool[MAX_HISTORYPOOL];
ghistory_t* g_historypoolHead = NULL;
qboolean	g_historypoolInit = qfalse;

////////////////////////////////////////////////////////////////////////////////
// Name       : AntiLag_Initialize
// Description: Initializes the anti-lag code
////////////////////////////////////////////////////////////////////////////////
void AntiLag_Initialize ( )
{
	int i;

	// If the pool has already been allocated then dont do it again
	if ( g_historypoolInit )
		return;

	// Initialize the head node
	g_historypoolInit = qtrue;
	g_historypoolHead = &g_historypool[0];
	g_historypool[0].next = NULL;
	g_historypool[0].prev = NULL;

	// Add all other pool entries to the list
	for ( i = 1; i < MAX_HISTORYPOOL; i ++ )
	{
		g_historypool[i].next = g_historypoolHead;
		g_historypool[i].prev = NULL;
		g_historypoolHead->prev = g_historypoolHead;
		g_historypoolHead = &g_historypool[i];
	}
}

////////////////////////////////////////////////////////////////////////////////
// Name       : AntiLag_AllocHistory
// Description: Allocate a history structure from the history pool
////////////////////////////////////////////////////////////////////////////////
ghistory_t* AntiLag_AllocHistory ( )
{
	ghistory_t* history;

	// If the history pool hasnt been initialized yet then
	// do so now.
	if ( NULL == g_historypoolHead )
	{
		if ( !g_historypoolInit )
			AntiLag_Initialize ( );
		else
			return NULL;
	}

	// Take the head node off of the history pool
	history = g_historypoolHead;
	g_historypoolHead = history->next;

	if ( g_historypoolHead )
		g_historypoolHead->prev = NULL;

	history->next = NULL;
	history->prev = NULL;

	return history;
}

////////////////////////////////////////////////////////////////////////////////
// Name       : AntiLag_FreeHistory
// Description: Frees a history structure back into the history pool
////////////////////////////////////////////////////////////////////////////////
void AntiLag_FreeHistory ( ghistory_t* history )
{
	// Return the node to the head of the history pool
	history->next = g_historypoolHead;
	history->prev = NULL;

	if ( g_historypoolHead )
		g_historypoolHead->prev = history;

	g_historypoolHead = history;
}

////////////////////////////////////////////////////////////////////////////////
// Name       : AntiLag_UpdateClientHistory
// Description: Updates the history information for the given client.  This
//				basically removes all client history information older than
//				one second and adds a new one for the current server frame.
////////////////////////////////////////////////////////////////////////////////
void AntiLag_UpdateClientHistory ( gentity_t* ent )
{
	ghistory_t* history;

	// First remove all history items from the client that
	// are older than 1 second
	for ( history = ent->client->historyTail; history;  )
	{
		ghistory_t* free;

		// If this piece of history data isnt a second or more
		// old then nothing else before it can be either
		if ( level.time - history->time < 1000 )
			break;

		free = history;
		history = history->prev;

		// Regress the history tail a bit to make up
		// for the history data we are about to free
		ent->client->historyTail = history;

		// Fix up the pointers since we just freed one
		if ( NULL == ent->client->historyTail )
			ent->client->historyHead = NULL;
		else
			ent->client->historyTail->next = NULL;

		// Add the history data back to the history pool
		AntiLag_FreeHistory ( free );
	}

	// Dont bother putting two entries in for the same frame
	if ( ent->client->historyHead && level.time == ent->client->historyHead->time )
		return;

	// Allcoate a new history item
	if ( NULL == (history = AntiLag_AllocHistory ( ) ) )
		return;

	// Add the item to the history list.
	if ( ent->client->historyHead == NULL )
	{
		ent->client->historyHead = history;
		ent->client->historyTail = history;
	}
	else
	{
		history->next = ent->client->historyHead;
		ent->client->historyHead->prev = history;
		ent->client->historyHead = history;
	}

	// Copy the clients current information into the newly allocated
	// history structure
	history->time = level.time;
	history->leganim = ent->s.legsAnim;
	history->torsoanim = ent->s.torsoAnim;
	VectorCopy ( ent->r.currentOrigin, history->rOrigin );
	VectorCopy ( ent->r.currentAngles, history->rAngles );
	VectorCopy ( ent->s.pos.trBase, history->sOrigin );
	VectorCopy ( ent->s.apos.trBase, history->sAngles );
	VectorCopy ( ent->r.absmin, history->absmin );
	VectorCopy ( ent->r.absmax, history->absmax );
}

////////////////////////////////////////////////////////////////////////////////
// Name       : AntiLag_ValidateTrace
// Description: Ensures that a trace done in the past is still valid in the
//				future.  Basically this just resets everyone back to real time
//				and runs a trace from the shooter to the point at which the
//				original trace hit.  This essentially just helps prevent shots
//				where a low ping player has hidden behind a wall and then gets
//				shot 500ms-1s later due to the anti-lag code.
////////////////////////////////////////////////////////////////////////////////
qboolean AntiLag_ValidateTrace ( gentity_t* ent, gentity_t* other, vec3_t endpos )
{
	gentity_t*	unlinkedEntities[MAX_BREAKABLE_HITS];
	int			unlinked = 0;
	vec3_t		start;
	vec3_t		end;
	vec3_t		hit;
	qboolean	result = qfalse;
	int			i;
	float		distSquared;

	// If the client has antilag turned off or this client is a bot then
	// there is no need to validate the trace because they arent using antilag
	if ( !ent->client->pers.antiLag || (ent->r.svFlags & SVF_BOT) )
		return qtrue;

	if ( !other->client->historyUndo )
		return qtrue;

	// Calculate the endpoint of the trace by using the hit location
	// relative to the players origin
	VectorSubtract ( endpos, other->s.pos.trBase, hit );
	VectorCopy ( other->client->historyUndo->sOrigin, endpos );
	VectorAdd ( endpos, hit, endpos );

	// Have they moved a enough to warrent this check?  If not then its a hit
	distSquared = DistanceSquared ( other->client->historyUndo->sOrigin, other->s.pos.trBase );
	if ( distSquared < g_bulletPredictionThreshold.integer * g_bulletPredictionThreshold.integer )
		return qtrue;

	// Take off the history of the player so we can test against its current position
	AntiLag_SetReferenceClient ( NULL );

	// retrace to the point that was hit from the two players
	VectorCopy ( ent->s.pos.trBase, start );
	VectorCopy ( other->s.pos.trBase, end );
	VectorAdd ( end, hit, end );

	// Make sure the trace starts at the players viewheight.
	start[2] += ent->client->ps.viewheight;

	// Loop until something is hit that the bullet cant pass through
	while ( unlinked < MAX_BREAKABLE_HITS )
	{
		trace_t tr;
		trap_Trace ( &tr, start, NULL, NULL, end, ent->s.number, MASK_SHOT );

		// If the trace was able to hit the client or hit nothing then
		// the original trace remains valid.
		if ( tr.entityNum == ENTITYNUM_NONE || tr.entityNum == other->s.clientNum )
		{
			result = qtrue;
			break;
		}

		// If this is a breakable, then skip it for now
//		if ( Q_stricmp(g_entities[tr.entityNum].classname, "func_breakable") ==0 )
		if ( Q_stricmp(g_entities[tr.entityNum].classhash == HSH_func_breakable )
		{
			trap_UnlinkEntity( &g_entities[tr.entityNum] );
			unlinkedEntities[unlinked] = &g_entities[tr.entityNum];
			unlinked++;
			continue;
		}

		tr.entityNum = 0;

		break;
	}

	// link back in any entities we unlinked
	for ( i = 0 ; i < unlinked ; i++ )
		trap_LinkEntity( unlinkedEntities[i] );

	// Return qtrue if the trace is valid and qfalse if not
	return result;
}

////////////////////////////////////////////////////////////////////////////////
// Name       : AntiLag_SetClientTimeframe
// Description: Set the clients position, animation, and rotation to that
//				which it was at the given timeframe.  This function is used
//				to turn the clock back for a given client.  If the current
//				server time or greater is given the client will be brought
//				back to realtime.
////////////////////////////////////////////////////////////////////////////////
void AntiLag_SetClientTimeframe ( gentity_t* ent, int time )
{
	float		lerp;
	ghistory_t*	historyBefore = NULL;
	ghistory_t*	historyAfter  = NULL;

	// If the time is in the future or equal to the current time
	// then use the undo information if available
	if ( time >= level.time )
	{
		// If the client isnt already in the past then
		// dont bother doing anything
		if ( !ent->client->historyUndo )
			return;

		// Move the client back into reality by moving over the undo information
		ent->s.legsAnim = ent->client->historyUndo->leganim;
		ent->s.torsoAnim = ent->client->historyUndo->torsoanim;
		VectorCopy ( ent->client->historyUndo->rOrigin, ent->r.currentOrigin );
		VectorCopy ( ent->client->historyUndo->sOrigin, ent->s.pos.trBase );
		VectorCopy ( ent->client->historyUndo->sAngles, ent->s.apos.trBase );
		VectorCopy ( ent->client->historyUndo->rAngles, ent->r.currentAngles );
		VectorCopy ( ent->client->historyUndo->absmin, ent->r.absmin );
		VectorCopy ( ent->client->historyUndo->absmax, ent->r.absmax );

		// Free the undo information
		AntiLag_FreeHistory ( ent->client->historyUndo );
		ent->client->historyUndo = NULL;

		return;
	}

	// Find the history data
	for ( historyBefore = ent->client->historyHead; historyBefore; historyBefore = historyBefore->next )
	{
		if ( historyBefore->next && time <= historyBefore->next->time )
			continue;

		historyAfter = historyBefore->next;
		break;
	}

	// Since we didnt find any history information to use we should
	// just keep this client in real time
	if ( historyBefore == NULL )
		return;

	// If we were at the end of the history list then just use
	// it as the values.
	if ( historyAfter == NULL )
		historyAfter = historyBefore;

	// Allocate a history slot for the undo information
	if ( NULL == (ent->client->historyUndo = AntiLag_AllocHistory ( ) ) )
		return;

	// Save the undo information
	ent->client->historyUndo->leganim = ent->s.legsAnim;
	ent->client->historyUndo->torsoanim = ent->s.torsoAnim;
	VectorCopy ( ent->r.currentOrigin, ent->client->historyUndo->rOrigin );
	VectorCopy ( ent->r.currentAngles, ent->client->historyUndo->rAngles );
	VectorCopy ( ent->r.absmin, ent->client->historyUndo->absmin );
	VectorCopy ( ent->r.absmax, ent->client->historyUndo->absmax );
	VectorCopy ( ent->s.pos.trBase, ent->client->historyUndo->sOrigin );
	VectorCopy ( ent->s.apos.trBase, ent->client->historyUndo->sAngles );

	// Copy over the animation information first
	ent->s.legsAnim = historyBefore->leganim;
	ent->s.torsoAnim = historyBefore->torsoanim;

	// Calculate the lerp value to use for the vectors
	lerp = (float)(time - historyAfter->time) / (float)(historyBefore->time - historyAfter->time);

	// Lerp all the vectors between the before and after history information
	LerpVector ( historyAfter->sOrigin, historyBefore->sOrigin, lerp, ent->s.pos.trBase );
	LerpVector ( historyAfter->sAngles, historyBefore->sAngles, lerp, ent->s.apos.trBase );
	LerpVector ( historyAfter->rOrigin, historyBefore->rOrigin, lerp, ent->r.currentOrigin );
	LerpVector ( historyAfter->rAngles, historyBefore->rAngles, lerp, ent->r.currentAngles );
	LerpVector ( historyAfter->absmax, historyBefore->absmax, lerp, ent->r.absmax );
	LerpVector ( historyAfter->absmin, historyBefore->absmin, lerp, ent->r.absmin );
}

////////////////////////////////////////////////////////////////////////////////
// Name       : AntiLag_SetReferenceClient
// Description: Sets the current timeframe which all clients currently exist.
//				The ping of the given reference client is used to determine
//				what its time frame is.  Passing a NULL reference client will
//				bring all clients back to real time.
////////////////////////////////////////////////////////////////////////////////
void AntiLag_SetReferenceClient ( gentity_t* ref )
{
	int i;
	int reftime;

	// If NULL was given then undo all history information
	if ( ref == NULL )
	{
		// Undo all history
		for ( i = 0; i < level.numConnectedClients; i ++ )
		{
			gentity_t* other = &g_entities[level.sortedClients[i]];

			// Undo the history information for this client.  If for some
			// reason the client is no longer in the past the function will
			// just return without actually doing anything.
			AntiLag_SetClientTimeframe ( other, level.time );
		}

		return;
	}

	// Bots and clients with anti-lag disabled dont actually change the
	// reference time.
	if ( (ref->r.svFlags & SVF_BOT) || !ref->client->pers.antiLag )
		return;

	// Figure out the reference time based on the reference clients ping
	reftime = level.time - ref->client->ps.ping;

	// Move all the clients back into the reference clients time frame.
	for ( i = 0; i < level.numConnectedClients; i ++ )
	{
		gentity_t* other = &g_entities[level.sortedClients[i]];

		if ( other == ref )
			continue;

		// Skip clients that are spectating
		if ( other->client->sess.sessionTeam == TEAM_SPECTATOR )
			continue;

		// Apply the history information based off the reference
		// entities lag.
		AntiLag_SetClientTimeframe ( other, reftime );
	}
}
*/
void G_SendBBOfAllClients( gentity_t *skip )
{
	int	   i;
	gentity_t  *ent;

	ent = &g_entities[0];

	for(i = 0; i < MAX_CLIENTS; i++, ent++)
	{
		if(ent->client && ent->inuse && (ent->client->sess.sessionTeam < TEAM_SPECTATOR) && (ent != skip))
		{
			gentity_t  *corpse = G_Spawn();

			if (!corpse)
				return;

			corpse->classname    = "player_bb";
			corpse->classhash    = HSH_player_bb;
			corpse->nextthink    = level.time + 3000;
			corpse->timestamp    = level.time;
			corpse->think	     = G_FreeEntity;
			corpse->clipmask     = 0;
			corpse->r.contents   = 0;
			corpse->s.eType      = ET_PLAYERBB;
			corpse->r.ownerNum   = ent->s.number;
			corpse->parent	     = ent;
			corpse->s.otherEntityNum = ent->client->animationdata.legs.frame;
			corpse->s.otherEntityNum2 = ent->client->animationdata.torso.frame;
			corpse->s.frame = ent->client->AttackTime - ent->client->ps.commandTime;
			corpse->s.eventParm = ent->client->pers.ariesModel;
			corpse->s.pos.trType = TR_STATIONARY;
			corpse->s.pos.trTime = level.time;
			G_SetOrigin(corpse, ent->s.pos.trBase);
			VectorCopy(ent->s.pos.trDelta, corpse->s.pos.trDelta);
			VectorCopy(ent->s.apos.trBase, corpse->s.apos.trBase);
			VectorCopy(ent->s.pos.trBase, corpse->r.currentOrigin);
			corpse->r.svFlags     |= SVF_SINGLECLIENT;
			corpse->r.singleClient = skip->s.number;

			trap_LinkEntity(corpse);
		}
	}
}
