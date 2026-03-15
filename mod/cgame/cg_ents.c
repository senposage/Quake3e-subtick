// Copyright (C) 1999-2000 Id Software, Inc.
//
// cg_ents.c -- present snapshot entities, happens every single frame

#include "cg_local.h"
#include "cg_drawaries.h"

qboolean CG_WallhackBlock(	centity_t *cent);

void	 CG_Mr_Sentry(centity_t *cent );
/*
======================
CG_PositionEntityOnTag

Modifies the entities position and axis by the given
tag location
======================
*/
void CG_PositionEntityOnTag( refEntity_t *entity, const refEntity_t *parent,
			     qhandle_t parentModel, char *tagName )
{
	int	       i;
	orientation_t  lerped;

	// lerp the tag
	trap_R_LerpTag( &lerped, parentModel, parent->oldframe, parent->frame,
			1.0 - parent->backlerp, tagName );

	// FIXME: allow origin offsets along tag?
	VectorCopy( parent->origin, entity->origin );

	for (i = 0 ; i < 3 ; i++)
	{
		VectorMA( entity->origin, lerped.origin[i], parent->axis[i], entity->origin );
	}

	// had to cast away the const to avoid compiler problems...
	MatrixMultiply( lerped.axis, ((refEntity_t *)parent)->axis, entity->axis );
	entity->backlerp = parent->backlerp;
}

/*
======================
CG_PositionRotatedEntityOnTag

Modifies the entities position and axis by the given
tag location
======================
*/
void CG_PositionRotatedEntityOnTag( refEntity_t *entity, const refEntity_t *parent,
				    qhandle_t parentModel, char *tagName )
{
	int	       i;
	orientation_t  lerped;
	vec3_t	       tempAxis[3];

//AxisClear( entity->axis );
	// lerp the tag
	trap_R_LerpTag( &lerped, parentModel, parent->oldframe, parent->frame,
			1.0 - parent->backlerp, tagName );

	// FIXME: allow origin offsets along tag?
	VectorCopy( parent->origin, entity->origin );

	for (i = 0 ; i < 3 ; i++)
	{
		VectorMA( entity->origin, lerped.origin[i], parent->axis[i], entity->origin );
	}

	// had to cast away the const to avoid compiler problems...
	MatrixMultiply( entity->axis, lerped.axis, tempAxis );
	MatrixMultiply( tempAxis, ((refEntity_t *)parent)->axis, entity->axis );
}

/*
==========================================================================

FUNCTIONS CALLED EACH FRAME

==========================================================================
*/

/*
======================
CG_SetEntitySoundPosition

Also called by event processing code
======================
*/
void CG_SetEntitySoundPosition( centity_t *cent )
{
	if (cent->currentState.solid == SOLID_BMODEL)
	{
		vec3_t	origin;
		float	*v;

		v = cgs.inlineModelMidpoints[cent->currentState.modelindex];
		VectorAdd( cent->lerpOrigin, v, origin );
//		if (cent->currentState.pos.trType!=TR_INTERPOLATE)
//		{
		trap_S_UpdateEntityPosition( cent->currentState.number, origin );
//		}
	}
	else
	{
//		if (cent->currentState.pos.trType!=TR_INTERPOLATE)
//		{
		trap_S_UpdateEntityPosition( cent->currentState.number, cent->lerpOrigin );
//		}
	}
}

/*
==================
CG_EntityEffects

Add continuous entity effects, like local entity emission and lighting
==================
*/
static void CG_EntityEffects( centity_t *cent )
{
	// If we're in intermission.
	if (cg.firstframeofintermission > 0)
	{
		// Kill entity sound.
		trap_S_StopLoopingSound(cent->currentState.number);

		return;
	}

	// update sound origins
	CG_SetEntitySoundPosition( cent );

	// Special code to handle looping sounds on movers
	if (cent->currentState.eType == ET_MOVER)
	{
		if (cent->currentState.loopSound)
		{
			if (cent->pe.legs.oldFrame == 0)
			{
				trap_S_AddRealLoopingSound( cent->currentState.number, cent->lerpOrigin, vec3_origin,
							    cgs.gameSounds[cent->currentState.loopSound] );
				cent->pe.legs.oldFrame = 1;
			}
		}
		else
		{
			if (cent->pe.legs.oldFrame == 1)
			{
				trap_S_StopLoopingSound ( cent->currentState.number );
				cent->pe.legs.oldFrame = 0;
			}
		}
	}
	// add loop sound
	else if (cent->currentState.loopSound)
	{
		if (cent->currentState.eType != ET_SPEAKER)
		{
			trap_S_AddLoopingSound( cent->currentState.number, cent->lerpOrigin, vec3_origin,
						cgs.gameSounds[cent->currentState.loopSound] );
		}
		else
		{
			// If bomb exploded.
			if (cg.bombExploded)
			{
				trap_S_StopLoopingSound(cent->currentState.number);
			}
			else
			{
				trap_S_AddRealLoopingSound( cent->currentState.number, cent->lerpOrigin, vec3_origin,
							    cgs.gameSounds[cent->currentState.loopSound] );
			}
		}
	}

	// constant light glow // DensitY - don't do this to players because they could be defusing
	if (cent->currentState.constantLight && (cent->currentState.eType != ET_PLAYER))
	{
		int  cl;
		int  i, r, g, b;

		cl = cent->currentState.constantLight;
		r  = cl & 255;
		g  = (cl >> 8) & 255;
		b  = (cl >> 16) & 255;
		i  = ((cl >> 24) & 255) * 4;
		trap_R_AddLightToScene( cent->lerpOrigin, i, r, g, b );
	}
}

/*
==================
CG_General
==================
*/
static void CG_General( centity_t *cent )
{
	refEntity_t    ent;
	entityState_t  *s1;

	s1 = &cent->currentState;

	// if set to invisible, skip
	if (!s1->modelindex)
	{
		return;
	}

	memset (&ent, 0, sizeof(ent));

	// set frame

	ent.frame    = s1->frame;
	ent.oldframe = ent.frame;
	ent.backlerp = 0;

	VectorCopy( cent->lerpOrigin, ent.origin);
	VectorCopy( cent->lerpOrigin, ent.oldorigin);

	if (s1->torsoAnim != 0)
	{
		//we're a ut_goal
		ent.hModel	    = cgs.media.jumpGoalModel;
		cent->lerpAngles[1] = cg.time * 0.25;
	}
	else
	{
		ent.hModel = cgs.gameModels[s1->modelindex];
	}

	// player model
	if (s1->number == cg.snap->ps.clientNum)
	{
		ent.renderfx |= RF_THIRD_PERSON;	// only draw from mirrors
	}

	// convert angles to axis
	AnglesToAxis( cent->lerpAngles, ent.axis );

	// add to refresh list
	trap_R_AddRefEntityToSceneX (&ent);
}

/*
==================
CG_Speaker

Speaker entities can automatically play sounds
==================
*/
static void CG_Speaker( centity_t *cent )
{
	if (!cent->currentState.clientNum)	// FIXME: use something other than clientNum...
	{
		return; 	// not auto triggering
	}

	if (cg.time < cent->miscTime)
	{
		return;
	}

	trap_S_StartSound (NULL, cent->currentState.number, CHAN_ITEM, cgs.gameSounds[cent->currentState.eventParm] );

	//	ent->s.frame = ent->wait * 10;
	//	ent->s.clientNum = ent->random * 10;
	cent->miscTime = cg.time + cent->currentState.frame * 100 + cent->currentState.clientNum * 100 * crandom();
}

