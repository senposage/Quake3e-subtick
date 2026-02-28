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

#include "server.h"
#include "sv_antilag.h"


/*
===============
SV_SendConfigstring

Creates and sends the server command necessary to update the CS index for the
given client
===============
*/
static void SV_SendConfigstring(client_t *client, int index)
{
	int maxChunkSize = MAX_STRING_CHARS - 24;
	int len;

	len = strlen(sv.configstrings[index]);

	if( len >= maxChunkSize ) {
		int		sent = 0;
		int		remaining = len;
		char	*cmd;
		char	buf[MAX_STRING_CHARS];

		while (remaining > 0 ) {
			if ( sent == 0 ) {
				cmd = "bcs0";
			}
			else if( remaining < maxChunkSize ) {
				cmd = "bcs2";
			}
			else {
				cmd = "bcs1";
			}
			Q_strncpyz( buf, &sv.configstrings[index][sent],
				maxChunkSize );

			SV_SendServerCommand( client, "%s %i \"%s\"", cmd,
				index, buf );

			sent += (maxChunkSize - 1);
			remaining -= (maxChunkSize - 1);
		}
	} else {
		// standard cs, just send it
		SV_SendServerCommand( client, "cs %i \"%s\"", index,
			sv.configstrings[index] );
	}
}

/*
===============
SV_UpdateConfigstrings

Called when a client goes from CS_PRIMED to CS_ACTIVE.  Updates all
Configstring indexes that have changed while the client was in CS_PRIMED
===============
*/
void SV_UpdateConfigstrings(client_t *client)
{
	int index;

	for( index = 0; index < MAX_CONFIGSTRINGS; index++ ) {
		// if the CS hasn't changed since we went to CS_PRIMED, ignore
		if(!client->csUpdated[index])
			continue;

		// do not always send server info to all clients
		if ( index == CS_SERVERINFO && ( SV_GentityNum( client - svs.clients )->r.svFlags & SVF_NOSERVERINFO ) ) {
			continue;
		}

		SV_SendConfigstring(client, index);
		client->csUpdated[index] = qfalse;
	}
}

/*
===============
SV_SetConfigstring

===============
*/
void SV_SetConfigstring (int index, const char *val) {
	int		i;
	client_t	*client;

	if ( index < 0 || index >= MAX_CONFIGSTRINGS ) {
		Com_Error (ERR_DROP, "SV_SetConfigstring: bad index %i", index);
	}

	if ( !val ) {
		val = "";
	}

	// don't bother broadcasting an update if no change
	if ( !strcmp( val, sv.configstrings[ index ] ) ) {
		return;
	}

	// change the string in sv
	Z_Free( sv.configstrings[index] );
	sv.configstrings[index] = CopyString( val );

	// send it to all the clients if we aren't
	// spawning a new server
	if ( sv.state == SS_GAME || sv.restarting ) {

		// send the data to all relevant clients
		for (i = 0, client = svs.clients; i < sv_maxclients->integer ; i++, client++) {
			if ( client->state < CS_ACTIVE ) {
				if ( client->state == CS_PRIMED )
					client->csUpdated[ index ] = qtrue;
				continue;
			}
			// do not always send server info to all clients
			if ( index == CS_SERVERINFO && ( SV_GentityNum( i )->r.svFlags & SVF_NOSERVERINFO ) ) {
				continue;
			}

			SV_SendConfigstring(client, index);
		}
	}
}


/*
===============
SV_GetConfigstring
===============
*/
void SV_GetConfigstring( int index, char *buffer, int bufferSize ) {
	if ( bufferSize < 1 ) {
		Com_Error( ERR_DROP, "SV_GetConfigstring: bufferSize == %i", bufferSize );
	}
	if ( index < 0 || index >= MAX_CONFIGSTRINGS ) {
		Com_Error (ERR_DROP, "SV_GetConfigstring: bad index %i", index);
	}
	if ( !sv.configstrings[index] ) {
		buffer[0] = '\0';
		return;
	}

	Q_strncpyz( buffer, sv.configstrings[index], bufferSize );
}


/*
===============
SV_SetUserinfo

===============
*/
void SV_SetUserinfo( int index, const char *val ) {
	if ( index < 0 || index >= sv_maxclients->integer ) {
		Com_Error (ERR_DROP, "SV_SetUserinfo: bad index %i", index);
	}

	if ( !val ) {
		val = "";
	}

	Q_strncpyz( svs.clients[index].userinfo, val, sizeof( svs.clients[ index ].userinfo ) );
	Q_strncpyz( svs.clients[index].name, Info_ValueForKey( val, "name" ), sizeof(svs.clients[index].name) );
}



/*
===============
SV_GetUserinfo

===============
*/
void SV_GetUserinfo( int index, char *buffer, int bufferSize ) {
	if ( bufferSize < 1 ) {
		Com_Error( ERR_DROP, "SV_GetUserinfo: bufferSize == %i", bufferSize );
	}
	if ( index < 0 || index >= sv_maxclients->integer ) {
		Com_Error (ERR_DROP, "SV_GetUserinfo: bad index %i", index);
	}
	Q_strncpyz( buffer, svs.clients[ index ].userinfo, bufferSize );
}


/*
================
SV_CreateBaseline

Entity baselines are used to compress non-delta messages
to the clients -- only the fields that differ from the
baseline will be transmitted
================
*/
static void SV_CreateBaseline( void ) {
	sharedEntity_t *ent;
	int				entnum;

	for ( entnum = 0; entnum < sv.num_entities ; entnum++ ) {
		ent = SV_GentityNum( entnum );
		if ( !ent->r.linked ) {
			continue;
		}
		ent->s.number = entnum;

		//
		// take current state as baseline
		//
		sv.svEntities[ entnum ].baseline = ent->s;
		sv.baselineUsed[ entnum ] = 1;
	}
}


