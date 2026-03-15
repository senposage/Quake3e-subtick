// Copyright (C) 1999-2000 Id Software, Inc.
//
// cg_effects.c -- these functions generate localentities, usually as a result
// of event processing

#include "cg_local.h"

/**
 * $(function)
 */
void BuildLightFloat(vec3_t WorldOrigin, vec3_t StartColor, vec3_t Result)
{
	vec3_t	ambientLight, dirLight, Lightdir;

	trap_R_LightForPoint( WorldOrigin, ambientLight, dirLight, Lightdir );

	if ((dirLight[0] < 64) && (dirLight[1] < 64) && (dirLight[2] < 64))
	{
		//minlight it
		dirLight[0] = dirLight[1] = dirLight[2] = 64;
	}

	Result[0] = ((float)(dirLight[0] / 255) * (float)StartColor[0] / 255) * 255;
	Result[1] = ((float)(dirLight[1] / 255) * (float)StartColor[1] / 255) * 255;
	Result[2] = ((float)(dirLight[2] / 255) * (float)StartColor[2] / 255) * 255;
}

/**
 * $(function)
 */
void BuildLightByte(vec3_t WorldOrigin, vec3_t StartColor, char *Result)
{
	vec3_t	ambientLight, dirLight, Lightdir;

	trap_R_LightForPoint( WorldOrigin, ambientLight, dirLight, Lightdir );

	Result[0] = ((float)(dirLight[0] / 255) * (float)StartColor[0] / 255) * 255;
	Result[1] = ((float)(dirLight[1] / 255) * (float)StartColor[1] / 255) * 255;
	Result[2] = ((float)(dirLight[2] / 255) * (float)StartColor[2] / 255) * 255;
}
/*
==================
CG_BubbleTrail

Bullets shot underwater
==================
*/
void CG_BubbleTrail( vec3_t start, vec3_t end, float spacing )
{
	vec3_t	move;
	vec3_t	vec;
	float	len;
	int i;

	VectorCopy (start, move);
	VectorSubtract (end, start, vec);
	VectorNormalize (vec,len);

	// advance a random amount first
	i = rand() % (int)spacing;
	VectorMA( move, i, vec, move );

	VectorScale (vec, spacing, vec);

	for ( ; i < len; i += spacing)
	{
		localEntity_t  *le;
		refEntity_t    *re;

		le		   = CG_AllocLocalEntity();
		le->leFlags    = LEF_PUFF_DONT_SCALE;
		le->leType	   = LE_MOVE_SCALE_FADE;
		le->startTime	   = cg.time;
		le->endTime    = cg.time + 1000 + random() * 250;
		le->lifeRate	   = 1.0 / (le->endTime - le->startTime);

		re		   = &le->refEntity;
		re->shaderTime	   = cg.time / 1000.0f;

		re->reType	   = RT_SPRITE;
		re->rotation	   = 0;
		re->radius	   = 3;
		re->customShader   = cgs.media.waterBubbleShader;
		re->shaderRGBA[0]  = 0xff;
		re->shaderRGBA[1]  = 0xff;
		re->shaderRGBA[2]  = 0xff;
		re->shaderRGBA[3]  = 0xff;

		le->color[3]	   = 1.0;

		le->pos.trType	   = TR_LINEAR;
		le->pos.trTime	   = cg.time;
		VectorCopy( move, le->pos.trBase );
		le->pos.trDelta[0] = crandom() * 5;
		le->pos.trDelta[1] = crandom() * 5;
		le->pos.trDelta[2] = crandom() * 5 + 6;

		VectorAdd (move, vec, move);
	}
}

/*
=====================
CG_SmokePuff

Adds a smoke puff or blood trail localEntity.
=====================
*/
localEntity_t *CG_SmokePuff( const vec3_t p, const vec3_t vel,
				 float radius,
				 float r, float g, float b, float a,
				 float duration,
				 int startTime,
				 int fadeInTime,
				 int leFlags,
				 qhandle_t hShader )
{
	static int	   seed = 0x92;
	trace_t 	   trace;
	localEntity_t  *le;
	refEntity_t    *re;
	vec3_t		   dir, start;

	//	int fadeInTime = startTime + duration / 2;

	le		   = CG_AllocLocalEntity();
	le->leFlags    = leFlags;
	le->radius	   = radius;

	re		   = &le->refEntity;
	re->rotation   = Q_random( &seed ) * 360;
	re->radius	   = radius;
	re->shaderTime = startTime / 1000.0f;

	le->leType	   = LE_MOVE_SCALE_FADE;
	le->startTime  = startTime;
	le->fadeInTime = fadeInTime;
	le->endTime    = startTime + duration;

	if (fadeInTime > startTime)
	{
		le->lifeRate = 1.0 / (le->endTime - le->fadeInTime);
	}
	else
	{
		le->lifeRate = 1.0 / (le->endTime - le->startTime);
	}
	le->color[0]   = r;
	le->color[1]   = g;
	le->color[2]   = b;
	le->color[3]   = a;

	le->pos.trType = TR_LINEAR;
	le->pos.trTime = startTime;
	VectorCopy( vel, le->pos.trDelta );
	VectorCopy( p, le->pos.trBase );

	VectorCopy( p, re->origin );
	re->customShader = hShader;

	// rage pro can't alpha fade, so use a different shader
	if (cgs.glconfig.hardwareType == GLHW_RAGEPRO)
	{
		re->customShader  = cgs.media.smokePuffRageProShader;
		re->shaderRGBA[0] = 0xff;
		re->shaderRGBA[1] = 0xff;
		re->shaderRGBA[2] = 0xff;
		re->shaderRGBA[3] = 0xff;
	}
	else
	{
		re->shaderRGBA[0] = le->color[0] * 0xff;
		re->shaderRGBA[1] = le->color[1] * 0xff;
		re->shaderRGBA[2] = le->color[2] * 0xff;
		re->shaderRGBA[3] = 0xff;
	}

	re->reType = RT_SPRITE;
	re->radius = le->radius;

	//clippable particles, find out their maximum travel distance
	if (le->leFlags & LEF_CLIP)
	{
		VectorCopy(le->pos.trBase, start);

		le->angles.trBase[0] = start[0];
		le->angles.trBase[1] = start[1];
		le->angles.trBase[2] = start[2];

		dir[0]			 = start[0] + (vel[0] * 10000);
		dir[1]			 = start[1] + (vel[1] * 10000);
		dir[2]			 = start[2] + (vel[2] * 10000);

		le->angles.trTime	 = 10;

		trap_CM_BoxTrace(&trace, start, dir, NULL, NULL, 0, CONTENTS_SOLID | CONTENTS_TRANSLUCENT);

		if (trace.allsolid == qfalse) //&&(trace.startsolid==qfalse)
		{
			VectorSubtract(le->pos.trBase, trace.endpos, dir);
			le->angles.trTime = VectorLength(dir);

			//=trace.fraction*10000;
		}

		if (le->angles.trTime < 100)
		{
			//If Under 2 seconds
			le->angles.trTime  = 10000;
			le->pos.trDelta[0] = 0;
			le->pos.trDelta[1] = 0;
			le->pos.trDelta[2] = 10 + rand() % 10;
		}
//		CG_Printf("2) Gren Puff Time Eval: %d",le->angles.trTime);
	}

	return le;
}

