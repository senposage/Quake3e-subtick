// Copyright (C) 1999-2000 Id Software, Inc.
//
// cg_playerstate.c -- this file acts on changes in a new playerState_t
// With normal play, this will be done after local prediction, but when
// following another player or playing back a demo, it will be checked
// when the snapshot transitions like all the other entities

#include "cg_local.h"

extern int  crosshairRGBModificationCount;
extern int  crosshairFriendRGBModificationCount;
extern int  scopeRGBModificationCount;
extern int  scopeFriendRGBModificationCount;
//extern int armbandModCount;

/*
==============
CG_CheckAmmo

If the ammo has gone low enough to generate the warning, play a sound
==============
*/
void CG_CheckAmmo( void )
{
/*
	int		i;
	int		total;
	int		previous;
	int		weapons;

	// see about how many seconds of ammo we have remaining
	weapons = cg.snap->ps.stats[ STAT_WEAPONS ];
	total = 0;
	for ( i = WP_MACHINEGUN ; i < WP_NUM_WEAPONS ; i++ ) {
		if ( ! ( weapons & ( 1 << i ) ) ) {
			continue;
		}
		switch ( i ) {
		case WP_ROCKET_LAUNCHER:
		case WP_GRENADE_LAUNCHER:
		case WP_RAILGUN:
		case WP_SHOTGUN:
#ifdef MISSIONPACK
		case WP_PROX_LAUNCHER:
#endif
			total += cg.snap->ps.ammo[i] * 1000;
			break;
		default:
			total += cg.snap->ps.ammo[i] * 200;
			break;
		}
		if ( total >= 5000 ) {
			cg.lowAmmoWarning = 0;
			return;
		}
	}

	previous = cg.lowAmmoWarning;

	if ( total == 0 ) {
		cg.lowAmmoWarning = 2;
	} else {
		cg.lowAmmoWarning = 1;
	}

	// play a sound on transitions
	if ( cg.lowAmmoWarning != previous ) {
		trap_S_StartLocalSound( cgs.media.noAmmoSound, CHAN_LOCAL_SOUND );
	}
*/
}

