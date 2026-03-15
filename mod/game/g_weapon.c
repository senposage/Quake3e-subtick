/**
 * Filename: g_weapon.c
 *
 * Copyright (C) 1999-2005 Id Software, Inc.
 * Copyright (C) 2000-2009 FrozenSand
 *
 * This file is part of Urban Terror source code.
 *
 * Perform the server side effects of a weapon firing
 */

#include "g_local.h"
#include "g_aries.h"

//static float   s_quadFactor;
static vec3_t  forward, right, up;
static vec3_t  muzzle;

#define NUM_NAILSHOTS  10
#define BUTTON_ANYZOOM (BUTTON_ZOOM1 | BUTTON_ZOOM2)

void utBulletFireFromDisposition(gentity_t *ent);
void utBulletFirePattern(gentity_t *ent, vec3_t origin, vec3_t origin2, int seed);

/**
 * G_BounceProjectile
 */
void G_BounceProjectile(vec3_t start, vec3_t impact, vec3_t dir, vec3_t endout)
{
	vec3_t	v, newv;
	float	dot;

	VectorSubtract(impact, start, v);
	dot = DotProduct(v, dir);
	VectorMA(v, -2 * dot, dir, newv);

	VectorNormalizeNR(newv);
	VectorMA(impact, 8192, newv, endout);
}

/**
 * SnapVectorTowards
 *
 * Round a vector to integers for more efficient network
 * transmission, but make sure that it rounds towards a given point
 * rather than blindly truncating. This prevents it from truncating
 * into a wall
 */
#if 0
//inlined
void SnapVectorTowards(vec3_t v, vec3_t to)
{
	int  i;

	for (i = 0 ; i < 3 ; i++) {
		if (to[i] <= v[i]) {
			v[i] = (int)v[i];
		} else {
			v[i] = (int)v[i] + 1;
		}
	}
}
#endif
/**
 * utWeaponFireShotgun
 *
 * Fires any shotgun
 */
void utWeaponFireShotgun(gentity_t *ent)
{
	gentity_t  *tent;

	/* Presend the shotgun blast for the misses. */
	tent = G_TempEntity(muzzle, EV_SHOTGUN);
	VectorScale(forward, 4096, tent->s.origin2);
	SnapVector(tent->s.origin2);
	tent->s.eventParm	   = rand() & 255;
	tent->s.otherEntityNum = ent->s.number;
	tent->s.weapon		   = ent->s.weapon;
	/*tent->r.svFlags |= SVF_BROADCAST; */

	/* Shoot */
	utBulletFirePattern(ent, tent->s.pos.trBase, tent->s.origin2,
				tent->s.eventParm);
}

/**
 * utWeaponFireHK69
 *
 * Fire the HK69
 */
void utWeaponFireHK69(gentity_t *ent)
{
	gentity_t  *grenade;
	gentity_t  *train;
	int 	   weaponSlot = utPSGetWeaponByID(&ent->client->ps, UT_WP_HK69);
	int 	   weaponMode = UT_WEAPON_GETMODE(ent->client->ps.weaponData[weaponSlot]);

	/* extra vertical velocity */
	forward[2] += 0.2f;

	VectorNormalizeNR(forward);

	grenade 						= G_Spawn();
	grenade->classname				= "grenade";
	grenade->classhash				= HSH_grenade;
	grenade->nextthink				= level.time + 2500;
	grenade->think					= G_ExplodeMissile;
	grenade->s.eType				= ET_MISSILE;
	grenade->r.svFlags				= SVF_USE_CURRENT_ORIGIN;
	grenade->s.weapon				= UT_WP_HK69;
	grenade->s.eFlags				= EF_BOUNCE_HALF;
	grenade->r.ownerNum 			= ent->s.number;
	grenade->parent 				= ent;
	grenade->damage 				= (int)((float)UT_MAX_HEALTH / 2 * 1.8f);
	grenade->splashDamage			= (int)((float)UT_MAX_HEALTH * 1.8f);
	grenade->splashRadius			= 300;
	grenade->methodOfDeath			= bg_weaponlist[UT_WP_HK69].mod;
	grenade->splashMethodOfDeath	= bg_weaponlist[UT_WP_HK69].mod;
	grenade->clipmask				= MASK_SHOT;
	grenade->target_ent 			= NULL;
	grenade->s.pos.trType			= TR_GRAVITY;
	grenade->r.svFlags			   |= SVF_BROADCAST;

	/* move a bit on the very first frame */
	grenade->s.pos.trTime = level.time - 50;

	VectorCopy(muzzle, grenade->s.pos.trBase);
	VectorScale(forward,
			bg_weaponlist[UT_WP_HK69].modes[weaponMode].recoil.spread,
			grenade->s.pos.trDelta);

	/* save net bandwidth */
	SnapVector(grenade->s.pos.trDelta);

	VectorCopy(muzzle, grenade->r.currentOrigin);

	/* "real" physics */
	VectorAdd(grenade->s.pos.trDelta, ent->client->ps.velocity,
		  grenade->s.pos.trDelta);

	train = NULL;

	if ((train = G_FindClassHash(train, HSH_func_ut_train)) != NULL) {
		if (train->s.number == ent->client->ps.groundEntityNum) {
			VectorAdd(grenade->s.pos.trDelta, train->s.pos.trDelta,
				  grenade->s.pos.trDelta);
		}
	}
}

/**
 * utWeaponThrowGrenade
 *
 * Throws the grenade (flash, HE and frag)
 */
void utWeaponThrowGrenade(gentity_t *ent)
{
	gentity_t  *grenade;
	gentity_t  *train;
	int 	   weaponMode = utPSGetWeaponMode(&ent->client->ps);
	int 	   speed	  = bg_weaponlist[ent->s.weapon].modes[weaponMode].recoil.spread;
	int 	   timer;
	qboolean   sploded	  = qfalse;

	timer = level.time - ent->client->botNotMovedCount;

	/*
	 * botNotMovedCount used to record how long player has held a grenade
	 * for after pin was released. Set in g_active.c
	 */
	if ((3500 <= timer) && (ent->client->botNotMovedCount != 0)) {
		sploded = qtrue;
	}

	/* They are dead or sploded, so dont give it forward velocity. */
	if (sploded || (ent->client->ps.pm_type == PM_DEAD)) {
		speed = 0;
	}

	/* extra vertical velocity */
	if (!sploded) {
		forward[2] += 0.2f;
		VectorNormalizeNR(forward);
	}

	grenade 		   = G_Spawn();
	grenade->classname = "grenade";
	grenade->classhash = HSH_grenade;

	/*
	 * botNotMovedCount is used ot record how long player has held
	 * a grenade for afer the pin has been released with weapmode
	 */
	if (!(ent->r.svFlags & SVF_BOT) &&
		(ent->client->botNotMovedCount != 0)) {
		grenade->nextthink = (level.time + 3500) - timer;
	} else {
		grenade->nextthink = (level.time + 3500); /* was 2500 */
	}
	ent->client->botNotMovedCount = 0;	/* clear grenade fuse timer */

	grenade->think			  = G_ExplodeMissile;
	grenade->s.eType		  = ET_MISSILE;
	grenade->r.svFlags		  = SVF_USE_CURRENT_ORIGIN;
	grenade->s.weapon		  = ent->s.weapon;
	grenade->s.eFlags		  = EF_BOUNCE_HALF;
	grenade->r.ownerNum 	  = ent->s.number;
	grenade->parent 		  = ent;
	grenade->damage 		  = (int)((float)UT_MAX_HEALTH / 2 *
						  bg_weaponlist[ent->s.weapon].damage[HL_UNKNOWN].value);
	grenade->splashDamage		  = (int)((float)UT_MAX_HEALTH * 2 *
						  bg_weaponlist[ent->s.weapon].damage[HL_UNKNOWN].value);

	grenade->splashRadius = 235;

	if (sploded & (bg_weaponlist[ent->s.weapon].mod == UT_MOD_HEGRENADE)) {
		grenade->methodOfDeath = UT_MOD_SPLODED;
	} else {
		grenade->methodOfDeath = bg_weaponlist[ent->s.weapon].mod;
	}
	grenade->splashMethodOfDeath = bg_weaponlist[ent->s.weapon].mod;
	grenade->clipmask		 = MASK_SHOT;
	grenade->target_ent 	 = NULL;
	grenade->s.pos.trType		 = TR_GRAVITY;
	grenade->r.svFlags		|= SVF_BROADCAST;

	/* move a bit on the very first frame */
	grenade->s.pos.trTime = level.time - 50;

	VectorCopy(muzzle, grenade->s.pos.trBase);

	/* If current weapon doesn't match requested weapon. */
	if (utPSGetWeaponID(&ent->client->ps) != ent->client->pers.cmd.weapon) {
		/* Simulate grenade drop. */
		VectorScale(forward, 600, grenade->s.pos.trDelta); //100
		//@Barbatos: FIXME! Is the problem with the nades sometimes thrown at player's foots related to this?
	} else {
		/* Throw it. */
		VectorScale(forward, speed, grenade->s.pos.trDelta);
	}

	/* save net bandwidth */
	SnapVector(grenade->s.pos.trDelta);

	VectorCopy(muzzle, grenade->r.currentOrigin);

	/* "real" physics */
	VectorAdd(grenade->s.pos.trDelta, ent->client->ps.velocity, grenade->s.pos.trDelta);

	train = NULL;

	if ((train = G_FindClassHash(train, HSH_func_ut_train)) != NULL) {
		if (train->s.number == ent->client->ps.groundEntityNum) {
			VectorAdd(grenade->s.pos.trDelta, train->s.pos.trDelta, grenade->s.pos.trDelta);
		}
	}

	/* See if we should remove this weapon */
	if (bg_weaponlist[ent->s.weapon].flags & UT_WPFLAG_REMOVEONEMPTY) {
		if (utPSGetWeaponBullets(&ent->client->ps) + utPSGetWeaponClips(&ent->client->ps) == 0) {
			/*NextWeapon(&ent->client->ps); */
			utPSSetWeaponID(&ent->client->ps, 0);
		}
	}
}

