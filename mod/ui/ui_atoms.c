// Copyright (C) 1999-2000 Id Software, Inc.
//
/**********************************************************************
	UI_ATOMS.C

	User interface building blocks and support functions.
**********************************************************************/
#include "ui_local.h"
#include "../common/c_bmalloc.h"

qboolean  m_entersound; 			// after a frame, so caching won't disrupt the sound
char	  *ui_keyboardInterfaceOptions[MAX_STOPS];
int 	  currStopID;

qboolean  newUI = qfalse;

void QDECL Com_Error(int level, const char *error, ...)
{
	va_list 	argptr;
	char		text[1024];

	va_start(argptr, error);
	vsprintf(text, error, argptr);
	va_end(argptr);

	trap_Error(va("%s", text));
}

void QDECL Com_Printf(const char *msg, ...)
{
	va_list 	argptr;
	char		text[1024];

	va_start(argptr, msg);
	vsprintf(text, msg, argptr);
	va_end(argptr);

	trap_Print(va("%s", text));
}

/*
=================
UI_ClampCvar
=================
*/
float UI_ClampCvar( float min, float max, float value )
{
	if (value < min)
	{
		return min;
	}

	if (value > max)
	{
		return max;
	}
	return value;
}

/*
=================
UI_StartDemoLoop
=================
*/
void UI_StartDemoLoop( void )
{
	trap_Cmd_ExecuteText( EXEC_APPEND, "d1\n" );
}

/**
 * UI_Argv
 */
char *UI_Argv( int arg )
{
	static char  buffer[MAX_STRING_CHARS];

	trap_Argv( arg, buffer, sizeof(buffer));

	return buffer;
}

/**
 * UI_Cvar_VariableString
 */
char *UI_Cvar_VariableString( const char *var_name )
{
	static char  buffer[MAX_STRING_CHARS];

	trap_Cvar_VariableStringBuffer( var_name, buffer, sizeof(buffer));

	return buffer;
}

/**
 * UI_SetBestScores
 */
void UI_SetBestScores(postGameInfo_t *newInfo, qboolean postGame)
{
	trap_Cvar_Set("ui_scoreAccuracy", va("%i%%", newInfo->accuracy));
	trap_Cvar_Set("ui_scoreImpressives", va("%i", newInfo->impressives));
	trap_Cvar_Set("ui_scoreExcellents", va("%i", newInfo->excellents));
	trap_Cvar_Set("ui_scoreDefends", va("%i", newInfo->defends));
	trap_Cvar_Set("ui_scoreAssists", va("%i", newInfo->assists));
	trap_Cvar_Set("ui_scoreGauntlets", va("%i", newInfo->gauntlets));
	trap_Cvar_Set("ui_scoreScore", va("%i", newInfo->score));
	trap_Cvar_Set("ui_scorePerfect", va("%i", newInfo->perfects));
	trap_Cvar_Set("ui_scoreTeam", va("%i to %i", newInfo->redScore, newInfo->blueScore));
	trap_Cvar_Set("ui_scoreBase", va("%i", newInfo->baseScore));
	trap_Cvar_Set("ui_scoreTimeBonus", va("%i", newInfo->timeBonus));
	trap_Cvar_Set("ui_scoreSkillBonus", va("%i", newInfo->skillBonus));
	trap_Cvar_Set("ui_scoreShutoutBonus", va("%i", newInfo->shutoutBonus));
	trap_Cvar_Set("ui_scoreTime", va("%02i:%02i", newInfo->time / 60, newInfo->time % 60));
	trap_Cvar_Set("ui_scoreCaptures", va("%i", newInfo->captures));

	if (postGame)
	{
		trap_Cvar_Set("ui_scoreAccuracy2", va("%i%%", newInfo->accuracy));
		trap_Cvar_Set("ui_scoreImpressives2", va("%i", newInfo->impressives));
		trap_Cvar_Set("ui_scoreExcellents2", va("%i", newInfo->excellents));
		trap_Cvar_Set("ui_scoreDefends2", va("%i", newInfo->defends));
		trap_Cvar_Set("ui_scoreAssists2", va("%i", newInfo->assists));
		trap_Cvar_Set("ui_scoreGauntlets2", va("%i", newInfo->gauntlets));
		trap_Cvar_Set("ui_scoreScore2", va("%i", newInfo->score));
		trap_Cvar_Set("ui_scorePerfect2", va("%i", newInfo->perfects));
		trap_Cvar_Set("ui_scoreTeam2", va("%i to %i", newInfo->redScore, newInfo->blueScore));
		trap_Cvar_Set("ui_scoreBase2", va("%i", newInfo->baseScore));
		trap_Cvar_Set("ui_scoreTimeBonus2", va("%i", newInfo->timeBonus));
		trap_Cvar_Set("ui_scoreSkillBonus2", va("%i", newInfo->skillBonus));
		trap_Cvar_Set("ui_scoreShutoutBonus2", va("%i", newInfo->shutoutBonus));
		trap_Cvar_Set("ui_scoreTime2", va("%02i:%02i", newInfo->time / 60, newInfo->time % 60));
		trap_Cvar_Set("ui_scoreCaptures2", va("%i", newInfo->captures));
	}
}

