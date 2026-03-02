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

/*
===============================================================================

OPERATOR CONSOLE ONLY COMMANDS

These commands can only be entered from stdin or by a remote operator datagram
===============================================================================
*/


/*
==================
SV_GetPlayerByHandle

Returns the player with player id or name from Cmd_Argv(1)
==================
*/
client_t *SV_GetPlayerByHandle( void ) {
	client_t	*cl;
	int			i;
	char		*s;
	char		cleanName[ MAX_NAME_LENGTH ];

	// make sure server is running
	if ( !com_sv_running->integer ) {
		return NULL;
	}

	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "No player specified.\n" );
		return NULL;
	}

	s = Cmd_Argv(1);

	// Check whether this is a numeric player handle
	for(i = 0; s[i] >= '0' && s[i] <= '9'; i++);
	
	if(!s[i])
	{
		int plid = atoi(s);

		// Check for numeric playerid match
		if(plid >= 0 && plid < sv_maxclients->integer)
		{
			cl = &svs.clients[plid];
			
			if(cl->state)
				return cl;
		}
	}

	// check for a name match
	for ( i=0, cl=svs.clients ; i < sv_maxclients->integer ; i++,cl++ ) {
		if ( cl->state < CS_CONNECTED ) {
			continue;
		}
		if ( !Q_stricmp( cl->name, s ) ) {
			return cl;
		}

		Q_strncpyz( cleanName, cl->name, sizeof(cleanName) );
		Q_CleanStr( cleanName );
		if ( !Q_stricmp( cleanName, s ) ) {
			return cl;
		}
	}

	Com_Printf( "Player %s is not on the server\n", s );

	return NULL;
}


/*
==================
SV_GetPlayerByNum

Returns the player with idnum from Cmd_Argv(1)
==================
*/
static client_t *SV_GetPlayerByNum( void ) {
	client_t	*cl;
	int			i;
	int			idnum;
	char		*s;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		return NULL;
	}

	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "No player specified.\n" );
		return NULL;
	}

	s = Cmd_Argv(1);

	for (i = 0; s[i]; i++) {
		if (s[i] < '0' || s[i] > '9') {
			Com_Printf( "Bad slot number: %s\n", s);
			return NULL;
		}
	}
	idnum = atoi( s );
	if ( idnum < 0 || idnum >= sv_maxclients->integer ) {
		Com_Printf( "Bad client slot: %i\n", idnum );
		return NULL;
	}

	cl = &svs.clients[idnum];
	if ( !cl->state ) {
		Com_Printf( "Client %i is not active\n", idnum );
		return NULL;
	}
	return cl;
}

//=========================================================


/*
==================
SV_Map_f

Restart the server on a different map
==================
*/
static void SV_Map_f( void ) {
	char		*cmd;
	char		*map;
	qboolean	killBots, cheat;
	char		expanded[MAX_QPATH];
	char		mapname[MAX_QPATH];
	int			len;

	map = Cmd_Argv(1);
	if ( !map || !*map ) {
		return;
	}

	// make sure the level exists before trying to change, so that
	// a typo at the server console won't end the game
	Com_sprintf( expanded, sizeof( expanded ), "maps/%s.bsp", map );
	// bypass pure check so we can open downloaded map
	FS_BypassPure();
	len = FS_FOpenFileRead( expanded, NULL, qfalse );
	FS_RestorePure();
	if ( len == -1 ) {
		Com_Printf( "Can't find map %s\n", expanded );
		return;
	}

	// force latched values to get set
	Cvar_Get ("g_gametype", "0", CVAR_SERVERINFO | CVAR_USERINFO | CVAR_LATCH );
	Cvar_Get("sv_matchId", "0", CVAR_SERVERINFO | CVAR_USERINFO | CVAR_INIT | CVAR_PROTECTED);

	cmd = Cmd_Argv(0);
	if( Q_stricmpn( cmd, "sp", 2 ) == 0 ) {
		Cvar_SetIntegerValue( "g_gametype", GT_SINGLE_PLAYER );
		Cvar_Set( "g_doWarmup", "0" );
		// may not set sv_maxclients directly, always set latched
		Cvar_SetLatched( "sv_maxclients", "8" );
		cmd += 2;
		if (!Q_stricmp( cmd, "devmap" ) ) {
			cheat = qtrue;
		} else {
			cheat = qfalse;
		}
		killBots = qtrue;
	}
	else {
		if ( !Q_stricmp( cmd, "devmap" ) ) {
			cheat = qtrue;
			killBots = qtrue;
		} else {
			cheat = qfalse;
			killBots = qfalse;
		}
		if( sv_gametype->integer == GT_SINGLE_PLAYER ) {
			Cvar_SetIntegerValue( "g_gametype", GT_FFA );
		}
	}

	// save the map name here cause on a map restart we reload the q3config.cfg
	// and thus nuke the arguments of the map command
	Q_strncpyz(mapname, map, sizeof(mapname));

	// start up the map
	SV_SpawnServer( mapname, killBots );

	// set the cheat value
	// if the level was started with "map <levelname>", then
	// cheats will not be allowed.  If started with "devmap <levelname>"
	// then cheats will be allowed
	if ( cheat ) {
		Cvar_Set( "sv_cheats", "1" );
	} else {
		Cvar_Set( "sv_cheats", "0" );
	}
}