/*
==================
CG_Item
==================
*/
static void CG_Item( centity_t *cent )
{
	refEntity_t    ent;
	entityState_t  *es;
	gitem_t        *item;

	es = &cent->currentState;

	if (es->modelindex >= bg_numItems)
	{
		CG_Error( "Bad item index %i on entity", es->modelindex );
	}

	// if set to invisible, skip
	if (!es->modelindex || (es->eFlags & EF_NODRAW))
	{
		return;
	}

	item = &bg_itemlist[es->modelindex];

	//FIXME: has to be a cheaper way to ID them then this?!/
	if (!strcmp(item->classname, "team_CTF_blueflag"))
	{
		cg.blueflagvisible = qtrue;
		VectorCopy(cent->lerpOrigin, cg.blueflagpos);
	}
	else
	if (!strcmp(item->classname, "team_CTF_redflag"))
	{
		cg.redflagvisible = qtrue;
		VectorCopy(cent->lerpOrigin, cg.redflagpos);
	}
	else
	if (!strcmp(item->classname, "ut_weapon_bomb"))
	{
		cg.bombvisible = qtrue;
		VectorCopy(cent->lerpOrigin, cg.bombpos);
	}
	else
	if (!strcmp(item->classname, "ut_planted_bomb"))
	{
		cg.bombvisible = qtrue;
		VectorCopy(cent->lerpOrigin, cg.bombpos);
	}

	/*
	if (es->modelindex == UT_ITEM_APR)
	{
		vec3_t angles = {0, 0, 0};
		int m = (es->time>>16)&0xff;

		cent->currentState.pos.trBase[2] = es->time2;

		C_AriesSetModelIdentity(c_aries[m].lower);
		C_AriesSetModelIdentity(c_aries[m].upper);
		C_AriesSetModelIdentity(c_aries[m].head);
		C_AriesSetModelIdentity(c_aries[m].helmet);

		C_AriesSetModelRotation(c_aries[m].lower,
				cent->currentState.angles);
		C_AriesSetModelRotation(c_aries[m].lower,
				cent->currentState.pos.trBase);

		C_AriesSetModelRotation(c_aries[m].upper,
				cent->currentState.angles2);

		angles[PITCH] = 0;
		angles[YAW] = 0;
		angles[ROLL] = 0;
		C_AriesSetModelRotation(c_aries[m].head, angles);

		C_AriesSetModelAnimation(c_aries[m].lower, es->time & 0xff,
			es->time & 0xff, es->time & 0xff, 0.0f);
		C_AriesSetModelAnimation(c_aries[m].torso, es->time & 0xff,
			es->time & 0xff, es->time & 0xff, 0.0f);

		CG_DrawAries(c_aries[m].lower, cent->currentState.pos.trBase,
				0, 255, 0);
	}
	*/
	memset (&ent, 0, sizeof(ent));

	// autorotate at one of two speeds
	// CRICEL: weapons lying on side angle rotation
	if (item->giType == IT_WEAPON)
	{
		if (!Q_stricmp(item->classname, "ut_weapon_knife"))
		{
			// Dok: knives can be stuck in walls, so their
			// angle is dependant upon the angle at which knife was thrown,
			// and that is set by the server, so we need to retreive that
			// value, which is held in currentState.angles

			// grab angle of knife from server supplied data and turn it into
			// this entity's axis
			VectorCopy( cent->currentState.angles, cent->lerpAngles );
			AnglesToAxis( cent->currentState.angles, ent.axis );

			// grab origin of trace point where knife hit wall from server supplied data
			//VectorCopy( cent->currentState.pos.trBase, ent.origin );
			//VectorCopy( cent->currentState.pos.trBase, cent->lerpOrigin );

			// z axis is length of knife, so we'll use it as a direction
			// vector to move the knife so only the end of the knife is sticking
			// into the wall
			// set origin of knife to 11 units back from wall impact point
			// which is stored in trBase
			VectorScale ( ent.axis[2], -5.5f, ent.origin ); // use ent.origin as a temp vector
			VectorAdd( ent.origin, cent->currentState.pos.trBase, cent->lerpOrigin );
			VectorAdd( ent.origin, cent->currentState.pos.trBase, ent.origin );

			// scale up knife for visibility
			VectorScale( ent.axis[0], 1.2f, ent.axis[0] );
			VectorScale( ent.axis[1], 1.2f, ent.axis[1] );
			VectorScale( ent.axis[2], 1.2f, ent.axis[2] );
			ent.nonNormalizedAxes = qtrue;

			// set model that players will see stuck into the wall
			ent.hModel = cg_items[es->modelindex].models[0];

			// add the model to the scene
			CG_AddWeapon ( &ent );
			return;
		}
		else
		{
			VectorCopy( cg.autoAnglesWeap, cent->lerpAngles );
			cent->lerpAngles[1] = (float)((cent->currentState.number * 672) % 360);
			//AxisCopy( cg.autoAxisWeap, ent.axis );
			AnglesToAxis( cent->lerpAngles, ent.axis );
		}
	}
	else
	{
		VectorCopy( cg.autoAngles, cent->lerpAngles );

		if (item->giType != IT_TEAM)
		{
			cent->lerpAngles[1] = (float)((cent->currentState.number * 672) % 360);
		}

		//AxisCopy( cg.autoAxisWeap, ent.axis );
		AnglesToAxis( cent->lerpAngles, ent.axis );

		// This sucks, but no time to get a new model now
		if (es->modelindex == UT_ITEM_SILENCER)
		{
			VectorScale( ent.axis[0], 0.75f, ent.axis[0] );
			VectorScale( ent.axis[1], 0.75f, ent.axis[1] );
			VectorScale( ent.axis[2], 0.75f, ent.axis[2] );
			ent.nonNormalizedAxes = qtrue;
		}
	}

	// the weapons have their origin where they attatch to player
	// models, so we need to offset them or they will rotate
	// eccentricly
	if ((item->giType == IT_WEAPON) || (item->giType == IT_HOLDABLE))
	{
		cent->lerpOrigin[0] -=
			cg_items[es->modelindex].midpoint[0] * ent.axis[0][0] +
			cg_items[es->modelindex].midpoint[1] * ent.axis[1][0] +
			cg_items[es->modelindex].midpoint[2] * ent.axis[2][0];
		cent->lerpOrigin[1] -=
			cg_items[es->modelindex].midpoint[0] * ent.axis[0][1] +
			cg_items[es->modelindex].midpoint[1] * ent.axis[1][1] +
			cg_items[es->modelindex].midpoint[2] * ent.axis[2][1];
		cent->lerpOrigin[2] -=
			cg_items[es->modelindex].midpoint[0] * ent.axis[0][2] +
			cg_items[es->modelindex].midpoint[1] * ent.axis[1][2] +
			cg_items[es->modelindex].midpoint[2] * ent.axis[2][2];

//		cent->lerpOrigin[2] -= 14;	// bring it down to the ground
		cent->lerpOrigin[2] -= (16 + cg_items[es->modelindex].bottompoint[2] - cg_items[es->modelindex].midpoint[2]);
	}

	ent.hModel = cg_items[es->modelindex].models[0];

	VectorCopy( cent->lerpOrigin, ent.origin);
	VectorCopy( cent->lerpOrigin, ent.oldorigin);

	ent.nonNormalizedAxes = qfalse;

	// if just respawned, slowly scale up
	//msec = cg.time - cent->miscTime;

	// If coming to a stop, play a noise
	if (cent->currentState.groundEntityNum != cent->nextState.groundEntityNum)
	{
		cent->currentState.groundEntityNum = cent->nextState.groundEntityNum;

		if ((item->giType != IT_WEAPON) || (bg_weaponlist[item->giTag].type <= UT_WPTYPE_MEDIUM_GUN))
		{
			trap_S_StartSound ( cent->lerpOrigin, ENTITYNUM_WORLD, CHAN_AUTO, cgs.media.UT_dropPistol );
		}
		else
		{
			trap_S_StartSound ( cent->lerpOrigin, ENTITYNUM_WORLD, CHAN_AUTO, cgs.media.UT_dropRifle );
		}
	}

	//frac = 1.0;

	// items without glow textures need to keep a minimum light value
	// so they are always visible
	if ((item->giType == IT_WEAPON) || (item->giType == IT_HOLDABLE))
	{
		ent.renderfx |= RF_MINLIGHT;
	}

	// increase the size of the weapons when they are presented as items
	if (item->giType == IT_WEAPON)
	{
		VectorScale( ent.axis[0], 1.5, ent.axis[0] );
		VectorScale( ent.axis[1], 1.5, ent.axis[1] );
		VectorScale( ent.axis[2], 1.5, ent.axis[2] );
		ent.nonNormalizedAxes = qtrue;
	}

	// add to refresh list
	//hack for Ctf flags, let their shader draw normally
	if (ent.hModel == cgs.media.flagModels[1])
	{
		ent.hModel = (cgs.gametype == GT_JUMP) ? ent.hModel : cgs.media.flagModels[CG_WhichSkin(TEAM_BLUE)];
		trap_R_AddRefEntityToSceneX( &ent );
	}
	else if (ent.hModel == cgs.media.flagModels[2])
	{
		ent.hModel = (cgs.gametype == GT_JUMP) ? ent.hModel : cgs.media.flagModels[CG_WhichSkin(TEAM_RED)];
		trap_R_AddRefEntityToSceneX( &ent );
	}
	else if (ent.hModel == cgs.media.neutralFlagModel)
	{
		trap_R_AddRefEntityToSceneX( &ent );
	}
	else
	{
		CG_AddWeapon ( &ent );
	}
}

