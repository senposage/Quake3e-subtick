// Copyright (C) 1999-2000 Id Software, Inc.
//
// cg_view.c -- setup all the parameters (position, angle, etc)
// for a 3D rendering
#include "cg_local.h"

// for precipitation
static precip_t  drop[UTPRECIP_MAX_DROPS];

static float	 sky_start;

#define MiniMapSize 512
#define RainMapSize 256
#define BODYSIZE    (MiniMapSize * MiniMapSize * 4)

static unsigned char  RainMap[RainMapSize * RainMapSize];
extern vec3_t	      bg_colors[20];

/*
=================
CG_CalcVrect

Sets the coordinates of the rendered window
=================
*/
static void CG_CalcVrect (void)
{
	int  size;

	// the intermission should allways be full screen
	if (cg.snap->ps.pm_type == PM_INTERMISSION)
	{
		size = 100;
	}
	else
	{
		// bound normal viewsize
		if (cg_viewsize.integer < 30)
		{
			trap_Cvar_Set ("cg_viewsize", "30");
			size = 30;
		}
		else if (cg_viewsize.integer > 100)
		{
			trap_Cvar_Set ("cg_viewsize", "100");
			size = 100;
		}
		else
		{
			size = cg_viewsize.integer;
		}
	}
	cg.refdef.width   = cgs.glconfig.vidWidth * size / 100;
	cg.refdef.width  &= ~1;

	cg.refdef.height  = cgs.glconfig.vidHeight * size / 100;
	cg.refdef.height &= ~1;

	cg.refdef.x	  = (cgs.glconfig.vidWidth - cg.refdef.width) / 2;
	cg.refdef.y	  = (cgs.glconfig.vidHeight - cg.refdef.height) / 2;
}

//==============================================================================

/*
===============
CG_OffsetThirdPersonView

===============
*/
#define FOCUS_DISTANCE 512
/**
 * $(function)
 */
static void CG_OffsetThirdPersonView( void )
{
	vec3_t	       forward, right, up;
	vec3_t	       view;
	vec3_t	       focusAngles;
	trace_t        trace;
	static vec3_t  mins = { -4, -4, -4 };
	static vec3_t  maxs = { 4, 4, 4 };
	vec3_t	       focusPoint;
	float	       focusDist;
	float	       forwardScale, sideScale;
	float	       range;
	float	       angle;

	if (cg.snap && (cg.snap->ps.pm_flags & PMF_FOLLOW) && (cg.snap->ps.eFlags & EF_CHASE))
	{
		range = 80.0f;
		angle = 0;
		angle = cg_thirdPersonAngle.value;
	}
	else
	{
		range = cg_thirdPersonRange.value;
		angle = cg_thirdPersonAngle.value;
	}

	cg.refdef.vieworg[2] += cg.predictedPlayerState.viewheight;

	VectorCopy( cg.refdefViewAngles, focusAngles );

	// if dead, look at killer
	if (cg.predictedPlayerState.stats[STAT_HEALTH] <= 0)
	{
//		focusAngles[YAW] = cg.predictedPlayerState.stats[STAT_DEAD_YAW];
//		cg.refdefViewAngles[YAW] = cg.predictedPlayerState.stats[STAT_DEAD_YAW];
	}

	if (focusAngles[PITCH] > 45)
	{
		focusAngles[PITCH] = 45;		// don't go too far overhead
	}
	AngleVectorsF( focusAngles, forward, NULL, NULL );

	VectorMA( cg.refdef.vieworg, FOCUS_DISTANCE, forward, focusPoint );

	VectorCopy( cg.refdef.vieworg, view );

	view[2] 		   += 8;

	cg.refdefViewAngles[PITCH] *= 0.5;

	AngleVectors( cg.refdefViewAngles, forward, right, up );

	forwardScale = cos( angle / 180 * M_PI );
	sideScale    = sin( angle / 180 * M_PI );
	VectorMA( view, -range * forwardScale, forward, view );
	VectorMA( view, -range * sideScale, right, view );

	// trace a ray from the origin to the viewpoint to make sure the view isn't
	// in a solid block.  Use an 8 by 8 block to prevent the view from near clipping anything

	if (!cg_cameraMode.integer)
	{
		CG_Trace( &trace, cg.refdef.vieworg, mins, maxs, view, cg.predictedPlayerState.clientNum, MASK_SOLID );

		if (trace.fraction != 1.0)
		{
			VectorCopy( trace.endpos, view );
			view[2] += (1.0 - trace.fraction) * 32;
			// try another trace to this position, because a tunnel may have the ceiling
			// close enogh that this is poking out

			CG_Trace( &trace, cg.refdef.vieworg, mins, maxs, view, cg.predictedPlayerState.clientNum, MASK_SOLID );
			VectorCopy( trace.endpos, view );
		}
	}

	VectorCopy( view, cg.refdef.vieworg );

	// select pitch to look at focus point from vieword
	VectorSubtract( focusPoint, cg.refdef.vieworg, focusPoint );
	focusDist = sqrt( focusPoint[0] * focusPoint[0] + focusPoint[1] * focusPoint[1] );

	if (focusDist < 1)
	{
		focusDist = 1;	// should never happen
	}
	cg.refdefViewAngles[PITCH] = -180 / M_PI *atan2( focusPoint[2], focusDist );
	cg.refdefViewAngles[YAW]  -= angle;
}

// this causes a compiler bug on mac MrC compiler
static void CG_StepOffset( void )
{
	int  timeDelta;

	// smooth out stair climbing
	timeDelta = cg.time - cg.stepTime;

	if (timeDelta < STEP_TIME)
	{
		cg.refdef.vieworg[2] -= cg.stepChange
					* (STEP_TIME - timeDelta) / STEP_TIME;
	}
}

/*
===============
CG_OffsetFirstPersonView

===============
*/
static void CG_OffsetFirstPersonView( void )
{
	float	*origin;
	float	*angles;
	float	bob;
	float	ratio;
	float	delta;
	float	speed;
	float	f;
	vec3_t	predictedVelocity;
	int	timeDelta;

	if (cg.snap->ps.pm_type == PM_INTERMISSION)
	{
		return;
	}

	origin = cg.refdef.vieworg;
	angles = cg.refdefViewAngles;

	// if dead, fix the angle and don't add any kick
	if ((cg.snap->ps.pm_type == PM_NORMAL) && (cg.snap->ps.stats[STAT_HEALTH] <= 0))
	{
		angles[ROLL]  = 40;
		angles[PITCH] = -15;
		angles[YAW]   = (cg.time / 100) % 360;
//gles[YAW] = cg.snap->ps.stats[STAT_DEAD_YAW];
		origin[2]    += cg.predictedPlayerState.viewheight;
		return;
	}

	// add angles based on weapon kick
	VectorAdd (angles, cg.kick_angles, angles);

	// add angles based on damage kick
	if (cg.damageTime)
	{
		ratio = cg.time - cg.damageTime;

		if (ratio < DAMAGE_DEFLECT_TIME)
		{
			ratio	      /= DAMAGE_DEFLECT_TIME;
			angles[PITCH] += ratio * cg.v_dmg_pitch;
			angles[ROLL]  += ratio * cg.v_dmg_roll;
		}
		else
		{
			ratio = 1.0 - (ratio - DAMAGE_DEFLECT_TIME) / DAMAGE_RETURN_TIME;

			if (ratio > 0)
			{
				angles[PITCH] += ratio * cg.v_dmg_pitch;
				angles[ROLL]  += ratio * cg.v_dmg_roll;
			}
		}
	}

	// add pitch based on fall kick
#if 0
	ratio = (cg.time - cg.landTime) / FALL_TIME;

	if (ratio < 0)
	{
		ratio = 0;
	}
	angles[PITCH] += ratio * cg.fall_value;
#endif

	// add angles based on velocity
	VectorCopy( cg.predictedPlayerState.velocity, predictedVelocity );

	delta	       = DotProduct ( predictedVelocity, cg.refdef.viewaxis[0]);
	angles[PITCH] += delta * cg_runpitch.value;

	delta	       = DotProduct ( predictedVelocity, cg.refdef.viewaxis[1]);
	angles[ROLL]  -= delta * cg_runroll.value;

	// add angles based on bob

	// make sure the bob is visible even at low speeds
	speed = cg.xyspeed > 200 ? cg.xyspeed : 200;

	delta = cg.bobfracsin * cg_bobpitch.value * speed;

	if (cg.predictedPlayerState.pm_flags & PMF_DUCKED)
	{
		delta *= 2;		// crouching
	}
	angles[PITCH] += delta;
	delta	       = cg.bobfracsin * cg_bobroll.value * speed;

	if (cg.predictedPlayerState.pm_flags & PMF_DUCKED)
	{
		delta *= 2;		// crouching accentuates roll
	}

	if (cg.bobcycle & 1)
	{
		delta = -delta;
	}
	angles[ROLL] += delta;

	// Add the recoil pitch in there.
	f	       = (float)cg.predictedPlayerState.stats[STAT_RECOIL];
	f	       = ((f / 100.0f) * M_PI / 180.0f);

	bob	       = utPSGetWeaponID(&cg.predictedPlayerState);
	bob	       = utPSGetWeaponMode(&cg.predictedPlayerState);
	f	       = (f / (((UT_MAX_RECOIL / 100.0f) * M_PI / 180.0f)) * bg_weaponlist[utPSGetWeaponID(&cg.predictedPlayerState)].modes[utPSGetWeaponMode(&cg.predictedPlayerState)].recoil.rise);
	angles[PITCH] -= f;

	if (utPSGetWeaponBullets ( &cg.predictedPlayerState ) % 2)
	{
		angles[YAW] += (f / 12);	// was 8 in 2.6a
	}
	else
	{
		angles[YAW] -= (f / 12);	// was 8 in 2.6a
	}
//===================================

	// add view height
	origin[2] += cg.predictedPlayerState.viewheight;

	// smooth out duck height changes
	timeDelta = cg.time - cg.duckTime;

	if (timeDelta < DUCK_TIME)
	{
		cg.refdef.vieworg[2] -= cg.duckChange
					* (DUCK_TIME - timeDelta) / DUCK_TIME;
	}

	// add bob height
	bob = cg.bobfracsin * cg.xyspeed * cg_bobup.value;

	if (bob > 6)
	{
		bob = 6;
	}

	origin[2] += bob;

	// add fall height
	delta = cg.time - cg.landTime;

	if (delta < LAND_DEFLECT_TIME)
	{
		f		      = delta / LAND_DEFLECT_TIME;
		cg.refdef.vieworg[2] += cg.landChange * f;
	}
	else if (delta < LAND_DEFLECT_TIME + LAND_RETURN_TIME)
	{
		delta		     -= LAND_DEFLECT_TIME;
		f		      = 1.0 - (delta / LAND_RETURN_TIME);
		cg.refdef.vieworg[2] += cg.landChange * f;
	}

	// add step offset
	CG_StepOffset();

	// add kick offset

	VectorAdd (origin, cg.kick_origin, origin);

	// pivot the eye based on a neck length
#if 0
	{
 #define NECK_LENGTH 8
		vec3_t	forward, up;

		cg.refdef.vieworg[2] -= NECK_LENGTH;
		AngleVectors( cg.refdefViewAngles, forward, NULL, up );
		VectorMA( cg.refdef.vieworg, 3, forward, cg.refdef.vieworg );
		VectorMA( cg.refdef.vieworg, NECK_LENGTH, up, cg.refdef.vieworg );
	}
#endif
}

/*
====================
CG_CalcFov

Fixed fov at intermissions, otherwise account for fov variable and zooms.
====================
*/
#define WAVE_AMPLITUDE 1
#define WAVE_FREQUENCY 0.4

/**
 * $(function)
 */
static int CG_CalcFov( void )
{
	float  x;
	float  phase;
	float  v;
	int    contents;
	float  fov_x, fov_y;
	float  zoomTo;
	float  zoomFrom;
	float  zoomLerp;
	int    inwater;
	//		Added 2.6a
	int    ZoomTime;

	if (cg.predictedPlayerState.pm_type == PM_INTERMISSION)
	{
		// if in intermission, use a fixed value
		fov_x = 90;
	}
	else
	{
		// user selectable
		{
			fov_x = cg_fov.value;

#ifndef _DEBUG
			//@Barbatos: if we are watching a demo, allow to have customizable fov
			if(cg.demoPlayback)
			{
				fov_x = cg_demoFov.value;

				// restrict a bit the fov
				if (fov_x < 70)
				{
					fov_x = 70;
					trap_Cvar_Set ( "cg_demoFOV", "70");
				}
				else if (fov_x > 140)
				{
					fov_x = 140;
					trap_Cvar_Set ( "cg_demoFOV", "140");
				}
			}
			else
			{
				fov_x = cg_fov.value;
				if (fov_x < 90)
				{
					trap_Cvar_Set ( "cg_fov", "90" );
					fov_x = 90;
				}
				else if (fov_x > 110)
				{
					trap_Cvar_Set ( "cg_fov", "110" );
					fov_x = 110;
				}
			}
#endif
		}
//--

		///if we're ghosting someone or its a demo playback (ie, ghosting ourselves)
		if ((cg.clientNum != cg.predictedPlayerState.clientNum) || (cg.demoPlayback))
		{
			if ((cg.predictedPlayerState.stats[STAT_BUTTONS] & (BUTTON_ZOOM1)) &&
			    (cg.predictedPlayerState.stats[STAT_BUTTONS] & (BUTTON_ZOOM2)))
			{
				cg.zoomedFrom = 3;
				cg.zoomed     = 3;
			}
			else
			if (cg.predictedPlayerState.stats[STAT_BUTTONS] & (BUTTON_ZOOM1))
			{
				cg.zoomedFrom = 1;
				cg.zoomed     = 1;
			}
			else
			if (cg.predictedPlayerState.stats[STAT_BUTTONS] & (BUTTON_ZOOM2))
			{
				cg.zoomedFrom = 2;
				cg.zoomed     = 2;
			}
			else
			{
				cg.zoomed     = 0;
				cg.zoomedFrom = 0;
			}
		}

		//if we are spectating ourselves after our zoomed host disconnected
		if ((cg.predictedPlayerState.persistant[PERS_TEAM] == TEAM_SPECTATOR) &&
		    (cg.clientNum == cg.predictedPlayerState.clientNum))
		{
			cg.zoomed     = 0;
			cg.zoomedFrom = 0;
		}

		//if our gun does not support zoom
		if(bg_weaponlist[utPSGetWeaponID( &cg.predictedPlayerState )].zoomfov[cg.zoomed - 1] == 0)
		{
			cg.zoomed     = 0;
			cg.zoomedFrom = 0;
		}

//--
		// if we're a speccy and not following, no zooming
		if (!(cg.snap->ps.pm_flags & PMF_FOLLOW) && (cg.clientNum != cg.predictedPlayerState.clientNum))
		{
			cg.zoomed     = 0;
			cg.zoomedFrom = 0;
		}

		// Determine the FOV the client is zooming to, if not zoomed it is the standard fov
		if (cg.zoomed > 0)
		{
			zoomTo = bg_weaponlist[utPSGetWeaponID(&cg.predictedPlayerState)].zoomfov[cg.zoomed - 1];
		}
		else
		{
			zoomTo = fov_x;
		}

		// Determine the FOV the client is zooming from, if not zoomed it is the standard fov
		if (cg.zoomedFrom > 0)
		{
			zoomFrom = bg_weaponlist[utPSGetWeaponID(&cg.predictedPlayerState)].zoomfov[cg.zoomedFrom - 1];
		}
		else
		{
			zoomFrom = fov_x;
		}

		// Determine the lerp between the from and to zoom fov values

		// DENSITY: added increase server side for sr8 so we'll also do it graphically client side
		// UPDATE: reset because of new firing style for sr8!

//		if( utPSGetWeaponID( &cg.predictedPlayerState ) == UT_WP_SR8 ) {
//			ZoomTime = 215;
//		}
//		else {
		ZoomTime = ZOOM_TIME;
//		}
		zoomLerp = (cg.time - cg.zoomTime) / (float)ZoomTime;

		// If still lerping then calculate the lerp fov, otherwise just use the to fov
		if (zoomLerp < 1.0f)
		{
			fov_x = zoomFrom + (zoomTo - zoomFrom) * zoomLerp;
		}
		else
		{
			fov_x = zoomTo;
		}
	}

	x     = cg.refdef.width / tan( fov_x / 360 * M_PI );
	fov_y = atan2( cg.refdef.height, x );
	fov_y = fov_y * 360 / M_PI;

	// warp if underwater
	contents = CG_PointContents( cg.refdef.vieworg, -1 );

	if (contents & (CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA))
	{
		phase	= cg.time / 1000.0 * WAVE_FREQUENCY * M_PI * 2;
		v	= WAVE_AMPLITUDE * sin( phase );
		fov_x  += v;
		fov_y  -= v;
		inwater = qtrue;
	}
	else
	{
		inwater = qfalse;
	}

	// set it
	cg.refdef.fov_x = fov_x;
	cg.refdef.fov_y = fov_y;

	if (cg.zoomed <= 0)
	{
		cg.zoomSensitivity = 1;
	}
	else
	{
		cg.zoomSensitivity = cg.refdef.fov_y / 75.0;
	}

	return inwater;
}