/*
================
SV_MapRestart_f

Completely restarts a level, but doesn't send a new gamestate to the clients.
This allows fair starts with variable load times.
================
*/
static void SV_MapRestart_f( void ) {
	int			i;
	client_t	*client;
	char		*denied;
	qboolean	isBot;
	int			delay;

	// make sure we aren't restarting twice in the same frame
	if ( com_frameTime == sv.serverId ) {
		return;
	}

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( sv.restartTime ) {
		return;
	}

	if ( Cmd_Argc() > 1 ) {
		delay = atoi( Cmd_Argv(1) );
	} else {
		delay = 5;
	}

	if ( delay && !Cvar_VariableIntegerValue( "g_doWarmup" ) ) {
		sv.restartTime = sv.time + delay * 1000;
		if ( sv.restartTime == 0 ) {
			sv.restartTime = 1;
		}
		SV_SetConfigstring( CS_WARMUP, va( "%i", sv.restartTime ) );
		return;
	}

	// check for changes in variables that can't just be restarted
	// check for maxclients change

#ifdef USE_MV
	if ( sv_maxclients->modified || sv_gametype->modified || sv_pure->modified || sv_mvClients->modified ) {
#else
    if ( sv_maxclients->modified || sv_gametype->modified || sv_pure->modified ) {
#endif
		char	mapname[MAX_QPATH];

		Com_Printf( "variable change -- restarting.\n" );
		// restart the map the slow way
		Q_strncpyz( mapname, Cvar_VariableString( "mapname" ), sizeof( mapname ) );

#ifdef USE_MV
		SV_MultiViewStopRecord_f(); // as an alternative: save/restore recorder state and continue recording?
#endif

		SV_SpawnServer( mapname, qfalse );
		return;
	}

	// toggle the server bit so clients can detect that a
	// map_restart has happened
	svs.snapFlagServerBit ^= SNAPFLAG_SERVERCOUNT;

	// generate a new serverid	
	// TTimo - don't update restartedserverId there, otherwise we won't deal correctly with multiple map_restart
	sv.serverId = com_frameTime;
	Cvar_Set( "sv_serverid", va("%i", sv.serverId ) );

	// if a map_restart occurs while a client is changing maps, we need
	// to give them the correct time so that when they finish loading
	// they don't violate the backwards time check in cl_cgame.c
	for (i=0 ; i<sv_maxclients->integer ; i++) {
		if (svs.clients[i].state == CS_PRIMED) {
			svs.clients[i].oldServerTime = sv.restartTime;
		}
	}

	// reset all the vm data in place without changing memory allocation
	// note that we do NOT set sv.state = SS_LOADING, so configstrings that
	// had been changed from their default values will generate broadcast updates
	sv.state = SS_LOADING;
	sv.restarting = qtrue;

	// make sure that level time is not zero
	sv.time = sv.time ? sv.time : 8;
	sv.gameTime = sv.time;
	sv.gameTimeResidual = 0;
	sv.timeResidual = 0;
	SV_SmoothInit(); // flush stale ring buffer entries from the previous round

	SV_RestartGameProgs();

	// run a few frames to allow everything to settle
	// sv.time and sv.gameTime advance in lockstep here (100ms steps each),
	// bypassing the sv_gameHz inner loop. sv_gameHz decoupling only applies
	// during live gameplay in SV_Frame — not during startup/restart settlement.
	for ( i = 0; i < 3; i++ )
	{
		sv.time += 100;
		sv.gameTime += 100;
		VM_Call( gvm, 1, GAME_RUN_FRAME, sv.gameTime );
	}

	sv.state = SS_GAME;
	sv.restarting = qfalse;

	// connect and begin all the clients
	for ( i = 0 ; i < sv_maxclients->integer ; i++ ) {
		client = &svs.clients[i];

		// send the new gamestate to all connected clients
		if ( client->state < CS_CONNECTED ) {
			continue;
		}

		if ( client->netchan.remoteAddress.type == NA_BOT ) {
			isBot = qtrue;
		} else {
			isBot = qfalse;
		}

		// add the map_restart command
		SV_AddServerCommand( client, "map_restart\n" );

		// connect the client again, without the firstTime flag
		denied = GVM_ArgPtr( VM_Call( gvm, 3, GAME_CLIENT_CONNECT, i, qfalse, isBot ) );
		if ( denied ) {
			// this generally shouldn't happen, because the client
			// was connected before the level change
			SV_DropClient( client, denied );
			Com_Printf( "SV_MapRestart_f(%d): dropped client %i - denied!\n", delay, i );
			continue;
		}

		if ( client->state == CS_ACTIVE )
			SV_ClientEnterWorld( client, &client->lastUsercmd );
		else {
			// If we don't reset client->lastUsercmd and are restarting during map load,
			// the client will hang because we'll use the last Usercmd from the previous map,
			// which is wrong obviously.
			SV_ClientEnterWorld( client, NULL );
		}
	}

	// run another frame to allow things to look at all the players
	// sv.time and sv.gameTime advance in lockstep here (same direct pattern as
	// the settlement loop above — bypasses sv_gameHz inner-loop logic).
	sv.time += 100;
	sv.gameTime += 100;
	VM_Call( gvm, 1, GAME_RUN_FRAME, sv.gameTime );
	svs.time += 100;
}


/*
==================
SV_Kick_f

Kick a user off of the server  FIXME: move to game
==================
*/
static void SV_Kick_f( void ) {
	client_t	*cl;
	int			i;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( Cmd_Argc() != 2 ) {
		Com_Printf ("Usage: kick <player name>\nkick all = kick everyone\nkick allbots = kick all bots\n");
		return;
	}

	cl = SV_GetPlayerByHandle();
	if ( !cl ) {
		if ( !Q_stricmp(Cmd_Argv(1), "all") ) {
			for ( i=0, cl=svs.clients ; i < sv_maxclients->integer ; i++,cl++ ) {
				if ( !cl->state ) {
					continue;
				}
				if( cl->netchan.remoteAddress.type == NA_LOOPBACK ) {
					continue;
				}
				SV_DropClient( cl, "was kicked" );
				cl->lastPacketTime = svs.time;	// in case there is a funny zombie
			}
		}
		else if ( !Q_stricmp(Cmd_Argv(1), "allbots") ) {
			for ( i=0, cl=svs.clients ; i < sv_maxclients->integer ; i++,cl++ ) {
				if ( !cl->state ) {
					continue;
				}
				if( cl->netchan.remoteAddress.type != NA_BOT ) {
					continue;
				}
				SV_DropClient( cl, "was kicked" );
				cl->lastPacketTime = svs.time;	// in case there is a funny zombie
			}
		}
		return;
	}
	if( cl->netchan.remoteAddress.type == NA_LOOPBACK ) {
		Com_Printf("Cannot kick host player\n");

		return;
	}

	SV_DropClient( cl, "was kicked" );
	cl->lastPacketTime = svs.time;	// in case there is a funny zombie
}


#ifndef STANDALONE
// these functions require the auth server which of course is not available anymore for stand-alone games.

#ifdef USE_BANS
/*
==================
SV_Ban_f

Ban a user from being able to play on this server through the auth
server
==================
*/
static void SV_Ban_f( void ) {
	client_t	*cl;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( Cmd_Argc() != 2 ) {
		Com_Printf ("Usage: banUser <player name>\n");
		return;
	}

	cl = SV_GetPlayerByHandle();

	if (!cl) {
		return;
	}

	if( cl->netchan.remoteAddress.type == NA_LOOPBACK ) {
		Com_Printf("Cannot kick host player\n");
		return;
	}

	// look up the authorize server's IP
	if ( !svs.authorizeAddress.ip[0] && svs.authorizeAddress.type != NA_BAD ) {
		Com_Printf( "Resolving %s\n", AUTHORIZE_SERVER_NAME );
		if ( !NET_StringToAdr( AUTHORIZE_SERVER_NAME, &svs.authorizeAddress, NA_IP ) ) {
			Com_Printf( "Couldn't resolve address\n" );
			return;
		}
		svs.authorizeAddress.port = BigShort( PORT_AUTHORIZE );
		Com_Printf( "%s resolved to %i.%i.%i.%i:%i\n", AUTHORIZE_SERVER_NAME,
			svs.authorizeAddress.ip[0], svs.authorizeAddress.ip[1],
			svs.authorizeAddress.ip[2], svs.authorizeAddress.ip[3],
			BigShort( svs.authorizeAddress.port ) );
	}

	// otherwise send their ip to the authorize server
	if ( svs.authorizeAddress.type != NA_BAD ) {
		NET_OutOfBandPrint( NS_SERVER, &svs.authorizeAddress,
			"banUser %i.%i.%i.%i", cl->netchan.remoteAddress.ip[0], cl->netchan.remoteAddress.ip[1], 
								   cl->netchan.remoteAddress.ip[2], cl->netchan.remoteAddress.ip[3] );
		Com_Printf("%s was banned from coming back\n", cl->name);
	}
}

/*
==================
SV_BanNum_f

Ban a user from being able to play on this server through the auth
server
==================
*/
static void SV_BanNum_f( void ) {
	client_t	*cl;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( Cmd_Argc() != 2 ) {
		Com_Printf ("Usage: banClient <client number>\n");
		return;
	}

	cl = SV_GetPlayerByNum();
	if ( !cl ) {
		return;
	}
	if( cl->netchan.remoteAddress.type == NA_LOOPBACK ) {
		Com_Printf("Cannot kick host player\n");
		return;
	}

	// look up the authorize server's IP
	if ( !svs.authorizeAddress.ip[0] && svs.authorizeAddress.type != NA_BAD ) {
		Com_Printf( "Resolving %s\n", AUTHORIZE_SERVER_NAME );
		if ( !NET_StringToAdr( AUTHORIZE_SERVER_NAME, &svs.authorizeAddress, NA_IP ) ) {
			Com_Printf( "Couldn't resolve address\n" );
			return;
		}
		svs.authorizeAddress.port = BigShort( PORT_AUTHORIZE );
		Com_Printf( "%s resolved to %i.%i.%i.%i:%i\n", AUTHORIZE_SERVER_NAME,
			svs.authorizeAddress.ip[0], svs.authorizeAddress.ip[1],
			svs.authorizeAddress.ip[2], svs.authorizeAddress.ip[3],
			BigShort( svs.authorizeAddress.port ) );
	}

	// otherwise send their ip to the authorize server
	if ( svs.authorizeAddress.type != NA_BAD ) {
		NET_OutOfBandPrint( NS_SERVER, &svs.authorizeAddress,
			"banUser %i.%i.%i.%i", cl->netchan.remoteAddress.ip[0], cl->netchan.remoteAddress.ip[1], 
								   cl->netchan.remoteAddress.ip[2], cl->netchan.remoteAddress.ip[3] );
		Com_Printf("%s was banned from coming back\n", cl->name);
	}
}

#endif // USE_BANS
#endif // !COM_STANDALONE

#ifdef USE_BANS
/*
==================
SV_RehashBans_f

Load saved bans from file.
==================
*/
static void SV_RehashBans_f(void)
{
	int index, filelen;
	fileHandle_t readfrom;
	char *textbuf, *curpos, *maskpos, *newlinepos, *endpos;
	char filepath[MAX_QPATH];
	
	// make sure server is running
	if ( !com_sv_running->integer ) {
		return;
	}
	
	serverBansCount = 0;
	
	if(!sv_banFile->string || !*sv_banFile->string)
		return;

	Com_sprintf(filepath, sizeof(filepath), "%s/%s", FS_GetCurrentGameDir(), sv_banFile->string);

	if((filelen = FS_SV_FOpenFileRead(filepath, &readfrom)) >= 0)
	{
		if(filelen < 2)
		{
			// Don't bother if file is too short.
			FS_FCloseFile(readfrom);
			return;
		}

		curpos = textbuf = Z_Malloc(filelen);
		
		filelen = FS_Read(textbuf, filelen, readfrom);
		FS_FCloseFile(readfrom);
		
		endpos = textbuf + filelen;
		
		for(index = 0; index < SERVER_MAXBANS && curpos + 2 < endpos; index++)
		{
			// find the end of the address string
			for(maskpos = curpos + 2; maskpos < endpos && *maskpos != ' '; maskpos++);
			
			if(maskpos + 1 >= endpos)
				break;

			*maskpos = '\0';
			maskpos++;
			
			// find the end of the subnet specifier
			for(newlinepos = maskpos; newlinepos < endpos && *newlinepos != '\n'; newlinepos++);
			
			if(newlinepos >= endpos)
				break;
			
			*newlinepos = '\0';
			
			if(NET_StringToAdr(curpos + 2, &serverBans[index].ip, NA_UNSPEC))
			{
				serverBans[index].isexception = (curpos[0] != '0');
				serverBans[index].subnet = atoi(maskpos);
				
				if(serverBans[index].ip.type == NA_IP &&
				   (serverBans[index].subnet < 1 || serverBans[index].subnet > 32))
				{
					serverBans[index].subnet = 32;
				}
				else if(serverBans[index].ip.type == NA_IP6 &&
					(serverBans[index].subnet < 1 || serverBans[index].subnet > 128))
				{
					serverBans[index].subnet = 128;
				}
			}
			
			curpos = newlinepos + 1;
		}
			
		serverBansCount = index;
		
		Z_Free(textbuf);
	}
}

/*
==================
SV_WriteBans

Save bans to file.
==================
*/
static void SV_WriteBans(void)
{
	int index;
	fileHandle_t writeto;
	char filepath[MAX_QPATH];
	
	if(!sv_banFile->string || !*sv_banFile->string)
		return;
	
	Com_sprintf(filepath, sizeof(filepath), "%s/%s", FS_GetCurrentGameDir(), sv_banFile->string);

	if((writeto = FS_SV_FOpenFileWrite(filepath)))
	{
		char writebuf[128];
		serverBan_t *curban;
		
		for(index = 0; index < serverBansCount; index++)
		{
			curban = &serverBans[index];
			
			Com_sprintf(writebuf, sizeof(writebuf), "%d %s %d\n",
				    curban->isexception, NET_AdrToString(&curban->ip), curban->subnet);
			FS_Write(writebuf, strlen(writebuf), writeto);
		}

		FS_FCloseFile(writeto);
	}
}

/*
==================
SV_DelBanEntryFromList

Remove a ban or an exception from the list.
==================
*/

static qboolean SV_DelBanEntryFromList(int index)
{
	if(index == serverBansCount - 1)
		serverBansCount--;
	else if(index < ARRAY_LEN(serverBans) - 1)
	{
		memmove(serverBans + index, serverBans + index + 1, (serverBansCount - index - 1) * sizeof(*serverBans));
		serverBansCount--;
	}
	else
		return qtrue;

	return qfalse;
}

/*
==================
SV_ParseCIDRNotation

Parse a CIDR notation type string and return a netadr_t and suffix by reference
==================
*/

static qboolean SV_ParseCIDRNotation(netadr_t *dest, int *mask, char *adrstr)
{
	char *suffix;
	
	suffix = strchr(adrstr, '/');
	if(suffix)
	{
		*suffix = '\0';
		suffix++;
	}

	if(!NET_StringToAdr(adrstr, dest, NA_UNSPEC))
		return qtrue;

	if(suffix)
	{
		*mask = atoi(suffix);
		
		if(dest->type == NA_IP)
		{
			if(*mask < 1 || *mask > 32)
				*mask = 32;
		}
		else
		{
			if(*mask < 1 || *mask > 128)
				*mask = 128;
		}
	}
	else if(dest->type == NA_IP)
		*mask = 32;
	else
		*mask = 128;
	
	return qfalse;
}

/*
==================
SV_AddBanToList

Ban a user from being able to play on this server based on his ip address.
==================
*/

static void SV_AddBanToList(qboolean isexception)
{
	char *banstring;
	char addy2[NET_ADDRSTRMAXLEN];
	netadr_t ip;
	int index, argc, mask;
	serverBan_t *curban;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	argc = Cmd_Argc();
	
	if(argc < 2 || argc > 3)
	{
		Com_Printf ("Usage: %s (ip[/subnet] | clientnum [subnet])\n", Cmd_Argv(0));
		return;
	}

	if(serverBansCount >= ARRAY_LEN(serverBans))
	{
		Com_Printf ("Error: Maximum number of bans/exceptions exceeded.\n");
		return;
	}

	banstring = Cmd_Argv(1);
	
	if(strchr(banstring, '.') || strchr(banstring, ':'))
	{
		// This is an ip address, not a client num.
		
		if(SV_ParseCIDRNotation(&ip, &mask, banstring))
		{
			Com_Printf("Error: Invalid address %s\n", banstring);
			return;
		}
	}
	else
	{
		client_t *cl;
		
		// client num.
		
		cl = SV_GetPlayerByNum();

		if(!cl)
		{
			Com_Printf("Error: Playernum %s does not exist.\n", Cmd_Argv(1));
			return;
		}
		
		ip = cl->netchan.remoteAddress;
		
		if(argc == 3)
		{
			mask = atoi(Cmd_Argv(2));
			
			if(ip.type == NA_IP)
			{
				if(mask < 1 || mask > 32)
					mask = 32;
			}
			else
			{
				if(mask < 1 || mask > 128)
					mask = 128;
			}
		}
		else
			mask = (ip.type == NA_IP6) ? 128 : 32;
	}

	if(ip.type != NA_IP && ip.type != NA_IP6)
	{
		Com_Printf("Error: Can ban players connected via the internet only.\n");
		return;
	}

	// first check whether a conflicting ban exists that would supersede the new one.
	for(index = 0; index < serverBansCount; index++)
	{
		curban = &serverBans[index];
		
		if(curban->subnet <= mask)
		{
			if((curban->isexception || !isexception) && NET_CompareBaseAdrMask(&curban->ip, ip, &curban->subnet))
			{
				Q_strncpyz(addy2, NET_AdrToString(&ip), sizeof(addy2));
				
				Com_Printf("Error: %s %s/%d supersedes %s %s/%d\n", curban->isexception ? "Exception" : "Ban",
					   NET_AdrToString(&curban->ip), curban->subnet,
					   isexception ? "exception" : "ban", addy2, mask);
				return;
			}
		}
		if(curban->subnet >= mask)
		{
			if(!curban->isexception && isexception && NET_CompareBaseAdrMask(&curban->ip, &ip, mask))
			{
				Q_strncpyz(addy2, NET_AdrToString(&curban->ip), sizeof(addy2));
			
				Com_Printf("Error: %s %s/%d supersedes already existing %s %s/%d\n", isexception ? "Exception" : "Ban",
					   NET_AdrToString(&ip), mask,
					   curban->isexception ? "exception" : "ban", addy2, curban->subnet);
				return;
			}
		}
	}

	// now delete bans that are superseded by the new one
	index = 0;
	while(index < serverBansCount)
	{
		curban = &serverBans[index];
		
		if(curban->subnet > mask && (!curban->isexception || isexception) && NET_CompareBaseAdrMask(&curban->ip, &ip, mask))
			SV_DelBanEntryFromList(index);
		else
			index++;
	}

	serverBans[serverBansCount].ip = ip;
	serverBans[serverBansCount].subnet = mask;
	serverBans[serverBansCount].isexception = isexception;
	
	serverBansCount++;
	
	SV_WriteBans();

	Com_Printf("Added %s: %s/%d\n", isexception ? "ban exception" : "ban",
		   NET_AdrToString(&ip), mask);
}

/*
==================
SV_DelBanFromList

Remove a ban or an exception from the list.
==================
*/

static void SV_DelBanFromList(qboolean isexception)
{
	int index, count = 0, todel, mask;
	netadr_t ip;
	char *banstring;
	
	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}
	
	if(Cmd_Argc() != 2)
	{
		Com_Printf ("Usage: %s (ip[/subnet] | num)\n", Cmd_Argv(0));
		return;
	}

	banstring = Cmd_Argv(1);
	
	if(strchr(banstring, '.') || strchr(banstring, ':'))
	{
		serverBan_t *curban;
		
		if(SV_ParseCIDRNotation(&ip, &mask, banstring))
		{
			Com_Printf("Error: Invalid address %s\n", banstring);
			return;
		}
		
		index = 0;
		
		while(index < serverBansCount)
		{
			curban = &serverBans[index];
			
			if(curban->isexception == isexception		&&
			   curban->subnet >= mask 			&&
			   NET_CompareBaseAdrMask(&curban->ip, &ip, mask))
			{
				Com_Printf("Deleting %s %s/%d\n",
					   isexception ? "exception" : "ban",
					   NET_AdrToString(&curban->ip), curban->subnet);
					   
				SV_DelBanEntryFromList(index);
			}
			else
				index++;
		}
	}
	else
	{
		todel = atoi(Cmd_Argv(1));

		if(todel < 1 || todel > serverBansCount)
		{
			Com_Printf("Error: Invalid ban number given\n");
			return;
		}
	
		for(index = 0; index < serverBansCount; index++)
		{
			if(serverBans[index].isexception == isexception)
			{
				count++;
			
				if(count == todel)
				{
					Com_Printf("Deleting %s %s/%d\n",
					   isexception ? "exception" : "ban",
					   NET_AdrToString(&serverBans[index].ip), serverBans[index].subnet);

					SV_DelBanEntryFromList(index);

					break;
				}
			}
		}
	}
	
	SV_WriteBans();
}


