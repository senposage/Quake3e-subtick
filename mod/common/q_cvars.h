/**
 * Filename: q_cvars.h
 *
 * Copyright (C) 1999-2005 Id Software, Inc.
 * Copyright (C) 2000-2009 FrozenSand
 *
 * This file is part of Urban Terror source code.
 *
 */

#ifndef _Q_CVARS_H_
#define _Q_CVARS_H_

#define CVAR_DEF
#include "../game/q_shared.h"

/**
 *
 * CVARS (console variables)
 *
 * Many variables can be used for cheating purposes, so when
 * cheats is zero, force all unspecified variables to their
 * default values.
 */

#define CVAR_ARCHIVE		1	 /*  system-wide configuration saving */
#define CVAR_USERINFO		2	 /* sent to server on connect or change  */
#define CVAR_SERVERINFO 	4	 /* sent in response to front end requests */
#define CVAR_SYSTEMINFO 	8	 /* these cvars will be dup. on all clients */
#define CVAR_INIT			16	 /* command line only cvar */
#define CVAR_LATCH			32	 /* will only change when C code next does */
/*
 * a Cvar_Get(), so it can't be changed
 * without proper initialization.  modified
 * will be set, even though the value hasn't
 * changed yet
 */
#define CVAR_ROM			64	 /* display only, cannot be set by user */
#define CVAR_USER_CREATED	128  /* created by a set command */
#define CVAR_TEMP			256  /* not archived */
#define CVAR_CHEAT			512  /* cannot be changed if cheats are disabled */
#define CVAR_NORESTART		1024 /* don't clear when cvar_restart is issued */
#define CVAR_SERVER_CREATED 2048 /* cvar was created by a server to the clnt */
/*
 * mod-defined CVAR flags
 */
#define CVAR_TRACK			4096 /* track the cvar */
#define CVAR_TEAMSHADER 	8192 /* related to the team shader */

#define CVAR_NONEXISTENT	0xFFFFFFFF		/* cvar doesn't exist */

/* nothing outside the Cvar_*() functions should modify these fields! */
typedef struct cvar_s {
	char	  *name;
	char	  *string;
	char	  *resetString; 	/* cvar_restart will reset to this value */
	char	  *latchedString;	/* for CVAR_LATCH vars */
	int 	  flags;
	qboolean  modified; 		/* set each time the cvar is changed */
	int 	  modificationCount;/* incremented each time the cvar is changed */
	float	  value;			/* atof( string ) */
	int 	  integer;			/* atoi( string ) */
	qboolean  validate;
	qboolean  integral;
	float	  min;
	float	  max;
	struct cvar_s *next;
	struct cvar_s *hashNext;
} cvar_t;

#define MAX_CVAR_VALUE_STRING 256

typedef int cvarHandle_t;

/*
 * the modules that run in the virtual machine can't access the cvar_t directly,
 *
 * so they must ask for structured updates
 */
typedef struct {
	cvarHandle_t  handle;
	int 		  modificationCount;
	float		  value;
	int 		  integer;
	char		  string[MAX_CVAR_VALUE_STRING];

	/* Mod-defined fields */
	const char	 *name;
	int 		  flags;
	int 		  oldModificationCount;
	qboolean	  clamped;
	float		  min;
	float		  max;
} vmCvar_t;

void Com_Cvar_Register(vmCvar_t *cvar, const char *var_name,
				  const char *value, int flags);
void Com_Cvar_Set(vmCvar_t *cvar, const char *value);
qboolean Com_Cvar_Update(vmCvar_t *cvar);

#endif /* _Q_CVARS_H_ */
