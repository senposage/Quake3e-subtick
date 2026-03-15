// Copyright (C) 1999-2000 Id Software, Inc.
//
// cg_consolecmds.c -- text commands typed in at the local console, or
// executed by a key binding

#include "cg_local.h"
#include "../ui/ui_shared.h"
#include "../common/c_bmalloc.h"

#ifdef MISSIONPACK
extern menuDef_t  *menuScoreboard;

#endif

//void CheckCvars(void);
void CG_NextItem_f              ( void );
void CG_PrevItem_f              ( void );
void CG_UseItem_f               ( void );
//void CG_WeapMode_f              ( void );
void CG_DropWeapon_f    ( void );
void CG_DropItem_f              ( void );
//void Cmd_WeaponSet_f    ( void );
//Iain:
void CG_PlayerList (void);
void CG_Maptoggle (void);

void CG_WeapToggle_f(void);
void CG_Radio_f(void);
//void CG_MemInfo_f(void);

void CG_RecordDemo(void);
void CG_OpenRadioMenu(void);

/**
 * $(function)
 */
static char *ConcatArgs( int start )
{
	int          i, c, tlen;
	static char  line[MAX_STRING_CHARS];
	int          len;
	char         arg[MAX_STRING_CHARS];

	len = 0;
	c   = trap_Argc();

	for (i = start ; i < c ; i++)
	{
		trap_Argv( i, arg, sizeof(arg));
		tlen = strlen( arg );

		if (len + tlen >= MAX_STRING_CHARS - 1)
		{
			break;
		}
		memcpy( line + len, arg, tlen );
		len += tlen;

		if (i != c - 1)
		{
			line[len] = ' ';
			len++;
		}
	}

	line[len] = 0;

	return line;
}

/**
 * $(function)
 */
static char *CleanPathName(char *string)
{
	int  i, l;

	l = strlen(string);

	// Check for invalid characters.
	for (i = 0; i < l; i++)
	{
		/*
		if (string[i] == ' '	||
		        string[i] == '\t'	||
		        string[i] == '\n'	||
		        string[i] == '\r'	||
		        string[i] == '\\'	||
		        string[i] == '|'	||
		        string[i] == '/'	||
		        string[i] == ':'	||
		        string[i] == '*'	||
		        string[i] == '?'	||
		        string[i] == '"'	||
		        string[i] == '<'	||
		        string[i] == '>')
		{
		        string[i] = '_';
		}
		*/

		if (!((string[i] >= 48) && (string[i] <= 57)) &&
		    !((string[i] >= 65) && (string[i] <= 90)) &&
		    !((string[i] >= 97) && (string[i] <= 122)))
		{
			string[i] = '_';
		}
	}

	return string;
}

/**
 * $(function)
 */
void CG_TargetCommand_f( void )
{
	int   targetNum;
	char  test[4];

	targetNum = CG_CrosshairPlayer();

	if (!targetNum)
	{
		return;
	}

	trap_Argv( 1, test, 4 );
	trap_SendConsoleCommand( va( "gc %i %i", targetNum, atoi( test )));
}

//turns the minimap on and off
void CG_Maptoggle (void)
{
	if (cg_maptoggle.integer == 0)
	{
		cg_maptoggle.integer = 1;
	}
	else
	{
		cg_maptoggle.integer = 0;
	}
}

/**
 * $(function)
 */
void CG_PlayerList (void)
{
	// Dokta8: implemented this command client side since client
	// knows client nums of all connected clients.
	// trap_SendClientCommand("playerlist");	// don't need this no more

	// loop thru all connected clients and print client # and name

	centity_t  *cent;
	int        i;

	CG_Printf("Current players:\n");

	for (i = 0; i < MAX_CLIENTS; i++)     // 64 is max # of player entities, always first 64 in list
	{
		cent = CG_ENTITIES(i);

		if (!cent)
		{
			continue;       // make sure we have a valid pointer
		}

		// only print if clientinfo is valid: normally it will be for connected clients
		if (cgs.clientinfo[i].infoValid)
		{
			CG_Printf("  %d: [%s]\n", i, cgs.clientinfo[i].name); //27's fixme - the index is the same as the number lol
		}
	}
	CG_Printf("End current player list.\n");
}

/*
=================
CG_SizeUp_f

Keybinding command
=================
*/
static void CG_SizeUp_f (void)
{
	trap_Cvar_Set("cg_viewsize", va("%i", (int)(cg_viewsize.integer + 10)));
}

/*
=================
CG_SizeDown_f

Keybinding command
=================
*/
static void CG_SizeDown_f (void)
{
	trap_Cvar_Set("cg_viewsize", va("%i", (int)(cg_viewsize.integer - 10)));
}

/*
=============
CG_Viewpos_f

Debugging command to print the current position
=============
*/
static void CG_Viewpos_f (void)
{
	CG_Printf ("(%i %i %i) : %i\n", (int)cg.refdef.vieworg[0],
	           (int)cg.refdef.vieworg[1], (int)cg.refdef.vieworg[2],
	           (int)cg.refdefViewAngles[YAW]);
}

/**
 * $(function)
 */
static void CG_ScoresDown_f( void )
{
	if (cg.scoresRequestTime + 2000 < cg.time)
	{
		// the scores are more than two seconds out of data,
		// so request new ones
		cg.scoresRequestTime = cg.time;
		trap_SendClientCommand( "score" );

/*
                // leave the current scores up if they were already
                // displayed, but if this is the first hit, clear them out
                if ( !cg.showScores ) {
                        cg.showScores = qtrue;
                        cg.numScores = 0;
                }
*/
		cg.showScores = qtrue;
	}
	else
	{
		// show the cached contents even if they just pressed if it
		// is within two seconds
		cg.showScores = qtrue;
	}
}

/**
 * $(function)
 */
static void CG_ScoresUp_f( void )
{
	if (cg.showScores)
	{
		cg.showScores    = qfalse;
		cg.scoreFadeTime = cg.time;
	}
}

#ifdef MISSIONPACK
extern menuDef_t  *menuScoreboard;
void Menu_Reset(void);                  // FIXME: add to right include file

/**
 * $(function)
 */