//============================================================================

/*
===============
CG_Missile
===============
*/
static void CG_Missile( centity_t *cent )
{
	refEntity_t	    ent;
	entityState_t	    *s1;
	const weaponInfo_t  *weapon;
	vec3_t		    angles;
	vec3_t		    lastpos;
	int		    lastcontents;
	vec3_t		    up;
	vec3_t		    delta;
	float		    len;
	float		    r, g, b;
	float		    dir, vel;
	utSkins_t           sidx;

	up[0]  = 0;
	up[1]  = 0;
	up[2]  = 1;

	s1     = &cent->currentState;
	weapon = &cg_weapons[s1->weapon];

	// calculate the axis
	VectorCopy( s1->angles, cent->lerpAngles);

	// add trails
	if (weapon->missileTrailFunc)
	{
		weapon->missileTrailFunc( cent, weapon );
	}

	// add dynamic light
	if (weapon->missileDlight)
	{
		trap_R_AddLightToScene(cent->lerpOrigin, weapon->missileDlight,
				       weapon->missileDlightColor[0], weapon->missileDlightColor[1], weapon->missileDlightColor[2] );
	}

	// add missile sound
	if (weapon->missileSound)
	{
		vec3_t	velocity;

		BG_EvaluateTrajectoryDelta( &cent->currentState.pos, cg.time, velocity );

		trap_S_AddLoopingSound( cent->currentState.number, cent->lerpOrigin, velocity, weapon->missileSound );
	}

	// create the render entity
	memset (&ent, 0, sizeof(ent));
	VectorCopy( cent->lerpOrigin, ent.origin);
	VectorCopy( cent->lerpOrigin, ent.oldorigin);

	// flicker between two skins
	ent.skinNum  = cg.clientFrame & 1;
	ent.hModel   = weapon->missileModel;
	ent.renderfx = weapon->missileRenderfx | RF_NOSHADOW;

	switch (s1->weapon)
	{
		case UT_WP_KNIFE:

			// rotate knife while flying: finally worked this FUCKER out
			vectoangles( s1->pos.trDelta, angles );

			if (trap_CM_PointContents( ent.origin, 0 ) & CONTENTS_WATER)
			{
				// do a bubble trail for knife in water
				BG_EvaluateTrajectory( &cent->currentState.pos, cg.snap->serverTime - 50, lastpos );
				lastcontents = CG_PointContents( lastpos, -1 );

				if (lastcontents & CONTENTS_WATER && !(cg.pauseState & UT_PAUSE_ON))
				{
					CG_BubbleTrail( ent.origin, lastpos, 8 );
				}

				// If we're not paused.
				if (!(cg.pauseState & UT_PAUSE_ON))
				{
					angles[0] += (cg.time % 360) * 0.25;	  // rotate slower in water
				}
			}
			else
			{
				// If we're not paused.
				if (!(cg.pauseState & UT_PAUSE_ON))
				{
					angles[0] += (cg.time % 360) * 2;
				}
			}
			AnglesToAxis( angles, ent.axis );

			// scale Mr Knife to be a little bigger for more viz while in air
			VectorScale( ent.axis[0], 2.5, ent.axis[0] );
			VectorScale( ent.axis[1], 2.5, ent.axis[1] );
			VectorScale( ent.axis[2], 2.5, ent.axis[2] );

			// fudge knife origin because the tag_weapon is not
			// in the centre of the weapon
			ent.origin[0] -=
				cg_weapons[UT_WP_KNIFE].weaponMidpoint[0] * ent.axis[0][0] +
				cg_weapons[UT_WP_KNIFE].weaponMidpoint[1] * ent.axis[1][0] +
				cg_weapons[UT_WP_KNIFE].weaponMidpoint[2] * ent.axis[2][0];
			ent.origin[1] -=
				cg_weapons[UT_WP_KNIFE].weaponMidpoint[0] * ent.axis[0][1] +
				cg_weapons[UT_WP_KNIFE].weaponMidpoint[1] * ent.axis[1][1] +
				cg_weapons[UT_WP_KNIFE].weaponMidpoint[2] * ent.axis[2][1];
			ent.origin[02] -=
				cg_weapons[UT_WP_KNIFE].weaponMidpoint[0] * ent.axis[0][2] +
				cg_weapons[UT_WP_KNIFE].weaponMidpoint[1] * ent.axis[1][2] +
				cg_weapons[UT_WP_KNIFE].weaponMidpoint[2] * ent.axis[2][2];

			ent.nonNormalizedAxes = qtrue;

			CG_AddWeapon ( &ent );
			return;

		case UT_WP_GRENADE_FLASH:
		case UT_WP_GRENADE_HE:

			// rotate gren while flying: finally worked this FUCKER out
			vectoangles( s1->pos.trDelta, angles );

			if ((s1->pos.trType != TR_STATIONARY) && !(cg.pauseState & UT_PAUSE_ON))
			{
				angles[1] = ((cg.time / 5) % 360);
				angles[0] = ((cg.time / 3) % 360);
			}

			// do a bubble trail for grenade water
			if (trap_CM_PointContents( ent.origin, 0 ) & CONTENTS_WATER)
			{
				BG_EvaluateTrajectory( &cent->currentState.pos, cg.snap->serverTime - 50, lastpos );
				lastcontents = CG_PointContents( lastpos, -1 );

				if (lastcontents & CONTENTS_WATER && !(cg.pauseState & UT_PAUSE_ON))
				{
					CG_BubbleTrail( ent.origin, lastpos, 8 );
				}
			}

			AnglesToAxis( angles, ent.axis );

			VectorScale( ent.axis[0], 1.55, ent.axis[0] );
			VectorScale( ent.axis[1], 1.55, ent.axis[1] );
			VectorScale( ent.axis[2], 1.55, ent.axis[2] );

			ent.nonNormalizedAxes = qtrue;

			CG_AddWeapon ( &ent );

			return;

		case UT_WP_GRENADE_SMOKE:

			// rotate gren while flying
			vectoangles( s1->pos.trDelta, angles );

			if (s1->powerups == 0) {
				r = 0.6f;
				g = 0.6f;
				b = 0.6f;
			} else {
				sidx = CG_WhichSkin(s1->powerups);
				r = skinInfos[sidx].smoke_color[0];
				g = skinInfos[sidx].smoke_color[1];
				b = skinInfos[sidx].smoke_color[2];
			}

			AnglesToAxis( angles, ent.axis );

			VectorScale( ent.axis[0], 1.55, ent.axis[0] );
			VectorScale( ent.axis[1], 1.55, ent.axis[1] );
			VectorScale( ent.axis[2], 1.55, ent.axis[2] );

			ent.nonNormalizedAxes = qtrue;

			CG_AddWeapon ( &ent );

			// do a bubble trail for grenade water
			if (trap_CM_PointContents( ent.origin, 0 ) & CONTENTS_WATER)
			{
				BG_EvaluateTrajectory( &cent->currentState.pos, cg.snap->serverTime - 50, lastpos );
				lastcontents = CG_PointContents( lastpos, -1 );

				if ((lastcontents & CONTENTS_WATER) && !(cg.pauseState & UT_PAUSE_ON))
				{
					CG_BubbleTrail( ent.origin, lastpos, 8 );
				}
				return; //dont need to smoke
			}

			//Smoke gren smoke
			VectorSubtract( ent.origin, cg.refdef.vieworg, delta );
			len = VectorLength( delta );

			if (s1->pos.trType != TR_STATIONARY)
			{
				// If we're not paused.
				if (!(cg.pauseState & UT_PAUSE_ON))
				{
					angles[1] = ((cg.time / 5) % 360); //rotate
					angles[0] = ((cg.time / 3) % 360);
				}

				dir   = rand() % 1000;
				vel   = rand() % 120;

				up[0] = cos(dir) * vel;
				up[1] = sin(dir) * vel;
				up[2] = 35;
				len   = 1000 + rand() % 200; // life

				if (cg.time > cent->miscTime)  // Make sure that it's not too soon to spawn a new one
				{
					cent->miscTime = cg.time + 50;

					// If we're not paused.
					if (!(cg.pauseState & UT_PAUSE_ON))
					{
						CG_SmokePuff(ent.origin, up, 50 + rand() % 10, r, g, b, 1, len, cg.time, 0, LEF_SHOWLASER, cgs.media.grenadeSmokePuffShader );
					}

					return;
				}
			}
			else
			{
				//Smoke view overlay
				if (len < 200)
				{
					cg.grensmokecolor[0] = r;
					cg.grensmokecolor[1] = g;
					cg.grensmokecolor[2] = b;
					cg.grensmokecolor[3] = ((200 - len) / 200) * 2.0f;

					if (cg.grensmokecolor[3] > 0.9)
					{
						cg.grensmokecolor[3] = 0.9f;
					}
				}
			}

			if ((len > 1000) || (rand() % 1000 > (500 - len))) //reduce the probability of a spawn if close to the gren down to 50% nearest, 100% at 1000 units
			{
				if (cg.time > cent->miscTime)  // Make sure that it's not too soon to spawn a new one
				{
					cent->miscTime = cg.time + 120;
					//if (cg.fps<0)
					//	{
					//		cent->miscTime+=(50-cg.fps);
					//	}

					trap_S_StartSound( ent.origin, ENTITYNUM_WORLD, CHAN_AUTO, cgs.media.UT_smokegrensound );

					dir   = rand() % 1000;
					vel   = rand() % 120;
					up[0] = cos(dir) * vel;
					up[1] = sin(dir) * vel;
					up[2] = 35;

					len   = 3000 + rand() % 3000; // life

					// If we're not paused.
					if (!(cg.pauseState & UT_PAUSE_ON))
					{
						CG_SmokePuff(ent.origin, up, 150 + rand() % 100, r, g, b, 1, len, cg.time, 0, LEF_CLIP | LEF_ALLOWOVERDRAW | LEF_SHOWLASER, cgs.media.grenadeSmokePuffShader );
					}
				}
			}
			return;

		case UT_WP_HK69:

			vectoangles( s1->pos.trDelta, angles );

			if (s1->pos.trType != TR_STATIONARY)
			{
				angles[0] += 90;
			}

			// do a bubble trail for grenade water
			if (trap_CM_PointContents( ent.origin, 0 ) & CONTENTS_WATER)
			{
				BG_EvaluateTrajectory( &cent->currentState.pos, cg.snap->serverTime - 50, lastpos );
				lastcontents = CG_PointContents( lastpos, -1 );

				if ((lastcontents & CONTENTS_WATER) && !(cg.pauseState & UT_PAUSE_ON))
				{
					CG_BubbleTrail( ent.origin, lastpos, 8 );
				}
			}

			AnglesToAxis( angles, ent.axis );

			VectorScale( ent.axis[0], 0.85f, ent.axis[0] );
			VectorScale( ent.axis[1], 0.85f, ent.axis[1] );
			VectorScale( ent.axis[2], 0.85f, ent.axis[2] );
			ent.origin[2]	     += 1.5f;

			ent.nonNormalizedAxes = qtrue;

			CG_AddWeapon ( &ent);

			return;

		case UT_WP_BOMB: // DensitY
			vectoangles( s1->pos.trDelta, angles );
			angles[ROLL] += 90;
			angles[PITCH] = 0;
			angles[YAW]   = 0;
			AnglesToAxis( angles, ent.axis );
			VectorScale( ent.axis[0], 2.0f, ent.axis[0] );
			VectorScale( ent.axis[1], 2.0f, ent.axis[1] );
			VectorScale( ent.axis[2], 2.0f, ent.axis[2] );
			ent.origin[2]	     += 1.5f;
			ent.nonNormalizedAxes = qtrue;
			CG_AddWeapon ( &ent );

			// Backup plant position.
			VectorCopy(cent->lerpOrigin, cg.bombPlantOrigin);

			return;

		default:
			vectoangles( s1->pos.trDelta, angles );
			AnglesToAxis( angles, ent.axis );
			break;
	}

	// convert direction of travel into axis
	if (VectorNormalize2( s1->pos.trDelta, ent.axis[0] ) == 0)
	{
		ent.axis[0][2] = 1;
	}

	// spin as it moves
	if (s1->pos.trType != TR_STATIONARY)
	{
		RotateAroundDirection( ent.axis, cg.time / 4 );
	}
	else
	{
		{
			RotateAroundDirection( ent.axis, s1->apos.trBase[1] );
		}
	}

	CG_AddWeapon ( &ent );
}

