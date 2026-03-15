/**
 * Filename: c_cvars.h
 *
 * Copyright (C) 1999-2005 Id Software, Inc.
 * Copyright (C) 2000-2009 FrozenSand
 *
 * This file is part of Urban Terror source code.
 *
 */

#ifndef _C_CVARS_H_
#define _C_CVARS_H_

#define MODULE_PREFIX C_
#define CVAR_UPDATE(X) C_UpdateCvar(X)

#include "../common/q_cvars.h"

void C_RegisterCvars(void);
void C_UpdateCvar(vmCvar_t *cvar);
void C_UpdateCvars(void);

#define CVAR_H_DECLARE
#include "../common/c_cvars.def"

#endif /* _C_CVARS_H_ */
