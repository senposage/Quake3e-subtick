/**
 * Filename: g_session.c
 *
 * Copyright (C) 1999-2005 Id Software, Inc.
 * Copyright (C) 2000-2009 FrozenSand
 *
 * This file is part of Urban Terror source code.
 *
 * This file contains routines to maintain session data between rounds.
 *
 * TODO: Implement new spawning model - don't destroy any entities that
 * don't actually require a respawn, e.g. like minimap entities etc.. spawns
 * lifts etc still require a respawn.
 */

#include "g_local.h"
/* Extern functions and variables */
qboolean G_CallSpawn(gentity_t *ent);
extern int	TSnumGroups;


//@r00t: Persistant positions saving/loading
void G_LoadPositionFromFile(gclient_t *client)
{
	fileHandle_t  f;
	int len;
	float pos[6];
	char tmp[MAX_INFO_STRING];
	char mapname[64];
	const char *guid;
	const char *xx;
	
	// Fenix: skip if not jump mode
	if (g_gametype.integer != GT_JUMP) {
	    return;
	}

	trap_GetUserinfo(client->ps.clientNum, tmp, sizeof(tmp));
	guid = Info_ValueForKey(tmp, "cl_guid");
	if (!guid || !guid[0] || (g_persistentPositions.integer<1)) return; // no GUID? no position saving for you!
	trap_Cvar_VariableStringBuffer("mapname",mapname,sizeof(mapname));
	xx = va("positions/%s/%s.pos",mapname,guid);
	len = trap_FS_FOpenFile(xx,&f,FS_READ);

	if (f) {
		if (len>0 && len<sizeof(tmp)-1) {
			trap_FS_Read(tmp,len,f);
			tmp[len]=0;
			if (sscanf(tmp, "%f,%f,%f,%f,%f,%f",pos,pos+1,pos+2,pos+3,pos+4,pos+5)==6) {
				client->sess.savedLocation[0] = pos[0];
				client->sess.savedLocation[1] = pos[1];
				client->sess.savedLocation[2] = pos[2];
				client->sess.savedLocationAngles[0] = pos[3];
				client->sess.savedLocationAngles[1] = pos[4];
				client->sess.savedLocationAngles[2] = pos[5];
			}
		}
		trap_FS_FCloseFile(f);
	}

}

void G_SavePositionToFile(gclient_t *client)
{
	fileHandle_t  f;
	int len;
	char tmp[MAX_INFO_STRING];
	char mapname[64];
	const char *guid;

	// Fenix: skip if not jump mode
    if (g_gametype.integer != GT_JUMP) {
        return;
    }

	trap_GetUserinfo(client->ps.clientNum, tmp, sizeof(tmp));
	guid = Info_ValueForKey(tmp, "cl_guid");
	if ((g_persistentPositions.integer < 1) || !guid[0] || !guid ||
		(!client->sess.savedLocation[0] && !client->sess.savedLocation[1] && !client->sess.savedLocation[2])) return;

	trap_Cvar_VariableStringBuffer("mapname",mapname,sizeof(mapname));
	len = trap_FS_FOpenFile(va("positions/%s/%s.pos",mapname,guid),&f,FS_WRITE );

	if (f) {
		len = Com_sprintf(tmp,sizeof(tmp)-1,"%f,%f,%f,%f,%f,%f\n%s",
			client->sess.savedLocation[0],client->sess.savedLocation[1],client->sess.savedLocation[2],
			client->sess.savedLocationAngles[0],client->sess.savedLocationAngles[1],client->sess.savedLocationAngles[2],
			client->pers.netname);
		trap_FS_Write(tmp,len,f);
		trap_FS_FCloseFile(f);
	}
}


/**
 * G_InitClientSessionData
 */
void G_InitClientSessionData(gclient_t *client)
{
	trap_Cvar_Set(va("session%i", client - level.clients), "0");
}

/**
 * G_WriteClientSessionData
 */
void G_WriteClientSessionData(gclient_t *client) {

    char        *s;
    const char  *var;

    s = va("%i,%i,%i,%i,%i,%i",
           client->sess.sessionTeam,
           client->sess.connectTime,
           client->sess.lastFailedVote,
           client->sess.inactivityTime,
           (client->sess.regainstamina) ? 1 : 0,
           (client->sess.allowgoto) ? 1 : 0);

    var = va("session%i", client - level.clients);
    trap_Cvar_Set(var, s);
}

/**
 * G_ReadSessionData
 */
