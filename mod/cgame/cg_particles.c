/*
	Handles the creation and movement of Urban Terror particles
	CG_AddFragment is used to do the actual particle rendering

	Normal particle usage is:

	localEntity_t	*le;		// note: each particle is a localentity

	le = CG_ParticleCreate ( );
	CG_ParticleSetAttributes ( &le, PART_ATTR_SPARK );
	CG_ParticleExplode( origin, ..., &le );

	if you call particle create and don't call particleExplode or particle launch then
	you'll get unpredictable effects
*/

#include "cg_local.h"

// pre-defined particle attributes
particle_t	p_attr[] = {
	{	  0 },

	// PART_ATTR_SPARK
	{
		0,						// particle flags
		qtrue,					// trail
		qtrue,					// bounce
		qfalse, 				// fade

		TR_EJECTA,				// trtype
		0.2f,					// lightness
		100,					// time to live
		0.0f,					// deviation

		1.0f,					// minradius
		1.0f,					// maxradius
		600,					// min velocity
		800,					// max velocity
		1.0f,					// 'up' scale

		0,						// mark radius
		{ 0.0f, 0.0f, 0.0f, 0.0f }, 		// mark colour
		{ 0.0f, 0.0f, 0.0f, 0.0f }, 		// particle colour modulation

		{	  0 }				// smoke trail data
	},

	// PART_ATTR_BLOOD
	{
		PART_FLAG_MARK | PART_FLAG_DIEONSTOP,
		qfalse, 				// trail
		qfalse, 				// bounce
		qfalse, 				// fade

		TR_GRAVITY, 			// trtype
		1.0f,					// lightness
		60000,					// time to live
		0.0f,					// deviation

		25.0f,					// minradius
		30.0f,					// maxradius
		200,					// min velocity
		400,					// max velocity
		1.0f,					// 'up' scale

		-0.35f, 					// mark radius (0 means use particle radius, -tve is part radius multiplier)
		{ 1.0f, 1.0f, 1.0f, 1.0f }, // mark colour
		{ 0.0f, 0.0f, 0.0f, 0.0f }, // particle colour modulation

		{					// smoke
			80.0f / 100.0f, 	// chance
			3500 / 8,			// interval
			10.0f / 100.0f, 	// percent of duration to trail smoke
			-25.0f, 			// float up or down rate
			3.0f,				// thickness (radius)
			{0.3f, 0.0f, 0.0f, 0.85f }	// smoke colour
		}
	},

	// PART_ATTR_BLOODTRICKLE
	{
		PART_FLAG_MARK | PART_FLAG_DIEONSTOP,
		qfalse, 				// trail
		qfalse, 				// bounce
		qfalse, 				// fade

		TR_GRAVITY, 			// trtype
		1.0f,					// lightness
		3500,					// time to live
		0.0f,					// deviation

		1.0f,					// minradius
		6.0f,					// maxradius
		20, 					// min velocity
		40, 					// max velocity
		1.0f,					// 'up' scale

		-1.5f,					// mark radius (0 means use particle radius)
		{ 1.0f, 1.0f, 1.0f, 1.0f }, 		// mark colour
		{ 0.0f, 0.0f, 0.0f, 0.0f }, 		// particle colour modulation

		{	  0 }
	},

	// PART_ATTR_BLOODTRICKLE_INVIS
	{
		PART_FLAG_MARK | PART_FLAG_DIEONSTOP | PART_FLAG_INVISIBLE,
		qfalse, 				// trail
		qfalse, 				// bounce
		qfalse, 				// fade

		TR_GRAVITY, 			// trtype
		1.0f,					// lightness
		3500,					// time to live
		0.0f,					// deviation

		1.0f,					// minradius
		6.0f,					// maxradius
		20, 					// min velocity
		40, 					// max velocity
		1.0f,					// 'up' scale

		-1.5f,					// mark radius (0 means use particle radius)
		{ 1.0f, 1.0f, 1.0f, 1.0f }, 		// mark colour
		{ 0.0f, 0.0f, 0.0f, 0.0f }, 		// particle colour modulation

		{	  0 }
	},

	// PART_ATTR_STONE
	{
		PART_FLAG_TRAIL | PART_FLAG_BOUNCE, 	// particle flags
		qfalse, 					// trail
		qtrue,						// bounce
		qfalse, 					// fade

		TR_GRAVITY, 			// trtype
		1.0f,					// lightness
		800,					// time to live
		0.0f,					// deviation

		1.5f,					// minradius
		3.0f,					// maxradius
		100,					// min velocity
		200,					// max velocity
		100.0f, 				// 'up' scale

		0,						// mark radius
		{ 0.0f, 0.0f, 0.0f, 0.0f }, 		// mark colour
		{ 0.75f, 0.75f, 0.75f, 1.0f },			// particle colour modulation(needs a shader with rgbgen entity)

		{					// smoke
			60.0f / 100.0f, 		// chance
			800 / 5,			// interval
			30.0f / 100.0f, 		// percent of life to run
			15.0f,				// up/down drift
			3.0f,				// thickness
			{1.0f, 1.0f, 1.0f, 0.5f }	// smoke colour
		}
	},

	// PART_ATTR_WOOD
	{
		PART_FLAG_BOUNCE,		// particle flags
		qfalse, 				// trail
		qtrue,					// bounce
		qfalse, 				// fade

		TR_GRAVITY2,			// trtype
		0.8f,					// lightness
		750,					// time to live
		0.0f,					// deviation

		1.0f,					// minradius
		2.0f,					// maxradius
		50, 					// min velocity
		150,					// max velocity
		100.0f, 				// 'up' scale

		0,						// mark radius
		{ 0.0f, 0.0f, 0.0f, 0.0f }, 		// mark colour
		{ 0.75f, 0.6f, 0.5f, 1.0f },		// particle colour modulation(needs a shader with rgbgen entity)

		{	  0 }
	},

	// PART_ATTR_SPLASH
	{
		PART_FLAG_FLOATS,					// particle flags
		qtrue,								// trail
		qfalse, 							// bounce
		qfalse, 							// fade

		TR_GRAVITY2,						// trtype
		0.8f,								// lightness
		1000,								// time to live
		0.0f,								// deviation

		1.0f,								// minradius
		2.0f,								// maxradius
		50.0f,								// min velocity
		150.0f, 							// max velocity
		250.0f, 							// 'up' scale

		0,									// mark radius
		{ 0.0f, 0.0f, 0.0f, 0.0f }, 		// mark colour
		{ 0.0f, 0.0f, 0.0f, 0.0f }, 		// particle colour modulation (needs a shader with rgbgen entity)

		{	  0 }
	},

	// PART_ATTR_EXPLOSION
	{
		PART_FLAG_DIEONSTOP,	// particle flags
		qtrue,					// trail
		qtrue,					// bounce
		qfalse, 				// fade

		TR_EJECTA,				// trtype
		0.2f,					// lightness
		800,					// time to live
		100.0f, 				// deviation

		2.0f,					// minradius
		2.0f,					// maxradius
		800,					// min velocity
		1000,					// max velocity
		1.0f,					// 'up' scale

		0,						// mark radius
		{ 0.0f, 0.0f, 0.0f, 0.0f }, 		// mark colour
		{ 0.0f, 0.0f, 0.0f, 0.0f }, 		// particle colour modulation(needs a shader with rgbgen entity)

		{ 0 }
	},

	// PART_ATTR_EXPLOSION_CHUNKS
	{
		0,						// particle flags
		qtrue,					// trail
		qtrue,					// bounce
		qtrue,					// fade

		TR_GRAVITY, 			// trtype
		1.0f,					// lightness
		800,					// time to live
		500.0f, 				// deviation

		2.0f,					// minradius
		2.0f,					// maxradius
		100,					// min velocity
		100,					// max velocity
		1.0f,					// 'up' scale

		0,						// mark radius
		{ 0.0f, 0.0f, 0.0f, 0.0f }, 		// mark colour
		{ 1.0f, 1.0f, 1.0f, 1.0f }, 		// particle colour modulation(needs a shader with rgbgen entity)

		{
			0.999f, 			// chance
			800 / 10,			// interval
			80.0f / 100.0f, 	// percent
			-25.0f, 			// drift
			10.0f,				// thickness
			{0.1f, 0.1f, 0.1f, 1.0f }	// smoke colour
		}
	},

	// PART_ATTR_DIRTKICKUP
	{
		PART_FLAG_DIEONSTOP,	// particle flags
		qfalse, 				// trail
		qfalse, 				// bounce
		qfalse, 				// fade

		TR_EJECTA,				// trtype
		1.0f,					// lightness
		100,					// time to live
		0.0f,					// deviation

		1.0f,					// minradius
		1.0f,					// maxradius
		200,					// min velocity
		400,					// max velocity
		400.0f, 				// 'up' scale

		0,						// mark radius
		{ 0.0f, 0.0f, 0.0f, 0.0f }, 		// mark colour
		{ 1.0f, 1.0f, 1.0f, 1.0f }, 		// particle colour modulation(needs a shader with rgbgen entity)

		{
			1.0f,				// chance
			100 / 4,			// interval
			1.0f,				// percent
			-20.0f, 			// drift
			3.0f,				// thickness
			{0.15f, 0.1f, 0.05f, 0.6f } // smoke colour
		}
	},

	// PART_ATTR_GRAVEL
	{
		PART_FLAG_DIEONSTOP,	// particle flags
		qfalse, 				// trail
		qtrue,					// bounce
		qfalse, 				// fade

		TR_GRAVITY, 			// trtype
		1.0f,					// lightness
		1000,					// time to live
		0.0f,					// deviation

		2.5f,					// minradius
		4.0f,					// maxradius
		100,					// min velocity
		100,					// max velocity
		150.0f, 				// 'up' scale

		0,						// mark radius
		{ 0.0f, 0.0f, 0.0f, 0.0f }, 		// mark colour
		{ 0.25f, 0.30f, 0.25f, 1.0f },		// particle colour modulation(needs a shader with rgbgen entity)

		{ 0 }
	},

	// PART_ATTR_GRASS
	{
		PART_FLAG_DIEONSTOP,	// particle flags
		qfalse, 				// trail
		qtrue,					// bounce
		qfalse, 				// fade

		TR_FEATHER, 			// trtype
		0.3f,					// lightness
		1500,					// time to live
		0.0f,					// deviation

		2.0f,					// minradius
		5.0f,					// maxradius
		200,					// min velocity
		400,					// max velocity
		125.0f, 				// 'up' scale

		0,						// mark radius
		{ 0.0f, 0.0f, 0.0f, 0.0f }, 		// mark colour
		{ 0.0f, 0.25f, 0.05f, 1.0f },		// particle colour modulation(needs a shader with rgbgen entity)

		{ 0 }
	},

	// PART_ATTR_SNOW
	{
		PART_FLAG_DIEONSTOP,			// particle flags
		qfalse, 				// trail
		qfalse, 				// bounce
		qtrue,					// fade

		TR_GRAVITY2,			// trtype
		0.75f,					// lightness
		1500,					// time to live
		0.0f,					// deviation

		5.0f,					// minradius
		10.0f,					// maxradius
		50, 					// min velocity
		75, 					// max velocity
		200.0f, 				// 'up' scale

		0,						// mark radius
		{ 0.0f, 0.0f, 0.0f, 0.0f }, 		// mark colour
		{ 1.0f, 1.0f, 1.0f, 0.5f }, 		// particle colour modulation(needs a shader with rgbgen entity)

		{
			1.0f,				// chance
			1500 / 7,			// interval
			10.0f / 100.0f, 	// percent
			-30.0f, 			// drift
			3.0f,				// thickness
			{1.0f, 1.0f, 1.0f, 0.5f }	// smoke colour
		}
	},

	// PART_ATTR_GLASS
	{
		PART_FLAG_BOUNCE,		// particle flags
		qfalse, 				// trail
		qtrue,					// bounce
		qfalse, 				// fade

		TR_GRAVITY2,			// trtype
		0.8f,					// lightness
		500,					// time to live
		0.0f,					// deviation

		0.5f,					// minradius
		1.0f,					// maxradius
		50, 					// min velocity
		150,					// max velocity
		100.0f, 				// 'up' scale

		0,						// mark radius
		{ 0.0f, 0.0f, 0.0f, 0.0f }, 		// mark colour
		{ 1.0f, 1.0f, 1.0f, 0.8f }, 		// particle colour modulation(needs a shader with rgbgen entity)

		{	  0 }
	},

	// PART_ATTR_MAX
	{	  0 },
};

