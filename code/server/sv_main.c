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

serverStatic_t	svs;				// persistant server info
server_t		sv;					// local server
vm_t			*gvm = NULL;		// game virtual machine

cvar_t	*sv_fps;				// time rate for running non-clients
cvar_t	*sv_gameHz;				// rate at which level.time advances, independent of sv_fps
cvar_t	*sv_snapshotFps;		// max snapshot send rate, independent of sv_fps
cvar_t	*sv_busyWait;			// spin last N ms before frame instead of sleeping, for precise timing at high sv_fps
cvar_t	*sv_pmoveMsec;			// max physics step size, enforces consistent movement regardless of client framerate
cvar_t	*sv_smoothClients;		// TR_LINEAR trajectory trick for smoother client rendering
cvar_t	*sv_antiwarp;			// engine-side antiwarp (replaces QVM g_antiwarp)
cvar_t	*sv_antiwarpTol;		// antiwarp tolerance in ms (0=auto)
cvar_t	*sv_antiwarpExtra;		// extrapolation window for mode 2 (ms, 0=auto=awTol)
cvar_t	*sv_antiwarpDecay;		// decay duration for mode 2 (ms)
cvar_t	*sv_bufferMs;			// position delay via ring buffer (ms, 0=off, <0=auto)
cvar_t	*sv_velSmooth;			// velocity smoothing window (ms, 0=off)
cvar_t	*sv_extrapolate;		// legacy position prediction (0=off)
cvar_t	*sv_timeout;			// seconds without any message
cvar_t	*sv_zombietime;			// seconds to sink messages after disconnect
cvar_t	*sv_rconPassword;		// password for remote server commands
cvar_t	*sv_privatePassword;	// password for the privateClient slots
cvar_t	*sv_allowDownload;
cvar_t	*sv_maxclients;
cvar_t	*sv_maxclientsPerIP;
cvar_t	*sv_clientTLD;

cvar_t	*sv_privateClients;		// number of clients reserved for password
cvar_t	*sv_hostname;
cvar_t	*sv_master[MAX_MASTER_SERVERS];		// master server ip address
cvar_t	*sv_reconnectlimit;		// minimum seconds between connect messages
cvar_t	*sv_padPackets;			// add nop bytes to messages
cvar_t	*sv_killserver;			// menu system can set to 1 to shut server down
cvar_t	*sv_mapname;
cvar_t	*sv_mapChecksum;
cvar_t	*sv_referencedPakNames;
cvar_t	*sv_serverid;
cvar_t	*sv_minRate;
cvar_t	*sv_maxRate;
cvar_t	*sv_dlRate;
cvar_t	*sv_gametype;
cvar_t	*sv_pure;
cvar_t	*sv_floodProtect;
cvar_t	*sv_lanForceRate; // dedicated 1 (LAN) server forces local client rates to 99999 (bug #491)
cvar_t	*sv_allowClientAdaptiveTiming;

cvar_t *sv_levelTimeReset;
cvar_t *sv_filter;

#ifdef USE_AUTH
cvar_t	*sv_authServerIP;
cvar_t	*sv_auth_engine;
#endif

#ifdef USE_SERVER_DEMO
cvar_t	*sv_demonotice;
cvar_t	*sv_demofolder;
#endif

#ifdef USE_MV
cvar_t	*sv_mvClients;
cvar_t	*sv_mvPassword;
#endif

#ifdef USE_BANS
cvar_t	*sv_banFile;
serverBan_t serverBans[SERVER_MAXBANS];
int serverBansCount = 0;
#endif

/*
=============================================================================

EVENT MESSAGES

=============================================================================
*/

/*
===============
SV_ExpandNewlines

Converts newlines to "\n" so a line prints nicer
===============
*/
static const char *SV_ExpandNewlines( const char *in ) {
	static char string[MAX_STRING_CHARS*2];
	int		l;

	l = 0;
	while ( *in && l < sizeof(string) - 3 ) {
		if ( *in == '\n' ) {
			string[l++] = '\\';
			string[l++] = 'n';
		} else {
			string[l++] = *in;
		}
		in++;
	}
	string[l] = '\0';

	return string;
}


/*
======================
SV_ReplacePendingServerCommands

FIXME: This is ugly
======================
*/
#if 0 // unused
static int SV_ReplacePendingServerCommands( client_t *client, const char *cmd ) {
	int i, index, csnum1, csnum2;

	for ( i = client->reliableSent+1; i <= client->reliableSequence; i++ ) {
		index = i & ( MAX_RELIABLE_COMMANDS - 1 );
		//
		if ( !Q_strncmp(cmd, client->reliableCommands[ index ], strlen("cs")) ) {
			sscanf(cmd, "cs %i", &csnum1);
			sscanf(client->reliableCommands[ index ], "cs %i", &csnum2);
			if ( csnum1 == csnum2 ) {
				Q_strncpyz( client->reliableCommands[ index ], cmd, sizeof( client->reliableCommands[ index ] ) );
				/*
				if ( client->netchan.remoteAddress.type != NA_BOT ) {
					Com_Printf( "WARNING: client %i removed double pending config string %i: %s\n", client-svs.clients, csnum1, cmd );
				}
				*/
				return qtrue;
			}
		}
	}
	return qfalse;
}
#endif


/*
======================
SV_AddServerCommand

The given command will be transmitted to the client, and is guaranteed to
not have future snapshot_t executed before it is executed
======================
*/
void SV_AddServerCommand( client_t *client, const char *cmd ) {
	int		index, i, n;

	// this is very ugly but it's also a waste to for instance send multiple config string updates
	// for the same config string index in one snapshot
//	if ( SV_ReplacePendingServerCommands( client, cmd ) ) {
//		return;
//	}

	// do not send commands until the gamestate has been sent
	if ( client->state < CS_PRIMED )
		return;

	client->reliableSequence++;
	// if we would be losing an old command that hasn't been acknowledged,
	// we must drop the connection
	// we check == instead of >= so a broadcast print added by SV_DropClient()
	// doesn't cause a recursive drop client
	if ( client->reliableSequence - client->reliableAcknowledge == MAX_RELIABLE_COMMANDS + 1 ) {
		// If the client has not sent a packet for several seconds they have
		// disconnected ungracefully (crash / Alt-F4 / OS-level quit without a
		// clean disconnect packet).  Drop them silently so the slot is freed
		// immediately and other players see a normal "disconnected" message
		// rather than the confusing "Server command overflow" error.
		if ( svs.time - client->lastPacketTime > 5000 ) {
			SV_DropClient( client, "disconnected" );
			return;
		}
		Com_Printf( "===== pending server commands =====\n" );
		n = client->reliableSequence - client->reliableAcknowledge;
		for ( i = 0; i < n; i++ ) {
			const int idx = client->reliableAcknowledge + 1 + i;
			Com_Printf( "cmd %5d: %s\n", i, client->reliableCommands[ idx & ( MAX_RELIABLE_COMMANDS - 1 ) ] );
		}
		Com_Printf( "cmd %5d: %s\n", i, cmd );
		SV_DropClient( client, "Server command overflow" );
		return;
	}
	index = client->reliableSequence & ( MAX_RELIABLE_COMMANDS - 1 );
	Q_strncpyz( client->reliableCommands[ index ], cmd, sizeof( client->reliableCommands[ index ] ) );
}