/**
 * utWeaponThrowSmokeGrenade
 *
 * Throws the smoke grenade
 */
void utWeaponThrowSmokeGrenade(gentity_t *ent)
{
	gentity_t  *grenade;
	gentity_t  *train;
	int 	   weaponMode = utPSGetWeaponMode(&ent->client->ps);
	int 	   speed	  = bg_weaponlist[ent->s.weapon].modes[weaponMode].recoil.spread;

	/* They are dead, so dont give it forward velocity. */
	if (ent->client->ps.pm_type == PM_DEAD) {
		speed = 0;
	}

	/* extra vertical velocity */
	forward[2] += 0.2f;
	VectorNormalizeNR(forward);

	/* Starts a little early so that it smokes while traveling :) */
	grenade 			= G_Spawn();
	grenade->classname	= "grenade";
	grenade->classhash	= HSH_grenade;
	grenade->nextthink	= level.time + 15000;
	grenade->think		= G_GrenadeShutdown;
	grenade->s.powerups = ent->client->sess.sessionTeam; /*Color */

	if (g_survivor.integer) {
		grenade->s.powerups = 0;  /* No color for TS */
	}
	grenade->s.eType			 = ET_MISSILE;
	grenade->r.svFlags			 = SVF_USE_CURRENT_ORIGIN;
	grenade->s.weapon			 = ent->s.weapon;
	grenade->s.eFlags			 = EF_BOUNCE_HALF;
	grenade->r.ownerNum 		 = ent->s.number;
	grenade->parent 			 = ent;
	grenade->damage 			 = (int)((float)UT_MAX_HEALTH / 2 *
						 bg_weaponlist[ent->s.weapon].damage[HL_UNKNOWN].value);
	grenade->splashDamage		 = (int)((float)UT_MAX_HEALTH * 2 *
						 bg_weaponlist[ent->s.weapon].damage[HL_UNKNOWN].value);
	grenade->splashRadius		 = 250;
	grenade->methodOfDeath		 = bg_weaponlist[ent->s.weapon].mod;
	grenade->splashMethodOfDeath = bg_weaponlist[ent->s.weapon].mod;
	grenade->clipmask			 = MASK_SHOT;
	grenade->target_ent 		 = NULL;
	grenade->s.pos.trType		 = TR_GRAVITY;
	grenade->r.mins[0]			 = 0;
	grenade->r.mins[1]			 = 0;
	grenade->r.mins[2]			 = 0;
	grenade->r.maxs[0]			 = 0;
	grenade->r.maxs[1]			 = 0;
	grenade->r.maxs[2]			 = 0;
	grenade->r.svFlags			|= SVF_BROADCAST;

	/* move a bit on the very first frame */
	grenade->s.pos.trTime = level.time - 50;

	VectorCopy(muzzle, grenade->s.pos.trBase);
	VectorScale(forward, speed, grenade->s.pos.trDelta);

	/* save net bandwidth */
	SnapVector(grenade->s.pos.trDelta);

	VectorCopy(muzzle, grenade->r.currentOrigin);

	/* "real" physics */
	VectorAdd(grenade->s.pos.trDelta, ent->client->ps.velocity, grenade->s.pos.trDelta);

	train = NULL;

	if ((train = G_FindClassHash(train, HSH_func_ut_train)) != NULL) {
		if (train->s.number == ent->client->ps.groundEntityNum) {
			VectorAdd(grenade->s.pos.trDelta, train->s.pos.trDelta, grenade->s.pos.trDelta);
		}
	}

	/* See if we should remove this weapon */
	if (bg_weaponlist[ent->s.weapon].flags & UT_WPFLAG_REMOVEONEMPTY) {
		if (utPSGetWeaponBullets(&ent->client->ps) + utPSGetWeaponClips(&ent->client->ps) == 0) {
			/*NextWeapon(&ent->client->ps); */
			utPSSetWeaponID(&ent->client->ps, 0);
		}
	}
}

/**
 * utWeaponThrowKnife
 */
void utWeaponThrowKnife(gentity_t *self, vec3_t start, vec3_t dir)
{
	gentity_t  *knife;
	int    weaponSlot =
		utPSGetWeaponByID(&self->client->ps, self->s.weapon);
	int    weaponMode =
		UT_WEAPON_GETMODE(self->client->ps.weaponData[weaponSlot]);
	int    speed	  =
		bg_weaponlist[self->s.weapon].modes[weaponMode].spread;

	/* They are dead, so dont give it forward velocity. */
	if (self->client->ps.pm_type == PM_DEAD) {
		speed = 0;
	}

	dir[2] += 0.05f;
	VectorNormalizeNR(dir);

	knife				 = G_Spawn();
	knife->classname	 = "knife_thrown";
	knife->classhash	 = HSH_knife_thrown;
	knife->nextthink	 = level.time + 5000;
	/* if it hasn't impacted after 5 seconds, kill it */
	knife->think		 = G_FreeEntity;
	knife->s.pos.trTime  = level.time;
	knife->s.eType		 = ET_MISSILE;
	/*	knife->r.svFlags = SVF_USE_CURRENT_ORIGIN; */
	knife->s.weapon 	 = UT_WP_KNIFE;
	knife->r.ownerNum	 = self->s.number;
	knife->s.time2		 = -1;
	knife->parent		 = self;
	knife->damage		 = 20;
	knife->splashDamage  = 0;
	knife->splashRadius  = 0;
	knife->methodOfDeath = UT_MOD_KNIFE_THROWN;
	knife->clipmask 	 = MASK_SHOT;
	knife->target_ent	 = NULL;
	knife->r.mins[0]	 = -5;
	knife->r.mins[1]	 = -5;
	knife->r.mins[2]	 = -5;
	knife->r.maxs[0]	 = 5;
	knife->r.maxs[1]	 = 5;
	knife->r.maxs[2]	 = 5;

	knife->s.pos.trType  = TR_GRAVITY;
	/* move a bit on the very first frame */
	knife->s.pos.trTime  = level.time;
	VectorCopy(start, knife->s.pos.trBase);
	VectorScale(dir, speed, knife->s.pos.trDelta);
	/* save net bandwidth */
	SnapVector(knife->s.pos.trDelta);

	VectorCopy(start, knife->r.currentOrigin);

	/* "real" physics */
	VectorAdd(knife->s.pos.trDelta, self->client->ps.velocity,
		  knife->s.pos.trDelta);

	/* See if we should remove this weapon */
	if (bg_weaponlist[self->s.weapon].flags & UT_WPFLAG_REMOVEONEMPTY) {
		if (utPSGetWeaponBullets(&self->client->ps) +
			utPSGetWeaponClips(&self->client->ps) == 0) {
			utPSSetWeaponID(&self->client->ps, 0);
		}
	}
}

/**
 * LogAccuracyHit
 */
