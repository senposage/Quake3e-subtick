/**
 * Filename: g_cmds.c
 *
 * Copyright (C) 1999-2005 Id Software, Inc.
 * Copyright (C) 2000-2009 FrozenSand
 *
 * This file is part of Urban Terror source code.
 *
 * Definitions shared by both the server game and client game
 * modules because games can change separately from the main system
 * version we need a second version that must match between game and cgame
 */
#include "g_local.h"

/* for the voice chats */
#include "../ui/menudef.h"

/**
 * Cmd_Score_f
 *
 * Sends the scoreboard message to the given client
 */
void Cmd_Score_f(gentity_t *ent) {

	char	   entry[1024];
	char	   string[1400];
	int 	   stringlength;
	int 	   i;
	int 	   j;
	gclient_t  *cl;
	int 	   numSorted;
	char	   login[32];
	char	   defaultLogin[4] = "---";

	// send the latest information on all clients
	string[0]	 = 0;
	stringlength = 0;

	numSorted	 = level.numConnectedClients;

	// Build the score message from the sorted clients.
	for (i = 0; i < numSorted ; i++) {

		int  ping;
		int  status;
		cl = &level.clients[level.sortedClients[i]];

		//i f connected then get the ping from the client info, 999 is the cap
		ping = 999;
		if (cl->pers.connected != CON_CONNECTING) {
			ping = cl->ps.ping; /* < 999 ? cl->ps.ping : 999; */
			ping = ping < 999 ? cl->ps.ping : 999;
		}

		//if client is a ghost
		if (cl->ghost && (cl->ps.pm_flags & PMF_FOLLOW)) {
			ping = cl->savedping;
		} else {
			cl->savedping = ping;
		}

		if (cl->pers.substitute) {
			//if the client is a substitute
			status = 2;
		} else if (cl->ghost || cl->ps.stats[STAT_HEALTH] <= 0) {
			//if the client is dead
			status = 1;
		} else {
			//normal status
			status = 0;
		}

#ifdef USE_AUTH
		//@Barbatos
		if( strlen(cl->sess.auth_login) < 2 ) {
			//if the client is not authed
			Q_strncpyz(login, defaultLogin, sizeof(login));
		} else {
			//if the client is authed
			Q_strncpyz(login, cl->sess.auth_login, sizeof(login));
		}
#else
		Q_strncpyz(login, defaultLogin, sizeof(login));
#endif

		/* Build the score string */
		Com_sprintf(entry, sizeof(entry),
						" %i %i %i %i %i %i %i %i %i %i %s",
						level.sortedClients[i],
						cl->ps.persistant[PERS_SCORE],
						ping,
						(level.time - cl->pers.enterTime +
						cl->survivor.time) / 60000,
						status,
						cl->ps.persistant[PERS_KILLED],
						cl->sess.jumpRun,
                        cl->ps.persistant[PERS_JUMPWAYS] & 0xFF,  // @r00t:JumpWayColors
                        cl->ps.persistant[PERS_JUMPWAYS] >> 8,    // @r00t:JumpWayColors
						cl->ps.persistant[PERS_JUMPBESTTIME],
						login
			   );

		j = strlen(entry);

		if (stringlength + j > 1024) {
			break;
		}

		strcpy(string + stringlength, entry);
		stringlength += j;
	}

	// Send the scores message to the client
	trap_SendServerCommand(ent - g_entities, va("scores %i %i %i%s", i,
				   level.teamScores[TEAM_RED],
				   level.teamScores[TEAM_BLUE],
				   string));
}

/**
 * Cmd_ScoreSingle_f
 */
void Cmd_ScoreSingle_f(gentity_t *src, gentity_t *dst) {

	gclient_t  *cl;
	int 	   ping, status;
	char	   login[32];
	char	   defaultLogin[4] = "---";

	//if invalid pointers
	if (!src || !dst) {
		return;
	}

	//if invalid entities
	if (!src->inuse || !src->client || !dst->inuse || !dst->client) {
		return;
	}

	//set client pointer.
	cl = src->client;

	//if the client is connected
	if (cl->pers.connected == CON_CONNECTED) {
		ping = cl->ps.ping < 999 ? cl->ps.ping : 999;
	} else {
		ping = 999;
	}

	//if the client is a ghost.
	if (cl->ghost && (cl->ps.pm_flags & PMF_FOLLOW)) {
		ping = cl->savedping;
	} else {
		cl->savedping = ping;
	}

	if (cl->pers.substitute) {
		//if the client is a substitute
		status = 2;
	}
	else if (cl->ghost || cl->ps.stats[STAT_HEALTH] <= 0) {
		//if the client is dead
		status = 1;
	}
	else {
		//normal status
		status = 0;
	}

#ifdef USE_AUTH
	//@Barbatos
	if( strlen(cl->sess.auth_login) < 2 ) {
		//if the client is not authed
		Q_strncpyz(login, defaultLogin, sizeof(login));
	} else {
		//if the client is authed
		Q_strncpyz(login, cl->sess.auth_login, sizeof(login));
	}
#else
		Q_strncpyz(login, defaultLogin, sizeof(login));
#endif

	// Send score command to the client
	trap_SendServerCommand(dst - g_entities, va("scoress %i %i %i %i %i %i %i %i %i %i %s",
							   cl - level.clients,
							   cl->ps.persistant[PERS_SCORE],
							   ping,
							   (level.time - cl->pers.enterTime +
							   cl->survivor.time) / 60000,
							   status,
							   cl->ps.persistant[PERS_KILLED],
							   cl->sess.jumpRun,
                               cl->ps.persistant[PERS_JUMPWAYS] & 0xFF, // @r00t:JumpWayColors
                               cl->ps.persistant[PERS_JUMPWAYS] >> 8,   // @r00t:JumpWayColors
							   cl->ps.persistant[PERS_JUMPBESTTIME],
							   login
						   ));
}

/**
 * Cmd_ScoreDouble_f
 */
void Cmd_ScoreDouble_f(gentity_t *src1, gentity_t *src2, gentity_t *dst) {

	gclient_t  *cl1;
	gclient_t  *cl2;
	int 	   ping1, ping2;
	int 	   status1, status2;
	char	   login1[32];
	char	   login2[32];
	char	   defaultLogin[4] = "---";

	//if invalid pointers
	if (!src1 || !src2 || !dst) {
		return;
	}

	//if invalid entities
	if (!src1->inuse || !src1->client || !src2->inuse ||
		!src2->client || !dst->inuse || !dst->client) {
		return;
	}

	//set client pointer
	cl1 = src1->client;

	//if the client is connected
	if (cl1->pers.connected == CON_CONNECTED) {
		ping1 = cl1->ps.ping < 999 ? cl1->ps.ping : 999;
	} else {
		ping1 = 999;
	}

	//if client is a ghost
	if (cl1->ghost && (cl1->ps.pm_flags & PMF_FOLLOW)) {
		ping1 = cl1->savedping;
	} else {
		cl1->savedping = ping1;
	}

	if (cl1->pers.substitute) {
		//if the client is a substitute
		status1 = 2;
	} else if (cl1->ghost || cl1->ps.stats[STAT_HEALTH] <= 0) {
		//if the client is dead
		status1 = 1;
	} else {
		//normal status
		status1 = 0;
	}

	//set the client pointer
	cl2 = src2->client;

	//if the client is connected
	if (cl2->pers.connected == CON_CONNECTED) {
		ping2 = cl2->ps.ping < 999 ? cl2->ps.ping : 999;
	} else {
		ping2 = 999;
	}

	//if the client is a ghost
	if (cl2->ghost && (cl2->ps.pm_flags & PMF_FOLLOW)) {
		ping2 = cl2->savedping;
	} else {
		cl2->savedping = ping2;
	}

	if (cl2->pers.substitute) {
		//if the client is a substitute
		status2 = 2;
	} else if (cl2->ghost || cl2->ps.stats[STAT_HEALTH] <= 0) {
		//if the client is dead
		status2 = 1;
	} else {
		//normal status
		status2 = 0;
	}

#ifdef USE_AUTH
	//@Barbatos
	if( strlen(cl1->sess.auth_login) < 2 ) {
		//if the client is not authed
		Q_strncpyz(login1, defaultLogin, sizeof(login1));
	} else {
		//if the client is authed
		Q_strncpyz(login1, cl1->sess.auth_login, sizeof(login1));
	}

	//@Barbatos
	if( strlen(cl2->sess.auth_login) < 2 ) {
		//if the client is not authed
		Q_strncpyz(login2, defaultLogin, sizeof(login2));
	} else {
		//if the client is authed
		Q_strncpyz(login2, cl2->sess.auth_login, sizeof(login2));
	}
#else
		Q_strncpyz(login1, defaultLogin, sizeof(login1));
		Q_strncpyz(login2, defaultLogin, sizeof(login2));
#endif

	/* Send score command to client. */
	trap_SendServerCommand(dst - g_entities, va(
							"scoresd %i %i %i %i %i %i %i %i %i %i %s"
								   " %i %i %i %i %i %i %i %i %i %i %s",

							cl1 - level.clients,
							cl1->ps.persistant[PERS_SCORE],
							ping1,
							(level.time - cl1->pers.enterTime +
							cl1->survivor.time) / 60000,
							status1,
							cl1->ps.persistant[PERS_KILLED],
							cl1->sess.jumpRun,
                            cl1->ps.persistant[PERS_JUMPWAYS] & 0xFF,  // @r00t:JumpWayColors
                            cl1->ps.persistant[PERS_JUMPWAYS] >> 8,    // @r00t:JumpWayColors
							cl1->ps.persistant[PERS_JUMPBESTTIME],
							login1,
							cl2 - level.clients,
							cl2->ps.persistant[PERS_SCORE],
							ping2,
							(level.time - cl2->pers.enterTime +
							cl2->survivor.time) / 60000,
							status2,
							cl2->ps.persistant[PERS_KILLED],
							cl2->sess.jumpRun,
                            cl2->ps.persistant[PERS_JUMPWAYS] & 0xFF,  // @r00t:JumpWayColors
                            cl2->ps.persistant[PERS_JUMPWAYS] >> 8,    // @r00t:JumpWayColors
							cl2->ps.persistant[PERS_JUMPBESTTIME],
							login2
						));
}

/**
 * $(function)
 */
void Cmd_ScoreGlobal_f(gentity_t *entity)
{
	/* If an invalid pointer. */
	if (!entity) {
		return;
	}

	/* If an invalid entity. */
	if (!entity->inuse || !entity->client) {
		return;
	}

	/* Send score command to client. */
	trap_SendServerCommand(entity - g_entities, va("scoresg %i %i",
													level.teamScores[TEAM_RED],
													level.teamScores[TEAM_BLUE]));
}

/**
 * CheatsOk
 *
 * Determines if cheats are enabled on this server
 */
qboolean CheatsOk(gentity_t *ent)
{
	/* If cheats arent on, then they arent on */
	if (!g_cheats.integer) {
		trap_SendServerCommand(ent - g_entities,
				va("print \"Cheats are not enabled on this"
					" server.\n\""));
		return qfalse;
	}

	/* Cheats required you to be alive */
	if (ent->health <= 0) {
		trap_SendServerCommand(ent - g_entities,
				va("print \"You must be alive to use this"
					" command.\n\""));
		return qfalse;
	}

	/* Go for it */
	return qtrue;
}

/*
==================
ConcatArgs
==================
*/
char *ConcatArgs(int start)
{
	int 	 i, c, tlen;
	static char  line[MAX_STRING_CHARS];
	int 	 len;
	char		 arg[MAX_STRING_CHARS];

	len = 0;
	c	= trap_Argc();

	for (i = start ; i < c ; i++) {
		trap_Argv(i, arg, sizeof(arg));
		tlen = strlen(arg);

		if (len + tlen >= MAX_STRING_CHARS - 1) {
			break;
		}

		memcpy(line + len, arg, tlen);
		len += tlen;

		if (i != c - 1) {
			line[len] = ' ';
			len++;
		}
	}

	line[len] = 0;

	return line;
}

/**
 * SuperSanitizeString
 *
 * Remove control characters, spaces, allows case
 */
void SuperSanitizeString(char *in, char *out)
{
	while (*in) {
		if (*in == '^') { //@r00t:Was 27... wtf? we want '^' not ESC !
			in += 2;		/* skip color code */
			continue;
		}

		if (*in < 33) {
			in++;
			continue;
		}

		if (*in > 126) {
			in++;
			continue;
		}

		*out++ = *in++ ;
	}

	*out = 0;
}

/*
==================
SanitizeString

Remove control characters, sets to lowercase

==================
*/

void SanitizeString(char *in, char *out)
{
	while (*in) {
		if (*in == '^') { //@r00t:Was 27... wtf? we want '^' not ESC !
			in += 2;		/* skip color code */
			continue;
		}

		if (*in < 32) {
			in++;
			continue;
		}

		*out++ = tolower(*in++);
	}

	*out = 0;
}

/**
 * ClientNumberFromString
 *
 * Returns a player number for either a number or name string
 * Returns -1 if invalid
 */
int ClientNumberFromString(gentity_t *to, char *s) {

	gclient_t  *cl;
	int 	   idnum, i;
	char	   s2[MAX_STRING_CHARS];
	char	   n2[MAX_STRING_CHARS];

	// Checking whether s is a numeric player handle.
	for(i = 0; s[i] >= '0' && s[i] <= '9'; i++);

	if((!s[i])) {

		// We got a numeric player handle as input.
		idnum = atoi(s);

		if ((idnum < 0) || (idnum >= level.maxclients)) {
			trap_SendServerCommand(to - g_entities, va("print \"Bad client slot: %i\n\"", idnum));
			return -1;
		}

		cl = &level.clients[idnum];

		if (cl->pers.connected != CON_CONNECTED) {
			trap_SendServerCommand(to - g_entities, va("print \"Player %i is not connected.\n\"", idnum));
			return -1;
		}

		return idnum;

	}

	// Search for a full name match
	SanitizeString(s, s2);

	for (idnum = 0, cl = level.clients; idnum < level.maxclients; idnum++, cl++) {

		// Client is not connected
		if (cl->pers.connected != CON_CONNECTED) {
			continue;
		}

		SanitizeString(cl->pers.netname, n2);

		if (!strcmp(n2, s2)) {
			return idnum;
		}
	}

	trap_SendServerCommand(to - g_entities, va("print \"Player %s is not on the server.\n\"", s));
	return -1;

}

/**
 * Cmd_Give_f
 *
 * Give items to a client
 */
void Cmd_Give_f(gentity_t *ent)
{
	char	   *name;
	gitem_t    *it;
	int    i;
	qboolean   give_all;
	gentity_t  *it_ent;
	trace_t    trace;

	if (!CheatsOk(ent)) {
		return;
	}

	name = ConcatArgs(1);

	if (Q_stricmp(name, "all") == 0) {
		give_all = qtrue;
	} else {
		give_all = qfalse;
	}

	if (give_all || (Q_stricmp(name, "health") == 0)) {
		ent->health = UT_MAX_HEALTH;

		if (!give_all) {
			return;
		}
	}

	if (give_all || (Q_stricmp(name, "weapons") == 0)) {
		int  weapon;

		for (weapon = UT_WP_NONE + 1; weapon < UT_WP_NUM_WEAPONS;
				weapon++) {
			if ((weapon != UT_WP_BOMB) && (weapon != UT_WP_GRENADE_FLASH) && (weapon != UT_WP_GRENADE_FRAG)){
				utPSGiveWeapon(&ent->client->ps, weapon,
					ent->client->pers.weaponModes[weapon] -
					'0');
			}
		}

		if (!give_all) {
			return;
		}
	}

	if (give_all || (Q_stricmp(name, "items") == 0)) {
		int  item;

		for (item = UT_ITEM_VEST + 1; item <= UT_ITEM_AMMO; item++) {
			utPSGiveItem(&ent->client->ps, item);
		}

		if (!give_all) {
			return;
		}
	}

	if (give_all || (Q_stricmp(name, "ammo") == 0)) {
		for (i = 0 ; i < MAX_WEAPONS ; i++) {
			int weaponSlot = utPSGetWeaponByID(&ent->client->ps, i);

			if (-1 != weaponSlot) {
				UT_WEAPON_SETCLIPS(
					ent->client->ps.weaponData[weaponSlot],
					100);
				UT_WEAPON_SETBULLETS(
					ent->client->ps.weaponData[weaponSlot],
					bg_weaponlist[UT_WEAPON_GETID(
						ent->client->ps.weaponData
						[weaponSlot])].bullets);
			}
		}

		if (!give_all) {
			return;
		}
	}

	/* spawn a specific item right on the player */
	if (!give_all) {
		it = BG_FindItem(name);

		if (!it) {
		  // If the item was not found by the pickup name try
		  // finding it by classname as well
		  it = BG_FindItemForClassname ( name );
		  if (!it ) {
			  return;
			}
		}

		if ( it->giType == IT_WEAPON ) {
		  int weaponSlot;

			utPSGiveWeapon(&ent->client->ps, it->giTag,
				ent->client->pers.weaponModes[it->giTag] -
				'0');

			weaponSlot = utPSGetWeaponByID(&ent->client->ps, it->giTag);

			if (-1 != weaponSlot) {
				UT_WEAPON_SETCLIPS(
					ent->client->ps.weaponData[weaponSlot],
					100);
				UT_WEAPON_SETBULLETS(
					ent->client->ps.weaponData[weaponSlot],
					bg_weaponlist[UT_WEAPON_GETID(
						ent->client->ps.weaponData
						[weaponSlot])].bullets);
	  }

	  return;
	}

		if (!strcmp(it->classname, "ut_weapon_bomb")) {
			return;
		}

		it_ent		  = G_Spawn();
		VectorCopy(ent->r.currentOrigin, it_ent->s.origin);
		it_ent->classname = it->classname;
		HASHSTR(it->classname,it_ent->classhash);
		G_SpawnItem(it_ent, it);
		FinishSpawningItem(it_ent);
		memset(&trace, 0, sizeof(trace));
		Touch_Item(it_ent, ent, &trace);

		if (it_ent->inuse) {
			G_FreeEntity(it_ent);
		}
	}
}

/**
 * Cmd_God_f
 *
 * Sets client to godmode
 */
void Cmd_God_f(gentity_t *ent)
{
	char  *msg;

	if (!CheatsOk(ent)) {
		return;
	}

	ent->flags ^= FL_GODMODE;

	if (!(ent->flags & FL_GODMODE)) {
		msg = "godmode OFF\n";
	} else {
		msg = "godmode ON\n";
	}

	trap_SendServerCommand(ent - g_entities, va("print \"%s\"", msg));
}

/**
 * Cmd_Notarget_f
 *
 * Sets client to no target
 */
void Cmd_Notarget_f(gentity_t *ent)
{
	char  *msg;

	if (!CheatsOk(ent)) {
		return;
	}

	ent->flags ^= FL_NOTARGET;

	if (!(ent->flags & FL_NOTARGET)) {
		msg = "notarget OFF\n";
	} else {
		msg = "notarget ON\n";
	}

	trap_SendServerCommand(ent - g_entities, va("print \"%s\"", msg));
}

/**
 * Cmd_Noclip_f
 *
 * Set client to noclip
 */
void Cmd_Noclip_f(gentity_t *ent)
{
	char  *msg;

	if (!CheatsOk(ent)) {
		return;
	}

	if (ent->client->noclip) {
		msg = "noclip OFF\n";
	} else {
		msg = "noclip ON\n";
	}

	ent->client->noclip = !ent->client->noclip;

	trap_SendServerCommand(ent - g_entities, va("print \"%s\"", msg));
}

/**
 * Cmd_LevelShot_f
 *
 * This is just to help generate the level pictures
 * for the menus.  It goes to the intermission immediately
 * and sends over a command to the client to resize the view,
 * hide the scoreboard, and take a special screenshot.
 */
void Cmd_LevelShot_f(gentity_t *ent)
{
	if (!CheatsOk(ent)) {
		return;
	}

	/* doesn't work in single player */
	if (g_gametype.integer != 0) {
		trap_SendServerCommand(ent - g_entities,
					   "print \"Must be in g_gametype 0 for"
					   " levelshot\n\"");
		return;
	}

	BeginIntermission();
	trap_SendServerCommand(ent - g_entities, "clientLevelShot");
}

/**
 * Cmd_LevelShot_f
 *
 * This is just to help generate the level pictures for the menus.
 * It goes to the intermission immediately and sends over a command
 * to the client to resize the view hide the scoreboard, and take a
 * special sceenshot.
 */
void Cmd_TeamTask_f(gentity_t *ent)
{
	char  userinfo[MAX_INFO_STRING];
	char  arg[MAX_TOKEN_CHARS];
	int   task;
	int   client = ent->client - level.clients;

	if (trap_Argc() != 2) {
		return;
	}

	trap_Argv(1, arg, sizeof(arg));
	task = atoi(arg);

	trap_GetUserinfo(client, userinfo, sizeof(userinfo));
	Info_SetValueForKey(userinfo, "teamtask", va("%d", task));
	trap_SetUserinfo(client, userinfo);
	ClientUserinfoChanged(client);
}

/**
 * Cmd_Kill_f
 */
