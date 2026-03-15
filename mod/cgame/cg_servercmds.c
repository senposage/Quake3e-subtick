// Copyright (C) 1999-2000 Id Software, Inc.
//
// cg_servercmds.c -- reliably sequenced text commands sent by the server
// these are processed at snapshot transition time, so there will definately
// be a valid snapshot this frame

#include "cg_local.h"
#include "../ui/menudef.h"

typedef struct
{
	const char	*order;
	int 	taskNum;
} orderTask_t;

/*
static const orderTask_t  validOrders[] = {
	{ VOICECHAT_GETFLAG,		   TEAMTASK_OFFENSE  },
	{ VOICECHAT_OFFENSE,		   TEAMTASK_OFFENSE  },
	{ VOICECHAT_DEFEND, 		   TEAMTASK_DEFENSE  },
	{ VOICECHAT_DEFENDFLAG, 	   TEAMTASK_DEFENSE  },
	{ VOICECHAT_PATROL, 		   TEAMTASK_PATROL	 },
	{ VOICECHAT_CAMP,			   TEAMTASK_CAMP	 },
	{ VOICECHAT_FOLLOWME,		   TEAMTASK_FOLLOW	 },
	{ VOICECHAT_RETURNFLAG, 	   TEAMTASK_RETRIEVE },
	{ VOICECHAT_FOLLOWFLAGCARRIER, TEAMTASK_ESCORT	 }
};

static const int	  numValidOrders = sizeof(validOrders) / sizeof(orderTask_t);
*/

/**
 * $(function)
 */
#if 0
static int CG_ValidOrder(const char *p)
{
	int  i;

	for (i = 0; i < numValidOrders; i++)
	{
		if (Q_stricmp(p, validOrders[i].order) == 0)
		{
			return validOrders[i].taskNum;
		}
	}
	return -1;
}
#endif

/*
=================
CG_ParseScores

=================
*/
static void CG_ParseScores( void ) {

	int  i;
	int  status;
	//int  powerups;
	int  jumprun;

	cg.numScores = atoi( CG_Argv( 1 ));

	if (cg.numScores > MAX_CLIENTS) {
		cg.numScores = MAX_CLIENTS;
	}

	cg.teamScores[0] = atoi( CG_Argv( 2 ));
	cg.teamScores[1] = atoi( CG_Argv( 3 ));

	memset( cg.scores, 0, sizeof(cg.scores));

	for (i = 0 ; i < cg.numScores ; i++) {

		//Get the scores
		cg.scores[i].client 	    = atoi( CG_Argv( i * 11 + 4  ));
		cg.scores[i].score		    = atoi( CG_Argv( i * 11 + 5  ));
		cg.scores[i].ping		    = atoi( CG_Argv( i * 11 + 6  ));
		cg.scores[i].time		    = atoi( CG_Argv( i * 11 + 7  ));
		status				        = atoi( CG_Argv( i * 11 + 8  ));
		cg.scores[i].deaths 	    = atoi( CG_Argv( i * 11 + 9  ));
		jumprun 			        = atoi( CG_Argv( i * 11 + 10 )); //@Fenix
		cg.scores[i].jumpWay		= atoi( CG_Argv( i * 11 + 11 )); //@r00t:JumpWayColors
		cg.scores[i].jumpBestWay	= atoi( CG_Argv( i * 11 + 12 )); //@r00t:JumpWayColors
		cg.scores[i].jumpBestTime	= atoi( CG_Argv( i * 11 + 13 )); //@Fenix

		//Safe string copy which ensure trailing \0
		Q_strncpyz(cg.scores[i].account, CG_Argv( i * 11 + 14), sizeof(cg.scores[i].account)); //@Barbatos //@Fenix

		//cg.scores[i].scoreFlags = atoi( CG_Argv( i * 15 + 8 ) );
		//powerups = atoi( CG_Argv( i * 15 + 9 ) );
		//cg.scores[i].accuracy = atoi(CG_Argv(i * 15 + 10));
		//cg.scores[i].impressiveCount = atoi(CG_Argv(i * 15 + 11));
		//cg.scores[i].excellentCount = atoi(CG_Argv(i * 15 + 12));
		//cg.scores[i].guantletCount = atoi(CG_Argv(i * 15 + 13));
		//cg.scores[i].defendCount = atoi(CG_Argv(i * 15 + 14));
		//cg.scores[i].assistCount = atoi(CG_Argv(i * 15 + 15));
		//cg.scores[i].perfect = atoi(CG_Argv(i * 15 + 16));
		//cg.scores[i].captures = atoi(CG_Argv(i * 15 + 17));

		if (status == 2) {
			//if the player is a substitute.
			cgs.clientinfo[cg.scores[i].client].substitute = qtrue;
			cgs.clientinfo[cg.scores[i].client].ghost	   = qtrue;

			if (!CG_ENTITIES(cg.scores[i].client)->currentValid) {
				//if we're not currently valid (out of pvs) update the dead flag status
				CG_ENTITIES(cg.scores[i].client)->currentState.eFlags |= EF_DEAD;
			}
		}
		else if (status == 1) {
			//if the player is dead
			cgs.clientinfo[cg.scores[i].client].substitute = qfalse;
			cgs.clientinfo[cg.scores[i].client].ghost	   = qtrue;

			if (!CG_ENTITIES(cg.scores[i].client)->currentValid) {
				//if we're not currently valid (out of pvs) update the dead flag status
				CG_ENTITIES(cg.scores[i].client)->currentState.eFlags |= EF_DEAD;
			}
		}
		else {
			//normal status
			cgs.clientinfo[cg.scores[i].client].substitute = qfalse;
			cgs.clientinfo[cg.scores[i].client].ghost	   = qfalse;

			if (!CG_ENTITIES(cg.scores[i].client)->currentValid) {
				//if we're not currently valid (out of pvs) update the dead flag status
				CG_ENTITIES(cg.scores[i].client)->currentState.eFlags &= ~EF_DEAD;
			}
		}

		//Check correct slot value
		if ((cg.scores[i].client < 0) || (cg.scores[i].client >= MAX_CLIENTS)) {
			cg.scores[i].client = 0;
		}

		//Set the team in score_t using the clientInfo_t value
		cg.scores[i].team = cgs.clientinfo[cg.scores[i].client].team;

		//Copy values in clientInfo_t
		cgs.clientinfo[cg.scores[i].client].jumprun 	    = (jumprun == 1) ? qtrue : qfalse;
		cgs.clientinfo[cg.scores[i].client].score			= cg.scores[i].score;
		cgs.clientinfo[cg.scores[i].client].deaths			= cg.scores[i].deaths;
		//cgs.clientinfo[ cg.scores[i].client ].powerups	= powerups;
		cgs.clientinfo[cg.scores[i].client].jumpBestWay 	= cg.scores[i].jumpBestWay; //@r00t:JumpWayColors
		cgs.clientinfo[cg.scores[i].client].jumpWay 	    = cg.scores[i].jumpWay; 	//@r00t:JumpWayColors
		cgs.clientinfo[cg.scores[i].client].jumpBestTime	= cg.scores[i].jumpBestTime;

		if (cgs.gametype == GT_JUMP && !cgs.clientinfo[cg.scores[i].client].jumprun)
			cgs.clientinfo[cg.scores[i].client].jumpStartTime = 0;

#ifdef USE_AUTH
		//Safe string copy which ensure trailing \0
		Q_strncpyz(cgs.clientinfo[cg.scores[i].client].auth_login, cg.scores[i].account, sizeof(cgs.clientinfo[cg.scores[i].client].auth_login));
#endif

		//if we are playing jump and the timer is deactivated make

	}

#ifdef MISSIONPACK
	CG_SetScoreSelection(NULL);
#endif
}

/**
 * $(function)
 */
