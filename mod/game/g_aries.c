/**
 * Filename: g_aries.c
 *
 * Copyright (C) 1999-2005 Id Software, Inc.
 * Copyright (C) 2000-2009 FrozenSand
 *
 * This file is part of Urban Terror source code.
 *
 */
#include "g_aries.h"

void C_AriesTraceInternal(ariesModel_t *model, vec3_t origin, vec3_t dir,
		ariesTraceResult_t *result);


/**
 * G_AriesTrace
 */
qboolean G_AriesTrace (gentity_t* ent, trace_t* tr, vec3_t muzzle, vec3_t end, ariesTraceResult_t *result)
{
	gentity_t*		traceEnt;
	vec3_t			dir;
	//int 			hit = 0;
	//float			angle = 0;
	//int 			i;
	/* holds 3 vectors: angles for torso, legs and head */
	angleData_t 	adata;
	/* not used here, but passed to the func for compat. with cgame */
	int 			pryFlags;
	int 			model;
	vec3_t			legsAxis[3];
	vec3_t			torsoAxis[3];
	vec3_t			headAxis[3];

	/* Get the original entity that was hit. */
	traceEnt = &g_entities[tr->entityNum];

	VectorSubtract(end, muzzle, dir);
	VectorNormalizeNR(dir);

	model = traceEnt->client->pers.ariesModel;

	/*
	 * BG_PlayerAngles is an implementation of CG_Player angles and is
	 * now called by both cgame and here in game so that the results are
	 * the same.  Note that for the netcode this is all assumed to be
	 * re-wound so we need to use values that are accounted for in the
	 * history buffer
	 * the critical ones are s.apos.trBase, s.pos.trDelta, and the 4
	 * legs, torso, pitch yaws
	 */
	BG_PlayerAngles(traceEnt->s.apos.trBase,
		traceEnt->s.pos.trDelta,
		traceEnt->client->animationdata.legs.frame,
		traceEnt->client->animationdata.torso.frame,
		ent->client->AttackTime - ent->client->ps.commandTime,
		&adata,
		&traceEnt->client->legsPitchAngle,
		&traceEnt->client->legsYawAngle,
		&traceEnt->client->torsoPitchAngle,
		&traceEnt->client->torsoYawAngle,
		&pryFlags );

	/* clear the current animation state */
	memset(&(c_aries[model].lower->state), 0, sizeof(ariesModelState_t));
	memset(&(c_aries[model].upper->state), 0, sizeof(ariesModelState_t));
	memset(&(c_aries[model].head->state), 0, sizeof(ariesModelState_t));
	memset(&(c_aries[model].helmet->state), 0, sizeof(ariesModelState_t));
	/*
	Com_Printf("G adata: {%f %f %f} {%f %f %f} {%f %f %f}\n",
			adata.legAngles[0], adata.legAngles[1],
			adata.legAngles[2],
			adata.torsoAngles[0], adata.torsoAngles[1],
			adata.torsoAngles[2],
			adata.headAngles[0], adata.headAngles[1],
			adata.headAngles[2]);
	*/

	/* now set the positions of the player model */
	VectorCopy(traceEnt->s.pos.trBase, c_aries[model].lower->state.origin);
	AnglesToAxis(adata.legAngles, legsAxis);
	AnglesToAxis(adata.torsoAngles, torsoAxis);
	AnglesToAxis(adata.headAngles, headAxis);

	/*
	//@Barbatos - scale booster for the legs in case we use the old player models
	VectorScale(legsAxis[0], 1.15, legsAxis[0]);
	VectorScale(legsAxis[1], 1.15, legsAxis[1]);
	VectorScale(legsAxis[2], 1.15, legsAxis[2]);
	c_aries[model].lower->state.origin[2] += 4;
	*/

	MatrixConvert34(legsAxis, c_aries[model].lower->state.rotation);
	MatrixConvert34(torsoAxis, c_aries[model].upper->state.rotation);
	MatrixConvert34(headAxis, c_aries[model].head->state.rotation);
	MatrixConvert34(headAxis, c_aries[model].helmet->state.rotation);

	/* set the animations */
	c_aries[model].lower->state.oldFrame =
		traceEnt->client->animationdata.legs.oldFrame;
	c_aries[model].lower->state.frame =
		traceEnt->client->animationdata.legs.frame;
	c_aries[model].lower->state.backlerp =
		traceEnt->client->animationdata.legs.backlerp;
	c_aries[model].upper->state.oldFrame =
		traceEnt->client->animationdata.torso.oldFrame;
	c_aries[model].upper->state.frame =
		traceEnt->client->animationdata.torso.frame;
	c_aries[model].upper->state.backlerp =
		traceEnt->client->animationdata.torso.backlerp;

//	  C_AriesTrace(c_aries[model].lower, muzzle, dir, result); INLINED
	result->hitCount = 0;
	C_AriesTraceInternal(c_aries[model].lower, muzzle, dir, result);
	C_AriesSortResult(result);

	if (result->hitCount) {
		VectorCopy(result->vecEntry, tr->endpos);
		VectorCopy(result->vecEntryNormal, tr->plane.normal);
		tr->entityNum = traceEnt->s.number;
		return qtrue;
	}
	return qfalse;
}

/**
 * G_AriesInit
 */
void G_AriesInit(void)
{
	C_AriesInit();
}
