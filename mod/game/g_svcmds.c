// Copyright (C) 1999-2000 Id Software, Inc.
//

// this file holds commands that can be executed by the server console, but not remote clients

#include "g_local.h"
#include "q_shared.h"
#include "../common/c_bmalloc.h"

char  iplist[4096 * 32];
//char	readiplist[4096 * 32];

/*
==============================================================================

PACKET FILTERING


You can add or remove addresses from the filter list with:

addip <ip>
removeip <ip>

The ip address is specified in dot format, and any unspecified digits will match any value, so you can specify an entire class C network with "addip 192.246.40".

Removeip will only remove an address specified exactly the same way.  You cannot addip a subnet, then removeip a single host.

listip
Prints the current list of filters.

g_filterban <0 or 1>

If 1 (the default), then ip addresses matching the current list will be prohibited from entering the game.	This is the default setting.

If 0, then only addresses matching the list will be allowed.  This lets you easily set up a private game, or a game that only allows players from your local network.


Urban Terror extras

  Use IP list wildcard '*' to match any number in that place, for example:
  "123.1.*.*" will match all addresses in 123.1.0.0/16.

  We added in an extra time feature, so that an IP address can expire after a certain time.
  The time is stored in minutes since 01 January for the present year.

  setipexpire	<ip> <minutes>
  only works on IPs exactly as typed, just like RemoveIP.  Minutes is minutes from
  NOW to expire the IP.  So setipexpire 123.1.1.4 180 will ban 123.1.1.4 for 3 hours
  or until January 1, whichever comes first.  By default AddIP will set the ban to
  never expire.

  Code notes: the cvar now stores IPs as aaa.bbb.ccc.ddd:123456 where the number
  after the colon specifies the minutes since 01 January that the entry expires.

  Issues:
  * all banned IPs, except for permanent ones will expire on 1 Jan each year
  * IPs banned through the referee commands are capped at 180 minutes
  * UINT_MAX is a 32-bit number, which is 4294967295, which requires 10 bytes of storage.
	This means up to 28 bytes per address in the table (16 for IP, 2 for space and : and 10 for time).
	The IP filter table can store up to 1024 entries, but the MAX string for a cvar is 1024,
	so the maximum number of bans could be as low as 35 (1024/28).	Id's implementation
	probably couldn't deal with more than around 60 IPs anyhow.  Could be optimised down
	to around 7 bytes per banned IP, but effort isn't worth it yet...




==============================================================================
*/

// extern	vmCvar_t	g_banIPs;
// extern	vmCvar_t	g_filterBan;

typedef struct ipFilter_s
{
	unsigned  mask;
	unsigned  compare;		// IP address packed into a 32-bit uint
	unsigned  unban;		// UT: yearday/hours/minutes player gets unbanned
} ipFilter_t;

#define MAX_IPFILTERS 4096	 /* was 1024 - which was unrealistic to store in a cvar
				so we'll use a text file, lawl */

static ipFilter_t  ipFilters[MAX_IPFILTERS];
static int		   numIPFilters;

static qboolean StringToFilter (char *s, ipFilter_t *f);
static void UpdateIPBans (void);
//Iain:
void Svcmd_Restart_f(qboolean DoAnotherWarmUp, qboolean KeepScores);

//
//	TimeToMins - converts a realtime into minutes elapsed since 01 January
//
int TimeToMins ( qtime_t *time )
{
	// 1440 is number of minutes in a day (24 * 60)
	return (time->tm_yday * 1440) + (time->tm_hour * 60) + time->tm_min;
}

//
//	MinsToTime - converts minutes since 01 January into a realtime
//

void MinsToTime ( const int minutes, qtime_t *result )
{
	result->tm_yday = minutes / 1440;
	result->tm_hour = (minutes % 1440) / 60;
	result->tm_min	= (result->tm_hour) % 60;
}

//
// expire bans
// Removes any IP address from the ban list
// if its time is up.  Set bantime to UINT_MAX
// if you never want it to expire.
//
void expireBans ( void )
{
	qtime_t   present;
	int 	  i;
	unsigned  minutes;

	trap_RealTime ( &present );
	minutes = TimeToMins( &present );

	for (i = 0; i < MAX_IPFILTERS; i++)
	{
		if (ipFilters[i].unban == UINT_MAX)
		{
			continue;				// permanent ban
		}

		if (ipFilters[i].unban < minutes)
		{
			// time to expire this entry
			ipFilters[i].compare = 0xffffffffu;
		}
	}

//	UpdateIPBans();
}

//
//	SetBanTime
//	Sets the time that a banned IP address
//	should be unbanned.  Time is minutes
//	since 01 January.
//	IP address should already exist in
//	the ipFilters list (eg: add with addip then call SetBanTime)
//
qboolean SetBanTime ( char *ipaddr, int minutes, qboolean write)
{
	ipFilter_t	f;
	int 	i;

	if (!StringToFilter( ipaddr, &f ))
	{
		return qfalse;
	}

	for (i = 0 ; i < numIPFilters ; i++)
	{
		if ((ipFilters[i].mask == f.mask) &&
			(ipFilters[i].compare == f.compare))
		{
			ipFilters[i].unban = minutes;
//			G_Printf ("Ban timer set.\n");

			if (write)
			{
				UpdateIPBans();
			}

			return qtrue;
		}
	}

	G_Printf ( "Didn't find %s.\n", ipaddr );
	return qfalse;
}

/*
=================
Svcmd_AddIPExpire_f
=================
*/
void Svcmd_AddIPExpire_f (void)
{
	char	 str[MAX_TOKEN_CHARS];
	int 	 minutes;
	qtime_t  present;

	if (trap_Argc() < 3)
	{
		G_Printf("Usage:  addipexpire <ip-mask> <minutes>\n");
		return;
	}

	trap_Argv( 2, str, sizeof(str));
	minutes = atoi( str );

	trap_RealTime( &present );
	minutes += TimeToMins( &present );

	trap_Argv( 1, str, sizeof(str));

	SetBanTime( str, minutes, qtrue );
}

/*
=================
StringToFilter
=================
*/
static qboolean StringToFilter (char *s, ipFilter_t *f)
{
	char  num[128];
	int   i, j;
	byte  b[4];
	byte  m[4];

	for (i = 0 ; i < 4 ; i++)
	{
		b[i] = 0;
		m[i] = 0;
	}

	for (i = 0 ; i < 4 ; i++)
	{
		if (((*s < '0') || (*s > '9')) && (*s!='*'))
		{
			G_Printf( "Bad filter address: %s\n", s );
			return qfalse;
		}

		j = 0;

		while ((*s >= '0' && *s <= '9') || *s=='*')
		{
			num[j++] = *s++;
		}
		if (num[0]=='*' && j==1) {
			m[i] = 0;
		} else {
			num[j] = 0;
			b[i] = atoi(num);
			m[i] = 255;
		}

		if (!*s)
		{
			break;
		}
		s++;
	}

	f->mask    = *(unsigned *)m;		// eg: 0 255 0 0
	f->compare = *(unsigned *)b;		// eg: 150 0 123 45 - stores as a 32-bit number

	return qtrue;
}