void G_ReadSessionData(gclient_t *client) {

    char        s[MAX_STRING_CHARS];
    const char  *var;
    int         team;
    int         regainstamina;
    int         allowgoto;

    var = va("session%i", client - level.clients);
    trap_Cvar_VariableStringBuffer(var, s, sizeof(s));
    sscanf(s, "%i,%i,%i,%i,%i,%i", &team,
                                   &client->sess.connectTime,
                                   &client->sess.lastFailedVote,
                                   &client->sess.inactivityTime,
                                   &regainstamina,
                                   &allowgoto);

    client->sess.sessionTeam = team;
    client->sess.regainstamina = regainstamina > 0 ? qtrue : qfalse;
    client->sess.allowgoto = allowgoto > 0 ? qtrue : qfalse;

    G_LoadPositionFromFile(client);

}

/**
 * G_InitSessionData
 */
void G_InitSessionData(gclient_t *client, char *userinfo) {

	clientSession_t  *sess;
	const char		 *value;

	sess = &client->sess;

	/* initial team determination */
	if ((g_gametype.integer >= GT_TEAM) && (g_gametype.integer != GT_JUMP)) {

	    if (!sess->sessionTeam) {
			sess->sessionTeam = TEAM_SPECTATOR;
		}

	} else {

		value = Info_ValueForKey(userinfo, "team");

		if (value[0] == 's') {
			/* a willing spectator, not a waiting-in-line */
			sess->sessionTeam = TEAM_SPECTATOR;
		} else {
			if ((g_maxGameClients.integer > 0) && (level.numNonSpectatorClients >= g_maxGameClients.integer)) {
				sess->sessionTeam = TEAM_SPECTATOR;
			} else {
				sess->sessionTeam = TEAM_FREE;
			}
		}
	}

	sess->spectatorState = SPECTATOR_FREE;
	sess->spectatorTime  = level.time;

	if (g_inactivity.integer) {
		sess->inactivityTime = level.time + g_inactivity.integer * 1000;
	} else {
		sess->inactivityTime = level.time + 60 * 1000;
	}

	sess->lastFailedVote = 0;
	sess->connectTime	 = trap_Milliseconds();

    #ifdef USE_AUTH
    //@Barbatos //@Kalish
    sess->renames = 0;
	Q_strncpyz(sess->lastname, " ", sizeof(sess->lastname));
    sess->welcome = 0;
    sess->dropreason[0] = '\0';
    sess->dropmessage[0] = '\0';
    sess->droptime = 0;
    sess->auth_name[0] = '\0';
    sess->auth_login[0] = '\0';
    sess->auth_notoriety[0] = '\0';
    sess->auth_level = 0;
    #endif

    //Barbatos, Fenix: jump mode
    sess->regainstamina = qfalse;
    sess->allowgoto = qfalse;

    //@r00t: Hax reporting
    sess->hax_report[0]=0;
    sess->hax_report[1]=0;
    sess->hax_report[2]=0;
    sess->hax_report[3]=0;
    sess->hax_report[4]=0;
    sess->hax_report[5]=0;
    sess->hax_report_time=0;
    sess->ip[0]=0; // ip
    memset(sess->guid_bin,0,32);

	G_LoadPositionFromFile(client);
	G_WriteClientSessionData(client);

	G_LogPrintf("Session data initialised for %s at %i\n", client->pers.netname, sess->connectTime);
}

/**
 * G_DoMoveOutCall
 *
 * If the player has the bomb, he automatically calls out
 */
//Fenix: refactored whole function
void G_DoMoveOutCall(void) {

	int i;
	gclient_t *cl;
	gentity_t *ent;

	// We are not playing BOMB
	if (g_gametype.integer != GT_UT_BOMB) {
		return;
	}

	for (i = 0; i < level.maxclients; i++) {

		cl = &level.clients[i];

		// Client not connected anymore.
		if (cl->pers.connected != CON_CONNECTED) {
			continue;
		}

		// The client is not in the RED team.
		if (cl->sess.sessionTeam != TEAM_RED) {
			continue;
		}

		// The client doesn't carry the bomb.
		if (utPSGetWeaponByID(&cl->ps, UT_WP_BOMB) == -1) {
			continue;
		}

		// If we got here means that we have found the player who is carrying the bomb.
		// Let's inform other team members with an autoradio message.
		ent = &g_entities[cl->ps.clientNum];

		switch (rand() % 3) {

			case 0: /* Move out */
					G_AutoRadio(10, ent);
					break;

			case 1: /* Cover me */
					G_AutoRadio(11, ent);
					break;

			case 2: /* I have the bomb */
					G_AutoRadio(3, ent);
					break;
		}

		// Exit the for loop.
		break;

	}
}


/**
 * G_InitWorldSession
 *
 * Re-initialises the world in between rounds
 */
