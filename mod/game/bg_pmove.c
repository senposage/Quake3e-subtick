// Copyright (C) 1999-2000 Id Software, Inc.
//
// bg_pmove.c -- both games player movement code
// takes a playerstate and a usercmd as input and returns a modifed playerstate

#include "q_shared.h"
#include "bg_public.h"
#include "bg_local.h"

#ifdef CGAME
extern vmCvar_t cg_walljumps;
extern vmCvar_t cg_stamina;
#elif  GAME
extern vmCvar_t g_walljumps;
extern vmCvar_t g_stamina;
#endif

#define PMOVE_HEIGHT_STAND	45
#define PMOVE_HEIGHT_CROUCH 24
#define PMOVE_HEIGHT_SWIM	10

pmove_t  *pm;
pml_t	 pml;

// movement parameters
float  pm_stopspeed 			= 100.0f;
float  pm_duckScale 			= 0.45f;
float  pm_limpScale 			= 0.80f;
float  pm_swimScale 			= 0.50f;
float  pm_wadeScale 			= 0.70f;
float  pm_ladderScale			= 0.50f;
//float pm_tiredScale			= 0.60f;//0.75
float  pm_tiredScale			= 0.75f; //0.75

float  pm_accelerate			= 5.0f;
//float pm_accelerate			= 10.0f;
float  pm_airaccelerate 		= 1.0f;
float  pm_wateraccelerate		= 4.0f;
float  pm_flyaccelerate 		= 8.0f;
float  pm_ladderaccelerate		= 3000.0f;

//float pm_friction 			= 6.0f;
float  pm_friction				= 4.0f;
float  pm_waterfriction 		= 1.0f;
float  pm_flightfriction		= 6.0f;
float  pm_spectatorfriction 	= 5.0f;
float  pm_ladderfriction		= 3000.0f;

float  pm_climbAccelerate		= 10.0f;

int    c_pmove					= 0;

void		PM_CheckLadder	( qboolean );
qboolean	PM_CheckLedge	( void );
static void PM_LadderMove	( void );
static void PM_LedgeMove	( void );

/*
===============
PM_AddEvent

===============
*/
void PM_AddEvent( int newEvent )
{
	BG_AddPredictableEventToPlayerstate( newEvent, 0, pm->ps );
}

/*
===============
PM_AddTouchEnt
===============
*/
void PM_AddTouchEnt( int entityNum )
{
	int  i;

	if (entityNum == ENTITYNUM_WORLD)
	{
		return;
	}

	if (pm->numtouch == MAXTOUCH)
	{
		return;
	}

	// see if it is already added
	for (i = 0 ; i < pm->numtouch ; i++)
	{
		if (pm->touchents[i] == entityNum)
		{
			return;
		}
	}

	// add it
	pm->touchents[pm->numtouch] = entityNum;
	pm->numtouch++;
}

/*
===================
PM_StartTorsoAnim
===================
*/
static void PM_StartTorsoAnim( int anim )
{
	if (pm->ps->pm_type >= PM_DEAD)
		return;
	if (pm->ps->torsoTimer > 0)
		return; 	// a high priority animation is running
	pm->ps->torsoAnim = ((pm->ps->torsoAnim & ANIM_TOGGLEBIT) ^ ANIM_TOGGLEBIT)
				| anim;
}

/**
 * $(function)
 */
static void PM_StartLegsAnim( int anim )
{
	if (pm->ps->legsAnim == anim)
		return;
	if (pm->ps->pm_type >= PM_DEAD)
		return;
	if (pm->ps->legsTimer > 0)
		return; 	// a high priority animation is running
	pm->ps->legsAnim = ((pm->ps->legsAnim & ANIM_TOGGLEBIT) ^ ANIM_TOGGLEBIT)
			   | anim;
}

/**
 * $(function)
 */
static void PM_ContinueLegsAnim( int anim )
{
	if (pm->ps->legsAnim == anim)
		return;
	if ((pm->ps->legsAnim & ~ANIM_TOGGLEBIT) == anim)
		return;
	if (pm->ps->legsTimer > 0)
		return; 	// a high priority animation is running
	PM_StartLegsAnim( anim );
}

/**
 * $(function)
 */
static void PM_ContinueTorsoAnim( int anim )
{
	if (pm->ps->torsoAnim == anim)
		return;
	if ((pm->ps->torsoAnim & ~ANIM_TOGGLEBIT) == anim)
		return;
	if (pm->ps->torsoTimer > 0)
		return; 	// a high priority animation is running
	PM_StartTorsoAnim( anim );
}

/**
 * $(function)
 */
static void PM_ForceLegsAnim( int anim )
{
	if (pm->ps->legsAnim == anim)
		return;

	pm->ps->legsTimer = 0;
	PM_StartLegsAnim( anim );
	//pm->ps->legsTimer = g_athenaAnims[anim].totalTime;
}

// added by Dokta8
static void PM_ForceTorsoAnim( int anim )
{
	/*if (pm->ps->torsoAnim == anim)*/
		/*return;*/
	pm->ps->torsoTimer = 0;
	PM_StartTorsoAnim( anim );
	pm->ps->torsoTimer = g_playerAnims[pm->ariesModel][anim].totalTime;
}

/*
==================
PM_ClipVelocity

Slide off of the impacting surface
==================
*/
/*
void PM_ClipVelocity( vec3_t in, vec3_t normal, vec3_t out, float overbounce )
{
	float  backoff;
	float  change;
	int    i;

	backoff = DotProduct (in, normal);

	if (backoff < 0)
	{
		backoff *= overbounce;
	}
	else
	{
		backoff /= overbounce;
	}

	for (i = 0 ; i < 3 ; i++)
	{
		change = normal[i] * backoff;
		out[i] = in[i] - change;
	}
}
*/
//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : PM_Stamina
// Description: Increases or decreases stamina based on movement
// Author	  : Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
static void PM_Stamina ( float vel ) {

	float	  f;
	int   	  i;
	qboolean  haskev;
	qboolean  threshold;
	//char	  info[MAX_NAME_LENGTH];
	qboolean  nostamina;

// @Fenix
#ifdef CGAME
	nostamina = (cg_stamina.integer >= 2) ? qtrue : qfalse;
#elif GAME
	nostamina = (g_stamina.integer >= 2) ? qtrue : qfalse;
#endif

	if(nostamina) {
		//@Barbatos 4.2.004 - be sure that the stamina is always at 100%
		pm->ps->stats[STAT_STAMINA] = pm->ps->stats[STAT_HEALTH] * UT_STAM_MUL;
		return;
	}

	if(pm->ps->stats[STAT_STAMINA] < 0) {
		pm->ps->stats[STAT_STAMINA] = 0;
	}

	//@Barbatos: when paused don't increase or decrease stamina
	if(pm->paused) {
		return;
	}

	threshold = ( ( pm->cmd.buttons & BUTTON_SPRINT ) && pm->cmd.rightmove == 0 && pm->cmd.forwardmove > 80 );
	if ( threshold && !( pm->ps->pm_flags & ( PMF_DUCKED | PMF_FORCE_DUCKED ) ) && ( vel > UT_MAX_RUNSPEED + 1 ) )
	{
		// Cap it at the sprint speed
		if ( vel > UT_MAX_SPRINTSPEED ) {
			vel = UT_MAX_SPRINTSPEED;
		}

		vel -= UT_MAX_RUNSPEED;

		f	 = vel / ( (float)UT_MAX_SPRINTSPEED - (float)UT_MAX_RUNSPEED * 1.05f );
		f	*= 0.25f * ( UT_MAX_HEALTH / 1000.0f ) * UT_STAM_MUL;
		f	*= pml.msec;

		// See if they have kevlar, if not give em a nice 50% bonus
		haskev = qfalse;

		for (i = 0; i < UT_MAX_ITEMS; i++)
		{
			if (UT_ITEM_GETID(pm->ps->itemData[i]) == UT_ITEM_VEST)
			{
				haskev = qtrue;
				break;
			}
		}

		if ( !haskev ) {
			f *= 0.5f;
		} else {
			f *= 0.9f;
		}

		pm->ps->stats[STAT_STAMINA] -= f;

		if ( pm->ps->stats[STAT_STAMINA] < 0 ) {
			pm->ps->stats[STAT_STAMINA] = 0;
		}
	}
	else
	{
		f  = 1.0f - ( vel / ( (float)UT_MAX_RUNSPEED * 1.2f ) );
		f *= 0.20f * ( UT_MAX_HEALTH / 1000.0f ) * UT_STAM_MUL;
		f *= pml.msec;

		if ( threshold && vel > 105.0f ) {
		// we are below stamina-draining speeds, but going fast enough to be sliding still,
		// and the player has the sprint and forward buttons still pressed

		// NOTE: the old code used to do something progressive here to reduce the stamina according to the speed
		// but testing showed that removing a flat amount of stamina per tick was just as good,
		// provided we're above turtling speed alread (that's roughly 100)

		f = -UT_STAM_SLIDE_DRAIN * ( (float)pml.msec / 8.0f );
		}

		pm->ps->stats[STAT_STAMINA] += f;

		if ( pm->ps->stats[STAT_STAMINA] > pm->ps->stats[STAT_HEALTH] * UT_STAM_MUL ) {
			pm->ps->stats[STAT_STAMINA] = pm->ps->stats[STAT_HEALTH] * UT_STAM_MUL;
	}
	}

	if (vel == 0)
	{
		pm->ps->stats[STAT_MOVEMENT_INACCURACY] = 0;
	}
	else
	{
		pm->ps->stats[STAT_MOVEMENT_INACCURACY] += pml.msec;

		if ( pm->ps->stats[STAT_MOVEMENT_INACCURACY] > UT_MAX_MOVEMENT_INACCURACY ) {
			pm->ps->stats[STAT_MOVEMENT_INACCURACY] = UT_MAX_MOVEMENT_INACCURACY;
		}
	}
}

/*
==================
PM_Friction

Handles both ground friction and water friction
==================
*/
static void PM_Friction( void )
{
	vec3_t	vec;
	float	*vel;
	float	speed, newspeed, control;
	float	drop;

	vel = pm->ps->velocity;

	VectorCopy( vel, vec );

	if (pml.walking)
	{
		vec[2] = 0; // ignore slope movement
	}

	speed = VectorLength(vec);

	if (speed < 1)
	{
		vel[0] = 0;
		vel[1] = 0; 	// allow sinking underwater
		// FIXME: still have z friction underwater?
		return;
	}

	drop = 0;

	// apply ground friction
	if (pm->waterlevel <= 1)
	{
		if (pml.walking && !(pml.groundTrace.surfaceFlags & SURF_SLICK))
		{
			// if getting knocked back, no friction
			if (!(pm->ps->pm_flags & PMF_TIME_KNOCKBACK))
			{
				control = speed < pm_stopspeed ? pm_stopspeed : speed;
				drop   += control * pm_friction * pml.frametime;
			}
		}
	}

	// apply water friction even if just wading
	if (pm->waterlevel)
	{
		drop += speed * pm_waterfriction * pm->waterlevel * pml.frametime;
	}

	if (pm->ps->pm_flags & PMF_LADDER)	 // If they're on a ladder...
	{
		drop += speed * pm_ladderfriction * pml.frametime;	// Add ladder friction!
	}

	if ((pm->ps->pm_type == PM_SPECTATOR) || (pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_GHOST))
	{
		drop += speed * pm_spectatorfriction * pml.frametime;
	}

	if (pm->ps->eFlags & EF_POWERSLIDE)
	{
		drop = 0.15f * ((float)pml.frametime * 1000.0f);
		//	Com_Printf("Drop %f\n",drop);
	}

	// scale the velocity
	newspeed = speed - drop;

	if (newspeed < 0)
	{
		newspeed = 0;
	}
	newspeed /= speed;

	vel[0]	  = vel[0] * newspeed;
	vel[1]	  = vel[1] * newspeed;
	vel[2]	  = vel[2] * newspeed;
}

/*
==============
PM_Accelerate

Handles user intended acceleration
==============
*/
static void PM_Accelerate( vec3_t wishdir, float wishspeed, float accel )
{
#if 1
	// q2 style
	int    i;
	float  addspeed, accelspeed, currentspeed;

	currentspeed = DotProduct (pm->ps->velocity, wishdir);
	addspeed	 = wishspeed - currentspeed;

	if (addspeed <= 0)
	{
		return;
	}
	accelspeed = accel * pml.frametime * wishspeed;

	if (accelspeed > addspeed)
	{
		accelspeed = addspeed;
	}

	for (i = 0 ; i < 3 ; i++)
	{
		pm->ps->velocity[i] += accelspeed * wishdir[i];
	}
#else
	// proper way (avoids strafe jump maxspeed bug), but feels bad
	vec3_t	wishVelocity;
	vec3_t	pushDir;
	float	pushLen;
	float	canPush;

	VectorScale( wishdir, wishspeed, wishVelocity );
	VectorSubtract( wishVelocity, pm->ps->velocity, pushDir );
	VectorNormalize( pushDir, pushLen );

	canPush = accel * pml.frametime * wishspeed;

	if (canPush > pushLen)
	{
		canPush = pushLen;
	}

	VectorMA( pm->ps->velocity, canPush, pushDir, pm->ps->velocity );
#endif
}

/*
============
PM_CmdScale

Returns the scale factor to apply to cmd movements
This allows the clients to use axial -127 to 127 values for all directions
without getting a sqrt(2) distortion in speed.
============
*/
static float PM_CmdScale( usercmd_t *cmd )
{
	int    max;
	float  total;
	float  scale;
	float  speed;

	speed = (float)UT_MAX_RUNSPEED;

	// Check for sprint
	if ((cmd->buttons & BUTTON_SPRINT) &&
		(cmd->rightmove == 0) &&
		(cmd->forwardmove > 80) && (pm->ps->stats[STAT_STAMINA] > 10)
		)
	{
		speed = UT_MAX_SPRINTSPEED;
	}

	max = abs( cmd->forwardmove );

	if (abs( cmd->rightmove ) > max)
	{
		max = abs( cmd->rightmove );
	}

	if (abs( cmd->upmove ) > max)
	{
		max = abs( cmd->upmove );
	}

	if (!max)
	{
		return 0;
	}

	total = sqrt( cmd->forwardmove * cmd->forwardmove
			  + cmd->rightmove * cmd->rightmove + cmd->upmove * cmd->upmove );
	scale = speed * max / (127.0 * total);

	return scale;
}

/*
================
PM_SetMovementDir

Determine the rotation of the legs reletive
to the facing dir
================
*/
static void PM_SetMovementDir( void )
{
	if (pm->cmd.forwardmove || pm->cmd.rightmove)
	{
		if ((pm->cmd.rightmove == 0) && (pm->cmd.forwardmove > 0))
		{
			pm->ps->movementDir = 0;
		}
		else if (pm->cmd.rightmove < 0 && pm->cmd.forwardmove > 0)
		{
			pm->ps->movementDir = 1;
		}
		else if (pm->cmd.rightmove < 0 && pm->cmd.forwardmove == 0)
		{
			pm->ps->movementDir = 2;
		}
		else if (pm->cmd.rightmove < 0 && pm->cmd.forwardmove < 0)
		{
			pm->ps->movementDir = 3;
		}
		else if (pm->cmd.rightmove == 0 && pm->cmd.forwardmove < 0)
		{
			pm->ps->movementDir = 4;
		}
		else if (pm->cmd.rightmove > 0 && pm->cmd.forwardmove < 0)
		{
			pm->ps->movementDir = 5;
		}
		else if (pm->cmd.rightmove > 0 && pm->cmd.forwardmove == 0)
		{
			pm->ps->movementDir = 6;
		}
		else if (pm->cmd.rightmove > 0 && pm->cmd.forwardmove > 0)
		{
			pm->ps->movementDir = 7;
		}
	}
	else
	{
		// if they aren't actively going directly sideways,
		// change the animation to the diagonal so they
		// don't stop too crooked
		if (pm->ps->movementDir == 2)
		{
			pm->ps->movementDir = 1;
		}
		else if (pm->ps->movementDir == 6)
		{
			pm->ps->movementDir = 7;
		}
	}
}