/*
=================
UpdateIPBans
This function sets a cvar so
that the banlist is persistant
The contents of the CVAR looks like
"120.13.45.67 12.54.245.2" - the mask isn't stored

Dok: going to modify it so we can store mins too
so format becomes
"120.13.45.67:234567 12.54.245.2:137379"


  Updated to use a textfile.
=================
*/
static void UpdateIPBans (void)
{
	byte		  b[4];
	int 		  i;
	fileHandle_t  f;

	//static char lastiplist[MAX_INFO_STRING] = "";
	if (numIPFilters == 0)
	{
		return;
	}

	memset(iplist, 0, sizeof(iplist));

	for (i = 0 ; i < numIPFilters ; i++)
	{
		if (ipFilters[i].compare == 0xffffffff)
		{
			continue;
		}

		*(unsigned *)b = ipFilters[i].compare;
		Com_sprintf( iplist + strlen(iplist), sizeof(iplist) - strlen(iplist),
				 "%i.%i.%i.%i:%i\n", b[0], b[1], b[2], b[3], ipFilters[i].unban);
	}

	//if ( strcmp ( lastiplist, iplist ) )

	//	strcpy ( lastiplist, iplist );

	//	trap_Cvar_Set( "g_banIPs", iplist );
	//}
	Com_Printf("Writing: banlist.txt\n");
	trap_FS_FOpenFile( "banlist.txt", &f, FS_WRITE );
	trap_FS_Write( iplist, strlen(iplist), f );
	trap_FS_FCloseFile( f );
}

/*
=================
G_FilterPacket

This function checks to see if
a player was banned: called from
g_client.c to ban players from connecting
=================
*/
qboolean G_FilterPacket (char *from)
{
	int 	  i;
	unsigned  in;
	byte	  m[4];
	char	  *p;

	// Don't ban localhost
	if (!strcmp(from, "localhost"))
		return qfalse;

	i = 0;
	p = from;

	while (*p && i < 4)
	{
		m[i] = 0;

		while (*p >= '0' && *p <= '9')
		{
			m[i] = m[i] * 10 + (*p - '0');
			p++;
		}

		if (!*p || (*p == ':'))
		{
			break;
		}
		i++, p++;
	}

	in = *(unsigned *)m;

	for (i = 0 ; i < numIPFilters ; i++)
	{
		if ((in & ipFilters[i].mask) == ipFilters[i].compare)
		{
			return g_filterBan.integer != 0;
		}
	}

	return g_filterBan.integer == 0;
}

/*
=================
AddIP
=================
*/
qboolean AddIP( char *str, qboolean write)
{
	int  i;

	for (i = 0 ; i < numIPFilters ; i++)
	{
		if (ipFilters[i].compare == 0xffffffff)
		{
			break;		// free spot
		}
	}

	if (i == numIPFilters)
	{
		if (numIPFilters == MAX_IPFILTERS)
		{
			G_Printf ("IP filter list is full\n");
			return qfalse;
		}
		numIPFilters++;
	}

	if (!StringToFilter (str, &ipFilters[i]))
	{
		ipFilters[i].compare = 0xffffffffu;
	}
	else
	{
		ipFilters[i].unban = UINT_MAX;	// for an AddIP, always set to permanent
		// banning: use SetBanTime() after AddIP
		// to give ban an expiry time.
	}

	if (write)
	{
		UpdateIPBans();
	}
	return qtrue;
}

/*
=================
G_ProcessIPBans

  Called from initgame in
  g_main.c at the start of
  new rounds - basically reads
  the contents of the g_banIPs cvar
  and restores the IP ban list

  UT modifies this so the
  IP is read properly given
  the extra bantime info

  Modified futher to load a banlist.txt

=================
*/
void G_ProcessIPBans(void)
{
	char *s, *t, *u, *r;
	char *end;
	char str[MAX_TOKEN_CHARS];
	fileHandle_t f;
	int length;

	length = trap_FS_FOpenFile( "banlist.txt", &f, FS_READ );

	trap_FS_Read( iplist, length, f );
	trap_FS_FCloseFile( f );
//	Q_strncpyz( str, g_banIPs.string, sizeof(str) );

	t	= iplist;
	end = iplist + length;

	while (1)
	{
		s = t;

		if (s >= end)
		{
			break; //done
		}

		while (*t != '\n' && t < end)
		{
			t++ ;
		}

		if (t - s > 1) // t contains aa.bb.cc.dd:123456; s now at beginning of next string
		{
			r = t + 1;

			memcpy(&str[0], s, t - s);
			str[t - s] = 0;

			AddIP( str, qfalse);

			{				// Urban Terror extra code: extract bantime from string
				u = strchr( str, ':' ); // set t to position of ':' character

				if (u)
				{
					SetBanTime( str, atoi(++u), qfalse);	// set IP address ban expiry time
				}
			}
		}
		t++;
	}
}

/*
=================
Svcmd_AddIP_f
=================
*/
void Svcmd_AddIP_f (void)
{
	char str[MAX_TOKEN_CHARS];
	char userinfo[MAX_INFO_STRING];
	char *value;
	int j;

	if (trap_Argc() < 2)
	{
		G_Printf("Usage:  addip <ip-mask>\n");
		return;
	}

	trap_Argv( 1, str, sizeof(str));

	if (strlen(str) < 3)
	{
		j = atoi(str);

		if ((j > -1) && (j < MAX_CLIENTS))
		{
			if (g_entities[j].inuse)
			{
				trap_GetUserinfo( j, userinfo, sizeof(userinfo));

				value = Info_ValueForKey (userinfo, "ip");
				AddIP(value, qtrue);
				trap_SendConsoleCommand( EXEC_INSERT, va("clientkick %d\n", j /*cl->ps.clientNum*/));
			}
		}
	}
	else
	{
		AddIP( str, qtrue );
	}
}

/*
=================
Svcmd_RemoveIP_f
=================
*/
void Svcmd_RemoveIP_f (void)
{
	char str[MAX_TOKEN_CHARS];

	if (trap_Argc() < 2)
	{
		G_Printf("Usage:  sv removeip <ip-mask>\n");
		return;
	}

	trap_Argv( 1, str, sizeof(str));

	RemoveIP( str );
}

// Dok: moved this into a func
qboolean RemoveIP( char *ip )
{
	ipFilter_t f;
	int i;

	if (!StringToFilter (ip, &f))
	{
		return qfalse;
	}

	for (i = 0 ; i < numIPFilters ; i++)
	{
		if ((ipFilters[i].mask == f.mask) &&
			(ipFilters[i].compare == f.compare))
		{
			ipFilters[i].compare = 0xffffffffu;
			G_Printf ("Removed.\n");

			UpdateIPBans();

			return qtrue;
		}
	}

	G_Printf ( "Didn't find %s.\n", ip );
	return qfalse;
}

