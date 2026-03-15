/**
 * Filename: q_cvars.c
 *
 * Copyright (C) 1999-2005 Id Software, Inc.
 * Copyright (C) 2000-2009 FrozenSand
 *
 * This file is part of Urban Terror source code.
 *
 */

#include "q_cvars.h"
#include "c_syscalls.h"

/**
 * Com_Cvar_Register
 */
void Com_Cvar_Register(vmCvar_t *cvar,
		       const char *var_name,
		       const char *value,
		       int flags)
{
	/* Cvar name isn't handled by the engine, we do it here */
	cvar->name  = var_name;
	cvar->flags = flags;
	//Com_Printf("Registering %s with value %s.\n", var_name, value);
	trap_Cvar_Register(cvar, var_name, value, flags);
}

/**
 * Com_Cvar_Set
 */
void Com_Cvar_Set(vmCvar_t *cvar, const char *value)
{
	if (cvar->clamped)
		/* normal setting */
		trap_Cvar_Set(cvar->name, value);
	else
	{
		/* clamped setting */
		char  buffer[30];
		/*trap_Cvar_Set(cvar->name,
			      Q_ftoanz(Com_Clamp(cvar->min,
						 cvar->max, cvar->value),
				       buffer, sizeof(buffer)));*/
		trap_Cvar_Set(cvar->name,
			      va("%f",Com_Clamp(cvar->min,
						 cvar->max, cvar->value),
				       buffer, sizeof(buffer)));
	}
}

/**
 * Com_Cvar_Update
 *
 * Updates the cvar and returns true if it has been changed, otherwise false.
 */
qboolean Com_Cvar_Update(vmCvar_t *cvar)
{
	trap_Cvar_Update(cvar);

	if(cvar->oldModificationCount != cvar->modificationCount)
	{
		cvar->oldModificationCount = cvar->modificationCount;
		return qtrue;
	}
	return qfalse;
}

/**
 * Com_Cvar_Clamp
 */
void Com_Cvar_Clamp(vmCvar_t *cvar, float min, float max)
{
	cvar->clamped = qtrue;
	cvar->min     = min;
	cvar->max     = max;
	Com_Cvar_Set(cvar, cvar->string);
}
