// Copyright (C) 1999-2000 Id Software, Inc.
//
#include "g_local.h"
#include "../common/c_bmalloc.h"

// g_client.c -- client functions that don't happen every frame


#ifdef USE_AUTH
//@Barbatos //@Kalish

#include "../game/auth_shared.c"


/*
================
SV_Auth_Print_User_Infos_f
================
*/
void SV_Auth_Print_User_Infos_f( gclient_t *cl, int target )
{
	char	msg[2048];
	char	addon[32];
	char	mlevel[128];

	if(target == 1)
		Q_strncpyz(addon, AUTH_PM_PREFIX, sizeof(addon));
	else
		addon[0] = '\0';

	if( strlen(cl->sess.auth_login) < 2 )
	{
		switch( cl->sess.auth_level )
		{
			case -1:
				mlevel[0] = '\0';
				break;
			case 0:
				Q_strncpyz(mlevel, " - " S_COLOR_WHITE "no account", sizeof(mlevel));
				break;
			case 1:
				Q_strncpyz(mlevel, " - " S_COLOR_RED "auth server disabled", sizeof(mlevel));
				break;
			case 2:
				Q_strncpyz(mlevel, " - " S_COLOR_RED "this server is banned", sizeof(mlevel));
				break;
			default:
				Q_strncpyz(mlevel, " - " S_COLOR_RED "ERROR", sizeof(mlevel));
		}
		Com_sprintf( msg, sizeof(msg), "%s" AUTH_PRINT_PREFIX S_COLOR_WHITE "%s" S_COLOR_WHITE "%s" S_COLOR_WHITE " ", addon, cl->pers.netname, mlevel );
	}
	else
	{
		switch( cl->sess.auth_level )
		{
			case -1:
				mlevel[0] = '\0';
				break;
			case 0:
				Q_strncpyz(mlevel, " - " S_COLOR_BLACK "Disabled", sizeof(mlevel));
				break;
			case 1:
				Q_strncpyz(mlevel, " - " S_COLOR_WHITE "Registered", sizeof(mlevel));
				break;
			case 2:
				Q_strncpyz(mlevel, " - " S_COLOR_YELLOW "Friend", sizeof(mlevel));
				break;
			case 3:
				Q_strncpyz(mlevel, " - " S_COLOR_BLUE "Member", sizeof(mlevel));
				break;
			case 4:
				Q_strncpyz(mlevel, " - " S_COLOR_GREEN "Referee", sizeof(mlevel));
				break;
			case 5:
				Q_strncpyz(mlevel, " - " S_COLOR_RED "Admin", sizeof(mlevel));
				break;
			case 6:
				Q_strncpyz(mlevel, " - " S_COLOR_MAGENTA "Head Admin", sizeof(mlevel));
				break;
			case 7:
				Q_strncpyz(mlevel, " - " S_COLOR_CYAN "Accounts Admin", sizeof(mlevel));
				break;
			case 8:
				Q_strncpyz(mlevel, " - " S_COLOR_CYAN "Frozen Sand Developer", sizeof(mlevel));
				break;
			default:
				Q_strncpyz(mlevel, " - " S_COLOR_CYAN "ERROR", sizeof(mlevel));
		}
		Com_sprintf( msg, sizeof(msg), "%s" AUTH_PRINT_PREFIX S_COLOR_WHITE "%s" S_COLOR_WHITE "%s" S_COLOR_WHITE " - %s account: " S_COLOR_WHITE "%s" S_COLOR_WHITE " ",
			addon, cl->pers.netname, mlevel, cl->sess.auth_notoriety, cl->sess.auth_name  );
	}

	if(target==1) // PM
	{
		trap_SendServerCommand( cl->ps.clientNum, va("print \"%s\"", msg ));
	}
	else if(target==2) // RCON
	{
		Com_Printf( AUTH_PREFIX" id: %i - name: " S_COLOR_WHITE "%s - login: %s - notoriety: %s - level: %i %s \n",
			cl->ps.clientNum, cl->pers.netname, cl->sess.auth_login, cl->sess.auth_notoriety, cl->sess.auth_level, mlevel );
	}
	else if( auth_verbosity.integer == 1 ) // TOP
	{
		trap_SendServerCommand( -1, va("print \"%s\"", msg ));
	}
	else if(auth_verbosity.integer == 2) // BOTTOM
	{
		trap_SendServerCommand( -1, va("chat \"%s\"", msg));
	}
}

/*
================
SV_Auth_DropClient
================
*/
void SV_Auth_DropClient( int idnum, char *reason, char *message )
{
	gclient_t	*cl;

	#ifdef DEBUG
	Com_Printf( "SV_Auth_DropClient( %i ) \n", idnum );
	#endif

	// get client
	if ( idnum < 0 || idnum >= level.maxclients ) {
		Com_Printf( AUTH_PREFIX" drop error - bad client slot: %i\n", idnum );
		return;
	}

	cl = &level.clients[idnum];

	if ( cl->pers.connected != CON_CONNECTING && cl->pers.connected != CON_CONNECTED ) {
		Com_Printf( AUTH_PREFIX" drop error - client %i is not active\n", idnum );
		return;
	}

	if ( cl->pers.connected == CON_CONNECTING )
	{
		// delayed
		#ifdef DEBUG
		Com_Printf( AUTH_PREFIX" drop delayed - slot: %i\n", idnum );
		#endif
		Q_strncpyz( cl->sess.dropreason, reason, sizeof(cl->sess.dropreason) );
		Q_strncpyz( cl->sess.dropmessage, message, sizeof(cl->sess.dropmessage) );
		cl->sess.droptime = 1;
		return;
	}

	G_LogPrintf("AccountKick: %i - %s\n", idnum, reason);

	cl->sess.droptime = 0;

	trap_Auth_DropClient( idnum, reason, message );
}

/*
================
SV_Auth_Request
================
*/
void SV_Auth_Request( int clientNum )
{
	char userinfo[MAX_INFO_STRING];
	gclient_t	*cl;
	char buffer[AUTH_MAX_STRING_CHARS];
	int test;

	if ( auth_mode > 0 )
	{
		cl = &level.clients[clientNum];

		if( cl->pers.connected != CON_CONNECTED
		&& cl->pers.connected != CON_CONNECTING )
			return;

		// @Fenix - do not send account requests for bots
		if (g_entities[clientNum].r.svFlags & SVF_BOT) {
			return;
		}

		// @KALISH
		// This request will be sent only one time using UDP
		// We need to be sure it is received and answered
		// MUST add a send/timeout/retry/count>max=authserver down
		// (same for ban and all strategic commands)

		Com_Printf( AUTH_PREFIX" sending request for slot %i : %s\n", clientNum,  cl->pers.netname);

		trap_GetUserinfo( clientNum, userinfo, sizeof(userinfo));

		buffer[0] = '\0'; test = 1;

		if(test>0) test = Auth_cat_str( buffer, sizeof(buffer), Info_ValueForKey(userinfo, "ip"), 1);
		if(test>0) test = Auth_cat_str( buffer, sizeof(buffer), Info_ValueForKey(userinfo, "authc"), 1);
		if(test>0) test = Auth_cat_str( buffer, sizeof(buffer), Info_ValueForKey(userinfo, "cl_guid"), 1);
		if(test>0) test = Auth_cat_int( buffer, sizeof(buffer), clientNum, 0);
		if(test>0) test = Auth_cat_str( buffer, sizeof(buffer), cl->pers.netname, 1);

		if(test<1) return;

		SV_Authserver_Send_Packet( "CLIENT", buffer );
	}
}

/*
================
SV_Authserver_Get_Packet
================
*/
void SV_Authserver_Get_Packet( void )
{
	gclient_t *client;
	int		i;
	int		len;
	int		idnum = -1;
	int		challenge;
	char	s[MAX_STRING_CHARS];
	char	r[MAX_STRING_CHARS];
	char	m[MAX_STRING_CHARS];
	char	l[MAX_STRING_CHARS];
	char	n[MAX_STRING_CHARS];
	char	d[MAX_STRING_CHARS];
	char	userinfo[MAX_INFO_STRING];
	char	buffer[AUTH_MAX_STRING_CHARS];

	if ( auth_mode < 1 )
		return;

	if(atoi(G_Argv(1)) != AUTH_PROTOCOL) {
		Com_Printf (AUTH_PREFIX" protocol error (%i - contact an admin at http://"AUTH_WEBSITE" \n"), atoi(G_Argv(1)));
		return;
	}

	Q_strncpyz(buffer, G_Argv(2), sizeof(buffer));

	Auth_decrypt_args(buffer);

	if(auth_argc<2) {
		Com_Printf (AUTH_PREFIX" decrypt error - contact an admin at http://"AUTH_WEBSITE" \n");
		return;
	}

	if(auth_argc<8) {
		Com_Printf (AUTH_PREFIX" auth args error - contact an admin at http://"AUTH_WEBSITE" \n");
		return;
	}

	Q_strncpyz(m, Auth_argv(0), sizeof(m)); // main
	challenge = atoi( Auth_argv(1) );		// challenge
	idnum = atoi( Auth_argv(2) );			// slot
	Q_strncpyz(s, Auth_argv(3), sizeof(s)); // statu
	Q_strncpyz(l, Auth_argv(4), sizeof(l)); // login
	Q_strncpyz(r, Auth_argv(5), sizeof(r)); // level / short reason
	Q_strncpyz(n, Auth_argv(6), sizeof(n)); // notoriety / user message
	Q_strncpyz(d, Auth_argv(7), sizeof(d)); // addon / reject public reason

	// on join a slot

	if ( !Q_stricmp( m, "AUTH:CLIENT" ) )
	{
		client = &level.clients[idnum];
		if (client->pers.connected == CON_CONNECTED || client->pers.connected == CON_CONNECTING) // client->sess.auth_state>1 &&
		{
			if ( !Q_stricmp( s, "ACCEPT" ) )
			{
				Com_Printf( AUTH_PREFIX" user validated - name: %s - slot: %i - login: %s - level: %s - notoriety: %s - addon: %s\n", client->pers.netname, idnum, l, r, n, d );
				G_LogPrintf("AccountValidated: %i - %s - %s - \"%s\"\n", idnum, l, r, n);
				if( !Q_stricmp(client->sess.auth_login, l) || !Q_stricmp(client->sess.auth_notoriety, n) || client->sess.auth_level != atoi(r) )
				{
					trap_GetUserinfo( client->ps.clientNum, userinfo, sizeof(userinfo));

					// to display auth cl infos in logs
					len = strlen( client->sess.auth_login ) + strlen( client->sess.auth_notoriety ) + 3 + strlen( userinfo );
					if( len < MAX_INFO_STRING ) {
						Info_SetValueForKey( userinfo, "auth_login", l );
						Info_SetValueForKey( userinfo, "auth_name", d );
						Info_SetValueForKey( userinfo, "auth_notoriety", n);
						Info_SetValueForKey( userinfo, "auth_level", r );
						ClientUserinfoChanged(idnum);
					} else {
						Com_Printf( "userinfo: info string too long" );
					}

					Com_sprintf(client->sess.auth_login, sizeof(client->sess.auth_login), "%s", l);
					Com_sprintf(client->sess.auth_notoriety, sizeof(client->sess.auth_notoriety), "%s", n);
					Com_sprintf(client->sess.auth_name, sizeof(client->sess.auth_name), "%s", d);
					client->sess.auth_level = atoi(r);

					if( auth_verbosity.integer > 0 )
						SV_Auth_Print_User_Infos_f( client, 0 );
				}
				return;
			}
			if ( !Q_stricmp( s, "REJECT" ) )
			{
				Com_Printf( AUTH_PREFIX" user rejected - name: %s - slot: %i - login: %s - reason: %s \n", client->pers.netname, idnum, l, r );
				G_LogPrintf("AccountRejected: %i - %s - \"%s\"\n", idnum, l, r);
				trap_SendServerCommand( -1, va("print \"" AUTH_PRINT_PREFIX S_COLOR_WHITE "%s\n\"", d ));
				SV_Auth_DropClient( idnum, va("%s rejected: %s", client->pers.netname, r ), Auth_Stripslashes(n) );
				return;
			}
		}
		Com_Printf( AUTH_PREFIX" user not connected - slot: %i \n", idnum );
		return;
	}

	if ( !Q_stricmp( m, "AUTH:MSG" ) )
	{
		client = &level.clients[idnum];
		if (client->pers.connected == CON_CONNECTED || client->pers.connected == CON_CONNECTING) {
			if ( !Q_stricmp( s, "SAY" ) )
			{
				if( auth_verbosity.integer == 1 )
					trap_SendServerCommand(client - level.clients, va("print \"%s %s\"", AUTH_PM_PREFIX, Auth_Stripslashes(n) ) );
				else
					trap_SendServerCommand(client - level.clients, va("chat \"%s %s\"", AUTH_PM_PREFIX, Auth_Stripslashes(n) ) );
				return;
			}
			if ( !Q_stricmp( s, "DROP" ) )
			{
				Com_Printf( AUTH_PREFIX" user dropped - name: %s - slot: %i - login: %s - reason: %s \n", client->pers.netname, idnum, l, r );
				G_LogPrintf("AccountDropped: %i - %s - \"%s\"\n", idnum, l, r);
				SV_Auth_DropClient( idnum, va("%s dropped: %s", client->pers.netname, r ), Auth_Stripslashes(n) );
				return;
			}
		}
		return;
	}

	// unknown command

	Com_Printf(AUTH_PREFIX" unknown request - contact an admin at http://"AUTH_WEBSITE" \n");
}