/*
=================
SV_SendServerCommand

Sends a reliable command string to be interpreted by 
the client game module: "cp", "print", "chat", etc
A NULL client will broadcast to all clients
=================
*/
void QDECL SV_SendServerCommand( client_t *cl, const char *fmt, ... ) {
	va_list		argptr;
	char		message[MAX_STRING_CHARS+128]; // slightly larger than allowed, to detect overflows
	client_t	*client;
	int			j, len;
	
	va_start( argptr, fmt );
	len = Q_vsnprintf( message, sizeof( message ), fmt, argptr );
	va_end( argptr );

	if ( cl != NULL ) {
		// outdated clients can't properly decode 1023-chars-long strings
		// http://aluigi.altervista.org/adv/q3msgboom-adv.txt
		if ( len <= 1022 || cl->longstr ) {
			SV_AddServerCommand( cl, message );
		}
		return;
	}

	// hack to echo broadcast prints to console
	if ( com_dedicated->integer && !strncmp( message, "print", 5 ) ) {
		Com_Printf( "broadcast: %s\n", SV_ExpandNewlines( message ) );
	}

	// send the data to all relevant clients
	for ( j = 0, client = svs.clients; j < sv.maxclients; j++, client++ ) {
		if ( len <= 1022 || client->longstr ) {
			SV_AddServerCommand( client, message );
		}
	}
}


/*
==============================================================================

MASTER SERVER FUNCTIONS

==============================================================================
*/

/*
================
SV_MasterHeartbeat

Send a message to the masters every few minutes to
let it know we are alive, and log information.
We will also have a heartbeat sent when a server
changes from empty to non-empty, and full to non-full,
but not on every player enter or exit.
================
*/
#define	HEARTBEAT_MSEC	(300*1000)
#define	MASTERDNS_MSEC	(24*60*60*1000)
static void SV_MasterHeartbeat( const char *message )
{
	static netadr_t	adr[MAX_MASTER_SERVERS][2]; // [2] for v4 and v6 address for the same address string.
	int			i;
	int			res;
	int			netenabled;

	netenabled = Cvar_VariableIntegerValue("net_enabled");

	// "dedicated 1" is for lan play, "dedicated 2" is for inet public play
	if (!com_dedicated || com_dedicated->integer != 2 || !(netenabled & (NET_ENABLEV4 | NET_ENABLEV6)))
		return;		// only dedicated servers send heartbeats

	// if not time yet, don't send anything
	if ( svs.nextHeartbeatTime - svs.time > 0 )
		return;

	svs.nextHeartbeatTime = svs.time + HEARTBEAT_MSEC;

#ifdef USE_AUTH
	VM_Call( gvm, 0, GAME_AUTHSERVER_HEARTBEAT );
#endif

	// send to group masters
	for (i = 0; i < MAX_MASTER_SERVERS; i++)
	{
		if(!sv_master[i]->string[0])
			continue;

		// see if we haven't already resolved the name or if it's been over 24 hours
		// resolving usually causes hitches on win95, so only do it when needed
		if ( sv_master[i]->modified || svs.time - svs.masterResolveTime[i] > 0 )
		{
			sv_master[i]->modified = qfalse;
			svs.masterResolveTime[i] = svs.time + MASTERDNS_MSEC;
			
			if(netenabled & NET_ENABLEV4)
			{
				Com_Printf("Resolving %s (IPv4)\n", sv_master[i]->string);
				res = NET_StringToAdr(sv_master[i]->string, &adr[i][0], NA_IP);

				if(res == 2)
				{
					// if no port was specified, use the default master port
					adr[i][0].port = BigShort(PORT_MASTER);
				}
				
				if(res)
					Com_Printf( "%s resolved to %s\n", sv_master[i]->string, NET_AdrToStringwPort( &adr[i][0] ) );
				else
					Com_Printf( "%s has no IPv4 address.\n", sv_master[i]->string );
			}
#ifdef USE_IPV6
			if(netenabled & NET_ENABLEV6)
			{
				Com_Printf("Resolving %s (IPv6)\n", sv_master[i]->string);
				res = NET_StringToAdr(sv_master[i]->string, &adr[i][1], NA_IP6);

				if(res == 2)
				{
					// if no port was specified, use the default master port
					adr[i][1].port = BigShort(PORT_MASTER);
				}
				
				if(res)
					Com_Printf( "%s resolved to %s\n", sv_master[i]->string, NET_AdrToStringwPort( &adr[i][1] ) );
				else
					Com_Printf( "%s has no IPv6 address.\n", sv_master[i]->string );
			}
#endif
		}

		if( adr[i][0].type == NA_BAD && adr[i][1].type == NA_BAD )
		{
			continue;
		}


		Com_Printf ("Sending heartbeat to %s\n", sv_master[i]->string );

		// this command should be changed if the server info / status format
		// ever changes incompatibly

		if(adr[i][0].type != NA_BAD)
			NET_OutOfBandPrint( NS_SERVER, &adr[i][0], "heartbeat %s\n", message);
		if(adr[i][1].type != NA_BAD)
			NET_OutOfBandPrint( NS_SERVER, &adr[i][1], "heartbeat %s\n", message);
	}
}


/*
=================
SV_MasterShutdown

Informs all masters that this server is going down
=================
*/
void SV_MasterShutdown( void )
{
	// send a heartbeat right now
	svs.nextHeartbeatTime = svs.time;
	SV_MasterHeartbeat(HEARTBEAT_FOR_MASTER);

	// send it again to minimize chance of drops
	svs.nextHeartbeatTime = svs.time;
	SV_MasterHeartbeat(HEARTBEAT_FOR_MASTER);

	// when the master tries to poll the server, it won't respond, so
	// it will be removed from the list

#ifdef USE_AUTH
	VM_Call( gvm, 0, GAME_AUTHSERVER_SHUTDOWN );
#endif
}


/*
==============================================================================

CONNECTIONLESS COMMANDS

==============================================================================
*/

// This is deliberately quite large to make it more of an effort to DoS
#define MAX_BUCKETS        16384
#define MAX_HASHES          1024

static leakyBucket_t buckets[ MAX_BUCKETS ];
static leakyBucket_t *bucketHashes[ MAX_HASHES ];
static rateLimit_t outboundRateLimit;

/*
================
SVC_HashForAddress
================
*/
static int SVC_HashForAddress( const netadr_t *address ) {
	const byte	*ip = NULL;
	int			size = 0;
	int			hash = 0;
	int			i;

	switch ( address->type ) {
		case NA_IP:  ip = address->ipv._4; size = 4;  break;
#ifdef USE_IPV6
		case NA_IP6: ip = address->ipv._6; size = 16; break;
#endif
		default: break;
	}

	for ( i = 0; i < size; i++ ) {
		hash += (int)( ip[ i ] ) * ( i + 119 );
	}

	hash = ( hash ^ ( hash >> 10 ) ^ ( hash >> 20 ) );
	hash &= ( MAX_HASHES - 1 );

	return hash;
}


/*
================
SVC_RelinkToHead
================
*/
static void SVC_RelinkToHead( leakyBucket_t *bucket, int hash ) {

	if ( bucket->prev != NULL ) {
		bucket->prev->next = bucket->next;
	} else {
		return;
	}

	if ( bucket->next != NULL ) {
		bucket->next->prev = bucket->prev;
	}

	bucket->next = bucketHashes[ hash ];
	if ( bucketHashes[ hash ] != NULL ) {
		bucketHashes[ hash ]->prev = bucket;
	}

	bucket->prev = NULL;
	bucketHashes[ hash ] = bucket;
}


