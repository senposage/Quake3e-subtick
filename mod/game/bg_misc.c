// Copyright (C) 1999-2000 Id Software, Inc.
//
// bg_misc.c -- both games misc functions, all completely stateless

#include "q_shared.h"
#include "bg_public.h"
#include "inv.h"

float	trGravity     = 1.0f;	// Dok: hack to use variable gravity in trajectory calcs

vec3_t	bg_colors[20] = {
	{   0,	 0, 255 }, //blue
	{ 255,	 0,  0	}, //red
	{   0, 255,   0 }, //green
	{   0,	 0,   0 }, //green
	{ 255, 255,   0 }, //yellow
	{ 255,	 0,  255}, //purple
	{   0, 255, 255 }, //teal
	{ 255, 255, 255 }, //white
	{  28, 122,  74 }, //army green
	{ 130,	30,  124}, //violet
	{ 128, 128,  255}, //whiteblue
	{ 255, 168,  0	}, //orange
	{ 255, 200,  128}, //tan
	{   0, 255,  168}, //greeny
	{ 255, 128,  200}, //pink
	{ 255, 128,  128}, //whitered
	{ 120, 120,   55}, //bronze
	{ 128, 255,  128}, //whitegreen
	{  64,	64,   64}, //grey
};

char	bg_handSkins[12][32] = {
	"hand_swat.skin",
	"hand_tag.skin",
	"hand_free.skin", // @Barbatos - free team hand skin
	"hand_jump.skin", // @Barbatos - jump mode hand skin
	"hand_orange.skin",
	"hand_olive.skin",
	"hand_white.skin",
	"hand_black.skin",
	"hand_desert.skin",
	"hand_cowboy.skin",
	"hand_cavalry.skin",
	"hand_droogs.skin",
};

//9 groups, 10 messages
char  RadioTextList[9][10][40] = {
	{
		"Responses", //Title
		"Affirmative",
		"Negative",
		"I'm on it!",
		"Area Secured",
		"Base is secure",
		"Medic on the way...",
		"I've got your back",
		"Enemy Terminated",
		"",
	},
	{
		"Orders",
		"Move in",
		"Fallback and regroup",
		"Hold your position",
		"Stick with me",
		"Cover me",
		"Requesting backup",
		"Go for the objective",
		"Flank them!",
		"Double time, let's move!",
	},
	{
		"Condition",
		"I'm moving in",
		"Awaiting orders",
		"I need a medic!",
		"Objective in sight",
		"Objective is clear",
		"I'm attacking",
		"I'm defending",
		"I'm flanking",
		"Holding here",
	},
	{
		"Queries",
		"Status?",
		"Objective status?",
		"Base status?",
		"Where's the enemy?",
		"Where are the medics?",
		"Anyone need support?",
		"Anyone need a medic?",
		"Who's ya daddy?",
		"How the hell are ya?",
	},
	{
		"Enemy Activity",
		"Enemy spotted",
		"Enemy heard",
		"Enemy is flanking!",
		"Enemy headed your way!",
		"Incoming!",
		"",
		"",
		"",
		"Objective in danger!"
	},
	{
		"Directional Calls",
		"North",
		"South",
		"East",
		"West",
		"Base",
		"High",
		"Low",
		"Water",
		"Here"
	},
	{
		"Capture The Flag",
		"I've got the flag",
		"I'm going for the flag",
		"They have our flag",
		"Base is being overrun!",
		"Recover the flag!",
		"Flag exiting left!",
		"Flag exiting right!",
		"Flag exiting front!",
		"Flag exiting back!"
	},
	{
		"Bomb Mode",
		"Heading to Bombsite Red",
		"Heading to Bombsite Black",
		"Enemy at Bombsite Red",
		"Enemy at Bombsite Black",
		"I have the bomb",
		"The bomb is loose!",
		"",
		"",
		""
	},
	{
		"Miscellaneous",
		"Good job, team",
		"Nice one",
		"Check your fire!",
		"Sorry about that",
		"Whatever",
		"No problem",
		"Oh, you idiot",
		"What the f***, over?",
		"Thanks"
	}
};

/*QUAKED item_***** ( 0 0 0 ) (-16 -16 -16) (16 16 16) suspended
DO NOT USE THIS CLASS, IT JUST HOLDS GENERAL INFORMATION.
The suspended flag will allow items to hang in the air, otherwise they are dropped to the next surface.

If an item is the target of another entity, it will not spawn in until fired.

An item fires all of its targets when it is picked up.	If the toucher can't carry it, the targets won't be fired.

"notfree" if set to 1, don't spawn in free for all games
"notteam" if set to 1, don't spawn in team games
"notsingle" if set to 1, don't spawn in single player games
"wait"	override the default wait before respawning.  -1 = never respawn automatically, which can be used with targeted spawning.
"random" random number of plus or minus seconds varied from the respawn time
"count" override quantity or duration on most items.
*/

