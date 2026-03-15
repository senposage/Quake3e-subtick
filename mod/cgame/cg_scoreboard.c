// Copyright (C) 1999-2000 Id Software, Inc.
//
// cg_scoreboard -- draw the scoreboard on top of the game screen
#include "cg_local.h"

#define SCOREBOARD_X (0)

#define SB_HEADER	 86
#define SB_TOP		 (SB_HEADER + 32)

// Where the status bar starts, so we don't overwrite it
#define SB_STATUSBAR		 420

#define SB_NORMAL_HEIGHT	 40
#define SB_INTER_HEIGHT 	 16    // interleaved height

#define SB_MAXCLIENTS_NORMAL ((SB_STATUSBAR - SB_TOP) / SB_NORMAL_HEIGHT)
#define SB_MAXCLIENTS_INTER  ((SB_STATUSBAR - SB_TOP) / SB_INTER_HEIGHT - 1)

// Used when interleaved

#define SB_LEFT_BOTICON_X  (SCOREBOARD_X + 0)
#define SB_LEFT_HEAD_X	   (SCOREBOARD_X + 32)
#define SB_RIGHT_BOTICON_X (SCOREBOARD_X + 64)
#define SB_RIGHT_HEAD_X    (SCOREBOARD_X + 96)
// Normal
#define SB_BOTICON_X	   (SCOREBOARD_X + 32)
#define SB_HEAD_X		   (SCOREBOARD_X + 64)

#define SB_SCORELINE_X	   112

#define SB_RATING_WIDTH    (6 * BIGCHAR_WIDTH)								// width 6
#define SB_SCORE_X		   (SB_SCORELINE_X + BIGCHAR_WIDTH) 				// width 6
#define SB_RATING_X 	   (SB_SCORELINE_X + 6 * BIGCHAR_WIDTH) 			// width 6
#define SB_PING_X		   (SB_SCORELINE_X + 12 * BIGCHAR_WIDTH + 8)		// width 5
#define SB_TIME_X		   (SB_SCORELINE_X + 17 * BIGCHAR_WIDTH + 8)		// width 5
#define SB_NAME_X		   (SB_SCORELINE_X + 22 * BIGCHAR_WIDTH)			// width 15

// The new and improved score board
//
// In cases where the number of clients is high, the score board heads are interleaved
// here's the layout

//
//	0	32	 80  112  144	240  320  400	<-- pixel position
//	bot head bot head score ping time name
//
//	wins/losses are drawn on bot icon now

//static qboolean  localClient; // true if local client has been displayed