/*
==================
CG_ScorePlum
==================
*/
void CG_ScorePlum( int client, vec3_t org, int score )
{
	localEntity_t  *le;
	refEntity_t    *re;
	vec3_t		   angles;
	static vec3_t  lastPos;

	// only visualize for the client that scored
	if (client != cg.predictedPlayerState.clientNum)
	{
		return;
	}

	le		  = CG_AllocLocalEntity();
	le->leFlags   = 0;
	le->leType	  = LE_SCOREPLUM;
	le->startTime = cg.time;
	le->endTime   = cg.time + 4000;
	le->lifeRate  = 1.0 / (le->endTime - le->startTime);

	le->color[0]  = le->color[1] = le->color[2] = le->color[3] = 1.0;
	le->radius	  = score;

	VectorCopy( org, le->pos.trBase );

	if ((org[2] >= lastPos[2] - 20) && (org[2] <= lastPos[2] + 20))
	{
		le->pos.trBase[2] -= 20;
	}

	//CG_Printf( "Plum origin %i %i %i -- %i\n", (int)org[0], (int)org[1], (int)org[2], (int)Distance(org, lastPos));
	VectorCopy(org, lastPos);

	re	   = &le->refEntity;

	re->reType = RT_SPRITE;
	re->radius = 16;

	VectorClear(angles);
	AnglesToAxis( angles, re->axis );
}

//////////////////////////////////////////////////////////
void CG_BombExplosionThink(void)
{
	int j;
	vec3_t	pos;
	vec3_t	dir;
	float	an;
	float	dis;

	if ((cg.time > cg.nextbombShockwave) && (cg.nextbombShockwave != 0))
	{
		cg.shockwaveCount++;
		cg.nextbombShockwave = cg.time + 50;

		if (cg.shockwaveCount > 60)
		{
			cg.nextbombShockwave = 0;
			cg.shockwaveCount	 = 0;
			return;
		}

		//make a shockwave
		for (j = 0; j < 8; j++)
		{
			an	   = rand() % 2000;
			dis    = 100 + ((cg.shockwaveCount % 45) * 16);
			pos[0] = cg.bombExplosionOrigin[0] + (cos(an) * (dis));
			pos[1] = cg.bombExplosionOrigin[1] + (sin(an) * (dis));
			pos[2] = cg.bombExplosionOrigin[2];

			dir[0] = 0;
			dir[1] = 0;
			dir[2] = 128;
			CG_SmokePuff(pos, dir, 512, 1, 1, 1, 0.5, 6000, cg.time, 0, LEF_CLIP | LEF_ALLOWOVERDRAW | LEF_SHOWLASER, cgs.media.grenadeSmokePuffShader);
		}
	}
}

/**
 * $(function)
 */
void CG_BombExplosion(vec3_t origin, float radius)
{
	bombParticle_t	*particle;
	vec3_t		cps[3];
	vec3_t		direction, velocity;
	float		length;
	float		coefA, coefB;
	int 	numParticles;
	int 	seed;
	int 	i, j;

	// Clear stuff.
	numParticles = 0;

	// Set seed number.
	seed = cg.time;

	// Clear particles.
	memset(&cg.bombParticles, 0, sizeof(cg.bombParticles));

	// Calculate control points.
	cps[0][0] = origin[0];
	cps[0][1] = origin[1];
	cps[0][2] = origin[2];

	cps[2][0] = origin[0];
	cps[2][1] = origin[1];
	cps[2][2] = origin[2] + radius;

	cps[1][0] = origin[0] + ((radius * ((Q_random(&seed) * 0.125f) + 0.25f)) * (Q_rand(&seed) & 1 ? 1.0f : -1.0f));
	cps[1][1] = origin[1] + ((radius * ((Q_random(&seed) * 0.125f) + 0.25f)) * (Q_rand(&seed) & 1 ? 1.0f : -1.0f));
	cps[1][2] = origin[2] + ((cps[2][2] - cps[0][2]) * (Q_random(&seed) * 0.375f) + 0.25f);

	// Calculate direction.
	VectorSet(direction, 0.0f, 0.0f, 1.0f);

	// Calculate velocity.
	VectorScale(direction, 5.0f, velocity);

	// Center column.
	for (i = 0; i < 10; i++)
	{
		// Set particle pointer.
		particle = &cg.bombParticles[numParticles];

		// Active particle.
		particle->active = qtrue;

		// Calculate curve coefficients.
		coefA = 1.0f - (coefB = (float)i / 10.0f);

		// Calculate position.
		particle->position[0] = (cps[0][0] * coefA * coefA) + (cps[1][0] * 2 * coefA * coefB) + (cps[2][0] * coefB * coefB);
		particle->position[1] = (cps[0][1] * coefA * coefA) + (cps[1][1] * 2 * coefA * coefB) + (cps[2][1] * coefB * coefB);
		particle->position[2] = (cps[0][2] * coefA * coefA) + (cps[1][2] * 2 * coefA * coefB) + (cps[2][2] * coefB * coefB);

		// Calculate velocity.
		VectorScale(velocity, 1.0f - ((float)i / 10.0f), particle->velocity);

		// Set shader.
		particle->shader = cgs.media.bombExplosionShaders[(Q_rand(&seed) & 2) + 5];

		// Set colour.
		particle->colour[0] = 1.0f;
		particle->colour[1] = 1.0f;
		particle->colour[2] = 1.0f;
		particle->colour[3] = 1.0f;

		// Set fade time.
		particle->fadeTime = (int)(7000.0f + (7000.0f * ((float)i / 10.0f)));

		// Set random angles.
		particle->angles[0] = Q_random(&seed) * 360.0f;
		particle->angles[1] = Q_random(&seed) * 360.0f;
		particle->angles[2] = Q_random(&seed) * 360.0f;

		// Calculate radius.
		particle->radius[2] = particle->radius[1] = particle->radius[0] = radius * (0.125f * (1.0f - ((float)i / 10.0f)));

		// Increase particle count.
		if ((numParticles++) == MAX_BOMBPARTICLES)
		{
			return;
		}
	}

	// Upper sphere.
	for (i = 0; i < 10; i++)
	{
		// Set particle pointer.
		particle = &cg.bombParticles[numParticles];

		// Active particle.
		particle->active = qtrue;

		// Calculate direction.
		direction[0] = (float)sin(DEG2RAD(Q_rand(&seed) & 360));
		direction[1] = (float)cos(DEG2RAD(Q_rand(&seed) & 360));
		direction[2] = (float)cos(DEG2RAD(Q_rand(&seed) & 360));

		VectorNormalizeNR(direction);

		// Calculate velocity.
		VectorScale(direction, 15.0f, particle->velocity);

		// Calculate position.
		for (j = 0; j < 3; j++)
		{
			particle->position[j] = origin[j] + (direction[j] * (radius * 0.1875f));
		}

		particle->position[2] += radius;

		// Set shader.
		particle->shader = cgs.media.bombExplosionShaders[Q_rand(&seed) & 4];

		// Set colour.
		particle->colour[0] = 0.75f;
		particle->colour[1] = 0.75f;
		particle->colour[2] = 0.75f;
		particle->colour[3] = 1.0f;

		// Set fade time.
		particle->fadeTime = 14000;

		// Set angles.
		particle->angles[0] = Q_random(&seed) * 360.0f;
		particle->angles[1] = Q_random(&seed) * 360.0f;
		particle->angles[2] = Q_random(&seed) * 360.0f;

		// Calculate radius.
		particle->radius[0] = radius * (0.1875f + (Q_random(&seed) * 0.125f));
		particle->radius[2] = particle->radius[1] = particle->radius[0];

		// Increase particle count.
		if ((numParticles++) == MAX_BOMBPARTICLES)
		{
			return;
		}
	}

	// Bottom sphere.
	for (i = 0; i < 5; i++)
	{
		// Set particle pointer.
		particle = &cg.bombParticles[numParticles];

		// Active particle.
		particle->active = qtrue;

		// Calculate direction.
		direction[0] = (float)sin(DEG2RAD(Q_rand(&seed) & 360));
		direction[1] = (float)cos(DEG2RAD(Q_rand(&seed) & 360));
		direction[2] = (float)cos(DEG2RAD(Q_rand(&seed) & 360));

		VectorNormalizeNR(direction);

		// Calculate velocity.
		VectorScale(direction, 3.0f, particle->velocity);

		// Calculate position.
		particle->position[0] = (cps[0][0] * 0.04f) + (cps[1][0] * 0.32f) + (cps[2][0] * 0.64f);
		particle->position[1] = (cps[0][1] * 0.04f) + (cps[1][1] * 0.32f) + (cps[2][1] * 0.64f);
		particle->position[2] = (cps[0][2] * 0.04f) + (cps[1][2] * 0.32f) + (cps[2][2] * 0.64f);

		for (j = 0; j < 3; j++)
		{
			particle->position[j] += direction[j] * (radius * (0.09375f + (Q_random(&seed) * 0.03125f)));
		}

		// Set shader.
		particle->shader = cgs.media.bombExplosionShaders[(Q_rand(&seed) & 2) + 5];

		// Set colour.
		particle->colour[0] = 1.0f;
		particle->colour[1] = 1.0f;
		particle->colour[2] = 1.0f;
		particle->colour[3] = 1.0f;

		// Set fade time.
		particle->fadeTime = 13000;

		// Set angles.
		particle->angles[0] = Q_random(&seed) * 360.0f;
		particle->angles[1] = Q_random(&seed) * 360.0f;
		particle->angles[2] = Q_random(&seed) * 360.0f;

		// Calculate radius.
		particle->radius[0] = radius * (0.1875f + (Q_random(&seed) * 0.125f));
		particle->radius[2] = particle->radius[1] = particle->radius[0];

		// Increase particle count.
		if ((numParticles++) == MAX_BOMBPARTICLES)
		{
			return;
		}
	}

	cg.nextbombShockwave = cg.time + 100;
	// Calculate distance.
	length			 = Distance(origin, cg.refdef.vieworg);

	// Shake screen.
	if (length < 2000.0f)
	{
		cg.screenshaketime = cg.time + (4500 * (1.0f - (length / 2000.0f)));
	}
}