#endif


vec3_t	playerMins = { -20, -20, -24};
vec3_t	playerMaxs = { 20, 20, 32};
//vec3_t	playerMins = {-15, -15, -24};
//    vec3_t	playerMaxs = {15, 15, 32};

extern vec3_t  bg_colors[20];
/*QUAKED info_ut_spawn (1 0 1) (-16 -16 -24) (16 16 32) initial
potential spawning position for Urban Terror.
"team"		team that this spawn belongs to ("red" or "blue")
"group" 	(Optional) this spawn groups with others of same groupID. A team chooses spawns from a certain group.
"spawnID"	if 0, this is a random spawn. Otherwise spawns will be chosen in incremental order.
*/
const char  *strEmpty = "";

/**
 * $(function)
 */
void SP_info_ut_spawn( gentity_t *ent )
{
	char  *team;
	char  *group;
	char  *gametype;

	// Get the spawn keywords
	G_SpawnString( "team", strEmpty, &team );
	G_SpawnString( "group", strEmpty, &group );
	G_SpawnString( "g_gametype", strEmpty, &gametype );

#ifdef _DEBUG
//	G_Printf ( "SPAWN:  team=%s  group=%s  gametype=%s\n", team, group, gametype );
#endif

	// Spawns are required to have a team identifier in the least. So just
	// ignore them if they have no team identifier

	// DensitY notes:
	// But of an oversite here, this is great if FFA didn't exist, as a result no matter what
	// the game mode is any FFA style spawns will be deleted (ie no team set..)
	// what it should be doing is checking if its g_gametype.integer is set to FFA and allow
	// spawns with no teams..
	if (!Q_stricmp(team, "red"))
	{
		ent->spawn	 = (gspawn_t *)bmalloc( sizeof(gspawn_t));
		ent->spawn->team = TEAM_RED;
	}
	else if (!Q_stricmp(team, "blue"))
	{
		ent->spawn	 = (gspawn_t *)bmalloc( sizeof(gspawn_t));
		ent->spawn->team = TEAM_BLUE;
	}
	else
	{
		G_Printf ( "Warning:  spawn ignored due to lack of team association\n" );
		G_FreeEntity ( ent );
		return;
	}

	// Save the group
	ent->spawn->group = (char *)bmalloc( strlen ( group ) + 1 );
	strcpy ( ent->spawn->group, group );

	// If no gametype was specified set it up so it works on all gametypes
	if (*gametype == 0)
	{
		ent->spawn->gametypes = 0xFFFF;
	}
	else
	{
		// Start with no gametypes
		ent->spawn->gametypes = 0;

		// Keep working through the string to find the supported gametypes
		while (*gametype)
		{
			char  *comma = strchr ( gametype, ',' );
			int   value  = 0;

			if (comma)
			{
				*comma = '\0';
			}

			// Get the value.
			value = atoi(gametype);

			if (comma)
			{
				*comma	 = ',';
				gametype = comma + 1;
			}
			else
			{
				gametype += strlen ( gametype );
			}

			// Set the flag for this gametype
			ent->spawn->gametypes |= (1 << value);
		}
	}

	/* This is for debugging
	    utFlagSpawn ( ent->s.origin, ent->spawn->team );
	*/
}

/*QUAKED info_player_deathmatch (1 0 1) (-16 -16 -24) (16 16 32) initial
potential spawning position for deathmatch games.
The first time a player enters the game, they will be at an 'initial' spot.
Targets will be fired when someone spawns in on them.
"nobots" will prevent bots from using this spot.
"nohumans" will prevent non-bots from using this spot.
*/
void SP_info_player_deathmatch( gentity_t *ent )
{
	int  i;

	G_SpawnInt( "nobots", "0", &i);

	if (i)
	{
		ent->flags |= FL_NO_BOTS;
	}
	G_SpawnInt( "nohumans", "0", &i );

	if (i)
	{
		ent->flags |= FL_NO_HUMANS;
	}
}

/*QUAKED info_player_start (1 0 0) (-16 -16 -24) (16 16 32)
equivelant to info_player_deathmatch
*/
void SP_info_player_start(gentity_t *ent)
{
	ent->classname = "info_player_deathmatch";
	ent->classhash = HSH_info_player_deathmatch;
	SP_info_player_deathmatch( ent );
}

/*QUAKED info_player_intermission (1 0 1) (-16 -16 -24) (16 16 32)
The intermission will be viewed from this point.  Target an info_notnull for the view direction.
*/
void SP_info_player_intermission( gentity_t *ent )
{
}

/*
=======================================================================

  SelectSpawnPoint

=======================================================================
*/

/*
================
SpotWouldTelefrag

================
*/
qboolean SpotWouldTelefrag( gentity_t *spot )
{
	int	   i, num;
	int	   touch[MAX_GENTITIES];
	gentity_t  *hit;
	vec3_t	   mins, maxs;

	VectorAdd( spot->s.origin, playerMins, mins );
	VectorAdd( spot->s.origin, playerMaxs, maxs );
	num = trap_EntitiesInBox( mins, maxs, touch, MAX_GENTITIES );

	for (i = 0 ; i < num ; i++)
	{
		hit = &g_entities[touch[i]];

		//if ( hit->client && hit->client->ps.stats[STAT_HEALTH] > 0 ) {
		if (hit->client)
		{
			return qtrue;
		}
	}

	return qfalse;
}

/*
================
SelectNearestDeathmatchSpawnPoint

Find the spot that we DON'T want to use
================
*/
#define     MAX_SPAWN_POINTS 128
/**
 * $(function)
 */
gentity_t *SelectNearestDeathmatchSpawnPoint( vec3_t from )
{
	gentity_t  *spot;
	vec3_t	   delta;
	float	   dist, nearestDist;
	gentity_t  *nearestSpot;

	nearestDist = 999999;
	nearestSpot = NULL;
	spot	    = NULL;

	while ((spot = G_FindClassHash (spot, HSH_info_player_deathmatch)) != NULL)
	{
		VectorSubtract( spot->s.origin, from, delta );
		dist = VectorLength( delta );

		if (dist < nearestDist)
		{
			nearestDist = dist;
			nearestSpot = spot;
		}
	}

	return nearestSpot;
}

/*
================
SelectRandomDeathmatchSpawnPoint

go to a random point that doesn't telefrag
================
*/
#define     MAX_SPAWN_POINTS 128
/**
 * $(function)
 */
gentity_t *SelectRandomDeathmatchSpawnPoint( void )
{
	gentity_t  *spot;
	int	   count;
	int	   selection;
	gentity_t  *spots[MAX_SPAWN_POINTS];

	count = 0;
	spot  = NULL;

	while ((spot = G_FindClassHash (spot, HSH_info_player_deathmatch)) != NULL)
	{
		if (SpotWouldTelefrag( spot ))
		{
			continue;
		}
		spots[count] = spot;
		count++;
	}

	if (!count)	// no spots that won't telefrag
	{
		return G_FindClassHash( NULL, HSH_info_player_deathmatch);
	}

	selection = rand() % count;
	return spots[selection];
}

/*
================
SelectRandomNPCSpawnPoint

go to a random point that doesn't telefrag
================
*/

gentity_t *SelectRandomNPCSpawnPoint( gentity_t *npc )
{
	gentity_t  *spot;
	int	   count;
	int	   selection;
	gentity_t  *spots[MAX_SPAWN_POINTS];

	count = 0;
	spot  = NULL;

	while ((spot = G_FindClassHash (spot, HSH_func_waypoint)) != NULL)
	{
		if (!(Q_stricmp(spot->npctype, npc->client->pers.netname) == 0) || (spot->currentWaypoint != 1))
		{
			continue;
		}

		if(SpotWouldTelefrag( spot ))
		{
			spot->s.origin[2] += 9;
		}

		spots[count] = spot;
		count++;
	}

	if (!count)	// no spots that won't telefrag
	{
		return G_FindClassHash( NULL, HSH_info_player_deathmatch);
	}

	selection = rand() % count;

	return spots[selection];
}

#define MIN_SPAWN_LOCATIONS 8

/*
===========
SelectRandomFurthestSpawnPoint

Chooses a player start, deathmatch start, etc
============
*/
gentity_t *SelectRandomFurthestSpawnPoint(vec3_t avoidPoint, vec3_t origin, vec3_t angles) {

    vec3_t     delta;
    float      dist;
    float      list_dist[64];
    gentity_t  *list_spot[64];
    gentity_t  *spot;
    int        numSpots, rnd, i, j;

    numSpots = 0;
    spot     = NULL;
    while ((spot = G_FindClassHash(spot, HSH_info_player_deathmatch)) != NULL) {

        if (SpotWouldTelefrag(spot)) {
            continue;
        }

        VectorSubtract(spot->s.origin, avoidPoint, delta);
        dist = VectorLength(delta);

        // Fenix: NOTE #1: in list_dist[] and list_spot[], the higher is the
        // array index, the shorter is the distance between the spawn location
        // and the location when the player died

        for (i = 0; i < numSpots; i++) {

            if (dist > list_dist[i]) {

                if (numSpots >= 64) {
                    numSpots = 64 - 1;
                }

                for (j = numSpots; j > i; j--) {
                    list_dist[j] = list_dist[j - 1];
                    list_spot[j] = list_spot[j - 1];
                }

                list_dist[i] = dist;
                list_spot[i] = spot;
                numSpots++;

                if (numSpots > 64) {
                    numSpots = 64;
                }

                break;
            }
        }

        if ((i >= numSpots) && (numSpots < 64)) {
            list_dist[numSpots] = dist;
            list_spot[numSpots] = spot;
            numSpots++;
        }
    }

    if (!numSpots) {

        spot = G_FindClassHash(NULL, HSH_info_player_deathmatch);

        if (!spot) {
            G_Error("Couldn't find a spawn point");
        }

        VectorCopy(spot->s.origin, origin);
        origin[2] += 9;
        VectorCopy(spot->s.angles, angles);
        return spot;

    }

    // Fenix: if we have enough available spawn  locations cut the size
    // of the array, so we use only locations which are really far from the
    // origin where the player died, otherwise pick a spawn location
    // among all the available ones to avoid repetition.
    if (numSpots >= MIN_SPAWN_LOCATIONS) {
        rnd = rand() % (numSpots / 2);
    } else {
        rnd = rand() % numSpots;
    }

    VectorCopy (list_spot[rnd]->s.origin, origin);
    origin[2] += 9;
    VectorCopy (list_spot[rnd]->s.angles, angles);

    return list_spot[rnd];
}

/*
===========
SelectSpawnPoint

Chooses a player start, deathmatch start, etc
============
*/
#if 0
gentity_t *SelectSpawnPoint ( vec3_t avoidPoint, vec3_t origin, vec3_t angles )
{
	return SelectRandomFurthestSpawnPoint( avoidPoint, origin, angles );

	/*
	gentity_t	*spot;
	gentity_t	*nearestSpot;

	nearestSpot = SelectNearestDeathmatchSpawnPoint( avoidPoint );

	spot = SelectRandomDeathmatchSpawnPoint ( );
	if ( spot == nearestSpot ) {
		// roll again if it would be real close to point of death
		spot = SelectRandomDeathmatchSpawnPoint ( );
		if ( spot == nearestSpot ) {
			// last try
			spot = SelectRandomDeathmatchSpawnPoint ( );
		}
	}

	// find a single player start spot
	if (!spot) {
		G_Error( "Couldn't find a spawn point" );
	}

	VectorCopy (spot->s.origin, origin);
	origin[2] += 9;
	VectorCopy (spot->s.angles, angles);

	return spot;
	*/
}
#endif
/*
===========
SelectNPCSpawnPoint

Chooses a spawn for an NPC,
============
*/
gentity_t *SelectNPCSpawnPoint ( gentity_t *npc, vec3_t avoidPoint, vec3_t origin, vec3_t angles )
{
	gentity_t  *spot;

	spot = SelectRandomNPCSpawnPoint ( npc );

	/*
	if(!spot){
		nearestSpot = SelectNearestDeathmatchSpawnPoint( avoidPoint );

		//try this ten times
		if ( spot == nearestSpot ) {
			// roll again if it would be real close to point of death
			spot = SelectRandomDeathmatchSpawnPoint ( );
			if ( spot == nearestSpot ) {
				// last try
				spot = SelectRandomDeathmatchSpawnPoint ( );
			}
		}
	}
	*/
	// find a single player start spot
	if (!spot)
	{
		G_Error( "Couldn't find a spawn point" );
	}

	VectorCopy (spot->s.origin, origin);
	origin[2] += 9;
	VectorCopy (spot->s.angles, angles);

	return spot;
}

