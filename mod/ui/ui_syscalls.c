// Copyright (C) 1999-2000 Id Software, Inc.
//
#include "ui_local.h"

// this file is only included when building a dll
// syscalls.asm is included instead when building a qvm

static int  (QDECL * syscall)( int arg, ... ) = (int (QDECL *)(int, ...)) - 1;

/**
 * $(function)
 */
DLLEXPORT void dllEntry( int (QDECL * syscallptr)( int arg, ... ))
{
	syscall = syscallptr;
}

/**
 * $(function)
 */
int PASSFLOAT( float x )
{
	float  floatTemp;

	floatTemp = x;
	return *(int *)&floatTemp;
}

/**
 * $(function)
 */
void trap_Print( const char *string )
{
	syscall( UI_PRINT, string );
}

/**
 * $(function)
 */
void trap_Error( const char *string )
{
	syscall( UI_ERROR, string );
}

/**
 * $(function)
 */
int trap_Milliseconds( void )
{
	return syscall( UI_MILLISECONDS );
}

/**
 * $(function)
 */
void trap_Cvar_Register( vmCvar_t *cvar, const char *var_name, const char *value, int flags )
{
	syscall( UI_CVAR_REGISTER, cvar, var_name, value, flags );
}

/**
 * $(function)
 */
void trap_Cvar_Update( vmCvar_t *cvar )
{
	syscall( UI_CVAR_UPDATE, cvar );
}

/**
 * $(function)
 */
void trap_Cvar_Set( const char *var_name, const char *value )
{
	syscall( UI_CVAR_SET, var_name, value );
}

/**
 * $(function)
 */
float trap_Cvar_VariableValue( const char *var_name )
{
	int  temp;

	temp = syscall( UI_CVAR_VARIABLEVALUE, var_name );
	return *(float *)&temp;
}

/**
 * $(function)
 */
void trap_Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize )
{
	syscall( UI_CVAR_VARIABLESTRINGBUFFER, var_name, buffer, bufsize );
}

/**
 * $(function)
 */
void trap_Cvar_SetValue( const char *var_name, float value )
{
	syscall( UI_CVAR_SETVALUE, var_name, PASSFLOAT( value ));
}

/**
 * $(function)
 */
void trap_Cvar_Reset( const char *name )
{
	syscall( UI_CVAR_RESET, name );
}

/**
 * $(function)
 */
void trap_Cvar_Create( const char *var_name, const char *var_value, int flags )
{
	syscall( UI_CVAR_CREATE, var_name, var_value, flags );
}

/**
 * $(function)
 */
void trap_Cvar_InfoStringBuffer( int bit, char *buffer, int bufsize )
{
	syscall( UI_CVAR_INFOSTRINGBUFFER, bit, buffer, bufsize );
}

/**
 * $(function)
 */
int trap_Argc( void )
{
	return syscall( UI_ARGC );
}

/**
 * $(function)
 */
void trap_Argv( int n, char *buffer, int bufferLength )
{
	syscall( UI_ARGV, n, buffer, bufferLength );
}

/**
 * $(function)
 */
void trap_Cmd_ExecuteText( int exec_when, const char *text )
{
	syscall( UI_CMD_EXECUTETEXT, exec_when, text );
}

/**
 * $(function)
 */
int trap_FS_FOpenFile( const char *qpath, fileHandle_t *f, fsMode_t mode )
{
	return syscall( UI_FS_FOPENFILE, qpath, f, mode );
}

/**
 * $(function)
 */
void trap_FS_Read( void *buffer, int len, fileHandle_t f )
{
	syscall( UI_FS_READ, buffer, len, f );
}

/**
 * $(function)
 */
void trap_FS_Write( const void *buffer, int len, fileHandle_t f )
{
	syscall( UI_FS_WRITE, buffer, len, f );
}

/**
 * $(function)
 */
void trap_FS_FCloseFile( fileHandle_t f )
{
	syscall( UI_FS_FCLOSEFILE, f );
}

/**
 * $(function)
 */
int trap_FS_GetFileList(  const char *path, const char *extension, char *listbuf, int bufsize )
{
	return syscall( UI_FS_GETFILELIST, path, extension, listbuf, bufsize );
}

/**
 * $(function)
 */
qhandle_t trap_R_RegisterModel( const char *name )
{
	return syscall( UI_R_REGISTERMODEL, name );
}

/**
 * $(function)
 */