//
// CG_ParticleCreate
// initialises a basic particle
//

localEntity_t *CG_ParticleCreate ( void )
{
	refEntity_t    *re;
	localEntity_t  *ple;

	ple = CG_AllocLocalEntity();

	if(!ple)
	{
		CG_Printf( "CG_ParticleCreate: couldn't allocate an LE\n");
		return NULL;
	}

	re = &ple->refEntity;

	// init local entity data
	ple->leType 	   = LE_FRAGMENT; // particle rendering gets handled by CG_AddFragment
	ple->color[0]		   = 1.0f;
	ple->color[1]		   = 1.0f;
	ple->color[2]		   = 1.0f;
	ple->color[3]		   = 1.0f;

	ple->startTime		   = cg.time + 50; // allow 50ms for future setting of particle data
	ple->endTime		   = cg.time + 1000;
	ple->lifeRate		   = 1.0 / (ple->endTime - ple->startTime);

	ple->fadeInTime 	   = 0;
	ple->gravity		   = 1.0f;
	ple->leBounceSoundType = 0;
	ple->leFlags		   = LEF_PARTICLE;
	ple->leMarkType 	   = 0;

	ple->light			   = 0.0f;
	ple->lightColor[0]	   = 0.0f;
	ple->lightColor[1]	   = 0.0f;
	ple->lightColor[2]	   = 0.0f;

	VectorClear ( ple->pos.trBase );
	VectorClear ( ple->pos.trDelta );
	ple->pos.trDuration   = 1000;
	ple->pos.trTime 	  = ple->startTime;
	ple->pos.trType 	  = TR_GRAVITY;

	ple->radius 		  = 1.0f;
	ple->scale[0]		  = 1.0f;
	ple->scale[1]		  = 1.0f;
	ple->scale[2]		  = 1.0f;

	ple->partType		  = 0;
	ple->partMarkShader   = 0;

	ple->smoke			  = 0;
	ple->timeTilNextChild = 0;

	// init refentity data
	re->customShader	  = cgs.media.whiteShader;
	AxisClear( re->axis );
	re->hModel		  = 0;
	re->nonNormalizedAxes = qfalse;
	VectorClear( re->origin );
	VectorClear( re->oldorigin );
	re->radius	 = ple->radius;
	re->renderfx = RF_NOSHADOW;
	re->reType	 = RT_MODEL; // particles only use refEnts for drawing models

	return ple;
}