/*
===========
SelectInitialSpawnPoint

Try to find a spawn point marked 'initial', otherwise
use normal spawn selection.
============
*/
gentity_t *SelectInitialSpawnPoint(vec3_t origin, vec3_t angles) {

    gentity_t  *spot;
    spot = NULL;

    while ((spot = G_FindClassHash (spot, HSH_info_player_deathmatch)) != NULL) {
        if (spot->spawnflags & 1) {
            break;
        }
    }

    if (!spot || SpotWouldTelefrag(spot)) {
        return SelectSpawnPoint(vec3_origin, origin, angles);
    }

    VectorCopy (spot->s.origin, origin);
    origin[2] += 9;
    VectorCopy (spot->s.angles, angles);

    return spot;
}

/*
===========
SelectSpectatorSpawnPoint

============
*/
gentity_t *SelectSpectatorSpawnPoint(vec3_t origin, vec3_t angles) {

    FindIntermissionPoint();

    VectorCopy( level.intermission_origin, origin );
    VectorCopy( level.intermission_angle, angles );

    return NULL;

}

/*
=======================================================================

BODYQUE

=======================================================================
*/

/*
===============
InitBodyQue
===============
*/
/*    void InitBodyQue (void)
    {
	int		i;
	gentity_t	*ent;

	level.bodyQueIndex = 0;
	for (i=0; i<BODY_QUEUE_SIZE ; i++)
      {
		ent = G_Spawn();
		ent->classname = "bodyque";
		ent->classhash = HSH_bodyque;
		ent->s.eType= EV_CORPSE;
	 ent->neverFree = qtrue;
		level.bodyQue[i] = ent;
	}
    }
    */
/*
=============
BodySink

After sitting around for five seconds, fall into the ground and dissapear
=============
*/
/*    void BodySink( gentity_t *ent ) {
	if ( level.time - ent->timestamp > 6500 )
		{
		// the body ques are never actually freed, they are just unlinked
		trap_UnlinkEntity( ent );
		ent->physicsObject = qfalse;
		return;
	}
		ent->physicsObject = qfalse;
		ent->nextthink = level.time + 10;
	ent->s.pos.trType=TR_INTERPOLATE;
		ent->s.pos.trBase[2] -= 1;
    } */

/*
=============
CopyToBodyQue

make an entity that looks
just like the existing corpse to leave behind.
=============
*/
/*   gentity_t* CopyToBodyQue( gentity_t *ent )
   {
   #ifdef MISSIONPACK
       gentity_t	*e;
       int i;
   #endif
       gentity_t		*body;
       int			contents;

       trap_UnlinkEntity (ent);

       // if client is in a nodrop area, don't leave the body
       contents = trap_PointContents( ent->s.origin, -1 );
       if ( contents & CONTENTS_NODROP )
     {
	       return NULL;
       }

       // grab a body que and cycle to the next one
       body = level.bodyQue[ level.bodyQueIndex ];
       level.bodyQueIndex = (level.bodyQueIndex + 1) % BODY_QUEUE_SIZE;

       trap_UnlinkEntity (body);

       body->s = ent->s;
       body->s.eFlags = EF_DEAD;		// clear EF_TALK, etc
       body->s.powerups = 0;	// clear powerups
       body->s.loopSound = 0;	// clear lava burning
       body->s.number = body - g_entities;
       body->timestamp = level.time;
       body->physicsObject = qtrue;
       body->physicsBounce = 0; 	// don't bounce
       body->s.time = ent->s.time&0x0000FF00;
       if ( body->s.groundEntityNum == ENTITYNUM_NONE )
     {
	       body->s.pos.trType = TR_GRAVITY;
	       body->s.pos.trTime = level.time;
	       VectorCopy( ent->client->ps.velocity, body->s.pos.trDelta );
       }
     else
     {
	       body->s.pos.trType = TR_STATIONARY;
       }
       body->s.event = 0;

	       body->s.legsAnim=ent->client->ps.legsAnim;
       body->s.legsAnim=ent->client->ps.legsAnim;
     // change the animation to the last-frame only, so the sequence
       // doesn't repeat anew for the body
       //t->client->ps.torsoAnim =body->s.torsoAnim;


       body->r.svFlags = ent->r.svFlags;

       body->clipmask = CONTENTS_SOLID | CONTENTS_PLAYERCLIP;
       body->r.contents = CONTENTS_CORPSE;

       body->r.ownerNum = ent->s.number;

       body->nextthink = level.time + (20 * 1000);
       body->timestamp=body->nextthink;

	       body->think = body_die;


       //body->die = body_die;
       body->takedamage = qfalse;

       VectorCopy ( ent->s.pos.trBase, body->s.pos.trBase);
	       VectorCopy ( ent->s.apos.trBase, body->s.apos.trBase);

	       VectorCopy ( body->s.pos.trBase, body->r.currentOrigin );


	       trap_LinkEntity (body);

       return body;
   } */

//======================================================================

/*
==================
SetClientViewAngle

==================
*/
void SetClientViewAngle( gentity_t *ent, vec3_t angle )
{
	int  i;

	// set the delta angle
	for (i = 0 ; i < 3 ; i++)
	{
		int  cmdAngle;

		cmdAngle			= ANGLE2SHORT(angle[i]);
		ent->client->ps.delta_angles[i] = cmdAngle - ent->client->pers.cmd.angles[i];
	}
	VectorCopy( angle, ent->s.angles );
	VectorCopy (ent->s.angles, ent->client->ps.viewangles);
}

/*
================
respawn
================
*/
void respawn( gentity_t *ent )
{
	gentity_t  *tent;

	if(ent->ignoreSpawn)
	{
		return;
	}

	ent->client->ghost = qfalse;
	ClientSpawn(ent);

	//	G_AddEvent( ent, EV_UT_FADEIN, 0 );
	// add a teleportation effect
	tent		  = G_TempEntity( ent->client->ps.origin, EV_PLAYER_TELEPORT_IN );
	tent->s.clientNum = ent->s.clientNum;
}

/*
================
TeamCount

Returns number of players on a team
================
*/
team_t TeamCount( int ignoreClientNum, team_t team )
{
	int  i;
	int  count = 0;

	for (i = 0 ; i < level.maxclients ; i++)
	{
		if (i == ignoreClientNum)
		{
			continue;
		}

		if (level.clients[i].pers.connected == CON_DISCONNECTED)
		{
			continue;
		}

		if (level.clients[i].pers.teamState.state != TEAM_ACTIVE)
		{
			continue;
		}

		if (level.clients[i].sess.sessionTeam == team)
		{
			count++;
		}
	}

	return count;
}

/**
 * $(function)
 */
team_t TeamLiveCount( int ignoreClientNum, team_t team )
{
	int  i;
	int  count = 0;

	for (i = 0 ; i < level.maxclients ; i++)
	{
		if (i == ignoreClientNum)
		{
			continue;
		}

		if (level.clients[i].pers.connected == CON_DISCONNECTED)
		{
			continue;
		}

		if (level.clients[i].ghost || (level.clients[i].ps.stats[STAT_HEALTH] <= 0))
		{
			continue;
		}

		if (level.clients[i].sess.sessionTeam == team)
		{
			count++;
		}
	}

	return count;
}

/**
 * $(function)
 */
int GhostCount( int ignoreClientNum, team_t team )
{
	int  i;
	int  count = 0;

	for (i = 0 ; i < level.maxclients ; i++)
	{
		if (i == ignoreClientNum)
		{
			continue;
		}

		if (level.clients[i].pers.connected == CON_DISCONNECTED)
		{
			continue;
		}

		if (level.clients[i].pers.teamState.state != TEAM_ACTIVE)
		{
			continue;
		}

		if (level.clients[i].sess.sessionTeam == team)
		{
			if((level.clients[i].ps.stats[STAT_HEALTH] <= 0) || level.clients[i].ghost)
			{
				count++;
			}
		}
	}

	return count;
}

/**
 * $(function)
 */
int GetAliveClient ( int firstClient )
{
	int  i = 0;

	for (i = firstClient; i < level.maxclients ; i++)
	{
		if (level.clients[i].pers.connected == CON_DISCONNECTED)
		{
			continue;
		}

		if (level.clients[i].sess.sessionTeam == TEAM_SPECTATOR)
		{
			continue;
		}

		if((level.clients[i].ps.stats[STAT_HEALTH] <= 0) || level.clients[i].ghost)
		{
			continue;
		}

		return i;
	}

	return -1;
}

/*
================
TeamLeader

Returns the client number of the team leader
================
*/
int TeamLeader( team_t team )
{
	int  i;

	for (i = 0 ; i < level.maxclients ; i++)
	{
		if (level.clients[i].pers.connected == CON_DISCONNECTED)
		{
			continue;
		}

		if (level.clients[i].sess.sessionTeam == team)
		{
			if (level.clients[i].sess.teamLeader)
			{
				return i;
			}
		}
	}

	return -1;
}

/*
================
PickTeam

================
*/
team_t PickTeam( int ignoreClientNum )
{
	int  counts[TEAM_NUM_TEAMS];

	counts[TEAM_BLUE] = TeamCount( ignoreClientNum, TEAM_BLUE );
	counts[TEAM_RED]  = TeamCount( ignoreClientNum, TEAM_RED );

	if (counts[TEAM_BLUE] > counts[TEAM_RED])
	{
		return TEAM_RED;
	}

	if (counts[TEAM_RED] > counts[TEAM_BLUE])
	{
		return TEAM_BLUE;
	}

	// equal team count, so join the team with the lowest score
	if (level.teamScores[TEAM_BLUE] > level.teamScores[TEAM_RED])
	{
		return TEAM_RED;
	}
	return TEAM_BLUE;
}


#if 0
/*
===========
ClientCheckName
============
*/
static void ClientCleanName( const char *in, char *out, int outSize )
{
	int   len, colorlessLen;
	char  ch;
	char  *p;
	int   spaces;

	//save room for trailing null byte
	outSize--;

	len	     = 0;
	colorlessLen = 0;
	p	     = out;
	*p	     = 0;
	spaces	     = 0;

	while(1)
	{
		ch = *in++;

		if(!ch)
		{
			break;
		}

		// don't allow leading spaces
		if(!*p && (ch == ' '))
		{
			continue;
		}

		// check colors
		if(ch == Q_COLOR_ESCAPE)
		{
			// add two color escapes for everyone typed so they show up
			// solo trailing carat is not a color prefix
			if(!*in)
			{
				break;
			}

			in++;
			continue;
			/*
					    // don't allow black in a name, period
					    if( ColorIndex(*in) == 0 ) {
						    in++;
						    continue;
					    }

					    // make sure room in dest for both chars
					    if( len > outSize - 2 ) {
						    break;
					    }

					    *out++ = ch;
					    *out++ = *in++;
					    len += 2;
					    continue;
			*/
		}

		// don't allow too many consecutive spaces
		if(ch == ' ')
		{
			spaces++;

			if(spaces > 3)
			{
				continue;
			}
		}
		else
		{
			spaces = 0;
		}

		if(len > outSize - 1)
		{
			break;
		}

		*out++ = ch;
		colorlessLen++;
		len++;
	}
	*out = 0;

	// don't allow empty names
	if((*p == 0) || (colorlessLen == 0))
	{
		Q_strncpyz( p, "UnnamedPlayer", outSize );
	}
}
#endif