/**
 * UI_LoadBestScores
 */
void UI_LoadBestScores(const char *map, int game)
{
	char		fileName[MAX_QPATH];
	fileHandle_t	f;
	postGameInfo_t	newInfo;

	memset(&newInfo, 0, sizeof(postGameInfo_t));
	Com_sprintf(fileName, MAX_QPATH, "games/%s_%i.game", map, game);

	if (trap_FS_FOpenFile(fileName, &f, FS_READ) >= 0)
	{
		int  size = 0;
		trap_FS_Read(&size, sizeof(int), f);

		if (size == sizeof(postGameInfo_t))
		{
			trap_FS_Read(&newInfo, sizeof(postGameInfo_t), f);
		}
		trap_FS_FCloseFile(f);
	}
	UI_SetBestScores(&newInfo, qfalse);

	Com_sprintf(fileName, MAX_QPATH, "demos/%s_%d.urtdemo", map, game );
	uiInfo.demoAvailable = qfalse;

	if (trap_FS_FOpenFile(fileName, &f, FS_READ) >= 0)
	{
		uiInfo.demoAvailable = qtrue;
		trap_FS_FCloseFile(f);
	}
}

/*
===============
UI_ClearScores
===============
*/
void UI_ClearScores(void)
{
	char			gameList[4096];
	char			 *gameFile;
	int 			i, len, count, size;
	fileHandle_t	f;
	postGameInfo_t	newInfo;

	count = trap_FS_GetFileList( "games", "game", gameList, sizeof(gameList));

	size  = sizeof(postGameInfo_t);
	memset(&newInfo, 0, size);

	if (count > 0)
	{
		gameFile = gameList;

		for (i = 0; i < count; i++)
		{
			len = strlen(gameFile);

			if (trap_FS_FOpenFile(va("games/%s", gameFile), &f, FS_WRITE) >= 0)
			{
				trap_FS_Write(&size, sizeof(int), f);
				trap_FS_Write(&newInfo, size, f);
				trap_FS_FCloseFile(f);
			}
			gameFile += len + 1;
		}
	}

	UI_SetBestScores(&newInfo, qfalse);
}

/**
 * $(function)
 */
static void UI_Cache_f(void)
{
	Display_CacheAll();
}

