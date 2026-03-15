/**
 * Filename: c_aries.h
 *
 * Copyright (C) 1999-2005 Id Software, Inc.
 * Copyright (C) 2000-2009 FrozenSand
 *
 * This file is part of Urban Terror source code.
 *
 */

#ifndef _C_ARIES_H_
#define _C_ARIES_H_

#include "../common/c_md3.h"
#include "../common/c_players.h"
#include "../common/c_arieshl.h"

#define ARIES_MAX_HITS 10

typedef struct ariesModelState_s {
	int    oldFrame;
	int    frame;
	float  backlerp;
	vec4_t rotation[4];
	vec3_t origin;
} ariesModelState_t;

typedef struct ariesModel_s {
	ariesModelState_t	  state;
	md3_t				 *md3;
	struct ariesModel_s **children;
	struct ariesModel_s  *parent;
} ariesModel_t;

typedef struct ariesPlayer_s {
	ariesModel_t *		 upper;
	ariesModel_t *		 lower;
	ariesModel_t *		 head;
	ariesModel_t *		 helmet;
} ariesPlayer_t;

typedef struct ariesTraceHit_s {
	ariesHitLocation_t	location;
	vec3_t			vecEntry;
	float			distance;
} ariesTraceHit_t;

typedef struct ariesTraceResult_s {
	vec3_t		 vecExit;
	vec3_t		 vecEntry;
	vec3_t		 vecExitNormal;
	vec3_t		 vecEntryNormal;
	float		 distExit;
	float		 distEntry;
	int 		 exitTriangle;
	int 		 entryTriangle;
	int 		 hitCount;
	ariesTraceHit_t  hits[ARIES_MAX_HITS];
} ariesTraceResult_t;

void		   C_AriesInit(void);
ariesModel_t * C_AriesLoadModel(const char *filename);
qboolean	   C_AriesAttachModels(ariesModel_t *dest, ariesModel_t *src, const char *tagName);
void		   C_AriesTrace(ariesModel_t* model, vec3_t origin, vec3_t dir, ariesTraceResult_t *result);
void		   C_AriesModelApplyMatrices(ariesModel_t *model, vec3_t in, vec3_t out);
void		   C_AriesMatrixTransform(ariesModel_t *model, float out[4][4]);
void		   C_AriesApplyTransform(float matrix[4][4], vec3_t vertex, vec3_t out);
void		   C_AriesApplyModelTagframe(ariesModel_t *model, md3Tag_t *t);

extern		   ariesPlayer_t c_aries[2];
#endif