void Cmd_Kill_f(gentity_t *ent) {

	// Not a valid pointer
	if (!ent->client) {
		return;
	}

	// If the client was spectator or already dead
	if ((ent->client->sess.sessionTeam == TEAM_SPECTATOR) || ent->client->ghost) {
		return;
	}

	// IF he has no health left
	if (ent->health <= 0) {
		return;
	}

	// If game is paused.
	if (level.pauseState & UT_PAUSE_ON) {
		return;
	}

	ent->flags &= ~FL_GODMODE;
	ent->client->ps.stats[STAT_HEALTH] = ent->health = -999;
	player_die(ent, ent, ent, 100000, MOD_SUICIDE);

	// If we are playing jump mode, matchmode is activated, max jump run attempts is defined server side, the client had timers activated and he was doing
	// a jump run meanwhile he killed himself, increment the jumpRunCount variable (the client was trying to obtain 1 more jump attempt for the current map
	// by teleporting himself at the spawn to cross again the ut_jumpstart entity without completing the current jumpRun) and reset the timers.
	if ((g_gametype.integer == GT_JUMP) && (g_matchMode.integer > 0) && (g_jumpruns.integer > 0) && (ent->client->sess.jumpRun != 0) && (ent->client->ps.persistant[PERS_JUMPSTARTTIME] > 0)) {

		ent->client->sess.jumpRunCount++;

		// Log everything for external bots
		G_LogPrintf("ClientJumpRunCanceled: %i - way: %i - attempt: %i of %i\n", ent->client->ps.clientNum, ent->client->sess.jumpWay, ent->client->sess.jumpRunCount, g_jumpruns.integer);
		trap_SendServerCommand(-1, va("print \"%s's run has been " S_COLOR_RED "stopped" S_COLOR_WHITE " (Run: %i of %i)\n\"", ent->client->pers.netname, ent->client->sess.jumpRunCount, g_jumpruns.integer));

		// Check if it's time to switch off jump timers
		if (ent->client->sess.jumpRunCount >= g_jumpruns.integer)
			ent->client->sess.jumpRun = 0;

		// Reset the timer
		ent->client->sess.jumpWay = 0;							   // Clear the jumpWay
		ent->client->ps.persistant[PERS_JUMPSTARTTIME] = 0;		   // Clear the jumpStartTime
		ent->client->ps.persistant[PERS_JUMPWAYS] &= 0xFFFFFF00;   // Clear the jumpWayColor

		SendScoreboardSingleMessageToAllClients(ent);

	}

}

/**
 * BroadcastTeamChange
 *
 * Let everyone know about a team change
 */
void BroadcastTeamChange(gclient_t *client, int oldTeam)
{
	if (client->sess.sessionTeam == TEAM_RED) {
		trap_SendServerCommand(-1, va("ccprint \"%d\" \"%s\" \"%d\"",
			CCPRINT_JOIN, client->pers.netname, 1));
	} else if (client->sess.sessionTeam == TEAM_BLUE) {
		trap_SendServerCommand(-1, va("ccprint \"%d\" \"%s\" \"%d\"",
			CCPRINT_JOIN, client->pers.netname, 2));
	} else if (client->sess.sessionTeam == TEAM_SPECTATOR &&
			oldTeam != TEAM_SPECTATOR) {
		trap_SendServerCommand(-1, va("print \"%s" S_COLOR_WHITE " joined the spectators.\n\"", client->pers.netname));
	} else if (client->sess.sessionTeam == TEAM_FREE) {
		trap_SendServerCommand(-1, va("ccprint \"%d\" \"%s\" \"%d\"",
			CCPRINT_JOIN, client->pers.netname, 0));
	}
}

/**
 * ChangeTeam
 *
 * Client change team
 */
void ChangeTeam(gentity_t *ent)
{
	/* remove god mode */
	ent->flags			  &= ~FL_GODMODE;
	/* remove health */
	ent->client->ps.stats[STAT_HEALTH] = ent->health = 0;
	/* Kill player */
    //@r00t:calling player_die on spectator will result in client seeing his mini scoreboard grayed out.
    //Do NOT disable this check, took me 2 hours to trace it here from cgame :/
    if (ent->client->sess.sessionTeam!=TEAM_SPECTATOR)
	player_die(ent, ent, ent, 100000, MOD_CHANGE_TEAM);
	/* make our friend the ghost :D */
	G_SetGhostMode(ent);
}

/**
 * SetTeam
 */
void SetTeam(gentity_t *ent, char *s, qboolean silent) {

	gclient_t  *client;
	int 	   clientNumber;
	int 	   previousTeam;
	int 	   backupTime;
	int 	   currentTeam;
	int 	   requestedTeam;
	qboolean   noGearChange;
	qboolean   noJoinMessage;
	int 	   backupScore = 0, backupKilled = 0;
	int 	   backupJumpBestTime = 0;
	int 	   backupJumpWays = 0; //@r00t:JumpWayColors

	//if not a valid client
	if (!ent->client) {
		return;
	}

	//set the pointer
	client = ent->client;

	//get the client number
	clientNumber = client - level.clients;

	//request to become a spectator
	if (!Q_stricmp(s, "s") || !Q_stricmp(s, "spectator")) {
		requestedTeam = TEAM_SPECTATOR;
	} else {
		//if we are playing a team based gametype
		if ((g_gametype.integer >= GT_TEAM) && (g_gametype.integer != GT_JUMP)) {
			if (!Q_stricmp(s, "red")) {
				//request to join the red team
				requestedTeam = TEAM_RED;
			} else if (!Q_stricmp(s, "blue")) {
				//request to join the blue team
				requestedTeam = TEAM_BLUE;
			} else {
				//autojoin
				requestedTeam = PickTeam(clientNumber);
			}

			//if we need to maintain team balance.
			if (g_teamForceBalance.integer &&
				(client->sess.pendingTeam == 0)) {
				int  numRedClients, numBlueClients;
				//get the number of clients per team
				numRedClients  = TeamCount(clientNumber, TEAM_RED);
				numBlueClients = TeamCount(clientNumber, TEAM_BLUE);

				//if we're trying to join the red team but
				//there are too many clients on it.
				if ((requestedTeam == TEAM_RED) && (numRedClients - numBlueClients > 1)) {
					if (g_entities[clientNumber].r.svFlags & SVF_BOT) {
						//if this client is a bot
						requestedTeam = TEAM_BLUE;
					} else {
						//inform the player
						trap_SendServerCommand(clientNumber, va ("ccprint \"%d\" \"%d\"", CCPRINT_TEAM_FULL, TEAM_RED));
						return;
					}
				}

				//if we're trying to join the blue team but
				//there are too many clients on it.
				if ((requestedTeam == TEAM_BLUE) && (numBlueClients - numRedClients > 1)) {
					if (g_entities[clientNumber].r.svFlags & SVF_BOT) {
						//if this client is a bot
						requestedTeam = TEAM_RED;
					} else {
						//inform the player
						trap_SendServerCommand(clientNumber, va ("ccprint \"%d\" \"%d\"", CCPRINT_TEAM_FULL, TEAM_BLUE));
						return;
					}
				}
			}
		} else {
			requestedTeam = TEAM_FREE;
		}
	}

	//get the current team
	currentTeam = client->sess.sessionTeam;

	//block if the player is trying to join
	//the same team he belongs to
	if (requestedTeam == currentTeam) {
		return;
	}

	//backup values
	noGearChange = client->noGearChange;

	//if there's a team change request pending
	if (client->sess.pendingTeam) {
		noJoinMessage = qtrue;
	} else {
		noJoinMessage = qfalse;
	}

	//if the player was dead before the team change request
	if ((currentTeam != TEAM_SPECTATOR) && (client->ps.stats[STAT_HEALTH] <= 0)) {
		//Place a corpse instead of the player's real body
		//@Fenix - Something is missing here? FIXME!
	}

	//begin team change
	client->pers.teamState.state = TEAM_BEGIN;

	//if client is not currently spectating
	//if (currentTeam != TEAM_SPECTATOR)
	ChangeTeam(ent);

	//set new team
	client->ps.persistant[PERS_TEAM] = requestedTeam;
	client->sess.sessionTeam	 = requestedTeam;

	//if we are going to spctator team
	if (requestedTeam == TEAM_SPECTATOR) {
		client->sess.spectatorTime	= level.time;
		client->sess.spectatorState = SPECTATOR_FREE;
	} else {
		client->sess.spectatorState = SPECTATOR_NOT;
	}

	//do not spectate anyone
	client->sess.spectatorClient = client - level.clients;

	//set correct parameters
	client->sess.teamLeader = qfalse;
	client->pers.substitute = qfalse;
	client->pers.captain = qfalse;

	//if we need to notify all clients of our team change
	if (!noJoinMessage) {
		if (!silent) {
			BroadcastTeamChange(client, currentTeam);
		}
	}

	//update client userinfo
	ClientUserinfoChanged(clientNumber);

	//team already changed
	client->sess.pendingTeam = 0;

	//update local variables
	previousTeam = currentTeam;
	currentTeam  = requestedTeam;
	backupTime	 = client->pers.enterTime;

	//if we weren't spectating or going to spectate
	if ((previousTeam != TEAM_SPECTATOR) && (currentTeam != TEAM_SPECTATOR)) {
		//make a backup copy.
		backupScore  = client->ps.persistant[PERS_SCORE];
		backupKilled = client->ps.persistant[PERS_KILLED];

		//@Fenix - backup jump best time if needed
		if (g_gametype.integer == GT_JUMP) {
			backupJumpBestTime = client->ps.persistant[PERS_JUMPBESTTIME];
			backupJumpWays = client->ps.persistant[PERS_JUMPWAYS]; //@r00t:JumpWayColors
		}
	}

	//initialize the client
	ClientBegin(clientNumber);

	//re-set current team and time
	client->ps.persistant[PERS_TEAM] = currentTeam;
	client->pers.enterTime		     = backupTime ;

	//if we weren't spectating or going to spectate. */
	if ((previousTeam != TEAM_SPECTATOR) &&
		(currentTeam != TEAM_SPECTATOR)) {
		//restore original values
		client->ps.persistant[PERS_SCORE]  = backupScore;
		client->ps.persistant[PERS_KILLED] = backupKilled;

		//@Fenix - restore jump best time if needed
		if (g_gametype.integer == GT_JUMP) {
			client->ps.persistant[PERS_JUMPBESTTIME] = backupJumpBestTime;
			client->ps.persistant[PERS_JUMPWAYS]     = backupJumpWays;
		}
	}

	// If round has already started enter as a ghost
	if (g_survivor.integer && (currentTeam != TEAM_SPECTATOR) && (level.survivorNoJoin || noGearChange)) {
		//set the ghost mode
		G_SetGhostMode(ent);
		trap_UnlinkEntity(ent);
	}

	// @Fenix - if we are playing jump stop the timer, no matter which team we joined
	if ((g_gametype.integer == GT_JUMP) && (client->sess.jumpRun != 0)) {

		// If matchmode is activated, max jump run attempts is defined server side, the client had timers activated and he was doing a jump run
		// meanwhile he switched team, increment the jumpRunCount variable (the client was trying to obtain 1 more jump attempt for the current map)
		if ((g_matchMode.integer > 0) && (g_jumpruns.integer > 0) && (client->ps.persistant[PERS_JUMPSTARTTIME] > 0)) {

			client->sess.jumpRunCount++;

			// Log everything for external bots
			G_LogPrintf("ClientJumpRunCanceled: %i - way: %i - attempt: %i of %i\n", client->ps.clientNum, client->sess.jumpWay, client->sess.jumpRunCount, g_jumpruns.integer);
			trap_SendServerCommand(-1, va("print \"%s's run has been " S_COLOR_RED "stopped" S_COLOR_WHITE " (Run: %i of %i)\n\"", client->pers.netname, client->sess.jumpRunCount, g_jumpruns.integer));

		}

		client->sess.jumpRun = 0;							  // Stop the timer
		client->sess.jumpWay = 0;							  // Clear the jumpWay
		client->ps.persistant[PERS_JUMPSTARTTIME] = 0;		  // Clear the jumpStartTime
		client->ps.persistant[PERS_JUMPWAYS] &= 0xFFFFFF00;   // Clear the jumpWayColor

	}

	// Update the scoreboard for all the clients
	SendScoreboardSingleMessageToAllClients(ent);

}

/**
 * StopFollowing
 *
 * If the client being followed leaves the game, or you just want to drop
 * to free floating spectator mode.
 */
void StopFollowing(gentity_t *ent, qboolean force)
{
	ent->client->ps.eFlags &= ~EF_CHASE;

	if (!level.intermissiontime && !level.survivorRoundRestartTime) {
		if (!force && g_followStrict.integer &&
			(ent->client->sess.sessionTeam != TEAM_SPECTATOR) &&
			(TeamLiveCount(-1, ent->client->sess.sessionTeam) > 0)) {
			return;
		}
	}

	if (!ent->client->ghost) {
		ent->client->ps.persistant[PERS_TEAM] = TEAM_SPECTATOR;
		ent->client->sess.sessionTeam		  = TEAM_SPECTATOR;
	} else {
		ent->client->ps.stats[STAT_UTPMOVE_FLAGS] |= UT_PMF_GHOST;
	}

	ent->client->sess.spectatorState = SPECTATOR_FREE;
	ent->client->ps.pm_flags	&= ~PMF_FOLLOW;
	ent->client->ps.pm_flags	&= ~PMF_LADDER;
	ent->client->ps.pm_flags	&= ~PMF_DUCKED;

	if (ent->client->ghost) {
		ent->client->ps.pm_flags &= ~PMF_HAUNTING;
	}

	ent->r.svFlags		 &= ~SVF_BOT;
	ent->client->ps.clientNum = ent - g_entities;

	if (ent->client->sess.spectatorClient != -1) {
		gclient_t  *cl = &level.clients[ent->client->sess.spectatorClient];

		int    i;

		for (i = 0; i < 3; i++) {
			ent->client->ps.delta_angles[i] =
				ANGLE2SHORT(cl->ps.viewangles[i] -
				SHORT2ANGLE(ent->client->pers.cmd.angles[i]));
		}

		VectorCopy(cl->ps.viewangles, ent->client->ps.viewangles);
		VectorCopy(cl->ps.origin, ent->client->ps.origin);
		VectorClear(ent->client->ps.velocity);
		ent->client->ps.movementDir = 0;
	}

	ent->client->sess.spectatorClient = ent->client->ps.clientNum;
}

/**
 * Cmd_Team_f
 */
void Cmd_Team_f(gentity_t *ent)
{
	int   oldTeam;
	char  s[MAX_TOKEN_CHARS];

	/* If game is paused. */
	if ((level.pauseState & UT_PAUSE_ON) &&
		(ent->client->sess.sessionTeam != TEAM_SPECTATOR)) {
		trap_SendServerCommand(ent - g_entities, "print \"May not switch teams while game is paused.\n\"");
		return;
	}

	if (trap_Argc() != 2) {
		oldTeam = ent->client->sess.sessionTeam;

		switch (oldTeam) {
		case TEAM_BLUE:
			trap_SendServerCommand(ent - g_entities,
					"print \"Blue team\n\"");
			break;

		case TEAM_RED:
			trap_SendServerCommand(ent - g_entities,
					"print \"Red team\n\"");
			break;

		case TEAM_FREE:
			trap_SendServerCommand(ent - g_entities,
					"print \"Free team\n\"");
			break;

		case TEAM_SPECTATOR:
			trap_SendServerCommand(ent - g_entities,
					"print \"Spectator team\n\"");
			break;
		}

		return;
	}

	if (ent->client->switchTeamTime > level.time) {
		trap_SendServerCommand(ent - g_entities, "print \"May not switch teams more than once per 5 seconds.\n\"");
		return;
	}

	trap_Argv(1, s, sizeof(s));

	/* Reset his inactivity timer */
	if (g_inactivity.integer) {
		ent->client->sess.inactivityTime = level.time +
			g_inactivity.integer * 1000;
	} else {
		ent->client->sess.inactivityTime = level.time + 60 * 1000;
	}

	SetTeam(ent, s, qfalse);

	ent->client->switchTeamTime = level.time + 5000;
}

/**
 * Cmd_Follow_f
 */
void Cmd_Follow_f(gentity_t *ent)
{
	int   i;
	char  arg[MAX_TOKEN_CHARS];

	if (trap_Argc() != 2) {
		if (ent->client->sess.spectatorState == SPECTATOR_FOLLOW) {
			StopFollowing(ent, qfalse);
		}

		return;
	}

	if (ent->client->sess.sessionTeam != TEAM_SPECTATOR) {
		trap_SendServerCommand(ent - g_entities, "print \"You need to be a spectator to use this function\" ");
		return;
	}

	trap_Argv(1, arg, sizeof(arg));
	i = ClientNumberFromString(ent, arg);

	if (i == -1) {
		return;
	}

	/* can't follow self */
	if (&level.clients[i] == ent->client) {
		return;
	}

	/* can't follow another spectator */
	if ((level.clients[i].sess.sessionTeam == TEAM_SPECTATOR) ||
		level.clients[i].ghost) {
		return;
	}

	ent->client->sess.spectatorState  = SPECTATOR_FOLLOW;
	ent->client->sess.spectatorClient = i;
}

/**
 * Cmd_FollowCycle_f
 */
int Cmd_FollowCycle_f(gentity_t *ent, int dir)
{
	int   clientnum;
	int   original;
	qboolean  deadTeam = qfalse;

	/* if they are playing a tournement game, count as a loss */

	/* first set them to spectator */
	if (!ent->client->ghost) {
		if (ent->client->sess.spectatorState == SPECTATOR_NOT) {
			SetTeam(ent, "s", qfalse);
		}
	}

	if (dir < -1 || dir > 1) {
		G_Error("Cmd_FollowCycle_f: bad dir %i", dir);
	}

	/* HACK: Sometimes this gets cleared to 0, please identify when */
	if (dir == 0) {
		dir = 1;
	}

	clientnum = ent->client->sess.spectatorClient;
	original  = clientnum;

	/* See if every player on our team is dead. */
	if ((ent->client->sess.sessionTeam == TEAM_RED) ||
		(ent->client->sess.sessionTeam == TEAM_BLUE)) {
		if (TeamLiveCount(-1, ent->client->sess.sessionTeam) == 0) {
			deadTeam = qtrue;
		}
	}

	do {
		clientnum += dir;

		if (clientnum >= level.maxclients) {
			clientnum = 0;
		}

		if (clientnum < 0) {
			clientnum = level.maxclients - 1;
		}

		/* can only follow connected clients */
		if (level.clients[clientnum].pers.connected != CON_CONNECTED) {
			continue;
		}

		/* can't follow another spectator */
		if ((level.clients[clientnum].sess.sessionTeam ==
			TEAM_SPECTATOR) || level.clients[clientnum].ghost) {
			continue;
		}

		/*
		 * If we cant spectate enemies and this guy isnt on our team
		 * then dont let em
		 */
		if (ent->client->sess.sessionTeam != TEAM_SPECTATOR) {
			if (g_followStrict.integer &&
				(level.clients[clientnum].sess.sessionTeam !=
				 ent->client->sess.sessionTeam)) {
				if (!deadTeam) {
					continue;
				} else if (ent->client->pers.substitute &&
						!g_survivor.integer) {
					continue;
				}
			}
		}

		if (level.clients[clientnum].ps.pm_type != PM_NORMAL) {
			continue;
		}

		/* Cant follow a dead guy */
		if (level.clients[clientnum].ps.stats[STAT_HEALTH] <= 0) {
			continue;
		}

		/* this is good, we can use it */
		ent->client->sess.spectatorClient     = clientnum;
		ent->client->sess.spectatorState      = SPECTATOR_FOLLOW;
		ent->s.eFlags                        |= EF_DEAD;
		ent->client->ps.persistant[PERS_TEAM] = level.clients[clientnum].sess.sessionTeam;


        if (g_gametype.integer == GT_JUMP) { //@r00t:Fixes bug 859 - send start time to client when spectator follows him
            gentity_t  *tempEntity;
	        tempEntity                   = G_TempEntity(level.clients[clientnum].ps.origin, EV_JUMP_START);
		    tempEntity->s.otherEntityNum = clientnum;
		    tempEntity->s.loopSound      = 0; // no sound on client
		    tempEntity->r.singleClient   = clientnum;
	        tempEntity->s.time           = level.clients[clientnum].ps.persistant[PERS_JUMPSTARTTIME];
	        tempEntity->r.svFlags       |= SVF_SINGLECLIENT;
	        Cmd_Score_f( ent ); // send all other stuff to spectator as well (otherwise colors of jump HUD are wrong)
        }

		return clientnum;
	} while (clientnum != original);

	/* No body to spectage, free float time. */
	StopFollowing(ent, qtrue);

	return -1;
}

#define MAX_MAPPING_STRING 256

/**
 * G_ChatGetVariableValue
 */