/*
==================
SV_ListBans_f

List all bans and exceptions on console
==================
*/

static void SV_ListBans_f(void)
{
	int index, count;
	serverBan_t *ban;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}
	
	// List all bans
	for(index = count = 0; index < serverBansCount; index++)
	{
		ban = &serverBans[index];
		if(!ban->isexception)
		{
			count++;

			Com_Printf("Ban #%d: %s/%d\n", count,
				    NET_AdrToString(&ban->ip), ban->subnet);
		}
	}
	// List all exceptions
	for(index = count = 0; index < serverBansCount; index++)
	{
		ban = &serverBans[index];
		if(ban->isexception)
		{
			count++;

			Com_Printf("Except #%d: %s/%d\n", count,
				    NET_AdrToString(&ban->ip), ban->subnet);
		}
	}
}

/*
==================
SV_FlushBans_f

Delete all bans and exceptions.
==================
*/

static void SV_FlushBans_f(void)
{
	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	serverBansCount = 0;
	
	// empty the ban file.
	SV_WriteBans();
	
	Com_Printf("All bans and exceptions have been deleted.\n");
}

static void SV_BanAddr_f(void)
{
	SV_AddBanToList(qfalse);
}

static void SV_ExceptAddr_f(void)
{
	SV_AddBanToList(qtrue);
}

static void SV_BanDel_f(void)
{
	SV_DelBanFromList(qfalse);
}

static void SV_ExceptDel_f(void)
{
	SV_DelBanFromList(qtrue);
}

#endif // USE_BANS