#if 0
/*
=================
CG_DrawScoreboard
=================
*/
static void CG_DrawClientScore( int y, score_t *score, float *color, float fade, qboolean largeFormat )
{
	char		  string[1024];
	vec3_t		  headAngles;
	clientInfo_t  *ci;
	int 	  iconx, headx;

	if ((score->client < 0) || (score->client >= cgs.maxclients))
	{
		Com_Printf( "Bad score->client: %i\n", score->client );
		return;
	}

	ci	  = &cgs.clientinfo[score->client];

	iconx = SB_BOTICON_X + (SB_RATING_WIDTH / 2);
	headx = SB_HEAD_X + (SB_RATING_WIDTH / 2);

	// draw the handicap or bot skill marker (unless player has flag)
	if (ci->powerups & (1 << PW_NEUTRALFLAG))
	{
		if(largeFormat)
		{
			CG_DrawFlagModel( iconx, y - (32 - BIGCHAR_HEIGHT) / 2, 32, 32, TEAM_FREE, qfalse );
		}
		else
		{
			CG_DrawFlagModel( iconx, y, 16, 16, TEAM_FREE, qfalse );
		}
	}
	else if (ci->powerups & (1 << PW_REDFLAG))
	{
		if(largeFormat)
		{
			CG_DrawFlagModel( iconx, y - (32 - BIGCHAR_HEIGHT) / 2, 32, 32, TEAM_RED, qfalse );
		}
		else
		{
			CG_DrawFlagModel( iconx, y, 16, 16, TEAM_RED, qfalse );
		}
	}
	else if (ci->powerups & (1 << PW_BLUEFLAG))
	{
		if(largeFormat)
		{
			CG_DrawFlagModel( iconx, y - (32 - BIGCHAR_HEIGHT) / 2, 32, 32, TEAM_BLUE, qfalse );
		}
		else
		{
			CG_DrawFlagModel( iconx, y, 16, 16, TEAM_BLUE, qfalse );
		}
	}
	else
	{
		if ((ci->botSkill > 0) && (ci->botSkill <= 5))
		{
			{
				if(largeFormat)
				{
					CG_DrawPic( iconx, y - (32 - BIGCHAR_HEIGHT) / 2, 32, 32, cgs.media.botSkillShaders[ci->botSkill - 1] );
				}
				else
				{
					CG_DrawPic( iconx, y, 16, 16, cgs.media.botSkillShaders[ci->botSkill - 1] );
				}
			}
		}
		else if (ci->handicap < 100)
		{
			Com_sprintf( string, sizeof(string), "%i", ci->handicap );
			CG_DrawSmallStringColor( iconx, y, string, color );
		}
	}

	// draw the face
	VectorClear( headAngles );
	headAngles[YAW] = 180;

	if(largeFormat)
	{
		CG_DrawHead( headx, y - (ICON_SIZE - BIGCHAR_HEIGHT) / 2, ICON_SIZE, ICON_SIZE,
				 score->client, headAngles );
	}
	else
	{
		CG_DrawHead( headx, y, 16, 16, score->client, headAngles );
	}

/* URBAN TERROR - not using these right now
#ifdef MISSIONPACK
	// draw the team task
	if ( ci->teamTask != TEAMTASK_NONE ) {
		if ( ci->teamTask == TEAMTASK_OFFENSE ) {
			CG_DrawPic( headx + 48, y, 16, 16, cgs.media.assaultShader );
		}
		else if ( ci->teamTask == TEAMTASK_DEFENSE ) {
			CG_DrawPic( headx + 48, y, 16, 16, cgs.media.defendShader );
		}
	}
#endif
*/

	// draw the score line
	if (score->ping == -1)
	{
		Com_sprintf(string, sizeof(string),
				" connecting	%s", ci->name);
	}
	else if (ci->team == TEAM_SPECTATOR)
	{
		Com_sprintf(string, sizeof(string),
				" SPECT %3i %4i %s", score->ping, score->time, ci->name);
	}
	else if((cg.predictedPlayerState.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_GHOST))
	{
		Com_sprintf(string, sizeof(string),
				" GHOST %3i %4i %s", score->ping, score->time, ci->name);
	}
	else
	{
		Com_sprintf(string, sizeof(string),
				"%5i %4i %4i %s", score->score, score->ping, score->time, ci->name);
	}

	// highlight your position
	if (score->client == cg.snap->ps.clientNum)
	{
		float  hcolor[4];
		int    rank;

		localClient = qtrue;

		if ((cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR) || (cgs.gametype >= GT_TEAM))
		{
			rank = -1;
		}
		else
		{
			rank = cg.snap->ps.persistant[PERS_RANK] & ~RANK_TIED_FLAG;
		}

		if (rank == 0)
		{
			hcolor[0] = 0;
			hcolor[1] = 0;
			hcolor[2] = 0.7f;
		}
		else if (rank == 1)
		{
			hcolor[0] = 0.7f;
			hcolor[1] = 0;
			hcolor[2] = 0;
		}
		else if (rank == 2)
		{
			hcolor[0] = 0.7f;
			hcolor[1] = 0.7f;
			hcolor[2] = 0;
		}
		else
		{
			hcolor[0] = 0.7f;
			hcolor[1] = 0.7f;
			hcolor[2] = 0.7f;
		}

		hcolor[3] = fade * 0.7;
		CG_FillRect( SB_SCORELINE_X + BIGCHAR_WIDTH + (SB_RATING_WIDTH / 2), y,
				 640 - SB_SCORELINE_X - BIGCHAR_WIDTH, BIGCHAR_HEIGHT + 1, hcolor );
	}

	CG_DrawBigString( SB_SCORELINE_X + (SB_RATING_WIDTH / 2), y, string, fade );

	// add the "ready" marker for intermission exiting
//	if ( cg.snap->ps.stats[ STAT_CLIENTS_READY ] & ( 1 << score->client ) ) {
//		CG_DrawBigStringColor( iconx, y, "READY", color );
//	}
}