//////////////////////////////////////////////////////////

localEntity_t *CG_FragExplosion(vec3_t origin, vec3_t dir, int radius, qhandle_t model, qhandle_t shader, int msec)
{
	localEntity_t  *ex;
	localEntity_t  *le;
	localEntity_t  *clone;
	trace_t 	   tr;
	vec3_t		   temp, newOrigin;
	vec3_t		   down = { 0.0f, 0.0f, -300.0f};
	float		   xs, ys, zs;

	float		   dist;
	int 		   seed;
	int 		   i;

	if (msec <= 0)
	{
		CG_Error("CG_FragExplosion: msec = %i", msec);
	}

	seed = cg.time;

	ex = CG_AllocLocalEntity();

	ex->leType				   = LE_SPRITE_EXPLOSION;
	ex->refEntity.radius	   = radius;
	ex->refEntity.rotation	   = Q_rand(&seed) % 360;
	ex->startTime			   = cg.time - (Q_rand(&seed) & 63);
	ex->endTime 			   = ex->startTime + msec;
	ex->refEntity.shaderTime   = ex->startTime / 1000.0f;
	ex->refEntity.hModel	   = model;
	ex->refEntity.customShader = shader;
	ex->color[0]			   = ex->color[1] = ex->color[2] = 1.0;

	VectorScale(dir, 16, temp);
	VectorAdd(temp, origin, newOrigin);
	VectorCopy(newOrigin, ex->refEntity.origin);
	VectorCopy(newOrigin, ex->refEntity.oldorigin);

	CG_Trace(&tr, ex->refEntity.origin, NULL, NULL, down, -1, MASK_SHOT);

	le = CG_ParticleCreate();
	CG_ParticleSetAttributes(le, PART_ATTR_EXPLOSION, cgs.media.tracerShader, 0, 0);

	for (i = 0; i < 48; i++)
	{
		xs = (Q_rand(&seed) % 1600) + 320;
		ys = (Q_rand(&seed) % 1600) + 320;
		zs = (Q_rand(&seed) % 1600) + 320;

		if (Q_rand(&seed) & 1)
		{
			xs *= -1;
		}

		if (Q_rand(&seed) & 1)
		{
			ys *= -1;
		}

		if ((Q_rand(&seed) & 1) && (tr.fraction != 1.0f))
		{
			zs *= -1;
		}

		VectorSet(temp, xs, ys, zs);

		if (i != 47)
		{
			clone = CG_ParticleClone(le);
			CG_ParticleLaunch(ex->refEntity.origin, temp, clone);
		}
		else
		{
			CG_ParticleLaunch(ex->refEntity.origin, temp, le);
		}
	}

	le = CG_ParticleCreate();
	CG_ParticleSetAttributes(le, PART_ATTR_STONE, cgs.media.tracerShader, 0, cgs.media.glassFragments[0]);

	for (i = 0; i < 32; i++)
	{
		xs = (Q_rand(&seed) % 500) + 100;
		ys = (Q_rand(&seed) % 500) + 100;
		zs = (Q_rand(&seed) % 500) + 100;

		if (Q_rand(&seed) & 1)
		{
			xs *= -1;
		}

		if (Q_rand(&seed) & 1)
		{
			ys *= -1;
		}

		if ((Q_rand(&seed) & 1) && (tr.fraction != 1.0f))
		{
			zs *= -1;
		}

		VectorSet(temp, xs, ys, zs);

		if (i != 31)
		{
			clone = CG_ParticleClone(le);
			CG_ParticleLaunch(ex->refEntity.origin, temp, clone);
		}
		else
		{
			CG_ParticleLaunch(ex->refEntity.origin, temp, le);
		}
	}

	le = CG_ParticleCreate();
	CG_ParticleSetAttributes(le, PART_ATTR_EXPLOSION_CHUNKS, cgs.media.tracerShader, 0, cgs.media.glassFragments[1]);

	for (i = 0; i < 8; i++)
	{
		xs = (Q_rand(&seed) % 500) + 100;
		ys = (Q_rand(&seed) % 500) + 100;
		zs = (Q_rand(&seed) % 500) + 100;

		if (Q_rand(&seed) & 1)
		{
			xs *= -1;
		}

		if (Q_rand(&seed) & 1)
		{
			ys *= -1;
		}

		if ((Q_rand(&seed) & 1) && (tr.fraction != 1.0f))
		{
			zs *= -1;
		}

		VectorSet(temp, xs, ys, zs);

		if (i != 7)
		{
			clone = CG_ParticleClone(le);
			CG_ParticleLaunch(ex->refEntity.origin, temp, clone);
		}
		else
		{
			CG_ParticleLaunch(ex->refEntity.origin, temp, le);
		}
	}

	/*
	for (i=0; i < 16; i ++)
	{
		xs = (Q_rand(&seed) % 10) + 2;
		ys = (Q_rand(&seed) % 10) + 2;
		zs = (Q_rand(&seed) % 10) + 2;

		if (Q_rand(&seed) & 1)
			xs *= -1;

		if (Q_rand(&seed) & 1)
			ys *= -1;

		if ((Q_rand(&seed) & 1) && (tr.fraction != 1.0f))
			zs *= -1;

		VectorSet(temp, xs, ys, zs);

		sx = 1 + (float)(Q_rand(&seed) & 4);
		sy = 1 + (float)(Q_rand(&seed) & 4);
		sz = 1 + (float)(Q_rand(&seed) & 8);

		origin[0] += temp[0] * sx;
		origin[1] += temp[1] * sy;
		origin[2] += temp[2] * sz;

		CG_SmokePuff(origin, temp, 100 + (Q_rand(&seed) & 50), 0.75f, 0.75f, 0.75f, 0.65f, 5000, cg.time, 0, 0, cgs.media.smokePuffShader);

		origin[0] -= temp[0] * sx;
		origin[1] -= temp[1] * sy;
		origin[2] -= temp[2] * sz;
	}
	*/

	VectorSubtract(cg.refdef.vieworg, origin, temp);
	dist = VectorLength(temp);

	if(dist < 700)
	{
		cg.screenshaketime = cg.time + (1500 - dist * 1.5);
	}

	return ex;
}

