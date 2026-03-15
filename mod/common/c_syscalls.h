/**
 * Filename: c_syscalls.h
 *
 * Copyright (C) 1999-2005 Id Software, Inc.
 * Copyright (C) 2000-2009 FrozenSand
 *
 * This file is part of Urban Terror source code.
 *
 */

#ifndef _C_SYSCALLS_H_
#define _C_SYSCALLS_H_

#include "../game/q_shared.h"

void	 trap_Printf( const char *fmt );
void	 trap_Error( const char *fmt );
int 	 trap_Milliseconds( void );
int 	 trap_Argc( void );
void	 trap_Argv( int n, char *buffer, int bufferLength );
void	 trap_Args( char *buffer, int bufferLength );
int 	 trap_FS_FOpenFile( const char *qpath, fileHandle_t *f, fsMode_t mode );
void	 trap_FS_Read( void *buffer, int len, fileHandle_t f );
void	 trap_FS_Write( const void *buffer, int len, fileHandle_t f );
void	 trap_FS_FCloseFile( fileHandle_t f );
int 	 trap_FS_GetFileList( const char *path, const char *extension, char *listbuf, int bufsize );
void	 trap_SendConsoleCommand( int exec_when, const char *text );
void	 trap_Cvar_Register( vmCvar_t *cvar, const char *var_name, const char *value, int flags );
void	 trap_Cvar_Update( vmCvar_t *cvar );
void	 trap_Cvar_Set( const char *var_name, const char *value );
int 	 trap_Cvar_VariableIntegerValue( const char *var_name );
float	 trap_Cvar_VariableValue( const char *var_name );
void	 trap_Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize );
int 	 trap_PC_LoadSource( const char *filename );
int 	 trap_PC_FreeSource( int handle );
int 	 trap_PC_ReadToken( int handle, pc_token_t *pc_token );
int 	 trap_PC_SourceFileAndLine( int handle, char *filename, int *line );

#endif /* _C_SYSCALLS_H_ */