//
//	load a preset particle profile
//
void CG_ParticleSetAttributes( localEntity_t *le, int attr, qhandle_t partShader, qhandle_t markShader, qhandle_t model )
{
	particle_t	*part = &p_attr[attr];

	if ((attr < 0) || (attr >= PART_ATTR_MAX))
	{
		CG_Printf("CG_ParticleSetAttributes() called with bad PART_ATTR\n");
		return;
	}

	le->partType			= attr;

	le->refEntity.customShader	= partShader;
	le->refEntity.hModel		= model;

	le->gravity 		= part->lightness;
	le->pos.trType			= part->trType;
	le->pos.trDuration		= part->timetolive;

	le->partMarkShader		= markShader;

	le->refEntity.shaderRGBA[0] =  (unsigned char)(part->partcolor[0] * 255.0f);
	le->refEntity.shaderRGBA[1] =  (unsigned char)(part->partcolor[1] * 255.0f);
	le->refEntity.shaderRGBA[2] =  (unsigned char)(part->partcolor[2] * 255.0f);
	le->refEntity.shaderRGBA[3] =  (unsigned char)(part->partcolor[3] * 255.0f);

	if (part->partcolor[3] != 0.0f)
	{
		le->color[0] = part->partcolor[0];
		le->color[1] = part->partcolor[1];
		le->color[2] = part->partcolor[2];
		le->color[3] = part->partcolor[3];
	}
}

