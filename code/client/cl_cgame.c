/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// cl_cgame.c  -- client system interaction with client game

#include "client.h"

#include "../botlib/botlib.h"

extern	botlib_export_t	*botlib_export;

//extern qboolean loadCamera(const char *name);
//extern void startCamera(int time);
//extern qboolean getCameraInfo(int time, vec3_t *origin, vec3_t *angles);

/*
====================
CL_GetGameState
====================
*/
static void CL_GetGameState( gameState_t *gs ) {
	*gs = cl.gameState;
}


/*
====================
CL_GetGlconfig
====================
*/
static void CL_GetGlconfig( glconfig_t *glconfig ) {
	*glconfig = cls.glconfig;
}


/*
====================
CL_GetUserCmd
====================
*/
static qboolean CL_GetUserCmd( int cmdNumber, usercmd_t *ucmd ) {
	// cmds[cmdNumber] is the last properly generated command

	// can't return anything that we haven't created yet
	if ( cl.cmdNumber - cmdNumber < 0 ) {
		Com_Error( ERR_DROP, "CL_GetUserCmd: cmdNumber (%i) > cl.cmdNumber (%i)", cmdNumber, cl.cmdNumber );
	}

	// the usercmd has been overwritten in the wrapping
	// buffer because it is too far out of date
	if ( cl.cmdNumber - cmdNumber >= CMD_BACKUP ) {
		return qfalse;
	}

	*ucmd = cl.cmds[ cmdNumber & CMD_MASK ];

	return qtrue;
}


/*
====================
CL_GetCurrentCmdNumber
====================
*/
static int CL_GetCurrentCmdNumber( void ) {
	return cl.cmdNumber;
}


/*
====================
CL_GetCurrentSnapshotNumber
====================
*/
static void CL_GetCurrentSnapshotNumber( int *snapshotNumber, int *serverTime ) {
	*snapshotNumber = cl.snap.messageNum;
	*serverTime = cl.snap.serverTime;
}


/*
====================
CL_GetSnapshot
====================
*/
static qboolean CL_GetSnapshot( int snapshotNumber, snapshot_t *snapshot ) {
	clSnapshot_t	*clSnap;
	int				i, count;

	if ( cl.snap.messageNum - snapshotNumber < 0 ) {
		Com_Error( ERR_DROP, "CL_GetSnapshot: snapshotNumber (%i) > cl.snapshot.messageNum (%i)", snapshotNumber, cl.snap.messageNum );
	}

	// if the frame has fallen out of the circular buffer, we can't return it
	if ( cl.snap.messageNum - snapshotNumber >= PACKET_BACKUP ) {
		return qfalse;
	}

	// if the frame is not valid, we can't return it
	clSnap = &cl.snapshots[snapshotNumber & PACKET_MASK];
	if ( !clSnap->valid ) {
		return qfalse;
	}

	// if the entities in the frame have fallen out of their
	// circular buffer, we can't return it
	if ( cl.parseEntitiesNum - clSnap->parseEntitiesNum >= MAX_PARSE_ENTITIES ) {
		return qfalse;
	}

	// write the snapshot
	snapshot->snapFlags = clSnap->snapFlags;
	snapshot->serverCommandSequence = clSnap->serverCommandNum;
	snapshot->ping = clSnap->ping;
	snapshot->serverTime = clSnap->serverTime;
	Com_Memcpy( snapshot->areamask, clSnap->areamask, sizeof( snapshot->areamask ) );
	snapshot->ps = clSnap->ps;
	count = clSnap->numEntities;
	if ( count > MAX_ENTITIES_IN_SNAPSHOT ) {
		Com_DPrintf( "CL_GetSnapshot: truncated %i entities to %i\n", count, MAX_ENTITIES_IN_SNAPSHOT );
		count = MAX_ENTITIES_IN_SNAPSHOT;
	}
	snapshot->numEntities = count;
	for ( i = 0 ; i < count ; i++ ) {
		snapshot->entities[i] =
			cl.parseEntities[ ( clSnap->parseEntitiesNum + i ) & (MAX_PARSE_ENTITIES-1) ];
	}

	// FIXME: configstring changes and server commands!!!

	return qtrue;
}


/*
=====================
CL_SetUserCmdValue
=====================
*/
static void CL_SetUserCmdValue( int userCmdValue, float sensitivityScale ) {
	cl.cgameUserCmdValue = userCmdValue;
	cl.cgameSensitivity = sensitivityScale;
}


/*
=====================
CL_AddCgameCommand
=====================
*/
static void CL_AddCgameCommand( const char *cmdName ) {
	Cmd_AddCommand( cmdName, NULL );
}


/*
=====================
CL_ConfigstringModified
=====================
*/
static void CL_ConfigstringModified( void ) {
	const char	*old, *s;
	int			i, index;
	const char	*dup;
	gameState_t	oldGs;
	int			len;

	index = atoi( Cmd_Argv(1) );
	if ( (unsigned) index >= MAX_CONFIGSTRINGS ) {
		Com_Error( ERR_DROP, "%s: bad configstring index %i", __func__, index );
	}
	// get everything after "cs <num>"
	s = Cmd_ArgsFrom(2);

	old = cl.gameState.stringData + cl.gameState.stringOffsets[ index ];
	if ( !strcmp( old, s ) ) {
		return;		// unchanged
	}

	// build the new gameState_t
	oldGs = cl.gameState;

	Com_Memset( &cl.gameState, 0, sizeof( cl.gameState ) );

	// leave the first 0 for uninitialized strings
	cl.gameState.dataCount = 1;

	for ( i = 0; i < MAX_CONFIGSTRINGS; i++ ) {
		if ( i == index ) {
			dup = s;
		} else {
			dup = oldGs.stringData + oldGs.stringOffsets[ i ];
		}
		if ( !dup[0] ) {
			continue;		// leave with the default empty string
		}

		len = strlen( dup );

		if ( len + 1 + cl.gameState.dataCount > MAX_GAMESTATE_CHARS ) {
			Com_Error( ERR_DROP, "%s: MAX_GAMESTATE_CHARS exceeded", __func__ );
		}

		// append it to the gameState string buffer
		cl.gameState.stringOffsets[ i ] = cl.gameState.dataCount;
		Com_Memcpy( cl.gameState.stringData + cl.gameState.dataCount, dup, len + 1 );
		cl.gameState.dataCount += len + 1;
	}

	if ( index == CS_SYSTEMINFO ) {
		// parse serverId and other cvars
		CL_SystemInfoChanged( qfalse );
	}
}