/*
===========
ClientUserInfoChanged

Called from ClientConnect when the player first connects and
directly by the server system when the player updates a userinfo variable.

The game can override any of the settings and call trap_SetUserinfo
if desired.
============
*/
void ClientSetGear ( gclient_t *client ) {

	// Dont allow gear changing right now
	if(client->noGearChange) {
		return;
	}

	// Start over
	// memset ( client->ps.weaponData, 0, sizeof(int) * UT_MAX_WEAPONS );
	{ int i; for(i = 0; i < UT_MAX_WEAPONS; i++) client->ps.weaponData[i] = ut_weapon_xor; } // memset replacement for xored values
	memset ( client->ps.itemData, 0, sizeof(int) * UT_MAX_ITEMS );

	// Everyone has a knife
	utPSGiveWeapon ( &client->ps, UT_WP_KNIFE, client->pers.weaponModes[UT_WP_KNIFE] - '0' );

	// Select the knife in case they dont have weapons for some reason
	client->ps.weaponslot = utPSGetWeaponByID ( &client->ps, UT_WP_KNIFE );

	// Give the bomb if applicable
	// Also a double check to see if this guy is on the right team..
	if((g_gametype.integer == GT_UT_BOMB) &&
	    // Fenix: BombHolder is picked only on round start and never changed. Check also if BOMB has been dropped and collected by another player
	    ((client->sess.bombHolder == qtrue) || ((level.BombHolderLastRound = client->ps.clientNum) && (level.UseLastRoundBomber == qtrue) && (!client->sess.bombHolder))) &&
	    (client->sess.sessionTeam == level.AttackingTeam)) {
		utPSGiveWeapon( &client->ps, UT_WP_BOMB, 0 );
		client->ps.weaponslot = utPSGetWeaponByID ( &client->ps, UT_WP_BOMB );
	}

	// Give the grenade
	if((client->pers.gear.grenade > UT_ITEM_NONE) && (client->pers.gear.grenade < UT_ITEM_MAX))
	{
#ifdef FLASHNADES
		if (bg_itemlist[client->pers.gear.grenade].giTag == UT_WP_GRENADE_FLASH)
		{
			if (!(g_gear.integer & GEAR_FLASH))
			{
				utPSGiveWeapon ( &client->ps, bg_itemlist[client->pers.gear.grenade].giTag, client->pers.weaponModes[bg_itemlist[client->pers.gear.grenade].giTag] - '0');
			}
		}
		else
#endif
		{
			if (!(g_gear.integer & GEAR_NADES))
			{
				utPSGiveWeapon ( &client->ps, bg_itemlist[client->pers.gear.grenade].giTag, client->pers.weaponModes[bg_itemlist[client->pers.gear.grenade].giTag] - '0');
			}
		}
	}

	// Give the sidearm
	if (!(g_gear.integer & GEAR_PISTOLS))
	{
		if((client->pers.gear.sidearm > UT_ITEM_NONE) && (client->pers.gear.sidearm < UT_ITEM_MAX))
		{
			utPSGiveWeapon ( &client->ps, bg_itemlist[client->pers.gear.sidearm].giTag, client->pers.weaponModes[bg_itemlist[client->pers.gear.sidearm].giTag] - '0'  );
			client->ps.weaponslot = utPSGetWeaponByID ( &client->ps, bg_itemlist[client->pers.gear.sidearm].giTag);
		}
	}

	// Give the secondary
	if((client->pers.gear.secondary > UT_ITEM_NONE) && (client->pers.gear.secondary < UT_ITEM_MAX))
	{
		//give the spas if we can have spas's
		if (bg_itemlist[client->pers.gear.secondary].giTag == UT_WP_SPAS12/* || bg_itemlist[client->pers.gear.secondary].giTag == UT_WP_BENELLI*/)
		{
			if (!(g_gear.integer & GEAR_SPAS))
			{
				utPSGiveWeapon ( &client->ps, bg_itemlist[client->pers.gear.secondary].giTag, client->pers.weaponModes[bg_itemlist[client->pers.gear.secondary].giTag] - '0' );
				client->ps.weaponslot = utPSGetWeaponByID ( &client->ps, bg_itemlist[client->pers.gear.secondary].giTag);
			}
		}
		else //Give the "others" if we can have autos
		{
			if (!(g_gear.integer & GEAR_AUTOS))
			{
				utPSGiveWeapon ( &client->ps, bg_itemlist[client->pers.gear.secondary].giTag, client->pers.weaponModes[bg_itemlist[client->pers.gear.secondary].giTag] - '0' );
				client->ps.weaponslot = utPSGetWeaponByID ( &client->ps, bg_itemlist[client->pers.gear.secondary].giTag);
			}
		}
	}

	// Give the primary
	if((client->pers.gear.primary > UT_ITEM_NONE) && (client->pers.gear.primary < UT_ITEM_MAX))
	{
		if ((bg_itemlist[client->pers.gear.primary].giTag == UT_WP_SR8) || (bg_itemlist[client->pers.gear.primary].giTag == UT_WP_PSG1))
		{
			if (!(g_gear.integer & GEAR_SNIPERS)) //if allowed give it
			{
				utPSGiveWeapon ( &client->ps, bg_itemlist[client->pers.gear.primary].giTag, client->pers.weaponModes[bg_itemlist[client->pers.gear.primary].giTag] - '0'   );
				client->ps.weaponslot = utPSGetWeaponByID ( &client->ps, bg_itemlist[client->pers.gear.primary].giTag);
			}
		}
		else
		if (bg_itemlist[client->pers.gear.primary].giTag == UT_WP_HK69)
		{
			if (!(g_gear.integer & GEAR_NADES)) //if allowed give it
			{
				utPSGiveWeapon ( &client->ps, bg_itemlist[client->pers.gear.primary].giTag, client->pers.weaponModes[bg_itemlist[client->pers.gear.primary].giTag] - '0'   );
				client->ps.weaponslot = utPSGetWeaponByID ( &client->ps, bg_itemlist[client->pers.gear.primary].giTag);
			}
		}
		else
		if (bg_itemlist[client->pers.gear.primary].giTag == UT_WP_NEGEV)
		{
			if (!(g_gear.integer & GEAR_NEGEV)) //if allowed give it
			{
				utPSGiveWeapon ( &client->ps, bg_itemlist[client->pers.gear.primary].giTag, client->pers.weaponModes[bg_itemlist[client->pers.gear.primary].giTag] - '0'   );
				client->ps.weaponslot = utPSGetWeaponByID ( &client->ps, bg_itemlist[client->pers.gear.primary].giTag);
			}
		}
		//@Fenix
		//Check the SPAS also for primary
		else
		if (bg_itemlist[client->pers.gear.primary].giTag == UT_WP_SPAS12/* || bg_itemlist[client->pers.gear.primary].giTag == UT_WP_BENELLI*/)
		{
			if (!(g_gear.integer & GEAR_SPAS))  //if allowed give it
			{
				utPSGiveWeapon ( &client->ps, bg_itemlist[client->pers.gear.primary].giTag, client->pers.weaponModes[bg_itemlist[client->pers.gear.primary].giTag] - '0' );
				client->ps.weaponslot = utPSGetWeaponByID ( &client->ps, bg_itemlist[client->pers.gear.primary].giTag);
			}
		}
		else
		{
			if (!(g_gear.integer & GEAR_AUTOS)) //if allowed give it
			{
				utPSGiveWeapon ( &client->ps, bg_itemlist[client->pers.gear.primary].giTag, client->pers.weaponModes[bg_itemlist[client->pers.gear.primary].giTag] - '0'   );
				client->ps.weaponslot = utPSGetWeaponByID ( &client->ps, bg_itemlist[client->pers.gear.primary].giTag);
			}
		}
	}

	// Now give the items
	utPSGiveItem ( &client->ps, client->pers.gear.item1 );
	utPSGiveItem ( &client->ps, client->pers.gear.item2 );
	utPSGiveItem ( &client->ps, client->pers.gear.item3 );

	// Add the c4 back if the client had it
	//if ( c4 )
	//	utPSGiveItem ( &client->ps, UT_ITEM_C4 );

	// Team leader gets a free vest and helmet
	if (client->sess.teamLeader && (g_gametype.integer == GT_UT_ASSASIN))
	{
		// Give the team leaders kevlar and a helmet
		utPSGiveItem ( &client->ps, UT_ITEM_VEST );
		utPSGiveItem ( &client->ps, UT_ITEM_HELMET );
	}

	// All entities have lasers, but they are unlinked unless they are on.
	client->laserSight = utPSIsItemOn(&client->ps, UT_ITEM_LASER);
}

/**
 * $(function)
 */