gitem_t  bg_itemlist[] = {
	{
		"none",
		NULL,
		{ NULL,
		  NULL,
		  0, 0},
		"icons/weapons/none",
		NULL,
		"None",
		0,
		0,
		0,
		"",
		""
	},	// leave index 0 alone

	{
		"team_CTF_redflag",
		NULL,
		{ "models/flags/r_flag.md3",
		  0, 0, 0 },
		"icons/iconf_red1",
		NULL,
		"Red Flag",
		0,
		IT_TEAM,
		PW_REDFLAG,
		"",
		"",
		PW_REDFLAG,
	},

	{
		"team_CTF_blueflag",
		NULL,
		{ "models/flags/b_flag.md3",
		  0, 0, 0 },
		"icons/iconf_blu1",
		NULL,
		"Blue Flag",
		0,
		IT_TEAM,
		PW_BLUEFLAG,
		"",
		"",
		PW_BLUEFLAG,
	},

	{
		"team_CTF_neutralflag",
		NULL,
		{ "models/powerups/instant/haste.md3",
		  0, 0, 0 },
		"icons/iconf_neutral1",
		NULL,
		"Neutral Flag",
		0,
		IT_TEAM,
		PW_NEUTRALFLAG,
		"",
		"",
		PW_NEUTRALFLAG,
	},

	{
		"ut_weapon_knife",
		"sound/misc/w_pkup.wav",
		{
			"models/weapons2/knife/knife.md3",
			"models/weapons2/knife/knife_hold.md3",
			0, 0
		},
		"icons/weapons/knife",
		"icons/ammo/kbar",
		"Knife",
		0,
		IT_WEAPON,
		UT_WP_KNIFE,
		"",
		"",

		0,
	},

	{
		"ut_weapon_beretta",
		"sound/misc/w_pkup.wav",
		{
			"models/weapons2/beretta/beretta.md3",
			"models/weapons2/beretta/beretta_hold.md3",
			0, 0
		},
		"icons/weapons/beretta",
		"icons/ammo/beretta",
		"Beretta 92G",
		0,
		IT_WEAPON,
		UT_WP_BERETTA,
		"",
		"",

		0,
	},

	{
		"ut_weapon_deagle",
		"sound/misc/w_pkup.wav",
		{
			"models/weapons2/deserteagle/deagle.md3",
			"models/weapons2/deserteagle/deagle_hold.md3",
			0, 0
		},
		"icons/weapons/deserteagle",
		"icons/ammo/deserteagle",
		"IMI DE .50 AE",
		0,
		IT_WEAPON,
		UT_WP_DEAGLE,
		"",
		"",

		0,
	},

	{
		"ut_weapon_spas12",
		"sound/misc/w_pkup.wav",
		{
			"models/weapons2/spas12/spas12.md3",
			"models/weapons2/spas12/spas12_hold.md3",
			0, 0
		},
		"icons/weapons/spas12",
		"icons/ammo/spas12",
		"Franchi SPAS12",
		0,
		IT_WEAPON,
		UT_WP_SPAS12,
		"",
		"",

		0,
	},

	{
		"ut_weapon_mp5k",
		"sound/misc/w_pkup.wav",
		{
			"models/weapons2/mp5k/mp5k.md3",
			"models/weapons2/mp5k/mp5k_hold.md3",
			0, 0
		},
		"icons/weapons/mp5k",
		"icons/ammo/mp5k",
		"HK MP5K",
		0,
		IT_WEAPON,
		UT_WP_MP5K,
		"",
		"",

		0,
	},

	{
		"ut_weapon_ump45",
		"sound/misc/w_pkup.wav",
		{
			"models/weapons2/ump45/ump45.md3",
			"models/weapons2/ump45/ump45_hold.md3",
			0, 0
		},
		"icons/weapons/ump45",
		"icons/ammo/ump45",
		"HK UMP45",
		0,
		IT_WEAPON,
		UT_WP_UMP45,
		"",
		"",

		0,
	},

	{
		"ut_weapon_hk69",
		"sound/misc/w_pkup.wav",
		{
			"models/weapons2/hk69/hk69.md3",
			"models/weapons2/hk69/hk69_hold.md3",
			0, 0
		},
		"icons/weapons/hk69",
		"icons/ammo/hk69",
		"HK69 40mm",
		0,
		IT_WEAPON,
		UT_WP_HK69,
		"",
		"",

		0,
	},

	{
		"ut_weapon_lr",
		"sound/misc/w_pkup.wav",
		{
			"models/weapons2/zm300/lr.md3",
			"models/weapons2/zm300/lr_hold.md3",
			0, 0
		},
		"icons/weapons/lr",
		"icons/ammo/lr",
		"ZM LR300",
		0,
		IT_WEAPON,
		UT_WP_LR,
		"",
		"",

		0,
	},

	{
		"ut_weapon_g36",
		"sound/misc/w_pkup.wav",
		{
			"models/weapons2/g36/g36.md3",
			"models/weapons2/g36/g36_hold.md3",
			0, 0
		},
		"icons/weapons/g36",
		"icons/ammo/g36",
		"HK G36",
		0,
		IT_WEAPON,
		UT_WP_G36,
		"",
		"",

		0,
	},

	{
		"ut_weapon_psg1",
		"sound/misc/w_pkup.wav",
		{
			"models/weapons2/psg1/psg1.md3",
			"models/weapons2/psg1/psg1_hold.md3",
			0, 0
		},
		"icons/weapons/psg1",
		"icons/ammo/psg1",
		"HK PSG1",
		0,
		IT_WEAPON,
		UT_WP_PSG1,
		"",
		"",

		0,
	},

	{
		"ut_weapon_grenade_he",
		"sound/misc/w_pkup.wav",
		{
			"models/weapons2/grenade/grenade.md3",
			"models/weapons2/grenade/grenade_hold.md3",
			0, 0
		},
		"icons/weapons/grenade_he",
		"icons/ammo/grenade_he",
		"HE Grenade",
		0,
		IT_WEAPON,
		UT_WP_GRENADE_HE,
		"",
		"",

		0,
	},

	//{ NULL },

	{
		"ut_weapon_grenade_flash",
		"sound/misc/w_pkup.wav",
		{
			"models/weapons2/grenade/grenade.md3",
			"models/weapons2/grenade/grenade_hold.md3",
			0, 0
		},
		"icons/weapons/grenade_flash",
		"icons/ammo/grenade_flash",
		"Flash Grenade",
		0,
		IT_WEAPON,
		UT_WP_GRENADE_FLASH,
		"",
		"",

		0,
	},

	{
		"ut_weapon_grenade_smoke",
		"sound/misc/w_pkup.wav",
		{
			"models/weapons2/grenade/grenade.md3",
			"models/weapons2/grenade/grenade_hold.md3",
			0, 0
		},
		"icons/weapons/grenade_smoke",
		"icons/ammo/grenade_smoke",
		"Smoke Grenade",
		0,
		IT_WEAPON,
		UT_WP_GRENADE_SMOKE,
		"",
		"",

		0,
	},
	{
		"ut_item_vest",
		"sound/misc/w_pkup.wav",
		{
			"models/players/athena/vest.md3",
			0, 0, 0
		},
		"icons/items/vest",
		NULL,
		"Kevlar Vest",
		1,
		IT_HOLDABLE,
		0,
		"",
		"",
		PW_VEST,
	},

	{
		"ut_item_nvg",
		"sound/misc/w_pkup.wav",
		{
			"models/players/athena/nvg.md3",
			0, 0, 0
		},
		"icons/items/nvg",
		NULL,
		"TacGoggles",
		1,
		IT_HOLDABLE,
		0,
		"",
		"",
		PW_NVG,
	},

	{
		"ut_item_medkit",
		"sound/misc/w_pkup.wav",
		{
			"models/players/gear/backpack.md3",
			0, 0
		},
		"icons/items/medkit",
		NULL,
		"Medkit",
		1,
		IT_HOLDABLE,
		0,
		"",
		"sound/items/use_medkit.wav",

		PW_MEDKIT,
	},

	{
		"ut_item_silencer",
		"sound/misc/w_pkup.wav",
		{
			"models/weapons2/m4/m4_silencer.md3",
			0, 0, 0
		},
		"icons/items/silencer",
		NULL,
		"Silencer",
		1,
		IT_HOLDABLE,
		0,
		"",
		"",
		PW_SILENCER,
	},

	{
		"ut_item_laser",
		"sound/misc/w_pkup.wav",
		{
			"models/weapons2/m4/m4_laser.md3",
			0, 0, 0
		},
		"icons/items/laser",
		NULL,
		"Laser Sight",
		1,
		IT_HOLDABLE,
		0,
		"",
		"",

		PW_LASER,
	},

	{
		"ut_item_helmet",
		"sound/misc/w_pkup.wav",
		{
			"models/players/athena/helmet.md3",
			0, 0, 0
		},
		"icons/items/helmet",
		NULL,
		"Helmet",
		1,
		IT_HOLDABLE,
		0,
		"",
		"",

		PW_HELMET,
	},

	{
		"ut_item_extraammo",
		"sound/misc/w_pkup.wav",
		{
			"models/players/athena/helmet.md3",
			0, 0, 0
		},
		"icons/items/extraammo",
		NULL,
		"Extra Ammo",
		1,
		IT_HOLDABLE,
		0,
		"",
		"",

		0,
	},

	{
		"ut_item_apr",
		"sound/misc/w_pkup.wav",
		{
			"models/players/athena/helmet.MD3",
			0, 0, 0
		},
		"icons/items/apr",
		NULL,
		"Exploding Monkey",
		1,
		IT_HOLDABLE,
		0,
		"",
		"",

		0,
	},

	{
		"ut_weapon_sr8",
		"sound/misc/w_pkup.wav",
		{
			"models/weapons2/sr8/sr8.md3",
			"models/weapons2/sr8/sr8_hold.md3",
			0, 0
		},
		"icons/weapons/sr8",
		"icons/ammo/sr8",
		"Remington SR8",
		0,
		IT_WEAPON,
		UT_WP_SR8,
		"",
		"",

		0,
	},

	{
		"ut_weapon_ak103",
		"sound/misc/w_pkup.wav",
		{
			"models/weapons2/ak103/ak103.md3",
			"models/weapons2/ak103/ak103_hold.md3",
			0, 0
		},
		"icons/weapons/ak103",
		"icons/ammo/ak103",
		"AK103 7.62mm",
		0,
		IT_WEAPON,
		UT_WP_AK103,
		"",
		"",

		0,
	},
	{
		"ut_weapon_bomb",
		"sound/misc/w_pkup.wav",
		{
			"models/weapons2/bombbag/bombbag.md3",
			"models/weapons2/bombbag/bombbag_hold.md3",
			0, 0
		},
		"icons/weapons/bomb",
		"icons/ammo/bomb",
		"BOMB",
		0,
		IT_WEAPON,
		UT_WP_BOMB,
		"",
		"",

		0,
	},

	{
		"ut_weapon_negev",
		"sound/misc/w_pkup.wav",
		{
			"models/weapons2/negev/negev.md3",
			"models/weapons2/negev/negev_hold.md3",
			0, 0
		},
		"icons/weapons/negev",
		"icons/ammo/negev",
		"IMI Negev",
		0,
		IT_WEAPON,
		UT_WP_NEGEV,
		"",
		"",

		0,
	},

	//@Barbatos: well it's just to have something here
	{
		"ut_weapon_grenade_frag",
		"sound/misc/w_pkup.wav",
		{
			"models/weapons2/grenade/grenade.md3",
			"models/weapons2/grenade/grenade_hold.md3",
			0, 0
		},
		"icons/weapons/grenade_frag",
		"icons/ammo/grenade_frag",
		"Flash Grenade",
		0,
		IT_WEAPON,
		UT_WP_GRENADE_FRAG,
		"",
		"",

		0,
	},

	{
		"ut_weapon_m4",
		"sound/misc/w_pkup.wav",
		{
			"models/weapons2/m4/m4.md3",
			"models/weapons2/m4/m4_hold.md3",
			0, 0
		},
		"icons/weapons/m4",
		"icons/ammo/m4",
		"Colt M4A1",
		0,
		IT_WEAPON,
		UT_WP_M4,
		"",
		"",

		0,
	},

	{
		"ut_weapon_glock",
		"sound/misc/w_pkup.wav",
		{
			"models/weapons2/glock/glock.md3",
			"models/weapons2/glock/glock_hold.md3",
			0, 0
		},
		"icons/weapons/glock",
		"icons/ammo/glock",
		"Glock 18",
		0,
		IT_WEAPON,
		UT_WP_GLOCK,
		"",
		"",

		0,
	},

	{
        "ut_weapon_colt1911",
        "sound/misc/w_pkup.wav",
        {
            "models/weapons2/colt1911/colt1911.md3",
            "models/weapons2/colt1911/colt1911_hold.md3",
            0, 0
        },
        "icons/weapons/colt1911",
        "icons/ammo/colt1911",
        "Colt 1911",
        0,
        IT_WEAPON,
        UT_WP_COLT1911,
        "",
        "",

        0,
    },

    {
        "ut_weapon_mac11",
        "sound/misc/w_pkup.wav",
        {
            "models/weapons2/mac11/mac11.md3",
            "models/weapons2/mac11/mac11_hold.md3",
            0, 0
        },
        "icons/weapons/mac11",
        "icons/ammo/mac11",
        "Mac 11",
        0,
        IT_WEAPON,
        UT_WP_MAC11,
        "",
        "",

        0,
    },

	/*{
		"ut_weapon_p90",
		"sound/misc/w_pkup.wav",
		{
			"models/weapons2/p90/p90.md3",
			"models/weapons2/p90/p90_hold.md3",
			0, 0
		},
		"icons/weapons/p90",
		"icons/ammo/p90",
		"FN P90",
		0,
		IT_WEAPON,
		UT_WP_P90,
		"",
		"",

		0,
	},

	{
		"ut_weapon_benelli",
		"sound/misc/w_pkup.wav",
		{
			"models/weapons2/benellim9/benellim9.md3",
			"models/weapons2/benellim9/benellim9_hold.md3",
			0, 0
		},
		"icons/weapons/benellim9",
		"icons/ammo/benellim9",
		"Benelli M4 Super 90",
		0,
		IT_WEAPON,
		UT_WP_BENELLI,
		"",
		"",

		0,
	},

	{
		"ut_weapon_magnum",
		"sound/misc/w_pkup.wav",
		{
			"models/weapons2/44magnum/44magnum.md3",
			"models/weapons2/44magnum/44magnum_hold.MD3",
			0, 0
		},
		"icons/weapons/44magnum",
		"icons/ammo/44magnum",
		".44 Magnum",
		0,
		IT_WEAPON,
		UT_WP_MAGNUM,
		"",
		"",

		0,
	},


	{
		"ut_weapon_dual_beretta",
		"sound/misc/w_pkup.wav",
		{
			"models/weapons2/beretta/beretta.md3",
			"models/weapons2/d-beretta/beretta_hold.md3",
			0, 0
		},
		"icons/weapons/beretta",
		"icons/ammo/beretta",
		"Dual Beretta 92Gs",
		0,
		IT_WEAPON,
		UT_WP_DUAL_BERETTA,
		"",
		"",

		0,
	},

	{
		"ut_weapon_dual_deagle",
		"sound/misc/w_pkup.wav",
		{
			"models/weapons2/d-deseagle/deagle.md3",
			"models/weapons2/d-deseagle/deagle_hold.md3",
			0, 0
		},
		"icons/weapons/deserteagle",
		"icons/ammo/deserteagle",
		"IMI DE .50 AE",
		0,
		IT_WEAPON,
		UT_WP_DUAL_DEAGLE,
		"",
		"",

		0,
	},

	{
		"ut_weapon_dual_glock",
		"sound/misc/w_pkup.wav",
		{
			"models/weapons2/d-glock/glock.md3",
			"models/weapons2/d-glock/glock_hold.MD3",
			0, 0
		},
		"icons/weapons/glock",
		"icons/ammo/glock",
		"Dual Glocks",
		0,
		IT_WEAPON,
		UT_WP_DUAL_GLOCK,
		"",
		"",

		0,
	},

	{
		"ut_weapon_dual_magnum",
		"sound/misc/w_pkup.wav",
		{
			"models/weapons2/d-44magnum/44magnum.md3",
			"models/weapons2/d-44magnum/44magnum_hold.MD3",
			0, 0
		},
		"icons/weapons/44magnum",
		"icons/ammo/44magum",
		"Dual .44 Magnums",
		0,
		IT_WEAPON,
		UT_WP_DUAL_MAGNUM,
		"",
		"",

		0,
	},*/

/*
	{
		"ut_item_bomb",
		"sound/misc/w_pkup.wav",
	{
			"models/gear/explosives.md3",
			0, 0, 0
		},
		"icons/items/c4",
		"icons/items/c4not",
		"C4",
		1,
		IT_TEAM,
		PW_NEUTRALFLAG,
		"",
		"",

		PW_NEUTRALFLAG,
	},
*/

	// end of list marker
	{ NULL}
};