void G_ChatGetVariableValue(gentity_t *ent, const char *vartoken, char *varBuffer, int bufferSize) {

	if (ent->client->ghost || (ent->client->ps.pm_type >= PM_DEAD)) {
		Com_sprintf(varBuffer, bufferSize, "");
		return;
	}

	if (!Q_stricmp(vartoken, "health")) {

		if (ent->health < UT_MAX_HEALTH / 4) {
			Com_sprintf(varBuffer, bufferSize, "near death");
		} else if (ent->health < UT_MAX_HEALTH / 2) {
			Com_sprintf(varBuffer, bufferSize, "badly wounded");
		} else if (ent->health < UT_MAX_HEALTH * 3 / 4) {
			Com_sprintf(varBuffer, bufferSize, "wounded");
		} else {
			Com_sprintf(varBuffer, bufferSize, "healthy");
		}

	} else if (!Q_stricmp(vartoken, "clips")) {
		Com_sprintf(varBuffer, bufferSize, "%i", (int)utPSGetWeaponClips(&ent->client->ps));
	} else if (!Q_stricmp(vartoken, "bullets")) {
		Com_sprintf(varBuffer, bufferSize, "%i", (int)utPSGetWeaponBullets(&ent->client->ps));
	} else if (!Q_stricmp(vartoken, "weapon")) {
		Com_sprintf(varBuffer, bufferSize, "%s", bg_weaponlist[utPSGetWeaponID(&ent->client->ps)].name);
	} else if (!Q_stricmp(vartoken, "location")) {
		strcpy(varBuffer, "unknown");
		Team_GetLocationMsg(ent, varBuffer, bufferSize);
	} else if (!Q_stricmp(vartoken, "gameitem")) {

        switch (g_gametype.integer) {

            case GT_CTF:
			    strcpy(varBuffer, "flag");
			    break;

            case GT_UT_BOMB:
                strcpy(varBuffer, "bomb");
                break;

            default:

                if (rand() & 1) strcpy(varBuffer, "coinbird");
                else strcpy(varBuffer, "cat-bird");
			    break;

		}

	} else if (!Q_stricmp(vartoken, "gametime")) {

		int  time = (g_timelimit.integer * 60000) - (level.time - level.startTime + level.survivorStartTime);

		if (time == 0) {	  /* No timelimit then don't bother! */
			Com_sprintf(varBuffer, bufferSize, "no timelimit");
		} else if (time > 1000 * 60) {
			Com_sprintf(varBuffer, bufferSize, "%i minute%s", time / (1000 * 60), time / (1000 * 60) > 1 ? "s" : "");
		} else {

			if (time < 0) time = 0;
			Com_sprintf(varBuffer, bufferSize, "%i second%s", time / 1000, time / 1000 > 1 ? "s" : "");
		}

	} else if (!Q_stricmp(vartoken, "roundtime")) {

		int  time = (int)(g_RoundTime.value * 60000) - (level.time - level.startTime);

		if (time > 1000 * 60) {
			Com_sprintf(varBuffer, bufferSize, "%i minutes", time / (1000 * 60));
		} else {

			if (time < 0) time = 0;
			Com_sprintf(varBuffer, bufferSize, "%i seconds", time / 1000);
		}

	} else if (!Q_stricmp(vartoken, "leader")) {

		int  leader;
		strcpy(varBuffer, "unknown");
		leader = TeamLeader(ent->client->sess.sessionTeam);

		if (leader != -1) {
			Com_sprintf(varBuffer, bufferSize, "%s", g_entities[leader].client->pers.netname);
		}

	} else if (!Q_stricmp(vartoken, "momo")) {
		varBuffer[0] = '\0';

		if (ent->client->maparrow) {
			ent->client->maparrow->s.time	  = level.time + 3500;
			ent->client->maparrow->s.generic1 = 2;
		}

	} else if (!Q_stricmp(vartoken, "crosshair")) {

		gentity_t  tempent;
		trace_t    trace;
		vec3_t	   start, end, forward, right, up;

		AngleVectors(ent->client->ps.viewangles, forward, right, up);

		/* TODO: Check if ent->r.currentOrigin was really necessary here */
		/* CalcMuzzlePointOrigin(ent, ent->r.currentOrigin, forward, right, up, start); */
		CalcEyePoint(ent, start);

		end[0] = start[0] + (forward[0] * 30000);
		end[1] = start[1] + (forward[1] * 30000);
		end[2] = start[2] + (forward[2] * 30000);

		/* Check for mines and enable with FOG */
		trap_Trace(&trace, start, NULL, NULL, end, ent->client->ps.clientNum, CONTENTS_SOLID);

		if (trace.startsolid == qtrue) {
			strcpy(varBuffer, "unknown");
			Team_GetLocationMsg(ent, varBuffer, bufferSize);
		} else {
			tempent.r.currentOrigin[0] = trace.endpos[0];
			tempent.r.currentOrigin[1] = trace.endpos[1];
			tempent.r.currentOrigin[2] = trace.endpos[2];
			tempent.triggerLocation    = NULL;

			Team_GetLocationMsg(&tempent, varBuffer, bufferSize);

			if (!strcmp(varBuffer, "crosshair")) {
				strcpy(varBuffer, "unknown");
				Team_GetLocationMsg(ent, varBuffer, bufferSize);
			}
		}
	} else {
		varBuffer[0] = '\0';
	}
}

/**
 * G_ChatSubstitution
 */
void G_ChatSubstitution(gentity_t *ent, char *text)
{
	char	  tempBuffer[MAX_MAPPING_STRING];
	char	  varBuffer[MAX_MAPPING_STRING];
	char	  *token;
	char	  *vartoken  = NULL;
	qboolean  inVariable = qfalse;

	/* Make sure its not too big */
	if (strlen(text) > MAX_MAPPING_STRING) {
		G_Printf("CG_UTParseRadio: radio string > MAX_MAPPING_STRING\n");
		return;
	}

	/* Copy the text so we can work with it */
	strcpy(tempBuffer, text);

	/* Loop through the buffer so we can alter the return string. */
	for (token = tempBuffer; *token; token++) {
		switch (*token) {
		case 0x19:
			break;

		case '#':
		case '$':

			if (!inVariable) {
				inVariable = qtrue;
				vartoken   = varBuffer;
			} else {
				*text = *token;
				text++;
			}

			break;

		default:

			/* If we are parsing a variable then do so */
			if (inVariable) {
				if (((*token >= 'a') && (*token <= 'z')) ||
						((*token >= 'A') && (*token <= 'Z'))) {
					*vartoken = *token;
					vartoken++;
				} else {
					inVariable = qfalse;
					*vartoken  = 0;

					G_ChatGetVariableValue(ent, varBuffer, varBuffer, MAX_MAPPING_STRING);

					if (varBuffer[0]) {
						*text = 0;
						strcat(text, varBuffer);
						text += strlen(varBuffer);
					}
				}
			}

			if (!inVariable) {
				*text = *token;
				text++;
			}

			break;
		}
	}

	*text = 0;

	if (inVariable && vartoken) {
		*vartoken = 0;
		G_ChatGetVariableValue(ent, varBuffer, varBuffer, MAX_MAPPING_STRING);
		strcat(text, varBuffer);
	}
}

/**
 * G_SayTo
 */
static void G_SayTo(gentity_t *ent, gentity_t *other, int mode, int color, const char *name, const char *message)
{
	int	  prefix;
	char	  *s;
	qboolean  fromsub  = qfalse;
	qboolean  fromdead = qfalse;
	qboolean  fromspec = qfalse;
	qboolean  todead   = qfalse;
	qboolean  tospec   = qfalse;

	if (ent->client->sess.muteTimer > level.time) {
		return;
	}

	if (!g_allowChat.integer || ((mode == SAY_ALL) &&
		(g_allowChat.integer < 2))) {
		return;
	}

	if (!other || !other->inuse || !other->client) {
		return;
	}

	//@Barbatos - if the client is ignored by the "other" one
	if (other->client->pers.ignoredClients[ent->client->ps.clientNum]) {
		return;
	}

	//@Barbatos: allow only the captains to talk during match with g_allowChat 3
	//It occurs only if matchmode is ON and both teams are ready
	if((g_allowChat.integer == 3) && (mode == SAY_ALL) && (level.matchState & UT_MATCH_ON) &&
		(level.matchState & UT_MATCH_RR) && (level.matchState & UT_MATCH_BR) && (!other->client->pers.captain))
		return;

	if (other->client->pers.connected != CON_CONNECTED) {
		return;
	}

	if ((mode == SAY_TEAM) && (!OnSameTeam(ent, other) && (ent != other))) { /* added && ent != other so radio can work. it takes care of the rest. */
		return;
	}

	if (!level.intermissiontime) {
		fromsub  = (ent->client->pers.substitute == qtrue);
		fromspec = (ent->client->sess.sessionTeam == TEAM_SPECTATOR);
		fromdead = (ent->client->ps.pm_type == PM_DEAD || ent->client->ghost);

		tospec	 = (other->client->sess.sessionTeam >= TEAM_SPECTATOR);
		todead	 = (other->client->ps.pm_type == PM_DEAD || other->client->ghost);

		/* IF the the guy speaking is dead or spec and the target isnt either then */

		/* no talkie talkie. */
		if ((!level.survivorRoundRestartTime)) {
			if (g_deadchat.integer == 1) {

				/*if from spectator, to a non spectator, cancel it */
				if (((fromspec) && !(tospec)) && (g_gametype.integer != GT_JUMP)) {
					return;
				}

				/*if from spectator, dead, sub to the living, and not sayteam, cancel */
				if (((fromsub || fromspec || fromdead) && !(tospec || todead)) && !(mode == SAY_TEAM) && (g_gametype.integer != GT_JUMP)) {
					return;
				}

			} else if (g_deadchat.integer == 2) {
				if (((fromspec) && !(tospec)) && (g_gametype.integer != GT_JUMP)) {
					return;
				}
			} else {
				if (((fromsub || fromspec || fromdead) && !(tospec || todead)) && (g_gametype.integer != GT_JUMP)) {
					return;
				}
			}
		}
	}

	/* Find the name color */
	if (mode == SAY_NPC) {
		prefix = TEAM_SPECTATOR;
	} else if (ent->client->sess.sessionTeam == TEAM_BLUE) {
		prefix = TEAM_BLUE;
	} else if (ent->client->sess.sessionTeam == TEAM_RED) {
		prefix = TEAM_RED;
	} else {
		prefix = TEAM_FREE;
	}

	/* Send the say */
	s = va("%s \"%d\" \"%s%s%s%c%c%s\"",
		   ((mode == SAY_TEAM) || (mode == SAY_NPC)) ? "tcchat" : "cchat",
		   prefix,
		   fromsub ? "(SUB) " : (fromdead ? "(DEAD) " : ""),
		   fromspec ? "(SPEC) " : "",
		   name, Q_COLOR_ESCAPE, color, message);

	trap_SendServerCommand(other - g_entities, s);
}

#define EC "\x19"

/**
 * G_Say
 */
void G_Say(gentity_t *ent, gentity_t *target, int mode, const char *chatText)
{
	int 		j;
	gentity_t  *other;
	int 	   color;
	char	   name[64];
	/* don't let text be too long for malicious reasons */
	char	   text[MAX_SAY_TEXT];
	char	   location[64];
	/* DensitY - strip colors for normal world chat only, allow colors for team chat */
	qboolean   stripcolor = qfalse;

	if (((g_gametype.integer < GT_TEAM) || (g_gametype.integer == GT_JUMP)) && (mode == SAY_TEAM)) {
		mode = SAY_ALL;  /*GottaBeKD disable for testing purposes */
	}

	switch (mode) {
	default:
	case SAY_ALL:
		G_LogPrintf("say: %d %s: %s\n", ent->s.number, ent->client->pers.netname, chatText);
		Com_sprintf(name, sizeof(name), "%s%c%c" EC ": ", ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_YELLOW);
		color	   = COLOR_YELLOW;
		stripcolor = qtrue;
		break;

	case SAY_TEAM:
		G_LogPrintf("sayteam: %d %s: %s\n", ent->s.number, ent->client->pers.netname, chatText);

		if (Team_GetLocationMsg(ent, location, sizeof(location))) {
			Com_sprintf(name, sizeof(name), "%s" S_COLOR_WHITE " (%s): ",
					ent->client->pers.netname, location);
		} else {
			Com_sprintf(name, sizeof(name), "%s%c%c" EC "" EC ": ",
					ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_YELLOW);
		}

		color	   = COLOR_YELLOW;
		stripcolor = qfalse;
		break;

	case SAY_TELL:

		G_LogPrintf("saytell: %d %d %s: %s\n", ent->s.number, target->s.number, ent->client->pers.netname, chatText);

		if (target && (g_gametype.integer >= GT_TEAM) && (g_gametype.integer != GT_JUMP) &&
				(target->client->sess.sessionTeam == ent->client->sess.sessionTeam) &&
				Team_GetLocationMsg(ent, location, sizeof(location))) {
			Com_sprintf(name, sizeof(name), EC "[%s%c%c" EC "] (%s)" EC ": ", ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE, location);
		} else {
			Com_sprintf(name, sizeof(name), EC "[%s%c%c" EC "]" EC ": ", ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE);
		}

		color	   = COLOR_YELLOW;
		stripcolor = qfalse;
		break;
	}

	Q_strncpyz(text, chatText, sizeof(text));

	G_ChatSubstitution(ent, text);	/* GottaBeKD for $health$, $ammo$, $weapon$ mappings */

	if (target) {
		if (stripcolor == qtrue) {
			G_SayTo(ent, target, mode, color, name, Q_CleanStr(text));
		} else {
			G_SayTo(ent, target, mode, color, name, text);
		}

		return;
	}

	/* echo the text to the console */
	if (g_dedicated.integer) {
		if (ent->client->sess.muteTimer > level.time) {
			G_Printf("%s(MUTED)%s\n", name, text);
		} else {
			G_Printf("%s%s\n", name, text);
		}
	}

	/* send it to all the apropriate clients */
	for (j = 0; j < level.maxclients; j++) {
		other = &g_entities[j];

		if (stripcolor == qtrue) {
			G_SayTo(ent, other, mode, color, name, Q_CleanStr(text));
		} else {
			G_SayTo(ent, other, mode, color, name, text);
		}
	}
}

/**
 * Cmd_Say_f
 */
static void Cmd_Say_f(gentity_t *ent, int mode, qboolean arg0)
{
	char  *p;

	if ((trap_Argc() < 2) && !arg0) {
		return;
	}

	if (arg0) {
		p = ConcatArgs(0);
	} else {
		p = ConcatArgs(1);
	}

	G_Say(ent, NULL, mode, p);
}

/**
 * Cmd_Tell_f
 */
static void Cmd_Tell_f(gentity_t *ent)
{
	int    targetNum;
	gentity_t  *target;
	char	   *p;
	char	   arg[MAX_TOKEN_CHARS];

	if (trap_Argc() < 2) {
		return;
	}

	trap_Argv(1, arg, sizeof(arg));
	targetNum = atoi(arg);

	if ((targetNum < 0) || (targetNum >= level.maxclients)) {
		return;
	}

	target = &g_entities[targetNum];

	if (!target || !target->inuse || !target->client) {
		return;
	}

	p = ConcatArgs(2);

	G_LogPrintf("tell: %s to %s: %s\n", ent->client->pers.netname, target->client->pers.netname, p);
	G_Say(ent, target, SAY_TELL, p);

	/* don't tell to the player self if it was already directed to this player */

	/* also don't send the chat back to a bot */
	if ((ent != target) && !(ent->r.svFlags & SVF_BOT)) {
		G_Say(ent, ent, SAY_TELL, p);
	}
}

/**
 * G_VoiceTo
 */
static void G_VoiceTo(gentity_t *ent, gentity_t *other, int mode, const char *id, qboolean voiceonly)
{
	int   color;
	char  *cmd;

	if (!other) {
		return;
	}

	if (!other->inuse) {
		return;
	}

	if (!other->client) {
		return;
	}

	if ((mode == SAY_TEAM) && !OnSameTeam(ent, other)) {
		return;
	}

	color = COLOR_GREEN;
	cmd   = "vchat";

	trap_SendServerCommand(other - g_entities, va("%s %d %d %d %s", cmd, voiceonly, ent->s.number, color, id));
}

/**
 * G_Voice
 */
void G_Voice(gentity_t *ent, gentity_t *target, int mode, const char *id, qboolean voiceonly)
{
	int    j;
	gentity_t  *other;

	if (((g_gametype.integer < GT_TEAM) || (g_gametype.integer == GT_JUMP)) && (mode == SAY_TEAM)) {
		mode = SAY_ALL;
	}

	if (target) {
		G_VoiceTo(ent, target, mode, id, voiceonly);
		return;
	}

	/* echo the text to the console */
	if (g_dedicated.integer) {
		G_Printf("voice: %s %s\n", ent->client->pers.netname, id);
	}

	/* send it to all the apropriate clients */
	for (j = 0; j < level.maxclients; j++) {
		other = &g_entities[j];
		G_VoiceTo(ent, other, mode, id, voiceonly);
	}
}

/**
 * Cmd_Voice_f
 */
static void Cmd_Voice_f(gentity_t *ent, int mode, qboolean arg0, qboolean voiceonly)
{
	char  *p;

	if ((trap_Argc() < 2) && !arg0) {
		return;
	}

	if (arg0) {
		p = ConcatArgs(0);
	} else {
		p = ConcatArgs(1);
	}

	G_Voice(ent, NULL, mode, p, voiceonly);
}

/**
 * Cmd_VoiceTell_f
 */
static void Cmd_VoiceTell_f(gentity_t *ent, qboolean voiceonly)
{
	int 	   targetNum;
	gentity_t  *target;
	char	   *id;
	char	   arg[MAX_TOKEN_CHARS];

	if (trap_Argc() < 2) {
		return;
	}

	trap_Argv(1, arg, sizeof(arg));
	targetNum = atoi(arg);

	if ((targetNum < 0) || (targetNum >= level.maxclients)) {
		return;
	}

	target = &g_entities[targetNum];

	if (!target || !target->inuse || !target->client) {
		return;
	}

	id = ConcatArgs(2);

	G_LogPrintf("vtell: %s to %s: %s\n", ent->client->pers.netname, target->client->pers.netname, id);
	G_Voice(ent, target, SAY_TELL, id, voiceonly);

	/* don't tell to the player self if it was already directed to this player */

	/* also don't send the chat back to a bot */
	if ((ent != target) && !(ent->r.svFlags & SVF_BOT)) {
		G_Voice(ent, ent, SAY_TELL, id, voiceonly);
	}
}

/**
 * Cmd_VoiceTaunt_f
 */
static void Cmd_VoiceTaunt_f(gentity_t *ent)
{
	gentity_t  *who;
	int    i;

	if (!ent->client) {
		return;
	}

	/* insult someone who just killed you */
	if (ent->enemy && ent->enemy->client && (ent->enemy->client->lastkilled_client == ent->s.number)) {
		/* i am a dead corpse */
		if (!(ent->enemy->r.svFlags & SVF_BOT)) {
			G_Voice(ent, ent->enemy, SAY_TELL, VOICECHAT_DEATHINSULT, qfalse);
		}

		if (!(ent->r.svFlags & SVF_BOT)) {
			G_Voice(ent, ent, SAY_TELL, VOICECHAT_DEATHINSULT, qfalse);
		}

		ent->enemy = NULL;
		return;
	}

	/* insult someone you just killed */
	if ((ent->client->lastkilled_client >= 0) && (ent->client->lastkilled_client != ent->s.number)) {
		who = g_entities + ent->client->lastkilled_client;

		if (who->client) {
			/* who is the person I just killed */
			if (!(who->r.svFlags & SVF_BOT)) {
				G_Voice(ent, who, SAY_TELL, VOICECHAT_KILLINSULT, qfalse);	/* and I killed them with something else */
			}

			if (!(ent->r.svFlags & SVF_BOT)) {
				G_Voice(ent, ent, SAY_TELL, VOICECHAT_KILLINSULT, qfalse);
			}

			ent->client->lastkilled_client = -1;
			return;
		}
	}

	if ((g_gametype.integer >= GT_TEAM) && (g_gametype.integer != GT_JUMP)){
		/* praise a team mate who just got a reward */
		for (i = 0; i < MAX_CLIENTS; i++) {
			who = g_entities + i;

			if (who->client && (who != ent) && (who->client->sess.sessionTeam == ent->client->sess.sessionTeam)) {
				if (who->client->rewardTime > level.time) {
					if (!(who->r.svFlags & SVF_BOT)) {
						G_Voice(ent, who, SAY_TELL, VOICECHAT_PRAISE, qfalse);
					}

					if (!(ent->r.svFlags & SVF_BOT)) {
						G_Voice(ent, ent, SAY_TELL, VOICECHAT_PRAISE, qfalse);
					}

					return;
				}
			}
		}
	}

	/* just say something */
	G_Voice(ent, NULL, SAY_ALL, VOICECHAT_TAUNT, qfalse);
}

static char  *gc_orders[] = {
	"hold your position",
	"hold this position",
	"come here",
	"cover me",
	"guard location",
	"search and destroy",
	"report"
};

/**
 * Cmd_GameCommand_f
 */
void Cmd_GameCommand_f(gentity_t *ent)
{
	int   player;
	int   order;
	char  str[MAX_TOKEN_CHARS];

	trap_Argv(1, str, sizeof(str));
	player = atoi(str);
	trap_Argv(2, str, sizeof(str));
	order  = atoi(str);

	if ((player < 0) || (player >= MAX_CLIENTS)) {
		return;
	}

	if ((order < 0) || (order > sizeof(gc_orders) / sizeof(char *))) {
		return;
	}

	G_Say(ent, &g_entities[player], SAY_TELL, gc_orders[order]);
	G_Say(ent, ent, SAY_TELL, gc_orders[order]);
}

/**
 * Cmd_Where_f
 */
void Cmd_Where_f(gentity_t *ent)
{
	trap_SendServerCommand(ent - g_entities, va("print \"%s\n\"", vtos(ent->s.origin)));
}

static const char  *gameNames[] = {
	"Free For All",
	"Last Man Standing",
	"",
	"Team Deathmatch",
	"Team Survivor",
	"Follow the Leader",
	"Capture and Hold",
	"Capture the Flag",
	"Bomb Mode",
	"Jump Training",
};


/**
 * Cmd_CallVote_f
 */