//FIXME: seems that with AUTH enabled the duplicate name check doesn't work properly.
void ClientUserinfoChanged( int clientNum )
{
	gentity_t  *ent;
	qboolean   teamLeader;
	int	       team;
	char	   *s;
	char	   *e;
	char	   oldname[MAX_STRING_CHARS];
	char	   cleanname[MAX_STRING_CHARS];
	int		   nb_cleanname;
	gclient_t  *client;
	char	   userinfo[MAX_INFO_STRING];
	char	   mod[MAX_QPATH];
	vec3_t	   rgb;
	////////////////////
	int	   sex	    = 0; //fem
	int	   race     = 0; //white
	int	   dupname = 0; //dupname autorename
	int	   j, r;

	//////////////////

	ent    = g_entities + clientNum;
	client = ent->client;

	trap_GetUserinfo( clientNum, userinfo, sizeof(userinfo));

	Com_Printf( "ClientUserinfo: %i %s\n", clientNum, userinfo );
	G_LogPrintf( "ClientUserinfo: %i %s\n", clientNum, userinfo );

	// check for malformed or illegal info strings
	if (!Info_Validate(userinfo))
	{
		strcpy (userinfo, "\\name\\badinfo");
	}

	// check for local client
	s = Info_ValueForKey( userinfo, "ip" );

	if (!strcmp( s, "localhost" ))
	{
		client->pers.localClient = qtrue;
	}

	s = Info_ValueForKey(userinfo, "cg_physics");
	client->pers.physics = atoi(s) ? 1 : 0;

	s = Info_ValueForKey(userinfo, "ut_timenudge");
	team = atoi(s);

	if (team > 50)
	{
		team = 50;
	}
	else if (team < 0)
	{
		team = 0;
	}
	client->pers.ut_timenudge = team;

	s = Info_ValueForKey(userinfo, "cg_autoPickup");
	client->pers.autoPickup = atoi(s);

	// Fenix: new ghosting system for jump mode
	if (g_gametype.integer == GT_JUMP) {
	    s = Info_ValueForKey(userinfo, "cg_ghost");
	    client->sess.ghost = atoi(s) > 0 ? qtrue : qfalse;
    } else {
        // Default to 0 if we are not playing jump mode
        client->sess.ghost = qfalse;
    }

	// keep old name
	Q_strncpyz ( oldname, client->pers.netname, sizeof(oldname));

	// get new name and clean up
	s = Info_ValueForKey (userinfo, "name");
	SuperSanitizeString(s, cleanname);

	// remove last char while is a space
	nb_cleanname = strlen(cleanname);
	while( nb_cleanname && cleanname[ nb_cleanname -1 ] == ' ' ){
		cleanname[ nb_cleanname -1 ] = 0;
		nb_cleanname--;
	}

	// check if cleanname == "all" (prevent problems like auth-whois all)
	if (!Q_stricmp(cleanname, "all")){
		strcpy( cleanname , "UnnamedPlayer" );
		// Q_strncpyz ( oldname, cleanname , sizeof( oldname ) );
	}

	//Check for dupe names
	for (r = 0; r < MAX_CLIENTS; r++){
		ent = &g_entities[r];

		if (ent->client && (ent->client->pers.connected != CON_DISCONNECTED)){
			if (r != clientNum){
				if (!Q_stricmp( cleanname , ent->client->pers.netname)){
					strncpy(cleanname , va("%s_%i", cleanname, clientNum) , MAX_STRING_CHARS );
					dupname = 1; //@r00t:Make sure following code is executed after forced rename
				}
			}
		}
	}

#ifdef USE_AUTH
	if (strlen(cleanname) == 0)
		strcpy(cleanname, "UnnamedPlayer");
	//@Barbatos (send auth request after all other check on name are done)
	if(Q_stricmp( cleanname , client->sess.lastname) || (client->sess.renames == 0))
	{
		if(auth_mode > 0){

			if (client->sess.renames >= AUTH_MAX_RENAMES){

				ClientCleanName( oldname, client->pers.netname, 21 );
				Q_strncpyz(cleanname, oldname, sizeof(cleanname));

				Info_SetValueForKey(userinfo, "name", oldname);

				if ( auth_verbosity.integer == 1 ) {
					trap_SendServerCommand( clientNum, va("print \"%s%s^1ERROR^7 You can't rename more than %i times.\"", AUTH_PM_PREFIX, AUTH_PRINT_PREFIX, AUTH_MAX_RENAMES ) );
				} else if (auth_verbosity.integer == 2) {
					trap_SendServerCommand( clientNum, va("chat \"%s%s^1ERROR^3 You can't rename more than %i times.\"", AUTH_PM_PREFIX, AUTH_PRINT_PREFIX, AUTH_MAX_RENAMES ) );
				}
			}
			else {
				if (client->sess.renames == (AUTH_MAX_RENAMES - 1)) {

				    if ( auth_verbosity.integer == 1 ) {
						trap_SendServerCommand( clientNum, va("print \"%s%s^1WARNING^7 You can't rename more than %i times. Last Try !\"", AUTH_PM_PREFIX, AUTH_PRINT_PREFIX, AUTH_MAX_RENAMES ) );
					} else if (auth_verbosity.integer == 2) {
						trap_SendServerCommand( clientNum, va("chat \"%s%s^1WARNING^3 You can't rename more than %i times. Last Try !\"", AUTH_PM_PREFIX, AUTH_PRINT_PREFIX, AUTH_MAX_RENAMES ) );
				    }
				}

				ClientCleanName( cleanname, client->pers.netname, 21 );
				Info_SetValueForKey(userinfo, "name", client->pers.netname);
				SV_Auth_Request(clientNum);
			}
		}
		else{
			ClientCleanName( cleanname, client->pers.netname, 21 );
			Info_SetValueForKey(userinfo, "name", client->pers.netname);
		}

		trap_SetUserinfo( clientNum, userinfo); //@r00t:Update userinfo name (needed for /rcon stat, ban etc.)
		client->sess.renames++;
		Q_strncpyz(client->sess.lastname, cleanname, sizeof(client->sess.lastname));

	}
#endif


	// D8 24FIX:
	// changed
	// ClientCleanName( s, client->pers.netname, sizeof(client->pers.netname) );
	// to
	//ClientCleanName( s, client->pers.netname, 21 );
	// and removed Iain's setting of a '0'

	// Get the weapon modes only if this is the first time, otherwise the server SHOULD be in sync due to button
	//if ( !client->pers.weaponModes[0] )

	//s=	Info_ValueForKey ( userinfo, "weapmodes");
	//strcpy ( client->pers.weaponModes, s	 ); // "weapmodes"

	// If weapon modes array is not initialized or client is still connecting.
	if (!client->pers.weaponModes[0] || (client->pers.connected == CON_CONNECTING))
	{
		// Read weapon modes from userinfo string.
		s = Info_ValueForKey(userinfo, "weapmodes");

		// If weapon modes string if of the correct length.
		if (strlen(s) == UT_WP_NUM_WEAPONS)
		{
			memcpy(client->pers.weaponModes, s, sizeof(client->pers.weaponModes));
		}
	}

	// Get the gear and set it
	utGear_Decompress ( Info_ValueForKey ( userinfo, "gear" ), &client->pers.gear);
	ClientSetGear ( client );

	// Scoreboard stuff?
	if (client->sess.sessionTeam == TEAM_SPECTATOR)
	{
		if (client->sess.spectatorState == SPECTATOR_SCOREBOARD)
		{
			Q_strncpyz( client->pers.netname, "scoreboard", sizeof(client->pers.netname));
		}
	}

	// If they are a connected client then show the team rename
	if (client->pers.connected == CON_CONNECTED)
	{
		if (strcmp( oldname, client->pers.netname ))
		{
			/*	// check to see if there is a vote against this guy first
				if ( level.voteTime && strstr ( level.voteString, oldname ) )
				{
					trap_SendServerCommand ( client - &level.clients[0], "print \"Cannot change name when a vote is active against you.\n\"" );
					strcpy ( client->pers.netname, oldname );
				}
				else if ( client->ps.pm_type != PM_NORMAL || client->ghost )
				{
					trap_SendServerCommand ( client - &level.clients[0], "print \"To prevent cheating, your name change will be applied as soon as you respawn.\n\"" );
					strcpy ( client->pers.newnetname, client->pers.netname );
					strcpy ( client->pers.netname, oldname );
				}
				else
				{ */
			trap_SendServerCommand( -1, va("print \"%s" S_COLOR_WHITE " renamed to %s\n\"", oldname,
						       client->pers.netname));
			/*	}*/
		}
	}

	// Set the name
	//memset(oldname, 0, sizeof(oldname));
	//if ( client->pers.newnetname[0] )
	//	strcpy ( oldname, client->pers.newnetname );
	//else
//		strcpy ( oldname, client->pers.netname );

	//Iain: choppy names cause we canny be arsed with long ones :)
	// 24BUG: this is messy, better to fix above in strcpy
	/*if(oldname[20]) {
		oldname[20]=0;
	};*/

	// set max health
//	client->pers.maxHealth = UT_MAX_HEALTH;

	// bots set their team a few frames later
	if ((client->sess.sessionTeam == TEAM_SPECTATOR) && (g_gametype.integer >= GT_TEAM) && (g_entities[clientNum].r.svFlags & SVF_BOT))
	{
		s = Info_ValueForKey( userinfo, "team" );

		if (!Q_stricmp( s, "red" ) || !Q_stricmp( s, "r" ))
		{
			team = TEAM_RED;
		}
		else if (!Q_stricmp( s, "blue" ) || !Q_stricmp( s, "b" ))
		{
			team = TEAM_BLUE;
		}
		else
		{
			// pick the team with the least number of players
			team = PickTeam( clientNum );
		}
	}
	else
	{
		team = client->sess.sessionTeam;
	}

	//27
	// Set the model now
	mod[0] = '\0';

	if (client->sess.sessionTeam == TEAM_BLUE)
	{
		Q_strncpyz( mod, Info_ValueForKey (userinfo, "raceblue"), sizeof(mod));
	}

	else if(client->sess.sessionTeam == TEAM_RED)
	{
		Q_strncpyz( mod, Info_ValueForKey (userinfo, "racered"), sizeof(mod));
	}

	//@Barbatos - jump and FFA/LMS models
	else
	{
		if(g_gametype.integer == GT_JUMP)
			Q_strncpyz( mod, Info_ValueForKey (userinfo, "racejump"), sizeof(mod));
		else
			Q_strncpyz( mod, Info_ValueForKey (userinfo, "racefree"), sizeof(mod));
	}

	mod[1]		  = '\0';

	sex		  = 1;
	race		  = 0;
	client->pers.race = 2;

	if (mod[0] == '0')
	{
		sex		  = 0;
		race		  = 0;
		client->pers.race = 0;
	}
	else
	if (mod[0] == '1')
	{
		sex		  = 0;
		race		  = 1; //male black
		client->pers.race = 1;
	}
	else
	if (mod[0] == '2') //fem white
	{
		sex		  = 1;
		race		  = 0;
		client->pers.race = 2;
	}
	else
	if (mod[0] == '3')
	{
		sex		  = 1;
		race		  = 1;
		client->pers.race = 3;
	}

	// Determine which aries model to use for this client.
	if ( sex==1 )
		     client->pers.ariesModel = ARIES_ORION;
	     else
		     client->pers.ariesModel = ARIES_ATHENA;

	//@Barbatos
	mod[0] = '\0';
	memset(client->pers.funstuff0, 0, sizeof(client->pers.funstuff0));
	memset(client->pers.funstuff1, 0, sizeof(client->pers.funstuff1));
	memset(client->pers.funstuff2, 0, sizeof(client->pers.funstuff2));

	if (client->sess.sessionTeam == TEAM_RED)
	{
		Q_strncpyz( mod, Info_ValueForKey (userinfo, "funred"), sizeof(mod));
	}
    else if (client->sess.sessionTeam == TEAM_BLUE)
	{
		Q_strncpyz( mod, Info_ValueForKey (userinfo, "funblue"), sizeof(mod));
	}
    else
    {
		Q_strncpyz( mod, Info_ValueForKey (userinfo, "funfree"), sizeof(mod));
    }

	//memset(client->pers.funstuffred,0,sizeof(client->pers.funstuffred));

	if (1 || (client->sess.sessionTeam == TEAM_RED) || (client->sess.sessionTeam == TEAM_BLUE))
	{
		s = &mod[0];
		e = &mod[0];

		for (j = 0; j < 3; j++)
		{
			while (*e != 0 && *e != ',')
			{
				e++;
			}

			if (e - s < 2)
			{
				break;
			}

			if (j == 0)
			{
				memcpy(client->pers.funstuff0, s, e - s);
			}

			if (j == 1)
			{
				memcpy(client->pers.funstuff1, s, e - s);
			}

			if (j == 2)
			{
				memcpy(client->pers.funstuff2, s, e - s);
			}
			e++;
			s = e;
		}
	}
	/////////////////////////////////////

	mod[0] = '\0';
	memset(mod, 0, sizeof(mod));
	Q_strncpyz( mod, Info_ValueForKey (userinfo, "cg_rgb"), sizeof(mod));
	rgb[0] = rgb[1] = rgb[2] = 255;

	if (strchr ( mod, ',' ))
	{
		sscanf ( mod, "%f,%f,%f",
			 &rgb[0],
			 &rgb[1],
			 &rgb[2]
			 );
	}
	else
	{
		sscanf (mod, "%f %f %f",
			&rgb[0],
			&rgb[1],
			&rgb[2]
			);
	}

	if ((rgb[0] < 0) || (rgb[0] > 255))
	{
		rgb[0] = 255;
	}

	if ((rgb[1] < 0) || (rgb[1] > 255))
	{
		rgb[1] = 255;
	}

	if ((rgb[2] < 0) || (rgb[2] > 255))
	{
		rgb[2] = 255;
	}

	//VectorNormalize(rgb);
	if ((rgb[0] > 1) || (rgb[1] > 1) || (rgb[2] > 1))
	{
		rgb[0] /= 255;
		rgb[1] /= 255;
		rgb[2] /= 255;
	}

	client->pers.armbandrgb[0] = (int)(rgb[0] * 255.0f);
	client->pers.armbandrgb[1] = (int)(rgb[1] * 255.0f);
	client->pers.armbandrgb[2] = (int)(rgb[2] * 255.0f);

	if (g_armbands.integer == 1)
	{
		//lock them to team colors
		if (client->sess.sessionTeam == TEAM_RED)
		{
			client->pers.armbandrgb[0] = (int)255;
			client->pers.armbandrgb[1] = (int)0;
			client->pers.armbandrgb[2] = (int)0;
		}

		if (client->sess.sessionTeam == TEAM_BLUE)
		{
			client->pers.armbandrgb[0] = (int)0;
			client->pers.armbandrgb[1] = (int)0;
			client->pers.armbandrgb[2] = (int)255;
		}

		if (client->sess.sessionTeam == TEAM_FREE)
		{
			if (g_gametype.integer == GT_JUMP) {
				//@Fenix - JUMP mode new color
				client->pers.armbandrgb[0] = (int)196;
				client->pers.armbandrgb[1] = (int)149;
				client->pers.armbandrgb[2] = (int)232;
			}
			else {
				//@Fenix - changed armband to green for FFA and LMS
				client->pers.armbandrgb[0] = (int)0;
				client->pers.armbandrgb[1] = (int)255;
				client->pers.armbandrgb[2] = (int)0;
			}
		}

		if (client->sess.sessionTeam == TEAM_SPECTATOR)
		{
			client->pers.armbandrgb[0] = (int)255;
			client->pers.armbandrgb[1] = (int)255;
			client->pers.armbandrgb[2] = (int)255;
		}
	}

	if (g_armbands.integer == 2)
	{
		//lock them to a 'random' clientnum color
		client->pers.armbandrgb[0] = (int)bg_colors[clientNum % 20][0];
		client->pers.armbandrgb[1] = (int)bg_colors[clientNum % 20][1];
		client->pers.armbandrgb[2] = (int)bg_colors[clientNum % 20][2];
	}

	//Com_sprintf( tempb,64, "%f %f %f",rgb[0],rgb[1],rgb[2]);
///	    trap_Cvar_Set("cg_armbandrgb",tempb);
	//	  Com_Printf( "cg_armbandRGB set to a random color: %f %f %f",rgb[0],rgb[1],rgb[2]);
	//   }

	/////////////////////
	if ((g_gametype.integer >= GT_TEAM) && (g_gametype.integer != GT_JUMP))
	{
		client->pers.teamInfo = qtrue;
	}
	else
	{
		s = Info_ValueForKey( userinfo, "teamoverlay" );

		if (!*s || (atoi( s ) != 0))
		{
			client->pers.teamInfo = qtrue;
		}
		else
		{
			client->pers.teamInfo = qfalse;
		}
	}

	// team Leader (1 = leader, 0 is normal player)
	teamLeader = client->sess.teamLeader;

//@r00t: Parse guid as HEX string
	s = Info_ValueForKey( userinfo, "cl_guid" );
	j = s ? strlen(s) : 0;
	if (j==32) {
		unsigned char *dst = client->sess.guid_bin;
		for(r=0;r<32;r+=2) {
			int d;
			int c = s[r]-48;
			if (c<0) { j=0; break; }
			if (c>9) { c-=7; if (c>15) { j=0; break; } }
			d = s[r+1]-48;
			if (d<0) { j=0; break; }
			if (d>9) { d-=7; if (d>15) { j=0; break; } }
			*(dst++)=(c<<4)|d;
		}
	}
	if (j!=32) memset(client->sess.guid_bin,0,32);
// also store IP
	s = Info_ValueForKey (userinfo, "ip");
	if (s) Q_strncpyz(client->sess.ip, s, sizeof(client->sess.ip)); else client->sess.ip[0]=0;

	if (!client->sess.hax_report[5]) { // don't have initial report
		s = Info_ValueForKey(userinfo, "cl_time"); // get last game hax info
		if (s) {
			int v1,v2;
			v1 = atoi(s); // can't use sscanf for some reason
			while(*s && *s!=' ') s++;
			if (*s) {
				v2 = atoi(s+1);
				if (v1 && v2) { // check for empty string or no values
					client->sess.hax_report[3] = v1;
					client->sess.hax_report[4] = v2;
					client->sess.hax_report[5] = 1;
					client->sess.hax_report_time = level.time;
				}
			}
		}
	}

	// send over a subset of the userinfo keys so other clients can
	// print scoreboards, display models, and play custom sounds
	//Iain: Added ip for warhammer
	if (ent->r.svFlags & SVF_BOT)
	{
		s = va("n\\%s\\t\\%i\\r\\%i\\tl\\%d\\f0\\%s\\f1\\%s\\f2\\%s\\a0\\%i\\a1\\%i\\a2\\%i",
		//s = va("n\\%s\\t\\%i\\r\\%i\\tl\\%d\\a0\\%i\\a1\\%i\\a2\\%i",
		       client->pers.netname,
		       client->sess.sessionTeam,
		       client->pers.race,
		       (int)teamLeader,
		       client->pers.funstuff0,
		       client->pers.funstuff1,
		       client->pers.funstuff2,
		       client->pers.armbandrgb[0],
		       client->pers.armbandrgb[1],
		       client->pers.armbandrgb[2]);
	}
	else
	{
		s = va("n\\%s\\t\\%i\\r\\%i\\tl\\%d\\f0\\%s\\f1\\%s\\f2\\%s\\a0\\%i\\a1\\%i\\a2\\%i",
		//s = va("n\\%s\\t\\%i\\r\\%i\\tl\\%d\\a0\\%i\\a1\\%i\\a2\\%i",
		       client->pers.netname,
		       client->sess.sessionTeam,
		       client->pers.race,
		       (int)teamLeader,
		       client->pers.funstuff0,
		       client->pers.funstuff1,
		       client->pers.funstuff2,
		       client->pers.armbandrgb[0],
		       client->pers.armbandrgb[1],
		       client->pers.armbandrgb[2]
		       );
	}
	trap_SetConfigstring( CS_PLAYERS + clientNum, s );

	//Iain Build The Team List Cvars
	G_LogPrintf( "ClientUserinfoChanged: %i %s\n", clientNum, s );
	BuildTeamLists();
}