/*
==================
SV_KickNum_f

Kick a user off of the server  FIXME: move to game
==================
*/
static void SV_KickNum_f( void ) {
	client_t	*cl;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( Cmd_Argc() != 2 ) {
		Com_Printf ("Usage: kicknum <client number>\n");
		return;
	}

	cl = SV_GetPlayerByNum();
	if ( !cl ) {
		return;
	}
	if( cl->netchan.remoteAddress.type == NA_LOOPBACK ) {
		SV_SendServerCommand(NULL, "print \"%s\"", "Cannot kick host player\n");
		return;
	}

	SV_DropClient( cl, "was kicked" );
	cl->lastPacketTime = svs.time;	// in case there is a funny zombie
}


/*
** SV_Strlen -- skips color escape codes
*/
int SV_Strlen( const char *str ) {
	const char *s = str;
	int count = 0;

	while ( *s ) {
		if ( Q_IsColorString( s ) ) {
			s += 2;
		} else {
			count++;
			s++;
		}
	}

	return count;
}


/*
==================
SV_CountQueue
==================
*/
static int SV_CountQueue( const client_t *cl ) {
	int count = 0;
	const netchan_buffer_t *buf = cl->netchan_start_queue;
	while ( buf ) {
		count++;
		buf = buf->next;
	}
	return count;
}


/*
================
SV_Status_f
================
*/
static void SV_Status_f( void ) {
	int i, j, l;
	const client_t *cl;
	const playerState_t *ps;
	const char *s;
	int max_namelength;
	int max_addrlength;
	char names[ MAX_CLIENTS * MAX_NAME_LENGTH ], *np[ MAX_CLIENTS ], nl[ MAX_CLIENTS ], *nc;
	char addrs[ MAX_CLIENTS * 48 ], *ap[ MAX_CLIENTS ], al[ MAX_CLIENTS ], *ac;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	max_namelength = 4; // strlen( "name" )
	max_addrlength = 7; // strlen( "address" )

	nc = names; *nc = '\0';
	ac = addrs; *ac = '\0';

	Com_Memset( np, 0, sizeof( np ) );
	Com_Memset( nl, 0, sizeof( nl ) );

	Com_Memset( ap, 0, sizeof( ap ) );
	Com_Memset( al, 0, sizeof( al ) );

	// first pass: save and determine max.legths of name/address fields
	for ( i = 0, cl = svs.clients ; i < sv_maxclients->integer ; i++, cl++ )
	{
		if ( cl->state == CS_FREE )
			continue;

		l = strlen( cl->name ) + 1;
		strcpy( nc, cl->name );
		np[ i ] = nc; nc += l;			// name pointer in name buffer
		nl[ i ] = SV_Strlen( cl->name );// name length without color sequences
		if ( nl[ i ] > max_namelength )
			max_namelength = nl[ i ];

		s = NET_AdrToString( &cl->netchan.remoteAddress );
		l = strlen( s ) + 1;
		strcpy( ac, s );
		ap[ i ] = ac; ac += l;			// address pointer in address buffer
		al[ i ] = l - 1;				// address length
		if ( al[ i ] > max_addrlength )
			max_addrlength = al[ i ];
	}

	Com_Printf( "map: %s\n", sv_mapname->string );

#if 0
	Com_Printf( "cl score ping name                        address                     rate\n" );
	Com_Printf( "-- ----- ---- --------------------------- --------------------------- -----\n" );
#else // variable-length fields
	Com_Printf( "cl score ping name" );
	for ( i = 0; i < max_namelength - 4; i++ )
		Com_Printf( " " );
	Com_Printf( " address" );
	for ( i = 0; i < max_addrlength - 7; i++ )
		Com_Printf( " " );
	Com_Printf( " rate  snpMs choked lastSz queue\n" );

	Com_Printf( "-- ----- ---- " );
	for ( i = 0; i < max_namelength; i++ )
		Com_Printf( "-" );
	Com_Printf( " " );
	for ( i = 0; i < max_addrlength; i++ )
		Com_Printf( "-" );
	Com_Printf( " ----- ----- ------ ------ -----\n" );
#endif

	for ( i = 0, cl = svs.clients ; i < sv_maxclients->integer ; i++, cl++ )
	{
		if ( cl->state == CS_FREE )
			continue;

		Com_Printf( "%2i ", i ); // id
		ps = SV_GameClientNum( i );
		Com_Printf( "%5i ", ps->persistant[PERS_SCORE] );

		// ping/status
		if ( cl->state == CS_PRIMED )
			Com_Printf( "PRM " );
		else if ( cl->state == CS_CONNECTED )
			Com_Printf( "CON " );
		else if ( cl->state == CS_ZOMBIE )
			Com_Printf( "ZMB " );
		else
			Com_Printf( "%4i ", cl->ping < 999 ? cl->ping : 999 );
	
		// variable-length name field
		s = np[ i ];
		Com_Printf( "%s", s );
		l = max_namelength - nl[ i ];
		for ( j = 0; j < l; j++ )
			Com_Printf( " " );

		// variable-length address field
		s = ap[ i ];
		Com_Printf( S_COLOR_WHITE " %s", s );
		l = max_addrlength - al[ i ];
		for ( j = 0; j < l; j++ )
			Com_Printf( " " );

		// rate + network health
		Com_Printf( " %5i %5i %6s %6i %5i\n",
			cl->rate,
			cl->snapshotMsec,
			cl->rateDelayed ? "CHOKED" : "ok",
			cl->netchan.lastSentSize,
			SV_CountQueue( cl ) );
	}

	Com_Printf( "\n" );
}


/*
==================
SV_ConSay_f
==================
*/
static void SV_ConSay_f( void ) {
	char	*p;
	char	text[1024];

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( Cmd_Argc () < 2 ) {
		return;
	}

	strcpy( text, "console: " );
	p = Cmd_ArgsFrom( 1 );

	if ( strlen( p ) > 1000 ) {
		return;
	}

	if ( *p == '"' ) {
		p++;
		p[strlen(p)-1] = '\0';
	}

	strcat( text, p );

	SV_SendServerCommand( NULL, "chat \"%s\"", text );
}


/*
==================
SV_ConTell_f
==================
*/
static void SV_ConTell_f( void ) {
	char	*p;
	char	text[1024];
	client_t	*cl;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( Cmd_Argc() < 3 ) {
		Com_Printf( "Usage: tell <client number> <text>\n" );
		return;
	}

	cl = SV_GetPlayerByNum();
	if ( !cl ) {
		return;
	}

	strcpy( text, S_COLOR_MAGENTA "console: " );
	p = Cmd_ArgsFrom( 2 );

	if ( strlen( p ) > 1000 ) {
		return;
	}

	if ( *p == '"' ) {
		p++;
		p[strlen(p)-1] = '\0';
	}

	strcat( text, p );

	Com_Printf( "%s\n", text );
	SV_SendServerCommand( cl, "chat \"%s\"", text );
}


/*
==================
SV_Heartbeat_f

Also called by SV_DropClient, SV_DirectConnect, and SV_SpawnServer
==================
*/
void SV_Heartbeat_f( void ) {
	svs.nextHeartbeatTime = svs.time;
}


/*
===========
SV_Serverinfo_f

Examine the serverinfo string
===========
*/
static void SV_Serverinfo_f( void ) {
	const char *info;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	Com_Printf ("Server info settings:\n");
	info = sv.configstrings[ CS_SERVERINFO ];
	if ( info ) {
		Info_Print( info );
	}
}


/*
===========
SV_Systeminfo_f

Examine the systeminfo string
===========
*/
static void SV_Systeminfo_f( void ) {
	const char *info;
	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}
	Com_Printf( "System info settings:\n" );
	info = sv.configstrings[ CS_SYSTEMINFO ];
	if ( info ) {
		Info_Print( info );
	}
}


/*
===========
SV_DumpUser_f

Examine all a users info strings
===========
*/
static void SV_DumpUser_f( void ) {
	client_t	*cl;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( Cmd_Argc() != 2 ) {
		Com_Printf ("Usage: dumpuser <userid>\n");
		return;
	}

	cl = SV_GetPlayerByHandle();
	if ( !cl ) {
		return;
	}

	Com_Printf( "userinfo\n" );
	Com_Printf( "--------\n" );
	Info_Print( cl->userinfo );
}


/*
=================
SV_KillServer
=================
*/
static void SV_KillServer_f( void ) {
	SV_Shutdown( "killserver" );
}

#ifdef USE_SERVER_DEMO
//===========================================================