//
// CG_ParticleLaunch
// sets particle velocity and origin
// also sets radius so we can have multiple parts with different radii
//
// origin: where particle starts, copied to trBase
// velocity: a velocity vector, copied to trDelta
// le: a particle local entity created by CG_ParticleCreate
void CG_ParticleLaunch ( vec3_t origin, vec3_t velocity, localEntity_t *le )
{
	particle_t	*part = &p_attr[le->partType];

	if (!le)
	{
		return;
	}

	VectorCopy( origin, le->refEntity.origin );
	VectorCopy( origin, le->refEntity.oldorigin );
	VectorCopy( origin, le->pos.trBase );
	VectorCopy( velocity, le->pos.trDelta );
	le->pos.trTime = cg.time;
	le->endTime    = le->pos.trTime + le->pos.trDuration;
	le->lifeRate   = 1.0 / (le->endTime - le->startTime);

	// tumble launched models
	if (le->refEntity.hModel)
	{
		le->leFlags  |= LEF_TUMBLE;
		le->angles.trType = TR_LINEAR;

		VectorClear( le->angles.trBase );
		le->angles.trDelta[0] = rand() % 700 + 100;
		le->angles.trDelta[1] = rand() % 700 + 100;
		le->angles.trDelta[2] = rand() % 700 + 100;
		le->angles.trTime	  = cg.time;
		le->angles.trDuration = part->timetolive;
	}

	// work out particle radius to allow for random particle sizes
	le->radius = part->minRadius + (random() * (part->maxRadius - part->minRadius));

	// ensure at least a small radius: can happen if someone accidentally sets minRadius > maxRadius
	if (le->radius <= 0)
	{
		le->radius = 0.01f;
	}

	if (le->refEntity.hModel)
	{
		le->refEntity.radius = le->radius;
		le->scale[0]		 = le->scale[1] = le->scale[2] = le->radius;
	}
}