/*
===========
ClientConnect

Called when a player begins connecting to the server.
Called again for every map change or tournement restart.

The session information will be valid after exit.

Return NULL if the client should be allowed, otherwise return
a string with the reason for denial.

Otherwise, the client will be sent the current gamestate
and will eventually get to ClientBegin.

firstTime will be qtrue the very first time a client connects
to the server machine, but qfalse on map changes and tournement
restarts.
============
*/

/**
 * ClientConnect
 *
 * When a client connects to the server.
 */
char *ClientConnect(int clientNum, qboolean firstTime, qboolean isBot)
{
	gentity_t  *entity;
	gclient_t  *client;
	char	   *value;
	char	   userinfo[MAX_INFO_STRING];
	//team_t	   team;
	//int	   i;

	// Set entity pointer.
	entity = &g_entities[clientNum];

	// Get user information.
	trap_GetUserinfo(clientNum, userinfo, sizeof(userinfo));

	// we're not a bot.
	if (!isBot)
	{
		// Get our ip.
		value = Info_ValueForKey(userinfo, "ip");

		// If we're banned from this server.
		if (G_FilterPacket(value))
		{
			return "Banned.";
		}
	}

	if (!isBot)
	{
		// Get our password.
		value = Info_ValueForKey(userinfo, "password");

		// If our password does not match the one the server uses.
		if (g_password.string[0] && Q_stricmp(g_password.string, "none") && (strcmp(g_password.string, value) != 0))
		{
			return "Invalid password";
		}
	}

	// Set client pointer.
	entity->client = level.clients + clientNum;
	client	       = entity->client;

	// No laser for now.
	entity->client->laserSight = qfalse;

	// reset team kill transgressions
	entity->breakshards = 0;
	entity->breakaxis = 0;

	// Initialize client data.
	memset(client, 0, sizeof(*client));

	// Configure client.
	client->pers.connected = CON_CONNECTING;
	client->savedclientNum = clientNum;
	client->ghost	       = qfalse;

	// Initialize clients' session data.
	if (firstTime || level.newSession) {
		G_InitSessionData(client, userinfo);
	} else {
		G_ReadSessionData(client);
	}

	// TODO: Remove
	//Com_Printf("Gametype: %d\nTeam: %d\n", g_gametype.integer, team);

	// Make sure the team is correct.
	if ((g_gametype.integer >= GT_TEAM) && (g_gametype.integer != GT_JUMP))
	{
		if (client->sess.sessionTeam == TEAM_FREE)
		{
			client->sess.sessionTeam = TEAM_SPECTATOR;
		}
	}
	else if ((g_gametype.integer < GT_TEAM) || (g_gametype.integer == GT_JUMP))
	{
		if (client->sess.sessionTeam != TEAM_SPECTATOR)
		{
			client->sess.sessionTeam = TEAM_FREE;
		}
	}

	// If this is a bot.
	if (isBot)
	{
		// Configure stuff.
		entity->r.svFlags |= SVF_BOT;
		entity->inuse	   = qtrue;

		// If unable to connect bot.
		if (!G_BotConnect( clientNum, !firstTime))
		{
			return "BotConnectfailed";
		}
	}

	// If client is connected to server for the first time (not map reload..), clear weapon modes array.
	if (firstTime)
	{
		memset(client->pers.weaponModes, 0, sizeof(client->pers.weaponModes));
		client->sess.inactivityTime = level.time + g_inactivity.integer * 1000;
	}

	// For log.
	G_LogPrintf("ClientConnect: %i\n", clientNum);

	#ifdef USE_AUTH
	// @Kalish
	if (firstTime)
	{
		client->sess.droptime = 0;
		memset(client->sess.dropreason, 0, sizeof(client->sess.dropreason));
		memset(client->sess.dropmessage, 0, sizeof(client->sess.dropmessage));
	}
	#endif

	// Get and distribute relevent paramters.
	ClientUserinfoChanged(clientNum);

	// If needed tell everyone we have connected.
	if (firstTime)
	{
		trap_SendServerCommand( -1, va("print \"%s" S_COLOR_WHITE " connected\n\"", client->pers.netname));
	}

	// If needed tell everyone on what team we are.
	if ((g_gametype.integer >= GT_TEAM) && (g_gametype.integer != GT_JUMP) && (client->sess.sessionTeam != TEAM_SPECTATOR))
	{
		BroadcastTeamChange(client, -1);
	}

	// Calcilate scoreboard ranks.
	CalculateRanks();

	return NULL;
}

/*
===========
ClientBegin

called when a client has finished connecting, and is ready
to be placed into the level.  This will happen every level load,
and on transition between teams, but doesn't happen on respawns
============
*/
void ClientBegin( int clientNum )
{
	gentity_t  *ent;
	gclient_t  *client;
	gentity_t  *tent, *tent2;
	int	   flags;

	ent    = g_entities + clientNum;

	client = level.clients + clientNum;

#ifdef _MAINTAIN_TEAM
	// Auto join from last map, this only works within the first 5 minutes of the server operating
	if (client->sess.pendingTeam)
	{
		if (level.time < 5 * 60 * 1000)
		{
			if ((g_gametype.integer >= GT_TEAM) && (g_gametype.integer != GT_JUMP) && ((client->sess.sessionTeam == TEAM_SPECTATOR) || (client->sess.sessionTeam == TEAM_FREE)))
			{
				if (client->sess.pendingTeam == TEAM_RED)
				{
					SetTeam ( ent, "red", qfalse );
					return;
				}
				else if (client->sess.pendingTeam == TEAM_BLUE)
				{
					SetTeam ( ent, "blue", qfalse);
					return;
				}
			}
		}
	}
#endif

	// Moved auto-join to here.
	if ((client->sess.sessionTeam == TEAM_SPECTATOR) &&
		 g_teamAutoJoin.integer &&
		 (g_gametype.integer >= GT_TEAM) &&
		 (g_gametype.integer != GT_JUMP) &&
		 !client->pers.wasAutoJoined &&
		 !(g_entities[ client - level.clients ].r.svFlags & SVF_BOT)) {

		client->pers.wasAutoJoined = qtrue;
		SetTeam ( ent, PickTeam( -1 ) == TEAM_RED ? "red" : "blue", qfalse);
		return;
	}

	if ( ((g_gametype.integer < GT_TEAM) || (g_gametype.integer == GT_JUMP)) && (client->sess.sessionTeam != TEAM_SPECTATOR) && (client->sess.sessionTeam != TEAM_FREE))
	{
		SetTeam(ent, "free", qfalse);
		return;
	}

	// if dead or connect during game, don't live.
	if (g_survivor.integer && !level.warmupTime)
		G_SetGhostMode(ent);

	client->spectatorDirection = 1;

	if (ent->r.linked)
	{
		trap_UnlinkEntity( ent );
	}

	G_InitGentity( ent );
	ent->touch		     = 0;
	ent->pain		     = 0;
	ent->client		     = client;

	client->pers.connected	     = CON_CONNECTED;
	client->pers.enterTime	     = level.time;
	client->pers.teamState.state = TEAM_BEGIN;

	// save eflags around this, because changing teams will
	// cause this to happen with a valid entity, and we
	// want to make sure the teleport bit is set right
	// so the viewpoint doesn't interpolate through the
	// world to the new position
	flags		  = client->ps.eFlags;
	memset( &client->ps, 0, sizeof(client->ps));
	client->ps.eFlags = flags;

	// Reset his inactivity timer
	//if (g_inactivity.integer)
	//	ent->client->sess.inactivityTime = level.time + g_inactivity.integer * 1000;
	//else
	//	ent->client->sess.inactivityTime = level.time + 60 * 1000;

	// This is a hack to ensure the weapon modes are correct
	//	if ( g_survivor.integer )
	//	{
	// Read the weapon modes too, server alwasy knows best when it comes to weapon modes
	//		char* var = va( "survivor%i_modes", client - level.clients );
	//		trap_Cvar_VariableStringBuffer ( var, client->pers.weaponModes, sizeof(client->pers.weaponModes) );
	//	}

	//gah DO NOT spawn them if we're in TS and the time has elapsed

	// COMMENTED OUT BY HIGHSEA.
	/*
	// DENSITY: MESSY FIX!! RECODE!
	if( g_survivor.integer && !level.warmupTime && ent->client->sess.sessionTeam != TEAM_SPECTATOR )
	{
	if ( level.survivorNoJoin || ent->client->noGearChange )
	{
			G_SetGhostMode( ent );
		}
	}
	ClientSpawn( ent );
	*/

	// Spawn.
	ClientSpawn(ent);

	//@Barbatos: LMS fix, can't spawn during a round
	/*if(g_survivor.integer && !level.warmupTime && (ent->client->sess.sessionTeam != TEAM_SPECTATOR) && (level.survivorNoJoin || ent->client->noGearChange))*/
	/*{*/
		/*// Set ghost mode.*/
		/*G_SetGhostMode(ent);*/

		/*// Free minimap client arrow.*/

		/*// Unlink client from collision engine.*/
		/*trap_UnlinkEntity(ent);*/
	/*}*/

	//NT - reset the origin trails
	G_ResetTrail( ent );

	//	if ( g_survivor.integer )
//		G_ReadSurvivorRoundData ( client );

	if (client->sess.sessionTeam != TEAM_SPECTATOR)
	{
		// send event
		tent		  = G_TempEntity( ent->client->ps.origin, EV_PLAYER_TELEPORT_IN );
		tent->s.clientNum = ent->s.clientNum;
		tent->r.svFlags   = SVF_BROADCAST;

		tent2 = G_TempEntity( ent->client->ps.origin, EV_UT_STARTRECORD );
		tent->s.clientNum = ent->s.clientNum;

		if(!g_survivor.integer || !client->survivor.playing)
		{
			trap_SendServerCommand( -1, va("ccprint \"%d\" \"%d\" \"%s\"", CCPRINT_ENTERED,
				client->sess.sessionTeam,
				client->pers.netname));
			//trap_SendServerCommand( -1, va("print \"%s%s" S_COLOR_WHITE " entered the game\n\"", TeamColorString(client->sess.sessionTeam), client->pers.netname));
			client->survivor.playing = qtrue;
		}

		if (g_gametype.integer == GT_UT_ASSASIN)
		{
			int  leader;

			if (-1 != (leader = TeamLeader ( client->sess.sessionTeam )))
			{
				if (leader == ent->s.clientNum)
				{
					trap_SendServerCommand( clientNum, va("cp \"You are the team leader.\"" ));
				}
				else
				{
					trap_SendServerCommand( clientNum, va("cp \"%s%s%s is the leader.\"", TeamColorString(client->sess.sessionTeam), level.clients[leader].pers.netname, S_COLOR_WHITE ));
				}
			}
		}
	}
	else
	{
		client->survivor.playing = qfalse;
	}

	// ADD MOTD HERE!!! - DensitY: ok :D
	trap_SendServerCommand( clientNum, va( "cp \"%s\"", g_JoinMessage.string ));

	G_LogPrintf( "ClientBegin: %i\n", clientNum );

	// count current clients and rank for scoreboard
	CalculateRanks();
	MapArrowSpawn( clientNum );


	#ifdef USE_AUTH
	//@Barbatos //@Kalish
	if ( auth_mode > 0 )
	{
		// timer to drop the client after map loading
		if( client->sess.droptime==1 )
		{
			client->sess.droptime = level.time;
		}

		// print the message to client only
		if( client->sess.droptime < 1 )
		{
			if(!client->sess.welcome) {
				client->sess.welcome = 1;
				SV_Auth_Print_User_Infos_f( client, 1 );
			}
		}
	}
	#endif
}