int	 bg_numItems = sizeof(bg_itemlist) / sizeof(bg_itemlist[0]) - 1;

/*
==============
BG_FindItemForPowerup
==============
*/
gitem_t *BG_FindItemForPowerup( powerup_t pw )
{
	int  i;

	for (i = 0 ; i < bg_numItems ; i++)
	{
		if (((bg_itemlist[i].giType == IT_POWERUP) ||
		     (bg_itemlist[i].giType == IT_TEAM) ||
		     (bg_itemlist[i].giType == IT_PERSISTANT_POWERUP)) &&
		    (bg_itemlist[i].giTag == pw))
		{
			return &bg_itemlist[i];
		}
	}

	return NULL;
}

/*
==============
BG_FindItemForHoldable
==============
*/
gitem_t *BG_FindItemForHoldable( holdable_t pw )
{
	int  i;

	for (i = 0 ; i < bg_numItems ; i++)
	{
		if ((bg_itemlist[i].giType == IT_HOLDABLE) && (bg_itemlist[i].giTag == pw))
		{
			return &bg_itemlist[i];
		}
	}

	Com_Error( ERR_DROP, "HoldableItem not found" );

	return NULL;
}

/*
===============
BG_FindItemForWeapon

===============
*/
gitem_t *BG_FindItemForWeapon( weapon_t weapon )
{
	gitem_t  *it;

	for (it = bg_itemlist + 1 ; it->classname ; it++)
	{
		if ((it->giType == IT_WEAPON) && (it->giTag == weapon))
		{
			return it;
		}
	}

	Com_Error( ERR_DROP, "Couldn't find item for weapon %i", weapon);
	return NULL;
}

/*
===============
BG_FindItem

===============
*/
gitem_t *BG_FindItem( const char *pickupName )
{
	gitem_t  *it;

	for (it = bg_itemlist + 1 ; it->classname ; it++)
	{
		if (!Q_stricmp( it->pickup_name, pickupName ))
		{
			return it;
		}
	}

	return NULL;
}


gitem_t *BG_FindItemForClassname ( const char *classname )
{
	gitem_t  *it;

	for (it = bg_itemlist + 1 ; it->classname ; it++)
	{
		if (!Q_stricmp( it->classname, classname ))
		{
			return it;
		}
	}

	return NULL;
}

/*
============
BG_PlayerTouchesItem

Items can be picked up without actually touching their physical bounds to make
grabbing them easier
============
*/
qboolean	BG_PlayerTouchesItem( playerState_t *ps, entityState_t *item, int atTime )
{
	vec3_t	origin;

	BG_EvaluateTrajectory( &item->pos, atTime, origin );

	// we are ignoring ducked differences here
	if ((ps->origin[0] - origin[0] > 44)
	    || (ps->origin[0] - origin[0] < -50)
	    || (ps->origin[1] - origin[1] > 36)
	    || (ps->origin[1] - origin[1] < -36)
	    || (ps->origin[2] - origin[2] > 36)
	    || (ps->origin[2] - origin[2] < -36))
	{
		return qfalse;
	}

	return qtrue;
}