static void CG_LoadHud_f( void)
{
	char        buff[1024];
	const char  *hudSet;

	memset(buff, 0, sizeof(buff));

	String_Init();
	Menu_Reset();

	hudSet = "ui/hud.txt";

	CG_LoadMenus(hudSet);
	menuScoreboard = NULL;
}

/**
 * $(function)
 */
static void CG_scrollScoresDown_f( void)
{
	if (menuScoreboard && cg.scoreBoardShowing)
	{
		Menu_ScrollFeeder(menuScoreboard, FEEDER_SCOREBOARD, qtrue);
		Menu_ScrollFeeder(menuScoreboard, FEEDER_REDTEAM_LIST, qtrue);
		Menu_ScrollFeeder(menuScoreboard, FEEDER_BLUETEAM_LIST, qtrue);
	}
}

/**
 * $(function)
 */
static void CG_scrollScoresUp_f( void)
{
	if (menuScoreboard && cg.scoreBoardShowing)
	{
		Menu_ScrollFeeder(menuScoreboard, FEEDER_SCOREBOARD, qfalse);
		Menu_ScrollFeeder(menuScoreboard, FEEDER_REDTEAM_LIST, qfalse);
		Menu_ScrollFeeder(menuScoreboard, FEEDER_BLUETEAM_LIST, qfalse);
	}
}

#ifdef USE_SINGLE_PLAYER


/**
 * $(function)
 */
static void CG_spWin_f( void)
{
	trap_Cvar_Set("cg_cameraOrbit", "2");
	trap_Cvar_Set("cg_cameraOrbitDelay", "35");
	trap_Cvar_Set("cg_thirdPerson", "1");
	trap_Cvar_Set("cg_thirdPersonAngle", "0");
	trap_Cvar_Set("cg_thirdPersonRange", "100");

/* URBAN TERROR - Less sounds
        CG_AddBufferedSound(cgs.media.winnerSound);
*/

	CG_CenterPrint("YOU WIN!", SCREEN_HEIGHT * .30, 0);
}

/**
 * $(function)
 */
static void CG_spLose_f( void)
{
	trap_Cvar_Set("cg_cameraOrbit", "2");
	trap_Cvar_Set("cg_cameraOrbitDelay", "35");
	trap_Cvar_Set("cg_thirdPerson", "1");
	trap_Cvar_Set("cg_thirdPersonAngle", "0");
	trap_Cvar_Set("cg_thirdPersonRange", "100");

/* URBAN TERROR - Less sounds
        CG_AddBufferedSound(cgs.media.loserSound);
*/

	CG_CenterPrint("YOU LOSE...", SCREEN_HEIGHT * .30, 0);
}
#endif

#endif

/**
 * $(function)
 */
static void CG_TellTarget_f( void )
{
	int   clientNum;
	char  command[128];
	char  message[128];

	clientNum = CG_CrosshairPlayer();

	if (clientNum == -1)
	{
		return;
	}

	trap_Args( message, 128 );
	Com_sprintf( command, 128, "tell %i %s", clientNum, message );
	trap_SendClientCommand( command );
}

/**
 * $(function)
 */
static void CG_TellAttacker_f( void )
{
	int   clientNum;
	char  command[128];
	char  message[128];

	clientNum = CG_LastAttacker();

	if (clientNum == -1)
	{
		return;
	}

	trap_Args( message, 128 );
	Com_sprintf( command, 128, "tell %i %s", clientNum, message );
	trap_SendClientCommand( command );
}

/**
 * $(function)
 */
static void CG_VoiceTellTarget_f( void )
{
	int   clientNum;
	char  command[128];
	char  message[128];

	clientNum = CG_CrosshairPlayer();

	if (clientNum == -1)
	{
		return;
	}

	trap_Args( message, 128 );
	Com_sprintf( command, 128, "vtell %i %s", clientNum, message );
	trap_SendClientCommand( command );
}

/**
 * $(function)
 */
static void CG_VoiceTellAttacker_f( void )
{
	int   clientNum;
	char  command[128];
	char  message[128];

	clientNum = CG_LastAttacker();

	if (clientNum == -1)
	{
		return;
	}

	trap_Args( message, 128 );
	Com_sprintf( command, 128, "vtell %i %s", clientNum, message );
	trap_SendClientCommand( command );
}

#ifdef MISSIONPACK
/**
 * $(function)
 */
static void CG_NextTeamMember_f( void )
{
//  CG_SelectNextPlayer();
}

/**
 * $(function)
 */
static void CG_PrevTeamMember_f( void )
{
//  CG_SelectPrevPlayer();
}

// ASS U ME's enumeration order as far as task specific orders, OFFENSE is zero, CAMP is last
//
static void CG_NextOrder_f( void )
{
/*
        clientInfo_t *ci = cgs.clientinfo + cg.snap->ps.clientNum;
        if (ci) {
                if (!ci->teamLeader && sortedTeamPlayers[cg_currentSelectedPlayer.integer] != cg.snap->ps.clientNum) {
                        return;
                }
        }
        if (cgs.currentOrder < TEAMTASK_CAMP) {
                cgs.currentOrder++;

                if (cgs.currentOrder == TEAMTASK_RETRIEVE) {
                        if (!CG_OtherTeamHasFlag()) {
                                cgs.currentOrder++;
                        }
                }

                if (cgs.currentOrder == TEAMTASK_ESCORT) {
                        if (!CG_YourTeamHasFlag()) {
                                cgs.currentOrder++;
                        }
                }

        } else {
                cgs.currentOrder = TEAMTASK_OFFENSE;
        }
        cgs.orderPending = qtrue;
        cgs.orderTime = cg.time + 3000;
*/
}

/**
 * $(function)
 */
static void CG_ConfirmOrder_f (void )
{
/*
        trap_SendConsoleCommand(va("cmd vtell %d %s\n", cgs.acceptLeader, VOICECHAT_YES));
        trap_SendConsoleCommand("+button5; wait; -button5");
        if (cg.time < cgs.acceptOrderTime) {
                trap_SendClientCommand(va("teamtask %d\n", cgs.acceptTask));
                cgs.acceptOrderTime = 0;
        }
*/
}

/**
 * $(function)
 */
