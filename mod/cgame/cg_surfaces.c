/**
 * Filename: $(filename)
 *
 * Copyright (C) 1999-2005 Id Software, Inc.
 * Copyright (C) 2000-2009 FrozenSand
 *
 * This file is part of Urban Terror source code.
 *
 */
#include "cg_local.h"

#define PART_MAXVEL 60
#define PART_MINVEL 5

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : surfaceEffectSmoke
// Description: Adds some smoke
// Author	  : Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
void surfaceEffectSmoke ( vec3_t origin, vec3_t from, vec3_t normal, float radius, float r, float g, float b, float a )
{
	vec3_t	vel;
	vec3_t	temp;
	vec3_t	up = { 0, 0, 8};
	int n;
	//int count;

	// No smoke for them poor rage pro owners
	if (cgs.glconfig.hardwareType == GLHW_RAGEPRO)
	{
		return;
	}

	//count=rand()%4;

	for (n = 0; n < 5; n++)
	{
		VectorSubtract( from, origin, temp );
		VectorNormalizeNR ( temp );

		// rotate it to the other side of the normal (reflect)
		RotatePointAroundVector(vel, normal, temp, 180);
		VectorScale (vel, rand() % (PART_MAXVEL - PART_MINVEL) + PART_MINVEL, vel);
		VectorAdd ( vel, up, vel );
		vel[0] += ((rand() % 100) - 50) * 0.15;
		vel[1] += ((rand() % 100) - 50) * 0.15;
		vel[2] += ((rand() % 100) - 50) * 0.15;
		CG_SmokePuff( origin, vel, radius, r, g, b, a, 900 + rand() % 1000, cg.time, 0, 0, cgs.media.shotgunSmokePuffShader );
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : surfaceEffectFlesh
// Description: blood, blood, blood
// Author	  : Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
void surfaceEffectFlesh ( vec3_t origin, vec3_t from, vec3_t normal, int severity )
{
	//particle_t	part;
	localEntity_t  *le;
	int 	   parts;
	vec3_t		   temp;

	if (cg_blood.value <= 0)
	{
		return;
	}

//	blood = cg_blood.value ;
//	if ( blood >0 )
//		blood = 5;

	// Add some normal smoke, maybe a little blackish
	//surfaceEffectSmoke ( origin, from, normal, 5 + rand()%5, 1.0f, 0.2f, 0.2f, 0.63f );

	// To put teh from on the other side is 2 * origin - from
	VectorScale ( origin, 2.0f, temp );
	VectorSubtract ( temp, from, from );

	VectorSubtract ( origin, from, normal );
	VectorNormalizeNR ( normal );

	if (severity == 0)
	{
		return;
	}

//	parts = ((severity*3/5) + (rand()%(severity*3/5)));
//	parts *= blood;
	parts		 = 5;

	le		 = CG_ParticleCreate ( );
	CG_ParticleSetAttributes ( le, PART_ATTR_BLOOD, cgs.media.bloodExplosionShader, cgs.media.bloodMarkShader[rand() % 4], 0); //cgs.media.glassFragments[0] );
	le->leFlags |= LEF_DIEONIMPACT;
	CG_ParticleExplode( origin, from, normal, parts, 25.0f, le );
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : surfaceEffectMetal
// Description: Metal was hit.. make some sparks and smoke
// Author	  : Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
void surfaceEffectMetal ( vec3_t origin, vec3_t from, vec3_t normal, int severity )
{
	localEntity_t  *le;

	if (cg_sfxParticles.integer)
	{
		le = CG_ParticleCreate ( );
		CG_ParticleSetAttributes ( le, PART_ATTR_SPARK, cgs.media.tracerShader, 0, 0 );
		CG_ParticleExplode( origin, from, normal, rand() % 3 + 1, 30.0f, le );

		//	  if (rand()%2==1)
		{
			localEntity_t  *ex;
			ex			 = CG_AllocLocalEntity();
			ex->leType		 = LE_SPRITE_EXPLOSION;
			ex->refEntity.radius = 20 + rand() % 10;

			// randomly rotate sprite orientation
			ex->refEntity.rotation = cg.time % 360;
			ex->startTime		   = cg.time;
			ex->endTime 		   = ex->startTime + 266;

			// bias the time so all shader effects start correctly
			ex->refEntity.shaderTime   = ex->startTime / 1000.0f;
			ex->refEntity.hModel	   = 0;
			ex->refEntity.customShader = cgs.media.sparkBoom;

			// set origin
			VectorCopy( origin, ex->refEntity.origin );
			VectorCopy( origin, ex->refEntity.oldorigin );
			ex->color[0] = ex->color[1] = ex->color[2] = 1.0;
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : surfaceEffectWood
// Description: Wood was hit, kick up some smoke and some wood fragments
// Author	  : Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
void surfaceEffectWood ( vec3_t origin, vec3_t from, vec3_t normal, int severity )
{
	localEntity_t  *le;

	if (cg_sfxParticles.integer)
	{
		le = CG_ParticleCreate ( );
		CG_ParticleSetAttributes ( le, PART_ATTR_WOOD, cgs.media.UT_ParticleShader, 0, cgs.media.glassFragments[0] );
		CG_ParticleExplode( origin, from, normal, rand() % 3 + 1, 40.0f, le );
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : surfaceEffectGlass
// Description: Wood was hit, kick up some smoke and some wood fragments
// Author	  : Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
void surfaceEffectGlass ( vec3_t origin, vec3_t from, vec3_t normal, int severity )
{
	localEntity_t  *le;

	if (cg_sfxParticles.integer)
	{
		le = CG_ParticleCreate ( );
		CG_ParticleSetAttributes ( le, PART_ATTR_GLASS, cgs.media.UT_ParticleShader, 0, cgs.media.glassFragments[0] );
		CG_ParticleExplode( origin, from, normal, rand() % 3 + 1, 40.0f, le );
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : surfaceEffectStone
// Description: Stome was hit, kick up some smoke and some stone fragments
// Author	  : Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
void surfaceEffectStone ( vec3_t origin, vec3_t from, vec3_t normal, int severity )
{
	localEntity_t  *le;

	if (cg_sfxParticles.integer)
	{
		le = CG_ParticleCreate ( );
		CG_ParticleSetAttributes ( le, PART_ATTR_STONE, cgs.media.UT_ParticleShader, 0, cgs.media.glassFragments[0] );

		CG_ParticleExplode( origin, from, normal, (rand() % 3) + 1, 0.01f, le );

		le = CG_ParticleCreate ( );
		CG_ParticleSetAttributes ( le, PART_ATTR_SPARK, cgs.media.tracerShader, 0, 0 );

		CG_ParticleExplode( origin, from, normal, rand() % 3 + 1, 40.0f, le );

//	if (rand()%3==1)
		{
			localEntity_t  *ex;
			ex			 = CG_AllocLocalEntity();
			ex->leType		 = LE_SPRITE_EXPLOSION;
			ex->refEntity.radius = 20 + rand() % 10;

			// randomly rotate sprite orientation
			ex->refEntity.rotation = cg.time % 360;
			ex->startTime		   = cg.time;
			ex->endTime 		   = ex->startTime + 266;

			// bias the time so all shader effects start correctly
			ex->refEntity.shaderTime   = ex->startTime / 1000.0f;
			ex->refEntity.hModel	   = 0;
			ex->refEntity.customShader = cgs.media.sparkBoom;

			// set origin
			VectorCopy( origin, ex->refEntity.origin );
			VectorCopy( origin, ex->refEntity.oldorigin );
			ex->color[0] = ex->color[1] = ex->color[2] = 1.0;
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : surfaceEffectGravel
// Description: Gravel, pieces of stone and some dirt
// Author	  : Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
void surfaceEffectGravel ( vec3_t origin, vec3_t from, vec3_t normal, int severity )
{
	localEntity_t  *le;

	if (cg_sfxParticles.integer)
	{
		le = CG_ParticleCreate ( );
		CG_ParticleSetAttributes ( le, PART_ATTR_GRAVEL, cgs.media.UT_chunkShader, 0, 0 );
		CG_ParticleExplode( origin, from, normal, rand() % 3 + 1, 130.0f, le );

		le = CG_ParticleCreate ( );
		CG_ParticleSetAttributes ( le, PART_ATTR_DIRTKICKUP, cgs.media.UT_ParticleShader, 0, 0 );
		CG_ParticleExplode( origin, from, normal, rand() % 3 + 1, 130.0f, le );
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : surfaceEffectGrass
// Description: Ummm... grass
// Author	  : Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
void surfaceEffectGrass ( vec3_t origin, vec3_t from, vec3_t normal, int severity )
{
	localEntity_t  *le;

	if (cg_sfxParticles.integer)
	{
		le = CG_ParticleCreate ( );
		CG_ParticleSetAttributes ( le, PART_ATTR_GRASS, cgs.media.UT_grassShader, 0, 0 );
		CG_ParticleExplode( origin, from, normal, rand() % 3 + 1, 150.0f, le );

		le = CG_ParticleCreate ( );
		CG_ParticleSetAttributes ( le, PART_ATTR_DIRTKICKUP, cgs.media.UT_ParticleShader, 0, 0 );
		CG_ParticleExplode( origin, from, normal, rand() % 3 + 1, 130.0f, le );
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : surfaceEffectSnow
// Description: Snow drifts, etc
// Author	  : Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
void surfaceEffectSnow ( vec3_t origin, vec3_t from, vec3_t normal, int severity )
{
	localEntity_t  *le;

	if (cg_sfxParticles.integer)
	{
		le = CG_ParticleCreate ( );
		CG_ParticleSetAttributes ( le, PART_ATTR_SNOW, cgs.media.snowShader, 0, 0 );
		CG_ParticleExplode( origin, from, normal, (rand() % 4) + 1, 150.0f, le );
	}
}

typedef void (*PFNSURFACEEFFECT)( vec3_t origin, vec3_t from, vec3_t normal, int severity );

typedef struct
{
	qhandle_t  markShader;
	int    impactSoundCount;
	qhandle_t  impactSounds[5];
	int    walkSoundCount;
	qhandle_t  walkSounds[5];
} surfaceEffectMedia;

typedef struct
{
	char			*markShader;
	vec4_t			markColor;
	vec4_t			smokeColor;
	char			*impactSounds[5];
	char			*walkSounds[5];
	PFNSURFACEEFFECT	effectFunc;
	float			marksize;
	surfaceEffectMedia	media;
} surfaceEffect;

surfaceEffect  g_surfaceEffects [] = {
	// default
	{
		"gfx/damage/default",
		{	 0, 	0,	   0,	 1 },
		{	 0, 	0,	   0,  .8f },
		{
			"sound/surfaces/bullets/concrete1.wav",
			"sound/surfaces/bullets/concrete2.wav",
			"sound/surfaces/bullets/concrete3.wav"
		},
		{
			"sound/surfaces/footsteps/boot1.wav",
			"sound/surfaces/footsteps/boot2.wav",
			"sound/surfaces/footsteps/boot3.wav",
			"sound/surfaces/footsteps/boot4.wav",
		},
		0, 1.4f,
	},

	// Tin
	{
		"gfx/damage/bullethole_metal1",
		{  .5f,   .5f,	 .5f,	 1 },
		{  .2f,   .2f,	 .2f,  .8f },
		{
			"sound/surfaces/bullets/metal1.wav",
			"sound/surfaces/bullets/metal2.wav",
			"sound/surfaces/bullets/metal3.wav"
		},
		{
			"sound/player/footsteps/clank1.wav",
			"sound/player/footsteps/clank2.wav",
			"sound/player/footsteps/clank3.wav",
			"sound/player/footsteps/clank4.wav",
		},
		surfaceEffectMetal, 2.4f,
	},

	// Aluminum
	{
		"gfx/damage/bullethole_metal1",
		{  .5f,   .5f,	 .5f,	 1 },
		{  .2f,   .2f,	 .2f,  .8f },
		{
			"sound/surfaces/bullets/metal1.wav",
			"sound/surfaces/bullets/metal2.wav",
			"sound/surfaces/bullets/metal3.wav"
		},
		{
			"sound/player/footsteps/clank1.wav",
			"sound/player/footsteps/clank2.wav",
			"sound/player/footsteps/clank3.wav",
			"sound/player/footsteps/clank4.wav",
		},
		surfaceEffectMetal, 2.4f,
	},

	// Iron
	{
		"gfx/damage/metaldent",
		{  .5f,   .5f,	 .5f,	 1 },
		{  .2f,   .2f,	 .2f,  .8f },
		{
			"sound/surfaces/bullets/metal1.wav",
			"sound/surfaces/bullets/metal2.wav",
			"sound/surfaces/bullets/metal3.wav"
		},
		{
			"sound/player/footsteps/clank1.wav",
			"sound/player/footsteps/clank2.wav",
			"sound/player/footsteps/clank3.wav",
			"sound/player/footsteps/clank4.wav",
		},
		surfaceEffectMetal, 2.4f,
	},

	// Titanium
	{
		"gfx/damage/metaldent",
		{  .5f,   .5f,	 .5f,	 1 },
		{  .2f,   .2f,	 .2f,  .8f },
		{
			"sound/surfaces/bullets/metal1.wav",
			"sound/surfaces/bullets/metal2.wav",
			"sound/surfaces/bullets/metal3.wav"
		},
		{
			"sound/player/footsteps/clank1.wav",
			"sound/player/footsteps/clank2.wav",
			"sound/player/footsteps/clank3.wav",
			"sound/player/footsteps/clank4.wav",
		},
		surfaceEffectMetal, 2.4f,
	},

	// Steel
	{
		"gfx/damage/metaldent",
		{  .5f,   .5f,	 .5f,	 1 },
		{  .2f,   .2f,	 .2f,  .8f },
		{
			"sound/surfaces/bullets/metal1.wav",
			"sound/surfaces/bullets/metal2.wav",
			"sound/surfaces/bullets/metal3.wav"
		},
		{
			"sound/player/footsteps/clank1.wav",
			"sound/player/footsteps/clank2.wav",
			"sound/player/footsteps/clank3.wav",
			"sound/player/footsteps/clank4.wav",
		},
		surfaceEffectMetal, 2.4f,
	},

	// Brass
	{
		"gfx/damage/bullethole_metal1",
		{  .5f,   .5f,	 .5f,	 1 },
		{  .2f,   .2f,	 .2f,  .8f },
		{
			"sound/surfaces/bullets/metal1.wav",
			"sound/surfaces/bullets/metal2.wav",
			"sound/surfaces/bullets/metal3.wav"
		},
		{
			"sound/player/footsteps/clank1.wav",
			"sound/player/footsteps/clank2.wav",
			"sound/player/footsteps/clank3.wav",
			"sound/player/footsteps/clank4.wav",
		},
		surfaceEffectMetal, 2.4f,
	},

	// Copper
	{
		"gfx/damage/bullethole_metal1",
		{  .5f,   .5f,	 .5f,	 1 },
		{  .2f,   .2f,	 .2f,  .8f },
		{
			"sound/surfaces/bullets/metal1.wav",
			"sound/surfaces/bullets/metal2.wav",
			"sound/surfaces/bullets/metal3.wav"
		},
		{
			"sound/player/footsteps/clank1.wav",
			"sound/player/footsteps/clank2.wav",
			"sound/player/footsteps/clank3.wav",
			"sound/player/footsteps/clank4.wav",
		},
		surfaceEffectMetal, 2.4f,
	},

	// Cement
	{
		"gfx/damage/concrete",
		{	 0, 	0,	   0,	 1 },
		{	 1, 	1,	   1,  .8f },
		{
			"sound/surfaces/bullets/concrete1.wav",
			"sound/surfaces/bullets/concrete2.wav",
			"sound/surfaces/bullets/concrete3.wav"
		},
		{
			"sound/surfaces/footsteps/concrete1.wav",
			"sound/surfaces/footsteps/concrete2.wav",
			"sound/surfaces/footsteps/concrete3.wav",
			"sound/surfaces/footsteps/concrete4.wav",
		},
		surfaceEffectStone, 2.4f,
	},

	// Rock
	{
		"gfx/damage/concrete",
		{	 0, 	0,	   0,	 1 },
		{	 1, 	1,	   1,  .8f },
		{
			"sound/surfaces/bullets/concrete1.wav",
			"sound/surfaces/bullets/concrete2.wav",
			"sound/surfaces/bullets/concrete3.wav"
		},
		{
			"sound/surfaces/footsteps/concrete1.wav",
			"sound/surfaces/footsteps/concrete2.wav",
			"sound/surfaces/footsteps/concrete3.wav",
			"sound/surfaces/footsteps/concrete4.wav",
		},
		surfaceEffectStone, 2.4f,
	},

	// Gravel
	{
		"gfx/damage/default",
		{	 0, 	0,	   0,	 1 },
		{	 1, 	1,	   1,  .8f },
		{
			"sound/surfaces/bullets/gravel1.wav",
			"sound/surfaces/bullets/gravel2.wav",
			"sound/surfaces/bullets/gravel3.wav"
		},
		{
			"sound/surfaces/footsteps/gravel1.wav",
			"sound/surfaces/footsteps/gravel2.wav",
			"sound/surfaces/footsteps/gravel3.wav",
			"sound/surfaces/footsteps/gravel4.wav",
		},
		surfaceEffectGravel, 1.4f,
	},

	// Pavement
	{
		"gfx/damage/concrete",
		{	 0, 	0,	   0,	 1 },
		{	 1, 	1,	   1,  .8f },
		{
			"sound/surfaces/bullets/concrete1.wav",
			"sound/surfaces/bullets/concrete2.wav",
			"sound/surfaces/bullets/concrete3.wav"
		},
		{
			"sound/surfaces/footsteps/concrete1.wav",
			"sound/surfaces/footsteps/concrete2.wav",
			"sound/surfaces/footsteps/concrete3.wav",
			"sound/surfaces/footsteps/concrete4.wav",
		},
		surfaceEffectStone, 2.4f,
	},

	// Brick
	{
		"gfx/damage/concrete",
		{	 0, 	0,	   0,	 1 },
		{	 1, 	1,	   1,  .8f },
		{
			"sound/surfaces/bullets/concrete1.wav",
			"sound/surfaces/bullets/concrete2.wav",
			"sound/surfaces/bullets/concrete3.wav"
		},
		{
			"sound/surfaces/footsteps/concrete1.wav",
			"sound/surfaces/footsteps/concrete2.wav",
			"sound/surfaces/footsteps/concrete3.wav",
			"sound/surfaces/footsteps/concrete4.wav",
		},
		surfaceEffectStone, 2.4f,
	},

	// Clay
	{
		"gfx/damage/dirt",
		{	 0, 	0,	   0,	 1 },
		{	 1, 	1,	   1,  .8f },
		{
			"sound/surfaces/bullets/concrete1.wav",
			"sound/surfaces/bullets/concrete2.wav",
			"sound/surfaces/bullets/concrete3.wav"
		},
		{
			"sound/surfaces/footsteps/concrete1.wav",
			"sound/surfaces/footsteps/concrete2.wav",
			"sound/surfaces/footsteps/concrete3.wav",
			"sound/surfaces/footsteps/concrete4.wav",
		},
		surfaceEffectStone, 3.4f,
	},

	// Grass
	{
		"gfx/damage/grass",
		{ 0.2f, 0.15f, 0.02f, 1.0f },
		{ 0.2f, 0.15f, 0.02f, 0.8f },
		{ NULL },
		{
			"sound/surfaces/footsteps/grass1.wav",
			"sound/surfaces/footsteps/grass2.wav",
			"sound/surfaces/footsteps/grass3.wav",
			"sound/surfaces/footsteps/grass4.wav",
		},
		surfaceEffectGrass, 3.4f,
	},

	// Dirt
	{
		"gfx/damage/dirt",
		{ 0.2f, 0.15f, 0.02f, 1.0f },
		{ 0.2f, 0.15f, 0.02f, 0.8f },
		{ NULL },
		{ NULL },
		0, 3.4f,
	},

	// Mud
	{
		"gfx/damage/dirt",
		{ 0.2f, 0.15f, 0.02f, 1.0f },
		{ 0.2f, 0.15f, 0.02f, 0.8f },
		{ NULL },
		{
			"sound/surfaces/footsteps/mud1.wav",
			"sound/surfaces/footsteps/mud2.wav",
			"sound/surfaces/footsteps/mud3.wav",
			"sound/surfaces/footsteps/mud4.wav",
		},

		0, 2.4f,
	},

	// Snow
	{
		"gfx/damage/snow",
		{  .6f,   .6f,	.65f,	 1 },
		{  .9f,   .9f,	.92f, 0.8f },
		{ NULL },
		{
			"sound/surfaces/footsteps/snow1.wav",
			"sound/surfaces/footsteps/snow2.wav",
			"sound/surfaces/footsteps/snow3.wav",
			"sound/surfaces/footsteps/snow4.wav",
		},
		surfaceEffectSnow, 3.4f,
	},

	// Ice
	{
		"gfx/damage/default",
		{  .6f,   .6f,	.65f,	 1 },
		{  .9f,   .9f,	.92f, 0.8f },
		{ NULL },
		{ NULL },
		0, 2.4f,
	},

	// Sand
	{
		"gfx/damage/sand",
		{ 0.2f, 0.15f, 0.02f, 0.1f },
		{ 0.2f, 0.15f, 0.02f, 0.8f },
		{ NULL },
		{ NULL },
		0, 2.4f,
	},

	// Ceramic Tile
	{
		"gfx/damage/plaster",
		{	 0, 	0,	   0,	 1 },
		{	 1, 	1,	   1, 0.8f },
		//{ NULL },
		//{ NULL },
		{
			"sound/surfaces/bullets/glass1.wav",
		},
		{
			"sound/surfaces/footsteps/concrete1.wav",
			"sound/surfaces/footsteps/concrete2.wav",
			"sound/surfaces/footsteps/concrete3.wav",
			"sound/surfaces/footsteps/concrete4.wav",
		},
		0, 3.4f,
	},

	// Linoleum
	{
		"gfx/damage/default",
		{	 0, 	0,	   0,	 1 },
		{	 1, 	1,	   1, 0.8f },
		{ NULL },
		{ NULL },
		0, 2.4f,
	},

	// Rug
	{
		"gfx/damage/default",
		{	 0, 	0,	   0,	 1 },
		{	 1, 	1,	   1, 0.8f },
		{ NULL },
		{ NULL },
		0, 1.4f,
	},

	// Plaster
	{
		"gfx/damage/plaster",
		{	 0, 	0,	   0,	 1 },
		{	 1, 	1,	   1,  .8f },
		{
			"sound/surfaces/bullets/concrete1.wav",
			"sound/surfaces/bullets/concrete2.wav",
			"sound/surfaces/bullets/concrete3.wav"
		},
		{
			"sound/surfaces/footsteps/concrete1.wav",
			"sound/surfaces/footsteps/concrete2.wav",
			"sound/surfaces/footsteps/concrete3.wav",
			"sound/surfaces/footsteps/concrete4.wav",
		},
		surfaceEffectStone, 2.4f,
	},

	// Plastic
	{
		"gfx/damage/default",
		{	 0, 	0,	   0,	 1 },
		{	 1, 	1,	   1, 0.8f },
		//{ NULL },
		//{ NULL },
		{
			"sound/surfaces/bullets/concrete1.wav",
			"sound/surfaces/bullets/concrete2.wav",
			"sound/surfaces/bullets/concrete3.wav"
		},
		{
			"sound/surfaces/footsteps/concrete1.wav",
			"sound/surfaces/footsteps/concrete2.wav",
			"sound/surfaces/footsteps/concrete3.wav",
			"sound/surfaces/footsteps/concrete4.wav",
		},
		0, 1.4f,
	},

	// Cardboard
	{
		"gfx/damage/default",
		{	 0, 	0,	   0,	 1 },
		{	 1, 	1,	   1, 0.8f },
		{ NULL },
		{ NULL },
		0, 2.4f,
	},

	// Hard wood
	{
		"gfx/damage/wood",
		{	 0, 	0,	   0,	 1 },
		{	 0, 	0,	   0, 0.8f },
		{
			"sound/surfaces/bullets/wood1.wav",
			"sound/surfaces/bullets/wood2.wav",
			"sound/surfaces/bullets/wood3.wav"
		},
		{
			"sound/surfaces/footsteps/wood1.wav",
			"sound/surfaces/footsteps/wood2.wav",
			"sound/surfaces/footsteps/wood3.wav",
			"sound/surfaces/footsteps/wood4.wav",
		},
		surfaceEffectWood, 2.4f,
	},

	// Soft wood
	{
		"gfx/damage/wood",
		{	 0, 	0,	   0,	 1 },
		{	 0, 	0,	   0, 0.8f },
		{
			"sound/surfaces/bullets/wood1.wav",
			"sound/surfaces/bullets/wood2.wav",
			"sound/surfaces/bullets/wood3.wav"
		},
		{
			"sound/surfaces/footsteps/wood1.wav",
			"sound/surfaces/footsteps/wood2.wav",
			"sound/surfaces/footsteps/wood3.wav",
			"sound/surfaces/footsteps/wood4.wav",
		},
		surfaceEffectWood, 2.4f,
	},

	// Plank
	{
		"gfx/damage/wood",
		{	 0, 	0,	   0, 1.0f },
		{	 0, 	0,	   0, 1.0f },
		{
			"sound/surfaces/bullets/wood1.wav",
			"sound/surfaces/bullets/wood2.wav",
			"sound/surfaces/bullets/wood3.wav"
		},
		{
			"sound/surfaces/footsteps/wood1.wav",
			"sound/surfaces/footsteps/wood2.wav",
			"sound/surfaces/footsteps/wood3.wav",
			"sound/surfaces/footsteps/wood4.wav",
		},
		surfaceEffectWood, 2.4f,
	},

	// Glass
	{
		"gfx/damage/nonbreakglass",
		{	 0, 	0,	   0,	 1 },
		{	 0, 	0,	   0, 0.8f },
		{
			"sound/surfaces/bullets/glass1.wav",
		},
		{ NULL },
		0, 6.0f,
	},

	// Water
	{
		NULL,
		{  0 },
		{  0 },
		{ NULL },
		{ NULL },
		0, 1.4f,
	},

	// Stucco
	{
		"gfx/damage/plaster",
		{	 0, 	0,	   0,	 1 },
		{	 1, 	1,	   1,  .8f },
		{
			"sound/surfaces/bullets/concrete1.wav",
			"sound/surfaces/bullets/concrete2.wav",
			"sound/surfaces/bullets/concrete3.wav"
		},
		{
			"sound/surfaces/footsteps/concrete1.wav",
			"sound/surfaces/footsteps/concrete2.wav",
			"sound/surfaces/footsteps/concrete3.wav",
			"sound/surfaces/footsteps/concrete4.wav",
		},
		surfaceEffectStone, 2.4f,
	},

	// Flesh
	{
		NULL,
		{  0 },
		{  0 },
		{
			"sound/body_impact.wav",
		},
		{ NULL },
		surfaceEffectFlesh, 1.4f,
	},

	// Head
	{
		NULL,
		{  0 },
		{  0 },
		{
			"sound/headshot.wav",
		},
		{ NULL },
		surfaceEffectFlesh, 1.4f,
	},

	// Helmet
	{
		NULL,
		{  0 },
		{	 0, 	0,	   0,  .8f },
		{
			"sound/helmethit.wav",
		},
		{ NULL },
		surfaceEffectMetal, 1.4f,
	},

	// Kevlar
	{
		NULL,
		{  0 },
		{	 0, 	0,	   0,  .8f },
		{
			"sound/vesthit.wav",
		},
		{ NULL },
		0, 1.4f,
	},

	// Ladder
	{
		NULL,
		{  0 },
		{	 0, 	0,	   0,  .8f },
		{
			NULL
		},
		{
			"sound/climb/ladder1.wav",
			"sound/climb/ladder2.wav",
			"sound/climb/ladder3.wav",
			"sound/climb/ladder4.wav",
		},
		0, 1.4f,
	},

	// Breakable
	{
		"gfx/damage/default",
		{	 0, 	0,	   0,	 1 },
		{ 0.5f,  0.5f,	0.5f,  .8f },
		{
			"sound/surfaces/bullets/glass1.wav",
		},
		{
			NULL,
		},
		0, 1.4f,
	},

	// Breakable Glass
	{
		"gfx/damage/bullet_mrk_glass",
		{	 0, 	0,	   0,	 1 },
		{	 0, 	0,	   0, 0.8f },
		{
			"sound/surfaces/bullets/glass1.wav",
		},
		{ NULL },
		surfaceEffectGlass, 2.0f,
	},
	{	//Wall sliding
		NULL,
		{  0 },
		{	 0, 	0,	   0,  .8f },
		{
			NULL
		},
		{
			"sound/climb/wallslide1.wav",
			"sound/climb/wallslide2.wav",
			"sound/climb/wallslide3.wav",
			"sound/climb/wallslide4.wav",
		},
		0, 1.4f,
	}
};

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : surfaceImpactSound
// Description: Plays the impact sound for the given surface type
// Author	  : Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
qboolean surfaceImpactSound ( vec3_t origin, int entityNum, int surfaceType )
{
	surfaceEffect  *effect = &g_surfaceEffects[surfaceType];

	// IF at leat one sound was set
	if (effect->media.impactSoundCount != 0)
	{
		// Play a random sound
		trap_S_StartSound ( origin, entityNum, CHAN_FX, effect->media.impactSounds[rand() % effect->media.impactSoundCount] );

		return qtrue;
	}

	return qfalse;
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : surfaceBulletHit
// Description: Renders bullet hit effects based on the surface that was hit
// Author	  : Apoxol
///////////////////////////////////////////////////////////////////////////////////////////

void surfaceBulletHit ( vec3_t origin, vec3_t from, vec3_t normal, int surfaces, int severity, centity_t *parent )
{
	surfaceEffect  *effect;
	qboolean	   water;
	vec3_t		   dirLight;
	float		   Color[3] = { 1, 1, 1};

	int 	   surface	= SURFACE_GET_TYPE(surfaces);

//

	if (severity == 0)
	{
		if (rand() % 5 > 1)
		{
			return;
		}
	}

//	 if (surface==0) surface=cgs.surfacedefault;
	effect = &g_surfaceEffects[surface];
	water  = (CG_PointContents ( origin, -1 ) & CONTENTS_WATER) ? qtrue : qfalse;

	if (cg_sfxSurfaceImpacts.integer && !water)
	{
		// Call the effect func if one is specified
		if (effect->effectFunc)
		{
			effect->effectFunc ( origin, from, normal, severity );
		}
		//	effect->effectFunc ( from, from, normal, severity );

		// Add some smoke if the smoke color alpha channel isnt zero
		if (effect->smokeColor[3] > 0.0f)
		{
			//	surfaceEffectSmoke ( origin, from, normal, 2.5f + (float)(rand()%17), effect->smokeColor[0], effect->smokeColor[1], effect->smokeColor[2], effect->smokeColor[3] );
		}
	}

	// Add the impact mark
	if(effect->markShader)
	{
		VectorAdd(origin, normal, origin);
		BuildLightFloat(origin, Color, dirLight);
//	trap_R_LightForPoint( origin, ambientLight, dirLight, Lightdir );

		//	  dirLight[0]/=255;
		//	dirLight[1]/=255;
		//	dirLight[2]/=255;
		// Draw it

		CG_ImpactMark( effect->media.markShader, origin,
//					  normal, random()*360, effect->markColor[0], effect->markColor[1], effect->markColor[2], effect->markColor[3],

				   normal, random() * 360, dirLight[0], dirLight[1], dirLight[2], effect->markColor[3],
				   qtrue, effect->marksize, qfalse, parent );
	}

	surfaceImpactSound ( origin, ENTITYNUM_WORLD, SURFACE_GET_TYPE(surfaces));
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : surfaceFootstep
// Description: Plays a footstep sound for the given surface
// Author	  : Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
void surfaceFootstep ( int entity, int surfaceType )
{
	surfaceEffect  *effect;
	qhandle_t	   sfxnum;

//	 if (surfaceType==0) surfaceType=cgs.surfacedefault;

	effect = &g_surfaceEffects[surfaceType];

	if (effect->media.walkSoundCount == 0)
	{
		//Switch to default boot noises
		effect = &g_surfaceEffects[0];
	}

	if (effect->media.walkSoundCount)
	{
		sfxnum = effect->media.walkSounds[rand() % effect->media.walkSoundCount] ;

		if (sfxnum)
		{
			trap_S_StartSound (NULL, entity, CHAN_BODY, sfxnum);
		}
	}
}

// 27 -  Once upon a time these were registered dynamically and getting stutters and shit.	Fixed for 2.4
void preloadAllSurfaceEffects(void)
{
	int 	   j, sound;
	surfaceEffect  *effect;

	for (j = 0; j < sizeof(g_surfaceEffects) / sizeof(g_surfaceEffects[0]); j++) // 39 effects atm.  Will fix this
	{
		effect = &g_surfaceEffects[j];

		//sound
		if (effect->media.walkSoundCount == 0)
		{
			for(sound = 0; sound < 5 && effect->walkSounds[sound]; sound++)
			{
				effect->media.walkSounds[sound] = trap_S_RegisterSound ( effect->walkSounds[sound], qfalse );

				if (effect->media.walkSounds[sound] != 0)
				{
					effect->media.walkSoundCount++;
				}
			}
		}

		//mark
		if	((effect->media.markShader == 0) && (effect->markShader != NULL))
		{
			effect->media.markShader = trap_R_RegisterShader ( effect->markShader );
		}

		//sounds
		if (effect->media.impactSoundCount == 0)
		{
			for(sound = 0; sound < 5 && effect->impactSounds[sound]; sound++)
			{
				effect->media.impactSounds[sound] = trap_S_RegisterSound ( effect->impactSounds[sound], qfalse);

				if (effect->media.impactSounds[sound] != 0)
				{
					effect->media.impactSoundCount++;
				}
			}
		}
	}
}