/*
===================
Svcmd_EntityList_f
===================
*/
void	Svcmd_EntityList_f (void)
{
	int e;
	gentity_t *check;

	check = g_entities + 1;

	for (e = 1; e < level.num_entities ; e++, check++)
	{
		if (!check->inuse)
		{
			continue;
		}
		G_Printf("%3i:", e);

		switch (check->s.eType)
		{
			case ET_GENERAL:
				G_Printf("ET_GENERAL		  ");
				break;

			case ET_PLAYER:
				G_Printf("ET_PLAYER 	  ");
				break;

			case ET_ITEM:
				G_Printf("ET_ITEM		  ");
				break;

			case ET_MISSILE:
				G_Printf("ET_MISSILE		  ");
				break;

			case ET_MOVER:
				G_Printf("ET_MOVER		  ");
				break;

			case ET_CORPSE:
				G_Printf("ET_CORPSE 	  ");
				break;

			case ET_PORTAL:
				G_Printf("ET_PORTAL 	  ");
				break;

			case ET_SPEAKER:
				G_Printf("ET_SPEAKER		  ");
				break;

			case ET_PUSH_TRIGGER:
				G_Printf("ET_PUSH_TRIGGER	  ");
				break;

			case ET_TELEPORT_TRIGGER:
				G_Printf("ET_TELEPORT_TRIGGER ");
				break;

			case ET_INVISIBLE:
				G_Printf("ET_INVISIBLE		  ");
				break;

			default:
				G_Printf("%3i			  ", check->s.eType);
				break;
		}

		if (check->classname)
		{
			G_Printf("%s", check->classname);
		}
		G_Printf("\n");
	}
}


/////////////////////////////////////////////////////////////////////
// Name        : ClientForString
// Description : Retrieve the correct gclient_t struct given a string
// Author      : Fenix (new version)
/////////////////////////////////////////////////////////////////////
gclient_t *ClientForString(const char *s) {

    int         count = 0;
    int         idnum, i;
    char        name[64];
    gclient_t   *client;
    gclient_t   *matches[MAX_CLIENTS];

    // Checking whether s is a numeric player handle
    for(i = 0; s[i] >= '0' && s[i] <= '9'; i++);

    if (!s[i]) {

        // Numeric player handle given as input
        idnum = atoi(s);
        if ((idnum < 0) || (idnum >= level.maxclients)) {
            Com_Printf("Bad client slot: %i\n", idnum);
            return NULL;
        }

        client = &level.clients[idnum];

        if (client->pers.connected == CON_DISCONNECTED) {
            G_Printf( "Client in slot %i is not connected\n", idnum );
            return NULL;
        }

        return client;

    } else {

        // Full/Partial player name given as input
        for (i = 0 ; i < level.maxclients ; i++) {

            client = &level.clients[i];

            // Checking if the client is connected.
            if (client->pers.connected == CON_DISCONNECTED) {
                continue;
            }

            // do NOT modify netname
            strcpy(name,client->pers.netname);
            Q_CleanStr(name);

            // Check for exact match
            if (!Q_stricmp(name,s)) {
                matches[0] = &level.clients[i];
                count=1;
                break;
            }

            // Check for substring match
            if (Q_strisub(name, s)) {
                matches[count] = &level.clients[i];
                count++;
            }

        }

        if (count == 0) {

            // No match found for the given input string
            G_Printf("No client found matching %s\n", s);
            return NULL;

        } else if (count > 1) {

            // Multiple matches found for the given string
            G_Printf("Multiple clients found matching %s:\n", s);

            for (i = 0; i < count; i++) {
                client = matches[i];
                // do NOT modify netname
                strcpy(name,client->pers.netname);
                Q_CleanStr(name);
                G_Printf(" %2d: [%s]\n",
                        client->ps.clientNum,
                        name);
            }

            return NULL;

        }

        // If we got here means that we just have a match
        // for the given string. We found our client!
        return matches[0];

    }
}


/*
===================
Svcmd_ForceTeam_f

forceteam <player> <team>
===================
*/
void	Svcmd_ForceTeam_f( void )
{
	gclient_t *cl;
	char str[MAX_TOKEN_CHARS];

	// find the player
	trap_Argv( 1, str, sizeof(str));
	cl = ClientForString( str );

	if (!cl)
	{
		return;
	}

	// set the team
	trap_Argv( 2, str, sizeof(str));
	SetTeam( &g_entities[cl - level.clients], str, qfalse );
}

char *ConcatArgs( int start );

/**
 * $(function)
 */
void svcmd_ClearScores_f(void)
{
/*	int i;
	gclient_t *client;

	G_Printf("Level Times Restarting\n");

	level.startTime = level.time;

	if(g_gametype.integer == GT_UT_SURVIVOR)
	{
		level.survivorStartTime = 0;
		level.newSession = qtrue;
	};


	for (i=0; i<TEAM_NUM_TEAMS; i++)
	{
		level.teamScores[i] = 0;
	};

	for (i=0; i<level.numConnectedClients; i++)
	{
		client = &level.clients[level.sortedClients[i]];
		client->survivor.time = 0;
		client->pers.enterTime = level.time;
		client->ps.persistant[PERS_KILLED] = 0;
		client->ps.persistant[PERS_SCORE] = 0;
	}
	return; */
}

/*=---- Slaping ----=*/

void Svcmd_Slap_f(void)   // Slap <playerName>
{
	gclient_t *client;
	char str[MAX_TOKEN_CHARS];

	gentity_t *ent;
	gentity_t *te;
	int angle	 = (rand() % 10000);
	vec3_t angles;
	int velocity = (rand() % 100);
	int i;

	if(trap_Argc() < 2)
	{
		G_Printf( "Usage:  slap <id/playername>\n" );
		return;
	}
	// get client from arg1 (from name / id)
	trap_Argv(1, str, sizeof(str));
	client = ClientForString(str);

	if(!client)
	{
		return;
	}

	// Dead, Ghosts and Spectators, no slaps for them
	if(client->ghost)
	{
		G_Printf( "You can't slap ghosts!\n" );
		return;
	}
	else if(client->ps.stats[STAT_HEALTH] <= 0)
	{
		G_Printf( "You can't slap the dead!\n" );
		return;
	}
	else if(client->sess.sessionTeam == TEAM_SPECTATOR)
	{
		G_Printf( "You can't slap spectators!\n" );
		return;
	}
	// ok your slappable now lets get started!
	angles[0] = (float)( cos( angle ) * velocity ); 	  // TODO: Take from the player's current velocity
	angles[1] = (float)( sin( angle ) * velocity );
	angles[2] = 0;		  //-10;
	ent   = &g_entities[client - level.clients];
	SetClientViewAngle( ent, angles );
	VectorNormalizeNR( angles );
	G_DoKnockBack( ent, angles, 65 );
	// angles even required here? (for damage)
	G_Damage( ent, ent, ent, angles, NULL, 5, 0, UT_MOD_SLAPPED, HL_UNKNOWN );
	te		= G_TempEntity( ent->s.pos.trBase, EV_UT_SLAP_SOUND );
	te->s.clientNum = ent->s.clientNum;
	te->r.svFlags  |= SVF_BROADCAST;
	trap_SendServerCommand( client->ps.clientNum, va("cp \"^1%s ^7you've been ^3SLAPPED!\"", client->pers.netname ));

	for(i = 0; i < level.maxclients; i++)
	{
		gclient_t *cl2;
		cl2 = &level.clients[i];

		if(cl2->pers.connected != CON_CONNECTED)
		{
			continue;
		}

		// Print out if your not the client or if your following the client...
		if((cl2->ps.clientNum == client->ps.clientNum) && !(cl2->ps.pm_flags & PMF_FOLLOW))
		{
			continue;
		}
		trap_SendServerCommand( cl2->ps.clientNum, va("cp \"^1%s ^7was ^3SLAPPED!^7 by the Admin\"", client->pers.netname ));
	}
}