qboolean LogAccuracyHit(gentity_t *target, gentity_t *attacker)
{
	if (!target->takedamage) {
		return qfalse;
	}

	if (target == attacker) {
		return qfalse;
	}

	if (!target->client) {
		return qfalse;
	}

	if (!attacker->client) {
		return qfalse;
	}

	if (target->client->ps.stats[STAT_HEALTH] <= 0) {
		return qfalse;
	}

	if (OnSameTeam(target, attacker)) {
		return qfalse;
	}

	return qtrue;
}

/**
 * CalcMuzzlePoint
 *
 * Set muzzle location relative to pivoting eye
 */
void CalcMuzzlePoint(gentity_t *ent, vec3_t forward, vec3_t right, vec3_t up, vec3_t muzzlePoint)
{
	VectorCopy(ent->s.pos.trBase, muzzlePoint);
	muzzlePoint[2] += ent->client->ps.viewheight;
	VectorMA(muzzlePoint, 14, forward, muzzlePoint);
	/*
	 * Snap to integer coordinates for more efficient network bandwidth usage
	 * TODO: Is the extra accuracy worth it?
	 * SnapVector( muzzlePoint );
	 */
}

/**
 * CalcEyePoint
 *
 * Calculate the eye point relative to the origin
 */
void CalcEyePoint(gentity_t *ent, vec3_t eyePoint)
{
	VectorCopy(ent->s.pos.trBase, eyePoint);
	eyePoint[2] += ent->client->ps.viewheight;
}

/**
 * utPlantBomb
 *
 * Plant the bomb
 */
void utPlantBomb(gentity_t *ent)
{
	int    i, j = 0;
	gentity_t  *temp, *site[2], *bomb;

	/* Check game mode, also check if the bomb isn't already planted */
	if ((g_gametype.integer != GT_UT_BOMB) || (level.BombPlanted == qtrue)) {
		return;
	}

	/* check if we are close enough */
	/*
	 * TODO: Have this check done every frame in Bomb mode and send an
	 *	 Icon Draw
	 */
	for (i = 0; i < level.num_entities; i++) {
		temp = &level.gentities[i];

		if (temp->s.eType == ET_BOMBSITE) {
			if (j >= 2) {
				G_Error("Too Many Bombsites!");
			}
			site[j] = &level.gentities[i];
			j++;
		}
	}

	if ((Distance(ent->r.currentOrigin, site[0]->s.origin) >
		 BOMB_RADIUS) &&
		(Distance(ent->r.currentOrigin, site[1]->s.origin) >
		 BOMB_RADIUS)) {
		ent->client->ps.weaponTime	= 0;
		ent->client->ps.weaponstate = WEAPON_READY;
		return;
	}

	/* no planting in ze air */
	if (ent->client->ps.groundEntityNum == ENTITYNUM_NONE) {
		ent->client->ps.weaponTime	= 0;
		ent->client->ps.weaponstate = WEAPON_READY;
		return;
	}

	/* site to plant at */
	if (Distance(ent->r.currentOrigin, site[0]->s.origin) <
		Distance(ent->r.currentOrigin, site[1]->s.origin)) {
		VectorCopy(site[0]->s.origin, level.BombPlantOrigin);
	} else {
		VectorCopy(site[1]->s.origin, level.BombPlantOrigin);
	}

	/* Plant our bomb */
	bomb		= G_Spawn();
	bomb->classname = "temp_bomb";	 /* Planted bomb entity name */
	bomb->classhash = HSH_temp_bomb;   /* Planted bomb entity name */
	bomb->nextthink = level.time + 3000;
	bomb->think = utBombThink_Plant;
	bomb->parent	= ent;
	trap_LinkEntity(bomb);

	/* Set Plant Flag */
	ent->client->ps.stats[STAT_UTPMOVE_FLAGS] |= UT_PMF_PLANTING;
	ent->client->ps.eFlags					  |= EF_UT_PLANTING;
	level.BombPlanted						   = qtrue;
	level.BombPlantTime 					   = level.time;

	AddScore(ent, ent->r.currentOrigin, 1);
	Stats_AddScore(ent, ST_BOMB_PLANT);

	/* Planting bomb */
	G_AutoRadio(4, ent);
}

/**
 * FireWeapon
 */
void FireWeapon(gentity_t *ent)
{
	if (ent->client->IsDefusing) {
		return;
	}

	/* Track shots taken for accuracy tracking. */
	if (ent->s.weapon != UT_WP_KNIFE) {
		ent->client->accuracy_shots++;
	}

	AngleVectors(ent->client->ps.viewangles, forward, right, up);
	CalcMuzzlePoint(ent, forward, right, up, muzzle);

	/* fire the specific weapon */
	switch (ent->s.weapon) {
		case UT_WP_KNIFE:
			Weapon_Knife_Fire(ent);
			break;

		case UT_WP_BERETTA:
		case UT_WP_UMP45:
		case UT_WP_LR:
		case UT_WP_G36:
		case UT_WP_M4:
		case UT_WP_AK103:
		case UT_WP_PSG1:
		case UT_WP_DEAGLE:
		case UT_WP_SR8:
		case UT_WP_MP5K:
		//case UT_WP_P90:
		case UT_WP_NEGEV:
		case UT_WP_GLOCK:
		case UT_WP_COLT1911:
		case UT_WP_MAC11:
		/*case UT_WP_MAGNUM:
		case UT_WP_DUAL_BERETTA:
		case UT_WP_DUAL_DEAGLE:
		case UT_WP_DUAL_GLOCK:
		case UT_WP_DUAL_MAGNUM:*/
			utBulletFireFromDisposition(ent);
			break;

		case UT_WP_SPAS12:
		//case UT_WP_BENELLI:
			utWeaponFireShotgun(ent);
			break;

		case UT_WP_HK69:
			utWeaponFireHK69(ent);
			break;

		case UT_WP_GRENADE_HE:
#ifdef FLASHNADES
		case UT_WP_GRENADE_FLASH:
#endif
			utWeaponThrowGrenade(ent);
			G_AutoRadio(9, ent);
			break;

		case UT_WP_GRENADE_SMOKE:
			utWeaponThrowSmokeGrenade(ent);
			break;

		case UT_WP_BOMB:
			utPlantBomb(ent);
			break;

		default:
			break;
	}
}

/**
 * utWeaponCalcDamage
 *
 * Calculates the damage for a given weapon at a specified distance and
 * at the specified hit location.
 */