void Cmd_CallVote_f(gentity_t *ent)
{
	gclient_t  *other;
	int 	   allowedVotes;
	qboolean   allowed = qfalse;
	char	   arg1[MAX_STRING_TOKENS];
	char	   arg2[MAX_STRING_TOKENS];
	char	   arg3[MAX_STRING_TOKENS];
	char	   *searchmap;
	char	   *suffix;
	int 	   i;
	int 	   waitTime;
	char	   nextmap[MAX_STRING_CHARS];

	/* Get allowed votes. */
	allowedVotes = g_allowVote.integer;

	/* If voting is not allowed or number out of range. */
	if (allowedVotes < 1) {
		trap_SendServerCommand(ent - g_entities, "print \"Voting not allowed here.\n\"");
		return;
	}

	if (level.time < g_newMapVoteTime.integer * 1000) {
		trap_SendServerCommand(ent - g_entities, "print \"Calling a vote must wait 30 seconds into a new map.\n\"");
		return;
	}

	//@Barbatos - disallow voting for spectators
	if(ent->client->sess.sessionTeam == TEAM_SPECTATOR) {
		trap_SendServerCommand(ent - g_entities, "print \"Calling a vote is not allowed for spectators.\n\"");
		return;
	}

	/* Get arguments. */
	trap_Argv(1, arg1, sizeof(arg1));
	trap_Argv(2, arg2, sizeof(arg2));
	trap_Argv(3, arg3, sizeof(arg3));

	/* If arguments not valid. */
	if (strspn(arg1, ";^\r\n") || strspn(arg2, ";^\r\n") || strspn(arg3, ";^\r\n")) {
		trap_SendServerCommand(ent - g_entities, "print \"Invalid vote string.\n\"");
		return;
	}

	/* If there's a vote in progress, */
	if (level.voteTime) {
		trap_SendServerCommand(ent - g_entities, "print \"A vote is already in progress.\n\"");
		return;
	}

	/* If the dude called it one time too many. */
	if (ent->client->pers.voteCount >= MAX_VOTE_COUNT) {
		trap_SendServerCommand(ent - g_entities, "print \"You have called the maximum number of votes.\n\"");
		return;
	}

	if (trap_Milliseconds() - ent->client->sess.connectTime < 30000) {
		trap_SendServerCommand(ent - g_entities, "print \"Not allowed to call a vote until you've been connected for 30 seconds.\n\"");
		return;
	}

	/* If we need to wait before calling a vote. */
	if (ent->client->sess.lastFailedVote && ((ent->client->sess.lastFailedVote + (g_failedVoteTime.integer * 1000)) > level.time)) {
		//@Fenix - tell the player how much time he needs to wait before to call another vote.
		waitTime = (int)(((ent->client->sess.lastFailedVote + (g_failedVoteTime.integer * 1000)) - level.time) / 1000);
		if (waitTime < 60) { suffix = (waitTime == 1) ? "second" : "seconds"; }
		else { suffix = ((waitTime = (int)ceil(waitTime/60)) == 1) ? "minute" : "minutes"; }
		trap_SendServerCommand(ent - g_entities, va("print \"You need to wait %d %s before to call another vote.\n\"", waitTime, suffix));
		return;
	}

	/*
	1 reload
	2 restart
	3 map
	4 nextmap
	5 kick
	  clientkick

	6 swapteams
	7 shuffleteams
	8 g_friendlyFire
	9 g_followStrict

	10 g_gametype
	11 g_waveRespawns

	12 timelimit
	13 fraglimit
	14 capturelimit

	15 g_respawnDelay
	16 g_redWaveRespawnDelay
	17 g_blueWaveRespawnDelay
	18 g_bombExplodeTime
	19 g_bombDefuseTime
	20 g_RoundTime
	21 g_captureScoreTime
	22 g_warmup

	23 g_matchMode
	24 g_timeouts
	25 g_timeoutLength
	26 exec

	27 g_swaproles
	28 g_maxrounds
	29 g_gear
	30 cyclemap
	*/

	if (!Q_stricmp(arg1, "reload")) {
		if (allowedVotes & (1 << 0)) {
			allowed = qtrue;
		}
	} else if (!Q_stricmp(arg1, "restart")) {
		if (allowedVotes & (1 << 1)) {
			allowed = qtrue;
		}
	} else if (!Q_stricmp(arg1, "map")) {
		if (allowedVotes & (1 << 2)) {
			allowed = qtrue;
		}
	} else if (!Q_stricmp(arg1, "nextmap")) {
		if (allowedVotes & (1 << 3)) {
			allowed = qtrue;
		}
	} else if (!Q_stricmp(arg1, "kick")) {
		if (allowedVotes & (1 << 4)) {
			allowed = qtrue;
		}
	} else if (!Q_stricmp(arg1, "clientkick")) {
		if (allowedVotes & (1 << 4)) {
			allowed = qtrue;
		}
	} else if (!Q_stricmp(arg1, "swapteams")) {
		if (allowedVotes & (1 << 5)) {
			allowed = qtrue;
		}
	} else if (!Q_stricmp(arg1, "shuffleteams")) {
		if (allowedVotes & (1 << 6)) {
			allowed = qtrue;
		}
	} else if (!Q_stricmp(arg1, "g_friendlyFire")) {
		if (allowedVotes & (1 << 7)) {
			allowed = qtrue;
		}
	} else if (!Q_stricmp(arg1, "g_followStrict")) {
		if (allowedVotes & (1 << 8)) {
			allowed = qtrue;
		}
	} else if (!Q_stricmp(arg1, "g_gametype")) {
		if (allowedVotes & (1 << 9)) {
			allowed = qtrue;
		}
	} else if (!Q_stricmp(arg1, "g_waveRespawns")) {
		if (allowedVotes & (1 << 10)) {
			allowed = qtrue;
		}
	} else if (!Q_stricmp(arg1, "timelimit")) {
		if (allowedVotes & (1 << 11)) {
			allowed = qtrue;
		}
	} else if (!Q_stricmp(arg1, "fraglimit")) {
		if (allowedVotes & (1 << 12)) {
			allowed = qtrue;
		}
	} else if (!Q_stricmp(arg1, "capturelimit")) {
		if (allowedVotes & (1 << 13)) {
			allowed = qtrue;
		}
	} else if (!Q_stricmp(arg1, "g_respawnDelay")) {
		if (allowedVotes & (1 << 14)) {
			allowed = qtrue;
		}
	} else if (!Q_stricmp(arg1, "g_redWave")) {
		if (allowedVotes & (1 << 15)) {
			allowed = qtrue;
		}
	} else if (!Q_stricmp(arg1, "g_blueWave")) {
		if (allowedVotes & (1 << 16)) {
			allowed = qtrue;
		}
	} else if (!Q_stricmp(arg1, "g_bombExplodeTime")) {
		if (allowedVotes & (1 << 17)) {
			allowed = qtrue;
		}
	} else if (!Q_stricmp(arg1, "g_bombDefuseTime")) {
		if (allowedVotes & (1 << 18)) {
			allowed = qtrue;
		}
	} else if (!Q_stricmp(arg1, "g_RoundTime")) {
		if (allowedVotes & (1 << 19)) {
			allowed = qtrue;
		}
	} else if (!Q_stricmp(arg1, "g_cahTime")) {
		if (allowedVotes & (1 << 20)) {
			allowed = qtrue;
		}
	} else if (!Q_stricmp(arg1, "g_warmup")) {
		if (allowedVotes & (1 << 21)) {
			allowed = qtrue;
		}
	} else if (!Q_stricmp(arg1, "g_matchMode")) {
		if (allowedVotes & (1 << 22)) {
			allowed = qtrue;
		}
	} else if (!Q_stricmp(arg1, "g_timeouts")) {
		if (allowedVotes & (1 << 23)) {
			allowed = qtrue;
		}
	} else if (!Q_stricmp(arg1, "g_timeoutLength")) {
		if (allowedVotes & (1 << 24)) {
			allowed = qtrue;
		}
	} else if (!Q_stricmp(arg1, "exec")) {
		if (allowedVotes & (1 << 25)) {
			allowed = qtrue;
		}
	} else if (!Q_stricmp(arg1, "g_swaproles")) {
		if (allowedVotes & (1 << 26)) {
			allowed = qtrue;
		}
	} else if (!Q_stricmp(arg1, "g_maxrounds")) {
		if (allowedVotes & (1 << 27)) {
			allowed = qtrue;
		}
	} else if (!Q_stricmp(arg1, "g_gear")) {
		if (allowedVotes & (1 << 28)) {
			allowed = qtrue;
		}
	} else if (!Q_stricmp(arg1, "cyclemap")) {
		if (allowedVotes & (1 << 29)) {
			allowed = qtrue;
		}
	} else {
		trap_SendServerCommand(ent - g_entities, "print \"Invalid vote string.\n\"");
		trap_SendServerCommand(ent - g_entities, "print \"Vote commands are: reload restart map nextmap g_gametype kick timelimit fraglimit capturelimit g_warmup g_friendlyFire swapteams shuffleteams g_respawnDelay g_waveRespawns g_redWave g_blueWave g_bombExplodeTime g_bombDefuseTime g_matchMode g_timeouts g_timeoutLength g_followStrict g_RoundTime g_cahTime exec g_gear g_maxrounds g_swaproles\n\"");

		return;
	}

	/* If this vote is not allowed currently. */
	if (!allowed) {
		trap_SendServerCommand(ent - g_entities, "print \"This vote is not allowed on this server.\n\"");
		return;
	}

	/* If there is still a vote to be executed. */
	if (level.voteExecuteTime) {
		level.voteExecuteTime = 0;
		trap_SendConsoleCommand(EXEC_APPEND, va("%s\n", level.voteString));
	}

	level.voteClientArg = -1;

	/* Handle vote types. */
	if (!Q_stricmp(arg1, "g_gametype")) {

		i = atoi(arg2);
		if ((i < GT_FFA) || (i >= GT_MAX_GAME_TYPE)) {
			trap_SendServerCommand(ent - g_entities, "print \"Invalid gametype.\n\"");
			return;
		}

		Com_sprintf(level.voteString, sizeof(level.voteString), "%s %d; reload", arg1, i);
		Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "%s %s", arg1, gameNames[i]);

	} else if (!Q_stricmp(arg1, "g_matchMode")) {
		Com_sprintf(level.voteString, sizeof(level.voteString), "%s \"%s\"; reload", arg1, arg2);
		Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "%s", level.voteString);
	} else if (!Q_stricmp(arg1, "reload")) {
		Com_sprintf(level.voteString, sizeof(level.voteString), "reload");
		Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "%s", level.voteString);
	} else if (!Q_stricmp(arg1, "restart")) {
		Com_sprintf(level.voteString, sizeof(level.voteString), "restart");
		Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "%s", level.voteString);
	} else if (!Q_stricmp(arg1, "map")) {

		//Fenix: checking whether the map name
		//has been specified in the vote string
		if (trap_Argc() < 3)
		    return;

		// Fenix: new function!
		// Handle partial map name matching
		searchmap = GetMapSoundingLike(ent, arg2);
		if (!searchmap)
		    return;

		Com_sprintf(level.voteString, sizeof(level.voteString), "%s %s", arg1, searchmap);
		Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "%s", level.voteString);

	} else if (!Q_stricmp(arg1, "nextmap")) {

	    //Fenix: checking whether the map name
        //has been specified in the vote string
        if (trap_Argc() < 3)
            return;

		// Fenix: new function!
        // Handle partial map name matching
        searchmap = GetMapSoundingLike(ent, arg2);
        if (!searchmap)
            return;

		Com_sprintf(level.voteString, sizeof(level.voteString), "g_nextmap %s", searchmap);

		if (g_NextMap.string[0]) {

		    Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "%s (current: %s)", level.voteString, g_NextMap.string);

		} else {

		    nextmap[0] = '\0';
            trap_Cvar_VariableStringBuffer("g_nextCycleMap", nextmap, sizeof(nextmap));

            if (nextmap[0]){
                Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "%s (current: %s)", level.voteString, nextmap);
            } else {
                Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "%s", level.voteString);
            }
		}

	} else if (!Q_stricmp(arg1, "cyclemap")) {
		Com_sprintf(level.voteString, sizeof(level.voteString), "cyclemap");
		Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "%s", level.voteString);
	} else if (!Q_stricmp(arg1, "swapteams")) {
		Com_sprintf(level.voteString, sizeof(level.voteString), "swapteams");
		Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "%s", level.voteString);
	} else if (!Q_stricmp(arg1, "g_gear")) {
		Com_sprintf(level.voteString, sizeof(level.voteString), "g_gear %s", arg2);
		Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "g_gear %s ( %s )", arg2, allowedGear(atoi(arg2)));
	} else if (!Q_stricmp(arg1, "shuffleteams")) {
		Com_sprintf(level.voteString, sizeof(level.voteString), "shuffleteams");
		Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "%s", level.voteString);
	} else if (!Q_stricmp(arg1, "kick") || !Q_stricmp(arg1, "clientkick") ) {

		//@Fenix - checking whether a player handle
		//has been specified in the vote string.
		if (trap_Argc() < 3) { return; }

		other = ClientForString(arg2);
		if (!other) {
			//@Fenix - bad player handle specified.
			//We do not have to print anything here, since
			//ClientForString() already does the job.
			return;
		}

		//@Fenix - level.voteString variable MUST contain the player slot as a handle and not the full name.
		//Changing this behavior will result in a failure while calling Svcmd_ClientKickReason_f in g_svcmds.c
		//because the target player may change his nickname meanwhile the vote is in progress.
		Com_sprintf(arg1, sizeof(arg1), "clientkickreason");
		Com_sprintf(arg2, sizeof(arg2), "%d", other->ps.clientNum);
		level.voteClientArg = other->ps.clientNum;

		//@Fenix - wow this looks horrible! In order not to display the player slot but the full player name
		//in the votation string printed as a server message, we need to print a "custom" message. FIXME
		if (trap_Argc() < 4) {
			// no reason specified
			Com_sprintf(level.voteString, sizeof(level.voteString), "%s \"%s\"", arg1, arg2);
			Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "Kick %s", other->pers.netname );
			trap_SendServerCommand(-1, va("print \"%s called a vote for kick %s\n\"", ent->client->pers.netname, other->pers.netname));
		}
		else {
			// reason specified
			char *arg3_concat = ConcatArgs(3);
			Com_sprintf(level.voteString, sizeof(level.voteString), "%s \"%s\" \"%s\"", arg1, arg2, arg3_concat);
			Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "Kick %s, reason: %s", other->pers.netname, arg3_concat);
			trap_SendServerCommand(-1, va("print \"%s called a vote for kick %s, reason: %s\n\"", ent->client->pers.netname, other->pers.netname, arg3_concat));
		}

	} else if (!Q_stricmp(arg1, "exec")) {
		Com_sprintf(level.voteString, sizeof(level.voteString), "%s %s", arg1, arg2);
		Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "%s", level.voteString);
	} else {
		Com_sprintf(level.voteString, sizeof(level.voteString), "%s \"%s\"", arg1, arg2);
		Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "%s", level.voteString);
	}

	//@Fenix - we need to exclude the clientkickreason callvote since we already displayed a tweaked message
	if (Q_stricmp(arg1, "clientkickreason") != 0)
		trap_SendServerCommand(-1, va("print \"%s called a vote for %s\n\"", ent->client->pers.netname, level.voteString));

	//@Barbatos
	G_LogPrintf("Callvote: %i - \"%s\"\n", ent->client->ps.clientNum, level.voteString);

	level.voteTime = level.time;
	level.voteYes  = 1;
	level.voteNo   = 0;

	// @Fenix - correctly reset clients voted flag.
	for (i=0 ; i<level.maxclients; i++) {
		other = &level.clients[i];
		other->voted = qfalse;
	}

	//Start the voting, the caller automatically votes yes.
	ent->client->voted		 = qtrue;
	ent->client->sess.lastFailedVote = 0;
	level.voteClient		 = ent - g_entities;

	trap_SetConfigstring(CS_VOTE_TIME, va("%i", level.voteTime));
	trap_SetConfigstring(CS_VOTE_STRING, level.voteDisplayString);
	trap_SetConfigstring(CS_VOTE_YES, va("%i", level.voteYes));
	trap_SetConfigstring(CS_VOTE_NO, va("%i", level.voteNo));
}

/**
 * Cmd_Vote_f
 */
void Cmd_Vote_f(gentity_t *ent)
{
	char msg[64];

	/* If there's no vote in progress. */
	if (!level.voteTime) {
		trap_SendServerCommand(ent - g_entities, "print \"No vote in progress.\n\"");
		return;
	}

	/* If the dude already voted. */
	if (ent->client->voted) {
		trap_SendServerCommand(ent - g_entities, "print \"Vote already cast.\n\"");
		return;
	}

	//@Barbatos - disallow voting for spectators
	if(ent->client->sess.sessionTeam == TEAM_SPECTATOR) {
		trap_SendServerCommand(ent - g_entities, "print \"Voting is not allowed for spectators.\n\"");
		return;
	}

	//@Barbatos: bugfix #183. "Allowed Voting - Load up Time Dependency"
	/*if (level.time - ent->client->pers.enterTime < 30000) {
		trap_SendServerCommand(ent - g_entities, "print \"Not allowed to vote until you've been connected for 30 seconds.\n\"");
		return;
	}*/

	/* Notify that we voted. */
	trap_SendServerCommand(ent - g_entities, "print \"Vote cast.\n\"");

	/* Set voted flag. */
	ent->client->voted = qtrue;

	/* Get vote message. */
	trap_Argv(1, msg, sizeof(msg));

	/* Determine vote type. @Barbatos */
	if (tolower(msg[0]) == 'y' || msg[0] == '1') {

		level.voteYes++;
		trap_SetConfigstring(CS_VOTE_YES, va("%i", level.voteYes));
		G_LogPrintf("Vote: %i - 1\n", ent->client->ps.clientNum);

	} else {

		level.voteNo++;
		trap_SetConfigstring(CS_VOTE_NO, va("%i", level.voteNo));
		G_LogPrintf("Vote: %i - 2\n", ent->client->ps.clientNum);

	}
}

/**
 * Cmd_CallTeamVote_f
 */
void Cmd_CallTeamVote_f(gentity_t *ent)
{
	int   i, team, cs_offset;
	char  arg1[MAX_STRING_TOKENS];
	char  arg2[MAX_STRING_TOKENS];

	team = ent->client->sess.sessionTeam;

	if (team == TEAM_RED) {
		cs_offset = 0;
	} else if (team == TEAM_BLUE) {
		cs_offset = 1;
	} else {
		return;
	}

	if (!g_allowVote.integer) {
		trap_SendServerCommand(ent - g_entities, "print \"Voting not allowed here.\n\"");
		return;
	}

	if (level.teamVoteTime[cs_offset]) {
		trap_SendServerCommand(ent - g_entities, "print \"A team vote is already in progress.\n\"");
		return;
	}

	if (ent->client->pers.teamVoteCount >= MAX_VOTE_COUNT) {
		trap_SendServerCommand(ent - g_entities, "print \"You have called the maximum number of team votes.\n\"");
		return;
	}

	if (level.time - ent->client->pers.enterTime < 30000) {
		trap_SendServerCommand(ent - g_entities, "print \"Not allowed to vote until you've been connected for 30 seconds.\n\"");
		return;
	}

	/* make sure it is a valid command to vote on */
	trap_Argv(1, arg1, sizeof(arg1));
	arg2[0] = '\0';

	for (i = 2; i < trap_Argc(); i++) {
		if (i > 2) {
			strcat(arg2, " ");
		}

		trap_Argv(i, &arg2[strlen(arg2)], sizeof(arg2) - strlen(arg2));
	}

	if (strchr(arg1, ';') || strchr(arg2, ';')) {
		trap_SendServerCommand(ent - g_entities, "print \"Invalid vote string.\n\"");
		return;
	}

	trap_SendServerCommand(ent - g_entities, "print \"Invalid vote string.\n\"");
	trap_SendServerCommand(ent - g_entities, "print \"Team vote commands are: leader <player>.\n\"");
}

/**
 * Cmd_TeamVote_f
 */
void Cmd_TeamVote_f(gentity_t *ent) {

	int   team, cs_offset;
	char  msg[64];

	team = ent->client->sess.sessionTeam;

	if (team == TEAM_RED) {
		cs_offset = 0;
	} else if (team == TEAM_BLUE) {
		cs_offset = 1;
	} else {
		return;
	}

	if (!level.teamVoteTime[cs_offset]) {
		trap_SendServerCommand(ent - g_entities, "print \"No team vote in progress.\n\"");
		return;
	}

	if (ent->client->ps.eFlags & EF_TEAMVOTED) {
		trap_SendServerCommand(ent - g_entities, "print \"Team vote already cast.\n\"");
		return;
	}

	//@Fenix - do not allow teamvote if spectator
	if (ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
		trap_SendServerCommand( ent-g_entities, "print \"Not allowed to vote as spectator.\n\"" );
		return;
	}

	if (level.time - ent->client->pers.enterTime < 30000) {
		trap_SendServerCommand(ent - g_entities, "print \"Not allowed to vote until you've been connected for 30 seconds.\n\"");
		return;
	}

	trap_SendServerCommand(ent - g_entities, "print \"Team vote cast.\n\"");

	ent->client->ps.eFlags |= EF_TEAMVOTED;

	trap_Argv(1, msg, sizeof(msg));

	if (tolower(msg[0]) == 'y' || msg[0] == '1') {

		level.teamVoteYes[cs_offset]++;
		trap_SetConfigstring(CS_TEAMVOTE_YES + cs_offset, va("%i", level.teamVoteYes[cs_offset]));

	} else {

		level.teamVoteNo[cs_offset]++;
		trap_SetConfigstring(CS_TEAMVOTE_NO + cs_offset, va("%i", level.teamVoteNo[cs_offset]));

	}

	/* a majority will be determined in TeamCheckVote, which will also account */
	/* for players entering or leaving */
}

/**
 * Cmd_SetViewpos_f
 */
void Cmd_SetViewpos_f(gentity_t *ent)
{
	vec3_t	origin, angles;
	char	buffer[MAX_TOKEN_CHARS];
	int i;

	if (!g_cheats.integer) {
		trap_SendServerCommand(ent - g_entities, va("print \"Cheats are not enabled on this server.\n\""));
		return;
	}

	if (trap_Argc() != 5) {
		trap_SendServerCommand(ent - g_entities, va("print \"usage: setviewpos x y z yaw\n\""));
		return;
	}

	VectorClear(angles);

	for (i = 0 ; i < 3 ; i++) {
		trap_Argv(i + 1, buffer, sizeof(buffer));
		origin[i] = atof(buffer);
	}

	trap_Argv(4, buffer, sizeof(buffer));
	angles[YAW] = atof(buffer);

	TeleportPlayer(ent, origin, angles);
}

/**
 * G_AutoRadio
 */
void G_AutoRadio(int number, gentity_t *ent)
{
	char  l[MAX_SAY_TEXT];
	int   i;

	/* Spectators, ghosts and dead people can't use radio calls. */
	if (ent->client->ghost || (ent->client->ps.stats[STAT_HEALTH] <= 0) || (ent->client->sess.sessionTeam == TEAM_SPECTATOR)) {
		return;
	}

	/* If unable to get the map location. */
	if (!Team_GetLocationMsg(ent, l, sizeof(l))) {
		l[0] = 0;
	}

	/* Send to all appropriate clients. */
	for (i = 0; i < level.maxclients; i++) {
		gentity_t  *other = &g_entities[i];

		/* If client is not connected. */
		if (other->client->pers.connected != CON_CONNECTED) {
			continue;
		}

		/* If client is not on the same team. */
		if (!OnSameTeam(ent, other)) {
			continue;
		}

		/* Send command to client. */
		trap_SendServerCommand(i, va("rsay %d %d %d \"%s\"", ent->client->ps.clientNum, -1, number, l));

		if (ent->client->maparrow) {
			ent->client->maparrow->s.time	  = level.time + 3500;
			ent->client->maparrow->s.generic1 = 1;
		}
	}
}

/**
 * Cmd_Radio_f
 *
 * Sends a radio msg
 */