/*
=======================
UI_CalcPostGameStats
=======================
*/
static void UI_CalcPostGameStats(void)
{
	char			map[MAX_QPATH];
	char			fileName[MAX_QPATH];
	char			info[MAX_INFO_STRING];
	fileHandle_t	f;
	int 			size, game, time, adjustedTime;
	postGameInfo_t	oldInfo;
	postGameInfo_t	newInfo;
	qboolean		newHigh = qfalse;

	trap_GetConfigString( CS_SERVERINFO, info, sizeof(info));
	Q_strncpyz( map, Info_ValueForKey( info, "mapname" ), sizeof(map));
	game = atoi(Info_ValueForKey(info, "g_gametype"));

	// compose file name
	Com_sprintf(fileName, MAX_QPATH, "games/%s_%i.game", map, game);
	// see if we have one already
	memset(&oldInfo, 0, sizeof(postGameInfo_t));

	if (trap_FS_FOpenFile(fileName, &f, FS_READ) >= 0)
	{
		// if so load it
		size = 0;
		trap_FS_Read(&size, sizeof(int), f);

		if (size == sizeof(postGameInfo_t))
		{
			trap_FS_Read(&oldInfo, sizeof(postGameInfo_t), f);
		}
		trap_FS_FCloseFile(f);
	}

	newInfo.accuracy	= atoi(UI_Argv(3));
	newInfo.impressives = atoi(UI_Argv(4));
	newInfo.excellents	= atoi(UI_Argv(5));
	newInfo.defends 	= atoi(UI_Argv(6));
	newInfo.assists 	= atoi(UI_Argv(7));
	newInfo.gauntlets	= atoi(UI_Argv(8));
	newInfo.baseScore	= atoi(UI_Argv(9));
	newInfo.perfects	= atoi(UI_Argv(10));
	newInfo.redScore	= atoi(UI_Argv(11));
	newInfo.blueScore	= atoi(UI_Argv(12));
	time			= atoi(UI_Argv(13));
	newInfo.captures	= atoi(UI_Argv(14));

	newInfo.time		= (time - trap_Cvar_VariableValue("ui_matchStartTime")) / 1000;
	adjustedTime		= uiInfo.mapList[ui_currentMap.integer].timeToBeat[game];

	if (newInfo.time < adjustedTime)
	{
		newInfo.timeBonus = (adjustedTime - newInfo.time) * 10;
	}
	else
	{
		newInfo.timeBonus = 0;
	}

	if ((newInfo.redScore > newInfo.blueScore) && (newInfo.blueScore <= 0))
	{
		newInfo.shutoutBonus = 100;
	}
	else
	{
		newInfo.shutoutBonus = 0;
	}

	newInfo.skillBonus = trap_Cvar_VariableValue("g_spSkill");

	if (newInfo.skillBonus <= 0)
	{
		newInfo.skillBonus = 1;
	}
	newInfo.score  = newInfo.baseScore + newInfo.shutoutBonus + newInfo.timeBonus;
	newInfo.score *= newInfo.skillBonus;

	// see if the score is higher for this one
	newHigh = (newInfo.redScore > newInfo.blueScore && newInfo.score > oldInfo.score);

	if	(newHigh)
	{
		// if so write out the new one
		uiInfo.newHighScoreTime = uiInfo.uiDC.realTime + 20000;

		if (trap_FS_FOpenFile(fileName, &f, FS_WRITE) >= 0)
		{
			size = sizeof(postGameInfo_t);
			trap_FS_Write(&size, sizeof(int), f);
			trap_FS_Write(&newInfo, sizeof(postGameInfo_t), f);
			trap_FS_FCloseFile(f);
		}
	}

	if (newInfo.time < oldInfo.time)
	{
		uiInfo.newBestTime = uiInfo.uiDC.realTime + 20000;
	}

	// put back all the ui overrides
	trap_Cvar_Set("capturelimit", UI_Cvar_VariableString("ui_saveCaptureLimit"));
	trap_Cvar_Set("fraglimit", UI_Cvar_VariableString("ui_saveFragLimit"));
	trap_Cvar_Set("cg_drawTimer", UI_Cvar_VariableString("ui_drawTimer"));
	trap_Cvar_Set("g_doWarmup", UI_Cvar_VariableString("ui_doWarmup"));
	trap_Cvar_Set("g_Warmup", UI_Cvar_VariableString("ui_Warmup"));
	trap_Cvar_Set("sv_pure", UI_Cvar_VariableString("ui_pure"));
	trap_Cvar_Set("g_friendlyFire", UI_Cvar_VariableString("ui_friendlyFire"));

	UI_SetBestScores(&newInfo, qtrue);
	UI_ShowPostGame(newHigh);
}