qhandle_t trap_R_RegisterSkin( const char *name )
{
	return syscall( UI_R_REGISTERSKIN, name );
}

/**
 * $(function)
 */
void trap_R_RegisterFont(const char *fontName, int pointSize, fontInfo_t *font)
{
	syscall( UI_R_REGISTERFONT, fontName, pointSize, font );
}

/**
 * $(function)
 */
qhandle_t trap_R_RegisterShaderNoMip(const char *name)
{
	return syscall(UI_R_REGISTERSHADERNOMIP, name);
}

/**
 * $(function)
 */
void trap_R_ClearScene( void )
{
	syscall( UI_R_CLEARSCENE );
}

/**
 * $(function)
 */
void trap_R_AddRefEntityToScene( const refEntity_t *re )
{
	syscall( UI_R_ADDREFENTITYTOSCENE, re );
}

/**
 * $(function)
 */
void trap_R_AddPolyToScene( qhandle_t hShader, int numVerts, const polyVert_t *verts )
{
	syscall( UI_R_ADDPOLYTOSCENE, hShader, numVerts, verts );
}

/**
 * $(function)
 */
void trap_R_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b )
{
	syscall( UI_R_ADDLIGHTTOSCENE, org, PASSFLOAT(intensity), PASSFLOAT(r), PASSFLOAT(g), PASSFLOAT(b));
}

/**
 * $(function)
 */
void trap_R_RenderScene( const refdef_t *fd )
{
	syscall( UI_R_RENDERSCENE, fd );
}

/**
 * $(function)
 */
void trap_R_SetColor( const float *rgba )
{
	syscall( UI_R_SETCOLOR, rgba );
}

/**
 * $(function)
 */
void trap_R_DrawStretchPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader )
{
	syscall( UI_R_DRAWSTRETCHPIC, PASSFLOAT(x), PASSFLOAT(y), PASSFLOAT(w), PASSFLOAT(h), PASSFLOAT(s1), PASSFLOAT(t1), PASSFLOAT(s2), PASSFLOAT(t2), hShader );
}

/**
 * $(function)
 */
void	trap_R_ModelBounds( clipHandle_t model, vec3_t mins, vec3_t maxs )
{
	syscall( UI_R_MODELBOUNDS, model, mins, maxs );
}

/**
 * $(function)
 */
void trap_UpdateScreen( void )
{
	syscall( UI_UPDATESCREEN );
}

/**
 * $(function)
 */
int trap_CM_LerpTag( orientation_t *tag, clipHandle_t mod, int startFrame, int endFrame, float frac, const char *tagName )
{
	return syscall( UI_CM_LERPTAG, tag, mod, startFrame, endFrame, PASSFLOAT(frac), tagName );
}

/**
 * $(function)
 */
void trap_S_StartLocalSound( sfxHandle_t sfx, int channelNum )
{
	syscall( UI_S_STARTLOCALSOUND, sfx, channelNum );
}

/**
 * $(function)
 */
sfxHandle_t	trap_S_RegisterSound( const char *sample, qboolean compressed )
{
	return syscall( UI_S_REGISTERSOUND, sample, compressed );
}

/**
 * $(function)
 */
void trap_Key_KeynumToStringBuf( int keynum, char *buf, int buflen )
{
	syscall( UI_KEY_KEYNUMTOSTRINGBUF, keynum, buf, buflen );
}

/**
 * $(function)
 */
void trap_Key_GetBindingBuf( int keynum, char *buf, int buflen )
{
	syscall( UI_KEY_GETBINDINGBUF, keynum, buf, buflen );
}

/**
 * $(function)
 */
void trap_Key_SetBinding( int keynum, const char *binding )
{
	syscall( UI_KEY_SETBINDING, keynum, binding );
}

/**
 * $(function)
 */
qboolean trap_Key_IsDown( int keynum )
{
	return syscall( UI_KEY_ISDOWN, keynum );
}

/**
 * $(function)
 */
qboolean trap_Key_GetOverstrikeMode( void )
{
	return syscall( UI_KEY_GETOVERSTRIKEMODE );
}

/**
 * $(function)
 */
void trap_Key_SetOverstrikeMode( qboolean state )
{
	syscall( UI_KEY_SETOVERSTRIKEMODE, state );
}

/**
 * $(function)
 */
void trap_Key_ClearStates( void )
{
	syscall( UI_KEY_CLEARSTATES );
}

/**
 * $(function)
 */