static void CG_ParseScoresSingle(void) {

	score_t  score;
	int  clientNum, slotNum;
	int  status;
	int 	 jumprun;
	int  i, j;

	// Get client number.
	clientNum = atoi(CG_Argv(1));

	// If there are no scores.
	if (cg.numScores < 1) {
		// Set to first slot.
		slotNum = 0;
		// Set scores number.
		cg.numScores = 1;
	}
	else {

		// Loop through scores.
		for (i = 0, slotNum = -1; i < cg.numScores; i++) {
			// If this is out client number.
			if (cg.scores[i].client == clientNum) {
				// Set slot number.
				slotNum = i;
				break;
			}
		}

		// If we didn't find a slot.
		if (slotNum == -1) {
			// Set slot number.
			slotNum = cg.numScores++;
		}
	}

	//Get the scores
	cg.scores[slotNum].client			= clientNum;
	cg.scores[slotNum].score			= atoi(CG_Argv(2));
	cg.scores[slotNum].ping 			= atoi(CG_Argv(3));
	cg.scores[slotNum].time 			= atoi(CG_Argv(4));
	status								= atoi(CG_Argv(5));
	cg.scores[slotNum].deaths			= atoi(CG_Argv(6));
	jumprun 							= atoi(CG_Argv(7));
	cg.scores[slotNum].jumpWay			= atoi(CG_Argv(8));  //@r00t:JumpWayColors
	cg.scores[slotNum].jumpBestWay		= atoi(CG_Argv(9));  //@r00t:JumpWayColors
	cg.scores[slotNum].jumpBestTime 	= atoi(CG_Argv(10));

	//Safe string copy which ensure trailing \0
	Q_strncpyz(cg.scores[slotNum].account, CG_Argv(11), sizeof(cg.scores[slotNum].account)); //@Barbatos //@Fenix

	if (status == 2) {
		//if the player is a substitute
		cgs.clientinfo[clientNum].substitute = qtrue;
		cgs.clientinfo[clientNum].ghost 	 = qtrue;

		if (!CG_ENTITIES(clientNum)->currentValid) {
			//if we're not currently valid (out of pvs) update the dead flag status
			CG_ENTITIES(clientNum)->currentState.eFlags |= EF_DEAD;
		}
	}
	else if (status == 1) {
		//if the player is dead
		cgs.clientinfo[clientNum].substitute = qfalse;
		cgs.clientinfo[clientNum].ghost 	 = qtrue;

		if (!CG_ENTITIES(clientNum)->currentValid) {
			//if we're not currently valid (out of pvs) update the dead flag status
			CG_ENTITIES(clientNum)->currentState.eFlags |= EF_DEAD;
		}
	}
	else {
		//normal status
		cgs.clientinfo[clientNum].substitute = qfalse;
		cgs.clientinfo[clientNum].ghost 	 = qfalse;

		if (!CG_ENTITIES(clientNum)->currentValid) {
			//if we're not currently valid (out of pvs) update the dead flag status
			CG_ENTITIES(clientNum)->currentState.eFlags &= ~EF_DEAD;
		}
	}

	//Set the team in score_t using the clientInfo_t value
	cg.scores[slotNum].team = cgs.clientinfo[clientNum].team;

	//Copy values in clientInfo_t
	cgs.clientinfo[clientNum].jumprun		= (jumprun == 1) ? qtrue : qfalse;
	cgs.clientinfo[clientNum].score 		= cg.scores[slotNum].score;
	cgs.clientinfo[clientNum].deaths		= cg.scores[slotNum].deaths;
	cgs.clientinfo[clientNum].jumpWay		= cg.scores[slotNum].jumpWay;	   //@r00t:JumpWayColors
	cgs.clientinfo[clientNum].jumpBestWay	= cg.scores[slotNum].jumpBestWay;  //@r00t:JumpWayColors
	cgs.clientinfo[clientNum].jumpBestTime	= cg.scores[slotNum].jumpBestTime;

	if (cgs.gametype == GT_JUMP && !cgs.clientinfo[clientNum].jumprun)
		cgs.clientinfo[clientNum].jumpStartTime = 0;

	//Sort scores
	for (i = 0; i < cg.numScores; i++) {
		for (j = 0; j < (cg.numScores - i) - 1; j++) {
			//@Fenix - different sorting method for jump mode -> order by best time ASC
			if (cgs.gametype == GT_JUMP) {
				//if next time is lower
				if (cg.scores[j].jumpBestTime > cg.scores[j + 1].jumpBestTime) {
					// Swap places.
					memcpy(&score, &cg.scores[j], sizeof(score_t));
					memcpy(&cg.scores[j], &cg.scores[j + 1], sizeof(score_t));
					memcpy(&cg.scores[j + 1], &score, sizeof(score_t));
				}
			} else {
				//if next score is higher
				if (cg.scores[j].score < cg.scores[j + 1].score) {
					// Swap places.
					memcpy(&score, &cg.scores[j], sizeof(score_t));
					memcpy(&cg.scores[j], &cg.scores[j + 1], sizeof(score_t));
					memcpy(&cg.scores[j + 1], &score, sizeof(score_t));
				}
			}
		}
	}

#ifdef MISSIONPACK
	CG_SetScoreSelection(NULL);
#endif
}

/**
 * $(function)
 */
static void CG_ParseScoresDouble(void) {

	score_t  score;
	int  clientNum, slotNum;
	int  jumprun;
	int  status;
	int  i, j;

	for (i = 0; i < 2; i++) {
		//Get client number
		clientNum = atoi(CG_Argv((i * 11) + 1));

		// If there are no scores.
		if (cg.numScores < 1) {
			// Set to first slot.
			slotNum = 0;
			// Set scores number.
			cg.numScores = 1;
		}
		else {
			// Loop through scores.
			for (j = 0, slotNum = -1; j < cg.numScores; j++) {
				// If this is out client number.
				if (cg.scores[j].client == clientNum){
					// Set slot number.
					slotNum = j;
					break;
				}
			}

			// If we didn't find a slot.
			if (slotNum == -1) {
				// Set slot number.
				slotNum = cg.numScores++;
			}
		}

		// Get scores.
		cg.scores[slotNum].client			= clientNum;
		cg.scores[slotNum].score			= atoi(CG_Argv((i * 11) + 2));
		cg.scores[slotNum].ping 			= atoi(CG_Argv((i * 11) + 3));
		cg.scores[slotNum].time 			= atoi(CG_Argv((i * 11) + 4));
		status								= atoi(CG_Argv((i * 11) + 5));
		cg.scores[slotNum].deaths			= atoi(CG_Argv((i * 11) + 6));
		jumprun 							= atoi(CG_Argv((i * 11) + 7));
		cg.scores[slotNum].jumpWay			= atoi(CG_Argv((i * 11) + 8)); //@r00t:JumpWayColors
		cg.scores[slotNum].jumpBestWay		= atoi(CG_Argv((i * 11) + 9)); //@r00t:JumpWayColors
		cg.scores[slotNum].jumpBestTime 	= atoi(CG_Argv((i * 11) +10));

		//Safe string copy which ensure trailing \0
		Q_strncpyz(cg.scores[slotNum].account, CG_Argv((i * 11) +11), sizeof(cg.scores[slotNum].account)); //@Barbatos //@Fenix

		if (status == 2) {
			//if the player is a substitute
			cgs.clientinfo[clientNum].substitute = qtrue;
			cgs.clientinfo[clientNum].ghost 	 = qtrue;

			if (!CG_ENTITIES(clientNum)->currentValid) {
				//if we're not currently valid (out of pvs) update the dead flag status
				CG_ENTITIES(clientNum)->currentState.eFlags |= EF_DEAD;
			}
		}
		else if (status == 1) {
			//if the player is dead
			cgs.clientinfo[clientNum].substitute = qfalse;
			cgs.clientinfo[clientNum].ghost 	 = qtrue;
			if (!CG_ENTITIES(clientNum)->currentValid) {
				//if we're not currently valid (out of pvs) update the dead flag status
				CG_ENTITIES(clientNum)->currentState.eFlags |= EF_DEAD;
			}
		}
		else {
			//normal status
			cgs.clientinfo[clientNum].substitute = qfalse;
			cgs.clientinfo[clientNum].ghost 	 = qfalse;

			if (!CG_ENTITIES(clientNum)->currentValid) {
				//if we're not currently valid (out of pvs) update the dead flag status
				CG_ENTITIES(clientNum)->currentState.eFlags &= ~EF_DEAD;
			}
		}

		//Set the team in score_t using the clientInfo_t value
		cg.scores[slotNum].team = cgs.clientinfo[clientNum].team;

		// Copy values in clientInfo_t
		cgs.clientinfo[clientNum].jumprun		= (jumprun == 1) ? qtrue : qfalse;
		cgs.clientinfo[clientNum].score 		= cg.scores[slotNum].score;
		cgs.clientinfo[clientNum].deaths		= cg.scores[slotNum].deaths;
		cgs.clientinfo[clientNum].jumpWay		= cg.scores[clientNum].jumpWay; 	 //@r00t:JumpWayColors
		cgs.clientinfo[clientNum].jumpBestWay	= cg.scores[clientNum].jumpBestWay;  //@r00t:JumpWayColors
		cgs.clientinfo[clientNum].jumpBestTime	= cg.scores[clientNum].jumpBestTime;

		if (cgs.gametype == GT_JUMP && !cgs.clientinfo[clientNum].jumprun)
			cgs.clientinfo[clientNum].jumpStartTime = 0;

	}

	//Sort scores
	for (i = 0; i < cg.numScores; i++) {
		for (j = 0; j < (cg.numScores - i) - 1; j++) {

			//@Fenix - different sorting method for jump mode -> order by best time ASC
			if (cgs.gametype == GT_JUMP) {
				// If next time is lower
				if (cg.scores[j].jumpBestTime > cg.scores[j + 1].jumpBestTime) {
					// Swap places.
					memcpy(&score, &cg.scores[j], sizeof(score_t));
					memcpy(&cg.scores[j], &cg.scores[j + 1], sizeof(score_t));
					memcpy(&cg.scores[j + 1], &score, sizeof(score_t));
				}
			} else {
				// If next score is higher.
				if (cg.scores[j].score < cg.scores[j + 1].score) {
					// Swap places.
					memcpy(&score, &cg.scores[j], sizeof(score_t));
					memcpy(&cg.scores[j], &cg.scores[j + 1], sizeof(score_t));
					memcpy(&cg.scores[j + 1], &score, sizeof(score_t));
				}
			}
		}
	}

#ifdef MISSIONPACK
	CG_SetScoreSelection(NULL);
#endif
}