float utWeaponCalcDamage(gentity_t *ent, gentity_t *other, int weapon, ariesTraceResult_t *result, HitLocation_t *finalHit, float *damaccum) {

	float	  damage            = 0;
	float	  rangeDelta	    = 0.0f;
	float	  deltaScale	    = 1.0f;
	qboolean  bleed 	        = qfalse;
	qboolean  spawnProtection   = level.time - other->client->protectionTime < g_respawnProtection.integer * 1000;

	// Fenix: no damage in Jump mode
	if (g_gametype.integer == GT_JUMP) {
	    return 0;
	}

	/* No damage if they are invincible */
	if (spawnProtection) {
		G_AddEvent(ent, EV_UT_BULLET_HIT_PROTECTED, other->s.number);
		return 0;
	}

	*finalHit = result->hits[0].location;

	if (other->client) {

		if ((*finalHit == HL_TORSO) || (*finalHit == HL_VEST)) {
			*finalHit = utPSHasItem(&other->client->ps, UT_ITEM_VEST) ? HL_VEST : HL_TORSO;
		}

        if ((*finalHit == HL_HEAD) || (*finalHit == HL_HELMET)) {
            *finalHit = utPSHasItem(&other->client->ps, UT_ITEM_HELMET) ? HL_HELMET : HL_HEAD;
        }

	}

	/* If weapon is the SPAS. */
	if (weapon == UT_WP_SPAS12 /*|| (weapon == UT_WP_BENELLI)*/) {
		/* If there's a valid effective range set. */
		if (bg_weaponlist[weapon].effectiveRange > 0.0f) {
			/* Calculate fraction. */
			rangeDelta = result->hits[0].distance / (float)(bg_weaponlist[weapon].effectiveRange * 10);

			/* Clamp value. */
			if (rangeDelta > 1.0f) {
				rangeDelta = 1.0f;
			} else if (rangeDelta < 0.0f) {
				rangeDelta = 0.0f;
			}

			/* Calculate scale value. */
			deltaScale = 1.0f - rangeDelta;
			deltaScale = deltaScale * deltaScale * deltaScale;
		}
	}

	/* Weapon damage table times. */
	damage = bg_weaponlist[weapon].damage[*finalHit].value;
	bleed  = bg_weaponlist[weapon].damage[*finalHit].bleed;

	/* Cant damage more than max health */
	if (damage >= 1.0f) {
		damage = 1.0f;
	} else if (damage <= 0.0f) {
		return 0.0f;
	}

	//@Barbatos: lowered spas damages
	if (weapon == UT_WP_SPAS12 && damage > 0.3f) {
		damage = 0.3f;
	}

	/* Cumulative damage for the shotguns */
	if ((weapon == UT_WP_SPAS12 /*|| (weapon == UT_WP_BENELLI)*/) && damaccum) {
		*damaccum += damage;
	}

	/* Bleeding is disabled under certain conditions */
	bleed = (g_gametype.integer == GT_JUMP) || (other->flags & FL_GODMODE) || spawnProtection ? qfalse : bleed;

	/* Bleeding and limping */
	// @r00t: Bleed variables aren't exactly well named:
	//  >bleedRate = how long will medic take, decreased while meding, in seconds
	//  >bleedCount = number of hits (max 4), controls how often health is reduced by bleeding
	//  >bleedCounter = used to count 100ms ticks to bleed every N ticks
	//  >bleedResidual = counter to get 100ms ticks
	if (other->client && bleed) {
		/*
		// NOTE: at the current values, the bleedRate stuff is inoperant
		// the bleeding will stop at the first bleed tick like it does for hit locations that don't use this
		*/
		/* If this is a spas. */
		if (weapon == UT_WP_SPAS12 /* || (weapon == UT_WP_BENELLI)*/) {

		    if (rangeDelta < 0.15f) {

		        // Fenix: changed limping according to the new hitloc (12th June 2013)
			    if ((*finalHit == HL_LEGLL) || (*finalHit == HL_LEGLR) || (*finalHit == HL_FOOTL) || (*finalHit == HL_FOOTR)) {
			        other->client->ps.stats[STAT_UTPMOVE_FLAGS] |= UT_PMF_LIMPING;
			    }

				other->client->bleedRate += 0.1f;
			}

		} else {

		    // Fenix: changed limping according to the new hitloc (12th June 2013)
		    if ((*finalHit == HL_LEGLL) || (*finalHit == HL_LEGLR) || (*finalHit == HL_FOOTL) || (*finalHit == HL_FOOTR)) {
				other->client->ps.stats[STAT_UTPMOVE_FLAGS] |= UT_PMF_LIMPING;
				other->client->bleedRate += (other->client->bleedRate < 1.0f) ? 1.0f : 0.6f; // to match 4.1.1
			} else {
				other->client->bleedRate += (other->client->bleedRate < 0.5f) ? 0.5f : 0.6f;
			}

		}

		other->bleedEnemy = ent;
		other->client->bleedWeapon = weapon;

		if ( other->client->bleedCount < 4 ) {
			// we are counting by the number of bullets that hit you and did make you bleed
			// for regular weapons .. if you got hit four times and you're not dead .. you'll be bleeding heavily
			// we have to limit this because the SPAS is 20 bullets
			// another, maybe better way would be to count the damage areas instead of doing this
			other->client->bleedResidual = 0; //@r00t:No instant medic,restart timer
			other->client->bleedCount++;
		}

		other->client->ps.stats[STAT_UTPMOVE_FLAGS] |= UT_PMF_BLEEDING;
	}

	switch (*finalHit) {

        case HL_TORSO:
        case HL_VEST:
        case HL_GROIN:
        case HL_BUTT:
            other->client->ps.stats[STAT_HITS] |= 0x01;
            break;

        case HL_ARML:
        case HL_ARMR:
        case HL_LEGUL:
        case HL_LEGUR:
        case HL_LEGLL:
        case HL_LEGLR:
        case HL_FOOTL:
        case HL_FOOTR:
            other->client->ps.stats[STAT_HITS] |= 0x02;
            break;

        case HL_HEAD:
        case HL_HELMET:
            other->client->ps.stats[STAT_HITS] |= 0x04;
            break;

        default:
            break;
	}

	/* SR8 unzoom check */
	if ((other && other->client) &&
	    (utPSGetWeaponID(&other->client->ps) == UT_WP_SR8) &&
		(damage >= 0.20f) &&
		((*finalHit != HL_LEGLL) &&
		 (*finalHit != HL_LEGLR) &&
		 (*finalHit != HL_FOOTL) &&
		 (*finalHit != HL_FOOTR))) {

		other->client->sr8PenaltyTime = level.time + 200;

	}

	return damage;
}

/**
 * utBulletCalcTraceEnd
 *
 * Calculates the end of the trace to be used for firing a bullet. This
 * calculation takes into account spread and movement disposition.
 */
void utBulletCalcTraceEnd(gentity_t *ent, vec3_t muzzle,  vec3_t forward, vec3_t end) {

	float	fSpread;
	float	fRecoil;
	float	fRecoilSpread;
	float	fRecoilPercentageSpread;
	float	fRecoilPercentageRise;
	float	fRecoilRise;
	float	fMovementSpeed;
	float	fMovementSpeed2;
	float	fMovementMultiplier;
	float	fAngleToRadian;

	vec3_t	velocity;
	float	fOffsetX;
	float	fOffsetY;
	float	fGaussianX = 0;
	float	fGaussianY = 0;
	float	fDistance;

	int zoomcheck;
	int weaponID   = utPSGetWeaponID(&ent->client->ps);
	int weaponMode = utPSGetWeaponMode(&ent->client->ps);
	/* 2.6a - DensitY, increase zoom time for Sr8 */
	int ZoomTime;

	VectorCopy(ent->client->ps.velocity, velocity);

	fAngleToRadian = 2.0f * M_PI / 360.0f;
	fDistance	   = 8192 * 2;
	fSpread 	   = fAngleToRadian * bg_weaponlist[weaponID].modes[weaponMode].spread;

	if (ent->client->ps.pm_flags & PMF_DUCKED) {

		if (ent->client->ps.groundEntityNum != ENTITYNUM_NONE) {
			fSpread *= 0.75f;
		} else {
			fSpread *= 1.25f;
		}

	}

	if (ent->client->laserSight) {
		fSpread *= 0.60f;
	}

	fRecoil = (float)ent->client->ps.stats[STAT_EVENT_PARAM];
	fRecoil = fRecoil * bg_weaponlist[weaponID].modes[weaponMode].recoil.rate;
	fRecoil = fRecoil < UT_MAX_RECOIL ? fRecoil : UT_MAX_RECOIL;

	/* Reduce recoil with laser */
	if (ent->client->laserSight) {
		fRecoil *= 0.90f;
	}

	fRecoilSpread = fAngleToRadian * bg_weaponlist[weaponID].modes[weaponMode].recoil.spread;
	fRecoilRise   = fAngleToRadian * bg_weaponlist[weaponID].modes[weaponMode].recoil.rise;

	if (ent->client->laserSight) {
		fRecoilSpread *= 0.80f;
	}

	if (ent->client->laserSight) {
		fRecoilRise *= 0.80f;
	}

	fRecoilPercentageSpread = fRecoil / UT_MAX_RECOIL;
	fRecoilPercentageRise	= ((float)ent->client->ps.stats[STAT_EVENT_PARAM]) / UT_MAX_RECOIL;
	VectorNormalize(velocity, fMovementSpeed);

	/* If weapon is an sr8 and we're in the air. */
	if ((weaponID == UT_WP_SR8) && (ent->client->ps.groundEntityNum == ENTITYNUM_NONE)) {
		/*
		 * if this isnt set to a constant shots fired at the peak of a jump HAVE NO PENALTY
		 * (asshats)
		 */
		if (fMovementSpeed < UT_MAX_SPRINTSPEED * 0.66f) {
			fMovementSpeed = UT_MAX_SPRINTSPEED;
		}
	}

	fMovementSpeed		 = fMovementSpeed > UT_MAX_SPRINTSPEED ? UT_MAX_SPRINTSPEED : fMovementSpeed;
	fMovementSpeed		 = fAngleToRadian * fMovementSpeed / 100.0f;
	fMovementMultiplier  = bg_weaponlist[weaponID].modes[weaponMode].movementMultiplier * 0.4f;
	fMovementSpeed2 	 = fMovementSpeed > 200 ? (fMovementSpeed - 200) : 0;
	fMovementSpeed		 = fMovementSpeed - fMovementSpeed2;

	fMovementMultiplier *= 0.75f;

	/* If weapon is an sr8 and we're in the air. */
	if ((weaponID == UT_WP_SR8) &&
		(ent->client->ps.groundEntityNum == ENTITYNUM_NONE)) {
		/* Increase movement multiplier. */
		fMovementMultiplier *= 3.0f;
	}

	while (1) {

		float  fGaussian;

		fGaussianX = (random() - 0.5f) + (random() - 0.5f);
		fGaussianY = (random() - 0.5f) + (random() - 0.5f);
		fGaussian  = fGaussianX * fGaussianX + fGaussianY * fGaussianY;

		if ((fGaussian < 1) && (fGaussian > (0.15f * fRecoilPercentageSpread))) {
			break;
		}

	}

	zoomcheck = 0;

	/* DensitY: small fix for 27's code */
	if ((ent->client->buttons & BUTTON_ZOOM1) ||
		(ent->client->buttons & BUTTON_ZOOM2)) {
		zoomcheck = 1;
	}

	fOffsetX = tan((fSpread + (fRecoilSpread * fRecoilPercentageSpread) + (fMovementSpeed * fMovementMultiplier) + (fMovementSpeed * fMovementMultiplier * 2)) * fGaussianX) * fDistance;
	fOffsetY = tan((fSpread + (fRecoilSpread * fRecoilPercentageSpread) + (fMovementSpeed * fMovementMultiplier) + (fMovementSpeed * fMovementMultiplier * 2)) * fGaussianY);
	fOffsetY += tan(fRecoilRise * fRecoilPercentageRise);
	fOffsetY *= fDistance;

	if ((weaponID == UT_WP_SR8) && (level.time < ent->client->sr8PenaltyTime)) {
		zoomcheck = 0;
	}

	/*
	 * Dokta8: calculate zoom inaccuracy
	 * 0x3800 is the mask for zoom buttons - changes if they change buttons (see also pmove)!!
	 */
	if ((zoomcheck == 0) && ((weaponID == UT_WP_PSG1) || (weaponID == UT_WP_SR8))) {
		fOffsetX += (random() * 4096.0f) - 2048.0f;  /* not zoomed: set inaccuracy */
		fOffsetY += (random() * 4096.0f) - 2048.0f;
	}
	/* DensitY: we only do this for the first zoom! */
	else if ((zoomcheck == 1) && ((weaponID == UT_WP_PSG1) || (weaponID == UT_WP_SR8))) {
		/*
		 * If they *are* zoomed then check the zoom timer to prevent
		 * cheat0rs.
		 * Change here if you want different speeds!, this will be
		 * implemented into bg_weapons soon!
		 */
		ZoomTime = 150;

		if ((level.time - ent->client->lastZoomTime) < ZoomTime) {
			/*
			 * They have to be zoomed for at least half a
			 * second or else they are not zoomed -
			 * set inaccuracy
			 */
			fOffsetX += (random() * 4096.0f) - 2048.0f;
			fOffsetY += (random() * 4096.0f) - 2048.0f;
		}
	}

	/* testing no spread */
	//if (g_noSpread.integer) {
	//	fOffsetX = 0.0f;
	//	fOffsetY = 0.0f;
	//}

	VectorMA(muzzle, fDistance, forward, end);
	VectorMA(end, fOffsetX, right, end);
	VectorMA(end, fOffsetY, up, end);
}