/*
===========
ClientSpawn

Called every time a client is placed fresh in the world:
after the first ClientBegin, and after each respawn
Initializes all non-persistant parts of playerState
============
*/
void ClientSpawn(gentity_t *ent) {

    int                     i;
    int                     index;
    int                     flags;
    int                     ghost;
    int                     voted;
    int                     savedPing;
    int                     eventSequence;
    int                     accuracy_hits, accuracy_shots;
    int                     persistant[MAX_PERSISTANT];
    //char                  *savedAreaBits;
    char                    userinfo[MAX_INFO_STRING];
    vec3_t                  spawn_origin, spawn_angles;
    clientPersistant_t      saved;
    clientSession_t         savedSess;
    gentity_t               *maparrowp;
    gentity_t               *spawnPoint;
    gclient_t               *client;
    unsigned                goalbits;

    // UT for NPC
    index  = ent - g_entities;
    client = ent->client;

    // find a spawn point
    // do it before setting health back up, so farthest
    // ranging doesn't count this client

    if(ent->ignoreSpawn) {
        return;
    }

    // Temporarily disabled cause NPC's are not used
    // if ( ent->r.svFlags & SVF_BOT && ent->isNPC == qtrue) {      // GottaBeKD: Spawn NPC
    //     spawnPoint = SelectNPCSpawnPoint(ent,
    //                                      client->ps.origin,
    //                                      spawn_origin, spawn_angles);
    // }

    client->IsDefusing = qfalse;
    client->sess.spectatorClient = client - level.clients;
    client->sess.spectatorState  = SPECTATOR_NOT;

    // if we are spectator
    if (client->sess.sessionTeam == TEAM_SPECTATOR) {
        spawnPoint = SelectSpectatorSpawnPoint(spawn_origin, spawn_angles);
    }
    // if we are playing a team based gametype
    else if ((g_gametype.integer >= GT_TEAM) && (g_gametype.integer != GT_JUMP)) {

        // all base oriented team games use the CTF spawn points

        // GottaBeKD: we use our own spawn point system now
        // spawnPoint = SelectCTFSpawnPoint(
        //                      client->sess.sessionTeam,
        //                      client->pers.teamState.state,
        //                      spawn_origin, spawn_angles);

        spawnPoint = SelectUTSpawnPoint(client->sess.sessionTeam,
                                        client->pers.teamState.state,
                                        spawn_origin, spawn_angles );

    }
    // solo modes
    else {

        // FIXME: This loop may never end on map with some
        // spawns marked NO_HUMANS and no INITIAL_SPAWN

        do {

            // the first spawn should be at a good looking spot
            if (!client->pers.initialSpawn && client->pers.localClient) {

                client->pers.initialSpawn = qtrue;
                spawnPoint = SelectInitialSpawnPoint( spawn_origin, spawn_angles );

            } else {

                // don't spawn near existing origin if possible
                spawnPoint = SelectSpawnPoint(client->ps.origin, spawn_origin, spawn_angles);
            }

            // Tim needs to prevent bots from spawning at the initial point on q3dm0...
            if ((spawnPoint->flags & FL_NO_BOTS) && (ent->r.svFlags & SVF_BOT)) {
                continue;    // try again
            }

            // just to be symetric, we have a nohumans option...
            if ((spawnPoint->flags & FL_NO_HUMANS) && !(ent->r.svFlags & SVF_BOT)) {
                continue;    // try again
            }

            break;

        } while (1);
    }

    client->pers.teamState.state = TEAM_ACTIVE;

    // always clear the kamikaze flag
    // ent->s.eFlags &= ~EF_KAMIKAZE;

    ent->s.eFlags &= ~EF_UT_MEDIC;
    ent->s.eFlags &= ~EF_UT_PLANTING;
    ent->s.eFlags &= ~EF_UT_DEFUSING;

    // always reset last zoom time
    client->lastZoomTime = 0;

    // toggle the teleport bit so the client knows to not lerp and never clear the voted flag
    flags  = ent->client->ps.eFlags & (EF_TELEPORT_BIT | EF_VOTED | EF_TEAMVOTED);
    flags ^= EF_TELEPORT_BIT;

    // clear everything but the persistant data
    saved           = client->pers;
    savedSess       = client->sess;
    savedPing       = client->ps.ping;
    accuracy_hits   = client->accuracy_hits;
    accuracy_shots  = client->accuracy_shots;
    //savedAreaBits = client->areabits;

    // this is next updated when player dies and goes to ghost mode
    if (client->ghost) {
        for(i = 0 ; i < MAX_PERSISTANT ; i++) {
            persistant[i] = ent->client->savedpersistant[i];
        }
    }
    else {
        for(i = 0 ; i < MAX_PERSISTANT ; i++) {
            persistant[i] = client->ps.persistant[i];
        }
    }

    eventSequence              = client->ps.eventSequence;
    ghost                      = client->ghost;

    ent->s.angles2[0]          = 0;              // Turn Their Head Back on and remove body smoke
    ent->client->laserSight    = qfalse;
    maparrowp                  = client->maparrow;

    voted                      = client->voted;
    goalbits                   = client->goalbits;

    // reset memory
    memset(client, 0, sizeof(*client));

    client->goalbits           = goalbits;
    client->maparrow           = maparrowp;

    client->voted              = voted;

    client->pers               = saved;
    client->sess               = savedSess;
    client->ps.ping            = savedPing;
    //client->areabits         = savedAreaBits;

    client->accuracy_hits      = accuracy_hits;
    client->accuracy_shots     = accuracy_shots;
    client->lastkilled_client  = -1;

    client->ps.pm_flags       &= ~PMF_WALL_JUMP;

    for (i = 0 ; i < MAX_PERSISTANT ; i++) {
        client->ps.persistant[i] = persistant[i];
    }

    client->ps.eventSequence     = eventSequence;
    // increment the spawncount so the client will detect the respawn
    client->ps.persistant[PERS_TEAM] = client->sess.sessionTeam;
    client->ps.persistant[PERS_SPAWN_COUNT]++;
    client->airOutTime         = level.time + DROWNTIME;

    if (g_survivor.integer) {

        client->ghost = ghost;

        if (client->ghost) {
            ent->client->ps.stats[STAT_UTPMOVE_FLAGS] |= UT_PMF_GHOST;
        }
    }

    trap_GetUserinfo( index, userinfo, sizeof(userinfo));

    // set max health
    // client->pers.maxHealth = 100;

    client->ps.eFlags      = flags;

    ent->s.groundEntityNum = ENTITYNUM_NONE;
    ent->client            = &level.clients[index];

    if(!ent->isNPC) {
        ent->takedamage = qtrue;
    }

    ent->inuse        = qtrue;
    ent->classname    = "player";
    ent->classhash    = HSH_player;
    ent->r.contents   = CONTENTS_BODY;
    ent->clipmask     = MASK_PLAYERSOLID;
    ent->die          = player_die;
    ent->waterlevel   = 0;
    ent->watertype    = 0;
    ent->flags        = 0;

    VectorCopy(playerMins, ent->r.mins);
    VectorCopy(playerMaxs, ent->r.maxs);

    client->ps.clientNum = index;

    // URBAN TERROR - NPC Init
    ent->isLeading    = qfalse;
    ent->health       = client->ps.stats[STAT_HEALTH] = UT_MAX_HEALTH;

    client->ps.stats[STAT_STAMINA] = UT_MAX_HEALTH * UT_STAM_MUL;

    ent->client->noGearChange = qfalse;

    if(ent->r.svFlags & SVF_BOT) {
        char  *gear = Info_ValueForKey (userinfo, "gear");
        utGear_Decompress(gear, &client->pers.gear);
    }

    utGear_Decompress(Info_ValueForKey(userinfo, "gear"), &client->pers.gear);
    ClientSetGear (client);

    if (ent->r.svFlags & SVF_BOT) {
        utPSTurnItemOn(&ent->client->ps, UT_ITEM_NVG);
    }

    G_SetOrigin(ent, spawn_origin);
    VectorCopy(spawn_origin, client->ps.origin);

    // the respawned flag will be cleared after the attack and jump keys come up
    client->ps.pm_flags |= PMF_RESPAWNED;

    trap_GetUsercmd(client - level.clients, &ent->client->pers.cmd);
    SetClientViewAngle(ent, spawn_angles);

    if ((ent->client->sess.sessionTeam != TEAM_SPECTATOR) && !ent->client->ghost) {

        trap_UnlinkEntity(ent);
        G_KillBox(ent);
        trap_LinkEntity(ent);

        // force the base weapon up
        // client->ps.weaponslot = utPSGetWeaponByID ( &client->ps, UT_WP_KNIFE );
        client->ps.weaponstate = WEAPON_READY;
        client->ps.weaponTime  = 500;
    }

    // don't allow full run speed for a bit
    client->ps.pm_flags     |= PMF_TIME_KNOCKBACK;
    client->ps.pm_time       = 100;
    client->LastClientUpdate = 0;
    client->respawnTime      = level.time;
    client->protectionTime   = level.time;

    //client->sess.inactivityTime = level.time + g_inactivity.integer * 1000;
    client->latched_buttons = 0;

    // set default animations
    client->ps.torsoAnim = TORSO_STAND_PISTOL;
    client->ps.legsAnim  = LEGS_IDLE;

    if (level.intermissiontime) {
        MoveClientToIntermission(ent);
    }
    else {

        // fire the targets of the spawn point
        G_UseTargets(spawnPoint, ent);

        client->ps.weaponslot = utPSGetWeaponByID (&client->ps, UT_WP_KNIFE);

        // select the highest weapon number available, after any
        // spawn given items have fired
        for (i = UT_MAX_WEAPONS - 1 ; i >= 0 ; i--) {
            if(UT_WEAPON_GETID(client->ps.weaponData[i] ) != 0) {
                client->ps.weaponslot = i;
                break;
            }
        }
    }

    // run a client frame to drop exactly to the floor,
    // initialize animations and other things
    client->ps.commandTime           = level.time - 100;
    ent->client->pers.cmd.serverTime = level.time;
    ClientThink( ent - g_entities );
    //client->ps.persistant[ PERS_SPAWN_COUNT ]++;

    // positively link the client, even if the command times are weird
    if ((ent->client->sess.sessionTeam != TEAM_SPECTATOR) && !ent->client->ghost) {
        BG_PlayerStateToEntityState(&client->ps, &ent->s, qtrue);
        VectorCopy(ent->client->ps.origin, ent->r.currentOrigin);
        trap_LinkEntity(ent);
    }

    // run the presend to set anything else, follow spectators wait
    // until all clients have been reconnected after map_restart
    if (ent->client->sess.spectatorState != SPECTATOR_FOLLOW) {
        ClientEndFrame(ent);
    }

    // clear entity state values
    BG_PlayerStateToEntityState(&client->ps, &ent->s, qtrue);

    // NPC NPC NPC
    // if(ent->isNPC && ent->ignoreSpawn){
    //     G_NPCKill( ent );
    // }

    // reset aries model offsets
    client->legsPitchAngle    = client->ps.viewangles[PITCH];
    client->legsYawAngle      = client->ps.viewangles[PITCH];
    client->torsoPitchAngle   = client->ps.viewangles[PITCH];
    client->torsoYawAngle     = client->ps.viewangles[PITCH];

    // make sure the grenade held timer is not set
    client->botNotMovedCount   = 0;
    // make sure the grenade mode toggle-after-fire is set to false
    client->botShouldJumpCount = 0;

    // 27 fix for FFA - just make sure they're on team free
    if ((g_gametype.integer == GT_FFA) && (client->sess.sessionTeam != TEAM_SPECTATOR)) {
        client->sess.sessionTeam = TEAM_FREE;
    }

    // reset the map icons
    if (ent->client && ent->client->maparrow) {
        ent->client->maparrow->s.time2   = level.time + 1200; // make it wait a bit so we dont see it lerp across the level
        ent->client->maparrow->nextthink = level.time + ((ent->s.number % 10) * 100);
    }

    // If we're a substitute and this is a survivor game type.
    if (client->pers.substitute && g_survivor.integer) {

        // If we're unable to become a substitute.
        if (!BecomeSubstitute(ent)) {
            // Switch back to active mode.
            Cmd_Substitute_f(ent);
        }
    }

    SendScoreboardSingleMessageToAllClients(ent);
}


void G_SavePositionToFile(gclient_t *client);