/**
 * $(function)
 */
static void CG_ParseScoresGlobal(void)
{
	// Get team scores.
	cg.teamScores[0] = atoi(CG_Argv(1));
	cg.teamScores[1] = atoi(CG_Argv(2));
}

/*
=================
CG_ParseTeamInfo

=================
*/
static void CG_ParseTeamInfo( void )
{
/*
	int 	i;
	int 	client;
	int 	numSortedTeamPlayers;

	numSortedTeamPlayers = atoi( CG_Argv( 1 ) );

	for ( i = 0 ; i < numSortedTeamPlayers ; i++ ) {
		client = atoi( CG_Argv( i * 6 + 2 ) );

		//sortedTeamPlayers[i] = client;

		cgs.clientinfo[ client ].location = atoi( CG_Argv( i * 6 + 3 ) );
		cgs.clientinfo[ client ].health = atoi( CG_Argv( i * 6 + 4 ) );
		cgs.clientinfo[ client ].armor = atoi( CG_Argv( i * 6 + 5 ) );
		cgs.clientinfo[ client ].curWeapon = atoi( CG_Argv( i * 6 + 6 ) );
		cgs.clientinfo[ client ].powerups = atoi( CG_Argv( i * 6 + 7 ) );
	}
*/
}

/*
================
CG_ParseServerinfo

This is called explicitly when the gamestate is first received,
and whenever the server updates any serverinfo flagged cvars
================
*/
void CG_ParseServerinfo( void ) {

    const char  *info;
    char        *buffer;
    int         skins;
    int         armbands;
    int         i;

    info                     = CG_ConfigString( CS_SERVERINFO );
    cgs.gametype             = atoi( Info_ValueForKey( info, "g_gametype" ));
    cgs.survivor             = atoi( Info_ValueForKey ( info, "g_survivor" )) ? qtrue : qfalse;
    cgs.antilagvis           = atoi( Info_ValueForKey ( info, "g_antilagvis" )) ? qtrue : qfalse;
    cgs.followenemy          = atoi(Info_ValueForKey(info, "g_followStrict")) ? qfalse : qtrue;
    cgs.g_maxrounds          = atoi( Info_ValueForKey ( info, "g_maxrounds" ));
    cgs.respawnDelay         = atoi( Info_ValueForKey ( info, "g_respawnDelay" ));
    cgs.waveRespawns         = atoi(Info_ValueForKey(info, "g_waveRespawns"));
    cgs.redWaveRespawnDelay  = atoi(Info_ValueForKey(info, "g_redWave"));
    cgs.blueWaveRespawnDelay = atoi(Info_ValueForKey(info, "g_blueWave"));

    trap_Cvar_Set("g_gametype", va("%i", cgs.gametype));

    cgs.teamflags            = atoi( Info_ValueForKey( info, "teamflags" ));
    cgs.fraglimit            = atoi( Info_ValueForKey( info, "fraglimit" ));
    cgs.capturelimit         = atoi( Info_ValueForKey( info, "capturelimit" ));
    cgs.timelimit            = atoi( Info_ValueForKey( info, "timelimit" ));
    cgs.g_hotpotato          = atof( Info_ValueForKey( info, "g_hotpotato" ));
    skins                    = atoi( Info_ValueForKey( info, "g_skins" ));
    armbands                 = atoi( Info_ValueForKey( info, "g_armbands" ));

    if (skins != cg.g_skins || armbands != cg.g_armbands) {

		// allow force skin changed, update player gfx
		cg.g_skins = skins;
		cg.g_armbands = armbands;

		//@r00t: Load skins on demand
		CG_LoadSkinOnDemand(CG_WhichSkin(TEAM_RED));
		CG_LoadSkinOnDemand(CG_WhichSkin(TEAM_BLUE));

		for (i = 0; i < MAX_CLIENTS; i++) {
			if (cgs.clientinfo[i].infoValid) {
				CG_SetupPlayerGFX(i, &cgs.clientinfo[i]);
				CG_ReloadHandSkins(i);
			}
		}
	}

    buffer = Info_ValueForKey(info, "g_teamNameBlue");
	strcpy(cg.teamNameBlue, buffer);
	buffer = Info_ValueForKey(info, "g_teamNameRed");
	strcpy(cg.teamNameRed, buffer);

	cgs.maxclients          = atoi( Info_ValueForKey( info, "sv_maxclients" ));
	cgs.precipEnabled       = atoi( Info_ValueForKey ( info, "g_enablePrecip" ));
	cgs.survivorRoundTime   = atoi( Info_ValueForKey ( info, "g_RoundTime" )) ;

	cg.firstframeofintermission = 0;

    buffer = Info_ValueForKey( info, "mapname" );
    trap_Cvar_Set("mapname", buffer);
    Com_sprintf( cgs.mapname, sizeof(cgs.mapname), "maps/%s.bsp", buffer );

}

/*
==================
CG_ParseWarmup
==================
*/
static void CG_ParseWarmup( void )
{
	const char	*info;
	int 	warmup;

	info		   = CG_ConfigString( CS_WARMUP );

	warmup		   = atoi( info );
	cg.warmupCount = -1;
	cg.warmup = warmup;
}