/*
================
SVC_BucketForAddress

Find or allocate a bucket for an address
================
*/
static leakyBucket_t *SVC_BucketForAddress( const netadr_t *address, int burst, int period ) {
	static leakyBucket_t dummy = { 0 };
	static int		start = 0;
	const int		hash = SVC_HashForAddress( address );
	const int		now = Sys_Milliseconds();
	leakyBucket_t	*bucket;
	int				i, n;

	for ( bucket = bucketHashes[ hash ], n = 0; bucket; bucket = bucket->next, n++ ) {
		switch ( bucket->type ) {
			case NA_IP:
				if ( memcmp( bucket->ipv._4, address->ipv._4, 4 ) == 0 ) {
					if ( n > 8 ) {
						SVC_RelinkToHead( bucket, hash );
					}
					return bucket;
				}
				break;
#ifdef USE_IPV6
			case NA_IP6:
				if ( memcmp( bucket->ipv._6, address->ipv._6, 16 ) == 0 ) {
					if ( n > 8 ) {
						SVC_RelinkToHead( bucket, hash );
					}
					return bucket;
				}
				break;
#endif
			default:
				return &dummy;
		}
	}

	for ( i = 0; i < MAX_BUCKETS; i++ ) {
		int interval;

		if ( start >= MAX_BUCKETS )
			start = 0;
		bucket = &buckets[ start++ ];
		interval = now - bucket->rate.lastTime;

		// Reclaim expired buckets
		if ( bucket->type != NA_BAD && (unsigned)interval > ( bucket->rate.burst * period ) ) {
			if ( bucket->prev != NULL ) {
				bucket->prev->next = bucket->next;
			} else {
				bucketHashes[ bucket->hash ] = bucket->next;
			}
			
			if ( bucket->next != NULL ) {
				bucket->next->prev = bucket->prev;
			}

			bucket->type = NA_BAD;
		}

		if ( bucket->type == NA_BAD ) {
			bucket->type = address->type;
			switch ( address->type ) {
				case NA_IP:  Com_Memcpy( bucket->ipv._4, address->ipv._4, 4 );  break;
#ifdef USE_IPV6
				case NA_IP6: Com_Memcpy( bucket->ipv._6, address->ipv._6, 16 ); break;
#endif
				default: break;
			}

			bucket->rate.lastTime = now;
			bucket->rate.burst = 0;
			bucket->hash = hash;
			bucket->toxic = 0;

			// Add to the head of the relevant hash chain
			bucket->next = bucketHashes[ hash ];
			if ( bucketHashes[ hash ] != NULL ) {
				bucketHashes[ hash ]->prev = bucket;
			}

			bucket->prev = NULL;
			bucketHashes[ hash ] = bucket;

			return bucket;
		}
	}

	// Couldn't allocate a bucket for this address
	return NULL;
}


/*
================
SVC_RateLimit
================
*/
qboolean SVC_RateLimit( rateLimit_t *bucket, int burst, int period ) {
	int now = Sys_Milliseconds();
	int interval = now - bucket->lastTime;
	int expired = interval / period;
	int expiredRemainder = interval % period;

	if ( expired > bucket->burst || interval < 0 ) {
		bucket->burst = 0;
		bucket->lastTime = now;
	} else {
		bucket->burst -= expired;
		bucket->lastTime = now - expiredRemainder;
	}

	if ( bucket->burst < burst ) {
		bucket->burst++;
		return qfalse;
	}

	return qtrue;
}


/*
================
SVC_RateDrop
================
*/
static void SVC_RateDrop( leakyBucket_t *bucket, int burst ) {
	if ( bucket != NULL ) {
		if ( bucket->toxic < 10000 )
			++bucket->toxic;
		bucket->rate.burst = burst * bucket->toxic;
		bucket->rate.lastTime = Sys_Milliseconds();
	}
}


/*
================
SVC_RateRestoreBurst
================
*/
static void SVC_RateRestoreBurst( leakyBucket_t *bucket ) {
	if ( bucket != NULL ) {
		if ( bucket->rate.burst > 0 ) {
			bucket->rate.burst--;
		}
	}
}


/*
================
SVC_RateRestoreToxic
================
*/
static void SVC_RateRestoreToxic( leakyBucket_t *bucket ) {
	if ( bucket != NULL ) {
		if ( bucket->toxic > 0 ) {
			bucket->toxic--;
		}
	}
}


/*
================
SVC_RateLimitAddress

Rate limit for a particular address
================
*/
qboolean SVC_RateLimitAddress( const netadr_t *from, int burst, int period ) {
	leakyBucket_t *bucket = SVC_BucketForAddress( from, burst, period );

	return bucket ? SVC_RateLimit( &bucket->rate, burst, period ) : qtrue;
}


/*
================
SVC_RateRestoreAddress

Decrease burst rate
================
*/
void SVC_RateRestoreBurstAddress( const netadr_t *from, int burst, int period ) {
	leakyBucket_t *bucket = SVC_BucketForAddress( from, burst, period );

	SVC_RateRestoreBurst( bucket );
}


/*
================
SVC_RateRestoreToxicAddress

Decrease toxicity
================
*/
void SVC_RateRestoreToxicAddress( const netadr_t *from, int burst, int period ) {
	leakyBucket_t *bucket = SVC_BucketForAddress( from, burst, period );

	SVC_RateRestoreToxic( bucket );
}


/*
================
SVC_RateDropAddress
================
*/
void SVC_RateDropAddress( const netadr_t *from, int burst, int period ) {
	leakyBucket_t *bucket = SVC_BucketForAddress( from, burst, period );

	SVC_RateDrop( bucket, burst );
}


/*
================
SVC_Status

Responds with all the info that qplug or qspy can see about the server
and all connected players.  Used for getting detailed information after
the simple info query.
================
*/
static void SVC_Status( const netadr_t *from ) {
	char	player[MAX_NAME_LENGTH + 32]; // score + ping + name
	char	status[MAX_PACKETLEN];
	char	*s;
	int		i;
	client_t	*cl;
	playerState_t	*ps;
	int		statusLength;
	int		playerLength;
	char	infostring[MAX_INFO_STRING+160]; // add some space for challenge string

	// ignore if we are in single player
#ifndef DEDICATED
	if ( Cvar_VariableIntegerValue( "g_gametype" ) == GT_SINGLE_PLAYER || Cvar_VariableIntegerValue("ui_singlePlayerActive")) {
		return;
	}
#endif

	// Prevent using getstatus as an amplifier
	if ( SVC_RateLimitAddress( from, 10, 1000 ) ) {
		if ( com_developer->integer ) {
			Com_Printf( "SVC_Status: rate limit from %s exceeded, dropping request\n",
				NET_AdrToString( from ) );
		}
		return;
	}

	// Allow getstatus to be DoSed relatively easily, but prevent
	// excess outbound bandwidth usage when being flooded inbound
	if ( SVC_RateLimit( &outboundRateLimit, 10, 100 ) ) {
		Com_DPrintf( "SVC_Status: rate limit exceeded, dropping request\n" );
		return;
	}

	// A maximum challenge length of 128 should be more than plenty.
	if ( strlen( Cmd_Argv( 1 ) ) > 128 )
		return;

	Q_strncpyz( infostring, Cvar_InfoString( CVAR_SERVERINFO, NULL ), sizeof( infostring ) );

	// echo back the parameter to status. so master servers can use it as a challenge
	// to prevent timed spoofed reply packets that add ghost servers
	Info_SetValueForKey( infostring, "challenge", Cmd_Argv( 1 ) );

	s = status;
	status[0] = '\0';
	statusLength = strlen( infostring ) + 16; // strlen( "statusResponse\n\n" )

	for ( i = 0; i < sv.maxclients; i++ ) {
		cl = &svs.clients[i];
		if ( cl->state >= CS_CONNECTED ) {

			ps = SV_GameClientNum( i );
			playerLength = Com_sprintf( player, sizeof( player ), "%i %i \"%s\"\n", 
				ps->persistant[ PERS_SCORE ], cl->ping, cl->name );
			
			if ( statusLength + playerLength >= MAX_PACKETLEN-4 )
				break; // can't hold any more
			
			s = Q_stradd( s, player );
			statusLength += playerLength;
		}
	}

	NET_OutOfBandPrint( NS_SERVER, from, "statusResponse\n%s\n%s", infostring, status );
}