/*=--- Prints out Big Text ----=*/

void Svcmd_BigText_f(void)	 // bigtext <text>
{ char str[MAX_TOKEN_CHARS];

  if(trap_Argc() < 2)
  {
	  G_Printf( "Usage:  bigtext <string>\n" );
	  return;
  }
  trap_Argv( 1, str, sizeof(str));
  trap_SendServerCommand( -1, va( "cp \"%s\"", str ));
}

/**
 * $(function)
 */
void Svcmd_Pause_f(void)
{
	// If we're paused.
	if (level.pauseState & UT_PAUSE_ON)
	{
		// If we're getting ready to resume.
		if (level.pauseState & UT_PAUSE_TR)
		{
			return;
		}

		// Set flag.
		level.pauseState |= UT_PAUSE_TR;

		// Set time to resume.
		level.pauseTime = level.time + 10000;

		// Set config string.
		trap_SetConfigstring(CS_PAUSE_STATE, va("%d %i %i", level.pauseState, level.pauseTime, level.pausedTime));
	}
	else
	{
		// If we're getting ready to pause.
		if (level.pauseState & UT_PAUSE_TR)
		{
			return;
		}

		// Set flags.
		level.pauseState |= UT_PAUSE_TR;
		level.pauseState |= UT_PAUSE_AC;
		level.pauseState &= ~UT_PAUSE_RC;
		level.pauseState &= ~UT_PAUSE_BC;

		// Set time to pause.
		level.pauseTime = level.time + 3000;

		// Set config string.
		trap_SetConfigstring(CS_PAUSE_STATE, va("%d %i %i", level.pauseState, level.pauseTime, level.pausedTime));
	}
}

//Swaps the clients over
void Svcmd_SwapClients(void)
{
	char redName[MAX_NAME_LENGTH];
	char blueName[MAX_NAME_LENGTH];
	int i;
	int redscore;

	// If we're not playing a team based gametype.
	if ((g_gametype.integer < GT_TEAM) || (g_gametype.integer == GT_JUMP))
	{
		return;
	}

	// Shuffle appropriate clients.
	for (i = 0; i < level.maxclients; i++)
	{
		gentity_t *other = &g_entities[i];

		// If client is not connected.
		if (other->client->pers.connected != CON_CONNECTED)
		{
			continue;
		}

		// If client is a spectator.
		if (other->client->sess.sessionTeam == TEAM_SPECTATOR)
		{
			continue;
		}

		// If client is on the red team.
		if (other->client->sess.sessionTeam == TEAM_RED)
		{
			// Switch him to the blue team.
			other->client->sess.pendingTeam = TEAM_BLUE;
			other->client->sess.queSwitch	= level.time + ((i % 4) * 500);
			//SetTeam(other, "blue",qtrue);
		}
		else if (other->client->sess.sessionTeam == TEAM_BLUE)
		{
			// Switch him to the red team.
			other->client->sess.pendingTeam = TEAM_RED;
			other->client->sess.queSwitch	= level.time + ((i % 4) * 500);
			//SetTeam(other, "red",qtrue);
		}
	}

	//swap the teamscores
	redscore			= level.teamScores[TEAM_RED];
	level.teamScores[TEAM_RED]	= level.teamScores[TEAM_BLUE];
	level.teamScores[TEAM_BLUE] = redscore;

	// If we're in match mode.
	if (level.matchState & UT_MATCH_ON)
	{
		// Get red team name.
		trap_Cvar_VariableStringBuffer("g_nameRed", redName, sizeof(redName));

		// Get blue team name.
		trap_Cvar_VariableStringBuffer("g_nameBlue", blueName, sizeof(blueName));

		// If names are set.
		if (strlen(redName) && strlen(blueName))
		{
			// Set new red name.
			trap_Cvar_Set("g_nameRed", blueName);

			// Set config string.
//			trap_SetConfigstring(CS_TEAM_NAME_RED, blueName);

			// Set new blue name.
			trap_Cvar_Set("g_nameBlue", redName);

			// Set config string.
//			trap_SetConfigstring(CS_TEAM_NAME_BLUE, redName);
		}
	}

	// Notify clients.
	trap_SendServerCommand(-1, "cp \"Teams are now swapped.\n\"");
}

/**
 * $(function)
 */
void Svcmd_SwapTeams_f(void)
{
	//Swap the clients
	Svcmd_SwapClients();

	// Set warmup time.
	level.warmupTime = level.time + ((g_warmup.integer) * 1000);

	// If warmup time is too short.
	if (level.warmupTime < level.time + 4000)
	{
		level.warmupTime = level.time + 4000;
	}

	// Set config string.
	trap_SetConfigstring(CS_WARMUP, va("%d", level.warmupTime));
}

/**
 * $(function)
 */