//////////////////////////////////////////////////////////

/*
====================
CG_MakeExplosion
====================
*/
localEntity_t *CG_MakeExplosion( vec3_t origin, vec3_t dir, int radius,
				 qhandle_t hModel, qhandle_t shader,
				 int msec, qboolean isSprite, qboolean particles )
{
	float		   xs, ys, zs;
	float		   dist;
	localEntity_t  *ex;
	vec3_t		   tmpVec, pos;
	int 		   j;

	vec3_t		   vel;
	int 		   seed, i;

	ex = 0;

	if (msec <= 0)
	{
		CG_Error( "CG_MakeExplosion: msec = %i", msec );
	}

	/*seed = cg.time;

	// skew the time a bit so they aren't all in sync
	offset = Q_rand(&seed) & 63;

	ex = CG_AllocLocalEntity();
	if ( isSprite )
	{
		ex->leType = LE_SPRITE_EXPLOSION;
		ex->refEntity.radius = radius;

		// randomly rotate sprite orientation
		ex->refEntity.rotation = Q_rand(&seed) % 360;
		VectorScale( dir, 16, tmpVec );
		VectorAdd( tmpVec, origin, newOrigin );

	}
	else
	{
		ex->leType = LE_EXPLOSION;
		VectorCopy( origin, newOrigin );

		// set axis with random rotate
		if ( !dir )
	{
			AxisClear( ex->refEntity.axis );
		}
	else
	{
			ang = Q_rand(&seed) % 360;
			VectorCopy( dir, ex->refEntity.axis[0] );
			RotateAroundDirection( ex->refEntity.axis, ang );
		}
	}

	ex->startTime = cg.time - offset;
	ex->endTime = ex->startTime + msec;

	// bias the time so all shader effects start correctly
	ex->refEntity.shaderTime = ex->startTime / 1000.0f;

	ex->refEntity.hModel = hModel;
	ex->refEntity.customShader = shader;
	ex->pos.trDelta[2]=100;

	// set origin
	VectorCopy( newOrigin, ex->refEntity.origin );
	VectorCopy( newOrigin, ex->refEntity.oldorigin );

	ex->color[0] = ex->color[1] = ex->color[2] = 1.0;*/

	//Firey core
	ex			 = CG_AllocLocalEntity();
	ex->leType		 = LE_SPRITE_EXPLOSION;
	ex->refEntity.radius = radius;

	// randomly rotate sprite orientation
	ex->refEntity.rotation = Q_rand(&seed) % 360;
	ex->startTime		   = cg.time;
	ex->endTime 	   = ex->startTime + msec;

	// bias the time so all shader effects start correctly
	ex->refEntity.shaderTime   = ex->startTime / 1000.0f;
	ex->refEntity.hModel	   = hModel;
	ex->refEntity.customShader = shader;

	// set origin
	VectorCopy( origin, ex->refEntity.origin );
	VectorCopy( origin, ex->refEntity.oldorigin );

	ex->color[0] = ex->color[1] = ex->color[2] = 1.0;

	// Add particles if told to do so
	if (particles)
	{
		localEntity_t  *le;
		localEntity_t  *clone;

		// check to see if the explosion went off in the air
		//CG_Trace( &tr, ex->refEntity.origin, NULL, NULL, down, -1, MASK_SHOT );

		// sparks
		le = CG_ParticleCreate( );
		CG_ParticleSetAttributes ( le, PART_ATTR_EXPLOSION, cgs.media.tracerShader, 0, 0 );

		for (i = 0; i < 32; i++)
		{
			xs = (Q_rand(&seed) % 1600) - 800;
			ys = (Q_rand(&seed) % 1600) - 800;
			zs = (Q_rand(&seed) % 1600) - 800;
			zs = fabs(zs);
			//	if ( (tr.fraction == 1.0f) && ( Q_rand(&seed)&1 ) ) zs = fabs(zs);

			VectorSet( vel, xs, ys, zs ) ;

			if(i != 31)
			{
				clone = CG_ParticleClone ( le );
				CG_ParticleLaunch ( origin, vel, clone );
			}
			else
			{
				CG_ParticleLaunch( origin, vel, le );
			}
		}

		// chunks of rubble
		le = CG_ParticleCreate( );
		CG_ParticleSetAttributes ( le, PART_ATTR_EXPLOSION_CHUNKS, cgs.media.tracerShader, 0, cgs.media.glassFragments[0] );

		for (i = 0; i < 32; i++)
		{
			xs = (Q_rand(&seed) % 500) - 250;
			ys = (Q_rand(&seed) % 500) - 250;
			zs = (Q_rand(&seed) % 500) ;

			VectorSet( vel, xs, ys, zs ) ;

			if(i != 31)
			{
				clone = CG_ParticleClone ( le );
				CG_ParticleLaunch ( origin, vel, clone );
			}
			else
			{
				CG_ParticleLaunch(origin, vel, le );
			}
		}
	}

	VectorSet ( tmpVec, 0, 0, 32 );

	for (j = 0; j < 7; j++)
	{
		tmpVec[0] = ((rand() & 16000) - 8000) * 0.05;
		tmpVec[1] = ((rand() & 16000) - 8000) * 0.05;
		tmpVec[2] = ((rand() & 16000)) * 0.005;

		CG_SmokePuff( origin, tmpVec, 100 + rand() % 100, 0.0f, 0.0f, 0.0f, 1.0f, 300 + rand() % 1000, cg.time, 0, LEF_TUMBLE, cgs.media.shotgunSmokePuffShader);
	}

	// Add a black smoke puff
	//VectorSet ( tmpVec, 0, 0, 8 );
	//CG_SmokePuff( origin, tmpVec, radius, 0.0f, 0.0f, 0.0f, 1.0f, 1800, cg.time, 0/*LEF_PUFF_DONT_SCALE*/, 0, cgs.media.smokePuffShader );

	/*
	for (i=0; i < 16; i ++)
	{
		xs = (Q_rand(&seed) % 10) + 2;
		ys = (Q_rand(&seed) % 10) + 2;
		zs = (Q_rand(&seed) % 10) + 2;

		if (Q_rand(&seed) & 1)
			xs *= -1;

		if (Q_rand(&seed) & 1)
			ys *= -1;

		if ((Q_rand(&seed) & 1) && (tr.fraction != 1.0f))
			zs *= -1;

		VectorSet(tmpVec, xs, ys, zs);

		sx = 1 + (float)(Q_rand(&seed) & 4);
		sy = 1 + (float)(Q_rand(&seed) & 4);
		sz = 1 + (float)(Q_rand(&seed) & 8);

		origin[0] += tmpVec[0] * sx;
		origin[1] += tmpVec[1] * sy;
		origin[2] += tmpVec[2] * sz;

		CG_SmokePuff(origin, tmpVec, (radius / 2) + (Q_rand(&seed) & (radius / 4)), 0.75f, 0.75f, 0.75f, 0.65f, 5000, cg.time, 0, 0, cgs.media.smokePuffShader);

		origin[0] -= tmpVec[0] * sx;
		origin[1] -= tmpVec[1] * sy;
		origin[2] -= tmpVec[2] * sz;
	}
	*/

	//27 calculate our distance from the player, and if we're within range, give us some screen shake
	VectorSubtract(cg.refdef.vieworg, origin, pos);
	dist = VectorLength(pos);

	// Changed DensitY: this was a little toooo strong
	if(dist < 700)
	{
		cg.screenshaketime = cg.time + (1500 - dist * 1.5);
	}

	trap_R_AddLightToScene(origin, 250, 1, 1, 1);

	return ex;
}