/*
================
CG_SetConfigValues

Called on load to set the initial values from configure strings
================
*/
void CG_SetConfigValues( void ) {

	const char	*s;

	cgs.scores1        = atoi( CG_ConfigString( CS_SCORES1 ));
	cgs.scores2        = atoi( CG_ConfigString( CS_SCORES2 ));
	cgs.levelStartTime = atoi( CG_ConfigString( CS_LEVEL_START_TIME ));
	cgs.roundEndTime   = atoi( CG_ConfigString( CS_ROUND_END_TIME ));
	cg.warmup          = atoi( CG_ConfigString( CS_WARMUP ));

	if(cgs.gametype == GT_CTF) {

		s		          = CG_ConfigString( CS_FLAGSTATUS );
		cgs.redflag       = s[0] - '0';
		cgs.blueflag	  = s[1] - '0';
		cgs.hotpotato	  = s[2] - '0';
		cgs.hotpotatotime = atoi( CG_ConfigString( CS_HOTPOTATOTIME));

	}

	/*else if( cgs.gametype == GT_UT_BTB || cgs.gametype == GT_UT_C4 ) {
		s = CG_ConfigString( CS_FLAGSTATUS );
		cgs.flagStatus = s[0] - '0';
	} */

	cg.bombPlantTime = atoi( CG_ConfigString( CS_BOMBPLANTTIME));
	cgs.g_funstuff = atoi( CG_ConfigString( CS_FUNSTUFF ));

	if(cgs.gametype == GT_JUMP) {
	    cgs.g_noDamage = atoi( CG_ConfigString( CS_NODAMAGE ));
	}

}

/*
=====================
CG_ShaderStateChanged
=====================
*/
void CG_ShaderStateChanged(void)
{
/*	char originalShader[MAX_QPATH];
	char newShader[MAX_QPATH];
	char timeOffset[16];
	const char *o;
	char *n,*t;

	o = CG_ConfigString( CS_SHADERSTATE );
	while (o && *o) {
		n = strstr(o, "=");
		if (n && *n) {
			strncpy(originalShader, o, n-o);
			originalShader[n-o] = 0;
			n++;
			t = strstr(n, ":");
			if (t && *t) {
				strncpy(newShader, n, t-n);
				newShader[t-n] = 0;
			} else {
				break;
			}
			t++;
			o = strstr(t, "@");
			if (o) {
				strncpy(timeOffset, t, o-t);
				timeOffset[o-t] = 0;
				o++;
				trap_R_RemapShader( originalShader, newShader, timeOffset );
			}
		} else {
			break;
		}
	} */
}

/*
================
CG_ConfigStringModified

================
*/
static void CG_ConfigStringModified( void )
{
	const char	*str;
	int 	num;
	utSkins_t  skin;

	num = atoi( CG_Argv( 1 ));

	// get the gamestate from the client system, which will have the
	// new configstring already integrated
	trap_GetGameState( &cgs.gameState );

	// look up the individual string that was modified
	str = CG_ConfigString( num );

	switch (num)
	{
		/*	case CS_TIMECHOP:
			 oldchop=cg.timechop;
			 cg.timechop=atoi(str);
			 cg.time-=cg.timechop;
			 break; */

		case CS_GAME_ID:
			cgs.gameid = atoi (str);
			break;

		/*		case CS_MUSIC:
					CG_StartMusic();
					break; */
		case CS_ROUNDSTAT:
			strcpy(cg.roundstat, str);
			break;

		case CS_SERVERINFO:
			CG_ParseServerinfo();
			break;

		case CS_WARMUP:
			CG_ParseWarmup();
			break;

		case CS_SCORES1:
			cgs.scores1 = atoi( str );
			break;

		case CS_SCORES2:
			cgs.scores2 = atoi( str );
			break;

		case CS_LEVEL_START_TIME:
			cgs.levelStartTime = atoi( str );
			break;

		case CS_ROUND_END_TIME:
			cgs.roundEndTime = atoi( str );
			break;

		case CS_CAPTURE_SCORE_TIME:
			cg.cahTime = atoi ( str );
			break;

		case CS_VOTE_TIME:
			cgs.voteTime	 = atoi( str );
			cgs.voteModified = qtrue;
			break;

		case CS_VOTE_YES:
			cgs.voteYes  = atoi( str );
			cgs.voteModified = qtrue;
			break;

		case CS_VOTE_NO:
			cgs.voteNo	 = atoi( str );
			cgs.voteModified = qtrue;
			break;

		case CS_VOTE_STRING:
			Q_strncpyz( cgs.voteString, str, sizeof(cgs.voteString));
			break;

		case CS_TEAMVOTE_TIME:
		case CS_TEAMVOTE_TIME + 1:
			cgs.teamVoteTime[num - CS_TEAMVOTE_TIME]	 = atoi( str );
			cgs.teamVoteModified[num - CS_TEAMVOTE_TIME] = qtrue;
			break;

		case CS_TEAMVOTE_YES:
		case CS_TEAMVOTE_YES + 1:
			cgs.teamVoteYes[num - CS_TEAMVOTE_YES]		= atoi( str );
			cgs.teamVoteModified[num - CS_TEAMVOTE_YES] = qtrue;
			break;

		case CS_TEAMVOTE_NO:
		case CS_TEAMVOTE_NO + 1:
			cgs.teamVoteNo[num - CS_TEAMVOTE_NO]	   = atoi( str );
			cgs.teamVoteModified[num - CS_TEAMVOTE_NO] = qtrue;
			break;

		case CS_TEAMVOTE_STRING:
		case CS_TEAMVOTE_STRING + 1:
			Q_strncpyz( cgs.teamVoteString[num - CS_TEAMVOTE_STRING], str, sizeof(cgs.teamVoteString));
			break;

		case CS_INTERMISSION:
			cg.intermissionStarted = atoi( str );
			break;

		case CS_NODAMAGE:
			if (cgs.gametype == GT_JUMP) {
				cgs.g_noDamage = atoi(str);
			}
			break;

		case CS_FUNSTUFF:
		    cgs.g_funstuff = atoi(str);
		    break;

		case CS_FLAGSTATUS:

			if(cgs.gametype == GT_CTF)
			{
				// format is rb where its red/blue, 0 is at base, 1 is taken, 2 is dropped
				cgs.redflag   = str[0] - '0';
				cgs.blueflag	  = str[1] - '0';
				cgs.hotpotato	  = str[2] - '0';
				cgs.hotpotatotime = atoi( CG_ConfigString( CS_HOTPOTATOTIME));
			}
/*			else if(  cgs.gametype == GT_UT_BTB || cgs.gametype == GT_UT_C4 )
			{
				cgs.flagStatus = str[0] - '0';
			} */
			break;

		case CS_BOMBPLANTTIME:
			cg.bombPlantTime = atoi( CG_ConfigString( CS_BOMBPLANTTIME));
			break;

		case CS_HOTPOTATOTIME:
			cgs.hotpotatotime = atoi( CG_ConfigString( CS_HOTPOTATOTIME));
			break;
		/*		case CS_SHADERSTATE:
					CG_ShaderStateChanged();
					break; */

		case CS_ROUNDWINNER:
			str = CG_ConfigString(CS_ROUNDWINNER);

			// If empty string.
			if (!strlen(str)) {
				CG_ClearAllMarks();
			} else if (Q_strncmp("Round Drawn", str, 11)) {
				if (cgs.gametype == GT_UT_SURVIVOR) {
					if (!Q_strncmp(S_COLOR_RED "Red", str, 5)) {
						str += 5;
						skin = CG_WhichSkin(TEAM_RED);
					} else if (!Q_strncmp(S_COLOR_BLUE "Blue", str, 6)) {
						str += 6;
						skin = CG_WhichSkin(TEAM_BLUE);
					} else {
						// this should not happen
						skin = 0;
					}
					str = va("%s%s%s", skinInfos[skin].ui_text_color, skinInfos[skin].defaultTeamColorName, str);
				} else {
					// no changes needed for LMS
				}
			}

			

			Com_sprintf(cg.survivorRoundWinner, sizeof(cg.survivorRoundWinner), str);
			break;

		case CS_PAUSE_STATE:
			str = CG_ConfigString(CS_PAUSE_STATE);
			sscanf ( str, "%i %i %i", &cg.pauseState, &cg.pauseTime, &cg.pausedTime);
			break;

		case CS_MATCH_STATE:
			cg.matchState = atoi(str);

			// If we should start recording a demo.
			if (cg_autoRecordMatch.integer &&
				(cg.matchState & UT_MATCH_ON) &&
				(cg.matchState & UT_MATCH_RR) &&
				(cg.matchState & UT_MATCH_BR))
			{
				// Start recording a demo.
				CG_RecordDemo();
			}
			break;

		/*		case CS_TEAM_NAME_RED:
					strcpy(cg.teamNameRed, str);
					break;

				case CS_TEAM_NAME_BLUE:
					strcpy(cg.teamNameBlue, str);
					break; */

		case CS_ROUNDCOUNT:
			cgs.g_roundcount = atoi(str);
			break;

		/*case CG_DEFUSE_TIMER:
			cg.DefuseLength = atoi(str) * 1000;
			break;

		case CG_DEFUSE_START:
			cg.DefuseStartTime = cg.time;
			break;*/

		default:

			if ((num >= CS_MODELS) && (num < CS_MODELS + MAX_MODELS))
			{
				cgs.gameModels[num - CS_MODELS] = trap_R_RegisterModel( str );
			}
			else if (num >= CS_SOUNDS && num < CS_SOUNDS + MAX_SOUNDS) //27 FIX
			{
				if (str[0] != '*')
				{
					cgs.gameSounds[num - CS_SOUNDS] = trap_S_RegisterSound( str, qfalse );
				}
			}
			else if (num >= CS_PLAYERS && num < CS_PLAYERS + MAX_CLIENTS)
			{
				CG_NewClientInfo( num - CS_PLAYERS );
			}
			else if (num >= CS_FLAGS && num < CS_FLAGS + MAX_FLAGS)
			{
				sscanf ( CG_ConfigString ( num ), "%i %i %f %f %f",
					 &cgs.gameFlags[num - CS_FLAGS].number,
					 &cgs.gameFlags[num - CS_FLAGS].team,
					 &cgs.gameFlags[num - CS_FLAGS].origin[0],
					 &cgs.gameFlags[num - CS_FLAGS].origin[1],
					 &cgs.gameFlags[num - CS_FLAGS].origin[2]  );
			}

			break;
	}
}