/*
================
SVC_Info

Responds with a short info message that should be enough to determine
if a user is interested in a server to do a full status
================
*/
static void SVC_Info( const netadr_t *from ) {
	int		i, count, humans;
	const char	*gamedir;
	char	infostring[MAX_INFO_STRING];

	// ignore if we are in single player
#ifndef DEDICATED
	if ( Cvar_VariableIntegerValue( "g_gametype" ) == GT_SINGLE_PLAYER || Cvar_VariableIntegerValue("ui_singlePlayerActive")) {
		return;
	}
#endif

	// Prevent using getinfo as an amplifier
	if ( SVC_RateLimitAddress( from, 10, 1000 ) ) {
		if ( com_developer->integer ) {
			Com_Printf( "SVC_Info: rate limit from %s exceeded, dropping request\n",
				NET_AdrToString( from ) );
		}
		return;
	}

	// Allow getinfo to be DoSed relatively easily, but prevent
	// excess outbound bandwidth usage when being flooded inbound
	if ( SVC_RateLimit( &outboundRateLimit, 10, 100 ) ) {
		Com_DPrintf( "SVC_Info: rate limit exceeded, dropping request\n" );
		return;
	}

	/*
	 * Check whether Cmd_Argv(1) has a sane length. This was not done in the original Quake3 version which led
	 * to the Infostring bug discovered by Luigi Auriemma. See http://aluigi.altervista.org/ for the advisory.
	 */

	// A maximum challenge length of 128 should be more than plenty.
	if ( strlen( Cmd_Argv( 1 ) ) > 128 )
		return;

	// don't count privateclients
	count = humans = 0;
	for ( i = sv_privateClients->integer; i < sv.maxclients; i++ ) {
		if ( svs.clients[i].state >= CS_CONNECTED ) {
			count++;
			if (svs.clients[i].netchan.remoteAddress.type != NA_BOT) {
				humans++;
			}
		}
	}

	infostring[0] = '\0';

	// echo back the parameter to status. so servers can use it as a challenge
	// to prevent timed spoofed reply packets that add ghost servers
	Info_SetValueForKey( infostring, "challenge", Cmd_Argv(1) );

	Info_SetValueForKey( infostring, "protocol", va( "%i", com_protocol->integer ) );
	Info_SetValueForKey( infostring, "hostname", sv_hostname->string );
	Info_SetValueForKey( infostring, "mapname", sv_mapname->string );
	Info_SetValueForKey( infostring, "clients", va("%i", count) );
	Info_SetValueForKey( infostring, "g_humanplayers", va( "%i", humans ) );
	Info_SetValueForKey( infostring, "sv_maxclients", va( "%i", sv.maxclients - sv_privateClients->integer ) );
	Info_SetValueForKey( infostring, "gametype", va( "%i", sv_gametype->integer ) );
	Info_SetValueForKey( infostring, "pure", va( "%i", sv.pure ) );
	Info_SetValueForKey( infostring, "g_needpass", va( "%d", Cvar_VariableIntegerValue( "g_needpass" ) ) );
	gamedir = Cvar_VariableString( "fs_game" );
	if ( *gamedir != '\0' ) {
		Info_SetValueForKey( infostring, "game", gamedir );
	}

#ifdef USE_AUTH
	Info_SetValueForKey( infostring, "auth", Cvar_VariableString( "auth" ) );
#endif

	if ( Cvar_VariableValue( "g_needpass" ) == 1 )
		Info_SetValueForKey( infostring, "password", "1" );

	NET_OutOfBandPrint( NS_SERVER, from, "infoResponse\n%s", infostring );
}


/*
================
SV_FlushRedirect
================
*/
static netadr_t redirectAddress; // for rcon return messages

static void SV_FlushRedirect( const char *outputbuf )
{
	if ( *outputbuf )
	{
		NET_OutOfBandPrint( NS_SERVER, &redirectAddress, "print\n%s", outputbuf );
	}
}


/*
===============
SVC_RemoteCommand

An rcon packet arrived from the network.
Shift down the remaining args
Redirect all printfs
===============
*/
static void SVC_RemoteCommand( const netadr_t *from ) {
	static rateLimit_t bucket;
	qboolean	valid;
	// TTimo - scaled down to accumulate, but not overflow anything network wise, print wise etc.
	// (OOB messages are the bottleneck here)
	char		sv_outputbuf[1024 - 16];
	const char	*cmd_aux, *pw;

	// Prevent using rcon as an amplifier and make dictionary attacks impractical
	if ( SVC_RateLimitAddress( from, 10, 1000 ) ) {
		if ( com_developer->integer ) {
			Com_Printf( "SVC_RemoteCommand: rate limit from %s exceeded, dropping request\n",
				NET_AdrToString( from ) );
		}
		return;
	}

	pw = Cmd_Argv( 1 );
	if ( ( sv_rconPassword->string[0] && strcmp( pw, sv_rconPassword->string ) == 0 ) ||
		( rconPassword2[0] && strcmp( pw, rconPassword2 ) == 0 ) ) {
		valid = qtrue;
		Com_Printf( "Rcon from %s: %s\n", NET_AdrToString( from ), Cmd_ArgsFrom( 2 ) );
	} else {
		// Make DoS via rcon impractical
		if ( SVC_RateLimit( &bucket, 10, 1000 ) ) {
			Com_DPrintf( "SVC_RemoteCommand: rate limit exceeded, dropping request\n" );
			return;
		}

		valid = qfalse;
		Com_Printf( "Bad rcon from %s: %s\n", NET_AdrToString( from ), Cmd_ArgsFrom( 2 ) );
	}

	// start redirecting all print outputs to the packet
	redirectAddress = *from;
	Com_BeginRedirect( sv_outputbuf, sizeof( sv_outputbuf ), SV_FlushRedirect );

	if ( !sv_rconPassword->string[0] && !rconPassword2[0] ) {
		Com_Printf( "No rconpassword set on the server.\n" );
	} else if ( !valid ) {
		Com_Printf( "Bad rconpassword.\n" );
	} else {
		// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=543
		// get the command directly, "rcon <pass> <command>" to avoid quoting issues
		// extract the command by walking
		// since the cmd formatting can fuckup (amount of spaces), using a dumb step by step parsing
		cmd_aux = Cmd_Cmd();
		while ( *cmd_aux && *cmd_aux <= ' ' ) // skip whitespace
			cmd_aux++;
		cmd_aux += 4; // "rcon"
		while ( *cmd_aux == ' ' )
			cmd_aux++;
		if ( *cmd_aux == '"' ) {
			cmd_aux++;
			while ( *cmd_aux && *cmd_aux != '"' ) // quoted password
				cmd_aux++;
			if ( *cmd_aux == '"' )
				cmd_aux++;
		} else {
			while ( *cmd_aux && *cmd_aux != ' ' ) // password
				cmd_aux++;
		}
		while ( *cmd_aux == ' ' )
			cmd_aux++;

		Cmd_ExecuteString( cmd_aux );
	}

	Com_EndRedirect();
}