/*
====================
CG_MakeFlash
====================
*/
localEntity_t *CG_MakeFlash( vec3_t origin, int msec )
{
	localEntity_t	*ex;

	ex = CG_AllocLocalEntity( );

	ex->leType = LE_EXPLOSION;
	ex->startTime = cg.time;
	ex->endTime = ex->startTime + msec;

	// set origin
	VectorCopy( origin, ex->refEntity.origin );
	VectorCopy( origin, ex->refEntity.oldorigin );

	return ex;
}

/*
=================
CG_Bleed

This is the spurt of blood when a character gets hit
=================
*/

void CG_Bleed( vec3_t origin, int entityNum )
{
	localEntity_t  *ex;

	if (!cg_blood.integer)
	{
		return;
	}

	ex		  = CG_AllocLocalEntity();
	ex->leType	  = LE_EXPLOSION;

	ex->startTime = cg.time;
	ex->endTime   = ex->startTime + 500;

	VectorCopy ( origin, ex->refEntity.origin);
	ex->refEntity.reType	   = RT_SPRITE;
	ex->refEntity.rotation	   = rand() % 360;
	ex->refEntity.radius	   = 30;

	ex->refEntity.customShader = cgs.media.bloodExplosionShader;

	// don't show player's own blood in view
	if (entityNum == cg.snap->ps.clientNum)
	{
		ex->refEntity.renderfx |= RF_THIRD_PERSON;
	}
}

/*
==================
CG_LaunchGib
==================
*/
void CG_LaunchGib( vec3_t origin, vec3_t velocity, qhandle_t hModel )
{
	localEntity_t  *le;
	refEntity_t    *re;

	le		  = CG_AllocLocalEntity();
	re		  = &le->refEntity;

	le->leType	  = LE_FRAGMENT;
	le->startTime = cg.time;
	le->endTime   = le->startTime + 15000; //don't want the gibs to clear.

	VectorCopy( origin, re->origin );
	AxisCopy( axisDefault, re->axis );
	re->hModel	   = hModel;

	le->pos.trType = TR_GRAVITY;
	VectorCopy( origin, le->pos.trBase );
	VectorCopy( velocity, le->pos.trDelta );
	le->pos.trTime		  = cg.time;

	le->bounceFactor	  = 0.6f;

	le->leBounceSoundType = LEBS_BLOOD;
	le->leMarkType		  = LEMT_BLOOD;
}

/*
===================
CG_GibPlayer

Generated a bunch of gibs launching out from the bodies location
===================
*/
void CG_GibPlayer(vec3_t origin, vec3_t velocity)
{
	vec3_t	gibVel;
	int seed;

	// If blood is disabled.
	if (!cg_blood.integer)
	{
		return;
	}

	// Set seed number.
	seed	  = cg.time;

	gibVel[0] = velocity[0] * (2.0f * Q_random(&seed));
	gibVel[1] = velocity[1] * (2.0f * Q_random(&seed));
	gibVel[2] = velocity[2] * (2.0f * Q_random(&seed));

	if (Q_rand(&seed) & 1)
	{
		CG_LaunchGib(origin, gibVel, cgs.media.gibSkull);
	}

	gibVel[0] = velocity[0] * (2.0f * Q_random(&seed));
	gibVel[1] = velocity[1] * (2.0f * Q_random(&seed));
	gibVel[2] = velocity[2] * (2.0f * Q_random(&seed));
	CG_LaunchGib(origin, gibVel, cgs.media.gibBrain);

	gibVel[0] = velocity[0] * (2.0f * Q_random(&seed));
	gibVel[1] = velocity[1] * (2.0f * Q_random(&seed));
	gibVel[2] = velocity[2] * (2.0f * Q_random(&seed));

	CG_LaunchGib(origin, gibVel, cgs.media.gibAbdomen);

	gibVel[0] = velocity[0] * (2.0f * Q_random(&seed));
	gibVel[1] = velocity[1] * (2.0f * Q_random(&seed));
	gibVel[2] = velocity[2] * (2.0f * Q_random(&seed));

	CG_LaunchGib(origin, gibVel, cgs.media.gibArm);

	gibVel[0] = velocity[0] * (2.0f * Q_random(&seed));
	gibVel[1] = velocity[1] * (2.0f * Q_random(&seed));
	gibVel[2] = velocity[2] * (2.0f * Q_random(&seed));
	CG_LaunchGib( origin, gibVel, cgs.media.gibChest);

	gibVel[0] = velocity[0] * (2.0f * Q_random(&seed));
	gibVel[1] = velocity[1] * (2.0f * Q_random(&seed));
	gibVel[2] = velocity[2] * (2.0f * Q_random(&seed));

	CG_LaunchGib(origin, gibVel, cgs.media.gibFist);

	gibVel[0] = velocity[0] * (2.0f * Q_random(&seed));
	gibVel[1] = velocity[1] * (2.0f * Q_random(&seed));
	gibVel[2] = velocity[2] * (2.0f * Q_random(&seed));

	CG_LaunchGib(origin, gibVel, cgs.media.gibFoot);

	gibVel[0] = velocity[0] * (2.0f * Q_random(&seed));
	gibVel[1] = velocity[1] * (2.0f * Q_random(&seed));
	gibVel[2] = velocity[2] * (2.0f * Q_random(&seed));

	CG_LaunchGib(origin, gibVel, cgs.media.gibForearm);

	gibVel[0] = velocity[0] * (2.0f * Q_random(&seed));
	gibVel[1] = velocity[1] * (2.0f * Q_random(&seed));
	gibVel[2] = velocity[2] * (2.0f * Q_random(&seed));

	CG_LaunchGib(origin, gibVel, cgs.media.gibIntestine);

	gibVel[0] = velocity[0] * (2.0f * Q_random(&seed));
	gibVel[1] = velocity[1] * (2.0f * Q_random(&seed));
	gibVel[2] = velocity[2] * (2.0f * Q_random(&seed));

	CG_LaunchGib(origin, gibVel, cgs.media.gibLeg);

	gibVel[0] = velocity[0] * (2.0f * Q_random(&seed));
	gibVel[1] = velocity[1] * (2.0f * Q_random(&seed));
	gibVel[2] = velocity[2] * (2.0f * Q_random(&seed));

	CG_LaunchGib(origin, gibVel, cgs.media.gibLeg);
}

/*
==================
CG_LaunchGib
==================
*/
void CG_LaunchExplode( vec3_t origin, vec3_t velocity, qhandle_t hModel )
{
	localEntity_t  *le;
	refEntity_t    *re;

	le		  = CG_AllocLocalEntity();
	re		  = &le->refEntity;

	le->leType	  = LE_FRAGMENT;
	le->startTime = cg.time;
	le->endTime   = le->startTime + 10000 + random() * 6000;

	VectorCopy( origin, re->origin );
	AxisCopy( axisDefault, re->axis );
	re->hModel	   = hModel;

	le->pos.trType = TR_GRAVITY;
	VectorCopy( origin, le->pos.trBase );
	VectorCopy( velocity, le->pos.trDelta );
	le->pos.trTime		  = cg.time;

	le->bounceFactor	  = 0.1f;

	le->leBounceSoundType = LEBS_BRASS;
	le->leMarkType		  = LEMT_NONE;
}