/*
=============
PM_CheckJump
=============
*/
static qboolean PM_CheckJump( void ) {

	int   		delta, i;
	qboolean  	haskev = qfalse;
	qboolean    nostamina;

	if (pm->ps->pm_flags & PMF_RESPAWNED) {
		return qfalse;		// don't allow jump until all buttons are up
	}

	/*
	// Cant jump so high if limping or out of stamina
	if ( (pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_LIMPING) ||
		 (pm->ps->stats[STAT_STAMINA] < (0.05f * (float)UT_MAX_HEALTH * (float)UT_STAM_MUL)))
		return qfalse;
   */

	if (pm->cmd.upmove < 10) {
		// not holding jump
		return qfalse;
	}

	// must wait for jump to be released
	if (pm->ps->pm_flags & PMF_JUMP_HELD) {
		// clear upmove so cmdscale doesn't lower running speed
		pm->cmd.upmove = 0;
		return qfalse;
	}

	pml.groundPlane 	 = qfalse;			// jumping away
	pml.walking 	 	 = qfalse;
	pm->ps->pm_flags    |= PMF_JUMP_HELD;

	pm->ps->groundEntityNum = ENTITYNUM_NONE;

	if (pm->ps->pm_flags & PMF_LADDER) {
		vec3_t	forward;
		VectorCopy ( pml.forward, forward );
		forward[2] = 0;
		VectorNormalizeNR ( forward );
		VectorMA ( pm->ps->velocity, -50, forward, pm->ps->velocity );
		pm->ps->pm_flags |= PMF_LADDER_IGNORE;
		return qtrue;
	}

	// Cant jump so high if limping or out of stamina
	if ((pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_LIMPING) ||
		(pm->ps->stats[STAT_STAMINA] < (0.05f * (float)UT_MAX_HEALTH * (float)UT_STAM_MUL))) {
		VectorScale ( pm->ps->velocity, 0.50f, pm->ps->velocity );
		pm->ps->velocity[2] = JUMP_VELOCITY * 0.75f;
	} else {
		pm->ps->velocity[2] = JUMP_VELOCITY;
	}

	PM_AddEvent( EV_JUMP );

	if (pm->ps->pm_flags & PMF_OTHERLEG) {
		pm->ps->pm_flags &= ~PMF_OTHERLEG;
		PM_ForceLegsAnim( pm->cmd.forwardmove >= 0 ? LEGS_JUMP : LEGS_BACKJUMP );
	}
	else {
		pm->ps->pm_flags |= PMF_OTHERLEG;
		PM_ForceLegsAnim( pm->cmd.forwardmove >= 0 ? LEGS_JUMPRT : LEGS_BACKJUMPRT );
	}

	delta = (int)(0.12f * UT_MAX_HEALTH * UT_STAM_MUL);

	/*for (i = 0; i < UT_MAX_WEAPONS; i ++) {
		if (UT_WEAPON_GETID(pm->ps->weaponData[i]) == UT_WP_NEGEV) {
			delta *= 3;
			break;
		}
	}*/

	//See if they have kevlar, if not give em a nice 25% bonus
	haskev = qfalse;

	for (i = 0; i < UT_MAX_ITEMS; i++) {
		if (UT_ITEM_GETID(pm->ps->itemData[i]) == UT_ITEM_VEST) {
			haskev = qtrue;
			break;
		}
	}

	if (!haskev) {
		delta *= 0.5f;
	}

    #ifdef CGAME
	nostamina = (cg_stamina.integer >= 2) ? qtrue : qfalse;
    #elif GAME
	nostamina = (g_stamina.integer >= 2) ? qtrue : qfalse;
    #endif

	// Fenix: changed stamina CVAR
	// Will be defaulted to 0 in all the gametypes but jump mode
	// We'll retrieve the correct value depending on the vm subsystem
	if(!nostamina) {
		// Decrement stamina
		pm->ps->stats[STAT_STAMINA] -= delta;
		if(pm->ps->stats[STAT_STAMINA] < 0) {
			pm->ps->stats[STAT_STAMINA] = 0;
		}
	}

	return qtrue;
}

/*
=============
PM_CheckWaterJump
=============
*/
static qboolean PM_CheckWaterJump( void ) {

	vec3_t	spot;
	int cont;
	vec3_t	flatforward;

	if (pm->ps->pm_time) {
		return qfalse;
	}

	// check for water jump
	if (pm->waterlevel != 2) {
		return qfalse;
	}

	flatforward[0] = pml.forward[0];
	flatforward[1] = pml.forward[1];
	flatforward[2] = 0;
	VectorNormalizeNR (flatforward);

	VectorMA (pm->ps->origin, 30, flatforward, spot);
	spot[2] += 4;
	cont	 = pm->pointcontents (spot, pm->ps->clientNum );

	if (!(cont & CONTENTS_SOLID)) {
		return qfalse;
	}

	spot[2] += 16;
	cont	 = pm->pointcontents (spot, pm->ps->clientNum );

	if (cont) {
		return qfalse;
	}

	// jump out of water
	VectorScale (pml.forward, 200, pm->ps->velocity);
	pm->ps->velocity[2] = 350;

	pm->ps->pm_flags   |= PMF_TIME_WATERJUMP;
	pm->ps->pm_time 	= 2000;

	return qtrue;
}

//============================================================================

/*
===================
PM_WaterJumpMove

Flying out of the water
===================
*/
static void PM_WaterJumpMove( void ) {

	// waterjump has no control, but falls

	pm->ps->pm_flags &= (~PMF_WALL);
	PM_StepSlideMove( qtrue );

	pm->ps->velocity[2] -= pm->ps->gravity * pml.frametime;

	if (pm->ps->velocity[2] < 0) {
		// cancel as soon as we are falling down again
		pm->ps->pm_flags &= ~PMF_ALL_TIMES;
		pm->ps->pm_time   = 0;
	}
}

/*
===================
PM_WaterMove

===================
*/
static void PM_WaterMove( void )
{
	int i;
	vec3_t	wishvel;
	float	wishspeed;
	vec3_t	wishdir;
	float	scale;
	float	vel;

	if (PM_CheckWaterJump())
	{
		PM_WaterJumpMove();
		return;
	}
#if 0
	// jump = head for surface
	if (pm->cmd.upmove >= 10)
	{
		if (pm->ps->velocity[2] > -300)
		{
			if (pm->watertype == CONTENTS_WATER)
			{
				pm->ps->velocity[2] = 100;
			}
			else if (pm->watertype == CONTENTS_SLIME)
			{
				pm->ps->velocity[2] = 80;
			}
			else
			{
				pm->ps->velocity[2] = 50;
			}
		}
	}
#endif
	PM_Friction ();

	scale = PM_CmdScale( &pm->cmd );

	//
	// user intentions
	//
	if (!scale)
	{
		wishvel[0] = 0;
		wishvel[1] = 0;
		wishvel[2] = -60;		// sink towards bottom
	}
	else
	{
		for (i = 0 ; i < 3 ; i++)
		{
			wishvel[i] = scale * pml.forward[i] * pm->cmd.forwardmove + scale * pml.right[i] * pm->cmd.rightmove;
		}

		wishvel[2] += scale * pm->cmd.upmove;
	}

	VectorCopy (wishvel, wishdir);
	VectorNormalize(wishdir, wishspeed);

	if (wishspeed > pm->ps->speed * pm_swimScale)
	{
		wishspeed = pm->ps->speed * pm_swimScale;
	}

	PM_Accelerate (wishdir, wishspeed, pm_wateraccelerate);

	// make sure we can go up slopes easily under water
	if (pml.groundPlane && (DotProduct( pm->ps->velocity, pml.groundTrace.plane.normal ) < 0))
	{
		vel = VectorLength(pm->ps->velocity);
		// slide along the ground plane
		PM_ClipVelocity (pm->ps->velocity, pml.groundTrace.plane.normal,
				 pm->ps->velocity, OVERCLIP );

		VectorNormalizeNR(pm->ps->velocity);
		VectorScale(pm->ps->velocity, vel, pm->ps->velocity);
	}

	if (PM_SlideMove( qfalse ))
	{
		pm->ps->pm_flags |= PMF_WALL;
	}
	else
	{
		pm->ps->pm_flags &= (~PMF_WALL);
	}
}

/*
===================
PM_FlyMove

Only with the flight powerup
===================
*/
static void PM_FlyMove( void )
{
	int i;
	vec3_t	wishvel;
	float	wishspeed;
	vec3_t	wishdir;
	float	scale;

	// normal slowdown
	PM_Friction ();

	scale = PM_CmdScale( &pm->cmd ) * 1.5f;

	//
	// user intentions
	//
	if (!scale)
	{
		wishvel[0] = 0;
		wishvel[1] = 0;
		wishvel[2] = 0;
	}
	else
	{
		for (i = 0 ; i < 3 ; i++)
		{
			wishvel[i] = scale * pml.forward[i] * pm->cmd.forwardmove + scale * pml.right[i] * pm->cmd.rightmove;
		}

		wishvel[2] += scale * pm->cmd.upmove;
	}

	VectorCopy (wishvel, wishdir);
	VectorNormalize(wishdir, wishspeed);

	PM_Accelerate (wishdir, wishspeed, pm_flyaccelerate);

	if ((fabs(pm->ps->velocity[0]) > 1.0f) || (fabs(pm->ps->velocity[1]) > 1.0f) || (fabs(pm->ps->velocity[2]) > 1.0f))
	{
		VectorMA (pm->ps->origin, pml.frametime, pm->ps->velocity, pm->ps->origin);
	}

//	PM_StepSlideMove( qfalse );
}

/*
===================
PM_AirMove

===================
*/
static void PM_AirMove( void ) {

	int    	   i;
	vec3_t	   wishvel;
	float	   fmove, smove;
	vec3_t	   wishdir;
	vec3_t	   dir;
	float	   wishspeed;
	float	   scale;
	usercmd_t  cmd;
	float	   scalee;
	trace_t    trace;
	vec3_t	   end;
	float	   temp;
	int maxwj;


	//Com_Printf("%i\n",pm->walljump);
	//pm->ps->pm_flags&= ~PMF_WALL_JUMP;
	//if (pm->ps->grapplePoint[0]>0 && pm->cmd.serverTime > pm->ps->grapplePoint[0] ) // + 250 if you want to delay sliding
	if (pm->ps->pm_flags & PMF_WALL_JUMP) {

		//if we're moving downwards and not too fast, its ok to walljump
		//Com_Printf("%f\n",pm->ps->velocity[2]);

		if ((pm->ps->velocity[2] < 0) && (pm->ps->velocity[2] > -450)) {

#ifdef CGAME
			maxwj = cg_walljumps.integer;
#elif GAME
			maxwj = g_walljumps.integer;
#endif

			//Fenix: changed from default (3) to server configuration value
			if (pm->ps->generic1 < maxwj) {
				//trace ahead on the x/y plane
				//if we hit space from waist up, dont jump
				dir[0]		= pm->ps->grapplePoint[1];
				dir[1]		= pm->ps->grapplePoint[2];
				dir[2]		= 0;

				end[0]		= pm->ps->origin[0] - (dir[0] * 5);
				end[1]		= pm->ps->origin[1] - (dir[1] * 5);
				end[2]		= pm->ps->origin[2];

				temp		= pm->mins[2];
				pm->mins[2] = 10;
				pm->trace(&trace, pm->ps->origin, pm->mins, pm->maxs, end, pm->ps->clientNum, pm->tracemask);
				pm->mins[2] = temp;

				if (trace.fraction < 1)
				{
					if (PM_CheckJump()) //sliding allows jumping
					{
						pm->ps->generic1++;

						//we jumped.
						if ((pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_LIMPING) ||
							(pm->ps->stats[STAT_STAMINA] < (0.05f * (float)UT_MAX_HEALTH * (float)UT_STAM_MUL)))
						{
							pm->ps->velocity[2] -= 100;
							scalee			 = 100;
						}
						else
						{
							scalee = 200;
						}
						pm->ps->velocity[0] += pm->ps->grapplePoint[1] * scalee;
						pm->ps->velocity[1] += pm->ps->grapplePoint[2] * scalee;

						//delta = (int)(0.05f * UT_MAX_HEALTH * UT_STAM_MUL);

						//pm->ps->stats[STAT_STAMINA] -= delta;
						//if(pm->ps->stats[STAT_STAMINA] < 0 )
						//	 pm->ps->stats[STAT_STAMINA] = 0;
						//BG_AddPredictableEventToPlayerstate( EV_FOOTSTEP, 3>>20, pm->ps );
						BG_AddPredictableEventToPlayerstate(ET_UT_WALLJUMP, DirToByte(dir), pm->ps );

						/*PM_ForceLegsAnim( BOTH_WALLJUMP );
						PM_ForceTorsoAnim( BOTH_WALLJUMP );*/

						//@Barbatos
						PM_ForceLegsAnim( LEGS_JUMP );
					}
				}
				else
				{
					pm->ps->generic1 = 10;
					//	  Com_Printf("rejected %f",pm->ps->velocity[2]);
				}
			}
		}

		///WALL SLIDING
/*	if (pm->ps->velocity[2]<-300)
	  {
	 if ( (pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_LIMPING) ||
		   (pm->ps->stats[STAT_STAMINA] < (0.05f * (float)UT_MAX_HEALTH * (float)UT_STAM_MUL)))
	 {

	 }
	 else
	 {
		pm->ps->pm_flags|= PMF_WALL_SLIDE;
		pm->ps->velocity[2]+=(pm->ps->gravity*0.90) *pml.frametime; //neg gravity
		pm->ps->velocity[0]-=pm->ps->velocity[0] *pml.frametime * 2.5;
		pm->ps->velocity[1]-=pm->ps->velocity[1] *pml.frametime * 2.5;


			old = pm->ps->bobCycle;
			bobmove = 0.4f;
			   pm->ps->bobCycle = (int)( old + bobmove * pml.msec ) & 255;

			// if we just crossed a cycle boundary, play an apropriate footstep event
			if ( ( ( old + 32 ) ^ ( pm->ps->bobCycle + 32 ) ) & 64 )
		{
				  BG_AddPredictableEventToPlayerstate( ET_UT_WALLSLIDE,0, pm->ps );

			delta = (int)(0.05f * UT_MAX_HEALTH * UT_STAM_MUL);

			pm->ps->stats[STAT_STAMINA] -= delta;
		if(pm->ps->stats[STAT_STAMINA] < 0 )
				pm->ps->stats[STAT_STAMINA] = 0;
		}
	 }
	  }*/
	}

	PM_Friction();

	fmove = pm->cmd.forwardmove;
	smove = pm->cmd.rightmove;

	cmd   = pm->cmd;
	scale = PM_CmdScale( &cmd );

	// set the movementDir so clients can rotate the legs for strafing
	//PM_SetMovementDir();

	// project moves down to flat plane
	pml.forward[2] = 0;
	pml.right[2]   = 0;
	VectorNormalizeNR (pml.forward);
	VectorNormalizeNR (pml.right);

	for (i = 0 ; i < 2 ; i++)
	{
		wishvel[i] = pml.forward[i] * fmove + pml.right[i] * smove;
	}
	wishvel[2] = 0;

	VectorCopy (wishvel, wishdir);
	VectorNormalize(wishdir, wishspeed);
	wishspeed *= scale;

	// not on ground, so little effect on velocity
	PM_Accelerate (wishdir, wishspeed, pm_airaccelerate);

	// we may have a ground plane that is very steep, even
	// though we don't have a groundentity
	// slide along the steep plane
	if (pml.groundPlane)
	{
		PM_ClipVelocity (pm->ps->velocity, pml.groundTrace.plane.normal,
				 pm->ps->velocity, OVERCLIP );
	}

/* URBAN TERROR - Needed the pmf_flag
#if 0
	//ZOID:  If we are on the grapple, try stair-stepping
	//this allows a player to use the grapple to pull himself
	//over a ledge
	if (pm->ps->pm_flags & PMF_GRAPPLE_PULL)
		PM_StepSlideMove ( qtrue );
	else
		PM_SlideMove ( qtrue );
#endif
*/
	pm->ps->pm_flags &= (~PMF_WALL);
	PM_StepSlideMove ( qtrue );
}