/*
Start a server-side demo.

This does it all, create the file and adjust the demo-related
stuff in client_t.

This is mostly ripped from sv_client.c/SV_SendClientGameState
and cl_main.c/CL_Record_f.
*/
static void SVD_StartDemoFile(client_t *client, const char *path) {

    int             i, len;
    entityState_t   *base, nullstate;
    msg_t           msg;
    byte            buffer[MAX_MSGLEN];
    fileHandle_t    file;
#ifdef USE_URT_DEMO
    char            *s;
    int             v, size;
#endif

    Com_DPrintf("SVD_StartDemoFile\n");
    assert(!client->demo_recording);

    // create the demo file and write the necessary header
    file = FS_FOpenFileWrite(path);
    assert(file != 0);

    /* File_write_header_demo // ADD this fx */
    /* HOLBLIN  entete demo */
    #ifdef USE_URT_DEMO
        //@Barbatos: get the mod version from the server
        s = Cvar_VariableString("g_modversion");

        size = strlen(s);
        len = LittleLong(size);
        FS_Write(&len, 4, file);
        FS_Write(s, size, file);

        v = LittleLong(URT_PROTOCOL_VERSION);
        FS_Write (&v, 4, file);

        len = 0;
        len = LittleLong(len);
        FS_Write(&len, 4, file);
        FS_Write(&len, 4, file);
    #endif
    /* END HOLBLIN  entete demo */

    MSG_Init(&msg, buffer, sizeof(buffer));
    MSG_Bitstream(&msg); // XXX server code doesn't do this, client code does
    MSG_WriteLong(&msg, client->lastClientCommand); // TODO: or is it client->reliableSequence?
    MSG_WriteByte(&msg, svc_gamestate);
    MSG_WriteLong(&msg, client->reliableSequence);

    for (i = 0; i < MAX_CONFIGSTRINGS; i++) {
        if (sv.configstrings[i][0]) {
            MSG_WriteByte(&msg, svc_configstring);
            MSG_WriteShort(&msg, i);
            MSG_WriteBigString(&msg, sv.configstrings[i]);
        }
    }

    Com_Memset(&nullstate, 0, sizeof(nullstate));
    for (i = 0 ; i < MAX_GENTITIES; i++) {
        base = &sv.svEntities[i].baseline;
        if (!base->number) {
            continue;
        }
        MSG_WriteByte(&msg, svc_baseline);
        MSG_WriteDeltaEntity(&msg, &nullstate, base, qtrue);
    }

    MSG_WriteByte(&msg, svc_EOF);
    MSG_WriteLong(&msg, client - svs.clients);
    MSG_WriteLong(&msg, sv.checksumFeed);
    MSG_WriteByte(&msg, svc_EOF); // XXX server code doesn't do this, SV_Netchan_Transmit adds it!

    len = LittleLong(client->netchan.outgoingSequence - 1);
    FS_Write(&len, 4, file);

    len = LittleLong (msg.cursize);
    FS_Write(&len, 4, file);
    FS_Write(msg.data, msg.cursize, file);

    #ifdef USE_URT_DEMO
        // add size of packet in the end for backward play /* holblin */
        FS_Write(&len, 4, file);
    #endif

    FS_Flush(file);

    // adjust client_t to reflect demo started
    client->demo_recording = qtrue;
    client->demo_file = file;
    client->demo_waiting = qtrue;
    client->demo_backoff = 1;
    client->demo_deltas = 0;
}

/*
Write a message to a server-side demo file.
*/
void SVD_WriteDemoFile(const client_t *client, const msg_t *msg) {

    int len;
    msg_t cmsg;
    byte cbuf[MAX_MSGLEN];
    fileHandle_t file = client->demo_file;

    if (*(int *)msg->data == -1) { // TODO: do we need this?
        Com_DPrintf("Ignored connectionless packet, not written to demo!\n");
        return;
    }

    // TODO: we only copy because we want to add svc_EOF; can we add it and then
    // "back off" from it, thus avoiding the copy?
    MSG_Copy(&cmsg, cbuf, sizeof(cbuf), (msg_t*) msg);
    MSG_WriteByte(&cmsg, svc_EOF); // XXX server code doesn't do this, SV_Netchan_Transmit adds it!

    // TODO: the headerbytes stuff done in the client seems unnecessary
    // here because we get the packet *before* the netchan has it's way
    // with it; just not sure that's really true :-/

    len = LittleLong(client->netchan.outgoingSequence);
    FS_Write(&len, 4, file);

    len = LittleLong(cmsg.cursize);
    FS_Write(&len, 4, file);

    FS_Write(cmsg.data, cmsg.cursize, file); // XXX don't use len!

    #ifdef USE_URT_DEMO
        // add size of packet in the end for backward play /* holblin */
        FS_Write(&len, 4, file);
    #endif

    FS_Flush(file);
}

/*
Stop a server-side demo.

This finishes out the file and clears the demo-related stuff
in client_t again.
*/
static void SVD_StopDemoFile(client_t *client) {

    int marker = -1;
    fileHandle_t file = client->demo_file;

    Com_DPrintf("SVD_StopDemoFile\n");
    assert(client->demo_recording);

    // write the necessary trailer and close the demo file
    FS_Write(&marker, 4, file);
    FS_Write(&marker, 4, file);
    FS_Flush(file);
    FS_FCloseFile(file);

    // adjust client_t to reflect demo stopped
    client->demo_recording = qfalse;
    client->demo_file = -1;
    client->demo_waiting = qfalse;
    client->demo_backoff = 1;
    client->demo_deltas = 0;
}

/*
Clean up player name to be suitable as path name.
Similar to Q_CleanStr() but tweaked.
*/
static void SVD_CleanPlayerName(char *name) {

    char *src = name, *dst = name, c;

    while ((c = *src)) {
        // note Q_IsColorString(src++) won't work since it's a macro
        if (Q_IsColorString(src)) {
            src++;
        } else if (c == ':' || c == '\\' || c == '/' || c == '*' || c == '?') {
            *dst++ = '%';
        } else if (c > ' ' && c < 0x7f) {
            *dst++ = c;
        }
        src++;
    }

    *dst = '\0';

    if (strlen(name) == 0) {
        strcpy(name, "UnnamedPlayer");
    }
}

/*
Generate unique name for a new server demo file.
(We pretend there are no race conditions.)
*/
static void SV_NameServerDemo(char *filename, int length, const client_t *client, char *fn) {

    qtime_t time;
    char playername[32];
    char demoName[64]; //@Barbatos

    Com_DPrintf("SV_NameServerDemo\n");

    Com_RealTime(&time);
    Q_strncpyz(playername, client->name, sizeof(playername));
    SVD_CleanPlayerName(playername);

    if (fn != NULL) {

        Q_strncpyz(demoName, fn, sizeof(demoName));

        #ifdef USE_URT_DEMO
            Com_Printf(filename, length-1, "%s/%s.urtdemo", sv_demofolder->string, demoName );
            if (FS_FileExists(filename)) {
                Com_Printf(filename, length-1, "%s/%s_%d.urtdemo", sv_demofolder->string, demoName, Sys_Milliseconds() );
            }
        #else
            Com_Printf(filename, length-1, "%s/%s.dm_%d", sv_demofolder->string, demoName , PROTOCOL_VERSION );
            if (FS_FileExists(filename)) {
                Com_Printf(filename, length-1, "%s/%s_%d.dm_%d", sv_demofolder->string, demoName, Sys_Milliseconds() , PROTOCOL_VERSION );
            }
        #endif
    } else {
        #ifdef USE_URT_DEMO
            Com_Printf(
                filename, length-1, "%s/%.4d-%.2d-%.2d_%.2d-%.2d-%.2d_%s_%d.urtdemo",
                sv_demofolder->string, time.tm_year+1900, time.tm_mon + 1, time.tm_mday,
                time.tm_hour, time.tm_min, time.tm_sec,
                playername,
                Sys_Milliseconds()
            );
        #else
            Com_Printf(
                filename, length-1, "%s/%.4d-%.2d-%.2d_%.2d-%.2d-%.2d_%s_%d.dm_%d",
                sv_demofolder->string, time.tm_year+1900, time.tm_mon + 1, time.tm_mday,
                time.tm_hour, time.tm_min, time.tm_sec,
                playername,
                Sys_Milliseconds(),
                PROTOCOL_VERSION
            );
        #endif
        filename[length-1] = '\0';

        if (FS_FileExists(filename)) {
            filename[0] = 0;
            return;
        }
    }
}

static void SV_StartRecordOne(client_t *client, char *filename) {

    char demoName[ MAX_QPATH ];

    char path[MAX_OSPATH];

    Com_DPrintf("SV_StartRecordOne\n");

    if (client->demo_recording) {
        Com_Printf("startserverdemo: %s is already being recorded\n", client->name);
        return;
    }

    if (client->state != CS_ACTIVE) {
        Com_Printf("startserverdemo: %s is not active\n", client->name);
        return;
    }

    if (client->netchan.remoteAddress.type == NA_BOT) {
        Com_Printf("startserverdemo: %s is a bot\n", client->name);
        return;
    }
    Com_Printf("PATH: %s \n", path);
    if (filename == NULL) {
        qtime_t t;
        Com_RealTime( &t );
        sprintf( demoName, "ftw-%s-%02i%02i%02i-%02i%02i%02i", client->name, t.tm_year-100, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec );
    } else {
        sprintf( demoName, "%s", filename );
    }
    Com_sprintf( path, sizeof( path ), "demos/%s.urtdemo", demoName );
    SVD_StartDemoFile(client, path);

    if(sv_demonotice->string) {
        SV_SendServerCommand(client, "print \"%s\"\n", sv_demonotice->string);
    }

    Com_Printf("startserverdemo: recording %s to %s\n", client->name, path);
}

static void SV_StartRecordAll(void) {

    int slot;
    client_t *client;

    Com_DPrintf("SV_StartRecordAll\n");

    for (slot = 0, client = svs.clients; slot < sv_maxclients->integer; slot++, client++) {
        // filter here to avoid lots of bogus messages from SV_StartRecordOne()
        if (client->netchan.remoteAddress.type == NA_BOT
            || client->state != CS_ACTIVE
            || client->demo_recording) {
            continue;
        }
        SV_StartRecordOne(client, NULL);
    }
}

static void SV_StopRecordOne(client_t *client) {

    Com_DPrintf("SV_StopRecordOne\n");

    if (!client->demo_recording) {
        Com_Printf("stopserverdemo: %s is not being recorded\n", client->name);
        return;
    }

    if (client->state != CS_ACTIVE) { // disconnects are handled elsewhere
        Com_Printf("stopserverdemo: %s is not active\n", client->name);
        return;
    }

    if (client->netchan.remoteAddress.type == NA_BOT) {
        Com_Printf("stopserverdemo: %s is a bot\n", client->name);
        return;
    }

    SVD_StopDemoFile(client);
    Com_Printf("stopserverdemo: stopped recording %s\n", client->name);


}