void Cmd_Radio_f(gentity_t *ent)
{
	char  l[MAX_SAY_TEXT];
	char  s[MAX_SAY_TEXT];
	int   group, msg, colour, i;

	/* Spectators, ghosts and dead people can't use radio calls. */
	if (ent->client->ghost || (ent->client->ps.stats[STAT_HEALTH] <= 0) || (ent->client->sess.sessionTeam == TEAM_SPECTATOR)) {
		return;
	}

	//@Barbatos: avoid spam by the muted players
	if (ent->client->sess.muteTimer > level.time)
		return;

	/* Get first argument. */
	s[0] = 0;
	trap_Argv(1, s, sizeof(s));

	/* If argument is invalid. */
	if (s[0] == 0) {
		return;
	}

	/*Check for "LOL" */
	Q_strlwr(s);

	if (!strcmp(s, "lol")) {
		G_AutoRadio(rand() % 3, ent);
		return;
	}

	/* Convert to integer. */
	group = atoi(s) ;

	/* If group number is out of bounds. */
	if ((group < 1) || (group > 9)) {
		return;
	}

	/* Get second argument. */
	s[0] = 0;
	trap_Argv(2, s, sizeof(s));

	/* If argument is invalid. */
	if (s[0] == 0) {
		return;
	}

	/* Convert to integer. */
	msg = atoi(s) ;

	/* If message number is out of bounds or sound file does not exist. */
	if ((msg < 0) || (msg > 9)) {
		return;
	}

	if (strlen(RadioTextList[group - 1][msg]) == 0) {
		return; /*empty messages not allowed */
	}

	/* If unable to get the map location. */
	if (!Team_GetLocationMsg(ent, l, sizeof(l))) {
		l[0] = 0;
	}

	/* Get third argument, */
	Q_strncpyz(s, ConcatArgs(3), sizeof(s));

	if (strlen(s) == 0) {
		/*Use the default text */
		strcpy(s, RadioTextList[group - 1][msg]);
	} else {
		/* Convert tokens to values. */
		G_ChatSubstitution(ent, s);
	}

	colour = group % 3;

	/* If this is a medic call, make medic icon appear over the player. */
	//@Fenix - do not show also for Jump and LMS
	if ((group == 3) && (msg == 3) &&
		(g_gametype.integer != GT_FFA) &&
		(g_gametype.integer != GT_LASTMAN) &&
		(g_gametype.integer != GT_JUMP)) {
		/* Display medic icon for 10 seconds. */
		ent->client->rewardTime = level.time + 10000;
		ent->client->ps.eFlags |= EF_UT_MEDIC;
	}

	/* Send to all appropriate clients. */
	for (i = 0; i < level.maxclients; i++) {
		gentity_t  *other = &g_entities[i];

		/* If client is not connected. */
		if (other->client->pers.connected != CON_CONNECTED) {
			continue;
		}

		/* If client is not on the same team. */
		if ((g_gametype.integer != GT_JUMP) && (!OnSameTeam(ent, other))) {
			continue;
		}

		// Fenix: if the client is ignored by the "other" one
		if (other->client->pers.ignoredClients[ent->client->ps.clientNum]) {
			return;
		}

		/* Send command to client. */
		trap_SendServerCommand(i, va("rsay %d %d %d \"%s\" \"%s\"", ent->s.number, group, msg, l, s));

		if (ent->client->maparrow) {
			ent->client->maparrow->s.time	  = level.time + 3500;
			ent->client->maparrow->s.generic1 = colour;
		}
	}

	//@Barbatos: logging the radio binds
	G_LogPrintf("Radio: %d - %d - %d - \"%s\" - \"%s\"\n", ent->s.number, group, msg, l, s);
}

/**
 * Cmd_DropWeapon_f
 *
 * Drops a weapon...
 */
void Cmd_DropWeapon_f(gentity_t *ent)
{
	int  weaponid;

	/* Make sure they arent dead */
	if ((ent->client->sess.sessionTeam == TEAM_SPECTATOR) || ent->client->ghost) {
		return;
	}

	/* Cant drop if your not alive */
	if (ent->client->ps.pm_type >= PM_DEAD) {
		return;
	}

	/* Cant drop if just fired */
	/*if ( ent->client->ps.weaponstate != WEAPON_READY ) */
	/*	return; */

	/* Get the id of the item to drop */
	weaponid = atoi(ConcatArgs(1));

	if (!weaponid) {
		return;
	}

	if ((weaponid == UT_WP_BOMB) || (weaponid == UT_WP_SR8)) {
		if (ent->client->ps.weaponstate != WEAPON_READY) {
			return;
		}
	}

	Drop_Weapon(ent, weaponid, 0);
}

/**
 * Cmd_DropItem_f
 *
 * Drops an item.
 */
void Cmd_DropItem_f(gentity_t *ent)
{
	int   itemid;
	char  s[10];

	/* Make sure they arent dead */
	if ((ent->client->sess.sessionTeam == TEAM_SPECTATOR) || ent->client->ghost) {
		return;
	}

	/* Cant drop if your not alive */
	if (ent->client->ps.pm_type >= PM_DEAD) {
		return;
	}

	/*get the info */
	strcpy(s, "");
	trap_Argv(1, s, sizeof(s));

	/* Get the id of the item to drop */
	itemid = atoi(s);

	if (!itemid) {
		if (!Q_stricmp(s, "kevlar")) {
			itemid = UT_ITEM_VEST;
		} else if (!Q_stricmp(s, "helmet")) {
			itemid = UT_ITEM_HELMET;
		} else if (!Q_stricmp(s, "nvg")) {
			itemid = UT_ITEM_NVG;
		} else if (!Q_stricmp(s, "medkit")) {
			itemid = UT_ITEM_MEDKIT;
		} else if (!Q_stricmp(s, "silencer")) {
			itemid = UT_ITEM_SILENCER;
		} else if (!Q_stricmp(s, "laser")) {
			itemid = UT_ITEM_LASER;
		} else if (!Q_stricmp(s, "flag")) {
			//@Fenix - changed the flag itemdrop behavior so it's
			//possible to use "ut_itemdrop flag" also in Jump mode
			if (utPSHasItem(&ent->client->ps, UT_ITEM_REDFLAG)) {
				itemid = UT_ITEM_REDFLAG;
			} else if (utPSHasItem(&ent->client->ps, UT_ITEM_BLUEFLAG)) {
				itemid = UT_ITEM_BLUEFLAG;
			} else if (utPSHasItem(&ent->client->ps, UT_ITEM_NEUTRALFLAG)) {
				itemid = UT_ITEM_NEUTRALFLAG;
			} else {
				return;
			}
		} else {
			return;
		}
	}

	/* Team leaders cant toss down their kevlar or helmet in assasin */
	if (itemid == UT_ITEM_HELMET) {
		if (g_gametype.integer == GT_UT_ASSASIN) {
			if (ent->client->sess.teamLeader) {
				return;
			}
		}
	}

	Drop_Item(ent, &bg_itemlist[itemid], 0, qtrue);

	if (itemid == UT_ITEM_LASER)
		ent->client->laserSight = qfalse;
}

/**
 * Cmd_UseItem_f
 *
 * Uses an item.
 */
void Cmd_UseItem_f(gentity_t *ent)
{
	int  itemid;
	int  itemSlot;
	int  itemFlags;
	char s[10];

	/* Cant drop if your not alive */
	if (ent->client->ps.pm_type >= PM_DEAD) {
		return;
	}

	/* Get the id of the item to drop */
	strcpy(s, "");
	trap_Argv(1, s, sizeof(s));

	itemid = atoi(s);

	// B1n: Allow for text-based itemid
	if (!itemid) {
		if (!Q_stricmp(s, "kevlar")) {
			itemid = UT_ITEM_VEST;
		} else if (!Q_stricmp(s, "helmet")) {
			itemid = UT_ITEM_HELMET;
		} else if (!Q_stricmp(s, "nvg")) {
			itemid = UT_ITEM_NVG;
		} else if (!Q_stricmp(s, "medkit")) {
			itemid = UT_ITEM_MEDKIT;
		} else if (!Q_stricmp(s, "silencer")) {
			itemid = UT_ITEM_SILENCER;
		} else if (!Q_stricmp(s, "laser")) {
			itemid = UT_ITEM_LASER;
		}  else {
			return;
		}
	}

	/* Find the item being dropped */
	if (-1 == (itemSlot = utPSGetItemByID(&ent->client->ps, itemid))) {
		return;
	}

	itemFlags = UT_ITEM_GETFLAGS(ent->client->ps.itemData[itemSlot]);

	/* Toggle the item on and off */
	if (itemFlags & UT_ITEMFLAG_ON) {
		UT_ITEM_SETFLAGS(ent->client->ps.itemData[itemSlot], itemFlags & (~UT_ITEMFLAG_ON));

		if (itemid == UT_ITEM_LASER)
			ent->client->laserSight = qfalse;
	} else {
		UT_ITEM_SETFLAGS(ent->client->ps.itemData[itemSlot], itemFlags | UT_ITEMFLAG_ON);

		if (itemid == UT_ITEM_LASER)
			ent->client->laserSight = qtrue;
	}
}

/**
 * Cmd_Elevator_f
 *
 * Handles elevator control requests.
 */
void Cmd_Elevator_f(gentity_t *ent)
{
	int    j;
	gentity_t  *train;
	gentity_t  *next;
	char	   *stopID;
	char	   *trainIDChar;
	int    trainID;

	trainIDChar = ConcatArgs(1);
	trainID 	= atoi(trainIDChar);

	stopID		= ConcatArgs(2);

	if (ent->client->ps.pm_type >= PM_DEAD) {
		return;
	}

	for (j = 0; j < MAX_GENTITIES; j++) {
		train = &g_entities[j];

//		  if (!Q_stricmp(train->classname, "func_ut_train") && (train->mover->trainID == trainID)) {
		if ((train->classhash==HSH_func_ut_train) && (train->mover->trainID == trainID)) {
			if (train->s.pos.trType != TR_STATIONARY) {
				return; 		/* Gotta: can't change the train while it's moving */
			}

			next = UTFindDesiredStop(stopID);

			if (next) {
				if (!Q_stricmp(next->targetname, train->mover->currTrain->targetname)) {
					return;
				}

				UTSetDesiredStop(train, next);
				train->nextthink = level.time;
			}
		}
	}
}

/**
 * Switch_Train_Stops
 */
void Switch_Train_Stops(gentity_t *ent, int trainID)
{
	int    j;
	gentity_t  *train;
	gentity_t  *next;

	if (ent && (ent->client->ps.pm_type >= PM_DEAD)) {
		return;
	}

	for (j = 0; j < MAX_GENTITIES; j++) {
		train = &g_entities[j];

		if (train->mover) {
//			  if (!Q_stricmp(train->classname, "func_ut_train") && (train->mover->trainID == trainID)) {
			if ((train->classhash==HSH_func_ut_train) && (train->mover->trainID == trainID)) {
				if (train->s.pos.trType != TR_STATIONARY) {
					return; 	/* Gotta: can't change the train while it's moving */
				}

				next = NULL;

				while ((next = G_FindClassHash(next, HSH_path_ut_stop)) != NULL) {
					if ((next->mover->trainID == train->mover->trainID) && (Q_stricmp(next->targetname, train->mover->currTrain->targetname))) {
						next->use(next, next, ent);
						break;
					} else {
						continue;
					}
				}
			}
		}
	}
}

/**
 * Cmd_Switch_Train_Stops_f
 *
 * Handles elevator switching when there are only 2 stops and display
 * is false
 */
void Cmd_Switch_Train_Stops_f(gentity_t *ent)
{
	char  *trainIDChar;
	int   trainID;

	trainIDChar = ConcatArgs(1);
	trainID 	= atoi(trainIDChar);

	Switch_Train_Stops(ent, trainID);
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : Cmd_Interface_f
// Description: Handles interface results
// Changelist : Gotta 10 March 2001
///////////////////////////////////////////////////////////////////////////////////////////
void Cmd_Interface_f(gentity_t *ent) {

	char		*selection;
	int 		selectionID;
	gentity_t	*interfaceEnt;
	gentity_t	*targetEnt;
	int 		currSelection;

	currSelection = atoi(ConcatArgs( 1 ));

	selection = ConcatArgs( 2 );
	selectionID = atoi(selection);

	targetEnt = NULL;
	interfaceEnt = NULL;
	while ((interfaceEnt = G_FindClassHash (interfaceEnt, HSH_func_keyboard_interface)) != NULL) {
		switch( currSelection ){
			case 0:
			{
				switch ( selectionID )
				{
					case 1:
					{
						if ((targetEnt = G_Find (NULL, FOFS(targetname), interfaceEnt->mover->stop2from1)) != NULL) {
//							interfaceEnt->mover->destination = G_Find(NULL, FOFS(targetname), interfaceEnt->mover->stop2);
							targetEnt->use(targetEnt, targetEnt, ent);
						}
						break;
					}
					case 2:
					{
						if ((targetEnt = G_Find (NULL, FOFS(targetname), interfaceEnt->mover->stop3from1)) != NULL) {
//							interfaceEnt->mover->destination = G_Find(NULL, FOFS(targetname), interfaceEnt->mover->stop3);
							targetEnt->use(targetEnt, targetEnt, ent);
						}
						break;
					}
					case 3:
					{
						if ((targetEnt = G_Find (NULL, FOFS(targetname), interfaceEnt->mover->stop4from1)) != NULL) {
//							interfaceEnt->mover->destination = G_Find(NULL, FOFS(targetname), interfaceEnt->mover->stop4);
							targetEnt->use(targetEnt, targetEnt, ent);
						}
						break;
					}
					case 4:
					{
						if ((targetEnt = G_Find (NULL, FOFS(targetname), interfaceEnt->mover->stop5from1)) != NULL) {
//							interfaceEnt->mover->destination = G_Find(NULL, FOFS(targetname), interfaceEnt->mover->stop5);
							targetEnt->use(targetEnt, targetEnt, ent);
						}
						break;
					}
					default: break;
				}
				break;
			}
			case 1:
			{
				switch ( selectionID )
				{
					case 0:
					{
						if ((targetEnt = G_Find (NULL, FOFS(targetname), interfaceEnt->mover->stop1from2)) != NULL) {
//							interfaceEnt->mover->destination = G_Find(NULL, FOFS(targetname), interfaceEnt->mover->stop1);
							targetEnt->use(targetEnt, targetEnt, ent);
						}
						break;
					}
					case 2:
					{
						if ((targetEnt = G_Find (NULL, FOFS(targetname), interfaceEnt->mover->stop3from2)) != NULL) {
//							interfaceEnt->mover->destination = G_Find(NULL, FOFS(targetname), interfaceEnt->mover->stop3);
							targetEnt->use(targetEnt, targetEnt, ent);
						}
						break;
					}
					case 3:
					{
						if ((targetEnt = G_Find (NULL, FOFS(targetname), interfaceEnt->mover->stop4from2)) != NULL) {
//							interfaceEnt->mover->destination = G_Find(NULL, FOFS(targetname), interfaceEnt->mover->stop4);
							targetEnt->use(targetEnt, targetEnt, ent);
						}
						break;
					}
					case 4:
					{
						if ((targetEnt = G_Find (NULL, FOFS(targetname), interfaceEnt->mover->stop5from2)) != NULL) {
//							interfaceEnt->mover->destination = G_Find(NULL, FOFS(targetname), interfaceEnt->mover->stop5);
							targetEnt->use(targetEnt, targetEnt, ent);
						}
						break;
					}
					default: break;
				}
				break;
			}
		case 2:
			{
				switch ( selectionID )
				{
					case 0:
					{
						if ((targetEnt = G_Find (NULL, FOFS(targetname), interfaceEnt->mover->stop1from3)) != NULL) {
//							interfaceEnt->mover->destination = G_Find(NULL, FOFS(targetname), interfaceEnt->mover->stop1);
							targetEnt->use(targetEnt, targetEnt, ent);
						}
						break;
					}
					case 1:
					{
						if ((targetEnt = G_Find (NULL, FOFS(targetname), interfaceEnt->mover->stop2from3)) != NULL) {
//							interfaceEnt->mover->destination = G_Find(NULL, FOFS(targetname), interfaceEnt->mover->stop2);
							targetEnt->use(targetEnt, targetEnt, ent);
						}
						break;
					}
					case 3:
					{
						if ((targetEnt = G_Find (NULL, FOFS(targetname), interfaceEnt->mover->stop4from3)) != NULL) {
//							interfaceEnt->mover->destination = G_Find(NULL, FOFS(targetname), interfaceEnt->mover->stop4);
							targetEnt->use(targetEnt, targetEnt, ent);
						}
						break;
					}
					case 4:
					{
						if ((targetEnt = G_Find (NULL, FOFS(targetname), interfaceEnt->mover->stop5from3)) != NULL) {
//							interfaceEnt->mover->destination = G_Find(NULL, FOFS(targetname), interfaceEnt->mover->stop5);
							targetEnt->use(targetEnt, targetEnt, ent);
						}
						break;
					}
					default: break;
				}
				break;
			}
			case 3:
			{
				switch ( selectionID )
				{
					case 0:
					{
						if ((targetEnt = G_Find (NULL, FOFS(targetname), interfaceEnt->mover->stop1from4)) != NULL) {
//							interfaceEnt->mover->destination = G_Find(NULL, FOFS(targetname), interfaceEnt->mover->stop1);
							targetEnt->use(targetEnt, targetEnt, ent);
						}
						break;
					}
					case 1:
					{
						if ((targetEnt = G_Find (NULL, FOFS(targetname), interfaceEnt->mover->stop2from4)) != NULL) {
//							interfaceEnt->mover->destination = G_Find(NULL, FOFS(targetname), interfaceEnt->mover->stop2);
							targetEnt->use(targetEnt, targetEnt, ent);
						}
						break;
					}
					case 2:
					{
						if ((targetEnt = G_Find (NULL, FOFS(targetname), interfaceEnt->mover->stop3from4)) != NULL) {
//							interfaceEnt->mover->destination = G_Find(NULL, FOFS(targetname), interfaceEnt->mover->stop3);
							targetEnt->use(targetEnt, targetEnt, ent);
						}
						break;
					}
					case 4:
					{
						if ((targetEnt = G_Find (NULL, FOFS(targetname), interfaceEnt->mover->stop5from4)) != NULL) {
//							interfaceEnt->mover->destination = G_Find(NULL, FOFS(targetname), interfaceEnt->mover->stop5);
							targetEnt->use(targetEnt, targetEnt, ent);
						}
						break;
					}
					default: break;
				}
				break;
			}
			case 4:
			{
				switch ( selectionID )
				{
					case 0:
					{
						if ((targetEnt = G_Find (NULL, FOFS(targetname), interfaceEnt->mover->stop1from5)) != NULL) {
//							interfaceEnt->mover->destination = G_Find(NULL, FOFS(targetname), interfaceEnt->mover->stop1);
							targetEnt->use(targetEnt, targetEnt, ent);
						}
						break;
					}
					case 1:
					{
						if ((targetEnt = G_Find (NULL, FOFS(targetname), interfaceEnt->mover->stop2from5)) != NULL) {
//							interfaceEnt->mover->destination = G_Find(NULL, FOFS(targetname), interfaceEnt->mover->stop2);
							targetEnt->use(targetEnt, targetEnt, ent);
						}
						break;
					}
					case 2:
					{
						if ((targetEnt = G_Find (NULL, FOFS(targetname), interfaceEnt->mover->stop3from5)) != NULL) {
//							interfaceEnt->mover->destination = G_Find(NULL, FOFS(targetname), interfaceEnt->mover->stop3);
							targetEnt->use(targetEnt, targetEnt, ent);
						}
						break;
					}
					case 3:
					{
						if ((targetEnt = G_Find (NULL, FOFS(targetname), interfaceEnt->mover->stop4from5)) != NULL) {
//							interfaceEnt->mover->destination = G_Find(NULL, FOFS(targetname), interfaceEnt->mover->stop4);
							targetEnt->use(targetEnt, targetEnt, ent);
						}
						break;
					}
					default: break;
				}
				break;
			}
		}

	}

}

/**
 * Cmd_RefLogin_f
 */
void Cmd_RefLogin_f(gentity_t *ent) {

	char arg[MAX_TOKEN_CHARS];

	if (g_referee.integer == 0) {
		trap_SendServerCommand(ent->client->ps.clientNum, "print \"Referee has been disabled on this server.\n\"");
		return; 	/* allows server to disable referee function */
	}

	if (g_refClient.integer != -1) {
		trap_SendServerCommand(ent->client->ps.clientNum, "print \"There is a referee already.\n\"");
		return;
	}

	if (strlen(g_refPass.string) == 0) {
		trap_SendServerCommand(ent->client->ps.clientNum, "print \"Server has not set a referee password.\n\"");
		return;
	}

	trap_Argv(1, arg, sizeof(arg));

	if (Q_stricmp(arg, g_refPass.string) == 0) {
		g_refClient.integer = ent->client->ps.clientNum;
	} else {
		trap_SendServerCommand(ent->client->ps.clientNum, "print \"Incorrect referee password.\n\"");
		return;
	}

	trap_SendServerCommand(ent->client->ps.clientNum, "print \"Login successful. You are now the referee. Use ref help for a list of commands.\n\"");
}

/**
 * Cmd_Ref_f
 */
void Cmd_Ref_f(gentity_t *ent) {

	char arg[MAX_TOKEN_CHARS];

	// Allow servers to disable referee function.
	if (g_referee.integer == 0) {
		trap_SendServerCommand(ent->client->ps.clientNum, "print \"Referee has been disabled on this server.\n\"");
		return;
	}

	if (ent->client->ps.clientNum != g_refClient.integer) {
		trap_SendServerCommand(ent->client->ps.clientNum, "print \"You are not a referee.\n\"");
		return;
	}

	trap_Argv(1, arg, sizeof(arg));

	/* KICK PLAYER COMMAND */
	if (Q_stricmp(arg, "kick") == 0) {
		Cmd_RefKick_f();
		return;
	/* MUTE COMMAND */
	} else if (Q_stricmp(arg, "mute") == 0) {
		Cmd_RefMute_f();
		return;
	/* FORCE TEAM COMMAND */
	} else if (Q_stricmp(arg, "forceteam") == 0) {
		Cmd_RefForceTeam_f();
		return;
	/* BAN PLAYER COMMAND */
	} else if (Q_stricmp(arg, "ban") == 0) {
		Cmd_RefBan_f();
		return;
	/* REFEREE RESIGN COMMAND */
	} else if (Q_stricmp(arg, "resign") == 0) {
		trap_SendServerCommand(g_refClient.integer, "print \"You resigned as referee\n\"");
		g_refClient.integer = -1;
		return;
	/* REFEREE PAUSE COMMAND */
	} else if (Q_stricmp(arg, "pause") == 0) {
		trap_SendServerCommand(g_refClient.integer, "print \"Feature not implemented yet.\n\"");
		return;
	/* REFEREE RESUME COMMAND */
	} else if (Q_stricmp(arg, "resume") == 0) {
		trap_SendServerCommand(g_refClient.integer, "print \"Feature not implemented yet.\n\"");
		return;
	/* REFEREE RESTART COMMAND */
	} else if (Q_stricmp(arg, "restart") == 0) {
		trap_SendServerCommand(g_refClient.integer, "print \"Feature not implemented yet.\n\"");
		return;
	/* REFEREE HELP COMMAND */
	} else if (Q_stricmp(arg, "help") == 0) {
		Cmd_RefHelp_f();
		return;
	} else {
		trap_SendServerCommand(g_refClient.integer, va("print \"Unknown referee command: %s.\nUse ref help for a list of commands.\n\"", arg));
	}
}

//@Fenix  - created separate function.
void Cmd_RefMute_f(void) {

	gclient_t *cl;
	char arg1[MAX_TOKEN_CHARS];
	char arg2[MAX_TOKEN_CHARS];
	char *suffix;
	int duration;
	int i;

	if ((trap_Argc() != 3) && (trap_Argc() != 4)) {
		trap_SendServerCommand(g_refClient.integer, "print \"Usage: ref mute <player> [<seconds>]\n\"");
		return;
	}

	// Retrieve the correct client.
	trap_Argv(2, arg1, sizeof(arg1));
	cl = ClientForString(arg1);

	if (!cl) {
		return;
	}

	if (trap_Argc() == 4) {
		// Retrieve the specified duration and checking
		// for a proper input value (only digits).
		trap_Argv(3, arg2, sizeof(arg2));
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
				trap_SendServerCommand(cl->ps.clientNum, "print \"The referee muted you: you cannot talk.\"\n");
				trap_SendServerCommand(-1, va( "print \"Player %s has been ^1muted ^7by the referee for ^1%d ^7%s.\n\"", cl->pers.netname, duration, suffix));

			} else {
				// Negative or 0 duration specified.
				// Ensure the player unmute.
				cl->sess.muteTimer = 0;
				trap_SendServerCommand(cl->ps.clientNum, "print \"The referee unmuted you.\"\n");
				trap_SendServerCommand(-1, va("print \"Player %s has been ^2unmuted ^7by the referee.\n\"", cl->pers.netname));
			}

		} else {
			trap_SendServerCommand(g_refClient.integer, "print \"Usage: ref mute <player> [<seconds>]\n\"");
			return;
		}

	} else {

		// We have no duration specified.
		if (cl->sess.muteTimer > 0) {
			// Unmuting the player.
			cl->sess.muteTimer = 0;
			trap_SendServerCommand(cl->ps.clientNum, "print \"The referee unmuted you.\"\n");
			trap_SendServerCommand(-1, va("print \"Player %s has been ^2unmuted ^7by the referee.\n\"", cl->pers.netname));
		} else {
			// Muting the player.
			// Using a very long mute expire time:
			// 2000000 seconds -> almost 556 hours xD.
			cl->sess.muteTimer = level.time + 2000000 * 1000;
			trap_SendServerCommand(cl->ps.clientNum, "print \"The referee muted you: you cannot talk.\"\n");
			trap_SendServerCommand(-1, va( "print \"Player %s has been ^1muted ^7by the referee.\n\"", cl->pers.netname));
		}

	}
}