#define EXP_VELOCITY 100
#define EXP_JUMP	 150
/*
===================
CG_GibPlayer

Generated a bunch of gibs launching out from the bodies location
===================
*/
void CG_BigExplode( vec3_t playerOrigin )
{
	vec3_t	origin, velocity;

	if (!cg_blood.integer)
	{
		return;
	}

	VectorCopy( playerOrigin, origin );
	velocity[0] = crandom() * EXP_VELOCITY;
	velocity[1] = crandom() * EXP_VELOCITY;
	velocity[2] = EXP_JUMP + crandom() * EXP_VELOCITY;
	CG_LaunchExplode( origin, velocity, cgs.media.smoke2 );

	VectorCopy( playerOrigin, origin );
	velocity[0] = crandom() * EXP_VELOCITY;
	velocity[1] = crandom() * EXP_VELOCITY;
	velocity[2] = EXP_JUMP + crandom() * EXP_VELOCITY;
	CG_LaunchExplode( origin, velocity, cgs.media.smoke2 );

	VectorCopy( playerOrigin, origin );
	velocity[0] = crandom() * EXP_VELOCITY * 1.5;
	velocity[1] = crandom() * EXP_VELOCITY * 1.5;
	velocity[2] = EXP_JUMP + crandom() * EXP_VELOCITY;
	CG_LaunchExplode( origin, velocity, cgs.media.smoke2 );

	VectorCopy( playerOrigin, origin );
	velocity[0] = crandom() * EXP_VELOCITY * 2.0;
	velocity[1] = crandom() * EXP_VELOCITY * 2.0;
	velocity[2] = EXP_JUMP + crandom() * EXP_VELOCITY;
	CG_LaunchExplode( origin, velocity, cgs.media.smoke2 );

	VectorCopy( playerOrigin, origin );
	velocity[0] = crandom() * EXP_VELOCITY * 2.5;
	velocity[1] = crandom() * EXP_VELOCITY * 2.5;
	velocity[2] = EXP_JUMP + crandom() * EXP_VELOCITY;
	CG_LaunchExplode( origin, velocity, cgs.media.smoke2 );
}

///////////////////////////////////////////////////////////////////////////

// #27 - launches a jet of blood out the back of a wound, and a mist at the entrance.
void CG_BleedPlayer( vec3_t woundOrigin, vec3_t woundDirection, int speed )
{
	localEntity_t  *le;
	refEntity_t    *re;

	if (!cg_blood.integer)
	{
		return;
	}

	woundDirection[0] *= speed;
	woundDirection[1] *= speed;
	woundDirection[2] *= speed;

	le		   = CG_AllocLocalEntity();
	re		   = &le->refEntity;

	le->leType	   = LE_FRAGMENT;
	le->startTime  = cg.time;
	le->endTime    = le->startTime + 5000 + random() * 3000;

	VectorCopy( woundOrigin, re->origin );
	AxisCopy( axisDefault, re->axis );
	re->hModel	   = 0;

	le->pos.trType = TR_GRAVITY;
	VectorCopy( woundOrigin, le->pos.trBase );
	VectorCopy( woundDirection, le->pos.trDelta );
	le->pos.trTime		  = cg.time;

	le->bounceFactor	  = 0.6f;
	le->leFlags 	  = LEF_DIEONIMPACT | LEF_DONTRENDER;
	le->leBounceSoundType = LEBS_BLOOD;
	le->leMarkType		  = LEMT_SMALLBLOOD;
}

/*
=========================
  UT Breakable Objects
  Fragment Effects Code
  Dokta8
=========================
*/

/*
==================
CG_LaunchFragment
by Dokta8
==================
*/
localEntity_t *CG_LaunchFragment(vec3_t origin,
				 vec3_t velocity,
				 qhandle_t hModel,
				 qhandle_t shader,
				 int breaktype,
				 float xs,
				 float ys,
				 float zs
				 )
{
	localEntity_t  *le;
	refEntity_t    *re;

	//particle_t *part = &p_attr[ le->partType ];

	le		  = CG_AllocLocalEntity();
	re		  = &le->refEntity;

	le->leType	  = LE_FRAGMENT;
	le->startTime = cg.time;

	if (cg.bombExploded)
	{
		if (cg.time - cg.bombExplodeTime >= 12000)
		{
			return NULL;
		}
		else
		{
			le->endTime = cg.bombExplodeTime + 12000;
		}
	}
	else
	{
		le->endTime = le->startTime + 5000 + (rand() % 3000);
	}

	VectorCopy( origin, re->origin );
//	AxisCopy( axisDefault, re->axis );

	if(hModel)
	{
		re->reType = RT_MODEL;
	}
	else
	{
		re->reType	  = RT_SPRITE;
		re->shaderRGBA[0] = 0xff;
		re->shaderRGBA[1] = 0xff;
		re->shaderRGBA[2] = 0xff;
		re->shaderRGBA[3] = 0xff;
		re->radius	  = 3;
	}

	re->hModel	   = hModel;
	le->gravity    = 1.0f;
	le->pos.trType = TR_GRAVITY2;
	VectorCopy( origin, le->pos.trBase );
	VectorCopy( velocity, le->pos.trDelta );
	le->pos.trTime		  = cg.time;

	le->angles.trType	  = TR_LINEAR;	// set up tumble parms
	VectorClear( le->angles.trBase );
	le->angles.trDelta[0] = crandom() * 720;	// tumble in all dimensions randomly
	le->angles.trDelta[1] = crandom() * 720;
	le->angles.trDelta[2] = crandom() * 720;
	le->angles.trTime	  = cg.time;

	le->bounceFactor	  = 0.3f;

	le->leFlags 		 |= LEF_TUMBLE; // make em tumble

	le->leBounceSoundType = LEBS_GLASS + breaktype;

	le->scale[0]		  = xs;
	le->scale[1]		  = ys;
	le->scale[2]		  = zs;

	if (xs || ys || zs)
	{
		VectorScale( le->refEntity.axis[0], le->scale[0], le->refEntity.axis[0] );
		VectorScale( le->refEntity.axis[1], le->scale[1], le->refEntity.axis[1] );
		VectorScale( le->refEntity.axis[2], le->scale[2], le->refEntity.axis[2] );
		le->refEntity.nonNormalizedAxes = qtrue;
	}

	re->customShader = shader;

	return le;
}

/*
===================
CG_BreakBreakable
by Dokta8
Shatters a func_breakable into pieces
===================
*/
#define FRAG_VELOCITY 100
#define FRAG_JUMP	  0
#if 1
/**
 * $(function)
 */