static void SV_StopRecordAll(void) {

    int slot;
    client_t *client;

    Com_DPrintf("SV_StopRecordAll\n");

    for (slot=0, client=svs.clients; slot < sv_maxclients->integer; slot++, client++) {
        // filter here to avoid lots of bogus messages from SV_StopRecordOne()
        if (client->netchan.remoteAddress.type == NA_BOT
            || client->state != CS_ACTIVE // disconnects are handled elsewhere
            || !client->demo_recording) {
            continue;
        }
        SV_StopRecordOne(client);
    }

}

/*
==================
SV_StartServerDemo_f

Record a server-side demo for given player/slot. The demo
will be called "YYYY-MM-DD_hh-mm-ss_playername_id.urtdemo",
in the "demos" directory under your game directory. Note
that "startserverdemo all" will start demos for all players
currently in the server. Players who join later require a
new "startserverdemo" command. If you are already recording
demos for some players, another "startserverdemo all" will
start new demos only for players not already recording. Note
that bots will never be recorded, not even if "all" is given.
The server-side demos will stop when "stopserverdemo" is issued
or when the server restarts for any reason (such as a new map
loading).
==================
*/
static void SV_StartServerDemo_f(void) {

    client_t *client;

    Com_DPrintf("SV_StartServerDemo_f\n");

    if (!com_sv_running->integer) {
        Com_Printf("startserverdemo: Server not running\n");
        return;
    }

    if (Cmd_Argc() < 2) {
        Com_Printf("Usage: startserverdemo <client-or-all> [<optional-demo-name>]\n");
        return;
    }

    if (!Q_stricmp(Cmd_Argv(1), "all")) {

        SV_StartRecordAll();

    } else {

        client = SV_GetPlayerByHandle();
        if (!client) {
            return;
        }

        if (Cmd_Argc() > 2) {
            SV_StartRecordOne(client, Cmd_Argv(2));
        } else {
            SV_StartRecordOne(client, NULL);
        }

    }
}

/*
==================
SV_StopServerDemo_f

Stop a server-side demo for given player/slot. Note that
"stopserverdemo all" will stop demos for all players in
the server.
==================
*/
static void SV_StopServerDemo_f(void)
{
	client_t *client;

	Com_DPrintf("SV_StopServerDemo_f\n");

	if (!com_sv_running->integer) {
		Com_Printf("stopserverdemo: Server not running\n");
		return;
	}

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: stopserverdemo <client-or-all>\n");
		return;
	}

	if (!Q_stricmp(Cmd_Argv(1), "all")) {

		SV_StopRecordAll();
	}
	else {

	    client = SV_GetPlayerByHandle();

	    if (!client) {
	        return;
	    }

		SV_StopRecordOne(client);
	}

}
#endif


/*
=================
SV_Locations
=================
*/
static void SV_Locations_f( void ) {

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( !sv_clientTLD->integer ) {
		Com_Printf( "Disabled on this server.\n" );
		return;
	}

	SV_PrintLocations_f( NULL );
}

#ifdef USE_FTWGL
/*
=================
SV_ClientScreenshot_f
=================
*/
static void SV_ClientScreenshot_f( void ) {
    // make sure server is running
    if ( !com_sv_running->integer ) {
        Com_Printf( "Server is not running.\n" );
        return;
    }

    if ( Cmd_Argc() == 1 ) {
        SV_SendServerCommand( NULL, "clientScreenshot");
    } else if ( Cmd_Argc() == 2 ) {
        SV_SendServerCommand( NULL, "clientScreenshot %s", Cmd_Argv(1) );
    }
}
#endif

//===========================================================

/*
==================
SV_CompleteMapName
==================
*/
static void SV_CompleteMapName( char *args, int argNum ) {
	if ( argNum == 2 ) 	{
		if ( sv_pure->integer ) {
			Field_CompleteFilename( "maps", "bsp", qtrue, FS_MATCH_PK3s | FS_MATCH_STICK );
		} else {
			Field_CompleteFilename( "maps", "bsp", qtrue, FS_MATCH_ANY | FS_MATCH_STICK );
		}
	}
}

#ifdef USE_MV

#define MV_CACHE_FILE "demos/mv-cache.dat"
#define MV_FILTER "/mv-*-*.dm_71"

int mv_record_count;
int mv_total_size;
int mv_insert_index;

typedef struct {
    char name[28];
    int  size;
} mv_file_record_t;
mv_file_record_t mvrecords[ MAX_MV_FILES ];


static void SV_TrimRecords( int add_count, int add_size );

static void SV_CreateRecordCache( void )
{
    mv_file_record_t *mvr;
    int nfiles, i, len, size;
    char **list, *name;

    //Com_Printf( S_COLOR_CYAN "...creating record cache\n" );

    mv_insert_index = mv_record_count;
    mv_record_count = 0;
    mv_total_size = 0;

    Com_Memset( mvrecords, 0, sizeof( mvrecords ) );
    list = FS_Home_ListFilteredFiles( "demos", ".dm_71", MV_FILTER, &nfiles );
    for ( i = 0; i < nfiles; i++ )
    {
        name = list[i];
        if ( name[0] == '\\' || name[0] == '/' )
            name++;
        len = (int)strlen( list[i] );
        if ( len < 22 || len >= sizeof( mvr->name ) )
            continue;

        size = FS_Home_FileSize( va( "demos/%s", name ) );
        if ( size <= 0 )
            continue;

        mvr = &mvrecords[ mv_record_count ];
        mvr->size = PAD( size, 4096 );
        strcpy( mvr->name, name );

        mv_total_size += mvr->size;

        mv_record_count++;

        if ( mv_record_count >= MAX_MV_FILES )
            break;
    }
    FS_FreeFileList( list );

    mv_insert_index = mv_record_count;
    mv_insert_index &= (MAX_MV_FILES-1);
}


/*
==================
SV_LoadRecordCache
==================
*/
void SV_LoadRecordCache( void )
{
    mv_file_record_t *mvr;
    fileHandle_t fh;
    int fileSize, i;

    mv_record_count = 0;
    mv_insert_index = 0;
    mv_total_size = 0;

    fileSize = FS_Home_FOpenFileRead( MV_CACHE_FILE, &fh );
    if ( fh == FS_INVALID_HANDLE )
    {
        SV_CreateRecordCache();
        SV_TrimRecords( 0, 0 );
        return;
    }

    if ( fileSize != sizeof( mvrecords ) )
    {
        FS_FCloseFile( fh );
        SV_CreateRecordCache();
        SV_TrimRecords( 0, 0 );
        return;
    }

    //Com_Printf( S_COLOR_CYAN "...reading record cache from file\n" );
    FS_Read( mvrecords, sizeof( mvrecords ), fh );
    FS_FCloseFile( fh );

    mvr = mvrecords;
    for ( i = 0; i < MAX_MV_FILES; i++, mvr++ )
    {
        if ( !mvr->name[0] || mvr->size <= 0 )
            break;
        mv_total_size += PAD( mvr->size, 4096 );
    }

    mv_record_count = i;
    mv_insert_index = i & (MAX_MV_FILES-1);

    SV_TrimRecords( 0, 0 );

    //Com_Printf( S_COLOR_CYAN "cache: %i items, %i bytes\n", mv_record_count, mv_total_size );
}


void SV_SaveRecordCache( void )
{
    mv_file_record_t z;
    fileHandle_t fh;
    int start;
    int n, count, pad;

    fh = FS_FOpenFileWrite( MV_CACHE_FILE );
    if ( fh == FS_INVALID_HANDLE )
        return;

    count = mv_record_count;
    if ( count > MAX_MV_FILES )
        count = MAX_MV_FILES;

    pad = MAX_MV_FILES - count;
    Com_Memset( &z, 0, sizeof( z ) );

    start = ( mv_insert_index - count ) & (MAX_MV_FILES-1);
    //Com_Printf( S_COLOR_CYAN "writing %i cache records from %i\n", count, start );

    while ( count > 0 )
    {
        n = count;
        if ( start + n > MAX_MV_FILES )
            n = MAX_MV_FILES - start;

        FS_Write( &mvrecords[ start ], sizeof( mv_file_record_t ) * n, fh );
        start = ( start + n ) & (MAX_MV_FILES-1);
        count -= n;
    }

    for ( n = 0; n < pad; n++ )
    {
        FS_Write( &z, sizeof( z ), fh );
    }

    FS_FCloseFile( fh );
}


static void SV_TrimRecords( int add_count, int add_size )
{
    int max_count;
    mv_file_record_t *mvr;

    //Com_Printf( S_COLOR_YELLOW "trim records count:%i size%i\n", mv_record_count, mv_total_size );

    // by file count
    if (  sv_mvFileCount->integer > 0 || mv_record_count + add_count > MAX_MV_FILES )
    {
        if ( sv_mvFileCount->integer > 0 && sv_mvFileCount->integer < MAX_MV_FILES )
            max_count = sv_mvFileCount->integer;
        else
            max_count = MAX_MV_FILES;

        while ( mv_record_count + add_count > max_count && mv_record_count > 0 )
        {
            mvr = mvrecords + (( mv_insert_index - mv_record_count ) & (MAX_MV_FILES-1));
            //Com_Printf( S_COLOR_RED "trim.count %i %s\n", mvr->size, mvr->name );
            if ( mvr->name[0] )
            {
                FS_HomeRemove( va( "demos/%s", mvr->name ) );
                mv_total_size -= mvr->size;
            }
            Com_Memset( mvr, 0, sizeof( *mvr ) );
            mv_record_count--;
        }
    }

    // by total size
    if ( sv_mvFolderSize->integer > 0 )
    {
        while ( (mv_total_size + add_size) > (sv_mvFolderSize->integer * 1024 * 1024) && mv_record_count > 0 )
        {
            mvr = mvrecords + (( mv_insert_index - mv_record_count ) & (MAX_MV_FILES-1));
            //Com_Printf( S_COLOR_RED "trim.size %i %s\n", mvr->size, mvr->name );
            if ( mvr->name[0] )
            {
                FS_HomeRemove( va( "demos/%s", mvr->name ) );
                mv_total_size -= mvr->size;
            }
            Com_Memset( mvr, 0, sizeof( *mvr ) );
            mv_record_count--;
        }
    }
}