void Svcmd_ShuffleTeams_f(void)
{
	int numRedPlayers, numBluePlayers;
	int redTeamMembers[MAX_CLIENTS];
	int blueTeamMembers[MAX_CLIENTS];
	int i;

	// If we're not playing a team based gametype.
	if ((g_gametype.integer < GT_TEAM) || (g_gametype.integer == GT_JUMP))
	{
		return;
	}

	numRedPlayers  = 0;
	numBluePlayers = 0;

	// Sort players based on team.
	for (i = 0; i < level.maxclients; i++)
	{
		gentity_t *other = &g_entities[i];

		// If client is not connected.
		if (other->client->pers.connected != CON_CONNECTED)
		{
			continue;
		}

		// If client is a spectator.
		if (other->client->sess.sessionTeam == TEAM_SPECTATOR)
		{
			continue;
		}

		// If client is on the red team.
		if (other->client->sess.sessionTeam == TEAM_RED)
		{
			redTeamMembers[numRedPlayers++] = i;
		}
		else if (other->client->sess.sessionTeam == TEAM_BLUE)
		{
			blueTeamMembers[numBluePlayers++] = i;
		}
	}

	// Loop through team players.
	for (i = 0; i < (numRedPlayers < numBluePlayers ? numRedPlayers : numBluePlayers); i++)
	{
		// If we should switch players team.
		if (rand() & 2)
		{
			// Move player to the red team.
			g_entities[blueTeamMembers[i]].client->sess.pendingTeam = TEAM_RED;
			SetTeam(&g_entities[blueTeamMembers[i]], "red", qtrue);

			// Move player to the blue team.
			g_entities[redTeamMembers[i]].client->sess.pendingTeam = TEAM_BLUE;
			SetTeam(&g_entities[redTeamMembers[i]], "blue", qtrue);
		}
	}

	// If there are any unshuffled players on the red team.
	if (i < numRedPlayers)
	{
		// Loop through unshuffled red players.
		for (; i < numRedPlayers; i++)
		{
			// If we should switch player team.
			if (rand() & 2)
			{
				g_entities[redTeamMembers[i]].client->sess.pendingTeam = TEAM_BLUE;
				SetTeam(&g_entities[redTeamMembers[i]], "blue", qtrue);
			}
		}
	}
	else if (i < numBluePlayers)
	{
		// Loop through unshuffled blue players.
		for (; i < numBluePlayers; i++)
		{
			// If we should switch player team.
			if (rand() & 2)
			{
				g_entities[blueTeamMembers[i]].client->sess.pendingTeam = TEAM_RED;
				SetTeam(&g_entities[blueTeamMembers[i]], "red", qtrue);
			}
		}
	}

	if (!g_shuffleNoRestart.integer) {
		// Set warmup time.
		level.warmupTime = level.time + ((g_warmup.integer) * 1000);

		// If warmup time is too short.
		if (level.warmupTime < level.time + 4000)
		{
			level.warmupTime = level.time + 4000;
		}

		// Set config string.
		trap_SetConfigstring(CS_WARMUP, va("%d", level.warmupTime));
	}

	// Notify clients.
	trap_SendServerCommand(-1, "cp \"Teams are now shuffled.\n\"");
}

/**
 * $(function)
 */
void Svcmd_ForceReady_f(void)
{
	// If we're not playing a team based game type.
	if ((g_gametype.integer < GT_TEAM) || (g_gametype.integer == GT_JUMP))
	{
		return;
	}

	// If we're not in match mode.
	if (!(level.matchState & UT_MATCH_ON))
	{
		return;
	}

	// If game is paused.
	if (level.pauseState & UT_PAUSE_ON)
	{
		return;
	}

	// If not enough players.
	if ((TeamCount(-1, TEAM_RED) < 1) || (TeamCount(-1, TEAM_BLUE) < 1))
	{
		return;
	}

	// If both teams are ready.
	if ((level.matchState & UT_MATCH_RR) && (level.matchState & UT_MATCH_BR))
	{
		return;
	}

	// Set ready flags.
	level.matchState |= UT_MATCH_RR;
	level.matchState |= UT_MATCH_BR;

	// Set config string.
	trap_SetConfigstring(CS_MATCH_STATE, va("%d", level.matchState));

	// Restart.
	Svcmd_Restart_f(qtrue, qfalse);
}

/**
 * $(function)
 */
void Svcmd_Restart_Times(void)
{
	// Reset times.
	level.startTime 	= level.time;
	level.survivorStartTime = level.time;
	level.pausedTime	= 0;

	// Clear timeouts.
	level.numRedTimeouts  = 0;
	level.numBlueTimeouts = 0;

	// No count down please, kthxggbye.
	level.warmupIntro = qfalse;
	level.warmupTime  = 0;

	// Clear round count
	level.g_roundcount = 0;
}

/**
 * $(function)
 */
void Svcmd_Restart_f(qboolean DoAnotherWarmUp, qboolean KeepScores)
{
	gclient_t *client;
	int 	  i;

	//Set the times bacl
	Svcmd_Restart_Times();

	if (KeepScores == qfalse)
	{
		//Wipe stats
		Stats_ClearAll();

		// Loop through teams.
		for (i = 0; i < TEAM_NUM_TEAMS; i++)
		{
			// Reset score.
			level.teamScores[i] = 0;
		}

		// Loop through clients.
		for (i = 0; i < level.maxclients; i++)
		{
			// Set pointer.
			client = &level.clients[i];

			// If client not connected.
			if (client->pers.connected != CON_CONNECTED)
			{
				continue;
			}

			// Reset score.
			client->survivor.time		   = 0;
			client->pers.enterTime		   = level.time;
			client->ps.persistant[PERS_SCORE]  = 0;
			client->ps.persistant[PERS_KILLED] = 0;
			client->ps.persistant[PERS_JUMPSTARTTIME]  = 0;
			client->ps.persistant[PERS_JUMPBESTTIME] = 0;

		}
	}

	// Restart everything.
	G_InitWorldSession();

	if (DoAnotherWarmUp)
	{
		// If this is a team based game type.
		if ((g_gametype.integer >= GT_TEAM) && (g_gametype.integer != GT_JUMP))
		{
			// Set warmup time.
			level.warmupTime = level.time + ((g_warmup.integer) * 1000);

			// If warmup time is too short.
			if (level.warmupTime < level.time + 4000)
			{
				level.warmupTime = level.time + 4000;
			}

			// Set config string.
			trap_SetConfigstring(CS_WARMUP, va("%d", level.warmupTime));
		}
	}
	else
	{
		// If this is a team based game type.
		/*	if (g_gametype.integer >= GT_TEAM)
			{
				// Set warmup time.
				level.warmupTime = level.time;

				// Set config string.
				trap_SetConfigstring(CS_WARMUP, va("%d", level.warmupTime+level.timechop));
			}

		*/
	}
	//trap_SendConsoleCommand( EXEC_APPEND, "map_restart 0\n" );
}

/**
 * $(function)
 */
void Svcmd_SwapRoles(void)
{
	Svcmd_SwapClients();

	//Keep the scores in pub play, else clear em for matchmode
	if (g_matchMode.integer)
	{
		//@Barbatos: unready both teams!
		level.matchState &= ~UT_MATCH_RR;
		level.matchState &= ~UT_MATCH_BR;

		Svcmd_Restart_f(qtrue, qfalse);
	}
	else
	{
		Svcmd_Restart_f(qtrue, qtrue);
	}

/*	 Svcmd_Restart_Times();
	 G_InitWorldSession();*/
}

/**
 * $(function)
 */
void Svcmd_Reload_f(void)
{
	char mapName[MAX_QPATH];

	// Get map name.
	trap_Cvar_VariableStringBuffer("mapname", mapName, sizeof(mapName));

	// Reload map.
	trap_SendConsoleCommand(EXEC_APPEND, va("map %s\n", mapName));
}

/**
 * $(function)
 */