void G_InitWorldSession(void)
{
	gentity_t  *ent;
	vec3_t	   vec;
	int 	   i, j;
	int 	   teamLeader;

	char	   serverinfo[MAX_INFO_STRING];

	trap_GetServerinfo(serverinfo, sizeof(serverinfo));

	G_LogPrintf("------------------------------------------------------------\n");
	G_LogPrintf("InitRound: %s\n", serverinfo);

	if (g_survivor.integer) {
		trap_SetConfigstring(CS_ROUNDWINNER, "");
	}

	// reset pause timers
	if (g_gametype.integer == GT_CTF) {
		level.pausedTime = 0;
		trap_SetConfigstring(CS_PAUSE_STATE, va("%d %i %i", level.pauseState, level.pauseTime, level.pausedTime));
	}

	level.intermissiontime		   = 0;
	level.exitTime				   = 0;
	level.intermissionQueued	   = qfalse;
	level.readyToExit			   = qfalse;
	level.survivorBlueDeaths	   = 0;
	level.survivorRedDeaths 	   = 0;
	level.survivorRoundRestartTime = 0;
	level.goalindex 			   = 0;
	level.redCapturePointsHeld	   = 0;
	level.blueCapturePointsHeld    = 0;
	level.NumBombSites			   = 0; /* for bomb mode */
	level.BombGoneOff			   = qfalse;
	level.BombDefused			   = qfalse;
	level.BombPlanted			   = qfalse;
	VectorClear(level.BombPlantOrigin);
	VectorClear(level.BombExplosionOrigin);
	level.BombExplosionTime 	   = 0;
	level.BombExplosionCheckTime   = 0;
	level.Bomber				   = 0;

	/* Smart entity destory to be added */
	for (i = MAX_CLIENTS; i < MAX_GENTITIES; i++) {
		ent = &g_entities[i];
		ent->neverFree = qfalse;
		G_FreeEntity(ent);
	}

	level.num_entities = MAX_CLIENTS;
	/* let the server system know where the entites are */
	trap_LocateGameData(level.gentities, level.num_entities,
				sizeof(gentity_t), &level.clients[0].ps,
				sizeof(level.clients[0]));
	level.spawning		 = qtrue;  /* so we can use g_spawn(), and others */
	level.RoundRespawn	 = qtrue;  /* use round respawning functions */
	level.locationLinked = qfalse; /* for locations, to relink them */
	for (i = 0, cur_levelspawn = 0; i < num_levelspawn;
			i++, cur_levelspawn++) {
		gentity_t  *NewEnt;

		if (i == ENTITYNUM_WORLD) {
			continue;
		}
		NewEnt = G_Spawn();
		G_ParseSpawnFields(NewEnt, cur_levelspawn);

		if ((g_gametype.integer >= GT_TEAM) && (g_gametype.integer != GT_JUMP)) {
			G_SpawnInt("notteam", "0", &j);

			if (j) {
				G_FreeEntity(NewEnt);
				continue;
			}
		} else {
			G_SpawnInt("notfree", "0", &j);

			if (j) {
				G_FreeEntity(NewEnt);
				continue;
			}
		}
		VectorCopy(NewEnt->s.origin, NewEnt->s.pos.trBase);
		VectorCopy(NewEnt->s.origin, NewEnt->r.currentOrigin);

		//@Fenix - remove flags from the level if we are not playing CTF of JUMP mode
		if ((g_gametype.integer != GT_CTF) && (g_gametype.integer != GT_JUMP)) {
//					  if (NewEnt->classname && !Q_stricmp(NewEnt->classname, "team_CTF_redflag")) {
					if (NewEnt->classname && NewEnt->classhash==HSH_team_CTF_redflag) {
						G_FreeEntity(NewEnt);
//					  } else if (NewEnt->classname && !Q_stricmp(NewEnt->classname, "team_CTF_blueflag")) {
					} else if (NewEnt->classname && NewEnt->classhash==HSH_team_CTF_blueflag) {
						G_FreeEntity(NewEnt);
					}
		}

		//@Fenix - spawn flags on the level if we are playing CTF or JUMP mode
		if ((g_gametype.integer == GT_CTF) || (g_gametype.integer == GT_JUMP)) {
//					  if (NewEnt->classname && !Q_stricmp(NewEnt->classname, "team_CTF_redflag")) {
					if (NewEnt->classname && NewEnt->classhash==HSH_team_CTF_redflag) {
						NewEnt->r.svFlags |= SVF_BROADCAST;
//					  } else if (NewEnt->classname && !Q_stricmp(NewEnt->classname, "team_CTF_blueflag")) {
					} else if (NewEnt->classname && NewEnt->classhash==HSH_team_CTF_blueflag) {
						NewEnt->r.svFlags |= SVF_BROADCAST;
					}
		}

		if (!G_CallSpawn(NewEnt)) {
			G_FreeEntity(NewEnt);

		}
	}
	level.spawning	   = qfalse; /* we are done. */
	level.RoundRespawn = qfalse; /* false it after use! */
	TSnumGroups    = 0;
	G_FindTeams();			  /* interlinks all entities */
	G_InitUTSpawnPoints(g_gametype.integer);

	for (i = 0; i < level.maxclients; i++) {
		gclient_t  *cl;
		gentity_t  *gt;

		cl = &level.clients[i];

		if (cl->pers.connected != CON_CONNECTED) {
			continue;
		}

		if ((cl->sess.sessionTeam == TEAM_SPECTATOR) &&
				(cl->ps.pm_flags & PMF_FOLLOW)) {
			continue;
		}

		if (cl->pers.substitute) {
			continue;
		}

		if (cl->ps.clientNum != i) {
			cl->ps.clientNum = cl->savedclientNum;
		}
		cl->ghost		= qfalse;
		cl->sess.bombHolder = qfalse;

		if (cl->sess.sessionTeam != level.survivorWinTeam) {
			cl->sess.teamLeader = qfalse;
		}
		gt = &g_entities[i];
		ClientSpawn(gt);

		/* FIXME: This could be giving us insane lag!!!??? */
		if (g_gametype.integer == GT_UT_ASSASIN) {
			ClientUserinfoChanged(i);
		}
	}

	if ((g_gametype.integer >= GT_TEAM) && (g_gametype.integer != GT_JUMP)){
		G_CheckTeamItems();
	}

	if (g_gametype.integer == GT_UT_ASSASIN) {
		if (level.survivorWinTeam == TEAM_RED) {
			CheckTeamLeader(TEAM_BLUE);
		} else if (level.survivorWinTeam == TEAM_BLUE) {
			CheckTeamLeader(TEAM_RED);
		} else {
			CheckTeamLeader(TEAM_RED);
			CheckTeamLeader(TEAM_BLUE);
		}
		teamLeader = TeamLeader(level.survivorWinTeam);

		if (teamLeader == -1) {  /* winning team has no leader */
			CheckTeamLeader(level.survivorWinTeam);
		} else { /* Gleam's silly printout */
			gclient_t  *cl;
			cl			= &level.clients[teamLeader];
			cl->sess.teamLeader = qfalse;
			SetLeader(level.survivorWinTeam, teamLeader);
			/* FIXME: This could be giving us insane lag!!!??? */
			ClientUserinfoChanged(teamLeader);
		}
	}
	level.survivorWinTeam = 0;

	for (i = 0; i < level.maxclients; i++) {
		gclient_t  *client;

		client = &level.clients[i];

		if (client->pers.connected != CON_CONNECTED) {
			continue;
		}
		MapArrowSpawn(client->ps.clientNum);

		if ((g_gametype.integer == GT_UT_BOMB) &&
				(client->sess.bombHolder == qtrue)) {
			ClientUserinfoChanged(i);
		}
	}
	SendScoreboardMessageToAllClients();
	G_InitUTBotItemGoals();
	vec[0]			= 0;
	vec[1]			= 0;
	vec[2]			= 0;
	ent 			= G_TempEntity(vec, EV_UT_CLEARMARKS);
	ent->r.svFlags	= SVF_BROADCAST;

	level.domoveoutcalltime = level.time + 500;
	level.roundEndTime	= level.time +
				  (int)(g_RoundTime.value * 60 * 1000);
	level.g_roundcount++;
	trap_SetConfigstring(CS_LEVEL_START_TIME,
				 va("%i", level.startTime));
	trap_SetConfigstring(CS_ROUND_END_TIME,
				 va("%i", level.roundEndTime));
	trap_SetConfigstring(CS_ROUNDCOUNT,
				 va("%i", level.g_roundcount));
	trap_SetConfigstring(CS_BOMBPLANTTIME,
				 va("%i", g_bombPlantTime.integer));
	bg_weaponlist[UT_WP_BOMB].modes[0].fireTime =
		g_bombPlantTime.integer * 1000;
}

/**
 * G_WriteSessionData
 */
void G_WriteSessionData(void)
{
	int  i;

	for (i = 0 ; i < level.maxclients ; i++) {
		if (level.clients[i].pers.connected == CON_CONNECTED) {
			G_SavePositionToFile(&level.clients[i]);
			G_WriteClientSessionData(&level.clients[i]);
		}
	}
}