/*
===============
CG_DamageBlendBlob

===============
*/
static void CG_DamageBlendBlob( void )
{
	int	     t;
	int	     maxTime;
	refEntity_t  ent;

	if (!cg.damageValue)
	{
		return;
	}

	if (!cg_viewBlob.integer)
	{
		return;
	}

	// ragePro systems can't fade blends, so don't obscure the screen
	if (cgs.glconfig.hardwareType == GLHW_RAGEPRO)
	{
		return;
	}

	maxTime = DAMAGE_TIME;
	t	= cg.time - cg.damageTime;

	if ((t <= 0) || (t >= maxTime))
	{
		return;
	}

	memset( &ent, 0, sizeof(ent));
	ent.reType   = RT_SPRITE;
	ent.renderfx = RF_FIRST_PERSON;

	VectorMA( cg.refdef.vieworg, 8, cg.refdef.viewaxis[0], ent.origin );
	VectorMA( ent.origin, cg.damageX * -8, cg.refdef.viewaxis[1], ent.origin );
	VectorMA( ent.origin, cg.damageY * 8, cg.refdef.viewaxis[2], ent.origin );

	ent.radius	  = cg.damageValue * 3;
	ent.customShader  = cgs.media.viewBloodShader;
	ent.shaderRGBA[0] = 255;
	ent.shaderRGBA[1] = 255;
	ent.shaderRGBA[2] = 255;
	ent.shaderRGBA[3] = 200 * (1.0 - ((float)t / maxTime));
	trap_R_AddRefEntityToSceneX( &ent );
}

/*
===============
CG_CalcViewValues

Sets cg.refdef view values
===============
*/
static int CG_CalcViewValues( void )
{
	playerState_t  *ps;
	int	       intensity;

	memset( &cg.refdef, 0, sizeof(cg.refdef));

	// strings for in game rendering
	// Q_strncpyz( cg.refdef.text[0], "Park Ranger", sizeof(cg.refdef.text[0]) );
	// Q_strncpyz( cg.refdef.text[1], "19", sizeof(cg.refdef.text[1]) );

	// calculate size of 3D view
	CG_CalcVrect();

	ps = &cg.predictedPlayerState;

	// intermission view
	if (ps->pm_type == PM_INTERMISSION)
	{
		VectorCopy( ps->origin, cg.refdef.vieworg );
		VectorCopy( ps->viewangles, cg.refdefViewAngles );
		AnglesToAxis( cg.refdefViewAngles, cg.refdef.viewaxis );
		return CG_CalcFov();
	}

	cg.bobcycle   = (ps->bobCycle & 128) >> 7;
	cg.bobfracsin = fabs( sin((ps->bobCycle & 127) / 127.0 * M_PI ));
	cg.xyspeed    = sqrt( ps->velocity[0] * ps->velocity[0] +
			      ps->velocity[1] * ps->velocity[1] );

	VectorCopy( ps->origin, cg.refdef.vieworg );
	VectorCopy( ps->viewangles, cg.refdefViewAngles );

	if (cg_cameraOrbit.integer)
	{
		if (cg.time > cg.nextOrbitTime)
		{
			cg.nextOrbitTime	   = cg.time + cg_cameraOrbitDelay.integer;
			cg_thirdPersonAngle.value += cg_cameraOrbit.value;
		}
	}

	// add error decay
	if (cg_errorDecay.value > 0)
	{
		int    t;
		float  f;

		t = cg.time - cg.predictedErrorTime;
		f = (cg_errorDecay.value - t) / cg_errorDecay.value;

		if ((f > 0) && (f < 1))
		{
			VectorMA( cg.refdef.vieworg, f, cg.predictedError, cg.refdef.vieworg );
		}
		else
		{
			cg.predictedErrorTime = 0;
		}
	}

	// Screen shake for grens
	if (cg.screenshaketime - cg.time > 0)
	{
		intensity		= (int)(cg.screenshaketime - cg.time);
		cg.refdefViewAngles[0] += ((rand() % intensity) - (intensity / 2)) / 200;
		cg.refdefViewAngles[1] += ((rand() % intensity) - (intensity / 2)) / 200;
	}

	if (cg.renderingThirdPerson)
	{
		// back away from character
		CG_OffsetThirdPersonView();
	}
	else
	{
		// offset for local bobbing and kicks
		CG_OffsetFirstPersonView();
	}

	// position eye reletive to origin
	AnglesToAxis( cg.refdefViewAngles, cg.refdef.viewaxis );

	if (cg.hyperspace)
	{
		cg.refdef.rdflags |= RDF_NOWORLDMODEL | RDF_HYPERSPACE;
	}

	// field of view
	return CG_CalcFov();
}

/*
=====================
CG_AddBufferedSound
=====================
*/
void CG_AddBufferedSound( sfxHandle_t sfx )
{
	if (!sfx)
	{
		return;
	}
	cg.soundBuffer[cg.soundBufferIn] = sfx;
	cg.soundBufferIn		 = (cg.soundBufferIn + 1) % MAX_SOUNDBUFFER;

	if (cg.soundBufferIn == cg.soundBufferOut)
	{
		cg.soundBufferOut++;
	}
}

/*
=====================
CG_PlayBufferedSounds
=====================
*/
static void CG_PlayBufferedSounds( void )
{
	if (cg.soundTime < cg.time)
	{
		if ((cg.soundBufferOut != cg.soundBufferIn) && cg.soundBuffer[cg.soundBufferOut])
		{
			trap_S_StartLocalSound(cg.soundBuffer[cg.soundBufferOut], CHAN_ANNOUNCER);
			cg.soundBuffer[cg.soundBufferOut] = 0;
			cg.soundBufferOut		  = (cg.soundBufferOut + 1) % MAX_SOUNDBUFFER;
			cg.soundTime			  = cg.time + 750;
		}
	}
}

////////////////////////////////////////////////////////////

void CG_DrawBombFX3D(void)
{
	bombParticle_t	*particle;
	refEntity_t	re;
	vec3_t		angles;
	float		distance, radius, delta;
	int		deltaTime;
	int		seed, i;

	// If bomb didn't explode.
	if (!cg.bombExploded)
	{
		return;
	}

	CG_BombExplosionThink();

	// Calculate time delta.
	deltaTime = cg.time - cg.bombExplodeTime;

	// If bomb explosion time is over.
	if (deltaTime > 12000)
	{
		return;
	}

	// Calculate radius scale.
	radius = 0.75f * ((float)deltaTime / 20000.0f);

	// Draw particles.
	for (i = 0; i < MAX_BOMBPARTICLES; i++)
	{
		// Set particle pointer.
		particle = &cg.bombParticles[i];

		// If an invalid particle.
		if (!particle->active)
		{
			continue;
		}

		// Calculate colour scale.
		if (deltaTime < particle->fadeTime)
		{
			delta = 1.0f - (float)(1.0f + sin(M_PI + ((M_PI / 2) * (1.0f - ((float)deltaTime / (float)particle->fadeTime)))));
		}
		else
		{
			continue;
		}

		// Initialize.
		memset(&re, 0, sizeof(re));

		// Set type.
		re.reType = RT_MODEL;

		// Calculate position.
		VectorMA(particle->position, (float)deltaTime / 1000.0f, particle->velocity, re.origin);

		// Set model.
		re.hModel = cgs.media.sphereLoResModel;

		// Set shader.
		re.customShader = particle->shader;

		// Set colour.
		re.shaderRGBA[0] =  (unsigned char)(255.0f * (particle->colour[0] * delta));
		re.shaderRGBA[1] =  (unsigned char)(255.0f * (particle->colour[1] * delta));
		re.shaderRGBA[2] =  (unsigned char)(255.0f * (particle->colour[2] * delta));
		re.shaderRGBA[3] = 255;

		// Scaled model.
		re.nonNormalizedAxes = qtrue;

		// Calculate model axis.
		AnglesToAxis(particle->angles, re.axis);

		// Set scale.
		VectorScale(re.axis[0], particle->radius[0] + (particle->radius[0] * radius), re.axis[0]);
		VectorScale(re.axis[1], particle->radius[1] + (particle->radius[1] * radius), re.axis[1]);
		VectorScale(re.axis[2], particle->radius[2] + (particle->radius[2] * radius), re.axis[2]);

		// Add to render list.
		trap_R_AddRefEntityToSceneX(&re);
	}

	// Explosion cloud.
	memset(&re, 0, sizeof(re));

	// Set type.
	re.reType = RT_MODEL;

	// Set position.
	re.origin[0] = cg.bombExplosionOrigin[0];
	re.origin[1] = cg.bombExplosionOrigin[1];
	re.origin[2] = cg.bombExplosionOrigin[2] + cg.bombExplosionRadius;

	// Set 3D model.
	re.hModel = cgs.media.sphereHiResModel;

	// Set shader.
	re.customShader = cgs.media.bombCloudShader;

	// Calculate colour scale.
	delta = 1.0f - (float)(1.0f + sin(M_PI + ((M_PI / 2) * (1.0f - ((float)deltaTime / 12000.0f)))));

	// Set colour.
	re.shaderRGBA[0] =  (unsigned char)(191.0f * delta);
	re.shaderRGBA[1] =  (unsigned char)(191.0f * delta);
	re.shaderRGBA[2] =  (unsigned char)(191.0f * delta);
	re.shaderRGBA[3] = 255;

	// Scaled model.
	re.nonNormalizedAxes = qtrue;

	// Set seed number.
	seed = cg.bombExplodeTime;

	// Set orientation.
	VectorSet(angles, 0, 360.0f * Q_random(&seed), 0);

	// Calculate model axis.
	AnglesToAxis(angles, re.axis);

	// Calculate radius.
	radius = (cg.bombExplosionRadius * 0.5f) + ((cg.bombExplosionRadius * 1.5f) * ((float)deltaTime / 12000.0f));

	// Set scale.
	VectorScale(re.axis[0], radius, re.axis[0]);
	VectorScale(re.axis[1], radius, re.axis[1]);
	VectorScale(re.axis[2], radius / 100.0f, re.axis[2]);

	// Add to render list.
	trap_R_AddRefEntityToSceneX(&re);

	// If shockwave time is over.
	if (deltaTime > 5000)
	{
		return;
	}

	// Calculate distance.
	distance = Distance(cg.bombExplosionOrigin, cg.refdef.vieworg);

	// Calculate radius.
	radius = (float)deltaTime;

	// Calculate distance delta.
	delta = (float)fabs(distance - radius);

	// If shockwave front is close enough.
	if (delta < 1000.0f)
	{
		// Calculate shake time.
		delta = (4500.0f * (1.0f - (delta / 1000.0f))) * (1.0f - (radius / 5000.0f));

		// If current shake time is less then the calculated one.
		if ((cg.screenshaketime < cg.time) || ((cg.screenshaketime - cg.time) < delta))
		{
			cg.screenshaketime = cg.time + delta;
		}
	}

	// If shockwave has passed us.
	if (distance <= radius)
	{
		return;
	}

	// Explosion shockwave.
	memset(&re, 0, sizeof(re));

	// Set type.
	re.reType = RT_MODEL;

	// Set position.
	re.origin[0] = cg.bombExplosionOrigin[0];
	re.origin[1] = cg.bombExplosionOrigin[1];
	re.origin[2] = cg.bombExplosionOrigin[2];

	// Set 3D model.
	re.hModel = cgs.media.sphereHiResModel;

	// Calculate colour scale.
	delta = 1.0f - (float)(1.0f + sin(M_PI + ((M_PI / 2) * (1.0f - ((float)deltaTime / 5000.0f)))));

	// Set colour.
	re.shaderRGBA[0] =  (unsigned char)(191.0f * delta);
	re.shaderRGBA[1] =  (unsigned char)(191.0f * delta);
	re.shaderRGBA[2] =  (unsigned char)(191.0f * delta);
	re.shaderRGBA[3] = 255;

	// Scaled model.
	re.nonNormalizedAxes = qtrue;

	// Set seed number.
	seed = cg.bombExplodeTime;

	for (i = 0; i < 3; i++)
	{
		// Set shader.
		re.customShader = cgs.media.bombShockwaveShaders[i];

		// Set orientation.
		VectorSet(angles, Q_random(&seed) * 360.0f, Q_random(&seed) * 360.0f, Q_random(&seed) * 360.0f);

		// Calculate model axis.
		AnglesToAxis(angles, re.axis);

		// Calculate scale delta.
		delta = 1.0f + (0.1125f * ((float)i / 3.0f));

		// Set scale.
		VectorScale(re.axis[0], radius * delta, re.axis[0]);
		VectorScale(re.axis[1], radius * delta, re.axis[1]);
		VectorScale(re.axis[2], radius * delta, re.axis[2]);

		// Add to render list.
		trap_R_AddRefEntityToSceneX(&re);
	}
}

////////////////////////////////////////////////////////////