/*
=================
CG_TeamScoreboard
=================
*/
static int CG_TeamScoreboard( int y, team_t team, float fade, int maxClients, int lineHeight )
{
	int 	  i;
	score_t 	  *score;
	float		  color[4];
	int 	  count;
	clientInfo_t  *ci;

	color[0] = color[1] = color[2] = 1.0;
	color[3] = fade;

	count	 = 0;

	for (i = 0 ; i < cg.numScores && count < maxClients ; i++)
	{
		score = &cg.scores[i];
		ci	  = &cgs.clientinfo[score->client];

		if (team != ci->team)
		{
			continue;
		}

		CG_DrawClientScore( y + lineHeight * count, score, color, fade, lineHeight == SB_NORMAL_HEIGHT );

		count++;
	}

	return count;
}
#endif


//================================================================================

/*
================
CG_CenterGiantLine
================
*/
static void CG_CenterGiantLine( float y, const char *string )
{
	float	x;
	vec4_t	color;

	color[0] = 1;
	color[1] = 1;
	color[2] = 1;
	color[3] = 1;

	x	 = 0.5 * (640 - GIANT_WIDTH * CG_DrawStrlen( string ));

	CG_DrawStringExt( x, y, string, color, qtrue, qtrue, GIANT_WIDTH, GIANT_HEIGHT, 0 );
}

/*
=================
CG_DrawTourneyScoreboard

Draw the oversize scoreboard for tournements
=================
*/
void CG_DrawOldTourneyScoreboard( void )
{
	const char	  *s;
	vec4_t		  color;
	int 	  min, tens, ones;
	clientInfo_t  *ci;
	int 	  y;
	int 	  i;
	int 	  teamMembers[TEAM_NUM_TEAMS];

	teamMembers[TEAM_RED]  = 3;
	teamMembers[TEAM_BLUE] = 3;

	// request more scores regularly
	if (cg.scoresRequestTime + 2000 < cg.time)
	{
		cg.scoresRequestTime = cg.time;
		trap_SendClientCommand( "score" );
	}

	color[0] = 1;
	color[1] = 1;
	color[2] = 1;
	color[3] = 1;

	// draw the dialog background
	color[0] = color[1] = color[2] = 0;
	color[3] = 1;
	CG_FillRect( 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, color );

	// print the mesage of the day
	s = CG_ConfigString( CS_MOTD );

	if (!s[0])
	{
		s = "Scoreboard";
	}

	// print optional title
	CG_CenterGiantLine( 8, s );

	// print server time
	ones  = cg.time / 1000;
	min   = ones / 60;
	ones %= 60;
	tens  = ones / 10;
	ones %= 10;
	s	  = va("%i:%i%i", min, tens, ones );

	CG_CenterGiantLine( 64, s );

	// print the two scores

	y = 160;

	if ((cgs.gametype >= GT_TEAM) && (cgs.gametype != GT_JUMP))
	{
		//
		// teamplay scoreboard
		//

		CG_DrawStringExt( 8, y, va("Red Team(%d)", teamMembers[TEAM_RED]), color, qtrue, qtrue, GIANT_WIDTH, GIANT_HEIGHT, 0 );
		s = va("%i", cg.teamScores[0] );
		CG_DrawStringExt( 632 - GIANT_WIDTH * strlen(s), y, s, color, qtrue, qtrue, GIANT_WIDTH, GIANT_HEIGHT, 0 );

		y += 64;

		CG_DrawStringExt( 8, y, va("Blue Team(%d)", teamMembers[TEAM_BLUE]), color, qtrue, qtrue, GIANT_WIDTH, GIANT_HEIGHT, 0 );
		s = va("%i", cg.teamScores[1] );
		CG_DrawStringExt( 632 - GIANT_WIDTH * strlen(s), y, s, color, qtrue, qtrue, GIANT_WIDTH, GIANT_HEIGHT, 0 );
	}
	else
	{
		//
		// free for all scoreboard
		//
		for (i = 0 ; i < MAX_CLIENTS ; i++)
		{
			ci = &cgs.clientinfo[i];

			if (!ci->infoValid)
			{
				continue;
			}

			if (ci->team != TEAM_FREE)
			{
				continue;
			}

			CG_DrawStringExt( 8, y, ci->name, color, qtrue, qtrue, GIANT_WIDTH, GIANT_HEIGHT, 0 );
			s  = va("%i", ci->score );
			CG_DrawStringExt( 632 - GIANT_WIDTH * strlen(s), y, s, color, qtrue, qtrue, GIANT_WIDTH, GIANT_HEIGHT, 0 );
			y += 64;
		}
	}
}

