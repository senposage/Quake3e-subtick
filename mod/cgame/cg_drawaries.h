/**
 * Filename: cg_aries.h
 *
 * Copyright (C) 1999-2005 Id Software, Inc.
 * Copyright (C) 2000-2009 FrozenSand
 *
 * This file is part of Urban Terror source code.
 *
 */

#ifndef _CG_ARIES_H_
#define _CG_ARIES_H_

#include "../common/c_aries.h"
#include "cg_local.h"

void CG_AriesDrawModel(ariesModel_t *model, qboolean server);
void CG_DrawAries(int model, vec3_t origin, refEntity_t *legs,
	refEntity_t *torso, refEntity_t *head);
void CG_DrawAriesEntity(centity_t *ent);

#endif /* _CG_ARIES_H_ */