static void CG_DenyOrder_f (void )
{
/*
        trap_SendConsoleCommand(va("cmd vtell %d %s\n", cgs.acceptLeader, VOICECHAT_NO));
        trap_SendConsoleCommand("+button6; wait; -button6");
        if (cg.time < cgs.acceptOrderTime) {
                cgs.acceptOrderTime = 0;
        }
*/
}


#ifdef USE_SINGLE_PLAYER

/**
 * $(function)
 */
static void CG_TaskOffense_f (void )
{
	if (cgs.gametype == GT_CTF)
	{
		trap_SendConsoleCommand(va("cmd vsay_team %s\n", VOICECHAT_ONGETFLAG));
	}
	else
	{
		trap_SendConsoleCommand(va("cmd vsay_team %s\n", VOICECHAT_ONOFFENSE));
	}
	trap_SendClientCommand(va("teamtask %d\n", TEAMTASK_OFFENSE));
}

/**
 * $(function)
 */
static void CG_TaskDefense_f (void )
{
	trap_SendConsoleCommand(va("cmd vsay_team %s\n", VOICECHAT_ONDEFENSE));
	trap_SendClientCommand(va("teamtask %d\n", TEAMTASK_DEFENSE));
}

/**
 * $(function)
 */
static void CG_TaskPatrol_f (void )
{
	trap_SendConsoleCommand(va("cmd vsay_team %s\n", VOICECHAT_ONPATROL));
	trap_SendClientCommand(va("teamtask %d\n", TEAMTASK_PATROL));
}

/**
 * $(function)
 */
static void CG_TaskCamp_f (void )
{
	trap_SendConsoleCommand(va("cmd vsay_team %s\n", VOICECHAT_ONCAMPING));
	trap_SendClientCommand(va("teamtask %d\n", TEAMTASK_CAMP));
}

/**
 * $(function)
 */
static void CG_TaskFollow_f (void )
{
	trap_SendConsoleCommand(va("cmd vsay_team %s\n", VOICECHAT_ONFOLLOW));
	trap_SendClientCommand(va("teamtask %d\n", TEAMTASK_FOLLOW));
}

/**
 * $(function)
 */
static void CG_TaskRetrieve_f (void )
{
	trap_SendConsoleCommand(va("cmd vsay_team %s\n", VOICECHAT_ONRETURNFLAG));
	trap_SendClientCommand(va("teamtask %d\n", TEAMTASK_RETRIEVE));
}

/**
 * $(function)
 */
static void CG_TaskEscort_f (void )
{
	trap_SendConsoleCommand(va("cmd vsay_team %s\n", VOICECHAT_ONFOLLOWCARRIER));
	trap_SendClientCommand(va("teamtask %d\n", TEAMTASK_ESCORT));
}

/**
 * $(function)
 */
static void CG_TaskOwnFlag_f (void )
{
	trap_SendConsoleCommand(va("cmd vsay_team %s\n", VOICECHAT_IHAVEFLAG));
}

/**
 * $(function)
 */
static void CG_TauntKillInsult_f (void )
{
	trap_SendConsoleCommand("cmd vsay kill_insult\n");
}

/**
 * $(function)
 */
static void CG_TauntPraise_f (void )
{
	trap_SendConsoleCommand("cmd vsay praise\n");
}

/**
 * $(function)
 */
static void CG_TauntTaunt_f (void )
{
	trap_SendConsoleCommand("cmd vtaunt\n");
}

/**
 * $(function)
 */
static void CG_TauntDeathInsult_f (void )
{
	trap_SendConsoleCommand("cmd vsay death_insult\n");
}

/**
 * $(function)
 */
static void CG_TauntGauntlet_f (void )
{
	trap_SendConsoleCommand("cmd vsay kill_guantlet\n");
}

/**
 * $(function)
 */
static void CG_TaskSuicide_f (void )
{
	int   clientNum;
	char  command[128];

	clientNum = CG_CrosshairPlayer();

	if (clientNum == -1)
	{
		return;
	}

	Com_sprintf( command, 128, "tell %i suicide", clientNum );
	trap_SendClientCommand( command );
}
#endif

//////////////////////////////////////////////////////////////////////////////////////////
// Name       : CG_MemInfo_f
// Description: Displays how much memory is being used by the vms'
// Author     : Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
//#define	MAX_LOCAL_ENTITIES	512
//extern localEntity_t	cg_localEntities[MAX_LOCAL_ENTITIES];

extern localEntity_t  *cg_freeLocalEntities;                    // single linked list
extern localEntity_t  *cg_activeLocalEntitiesBegin;             // single linked list

#ifdef DEBUG
/**
 * $(function)
 */
void CG_MemInfo_f ( void )
{
	CG_Printf ( "CGAME:\n" );
	CG_Printf ( "  cgs_t            = %i (199K)\n", sizeof(cgs_t));
	CG_Printf ( "  cg_t             = %i (123K)\n", sizeof(cg_t));
	CG_Printf ( "  cg_entities      = %i (688K)\n", sizeof(cg_entities));
	CG_Printf ( "  cg_weapons       = %i (212K)\n", sizeof(cg_weapons));
	CG_Printf ( "  cg_items         = %i (13K)\n", sizeof(cg_items));
	CG_Printf ( "  cg_markPolys     = %i (74K)\n", sizeof(cg_markPolys));
	print_lists();

/*	for ( i = 0, le=cg_freeLocalEntities; le; le=le->next, i++ );

        CG_Printf ( "  Local Entities:\n" );
        CG_Printf ( "    free           = %i\n", i );

        for ( l = 0, le=cg_activeLocalEntitiesBegin; le; le=le->next, l++ );

        CG_Printf ( "    used           = %i\n", l );
        CG_Printf ( "    total          = %i\n", l + i );  */

//	trap_SendClientCommand( "ut_meminfo" );
}
#endif
/**
 * $(function)
 */