/*
=================
SV_ConnectionlessPacket

A connectionless packet has four leading 0xff
characters to distinguish it from a game channel.
Clients that are in the game can still send
connectionless packets.
=================
*/
static void SV_ConnectionlessPacket( const netadr_t *from, msg_t *msg ) {
	const char *s;
	const char *c;

	MSG_BeginReadingOOB( msg );
	MSG_ReadLong( msg );		// skip the -1 marker

	if ( !memcmp( "connect ", msg->data + 4, 8 ) ) {
		if ( msg->cursize > MAX_INFO_STRING*2 ) { // if we assume 200% compression ratio on userinfo
			if ( com_developer->integer ) {
				Com_Printf( "%s : connect packet is too long - %i\n", NET_AdrToString( from ), msg->cursize );
			}
			return;
		}
		Huff_Decompress( msg, 12 );
	}

	s = MSG_ReadStringLine( msg );
	Cmd_TokenizeString( s );

	c = Cmd_Argv(0);

	if ( com_developer->integer ) {
		Com_Printf( "SV packet %s : %s\n", NET_AdrToString( from ), c );
	}

	if ( !Q_stricmp(c, "rcon") ) {
		SVC_RemoteCommand( from );
		return;
	}

	if ( !com_sv_running->integer ) {
		return;
	}

	if (!Q_stricmp(c, "getstatus")) {
		SVC_Status( from );
	} else if (!Q_stricmp(c, "getinfo")) {
		SVC_Info( from );
	} else if (!Q_stricmp(c, "getchallenge")) {
		SV_GetChallenge( from );
	} else if (!Q_stricmp(c, "connect")) {
		SV_DirectConnect( from );
#ifndef STANDALONE
	} else if (!Q_stricmp(c, "ipAuthorize")) {
		// removed from codebase since stateless challenges
#endif
	} else if (!Q_stricmp(c, "disconnect")) {
		// if a client starts up a local server, we may see some spurious
		// server disconnect messages when their new server sees our final
		// sequenced messages to the old client
#ifdef USE_AUTH
	} else if (!Q_stricmp(c, "AUTH:SV")) {
		netadr_t authServerIP;
		NET_StringToAdr( sv_authServerIP->string, &authServerIP, NA_IP );
		if ( !NET_CompareBaseAdr( from, &authServerIP ) ) {
			Com_Printf( "AUTH:SV not from the Auth Server\n" );
			return;
		}
		VM_Call( gvm, 0, GAME_AUTHSERVER_PACKET );
#endif
	} else {
		if ( com_developer->integer ) {
			Com_Printf( "bad connectionless packet from %s:\n%s\n",
				NET_AdrToString( from ), s );
		}
	}
}

//============================================================================

/*
=================
SV_PacketEvent
=================
*/
void SV_PacketEvent( const netadr_t *from, msg_t *msg ) {
	int			i;
	client_t	*cl;
	int			qport;

	if ( msg->cursize < 6 ) // too short for anything
		return;

	// check for connectionless packet (0xffffffff) first
	if ( *(int32_t *)msg->data == -1 ) {
		SV_ConnectionlessPacket( from, msg );
		return;
	}

	if ( sv.state == SS_DEAD ) {
		return;
	}

	// read the qport out of the message so we can fix up
	// stupid address translating routers
	MSG_BeginReadingOOB( msg );
	MSG_ReadLong( msg ); // sequence number
	qport = MSG_ReadShort( msg ) & 0xffff;

	// find which client the message is from
	for ( i = 0, cl = svs.clients; i < sv.maxclients; i++, cl++ ) {
		if ( cl->state == CS_FREE ) {
			continue;
		}
		if ( !NET_CompareBaseAdr( from, &cl->netchan.remoteAddress ) ) {
			continue;
		}
		// it is possible to have multiple clients from a single IP
		// address, so they are differentiated by the qport variable
		if ( cl->netchan.qport != qport ) {
			continue;
		}

		// make sure it is a valid, in sequence packet
		if ( SV_Netchan_Process( cl, msg ) ) {
			// the IP port can't be used to differentiate clients, because
			// some address translating routers periodically change UDP
			// port assignments
			if ( cl->netchan.remoteAddress.port != from->port ) {
				Com_Printf( "SV_PacketEvent: fixing up a translated port\n" );
				cl->netchan.remoteAddress.port = from->port;
			}
			// zombie clients still need to do the Netchan_Process
			// to make sure they don't need to retransmit the final
			// reliable message, but they don't do any other processing
			if ( cl->state != CS_ZOMBIE ) {
				cl->lastPacketTime = svs.time;	// don't timeout
				SV_ExecuteClientMessage( cl, msg );
			}
			return;
		}
	}
}


/*
===================
SV_CalcPings

Updates the cl->ping variables
===================
*/
static void SV_CalcPings( void ) {
	int			i, j;
	client_t	*cl;
	int			total, count;
	int			delta;
	playerState_t	*ps;

	for ( i = 0; i < sv.maxclients; i++ ) {
		cl = &svs.clients[i];
		if ( cl->state != CS_ACTIVE ) {
			cl->ping = 999;
			continue;
		}
		if ( !cl->gentity ) {
			cl->ping = 999;
			continue;
		}
		if ( cl->netchan.remoteAddress.type == NA_BOT ) {
			cl->ping = 0;
			continue;
		}

		total = 0;
		count = 0;
		for ( j = 0 ; j < PACKET_BACKUP ; j++ ) {
			if ( cl->frames[j].messageAcked == 0 ) {
				continue;
			}
			delta = cl->frames[j].messageAcked - cl->frames[j].messageSent;
			count++;
			total += delta;
		}
		if (!count) {
			cl->ping = 999;
		} else {
			cl->ping = total/count;
			if ( cl->ping > 999 ) {
				cl->ping = 999;
			}
		}

		// let the game dll know about the ping
		ps = SV_GameClientNum( i );
		ps->ping = cl->ping;
	}
}

/*
==================
SV_CheckTimeouts

If a packet has not been received from a client for timeout->integer 
seconds, drop the connection.  Server time is used instead of
realtime to avoid dropping the local client while debugging.

When a client is normally dropped, the client_t goes into a zombie state
for a few seconds to make sure any final reliable message gets resent
if necessary
==================
*/
static void SV_CheckTimeouts( void ) {
	int		i;
	client_t	*cl;
	int			droppoint;
	int			zombiepoint;

	droppoint = svs.time - 1000 * sv_timeout->integer;
	zombiepoint = svs.time - 1000 * sv_zombietime->integer;

	for ( i = 0, cl = svs.clients ; i < sv.maxclients; i++, cl++ ) {
		if ( cl->state == CS_FREE ) {
			continue;
		}
		// message times may be wrong across a changelevel
		if ( cl->lastPacketTime - svs.time > 0 ) {
			cl->lastPacketTime = svs.time;
		}

		if ( cl->state == CS_ZOMBIE && cl->lastPacketTime - zombiepoint < 0 ) {
			// using the client id cause the cl->name is empty at this point
			SV_PrintClientStateChange( cl, CS_FREE );
			cl->state = CS_FREE;	// can now be reused
			continue;
		}
		if ( cl->justConnected && svs.time - cl->lastPacketTime > 4000 ) {
			// for real client 4 seconds is more than enough to respond
			SVC_RateDropAddress( &cl->netchan.remoteAddress, 10, 1000 ); // enforce burst with progressive multiplier
			SV_DropClient( cl, NULL ); // drop silently
			cl->state = CS_FREE;
			continue;
		}
		// Ghost-client guard: a fully in-game client whose process was killed
		// stops sending packets immediately (OS closes the socket).  With the
		// default sv_timeout of 200 s the normal timeout fires far too late --
		// the 128-slot reliable command buffer can fill up with broadcast
		// prints in seconds, causing a noisy "Server command overflow" drop.
		// Kick them cleanly after 10 s of silence so the slot is freed well
		// before the buffer ever overflows.
		// Let SV_DropClient handle state transition to CS_ZOMBIE naturally
		// so the disconnect message can be retransmitted to other clients.
		if ( cl->state == CS_ACTIVE && svs.time - cl->lastPacketTime > 10000 ) {
			SV_DropClient( cl, "disconnected" );
			continue;
		}
		if ( cl->state >= CS_CONNECTED && cl->lastPacketTime - droppoint < 0 ) {
			// wait several frames so a debugger session doesn't
			// cause a timeout
			if ( ++cl->timeoutCount > 5 ) {
				SV_DropClient( cl, "timed out" );
				cl->state = CS_FREE;	// don't bother with zombie state
			}
		} else {
			cl->timeoutCount = 0;
		}
	}
}


