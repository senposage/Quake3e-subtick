// Copyright (C) 1999-2000 Id Software, Inc.
//
// cg_weapons.c -- events and effects dealing with weapons
#include "cg_local.h"
#include "../common/c_players.h"

#define MAX_SMOKEPOS   64
#define BK_LASER_SCALE 1.5f
// 27 - Where should I put this? ;)
int	       NextSmokeposIndex;
localEntity_t  *SmokePos[MAX_SMOKEPOS];

///
#define MAX_BBS 32
vec3_t	BeeBeePos[MAX_BBS];
int	BeeBeeTime[MAX_BBS];
///////

vec3_t	markt[6] = {
	{  0,  0,  1},
	{  0,  0, -1},
	{  0,  1,  0},
	{  0, -1,  0},
	{  1,  0,  0},
	{ -1,  0,  0},
};

/**
 * $(function)
 */
void CG_DrawPersistantBBs(void)
{
	int	j;
	vec3_t	mins = { -10, -10, -10};
	vec3_t	maxs = { 10, 10, 10};

	for (j = 0; j < MAX_BBS; j++)
	{
		if (BeeBeeTime[j] > cg.time)
		{
			CG_DrawBB(BeeBeePos[j], mins, maxs, 1, 0, 255, 255);
		}
	}
}

/**
 * $(function)
 */
void CG_AddPersistantBB(vec3_t pos)
{
	int  j;

	for (j = 0; j < MAX_BBS; j++)
	{
		if (BeeBeeTime[j] < cg.time)
		{
			BeeBeeTime[j]	= cg.time + 3000;
			BeeBeePos[j][0] = pos[0];
			BeeBeePos[j][1] = pos[1];
			BeeBeePos[j][2] = pos[2];
			break;
		}
	}
}

/**
 * $(function)
 */
void CG_DebugPlayerPos(void)
{
	int	      i;
	clientInfo_t  *ci;
	centity_t     *cent;

	for (i = 0 ; i < MAX_CLIENTS ; i++)
	{
		ci = &cgs.clientinfo[i];

		if (!ci->infoValid)
		{
			continue;
		}

		cent = CG_ENTITIES(i);

		if (cent->currentValid)
		{
			CG_AddPersistantBB(cent->lerpOrigin);
		}
	}
}

/**
 * $(function)
 */