static void SV_InsertFileRecord( const char *name )
{
    mv_file_record_t *mvr;
    int size, len;

    if ( !Q_stricmpn( name, "demos/", 6 ) )
        name += 5;
    else
        return;

    if ( !Com_FilterPath( MV_FILTER, name ) ) {
        //Com_Printf( "filtered %s\n", name );
        return;
    }
    name++; // skip '/'

    len = strlen( name );
    if ( len < 22 || len >= sizeof( mvr->name ) ) {
        //Com_Printf( "filtered1 %s\n", name );
        return;
    }

    size = FS_Home_FileSize( va( "demos/%s", name ) );
    if ( size <= 0 ) {
        return;
    }

    size = PAD( size, 4096 );

    SV_TrimRecords( 1, size );

    mvr = &mvrecords[ mv_insert_index ];
    strcpy( mvr->name, name );
    mvr->size = size;

    mv_total_size += mvr->size;
    mv_insert_index = ( mv_insert_index + 1 ) & (MAX_MV_FILES-1);

    if ( mv_record_count < MAX_MV_FILES )
        mv_record_count++;

    //Com_Printf( S_COLOR_CYAN "Record index %i, count %i\n", mv_insert_index, mv_record_count );
    //SV_SaveRecordCache();
}



/*
==================
SV_SetTargetClient
==================
*/
void SV_SetTargetClient( int clientNum )
{
    sv_lastAck = 0; // force to fetch latest target' reliable acknowledge
    sv_lastClientSeq = 0;
    sv_demoClientID = clientNum;
}


/*
==================
SV_ForwardServerCommands
==================
*/
void SV_ForwardServerCommands( client_t *recorder /*, const client_t *client */ )
{
    const client_t *client;
    const char *cmd;
    int src_index;
    int	dst_index;
    int i;

    if ( sv_demoClientID < 0 )
        return;

    client = svs.clients + sv_demoClientID;



    // FIXME: track reliableSequence globally?
    if ( !sv_lastAck ) {
        sv_lastAck = client->reliableAcknowledge;
    }

    //if ( client->reliableAcknowledge >= client->reliableSequence )
    if ( sv_lastAck >= client->reliableSequence )
        return; // nothing to send

    //for ( i = client->reliableAcknowledge + 1 ; i <= client->reliableSequence ; i++ ) {
    for ( i = sv_lastAck + 1 ; i <= client->reliableSequence ; i++ ) {
        src_index = i & ( MAX_RELIABLE_COMMANDS - 1 );
        cmd = client->reliableCommands[ src_index ];
        // filter commands here:
        if ( strncmp( cmd, "tell ", 5 ) == 0 ) // TODO: other commands
            continue;
        dst_index = ++recorder->reliableSequence & ( MAX_RELIABLE_COMMANDS - 1 );
        Q_strncpyz( recorder->reliableCommands[ dst_index ], cmd, sizeof( recorder->reliableCommands[ dst_index ] ) );
    }

    sv_lastAck = client->reliableSequence;
}


/*
==================
SV_MultiViewRecord_f
==================
*/
static void SV_MultiViewRecord_f( void )
{
    entityState_t *base, nullstate;

    client_t	*recorder;	// recorder slot
    byte		msgData[ MAX_MSGLEN_BUF ];
    msg_t		msg;

    char demoName[ MAX_QPATH ];
    char name[ MAX_QPATH ];
    const char *s;
    int i, cid, len;

    if ( Cmd_Argc() > 2 ) {
        Com_Printf( "usage: mvrecord [filename]\n" );
        return;
    }

    if ( sv_demoFile != FS_INVALID_HANDLE ) {
        Com_Printf( "Already recording multiview.\n" );
        return;
    }

    if ( sv.state != SS_GAME || !svs.clients ) {
        Com_Printf( "Game is not running.\n" );
        return;
    }

    cid = SV_FindActiveClient( qfalse /* checkCommands */, -1 /* skipClientNum */, 0 /* minActive */ );

    if ( cid < 0 ) {
        Com_Printf( "No active clients connected.\n" );
        return;
    }

    if ( Cmd_Argc() == 2 ) {
        s = Cmd_Argv( 1 );
        Q_strncpyz( demoName, s, sizeof( demoName ) );
        Com_sprintf( name, sizeof( name ), "demos/%s.%s%d", demoName, DEMOEXT, NEW_PROTOCOL_VERSION );
    } else {
        qtime_t t;

        Com_RealTime( &t );
        // name in format mv-YYMMDD-HHmmSS.dm_71 (23+4) = 27, ok
        sprintf( demoName, "mv-%02i%02i%02i-%02i%02i%02i", t.tm_year-100, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec );
        Com_sprintf( name, sizeof( name ), "demos/%s.%s%d", demoName, DEMOEXT, NEW_PROTOCOL_VERSION );

        i = 0;
        // try with suffix: mv-YYMMDD-HHmmSS-[0..999].dm_71
        while ( FS_FileExists( name ) && i++ <= 999 )
            Com_sprintf( name, sizeof( name ), "demos/%s-%i.%s%d", demoName, i, DEMOEXT, NEW_PROTOCOL_VERSION );
    }

    strcpy( sv_demoFileNameLast, name );
    strcpy( sv_demoFileName, name );
    // add .tmp to server-side demos so we can rename them later
    Q_strcat( sv_demoFileName, sizeof( sv_demoFileName ), ".tmp" );

    Com_Printf( S_COLOR_CYAN "start recording to %s using primary client id %i.\n", sv_demoFileName, cid );
    sv_demoFile = FS_FOpenFileWrite( sv_demoFileName );

    if ( sv_demoFile == FS_INVALID_HANDLE ) {
        Com_Printf( "ERROR: couldn't open %s.\n", sv_demoFileName );
        sv_demoFileName[0] = '\0';
        sv_demoFileNameLast[0] = '\0';
        return;
    }

    recorder = svs.clients + sv_maxclients->integer; // reserved recorder slot

    SV_SetTargetClient( cid );

    Com_Memset( recorder, 0, sizeof( *recorder ) );

    recorder->multiview.protocol = MV_PROTOCOL_VERSION;
    recorder->multiview.recorder = qtrue;
    recorder->state = CS_ACTIVE;

    recorder->deltaMessage = -1; // reset delta encoding in next snapshot
    recorder->netchan.outgoingSequence = 1;
    recorder->netchan.remoteAddress.type = NA_LOOPBACK;

    // empty command buffer
    recorder->reliableSequence = 0;
    recorder->reliableAcknowledge = 0;

    recorder->lastClientCommand = 1;

    MSG_Init( &msg, msgData, MAX_MSGLEN );
    MSG_Bitstream( &msg );

    // NOTE, MRE: all server->client messages now acknowledge
    MSG_WriteLong( &msg, recorder->lastClientCommand );

    SV_UpdateServerCommandsToClient( recorder, &msg );

#ifdef USE_MV_ZCMD
    // we are resetting delta sequence after gamestate
	recorder->multiview.z.deltaSeq = 0;
#endif

    MSG_WriteByte( &msg, svc_gamestate );

    // all future zcmds must have reliableSequence greater than this
    MSG_WriteLong( &msg, recorder->reliableSequence );

    // write the configstrings
    for ( i = 0 ; i < MAX_CONFIGSTRINGS ; i++ ) {
        if ( sv.configstrings[i][0] ) {
            MSG_WriteByte( &msg, svc_configstring );
            MSG_WriteShort( &msg, i );
            MSG_WriteBigString( &msg, sv.configstrings[i] );
        }
    }

    // write the baselines
    Com_Memset( &nullstate, 0, sizeof( nullstate ) );
    for ( i = 0 ; i < MAX_GENTITIES; i++ ) {
        base = &sv.svEntities[ i ].baseline;
        if ( !sv.baselineUsed[ i ] ) {
            continue;
        }
        MSG_WriteByte( &msg, svc_baseline );
        MSG_WriteDeltaEntity( &msg, &nullstate, base, qtrue );
    }

    MSG_WriteByte( &msg, svc_EOF );

    MSG_WriteLong( &msg, sv_demoClientID ); // selected client id

    // write the checksum feed
    MSG_WriteLong( &msg, sv.checksumFeed );

    // finalize packet
    MSG_WriteByte( &msg, svc_EOF );

    len = LittleLong( recorder->netchan.outgoingSequence - 1 );
    FS_Write( &len, 4, sv_demoFile );

    // data size
    len = LittleLong( msg.cursize );
    FS_Write( &len, 4, sv_demoFile );

    // data
    FS_Write( msg.data, msg.cursize, sv_demoFile );
}