/*
===================
CL_GetServerCommand

Set up argc/argv for the given command
===================
*/
static qboolean CL_GetServerCommand( int serverCommandNumber ) {
	const char *s;
	const char *cmd;
	static char bigConfigString[BIG_INFO_STRING];
	int argc, index;

	// if we have irretrievably lost a reliable command, drop the connection
	if ( clc.serverCommandSequence - serverCommandNumber >= MAX_RELIABLE_COMMANDS ) {
		// when a demo record was started after the client got a whole bunch of
		// reliable commands then the client never got those first reliable commands
		if ( clc.demoplaying ) {
			Cmd_Clear();
			return qfalse;
		}
		SCR_LogNote( "WARN:CMDCYCLED",
			va( "reliable cmd cycled out: req=%d stored=%d gap=%d",
				serverCommandNumber, clc.serverCommandSequence,
				clc.serverCommandSequence - serverCommandNumber ) );
		SCR_LogDisconnect( "reliable cmd cycled out (ERR_DROP)" );
		Com_Error( ERR_DROP, "CL_GetServerCommand: a reliable command was cycled out" );
		return qfalse;
	}

	if ( clc.serverCommandSequence - serverCommandNumber < 0 ) {
		Com_Error( ERR_DROP, "CL_GetServerCommand: requested a command not received" );
		return qfalse;
	}

	index = serverCommandNumber & ( MAX_RELIABLE_COMMANDS - 1 );
	s = clc.serverCommands[ index ];
	clc.lastExecutedServerCommand = serverCommandNumber;

	Com_DPrintf( "serverCommand: %i : %s\n", serverCommandNumber, s );

	if ( clc.serverCommandsIgnore[ index ] ) {
		Cmd_Clear();
		return qfalse;
	}

rescan:
	Cmd_TokenizeString( s );
	cmd = Cmd_Argv(0);
	argc = Cmd_Argc();

	if ( !strcmp( cmd, "disconnect" ) ) {
		// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=552
		// allow server to indicate why they were disconnected
		const char *reason = ( argc >= 2 ) ? Cmd_Argv( 1 ) : "(no reason)";
		/* Log the in-band disconnect before acting on it so we always have
		   a record of what the server sent even if the reason string helps
		   trace bad-faith server behaviour. */
		SCR_LogNote( "SVCMD:DISCONNECT",
			va( "in-band reliable disconnect: reason=\"%s\""
				"  cmdSeq=%d  relSeq=%d  relAck=%d",
				reason,
				clc.serverCommandSequence,
				clc.reliableSequence, clc.reliableAcknowledge ) );
		SCR_LogDisconnect( reason );
		if ( argc >= 2 )
			Com_Error( ERR_SERVERDISCONNECT, "Server disconnected - %s", reason );
		else
			Com_Error( ERR_SERVERDISCONNECT, "Server disconnected" );
	}

	if ( !strcmp( cmd, "bcs0" ) ) {
		Com_sprintf( bigConfigString, BIG_INFO_STRING, "cs %s \"%s", Cmd_Argv(1), Cmd_Argv(2) );
		return qfalse;
	}

	if ( !strcmp( cmd, "bcs1" ) ) {
		s = Cmd_Argv(2);
		if( strlen(bigConfigString) + strlen(s) >= BIG_INFO_STRING ) {
			Com_Error( ERR_DROP, "bcs exceeded BIG_INFO_STRING" );
		}
		strcat( bigConfigString, s );
		return qfalse;
	}

	if ( !strcmp( cmd, "bcs2" ) ) {
		s = Cmd_Argv(2);
		if( strlen(bigConfigString) + strlen(s) + 1 >= BIG_INFO_STRING ) {
			Com_Error( ERR_DROP, "bcs exceeded BIG_INFO_STRING" );
		}
		strcat( bigConfigString, s );
		strcat( bigConfigString, "\"" );
		s = bigConfigString;
		goto rescan;
	}

	if ( !strcmp( cmd, "cs" ) ) {
		CL_ConfigstringModified();
		// reparse the string, because CL_ConfigstringModified may have done another Cmd_TokenizeString()
		Cmd_TokenizeString( s );
		return qtrue;
	}

	if ( !strcmp( cmd, "map_restart" ) ) {
		// clear notify lines and outgoing commands before passing
		// the restart to the cgame
		Con_ClearNotify();
		// reparse the string, because Con_ClearNotify() may have done another Cmd_TokenizeString()
		Cmd_TokenizeString( s );
		Com_Memset( cl.cmds, 0, sizeof( cl.cmds ) );
		cls.lastVidRestart = Sys_Milliseconds(); // hack for OSP mod
		return qtrue;
	}

	// the clientLevelShot command is used during development
	// to generate 128*128 screenshots from the intermission
	// point of levels for the menu system to use
	// we pass it along to the cgame to make appropriate adjustments,
	// but we also clear the console and notify lines here
	if ( !strcmp( cmd, "clientLevelShot" ) ) {
		// don't do it if we aren't running the server locally,
		// otherwise malicious remote servers could overwrite
		// the existing thumbnails
		if ( !com_sv_running->integer ) {
			return qfalse;
		}
		// close the console
		Con_Close();
		// take a special screenshot next frame
		Cbuf_AddText( "wait ; wait ; wait ; wait ; screenshot levelshot\n" );
		return qtrue;
	}

#ifdef USE_FTWGL
	if ( !strcmp( cmd, "clientScreenshot" ) ) {
		if ( Cmd_Argc() == 1 ) {
			Cbuf_AddText( "wait ; wait ; wait ; wait ; screenshot silent\n" );
		} else if ( Cmd_Argc() == 2 ) {
			Cbuf_AddText( va( "wait ; wait ; wait ; wait ; screenshot silent %s\n", Cmd_Argv(1) ) );
		}
		return qfalse;
	}
#endif

	// we may want to put a "connect to other server" command here

	// cgame can now act on the command
	return qtrue;
}


/*
====================
CL_CM_LoadMap

Just adds default parameters that cgame doesn't need to know about
====================
*/
static void CL_CM_LoadMap( const char *mapname ) {
	int		checksum;

	CM_LoadMap( mapname, qtrue, &checksum );
}


/*
====================
CL_ShutdonwCGame

====================
*/
void CL_ShutdownCGame( void ) {

	Key_SetCatcher( Key_GetCatcher( ) & ~KEYCATCH_CGAME );
	cls.cgameStarted = qfalse;

	if ( !cgvm ) {
		return;
	}

	re.VertexLighting( qfalse );

	VM_Call( cgvm, 0, CG_SHUTDOWN );
	VM_Free( cgvm );
	cgvm = NULL;
	FS_VM_CloseFiles( H_CGAME );
}


static int FloatAsInt( float f ) {
	floatint_t fi;
	fi.f = f;
	return fi.i;
}


static void *VM_ArgPtr( intptr_t intValue ) {

	if ( !intValue || cgvm == NULL )
	  return NULL;

	if ( cgvm->entryPoint )
		return (void *)(intValue);
	else
		return (void *)(cgvm->dataBase + (intValue & cgvm->dataMask));
}


static qboolean CL_GetValue( char* value, int valueSize, const char* key ) {

	if ( !Q_stricmp( key, "trap_R_AddRefEntityToScene2" ) ) {
		Com_sprintf( value, valueSize, "%i", CG_R_ADDREFENTITYTOSCENE2 );
		return qtrue;
	}

	if ( !Q_stricmp( key, "trap_R_ForceFixedDLights" ) ) {
		Com_sprintf( value, valueSize, "%i", CG_R_FORCEFIXEDDLIGHTS );
		return qtrue;
	}

	if ( !Q_stricmp( key, "trap_R_AddLinearLightToScene_Q3E" ) && re.AddLinearLightToScene ) {
		Com_sprintf( value, valueSize, "%i", CG_R_ADDLINEARLIGHTTOSCENE );
		return qtrue;
	}

	if ( !Q_stricmp( key, "trap_IsRecordingDemo" ) ) {
		Com_sprintf( value, valueSize, "%i", CG_IS_RECORDING_DEMO );
		return qtrue;
	}

	if ( !Q_stricmp( key, "trap_Cvar_SetDescription_Q3E" ) ) {
		Com_sprintf( value, valueSize, "%i", CG_CVAR_SETDESCRIPTION );
		return qtrue;
	}

	return qfalse;
}