/*
=================
UI_ConsoleCommand
=================
*/
qboolean UI_ConsoleCommand( int realTime )
{
	char  *cmd;
	char  *arg;
	char  *ki;
	char  *s;
	int   i = 0;

	uiInfo.uiDC.frameTime = realTime - uiInfo.uiDC.realTime;
	uiInfo.uiDC.realTime  = realTime;

	cmd 		  = UI_Argv( 0 );

	// ensure minimum menu data is available
	//Menu_Cache();


	#ifdef USE_AUTH
	//@Barbatos //@Kalish
	if( Q_stricmp (cmd, AUTH_CMD_CL) == 0 )
	{
		CL_Auth_Cmd_f( "__USE_ARGV__" );
		return qtrue;
	}
	#endif


	if (Q_stricmp (cmd, "ui_test") == 0)
	{
		UI_ShowPostGame(qtrue);
	}

	if (Q_stricmp (cmd, "ui_report") == 0)
	{
		UI_Report();
		return qtrue;
	}

	if (Q_stricmp (cmd, "ui_load") == 0)
	{
		UI_Load();
		return qtrue;
	}

	if (Q_stricmp (cmd, "remapShader") == 0)
	{
		if (trap_Argc() == 4)
		{
			char  shader1[MAX_QPATH];
			char  shader2[MAX_QPATH];
			Q_strncpyz(shader1, UI_Argv(1), sizeof(shader1));
			Q_strncpyz(shader2, UI_Argv(2), sizeof(shader2));
			trap_R_RemapShader(shader1, shader2, UI_Argv(3));
			return qtrue;
		}
	}

	if (Q_stricmp (cmd, "postgame") == 0)
	{
		UI_CalcPostGameStats();
		return qtrue;
	}

	if (Q_stricmp (cmd, "ui_cache") == 0)
	{
		UI_Cache_f();
		return qtrue;
	}

	if (Q_stricmp (cmd, "ui_teamOrders") == 0)
	{
		//UI_TeamOrdersMenu_f();
		return qtrue;
	}

	if (Q_stricmp (cmd, "ui_cdkey") == 0)
	{
		//UI_CDKeyMenu_f();
		return qtrue;
	}

	if (Q_stricmp (cmd, "ui_selectteam") == 0)
	{
		_UI_SetActiveMenu(UIMENU_SELECT_TEAM);
		return qtrue;
	}

	if (Q_stricmp (cmd, "ui_selectgear") == 0)
	{
		_UI_SetActiveMenu(UIMENU_SELECT_GEAR);
		return qtrue;
	}

/*	if ( Q_stricmp (cmd, "ui_radio") == 0 )
   {
		if ( UI_Argv( 1 ) && *UI_Argv( 1 ) != 0 )
		{
			switch ( atoi(UI_Argv( 1 )))
			{
				case 0:
					_UI_SetActiveMenu(UIMENU_RADIO_0);
					break;

				case 1:
					_UI_SetActiveMenu(UIMENU_RADIO_1);
					break;

				case 2:
					_UI_SetActiveMenu(UIMENU_RADIO_2);
					break;

				case 3:
					_UI_SetActiveMenu(UIMENU_RADIO_3);
					break;
			}
		}
		else
			_UI_SetActiveMenu(UIMENU_RADIO);

		return qtrue;
	} */

	if (Q_stricmp (cmd, "ui_keyboardinterface") == 0)
	{
		currStopID = atoi(UI_Argv( 1 ));

		for (i = 0; i < MAX_STOPS; i++)
		{
			arg = UI_Argv(i+2);
			ui_keyboardInterfaceOptions[i] = bmalloc(strlen(arg));
			strcpy(ui_keyboardInterfaceOptions[i], arg);
			if (ui_debug.integer)
				Com_Printf("Choice %d: %s\n", i, arg);

			if (arg)
			{
				for (s = arg, ki = ui_keyboardInterfaceOptions[i]; *s; s++,ki++)
					*ki = (*s == '$' ? ' ' : *s);
			}
		}

		_UI_SetActiveMenu(UIMENU_KEYBOARD_INTERFACE);
		return qtrue;
	}

	return qfalse;
}