#ifdef ENCRYPTED
/* *** DO NOT INSERT NEW VARIABLES IN THIS BLOCK *** */
int hax_cvar_table[] = { // ptr_next,ret1
// completely useless table only used to read hax_cvars_found in non-obvious way
107,215,29,42,43,                        //   0     A
34 ,102,                                 //   5     G
5  ,111,211,41,150,128,                  //   7     F
49 ,117,69,92,114,112,220,114,45,        //  13     M
58 ,18,218,251,47,29,227,29,             //  22     O
117,248,246,188,                         //  30     S
71 ,162,122,86,                          //  34     H
55 ,45,53,235,111,16,44,142,228,162,240, //  38     D
22 ,87,226,78,33,112,                    //  49     N
7  ,49,183,                              //  55     E
89 ,79,127,195,216,                      //  58     P
30 ,135,175,92,28,29,178,123,            //  63     R
112,70,244,60,104,106,79,201,83,94,51,   //  71     I
13 ,180,217,172,16,37,156,               //  82     L
63 ,78,129,28,197,72,                    //  89     Q
38 ,148,90,                              //  95     C
82 ,180,224,197,175,94,109,67,4,         //  98     K
95 ,181,206,211,23,                      // 107     B
98 ,83,41,43,218,                        // 112     J
0};                                      // 117     T <<< THIS WILL OVERFLOW TO VARIABLES BELOW
int hax_cvars_found = 0;
/* *** END OF BLOCK *** */

// max 15 cvars
char *hax_cvar_names[] = {"cl_wallhack",     // 4.2.x hack cvars
                          "cl_aimbot",
                          "cl_aimbotFov",
                          "cl_aimAt",
                          "cl_removeSmoke",
                          "cl_removeScope",
                          "urth_autoaim",    // 4.1.1 urth cvars
                          "urth_autoshoot",
                          "urth_WallHack",
                         };


void CG_DoHaxCvarCheck()
{
 int i;
 char tmp1[16];
 char tmp2[16];
 if (hax_cvars_found) return; // if we detected something, keep the results and dont run it again
 for(i=0;i<sizeof(hax_cvar_names)/sizeof(hax_cvar_names[0]);i++) {
  strcpy(hax_cvar_names[i],tmp1);
  tmp2[0]=0;
  trap_Cvar_VariableStringBuffer(tmp1,tmp2,16);
  if (tmp2[0]) {
   hax_cvars_found++;
  }
 }

 if (hax_cvars_found) { // this must be called ONLY ONCE
  // It's very important to make sure this function looks complete, so there have to be some action.
  // We really don't want anybody to try to find that hax_cvars_found is actually read from somewhere else.
  // Let it look like we are sending rcon to server, that would normally result in log in server log file.

  char *m1 = "rcon set gg \"Admin, please, ban me! I'm aimbotter and wallhacker!\"\n"; // << Feel-free-to-insert-another-stupid-message-here
  trap_SendConsoleCommand(m1); // will never be called
 }
}
#endif

//=========================================================================

void CG_BombEffects (void)
{
	vec3_t	normal = { 0, 0, 0};
	vec3_t	origin;

	if(cg.time < cg.bombExplodeTime)
	{
		return;
	}

	cg.bombExplodeCount--;

	if(cg.bombExplodeCount <= 0)
	{
		cg.bombExplodeCount = 0;
		return;
	}

	cg.bombExplodeTime = cg.time + 250 + rand() % 500;

	VectorCopy ( cg.bombExplodeOrigin, origin );

	origin[0] += rand() % 40;
	origin[1] += rand() % 40;

	CG_MakeExplosion( origin,
			  normal,
			  250,
			  cgs.media.dishFlashModel,
			  cgs.media.grenadeExplosionShader,
			  600,
			  qtrue,
			  qtrue );

	trap_S_StartSound( origin, ENTITYNUM_WORLD, CHAN_AUTO, cgs.media.UT_explodeSound );
}

/*=---- Checks for Owned TS/FTL win and plays laughing sound ----=*/

static void CG_CheckOwnedSound(void)
{
	if(cg.IsOwnedSound == qtrue)
	{
		if(cg.time >= cg.OwnedSoundTimeStart)
		{
			trap_S_StartLocalSound( cgs.media.UT_LaughRound, CHAN_LOCAL_SOUND );
			cg.IsOwnedSound = qfalse;
		}
	}
}


int HashStr(const char *s)
{
	int a = 1234567;
	int b = 2345678;
	int c = 3456789;
	while(*s) {
		a=a+*s;
		b=a+b+c;
		c*=b;
		s++;
	}
	return b^a^c;
}


void CG_DoShaderDelayedLoad(void)
{
 unsigned int t0,scope;
#ifdef ENCRYPTED
 SendHaxValues(); // this will send status for last game played
 scope=0;
 do {
  t3 = hax_cvar_table[scope+1]*scope;
  scope = hax_cvar_table[scope];
 } while(scope);
#endif
 // r00t: Detection of wallhacks using shaders, called before first good frame
 // Idea here is that wallhack calls register shader during VM initialization and that means shader numbers
 // will be lower than "snow" shader (only used to get current shader nr).
 cgs.media.snowShader             = trap_R_RegisterShader( "snow" ); // hope that no one hates the snow

 scope                            = trap_R_RegisterShader( "gfx/crosshairs/scopes/scope" );
 cgs.media.viewsmokeshader        = trap_R_RegisterShader( "viewsmokepuff" );
// cgs.media.grenadeSmokePuffShader = trap_R_RegisterShader( "grenadeSmokePuff" );
#ifdef ENCRYPTED
 t0 = scope                            - cgs.media.snowShader;
 t1 = cgs.media.viewsmokeshader        - cgs.media.snowShader;
// t2 = cgs.media.grenadeSmokePuffShader - cgs.media.snowShader;

// CG_Printf("CGVIEW SHDR: %d %d %d %d\n",cgs.media.snowShader,scope,cgs.media.viewsmokeshader,cgs.media.grenadeSmokePuffShader);
// CG_Printf("T3=%d/%d\n",t3,((t3*16)/117));

 hax_detect_flags|= (t0>>31)|((t1>>30)&2)/*|((t2>>29)&4)*/|((t3*16)/117);
#endif
 for(t0=1;t0<sizeof(cg_weapons)/sizeof(cg_weapons[0]);t0++) {
  if (cg_weapons[t0].scopeShader[1] == -1) cg_weapons[t0].scopeShader[1] = scope;
 }
}