void CG_BreakBreakable(entityState_t *es, vec3_t dir, qhandle_t shader)
{
	qhandle_t  model;
	vec3_t	   origin, range, velocity;
	float	   xs, zs, ys;
	float	   maxLength;
	float	   radius, dist;
	int    breakType;
	int    numShards;
	int    seed;
	int    i;

	// Get breakable type.
	breakType = es->generic1;

	// Get shards number.
	numShards = es->eventParm;

	// Clamp.
	if (numShards > cg_maxFragments.integer)
	{
		numShards = cg_maxFragments.integer;
	}

	if (numShards < 32)
	{
		numShards = 32;
	}

	// If not a valid number.
	if (numShards < 1)
	{
		return;
	}

	// Calculate range.
	for (i = 0, maxLength = 0.0f; i < 3; i++)
	{
		range[i] = (float)fabs(es->origin2[i] - es->origin[i]);

		if (maxLength < range[i])
		{
			maxLength = range[i];
		}
	}

	// Set seed number.
	seed = cg.time;

	// Generate fragments.
	for (i = 0; i < numShards; i++)
	{
		// Calculate scale.
		xs = (Q_random(&seed) * 1.5f) * (maxLength / (float)numShards);
		ys = (Q_random(&seed) * 1.5f) * (maxLength / (float)numShards);
		zs = (Q_random(&seed) * 1.5f) * (maxLength / (float)numShards);

		// Calculate fragment origin.
		origin[0] = UT_StdRand(es->origin[0], range[0]);
		origin[1] = UT_StdRand(es->origin[1], range[1]);
		origin[2] = UT_StdRand(es->origin[2], range[2]);

		// If bomb exploded.
		if (cg.bombExploded && ((radius = ((cg.time - cg.bombExplodeTime) * 1.0f)) < 5000.0f))
		{
			// Calculate direction.
			VectorSubtract(origin, cg.bombExplosionOrigin, velocity);
			VectorNormalize(velocity,dist);

			// If shockwave within range of shard.
			if ((float)fabs(radius - dist) <= 500.0f)
			{
				// Calculate fragment velocity.
				VectorScale(velocity, 2500.0f * (1.0f - (radius / 5000.0f)), velocity);
			}
			else
			{
				// Calculate fragment velocity.
				velocity[0] = es->angles2[0] + (FRAG_VELOCITY * (Q_crandom(&seed) * 2.0f));
				velocity[1] = es->angles2[1] + (FRAG_VELOCITY * (Q_crandom(&seed) * 2.0f));
				velocity[2] = es->angles2[2] + (FRAG_VELOCITY *(Q_crandom(&seed) * 2.0f));
			}
		}
		else
		{
			// Calculate fragment velocity.
			velocity[0] = es->angles2[0] + (FRAG_VELOCITY * (Q_crandom(&seed) * 2.0f));
			velocity[1] = es->angles2[1] + (FRAG_VELOCITY * (Q_crandom(&seed) * 2.0f));
			velocity[2] = es->angles2[2] + (FRAG_VELOCITY * (Q_crandom(&seed) * 2.0f));
		}

		// Select model.
		switch (breakType)
		{
			case 0: // Glass.
			case 1: // Wood - simple.
			case 2: // Ceramic.
			case 3: // Plastic.
			case 4: // Metal simple.
			case 9: // Wood - explosives.
			case 10: // Wood - hk69.
				model = cgs.media.glassFragments[Q_rand(&seed) & 2];
				xs	 *= 0.15f;
				ys	 *= 0.15f;
				zs	 *= 0.15f;
				break;

			case 5: // Metal - explosives.
			case 6: // Metal - hk69.
				model = cgs.media.metalFragments[Q_rand(&seed) & 2];
				break;

			case 7: // Stone - explosives.
			case 8: // Stone - hk69.
				model = cgs.media.stoneFragments[Q_rand(&seed) & 2];
				break;
		}

		// Create fragment.
		CG_LaunchFragment(origin, velocity, model, shader, breakType, xs, ys, zs);
	}
}
#else
/**
 * $(function)
 */
void CG_BreakBreakable( entityState_t *es, vec3_t dir, qhandle_t shader )
{
	int    i;
	vec3_t	   origin, velocity, range;
	qhandle_t  hModel;
	float	   modelcheck;
	int    breaktype;
	int    shards;
	int    axisminsize, thinaxis;
	float	   xs = 0.1f;
	float	   ys = 0.1f;
	float	   zs = 0.1f;

	// es->origin will contain the min bounds for the breakable object
	// es->origin2 contains the max bounds for the object
	// es->angles2 contains the direction from which the break comes
	// es->eventParm contains the number of fragments in bits 0 to 5
	// and the breaktype in 6, 7 and 8

	//shards = es->eventParm & 0x3F;	// mask bottom 6 bits for shards
	//shards*=6;
	//breaktype = es->eventParm >> 6;		// rotate left to get breaktype

	shards	  = es->eventParm * 6;
	breaktype = es->generic1;

	// Dokta8: allow player to cap glass fragments
	if (shards > cg_maxFragments.integer)
	{
		shards = cg_maxFragments.integer;
	}

	if (!shards)
	{
		return;
	}

	// we want to spawn fragments in random locations within the bounds
	// of the object
	// so it's mins + crandom(abs(max-mins))
	// subtract max from min and store in range
	axisminsize = 10000;
	thinaxis	= 0;

	for (i = 0; i < 3; i++)
	{
		range[i] = fabs(es->origin2[i] - es->origin[i]);

		// find the thinnest axis for shard size calc
		if (range[i] < axisminsize)
		{
			axisminsize = range[i];
			thinaxis	= i;
		}
	}

	//shader = 0;

/*
	switch ( breaktype ) {
	default:
	case 0: // glass
		shader = cgs.media.UT_glassFragment;
		break;
	case 1: // wood
		shader = cgs.media.UT_woodFragment;
		break;
	case 2: // ceramic
		shader = cgs.media.UT_stoneFragment;
		break;
	case 3: // plastic
		shader = cgs.media.UT_stoneFragment;
		break;
	}
*/

	for (i = 0; i < shards; i++) // do it for each fragment/shard

	{	// set scaling factors for shards
		if ((breaktype >= 4) && (breaktype <= 8))
		{
			xs = (float)(rand() % 100) / (1.0f * shards); //range[1] / (20 + (rand() % 40));
			ys = (float)(rand() % 100) / (1.0f * shards); //range[1] / (20 + (rand() % 40));
			zs = (float)(rand() % 100) / (1.0f * shards); //range[2] / (20 + (rand() % 40));
		}
		else
		{
			switch (thinaxis)
			{
				case 0:
					xs = 0.1f;
					ys = (float)(rand() % 100) / (1.0f * shards); //range[1] / (20 + (rand() % 40));
					zs = (float)(rand() % 100) / (1.0f * shards); //range[2] / (20 + (rand() % 40));
					break;

				case 1:
					xs = (float)(rand() % 100) / (1.0f * shards); //range[0] / (20 + (rand() % 40));
					ys = 0.1f;
					zs = (float)(rand() % 100) / (1.0f * shards); //range[2] / (20 + (rand() % 40));
					break;

				case 2:
					xs = (float)(rand() % 100) / (1.0f * shards); //range[0] / (20 + (rand() % 40));
					ys = (float)(rand() % 100) / (1.0f * shards); //range[1] / (20 + (rand() % 40));
					zs = 0.1f;
					break;
			}
		}

		// Scale based on type.
		switch (breaktype)
		{
			case 1: // Wood.
				xs *= 3;
				ys *= 3;
				zs *= 3;
				break;

			case 3: // Plastic.
				xs *= 2;
				ys *= 2;
				zs *= 2;
				break;

			case 4: // Metal.
			case 5:
			case 6:
				xs *= 7;
				ys *= 7;
				zs *= 7;
				break;

			case 7: // Stone.
			case 8:
				xs *= 7;
				ys *= 7;
				zs *= 7;
				break;
		}

		// Select model.
		modelcheck = rand() % 3;

		// Metal.
		if ((breaktype >= 4) && (breaktype <= 6))
		{
			if (modelcheck == 0)
			{
				hModel = cgs.media.metalFragments[0];
			}
			else if (modelcheck == 1)
			{
				hModel = cgs.media.metalFragments[1];
			}
			else
			{
				hModel = cgs.media.metalFragments[2];
			}
		}
		// Stone.
		else if (breaktype >= 7 && breaktype <= 8)
		{
			if (modelcheck == 0)
			{
				hModel = cgs.media.stoneFragments[0];
			}
			else if (modelcheck == 1)
			{
				hModel = cgs.media.stoneFragments[1];
			}
			else
			{
				hModel = cgs.media.stoneFragments[2];
			}
		}
		else
		{
			if (modelcheck == 0)
			{
				hModel = cgs.media.glassFragments[0];
			}
			else if (modelcheck == 1)
			{
				hModel = cgs.media.glassFragments[1];
			}
			else
			{
				hModel = cgs.media.glassFragments[2];
			}
		}

		// randomise starting point to a point within the object's bounds
		origin[0]	= UT_StdRand(es->origin[0], range[0]);
		origin[1]	= UT_StdRand(es->origin[1], range[1]);
		origin[2]	= UT_StdRand(es->origin[2], range[2]);

		velocity[0] = es->angles2[0] + FRAG_VELOCITY *crandom() * 2;
		velocity[1] = es->angles2[1] + FRAG_VELOCITY *crandom() * 2;
		velocity[2] = es->angles2[2] + (FRAG_VELOCITY / 5) * crandom() * 2;

		CG_LaunchFragment( origin, velocity, hModel, shader, breaktype, xs, ys, zs );
	}
}
#endif
//
// CG_Shards
// Dokta8
//
// Creates a bunch of polys in a tesselated plane
// and turns them into localentities for rendering
// in other words, it makes a smashed window