static void CL_ForceFixedDlights( void ) {
	cvar_t *cv;

	cv = Cvar_Get( "r_dlightMode", "1", 0 );
	if ( cv ) {
		Cvar_CheckRange( cv, "1", "2", CV_INTEGER );
	}
}


/*
====================
CL_CgameSystemCalls

The cgame module is making a system call
====================
*/
static intptr_t CL_CgameSystemCalls( intptr_t *args ) {
	switch( args[0] ) {
	case CG_PRINT:
		Com_Printf( "%s", (const char*)VMA(1) );
		return 0;
	case CG_ERROR:
		Com_Error( ERR_DROP, "%s", (const char*)VMA(1) );
		return 0;
	case CG_MILLISECONDS:
		return Sys_Milliseconds();
	case CG_CVAR_REGISTER:
		Cvar_Register( VMA(1), VMA(2), VMA(3), args[4], cgvm->privateFlag );
		return 0;
	case CG_CVAR_UPDATE:
		Cvar_Update( VMA(1), cgvm->privateFlag );
		return 0;
	case CG_CVAR_SET:
		// Prevent QVM from overriding cvars that could be weaponised by a
		// malicious or buggy server to sabotage the client connection or
		// degrade gameplay.  Every interception is logged so we can see
		// what the vanilla QVM is attempting.
		//
		// Unconditionally intercepted (silently ignored):
		//   snaps           — server controls delivery rate anyway; wrong value
		//                     here breaks our snapshot-interval logic
		//   cg_smoothClients — purely client-side rendering preference
		//   net_qport       — changing port mid-session breaks the netchan
		//   rate=0          — zero rate causes server to stop sending snapshots
		//   net_dropsim     — simulates packet drops on outgoing traffic;
		//                     a server setting this would cause us to shed our
		//                     own packets, making us appear to time out
		//   cl_packetdelay  — adds artificial send latency; could push ping
		//                     above sv_maxPing and trigger a server ping-kick
		//   cl_packetdup    — controls redundant command retransmission;
		//                     a server zeroing this removes loss mitigation,
		//                     while a server setting it high floods the server
		//   cl_timeout      — lowering this lets the server trigger our own
		//                     timeout watchdog at will
		//   in_mouse        — disabling mouse input is pure sabotage
		//   cl_noOOBDisconnect — already CVAR_PROTECTED, but explicit here
		//                        for clarity: a server must not be able to
		//                        disarm our OOB-disconnect guard
		//
		// Also blocked by Cvar_SetSafe for CVAR_PROTECTED/PRIVATE cvars
		// (e.g. com_maxfps, cl_timeNudge) — those are logged as QVM:SET_BLOCKED.
		if ( Q_stricmp( (const char *)VMA(1), "snaps"            ) == 0
			|| Q_stricmp( (const char *)VMA(1), "cg_smoothClients" ) == 0
			|| Q_stricmp( (const char *)VMA(1), "net_qport"        ) == 0
			|| Q_stricmp( (const char *)VMA(1), "net_dropsim"      ) == 0
			|| Q_stricmp( (const char *)VMA(1), "cl_packetdelay"   ) == 0
			|| Q_stricmp( (const char *)VMA(1), "cl_packetdup"     ) == 0
			|| Q_stricmp( (const char *)VMA(1), "cl_timeout"       ) == 0
			|| Q_stricmp( (const char *)VMA(1), "in_mouse"         ) == 0
			|| Q_stricmp( (const char *)VMA(1), "cl_noOOBDisconnect" ) == 0
			|| ( Q_stricmp( (const char *)VMA(1), "rate" ) == 0
				&& atoi( (const char *)VMA(2) ) <= 0 ) ) {
			SCR_LogNote( "QVM:SET_INTERCEPTED",
				va( "\"%s\" = \"%s\" (silently ignored)",
					(const char *)VMA(1), (const char *)VMA(2) ) );
		} else {
			// Check whether Cvar_SetSafe will block it (CVAR_PROTECTED/PRIVATE)
			// before calling it, so we can log the attempt regardless.
			{
				unsigned cvFlags = Cvar_Flags( (const char *)VMA(1) );
				if ( cvFlags != CVAR_NONEXISTENT
					&& ( cvFlags & ( CVAR_PROTECTED | CVAR_PRIVATE ) ) ) {
					SCR_LogNote( "QVM:SET_BLOCKED",
						va( "\"%s\" = \"%s\" (protected)",
							(const char *)VMA(1), (const char *)VMA(2) ) );
				}
			}
			Cvar_SetSafe( VMA(1), VMA(2) );
		}
		return 0;
	case CG_CVAR_VARIABLESTRINGBUFFER:
		VM_CHECKBOUNDS( cgvm, args[2], args[3] );
		Cvar_VariableStringBufferSafe( VMA(1), VMA(2), args[3], CVAR_PRIVATE );
		return 0;
	case CG_ARGC:
		return Cmd_Argc();
	case CG_ARGV:
		VM_CHECKBOUNDS( cgvm, args[2], args[3] );
		Cmd_ArgvBuffer( args[1], VMA(2), args[3] );
		return 0;
	case CG_ARGS:
		VM_CHECKBOUNDS( cgvm, args[1], args[2] );
		Cmd_ArgsBuffer( VMA(1), args[2] );
		return 0;

	case CG_FS_FOPENFILE:
		return FS_VM_OpenFile( VMA(1), VMA(2), args[3], H_CGAME );
	case CG_FS_READ:
		VM_CHECKBOUNDS( cgvm, args[1], args[2] );
		FS_VM_ReadFile( VMA(1), args[2], args[3], H_CGAME );
		return 0;
	case CG_FS_WRITE:
		VM_CHECKBOUNDS( cgvm, args[1], args[2] );
		FS_VM_WriteFile( VMA(1), args[2], args[3], H_CGAME );
		return 0;
	case CG_FS_FCLOSEFILE:
		FS_VM_CloseFile( args[1], H_CGAME );
		return 0;
	case CG_FS_SEEK:
		return FS_VM_SeekFile( args[1], args[2], args[3], H_CGAME );

	case CG_SENDCONSOLECOMMAND: {
		const char *cmd = VMA(1);

		/* Log every console command the QVM injects so we have a full trace.
		   At level 1 flag any command that could force a disconnect or quit;
		   at level 2 log all of them for completeness. */
		{
			char   tok[64];
			size_t i = 0;
			/* Extract the first token (command name) from the string. */
			while ( i < sizeof(tok) - 1 && cmd[i] && cmd[i] != ' '
				&& cmd[i] != '\t' && cmd[i] != '\n' && cmd[i] != ';' ) {
				tok[i] = cmd[i]; i++;
			}
			tok[i] = '\0';

			if ( Q_stricmp( tok, "disconnect" ) == 0
				|| Q_stricmp( tok, "quit"       ) == 0
				|| Q_stricmp( tok, "exit"       ) == 0
				|| Q_stricmp( tok, "exec"       ) == 0 ) {
				/* Dangerous: log at level 1 so it always appears in the log.
				   Block execution — the server should not be able to force
				   these through the cgame console-command channel. */
				SCR_LogNote( "QVM:CONSOLECMD_BLOCKED",
					va( "dangerous cmd blocked: \"%s\"", cmd ) );
				Com_Printf( S_COLOR_YELLOW
					"WARNING: server tried to inject dangerous console command"
					" via cgame — blocked: \"%s\"\n", cmd );
				return 0;
			}

			SCR_LogNote( "QVM:CONSOLECMD", va( "\"%s\"", cmd ) );
		}

		Cbuf_NestedAdd( cmd );
		return 0;
	}
	case CG_ADDCOMMAND:
		CL_AddCgameCommand( VMA(1) );
		return 0;
	case CG_REMOVECOMMAND:
		Cmd_RemoveCommandSafe( VMA(1) );
		return 0;
	case CG_SENDCLIENTCOMMAND:
		CL_AddReliableCommand( VMA(1), qfalse );
		return 0;
	case CG_UPDATESCREEN:
		// this is used during lengthy level loading, so pump message loop
		// Com_EventLoop();	// FIXME: if a server restarts here, BAD THINGS HAPPEN!
		// We can't call Com_EventLoop here, a restart will crash and this _does_ happen
		// if there is a map change while we are downloading at pk3.
		// ZOID
		SCR_UpdateScreen();
		return 0;
	case CG_CM_LOADMAP:
		CL_CM_LoadMap( VMA(1) );
		return 0;
	case CG_CM_NUMINLINEMODELS:
		return CM_NumInlineModels();
	case CG_CM_INLINEMODEL:
		return CM_InlineModel( args[1] );
	case CG_CM_TEMPBOXMODEL:
		return CM_TempBoxModel( VMA(1), VMA(2), /*int capsule*/ qfalse );
	case CG_CM_TEMPCAPSULEMODEL:
		return CM_TempBoxModel( VMA(1), VMA(2), /*int capsule*/ qtrue );
	case CG_CM_POINTCONTENTS:
		return CM_PointContents( VMA(1), args[2] );
	case CG_CM_TRANSFORMEDPOINTCONTENTS:
		return CM_TransformedPointContents( VMA(1), args[2], VMA(3), VMA(4) );
	case CG_CM_BOXTRACE:
		CM_BoxTrace( VMA(1), VMA(2), VMA(3), VMA(4), VMA(5), args[6], args[7], /*int capsule*/ qfalse );
		return 0;
	case CG_CM_CAPSULETRACE:
		CM_BoxTrace( VMA(1), VMA(2), VMA(3), VMA(4), VMA(5), args[6], args[7], /*int capsule*/ qtrue );
		return 0;
	case CG_CM_TRANSFORMEDBOXTRACE:
		CM_TransformedBoxTrace( VMA(1), VMA(2), VMA(3), VMA(4), VMA(5), args[6], args[7], VMA(8), VMA(9), /*int capsule*/ qfalse );
		return 0;
	case CG_CM_TRANSFORMEDCAPSULETRACE:
		CM_TransformedBoxTrace( VMA(1), VMA(2), VMA(3), VMA(4), VMA(5), args[6], args[7], VMA(8), VMA(9), /*int capsule*/ qtrue );
		return 0;
	case CG_CM_MARKFRAGMENTS:
		return re.MarkFragments( args[1], VMA(2), VMA(3), args[4], VMA(5), args[6], VMA(7) );
	case CG_S_STARTSOUND:
		S_StartSound( VMA(1), args[2], args[3], args[4] );
		return 0;
	case CG_S_STARTLOCALSOUND:
		S_StartLocalSound( args[1], args[2] );
		return 0;
	case CG_S_CLEARLOOPINGSOUNDS:
		S_ClearLoopingSounds(args[1]);
		return 0;
	case CG_S_ADDLOOPINGSOUND:
		S_AddLoopingSound( args[1], VMA(2), VMA(3), args[4] );
		return 0;
	case CG_S_ADDREALLOOPINGSOUND:
		S_AddRealLoopingSound( args[1], VMA(2), VMA(3), args[4] );
		return 0;
	case CG_S_STOPLOOPINGSOUND:
		S_StopLoopingSound( args[1] );
		return 0;
	case CG_S_UPDATEENTITYPOSITION:
		S_UpdateEntityPosition( args[1], VMA(2) );
		return 0;
	case CG_S_RESPATIALIZE:
		S_Respatialize( args[1], VMA(2), VMA(3), args[4] );
		return 0;
	case CG_S_REGISTERSOUND:
		return S_RegisterSound( VMA(1), args[2] );
	case CG_S_STARTBACKGROUNDTRACK:
		S_StartBackgroundTrack( VMA(1), VMA(2) );
		return 0;
	case CG_R_LOADWORLDMAP:
		re.LoadWorld( VMA(1) );
		return 0;
	case CG_R_REGISTERMODEL:
		return re.RegisterModel( VMA(1) );
	case CG_R_REGISTERSKIN:
		return re.RegisterSkin( VMA(1) );
	case CG_R_REGISTERSHADER:
		return re.RegisterShader( VMA(1) );
	case CG_R_REGISTERSHADERNOMIP:
		return re.RegisterShaderNoMip( VMA(1) );
	case CG_R_REGISTERFONT:
		re.RegisterFont( VMA(1), args[2], VMA(3));
		return 0;
	case CG_R_CLEARSCENE:
		re.ClearScene();
		return 0;
	case CG_R_ADDREFENTITYTOSCENE:
		re.AddRefEntityToScene( VMA(1), qfalse );
		return 0;
	case CG_R_ADDPOLYTOSCENE:
		re.AddPolyToScene( args[1], args[2], VMA(3), 1 );
		return 0;
	case CG_R_ADDPOLYSTOSCENE:
		re.AddPolyToScene( args[1], args[2], VMA(3), args[4] );
		return 0;
	case CG_R_LIGHTFORPOINT:
		return re.LightForPoint( VMA(1), VMA(2), VMA(3), VMA(4) );
	case CG_R_ADDLIGHTTOSCENE:
		re.AddLightToScene( VMA(1), VMF(2), VMF(3), VMF(4), VMF(5) );
		return 0;
	case CG_R_ADDADDITIVELIGHTTOSCENE:
		re.AddAdditiveLightToScene( VMA(1), VMF(2), VMF(3), VMF(4), VMF(5) );
		return 0;
	case CG_R_RENDERSCENE:
		re.RenderScene( VMA(1) );
		return 0;
	case CG_R_SETCOLOR:
		re.SetColor( VMA(1) );
		return 0;
	case CG_R_DRAWSTRETCHPIC:
		re.DrawStretchPic( VMF(1), VMF(2), VMF(3), VMF(4), VMF(5), VMF(6), VMF(7), VMF(8), args[9] );
		return 0;
	case CG_R_MODELBOUNDS:
		re.ModelBounds( args[1], VMA(2), VMA(3) );
		return 0;
	case CG_R_LERPTAG:
		return re.LerpTag( VMA(1), args[2], args[3], args[4], VMF(5), VMA(6) );
	case CG_GETGLCONFIG:
		VM_CHECKBOUNDS( cgvm, args[1], sizeof( glconfig_t ) );
		CL_GetGlconfig( VMA(1) );
		return 0;
	case CG_GETGAMESTATE:
		VM_CHECKBOUNDS( cgvm, args[1], sizeof( gameState_t ) );
		CL_GetGameState( VMA(1) );
		return 0;
	case CG_GETCURRENTSNAPSHOTNUMBER:
		CL_GetCurrentSnapshotNumber( VMA(1), VMA(2) );
		return 0;
	case CG_GETSNAPSHOT:
		return CL_GetSnapshot( args[1], VMA(2) );
	case CG_GETSERVERCOMMAND:
		return CL_GetServerCommand( args[1] );
	case CG_GETCURRENTCMDNUMBER:
		return CL_GetCurrentCmdNumber();
	case CG_GETUSERCMD:
		return CL_GetUserCmd( args[1], VMA(2) );
	case CG_SETUSERCMDVALUE:
		CL_SetUserCmdValue( args[1], VMF(2) );
		return 0;
	case CG_MEMORY_REMAINING:
		return Hunk_MemoryRemaining();
	case CG_KEY_ISDOWN:
		return Key_IsDown( args[1] );
	case CG_KEY_GETCATCHER:
		return Key_GetCatcher();
	case CG_KEY_SETCATCHER:
		// Don't allow the cgame module to close the console
		Key_SetCatcher( args[1] | ( Key_GetCatcher( ) & KEYCATCH_CONSOLE ) );
		return 0;
	case CG_KEY_GETKEY:
		return Key_GetKey( VMA(1) );

	// shared syscalls
	case TRAP_MEMSET:
		VM_CHECKBOUNDS( cgvm, args[1], args[3] );
		Com_Memset( VMA(1), args[2], args[3] );
		return args[1];
	case TRAP_MEMCPY:
		VM_CHECKBOUNDS2( cgvm, args[1], args[2], args[3] );
		Com_Memcpy( VMA(1), VMA(2), args[3] );
		return args[1];
	case TRAP_STRNCPY:
		VM_CHECKBOUNDS( cgvm, args[1], args[3] );
		Q_strncpy( VMA(1), VMA(2), args[3] );
		return args[1];
	case TRAP_SIN:
		return FloatAsInt( sin( VMF(1) ) );
	case TRAP_COS:
		return FloatAsInt( cos( VMF(1) ) );
	case TRAP_ATAN2:
		return FloatAsInt( atan2( VMF(1), VMF(2) ) );
	case TRAP_SQRT:
		return FloatAsInt( sqrt( VMF(1) ) );

	case CG_FLOOR:
		return FloatAsInt( floor( VMF(1) ) );
	case CG_CEIL:
		return FloatAsInt( ceil( VMF(1) ) );
	case CG_TESTPRINTINT:
		return sprintf( VMA(1), "%i", (int)args[2] );
	case CG_TESTPRINTFLOAT:
		return sprintf( VMA(1), "%f", VMF(2) );
	case CG_ACOS:
		return FloatAsInt( Q_acos( VMF(1) ) );

	case CG_PC_ADD_GLOBAL_DEFINE:
		return botlib_export->PC_AddGlobalDefine( VMA(1) );
	case CG_PC_LOAD_SOURCE:
		return botlib_export->PC_LoadSourceHandle( VMA(1) );
	case CG_PC_FREE_SOURCE:
		return botlib_export->PC_FreeSourceHandle( args[1] );
	case CG_PC_READ_TOKEN:
		return botlib_export->PC_ReadTokenHandle( args[1], VMA(2) );
	case CG_PC_SOURCE_FILE_AND_LINE:
		return botlib_export->PC_SourceFileAndLine( args[1], VMA(2), VMA(3) );

	case CG_S_STOPBACKGROUNDTRACK:
		S_StopBackgroundTrack();
		return 0;

	case CG_REAL_TIME:
		return Com_RealTime( VMA(1) );
	case CG_SNAPVECTOR:
		Sys_SnapVector( VMA(1) );
		return 0;

	case CG_CIN_PLAYCINEMATIC:
		return CIN_PlayCinematic(VMA(1), args[2], args[3], args[4], args[5], args[6]);

	case CG_CIN_STOPCINEMATIC:
		return CIN_StopCinematic(args[1]);

	case CG_CIN_RUNCINEMATIC:
		return CIN_RunCinematic(args[1]);

	case CG_CIN_DRAWCINEMATIC:
		CIN_DrawCinematic(args[1]);
		return 0;

	case CG_CIN_SETEXTENTS:
		CIN_SetExtents(args[1], args[2], args[3], args[4], args[5]);
		return 0;

	case CG_R_REMAP_SHADER:
		re.RemapShader( VMA(1), VMA(2), VMA(3) );
		return 0;

/*
	case CG_LOADCAMERA:
		return loadCamera(VMA(1));

	case CG_STARTCAMERA:
		startCamera(args[1]);
		return 0;

	case CG_GETCAMERAINFO:
		return getCameraInfo(args[1], VMA(2), VMA(3));
*/
	case CG_GET_ENTITY_TOKEN:
		VM_CHECKBOUNDS( cgvm, args[1], args[2] );
		return re.GetEntityToken( VMA(1), args[2] );

	case CG_R_INPVS:
		return re.inPVS( VMA(1), VMA(2) );

	// engine extensions
	case CG_R_ADDREFENTITYTOSCENE2:
		re.AddRefEntityToScene( VMA(1), qtrue );
		return 0;

	case CG_R_ADDLINEARLIGHTTOSCENE:
		re.AddLinearLightToScene( VMA(1), VMA(2), VMF(3), VMF(4), VMF(5), VMF(6) );
		return 0;

	case CG_R_FORCEFIXEDDLIGHTS:
		CL_ForceFixedDlights();
		return 0;

	case CG_IS_RECORDING_DEMO:
		return clc.demorecording;

	case CG_CVAR_SETDESCRIPTION:
		Cvar_SetDescription2( (const char*)VMA(1), (const char*)VMA(2) );
		return 0;

	case CG_TRAP_GETVALUE:
		VM_CHECKBOUNDS( cgvm, args[1], args[2] );
		return CL_GetValue( VMA(1), args[2], VMA(3) );

	default:
		Com_Error( ERR_DROP, "Bad cgame system trap: %ld", (long int) args[0] );
	}
	return 0;
}