//Scoreboard can do the god damn radio too

void DrawRadioWindow(void)
{
	int j;
	int x;
	int y;
	vec4_t	white  = { 1, 1, 1, 1 };
	vec4_t	hcolor = { 0, 0, 0, 0.45f};
	vec4_t	grey   = { 0.7f, 0.7f, 0.7f, 1};
	vec4_t	teamcol;

	char	text[64];
	int tier;
	int startx, starty, width;
	int lines;
	int TappedKey  = -1;

	int CharHeight = TINYCHAR_HEIGHT;
	int CharWidth  = TINYCHAR_WIDTH;
	int team	   = cgs.clientinfo[cg.clientNum].team;

	if (team == 1) //red
	{
		teamcol[0] = 0.5;
		teamcol[1] = 0;
		teamcol[2] = 0;
		teamcol[3] = 0.75;
	}
	else
	{
		teamcol[0] = 0;
		teamcol[1] = 0;
		teamcol[2] = 0.5;
		teamcol[3] = 0.75;
	}

	x	   = 7;
	y	   = 80;

	startx = x;
	starty = y;
	width  = x + CharWidth * 27;
	lines  = 0;

	//grab the variable
	tier = cg.radiomenu - 1; //cg.radiomenu is 0 for empty

	if (tier < 0)
	{
		return;  //menu is off
	}

	//Handle the input.
	if (trap_Key_IsDown(27))
	{
		cg.radiomenu = 0;

		//Click
		trap_S_StartLocalSound ( cgs.media.UT_radiomenuclick, CHAN_ANNOUNCER );
		return;
	}

	for (j = 0; j < 10; j++)
	{
		if (trap_Key_IsDown( 48 + j ))
		{
			cg.RadioKeyHeld[j]++;

			if (cg.RadioKeyHeld[j] == 1)
			{
				TappedKey = j;
			}
		}
		else
		{
			cg.RadioKeyHeld[j] = 0;
		}
	}

	//0 key
	if (TappedKey == 0) //0 key was pressed
	{
		if (tier == 0) //exit
		{
			cg.radiomenu = 0;
			//Click
			trap_S_StartLocalSound ( cgs.media.UT_radiomenuclick, CHAN_ANNOUNCER );
			return;
		}
		else //else go back to the first tier
		{
			cg.radiomenu = 1;
			//Click
			trap_S_StartLocalSound ( cgs.media.UT_radiomenuclick, CHAN_ANNOUNCER );
		}
	}
	else
	if (TappedKey > 0 && TappedKey < 10)
	{
		if (cg.radiomenu == 1)
		{
			cg.radiomenu = TappedKey + 1;
			//Click
			trap_S_StartLocalSound ( cgs.media.UT_radiomenuclick, CHAN_ANNOUNCER );
		}
		else
		{
			//Send a Radio Command
			tier = cg.radiomenu - 1; //cg.radiomenu is 0 for empty

			if (strlen(RadioTextList[tier - 1][TappedKey]) > 0)
			{
				trap_SendClientCommand( va("ut_radio %i %i", tier, TappedKey ));
			}
			cg.radiomenu = 0;
			//Click
			trap_S_StartLocalSound ( cgs.media.UT_radiomenuclick, CHAN_ANNOUNCER );
			return;
		}
	}

	//Render it in
	tier = cg.radiomenu - 1; //cg.radiomenu is 0 for empty

	if (tier == 0) //main menu
	{
		int  BorderHeight = 3;
		int  TotalHeight  = 19;
		CG_FillRect( startx, starty, startx + width, (BorderHeight * CharHeight), teamcol );
		CG_FillRect( startx, starty + (CharHeight * BorderHeight), startx + width, (TotalHeight - BorderHeight) * CharHeight, hcolor );

		CG_DrawRect( startx, starty, startx + width, (TotalHeight * CharHeight), 1, grey);
		CG_DrawRect( startx, starty, startx + width, (BorderHeight * CharHeight), 1, grey);

		Com_sprintf(text, 64, "^7Radio Commands");
		CG_DrawStringExt(startx + CharWidth, starty + CharHeight, text, white, qfalse, qtrue, CharWidth, CharHeight, 64);

		y = starty + (CharHeight * 4);

		for (j = 0; j < 9; j++)
		{
			Com_sprintf(text, 64, " ^7%i. ^7%s", j + 1, RadioTextList[j][0]);
			Q_strupr(text);
			CG_DrawStringExt(x, y, text, white, qfalse, qtrue, CharWidth, CharHeight, 64);
			y += CharHeight + 3;
		}

		y += CharHeight / 2;
		Com_sprintf(text, 64, " ^70. CANCEL");
		CG_DrawStringExt(x, y, text, white, qfalse, qtrue, CharWidth, CharHeight, 64);
		y += CharHeight + 2;
	}
	else //Sub Tiers
	{
		int  BorderHeight = 3;
		int  TotalHeight  = 19;
		CG_FillRect( startx, starty, startx + width, (BorderHeight * CharHeight), teamcol );
		CG_FillRect( startx, starty + (CharHeight * BorderHeight), startx + width, (TotalHeight - BorderHeight) * CharHeight, hcolor );

		CG_DrawRect( startx, starty, startx + width, (TotalHeight * CharHeight), 1, grey);
		CG_DrawRect( startx, starty, startx + width, (BorderHeight * CharHeight), 1, grey);

		Com_sprintf(text, 64, "^7%s", RadioTextList[tier - 1][0]);
		CG_DrawStringExt(startx + CharWidth, starty + CharHeight, text, white, qfalse, qtrue, CharWidth, CharHeight, 64);

		y = starty + (CharHeight * 4);

		for (j = 0; j < 9; j++)
		{
			if (strlen(RadioTextList[tier - 1][j + 1]) > 0)
			{
				Com_sprintf(text, 64, " ^7%i. ^7%s", j + 1, RadioTextList[tier - 1][j + 1]);
				Q_strupr(text);
				CG_DrawStringExt(x, y, text, white, qfalse, qtrue, CharWidth, CharHeight, 64);
			}
			y += CharHeight + 3;
		}
		y += CharHeight / 2;
		Com_sprintf(text, 64, " ^70. CANCEL");
		CG_DrawStringExt(x, y, text, white, qfalse, qtrue, CharWidth, CharHeight, 64);
		y += CharHeight + 2;
	}
}