qboolean RaySphere( vec3_t rayorigin, vec3_t raydir, vec3_t sphereorigin, float radius, vec3_t intersection1, vec3_t intersection2 )
{
	float	det;
	vec3_t	otherorigin;
	vec3_t	otherdir;
	vec3_t	dist;
	float	xma = rayorigin[0] - sphereorigin[0];
	float	ymb = rayorigin[1] - sphereorigin[1];
	float	zmc = rayorigin[2] - sphereorigin[2];
	float	t1;
	float	b, c;

	VectorSubtract(rayorigin, sphereorigin, dist);

	if (VectorLength(dist) < radius)
	{
		intersection1[0] = rayorigin[0];
		intersection1[1] = rayorigin[1];
		intersection1[2] = rayorigin[2];
	}
	else
	{
		// a is always 1 because our ray is normalized
		b   = 2 * (raydir[0] * xma + raydir[1] * ymb + raydir[2] * zmc);
		c   = xma * xma + ymb * ymb + zmc * zmc - radius * radius;

		det = b * b - 4 * c;

		if (det >= 0)
		{
			det = sqrt(det);

			t1  = (-b - det) * 0.5f;

			if (t1 <= 0)
			{
				return qfalse;
			}

			intersection1[0] = rayorigin[0] + (raydir[0] * t1);
			intersection1[1] = rayorigin[1] + (raydir[1] * t1);
			intersection1[2] = rayorigin[2] + (raydir[2] * t1);
		}
	}

	// Ok, now find out where the exit point was.
	VectorMA(rayorigin, radius * 3, raydir, otherorigin);
	otherdir[0] = -raydir[0];
	otherdir[1] = -raydir[1];
	otherdir[2] = -raydir[2];
	xma	    = otherorigin[0] - sphereorigin[0];
	ymb	    = otherorigin[1] - sphereorigin[1];
	zmc	    = otherorigin[2] - sphereorigin[2];
	b	    = 2 * (otherdir[0] * xma + otherdir[1] * ymb + otherdir[2] * zmc);
	c	    = xma * xma + ymb * ymb + zmc * zmc - radius * radius;
	det	    = b * b - 4 * c;

	if (det >= 0)
	{
		det = sqrt(det);
		t1  = (-b - det) * 0.5f;
		//	if (t1<=0)
		//	{
		//		return qfalse;
		//	}
		intersection2[0] = otherorigin[0] + (otherdir[0] * t1);
		intersection2[1] = otherorigin[1] + (otherdir[1] * t1);
		intersection2[2] = otherorigin[2] + (otherdir[2] * t1);
		return qtrue;
	}

	return qfalse;
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name       : CG_Eject
// Description: Ejects a fragment with teh given model from the given weapon
// Author     : Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
static localEntity_t *CG_Eject ( centity_t *cent, refEntity_t *parent, qhandle_t model, float lengthScale, float diameterScale )
{
	refEntity_t    ejector;
	localEntity_t  *le;
	refEntity_t    *re;
	float	       waterScale = 1.0f;
	vec3_t	       angles;
	int	       brasstime;

	brasstime = cg_sfxBrassTime.integer;

	if (brasstime > 20000)
	{
		brasstime = 20000;
	}

	// Double check
	if (cg_sfxBrassTime.integer <= 0)
	{
		return NULL;
	}

	// Set up the ejector refentity so we can find its location on the parent model
	memset( &ejector, 0, sizeof(ejector));
	VectorCopy( parent->lightingOrigin, ejector.lightingOrigin );
	//ejector.shadowPlane = ejector.shadowPlane;
	//ejector.renderfx    = ejector.renderfx;

	// Find its location
	CG_PositionEntityOnTag( &ejector, parent, parent->hModel, "tag_eject");

	// Now its time to make the shell object
	le = CG_AllocLocalEntity();
	re = &le->refEntity;

	// If in first person we have to hack it so its not behind the hands
	if (parent->renderfx & RF_FIRST_PERSON)
	{
		re->renderfx |= RF_DEPTHHACK;
	}

	// Initialize the entity
	le->leType     = LE_FRAGMENT;
	le->startTime  = cg.time;
	le->endTime    = le->startTime + brasstime + (brasstime / 4) * random();
	le->pos.trType = TR_GRAVITY;
	le->pos.trTime = cg.time - (rand() & 15);

	// Set the origin
	VectorCopy( ejector.origin, re->origin );
	VectorCopy( re->origin, le->pos.trBase );

	// IF we are in water then slow stuff down
	if (CG_PointContents( re->origin, -1 ) & CONTENTS_WATER)
	{
		waterScale = 0.10f;
	}

	// Set its velocity
	VectorScale ( ejector.axis[0], 50 + rand() % 100, le->pos.trDelta );
	VectorAdd ( cent->currentState.pos.trDelta, le->pos.trDelta, le->pos.trDelta );

	// Up a bit
	le->pos.trDelta[2] = 100;

	// Rotate it a bit, the shell isnt rotated properly to start with
	angles[YAW]   = 0;
	angles[PITCH] = 90;
	angles[ROLL]  = 0;
	AnglesToAxis( angles, re->axis );

	// Set its model
	re->hModel = model;

	// Set its bounce and tumble information
	le->bounceFactor	 = 0.4 * waterScale;
	le->angles.trType	 = TR_LINEAR;
	le->angles.trTime	 = cg.time;
	le->angles.trBase[PITCH] = 70;
	le->angles.trDelta[0]	 = 720;
	le->angles.trDelta[1]	 = 90;
	le->angles.trDelta[2]	 = 180;
	le->leFlags		 = LEF_TUMBLE;
	le->leBounceSoundType	 = LEBS_BRASS;
	le->leMarkType		 = LEMT_NONE;

	le->scale[0]		 = diameterScale;
	le->scale[1]		 = diameterScale;
	le->scale[2]		 = lengthScale;

	return le;
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name       : CG_EjectBullet
// Description: Ejects a bullet
// Author     : Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
void CG_EjectBullet ( centity_t *cent, refEntity_t *parent, float lengthScale, float diameterScale )
{
	localEntity_t  *shell = CG_Eject ( cent, parent, cgs.media.machinegunBrassModel, lengthScale, diameterScale );

	if (shell)
	{
		shell->bounceSound = cgs.media.UT_brassBounceSound[rand() % 2];
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name       : CG_EjectShell
// Description: Ejects a shell
// Author     : Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
static void CG_EjectShell ( centity_t *cent, refEntity_t *parent, float lengthScale, float diameterScale )
{
	localEntity_t  *shell = CG_Eject ( cent, parent, cgs.media.shotgunBrassModel, lengthScale, diameterScale );

	if (shell)
	{
		shell->bounceSound = cgs.media.UT_shellBounceSound[rand() % 3];
	}
}

#ifdef MISSIONPACK
#if 0
/*
==========================
CG_NailgunEjectBrass
==========================
*/
static void CG_NailgunEjectBrass( centity_t *cent )
{
	localEntity_t  *smoke;
	vec3_t	       origin;
	vec3_t	       v[3];
	vec3_t	       offset;
	vec3_t	       xoffset;
	vec3_t	       up;

	AnglesToAxis( cent->lerpAngles, v );

	offset[0]  = 0;
	offset[1]  = -12;
	offset[2]  = 24;

	xoffset[0] = offset[0] * v[0][0] + offset[1] * v[1][0] + offset[2] * v[2][0];
	xoffset[1] = offset[0] * v[0][1] + offset[1] * v[1][1] + offset[2] * v[2][1];
	xoffset[2] = offset[0] * v[0][2] + offset[1] * v[1][2] + offset[2] * v[2][2];
	VectorAdd( cent->lerpOrigin, xoffset, origin );

	VectorSet( up, 0, 0, 64 );

	smoke	      = CG_SmokePuff( origin, up, 32, 1, 1, 1, 0.33f, 700, cg.time, 0, 0, cgs.media.smokePuffShader );
	// use the optimized local entity add
	smoke->leType = LE_SCALE_FADE;
}
#endif
#endif

/*
==========================
CG_RocketTrail
==========================
*/
static void CG_RocketTrail( centity_t *ent, const weaponInfo_t *wi )
{
	int	       step;
	vec3_t	       origin, lastPos;
	int	       t;
	int	       startTime, contents;
	int	       lastContents;
	entityState_t  *es;
	vec3_t	       up;
	localEntity_t  *smoke;

	// If we're paused.
	if (cg.pauseState & UT_PAUSE_ON)
	{
		return;
	}

	up[0]	  = 0;
	up[1]	  = 0;
	up[2]	  = 0;

	step	  = 50;

	es	  = &ent->currentState;
	startTime = ent->miscTime;
	t	  = step * ((startTime + step) / step);

	BG_EvaluateTrajectory( &es->pos, cg.time, origin );
	contents = CG_PointContents( origin, -1 );

	// if object (e.g. grenade) is stationary, don't toss up smoke
	if (es->pos.trType == TR_STATIONARY)
	{
		ent->miscTime = cg.time;
		return;
	}

	BG_EvaluateTrajectory( &es->pos, ent->miscTime, lastPos );
	lastContents  = CG_PointContents( lastPos, -1 );

	ent->miscTime = cg.time;

	if (contents & (CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA))
	{
		if (contents & lastContents & CONTENTS_WATER)
		{
			CG_BubbleTrail( lastPos, origin, 8 );
		}
		return;
	}

	for ( ; t <= ent->miscTime ; t += step)
	{
		BG_EvaluateTrajectory( &es->pos, t, lastPos );

		smoke = CG_SmokePuff( lastPos, up,
				      wi->trailRadius,
				      1, 1, 1, 0.33f,
				      wi->wiTrailTime,
				      t,
				      0,
				      0,
				      cgs.media.shotgunSmokePuffShader );
		// use the optimized local entity add
		smoke->leType = LE_SCALE_FADE;
	}
}

/*
==========================
CG_GrenadeTrail
==========================
*/
static void CG_GrenadeTrail( centity_t *ent, const weaponInfo_t *wi )
{
	CG_RocketTrail( ent, wi );
}

//#define MAX_VIEWWEAPON_TAGS		11
#define 	MAX_VIEWWEAPON_TAGS 20

typedef struct utViewWeaponTags_s
{
	char	   *tag;
	char	   *model;
	char	   *skin;
	qhandle_t  hModel;
	qhandle_t  hSkin;
} utViewWeaponTags_t;

utViewWeaponTags_t  cg_viewWeaponTags[][MAX_VIEWWEAPON_TAGS] = {
	{ { 0 }},

	// Knife
	{ { 0 }},

	// Beretta
	{
		{ "tag_main", "models/weapons2/beretta/beretta_view_main.md3" },
		{ "tag_clip", "models/weapons2/beretta/beretta_view_clip.md3" },
		{ "tag_cock", "models/weapons2/beretta/beretta_view_cock.md3" },
		{ "tag_safety", "models/weapons2/beretta/beretta_view_safety.md3" },
		{ "tag_slide", "models/weapons2/beretta/beretta_view_slide.md3" },
		{ "tag_sliderel", "models/weapons2/beretta/beretta_view_sliderel.md3" },
		{ "tag_trigger", "models/weapons2/beretta/beretta_view_trigger.md3" },
	},

	// Deagle
	{
		{ "tag_main", "models/weapons2/deserteagle/deagle_view_main.md3" },
		{ "tag_clip", "models/weapons2/deserteagle/deagle_view_clip.md3" },
		{ "tag_cock", "models/weapons2/deserteagle/deagle_view_cock.md3" },
		{ "tag_safety", "models/weapons2/deserteagle/deagle_view_safety.md3" },
		{ "tag_slide", "models/weapons2/deserteagle/deagle_view_slide.md3" },
		{ "tag_sliderel", "models/weapons2/deserteagle/deagle_view_sliderel.md3" },
		{ "tag_trigger", "models/weapons2/deserteagle/deagle_view_trigger.md3" },
	},

	// Spas12
	{
		{ "tag_main", "models/weapons2/spas12/spas12_view_main.md3" },
		{ "tag_ejector", "models/weapons2/spas12/spas12_view_ejector.md3" },
		{ "tag_slide", "models/weapons2/spas12/spas12_view_slide.md3" },
	},

	// MP5K
	{
		{ "tag_main", "models/weapons2/mp5k/mp5k_view_main.md3" },
		{ "tag_bolt", "models/weapons2/mp5k/mp5k_view_bolt.md3" },
		{ "tag_clip", "models/weapons2/mp5k/mp5k_view_clip.md3" },
		{ "tag_cliprel", "models/weapons2/mp5k/mp5k_view_cliprel.md3" },
		{ "tag_ejector", "models/weapons2/mp5k/mp5k_view_ejector.md3" },
		{ "tag_mode", "models/weapons2/mp5k/mp5k_view_mode.md3" },
		{ "tag_trigger", "models/weapons2/mp5k/mp5k_view_trigger.md3" },
	},

	// UMP45
	{
		{ "tag_main", "models/weapons2/ump45/ump45_view_main.md3" },
		{ "tag_bolt", "models/weapons2/ump45/ump45_view_bolt.md3" },
		{ "tag_clip", "models/weapons2/ump45/ump45_view_clip.md3" },
		{ "tag_ejector", "models/weapons2/ump45/ump45_view_ejector.md3" },
		{ "tag_mode", "models/weapons2/ump45/ump45_view_mode.md3" },
		{ "tag_trigger", "models/weapons2/ump45/ump45_view_trigger.md3" },
	},

	// HK69
	{
		{ "tag_aim", "models/weapons2/hk69/hk69_view_aim.md3" },
		{ "tag_barrel", "models/weapons2/hk69/hk69_view_barrel.md3" },
		{ "tag_butt", "models/weapons2/hk69/hk69_view_butt.md3" },
		{ "tag_cock", "models/weapons2/hk69/hk69_view_cock.md3" },
		{ "tag_grenade", "models/weapons2/hk69/hk69_view_grenade.md3" },
		{ "tag_main", "models/weapons2/hk69/hk69_view_main.md3" },
		{ "tag_safety", "models/weapons2/hk69/hk69_view_safety.md3" },
		{ "tag_shell", "models/weapons2/hk69/hk69_view_shell.md3" },
		{ "tag_trigger", "models/weapons2/hk69/hk69_view_trigger.md3" },
	},

	// LR
	{
		{ "tag_main", "models/weapons2/zm300/lr_view_main.md3" },
		{ "tag_bolt", "models/weapons2/zm300/lr_view_bolt.md3" },
		{ "tag_clip", "models/weapons2/zm300/lr_view_clip.md3" },
		{ "tag_ejector", "models/weapons2/zm300/lr_view_ejector.md3" },
		{ "tag_flap", "models/weapons2/zm300/lr_view_flap.md3" },
		{ "tag_mode", "models/weapons2/zm300/lr_view_mode.md3" },
		{ "tag_trigger", "models/weapons2/zm300/lr_view_trigger.md3" },
	},

	// G36
	{
		{ "tag_main", "models/weapons2/g36/g36_view_main.md3" },
		{ "tag_clip", "models/weapons2/g36/g36_view_clip.md3" },
		{ "tag_cliprel", "models/weapons2/g36/g36_view_cliprel.md3" },
		{ "tag_ejector", "models/weapons2/g36/g36_view_ejector.md3" },
		{ "tag_mode", "models/weapons2/g36/g36_view_mode.md3" },
		{ "tag_trigger", "models/weapons2/g36/g36_view_trigger.md3" },
	},

	// PSG1
	{
		{ "tag_bolt", "models/weapons2/psg1/psg1_view_bolt.md3" },
//		{ "tag_capbk",		"models/weapons2/psg1/psg1_view_capbk.md3" },
//		{ "tag_capfr",		"models/weapons2/psg1/psg1_view_capfr.md3" },
		{ "tag_clip", "models/weapons2/psg1/psg1_view_clip.md3" },
		{ "tag_ejector", "models/weapons2/psg1/psg1_view_ejector.md3" },
		{ "tag_main", "models/weapons2/psg1/psg1_view_main.md3" },
		{ "tag_mode", "models/weapons2/psg1/psg1_view_mode.md3" },
		{ "tag_trigger", "models/weapons2/psg1/psg1_view_trigger.md3" },
	},

	// HE-GREN
	{
		{ "tag_main", "models/weapons2/grenade/grenade_view_main.md3", "models/weapons2/grenade/grenade_view_main_he.skin" },
		{ "tag_flap", "models/weapons2/grenade/grenade_view_flap.md3" },
		{ "tag_ping", "models/weapons2/grenade/grenade_view_ping.md3" },
		{ "tag_ring", "models/weapons2/grenade/grenade_view_ring.md3" },
	},

	// FLASH-GREN
	{
		{ "tag_main", "models/weapons2/grenade/grenade_view_main.md3", "models/weapons2/grenade/grenade_view_main_flash.skin" },
		{ "tag_flap", "models/weapons2/grenade/grenade_view_flap.md3" },
		{ "tag_ping", "models/weapons2/grenade/grenade_view_ping.md3" },
		{ "tag_ring", "models/weapons2/grenade/grenade_view_ring.md3" },
	},
	
	// SMOKE-GREN
	{
		{ "tag_main", "models/weapons2/grenade/grenade_view_main.md3", "models/weapons2/grenade/grenade_view_main_smoke.skin" },
		{ "tag_flap", "models/weapons2/grenade/grenade_view_flap.md3" },
		{ "tag_ping", "models/weapons2/grenade/grenade_view_ping.md3" },
		{ "tag_ring", "models/weapons2/grenade/grenade_view_ring.md3" },
	},

	// SR-8
	{
		{ "tag_bolt", "models/weapons2/sr8/sr8_view_bolt.md3" },
		{ "tag_clip", "models/weapons2/sr8/sr8_view_clip.md3" },
		{ "tag_ejector", "models/weapons2/sr8/sr8_view_ejector.md3" },
		{ "tag_main", "models/weapons2/sr8/sr8_view_main.md3" },
		{ "tag_mode", "models/weapons2/sr8/sr8_view_mode.md3" },
		{ "tag_trigger", "models/weapons2/sr8/sr8_view_trigger.md3" },
	},

	// AK103
	{
		{ "tag_clip", "models/weapons2/ak103/ak103_view_clip.md3" },
		{ "tag_ejector", "models/weapons2/ak103/ak103_view_ejector.md3" },
		{ "tag_main", "models/weapons2/ak103/ak103_view_main.md3" },
		{ "tag_mode", "models/weapons2/ak103/ak103_view_mode.md3" },
		{ "tag_trigger", "models/weapons2/ak103/ak103_view_trigger.md3" }
	},
	// BOMB
	{
		{ "tag_main", "models/weapons2/bombbag/bombbag_view_main.md3" },
		{ "tag_clip1", "models/weapons2/bombbag/bombbag_view_clip1.md3" },
		{ "tag_clip2", "models/weapons2/bombbag/bombbag_view_clip2.md3" },
		{ "tag_knob1", "models/weapons2/bombbag/bombbag_view_knob1.md3" },
		{ "tag_knob2", "models/weapons2/bombbag/bombbag_view_knob2.md3" },
		{ "tag_button", "models/weapons2/bombbag/bombbag_view_button.md3" },
		{ "tag_safety", "models/weapons2/bombbag/bombbag_view_safety.md3" }
	},

	// NEGEV
	{
		{ "tag_bolt", "models/weapons2/negev/negev_view_bolt.md3" },
		{ "tag_box", "models/weapons2/negev/negev_view_box.md3" },
		{ "tag_flap", "models/weapons2/negev/negev_view_flap.md3" },
		{ "tag_flap1", "models/weapons2/negev/negev_view_flap1.md3" },
		{ "tag_flap2", "models/weapons2/negev/negev_view_flap2.md3" },
		{ "tag_handle", "models/weapons2/negev/negev_view_handle.md3" },
		{ "tag_main", "models/weapons2/negev/negev_view_main.md3" },
		{ "tag_mode", "models/weapons2/negev/negev_view_mode.md3" },
		{ "tag_trigger", "models/weapons2/negev/negev_view_trigger.md3" },
		// Bullet tags
		{ "tag_bul01", "models/weapons2/negev/negev_view_bullet.md3" },
		{ "tag_bul02", "models/weapons2/negev/negev_view_bullet.md3" },
		{ "tag_bul03", "models/weapons2/negev/negev_view_bullet.md3" },
		{ "tag_bul04", "models/weapons2/negev/negev_view_bullet.md3" },
		{ "tag_bul05", "models/weapons2/negev/negev_view_bullet.md3" },
		{ "tag_bul06", "models/weapons2/negev/negev_view_bullet.md3" },
		{ "tag_bul07", "models/weapons2/negev/negev_view_bullet.md3" },
		{ "tag_bul08", "models/weapons2/negev/negev_view_bullet.md3" },
		{ "tag_bul09", "models/weapons2/negev/negev_view_bullet.md3" },
		{ "tag_bul10", "models/weapons2/negev/negev_view_bullet.md3" }
	},

	//@Barbatos: frag gren
	{ {NULL, NULL} },

	// M4
	{
		{ "tag_main", "models/weapons2/m4/m4_view_main.md3" },
		{ "tag_bolt", "models/weapons2/m4/m4_view_bolt.md3" },
		{ "tag_clip", "models/weapons2/m4/m4_view_clip.md3" },
		{ "tag_ejector", "models/weapons2/m4/m4_view_ejector.md3" },
		{ "tag_flap", "models/weapons2/m4/m4_view_flap.md3" },
		{ "tag_mode", "models/weapons2/m4/m4_view_mode.md3" },
		{ "tag_trigger", "models/weapons2/m4/m4_view_trigger.md3" },
	},

	// Glock
	{
		{ "tag_main", "models/weapons2/glock/glock_view_main.md3" },
		{ "tag_clip", "models/weapons2/glock/glock_view_clip.md3" },
		{ "tag_cock", "models/weapons2/glock/glock_view_cock.md3" },
		{ "tag_safety", "models/weapons2/glock/glock_view_safety.md3" },
		{ "tag_slide", "models/weapons2/glock/glock_view_slide.md3" },
		{ "tag_sliderel", "models/weapons2/glock/glock_view_sliderel.md3" },
		{ "tag_trigger", "models/weapons2/glock/glock_view_trigger.md3" },
	},

	// Colt 1911
    {
        { "tag_main",     "models/weapons2/colt1911/colt1911_view_main.md3"         },
        { "tag_clip",     "models/weapons2/colt1911/colt1911_view_clip.md3"         },
        { "tag_cock",     "models/weapons2/colt1911/colt1911_view_cock.md3"         },
        { "tag_slide",    "models/weapons2/colt1911/colt1911_view_slide.md3"        },
    },

    // Mac 11
    {
        { "tag_main",     "models/weapons2/mac11/mac11_view_main.md3"               },
        { "tag_clip",     "models/weapons2/mac11/mac11_view_clip.md3"               },
        { "tag_cock",     "models/weapons2/mac11/mac11_view_cock.md3"               },
    },

	// P90
	/*{
		{ "tag_main", "models/weapons2/p90/p90_view_main.md3" },
		{ "tag_bolt", "models/weapons2/p90/p90_view_bolt.md3" },
		{ "tag_clip", "models/weapons2/p90/p90_view_clip.md3" },
		{ "tag_trigger", "models/weapons2/p90/p90_view_trigger.md3" },
	},

	// Benelli
	{
		{ "tag_main", "models/weapons2/benellim9/benellim9_view_main.md3" },
		{ "tag_ejector", "models/weapons2/benellim9/benellim9_view_ejector.md3" },
		{ "tag_slide", "models/weapons2/benellim9/benellim9_view_slide.md3" },
		{ "tag_trigger", "models/weapons2/benellim9/benellim9_view_trigger.md3" },
	},

	// Magnum
	{
	  { NULL, NULL },
	},

	// Dual Beretta
	{
	  { NULL, NULL },
	},

	// Dual Deagle
	{
	  { NULL, NULL },
	},

	// Dual Glock
	{
		{ "tag_main", "models/weapons2/glock/glock_view_main.md3" },
		{ "tag_clip", "models/weapons2/glock/glock_view_clip.md3" },
		{ "tag_cock", "models/weapons2/glock/glock_view_cock.md3" },
		{ "tag_safety", "models/weapons2/glock/glock_view_safety.md3" },
		{ "tag_slide", "models/weapons2/glock/glock_view_slide.md3" },
		{ "tag_sliderel", "models/weapons2/glock/glock_view_sliderel.md3" },
		{ "tag_trigger", "models/weapons2/glock/glock_view_trigger.md3" },
	},

	// Dual Magnum
	{
	  { NULL, NULL },
	},*/

};

/*
=================
CG_RegisterWeapon

The server says this item is used on this level
=================
*/
void CG_RegisterWeapon( int weaponNum )
{
	weaponInfo_t  *weaponInfo;
	gitem_t       *item, *ammo;
	char	      path[MAX_QPATH];
	vec3_t	      mins, maxs;
	int	      i;

	weaponInfo = &cg_weapons[weaponNum];

	if (weaponNum <= UT_WP_NONE)
	{
		return;
	}

	if (weaponInfo->registered)
	{
		return;
	}

	memset( weaponInfo, 0, sizeof(*weaponInfo));
	weaponInfo->registered = qtrue;

	for (item = bg_itemlist + 1 ; item->classname ; item++)
	{
		if ((item->giType == IT_WEAPON) && (item->giTag == weaponNum))
		{
			weaponInfo->item = item;
			break;
		}
	}

	if (!item->classname)
	{
		CG_Error( "Couldn't find weapon %i", weaponNum );
	}
	CG_RegisterItemVisuals( item - bg_itemlist );

	// load cmodel before model so filecache works
	weaponInfo->weaponModel = trap_R_RegisterModel( item->world_model[0] );

	// Load the view model for urban terror.
	weaponInfo->viewModel  = trap_R_RegisterModel( item->world_model[1] );

	weaponInfo->handsModel = trap_R_RegisterModel ( "models/weapons2/handskins/hands.md3" );

	// Load the animation.cfg for the view model.
	if(weaponInfo->viewModel)
	{
		COM_StripFilename ( item->world_model[1], path );
		strcat ( path, "/animation.cfg" );

		CG_ParseWeaponAnimations ( path, weaponInfo->viewAnims );

		COM_StripFilename ( item->world_model[1], path );
		strcat ( path, "/sound.cfg" );

		CG_ParseWeaponSounds ( path, weaponInfo );

		for (i = 0; cg_viewWeaponTags[weaponNum][i].tag; i++)
		{
			cg_viewWeaponTags[weaponNum][i].hModel = trap_R_RegisterModel( cg_viewWeaponTags[weaponNum][i].model );

			if (cg_viewWeaponTags[weaponNum][i].skin)
			{
				cg_viewWeaponTags[weaponNum][i].hSkin = trap_R_RegisterSkin ( cg_viewWeaponTags[weaponNum][i].skin );
			}
		}
	}

	// calc midpoint for rotation
	trap_R_ModelBounds( weaponInfo->weaponModel, mins, maxs );

	for (i = 0 ; i < 3 ; i++)
	{
		weaponInfo->weaponMidpoint[i] = mins[i] + 0.5 * (maxs[i] - mins[i]);
	}

	weaponInfo->weaponIcon = trap_R_RegisterShader( item->icon );

	if(item->icon2)
	{
		weaponInfo->ammoIcon = trap_R_RegisterShader( item->icon2);
	}

	for (ammo = bg_itemlist + 1 ; ammo->classname ; ammo++)
	{
		if ((ammo->giType == IT_AMMO) && (ammo->giTag == weaponNum))
		{
			break;
		}
	}

	if (ammo->classname && ammo->world_model[0])
	{
		weaponInfo->ammoModel = trap_R_RegisterModel( ammo->world_model[0] );
	}

	strcpy( path, item->world_model[0] );
	COM_StripExtension( path, path );
	strcat( path, "_flash.md3" );
	weaponInfo->flashModel = trap_R_RegisterModel( path );

	if (!weaponInfo->flashModel)
	{
		weaponInfo->flashModel = trap_R_RegisterModel( "models/weapons2/flash/flash.md3" );
	}

	weaponInfo->viewFlashModel = trap_R_RegisterModel( "models/weapons2/machinegun/machinegun_flash.md3" );

	if(item->world_model[0])
	{
		strcpy( path, item->world_model[0] );
		COM_StripExtension( path, path );
		strcat( path, "_barrel.md3" );
		weaponInfo->barrelModel = trap_R_RegisterModel( path );
	}

/*
	strcpy( path, item->world_model[0] );
	COM_StripExtension( path, path );
	strcat( path, "_hand.md3" );
	weaponInfo->handsModel = trap_R_RegisterModel( path );

	if ( !weaponInfo->handsModel ) {
		weaponInfo->handsModel = trap_R_RegisterModel( "models/weapons2/shotgun/shotgun_hand.md3" );
	}
*/

	weaponInfo->loopFireSound = qfalse;

	switch (weaponNum)
	{
		case UT_WP_KNIFE:
			weaponInfo->missileModel = trap_R_RegisterModel( "models/weapons2/knife/knife.md3" );
			weaponInfo->missileSound = trap_S_RegisterSound( "sound/weapons/knife/knifefly.wav", qfalse );
			//weaponInfo->flashSound[0] = trap_S_RegisterSound( "sound/weapons/rocket/rocklf1a.wav", qfalse );
			//cgs.media.rocketExplosionShader = trap_R_RegisterShader( "rocketExplosion" );
			break;

		case UT_WP_HK69:
			weaponInfo->missileModel	 = trap_R_RegisterModel( "models/ammo/grenade1.md3" );
			weaponInfo->missileTrailFunc	 = CG_GrenadeTrail;
			weaponInfo->wiTrailTime 	 = 700;
			weaponInfo->trailRadius 	 = 32;
			MAKERGB( weaponInfo->flashDlightColor, 1, 0.70f, 0 );
			weaponInfo->flashSound[0]	 = trap_S_RegisterSound( "sound/weapons/grenade/grenlf1a.wav", qfalse );
			cgs.media.grenadeExplosionShader = trap_R_RegisterShader( "grenadeBoom" );
			break;

		case UT_WP_GRENADE_HE:
			weaponInfo->missileModel	 = trap_R_RegisterModel( "models/weapons2/grenade/grenade.md3");
			MAKERGB( weaponInfo->flashDlightColor, 1, 0.70f, 0 );
			weaponInfo->flashSound[0]	 = trap_S_RegisterSound( "sound/weapons/grenade/grenlf1a.wav", qfalse );
			cgs.media.grenadeExplosionShader = trap_R_RegisterShader( "grenadeExplosion" );
			break;

		case UT_WP_GRENADE_FLASH:
			weaponInfo->missileModel       = trap_R_RegisterModel( "models/weapons2/grenade/grenade.md3" );
			MAKERGB( weaponInfo->flashDlightColor, 1, 0.70f, 0 );
			weaponInfo->flashSound[0]      = trap_S_RegisterSound( "sound/weapons/grenade/grenlf1a.wav", qfalse );
			cgs.media.flashExplosionShader = trap_R_RegisterShader( "flashExplosion" );
			break;
		
		case UT_WP_GRENADE_SMOKE:
			weaponInfo->missileModel	 = trap_R_RegisterModel( "models/weapons2/grenade/grenade.md3");
			MAKERGB( weaponInfo->flashDlightColor, 1, 0.70f, 0 );
			weaponInfo->flashSound[0]	 = trap_S_RegisterSound( "sound/weapons/grenade/grenlf1a.wav", qfalse );
			cgs.media.grenadeExplosionShader = trap_R_RegisterShader( "grenadeExplosion" );
			break;

		//case UT_WP_BENELLI:
		case UT_WP_SPAS12:
			MAKERGB( weaponInfo->flashDlightColor, 1, 0.70f, 0 );
			weaponInfo->ejectBrassFunc = CG_EjectShell;
			break;

		case UT_WP_UMP45:
		case UT_WP_BERETTA:
		case UT_WP_GLOCK:
		case UT_WP_COLT1911:
		case UT_WP_DEAGLE:
		case UT_WP_LR:
		case UT_WP_AK103:
		case UT_WP_M4:
		case UT_WP_MP5K:
		case UT_WP_NEGEV:
		case UT_WP_MAC11:
        //case UT_WP_P90:
        //case UT_WP_MAGNUM:
		//case UT_WP_DUAL_BERETTA:
		//case UT_WP_DUAL_DEAGLE:
		//case UT_WP_DUAL_GLOCK:
		//case UT_WP_DUAL_MAGNUM:
			MAKERGB( weaponInfo->flashDlightColor, 1, 0.70f, 0 );
			weaponInfo->ejectBrassFunc = CG_EjectBullet;
			break;

		case UT_WP_G36:
			MAKERGB( weaponInfo->flashDlightColor, 1, 0.70f, 0 );
			//weaponInfo->scopeShader[0] = trap_R_RegisterShader("gfx/crosshairs/scopes/generic.tga");
			weaponInfo->scopeShader[1] = -1; //trap_R_RegisterShader("gfx/crosshairs/scopes/scope"); // r00t: will be loaded later (cg_view :: CG_DoShaderDelayedLoad)
			weaponInfo->ejectBrassFunc = CG_EjectBullet;
			break;

		case UT_WP_PSG1:
		case UT_WP_SR8:
			MAKERGB( weaponInfo->flashDlightColor, 1, 0.70f, 0 );
			//weaponInfo->scopeShader[0] = trap_R_RegisterShader("gfx/crosshairs/scopes/generic.tga");
			weaponInfo->scopeShader[1] = -1; //trap_R_RegisterShader("gfx/crosshairs/scopes/scope"); // r00t: will be loaded later (cg_view :: CG_DoShaderDelayedLoad)
			weaponInfo->ejectBrassFunc = CG_EjectBullet;
			break;

		case UT_WP_BOMB:
			weaponInfo->missileModel = trap_R_RegisterModel( "models/weapons2/bombbag/bombbag.md3" );

			for (i = 0; i < 8; i++)
			{
				cgs.media.bombExplosionShaders[i] = trap_R_RegisterShader(va("bombExplosion%d", i + 1));
			}

			cgs.media.bombCloudShader = trap_R_RegisterShader("bombCloud");

			for (i = 0; i < 3; i++)
			{
				cgs.media.bombShockwaveShaders[i] = trap_R_RegisterShader(va("bombShockwave%d", i + 1));
			}

			cgs.media.bombBrightShader = trap_R_RegisterShader("bombBright");
			break;

		//weaponInfo->missileSound = trap_S_RegisterSound( "sound/weapons/knife/knifefly.wav", qfalse );
//		break;
		default:
			MAKERGB( weaponInfo->flashDlightColor, 1, 0.70f, 0 );
			weaponInfo->flashSound[0] = trap_S_RegisterSound( "sound/weapons/rocket/rocklf1a.wav", qfalse );
			break;
	}
}

/*
=================
CG_RegisterItemVisuals

The server says this item is used on this level
=================
*/
void CG_RegisterItemVisuals( int itemNum )
{
	itemInfo_t  *itemInfo;
	gitem_t     *item;
	vec3_t	    mins;
	vec3_t	    maxs;
	int	    i;

	itemInfo = &cg_items[itemNum];

	if (itemInfo->registered)
	{
		return;
	}

	item = &bg_itemlist[itemNum];

	//@Barbatos: ugly hack to be sure it won't load frag/flash nades
	if(/*(item->giTag == UT_WP_GRENADE_FLASH) ||*/ item->giTag == UT_WP_GRENADE_FRAG)
	{
		return;
	}

	memset( itemInfo, 0, sizeof(&itemInfo));
	itemInfo->registered = qtrue;

	itemInfo->models[0]  = trap_R_RegisterModel( item->world_model[0] );

	// calc midpoint for rotation
	trap_R_ModelBounds( itemInfo->models[0], mins, maxs );

	for (i = 0 ; i < 3 ; i++)
	{
		itemInfo->midpoint[i]	 = mins[i] + 0.5 * (maxs[i] - mins[i]);
		itemInfo->bottompoint[i] = mins[i];
	}

	itemInfo->icon = trap_R_RegisterShaderNoMip( item->icon );

	if(NULL != item->icon2)
	{
		itemInfo->icon2 = trap_R_RegisterShader ( item->icon2 );
	}

	if (item->giType == IT_WEAPON)
	{
		CG_RegisterWeapon( item->giTag );
	}

/*
	//
	// powerups have an accompanying ring or sphere
	//
	if ( item->giType == IT_POWERUP || item->giType == IT_HEALTH ||
		item->giType == IT_ARMOR || item->giType == IT_HOLDABLE ) {
		if ( item->world_model[1] ) {
			itemInfo->models[1] = trap_R_RegisterModel( item->world_model[1] );
		}
	}
*/
}

/*
========================================================================================

VIEW WEAPON

========================================================================================
*/

/*
==============
CG_CalculateWeaponPosition
==============
*/
static void CG_CalculateWeaponPosition( vec3_t origin, vec3_t angles )
{
	float	scale;
	int	delta;
	float	fracsin;
	vec3_t	forward;
	vec3_t	right;
	vec3_t	up;
	vec3_t	vangles;
	int	speed = sqrt( cg.predictedPlayerState.velocity[0] * cg.predictedPlayerState.velocity[0] +
			      cg.predictedPlayerState.velocity[1] * cg.predictedPlayerState.velocity[1] +
			      cg.predictedPlayerState.velocity[2] * cg.predictedPlayerState.velocity[2]);

	VectorCopy( cg.refdef.vieworg, origin );
	VectorCopy( cg.refdefViewAngles, angles );

	// Adjust the velocity so it points forward.
	vectoangles ( cg.predictedPlayerState.velocity, vangles );
	vangles[1] += (360 - cg.predictedPlayerState.viewangles[1]);
	vangles[2] += (360 - cg.predictedPlayerState.viewangles[2]);
	AngleVectors ( vangles, forward, right, up);

	VectorScale ( forward, speed, forward );

	VectorMA( origin, forward[1] * 0.008, cg.refdef.viewaxis[1], origin );

//	if ( forward[2] > 50 )
//		VectorMA( origin, forward[2] * 0.005, cg.refdef.viewaxis[2], origin );

	// on odd legs, invert some angles
	if (cg.bobcycle & 1)
	{
		scale = -cg.xyspeed;
	}
	else
	{
		scale = cg.xyspeed;
	}

	// gun angles from bobbing
//	angles[ROLL] += scale * cg.bobfracsin * 0.005;
//	angles[YAW] += scale * cg.bobfracsin * 0.01;
//	angles[PITCH] += cg.xyspeed * cg.bobfracsin * 0.005;

	origin[2] += cg.xyspeed * cg.bobfracsin * 0.003;

//	if ( cg.xyspeed > 170 )
//		origin[2] -= ((cg.xyspeed) * 0.013);

//	origin[2] -= ((1.0f - sin ( (M_PI / 2.0f) + (M_PI / 2.0f) * ((float)cg.xyspeed/300.0f) )) * 2.0f);

	// drop the weapon when landing
	delta = cg.time - cg.landTime;

	if (delta < LAND_DEFLECT_TIME)
	{
		origin[2] += cg.landChange * 0.25 * delta / LAND_DEFLECT_TIME;
	}
	else if (delta < LAND_DEFLECT_TIME + LAND_RETURN_TIME)
	{
		origin[2] += cg.landChange * 0.25 *
			     (LAND_DEFLECT_TIME + LAND_RETURN_TIME - delta) / LAND_RETURN_TIME;
	}
/*

#if 0
	// drop the weapon when stair climbing
	delta = cg.time - cg.stepTime;
	if ( delta < STEP_TIME/2 ) {
		origin[2] -= cg.stepChange*0.25 * delta / (STEP_TIME/2);
	} else if ( delta < STEP_TIME ) {
		origin[2] -= cg.stepChange*0.25 * (STEP_TIME - delta) / (STEP_TIME/2);
	}
#endif

*/

	// idle drift
	scale	       = 20;
	fracsin        = sin( cg.time * 0.001 );
	angles[ROLL]  += scale * fracsin * 0.01;
	angles[YAW]   += scale * fracsin * 0.01;
	angles[PITCH] += scale * fracsin * 0.01;
}

/*
========================
CG_AddWeaponWithPowerups
========================
*/
void CG_AddWeapon( refEntity_t *gun )
{
	/* add the entity to the scene like quad damage effect */
	trap_R_AddRefEntityToSceneX( gun );
}

/**
 * $(function)
 */
void CG_AddWeaponShiney( refEntity_t *gun )
{
	float  delta[2], timeOffset;

	/* first set the offset to be origin / projected texture size */

	/* position */
	delta[0] = gun->origin[0] / 128.0f;
	delta[1] = gun->origin[1] / 128.0f;

	/* rotation */
//	delta[ 0 ] = (gun->axis[ 0 ][ 0 ] + 1.0f) * 0.5f;
//	delta[ 1 ] = (gun->axis[ 0 ][ 1 ] + 1.0f) * 0.5f;

	/* position and rotation (driving game gloss) */
//	delta[ 0 ] = (gun->axis[ 0 ][ 0 ] + 1.0f) * 0.25f + gun->origin[ 0 ] / 512.0f;
//	delta[ 1 ] = (gun->axis[ 0 ][ 1 ] + 1.0f) * 0.25f + gun->origin[ 1 ] / 512.0f;

	/* then scale by time (this is an awful hack) */
	if(delta[0] != 0.0f)
	{
		timeOffset = delta[0];
		delta[0]   = 1.0f;
		delta[1]  /= timeOffset;
	}
	else if(delta[1] != 0.0f)
	{
		timeOffset = delta[1];
		delta[0]  /= timeOffset;
		delta[1]   = 1.0f;
	}
	else
	{
		timeOffset = 0.0f;
	}

	/* then set the values for tcMod entityTranslate and shader time */
	gun->shaderTexCoord[0] = delta[0];
	gun->shaderTexCoord[1] = delta[1];

	gun->shaderTime        = ((float)cg.time / 1000.0f) + timeOffset;

	/* add the entity to the scene like quad damage effect */
	trap_R_AddRefEntityToSceneX( gun );
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name       : CG_AddMuzzleFlash
// Description: Adds a muzzle flash to the scene
// Author     : Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
/*
void CG_AddMuzzleFlash ( refEntity_t* parent, centity_t* cent, qboolean firstPerson )
{
	refEntity_t flash;
	vec3_t		angles, forward, up;

	// Dont add it if the muzzle flash is turned off.
	if(!cg_sfxMuzzleFlash.integer )
		return;

	// No flash if this weapon doenst support it
	if(bg_weaponlist[ cent->currentState.weapon ].flags & UT_WPFLAG_NOFLASH)
		return;

	// No flash if the client is using a silencer
	if ( cent->currentState.powerups & POWERUP(PW_SILENCER) )
		return;

	//  Let the muzzle flash stick around for 50ms or so
	if ( cgs.clientinfo[ cent->currentState.clientNum ].muzzleFlashTime + 40 < cg.time )
		return;

	// Set up the flash entity
	memset( &flash, 0, sizeof( flash ) );
	VectorCopy( parent->lightingOrigin, parent->lightingOrigin );
	flash.shadowPlane = parent->shadowPlane;
	flash.renderfx = parent->renderfx;

	// IF there is no flash model for this weapon, then skip it.
	flash.hModel = firstPerson?cg_weapons[cent->currentState.weapon].viewFlashModel:cg_weapons[cent->currentState.weapon].flashModel;
	if (!flash.hModel)
		return;

	// If this is the beginning of the flahs, update its angle.
	if ( cgs.clientinfo[ cent->currentState.clientNum ].muzzleFlashTime == cg.time )
	{
		cgs.clientinfo[ cent->currentState.clientNum ].muzzleFlashAngle += ((rand()%30)+10);
		cgs.clientinfo[ cent->currentState.clientNum ].muzzleFlashAngle %= 360;
	}

	// Set up the angles on the muzzle flash.
	angles[YAW] = 0;
	angles[PITCH] = 0;
	angles[ROLL] = cgs.clientinfo[ cent->currentState.clientNum ].muzzleFlashAngle;
	AnglesToAxis( angles, flash.axis );

	// Scale it based on the gun
	VectorScale ( flash.axis[0], bg_weaponlist[cent->currentState.weapon].flashScale, flash.axis[0] );
	VectorScale ( flash.axis[1], bg_weaponlist[cent->currentState.weapon].flashScale, flash.axis[1] );
	VectorScale ( flash.axis[2], bg_weaponlist[cent->currentState.weapon].flashScale, flash.axis[2] );

	// Scale it based on whether they are first person or not
	if ( firstPerson )
	{
		VectorScale ( flash.axis[0], 0.75f, flash.axis[0] );
		VectorScale ( flash.axis[1], 0.75f, flash.axis[1] );
		VectorScale ( flash.axis[2], 0.75f, flash.axis[2] );
	}
	else
	{
		VectorScale ( flash.axis[0], 0.50f, flash.axis[0] );
		VectorScale ( flash.axis[1], 0.50f, flash.axis[1] );
		VectorScale ( flash.axis[2], 0.50f, flash.axis[2] );
	}

	// Position it
	CG_PositionRotatedEntityOnTag( &flash, parent, parent->hModel, "tag_flash");

	// If we're zoomed, center muzzle flash.
	if (firstPerson && cent->currentState.clientNum == cg.predictedPlayerState.clientNum && cg.zoomed > 0)
	{
		AngleVectorsFU(cg.refdefViewAngles, forward, NULL, up);

		flash.origin[0] = cg.refdef.vieworg[0] + (forward[0] * 30.0f) - (up[0] * 10.0f);
		flash.origin[1] = cg.refdef.vieworg[1] + (forward[1] * 30.0f) - (up[1] * 10.0f);
		flash.origin[2] = cg.refdef.vieworg[2] + (forward[2] * 30.0f) - (up[2] * 10.0f);
	}

	// Now add it
	trap_R_AddRefEntityToSceneX( &flash );
	if ( !firstPerson )
	{
		VectorScale ( flash.axis[0], 0.5f, flash.axis[0] );
		VectorScale ( flash.axis[1], 0.5f, flash.axis[1] );
		VectorScale ( flash.axis[2], 0.5f, flash.axis[2] );
		flash.hModel = cg_weapons[cent->currentState.weapon].viewFlashModel;
		trap_R_AddRefEntityToSceneX( &flash );
	}

	// Add some smoke and a light if its the initial flash hit
	if ( cgs.clientinfo[ cent->currentState.clientNum ].muzzleFlashTime == cg.time )
	{
//		CG_Printf ( "Adding light\n" );

		// Add some light
		trap_R_AddLightToScene( flash.origin, 300 + (rand()&31),
								cg_weapons[cent->currentState.weapon].flashDlightColor[0],
								cg_weapons[cent->currentState.weapon].flashDlightColor[1],
								cg_weapons[cent->currentState.weapon].flashDlightColor[2] );

		// Add some smoke
		if ( cgs.glconfig.hardwareType != GLHW_RAGEPRO )
		{
			vec3_t dir;
			vec3_t forward;
			vec3_t up;
			int	   radius;

			VectorCopy ( flash.origin, dir );
			VectorNormalizeNR( dir );
			AngleVectorsFU ( cent->lerpAngles, forward, NULL, up );
			VectorNormalizeNR ( forward );
			VectorMA ( dir, 50 + rand()%50, forward, dir );
			VectorMA ( dir, 10, up, dir );

//			VectorAdd ( dir, cent->currentState.pos.trDelta, dir );

			radius = 10;

			CG_SmokePuff( flash.origin, dir, radius, 1, 1, 1, 0.17f, 700, cg.time, 0, 0, cgs.media.shotgunSmokePuffShader );
		}
	}

}
*/

//////////////////////////////////////////////////////////////////////////////////////////
// Name       : CG_AddMuzzleFlash
// Description: Adds a muzzle flash to the scene
// Author     : Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
void CG_AddMuzzleFlash(refEntity_t *parent, centity_t *cent, qboolean firstPerson)
{
	refEntity_t    flash;
	vec3_t	       angles, forward, up, dir;
	float	       deltaTime, duration;
	float	       radius;

	// No flash if this weapon doenst support it
	if (bg_weaponlist[cent->currentState.weapon].flags & UT_WPFLAG_NOFLASH)
	{
		return;
	}

	// No flash if the client is using a silencer
	if (cent->currentState.powerups & POWERUP(PW_SILENCER))
	{
		return;
	}

	// Set up the flash entity
	memset(&flash, 0, sizeof(flash));

	VectorCopy(parent->lightingOrigin, parent->lightingOrigin);

	flash.shadowPlane = parent->shadowPlane;
	flash.renderfx	  = parent->renderfx;

	flash.hModel	  = cg_weapons[cent->currentState.weapon].flashModel;

	if (!flash.hModel)
	{
		return;
	}

	// If this is the beginning of the flahs, update its angle.
	if (cgs.clientinfo[cent->currentState.clientNum].muzzleFlashTime == cg.time)
	{
		cgs.clientinfo[cent->currentState.clientNum].muzzleFlashAngle += ((rand() % 30) + 10);
		cgs.clientinfo[cent->currentState.clientNum].muzzleFlashAngle %= 360;
	}

	// Set up the angles on the muzzle flash.
	angles[YAW]   = 0;
	angles[PITCH] = 0;
	angles[ROLL]  = cgs.clientinfo[cent->currentState.clientNum].muzzleFlashAngle;

	AnglesToAxis(angles, flash.axis);

	// Scale it based on the gun
	VectorScale(flash.axis[0], bg_weaponlist[cent->currentState.weapon].flashScale, flash.axis[0]);
	VectorScale(flash.axis[1], bg_weaponlist[cent->currentState.weapon].flashScale, flash.axis[1]);
	VectorScale(flash.axis[2], bg_weaponlist[cent->currentState.weapon].flashScale, flash.axis[2]);

	// Scale it based on whether they are first person or not
/*	if (firstPerson)
	{
		VectorScale(flash.axis[0], 0.75f, flash.axis[0]);
		VectorScale(flash.axis[1], 0.75f, flash.axis[1]);
		VectorScale(flash.axis[2], 0.75f, flash.axis[2]);
	}
	else
	{
		VectorScale(flash.axis[0], 0.50f, flash.axis[0]);
		VectorScale(flash.axis[1], 0.50f, flash.axis[1]);
		VectorScale(flash.axis[2], 0.50f, flash.axis[2]);
	}*/

	// Position it on the correct hand
	if ( cgs.clientinfo[cent->currentState.clientNum].muzzleFlashHand ) {
	  CG_PositionRotatedEntityOnTag( &flash, parent, parent->hModel, "tag_flash01");
	} else {
	  CG_PositionRotatedEntityOnTag( &flash, parent, parent->hModel, "tag_flash");
	}

	// If we're zoomed, center muzzle flash.
	if (firstPerson && (cent->currentState.clientNum == cg.predictedPlayerState.clientNum) && (cg.zoomed > 0))
	{
		AngleVectorsFU(cg.refdefViewAngles, forward, NULL, up);

		flash.origin[0] = cg.refdef.vieworg[0] + (forward[0] * 30.0f) - (up[0] * 10.0f);
		flash.origin[1] = cg.refdef.vieworg[1] + (forward[1] * 30.0f) - (up[1] * 10.0f);
		flash.origin[2] = cg.refdef.vieworg[2] + (forward[2] * 30.0f) - (up[2] * 10.0f);
	}

	deltaTime = cg.time - cgs.clientinfo[cent->currentState.clientNum].muzzleFlashTime;
	duration  = 2000;

	if ((deltaTime > 50) && (deltaTime < duration) && (cgs.clientinfo[cent->currentState.clientNum].muzzleFlashTime != 0))
	{
		deltaTime = 1.0f - (deltaTime / duration);

		duration  = (750.0f + (random() * 750.0f)) * (0.625f + (deltaTime * 0.375f));

/*	   le = CG_AllocLocalEntity();
	   le->leType = LE_MOVE_SCALE_FADE;
      le->leFlags = 0;

      if (cent->currentState.weapon == UT_WP_SPAS12)
	 le->radius = 3.0f + ((1.0f - random()) * 5.0f);
      else
	 le->radius = 2.0f + ((1.0f - random()) * 4.0f);

      if (!firstPerson)
	 le->radius *= 0.5f;

//	le->radius2 = 0;

      le->scale[0] = 360;
      le->scale[1] = 10;

	   le->startTime = cg.time;
	   le->endTime = cg.time + duration;
      le->fadeInTime = cg.time + (duration * (0.25f + (random() * 0.25f)));

	   le->color[0] = le->color[1] = le->color[2] = 0.4f + (random() * 0.2f);

      if (cent->currentState.weapon == UT_WP_DEAGLE || cent->currentState.weapon == UT_WP_BERETTA)
	 le->color[3] = (0.1875f + (random() * 0.0625f)) * deltaTime;
      else
	 le->color[3] = (0.21875f + (random() * 0.03125f)) * deltaTime;

	   le->pos.trType = TR_LINEAR;
      le->pos.trTime = cg.time;

      VectorCopy(flash.origin, le->pos.trBase);
      VectorNormalize2NR(flash.axis[0], forward);
      up[0] = 0;
      up[1] = 0;
       up[2] = 1;
      if (cent->currentState.weapon == UT_WP_SPAS12)
	 LerpVector(up, forward, random() * 0.4f, forward);
      else
	 LerpVector(up, forward, random() * 0.1f, forward);

      VectorNormalizeNR(forward);

      RotatePointAroundVector(le->pos.trDelta, up, forward, random() * 360.0f);

      VectorScale(le->pos.trDelta, 7.5f + (random() * 15.0f), le->pos.trDelta);

//	le->seed = rand();

      le->refEntity.customShader = cgs.media.smokePuffShader;//muzzleSmoke;
      */

		if (cg.time > cgs.clientinfo[cent->currentState.clientNum].weaponsmoketime)
		{
			radius	= 1.0f + ((1.0f - random()) * 4.0f);
			radius *= 0.15f;

			VectorNormalize2NR(flash.axis[0], forward);
			up[0] = 0;
			up[1] = 0;
			up[2] = 1;
			LerpVector(up, forward, random() * 0.1f, forward);

			VectorNormalizeNR(forward);

			RotatePointAroundVector(dir, up, forward, random() * 360.0f);

			VectorScale(dir, 7.5f + (random() * 15.0f), dir);

			CG_SmokePuff(flash.origin, dir, 0, 1, 1, 1, 0.10f, duration, cg.time, 0, 0, cgs.media.shotgunSmokePuffShader );

			cgs.clientinfo[cent->currentState.clientNum].weaponsmoketime = cg.time + 20;
		}
	}

	deltaTime = cg.time - cgs.clientinfo[cent->currentState.clientNum].muzzleFlashTime;

	//  Let the muzzle flash stick around for 50ms or so
	if (deltaTime > 90)
	{
		return;
	}

	deltaTime /= 90;

	VectorScale(flash.axis[0], 0.1 + (deltaTime * 1), flash.axis[0]);
	VectorScale(flash.axis[1], 0.5f + (deltaTime * 0.15f), flash.axis[1]);
	VectorScale(flash.axis[2], 0.5f + (deltaTime * 0.15f), flash.axis[2]);

	if (deltaTime <= 0.25f)
	{
		duration = deltaTime / 0.25f;
	}
	else if (deltaTime >= 0.75f)
	{
		duration = 1.0f - ((deltaTime - 0.75f) / 0.25f);
	}
	else
	{
		duration = 1.0f;
	}

	if (duration < 0.25f)
	{
		duration = 0.25f;
	}

	flash.shaderRGBA[0] = flash.shaderRGBA[1] = flash.shaderRGBA[2] = 255 * duration;
	flash.shaderRGBA[3] = 255;

	// Now add it
	trap_R_AddRefEntityToSceneX(&flash);

	if (cgs.clientinfo[cent->currentState.clientNum].muzzleFlashTime == cg.time)
	{
		/*   trap_R_AddLightToSceneET(flash.origin, 100, 300 + (rand() & 31),
									     cg_weapons[cent->currentState.weapon].flashDlightColor[0],
									     cg_weapons[cent->currentState.weapon].flashDlightColor[1],
									     cg_weapons[cent->currentState.weapon].flashDlightColor[2],
				     0, REF_JUNIOR_DLIGHT); */
		trap_R_AddLightToScene( flash.origin, 300 + (rand() & 31),
					cg_weapons[cent->currentState.weapon].flashDlightColor[0],
					cg_weapons[cent->currentState.weapon].flashDlightColor[1],
					cg_weapons[cent->currentState.weapon].flashDlightColor[2] );
	}
}

/**
 * $(function)
 */
void CG_DrawLaserDot(vec3_t end)
{
	refEntity_t  ent;	     //laser dot

	memset (&ent, 0, sizeof(ent));
	ent.customShader = cgs.media.UT_laserShaderRed;

	VectorCopy (end, ent.origin);
	VectorCopy (end, ent.oldorigin);

	ent.reType   = RT_SPRITE;
	ent.radius   = 0.9f * BK_LASER_SCALE;
	ent.rotation = 0;
	trap_R_AddRefEntityToSceneX (&ent);
}

// Adds a particle to the list to be checked against for smoke
void CG_MarkSmokeForLaser(localEntity_t *in)
{
	SmokePos[NextSmokeposIndex] = in;
	NextSmokeposIndex++;

	if (NextSmokeposIndex == MAX_SMOKEPOS)
	{
		NextSmokeposIndex = 0; //wrap if we hit the max
	}
}

// 27 - draws in the laser line/sphere intersections
// Soz about the code..
void CG_DrawSmokeLines(vec3_t start, vec3_t end)
{
	int	j;
	vec3_t	dir;
	vec4_t	red = { 1, 0, 0, 1};
	vec3_t	fronts;
	vec3_t	backs;
	vec3_t	test;
//	vec3_t mins,maxs;

	float  radius, raylength;

	VectorSubtract(end, start, dir);
	VectorNormalizeNR(dir);

	VectorSubtract(end, start, test);
	raylength = VectorLength(test);

	for (j = 0; j < MAX_SMOKEPOS; j++)
	{
		if (SmokePos[j] == NULL)
		{
			return;
		}

		radius = SmokePos[j]->lightColor[1];

		//{make sure the sphere isn't past the end of the laser}
		VectorSubtract(start, SmokePos[j]->angles.trBase, test);

		if (VectorLength(test) - radius > raylength)
		{
			continue;
		}

		if (RaySphere(start, dir, SmokePos[j]->angles.trBase, radius, fronts, backs) == qtrue)
		{
			//Clip the lines

			//Start (check the vec of the gun against vec to the start of the line)
			// this is done so that standing inside a smoke bubble doesn't look odd
			VectorSubtract(fronts, start, test);

			if (DotProduct(dir, test) < 0)
			{
				VectorCopy(start, fronts);
				VectorSubtract(backs, start, test);

				if (DotProduct(dir, test) < 0)
				{
					continue;
				}
			}

			//Check to see if the end of the line is protruding
			VectorSubtract(start, backs, test);

			if (VectorLength(test) > raylength)
			{
				continue;
			}

			red[3] = (SmokePos[j]->lightColor[0] / 255); //alpha
			CG_Line(fronts, backs, 1.0f * BK_LASER_SCALE, cgs.media.laserShader, red, red);

			//	mins[0]=mins[1]=mins[2]=-radius;
			//	maxs[0]=maxs[1]=maxs[2]=radius;

			//	CG_DrawBB(SmokePos[j]->angles.trBase,mins,maxs,3);
		}
	}
}

/**
 * $(function)
 */
void CG_ClearLaserSmoke(void)
{
	int  j;

	NextSmokeposIndex = 0;

	for (j = 0; j < MAX_SMOKEPOS; j++)
	{
		SmokePos[j] = NULL;
	}
}

/**
 * $(function)
 */
void CG_DrawLaser(vec3_t start, vec3_t drawfrom, vec3_t end, qboolean drawdot, centity_t *cent)
{
	int	 contents;
	trace_t  trace;
	vec4_t	 red   = { 1, 0, 0, 1};
	vec4_t	 half  = { 1, 0, 0, 1.0};	    //ok ok so it's not 1/2
	vec4_t	 ende  = { 1, 0, 0, 1.0};	    //ok ok so it's not 1/2

	vec4_t	 clear = { 1, 0, 0, 0.0};
	vec3_t	 dis, endpos;
	float	 a;

	// Pos of laser dot
	CG_Trace( &trace, start, NULL, NULL, end, 0, CONTENTS_SOLID | CONTENTS_BODY );

	CG_DrawSmokeLines(drawfrom, end);

	//Draw the laser dot
	if ((trace.fraction != 1) && !(trace.surfaceFlags & SURF_SKY))
	{
		VectorAdd(trace.endpos, trace.plane.normal, trace.endpos); //offset slightly
		CG_DrawLaserDot(trace.endpos);

		//draw laser trail
		if ((cent->pe.laserpos[0] != 0) && (cent->pe.laserpos[1] != 0) && (cent->pe.laserpos[2] != 0))
		{
			dis[0] = trace.endpos[0] - cent->pe.laserpos[0];
			dis[1] = trace.endpos[1] - cent->pe.laserpos[1];
			dis[2] = trace.endpos[2] - cent->pe.laserpos[2];

			a      = VectorLength(dis);

			if (a == 0)
			{
				a = 1;
			}
			dis[0] /= a;
			dis[1] /= a;
			dis[2] /= a;

			if (a > 15)
			{
				a = 15;
			}

			endpos[0] = trace.endpos[0] - (dis[0] * a);
			endpos[1] = trace.endpos[1] - (dis[1] * a);
			endpos[2] = trace.endpos[2] - (dis[2] * a);

			CG_Line(trace.endpos, endpos, 1 * BK_LASER_SCALE, cgs.media.whiteShader, clear, red);
		}

		//copy laser trail
		VectorCopy(trace.endpos, cent->pe.laserpos);
	}

	// Now do that Uber Cool Fog/Water laser line FX
	contents = trap_CM_PointContents( start, 0 );

	if ((contents & CONTENTS_WATER) || (contents & CONTENTS_FOG))
	{
		//The gun nozel is inside fog or water, tracing out.
		//so to find the segment we draw, we trace back to the gun nozzel looking for fog or water

		VectorCopy(trace.endpos, end);
		trap_CM_BoxTrace( &trace, end, drawfrom, NULL, NULL, 0, CONTENTS_WATER | CONTENTS_FOG  );

		if (trace.allsolid == 1)
		{
			//the entire beam is submerged - draw the original section to the end of the trace
			//end[3]=
			CG_Line(drawfrom, end, 1.0f * BK_LASER_SCALE, cgs.media.laserShader, half, ende);
		}
		else
		if (trace.fraction != 1)
		{
			//the beam start is submerged,
			CG_Line(drawfrom, trace.endpos, 1.0f * BK_LASER_SCALE, cgs.media.laserShader, half, half);
		}
	}
	else
	{
		//The gun nozel is outsize fog or water, tracing in.
		VectorCopy(trace.endpos, end);
		trap_CM_BoxTrace( &trace, drawfrom, end, NULL, NULL, 0, CONTENTS_WATER | CONTENTS_FOG  );

		// we hit a bank of fog or water.  Draw the laser the rest of the way
		if (trace.fraction != 1)
		{
			CG_Line(trace.endpos, end, 1.0f * BK_LASER_SCALE, cgs.media.laserShader, half, half);
		}
	}
}

/*
=============
CG_AddPlayerWeapon

Used for both the view weapon (ps is valid) and the world model other character models (ps is NULL)
The main player will have this called for BOTH cases, so effects like light and
sound should only be done on the world model case.
=============
*/
void CG_AddPlayerWeapon( refEntity_t *parent, playerState_t *ps, centity_t *cent, int team )
{
	refEntity_t   gun;

	weapon_t      weaponNum;
	weaponInfo_t  *weapon;
	centity_t     *nonPredictedCent;
	qboolean      inview = qfalse;
	clientInfo_t  *cinfo;
	int	      i, anim;
	vec3_t	      start, end, rot;

	weaponNum = cent->currentState.weapon;

	CG_RegisterWeapon( weaponNum );
	weapon = &cg_weapons[weaponNum];

	cinfo  = &cgs.clientinfo[cent->currentState.clientNum];

	// add the weapon
	memset( &gun, 0, sizeof(gun));
	VectorCopy( parent->lightingOrigin, gun.lightingOrigin );
	gun.shadowPlane = parent->shadowPlane;
//	parent->renderfx|= RF_LIGHTING_ORIGIN;	//eh, naughty
	//  gun.renderfx=0;
	gun.renderfx = parent->renderfx ;

	if(ps && weapon->viewModel)
	{
		inview	       = qtrue;

		gun.hModel     = weapon->viewModel;
		gun.customSkin = weapon->viewSkin;
		gun.frame      = cent->pe.view.frame;
		gun.oldframe   = cent->pe.view.oldFrame;
		gun.backlerp   = cent->pe.view.backlerp;
		gun.renderfx  |= RF_FIRST_PERSON;
	}
	else
	{
		gun.hModel = weapon->weaponModel;
	}

	//

	//Drop out if we're getting asked to draw outselves from non-inview and non-thirdperson

	//if ((cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR)&&(cent->currentState.clientNum==cg.predictedPlayerState.clientNum)) return;

	if (!gun.hModel)
	{
		return;
	}

	CG_PositionEntityOnTag( &gun, parent, parent->hModel, "tag_weapon");

	if (!((cg.zoomed > 0) && ps))
	{
		CG_AddWeaponShiney( &gun ); //cent->currentState.powerups

		// Add all the tagged models
		if(ps && weapon->viewModel)
		{
			for(i = 0; cg_viewWeaponTags[weaponNum][i].hModel; i++)
			{
				refEntity_t  gun_part;
				memset(&gun_part, 0, sizeof(gun_part));
				gun_part.hModel      = cg_viewWeaponTags[weaponNum][i].hModel;
				gun_part.shadowPlane = parent->shadowPlane;
				gun_part.renderfx    = parent->renderfx;
				gun_part.customSkin  = cg_viewWeaponTags[weaponNum][i].hSkin;

				// Special Negev stuff, don't render the bullets if we don't have enough - DensitY
				if((weaponNum == UT_WP_NEGEV) && ((i - 8) >= 0))
				{
					if(utPSGetWeaponBullets( &cg.snap->ps ) >= (i - 8) ||
						((cg.snap->ps.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_RELOADING) &&
						(cg.snap->ps.weaponTime < bg_weaponlist[weaponNum].reloadEndTime)))
					{
						CG_PositionEntityOnTag( &gun_part, &gun, gun.hModel, cg_viewWeaponTags[weaponNum][i].tag );
					}
				}
				else
				{
					CG_PositionEntityOnTag( &gun_part, &gun, gun.hModel, cg_viewWeaponTags[weaponNum][i].tag );
				}

				CG_AddWeaponShiney( &gun_part); //, cent->currentState.powerups );
			}
		}

		// If the client has a silencer then add it to the weapon
		if(cent->currentState.powerups & POWERUP(PW_SILENCER))
		{
			refEntity_t  silencer;
			memset(&silencer, 0, sizeof(silencer));
			VectorCopy( parent->lightingOrigin, silencer.lightingOrigin );

			//if (inview) silencer.hModel = trap_R_RegisterModel(va("models/weapons2/%s/silencer.md3", bg_weaponlist[weaponNum].modelName));
            //else silencer.hModel = trap_R_RegisterModel(va("models/weapons2/%s/silencer-3rd.md3", bg_weaponlist[weaponNum].modelName));

			// Fenix: if the the model has it's own silencer then use it, otherwise
            // backup using the m4 silencer (it fits the most of the weapon models)
			switch (weaponNum) {

                case UT_WP_GLOCK:
                    silencer.hModel = trap_R_RegisterModel(va("models/weapons2/glock/silencer.md3"));
                    break;

                case UT_WP_COLT1911:
                    silencer.hModel = trap_R_RegisterModel(va("models/weapons2/colt1911/silencer.md3"));
                    break;

                case UT_WP_MAC11:
                    silencer.hModel = trap_R_RegisterModel(va("models/weapons2/mac11/silencer.md3"));
                    break;

                default:
                    silencer.hModel = trap_R_RegisterModel(va("models/weapons2/m4/m4_silencer.MD3"));
                    break;
            }

			silencer.shadowPlane = parent->shadowPlane;
			silencer.renderfx    = parent->renderfx & ~RF_SHADOW_PLANE;

			CG_PositionEntityOnTag( &silencer, &gun, gun.hModel, "tag_flash");


			if(!inview)
			{
				VectorScale( silencer.axis[0], bg_weaponlist[weaponNum].silencerWorldScale, silencer.axis[0] );
				VectorScale( silencer.axis[1], bg_weaponlist[weaponNum].silencerWorldScale, silencer.axis[1] );
				VectorScale( silencer.axis[2], bg_weaponlist[weaponNum].silencerWorldScale, silencer.axis[2] );
			}
			else
			{
				VectorScale( silencer.axis[0], bg_weaponlist[weaponNum].silencerViewScale, silencer.axis[0] );
				VectorScale( silencer.axis[1], bg_weaponlist[weaponNum].silencerViewScale, silencer.axis[1] );
				VectorScale( silencer.axis[2], bg_weaponlist[weaponNum].silencerViewScale, silencer.axis[2] );
			}


			CG_AddWeapon( &silencer);
		}
	}

	if (cent->currentState.powerups & POWERUP(PW_LASER))
	{
		refEntity_t laser;

		memset(&laser,0,sizeof(laser));
		VectorCopy( parent->lightingOrigin, laser.lightingOrigin );

		// Fenix: if the the model has it's own laser then use it, otherwise
        // backup using the m4 silencer (it fits the most of the weapon models)
        switch (weaponNum) {

            case UT_WP_GLOCK:
                laser.hModel = trap_R_RegisterModel(va("models/weapons2/glock/laser.md3"));
                break;

            case UT_WP_COLT1911:
                laser.hModel = trap_R_RegisterModel(va("models/weapons2/colt1911/laser.md3"));
                break;

            case UT_WP_MAC11:
                laser.hModel = trap_R_RegisterModel(va("models/weapons2/mac11/laser.md3"));
                break;

            default:
                laser.hModel = trap_R_RegisterModel(va("models/weapons2/m4/m4_laser.md3"));
                break;
        }

		laser.shadowPlane = parent->shadowPlane;
		laser.renderfx = parent->renderfx & ~RF_SHADOW_PLANE;

		if (weaponNum == UT_WP_NEGEV || weaponNum == UT_WP_AK103)
		{
			CG_PositionEntityOnTag( &laser, &gun, gun.hModel, "tag_flash");
		}
		else
		{
			CG_PositionEntityOnTag( &laser, &gun, gun.hModel, "tag_laser");
		}

		if (!((cent->currentState.clientNum==cg.predictedPlayerState.clientNum)&&(!cg.renderingThirdPerson)&&(!inview)&&(cg.snap->ps.persistant[PERS_TEAM] != TEAM_SPECTATOR)))
		{
			anim = cent->pe.torso.animationNumber & ~ANIM_TOGGLEBIT;
			if(!inview)
			{


				if ((anim != TORSO_WEAPON_LOWER) && (anim != TORSO_WEAPON_RAISE) && (anim != TORSO_RELOAD_PISTOL) && (anim != TORSO_RELOAD_RIFLE))
				{
					// 27's NEW LASER CODE (for external views)
					VectorCopy( laser.origin, start );
					AngleVectorsF( cent->lerpAngles, rot, NULL, NULL );
					VectorMA( start, 131072, rot, end );

					CG_DrawLaser(start, start, end, qtrue, cent);
				}
			}
			else
			      {

					  if ((anim!=TORSO_WEAPON_LOWER)&&(anim!=TORSO_WEAPON_RAISE)&&(anim!=TORSO_RELOAD_PISTOL)&&(anim!=TORSO_RELOAD_RIFLE))
					  {
						   // 27's NEW LASER CODE (for player POV)
						   VectorCopy( cg.refdef.vieworg, start );
						   VectorMA( start, 131072, cg.refdef.viewaxis[0], end );
							CG_DrawLaser(start,laser.origin,end,qtrue,cent);
					  }
			      }
		}

		//@Barbatos
		if(!inview)
		{
			VectorScale( laser.axis[0], bg_weaponlist[weaponNum].silencerWorldScale, laser.axis[0] );
			VectorScale( laser.axis[1], bg_weaponlist[weaponNum].silencerWorldScale, laser.axis[1] );
			VectorScale( laser.axis[2], bg_weaponlist[weaponNum].silencerWorldScale, laser.axis[2] );
		}
		else
		{
			VectorScale( laser.axis[0], bg_weaponlist[weaponNum].silencerViewScale, laser.axis[0] );
			VectorScale( laser.axis[1], bg_weaponlist[weaponNum].silencerViewScale, laser.axis[1] );
			VectorScale( laser.axis[2], bg_weaponlist[weaponNum].silencerViewScale, laser.axis[2] );
		}

		if (!((weaponNum == UT_WP_NEGEV) || (weaponNum == UT_WP_AK103)))
		{
			CG_AddWeapon(&laser);
		}
	}

	// make sure we aren't looking at cg.predictedPlayerEntity for LG
	nonPredictedCent = CG_ENTITIES(cent->currentState.clientNum);

/* // r00t: How the hell could this EVER happen?!
   // condition is: ((cg_entities + clientNum) - cg_entities) != clientNum
	// if the index of the nonPredictedCent is not the same as the clientNum
	// then this is a fake player (like on teh single player podiums), so
	// go ahead and use the cent
	if((nonPredictedCent - cg_entities) != cent->currentState.clientNum)
	{
		nonPredictedCent = cent;
	}
*/

	// Add in the muzzle flash
	if ((cent->currentState.clientNum != cg.snap->ps.clientNum) || cg_thirdPerson.integer || (inview && !cg_thirdPerson.integer))
	{
		CG_AddMuzzleFlash ( &gun, cent, inview );
	}

	// Attempt to add brass if this is the inital shot
	if (cgs.clientinfo[cent->currentState.clientNum].ejectTime)
	{
		if (cg.time >= cgs.clientinfo[cent->currentState.clientNum].ejectTime)
		{
			if (weapon->ejectBrassFunc && cg_sfxBrassTime.integer)
			{
				if (cent->currentState.clientNum == cg.snap->ps.clientNum)
				{
					if ((cg_thirdPerson.integer && !inview) || (!cg_thirdPerson.integer && inview))
					{
						cgs.clientinfo[cent->currentState.clientNum].ejectTime = 0;
						weapon->ejectBrassFunc( cent, &gun, bg_weaponlist[weaponNum].brassLengthScale, bg_weaponlist[weaponNum].brassDiameterScale );
					}
				}
				else if (cg_sfxBrassTime.integer > 0 && weapon->ejectBrassFunc)
				{
					cgs.clientinfo[cent->currentState.clientNum].ejectTime = 0;
					weapon->ejectBrassFunc( cent, &gun, bg_weaponlist[weaponNum].brassLengthScale, bg_weaponlist[weaponNum].brassDiameterScale );
				}
			}
		}
	}
}

//
// DensitY Comment: Bolt Actions need to be implemented Correctly!
//
qboolean CG_SetNextAnimation ( int anim )
{
	int  newanim = -1;
	int  weaponSlot;
	int  weaponData;

	// Make sure we have the weapon
	if(-1 == (weaponSlot = cg.predictedPlayerState.weaponslot))
	{
		// This is just in case, but its a problem if it happens
		cg.weaponAnimation = anim;
		return qfalse;
	}

	// Get the weapon info.
	weaponData		 = cg.predictedPlayerState.weaponData[weaponSlot];
	cg.weaponAnimationWeapon = UT_WEAPON_GETID(weaponData);

	switch (anim)
	{
		case WEAPON_READY_FIRE:

			if (!(bg_weaponlist[UT_WEAPON_GETID(weaponData)].modes[UT_WEAPON_GETMODE(weaponData)].flags & UT_WPMODEFLAG_HOLDTOREADY))
			{
				newanim = WEAPON_FIRE;
			}
			break;

		case WEAPON_RELOAD_START:

			if(cg.predictedPlayerState.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_RELOADING)
			{
				newanim = WEAPON_RELOAD;
			}
			break;

		//Bolt action sr8
		/*		case WEAPON_RELOAD:
					if ( cg_weapons[utPSGetWeaponID (&cg.snap->ps)].viewAnims[WEAPON_RELOAD_END].numFrames )
						newanim = WEAPON_RELOAD_END;

					if( UT_WEAPON_GETID(weaponData) != UT_WP_SR8 )
			 {
						if ( cg.predictedPlayerState.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_RELOADING )
					return qtrue;
			 }
			 else
				return qtrue;

					break;*/

		case WEAPON_RELOAD:

			if (cg_weapons[utPSGetWeaponID (&cg.snap->ps)].viewAnims[WEAPON_RELOAD_END].numFrames)
			{
				newanim = WEAPON_RELOAD_END;
			}

			if(UT_WEAPON_GETID(weaponData) != UT_WP_SR8)
			{
				if (cg.predictedPlayerState.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_RELOADING)
				{
					return qtrue;
				}
			}

			break;

//
// 2.6a to sr8 firing.. Zooms out after fired - DensitY
//
		case WEAPON_FIRE:

			if((UT_WEAPON_GETID(weaponData) == UT_WP_SR8) && (UT_WEAPON_GETBULLETS(weaponData) > 0))
			{
//	   if( UT_WEAPON_GETID(weaponData) == UT_WP_SR8  )
				cg.zoomedSaved = cg.zoomed;
				cg.zoomed      = -1;
				newanim        = WEAPON_BOLT;
			}

			break;

		case WEAPON_BOLT:
				       //cg.zoomed = cg.zoomedSaved;
			cg.zoomed = 0; // Stay zoomed out
			break;
	}

	if (newanim != -1)
	{
		CG_SetViewAnimation ( newanim );
		return qtrue;
	}

	return qfalse;
}

/**
 * $(function)
 */
void CG_SetViewAnimation ( int anim )
{
	weaponInfo_t  *pWeaponInfo;
	int	      weaponSlot;
	int	      weaponData;
	int	      weaponMode;
	int	      weaponID;

	if (-1 == (weaponSlot = cg.predictedPlayerState.weaponslot))
	{
		// This is just in case, but its a problem if it happens
		cg.weaponAnimation = anim;
		return;
	}

	weaponData		 = cg.predictedPlayerState.weaponData[weaponSlot];
	weaponMode		 = UT_WEAPON_GETMODE(weaponData);
	weaponID		 = UT_WEAPON_GETID(weaponData);

	cg.weaponAnimationWeapon = weaponID;

	// Get the weapon info.
	pWeaponInfo = &cg_weapons[weaponID];

	// Reset the reloading flag, the reload anim will turn it back on
	cg.reloading = qfalse;

	switch(anim)
	{
		case WEAPON_RELOAD:

			if((weaponID == UT_WP_NEGEV) && (UT_WEAPON_GETBULLETS(weaponData) <= 0))   // Small animation fix for the negev - DensitY
			{
				anim = WEAPON_RELOAD_END;
			}
			cg.reloading = qtrue;
			break;

		case WEAPON_RELOAD_START:

			if (!pWeaponInfo->viewAnims[WEAPON_RELOAD_START].numFrames)
			{
				anim = WEAPON_RELOAD;
			}
			cg.reloading = qtrue;
			break;

		case WEAPON_READY_FIRE:

			if(pWeaponInfo->viewAnims[WEAPON_READY_FIRE_ALT].numFrames && (bg_weaponlist[UT_WEAPON_GETID(weaponData)].modes[UT_WEAPON_GETMODE(weaponData)].flags & UT_WPMODEFLAG_ALTANIMS))
			{
				anim = WEAPON_READY_FIRE_ALT;
			}
			else if(pWeaponInfo->viewAnims[WEAPON_READY_FIRE].numFrames && !(bg_weaponlist[UT_WEAPON_GETID(weaponData)].modes[UT_WEAPON_GETMODE(weaponData)].flags & UT_WPMODEFLAG_ALTANIMS))
			{
				anim = WEAPON_READY_FIRE;
			}
			break;

		case WEAPON_IDLE:

			if(pWeaponInfo->viewAnims[WEAPON_READY_FIRE_IDLE_ALT].numFrames && (cg.predictedPlayerState.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_READY_FIRE))
			{
				anim = WEAPON_READY_FIRE_IDLE_ALT;
			}
			else if(pWeaponInfo->viewAnims[WEAPON_READY_FIRE_IDLE].numFrames && (cg.predictedPlayerState.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_READY_FIRE))
			{
				anim = WEAPON_READY_FIRE_IDLE;
			}
			/*else if ( weaponID >= UT_WP_DUAL_BERETTA && weaponID <= UT_WP_DUAL_MAGNUM && UT_WEAPON_GETBULLETS(weaponData) == 1 && pWeaponInfo->viewAnims[WEAPON_LASTROUND].numFrames )
			{
			  anim = WEAPON_LASTROUND;
      }*/
			else if(pWeaponInfo->viewAnims[WEAPON_IDLE_ALT].numFrames && (bg_weaponlist[weaponID].modes[weaponMode].flags & UT_WPMODEFLAG_ALTANIMS))
			{
				anim = WEAPON_IDLE_ALT;
			}
			else if(pWeaponInfo->viewAnims[WEAPON_IDLE_EMPTY].numFrames && UT_WEAPON_GETBULLETS(weaponData) == 0)
			{
				anim = WEAPON_IDLE_EMPTY;
			}
			break;

		case WEAPON_FIRE:

			if (pWeaponInfo->viewAnims[WEAPON_READY_FIRE].numFrames && !(bg_weaponlist[UT_WEAPON_GETID(weaponData)].modes[UT_WEAPON_GETMODE(weaponData)].flags & (UT_WPMODEFLAG_ALTANIMS | UT_WPMODEFLAG_HOLDTOREADY)) && (cg.predictedPlayerEntity.pe.view.animationNumber != WEAPON_READY_FIRE))
			{
				anim = WEAPON_READY_FIRE;
			}
			/*else if ( weaponID >= UT_WP_DUAL_BERETTA && weaponID <= UT_WP_DUAL_MAGNUM )
			{
			  int bullets = UT_WEAPON_GETBULLETS(weaponData);
			  if ( bullets == 0 && pWeaponInfo->viewAnims[WEAPON_FIRE_LAST_LEFT].numFrames )
			  {
			    anim = WEAPON_FIRE_LAST_LEFT;
			  }
			  else if ( bullets == 1 && pWeaponInfo->viewAnims[WEAPON_FIRE_LAST_RIGHT].numFrames )
			  {
			    anim = WEAPON_FIRE_LAST_RIGHT;
			  }
			  else if ( !(bullets & 1) && pWeaponInfo->viewAnims[WEAPON_FIRE_ALT].numFrames )
			  {
			    anim = WEAPON_FIRE_ALT;
			  }
			}*/
			else if(pWeaponInfo->viewAnims[WEAPON_FIRE_LAST].numFrames && UT_WEAPON_GETBULLETS(weaponData) == 0)
			{
				anim = WEAPON_FIRE_LAST;
			}
			else if (pWeaponInfo->viewAnims[WEAPON_FIRE_ALT].numFrames && (bg_weaponlist[UT_WEAPON_GETID(weaponData)].modes[UT_WEAPON_GETMODE(weaponData)].flags & UT_WPMODEFLAG_ALTANIMS))
			{
				anim = WEAPON_FIRE_ALT;
			}

			break;
	}

	cg.weaponAnimation = anim;
}

/**
 * $(function)
 */
void CG_AnimateViewWeapon ( playerState_t *ps  )
{
	int	      weaponID = utPSGetWeaponID ( ps );
	centity_t     *cent    = &cg.predictedPlayerEntity;
	weaponInfo_t  *weapon  = &cg_weapons[weaponID];

	if (utPSGetWeaponBullets ( ps ) && (cent->pe.view.animationNumber == WEAPON_IDLE_EMPTY))
	{
		CG_SetViewAnimation ( WEAPON_IDLE );
	}

	if ((cg.weaponAnimationWeapon != weaponID) || (cg.weaponAnimation != -1))
	{
		cg.weaponAnimationWeapon = weaponID;

		if(cg.weaponAnimation != -1)
		{
			cent->pe.view.animationNumber = cg.weaponAnimation;
		}
		else
		{
			CG_SetViewAnimation ( WEAPON_IDLE );
			cent->pe.view.animationNumber = cg.weaponAnimation;
		}

		C_ClearLerpFrame ( weapon->viewAnims, &cent->pe.view, cent->pe.view.animationNumber, cg.time);

		cg.weaponAnimation = -1;
	}

	C_RunLerpFrame( weapon->viewAnims, &cent->pe.view, cent->pe.view.animationNumber, 1.0, cg.time );

	if(cent->pe.view.end)
	{
		if (!CG_SetNextAnimation ( cent->pe.view.animationNumber ))
		{
			CG_SetViewAnimation ( WEAPON_IDLE );
		}
	}
	else if(cent->pe.view.frame != cent->pe.view.oldFrame)
	{
		static int  soundFrame = -1;
		int	    sound;

		if(soundFrame != cent->pe.view.frame)
		{
			soundFrame = cent->pe.view.frame;

			// Check for weapon ejecting by frame
			if (bg_weaponlist[weaponID].brassFrame && !cg_thirdPerson.integer)
			{
				// If the eject frame is now, then do it
				if ((bg_weaponlist[weaponID].brassFrame > cent->pe.view.oldFrame) &&
				    (bg_weaponlist[weaponID].brassFrame <= cent->pe.view.frame))
				{
					cgs.clientinfo[cent->currentState.clientNum].ejectTime = cg.time;
				}
			}

			for(sound = 0; weapon->viewSounds[sound].sound; sound++)
			{
				if(weapon->viewSounds[sound].frame == soundFrame)
				{
					trap_S_StartSound( NULL, ps->clientNum, CHAN_WEAPON, weapon->viewSounds[sound].sound );
					break;
				}
			}
		}
	}
}

/*
==============
CG_AddViewWeapon

Add the weapon, and flash for the player's view
==============
*/

void CG_AddViewWeapon( playerState_t *ps )
{
	refEntity_t   hand;
	centity_t     *cent;
	float	      fovOffset;
	vec3_t	      angles;
	weaponInfo_t  *weapon;
	float	      hideOffset = 0.0f;
	float	      gun_x	 = 0;
	float	      gun_y	 = 0;
	float	      gun_z	 = 0;
	int	      weaponID	 = utPSGetWeaponID ( ps );

	if (weaponID == 0)
	{
		return;
	}

	if ((ps->stats[STAT_HEALTH] <= 0) || (ps->persistant[PERS_TEAM] == TEAM_SPECTATOR) || (ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_GHOST))
	{
		return;
	}

	if (ps->pm_type == PM_INTERMISSION)
	{
		return;
	}

	CG_AnimateViewWeapon ( ps );

	// no gun if in third person view
	if (cg.renderingThirdPerson)
	{
		cg.zoomed = 0;
		return;
	}

	// don't draw if testing a gun model
	if (cg.testGun)
	{
		return;
	}

	// If we are running the small gun size then modify the gun values
	if (cg_gunSize.integer == 1)
	{
		gun_x = 10;
		gun_y = 0;
		gun_z = -4;
	}
	else
	{
		gun_x = 7;
	}

	if (cg.time - 100 < cgs.clientinfo[ps->clientNum].muzzleFlashTime)
	{
		gun_x -= (2.0f * (float)(cgs.clientinfo[ps->clientNum].muzzleFlashTime - (cg.time - 100)) / 100.0f);
	}

	switch (weaponID)
	{
		case UT_WP_DEAGLE:
		case UT_WP_BERETTA:
		case UT_WP_GLOCK:
		case UT_WP_COLT1911:
		//case UT_WP_MAGNUM:
		//case UT_WP_DUAL_DEAGLE:
		//case UT_WP_DUAL_BERETTA:
		//case UT_WP_DUAL_GLOCK:
		//case UT_WP_DUAL_MAGNUM:
			gun_x += 7;
			gun_z -= 3;
			break;
	}

	if (cg_fov.integer > 90)
	{
		gun_z -= (20 - (110 - cg_fov.integer)) / 5;
	}

	// Play the bandage sound as long as they arent dead
	if (!(CG_ENTITIES(ps->clientNum)->currentState.eFlags & EF_DEAD))
	{
		/*
		if ( (ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_TEAM_BANDAGE) ||
			 (ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_BANDAGING)	  )
		{
			if( cg.bandageTime && cg.time >= cg.bandageTime )
			{
				trap_S_StartSound (NULL, cg.predictedPlayerState.clientNum, CHAN_AUTO, cgs.media.UT_bandageSound );
				cg.bandageTime = cg.time + UT_BANDAGE_TIME;
			}
		}
		else
			cg.bandageTime = 0;
		*/

		// If we're healing someone.
		if (ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_TEAM_BANDAGE)
		{
			if (cg.bandageTime && (cg.time >= cg.bandageTime))
			{
				trap_S_StartSound(NULL, cg.predictedPlayerState.clientNum, CHAN_AUTO, cgs.media.UT_bandageSound);
				cg.bandageTime = cg.time + UT_HEAL_TIME;
			}
		}
		// If we're bandaging.
		else if (ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_BANDAGING)
		{
			if (cg.bandageTime && (cg.time >= cg.bandageTime))
			{
				trap_S_StartSound(NULL, cg.predictedPlayerState.clientNum, CHAN_AUTO, cgs.media.UT_bandageSound);
				cg.bandageTime = cg.time + UT_BANDAGE_TIME;
			}
		}
		else
		{
			cg.bandageTime = 0;
		}
	}
	else
	{
		cg.bandageTime = 0;
	}

	// No weapon when bandaging.
/*f ( (ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_TEAM_BANDAGE) ||
		 (ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_BANDAGING)    ||
	     (ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_CLIMBING)	   ||
	     (ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_DEFUSING)	   ||
	     (ps->pm_flags & PMF_LADDER_UPPER)					   ||
//		 (ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_PLANTING)     ||
		 cg.time < cg.weaponChangeTime	)*/
	if ((cg.predictedPlayerState.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_TEAM_BANDAGE) ||
	    (cg.predictedPlayerState.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_BANDAGING) ||
	    (cg.predictedPlayerState.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_CLIMBING) ||
	    (cg.predictedPlayerState.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_DEFUSING) ||
	    (cg.snap->ps.pm_flags & PMF_LADDER_UPPER) ||
	    //	 (cg.predictedPlayerState.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_PLANTING)	   ||
	    (cg.time < cg.weaponChangeTime))
	{
		cg.zoomed = 0;

		if(cg.weaponHidden)
		{
			return;
		}

		if(!cg.weaponHideShowTime)
		{
			cg.weaponHideShowTime = cg.time;
		}

		if(cg.time - cg.weaponHideShowTime > UT_WEAPONHIDE_TIME)
		{
			cg.weaponHidden       = qtrue;
			cg.weaponHideShowTime = 0;
			return;
		}

		hideOffset  = (float)(cg.time - cg.weaponHideShowTime) / (float)UT_WEAPONHIDE_TIME;
		hideOffset *= 30;
	}
	else if (cg.weaponHidden || cg.weaponChangeTime)
	{
		if (cg.weaponChangeTime)
		{
			cg.weaponHidden       = qtrue;
			cg.weaponChangeTime   = 0;
			cg.weaponHideShowTime = 0;
		}

		if(!cg.weaponHideShowTime)
		{
			cg.weaponHideShowTime = cg.time;
		}

		if(cg.time - cg.weaponHideShowTime > UT_WEAPONHIDE_TIME)
		{
			cg.weaponHidden       = qfalse;
			cg.weaponHideShowTime = 0;
			cg.weaponChangeTime   = 0;
		}
		else
		{
			hideOffset = (float)(cg.time - cg.weaponHideShowTime) / (float)UT_WEAPONHIDE_TIME;
			hideOffset = 30 - (hideOffset * 30);
		}
	}

	fovOffset = 0;

	cent	  = &cg.predictedPlayerEntity;	// &cg_entities[cg.snap->ps.clientNum];
	CG_RegisterWeapon( weaponID );
	weapon	  = &cg_weapons[weaponID];

	memset (&hand, 0, sizeof(hand));

	// set up gun position
	CG_CalculateWeaponPosition( hand.origin, angles );

/*   if (weaponID==UT_WP_HK69)
   {
      aanim=cent->pe.torso.animationNumber & ~ANIM_TOGGLEBIT;
      if ((aanim==TORSO_RELOAD_RIFLE))
      {
	 CG_Printf("asdasd\n");
	 gun_y+=6;
      }

   }*/

	AnglesToAxis( angles, hand.axis );

	// obsolete as guncorrect is locked, vectorscale replaced by new version - CrazyButcher
	//if ( cg_gunCorrectFOV.integer != 0 )
	{
		VectorScale ( hand.axis[0], (float)((float)cg_fov.integer * 0.55f / (float)90.0f), hand.axis[0] );
	}

/*	// fix to make gun scaled down more if FOV is raised, Apox's code scaled too little
	VectorScale ( hand.axis[0], (0.55f + 0.08f * ((90.0f - (float)cg_fov.integer) / 10.0f)) , hand.axis[0] );
	// moves gun backward according to FOV, as view is moved back on raised FOV hand.origin.x is moved back as well
	fovcor_x = (((float)cg_fov.integer - 90.0f)/6.0f); */

	VectorMA( hand.origin, cg_gun_x.value - hideOffset + gun_x, cg.refdef.viewaxis[0], hand.origin );
	VectorMA( hand.origin, cg_gun_y.value + gun_y, cg.refdef.viewaxis[1], hand.origin );
	VectorMA( hand.origin, (cg_gun_z.value + fovOffset - hideOffset) + gun_z, cg.refdef.viewaxis[2], hand.origin );

	// map torso animations to weapon animations
	if (cg_gun_frame.integer)
	{
		// development tool
		hand.frame    = hand.oldframe = cg_gun_frame.integer;
		hand.backlerp = 0;
	}
	else if (cg_drawHands.integer)
	{
		hand.frame    = cent->pe.view.frame;
		hand.oldframe = cent->pe.view.oldFrame;
		hand.backlerp = cent->pe.view.backlerp;
		hand.renderfx = RF_DEPTHHACK | RF_FIRST_PERSON | RF_MINLIGHT;
		hand.hModel   = weapon->handsModel;

		CG_ReloadHandSkins(ps->clientNum);

		// No gun when zoomed.
		CG_AddPlayerWeapon( &hand, ps, &cg.predictedPlayerEntity, ps->persistant[PERS_TEAM] );
	}
}

/*
==============================================================================

WEAPON SELECTION

==============================================================================
*/

/*
===================
CG_DrawWeaponSelect
===================
*/
void CG_DrawWeaponSelect( void )
{
	int    i;
//	int		bits;
	int    count;
	int    x, y, w;
	char   *name;
	float  *color;

	// don't display if dead
	if (cg.predictedPlayerState.stats[STAT_HEALTH] <= 0)
	{
		return;
	}

	color = CG_FadeColor( cg.weaponSelectTime, WEAPON_SELECT_TIME );

	if (!color)
	{
		return;
	}
	trap_R_SetColor( color );

	// showing weapon select clears pickup item display, but not the blend blob
	cg.itemPickupTime = 0;
	cg.itemSelectTime = 0;

/* URBAN TERROR
	// count the number of weapons owned
	bits = cg.snap->ps.stats[ STAT_WEAPONS ];
	count = 0;
	for ( i = 1 ; i < 16 ; i++ ) {
		if ( bits & ( 1 << i ) ) {
			count++;
		}
	}

*/

	// Count the number of weapons held
	count = 0;

	for(i = 0; i < UT_MAX_WEAPONS; i++)
	{
		if (UT_WEAPON_GETID(cg.snap->ps.weaponData[i]) != 0)
		{
			count++ ;
		}
	}

	// No weapons, then tehre is nothing to show
	if (count == 0)
	{
		return;
	}

	x = 320 - count * (80 / 2);
	y = 400;

/*
	{
		vec4_t color = {0.0f / 255.0f, 31.0f / 255.0f ,78.0f / 255.0f, 1.0f };
		vec4_t white = {128.0f / 255.0f, 128.0f / 255.0f ,128.0f / 255.0f, 1.0f };
		int width = count * 80 + 6; // w(count<2?2:count)*80 + 6;
		CG_FillRect ( 320 - (width/2) - 1, y - 22, width + 2, 69, white );
		CG_FillRect ( 320 - (width/2), y - 21, width, 67, color );
	}
*/

	for (i = 0 ; i < UT_MAX_WEAPONS; i++)
	{
		int  weaponData = cg.snap->ps.weaponData[i];
		int  weaponID	= UT_WEAPON_GETID(weaponData);

		if(!weaponID)
		{
			continue;
		}

		CG_RegisterWeapon( weaponID );

		{
			vec4_t	bkcolor  = { 0, 0, 0, 0.7f };
			vec4_t	selcolor = { 93.0f / 255.0f, 106.0f / 255.0f, 123.0f / 255.0f, 0.9f };
			vec4_t	white	 = { 128.0f / 255.0f, 128.0f / 255.0f, 128.0f / 255.0f, 1.0f };

			CG_DrawRect ( x + 3, y - 1, 74, 42, 1, white );

			if (weaponID == cg.weaponSelect)
			{
				CG_FillRect ( x + 4, y, 72, 40, selcolor );

				name = cg_weapons[cg.weaponSelect].item->pickup_name;

				if (name)
				{
					w = CG_Text_Width( name, 0.25f, 0);
					CG_Text_Paint(x + 76 - w, y - 4, 0.25f, color, name, 0, 0, ITEM_TEXTSTYLE_OUTLINED);
				}
			}
			else
			{
				CG_FillRect ( x + 4, y, 72, 40, bkcolor );
			}
		}

		// draw weapon icon
		if ((weaponID == UT_WP_GRENADE_HE) || (weaponID == UT_WP_GRENADE_FLASH) || (weaponID == UT_WP_GRENADE_SMOKE))
		{
			CG_DrawPic( x + 24, y + 4, 32, 32, cg_weapons[weaponID].weaponIcon );
		}
		else
		{
			CG_DrawPic( x + 8, y + 4, 64, 32, cg_weapons[weaponID].weaponIcon );
		}

		x += 80;
	}

	// draw the selected name
	if (cg_weapons[cg.weaponSelect].item)
	{
	}

	trap_R_SetColor( NULL );
}

/*
===============
CG_WeaponSelectable
===============
*/
static qboolean CG_WeaponSelectable( int weaponid )
{
	if (-1 == utPSGetWeaponByID ( &cg.snap->ps, weaponid ))
	{
		return qfalse;
	}

	return qtrue;
}

/**
 * $(function)
 */
void CG_BiggestWeapon_f(void)
{
	int  oldWeaponID;
	int  tmpWeaponID;
	int  newWeaponID;
	int  bigWeaponID    = 0;
	int  mediumWeaponID = 0;
	int  smallWeaponID  = 0;
	int  meleeWeaponID  = 0;
	int  i;

	oldWeaponID = utPSGetWeaponID(&cg.snap->ps);
	newWeaponID = oldWeaponID;

	for (i = 0; i < UT_MAX_WEAPONS; i++)
	{
		tmpWeaponID = UT_WEAPON_GETID(cg.snap->ps.weaponData[i]);

		if (!tmpWeaponID || (tmpWeaponID == oldWeaponID) || (bg_weaponlist[tmpWeaponID].type == UT_WPTYPE_PLANT))
		{
			continue;
		}

		switch (bg_weaponlist[tmpWeaponID].type)
		{
			case UT_WPTYPE_MELEE:
			case UT_WPTYPE_THROW:
				meleeWeaponID = tmpWeaponID;
				break;

			case UT_WPTYPE_SMALL_GUN:
				smallWeaponID = tmpWeaponID;
				break;

      case UT_WPTYPE_SMALL_GUN_DUAL:
			case UT_WPTYPE_MEDIUM_GUN:
				mediumWeaponID = tmpWeaponID;
				break;

			case UT_WPTYPE_LARGE_GUN:
				bigWeaponID = tmpWeaponID;
				break;

			default:
				continue;
		}
	}

	if (bigWeaponID)
	{
		newWeaponID = bigWeaponID;
	}
	else if (mediumWeaponID)
	{
		newWeaponID = mediumWeaponID;
	}
	else if (smallWeaponID)
	{
		newWeaponID = smallWeaponID;
	}
	else if (meleeWeaponID)
	{
		newWeaponID = meleeWeaponID;
	}

	cg.weaponSelect 						  = newWeaponID;
	cg.weaponSelectTime						  = cg.time;
	cg.zoomed							  = 0;
	cgs.clientinfo[cg.predictedPlayerState.clientNum].muzzleFlashTime = 0;
}

/*
===============
CG_NextWeapon_f
===============
*/
void CG_NextWeapon_f( void )
{
	int  i;
	int  original;
	int  weaponSlot;

	if (!cg.snap)
	{
		return;
	}

	if (cg.snap->ps.pm_flags & PMF_FOLLOW)
	{
		return;
	}

	// If we're paused.
	if (cg.pauseState & UT_PAUSE_ON)
	{
		return;
	}

	cg.weaponSelectTime = cg.time;
	original	    = cg.weaponSelect;

	if (-1 == (weaponSlot = utPSGetWeaponByID ( &cg.snap->ps, original )))
	{
		weaponSlot = 0;
	}

	for (i = 0 ; i < UT_MAX_WEAPONS ; i++)
	{
		// Move to the next weapon
		weaponSlot++ ;

		// Make sure it didnt wrap around.
		if (weaponSlot >= UT_MAX_WEAPONS)
		{
			weaponSlot = 0;
		}
		;

		if((UT_WEAPON_GETID(cg.snap->ps.weaponData[weaponSlot]) != 0) && (UT_WEAPON_GETID(cg.snap->ps.weaponData[weaponSlot]) != UT_WP_KNIFE))
		{
			break;
		}
	}

	if (i == UT_MAX_WEAPONS)
	{
		cg.weaponSelect = utPSGetWeaponByID(&cg.snap->ps, UT_WP_KNIFE);

		if (!cg.weaponSelect)
		{
			cg.weaponSelect = original;
		}
	}
	else
	{
		cg.weaponSelect = UT_WEAPON_GETID(cg.snap->ps.weaponData[weaponSlot]);
		cg.zoomed	= 0;
	}

	cgs.clientinfo[cg.predictedPlayerState.clientNum].muzzleFlashTime = 0;
}

/*
===============
CG_PrevWeapon_f
===============
*/
void CG_PrevWeapon_f( void )
{
	int  i;
	int  original;
	int  weaponSlot;

	if (!cg.snap)
	{
		return;
	}

	if (cg.snap->ps.pm_flags & PMF_FOLLOW)
	{
		return;
	}

	// If we're paused.
	if (cg.pauseState & UT_PAUSE_ON)
	{
		return;
	}

	cg.weaponSelectTime = cg.time;
	original	    = cg.weaponSelect;

	if (-1 == (weaponSlot = utPSGetWeaponByID ( &cg.snap->ps, original )))
	{
		weaponSlot = 0;
	}
	;

	for (i = 0 ; i < UT_MAX_WEAPONS ; i++)
	{
		// Move to the previous weapon
		weaponSlot-- ;

		// Make sure it didnt wrap around.
		if (weaponSlot < 0)
		{
			weaponSlot = UT_MAX_WEAPONS - 1;
		}

		if((UT_WEAPON_GETID( cg.snap->ps.weaponData[weaponSlot] ) != 0) && (UT_WEAPON_GETID(cg.snap->ps.weaponData[weaponSlot]) != UT_WP_KNIFE))
		{
			break;
		}
	}

	if (i == UT_MAX_WEAPONS)
	{
		cg.weaponSelect = utPSGetWeaponByID(&cg.snap->ps, UT_WP_KNIFE);

		if (!cg.weaponSelect)
		{
			cg.weaponSelect = original;
		}
	}
	else
	{
		cg.weaponSelect = UT_WEAPON_GETID( cg.snap->ps.weaponData[weaponSlot] );
		cg.zoomed	= 0;
	}

	cgs.clientinfo[cg.predictedPlayerState.clientNum].muzzleFlashTime = 0;
}

/*
===============
CG_Weapon_f
===============
*/
void CG_Weapon_f( void )
{
	int  num;
	int  weaponSlot;

	if (!cg.snap)
	{
		return;
	}

	if (cg.snap->ps.pm_flags & PMF_FOLLOW)
	{
		return;
	}

	if (cg.radiomenu != 0)
	{
		return; 	//radio menu cancels this
	}

	// If we're paused.
	if (cg.pauseState & UT_PAUSE_ON)
	{
		return;
	}

	num = atoi( CG_Argv( 1 ));

	if ((num <= UT_WP_NONE) || (num >= UT_WP_NUM_WEAPONS))
	{
		return;
	}

	if (num == cg.weaponSelect)
	{
		return;
	}

	if (-1 == (weaponSlot = utPSGetWeaponByID ( &cg.snap->ps, num )))
	{
		return;
	}

	cg.weaponSelect 						  = num;
	cg.weaponSelectTime						  = cg.time;
	cg.zoomed							  = 0;
	cgs.clientinfo[cg.predictedPlayerState.clientNum].muzzleFlashTime = 0;
}

/*
===================
CG_OutOfAmmoChange

The current weapon has just run out of ammo
===================
*/
void CG_OutOfAmmoChange( void )
{
	int  i;

	cg.weaponSelectTime = cg.time;

	for (i = 15 ; i > 0 ; i--)
	{
		if (CG_WeaponSelectable( i ))
		{
			cg.weaponSelect = i;
			break;
		}
	}
}

/*
================
CG_PlayerIsInside

GottaBeKD, January 7th, 2001
Does a vertical trace upwards
================
*/
qboolean CG_PlayerIsInside( centity_t *cent )
{
	vec3_t	 up;
	vec3_t	 UTorigin;
	trace_t  trace;

	VectorCopy( cent->lerpOrigin, UTorigin);
	VectorSet( up, UTorigin[0], UTorigin[1], UTorigin[2] + 100000 );
	CG_Trace (&trace, UTorigin, NULL, NULL, up, cent->currentState.number, MASK_SOLID);

	return !(trace.surfaceFlags & SURF_SKY);
}

/*
===================================================================================================

WEAPON EVENTS

===================================================================================================
*/

/*
================
CG_FireWeapon

Caused by an EV_FIRE_WEAPON event
================
*/
void CG_FireWeapon( centity_t *cent )
{
	entityState_t  *ent;
	weaponInfo_t   *weap;
	int	       contents;
	vec3_t	       origin;

	ent = &cent->currentState;

	if (ent->weapon == UT_WP_NONE)
	{
		return;
	}

	weap = &cg_weapons[ent->weapon];

	//Drops a BB of this client for debugging
	if (cgs.antilagvis)
	{
		if (cent->currentState.clientNum == cg.clientNum)
		{
			CG_DebugPlayerPos();
		}
	}

	// mark the entity as muzzle flashing, so when it is added it will
	// append the flash to the weapon model
	cgs.clientinfo[cent->currentState.clientNum].muzzleFlashTime = cg.time;
	cgs.clientinfo[cent->currentState.clientNum].muzzleFlashHand = ent->eventParm & 1;

	if (weap->ejectBrassFunc)   //gtv bug
	{
		if ((cent->currentState.number != cg.predictedPlayerState.clientNum) || !bg_weaponlist[ent->weapon].brassFrame || cg_thirdPerson.integer)
		{
			cgs.clientinfo[cent->currentState.clientNum].ejectTime = cg.time;
		}
	}

	//GottaBeKD, January 7th, 2001
	VectorCopy(ent->pos.trBase, origin);
	origin[2] += (((cent->currentState.time >> 16) & 0xFF) - 128);
	contents   = trap_CM_PointContents( origin, 0 );

	// If this is a grenade then send the fire in the hole msg.
	if((cent->currentState.powerups & POWERUP(PW_SILENCER)) && weap->sndFireSilenced)
	{
		trap_S_StartSound( NULL, ent->number, CHAN_WEAPON, weap->sndFireSilenced);
	}
	else if((contents & CONTENTS_WATER) && weap->sndFireWater)
	{
		trap_S_StartSound( NULL, ent->number, CHAN_WEAPON, weap->sndFireWater );
	}
	else if(CG_PlayerIsInside(cent) && weap->sndFireInside)
	{
		trap_S_StartSound( NULL, ent->number, CHAN_WEAPON, weap->sndFireInside );
	}
	else if(weap->sndFireOutside)
	{
		trap_S_StartSound( NULL, ent->number, CHAN_WEAPON, weap->sndFireOutside );
	}

/* URBAN TERROR - We now use sounds defined in sound.cfg
	if ( c > 0 ) {
		c = rand() % c;
		if ( weap->flashSound[c] )
		{
			trap_S_StartSound( NULL, ent->number, CHAN_WEAPON, weap->flashSound[c] );
		}
	}

	// do brass ejection
	if ( weap->f && cg_brassTime.integer > 0 ) {
		weap->ejectBrassFunc( cent );
	}
*/

	cgs.clientinfo[cent->currentState.clientNum].ejectBrass = qtrue;

	// If this client is us.
	if (ent->clientNum == cg.snap->ps.clientNum)
	{
		// Move to the next weapon if this was the last shot with this one
		if (bg_weaponlist[utPSGetWeaponID(&cg.predictedPlayerState)].flags & UT_WPFLAG_REMOVEONEMPTY)
		{
			if ((utPSGetWeaponID(&cg.predictedPlayerState) == UT_WP_BOMB) || ((utPSGetWeaponBullets (&cg.predictedPlayerState) + utPSGetWeaponClips ( &cg.predictedPlayerState)) == 0))
			{
				//CG_NextWeapon_f ( );
				CG_BiggestWeapon_f(); //Yuck this should be server enforced in case we miss the event
			}
		}
	}
}

/*
=================
CG_MissileHitWall

Caused by an EV_MISSILE_MISS event, or directly by local bullet tracing
=================
*/
void CG_MissileHitWall( int weapon, int clientNum, vec3_t origin, vec3_t dir, impactSound_t soundType )
{
	qhandle_t      mod;
	qhandle_t      mark;
	qhandle_t      shader;
	sfxHandle_t    sfx;
	sfxHandle_t    sfx2 = 0;
	sfxHandle_t    sfx3 = 0;
	float	       radius;
	float	       light;
	vec3_t	       lightColor;
	vec3_t	       temp;
	localEntity_t  *le = NULL;
	qboolean       isSprite;
	int	       duration, j;
	qboolean       particles = qtrue;

	mark	      = 0;
	radius	      = 32;
	sfx	      = 0;
	mod	      = 0;
	shader	      = 0;
	light	      = 0;
	lightColor[0] = 1;
	lightColor[1] = 1;
	lightColor[2] = 0;

	// set defaults
	isSprite = qfalse;
	duration = 500;

	switch (weapon)
	{
		case UT_WP_GRENADE_FLASH:

			mod	  = cgs.media.dishFlashModel;
			shader	  = cgs.media.flashExplosionShader;
			particles = qfalse;

			if (CG_PointContents( origin, 0 ) & CONTENTS_WATER)
			{
				sfx = cgs.media.UT_explodeH2OSound;
			}
			else
			{
				sfx = cgs.media.UT_explodeSound;
			}

			mark	 = cgs.media.burnMarkShader;
			radius	 = 90;
			duration = 50;
			isSprite = qtrue;

			lightColor[0] = 1;
			lightColor[1] = 1;
			lightColor[2] = 1;
			light	      = 1200;

/*			// Handle the imparing flash code..
			{
				trace_t  trace;
				vec3_t	 start;
				vec3_t	 end;

				VectorCopy ( origin, start );
				start[2] += cg.snap->ps.viewheight;

				VectorCopy ( cg.refdef.vieworg, end );
				end[2] += cg.snap->ps.viewheight;

				// See if the player can even see it
				CG_Trace ( &trace, start, NULL, NULL, end, ENTITYNUM_NONE, MASK_SHOT );

				if (!(trace.contents & CONTENTS_SOLID))
				{
					vec3_t	delta;
					float	distance;
					vec3_t	fromangles;
					int	angle;
					int	time;
					int	SVGDistanceEx;

					VectorSubtract( cg.refdef.vieworg, origin, delta );

					SVGDistanceEx = (utPSIsItemOn( &cg.predictedPlayerState, UT_ITEM_NVG ) ? 2 : 1);

					// get the real distance
					distance = VectorLength( delta );

					// Too far i would say
					if(distance > 3000)
					{
						break;
					}

					VectorNormalizeNR ( delta );
					vectoangles ( delta, fromangles );

					angle  = cg.snap->ps.viewangles[1] - fromangles[1];
					angle += 180;

					if(angle < 0)
					{
						angle += 360;
					}

					// add distance if not facing the grenade..
					if(!((angle > 320) || (angle < 40)))
					{
						angle	  = 180 - abs(angle - 180);
						distance += (UT_FLASH_MAX_DISTANCE * angle / 280);
					}

					if(distance > UT_FLASH_MAX_DISTANCE - 50)
					{
						distance = UT_FLASH_MAX_DISTANCE - 50;
					}

					distance = UT_FLASH_MAX_DISTANCE - distance;
					time	 = UT_FLASH_MAX_TIME * (utPSIsItemOn ( &cg.predictedPlayerState, UT_ITEM_NVG ) ? 2 : 1);

					// Don't do it if your dead!
					if(cg.predictedPlayerState.stats[STAT_HEALTH] > 0)
					{
						CG_SetColorOverlay ( 1, 1, 1, 1 * distance / UT_FLASH_MAX_DISTANCE, time * distance / UT_FLASH_MAX_DISTANCE, UT_OCFADE_SIN_OUT, time * distance / UT_FLASH_MAX_DISTANCE );
					}
				}
			}
*/
			break;

		case UT_WP_GRENADE_SMOKE:

			if (CG_PointContents( origin, 0 ) & CONTENTS_WATER)
			{
				sfx = cgs.media.UT_explodeH2OSound;
			}
			else
			{
				sfx = cgs.media.UT_explodeSound;
			}

			break;

		case UT_WP_GRENADE_HE:
		case UT_WP_HK69:
			mod    = cgs.media.dishFlashModel;
			shader = cgs.media.grenadeExplosionShader;

			if (CG_PointContents( origin, 0 ) & CONTENTS_WATER)
			{
				sfx = cgs.media.UT_explodeH2OSound;
			}
			else
			{
				sfx = cgs.media.UT_explodeSound;
			}

			mark	 = cgs.media.burnMarkShader;
			radius	 = 90;
			light	 = 300;
			isSprite = qfalse;
			break;

		// Bomb Explostion effect
		case UT_WP_BOMB:
			shader	 = cgs.media.grenadeExplosionShader; // We need our own
			mark	 = cgs.media.burnMarkShader;

			radius	 = cgs.worldmaxs[2] - origin[2];

			sfx	 	 = cgs.media.bombExplosionSound;
			sfx2	 = cgs.media.blastWind;
			sfx3	 = cgs.media.blastFire;
			mod	 	 = cgs.media.dishFlashModel;
			light	 = 500;
			isSprite = qtrue;

			//	CG_SetColorOverlay(1.0f, 1.0f, 1.0f, 1.0f, 1300, UT_OCFADE_SIN_OUT, 900);

			// Set bomb exploded stuff.
			cg.bombExploded        = qtrue;
			cg.bombExplodeTime     = cg.time;
			VectorCopy(origin, cg.bombExplosionOrigin);
			cg.bombExplosionRadius = radius;

			break;
	}

	if (sfx)
	{
		// If it's the bomb.
		if (weapon == UT_WP_BOMB)
		{
			// Play bomb sound.
			trap_S_StartLocalSound(sfx, CHAN_WEAPON);

			// Play wind sound.
			trap_S_StartLocalSound(sfx2, CHAN_LOCAL_SOUND);

			// Play fire sound.
			trap_S_StartSound(origin, ENTITYNUM_WORLD, CHAN_AUTO, sfx3);
		}
		else
		{
			trap_S_StartSound(origin, ENTITYNUM_WORLD, CHAN_AUTO, sfx);
		}
	}

	// create the explosion
//	if ( mod )
	{
		if (weapon == UT_WP_HK69)
		{
			le = CG_MakeExplosion(origin, dir, radius + (rand() % 15), mod, shader, duration, isSprite, particles);
			//	le = CG_FragExplosion( origin, dir, radius, mod, shader, duration);
			le = NULL;
		}
		else if (weapon == UT_WP_BOMB)
		{
			CG_BombExplosion(origin, radius);
			le = NULL;
		}
		else if (weapon == UT_WP_GRENADE_FLASH)
		{
			le = CG_MakeFlash( origin, duration );
		}
		else
		{
			le = CG_MakeExplosion(origin, dir, radius + (rand() % 15), mod, shader, duration, isSprite, particles);
			le = NULL;
		}

		if (le)
		{
			le->light = light;
			VectorCopy(lightColor, le->lightColor);
		}
	}

	if ((weapon != UT_WP_BOMB) && mark)
	{
		//fixme needs to do a trace and mark the surfaces properly...
		vec3_t	 angl;
		radius = 80;

		for (j = 0; j < 6; j++)
		{
			VectorCopy(origin, temp);
			angl[0] = markt[j][0] * 64;
			angl[1] = markt[j][1] * 64;
			angl[2] = markt[j][2] * 64;

			CG_ImpactMark( mark, origin, angl, random() * 360, 1, 1, 1, 1, 0, radius, qfalse, NULL );
		}
	}
	else if (weapon == UT_WP_BOMB && mark)
	{
		trace_t  tr;
		vec3_t	 end;

		end[0] = origin[0];
		end[1] = origin[1];
		end[2] = origin[2] - 100;

		trap_CM_BoxTrace(&tr, origin, end, NULL, NULL, 0, CONTENTS_SOLID);

		if (tr.fraction < 1)
		{
			CG_ImpactMark(mark, tr.endpos, tr.plane.normal, random() * 360, 1, 1, 1, 1, 0, radius * 0.5f, qfalse, NULL);
		}
	}
}

/*
=================
CG_MissileHitPlayer
=================
*/
void CG_MissileHitPlayer( int weapon, vec3_t origin, vec3_t dir, int entityNum )
{
	CG_Bleed( origin, entityNum );

	// some weapons will make an explosion with the blood, while
	// others will just make the blood
	switch (weapon)
	{
		case UT_WP_HK69:
		case UT_WP_GRENADE_HE:
			CG_MissileHitWall( weapon, 0, origin, dir, IMPACTSOUND_FLESH );
			break;
		default:
			break;
	}
}

/*
============================================================================

SHOTGUN TRACING

============================================================================
*/

/*
================
CG_ShotgunPellet
================
*/
static void CG_ShotgunPellet( vec3_t start, vec3_t end, int skipNum, int severity, int weapon )
{
	trace_t  tr;
	int	 sourceContentType, destContentType;

	CG_Trace( &tr, start, NULL, NULL, end, skipNum, MASK_SHOT );

	sourceContentType = trap_CM_PointContents( start, 0 );
	destContentType   = trap_CM_PointContents( tr.endpos, 0 );

	if (tr.surfaceFlags & SURF_NOIMPACT)
	{
		return;
	}

	if (CG_ENTITIES(tr.entityNum)->currentState.eType == ET_PLAYER)
	{
/* URBAN TERROR - we send the hits from the server
		CG_MissileHitPlayer( WP_SHOTGUN, tr.endpos, tr.plane.normal, tr.entityNum );
*/
	}
	else
	{
		if (tr.surfaceFlags & SURF_NOIMPACT)
		{
			// SURF_NOIMPACT will not make a flame puff or a mark
			return;
		}

		CG_Bullet( tr.endpos, skipNum, tr.plane.normal, qfalse, ENTITYNUM_WORLD, tr.surfaceFlags, severity, weapon );
/*
		if ( tr.surfaceFlags & SURF_METALSTEPS ) {

			CG_MissileHitWall( WP_SHOTGUN, 0, tr.endpos, tr.plane.normal, IMPACTSOUND_METAL );
		} else {
			CG_MissileHitWall( WP_SHOTGUN, 0, tr.endpos, tr.plane.normal, IMPACTSOUND_DEFAULT );
		}
*/
	}
}

/*
================
CG_ShotgunPattern

Perform the same traces the server did to locate the
hit splashes (FIXME: ranom seed isn't synce anymore)
================
*/
static void CG_ShotgunPattern(entityState_t *es)
{
	vec3_t	forward, right, up;
	vec3_t	point, offset, end;
	vec3_t	axis = { 0, 0, 1};
	float	spread, delta;
	int	i;
	int	severity;

	// Calculate the player's orientation vectors.
	VectorNormalize2NR(es->origin2, forward);
	PerpendicularVector(right, forward);
	CrossProduct(forward, right, up);

	// Calculate spread radius.
	spread = tan(DEG2RAD(bg_weaponlist[es->weapon].modes[0].spread)) * 16384;

	// Generate firing pattern.
	for (i = 0; i < UT_SHOTGUN_COUNT; i++)
	{
		// Set point.
		point[0] = 0;
		point[1] = 1;
		point[2] = 0;

		// Calculate point in firing pattern.
		RotatePointAroundVector(offset, axis, point, (360 / UT_SHOTGUN_COUNT) * i);

		// Calculate delta.
		delta = fabs(sin(Q_crandom(&es->eventParm)));

		// Scale delta.
		delta *= 1.0f;

		// Adjust for spread.
		offset[0] *= spread * delta;
		offset[1] *= spread * delta;

		// Calculate end point.
		VectorMA(es->pos.trBase, 16384, forward, end);

		// Adjust end point for firing pattern.
		VectorMA(end, offset[0], right, end);
		VectorMA(end, offset[1], up, end);

		// Simulate pellet.
		severity = 0;

		if (i == 0)
		{
			severity = 25;
		}

		CG_ShotgunPellet(es->pos.trBase, end, es->otherEntityNum, severity, es->weapon);
	}
}

/*
==============
CG_ShotgunFire
==============
*/
void CG_ShotgunFire( entityState_t *es )
{
	vec3_t	v;

	VectorSubtract( es->origin2, es->pos.trBase, v );
	VectorNormalizeNR( v );
	VectorScale( v, 32, v );
	VectorAdd( es->pos.trBase, v, v );

	CG_ShotgunPattern( es );
}

/*
============================================================================

BULLETS

============================================================================
*/

/*
===============
CG_Tracer
===============
*/
#if 1 // TODO: DensitY
void CG_Tracer(vec3_t source, vec3_t dest, qboolean silenced)
{
	polyVert_t  verts[4];
	trace_t     tr;
	vec3_t	    forward, right;
	vec3_t	    start, finish;
	vec3_t	    midpoint;
	vec3_t	    view;
	vec3_t	    line;
	float	    len, begin, end;
	float	    dot1, dot2, dist;
	qboolean    whizer = qfalse;

	// Bullet path vector.
	VectorSubtract(dest, source, forward);
	VectorNormalize(forward,len);

	// Origin and destination too close to each other.
	if (len < 100.0f)
	{
		return;
	}

	// Vector from view origin to bullet origin.
	view[0] = cg.refdef.vieworg[0] - source[0];
	view[1] = cg.refdef.vieworg[1] - source[1];
	view[2] = cg.refdef.vieworg[2] - source[2];

	VectorNormalize(view,dist);

	// If we're too close to bullet origin.
	if (dist < 128.0f)
	{
		return;
	}

	// Base impact sound.
	trap_S_StartSound(dest, ENTITYNUM_WORLD, CHAN_FX, cgs.media.basetracerSound[rand() % 3]);

	// Direction of bullet.
	line[0] = dest[0] - source[0];
	line[1] = dest[1] - source[1];
	line[2] = dest[2] - source[2];

	VectorNormalizeNR(line);

	dot1 = (float)fabs(DotProduct(line, view));

	// Vector from view origin to bullets' path end.
	view[0] = cg.refdef.vieworg[0] - dest[0];
	view[1] = cg.refdef.vieworg[1] - dest[1];
	view[2] = cg.refdef.vieworg[2] - dest[2];

	VectorNormalizeNR(view);

	dot2 = (float)fabs(DotProduct(line, view));

	// If bullets' path was close enough to our view origin.
	if ((silenced ? ((dot1 > 0.98f) && (dot2 > 0.98f)) : ((dot1 > 0.95f) && (dot2 > 0.95f))) && (Distance(cg.refdef.vieworg, source) <= Distance(dest, source)))
	{
		// Do a trace.
		trap_CM_BoxTrace(&tr, source, dest, NULL, NULL, 0, CONTENTS_SOLID);

		// If end point matches given hit point.
		if (tr.fraction == 1.0)
		{
			whizer = qtrue;
		}
	}

	// If it's a whizzzer.
	if (whizer)
	{
		// Approximately next to your head.
		midpoint[0] = source[0] + (line[0] * dist);
		midpoint[1] = source[1] + (line[1] * dist);
		midpoint[2] = source[2] + (line[2] * dist);

		// Whizzz.
		trap_S_StartSound(midpoint, ENTITYNUM_WORLD, CHAN_FX, cgs.media.tracerSound[rand() % 2]);

		// Tracer line.
		VectorCopy(source, start);
		VectorCopy(dest, finish);
	}
	else
	{
		// Tracer line.
		begin = 30.0f + (random() * (len - 30.0f));

		end   = begin + cg_tracerLength.value;

		if (end > len)
		{
			end = len;
		}

		VectorMA(source, begin, forward, start);
		VectorMA(source, end, forward, finish);
	}

	line[0] = DotProduct(forward, cg.refdef.viewaxis[1]);
	line[1] = DotProduct(forward, cg.refdef.viewaxis[2]);

	VectorScale(cg.refdef.viewaxis[1], line[1], right);
	VectorMA(right, -line[0], cg.refdef.viewaxis[2], right);
	VectorNormalizeNR(right);

	VectorMA(finish, cg_tracerWidth.value, right, verts[0].xyz);
	verts[0].st[0]	     = 0;
	verts[0].st[1]	     = 1;
	verts[0].modulate[0] = 255;
	verts[0].modulate[1] = 255;
	verts[0].modulate[2] = 255;
	verts[0].modulate[3] = 255;

	VectorMA(finish, -cg_tracerWidth.value, right, verts[1].xyz);
	verts[1].st[0]	     = 1;
	verts[1].st[1]	     = 0;
	verts[1].modulate[0] = 255;
	verts[1].modulate[1] = 255;
	verts[1].modulate[2] = 255;
	verts[1].modulate[3] = 255;

	VectorMA(start, -cg_tracerWidth.value, right, verts[2].xyz);
	verts[2].st[0]	     = 1;
	verts[2].st[1]	     = 1;
	verts[2].modulate[0] = 255;
	verts[2].modulate[1] = 255;
	verts[2].modulate[2] = 255;
	verts[2].modulate[3] = 255;

	VectorMA(start, cg_tracerWidth.value, right, verts[3].xyz);
	verts[3].st[0]	     = 0;
	verts[3].st[1]	     = 0;
	verts[3].modulate[0] = 255;
	verts[3].modulate[1] = 255;
	verts[3].modulate[2] = 255;
	verts[3].modulate[3] = 255;

	trap_R_AddPolyToScene(cgs.media.tracerShader, 4, verts);
}
#else
/**
 * $(function)
 */
void CG_Tracer( vec3_t source, vec3_t dest, qboolean silenced )
{
	vec3_t	    forward, right;
	polyVert_t  verts[4];
	vec3_t	    line;
	float	    len, begin, end;
	vec3_t	    start, finish;
	vec3_t	    midpoint;

	vec3_t	    view;
	float	    dot1, dot2, dist;

	// tracer
	VectorSubtract( dest, source, forward );
	VectorNormalize( forward,len );

	// start at least a little ways from the muzzle
	if (len < 100)
	{
		return;
	}
	begin = 30 + random() * (len - 30);

	end = begin + cg_tracerLength.value;

	if (end > len)
	{
		end = len;
	}
	VectorMA( source, begin, forward, start );
	VectorMA( source, end, forward, finish );

	line[0] = DotProduct( forward, cg.refdef.viewaxis[1] );
	line[1] = DotProduct( forward, cg.refdef.viewaxis[2] );

	VectorScale( cg.refdef.viewaxis[1], line[1], right );
	VectorMA( right, -line[0], cg.refdef.viewaxis[2], right );
	VectorNormalizeNR( right );

	VectorMA( finish, cg_tracerWidth.value, right, verts[0].xyz );
	verts[0].st[0]	     = 0;
	verts[0].st[1]	     = 1;
	verts[0].modulate[0] = 255;
	verts[0].modulate[1] = 255;
	verts[0].modulate[2] = 255;
	verts[0].modulate[3] = 255;

	VectorMA( finish, -cg_tracerWidth.value, right, verts[1].xyz );
	verts[1].st[0]	     = 1;
	verts[1].st[1]	     = 0;
	verts[1].modulate[0] = 255;
	verts[1].modulate[1] = 255;
	verts[1].modulate[2] = 255;
	verts[1].modulate[3] = 255;

	VectorMA( start, -cg_tracerWidth.value, right, verts[2].xyz );
	verts[2].st[0]	     = 1;
	verts[2].st[1]	     = 1;
	verts[2].modulate[0] = 255;
	verts[2].modulate[1] = 255;
	verts[2].modulate[2] = 255;
	verts[2].modulate[3] = 255;

	VectorMA( start, cg_tracerWidth.value, right, verts[3].xyz );
	verts[3].st[0]	     = 0;
	verts[3].st[1]	     = 0;
	verts[3].modulate[0] = 255;
	verts[3].modulate[1] = 255;
	verts[3].modulate[2] = 255;
	verts[3].modulate[3] = 255;

	trap_R_AddPolyToScene( cgs.media.tracerShader, 4, verts );

	//Clever.
	view[0] = cg.refdef.vieworg[0] - source[0];
	view[1] = cg.refdef.vieworg[1] - source[1];
	view[2] = cg.refdef.vieworg[2] - source[2];

	//Base impact sound
	trap_S_StartSound( dest, ENTITYNUM_WORLD, CHAN_FX, cgs.media.basetracerSound[rand() % 3] );

	//Whiz SFX
	dist = VectorLength(view);

	if (dist < 64)
	{
		return;
	}

	VectorNormalizeNR(view);

	//direction of bullet
	line[0] = source[0] - dest[0];
	line[1] = source[1] - dest[1];
	line[2] = source[2] - dest[2];
	VectorNormalizeNR(line);

	dot1	= fabs(DotProduct( line, view));

	view[0] = cg.refdef.vieworg[0] - dest[0];
	view[1] = cg.refdef.vieworg[1] - dest[1];
	view[2] = cg.refdef.vieworg[2] - dest[2];
	VectorNormalizeNR(view);

	dot2 = fabs( DotProduct( line, view));

	CG_Printf("Dist %f %f\n", dot1, dot2);

	if ((dot1 > 0.9) && (dot2 > 0.9))
	{
		//CG_Printf("Whuz %f %f\n",dot1,dot2);
		//trap_S_StartSound( cg.refdef.vieworg, ENTITYNUM_WORLD, CHAN_FX, cgs.media.tracerSound[rand()%2] );
		// midpoint[0] = ( start[0] + finish[0] ) * 0.5;
		//midpoint[1] = ( start[1] + finish[1] ) * 0.5;
//	   midpoint[2] = ( start[2] + finish[2] ) * 0.5;

		// add the tracer sound

		midpoint[0] = start[0] + line[0] * dist; //approximately next to your head
		midpoint[1] = start[1] + line[1] * dist;
		midpoint[2] = start[2] + line[2] * dist;

		trap_S_StartSound( midpoint, ENTITYNUM_WORLD, CHAN_FX, cgs.media.tracerSound[rand() % 2] );

		//either whiz towards or whiz away
		/*    AngleVectorsF( cg.refdefViewAngles, line, NULL, NULL );
		    VectorNormalizeNR(line);

		    dot1=DotProduct(line,view);
		    CG_Printf("Whuz %f\n",dot1);

		    //Vieworg would be better replaced by nearest point on line.. but we dont have that
		    if (dot1>0)
		    {
		       trap_S_StartSound( cg.refdef.vieworg, ENTITYNUM_WORLD, CHAN_FX, cgs.media.tracerSound[1] );
		    }
		    else
		    {
		       trap_S_StartSound( cg.refdef.vieworg, ENTITYNUM_WORLD, CHAN_FX, cgs.media.tracerSound[0] );
		    }
		 */
	}
}
#endif

/*
======================
CG_CalcMuzzlePoint
======================
*/
qboolean CG_CalcMuzzlePoint( int entityNum, vec3_t muzzle )
{
	vec3_t	   forward;
	centity_t  *cent;
	int	   anim;

	if (entityNum == cg.snap->ps.clientNum)
	{
		VectorCopy( cg.snap->ps.origin, muzzle );
		muzzle[2] += cg.snap->ps.viewheight;
		muzzle[2] -= 5;
		AngleVectorsF( cg.snap->ps.viewangles, forward, NULL, NULL );
		VectorMA( muzzle, 14, forward, muzzle );
		return qtrue;
	}

	cent = CG_ENTITIES(entityNum);

	if (!cent->currentValid)
	{
		return qfalse;
	}

	VectorCopy( cent->currentState.pos.trBase, muzzle );

	AngleVectorsF( cent->currentState.apos.trBase, forward, NULL, NULL );
	anim = cent->currentState.legsAnim & ~ANIM_TOGGLEBIT;

	if ((anim == LEGS_WALKCR) || (anim == LEGS_IDLECR))
	{
		muzzle[2] += CROUCH_VIEWHEIGHT;
	}
	else
	{
		muzzle[2] += DEFAULT_VIEWHEIGHT;
	}

	muzzle[2] -= 5;

	VectorMA( muzzle, 14, forward, muzzle );

	return qtrue;
}

/*
======================
CG_Bullet

Renders bullet effects.
======================
*/
void CG_Bullet( vec3_t end, int sourceEntityNum, vec3_t normal, qboolean flesh, int fleshEntityNum, int surfaces, int severity, int weapon )
{
	trace_t  trace;
	int	 sourceContentType, destContentType;
	vec3_t	 start;
	vec3_t	 realend;
	int	 spacer;
	vec3_t	 vec;

	realend[0] = end[0];
	realend[1] = end[1];
	realend[2] = end[2];

	if (sourceEntityNum < MAX_CLIENTS)
	{
		CG_CalcMuzzlePoint( sourceEntityNum, start );
	}
	else
	{
		VectorCopy(CG_ENTITIES(sourceEntityNum)->lerpOrigin, start);
	}

	spacer = 25;

	if (weapon == UT_WP_SPAS12/* || weapon == UT_WP_BENELLI*/)
	{
		spacer = 256;
	}

	// if the shooter is currently valid, calc a source point and possibly
	// do trail effects
	if ((sourceEntityNum >= 0) && (cg_tracerChance.value > 0))
	{
		//	if ( CG_CalcMuzzlePoint( sourceEntityNum, start ) )
		{
			{
				vec3_t	vec;
				vec3_t	endtest;
				VectorSubtract ( end, start, vec );
				VectorScale ( vec, 0.98f, vec );
				VectorAdd ( start, vec, endtest );

				sourceContentType = trap_CM_PointContents( start, 0 ) & CONTENTS_WATER;
				destContentType   = trap_CM_PointContents( endtest, 0 ) & CONTENTS_WATER;
			}

			// do a complete bubble trail if necessary
			if (((sourceContentType) == (destContentType)) && (sourceContentType & CONTENTS_WATER))
			{
				CG_BubbleTrail( start, end, spacer );
			}
			// bubble trail from water into air
			else if ((sourceContentType & CONTENTS_WATER))
			{
				trap_CM_BoxTrace( &trace, end, start, NULL, NULL, 0, CONTENTS_WATER );
				CG_BubbleTrail( start, trace.endpos, spacer );
			}
			// bubble trail from air into water
			else if ((destContentType & CONTENTS_WATER))
			{
				static int  lastSplash = 0;

				trap_CM_BoxTrace( &trace, start, end, NULL, NULL, 0, CONTENTS_WATER );
				trap_S_StartSound ( trace.endpos, ENTITYNUM_WORLD, CHAN_AUTO, cgs.media.water1);

				if (cg.time - lastSplash > 300)
				{
					localEntity_t  *le;

					lastSplash = cg.time;

					/*memset ( &particle, 0, sizeof(particle) );
					particle.flags			= PART_FLAG_FLOATS;
					particle.lightness		= 1.0f;
					particle.bounce 		= qtrue;
					particle.trail			= qfalse;
					particle.timeToLive		= 1000;
					particle.radius 		= 1.0f;
					particle.deviation		= 0.1f;

					particle.head.shader = cgs.media.snowShader;
					particle.head.radius = 5;
					particle.head.color[0] = 1.0f;
					particle.head.color[1] = 1.0f;
					particle.head.color[2] = 1.0f;
					particle.head.color[3] = 1.0f;

					particle.up = 1000;
					*/

					//CG_LaunchParticles ( trace.endpos, start, trace.plane.normal, (rand()%5) + 5, 1, 400, 750, &particle );
					le = CG_ParticleCreate ( );
					CG_ParticleSetAttributes ( le, PART_ATTR_SPLASH, cgs.media.snowShader, 0, 0 );
					CG_ParticleExplode( trace.endpos, start, trace.plane.normal, (rand() % 5) + 5, 25.0f, le );
				}

				CG_BubbleTrail( trace.endpos, end, spacer );
			}

			// draw a tracer
			if ((sourceEntityNum != cg.predictedPlayerState.clientNum) && (weapon != UT_WP_KNIFE_THROWN) && (weapon != UT_WP_KNIFE) && (weapon != UT_WP_SPAS12) /*&& (weapon != UT_WP_BENELLI)*/)
			{
				if ((crandom() < cg_tracerChance.value) && !(CG_PointContents(end, -1) & CONTENTS_WATER))
				{
					if ((sourceEntityNum >= 0) && (sourceEntityNum < MAX_CLIENTS))
					{
						if ((CG_ENTITIES(sourceEntityNum)->currentState.powerups & POWERUP(PW_SILENCER)))
						{
							CG_Tracer(start, end, qtrue);
						}
						else
						{
							CG_Tracer(start, end, qfalse);
						}
					}
					else
					{
						CG_Tracer(start, end, qfalse);
					}
				}
			}
		}
	}

	VectorNormalizeNR(normal);

	if ((weapon != UT_WP_KNIFE_THROWN) && (weapon != UT_WP_KNIFE))
	{
		surfaceBulletHit (realend, start, normal, surfaces, severity, NULL );
	}

	// Get surface.
//	i = SURFACE_GET_TYPE(es->time2);

	// If valid surface.
//	if (i == SURF_TYPE_TIN || i == SURF_TYPE_ALUMINUM || i == SURF_TYPE_IRON || i == SURF_TYPE_TITANIUM	||
	//i == SURF_TYPE_STEEL || i == SURF_TYPE_BRASS || i == SURF_TYPE_COPPER || i == SURF_TYPE_CEMENT	||
//		i == SURF_TYPE_ROCK || i == SURF_TYPE_ICE || i == SURF_TYPE_LADDER)
	if (severity > 0)
	{
		// Add light to impact area.
		vec[0] = realend[0];
		vec[1] = realend[1];
		vec[2] = realend[2];
		trap_R_AddLightToScene(vec, 75, cg_weapons[weapon].flashDlightColor[0], cg_weapons[weapon].flashDlightColor[1], cg_weapons[weapon].flashDlightColor[2]);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name       : CG_ParseWeaponSounds
// Description: Reads a configuration file to determine the various sounds for a weapon
// Author     : Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
qboolean CG_ParseWeaponSounds(const char *filename, weaponInfo_t *wp )
{
	char		  *text_p;
	int		  len;
	char		  *token;
	int		  skip;
	char		  text[20000];
	fileHandle_t	  f;
	animationSound_t  *pSound = &wp->viewSounds[0];

	// load the file
	len = trap_FS_FOpenFile( filename, &f, FS_READ );

	if (len <= 0)
	{
		return qfalse;
	}

	if (len >= sizeof(text) - 1)
	{
		CG_Printf( "File %s too long\n", filename );
		return qfalse;
	}

	trap_FS_Read( text, len, f );
	text[len] = 0;
	trap_FS_FCloseFile( f );

	// parse the text
	text_p = text;
	skip   = 0;	// quit the compiler warning

	// read information for each frame
	while (1)
	{
		token = COM_Parse( &text_p );

		if (!token || (*token == 0))
		{
			break;
		}

		// Parse a sound that is played on a animation frame of the view model
		if(Q_stricmp ( token, "anim" ) == 0)
		{
			token = COM_Parse( &text_p );

			if (!token || (*token == 0))
			{
				break;
			}

			pSound->frame = atoi(token);

			token	      = COM_Parse( &text_p );

			if (!token || (*token == 0))
			{
				break;
			}

			if(0 != (pSound->sound = trap_S_RegisterSound( token, qfalse )))
			{
				pSound++;
				pSound->sound = 0;
			}

			continue;
		}
		// Parse a weapon fire sound
		else if (Q_stricmp ( token, "fire" ) == 0)
		{
			sfxHandle_t  *pHandle = NULL;

			token = COM_Parse( &text_p );

			if (!token || (*token == 0))
			{
				break;
			}

			if(Q_stricmp ( token, "inside" ) == 0)
			{
				pHandle = &wp->sndFireInside;
			}
			else if(Q_stricmp ( token, "outside" ) == 0)
			{
				pHandle = &wp->sndFireOutside;
			}
			else if(Q_stricmp ( token, "water" ) == 0)
			{
				pHandle = &wp->sndFireWater;
			}
			else if(Q_stricmp ( token, "silenced" ) == 0)
			{
				pHandle = &wp->sndFireSilenced;
			}

			token = COM_Parse( &text_p );

			if (!token || (*token == 0))
			{
				break;
			}

			if(pHandle)
			{
				*pHandle = trap_S_RegisterSound ( token, qfalse );
			}
		}
	}

	return qtrue;
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name       : CG_ParseWeaponAnimations
// Description: Reads a configuration file containing animation counts and rates
//				models/weapons/beretta/animation.cfg, etc
// Author     : Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
qboolean CG_ParseWeaponAnimations ( char *filename, animation_t *animations )
{
	pc_token_t  token;
	int	    anim;
	int	    handle;

	// Open up the source to parse
	if (!(handle = trap_PC_LoadSource(filename)))
	{
		return qfalse;
	}

	// read optional parameters
	while (1)
	{
		// Get the animation #
		if (!trap_PC_ReadToken ( handle, &token ))
		{
			break;
		}

		anim = token.intvalue;

		// Get the first frame
		if (!trap_PC_ReadToken ( handle, &token ))
		{
			break;
		}

		animations[anim].firstFrame = token.intvalue;

		// Get the frame count
		if (!trap_PC_ReadToken ( handle, &token ))
		{
			break;
		}

		animations[anim].numFrames = token.intvalue;

		// Get the frame lerp
		if (!trap_PC_ReadToken ( handle, &token ))
		{
			break;
		}

		if (token.intvalue == 0)
		{
			CG_Error ( va("Invalid weapon configuration file:  %s", filename ));
		}

		animations[anim].frameLerp   = 1000 / token.intvalue;
		animations[anim].initialLerp = 1000 / token.intvalue;

		// Get the loop frames
		if (!trap_PC_ReadToken ( handle, &token ))
		{
			break;
		}

		animations[anim].loopFrames = token.intvalue;
	}

	trap_PC_FreeSource ( handle );

	return qtrue;
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name       : CG_WeaponMode
// Description: Handles the weapon mode switch
// Changlist  : apoxol 04 Jan 2001 created
///////////////////////////////////////////////////////////////////////////////////////////
void CG_WeaponMode ( int oldMode )
{
	int	      newMode;
	weaponInfo_t  *weapon;
	int	      weaponID;

	// predict the new mode.
	weaponID = utPSGetWeaponID ( &cg.predictedPlayerState );
	newMode  = utPSGetWeaponMode ( &cg.predictedPlayerState );
	weapon	 = &cg_weapons[weaponID];

	// If the new mode uses alt anims and the old didnt then set the anim to change
	if((bg_weaponlist[weaponID].modes[newMode].flags & UT_WPMODEFLAG_ALTANIMS) &&
	   !(bg_weaponlist[weaponID].modes[oldMode].flags & UT_WPMODEFLAG_ALTANIMS))
	{
		if(weapon->viewAnims[WEAPON_TOALTERNATE].numFrames)
		{
			cg.weaponAnimation = WEAPON_TOALTERNATE;
		}
	}

	// If the new mode doesnt use alt anims and the old does then switch
	if(!(bg_weaponlist[weaponID].modes[newMode].flags & UT_WPMODEFLAG_ALTANIMS) &&
	   (bg_weaponlist[weaponID].modes[oldMode].flags & UT_WPMODEFLAG_ALTANIMS))
	{
		if(weapon->viewAnims[WEAPON_TONORMAL].numFrames)
		{
			cg.weaponAnimation = WEAPON_TONORMAL;
		}
	}

	// Set the weapon mode
	cg.weaponModes[weaponID] = '0' + newMode;
	// Local Saving
	trap_Cvar_Set ( "weapmodes_save", cg.weaponModes );
}