int trap_Key_GetCatcher( void )
{
	return syscall( UI_KEY_GETCATCHER );
}

/**
 * $(function)
 */
void trap_Key_SetCatcher( int catcher )
{
	syscall( UI_KEY_SETCATCHER, catcher );
}

/**
 * $(function)
 */
void trap_GetClipboardData( char *buf, int bufsize )
{
	syscall( UI_GETCLIPBOARDDATA, buf, bufsize );
}

/**
 * $(function)
 */
void trap_GetClientState( uiClientState_t *state )
{
	syscall( UI_GETCLIENTSTATE, state );
}

/**
 * $(function)
 */
void trap_GetGlconfig( glconfig_t *glconfig )
{
	syscall( UI_GETGLCONFIG, glconfig );
}

/**
 * $(function)
 */
int trap_GetConfigString( int index, char *buff, int buffsize )
{
	return syscall( UI_GETCONFIGSTRING, index, buff, buffsize );
}

/**
 * $(function)
 */
int	trap_LAN_GetServerCount( int source )
{
	return syscall( UI_LAN_GETSERVERCOUNT, source );
}

/**
 * $(function)
 */
void trap_LAN_GetServerAddressString( int source, int n, char *buf, int buflen )
{
	syscall( UI_LAN_GETSERVERADDRESSSTRING, source, n, buf, buflen );
}

/**
 * $(function)
 */
void trap_LAN_GetServerInfo( int source, int n, char *buf, int buflen )
{
	syscall( UI_LAN_GETSERVERINFO, source, n, buf, buflen );
}

/**
 * $(function)
 */
int trap_LAN_GetServerPing( int source, int n )
{
	return syscall( UI_LAN_GETSERVERPING, source, n );
}

/**
 * $(function)
 */
int trap_LAN_GetPingQueueCount( void )
{
	return syscall( UI_LAN_GETPINGQUEUECOUNT );
}

/**
 * $(function)
 */
int trap_LAN_ServerStatus( const char *serverAddress, char *serverStatus, int maxLen )
{
	return syscall( UI_LAN_SERVERSTATUS, serverAddress, serverStatus, maxLen );
}

/**
 * $(function)
 */
void trap_LAN_SaveCachedServers()
{
	syscall( UI_LAN_SAVECACHEDSERVERS );
}

/**
 * $(function)
 */
void trap_LAN_LoadCachedServers()
{
	syscall( UI_LAN_LOADCACHEDSERVERS );
}

/**
 * $(function)
 */
void trap_LAN_ResetPings(int n)
{
	syscall( UI_LAN_RESETPINGS, n );
}

/**
 * $(function)
 */
void trap_LAN_ClearPing( int n )
{
	syscall( UI_LAN_CLEARPING, n );
}

/**
 * $(function)
 */
void trap_LAN_GetPing( int n, char *buf, int buflen, int *pingtime )
{
	syscall( UI_LAN_GETPING, n, buf, buflen, pingtime );
}

/**
 * $(function)
 */
void trap_LAN_GetPingInfo( int n, char *buf, int buflen )
{
	syscall( UI_LAN_GETPINGINFO, n, buf, buflen );
}

/**
 * $(function)
 */
void trap_LAN_MarkServerVisible( int source, int n, qboolean visible )
{
	syscall( UI_LAN_MARKSERVERVISIBLE, source, n, visible );
}

/**
 * $(function)
 */
int trap_LAN_ServerIsVisible( int source, int n)
{
	return syscall( UI_LAN_SERVERISVISIBLE, source, n );
}

/**
 * $(function)
 */
qboolean trap_LAN_UpdateVisiblePings( int source )
{
	return syscall( UI_LAN_UPDATEVISIBLEPINGS, source );
}

/**
 * $(function)
 */
int trap_LAN_AddServer(int source, const char *name, const char *addr)
{
	return syscall( UI_LAN_ADDSERVER, source, name, addr );
}

/**
 * $(function)
 */
void trap_LAN_RemoveServer(int source, const char *addr)
{
	syscall( UI_LAN_REMOVESERVER, source, addr );
}

/**
 * $(function)
 */
int trap_LAN_CompareServers( int source, int sortKey, int sortDir, int s1, int s2 )
{
	return syscall( UI_LAN_COMPARESERVERS, source, sortKey, sortDir, s1, s2 );
}

/**
 * $(function)
 */
int trap_MemoryRemaining( void )
{
	return syscall( UI_MEMORY_REMAINING );
}