/*
================
BG_CanItemBeGrabbed

Returns false if the item should not be picked up.
This needs to be the same for client side prediction and server use.
================
*/
qboolean BG_CanItemBeGrabbed( int gametype, const entityState_t *ent, const playerState_t *ps )
{
	gitem_t  *item;

#ifdef MISSIONPACK
//	int		upperBound;
#endif

	if ((ent->modelindex < 1) || (ent->modelindex >= bg_numItems))
	{
		Com_Error( ERR_DROP, "BG_CanItemBeGrabbed: index out of range" );
	}

	item = &bg_itemlist[ent->modelindex];

	// No picking up this item right now.
	if (ent->eFlags & EF_NOPICKUP)
	{
		return qfalse;
	}

	// If this is a weapon.
	if ((item->giType == IT_WEAPON) || (item->giType == IT_AMMO))
	{
		// If we're busy doing something then we can't pickup stuff.
		if ((ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_BANDAGING) || (ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_CLIMBING) ||
		    (ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_DEFUSING) || (ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_RELOADING) ||
		    (ps->pm_flags & PMF_LADDER_UPPER) || (ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_PLANTING) ||
		    ((utPSGetWeaponID(ps) == UT_WP_NEGEV) && (ps->weaponstate == WEAPON_READY_FIRE) && (ps->weaponTime > 0)))
		{
			return qfalse;
		}
	}

	switch(item->giType)
	{
		// Make sure they dont already have a weapon of that type.
		case IT_WEAPON:
			{
				int  weaponID;
				int  numSmall  = 0;
				int  numMedium = 0;
				int  numBig    = 0;
				int  small[UT_MAX_WEAPONS];
				int  medium[UT_MAX_WEAPONS];
				int  big[UT_MAX_WEAPONS];
				int  i, j, k;

				// D8: don't allow pickup of grenades if player already has enough
				// note - the addammo func already makes sure we don't add too many
				// grens, what this does is means that the grens stay on the ground
				// so someone else can get them
				if ((item->giTag == UT_WP_GRENADE_HE) || (item->giTag == UT_WP_GRENADE_FLASH) || (item->giTag == UT_WP_GRENADE_SMOKE))
				{
					int  grenWeaponSlot = utPSGetWeaponByID( ps, item->giTag );

					if ((grenWeaponSlot != -1) && (UT_WEAPON_GETBULLETS( ps->weaponData[grenWeaponSlot]) >= 2))
					{
						return qfalse;
					}
				}

				if (-1 != utPSGetWeaponByID ( ps, item->giTag ))
				{
					return qtrue;
				}

				if ((bg_weaponlist[item->giTag].type == UT_WPTYPE_MELEE) ||
				    (bg_weaponlist[item->giTag].type == UT_WPTYPE_THROW))
				{
					return qtrue;
				}

				for(i = 0; i < UT_MAX_WEAPONS; i++) // MAX_WEAPONS
				{
					weaponID = UT_WEAPON_GETID(ps->weaponData[i]);

					if (!weaponID)
					{
						continue;
					}

					switch (bg_weaponlist[weaponID].type)
					{
						case UT_WPTYPE_SMALL_GUN:
							small[numSmall++] = weaponID;
							break;

						case UT_WPTYPE_SMALL_GUN_DUAL:
						case UT_WPTYPE_MEDIUM_GUN:
							medium[numMedium++] = weaponID;
							break;

						case UT_WPTYPE_LARGE_GUN:
							big[numBig++] = weaponID;
							break;
						
						default:
							continue;
					}
				}

				switch (bg_weaponlist[item->giTag].type)
				{
					default:
						break;

					// Old
					/*
					case UT_WPTYPE_SMALL_GUN:
						if ( small == 0 )
							return qtrue;
						break;

					case UT_WPTYPE_MEDIUM_GUN:
					case UT_WPTYPE_LARGE_GUN:
						if ( big < 2 )
							return qtrue;
						break;
					case UT_WPTYPE_PLANT: // Our Team Check will be server side
						return qtrue;
					*/

					case UT_WPTYPE_SMALL_GUN:

						if (numSmall == 0)
						{
							return qtrue;
						}
						break;

	  			case UT_WPTYPE_SMALL_GUN_DUAL:
					case UT_WPTYPE_MEDIUM_GUN:

						if (numBig == 0)
						{
							if (numMedium < 2)
							{
								return qtrue;
							}
						}
						else if (numBig == 1)
						{
							if (numMedium == 0)
							{
								k = 0;

								for (j = 0; j < numBig; j++)
								{
									if (big[j] == UT_WP_NEGEV)
									{
										k = 1;
										break;
									}
								}

								if (!k)
								{
									return qtrue;
								}
							}
						}
						break;

					case UT_WPTYPE_LARGE_GUN:

						if (numBig == 0)
						{
							if (numMedium == 0)
							{
								return qtrue;
							}
							else if (numMedium == 1)
							{
								if (item->giTag != UT_WP_NEGEV)
								{
									return qtrue;
								}
							}
						}
						break;

					case UT_WPTYPE_PLANT: // Our Team Check will be server side
						return qtrue;
				}

				return qfalse;
			}

		case IT_AMMO:
			{
				int  weapon = utPSGetWeaponByID ( ps, item->giTag );

				if(-1 == weapon)
				{
					return qfalse;
				}

				if (UT_WEAPON_GETBULLETS(weapon) >= 200)
				{
					return qfalse;
				}

				return qtrue;
			}

		case IT_ARMOR:
/* URBAN TERROR - no armor
#ifdef MISSIONPACK
		if( bg_itemlist[ps->stats[STAT_PERSISTANT_POWERUP]].giTag == PW_SCOUT ) {
			return qfalse;
		}

		// we also clamp armor to the maxhealth for handicapping
		if( bg_itemlist[ps->stats[STAT_PERSISTANT_POWERUP]].giTag == PW_GUARD ) {
			upperBound = ps->stats[STAT_MAX_HEALTH];
		}
		else {
			upperBound = ps->stats[STAT_MAX_HEALTH] * 2;
		}

		if ( ps->stats[STAT_ARMOR] >= upperBound ) {
			return qfalse;
		}
#else
		if ( ps->stats[STAT_ARMOR] >= ps->stats[STAT_MAX_HEALTH] * 2 ) {
			return qfalse;
		}
#endif
*/
			return qtrue;

		case IT_HEALTH:
/* URBAN TERROR - no health
		// small and mega healths will go over the max, otherwise
		// don't pick up if already at max
#ifdef MISSIONPACK
		if( bg_itemlist[ps->stats[STAT_PERSISTANT_POWERUP]].giTag == PW_GUARD ) {
			upperBound = ps->stats[STAT_MAX_HEALTH];
		}
		else
#endif
		if ( item->quantity == 5 || item->quantity == 100 ) {
			if ( ps->stats[STAT_HEALTH] >= ps->stats[STAT_MAX_HEALTH] * 2 ) {
				return qfalse;
			}
			return qtrue;
		}

		if ( ps->stats[STAT_HEALTH] >= ps->stats[STAT_MAX_HEALTH] ) {
			return qfalse;
		}
*/
			return qtrue;

		case IT_POWERUP:
			return qtrue; // powerups are always picked up

#ifdef MISSIONPACK
		case IT_PERSISTANT_POWERUP:
			// can only hold one item at a time
/*		if ( ps->stats[STAT_PERSISTANT_POWERUP] ) {
			return qfalse;
		}

		// check team only
		if( ( ent->generic1 & 2 ) && ( ps->persistant[PERS_TEAM] != TEAM_RED ) ) {
			return qfalse;
		}
		if( ( ent->generic1 & 4 ) && ( ps->persistant[PERS_TEAM] != TEAM_BLUE ) ) {
			return qfalse;
		}

		return qtrue; */
#endif

		// team items, such as flags
		case IT_TEAM:

			switch (gametype)
			{
				// Anyone can pick up the bomb in this gametype
				case GT_CTF:

					// ent->modelindex2 is non-zero on items if they are dropped
					// we need to know this because we can pick up our dropped flag (and return it)
					// but we can't pick up our flag at base
					if (ps->persistant[PERS_TEAM] == TEAM_RED)
					{
						if ((item->giTag == PW_BLUEFLAG) ||
						    ((item->giTag == PW_REDFLAG) && ent->modelindex2) ||
						    ((item->giTag == PW_REDFLAG) && utPSHasItem ( ps, UT_ITEM_BLUEFLAG )))
						{
							return qtrue;
						}
					}
					else if (ps->persistant[PERS_TEAM] == TEAM_BLUE)
					{
						if ((item->giTag == PW_REDFLAG) ||
						    ((item->giTag == PW_BLUEFLAG) && ent->modelindex2) ||
						    ((item->giTag == PW_BLUEFLAG) && utPSHasItem ( ps, UT_ITEM_REDFLAG)))
						{
							return qtrue;
						}
					}

					break;


				case GT_JUMP:

					//@Fenix - JUMP mode -> we can pick up all the flags
					if (ps->persistant[PERS_TEAM] == TEAM_FREE) {
						return qtrue;
					}

					break;
			}

			return qfalse;

		case IT_HOLDABLE:

			// Cant carry two of the same thing.
			if (utPSHasItem ( ps, ent->modelindex ))
			{
				return qfalse;
			}

			return qtrue;

		case IT_BAD:
			Com_Error( ERR_DROP, "BG_CanItemBeGrabbed: IT_BAD" );
			break;
	}

	return qfalse;
}

//======================================================================