/*
==================
SV_MultiViewStopRecord_f
==================
*/
void SV_MultiViewStopRecord_f( void )
{
    client_t *recorder;

    if ( !svs.clients )
        return;

    recorder = svs.clients + sv_maxclients->integer; // recorder slot

    if ( sv_demoFile != FS_INVALID_HANDLE ) {

        FS_FCloseFile( sv_demoFile );
        sv_demoFile = FS_INVALID_HANDLE;

        // rename final file
        if ( sv_demoFileNameLast[0] && sv_demoFileName[0] ) {
            FS_Rename( sv_demoFileName, sv_demoFileNameLast );
        }

        // store in cache
        SV_InsertFileRecord( sv_demoFileNameLast );

        Com_Printf( S_COLOR_CYAN "stopped multiview recording.\n" );
        return;
    }

    sv_demoFileName[0] = '\0';
    sv_demoFileNameLast[0] = '\0';

    SV_SetTargetClient( -1 );
#if 1
    Com_Memset( recorder, 0, sizeof( *recorder ) );
#else
    recorder->netchan.outgoingSequence = 0;
	recorder->multiview.protocol = 0;
	recorder->multiview.recorder = qfalse;
	recorder->state = CS_FREE;
#endif
}


/*
==================
SV_TrackDisconnect
==================
*/
void SV_TrackDisconnect( int clientNum )
{
    int cid;

    svs.clients[ clientNum ].multiview.scoreQueryTime = 0;

    if ( clientNum == sv_demoClientID ) {
        cid = SV_FindActiveClient( qfalse, sv_demoClientID, 0 ); // TODO: count sv_autoRecord?
        if ( cid < 0 ) {
            SV_MultiViewStopRecord_f();
            return;
        }
        Com_DPrintf( "mvrecorder: switch primary client id to %i\n", cid );
        SV_SetTargetClient( cid );
    }
}
#endif // USE_MV

/*
==================
SV_AddOperatorCommands
==================
*/
void SV_AddOperatorCommands( void ) {
	static qboolean	initialized;

	if ( initialized ) {
		return;
	}
	initialized = qtrue;

	Cmd_AddCommand ("heartbeat", SV_Heartbeat_f);
    Cmd_SetDescription( "heartbeat", "Send a manual heartbeat to the master servers\nusage: heartbeat" );

    Cmd_AddCommand ("kick", SV_Kick_f);
    Cmd_SetDescription( "kick", "Kick the player with the given name off the server\nusag: kick <playername>" );

#ifndef STANDALONE
#ifdef USE_BANS
	if(!Cvar_VariableIntegerValue("com_standalone"))
	{
		Cmd_AddCommand ("banUser", SV_Ban_f);
        Cmd_SetDescription( "banUser", "Ban a client by their player name\nusage: banUser <playername>" );

		Cmd_AddCommand ("banClient", SV_BanNum_f);
        Cmd_SetDescription( "banClient", "Ban a client by their slot number\nusage: banClient <slot #>" );
	}
#endif
#endif
	Cmd_AddCommand ("clientkick", SV_KickNum_f);
    Cmd_SetDescription( "clientkick", "Kick a client by slot number used\nusage: clientkick <slot #>" );

    Cmd_AddCommand ("status", SV_Status_f);
    Cmd_SetDescription( "status", "Show status of the currently connected server\nusage: status" );

    Cmd_AddCommand ("dumpuser", SV_DumpUser_f);
    Cmd_SetDescription( "dumpuser", "Display user info (handicap, model/color, rail color, more)\nusage: dumpuser <playername>");

    Cmd_AddCommand ("map_restart", SV_MapRestart_f);
    Cmd_SetDescription( "map_restart", "Resets the game on the same map\nusage: map_restart" );

    Cmd_AddCommand ("sectorlist", SV_SectorList_f);
    Cmd_SetDescription( "sectorlist", "Lists sectors and number of entities in each on the currently loaded map\nusage: sectorlist" );

    Cmd_AddCommand ("map", SV_Map_f);
	Cmd_SetCommandCompletionFunc( "map", SV_CompleteMapName );
    Cmd_SetDescription( "map", "Loads specified map\nusage: map <mapname>" );

#ifndef PRE_RELEASE_DEMO
	Cmd_AddCommand ("devmap", SV_Map_f);
	Cmd_SetCommandCompletionFunc( "devmap", SV_CompleteMapName );
    Cmd_SetDescription( "devmap", "Load maps in development mode\nusage: devmap <mapname>" );

    Cmd_AddCommand ("spmap", SV_Map_f);
	Cmd_SetCommandCompletionFunc( "spmap", SV_CompleteMapName );
    Cmd_SetDescription( "spmap", "Load map in single player mode with bots\nusage: spmap <mapname>" );

    Cmd_AddCommand ("spdevmap", SV_Map_f);
	Cmd_SetCommandCompletionFunc( "spdevmap", SV_CompleteMapName );
    Cmd_SetDescription( "spdevmap", "Load maps in development single player mode\nusage: spdevmap <mapname>" );

#endif
	Cmd_AddCommand ("killserver", SV_KillServer_f);
    Cmd_SetDescription( "killserver", "Stops server from running\nusage: killserver" );

#ifdef USE_BANS	
	Cmd_AddCommand("rehashbans", SV_RehashBans_f);
    Cmd_SetDescription( "rehashbans", "Hash ban list for faster lookup\nusage: rehashbans" );

	Cmd_AddCommand("listbans", SV_ListBans_f);
    Cmd_SetDescription( "listbans", "List all ban rules\nusage: listbans" );

    Cmd_AddCommand("banaddr", SV_BanAddr_f);
    Cmd_SetDescription( "banaddr", "Ban a specific IP address\nusage: banaddr <address>" );

	Cmd_AddCommand("exceptaddr", SV_ExceptAddr_f);
    Cmd_SetDescription( "exceptaddr", "Allow a specific IP address to be excluded from ban\nusage: exceptaddr <address>" );

    Cmd_AddCommand("bandel", SV_BanDel_f);
    Cmd_SetDescription( "bandel", "Remove a specific ban\nusage: bandel <ban>" );

	Cmd_AddCommand("exceptdel", SV_ExceptDel_f);
	Cmd_SetDescription( "exceptdel", "Allow a specific ban to be excluded\nusage: exceptdel <ban>" );

	Cmd_AddCommand("flushbans", SV_FlushBans_f);
	Cmd_SetDescription( "flushbans", "Clear all bans\nusage: flushbans" );
#endif
	Cmd_AddCommand( "filter", SV_AddFilter_f );
    Cmd_SetDescription( "filter", "Filter a specific client from connecting\nusage: %s <id> [key1] [key2]" );

	Cmd_AddCommand( "filtercmd", SV_AddFilterCmd_f );
    Cmd_SetDescription( "filtercmd", "Run a command while filtering\nusage: %s <filter format string>" );
#ifdef USE_MV
	Cmd_AddCommand( "mvrecord", SV_MultiViewRecord_f );
    Cmd_SetDescription( "mvrecord", "Start a multiview recording\nusage: mvrecord <filename>" );

	Cmd_AddCommand( "mvstoprecord", SV_MultiViewStopRecord_f );
    Cmd_SetDescription( "mvstoprecord", "Stop a multiview recording\nusage: mvstoprecord" );
#endif

#ifdef USE_SERVER_DEMO
    Cmd_AddCommand("startserverdemo", SV_StartServerDemo_f);
    Cmd_SetDescription( "startserverdemo", "Start server side recording" );

    Cmd_AddCommand("stopserverdemo", SV_StopServerDemo_f);
    Cmd_SetDescription( "stopserverdemo", "Stop a server side recording" );
#endif
}


/*
==================
SV_RemoveOperatorCommands
==================
*/
void SV_RemoveOperatorCommands( void ) {
#if 0
	// removing these won't let the server start again
	Cmd_RemoveCommand ("heartbeat");
	Cmd_RemoveCommand ("kick");
	Cmd_RemoveCommand ("clientkick");
	Cmd_RemoveCommand ("banUser");
	Cmd_RemoveCommand ("banClient");
	Cmd_RemoveCommand ("status");
	Cmd_RemoveCommand ("dumpuser");
	Cmd_RemoveCommand ("map_restart");
	Cmd_RemoveCommand ("sectorlist");
#endif
}


void SV_AddDedicatedCommands( void )
{
	Cmd_AddCommand( "serverinfo", SV_Serverinfo_f );
    Cmd_SetDescription( "serverinfo", "Gives information about local server from the console of that server\nusage: serverinfo" );

    Cmd_AddCommand( "systeminfo", SV_Systeminfo_f );
    Cmd_SetDescription( "systeminfo", "Returns values for g_syncronousclients, sv_serverid, and timescale\nusage: systeminfo" );

    Cmd_AddCommand( "tell", SV_ConTell_f );
    Cmd_SetDescription( "tell", "Say something to an individual on the server\nusage: tell <playername> <text>" );

    Cmd_AddCommand( "say", SV_ConSay_f );
    Cmd_SetDescription( "say", "Say something to everyone on the server\nusage: svsay <text>");

    Cmd_AddCommand( "locations", SV_Locations_f );
    Cmd_SetDescription( "locations", "Display a list of client locations from their country setting\nusage: locations" );

#ifdef USE_FTWGL
    Cmd_AddCommand("clientScreenshot", SV_ClientScreenshot_f );
#endif
}


void SV_RemoveDedicatedCommands( void )
{
	Cmd_RemoveCommand( "serverinfo" );
	Cmd_RemoveCommand( "systeminfo" );
	Cmd_RemoveCommand( "tell" );
	Cmd_RemoveCommand( "say" );
	Cmd_RemoveCommand( "locations" );
}