/*
===============
CG_MapRestart

The server has issued a map_restart, so the next snapshot
is completely new and should not be interpolated to.

A tournement restart will clear everything, but doesn't
require a reload of all the media
===============
*/
static void CG_MapRestart( void )
{
	int  i;

	if (cg_showmiss.integer)
	{
		CG_Printf( "CG_MapRestart\n" );
	}

	// Send this data to the server.
	trap_Cvar_Set ( "weapmodes", cg.weaponModes );

	CG_InitLocalEntities();
	CG_InitMarkPolys();

	// need to do this to make rain and snow restart right
	CG_InitPrecipitation();

	cg.intermissionStarted = qfalse;

	cgs.voteTime		   = 0;

	cg.mapRestart		   = qtrue;

	CG_StartMusic();

	trap_S_ClearLoopingSounds(qtrue);

	if (cg_singlePlayerActive.integer)
	{
		trap_Cvar_Set("ui_matchStartTime", va("%i", cg.time));

		if (cg_recordSPDemo.integer && *cg_recordSPDemoName.string)
		{
			trap_SendConsoleCommand(va("set g_synchronousclients 1 ; record %s \n", cg_recordSPDemoName.string));
		}
	}

	for(i = 0; i < cgs.maxclients; i++)
	{
		cgs.clientinfo[i].bleedCount = 0;
		cgs.clientinfo[i].ghost 	 = qfalse;
	}

	trap_Cvar_Set ( "cg_thirdPerson", "0" );
}

#define MAX_VOICEFILESIZE 16384
#define MAX_VOICEFILES	  8
#define MAX_VOICECHATS	  64
#define MAX_VOICESOUNDS   64
#define MAX_CHATSIZE	  64
#define MAX_HEADMODELS	  64

/**
 * $(function)
 */
static void CG_RemoveChatEscapeChar( char *text )
{
	int  i, l;

	l = 0;

	for (i = 0; text[i]; i++)
	{
		if (text[i] == '\x19')
		{
			continue;
		}
		text[l++] = text[i];
	}
	text[l] = '\0';
}

/**
 * $(function)
 */
static void CG_Radio(void)
{
	char  l[MAX_SAY_TEXT];
	char  s[MAX_SAY_TEXT];
	char  t[MAX_SAY_TEXT];
	char  file[MAX_SAY_TEXT];
	int   client	  = atoi(CG_Argv(1));
	int   group   = atoi(CG_Argv(2));
	int   msg	  = atoi(CG_Argv(3));
	char  *legacy_colour;
	char  *colour;
	int   SoundHandle = 0;

	// Get client colour.
	switch (cgs.clientinfo[client].team)
	{
		case TEAM_BLUE:
			legacy_colour = S_COLOR_BLUE;
			colour = skinInfos[CG_WhichSkin(TEAM_BLUE)].ui_text_color;
			break;

		case TEAM_RED:
			legacy_colour = S_COLOR_RED;
			colour = skinInfos[CG_WhichSkin(TEAM_RED)].ui_text_color;
			break;

		default:
			legacy_colour = S_COLOR_YELLOW;
			colour = S_COLOR_YELLOW;
			break;
	}

	// Get location text.
	Q_strncpyz(l, CG_Argv(4), sizeof(l));

	if (group == -1) // Its an autoradio
	{
		if (cg_autoRadio.integer == 0)
		{
			return;
		}

		switch (msg)
		{
			case 0:
				{
					strcpy(s, "Bahahaha.");
					strcpy(file, "lol1");
					break;
				}

			case 1:
				{
					strcpy(s, "Heh.");
					strcpy(file, "lol2");
					break;
				}

			case 2:
				{
					strcpy(s, "Haha.");
					strcpy(file, "lol3");
					break;
				}

			case 3:
				{
					strcpy(s, "I've got the bomb.");
					strcpy(file, "a1");
					break;
				}

			case 4:
				{
					strcpy(s, "I'm planting the bomb, cover me.");
					strcpy(file, "a2");
					break;
				}

			case 5:
				{
					strcpy(s, "Base to ground units, operative bomb detected.");
					strcpy(file, "a3");
					break;
				}

			case 6:
				{
					strcpy(s, "Base to ground units, detonation imminent in 3 seconds.");
					strcpy(file, "a4");
					break;
				}

			case 7:
				{
					strcpy(s, "Defusing...cover me");
					strcpy(file, "a5");
					break;
				}

			case 8:
				{
					strcpy(s, "They lost the bomb, cover it!");
					strcpy(file, "a6");
					break;
				}

			case 9:
				{
					if (cg_autoRadio.integer > 1)
					{
						return;
					}
					strcpy(s, "Grenade!");
					strcpy(file, "a7");
					break;
				}

			case 10:
				{
					strcpy(s, "Move out");
					strcpy(file, "a8");
					break;
				}

			case 11:
				{
					strcpy(s, "Cover Me");
					strcpy(file, "25");
					break;
				}

			default:
				{
					return;

					break;
				}
		}
	}
	else
	{
		// Get message text.
		Q_strncpyz(s, CG_Argv(5), sizeof(s));
	}

	// If location text is included.
	if (l[0])
	{
		Com_sprintf(t, sizeof(t), "%s>%s%s%s (%s): %s%s", legacy_colour, colour, cgs.clientinfo[client].name, S_COLOR_WHITE, l, S_COLOR_YELLOW, s);
	}
	else
	{
		Com_sprintf(t, sizeof(t), "%s>%s%s%s: %s%s", legacy_colour, colour, cgs.clientinfo[client].name, S_COLOR_WHITE, S_COLOR_YELLOW, s);
	}

	// Display text.
	CG_AddChatText(t);

	// Play radio sound.
	if (!cg_noVoiceChats.integer)
	{
		//if (!bg_radiogrouplist[group].messages[msg].sound[cgs.clientinfo[client].gender])
		{
			char  filename[MAX_QPATH];

			if (group == -1)
			{
				if (cgs.clientinfo[client].gender == GENDER_FEMALE)
				{
					Com_sprintf( filename, MAX_QPATH, "sound/radio/female/female%s.wav", file);
				}
				else
				{
					Com_sprintf( filename, MAX_QPATH, "sound/radio/male/male%s.wav", file );
				}

				SoundHandle = trap_S_RegisterSound ( filename, qfalse );
			}
			else
			{
				/*if (cgs.clientinfo[client].gender == GENDER_FEMALE)
				{
					Com_sprintf( filename, MAX_QPATH, "sound/radio/female/female%i%i.wav", group, msg );
				}
				else
				{
					Com_sprintf( filename, MAX_QPATH, "sound/radio/male/male%i%i.wav", group, msg );
				}*/

				SoundHandle = cgs.media.radioSounds[cgs.clientinfo[client].gender][group][msg];
			}

			//if ( bg_radiogrouplist[group].messages[msg].sound[cgs.clientinfo[client].gender] )
			trap_S_StartLocalSound ( SoundHandle, CHAN_ANNOUNCER );
		}
	} //*/
}

