/**
 * Filename: g_aries.h
 *
 * Copyright (C) 1999-2005 Id Software, Inc.
 * Copyright (C) 2000-2009 FrozenSand
 *
 * This file is part of Urban Terror source code.
 *
 */

#include "g_local.h"

qboolean G_AriesTrace (gentity_t* ent, trace_t* tr, vec3_t muzzle, vec3_t end, ariesTraceResult_t *result);
qboolean G_AriesTrace (gentity_t* ent, trace_t* tr, vec3_t muzzle, vec3_t end, ariesTraceResult_t *result);
void C_AriesSortResult(ariesTraceResult_t* result);
void G_AriesInit(void);