/**
 * utTrace
 *
 * Traces a bullet through the world. A bullet will break stuff if it hits
 * and goes through it.
 */
qboolean utTrace(gentity_t *ent, trace_t *tr, vec3_t muzzle, vec3_t end, float damage, ariesTraceResult_t *result, qboolean antiLag) {

	gentity_t  *unlinkedEntities[MAX_BREAKABLE_HITS];
	gentity_t  *traceEnt;
	gentity_t  *tent;
	int        i;
	qboolean   hitreg	= qfalse;
	int        unlinked = 0;
	int        finaltime;

	/* Unlink the shooter so he doenst hit himself. */
	trap_UnlinkEntity(ent);

	/*
	 *NT - shift other clients back to the client's idea of the server
	 * time to compensate for lag
	 */
	if (ent->client && !(ent->r.svFlags & SVF_BOT) && (antiLag == qtrue)) {
		finaltime = ent->client->AttackTime - ent->client->pers.ut_timenudge;
		G_TimeShiftAllClients(finaltime, ent);
	}

	/*27 - Send off a copy of this to the client so we can compare how it's going. */
	if (g_antilagvis.integer) {
		G_SendBBOfAllClients(ent);
	}

	/* Run over the clients and increase their hit box size for this trace */
	for (i = 0 ; i < level.numConnectedClients; i++) {

		gentity_t  *other = &g_entities[level.sortedClients[i]];

		if (other == ent) {
			continue;
		}

		if ((other->client->sess.sessionTeam == TEAM_SPECTATOR) || other->client->ghost) {
			continue;
		}

		if (other->client->pers.substitute) {
			continue;
		}

		/* Adjust the hit box too */
		other->r.absmax[0] += 25;
		other->r.absmax[1] += 25;
		other->r.absmax[2] += 25;
		other->r.absmin[0] -= 25;
		other->r.absmin[1] -= 25;
		other->r.maxs[0]	= 29;
		other->r.maxs[1]	= 29;
		other->r.maxs[2]   += 15;
		other->r.mins[0]	= -29;
		other->r.mins[1]	= -29;
	}

	do {

		/* Trace the shot */
		trap_Trace(tr, muzzle, NULL, NULL, end, ent->s.number, MASK_SHOT);

		/* Make sure the trace entity isnt above the max */
		if (tr->entityNum >= ENTITYNUM_MAX_NORMAL) {
			break;
		}

		/* Get the trace entity */
		traceEnt = &g_entities[tr->entityNum];

		/*
		 * Apoxol: New hit detection system.  Since we know we hit a client we now
		 *	   need to figure out where on that client we hit.	We do this by retracing for
		 *	   each of the possible hit areas.
		 */

		if (traceEnt->takedamage && traceEnt->client && !(traceEnt->client->ghost)) {
			/* If this client has health then hit it, otherwise go through it */
			if (traceEnt->health > 0) {
				/*
				 * Calculate exact hit position on
				 * player model
				 */
				hitreg = G_AriesTrace(ent, tr, muzzle, end, result);

				if (hitreg) {
					break;
				}

				/*
				 * TODO: Old aries shot lagged players,
				 * even if it was a miss - a bit harsh?
				 * Or was it necessary?
				 */
			}

		} else {
			/* the thing we hit was not a breakable */
		    // if (strcmp(traceEnt->classname, "func_breakable") != 0) {
			if (traceEnt->classhash != HSH_func_breakable) {
				break;
			}

			/* otherwise it was a breakable, so damage it. */
			if (damage && (traceEnt->health > 0)) {

				G_Damage(traceEnt, ent, ent, forward, tr->endpos, 100, 0, bg_weaponlist[ent->s.weapon].mod, HL_UNKNOWN);

				/* set off an impact event so client can draw bullet holes */
				if (traceEnt->health > 0) { /* only mark it if it's not going to smash */
					SnapVectorTowards(tr->endpos, muzzle);
					tent			        = G_TempEntity(tr->endpos, EV_BULLET_HIT_BREAKABLE);
					tent->s.eventParm	    = DirToByte(tr->plane.normal);
					tent->s.otherEntityNum	= ent->breakthickness;
					tent->s.otherEntityNum2 = traceEnt->s.number;
					/*tent->s.time2 = ent->breaktype; */
					tent->s.time2		    = traceEnt->breaktype;
				}
			}

			/* If bullet is stopped. */
			if (traceEnt->breaktype != 0) {
				break;
			}
		}

		/* unlink this entity, so the next trace will go past it */
		trap_UnlinkEntity(traceEnt);
		unlinkedEntities[unlinked] = traceEnt;
		unlinked++;

	} while (unlinked < MAX_BREAKABLE_HITS);

	/* link back in any entities we unlinked */
	for (i = 0 ; i < unlinked ; i++) {
		trap_LinkEntity(unlinkedEntities[i]);
	}

	for (i = 0 ; i < level.numConnectedClients; i++) {

		gentity_t  *other = &g_entities[level.sortedClients[i]];

		if (other == ent) {
			continue;
		}

		if ((other->client->sess.sessionTeam == TEAM_SPECTATOR) || other->client->ghost) {
			continue;
		}

		if (other->client->pers.substitute) {
			continue;
		}

		other->r.absmax[0] -= 25;
		other->r.absmax[1] -= 25;
		other->r.absmax[2] -= 25;
		other->r.absmin[0] += 25;
		other->r.absmin[1] += 25;
		other->r.maxs[0]	= 15;
		other->r.maxs[1]	= 15;
		other->r.maxs[2]   -= 15;
		other->r.mins[0]	= -15;
		other->r.mins[1]	= -15;
	}

	/*NT - move the clients back to their proper positions */
	if (ent->client && !(ent->r.svFlags & SVF_BOT) && (antiLag == qtrue)) {
		/* if ( ent->client->pers.antiLag )	// clients can turn antilag on or off */
		G_UnTimeShiftAllClients(ent);
	}

	/* Relink the shooter */
	trap_LinkEntity(ent);

	return hitreg;
}