void Svcmd_ForceTeamName_f(void)
{
	char arg[MAX_TOKEN_CHARS];
	char newName[MAX_NAME_LENGTH];

	// If this is not a team based game type.
	if ((g_gametype.integer < GT_TEAM) || (g_gametype.integer == GT_JUMP))
	{
		return;
	}

	// If not enough arguments.
	if (trap_Argc() < 2)
	{
		return;
	}

	// Get first argument.
	trap_Argv(1, arg, sizeof(arg));

	// Red team.
	if (!Q_stricmp(arg, "red"))
	{
		// Get new name.
		Com_sprintf(newName, sizeof(newName), "%s", ConcatArgs(2));

		// Set cvar.
		trap_Cvar_Set("g_nameRed", newName);

		// Set config string.
//		trap_SetConfigstring(CS_TEAM_NAME_RED, newName);
	}
	// Blue team.
	else if (!Q_stricmp(arg, "blue"))
	{
		// Get new name.
		Com_sprintf(newName, sizeof(newName), "%s", ConcatArgs(2));

		// Set cvar.
		trap_Cvar_Set("g_nameBlue", newName);

		// Set config string.
		//trap_SetConfigstring(CS_TEAM_NAME_BLUE, newName);
	}
}

// The Nuke Command.
// Do not use if you have a heart condition or while nursing.
void Svcmd_Nuke_f(void)
{
	gclient_t *client;
	char	  str[MAX_TOKEN_CHARS];

	gentity_t *ent;
	gentity_t *missile;
	gentity_t *airplane;
	vec3_t	  up, dir, vec, temp;
	trace_t   tr;
	int 	  angle;
	float	  dist;
	vec3_t	  missileOrigin;
	vec3_t	  missileDirection;
	float	  speed;

	// If not enough arguments.
	if (trap_Argc() < 2)
	{
		G_Printf( "Usage: nuke <id/playername>\n" );
		return;
	}
	// get client from arg1 (from name / id)
	trap_Argv(1, str, sizeof(str));
	client = ClientForString(str);

	// If an invalid client.
	if (!client)
	{
		return;
	}

	// Dead, Ghosts and Spectators, no nuke for them
	if(client->ghost)
	{
		G_Printf( "You can't nuke ghosts!\n" );
		return;
	}
	else if(client->ps.stats[STAT_HEALTH] <= 0)
	{
		G_Printf( "You can't nuke the dead!\n" );
		return;
	}
	else if(client->sess.sessionTeam == TEAM_SPECTATOR)
	{
		G_Printf( "You can't nuke spectators!\n" );
		return;
	}

	// If we're in match mode.
	if (level.matchState & UT_MATCH_ON)
	{
		G_Printf( "You can't nuke during a match.\n" );
		return;
	}

	// If it's too soon for another nuke.
	if (level.time - level.lastNukeTime < 2500)
	{
		G_Printf( "Please wait for your last nuke to reach the target.\n" );
		return;
	}

	// Up vector.
	up[0] = 0;
	up[1] = 0;
	up[2] = 1;

	// Direction vector.
	dir[0] = 0.6f;
	dir[1] = 0;
	dir[2] = 0.3f;

	dist   = -1;

	for (angle = 0; angle < 360; angle += 15)
	{
		// Calculate new direction.
		RotatePointAroundVector(vec, up, dir, angle);

		// Calculate end position.
		VectorMA(client->ps.origin, 16384, vec, temp);

		// Trace.
		trap_Trace(&tr, client->ps.origin, NULL, NULL, temp, client->ps.clientNum, MASK_SHOT);

		// If we hit the sky.
		if (SURFACE_GET_TYPE(tr.surfaceFlags) == 0)
		{
			float d = Distance(client->ps.origin, tr.endpos);

			if (d > dist)
			{
				dist = d;
				VectorCopy(tr.endpos, missileOrigin);
				VectorScale(vec, -1, missileDirection);
			}
		}
	}

	if (dist == -1)
	{
		G_Printf( "We cannot find a lock on the target!\n" );
		return;
	}

	// Create airplane flyby sound event.
	ent 					= &g_entities[client - level.clients];
	airplane				= G_TempEntity(missileOrigin, EV_UT_NUKE_SOUND);
	airplane->s.clientNum	= ent->s.clientNum;
	airplane->r.svFlags 	|= SVF_BROADCAST;

	// Nuke the bastard; use a modified hk69 nade.
	missile 				 = G_Spawn();
	missile->classname		 = "grenade";
	missile->classhash		 = HSH_grenade;
	missile->nextthink		 = level.time + 15000;
	missile->think			 = G_ExplodeMissile;
	missile->s.eType		 = ET_MISSILE;
	missile->r.svFlags		 = SVF_USE_CURRENT_ORIGIN;
	missile->s.weapon		 = UT_WP_HK69;
	missile->r.ownerNum 	 = -1;								// -1 for flag		// ent->s.number for HK / HE
	missile->parent 		 = ent; 								// NULL for flag	// ent for HK / HE
	missile->damage 		 = (int)((float)UT_MAX_HEALTH / 2 * 	// 200 for flag 	// (int)((float)UT_MAX_HEALTH / 2 * 1.8f)
			bg_weaponlist[UT_WP_HK69].damage[HL_UNKNOWN].value);							 //(int)((float)UT_MAX_HEALTH / 2 * bg_weaponlist[ent->s.weapon].damage[HL_UNKNOWN].value) for HE
	missile->splashDamage		 = (int)((float)UT_MAX_HEALTH * 		// 200 for flag 	// (int)((float)UT_MAX_HEALTH * 1.8f)
			bg_weaponlist[UT_WP_HK69].damage[HL_UNKNOWN].value);							//(int)((float)UT_MAX_HEALTH * 2 * bg_weaponlist[ent->s.weapon].damage[HL_UNKNOWN].value) for HE
	missile->splashRadius		 = 300; //@Barbatos - previously 300	// 300 for flag 	// 300 for HK	// 235 for HE
	missile->methodOfDeath		 = UT_MOD_NUKED;
	missile->splashMethodOfDeath = UT_MOD_NUKED;
	missile->clipmask			 = MASK_SHOT;
	missile->target_ent 		 = NULL;								// NULL for flag	// NULL
	missile->s.pos.trType		 = TR_LINEAR;
	missile->r.svFlags			|= SVF_BROADCAST;

	missile->s.pos.trTime		 = level.time - 50;

	// maybe useless ...
	missile->client 			 = NULL;

	VectorCopy(missileOrigin, missile->s.pos.trBase);
	VectorCopy(missileOrigin, missile->r.currentOrigin);

	// Calculate required speed for a 3sec flight.
	speed = Distance(missileOrigin, client->ps.origin) / 3;

	if (speed < 600)
	{
		speed = 600;
	}

	VectorScale(missileDirection, speed, missile->s.pos.trDelta);

	missile->s.pos.trDelta[0] += client->ps.velocity[0];
	missile->s.pos.trDelta[1] += client->ps.velocity[1];

	SnapVector(missile->s.pos.trDelta);

	// Set current time.
	level.lastNukeTime = level.time;
}