/*
=================
CG_ServerCommand

The string has been tokenized and can be retrieved with
Cmd_Argc() / Cmd_Argv()
=================
*/
static void CG_ServerCommand( void )
{
	const char	*cmd;
	char		text[MAX_SAY_TEXT];
	int 	i;
	char    *s;

	cmd = CG_Argv(0);

	if (!cmd[0])
	{
		// server claimed the command
		return;
	}

	if (!strcmp( cmd, "ccprint" ))
	{
		switch ( atoi(CG_Argv(1)) ) {
		case CCPRINT_FLAG_TAKEN:
			if (cgs.gametype == GT_JUMP) {
				i = (atoi(CG_Argv(4)) == TEAM_RED) ? UT_SKIN_RED : UT_SKIN_BLUE;
			} else {
				i = CG_WhichSkin(atoi(CG_Argv(4)));
			}
			s = va("%s%s" S_COLOR_WHITE " has taken the %s%s" S_COLOR_WHITE " flag!",
				skinInfos[CG_WhichSkin(atoi(CG_Argv(2)))].ui_text_color,
				CG_Argv(3),
				skinInfos[i].ui_text_color,
				skinInfos[i].defaultTeamColorName);
				CG_CenterPrint ( s, SCREEN_HEIGHT * 0.25f, BIGCHAR_WIDTH );
			return;
		case CCPRINT_FLAG_CAPTURED:
			if (cgs.gametype == GT_JUMP) {
				i = (atoi(CG_Argv(4)) == TEAM_RED) ? UT_SKIN_RED : UT_SKIN_BLUE;
			} else {
				i = CG_WhichSkin(atoi(CG_Argv(4)));
			}
			s = va("%s%s" S_COLOR_WHITE " captured the %s%s" S_COLOR_WHITE " flag!",
				skinInfos[CG_WhichSkin(atoi(CG_Argv(2)))].ui_text_color,
				CG_Argv(3),
				skinInfos[i].ui_text_color,
				skinInfos[i].defaultTeamColorName);

			CG_CenterPrint ( s, SCREEN_HEIGHT * 0.25f, BIGCHAR_WIDTH );
			return;
		case CCPRINT_FLAG_DROPPED:
			i = atoi(CG_Argv(4));
			if (!i) { //neutral flag
				s = va("%s%s" S_COLOR_WHITE " dropped the " S_COLOR_WHITE "FREE flag!",
					skinInfos[CG_WhichSkin(atoi(CG_Argv(2)))].ui_text_color,
					CG_Argv(3));
			} else {
				if (cgs.gametype == GT_JUMP) {
					i = (i == TEAM_RED) ? UT_SKIN_RED : UT_SKIN_BLUE;
				} else {
					i = CG_WhichSkin(i);
				}
				s = va("%s%s" S_COLOR_WHITE " dropped the %s%s" S_COLOR_WHITE " flag!",
					skinInfos[CG_WhichSkin(atoi(CG_Argv(2)))].ui_text_color,
					CG_Argv(3),
					skinInfos[i].ui_text_color,
					skinInfos[i].defaultTeamColorName);
			}
			CG_CenterPrint ( s, SCREEN_HEIGHT * 0.25f, BIGCHAR_WIDTH );
			return;
		case CCPRINT_FLAG_RETURN:
			if (cgs.gametype == GT_JUMP) {
				i = (atoi(CG_Argv(2)) == TEAM_RED) ? UT_SKIN_RED : UT_SKIN_BLUE;
			} else {
				i = CG_WhichSkin(atoi(CG_Argv(2)));
			}
			s = va("The %s%s" S_COLOR_WHITE " flag has returned!",
				skinInfos[i].ui_text_color,
				skinInfos[i].defaultTeamColorName);
			CG_CenterPrint ( s, SCREEN_HEIGHT * 0.25f, BIGCHAR_WIDTH );
			return;
		case CCPRINT_ENTERED:
			CG_Printf( "%s%s" S_COLOR_WHITE " entered the game\n",
				skinInfos[CG_WhichSkin(atoi(CG_Argv(2)))].ui_text_color,
				CG_Argv(3));
			return;
		case CCPRINT_JOIN:
			i = atoi(CG_Argv(3));
			if (!i) {
				i = CG_WhichSkin(i);
				s = va("%s" S_COLOR_WHITE " joined the %s%s " S_COLOR_WHITE "team.\n",
					CG_Argv(2),
					skinInfos[i].ui_text_color,
					skinInfos[i].defaultTeamColorName);		
			} else {
				s = va("%s" S_COLOR_WHITE " joined the battle.\n",
					CG_Argv(2));
			}
			CG_Printf(s);
			return;
		case CCPRINT_MATCH_SUB:
			i = CG_WhichSkin(atoi(CG_Argv(3)));
			s = va("%s%s" S_COLOR_WHITE " is a substitute for the %s%s" S_COLOR_WHITE " team.",
				skinInfos[i].ui_text_color,
				CG_Argv(2),
				skinInfos[i].ui_text_color,
				skinInfos[i].defaultTeamColorName);
			CG_Printf(s);
			return;
		case CCPRINT_MATCH_ACTIVE:
			i = CG_WhichSkin(atoi(CG_Argv(3)));
			s = va("%s%s" S_COLOR_WHITE " is an active player for the %s%s" S_COLOR_WHITE " team.",
				skinInfos[i].ui_text_color,
				CG_Argv(2),
				skinInfos[i].ui_text_color,
				skinInfos[i].defaultTeamColorName);
			CG_Printf(s);
			return;
		case CCPRINT_MATCH_FORCESUB:
			i = CG_WhichSkin(atoi(CG_Argv(3)));
			s = va("%s%s" S_COLOR_WHITE " was forced to be a substitute for the %s%s" S_COLOR_WHITE " team.",
				skinInfos[i].ui_text_color,
				CG_Argv(2),
				skinInfos[i].ui_text_color,
				skinInfos[i].defaultTeamColorName);
			CG_Printf(s);
			return;
		case CCPRINT_MATCH_UNCAP:
			i = CG_WhichSkin(atoi(CG_Argv(3)));
			s = va("%s%s" S_COLOR_WHITE " is no longer the captain of the %s%s" S_COLOR_WHITE " team.",
				skinInfos[i].ui_text_color,
				CG_Argv(2),
				skinInfos[i].ui_text_color,
				skinInfos[i].defaultTeamColorName);
			CG_Printf(s);
			return;
		case CCPRINT_MATCH_CAP:
			i = CG_WhichSkin(atoi(CG_Argv(3)));
			s = va("%s%s" S_COLOR_WHITE " is now the captain of the %s%s" S_COLOR_WHITE " team.",
				skinInfos[i].ui_text_color,
				CG_Argv(2),
				skinInfos[i].ui_text_color,
				skinInfos[i].defaultTeamColorName);
			CG_Printf(s);
			return;
		case CCPRINT_TEAM_FULL:
			i = CG_WhichSkin(atoi(CG_Argv(2)));
			s = va("%s%s" S_COLOR_WHITE " team has too many players.",
				skinInfos[i].ui_text_color,
				skinInfos[i].defaultTeamColorName);
			CG_Printf(s);
			return ;
		}
		CG_Printf( "Unknown ccprint command: %d\n", atoi(CG_Argv(1)) );
		return;
	}

	if (!strcmp( cmd, "cp" ))
	{
		CG_CenterPrint( CG_Argv(1), SCREEN_HEIGHT * 0.25f, BIGCHAR_WIDTH );
		return;
	}

	if (!strcmp( cmd, "cs" ))
	{
		CG_ConfigStringModified();
		return;
	}

	if (!strcmp( cmd, "print" ))
	{
		CG_Printf( "%s", CG_Argv(1));
/* URBAN TERROR - not using these sounds
#ifdef MISSIONPACK
		cmd = CG_Argv(1);			// yes, this is obviously a hack, but so is the way we hear about
									// votes passing or failing
		if ( !Q_stricmpn( cmd, "vote failed", 11 ) || !Q_stricmpn( cmd, "team vote failed", 16 )) {
			trap_S_StartLocalSound( cgs.media.voteFailed, CHAN_ANNOUNCER );
		} else if ( !Q_stricmpn( cmd, "vote passed", 11 ) || !Q_stricmpn( cmd, "team vote passed", 16 ) ) {
			trap_S_StartLocalSound( cgs.media.votePassed, CHAN_ANNOUNCER );
		}
#endif
*/
		return;
	}

	if (!strcmp (cmd, "rsay" ))
	{
		CG_Radio ( );
		return;
	}

	if (!strcmp( cmd, "chat" ))
	{
		if (!cg_teamChatsOnly.integer)
		{
			if (cg_chatSound.integer > 1) {
				trap_S_StartLocalSound( cgs.media.talkSound, CHAN_LOCAL_SOUND );
			}

			Q_strncpyz( text, CG_Argv(1), MAX_SAY_TEXT );
			CG_RemoveChatEscapeChar( text );
			CG_AddChatText ( text );
		}
		return;
	}

	if (!strcmp( cmd, "tchat" ))
	{
		Q_strncpyz( text, CG_Argv(1), MAX_SAY_TEXT );
		CG_RemoveChatEscapeChar( text );
		CG_AddChatText ( text );
		return;
	}
	// skin customized version of 'chat'
	if (!strcmp( cmd, "cchat" ))
	{
		if (!cg_teamChatsOnly.integer)
		{
			if (cg_chatSound.integer > 1) {
				trap_S_StartLocalSound( cgs.media.talkSound, CHAN_LOCAL_SOUND );
			}
			i = atoi(CG_Argv(1));
			switch (i) {
			case TEAM_SPECTATOR: // SAY_NPC
				Com_sprintf(text, MAX_SAY_TEXT, "%s%s", S_COLOR_GREEN, CG_Argv(2));
				break;
			case TEAM_FREE:
				Com_sprintf(text, MAX_SAY_TEXT, "%s%s", S_COLOR_WHITE, CG_Argv(2));
				break;
			case TEAM_BLUE:
				s = skinInfos[CG_WhichSkin(i)].ui_text_color;
				Com_sprintf(text, MAX_SAY_TEXT, "%s>%s%s", S_COLOR_BLUE, s, CG_Argv(2));
				break;
			case TEAM_RED:
				s = skinInfos[CG_WhichSkin(i)].ui_text_color;
				Com_sprintf(text, MAX_SAY_TEXT, "%s>%s%s", S_COLOR_RED, s, CG_Argv(2));
				break;
			}
			CG_RemoveChatEscapeChar( text );
			CG_AddChatText ( text );
		}
		return;
	}
	// skin customized version of 'tchat'
	if (!strcmp( cmd, "tcchat" ))
	{
		i = atoi(CG_Argv(1));
		switch (i) {
		case TEAM_SPECTATOR: // SAY_NPC
			Com_sprintf(text, MAX_SAY_TEXT, "%s%s", S_COLOR_GREEN, CG_Argv(2));
			break;
		case TEAM_FREE:
			Com_sprintf(text, MAX_SAY_TEXT, "%s%s", S_COLOR_WHITE, CG_Argv(2));
			break;
		case TEAM_BLUE:
			s = skinInfos[CG_WhichSkin(i)].ui_text_color;
			Com_sprintf(text, MAX_SAY_TEXT, "%s>%s%s", S_COLOR_BLUE, s, CG_Argv(2));
			break;
		case TEAM_RED:
			s = skinInfos[CG_WhichSkin(i)].ui_text_color;
			Com_sprintf(text, MAX_SAY_TEXT, "%s>%s%s", S_COLOR_RED, s, CG_Argv(2));
			break;
		}
		CG_RemoveChatEscapeChar( text );
		CG_AddChatText ( text );
		return;
	}

	if (!strcmp( cmd, "vchat" ))
	{
		return;
	}

	if (!strcmp( cmd, "scores" ))
	{
		CG_ParseScores();
		return;
	}

	if (!strcmp( cmd, "scoress" ))
	{
		CG_ParseScoresSingle();
		return;
	}

	if (!strcmp( cmd, "scoresd" ))
	{
		CG_ParseScoresDouble();
		return;
	}

	if (!strcmp( cmd, "scoresg" ))
	{
		CG_ParseScoresGlobal();
		return;
	}

	if (!strcmp( cmd, "tinfo" ))
	{
		CG_ParseTeamInfo();
		return;
	}

	if (!strcmp( cmd, "map_restart" ))
	{
		CG_MapRestart();
		return;
	}

	if (Q_stricmp (cmd, "remapShader") == 0)
	{
		if (trap_Argc() == 4)
		{
			trap_R_RemapShader(CG_Argv(1), CG_Argv(2), CG_Argv(3));
		}
	}

	// 'sploded.  --Thaddeus (2002/04/05)
	if (!Q_stricmp( cmd, "sploded" ))
	{
		// @Fenix - do not display on Jump mode
		// because player damage is disabled
		if (cgs.gametype != GT_JUMP) {
			CG_CenterPrint( "'Sploded.", SCREEN_HEIGHT * 0.25f, BIGCHAR_WIDTH );
		}

		return;
	}

	// clientLevelShot is sent before taking a special screenshot for
	// the menu system during development
	if (!strcmp( cmd, "clientLevelShot" ))
	{
		cg.levelShot = qtrue;
		return;
	}

	// load a keyboard interface menu
	if (!strcmp( cmd, "ui_keyboardinterface" ))
	{
		char  stops[MAX_SAY_TEXT];
		int   currStopID;
		int   len;

		currStopID = atoi(CG_Argv( 1 ));

		Com_sprintf(stops, MAX_SAY_TEXT, "ui_keyboardinterface %d ", currStopID);
		len = strlen(stops);

		for (i = 0; i < MAX_STOPS; i++)
		{
			trap_Argv(i + 2, stops + len, MAX_SAY_TEXT - len);
			len = strlen(stops);
			stops[len++] = ' ';
		}
		stops[len++] = '\n';
		stops[len++] = '\0';

		trap_SendConsoleCommand(stops);
		return;
	}

	if (!strcmp(cmd, "stats"))
	{
		trap_Cvar_Set("statsoutput0", CG_Argv(1));
		return;
	}

	if (!strcmp(cmd, "stats1"))
	{
		trap_Cvar_Set("statsoutput1", CG_Argv(1));
		return;
	}

	/***
		s.e.t.i. 8/20/2010: followpowerup, this message was broadcasted to all clients
			Argv(1) holds the client number to follow
	***/
	if ( !strcmp( cmd, "followpowerup" ) ) {
		// Need to make sure:
		// 1) Person is a spectator
		// 2) Person is not a sub
		// 3)
		if ( (cg_followpowerup.integer == 1) &&
			(cgs.clientinfo[cg.clientNum].team == TEAM_SPECTATOR) &&
			(cgs.clientinfo[cg.clientNum].substitute == qfalse)) {
			char followcmd[32];

			Com_sprintf(followcmd, 32, "follow %s", CG_Argv(1)); // client number is argv(1)
			//Com_Printf(followcmd);
			trap_SendClientCommand(followcmd); // send the command
		}

		return;
	}

	/***
		s.e.t.i. 8/20/2010: followkiller, this message was broadcasted to all clients
			Argv(1) holds the client number to follow
	***/
	if ( !strcmp( cmd, "followkiller" ) ) {
		if ( (cg_followkiller.integer == 1) &&
			(cgs.clientinfo[cg.clientNum].team == TEAM_SPECTATOR) &&  // do they have cg_followpowerup set
			(cgs.clientinfo[cg.clientNum].substitute == qfalse)) {
			char followcmd[32];

			Com_sprintf(followcmd, 32, "follow %s", CG_Argv(1)); // client number is argv(1)
			//Com_Printf(followcmd);
			trap_SendClientCommand(followcmd); // send the command
		}

		return;
	}

	CG_Printf( "Unknown client game command: %s\n", cmd );
}