/**
 * utBulletHit
 *
 * Handles the result of a bullet hit.
 */
float  utBulletHit(gentity_t *ent, trace_t *tr, int weapon, ariesTraceResult_t *result, qboolean reportHit, float *damaccum) {

	gentity_t  *traceEnt;
	gentity_t  *tent;
	qboolean   spasfudge;

	if ((weapon == UT_WP_SPAS12 /*|| (weapon == UT_WP_BENELLI)*/) && reportHit) {
		spasfudge = qtrue;
	} else {
		spasfudge = qfalse;
	}

	/* Get the entity that was hit */
	traceEnt = &g_entities[tr->entityNum];

	/* Cause damage if this entity takes it */
	if (traceEnt->takedamage || spasfudge) {

		float		   damage = 0;
		HitLocation_t  hit	  = HL_UNKNOWN;

		/* Cause the damage now */
		if (traceEnt->client) {

			/* Invalid */
			if (!result || !result->hitCount) {  //@r00t: Added numHits check, fix for crash
				return 0;
			}

			if ((traceEnt != ent) && OnSameTeam(traceEnt, ent)) {
				if (!g_friendlyFire.integer) {
					return 0;
				}
			}

			if (!spasfudge) {

				/* Determine how much damage was caused */
				damage	= utWeaponCalcDamage(ent, traceEnt, weapon, result, &hit, damaccum);
				damage *= (traceEnt->client ? UT_MAX_HEALTH : 100);

				if (hit == HL_UNKNOWN) {
					damage = 0;
				}

			} else {

				damage = 1;
				hit    = result->hits[0].location;
				if (traceEnt->client) {

					if ((hit == HL_TORSO) || (hit == HL_VEST)) {
						hit = utPSHasItem(&traceEnt->client->ps, UT_ITEM_VEST) ? HL_VEST : HL_TORSO;
					}

					if ((hit == HL_HEAD) || (hit == HL_HELMET)) {
						hit = utPSHasItem(&traceEnt->client->ps, UT_ITEM_HELMET) ? HL_HELMET : HL_HEAD;
					}

				}
			}

			/* Send the bullet hit message */
			if (reportHit && damage) {

				if (g_loghits.integer) {

					G_LogPrintf("Hit: %i %i %i %i: %s hit"
							" %s in the %s\n",
							traceEnt->s.number,
							ent->s.number,
							hit,
							weapon,
							ent->client->pers.netname,
							traceEnt->client->pers.netname,
							c_ariesHitLocationNames[hit]);
				}

				tent                      = G_TempEntity(tr->endpos, EV_BULLET_HIT_FLESH);
				tent->r.svFlags          |= SVF_BROADCAST;
				tent->s.eventParm         = DirToByte(tr->plane.normal);
				tent->s.otherEntityNum    = ent->s.number;
				tent->s.otherEntityNum2   = traceEnt->s.number;
				tent->s.time2             = hit;
				tent->s.time              = result->entryTriangle;

				if ((weapon != UT_WP_SPAS12) /*&& (weapon != UT_WP_BENELLI)*/) {

					tent->s.generic1 = (int)damage;

				} else {

				    tent->s.generic1 = (int)(*damaccum * 100);

					if (tent->s.generic1 > UT_MAX_HEALTH) {
						tent->s.generic1 = UT_MAX_HEALTH;
					}

				}

				tent->s.legsAnim = weapon;	/* needed for kicking */
				/* #27 - client bullet Splash effects */

				VectorCopy(tr->endpos, tent->s.pos.trBase);  /*
										  *position
										  *Flag it if the players head exploded
										  */

				/* normal of bullet direction */
				VectorSubtract(ent->s.pos.trBase, traceEnt->s.pos.trBase, tent->s.pos.trDelta);

				VectorNormalizeNR(tent->s.pos.trDelta);
				VectorScale(forward, 500, tent->s.origin2);
				SnapVector(tent->s.origin2);
			}
		} else {
			damage = bg_weaponlist[weapon].damage[HL_UNKNOWN].value * 100.0f;
		}

		AngleVectors(ent->s.apos.trBase, forward, right, up);

		if (damage) {

		    /* Don't do this for the SPAS damage report */
			if (!spasfudge) {
				G_Damage(traceEnt, ent, ent, forward, tr->endpos, (int)damage, 0, bg_weaponlist[weapon].mod, hit);
			}

		}

		return damage;

	} else if (reportHit && !(tr->contents & CONTENTS_WATER)) {

		tent = G_TempEntity(tr->endpos, weapon == UT_WP_KNIFE_THROWN ? EV_KNIFE_HIT_WALL : EV_BULLET_HIT_WALL);
		/*
		 * 27 Take out if battle gets too quite..
		 *		tent->r.svFlags |= SVF_BROADCAST;
		 */
		tent->s.eventParm      = DirToByte(tr->plane.normal);
		tent->s.weapon         = weapon;
		tent->s.time2          = tr->surfaceFlags;
		tent->s.otherEntityNum = ent->s.number;
		tent->s.generic1       = 1;
	}

	return 0;
}

/**
 * utBulletFireFromDisposition
 *
 * Fires a bullet using the players movement to affect its accuracy.
 */
void utBulletFireFromDisposition(gentity_t *ent) {

	trace_t 			tr;
	vec3_t				end;
	vec3_t				forward;
	vec3_t				up;
	vec3_t				right;
	int 				hitreg;
	float				damage;
	int 				weaponSlot;
	int 				weaponID;
	int 				weaponMode;
	ariesTraceResult_t	result;

	/* Get the weapon */
	weaponSlot = utPSGetWeaponByID(&ent->client->ps, ent->s.weapon);
	weaponID   = UT_WEAPON_GETID(ent->client->ps.weaponData[weaponSlot]);
	weaponMode = UT_WEAPON_GETMODE(ent->client->ps.weaponData[weaponSlot]);

	AngleVectors(ent->client->ps.viewangles, forward, right, up);
	CalcEyePoint(ent, muzzle);

	utBulletCalcTraceEnd(ent, muzzle, forward, end);

	/* Traces a bullet's path */
	hitreg = utTrace(ent, &tr, muzzle, end, 1.0f, &result, qtrue);

	/* Make sure something was hit */
	if (tr.surfaceFlags & SURF_NOIMPACT) {
		return;
	}

	/* Perform the hit */
	damage = utBulletHit(ent, &tr, weaponID, &result, qtrue, NULL);

	/* Stat away the data */
	StatsEngineFire(ent->s.number, weaponID, result.hits[0].location, tr.entityNum, damage);
}

/**
 * utBulletFirePattern
 */