void CG_Radio_f(void)
{
	char  s[10];
	int   group, msg;

	// Get first argument.
	strcpy(s, "");
	trap_Argv(1, s, sizeof(s));

	// If argument is invalid.
	if (s[0] == 0)
	{
		return;
	}

	//Check for "lol"
	Q_strlwr(s);

	if (!strcmp(s, "lol"))
	{
		trap_SendClientCommand("ut_radio lol");
		return;
	}

	// Convert to integer.
	group = atoi(s);

	// If group is out of bounds.
	if ((group < 1) || (group > 9))
	{
		return;
	}

	// Get second argument.
	strcpy(s, "");
	trap_Argv(2, s, sizeof(s));

	// If argument is invalid,
	if (s[0] == 0)
	{
		return;
	}

	// Convert to integer.
	msg = atoi(s);

	// If message is out of bounds.
	if ((msg < 1) || (msg > 9))
	{
		return;
	}

	// If sound file does not exist.
	if (strlen(RadioTextList[group - 1][msg]) == 0)
	{
		return;
	}

	// Send command to server.
	trap_SendClientCommand(va("ut_radio %d %d %s", group, msg, ConcatArgs(3)));
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name       : CG_NextItem_f
// Description: Moves to the next item in your inventory
// Author     : Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
void CG_NextItem_f( void )
{
	int  i;
	int  original;
	int  itemSlot;

	if (!cg.snap)
	{
		return;
	}

	if (cg.snap->ps.pm_flags & PMF_FOLLOW)
	{
		return;
	}

	// If we're paused.
	if (cg.pauseState & UT_PAUSE_ON)
	{
		return;
	}

	cg.itemSelectTime = cg.time;
	original          = cg.itemSelect;

	if (-1 == (itemSlot = utPSGetItemByID ( &cg.snap->ps, original )))
	{
		itemSlot = 0;
		original = 0;
	}

	for (i = 0 ; i < UT_MAX_ITEMS; i++)
	{
		// Move to the next item
		itemSlot++ ;

		// Make sure it didnt wrap around.
		if (itemSlot >= UT_MAX_ITEMS)
		{
			itemSlot = 0;
		}

		if(UT_ITEM_GETID(cg.snap->ps.itemData[itemSlot]) != 0)
		{
			break;
		}
	}

	if (i == UT_MAX_ITEMS)
	{
		cg.itemSelect = original;
	}
	else
	{
		cg.itemSelect = UT_ITEM_GETID(cg.snap->ps.itemData[itemSlot]);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name       : CG_PrevItem_f
// Description: Moves to the previous item in your inventory
// Author     : Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
void CG_PrevItem_f( void )
{
	int  i;
	int  original;
	int  itemSlot;

	if (!cg.snap)
	{
		return;
	}

	if (cg.snap->ps.pm_flags & PMF_FOLLOW)
	{
		return;
	}

	// If we're paused.
	if (cg.pauseState & UT_PAUSE_ON)
	{
		return;
	}

	cg.itemSelectTime = cg.time;
	original          = cg.itemSelect;

	if (-1 == (itemSlot = utPSGetItemByID ( &cg.snap->ps, original )))
	{
		itemSlot = 0;
		original = 0;
	}

	for (i = 0 ; i < UT_MAX_ITEMS ; i++)
	{
		// Move to the previous item
		itemSlot-- ;

		// Make sure it didnt wrap around.
		if (itemSlot < 0)
		{
			itemSlot = UT_MAX_ITEMS - 1;
		}

		if(UT_ITEM_GETID(cg.snap->ps.itemData[itemSlot]) != 0)
		{
			break;
		}
	}

	if (i == UT_MAX_ITEMS)
	{
		cg.itemSelect = original;
	}
	else
	{
		cg.itemSelect = UT_ITEM_GETID(cg.snap->ps.itemData[itemSlot]);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name       : CG_UseItem_f
// Description: Uses a item.
// Changlist  : apoxol 04 Jan 2001 created
///////////////////////////////////////////////////////////////////////////////////////////
void CG_UseItem_f ( void )
{
	char  command[128];
	char  s[10];

	if (cg.snap->ps.pm_flags & PMF_FOLLOW)
	{
		return;
	}

	// If we're paused.
	if (cg.pauseState & UT_PAUSE_ON)
	{
		return;
	}

	// no using items when specing or dead or ghost
	if (!(cg.predictedPlayerState.pm_type == PM_NORMAL) ||
	    (cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR) ||
	    (cg.predictedPlayerState.persistant[PERS_TEAM] == TEAM_SPECTATOR) ||
	    (cg.predictedPlayerState.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_GHOST))
	{
		return;
	}

	switch (cg.itemSelect)
	{
		case UT_ITEM_LASER:
				trap_S_StartLocalSound ( cgs.media.laserOnOff, CHAN_LOCAL_SOUND );
			break;

		case UT_ITEM_NVG:

			if (utPSIsItemOn ( &cg.snap->ps, UT_ITEM_NVG ))
			{
				trap_S_StartLocalSound ( cgs.media.nvgOff, CHAN_LOCAL_SOUND );
			}
			else
			{
				trap_S_StartLocalSound ( cgs.media.nvgOn, CHAN_LOCAL_SOUND );
			}
			break;

		default:
			trap_S_StartLocalSound ( cgs.media.flashLight, CHAN_LOCAL_SOUND );
			break;
	}

	strcpy(s, "");
	trap_Argv(1, s, sizeof(s));

	// B1n: Allow item use by name
	if (strlen(s) == 0) {
		if(!cg.itemSelect) { return; }

		Com_sprintf( command, 128, "ut_itemuse %i", cg.itemSelect );
	} else {
		Com_sprintf( command, 128, "ut_itemuse %s", s);
	}
	trap_SendClientCommand( command );
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name       : CG_DropItem_f
// Description: Drops a item.
// Changlist  : apoxol 04 Jan 2001 created
///////////////////////////////////////////////////////////////////////////////////////////
void CG_DropItem_f ( void )
{
	char  command[128];
	char  s[10];

	if (cg.snap->ps.pm_flags & PMF_FOLLOW)
	{
		return;
	}

	if (cg.predictedPlayerState.pm_type != PM_NORMAL)
	{
		return;
	}

	// If we're paused.
	if (cg.pauseState & UT_PAUSE_ON)
	{
		return;
	}

	// see if theres a param
	strcpy(s, "");
	trap_Argv(1, s, sizeof(s));

	if (strlen(s) == 0)
	{
		if(!cg.itemSelect)
		{
			CG_NextItem_f ( );
		}

		Com_sprintf( command, 128, "ut_itemdrop %i", cg.itemSelect ); //drop the current selected
	}
	else
	{
		Com_sprintf( command, 128, "ut_itemdrop %s", s); //drop the param
	}
	CG_NextItem_f ( );

	trap_SendClientCommand( command );
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name       : CG_DropWeapon_f
// Description: Drops a weapon.
// Changlist  : apoxol 04 Jan 2001 created
///////////////////////////////////////////////////////////////////////////////////////////
void CG_DropWeapon_f ( void )
{
	char  command[128];
	int   weaponID;

	if (cg.snap->ps.pm_flags & PMF_FOLLOW)
	{
		return;
	}

	if (cg.predictedPlayerState.pm_type != PM_NORMAL)
	{
		return;
	}

	// Cant drop the knife
	if((weaponID = utPSGetWeaponID ( &cg.snap->ps )) == UT_WP_KNIFE)
	{
		return;
	}

	// If we're doing something with the gun.
	//if (cg.predictedPlayerState.weaponTime > 0)
	//	return;

	// If we're paused.
	if (cg.pauseState & UT_PAUSE_ON)
	{
		return;
	}

	// If the gun are not ready
	if ((weaponID == UT_WP_SR8) && (cg.predictedPlayerState.weaponstate != WEAPON_READY))
	{
		return;
	}

	// Prepare the drop statement
	Com_sprintf( command, 128, "ut_weapdrop %i", weaponID );

	// Switch to the next weapon since we are dropping the current one
	//CG_NextWeapon_f ( );
	CG_BiggestWeapon_f();

	// Tell the server about the weapon drop
	trap_SendClientCommand( command );
}

 #define TOGGLE_PRIMARY   0
 #define TOGGLE_SECONDARY 1
 #define TOGGLE_SIDEARM   2
 #define TOGGLE_GRENADE   3
 #define TOGGLE_KNIFE     4
 #define TOGGLE_BOMB      5

int  toggles[][10] = {
	{
		UT_WP_LR,
		UT_WP_G36,
		UT_WP_AK103,
		UT_WP_M4,
		UT_WP_PSG1,
		UT_WP_SR8,
		UT_WP_HK69,
		UT_WP_NEGEV,
		UT_WP_NONE,
	},
	{
		UT_WP_MP5K,
		UT_WP_UMP45,
		UT_WP_SPAS12,
		UT_WP_MAC11,
		/*UT_WP_P90,
		UT_WP_BENELLI,
		UT_WP_DUAL_BERETTA,
		UT_WP_DUAL_DEAGLE,
		UT_WP_DUAL_GLOCK,
		UT_WP_DUAL_MAGNUM,*/
		UT_WP_NONE,
	},
	{
		UT_WP_BERETTA,
		UT_WP_DEAGLE,
		UT_WP_GLOCK,
		UT_WP_COLT1911,
		//UT_WP_MAGNUM,
		UT_WP_NONE,
	},
	{
		UT_WP_GRENADE_HE,
		UT_WP_GRENADE_FLASH,
		UT_WP_GRENADE_SMOKE,
		UT_WP_NONE,
	},
	{
		UT_WP_KNIFE,
		UT_WP_NONE,
	},
	{
		UT_WP_BOMB,
		UT_WP_NONE,
	}
};

/**
 * $(function)
 */
int GetToggleWeapon (char *typeName, int skip) {

	int  i;
	int  l;
	int  type;
	int  weapon;

	if (!Q_stricmp(typeName, "sidearm" )) {
		type = TOGGLE_SIDEARM;
	} else if (!Q_stricmp(typeName, "primary")) {
		type = TOGGLE_PRIMARY;
	} else if (!Q_stricmp(typeName, "secondary")) {
		type = TOGGLE_SECONDARY;
	} else if (!Q_stricmp(typeName, "grenade")) {
		type = TOGGLE_GRENADE;
	} else if (!Q_stricmp(typeName, "knife")) {
		type = TOGGLE_KNIFE;
	} else if (!Q_stricmp(typeName, "bomb")) {
		type = TOGGLE_BOMB;
	} else {
		return UT_WP_NONE;
	}

	for (i = UT_MAX_WEAPONS - 1; i >= 0 ; i--) {    // max_weapons

		weapon = UT_WEAPON_GETID(cg.predictedPlayerState.weaponData[i]);

		// If this is the one we have to skip
		if (weapon == skip) {
			continue;
		}

		// If the weapon slot is empty
		if (weapon == UT_WP_NONE) {
			continue;
		}

		for (l = 0; toggles[type][l] != UT_WP_NONE; l++) {
			if (toggles[type][l] == weapon) {
				return weapon;
			}
		}
	}

	return UT_WP_NONE;
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name       : CG_WeapToggle_f
// Description: Toggles the weapon between a and b
// Changlist  : apoxol 04 Jan 2001 created
///////////////////////////////////////////////////////////////////////////////////////////
void CG_WeapToggle_f ( void ) {

	const char  *arg;
	int         startWeapon;
	int         firstToggle;
	int         secondToggle;

	if (cg.snap->ps.pm_flags & PMF_FOLLOW) {
		return;
	}

	// If we're paused.
	if (cg.pauseState & UT_PAUSE_ON) {
		return;
	}

	// Radio menu cancels this
	if (cg.radiomenu != 0) {
		return;
	}

	// No input given
	if (NULL == (arg = CG_Argv(1))) {
		return;
	}

	// Get current gun
	startWeapon = utPSGetWeaponID(&cg.predictedPlayerState);

	// Get the 1st toggle weapon
	firstToggle = GetToggleWeapon ((char *)arg, UT_WP_NONE);

	if (firstToggle == UT_WP_NONE) {
		if (Q_stricmp (arg, "primary") == 0) {
			// @Fenix - This because secondary weapons can be equipped as primary
			// GetToggleWeapon will return UT_WP_NONE just if we do not equip
			// a proper Primary weapon in the Primary weapon slot. Because of this
			// we will need to skip the weapon we are currently holding otherwise if
			// we equip 2 secondary weapons, ut_weaptoggle will get stuck on the 1st one
			firstToggle = GetToggleWeapon ("secondary", startWeapon);
		}
	}

	if (NULL == (arg = CG_Argv(2))) {
		secondToggle = firstToggle;
	}
	else {

		// We got a second parameter specified. Getting the second weapon
		secondToggle = GetToggleWeapon ((char *)arg, UT_WP_NONE);
		if (secondToggle == UT_WP_NONE) {
			secondToggle = firstToggle;
		} else if (firstToggle == UT_WP_NONE) {
			firstToggle = secondToggle;
		}
	}

	if (firstToggle == UT_WP_NONE) {
		return;
	}

	if (startWeapon == firstToggle) {
		cg.weaponSelect = secondToggle;
	} else {
		cg.weaponSelect = firstToggle;
	}

	// If weapon was changed.
	if (cg.weaponSelect != startWeapon) {
		cg.weaponSelectTime                                               = cg.time;
		cg.zoomed                                                         = 0;
		cgs.clientinfo[cg.predictedPlayerState.clientNum].muzzleFlashTime = 0;
	}
}

#endif

/**
 * $(function)
 */
void CG_UTEcho_f ( void )
{
	char  *arg;
	char  fullstring[1024];
	int   i;

	for (i = 0; i < 1024; i++)
	{
		fullstring[i] = (char)0;
	}

	i   = 1;
	arg = (char *)CG_Argv(i);

	while (arg[0])
	{
		strcat( fullstring, arg );
		strcat( fullstring, " " );
		i++;
		arg = (char *)CG_Argv(i);
	}

	CG_Printf("%s", fullstring);
}

/*
==================
CG_StartOrbit_f
==================
*/

static void CG_StartOrbit_f( void )
{
	if (!cgs.cheats)
	{
		CG_Printf ( "startorbit is cheat protected.\n" );
		return;
	}

	if (cg_cameraOrbit.value != 0)
	{
		trap_Cvar_Set ("cg_cameraOrbit", "0");
		trap_Cvar_Set("cg_thirdPerson", "0");
	}
	else
	{
		trap_Cvar_Set("cg_cameraOrbit", "5");
		trap_Cvar_Set("cg_thirdPerson", "1");
		trap_Cvar_Set("cg_thirdPersonAngle", "0");
		trap_Cvar_Set("cg_thirdPersonRange", "100");
	}
}

/*=---- Temp function for Gleam ----*/

void CG_ClearAllMarks(void);

/**
 * $(function)
 */
//static void CG_ClearAll_f( void )   // THIS SHOULD BE DISABLED FOR RELEASE!
//{ CG_ClearAllMarks();
//}

/////////////////////////////////////////

// commands for the invader game
void inv_leftmove( void )
{
	if (cg.invadergame.playerdx < 0)
	{
		cg.invadergame.playerdx = 0;
	}
	else
	{
		cg.invadergame.playerdx = -1;
	}
}

/**
 * $(function)
 */
void inv_rightmove( void )
{
	if (cg.invadergame.playerdx > 0)
	{
		cg.invadergame.playerdx = 0;
	}
	else
	{
		cg.invadergame.playerdx = 1;
	}
}

/**
 * $(function)
 */
void inv_firemove( void )
{
	cg.invadergame.playershoot = cg.time;
}

/////////////////////////////////////////

////////// BREAKOUT //////////

void breakout_play(void)
{
	// If we're already playing.
	if (cg.breakout.play)
	{
		cg.breakout.play = qfalse;
	}
	else
	{
		cg.breakout.play = qtrue;
	}
}

/**
 * $(function)
 */
void breakout_reset(void)
{
	cg.breakout.reset = qtrue;
}

/**
 * $(function)
 */
void breakout_pause(void)
{
	// If we're already paused.
	if (cg.breakout.pause)
	{
		cg.breakout.pause = qfalse;
	}
	else
	{
		cg.breakout.pause = qtrue;
	}
}

/**
 * $(function)
 */
void breakout_left(void)
{
	// Update input only if we're playing.
	if (cg.breakout.play && !cg.breakout.pause && !cg.breakout.gameover && (cg.breakout.delay <= cg.time))
	{
		cg.breakout.paddle.v = -1;
	}
}

/**
 * $(function)
 */
void breakout_right(void)
{
	// Update input only if we're playing.
	if (cg.breakout.play && !cg.breakout.pause && !cg.breakout.gameover && (cg.breakout.delay <= cg.time))
	{
		cg.breakout.paddle.v = 1;
	}
}

/////////// SNAKE ////////////

void snake_play(void)
{
	// If we're already playing.
	if (cg.snake.play)
	{
		cg.snake.play = qfalse;
	}
	else
	{
		cg.snake.play = qtrue;
	}
}

/**
 * $(function)
 */
void snake_reset(void)
{
	cg.snake.reset = qtrue;
}

/**
 * $(function)
 */
void snake_pause(void)
{
	// If we're already paused.
	if (cg.snake.pause)
	{
		cg.snake.pause = qfalse;
	}
	else
	{
		cg.snake.pause = qtrue;
	}
}

/**
 * $(function)
 */
void snake_up(void)
{
	// Update input only if we're playing.
	if (cg.snake.play && !cg.snake.pause && !cg.snake.gameover && (cg.snake.delay <= cg.time))
	{
		if ((cg.snake.parts < 2) || (!cg.snake.body[0].s && (cg.snake.body[0].x != cg.snake.body[1].x)))
		{
			cg.snake.body[0].s    = 1;

			cg.snake.body[0].v[0] = 0;
			cg.snake.body[0].v[1] = -1;
		}
	}
}

/**
 * $(function)
 */
void snake_down(void)
{
	// Update input only if we're playing.
	if (cg.snake.play && !cg.snake.pause && !cg.snake.gameover && (cg.snake.delay <= cg.time))
	{
		if ((cg.snake.parts < 2) || (!cg.snake.body[0].s && (cg.snake.body[0].x != cg.snake.body[1].x)))
		{
			cg.snake.body[0].s    = 1;

			cg.snake.body[0].v[0] = 0;
			cg.snake.body[0].v[1] = 1;
		}
	}
}

/**
 * $(function)
 */
void snake_left(void)
{
	// Update input only if we're playing.
	if (cg.snake.play && !cg.snake.pause && !cg.snake.gameover && (cg.snake.delay <= cg.time))
	{
		if ((cg.snake.parts < 2) || (!cg.snake.body[0].s && (cg.snake.body[0].y != cg.snake.body[1].y)))
		{
			cg.snake.body[0].s    = 1;

			cg.snake.body[0].v[0] = -1;
			cg.snake.body[0].v[1] = 0;
		}
	}
}

/**
 * $(function)
 */
void snake_right(void)
{
	// Update input only if we're playing.
	if (cg.snake.play && !cg.snake.pause && !cg.snake.gameover && (cg.snake.delay <= cg.time))
	{
		if ((cg.snake.parts < 2) || (!cg.snake.body[0].s && (cg.snake.body[0].y != cg.snake.body[1].y)))
		{
			cg.snake.body[0].s    = 1;

			cg.snake.body[0].v[0] = 1;
			cg.snake.body[0].v[1] = 0;
		}
	}
}

/////////////////////////////////////////

void CG_RecordDemo(void)
{
	const char  *gameType;
	char        mapName[MAX_QPATH];
	char        playerName[MAX_NAME_LENGTH];
	qtime_t     time;
	char        demoName[MAX_OSPATH];

	if (cg.demoPlayback)
	{
		return;
	}

	// Set game type name.
	switch (cgs.gametype)
	{
		case GT_FFA:
			gameType = "FFA";
			break;

		case GT_LASTMAN:
			gameType = "LASTMAN";
			break;

		case GT_JUMP:
			gameType = "JUMP";
			break;

		case GT_TEAM:
			gameType = "TDM";
			break;

		case GT_UT_SURVIVOR:
			gameType = "TS";
			break;

		case GT_UT_ASSASIN:
			gameType = "FTL";
			break;

		case GT_UT_CAH:
			gameType = "CAH";
			break;

		case GT_CTF:
			gameType = "CTF";
			break;

		case GT_UT_BOMB:
			gameType = "BOMB";
			break;

		default:
			gameType = "NA";
			break;
	}

	// Get map name.
	trap_Cvar_VariableStringBuffer("mapname", mapName, sizeof(mapName));

	// Get player name.
	trap_Cvar_VariableStringBuffer("name", playerName, sizeof(playerName));
	Q_CleanStr(playerName); // strip color formatting chars

	// Get current time.
	trap_RealTime(&time);

	// If we're in match mode.
	if (cg.matchState & UT_MATCH_ON)
	{
		Com_sprintf(demoName, sizeof(demoName), "%04d%02d%02d_%02d%02d%02d_%s-%s-%s-%s-vs-%s",
		            time.tm_year + 1900, time.tm_mon + 1, time.tm_mday,
		            time.tm_hour, time.tm_min, time.tm_sec,
		            playerName,
		            mapName,
		            gameType,
		            cg.teamNameRed, cg.teamNameBlue);
	}
	else
	{
		Com_sprintf(demoName, sizeof(demoName), "%04d%02d%02d_%02d%02d%02d_%s-%s-%s",
		            time.tm_year + 1900, time.tm_mon + 1, time.tm_mday,
		            time.tm_hour, time.tm_min, time.tm_sec,
		            playerName,
		            mapName,
		            gameType);
	}

	// Clean demo name.
	CleanPathName(demoName);

	// Start recording.
	trap_SendConsoleCommand("g_synchronousClients 1\n");
	trap_SendConsoleCommand(va("record %s\n", demoName));
	trap_SendConsoleCommand("g_synchronousClients 0\n");
}

#ifdef VMHACKBUILDHELPINFO
extern int cg_numSolidEntities;
extern centity_t  **cg_solidEntities;
void CG_vmhaxinfo(void)
// This dumps info used to make WH/AIMBOT, never EVER should be in final build!
{
 centity_t *c = NULL;
 clientInfo_t *ci = NULL;
 CG_Printf ( "#define VM_cg_entities               %d\n", cg_entities);
 CG_Printf ( "#define VM_cg_entities_item          %d\n", sizeof(cg_entities[0]));
 CG_Printf ( "#define VM_cg_entities_currentValid  %d\n", &c->currentValid );
 CG_Printf ( "#define VM_cg_entities_currentState  %d\n", &c->currentState);
 CG_Printf ( "#define VM_cg_entities_lerpOrigin    %d\n", &c->lerpOrigin);
 CG_Printf ( "#define VM_cgs_clientinfo            %d\n", cgs.clientinfo);
 CG_Printf ( "#define VM_cgs_clientinfo_item       %d\n", sizeof(cgs.clientinfo[0]) );
 CG_Printf ( "#define VM_cgs_clientinfo_name       %d\n", &ci->name );
 CG_Printf ( "#define VM_cgs_clientinfo_team       %d\n", &ci->team);
 CG_Printf ( "#define VM_cg_clientnum              %d\n", &cg.clientNum);
 CG_Printf ( "#define VM_cg_numsolidentities       %d\n", &cg_numSolidEntities);
 CG_Printf ( "#define VM_cg_solidentities          %d\n", &cg_solidEntities);
 CG_Printf ( "#define VM_cg_solidentities_item     %d\n", sizeof(centity_t));
}
#endif

typedef struct
{
	char  *cmd;
	void (*function)(void);
} consoleCommand_t;

static consoleCommand_t  commands[] = {
	{ "viewpos",        CG_Viewpos_f           },
	{ "+scores",        CG_ScoresDown_f        },
	{ "-scores",        CG_ScoresUp_f          },
	{ "sizeup",         CG_SizeUp_f            },
	{ "sizedown",       CG_SizeDown_f          },
	{ "weapnext",       CG_NextWeapon_f        },
	{ "weapprev",       CG_PrevWeapon_f        },
	{ "weapon",         CG_Weapon_f            },
	{ "tell_target",    CG_TellTarget_f        },
	{ "tell_attacker",  CG_TellAttacker_f      },
	{ "vtell_target",   CG_VoiceTellTarget_f   },
	{ "vtell_attacker", CG_VoiceTellAttacker_f },
//	{ "tcmd", CG_TargetCommand_f },
#ifdef MISSIONPACK
	{ "loadhud",        CG_LoadHud_f           },
	{ "nextTeamMember", CG_NextTeamMember_f    },
	{ "prevTeamMember", CG_PrevTeamMember_f    },
	{ "nextOrder",      CG_NextOrder_f         },
	{ "confirmOrder",   CG_ConfirmOrder_f      },
	{ "denyOrder",      CG_DenyOrder_f         },
//	{ "spWin", CG_spWin_f },
//	{ "spLose", CG_spLose_f },
	{ "scoresDown",     CG_scrollScoresDown_f  },
	{ "scoresUp",       CG_scrollScoresUp_f    },
#endif
	{ "startOrbit",     CG_StartOrbit_f        },

	// Urban Terror
	{ "ut_itemnext",    CG_NextItem_f          },
	{ "ut_itemprev",    CG_PrevItem_f          },
	{ "ut_itemdrop",    CG_DropItem_f          },
	{ "ut_itemuse",     CG_UseItem_f           },
	{ "ut_weapdrop",    CG_DropWeapon_f        },
	{ "ut_weaptoggle",  CG_WeapToggle_f        },
	{ "ut_zoomin",      CG_UTZoomIn            },
	{ "ut_zoomout",     CG_UTZoomOut           },
	{ "ut_zoomreset",   CG_UTZoomReset         },
	{ "ut_radio",       CG_Radio_f             },
#ifdef DEBUG
	{ "ut_meminfo",     CG_MemInfo_f           },
#endif
	{ "ut_echo",        CG_UTEcho_f            },
	{ "inv_left",       inv_leftmove           },
	{ "inv_right",      inv_rightmove          },
	{ "inv_fire",       inv_firemove           },
	{ "breakout_play",  breakout_play          },
	{ "breakout_reset", breakout_reset         },
	{ "breakout_pause", breakout_pause         },
	{ "breakout_left",  breakout_left          },
	{ "breakout_right", breakout_right         },
	{ "snake_play",     snake_play             },
	{ "snake_reset",    snake_reset            },
	{ "snake_pause",    snake_pause            },
	{ "snake_up",       snake_up               },
	{ "snake_down",     snake_down             },
	{ "snake_left",     snake_left             },
	{ "snake_right",    snake_right            },
	{ "playerlist",     CG_PlayerList          },
	{ "clientlist",     CG_PlayerList          },

	{ "maptoggle",      CG_Maptoggle           },
	{ "recorddemo",     CG_RecordDemo          },
	{ "ui_radio",       CG_OpenRadioMenu       },
#ifdef VMHACKBUILDHELPINFO
	{ "vmhaxinfo",      CG_vmhaxinfo              }, // NEVER in release
#endif
	// KILL BEFORE RELEASE DENSITY!
//	{ "debug_smokekill", CG_ClearAll_f },
};

/*
=================
CG_ConsoleCommand

The string has been tokenized and can be retrieved with
Cmd_Argc() / Cmd_Argv()
=================
*/
qboolean CG_ConsoleCommand( void )
{
	const char  *cmd;
	int         i;

	cmd = CG_Argv(0);

	for (i = 0 ; i < sizeof(commands) / sizeof(commands[0]) ; i++)
	{
		if (!Q_stricmp( cmd, commands[i].cmd ))
		{
			commands[i].function();
			return qtrue;
		}
	}

	return qfalse;
}

/*
=================
CG_InitConsoleCommands

Let the client system know about all of our commands
so it can perform tab completion
=================
*/
void CG_InitConsoleCommands( void ) {
	int  i;

	for (i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
		trap_AddCommand( commands[i].cmd );
	}

	// the game server will interpret these commands, which will be automatically
	// forwarded to the server after they are not recognized locally

	trap_AddCommand ("addbot");
    #ifdef USE_AUTH
	trap_AddCommand ("auth-set");
    #endif
	trap_AddCommand ("callvote");
	trap_AddCommand ("callteamvote");
	//trap_AddCommand("clientlist");
	trap_AddCommand ("captain");
	trap_AddCommand ("follow");
	trap_AddCommand ("follownext");
	trap_AddCommand ("followprev");
	trap_AddCommand ("forceanim");
	trap_AddCommand ("give");
	trap_AddCommand ("god");
	trap_AddCommand ("ignore");
	trap_AddCommand ("ignorelist");
	trap_AddCommand ("kill");
	trap_AddCommand ("levelshot");
	trap_AddCommand ("loadPos");
	trap_AddCommand ("allowgoto");
	trap_AddCommand ("map");
	trap_AddCommand ("noclip");
	trap_AddCommand ("notarget");
	//trap_AddCommand("playerlist");
	//trap_AddCommand("playerstatus"); //@Fenix - is not linked to anything
	trap_AddCommand ("ready");
	trap_AddCommand ("regainstamina");
	trap_AddCommand ("ref");
	trap_AddCommand ("reflogin");
	trap_AddCommand ("savePos");
	trap_AddCommand ("say");
	trap_AddCommand ("say_team");
	trap_AddCommand ("setviewpos");
	trap_AddCommand ("stats");
	trap_AddCommand ("sub");
	trap_AddCommand ("team");
	trap_AddCommand ("teamname");
	trap_AddCommand ("teamtask");
	trap_AddCommand ("teamvote");
	trap_AddCommand ("tell");
	trap_AddCommand ("ut_interface");
	trap_AddCommand ("ut_itemdrop");
	trap_AddCommand ("ut_itemuse");
	trap_AddCommand ("ut_radio");
	trap_AddCommand ("ut_weapdrop");
	trap_AddCommand ("vsay");
	trap_AddCommand ("vsay_team");
	//trap_AddCommand ("vtell");
	trap_AddCommand ("vtaunt");
	trap_AddCommand ("vosay");
	trap_AddCommand ("vosay_team");
	trap_AddCommand ("vote");
	trap_AddCommand ("votell");
	trap_AddCommand ("where");

}

/**
 * $(function)
 */
void CG_RadioMessage ( int group, int msg )
{
	trap_SendClientCommand( va( "ut_radio %i %i", group, msg  ));
}

/**
 * $(function)
 */
void CG_OpenRadioMenu(void)
{
	if (cg.radiomenu == 0)
	{
		if ((cgs.gametype >= GT_TEAM) && (cgs.gametype != GT_JUMP)) // no opening the radio menu in FFA
		{
			if ((cgs.clientinfo[cg.predictedPlayerState.clientNum].team == 1) ||
			    (cgs.clientinfo[cg.predictedPlayerState.clientNum].team == 2))
			{
				cg.radiomenu = 1;
				//Click
				trap_S_StartLocalSound ( cgs.media.UT_radiomenuclick, CHAN_ANNOUNCER );
			}
		}
	}
	else
	{
		cg.radiomenu = 0;
	}
}
