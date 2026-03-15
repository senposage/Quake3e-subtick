/**
 * Filename: c_weapons.c
 *
 * Copyright (C) 1999-2005 Id Software, Inc.
 * Copyright (C) 2000-2009 FrozenSand
 *
 * This file is part of Urban Terror source code.
 *
 */

#include "c_weapons.h"
#include "../game/inv.h"
#include "../game/bg_public.h"

weaponData_t bg_weaponlist[] = {
	/* No weapon */
	{ "None" },

	/* Knife */
	{
		"Knife",
		"knife",
		UT_WPFLAG_NOFLASH | UT_WPFLAG_REMOVEONEMPTY,
		UT_WPTYPE_MELEE,
		0,

		INVENTORY_KNIFE,
		INVENTORY_KNIFE_AMMO,

		0,    /* clips */
		5,    /* bullets */
		400,  /* reload time */
		0,    /* reload start time */
		0,    /* reload finish time */

		/* damage */
		{
        //  {  dmg , bleed  }
            { 1.00f, qfalse }, /* HL_UNKNOWN */
            { 1.00f, qfalse }, /* HL_HEAD    */
            { 0.60f, qfalse }, /* HL_HELMET  */
            { 0.44f, qtrue  }, /* HL_TORSO   */
            { 0.35f, qfalse }, /* HL_VEST    */
            { 0.20f, qtrue  }, /* HL_ARML    */
            { 0.20f, qtrue  }, /* HL_ARMR    */
            { 0.40f, qtrue  }, /* HL_GROIN   */
            { 0.37f, qtrue  }, /* HL_BUTT    */
            { 0.20f, qtrue  }, /* HL_LEGUL   */
            { 0.20f, qtrue  }, /* HL_LEGUR   */
            { 0.18f, qtrue  }, /* HL_LEGLL   */
            { 0.18f, qtrue  }, /* HL_LEGLR   */
            { 0.15f, qtrue  }, /* HL_FOOTL   */
            { 0.15f, qtrue  }, /* HL_FOOTR   */
		},

		10000,	/* effective range */
		20.0f,	/* knockback */

		UT_WP_KNIFE,
		UT_MOD_KNIFE,

		0,	/* Delay for brass eject */

		0,	/* Brass length scale */
		0,	/* Brass diameter scale */

		0,	/* Silencer world scale */
		0,	/* Silencer view scale */

		1.0f,	/* Flash Scale */

		{      0 },

		{
			{
				"Slash",		  /* name */
				UT_WPMODEFLAG_NOAMMO |
				UT_WPMODEFLAG_NORECOIL |
				UT_WPMODEFLAG_NOHOLD,	  /* flags */

				250,			  /* fire time */
				1148,			  /* change time */
			},

			{
				"Throw",
				UT_WPMODEFLAG_ALTANIMS |
				UT_WPMODEFLAG_HOLDTOREADY |
				UT_WPMODEFLAG_NORECOIL,   /* flags */
				200,			  /* fire time */
				1148,			  /* change time */
				1400,			  /* spread? */
			},

			{ NULL	 }
		},
	},

	/* Beretta */
	{
		"Beretta 92G",
		"beretta",
		0,
		UT_WPTYPE_SMALL_GUN,
		POWERUP(PW_SILENCER) | POWERUP(PW_LASER),

		INVENTORY_BERETTA,
		INVENTORY_BERETTA_AMMO,

		2,    /* Clips */
		15,   /* Shots per clip */
		1640, /* reload time */
		0,    /* reload start time */
		0,    /* reload finish time */

		/* damage */
		{
        //  {  dmg , bleed  }
            { 0.58f, qtrue  },  /* HL_UNKNOWN */
            { 1.00f, qfalse },  /* HL_HEAD    */
            { 0.34f, qfalse },  /* HL_HELMET  */
            { 0.30f, qtrue  },  /* HL_TORSO   */
            { 0.20f, qfalse },  /* HL_VEST    */
            { 0.11f, qtrue  },  /* HL_ARML    */
            { 0.11f, qtrue  },  /* HL_ARMR    */
            { 0.25f, qtrue  },  /* HL_GROIN   */
            { 0.22f, qtrue  },  /* HL_BUTT    */
            { 0.15f, qtrue  },  /* HL_LEGUL   */
            { 0.15f, qtrue  },  /* HL_LEGUR   */
            { 0.13f, qtrue  },  /* HL_LEGLL   */
            { 0.13f, qtrue  },  /* HL_LEGLR   */
            { 0.11f, qtrue  },  /* HL_FOOTL   */
            { 0.11f, qtrue  },  /* HL_FOOTR   */
		},

		700,		/* effective range */
		15.0f,		/* knockback */

		UT_WP_BERETTA,
		UT_MOD_BERETTA, /* Means of death */

		0,		/* Delay for brass eject */

		0.5f,		/* Brass length scale */
		1.0f,		/* Brass diameter scale */

		0.27f,		/* Silencer world scale */
		0.75f,		/* Silencer view scale */

		0.35f,		/* Flash Scale */

		{      0 },

		{
			{
				"Semi-Automatic",	  /* Name */
				UT_WPMODEFLAG_NOHOLD,
				120,			  /* fire time */
				0,
				0.7f,			  /* spread */
				0.05f,			  /* movement */
				{
					0,
					50,
					3.0f,
					10.0f,
					1.0f,
				}
			},

			{ NULL	 }
		},
	},

	/* Desert Eagle */
	{
		"IMI DE .50 AE",
		"deserteagle",
		0,
		UT_WPTYPE_SMALL_GUN,
		POWERUP(PW_LASER),

		INVENTORY_DEAGLE,
		INVENTORY_DEAGLE_AMMO,

		2,    /* Clips */
		7,    /* Shots per clip */
		3220, /* reload time */
		0,    /* reload start time */
		0,    /* reload finish time */

		/* damage */
		{
        //  {  dmg , bleed  }
            { 1.10f, qtrue  },  /* HL_UNKNOWN */
            { 1.00f, qfalse },  /* HL_HEAD    */
            { 0.66f, qfalse },  /* HL_HELMET  */
            { 0.57f, qtrue  },  /* HL_TORSO   */
            { 0.38f, qfalse },  /* HL_VEST    */
            { 0.22f, qtrue  },  /* HL_ARML    */
            { 0.22f, qtrue  },  /* HL_ARMR    */
            { 0.45f, qtrue  },  /* HL_GROIN   */
            { 0.41f, qtrue  },  /* HL_BUTT    */
            { 0.28f, qtrue  },  /* HL_LEGUL   */
            { 0.28f, qtrue  },  /* HL_LEGUR   */
            { 0.22f, qtrue  },  /* HL_LEGLL   */
            { 0.22f, qtrue  },  /* HL_LEGLR   */
            { 0.18f, qtrue  },  /* HL_FOOTL   */
            { 0.18f, qtrue  },  /* HL_FOOTR   */
		},

		600,		/* effective range */
		100.0f, 	/* knockback */

		UT_WP_DEAGLE,
		UT_MOD_DEAGLE,	/* Means of death */

		0,		/* Delay for brass eject */

		0.5f,		/* Brass length scale */
		1.5f,		/* Brass diameter scale */

		0.27f,		/* Silencer world scale */
		0.75f,		/* Silencer view scale */

		0.4f,	/* Flash Scale */

		{      0 },

		{
			{
				"Semi-Automatic",	 /* Name */
				UT_WPMODEFLAG_NOHOLD,
				120,			 /* fire time */
				0,
				0.8f,			 /* spread */
				0.05f,			 /* movement */
				{
					0,
					20,
					20.0f,
					12.0f,
					1.0f,
				}
			},

			{ NULL	 }
		},
	},

	/* SPAS-12 */
	{
		"Franchi SPAS12",
		"spas12",
		UT_WPFLAG_SINGLEBULLETCLIP,
		UT_WPTYPE_MEDIUM_GUN,
		0,

		INVENTORY_SPAS12,
		INVENTORY_SPAS12_AMMO,

		24,
		8,
		533,  /* reload time */
		533,  /* reload start time */
		866,  /* reload finish time */

		/* damage */
		{
        //  {  dmg , bleed  }
            { 0.06f, qtrue  },  /* HL_UNKNOWN */
            { 0.06f, qtrue  },  /* HL_HEAD    */
            { 0.04f, qfalse },  /* HL_HELMET  */
            { 0.04f, qtrue  },  /* HL_TORSO   */
            { 0.02f, qfalse },  /* HL_VEST    */
            { 0.02f, qtrue  },  /* HL_ARML    */
            { 0.02f, qtrue  },  /* HL_ARMR    */
            { 0.03f, qtrue  },  /* HL_GROIN   */
            { 0.03f, qtrue  },  /* HL_BUTT    */
            { 0.02f, qtrue  },  /* HL_LEGUL   */
            { 0.02f, qtrue  },  /* HL_LEGUR   */
            { 0.02f, qtrue  },  /* HL_LEGLL   */
            { 0.02f, qtrue  },  /* HL_LEGLR   */
            { 0.02f, qtrue  },  /* HL_FOOTL   */
            { 0.02f, qtrue  },  /* HL_FOOTR   */

		},

		300,		/* effective range */
		5.0f,		/* knockback */

		UT_WP_SPAS12,
		UT_MOD_SPAS,

		0,		/* Delay for brass eject */

		0.75f,		/* Brass length scale */
		0.75f,		/* Brass diameter scale */

		0,		/* Silencer world scale */
		0,		/* Silencer view scale */

		1.25f,		/* Flash Scale */

		{      0 },

		{
			{
				"Semi-Automatic",	 /* Name */
				UT_WPMODEFLAG_NOHOLD,	 /* Flags */
				350,			 /* fire time */
				0,
				5.0f,			 /* spread */
				0.0f,			 /* movement */
				{
					0,
					15,
					42.5f,
					2.0f,
					1.0f,
				},
			},

			{ NULL	 },
		},
	},

	/* MP5k */
	{
		"HK MP5K",
		"mp5k",
		0,
		UT_WPTYPE_MEDIUM_GUN,
		POWERUP(PW_SILENCER) | POWERUP(PW_LASER),

		INVENTORY_MP5K,
		INVENTORY_MP5K_AMMO,

		2,    /* Clips */
		30,   /* Bullets */
		2460, /* reload time */
		0,    /* reload start time */
		0,    /* reload finish time */

		/* damage */
		{
        //  {  dmg , bleed  }
            { 0.58f, qtrue  },  /* HL_UNKNOWN */
            { 0.50f, qfalse },  /* HL_HEAD    */
            { 0.34f, qfalse },  /* HL_HELMET  */
            { 0.30f, qtrue  },  /* HL_TORSO   */
            { 0.20f, qfalse },  /* HL_VEST    */
            { 0.11f, qtrue  },  /* HL_ARML    */
            { 0.11f, qtrue  },  /* HL_ARMR    */
            { 0.25f, qtrue  },  /* HL_GROIN   */
            { 0.22f, qtrue  },  /* HL_BUTT    */
            { 0.15f, qtrue  },  /* HL_LEGUL   */
            { 0.15f, qtrue  },  /* HL_LEGUR   */
            { 0.13f, qtrue  },  /* HL_LEGLL   */
            { 0.13f, qtrue  },  /* HL_LEGLR   */
            { 0.11f, qtrue  },  /* HL_FOOTL   */
            { 0.11f, qtrue  },  /* HL_FOOTR   */
		},

		650,		/* effective range */
		15.0f,		/* knockback */

		UT_WP_MP5K,
		UT_MOD_MP5K,	/* Means of death */

		0,		/* Delay for brass eject */

		0.5f,		/* Brass length scale */
		1.0f,		/* Brass diameter scale */

		0.30f,		/* Silencer world scale */
		0.75f,		/* Silencer view scale */

		0.5f,		/* Flash Scale */

		{      0 },    /* Zoom levels */

		{
			{
				"Burst",		 /* Name */
				UT_WPMODEFLAG_NOHOLD,	 /* Flags */
				100,			 /* fire time */
				0,
				1.1f,			 /* spread */
				0.275f, 		 /* movement */
				{
					130,
					100,
					1.5f,
					3.5f,
					1.0f,
				},
				100,
				3,
			},

			{
				"Automatic",		 /* Name */
				0,			 /* Flags */
				100,			 /* fire time */
				0,
				1.1f,			 /* spread */
				0.275f, 		 /* movement */
				{
					130,
					100,
					1.5f,
					3.25f,
					1.0f,
				}
			},

			{ NULL	 }
		},
	},

	/* UMP45 */
	{
		"HK UMP45",					/* Name */
		"ump45",
		0,						/* Flags */
		UT_WPTYPE_MEDIUM_GUN,				/* Type */
		POWERUP(PW_SILENCER) | POWERUP(PW_LASER),	/* Available powerups */

		INVENTORY_UMP45,
		INVENTORY_UMP45_AMMO,

		2,    /* Clips */
		30,   /* Bullets */
		2650, /* reload time */
		0,    /* reload start time */
		0,    /* reload finish time */

		/* damage */
		{
        //  {  dmg , bleed  }
            { 0.75f, qtrue  },  /* HL_UNKNOWN */
            { 1.00f, qfalse },  /* HL_HEAD    */
            { 0.51f, qfalse },  /* HL_HELMET  */
            { 0.44f, qtrue  },  /* HL_TORSO   */
            { 0.29f, qfalse },  /* HL_VEST    */
            { 0.17f, qtrue  },  /* HL_ARML    */
            { 0.17f, qtrue  },  /* HL_ARMR    */
            { 0.36f, qtrue  },  /* HL_GROIN   */
            { 0.32f, qtrue  },  /* HL_BUTT    */
            { 0.21f, qtrue  },  /* HL_LEGUL   */
            { 0.21f, qtrue  },  /* HL_LEGUR   */
            { 0.17f, qtrue  },  /* HL_LEGLL   */
            { 0.17f, qtrue  },  /* HL_LEGLR   */
            { 0.14f, qtrue  },  /* HL_FOOTL   */
            { 0.14f, qtrue  },  /* HL_FOOTR   */
		},

		900,		/* effective range */
		40.0f,		/* knockback */

		UT_WP_UMP45,
		UT_MOD_UMP45,	/* Means of death */

		0,		/* Delay for brass eject */

		0.5f,		/* Brass length scale */
		1.5f,		/* Brass diameter scale */

		0.4f,		/* Silencer world scale */
		1,		/* Silencer view scale */

		0.6f,		/* Flash Scale */

		{      0 },	/* Zoom levels */

		{
			{
				"Spam", 		 /* Name */
				UT_WPMODEFLAG_NOHOLD,	 /* Flags */
				500,			 /* fire time */
				0,
				1.0f,			 /* spread */
				0.3f,			 /* movement */
				{
					180,
					80,
					8.0f,
					4.0f,
					1.0f,
				},
				50,
				3,
			},

			{
				"Automatic",		 /* Name */
				0,			 /* Flags */
				150,			 /* fire time */
				0,
				1.0f,			 /* spread */
				0.3f,			 /* movement */
				{
					140,
					30,
					5.0f,
					3.0f,
					1.0f,
				}
			},

			{ NULL	 }
		},
	},

	/* HK-69 */
	{
		"HK69 40mm",
		"hk69",
		UT_WPFLAG_SINGLEBULLETCLIP | UT_WPFLAG_NOFLASH,
		UT_WPTYPE_LARGE_GUN,
		0,

		INVENTORY_HK69,
		INVENTORY_HK69_AMMO,

		3,    /* Clips */
		1,    /* Bullets */
		2400, /* reload time */
		0,    /* reload start time */
		0,    /* reload finish time */

		/* damage */
		{
        //  {  dmg , bleed  }
            { 1.80f, qtrue  },  /* HL_UNKNOWN */
            { 0.25f, qtrue  },  /* HL_HEAD    */
            { 0.25f, qfalse },  /* HL_HELMET  */
            { 0.25f, qtrue  },  /* HL_TORSO   */
            { 0.25f, qfalse },  /* HL_VEST    */
            { 0.25f, qtrue  },  /* HL_ARML    */
            { 0.25f, qtrue  },  /* HL_ARMR    */
            { 0.25f, qtrue  },  /* HL_GROIN   */
            { 0.25f, qtrue  },  /* HL_BUTT    */
            { 0.25f, qtrue  },  /* HL_LEGUL   */
            { 0.25f, qtrue  },  /* HL_LEGUR   */
            { 0.25f, qtrue  },  /* HL_LEGLL   */
            { 0.25f, qtrue  },  /* HL_LEGLR   */
            { 0.25f, qtrue  },  /* HL_FOOTL   */
            { 0.25f, qtrue  },  /* HL_FOOTR   */
		},


		UT_MAX_DISTANCE,	  /* effective range */
		200.0f, 	/* knockback */

		UT_WP_HK69,
		UT_MOD_HK69,

		0,		/* Delay for brass eject */

		1,		/* Brass length scale */
		1,		/* Brass diameter scale */

		0,		/* Silencer world scale */
		0,		/* Silencer view scale */

		1.0f,	/* Flash Scale */

		{      0 },

		{
			{
				"Short Range",		 /* Name */
				UT_WPMODEFLAG_NOHOLD |
				UT_WPMODEFLAG_NORECOIL,  /* Flags */
				50,			 /* fire time */
				769,
				0,			 /* spread */
				0,			 /* movement */
				{
					0, 0, 0, 1100
				}
			},

			{
				"Long Range",		 /* Name */
				UT_WPMODEFLAG_NOHOLD |
				UT_WPMODEFLAG_ALTANIMS |
				UT_WPMODEFLAG_NORECOIL,  /* Flags */
				50,			 /* fire time */
				769,
				0,			 /* spread */
				0,			 /* movement */
				{
					0, 0, 0, 1500
				}
			},

			{ NULL	 }
		},
	},

	/* M4 aka ZM LR300 */
	{
		"ZM LR300", /* M4 */
		"zm300",
		0,
		UT_WPTYPE_LARGE_GUN,
		POWERUP(PW_SILENCER) | POWERUP(PW_LASER),

		INVENTORY_LR,
		INVENTORY_LR_AMMO,

		2,
		30,
		2450, /* reload time */
		0,    /* reload start time */
		0,    /* reload finish time */

		/* damage */
		{
        //  {  dmg , bleed  }
            { 0.85f, qtrue  },  /* HL_UNKNOWN */
            { 1.00f, qfalse },  /* HL_HEAD    */
            { 0.51f, qfalse },  /* HL_HELMET  */
            { 0.44f, qtrue  },  /* HL_TORSO   */
            { 0.29f, qfalse },  /* HL_VEST    */
            { 0.17f, qtrue  },  /* HL_ARML    */
            { 0.17f, qtrue  },  /* HL_ARMR    */
            { 0.37f, qtrue  },  /* HL_GROIN   */
            { 0.33f, qtrue  },  /* HL_BUTT    */
            { 0.20f, qtrue  },  /* HL_LEGUL   */
            { 0.20f, qtrue  },  /* HL_LEGUR   */
            { 0.17f, qtrue  },  /* HL_LEGLL   */
            { 0.17f, qtrue  },  /* HL_LEGLR   */
            { 0.14f, qtrue  },  /* HL_FOOTL   */
            { 0.14f, qtrue  },  /* HL_FOOTR   */
		},

		1600,		/* effective range */
		25.0f,		/* knockback */

		UT_WP_LR,
		UT_MOD_LR,

		0,		/* Delay for brass eject */

		1,		/* Brass length scale */
		1,		/* Brass diameter scale */

		0.4f,		/* Silencer world scale */
		1,		/* Silencer view scale */

		1.0f,		/* Flash Scale */

		{      0 },

		{
			{
				"Burst",		 /* Name */
				UT_WPMODEFLAG_NOHOLD,	 /* Flags */
				120,			 /* fire time */
				0,
				0.5f,			 /* spread */
				0.2f,			 /* movement */
				{
					160,
					50,
					8.0f,
					7.0f,
					1.0f,
				},
				120,			 /* Burst time */
				3,			 /* Burst shot count */
			},

			{
				"Semi-Automatic",	 /* Name */
				UT_WPMODEFLAG_NOHOLD,	 /* Flags */
				120,			 /* fire time */
				0,
				0.5f,			 /* spread */
				0.2f,			 /* movement */
				{
					160,
					50,
					8.0f,
					7.0f,
					1.0f,
				}
			},

			{
				"Automatic",		 /* Name */
				0,			 /* Flags */
				120,			 /* fire time */
				0,
				0.5f,			 /* spread */
				0.2f,			 /* movement */
				{
					160,
					50,
					8.0f,
					7.0f,
					1.0f,
				}
			},

			{ NULL	 }
		},
	},

	/* G36 */
	{
		"HK G36",		     /* Name */
		"g36",
		0,			     /* Flags */
		UT_WPTYPE_LARGE_GUN,	     /* Weapon type */
		POWERUP(PW_SILENCER),	     /* Available powerups */

		INVENTORY_G36,
		INVENTORY_G36_AMMO,

		2,    /* Clips */
		30,   /* Bullets per clip */
		2380, /* reload time */
		0,    /* reload start time */
		0,    /* reload finish time */

		/* damage */
		{
        //  {  dmg , bleed  }
            { 0.80f, qtrue  },  /* HL_UNKNOWN */
            { 1.00f, qfalse },  /* HL_HEAD    */
            { 0.51f, qfalse },  /* HL_HELMET  */
            { 0.44f, qtrue  },  /* HL_TORSO   */
            { 0.29f, qfalse },  /* HL_VEST    */
            { 0.17f, qtrue  },  /* HL_ARML    */
            { 0.17f, qtrue  },  /* HL_ARMR    */
            { 0.37f, qtrue  },  /* HL_GROIN   */
            { 0.33f, qtrue  },  /* HL_BUTT    */
            { 0.20f, qtrue  },  /* HL_LEGUL   */
            { 0.20f, qtrue  },  /* HL_LEGUR   */
            { 0.17f, qtrue  },  /* HL_LEGLL   */
            { 0.17f, qtrue  },  /* HL_LEGLR   */
            { 0.14f, qtrue  },  /* HL_FOOTL   */
            { 0.14f, qtrue  },  /* HL_FOOTR   */
		},

		UT_MAX_DISTANCE, /* effective range */
		20.0f,		 /* knockback */

		UT_WP_G36,
		UT_MOD_G36,

		0,		/* Delay for brass eject */

		1,		/* Brass length scale */
		1,		/* Brass diameter scale */

		0.4f,		/* Silencer world scale */
		1,		/* Silencer view scale */

		1.0f,		/* Flash Scale */

		{     40 },

		{
			{
				"Burst",		 /* Name */
				UT_WPMODEFLAG_NOHOLD,	 /* Flags */
				138,			 /* fire time */
				0,
				0.075f, 		 /* spread */
				0.1625f,		 /* movement */
				{
					175,		 /* delay */
					23,		 /* fall */
					2.5f,		 /* rise */
					3.0f,		 /* spread */
					2.0f,		 /* rate */
				},
				130,			 /* Burst time */
				3,			 /* Burst rounds */
			},

			{
				"Semi-Automatic",	 /* Name */
				UT_WPMODEFLAG_NOHOLD,	 /* Flags */
				138,			 /* fire time */
				0,
				0.075f, 		 /* spread */
				0.1625f,		 /* movement */
				{
					175,
					23,
					2.5f,
					3.0f,
					2.0f,
				}
			},

			{
				"Automatic",		  /* Name */
				0,			  /* Flags */
				138,			  /* fire time */
				0,
				0.075f, 		  /* spread */
				0.1625f,		  /* movement */
				{
					175,
					23,
					2.5f,
					3.0f,
					2.0f,
				},
			},

			{ NULL	 }
		},
	},

	/* PSG1 */
	{
		"HK PSG1",
		"psg1",
		UT_WPFLAG_PENETRATE_HELMET | UT_WPFLAG_NOCROSSHAIR,
		UT_WPTYPE_LARGE_GUN,
		POWERUP(PW_SILENCER),

		INVENTORY_PSG1,
		INVENTORY_PSG1_AMMO,

		3,
		8,    /* 8 bullets in the clip.. not 5 */
		3000, /* reload time */
		0,    /* reload start time */
		0,    /* reload finish time */

		/* damage */
		{
        //  {  dmg , bleed  }
            { 1.80f, qtrue  },  /* HL_UNKNOWN */
            { 1.00f, qfalse },  /* HL_HEAD    */
            { 1.00f, qfalse },  /* HL_HELMET  */
            { 0.97f, qtrue  },  /* HL_TORSO   */
            { 0.63f, qfalse },  /* HL_VEST    */
            { 0.36f, qtrue  },  /* HL_ARML    */
            { 0.36f, qtrue  },  /* HL_ARMR    */
            { 0.70f, qtrue  },  /* HL_GROIN   */
            { 0.70f, qtrue  },  /* HL_BUTT    */
            { 0.41f, qtrue  },  /* HL_LEGUL   */
            { 0.41f, qtrue  },  /* HL_LEGUR   */
            { 0.36f, qtrue  },  /* HL_LEGLL   */
            { 0.36f, qtrue  },  /* HL_LEGLR   */
            { 0.29f, qtrue  },  /* HL_FOOTL   */
            { 0.29f, qtrue  },  /* HL_FOOTR   */
		},

		UT_MAX_DISTANCE, /* effective range */
		100.0f, 	 /* knockback */

		UT_WP_PSG1,
		UT_MOD_PSG1,

		0,		/* Delay for brass eject */

		1.5f,		/* Brass length scale */
		2.0f,		/* Brass diameter scale */

		0.4f,		/* Silencer world scale */
		1,		/* Silencer view scale */

		1.0,		/* Flash Scale */

		{ 28, 15, 8},

		{
			{
				"Semi-Automatic",	  /* Name */
				UT_WPMODEFLAG_NOHOLD,	  /* Flags */
				500,			  /* fire time */
				0,
				0.0f,			   /* spread */
				0.5f,			   /* movement */
				{
					50,
					10,
					6.0f,
					7.0f,
					1.0f,
				}

			},

			{ NULL	 }
		},
	},

	/* Concussion Grenade */
	{
		"HE Grenade",
		"grenade",
		UT_WPFLAG_NOFLASH | UT_WPFLAG_REMOVEONEMPTY,
		UT_WPTYPE_THROW,
		0,

		INVENTORY_GRENADE_HE,
		INVENTORY_GRENADE_HE_AMMO,

		0,    /* clips */
		2,    /* bullets */
		860,
		0,    /* reload start time */
		0,    /* reload finish time */

		/* damage */
		{
        //  {  dmg , bleed  }
            { 1.00f, qfalse },  /* HL_UNKNOWN */
            { 1.00f, qfalse },  /* HL_HEAD    */
            { 1.00f, qfalse },  /* HL_HELMET  */
            { 1.00f, qfalse },  /* HL_TORSO   */
            { 1.00f, qfalse },  /* HL_VEST    */
            { 1.00f, qfalse },  /* HL_ARML    */
            { 1.00f, qfalse },  /* HL_ARMR    */
            { 1.00f, qfalse },  /* HL_GROIN   */
            { 1.00f, qfalse },  /* HL_BUTT    */
            { 1.00f, qfalse },  /* HL_LEGUL   */
            { 1.00f, qfalse },  /* HL_LEGUR   */
            { 1.00f, qfalse },  /* HL_LEGLL   */
            { 1.00f, qfalse },  /* HL_LEGLR   */
            { 1.00f, qfalse },  /* HL_FOOTL   */
            { 1.00f, qfalse },  /* HL_FOOTR   */
		},

		UT_MAX_DISTANCE, /* effective range */
		200.0f, 	 /* knockback */

		UT_WP_GRENADE_HE,
		UT_MOD_HEGRENADE,

		0,		/* Delay for brass eject */

		0,		/* Brass length scale */
		0,		/* Brass diameter scale */

		0,		/* Silencer world scale */
		0,		/* Silencer view scale */

		1.0f,		/* Flash Scale */

		{      0 },

		{
			/*
			{
				"Arm on Release",
				UT_WPMODEFLAG_HOLDTOREADY |
				UT_WPMODEFLAG_NORECOIL,
				400,
				0,
				0,
				0,
				{
					0,0,0,800
				}
			},
			*/

			{
				"Instant Arm",
				UT_WPMODEFLAG_HOLDTOREADY |
				UT_WPMODEFLAG_NORECOIL,
				400,
				0,
				0,
				0,
				{
					0, 0, 0, 800
				}
			},

			{ NULL	 }
		},
	},

	/* Flash Grenade */
	{
		"Flash Grenade",
		"grenade",
		UT_WPFLAG_NOFLASH | UT_WPFLAG_REMOVEONEMPTY,
		UT_WPTYPE_THROW,
		0,

		INVENTORY_GRENADE_FLASH,
		INVENTORY_GRENADE_FLASH_AMMO,

		0,    /* clips */
		2,    /* bullets */
		860,
		0,    /* reload start time */
		0,    /* reload finish time */

		/* damage */
		{
        //  {  dmg , bleed  }
            { 0.05f, qfalse },  /* HL_UNKNOWN */
            { 0.00f, qfalse },  /* HL_HEAD    */
            { 0.00f, qfalse },  /* HL_HELMET  */
            { 0.00f, qfalse },  /* HL_TORSO   */
            { 0.00f, qfalse },  /* HL_VEST    */
            { 0.00f, qfalse },  /* HL_ARML    */
            { 0.00f, qfalse },  /* HL_ARMR    */
            { 0.00f, qfalse },  /* HL_GROIN   */
            { 0.00f, qfalse },  /* HL_BUTT    */
            { 0.00f, qfalse },  /* HL_LEGUL   */
            { 0.00f, qfalse },  /* HL_LEGUR   */
            { 0.00f, qfalse },  /* HL_LEGLL   */
            { 0.00f, qfalse },  /* HL_LEGLR   */
            { 0.00f, qfalse },  /* HL_FOOTL   */
            { 0.00f, qfalse },  /* HL_FOOTR   */
		},

		UT_MAX_DISTANCE, /* effective range */
		20.0f,		 /* knockback */

		UT_WP_GRENADE_FLASH,
		UT_MOD_FLASHGRENADE,

		0,		/* Delay for brass eject */

		0,		/* Brass length scale */
		0,		/* Brass diameter scale */

		0,		/* Silencer world scale */
		0,		/* Silencer view scale */

		1.0f,	/* Flash Scale */

		{      0 },

		{
			/*
			{
				"Arm on Release",
				UT_WPMODEFLAG_HOLDTOREADY |
				UT_WPMODEFLAG_NORECOIL,
				400,
				0,
				0,
				0,
				{
					0,0,0,800
				}
			},
			*/

			{
				"Instant Arm",
				UT_WPMODEFLAG_HOLDTOREADY |
				UT_WPMODEFLAG_NORECOIL,
				400,
				0,
				0,
				0,
				{
					0, 0, 0, 800
				}
			},

			{ NULL	 }
		},
	},

	/* Smoke Grenade */
	{
		"Smoke Grenade",
		"grenade",
		UT_WPFLAG_NOFLASH | UT_WPFLAG_REMOVEONEMPTY,
		UT_WPTYPE_THROW,
		0,

		INVENTORY_GRENADE_SMOKE,
		INVENTORY_GRENADE_SMOKE_AMMO,

		0,		/* clips */
		2,		/* bullets */
		860,
		0,		/* reload start time */
		0,		/* reload finish time */

		/* damage */
		{
        //  {  dmg , bleed  }
            { 0.05f, qfalse },  /* HL_UNKNOWN */
            { 0.00f, qfalse },  /* HL_HEAD    */
            { 0.00f, qfalse },  /* HL_HELMET  */
            { 0.00f, qfalse },  /* HL_TORSO   */
            { 0.00f, qfalse },  /* HL_VEST    */
            { 0.00f, qfalse },  /* HL_ARML    */
            { 0.00f, qfalse },  /* HL_ARMR    */
            { 0.00f, qfalse },  /* HL_GROIN   */
            { 0.00f, qfalse },  /* HL_BUTT    */
            { 0.00f, qfalse },  /* HL_LEGUL   */
            { 0.00f, qfalse },  /* HL_LEGUR   */
            { 0.00f, qfalse },  /* HL_LEGLL   */
            { 0.00f, qfalse },  /* HL_LEGLR   */
            { 0.00f, qfalse },  /* HL_FOOTL   */
            { 0.00f, qfalse },  /* HL_FOOTR   */
		},

		UT_MAX_DISTANCE, /* effective range */
		20.0f,		 /* knockback */

		UT_WP_GRENADE_SMOKE,
		UT_MOD_SMOKEGRENADE,

		0,		/* Delay for brass eject */

		0,		/* Brass length scale */
		0,		/* Brass diameter scale */

		0,		/* Silencer world scale */
		0,		/* Silencer view scale */

		1.0f,	/* Flash Scale */

		{      0 },

		{
/*
			{
				"Roll", 		   // name
				UT_WPMODEFLAG_HOLDTOREADY, // flags
				100,			   // damage
				400,			   // fire time
			},
*/
			{
				"Throw",
				UT_WPMODEFLAG_HOLDTOREADY |
				UT_WPMODEFLAG_NORECOIL,
				400,
				0,
				0,
				0,
				{
					0, 0, 0, 800
				}
			},

			{ NULL	 }
		},
	},

	/* SR-8 */
	{
		"Remington SR8",
		"sr8",
		UT_WPFLAG_PENETRATE_KEVLAR |
		UT_WPFLAG_PENETRATE_HELMET |
		UT_WPFLAG_NOCROSSHAIR,

		UT_WPTYPE_LARGE_GUN,
		0,

		INVENTORY_SR8,
		INVENTORY_SR8_AMMO,

		3,
		5,
		2870, /* reload time  - 2100 */
		0,    /* reload start time */
		0,    /* reload finish time */

		/* damage */
		{
        //  {  dmg , bleed  }
            { 2.50f, qfalse },  /* HL_UNKNOWN */
            { 1.00f, qfalse },  /* HL_HEAD    */
            { 1.00f, qfalse },  /* HL_HELMET  */
            { 1.00f, qfalse },  /* HL_TORSO   */
            { 1.00f, qfalse },  /* HL_VEST    */
            { 0.50f, qtrue  },  /* HL_ARML    */
            { 0.50f, qtrue  },  /* HL_ARMR    */
            { 0.80f, qtrue  },  /* HL_GROIN   */
            { 0.70f, qtrue  },  /* HL_BUTT    */
            { 0.60f, qtrue  },  /* HL_LEGUL   */
            { 0.60f, qtrue  },  /* HL_LEGUR   */
            { 0.50f, qtrue  },  /* HL_LEGLL   */
            { 0.50f, qtrue  },  /* HL_LEGLR   */
            { 0.40f, qtrue  },  /* HL_FOOTL   */
            { 0.40f, qtrue  },  /* HL_FOOTR   */
		},

		UT_MAX_DISTANCE, /* effective range */
		110.0f, 	 /* knockback */

		UT_WP_SR8,
		UT_MOD_SR8,

		30,		/* Delay for brass eject */

		2.0f,		/* Brass length scale */
		2.25f,		/* Brass diameter scale */

		0.4f,		/* Silencer world scale */
		1,		/* Silencer view scale */

		1.0f,		/* Flash Scale */

		{ 24, 9, 3},

		{
			{
				"Semi Automatic",	 /* Name */
				UT_WPMODEFLAG_NOHOLD,	 /* Flags */
				2100,			 /* fire time */
				0,
				0.0f,			 /* spread */
				0.9f,			 /* movement */
				{
					0,		 /* delay */
					12,		 /* fall */
					5.0f,		 /* rise */
					1.0f,		 /* spread */
					1.0f,		 /* rate */
				}
			},
/*			{
				"Auto Bolt",		// Name
				UT_WPMODEFLAG_NOHOLD,	// Flags
				150,			// fire time
				0,
				0.0f,			// spread
				0.4f,			// movement
				{
					0,	    //delay
					12,	    //fall
					5.0f,	    //rise
					1.0f,	    //spread
					1.0f,	    //rate
				}

		}, */

			{ NULL	 }
		},
	},

	/* AK103 */
	{
		"AK103 7.62mm",
		"ak103",
		0,
		UT_WPTYPE_LARGE_GUN,
		POWERUP(PW_SILENCER) | POWERUP(PW_LASER), //@Barbatos 4.2.002 added back the laser

		INVENTORY_AK103,
		INVENTORY_AK103_AMMO,

		2,
		30,
		3150, /* reload time */
		0,    /* reload start time */
		0,    /* reload finish time */

		/* damage */
		{
        //  {  dmg , bleed  }
            { 1.00f, qfalse },  /* HL_UNKNOWN */
            { 1.00f, qfalse },  /* HL_HEAD    */
            { 0.58f, qfalse },  /* HL_HELMET  */
            { 0.51f, qtrue  },  /* HL_TORSO   */
            { 0.34f, qfalse },  /* HL_VEST    */
            { 0.19f, qtrue  },  /* HL_ARML    */
            { 0.19f, qtrue  },  /* HL_ARMR    */
            { 0.41f, qtrue  },  /* HL_GROIN   */
            { 0.34f, qtrue  },  /* HL_BUTT    */
            { 0.22f, qtrue  },  /* HL_LEGUL   */
            { 0.22f, qtrue  },  /* HL_LEGUR   */
            { 0.19f, qtrue  },  /* HL_LEGLL   */
            { 0.19f, qtrue  },  /* HL_LEGLR   */
            { 0.15f, qtrue  },  /* HL_FOOTL   */
            { 0.15f, qtrue  },  /* HL_FOOTR   */
		},

		1300,		/* effective range */
		30.0f,		/* knockback */

		UT_WP_AK103,
		UT_MOD_AK103,

		0,		/* Delay for brass eject */

		1,		/* Brass length scale */
		1,		/* Brass diameter scale */

		0.4f,		/* Silencer world scale */
		1,		/* Silencer view scale */

		1.0f,	/* Flash Scale */

		{      0 },

		{
			{
				"Burst",		 /* Name */
				UT_WPMODEFLAG_NOHOLD,	 /* Flags */
				140,			 /* fire time */
				0,
				0.40f,			 /* spread */
				0.35f,			 /* movement */
				{
					155,
					50,
					12.0f,
					9.0f,
					0.9f,
				},
				140,			 /* Burst time */
				3,			 /* Burst shot count */
			},

			{
				"Semi-Automatic",	 /* Name */
				UT_WPMODEFLAG_NOHOLD,	 /* Flags */
				140,			 /* fire time */
				0,
				0.45f,			 /* spread */
				0.35f,			 /* movement */
				{
					155,
					50,
					12.0f,
					9.0f,
					0.9f,
				}
			},

			{
				"Automatic",		 /* Name */
				0,			 /* Flags */
				140,			 /* fire time */
				0,
				0.45f,			 /* spread */
				0.35f,			 /* movement */
				{
					155,
					50,
					12.0f,
					9.0f,
					0.9f,
				}
			},

			{ NULL	 }
		},
	},

	/* BOMB */
	{
		"Bomb",
		"bombbag",
		UT_WPFLAG_NOFLASH | UT_WPFLAG_REMOVEONEMPTY,
		UT_WPTYPE_PLANT,
		0,

		INVENTORY_NONE,
		INVENTORY_NONE,

		0,    /* clips */
		1,    /* bullets */
		860,
		0,    /* reload start time */
		0,    /* reload finish time */

		/* damage */
		{
        //  {  dmg , bleed  }
            { 1.00f, qfalse },  /* HL_UNKNOWN */
            { 0.00f, qfalse },  /* HL_HEAD    */
            { 0.00f, qfalse },  /* HL_HELMET  */
            { 0.00f, qfalse },  /* HL_TORSO   */
            { 0.00f, qfalse },  /* HL_VEST    */
            { 0.00f, qfalse },  /* HL_ARML    */
            { 0.00f, qfalse },  /* HL_ARMR    */
            { 0.00f, qfalse },  /* HL_GROIN   */
            { 0.00f, qfalse },  /* HL_BUTT    */
            { 0.00f, qfalse },  /* HL_LEGUL   */
            { 0.00f, qfalse },  /* HL_LEGUR   */
            { 0.00f, qfalse },  /* HL_LEGLL   */
            { 0.00f, qfalse },  /* HL_LEGLR   */
            { 0.00f, qfalse },  /* HL_FOOTL   */
            { 0.00f, qfalse },  /* HL_FOOTR   */
		},

		UT_MAX_DISTANCE, /* effective range */
		20.0f,		 /* knockback */

		UT_WP_KNIFE,
		UT_MOD_BOMBED,

		0,		/* Delay for brass eject */

		0,		/* Brass length scale */
		0,		/* Brass diameter scale */

		0,		/* Silencer world scale */
		0,		/* Silencer view scale */

		1.0f,		/* Flash Scale */

		{      0 },

		{
			{
				"Plant",
				UT_WPMODEFLAG_NOAMMO |
				UT_WPMODEFLAG_NOHOLD |
				UT_WPMODEFLAG_NORECOIL,
				3000,
				0,
				0,
				0,
				{
					0,
					0,
					0,
					800,
				}
			},

			{ NULL	 }
		},
	},
	/* NEGEV */
	{
		"IMI Negev",			 /* Name */
		"negev",
		0,				 /* Flags */
		UT_WPTYPE_LARGE_GUN,		 /* Weapon type */
		POWERUP(PW_SILENCER),		 /* Available powerups */

		INVENTORY_NEGEV,
		INVENTORY_NEGEV_AMMO,

		1,    /* Clips */
		90,   /* Bullets per clip */
		7000, /* reload time */
		0,    /* reload start time */
		4500, /* reload finish time */

		/* damage */
		{
        //  {  dmg , bleed  }
            { 0.58f, qtrue  },  /* HL_UNKNOWN */
            { 0.50f, qtrue  },  /* HL_HEAD    */
            { 0.34f, qfalse },  /* HL_HELMET  */
            { 0.30f, qtrue  },  /* HL_TORSO   */
            { 0.20f, qfalse },  /* HL_VEST    */
            { 0.11f, qtrue  },  /* HL_ARML    */
            { 0.11f, qtrue  },  /* HL_ARMR    */
            { 0.25f, qtrue  },  /* HL_GROIN   */
            { 0.22f, qtrue  },  /* HL_BUTT    */
            { 0.13f, qtrue  },  /* HL_LEGUL   */
            { 0.13f, qtrue  },  /* HL_LEGUR   */
            { 0.11f, qtrue  },  /* HL_LEGLL   */
            { 0.11f, qtrue  },  /* HL_LEGLR   */
            { 0.09f, qtrue  },  /* HL_FOOTL   */
            { 0.09f, qtrue  },  /* HL_FOOTR   */
		},

		UT_MAX_DISTANCE, /* effective range */
		40.0f,		 /* knockback */

		UT_WP_NEGEV,
		UT_MOD_NEGEV,

		0,		/* Delay for brass eject */

		1,		/* Brass length scale */
		1,		/* Brass diameter scale */

		0.4f,		/* Silencer world scale */
		1,		/* Silencer view scale */

		1.0f,		/* Flash Scale */

		{      0 },

		{
			{
				"Automatic",		 /* Name */
				0,
				100,			 /* fire time */
				0,
				1.1f,			 /* spread */
				0.275f, 		 /* movement */
				{
					130,
					100,
					1.5f,
					3.25f,
					1.0f,
				}
			},

			{ NULL	 }
		},
	},

	//@Barbatos: GRENADE_FRAG
	//We still keep it or it'll break compatibility with 4.1 configs
	{ NULL },

	/* M4 aka ZM LR300 */
	{
		"M4A1", /* M4 */
		"m4",
		0,
		UT_WPTYPE_LARGE_GUN,
		POWERUP(PW_SILENCER) | POWERUP(PW_LASER),

		INVENTORY_M4,
		INVENTORY_M4_AMMO,

		2,
		30,
		2450, /* reload time */
		0,    /* reload start time */
		0,    /* reload finish time */

		/* damage */
		{
        //  {  dmg , bleed  }
            { 0.85f, qtrue  },  /* HL_UNKNOWN */
            { 1.00f, qfalse },  /* HL_HEAD    */
            { 0.51f, qfalse },  /* HL_HELMET  */
            { 0.44f, qtrue  },  /* HL_TORSO   */
            { 0.29f, qfalse },  /* HL_VEST    */
            { 0.17f, qtrue  },  /* HL_ARML    */
            { 0.17f, qtrue  },  /* HL_ARMR    */
            { 0.37f, qtrue  },  /* HL_GROIN   */
            { 0.33f, qtrue  },  /* HL_BUTT    */
            { 0.20f, qtrue  },  /* HL_LEGUL   */
            { 0.20f, qtrue  },  /* HL_LEGUR   */
            { 0.17f, qtrue  },  /* HL_LEGLL   */
            { 0.17f, qtrue  },  /* HL_LEGLR   */
            { 0.14f, qtrue  },  /* HL_FOOTL   */
            { 0.14f, qtrue  },  /* HL_FOOTR   */
		},

		1600,		/* effective range */
		25.0f,		/* knockback */

		UT_WP_M4,
		UT_MOD_M4,

		0,		/* Delay for brass eject */

		1,		/* Brass length scale */
		1,		/* Brass diameter scale */

		0.4f,		/* Silencer world scale */
		1,		/* Silencer view scale */

		1.0f,	/* Flash Scale */

		{      0 },

		{
			{
				"Burst",		 /* Name */
				UT_WPMODEFLAG_NOHOLD,	 /* Flags */
				370,			 /* fire time */
				0,
				0.5f,			 /* spread */
				0.2f,			 /* movement */
				{
					160,
					100,
					8.0f,
					7.0f,
					1.0f,
				},
				120,			 /* Burst time */
				3,			 /* Burst shot count */
			},

			{
				"Semi-Automatic",	 /* Name */
				UT_WPMODEFLAG_NOHOLD,	 /* Flags */
				120,			 /* fire time */
				0,
				0.5f,			 /* spread */
				0.2f,			 /* movement */
				{
					160,
					100,
					8.0f,
					7.0f,
					1.0f,
				}
			},

			{
				"Automatic",		 /* Name */
				0,			 /* Flags */
				120,			 /* fire time */
				0,
				0.5f,			 /* spread */
				0.2f,			 /* movement */
				{
					160,
					100,
					8.0f,
					7.0f,
					1.0f,
				}
			},

			{ NULL	 }
		},
	},

/* Glock */
	{
		"Glock 18",
		"glock",
		0,
		UT_WPTYPE_SMALL_GUN,
		POWERUP(PW_SILENCER) | POWERUP(PW_LASER),

		INVENTORY_GLOCK,
		INVENTORY_GLOCK_AMMO,

		2,    /* Clips */
		12,   /* Shots per clip */
		2600, /* reload time */
		0,    /* reload start time */
		0,    /* reload finish time */

		/* damage */
		{
        //  {  dmg , bleed  }
            { 0.58f, qtrue  },  /* HL_UNKNOWN */
            { 0.60f, qtrue  },  /* HL_HEAD    */
            { 0.40f, qfalse },  /* HL_HELMET  */
            { 0.33f, qtrue  },  /* HL_TORSO   */
            { 0.23f, qfalse },  /* HL_VEST    */
            { 0.14f, qtrue  },  /* HL_ARML    */
            { 0.14f, qtrue  },  /* HL_ARMR    */
            { 0.28f, qtrue  },  /* HL_GROIN   */
            { 0.25f, qtrue  },  /* HL_BUTT    */
            { 0.17f, qtrue  },  /* HL_LEGUL   */
            { 0.17f, qtrue  },  /* HL_LEGUR   */
            { 0.14f, qtrue  },  /* HL_LEGLL   */
            { 0.14f, qtrue  },  /* HL_LEGLR   */
            { 0.11f, qtrue  },  /* HL_FOOTL   */
            { 0.11f, qtrue  },  /* HL_FOOTR   */
		},

		700,		/* effective range */
		15.0f,		/* knockback */

		UT_WP_GLOCK,
		UT_MOD_GLOCK,	/* Means of death */

		0,		/* Delay for brass eject */

		0.5f,		/* Brass length scale */
		1.0f,		/* Brass diameter scale */

		0.27f,		/* Silencer world scale */
		0.75f,		/* Silencer view scale */

		0.35f,		/* Flash Scale */

		{      0 },

		{
			{
				"Burst",		 /* Name */
				UT_WPMODEFLAG_NOHOLD,	 /* Flags */
				420,			 /* fire time */
				0,
				1.0f,			 /* spread */
				0.3f,			 /* movement */
				{
					180,
					80,
					8.0f,
					4.0f,
					1.0f,
				},
				50,
				3,
			},

			{
				"Semi-Automatic",	  /* Name */
				UT_WPMODEFLAG_NOHOLD,
				120,			  /* fire time */
				0,
				0.75f,			  /* spread */
				0.05f,			  /* movement */
				{
					0,
					50,
					6.0f,  // Rise (Recoil)
					10.0f,
					1.0f,
				}
			},

			{ NULL	 }
		},
	},

	// Colt 1911
    {
        "Colt 1911",                                    // Name
        "colt1911",                                     // Model name
        0,                                              // Flags
        UT_WPTYPE_SMALL_GUN,                            // Weapon type
        POWERUP(PW_SILENCER) | POWERUP(PW_LASER),       // Powerups

        INVENTORY_COLT1911,                             // Inventory weapon ID
        INVENTORY_COLT1911_AMMO,                        // Inventory ammo ID

        2,                                              // Clips
        10,                                             // Bullets per clip
        2300,                                           // Reload time
        0,                                              // Reload start time
        0,                                              // Reload finish time

        {                                               // Damage table (value, bleed)

            { 1.00f, qtrue  },                          // HL_UNKNOWN
            { 1.00f, qfalse },                          // HL_HEAD
            { 0.60f, qfalse },                          // HL_HELMET
            { 0.37f, qtrue  },                          // HL_TORSO
            { 0.27f, qfalse },                          // HL_VEST
            { 0.15f, qtrue  },                          // HL_ARML
            { 0.15f, qtrue  },                          // HL_ARMR
            { 0.32f, qtrue  },                          // HL_GROIN
            { 0.29f, qtrue  },                          // HL_BUTT
            { 0.22f, qtrue  },                          // HL_THIGHL
            { 0.22f, qtrue  },                          // HL_THIGHR
            { 0.15f, qtrue  },                          // HL_CALFL
            { 0.15f, qtrue  },                          // HL_CALFR
            { 0.11f, qtrue  },                          // HL_FOOTL
            { 0.11f, qtrue  },                          // HL_FOOTR

        },

        700,                                            // Effective range
        15.0f,                                          // Knockback

        UT_WP_COLT1911,                                 // Weapon ID
        UT_MOD_COLT1911,                                // Means of death ID

        0,                                              // Delay for brass eject
        0.5f,                                           // Brass length scale
        1.0f,                                           // Brass diameter scale
        0.27f,                                          // Silencer world scale
        0.75f,                                          // Silencer view scale
        0.35f,                                          // Flash Scale

        { 0 },                                          // Zoom levels

        {
                                                        // Weapon modes
            {
                "Semi-Automatic",                       // Name
                UT_WPMODEFLAG_NOHOLD,                   // Flags
                120,                                    // Fire time
                0,                                      // Change time
                0.75f,                                  // Spread
                0.05f,                                  // Movement
                {
                    0,                                  // Delay (Recoil)
                    50,                                 // Fall (Recoil)
                    13.0f,                               // Rise (Recoil)
                    13.0f,                              // Spread (Recoil)
                    1.0f,                               // Rate (Recoil)
                }
            },

            { NULL }

        },
    },

    // Ingram Mac 11
    {
        "Ingram Mac 11",                                // Name
        "mac11",                                        // Model name
        0,                                              // Flags
        UT_WPTYPE_MEDIUM_GUN,                           // Weapon type
        POWERUP(PW_SILENCER),                           // Powerups

        INVENTORY_MAC11,                                // Inventory weapon ID
        INVENTORY_MAC11_AMMO,                           // Inventory ammo ID

        2,                                              // Clips
        32,                                             // Bullets per clip
        1800,                                           // Reload time
        0,                                              // Reload start time
        0,                                              // Reload finish time

        {                                               // Damage table (value, bleed)

            { 0.34f, qtrue  },                          // HL_UNKNOWN
            { 0.34f, qtrue  },                          // HL_HEAD
            { 0.29f, qfalse },                          // HL_HELMET
            { 0.20f, qtrue  },                          // HL_TORSO
            { 0.15f, qfalse },                          // HL_VEST
            { 0.11f, qtrue  },                          // HL_ARML
            { 0.11f, qtrue  },                          // HL_ARMR
            { 0.18f, qtrue  },                          // HL_GROIN
            { 0.17f, qtrue  },                          // HL_BUTT
            { 0.15f, qtrue  },                          // HL_THIGHL
            { 0.15f, qtrue  },                          // HL_THIGHR
            { 0.13f, qtrue  },                          // HL_CALFL
            { 0.13f, qtrue  },                          // HL_CALFR
            { 0.11f, qtrue  },                          // HL_FOOTL
            { 0.11f, qtrue  },                          // HL_FOOTR

        },

        650,                                            // Effective range
        15.0f,                                          // Knockback

        UT_WP_MAC11,                                    // Weapon ID
        UT_MOD_MAC11,                                   // Means of death ID

        0,                                              // Delay for brass eject
        0.5f,                                           // Brass length scale
        1.0f,                                           // Brass diameter scale
        0.30f,                                          // Silencer world scale
        0.75f,                                          // Silencer view scale
        0.5f,                                           // Flash Scale

        { 0 },                                          // Zoom levels

        {
            //{
            //    "Burst",                              // Name
            //    UT_WPMODEFLAG_NOHOLD,                 // Flags
            //    60,                                   // Fire time
            //    0,                                    // Change time
            //    1.3f,                                 // Spread
            //    0.35f,                                // Movement
            //    {
            //        130,                              // Delay (Recoil)
            //        100,                              // Fall (Recoil)
            //        1.5f,                             // Rise (Recoil)
            //        3.5f,                             // Spread (Recoil)
            //        1.0f,                             // Rate (Recoil)
            //    },
            //    100,                                  // Burst time
            //    3,                                    // Burst rounds
            //},

            {
                "Automatic",                            // Name
                0,                                      // Flags
                85,                                     // fire time
                0,                                      // Change time
                1.3f,                                   // Spread
                0.35f,                                  // Movement
                {
                    130,                                // Delay (Recoil)
                    100,                                // Fall (Recoil)
                    6.0f,                               // Rise (Recoil)
                    4.25f,                              // Spread (Recoil)
                    1.0f,                               // Rate (Recoil)
                }
            },

            { NULL }

        },
    },

//@Barbatos we don't use these weapons in 4.2
#if 0

	/* P90 */
	{
		"FN P90",		 /* Name */
		"p90",
		0,			 /* Flags */
		UT_WPTYPE_MEDIUM_GUN,	 /* Type */
		POWERUP(PW_SILENCER) |
		POWERUP(PW_LASER),	 /* Available powerups */

		INVENTORY_P90,
		INVENTORY_P90_AMMO,

		2,    /* Clips */
		50,   /* Bullets */
		4200, /* reload time */
		0,    /* reload start time */
		0,    /* reload finish time */

		/* damage */
		{
			{ 0.58f, qtrue },  /* HL_UNKNOWN */
			{ 0.50f, qtrue },  /* HL_HEAD */
			{ 0.50f, qtrue },  /* HL_EARS */
			{ 0.50f, qtrue },  /* HL_HAIR */
			{ 0.34f, qfalse }, /* HL_HELMET */
			{ 0.30f, qtrue },  /* HL_TORSO */
			{ 0.20f, qfalse }, /* HL_VEST */
			{ 0.11f, qtrue },  /* HL_ARML */
			{ 0.11f, qtrue },  /* HL_ARMR */
			{ 0.11f, qtrue },  /* HL_LEGS */
			{ 0.11f, qtrue },  /* HL_HOLSTER */
			{ 0.11f, qtrue },  /* HL_POCKET */
			{ 0.11f, qtrue },  /* HL_BODY */
			{ 0.30f, qtrue },  /* HL_CHEST */
			{ 0.30f, qtrue },  /* HL_BACK */
		},

		650,		/* effective range */
		15.0f,		/* knockback */

		UT_WP_P90,
		UT_MOD_P90,	/* Means of death */

		0,		/* Delay for brass eject */

		0.5f,		/* Brass length scale */
		1.0f,		/* Brass diameter scale */

		0.30f,		/* Silencer world scale */
		0.75f,		/* Silencer view scale */

		0.5f,		/* Flash Scale */

		{      0 },	/* Zoom levels */

		{
			{
				"Burst",		 /* Name */
				UT_WPMODEFLAG_NOHOLD,	 /* Flags */
				100,			 /* fire time */
				0,
				1.1f,			 /* spread */
				0.300f, 		 /* movement */
				{
					130,
					100,
					1.5f,
					3.5f,
					1.0f,
				},
				100,
				3,
			},

			{
				"Semi-Automatic",	 /* Name */
				UT_WPMODEFLAG_NOHOLD,	 /* Flags */
				138,			 /* fire time */
				0,
				0.5f,			 /* spread */
				0.300f, 		 /* movement */
				{
					130,
					100,
					1.5f,
					3.0f,
					1.0f,
				}
			},

			{
				"Automatic",		 /* Name */
				0,
				100,			 /* fire time */
				0,
				1.3f,			 /* spread */
				0.300f, 		 /* movement */
				{
					130,
					100,
					1.5f,
					3.25f,
					1.0f,
				}
			},

			{ NULL	 }
		},
	},

	/* Benelli */
	{
		"Benelli M4 Super 90",
		"benelli",
		UT_WPFLAG_SINGLEBULLETCLIP,
		UT_WPTYPE_MEDIUM_GUN,
		0,

		INVENTORY_BENELLI,
		INVENTORY_BENELLI_AMMO,

		24,
		8,
		533,  /* reload time */
		533,  /* reload start time */
		866,  /* reload finish time */

		/* damage */
		{
			{ 0.95f, qtrue },  /* HL_UNKNOWN */
			{ 0.24f, qtrue },  /* HL_HEAD */
			{ 0.24f, qtrue },  /* HL_EARS */
			{ 0.24f, qtrue },  /* HL_HAIR */
			{ 0.16f, qfalse }, /* HL_HELMET */
			{ 0.10f, qtrue },  /* HL_TORSO */
			{ 0.08f, qfalse }, /* HL_VEST */
			{ 0.06f, qtrue },  /* HL_ARML */
			{ 0.06f, qtrue },  /* HL_ARMR */
			{ 0.06f, qtrue },  /* HL_LEGS */
			{ 0.06f, qtrue },  /* HL_HOLSTER */
			{ 0.06f, qtrue },  /* HL_POCKET */
			{ 0.06f, qtrue },  /* HL_BODY */
			{ 0.10f, qtrue },  /* HL_CHEST */
			{ 0.10f, qtrue },  /* HL_BACK */
		},

		300,		/* effective range */
		5.0f,		/* knockback  */

		UT_WP_BENELLI,
		UT_MOD_BENELLI,

		0,		/* Delay for brass eject */

		0.75f,		/* Brass length scale */
		0.75f,		/* Brass diameter scale */

		0,		/* Silencer world scale */
		0,		/* Silencer view scale */

		1.25f,		/* Flash Scale */

		{      0 },

		{
			{
				"Semi-Automatic",	 /* Name */
				UT_WPMODEFLAG_NOHOLD,	 /* Flags */
				350,			 /* fire time */
				0,
				3.0f,			 /* spread */
				0.0f,			 /* movement */
				{
					0,
					15,
					42.5f,
					2.0f,
					1.0f,
				}
			},

			{ NULL	 }
		},
	},

	/* Magnum */
	{
		"Magnum",
		"magnum",
		0,
		UT_WPTYPE_SMALL_GUN,
		0,

		INVENTORY_MAGNUM,
		INVENTORY_MAGNUM_AMMO,

		3,    /* Clips */
		6,    /* Shots per clip */
		1640, /* reload time */
		0,    /* reload start time */
		0,    /* reload finish time */

		/* damage */
		{
			{ 1.10f, qtrue },  /* HL_UNKNOWN */
			{ 1.00f, qfalse }, /* HL_HEAD */
			{ 1.00f, qfalse }, /* HL_EARS */
			{ 1.00f, qfalse }, /* HL_HAIR */
			{ 0.66f, qfalse }, /* HL_HELMET */
			{ 0.57f, qtrue },  /* HL_TORSO */
			{ 0.38f, qfalse }, /* HL_VEST */
			{ 0.22f, qtrue },  /* HL_ARML */
			{ 0.22f, qtrue },  /* HL_ARMR */
			{ 0.22f, qtrue },  /* HL_LEGS */
			{ 0.22f, qtrue },  /* HL_HOLSTER */
			{ 0.22f, qtrue },  /* HL_POCKET */
			{ 0.22f, qtrue },  /* HL_BODY */
			{ 0.57f, qtrue },  /* HL_CHEST */
			{ 0.57f, qtrue },  /* HL_BACK */
		},

		700,		/* effective range */
		100.0f, 	/* knockback */

		UT_WP_MAGNUM,
		UT_MOD_MAGNUM,	/* Means of death */

		0,		/* Delay for brass eject */

		0.5f,		/* Brass length scale */
		1.0f,		/* Brass diameter scale */

		0.27f,		/* Silencer world scale */
		0.75f,		/* Silencer view scale */

		0.35f,		/* Flash Scale */

		{      0 },

		{
			{
				"Semi-Automatic",	  /* Name */
				UT_WPMODEFLAG_NOHOLD,
				120,			  /* fire time */
				0,
				0.7f,			  /* spread */
				0.05f,			  /* movement */
				{
					0,
					50,
					3.0f,
					10.0f,
					1.0f,
				}
			},

			{ NULL	 }
		},
	},


	/* Dual Berettas */
	{
		"Dual Beretta 92G",
		"beretta",
		0,
		UT_WPTYPE_SMALL_GUN_DUAL,
		POWERUP(PW_SILENCER) | POWERUP(PW_LASER),

		INVENTORY_BERETTA,
		INVENTORY_BERETTA_AMMO,

		2,    /* Clips */
		30,   /* Shots per clip */
		1640, /* reload time */
		0,    /* reload start time */
		0,    /* reload finish time */

		/* damage */
		{
			{ 0.58f, qtrue },  /* HL_UNKNOWN */
			{ 1.00f, qfalse }, /* HL_HEAD */
			{ 1.00f, qfalse }, /* HL_EARS */
			{ 1.00f, qfalse }, /* HL_HAIR */
			{ 0.34f, qfalse }, /* HL_HELMET */
			{ 0.30f, qtrue },  /* HL_TORSO */
			{ 0.20f, qfalse }, /* HL_VEST */
			{ 0.11f, qtrue },  /* HL_ARML */
			{ 0.11f, qtrue },  /* HL_ARMR */
			{ 0.11f, qtrue },  /* HL_LEGS */
			{ 0.11f, qtrue },  /* HL_HOLSTER */
			{ 0.11f, qtrue },  /* HL_POCKET */
			{ 0.11f, qtrue },  /* HL_BODY */
			{ 0.30f, qtrue },  /* HL_CHEST */
			{ 0.30f, qtrue },  /* HL_BACK */
		},

		700,		/* effective range */
		15.0f,		/* knockback */

		UT_WP_DUAL_BERETTA,
		UT_MOD_BERETTA, /* Means of death */

		0,		/* Delay for brass eject */

		0.5f,		/* Brass length scale */
		1.0f,		/* Brass diameter scale */

		0.27f,		/* Silencer world scale */
		0.75f,		/* Silencer view scale */

		0.35f,		/* Flash Scale */

		{      0 },

		{
			{
				"Semi-Automatic",	  /* Name */
				UT_WPMODEFLAG_NOHOLD,
				120,			  /* fire time */
				0,
				0.7f,			  /* spread */
				0.05f,			  /* movement */
				{
					0,
					50,
					3.0f,
					10.0f,
					1.0f,
				}
			},

			{ NULL	 }
		},
	},

	/* Dual Desert Eagles */
	{
		"Dual IMI DE .50 AE",
		"deserteagle",
		0,
		UT_WPTYPE_SMALL_GUN_DUAL,
		POWERUP(PW_LASER),

		INVENTORY_DEAGLE,
		INVENTORY_DEAGLE_AMMO,

		2,    /* Clips */
		14,    /* Shots per clip */
		3220, /* reload time */
		0,    /* reload start time */
		0,    /* reload finish time */

		/* damage */
		{
			{ 1.10f, qtrue },  /* HL_UNKNOWN */
			{ 1.00f, qfalse }, /* HL_HEAD */
			{ 1.00f, qfalse }, /* HL_EARS */
			{ 1.00f, qfalse }, /* HL_HAIR */
			{ 0.66f, qfalse }, /* HL_HELMET */
			{ 0.57f, qtrue },  /* HL_TORSO */
			{ 0.38f, qfalse }, /* HL_VEST */
			{ 0.22f, qtrue },  /* HL_ARML */
			{ 0.22f, qtrue },  /* HL_ARMR */
			{ 0.22f, qtrue },  /* HL_LEGS */
			{ 0.22f, qtrue },  /* HL_HOLSTER */
			{ 0.22f, qtrue },  /* HL_POCKET */
			{ 0.22f, qtrue },  /* HL_BODY */
			{ 0.57f, qtrue },  /* HL_CHEST */
			{ 0.57f, qtrue },  /* HL_BACK */
		},

		600,		/* effective range */
		100.0f, 	/* knockback */

		UT_WP_DUAL_DEAGLE,
		UT_MOD_DEAGLE,	/* Means of death */

		0,		/* Delay for brass eject */

		0.5f,		/* Brass length scale */
		1.5f,		/* Brass diameter scale */

		0.27f,		/* Silencer world scale */
		0.75f,		/* Silencer view scale */

		0.4f,	/* Flash Scale */

		{      0 },

		{
			{
				"Semi-Automatic",	 /* Name */
				UT_WPMODEFLAG_NOHOLD,
				250,			 /* fire time */
				0,
				0.8f,			 /* spread */
				0.05f,			 /* movement */
				{
					0,
					20,
					20.0f,
					12.0f,
					1.0f,
				}
			},

			{ NULL	 }
		},
	},

	/* Dual Glock */
	{
		"Dual Glocks",
		"glock",
		0,
		UT_WPTYPE_SMALL_GUN_DUAL,
		POWERUP(PW_SILENCER) | POWERUP(PW_LASER),

		INVENTORY_GLOCK,
		INVENTORY_GLOCK_AMMO,

		2,    /* Clips */
		30,   /* Shots per clip */
		1640, /* reload time */
		0,    /* reload start time */
		0,    /* reload finish time */

		/* damage */
		{
			{ 0.58f, qtrue },  /* HL_UNKNOWN */
			{ 1.00f, qfalse }, /* HL_HEAD */
			{ 1.00f, qfalse }, /* HL_EARS */
			{ 1.00f, qfalse }, /* HL_HAIR */
			{ 0.34f, qfalse }, /* HL_HELMET */
			{ 0.30f, qtrue },  /* HL_TORSO */
			{ 0.20f, qfalse }, /* HL_VEST */
			{ 0.11f, qtrue },  /* HL_ARML */
			{ 0.11f, qtrue },  /* HL_ARMR */
			{ 0.11f, qtrue },  /* HL_LEGS */
			{ 0.11f, qtrue },  /* HL_HOLSTER */
			{ 0.11f, qtrue },  /* HL_POCKET */
			{ 0.11f, qtrue },  /* HL_BODY */
			{ 0.30f, qtrue },  /* HL_CHEST */
			{ 0.30f, qtrue },  /* HL_BACK */
		},

		700,		/* effective range */
		15.0f,		/* knockback */

		UT_WP_DUAL_GLOCK,
		UT_MOD_GLOCK,  /* Means of death */

		0,		/* Delay for brass eject */

		0.5f,		/* Brass length scale */
		1.0f,		/* Brass diameter scale */

		0.27f,		/* Silencer world scale */
		0.75f,		/* Silencer view scale */

		0.35f,		/* Flash Scale */

		{      0 },

		{
			{
				"Semi-Automatic",	  /* Name */
				UT_WPMODEFLAG_NOHOLD,
				120,			  /* fire time */
				0,
				0.7f,			  /* spread */
				0.05f,			  /* movement */
				{
					0,
					50,
					3.0f,
					10.0f,
					1.0f,
				}
			},

			{ NULL	 }
		},
	},

	/* Dual Magnum */
	{
		"Dual .44 Magnums",
		"magnum",
		0,
		UT_WPTYPE_SMALL_GUN_DUAL,
		POWERUP(PW_LASER),

		INVENTORY_MAGNUM,
		INVENTORY_MAGNUM_AMMO,

		3,    /* Clips */
		12,    /* Shots per clip */
		1640, /* reload time */
		0,    /* reload start time */
		0,    /* reload finish time */

		/* damage */
		{
			{ 1.10f, qtrue },  /* HL_UNKNOWN */
			{ 1.00f, qfalse }, /* HL_HEAD */
			{ 1.00f, qfalse }, /* HL_EARS */
			{ 1.00f, qfalse }, /* HL_HAIR */
			{ 0.66f, qfalse }, /* HL_HELMET */
			{ 0.57f, qtrue },  /* HL_TORSO */
			{ 0.38f, qfalse }, /* HL_VEST */
			{ 0.22f, qtrue },  /* HL_ARML */
			{ 0.22f, qtrue },  /* HL_ARMR */
			{ 0.22f, qtrue },  /* HL_LEGS */
			{ 0.22f, qtrue },  /* HL_HOLSTER */
			{ 0.22f, qtrue },  /* HL_POCKET */
			{ 0.22f, qtrue },  /* HL_BODY */
			{ 0.57f, qtrue },  /* HL_CHEST */
			{ 0.57f, qtrue },  /* HL_BACK */
		},

		700,		/* effective range */
		100.0f, 	/* knockback */

		UT_WP_DUAL_MAGNUM,
		UT_MOD_MAGNUM,	/* Means of death */

		0,		/* Delay for brass eject */

		0.5f,		/* Brass length scale */
		1.0f,		/* Brass diameter scale */

		0.27f,		/* Silencer world scale */
		0.75f,		/* Silencer view scale */

		0.35f,		/* Flash Scale */

		{      0 },

		{
			{
				"Semi-Automatic",	  /* Name */
				UT_WPMODEFLAG_NOHOLD,
				120,			  /* fire time */
				0,
				0.7f,			  /* spread */
				0.05f,			  /* movement */
				{
					0,
					50,
					3.0f,
					10.0f,
					1.0f,
				}
			},

			{ NULL	 }
		},
	},

#endif


	/* No weapon */
	{ "None" },

	/* KICK - Special weapon */
	{
		"Kick",
		"kick",
		UT_WPFLAG_NOFLASH,
		UT_WPTYPE_MELEE,
		0,

		INVENTORY_NONE,
		INVENTORY_NONE,

		0,    /* clips */
		0,    /* bullets */
		0,
		0,    /* reload start time */
		0,    /* reload finish time */


		/* damage */
		{
        //  {  dmg , bleed  }
            { 0.50f, qfalse },  /* HL_UNKNOWN */
            { 0.30f, qfalse },  /* HL_HEAD    */
            { 0.20f, qfalse },  /* HL_HELMET  */
            { 0.20f, qfalse },  /* HL_TORSO   */
            { 0.20f, qfalse },  /* HL_VEST    */
            { 0.20f, qfalse },  /* HL_ARML    */
            { 0.20f, qfalse },  /* HL_ARMR    */
            { 0.20f, qfalse },  /* HL_GROIN   */
            { 0.20f, qfalse },  /* HL_BUTT    */
            { 0.20f, qfalse },  /* HL_LEGUL   */
            { 0.20f, qfalse },  /* HL_LEGUR   */
            { 0.20f, qfalse },  /* HL_LEGLL   */
            { 0.20f, qfalse },  /* HL_LEGLR   */
            { 0.20f, qfalse },  /* HL_FOOTL   */
            { 0.20f, qfalse },  /* HL_FOOTR   */
		},

		1.0,		/* effective range */
		100.0f, 	/* knockback */

		UT_WP_KICK,
		UT_MOD_KICKED,

		0,		/* Delay for brass eject */

		0,		/* Brass length scale */
		0,		/* Brass diameter scale */

		0,		/* Silencer world scale */
		0,		/* Silencer view scale */

		1.0f,		/* Flash Scale */

		{      0 },

		{
			{
				"Kick",
				UT_WPMODEFLAG_NOAMMO | UT_WPMODEFLAG_NORECOIL,
				400,
				0,
			},

			{ NULL	 }
		},
	},

	/* special weapon thrown knife - not a proper weapon */
	{
		"Thrown knife",
		"knife",
		UT_WPFLAG_NOFLASH,
		UT_WPTYPE_THROW,
		0,

		INVENTORY_NONE,
		INVENTORY_NONE,

		0,    /* clips */
		0,    /* bullets */
		0,
		0,    /* reload start time */
		0,    /* reload finish time */

		/* damage */
		{
        //  {  dmg , bleed  }
            { 1.00f, qfalse }, /* HL_UNKNOWN */
            { 1.00f, qfalse }, /* HL_HEAD    */
            { 0.60f, qfalse }, /* HL_HELMET  */
            { 0.44f, qtrue  }, /* HL_TORSO   */
            { 0.35f, qfalse }, /* HL_VEST    */
            { 0.20f, qtrue  }, /* HL_ARML    */
            { 0.20f, qtrue  }, /* HL_ARMR    */
            { 0.40f, qtrue  }, /* HL_GROIN   */
            { 0.37f, qtrue  }, /* HL_BUTT    */
            { 0.20f, qtrue  }, /* HL_LEGUL   */
            { 0.20f, qtrue  }, /* HL_LEGUR   */
            { 0.18f, qtrue  }, /* HL_LEGLL   */
            { 0.18f, qtrue  }, /* HL_LEGLR   */
            { 0.15f, qtrue  }, /* HL_FOOTL   */
            { 0.15f, qtrue  }, /* HL_FOOTR   */
		},

		900,		/* effective range */
		0.0f,		/* knockback */

		UT_WP_KNIFE_THROWN,
		UT_MOD_KNIFE_THROWN,

		0,		/* Delay for brass eject */

		0,		/* Brass length scale */
		0,		/* Brass diameter scale */

		0,		/* Silencer world scale */
		0,		/* Silencer view scale */

		1.0f,		/* Flash Scale */

		{      0 },

		{
			{
				"Thrown knife",
				UT_WPMODEFLAG_NOAMMO | UT_WPMODEFLAG_NORECOIL,
				400,
				0,
			},

			{ NULL	 }
		},
	},
};

int bg_numWeapons = sizeof(bg_weaponlist) / sizeof(bg_weaponlist[0]) - 1;