/*
================
BG_EvaluateTrajectory

================
*/
void BG_EvaluateTrajectory( const trajectory_t *tr, int atTime, vec3_t result )
{
	float	deltaTime;
	float	phase;
	float	fraction;
	vec3_t	nextpos, s, t;

	switch(tr->trType)
	{
		case TR_STATIONARY:
		case TR_INTERPOLATE:
			VectorCopy( tr->trBase, result );
			break;

		case TR_LINEAR:
			deltaTime = (atTime - tr->trTime) * 0.001; // milliseconds to seconds
			VectorMA( tr->trBase, deltaTime, tr->trDelta, result );
			break;

		case TR_SINE:
			deltaTime = (atTime - tr->trTime) / (float)tr->trDuration;
			phase	  = sin( deltaTime * M_PI * 2 );
			VectorMA( tr->trBase, phase, tr->trDelta, result );
			break;

		case TR_LINEAR_STOP:

			if (atTime > tr->trTime + tr->trDuration)
			{
				atTime = tr->trTime + tr->trDuration;
			}
			deltaTime = (atTime - tr->trTime) * 0.001; // milliseconds to seconds

			if (deltaTime < 0)
			{
				deltaTime = 0;
			}
			VectorMA( tr->trBase, deltaTime, tr->trDelta, result );
			break;

		case TR_WATER:
			deltaTime  = (atTime - tr->trTime) * 0.001; // milliseconds to seconds
			VectorMA( tr->trBase, deltaTime, tr->trDelta, result );
			result[2] -= 0.5 * DEFAULT_GRAVITY * deltaTime * deltaTime * 0.2f;	// FIXME: local gravity...
			result[2] -= tr->trDuration;
			break;

		case TR_GRAVITY:
			deltaTime  = (atTime - tr->trTime) * 0.001; // milliseconds to seconds
			VectorMA( tr->trBase, deltaTime, tr->trDelta, result );
			result[2] -= 0.5 * DEFAULT_GRAVITY * deltaTime * deltaTime;	// FIXME: local gravity...
			break;

		case TR_GRAVITY2:
			// Dokta8: same as gravity, but uses trajectory->gfactor
			// to reduce the gravity effect on the particle, simulating
			// things like smoke or dust particles

			deltaTime  = (atTime - tr->trTime) * 0.001; // milliseconds to seconds
			VectorMA( tr->trBase, deltaTime, tr->trDelta, result );
			result[2] -= 0.5 * DEFAULT_GRAVITY * trGravity * deltaTime * deltaTime; // FIXME: local gravity...
			break;

		case TR_FEATHER:
								   // dok: fall softly after initial rapid deceleration
			deltaTime = (atTime - tr->trTime) * 0.001; // milliseconds to seconds

			if (tr->trDuration)
			{
				fraction = (float)(atTime - tr->trTime) / (float)tr->trDuration;
			}
			else
			{
				fraction = 0.0f;
			}

			if (fraction < 0.10f)
			{
				result[0] = tr->trBase[0] + ((tr->trDelta[0] * deltaTime) + (0.5f * -(tr->trDelta[0] / (tr->trDuration * 0.001 * 0.25)) * deltaTime * deltaTime));
				result[1] = tr->trBase[1] + ((tr->trDelta[1] * deltaTime) + (0.5f * -(tr->trDelta[1] / (tr->trDuration * 0.001 * 0.25)) * deltaTime * deltaTime));
				result[2] = tr->trBase[2] + ((tr->trDelta[2] * deltaTime) + (0.5f * -(tr->trDelta[2] / (tr->trDuration * 0.001 * 0.25)) * deltaTime * deltaTime));
			}
			else
			{
				phase	   = (float)tr->trDuration * 0.10f * 0.001f; // delta time at 25%
				deltaTime -= phase;				     // correct deltatime to ignore phase component
				result[0]  = tr->trBase[0] + ((tr->trDelta[0] * phase) + (0.5f * -(tr->trDelta[0] / (tr->trDuration * 0.001 * 0.25)) * phase * phase));
				result[1]  = tr->trBase[1] + ((tr->trDelta[1] * phase) + (0.5f * -(tr->trDelta[1] / (tr->trDuration * 0.001 * 0.25)) * phase * phase));
				result[2]  = tr->trBase[2] + ((tr->trDelta[2] * phase) + (0.5f * -(tr->trDelta[2] / (tr->trDuration * 0.001 * 0.25)) * phase * phase));

				result[0] += (15.0 * (fraction - 0.10)) * cos(deltaTime * M_PI * 2);
				result[1] += (15.0 * (fraction - 0.10)) * sin(deltaTime * M_PI * 2);
				result[2] -= 0.5 * DEFAULT_GRAVITY * trGravity * deltaTime * deltaTime; // accelerate slower
			}

			break;

		case TR_EJECTA:
			// Dok::for sparks and other fast-moving rapidly decelerating short-lived particles
			// decelrate tr_linear to a halt over duration

			// s = u * t + 0.5 * a * t * t
			// where a = -initial velocity/time
			// and time (t) is in seconds

			deltaTime = (atTime - tr->trTime) * 0.001;
			result[0] = tr->trBase[0] + ((tr->trDelta[0] * deltaTime) + (0.5f * -(tr->trDelta[0] / (tr->trDuration * 0.001)) * deltaTime * deltaTime));
			result[1] = tr->trBase[1] + ((tr->trDelta[1] * deltaTime) + (0.5f * -(tr->trDelta[1] / (tr->trDuration * 0.001)) * deltaTime * deltaTime));
			result[2] = tr->trBase[2] + ((tr->trDelta[2] * deltaTime) + (0.5f * -(tr->trDelta[2] / (tr->trDuration * 0.001)) * deltaTime * deltaTime));
			break;

		case TR_EJECTA2:
			// ejected matter - high speed short-lived, spiralling

			// calc position in future
			deltaTime  = ((atTime + 50) - tr->trTime) * 0.001; // milliseconds to seconds
			nextpos[0] = tr->trBase[0] + ((tr->trDelta[0] * deltaTime) + (0.5f * -(tr->trDelta[0] / (tr->trDuration * 0.001)) * deltaTime * deltaTime));
			nextpos[1] = tr->trBase[1] + ((tr->trDelta[1] * deltaTime) + (0.5f * -(tr->trDelta[1] / (tr->trDuration * 0.001)) * deltaTime * deltaTime));
			nextpos[2] = tr->trBase[2] + ((tr->trDelta[2] * deltaTime) + (0.5f * -(tr->trDelta[2] / (tr->trDuration * 0.001)) * deltaTime * deltaTime));

			// calc position now
			deltaTime = (atTime - tr->trTime) * 0.001;
			result[0] = tr->trBase[0] + ((tr->trDelta[0] * deltaTime) + (0.5f * -(tr->trDelta[0] / (tr->trDuration * 0.001)) * deltaTime * deltaTime));
			result[1] = tr->trBase[1] + ((tr->trDelta[1] * deltaTime) + (0.5f * -(tr->trDelta[1] / (tr->trDuration * 0.001)) * deltaTime * deltaTime));
			result[2] = tr->trBase[2] + ((tr->trDelta[2] * deltaTime) + (0.5f * -(tr->trDelta[2] / (tr->trDuration * 0.001)) * deltaTime * deltaTime));

			// make a direction vector
			VectorSubtract( nextpos, result, nextpos );
			VectorNormalizeNR ( nextpos );
			PerpendicularVector( s, nextpos );
			CrossProduct( nextpos, s, t );

			if (tr->trDuration)
			{
				fraction = 1.0f - (float)(atTime - tr->trTime) / (float)tr->trDuration;
			}
			else
			{
				fraction = 1.0f;
			}

			if (fraction <= 0.0f)
			{
				fraction = 0.0f;
			}

			if (fraction > 1.0f)
			{
				fraction = 1.0f;
			}

			// spin around axis of direction of travel
			VectorMA( result, cos( deltaTime * M_PI * (20.0f * fraction)) * (fraction * 20), s, result );
			VectorMA( result, sin( deltaTime * M_PI * (20.0f * fraction)) * (fraction * 20), t, result );
			break;

		case TR_SNOWFLAKE:
								    // spiral down to earth
			deltaTime  = (atTime - tr->trTime) * 0.001; // milliseconds to seconds
			VectorMA( tr->trBase, deltaTime, tr->trDelta, result );
			result[0] += 50 * cos(deltaTime * M_PI / 4);
			result[1] += 50 * sin(deltaTime * M_PI / 4);
			break;

		default:
			Com_Error( ERR_DROP, "BG_EvaluateTrajectory: unknown trType: %i", tr->trType  );
			break;
	}
}

/*
================
BG_EvaluateTrajectoryDelta

For determining velocity at a given time
================
*/
void BG_EvaluateTrajectoryDelta( const trajectory_t *tr, int atTime, vec3_t result )
{
	float  deltaTime;
	float  phase;

	switch(tr->trType)
	{
		case TR_STATIONARY:
		case TR_INTERPOLATE:
			VectorClear( result );
			break;

		case TR_SNOWFLAKE:
		case TR_LINEAR:
			VectorCopy( tr->trDelta, result );
			break;

		case TR_SINE:
			deltaTime = (atTime - tr->trTime) / (float)tr->trDuration;
			phase	  = cos( deltaTime * M_PI * 2 ); // derivative of sin = cos
			phase	 *= 0.5;
			VectorScale( tr->trDelta, phase, result );
			break;

		case TR_LINEAR_STOP:

			if (atTime > tr->trTime + tr->trDuration)
			{
				VectorClear( result );
				return;
			}
			VectorCopy( tr->trDelta, result );
			break;

		case TR_WATER:
			deltaTime  = (atTime - tr->trTime) * 0.001; // milliseconds to seconds
			VectorCopy( tr->trDelta, result );
			result[2] -= 0.5 * DEFAULT_GRAVITY * deltaTime * deltaTime * 0.2f;	// FIXME: local gravity...
			result[2] -= tr->trDuration;
			break;

		case TR_GRAVITY:
			deltaTime  = (atTime - tr->trTime) * 0.001; // milliseconds to seconds
			VectorCopy( tr->trDelta, result );
			result[2] -= DEFAULT_GRAVITY * deltaTime;	// FIXME: local gravity...
			break;

		case TR_EJECTA:
		case TR_EJECTA2:
			// fix me
			VectorCopy( tr->trDelta, result );
			break;

		case TR_GRAVITY2:
								    // Dokta8: same as gravity, but uses trajectory->gfactor
								    // to reduce the gravity effect on the particle, simulating
								    // things like smoke or dust particles
			deltaTime  = (atTime - tr->trTime) * 0.001; // milliseconds to seconds
			VectorCopy( tr->trDelta, result );
			result[2] -= DEFAULT_GRAVITY * trGravity * deltaTime;	// FIXME: local gravity...
			break;

		default:
			Com_Error( ERR_DROP, "BG_EvaluateTrajectoryDelta: unknown trType: %i", tr->trTime );
			break;
	}
}
#ifdef _DEBUG
#error This table is outdated
char  *eventnames[] = {
	"EV_NONE",

	"EV_FOOTSTEP",
	"EV_FOOTSTEP_METAL",
	"EV_FOOTSPLASH",
	"EV_FOOTWADE",
	"EV_SWIM",
	"EV_STEP",
	"EV_STEP_4",
	"EV_STEP_8",
	"EV_STEP_12",
	"EV_STEP_16",

	"EV_FALL_SHORT",
	"EV_FALL_MEDIUM",
	"EV_FALL_FAR",

	"EV_JUMP",
	"EV_WATER_TOUCH",	  // foot touches
	"EV_WATER_TOUCH_FAST",	  // foot touches
	"EV_WATER_LEAVE",	  // foot leaves
	"EV_WATER_UNDER",	  // head touches
	"EV_WATER_CLEAR",	  // head leaves

	"EV_ITEM_PICKUP",	  // normal item pickups are predictable
	"EV_GLOBAL_ITEM_PICKUP",  // powerup / team sounds are broadcast to everyone

	"EV_NOAMMO",
	"EV_CHANGE_WEAPON",
	"EV_FIRE_WEAPON",

	"EV_ITEM_RESPAWN",
	"EV_PLAYER_TELEPORT_IN",
	"EV_PLAYER_TELEPORT_OUT",

	"EV_GRENADE_BOUNCE",	  // eventParm will be the soundindex

	"EV_GENERAL_SOUND",
	"EV_GLOBAL_SOUND",	  // no attenuation
	"EV_GLOBAL_TEAM_SOUND",

	"EV_BULLET_HIT_FLESH",
	"EV_BULLET_HIT_WALL",
	"EV_BULLET_HIT_BREAKABLE",

	"EV_MISSILE_HIT",
	"EV_MISSILE_MISS",
	"EV_MISSILE_MISS_METAL",
	"EV_GREN_SMOKING",
	"EV_GREN_STOPSMOKING",
	"EV_SHOTGUN",
	"EV_BULLET",		  // otherEntity is the shooter

	"EV_PAIN",
	"EV_DEATH1",
	"EV_DEATH2",
	"EV_DEATH3",
	"EV_OBITUARY",

	"EV_POWERUP_QUAD",
	"EV_POWERUP_BATTLESUIT",
	"EV_POWERUP_REGEN",

	"EV_GIB_PLAYER",	  // gib a previously living player
	"EV_SCOREPLUM", 	  // score plum

	"EV_KAMIKAZE",		  // kamikaze explodes

	"EV_DEBUG_LINE",
	"EV_STOPLOOPINGSOUND",
	"EV_TAUNT",

	// Urban Terror extra events
	"EV_BREAK",		  // called when a breakable is smashed
	"EV_RELOAD",
	"EV_BANDAGE",
	"EV_TEAM_BANDAGE",
	"EV_BOMB_BASE",
	"EV_KICK",
	"EV_RADIO",
	"EV_UT_STARTRECORD", //@Barbatos
};
#endif
/*
===============
BG_AddPredictableEventToPlayerstate

Handles the sequence numbers
===============
*/