/*
===============
SV_BoundMaxClients
===============
*/
static void SV_BoundMaxClients( int minimum ) {
	// get the current maxclients value
	Cvar_Get( "sv_maxclients", "8", 0 );

	sv_maxclients->modified = qfalse;

	if ( sv_maxclients->integer < minimum ) {
		Cvar_Set( "sv_maxclients", va("%i", minimum) );
	}

#ifdef USE_MV
    sv_maxclients->modified = qfalse;

    // get the current demoClients value
    Cvar_Get( "sv_mvClients", "0", 0 );
    sv_mvClients->modified = qfalse;

    if ( sv_mvClients->integer > sv_maxclients->integer ) {
        Cvar_Set( "sv_mvClients", va( "%i", sv_maxclients->integer ) );
        sv_mvClients->modified = qfalse;
    }
#endif
}


/*
===============
SV_SetSnapshotParams
===============
*/
static void SV_SetSnapshotParams( void )
{
#ifdef USE_MV
    svs.numSnapshotPSF = sv_mvClients->integer * PACKET_BACKUP * MAX_CLIENTS;

    // reserve 2 additional frames for recorder slot
    svs.numSnapshotPSF += 2 * MAX_CLIENTS;

    if ( svs.numSnapshotPSF )
        svs.modSnapshotPSF = ( 0x10000000 / svs.numSnapshotPSF ) * svs.numSnapshotPSF;
    else
        svs.modSnapshotPSF = 1;
#endif

    // PACKET_BACKUP frames is just about 6.67MB so use that even on listen servers
	svs.numSnapshotEntities = PACKET_BACKUP * MAX_GENTITIES;
}


/*
===============
SV_Startup

Called when a host starts a map when it wasn't running
one before.  Successive map or map_restart commands will
NOT cause this to be called, unless the game is exited to
the menu system first.
===============
*/
static void SV_Startup( void ) {
	if ( svs.initialized ) {
		Com_Error( ERR_FATAL, "SV_Startup: svs.initialized" );
	}
	SV_BoundMaxClients( 1 );

#ifdef USE_MV
	svs.clients = Z_TagMalloc( ( sv_maxclients->integer + 1 ) * sizeof( client_t ), TAG_CLIENTS ); // +1 client slot for recorder
	Com_Memset( svs.clients, 0, ( sv_maxclients->integer + 1 ) * sizeof( client_t ) );
#else
	svs.clients = Z_TagMalloc( sv_maxclients->integer * sizeof( client_t ), TAG_CLIENTS );
	Com_Memset( svs.clients, 0, sv_maxclients->integer * sizeof( client_t ) );
#endif
	SV_SetSnapshotParams();
	svs.initialized = qtrue;

	// Don't respect sv_killserver unless a server is actually running
	if ( sv_killserver->integer ) {
		Cvar_Set( "sv_killserver", "0" );
	}

	Cvar_Set( "sv_running", "1" );

	// Join the ipv6 multicast group now that a map is running so clients can scan for us on the local network.
#ifdef USE_IPV6
	NET_JoinMulticast6();
#endif
}


