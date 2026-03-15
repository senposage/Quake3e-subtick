/**
 * Filename: c_cvars.c
 *
 * Copyright (C) 1999-2005 Id Software, Inc.
 * Copyright (C) 2000-2009 FrozenSand
 *
 * This file is part of Urban Terror source code.
 *
 */

#include "c_cvars.h"

#define CVAR_C_DECLARE
#include "c_cvars.def"

/**
 * C_RegisterCvars
 */
void C_RegisterCvars(void)
{
#define CVAR_C_REGISTER
#include "c_cvars.def"
}

/**
 * C_UpdateCvar
 */
void C_UpdateCvar(vmCvar_t *cvar)
{
	/* update and see if it is tracked */
	Com_Cvar_Update(cvar);
}

/**
 * C_UpdateCvars
 */
void C_UpdateCvars(void)
{
#define CVAR_C_UPDATE
#include "c_cvars.def"
}