/**
 * UI_Shutdown
 */
void UI_Shutdown( void )
{
}

/**
 * UI_AdjustFrom640
 *
 * Adjusted for resolution and screen aspect ratio
 */
void UI_AdjustFrom640( float *x, float *y, float *w, float *h )
{
	*x *= uiInfo.uiDC.xscale;
	*y *= uiInfo.uiDC.yscale;
	*w *= uiInfo.uiDC.xscale;
	*h *= uiInfo.uiDC.yscale;
}

/**
 * UI_DrawNamedPic
 */
void UI_DrawNamedPic( float x, float y, float width, float height, const char *picname )
{
	qhandle_t  hShader;

	hShader = trap_R_RegisterShaderNoMip( picname );
	UI_AdjustFrom640( &x, &y, &width, &height );
	trap_R_DrawStretchPic( x, y, width, height, 0, 0, 1, 1, hShader );
}

/**
 * UI_DrawHandlePic
 */
void UI_DrawHandlePic( float x, float y, float w, float h, qhandle_t hShader )
{
	float  s0;
	float  s1;
	float  t0;
	float  t1;

	if(w < 0)	// flip about vertical
	{
		w  = -w;
		s0 = 1;
		s1 = 0;
	}
	else
	{
		s0 = 0;
		s1 = 1;
	}

	if(h < 0)	// flip about horizontal
	{
		h  = -h;
		t0 = 1;
		t1 = 0;
	}
	else
	{
		t0 = 0;
		t1 = 1;
	}

	UI_AdjustFrom640( &x, &y, &w, &h );
	trap_R_DrawStretchPic( x, y, w, h, s0, t0, s1, t1, hShader );
}

/**
 * UI_FillRect
 *
 * Coordinates are 640*480 virtual values
 */
void UI_FillRect( float x, float y, float width, float height, const float *color )
{
	trap_R_SetColor( color );

	UI_AdjustFrom640( &x, &y, &width, &height );
	trap_R_DrawStretchPic( x, y, width, height, 0, 0, 0, 0, uiInfo.uiDC.whiteShader );

	trap_R_SetColor( NULL );
}

/**
 * UI_DrawSides
 */
void UI_DrawSides(float x, float y, float w, float h)
{
	UI_AdjustFrom640( &x, &y, &w, &h );
	trap_R_DrawStretchPic( x, y, 1, h, 0, 0, 0, 0, uiInfo.uiDC.whiteShader );
	trap_R_DrawStretchPic( x + w - 1, y, 1, h, 0, 0, 0, 0, uiInfo.uiDC.whiteShader );
}

/**
 * UI_DrawTopBottom
 */
void UI_DrawTopBottom(float x, float y, float w, float h)
{
	UI_AdjustFrom640( &x, &y, &w, &h );
	trap_R_DrawStretchPic( x, y, w, 1, 0, 0, 0, 0, uiInfo.uiDC.whiteShader );
	trap_R_DrawStretchPic( x, y + h - 1, w, 1, 0, 0, 0, 0, uiInfo.uiDC.whiteShader );
}
/**
 * UI_DrawRect
 *
 * Coordinates are 640*480 virtual values
 */
void UI_DrawRect( float x, float y, float width, float height, const float *color )
{
	trap_R_SetColor( color );

	UI_DrawTopBottom(x, y, width, height);
	UI_DrawSides(x, y, width, height);

	trap_R_SetColor( NULL );
}

/**
 * UI_SetColor
 */
void UI_SetColor( const float *rgba )
{
	trap_R_SetColor( rgba );
}

/**
 * UI_UpdateScreen
 */
void UI_UpdateScreen( void )
{
	trap_UpdateScreen();
}

/**
 * UI_DrawTextBox
 */