/**
 * $(function)
 */
void Svcmd_ClientKickReason_f(void) {

	char arg1[MAX_TOKEN_CHARS];
	char arg2[MAX_TOKEN_CHARS];

	if (trap_Argc() < 2) {
		G_Printf("Usage: clientkickreason <client number> [<reason>]\n");
		return;
	}

	trap_Argv(1, arg1, sizeof(arg1));
	trap_Argv(2, arg2, sizeof(arg2));

	if (strlen(arg2) == 0) {
		Com_sprintf(arg2, sizeof(arg2), "was kicked");
	} else {
		Com_sprintf(arg2, sizeof(arg2), "was kicked: %s", arg2);
	}

	// Dropping the client.
	trap_DropClient(atoi(arg1), arg2);
}

/**
 * $(function)
 */
void Svcmd_ClientList_f(void)
{
	gentity_t *entity;
	gclient_t *client;
	char *value;
	char userinfo[MAX_INFO_STRING];
	int i;

	// Loop through clients.
	for (i = 0; i < level.maxclients; i++)
	{
		// Get entity pointer.
		entity = &g_entities[i];

		// If an invalid client.
		if (!entity->client || (entity->r.svFlags & SVF_BOT))
		{
			continue;
		}

		// Get client pointer.
		client = entity->client;

		// If client not connected.
		if (client->pers.connected != CON_CONNECTED)
		{
			continue;
		}

		// Get client information.
		trap_GetUserinfo(client - level.clients, userinfo, sizeof(userinfo));

		// Get client guid.
		value = Info_ValueForKey(userinfo, "cl_guid");

		// If not a valid guid.
		if (!value || !value[0])
		{
			G_Printf("%d %s\n", client - level.clients, client->pers.netname);
		}
		else
		{
			G_Printf("%s %d %s\n", value, client - level.clients, client->pers.netname);
		}
	}
}

/**
 * $(function)
 */
void Svcmd_Playerlist_f(void)
{
	int i;
	gclient_t *client;
	char *value;
	char mapname[256];
	//char configbuf[256];
	char userinfo[MAX_INFO_STRING];
	int maxclients;

	trap_Cvar_VariableStringBuffer( "mapname", mapname, sizeof(mapname));

	Com_Printf("Map: %s\n", mapname);

	Com_Printf("Players: %d\n", level.numConnectedClients);

	if ((g_gametype.integer >= GT_TEAM) && (g_gametype.integer != GT_JUMP))
	{
		Com_Printf("Scores: R:%d B:%d\n", level.teamScores[TEAM_RED], level.teamScores[TEAM_BLUE]);
	}

	maxclients = trap_Cvar_VariableIntegerValue("sv_maxclients");

	for(i = 0, client = level.clients; i < maxclients; i++, client++)
	{
		//trap_GetConfigstring(CS_PLAYERS + client->ps.clientNum, configbuf, sizeof(configbuf));
		//	 if (strlen(configbuf) == 0) continue;
		if (client->pers.connected == CON_DISCONNECTED)
		{
			continue;
		}
		trap_GetUserinfo( client->ps.clientNum, userinfo, sizeof(userinfo));
		value = Info_ValueForKey (userinfo, "ip");

		if (strlen(value) > 0)
		{
			Com_Printf("%d: %s %s k:%d d:%d ping:%d %s\n", client->ps.clientNum, client->pers.netname, TeamName(client->sess.sessionTeam), client->ps.persistant[PERS_SCORE], client->ps.persistant[PERS_KILLED], client->ps.ping, value );
		}
		else
		{
			Com_Printf("[connecting] %s\n", client->pers.netname );
		}
	}
}

//@Barbatos: rcon command asked by RaideR to kill someone!

void Svcmd_Smite_f(void)
{
	gclient_t *client;
	gentity_t *ent;
	char str[MAX_TOKEN_CHARS];

	if(trap_Argc() < 2)
	{
		G_Printf( "Usage: smite <player>\n" );
		return;
	}

	// If we're in match mode.
	/*if (level.matchState & UT_MATCH_ON)
	{
		return;
	}*/

	// If not enough arguments.
	if (trap_Argc() < 2)
	{
		return;
	}

	// Get client name.
	trap_Argv(1, str, sizeof(str));

	// Get client from name.
	client = ClientForString(str);

	// If an invalid client.
	if (!client)
	{
		return;
	}

	// If this client is dead, a spectator or a substitute.
	if (client->ghost || (client->ps.stats[STAT_HEALTH] <= 0) || (client->sess.sessionTeam == TEAM_SPECTATOR) || (client->pers.substitute))
	{
		return;
	}

	/* If game is paused. */
	if (level.pauseState & UT_PAUSE_ON) {
		return;
	}

	ent = &g_entities[client - level.clients];
	client->ps.stats[STAT_HEALTH] = ent->health = -999;
	player_die(ent, ent, ent, 100000, UT_MOD_SMITED);

	trap_SendServerCommand( client->ps.clientNum, va("cp \"^1%s ^7you've been ^3SMITTEN!\"", client->pers.netname ));
}


//@Fenix - refactored mute command.
void Svcmd_Mute_f(void) {

	gclient_t *cl;
	char arg1[MAX_TOKEN_CHARS];
	char arg2[MAX_TOKEN_CHARS];
	char *suffix;
	int duration;
	int i;

	if ((trap_Argc() != 2) && (trap_Argc() != 3)) {
		G_Printf("Usage: mute <player> [<seconds>]\n");
		return;
	}

	// Retrieve the client handle.
	trap_Argv(1, arg1, sizeof(arg1));

	// Get the client structure
	cl = ClientForString(arg1);

	if (!cl) {
		// @Fenix - not showing any message.
		// ClientForString will handle this.
		return;
	}

	if (trap_Argc() == 3) {
		// Retrieve the specified duration and checking
		// for a proper input value (only digits).
		trap_Argv(2, arg2, sizeof(arg2));
		for(i=0; arg2[i]>='0' && arg2[i]<='9'; i++);

		if (!arg2[i]) {
			// Converting the given duration.
			// NOTE: negative duration will be converted to 0
			// in order to prevent possible future bugs.
			duration = atoi(arg2);
			if (duration < 0) duration = 0;

			if (duration > 0) {
				// Specified a positive duration value.
				// we are going to mute the player in this case.
				// This will also overwrite past player temp mute.
				cl->sess.muteTimer = level.time + duration * 1000;
				if (duration < 60) { suffix = (duration == 1) ? "second" : "seconds"; }
				else { suffix = ((duration = (int)ceil(duration/60)) == 1) ? "minute" : "minutes"; }
				trap_SendServerCommand(cl->ps.clientNum, "print \"The admin muted you: you cannot talk\"\n");
				trap_SendServerCommand(-1, va( "print \"%s has been ^1muted ^7by the admin for ^1%d ^7%s\n\"", cl->pers.netname, duration, suffix));

			} else {
				// Negative or 0 duration specified.
				// Ensure the player unmute.
				cl->sess.muteTimer = 0;
				trap_SendServerCommand(cl->ps.clientNum, "print \"The admin unmuted you\"\n");
				trap_SendServerCommand(-1, va("print \"%s has been ^2unmuted ^7by the admin\n\"", cl->pers.netname));
			}

		} else {
			G_Printf("Usage: mute <player> [<seconds>]\n");
			return;
		}

	} else {

		// We have no duration specified.
		if (cl->sess.muteTimer > 0) {
			// Unmuting the player.
			cl->sess.muteTimer = 0;
			trap_SendServerCommand(cl->ps.clientNum, "print \"The admin unmuted you\"\n");
			trap_SendServerCommand(-1, va("print \"%s has been ^2unmuted ^7by the admin\n\"", cl->pers.netname));
		} else {
			// Muting the player.
			// Using a very long mute expire time:
			// 2000000 seconds -> almost 556 hours xD.
			cl->sess.muteTimer = level.time + 2000000 * 1000;
			trap_SendServerCommand(cl->ps.clientNum, "print \"The admin muted you: you cannot talk\"\n");
			trap_SendServerCommand(-1, va( "print \"%s has been ^1muted ^7by the admin\n\"", cl->pers.netname));
		}

	}
}