/*
===========
ClientDisconnect

Called when a player drops from the server.
Will not be called between levels.

This should NOT be called directly by any game logic,
call trap_DropClient(), which will call this and do
server system housekeeping.
============
*/
void ClientDisconnect(int clientNum) {

    gentity_t   *ent;
    gentity_t   *tent;
    int         i;
    #ifdef USE_AUTH
    char        userinfo[MAX_INFO_STRING];
    char        buffer[AUTH_MAX_STRING_CHARS];
    int         test;
    #endif

    // cleanup if we are kicking a
    // bot that hasn't spawned yet
    G_RemoveQueuedBotBegin(clientNum);

    ent = g_entities + clientNum;

    if (!ent->client) {
        return;
    }

    // save position on disconnect
    G_SavePositionToFile(ent->client);

    // if (ent->client->ps.pm_type != PM_SPECTATOR && ent->client->ps.pm_type != PM_INTERMISSION &&
    //     ent->client->ps.pm_type != PM_DEAD) {
    //     // Don't toss items if dead, spectator,
    //     // ghost (aka dead) or in intermission
    //     TossClientItems(ent);
    // }
    // if (utPSGetItemByID(&ent->client->ps, UT_ITEM_NEUTRALFLAG) != -1) {
    //     Team_ReturnFlag(TEAM_FREE);
    // }

    //Toss Items that need tossing, but remember client->ps is not valid if we're spectating or subbing

    if ((ent->client->sess.sessionTeam != TEAM_SPECTATOR) || (ent->client->sess.spectatorState == SPECTATOR_NOT)) {

        if (!ent->client->pers.substitute && !(ent->s.eFlags & EF_DEAD)) {

            if (utPSGetItemByID(&ent->client->ps, UT_ITEM_REDFLAG) != -1) {
                Team_ReturnFlag(TEAM_RED);
            }
            else if(utPSGetItemByID(&ent->client->ps, UT_ITEM_BLUEFLAG) != -1) {
                Team_ReturnFlag(TEAM_BLUE);
            }
            else if(utPSGetWeaponByID( &ent->client->ps, UT_WP_BOMB ) != -1) {
                Drop_Weapon(ent, UT_WP_BOMB, rand() % 360);
            }

        }
    }

    // stop any following clients
    for (i = 0 ; i < level.maxclients ; i++) {

        if (((level.clients[i].sess.sessionTeam == TEAM_SPECTATOR) || level.clients[i].ghost) &&
             (level.clients[i].sess.spectatorState == SPECTATOR_FOLLOW) &&
             (level.clients[i].sess.spectatorClient == clientNum)) {

            // If we're a substitute
            if (g_entities[i].client->pers.substitute && g_followStrict.integer) {
                Cmd_FollowCycle_f(&g_entities[i], g_entities[i].client->spectatorDirection);
            } else {
                StopFollowing(&g_entities[i], qtrue);
            }
        }
    }

    // send effect if they were completely connected
    if ((ent->client->pers.connected == CON_CONNECTED) && (ent->client->sess.sessionTeam != TEAM_SPECTATOR)) {
        tent = G_TempEntity(ent->client->ps.origin, EV_PLAYER_TELEPORT_OUT);
        tent->s.clientNum = ent->s.clientNum;
    }

    G_LogPrintf("ClientDisconnect: %i\n", clientNum);

    // free up referee if this was the ref
    if (g_refClient.integer == clientNum) {
        g_refClient.integer = -1;
    }

    #ifdef USE_AUTH
    //@Barbatos //@Kalish
    if (auth_mode > 0) {

        trap_GetUserinfo(clientNum, userinfo, sizeof(userinfo));

        Com_Printf(AUTH_PREFIX" sending disconnect for slot %i : %s\n", clientNum,  ent->client->pers.netname);

        buffer[0] = '\0'; test = 1;

        if (test > 0) test = Auth_cat_str(buffer, sizeof(buffer), Info_ValueForKey(userinfo, "ip"), 1);
        if (test > 0) test = Auth_cat_str(buffer, sizeof(buffer), Info_ValueForKey(userinfo, "authc"), 1);
        if (test > 0) test = Auth_cat_str(buffer, sizeof(buffer), ent->client->sess.auth_login, 1);
        if (test > 0) test = Auth_cat_int(buffer, sizeof(buffer), clientNum, 0);
        if (test > 0) test = Auth_cat_str(buffer, sizeof(buffer), ent->client->pers.netname, 1);

        if (test > 0) SV_Authserver_Send_Packet("DISCONNECT", buffer);

        // @Kalish USELESS ATM
        // ent->client->sess.auth_state = 0;
        // ent->client->sess.auth_count = 0;
        // ent->client->sess.auth_time = 0;

        ent->client->sess.auth_level = 0;
        ent->client->sess.renames = 0;
    }
    #endif


    trap_UnlinkEntity(ent);
    ent->s.modelindex                      = 0;
    ent->inuse                             = qfalse;
    ent->classname                         = "disconnected";
    ent->classhash                         = HSH_disconnected;
    ent->client->pers.connected            = CON_DISCONNECTED;
    ent->client->ps.persistant[PERS_TEAM]  = TEAM_FREE;
    ent->client->sess.sessionTeam          = TEAM_FREE;

    // Clear weapon modes array
    memset(ent->client->pers.weaponModes, 0, sizeof(ent->client->pers.weaponModes));

    trap_SetConfigstring(CS_PLAYERS + clientNum, "");

    CalculateRanks();

    if (ent->r.svFlags & SVF_BOT) {
        BotAIShutdownClient(clientNum, qfalse);
    }

    G_WriteClientSessionData(ent->client);
    ent->client->sess.sessionTeam = TEAM_SPECTATOR;

    BuildTeamLists();

    // if there was active vote referencing this client (eg. kick), reset the vote
    if (level.voteClientArg == clientNum) {
        CheckVote(qtrue);
    }

}

/**
 * $(function)
 */
void BuildTeamLists(void)
{
	char  blueTeamList[64];
	char  redTeamList[64];
	int   g = 0, k = 0, team;

	if((g_gametype.integer >= GT_TEAM) && (g_gametype.integer != GT_JUMP))
	{
		memset(blueTeamList, 0, sizeof(blueTeamList));
		memset(redTeamList, 0, sizeof(redTeamList));

		for (g = 0, k = 1 ; g < level.maxclients && g < 64 ; g++, k++)
		{
			if(level.clients[g].pers.connected != CON_CONNECTED)
			{
				continue;
			}

			team = level.clients[g].sess.sessionTeam;

			switch(team)
			{
				case TEAM_RED:
					{
						Q_strcat(redTeamList, 64, va("%c", (char)(k + 64)));
						break;
					}

				case TEAM_BLUE:
					{
						Q_strcat(blueTeamList, 64, va("%c", (char)(k + 64)));
						break;
					}
			}
		}

		trap_Cvar_Set( "g_redTeamList", redTeamList );
		trap_Cvar_Set( "g_blueTeamList", blueTeamList );
	}
}

// MapArrowSpawn [27]
// Spawns in an entity to dopelganger the
//    YAW
//    TRANSLATION
//    HEALTH
// of a particular client

void MapArrowSpawn(int ClientNum)
{
	gentity_t  *base;
	gentity_t  *ent;
	gclient_t  *arrowClient;
	int	   j;

	base = NULL;

	//check to see we dont already have one
	for (j = 0; j < MAX_GENTITIES; j++)
	{
		ent = &g_entities[j];

//		  if (!Q_stricmp(ent->classname, "fl"))
		if (ent->classhash==HSH_fl)
		{
			if (ClientNum == ent->health)
			{
				base = ent;
				break;
			}
		}
	}

	if (base == NULL)
	{
		base = G_Spawn();
	}

	base->classname       = "fl";
	base->classhash       = HSH_fl;
	base->nextthink       = level.time; // Fire this frame
	base->health	      = ClientNum;

	arrowClient	      = &level.clients[ClientNum];
	arrowClient->maparrow = base;

	base->s.pos.trType    = TR_STATIONARY;
	base->s.apos.trType   = TR_STATIONARY;
	base->think	      = MapArrowThink;
	base->r.svFlags      |= SVF_BROADCAST;
	base->r.svFlags      |= SVF_CLIENTMASK;
	base->r.singleClient  = 0;
	base->s.frame	      = 3; //start as speccy
	base->s.legsAnim      = 0;
	base->s.time2	      = level.time + 1000; // s.time2 is used as a teleport bit.  Set it to level.time+1000 when a player spawns.
	base->s.eType	      = ET_HUDARROW;
	trap_LinkEntity (base);
}

/**
 * $(function)
 */
void MapArrowThink(gentity_t *ent )
{
	gentity_t  *player;
	gclient_t  *arrowClient;
	gclient_t  *otherClient;
	int	   i;

	player = g_entities + ent->health; // pointer addition - how novel.

	if (!player->client)
	{
		G_FreeEntity(ent);
		return;
	}

	if (!player->inuse)
	{
		G_FreeEntity(ent);
		return;
	}

	if (player->client->pers.connected == CON_DISCONNECTED)
	{
		G_FreeEntity( ent );
		return;
	}

	ent->nextthink	  = level.time + 1000; //makes sure it stay staggered

	ent->s.origin2[0] = ent->s.pos.trBase[0];
	ent->s.origin2[1] = ent->s.pos.trBase[1];
//	ent->s.origin2[2]=ent->s.pos.trBase[2];

	ent->s.pos.trBase[0] = ((int)player->s.pos.trBase[0] / 10) * 10;
	ent->s.pos.trBase[1] = ((int)player->s.pos.trBase[1] / 10) * 10;
//	ent->s.pos.trBase[2]=((int)player->s.pos.trBase[2]/10)*10;

	ent->s.pos.trTime     = level.time;
	ent->s.eFlags	      = player->s.eFlags;
	ent->s.frame	      = player->client->sess.sessionTeam;

	ent->s.apos.trBase[0] = ent->s.apos.trBase[1];
	ent->s.apos.trBase[1] = ((int)player->s.apos.trBase[1] / 32) * 32;

	//	ent->s.apos.trTime=level.time;
	ent->s.clientNum = ent->health;
	ent->s.powerups  = player->s.powerups;

	// Clear clients transmit bits.
	ent->r.singleClient = 0;

	// Set arrow client pointer.
	arrowClient	      = &level.clients[ent->s.clientNum];

	arrowClient->maparrow = ent;

	// If arrow client is not a spectator.
	if (arrowClient->sess.sessionTeam != TEAM_SPECTATOR)
	{
		// Loop through all the other clients.
		for (i = 0; i < level.maxclients && i < 32; i++)
		{
			// If this client is not the arrow client.
			if (ent->s.clientNum != i)
			{
				// Set other client pointer.
				otherClient = &level.clients[i];

				// If client is not connected.
				if (otherClient->pers.connected == CON_DISCONNECTED)
				{
					continue;
				}

				// If we're playing a team based game type.
				if ((g_gametype.integer >= GT_TEAM) && (g_gametype.integer != GT_JUMP))
				{
					// If client is a spectator, on the same team as the arrow client or dead.
					if ((otherClient->sess.sessionTeam == TEAM_SPECTATOR) || (arrowClient->sess.sessionTeam == otherClient->sess.sessionTeam) ||
					    (!g_followStrict.integer /*g_followEnemy.integer*/ && ((otherClient->ps.pm_type == PM_DEAD) || otherClient->ghost)))
					{
						// Set client bit.
						ent->r.singleClient |= 1 << i;
					}
				}
				else
				{
					// If client is a spectator or is dead. @Barbatos: draw everyone if gametype = JUMP
					if ((g_gametype.integer == GT_JUMP) || ((otherClient->sess.sessionTeam == TEAM_SPECTATOR) ||
					    (!g_followStrict.integer /*g_followEnemy.integer*/ && ((otherClient->ps.pm_type == PM_DEAD) || otherClient->ghost))))
					{
						// Set client bit.
						ent->r.singleClient |= 1 << i;
					}
				}
			}
		}
	}
}

//Checks for contraband weapon combos.	dirty haxors
int Contraband( gentity_t *ent )
{
	int  primaryCount   = 0;
	int  secondaryCount = 0;
	int  negevCount     = 0;
	int  pistolCount    = 0;
	int  id 	    = 0;
	int  slot;

	if (g_cheats.integer)
	{
		return 0;
	}

	//Make sure they're not holding anything they shouldnt have
	for (slot = UT_MAX_WEAPONS - 1 ; slot >= 0 ; slot--)
	{
		id = UT_WEAPON_GETID(ent->client->ps.weaponData[slot]) ;

		switch (id)
		{
			case UT_WP_LR:
			case UT_WP_G36:
			case UT_WP_PSG1:
			case UT_WP_SR8:
			case UT_WP_AK103:
			case UT_WP_M4:
			case UT_WP_HK69:
				primaryCount++;

				if (primaryCount > 1)
				{
					UT_WEAPON_SETID      ( ent->client->ps.weaponData[slot], 0 );
					UT_WEAPON_SETBULLETS ( ent->client->ps.weaponData[slot], 0 );
					UT_WEAPON_SETCLIPS   ( ent->client->ps.weaponData[slot], 0 );
					ent->client->noGearChange = qtrue;
				}
				break;

			case UT_WP_NEGEV:
				primaryCount++;
				negevCount++;

				if (primaryCount > 1)
				{
					UT_WEAPON_SETID      ( ent->client->ps.weaponData[slot], 0 );
					UT_WEAPON_SETBULLETS ( ent->client->ps.weaponData[slot], 0 );
					UT_WEAPON_SETCLIPS   ( ent->client->ps.weaponData[slot], 0 );
					ent->client->noGearChange = qtrue;
				}
				break;

			case UT_WP_BERETTA:
			case UT_WP_DEAGLE:
			case UT_WP_GLOCK:
			case UT_WP_COLT1911:
			//case UT_WP_MAGNUM:
				pistolCount++;

				if (pistolCount > 1)
				{
					UT_WEAPON_SETID      ( ent->client->ps.weaponData[slot], 0 );
					UT_WEAPON_SETBULLETS ( ent->client->ps.weaponData[slot], 0 );
					UT_WEAPON_SETCLIPS   ( ent->client->ps.weaponData[slot], 0 );
					ent->client->noGearChange = qtrue;
				}
				break;
		}
	}

	//Check the secondaries now
	for (slot = UT_MAX_WEAPONS - 1 ; slot >= 0 ; slot--)
	{
		id = UT_WEAPON_GETID(ent->client->ps.weaponData[slot]) ;

		switch (id)
		{
			case UT_WP_SPAS12:
			case UT_WP_MP5K:
			case UT_WP_UMP45:
			case UT_WP_MAC11:
            //case UT_WP_P90:
            //case UT_WP_BENELLI:
			/*case UT_WP_DUAL_BERETTA:
			case UT_WP_DUAL_DEAGLE:
			case UT_WP_DUAL_GLOCK:
			case UT_WP_DUAL_MAGNUM:*/

				secondaryCount++;

				if (((secondaryCount > 0) && (negevCount > 0)) || ((secondaryCount > 1) && (primaryCount > 0)))
				{
					UT_WEAPON_SETID      ( ent->client->ps.weaponData[slot], 0 );
					UT_WEAPON_SETBULLETS ( ent->client->ps.weaponData[slot], 0 );
					UT_WEAPON_SETCLIPS   ( ent->client->ps.weaponData[slot], 0 );
					ent->client->noGearChange = qtrue;
				}
				break;
		}
	}
	return 0;
}
