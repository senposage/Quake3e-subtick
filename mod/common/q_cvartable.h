/**
 * Filename: q_cvars.h
 *
 * Copyright (C) 1999-2005 Id Software, Inc.
 * Copyright (C) 2000-2009 FrozenSand
 *
 * This file is part of Urban Terror source code.
 *
 */

#ifndef CVAR_PREFIX
 #define CVAR_PREFIX(ENTRY) ENTRY
#endif
#ifndef CVAR_UPDATE
 #define CVAR_UPDATE(X)     Com_Cvar_Update(X)
#endif

#ifdef CVAR_C_DECLARE
 #define CVAR(ENTRY, DEFAULT, FLAGS) vmCvar_t CVAR_PREFIX(ENTRY)
 #define CVAR_CLAMP(ENTRY, MIN, MAX)
#endif

#ifdef CVAR_H_DECLARE
 #define CVAR(ENTRY, DEFAULT, FLAGS) extern vmCvar_t CVAR_PREFIX(ENTRY)
 #define CVAR_CLAMP(ENTRY, MIN, MAX)
#endif

#ifdef CVAR_C_REGISTER
 #define CVAR(ENTRY, DEFAULT, FLAGS) \
	Com_Cvar_Register(&CVAR_PREFIX(ENTRY), \
			  CVAR_PREFIX_STRING # ENTRY, \
			  DEFAULT, \
			  FLAGS)
 #define CVAR_CLAMP(ENTRY, MIN, MAX) Com_Cvar_Clamp(&CVAR_PREFIX(\
							    ENTRY), MIN, MAX)
#endif

#ifdef CVAR_C_UPDATE
 #define CVAR(ENTRY, DEFAULT, FLAGS) CVAR_UPDATE(&CVAR_PREFIX(ENTRY))
 #define CVAR_CLAMP(ENTRY, MIN, MAX)
#endif