void UI_DrawTextBox (int x, int y, int width, int lines)
{
	UI_FillRect( x + BIGCHAR_WIDTH / 2, y + BIGCHAR_HEIGHT / 2, (width + 1) * BIGCHAR_WIDTH, (lines + 1) * BIGCHAR_HEIGHT, colorBlack );
	UI_DrawRect( x + BIGCHAR_WIDTH / 2, y + BIGCHAR_HEIGHT / 2, (width + 1) * BIGCHAR_WIDTH, (lines + 1) * BIGCHAR_HEIGHT, colorWhite );
}

/**
 * UI_CursorInRect
 */
qboolean UI_CursorInRect (int x, int y, int width, int height)
{
	if ((uiInfo.uiDC.cursorx < x) ||
		(uiInfo.uiDC.cursory < y) ||
		(uiInfo.uiDC.cursorx > x + width) ||
		(uiInfo.uiDC.cursory > y + height))
	{
		return qfalse;
	}

	return qtrue;
}

/**
 * UI_Text_Width
 */
int UI_Text_Width(const char *text, float scale, int limit)
{
	int 	 count, len;
	float		 out;
	glyphInfo_t  *glyph;
	float		 useScale;
	const byte	 *s    = (byte *)text;
	fontInfo_t	 *font = &uiInfo.uiDC.Assets.textFont;

	if (scale <= ui_smallFont.value)
	{
		font = &uiInfo.uiDC.Assets.smallFont;
	}
	else if (scale >= ui_bigFont.value)
	{
		font = &uiInfo.uiDC.Assets.bigFont;
	}
	useScale = scale * font->glyphScale;
	out  = 0;

	if (text)
	{
		len = strlen(text);

		if ((limit > 0) && (len > limit))
		{
			len = limit;
		}
		count = 0;

		while (s && *s && count < len)
		{
			if (Q_IsColorString(s))
			{
				s += 2;
				continue;
			}
			else
			{
				glyph = &font->glyphs[*s];
				out  += glyph->xSkip;
				s++;
				count++;
			}
		}
	}
	return out * useScale;
}

/**
 * UI_Text_Height
 */
int UI_Text_Height(const char *text, float scale, int limit)
{
	int 	 len, count;
	float		 max;
	glyphInfo_t  *glyph;
	float		 useScale;
	const byte	 *s    = (byte *)text;
	fontInfo_t	 *font = &uiInfo.uiDC.Assets.textFont;

	if (scale <= ui_smallFont.value)
	{
		font = &uiInfo.uiDC.Assets.smallFont;
	}
	else if (scale >= ui_bigFont.value)
	{
		font = &uiInfo.uiDC.Assets.bigFont;
	}
	useScale = scale * font->glyphScale;
	max  = 0;

	if (text)
	{
		len = strlen(text);

		if ((limit > 0) && (len > limit))
		{
			len = limit;
		}
		count = 0;

		while (s && *s && count < len)
		{
			if (Q_IsColorString(s))
			{
				s += 2;
				continue;
			}
			else
			{
				glyph = &font->glyphs[*s];

				if (max < glyph->height)
				{
					max = glyph->height;
				}
				s++;
				count++;
			}
		}
	}
	return max * useScale;
}

/**
 * UI_Text_PaintChar
 */
void UI_Text_PaintChar(float x, float y, float width, float height, float scale, float s, float t, float s2, float t2, qhandle_t hShader)
{
	float  w, h;

	w = width * scale;
	h = height * scale;
	UI_AdjustFrom640( &x, &y, &w, &h );
	trap_R_DrawStretchPic( x, y, w, h, s, t, s2, t2, hShader );
}

/**
 * UI_Text_Paint
 */