/*
====================
CL_DllSyscall
====================
*/
static intptr_t QDECL CL_DllSyscall( intptr_t arg, ... ) {
#if !id386 || defined __clang__
	intptr_t	args[10]; // max.count for cgame
	va_list	ap;
	int i;

	args[0] = arg;
	va_start( ap, arg );
	for (i = 1; i < ARRAY_LEN( args ); i++ )
		args[ i ] = va_arg( ap, intptr_t );
	va_end( ap );

	return CL_CgameSystemCalls( args );
#else
	return CL_CgameSystemCalls( &arg );
#endif
}


/*
====================
CL_InitCGame

Should only be called by CL_StartHunkUsers
====================
*/
void CL_InitCGame( void ) {
	const char			*info;
	const char			*mapname;
	int					t1, t2;
	vmInterpret_t		interpret;

	Cbuf_NestedReset();

	t1 = Sys_Milliseconds();

	// put away the console
	Con_Close();

	// find the current mapname
	info = cl.gameState.stringData + cl.gameState.stringOffsets[ CS_SERVERINFO ];
	mapname = Info_ValueForKey( info, "mapname" );
	Com_sprintf( cl.mapname, sizeof( cl.mapname ), "maps/%s.bsp", mapname );

	// allow vertex lighting for in-game elements
	re.VertexLighting( qtrue );

	// load the dll or bytecode
	interpret = Cvar_VariableIntegerValue( "vm_cgame" );
	if ( cl_connectedToPureServer )
	{
		// if sv_pure is set we only allow qvms to be loaded
		if ( interpret != VMI_COMPILED && interpret != VMI_BYTECODE )
			interpret = VMI_COMPILED;
	}

	// Register UrT 4.3 cgame patch cvars before loading the VM so
	// VM_ReplaceInstructions sees the correct default values.
	// CVAR_PROTECTED prevents the QVM from disabling its own patches.
	//
	// Bitmask (both cvars share the same bit layout):
	//   bit 0 = Patch 2: frameInterpolation clamp fix (safe on any server)
	//   bit 1 = Patch 3: nextSnap null-pointer crash fix (safe on any server)
	//   bit 2 = Patch 1: TR_INTERPOLATE velocity extrapolation / TR_LINEAR
	//           Requires server-side trTime anchor — only safe on our custom
	//           server.  Auto-suppressed on vanilla servers via
	//           cl_qvmPatchVanilla + cl_urt43serverIsVanilla.
	//
	// cl_urt43cgPatches  — used when connected to our custom server (default 7)
	// cl_qvmPatchVanilla — used when connected to a vanilla server   (default 3)
	Cvar_Get( "cl_urt43cgPatches", "7", CVAR_ARCHIVE | CVAR_PROTECTED );
	Cvar_SetDescription( "cl_urt43cgPatches",
		"QVM patch bitmask applied when connected to a custom (Quake3e-urt) server. "
		"Bit 0 = Patch2 (frameInterpolation clamp), "
		"bit 1 = Patch3 (null-snapshot crash fix), "
		"bit 2 = Patch1 (TR_LINEAR velocity extrapolation — requires server-side trTime anchor). "
		"Default 7 (all patches). "
		"On vanilla servers cl_qvmPatchVanilla is used instead." );
	Cvar_Get( "cl_qvmPatchVanilla", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( "cl_qvmPatchVanilla",
		"QVM patch bitmask applied when connected to a vanilla UrT server "
		"(server lacks sv_snapshotFps — no server-side trTime anchor for TR_LINEAR). "
		"Bit 0 = Patch2 (frameInterpolation clamp), "
		"bit 1 = Patch3 (null-snapshot crash fix), "
		"bit 2 = Patch1 (TR_LINEAR — unsafe on vanilla servers, leave unset). "
		"Default 0 (no patches on vanilla servers — safe baseline for A/B testing). "
		"On custom servers cl_urt43cgPatches is used instead." );

	cgvm = VM_Create( VM_CGAME, CL_CgameSystemCalls, CL_DllSyscall, interpret );
	if ( !cgvm ) {
		Com_Error( ERR_DROP, "VM_Create on cgame failed" );
	}
	cls.state = CA_LOADING;

	// init for this gamestate
	// use the lastExecutedServerCommand instead of the serverCommandSequence
	// otherwise server commands sent just before a gamestate are dropped
	VM_Call( cgvm, 3, CG_INIT, clc.serverMessageSequence, clc.lastExecutedServerCommand, clc.clientNum );

	// reset any CVAR_CHEAT cvars registered by cgame
	if ( !clc.demoplaying && !cl_connectedToCheatServer )
		Cvar_SetCheatState();

	// we will send a usercmd this frame, which
	// will cause the server to send us the first snapshot
	cls.state = CA_PRIMED;

	t2 = Sys_Milliseconds();

	Com_Printf( "CL_InitCGame: %5.2f seconds\n", (t2-t1)/1000.0 );

	// have the renderer touch all its images, so they are present
	// on the card even if the driver does deferred loading
	re.EndRegistration();

	// make sure everything is paged in
	if (!Sys_LowPhysicalMemory()) {
		Com_TouchMemory();
	}

	// clear anything that got printed
	Con_ClearNotify ();

	// do not allow vid_restart for first time
	cls.lastVidRestart = Sys_Milliseconds();
}


/*
====================
CL_GameCommand

See if the current console command is claimed by the cgame
====================
*/

qboolean CL_GameCommand( void ) {
	qboolean bRes;

	if ( !cgvm ) {
		return qfalse;
	}

	bRes = (qboolean)VM_Call( cgvm, 0, CG_CONSOLE_COMMAND );

	Cbuf_NestedReset();

	return bRes;
}


/*
=====================
CL_CGameRendering
=====================
*/
void CL_CGameRendering( stereoFrame_t stereo ) {
	VM_Call( cgvm, 3, CG_DRAW_ACTIVE_FRAME, cl.serverTime, stereo, clc.demoplaying );
#ifdef DEBUG
	VM_Debug( 0 );
#endif
}


/*
=================
CL_AdjustTimeDelta

Adjust the clients view of server time.

We attempt to have cl.serverTime exactly equal the server's view
of time plus the timeNudge, but with variable latencies over
the internet it will often need to drift a bit to match conditions.

Our ideal time would be to have the adjusted time approach, but not pass,
the very latest snapshot.

Adjustments are only made when a new snapshot arrives with a rational
latency, which keeps the adjustment process framerate independent and
prevents massive overadjustment during times of significant packet loss
or bursted delayed packets.
=================
*/

// Sub-millisecond fractional accumulator for the slow drift path.
// 1 unit = 1/4 ms; each slow-path step adds +/-2 units (= +/-0.5 ms);
// commit threshold is +/-4 units (= +/-1 ms).
// Eliminates the +/-1ms oscillation that causes ping jitter.
static int slowFrac = 0;

static void CL_AdjustTimeDelta( void ) {
	int		newDelta;
	int		deltaDelta;
	int		resetTime;
	int		fastAdjust;

	// Scale thresholds proportionally to the measured snapshot interval so the
	// system reacts at the same number-of-snapshots equivalent across all rates.
	// At 20Hz (snapshotMsec=50): resetTime=500, fastAdjust=100 — same as vanilla.
	// At 60Hz (snapshotMsec=16): resetTime=500 (floor), fastAdjust=50 (floor).
	// Disabled on vanilla servers and when the server forbids adaptive timing.
	if ( cl_adaptiveTiming->integer && !cl.serverForbidsAdaptiveTiming ) {
		resetTime  = cl.snapshotMsec * 10;
		fastAdjust = cl.snapshotMsec * 2;
		if ( resetTime  < 500 ) resetTime  = 500;
		if ( fastAdjust <  50 ) fastAdjust =  50;
	} else {
		resetTime  = 500;
		fastAdjust = 100;
	}

	cl.newSnapshots = qfalse;

	// the delta never drifts when replaying a demo
	if ( clc.demoplaying ) {
		return;
	}

	newDelta = cl.snap.serverTime - cls.realtime;
	deltaDelta = abs( newDelta - cl.serverTimeDelta );

	if ( deltaDelta > resetTime ) {
		cl.serverTimeDelta = newDelta;
		cl.oldServerTime = cl.snap.serverTime;	// FIXME: is this a problem for cgame?
		cl.serverTime = cl.snap.serverTime;
		slowFrac = 0;
		if ( cl_showTimeDelta->integer ) {
			Com_Printf( "<RESET> " );
		}
		SCR_NetMonitorAddResetAdjust();
		SCR_LogTimingEvent( "RESET", cl.serverTimeDelta, deltaDelta );
	} else if ( deltaDelta > fastAdjust ) {
		// fast adjust, cut the difference in half
		if ( cl_showTimeDelta->integer ) {
			Com_Printf( "<FAST> " );
		}
		cl.serverTimeDelta = ( cl.serverTimeDelta + newDelta ) >> 1;
		slowFrac = 0;
		SCR_NetMonitorAddFastAdjust();
		SCR_LogTimingEvent( "FAST ", cl.serverTimeDelta, deltaDelta );
	} else {
		// if any of the frames between this and the previous snapshot
		// had to be extrapolated, nudge our sense of time back a little
		if ( com_timescale->value == 0 || com_timescale->value == 1 ) {
			if ( cl.serverForbidsAdaptiveTiming ) {
				// vanilla Q3: direct ms adjustment, no fractional accumulator
				if ( cl.extrapolatedSnapshot ) {
					cl.extrapolatedSnapshot = qfalse;
					cl.serverTimeDelta -= 2;
					SCR_NetMonitorAddSlowAdjust( -2 );
				} else {
					cl.serverTimeDelta++;
					SCR_NetMonitorAddSlowAdjust( +1 );
				}
			} else {
				// adaptive timing: slow drift via fractional accumulator
				if ( cl.extrapolatedSnapshot ) {
					cl.extrapolatedSnapshot = qfalse;
					// At high fps (snapshotMsec < 30, e.g. 60Hz) the backward step equals
					// the forward step (-2 / +2), so slowFrac oscillates in [-2,+2] without
					// ever reaching the +/-4 commit threshold.  serverTimeDelta stays rock-stable
					// (no +/-1ms ping jitter) at the cost of a ~50% detection-zone entry rate.
					// This appears in the netgraph as Ext ~ snapsHz/2 (e.g. ~30/s at 60Hz)
					// and is EXPECTED BEHAVIOR, not actual client-side extrapolation.  The
					// safety cap (Clp) prevents cl.serverTime from passing the snapshot.
					// At low fps (snapshotMsec >= 30, e.g. 20Hz): -4 gives a 33% rate.
					slowFrac -= ( cl.snapshotMsec < 30 ) ? 2 : 4;
				} else {
					slowFrac += 2;
				}
				// Commit a whole-ms adjustment once the accumulator reaches +/-1 ms.
				// Mode 2 (proportional): scale commit to 25% of remaining error
				// for faster convergence on mid-range disturbances.
				if ( slowFrac >= 4 ) {
					int step = 1;
					if ( cl_adaptiveTiming->integer >= 2 && !cl.serverForbidsAdaptiveTiming && deltaDelta > cl.snapshotMsec ) {
						step = deltaDelta / 4;
						if ( step < 1 ) step = 1;
						if ( step > deltaDelta / 2 ) step = deltaDelta / 2;
					}
					cl.serverTimeDelta += step;
					slowFrac -= 4;
					SCR_NetMonitorAddSlowAdjust( +step );
				} else if ( slowFrac <= -4 ) {
					int step = 1;
					if ( cl_adaptiveTiming->integer >= 2 && !cl.serverForbidsAdaptiveTiming && deltaDelta > cl.snapshotMsec ) {
						step = deltaDelta / 4;
						if ( step < 1 ) step = 1;
						if ( step > deltaDelta / 2 ) step = deltaDelta / 2;
					}
					cl.serverTimeDelta -= step;
					slowFrac += 4;
					SCR_NetMonitorAddSlowAdjust( -step );
				}
			}
		}
	}

	if ( cl_showTimeDelta->integer ) {
		Com_Printf( "%i ", cl.serverTimeDelta );
	}
}


/*
==================
CL_FirstSnapshot
==================
*/
static void CL_FirstSnapshot( void ) {
	// ignore snapshots that don't have entities
	if ( cl.snap.snapFlags & SNAPFLAG_NOT_ACTIVE ) {
		return;
	}
	cls.state = CA_ACTIVE;

	// clear old game so we will not switch back to old mod on disconnect
	CL_ResetOldGame();

	// set the timedelta so we are exactly on this first frame
	cl.serverTimeDelta = cl.snap.serverTime - cls.realtime;
	cl.oldServerTime = cl.snap.serverTime;
	slowFrac = 0;

	clc.timeDemoBaseTime = cl.snap.serverTime;

	// if this is the first frame of active play,
	// execute the contents of activeAction now
	// this is to allow scripting a timedemo to start right
	// after loading
	if ( cl_activeAction->string[0] ) {
		Cbuf_AddText( cl_activeAction->string );
		Cbuf_AddText( "\n" );
		Cvar_Set( "activeAction", "" );
	}

	Sys_BeginProfiling();
}


/*
==================
CL_AvgPing

Calculates Average Ping from snapshots in buffer. Used by AutoNudge.
==================
*/
static float CL_AvgPing( void ) {
	int ping[PACKET_BACKUP];
	int count = 0;
	int i, j, iTemp;
	float result;

	for ( i = 0; i < PACKET_BACKUP; i++ ) {
		if ( cl.snapshots[i].ping > 0 && cl.snapshots[i].ping < 999 ) {
			ping[count] = cl.snapshots[i].ping;
			count++;
		}
	}

	if ( count == 0 )
		return 0;

	// sort ping array
	for ( i = count - 1; i > 0; --i ) {
		for ( j = 0; j < i; ++j ) {
			if (ping[j] > ping[j + 1]) {
				iTemp = ping[j];
				ping[j] = ping[j + 1];
				ping[j + 1] = iTemp;
			}
		}
	}

	// use median average ping
	if ( (count % 2) == 0 )
		result = (ping[count / 2] + ping[(count / 2) - 1]) / 2.0f;
	else
		result = ping[count / 2];

	return result;
}


/*
==================
CL_TimeNudge

Returns either auto-nudge or cl_timeNudge value.
==================
*/
static int CL_TimeNudge( void ) {
	float autoNudge = cl_autoNudge->value;

	if ( autoNudge != 0.0f )
		return (int)((CL_AvgPing() * autoNudge) + 0.5f) * -1;
	else
		return cl_timeNudge->integer;
}


/*
==================
CL_SetCGameTime
==================
*/
void CL_SetCGameTime( void ) {
	qboolean demoFreezed;

	// getting a valid frame message ends the connection process
	if ( cls.state != CA_ACTIVE ) {
		if ( cls.state != CA_PRIMED ) {
			return;
		}
		if ( clc.demoplaying ) {
			// we shouldn't get the first snapshot on the same frame
			// as the gamestate, because it causes a bad time skip
			if ( !clc.firstDemoFrameSkipped ) {
				clc.firstDemoFrameSkipped = qtrue;
				return;
			}
			CL_ReadDemoMessage();
		}
		if ( cl.newSnapshots ) {
			cl.newSnapshots = qfalse;
			CL_FirstSnapshot();
		}
		if ( cls.state != CA_ACTIVE ) {
			return;
		}
	}

	// if we have gotten to this point, cl.snap is guaranteed to be valid
	if ( !cl.snap.valid ) {
		Com_Error( ERR_DROP, "CL_SetCGameTime: !cl.snap.valid" );
	}

	// allow pause in single player
	if ( sv_paused->integer && CL_CheckPaused() && com_sv_running->integer ) {
		// paused
		return;
	}

	if ( cl.snap.serverTime - cl.oldFrameServerTime < 0 ) {
		Com_Error( ERR_DROP, "cl.snap.serverTime < cl.oldFrameServerTime" );
	}
	cl.oldFrameServerTime = cl.snap.serverTime;

	// get our current view of time
	demoFreezed = clc.demoplaying && com_timescale->value == 0.0f;
	if ( demoFreezed ) {
		// \timescale 0 is used to lock a demo in place for single frame advances
		cl.serverTimeDelta -= cls.frametime;
	} else {
		// cl_timeNudge is a user adjustable cvar that allows more
		// or less latency to be added in the interest of better
		// smoothness or better responsiveness.
		cl.serverTime = cls.realtime + cl.serverTimeDelta - CL_TimeNudge();

		// guarantee that time will never flow backwards, even if
		// serverTimeDelta made an adjustment or cl_timeNudge was changed
		if ( cl.serverTime - cl.oldServerTime < 0 ) {
			cl.serverTime = cl.oldServerTime;
		}
		// Safety cap: keep cl.serverTime strictly below snap.serverTime.
		//
		// WHY A CAP: usercmd.serverTime is stamped directly from cl.serverTime
		// (CL_FinishMove), so if cl.serverTime runs ahead of the server's sv.time
		// the URT game QVM computes a negative (→ 999) ping, triggering a ping-kick
		// that feeds the zombie-state reconnect flood.  The cap is the architecturally
		// correct fix — not a workaround — because the alternative would require
		// decoupling usercmd.serverTime from cl.serverTime.
		//
		// MARGIN vs BACKSTOP: capMs = snapshotMsec/4.  The extrapolate threshold in
		// the adaptive-timing check below is snapshotMsec/3 — slightly larger than
		// capMs — so CL_AdjustTimeDelta starts pulling serverTimeDelta back BEFORE
		// cl.serverTime can reach the cap boundary.  In normal operation the cap
		// fires rarely (jitter spikes only); it is a backstop, not a primary
		// timing control.  A margin that is too tight (e.g. 1ms) makes the cap fire
		// every inter-snapshot client frame, freezing cl.serverTime and causing
		// visible stutter and movement lag.
		//
		// RELEASE (2000ms): once drift ≥ 2s the server is presumed dead (no snapshot
		// for 2 full seconds).  The cap is released so cl.serverTime can advance and
		// the engine can extrapolate gracefully until cl_timeout fires.  This is an
		// absolute wall-clock threshold — not proportional to snapshotMsec — because
		// "server has been silent for 2 seconds" is rate-independent.
		{
			int capMs = cl.snapshotMsec / 4;
			if ( capMs < 2 ) capMs = 2;
			if ( capMs > 8 ) capMs = 8;
			if ( cl.serverTime >= cl.snap.serverTime ) {
				int drift = cl.serverTime - cl.snap.serverTime;
				if ( drift < 2000 ) { // release after 2s of silence (server presumed dead)
					cl.serverTime = cl.snap.serverTime - capMs;
					SCR_NetMonitorAddCapHit();
				} else {
					SCR_LogCapRelease( drift );
				}
			}
		}
		cl.oldServerTime = cl.serverTime;

		// Compute estimated frameInterpolation for the net monitor widget
		{
			const clSnapshot_t *prevSnap = &cl.snapshots[ (cl.snap.messageNum - 1) & PACKET_MASK ];
			int interval = cl.snap.serverTime - prevSnap->serverTime;
			if ( prevSnap->valid && interval > 0 ) {
				cl.frameInterpolation = (float)( cl.serverTime - prevSnap->serverTime ) / (float)interval;
				if ( cl.frameInterpolation < 0.0f ) cl.frameInterpolation = 0.0f;
				if ( cl.frameInterpolation > 1.0f ) cl.frameInterpolation = 1.0f;
			} else {
				cl.frameInterpolation = 0.0f;
			}
		}

		SCR_NetMonitorAddTimeDelta( cl.serverTimeDelta );

		// note if we are almost past the latest frame (without timeNudge),
		// so we will try and adjust back a bit when the next snapshot arrives.
		if ( cl_adaptiveTiming->integer && !cl.serverForbidsAdaptiveTiming ) {
			// Scale the detection window with the measured snapshot interval.
			// Evaluated in 1/4ms units so slowFrac state is visible.
			// At 20Hz: thresh=16ms (vs hardcoded 5ms) → better equilibrium on vanilla.
			int extrapolateThresh = cl.snapshotMsec / 3;
			if ( extrapolateThresh <  3 ) extrapolateThresh =  3;
			if ( extrapolateThresh > 16 ) extrapolateThresh = 16;
			if ( ( cls.realtime + cl.serverTimeDelta - cl.snap.serverTime ) * 4 + slowFrac >= -( extrapolateThresh * 4 ) ) {
				cl.extrapolatedSnapshot = qtrue;
				SCR_NetMonitorAddExtrap();
			}
		} else {
			// vanilla Q3: diff >= -5ms equivalent (when slowFrac=0, the full
			// adaptive check "(diff * 4 + slowFrac) >= -20" reduces to "diff >= -5")
			if ( cls.realtime + cl.serverTimeDelta - cl.snap.serverTime >= -5 ) {
				cl.extrapolatedSnapshot = qtrue;
			}
		}
	}

	// if we have gotten new snapshots, drift serverTimeDelta
	// don't do this every frame, or a period of packet loss would
	// make a huge adjustment
	if ( cl.newSnapshots ) {
		CL_AdjustTimeDelta();
	}

	if ( !clc.demoplaying ) {
		return;
	}

	// if we are playing a demo back, we can just keep reading
	// messages from the demo file until the cgame definitely
	// has valid snapshots to interpolate between

	// a timedemo will always use a deterministic set of time samples
	// no matter what speed machine it is run on,
	// while a normal demo may have different time samples
	// each time it is played back
	if ( com_timedemo->integer ) {
		if ( !clc.timeDemoStart ) {
			clc.timeDemoStart = Sys_Milliseconds();
		}
		clc.timeDemoFrames++;
		cl.serverTime = clc.timeDemoBaseTime + clc.timeDemoFrames * ( cl.snapshotMsec > 0 ? cl.snapshotMsec : 50 );
	}

	//while ( cl.serverTime >= cl.snap.serverTime ) {
	while ( cl.serverTime - cl.snap.serverTime >= 0 ) {
		// feed another message, which should change
		// the contents of cl.snap
		CL_ReadDemoMessage();
		if ( cls.state != CA_ACTIVE ) {
			return; // end of demo
		}
	}
}