void trap_Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize );

/**
 * $(function)
 */
void BG_AddPredictableEventToPlayerstate( int newEvent, int eventParm, playerState_t *ps )
{
#ifdef _DEBUG
	{
		char  buf[256];
		trap_Cvar_VariableStringBuffer("showevents", buf, sizeof(buf));

		if (atof(buf) != 0)
		{
 #ifdef QAGAME
			Com_Printf(" game event svt %5d -> %5d: num = %20s parm %d\n", ps->pmove_framecount /*ps->commandTime*/, ps->eventSequence, eventnames[newEvent], eventParm);
 #else
			Com_Printf("Cgame event svt %5d -> %5d: num = %20s parm %d\n", ps->pmove_framecount /*ps->commandTime*/, ps->eventSequence, eventnames[newEvent], eventParm);
 #endif
		}
	}
#endif
	ps->events[ps->eventSequence & (MAX_PS_EVENTS - 1)]	= CSE_ENC(newEvent);
	ps->eventParms[ps->eventSequence & (MAX_PS_EVENTS - 1)] = eventParm;
	ps->eventSequence++;
}

/**
 * $(function)
 */
void utPlayerStateToEntityState ( playerState_t *ps, entityState_t *s )
{
	int  i;
	int  small;
	int  medium;
	int  large;
	int  weaponID;

	s->weapon	   = UT_WEAPON_GETID(ps->weaponData[ps->weaponslot]);
	s->groundEntityNum = ps->groundEntityNum;

	s->powerups	   = 0;

	for (i = 0; i < UT_MAX_ITEMS; i++)
	{
		int  itemID    = UT_ITEM_GETID(ps->itemData[i]);
		int  itemFlags = UT_ITEM_GETFLAGS(ps->itemData[i]);

		if(itemID)
		{
			switch(bg_itemlist[itemID].powerup)
			{
				case PW_LASER:
				case PW_SILENCER:

					if(itemFlags & UT_ITEMFLAG_ON)
					{
						if(bg_weaponlist[s->weapon].powerups & POWERUP(bg_itemlist[itemID].powerup))
						{
							s->powerups |= POWERUP(bg_itemlist[itemID].powerup);
						}
					}
					break;

				case PW_NVG:

					if (ps->stats[STAT_HEALTH] > 0)
					{
						if(itemFlags & UT_ITEMFLAG_ON)
						{
							s->powerups |= POWERUP(bg_itemlist[itemID].powerup);
						}
					}
					break;

				case PW_BACKPACK:
				case PW_MEDKIT:
				case PW_NEUTRALFLAG:
				case PW_REDFLAG:
				case PW_BLUEFLAG:
				case PW_VEST:
				case PW_HELMET:
					s->powerups |= POWERUP(bg_itemlist[itemID].powerup);
					break;
			}
		}
	}

	//Determine if they have a bomb
	for(i = 0; i < UT_MAX_WEAPONS; i++)   // MAX_WEAPONS
	{
		weaponID = UT_WEAPON_GETID(ps->weaponData[i]);

		if (!weaponID)
		{
			continue;
		}

		if (weaponID == UT_WP_BOMB)
		{
			s->powerups |= POWERUP(PW_CARRYINGBOMB);
			break;
		}
	}

	if(ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_BLEEDING)
	{
		s->powerups |= POWERUP(PW_BLEEDING);
	}

	if (ps->pm_flags & PMF_LADDER_UPPER)
	{
		s->powerups |= POWERUP(PW_NOWEAPON);
	}

	// Setup the carrying weapons
	small  = 0;
	medium = 0;
	large  = 0;

	for(i = 0; i < UT_MAX_WEAPONS; i++)  //UT_MAX_WEAPONS
	{
		int  weaponID = UT_WEAPON_GETID ( ps->weaponData[i] );

		if(!weaponID)
		{
			continue;
		}

		// Dont count the current weapon unless no weapon is specified.
		if(s->weapon == weaponID)
		{
			if (!(s->powerups & POWERUP(PW_NOWEAPON)))
			{
				continue;
			}
		}

		switch (bg_weaponlist[weaponID].type)
		{
			case UT_WPTYPE_SMALL_GUN:
				small = weaponID;
				break;

		  case UT_WPTYPE_SMALL_GUN_DUAL:
			case UT_WPTYPE_MEDIUM_GUN:
				medium = weaponID;
				break;

			case UT_WPTYPE_LARGE_GUN:
				large = weaponID;
				break;

			default:
				continue;
		}
	}

	if(medium && !large)
	{
		large  = medium;
		medium = 0;
	}

	s->time2 = small + (medium << 8) + (large << 16);

	// Tired rate 0 - 100, 100 being most tired
	if (ps->stats[STAT_HEALTH] > 0)
	{
		small = 100 - (ps->stats[STAT_STAMINA] * 100 / (ps->stats[STAT_HEALTH] * UT_STAM_MUL));
	}
	else
	{
		small = 0;
	}

	s->time2    += (small << 24);

	s->loopSound = ps->loopSound;
	s->time      = ps->stats[STAT_HEALTH];

	if (s->time < 0)
	{
		s->time = 0;
	}
	s->time += (ps->stats[STAT_HITS] << 8) + ((ps->viewheight + 128) << 16);
}

/*
========================
BG_PlayerStateToEntityState

This is done after each set of usercmd_t on the server,
and after local prediction on the client
========================
*/
void BG_PlayerStateToEntityState( playerState_t *ps, entityState_t *s, qboolean snap )
{
	if ((ps->pm_type == PM_INTERMISSION) || (ps->pm_type == PM_SPECTATOR) || (ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_GHOST) || (ps->pm_type == PM_DEAD))
	{
		s->eType = ET_INVISIBLE;
	}
	else
	{
		s->eType = ET_PLAYER;
	}

	s->number     = ps->clientNum;

	s->pos.trType = TR_INTERPOLATE;
	VectorCopy( ps->origin, s->pos.trBase );

	if (snap)
	{
		SnapVector( s->pos.trBase );
	}
	// set the trDelta for flag direction
	VectorCopy( ps->velocity, s->pos.trDelta );

	s->apos.trType = TR_INTERPOLATE;
	VectorCopy( ps->viewangles, s->apos.trBase );

	if (snap)
	{
		SnapVector( s->apos.trBase );
	}

	s->angles2[YAW] = ps->movementDir;
	s->legsAnim	= ps->legsAnim;
	s->torsoAnim	= ps->torsoAnim;
	s->clientNum	= ps->clientNum;	// ET_PLAYER looks here instead of at number
	// so corpses can also reference the proper config
	s->eFlags	= ps->eFlags;

	if (ps->stats[STAT_HEALTH] <= 0)
	{
		s->eFlags |= EF_DEAD;
	}
	else
	{
		s->eFlags &= ~EF_DEAD;
	}

	if (ps->externalEvent)
	{
		s->event     = ps->externalEvent;
		s->eventParm = ps->externalEventParm;
	}
	else
	if (ps->entityEventSequence < ps->eventSequence)
	{
		int  seq;

		if (ps->entityEventSequence < ps->eventSequence - MAX_PS_EVENTS)
		{
			ps->entityEventSequence = ps->eventSequence - MAX_PS_EVENTS;
		}
		seq	     = ps->entityEventSequence & (MAX_PS_EVENTS - 1);
		s->event     = ps->events[seq] | ((ps->entityEventSequence & 3) << 8);
		s->eventParm = ps->eventParms[seq];
		ps->entityEventSequence++;
	}


	utPlayerStateToEntityState ( ps, s );
}