//
// CG_ParticleClone
// Returns a copy of a given localentity
// Always succeeds because AlloxLocalEntity always succeeds
//
localEntity_t *CG_ParticleClone( const localEntity_t *original )
{
	localEntity_t  *clone;
	localEntity_t  *next, *prev;

	clone = CG_AllocLocalEntity();

	// save back le->next and le->prev or LE won't be linked in right
	next = clone->next;
	prev = clone->prev;

	memcpy( clone, original, sizeof(localEntity_t));
	clone->next = next;
	clone->prev = prev;

	return clone;
}

//
// CG_ParticleExplode
// Spews out a number of particles from a point
// Used for explosions of particles (wall impacts, etc)
//
//	origin: where explosion starts
//	from:	the direction from which the impact that sets
//			off this explosion comes; can be NULL (random)
//	normal: normal of surface... if NULL assumes up
//	count:	number of particles to spawn
//	radius: radius of cone in which particles will eject: 0 is precisely along the incidence angle of from
//	le: 	the base particle to spawn

void CG_ParticleExplode (vec3_t origin,
			 vec3_t from,
			 vec3_t normal,
			 int count,
			 float radius,
			 localEntity_t *le
			 )
{
	int 	   i;
//	int 		contents;
	vec3_t		   dir;
	vec3_t		   vel;
	vec3_t		   s;
	vec3_t		   t;
	vec3_t		   temp;
	localEntity_t  *clone;

	particle_t	   *part = &p_attr[le->partType];

	// Dont do anything if the count is zero
	if(count == 0)
	{
		return;
	}

	// Get the contents so we can do some special things if water
	//contents = trap_CM_PointContents( origin, 0 );

	if (NULL == normal)
	{
		VectorSet( normal, 0.0f, 0.0f, 1.0f );
	}

	if (NULL == from)
	{
		VectorSet( dir, (rand() % 10000) - 5000, (rand() % 10000) - 5000, (rand() % 10000) - 5000 );
		VectorNormalizeNR( dir );
	}

	// Allow player to cap particles
	//if (count > cg_maxFragments.integer)
	//	count = cg_maxFragments.integer;

	VectorSubtract( from, origin, temp );
	VectorNormalizeNR ( temp );

	// move a little off impact point
	//VectorMA( origin, 2, normal, origin );

	RotatePointAroundVector(dir, normal, temp, 180);

	// s is one rotation 'direction
	PerpendicularVector( s, dir );

	// t is the other
	CrossProduct( dir, s, t );

	// Move it away from the wall a bit so it doesnt start in it
//	VectorMA ( origin, 5, dir, origin );

	// Launch them particles
	for (i = 0; i < count; i++)
	{
		vec3_t	up = { 0, 0, 28};

		if (part->up)
		{
			up[2] = part->up;
		}

		RotatePointAroundVector( temp, s, dir, (random() * radius) - (radius * 0.5f));
		RotatePointAroundVector( vel, t, temp, (random() * radius) - (radius * 0.5f));
		VectorScale (vel, part->minVelocity + random() * (part->maxVelocity - part->minVelocity), vel);
		VectorAdd ( vel, up, vel );
		//VectorMA ( vel, jump, up, vel );

		// launch duplicates of base particle until last one
		if (i != (count - 1))
		{
			clone = CG_ParticleClone ( le );	// always succeeds
			CG_ParticleLaunch ( origin, vel, clone );
		}
		else
		{
			CG_ParticleLaunch ( origin, vel, le );
		}
	}
}