/*
===============
CG_Mover
===============
*/
static void CG_Mover( centity_t *cent )
{
	refEntity_t    ent;
	entityState_t  *s1;

	//  vec3_t axis[3];
	// vec3_t mins,maxs;
	s1 = &cent->currentState;

	// create the render entity
	memset (&ent, 0, sizeof(ent));
	VectorCopy( cent->lerpOrigin, ent.origin);
	VectorCopy( cent->lerpOrigin, ent.oldorigin);
	AnglesToAxis( cent->lerpAngles, ent.axis );

	ent.renderfx = RF_NOSHADOW;

	// flicker between two skins (FIXME?)
	ent.skinNum = (cg.time >> 6) & 1;

	// get the model, either as a bmodel or a modelindex
	if (s1->solid == SOLID_BMODEL)
	{
		ent.hModel = cgs.inlineDrawModel[s1->modelindex];

/*	if (s1->time2==CONTENTS_SOLID)
      {
	 trap_R_ModelBounds( ent.hModel, mins, maxs );
	 CG_DrawBBAxis(cent->lerpOrigin,mins,maxs,5,ent.axis);
      }*/
	}
	else
	{
		ent.hModel = cgs.gameModels[s1->modelindex];
	}

	// add to refresh list
	trap_R_AddRefEntityToSceneX(&ent);

	// add the secondary model
	if (s1->modelindex2)
	{
		ent.skinNum = 0;
		ent.hModel  = cgs.gameModels[s1->modelindex2];
		trap_R_AddRefEntityToSceneX(&ent);
	}
}