/*
========================
BG_PlayerStateToEntityStateExtraPolate

This is done after each set of usercmd_t on the server,
and after local prediction on the client
========================
*/
void BG_PlayerStateToEntityStateExtraPolate( playerState_t *ps, entityState_t *s, int time, qboolean snap )
{
	if ((ps->pm_type == PM_INTERMISSION) || (ps->pm_type == PM_SPECTATOR) || (ps->stats[STAT_UTPMOVE_FLAGS] & UT_PMF_GHOST) || (ps->pm_type == PM_DEAD))
	{
		s->eType = ET_INVISIBLE;
	}
	else
	{
		s->eType = ET_PLAYER;
	}

	s->number     = ps->clientNum;

	s->pos.trType = TR_LINEAR_STOP;
	VectorCopy( ps->origin, s->pos.trBase );

	if (snap)
	{
		SnapVector( s->pos.trBase );
	}
	// set the trDelta for flag direction and linear prediction
	VectorCopy( ps->velocity, s->pos.trDelta );
	// set the time for linear prediction
	s->pos.trTime	  = time;
	// set maximum extra polation time
	s->pos.trDuration = 50; // 1000 / sv_fps (default = 20)

	s->apos.trType	  = TR_INTERPOLATE;
	VectorCopy( ps->viewangles, s->apos.trBase );

	if (snap)
	{
		SnapVector( s->apos.trBase );
	}

	s->angles2[YAW] = ps->movementDir;
	s->legsAnim	= ps->legsAnim;
	s->torsoAnim	= ps->torsoAnim;
	s->clientNum	= ps->clientNum;	// ET_PLAYER looks here instead of at number
	// so corpses can also reference the proper config
	s->eFlags	= ps->eFlags;

	if (ps->stats[STAT_HEALTH] <= 0)
	{
		s->eFlags |= EF_DEAD;
	}
	else
	{
		s->eFlags &= ~EF_DEAD;
	}

	if (ps->externalEvent)
	{
		s->event     = ps->externalEvent;
		s->eventParm = ps->externalEventParm;
	}
	else if (ps->entityEventSequence < ps->eventSequence)
	{
		int  seq;

		if (ps->entityEventSequence < ps->eventSequence - MAX_PS_EVENTS)
		{
			ps->entityEventSequence = ps->eventSequence - MAX_PS_EVENTS;
		}
		seq	     = ps->entityEventSequence & (MAX_PS_EVENTS - 1);
		s->event     = ps->events[seq] | ((ps->entityEventSequence & 3) << 8);
		s->eventParm = ps->eventParms[seq];
		ps->entityEventSequence++;
	}

	utPlayerStateToEntityState ( ps, s );
}

/**
 * $(function)
 */
int utPSGiveWeapon ( playerState_t *ps, weapon_t weaponid, int mode )
{
	int  weaponSlot = 0;

	// Make sure its a valid weapon number
	if ((weaponid <= UT_WP_NONE) || (weaponid >= UT_WP_NUM_WEAPONS))
	{
		return -1;
	}

	if (-1 != (weaponSlot = utPSGetWeaponByID ( ps, weaponid )))
	{
		return -1;
	}

	if (-1 == (weaponSlot = utPSGetWeaponByID ( ps, 0 )))
	{
		return -1;
	}

	if ((mode < 0) || (mode >= MAX_WEAPON_MODES))
	{
		mode = 0;
	}
	else if (!bg_weaponlist[weaponid].modes[mode].name)
	{
		mode = 0;
	}

	UT_WEAPON_SETID ( ps->weaponData[weaponSlot], weaponid );
	UT_WEAPON_SETBULLETS ( ps->weaponData[weaponSlot], bg_weaponlist[weaponid].bullets );
	UT_WEAPON_SETCLIPS ( ps->weaponData[weaponSlot], bg_weaponlist[weaponid].clips );
	UT_WEAPON_SETMODE ( ps->weaponData[weaponSlot], mode );

	return weaponSlot;
}

/**
 * $(function)
 */
int utPSGetWeaponByID ( const playerState_t *ps, weapon_t weaponid )
{
	int  slot;

	for (slot = 0; slot < UT_MAX_WEAPONS; slot++)	//max_weapons
	{
		if (UT_WEAPON_GETID ( ps->weaponData[slot] ) == weaponid)
		{
			return slot;
		}
	}

	return -1;
}

/**
 * $(function)
 */
int utPSGiveItem ( playerState_t *ps, utItemID_t itemid )
{
	int  itemSlot;

	if ((itemid <= UT_ITEM_NONE) || (itemid >= UT_ITEM_MAX))
	{
		return -1;
	}

	if (-1 != utPSGetItemByID ( ps, itemid ))
	{
		return -1;
	}

	if (-1 == (itemSlot = utPSGetItemByID ( ps, 0 )))
	{
		return -1;
	}

	switch (itemid)
	{
		case UT_ITEM_AMMO:
			{
				int  i;

				for (i = 0; i < UT_MAX_WEAPONS; i++) //UT_MAX_WEAPONS
				{
					int  weaponID = UT_WEAPON_GETID( ps->weaponData[i] );
					int  ammo;

					if (weaponID == UT_WP_KNIFE)
					{
						continue;
					}

					if ((weaponID == UT_WP_GRENADE_HE) || (weaponID == UT_WP_GRENADE_FLASH) || (weaponID == UT_WP_GRENADE_SMOKE))
					{
						continue;
					}

					ammo  = UT_WEAPON_GETCLIPS(ps->weaponData[i]);
					ammo += bg_weaponlist[weaponID].clips;

					UT_WEAPON_SETCLIPS(ps->weaponData[i], ammo );
				}
				break;
			}

		default:
			UT_ITEM_SETID( ps->itemData[itemSlot], itemid );
			//UT_ITEM_SETFLAGS ( ps->itemData[itemSlot], (itemid != UT_ITEM_NVG)?UT_ITEMFLAG_ON:0 );
			UT_ITEM_SETFLAGS(ps->itemData[itemSlot], UT_ITEMFLAG_ON);
			break;
	}

	return itemSlot;
}

/**
 * $(function)
 */
void utPSTurnItemOn ( playerState_t *ps, utItemID_t itemid )
{
	int  itemSlot;

	if (-1 == (itemSlot = utPSGetItemByID ( ps, itemid )))
	{
		return;
	}

	UT_ITEM_SETFLAGS ( ps->itemData[itemSlot], UT_ITEM_GETFLAGS(ps->itemData[itemSlot]) | UT_ITEMFLAG_ON );
}

/**
 * $(function)
 */
void utPSTurnItemOff ( playerState_t *ps, utItemID_t itemid )
{
	int  itemSlot;

	if (-1 == (itemSlot = utPSGetItemByID ( ps, itemid )))
	{
		return;
	}

	UT_ITEM_SETFLAGS ( ps->itemData[itemSlot], UT_ITEM_GETFLAGS(ps->itemData[itemSlot]) & (~UT_ITEMFLAG_ON));
}

/**
 * $(function)
 */
int utPSGetItemByID ( const playerState_t *ps, utItemID_t itemid )
{
	int  itemSlot;

	for (itemSlot = 0; itemSlot < UT_MAX_ITEMS; itemSlot++)
	{
		if(UT_ITEM_GETID(ps->itemData[itemSlot]) == itemid)
		{
			return itemSlot;
		}
	}

	return -1;
}

/**
 * $(function)
 */
qboolean utPSHasItem ( const playerState_t *ps, utItemID_t itemid )
{
	// Dead people have NOTHING
	if (ps->stats[STAT_HEALTH] <= 0)
	{
		return qfalse;
	}

	if(-1 == utPSGetItemByID ( ps, itemid ))
	{
		return qfalse;
	}

	return qtrue;
}

/**
 * $(function)
 */
qboolean utPSIsItemOn ( const playerState_t *ps, utItemID_t itemid )
{
	int  itemSlot;

	// Dead people have NOTHING
	if (ps->stats[STAT_HEALTH] <= 0)
	{
		return qfalse;
	}

	if(-1 == (itemSlot = utPSGetItemByID ( ps, itemid )))
	{
		return qfalse;
	}

	return (UT_ITEM_GETFLAGS(ps->itemData[itemSlot]) & UT_ITEMFLAG_ON) ? qtrue : qfalse;
}

/*
===============
BG_PlayerAngles

A modified version of Id's original CG_PlayerAngles
that is designed to work from both game and cgame.

For UT we need to know where the player's torso and
legs and head are so we can use this rotation info
for ARIES.

Author: dokta8

Handles seperate torso motion

  legs pivot based on direction of movement

  head always looks exactly at cent->lerpAngles

  if motion < 20 degrees, show in head only
  if < 45 degrees, also show in torso
===============
*/

/*
BG_PlayerAngles needs:
	legsAnim
	torsoAnim
	velocity
	viewAngles
	frameTime

	CGame needs:
		cent->pe.legs.yawing
		cent->pe.legs.pitching
		cent->pe.torso.yawing
		cent->pe.torso.pitching

	In addition, we need to store:
		cent->pe.legs.yawAngle
		cent->pe.legs.pitchAngle
		cent->pe.torso.yawAngle
		cent->pe.torso.pitchAngle

	BG_PlayerAngles ALSO needs these to
	calculate swing angles, so we need to
	store them somewhere

	Now, finally, we also need to output:
		legsAngles
		torsoAngles
		headAngles

*/