void UI_Text_Paint(float x, float y, float scale, vec4_t color, const char *text, float adjust, int limit, int style)
{
	int 		 len, count;
	vec4_t		 newColor;
	glyphInfo_t  *glyph;
	float		 useScale;
	fontInfo_t	 *font = &uiInfo.uiDC.Assets.textFont;

	if (scale <= ui_smallFont.value)
	{
		font = &uiInfo.uiDC.Assets.smallFont;
	}
	else if (scale >= ui_bigFont.value)
	{
		font = &uiInfo.uiDC.Assets.bigFont;
	}
	useScale = scale * font->glyphScale;

	if (text)
	{
		const byte	*s = (byte *)text;
		trap_R_SetColor( color );
		memcpy(&newColor[0], &color[0], sizeof(vec4_t));
		len = strlen(text);

		if (limit > len || limit <= 0)
			limit = len;

		count = 0;

		while (s && *s && count < limit)
		{
			glyph = &font->glyphs[*s];

			//int yadj = Assets.textFont.glyphs[text[i]].bottom + Assets.textFont.glyphs[text[i]].top;
			//float yadj = scale * (Assets.textFont.glyphs[text[i]].imageHeight - Assets.textFont.glyphs[text[i]].height);
			if (Q_IsColorString( s ))
			{
				memcpy( newColor, g_color_table[ColorIndex(*(s + 1))], sizeof(newColor));
				newColor[3] = color[3];
				trap_R_SetColor( newColor );
				s += 2;
				continue;
			}
			else
			{
				float  yadj = useScale * glyph->top;

				if ((style == ITEM_TEXTSTYLE_SHADOWED) || (style == ITEM_TEXTSTYLE_SHADOWEDMORE))
				{
					int  ofs = style == ITEM_TEXTSTYLE_SHADOWED ? 1 : 2;
					colorBlack[3] = newColor[3];
					trap_R_SetColor( colorBlack );
					UI_Text_PaintChar(x + ofs, y - yadj + ofs,
							   glyph->imageWidth,
							   glyph->imageHeight,
							   useScale,
							   glyph->s,
							   glyph->t,
							   glyph->s2,
							   glyph->t2,
							   glyph->glyph);
					trap_R_SetColor( newColor );
					colorBlack[3] = 1.0;
				}
				UI_Text_PaintChar(x, y - yadj,
						   glyph->imageWidth,
						   glyph->imageHeight,
						   useScale,
						   glyph->s,
						   glyph->t,
						   glyph->s2,
						   glyph->t2,
						   glyph->glyph);

				x += (glyph->xSkip * useScale) + adjust;
				s++;
				count++;
			}
		}
		trap_R_SetColor( NULL );
	}
}

/**
 * UI_Text_PaintWithCursor
 */