/*
==================
SV_CheckPaused
==================
*/
static qboolean SV_CheckPaused( void ) {

#ifdef DEDICATED
	// can't pause on dedicated servers
	return qfalse;
#else
	const client_t *cl;
	int	count;
	int	i;

	if ( !cl_paused->integer ) {
		return qfalse;
	}

	// only pause if there is just a single client connected
	count = 0;
	for ( i = 0, cl = svs.clients ; i < sv.maxclients; i++, cl++ ) {
		if ( cl->state >= CS_CONNECTED && cl->netchan.remoteAddress.type != NA_BOT ) {
			count++;
		}
	}

	if ( count > 1 ) {
		// don't pause
		if (sv_paused->integer)
			Cvar_Set("sv_paused", "0");
		return qfalse;
	}

	if (!sv_paused->integer)
		Cvar_Set("sv_paused", "1");

	return qtrue;
#endif // !DEDICATED
}


/*
==================
SV_FrameMsec
Return time in millseconds until processing of the next server frame.
==================
*/
int SV_FrameMsec( void )
{
	if ( sv_fps )
	{
		int frameMsec;
		int timeLeft;
		int spinMs;

		frameMsec = 1000.0f / sv_fps->value;

		if ( frameMsec < sv.timeResidual )
			return 0;

		timeLeft = frameMsec - sv.timeResidual;

		// If sv_busyWait is set, spin (return 0) for the last N ms before
		// the frame is due, giving precise wakeup without OS sleep jitter.
		// Costs ~1 CPU core but eliminates stutter at high sv_fps.
		spinMs = sv_busyWait ? sv_busyWait->integer : 0;
		if ( spinMs > 0 && timeLeft <= spinMs )
			return 0;

		return timeLeft;
	}
	else
		return 1;
}


/*
==================
SV_TrackCvarChanges
==================
*/
void SV_TrackCvarChanges( void )
{
	client_t *cl;
	int i;

	if ( sv_maxRate->integer && sv_maxRate->integer < 1000 ) {
		Cvar_Set( "sv_maxRate", "1000" );
		Com_DPrintf( "sv_maxRate adjusted to 1000\n" );
	}

	if ( sv_minRate->integer && sv_minRate->integer < 1000 ) {
		Cvar_Set( "sv_minRate", "1000" );
		Com_DPrintf( "sv_minRate adjusted to 1000\n" );
	}

	// When sv_gameHz changes at runtime, clamp gameTimeResidual so it doesn't
	// burst-fire multiple game frames on the next tick
	if ( sv_gameHz->modified ) {
		int _gameHz = (sv_gameHz->integer > 0) ? sv_gameHz->integer : sv_fps->integer;
		int newGameMsec;
		if ( _gameHz < 1 )               _gameHz = 1;
		if ( _gameHz > sv_fps->integer ) _gameHz = sv_fps->integer;
		newGameMsec = 1000 / _gameHz;
		if ( sv.gameTimeResidual < 0 )
			sv.gameTimeResidual = 0;
		if ( sv.gameTimeResidual >= newGameMsec )
			sv.gameTimeResidual = newGameMsec - 1;
		sv_gameHz->modified = qfalse;
		Com_DPrintf( "sv_gameHz changed to %d -- gameTimeResidual clamped\n", sv_gameHz->integer );
	}

	// When sv_antiwarp is enabled, force QVM g_antiwarp off to prevent double injection.
	if ( sv_antiwarp->modified ) {
		if ( sv_antiwarp->integer ) {
			Cvar_Set( "g_antiwarp", "0" );
			Com_Printf( "sv_antiwarp enabled -- forcing g_antiwarp 0\n" );
		}
		sv_antiwarp->modified = qfalse;
	}

	// When sv_antilag is enabled, force QVM g_antilag off.
	// The engine shadow system intercepts all G_TRACE/G_TRACECAPSULE syscalls and
	// handles hit registration directly.  With g_antilag 1, the QVM FIFO antilag
	// would rewind entities before calling trap_Trace, creating an unnecessary
	// pre-setup step that the engine then overrides anyway.  Forcing g_antilag 0
	// makes the execution path unambiguous: engine shadow rewind runs against the
	// post-game-frame entity state, no FIFO intermediary.
	if ( sv_antilag && sv_antilag->modified ) {
		if ( sv_antilag->integer ) {
			Cvar_Set( "g_antilag", "0" );
			Com_Printf( "sv_antilag enabled -- forcing g_antilag 0\n" );
		}
		sv_antilag->modified = qfalse;
	}
	if ( sv_antiwarpTol->modified )
		sv_antiwarpTol->modified = qfalse;
	if ( sv_antiwarpExtra->modified )
		sv_antiwarpExtra->modified = qfalse;
	if ( sv_antiwarpDecay->modified )
		sv_antiwarpDecay->modified = qfalse;

	// When sv_fps changes at runtime, clamp the time residual so it doesn't
	// represent more than one frame at the new rate
	if ( sv_fps->modified || sv_snapshotFps->modified ) {
		qboolean fpsChanged = sv_fps->modified;
		int newFrameMsec = 1000 / sv_fps->integer;

		if ( fpsChanged ) {
			if ( sv.timeResidual < 0 )
				sv.timeResidual = 0;
			if ( sv.timeResidual >= newFrameMsec )
				sv.timeResidual = newFrameMsec - 1;
			// When sv_gameHz <= 0 the game frame rate equals sv_fps
			if ( sv_gameHz->integer <= 0 ) {
				if ( sv.gameTimeResidual < 0 )
					sv.gameTimeResidual = 0;
				if ( sv.gameTimeResidual >= newFrameMsec )
					sv.gameTimeResidual = newFrameMsec - 1;
			}
			Com_DPrintf( "sv_fps changed to %d -- timeResidual clamped\n", sv_fps->integer );
			if ( sv_fps->integer > 0 && 1000 % sv_fps->integer != 0 ) {
				Com_Printf( S_COLOR_YELLOW "WARNING: sv_fps %d is not a factor of 1000 -- "
					"frame interval %.2f ms is fractional. "
					"Use 10, 20, 25, 40, 50, 100, or 125 for exact timing.\n",
					sv_fps->integer, 1000.0f / sv_fps->integer );
			}
		}

		sv_fps->modified = qfalse;
		sv_snapshotFps->modified = qfalse;
	}

	Cvar_ResetGroup( CVG_SERVER, qfalse );

	if ( sv.state == SS_DEAD || !svs.clients )
		return;

	for ( i = 0, cl = svs.clients; i < sv.maxclients; i++, cl++ ) {
		if ( cl->state >= CS_CONNECTED ) {
			SV_UserinfoChanged( cl, qfalse, qfalse ); // do not update userinfo, do not run filter
		}
	}
}