void utBulletFirePattern(gentity_t *ent, vec3_t origin, vec3_t origin2, int seed) {

	vec3_t				forward, right, up;
	vec3_t				point, offset, end;
	vec3_t				axis = { 0, 0, 1 };
	trace_t 			tr;
	float				spread, delta, dist;
	//int 				hitreg;
	int 				i,j;
	float				damac	= 0;
	//int 				savedEntNum = 0;
	gentity_t			*traceEnt;
	HitLocation_t		hits;
	ariesTraceResult_t	result;

	//int 				spasHitHead  = 0;
	//int 				spasHitTorso = 0;
	//int 				spasHitLegs  = 0;
	//int 				spasHitArmsL = 0;
	//int 				spasHitArmsR = 0;
	//qboolean			isHead		 = qfalse;
	//qboolean			isTorso 	 = qfalse;
  qboolean            reportHit;

    float hitstats[4][HL_MAX] = {0}; // head,torso,legs,armsL,armR
    int hitnums[4][HL_MAX] = {0}; // head,torso,legs,armsL,armR
    int hitents[4] = {ENTITYNUM_NONE,ENTITYNUM_NONE,ENTITYNUM_NONE,ENTITYNUM_NONE};

	/* Clear spas hits array */
	//memset(spasHits, 0, sizeof(spasHits));

	/* Calculate the player's orientation vectors. */
	VectorNormalize2NR(origin2, forward);
	PerpendicularVector(right, forward);
	CrossProduct(forward, right, up);

	CalcEyePoint(ent, muzzle);

	/* Calculate spread radius. */
	spread = tan(DEG2RAD(bg_weaponlist[ent->s.weapon].modes[0].spread)) * 16384;

	/* Generate firing pattern. */
	for (i = 0; i < UT_SHOTGUN_COUNT; i++) {

	    /* Set point. */
		point[0] = 0;
		point[1] = 1;
		point[2] = 0;

		/* Calculate point in firing pattern. */
		RotatePointAroundVector(offset, axis, point, (360 / UT_SHOTGUN_COUNT) * i);

		/* Calculate delta. */
		// delta = fabs(sin(Q_crandom(&seed)));
		_rndstep(&seed);delta = fabs(sin(_crandom(&seed)));

		/* Scale delta. */
		delta *= 1.0f;

		/* Adjust for spread. */
		offset[0] *= spread * delta;
		offset[1] *= spread * delta;

		/* Calculate end point. */
		VectorMA(origin, 16384, forward, end);

		/* Adjust end point for firing pattern. */
		VectorMA(end, offset[0], right, end);
		VectorMA(end, offset[1], up, end);

		/*
		 * Trace the pellet.
		 * If we hit something.
		 */
		if (utTrace(ent, &tr, muzzle, end, 1, &result, qtrue)) {

			/* Get the hit location */
			hits = result.hits[0].location;

			/* Calculate distance. */
			dist = Distance(origin, tr.endpos);

			/* If we're not too far away. */
			if (dist < 750.0f) {
//@r00t: New experimental SPAS code
#if 1
				float dmg = 0;
				/* Handle bullet hitting. */
				utBulletHit(ent, &tr, UT_WP_SPAS12, &result, qfalse, &dmg);

				if (tr.entityNum != ENTITYNUM_NONE) {
					int hent;
//					G_Printf("SPAS hit ent:%d where:%d dmg:%f\n",tr.entityNum,hits,dmg);
					if (hitents[0]==ENTITYNUM_NONE || hitents[0]==tr.entityNum) {
						hitents[0]=tr.entityNum;
						hent=0;
					} else
					if (hitents[1]==ENTITYNUM_NONE || hitents[1]==tr.entityNum) {
						hitents[1]=tr.entityNum;
						hent=1;
					} else
					if (hitents[2]==ENTITYNUM_NONE || hitents[2]==tr.entityNum) {
						hitents[2]=tr.entityNum;
						hent=2;
					} else
					if (hitents[3]==ENTITYNUM_NONE || hitents[3]==tr.entityNum) {
						hent=3;
						hitents[3]=tr.entityNum;
					} else continue;
					if (hits==HL_HELMET) hits=HL_HEAD; else
					if (hits==HL_VEST) hits=HL_TORSO;
					hitstats[hent][hits]+=dmg;
					hitnums[hent][hits]++;
				}
			}
		}
	}

//	G_Printf("--- SPAS ---\n");
	result.hitCount = 0;

	for (i = 0; i < 4; i++) {
        if (hitents[i] != ENTITYNUM_NONE) {
            tr.entityNum = hitents[i];
            result.hitCount = 1;
            traceEnt = &g_entities[tr.entityNum];

            for (j = 0; j < HL_MAX; j++) {
                if (hitnums[i][j]) {
                    float damac = hitstats[i][j];
                    result.hits[0].location = j;

                    // @fruk: dont report hit when target is already dead
                    reportHit = !(traceEnt->s.eFlags & EF_DEAD);
                    utBulletHit(ent, &tr, UT_WP_SPAS12, &result, reportHit, &damac);
                    StatsEngineFire(ent->s.number, UT_WP_SPAS12, j, tr.entityNum, damac * 100);
                }
            }
        }
	}

	if (!result.hitCount) {
		result.hitCount=1;
		result.hits[0].location = HL_UNKNOWN;
		StatsEngineFire(ent->s.number, UT_WP_SPAS12, result.hits[0].location, tr.entityNum, damac * 100);
	}

#else
				/* Handle bullet hitting. */
				utBulletHit(ent, &tr, UT_WP_SPAS12, &result, qfalse, &damac);

				switch (hits)
				{
					case HL_ARML:
						spasHitArmsL++;
						break;
					case HL_ARMR:
						spasHitArmsR++;
						break;
					case HL_LEGS:
						spasHitLegs++;
						break;
					case HL_VEST:
						spasHitTorso++;
						break;
					case HL_TORSO:
						spasHitTorso++;
						isTorso = qtrue;
						break;
					case HL_HELMET:
						spasHitHead++;
						break;
					case HL_HEAD:
						spasHitHead++;
						isHead = qtrue;
						break;
					default:
						break;
				}

				if (tr.entityNum != ENTITYNUM_NONE)
				{
					traceEnt = &g_entities[tr.entityNum];

					if (traceEnt->client)
					{
						savedEntNum = tr.entityNum;
					}
				}
			}
		}
	}

	if (damac > 0)
	{
		tr.entityNum = savedEntNum;

		if (spasHitArmsL + spasHitArmsR > spasHitLegs)
		{
			if (spasHitArmsL + spasHitArmsR > spasHitTorso)
			{
				if (spasHitArmsL + spasHitArmsR > spasHitHead)
					if (spasHitArmsL > spasHitArmsR)
						result.hits[0].location = HL_ARML;
					else
						result.hits[0].location = HL_ARMR;
				else
					result.hits[0].location = HL_HEAD;
			}
			else
			{
				if (spasHitHead > spasHitTorso)
					result.hits[0].location = HL_HEAD;
				else
					result.hits[0].location = HL_TORSO;
			}
		}
		else
		{
			if (spasHitLegs > spasHitTorso)
			{
				if (spasHitLegs > spasHitHead)
					result.hits[0].location = HL_LEGS;
				else
					result.hits[0].location = HL_HEAD;
			}
			else
			{
				if (spasHitHead > spasHitTorso)
					result.hits[0].location = HL_HEAD;
				else
					result.hits[0].location = HL_TORSO;
			}
		}

		if (hits == HL_TORSO && !isTorso) result.hits[0].location = HL_VEST;
		if (hits == HL_HEAD && !isHead) result.hits[0].location = HL_HELMET;

		result.hitCount=1; //@r00t: fix for skipping SPAS hits
		utBulletHit(ent, &tr, UT_WP_SPAS12, &result, qtrue, &damac);
	}
	else
	{
		result.hits[0].location = HL_UNKNOWN;
	}
	StatsEngineFire(ent->s.number, UT_WP_SPAS12, result.hits[0].location, tr.entityNum, damac * 100);
#endif
}

/**
 * utLaserThink
 *
 * Think for the laser sight
 */
void utLaserThink(gentity_t *self) {

	vec3_t	 end, start, forward, up;
	vec3_t	 angles;
	trace_t  tr;

	self->nextthink = level.time + 10;

	VectorCopy(self->parent->client->ps.viewangles, angles);
	AngleVectors(angles, forward, right, up);
	CalcEyePoint(self->parent, start);

	{
		float  fRecoilPercentage;
		float  fRecoilRise;
		float  fAngleToRadian;
		float  fOffsetY;
		float  fDistance;
		int    weaponID   = utPSGetWeaponID(&self->parent->client->ps);
		int    weaponMode = utPSGetWeaponMode(&self->parent->client->ps);

		fAngleToRadian	  = 2.0f * M_PI / 360.0f;
		fDistance	  = 8192 * 2;
		fRecoilRise   = fAngleToRadian *
					bg_weaponlist[weaponID].modes[weaponMode].recoil.rise;
		fRecoilPercentage =
			(float)self->parent->client->ps.stats[STAT_RECOIL] /
			30000.0f;

		fOffsetY  = tan(fRecoilRise * fRecoilPercentage);
		fOffsetY *= fDistance;

		if (fDistance) {
			VectorMA(start, fDistance, forward, end);
		}
		VectorMA(end, fOffsetY, up, end);
	}

	/* Trace Postition */
	trap_Trace(&tr, start, NULL, NULL, end, self->parent->s.number, MASK_SHOT);

	/* Hit anything? */
	if ((tr.surfaceFlags & SURF_NOIMPACT) || (tr.surfaceFlags & SURF_SKY)) {
		trap_UnlinkEntity(self);
		return;
	}

	/* Move forward to keep visible */
	if (tr.fraction != 1) {
		VectorMA(tr.endpos, -4, forward, tr.endpos);
	}

	/*(27) copy the old position into delta (for that streaky fx) */
	VectorCopy(self->s.pos.trBase, self->s.pos.trDelta);
	/*(27) Copy for fog volume tracking. */
	VectorCopy(start, self->s.apos.trBase);

	/* Set position */
	VectorCopy(tr.endpos, self->r.currentOrigin);
	VectorCopy(tr.endpos, self->s.pos.trBase);

	vectoangles(tr.plane.normal, self->s.angles);

	self->s.time = self->parent->client - level.clients ;

	trap_LinkEntity(self);
}