/*
====================
CG_ExecuteNewServerCommands

Execute all of the server commands that were received along
with this this snapshot.
====================
*/
void CG_ExecuteNewServerCommands( int latestSequence )
{
	while (cgs.serverCommandSequence < latestSequence)
	{
		if (trap_GetServerCommand( ++cgs.serverCommandSequence ))
		{
			CG_ServerCommand();
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : CG_AddChatText
// Description: Adds some text to the chat area
// Author	  : Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
void CG_AddChatText ( const char *str )
{
	int   chatHeight;
	//qboolean  firstline = qfalse;
	int   len;

	// Dump it to the console first
	trap_Print( str );
	trap_Print( "\n" );

	if (cg_standardChat.integer)
	{
		return;
	}

	if (cg_chatHeight.integer < CHAT_HEIGHT)
	{
		chatHeight = cg_chatHeight.integer;
	}
	else
	{
		chatHeight = CHAT_HEIGHT;
	}

	if ((chatHeight <= 0) || (cg_chatTime.integer <= 0))
	{
		// team chat disabled, dump into normal chat
		cgs.chatPos = cgs.chatLastPos = 0;
		return;
	}

	len = strlen( str );

	while(len)
	{
		qboolean  dash	 = qfalse;
		int   rewind = 0;

		if(len > CHAT_WIDTH)
		{
			if((str[CHAT_WIDTH - 1] != ' ') && (str[CHAT_WIDTH - 1] != '\n'))
			{
				// Rewind the the last space
				for (rewind = 1; str[CHAT_WIDTH - rewind - 1] != ' ' && rewind < 10; rewind++);

				if(str[CHAT_WIDTH - rewind - 1] != ' ')
				{
					dash   = qtrue;
					rewind = 1;
				}
			}

			Q_strncpyz ( cgs.chatText[cgs.chatPos % chatHeight], S_COLOR_YELLOW, CHAT_WIDTH  );
			Q_strncpyz ( cgs.chatText[cgs.chatPos % chatHeight] + 2, str, CHAT_WIDTH - rewind );

			if(dash)
			{
				strcat ( cgs.chatText[cgs.chatPos % chatHeight] + 2, "-" );
				rewind++;
			}

			str += (CHAT_WIDTH - rewind);
			len -= (CHAT_WIDTH - rewind);
		}
		else
		{
			Q_strncpyz ( cgs.chatText[cgs.chatPos % chatHeight], S_COLOR_YELLOW, CHAT_WIDTH );
			Q_strncpyz ( cgs.chatText[cgs.chatPos % chatHeight] + 2, str, CHAT_WIDTH );

			len -= len;
		}

		cgs.chatTextTimes[cgs.chatPos % chatHeight] = cg.time;
		cgs.chatPos++;

		if (cgs.chatPos - cgs.chatLastPos > chatHeight)
		{
			cgs.chatLastPos = cgs.chatPos - chatHeight;
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : CG_AddMessageText
// Description: Adds some text to the message area
// Author	  : Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
void CG_AddMessageText( const char *str )
{
	int   msgHeight;
	qboolean  firstline = qtrue;
	int   len;
	char	  nextColor = '*';

	if (cg_standardChat.integer)
	{
		return;
	}

	if (cg_msgHeight.integer < MSG_HEIGHT)
	{
		msgHeight = cg_msgHeight.integer;
	}
	else
	{
		msgHeight = MSG_HEIGHT;
	}

	// messages disabled
	if ((msgHeight <= 0) || (cg_msgTime.integer <= 0))
	{
		cgs.msgPos = cgs.msgLastPos = 0;
		return;
	}

	len = strlen( str );

	while(len)
	{
		qboolean  dash	 = qfalse;
		int   rewind = 0;

		if(len > MSG_WIDTH)
		{
			if((str[MSG_WIDTH - 1] != ' ') && (str[MSG_WIDTH - 1] != '\n'))
			{
				// Rewind the the last space
				for (rewind = 1; str[MSG_WIDTH - rewind - 1] != ' ' && rewind < 10; rewind++)
				{
					;
				}

				if(str[MSG_WIDTH - rewind - 1] != ' ')
				{
					dash   = qtrue;
					rewind = 1;
				}
			}

			if(!firstline)
			{
				if (nextColor == '*')
				{
					Q_strncpyz ( cgs.msgText[cgs.msgPos % msgHeight], S_COLOR_WHITE, MSG_WIDTH	);
				}
				else
				{
					Com_sprintf ( cgs.msgText[cgs.msgPos % msgHeight], 4, "^%c", nextColor );
				}
			}

			Q_strncpyz ( cgs.msgText[cgs.msgPos % msgHeight] + (firstline ? 0 : 2), str, MSG_WIDTH - rewind );

			if(dash)
			{
				strcat ( cgs.msgText[cgs.msgPos % msgHeight] + (firstline ? 0 : 2), "-" );
				rewind++;
			}

			{
				int  col;
				nextColor = '*';

				for (col = strlen ( cgs.msgText[cgs.msgPos % msgHeight] ) - 1; col >= 0; col--)
				{
					if (cgs.msgText[cgs.msgPos % msgHeight][col] == '^')
					{
						nextColor = cgs.msgText[cgs.msgPos % msgHeight][col + 1];
						break;
					}
				}
			}

			str += (MSG_WIDTH - rewind);
			len -= (MSG_WIDTH - rewind);
		}
		else
		{
			if(!firstline)
			{
				if (nextColor == '*')
				{
					Q_strncpyz ( cgs.msgText[cgs.msgPos % msgHeight], S_COLOR_WHITE, MSG_WIDTH	);
				}
				else
				{
					Com_sprintf ( cgs.msgText[cgs.msgPos % msgHeight], 4, "^%c", nextColor );
				}
			}

			Q_strncpyz ( cgs.msgText[cgs.msgPos % msgHeight] + (firstline ? 0 : 2), str, MSG_WIDTH );

			len -= len;
		}

		cgs.msgTextTimes[cgs.msgPos % msgHeight] = cg.time;
		cgs.msgPos++;

		if (cgs.msgPos - cgs.msgLastPos > msgHeight)
		{
			cgs.msgLastPos = cgs.msgPos - msgHeight;
		}

		firstline = qfalse;
	}
}
