/**
 * Filename: c_weapons.h
 *
 * Copyright (C) 1999-2005 Id Software, Inc.
 * Copyright (C) 2000-2009 FrozenSand
 *
 * This file is part of Urban Terror source code.
 *
 */

#ifndef _C_WEAPONS_H_
#define _C_WEAPONS_H_

#include "../common/c_arieshl.h"
#include "../game/q_shared.h"

#define UT_WPFLAG_NOFLASH          (1 << 0)
#define UT_WPFLAG_NOCROSSHAIR      (1 << 1)
#define UT_WPFLAG_PENETRATE_KEVLAR (1 << 2)
#define UT_WPFLAG_PENETRATE_HELMET (1 << 3)
#define UT_WPFLAG_SINGLEBULLETCLIP (1 << 4)
#define UT_WPFLAG_REMOVEONEMPTY    (1 << 5)

#define UT_WPMODEFLAG_NOHOLD       (1 << 0)
#define UT_WPMODEFLAG_ALTANIMS     (1 << 1)
#define UT_WPMODEFLAG_NOAMMO       (1 << 2)
#define UT_WPMODEFLAG_HOLDTOREADY  (1 << 3)
#define UT_WPMODEFLAG_NORECOIL     (1 << 4)

typedef enum {
	UT_WP_NONE = 0,
	UT_WP_KNIFE,
	UT_WP_BERETTA,
	UT_WP_DEAGLE,
	UT_WP_SPAS12,
	UT_WP_MP5K,
	UT_WP_UMP45,
	UT_WP_HK69,
	UT_WP_LR,
	UT_WP_G36,
	UT_WP_PSG1,
	UT_WP_GRENADE_HE,
	UT_WP_GRENADE_FLASH,
	UT_WP_GRENADE_SMOKE,
	UT_WP_SR8,
	UT_WP_AK103,
	UT_WP_BOMB,
	UT_WP_NEGEV,
	UT_WP_GRENADE_FRAG,
	UT_WP_M4,
	UT_WP_GLOCK,
	UT_WP_COLT1911,
	UT_WP_MAC11,
	UT_WP_P90,		/* 23 — FN P90 (activated in 4.3) */
	UT_WP_BENELLI,		/* 24 — Benelli M4 Super 90 (activated in 4.3) */
	UT_WP_MAGNUM,		/* 25 — .44 Magnum (activated in 4.3) */
	UT_WP_FRF1,		/* 26 — FR-F1 sniper rifle (new in 4.3) */
	UT_WP_TOD50,		/* 27 — reserved; weapon not shipped in 4.3.4 (slot exists, no data) */

	/* Dual weapons remain disabled */
	/*UT_WP_DUAL_BERETTA,
	UT_WP_DUAL_DEAGLE,
	UT_WP_DUAL_GLOCK,
	UT_WP_DUAL_MAGNUM,*/

	UT_WP_NUM_WEAPONS,	/* 28 — confirmed from 4.3.4 QVM bytecode analysis */
	UT_WP_KICK,
	UT_WP_KNIFE_THROWN,
} weapon_t;

typedef enum {
	UT_WPTYPE_MELEE,
	UT_WPTYPE_THROW,
	UT_WPTYPE_SMALL_GUN,
	UT_WPTYPE_SMALL_GUN_DUAL,
	UT_WPTYPE_MEDIUM_GUN,
	UT_WPTYPE_MEDIUM_GUN_DUAL,
	UT_WPTYPE_LARGE_GUN,
	UT_WPTYPE_PLANT
} weaponType_t;

#define MAX_ZOOM_LEVELS  4
#define MAX_WEAPON_MODES 4

typedef struct {
	int    delay;
	int    fall;
	float  rise;
	float  spread;
	float  rate;
} weaponRecoil_t;

typedef struct {
	char		 *name;

	int 		 flags;

	int 		 fireTime;
	int 		 changeTime;

	float		 spread;
	float		 movementMultiplier;
	weaponRecoil_t	 recoil;

	int 		 burstTime;
	int 		 burstRounds;
} weaponMode_t;

typedef struct {
	float value;
	qboolean bleed;
} damage_t;

typedef struct gweapon_s {
	char		*name;
	char		*modelName;
	int 		flags;
	weaponType_t	type;
	int 		powerups;

	int 		botInventoryWeaponID;
	int 		botInventoryAmmoID;

	int 		clips;
	int 		bullets;

	int 		reloadTime;
	int 		reloadStartTime;
	int 		reloadEndTime;

	damage_t	damage[HL_MAX];
	int 		effectiveRange;
	float		knockback;

	int 		q3weaponid;
	int 		mod;

	int 		brassFrame;
	float		brassLengthScale;
	float		brassDiameterScale;
	float		silencerWorldScale;
	float		silencerViewScale;
	float		flashScale;

	int 		zoomfov[MAX_ZOOM_LEVELS];
	weaponMode_t	modes[MAX_WEAPON_MODES];
} weaponData_t;

extern weaponData_t    bg_weaponlist[];
extern int		   bg_numWeapons;

#endif /* _C_WEAPONS_H_ */