/**
 * G_Kick
 *
 * Performs a kick from a server event
 */
void G_Kick(gentity_t *kicker, gentity_t *target) {

	vec3_t			forward, right, up;
	vec3_t			dir;
	float			ff;
	ariesTraceResult_t	result;

	/*
	 * If friendly fire isnt on then dont let people kick
	 * their own teammates
	 */
	if (!g_friendlyFire.integer && (g_gametype.integer >= GT_TEAM) &&
		target->client && (target->client->sess.sessionTeam == kicker->client->sess.sessionTeam)) {
		return;
	}

	/*boot knife combo */
	if ((utPSGetWeaponID(&kicker->client->ps) != UT_WP_KNIFE) &&
		(utPSGetWeaponID(&kicker->client->ps) != UT_WP_DEAGLE) &&
		(utPSGetWeaponID(&kicker->client->ps) != UT_WP_GRENADE_HE) &&
		(utPSGetWeaponID(&kicker->client->ps) != UT_WP_GRENADE_FLASH) &&
		(utPSGetWeaponID(&kicker->client->ps) != UT_WP_GRENADE_SMOKE) &&
		(utPSGetWeaponID(&kicker->client->ps) != UT_WP_BERETTA) &&
		(utPSGetWeaponID(&kicker->client->ps) != UT_WP_GLOCK) &&
		(utPSGetWeaponID(&kicker->client->ps) != UT_WP_COLT1911) && /*
		(utPSGetWeaponID(&kicker->client->ps) != UT_WP_MAGNUM) &&*/
		(utPSGetWeaponID(&kicker->client->ps) != UT_WP_BOMB)) {
		return;
	}

	AngleVectors(kicker->client->ps.viewangles, forward, right, up);

	dir[0] = target->r.currentOrigin[0] - kicker->client->ps.origin[0];
	dir[1] = target->r.currentOrigin[1] - kicker->client->ps.origin[1];
	dir[2] = target->r.currentOrigin[2] - kicker->client->ps.origin[2];
	VectorNormalizeNR(dir);
	ff = DotProduct(dir, forward);

	if (ff > 0.8) {
		result.hits[0].location = HL_TORSO;
		result.hits[0].distance = 0.0f;
		utKickHit(kicker, target, &result);
	}
}

/**
 * utKickHit
 */
void utKickHit(gentity_t *ent, gentity_t *other, ariesTraceResult_t *result) {

	gentity_t  *tent;

	/* Cause damage if this entity takes it */
	if (other->takedamage) {

	    float damage;
		ariesHitLocation_t	hit;

		/* Cause the damage now */
		if (other->client) {

			if ((other != ent) && OnSameTeam(other, ent)) {
				if (!g_friendlyFire.integer) {
					return;
				}
			}

			/* Determine how much damage was caused */
			damage = utWeaponCalcDamage(ent, other, UT_WP_KICK, result, &hit, NULL);

			/* don't let kicks do more than 1/5th of total */
			if (damage > 0.2f) {
				damage = 0.2f;
			}

			damage *= (other->client ? UT_MAX_HEALTH : 100);

			/* Send the kick hit message */
			if (damage) {

				if (g_loghits.integer) {
					G_LogPrintf("Hit: %i %i %i %i: %s hit"
							" %s in the %s\n",
							other->s.number,
							ent->s.number,
							hit,
							UT_WP_KICK,
							ent->client->pers.netname,
							other->client->pers.netname,
							c_ariesHitLocationNames[hit]);

				}

				tent					= G_TempEntity(other->r.currentOrigin, EV_BULLET_HIT_FLESH);
				tent->s.eventParm		= DirToByte(forward);
				tent->s.otherEntityNum	= ent->s.number;
				tent->s.otherEntityNum2 = other->s.number;
				tent->s.time2			= hit;
				tent->s.time			= 0;
				tent->s.generic1		= (int)damage;
				tent->s.legsAnim		= UT_WP_KICK;

				/* #27 - client bullet Splash effects */
				VectorCopy(other->r.currentOrigin, tent->s.pos.trBase);
				VectorSubtract(ent->s.pos.trBase, other->s.pos.trBase, tent->s.pos.trDelta);
				VectorNormalizeNR(tent->s.pos.trDelta);

				VectorScale(forward, 500, tent->s.origin2);
				SnapVector(tent->s.origin2);
			}
		} else {
			damage = bg_weaponlist[UT_WP_KICK]. damage[HL_UNKNOWN].value * 100.0f;
		}

		AngleVectors(ent->s.apos.trBase, forward, right, up);

		if (damage) {

			G_Damage(other, ent, ent, forward,
				 other->r.currentOrigin,
				 (int)damage, 0,
				 bg_weaponlist[UT_WP_KICK].mod, hit);
		}
	}
}

/**
 * G_Goomba
 */
void G_Goomba(gentity_t *kicker, gentity_t *target, int dmg) {

	/*
	 * If friendly fire isnt on then dont let people kick
	 * their own teammates
	 */
	if (!g_friendlyFire.integer && (g_gametype.integer >= GT_TEAM)) {

	    if (target->client && (target->client->sess.sessionTeam == kicker->client->sess.sessionTeam)) {
			return;
		}

	}

	G_Damage(target, kicker, kicker, NULL, NULL, dmg, 0, UT_MOD_GOOMBA, HL_UNKNOWN);
}

#define UT_WPMODE_KNIFE_SLASH 0

/**
 * Weapon_Knife_Thrust
 *
 * Thrusts a knife
 */
void Weapon_Knife_Thrust(gentity_t *ent) {

	trace_t 		    tr;
	vec3_t			    end;
	vec3_t			    thrustangle;
	//int 		        triangle;
	int 		        hitreg;
	int 		        j;
	ariesTraceResult_t	result;

	thrustangle[0] = ent->client->ps.viewangles[0];
	thrustangle[1] = ent->client->ps.viewangles[1] - 30;
	thrustangle[2] = ent->client->ps.viewangles[2];

	if (thrustangle[1] < 0) {
		thrustangle[1] += 360;
	}

	AngleVectors(ent->client->ps.viewangles, forward, right, up);
	CalcEyePoint(ent, muzzle);

	for (j = 0; j < 6; j++) {

		/* set aiming directions */
		AngleVectors(thrustangle, forward, right, up);
		thrustangle[1] += 10;

		if (thrustangle[1] > 360) {
			thrustangle[1] -= 360;
		}

		VectorMA(muzzle, 30, forward, end);
		/* Trace for a player that got hit */
		hitreg = utTrace(ent, &tr, muzzle, end, 0.35f, &result, qtrue);

		/* Make sure something was hit */
		if ((tr.surfaceFlags & SURF_NOIMPACT) ||
			(tr.fraction == 1.0)) {
			continue;
		}

		/* Hit stuff */
		utBulletHit(ent, &tr, UT_WP_KNIFE, &result, qtrue, NULL);
		break;
	}
}

/**
 * Weapon_Knife_Fire
 *
 * Fires a knife
 */
void Weapon_Knife_Fire(gentity_t *ent) {

	/* If slash mode then dont throw anything */
	if (utPSGetWeaponMode(&ent->client->ps) == UT_WPMODE_KNIFE_SLASH) {
		Weapon_Knife_Thrust(ent);
		return;
	}

	CalcEyePoint(ent, muzzle);
	/* Spawn missle and forget it */
	utWeaponThrowKnife(ent, muzzle, forward);
}