/*
=================
ConsoleCommand

=================
*/
qboolean	ConsoleCommand( void )
{
	char cmd[MAX_TOKEN_CHARS];

	trap_Argv( 0, cmd, sizeof(cmd));

	if (Q_stricmp (cmd, "entitylist") == 0)
	{
		Svcmd_EntityList_f();
		return qtrue;
	}

	if (Q_stricmp (cmd, "forceteam") == 0)
	{
		Svcmd_ForceTeam_f();
		return qtrue;
	}

	if (Q_stricmp (cmd, "game_memory") == 0)
	{
		print_lists();
		return qtrue;
	}

	if (Q_stricmp (cmd, "addbot") == 0)
	{
		Svcmd_AddBot_f();
		return qtrue;
	}

	if (Q_stricmp (cmd, "botlist") == 0)
	{
		Svcmd_BotList_f();
		return qtrue;
	}

	if (Q_stricmp (cmd, "addip") == 0)
	{
		Svcmd_AddIP_f();
		return qtrue;
	}

	if (Q_stricmp (cmd, "removeip") == 0)
	{
		Svcmd_RemoveIP_f();
		return qtrue;
	}

	if (Q_stricmp (cmd, "addipexpire") == 0)
	{
		Svcmd_AddIPExpire_f();
		return qtrue;
	}

//	if (Q_stricmp (cmd, "listip") == 0) {
//		trap_SendConsoleCommand( EXEC_INSERT, "g_banIPs\n" );
//		return qtrue;
//	}

	if (Q_stricmp (cmd, "cyclemap" ) == 0)
	{
		g_CycleMap();
		//utMapCycleGotoNext ( );
		return qtrue;
	}

	if(Q_stricmp (cmd, "testwrite" ) == 0) // Removed for now

	{ //   char filename[512];
//	 char buffer[20000];
//		fileHandle_t f = 0;

		/*tPlayerStats test;
		memset(&test,0,sizeof(tPlayerStats));

		tPlayerStat_Write(&test,&buffer[0],4096);


		Com_sprintf(filename,256,"test.txt");
			 trap_FS_FOpenFile( filename, &f, FS_WRITE );

		trap_FS_Write(&buffer[0],strlen(buffer),f);
		trap_FS_FCloseFile(f);

		*/
		return qtrue;
	}

	//
	// DensitY: Added this crap below :p
	//
	if(Q_stricmp( cmd, "bigtext" ) == 0)
	{
		Svcmd_BigText_f();
		return qtrue;
	}

	if(Q_stricmp( cmd, "slap" ) == 0)
	{
		Svcmd_Slap_f();
		return qtrue;
	}

	if(Q_stricmp( cmd, "players" ) == 0)
	{
		Svcmd_Playerlist_f();
		return qtrue;
	}

	if(Q_stricmp( cmd, "veto" ) == 0)
	{
		CheckVote(qtrue);
		return qtrue;
	}

	if(Q_stricmp( cmd, "mute" ) == 0)
	{
		Svcmd_Mute_f();
		return qtrue;
	}

	if(Q_stricmp( cmd, "pause" ) == 0)
	{
		Svcmd_Pause_f();
		return qtrue;
	}

	if(Q_stricmp( cmd, "swapteams" ) == 0)
	{
		Svcmd_SwapTeams_f();
		return qtrue;
	}

	if(Q_stricmp( cmd, "shuffleteams" ) == 0)
	{
		Svcmd_ShuffleTeams_f();
		return qtrue;
	}

	if (Q_stricmp( cmd, "forceready" ) == 0)
	{
		Svcmd_ForceReady_f();
		return qtrue;
	}

	if (Q_stricmp( cmd, "restart" ) == 0)
	{
		level.matchState &= ~UT_MATCH_RR;
		level.matchState &= ~UT_MATCH_BR;
		// Set config string.
		trap_SetConfigstring(CS_MATCH_STATE, va("%d", level.matchState));
		Svcmd_Restart_f(qtrue, qfalse);
		return qtrue;
	}

	if (Q_stricmp( cmd, "reload" ) == 0)
	{
		Svcmd_Reload_f();
		return qtrue;
	}

	if (Q_stricmp( cmd, "forceteamname" ) == 0)
	{
		Svcmd_ForceTeamName_f();
		return qtrue;
	}

	if (Q_stricmp( cmd, "nuke" ) == 0)
	{
		Svcmd_Nuke_f();
		return qtrue;
	}

	//@Barbatos: smite command added
	if( Q_stricmp ( cmd, "smite" ) == 0)
	{
		Svcmd_Smite_f();
		return qtrue;
	}

	if (Q_stricmp( cmd, "clientlist" ) == 0)
	{
		Svcmd_ClientList_f();
		return qtrue;
	}

	if (Q_stricmp( cmd, "clientkickreason" ) == 0)
	{
		Svcmd_ClientKickReason_f();
		return qtrue;
	}

	if (g_dedicated.integer)
	{
		if (Q_stricmp (cmd, "say") == 0)
		{
			//G_Printf("HEY WE GOT A SAY");
			trap_SendServerCommand( -1, va("print \"^7server:%s %s\"", S_COLOR_YELLOW, ConcatArgs(1)));
			return qtrue;
		}
		if (g_dedAutoChat.integer) {
			// everything else will also be printed as a say command
			trap_SendServerCommand( -1, va("print \"^7server:%s %s\"", S_COLOR_YELLOW, ConcatArgs(0)));
			return qtrue;
		}
	}

	return qfalse;
}