/*
=================
CG_DrawActiveFrame

Generates and draws a game scene and status information at the given time.
=================
*/
void CG_DrawActiveFrame( int serverTime, stereoFrame_t stereoView, qboolean demoPlayback )
{
	int    inwater;
	int    old;

	float  days, hours, mins, secs;

//	trap_SendConsoleCommand("/seta OGC_AIM 0\n");
//	trap_SendConsoleCommand("OGC_WALL 0\n");
//	trap_SendConsoleCommand("OGC_NAMES 0\n");
//	trap_SendConsoleCommand("OGC_WEAPONS 0\n");
//	trap_SendConsoleCommand("OGC_GLOW 0\n");

	//Time Chop
//   serverTime-=cg.timechop;
	///////////////////////

	cg.NumPlayersSnap = 0;
	cg.NumBombSites   = 0;
	cg.time 	  = serverTime;
	cg.demoPlayback   = demoPlayback;

	// update cvars
	CG_UpdateCvars();
	CG_ClearLaserSmoke();

	// vary gamma a little each client frame to get Q3
	// to reset to its r_gamma value incase an external
	// app has changed it
/*	{
		float gamma;
		char var[MAX_TOKEN_CHARS];
		static float oldgamma = 0.0f;

		// get current r_gamme
		trap_Cvar_VariableStringBuffer( "r_gamma", var, sizeof( var ) );
		gamma = atof( var );

		if ( fabs(oldgamma - gamma) != 0.001f )
		{
			if ( gamma <= 1.0f )
				oldgamma = gamma + 0.001f;
			else
				oldgamma = gamma - 0.001f;
		}
		else
		{
			trap_Cvar_Set( "r_gamma", va( "%f", oldgamma ) );
			oldgamma = gamma;
		}
	} */
	//TOTALLY uneeded.

	// if we are only updating the screen as a loading
	// pacifier, don't even try to read snapshots
	if (cg.infoScreenText[0] != 0)
	{
		const char *s;
		int gameVersion;

		s = CG_ConfigString( CS_GAME_VERSION_UPDATER );
		gameVersion = atoi( s );

		if (cg.demoPlayback && gameVersion != GAME_VERSION_UPDATER) { // @r00t: Don't allow playback of demos from different mod version
			const char *gv = CG_ConfigString(CS_GAME_VERSION);
			if (!gv) CG_Error("No mod version info for this demo");
			CG_Error(va("This demo requires Urban Terror mod version %s and can't be played on this version.\n\n"
				"You can download older versions on www.urbanterror.info download page.",gv));
		}

		//if((!cg.demoPlayback) || ((cg.demoPlayback) && (gameVersion < 41900)))
		{
			const char *s1,*s2,*s3,*s4;
			s1 = CG_ConfigString( CS_SERVERINFO );
			s2 = CG_ConfigString( CS_SYSTEMINFO );
			s3 = CG_ConfigString( CS_SCORES2 );
			s4 = CG_ConfigString( CS_GAME_VERSION );
/*
			trap_Print("SERVERINFO:");trap_Print(s1);trap_Print("\n");
			trap_Print("SYSTEMINFO:");trap_Print(s2);trap_Print("\n");
			trap_Print("CS_SCORES2:");trap_Print(s3);trap_Print("\n");
			trap_Print("CS_GAME_VERSION:");trap_Print(s4);trap_Print("\n");
*/
			// SERVERINFO is hardcoded in GTV, also some parts of SYSTEMINFO,
			// so first check for some basic fixed values...
			if (
				!strcmp(Info_ValueForKey( s1, "sv_hostname" ),"FonFon") &&
				!strcmp(Info_ValueForKey( s2, "fs_game" ),"gtv3") &&
				!strcmp(Info_ValueForKey( s2, "sv_serverid" ),"6343666") &&
				!strcmp(s3,"-9999") && !strcmp(s4,"baseq3-1")) {
					int h1;
					h1 = HashStr(s1);
//					CG_Printf("HASH: %d\n",h1);
					if (h1==-976752689) { // ... then if SERVERINFO is exactly same
//					CG_Printf("GTV detected\n");
						gameVersion = GAME_VERSION_UPDATER; // ... and if it's really GTV, fake the version check
					}
			}

			if( gameVersion != GAME_VERSION_UPDATER)
			{
				/* if WE are not up to date, quit and launch the updater */
				if( gameVersion > GAME_VERSION_UPDATER )
				{
					CG_Error( "Client/Server game mismatch: Your game is not up to date! Please run UrTUpdater or get the latest release on www.urbanterror.info");
				}

				/* the SERVER is not up to date, quit */
				else if( gameVersion < GAME_VERSION_UPDATER )
				{
					CG_Error( "Client/Server game mismatch: The server you're trying to connect to is not up to date. Please join another server.");
				}
			}
		}

		CG_DrawInformation();
		return;
	}

	// any looped sounds will be respecified as entities
	// are added to the render list
	trap_S_ClearLoopingSounds(qfalse);

	// clear all the render lists
	trap_R_ClearScene();

	//cg.time-=50;
	if (ut_timenudge.integer)
	{
		cg.time -= ut_timenudge.integer;
	}

	// set up cg.snap and possibly cg.nextSnap
	CG_ProcessSnapshots();

	// Update the survivor round start time.
	if (!cg.survivorRoundStart && cgs.survivor && !cg.warmup)
	{
		cg.survivorRoundStart = cg.time;
	}

	//
	CG_BuildBoundsOptimizer();
#ifdef ENCRYPTED
	if (!cg.clientFrame) CG_DoHaxCvarCheck(); //r00t: check for well known hack cvars
#endif
	// if we haven't received any snapshots yet, all
	// we can draw is the information screen
	if (!cg.snap || (cg.snap->snapFlags & SNAPFLAG_NOT_ACTIVE))
	{
		CG_DrawInformation();
		return;
	}

	// let the client system know what our weapon and zoom settings are

	trap_SetUserCmdValue( cg.weaponSelect, cg.zoomSensitivity );

	// update cg.predictedPlayerState
	CG_PredictPlayerState();

	// this counter will be bumped for every valid scene we generate
	cg.clientFrame++;
	cg.fpscounter++;

	if (!cgs.media.snowShader) CG_DoShaderDelayedLoad(); //r00t: wallhack check

	if (cg.time - cg.LastFpsPoll > 1000)
	{
		cg.LastFpsPoll = cg.time;
		cg.fpsnow      = cg.fpscounter;
		cg.fpscounter  = 0;
	}

	// decide on third person view
	// Dok: stay in 1st person for 5 seconds to see fade to black effect
//	if ( (cg.predictedPlayerState.pm_flags & PMF_LADDER_UPPER) || cg_thirdPerson.integer || ((cg.snap->ps.stats[STAT_HEALTH] <= 0)
	if (cg_thirdPerson.integer)
	{
		cg.renderingThirdPerson = qtrue;
	}
	else if (cg.snap->ps.pm_type == PM_DEAD || (cg.snap->ps.pm_type == PM_NORMAL && cg.snap->ps.stats[STAT_HEALTH] <= 0))
	{
		cg.lastDeathTime	= 0;
		cg.renderingThirdPerson = qtrue;
	}
	else if (cg.snap->ps.pm_flags & PMF_FOLLOW)
	{
		if (cg.snap->ps.eFlags & EF_CHASE)
		{
			cg.renderingThirdPerson = qtrue;
		}
		else
		{
			cg.renderingThirdPerson = qfalse;
		}
	}
	else
	{
		cg.renderingThirdPerson = qfalse;
	}

	// build cg.refdef
	inwater = CG_CalcViewValues();

	//If the player is alive, unfade us

	//if (!( cg.snap->ps.pm_type == PM_DEAD))
	{
		//    if (cg.overlayColorTime==0 && cg.overlayColorFade ==UT_OCFADE_LINEAR_OUT )
		{
//		CG_SetColorOverlay( 0, 0, 0,1, 5000, UT_OCFADE_LINEAR_IN, 500 );
		}
	}

	// first person blend blobs, done after AnglesToAxis
	if (!cg.renderingThirdPerson)
	{
		CG_DamageBlendBlob();
	}

	// build the render lists
	if (!cg.hyperspace)
	{
		if (cgs.antilagvis)
		{
			CG_DrawPersistantBBs();
		}
		CG_AddMarks();
		CG_AddLocalEntities();
		CG_AddPacketEntities(); 		// adter calcViewValues, so predicted player state is correct
		CG_AddPrecipitation();
	}

	CG_DrawOnScreenBombMarkers();

	CG_AddViewWeapon( &cg.predictedPlayerState );

	if (cg_servertime.integer)
	{
		CG_Printf("(%i) (%f) (!! %i)\n", cg.time, (float)(cg.time), cg.time - (float)(cg.time));
		secs  = cg.time / 1000;
		mins  = secs / 60;
		hours = mins / 60;
		days  = hours / 24;
		CG_Printf("%i %i %i %i\n", (int)days % 24, (int)hours % 24, (int)mins % 60, (int)secs % 60);
	}
#ifdef ENCRYPTED
UpdateHaxCounters();
#endif
//	if (cgs.media.quadShader-cgs.media.viewBloodShader!=1)
//	{
//		CG_Printf( "Cheat Detection 1\n" );
//	}
	// Not the most idea place
	CG_CheckOwnedSound();
	//if (cg.time >= cg.RoundFadeStart && cg.RoundFadeStart!=0)
	{
		//    cg.RoundFadeStart=0;
//	CG_SetColorOverlay( 0, 0, 0, 0, 5000, UT_OCFADE_LINEAR_OUT, 500 );
	}

	// add buffered sounds
	CG_PlayBufferedSounds();

	// play buffered voice chats
//	CG_PlayBufferedVoiceChats();

	cg.refdef.time = cg.time;
	memcpy( cg.refdef.areamask, cg.snap->areamask, sizeof(cg.refdef.areamask));

	// update audio positions
	trap_S_Respatialize( cg.snap->ps.clientNum, cg.refdef.vieworg, cg.refdef.viewaxis, inwater );

	// make sure the lagometerSample and frame timing isn't done twice when in stereo
	if (stereoView != STEREO_RIGHT)
	{
		cg.frametime = cg.time - cg.oldTime;

		if (cg.frametime < 0)
		{
			cg.frametime = 0;
		}
		cg.framecount++;
		cg.framesum += cg.frametime;
		cg.avgtime   = cg.framesum / cg.framecount;
		cg.oldTime   = cg.time;
		CG_AddLagometerFrameInfo();
	}

	if (cg_timescale.value != cg_timescaleFadeEnd.value)
	{
		if (cg_timescale.value < cg_timescaleFadeEnd.value)
		{
			cg_timescale.value += cg_timescaleFadeSpeed.value * ((float)cg.frametime) / 1000;

			if (cg_timescale.value > cg_timescaleFadeEnd.value)
			{
				cg_timescale.value = cg_timescaleFadeEnd.value;
			}
		}
		else
		{
			cg_timescale.value -= cg_timescaleFadeSpeed.value * ((float)cg.frametime) / 1000;

			if (cg_timescale.value < cg_timescaleFadeEnd.value)
			{
				cg_timescale.value = cg_timescaleFadeEnd.value;
			}
		}

		if (cg_timescaleFadeSpeed.value)
		{
			trap_Cvar_Set("timescale", va("%f", cg_timescale.value));
		}
	}

	//CG_BombEffects ( );

	// Draw bomb FX.
	CG_DrawBombFX3D();

	// actually issue the rendering calls
	CG_DrawActive( stereoView );

	old = cg.zoomed;

	if (old == -1)
	{
		old = 0;
	}
	// set command buttons so server can use it to construct zoom level
/*	if ( cg.zoomed == 1 )
		trap_SendConsoleCommand ( "+button12;" );
	else
		trap_SendConsoleCommand ( "-button12;" );

	if ( cg.zoomed == 2 )
		trap_SendConsoleCommand ( "+button13;" );
	else
		trap_SendConsoleCommand ( "-button13;" );

	if ( cg.zoomed== 3 )
		trap_SendConsoleCommand ( "+button14;" );
	else
		trap_SendConsoleCommand ( "-button14;" ); */

	switch (cg.zoomed)
	{
		case 1:
			{
				trap_SendConsoleCommand ( "+button13;" );
				trap_SendConsoleCommand ( "-button14;" );

				break;
			}

		case 2:
			{
				trap_SendConsoleCommand ( "-button13;" );
				trap_SendConsoleCommand ( "+button14;" );
				break;
			}

		case 3:
			{
				trap_SendConsoleCommand ( "+button13;" );
				trap_SendConsoleCommand ( "+button14;" );
				break;
			}

		default:
			{
				trap_SendConsoleCommand ( "-button13;" );
				trap_SendConsoleCommand ( "-button14;" );
				break;
			}
	}

	// If we're paused.
	if (cg.pauseState & UT_PAUSE_ON)
	{
		// Adjust times for pause.
		//cgs.gameid += cg.frametime; // Map start time.
		cg.pausedTime	   += cg.frametime;
		cgs.hotpotatotime  += cg.frametime;
		cgs.levelStartTime += cg.frametime; // Round start time.
		cgs.roundEndTime   += cg.frametime;
		cg.cahTime	   += cg.frametime;

		// If we're supposed to respawn.
		if (cg.respawnTime > cg.time)
		{
			cg.respawnTime += cg.frametime;
		}

		// If warmup count down is in progress.
		if (cg.warmup > 0)
		{
			cg.warmup += cg.frametime;
		}
	}

	CG_LimitCvars();
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name       : CG_ZoomTO
// Description: Zooms to a particular level
// Author     : Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
void CG_ZoomTo ( int zoom )
{
	if (cg.zoomed == zoom)
	{
		return;
	}

	// No zooming when following or in third person
	if ((cg.predictedPlayerState.pm_flags & PMF_FOLLOW) || cg_thirdPerson.integer)
	{
		return;
	}

	// Apoxol: No zoom when bandaging or reloading
	if((cg.predictedPlayerState.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_BANDAGING))
	{
		return;
	}

	if((cg.predictedPlayerState.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_CLIMBING))
	{
		return;
	}

	if((cg.predictedPlayerState.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_RELOADING))
	{
		return;
	}

	if((cg.predictedPlayerState.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_PLANTING))
	{
		return;
	}

	if((cg.predictedPlayerState.pm_flags & PMF_LADDER_UPPER))
	{
		return;
	}

	if((cg.predictedPlayerState.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_DEFUSING))
	{
		return;
	}

	// no zoom when specing or dead or ghost
	if (!(cg.predictedPlayerState.pm_type == PM_NORMAL) ||
	    (cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR) ||
	    (cg.predictedPlayerState.persistant[PERS_TEAM] == TEAM_SPECTATOR) ||
	    (cg.predictedPlayerState.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_GHOST))
	{
		return;
	}

	if (bg_weaponlist[utPSGetWeaponID(&cg.predictedPlayerState)].zoomfov[0] == 0)
	{
		return;
	}

	if((zoom > 0) && (bg_weaponlist[utPSGetWeaponID(&cg.predictedPlayerState)].zoomfov[zoom - 1] == 0))
	{
		zoom = 0;
	}

	cg.zoomedFrom = cg.zoomed;
	cg.zoomed     = zoom;

	if(cg.zoomed)
	{
		cg.zoomTime = cg.time;
	}

	trap_S_StartSound( NULL, cg.clientNum, CHAN_AUTO, cgs.media.UT_zoomSound );
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name       : CG_UTZoomIn
// Description: Zooms in one level
// Author     : Dokta8
///////////////////////////////////////////////////////////////////////////////////////////
void CG_UTZoomIn( void )
{
	int  zoom;
	int  weaponID = utPSGetWeaponID(&cg.predictedPlayerState);

	// Cant zoom right now
	if (cg.zoomed == -1)
	{
		return;
	}

	zoom = cg.zoomed + 1;

	if((zoom - 1 >= MAX_ZOOM_LEVELS) || (bg_weaponlist[weaponID].zoomfov[zoom - 1] == 0))
	{
		if (cg_zoomWrap.integer)
		{
			zoom = 0;
		}
		else
		{
			return;
		}
	}

	CG_ZoomTo ( zoom );
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name       : CG_UTZoomOut
// Description: Zooms out one level
// Author     : Dokta8
///////////////////////////////////////////////////////////////////////////////////////////
void CG_UTZoomOut(void )
{
	int  zoom;
	int  weaponID = utPSGetWeaponID(&cg.predictedPlayerState);

	// No zoomout when dead
	if (!(cg.predictedPlayerState.pm_type == PM_NORMAL))
	{
		return;
	}

	// If they arent zoomed and zoom wrap is on then try to find the next zoom
	if((cg.zoomed == 0) && cg_zoomWrap.integer)
	{
		zoom = MAX_ZOOM_LEVELS;

		while (bg_weaponlist[weaponID].zoomfov[zoom - 1] == 0 && zoom != 0)
		{
			zoom--;
		}
	}
	else
	{
		// dont unzoom if not zoomed.
		if (!cg.zoomed)
		{
			return;
		}

		zoom = cg.zoomed - 1;
	}

	CG_ZoomTo ( zoom );
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name       : CG_UTZoomReset
// Description: Resets the zoom
// Author     : Dokta8
///////////////////////////////////////////////////////////////////////////////////////////
void CG_UTZoomReset(void)
{
	if (!cg.zoomed)
	{
		return;
	}

	cg.zoomed      = 0;
	cg.zoomedSaved = cg.zoomed;

	trap_S_StartSound ( NULL, cg.clientNum, CHAN_AUTO, cgs.media.UT_zoomSound );
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name       : CG_InitPrecipitation
// Description: Initialise rain and snow effects for first use
// Author     : Dokta8
///////////////////////////////////////////////////////////////////////////////////////////
void CG_InitPrecipitation( void )
{
	int  i;

//	if (cg_maxPrecip.integer == 0) return;

	if (!cgs.precipEnabled)
	{
		return;
	}

//	if (cgs.precipAmount > UTPRECIP_MAX_DROPS) cgs.precipAmount = UTPRECIP_MAX_DROPS;

	CG_BuildRainMap();
	sky_start = cgs.worldmaxs[2];

	for(i = 0; i < UTPRECIP_MAX_DROPS; i++)
	{
		drop[i].timeOffset = rand() % 400;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name       : CG_AddPrecipitation
// Description: Rain and snow effects, done each client frame
// Author     : Dokta8
///////////////////////////////////////////////////////////////////////////////////////////

// length of a raindrop
#define UT_DROP_LENGTH 72

/**
 * $(function)
 */
void CG_AddPrecipitation( void )
{
	int	  i;
	int	  numDrops;
	vec3_t	  result;
	vec3_t	  tail;
	vec3_t	  up	    = { 0.0f, 0.0f, 1.0f };
	vec3_t	  ppos;
	vec4_t	  white     = { 0.7f, 0.8f, 1.0f, 0.2f };
	vec4_t	  halfwhite = { 0.7f, 0.8f, 1.0f, 0.0f };
	qboolean  snow	    = qtrue;
	qboolean  isSnow    = qfalse;
	vec3_t	  dist;
	float	  len;

	if (cgs.precipEnabled == 0)
	{
		return; 				// from server
	}

	if (cgs.precipEnabled == 1)
	{
		snow = qfalse;
	}

	VectorCopy( CG_ENTITIES(cg.clientNum)->lerpOrigin, ppos );

	// allow client to control number of drops
	numDrops = UTPRECIP_MAX_DROPS;

	for(i = 0; i < numDrops; i++)
	{
		if (drop[i].tr.trType == TR_SNOWFLAKE)
		{
			isSnow = qtrue;
		}

		BG_EvaluateTrajectory( &drop[i].tr, cg.time, result );

		// see if drop is too far from player (add 59 to account for snow drift
		VectorSubtract(result, cg.refdef.vieworg, dist);
		dist[2] = 0;
		len	= VectorLengthSquared(dist);

		if (len > 1200000)  //1050x1050
		{
			CG_StartDrop( i, snow, qtrue );
			continue;
		}

		// see if drop has reached the ground
		if (result[2] <= drop[i].endpos[2])
		{
			// draw splash
			if (!isSnow)
			{
				CG_ImpactMark( cgs.media.wakeMarkShader, drop[i].endpos, up, 0.0f, 0.0f, 0.5f, 0.7f, 0.75f, qtrue, 4.0f, qtrue, NULL );
			}
			CG_StartDrop( i, snow, qfalse );
			continue;
		}

		// draw droplet/flake
		if (isSnow)
		{
			//	if ( trap_CM_PointContents( result, 0 ) & CONTENTS_SOLID )
			//	{
			//		CG_StartDrop( i, snow, qfalse ); // kill flakes that stray into walls
			//		continue;
			//	}
			CG_DrawFlake( result, 3.0f);
		}
		else
		{
			VectorSet( tail, result[0], result[1], result[2] + UT_DROP_LENGTH );
			CG_Line( tail, result, 0.6f, cgs.media.whiteShader, white, halfwhite );
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name       : CG_StartDrop
// Description: Init a rain or snow drop/flake
// Author     : Dokta8
///////////////////////////////////////////////////////////////////////////////////////////

void CG_StartDrop( int index, qboolean snow, qboolean culled )
{
	float	x, y, zz;
	float	aa;
	int	xx, yy, midpoint;

	//centity_t	*cent;
	precip_t  *pdrop;
	float	  dist, angle;

	if (index > UTPRECIP_MAX_DROPS)
	{
		CG_Printf( "CG_StartDrop: index > UTPRECIP_MAX_DROPS\n" );
	}

	//cent = &cg_entities[ cg.clientNum ];
	pdrop = &drop[index];

	// don't recalc new drops too fast or we'll get massive performance hits
	if (((cg.time - pdrop->tr.trTime) < 128) && (!culled))
	{
		return;
	}

	// do a trace from up really high and see if we hit the sky
	// trace's x and y are within radius of 500 units of player

	angle = rand() % 100000;

	if (culled) // culled will come here if a drop moved outside view
	{
		dist		  = 1000;
		pdrop->timeOffset = 0;
	}
	else
	{
		dist = rand() % 1000;
	}

	x = cg.refdef.vieworg[0] + (cos(angle) * dist);
	y = cg.refdef.vieworg[1] + (sin(angle) * dist);

	if ((x > cgs.worldmaxs[0]) || (x < cgs.worldmins[0]) ||
	    (y > cgs.worldmaxs[1]) || (y < cgs.worldmins[1]))
	{
		pdrop->endpos[2] = 99999;		 // but make sure it won't get thru for 128ms
		return;
	}
	// sky_start holds a global sky height... using this means
	// that we can save a trace, at the cost of accuracy... if the
	// sky is put at different heights on a map, this will fail.

//	VectorSet( end, x, y, -8192.0f );

	xx = (-(x - cgs.worldmaxs[0]) / (cgs.worldmaxs[0] - cgs.worldmins[0])) * 256;
	yy = (-(y - cgs.worldmaxs[1]) / (cgs.worldmaxs[1] - cgs.worldmins[1])) * 256;
//	xx=(int)(xx*(1/RainMapSize));
//	yy=(int)(yy*(1/RainMapSize));
	zz = (cgs.worldmaxs[2] - cgs.worldmins[2]) / 256;

	if (RainMap[xx + (yy * 256)] == 0)
	{
		// We can't spawn the drop so we try again in a second
		//pdrop->timeOffset=
		pdrop->endpos[2] = 99999;		 // but make sure it won't get thru for 128ms
		return;
	}
	aa = sky_start - (((RainMap[xx + (yy * 256)]) * zz));

	VectorSet( pdrop->endpos, x, y, aa);

	if (culled) ///start it randomly somewhere between the ground and here
	{
		midpoint = pdrop->endpos[2] + (rand() % ((int)(sky_start - pdrop->endpos[2])));
		VectorSet( pdrop->tr.trBase, x, y, midpoint );
	}
	else
	{
		VectorSet( pdrop->tr.trBase, x, y, sky_start );
	}

//	pdrop->tr.trBase[2]=pdrop->endpos[2]-10;
//	CG_Line(pdrop->endpos,pdrop->tr.trBase,2,cgs.media.whiteShader,white,white);
//	pdrop->endpos[2] =99999;		// but make sure it won't get thru for 128ms
//	return;
	// trace from sky pos (trBase) to next surface
//	VectorCopy( pdrop->tr.trBase, start );
//	start[2] -= 16; //move back into the sky 16

	// have to use CG_Trace... box trace doesn't see spawned ents like glass
	//CG_Trace( &tr, start, NULL, NULL, end, -1, MASK_SHOT | CONTENTS_WATER );	// don't go thru water
	//trap_CM_BoxTrace ( &tr, start, end, NULL, NULL, 0, MASK_SHOT | CONTENTS_WATER); // faster than CG_trace

	//if ( tr.fraction == 1.0f )
	//{
	//	// this will occur if the sky is over an area with no ground.
	//CG_Printf("CG_StartDrop: rain trace missed the ground (sky: %f)\n", pdrop->tr.trBase[2]);
	//	pdrop->tr.trBase[2] = -9999.0f; // ensure that drop will be recalced
	//	pdrop->tr.trTime = cg.time;		// but make sure it won't get thru for 128ms
	//	return;
	//}

	// store trace intersection with ground or object endpos
	//VectorCopy( tr.endpos, pdrop->endpos );

	// if the endpos is lower than the player then
	// we want to cap the end pos to just below them
	// because there's no point in having it fall all the way
//	if ( pdrop->endpos[2] < (cent->lerpOrigin[2]-128) )
//	{
	// cap end pos to no more than 128 units below player
	// to speed things up if player is somewhere like a
	// building ledge.
//			pdrop->endpos[2] = cent->lerpOrigin[2] - 128;
//	}

	// offset drops if this is an initialisation
	if (pdrop->timeOffset == 0)
	{
/*		float distance;
		int time;

		distance = fabs(pdrop->endpos[2] - pdrop->tr.trBase[2]);

		// calculate time it takes flake or drop to cover fall
		if (snow) {
			time = (int)((distance / 100) * 1000);
		} else {
			time = (int)(sqrt(distance/400) * 1000);
		}

		if (time <= 0) time = 1;	// small distances  */
		pdrop->tr.trTime  = cg.time; // - ( rand() % time );
		pdrop->timeOffset = 1;	// don't use offset in future
	}
	else
	{
		pdrop->tr.trTime = cg.time;
	}

	// normal new drop
	if (snow)
	{
		VectorSet( pdrop->tr.trDelta, 0.0f, 0.0f, -250.0f );
		//pdrop->tr.trTime = cg.time;
		pdrop->tr.trType     = TR_SNOWFLAKE;
		pdrop->tr.trDuration = 10000;
	}
	else
	{
		angle = rand() % 2000; //Makes rain wiggle around just a little
		VectorSet( pdrop->tr.trDelta, sin(angle) * 20, cos(angle) * 20, -250.2f );
		//pdrop->tr.trTime = cg.time;
		pdrop->tr.trTime     = cg.time;
		pdrop->tr.trType     = TR_GRAVITY;
		pdrop->tr.trDuration = 10000;
	}
}

// draws a snow flake using a polygon
void CG_DrawFlake( vec3_t result, float radius )
{
	vec3_t	    playeraxis[3];
	vec3_t	    lineofsight, s, t;
	polyVert_t  verts[4];

	// do a quick low-impact test so we don't draw flakes behind the player
	// (don't skip flakes if player is a ghost or in 3rd person)
	// FIXME: InFieldOfVision func seems to be a bit fux0red
	//if (!cg_thirdPerson.integer || !(cg.snap->ps.stats[STAT_UTPMOVE_FLAGS]&UT_PMF_GHOST)) {
	//	VectorCopy( result, s );	// temp variable because next function changes s
	//	if (!CG_InFieldOfVision( cg.snap->ps.viewangles, cg_fov.value, s )) return;
	//}

	// result is a point in 3D space.
	// we want to build a rectangle that is centered on <result>
	// but which has a normal pointing along the viewer's z axis

	AnglesToAxis ( CG_ENTITIES(cg.clientNum)->lerpAngles, playeraxis );
	VectorCopy( playeraxis[0], lineofsight );

	VectorNormalizeNR( lineofsight );
	PerpendicularVector( s, lineofsight );	// s is 'x'
	CrossProduct( lineofsight, s, t );	// t is 'y'

	// so now we have s and t vectors, we can work out 4 points for
	// the rectangle, starting upper top right, winding anti-clockwise
	VectorMA( result, radius, t, verts[0].xyz );
	verts[0].st[0]	     = 0.0f; //0
	verts[0].st[1]	     = 0.0f; //0
	verts[0].modulate[0] = 200;
	verts[0].modulate[1] = 200;
	verts[0].modulate[2] = 200;
	verts[0].modulate[3] = 64;

	VectorMA( result, -radius, s, verts[1].xyz );
	verts[1].st[0]	     = 1.0f; // 1
	verts[1].st[1]	     = 0.0f; // 0
	verts[1].modulate[0] = 200;
	verts[1].modulate[1] = 200;
	verts[1].modulate[2] = 200;
	verts[1].modulate[3] = 64;

	VectorMA( result, -radius, t, verts[2].xyz );
	verts[2].st[0]	     = 1.0f;
	verts[2].st[1]	     = 1.0f;
	verts[2].modulate[0] = 200;
	verts[2].modulate[1] = 200;
	verts[2].modulate[2] = 200;
	verts[2].modulate[3] = 64;

	VectorMA( result, radius, s, verts[3].xyz );
	verts[3].st[0]	     = 0.0f; // 0
	verts[3].st[1]	     = 1.0f; // 1
	verts[3].modulate[0] = 200;
	verts[3].modulate[1] = 200;
	verts[3].modulate[2] = 200;
	verts[3].modulate[3] = 64;

	trap_R_AddPolyToScene( cgs.media.snowShader, 4, verts );
}

// stolen from ai_dmq3.c
qboolean CG_InFieldOfVision(vec3_t viewangles, float fov, vec3_t angles)
{
	int    i;
	float  diff, angle;

	for (i = 0; i < 2; i++)
	{
		angle	  = AngleMod(viewangles[i]);
		angles[i] = AngleMod(angles[i]);
		diff	  = angles[i] - angle;

		if (angles[i] > angle)
		{
			if (diff > 180.0)
			{
				diff -= 360.0;
			}
		}
		else
		{
			if (diff < -180.0)
			{
				diff += 360.0;
			}
		}

		if (diff > 0)
		{
			if (diff > fov * 0.5)
			{
				return qfalse;
			}
		}
		else
		{
			if (diff < -fov * 0.5)
			{
				return qfalse;
			}
		}
	}
	return qtrue;
}

// [27]
// This function will find the mins and maxs bounding box that would completely
// encapsulate the map.
void CG_FindLevelExtents(vec3_t worldmins, vec3_t worldmaxs)
{
	trace_t  trace;
	vec3_t	 mins;
	vec3_t	 maxs;
	vec3_t	 start;
	vec3_t	 end;

	// set up mins/maxs/start/end for roof/floor checks
	mins[0]  = -65535;
	mins[1]  = -65535;
	mins[2]  = -1;
	maxs[0]  = 65535;
	maxs[1]  = 65535;
	maxs[2]  = 1;

	start[0] = 0;
	start[1] = 0;
	start[2] = -65535;
	end[0]	 = 0;
	end[1]	 = 0;
	end[2]	 = 65535;

	//High to low
	trap_CM_BoxTrace( &trace, start, end, mins, maxs, 0, CONTENTS_SOLID);
	worldmins[2] = trace.endpos[2];
	//Low to High
	trap_CM_BoxTrace( &trace, end, start, mins, maxs, 0, CONTENTS_SOLID);
	worldmaxs[2] = trace.endpos[2];

	//east west
	mins[0]  = -1;
	mins[1]  = -65535;
	mins[2]  = -65535;
	maxs[0]  = 1;
	maxs[1]  = 65535;
	maxs[2]  = 65535;

	start[0] = -65535;
	start[1] = 0;
	start[2] = 0;
	end[0]	 = 65535;
	end[1]	 = 0;
	end[2]	 = 0;
	//High to low
	trap_CM_BoxTrace( &trace, start, end, mins, maxs, 0, CONTENTS_SOLID);
	worldmins[0] = trace.endpos[0];
	//Low to High
	trap_CM_BoxTrace( &trace, end, start, mins, maxs, 0, CONTENTS_SOLID);
	worldmaxs[0] = trace.endpos[0];

	//north south
	mins[0]  = -65535;
	mins[1]  = -1;
	mins[2]  = -65535;
	maxs[0]  = 65535;
	maxs[1]  = 1;
	maxs[2]  = 65535;

	start[0] = 0;
	start[1] = -65535;
	start[2] = 0;
	end[0]	 = 0;
	end[1]	 = 65535;
	end[2]	 = 0;
	//High to low
	trap_CM_BoxTrace( &trace, start, end, mins, maxs, 0, CONTENTS_SOLID);
	worldmins[1] = trace.endpos[1];
	//Low to High
	trap_CM_BoxTrace( &trace, end, start, mins, maxs, 0, CONTENTS_SOLID);
	worldmaxs[1] = trace.endpos[1];
}

/**
 * $(function)
 */
void CG_BuildMiniMap(void)
{
	int	      x, y;
	unsigned int  a, q;
	float	      levelx, levely, levelz;
	trace_t       trace;
	fileHandle_t  MiniMapFile;
	int	      foot;

	unsigned int  MiniMap[MiniMapSize]; // one row
	char	      targaheader[18];
	char	      targafooter[26]; //head + body +foot

	char	      filename[MAX_QPATH];

	vec3_t	      start, end;

	CG_FindLevelExtents(cgs.worldmins, cgs.worldmaxs);

	levelz = cgs.worldmaxs[2] - cgs.worldmins[2];
	levelx = cgs.worldmaxs[0] - cgs.worldmins[0];
	levely = cgs.worldmaxs[1] - cgs.worldmins[1];

	//Make levelx= to the larger of the two
	if (levelx < levely)
	{
		levelx = levely;
	}
	cgs.wxdelta = levelx / MiniMapSize;

	start[2]    = cgs.worldmaxs[2];
	end[2]	    = cgs.worldmins[2];

	strcpy(filename, cgs.mapname);
	foot		   = strlen(filename);

	filename[foot - 3] = 'T';
	filename[foot - 2] = 'G';
	filename[foot - 1] = 'A';

	trap_FS_FOpenFile( filename, &MiniMapFile, FS_READ );

	if (MiniMapFile != 0)
	{
		return;
	}
	// ELSE WE BUILD A NEW MINIMAP TGA AS WELL

	for (y = 0; y < MiniMapSize; y++)
	{
		MiniMap[y] = 0;
	}

	trap_FS_FOpenFile( filename, &MiniMapFile, FS_WRITE );

	// Set unused fields of header to 0
	memset(targaheader, 0, sizeof(targaheader));
	targaheader[2]	= 2;			       /* image type True color uncompressed*/
	targaheader[12] = (char)(MiniMapSize & 0xFF);  //w
	targaheader[13] = (char)(MiniMapSize >> 8);    //w
	targaheader[14] = (char)(MiniMapSize & 0xFF);  //h
	targaheader[15] = (char)(MiniMapSize >> 8);    //h
	targaheader[16] = 32;
	targaheader[17] = 3; //0x00;	/* non-interlaced, useful alpha, no funny alignments */

	trap_FS_Write(	targaheader, 18, MiniMapFile);

	for (y = 0; y < MiniMapSize; y++)
	{
		for (x = 0; x < MiniMapSize; x++)
		{
			MiniMap[x] = 0;
		}

		if ((cgs.worldmaxs[1] - (y * cgs.wxdelta) > cgs.worldmins[1]))
		{
			start[1] = cgs.worldmaxs[1] - (y * cgs.wxdelta);
			end[1]	 = cgs.worldmaxs[1] - (y * cgs.wxdelta);

			for (x = 0; x < MiniMapSize; x++)
			{
				if (cgs.worldmaxs[0] - (x * cgs.wxdelta) > cgs.worldmins[0])
				{
					start[0] = cgs.worldmaxs[0] - (x * cgs.wxdelta);
					end[0]	 = cgs.worldmaxs[0] - (x * cgs.wxdelta);

					trap_CM_BoxTrace( &trace, start, end, NULL, NULL, 0, CONTENTS_SOLID);

					// We might have hit the sky, which is uh, bad.
					// If we DID (due to a poorly caulk'd sky) we have to burrow through it.
					trace.endpos[2] -= 20;

					while (trap_CM_PointContents(trace.endpos, 0) & CONTENTS_SOLID)
					{
						trace.endpos[2] -= 20;
					}
					trap_CM_BoxTrace( &trace, trace.endpos, end, NULL, NULL, 0, CONTENTS_SOLID);

					if ((!trace.startsolid) && (trace.fraction != 1))
					{
						q = ((trace.endpos[2] - cgs.worldmins[2]) / levelz) * 256;

						if (q > 255)
						{
							q = 255;
						}

						//q=255-q;
						if (q > 0)
						{
							a = 255 << 24;
						}
						else
						{
							a = 0;
						}

						MiniMap[x] = ((q << 8) + a);
					}
				}
			}
		}
		//Write out the part of the file
		trap_FS_Write(	MiniMap, MiniMapSize * 4, MiniMapFile);
	}

	targafooter[0]	= 0;
	targafooter[1]	= 0;
	targafooter[2]	= 0;
	targafooter[3]	= 0;
	targafooter[4]	= 0;
	targafooter[5]	= 0;
	targafooter[6]	= 0;
	targafooter[7]	= 0;

	targafooter[8]	= 'T';
	targafooter[9]	= 'R';
	targafooter[10] = 'U';
	targafooter[11] = 'E';
	targafooter[12] = 'V';
	targafooter[13] = 'I';
	targafooter[14] = 'S';
	targafooter[15] = 'I';
	targafooter[16] = 'O';
	targafooter[17] = 'N';
	targafooter[18] = '-';
	targafooter[19] = 'X';
	targafooter[20] = 'F';
	targafooter[21] = 'I';
	targafooter[22] = 'L';
	targafooter[23] = 'E';
	targafooter[24] = '.';
	targafooter[25] = 0;

	trap_FS_Write(	targafooter, 26, MiniMapFile);

	//Save it
	trap_FS_FCloseFile(MiniMapFile);
	trap_Print( va( "Minimap Tga for %s written.\n", filename ));
}

/**
 * $(function)
 */
void CG_LoadMiniMap(void)
{
	char  mapname[MAX_QPATH];
	int   foot;

	CG_BuildMiniMap();

	strcpy(mapname, cgs.mapname);
	foot		  = strlen(mapname);
	mapname[foot - 3] = 'T';
	mapname[foot - 2] = 'G';
	mapname[foot - 1] = 'A';
	cgs.media.MiniMap = trap_R_RegisterShaderNoMip(mapname);
	//FIXME: the auto script generator wont make scripts for outside the q3UT2 folder.
	//Might be an idea to do a manual file write here and vid_restart
	//if (cgs.MiniMap==0)
	cgs.media.MiniMapArrow = trap_R_RegisterShaderNoMip("MiniMapArrow");
}

//Draws the minimap
//27: WAY too many local vars, this should be split across several functions
void CG_DrawMiniMap(void) {

	int            skin;
	int	       x, y, mx, my, w, h, ww, hh, zzz;
	int	       flag;
	int	       glowtime = 0;
	int	       glowcolor = 0;
	int	       flagstate = 0;
	int	   	   i;
	int	       framenum;
	float	   frac, angle;
	float	   z1, x1, y1, scale, width, arrowsize;
	float	   x2, y2;
	float	   xoff, yoff;
	float	   xyrat, xadj, yadj;
	qboolean   OkToDraw;
  //qboolean   flipped;
	vec4_t	   orange = { 1, 0.7f, 0.2f, 1 };
	//vec4_t	   col;
	vec4_t	   color;
	centity_t  *cent;
	qhandle_t  flagIcon;

	glowtime  = 0;
	glowcolor = 0;
	flagstate = 0;

	// New!
	if((cg_maptoggle.integer == 0) && !(cg.predictedPlayerState.stats[STAT_BUTTONS] & BUTTON_MINIMAP)) {
		return;
	}

	color[0] = 1.0f;
	color[1] = 1.0f;
	color[2] = 1.0f;
	color[3] = cg_mapalpha.value;

	if (color[3] < 0) {
		color[3] = 0;
	}

	if (color[3] > 1) {
		color[3] = 1;
	}

	trap_R_SetColor( color );

	w = cg_mapsize.integer;

	if (w <= 0) {
		return;
	}

	if (w > 512) {
		w = 512;
	}

	width = (cgs.wxdelta * MiniMapSize);
	scale = width / w;
	h     = w;

	// if (cg_mapflip.integer ==0) {
	//     flipped=qtrue;
	// } else {
	//	flipped=qfalse;
	// }

	if (cgs.worldmaxs[0] - cgs.worldmins[0] > cgs.worldmaxs[1] - cgs.worldmins[1]) {
		xyrat = (cgs.worldmaxs[1] - cgs.worldmins[1]) / (cgs.worldmaxs[0] - cgs.worldmins[0]);
		yadj  = (xyrat * w) - w; // X on the left
		xadj  = 0;
	}
	else {
		xyrat = (cgs.worldmaxs[0] - cgs.worldmins[0]) / (cgs.worldmaxs[1] - cgs.worldmins[1]);
		xadj  = (xyrat * w) - w; // X on the left
		yadj  = 0;
	}

	if ((cg_mappos.integer < 1) || (cg_mappos.integer > 11)) {
		return;
	}


	switch (cg_mappos.integer) {

	    case 1: 	 	// top left
			x = xadj;
			y = 0;
			break;

		case 2: 	 	// middle left
			x = xadj;
			y = (240 - ((w) / 2));
			break;

		case 3: 	 	// bottom left 1
			x = xadj;
			y = (400 - w) - yadj;
			break;

		case 4: 	 	// bottom left 2
			x = xadj + 60;
			y = (480 - w) - yadj;
			break;

		case 5:  		// middle bottom
			x = (320 - ((w + xadj) / 2)) + xadj;
			y = (480 - w) - yadj;
			break;

		case 6:  		// bottom right
			x = (640 - w);
			y = (440 - w) - yadj;
			break;

		case 7: 	 	// middle right
			x = (640 - w);
			y = (240 - ((w) / 2));
			break;

		case 8: 	 	// top right 1
			x = (640 - w); //+xadj;
			y = 75; //+yadj;
			break;

		case 9: 	 	// top right 2
			x = (520 - w);
			y = 0;
			break;

		case 10:		// top middle
			x = (320 - ((w + xadj) / 2)) + xadj;
			y = 0; //yadj;
			break;

		case 11:		// middle
			x = (320 - ((w + xadj) / 2)) + xadj;
			y = (240 - ((w) / 2));
			break;
	}

	h   = w;
	zzz = 0;
	mx  = x;
	my  = y;
	ww  = w;
	hh  = h;

	CG_AdjustFrom640(&mx, &my, &ww, &hh);

	if (cgs.media.MiniMap != 0) {
		trap_R_DrawStretchPic(	mx, my, ww, hh, 1.0f, 1.0f, 0.0f, 0.0f, cgs.media.MiniMap);
	}

	// Draw bomb sites - DensitY
	if(cgs.gametype == GT_UT_BOMB) {

		int  jj;
		int  coord[2];
		int  ImageSize;

		// Set Image size, increase Cycle time by 50ms
		ImageSize = 32 * cos(((float)cg.time) / 300.0f );	// Full cycle every 300ms

		for(jj = 0; jj < cg.NumBombSites; jj++) {

			x1 = (cgs.cg_BombSites[jj].Position[0] - cgs.worldmaxs[0]) / scale;
			y1 = (cgs.cg_BombSites[jj].Position[1] - cgs.worldmaxs[1]) / scale;
			x1 = w + ((x) + x1);
			y1 = (y) - y1;
			CG_AdjustFrom640( &x1, &y1, &zzz, &zzz );

			// position of image
			coord[0] = (x1 - (ImageSize >> 1));
			coord[1] = (y1 - (ImageSize >> 1));

			// Draw image
			if (cgs.cg_BombSites[jj].b) {
				color[0] = 0.0f;
				color[1] = 0.0f;
				color[2] = 0.0f;
				color[3] = 1;
				trap_R_SetColor( color );
				trap_R_DrawStretchPic( coord[0], coord[1], ImageSize, ImageSize, 0, 0, 1, 1, cgs.media.ut_BombMiniMapMarker );
			}
			else {
				color[0] = 1.0f;
				color[1] = 1.0f;
				color[2] = 1.0f;
				color[3] = 1;
				trap_R_SetColor( color );
				trap_R_DrawStretchPic( coord[0], coord[1], ImageSize, ImageSize, 0, 0, 1, 1, cgs.media.ut_BombMiniMapMarker );
			}
		}
	}


	for (i = MAX_CLIENTS; i < MAX_GENTITIES; i++) {

		cent = CG_ENTITIES(i);

		// Make sure we have a valid pointer
		if (!cent) {
			continue;
		}

		if (cent->currentState.eType != ET_HUDARROW) {
			continue;
		}

		// Only print if clientinfo is valid: normally
		// it will be for all the connected clients
		if (cent->currentValid) {

			OkToDraw = qfalse;

			// Don't draw if hidden
			if (cg.time < cent->currentState.time2) {
				continue;
			}

			// Ignore spectators
			if (cent->currentState.frame == 3) {
				continue;
			}

			// Figure out it's radio glow early for the local rendering though
			// Ignore if we're viewing this one
			if (cent->currentState.clientNum == cg.predictedPlayerState.clientNum) {
				// Grab some info out of the arrow info, that we dont know about anywhere else
				glowtime  = cent->currentState.time;
				glowcolor = cent->currentState.generic1;
				flagstate = cent->currentState.legsAnim;
				continue;
			}

			// Ignore it if we're in FFA and not dead, and not in spectators
			if (cgs.gametype < GT_TEAM) {

				if ((CG_ENTITIES(cg.clientNum)->currentState.eFlags & EF_DEAD) || (cgs.clientinfo[cg.clientNum].team == TEAM_SPECTATOR)) {
					OkToDraw = qtrue;
				} else {
					OkToDraw = qfalse;
				}

			}
			else {

				// For all other gamemodes, make sure that we're on the
				// same team, unless we're dead and teamfollow is off

				OkToDraw = qfalse;

				// If we're dead or viewing another players eyes
				if ((cg.snap->ps.pm_flags & PMF_FOLLOW) || (cg.predictedPlayerState.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_GHOST) & (cg.predictedPlayerState.persistant[PERS_TEAM] != TEAM_SPECTATOR)) {

					if	(cgs.followenemy == 1) {
						// If its on, then we allow all drawing
						OkToDraw = qtrue;
					}
					else {

						// If its not on, make sure we only draw the same team as the view we're seeing
						if (cgs.clientinfo[cg.predictedPlayerState.clientNum].team == cent->currentState.frame) {
							OkToDraw = qtrue;
						}

					}
				}
				else {

					// We're alive, so check for matching teams
					if (cg.predictedPlayerState.persistant[PERS_TEAM] == cent->currentState.frame) {
						OkToDraw = qtrue;
					}

					// @Fenix - Dead code
					// Make sure the icon isn't dead...
					// if ((cent->currentState.eFlags & EF_DEAD)) //&&(cg.predictedPlayerState.stats[STAT_UTPMOVE_FLAGS]&UT_PMF_GHOST))
					// {
					//	  OkToDraw=qfalse;
					// }

				}

				// Draw if we're in spectator team
				if (cg.predictedPlayerState.persistant[PERS_TEAM] == TEAM_SPECTATOR) {
					OkToDraw = qtrue;
				}

			}

			// if ((cgs.clientinfo[cg.clientNum].team!=0)&&(cgs.clientinfo[cg.clientNum].team!=3)) {
			//     if ((cgs.clientinfo[cent->currentState.clientNum].team!=cgs.clientinfo[cg.clientNum].team)) {
			//         continue;
			//     }
			// }

			// @Barbatos - jump mode - always draw
			if(cgs.gametype == GT_JUMP) {
				OkToDraw = qtrue;
			}

			if (OkToDraw == qfalse) {
				continue;
			}

			color[3] = 1;

			// Figure out our frac from the last snapshot
			frac = cg.time - cent->currentState.pos.trTime;

			if (frac > 1000) {
				frac = 1000;
			}

			frac /= 1000;

			angle = LerpAngle(cent->lerpAngles[0], cent->lerpAngles[1], frac);

			framenum = (-angle - 90) / 5.625;

			if (framenum > 63) {
				framenum -= 64;
			}

			if (framenum < 0) {
				framenum += 64;
			}

			yoff = framenum % 8;
			xoff = (int)framenum / 8;

			// Lerp
			x1 = (cent->currentState.origin2[0] * (1 - frac)) + (cent->lerpOrigin[0] * (frac));
			y1 = (cent->currentState.origin2[1] * (1 - frac)) + (cent->lerpOrigin[1] * (frac));
			z1 = (cent->currentState.origin2[2] * (1 - frac)) + (cent->lerpOrigin[2] * (frac));

			x1 = (x1 - cgs.worldmaxs[0]) / scale;
			y1 = (y1 - cgs.worldmaxs[1]) / scale;
			z1 = (z1 - cgs.worldmins[2]) / (cgs.worldmaxs[2] - cgs.worldmins[2]);

			x1 = w + ((x) + x1);
			y1 = (y) - y1;

			CG_AdjustFrom640(&x1, &y1, &zzz, &zzz);
			arrowsize = ((w * (1 + z1)) / 40) * cg_maparrowscale.value;

			// @Barbatos: was 3. Increased due to big
			// maps where we couldn't see the arrows
			if (arrowsize < 10) {
				arrowsize = 10;
			}

			// If we're currently viewing this player
			if (cent->currentState.clientNum == cg.predictedPlayerState.clientNum && (!(cent->currentState.eFlags & EF_DEAD))) {
				color[0] = color[0] = (1.0f + cos(((float)cg.time) / 350.0f)) / 2;
				color[1] = color[0];
				color[2] = color[0];
				color[3] = 1.0f;
				trap_R_SetColor(color);
				trap_R_DrawStretchPic(	x1 - (arrowsize * 1.3), y1 - (arrowsize * 1.3), (arrowsize * 1.3) * 2, (arrowsize * 1.3) * 2, yoff * 0.125f, xoff * 0.125f, (yoff * 0.125f) + 0.125f, (xoff * 0.125f) + 0.125f, cgs.media.MiniMapArrow);
			}

			// 1 // 4
			// color[0]=bg_colors[cent->currentState.clientNum % 20][0]/255;
			// color[1]=bg_colors[cent->currentState.clientNum % 20][1]/255;
			// color[2]=bg_colors[cent->currentState.clientNum % 20][2]/255;

			color[0] = cgs.clientinfo[cent->currentState.clientNum].armbandRGB[0] / 255;
			color[1] = cgs.clientinfo[cent->currentState.clientNum].armbandRGB[1] / 255;
			color[2] = cgs.clientinfo[cent->currentState.clientNum].armbandRGB[2] / 255;

            if (cg.g_armbands==1) {
                skin = CG_WhichSkin(cgs.clientinfo[cent->currentState.clientNum].team);
                color[0] = skinInfos[skin].defaultArmBandColor[0] / 255;
                color[1] = skinInfos[skin].defaultArmBandColor[1] / 255;
                color[2] = skinInfos[skin].defaultArmBandColor[2] / 255;
            }

			// color[0]=ci.
			// color[1]=
			// color[2]=

			if ((cent->currentState.eFlags & EF_DEAD)) {
				continue;
			}

			// If we're in radio time...
			if (cent->currentState.time > cg.time) {

				color[0] = 0;
				color[1] = 0;
				color[2] = 0;

				if (cent->currentState.generic1 == 0) { // normal
					color[0] = (1.0f + sin(((float)cg.time) / 50.0f)) / 2; //sin wave between 0-1
					color[1] = (1.0f + sin(((float)cg.time) / 50.0f)) / 2; //sin wave between 0-1
				}

				if (cent->currentState.generic1 == 1) { // medic
					color[0] = (1.0f + sin(((float)cg.time) / 50.0f)) / 2; //sin wave between 0-1
				}

				if (cent->currentState.generic1 == 2) { // order
					color[1] = (1.0f + sin(((float)cg.time) / 50.0f)) / 2; //sin wave between 0-1
					color[2] = (1.0f + sin(((float)cg.time) / 50.0f)) / 2; //sin wave between 0-1
				}

				if (cent->currentState.generic1 == 3) { // order neg
					color[0] = (1.0f + sin(((float)cg.time) / 50.0f)) / 2; //sin wave between 0-1
					color[2] = (1.0f + sin(((float)cg.time) / 50.0f)) / 2; //sin wave between 0-1
				}
			}

			trap_R_SetColor(color);
			trap_R_DrawStretchPic(x1 - (arrowsize * 1.0), y1 - (arrowsize * 1.0), (arrowsize * 1.0) * 2, (arrowsize * 1.0) * 2, yoff * 0.125f, (xoff * 0.125f) + 0.125f, (yoff * 0.125f) + 0.125f, xoff * 0.125f, cgs.media.MiniMapArrow);

			if (cgs.clientinfo[cg.clientNum].team == TEAM_SPECTATOR) {

				switch(cent->currentState.frame) {

					case 1:
						VectorCopy(g_color_table[1], color);
						break;

					case 2:
						// VectorCopy(g_color_table[4], color);
						color[0] = (float)(64.0f / 255.0f);
						color[1] = (float)(64.0f / 255.0f);
						color[2] = (float)(255.0f / 255.0f);
						break;

					case 0:
						VectorCopy(g_color_table[3], color);
						break;

				}

				if ((cent->currentState.eFlags & EF_DEAD))
					color[3] *= 0.2f;

				trap_R_SetColor(color);
				trap_R_DrawStretchPic(x1 - (arrowsize * 0.15), y1 - (arrowsize * 0.15), (arrowsize * 0.15) * 2, (arrowsize * 0.15) * 2, 0, 0, 1, 1, cgs.media.whiteShader);
			}

			if ((cent->currentState.powerups & POWERUP(PW_CARRYINGBOMB)) || (cgs.clientinfo[cent->currentState.clientNum].teamLeader)) { // @Barbatos: bomb or teamleader
				orange[3] = (1.0f + cos(((float)cg.time) / 50.0f)) / 2;
				trap_R_SetColor(orange);
				trap_R_DrawStretchPic(x1 - (arrowsize * 1.0), y1 - (arrowsize * 1.0), (arrowsize * 1.0) * 2, (arrowsize * 1.0) * 2, yoff * 0.125f, (xoff * 0.125f) + 0.125f, (yoff * 0.125f) + 0.125f, xoff * 0.125f, cgs.media.MiniMapArrow);
			}

			if (cent->currentState.powerups & POWERUP(PW_MEDKIT)) { // Red flag

				color[0] = 1;
				color[1] = 1;
				color[2] = 1;
				color[3] = (1.0f + cos(((float)cg.time) / 150.0f)) / 2;

				trap_R_SetColor(color);

				x2 = x1 + (sin(DEG2RAD(angle - 90)) * 5);
				y2 = y1 + (cos(DEG2RAD(angle - 90)) * 5);
				trap_R_DrawStretchPic(	x2 - 4, y2 - 4, 8, 8, 0, 0, 1, 1, cgs.media.UT_FlagMedic);
			}

			color[0] = 1;
			color[1] = 1;
			color[2] = 1;
			color[3] = 1;

			trap_R_SetColor( color );

			x1 += cos(((float)cg.time) / 100.0f);
			y1 += sin(((float)cg.time) / 100.0f);

			if (cent->currentState.powerups & POWERUP(PW_REDFLAG)) {
				skin = (cgs.gametype == GT_JUMP) ? UT_SKIN_RED : CG_WhichSkin(TEAM_RED);
				trap_R_DrawStretchPic(	x1 - 4, y1 - 4, 8, 8, 0, 0, 1, 1, cgs.media.UT_FlagIcon[skin]);
			}

			if (cent->currentState.powerups & POWERUP(PW_BLUEFLAG)) {
				skin = (cgs.gametype == GT_JUMP) ? UT_SKIN_BLUE : CG_WhichSkin(TEAM_BLUE);
				trap_R_DrawStretchPic(	x1 - 4, y1 - 4, 8, 8, 0, 0, 1, 1, cgs.media.UT_FlagIcon[skin]);
			}
		}
	}

	// Draw us with local data if we're spectating a client or ourselves
	if ((cgs.clientinfo[cg.predictedPlayerState.clientNum].team != 3) && !(cg.snap->ps.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_GHOST)) {

		angle	 = cg.refdefViewAngles[1];
		framenum = (-cg.refdefViewAngles[1] - 90) / 5.625;

		if (framenum > 63) {
			framenum -= 64;
		}

		if (framenum < 0) {
			framenum += 64;
		}

		yoff = framenum % 8;
		xoff = (int)framenum / 8;

		x1 = (cg.refdef.vieworg[0] - cgs.worldmaxs[0]) / scale;
		y1 = (cg.refdef.vieworg[1] - cgs.worldmaxs[1]) / scale;
		z1 = (cg.refdef.vieworg[2] - cgs.worldmins[2]) / (cgs.worldmaxs[2] - cgs.worldmins[2]);
		x1 = w + ((x) + x1);
		y1 = (y) - y1;

		CG_AdjustFrom640(&x1, &y1, &zzz, &zzz);

		arrowsize = ((w * (1 + z1)) / 40) * cg_maparrowscale.value;

		// @Barbatos: was 3
		if (arrowsize < 9) {
			arrowsize = 9;
		}

		color[0] = (1.0f + cos(((float)cg.time) / 150.0f)) / 2;
		color[1] = color[0];
		color[2] = color[1];
		color[3] = color[0];

		if (CG_ENTITIES(cg.predictedPlayerState.clientNum)->currentState.eFlags & EF_DEAD) {
			color[3] = 0.0f;
		}
		else {
			color[3] = 1.0f;
		}

		trap_R_SetColor(color);
		trap_R_DrawStretchPic(x1 - (arrowsize * 1.3), y1 - (arrowsize * 1.3), (arrowsize * 1.3) * 2, (arrowsize * 1.3) * 2, yoff * 0.125f, (xoff * 0.125f) + 0.125f, (yoff * 0.125f) + 0.125f, xoff * 0.125f, cgs.media.MiniMapArrow);

		// color[0]=bg_colors[cg.predictedPlayerState.clientNum % 20][0]/255;
		// color[1]=bg_colors[cg.predictedPlayerState.clientNum % 20][1]/255;
		// color[2]=bg_colors[cg.predictedPlayerState.clientNum % 20][2]/255;
		color[0] = cgs.clientinfo[cg.predictedPlayerState.clientNum].armbandRGB[0] / 255;
		color[1] = cgs.clientinfo[cg.predictedPlayerState.clientNum].armbandRGB[1] / 255;
		color[2] = cgs.clientinfo[cg.predictedPlayerState.clientNum].armbandRGB[2] / 255;

        if (cg.g_armbands==1) {
            skin = CG_WhichSkin(cgs.clientinfo[cent->currentState.clientNum].team);
            color[0] = skinInfos[skin].defaultArmBandColor[0] / 255;
            color[1] = skinInfos[skin].defaultArmBandColor[1] / 255;
            color[2] = skinInfos[skin].defaultArmBandColor[2] / 255;
        }

		/*
			switch( cgs.clientinfo[ cg.predictedPlayerState.clientNum ].team ) {
				case 1:
					VectorCopy( g_color_table[ 1 ], color );
					break;
				case 2:
					//VectorCopy( g_color_table[ 4 ], color );
					//120, 170, 230
					color[ 0 ] = ( float )( 64.0f / 255.0f );
					color[ 1 ] = ( float )( 64.0f / 255.0f );
					color[ 2 ] = ( float )( 255.0f / 255.0f );
					break;
				case 0:
					VectorCopy( g_color_table[ 3 ], color );
					break;
			  }
		*/

		// FIXME: we dont see our own arrow on the snapshot anymore.. beh
		if (glowtime > cg.time) {

			color[0] = 0;
			color[1] = 0;
			color[2] = 0;

			if (glowcolor == 0) { // normal
				color[0] = (1.0f + sin(((float)cg.time) / 50.0f)) / 2; //sin wave between 0-1
				color[1] = (1.0f + sin(((float)cg.time) / 50.0f)) / 2; //sin wave between 0-1
			}

			if (glowcolor == 1) { // medic
				color[0] = (1.0f + sin(((float)cg.time) / 50.0f)) / 2; //sin wave between 0-1
			}

			if (glowcolor == 2) { // order
				color[1] = (1.0f + sin(((float)cg.time) / 50.0f)) / 2; //sin wave between 0-1
				color[2] = (1.0f + sin(((float)cg.time) / 50.0f)) / 2; //sin wave between 0-1
			}

			if (glowcolor == 3) { // order neg
				color[0] = (1.0f + sin(((float)cg.time) / 50.0f)) / 2; //sin wave between 0-1
				color[2] = (1.0f + sin(((float)cg.time) / 50.0f)) / 2; //sin wave between 0-1
			}
		}

		trap_R_SetColor(color);
		trap_R_DrawStretchPic(x1 - (arrowsize * 1), y1 - (arrowsize * 1), (arrowsize * 1) * 2, (arrowsize * 1) * 2, yoff * 0.125f, (xoff * 0.125f) + 0.125f, (yoff * 0.125f) + 0.125f, xoff * 0.125f, cgs.media.MiniMapArrow);

		// see if they have a flag
		if (utPSGetWeaponByID(&cg.snap->ps, UT_WP_BOMB) > 0) { // bomb
			orange[3] = (1.0f + cos(((float)cg.time) / 50.0f)) / 2;
			trap_R_SetColor(orange);
			trap_R_DrawStretchPic(	x1 - (arrowsize * 1), y1 - (arrowsize * 1), (arrowsize * 1) * 2, (arrowsize * 1) * 2, yoff * 0.125f, (xoff * 0.125f) + 0.125f, (yoff * 0.125f) + 0.125f, xoff * 0.125f, cgs.media.MiniMapArrow);
		}

		if (utPSHasItem(&cg.snap->ps, UT_ITEM_MEDKIT)) {
			color[0] = 1;
			color[1] = 1;
			color[2] = 1;
			color[3] = (1.0f + cos(((float)cg.time) / 150.0f)) / 2;
			trap_R_SetColor(color);
			x2 = x1 + (sin(DEG2RAD(angle - 90)) * 5);
			y2 = y1 + (cos(DEG2RAD(angle - 90)) * 5);
			trap_R_DrawStretchPic(	x2 - 4, y2 - 4, 8, 8, 0, 0, 1, 1, cgs.media.UT_FlagMedic);
		}

		x1	+= cos(((float)cg.time) / 100.0f);
		y1	+= sin(((float)cg.time) / 100.0f);

		color[0] = 1;
		color[1] = 1;
		color[2] = 1;
		color[3] = 1;

		trap_R_SetColor(color);

		// If we're packing the red flag.
		if (utPSHasItem(&cg.snap->ps, UT_ITEM_REDFLAG)) {
			skin = (cgs.gametype == GT_JUMP) ? UT_SKIN_RED : CG_WhichSkin(TEAM_RED);
			trap_R_DrawStretchPic(	x1 - 4, y1 - 4, 8, 8, 0, 0, 1, 1, cgs.media.UT_FlagIcon[skin]);
		} else if (utPSHasItem(&cg.snap->ps, UT_ITEM_BLUEFLAG)) {
			skin = (cgs.gametype == GT_JUMP) ? UT_SKIN_BLUE : CG_WhichSkin(TEAM_BLUE);
            		trap_R_DrawStretchPic(	x1 - 4, y1 - 4, 8, 8, 0, 0, 1, 1, cgs.media.UT_FlagIcon[skin]);
		}
	}

	if (cg.bombvisible == qtrue) {
		if ((cgs.clientinfo[cg.predictedPlayerState.clientNum].team != TEAM_BLUE) ||
		    ((cg.predictedPlayerState.clientNum != cg.clientNum) && !(cg.snap->ps.pm_flags & PMF_FOLLOW))
		    || (cgs.redflag == 0)) {

			orange[3] = (1.0f + cos(((float)cg.time) / 50.0f)) / 2;
			trap_R_SetColor(orange);

			x1 = (cg.bombpos[0] - cgs.worldmaxs[0]) / scale;
			y1 = (cg.bombpos[1] - cgs.worldmaxs[1]) / scale;
			x1 = w + ((x) + x1);
			y1 = ((y) - y1);

			CG_AdjustFrom640(&x1, &y1, &zzz, &zzz);
			trap_R_DrawStretchPic(	x1 - 10, y1 - 10, 20, 20, 0, 0, 1, 1, cgs.media.ut_BombMiniMapMarker);
		}
	}

	// Reset the colors back
	color[0] = 1.0f;
	color[1] = 1.0f;
	color[2] = 1.0f;
	color[3] = 1;
	trap_R_SetColor(color);

	// @Fenix - draw also team based flags if we are playing JUMP mode
	if ((cgs.gametype == GT_CTF) || (cgs.gametype == GT_JUMP)) {

		if (cg.redflagvisible == qtrue) {
			if ((cgs.clientinfo[cg.predictedPlayerState.clientNum].team != TEAM_RED) ||
			   ((cg.predictedPlayerState.clientNum != cg.clientNum) && !(cg.snap->ps.pm_flags & PMF_FOLLOW)) ||
			   (cgs.redflag == 0)) {

				x1 = (cg.redflagpos[0] - cgs.worldmaxs[0]) / scale;
				y1 = (cg.redflagpos[1] - cgs.worldmaxs[1]) / scale;
				x1 = w + ((x) + x1);
				y1 = ((y) - y1);

				CG_AdjustFrom640(&x1, &y1, &zzz, &zzz);
				skin = (cgs.gametype == GT_JUMP) ? UT_SKIN_RED : CG_WhichSkin(TEAM_RED);
				trap_R_DrawStretchPic(	x1 - 4, y1 - 4, 8, 8, 0, 0, 1, 1, cgs.media.UT_FlagIcon[skin]);
			}
		}

		if (cg.blueflagvisible == qtrue) {
			if  ((cgs.clientinfo[cg.predictedPlayerState.clientNum].team != TEAM_BLUE) ||
				((cg.predictedPlayerState.clientNum != cg.clientNum) && !(cg.snap->ps.pm_flags & PMF_FOLLOW)) ||
				(cgs.blueflag == 0)) {

				x1 = (cg.blueflagpos[0] - cgs.worldmaxs[0]) / scale;
				y1 = (cg.blueflagpos[1] - cgs.worldmaxs[1]) / scale;
				x1 = w + ((x) + x1);
				y1 = (y) - y1;

				CG_AdjustFrom640(&x1, &y1, &zzz, &zzz);
				skin = (cgs.gametype == GT_JUMP) ? UT_SKIN_BLUE : CG_WhichSkin(TEAM_BLUE);
				trap_R_DrawStretchPic(	x1 - 4, y1 - 4, 8, 8, 0, 0, 1, 1, cgs.media.UT_FlagIcon[skin]);
			}
		}
	}

	// Draw in the CAH/FTL flags if available
	for (flag = 0; cgs.gameFlags[flag].number; flag++) {

		x1 = (cgs.gameFlags[flag].origin[0] - cgs.worldmaxs[0]) / scale;
		y1 = (cgs.gameFlags[flag].origin[1] - cgs.worldmaxs[1]) / scale;
		x1 = w + ((x) + x1);
		y1 = (y) - y1;

		if ((cgs.gameFlags[flag].team == TEAM_RED) || (cgs.gameFlags[flag].team == TEAM_BLUE)) {
			x1 += cos(((float)cg.time) / 100.0f);
			y1 += sin(((float)cg.time) / 100.0f);
			flagIcon = cgs.media.UT_FlagIcon [CG_WhichSkin(cgs.gameFlags[flag].team)];
		} else
		{
			flagIcon = cgs.media.UT_FlagNeutralIcon;
		}

		CG_AdjustFrom640(&x1, &y1, &zzz, &zzz);
		trap_R_DrawStretchPic(	x1 - 4, y1 - 4, 8, 8, 0, 0, 1, 1, flagIcon);
	}

	// Make sure the entity is actually visible next frame
	cg.redflagvisible  = qfalse;
	cg.blueflagvisible = qfalse;
	cg.bombvisible	   = qfalse;

}

/**
 * $(function)
 */
void CG_BuildRainMap(void)
{
	int	 x, y;
	char	 q;
	float	 levelx, levely, levelz;
	trace_t  trace;

	vec3_t	 start, end;

	CG_FindLevelExtents(cgs.worldmins, cgs.worldmaxs);

	levelx	    = cgs.worldmaxs[0] - cgs.worldmins[0];
	levely	    = cgs.worldmaxs[1] - cgs.worldmins[1];

	levelz	    = cgs.worldmaxs[2] - cgs.worldmins[2];

	cgs.txdelta = levelx / RainMapSize;

	cgs.tydelta = levely / RainMapSize;

	start[2]    = cgs.worldmaxs[2];
	end[2]	    = cgs.worldmins[2];

	for (y = 0; y < RainMapSize * RainMapSize; y++)
	{
		RainMap[y] = 0;
	}

	for (y = 0; y < RainMapSize; y++)
	{
		start[1] = cgs.worldmaxs[1] - (y * cgs.tydelta);
		end[1]	 = cgs.worldmaxs[1] - (y * cgs.tydelta);

		for (x = 0; x < RainMapSize; x++)
		{
			start[0] = cgs.worldmaxs[0] - (x * cgs.txdelta);
			end[0]	 = cgs.worldmaxs[0] - (x * cgs.txdelta);

			// Move downwards
			trap_CM_BoxTrace( &trace, start, end, NULL, NULL, 0, CONTENTS_SOLID);

			// We might have hit the sky, which is uh, bad.
			// If we DID (due to a poorly caulk'd sky) we have to burrow through it.
			trace.endpos[2] -= 20;

			while (trap_CM_PointContents(trace.endpos, 0) & CONTENTS_SOLID)
			{
				trace.endpos[2] -= 20;
			}
			trap_CM_BoxTrace( &trace, trace.endpos, end, NULL, NULL, 0, CONTENTS_SOLID | CONTENTS_WATER);

			if ((!trace.startsolid) && (trace.fraction != 1))
			{
				q = ((trace.endpos[2] - cgs.worldmins[2]) / levelz) * 256;

				//trace.fraction*255;
				RainMap[x + (y * RainMapSize)] = (char)255 - q;
				// Ok, we've saved it but it might not have been under sky..
				trap_CM_BoxTrace( &trace, trace.endpos, start, NULL, NULL, 0, CONTENTS_SOLID);

				if (!(trace.surfaceFlags & SURF_SKY))
				{
					RainMap[x + (y * RainMapSize)] = 0; // clear it if no sky
				}
			}
		}
	}

//	trap_FS_FOpenFile( "test.raw", &MapFile, FS_WRITE );
//	trap_FS_Write(	RainMap,RainMapSize*RainMapSize, MapFile);
//	trap_FS_FCloseFile(MapFile);
}

//--------------------------------------------------------------------------------
// Spectator Print Data - DensitY
//--------------------------------------------------------------------------------

/*
	Below is some code to improve shoutcasting, hopefully.

	Graphically this isn't pretty but right now it does the job.. If there is a BIG demand
	for a graphical improvement, it'll be done for 2.7
*/

// Extern Variables

extern specplayerinfo_t  PlayerInfo[MAX_CLIENTS];  // from cg_players.c

/*=---- Quick bsp trace test of the player ----=*/

static qboolean CG_TracePlayerSpec( vec3_t Cam, specplayerinfo_t *p )
{
	trace_t  Trace;
	int	 IsVis = qfalse;

	//
	// TODO: Fog and Glass checks - DensitY
	//
	CG_Trace( &Trace, Cam, NULL, NULL, p->XYZ, p->num, MASK_SHOT & ~CONTENTS_BODY  );

	if((Trace.fraction == 1) || (Trace.entityNum == p->num))
	{
		IsVis = qtrue;
	}
	return IsVis;
}

/*=---- Draw Name ----=*/

static void CG_DrawSpectatorDataName( specplayerinfo_t *p, int x, int y )   // x and y are the base coords
{ vec4_t  Color;

	// Change color based on team
  if(p->Team == TEAM_RED || p->Team == TEAM_BLUE) // should put in vector4copy in q_math.c
  {
	  Color[0] = skinInfos[CG_WhichSkin(p->Team)].ui_fl_info_color[0];
	  Color[1] = skinInfos[CG_WhichSkin(p->Team)].ui_fl_info_color[1];
	  Color[2] = skinInfos[CG_WhichSkin(p->Team)].ui_fl_info_color[2];
	  Color[3] = skinInfos[CG_WhichSkin(p->Team)].ui_fl_info_color[3];
  }
  else
  {
	  Color[0] = colorWhite[0];
	  Color[1] = colorWhite[1];
	  Color[2] = colorWhite[2];
	  Color[3] = colorWhite[3];
  }
  CG_Text_Paint( x - CG_Text_Width( p->Name, 0.175f, 0 ) / 2, y, 0.25f, Color, p->Name,
		 0, 0, ITEM_TEXTSTYLE_OUTLINED );
}

/*=---- Draw Health ----=*/

static void CG_DrawSpectatorDataHealth( specplayerinfo_t *p, int x, int y )
{
	vec4_t	BackgroundColor = { 0.0f, 0.0f, 0.0f, 0.3f };
	vec4_t	ForegroundColor = { 1.0f, 0.0f, 0.0f, 0.6f };

	CG_FillRect( x - (50 / 2) + (CG_Text_Width( p->Name, 0.175f, 0 ) / 4), y + 6, 51, 6, BackgroundColor );
	CG_FillRect( x - (50 / 2) + (CG_Text_Width( p->Name, 0.175f, 0 ) / 4) + 1, y + 7, p->Health / 2, 4, ForegroundColor );
}

/*=--- Draw Weapon Text ----=*/

static void CG_DrawSpectatorDataWeapon( specplayerinfo_t *p, int x, int y )
{
	const char  *WeaponText;

	switch(p->Weapon)
	{
		case UT_WP_KNIFE:
			WeaponText = va( "Knife" );
			break;

		case UT_WP_BERETTA:
			WeaponText = va("Berreta 92G"); //va( "Beretta" );
			break;

		case UT_WP_GLOCK:
			WeaponText = va("Glock"); //va( "Glock" );
			break;

		case UT_WP_COLT1911:
            WeaponText = va("Colt 1911");
            break;

		/*
		case UT_WP_MAGNUM:
			WeaponText = va(".44 Magnum"); //va( "Glock" );
			break;
		*/
		case UT_WP_DEAGLE:
			WeaponText = va("IMI DE .50 AE"); //va( "Deagle" );
			break;

		/*case UT_WP_DUAL_BERETTA:
			WeaponText = va("Dual Beretta 92G");
			break;

		case UT_WP_DUAL_DEAGLE:
			WeaponText = va("Dual IMI DE .50 AE");
			break;

		case UT_WP_DUAL_GLOCK:
			WeaponText = va("Dual Glock 9mm");
			break;

		case UT_WP_DUAL_MAGNUM:
			WeaponText = va("Dual .44 Magnum");
			break;
		*/
		case UT_WP_SPAS12:
			WeaponText = va("Franchi SPAS12"); //va( "SPAS12" );
			break;

		/*case UT_WP_BENELLI:
			WeaponText = va("Benelli M4 Super 90"); //va( "Benelli" );
			break;
		*/
		case UT_WP_MP5K:
			WeaponText = va("HK MP5K"); //va( "Mp5k" );
			break;

		case UT_WP_UMP45:
			WeaponText = va("HK UMP45"); //va( "Ump45" );
			break;

		case UT_WP_MAC11:
            WeaponText = va("Mac 11");
            break;

		case UT_WP_HK69:
			WeaponText = va("HK69 40mm"); //va( "HK69" );
			break;

		case UT_WP_LR:
			WeaponText = va("ZM LR300"); //va( "LR-300" );
			break;

		case UT_WP_G36:
			WeaponText = va("HK G36"); //va( "G36" );
			break;

		case UT_WP_PSG1:
			WeaponText = va("HK PSG1"); //va( "Psg1" );
			break;

		case UT_WP_GRENADE_HE:
			WeaponText = va( "He Grenade" );
			break;

		case UT_WP_GRENADE_FLASH:
			WeaponText = va( "Flash Grenade" );
			break;
		
		case UT_WP_GRENADE_SMOKE:
			WeaponText = va( "Smoke Grenade" );
			break;

		case UT_WP_SR8:
			WeaponText = va("Remington SR8"); //va( "SR8" );
			break;

		case UT_WP_AK103:
			WeaponText = va("AK103 7.62mm"); //va( "AK103" );
			break;

		case UT_WP_M4:
			WeaponText = va("M4A1"); //va( "AK103" );
			break;

		/*case UT_WP_P90:
			WeaponText = va("FN P90"); //va( "FN P90" );
			break;
		*/
		case UT_WP_BOMB:
			WeaponText = va( "BOMB" );
			break;

		case UT_WP_NEGEV:
			WeaponText = va("IMI Negev"); //va( "NEGEV" );
			break;
		default:	// No weapon, so print nothing
			WeaponText = va( "" );
			break;
	}
	CG_Text_Paint( x - CG_Text_Width( WeaponText, 0.2f, 0 ) / 2 + 5, y + 25, 0.2f, colorWhite, WeaponText,
		       0, 0, ITEM_TEXTSTYLE_OUTLINED );
}

//
// DensitY: updated Local camera to Screen coord code (thanks highsea),
//			Screenwidth is fixed because drawing functions work on the 640x480 base
//

/*=---- Translates a Local 3d vector to Screen Coords ----=*/

static int VectorToScreenX( vec3_t v )
{
	int    ReturnXCoord;
	float  ScaleX = (640 / 2) / tan( DEG2RAD( cg.refdef.fov_x / 2 ));

	//ReturnXCoord = ( int )( v[ 0 ] * ( 1.0f / v[ 2 ] ) * 320 + ( 320 - 0.5 ) );
	ReturnXCoord = (int)(((ScaleX * v[0]) / v[2]) + (640 / 2));
	return ReturnXCoord;
}

/*=---- Translates a Local 3d vector to Screen Coords ----=*/

static int VectorToScreenY( vec3_t v )
{
	int    ReturnYCoord;
	float  ScaleY = (480 / 2) / tan( DEG2RAD( cg.refdef.fov_y / 2 ));

	//ReturnYCoord = ( int )( 480 - ( v[ 1 ] * ( 1.0f / v[ 2 ] ) * 320 + ( 240 + 0.5 ) ) );
	ReturnYCoord = (int)(((-1.0f * ScaleY * v[1]) / v[2]) + (480 / 2)) ;
	return ReturnYCoord;
}

/*=---- CG_DrawSpectatorData, function needs breaking down ----=*/

void CG_DrawSpectatorData( void )
{
	vec3_t		  vForward, vRight, vUp;
	vec3_t		  Vec;
	vec3_t		  NewVector;
	vec3_t		  CheckVector;
	int		  BaseX, BaseY;
	int		  i;
	specplayerinfo_t  *p;

	if(!cg_SpectatorShoutcaster.integer)
	{
		return;
	}

	if (cgs.clientinfo[cg.snap->ps.clientNum].team != TEAM_SPECTATOR)
	{
		return;
	}

	for(i = 0; i < cg.NumPlayersSnap; i++)
	{
		p = &PlayerInfo[i];
		// build our view vectors (3x3 matrix)
		AngleVectors( cg.refdefViewAngles, vForward, vRight, vUp );
		VectorSubtract( cg.refdef.vieworg, p->XYZ, CheckVector );

		if(DotProduct( CheckVector, vForward ) > 0)	// if behind then no go
		{
			continue;
		}

		if(CG_TracePlayerSpec( cg.refdef.vieworg, p ) == qfalse)     // something blocking it then no go
		{
			continue;
		}
		// translate vector from world to local 3d coord space
		VectorSubtract( p->XYZ, cg.refdef.vieworg, Vec );
		NewVector[0] = DotProduct( Vec, vRight );
		NewVector[1] = DotProduct( Vec, vUp );
		NewVector[2] = DotProduct( Vec, vForward );
		// transform vector from local 3d to 2d space
		BaseX	     = VectorToScreenX( NewVector );
		BaseY	     = VectorToScreenY( NewVector );
		// Draw
		CG_DrawSpectatorDataName( p, BaseX, BaseY );
		CG_DrawSpectatorDataHealth( p, BaseX, BaseY );
		CG_DrawSpectatorDataWeapon( p, BaseX, BaseY );
	}
}

//--------------------------------------------------------------------------------
// On screen bomb site marker - DensitY
//--------------------------------------------------------------------------------

/*
	not at all related to the minimap bombsite markers
*/

/*=---- perform a trace from our viewport to the bombsite returning false if we hit something ----=*/
#if 0
static qboolean CG_TraceBombSite( vec3_t Cam, bombsite_t *b )
{
	trace_t  Trace;
	int	 IsVis = qfalse;

	CG_Trace( &Trace, Cam, NULL, NULL, b->Position, b->EntityNumber, MASK_SHOT );

	// TODO: Trace with View Weapon!
	if((Trace.fraction == 1) || (Trace.entityNum == b->EntityNumber))
	{
		IsVis = qtrue;
	}
	return IsVis;
}
#endif

/*=---- Actual drawing code ----=*/

#if 1
void CG_DrawBombMarker(bombsite_t *b)
{
	float  cycle;

	// If this site doesn't support a marker.
	if (b->noMarker)
	{
		return;
	}

	// Calculate cycle time.
	cycle = (float)fabs(cos((float)cg.time / 300.0f));

	if (b->b)
	{
		CG_ImpactMark(cgs.media.ut_BombMiniMapMarker, b->markerPos, b->markerDir,
			      0, 0, 0, 0, cycle, qfalse, 75.0f * cycle, qtrue, NULL);
	}
	else
	{
		CG_ImpactMark(cgs.media.ut_BombMiniMapMarker, b->markerPos, b->markerDir,
			      0, cycle, cycle, cycle, cycle, qfalse, 75.0f * cycle, qtrue, NULL);
	}
}
#else
/**
 * $(function)
 */
void CG_DrawBombMarker( bombsite_t *b )
{
	refEntity_t  BombMarker;
	float	     bobber;

	memset( &BombMarker, 0, sizeof(BombMarker));
	VectorCopy( b->Position, BombMarker.origin );
	BombMarker.origin[2]	+= 8; // slap it up 8 units
	BombMarker.reType	 = RT_SPRITE;
	BombMarker.customShader  = cgs.media.ut_BombSiteMarker;
//	bobber = 15.0f * sin( cg.time / 50 );
	bobber			 = 10.0f;
	BombMarker.radius	 = bobber;
	//BombMarker.renderfx = ;
	BombMarker.shaderRGBA[0] = 0xff;
	BombMarker.shaderRGBA[1] = 0xff;
	BombMarker.shaderRGBA[2] = 0xff;
	BombMarker.shaderRGBA[3] = 0xff;
	trap_R_AddRefEntityToSceneX( &BombMarker );
}
#endif

/*=---- Draw bombmakers ----=*/

void CG_DrawOnScreenBombMarkers(void)
{
	int	    i;
	vec3_t	    vForward, vRight, vUp;     // rotation matrix split into 3 vectors
	vec3_t	    CheckVector;
	bombsite_t  *bombsite;

	if(cgs.gametype != GT_UT_BOMB)
	{
		return;
	}

	// If bomb exploded.
	if (cg.bombExploded)
	{
		return;
	}

	// loop through max of 2 bomb sites
	AngleVectors( cg.refdefViewAngles, vForward, vRight, vUp );

	for(i = 0; i < cg.NumBombSites; i++)
	{
		bombsite = &cgs.cg_BombSites[i];

		if (i == 1)
		{
			cgs.cg_BombSites[i].b = qtrue;
		}
		else
		{
			cgs.cg_BombSites[i].b = qfalse;
		}
		// build rotation matrix/vectors from current view angle
		// translate bombsite from world to local camera coords
		VectorSubtract( cg.refdef.vieworg, bombsite->Position, CheckVector );

		// check if the bombsite is in front of us
		if(DotProduct( CheckVector, vForward ) > 0)
		{
			continue;
		}
		// check if anything is blocking us from the marker
		//	if( !CG_TraceBombSite( cg.refdef.vieworg, bombsite ) ) {
		//		continue;
		//	}
		// ok lets Draw our polygon!
		CG_DrawBombMarker( bombsite );
	}
}