/*
===============
CG_Beam

Also called as an event
===============
*/
void CG_Beam( centity_t *cent )
{
	refEntity_t    ent;
	entityState_t  *s1;

	s1 = &cent->currentState;

	// create the render entity
	memset (&ent, 0, sizeof(ent));
	VectorCopy( s1->pos.trBase, ent.origin );
	VectorCopy( s1->origin2, ent.oldorigin );
	AxisClear( ent.axis );
	ent.reType   = RT_BEAM;

	ent.renderfx = RF_NOSHADOW;

	// add to refresh list
	trap_R_AddRefEntityToSceneX(&ent);
}

/*
===============
CG_Portal
===============
*/
static void CG_Portal( centity_t *cent )
{
	refEntity_t    ent;
	entityState_t  *s1;

	s1 = &cent->currentState;

	// create the render entity
	memset (&ent, 0, sizeof(ent));
	VectorCopy( cent->lerpOrigin, ent.origin );
	VectorCopy( s1->origin2, ent.oldorigin );
	ByteToDir( s1->eventParm, ent.axis[0] );
	PerpendicularVector( ent.axis[1], ent.axis[0] );

	// negating this tends to get the directions like they want
	// we really should have a camera roll value
	VectorSubtract( vec3_origin, ent.axis[1], ent.axis[1] );

	CrossProduct( ent.axis[0], ent.axis[1], ent.axis[2] );
	ent.reType   = RT_PORTALSURFACE;
	ent.oldframe = 0; //s1->powerups;
	ent.frame    = 0; // s1->frame; 	// rotation speed
	ent.skinNum  = s1->clientNum / 256.0 * 360;	// roll offset

	// add to refresh list
	trap_R_AddRefEntityToSceneX(&ent);
}

/*
=========================
CG_AdjustPositionForMover

Also called by client movement prediction code
=========================
*/
void CG_AdjustPositionForMover( const vec3_t in, const vec3_t ain, int moverNum, int fromTime, int toTime, vec3_t out, vec3_t aout )

//void CG_AdjustPositionForMover( const vec3_t in, int moverNum, int fromTime, int toTime, vec3_t out )
{
	centity_t  *cent;
	vec3_t	   oldOrigin, origin, deltaOrigin;
	vec3_t	   oldAngles, angles, deltaAngles;

	if ((moverNum <= 0) || (moverNum >= ENTITYNUM_MAX_NORMAL))
	{
		VectorCopy( in, out );
		VectorCopy( ain, aout );

		return;
	}

	cent = CG_ENTITIES(moverNum);

	if (cent->currentState.eType != ET_MOVER)
	{
		VectorCopy( in, out );
		VectorCopy( ain, aout );

		return;
	}

	BG_EvaluateTrajectory( &cent->currentState.pos, fromTime, oldOrigin );
	BG_EvaluateTrajectory( &cent->currentState.apos, fromTime, oldAngles );

	BG_EvaluateTrajectory( &cent->currentState.pos, toTime, origin );
	BG_EvaluateTrajectory( &cent->currentState.apos, toTime, angles );

	VectorSubtract( origin, oldOrigin, deltaOrigin );
	VectorSubtract( angles, oldAngles, deltaAngles );

	VectorAdd( in, deltaOrigin, out );
	VectorAdd( ain, deltaAngles, aout );

	// FIXME: origin change when on a rotating object
}

/*
=============================
CG_InterpolateEntityPosition
=============================
*/
static void CG_InterpolateEntityPosition( centity_t *cent )
{
	vec3_t	current, next;
	float	f;

	// it would be an internal error to find an entity that interpolates without
	// a snapshot ahead of the current one
	if (cg.nextSnap == NULL)
	{
		CG_Error( "CG_InterpoateEntityPosition: cg.nextSnap == NULL" );
	}

	f = cg.frameInterpolation;

	// this will linearize a sine or parabolic curve, but it is important
	// to not extrapolate player positions if more recent data is available
	BG_EvaluateTrajectory( &cent->currentState.pos, cg.snap->serverTime, current );
	BG_EvaluateTrajectory( &cent->nextState.pos, cg.nextSnap->serverTime, next );

	cent->lerpOrigin[0] = current[0] + f * (next[0] - current[0]);
	cent->lerpOrigin[1] = current[1] + f * (next[1] - current[1]);
	cent->lerpOrigin[2] = current[2] + f * (next[2] - current[2]);

	BG_EvaluateTrajectory( &cent->currentState.apos, cg.snap->serverTime, current );
	BG_EvaluateTrajectory( &cent->nextState.apos, cg.nextSnap->serverTime, next );

	cent->lerpAngles[0] = LerpAngle( current[0], next[0], f );
	cent->lerpAngles[1] = LerpAngle( current[1], next[1], f );
	cent->lerpAngles[2] = LerpAngle( current[2], next[2], f );
}