/*
==================
SV_Restart
==================
*/
static void SV_Restart( const char *reason ) {
	qboolean sv_shutdown = qfalse;
	char mapName[ MAX_CVAR_VALUE_STRING ];
	int i;

	if ( svs.clients ) {
		// check if we can reset map time without full server shutdown
		for ( i = 0; i < sv.maxclients; i++ ) {
			if ( svs.clients[i].state >= CS_CONNECTED ) {
				sv_shutdown = qtrue;
				break;
			}
		}
	}

	sv.time = 0; // force level time reset
	sv.gameTime = 0;
	sv.gameTimeResidual = 0;
	sv.restartTime = 0;
	
	Cvar_VariableStringBuffer( "mapname", mapName, sizeof( mapName ) );
	
	if ( sv_shutdown ) {
		SV_Shutdown( reason );
	}

	Cbuf_AddText( va( "map %s\n", mapName ) );
}


/*
==================
SV_Frame

Player movement occurs as a result of packet events, which
happen before SV_Frame is called
==================
*/
void SV_Frame( int msec ) {
	int		frameMsec;
	int		startTime;
	int		i;

	if ( Cvar_CheckGroup( CVG_SERVER ) || sv_fps->modified )
		SV_TrackCvarChanges(); // update rate settings, etc.

	// the menu kills the server with this cvar
	if ( sv_killserver->integer ) {
		SV_Shutdown( "Server was killed" );
		Cvar_Set( "sv_killserver", "0" );
		return;
	}

	if ( !com_sv_running->integer )
	{
		if ( com_dedicated->integer )
		{
			// Block indefinitely until something interesting happens
			// on STDIN.
			Sys_Sleep( -1 );
		}
		return;
	}

	// allow pause if only the local client is connected
	if ( SV_CheckPaused() ) {
		return;
	}

	// if it isn't time for the next frame, do nothing

	frameMsec = 1000 / sv_fps->integer * com_timescale->value;
	// don't let it scale below 1ms
	if(frameMsec < 1)
	{
		Cvar_Set( "timescale", va( "%f", sv_fps->value / 1000.0f ) );
		Com_DPrintf( "timescale adjusted to %f\n", com_timescale->value );
		frameMsec = 1;
	}

	sv.timeResidual += msec;

	// Safety cap: allow at most 2 ticks of catch-up per Com_Frame so normal OS
	// scheduling jitter is absorbed gracefully (one late frame -> double-tick to
	// recover). sv_fps change burst protection is handled precisely in
	// SV_TrackCvarChanges; this is a backstop for extreme lag (debugger pause,
	// system suspend, map load, etc.) that would otherwise fire dozens of ticks
	// at once, flooding the reliable command buffer before clients can ack any.
	if ( sv.timeResidual > frameMsec * 2 )
		sv.timeResidual = frameMsec * 2;

	if ( !com_dedicated->integer )
		SV_BotFrame( sv.time + sv.timeResidual );

	// if time is about to hit the 32nd bit, kick all clients
	// and clear sv.time, rather
	// than checking for negative time wraparound everywhere.
	// 2giga-milliseconds = 23 days, so it won't be too often
	if ( sv.time > 0x78000000 ) {
		SV_Restart( "Restarting server due to time wrapping" );
		return;
	}

	// try to do silent restart earlier if possible
	if ( sv.time > (12*3600*1000) && ( sv_levelTimeReset->integer == 0 || sv.time > 0x40000000 ) ) {
		if ( svs.clients ) {
			for ( i = 0; i < sv.maxclients; i++ ) {
				// FIXME: deal with bots (reconnect?)
				if ( svs.clients[i].state != CS_FREE && svs.clients[i].netchan.remoteAddress.type != NA_BOT ) {
					break;
				}
			}
			if ( i == sv.maxclients ) {
				SV_Restart( "Restarting server" );
				return;
			}
		}
	}

	if ( sv.restartTime && sv.time - sv.restartTime >= 0 ) {
		sv.restartTime = 0;
		Cbuf_AddText( "map_restart 0\n" );
		return;
	}

	// update infostrings if anything has been changed
	if ( cvar_modifiedFlags & CVAR_SERVERINFO ) {
		SV_SetConfigstring( CS_SERVERINFO, Cvar_InfoString( CVAR_SERVERINFO, NULL ) );
		cvar_modifiedFlags &= ~CVAR_SERVERINFO;
	}
	if ( cvar_modifiedFlags & CVAR_SYSTEMINFO ) {
		SV_SetConfigstring( CS_SYSTEMINFO, Cvar_InfoString_Big( CVAR_SYSTEMINFO, NULL ) );
		cvar_modifiedFlags &= ~CVAR_SYSTEMINFO;
	}

	if ( com_speeds->integer ) {
		startTime = Sys_Milliseconds();
	} else {
		startTime = 0;	// quite a compiler warning
	}

	// update ping based on the all received frames
	SV_CalcPings();

	// run the game simulation in chunks
	// Snapshot dispatch is inside this loop so each engine tick at sv_fps rate produces
	// its own snapshot send opportunity, preventing double-interval gaps.
	while ( sv.timeResidual >= frameMsec ) {
		qboolean _gameFrameRan = qfalse;
		sv.timeResidual -= frameMsec;
		svs.time += frameMsec;
		sv.time += frameMsec;

		// Fire GAME_RUN_FRAME at sv_gameHz rate, independent of sv_fps.
		// sv_fps = input sampling rate; sv_gameHz = level.time rate.
		//
		// sv_gameHz > 0 (e.g. 20): GAME_RUN_FRAME fires every (1000/sv_gameHz) ms.
		// sv_gameHz <= 0 (disabled): effective rate = sv_fps. GAME_RUN_FRAME fires
		//   every engine tick -- sv.gameTime == sv.time always.
		{
			int _gameHz = (sv_gameHz && sv_gameHz->integer > 0) ? sv_gameHz->integer : sv_fps->integer;
			int _gameMsec;
			if ( _gameHz < 1 )               _gameHz = 1;
			if ( _gameHz > sv_fps->integer ) _gameHz = sv_fps->integer;
			_gameMsec = 1000 / _gameHz;
			sv.gameTimeResidual += frameMsec;

			while ( sv.gameTimeResidual >= _gameMsec ) {
				sv.gameTimeResidual -= _gameMsec;
				sv.gameTime += _gameMsec;
				_gameFrameRan = qtrue;

				// --- Engine-side antiwarp: inject blank commands for lagging clients ---
				// Runs before GAME_RUN_FRAME so the QVM sees fresh ClientThink calls.
				// Uses gameMsec instead of hardcoded 50ms, works at any sv_fps / sv_gameHz.
				//
				// Mode 1: constant -- keep last inputs indefinitely (QVM-style).
				// Mode 2: decay -- extrapolate for sv_antiwarpExtra ms,
				//   then linearly decay inputs over sv_antiwarpDecay ms,
				//   then coast to stop via Pmove friction.
				if ( sv_antiwarp && sv_antiwarp->integer ) {
					int awMode = sv_antiwarp->integer;
					int awTol = ( sv_antiwarpTol && sv_antiwarpTol->integer > 0 )
						? sv_antiwarpTol->integer : _gameMsec;
					int awExtraMs = ( sv_antiwarpExtra && sv_antiwarpExtra->integer > 0 )
						? sv_antiwarpExtra->integer : awTol;
					int awDecayMs = ( sv_antiwarpDecay && sv_antiwarpDecay->integer > 0 )
						? sv_antiwarpDecay->integer : 0;
					int awIdx;
					client_t *awCl;

					for ( awIdx = 0, awCl = svs.clients; awIdx < sv.maxclients; awIdx++, awCl++ ) {
						int awGap;

						if ( awCl->state != CS_ACTIVE )
							continue;
						if ( awCl->netchan.remoteAddress.type == NA_BOT )
							continue;
						if ( awCl->awLastThinkTime == 0 )
							continue; // never received a real command yet

						awGap = sv.gameTime - awCl->awLastThinkTime;
						if ( awGap > awTol ) {
							awCl->lastUsercmd.serverTime += _gameMsec;
							// Cap so injected serverTime never outruns real time --
							// without this, a long lag spike inflates serverTime
							// so that real usercmds are silently dropped on resume.
							if ( awCl->lastUsercmd.serverTime > sv.gameTime )
								awCl->lastUsercmd.serverTime = sv.gameTime;

							if ( awMode >= 2 ) {
								int decayStart = awTol + awExtraMs;
								int elapsed = awGap - decayStart;

								if ( elapsed > 0 ) {
									if ( awDecayMs > 0 && elapsed < awDecayMs ) {
										int scale = 127 - ( 127 * elapsed / awDecayMs );
										if ( scale < 0 ) scale = 0;
										awCl->lastUsercmd.forwardmove = (signed char)( (int)awCl->lastUsercmd.forwardmove * scale / 127 );
										awCl->lastUsercmd.rightmove   = (signed char)( (int)awCl->lastUsercmd.rightmove   * scale / 127 );
										awCl->lastUsercmd.upmove      = (signed char)( (int)awCl->lastUsercmd.upmove      * scale / 127 );
									} else {
										awCl->lastUsercmd.forwardmove = 0;
										awCl->lastUsercmd.rightmove   = 0;
										awCl->lastUsercmd.upmove      = 0;
									}
								}
							}

							VM_Call( gvm, 1, GAME_CLIENT_THINK, awIdx );
							awCl->awLastThinkTime = sv.gameTime;

							if ( awCl->state != CS_ACTIVE )
								continue; // client was kicked during think
						}
					}
				}

				// Bot AI ticks in lockstep with GAME_RUN_FRAME at sv_gameHz rate.
				if ( com_dedicated->integer )
					SV_BotFrame( sv.gameTime );
				VM_Call( gvm, 1, GAME_RUN_FRAME, sv.gameTime );
			}
		}

		// Record positions into smooth buffer when either feature is enabled.
		// Ring buffer feeds both sv_bufferMs (position delay) and sv_velSmooth
		// (velocity smoothing).
		//
		// Runs unconditionally every sv_fps tick -- NOT gated on _gameFrameRan.
		// sv_bufferMs delays positions by N wall-clock ms (sv.time).  It needs
		// one entry per sv_fps tick so that a "give me position from T-bufferMs"
		// lookup always finds a tightly-bracketed pair.  When sv_gameHz > 0,
		// entity positions do not change between game frames, but the timestamps
		// still advance; recording duplicates is harmless and necessary for
		// correct ms-accurate buffering.  Gating on _gameFrameRan would collapse
		// the ring buffer to game-frame rate and break bufferMs accuracy.
		if ( ( ( sv_extrapolate && sv_extrapolate->integer ) || ( sv_smoothClients && sv_smoothClients->integer ) )
				&& ( ( sv_bufferMs && sv_bufferMs->integer != 0 )
				  || ( sv_velSmooth && sv_velSmooth->integer > 0 ) ) ) {
			SV_SmoothRecordAll();
		}

		// Record antilag shadow positions AFTER the game frame so that
		// shadow[T] == snapshot[T] (both capture post-game-frame entity state).
		// Guarded by _gameFrameRan: when sv_gameHz > 0 the inner loop may not fire
		// every sv_fps tick (gameTimeResidual < gameMsec). Recording on a no-game-frame
		// tick would add a duplicate shadow entry (stale positions, new sv.time tag)
		// that does not correspond to any real entity state change. Only record when
		// the game actually advanced this tick.
		if ( sv_antilag && sv_antilag->integer && _gameFrameRan )
			SV_Antilag_RecordPositions();

		// Issue and dispatch snapshots once per sv_fps tick, not once per Com_Frame.
		SV_IssueNewSnapshot();
		SV_SendClientMessages();
	}

	if ( com_speeds->integer ) {
		time_game = Sys_Milliseconds () - startTime;
	}

	// check timeouts (once per Com_Frame is sufficient)
	SV_CheckTimeouts();

	// send a heartbeat to the master if needed
	SV_MasterHeartbeat(HEARTBEAT_FOR_MASTER);
}