//@Fenix  - created separate function.
void Cmd_RefForceTeam_f(void) {

	gclient_t *cl;
	char arg1[MAX_TOKEN_CHARS];
	char arg2[MAX_TOKEN_CHARS];

	if (trap_Argc() != 4) {
		trap_SendServerCommand(g_refClient.integer, "print \"Usage: ref forceteam <player> <red|blue|spectator>\n\"");
		return;
	}

	// Retrieve the correct client.
	trap_Argv(2, arg1, sizeof(arg1));
	cl = ClientForString(arg1);

	if (!cl) {
		return;
	}

	// Retrieve the specified team.
	trap_Argv(3, arg2, sizeof(arg2));

	// Actually SetTeam will handle mangled team names but
	// We perform the check anyway in order to show the help text.
	if ((Q_stricmp(arg2, "red") != 0) && (Q_stricmp(arg2, "blue") != 0) && (Q_stricmp(arg2, "spectator") != 0)) {
		trap_SendServerCommand(g_refClient.integer, "print \"Usage: ref forceteam <player> <red|blue|spectator>\n\"");
		return;
	}

	SetTeam(&g_entities[cl - level.clients], arg2, qfalse);
	trap_SendServerCommand(cl->ps.clientNum, "print \"The referee changed your team.\n\"");

}

//@Fenix  - created separate function.
void Cmd_RefKick_f(void) {

	gclient_t *cl;
	char arg1[MAX_TOKEN_CHARS];
	char arg2[MAX_TOKEN_CHARS];

	if (trap_Argc() < 3) {
		trap_SendServerCommand(g_refClient.integer, "print \"Usage: ref kick <player> [<reason>]\n\"");
		return;
	}

	// Retrieve the correct client.
	trap_Argv(2, arg1, sizeof(arg1));
	cl = ClientForString(arg1);

	if (!cl) {
		return;
	}

	if (trap_Argc() > 3) {
		// Reason was specified by the referee.
		Com_sprintf(arg2, sizeof(arg2), "was kicked by the referee (%s)", ConcatArgs(3));
	}
	else {
		// No reason specified. Using the default one.
		Com_sprintf(arg2, sizeof(arg2), "was kicked by the referee");
	}

	trap_DropClient(cl->ps.clientNum, arg2);

}

//@Fenix  - created separate function.
void Cmd_RefBan_f(void) {

	gclient_t *cl;
	char arg1[MAX_TOKEN_CHARS];
	char arg2[MAX_TOKEN_CHARS];
	char *ipaddr;
	char userinfo[MAX_INFO_STRING];
	int banmins;
	qtime_t time;

	if (g_refNoBan.integer) {
		trap_SendServerCommand(g_refClient.integer, "print \"Referees can't use the ban feature on this server.\n\"");
		return;
	}

	if (!g_dedicated.integer) {
		trap_SendServerCommand(g_refClient.integer, "print \"Ban feature only works on dedicated servers.\n\"");
		return;
	}

	if (trap_Argc() != 4) {
		trap_SendServerCommand(g_refClient.integer, "print \"Usage: ref ban <player> <minutes>\n\"");
		return;
	}

	// Retrieve the correct client.
	trap_Argv(2, arg1, sizeof(arg1));
	cl = ClientForString(arg1);

	if (!cl) {
		return;
	}

	// Retrieve the player IP address.
	trap_GetUserinfo(cl->ps.clientNum, userinfo, sizeof(userinfo));
	ipaddr = Info_ValueForKey(userinfo, "ip");

	// Retrieve the number of minutes.
	trap_Argv(3, arg2, sizeof(arg2));
	banmins = atoi(arg2);

	if (banmins <= 0) {
		// Removing the ban.
		SetBanTime(ipaddr, 0, qtrue);
		return;
	}

	// Cap max ban time to 3 hours
	if (banmins > 180) {
		banmins = 180;
		trap_SendServerCommand(g_refClient.integer, "print \"Ban time capped to 180 minutes.\n\"");
	}

	// Add the player to the ban list.
	AddIP(ipaddr, qtrue);

	// Set the ban time.
	trap_RealTime(&time);
	banmins += TimeToMins(&time);
	SetBanTime(ipaddr, banmins, qtrue);

}

//@Fenix  - created separate function.
void Cmd_RefHelp_f(void) {

	trap_SendServerCommand(g_refClient.integer, "print \"Referee commands:\n\"");
	trap_SendServerCommand(g_refClient.integer, "print \"ref mute <player> [<seconds>]	- Disallow a player to use the chat\n\"");
	trap_SendServerCommand(g_refClient.integer, "print \"ref forceteam <player> <team>	- Force a player in the specified team\n\"");
	trap_SendServerCommand(g_refClient.integer, "print \"ref kick <player> [<reason>]	- Kick a player from the server\n\"");
	trap_SendServerCommand(g_refClient.integer, "print \"ref ban <player> <minutes> 	- Ban a player from the server\n\"");
	trap_SendServerCommand(g_refClient.integer, "print \"ref pause				- Pause a game in progress\n\"");
	trap_SendServerCommand(g_refClient.integer, "print \"ref resume 			- Resume a paused game\n\"");
	trap_SendServerCommand(g_refClient.integer, "print \"ref restart			- Restart a paused game\n\"");
	trap_SendServerCommand(g_refClient.integer, "print \"ref resign 			- Resign from being the referee\n\"");

}

/**
 * BecomeSubstitute
 */
qboolean BecomeSubstitute(gentity_t *ent)
{
	gclient_t  *client;
	int    clientNumber;
	int    teamCount   = 0;
	int    activeCount = 0;
	int    i;

	/* If invalid client. */
	if (!ent->client) {
		return qfalse;
	}

	/* Set client pointer. */
	client = ent->client;

	/* Get client number. */
	clientNumber = client - level.clients;

	/* If we're not playing a team based game type. */
	if ((g_gametype.integer < GT_TEAM) || (g_gametype.integer == GT_JUMP)){
		return qfalse;
	}

	/* If we're not on either the red or the blue team. */
	if ((client->sess.sessionTeam != TEAM_RED) && (client->sess.sessionTeam != TEAM_BLUE)) {
		return qfalse;
	}

	/* Count team clients. */
	for (i = 0; i < level.maxclients; i++) {
		gentity_t  *other = &g_entities[i];

		/* If client is not connected. */
		if (other->client->pers.connected != CON_CONNECTED) {
			continue;
		}

		/* If client is not on our team. */
		if (other->client->sess.sessionTeam != client->sess.sessionTeam) {
			continue;
		}

		/* Increae team count. */
		teamCount++;

		/* If it's our selfs. */
		if ((other->client - level.clients) == clientNumber) {
			continue;
		}

		/* If this client is a substitute or dead. */
		if (other->client->pers.substitute) {	   /* || other->client->ghost || other->client->ps.stats[STAT_HEALTH] <= 0) */
			continue;
		}

		/* Increase active count. */
		activeCount++;
	}

	/* If not enough active clients. */
	if ((teamCount < 2) || (activeCount == 0)) {
		return qfalse;
	}

	/* Set substitute flag. */
	client->pers.substitute = qtrue;

	/* Clear scores. */
	//client->ps.persistant[PERS_SCORE]  = 0;
	//client->ps.persistant[PERS_KILLED] = 0;

	/* Drop team items like flags. */

	/*ChangeTeam(ent); */
	if (!ent->client->ghost) {
		if (utPSGetItemByID(&ent->client->ps, UT_ITEM_REDFLAG) != -1) {
			Team_ReturnFlag(TEAM_RED);
		} else if (utPSGetItemByID(&ent->client->ps, UT_ITEM_BLUEFLAG) != -1) {
			Team_ReturnFlag(TEAM_BLUE);
		} else if (utPSGetWeaponByID(&ent->client->ps, UT_WP_BOMB) != -1) {
			/* Drop the bomb */
			Drop_Weapon(ent, UT_WP_BOMB, rand() % 360);
		}
	}

	/* Set ghost mode. */
	G_SetGhostMode(ent);

	/* Unlink client from collision engine. */
	trap_UnlinkEntity(ent);

	/* Follow mode. */
	client->sess.spectatorClient = client - level.clients;
	client->sess.spectatorState  = SPECTATOR_FOLLOW;

	client->pers.substitute 	 = qtrue;

	return qtrue;
}

/**
 * LeaveSubstitute
 */
qboolean LeaveSubstitute(gentity_t *ent)
{
	gclient_t  *client;

	/* If invalid client. */
	if (!ent->client) {
		return qfalse;
	}

	/* Set client pointer. */
	client = ent->client;

	/* If we're not playing a team based game type. */
	if ((g_gametype.integer < GT_TEAM) || (g_gametype.integer == GT_JUMP)){
		return qfalse;
	}

	/* If we're not on either the red or the blue team. */
	if ((client->sess.sessionTeam != TEAM_RED) && (client->sess.sessionTeam != TEAM_BLUE)) {
		return qfalse;
	}

	/* Clear substitute flag. */
	client->pers.substitute = qfalse;

	/* If we're playing a survivor game type. */
	if (g_survivor.integer) {
		/* If we can join the game. */
		if (level.warmupTime && (!level.survivorNoJoin || client->noGearChange)) {
			client->sess.inactivityTime = level.time + g_inactivity.integer * 1000;

			/* Clear ghost flag. */
			client->ghost = qfalse;
			/* Spawn. */
			ClientSpawn(ent);
		}
	} else {
		client->sess.inactivityTime = level.time + g_inactivity.integer * 1000;

		/* Clear ghost flag. */
		client->ghost = qfalse;

		//@Barbatos: wait the next wave to spawn in CTF mode
		if((g_gametype.integer == GT_CTF) && (g_waveRespawns.integer == 1) && (g_ctfUnsubWait.integer == 1))
			return qtrue;

		/* If we're not playing CTF or ctfwait != 1, spawn the player */
		else
			ClientSpawn(ent);
	}

	return qtrue;
}

/**
 * Cmd_Substitute_f
 */
void Cmd_Substitute_f(gentity_t *ent)
{
	gclient_t  *client;
	int    clientNumber;
	int    delay, delta;

	/* If invalid client. */
	if (!ent->client) {
		return;
	}

	/* Set client pointer. */
	client = ent->client;

	/* Get client number. */
	clientNumber = client - level.clients;

	/* If we're not playing a team based game type. */
	if ((g_gametype.integer < GT_TEAM) || (g_gametype.integer == GT_JUMP)){
		return;
	}

	/* If we're not in match mode. */
	if (!(level.matchState & UT_MATCH_ON)) {
		return;
	}

	/* If we're not on either the red or the blue team. */
	if ((client->sess.sessionTeam != TEAM_RED) && (client->sess.sessionTeam != TEAM_BLUE)) {
		return;
	}

	/* If we're calling sub to soon. */
	if (level.time < client->nextSubCallTime) {
		/* Notify client. */
		trap_SendServerCommand(ent - g_entities, va("print \"You must wait %d seconds before calling sub again.\n\"", (client->nextSubCallTime - level.time) / 1000));

		return;
	}

	/* Calculate call time delay. */
	if (g_survivor.integer) {
		client->nextSubCallTime = level.time + 4000;
	} else {
		if (!g_waveRespawns.integer) {
			if (g_respawnDelay.integer > 2) {
				client->nextSubCallTime = level.time + (g_respawnDelay.integer * 1000);
			} else {
				client->nextSubCallTime = level.time + 2000;
			}
		} else {
			if (client->sess.sessionTeam == TEAM_RED) {
				delay = g_redWave.integer;
			} else {
				delay = g_blueWave.integer;
			}

			if (delay <= 2) {
				delay = 2;
			}

			delta			= (level.time) + ((delay * 1000) - ((level.time - level.pausedTime) % (delay * 1000)));

			client->nextSubCallTime = delta;
		}
	}

	/* If we're not a substitute. */
	if (!client->pers.substitute) {
		/* Become a substitute. */
		if (BecomeSubstitute(ent)) {
			/* If we're on the red team. */
			if (client->sess.sessionTeam == TEAM_RED) {
				/* Notify clients. */
				trap_SendServerCommand(-1, va("ccprint \"%d\" \"%s\" \"%d\"",
					CCPRINT_MATCH_SUB,
					client->pers.netname,
					1));
				//trap_SendServerCommand(-1, va("print \"%s%s %sis a substitute for the %sred %steam.\"", S_COLOR_RED, client->pers.netname, S_COLOR_WHITE, S_COLOR_RED, S_COLOR_WHITE));
			} else if (client->sess.sessionTeam == TEAM_BLUE) {
				trap_SendServerCommand(-1, va("ccprint \"%d\" \"%s\" \"%d\"",
					CCPRINT_MATCH_SUB,
					client->pers.netname,
					2));
				/* Notify clients. */
				//trap_SendServerCommand(-1, va("print \"%s%s %sis a substitute for the %sblue %steam.\"", S_COLOR_BLUE, client->pers.netname, S_COLOR_WHITE, S_COLOR_BLUE, S_COLOR_WHITE));
			}
		}
	} else {
		/* Leave substitute. */
		if (LeaveSubstitute(ent)) {
			MapArrowSpawn(ent->s.number);

			/* If we're on the red team. */
			if (client->sess.sessionTeam == TEAM_RED) {
				/* Notify clients. */
				trap_SendServerCommand(-1, va("ccprint \"%d\" \"%s\" \"%d\"",
					CCPRINT_MATCH_ACTIVE,
					client->pers.netname,
					1));
				//trap_SendServerCommand(-1, va("print \"%s%s %sis an active player for the %sred %steam.\"", S_COLOR_RED, client->pers.netname, S_COLOR_WHITE, S_COLOR_RED, S_COLOR_WHITE));
			} else if (client->sess.sessionTeam == TEAM_BLUE) {
				/* Notify clients. */
				trap_SendServerCommand(-1, va("ccprint \"%d\" \"%s\" \"%d\"",
					CCPRINT_MATCH_ACTIVE,
					client->pers.netname,
					2));
				//trap_SendServerCommand(-1, va("print \"%s%s %sis an active player for the %sblue %steam.\"", S_COLOR_BLUE, client->pers.netname, S_COLOR_WHITE, S_COLOR_BLUE, S_COLOR_WHITE));
			}
		}
	}

	SendScoreboardMessageToAllClients();		/*update it */
}

////////////////////////////////////////////////////////////////
// Name         : Cmd_Ignore_f
// Description  : Add and Remove players in/from the ignore list
// Author       : Barbatos, Fenix
////////////////////////////////////////////////////////////////
void Cmd_Ignore_f(gentity_t *ent) {

    gclient_t *client;
    gclient_t *ignored;
    //char      *name;
    char      str[MAX_TOKEN_CHARS];

    // Set pointer
    client = ent->client;

    // Fenix: check command syntax
    if (trap_Argc() != 2) {
        trap_SendServerCommand(client->ps.clientNum, va("print \"Usage: ignore <client>\n\""));
        return;
    }

    // Find the player
    trap_Argv(1, str, sizeof(str));
    ignored = ClientForString(str);

    // Fenix: block if we didn't find the player
    // or if the client is trying to ignore himself
    if (!ignored || ignored == client) {
        return;
    }

    // Fenix: just switch the flag and display the correct message
    client->pers.ignoredClients[ignored->ps.clientNum] = !client->pers.ignoredClients[ignored->ps.clientNum];

    if (client->pers.ignoredClients[ignored->ps.clientNum]) {
        trap_SendServerCommand(client->ps.clientNum, va("print \"%s has been %sadded %sto your ignore list\n\"", S_COLOR_GREEN, ignored->pers.netname, S_COLOR_WHITE));
    } else {
        trap_SendServerCommand(client->ps.clientNum, va("print \"%s has been %sremoved %sfrom your ignore list\n\"", S_COLOR_RED, ignored->pers.netname, S_COLOR_WHITE));
    }

}

////////////////////////////////////////////////////////////////
// Name         : Cmd_IgnoreList_f
// Description  : Display a client personal ignore list
// Author       : Barbatos, Fenix
////////////////////////////////////////////////////////////////
void Cmd_IgnoreList_f(gentity_t *ent) {

    int       i;
    gclient_t *client;
    gclient_t *other;
    char      name[64];

    // Set pointer
    client = ent->client;

    trap_SendServerCommand(client->ps.clientNum, "print \"Ignored clients:\n\"");

    for(i = 0; i < sizeof(client->pers.ignoredClients); i++) {

        // If it's not ignored
        if (!client->pers.ignoredClients[i])
            continue;

        // Set the client pointer
        other = &level.clients[i];

        // If it's not connected
        if (other->pers.connected != CON_CONNECTED)
            continue;

        // do NOT modify netname
        strcpy(name,other->pers.netname);
        Q_CleanStr(name);

        // Display the ignored player
        trap_SendServerCommand(client->ps.clientNum, va("print \" %2d: [%s]\n\n\"", other->ps.clientNum, name));

    }

}


/////////////////////////////////////////////////////////////////////
// Name        : SortMaps
// Description : Array sorting comparison function (for qsort)
// Author      : Fenix
/////////////////////////////////////////////////////////////////////
static int QDECL SortMaps(const void *a, const void *b) {
    return strcmp (*(const char **) a, *(const char **) b);
}


/////////////////////////////////////////////////////////////////////
// Name        : GetMapSoundingLike
// Description : Retrieve a full map name given a substring of it
// Author      : Fenix
/////////////////////////////////////////////////////////////////////
char *GetMapSoundingLike(gentity_t *ent, const char *s) {

    int  i, mapcount;
    int  len = 0, count = 0;
    char *matches[MAX_MAPLIST_SIZE];
    char *searchmap;
    char expanded[MAX_QPATH];
    char mapname[MAX_QPATH];
    static char maplist[MAX_MAPLIST_STRING];
    fileHandle_t f;

    // [BUGFIX]: instead of iterating through all the maps matching both full and
    // partial name, search just for the exact map name and return it if the match is found
    Com_sprintf(mapname, sizeof(mapname), s);
    Com_sprintf(expanded, sizeof(expanded), "maps/%s.bsp", s);
    if (trap_FS_FOpenFile(expanded, &f, FS_READ) > 0) {
        trap_FS_FCloseFile(f);
        searchmap = mapname;
        return searchmap;
    }

    // We didn't found an exact name match. Keep iterating through all the
    // available maps matching partial substrings
    if (!(mapcount = trap_FS_GetFileList("maps", ".bsp", maplist, sizeof(maplist)))) {
        trap_SendServerCommand(ent - g_entities, "print \"Unable to retrieve map list\n\"");
        return NULL;
    }

    searchmap = maplist;
    for(i = 0; i < mapcount && count < MAX_MAPLIST_SIZE; i++, searchmap += len + 1) {

        len = strlen(searchmap);
        COM_StripExtension(searchmap, searchmap);

        // Fenix: commented out since we moved the
        // check before the for cycle which iterate
        // through all the available maps

        // Check for exact match
        //if (!Q_stricmp(searchmap,s)) {
        //    matches[0] = searchmap;
        //    count = 1;
        //    break;
        //}

        // Check for substring match
        if (Q_strisub(searchmap, s)) {
            matches[count] = searchmap;
            count++;
        }

    }

    if (count == 0) {

        // No match found for the given map name input
        trap_SendServerCommand(ent - g_entities, va("print \"No map found matching \"%s\"\n\"", s));
        return NULL;

    } else if (count > 1) {

        // Multiple matches found for the given map name
        trap_SendServerCommand(ent - g_entities, va("print \"Multiple maps found matching \"%s\":\n\"", s));

        // Sorting the short map list alphabetically
        qsort(matches, count, sizeof(char *), SortMaps);

        for (i = 0; i < count; i++) {
            // Printing a short map list so the user can retry with a more specific name
            trap_SendServerCommand(ent - g_entities, va("print \" %2d: [%s]\n\"", i + 1, matches[i]));
        }

        if (count >= MAX_MAPLIST_SIZE) {
            // Tell the user that there are actually more
            // maps matching the given substring, although
            // we are not displaying them....
            trap_SendServerCommand(ent - g_entities, "print \"...and more\n\"");

        }

        return NULL;

    }

    // If we got here means that we just have a match
    // for the given string. We found our map!
    return matches[0];

}