/*
==================
SV_ChangeMaxClients
==================
*/
void SV_ChangeMaxClients( void ) {
	int		    oldMaxClients;
	int		    i;
	client_t	*oldClients;
	int		    count;
#ifdef USE_MV
	int		    oldMVClients;
	client_t    recorder;
#endif

	// get the highest client number in use
	count = 0;
	for ( i = 0 ; i < sv_maxclients->integer ; i++ ) {
		if ( svs.clients[i].state >= CS_CONNECTED ) {
			if (i > count)
				count = i;
		}
	}
	count++;

#ifdef USE_MV
    if ( sv_demoFile != FS_INVALID_HANDLE && svs.clients[ sv_maxclients->integer ].state >= CS_ACTIVE )
        // save recorder slot state
        memcpy( &recorder, &svs.clients[ sv_maxclients->integer ], sizeof( recorder ) );
    else
        recorder.multiview.recorder = qfalse;

    oldMVClients = sv_mvClients->integer;
#endif

	oldMaxClients = sv_maxclients->integer;
	// never go below the highest client number in use
	SV_BoundMaxClients( count );
	// if still the same
#ifdef USE_MV
	if ( sv_maxclients->integer == oldMaxClients && sv_mvClients->integer == oldMVClients ) {
#else
	if ( sv_maxclients->integer == oldMaxClients ) {
#endif
		return;
	}

	oldClients = Hunk_AllocateTempMemory( count * sizeof(client_t) );
	// copy the clients to hunk memory
	for ( i = 0 ; i < count ; i++ ) {
		if ( svs.clients[i].state >= CS_CONNECTED ) {
			oldClients[i] = svs.clients[i];
		}
		else {
			Com_Memset(&oldClients[i], 0, sizeof(client_t));
		}
	}

	// free old clients arrays
	Z_Free( svs.clients );

	// allocate new clients
#ifdef USE_MV
	svs.clients = Z_TagMalloc( ( sv_maxclients->integer + 1 ) * sizeof( client_t ), TAG_CLIENTS );
	Com_Memset( svs.clients, 0, ( sv_maxclients->integer + 1 ) * sizeof( client_t ) );
#else
	svs.clients = Z_TagMalloc( sv_maxclients->integer * sizeof( client_t ), TAG_CLIENTS );
	Com_Memset( svs.clients, 0, sv_maxclients->integer * sizeof( client_t ) );
#endif

	// copy the clients over
	for ( i = 0 ; i < count ; i++ ) {
		if ( oldClients[i].state >= CS_CONNECTED ) {
			svs.clients[i] = oldClients[i];
		}
	}

#ifdef USE_MV
	if ( recorder.multiview.recorder ) {
		// restore recorder slot state
		Com_Memcpy( &svs.clients[ sv_maxclients->integer ], &recorder, sizeof( recorder ) );
	}
#endif

	// free the old clients on the hunk
	Hunk_FreeTempMemory( oldClients );

	SV_SetSnapshotParams();
}


/*
================
SV_ClearServer
================
*/
static void SV_ClearServer( void ) {
	int i;

	for ( i = 0 ; i < MAX_CONFIGSTRINGS ; i++ ) {
		if ( sv.configstrings[i] ) {
			Z_Free( sv.configstrings[i] );
		}
	}

	if ( !sv_levelTimeReset->integer ) {
		i = sv.time;
		Com_Memset( &sv, 0, sizeof( sv ) );
		sv.time = i;
	} else {
		Com_Memset( &sv, 0, sizeof( sv ) );
	}
}


/*
================
SV_SpawnServer

Change the server to a new map, taking all connected
clients along with it.
This is NOT called for map_restart
================
*/
void SV_SpawnServer( const char *mapname, qboolean killBots ) {
	int			i;
	int			checksum;
	qboolean	isBot;
	const char	*p;

	// shut down the existing game if it is running
	SV_ShutdownGameProgs();

	Com_Printf( "------ Server Initialization ------\n" );
	Com_Printf( "Server: %s\n", mapname );

	Sys_SetStatus( "Initializing server..." );

#ifdef USE_MV
	SV_LoadRecordCache();
#endif

#ifndef DEDICATED
	// if not running a dedicated server CL_MapLoading will connect the client to the server
	// also print some status stuff
	CL_MapLoading();

	// make sure all the client stuff is unloaded
	CL_ShutdownAll();
#endif

	// clear the whole hunk because we're (re)loading the server
	Hunk_Clear();

	// clear collision map data
	CM_ClearMap();

	// timescale can be updated before SV_Frame() and cause division-by-zero in SV_RateMsec()
	Cvar_CheckRange( com_timescale, "0.001", NULL, CV_FLOAT );

	// Restart renderer?
	// CL_StartHunkUsers( );

	// init client structures and svs.numSnapshotEntities
	if ( !Cvar_VariableIntegerValue( "sv_running" ) ) {
		SV_Startup();
	} else {
		// check for maxclients change

#ifdef USE_MV
		if ( sv_maxclients->modified || sv_mvClients->modified ) {
#else
        if ( sv_maxclients->modified ) {
#endif
			SV_ChangeMaxClients();
		}
	}

#ifndef DEDICATED
	// remove pure paks that may left from client-side
	FS_PureServerSetLoadedPaks( "", "" );
	FS_PureServerSetReferencedPaks( "", "" );
#endif

	// clear pak references
	FS_ClearPakReferences( 0 );

	// allocate the snapshot entities on the hunk
	svs.snapshotEntities = Hunk_Alloc( sizeof(entityState_t)*svs.numSnapshotEntities, h_high );

	// initialize snapshot storage
	SV_InitSnapshotStorage();

#ifdef USE_MV
	// MV protocol support
	if ( svs.numSnapshotPSF ) // can be zero?
		svs.snapshotPSF = Hunk_Alloc( sizeof(psFrame_t)*svs.numSnapshotPSF, h_high );
	else
		svs.snapshotPSF = NULL;

	svs.nextSnapshotPSF = 0;
#endif

	// toggle the server bit so clients can detect that a
	// server has changed
	svs.snapFlagServerBit ^= SNAPFLAG_SERVERCOUNT;

	// set nextmap to the same map, but it may be overridden
	// by the game startup or another console command
	Cvar_Set( "nextmap", "map_restart 0" );
//	Cvar_Set( "nextmap", va("map %s", server) );

	// try to reset level time if server is empty
	if ( !sv_levelTimeReset->integer && !sv.restartTime ) {
		for ( i = 0; i < sv_maxclients->integer; i++ ) {
			if ( svs.clients[i].state >= CS_CONNECTED ) {
				break;
			}
		}
		if ( i == sv_maxclients->integer ) {
			sv.time = 0;
		}
	}

	for ( i = 0; i < sv_maxclients->integer; i++ ) {
		// save when the server started for each client already connected
		if ( svs.clients[i].state >= CS_CONNECTED && sv_levelTimeReset->integer ) {
			svs.clients[i].oldServerTime = sv.time;
		} else {
			svs.clients[i].oldServerTime = 0;
		}
	}

	// wipe the entire per-level structure
	SV_ClearServer();
	for ( i = 0 ; i < MAX_CONFIGSTRINGS ; i++ ) {
		sv.configstrings[i] = CopyString("");
	}

	// make sure we are not paused
#ifndef DEDICATED
	Cvar_Set( "cl_paused", "0" );
#endif

	// get latched value
	Cvar_Get( "sv_pure", "1", CVAR_SYSTEMINFO | CVAR_LATCH );

	// get a new checksum feed and restart the file system
	srand( Com_Milliseconds() );
	Com_RandomBytes( (byte*)&sv.checksumFeed, sizeof( sv.checksumFeed ) );
	FS_Restart( sv.checksumFeed );

	Sys_SetStatus( "Loading map %s", mapname );
	CM_LoadMap( va( "maps/%s.bsp", mapname ), qfalse, &checksum );

	// set serverinfo visible name
	Cvar_Set( "mapname", mapname );

	Cvar_Set( "sv_mapChecksum", va( "%i",checksum ) );

	// serverid should be different each time
	sv.serverId = com_frameTime;
	sv.restartedServerId = sv.serverId; // I suppose the init here is just to be safe
	sv.checksumFeedServerId = sv.serverId;
	Cvar_Set( "sv_serverid", va( "%i", sv.serverId ) );

	// clear physics interaction links
	SV_ClearWorld();

	// media configstring setting should be done during
	// the loading stage, so connected clients don't have
	// to load during actual gameplay
	sv.state = SS_LOADING;

	// make sure that level time is not zero
	sv.time = sv.time ? sv.time : 8;
	sv.gameTime = sv.time;
	sv.gameTimeResidual = 0;

	// load and spawn all other entities
	SV_InitGameProgs();

	// don't allow a map_restart if game is modified
	sv_gametype->modified = qfalse;

	sv_pure->modified = qfalse;

	// run a few frames to allow everything to settle
	for ( i = 0; i < 3; i++ )
	{
		sv.time += 100;
		sv.gameTime += 100;
		VM_Call( gvm, 1, GAME_RUN_FRAME, sv.gameTime );
		SV_BotFrame( sv.gameTime );
	}

	// create a baseline for more efficient communications
	SV_CreateBaseline();

#ifdef USE_SERVER_DEMO
    // stop server-side demo (if any)
	if (com_dedicated->integer)
		Cbuf_ExecuteText(EXEC_NOW, "stopserverdemo all");
#endif

	for ( i = 0; i < sv_maxclients->integer; i++ ) {
		// send the new gamestate to all connected clients
		if ( svs.clients[i].state >= CS_CONNECTED ) {
			const char *denied;

			if ( svs.clients[i].netchan.remoteAddress.type == NA_BOT ) {
				if ( killBots ) {
					SV_DropClient( &svs.clients[i], "was kicked" );
					continue;
				}
				isBot = qtrue;
			}
			else {
				isBot = qfalse;
			}

			// connect the client again
			denied = GVM_ArgPtr( VM_Call( gvm, 3, GAME_CLIENT_CONNECT, i, qfalse, isBot ) );	// firstTime = qfalse
			if ( denied ) {
				// this generally shouldn't happen, because the client
				// was connected before the level change
				SV_DropClient( &svs.clients[i], denied );
			} else {
				if( !isBot ) {
					// when we get the next packet from a connected client,
					// the new gamestate will be sent
					svs.clients[i].state = CS_CONNECTED;
				}
				else {
					client_t		*client;
					sharedEntity_t	*ent;

					client = &svs.clients[i];
					client->state = CS_ACTIVE;
					ent = SV_GentityNum( i );
					ent->s.number = i;
					client->gentity = ent;

					client->deltaMessage = client->netchan.outgoingSequence - ( PACKET_BACKUP + 1 ); // force delta reset
					client->lastSnapshotTime = svs.time - 9999; // generate a snapshot immediately

					VM_Call( gvm, 1, GAME_CLIENT_BEGIN, i );
				}
			}
		}
	}

	// run another frame to allow things to look at all the players
	sv.time += 100;
	sv.gameTime += 100;
	VM_Call( gvm, 1, GAME_RUN_FRAME, sv.gameTime );
	SV_BotFrame( sv.gameTime );
	svs.time += 100;

	// we need to touch the cgame and ui qvm because they could be in
	// separate pk3 files and the client will need to download the pk3
	// files with the latest cgame and ui qvm to pass the pure check
	FS_TouchFileInPak( "vm/cgame.qvm" );
	FS_TouchFileInPak( "vm/ui.qvm" );

	// the server sends these to the clients so they can figure
	// out which pk3s should be auto-downloaded
	p = FS_ReferencedPakNames();
	if ( FS_ExcludeReference() ) {
		// \fs_excludeReference may mask our current ui/cgame qvms
		FS_TouchFileInPak( "vm/cgame.qvm" );
		FS_TouchFileInPak( "vm/ui.qvm" );
		// rebuild referenced paks list
		p = FS_ReferencedPakNames();
	}
	Cvar_Set( "sv_referencedPakNames", p );

	p = FS_ReferencedPakChecksums();
	Cvar_Set( "sv_referencedPaks", p );

	Cvar_Set( "sv_paks", "" );
	Cvar_Set( "sv_pakNames", "" ); // not used on client-side

	if ( sv_pure->integer ) {
		int freespace, pakslen, infolen;
		qboolean overflowed = qfalse;
		qboolean infoTruncated = qfalse;

		p = FS_LoadedPakChecksums( &overflowed );

		pakslen = strlen( p ) + 9; // + strlen( "\\sv_paks\\" )
		freespace = SV_RemainingGameState();
		infolen = strlen( Cvar_InfoString_Big( CVAR_SYSTEMINFO, &infoTruncated ) );

		if ( infoTruncated ) {
			Com_Printf( S_COLOR_YELLOW "WARNING: truncated systeminfo!\n" );
		}

		if ( pakslen > freespace || infolen + pakslen >= BIG_INFO_STRING || overflowed ) {
			// switch to degraded pure mode
			// this could *potentially* lead to a false "unpure client" detection
			// which is better than guaranteed drop
			Com_DPrintf( S_COLOR_YELLOW "WARNING: skipping sv_paks setup to avoid gamestate overflow\n" );
		} else {
			// the server sends these to the clients so they will only
			// load pk3s also loaded at the server
			Cvar_Set( "sv_paks", p );
			if ( *p == '\0' ) {
				Com_Printf( S_COLOR_YELLOW "WARNING: sv_pure set but no PK3 files loaded\n" );
			}
		}
	}

	// save systeminfo and serverinfo strings
	SV_SetConfigstring( CS_SYSTEMINFO, Cvar_InfoString_Big( CVAR_SYSTEMINFO, NULL ) );
	cvar_modifiedFlags &= ~CVAR_SYSTEMINFO;

	SV_SetConfigstring( CS_SERVERINFO, Cvar_InfoString( CVAR_SERVERINFO, NULL ) );
	cvar_modifiedFlags &= ~CVAR_SERVERINFO;

	// any media configstring setting now should issue a warning
	// and any configstring changes should be reliably transmitted
	// to all clients
	sv.state = SS_GAME;

	// send a heartbeat now so the master will get up to date info
	SV_Heartbeat_f();

	Hunk_SetMark();

	Com_Printf ("-----------------------------------\n");

	Sys_SetStatus( "Running map %s", mapname );
}


/*
===============
SV_Init

Only called at main exe startup, not for each game
===============
*/
void SV_Init( void )
{
	int index;

	SV_AddOperatorCommands();

	if ( com_dedicated->integer )
		SV_AddDedicatedCommands();

	// serverinfo vars
	Cvar_Get ("dmflags", "0", CVAR_SERVERINFO);
	Cvar_Get ("fraglimit", "20", CVAR_SERVERINFO);
	Cvar_Get ("timelimit", "0", CVAR_SERVERINFO);
	sv_gametype = Cvar_Get ("g_gametype", "0", CVAR_SERVERINFO | CVAR_LATCH );
    Cvar_SetDescription(sv_gametype, "Holds the game style for the current match included in server info\nDefault: 0");

    Cvar_Get ("sv_keywords", "", CVAR_SERVERINFO);
    Cvar_SetDescription(Cvar_Get ("sv_keywords", "", CVAR_SERVERINFO),
                        "Holds the search string entered in the internet connection menu\nDefault: empty");
	Cvar_Get ("protocol", va("%i", PROTOCOL_VERSION), CVAR_SERVERINFO | CVAR_ROM);
    Cvar_SetDescription(Cvar_Get ("protocol", va("%i", PROTOCOL_VERSION), CVAR_SERVERINFO | CVAR_ROM),
                        "Holds the network protocol version\nDefault: " XSTRING(PROTOCOL_VERSION));
	sv_mapname = Cvar_Get ("mapname", "nomap", CVAR_SERVERINFO | CVAR_ROM);
    Cvar_SetDescription(sv_mapname, "Holds the name of the current map\nDefault: nomap");

    sv_privateClients = Cvar_Get( "sv_privateClients", "0", CVAR_SERVERINFO );
    Cvar_SetDescription(sv_privateClients, "The number of spots, out of sv_maxclients, reserved for players with the server password\nDefault: 0");

    Cvar_CheckRange( sv_privateClients, "0", va( "%i", MAX_CLIENTS-1 ), CV_INTEGER );
	sv_hostname = Cvar_Get ("sv_hostname", "noname", CVAR_SERVERINFO | CVAR_ARCHIVE );
    Cvar_SetDescription(sv_hostname, "Set the name of the server\nDefault: noname");

    sv_maxclients = Cvar_Get ("sv_maxclients", "8", CVAR_SERVERINFO | CVAR_LATCH);
	Cvar_CheckRange( sv_maxclients, "1", XSTRING(MAX_CLIENTS), CV_INTEGER );
    Cvar_SetDescription(sv_maxclients, "Maximum number of people allowed to join the server\nDefault: 8");

    sv_maxclientsPerIP = Cvar_Get( "sv_maxclientsPerIP", "3", CVAR_ARCHIVE );
	Cvar_CheckRange( sv_maxclientsPerIP, "1", NULL, CV_INTEGER );
	Cvar_SetDescription( sv_maxclientsPerIP, "Limits number of simultaneous connections from the same IP address." );
    Cvar_SetDescription( sv_maxclientsPerIP, "Limits number of simultaneous connections from the same IP address.\nDefault: 3" );

    sv_clientTLD = Cvar_Get( "sv_clientTLD", "0", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( sv_clientTLD, NULL, NULL, CV_INTEGER );
    Cvar_SetDescription(sv_clientTLD, "Include client locations in status and demo recordings\nDefault: 0");

#ifdef USE_MV
    Cvar_Get( "mvproto", va( "%i", MV_PROTOCOL_VERSION ), CVAR_SERVERINFO | CVAR_ROM );
    sv_autoRecord = Cvar_Get( "sv_mvAutoRecord", "0", CVAR_ARCHIVE | CVAR_SERVERINFO );
    Cvar_SetDescription(sv_autoRecord, "Automatically record a multiview demo\nDefault: 0");

    sv_demoFlags = Cvar_Get( "sv_mvFlags", "3", CVAR_ARCHIVE );
    Cvar_SetDescription(sv_demoFlags, "Record scoring in the multiview demo\nDefault: 3");

    sv_mvClients = Cvar_Get( "sv_mvClients", "0", CVAR_ARCHIVE | CVAR_LATCH );
    Cvar_SetDescription(sv_mvClients, "Number of multiview clients allowed\nDefault: 8");
    Cvar_CheckRange( sv_mvClients, "0", NULL, CV_INTEGER );

    sv_mvPassword = Cvar_Get( "sv_mvPassword", "", CVAR_ARCHIVE );
    Cvar_SetDescription(sv_mvPassword, "Set the password for multiview clients\nDefault: empty");

    sv_mvFileCount = Cvar_Get( "sv_mvFileCount", "1024", CVAR_ARCHIVE );
    Cvar_CheckRange( sv_mvFileCount, "0", XSTRING( MAX_MV_FILES ), CV_INTEGER );
    Cvar_SetDescription(sv_mvFileCount, "Set the maximum number of multiview recordings before it reuses numeric names\nDefault: 1024");

    sv_mvFolderSize = Cvar_Get( "sv_mvFolderSize", "768", CVAR_ARCHIVE );
    Cvar_CheckRange( sv_mvFolderSize, "0", "2048", CV_INTEGER );
    Cvar_SetDescription(sv_mvFolderSize, "Set the multiview folder size for automatic recordings, rotate when maxed out\nDefault: 768");

    //Cvar_SetDescription( sv_mvFileCount, "Max. count of autorecorded demos, older demos will be deleted to release free space\n" );
    //Cvar_SetDescription( sv_mvFolderSize, "Max. total size of autorecorded demos in megabytes, older demos will be deleted to release free space\n" );

    SV_LoadRecordCache();
#endif

	sv_minRate = Cvar_Get ("sv_minRate", "0", CVAR_ARCHIVE_ND | CVAR_SERVERINFO );
    Cvar_SetDescription(sv_minRate, "Force clients to play with a minimum latency\nDefault: 0");

    sv_maxRate = Cvar_Get ("sv_maxRate", "0", CVAR_ARCHIVE_ND | CVAR_SERVERINFO );
    Cvar_SetDescription(sv_maxRate, "Force all clients to play with a max rate, limit an advantage for having a low latency\nDefault: 0");

    sv_minPing = Cvar_Get("sv_minPing", "0", CVAR_ARCHIVE | CVAR_SERVERINFO);
	sv_maxPing = Cvar_Get("sv_maxPing", "0", CVAR_ARCHIVE | CVAR_SERVERINFO);

	sv_dlRate = Cvar_Get("sv_dlRate", "100", CVAR_ARCHIVE | CVAR_SERVERINFO);
    Cvar_SetDescription(sv_dlRate, "Set the maximum rate for server downloads\nDefault: 100");

    sv_floodProtect = Cvar_Get ("sv_floodProtect", "1", CVAR_ARCHIVE | CVAR_SERVERINFO );
    Cvar_SetDescription( sv_floodProtect, "Toggle server flood protection to keep players from bringing the server down\nDefault: 1" );

    // systeminfo
	Cvar_Get( "sv_cheats", "1", CVAR_SYSTEMINFO | CVAR_ROM );
	sv_serverid = Cvar_Get( "sv_serverid", "0", CVAR_SYSTEMINFO | CVAR_ROM );
    Cvar_SetDescription(sv_serverid, "Hold the server ID sent to clients");

    sv_pure = Cvar_Get( "sv_pure", "1", CVAR_SYSTEMINFO | CVAR_LATCH );
    Cvar_SetDescription(sv_pure, "Make sure clients load the same pak files as the server, disallow native VMs\nDefault: 1");

    Cvar_Get( "sv_paks", "", CVAR_SYSTEMINFO | CVAR_ROM );
	Cvar_Get( "sv_pakNames", "", CVAR_SYSTEMINFO | CVAR_ROM );
	Cvar_Get( "sv_referencedPaks", "", CVAR_SYSTEMINFO | CVAR_ROM );
	sv_referencedPakNames = Cvar_Get( "sv_referencedPakNames", "", CVAR_SYSTEMINFO | CVAR_ROM );
    Cvar_SetDescription(sv_referencedPakNames, "Holds the names of paks referenced by the server for comparison client-side\nDefault: empty");

    // server vars
	sv_rconPassword = Cvar_Get ("rconPassword", "", CVAR_TEMP );
    Cvar_SetDescription(sv_rconPassword, "Set the rcon password required to send the server commands\nDefault: empty");

    sv_privatePassword = Cvar_Get ("sv_privatePassword", "", CVAR_TEMP );
    Cvar_SetDescription(sv_privatePassword, "Set password for private clients to login\nDefault: empty");

    sv_fps = Cvar_Get ("sv_fps", "60", CVAR_TEMP | CVAR_PROTECTED );

    sv_gameHz = Cvar_Get ("sv_gameHz", "20", CVAR_ARCHIVE | CVAR_SERVERINFO );
    Cvar_SetDescription(sv_gameHz, "Rate at which level.time advances and GAME_RUN_FRAME fires (independent of sv_fps).\nKeep at 20 to match UT4.3 timer expectations. Never set higher than sv_fps.\n0 = disabled, falls back to sv_fps.\nDefault: 20");

    sv_snapshotFps = Cvar_Get ("sv_snapshotFps", "-1", CVAR_ARCHIVE | CVAR_SERVERINFO );
    Cvar_SetDescription(sv_snapshotFps, "Max snapshot send rate to clients.\n-1 = match sv_fps (default, live-tracks sv_fps changes).\n 0 = fall back to per-client 'snaps' userinfo (vanilla Q3 behavior).\n>0 = explicit rate, capped to sv_fps.\nDefault: -1");

    sv_busyWait = Cvar_Get ("sv_busyWait", "0", CVAR_ARCHIVE );
    Cvar_SetDescription(sv_busyWait, "Spin for last N milliseconds before each frame instead of sleeping.\nEliminates OS scheduler jitter at the cost of ~1 CPU core at 100pct.\nThe timeResidual clamping fix makes this unnecessary in normal operation.\nOnly enable if you observe measurable frame timing jitter on your specific hardware.\nDefault: 0 (disabled)");

    sv_pmoveMsec = Cvar_Get ("sv_pmoveMsec", "8", CVAR_ARCHIVE | CVAR_SERVERINFO );
    Cvar_SetDescription(sv_pmoveMsec, "Maximum physics step size in milliseconds. Enforces consistent movement\nregardless of client framerate. 8=125fps equivalent. 0=disabled.\nDefault: 8");
    Cvar_SetDescription(sv_fps, "Engine tick and input sampling rate (Hz). Higher values give finer input resolution.\nDefault: 60");

    sv_extrapolate = Cvar_Get ("sv_extrapolate", "1", CVAR_ARCHIVE );
    Cvar_SetDescription(sv_extrapolate, "Engine-side position correction for high sv_fps snapshots.\n"
        "0 = disabled (vanilla: players stutter when sv_fps > sv_gameHz)\n"
        "1 = enabled (real players use actual Pmove position, bots use velocity extrapolation)\n"
        "Interpolation windows: sv_fps 20=50ms, 60=16.7ms, 90=11.1ms, 125=8ms\n"
        "Default: 1");

    sv_smoothClients = Cvar_Get ("sv_smoothClients", "0", CVAR_ARCHIVE );
    Cvar_SetDescription(sv_smoothClients, "Set player trajectory type to TR_LINEAR for smoother client rendering.\n"
        "0 = TR_INTERPOLATE (cgame lerps between snapshot positions)\n"
        "1 = TR_LINEAR (cgame evaluates trBase + trDelta*dt continuously)\n"
        "Works independently of sv_extrapolate. Experimental.\n"
        "sv_bufferMs 0 = no position latency (trBase = current ps->origin).\n"
        "Default: 0");

    sv_bufferMs = Cvar_Get ("sv_bufferMs", "0", CVAR_ARCHIVE );
    Cvar_SetDescription(sv_bufferMs, "Per-client position delay in milliseconds.\n"
        "Requires sv_extrapolate 1 or sv_smoothClients 1 — has no effect in vanilla mode (both 0).\n"
        "0 = disabled (use latest position, no extra latency)\n"
        "-1 = auto (1000/sv_fps = one snapshot interval, minimum for clean interpolation)\n"
        "     sv_fps 20: 50ms, sv_fps 40: 25ms, sv_fps 60: 16ms, sv_fps 80: 12ms, sv_fps 100: 10ms\n"
        "1-100 = manual delay — trades latency for position stability\n"
        "Default: 0");

    sv_velSmooth = Cvar_Get ("sv_velSmooth", "32", CVAR_ARCHIVE );
    Cvar_SetDescription(sv_velSmooth, "Velocity smoothing window in milliseconds.\n"
        "Requires sv_smoothClients 1 (TR_LINEAR mode) — has no effect otherwise.\n"
        "Averages player velocity over the last N ms from the ring buffer.\n"
        "Reduces sawtooth artifacts from rapid direction changes.\n"
        "0 = disabled (use raw velocity)\n"
        "Default: 32");

    //Cvar_CheckRange( sv_fps, "20", "125", CV_INTEGER );
	sv_timeout = Cvar_Get( "sv_timeout", "200", CVAR_TEMP );
	Cvar_CheckRange( sv_timeout, "4", NULL, CV_INTEGER );
	Cvar_SetDescription( sv_timeout, "Seconds without any message before automatic client disconnect" );
	sv_zombietime = Cvar_Get( "sv_zombietime", "2", CVAR_TEMP );
	Cvar_CheckRange( sv_zombietime, "1", NULL, CV_INTEGER );
	Cvar_SetDescription( sv_zombietime, "Seconds to sink messages after disconnect" );
	Cvar_Get ("nextmap", "", CVAR_TEMP );

	sv_allowDownload = Cvar_Get ("sv_allowDownload", "1", CVAR_SERVERINFO);
    Cvar_SetDescription( sv_allowDownload, "Toggle the ability for clients to download files maps from server\nDefault: 1" );

    Cvar_Get ("sv_dlURL", "", CVAR_SERVERINFO | CVAR_ARCHIVE);
    Cvar_SetDescription(Cvar_Get ("sv_dlURL", "", CVAR_SERVERINFO | CVAR_ARCHIVE),
                        "Set the download URL for clients to download content\nDefault: empty");

    sv_master[0] = Cvar_Get ("sv_master1", MASTER_SERVER_NAME, 0 );
    sv_master[1] = Cvar_Get ("sv_master2", MASTER2_SERVER_NAME, CVAR_ARCHIVE );
    sv_master[2] = Cvar_Get ("sv_master3", MASTER3_SERVER_NAME, CVAR_ARCHIVE );

	for ( index = 0; index < MAX_MASTER_SERVERS; index++ )
		sv_master[ index ] = Cvar_Get( va( "sv_master%d", index + 1 ), "", CVAR_ARCHIVE_ND );

    sv_reconnectlimit = Cvar_Get( "sv_reconnectlimit", "3", 0 );
	Cvar_CheckRange( sv_reconnectlimit, "0", "12", CV_INTEGER );
    Cvar_SetDescription(sv_reconnectlimit, "Number of times a disconnected client can come back and reconnect\nDefault: 12");

    sv_padPackets = Cvar_Get ( "sv_padPackets", "0", CVAR_DEVELOPER );
    Cvar_SetDescription( sv_padPackets, "Toggles the padding of network packets\nDefault: 0" );

    sv_killserver = Cvar_Get ("sv_killserver", "0", 0);
    Cvar_SetDescription( sv_killserver, "Set to a one the server goes down\nDefault: 0" );

    sv_mapChecksum = Cvar_Get ( "sv_mapChecksum", "", CVAR_ROM );
    Cvar_SetDescription( sv_mapChecksum, "Allows clients to compare the map checksum\nDefault: empty" );

    sv_lanForceRate = Cvar_Get ( "sv_lanForceRate", "1", CVAR_ARCHIVE_ND );
    Cvar_SetDescription( sv_lanForceRate, "Force clients to use the same packet rate as the server\nDefault: 1" );

    sv_strictAuth = Cvar_Get ( "sv_strictAuth", "1", CVAR_ARCHIVE );

#ifdef USE_SERVER_DEMO
    sv_demonotice = Cvar_Get ( "sv_demonotice", "", CVAR_ARCHIVE );
	sv_demofolder = Cvar_Get ( "sv_demofolder", "serverdemos", CVAR_ARCHIVE );
#endif

#ifdef USE_AUTH
    sv_authServerIP = Cvar_Get( "sv_authServerIP", "", CVAR_TEMP | CVAR_ROM );
	sv_auth_engine = Cvar_Get( "sv_auth_engine", "1", CVAR_ROM );
#endif

#ifdef USE_BANS
	sv_banFile = Cvar_Get("sv_banFile", "serverbans.dat", CVAR_ARCHIVE);
    Cvar_SetDescription("Set the file to store a cache of all the player bans\nDefault: serverbans.dat");
#endif

	sv_levelTimeReset = Cvar_Get( "sv_levelTimeReset", "0", CVAR_ARCHIVE_ND );
    Cvar_SetDescription( sv_levelTimeReset, "Reset the clock in between matches\nDefault: 0");

    sv_filter = Cvar_Get( "sv_filter", "filter.txt", CVAR_ARCHIVE );
    Cvar_SetDescription(sv_filter, "Set the ban filter file\nDefault: filter.txt");

    // initialize bot cvars so they are listed and can be set before loading the botlib
	SV_BotInitCvars();

	// init the botlib here because we need the pre-compiler in the UI
	SV_BotInitBotLib();

#ifdef USE_BANS
	// Load saved bans
	Cbuf_AddText("rehashbans\n");
#endif

	// track group cvar changes
	Cvar_SetGroup( sv_lanForceRate, CVG_SERVER );
	Cvar_SetGroup( sv_minRate, CVG_SERVER );
	Cvar_SetGroup( sv_maxRate, CVG_SERVER );
	Cvar_SetGroup( sv_fps, CVG_SERVER );
	Cvar_SetGroup( sv_gameHz, CVG_SERVER );
	Cvar_SetGroup( sv_snapshotFps, CVG_SERVER );
	Cvar_SetGroup( sv_busyWait, CVG_SERVER );
	Cvar_SetGroup( sv_pmoveMsec, CVG_SERVER );
	Cvar_SetGroup( sv_extrapolate, CVG_SERVER );
	Cvar_SetGroup( sv_smoothClients, CVG_SERVER );
	Cvar_SetGroup( sv_bufferMs, CVG_SERVER );
	Cvar_SetGroup( sv_velSmooth, CVG_SERVER );

	// force initial check
	SV_TrackCvarChanges();

	SV_InitChallenger();

	SV_Antilag_Init();
	SV_SmoothInit();
}


/*
==================
SV_FinalMessage

Used by SV_Shutdown to send a final message to all
connected clients before the server goes down.  The messages are sent immediately,
not just stuck on the outgoing message list, because the server is going
to totally exit after returning from this function.
==================
*/
void SV_FinalMessage( const char *message ) {
	int			i, j;
	client_t	*cl;

	// send it twice, ignoring rate
	for ( j = 0 ; j < 2 ; j++ ) {
		for (i=0, cl = svs.clients ; i < sv_maxclients->integer ; i++, cl++) {
			if (cl->state >= CS_CONNECTED ) {
				// don't send a disconnect to a local client
				if ( cl->netchan.remoteAddress.type != NA_LOOPBACK ) {
					SV_SendServerCommand( cl, "print \"%s\n\"\n", message );
					SV_SendServerCommand( cl, "disconnect \"%s\"", message );
				}
				// force a snapshot to be sent
				cl->lastSnapshotTime = svs.time - 9999; // generate a snapshot immediately
				cl->state = CS_ZOMBIE; // skip delta generation
				SV_SendClientSnapshot( cl );
			}
		}
	}
}


/*
================
SV_Shutdown

Called when each game quits,
before Sys_Quit or Sys_Error
================
*/
void SV_Shutdown( const char *finalmsg ) {
	if ( !com_sv_running || !com_sv_running->integer ) {
#ifdef USE_MV
		SV_SaveRecordCache();
#endif
		return;
	}

	Com_Printf( "----- Server Shutdown (%s) -----\n", finalmsg );

#if USE_SERVER_DEMO
    if (com_dedicated->integer)
		Cbuf_ExecuteText(EXEC_NOW, "stopserverdemo all");
#endif

#ifdef USE_IPV6
	NET_LeaveMulticast6();
#endif

	if ( svs.clients && !com_errorEntered ) {
		SV_FinalMessage( finalmsg );
	}

#ifdef USE_MV
    if ( sv_demoFile != FS_INVALID_HANDLE ) {
        // finalize record
        if ( svs.clients[ sv_maxclients->integer ].multiview.recorder ) {
            SV_SendClientSnapshot( &svs.clients[ sv_maxclients->integer ] );
        }
        SV_MultiViewStopRecord_f();
    }

    SV_SaveRecordCache();
#endif

	SV_RemoveOperatorCommands();
	SV_MasterShutdown();
	SV_ShutdownGameProgs();
	SV_InitChallenger();

	// free current level
	SV_ClearServer();

	SV_FreeIP4DB();

	// free server static data
	if ( svs.clients ) {
		int index;

		for ( index = 0; index < sv_maxclients->integer; index++ )
			SV_FreeClient( &svs.clients[ index ] );

		Z_Free( svs.clients );
	}
	Com_Memset( &svs, 0, sizeof( svs ) );
	sv.time = 0;

	Cvar_Set( "sv_running", "0" );

	// allow setting timescale 0 for demo playback
	Cvar_CheckRange( com_timescale, "0", NULL, CV_FLOAT );

#ifndef DEDICATED
	Cvar_Set( "ui_singlePlayerActive", "0" );
#endif

	Com_Printf( "---------------------------\n" );

#ifndef DEDICATED
	// disconnect any local clients
	if ( sv_killserver->integer != 2 )
		CL_Disconnect( qfalse );
#endif

	// clean some server cvars
	Cvar_Set( "sv_referencedPaks", "" );
	Cvar_Set( "sv_referencedPakNames", "" );
	Cvar_Set( "sv_mapChecksum", "" );
	Cvar_Set( "sv_serverid", "0" );

	Sys_SetStatus( "Server is not running" );
}