/*
===============
CG_CalcEntityLerpPositions

===============
*/
static void CG_CalcEntityLerpPositions( centity_t *cent )
{
	// if this player does not want to see extrapolated players
	if (!cg_smoothClients.integer)
	{
		// make sure the clients use TR_INTERPOLATE
		if (cent->currentState.number < MAX_CLIENTS)
		{
			cent->currentState.pos.trType = TR_INTERPOLATE;
			cent->nextState.pos.trType    = TR_INTERPOLATE;
		}
	}

	if (cent->interpolate && (cent->currentState.pos.trType == TR_INTERPOLATE))
	{
		CG_InterpolateEntityPosition( cent );
		return;
	}

	// first see if we can interpolate between two snaps for
	// linear extrapolated clients
	if (cent->interpolate && (cent->currentState.pos.trType == TR_LINEAR_STOP) &&
	    (cent->currentState.number < MAX_CLIENTS))
	{
		CG_InterpolateEntityPosition( cent );
		return;
	}

	// just use the current frame and evaluate as best we can
	BG_EvaluateTrajectory( &cent->currentState.pos, cg.time, cent->lerpOrigin );
	BG_EvaluateTrajectory( &cent->currentState.apos, cg.time, cent->lerpAngles );

	// adjust for riding a mover if it wasn't rolled into the predicted
	// player state
	if (cent != &cg.predictedPlayerEntity)
	{
		CG_AdjustPositionForMover( cent->lerpOrigin, cent->lerpAngles, cent->currentState.groundEntityNum,
					   cg.snap->serverTime, cg.time, cent->lerpOrigin, cent->lerpAngles );
	}
}

/*
===============
CG_TeamBase
===============
*/
static void CG_TeamBase( centity_t *cent )
{
	refEntity_t  model;

	//@Fenix - add flags for JUMP mode
	if ((cgs.gametype == GT_CTF) || (cgs.gametype == GT_JUMP))
	{
		// show the flag base
		memset(&model, 0, sizeof(model));
		model.reType = RT_MODEL;
		VectorCopy( cent->lerpOrigin, model.lightingOrigin );
		VectorCopy( cent->lerpOrigin, model.origin );
		AnglesToAxis( cent->currentState.angles, model.axis );

		if (cent->currentState.modelindex == TEAM_RED)
		{
			VectorCopy( cent->lerpOrigin, cg.redflagpos );
			cg.redflagvisible = qtrue;
			model.hModel	  = cgs.media.flagBaseModels[(cgs.gametype == GT_JUMP) ? UT_SKIN_RED : CG_WhichSkin(TEAM_RED)];
		}
		else if (cent->currentState.modelindex == TEAM_BLUE)
		{
			VectorCopy( cent->lerpOrigin, cg.blueflagpos );
			cg.blueflagvisible = qtrue;
			model.hModel	  = cgs.media.flagBaseModels[(cgs.gametype == GT_JUMP) ? UT_SKIN_BLUE : CG_WhichSkin(TEAM_BLUE)];
		}
		else
		{
			model.hModel = cgs.media.flagBaseModels[CG_WhichSkin(TEAM_FREE)];
		}
		trap_R_AddRefEntityToSceneX( &model );
	}
	else
	if (cgs.gametype == GT_UT_CAH || cgs.gametype == GT_UT_ASSASIN)
	{
		// show harvester model
		memset(&model, 0, sizeof(model));
		model.reType = RT_MODEL;
		VectorCopy( cent->lerpOrigin, model.lightingOrigin );
		VectorCopy( cent->lerpOrigin, model.origin );
		AnglesToAxis( cent->currentState.angles, model.axis );

		if (cent->currentState.modelindex == TEAM_RED)
		{
			model.hModel = cgs.media.flagBaseModels[CG_WhichSkin(TEAM_RED)];
		}
		else if (cent->currentState.modelindex == TEAM_BLUE)
		{
			model.hModel = cgs.media.flagBaseModels[CG_WhichSkin(TEAM_BLUE)];
		}
		else
		{
			model.hModel = cgs.media.neutralFlagBaseModel;
		}
		trap_R_AddRefEntityToSceneX( &model );
	}
}

/*
===============
CG_AddCEntity

===============
*/
static void CG_AddCEntity( centity_t *cent )
{
	// event-only entities will have been dealt with already
	if (cent->currentState.eType >= ET_EVENTS)
	{
		return;
	}

	// calculate the current origin
	CG_CalcEntityLerpPositions( cent );

	// add automatic effects
	CG_EntityEffects( cent );

	switch (cent->currentState.eType)
	{
		default:
			CG_Error( "Bad entity type: %i\n", cent->currentState.eType );
			break;

		case ET_INVISIBLE:
		case ET_PUSH_TRIGGER:
		case ET_TELEPORT_TRIGGER:
			break;

		case ET_HUDARROW:

			if ((cent->currentState.clientNum > 0) && (cent->currentState.clientNum < MAX_CLIENTS))
			{
				cgs.clientinfo[cent->currentState.clientNum].hasFlag = qfalse;
				cgs.clientinfo[cent->currentState.clientNum].hasBomb = qfalse;

				//@Fenix - JUMP mode flags
				if (((cgs.gametype == GT_CTF) || (cgs.gametype == GT_JUMP)) && ((cgs.clientinfo[cg.predictedPlayerState.clientNum].team == TEAM_SPECTATOR) || (cgs.clientinfo[cg.predictedPlayerState.clientNum].team == cgs.clientinfo[cent->currentState.clientNum].team)))
				{
					if ((cent->currentState.powerups & POWERUP(PW_REDFLAG)) || (cent->currentState.powerups & POWERUP(PW_BLUEFLAG)))
					{
						cgs.clientinfo[cent->currentState.clientNum].hasFlag = qtrue;
					}
				}
				else if (cgs.gametype == GT_UT_BOMB && (cgs.clientinfo[cg.predictedPlayerState.clientNum].team == TEAM_SPECTATOR || cgs.clientinfo[cg.predictedPlayerState.clientNum].team == cgs.clientinfo[cent->currentState.clientNum].team))
				{
					if (cent->currentState.powerups & POWERUP(PW_CARRYINGBOMB))
					{
						cgs.clientinfo[cent->currentState.clientNum].hasBomb = qtrue;
					}
				}
			}

			break;

		case ET_GENERAL:
			CG_General( cent );
			break;

		case ET_PLAYER:
			cgs.clientinfo[cent->currentState.clientNum].hasFlag = qfalse;
			cgs.clientinfo[cent->currentState.clientNum].hasBomb = qfalse;

			//@Fenix - JUMP mode flags
			if (((cgs.gametype == GT_CTF) || (cgs.gametype == GT_JUMP)) && ((cgs.clientinfo[cg.predictedPlayerState.clientNum].team == TEAM_SPECTATOR) || (cgs.clientinfo[cg.predictedPlayerState.clientNum].team == cgs.clientinfo[cent->currentState.clientNum].team)))
			{
				if ((cent->currentState.powerups & POWERUP(PW_REDFLAG)) || (cent->currentState.powerups & POWERUP(PW_BLUEFLAG)))
				{
					cgs.clientinfo[cent->currentState.clientNum].hasFlag = qtrue;
				}
			}
			else if (cgs.gametype == GT_UT_BOMB && (cgs.clientinfo[cg.predictedPlayerState.clientNum].team == TEAM_SPECTATOR || cgs.clientinfo[cg.predictedPlayerState.clientNum].team == cgs.clientinfo[cent->currentState.clientNum].team))
			{
				if (cent->currentState.powerups & POWERUP(PW_CARRYINGBOMB))
				{
					cgs.clientinfo[cent->currentState.clientNum].hasBomb = qtrue;
				}
			}

			CG_BuildPlayerSpectatorList( cent ); // Added - DensitY

//	if (CG_WallhackBlock(cent))
			CG_Player( cent ); //ONLY ever used for living players now

			break;

		case ET_CORPSE:
			CG_Corpse( cent );
			break;

		case ET_PLAYERBB:
			// CG_PlayerBB( cent ); // For Debugging the AntiLag
			CG_DrawAriesEntity( cent );
			break;

		case ET_MR_SENTRY:
			CG_Mr_Sentry( cent );
			break;

		case ET_JUMP_START:
			//CG_Jump_Start( cent );
			break;

		case ET_JUMP_STOP:
			//CG_Jump_Stop( cent );
			break;

		case ET_ITEM:
			CG_Item( cent );
			break;

		case ET_MISSILE:
			CG_Missile( cent );
			break;

		case ET_MOVER:
			CG_Mover( cent );
			break;

		case ET_PORTAL:
			CG_Portal( cent );
			break;

		case ET_SPEAKER:
			CG_Speaker( cent );
			break;

		case ET_TEAM:
			CG_TeamBase( cent );
			break;

		case ET_BOMBSITE:
			CG_AddBombSite( cent );
			break;
//	case ET_LASER:
//		CG_LaserSight (cent);
//		break;
	}
}