////////////////////////////////////////////////////////////////
// Name         : Cmd_MapList_f
// Description  : Display the server map list
// Author       : Fenix
////////////////////////////////////////////////////////////////
#if 0
void Cmd_MapList_f(gentity_t *ent) {

    char *assetname;
    char *sortedmaplist[MAX_MAPLIST_SIZE];
    char maplist[MAX_MAPLIST_STRING];
    char mapname[MAX_QPATH];
    int  mapcount;
    int  i, len = 0;

    if (!(mapcount = trap_FS_GetFileList("maps", ".bsp", maplist, sizeof(maplist)))) {
        trap_SendServerCommand(ent - g_entities, "print \"Unable to retrieve map list\n\"");
        return;
    }

    // Clamp the value to prevent overflow
    if (mapcount > MAX_MAPLIST_SIZE) {
        mapcount = MAX_MAPLIST_SIZE;
    }

    assetname = maplist;
    for(i = 0; i < mapcount; i++, assetname += len + 1) {
        len = strlen(assetname);
        COM_StripExtension(assetname, assetname);
        sortedmaplist[i] = assetname;
    }

    // Sorting the map list alphabetically
    qsort(sortedmaplist, mapcount, sizeof(char *), SortMaps);

    trap_SendServerCommand(ent - g_entities, "print \"Current maplist:\n\"");

    for (i = 0; i < mapcount; i++) {
        // Printing the map list
        trap_SendServerCommand(ent - g_entities, va("print \"%3d: [%s]\n\"", i + 1, sortedmaplist[i]));
    }

    trap_SendServerCommand(ent - g_entities, "print \"End current maplist.\n\"");

}
#endif


//@Barbatos: Cmd_ForceSub_f
//@Fenix: removed non necessary code
void Cmd_ForceSub_f(gentity_t *ent)
{
	gclient_t  *client;
	gclient_t  *other;
	char	   str[MAX_TOKEN_CHARS];

	/* Set pointer. */
	client = ent->client;

	/* If we're not in match mode. */
	if (!(level.matchState & UT_MATCH_ON)) {
		return;
	}

	// Search the player
	trap_Argv( 1, str, sizeof(str));
	other = ClientForString( str );

	// Player not found
	if (!other) {
		return;
	}

	// If the dude is not in a valid team or if the dude is not in our team
	if (((client->sess.sessionTeam != TEAM_RED) && (client->sess.sessionTeam != TEAM_BLUE)) || (client->sess.sessionTeam != other->sess.sessionTeam)) {
		return;
	}

	// If the dude aint captain
	if (!client->pers.captain) {
		trap_SendServerCommand(client->ps.clientNum, "print \"You are not captain of any team.\n\"");
		return;
	}

	// If the captain is substitute
	if (client->pers.substitute) {
		trap_SendServerCommand(client->ps.clientNum, "print \"Substitutes can't force another player to become substitute.\n\"");
		return;
	}

	// If the dude is already substitute
	if (other->pers.substitute) {
		trap_SendServerCommand(client->ps.clientNum, "print \"This player is already a substitute of your team.\n\"");
		return;
	}

    if (BecomeSubstitute(&g_entities[other - level.clients])) {
        /* If we're on the red team. */
        if (other->sess.sessionTeam == TEAM_RED) {
            /* Notify clients. */
            trap_SendServerCommand(-1, va("print \"%d\" \"%s\" \"%d\"",
                CCPRINT_MATCH_FORCESUB,
				other->pers.netname,
				1));
		} else if (other->sess.sessionTeam == TEAM_BLUE) {
            /* Notify clients. */
			trap_SendServerCommand(-1, va("print \"%d\" \"%s\" \"%d\"",
				CCPRINT_MATCH_FORCESUB,
				other->pers.netname,
				2));
        }
	} else {
		trap_SendServerCommand(client->ps.clientNum, "print \"Something went wrong!\n\"");
	}
}

/**
 * Cmd_Captain_f
 */
void Cmd_Captain_f(gentity_t *ent)
{
	gclient_t  *client;
	gclient_t  *other;
	int 	   captainClient;
	int 	   i;

	/* Set pointer. */
	client = ent->client;

	/* No captain. */
	captainClient = -1;

	/* If this isn't a team based game type. */
	if ((g_gametype.integer < GT_TEAM) || (g_gametype.integer == GT_JUMP)){
		return;
	}

	/* If we're not in match mode. */
	if (!(level.matchState & UT_MATCH_ON)) {
		return;
	}

	/* If the dude aint on a team. */
	if ((client->sess.sessionTeam != TEAM_RED) && (client->sess.sessionTeam != TEAM_BLUE)) {
		return;
	}

	/* If we're the captain. */
	if (client->pers.captain) {
		/* Clear captain flag. */
		client->pers.captain = qfalse;

		/* If we're on the red team. */
		if (client->sess.sessionTeam == TEAM_RED) {
			/* Notify clients. */
			trap_SendServerCommand(-1, va( "ccprint \"%d\" \"%s\" \"%d\"", CCPRINT_MATCH_UNCAP, client->pers.netname, 1));
			//trap_SendServerCommand(-1, va( "print \"^1%s^7 is no longer the captain of the ^1red^7 team.\"", client->pers.netname));
		} else if (client->sess.sessionTeam == TEAM_BLUE) {
			/* Notify clients. */
			trap_SendServerCommand(-1, va( "ccprint \"%d\" \"%s\" \"%d\"", CCPRINT_MATCH_UNCAP, client->pers.netname, 2));
			//trap_SendServerCommand(-1, va("print \"^4%s^7 is no longer the captain of the ^4blue^7 team.\"", client->pers.netname));
		}

		return;
	}

	/* Loop through clients. */
	for (i = 0; i < level.maxclients; i++) {
		/* Set pointer. */
		other = &level.clients[i];

		/* If client is not connected. */
		if (other->pers.connected != CON_CONNECTED) {
			continue;
		}

		/* If client is not on a valid team. */
		if ((other->sess.sessionTeam != TEAM_RED) && (other->sess.sessionTeam != TEAM_BLUE)) {
			continue;
		}

		/* If this dude is a captain and he's on our team. */
		if (other->pers.captain && (other->sess.sessionTeam == client->sess.sessionTeam)) {
			/* Set captain client number. */
			captainClient = i;

			break;
		}
	}

	/* If there's already a captain on our team. */
	if (captainClient != -1) {
		/* Notify client. */
		trap_SendServerCommand(client - level.clients,
					   va("print \"The captain spot is already taken by %s on your team.\n\"", level.clients[captainClient].pers.netname));

		/*however he might be 999, so check */
		other = &level.clients[captainClient];

		if (other->ps.ping > 900) {
			trap_SendServerCommand(client - level.clients,
						   va("print \"..Except.. he's timed out now.\n\"", level.clients[captainClient].pers.netname));

			/* Clear captain flag. */
			other->pers.captain = qfalse;

			/* If we're on the red team. */
			if (other->sess.sessionTeam == TEAM_RED) {
				/* Notify clients. */
				trap_SendServerCommand(-1, va( "ccprint \"%d\" \"%s\" \"%d\"", CCPRINT_MATCH_UNCAP, client->pers.netname, 1));
				//trap_SendServerCommand(-1, va("print \"^1%s^7 is no longer the captain of the ^1red^7 team. Coup by %s\"", other->pers.netname, client->pers.netname));
			} else if (other->sess.sessionTeam == TEAM_BLUE) {
				/* Notify clients. */
				trap_SendServerCommand(-1, va( "ccprint \"%d\" \"%s\" \"%d\"", CCPRINT_MATCH_UNCAP, client->pers.netname, 2));
				//trap_SendServerCommand(-1, va("print \"^4%s^7 is no longer the captain of the ^4blue^7 team. Coup by %s\"", other->pers.netname, client->pers.netname));
			}

			captainClient = client->ps.clientNum;
		} else {
			return;
		}
	}

	/* Set captain flag. */
	client->pers.captain = qtrue;

	/* If we're on the red team. */
	if (client->sess.sessionTeam == TEAM_RED) {
		/* Notify clients. */
		trap_SendServerCommand(-1, va( "ccprint \"%d\" \"%s\" \"%d\"", CCPRINT_MATCH_CAP, client->pers.netname, 1));
		//trap_SendServerCommand(-1, va("print \"^1%s^7 is now the captain of the ^1red^7 team.\"", client->pers.netname));
	} else if (client->sess.sessionTeam == TEAM_BLUE) {
		/* Notify clients. */
		trap_SendServerCommand(-1, va( "ccprint \"%d\" \"%s\" \"%d\"", CCPRINT_MATCH_CAP, client->pers.netname, 2));
		//trap_SendServerCommand(-1, va("print \"^4%s^7 is now the captain of the ^4blue^7 team.\"", client->pers.netname));
	}
}

/**
 * Cmd_Ready_f
 */
void Cmd_Ready_f(gentity_t *ent)
{
	gclient_t  *client;
	gclient_t  *other;
	gentity_t  *tempEntity;
	int 	   captainClient, i;

	/* Set client pointer. */
	client = ent->client;

	////////////////////////
	// JUMP MODE BEHAVIOR //
	////////////////////////
	if (g_gametype.integer == GT_JUMP) {

		// If the client is a spectator
		if (client->sess.sessionTeam == TEAM_SPECTATOR) {
			return;
		}

		// If matchmode is activated
		if ((g_matchMode.integer > 0) && (g_jumpruns.integer > 0)) {

			// Check if the client is allowed to activate the timers
			if (client->sess.jumpRunCount >= g_jumpruns.integer) {
				trap_SendServerCommand(client->ps.clientNum, "print \"You have reached the maximum allowed attempts for this map\"");
				return;
			}

			// Check if the timer has been deactivated while it was
			// running and if so increment the jumpRunCount variable
			if ((client->sess.jumpRun) && (client->ps.persistant[PERS_JUMPSTARTTIME] > 0)) {

				client->sess.jumpRunCount++;

				// Log everything for external bots
				G_LogPrintf("ClientJumpRunCanceled: %i - way: %i - attempt: %i of %i\n", client->ps.clientNum, client->sess.jumpWay, client->sess.jumpRunCount, g_jumpruns.integer);
				trap_SendServerCommand(-1, va("print \"%s's run has been " S_COLOR_RED "stopped" S_COLOR_WHITE " (Run: %i of %i)\n\"", client->pers.netname, client->sess.jumpRunCount, g_jumpruns.integer));

				// Reset the timer
				client->sess.jumpWay = 0;							  // Clear the jumpWay
				client->ps.persistant[PERS_JUMPSTARTTIME] = 0;		  // Clear the jumpStartTime
				client->ps.persistant[PERS_JUMPWAYS] &= 0xFFFFFF00;   // Clear the jumpWayColor

				tempEntity			         = G_TempEntity(ent->s.pos.trBase, EV_JUMP_CANCEL);
				tempEntity->s.otherEntityNum = ent->s.number;
				tempEntity->r.singleClient	 = ent->s.number;
				tempEntity->s.time		     = level.time;
				tempEntity->r.svFlags		|= SVF_SINGLECLIENT;
			}

		} else {

			if ((client->sess.jumpRun) && (client->ps.persistant[PERS_JUMPSTARTTIME] > 0)) {

				// Log everything for external bots
				G_LogPrintf("ClientJumpRunCanceled: %i - way: %i\n", client->ps.clientNum, client->sess.jumpWay);
				trap_SendServerCommand(-1, va("print \"%s's run has been " S_COLOR_RED "stopped" S_COLOR_WHITE ".\n\"", client->pers.netname));

				// Reset the timer
				client->sess.jumpWay = 0;							  // Clear the jumpWay
				client->ps.persistant[PERS_JUMPSTARTTIME] = 0;		  // Clear the jumpStartTime
				client->ps.persistant[PERS_JUMPWAYS] &= 0xFFFFFF00;   // Clear the jumpWayColor

				tempEntity			         = G_TempEntity(ent->s.pos.trBase, EV_JUMP_CANCEL);
				tempEntity->s.otherEntityNum = ent->s.number;
				tempEntity->r.singleClient	 = ent->s.number;
				tempEntity->s.time		     = level.time;
				tempEntity->r.svFlags		|= SVF_SINGLECLIENT;

			}

		}

		client->sess.jumpRun = !client->sess.jumpRun;
		trap_SendServerCommand(client->ps.clientNum, va("print \"Your personal timer has been %s.\"", ((client->sess.jumpRun == 1) ? "activated" : "deactivated")));
		SendScoreboardSingleMessageToAllClients(ent);
		return;

	}



	/* If this isn't a team based game type. */
	if ((g_gametype.integer < GT_TEAM) || (g_gametype.integer == GT_JUMP)){
		return;
	}

	/* If not enough players on both teams. */
	if ((TeamCount(-1, TEAM_RED) < 1) || (TeamCount(-1, TEAM_BLUE) < 1)) {
		return;
	}

	/* If we're not in match mode. */
	if (!(level.matchState & UT_MATCH_ON)) {
		return;
	}

	/* If game is paused. */
	if (level.pauseState & UT_PAUSE_ON) {
		return;
	}

	/* If the dude aint on a team. */
	if ((client->sess.sessionTeam != TEAM_RED) && (client->sess.sessionTeam != TEAM_BLUE)) {
		return;
	}

	/* If this dude aint the captain. */
	if (!client->pers.captain) {
		/* No captain. */
		captainClient = -1;

		/* Loop through clients. */
		for (i = 0; i < level.maxclients; i++) {
			/* Set pointer. */
			other = &level.clients[i];

			/* If client is not connected. */
			if (other->pers.connected != CON_CONNECTED) {
				continue;
			}

			/* If client is not on a valid team. */
			if ((other->sess.sessionTeam != TEAM_RED) && (other->sess.sessionTeam != TEAM_BLUE)) {
				continue;
			}

			/* If this dude is a captain and he's on our team. */
			if (other->pers.captain && (other->sess.sessionTeam == client->sess.sessionTeam)) {
				/* Set captain client number. */
				captainClient = i;

				break;
			}
		}

		/* If there's no captain */
		if (captainClient == -1) {
			/* Set captain flag. */
			client->pers.captain = qtrue;

			/* If we're on the red team. */
			if (client->sess.sessionTeam == TEAM_RED) {
				/* Notify clients. */
				trap_SendServerCommand(-1, va( "ccprint \"%d\" \"%s\" \"%d\"", CCPRINT_MATCH_CAP, client->pers.netname, 1));
				//trap_SendServerCommand(-1, va("print \"^1%s^7 is now the captain of the ^1red^7 team.\"", client->pers.netname));
			} else if (client->sess.sessionTeam == TEAM_BLUE) {
				/* Notify clients. */
				trap_SendServerCommand(-1, va( "ccprint \"%d\" \"%s\" \"%d\"", CCPRINT_MATCH_CAP, client->pers.netname, 2));
				//trap_SendServerCommand(-1, va("print \"^4%s^7 is now the captain of the ^4blue^7 team.\"", client->pers.netname));
			}
		} else {
			return;
		}
	}

	/* If we're on the red team. */
	if (client->sess.sessionTeam == TEAM_RED) {
		/* If red team is not ready. */
		if (!(level.matchState & UT_MATCH_RR)) {
			/* Set ready flag. */
			level.matchState |= UT_MATCH_RR;

			/* Set config string. */
			trap_SetConfigstring(CS_MATCH_STATE, va("%d", level.matchState));

			/* If the blue team is ready. */
			if (level.matchState & UT_MATCH_BR) {
				/*Set us back to the first half. */
				/*	trap_SetConfigstring( CS_ROUNDSTAT, "0 0 0"); */
				/*	if (g_swaproles.integer) */
				/*	{ */
				/*	  level.swaproles=1; */
				/*	} */

				/* Restart. */
				Svcmd_Restart_f(qtrue, qfalse);
			}
		} else {
			/* If blue team is not ready. */
			if (!(level.matchState & UT_MATCH_BR)) {
				/* Clear ready flag. */
				level.matchState &= ~UT_MATCH_RR;

				/* Set config string. */
				trap_SetConfigstring(CS_MATCH_STATE, va("%d", level.matchState));
			}
		}
	}
	/* Or on the blue team. */
	else {
		/* If blue team is not ready. */
		if (!(level.matchState & UT_MATCH_BR)) {
			/* Set ready flag. */
			level.matchState |= UT_MATCH_BR;

			/* Set config string. */
			trap_SetConfigstring(CS_MATCH_STATE, va("%d", level.matchState));

			/* If the red team is ready. */
			if (level.matchState & UT_MATCH_RR) {
				/*Set us back to the first half. */
				/*	 trap_SetConfigstring( CS_ROUNDSTAT, "0 0 0"); */
				/*	 if (g_swaproles.integer) */
				/*	 { */
				/*	   level.swaproles=1; */
				/*	 } */
				/* Restart. */
				Svcmd_Restart_f(qtrue, qfalse);
			}
		} else {
			/* If red team is not ready. */
			if (!(level.matchState & UT_MATCH_RR)) {
				/* Clear ready flag. */
				level.matchState &= ~UT_MATCH_BR;

				/* Set config string. */
				trap_SetConfigstring(CS_MATCH_STATE, va("%d", level.matchState));
			}
		}
	}
}

/**
 * Cmd_TeamName_f
 */
void Cmd_TeamName_f(gentity_t *ent)
{
	gclient_t	*client;
	const char	*s;

	/* Set client pointer. */
	client = ent->client;

	/* If this isn't a team based game type. */
	if ((g_gametype.integer < GT_TEAM) || (g_gametype.integer == GT_JUMP)){
		return;
	}

	/* If we're not in match mode. */
	if (!(level.matchState & UT_MATCH_ON)) {
		return;
	}

	/* If both teams are ready. */
	if ((level.matchState & UT_MATCH_RR) && (level.matchState & UT_MATCH_BR)) {
		return;
	}

	/* If the dude aint on a team. */
	if ((client->sess.sessionTeam != TEAM_RED) && (client->sess.sessionTeam != TEAM_BLUE)) {
		return;
	}

	/* If this dude aint the captain. */
	if (!client->pers.captain) {
		return;
	}

	/* Get new team name. */
	s = ConcatArgs(1);

	/* If an invalid string. */
	if (strlen(s) >= MAX_NAME_LENGTH) {
		return;
	}

	/* If we're on the red team. */
	if (client->sess.sessionTeam == TEAM_RED) {
		/* Set new name. */
		trap_Cvar_Set("g_nameRed", s);
	}
	/* Or the blue team. */
	else {
		/* Set new name. */
		trap_Cvar_Set("g_nameBlue", s);
	}
}

/**
 * Cmd_Stats_f
 */
void Cmd_Stats_f(gentity_t *entity)
{
	StatsEnginePrint(entity);
}

/**
 * Cmd_Timeout_f
 */
void Cmd_Timeout_f(gentity_t *entity)
{
	gclient_t  *client;

	/* Set client pointer. */
	client = entity->client;

	/* If this isn't a team based game type. */
	if ((g_gametype.integer < GT_TEAM) || (g_gametype.integer == GT_JUMP)){
		return;
	}

	/* If we're not in match mode. */
	if (!(level.matchState & UT_MATCH_ON) || !(level.matchState & UT_MATCH_RR) || !(level.matchState & UT_MATCH_BR)) {
		return;
	}

	/* If the dude aint on a team. */
	if ((client->sess.sessionTeam != TEAM_RED) && (client->sess.sessionTeam != TEAM_BLUE)) {
		return;
	}

	/* If this dude aint the captain. */
	if (!client->pers.captain) {
		return;
	}

	/* If we're paused. */
	if (level.pauseState & UT_PAUSE_ON) {
		/* If we're getting ready to resume. */
		if (level.pauseState & UT_PAUSE_TR) {
			return;
		}

		/* If we're on the red team. */
		if (client->sess.sessionTeam == TEAM_RED) {
			/* If the timeout wasn't called by us. */
			if (!(level.pauseState & UT_PAUSE_RC)) {
				return;
			}
		} else {
			/* If the timeout wasn't called by us. */
			if (!(level.pauseState & UT_PAUSE_BC)) {
				return;
			}
		}

		/* Set flag. */
		level.pauseState |= UT_PAUSE_TR;

		/* Set time to resume. */
		level.pauseTime = level.time + 10000;

		/* Set config string. */
		trap_SetConfigstring(CS_PAUSE_STATE, va("%d %i %i", level.pauseState, level.pauseTime, level.pausedTime));
	} else {
		/* If we're getting ready to pause. */
		if (level.pauseState & UT_PAUSE_TR) {
			return;
		}

		/* If we're on the red team. */
		if (client->sess.sessionTeam == TEAM_RED) {
			/* If red team has no timeouts left. */
			if (level.numRedTimeouts == g_timeouts.integer) {
				return;
			} else {
				level.numRedTimeouts++;
			}
		} else {
			/* If blue team has no timeouts left. */
			if (level.numBlueTimeouts == g_timeouts.integer) {
				return;
			} else {
				level.numBlueTimeouts++;
			}
		}

		/* Set flags. */
		level.pauseState |= UT_PAUSE_TR;
		level.pauseState &= ~UT_PAUSE_AC;

		/* If we're on the red team. */
		if (client->sess.sessionTeam == TEAM_RED) {
			level.pauseState |= UT_PAUSE_RC;
			level.pauseState &= ~UT_PAUSE_BC;
		} else {
			level.pauseState &= ~UT_PAUSE_RC;
			level.pauseState |= UT_PAUSE_BC;
		}

		/* Set time to pause. */
		level.pauseTime = level.time + 3000;

		/* Set config string. */
		trap_SetConfigstring(CS_PAUSE_STATE, va("%d %i %i", level.pauseState, level.pauseTime, level.pausedTime));
	}
}