/*
====================
SV_RateMsec

Return the number of msec until another message can be sent to
a client based on its rate settings
====================
*/

#define UDPIP_HEADER_SIZE 28
#define UDPIP6_HEADER_SIZE 48

int SV_RateMsec( const client_t *client )
{
	int rate, rateMsec;
	int messageSize;
	
	if ( !client->rate )
		return 0;

	messageSize = client->netchan.lastSentSize;

#ifdef USE_IPV6
	if ( client->netchan.remoteAddress.type == NA_IP6 )
		messageSize += UDPIP6_HEADER_SIZE;
	else
#endif
		messageSize += UDPIP_HEADER_SIZE;
		
	rateMsec = messageSize * 1000 / ((int) (client->rate * com_timescale->value));
	rate = Sys_Milliseconds() - client->netchan.lastSentTime;
	
	if ( rate > rateMsec )
		return 0;
	else
		return rateMsec - rate;
}


/*
====================
SV_SendQueuedPackets

Send download messages and queued packets in the time that we're idle, i.e.
not computing a server frame or sending client snapshots.
Return the time in msec until we expect to be called next
====================
*/
int SV_SendQueuedPackets( void )
{
	int numBlocks;
	int dlStart, deltaT, delayT;
	static int dlNextRound = 0;
	int timeVal = INT_MAX;

	// Send out fragmented packets now that we're idle
	delayT = SV_SendQueuedMessages();
	if(delayT >= 0)
		timeVal = delayT;

	if(sv_dlRate->integer)
	{
		// Rate limiting. This is very imprecise for high
		// download rates due to millisecond timedelta resolution
		dlStart = Sys_Milliseconds();
		deltaT = dlNextRound - dlStart;

		if(deltaT > 0)
		{
			if(deltaT < timeVal)
				timeVal = deltaT + 1;
		}
		else
		{
			numBlocks = SV_SendDownloadMessages();

			if(numBlocks)
			{
				// There are active downloads
				deltaT = Sys_Milliseconds() - dlStart;

				delayT = 1000 * numBlocks * MAX_DOWNLOAD_BLKSIZE;
				delayT /= sv_dlRate->integer * 1024;

				if(delayT <= deltaT + 1)
				{
					// Sending the last round of download messages
					// took too long for given rate, don't wait for
					// next round, but always enforce a 1ms delay
					// between DL message rounds so we don't hog
					// all of the bandwidth. This will result in an
					// effective maximum rate of 1MB/s per user, but the
					// low download window size limits this anyways.
					if(timeVal > 2)
						timeVal = 2;

					dlNextRound = dlStart + deltaT + 1;
				}
				else
				{
					dlNextRound = dlStart + delayT;
					delayT -= deltaT;

					if(delayT < timeVal)
						timeVal = delayT;
				}
			}
		}
	}
	else
	{
		if(SV_SendDownloadMessages())
			timeVal = 0;
	}

	return timeVal;
}