/*
===================
PM_WalkMove

===================
*/
static void PM_WalkMove( void )
{
	int    i;
	vec3_t	   wishvel;
	float	   fmove, smove;
	vec3_t	   wishdir;
	float	   wishspeed;
	float	   scale;
	usercmd_t  cmd;
	float	   accelerate;
	float	   vel;
	vec3_t	   original;

	if ((pm->waterlevel > 2) && (DotProduct( pml.forward, pml.groundTrace.plane.normal ) > 0))
	{
		// begin swimming
		PM_WaterMove();
		return;
	}

	if (PM_CheckJump ())
	{
		// jumped away
		if (pm->waterlevel > 1)
		{
			PM_WaterMove();
		}
		else
		{
			PM_AirMove();
		}
		return;
	}

	PM_Friction ();

	fmove = pm->cmd.forwardmove;
	smove = pm->cmd.rightmove;

	cmd   = pm->cmd;
	scale = PM_CmdScale( &cmd );

	// set the movementDir so clients can rotate the legs for strafing
	PM_SetMovementDir();

	// project moves down to flat plane
	pml.forward[2] = 0;
	pml.right[2]   = 0;

	// project the forward and right directions onto the ground plane
	PM_ClipVelocity (pml.forward, pml.groundTrace.plane.normal, pml.forward, OVERCLIP );
	PM_ClipVelocity (pml.right, pml.groundTrace.plane.normal, pml.right, OVERCLIP );
	//
	VectorNormalizeNR (pml.forward);
	VectorNormalizeNR (pml.right);

	for (i = 0 ; i < 3 ; i++)
	{
		wishvel[i] = pml.forward[i] * fmove + pml.right[i] * smove;
	}
	// when going up or down slopes the wish velocity should Not be zero
//	wishvel[2] = 0;

	VectorCopy (wishvel, wishdir);
	VectorNormalize(wishdir, wishspeed);
	wishspeed *= scale;

	// clamp the speed lower if ducking
	if (pm->ps->pm_flags & PMF_DUCKED)
	{
		wishspeed = pm->ps->speed * pm_duckScale;
	}

	if (pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_LIMPING)
	{
		if (wishspeed > pm->ps->speed * pm_limpScale)
		{
			wishspeed = pm->ps->speed * pm_limpScale;
		}
	}

	// Handle slowing down due to stamina
	if(pm->ps->stats[STAT_STAMINA] < UT_MAX_HEALTH * UT_STAM_MUL / 4)
	{
		float  tiredspeed = wishspeed * pm_tiredScale;
		float  speed;

		speed  = (float)pm->ps->stats[STAT_STAMINA] / (UT_MAX_HEALTH * UT_STAM_MUL / 4.0f);
		speed *= (wishspeed - tiredspeed);
		speed += tiredspeed;

		if(speed < UT_MAX_RUNSPEED * pm_tiredScale)
		{
			speed = UT_MAX_RUNSPEED * pm_tiredScale;
		}

		if (speed < wishspeed)
		{
			wishspeed = speed;
		}
	}

	// clamp the speed lower if wading or walking on the bottom
	if (pm->waterlevel)
	{
		float  waterScale;

		waterScale = pm->waterlevel / 3.0;
		waterScale = 1.0 - (1.0 - pm_swimScale) * waterScale;

		if (wishspeed > pm->ps->speed * waterScale)
		{
			wishspeed = pm->ps->speed * waterScale;
		}
	}

	// Dokta8: implement limping for leg shots.  Added a PMF_LIMPING to pm_flags
	if (pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_LIMPING)
	{
		if ((pm->cmd.serverTime % 500) < 250)	// slow player down every 1/4 sec
		{
			wishspeed = pm->ps->speed * 0.6;
		}
	}

	// when a player gets hit, they temporarily lose
	// full control, which allows them to be moved a bit
	if ((pml.groundTrace.surfaceFlags & SURF_SLICK) || (pm->ps->pm_flags & PMF_TIME_KNOCKBACK))
	{
		accelerate = pm_airaccelerate;
	}
	else
	{
		accelerate = pm_accelerate;
	}

	// They must be sprinting.	Accel slower
	if (wishspeed > pm->ps->speed)
	{
		accelerate = pm_friction + 0.05f;
	}

	PM_Accelerate (wishdir, wishspeed, accelerate);

	//Com_Printf("velocity = %1.1f %1.1f %1.1f\n", pm->ps->velocity[0], pm->ps->velocity[1], pm->ps->velocity[2]);
	//Com_Printf("velocity1 = %1.1f\n", VectorLength(pm->ps->velocity));

	if ((pml.groundTrace.surfaceFlags & SURF_SLICK) || (pm->ps->pm_flags & PMF_TIME_KNOCKBACK))
	{
		pm->ps->velocity[2] -= pm->ps->gravity * pml.frametime;
	}
	else
	{
		// don't reset the z velocity for slopes
//		pm->ps->velocity[2] = 0;
	}

//	vel = VectorLength(pm->ps->velocity);

	{
		//Overbounce bug right here folks

		vel = VectorLength(pm->ps->velocity);
		VectorCopy(pm->ps->velocity, original);

		// slide along the ground plane
		PM_ClipVelocity (pm->ps->velocity, pml.groundTrace.plane.normal,
				 pm->ps->velocity, OVERCLIP );

		//27 overbounce clipping
		if (DotProduct(original, pm->ps->velocity) < -0.5)
		{
			pm->ps->velocity[0] = 0;
			pm->ps->velocity[1] = 0;
			pm->ps->velocity[2] = 0;
		}

		// don't decrease velocity when going up or down a slope
		VectorNormalizeNR(pm->ps->velocity);
		VectorScale(pm->ps->velocity, vel, pm->ps->velocity);
	}

	// slide along the ground plane
	PM_ClipVelocity (pm->ps->velocity, pml.groundTrace.plane.normal,
			 pm->ps->velocity, OVERCLIP );

	// don't decrease velocity when going up or down a slope
	VectorNormalizeNR(pm->ps->velocity);
	VectorScale(pm->ps->velocity, vel, pm->ps->velocity);

	PM_Stamina( vel );

	// don't do anything if standing still
	if (!pm->ps->velocity[0] && !pm->ps->velocity[1])
	{
		pm->ps->pm_flags &= (~PMF_WALL);
		return;
	}

	pm->ps->pm_flags &= (~PMF_WALL);
	PM_StepSlideMove( qfalse );
}

/*
==============
PM_DeadMove
==============
*/
static void PM_DeadMove( void )
{
	float  forward;

	if (!pml.walking)
	{
		return;
	}

	// extra friction

	forward  = VectorLength (pm->ps->velocity);
	forward -= 20;

	if (forward <= 0)
	{
		VectorClear (pm->ps->velocity);
	}
	else
	{
		VectorNormalizeNR (pm->ps->velocity);
		VectorScale (pm->ps->velocity, forward, pm->ps->velocity);
	}
}

/*
===============
PM_NoclipMove
===============
*/
static void PM_NoclipMove( void )
{
	float	speed, drop, friction, control, newspeed;
	int i;
	vec3_t	wishvel;
	float	fmove, smove;
	vec3_t	wishdir;
	float	wishspeed;
	float	scale;

	pm->ps->viewheight = DEFAULT_VIEWHEIGHT;

	// friction

	speed = VectorLength (pm->ps->velocity);

	if (speed < 1)
	{
		VectorCopy (vec3_origin, pm->ps->velocity);
	}
	else
	{
		drop	 = 0;

		friction = pm_friction * 1.5;	  // extra friction
		control  = speed < pm_stopspeed ? pm_stopspeed : speed;
		drop	+= control * friction * pml.frametime;

		// scale the velocity
		newspeed = speed - drop;

		if (newspeed < 0)
		{
			newspeed = 0;
		}
		newspeed /= speed;

		VectorScale (pm->ps->velocity, newspeed, pm->ps->velocity);
	}

	// accelerate
	scale = PM_CmdScale( &pm->cmd );

	fmove = pm->cmd.forwardmove;
	smove = pm->cmd.rightmove;

	for (i = 0 ; i < 3 ; i++)
	{
		wishvel[i] = pml.forward[i] * fmove + pml.right[i] * smove;
	}
	wishvel[2] += pm->cmd.upmove;

	VectorCopy (wishvel, wishdir);
	VectorNormalize(wishdir, wishspeed);
	wishspeed *= scale;

	PM_Accelerate( wishdir, wishspeed, pm_flyaccelerate );

	// move
	VectorMA (pm->ps->origin, pml.frametime, pm->ps->velocity, pm->ps->origin);
}

//============================================================================

/*
================
PM_FootstepForSurface

Returns an event number apropriate for the groundsurface
================
*/
static void PM_FootstepForSurface( void )
{
	if (pml.groundTrace.surfaceFlags & SURF_NOSTEPS)
	{
		return;
	}

	BG_AddPredictableEventToPlayerstate( EV_FOOTSTEP, SURFACE_GET_TYPE(pml.groundTrace.surfaceFlags), pm->ps );
}

/*
=================
PM_CrashLand

Check for hard landings that generate sound events
=================
*/
static void PM_CrashLand( int entnum )
{
	float  delta;
	float  dist;
	float  vel, acc;
	float  t;
	float  a, b, c, den;

	// If we are on the ground, reset the walljump ability
	pm->ps->generic1 = 0;

	// Dokta8 - if we are on the ground, reset the kick ability
	pm->ps->stats[STAT_UTPMOVE_FLAGS] &= ~UT_PMF_KICKING;

	// calculate the exact velocity on landing
	dist = pm->ps->origin[2] - pml.previous_origin[2];
	vel  = pml.previous_velocity[2];
	acc  = -pm->ps->gravity;

	a	 = acc / 2;
	b	 = vel;
	c	 = -dist;

	den  = b * b - 4 * a * c;

	if (den < 0)
	{
		return;
	}
	t	  = (-b - sqrt( den )) / (2 * a);

	delta = vel + t * acc;

	// Dont play this animation unecisarily
	if (delta < -200)
	{
		// decide which landing animation to use
//		if ( pm->ps->pm_flags & PMF_BACKWARDS_JUMP ) {
//			PM_ForceLegsAnim( LEGS_BACKLAND );
//		} else {
		if (!(pm->ps->pm_flags & PMF_DUCKED))
		{
			pm->ps->legsTimer = TIMER_LAND;
			PM_ForceLegsAnim( LEGS_LAND );
		}

//		}
	}
	delta = delta * delta * 0.0001;

/*	GottaBeKD: Not in UT
	// ducking while falling doubles damage
	if ( pm->ps->pm_flags & PMF_DUCKED ) {
		delta *= 2;
	}
*/

	// never take falling damage if completely underwater
	if (pm->waterlevel == 3)
	{
		return;
	}

	// reduce falling damage if there is standing water
	if (pm->waterlevel == 2)
	{
		delta *= 0.25;
	}

	if (pm->waterlevel == 1)
	{
		delta *= 0.5;
	}

	if (delta < 1)
	{
		return;
	}

	// create a local entity event to play the sound

	// SURF_NODAMAGE is used for bounce pads where you don't ever
	// want to take damage or play a crunch sound
	if (pm->ps->stats [STAT_HEALTH] > 0)
	{
		if (!(pml.groundTrace.surfaceFlags & SURF_NODAMAGE))
		{
			if (delta > 45)
			{
				if (entnum < MAX_CLIENTS)
				{
					//PM_AddEvent(EV_UT_GOOMBA );
					BG_AddPredictableEventToPlayerstate( EV_UT_GOOMBA, entnum, pm->ps);
					pm->ps->velocity[2] = 300; //Thock!
				}
				else
				{
					PM_AddEvent( EV_FALL_FAR );
					pm->ps->stats[STAT_EVENT_PARAM] = (float)(UT_MAX_HEALTH * 110 / 100) * (delta / 65);
				}
			}
			else
			if (delta > 30)
			{
				if (entnum < MAX_CLIENTS)
				{
					//PM_AddEvent( EV_UT_GOOMBA );
					BG_AddPredictableEventToPlayerstate( EV_UT_GOOMBA, entnum, pm->ps);
					//PM_AddEvent(EV_UT_GOOMBA);
					pm->ps->velocity[2] = 300; //Thock!
				}
				else
				{
					PM_AddEvent( EV_FALL_MEDIUM );
					pm->ps->stats[STAT_EVENT_PARAM] = (float)(UT_MAX_HEALTH * 30 / 100) * (delta / 40);
				}
			}
			else
			if (delta > 7)
			{
				PM_FootstepForSurface();
			}
		}
	}

	// start footstep cycle over
	pm->ps->bobCycle = 0;

	if (pm->ps->pm_flags & PMF_DUCKED)
	{
		float  xy;
		//Com_Printf("Power slidey");

		xy = sqrt( pm->ps->velocity[0] * pm->ps->velocity[0]
			   + pm->ps->velocity[1] * pm->ps->velocity[1] );

		if (xy > 300)
		{
			//pm->ps->pm_flags |= PMF_TIME_POWERSLIDE;
			pm->ps->eFlags |= EF_POWERSLIDE;
			// Com_Printf("FAst slidey");
		}
	}
}

/*
=============
PM_CheckStuck
=============
*/
/*
void PM_CheckStuck(void) {
	trace_t trace;

	pm->trace (&trace, pm->ps->origin, pm->mins, pm->maxs, pm->ps->origin, pm->ps->clientNum, pm->tracemask);
	if (trace.allsolid) {
		//int shit = qtrue;
	}
}
*/

/*
=============
PM_CorrectAllSolid
=============
*/
static int PM_CorrectAllSolid( trace_t *trace )
{
	int i, j, k;
	vec3_t	point;

	if (pm->debugLevel)
	{
		Com_Printf("%i:allsolid\n", c_pmove);
	}

	// jitter around
	for (i = -1; i <= 1; i++)
	{
		for (j = -1; j <= 1; j++)
		{
			for (k = -1; k <= 1; k++)
			{
				VectorCopy(pm->ps->origin, point);
				point[0] += (float)i;
				point[1] += (float)j;
				point[2] += (float)k;
				pm->trace (trace, point, pm->mins, pm->maxs, point, pm->ps->clientNum, pm->tracemask);

				if (!trace->allsolid)
				{
					point[0] = pm->ps->origin[0];
					point[1] = pm->ps->origin[1];
					point[2] = pm->ps->origin[2] - 0.25;

					pm->trace (trace, pm->ps->origin, pm->mins, pm->maxs, point, pm->ps->clientNum, pm->tracemask);
					pml.groundTrace = *trace;
					return qtrue;
				}
			}
		}
	}

	pm->ps->groundEntityNum = ENTITYNUM_NONE;
	pml.groundPlane 	= qfalse;
	pml.walking 	= qfalse;

	return qfalse;
}

/*
=============
PM_GroundTraceMissed

The ground trace didn't hit a surface, so we are in freefall
=============
*/
static void PM_GroundTraceMissed( void )
{
	trace_t  trace;
	vec3_t	 point;

	// ITs ok if we are on a ladder
	if (pm->ps->pm_flags & PMF_LADDER)
	{
		pm->ps->groundEntityNum = ENTITYNUM_NONE;
		pml.groundPlane 	= qfalse;
		pml.walking 	= qfalse;
		return;
	}

	pm->ps->pm_flags |= PMF_WALL;
	PM_CheckLadder ( qtrue );

	if (pm->ps->groundEntityNum != ENTITYNUM_NONE)
	{
		// we just transitioned into freefall
		if (pm->debugLevel)
		{
			Com_Printf("%i:lift\n", c_pmove);
		}

		if (pm->ps->pm_flags & PMF_LADDER)
		{
			return;
		}

		// if they aren't in a jumping animation and the ground is a ways away, force into it
		// if we didn't do the trace, the player would be backflipping down staircases
		VectorCopy( pm->ps->origin, point );
		point[2] -= 64;

		pm->trace (&trace, pm->ps->origin, pm->mins, pm->maxs, point, pm->ps->clientNum, pm->tracemask);

		if (trace.fraction == 1.0)
		{
			if (pm->ps->pm_flags & PMF_OTHERLEG)
			{
				pm->ps->pm_flags &= ~PMF_OTHERLEG;
				PM_ForceLegsAnim( pm->cmd.forwardmove >= 0 ? LEGS_JUMP : LEGS_BACKJUMP );
			}
			else
			{
				pm->ps->pm_flags |= PMF_OTHERLEG;
				PM_ForceLegsAnim( pm->cmd.forwardmove >= 0 ? LEGS_JUMPRT : LEGS_BACKJUMPRT );
			}

			pm->ps->groundEntityNum = ENTITYNUM_NONE;
			pml.walking 	= qfalse;
		}
	}
	pml.groundPlane 	= qfalse;
}

/*
=============
PM_GroundTrace
=============
*/
static void PM_GroundTrace( void )
{
	vec3_t	 point;
	trace_t  trace;

	point[0] = pm->ps->origin[0];
	point[1] = pm->ps->origin[1];
	point[2] = pm->ps->origin[2] - 0.25;

	pm->trace (&trace, pm->ps->origin, pm->mins, pm->maxs, point, pm->ps->clientNum, pm->tracemask);
	pml.groundTrace = trace;

	// do something corrective if the trace starts in a solid...
	if (trace.allsolid)
	{
		if (!PM_CorrectAllSolid(&trace))
		{
			return;
		}
	}

	// if the trace didn't hit anything, we are in free fall
	if (trace.fraction == 1.0)
	{
		PM_GroundTraceMissed();
		pml.groundPlane = qfalse;
		pml.walking = qfalse;
		return;
	}

	// check if getting thrown off the ground
	if ((pm->ps->velocity[2] > 0) && (DotProduct( pm->ps->velocity, trace.plane.normal ) > 10))
	{
		if (pm->debugLevel)
		{
			Com_Printf("%i:kickoff\n", c_pmove);
		}

		// go into jump animation
		if (pm->ps->pm_flags & PMF_OTHERLEG)
		{
			pm->ps->pm_flags &= ~PMF_OTHERLEG;
			PM_ForceLegsAnim( pm->cmd.forwardmove >= 0 ? LEGS_JUMP : LEGS_BACKJUMP );
		}
		else
		{
			pm->ps->pm_flags |= PMF_OTHERLEG;
			PM_ForceLegsAnim( pm->cmd.forwardmove >= 0 ? LEGS_JUMPRT : LEGS_BACKJUMPRT );
		}

		pm->ps->groundEntityNum = ENTITYNUM_NONE;
		pml.groundPlane 	= qfalse;
		pml.walking 	= qfalse;
		return;
	}

	// slopes that are too steep will not be considered onground
	if (trace.plane.normal[2] < MIN_WALK_NORMAL)
	{
		if (pm->debugLevel)
		{
			Com_Printf("%i:steep\n", c_pmove);
		}
		// FIXME: if they can't slide down the slope, let them
		// walk (sharp crevices)
		pm->ps->groundEntityNum = ENTITYNUM_NONE;
		pml.groundPlane 	= qtrue;
		pml.walking 	= qfalse;
		return;
	}

	if (pm->waterlevel > 2)
	{
		pm->ps->groundEntityNum = ENTITYNUM_NONE;
		pml.groundPlane 	= qtrue;
		pml.walking 	= qfalse;
		return;
	}

	pm->ps->pm_flags &= (~PMF_LADDER_IGNORE);
	pm->ps->pm_flags &= (~PMF_FORCE_DUCKED);
	pml.groundPlane   = qtrue;
	pml.walking   = qtrue;

	// hitting solid ground will end a waterjump
	if (pm->ps->pm_flags & PMF_TIME_WATERJUMP)
	{
		pm->ps->pm_flags &= ~(PMF_TIME_WATERJUMP);
		//pm->ps->pm_flags &= ~(PMF_TIME_WATERJUMP | PMF_TIME_LAND);
		pm->ps->pm_time   = 0;
	}

	if (pm->ps->groundEntityNum == ENTITYNUM_NONE)
	{
		// just hit the ground
		if (pm->debugLevel)
		{
			Com_Printf("%i:Land\n", c_pmove);
		}

		PM_CrashLand(trace.entityNum);

		// don't do landing time if we were just going down a slope
		if (pml.previous_velocity[2] < -200)
		{
			// don't allow another jump for a little while
//			pm->ps->pm_flags |= PMF_TIME_LAND;
			pm->ps->pm_time = 250;
		}
	}

	pm->ps->groundEntityNum = trace.entityNum;

	// don't reset the z velocity for slopes
//	pm->ps->velocity[2] = 0;

	PM_AddTouchEnt( trace.entityNum );
}