/*
	CG_ParticleSmoke
	Spawns an AddScaleFade smokepuff along the
	path of a flying particle
*/

void CG_ParticleSmoke( localEntity_t *le, qhandle_t shader )
{
	int 	time;
	float		radius;
	vec3_t		origin, vel;

	particle_t	*part = &p_attr[le->partType];

	if (le->smoke == -1)
	{
		return;
	}

	// don't always render the effect
	if ((random() > part->smoke.chance) && (le->smoke == 0))
	{
		le->smoke = -1; 	// make sure it won't ever generate for this particle
		return;
	}

	// calculate time into particle's flight
	time = cg.time - le->pos.trTime;	// milliseconds into flight

	if (time <= 0)
	{
		return; 	// particle hasn't started yet
	}

	// has particle finished smoking?
	if (time > (le->pos.trDuration * part->smoke.percent))
	{
		le->smoke = -1;
		return;
	}

	// should we release a new smokepuff? return if no
	if (time < ((part->smoke.interval * part->smoke.percent) * le->smoke))
	{
		return;
	}

	le->smoke++;	// count number of smoke particles we've released

	radius = part->smoke.thickness; //le->radius / 4.0f;

	if (radius <= 0.0f)
	{
		radius = 0.1f;
	}
	VectorSet( vel, 0.0f, 0.0f, part->smoke.floatrate );	// rise/fall rate

	// Hack for custom gravity
	if((le->pos.trType == TR_GRAVITY2) || (le->pos.trType == TR_FEATHER))
	{
		trGravity = le->gravity;
	}

	BG_EvaluateTrajectory( &le->pos, cg.time, origin ); // where is the parent particle now?
	trGravity = 1.0f;

	CG_SmokePuff( origin, vel, radius, part->smoke.color[0], part->smoke.color[1], part->smoke.color[2], part->smoke.color[3], 900, cg.time, 0, LEF_PARTICLE, shader);
}