void BG_PlayerAngles(	vec3_t viewAngles,
			vec3_t pVelocity,
			int plegsAnim,
			int ptorsoAnim,
			int frametime,
			angleData_t *adata,
			float *legsPitchAngle,
			float *legsYawAngle,
			float *torsoPitchAngle,
			float *torsoYawAngle,
			int *pryFlags )
{
	vec3_t	legsAngles, torsoAngles, headAngles;
	float	dest;
	vec3_t	velocity;
	float	speed;
	int	legsAnim;
	int	torsoAnim;

	// vars to be turned into bits to be passed back in pryFlags
	qboolean  torsoPitching;
	qboolean  torsoYawing;
	qboolean  legsYawing;

	//frametime = 50;

	legsAnim  = (plegsAnim & ~ANIM_TOGGLEBIT);
	torsoAnim = (ptorsoAnim & ~ANIM_TOGGLEBIT);

	VectorCopy( viewAngles, headAngles );
	headAngles[PITCH] *= 0.5;
	headAngles[YAW]    = AngleMod( headAngles[YAW] );
	VectorClear( legsAngles );
	VectorClear( torsoAngles );

	// --------- yaw -------------

	// allow yaw to drift a bit

	torsoPitching = qfalse;
	torsoYawing   = qfalse;
	legsYawing    = qfalse;

	// Determine if we should be pitching.
	switch (torsoAnim)
	{
		// These are the exclusion animations for pitching.
		case BOTH_LEDGECLIMB:
		case BOTH_CLIMB:
		case BOTH_CLIMB_IDLE:
			break;

		default:

			if (legsAnim != LEGS_IDLE)
			{
				torsoYawing = qtrue;	// always center
				legsYawing  = qtrue;	// always center
			}
			// gsigms: that is when i touched animations and
			// that all hell broke loose
			//if (legsAnim != LEGS_WALKCR
			//&& legsAnim != LEGS_BACKWALKCR
			//&& legsAnim != LEGS_IDLECR) {
				torsoPitching = qtrue;
		//	}
			torsoYawing   = qtrue;
			break;
	}

	torsoAngles[YAW] = headAngles[YAW];
	legsAngles[YAW]  = headAngles[YAW];

	// D8: cg_swingSpeed.value is now useless; locked to the default of 0.3
	BG_SwingAngles( torsoAngles[YAW], 0, 90, 0.3f, torsoYawAngle, &torsoYawing, frametime );
	BG_SwingAngles( legsAngles[YAW], 0, 90, 0.3f, legsYawAngle, &legsYawing, frametime );

	torsoAngles[YAW] = *torsoYawAngle;
	legsAngles[YAW]  = *legsYawAngle;

	switch (legsAnim)
	{
		case LEGS_BACKRUN:
		case LEGS_BACKWALKCR:
		case LEGS_BACKWALK:
		case LEGS_BACKLIMP:
		case LEGS_BACKJUMP:
			{
				vec3_t	vangles;
				int	speed;
				int	diff;
				int	a;
				int	b;

				VectorCopy( pVelocity, velocity ); // D8 - was cent->currentState.pos.trDelta
				vectoangles ( velocity, vangles );

				velocity[2] = 0;
				VectorNormalize( velocity, speed );
				speed	    = speed > 200 ? 200 : speed;

				a	    = 0;
				b	    = (int)((vangles[YAW] + 180) - headAngles[YAW]);

				if (b >= 180)
				{
					b = b - 360;
				}
				else if (b <= -180)
				{
					b = b + 360;
				}

				if (b > 60)
				{
					b = 60;
				}
				else if (b < -60)
				{
					b = -60;
				}

				diff		= b;

				legsAngles[YAW] = LerpAngle ( headAngles[YAW], headAngles[YAW] + diff, speed / 200.0f );

				break;
			}

		case LEGS_RUN:
		case LEGS_WALK:
		case LEGS_LIMP:
		case LEGS_WALKCR:
		case LEGS_JUMP:
			{
				vec3_t	vangles;
				int	speed;
				int	diff;
				int	a;
				int	b;

				VectorCopy( pVelocity, velocity ); // D8 - was cent->currentState.pos.trDelta
				vectoangles ( velocity, vangles );

				velocity[2] = 0;
				VectorNormalize( velocity, speed );
				speed	    = speed > 200 ? 200 : speed;

				a	    = 0;
				b	    = (int)(vangles[YAW] - headAngles[YAW]);

				if (b >= 180)
				{
					b = b - 360;
				}
				else if (b <= -180)
				{
					b = b + 360;
				}

				if (b > 60)
				{
					b = 60;
				}
				else if (b < -60)
				{
					b = -60;
				}

				diff		= b;

				legsAngles[YAW] = LerpAngle ( headAngles[YAW], headAngles[YAW] + diff, speed / 200.0f );

				break;
			}

		default:
			legsAngles[YAW] = *legsYawAngle;
			break;
	}

	// --------- pitch -------------
	{
		// only show a fraction of the pitch angle in the torso
		if (!torsoPitching)
		{
			dest = 0;
		}
		else if (headAngles[PITCH] > 180)
		{
			dest = (-360 + headAngles[PITCH]); // * 0.50f;
		}
		else
		{
			dest = headAngles[PITCH] ; //  * 0.50f;
		}

		if (((legsAnim & (~ANIM_TOGGLEBIT)) == BOTH_CLIMB) ||
		    ((legsAnim & (~ANIM_TOGGLEBIT)) == BOTH_CLIMB_IDLE))
		{
			*torsoPitchAngle = dest + 15;
		}
		else
		{
			*torsoPitchAngle = dest + 5;	// D8: what were we always adding a 5 degree pitch for?
		}
		torsoAngles[PITCH] = *torsoPitchAngle;
	}

	// --------- roll -------------

	// lean towards the direction of travel
	VectorCopy( pVelocity, velocity );
	VectorNormalize( velocity, speed );

	if (speed)
	{
		vec3_t	axis[3];
		float	side;

		speed *= 0.1f;

		AnglesToAxis( legsAngles, axis );
		side = speed * DotProduct( velocity, axis[1] );
		//torsoAngles[ROLL] -= side;
		// torsoAngles[PITCH] += side;
		//torsoAngles[ROLL] += side;
		//side = speed * DotProduct( velocity, axis[0] );
		//	legsAngles[PITCH] += side;
	}

	// pain twitch
	// D8 - remove pain twitch for now - maybe implement
	// client side only?
	// CG_AddPainTwitch( cent, torsoAngles );

	// return pitching booleans as bits in pryFlags
	// this is only used by cdata to make cent->pe.torso.pitching etc.
	if (pryFlags)
	{
		*pryFlags = (torsoPitching) + (torsoYawing >> 1) + (legsYawing >> 2);
	}

	AnglesSubtract( headAngles, torsoAngles, headAngles );
	AnglesSubtract( torsoAngles, legsAngles, torsoAngles );

//makes the players head not so wanky angled
	// headAngles[2]-=15;
	// headAngles[0]-=10;
	VectorCopy( headAngles, adata->headAngles );
	VectorCopy( torsoAngles, adata->torsoAngles );
	VectorCopy( legsAngles, adata->legAngles );
}

/*
==================
BG_SwingAngles
==================
*/
void BG_SwingAngles( float destination, float swingTolerance, float clampTolerance, float speed, float *angle, qboolean *swinging, int frametime )
{
	float  swing;
	float  move;
	float  scale;

	if (!*swinging)
	{
		// see if a swing should be started
		AngleSubtract( *angle, destination, swing );

		if ((swing > swingTolerance) || (swing < -swingTolerance))
		{
			*swinging = qtrue;
		}
	}

	if (!*swinging)
	{
		return;
	}

	// modify the speed depending on the delta
	// so it doesn't seem so linear
	AngleSubtract( destination, *angle, swing );
	scale = fabs( swing );

	if (scale < swingTolerance * 0.5)
	{
		scale = 0.5;
	}
	else if (scale < swingTolerance)
	{
		scale = 1.0;
	}
	else
	{
		scale = 2.0;
	}

	// swing towards the destination angle
	if (swing >= 0)
	{
		move = frametime * scale * speed;

		if (move >= swing)
		{
			move	  = swing;
			*swinging = qfalse;
		}
		*angle = AngleMod( *angle + move );
	}
	else if (swing < 0)
	{
		move = frametime * scale * -speed;

		if (move <= swing)
		{
			move	  = swing;
			*swinging = qfalse;
		}
		*angle = AngleMod( *angle + move );
	}

	// clamp to no more than tolerance
	AngleSubtract( destination, *angle, swing );

	if (swing > clampTolerance)
	{
		*angle = AngleMod( destination - (clampTolerance - 1));
	}
	else if (swing < -clampTolerance)
	{
		*angle = AngleMod( destination + (clampTolerance - 1));
	}
}


unsigned int ut_weapon_xor = 0;
#ifdef USE_AUTH
/////////////////////////////////////////////////
// These SERVERINFO keys will be hashed
// IMPORTANT: These must be all CVAR_SERVERINFO and either CVAR_ROM or CVAR_LATCH (must never change during game)
char *cse_si_keys[] = {"g_needpass",
                       "version",
                       "mapname",
                       "g_modversion",
                       "g_gametype",
                       "sv_maxclients",
                       "g_matchMode"};

void BG_RandomInit(const char *serverinfo)
{
 int i;
 unsigned int rnd;
 const char *x;
 char tmp[32];

 rnd=1337001337;
 // Hash the server info vars
 for(i=0;i<sizeof(cse_si_keys)/sizeof(cse_si_keys[0]);i++) {
  strcpy(cse_si_keys[i],tmp);
  x = Info_ValueForKey(serverinfo,tmp);
  if (!x) continue;
  while(*x) {
   rnd+=*(x++);
   rnd^=rnd<<13;
   rnd^=rnd>>3;
   rnd^=rnd<<27;
  }
 }
 ut_weapon_xor = rnd^0xBEC99226;

#ifdef DYNEVENTS
// cse_enc_tab[0]=0; // event zero is "empty" flag, must stay zero
// cse_dec_tab[0]=0; // not needed - arrays initialized to zero

 for(i=1;i<256;i++) {
  int j;
  while(1) {
   rnd^=rnd<<13;
   rnd^=rnd>>3;
   rnd^=rnd<<27;
   j = rnd&0xFF;
   if (j && !cse_enc_tab[j]) {
    cse_enc_tab[j] = i;
    cse_dec_tab[i] = j;
    break;
   }
  }
 }
#endif
}
#endif

#ifdef DYNEVENTS
//@r00t: Dynamic event mapping - tables for encoding and decoding event numbers
unsigned char cse_enc_tab[256] = {0};
unsigned char cse_dec_tab[256] = {0};

int CSE_ENC(int x)
{
 return cse_enc_tab[x];
}

int CSE_DEC(int x)
{
 return cse_dec_tab[x];
}
#endif