/*
=============
PM_SetWaterLevel	FIXME: avoid this twice?  certainly if not moving
=============
*/
static void PM_SetWaterLevel( void )
{
	vec3_t	point;
	int cont;
	int sample1;
	int sample2;

	//
	// get waterlevel, accounting for ducking
	//
	pm->waterlevel = 0;
	pm->watertype  = 0;

	point[0]	   = pm->ps->origin[0];
	point[1]	   = pm->ps->origin[1];
	point[2]	   = pm->ps->origin[2] + MINS_Z + 1;
	cont		   = pm->pointcontents( point, pm->ps->clientNum );

	if (cont & MASK_WATER)
	{
		sample2 	   = pm->ps->viewheight - MINS_Z;
		sample1 	   = sample2 / 2;

		pm->watertype  = cont;
		pm->waterlevel = 1;
		point[2]	   = pm->ps->origin[2] + MINS_Z + sample1;
		cont		   = pm->pointcontents (point, pm->ps->clientNum );

		if (cont & MASK_WATER)
		{
			pm->waterlevel = 2;
			point[2]	   = pm->ps->origin[2] + MINS_Z + sample2;
			cont		   = pm->pointcontents (point, pm->ps->clientNum );

			if (cont & MASK_WATER)
			{
				pm->waterlevel = 3;
			}
		}
	}
}

/*
==============
PM_CheckDuck

Sets mins, maxs, and pm->ps->viewheight
==============
*/
static void PM_CheckDuck (void)
{
	trace_t  trace;
	float	 delta;
	float	 xy;

	pm->mins[0] = -15;
	pm->mins[1] = -15;

	pm->maxs[0] = 15;
	pm->maxs[1] = 15;

	pm->mins[2] = MINS_Z;
	pm->maxs[2] = PMOVE_HEIGHT_STAND;

	if ((pm->ps->pm_type == PM_DEAD) || pm->freeze)
	{
//pm->maxs[2] = -8;
		pm->ps->viewheight = DEAD_VIEWHEIGHT;
		return;
	}

	//If in air and already ducked, thats ok, if not dont allow ducking
/*	 if (pm->ps->groundEntityNum==ENTITYNUM_NONE )
   {
	  if (!(pm->ps->pm_flags &	PMF_DUCKED))
	  {
	 if (pm->cmd.upmove<0) pm->cmd.upmove=0;
	  }
   }*/

	if (((pm->cmd.upmove < 0) || (pm->ps->pm_flags & PMF_FORCE_DUCKED)) &&
		!(pm->ps->pm_flags & PMF_LADDER) && !(pm->waterlevel > 1) && (pm->ps->legsTimer == 0))
	{
		//	 if (!(pm->ps->pm_flags &  PMF_DUCKED)) //set time of initial duck
//		pm->ps->grapplePoint[0]=pm->cmd.serverTime+500;
		if (!(pm->ps->pm_flags & PMF_DUCKED))
		{
			if (pm->ps->groundEntityNum != ENTITYNUM_NONE)
			{
				delta				 = (int)(0.1f * UT_MAX_HEALTH * UT_STAM_MUL);

				pm->ps->stats[STAT_STAMINA] -= delta;

				if(pm->ps->stats[STAT_STAMINA] < 0)
				{
					pm->ps->stats[STAT_STAMINA] = 0;
				}
			}
			PM_ForceLegsAnim(LEGS_STANDCR);
			pm->ps->legsTimer = 200;
		}

		// do duck
		pm->ps->pm_flags |= PMF_DUCKED;
	}
	else
	{
		// stand up if possible
		if (pm->ps->pm_flags & PMF_DUCKED)
		{
//		   if (pm->cmd.serverTime>pm->ps->grapplePoint[0])
			if ((pm->ps->groundEntityNum != ENTITYNUM_NONE) || (pm->waterlevel > 1))
			{
				if (pm->ps->legsTimer == 0)
				{
					pm->maxs[2] = PMOVE_HEIGHT_STAND;
					// try to stand up
					pm->trace (&trace, pm->ps->origin, pm->mins, pm->maxs, pm->ps->origin, pm->ps->clientNum, pm->tracemask );

					if (!trace.allsolid)
					{
						//if unducking
						/*	   delta = (int)(0.1f * UT_MAX_HEALTH * UT_STAM_MUL);

							  pm->ps->stats[STAT_STAMINA] -= delta;
							 if(pm->ps->stats[STAT_STAMINA] < 0 )
								  pm->ps->stats[STAT_STAMINA] = 0; */

						pm->ps->pm_flags	   &= ~PMF_DUCKED;
						//	 pm->ps->stats[STAT_UNDUCK_TIME]=0;
						pm->ps->grapplePoint[0] = 0;

						PM_ForceLegsAnim(LEGS_CRSTAND);
						pm->ps->legsTimer = 200;
					}
				}
			}
		}
	}

	if (pm->ps->pm_flags & PMF_DUCKED)
	{
		pm->maxs[2]    = PMOVE_HEIGHT_CROUCH;
		pm->ps->viewheight = CROUCH_VIEWHEIGHT;
	}
	else if ((pm->ps->legsAnim & (~ANIM_TOGGLEBIT)) == LEGS_SWIM)
	{
		pm->maxs[2]    = PMOVE_HEIGHT_SWIM;
		pm->ps->viewheight = -5; // SWIM_VIEWHEIGHT;
	}
	else
	{
		pm->maxs[2]    = PMOVE_HEIGHT_STAND;
		pm->ps->viewheight = DEFAULT_VIEWHEIGHT;
	}

	xy = sqrt( pm->ps->velocity[0] * pm->ps->velocity[0] + pm->ps->velocity[1] * pm->ps->velocity[1] );

	if ((xy < 300) || (pm->cmd.upmove > -10))
	{
		pm->ps->eFlags &= ~EF_POWERSLIDE;
		//pm->ps->pm_flags &=~PMF_TIME_POWERSLIDE;
	}
}

//===================================================================

/*
===============
PM_Footsteps
===============
*/
static void PM_Footsteps( void )
{
	float	  bobmove;
	int   old;
	qboolean  footstep;
	int weaponType = bg_weaponlist [utPSGetWeaponID(pm->ps)].type;
	int weaponid = bg_weaponlist [utPSGetWeaponID(pm->ps)].q3weaponid;

	//
	// calculate speed and cycle to be used for
	// all cyclic walking effects
	//
	pm->xyspeed = sqrt( pm->ps->velocity[0] * pm->ps->velocity[0]
				+ pm->ps->velocity[1] * pm->ps->velocity[1] );

	if (pm->ps->groundEntityNum == ENTITYNUM_NONE)
	{
		// airborne leaves position in cycle intact, but doesn't advance
		if (pm->waterlevel > 1)
		{
			PM_ContinueLegsAnim( LEGS_SWIM );
		}
		else
		if (pm->ps->pm_flags & PMF_DUCKED)
		{
			PM_ContinueLegsAnim( LEGS_IDLECR );
		}

		// handle ledge climb and ladder climb anims
		if (pm->ps->pm_flags & PMF_LADDER)
		{
			if (pm->cmd.forwardmove)
			{
				// only do these anims if they are moving on a ladder
				PM_ContinueLegsAnim( BOTH_CLIMB );

				if (pm->ps->pm_flags & PMF_LADDER_UPPER)
				{
					PM_ContinueTorsoAnim( BOTH_CLIMB );
				}
			}
			else
			{
				PM_ContinueLegsAnim( BOTH_CLIMB_IDLE );

				if (pm->ps->pm_flags & PMF_LADDER_UPPER)
				{
					PM_ContinueTorsoAnim( BOTH_CLIMB_IDLE );
				}
			}

			if (pm->cmd.forwardmove)
			{
				// check for footstep / splash sounds
				old 	 = pm->ps->bobCycle;
				bobmove 	 = 0.4f;
				pm->ps->bobCycle = (int)( old + bobmove * pml.msec ) & 255;

				// if we just crossed a cycle boundary, play an apropriate footstep event
				if (((old + 64) ^ (pm->ps->bobCycle + 64)) & 128)
				{
					PM_AddEvent( EV_UT_LADDER );
				}
			}
		}
		// check for ledge climb
		else if (pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_CLIMBING)
		{
			PM_ContinueLegsAnim( BOTH_LEDGECLIMB );
			PM_ContinueTorsoAnim( BOTH_LEDGECLIMB );
		}
		else if (pm->ps->pm_flags & PMF_FORCE_DUCKED)
		{
			PM_ContinueTorsoAnim( TORSO_STAND_CROUCH );
			PM_ContinueLegsAnim( LEGS_IDLECR );
		}

		return;
	}

	// if not trying to move
	if (!pm->cmd.forwardmove && !pm->cmd.rightmove)
	{
		if (pm->xyspeed < 5)
		{
			pm->ps->bobCycle = 0;	// start at beginning of cycle again

			if (pm->ps->pm_flags & PMF_DUCKED)
			{
				PM_ContinueTorsoAnim( TORSO_STAND_CROUCH );
				PM_ContinueLegsAnim( LEGS_IDLECR );
			}
			else
			{
				PM_ContinueLegsAnim( LEGS_IDLE );
			}
		}
		return;
	}

	footstep = qfalse;

	if (pm->ps->pm_flags & PMF_DUCKED)
	{
		bobmove = 0.3f; // ducked characters bob much faster

		if (!(pm->ps->eFlags & EF_POWERSLIDE)) {

			if ( weaponType == UT_WPTYPE_MEDIUM_GUN ) {

			    if (weaponid == UT_WP_MAC11) {
			        PM_ContinueTorsoAnim( TORSO_CROUCH_WALK );
			    } else {
			        PM_ContinueTorsoAnim( TORSO_SECONDARY_CROUCHWALK );
			    }

			} else {
				PM_ContinueTorsoAnim( TORSO_CROUCH_WALK );
			}

			if (pm->ps->pm_flags & PMF_BACKWARDS_RUN) {
				PM_ContinueLegsAnim( LEGS_BACKWALKCR );
			} else {
				PM_ContinueLegsAnim( LEGS_WALKCR );
			}

		} else {

		    if ( weaponType == UT_WPTYPE_MEDIUM_GUN ) {

			    if (weaponid == UT_WP_MAC11) {
			        PM_ContinueTorsoAnim( TORSO_STAND_CROUCH );
                } else {
                    PM_ContinueTorsoAnim( TORSO_SECONDARY_CROUCH );
                }

			} else {
				PM_ContinueTorsoAnim( TORSO_STAND_CROUCH );
			}

			PM_ContinueLegsAnim( LEGS_IDLECR );
		}
	}
	else
	{
		qboolean  limp = (pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_LIMPING) ? qtrue : qfalse;

		if (!(pm->cmd.buttons & BUTTON_WALKING))
		{
			bobmove = 0.40f;	// faster speeds bob faster

			switch ( weaponType )
			{
				case UT_WPTYPE_MELEE:
				case UT_WPTYPE_THROW:
				case UT_WPTYPE_SMALL_GUN:
				case UT_WPTYPE_SMALL_GUN_DUAL:
					PM_ContinueTorsoAnim( TORSO_RUN_1HANDED );
					break;
				case UT_WPTYPE_MEDIUM_GUN:
					PM_ContinueTorsoAnim( TORSO_SECONDARY_RUN );
					break;
				default:
					PM_ContinueTorsoAnim( TORSO_RUN_2HANDED );
					break;
			}

			if (pm->ps->pm_flags & PMF_BACKWARDS_RUN)
			{
				PM_ContinueLegsAnim( limp ? LEGS_BACKLIMP : LEGS_BACKRUN );
			}
			else
			{
				if ((pm->cmd.buttons & BUTTON_SPRINT) && (pm->cmd.rightmove == 0) && (pm->cmd.forwardmove > 80))
					PM_ContinueLegsAnim( limp ? LEGS_LIMP : LEGS_SPRINT );
				else
					PM_ContinueLegsAnim( limp ? LEGS_LIMP : LEGS_RUN );
			}
			footstep = qtrue;
		}
		else
		{
			bobmove = 0.3f; // walking bobs slow

			if (pm->ps->pm_flags & PMF_BACKWARDS_RUN)
			{
				PM_ContinueTorsoAnim( TORSO_STAND_WALK );
				PM_ContinueLegsAnim( limp ? LEGS_BACKLIMP : LEGS_BACKWALK );
			}
			else
			{
				PM_ContinueTorsoAnim( TORSO_STAND_WALK );
				PM_ContinueLegsAnim( limp ? LEGS_LIMP : LEGS_WALK );
			}
		}
	}

	// check for footstep / splash sounds
	old 	   = pm->ps->bobCycle;
	pm->ps->bobCycle = (int)( old + bobmove * pml.msec ) & 255;

	// if we just crossed a cycle boundary, play an apropriate footstep event
	if (((old + 64) ^ (pm->ps->bobCycle + 64)) & 128)
	{
		if (pm->waterlevel == 0)
		{
			// on ground will only play sounds if running
			if (footstep)
			{
				PM_FootstepForSurface();
			}
		}
		else if (pm->waterlevel == 1)
		{
			// splashing
			PM_AddEvent( EV_FOOTSPLASH );
		}
		else if (pm->waterlevel == 2)
		{
			// wading / swimming at surface
			PM_AddEvent( EV_SWIM );
		}
		else if (pm->waterlevel == 3)
		{
			// no sound when completely underwater
		}
	}
}

/*
==============
PM_WaterEvents

Generate sound events for entering and leaving water
==============
*/
static void PM_WaterEvents( void )		// FIXME?
{	//
	// if just entered a water volume, play a sound
	//
	if (!pml.previous_waterlevel && pm->waterlevel)
	{
		if (pm->ps->velocity[2] < -500)
		{
			PM_AddEvent( EV_WATER_TOUCH_FAST );
		}
		else
		{
			PM_AddEvent( EV_WATER_TOUCH );
		}
	}

	//
	// if just completely exited a water volume, play a sound
	//
	if (pml.previous_waterlevel && !pm->waterlevel)
	{
		PM_AddEvent( EV_WATER_LEAVE );
	}

	//
	// check for head just going under water
	//
	if ((pml.previous_waterlevel != 3) && (pm->waterlevel == 3))
	{
		PM_AddEvent( EV_WATER_UNDER );
	}

	//
	// check for head just coming out of water
	//
	if ((pml.previous_waterlevel == 3) && (pm->waterlevel != 3))
	{
		PM_AddEvent( EV_WATER_CLEAR );
	}
}