/**
 * $(function)
 */
void trap_GetCDKey( char *buf, int buflen )
{
	syscall( UI_GET_CDKEY, buf, buflen );
}

/**
 * $(function)
 */
void trap_SetCDKey( char *buf )
{
	syscall( UI_SET_CDKEY, buf );
}

/**
 * $(function)
 */
int trap_PC_AddGlobalDefine( char *define )
{
	return syscall( UI_PC_ADD_GLOBAL_DEFINE, define );
}

/**
 * $(function)
 */
int trap_PC_LoadSource( const char *filename )
{
	return syscall( UI_PC_LOAD_SOURCE, filename );
}

/**
 * $(function)
 */
int trap_PC_FreeSource( int handle )
{
	return syscall( UI_PC_FREE_SOURCE, handle );
}

/**
 * $(function)
 */
int trap_PC_ReadToken( int handle, pc_token_t *pc_token )
{
	return syscall( UI_PC_READ_TOKEN, handle, pc_token );
}

/**
 * $(function)
 */
int trap_PC_SourceFileAndLine( int handle, char *filename, int *line )
{
	return syscall( UI_PC_SOURCE_FILE_AND_LINE, handle, filename, line );
}

/**
 * $(function)
 */
void trap_S_StopBackgroundTrack( void )
{
	syscall( UI_S_STOPBACKGROUNDTRACK );
}

/**
 * $(function)
 */
void trap_S_StartBackgroundTrack( const char *intro, const char *loop)
{
	syscall( UI_S_STARTBACKGROUNDTRACK, intro, loop );
}

/**
 * $(function)
 */
int trap_RealTime(qtime_t *qtime)
{
	return syscall( UI_REAL_TIME, qtime );
}

// this returns a handle.  arg0 is the name in the format "idlogo.roq", set arg1 to NULL, alteredstates to qfalse (do not alter gamestate)
int trap_CIN_PlayCinematic( const char *arg0, int xpos, int ypos, int width, int height, int bits)
{
	return syscall(UI_CIN_PLAYCINEMATIC, arg0, xpos, ypos, width, height, bits);
}

// stops playing the cinematic and ends it.  should always return FMV_EOF
// cinematics must be stopped in reverse order of when they are started
e_status trap_CIN_StopCinematic(int handle)
{
	return syscall(UI_CIN_STOPCINEMATIC, handle);
}

// will run a frame of the cinematic but will not draw it.  Will return FMV_EOF if the end of the cinematic has been reached.
e_status trap_CIN_RunCinematic (int handle)
{
	return syscall(UI_CIN_RUNCINEMATIC, handle);
}

// draws the current frame
void trap_CIN_DrawCinematic (int handle)
{
	syscall(UI_CIN_DRAWCINEMATIC, handle);
}

// allows you to resize the animation dynamically
void trap_CIN_SetExtents (int handle, int x, int y, int w, int h)
{
	syscall(UI_CIN_SETEXTENTS, handle, x, y, w, h);
}

/**
 * $(function)
 */
void	trap_R_RemapShader( const char *oldShader, const char *newShader, const char *timeOffset )
{
	syscall( UI_R_REMAP_SHADER, oldShader, newShader, timeOffset );
}

/**
 * $(function)
 */
qboolean trap_VerifyCDKey( const char *key, const char *chksum)
{
	return syscall( UI_VERIFY_CDKEY, key, chksum);
}

int trap_Q_vsnprintf(char *text, size_t maxlen, const char *fmt, va_list ap)
{
	return syscall( UI_Q_VSNPRINTF, text, maxlen, fmt, ap);
}

#ifdef USE_AUTH
qboolean trap_NET_StringToAdr ( const char *s, netadr_t *a)
{
	return syscall( UI_NET_STRINGTOADR, s, a);
}

void trap_NET_SendPacket (netsrc_t sock, int length, const void *data, const char *to)
{
	syscall( UI_NET_SENDPACKET, sock, length, data, to);
}

qboolean trap_NET_CompareBaseAdr (netadr_t a, netadr_t b)
{
	return syscall( UI_NET_COMPAREBASEADR );
}
#endif

//char *trap_CopyString( const char *in )
//{
//	return syscall( UI_COPYSTRING, in);
//}

void trap_Sys_StartProcess( char *exeName, qboolean doexit )
{
	syscall( UI_SYS_STARTPROCESS, exeName, doexit );
}