void CG_Shards( vec3_t mins, vec3_t maxs, vec3_t normal, int thinaxis, int shards )
{
}

// a, b, c: the master triangle to be tesselated
// tris: a pointer to an array (array[maxtris][4][3]) of triangles that will be filled
// with triangle data must be big enough to hold (maxtris * maxtris) * 3
// level - should be 0 when called outside the function
// NOTE: this function is NOT thread safe!!

void ut_tesselate( vec3_t a, vec3_t b, vec3_t c, float tris[UT_MAX_SHARDS][4][3], int level )
{
}

//
// CG_Ricochet
// Creates a visible ricochet effect
//
/*
===================
CG_Particles
by Dokta8
Creates some number of particles at <point> and ejects them into space
with an optional impact direction and surface normal (for bullets hitting
wall <from> the player against the <normal> of the wall
===================
*/
#define PART_MAXVEL 60
#define PART_MINVEL 5

/**
 * $(function)
 */
void CG_BulletImpact ( vec3_t point, vec3_t from, vec3_t normal, int bits )
{
}

/**
 * $(function)
 */
void CG_Ricochet( vec3_t point, vec3_t from, vec3_t normal, int bits, int surfaces )
{
}

//
// CG_LaunchParticle
// Used by all effects that launch a particle
//

/*void CG_LaunchParticle( vec3_t origin, vec3_t velocity, particle_t *part ) {

	localEntity_t*	le;
	refEntity_t*	re;

	// Get use a new entity to add.
	le = CG_AllocLocalEntity();
	if(!le)
		return;

	re = &le->refEntity;

	// Initialize the fragment
	le->leType			= LE_FRAGMENT;
	le->startTime		= cg.time;
	le->endTime 		= le->startTime + part->timeToLive - (part->deviation * (rand() % (part->timeToLive+1)));
	le->lifeRate		= 1.0f / (float)( le->endTime - le->startTime );

	// convert negtive values for radius into random number
	if (part->radius < 0.0f) {
		CG_Printf("Negative radius: %.2f ... ", part->radius);
		part->radius = ((random() * part->radius) * -1.0f) + 1.0f;
		CG_Printf("fixed to %.2f\n", part->radius);
	}

	// force at least a small radius
	if (part->radius == 0.0f) part->radius = 0.5f;

	CG_Printf( "Part->radius is %.2f\n", part->radius );

	le->radius			= part->radius;
	le->color[3]		= 1.0f;
	le->leFlags 		= (part->trail?LEF_PART_TRAIL:0) | (part->bounce?LEF_PART_BOUNCE:0) | LEF_PARTICLE;
	if ( !part->trtype ) {
		le->pos.trType		= TR_GRAVITY2;
	} else {
		le->pos.trType		= part->trtype;
	}
	le->pos.trTime		= cg.time;
	le->bounceFactor	= 0.3f;
	le->gravity 	= part->lightness;

	VectorCopy( origin,   le->pos.trBase );
	VectorCopy( velocity, le->pos.trDelta );

	// Setup the ref entity
	VectorCopy( origin, re->origin );

	AxisCopy( axisDefault, re->axis );

	re->reType		  = part->head.model? RT_MODEL:RT_SPRITE;
	re->rotation	  = 0;
	re->radius		  = part->radius;
	re->customShader  = part->head.shader;
	re->shaderRGBA[0] = 255.0f * part->head.color[0];
	re->shaderRGBA[1] = 255.0f * part->head.color[1];
	re->shaderRGBA[2] = 255.0f * part->head.color[2];
	re->shaderRGBA[3] = 255.0f * part->head.color[3];
	re->hModel		  = part->head.model;

	// Save the particle information for the trail and such
	memcpy( &le->part, part, sizeof(particle_t));
}*/

//
// CG_CreateEmitter
// Creates a local entity that is invisible, but which moves
// and leaves a trail of particles behind it.

// origin:		where emitter starts
// velocity:	initial velocity for emitter
// duration:	how long (in milliseconds) emitter lives
// particles:	how many particles to produce
// pbasevel:	the initial base velocity of generated particles
// randomness:	a number between 0 and 1 that determines how regular the emissions are (0 is regular, 1 random)
// trtype:		sets the emitter's movement behaviour, must be a TR_ constant
// part:		describes the particles this emitter generates

/*void CG_CreateEmitter (	vec3_t origin,
					vec3_t velocity,
					int duration,
					int particles,
					vec3_t pbasevel,
					float randomness,
					int trtype,
					particle_t *part ) {

	localEntity_t	*le;

	le = CG_AllocLocalEntity();

	// set up local entity data for emitter
	le->leType = LE_EMITTER;
	le->startTime = cg.time;
	le->endTime = le->startTime + duration;
	le->lifeRate = 1.0 / ( le->endTime - le->startTime );

	le->pos.trType = trtype;
	VectorCopy( origin, le->pos.trBase );
	VectorCopy( velocity, le->pos.trDelta );
	le->pos.trTime = cg.time;
	le->pos.trDuration = duration;

	VectorCopy (le->pos.trBase, le->refEntity.origin);	// needed for bounce

	// set up emitter data
	le->emit.current_particle = 0;
	le->emit.deviation = randomness;
	le->emit.total_particles = particles;
	VectorCopy( pbasevel, le->emit.velocity );

	le->emit.nextEmitTime = le->startTime + (rand() % (((le->endTime - le->startTime) / le->emit.total_particles)) * le->emit.deviation);

	// copy in particle data
	memcpy( &le->part, part, sizeof(particle_t) );
}*/

/*void CG_LaunchParticles (
	vec3_t		origin,
	vec3_t		from,
	vec3_t		normal,
	int 		count,
	float		radius,
	int 		minspeed,
	int 		maxspeed,
	particle_t* particle
	)
{
	int 		i;
	int 		contents;
	vec3_t		dir;
	vec3_t		vel;
	vec3_t		s;
	vec3_t		t;
	vec3_t		temp;

	// Dont do anything if the count is zero
	if(count == 0)
		return;

	// Get the contents so we can do some special things if water
	contents = trap_CM_PointContents( origin, 0 );

	if ( NULL == normal ) {
		VectorSet( normal, 0.0f, 0.0f, 1.0f );
	}

	if ( NULL == dir ) {
		VectorSet( dir, (rand() % 10000) - 5000, (rand() % 10000) - 5000, (rand() % 10000) - 5000 );
		VectorNormalizeNR( dir );
	}


	// Allow player to cap particles
	if (count > cg_maxFragments.integer)
		count = cg_maxFragments.integer;

	VectorSubtract( from, origin, temp );
	VectorNormalizeNR ( temp );

	// move a little off impact point
	VectorMA( origin, 2, normal, origin );

	RotatePointAroundVector(dir, normal, temp, 180);

	// s is one rotation 'direction
	PerpendicularVector( s, dir );

	// t is the other
	CrossProduct( dir, s, t );

	// Move it away from the wall a bit so it doesnt start in it
	VectorMA ( origin, 5, dir, origin );

	// Launch them particles
	for ( i = 0; i < count; i ++ )
	{
		vec3_t	up = {0,0,1};
		float	jump = 28.0f;

		if ( particle->up )
			jump = particle->up;

		RotatePointAroundVector( temp, s, dir, radius*(rand()%40 - 20) );
		RotatePointAroundVector( vel, t, temp, radius*(rand()%40 - 20) );
		VectorScale (vel, minspeed + rand()%(maxspeed-minspeed+1), vel);
		//VectorAdd ( vel, up, vel );
		VectorMA ( vel, jump, up, vel );

		CG_LaunchParticle ( origin, vel, particle );
	}
}*/