/*
===============
PM_BeginWeaponChange
===============
*/
static void PM_BeginWeaponChange( int weapon )
{
	// raedwulf TODO: somehow WEAPON_FIRING state gets set during a reload,
	// causing a strange effect... we need to fix this by making sure the
	// WEAPON_FIRING state does not get set
	if (!(pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_RELOADING) &&
			pm->ps->weaponstate == WEAPON_FIRING)
	/*if (pm->ps->weaponstate == WEAPON_FIRING)*/
	{
		if (utPSGetWeaponBullets(pm->ps) > 0)
		{
			return;
		}
	}

	if ((weapon <= UT_WP_NONE) || (weapon >= UT_WP_NUM_WEAPONS))
	{
		return;
	}

	if (pm->ps->weaponstate == WEAPON_DROPPING)
	{
		return;
	}

	// no weapon change if bandaging a teammate
	if (pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_TEAM_BANDAGE)
	{
		return;
	}

	// Dont have that weapon
	if (-1 == utPSGetWeaponByID ( pm->ps, weapon ))
	{
		return;
	}

	BG_AddPredictableEventToPlayerstate( EV_CHANGE_WEAPON, 0, pm->ps );
	pm->ps->weaponstate = WEAPON_DROPPING;

	if (utPSGetWeaponID ( pm->ps) == 0)
	{
		pm->ps->weaponTime = 0;
	}
	else
	{
		//if ( pm->ps->weaponTime < UT_WEAPONHIDE_TIME )	// this check just makes sure another timer isn't in operation
		pm->ps->weaponTime = UT_WEAPONHIDE_TIME;
	}

	PM_ForceTorsoAnim( TORSO_WEAPON_LOWER );

	// Time to clear stuff that could cause a problem.
	pm->ps->stats[STAT_BURST_ROUNDS]   = 0;
	pm->ps->stats[STAT_RECOIL]	   = 0;
	pm->ps->stats[STAT_RECOIL_TIME]    = 0;
	pm->ps->stats[STAT_UTPMOVE_FLAGS] &= ~UT_PMF_READY_FIRE;
	pm->ps->stats[STAT_UTPMOVE_FLAGS] &= ~UT_PMF_RELOADING;
	pm->ps->stats[STAT_UTPMOVE_FLAGS] &= ~UT_PMF_BANDAGING;
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : PM_FinishWeaponChange
// Description: Finishes the weapon change the user requested
// Author	  : Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
static void PM_FinishWeaponChange( void )
{
	int  weaponslot;

	// Get the slot of the weapon they switched to
	weaponslot = utPSGetWeaponByID ( pm->ps, pm->cmd.weapon );

	if (-1 == weaponslot)
	{
		weaponslot = utPSGetWeaponByID ( pm->ps, UT_WP_KNIFE );
	}

	// Set the new weapon and setup the finish of the switch
	pm->ps->weaponslot	= weaponslot;
	pm->ps->weaponstate = WEAPON_RAISING;
	pm->ps->weaponTime += 250;

//	 if (pm->cmd.weapon==UT_WP_DEAGLE)||(pm->cmd.weapon==UT_WP_DEAGLE)
	//{
//	PM_StartTorsoAnim( TORSO_WEAPON_RAISE );
	// }
	//else
	//{
	PM_StartTorsoAnim( TORSO_WEAPON_RAISE );
	//}

	BG_AddPredictableEventToPlayerstate( EV_CHANGE_WEAPON, 1, pm->ps );
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : PM_TorsoAnimation
// Description: Performs a ledge climb.
// Author	  : Dokta8
///////////////////////////////////////////////////////////////////////////////////////////
static void PM_TorsoAnimation( void )
{
	// If our weapon isnt ready then ignore this call.
	if (pm->ps->weaponstate != WEAPON_READY)
	{
		return;
	}

	// If we are swimming there is a special animation
	if ((pm->ps->legsAnim & (~ANIM_TOGGLEBIT)) == LEGS_SWIM)
	{
		PM_ContinueTorsoAnim( TORSO_SWIM );
		return;
	}

	if ((pm->cmd.buttons & BUTTON_ZOOM1) || (pm->cmd.buttons & BUTTON_ZOOM2))
	{
		PM_ContinueTorsoAnim( TORSO_SCOPE );
	}
	else
	{
		// Now base the animation off the weapon type.
		switch (bg_weaponlist [utPSGetWeaponID(pm->ps )].type)
		{
			default:
			case UT_WPTYPE_MELEE:
				PM_ContinueTorsoAnim( TORSO_STAND_KNIFE );
				break;

			case UT_WPTYPE_THROW:
				PM_ContinueTorsoAnim( TORSO_STAND_KNIFE );
				break;

			case UT_WPTYPE_SMALL_GUN:
				PM_ContinueTorsoAnim( TORSO_STAND_PISTOL );
				break;

			case UT_WPTYPE_SMALL_GUN_DUAL:
				PM_ContinueTorsoAnim( TORSO_STAND_DUAL );
				break;

			case UT_WPTYPE_MEDIUM_GUN:

			    if (bg_weaponlist [utPSGetWeaponID(pm->ps )].q3weaponid == UT_WP_MAC11) {
			        PM_ContinueTorsoAnim( TORSO_STAND_PISTOL );
                } else {
                    PM_ContinueTorsoAnim( TORSO_SECONDARY_STAND );
                }

			    break;

			case UT_WPTYPE_LARGE_GUN:
				PM_ContinueTorsoAnim( TORSO_STAND_RIFLE );
				break;
		}
	}

}

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : PM_WeaponMode
// Description: Changes weapon mode
// Author	  : Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
static void PM_WeaponMode ( void )
{
	int  weaponID	   = 0;
	int  oldWeaponMode = 0;
	int  newWeaponMode = 0;

	// If the weapon mode button is being the see if it was
	// released and if so clear the held flag.
	if (pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_WEAPON_MODE_HELD)
	{
		if(!(pm->cmd.buttons & BUTTON_WEAPON_MODE))
		{
			pm->ps->stats[STAT_UTPMOVE_FLAGS] &= ~UT_PMF_WEAPON_MODE_HELD;
		}

		return;
	}

	// Cant do it right now
	if (pm->ps->weaponTime > 0)
	{
		return;
	}

	// Cant change mode if we are climbing.
	if(pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_CLIMBING)
	{
		return;
	}

	// Cant change mode if we are reloading.
	if(pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_RELOADING)
	{
		return;
	}

	// Get the weapon id
	weaponID = utPSGetWeaponID ( pm->ps );

	if (weaponID <= 0)
	{
		return;
	}

	// Get the old weapon mode
	oldWeaponMode = utPSGetWeaponMode ( pm->ps );

	// if it's a grenade then only allow mode change to armed
	if (((weaponID == UT_WP_GRENADE_HE) || (weaponID == UT_WP_GRENADE_FLASH) || (weaponID == UT_WP_GRENADE_SMOKE)) && (pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_FIRE_HELD))
	{
		if (oldWeaponMode == 1)
		{
			return;
		}
	}

	// no throwing the last knife
	if (weaponID == UT_WP_KNIFE && utPSGetWeaponBullets ( pm->ps ) == 1)
	{
		if (oldWeaponMode == 0)
		{
			return;
		}
	}

	// If the weapon mode button wasnt pressed, then we arent changing modes are we.
	else if (!(pm->cmd.buttons & BUTTON_WEAPON_MODE))
	{
		return;
	}

	// Determine the new weapon mode
	newWeaponMode = oldWeaponMode + 1;

	if (!bg_weaponlist[weaponID].modes[newWeaponMode].name)
	{
		newWeaponMode = 0;
	}

	// No change, then forget it
	if (newWeaponMode == oldWeaponMode)
	{
		return;
	}

	pm->ps->stats[STAT_UTPMOVE_FLAGS] |= UT_PMF_WEAPON_MODE_HELD;
	pm->ps->weaponTime		   = bg_weaponlist[weaponID].modes[newWeaponMode].changeTime;
	utPSSetWeaponMode ( pm->ps, newWeaponMode );

	// This event is now added at the end of PmoveSingle, to make sure it doesnt get overwritten.
	//BG_AddPredictableEventToPlayerstate( EV_UT_WEAPON_MODE, oldWeaponMode, pm->ps );
	pm->weapModeEvent	   = qtrue;
	pm->weapModeEventParam = oldWeaponMode;
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : PM_Reload
// Description: Reloads a weapon
// Author	  : Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
static void PM_Reload ( qboolean firstReload )
{
	int  weaponID = 0;

	weaponID = utPSGetWeaponID ( pm->ps );

	/*	if (weaponID == UT_WP_SR8) //if auto reload
	  {
		 weaponMode= utPSGetWeaponMode ( pm->ps );
		 if (utPSGetWeaponBullets ( pm->ps )==0 && weaponMode!=0)
		 {
		pm->cmd.buttons|= BUTTON_RELOAD;
		 }
	  }*/

	//@Fenix - new reload style for the SPAS (commented out for the moment)
	//if (weaponID == UT_WP_SPAS12) {
	//	if ((!(pm->cmd.buttons & BUTTON_RELOAD)) &&
	//		(!(pm->cmd.buttons & BUTTON_ATTACK)) &&
	//		(pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_RELOADING)) {
	//			pm->cmd.buttons |= BUTTON_RELOAD;
	//		}
	//}

	// If the reload button wasnt pressed, then we arent reloading are we.
	if(!(pm->cmd.buttons & BUTTON_RELOAD))
	{
		return;
	}

//	weaponID = utPSGetWeaponID ( pm->ps );
	if (weaponID == UT_WP_SR8)
	{
		// Only allow reload on weapon ready
		if (utPSGetWeaponBullets ( pm->ps ) > 0)
		{
			if (pm->ps->weaponstate != WEAPON_READY)
			{
				return;
			}
		}
	}

	// Get the weapon id
	//weaponID = utPSGetWeaponID ( pm->ps );
	if (weaponID <= 0)
	{
		return;
	}

	// Make sure we need to reload
	if (utPSGetWeaponBullets ( pm->ps ) == bg_weaponlist[weaponID].bullets)
	{
		return;
	}

	// Make sure this weapon has some clips to use up
	if(!utPSGetWeaponClips ( pm->ps ))
	{
		return;
	}

	// Cant reload if we are on a ladder.
	if(pm->ps->pm_flags & PMF_LADDER_UPPER)
	{
		return;
	}

	// Cant reload if we are climbing.
	if(pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_CLIMBING)
	{
		return;
	}

	// Cant reload if we are reloading.
	if(pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_RELOADING)
	{
		return;
	}

	// Cant reload if we are switching weapons
	if (pm->ps->weaponstate == WEAPON_DROPPING)
	{
		return;
	}

	// if the hk is long range, then set it to short and exit
	// TODO: FIX
/*	  if (weaponID == UT_WP_HK69)
	{
	//if the gun was doing something, we cant do it
	if (pm->ps->weaponTime>0) return;

	   if (utPSGetWeaponMode ( pm->ps )==1)
	   {
	  pm->cmd.buttons = BUTTON_WEAPON_MODE;
	  PM_WeaponMode();
	  //pm->ps->stats[STAT_UTPMOVE_FLAGS] &= ~UT_PMF_WEAPON_MODE_HELD ;
	  return;
	   }
	}  */
	//if( pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_WEAPON_MODE_HELD )
	//	return;

	pm->ps->stats[STAT_UTPMOVE_FLAGS] |= UT_PMF_RELOADING;
	pm->ps->weaponTime		   = bg_weaponlist[weaponID].reloadTime + (firstReload ? bg_weaponlist[weaponID].reloadStartTime : 0);

	// start player model reload anim
	if (bg_weaponlist [weaponID].type >= UT_WPTYPE_MEDIUM_GUN )
	{
		PM_ForceTorsoAnim( TORSO_RELOAD_RIFLE );
	}
	else
	{
		PM_ForceTorsoAnim( TORSO_RELOAD_PISTOL );
	}

	// Tell everyone else about the reload too
	PM_AddEvent( EV_UT_RELOAD );
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name       : PM_Bandage
// Description: Bandages your bleeding self.
// Author      : Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
static void PM_Bandage ( void ) {

    // Only allow bandage on weapon ready
    if (pm->ps->weaponstate != WEAPON_READY) {
        return;
    }

    // Make sure they aren't team bandaging either
    if(pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_TEAM_BANDAGE) {
        return;
    }

    // If we are bandaging then don't let them bandage
    if (pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_BANDAGING) {
        return;
    }

    // Can't bandage if we are on a ladder
    if(pm->ps->pm_flags & PMF_LADDER_UPPER) {
        return;
    }

    // Can't bandage if we are climbing
    if(pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_CLIMBING) {
        return;
    }

    // If the client is still firing don't let them bandage
    if ((pm->ps->weaponstate == WEAPON_FIRING) && (pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_FIRE_HELD)) {
        return;
    }

    // If weapon is doing something
    if (pm->ps->weaponTime > 0) {
        return;
    }

    // If they didn't bandage reload, then there is no bandage
    if (!(pm->cmd.buttons & BUTTON_BANDAGE)) {
        return;
    }

    // If we aren't bleeding, then don't bandage
    if(!(pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_BLEEDING) && !(pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_LIMPING)) {

        trace_t  tr;
        vec3_t   point;
        vec3_t   origin;

        VectorCopy(pm->ps->origin, origin);
        origin[2] += ( pm->ps->viewheight / 2 ); // dividing by two seems to give better results
        VectorMA(origin, 64, pml.forward, point);

        // add a predicted event for team mate heal if a player is in front
        pm->trace (&tr, origin, pm->mins, pm->maxs, point, pm->ps->clientNum, MASK_SHOT);

        if (tr.contents & CONTENTS_BODY) {

            pm->ps->stats[STAT_UTPMOVE_FLAGS] |= UT_PMF_TEAM_BANDAGE;

            // Make sure you can't do anything for a bit
            if (utPSHasItem ( pm->ps, UT_ITEM_MEDKIT )) {
                pm->ps->weaponTime = UT_HEAL_TIME  / 2;
            } else {
                pm->ps->weaponTime = UT_HEAL_TIME ;
            }

            PM_AddEvent ( EV_UT_TEAM_BANDAGE );
            PM_ForceTorsoAnim( TORSO_BANDAGE );
        }

        return;
    }

    pm->ps->weaponTime = UT_BANDAGE_TIME;

    // Set the bandaging flag
    pm->ps->stats[STAT_UTPMOVE_FLAGS] |= UT_PMF_BANDAGING;

    // start bandage anim
    PM_ForceTorsoAnim( TORSO_BANDAGE );

    // Kick off the bandage
    PM_AddEvent ( EV_UT_BANDAGE );

    // If we are reloading then cancel it so we can bandage
    if (pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_RELOADING) {
        pm->ps->stats[STAT_UTPMOVE_FLAGS] &= ~UT_PMF_RELOADING;
    }

}

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : PM_Use
// Description: Checks to see if the client is using
// Author	  : Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
static void PM_Use ( void )
{
	// Add the UT_USE event the first time the button is pressed.
	if (pm->cmd.buttons & BUTTON_USE)
	{
		// Only add if it isnt already held
		if(!(pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_USE_HELD))
		{
			pm->ps->stats[STAT_UTPMOVE_FLAGS] |= UT_PMF_USE_HELD;
			PM_AddEvent( EV_UT_USE );
		}
	}
	else
	{
		// Clear all use flags
		pm->ps->stats[STAT_UTPMOVE_FLAGS] &= (~UT_PMF_USE_HELD);
		//pm->ps->stats[STAT_UTPMOVE_FLAGS] &= (~UT_PMF_PLANTING);
		//pm->ps->stats[STAT_UTPMOVE_FLAGS] &= (~UT_PMF_DEFUSING);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : PM_CheckAmmo
// Description: Checks to see if the current weapon has enough ammo to shoot
// Author	  : Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
qboolean PM_CheckAmmo ( void )
{
	// Check to see if this weapon mode needs ammo
	if (bg_weaponlist[utPSGetWeaponID(pm->ps)].modes[utPSGetWeaponMode(pm->ps)].flags & UT_WPMODEFLAG_NOAMMO)
	{
		return qtrue;
	}

	// Check to see if there are bullets left
	if(!utPSGetWeaponBullets(pm->ps))
	{
		return qfalse;
	}

	return qtrue;
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : PM_Recoil
// Description: Handles the decrease of recoil
// Author	  : Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
static void PM_Recoil ( void )
{
	// Decrease the recoil delay time
	if (pm->ps->stats[STAT_RECOIL_TIME])
	{
		pm->ps->stats[STAT_RECOIL_TIME] -= pml.msec;
	}

	// If its less than zero then mission complete
	if (pm->ps->stats[STAT_RECOIL_TIME] < 0)
	{
		pm->ps->stats[STAT_RECOIL_TIME] = 0;
	}

	// If its at zero we can start decreasing the recoil
	if (pm->ps->stats[STAT_RECOIL_TIME] == 0)
	{
		pm->ps->stats[STAT_RECOIL] -= (bg_weaponlist[utPSGetWeaponID(pm->ps)].modes[utPSGetWeaponMode(pm->ps)].recoil.fall * pml.msec);
	}

	// Make sure the recoil doesnt go below zero
	if (pm->ps->stats[STAT_RECOIL] < 0)
	{
		pm->ps->stats[STAT_RECOIL] = 0;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : PM_Kick
// Description: Handles the clearing of the jump held flag and kicking
// Author	  : Apoxol, Dokta8
///////////////////////////////////////////////////////////////////////////////////////////
void PM_Kick ( void )
{
	trace_t  tr;
	vec3_t	 point, start;

	if (pm->waterlevel > 1)
	{
		return; 			// can't kick in water
	}

	if ((pm->ps->pm_type == PM_DEAD) || (pm->ps->pm_type == PM_SPECTATOR))
	{
		return; 								 // can't kick if we're dead or speccing
	}

	// If they arent holding jump anymore then clear the flag and return
	// since they cant be kicking
	if (pm->cmd.upmove < 10)
	{
		pm->ps->pm_flags &= ~PMF_JUMP_HELD;
		return;
	}

	// If they arent moving up and forward then they arent kicking
	if (!((pm->cmd.forwardmove >= 10) && !pm->warmup))
	{
		return;
	}

	// If they have already kicked then dont keep doing it
	if (pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_KICKING)
	{
		return;
	}

	// If bandage button is pressed.
	if (pm->cmd.buttons & BUTTON_BANDAGE)
	{
		return;
	}

	//Upwards movement pls
	if (pm->ps->velocity[2] < 0)
	{
		return;
	}

	// Prepare for the kick trace
	VectorCopy ( pm->ps->origin, start );
	start[2] += 30;
	VectorMA( start, 30, pml.forward, point );

	// Trace the kick
	pm->trace (&tr, start, pm->mins, pm->maxs, point, pm->ps->clientNum, MASK_SHOT);

	// Did it hit someone?
	if (tr.contents & CONTENTS_BODY)
	{
		// Set the kicking flag so we dont keep doing it
		pm->ps->stats[STAT_UTPMOVE_FLAGS] |= UT_PMF_KICKING;

		// Add the kick event so we can handle damage, etc.
		BG_AddPredictableEventToPlayerstate( EV_UT_KICK, tr.entityNum, pm->ps );
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : PM_Weapon
// Description: Handles weapon related functionality
// Author	  : Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
static void PM_Weapon( void )
{
	int  addTime;
	int  weaponID;
	int  weaponMode;
	int  legsanim;

	// Clear the fire held button if its not down anymore
	if (!(pm->cmd.buttons & 1))
	{
		pm->ps->stats[STAT_UTPMOVE_FLAGS] &= ~UT_PMF_FIRE_HELD;
	}

	// don't allow attack until all buttons are up
	if (pm->ps->pm_flags & PMF_RESPAWNED)
	{
		return;
	}

	// check for dead player
	if (pm->ps->stats[STAT_HEALTH] <= 0)
	{
//		pm->ps->weaponslot = 0;
		return;
	}

	// Call use from here cuz we dont want to have to do the above checks twice
	PM_Use ( );

	// Get the urban terror weapon using the weapon id
	weaponID   = utPSGetWeaponID ( pm->ps );
	weaponMode = utPSGetWeaponMode ( pm->ps );

	// Decrease the weapon time
	if (pm->ps->weaponTime > 0)
	{
		pm->ps->weaponTime -= pml.msec;
	}

	// Handle recoil
	PM_Recoil ( );

	PM_WeaponMode ( );

	// If the weapon time has expired and we arent firing then see if
	// the weapon has changed.

	weaponID = utPSGetWeaponID ( pm->ps );

	if (weaponID != pm->cmd.weapon)
	{
		//if ( weaponID == UT_WP_NEGEV && pm->ps->weaponTime>0)
		//	return;

		PM_BeginWeaponChange(pm->cmd.weapon);
	}

	// If we're on a ladder and got an armed grenade, put the pin back in. :)
	if((pm->ps->pm_flags & PMF_LADDER_UPPER) && (pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_FIRE_HELD)
		&& ((weaponID == UT_WP_GRENADE_HE) || (weaponID == UT_WP_GRENADE_SMOKE) || (weaponID = UT_WP_GRENADE_FLASH)))
	{
		pm->ps->stats[STAT_UTPMOVE_FLAGS] &= ~(UT_PMF_FIRE_HELD | UT_PMF_READY_FIRE);
		pm->ps->weaponstate = WEAPON_DROPPING;
		return;
	}

	// No weapons stuff while climbing or on a ladder
	if((pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_CLIMBING) ||
	   (pm->ps->pm_flags & PMF_LADDER_UPPER))
	{
		return;
	}

	// No weapons stuff while sliding
//	if( (pm->ps->pm_flags & PMF_WALL_SLIDE) )
//		return;

	// Hide our weapon while defusing
	if(pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_DEFUSING)
	{
		if(pm->ps->weaponTime < UT_WEAPONHIDE_TIME)
		{
			pm->ps->weaponTime = UT_WEAPONHIDE_TIME;
		}
	}

	if(pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_BANDAGING)
	{
		// If the attack button wasnt pressed then cancel the bandage
		if(pm->cmd.buttons & BUTTON_ATTACK)
		{
			// If weapon ain't doing nothing.
			if (pm->ps->weaponTime <= 0)
			{
				pm->ps->stats[STAT_UTPMOVE_FLAGS] &= ~UT_PMF_BANDAGING;

				if (pm->ps->weaponTime < UT_WEAPONHIDE_TIME)
				{
					pm->ps->weaponTime = UT_WEAPONHIDE_TIME;
				}
			}
		}
		// If we are still bandaging then check to see if we are done
		else
		{
			if((pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_BLEEDING) || (pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_LIMPING))
			{
				return;
			}

			// Make sure the guy can shoot again
			if (pm->ps->weaponTime < UT_WEAPONHIDE_TIME)
			{
				pm->ps->weaponTime = UT_WEAPONHIDE_TIME;
			}

			// Clear all the bleeding and bandaging flags
			pm->ps->stats[STAT_UTPMOVE_FLAGS] &= ~UT_PMF_BANDAGING;
		// useless, there is an early out above if those are set - TTimo
		//			pm->ps->stats[STAT_UTPMOVE_FLAGS] &= ~UT_PMF_LIMPING;
		//			pm->ps->stats[STAT_UTPMOVE_FLAGS] &= ~UT_PMF_BLEEDING;
		}
	}

	// If the weapon time hasnt expired yet then dont go any further.
	// Added 100 so that there is time for animatons to catchup
	if (pm->ps->weaponTime > 0)
	{
		return;
	}

	// If the fire button is held and the weapon doesnt support holding the
	// button down then just return.
	if(pm->ps->stats[STAT_BURST_ROUNDS] <= 0)
	{
		if((pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_FIRE_HELD))
		{
			if(bg_weaponlist[weaponID].modes[weaponMode].flags & UT_WPMODEFLAG_NOHOLD)
			{
				pm->ps->weaponTime	= 0;
				pm->ps->weaponstate = WEAPON_READY;
				return;
			}
		}
	}

	// If they were team bandaging then clear the flag, if the bandage button is still down
	// then dont let them shoot.
	if (pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_TEAM_BANDAGE)
	{
		pm->ps->stats[STAT_UTPMOVE_FLAGS] &= (~UT_PMF_TEAM_BANDAGE);

		// Still bandaging?  then dont bother checking for a weapon fire.
		if(pm->cmd.buttons & BUTTON_BANDAGE)
		{
			// Check bandage one more time, just in case
			PM_Bandage();
			return;
		}

		return;
	}

	// If we were reloading then we are finished now and
	// can add the new ammo.
	if (pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_RELOADING)
	{
		if(bg_weaponlist[weaponID].flags & UT_WPFLAG_SINGLEBULLETCLIP)
		{
			utPSSetWeaponBullets ( pm->ps, utPSGetWeaponBullets ( pm->ps ) + 1 );
			utPSSetWeaponClips	 ( pm->ps, utPSGetWeaponClips ( pm->ps ) - 1 );
		}
		else
		{
			utPSSetWeaponBullets ( pm->ps, bg_weaponlist[weaponID].bullets );
			utPSSetWeaponClips	 ( pm->ps, utPSGetWeaponClips ( pm->ps ) - 1 );
		}

		pm->ps->stats[STAT_BURST_ROUNDS]   = 0;
		pm->ps->stats[STAT_UTPMOVE_FLAGS] &= (~UT_PMF_RELOADING);

		// Run the reload check one more time in case they are holdin the
		// button down
		PM_Reload(qfalse);

		// Add the end time
		//pm->ps->weaponTime += bg_weaponlist[utPSGetWeaponID(pm->ps)].reloadEndTime;

		//pm->ps->weaponstate |= ~WEAPON_DROPPING; // if they had a new weaponchange qued, stop it
		return;
	}

	// If the weapon is raising or dropping then we dont have anything
	// left to do really.
	if (pm->ps->weaponstate == WEAPON_DROPPING)
	{
		PM_FinishWeaponChange();
		return;
	}
	else if (pm->ps->weaponstate == WEAPON_RAISING)
	{
		pm->ps->weaponstate = WEAPON_READY;

		if ((pm->cmd.buttons & BUTTON_ZOOM1) || (pm->cmd.buttons & BUTTON_ZOOM2))
		{
			PM_ContinueTorsoAnim( TORSO_SCOPE );
		}
		else
		{
			switch (bg_weaponlist [utPSGetWeaponID(pm->ps)].type)
			{
				default:
				case UT_WPTYPE_MELEE:
					PM_ContinueTorsoAnim( TORSO_STAND_KNIFE );
					break;

				case UT_WPTYPE_THROW:
					PM_ContinueTorsoAnim( TORSO_STAND_KNIFE );
					break;

				case UT_WPTYPE_SMALL_GUN:
					PM_ContinueTorsoAnim( TORSO_STAND_PISTOL );
					break;

				case UT_WPTYPE_SMALL_GUN_DUAL:
					PM_ContinueTorsoAnim( TORSO_STAND_DUAL );
					break;

				case UT_WPTYPE_MEDIUM_GUN:
					PM_ContinueTorsoAnim( TORSO_SECONDARY_STAND );
					break;

				case UT_WPTYPE_LARGE_GUN:
					PM_ContinueTorsoAnim( TORSO_STAND_RIFLE );
					break;

				// For Bomb
				case UT_WPTYPE_PLANT:
					PM_ContinueTorsoAnim( TORSO_STAND_KNIFE );
					break;
			}
		}

		return;
	}

	// Apoxol: Dont let them shoot during warmup...
	if(pm->warmup || pm->noFiring)
	{
		return;
	}

	// If this weapon supports readying then handle things a bit differently
	if (bg_weaponlist[weaponID].modes[weaponMode].flags & UT_WPMODEFLAG_HOLDTOREADY)
	{
		if (!PM_CheckAmmo ( ))
		{
			pm->ps->weaponTime	= 0;
			pm->ps->weaponstate = WEAPON_READY;
			return;
		}

		// If attack button is pressed but we're not trying to switch from an armed nade.
		if((pm->cmd.buttons & BUTTON_ATTACK) && !(((weaponID == UT_WP_GRENADE_HE) || (weaponID == UT_WP_GRENADE_FLASH)) && (weaponID != pm->cmd.weapon)))
		{
			if(!(pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_READY_FIRE))
			{
				// If this is the bomb make sure we're not moving too fast.
				//if (weaponID != UT_WP_BOMB || (VectorLength(pm->ps->velocity) < 125.0f && pm->ps->groundEntityNum != ENTITYNUM_NONE))
				{
					pm->ps->stats[STAT_UTPMOVE_FLAGS] |= UT_PMF_FIRE_HELD;
					pm->ps->stats[STAT_UTPMOVE_FLAGS] |= UT_PMF_READY_FIRE;
					pm->ps->weaponTime		   = bg_weaponlist[weaponID].reloadTime;

					// fire weapon
					PM_AddEvent( EV_UT_READY_FIRE );
				}

				return;
			}

			if((pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_READY_FIRE) &&
			   !(pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_FIRE_HELD))
			{
				// fire held is cleared while holding weapon
				pm->ps->stats[STAT_UTPMOVE_FLAGS] &= ~UT_PMF_READY_FIRE;
			}
			else
			{
				return;
			}
		}
		else if (!(pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_READY_FIRE))
		{
			pm->ps->weaponTime	= 0;
			pm->ps->weaponstate = WEAPON_READY;
			return;
		}
		else
		{
			pm->ps->stats[STAT_UTPMOVE_FLAGS] &= ~UT_PMF_READY_FIRE;
		}
	}
	// check for fire
	else if (pm->ps->stats[STAT_BURST_ROUNDS] == 0)
	{
		if (!(pm->cmd.buttons & BUTTON_ATTACK))
		{
			pm->ps->weaponTime	= 0;
			pm->ps->weaponstate = WEAPON_READY;
			return;
		}

		pm->ps->stats[STAT_UTPMOVE_FLAGS] |= UT_PMF_FIRE_HELD;
	}

	// raedwulf: only fire when the weapon isn't switching
	/*if (weaponID != pm->cmd.weapon)*/
		/*return;*/
	if (pm->ps->weaponstate == WEAPON_RAISING)
		return;

	if (!PM_CheckAmmo ( ))
	{
		pm->ps->stats[STAT_BURST_ROUNDS] = 0;
		PM_AddEvent( EV_NOAMMO );
		pm->ps->weaponTime		+= 500;
		return;
	}
	else if (utPSGetWeaponBullets ( pm->ps ) && !(bg_weaponlist[weaponID].modes[weaponMode].flags & UT_WPMODEFLAG_NOAMMO))
	{
		// raedwulf: no firing whilst weapon change...
		utPSSetWeaponBullets(pm->ps, utPSGetWeaponBullets(pm->ps) - 1);
	}

	if ((pm->ps->legsAnim & (~ANIM_TOGGLEBIT)) == LEGS_SWIM)
	{
		if (bg_weaponlist [utPSGetWeaponID(pm->ps)].type == UT_WPTYPE_MELEE)
		{
			PM_ContinueTorsoAnim( TORSO_ATTACK_KNIFE );
		}
		else
		{
			PM_ContinueTorsoAnim( TORSO_SWIM );
		}
	}
	else
	{
		// handle player model weapon fire animations
		// start the animation even if out of ammo

		legsanim = pm->ps->legsAnim & ~ANIM_TOGGLEBIT;

		//special case the pumpgun
		if (utPSGetWeaponID(pm->ps) == UT_WP_SPAS12 /*|| utPSGetWeaponID(pm->ps) == UT_WP_BENELLI*/)
		{
			if ((legsanim == LEGS_RUN) || (legsanim == LEGS_SPRINT) || (legsanim == LEGS_BACKRUN))
			{
				PM_ForceTorsoAnim( TORSO_RUN_ATTACK_PUMPGUN );
			}
			else
			{
				PM_ForceTorsoAnim( TORSO_ATTACK_PUMPGUN );
			}
		}
		else
		{
			if ((pm->cmd.buttons & BUTTON_ZOOM1) || (pm->cmd.buttons & BUTTON_ZOOM2))
			{
				PM_ContinueTorsoAnim( TORSO_ATTACK_SCOPE );
			}
			else
			{
				switch (bg_weaponlist [utPSGetWeaponID(pm->ps)].type)
				{
					default:
					case UT_WPTYPE_MELEE:
						PM_ForceTorsoAnim( TORSO_ATTACK_KNIFE );
						break;

					case UT_WPTYPE_THROW:
						PM_ForceTorsoAnim( TORSO_ATTACK_GRENADE );
						break;

					case UT_WPTYPE_SMALL_GUN:
						if ((legsanim == LEGS_RUN) || (legsanim == LEGS_SPRINT) || (legsanim == LEGS_BACKRUN))
						{
							PM_ForceTorsoAnim( TORSO_RUN_ATTACK_PISTOL );
						}
						else
						{
							PM_ForceTorsoAnim( TORSO_ATTACK_PISTOL );
						}
						break;

					case UT_WPTYPE_SMALL_GUN_DUAL:
						if ((legsanim == LEGS_RUN) || (legsanim == LEGS_SPRINT) || (legsanim == LEGS_BACKRUN))
						{
							PM_ForceTorsoAnim( TORSO_RUN_ATTACK_DUAL );
						}
						else
						{
							PM_ForceTorsoAnim( TORSO_ATTACK_DUAL );
						}
						break;

					case UT_WPTYPE_MEDIUM_GUN:
						if ((legsanim == LEGS_RUN) || (legsanim == LEGS_SPRINT) || (legsanim == LEGS_BACKRUN))
						{
						    if (bg_weaponlist [utPSGetWeaponID(pm->ps)].q3weaponid == UT_WP_MAC11) {
						        PM_ForceTorsoAnim( TORSO_RUN_ATTACK_PISTOL );
						    } else {
						        PM_ForceTorsoAnim( TORSO_RUN_ATTACK_RIFLE );
						    }

						}
						else
						{
						    if (bg_weaponlist [utPSGetWeaponID(pm->ps)].q3weaponid == UT_WP_MAC11) {
						        PM_ForceTorsoAnim( TORSO_ATTACK_PISTOL );
						    } else {
						        PM_ForceTorsoAnim( TORSO_SECONDARY_ATTACK );
						    }

						}
						break;

					case UT_WPTYPE_LARGE_GUN:
						if ((legsanim == LEGS_RUN) || (legsanim == LEGS_SPRINT) || (legsanim == LEGS_BACKRUN))
						{
							PM_ForceTorsoAnim( TORSO_RUN_ATTACK_RIFLE );
						}
						else
						{
							PM_ForceTorsoAnim( TORSO_ATTACK_RIFLE );
						}
						break;

					// For BOMB
					case UT_WPTYPE_PLANT:
						/*
							Special Torso animation for when a player is planting a bomb
							these have yet to be done, but hte code is here in place for it

							Right now the player will do nothing in respect to animation
						*/
					PM_ForceTorsoAnim( TORSO_ATTACK_GRENADE );
					break;
				}
			}
		}
	}

	// fire weapon
	// This event is now added at the end of PmoveSingle, to make sure it doesnt get overwritten.
	//PM_AddEvent( EV_FIRE_WEAPON );
	pm->weapFireEvent = qtrue;
	pm->weapFireEventParam = 0;

	// Turn the fire held flag on so we can track semi-auto mode
	pm->ps->weaponstate = WEAPON_FIRING;

	// Default fire time
	addTime = bg_weaponlist[weaponID].modes[weaponMode].fireTime;

	// Handle round bursting.
	if(pm->ps->stats[STAT_BURST_ROUNDS] == 0)
	{
		if(bg_weaponlist[weaponID].modes[weaponMode].burstRounds)
		{
			pm->ps->stats[STAT_BURST_ROUNDS] = bg_weaponlist[weaponID].modes[weaponMode].burstRounds;
		}
	}

	if(pm->ps->stats[STAT_BURST_ROUNDS] > 1)
	{
		addTime = bg_weaponlist[weaponID].modes[weaponMode].burstTime;
		pm->ps->stats[STAT_BURST_ROUNDS]--;
	}
	else if(pm->ps->stats[STAT_BURST_ROUNDS] > 0)
	{
		pm->ps->stats[STAT_BURST_ROUNDS]--;
	}

	// If there is recoil for this weapon then add it.
	if (!(bg_weaponlist[weaponID].modes[weaponMode].flags & UT_WPMODEFLAG_NORECOIL))
	{
		pm->ps->stats[STAT_EVENT_PARAM] = pm->ps->stats[STAT_RECOIL];
		pm->ps->stats[STAT_RECOIL]	   += (int)((float)(UT_MAX_RECOIL / bg_weaponlist[weaponID].bullets));

		if (pm->ps->stats[STAT_RECOIL] > UT_MAX_RECOIL)
		{
			pm->ps->stats[STAT_RECOIL] = UT_MAX_RECOIL;
		}
		pm->ps->stats[STAT_RECOIL_TIME] = bg_weaponlist[weaponID].modes[weaponMode].recoil.delay;
	}
	// otherwise clear it
	else
	{
		pm->ps->stats[STAT_EVENT_PARAM] = 0;
		pm->ps->stats[STAT_RECOIL_TIME] = 0;
		pm->ps->stats[STAT_RECOIL]	= 0;
	}

	// Add the delay time
	if (utPSGetWeaponBullets ( pm->ps ))
	{
		pm->ps->weaponTime += addTime;
	}
}

/*
================
PM_DropTimers
================
*/
static void PM_DropTimers( void )
{
	// drop misc timing counter
	if (pm->ps->pm_time)
	{
		if (pml.msec >= pm->ps->pm_time)
		{
			pm->ps->pm_flags &= ~PMF_ALL_TIMES;
			pm->ps->pm_time   = 0;
		}
		else
		{
			pm->ps->pm_time -= pml.msec;
		}
	}

	// drop animation counter
	if (pm->ps->legsTimer > 0)
	{
		pm->ps->legsTimer -= pml.msec;

		if (pm->ps->legsTimer < 0)
		{
			pm->ps->legsTimer = 0;
		}
	}

	if (pm->ps->torsoTimer > 0)
	{
		pm->ps->torsoTimer -= pml.msec;

		if (pm->ps->torsoTimer < 0)
		{
			pm->ps->torsoTimer = 0;
		}
	}
}

/*
================
PM_UpdateViewAngles

This can be used as another entry point when only the viewangles
are being updated isntead of a full move
================
*/
void PM_UpdateViewAngles( playerState_t *ps, const usercmd_t *cmd )
{
	short  temp;
	int    i;

	if (ps->pm_type == PM_INTERMISSION)
	{
		return; 	// no view changes at all
	}

	if ((ps->pm_type != PM_SPECTATOR) && !(ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_GHOST) && (ps->stats[STAT_HEALTH] <= 0))
	{
		return; 	// no view changes at all
	}

	// circularly clamp the angles with deltas
	for (i = 0 ; i < 3 ; i++)
	{
		temp = cmd->angles[i] + ps->delta_angles[i];

		if (i == PITCH)
		{
			// don't let the player look up or down more than 90 degrees
			if (temp > (16000 - PITCH_FUCKER))
			{
				ps->delta_angles[i] = (16000 - PITCH_FUCKER) - cmd->angles[i];
				temp			= 16000 - PITCH_FUCKER;
			}
			else if (temp < (-16000 - PITCH_FUCKER))
			{
				ps->delta_angles[i] = (-16000 - PITCH_FUCKER) - cmd->angles[i];
				temp			= -16000 - PITCH_FUCKER;
			}
			temp += PITCH_FUCKER;
		}
		ps->viewangles[i] = SHORT2ANGLE(temp);
	}
}


#ifdef FLASHNADES
/*
================
PM_FlashGrenade
================
*/
static void PM_FlashGrenade( void )
{
	int blind, sound;

	blind = pm->ps->stats[STAT_BLIND] & 0x0000FFFF;
	sound = (pm->ps->stats[STAT_BLIND] & 0xFFFF0000) >> 16;

	blind -= pml.msec;
	if( blind < 0 )
	{
		blind = 0;
	}

	sound -= pml.msec;
	if( sound < 0 )
	{
		sound = 0;
	}

	pm->ps->stats[STAT_BLIND] = blind | (sound << 16);
}
#endif


/*
================
PmoveSingle

================
*/
void trap_SnapVector( float *v );

/**
 * $(function)
 */
void PmoveSingle (pmove_t *pmove)
{
	pm = pmove;

	// this counter lets us debug movement problems with a journal
	// by setting a conditional breakpoint fot the previous frame
	c_pmove++;

	// clear results
	pm->numtouch   = 0;
	pm->watertype  = 0;
	pm->waterlevel = 0;
//	 pm->ps->pm_flags&= ~PMF_WALL_JUMP;

	// corpses can fly through bodies
//	if ( pm->ps->stats[STAT_HEALTH] <= 0 )
//		pm->tracemask &= ~CONTENTS_BODY;

	// Make sure walking button is clear if they are running, to avoid
	// proxy no-footsteps cheats
	if ((abs( pm->cmd.forwardmove ) > 64) || (abs( pm->cmd.rightmove ) > 64))
	{
		pm->cmd.buttons &= ~BUTTON_WALKING;
	}

	// if talk button is down, dissallow all other input
	// this is to prevent any possible intercept proxy from
	// adding fake talk balloons
	if (pm->cmd.buttons & BUTTON_TALK)
	{
		pm->ps->eFlags		  |= EF_TALK;
		pmove->cmd.buttons	   = BUTTON_TALK;
		pmove->cmd.forwardmove = 0;
		pmove->cmd.rightmove   = 0;
		pmove->cmd.upmove	   = 0;
	}
	else
	{
		pm->ps->eFlags &= ~EF_TALK;
	}

	// Set the firing flag so bots know who to shoot at
	if (!(pm->ps->pm_flags & PMF_RESPAWNED) &&
		(pm->ps->pm_type != PM_INTERMISSION) &&
		(pm->cmd.buttons & BUTTON_ATTACK) &&
		(utPSGetWeaponBullets(pm->ps)))
	{
		pm->ps->eFlags |= EF_FIRING;
	}
	else
	{
		pm->ps->eFlags &= ~EF_FIRING;
	}

	// clear the respawned flag if attack and use are cleared
	if ((pm->ps->stats[STAT_HEALTH] > 0) &&
		!(pm->cmd.buttons & (BUTTON_ATTACK | BUTTON_USE_HOLDABLE)))
	{
		pm->ps->pm_flags &= ~PMF_RESPAWNED;
	}

	/*
	// If both the attack and the bandage buttons are pressed.
	if (pm->cmd.buttons & BUTTON_ATTACK && pm->cmd.buttons & BUTTON_BANDAGE)
	{
		// Clear attack button; fixes bandage exploit.
		pm->cmd.buttons &= ~BUTTON_ATTACK;
	}
	*/

	// clear all pmove local vars
	memset (&pml, 0, sizeof(pml));

	// determine the time
	pml.msec = pmove->cmd.serverTime - pm->ps->commandTime;

	if (pml.msec < 1)
	{
		pml.msec = 1;
	}
	else if (pml.msec > 200)
	{
		pml.msec = 200;
	}

	pm->ps->commandTime 		  = pmove->cmd.serverTime;
	pm->ps->persistant[PERS_COMMAND_TIME] = pml.msec;

	// save old org in case we get stuck
	VectorCopy (pm->ps->origin, pml.previous_origin);

	// save old velocity for crashlanding
	VectorCopy (pm->ps->velocity, pml.previous_velocity);

	pml.frametime = pml.msec * 0.001;

	if (!pm->freeze)
	{
		// update the viewangles
		PM_UpdateViewAngles( pm->ps, &pm->cmd );
	}

	AngleVectors (pm->ps->viewangles, pml.forward, pml.right, pml.up);

	// Handle kicking
	PM_Kick ( );

	// decide if backpedaling animations should be used
	if (pm->cmd.forwardmove < 0)
	{
		pm->ps->pm_flags |= PMF_BACKWARDS_RUN;
	}
	else if (pm->cmd.forwardmove > 0 || (pm->cmd.forwardmove == 0 && pm->cmd.rightmove))
	{
		pm->ps->pm_flags &= ~PMF_BACKWARDS_RUN;
	}

	// If dead or frozen then dont let them move
	if (((pm->ps->pm_type >= PM_DEAD) && !(pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_GHOST))
		|| (pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_FROZEN) || (pm->warmup == 2) || pm->freeze)
	{
		pm->cmd.forwardmove = 0;
		pm->cmd.rightmove	= 0;
		pm->cmd.upmove		= 0;
	}

	if (pm->warmup == 2)
	{
		PM_CheckDuck ();
		PM_GroundTrace();
		PM_AirMove();
		return;
	}

	// If frozen then no movement
	if (pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_FROZEN)
	{
		// make weapon function
		if (pm->ps->weaponTime > 0)
		{
			pm->ps->weaponTime -= pml.msec;
		}

		if (pm->ps->weaponTime > 0)
		{
			PM_CheckDuck ();
			PM_GroundTrace();
			PM_AirMove();
			return;
		}

		pm->ps->weaponTime		   = 0;
		pm->ps->stats[STAT_UTPMOVE_FLAGS] &= ~UT_PMF_FROZEN;
	}

	if ((pm->ps->pm_type == PM_SPECTATOR) || (pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_GHOST) || pm->freeze)
	{
		PM_CheckDuck ();
//		PM_NoclipMove ( );
		PM_FlyMove ();
		PM_DropTimers ();
		return;
	}

	if (pm->ps->pm_type == PM_NOCLIP)
	{
		PM_NoclipMove ();
		PM_DropTimers ();
		return;
	}

	if (pm->ps->pm_type == PM_FREEZE)
	{
		return; 	// no movement at all
	}

/*	if ( pm->ps->pm_type == PM_INTERMISSION || pm->ps->pm_type == PM_SPINTERMISSION) {
		return; 	// no movement at all
	} */

	// set watertype, and waterlevel
	PM_SetWaterLevel();
	pml.previous_waterlevel = pmove->waterlevel;

	// set mins, maxs, and viewheight
	PM_CheckDuck ();
	PM_GroundTrace();

	if (pm->ps->pm_type == PM_DEAD)
	{
		PM_DeadMove ();
	}

	PM_DropTimers();

	// ARTHUR TOMLIN check and see if they're on a ladder
	PM_CheckLadder(qfalse);

	// Make sure we dont check ledges if we are on a ladder, they cause some
	// horrible effects.
	if (!(pm->ps->pm_flags & PMF_LADDER))
	{
		qboolean  ledge = (pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_CLIMBING) ? qtrue : qfalse;

		if (PM_CheckLedge() && ledge)
		{
			pm->ps->pm_flags |= PMF_FORCE_DUCKED;
		}

		if (!ledge && ((pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_CLIMBING) ? qtrue : qfalse))
		{
			PM_AddEvent ( EV_UT_GRAB_LEDGE );
		}
	}

	if (pm->ps->pm_flags & PMF_TIME_WATERJUMP)
	{
		PM_WaterJumpMove();
	}
	else if (pm->ps->pm_flags & PMF_LADDER)
	{
		PM_LadderMove();
	}
	else if (pm->waterlevel > 1)
	{
		// swimming
		PM_WaterMove();
	}
	else if (pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_CLIMBING)
	{
		PM_LedgeMove();
	}
	else if (pml.walking)
	{
		// walking on ground
		PM_WalkMove();
	}
	else
	{
		// airborne
		PM_AirMove();
	}

	// set groundentity, watertype, and waterlevel
	PM_GroundTrace();
	PM_SetWaterLevel();

	// Check ut stuff.
	PM_Reload( qtrue );
	PM_Bandage();
	//PM_Defuse();

	// weapons
	PM_Weapon();

	// torso animation
	PM_TorsoAnimation();

	// footstep events / legs animations / climbing animations / running with weap torso
	PM_Footsteps();

	// entering / leaving water splashes
	PM_WaterEvents();

	// flash grenade
#ifdef FLASHNADES
	PM_FlashGrenade();
#endif
	// snap some parts of playerstate to save network bandwidth
	trap_SnapVector( pm->ps->velocity );
}

/*
================
Pmove

Can be called by either the server or the client
================
*/
void Pmove (pmove_t *pmove)
{
	int  finalTime;

	finalTime = pmove->cmd.serverTime;

	if (finalTime < pmove->ps->commandTime)
	{
		return; // should not happen
	}

	if (finalTime > pmove->ps->commandTime + 1000)
	{
		pmove->ps->commandTime = finalTime - 1000;
	}

	pmove->ps->pmove_framecount = (pmove->ps->pmove_framecount + 1) & ((1 << PS_PMOVEFRAMECOUNTBITS) - 1);

	// Clear event flags.
	pmove->evstepHack	  = 0;
	pmove->weapModeEvent	  = qfalse;
	pmove->weapModeEventParam = 0;
	pmove->weapFireEvent	  = qfalse;
	pmove->weapFireEventParam = 0;

	// chop the move up if it is too long, to prevent framerate
	// dependent behavior
	while (pmove->ps->commandTime != finalTime)
	{
		int  msec;

		msec = finalTime - pmove->ps->commandTime;

		if (pmove->physics)
		{
			if (msec > PMOVE_MSEC)
			{
				msec = PMOVE_MSEC;
			}
		}
		else
		{
			if (msec > 66)
			{
				msec = 66;
			}
		}

		pmove->cmd.serverTime = pmove->ps->commandTime + msec;
		PmoveSingle( pmove );

		if (pmove->ps->pm_flags & PMF_JUMP_HELD)
		{
			pmove->cmd.upmove = 20;
		}
	}

	pmove->ps->stats[STAT_BUTTONS] = (pmove->cmd.buttons);
	// D8: if the player is dead then they can;t be spectated, and they
	// may want to use STAT_DEAD_YAW, so don't set zoom level
//	if ( !(pm->ps->eFlags & EF_DEAD) )
//	{
	// extract zoom bits from buttons so we can access on the client
	// pm->ps->stats[STAT_DEAD_YAW] = ((pmove->cmd.buttons & 0x3800) >> 11);
//		pm->ps->stats[STAT_DEAD_YAW] = (pmove->cmd.buttons);

//	}

	//PM_CheckStuck();

	// If step event needs to be added.
	if (pmove->evstepHack)
	{
		BG_AddPredictableEventToPlayerstate(EV_STEP, pmove->evstepHack, pmove->ps);
	}

	// If weapon mode event needs to be added.
	if (pmove->weapModeEvent)
	{
		BG_AddPredictableEventToPlayerstate(EV_UT_WEAPON_MODE, pmove->weapModeEventParam, pmove->ps);
	}

	// If weapon fire event needs to be added.
	if (pmove->weapFireEvent)
	{
	  //int weaponID = utPSGetWeaponID(pmove->ps);
	  //int bullets = UT_WEAPON_GETBULLETS(pmove->ps->weaponData[pmove->ps->weaponslot]);
	  /*if ( weaponID >= UT_WP_DUAL_BERETTA && weaponID <= UT_WP_DUAL_MAGNUM )
	  {
		pmove->weapFireEventParam = !(bullets & 1);
	  }*/
		BG_AddPredictableEventToPlayerstate(EV_FIRE_WEAPON, pmove->weapFireEventParam, pmove->ps);
	}
}

/*
=============
PM_CheckLadder [ ARTHUR TOMLIN ]
=============
*/
void PM_CheckLadder( qboolean falling )
{
	vec3_t	 flatforward, spot;
	vec3_t	 origin;
	trace_t  trace;

	if (!(pm->ps->pm_flags & PMF_LADDER))
	{
		if (!(pm->ps->pm_flags & PMF_WALL))
		{
			return;
		}
	}

	// First clear the ladder flags
	pm->ps->pm_flags &= (~PMF_LADDER);

	// If we are ignoring ladders then ignore them
	if (pm->ps->pm_flags & PMF_LADDER_IGNORE)
	{
		return;
	}

	// check for ladder
	flatforward[0] = pml.forward[0];
	flatforward[1] = pml.forward[1];
	flatforward[2] = 0;
	VectorNormalizeNR (flatforward);

	// If we are falling then we do a larger check and only from the feet.
	if (falling)
	{
		// Check from the feet
		VectorCopy ( pm->ps->origin, origin );
		origin[2] += MINS_Z;
		VectorMA (origin, 10, flatforward, spot);
	}
	else
	{
		// Check from the view origin.
		VectorCopy ( pm->ps->origin, origin );
		origin[2] += pm->ps->viewheight;
		VectorMA (origin, 1, flatforward, spot);

		// Trace it
		pm->trace (&trace, origin, pm->mins, pm->maxs, spot,
			   pm->ps->clientNum, MASK_DEADSOLID);

		// If we hit then the upper body is hitting the ladder
		if ((trace.fraction < 1) && (trace.surfaceFlags & SURF_LADDER))
		{
			pm->ps->pm_flags |= PMF_LADDER_UPPER;
		}
		else if ((trace.fraction < 1) && trace.entityNum && trace.contents == 0)
		{
			pm->trace (&trace, origin, pm->mins, pm->maxs, spot, trace.entityNum, MASK_DEADSOLID);

			// If we hit then the upper body is hitting the ladder
			if ((trace.fraction < 1) && (trace.surfaceFlags & SURF_LADDER))
			{
				pm->ps->pm_flags |= PMF_LADDER_UPPER;
			}
		}

		// Now trace from the feet
		VectorCopy ( pm->ps->origin, origin );
//		origin[2] += MINS_Z + 30;
		VectorMA (origin, 1, flatforward, spot);
	}

	// Trace it
	pm->trace (&trace, origin, pm->mins, pm->maxs, spot,
		   pm->ps->clientNum, MASK_PLAYERSOLID);

	// IF we hit then the lower body is hitting hte ladder
	if ((trace.fraction < 1) && (trace.surfaceFlags & SURF_LADDER))
	{
		// If we were falling then we need to move them a tad so
		// they are on the ladder properly.  Dont set the ladder
		// flag though, we will wait for the real check to do that.
		if (falling)
		{
			VectorCopy ( trace.endpos, pm->ps->origin );
			pm->ps->origin[2] -= MINS_Z;

			PM_CheckLadder ( qfalse );
		}
		else
		{
			pm->ps->pm_flags |= PMF_LADDER_LOWER;
		}
	}

	// Crash land if we were falling.
	if (falling && (pm->ps->pm_flags & PMF_LADDER))
	{
		PM_CrashLand(trace.entityNum);
	}
}

/*
===================
PM_LadderMove()
by: Calrathan [Arthur Tomlin]

Right now all I know is that this works for VERTICAL ladders.
Ladders with angles on them (urban2 for AQ2) haven't been tested.
===================
*/
static void PM_LadderMove( void )
{
	int i;
	vec3_t	wishvel;
	float	wishspeed;
	vec3_t	wishdir;
	float	scale;
	float	vel;

	// Check to see if they hit jump, the jump key is a
	// way to get out of ladder climbing.
	PM_CheckJump ( );

	// IF the ignore flag is set they must have bailed off of the ladder so
	// dont bother doing normal ladder move stuff.	Just slide em
	if (pm->ps->pm_flags & PMF_LADDER_IGNORE)
	{
		// move without gravity
		PM_SlideMove( qfalse );
		return;
	}

	PM_Friction ();

	scale = PM_CmdScale( &pm->cmd );

	// user intentions [what the user is attempting to do]
	if (!scale)
	{
		wishvel[0] = 0;
		wishvel[1] = 0;
		wishvel[2] = 0;
	}
	else	 // if they're trying to move... lets calculate it

	{
/*
		if ( pm->ps->pm_flags & PMF_LADDER_UPPER )
		{
			int forward = pm->cmd.forwardmove;

			if ( !pml.groundPlane && forward < 0 )
				forward *= -1;

			for (i=0 ; i<3 ; i++)
				wishvel[i] = scale * pml.forward[i]*forward  +
							 scale * pml.right[i]*pm->cmd.rightmove;

			if ( !pml.groundPlane && pm->cmd.forwardmove < 0 )
				wishvel[2] *= -1;
		}
		else
*/
		{
			if (!pml.groundPlane)
			{
				for (i = 0 ; i < 3 ; i++)
				{
					wishvel[i] = scale * pml.right[i] * pm->cmd.rightmove;
				}
			}
			else
			{
				for (i = 0 ; i < 3 ; i++)
				{
					wishvel[i] = scale * pml.forward[i] * pm->cmd.forwardmove +
							 scale * pml.right[i] * pm->cmd.rightmove;
				}
			}

			if (pm->cmd.forwardmove > 0)
			{
				wishvel[2] = 150;
			}
			else if (pm->cmd.forwardmove < 0)
			{
				wishvel[2] = -150;
			}
		}
	}

	VectorCopy (wishvel, wishdir);
	VectorNormalize(wishdir, wishspeed);

	if (wishspeed > pm->ps->speed * pm_ladderScale)
	{
		wishspeed = pm->ps->speed * pm_ladderScale;
	}

	PM_Accelerate (wishdir, wishspeed, pm_ladderaccelerate);

	// This SHOULD help us with sloped ladders, but it remains untested.
	if (pml.groundPlane && (DotProduct( pm->ps->velocity,
						pml.groundTrace.plane.normal ) < 0))
	{
		vel = VectorLength(pm->ps->velocity);
		// slide along the ground plane [the ladder section under our feet]
		PM_ClipVelocity (pm->ps->velocity, pml.groundTrace.plane.normal,
				 pm->ps->velocity, OVERCLIP );

		VectorNormalizeNR(pm->ps->velocity);
		VectorScale(pm->ps->velocity, vel, pm->ps->velocity);
	}

	PM_SlideMove( qfalse ); // move without gravity

	// Increase / decrease stamina
	PM_Stamina( VectorLength( pm->ps->velocity ) );
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : utGetFlatForward
// Description: Gets a flat forward vector
// Author	  : Dokta8
///////////////////////////////////////////////////////////////////////////////////////////
void utGetFlatForward(vec3_t flatforward)
{
	flatforward[0] = pml.forward[0];
	flatforward[1] = pml.forward[1];
	flatforward[2] = 0;
	VectorNormalizeNR(flatforward);
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : utCheckSides
// Description: Check sides
// Author	  : Dokta8
///////////////////////////////////////////////////////////////////////////////////////////
qboolean utCheckSides(void)
{
	vec3_t	 chunkMins, chunkMaxs, checkPt;
	vec3_t	 shift;
	vec3_t	 flatforward, spot;
	trace_t  trace;

	chunkMins[0] = -4;		// normal bot is -15
	chunkMins[1] = -4;		// normal bot is -15
	chunkMins[2] = -4;		// normal bot is -24
	chunkMaxs[0] = 4;		// normal top is 15
	chunkMaxs[1] = 4;		// normal top is 15
	chunkMaxs[2] = 4;		// normal top is 32 stand

	VectorCopy(pm->ps->origin, checkPt);

	checkPt[2] += 28;

	// pml.right is the direction that is right from the player
	VectorCopy(pml.right, shift);
	shift[2] = 0;
	VectorNormalizeNR(shift);

	VectorScale(shift, 12.0, shift);
	VectorAdd(checkPt, shift, checkPt);

	utGetFlatForward(flatforward);

	// here we do a check to see if there is stuff on the sides for us to climb on
	// right first
	VectorMA(checkPt, 20.0, flatforward, spot); // check right in front of person
	pm->trace (&trace, checkPt, chunkMins, chunkMaxs, spot, pm->ps->clientNum,
		   MASK_PLAYERSOLID & ~CONTENTS_BODY);		// people don't count, can't climb on them

	if (trace.fraction == 1.0)		// space in front on side
	{
//		pm->ps->stats[STAT_CLIMB_TIME] = 0;
//		Com_Printf("No wall on right...\n");	// cricel says
//		TellVector(spot, "right check point");
		return qfalse;
	}

	// then left
	VectorMA(checkPt, -2.0, shift, checkPt);		// flip dir and make up for previous shift
	VectorMA(checkPt, 20.0, flatforward, spot); 	// check right in front of person
	pm->trace (&trace, checkPt, chunkMins, chunkMaxs, spot, pm->ps->clientNum,
		   MASK_PLAYERSOLID & ~CONTENTS_BODY);		// people don't count, can't climb on them

	if (trace.fraction == 1.0)		// space in front on side
	{
//		pm->ps->stats[STAT_CLIMB_TIME] = 0;
//		Com_Printf("No wall on left...\n"); 	// cricel says
//		TellVector(spot, "left check point");
		return qfalse;
	}

//	Com_Printf("Found walls on sides\n");		// cricel says
	return qtrue;
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : PM_CheckLedge
// Description: Checks a ledge climb
// Author	  : Dokta8
///////////////////////////////////////////////////////////////////////////////////////////
qboolean PM_CheckLedge( void )
{
	vec3_t	 flatforward;
	vec3_t	 spot;
	trace_t  trace;
	vec3_t	 chunkMins;
	vec3_t	 chunkMaxs;
	vec3_t	 checkPt;

	if (!(pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_CLIMBING))
	{
		if (!(pm->ps->pm_flags & PMF_WALL))
		{
			return qfalse;
		}
	}

	// If the client isnt moving forward or is ducking them they cant
	// be climing.
	if(pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_CLIMBING)
	{
		if ((pm->cmd.forwardmove <= 0) || (pm->ps->pm_flags & PMF_DUCKED) || (pm->ps->stats[STAT_CLIMB_TIME] - pm->ps->origin[2] > 5))
		{
			// throw you backwards
			utGetFlatForward(flatforward);
			VectorScale(flatforward, -1, flatforward);	// reverse dir
			flatforward[2] = -.2f;				// guarantees falling down
			VectorNormalizeNR(flatforward);
			//VectorMA(pm->ps->velocity, 32.0, flatforward, pm->ps->velocity);
			VectorMA(pm->ps->velocity, 25.0, flatforward, pm->ps->velocity);

			pm->ps->stats[STAT_CLIMB_TIME]	   = 0;
			pm->ps->stats[STAT_UTPMOVE_FLAGS] &= (~UT_PMF_CLIMBING);

			if (pm->ps->weaponTime < UT_WEAPONHIDE_TIME)
			{
				pm->ps->weaponTime = UT_WEAPONHIDE_TIME;
			}

			return qfalse;
		}

		// If they have climbed enough to pull themselves over the ledge then
		// stop the ledge climb.
		if(pm->ps->origin[2] - pm->ps->stats[STAT_CLIMB_TIME] > 50)
		{
			pm->ps->stats[STAT_CLIMB_TIME]	   = 0;
			pm->ps->stats[STAT_UTPMOVE_FLAGS] &= (~UT_PMF_CLIMBING);

			if (pm->ps->weaponTime < UT_WEAPONHIDE_TIME)
			{
				pm->ps->weaponTime = UT_WEAPONHIDE_TIME;
			}

			return qtrue;
		}
	}

	if (pm->cmd.forwardmove <= 0)
	{
		return qfalse;
	}

	// If we're not in the air.
	if (pm->ps->groundEntityNum != ENTITYNUM_NONE)
	{
		return qfalse;
	}

	// You are now going to attempt climbing...
	// the bounding boxes for the test of the space in front of us
	VectorCopy(pm->mins, chunkMins);
	chunkMins[2] = -4;		// normal bot is -24
	VectorCopy(pm->maxs, chunkMaxs);
	chunkMaxs[2] = 4;		// normal top is 32

	VectorCopy(pm->ps->origin, checkPt);

	// check at top of jump height (48 -- remember assumption)
	//	so since 4 up and 4 down, check at 52, 52-24 (standing height) = 28
	if (pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_CLIMBING)
	{
		checkPt[2] = pm->ps->stats[STAT_CLIMB_TIME] + 28;
	}
	else
	{
		checkPt[2] += (pm->ps->viewheight);
	}

	// find out if something in front -> modified from ladder check
	utGetFlatForward(flatforward);
	VectorMA(checkPt, 4.0, flatforward, spot);	// check as if player is 4 units ahead

	// People in front don't count
	pm->trace (&trace, checkPt, chunkMins, chunkMaxs,
		   spot, pm->ps->clientNum, MASK_PLAYERSOLID & ~CONTENTS_BODY);

	// Since there is nothing in front we should clear
	if (trace.fraction == 1.0)
	{
		if (pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_CLIMBING)
		{
			pm->ps->stats[STAT_CLIMB_TIME]	   = 0;
			pm->ps->stats[STAT_UTPMOVE_FLAGS] &= (~UT_PMF_CLIMBING);

			if (pm->ps->weaponTime < UT_WEAPONHIDE_TIME)
			{
				pm->ps->weaponTime = UT_WEAPONHIDE_TIME;
			}

			// Start the weapon raise animation
			PM_StartTorsoAnim( TORSO_WEAPON_RAISE );
		}

		return qtrue;
	}

	if (pm->ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_CLIMBING)
	{
		return qfalse;
	}

	// now we move up to just over the first check but make room for bottom
	checkPt[2] += chunkMaxs[2] - chunkMins[2];	// chunkMins is neg

	// here we do a check to make sure this is not a solid wall, however they can try to
	//	climb up onto a ledge that is too small, so long as there is something to grab
	//	onto (8 units) and then in LedgeMove we check to see if they have hit something at
	//	whatever point in motion they are in at which point they are immediately knocked back
	VectorMA(checkPt, 8.0, flatforward, spot);	// check 8 units ahead
	pm->trace (&trace, checkPt, chunkMins, chunkMaxs, spot, pm->ps->clientNum,
		   MASK_PLAYERSOLID & ~CONTENTS_BODY);		// people up top don't count, might move

	// extra .2 seconds is just to cover for margin of error with lag pushing you into the wall
	if (trace.fraction == 1.0)		// space above
	{
		if (!utCheckSides())
		{
			return qfalse;
		}

		// Clear any existing jump velocity.
		pm->ps->velocity[2] = 0.0;

		// Set the climb time
		pm->ps->stats[STAT_CLIMB_TIME] = pm->ps->origin[2];

		// Set the climbing flag
		pm->ps->stats[STAT_UTPMOVE_FLAGS] |= UT_PMF_CLIMBING;

		// Start the weapon drop animation
		PM_StartTorsoAnim( TORSO_WEAPON_LOWER );
	}

	return qfalse;
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : PM_LedgeMove
// Description: Performs a ledge climb.
// Author	  : Dokta8
///////////////////////////////////////////////////////////////////////////////////////////
static void PM_LedgeMove( void )
{
	int  i;
	vec3_t	 wishvel;
	float	 wishspeed;
	vec3_t	 wishdir;
	float	 scale;
	trace_t  trace;

	PM_Friction ();

	scale = PM_CmdScale( &pm->cmd );

	// user intentions [what the user is attempting to do]
	if (!scale)
	{
		wishvel[0] = 0;
		wishvel[1] = 0;
		wishvel[2] = 0;
	}
	else	 // if they're trying to move... lets calculate it
	{
		for (i = 0 ; i < 3 ; i++)
		{
			wishvel[i] = scale * pml.forward[i] * pm->cmd.forwardmove + scale * pml.right[i] * pm->cmd.rightmove;
		}
		wishvel[2] += scale * pm->cmd.upmove;
	}

	if (!pml.groundPlane)
	{
		for (i = 0 ; i < 3 ; i++)
		{
			wishvel[i] = scale * pml.right[i] * pm->cmd.rightmove;
		}
	}
	else
	{
		for (i = 0 ; i < 3 ; i++)
		{
			wishvel[i] = scale * pml.forward[i] * pm->cmd.forwardmove +
					 scale * pml.right[i] * pm->cmd.rightmove;
		}
	}

	if (pm->cmd.forwardmove > 0)
	{
		wishvel[2] = 70;
	}
	else if (pm->cmd.forwardmove < 0)
	{
		wishvel[2] = -70;
	}

	wishvel[0]		= 0;
	wishvel[1]		= 0;

	pm->ps->velocity[0] = pm->ps->velocity[1] = 0;

	VectorCopy (wishvel, wishdir);
	VectorNormalize(wishdir, wishspeed);
/*

	if ( wishspeed > 240 * .20 ) { //pm->ps->speed * pm_climbScale ) {
		wishspeed = 240 * .20; // pm->ps->speed * pm_climbScale;
	}
*/

	PM_Accelerate (wishdir, wishspeed, pm_climbAccelerate);

	pm->trace (&trace, pm->ps->origin, pm->mins, pm->maxs, pm->ps->origin, pm->ps->clientNum,
		   MASK_PLAYERSOLID & ~CONTENTS_BODY);

	if (trace.fraction < 1)
	{
		// player is stuck due to sloped surface: give them a boost
		//pm->ps->origin[2] += 8;
		Com_Printf( "Player is stuck.\n" );
	}

	// cricel: gives ladder anim stuff [temporary] so we can fix weap accuracy while on ladder
	//pm->ps->torsoAnim = TORSO_STAND;
	//pm->ps->legsAnim = LEGS_WALK;

	PM_SlideMove( qfalse ); // move without gravity

	return;
}