/*
===============
CG_AddPacketEntities

===============
*/
void CG_AddPacketEntities( void )
{
	int	       num;
	centity_t      *cent;
	playerState_t  *ps;

	// set cg.frameInterpolation
	if (cg.nextSnap)
	{
		int  delta;

		delta = (cg.nextSnap->serverTime - cg.snap->serverTime);

		if (delta == 0)
		{
			cg.frameInterpolation = 0;
		}
		else
		{
			cg.frameInterpolation = (float)( cg.time - cg.snap->serverTime ) / delta;
		}
	}
	else
	{
		cg.frameInterpolation = 0;	// actually, it should never be used, because
		// no entities should be marked as interpolating
	}
/*
	// the auto-rotating items will all have the same axis
	cg.autoAngles[0] = 0;
	cg.autoAngles[1] = ( cg.time & 2047 ) * 360 / 2048.0;
	cg.autoAngles[2] = 0;

	cg.autoAnglesFast[0] = 0;
	cg.autoAnglesFast[1] = ( cg.time & 1023 ) * 360 / 1024.0f;
	cg.autoAnglesFast[2] = 0;
*/
	//GottaBeKD no rotation
	cg.autoAngles[0]     = 0;
	cg.autoAngles[1]     = 0;
	cg.autoAngles[2]     = 0;

	cg.autoAnglesWeap[0] = 0;
	cg.autoAnglesWeap[1] = 0;
	cg.autoAnglesWeap[2] = 90;

	AnglesToAxis( cg.autoAngles, cg.autoAxis );
	AnglesToAxis( cg.autoAnglesWeap, cg.autoAxisWeap );

	// generate and add the entity from the playerstate
	ps = &cg.predictedPlayerState;
	BG_PlayerStateToEntityState( ps, &cg.predictedPlayerEntity.currentState, qfalse );
	CG_AddCEntity( &cg.predictedPlayerEntity );

	// lerp the non-predicted value for lightning gun origins
	CG_CalcEntityLerpPositions( CG_ENTITIES(cg.snap->ps.clientNum) );

	// add each entity sent over by the server
	for (num = 0 ; num < cg.snap->numEntities ; num++)
	{
		cent = CG_ENTITIES(cg.snap->entities[num].number);

		// If we're paused.
		if (cg.pauseState & UT_PAUSE_ON)
		{
			// If this entity is a player, missile, item or a mover.
			if ((cent->currentState.eType == ET_PLAYER) ||
			    (cent->currentState.eType == ET_MISSILE) ||
			    (cent->currentState.eType == ET_ITEM) ||
			    (cent->currentState.eType == ET_MOVER))
			{
				// Adjust times for pause.
				cent->currentState.pos.trTime  += cg.frametime;
				cent->currentState.apos.trTime += cg.frametime;
			}
		}

		CG_AddCEntity( cent );
	}
}

/*=---- Adds a bombsite to our client side list of bombsites ----=*/

void CG_AddBombSite( centity_t *ce )
{
	bombsite_t  *bomb;
	trace_t     tr;
	vec3_t	    end;

	// If an invalid number of bomb sites.
	if(cg.NumBombSites >= 2)
	{
		return;
	}

	// Add bomb site.
	bomb		   = &cgs.cg_BombSites[cg.NumBombSites];
	bomb->Range	   = ce->currentState.frame;
	bomb->EntityNumber = ce->currentState.number;
	VectorCopy(ce->currentState.origin, bomb->Position);

	// Calculate trace end position.
	end[0] = bomb->Position[0];
	end[1] = bomb->Position[1];
	end[2] = bomb->Position[2] - 300;

	// Trace.
	trap_CM_BoxTrace(&tr, bomb->Position, end, NULL, NULL, 0, CONTENTS_SOLID);

	// If we hit something solid.
	if (tr.fraction < 1.0f)
	{
		VectorCopy(tr.endpos, bomb->markerPos);
		VectorCopy(tr.plane.normal, bomb->markerDir);

		bomb->noMarker = qfalse;
	}
	else
	{
		// No bomb site marker.
		bomb->noMarker = qtrue;
	}

	cg.NumBombSites++;
}

/**
 * $(function)
 */