/*
==============
CG_DamageFeedback
==============
*/
void CG_DamageFeedback( int yawByte, int pitchByte, int damage )
{
	float	left, front, up;
	float	kick;
	int	health;
	float	scale;
	vec3_t	dir;
	vec3_t	angles;
	float	dist;
	float	yaw, pitch;

	// show the attacking player's head and name in corner
	cg.attackerTime = cg.time;

	// the lower on health you are, the greater the view kick will be
	health = cg.snap->ps.stats[STAT_HEALTH];

	if (health < 40)
	{
		scale = 1;
	}
	else
	{
		scale = 40.0 / health;
	}
	kick = damage * scale;

	if (kick < 5)
	{
		kick = 5;
	}

	if (kick > 10)
	{
		kick = 10;
	}

	// if yaw and pitch are both 255, make the damage always centered (falling, etc)
	if ((yawByte == 255) && (pitchByte == 255))
	{
		cg.damageX     = 0;
		cg.damageY     = 0;
		cg.v_dmg_roll  = 0;
		cg.v_dmg_pitch = -kick;
	}
	else
	{
		// positional
		pitch	      = pitchByte / 255.0 * 360;
		yaw	      = yawByte / 255.0 * 360;

		angles[PITCH] = pitch;
		angles[YAW]   = yaw;
		angles[ROLL]  = 0;

		AngleVectorsF( angles, dir, NULL, NULL );
		VectorSubtract( vec3_origin, dir, dir );

		front  = DotProduct (dir, cg.refdef.viewaxis[0] );
		left   = DotProduct (dir, cg.refdef.viewaxis[1] );
		up     = DotProduct (dir, cg.refdef.viewaxis[2] );

		dir[0] = front;
		dir[1] = left;
		dir[2] = 0;
		dist   = VectorLength( dir );

		if (dist < 0.1)
		{
			dist = 0.1f;
		}

		cg.v_dmg_roll  = kick * left;

		cg.v_dmg_pitch = -kick * front;

		if (front <= 0.1)
		{
			front = 0.1f;
		}
		cg.damageX = -left / front;
		cg.damageY = up / dist;
	}

	// clamp the position
	if (cg.damageX > 1.0)
	{
		cg.damageX = 1.0;
	}

	if (cg.damageX < -1.0)
	{
		cg.damageX = -1.0;
	}

	if (cg.damageY > 1.0)
	{
		cg.damageY = 1.0;
	}

	if (cg.damageY < -1.0)
	{
		cg.damageY = -1.0;
	}

	// don't let the screen flashes vary as much
	if (kick > 10)
	{
		kick = 10;
	}
	cg.damageValue = kick;
	cg.v_dmg_time  = cg.time + DAMAGE_TIME;
	cg.damageTime  = cg.snap->serverTime;
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name       : CG_Respawn
// Description: Handles a respawn event
// Author     : Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
void CG_Respawn( void )
{
	int  i;

	//27 fix: make sure the count goes up
	crosshairRGBModificationCount++;
	crosshairFriendRGBModificationCount++;
	scopeRGBModificationCount++;
	scopeFriendRGBModificationCount++;
//    armbandModCount++;

	//	trap_Cvar_Set( "teamoverlay", "1" );

	// no error decay on player movement
	cg.thisFrameTeleport = qtrue;

	// select the weapon the server says we are using
	cg.weaponSelect 	= utPSGetWeaponID ( &cg.snap->ps );

	cg.predictedPlayerState = cg.snap->ps;

	cg.itemSelect		= 0;

	for (i = 0; i < UT_ITEM_MAX; i++)
	{
		if (UT_ITEM_GETID(cg.snap->ps.itemData[i]))
		{
			cg.itemSelect = UT_ITEM_GETID(cg.snap->ps.itemData[i]);
			break;
		}
	}

	// display weapons available
	cg.weaponSelectTime = 0;

	CG_SetViewAnimation ( WEAPON_IDLE );

	memset ( &cg.bleedingFrom, 0, sizeof(cg.bleedingFrom));
	cg.bleedingFromCount	  = 0;

	cg.overlayColorTime	  = 0;
	cg.overlayShaderTime	  = 0;
	cg.overlayFade		  = 0.0f;
	cg.overlayColorFadeTime   = 0;
	cg.OwnedType		  = 0;
	//cg.overlayColorFade = UT_OCFADE_LINEAR_IN;
	cg.grensmokecolor[3]	  = 0;
	cg.zoomed		  = 0;
	cg.zoomedFrom		  = 0;
	cg.DefuseOverlay	  = qfalse;
	cg.BombDefused		  = qfalse;
	cg.DefuseStartTime	  = 0;
	cg.DefuseEndTime	  = 0;
	cg.survivorRoundWinner[0] = 0;

	cg.weaponHidden 	  = qtrue;

	trap_S_StopLoopingSound(cg.predictedPlayerState.clientNum ); // stop underwater sound
	cg.playingWaterSFX = qfalse;

	//clear any music
	trap_S_StopBackgroundTrack();

	// Update client side skin selection on respawn
	CG_UpdateSkins(0);
}

extern char  *eventnames[];

/*
==============
CG_CheckPlayerstateEvents
==============
*/
void CG_CheckPlayerstateEvents( playerState_t *ps, playerState_t *ops )
{
	int	   i;
	int	   event;
	centity_t  *cent;

	if (ps->externalEvent && (ps->externalEvent != ops->externalEvent))
	{
		cent			     = CG_ENTITIES(ps->clientNum);
		cent->currentState.event     = ps->externalEvent;
		cent->currentState.eventParm = ps->externalEventParm;
		CG_EntityEvent( cent, cent->lerpOrigin );
	}
	cent = &cg.predictedPlayerEntity; // cg_entities[ ps->clientNum ];

	// go through the predictable events buffer
	for (i = ps->eventSequence - MAX_PS_EVENTS ; i < ps->eventSequence ; i++)
	{
		// if we have a new predictable event
		if ((i >= ops->eventSequence)
		    // or the server told us to play another event instead of a predicted event we already issued
		    // or something the server told us changed our prediction causing a different event
		    || ((i > ops->eventSequence - MAX_PS_EVENTS) && (ps->events[i & (MAX_PS_EVENTS - 1)] != ops->events[i & (MAX_PS_EVENTS - 1)])))
		{
			event			     = ps->events[i & (MAX_PS_EVENTS - 1)];
			cent->currentState.event     = event;
			cent->currentState.eventParm = ps->eventParms[i & (MAX_PS_EVENTS - 1)];
			CG_EntityEvent( cent, cent->lerpOrigin );

			cg.predictableEvents[i & (MAX_PREDICTED_EVENTS - 1)] = event;

			cg.eventSequence++;
		}
	}
}

/*
==================
CG_CheckChangedPredictableEvents
==================
*/
void CG_CheckChangedPredictableEvents( playerState_t *ps )
{
	int	   i;
	int	   event;
	centity_t  *cent;

	cent = &cg.predictedPlayerEntity;

	for (i = ps->eventSequence - MAX_PS_EVENTS ; i < ps->eventSequence ; i++)
	{
		//
		if (i >= cg.eventSequence)
		{
			continue;
		}

		// if this event is not further back in than the maximum predictable events we remember
		if (i > cg.eventSequence - MAX_PREDICTED_EVENTS)
		{
			// if the new playerstate event is different from a previously predicted one
			if (ps->events[i & (MAX_PS_EVENTS - 1)] != cg.predictableEvents[i & (MAX_PREDICTED_EVENTS - 1)])
			{
				event			     = ps->events[i & (MAX_PS_EVENTS - 1)];
				cent->currentState.event     = event;
				cent->currentState.eventParm = ps->eventParms[i & (MAX_PS_EVENTS - 1)];
				CG_EntityEvent( cent, cent->lerpOrigin );

				cg.predictableEvents[i & (MAX_PREDICTED_EVENTS - 1)] = event;

				if (cg_showmiss.integer)
				{
					CG_Printf("WARNING: changed predicted event\n");
				}
			}
		}
	}
}

#if 0
/*
==================
pushReward
==================
*/
static void pushReward(sfxHandle_t sfx, qhandle_t shader, int rewardCount)
{
	if (cg.rewardStack < (MAX_REWARDSTACK - 1))
	{
		cg.rewardStack++;
		cg.rewardSound[cg.rewardStack]	= sfx;
		cg.rewardShader[cg.rewardStack] = shader;
		cg.rewardCount[cg.rewardStack]	= rewardCount;
	}
}
#endif

/*
==================
CG_CheckLocalSounds
==================
*/
void CG_CheckLocalSounds( playerState_t *ps, playerState_t *ops )
{
	// don't play the sounds if the player just changed teams
	if (ps->persistant[PERS_TEAM] != ops->persistant[PERS_TEAM])
	{
		return;
	}

/*
	// health changes of more than -1 should make pain sounds
	if ( ps->stats[STAT_HEALTH] < ops->stats[STAT_HEALTH] - 1 ) {
		if ( ps->stats[STAT_HEALTH] > 0 && (ops->stats[STAT_HEALTH] - ps->stats[STAT_HEALTH]) > 10) {
			CG_PainEvent( &cg.predictedPlayerEntity, ps->stats[STAT_HEALTH] );
		}
	}
*/

	// if we are going into the intermission, don't start any voices
	if (cg.intermissionStarted)
	{
		return;
	}

	// check for flag pickup
	if ((cgs.gametype >= GT_TEAM) && (cgs.gametype != GT_JUMP))
	{
		if (((utPSHasItem ( ps, UT_ITEM_REDFLAG ) != utPSHasItem ( ops, UT_ITEM_REDFLAG )) && utPSHasItem ( ps, UT_ITEM_REDFLAG )) ||
		    ((utPSHasItem ( ps, UT_ITEM_BLUEFLAG ) != utPSHasItem ( ops, UT_ITEM_BLUEFLAG )) && utPSHasItem ( ps, UT_ITEM_BLUEFLAG )) ||
		    ((utPSHasItem ( ps, UT_ITEM_NEUTRALFLAG ) != utPSHasItem ( ops, UT_ITEM_NEUTRALFLAG )) && utPSHasItem ( ps, UT_ITEM_NEUTRALFLAG )))
		{
//			trap_S_StartLocalSound( cgs.media.youHaveFlagSound, CHAN_ANNOUNCER );
		}
	}
}

/*
===============
CG_TransitionPlayerState

===============
*/
void CG_TransitionPlayerState( playerState_t *ps, playerState_t *ops )
{
	int  old, news;

	// check for changing follow mode
	if ((ps->clientNum != ops->clientNum) && !(ops->pm_flags & PMF_FOLLOW))     // DensitY: client fix for cg_respawn bug
	{
		cg.thisFrameTeleport = qtrue;
		// make sure we don't get any unwanted transition effects
		*ops		     = *ps;
	}

	//Make sure the handskins are set

	// Damage events (player is getting wounded),  only display if we're not switching between clients.
	if ((ps->clientNum == ops->clientNum) && (ps->damageEvent != ops->damageEvent) && ps->damageCount)
	{
		CG_DamageFeedback( ps->damageYaw, ps->damagePitch, ps->damageCount );
	}

	// Kill the bandage sounds
	if (!(ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_BANDAGING) && (ops->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_BANDAGING))
	{
		trap_S_StopLoopingSound ( ps->clientNum );
	}

	// respawning
	if (ps->persistant[PERS_SPAWN_COUNT] != ops->persistant[PERS_SPAWN_COUNT])
	{
		CG_Respawn();
	}
	else if (cg.mapRestart)
	{
		CG_Respawn();
		cg.mapRestart = qfalse;
	}

	// Look for weapon mode change
	old  = utPSGetWeaponMode ( ops );
	news = utPSGetWeaponMode ( ps );

	if ((old != news) && (utPSGetWeaponID ( ops ) == utPSGetWeaponID ( ps )))
	{
		CG_WeaponMode ( utPSGetWeaponMode ( ops ));
	}

/*
	else if( ps->weaponslot != ops->weaponslot && ps->weaponslot != -1 )
	{
		cg.weaponSelect = utPSGetWeaponID ( ps );
		CG_SetViewAnimation ( WEAPON_IDLE );
	}
*/

	// Clear the hud if we were bleeding or limping and arent anymore
	if (cg.bleedingFromCount) {

		if (!((ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_BLEEDING) || (ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_LIMPING))) {
			memset ( &cg.bleedingFrom, 0, sizeof(cg.bleedingFrom));
			cg.bleedingFromCount = 0;
		}

	}

	if ((cg.snap->ps.pm_type != PM_INTERMISSION) &&
	    ((ps->persistant[PERS_TEAM] != TEAM_SPECTATOR) ||
	     (ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_GHOST)))
	{
		CG_CheckLocalSounds( ps, ops );
	}

	// check for going low on ammo
	CG_CheckAmmo();

	// run events
	CG_CheckPlayerstateEvents( ps, ops );

	// smooth the ducking viewheight change
	if (ps->viewheight != ops->viewheight)
	{
		cg.duckChange = ps->viewheight - ops->viewheight;
		cg.duckTime   = cg.time;
	}
}