/**
 * Cmd_ForceAnim_f
 *
 * Force the animation for testing purposes
 */
void Cmd_ForceAnim_f(gentity_t *ent)
{
	char	   arg[MAX_TOKEN_CHARS];

	if (!CheatsOk(ent)) {
		return;
	}

	if (trap_Argc() >= 2) {
		trap_Argv(1, arg, sizeof(arg));
		ent->client->ps.torsoAnim = atoi(arg);

		trap_Argv(2, arg, sizeof(arg));
		ent->client->ps.legsAnim = atoi(arg);
	}

	if (trap_Argc() >= 4) {
		trap_Argv(3, arg, sizeof(arg));
		ent->client->ps.torsoTimer = atoi(arg);

		trap_Argv(4, arg, sizeof(arg));
		ent->client->ps.legsTimer = atoi(arg);
	} else {
		ent->client->ps.torsoTimer = 1000;
		ent->client->ps.legsTimer = 1000;
	}
}

//@Barbatos - jump mode feature - save current position
void Cmd_SavePosition_f(gentity_t *entity) {

	gclient_t *client;

	client = entity->client;

	/* if we're not playing jump gametype */
	if(g_gametype.integer != GT_JUMP)
		return;

	/* If we're in spectator. */
	if (client->sess.sessionTeam == TEAM_SPECTATOR)
		return;

	/* If we're substitute */
	if (client->pers.substitute)
		return;

	/* Fenix: If we are in a jumprun */
	if (client->sess.jumpRun)
		return;

	/* If the server does not allow to save/load position */
	if (g_allowPosSaving.integer < 1) {
		trap_SendServerCommand(client->ps.clientNum, "print \"Position saving is disabled on this server\n\"");
		return;
	}

	/* We're better disallowing saving while moving */
	if ( (client->ps.velocity[0] != 0) || (client->ps.velocity[1] != 0) || (client->ps.velocity[2] != 0) ) {
		trap_SendServerCommand(client->ps.clientNum, "print \"You can't save your position while moving\n\"");
		return;
	}

	/* Fenix: Disallow if dead */
	if (client->ps.pm_type != PM_NORMAL) {
		trap_SendServerCommand(client->ps.clientNum, "print \"You must be alive and in-game to save your position\n\"");
		return;
	}

	/* Disallow if crouched */
	if (client->ps.pm_flags & PMF_DUCKED) {
		trap_SendServerCommand(client->ps.clientNum, "print \"You cannot save your position while being crouched\n\"");
		return;
	}

	/* Disallow if not standing on a solid ground */
	if (client->ps.groundEntityNum != ENTITYNUM_WORLD) {
		trap_SendServerCommand(client->ps.clientNum, "print \"You must be standing on a solid ground to save your position\n\"");
		return;
	}

	/* Now save the position and the angles */
	VectorCopy(client->ps.origin, client->sess.savedLocation);
	VectorCopy(client->ps.viewangles, client->sess.savedLocationAngles);

	// Fenix: logging command execution.
	G_LogPrintf("ClientSavePosition: %d - %f - %f - %f\n",
				 client->ps.clientNum,
				 client->sess.savedLocation[0],
				 client->sess.savedLocation[1],
				 client->sess.savedLocation[2]);

	// Inform the client of the successful save.
	trap_SendServerCommand(client->ps.clientNum, "print \"Your position has been saved\n\"");

}

//@Barbatos - jump mode feature - load saved position
void Cmd_LoadPosition_f(gentity_t *entity) {

	gclient_t *client;

	client = entity->client;

	/* if we're not playing jump gametype */
	if(g_gametype.integer != GT_JUMP)
		return;

	/* If we're in spectator. */
	if (client->sess.sessionTeam == TEAM_SPECTATOR)
		return;

	/* If we're substitute */
	if (client->pers.substitute)
		return;

	/* Fenix: If we are in a jumprun */
	if (client->sess.jumpRun)
		return;

	/* If the server does not allow to save/load position */
	if (g_allowPosSaving.integer < 1) {
		trap_SendServerCommand(client->ps.clientNum, "print \"Position saving is disabled on this server\n\"");
		return;
	}

	/* If there is no position to load */
	if (!client->sess.savedLocation[0] || !client->sess.savedLocation[1] || !client->sess.savedLocation[2]) {
		trap_SendServerCommand(client->ps.clientNum, "print \"There is no position to load\n\"");
		return;
	}

	/* Fenix: Disallow if dead */
	if (client->ps.pm_type != PM_NORMAL) {
		trap_SendServerCommand(client->ps.clientNum, "print \"You must be alive and in-game to load your position\n\"");
		return;
	}

	/* Copy back saved position and the angles */
	VectorCopy(client->sess.savedLocation, client->ps.origin);
	SetClientViewAngle(entity, client->sess.savedLocationAngles);

	/* Don't move! */
	VectorClear(client->ps.velocity);

	/* Regenerate the stamina */
	client->ps.stats[STAT_STAMINA] = client->ps.stats[STAT_HEALTH] * UT_STAM_MUL;

	//-- @r00t: Following code is from TeleportPlayer() function:

	// toggle the teleport bit so the client knows to not lerp
	client->ps.eFlags ^= EF_TELEPORT_BIT;

	// save results of pmove
	BG_PlayerStateToEntityState( &client->ps, &entity->s, qtrue );

	// use the precise origin for linking
	VectorCopy( client->ps.origin, entity->r.currentOrigin );

	//NT - reset the origin trail for this player to avoid lerping in a time shift
	G_ResetTrail(entity);

	//-- end of code from TeleportPlayer()

	//@Fenix - logging command execution.
	G_LogPrintf("ClientLoadPosition: %d - %f - %f - %f\n",
				 client->ps.clientNum,
				 client->sess.savedLocation[0],
				 client->sess.savedLocation[1],
				 client->sess.savedLocation[2]);

	// Informing the client of the successful load.
	trap_SendServerCommand(client->ps.clientNum, "print \"Your position has been loaded\n\"");

}

//////////////////////////////////////////////////////////////////////////////////////////
// Name       : Cmd_RegainStamina_f
// Author     : Barbatos, Fenix
// Description: Refill stamina very quickly when standing still (Jump mode only)
//////////////////////////////////////////////////////////////////////////////////////////
void Cmd_RegainStamina_f(gentity_t *ent) {

    gclient_t *client;

    // Copy client reference
    client = ent->client;

    // Invalid gametype
    if (g_gametype.integer != GT_JUMP) {
        return;
    }

    // Can't use if spectator
    if (client->sess.sessionTeam == TEAM_SPECTATOR) {
        return;
    }

    // Can't use if substitute
    if (client->pers.substitute) {
        return;
    }

    // Can't use if in jump run
    if (client->sess.jumpRun) {
        return;
    }

    // If infinite stamina is enabled
    if (g_stamina.integer > 1) {
        return;
    }

    // Check if the feature is enabled on the server
    if (g_stamina.integer < 1) {
        trap_SendServerCommand(client->ps.clientNum, "print \"Stamina regeneration is disabled on this server\n\"");
        return;
    }

    if (client->sess.regainstamina) {
        client->sess.regainstamina = qfalse;
        trap_SendServerCommand(client->ps.clientNum, va("print \"Stamina regeneration turned %soff\n\"", S_COLOR_RED));
    } else {
        client->sess.regainstamina = qtrue;
        trap_SendServerCommand(client->ps.clientNum, va("print \"Stamina regeneration turned %son\n\"", S_COLOR_GREEN));
    }

}


//////////////////////////////////////////////////////////////////////////////////////////
// Name       : Cmd_AllowGoto_f
// Author     : Fenix
// Description: Toggle on and off personal goto
//////////////////////////////////////////////////////////////////////////////////////////
void Cmd_AllowGoto_f(gentity_t *ent) {

    gclient_t *client;

    // Copy client reference
    client = ent->client;

    // Invalid gametype
    if (g_gametype.integer != GT_JUMP) {
        return;
    }

    // Can't use if spectator
    if (client->sess.sessionTeam == TEAM_SPECTATOR) {
        return;
    }

    // Can't use if substitute
    if (client->pers.substitute) {
        return;
    }

    // Can't use if in jump run
    if (client->sess.jumpRun) {
        return;
    }

    // Check if the feature is enabled on the server
    if (g_allowGoto.integer < 1) {
        trap_SendServerCommand(client->ps.clientNum, "print \"Goto is disabled on this server\n\"");
        return;
    }

    if (client->sess.allowgoto) {
        client->sess.allowgoto = qfalse;
        trap_SendServerCommand(client->ps.clientNum, va("print \"Your personal goto has been turned %soff\n\"", S_COLOR_RED));
    } else {
        client->sess.allowgoto = qtrue;
        trap_SendServerCommand(client->ps.clientNum, va("print \"Your personal goto has been turned %son\n\"", S_COLOR_GREEN));
    }

}


//////////////////////////////////////////////////////////////////////////////////////////
// Name       : Cmd_Goto_f
// Author     : Fenix
// Description: Teleport a player to another one
//////////////////////////////////////////////////////////////////////////////////////////
void Cmd_Goto_f(gentity_t *ent) {

    gclient_t *client;
    gclient_t *other;
    char      str[MAX_TOKEN_CHARS];

    // Copy client reference
    client = ent->client;

    // Invalid gametype
    if (g_gametype.integer != GT_JUMP) {
        return;
    }

    // Can't use if spectator
    if (client->sess.sessionTeam == TEAM_SPECTATOR) {
        return;
    }

    // Can't use if substitute
    if (client->pers.substitute) {
        return;
    }

    // Can't use if in jump run
    if (client->sess.jumpRun) {
        return;
    }

    // Check if the feature is enabled on the server
    if (g_allowGoto.integer < 1) {
        trap_SendServerCommand(client->ps.clientNum, "print \"Goto is disabled on this server\n\"");
        return;
    }

    // Disallow if dead
    if (client->ps.pm_type != PM_NORMAL) {
        trap_SendServerCommand(client->ps.clientNum, "print \"You must be alive and in-game to use goto\n\"");
        return;
    }

    // We're better disallow goto while moving
    if ((client->ps.velocity[0] != 0) || (client->ps.velocity[1] != 0) || (client->ps.velocity[2] != 0)) {
        trap_SendServerCommand(client->ps.clientNum, "print \"You can't use goto while moving\n\"");
        return;
    }

    // Disallow if crouched
    if (client->ps.pm_flags & PMF_DUCKED) {
        trap_SendServerCommand(client->ps.clientNum, "print \"You cannot use goto while being crouched\n\"");
        return;
    }

    // Disallow if not standing on a solid ground
    if (client->ps.groundEntityNum != ENTITYNUM_WORLD) {
        trap_SendServerCommand(client->ps.clientNum, "print \"You must be standing on a solid ground to use goto\n\"");
        return;
    }

    // Wrong input
    if (trap_Argc() != 2) {
        trap_SendServerCommand(client->ps.clientNum, va("print \"Usage: goto <client>\n\""));
        return;
    }

    // Search the target client
    trap_Argv(1, str, sizeof(str));
    other = ClientForString(str);

    if (!other || other == client) {
        return;
    }

    // If target doesn't allow goto
    if (!other->sess.allowgoto) {
        trap_SendServerCommand(client->ps.clientNum, "print \"Target client doesn't allow goto\n\"");
        return;
    }

    // If target is a spectator
    if (other->sess.sessionTeam == TEAM_SPECTATOR) {
        trap_SendServerCommand(client->ps.clientNum, "print \"Target is currently in spectator mode\n\"");
        return;
    }

    // If target is dead
    if (other->ps.pm_type != PM_NORMAL) {
        trap_SendServerCommand(client->ps.clientNum, "print \"Target client is not alive or in-game\n\"");
        return;
    }

    // If target is in a jump run
    if (other->sess.jumpRun) {
        trap_SendServerCommand(client->ps.clientNum, "print \"Target client is currently doing a run\n\"");
        return;
    }

    // If target is moving
    if ((other->ps.velocity[0] != 0) || (other->ps.velocity[1] != 0) || (other->ps.velocity[2] != 0)) {
        trap_SendServerCommand(client->ps.clientNum, "print \"Target client is not standing still\n\"");
        return;
    }

    // If target is crouched
    if (other->ps.pm_flags & PMF_DUCKED) {
        trap_SendServerCommand(client->ps.clientNum, "print \"Target client is currently crouched\n\"");
        return;
    }

    // If target is not on a solid ground
    if (other->ps.groundEntityNum != ENTITYNUM_WORLD) {
        trap_SendServerCommand(client->ps.clientNum, "print \"Target client is not standing on a solid ground\n\"");
        return;
    }

    // Perform the goto
    VectorCopy(other->ps.origin, client->ps.origin);
    client->ps.stats[STAT_STAMINA] = client->ps.stats[STAT_HEALTH] * UT_STAM_MUL;

    // Logging command execution
    G_LogPrintf("ClientGoto: %d - %d - %f - %f - %f\n",
                    client->ps.clientNum,
                    other->ps.clientNum,
                    other->ps.origin[0],
                    other->ps.origin[1],
                    other->ps.origin[2]);

    // Informing both players on what happened
    trap_SendServerCommand(client->ps.clientNum, va("print \"Your have been teleported to %s\n\"", other->pers.netname));
    trap_SendServerCommand(other->ps.clientNum, va("print \"%s has been teleported to you\n\"", client->pers.netname));

}

void Cmd_HaxSync_f(gentity_t *ent)
//@r00t: Hax reporting
{
	unsigned int v1,v2;
	gclient_t *client;
	char tmp[16];
	int argc = trap_Argc();
	if (argc != 3) return;

	trap_Argv(1, tmp, sizeof(tmp));
	v1 = atoi(tmp);
	trap_Argv(2, tmp, sizeof(tmp));
	v2 = atoi(tmp);

	client = ent->client;
	client->sess.hax_report[0] = v1;
	client->sess.hax_report[1] = v2;
	client->sess.hax_report[2] = 0; // data age counter
	client->sess.hax_report_time = level.time;

#ifdef HAX_DEBUG
 // DEBUGGING ONLY - outputs parsed info to server log
{
 	unsigned int ov1,ov2;
 	unsigned int scr;
 	unsigned char temp2[8];

 ov1 = v1;
 ov2 = v2;

 scr = 0xB7E7B3E6^(v1&0xFF);
 scr^=scr<<9;
 scr^=scr>>5;
 scr^=scr<<1;
 v1^=scr;
 scr^=scr<<9;
 scr^=scr>>5;
 scr^=scr<<1;
 v2^=scr;

 temp2[0]=v1>>8;
 temp2[1]=v1>>16;
 temp2[2]=v1>>24;
 temp2[3]=v2;
 temp2[4]=v2>>8;
 temp2[5]=v2>>16;
 temp2[6]=v2>>24;

 if (((temp2[0]^temp2[1]^temp2[2]^temp2[3]^temp2[4]^temp2[5])&0xFF) != temp2[6]) return;
 G_LogPrintf("HAXINFO: %s = %d %d = %d %d %d | %d %d %d\n",
  ent->client->pers.netname,ov1,ov2,temp2[0],temp2[1],temp2[2],temp2[3],temp2[4],temp2[5]);
 }
#endif
}


/**
 * ClientCommand
 */
void ClientCommand(int clientNum) {

    gentity_t  *ent;
    char        cmd[MAX_TOKEN_CHARS];

    ent = g_entities + clientNum;

    if (!ent->client) {
        return;
    }

    trap_Argv(0, cmd, sizeof(cmd));

    if (Q_stricmp(cmd, "reflogin") == 0) {
        Cmd_RefLogin_f(ent);
    } else if (Q_stricmp(cmd, "ref") == 0) {
        Cmd_Ref_f(ent);
    } else if (Q_stricmp(cmd, "say") == 0) {
        Cmd_Say_f(ent, SAY_ALL, qfalse);
    } else if (Q_stricmp(cmd, "say_team") == 0) {
        Cmd_Say_f(ent, SAY_TEAM, qfalse);
    } else if (Q_stricmp(cmd, "tell") == 0) {
        Cmd_Tell_f(ent);
    } else if (Q_stricmp(cmd, "vsay") == 0) {
        Cmd_Voice_f(ent, SAY_ALL, qfalse, qfalse);
    } else if (Q_stricmp(cmd, "vsay_team") == 0) {
        Cmd_Voice_f(ent, SAY_TEAM, qfalse, qfalse);
    } else if (Q_stricmp(cmd, "vosay") == 0) {
        Cmd_Voice_f(ent, SAY_ALL, qfalse, qtrue);
    } else if (Q_stricmp(cmd, "vosay_team") == 0) {
        Cmd_Voice_f(ent, SAY_TEAM, qfalse, qtrue);
    } else if (Q_stricmp(cmd, "votell") == 0) {
        Cmd_VoiceTell_f(ent, qtrue);
    } else if (Q_stricmp(cmd, "vtaunt") == 0) {
        Cmd_VoiceTaunt_f(ent);
    } else if (Q_stricmp(cmd, "score") == 0) {
        Cmd_Score_f(ent);
    } else if (Q_stricmp(cmd, "give") == 0) {
        Cmd_Give_f(ent);
    } else if (Q_stricmp(cmd, "forcesub") == 0) {
        Cmd_ForceSub_f(ent);
    } else if (Q_stricmp(cmd, "ignore") == 0) {
        Cmd_Ignore_f(ent);
    } else if (Q_stricmp(cmd, "ignorelist") == 0) {
        Cmd_IgnoreList_f(ent);
    //} else if (Q_stricmp(cmd, "maplist") == 0) {
    //    Cmd_MapList_f(ent);
    } else if (Q_stricmp(cmd, "god") == 0) {
        Cmd_God_f(ent);
    } else if (Q_stricmp(cmd, "notarget") == 0) {
        Cmd_Notarget_f(ent);
    } else if (Q_stricmp(cmd, "noclip") == 0) {
        Cmd_Noclip_f(ent);
    } else if (Q_stricmp(cmd, "kill") == 0) {
        Cmd_Kill_f(ent);
    } else if (Q_stricmp(cmd, "teamtask") == 0) {
        Cmd_TeamTask_f(ent);
    } else if (Q_stricmp(cmd, "levelshot") == 0) {
        Cmd_LevelShot_f(ent);
    } else if (Q_stricmp(cmd, "follow") == 0) {
        Cmd_Follow_f(ent);
    } else if (Q_stricmp(cmd, "follownext") == 0) {
        Cmd_FollowCycle_f(ent, 1);
    } else if (Q_stricmp(cmd, "followprev") == 0) {
        Cmd_FollowCycle_f(ent, -1);
    } else if (Q_stricmp(cmd, "team") == 0) {
        Cmd_Team_f(ent);
    } else if (Q_stricmp(cmd, "where") == 0) {
        Cmd_Where_f(ent);
    } else if (Q_stricmp(cmd, "callvote") == 0) {
        Cmd_CallVote_f(ent);
    } else if (Q_stricmp(cmd, "vote") == 0) {
        Cmd_Vote_f(ent);
    } else if (Q_stricmp(cmd, "setviewpos") == 0) {
        Cmd_SetViewpos_f(ent);
    } else if (Q_stricmp(cmd, "stats") == 0) {
        Cmd_Stats_f(ent);
    } else if (Q_stricmp(cmd, "ut_itemdrop") == 0) {
        Cmd_DropItem_f(ent);
    } else if (Q_stricmp(cmd, "ut_itemuse") == 0) {
        Cmd_UseItem_f(ent);
    } else if (Q_stricmp(cmd, "ut_weapdrop") == 0) {
        Cmd_DropWeapon_f(ent);
    } else if (Q_stricmp(cmd, "ut_radio") == 0) {
        Cmd_Radio_f(ent);
    } else if (Q_stricmp(cmd, "ut_interface") == 0) {
        Cmd_Interface_f(ent);
    } else if (Q_stricmp(cmd, "sub") == 0) {
        Cmd_Substitute_f(ent);
    } else if (Q_stricmp(cmd, "captain") == 0) {
        Cmd_Captain_f(ent);
    } else if (Q_stricmp(cmd, "ready") == 0) {
        Cmd_Ready_f(ent);
    } else if (Q_stricmp(cmd, "teamname") == 0) {
        Cmd_TeamName_f(ent);
    } else if (Q_stricmp(cmd, "timeout") == 0) {
        Cmd_Timeout_f(ent);
    } else if (Q_stricmp(cmd, "forceanim") == 0) {
        Cmd_ForceAnim_f(ent);
    } else if ( (Q_stricmp(cmd, "savePos") == 0) || (Q_stricmp(cmd, "save") == 0)) {
        Cmd_SavePosition_f(ent);
    } else if ( (Q_stricmp(cmd, "loadPos") == 0) || (Q_stricmp(cmd, "load") == 0) ) {
        Cmd_LoadPosition_f(ent);
    } else if (Q_stricmp(cmd, "regainstamina") == 0) {
        Cmd_RegainStamina_f(ent);
    } else if (Q_stricmp(cmd, "goto") == 0) {
        Cmd_Goto_f(ent);
    } else if (Q_stricmp(cmd, "allowgoto") == 0) {
        Cmd_AllowGoto_f(ent);
    } else if (Q_stricmp(cmd, "tsync") == 0) { //@r00t: hax status reporting
        Cmd_HaxSync_f(ent);
    } else {
        trap_SendServerCommand(clientNum, va("print \"Unknown command: %s\n\"", cmd));
    }

}