void UI_Text_PaintWithCursor(float x, float y, float scale, vec4_t color, const char *text, int cursorPos, char cursor, int limit, int style)
{
	int 		 len, count;
	vec4_t		 newColor;
	glyphInfo_t  *glyph, *glyph2;
	float		 yadj;
	float		 useScale;
	fontInfo_t	 *font = &uiInfo.uiDC.Assets.textFont;

	if (scale <= ui_smallFont.value)
	{
		font = &uiInfo.uiDC.Assets.smallFont;
	}
	else if (scale >= ui_bigFont.value)
	{
		font = &uiInfo.uiDC.Assets.bigFont;
	}
	useScale = scale * font->glyphScale;

	if (text)
	{
		const byte	*s = (byte *)text;
		trap_R_SetColor( color );
		memcpy(&newColor[0], &color[0], sizeof(vec4_t));
		len = strlen(text);

		if ((limit > 0) && (len > limit))
		{
			len = limit;
		}
		count  = 0;
		glyph2 = &font->glyphs[(byte)cursor];

		while (s && *s && count < len)
		{
			glyph = &font->glyphs[*s];

			//int yadj = Assets.textFont.glyphs[text[i]].bottom + Assets.textFont.glyphs[text[i]].top;
			//float yadj = scale * (Assets.textFont.glyphs[text[i]].imageHeight - Assets.textFont.glyphs[text[i]].height);
			if (Q_IsColorString( s ))
			{
				memcpy( newColor, g_color_table[ColorIndex(*(s + 1))], sizeof(newColor));
				newColor[3] = color[3];
				trap_R_SetColor( newColor );
				s	   += 2;
				continue;
			}
			else
			{
				yadj = useScale * glyph->top;

				if ((style == ITEM_TEXTSTYLE_SHADOWED) || (style == ITEM_TEXTSTYLE_SHADOWEDMORE))
				{
					int  ofs = style == ITEM_TEXTSTYLE_SHADOWED ? 1 : 2;
					colorBlack[3] = newColor[3];
					trap_R_SetColor( colorBlack );
					UI_Text_PaintChar(x + ofs, y - yadj + ofs,
							   glyph->imageWidth,
							   glyph->imageHeight,
							   useScale,
							   glyph->s,
							   glyph->t,
							   glyph->s2,
							   glyph->t2,
							   glyph->glyph);
					colorBlack[3] = 1.0;
					trap_R_SetColor( newColor );
				}
				UI_Text_PaintChar(x, y - yadj,
						   glyph->imageWidth,
						   glyph->imageHeight,
						   useScale,
						   glyph->s,
						   glyph->t,
						   glyph->s2,
						   glyph->t2,
						   glyph->glyph);

				// CG_DrawPic(x, y - yadj, scale * uiDC.Assets.textFont.glyphs[text[i]].imageWidth, scale * uiDC.Assets.textFont.glyphs[text[i]].imageHeight, uiDC.Assets.textFont.glyphs[text[i]].glyph);
				yadj = useScale * glyph2->top;

				if ((count == cursorPos) && !((uiInfo.uiDC.realTime / BLINK_DIVISOR) & 1))
				{
					UI_Text_PaintChar(x, y - yadj,
							   glyph2->imageWidth,
							   glyph2->imageHeight,
							   useScale,
							   glyph2->s,
							   glyph2->t,
							   glyph2->s2,
							   glyph2->t2,
							   glyph2->glyph);
				}

				x += (glyph->xSkip * useScale);
				s++;
				count++;
			}
		}

		// need to paint cursor at end of text
		if ((cursorPos == len) && !((uiInfo.uiDC.realTime / BLINK_DIVISOR) & 1))
		{
			yadj = useScale * glyph2->top;
			UI_Text_PaintChar(x, y - yadj,
					   glyph2->imageWidth,
					   glyph2->imageHeight,
					   useScale,
					   glyph2->s,
					   glyph2->t,
					   glyph2->s2,
					   glyph2->t2,
					   glyph2->glyph);
		}

		trap_R_SetColor( NULL );
	}
}

/**
 * UI_Text_Paint_Limit
 */
void UI_Text_Paint_Limit(float *maxX, float x, float y, float scale, vec4_t color, const char *text, float adjust, int limit)
{
	int 		 len, count;
	vec4_t		 newColor;
	glyphInfo_t  *glyph;

	if (text)
	{
		const byte	*s	  = (byte *)text;
		float		max   = *maxX;
		float		useScale;
		fontInfo_t	*font = &uiInfo.uiDC.Assets.textFont;

		if (scale <= ui_smallFont.value)
		{
			font = &uiInfo.uiDC.Assets.smallFont;
		}
		else if (scale > ui_bigFont.value)
		{
			font = &uiInfo.uiDC.Assets.bigFont;
		}
		useScale = scale * font->glyphScale;
		trap_R_SetColor( color );
		len  = strlen(text);

		if ((limit > 0) && (len > limit))
		{
			len = limit;
		}
		count = 0;

		while (s && *s && count < len)
		{
			glyph = &font->glyphs[*s];

			if (Q_IsColorString( s ))
			{
				memcpy( newColor, g_color_table[ColorIndex(*(s + 1))], sizeof(newColor));
				newColor[3] = color[3];
				trap_R_SetColor( newColor );
				s	   += 2;
				continue;
			}
			else
			{
				float  yadj = useScale * glyph->top;

				if (UI_Text_Width((char *)s, useScale, 1) + x > max)
				{
					*maxX = 0;
					break;
				}
				UI_Text_PaintChar(x, y - yadj,
						   glyph->imageWidth,
						   glyph->imageHeight,
						   useScale,
						   glyph->s,
						   glyph->t,
						   glyph->s2,
						   glyph->t2,
						   glyph->glyph);
				x	 += (glyph->xSkip * useScale) + adjust;
				*maxX = x;
				count++;
				s++;
			}
		}
		trap_R_SetColor( NULL );
	}
}