qboolean CG_WallhackBlock(	centity_t *cent)
{
	vec3_t	 mins, maxs;
	int	 x, y, z;
	vec3_t	 dest;
	vec3_t	 center = { 0, 0, 0};
	int	 count;
	vec4_t	 plane;

	trace_t  trace;

	float	 dotty;

	//Do some quick tests to see if we can completely discard this guy.

	//If we're fairly close...
	VectorSubtract(cent->lerpOrigin, cg.refdef.vieworg, dest);

	if (VectorLength(dest) < 40)
	{
		return qtrue;
	}

	if (VectorLength(dest) > 2000)
	{
		return qtrue;		       //ryadh
	}
	//If we're not in the FOV
	VectorNormalizeNR(dest);
	dotty = DotProduct(dest, cg.refdef.viewaxis[0]);

	if (dotty < -0.1f)
	{
		return qtrue;
	}
	//CG_Printf("Dot %f",dotty);

	mins[0] = -20;
	mins[1] = -20;
	maxs[0] = 20;
	maxs[1] = 20;
	mins[2] = MINS_Z;
	maxs[2] = 42;

	VectorCopy(cg.refdef.vieworg, center);
//   if (cg_debugAW.integer) CG_DrawBB(cent->lerpOrigin,mins,maxs,1,0,0,255);

	VectorAdd(mins, cent->lerpOrigin, mins);
	VectorAdd(maxs, cent->lerpOrigin, maxs);

	count = 0;
	//  return qtrue;
	//Make a view/player vector
//   VectorSubtract(cent->lerpOrigin,center,viewvec);

	//Pick the smart axis's
	if (dest[0] > 0)
	{
		plane[0] = maxs[0];
	}
	else
	{
		plane[0] = mins[0];
	}

	if (dest[1] > 0)
	{
		plane[1] = maxs[1];
	}
	else
	{
		plane[1] = mins[1];
	}

	if (dest[2] < 0)
	{
		plane[2] = maxs[2];
	}
	else
	{
		plane[2] = mins[2];
	}
	center[1] += 4;

	//Top
	for (x = mins[0]; x < maxs[0]; x += 15)
	{
		for (y = mins[1]; y < maxs[1]; y += 15)
		{
			z = plane[2];
			count++;

			dest[0] = x;
			dest[1] = y;
			dest[2] = z;
			trap_CM_BoxTrace(&trace, cg.refdef.vieworg, dest, NULL, NULL, 0, CONTENTS_SOLID | CONTENTS_TRANSLUCENT );

			//  CG_Line(center,dest,1,cgs.media.whiteShaderSolid,white,white);
			if (trace.fraction == 1)
			{
				return qtrue;
			}
			else
			{
				//Glass Checky Thing
				if ((SURFACE_GET_TYPE(trace.surfaceFlags) == SURF_TYPE_GLASS) || (trace.contents & CONTENTS_TRANSLUCENT))
				{
					return qtrue;
				}
			}
		}
	}

	//Side 1
	for (x = mins[2]; x < maxs[2]; x += 15)
	{
		for (y = mins[1]; y < maxs[1]; y += 15)
		{
			z = plane[0];
			count++;

			dest[0] = z;
			dest[1] = y;
			dest[2] = x;
			trap_CM_BoxTrace(&trace, cg.refdef.vieworg, dest, NULL, NULL, 0, CONTENTS_SOLID | CONTENTS_TRANSLUCENT );

			// CG_Line(center,dest,1,cgs.media.whiteShaderSolid,white,white);
			if (trace.fraction == 1)
			{
				return qtrue;
			}
			else
			{
				//Glass Checky Thing
				if ((SURFACE_GET_TYPE(trace.surfaceFlags) == SURF_TYPE_GLASS) || (trace.contents & CONTENTS_TRANSLUCENT))
				{
					return qtrue;
				}
			}
		}
	}

	//Side 2
	for (x = mins[2]; x < maxs[2]; x += 15)
	{
		for (y = mins[0]; y < maxs[0]; y += 15)
		{
			z = plane[1];
			count++;

			dest[0] = y;
			dest[1] = z;
			dest[2] = x;
			trap_CM_BoxTrace(&trace, cg.refdef.vieworg, dest, NULL, NULL, 0, CONTENTS_SOLID | CONTENTS_TRANSLUCENT );

			//CG_Line(center,dest,1,cgs.media.whiteShaderSolid,white,white);
			if (trace.fraction == 1)
			{
				return qtrue;
			}
			else
			{
				//Glass Checky Thing
				if ((SURFACE_GET_TYPE(trace.surfaceFlags) == SURF_TYPE_GLASS) || (trace.contents & CONTENTS_TRANSLUCENT))
				{
					return qtrue;
				}
			}
		}
	}

	//dest[0]=(mins[0]+maxs[0])*0.5;
	//dest[1]=(mins[1]+maxs[1])*0.5;
	//dest[2]=(mins[2]+maxs[2])*0.5;
/*   if (cg_debugAW.integer)
   {
      CG_Line(center,dest,2,cgs.media.whiteShader,red,red);
      CG_Printf("Client: %d Traces: %d - INVISIBLE",cent->currentState.clientNum,count);
   } */

	return qfalse; //Can't be seen.
}

/**
 * $(function)
 */
void CG_Mr_Sentry(centity_t *cent )
{
	refEntity_t  ent;
	vec3_t	     angles;
	float	     det;

	memset (&ent, 0, sizeof(ent));
	VectorCopy(cent->lerpOrigin, ent.origin);

	//ent.origin
	ent.hModel = cgs.media.mrSentryBase;
	ent.reType = RT_MODEL;

	if (cent->currentState.powerups == 1)
	{
		ent.customShader = cgs.media.mrSentryRed;
	}
	else
	{
		ent.customShader = cgs.media.mrSentryBlue;
	}
	ent.nonNormalizedAxes = qtrue;

	// Set orientation.
	VectorSet(angles, cent->lerpAngles[0], cent->lerpAngles[1], 0);

	AnglesToAxis(angles, ent.axis);
	trap_R_AddRefEntityToSceneX (&ent);

	ent.hModel = cgs.media.mrSentryBarrel;

	//If we fired recently
	det = cg.time - cent->currentState.time;

	if ((det > 0) && (det < 2000))
	{
		if (cent->pe.legs.yawAngle < 50)
		{
			cent->pe.legs.yawAngle += 1;
		}
	}
	else
	{
		if (cent->pe.legs.yawAngle > 0)
		{
			cent->pe.legs.yawAngle -= 1;
		}
	}

	//Eject a shell and make firing noises
	if (cent->pe.legs.animationNumber != cent->currentState.generic1)
	{
		trap_S_StartSound(cent->lerpOrigin, cent->currentState.number, CHAN_AUTO, cgs.media.mrSentryFire);
		CG_EjectBullet(cent, &ent, 1, 1);
		cent->pe.legs.animationNumber = cent->currentState.generic1;
	}

	//Play Spinup noise
	if (cent->pe.torso.animationNumber != cent->currentState.time2)
	{
		trap_S_StartSound(cent->lerpOrigin, cent->currentState.number, CHAN_AUTO, cgs.media.mrSentrySpinUp);
		cent->pe.torso.animationNumber = cent->currentState.time2;
	}

	//Play Spindown noise
	if (cent->pe.torso.frame != cent->currentState.torsoAnim)
	{
		trap_S_StartSound(cent->lerpOrigin, cent->currentState.number, CHAN_AUTO, cgs.media.mrSentrySpinDown);
		cent->pe.torso.frame = cent->currentState.torsoAnim;
	}

	cent->pe.legs.pitchAngle += cent->pe.legs.yawAngle;

	VectorSet(angles, cent->lerpAngles[0], cent->lerpAngles[1], cent->pe.legs.pitchAngle);

	//VectorSet(angles,angles[0],angles[1] ,0);

	AnglesToAxis(angles, ent.axis);

	CG_AddWeaponShiney(&ent);
}

//@Barba: start line for jump mode
/*
void CG_Jump_Start(centity_t *cent )
{
	refEntity_t  ent;
	vec3_t	     angles;

	memset (&ent, 0, sizeof(ent));
	VectorCopy(cent->lerpOrigin, ent.origin);

	//ent.origin
	ent.hModel = cgs.media.jumpStartModel;
	ent.reType = RT_MODEL;

	// Set orientation.
	VectorSet(angles, cent->lerpAngles[0], cent->lerpAngles[1], 0);

	AnglesToAxis(angles, ent.axis);
	trap_R_AddRefEntityToSceneX (&ent);
}

//@Barba: finish line for jump mode
void CG_Jump_Stop(centity_t *cent )
{
	refEntity_t  ent;
	vec3_t	     angles;

	memset (&ent, 0, sizeof(ent));
	VectorCopy(cent->lerpOrigin, ent.origin);

	//ent.origin
	ent.hModel = cgs.media.jumpStopModel;
	ent.reType = RT_MODEL;

	// Set orientation.
	VectorSet(angles, cent->lerpAngles[0], cent->lerpAngles[1], 0);

	AnglesToAxis(angles, ent.axis);
	trap_R_AddRefEntityToSceneX (&ent);
}
*/
