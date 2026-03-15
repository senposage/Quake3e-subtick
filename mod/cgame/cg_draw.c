// Copyright (C) 1999-2000 Id Software, Inc.
//
// cg_draw.c -- draw all of the graphical elements during
// active (after loading) gameplay

#include "cg_local.h"
#include "../ui/ui_shared.h"

// used for scoreboard
extern displayContextDef_t	cgDC;
menuDef_t					*menuScoreboard = NULL;

const int					RADAR_RADIUS	= 32;
extern vec3_t				bg_colors[20];

static qboolean CG_DrawSurvivorRoundWinner (void);
static qboolean CG_DrawSPGameTimers 	   (void);
static void CG_DrawMiniScoreboard		   (void);
//static void CG_DrawRadar				   (float x, float y);
static void CG_DrawPauseStatus			   (void);
static void CG_DrawMatchStatus			   (void);

/**
 * $(function)
 */
int CG_Text_Width(const char *text, float scale, int limit)
{
	int 		 count, len;
	float		 out;
	glyphInfo_t  *glyph;
	float		 useScale;
	const char	 *s    = text;
	fontInfo_t	 *font = &cgDC.Assets.textFont;

	if (scale <= cg_smallFont.value)
	{
		font = &cgDC.Assets.smallFont;
	}
	else if (scale > cg_bigFont.value)
	{
		font = &cgDC.Assets.bigFont;
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
				glyph = &font->glyphs[(int)*s];
				out  += glyph->xSkip;
				s++;
				count++;
			}
		}
	}
	return out * useScale;
}

/**
 * $(function)
 */
int CG_Text_Height(const char *text, float scale, int limit)
{
	int 		 len, count;
	float		 max;
	glyphInfo_t  *glyph;
	float		 useScale;
	const char	 *s    = text;
	fontInfo_t	 *font = &cgDC.Assets.textFont;

	if (scale <= cg_smallFont.value)
	{
		font = &cgDC.Assets.smallFont;
	}
	else if (scale > cg_bigFont.value)
	{
		font = &cgDC.Assets.bigFont;
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
				glyph = &font->glyphs[(int)*s];

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
 * $(function)
 */
void CG_Text_PaintChar(float x, float y, float width, float height, float scale, float s, float t, float s2, float t2, qhandle_t hShader)
{
	float  w, h;

	w = width * scale;
	h = height * scale;
	CG_AdjustFrom640( &x, &y, &w, &h );
	trap_R_DrawStretchPic( x, y, w, h, s, t, s2, t2, hShader );
}

/**
 * $(function)
 */
void CG_Text_Paint(float x, float y, float scale, vec4_t color, const char *text, float adjust, int limit, int style)
{
	int 	 len, count;
	vec4_t		 newColor;
	glyphInfo_t  *glyph;
	float		 useScale;
	fontInfo_t	 *font = &cgDC.Assets.textFont;

	if (scale <= cg_smallFont.value)
	{
		font = &cgDC.Assets.smallFont;
	}
	else if (scale > cg_bigFont.value)
	{
		font = &cgDC.Assets.bigFont;
	}
	useScale = scale * font->glyphScale;

	if (text)
	{
		const char	*s = text;
		trap_R_SetColor( color );
		memcpy(&newColor[0], &color[0], sizeof(vec4_t));
		len = strlen(text);

		if ((limit > 0) && (len > limit))
		{
			len = limit;
		}
		count = 0;

		while (s && *s && count < len)
		{
			glyph = &font->glyphs[(int)*s];

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
				float  yadj = useScale * glyph->top;

				if ((style == ITEM_TEXTSTYLE_SHADOWED) || (style == ITEM_TEXTSTYLE_SHADOWEDMORE) || (style == ITEM_TEXTSTYLE_OUTLINED))
				{
					int  ofs = style == ITEM_TEXTSTYLE_SHADOWEDMORE ? 2 : 1;
					colorBlack[3] = newColor[3];
					trap_R_SetColor( colorBlack );
					CG_Text_PaintChar(x + ofs, y - yadj + ofs,
							  glyph->imageWidth,
							  glyph->imageHeight,
							  useScale,
							  glyph->s,
							  glyph->t,
							  glyph->s2,
							  glyph->t2,
							  glyph->glyph);

					if(style == ITEM_TEXTSTYLE_OUTLINED)
					{
						CG_Text_PaintChar(x - ofs, y - yadj - ofs,
								  glyph->imageWidth,
								  glyph->imageHeight,
								  useScale,
								  glyph->s,
								  glyph->t,
								  glyph->s2,
								  glyph->t2,
								  glyph->glyph);
						CG_Text_PaintChar(x + ofs, y - yadj - ofs,
								  glyph->imageWidth,
								  glyph->imageHeight,
								  useScale,
								  glyph->s,
								  glyph->t,
								  glyph->s2,
								  glyph->t2,
								  glyph->glyph);
						CG_Text_PaintChar(x - ofs, y - yadj + ofs,
								  glyph->imageWidth,
								  glyph->imageHeight,
								  useScale,
								  glyph->s,
								  glyph->t,
								  glyph->s2,
								  glyph->t2,
								  glyph->glyph);
					}

					colorBlack[3] = 1.0;
					trap_R_SetColor( newColor );
				}
				CG_Text_PaintChar(x, y - yadj,
						  glyph->imageWidth,
						  glyph->imageHeight,
						  useScale,
						  glyph->s,
						  glyph->t,
						  glyph->s2,
						  glyph->t2,
						  glyph->glyph);
				// CG_DrawPic(x, y - yadj, scale * cgDC.Assets.textFont.glyphs[text[i]].imageWidth, scale * cgDC.Assets.textFont.glyphs[text[i]].imageHeight, cgDC.Assets.textFont.glyphs[text[i]].glyph);
				x += (glyph->xSkip * useScale) + adjust;
				s++;
				count++;
			}
		}
		trap_R_SetColor( NULL );
	}
}

/*
================
CG_Draw3DModel

================
*/
void CG_Draw3DModel( float x, float y, float w, float h, qhandle_t model, qhandle_t skin, vec3_t origin, vec3_t angles )
{
	refdef_t	 refdef;
	refEntity_t  ent;

	CG_AdjustFrom640( &x, &y, &w, &h );

	memset( &refdef, 0, sizeof(refdef));

	memset( &ent, 0, sizeof(ent));
	AnglesToAxis( angles, ent.axis );
	VectorCopy( origin, ent.origin );
	ent.hModel	   = model;
	ent.customSkin = skin;
	ent.renderfx   = RF_NOSHADOW;		// no stencil shadows

	refdef.rdflags = RDF_NOWORLDMODEL;

	AxisClear( refdef.viewaxis );

	refdef.fov_x  = 30;
	refdef.fov_y  = 30;

	refdef.x	  = x;
	refdef.y	  = y;
	refdef.width  = w;
	refdef.height = h;

	refdef.time   = cg.time;

	trap_R_ClearScene();
	trap_R_AddRefEntityToSceneX( &ent );
	trap_R_RenderScene( &refdef );
}

/*
================
CG_DrawHead

Used for both the status bar and the scoreboard
================
*/
void CG_DrawHead( float x, float y, float w, float h, int clientNum, vec3_t headAngles )
{
	clipHandle_t  cm;
	clientInfo_t  *ci;
	float		  len;
	vec3_t		  origin;
	vec3_t		  mins, maxs;

	ci = &cgs.clientinfo[clientNum];

	cm = ci->headModel;

	if (!cm)
	{
		return;
	}

	// offset the origin y and z to center the head
	trap_R_ModelBounds( cm, mins, maxs );

	origin[2] = -0.5 * (mins[2] + maxs[2]);
	origin[1] = 0.5 * (mins[1] + maxs[1]);

	// calculate distance so the head nearly fills the box
	// assume heads are taller than wide
	len   = 0.7 * (maxs[2] - mins[2]);
	origin[0] = len / 0.268;	// len / tan( fov/2 )

	CG_Draw3DModel( x, y, w, h, ci->headModel, ci->headSkin, origin, headAngles );
}

/*
================
CG_DrawFlagModel

Used for both the status bar and the scoreboard
================
*/
void CG_DrawFlagModel( float x, float y, float w, float h, int team, qboolean force2D )
{
	float	   len;
	vec3_t	   origin, angles;
	vec3_t	   mins, maxs;
	qhandle_t  handle;

	if (!force2D)
	{
		VectorClear( angles );

		if (cgs.gametype == GT_JUMP) {
			handle = cgs.media.flagModels[(team == TEAM_RED) ? UT_SKIN_RED : UT_SKIN_BLUE];
		} else {
			handle = cgs.media.flagModels[CG_WhichSkin(team)];
		}

		// offset the origin y and z to center the flag
		trap_R_ModelBounds( handle, mins, maxs );

		origin[2] = -0.5 * (mins[2] + maxs[2]);
		origin[1] = 0.5 * (mins[1] + maxs[1]);

		// calculate distance so the flag nearly fills the box
		// assume heads are taller than wide
		len 	= 0.5 * (maxs[2] - mins[2]);
		origin[0]	= len / 0.268;	// len / tan( fov/2 )

		angles[YAW] = 60 * sin( cg.time / 2000.0 );

		CG_Draw3DModel( x, y, w, h, handle, 0, origin, angles );
	}
}

/*
==================
CG_DrawSnapshot
==================
*/
#if 0
static float CG_DrawSnapshot( float y )
{
	char  *s;
	int   w;

	s = va( "time:%i snap:%i cmd:%i", cg.snap->serverTime,
		cg.latestSnapshotNum, cgs.serverCommandSequence );
	w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH;

	CG_DrawBigString( 635 - w, y + 2, s, 1.0F);

	return y + BIGCHAR_HEIGHT + 4;
}
#endif

/*
==================
CG_DrawFPS
==================
*/
#define FPS_FRAMES 4
/**
 * $(function)
 */
static int CG_CalcFPS( void )
{
	static int	previousTimes[FPS_FRAMES];
	static int	index;
	int 	i, total;
	int 	fps;
	static int	previous;
	int 	t, frameTime;

	// don't use serverTime, because that will be drifting to
	// correct for internet lag changes, timescales, timedemos, etc
	t				  = trap_Milliseconds();
	frameTime			  = t - previous;
	previous			  = t;

	previousTimes[index % FPS_FRAMES] = frameTime;
	index++;

	if (index > FPS_FRAMES)
	{
		// average multiple frames together to smooth changes out a bit
		total = 0;

		for (i = 0 ; i < FPS_FRAMES ; i++)
		{
			total += previousTimes[i];
		}

		if (!total)
		{
			total = 1;
		}
		fps    = 1000 * FPS_FRAMES / total;
		cg.fps = fps;
		return fps;
	}

	return 0;
}

/**
 * $(function)
 */
static char *GetSurvivorTimer(void)
{
	int  mins_r, secs_r, tens_r;
	int  mins_l, secs_l, tens_l;
	int  misecs;

	misecs = (cgs.roundEndTime - cg.time);

	if (misecs > 0)
	{
		secs_r	= misecs / 1000;
		mins_r	= secs_r / 60;
		secs_r -= mins_r * 60;
		tens_r	= secs_r / 10;
		secs_r -= tens_r * 10;
	}
	else
	{
		secs_r = 0;
		mins_r = 0;
		tens_r = 0;
	}

	if (cgs.g_maxrounds)
	{
		if (cgs.g_roundcount == 0)
		{
			cgs.g_roundcount = 1;
		}

		if (cgs.g_roundcount > cgs.g_maxrounds)
		{
			cgs.g_roundcount = cgs.g_maxrounds;
		}
		return va("%i of %i | %i:%i%i", cgs.g_roundcount, cgs.g_maxrounds, mins_r, tens_r, secs_r);
	}
	else
	if (cgs.timelimit > 0)
	{
		misecs = (cgs.timelimit * 60000) - (cg.time - cgs.levelStartTime);
		//misecs = (cgs.gameid + (cgs.timelimit * 60000)) - cg.time;

		if (misecs > 0)
		{
			secs_l	= misecs / 1000;
			mins_l	= secs_l / 60;
			secs_l -= mins_l * 60;
			tens_l	= secs_l / 10;
			secs_l -= tens_l * 10;
		}
		else
		{
			secs_l = 0;
			mins_l = 0;
			tens_l = 0;
		}

		return va("%i:%i%i | %i:%i%i", mins_l, tens_l, secs_l, mins_r, tens_r, secs_r);
	}

	return va("%i:%i%i", mins_r, tens_r, secs_r);
}

/**
 * $(function)
 */
static char *GetCaptureAndHoldTimer(void)
{
	int  mins_l, secs_l, tens_l;
	int  mins_f, secs_f, tens_f;
	int  misecs;

	misecs = cg.time - cgs.levelStartTime;

	if (misecs > 0)
	{
		secs_l	= misecs / 1000;
		mins_l	= secs_l / 60;
		secs_l -= mins_l * 60;
		tens_l	= secs_l / 10;
		secs_l -= tens_l * 10;
	}
	else
	{
		secs_l = 0;
		mins_l = 0;
		tens_l = 0;
	}

	secs_f	= (cg.cahTime - cg.time) / 1000;
	mins_f	= secs_l / 60;
	secs_f -= mins_f * 60;
	tens_f	= secs_f / 10;
	secs_f -= tens_f * 10;

	if (cgs.timelimit > 0)
	{
		//misecs = (cgs.gameid + (cgs.timelimit * 60000)) - cg.time;
		misecs = (cgs.timelimit * 60000) - (cg.time - cgs.levelStartTime);

		if (misecs > 0)
		{
			secs_l	= misecs / 1000;
			mins_l	= secs_l / 60;
			secs_l -= mins_l * 60;
			tens_l	= secs_l / 10;
			secs_l -= tens_l * 10;
		}
		else
		{
			secs_l = 0;
			mins_l = 0;
			tens_l = 0;
		}
	}

	return va("%i:%i%i | %i:%i%i", mins_l, tens_l, secs_l, mins_f, tens_f, secs_f);
}

/**
 * $(function)
 */
static char *GetDefaultTimer(void)
{
	int  mins, secs, tens;
	int  msec;

//	if (cgs.timelimit > 0)
//	msec = (cgs.gameid + (cgs.timelimit * 60000)) - (cg.time);
//	else
	//msec = (/*cgs.gameid*/ (cgs.timelimit * 60000))-(cg.time - cgs.levelStartTime);
	msec = (cgs.timelimit * 60000) - (cg.time - cgs.levelStartTime);

//
	if (msec > 0)
	{
		secs  = msec / 1000;
		mins  = secs / 60;
		secs -= mins * 60;
		tens  = secs / 10;
		secs -= tens * 10;
	}
	else
	{
		secs = 0;
		mins = 0;
		tens = 0;
	}

	return va("%i:%i%i", mins, tens, secs);
}

/**
 * $(function)
 */
char *CG_GetTimerString(void)
{
	if (cgs.survivor)
	{
		return GetSurvivorTimer();
	}
	else if (cgs.gametype == GT_UT_CAH && cg.cahTime)
	{
		return GetCaptureAndHoldTimer();
	}

	return GetDefaultTimer();
}

/*
char* CG_GetTimerString ( void )
{
	char		*s;
	int 		mins, seconds, tens;
	int 		msec;

	s = va("");

	msec = cg.time - cgs.levelStartTime;

	if ( cgs.survivor)
		msec = (cgs.survivorRoundTime * 60 * 1000) - msec;

	if ( msec > 0 )
	{
		seconds = msec / 1000;
		mins = seconds / 60;
		seconds -= mins * 60;
		tens = seconds / 10;
		seconds -= tens * 10;
	}
	else
	{
		seconds = 0;
		mins = 0;
		tens = 0;
	}

	if ( cgs.gametype == GT_UT_CAH && cg.captureScoreTime )
	{
		int mins2, seconds2, tens2;
		seconds2 = (cg.captureScoreTime - cg.time) / 1000;
		mins2 = seconds / 60;
		seconds2 -= mins2 * 60;
		tens2 = seconds2 / 10;
		seconds2 -= tens2 * 10;

		s = va( "%i:%i%i | %i:%i%i", mins, tens, seconds, mins2, tens2, seconds2 );
	}
	else
		s = va( "%i:%i%i", mins, tens, seconds );

	return s;
}
*/

char *CG_GetWaveTimerString(int team)
{
	int  delay, msec, delta;
	int  mins, seconds, tens;

	if (team == TEAM_RED)
	{
		delay = cgs.redWaveRespawnDelay;
	}
	else
	{
		delay = cgs.blueWaveRespawnDelay;
	}

	//msec = (int)((cg.time / 1000) - (cgs.levelStartTime / 1000));

	//Time of next pause
	delta = ((cg.time - cg.pausedTime) % (delay * 1000));

	msec  = delta ;
	msec /= 1000;
	msec += 1;

	if (msec <= delay)
	{
		msec = (delay - msec) * 1000;
	}
	else
	{
		msec = (delay - (msec % delay)) * 1000;
	}

	if (msec > 0)
	{
		seconds  = msec / 1000;
		mins	 = seconds / 60;
		seconds -= mins * 60;
		tens	 = seconds / 10;
		seconds -= tens * 10;
	}
	else
	{
		seconds = 0;
		mins	= 0;
		tens	= 0;
	}

	return va("%i:%i%i", mins, tens, seconds);
}

/**
 * $(function)
 */
char *CG_GetHotPotatoString(void)
{
	int  delay, msec;
	int  mins, seconds, tens;

	delay = cgs.hotpotatotime;

	if (cgs.hotpotato == 0)
	{
		return va("--:--");
	}

	msec = (int)( cgs.hotpotatotime - cg.time );

	if (msec > 0)
	{
		seconds  = msec / 1000;
		mins	 = seconds / 60;
		seconds -= mins * 60;
		tens	 = seconds / 10;
		seconds -= tens * 10;
	}
	else
	{
		seconds = 0;
		mins	= 0;
		tens	= 0;
	}

	return va("%i:%i%i", mins, tens, seconds);
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name        : CG_GetJumpTime
// Description : Returns the jump timer string
// Author      : Barbatos, Fenix
///////////////////////////////////////////////////////////////////////////////////////////
char *CG_GetJumpTime(int msec) {

	int seconds, mins, hours;
	seconds = msec / 1000;
	msec -= seconds * 1000;
	mins = seconds / 60;
	seconds -=mins * 60;
	hours = mins / 60;
	mins -= hours * 60;

	return va("%01d:%02d:%02d.%03d", hours, mins, seconds, msec);

}

#if 1
/**
 * $(function)
 */
static void CG_DrawRespawnTimer(void)
{
	char  *s;
	int   secs;

	// If we're spectating someone.
	if ((cg.snap->ps.pm_flags & PMF_FOLLOW) || (cg.snap->ps.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_GHOST))
	{
		cg.respawnTime = 0;

		return;
	}

	// See if count-down is done.
	if (cg.time > cg.respawnTime)
	{
		cg.respawnTime = 0;

		return;
	}
	else
	{
		// If we're doing wave respawns.
		if (cgs.waveRespawns && (cgs.gametype >= GT_TEAM) && (cgs.gametype != GT_JUMP)) //@Barbatos
		{
			if (cgs.clientinfo[cg.predictedPlayerState.clientNum].team == TEAM_RED)
			{
				if (cgs.redWaveRespawnDelay <= 2)
				{
					cg.respawnTime = 0;

					return;
				}
			}
			else
			{
				if (cgs.blueWaveRespawnDelay <= 2)
				{
					cg.respawnTime = 0;

					return;
				}
			}
		}
		else
		{

			if (cgs.respawnDelay <= 2)
			{
				cg.respawnTime = 0;

				return;
			}
		}
	}

	// Calculate seconds.
	secs = (cg.respawnTime - cg.time) / 1000;

	// If we're doing wave respawns.
	if (cgs.waveRespawns && (cgs.gametype >= GT_TEAM) && (cgs.gametype != GT_JUMP)) //@Barbatos
	{
		switch (secs)
		{
			case 0:
				s = va("Respawning Now");
				break;

			case 1:
				s = va("Wave Respawn in %i second", secs);
				break;

			default:
				s = va("Wave Respawn in %i seconds", secs);
				break;
		}
	}
	else
	{
		switch (secs)
		{
			case 0:
				s = va("Respawning Now");
				break;

			case 1:
				s = va("Respawn in %i second", secs);
				break;

			default:
				s = va("Respawn in %i seconds", secs);
				break;
		}
	}

	// Draw the text.
	CG_Text_Paint(320 - (CG_Text_Width(s, 0.40f, 0) / 2), 440, 0.40f, colorWhite, s, 0, 0, ITEM_TEXTSTYLE_OUTLINED);
}
#else
/**
 * $(function)
 */
static void CG_DrawRespawnTimer ( void )
{
	int   secs;
	char  *s;

	//Dont Draw the respawn timer cause it goes over the follow box...
	if ((cg.snap->ps.pm_flags & PMF_FOLLOW)) // || (cgs.clientinfo[cg.clientNum].ghost))
	{
		return;
	}

	// See if we are done.
	if ((cg.time > cg.respawnTime) || ((cgs.respawnDelay <= 2) && !(cgs.waveRespawns && (cgs.gametype >= GT_TEAM) && ((cgs.clientinfo[cg.predictedPlayerState.clientNum].team == TEAM_RED) || (cgs.clientinfo[cg.predictedPlayerState.clientNum].team == TEAM_BLUE)))))
	{
		cg.respawnTime = 0;
		return;
	}

	secs = (cg.respawnTime - cg.time) / 1000;

	if (cgs.waveRespawns && (cgs.gametype >= GT_TEAM))
	{
		if (secs > 1)
		{
			s = va("Wave Respawn in %i seconds", secs );
		}

		if (secs == 1)
		{
			s = va("Wave Respawn in %i second", secs );
		}

		if (secs == 0)
		{
			s = va("Respawning Now" );
		}
	}
	else
	{
		if (secs > 1)
		{
			s = va("Respawn in %i seconds", secs );
		}

		if (secs == 1)
		{
			s = va("Respawn in %i second", secs );
		}

		if (secs == 0)
		{
			s = va("Respawning Now" );
		}
	}

	CG_Text_Paint ( 320 - CG_Text_Width ( s, 0.40f, 0 ) / 2,
			440,
			0.40f,
			colorWhite,
			s,
			0, 0,
			ITEM_TEXTSTYLE_OUTLINED );
}
#endif

/*
=================
CG_DrawTimer
============
=====
*/
#if 0
static float CG_DrawTimer( float y )
{
	char   *s	= CG_GetTimerString ( );
	float  color[4] = { 1, 1, 1, 1};

	CG_Text_Paint ( 320 - CG_Text_Width ( s, 0.35f, 0 ) / 2,
			470,
			0.35f,
			color,
			s,
			0, 0,
			ITEM_TEXTSTYLE_OUTLINED );

	return y + 20;
}
#endif

/*
=====================
CG_DrawUpperRight

=====================
*/
static void CG_DrawUpperRight( void )
{
	CG_DrawMiniScoreboard ( );
	CG_DrawRespawnTimer ( );

	//if ( cg.predictedPlayerState.persistant[PERS_TEAM] != TEAM_SPECTATOR )
	if (cgs.clientinfo[cg.snap->ps.clientNum].team != TEAM_SPECTATOR) {

		rectDef_t  r;
		vec4_t* color;

		r.x = 5;
		r.y = 385;
		r.w = 45;
		r.h = 90;

		color = &skinInfos[CG_WhichSkin(cgs.clientinfo[cg.snap->ps.clientNum].team)].ui_combat_color;

		CG_DrawPlayerHealth ( &r, *color );

		r.x = 565;
		r.y = 448;
		r.w = 70;
		r.h = 32;

		CG_DrawPlayerAmmoValue ( &r, *color );

		r.x = 560;
		r.y = 428;
		r.w = 70;
		r.h = 20;

		CG_DrawPlayerWeaponMode ( &r, 0.20f, colorWhite, 3 );
	}

/*
	y = 0;

	if ( cg_drawSnapshot.integer ) {
		y = CG_DrawSnapshot( y );
	}
	if ( cg_drawFPS.integer ) {
		y = CG_DrawFPS( y );
	}

	if ( cg.respawnTime != 0 )
	else if ( cg_drawTimer.integer )
		CG_DrawTimer( y );
*/
}

/**
 * $(function)
 */
static void CG_DrawChat( void )
{
	int h;
	int i;
	vec4_t	hcolor;
	int chatHeight;

#define CHATLOC_X	 5
#define CHAT_LINE_HEIGHT 15

	int  CHATLOC_Y = 340 - (cg_chatHeight.integer - 5) * CHAT_LINE_HEIGHT;

	if (cg_chatHeight.integer < CHAT_HEIGHT)
	{
		chatHeight = cg_chatHeight.integer;
	}
	else
	{
		chatHeight = CHAT_HEIGHT;
	}

	if (chatHeight <= 0)
	{
		return;
	}

	if (cgs.chatLastPos != cgs.chatPos)
	{
		if (cg.time - cgs.chatTextTimes[cgs.chatLastPos % chatHeight] > cg_chatTime.integer)
		{
			cgs.chatLastPos++;
		}

		h	  = (cgs.chatPos - cgs.chatLastPos) * CHATCHAR_HEIGHT;

		hcolor[3] = 1.0;

		for (i = cgs.chatLastPos; i < cgs.chatPos; i++)
		{
			CG_Text_Paint ( CHATLOC_X,
					CHATLOC_Y + (i - cgs.chatLastPos) * CHAT_LINE_HEIGHT,
					0.25f,
					hcolor,
					cgs.chatText[i % chatHeight],
					0, 0,
					ITEM_TEXTSTYLE_OUTLINED );
		}
	}
}

/**
 * $(function)
 */
static void CG_DrawMessages( void )
{
	int h;
	int i;
	vec4_t	hcolor;
	int chatHeight;
	int top;

#define PRINTLOC_Y	15	   // bottom end
#define PRINTLOC_X	5
#define MSG_LINE_HEIGHT 15

	// IF talking move the text down a bit
	if(cg.predictedPlayerState.eFlags & EF_TALK)
	{
		top = 20;
	}
	else
	{
		top = 0;
	}

	if (cg_msgHeight.integer < MSG_HEIGHT)
	{
		chatHeight = cg_msgHeight.integer;
	}
	else
	{
		chatHeight = MSG_HEIGHT;
	}

	if (chatHeight <= 0)
	{
		return; // disabled
	}

	if (cgs.msgLastPos != cgs.msgPos)
	{
		if (cg.time - cgs.msgTextTimes[cgs.msgLastPos % chatHeight] > cg_msgTime.integer)
		{
			cgs.msgLastPos++;
		}

		h	  = (cgs.msgPos - cgs.msgLastPos) * MSG_CHAR_HEIGHT;

		hcolor[0] = hcolor[1] = hcolor[2] = hcolor[3] = 1.0;

		for (i = cgs.msgLastPos; i < cgs.msgPos; i++)
		{
			CG_Text_Paint ( PRINTLOC_X,
					PRINTLOC_Y + top + (i - cgs.msgLastPos) * MSG_LINE_HEIGHT,
					0.25f,
					hcolor,
					cgs.msgText[i % chatHeight],
					0, 0,
					ITEM_TEXTSTYLE_OUTLINED );
		}
	}
}

/*
===============================================================================

LAGOMETER

===============================================================================
*/

#define LAG_SAMPLES 128

typedef struct
{
	int  frameSamples[LAG_SAMPLES];
	int  frameCount;
	int  snapshotFlags[LAG_SAMPLES];
	int  snapshotSamples[LAG_SAMPLES];
	int  snapshotCount;
} lagometer_t;

lagometer_t  lagometer;

/*
==============
CG_AddLagometerFrameInfo

Adds the current interpolate / extrapolate bar for this frame
==============
*/
void CG_AddLagometerFrameInfo( void )
{
	int  offset;

	offset								 = cg.time - cg.latestSnapshotTime;
	lagometer.frameSamples[lagometer.frameCount & (LAG_SAMPLES - 1)] = offset;
	lagometer.frameCount++;
}

/*
==============
CG_AddLagometerSnapshotInfo

Each time a snapshot is received, log its ping time and
the number of snapshots that were dropped before it.

Pass NULL for a dropped packet.
==============
*/
void CG_AddLagometerSnapshotInfo( snapshot_t *snap, qboolean warped )
{
	// dropped packet
	if (!snap)
	{
		lagometer.snapshotSamples[lagometer.snapshotCount & (LAG_SAMPLES - 1)] = -1;
		lagometer.snapshotCount++;
		return;
	}

	// add this snapshot's info
	lagometer.snapshotSamples[lagometer.snapshotCount & (LAG_SAMPLES - 1)] = snap->ping;
	lagometer.snapshotFlags[lagometer.snapshotCount & (LAG_SAMPLES - 1)]   = snap->snapFlags;

	if (warped)
	{
		lagometer.snapshotFlags[lagometer.snapshotCount & (LAG_SAMPLES - 1)] |= SNAPFLAG_ANTIWARPED;
	}
	lagometer.snapshotCount++;
}

/*
==============
CG_DrawDisconnect

Should we draw something differnet for long lag vs no packets?
==============
*/
static void CG_DrawDisconnect( void )
{
	float		x, y;
	int 	cmdNum;
	usercmd_t	cmd;
	const char	*s;
	int 	w;

	if (cg_noci.integer) { //@B1n: Don't show CI message w/ cg_noci
		return;
	}

	// draw the phone jack if we are completely past our buffers
	cmdNum = trap_GetCurrentCmdNumber() - CMD_BACKUP + 1;
	trap_GetUserCmd( cmdNum, &cmd );

	if ((cmd.serverTime <= cg.snap->ps.commandTime)
		|| (cmd.serverTime > cg.time))	// special check for map_restart
	{
		return;
	}

	// also add text in center of screen
	s = "Connection Interrupted";
	w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH;
	CG_DrawBigString( 320 - w / 2, 100, s, 1.0F);

	// blink the icon
	if ((cg.time >> 9) & 1)
	{
		return;
	}

	x = 640 - 24;
	y = 480 - 24;

	CG_DrawPic( x, y, 24, 24, trap_R_RegisterShader("gfx/2d/net.tga" ));
}

#define MAX_LAGOMETER_PING	900
#define MAX_LAGOMETER_RANGE 300

/*
==============
CG_DrawLagometer
==============
*/
static void CG_DrawLagometer( void )
{
	int    a, x, y, i;
	float  v;
	float  ax, ay, aw, ah, mid, range;
	int    color;
	float  vscale;

	if (!cg_lagometer.integer || cgs.localServer)
	{
		CG_DrawDisconnect();
		return;
	}

	//
	// draw the graph
	//
/*
#ifdef MISSIONPACK
	x = 640 - 48;
	y = 480 - 144;
#else
	x = 640 - 48;
	y = 480 - 48;
#endif
*/
	x = 563;
	y = 4;

	if (cg_drawFPS.integer)
	{
		y += 19;
	}

	if (cg_drawTeamScores.integer && (cgs.gametype >= GT_TEAM) && (cgs.gametype != GT_JUMP) && cg.snap)
	{
		y += 34;
	}

	//if (cgs.gametype == GT_UT_ASSASIN || cgs.gametype == GT_UT_CAH)
	//	y += 90;

	if (cg_drawTimer.integer)
	{
		y += 34;

		if (!cgs.survivor && cgs.waveRespawns && (cgs.gametype >= GT_TEAM) && (cgs.gametype != GT_JUMP) && ((cgs.clientinfo[cg.predictedPlayerState.clientNum].team == TEAM_RED) || (cgs.clientinfo[cg.predictedPlayerState.clientNum].team == TEAM_BLUE)))
		{
			y += 34;
		}

		//@Fenix - add a little space for the new timers
		if (cgs.gametype == GT_JUMP) {
			y += 8;
		}

	}

	if (cgs.g_hotpotato > 0)
	{
		y += 20;
	}

	trap_R_SetColor( NULL );
	CG_DrawPic( x, y, 48, 48, cgs.media.lagometerShader );

	ax = x;
	ay = y;
	aw = 48;
	ah = 48;
	CG_AdjustFrom640( &ax, &ay, &aw, &ah );

	color  = -1;
	range  = ah / 3;
	mid    = ay + range;

	vscale = range / MAX_LAGOMETER_RANGE;

	// draw the frame interpoalte / extrapolate graph
	for (a = 0 ; a < aw ; a++)
	{
		i  = (lagometer.frameCount - 1 - a) & (LAG_SAMPLES - 1);
		v  = lagometer.frameSamples[i];
		v *= vscale;

		if (v > 0)
		{
			if (color != 1)
			{
				color = 1;
				trap_R_SetColor( g_color_table[ColorIndex(COLOR_YELLOW)] );
			}

			if (v > range)
			{
				v = range;
			}
			trap_R_DrawStretchPic ( ax + aw - a, mid - v, 1, v, 0, 0, 0, 0, cgs.media.whiteShader );
		}
		else if (v < 0)
		{
			if (color != 2)
			{
				color = 2;
				trap_R_SetColor( g_color_table[ColorIndex(COLOR_BLUE)] );
			}
			v = -v;

			if (v > range)
			{
				v = range;
			}
			trap_R_DrawStretchPic( ax + aw - a, mid, 1, v, 0, 0, 0, 0, cgs.media.whiteShader );
		}
	}

	// draw the snapshot latency / drop graph
	range  = ah / 2;
	vscale = range / MAX_LAGOMETER_PING;

	for (a = 0 ; a < aw ; a++)
	{
		i = (lagometer.snapshotCount - 1 - a) & (LAG_SAMPLES - 1);
		v = lagometer.snapshotSamples[i];

		if (v > 0)
		{
			if (lagometer.snapshotFlags[i] & SNAPFLAG_RATE_DELAYED)
			{
				if (color != 5)
				{
					color = 5;	// YELLOW for rate delay
					trap_R_SetColor( g_color_table[ColorIndex(COLOR_YELLOW)] );
				}
			}
			else
			if (lagometer.snapshotFlags[i] & SNAPFLAG_ANTIWARPED)
			{
				if (color != 6)
				{
					color = 6; //Black for ANTIWARP STING
					trap_R_SetColor( g_color_table[ColorIndex(COLOR_BLACK)] );
				}
			}
			else
			{
				if (color != 3)
				{
					color = 3;
					trap_R_SetColor( g_color_table[ColorIndex(COLOR_GREEN)] );
				}
			}
			v = v * vscale;

			if (v > range)
			{
				v = range;
			}
			trap_R_DrawStretchPic( ax + aw - a, ay + ah - v, 1, v, 0, 0, 0, 0, cgs.media.whiteShader );
		}
		else if (v < 0)
		{
			if (color != 4)
			{
				color = 4;		// RED for dropped snapshots
				trap_R_SetColor( g_color_table[ColorIndex(COLOR_RED)] );
			}
			trap_R_DrawStretchPic( ax + aw - a, ay + ah - range, 1, range, 0, 0, 0, 0, cgs.media.whiteShader );
		}
	}

	trap_R_SetColor( NULL );

	if (cg_nopredict.integer || cg_synchronousClients.integer)
	{
		CG_DrawBigString( ax, ay, "snc", 1.0 );
	}

	CG_DrawDisconnect();
}

/*
===============================================================================

CENTER PRINTING

===============================================================================
*/

/*
==============
CG_CenterPrint

Called for important messages that should stay in the center of the screen
for a few moments
==============
*/
void CG_CenterPrint( const char *str, int y, int charWidth )
{
	char  *s;

	Q_strncpyz( cg.centerPrint, str, sizeof(cg.centerPrint));

	// Get it to the console too
	trap_Print( str );
	trap_Print( "\n" );

	//Com_Printf("%i",cg.time);
	cg.centerPrintTime	= cg.time;
	cg.centerPrintY 	= y;
	cg.centerPrintCharWidth = charWidth;

	// count the number of lines for centering
	cg.centerPrintLines = 1;
	s			= cg.centerPrint;

	while(*s)
	{
		if (*s == '\n')
		{
			cg.centerPrintLines++;
		}
		s++;
	}
}

/*
===================
CG_DrawCenterString
===================
*/
static void CG_DrawCenterString( void )
{
	char   *start;
	int    l;
	int    x, y, w, h;
	float  *color;

	if (!cg.centerPrintTime)
	{
		return;
	}

	color = CG_FadeColor( cg.centerPrintTime, 1000 * cg_centertime.value );

	if (!color)
	{
		return;
	}

	trap_R_SetColor( color );

	start = cg.centerPrint;

	y	  = cg.centerPrintY - cg.centerPrintLines * BIGCHAR_HEIGHT / 2;

	while (1)
	{
		char  linebuffer[1024];

		for (l = 0; l < 100; l++)
		{
			if (!start[l] || (start[l] == '\n'))
			{
				break;
			}
			linebuffer[l] = start[l];
		}
		linebuffer[l] = 0;

#ifdef MISSIONPACK
		w  = CG_Text_Width(linebuffer, 0.35f, 0);
		h  = CG_Text_Height(linebuffer, 0.35f, 0);
		x  = (SCREEN_WIDTH - w) / 2;
		CG_Text_Paint(x, y + h, 0.35f, color, linebuffer, 0, 0, ITEM_TEXTSTYLE_OUTLINED);
		y += h + 6;
#else
		w  = cg.centerPrintCharWidth * CG_DrawStrlen( linebuffer );

		x  = (SCREEN_WIDTH - w) / 2;

		CG_DrawStringExt( x, y, linebuffer, color, qfalse, qtrue,
				  cg.centerPrintCharWidth, (int)(cg.centerPrintCharWidth * 1.5), 0 );

		y += cg.centerPrintCharWidth * 1.5;
#endif

		while (*start && (*start != '\n'))
		{
			start++;
		}

		if (!*start)
		{
			break;
		}
		start++;
	}

	trap_R_SetColor( NULL );
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : CG_DrawCrosshair
// Description: Draws the crosshair in the center of the screen
// Author	  : Apoxol
///////////////////////////////////////////////////////////////////////////////////////////

static void DrawCrosshairNew(void)
{
	float  offset, speed, recoil;
	int    size;

	// Get crosshair size.
	size = cg_crosshairSize.integer;

	// Clamp.
	if (size < 1)
	{
		size = 1;
	}
	else if (size > 50)
	{
		size = 50;
	}

	// If were aiming at a teamate.
	if ((cg.crosshairColorClientNum != -1) && (cgs.gametype >= GT_TEAM) && (cgs.clientinfo[cg.crosshairColorClientNum].team == cgs.clientinfo[cg.snap->ps.clientNum].team))
	{
		// Set colour.
		trap_R_SetColor(cg.crosshairFriendRGB);
	}
	else
	{
		// Set colour.
		trap_R_SetColor(cg.crosshairRGB);
	}

	// If we're in the air.
	if (cg.predictedPlayerState.groundEntityNum == ENTITYNUM_NONE)
	{
		offset = 8.0f;
	}
	else
	{
		// Calculate speed.
		speed = VectorLength(cg.predictedPlayerState.velocity);

		// Calculate recoil.
		recoil = (float)cg.predictedPlayerState.stats[STAT_RECOIL] / (float)UT_MAX_RECOIL;

		if (speed > 105.f)
		{
			if (speed < 145.0f)
			{
				offset = 2.0f + (6.0f * (((speed - 105.0f) / 40.0f) + recoil));

				// Clamp.
				if (offset > 8.0f)
				{
					offset = 8.0f;
				}
			}
			else
			{
				offset = 8.0f;
			}
		}
		else
		{
			offset = 2.0f + (6.0f * recoil);
		}
	}

	// Draw left.
	CG_DrawPic(320 - size - offset, 240, size, 1, cgs.media.whiteShader);

	// Draw right.
	CG_DrawPic(321 + offset, 240, size, 1, cgs.media.whiteShader);

	// Draw upper.
	CG_DrawPic(320, 240 - size - offset, 1, size, cgs.media.whiteShader);

	// Draw lower.
	CG_DrawPic(320, 241 + offset, 1, size, cgs.media.whiteShader);

	// Clear colour.
	trap_R_SetColor(NULL);
}

/**
 * $(function)
 */
static void DrawCrosshairNew2(void)
{
	float  recoil;
	float  offsetL, offsetR, offsetU, offsetD;
	int    size;

	// Get crosshair size.
	size = cg_crosshairSize.integer;

	// Clamp size.
	if (size < 1)
	{
		size = 1;
	}

	// If were aiming at a teamate.
	if ((cg.crosshairColorClientNum != -1) && (cgs.gametype >= GT_TEAM) && (cgs.clientinfo[cg.crosshairColorClientNum].team == cgs.clientinfo[cg.snap->ps.clientNum].team))
	{
		// Set colour.
		trap_R_SetColor(cg.crosshairFriendRGB);
	}
	else
	{
		// Set colour.
		trap_R_SetColor(cg.crosshairRGB);
	}

	// Calculate recoil.
	recoil = (float)cg.predictedPlayerState.stats[STAT_RECOIL] / (float)UT_MAX_RECOIL;

	// Calculate offset.
	switch (utPSGetWeaponID(&cg.predictedPlayerState))
	{
		case UT_WP_KNIFE:
			offsetL = 10.0f;
			offsetR = 10.0f;
			offsetU = 10.0f;
			offsetD = 10.0f;
			break;

	/*case UT_WP_DUAL_BERETTA:
	case UT_WP_DUAL_DEAGLE:
	case UT_WP_DUAL_GLOCK:
	case UT_WP_DUAL_MAGNUM:
			offsetL = 5.0f + (recoil * 25.0f);
			offsetD = offsetU = offsetR = offsetL;
			size   += recoil * 10.0f;
			break;

	case UT_WP_MAGNUM:
			offsetL = 5.0f + (recoil * 25.0f);
			offsetD = offsetU = offsetR = offsetL;
			size   += recoil * 10.0f;
			break;
	 */
		case UT_WP_GLOCK:
			offsetL = 5.0f + (recoil * 25.0f);
			offsetD = offsetU = offsetR = offsetL;
			size   += recoil * 10.0f;
			break;

		case UT_WP_COLT1911:
            offsetL = 5.0f + (recoil * 25.0f);
            offsetD = offsetU = offsetR = offsetL;
            size   += recoil * 10.0f;
            break;

		case UT_WP_BERETTA:
			offsetL = 5.0f + (recoil * 25.0f);
			offsetD = offsetU = offsetR = offsetL;
			size   += recoil * 10.0f;
			break;

		case UT_WP_DEAGLE:
			offsetL = 5.0f + (recoil * 20.0f);
			offsetD = offsetR = offsetL;
			offsetU = 5.0f + (recoil * 30.0f);
			size   += recoil * 20.0f;
			break;

		case UT_WP_SPAS12:
			offsetL = 8.0f + (recoil * 70.0f);
			offsetD = offsetU = offsetR = offsetL;
			size   += recoil * 30.0f;
			break;

		/*case UT_WP_BENELLI:
			offsetL = 8.0f + (recoil * 70.0f);
			offsetD = offsetU = offsetR = offsetL;
			size   += recoil * 30.0f;
			break;*/

		case UT_WP_MP5K:
			offsetL = 4.0f + (recoil * 15.0f);
			offsetD = offsetU = offsetR = offsetL;
			size   += recoil * 5.0f;
			break;

		case UT_WP_MAC11:
            offsetL = 3.0f + (recoil * 27.5f);
            offsetD = offsetR = offsetL;
            offsetU = 3.0f + (recoil * 40.0f);
            size   += recoil * 10.0f;
            break;

		/*case UT_WP_P90:
			offsetL = 4.0f + (recoil * 15.0f);
			offsetD = offsetU = offsetR = offsetL;
			size   += recoil * 5.0f;
			break;*/

		case UT_WP_UMP45:
			offsetL = 4.0f + (recoil * 27.5f);
			offsetD = offsetR = offsetL;
			offsetU = 4.0f + (recoil * 35.0f);
			size   += recoil * 5.0f;
			break;

		case UT_WP_HK69:
			offsetL = 8.0f;
			offsetR = 8.0f;
			offsetU = 8.0f;
			offsetD = 8.0f;
			break;

		case UT_WP_LR:
			offsetL = 3.0f + (recoil * 15.0f);
			offsetD = offsetR = offsetL;
			offsetU = 3.0f + (recoil * 20.0f);
			size   += recoil * 5.0f;
			break;

		case UT_WP_G36:
			offsetL = 3.0f + (recoil * 15.0f);
			offsetD = offsetR = offsetL;
			offsetU = 3.0f + (recoil * 15.0f);
			size   += recoil * 5.0f;
			break;

		case UT_WP_GRENADE_HE:
			offsetL = 9.0f;
			offsetR = 9.0f;
			offsetU = 9.0f;
			offsetD = 9.0f;
			break;

		case UT_WP_GRENADE_SMOKE:
			offsetL = 9.0f;
			offsetR = 9.0f;
			offsetU = 9.0f;
			offsetD = 9.0f;
			break;

		case UT_WP_AK103:
			offsetL = 3.0f + (recoil * 27.5f);
			offsetD = offsetR = offsetL;
			offsetU = 3.0f + (recoil * 40.0f);
			size   += recoil * 10.0f;
			break;

		case UT_WP_M4:
			offsetL = 3.0f + (recoil * 27.5f);
			offsetD = offsetR = offsetL;
			offsetU = 3.0f + (recoil * 40.0f);
			size   += recoil * 10.0f;
			break;

		case UT_WP_NEGEV:
			offsetL = 4.0f + (recoil * 70.0f);
			offsetD = offsetR = offsetL;
			offsetU = 4.0f + (recoil * 100.0f);
			size   += recoil * 30.0f;
			break;

	  default:
		offsetL = 0.0f;
		offsetD = 0.0f;
		offsetU = 0.0f;
		offsetR = 0.0f;
		break;
	}

	// Cap offset.
	if (offsetL > 100.0f)
	{
		offsetL = 100.0f;
	}

	if (offsetR > 100.0f)
	{
		offsetR = 100.0f;
	}

	if (offsetU > 100.0f)
	{
		offsetU = 100.0f;
	}

	if (offsetD > 100.0f)
	{
		offsetD = 100.0f;
	}

	// Clamp size.
	if (size > 50)
	{
		size = 50;
	}

	if ((cg_drawCrosshair.integer == 11) || (cg_drawCrosshair.integer == 12))
	{
		// Draw middle.
		CG_DrawPic(320, 240, 1, 1, cgs.media.whiteShader);
	}

	// Draw left.
	CG_DrawPic(320 - size - offsetL, 240, size, 1, cgs.media.whiteShader);

	// Draw right.
	CG_DrawPic(321 + offsetR, 240, size, 1, cgs.media.whiteShader);

	if (cg_drawCrosshair.integer != 12)
	{
		// Draw upper.
		CG_DrawPic(320, 240 - size - offsetU, 1, size, cgs.media.whiteShader);
	}

	// Draw lower.
	CG_DrawPic(320, 241 + offsetD, 1, size, cgs.media.whiteShader);

	// Clear colour.
	trap_R_SetColor(NULL);
}

/**
 * $(function)
 */
static void DrawCrosshairNew3(void)
{
	// If were aiming at a teamate.
	if ((cg.crosshairColorClientNum != -1) && (cgs.gametype >= GT_TEAM) && (cgs.clientinfo[cg.crosshairColorClientNum].team == cgs.clientinfo[cg.snap->ps.clientNum].team))
	{
		// Set colour.
		trap_R_SetColor(cg.crosshairFriendRGB);
	}
	else
	{
		// Set colour.
		trap_R_SetColor(cg.crosshairRGB);
	}

	CG_DrawPic(320, 240, 1, 1, cgs.media.whiteShader);
}
/**
 * $(function)
 */
static void DrawCrosshairOld(void)
{
	float		 w, h;
	float		 x, y;
	int 	 ca;
	crosshair_t  *crosshair;
	vec3_t		 velocity;
	float		 speed;
	float		 f;
	int 	 weaponID;
	int 	 weaponMode;

	// If our crosshair is over someone and its one of our teammates then turn the crosshair green
	trap_R_SetColor( cg.crosshairRGB );

	if ((cg.crosshairColorClientNum != -1) && (cgs.gametype >= GT_TEAM))
	{
		if (cgs.clientinfo[cg.crosshairColorClientNum].team == cgs.clientinfo[cg.snap->ps.clientNum].team)
		{
			trap_R_SetColor( cg.crosshairFriendRGB );
		}
	}

	w = h = cg_crosshairSize.value;
	x = cg_crosshairX.integer;
	y = cg_crosshairY.integer;
	CG_AdjustFrom640( &x, &y, &w, &h );

	ca = cg_drawCrosshair.integer;

	if (ca < 1)
	{
		ca = 1;
	}

	crosshair = utCrosshairGetAt ( ca - 1 );

	VectorCopy ( cg.predictedPlayerState.velocity, velocity );
//	velocity[2] = 0;
	VectorNormalize ( velocity, speed );

	if (cg.predictedPlayerState.stats[STAT_MOVEMENT_INACCURACY] < UT_INACCURACY_THRESHOLD)
	{
		speed = 0;
	}
	else
	{
		speed *= ((float)(cg.predictedPlayerState.stats[STAT_MOVEMENT_INACCURACY] - UT_INACCURACY_THRESHOLD) / UT_INACCURACY_TIME);
	}

	speed	   = speed > UT_MAX_SPRINTSPEED ? UT_MAX_SPRINTSPEED : speed;

	weaponID   = utPSGetWeaponID ( &cg.snap->ps );
	weaponMode = utPSGetWeaponMode ( &cg.snap->ps );

	if (bg_weaponlist[weaponID].modes[weaponMode].flags & UT_WPMODEFLAG_NORECOIL)
	{
		f = 0;
	}
	else
	{
		f  = (float)cg.predictedPlayerState.stats[STAT_RECOIL] / UT_MAX_RECOIL;
		f *= bg_weaponlist[weaponID].modes[weaponMode].recoil.spread;
		f += (bg_weaponlist[weaponID].modes[weaponMode].spread * ((cg.predictedPlayerState.pm_flags & PMF_DUCKED) ? 0.75f : 1) * ((CG_ENTITIES(cg.clientNum)->currentState.powerups & POWERUP(PW_LASER)) ? 0.75f : 1));

		{
			float  speed1 = speed;
			float  speed2 = 0;

			if (speed1 > 200)
			{
				speed2 = speed1 - 200;
				speed1 = 200;
			}

			f += (((float)speed1 / 100.0f) * bg_weaponlist[weaponID].modes[weaponMode].movementMultiplier * 0.4f);
			f += (((float)speed2 / 100.0f) * bg_weaponlist[weaponID].modes[weaponMode].movementMultiplier * 0.4f * 4);
		}
	}

	f *= 6;

	x++;
	y++;

	if (crosshair->center)
	{
		trap_R_DrawStretchPic( x + cg.refdef.x + 0.5f * (cg.refdef.width - w),
					   y + cg.refdef.y + 0.5f * (cg.refdef.height - h),
					   w, h, 0, 0, 1, 1, crosshair->center );
	}

	if (crosshair->right)
	{
		trap_R_DrawStretchPic( x + cg.refdef.x + 0.5f * (cg.refdef.width - w) + (w * 0.5f) + (f * cgs.screenXScale),
					   y + cg.refdef.y + 0.5f * (cg.refdef.height - h),
					   w, h, 0, 0, 1, 1, crosshair->right );
	}

	if (crosshair->left)
	{
		trap_R_DrawStretchPic( x + cg.refdef.x + 0.5f * (cg.refdef.width - w) - (w * 0.5f) - (f * cgs.screenXScale),
					   y + cg.refdef.y + 0.5f * (cg.refdef.height - h),
					   w, h, 0, 0, 1, 1, crosshair->left );
	}

	if (crosshair->top)
	{
		trap_R_DrawStretchPic( x + cg.refdef.x + 0.5f * (cg.refdef.width - w),
					   y + cg.refdef.y + 0.5f * (cg.refdef.height - h) - (h * 0.5f) - (f * cgs.screenYScale),
					   w, h, 0, 0, 1, 1, crosshair->top );
	}

	if (crosshair->bottom)
	{
		trap_R_DrawStretchPic( x + cg.refdef.x + 0.5f * (cg.refdef.width - w),
					   y + cg.refdef.y + 0.5f * (cg.refdef.height - h) + (h * 0.5f) + (f * cgs.screenYScale),
					   w, h, 0, 0, 1, 1, crosshair->bottom );
	}
}

#if 1
/**
 * $(function)
 */
static void CG_DrawCrosshair(void)
{
	// If crosshair shouldn't be drawn.
	if (!cg_drawCrosshair.integer)
	{
		return;
	}

	// If we're a spectator or a ghost.
	if ((cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR) || (cg.snap->ps.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_GHOST))
	{
		return;
	}

	// If we're in third person.
	if (cg.renderingThirdPerson)
	{
		return;
	}

	// If this weapon doesn't need a crosshair.
	if (bg_weaponlist[utPSGetWeaponID(&cg.predictedPlayerState)].flags & UT_WPFLAG_NOCROSSHAIR)
	{
		return;
	}

	// If this is the bomb.
	if (utPSGetWeaponID(&cg.predictedPlayerState) == UT_WP_BOMB)
	{
		return;
	}

	// If weapon is hidden.
/*	if ( (cg.predictedPlayerState.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_BANDAGING) ||
		 (cg.predictedPlayerState.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_CLIMBING)	||
		 (cg.predictedPlayerState.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_DEFUSING)	||
		 (cg.predictedPlayerState.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_RELOADING) ||
		 (cg.predictedPlayerState.pm_flags	& PMF_LADDER_UPPER) 				||
		 (cg.predictedPlayerState.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_PLANTING)	||
		 (utPSGetWeaponID(&cg.predictedPlayerState) == UT_WP_NEGEV && cg.predictedPlayerState.weaponstate == WEAPON_READY_FIRE && cg.predictedPlayerState.weaponTime > 0))
	{
		return;
	} */

	if ((cg.predictedPlayerState.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_BANDAGING) ||
		(cg.predictedPlayerState.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_CLIMBING) ||
		(cg.predictedPlayerState.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_DEFUSING) ||
		(cg.predictedPlayerState.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_RELOADING))
	{
		return;
	}

	if (cg.predictedPlayerState.pm_flags & PMF_LADDER_UPPER)
	{
		return;
	}

	if	  ((cg.predictedPlayerState.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_PLANTING) ||
		   ((utPSGetWeaponID(&cg.predictedPlayerState) == UT_WP_NEGEV) && (cg.predictedPlayerState.weaponstate == WEAPON_READY_FIRE) && (cg.predictedPlayerState.weaponTime > 0)))
	{
		return;
	}

	// If we want to drawn the old crosshairs.
	if (cg_drawCrosshair.integer < 9)
	{
		DrawCrosshairOld();
	}
	else if (cg_drawCrosshair.integer == 9)
	{
		DrawCrosshairNew();
	}
	else if (cg_drawCrosshair.integer == 10 || cg_drawCrosshair.integer == 11 || cg_drawCrosshair.integer == 12)
	{
		DrawCrosshairNew2();
	}
	else
	{
		DrawCrosshairNew3();
	}
}
#else
/**
 * $(function)
 */
static void CG_DrawCrosshair(void)
{
	float		 w, h;
	float		 x, y;
	int 	 ca;
	crosshair_t  *crosshair;
	vec3_t		 velocity;
	float		 speed;
	float		 f;
	int 	 weaponID;
	int 	 weaponMode;

	// See if the user wants the crosshair to be shown.
	if (!cg_drawCrosshair.integer)
	{
		return;
	}

	// Ghosts and specs dont need crosshairs
	if ((cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR) || (cg.snap->ps.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_GHOST))
	{
		return;
	}

	// No crosshair in third person
	if (cg.renderingThirdPerson)
	{
		return;
	}

	// Look to see if this weapon has no crosshair
	if(bg_weaponlist[utPSGetWeaponID(&cg.predictedPlayerState)].flags & UT_WPFLAG_NOCROSSHAIR)
	{
		return;
	}

	// If the weapon is hidden then dont draw the crosshair.
	if ((cg.predictedPlayerState.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_BANDAGING) ||
		(cg.predictedPlayerState.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_CLIMBING) ||
		(cg.predictedPlayerState.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_DEFUSING) ||
		(cg.predictedPlayerState.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_RELOADING) ||
		(cg.predictedPlayerState.pm_flags & PMF_LADDER_UPPER) ||
		(cg.predictedPlayerState.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_PLANTING) ||
		((utPSGetWeaponID(&cg.predictedPlayerState) == UT_WP_NEGEV) && (cg.predictedPlayerState.weaponstate == WEAPON_READY_FIRE) && (cg.predictedPlayerState.weaponTime > 0)))
	{
		return;
	}

	// If our crosshair is over someone and its one of our teammates then turn the crosshair green
	trap_R_SetColor( cg.crosshairRGB );

	if ((cg.crosshairColorClientNum != -1) && (cgs.gametype >= GT_TEAM))
	{
		if (cgs.clientinfo[cg.crosshairColorClientNum].team == cgs.clientinfo[cg.snap->ps.clientNum].team)
		{
			trap_R_SetColor( cg.crosshairFriendRGB );
		}
	}

	w = h = cg_crosshairSize.value;
	x = cg_crosshairX.integer;
	y = cg_crosshairY.integer;
	CG_AdjustFrom640( &x, &y, &w, &h );

	ca = cg_drawCrosshair.integer;

	if (ca < 1)
	{
		ca = 1;
	}

	crosshair = utCrosshairGetAt ( ca - 1 );

	VectorCopy ( cg.predictedPlayerState.velocity, velocity );
//	velocity[2] = 0;
	VectorNormalize ( velocity, speed );

	if (cg.predictedPlayerState.stats[STAT_MOVEMENT_INACCURACY] < UT_INACCURACY_THRESHOLD)
	{
		speed = 0;
	}
	else
	{
		speed *= ((float)(cg.predictedPlayerState.stats[STAT_MOVEMENT_INACCURACY] - UT_INACCURACY_THRESHOLD) / UT_INACCURACY_TIME);
	}

	speed	   = speed > UT_MAX_SPRINTSPEED ? UT_MAX_SPRINTSPEED : speed;

	weaponID   = utPSGetWeaponID ( &cg.snap->ps );
	weaponMode = utPSGetWeaponMode ( &cg.snap->ps );

	if (bg_weaponlist[weaponID].modes[weaponMode].flags & UT_WPMODEFLAG_NORECOIL)
	{
		f = 0;
	}
	else
	{
		f  = (float)cg.predictedPlayerState.stats[STAT_RECOIL] / UT_MAX_RECOIL;
		f *= bg_weaponlist[weaponID].modes[weaponMode].recoil.spread;
		f += (bg_weaponlist[weaponID].modes[weaponMode].spread * ((cg.predictedPlayerState.pm_flags & PMF_DUCKED) ? 0.75f : 1) * ((CG_ENTITIES(cg.clientNum)->currentState.powerups & POWERUP(PW_LASER)) ? 0.75f : 1));

		{
			float  speed1 = speed;
			float  speed2 = 0;

			if (speed1 > 200)
			{
				speed2 = speed1 - 200;
				speed1 = 200;
			}

			f += (((float)speed1 / 100.0f) * bg_weaponlist[weaponID].modes[weaponMode].movementMultiplier * 0.4f);
			f += (((float)speed2 / 100.0f) * bg_weaponlist[weaponID].modes[weaponMode].movementMultiplier * 0.4f * 4);
		}
	}

	f *= 6;

	x++;
	y++;

	if (crosshair->center)
	{
		trap_R_DrawStretchPic( x + cg.refdef.x + 0.5f * (cg.refdef.width - w),
					   y + cg.refdef.y + 0.5f * (cg.refdef.height - h),
					   w, h, 0, 0, 1, 1, crosshair->center );
	}

	if (crosshair->right)
	{
		trap_R_DrawStretchPic( x + cg.refdef.x + 0.5f * (cg.refdef.width - w) + (w * 0.5f) + (f * cgs.screenXScale),
					   y + cg.refdef.y + 0.5f * (cg.refdef.height - h),
					   w, h, 0, 0, 1, 1, crosshair->right );
	}

	if (crosshair->left)
	{
		trap_R_DrawStretchPic( x + cg.refdef.x + 0.5f * (cg.refdef.width - w) - (w * 0.5f) - (f * cgs.screenXScale),
					   y + cg.refdef.y + 0.5f * (cg.refdef.height - h),
					   w, h, 0, 0, 1, 1, crosshair->left );
	}

	if (crosshair->top)
	{
		trap_R_DrawStretchPic( x + cg.refdef.x + 0.5f * (cg.refdef.width - w),
					   y + cg.refdef.y + 0.5f * (cg.refdef.height - h) - (h * 0.5f) - (f * cgs.screenYScale),
					   w, h, 0, 0, 1, 1, crosshair->top );
	}

	if (crosshair->bottom)
	{
		trap_R_DrawStretchPic( x + cg.refdef.x + 0.5f * (cg.refdef.width - w),
					   y + cg.refdef.y + 0.5f * (cg.refdef.height - h) + (h * 0.5f) + (f * cgs.screenYScale),
					   w, h, 0, 0, 1, 1, crosshair->bottom );
	}
}
#endif

/*
=================
CG_ScanForCrosshairEntity
=================
*/
static void CG_ScanForCrosshairEntity( void )
{
	trace_t  trace;
	vec3_t	 start, end;

	VectorCopy( cg.refdef.vieworg, start );
	VectorMA( start, 131072, cg.refdef.viewaxis[0], end );

	cg.crosshairColorClientNum = -1;

	CG_Trace( &trace, start, vec3_origin, vec3_origin, end,
		  cg.snap->ps.clientNum, CONTENTS_SOLID | CONTENTS_BODY );

//	VectorCopy (trace.endpos, cg.predictedLaserOrigin );

	if (trace.entityNum >= MAX_CLIENTS)
	{
		return;
	}

	// if the player is in fog, don't show it
	// DensitY: gives us a pain in the but problem on ut_docks so bye bye
/*	content = trap_CM_PointContents( trace.endpos, 0 );
	if ( content & CONTENTS_FOG )
	{
		return;
	} */

/* URBAN TERROR - Invis, who you kidding
	// if the player is invisible, don't show it
	if ( CG_ENTITIES( trace.entityNum )->currentState.powerups & ( 1 << PW_INVIS ) ) {
		return;
	}
*/

	// update the fade timer
	cg.crosshairClientNum	   = trace.entityNum;
	cg.crosshairClientTime	   = cg.time;
	cg.crosshairColorClientNum = trace.entityNum;
}

/*
=====================
CG_DrawCrosshairNames
=====================
*/
static void DrawCrosshairNamesOld(void)
{
	float	  *color;
	char	  *out;
	char	  *status;
	float	  w;
	int StateTime;
	qboolean  bandage = qfalse;

	// draw the name of the player being looked at
	color = CG_FadeColor( cg.crosshairClientTime, 1000 );

	if (!color)
	{
		trap_R_SetColor( NULL );
		return;
	}

	// Draw the name in the team color
	color[3] = 1;

	StateTime = CG_ENTITIES(cg.crosshairClientNum)->currentState.time & 0xFF;

	// Determine if we can bandage this guy
	if (cg.predictedPlayerEntity.currentState.powerups & POWERUP(PW_MEDKIT))
	{
		if (StateTime < UT_MAX_HEAL_MEDKIT)
		{
			bandage = qtrue;
		}
	}
	else
	{
		if (StateTime < UT_MAX_HEAL)
		{
			bandage = qtrue;
		}
	}

	if (bandage)
	{
		status = "needs medic";
	}
	else
	{
		if (StateTime < UT_MAX_HEALTH / 4)
		{
			status = "near death";
		}
		else if (StateTime < UT_MAX_HEALTH / 2)
		{
			status = "badly wounded";
		}
		else if (StateTime < UT_MAX_HEALTH * 3 / 4)
		{
			status = "wounded";
		}
		else
		{
			status = "healthy";
		}
	}

	if ((cgs.gametype == GT_UT_ASSASIN) && cgs.clientinfo[cg.crosshairClientNum].teamLeader)
	{
		out = va("%s%s (LEADER): %s", CG_GetColoredClientName(&cgs.clientinfo[cg.crosshairClientNum]), S_COLOR_WHITE, status);
	}
	else if (cgs.gametype == GT_CTF && ((CG_ENTITIES(cg.crosshairClientNum)->currentState.powerups & POWERUP(PW_REDFLAG)) || (CG_ENTITIES(cg.crosshairClientNum)->currentState.powerups & POWERUP(PW_BLUEFLAG))))
	{
		out = va("%s%s (FLAG): %s", CG_GetColoredClientName(&cgs.clientinfo[cg.crosshairClientNum]), S_COLOR_WHITE, status);
	}
	else if (cgs.gametype == GT_UT_BOMB && (CG_ENTITIES(cg.crosshairClientNum)->currentState.powerups & POWERUP(PW_CARRYINGBOMB)))
	{
		out = va("%s%s (BOMB): %s", CG_GetColoredClientName(&cgs.clientinfo[cg.crosshairClientNum]), S_COLOR_WHITE, status);
	}
	else
	{
		out = va("%s%s: %s", CG_GetColoredClientName(&cgs.clientinfo[cg.crosshairClientNum]), S_COLOR_WHITE, status);
	}

	w = CG_Text_Width( out, 0.22f, 0);

	//CG_Text_Paint( 630 - w, 432, 0.22f, color, out, 0, 0, 3);
	//CG_Text_Paint( 40, 480 - 7 + y, 0.22f, color, out, 0, 0, 3);

	//@Fenix - fix cg_crosshairnamestype 0 bug
	CG_Text_Paint( 42, 474, 0.22f, color, out, 0, 0, 3);
}

/**
 * $(function)
 */
static void DrawCrosshairNamesNew2(void)
{
	float	  w, h, s, x, y;
	float	  alpha;
	char	  *out;
	int   health,powerups;
	qboolean  bandage = qfalse;
	vec4_t	  whiteColour;
	vec4_t	  greyColour;
	vec4_t	  healthColour;

	alpha		   = 1.0f - ((float)(cg.time - cg.crosshairClientTime) / 1000);

	whiteColour[0] = 1;
	whiteColour[1] = 1;
	whiteColour[2] = 1;
	whiteColour[3] = alpha;

	greyColour[0]  = 0;
	greyColour[1]  = 0;
	greyColour[2]  = 0;
	greyColour[3]  = 1 * alpha;

	health		   = CG_ENTITIES(cg.crosshairClientNum)->currentState.time & 0xff;

	if (cg.predictedPlayerEntity.currentState.powerups & POWERUP(PW_MEDKIT))
	{
		if (health < UT_MAX_HEAL_MEDKIT)
		{
			bandage = qtrue;
		}
	}
	else
	{
		if (health < UT_MAX_HEAL)
		{
			bandage = qtrue;
		}
	}

	if (bandage)
	{
		healthColour[0] = 1.0f;
		healthColour[1] = 0.0f;
		healthColour[2] = 0.0f;
		healthColour[3] = alpha;
	}
	else if (health < (UT_MAX_HEALTH * 3 / 4))
	{
		healthColour[0] = 1.0f;
		healthColour[1] = 1.0f;
		healthColour[2] = 0.0f;
		healthColour[3] = alpha;
	}
	else
	{
		healthColour[0] = 0.0f;
		healthColour[1] = 1.0f;
		healthColour[2] = 0.0f;
		healthColour[3] = alpha;
	}

	powerups = CG_ENTITIES(cg.crosshairClientNum)->currentState.powerups;

	if ((cgs.gametype == GT_UT_ASSASIN) && cgs.clientinfo[cg.crosshairClientNum].teamLeader)
	{
		out = va("%s (LEADER)", cgs.clientinfo[cg.crosshairClientNum].name);
	}
	else if (cgs.gametype == GT_CTF && ((powerups & POWERUP(PW_REDFLAG)) || (powerups & POWERUP(PW_BLUEFLAG))))
	{
		out = va("%s (FLAG)", cgs.clientinfo[cg.crosshairClientNum].name);
	}
	else if (cgs.gametype == GT_UT_BOMB && (powerups & POWERUP(PW_CARRYINGBOMB)))
	{
		out = va("%s (BOMB)", cgs.clientinfo[cg.crosshairClientNum].name);
	}
	else
	{
		out = va("%s", cgs.clientinfo[cg.crosshairClientNum].name);
	}

	s = cg_crosshairNamesSize.value;

	if (s < 0.0f)
	{
		s = 0.0f;
	}
	else if (s > 1.0f)
	{
		s = 1.0f;
	}

	x = 320;

	if (cg_crosshairNamesType.integer == 3)
	{
		y = 180;
	}
	else
	{
		y = 300;
	}

	w = CG_Text_Width(out, s, 0);

	//@Barbatos
	h = CG_Text_Height(out, s, 0);
	if(h < 8)
		h = 8;

	CG_Text_Paint(x - (w / 2), y, s, whiteColour, out, 0, 0, 3);

	if (w < 75)
	{
		w = 75;
	}

	CG_FillRect(x - (w / 2), y + (h / 2), w, h / 2, greyColour);

	if (health > 0)
	{
		CG_FillRect(x - (w / 2) + 1, y + (h / 2) + 1, (w * ((float)health / 100)) - 2, (h / 2) - 2, healthColour);
	}
}

/**
 * $(function)
 */
static void DrawCrosshairNamesNew(void)
{
	float	  w, h, s, x, y;
	float	  alpha;
	float	  dist, distScale;
	char	  *out;
	int   health, powerups;
	qboolean  bandage = qfalse;
	vec4_t	  whiteColour;
	vec4_t	  greyColour;
	vec4_t	  healthColour;
	vec3_t	  viewForward, viewRight, viewUp;
	vec3_t	  worldVec, localVec;

	alpha		   = 1.0f - ((float)(cg.time - cg.crosshairClientTime) / 1000);

	whiteColour[0] = 1;
	whiteColour[1] = 1;
	whiteColour[2] = 1;
	whiteColour[3] = alpha;

	greyColour[0]  = 0;
	greyColour[1]  = 0;
	greyColour[2]  = 0;
	greyColour[3]  = 1 * alpha;

	health		   = CG_ENTITIES(cg.crosshairClientNum)->currentState.time & 0xff;

	if (cg.predictedPlayerEntity.currentState.powerups & POWERUP(PW_MEDKIT))
	{
		if (health < UT_MAX_HEAL_MEDKIT)
		{
			bandage = qtrue;
		}
	}
	else
	{
		if (health < UT_MAX_HEAL)
		{
			bandage = qtrue;
		}
	}

	if (bandage)
	{
		healthColour[0] = 1.0f;
		healthColour[1] = 0.0f;
		healthColour[2] = 0.0f;
		healthColour[3] = alpha;
	}
	else if (health < (UT_MAX_HEALTH * 3 / 4))
	{
		healthColour[0] = 1.0f;
		healthColour[1] = 1.0f;
		healthColour[2] = 0.0f;
		healthColour[3] = alpha;
	}
	else
	{
		healthColour[0] = 0.0f;
		healthColour[1] = 1.0f;
		healthColour[2] = 0.0f;
		healthColour[3] = alpha;
	}

	powerups = CG_ENTITIES(cg.crosshairClientNum)->currentState.powerups;

	if ((cgs.gametype == GT_UT_ASSASIN) && cgs.clientinfo[cg.crosshairClientNum].teamLeader)
	{
		out = va("%s (LEADER)", cgs.clientinfo[cg.crosshairClientNum].name);
	}
	else if (cgs.gametype == GT_CTF && ((powerups & POWERUP(PW_REDFLAG)) || (powerups & POWERUP(PW_BLUEFLAG))))
	{
		out = va("%s (FLAG)", cgs.clientinfo[cg.crosshairClientNum].name);
	}
	else if (cgs.gametype == GT_UT_BOMB && (powerups & POWERUP(PW_CARRYINGBOMB)))
	{
		out = va("%s (BOMB)", cgs.clientinfo[cg.crosshairClientNum].name);
	}
	else
	{
		out = va("%s", cgs.clientinfo[cg.crosshairClientNum].name);
	}

	s = cg_crosshairNamesSize.value;

	if (s < 0.0f)
	{
		s = 0.0f;
	}
	else if (s > 1.0f)
	{
		s = 1.0f;
	}

	//w = CG_Text_Width(out, s, 0);
	//h = CG_Text_Height(out, s, 0);
	w	 = CG_Text_Width(out, s, 0);

	h	 = 13;

	dist = Distance(cg.refdef.vieworg, CG_ENTITIES(cg.crosshairClientNum)->lerpOrigin) * cg.zoomSensitivity;

	if (dist > 600)
	{
		distScale = dist / 600;
	}
	else if (dist <= 100 && dist > 46)
	{
		distScale = dist / 100;
	}
	else if (dist <= 46)
	{
		distScale = 0.46f;
	}
	else
	{
		distScale = 1;
	}

	if (distScale == 0.46f)
	{
		x = 320;
		y = 300;
	}
	else
	{
		// Calculate our view direction.
		AngleVectors(cg.refdefViewAngles, viewForward, viewRight, viewUp);

		// Get player position.
		VectorCopy(CG_ENTITIES(cg.crosshairClientNum)->lerpOrigin, worldVec);
		worldVec[2] += 64 * distScale;

		// Transform vector from world to local space.
		VectorSubtract(worldVec, cg.refdef.vieworg, worldVec);

		localVec[0] = DotProduct(worldVec, viewRight);
		localVec[1] = DotProduct(worldVec, viewUp);
		localVec[2] = DotProduct(worldVec, viewForward);

		// Transform vector from local to screen space.
		x = ((((640 / 2) / tan(DEG2RAD(cg.refdef.fov_x / 2))) * localVec[0]) / localVec[2]) + (640 / 2);
		y = ((-1.0f * ((480 / 2) / tan(DEG2RAD(cg.refdef.fov_y / 2))) * localVec[1]) / localVec[2]) + (480 / 2);
	}

	CG_Text_Paint(x - (w / 2), y, s, whiteColour, out, 0, 0, 3);

	if (w < 75)
	{
		w = 75;
	}

	CG_FillRect(x - (w / 2), y + (h / 2), w, h / 2, greyColour);

	if (health > 0)
	{
		CG_FillRect(x - (w / 2) + 1, y + (h / 2) + 1, (w * ((float)health / 100)) - 2, (h / 2) - 2, healthColour);
	}
}

/**
 * $(function)
 */
static void CG_DrawCrosshairNames(void)
{
	vec3_t	forward, direction;

	// No crosshair names if the crosshair is turned off
	if (!cg_drawCrosshair.integer)
	{
		return;
	}

	// No crosshair names if they are turned off
	if (!cg_drawCrosshairNames.integer)
	{
		return;
	}

	// Only show crosshair names in team games
	if (cgs.gametype < GT_TEAM)
	{
		return;
	}

	// No crosshair names in third person
	if (cg.renderingThirdPerson)
	{
		return;
	}

	if (cg.crosshairClientNum == cg.snap->ps.clientNum)
	{
		return;
	}

	if ((cg.crosshairClientTime == 0) || ((cg.time - cg.crosshairClientTime) > 1000))
	{
		return;
	}

	// If they arent on the same team then dont show the name
	if (cg.snap && (cgs.clientinfo[cg.crosshairClientNum].team != cgs.clientinfo[cg.snap->ps.clientNum].team) && (cgs.gametype != GT_JUMP))
	{
		return;
	}

	// Make sure they have a name
	if (!cgs.clientinfo[cg.crosshairClientNum].name[0])
	{
		return;
	}

	if (cg.snap->ps.stats[STAT_HEALTH] <= 0)
	{
		if (!(cg.snap->ps.pm_flags & PMF_FOLLOW))
		{
			return;
		}
	}

	AngleVectorsF(cg.refdefViewAngles, forward, NULL, NULL);

	VectorSubtract(CG_ENTITIES(cg.crosshairClientNum)->lerpOrigin, cg.refdef.vieworg, direction);
	VectorNormalizeNR(direction);

	if (DotProduct(forward, direction) <= 0.0f)
	{
		return;
	}

	if ((cg_crosshairNamesType.integer == 2) || (cg_crosshairNamesType.integer == 3))
	{
		DrawCrosshairNamesNew2();
	}
	else if (cg_crosshairNamesType.integer == 1)
	{
		DrawCrosshairNamesNew();
	}
	else
	{
		DrawCrosshairNamesOld();
	}

	trap_R_SetColor( NULL );
}

//==============================================================================

static void CG_DrawSpectatorBox ( void )
{
	/*
	static float colBack[4] = { 0, 0, 0, 0.7f };
	static float colLine[4] = { .5f, .5f, .5f, .9f };

	CG_DrawRect ( 169, 404, 302, 72, 1, colLine );
	CG_FillRect ( 170, 405, 300, 70, colBack );
	*/

	static float  colourBlack[4] = { 0.0f, 0.0f, 0.0f, 0.25f};
	static float  colourGrey[4]  = { 0.8f, 0.8f, 0.8f, 0.25f};
	static float  colourRed[4]	 = { 0.5f, 0.0f, 0.0f, 0.25f};
	static float  colourBlue[4]  = { 0.0f, 0.0f, 0.5f, 0.25f};
	int 	  team;

	//@B1n in clear mode, dont 
	if (cg_cleanFollow.integer) {
		return;
	}

	team = cgs.clientinfo[cg.predictedPlayerState.clientNum].team;

	CG_DrawRect(169, 404, 302, 72, 1, colourGrey);

	if (cg.snap)
	{
		if (team == TEAM_RED)
		{
			CG_FillRect(170, 405, 300, 70, colourRed);
		}
		else if (team == TEAM_BLUE)
		{
			CG_FillRect(170, 405, 300, 70, colourBlue);
		}
		else
		{
			CG_FillRect(170, 405, 300, 70, colourBlack);
		}
	}
	else
	{
		CG_FillRect(170, 405, 300, 70, colourBlack);
	}
}

/*
=================
CG_DrawSpectator
=================
*/
static void CG_DrawSpectator(void)
{
	// Don't draw spectator, if following
	if (cg.snap->ps.pm_flags & PMF_FOLLOW)
		return;

	if (cg_cleanFollow.integer) {
		return;
	}

	CG_DrawSpectatorBox ( );

	CG_Text_Paint ( 320 - CG_Text_Width ( "SPECTATOR", 0.425f, 0 ) / 2,
			430, 0.40f, colorWhite,
			"SPECTATOR",
			0, 0, 3 );		   //ITEM_TEXTSTYLE_OUTLINED );

	CG_Text_Paint ( 180, 455, 0.25f, colorWhite,
			"Press 'ESC' to open menu and join game",
			0, 0, 0 );

	CG_Text_Paint ( 180, 470, 0.25f, colorWhite,
			"Press 'FIRE' or 'USE' to switch to follow mode",
			0, 0, 0 );
}

/**
 * $(function)
 */
static void CG_DrawGhost(void)
{
	if (cg_cleanFollow.integer) {
		return;
	}

	CG_DrawSpectatorBox ( );

	CG_Text_Paint ( 320 - CG_Text_Width ( "GHOST", 0.425f, 0 ) / 2,
			430, 0.425f, colorWhite,
			"GHOST",
			0, 0, 3 );		   //ITEM_TEXTSTYLE_OUTLINED );

	CG_Text_Paint ( 180, 460, 0.25f, colorWhite,
			"Press 'FIRE' or 'USE' to switch to haunt mode",
			0, 0, 0 );
}

/*
=================
CG_DrawFollow
=================
*/
static qboolean CG_DrawFollow( void )
{
	const char	*name;
	int 	widthSpec;
	int 	widthName;
	int 	widthTotal;
	char		*follow;
	static int	lastFollow = -1;
	int textHeight = 430;

	// Must be following
	if (!(cg.snap->ps.pm_flags & PMF_FOLLOW))
	{
		lastFollow = -1;
		return qfalse;
	}

	// Kill overlays that just started
	if (lastFollow != cg.snap->ps.clientNum)
	{
		cg.overlayColorTime = 0;
		lastFollow		= cg.snap->ps.clientNum;
	}

	if (cg.snap->ps.pm_flags & PMF_HAUNTING)
	{
		follow = "haunting ";
	}
	else
	{
		follow = "following ";
	}

	CG_DrawSpectatorBox ( );

	if (cg_cleanFollow.integer) {
		textHeight = 475;
	}

	// Get the player name
	name	   = cgs.clientinfo[cg.snap->ps.clientNum].name;

	widthSpec  = CG_Text_Width ( follow, 0.425f, 0 );
	widthName  = CG_Text_Width ( name, 0.425f, 0 );
	widthTotal = (widthSpec + widthName);

	CG_Text_Paint ( 320 - widthTotal / 2, textHeight, 0.40f, colorWhite,
			follow,
			0, 0, 3 );		   //ITEM_TEXTSTYLE_OUTLINED );

	CG_Text_Paint ( 320 - widthTotal / 2 + widthSpec, textHeight, 0.40f,
			CG_TeamColor(cgs.clientinfo[cg.snap->ps.clientNum].team),
			name,
			0, 0, 3 );		   //ITEM_TEXTSTYLE_OUTLINED );

	//@B1n in clear mode, dont 
	if (cg_cleanFollow.integer) {
		return qtrue;
	}

	if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR)
	{
		CG_Text_Paint ( 180, 440, 0.25f, colorWhite,
				"Press 'ESC' to open menu and join game",
				0, 0, 0 );
	}

	if (cg.snap && (cg.snap->ps.pm_flags & PMF_FOLLOW) && (cg.snap->ps.eFlags & EF_CHASE))
	{
		CG_Text_Paint ( 180, 455, 0.25f, colorWhite,
				va("Press 'USE' to stop %s", follow),
				0, 0, 0 );
	}
	else
	{
		CG_Text_Paint ( 180, 455, 0.25f, colorWhite,
				va("Press 'USE' to switch to chase cam"),
				0, 0, 0 );
	}

	CG_Text_Paint ( 180, 470, 0.25f, colorWhite,
			"Press 'FIRE' to follow the next player",
			0, 0, 0 );

	return qtrue;
}

/*
=================
CG_DrawVote
=================
*/
static void CG_DrawVote(void)
{
	char  *s;
	int   sec;

	if (!cgs.voteTime)
	{
		return;
	}

	// play a talk beep whenever it is modified
	if (cgs.voteModified)
	{
		cgs.voteModified = qfalse;
		if (cg_chatSound.integer > 0) {
			trap_S_StartLocalSound( cgs.media.talkSound, CHAN_LOCAL_SOUND );
		}
	}

	sec = (VOTE_TIME - (cg.time - cgs.voteTime)) / 1000;

	if (sec < 0)
	{
		sec = 0;
	}

	s = va("VOTE(%i):%s yes:%i no:%i", sec, cgs.voteString, cgs.voteYes, cgs.voteNo);
	CG_Text_Paint ( 5, 85, 0.28f, colorWhite, s, 0, 0, 4 );

	s = "to vote press ESC then click Vote, or F1(Yes) or F2(No)";
	CG_Text_Paint ( 5, 102, 0.28f, colorWhite, s, 0, 0, 4 );
}

/*
=================
CG_DrawTeamVote
=================
*/
static void CG_DrawTeamVote(void)
{
	char  *s;
	int   sec, cs_offset;

	if (cgs.clientinfo->team == TEAM_RED)
	{
		cs_offset = 0;
	}
	else if (cgs.clientinfo->team == TEAM_BLUE)
	{
		cs_offset = 1;
	}
	else
	{
		return;
	}

	if (!cgs.teamVoteTime[cs_offset])
	{
		return;
	}

	// play a talk beep whenever it is modified
	if (cgs.teamVoteModified[cs_offset])
	{
		cgs.teamVoteModified[cs_offset] = qfalse;
		if (cg_chatSound.integer > 0) {
			trap_S_StartLocalSound( cgs.media.talkSound, CHAN_LOCAL_SOUND );
		}
	}

	sec = (VOTE_TIME - (cg.time - cgs.teamVoteTime[cs_offset])) / 1000;

	if (sec < 0)
	{
		sec = 0;
	}
	s = va("TEAMVOTE(%i):%s yes:%i no:%i", sec, cgs.teamVoteString[cs_offset],
		   cgs.teamVoteYes[cs_offset], cgs.teamVoteNo[cs_offset] );
//	CG_DrawSmallString( 0, 90, s, 1.0F );

	CG_Text_Paint ( 5, 85, 0.28f, colorWhite, s, 0, 0, 4 );
}

/**
 * $(function)
 */
static qboolean CG_DrawScoreboard(void)
{
	static qboolean  firstTime = qtrue;
	float		 fade, *fadeColor;

	if (menuScoreboard)
	{
		menuScoreboard->window.flags &= ~WINDOW_FORCED;
	}

	if (cg_paused.integer)
	{
		cg.deferredPlayerLoading = 0;
		firstTime		 = qtrue;
		return qfalse;
	}

	// don't draw scoreboard during death while warmup up
	if (cg.warmup && !cg.showScores)
	{
		return qfalse;
	}

	if (cg.showScores || (cg.predictedPlayerState.pm_type == PM_INTERMISSION))
	{
		fade	  = 1.0;
		fadeColor = colorWhite;
	}
	else
	{
		fadeColor = CG_FadeColor( cg.scoreFadeTime, FADE_TIME );

		if (!fadeColor)
		{
			// next time scoreboard comes up, don't print killer
			cg.deferredPlayerLoading = 0;
			cg.killerName[0]	 = 0;
			firstTime		 = qtrue;
			return qfalse;
		}
		fade = *fadeColor;
	}

	if (menuScoreboard == NULL)
	{
		if ((cgs.gametype >= GT_TEAM) && (cgs.gametype != GT_JUMP))
		{
			menuScoreboard = Menus_FindByName("teamscore_menu");
		}
		else
		{
			menuScoreboard = Menus_FindByName("score_menu");
		}
	}

	if (menuScoreboard)
	{
		if (firstTime)
		{
			CG_SetScoreSelection(menuScoreboard);
			firstTime = qfalse;
		}
		CG_ScoreboardInit ( );
		Menu_Paint(menuScoreboard, qtrue);
	}

	return qtrue;
}

/*
=================
CG_DrawIntermission
=================
*/
static void CG_DrawIntermission( void )
{
	cg.scoreFadeTime	 = cg.time;
	cg.scoreBoardShowing = CG_DrawScoreboard();
}

/*
=================
CG_DrawWarmup
=================
*/
static void CG_DrawWarmup( void )
{
	int 	w;
	int 	sec;
	float		scale;
	int 	cw;
	const char	*s;
	qboolean	match;

	sec = cg.warmup;

	if (!sec)
	{
		return;
	}

	//negatives mean we're in Waiting For Players zone
	if (sec < 0)
	{
		s = "Waiting for players";
		CG_Text_Paint ( 320 - CG_Text_Width ( s, 0.425f, 0 ) / 2,
				64,
				0.425f,
				colorWhite,
				s,
				0, 0,
				ITEM_TEXTSTYLE_OUTLINED );

		cg.warmupCount = 0;
		return;
	}

	sec = (sec - cg.time) / 1000;

	if (sec < 1)
	{
		cg.warmup = 0;
		sec   = 0;
		return;
	}

	// If match is ready.
	if ((cg.matchState & UT_MATCH_ON) && (cg.matchState & UT_MATCH_RR) && (cg.matchState & UT_MATCH_BR))
	{
		match = qtrue;
	}
	else
	{
		match = qfalse;
	}

	if (cgs.gametype == GT_FFA)
	{
		s = "Free For All";
	}
	else if (cgs.gametype == GT_LASTMAN)
	{
		s = "Last Man Standing";
	}
	else if (cgs.gametype == GT_JUMP)
	{
		s = "Jump";
	}

	else if (cgs.gametype == GT_TEAM)
	{
		// If we're in match mode.
		if (match)
		{
			s = "Team Deathmatch Match";
		}
		else
		{
			s = "Team Deathmatch";
		}
	}
	else if (cgs.gametype == GT_UT_SURVIVOR)
	{
		// If we're in match mode.
		if (match)
		{
			s = "Team Survivor Match";
		}
		else
		{
			s = "Team Survivor";
		}
	}
	else if (cgs.gametype == GT_CTF)
	{
		// If we're in match mode.
		if (match)
		{
			s = "Capture the Flag Match";
		}
		else
		{
			s = "Capture the Flag";
		}
	}/* else if ( cgs.gametype == GT_UT_BTB ) {
		s = "Search and Destroy";
	/	} else if ( cgs.gametype == GT_UT_C4 ) {
		s = "Infiltrate";
	} */
	else if (cgs.gametype == GT_UT_ASSASIN)
	{
		// If we're in match mode.
		if (match)
		{
			s = "Follow the Leader Match";
		}
		else
		{
			s = "Follow the Leader";
		}
	}
	else if (cgs.gametype == GT_UT_CAH)
	{
		// If we're in match mode.
		if (match)
		{
			s = "Capture and Hold Match";
		}
		else
		{
			s = "Capture and Hold";
		}
	}
	else if(cgs.gametype == GT_UT_BOMB)
	{
		// If we're in match mode.
		if (match)
		{
			s = "Bomb Match";
		}
		else
		{
			s = "Bomb";
		}
	}
	else
	{
		s = "";
	}
#ifdef MISSIONPACK
	w = CG_Text_Width(s, 0.6f, 0);
	CG_Text_Paint(320 - w / 2, 50, 0.6f, colorWhite, s, 0, 0, ITEM_TEXTSTYLE_OUTLINED);
#else
	w = CG_DrawStrlen( s );

	if (w > 640 / GIANT_WIDTH)
	{
		cw = 640 / w;
	}
	else
	{
		cw = GIANT_WIDTH;
	}
	CG_DrawStringExt( 320 - w * cw / 2, 25, s, colorWhite,
			  qfalse, qtrue, cw, (int)(cw * 1.1), 0 );
#endif

	// If we're in match mode.
	if ((sec == 0) && match)
	{
		// Set text.
		s = "Live, Play Ball!";
	}
	else
	{
		if ((strlen(cg.roundstat) > 0) && (cg.roundstat[0] > '0'))
		{
			s = va("Second Half Starts in: %i", sec );
		}
		else
		{
			// Set text.
			s = va("Starts in: %i", sec );
		}
	}

	if (sec != cg.warmupCount)
	{
		cg.warmupCount = sec;
/* URBAN TERROR - Not using these sounds
		switch ( sec ) {
		case 0:
			trap_S_StartLocalSound( cgs.media.count1Sound, CHAN_ANNOUNCER );
			break;
		case 1:
			trap_S_StartLocalSound( cgs.media.count2Sound, CHAN_ANNOUNCER );
			break;
		case 2:
			trap_S_StartLocalSound( cgs.media.count3Sound, CHAN_ANNOUNCER );
			break;
		default:
			break;
		}
*/
	}
	scale = 0.45f;
	cw	  = 16;
	scale = 0.45f;

	switch (cg.warmupCount)
	{
		case 2:

//	CG_SetColorOverlay( 0, 0, 0, 0, 5000, UT_OCFADE_LINEAR_OUT, 500 );

			break;
	}

#ifdef MISSIONPACK
	w = CG_Text_Width(s, scale, 0);
	CG_Text_Paint(320 - w / 2, 85, scale, colorWhite, s, 0, 0, ITEM_TEXTSTYLE_OUTLINED);
#else
	w = CG_DrawStrlen( s );
	CG_DrawStringExt( 320 - w * cw / 2, 70, s, colorWhite,
			  qfalse, qtrue, cw, (int)(cw * 1.5), 0 );
#endif
}

//==================================================================================
#ifdef MISSIONPACK
/*
=================
CG_DrawTimedMenus
=================
*/
void CG_DrawTimedMenus(void)
{
	if (cg.voiceTime)
	{
		int  t = cg.time - cg.voiceTime;

		if (t > 2500)
		{
			Menus_CloseByName("voiceMenu");
			trap_Cvar_Set("cl_conXOffset", "0");
			cg.voiceTime = 0;
		}
	}
}
#endif

//
// Bomb Code
//

/*=---- Draw defuse timer overlay ----=*/

#if 1
void CG_DrawDefuseTimer(void)
{
	const char	*s;
	int 	difuseTime;
	vec4_t		colour;
	float		x, y, w, h;
	float		scale;
	int 	i;

	// If no valid snap.
	if (!cg.snap)
	{
		return;
	}

	// If we have the red flag.
	if ((cg.predictedPlayerEntity.currentState.powerups & POWERUP(PW_REDFLAG)) && !(cg.predictedPlayerState.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_GHOST))
	{
		// Set dimensions.
		x = 304;
		y = 444;
		w = 32;
		h = 32;

		// Draw red flag icon.
		CG_DrawPic(x, y, w, h, cgs.media.colorFlagShaders[(cgs.gametype == GT_JUMP) ? UT_SKIN_RED :  CG_WhichSkin(TEAM_RED)][0]);

		return;
	}

	// If we have the blue flag.
	if ((cg.predictedPlayerEntity.currentState.powerups & POWERUP(PW_BLUEFLAG)) && !(cg.predictedPlayerState.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_GHOST))
	{
		// Set dimensions.
		x = 304;
		y = 444;
		w = 32;
		h = 32;

		// Draw blue flag icon.
		CG_DrawPic(x, y, w, h, cgs.media.colorFlagShaders[(cgs.gametype == GT_JUMP) ? UT_SKIN_BLUE : CG_WhichSkin(TEAM_BLUE)][0]);

		return;
	}

	// If we're packing the bomb.
	if ((cg.predictedPlayerEntity.currentState.powerups & POWERUP(PW_CARRYINGBOMB)) && !(cg.predictedPlayerState.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_GHOST))
	{
		// If we're not planting.
		if (!(cg.predictedPlayerState.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_PLANTING))
		{
			// Set dimensions.
			x = 296;
			y = 434;
			w = 48;
			h = 48;

			// Loop through bomb sites.
			for (i = 0; i < cg.NumBombSites; i++)
			{
				// If we're close to that site.
				if (Distance(cg.predictedPlayerEntity.currentState.pos.trBase, cgs.cg_BombSites[i].Position) < BOMB_RADIUS) // (cgs.cg_BombSites[i].Range - 100))
				{
					// If we're moving too fast.
					if ((VectorLength(cg.predictedPlayerState.velocity) >= 75.0f) || (cg.predictedPlayerState.groundEntityNum == ENTITYNUM_NONE))
					{
						break;
					}

					// Calculate scale.
					scale = (float)fabs(cos((float)cg.time / 150.0f)) * 6.0f;

					// Scale icon.
					x -= scale;
					y -= scale + 2;
					w += scale * 2.0f;
					h += scale * 2.0f;

					break;
				}
			}

			// Draw bomb icon.
			CG_DrawPic(x, y, w, h, trap_R_RegisterShader("icons/ammo/bomb"));
		}

		return;
	}

	// If we're difusing.
	if ((cg.predictedPlayerState.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_DEFUSING) && !(cg.predictedPlayerState.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_GHOST))
	{
		// If an invalid time.
		if ((cg.DefuseStartTime == 0) || (cg.time < cg.DefuseStartTime))
		{
			return;
		}

		// Calculate difuse time.
		difuseTime = cg.time - cg.DefuseStartTime;

		// If time elapsed.
		if (difuseTime > cg.DefuseLength)
		{
			return;
		}

		// Calculate time scale.
		scale = (float)difuseTime / (float)cg.DefuseLength;

		// Set dimensions.
		x = 0;
		y = 227;
		w = 641;
		h = 26;

		// Set colour.
		colour[0] = 0.8f;
		colour[1] = 0.8f;
		colour[2] = 0.8f;
		colour[3] = 0.5f;

		// Draw border.
		CG_DrawRect(x - 1, y - 1, w + 2, h + 2, 1, colour);

		// Set colour.
		if (cgs.clientinfo[cg.predictedPlayerState.clientNum].team == TEAM_RED)
		{
			colour[0] = 0.5f;
			colour[1] = 0.0f;
			colour[2] = 0.0f;
			colour[3] = 0.5f;
		}
		else
		{
			colour[0] = 0.0f;
			colour[1] = 0.0f;
			colour[2] = 0.5f;
			colour[3] = 0.5f;
		}

		// Draw progress bar.
		CG_FillRect(x, y, w * scale, h, colour);

		// Set colour.
		colour[0] = 1.0f;
		colour[1] = 1.0f;
		colour[2] = 1.0f;
		colour[3] = 1.0f;

		// Set text.
		switch ((cg.time >> 9) & 3)
		{
			case 3:
				s = va("DEFUSING...     %02d%%", (int)(100.0f * scale));
				break;

			case 2:
				s = va("DEFUSING..      %02d%%", (int)(100.0f * scale));
				break;

			case 1:
				s = va("DEFUSING.       %02d%%", (int)(100.0f * scale));
				break;

			default:
				s = va("DEFUSING        %02d%%", (int)(100.0f * scale));
				break;
		}

		// Set dimensions.
		x = 64;
		y = 247;

		// Draw text.
		CG_Text_Paint(x, y, 0.315f, colour, s, 0, 0, 3);

		return;
	}
}
#else
/**
 * $(function)
 */
void CG_DrawDefuseTimer(void)	// TODO: Plant timer too!
{ const int   BarWidth	   = 300;
  const int   BarHeight    = 18;
  const int   BarX	   = ((640 / 2) - (BarWidth / 2));
  const int   BarY	   = 240 + ((240 / 2) - (BarHeight / 2));	  // position center lower half of the screen
  const int   MSUD	   = 50;					  // MSec update
  const char  *DefuseText  = va( "Defusing" );
	// TEMP!
  const char  *DefuseText2 = va( "PLANTING BOMB!" );
	// END TEMP!
  const char  *s;
  vec4_t	  BackgroundColor = { 0.8f, 0.8f, 0.0f, 0.2f };
  vec4_t	  ForegroundColor = { 1.0f, 1.0f, 0.0f, 0.4f };
  int		  width 	  = 0;
  int		  DefusingTime;
  centity_t   *cent 	  = &cg.predictedPlayerEntity;

	//centity_t *cent=CG_ENTITIES(cg.clientNum);

  if((cg.snap->ps.stats[STAT_HEALTH] <= 0) || (cg.BombDefused == qtrue))
  {
	  // player is dead, don't continue
	  return;
  }

	// TEMP!
  if(cg.snap->ps.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_PLANTING)
  {
	  CG_Text_Paint( 320 - CG_Text_Width( DefuseText2, 0.6f, 0 ) / 2, 240, 0.6f, colorWhite, DefuseText2, 0, 0, ITEM_TEXTSTYLE_OUTLINED );
	  return;
  }

	// END TEMP!
  if((cg.DefuseOverlay == qfalse) && !(cg.snap->ps.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_DEFUSING))
  {
	  // not defusing at all
	  return;
  }
  else if(cg.DefuseOverlay == qfalse && (cg.snap->ps.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_DEFUSING))
  {
	  // Just started
	  cg.DefuseOverlay	 = qtrue;
	  // Protenial problem using this varible since it could interfere with the player's entity lighting
	  s 		 = CG_ConfigString( CG_DEFUSE_START );
	  cg.DefuseStartTime = atoi( s );
	  //cg.DefuseStartTime = ( cent->currentState.loopSound ); // bump the time back if client is already defusing
	  s 		 = CG_ConfigString( CG_DEFUSE_TIMER );
	  cg.DefuseEndTime	 = cg.time + (atoi( s ) * 1000);
  }
  else if(cg.DefuseOverlay == qtrue && !(cg.snap->ps.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_DEFUSING))
  {
	  // just stopped
	  cg.DefuseOverlay	 = qfalse;
	  cg.DefuseStartTime = cg.DefuseEndTime = 0;
	  return;
  }
  else
  {
	  if(cg.pauseState & UT_PAUSE_ON)
	  {
		  cg.DefuseStartTime += cg.frametime;
		  cg.DefuseEndTime	 += cg.frametime;
	  }
	  else if(cg.time >= cg.DefuseEndTime)
	  {
		  cg.DefuseOverlay	 = qfalse;
		  cg.DefuseStartTime = cg.DefuseEndTime = 0;
		  cg.BombDefused	 = qtrue;	// bomb has been defused
		  return;
	  }
	  // Work out how much of the bar should be rendered
	  DefusingTime = (cg.DefuseEndTime - cg.DefuseStartTime) / MSUD;
	  width 	   = (((cg.time - cg.DefuseStartTime) / MSUD) * BarWidth) / DefusingTime;
	  // outter bar
	  // 640 >> 1 | 240 >> 1 | 240 + ( 240 >> 1 )
	  CG_FillRect( 320 - ((BarWidth + 4) >> 1), 360 - ((BarHeight + 4) >> 1), BarWidth + 4, BarHeight + 4, BackgroundColor );
	  // Actual updated bar
	  CG_FillRect( BarX, BarY, (int)width, BarHeight, ForegroundColor );
	  // 640 >> 1 | 480 >> 1
	  CG_Text_Paint( 320 - CG_Text_Width( DefuseText, 0.6f, 0 ) / 2, 240, 0.6f, colorWhite, DefuseText, 0, 0, ITEM_TEXTSTYLE_OUTLINED );
  }
}
#endif

//////////////////////////////////////////////

static void CG_DrawBombFX2D(void)
{
	vec4_t	colour;
	float	scale;
	int deltaTime;

	// If bomb didn't explode.
	if (!cg.bombExploded)
	{
		return;
	}

	// Calculate time delta.
	deltaTime = cg.time - cg.bombExplodeTime;

	// If bomb explosion time is over.
	if (deltaTime > 12000)
	{
		return;
	}

	// Calculate colour scale.
	scale	  = 1.0f - (float)(1.0f + sin(M_PI + ((M_PI / 2) * (1.0f - ((float)(deltaTime >= 10000 ? 10000 : deltaTime) / 10000.0f)))));

	colour[0] = (251.0f / 255.0f) * scale;
	colour[1] = (77.0f / 255.0f) * scale;
	colour[2] = (70.0f / 255.0f) * scale;
	colour[3] = 1.0f;

	// Set colour.
	trap_R_SetColor(colour);

	// Apply bright shader.
	trap_R_DrawStretchPic(cg.refdef.x, cg.refdef.y, cg.refdef.width, cg.refdef.height, 0, 0, 1, 1, cgs.media.bombBrightShader);

	// Clear colour.
	trap_R_SetColor(NULL);
}

//////////////////////////////////////////////

/*
=================
CG_Draw2D
=================
*/
static void CG_Draw2D( void )
{
	int 	matchmode;
	const char	*info;
	qboolean	takeshot = qfalse;

//	const	char *soz;

/*
#ifdef MISSIONPACK
	if (cgs.orderPending && cg.time > cgs.orderTime) {
		CG_CheckOrderPending();
	}
#endif
*/
	// --- HACK ---
/*	if( cg.predictedPlayerState.stats[ STAT_UTPMOVE_FLAGS ] & UT_PMF_GHOST || cg.snap->ps.pm_flags & PMF_FOLLOW ) {
		cg.overlayColorTime  = 0;
		cg.overlayShaderTime = 0;
		cg.overlayFade = 0.0f;
		cg.overlayColorFadeTime = 0;
		cg.overlayColorFade = UT_OCFADE_LINEAR_IN;
	} */

	// Draw smoke gren stuff
	CG_DrawSmokeOverlay();

	// draw space invader game if playing
	if (cg_spaceInvaders.integer)
	{
		inv_doround();
		return;
	}

	// if we are taking a levelshot for the menu, don't draw anything
	if (cg.levelShot)
	{
		return;
	}

	// Draw bomb FX before anything else.
	CG_DrawBombFX2D();

	CG_DrawSpectatorData(); // Oswald's Fav - DensitY

	if (cg_draw2D.integer == 0)
	{
		if (!cg_thirdPerson.integer && (cg.snap->ps.stats[STAT_HEALTH] > 0))
		{
			if (utPSIsItemOn ( &cg.snap->ps, UT_ITEM_NVG ))
			{
				CG_DrawNVG ( 0 );
				CG_DrawNVG ( 1 );
			}

			if (!cg_thirdPerson.integer && (cg.zoomed > 0))
			{
				CG_DrawSniperCrosshair();
			}
		}

		CG_DrawShaderOverlay ( );
		CG_DrawColorOverlay ( );
#ifdef FLASHNADES
		CG_DrawFlashGrenadeFade ( );
#endif
		CG_DrawChat ( );
		CG_DrawMessages ( );
		return;
	}

	if (cg.snap->ps.pm_type == PM_INTERMISSION)
	{
		CG_DrawChat ( );
		CG_DrawMessages ( );
		CG_DrawIntermission();

		if (cg.firstframeofintermission == 0)
		{
			//	trap_S_StartBackgroundTrack("music/mainmenu.wav","true");
			//do intermission first frame stuff like start music
		}

		if (cg.firstframeofintermission == 1)
		{
			// Start the music!
			//trap_S_StartBackgroundTrack("music/mainmenu.wav","");
			info	  = CG_ConfigString( CS_SERVERINFO );
			matchmode = atoi(Info_ValueForKey( info, "g_matchmode" ));
//#ifdef ENCRYPTED
//			SendHaxValues(); //r00t: Send final stats at the end of round
//#endif
			if (cg_autoScreenshot.integer && !cg.demoPlayback)
			{
				//If we've got cg_autoscreenshot on 2, only take the shot if its matchmode
				takeshot = qfalse;

				if ((matchmode && (cg_autoScreenshot.integer == 2)))
				{
					takeshot = qtrue;
				}

				if (cg_autoScreenshot.integer == 1)
				{
					takeshot = qtrue;
				}

				if (takeshot == qtrue)
				{
					if (cg.numScores > 0)
					{
						// Take an auto screenshot
						qtime_t  time;
						int  i, kills, deaths;
						int  redscore, bluescore;
						char	 *mapname;
						char	 gametype[255];

						Com_sprintf(gametype, 255, "%s", "FFA");

						switch (cgs.gametype)
						{
							case GT_FFA:
								Com_sprintf(gametype, 255, "%s", "FFA");
								break;

							case GT_LASTMAN:
								Com_sprintf(gametype, 255, "%s", "LMS");
								break;

							case GT_JUMP:
								Com_sprintf(gametype, 255, "%s", "JUMP");
								break;

							case GT_CTF:
								Com_sprintf(gametype, 255, "%s", "CTF");
								break;

							case GT_UT_CAH:
								Com_sprintf(gametype, 255, "%s", "CAH");
								break;

							case GT_UT_ASSASIN:
								Com_sprintf(gametype, 255, "%s", "FTL");
								break;

							case GT_UT_SURVIVOR:
								Com_sprintf(gametype, 255, "%s", "TS");
								break;

							case GT_TEAM:
								Com_sprintf(gametype, 255, "%s", "TDM");
								break;

							case GT_UT_BOMB:
								Com_sprintf(gametype, 255, "%s", "BOMB");
								break;

							default:
								Com_sprintf(gametype, 255, "NA");
								break;
						}
						kills	= 0;
						deaths	= 0;
						trap_RealTime(&time); //get time
						mapname = Info_ValueForKey( CG_ConfigString( CS_SERVERINFO ), "mapname" );
						mapname = Q_strupr(mapname);

						for (i = 0; i < cgs.maxclients; i++)
						{
							if (!cgs.clientinfo[i].infoValid)
							{
								continue;
							}

							if (cg.clientNum == i)
							{
								//if (!cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR )
								//{
								kills  = cgs.clientinfo[i].score;
								deaths = cgs.clientinfo[i].deaths;
								break;
								//}
							}
						}
						redscore  = cg.teamScores[0];
						bluescore = cg.teamScores[1];

						if(cgs.gametype < GT_TEAM)
						{
							trap_SendConsoleCommand ( va("screenshotJPEG %s_%s(%d_%d)(RED%d)(BLUE%d)(%d-%d)(%d-%d)\n", mapname, gametype, kills, deaths, redscore, bluescore, time.tm_mday, time.tm_mon + 1, time.tm_hour, time.tm_min ));
						}
						else
						{
							trap_SendConsoleCommand ( va("screenshotJPEG %s_%s(%d_%d)(RED--)(BLUE--)(%d-%d)(%d-%d)\n", mapname, gametype, kills, deaths, time.tm_mday, time.tm_mon + 1, time.tm_hour, time.tm_min ));
						}
						//trap_SendConsoleCommand ( va("\ screenshotJPEG AutoShot_%s_%d-%d_%d:%d_K%d:D%d\n",mapname, time.tm_mday,time.tm_mon,time.tm_hour,time.tm_min,kills,deaths ));
					}
				}

				//@Barbatos: stop recording a demo
				if(matchmode && cg_autoRecordMatch.integer)
				{
					trap_SendConsoleCommand( "stoprecord\n" );
				}

			}
			//do second frame of intermission stuff like auto screenie
		}

		cg.firstframeofintermission++;
		return;
	}

	cg.firstframeofintermission = 0; //we're not on intermission so clear it

	// scan the known entities to see if the crosshair is sighted on one
	CG_ScanForCrosshairEntity();

	if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR)
	{
		CG_DrawSpectator();
		CG_DrawCrosshairNames();
		CG_DrawCrosshair();
		CG_DrawMiniMap(); // so it is obscured by almost everything else
	}
	else
	if (cg.snap->ps.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_GHOST)
	{
		cg.overlayColorDuration = 0;

		CG_DrawGhost();
		CG_DrawCrosshairNames();
		CG_DrawCrosshair();
		CG_DrawMiniMap(); // so it is obscured by almost everything else
	}
	else
	{
		if (!cg_thirdPerson.integer && (cg.snap->ps.stats[STAT_HEALTH] > 0))
		{
			if (utPSIsItemOn ( &cg.snap->ps, UT_ITEM_NVG ))
			{
				CG_DrawNVG ( 0 );
				CG_DrawNVG ( 1 );
			}

			if (!cg_thirdPerson.integer && (cg.zoomed > 0))
			{
				CG_DrawSniperCrosshair();
			}
			else
			{
				CG_DrawCrosshair();
			}
		}

		CG_DrawMiniMap(); // so it is obscured by almost everything else

		CG_DrawShaderOverlay ( );
		CG_DrawColorOverlay ( );
#ifdef FLASHNADES
		CG_DrawFlashGrenadeFade ( );
#endif
		// don't draw any status if dead or the scoreboard is being explicitly shown
		if (cg.snap->ps.stats[STAT_HEALTH] > 0)
		{
			if (!cg.showScores)
			{
				if (cg_drawStatus.integer)
				{
					Menu_PaintAll();
					CG_DrawTimedMenus();
				}

				CG_DrawCrosshairNames();
				CG_DrawWeaponSelect();
				CG_DrawItemSelect();
				CG_DrawSPGameTimers( );
			}
		}
	}

// Special Warrning for now! - DensitY
/*	if( cg.predictedPlayerState.stats[ STAT_HEALTH ] <= 0 && cgs.gametype >= 4 && cgs.gametype <= 5 ) {
		soz = va( "Ghost/Spectator Mode in TS and FTL is currently getting Fixed. Sorry! - DensitY/TwentySeven" );
		CG_Text_Paint( 320 - CG_Text_Width( soz, 0.25f, 0) / 2, 150, 0.25f,
				colorWhite, soz, 0, 0, ITEM_TEXTSTYLE_OUTLINED);
	} */
// Done
	CG_DrawDefuseTimer();
	CG_DrawChat ( );
	CG_DrawMessages ( );

	CG_DrawVote();
	CG_DrawTeamVote();

	CG_DrawLagometer();

	if (!cg_paused.integer)
	{
		CG_DrawUpperRight();
	}

/*	 if ( cg.predictedPlayerState.pm_flags	& PMF_WATCHING_GTV)
   {
	  const char *s="GTV!";
	   CG_Text_Paint ( 320 - CG_Text_Width ( s, 0.425f, 0 ) / 2, 32,
						0.425f,
						colorWhite,
						s,		0, 0,		ITEM_TEXTSTYLE_OUTLINED );
   } */

	CG_DrawFollow();
	CG_DrawWarmup();
	CG_DrawSurvivorRoundWinner ( );

	DrawRadioWindow();

	// don't draw center string if scoreboard is up
	cg.scoreBoardShowing = CG_DrawScoreboard();

	if (!cg.scoreBoardShowing)
	{
		CG_DrawCenterString();
	}

	// Pause stuff.
	CG_DrawPauseStatus();

	// Match stuff.
	CG_DrawMatchStatus();

	// Do breakout frame.
	breakout_DoFrame();

	// Do snake frame.
	snake_DoFrame();

	// If we need to display debug info.
	if (cg_showmiss.integer)
	{
		if (cg.time - cg.lastPPStime >= 1000)
		{
			cg.lastPPStime	= cg.time;
			cg.lastPPScount = cg.ppsCount;
			cg.ppsCount = 0;
		}

		// Show how many prediction loops we did in the last second.
		CG_Text_Paint(300, 50, 0.25f, g_color_table[7], va("%d pps", cg.lastPPScount), 0, 0, 4);

		if (cg.time - cg.lastSPStime >= 1000)
		{
			cg.lastSPStime	= cg.time;
			cg.lastSPScount = cg.spsCount;
			cg.spsCount = 0;
		}

		// Show how many snapshot loops we did in the last second.
		CG_Text_Paint(300, 70, 0.25f, g_color_table[7], va("%d sps", cg.lastSPScount), 0, 0, 4);
	}
}

/*
=====================
CG_DrawActive

Perform all drawing needed to completely fill the screen
=====================
*/
void CG_DrawActive( stereoFrame_t stereoView )
{
	float	separation;
	vec3_t	baseOrg;
	//unsigned int chk[2];

	// optionally draw the info screen instead
	if (!cg.snap)
	{
		CG_DrawInformation();
		return;
	}
//#ifdef ENCRYPTED
//	chk[0]=*(unsigned int*)(((char*)&cg.refdef)+refdef_offsets[0]); // r00t: FOV hax detect
//	chk[1]=*(unsigned int*)(((char*)&cg.refdef)+refdef_offsets[1]); // r00t: FOV hax detect
//#endif
	if(cg.popupSelectTeamMenu)
	{
		trap_SendConsoleCommand ( "ui_selectteam;" );
		cg.popupSelectTeamMenu = qfalse;
	}
	else if (cg.popupSelectGearMenu)
	{
		trap_SendConsoleCommand ( "ui_selectgear;" );
		cg.popupSelectGearMenu = qfalse;
	}

	switch (stereoView)
	{
		case STEREO_CENTER:
			separation = 0;
			break;

		case STEREO_LEFT:
			separation = -cg_stereoSeparation.value / 2;
			break;

		case STEREO_RIGHT:
			separation = cg_stereoSeparation.value / 2;
			break;

		default:
			separation = 0;
			CG_Error( "CG_DrawActive: Undefined stereoView" );
			break;
	}

	// clear around the rendered view if sized down
	CG_TileClear();

	// offset vieworg appropriately if we're doing stereo separation
	VectorCopy( cg.refdef.vieworg, baseOrg );

	if (separation != 0)
	{
		VectorMA( cg.refdef.vieworg, -separation, cg.refdef.viewaxis[1], cg.refdef.vieworg );
	}

	// draw 3D view
	trap_R_RenderScene( &cg.refdef );

	// restore original viewpoint if running stereo
	if (separation != 0)
	{
		VectorCopy( baseOrg, cg.refdef.vieworg );
	}
//#ifdef ENCRYPTED
//	refdef_latch[0]|=chk[0] - (*(unsigned int*)(((char*)&cg.refdef)+refdef_offsets[0])); // r00t: FOV hax detect
//	refdef_latch[0]|=chk[1] - (*(unsigned int*)(((char*)&cg.refdef)+refdef_offsets[1])); // r00t: FOV hax detect
//#endif
	cg.q3fps = CG_CalcFPS ( );

	// draw status bar and other floating elements
	CG_Draw2D();
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : CG_DrawItemSelect
// Description: Draws the item selection
// Author	  : Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
void CG_DrawItemSelect( void )
{
	int    i;
	int    count;
	int    x, y, w;
	char   *name;
	float  *color;
	qhandle_t handle;
	int team;

	// don't display if dead
	if (cg.predictedPlayerState.stats[STAT_HEALTH] <= 0)
	{
		return;
	}

	color = CG_FadeColor( cg.itemSelectTime, ITEM_SELECT_TIME );

	if (!color)
	{
		return;
	}
	trap_R_SetColor( color );

	// showing item select clears weapon select display, but not the blend blob
	cg.weaponSelectTime = 0;

	// Count the number of items held
	count = 0;

	for(i = 0; i < UT_MAX_ITEMS; i++)
	{
		if (UT_ITEM_GETID(cg.snap->ps.itemData[i]) != 0)
		{
			count++ ;
		}
	}

	x = 320 - count * (80 / 2);
	y = 400;

/*
	{
		vec4_t col = {0.0f / 255.0f, 31.0f / 255.0f ,78.0f / 255.0f, 1.0f };
		vec4_t white = {128.0f / 255.0f, 128.0f / 255.0f ,128.0f / 255.0f, 1.0f };
		int width = count * 80 + 6;
		col[3] = color[3];
		white[3] = color[3];
		CG_FillRect ( 320 - (width/2) - 1, y - 22, width + 2, 69, white );
		CG_FillRect ( 320 - (width/2), y - 21, width, 67, col );
	}
*/

	for (i = 0 ; i < UT_MAX_ITEMS; i++)
	{
		int  itemID = UT_ITEM_GETID( cg.snap->ps.itemData[i] );

		if(!itemID)
		{
			continue;
		}

		CG_RegisterItemVisuals ( itemID );

		{
			vec4_t	bkcolor  = { 0, 0, 0, 0.7f };
			vec4_t	selcolor = { 93.0f / 255.0f, 106.0f / 255.0f, 123.0f / 255.0f, 0.9f };
			vec4_t	white	 = { 128.0f / 255.0f, 128.0f / 255.0f, 128.0f / 255.0f, 1.0f };

			CG_DrawRect ( x + 3, y - 1, 74, 42, 1, white );

			if (itemID == cg.itemSelect)
			{
				CG_FillRect ( x + 4, y, 72, 40, selcolor );

				name = bg_itemlist [cg.itemSelect].pickup_name;
				team = 0;
				if ( itemID == 1 ) {
					team = TEAM_RED;
				} else if ( itemID == 2 ) {
					team = TEAM_BLUE;
				}
				if (team) {
					if (cgs.gametype == GT_JUMP) {
						name = skinInfos[(team==TEAM_RED) ? UT_SKIN_RED : UT_SKIN_BLUE].ui_item_list_flag_name;
					} else {
						name = skinInfos[CG_WhichSkin(team)].ui_item_list_flag_name;
					}
				}

				if (name)
				{
					w = CG_Text_Width( name, 0.25f, 0);
					CG_Text_Paint(x + 76 - w, y - 4, 0.25f, color, name, 0, 0, ITEM_TEXTSTYLE_OUTLINED);
				}
			}
			else
			{
				CG_FillRect ( x + 4, y, 72, 40, bkcolor );
			}
		}

		// draw weapon icon
		trap_R_SetColor( color );
		if ( itemID == 1 ) { // red flag (cf bg_misc::bg_itemlist)
			handle = cgs.media.colorFlagShaders[(cgs.gametype == GT_JUMP) ? UT_SKIN_RED : CG_WhichSkin(TEAM_RED)][0];
		} else if ( itemID == 2 ) { // blue flag (cf bg_misc::bg_itemlist)
			handle = cgs.media.colorFlagShaders[(cgs.gametype == GT_JUMP) ? UT_SKIN_BLUE : CG_WhichSkin(TEAM_BLUE)][0];
		} else {
			handle = cg_items[itemID].icon;
		}
		CG_DrawPic( x + 8 + 16, y + 4, 32, 32, handle);

		x += 80;
	}

	// draw the selected name

	trap_R_SetColor( NULL );
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : CG_DrawNVG
// Description: Draws night vision
// Author	  : Apoxol
// Update	  : 27
///////////////////////////////////////////////////////////////////////////////////////////
static qboolean NvgEntityVisible(centity_t *entity)
{
	trace_t   tr;
	vec3_t	  origin, end;
	vec3_t	  viewForward, viewToEntity;
	qboolean  visible;

	// If an invalid pointer.
	if (!entity)
	{
		return qfalse;
	}

	// If an invalid entity.
	if (!entity->currentValid)
	{
		return qfalse;
	}

	// Clear stuff.
	visible = qfalse;

	// Calculate view direction vector.
	AngleVectorsF(cg.refdefViewAngles, viewForward, NULL, NULL);

	// Calculate view to entity vector.
	VectorSubtract(entity->lerpOrigin, cg.refdef.vieworg, viewToEntity);
	VectorNormalizeNR(viewToEntity);

	// If entity is within fov.
	if (DotProduct(viewForward, viewToEntity) < cos(DEG2RAD(cg.refdef.fov_x / 2)))
	{
		return qfalse;
	}

	// Set trace point.
	origin[0] = entity->lerpOrigin[0];
	origin[1] = entity->lerpOrigin[1];
	origin[2] = entity->lerpOrigin[2] + 8;	// Nudge up abit.

	// Trace.
	CG_Trace(&tr, cg.refdef.vieworg, NULL, NULL, origin, cg.clientNum, MASK_SHOT | CONTENTS_WATER);

	// If entity is visible.
	if ((tr.fraction == 1) || (tr.entityNum == entity->currentState.number))
	{
		visible = qtrue;
	}

	// If entity is not visible.
	if (!visible)
	{
		// Set trace point.
		end[0] = origin[0];
		end[1] = origin[1];
		end[2] = origin[2] + 16;

		// Trace.
		CG_Trace(&tr, cg.refdef.vieworg, NULL, NULL, end, cg.clientNum, MASK_SHOT | CONTENTS_WATER);

		// If entity is visible.
		if ((tr.fraction == 1) || (tr.entityNum == entity->currentState.number))
		{
			visible = qtrue;
		}
	}

	// If entity is not visible.
	if (!visible)
	{
		// Set trace point.
		end[0] = origin[0] + 8;
		end[1] = origin[1];
		end[2] = origin[2];

		// Trace.
		CG_Trace(&tr, cg.refdef.vieworg, NULL, NULL, end, cg.clientNum, MASK_SHOT | CONTENTS_WATER);

		// If entity is visible.
		if ((tr.fraction == 1) || (tr.entityNum == entity->currentState.number))
		{
			visible = qtrue;
		}
	}

	// If entity is not visible.
	if (!visible)
	{
		// Set trace point.
		end[0] = origin[0] - 8;
		end[1] = origin[1];
		end[2] = origin[2];

		// Trace.
		CG_Trace(&tr, cg.refdef.vieworg, NULL, NULL, end, cg.clientNum, MASK_SHOT | CONTENTS_WATER);

		// If entity is visible.
		if ((tr.fraction == 1) || (tr.entityNum == entity->currentState.number))
		{
			visible = qtrue;
		}
	}

	// If entity is not visible.
	if (!visible)
	{
		// Set trace point.
		end[0] = origin[0];
		end[1] = origin[1] + 8;
		end[2] = origin[2];

		// Trace.
		CG_Trace(&tr, cg.refdef.vieworg, NULL, NULL, end, cg.clientNum, MASK_SHOT | CONTENTS_WATER);

		// If entity is visible.
		if ((tr.fraction == 1) || (tr.entityNum == entity->currentState.number))
		{
			visible = qtrue;
		}
	}

	// If entity is not visible.
	if (!visible)
	{
		// Set trace point.
		end[0] = origin[0];
		end[1] = origin[1] - 8;
		end[2] = origin[2];

		// Trace.
		CG_Trace(&tr, cg.refdef.vieworg, NULL, NULL, end, cg.clientNum, MASK_SHOT | CONTENTS_WATER);

		// If entity is visible.
		if ((tr.fraction == 1) || (tr.entityNum == entity->currentState.number))
		{
			visible = qtrue;
		}
	}

	return visible;
}

/**
 * $(function)
 */
static void NvgEntityBoxSize(centity_t *entity, vec3_t size)
{
	vec3_t	mins, maxs;
	float	min, max;
	int i;

	// If an invalid pointer.
	if (!entity)
	{
		return;
	}

	// If an invalid entity.
	if (!entity->currentValid)
	{
		return;
	}

	// If an invalid entity type.
	if ((entity->currentState.eType != ET_ITEM) && (entity->currentState.eType != ET_MISSILE))
	{
		return;
	}

	// Clear.
	VectorClear(mins);
	VectorClear(maxs);

	// Get bounds.
	trap_R_ModelBounds(cg_items[entity->currentState.modelindex].models[0], mins, maxs);

	// Calculate dimensions.
	for (i = 0; i < 3; i++)
	{
		min = (float)fabs(mins[i]);
		max = (float)fabs(maxs[i]);

		if (min > max)
		{
			size[i] = min;
		}
		else
		{
			size[i] = max;
		}

		// Set minimum size if needed.
		if (size[i] < 8)
		{
			size[i] = 8;
		}
	}
}

/**
 * $(function)
 */
void NvgWorldToScreen(vec3_t world, vec2_t screen)
{
	vec3_t	viewForward, viewRight, viewUp;
	vec3_t	deltaVec, localVec;

	// Calculate our view direction.
	AngleVectors(cg.refdefViewAngles, viewForward, viewRight, viewUp);

	// Transform vector from world to local space.
	VectorSubtract(world, cg.refdef.vieworg, deltaVec);

	localVec[0] = DotProduct(deltaVec, viewRight);
	localVec[1] = DotProduct(deltaVec, viewUp);
	localVec[2] = DotProduct(deltaVec, viewForward);

	// Transform vector from local to screen space.
	screen[0] = ((((640 / 2) / tan(DEG2RAD(cg.refdef.fov_x / 2))) * localVec[0]) / localVec[2]) + (640 / 2);
	screen[1] = ((-1.0f * ((480 / 2) / tan(DEG2RAD(cg.refdef.fov_y / 2))) * localVec[1]) / localVec[2]) + (480 / 2);
}

#if 1
/**
 * $(function)
 */
void CG_DrawNVG(int pass)
{
	centity_t  *entity;
	vec3_t	   viewRight, viewUp;
	vec3_t	   size, position;
	vec3_t	   square3d[4];
	vec2_t	   square2d[4];
	float	   delta, width, height;
	int    seed;
	int    i, j;
	vec4_t	   colour;

	seed = cg.time;

	if (pass == 0)
	{
		// Draw shader.
		colour[0] = 0;
		colour[1] = 0;
		colour[2] = 0;
		colour[3] = 0;
		trap_R_SetColor( colour );
		// trap_R_DrawStretchPic(cg.refdef.x, cg.refdef.y, cg.refdef.width,cg.refdef.height, 0, 0, 1, 1, cgs.media.nvgStatic);
	}

	if (pass == 1)
	{
		// Loop through entities.
		for (i = 0; i < MAX_GENTITIES; i++)
		{
			// Set pointer.
			entity = CG_ENTITIES(i);

			// If an invalid pointer.
			if (!entity)
			{
				continue;
			}

			// If an invalid entity.
			if (!entity->currentValid)
			{
				continue;
			}

			// If this is a valid entity type.
			if ( /* entity->currentState.eType == ET_ITEM	||*/
				(entity->currentState.eType == ET_MISSILE) ||
				((entity->currentState.eType == ET_PLAYER) && !(entity->currentState.eFlags & EF_DEAD) && (entity->currentState.clientNum != cg.clientNum)))
			{
				// If entity is visible.
				if (NvgEntityVisible(entity))
				{
					// If wasn't visible before.
					if (cg.nvgVisibleTime[entity->currentState.number] == 0)
					{
						// If this is a player.
						/*if (entity->currentState.eType == ET_PLAYER)
						{
							// If this is not a team based game type or this dude not on our team.
							if (cgs.gametype < GT_TEAM ||
								cgs.clientinfo[entity->currentState.clientNum].team != cgs.clientinfo[cg.predictedPlayerState.clientNum].team)
							{
								// If the dude didn't fire.
								if (cgs.clientinfo[entity->currentState.clientNum].muzzleFlashTime == 0 ||
									(cg.time - cgs.clientinfo[entity->currentState.clientNum].muzzleFlashTime) > 1500)
								{
									// If we're not aiming at him and he didn't fire.
									if (cg.crosshairColorClientNum != entity->currentState.clientNum || (cg.time - cg.crosshairClientTime) > 1500 ||
										(cg.time - cgs.clientinfo[cg.clientNum].muzzleFlashTime) > 1500)
									{
										// If he's not moving too fast.
										if (VectorLength(entity->currentState.pos.trDelta) <= 200.0f)
										{
											continue;
										}
									}
								}
							}
						}*/

						// Set time.
						if (entity->currentState.eType == ET_ITEM)
						{
							cg.nvgVisibleTime[entity->currentState.number] = cg.time + 350;
						}
						else
						{
							cg.nvgVisibleTime[entity->currentState.number] = cg.time + 500;
						}
					}

					// Clear.
					VectorClear(size);

					// If this is a simple entity.
					if ((entity->currentState.eType == ET_ITEM) || (entity->currentState.eType == ET_MISSILE))
					{
						// Get size.
						NvgEntityBoxSize(entity, size);
					}
					else
					{
						// Set size.
						size[0] = 30;
						size[1] = 30;

						// If player is crouched.
						if (((entity->currentState.legsAnim & ~ANIM_TOGGLEBIT) == LEGS_WALKCR) ||
							((entity->currentState.legsAnim & ~ANIM_TOGGLEBIT) == LEGS_IDLECR))
						{
							size[2] = 48;
						}
						else
						{
							size[2] = 62;
						}
					}

					// Calculate width, height.
					if (size[0] > size[1])
					{
						width = size[0];
					}
					else
					{
						width = size[1];
					}

					height = size[2];

					// Calculate view direction.
					AngleVectorsRU(cg.refdefViewAngles, NULL, viewRight, viewUp);

					// Calculate point.
					VectorSubtract(entity->lerpOrigin, cg.refdef.vieworg, position);
					VectorNormalize(position,delta);

					// If we need to add the "lock-on" effect.
					if (entity->currentState.eType == ET_ITEM)
					{
						if (cg.time - cg.nvgVisibleTime[entity->currentState.number] <= 0)
						{
							delta *= 1.0f - ((float)(cg.nvgVisibleTime[entity->currentState.number] - cg.time) / 350.0f);
						}
						else
						{
							cg.nvgVisibleTime[entity->currentState.number] = cg.time;
						}
					}
					else if (cg.time - cg.nvgVisibleTime[entity->currentState.number] <= 0)
					{
						// Scale distance.
						delta *= 1.0f - ((float)(cg.nvgVisibleTime[entity->currentState.number] - cg.time) / 500.0f);
					}
					else
					{
						cg.nvgVisibleTime[entity->currentState.number] = cg.time;
					}

					VectorMA(cg.refdef.vieworg, delta, position, position);

					// Calculate 3D square.
					VectorMA(position, -width, viewRight, square3d[0]);
					VectorMA(square3d[0], height, viewUp, square3d[0]);

					VectorMA(position, width, viewRight, square3d[1]);
					VectorMA(square3d[1], height, viewUp, square3d[1]);

					VectorMA(position, width, viewRight, square3d[2]);
					VectorMA(square3d[2], -height, viewUp, square3d[2]);

					VectorMA(position, -width, viewRight, square3d[3]);
					VectorMA(square3d[3], -height, viewUp, square3d[3]);

					// Calculate 2D square.
					for (j = 0; j < 4; j++)
					{
						NvgWorldToScreen(square3d[j], square2d[j]);
					}

					// If this is an item.
					if (entity->currentState.eType == ET_ITEM)
					{
						// Set colour.
						colour[0] = 0.0f;
						colour[1] = 0.0f;
						colour[2] = 1.0f;

						// Set alpha.
						if (delta > 4500)
						{
							colour[3] = 0.1f;
						}
						else
						{
							colour[3] = 1.0f - (delta / 4500);
						}
					}
					// If this is a missile.
					else if (entity->currentState.eType == ET_MISSILE)
					{
						// Set colour.
						colour[0] = 1.0f;
						colour[1] = (float)(((cg.time >> 7) & 1) ? 1.0f : 0.0f);
						colour[2] = 0.0f;

						// Set alpha.
						if (delta > 4500)
						{
							colour[3] = 0.1f;
						}
						else
						{
							colour[3] = 1.0f - (delta / 4500);
						}
					}
					// Or a player.
					else
					{
						// If this is a team based game and the dude on our team.
						if ((cgs.gametype >= GT_TEAM) && (cgs.gametype != GT_JUMP) && (cgs.clientinfo[entity->currentState.clientNum].team == cgs.clientinfo[cg.predictedPlayerState.clientNum].team))
						{
							// Set colour.
							colour[0] = 0.0f;
							colour[1] = 0.0f;
							colour[2] = 1.0f;
							colour[3] = 1.0f;

							if (cg_nvg.integer == 2)
							{
								colour[0] = 1.0f;
								colour[1] = 1.0f;
								colour[2] = 0.0f;
							}
						}
						else
						{
							// Set colour.
							colour[0] = 1.0f;
							colour[1] = 0.0f;
							colour[2] = 0.0f;
							colour[3] = 1.0f;

							if (cg_nvg.integer == 1)
							{
								colour[0] = 1.0f;
								colour[1] = 1.0f;
								colour[2] = 0.0f;
							}
						}

						cgs.clientinfo[entity->currentState.clientNum].nvgVisible = qtrue;
					}

					// Calculate 2D width, height.
					width	  = square2d[1][0] - square2d[0][0];
					height	  = square2d[3][1] - square2d[0][1];

					colour[0] = 0.75;
					colour[1] = 0.75;
					colour[2] = 0.75;
					colour[3] = 0.75;

					// If square is within scope view bounds.
					if ((square2d[0][0] > 40.0f) && (square2d[0][1] > 30.0f) && (square2d[1][0] < 620.0f) && (square2d[1][1] < 450.0f))
					{
						// Draw 2D square.
						CG_DrawRect(square2d[0][0], square2d[0][1], width, height, 1, colour);

						//top
						CG_FillRect(square2d[0][0] + (width * 0.5), 0, 1, square2d[0][1], colour);

						//bottom
						CG_FillRect(square2d[0][0] + (width * 0.5), square2d[0][1] + height, 1, 480, colour);

						//left
						CG_FillRect(0, square2d[0][1] + (height * 0.5), square2d[0][0], 1, colour);

						//right
						CG_FillRect(square2d[1][0], square2d[0][1] + (height * 0.5), 640, 1, colour);

						//right
						//	 CG_FillRect(square2d[0][0]+(width *0.5),square2d[0][1]+height ,1, 480 ,colour);

/*						CG_FillRect(square2d[0][0] - 1, square2d[0][1] - 1, (width * 0.1f) + 2, 2, colour);
						CG_FillRect(square2d[0][0] - 1, square2d[0][1] - 1, 2, (height * 0.1f) + 2, colour);

						CG_FillRect(square2d[1][0] - ((width * 0.1f) + 1), square2d[1][1] - 1, (width * 0.1f) + 2, 2, colour);
						CG_FillRect(square2d[1][0] - 1, square2d[1][1] - 1, 2, (height * 0.1f) + 2, colour);

						CG_FillRect(square2d[2][0] - ((width * 0.1f) + 1), square2d[2][1] - 1, (width * 0.1f) + 2, 2, colour);
						CG_FillRect(square2d[2][0] - 1, square2d[2][1] - ((height * 0.1f) + 1), 2, (height * 0.1f) + 2, colour);

						CG_FillRect(square2d[3][0] - 1, square2d[3][1] - 1, (width * 0.1f) + 2, 2, colour);
						CG_FillRect(square2d[3][0] - 1, square2d[3][1] - ((height * 0.1f) + 1), 2, (height * 0.1f) + 2, colour);*/
					}
				}
				else
				{
					// Clear time if needed.
					if (cg.time - cg.nvgVisibleTime[entity->currentState.number] > 10000)
					{
						// If this entity is a player.
						if (entity->currentState.eType == ET_PLAYER)
						{
							cgs.clientinfo[entity->currentState.clientNum].nvgVisible = qfalse;
						}

						cg.nvgVisibleTime[entity->currentState.number] = 0;
					}
				}
			}
			else
			{
				// If this entity is a player.
				if (entity->currentState.eType == ET_PLAYER)
				{
					cgs.clientinfo[entity->currentState.clientNum].nvgVisible = qfalse;
				}

				// Clear time.
				cg.nvgVisibleTime[entity->currentState.number] = 0;
			}
		}

		// Draw shader.
		switch (cg_nvg.integer)
		{
			default: //green
			case 0:
				colour[0] = 0;
				colour[1] = 1;
				colour[2] = 0;
				colour[3] = 1;
				break;

			case 1: // red
				colour[0] = 1;
				colour[1] = 0;
				colour[2] = 0;
				colour[3] = 1;
				break;

			case 2:
				colour[0] = 0;
				colour[1] = 0;
				colour[2] = 1;
				colour[3] = 1;
				break;

			case 3:
				colour[0] = 1;
				colour[1] = 1;
				colour[2] = 0;
				colour[3] = 1;
				break;

			case 4:
				colour[0] = 0;
				colour[1] = 1;
				colour[2] = 1;
				colour[3] = 1;
				break;

			case 5:
				colour[0] = 1;
				colour[1] = 0;
				colour[2] = 1;
				colour[3] = 1;
				break;

			case 6: //pink
				colour[0] = 1.0f;
				colour[1] = 0.3f;
				colour[2] = 0.3f;
				colour[3] = 1;
				break;

			case 7: //orange
				colour[0] = 1;
				colour[1] = 0.5f;
				colour[2] = 0;
				colour[3] = 1;
				break;
		}
		//	colour[0]*=0.85;
		//	 colour[1]*=0.85;
		// colour[2]*=0.85;
		colour[3] = 0.5;
		trap_R_SetColor( colour );

		trap_R_DrawStretchPic(cg.refdef.x, cg.refdef.y, cg.refdef.width, cg.refdef.height, 0, 0, 1, 1, cgs.media.nvgScope);
	}
}
#else
/**
 * $(function)
 */
void CG_DrawNVG ( int pass )
{
	if (pass == 0)
	{
		trap_R_DrawStretchPic( cg.refdef.x, cg.refdef.y, cg.refdef.width, cg.refdef.height, 0, 0, 1, 1, cgs.media.nvgStatic );
	}

	if (pass == 1)
	{
		trap_R_DrawStretchPic( cg.refdef.x, cg.refdef.y, cg.refdef.width, cg.refdef.height, 0, 0, 1, 1, cgs.media.nvgScope );
	}
}
#endif

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : CG_DrawSniperCrosshair
// Description: Draw sniper rifle crosshair
// Author	  : Dokta8
///////////////////////////////////////////////////////////////////////////////////////////
static void DrawSniperCrosshairNew(void)
{
	int weaponID;
	int scopeID;
	vec4_t	colourBlack = { 0, 0, 0, 1};
	float	zz;
	vec3_t	velocity;

	// Get weapon.
	weaponID = utPSGetWeaponID(&cg.snap->ps);

	// Choose scope type.
	if (weaponID == UT_WP_G36)
	{
		scopeID = cg_scopeG36.integer;
	}
	else if (weaponID == UT_WP_PSG1)
	{
		scopeID = cg_scopePSG.integer;
	}
	else if (weaponID == UT_WP_SR8)
	{
		scopeID = cg_scopeSR8.integer;
	}
	else
	{
		return;
	}

	// If scope shader is loaded.
	if (cg_weapons[weaponID].scopeShader[1])
	{
		trap_R_DrawStretchPic(cg.refdef.x, cg.refdef.y, cg.refdef.width, cg.refdef.height, 0, 0, 1, 1, cg_weapons[utPSGetWeaponID(&cg.snap->ps)].scopeShader[1]);
	}

	// If a friendly is in view.
	if ((cg.crosshairColorClientNum != -1) && (cgs.gametype >= GT_TEAM) && (cgs.gametype != GT_JUMP) && (cgs.clientinfo[cg.crosshairColorClientNum].team == cgs.clientinfo[cg.snap->ps.clientNum].team))
	{
		// Set colour.
		trap_R_SetColor(cg.scopeFriendRGB);
	}
	else
	{
		// Set colour.
		trap_R_SetColor(cg.scopeRGB);
	}

	// Clamp.
	if (scopeID < 0)
	{
		scopeID = 0;
	}

	if (scopeID > 3)
	{
		scopeID = 3;
	}

	// DOT
	if (scopeID == 3)
	{
		trap_R_DrawStretchPic((cgs.glconfig.vidWidth / 2) - (1), (cgs.glconfig.vidHeight / 2) - (1), 3, 3, 0, 0, 1, 1, cgs.media.whiteShader);
		//trap_R_DrawStretchPic(cgs.glconfig.vidWidth / 2 - (2 ), cgs.glconfig.vidHeight / 2 - (2 * cgs.screenYScale), 4, 4, 0, 0, 1, 1, cgs.media.whiteShader);
	}
	// T
	else if (scopeID == 2)
	{
		// t
		trap_R_DrawStretchPic(cgs.glconfig.vidWidth / 2, cgs.glconfig.vidHeight / 2, 1 * cgs.screenXScale, 235 * cgs.screenYScale, 0, 0, 1, 1, cgs.media.whiteShader);

		// left
		trap_R_DrawStretchPic((cgs.glconfig.vidWidth / 2) - (190 * cgs.screenXScale), (cgs.glconfig.vidHeight / 2) - (2 * cgs.screenXScale), 174 * cgs.screenXScale, 3 * cgs.screenYScale, 0, 0, 1, 1, cgs.media.whiteShader);
		trap_R_DrawStretchPic((cgs.glconfig.vidWidth / 2) - (235 * cgs.screenXScale), (cgs.glconfig.vidHeight / 2) - (2 * cgs.screenYScale), 45 * cgs.screenXScale, 6 * cgs.screenYScale, 0, 0, 1, 1, cgs.media.whiteShader);

		// right
		trap_R_DrawStretchPic((cgs.glconfig.vidWidth / 2) + (16 * cgs.screenXScale), (cgs.glconfig.vidHeight / 2) - (2 * cgs.screenXScale), 174 * cgs.screenXScale, 3 * cgs.screenYScale, 0, 0, 1, 1, cgs.media.whiteShader);
		trap_R_DrawStretchPic((cgs.glconfig.vidWidth / 2) + (190 * cgs.screenXScale), (cgs.glconfig.vidHeight / 2) - (2 * cgs.screenYScale), 46 * cgs.screenXScale, 6 * cgs.screenYScale, 0, 0, 1, 1, cgs.media.whiteShader);
	}
	// X
	else if (scopeID == 1)
	{
		// x
		trap_R_DrawStretchPic(cgs.glconfig.vidWidth / 2, (cgs.glconfig.vidHeight / 2) - (130 * cgs.screenYScale), 1, 260 * cgs.screenYScale, 0, 0, 1, 1, cgs.media.whiteShader);
		trap_R_DrawStretchPic((cgs.glconfig.vidWidth / 2) - (130 * cgs.screenXScale), cgs.glconfig.vidHeight / 2, 260 * cgs.screenXScale, 1, 0, 0, 1, 1, cgs.media.whiteShader);

		trap_R_SetColor(cg.scopeRGB);
		// top
		trap_R_DrawStretchPic((cgs.glconfig.vidWidth / 2) - (2 * cgs.screenXScale), (cgs.glconfig.vidHeight / 2) - (205 * cgs.screenYScale), 3 * cgs.screenXScale, 75 * cgs.screenYScale, 0, 0, 1, 1, cgs.media.whiteShader);
		trap_R_DrawStretchPic((cgs.glconfig.vidWidth / 2) - (5 * cgs.screenXScale), (cgs.glconfig.vidHeight / 2) - (236 * cgs.screenYScale), 6 * cgs.screenXScale, 31 * cgs.screenYScale, 0, 0, 1, 1, cgs.media.whiteShader);

		// bottom
		trap_R_DrawStretchPic(cgs.glconfig.vidWidth / 2, (cgs.glconfig.vidHeight / 2) + (130 * cgs.screenYScale), 3 * cgs.screenXScale, 75 * cgs.screenYScale, 0, 0, 1, 1, cgs.media.whiteShader);
		trap_R_DrawStretchPic(cgs.glconfig.vidWidth / 2, (cgs.glconfig.vidHeight / 2) + (205 * cgs.screenYScale), 6 * cgs.screenXScale, 30 * cgs.screenYScale, 0, 0, 1, 1, cgs.media.whiteShader);

		// left
		trap_R_DrawStretchPic((cgs.glconfig.vidWidth / 2) - (205 * cgs.screenXScale), cgs.glconfig.vidHeight / 2, 75 * cgs.screenXScale, 3 * cgs.screenYScale, 0, 0, 1, 1, cgs.media.whiteShader);
		trap_R_DrawStretchPic((cgs.glconfig.vidWidth / 2) - (235 * cgs.screenXScale), cgs.glconfig.vidHeight / 2, 30 * cgs.screenXScale, 6 * cgs.screenYScale, 0, 0, 1, 1, cgs.media.whiteShader);

		// right
		trap_R_DrawStretchPic((cgs.glconfig.vidWidth / 2) + (130 * cgs.screenXScale), (cgs.glconfig.vidHeight / 2) - (2 * cgs.screenXScale), 75 * cgs.screenXScale, 3 * cgs.screenYScale, 0, 0, 1, 1, cgs.media.whiteShader);
		trap_R_DrawStretchPic((cgs.glconfig.vidWidth / 2) + (205 * cgs.screenXScale), (cgs.glconfig.vidHeight / 2) - (5 * cgs.screenYScale), 31 * cgs.screenXScale, 6 * cgs.screenYScale, 0, 0, 1, 1, cgs.media.whiteShader);
	}
	// X - Black
	else if (scopeID == 0)
	{
		// x
		trap_R_DrawStretchPic(cgs.glconfig.vidWidth / 2, (cgs.glconfig.vidHeight / 2) - (100 * cgs.screenYScale), 1 * cgs.screenXScale, 200 * cgs.screenYScale, 0, 0, 1, 1, cgs.media.whiteShader);
		trap_R_DrawStretchPic((cgs.glconfig.vidWidth / 2) - (100 * cgs.screenXScale), cgs.glconfig.vidHeight / 2, 200 * cgs.screenXScale, 1 * cgs.screenYScale, 0, 0, 1, 1, cgs.media.whiteShader);

		// Set colour.
		trap_R_SetColor(colourBlack);

		// top
		trap_R_DrawStretchPic((cgs.glconfig.vidWidth / 2) - (4 * cgs.screenXScale), (cgs.glconfig.vidHeight / 2) - (236 * cgs.screenYScale), 5 * cgs.screenXScale, 136 * cgs.screenYScale, 0, 0, 1, 1, cgs.media.whiteShader);

		// bottom
		trap_R_DrawStretchPic(cgs.glconfig.vidWidth / 2, (cgs.glconfig.vidHeight / 2) + (100 * cgs.screenYScale), 5 * cgs.screenXScale, 135 * cgs.screenYScale, 0, 0, 1, 1, cgs.media.whiteShader);

		// left
		trap_R_DrawStretchPic((cgs.glconfig.vidWidth / 2) - (235 * cgs.screenXScale), cgs.glconfig.vidHeight / 2, 135 * cgs.screenXScale, 5 * cgs.screenYScale, 0, 0, 1, 1, cgs.media.whiteShader);

		// right
		trap_R_DrawStretchPic((cgs.glconfig.vidWidth / 2) + (100 * cgs.screenXScale), (cgs.glconfig.vidHeight / 2) - (4 * cgs.screenXScale), 136 * cgs.screenXScale, 5 * cgs.screenYScale, 0, 0, 1, 1, cgs.media.whiteShader);
	}

	zz = 0;

	if (cg_scopering.integer)
	{
		// draw movement penalty indicator
		if ((cg_scopering.integer == 2) || (weaponID == UT_WP_SR8))
		{
			if (weaponID == UT_WP_SR8)
			{
				if (cg.predictedPlayerState.groundEntityNum == ENTITYNUM_NONE)
				{
					zz = UT_MAX_SPRINTSPEED;
					zz = zz / 10;
					trap_R_DrawStretchPic((cgs.glconfig.vidWidth / 2) - (zz), (cgs.glconfig.vidHeight / 2) - (zz), zz * 2 + 1, zz * 2 + 1, 0, 0, 1, 1, cgs.media.ringShader);
					zz = UT_MAX_SPRINTSPEED;
				}
			}

			if (zz == 0)
			{
				VectorCopy ( cg.predictedPlayerState.velocity, velocity );

				VectorNormalize(velocity,zz);
				zz = zz > UT_MAX_SPRINTSPEED ? UT_MAX_SPRINTSPEED : zz;
			}
			zz = zz / 5;
			trap_R_DrawStretchPic((cgs.glconfig.vidWidth / 2) - (zz), (cgs.glconfig.vidHeight / 2) - (zz), zz * 2 + 1, zz * 2 + 1, 0, 0, 1, 1, cgs.media.ringShader);
		}
	}

	// Clear colour.
	trap_R_SetColor(NULL);
}

/**
 * $(function)
 */
#if 0
static void DrawSniperCrosshairOld(void)
{
	if (cg_weapons[utPSGetWeaponID ( &cg.snap->ps )].scopeShader[0])
	{
		trap_R_DrawStretchPic(	cg.refdef.x, cg.refdef.y, cg.refdef.width, cg.refdef.height, 0, 0, 1, 1, cg_weapons[utPSGetWeaponID ( &cg.snap->ps )].scopeShader[0] );
	}

	trap_R_SetColor( cg.scopeRGB );

	if ((cg.crosshairColorClientNum != -1) && (cgs.gametype >= GT_TEAM) && (cgs.gametype != GT_JUMP)) //@barba
	{
		if (cgs.clientinfo[cg.crosshairColorClientNum].team == cgs.clientinfo[cg.snap->ps.clientNum].team)	  // was cg.clientNum
		{
			trap_R_SetColor( cg.scopeFriendRGB );
		}
	}

	trap_R_DrawStretchPic ( cgs.glconfig.vidWidth / 2, cgs.glconfig.vidHeight / 2 - 100 * cgs.screenYScale, 1 * cgs.screenXScale, 200 * cgs.screenYScale, 0, 0, 1, 1, cgs.media.whiteShader );
	trap_R_DrawStretchPic ( cgs.glconfig.vidWidth / 2 - 100 * cgs.screenXScale, cgs.glconfig.vidHeight / 2, 200 * cgs.screenXScale, 1 * cgs.screenYScale, 0, 0, 0, 1, cgs.media.whiteShader );

	trap_R_SetColor( NULL );
}
#endif

/**
 * $(function)
 */
void CG_DrawSniperCrosshair(void)
{
	DrawSniperCrosshairNew();

	/*
	// If we should draw the new scopes.
	if (cg_scopeType.integer)
	{
		DrawSniperCrosshairNew();
	}
	else
	{
		DrawSniperCrosshairOld();
	}
	*/
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : CG_SetShaderOverlay
// Description: Sets the shader overlay
// Author	  : Dokta8 / Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
void CG_SetShaderOverlay ( qhandle_t shaderHandle, int duration )
{
	cg.overlayShader	 = shaderHandle;
	cg.overlayShaderTime	 = cg.time;
	cg.overlayShaderDuration = duration;
}

/**
 * $(function)
 */
void CG_DrawSmokeOverlay(void)
{
	float	nudge, nudgex;
//	char		var[MAX_TOKEN_CHARS];
	vec3_t	ambientLight, dirLight, Lightdir;
	vec4_t	finalcolor;

	if (cg.grensmokecolor[3] > 0.1)
	{
		if ((!cg_thirdPerson.integer && (cg.snap->ps.stats[STAT_HEALTH] > 0)) && (utPSIsItemOn ( &cg.snap->ps, UT_ITEM_NVG )))
		{
			if (cg.grensmokecolor[3] > 0.5f)
			{
				cg.grensmokecolor[3] = 0.5f;			  //make nvg immune to a bit of the overlay opacity
			}
		}

		if (cg.grensmokecolor[3] > 0.95f)
		{
			cg.grensmokecolor[3] = 0.95f;
		}

		trap_R_LightForPoint(cg.refdef.vieworg, ambientLight, dirLight, Lightdir );

		finalcolor[0] = (dirLight[0] / 255) * cg.grensmokecolor[0];
		finalcolor[1] = (dirLight[1] / 255) * cg.grensmokecolor[1];
		finalcolor[2] = (dirLight[2] / 255) * cg.grensmokecolor[2];
		finalcolor[3] = cg.grensmokecolor[3];

		trap_R_SetColor( finalcolor );

		nudge  = (cg.refdefViewAngles[0]) * 0.004;
		nudgex = (3.14 + cg.refdefViewAngles[1]) * 0.004;

		// see if we are running vertex, because it makes this effect too transparent
		//trap_Cvar_VariableStringBuffer( "r_vertexlight", var, sizeof( var ) );
		//if (var[0]=='1')
		trap_R_DrawStretchPic(	cg.refdef.x, cg.refdef.y, cg.refdef.width, cg.refdef.height, nudgex, 0.0f + nudge, 1.0f + nudgex, 1 + nudge, cgs.media.viewsmokeshader );
		trap_R_DrawStretchPic(	cg.refdef.x, cg.refdef.y, cg.refdef.width, cg.refdef.height, nudgex, 1 - nudge, 1.0f + nudgex, nudge, cgs.media.viewsmokeshader );

		trap_R_SetColor( NULL );
	}

	if (cg.grensmokecolor[3] > 0)
	{
		cg.grensmokecolor[3] -= 0.01f;			 //Fixme: #27 - this is FPS dependant ATM..  should not be
	}

	if (cg.grensmokecolor[3] < 0)
	{
		cg.grensmokecolor[3] = 0.0f;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : CG_DrawUTViewOverlay
// Description: Draws shader overlays such as NVGs
// Author	  : Dokta8 / Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
void CG_DrawShaderOverlay (void)
{
	// See if there is nothing to do first
	if (!cg.overlayShaderTime)
	{
		return;
	}

	// See if we are all done now
	if ((cg.time - cg.overlayShaderTime) > cg.overlayShaderDuration)
	{
		cg.overlayShaderTime = 0;
		return;
	}

	// Draw the shader
	trap_R_DrawStretchPic( 0, 0, cgs.glconfig.vidWidth, cgs.glconfig.vidHeight, 0, 0, 1, 1, cg.overlayShader );
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : CG_SetColorOverlay
// Description: Draws color overlays such as death and flash
// Author	  : Dokta8 / Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
void CG_SetColorOverlay ( float r, float g, float b, float a, int duration, int fade, int fadeTime )
{
	cg.overlayColor[0]	= r;
	cg.overlayColor[1]	= g;
	cg.overlayColor[2]	= b;
	cg.overlayColor[3]	= a;
	cg.overlayColorFade = fade;
	cg.overlayColorFadeTime = fadeTime;
	cg.overlayColorTime = cg.time;
	cg.overlayColorDuration = duration;
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : CG_DrawColorOverlay
// Description: Draws colors overlays such as death and flash
// Author	  : Dokta8 / Apoxol
// Changes/Bug Fixes   : DensitY - this is somewhat a mess :p
///////////////////////////////////////////////////////////////////////////////////////////

void CG_DrawColorOverlay( void )
{
	vec4_t	color;

	// If we're paused.
	if (cg.pauseState & UT_PAUSE_ON)
	{
		cg.overlayColorTime += cg.frametime;
	}

//	if( cg.snap->ps.pm_flags & PMF_FOLLOW /*&& cg.snap->ps.clientNum != cg.clientNum */) {
//		cg.overlayFade = 0.0f;
//		return;
//	}
	// See if we are all done now
	if((cg.time - cg.overlayColorTime) > cg.overlayColorDuration)
	{
		cg.overlayColorTime = 0;
		//if( cg.predictedPlayerState.stats[ STAT_UTPMOVE_FLAGS ] & UT_PMF_GHOST ) {
		//	cg.overlayFade = 0.0f;
		//}
		//else
		//	  {
//			if( cg.overlayColorFade == UT_OCFADE_LINEAR_OUT ) {
//				cg.overlayFade = 1.0f;
//			}
//			else if( cg.overlayColorFade == UT_OCFADE_LINEAR_IN || cg.overlayColorFade == UT_OCFADE_SIN_OUT ) {
//				cg.overlayFade = 0.0f;
//			}
		//	}
		// IF we are still fading then do so
	} //else
	else if((cg.time - cg.overlayColorTime) < cg.overlayColorFadeTime)
	{
		// Calculate the fade amount
		cg.overlayFade = (float)(cg.time - cg.overlayColorTime) / (float)cg.overlayColorFadeTime;

		// Clamp to 0
		if(cg.overlayFade < 0.1f)
		{
			cg.overlayFade = 0.0f;
		}

		if (cg.overlayFade >= 1.0f)
		{
			cg.overlayFade = 0.99f;
		}

		// Now determine how we are fading
		switch(cg.overlayColorFade)
		{
			case UT_OCFADE_LINEAR_IN:
				cg.overlayFade = 1.0 - cg.overlayFade;
				break;

			case UT_OCFADE_LINEAR_OUT:
				//cg.overlayFade = cg.overlayFade; //@Fenix - self assignment
				break;

			case UT_OCFADE_SIN_OUT:
				cg.overlayFade	= 1.0 - cg.overlayFade;
				cg.overlayFade *= 1.5;

				if(cg.overlayFade < 0.01f)
				{
					cg.overlayFade = 0.0f;
				}

				if(cg.overlayFade > 1.0f)
				{
					cg.overlayFade = 1.0f;
				}
				//; sin ( DEG2RAD(90) + ( overlayFade * DEG2RAD(90) ) );
				break;

			default:
				//error
				return;

				break;
		}
	}

	// Draw the overlay now
	if((cg.overlayFade > 0.1f) && (cg.overlayFade <= 1.0f))   // Don't draw outside these bounds
	{
		color[0] = cg.overlayColor[0];						// the > 0.1 test is so we don't draw if its completely transparent
		color[1] = cg.overlayColor[1];
		color[2] = cg.overlayColor[2];
		color[3] = cg.overlayFade;
		CG_FillRect( 0, 0, 640, 480, color );
	}
}

/*
=================
CG_DrawFlasGrenadeFade
=================
*/
void CG_DrawFlashGrenadeFade( void )
{
	int blind, sound;
	vec4_t col;

	if (!cg.snap)
	{
		return;
	}

	blind = cg.snap->ps.stats[STAT_BLIND] & 0x0000FFFF;
	sound = (cg.snap->ps.stats[STAT_BLIND] & 0xFFFF0000) >> 16;

	if( blind )
	{
		if( utPSIsItemOn ( &cg.snap->ps, UT_ITEM_NVG ) )		// nvgs
		{
			col[0] = col[1] = col[2] = 1.0f;
		}
		else
		{
			col[0] = col[1] = col[2] = 0.8f;
		}
		col[3] = ( (blind > 2000) ? 1.0f : (float)blind/2000.0f );

		CG_FillRect( 0, 0, 640, 480, col );
	}

//	if( sound )
//	{
//		col[3] = ( (sound > 2000) ? 1.0f : (float)sound/2000.0f );
//
//		trap_S_AddLoopingSound( cg.snap->ps.origin, vec3_origin, cgs.media.flashBangDroneSnd, (int)(255.0f*2.0f*col[3]), 0 );
//	}
}

/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
//
// SPACE INVADERS!!
//
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////

void inv_doround( void )
{
	int i, j, x, y, bx, by, lx, ly;
	int points;
//	qboolean move;
	vec4_t	white		= { 1.0f, 1.0f, 1.0f, 1.0f };
	vec4_t	green		= { 0.0f, 1.0f, 0.0f, 1.0f };

	char	gameover[]	= "Game Over!";
	char	pressfire[] = "(press fire to play)";
	float	scale		= 0.8f;
	int w		= 0;
	int h		= 0;

	inv_uidraw();

	if (cg.invadergame.playing)
	{
		// the pausetimer halts the game, but allows it to be drawn still
		if (cg.invadergame.pausetimer < cg.time)
		{
			// move invaders
			inv_invaderBounds();	// set min and max

			// move invaders only every "speed" milliseconds
			if (cg.time >= cg.invadergame.lastinvmove + cg.invadergame.speed)
			{
				cg.invadergame.lastinvmove = cg.time;

				cg.invadergame.icol   += (cg.invadergame.direction * INV_HSTEP); // direction can change
				inv_xyforrc( cg.invadergame.maxy, cg.invadergame.maxx, &bx, &by );
				inv_xyforrc( cg.invadergame.miny, cg.invadergame.minx, &lx, &ly );

				if ((bx > INV_MAXX) || (lx < INV_STARTX))
				{
					cg.invadergame.irow  += INV_VSTEP;					// move down a row if at either end
					cg.invadergame.direction *= -1; 					// reverse dir
					cg.invadergame.icol  += (cg.invadergame.direction * INV_HSTEP); // cancel out xmove

					if (by >= INV_MAXY)
					{
						// invaded!!
						CG_Printf("All your bases are belong to us\n");
						cg.invadergame.playing = qfalse;
						inv_hiscore();
						return;
					}
				}

				// see if invader should shoot
				inv_ai();
			}

			// move invadershots
			for (i = 0; i < INV_MAXSHOTS; i++)
			{
				if (cg.invadergame.invadershoty[i] < INV_LINE)
				{
					cg.invadergame.invadershoty[i]++;
				}
				else
				{
					cg.invadergame.numActiveInvaderShots--;
				}
			}

			// move player
			cg.invadergame.playerx += (cg.invadergame.playerdx * 2);

			if (cg.invadergame.playerx <= INV_STARTX)
			{
				cg.invadergame.playerx = INV_STARTX;
			}

			if (cg.invadergame.playerx >= INV_PENDX - INV_PWIDTH)
			{
				cg.invadergame.playerx = INV_PENDX - INV_PWIDTH;
			}

			// check for player fire
			if (cg.invadergame.playershoot)
			{
				cg.invadergame.playershoot = 0;

				if (cg.invadergame.shoty <= 0)
				{
					cg.invadergame.shotx = cg.invadergame.playerx + (INV_PWIDTH / 2);
					cg.invadergame.shoty = cg.invadergame.playery;
				}
			}

			// move playershots
			if (cg.invadergame.shoty > 0)
			{
				cg.invadergame.shoty -= 3;
			}

			// do collision detection

			// player shoot barrier?
			if (inv_checkbarrier(cg.invadergame.shotx, cg.invadergame.shoty, qtrue))
			{
				cg.invadergame.shoty = -1;
			}

			// invader shoot barrier?
			for (i = 0; i < INV_MAXSHOTS; i++)
			{
				if (inv_checkbarrier(cg.invadergame.invadershotx[i], cg.invadergame.invadershoty[i], qfalse))
				{
					cg.invadergame.invadershoty[i] = 1000;
				}
			}

			// player shoot invader?
			if ((points = inv_shotCollideInvader( cg.invadergame.shotx, cg.invadergame.shoty )))
			{
				// play destroy sound
				cg.invadergame.score += ((INV_INVROWS - points) + 1) * 5;

				if (cg.invadergame.numInvaders <= 0)
				{
					// player killed all the invaders; start next wave
					cg.invadergame.wave++;

					if (cg.invadergame.wave > 20)
					{
						cg.invadergame.wave = 20;
					}
					cg.invadergame.direction = 1;
					cg.invadergame.icol  = 0;
					cg.invadergame.irow  = 0;

					for (i = 0; i < INV_INVCOLS; i++)
					{
						for (j = 0; j < INV_INVROWS; j++)
						{
							cg.invadergame.invaders[i][j] = 1;
						}
					}

					for (i = 0; i < INV_MAXSHOTS; i++)
					{
						cg.invadergame.invadershoty[i] = 1000;
					}

					cg.invadergame.playerx			 = (INV_STARTX + (INV_PENDX - INV_STARTX) / 2) - (INV_PWIDTH / 2);
					cg.invadergame.playery			 = INV_PSHIPY;

					cg.invadergame.lastinvmove		 = 0;
					cg.invadergame.shoty			 = -1;
					cg.invadergame.numActiveInvaderShots = 0;
					cg.invadergame.numInvaders		 = INV_INVCOLS * INV_INVROWS;
					cg.invadergame.speed			 = 1000 - (25 * cg.invadergame.wave);

					// reset barriers
					for (i = 0; i < 4; i++)
					{
						for (j = 0; j < INV_BWIDTH; j++)
						{
							cg.invadergame.barriers[j][i][0] = 0;
							cg.invadergame.barriers[j][i][1] = 0;
						}
					}

					cg.invadergame.pausetimer = cg.time + 4000;
					return;
				}
				cg.invadergame.shoty  = -1; // let player shoot again
				cg.invadergame.speed  = 1000 - (25 * cg.invadergame.wave);
				cg.invadergame.speed -= ((INV_INVCOLS * INV_INVROWS) - cg.invadergame.numInvaders) / 4; // speed up invaders as their number gets fewer

				if (cg.invadergame.speed < 50)
				{
					cg.invadergame.speed = 50;
				}
			}

			// invader shoot player?
			for (i = 0; i < INV_MAXSHOTS; i++)
			{
				if (inv_shotCollidePlayer ( cg.invadergame.invadershotx[i], cg.invadergame.invadershoty[i] ))
				{
					// player is hit
					cg.invadergame.invadershoty[i] = 1000;	// cause the shot to be removed
					cg.invadergame.pausetimer	   = cg.time + 3000;
					cg.invadergame.playerlives--;

					// move player back to centre
					cg.invadergame.playerx = (INV_STARTX + (INV_PENDX - INV_STARTX) / 2) - (INV_PWIDTH / 2);
					cg.invadergame.playery = INV_PSHIPY;

					if (cg.invadergame.playerlives <= 0)
					{
						cg.invadergame.playing = qfalse;
						inv_hiscore();
						return;
					}
				}
			}
		}	// end pause timer

		// draw stuff

		// draw shots
		for (i = 0; i < INV_MAXSHOTS; i++)
		{
			if (cg.invadergame.invadershoty[i] < INV_LINE)
			{
				CG_FillRect((float)cg.invadergame.invadershotx[i] - 1, (float)cg.invadergame.invadershoty[i] - 10, (float)3, (float)10, white);
			}
		}

		if (cg.invadergame.shoty > INV_STARTY)
		{
			CG_FillRect((float)cg.invadergame.shotx - 1, (float)cg.invadergame.shoty + 10, (float)3, (float)10, white);
		}

		// draw invaders
		// find location of top left invader even if it's dead
		inv_xyforrc( 0, 0, &x, &y );

		for (i = 0; i < INV_INVCOLS; i++)
		{
			for (j = 0; j < INV_INVROWS; j++)
			{
				if (cg.invadergame.invaders[i][j])
				{
					CG_FillRect((float)x + ((INV_WIDTH + INV_XSPACING) * i), (float)y + ((INV_HEIGHT + INV_YSPACING) * j), (float)INV_WIDTH, (float)INV_HEIGHT, white);
				}
			}
		}

		// draw player
		CG_FillRect((float)cg.invadergame.playerx, (float)cg.invadergame.playery, (float)INV_PWIDTH, (float)INV_PHEIGHT, green);

		// draw bases
		for(i = 0; i < 4; i++)
		{
			inv_drawbase( i );
		}
	}
	else
	{
		// initialise game and draw game over
		w = CG_Text_Width(gameover, scale, 0);
		CG_Text_Paint(320 - w / 2, 60, scale, white, gameover, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);

		w = CG_Text_Width(pressfire, scale * 0.55f, 0);
		CG_Text_Paint(320 - w / 2, 100, scale * 0.55f, white, pressfire, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);

		h = CG_Text_Height( "0", scale * 0.3f, 0);

		for (i = 0; i < 10; i++)
		{
			w = CG_Text_Width(cg.invadergame.hinames[i], scale * 0.3, 0);
			CG_Text_Paint(INV_STARTX, 150 + (i * (h + 4)), scale * 0.3f, white, cg.invadergame.hinames[i], 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);
			w = CG_Text_Width("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX", scale * 0.3, 0);
			CG_Text_Paint(INV_STARTX + w, 150 + (i * (h + 4)), scale * 0.3f, white, va("%d", cg.invadergame.hiscores[i]), 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);
		}

		if (cg.invadergame.playershoot)
		{
			inv_initgame();
		}
	}
}

/**
 * $(function)
 */
void inv_hiscore( void )
{
	int 	  i, j;
	clientInfo_t  *ci;

	ci = &cgs.clientinfo[cg.clientNum];

	if (cg.invadergame.score < cg.invadergame.hiscores[9])
	{
		return;
	}

	// do a nasty sort: CHECK THIS!!!!!!
	for (i = 0; i < 10; i++)
	{
		if (cg.invadergame.score > cg.invadergame.hiscores[i])
		{
			for (j = i; j < 9; j++)
			{
				cg.invadergame.hiscores[j + 1] = cg.invadergame.hiscores[j];
				Q_strncpyz( cg.invadergame.hinames[j + 1], cg.invadergame.hinames[j], 32 );
			}
			cg.invadergame.hiscores[i] = cg.invadergame.score;
			Q_strncpyz( cg.invadergame.hinames[i], ci->name, 32 );
			break;
		}
	}
}

/**
 * $(function)
 */
void inv_drawbase( int base )
{
	vec4_t	green = { 0.0f, 1.0f, 0.0f, 1.0f };
	int i;

	// this is awful but it's the only way to do shootable barriers
	for (i = 0; i < INV_BWIDTH; i++)
	{
		if (cg.invadergame.barriers[i][base][0] + cg.invadergame.barriers[i][base][1] >= INV_BHEIGHT)
		{
			continue;
		}
		CG_FillRect ( INV_STARTX + INV_BLEFTOFF + ((INV_BWIDTH + INV_BSPACING) * base) + i, INV_BASEY + cg.invadergame.barriers[i][base][0], 1, INV_BHEIGHT - cg.invadergame.barriers[i][base][1], green );
	}
}

/**
 * $(function)
 */
qboolean inv_checkbarrier( int sx, int sy, qboolean isplayer )
{
	int  topx, i;
	int  side = 0;

	if (isplayer)
	{
		side = 1;
	}

	if ((sy < (INV_BASEY + INV_BHEIGHT)) || (sy > INV_BASEY))
	{
		return qfalse;
	}

	for (i = 0; i < 4; i++)
	{
		topx = INV_STARTX + INV_BLEFTOFF + ((INV_BWIDTH + INV_BSPACING) * i);

		if ((sx >= topx) && (sx < topx + INV_BWIDTH))
		{
			if((cg.invadergame.barriers[sx - topx][i][0] + cg.invadergame.barriers[sx - topx][i][1]) >= INV_BHEIGHT)
			{
				return qfalse;
			}
			cg.invadergame.barriers[sx - topx][i][side]++;
			return qtrue;
		}
	}

	return qfalse;
}

/**
 * $(function)
 */
void inv_uidraw( void )
{
	vec4_t	white = { 1.0f, 1.0f, 1.0f, 1.0f };
	vec4_t	green = { 0.0f, 1.0f, 0.0f, 1.0f };
	vec4_t	dim   = { 0.0f, 0.0f, 0.0f, 0.5f };

	float	w, h;
	float	scale = 0.3f;

	CG_FillRect ( 0, 0, 640, 480, dim );

	// draw scores
	w = CG_Text_Width("SCORE <1>", scale, 0);
	h = CG_Text_Height("SCORE <1>", scale, 0);
	CG_Text_Paint(INV_STARTX + 16, INV_SCORE1Y + h, scale, white, "SCORE <1>", 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);
	CG_Text_Paint(INV_STARTX + 16, INV_SCORE1Y + h + h + 10, scale, white, va("%d", cg.invadergame.score), 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);

	w = w + 20;
	CG_Text_Paint(INV_STARTX + w, INV_SCORE1Y + h, scale, white, "HI-SCORE", 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);
	CG_Text_Paint(INV_STARTX + w, INV_SCORE1Y + h + h + 10, scale, white, "  20000", 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);

	w = CG_Text_Width("SCORE <2>", scale, 0);
	CG_Text_Paint(INV_PENDX - w - 16, INV_SCORE1Y + h, scale, white, "SCORE <2>", 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);
	w = CG_Text_Width("0000", scale, 0);
	CG_Text_Paint(INV_PENDX - w - 16, INV_SCORE1Y + h + h + 10, scale, white, "0000", 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);

	// draw bottom stuff
	CG_FillRect ( INV_STARTX, INV_LINE, INV_PENDX - INV_STARTX, 1, green );

	w = CG_Text_Width(va("%d", cg.invadergame.playerlives ), scale, 0);
	CG_Text_Paint(INV_STARTX + 16, INV_LINE + 2 + h, scale, white, va("%d", cg.invadergame.playerlives ), 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);

	w = CG_Text_Width("CREDIT 01", scale, 0);
	CG_Text_Paint(INV_PENDX - w - 32, INV_LINE + 2 + h, scale, white, "CREDIT 01", 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);
}

/**
 * $(function)
 */
void inv_initgame( void )
{
	int  i, j;

	cg.invadergame.playing	   = qtrue;
	cg.invadergame.pausetimer  = 0;
	cg.invadergame.score	   = 0;
	cg.invadergame.playerlives = 3;
	cg.invadergame.speed	   = 1000; // smaller is faster
	cg.invadergame.direction   = 1;
	cg.invadergame.icol    = 0;
	cg.invadergame.irow    = 0;
	cg.invadergame.wave    = 0;

	for (i = 0; i < INV_INVCOLS; i++)
	{
		for (j = 0; j < INV_INVROWS; j++)
		{
			cg.invadergame.invaders[i][j] = 1;
		}
	}

	for (i = 0; i < INV_MAXSHOTS; i++)
	{
		cg.invadergame.invadershoty[i] = 1000;
	}

	cg.invadergame.playerx			 = (INV_STARTX + (INV_PENDX - INV_STARTX) / 2) - (INV_PWIDTH / 2);
	cg.invadergame.playery			 = INV_PSHIPY;

	cg.invadergame.lastinvmove		 = 0;
	cg.invadergame.shoty			 = -1;
	cg.invadergame.numActiveInvaderShots = 0;
	cg.invadergame.numInvaders		 = INV_INVCOLS * INV_INVROWS;

	// reset barriers
	for (i = 0; i < 3; i++)
	{
		for (j = 0; j < INV_BWIDTH; j++)
		{
			cg.invadergame.barriers[j][i][0] = 0;
			cg.invadergame.barriers[j][i][1] = 0;
		}
	}
}

// Space Invaders Utility Functions

// return the row and column of a given screen coordinate
// can return 0 and negatives
void inv_rcforxy ( const int x, const int y, int *row, int *col )
{
	*col = (x - INV_STARTX - cg.invadergame.icol) / (INV_WIDTH + INV_XSPACING);
	*row = (y - INV_STARTY - cg.invadergame.irow) / (INV_HEIGHT + INV_YSPACING);
}

// return the screen coordinates of a given row and column
void inv_xyforrc ( const int row, const int col, int *x, int *y )
{
	*x = INV_STARTX + cg.invadergame.icol + (col * (INV_WIDTH + INV_XSPACING));
	*y = INV_STARTY + cg.invadergame.irow + (row * (INV_HEIGHT + INV_YSPACING));
}

/**
 * $(function)
 */
int inv_shotCollideInvader( int shotx, int shoty )
{
	int  col, row;

	inv_rcforxy( shotx, shoty, &row, &col );

	if ((col >= 0) && (col < INV_INVCOLS) && (row >= 0) && (row < INV_INVROWS))
	{
		if (cg.invadergame.invaders[col][row])
		{
			cg.invadergame.invaders[col][row] = 0;
			cg.invadergame.numInvaders--;
			return row;
		}
	}

	return 0;
}

/**
 * $(function)
 */
qboolean inv_shotCollidePlayer( int shotx, int shoty )
{
	if (((shotx > cg.invadergame.playerx) &&
		 (shotx < cg.invadergame.playerx + INV_PWIDTH)) &&
		((shoty >= cg.invadergame.playery) && (shoty <= cg.invadergame.playery + INV_PHEIGHT)))
	{
		return qtrue;
	}

	return qfalse;
}

// work out the dimension of remaining invader grid
// min and max values are rows and columns, not screen coords
void inv_invaderBounds( void )
{
	int  i, j;
	int  minx = INV_INVCOLS;
	int  maxx = 0;
	int  miny = INV_INVROWS;
	int  maxy = 0;

	for (i = 0; i < INV_INVCOLS; i++)
	{
		for (j = 0; j < INV_INVROWS; j++)
		{
			if ((cg.invadergame.invaders[i][j]) && (i < minx))
			{
				minx = i;
			}

			if ((cg.invadergame.invaders[i][j]) && (i > maxx))
			{
				maxx = i;
			}

			if ((cg.invadergame.invaders[i][j]) && (j < miny))
			{
				miny = j;
			}

			if ((cg.invadergame.invaders[i][j]) && (j > maxy))
			{
				maxy = j;
			}
		}
	}

	cg.invadergame.minx = minx;
	cg.invadergame.maxx = maxx;
	cg.invadergame.miny = miny;
	cg.invadergame.maxy = maxy;
}

// l33t space invader AI firing routine... look our MrElusive, your butt is MINE
void inv_ai( void )
{
	int  i, j = 0, firingx, firingy, ref;
	int  x, y;
	int  test = -1;

	// do invader shooting
	if (cg.invadergame.numActiveInvaderShots < INV_MAXSHOTS)
	{
		// find an invader to shoot
		ref = rand() % cg.invadergame.numInvaders;

		// loop thru all invaders and find the row and column of the ref'th one that's alive
		for (i = 0; i < INV_INVCOLS; i++)
		{
			for (j = 0; j < INV_INVROWS; j++)
			{
				if (cg.invadergame.invaders[i][j])
				{
					test++;
				}

				if (test == ref)
				{
					break;		// is he the One?
				}
			}

			if (test == ref)
			{
				break;
			}
		}

		firingx = i;
		firingy = j;

		// find a free shot: free shots have gone off the screen

		for (i = 0; i < 5; i++)
		{
			if (i == 4)
			{
				return;   // no free shots: should never happen
			}

			if (cg.invadergame.invadershoty[i] >= INV_LINE)
			{
				break;
			}
		}

		inv_xyforrc( firingy, firingx, &x, &y );

		cg.invadergame.invadershotx[i] = x + (INV_WIDTH / 2);
		cg.invadergame.invadershoty[i] = y + (INV_HEIGHT / 2);

		cg.invadergame.numActiveInvaderShots++;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

/*=---- draw who won and if they get owned ----=*/

static qboolean CG_DrawSurvivorRoundWinner( void )
{
//	static char *Owned1 = "Flawless";
//	static char *Owned2 = "CONED!";
	char  *start;
	char  *end;
	int   y;

	if (!cg.survivorRoundWinner[0])
	{
		return qfalse;
	}
	y	  = 200;
	start = cg.survivorRoundWinner;

	while(start && *start)
	{
		end = strchr ( start, '\r' );

		if(end)
		{
			*end = '\0';
		}
		CG_Text_Paint( 320 - CG_Text_Width(start, 0.4f, 0) / 2, y,
				   0.4f, colorWhite, start,
				   0, 0, ITEM_TEXTSTYLE_OUTLINED);

		if(end)
		{
			*end  = '\r';
			start = end + 1;
		}
		else
		{
			start = NULL;
		}

		y += 20;
	}
	y += 20;

	if(cg.OwnedType == 1)
	{
		if(cg.time >= cg.OwnedSoundTimeStart)
		{
			CG_Text_Paint( 320 - CG_Text_Width( "Flawless", 0.8f, 0) / 2, y, 0.8f,
					   colorWhite, "Flawless", 0, 0, ITEM_TEXTSTYLE_OUTLINED);
		}
	}
	else if(cg.OwnedType == 2)	   // This is VERY Rare
	{
		if(cg.time >= cg.OwnedSoundTimeStart)
		{
			CG_Text_Paint( 320 - CG_Text_Width( "Coned!", 0.8f, 0) / 2, y, 0.8f,
					   colorWhite, "Coned!", 0, 0, ITEM_TEXTSTYLE_OUTLINED);
		}
	}
	return qtrue;
}

/**
 * $(function)
 */
static qboolean CG_DrawSPGameTimers(void)
{
	// If defusing draw a status
	if(cg.sniperGameRoundTime)
	{
		if(cg.time > cg.sniperGameRoundTime)
		{
			cg.sniperGameRoundTime = 0;
		}
		else
		{
			char  out[50];
			Com_sprintf ( out, sizeof(out), "Time Left: %i", (cg.sniperGameRoundTime - cg.time) / 1000 + 1 );
			CG_Text_Paint( 320 - CG_Text_Width(out, 0.3f, 0) / 2, 420, 0.3f, colorWhite, out, 0, 0, ITEM_TEXTSTYLE_SHADOWED);
			trap_R_SetColor( NULL );
		}
	}
	else if(cg.raceStartTime)
	{
		char  out[50];
		int   mins;
		int   secs;
		int   msecs;
		int   tens;
		char  *s;

		msecs = cg.time - cg.raceStartTime;

		tens  = msecs / 100;
		tens  = tens % 10;
		secs  = msecs / 1000;
		mins  = secs / 60;
		secs  = secs - (mins * 60);
		s	  = va("%i.%i", secs, tens);

		if(mins > 0)
		{
			s = va("%i:%s", mins, s);
		}
//			length = CG_Text_Width(s, 1, 0);
//			CG_Printf("length is %d\n", length);
		Com_sprintf ( out, sizeof(out), "%s", s );
		CG_Text_Paint( 320 - CG_Text_Width(out, 0.3f, 0) / 4, 420, 0.3f, colorWhite, out, 0, 0, ITEM_TEXTSTYLE_SHADOWED);
		trap_R_SetColor( NULL );
	}

	return qtrue;
}

/**
 * $(function)
 */
void DrawClientOverlay(int i, int x, int y, int width, int ourTeam) {

	qboolean		dead;
	int 			team, kills = 0, deaths = 0, j;
	/*
	static vec4_t	colorBlue2		= {   0.0f,   0.0f,   0.5f,   0.75f };
	static vec4_t	colorRed2		= {    0.5,   0.0f,   0.0f,   0.75f };
	static vec4_t	colorYellow2	= {   0.4f,   0.4f,   0.2f,   0.75f };
	static vec4_t	colorGrey		= {   0.5f,   0.5f,   0.5f,   1.0f	};
	static vec4_t	colorJumpMode	= {   0.42,   0.22,   0.53,   1.0	}; //@Barbatos - purple for the jump mode
	static vec4_t	colorJumpMode2	= {   0.42,   0.22,   0.53,   0.75f	};
	static vec4_t	colorGreen	= {   0.0f,  0.37f,  0.09f,   1.0f	}; //@Fenix - new color for FFA and LMS
	static vec4_t	colorGreen2	= {   0.0f,  0.37f,  0.09f,   0.75f	};
	*/

	vec4_t			*colorTeam;
	vec4_t			col, finalcolor;
	char			*s;
	utSkins_t               sidx;

	team = cgs.clientinfo[i].team;

	//@Fenix - fix bug #782
	dead = cgs.clientinfo[i].ghost;
	//if(cg_entities[i].currentState.eFlags & EF_DEAD) {
	//	dead = qtrue;
	//} else
	//	dead = qfalse;
	//}

	col[0] = cgs.clientinfo[i].armbandRGB[0] / 255;
	col[1] = cgs.clientinfo[i].armbandRGB[1] / 255;
	col[2] = cgs.clientinfo[i].armbandRGB[2] / 255;
	col[3] = 1.f;

	if (cg.g_armbands==1) {
		sidx = CG_WhichSkin(team);
		col[0] = skinInfos[sidx].defaultArmBandColor[0] / 255;
		col[1] = skinInfos[sidx].defaultArmBandColor[1] / 255;
		col[2] = skinInfos[sidx].defaultArmBandColor[2] / 255;
	}

	//@Fenix - force removed on BK request
	//@holblin // @Barbatos: don't force the color if we're playing JUMP mode
	/*if ((cg.predictedPlayerState.persistant[PERS_TEAM] == TEAM_FREE) && (cgs.gametype != GT_JUMP)) {
		//@Fenix - changed color from YELLOW to GREEN for FFS and LMS only
		if (team == TEAM_FREE) {
			col[0] = 0;
			col[1] = 1;
			col[2] = 0;
			col[3] = 1;
		}
	}
	*/

	colorTeam = &skinInfos[CG_WhichSkin(team)].ui_teamoverlay_fill_color;
	CG_FillRect ( x, y, width, 11, *colorTeam );

	{
		for (j = 0; j < cg.numScores; j++) {
			if (cg.scores[j].client == i) {
				kills  = cg.scores[j].score;
				deaths = cg.scores[j].deaths;
			}
		}

		//@Fenix - do not draw scores in jump mode
		if ((cg_drawTeamOverlayScores.integer) && (cgs.gametype != GT_JUMP)) {
			s = va("%i:%i %s", kills, deaths, cgs.clientinfo[i].name);
		} else {
			s = va("%s", cgs.clientinfo[i].name);
		}

		//@Barbatos: "need medic" highlight on the scoreboard. Only team mates can see it
		if(!dead && ((CG_ENTITIES(i)->currentState.eFlags & EF_UT_MEDIC) && team != TEAM_FREE && team == ourTeam)) {
			finalcolor[0] = colorYellow[0];
			finalcolor[1] = colorYellow[1];
			finalcolor[2] = colorYellow[2];
			finalcolor[3] = colorYellow[3];
			finalcolor[3] = 0.5f + (cos((float)cg.time / 100.0f) / 2);
			CG_Text_Paint ( x + 12, y + 9, 0.18f, finalcolor, s, 0, 0, 0 ); // dead?colorGrey:colorWhite
		}
		else {
			CG_Text_Paint ( x + 12, y + 9, 0.18f, dead ? skinInfos[CG_WhichSkin(team)].ui_dead_color : skinInfos[CG_WhichSkin(team)].ui_inv_color, s, 0, 0, 0 ); // dead?colorGrey:colorWhite
		}
	}

	CG_DrawRect(x, y - 1, width, 12, 1, skinInfos[CG_WhichSkin(team)].ui_dead_color);

	if (!dead) {
		CG_DrawRect(x + 2, y + 1, 8, 8, 1, skinInfos[CG_WhichSkin(team)].ui_inv_color);
		CG_FillRect(x + 3, y + 2, 6, 6, col);
	} else {
		CG_DrawRect(x + 3, y + 2, 6, 6, 1, col);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name        : CG_DrawMiniScoreboard
// Description : Displays the miniscoreboard
// Author      : Apoxol
// Revision    : Fenix  - 20th March, 2013
///////////////////////////////////////////////////////////////////////////////////////////
static void CG_DrawMiniScoreboard ( void )
{
	static float  colorWhite[4]        = { 1.0f,  1.0f,  1.0f,  1.0f };
	static float  colorGrey[4]         = { 0.5f,  0.5f,  0.5f,  1.0f };
	/*
    static float  colorBlue[4]         = { 0.0f,  0.0f,  0.5f,     1 };
    static float  colorBlue2[4]        = { 0.0f,  0.0f,  0.5f, 0.75f };
    static float  colorRed[4]          = {  0.5,  0.0f,  0.0f,     1 };
    static float  colorRed2[4]         = {  0.5,  0.0f,  0.0f, 0.75f };
    static float  colorBlack[4]        = { 0.0f,  0.0f,  0.0f,  0.8f };
    static float  colorBlack2[4]       = { 0.0f,  0.0f,  0.0f,  0.3f };
    static float  colorYellow[4]       = { 0.5f,  0.5f,  0.0f,  1.0f };
    static float  colorYellow2[4]      = { 0.4f,  0.4f,  0.2f, 0.75f };
    static float  colorJumpMode[4]     = { 0.42,  0.22,  0.53,   1.0 };
    static float  colorJumpMode2[4]    = { 0.42,  0.22,  0.53, 0.75f };
    static float  colorGreen[4]        = { 0.0f, 0.37f, 0.09f,  1.0f };
    static float  colorGreen2[4]       = { 0.0f, 0.37f, 0.09f, 0.75f };
		*/

    vec4_t jumpRunTimerColors[6] = {
                                        { 1.0f,   1.0f,  1.0f,    1.0f },     // White
                                        { 1.0f,   0.0f,  0.0f,    1.0f },     // Red
                                        { 0.0f,   1.0f,  0.0f,    1.0f },     // Light Green
                                        { 0.0f,  0.37f, 0.09f,    1.0f },     // Dark Green
                                        { 0.5f,   0.5f,  0.5f,    1.0f },     // Grey
                                        { 1.0f,   1.0f,  0.0f,    1.0f }      // Grey
                                   };

    char   *s;
    char   name[10];
    int    l, fps, team, xx = 540, yy = 120, y = 4;
    int    w;
    int    msec;
    int    colorNumber;
    float  finalcolor[4];
    float  remainder;
    utSkins_t sidx;

    if (!cg_drawTeamOverlayScores.integer)
        xx = 560;

    team = cgs.clientinfo[cg.predictedPlayerState.clientNum].team;

    // If there's no valid snap.
    if (!cg.snap)
        return;

    if (cg_drawFPS.integer) {

        CG_DrawRect ( 524, 4, 107, 16, 1, colorGrey );
        CG_FillRect ( 525, 5, 105, 14, colorBlack );
        fps = cg.q3fps; // OMFG, can you belive this was causing physics bugs?!

        if (cg_drawFPS.integer == 1) {
            fps = cg.fpsnow;
        }

        s = va ( "%i fps", fps );
        CG_Text_Paint ( 524 + 4, 16, 0.22f, colorWhite, s, 0, 0, 0 );

        s = va ( "%i ping", (cg.snap && !cg.demoPlayback) ? cg.snap->ping : 0 );
        CG_Text_Paint ( 627 - CG_Text_Width(s, 0.22f, 0), 16, 0.22f, colorWhite, s, 0, 0, ITEM_TEXTSTYLE_OUTLINED );

        y += 19;

    }

    if (cg_drawTeamScores.integer && (cgs.gametype >= GT_TEAM) && (cgs.gametype != GT_JUMP) && cg.snap) {

        int  teamCount[TEAM_NUM_TEAMS]      = { 0 };
        int  teamCountAlive[TEAM_NUM_TEAMS] = { 0 };
        int  i;

        for(i = 0; i < MAX_CLIENTS; i++) {

            if(cgs.clientinfo[i].team == TEAM_RED) {

                teamCount[TEAM_RED]++;

                if (!cgs.clientinfo[i].ghost || !(CG_ENTITIES(i)->currentState.eFlags & EF_DEAD)) {
                    teamCountAlive[TEAM_RED]++;
                }

            } else if (cgs.clientinfo[i].team == TEAM_BLUE) {

                teamCount[TEAM_BLUE]++;

                if (!cgs.clientinfo[i].ghost || !(CG_ENTITIES(i)->currentState.eFlags & EF_DEAD)) {
                    teamCountAlive[TEAM_BLUE]++;
                }
            }

        }


        // Red team
        sidx = CG_WhichSkin(TEAM_RED);
        finalcolor[0] = skinInfos[sidx].ui_miniscoreboard_fill_color[0];
        finalcolor[1] = skinInfos[sidx].ui_miniscoreboard_fill_color[1];
        finalcolor[2] = skinInfos[sidx].ui_miniscoreboard_fill_color[2];
        finalcolor[3] = skinInfos[sidx].ui_miniscoreboard_fill_color[3];

        // Sin wave if the red flag has been taken
        if (cgs.redflag > 0) finalcolor[3] = 0.5f + (sin((float)cg.time / 100.0f) / 2);

        if (cgs.gametype == GT_UT_CAH) {
            CG_DrawRect ( 524, y, 107, 14, 1, colorGrey );
            CG_FillRect ( 525, y + 1, 78-2, 12, finalcolor );
            CG_FillRect ( 525+76, y + 1, 31-2, 12, colorRed );
        } else {
            CG_DrawRect ( 524, y, 78, 14, 1, colorGrey );
            CG_DrawRect ( 524+80, y, 27, 14, 1, colorGrey );
            CG_FillRect ( 525, y + 1, 78-2, 12, finalcolor );
            CG_FillRect ( 525+80, y + 1, 27-2, 12, colorRed );
        }

        // CG_FillRectHGrad( 545+50, y + 1, 95-50, 12, skinInfos[2].ui_teamscores_teambar_color);

        if(cg_drawTeamScores.integer == 1) {

            // Removed at 27's request
            // CG_Text_Paint ( 544 + 7, y + 12, 0.22f, colorWhite, va("red [%d/%d]", teamCountAlive[TEAM_RED], teamCount[TEAM_RED]), 0, 0, 0 );
            // If we have a valid team name.
            l = strlen(cg.teamNameRed);

            if (l > 0) {

                if (l < 10) {
                    strcpy(name, cg.teamNameRed);
                } else {
                    memcpy(name, cg.teamNameRed, 10);
                    name[9] = '\0';
                    name[8] = '.';
                    name[7] = '.';
                }

                CG_Text_Paint ( 524 + 4, y + 11, 0.22f, skinInfos[sidx].ui_inv_color, name, 0, 0, 0 );
            }
            else {
                CG_Text_Paint ( 524 + 4, y + 11, 0.22f, skinInfos[sidx].ui_inv_color, skinInfos[sidx].defaultTeamName, 0, 0, 0 );
            }

        }

        if (cgs.gametype == GT_UT_CAH) {
            s = va("%i | %i", cg.snap->ps.persistant[PERS_SCORERB] & 0xFF, cg.snap->ps.persistant[PERS_FLAGSRB] & 0xFF ); //r00t
        } else {
            s = va("%i", cg.teamScores[0]);
        }

        CG_Text_Paint ( 627 - CG_Text_Width(s, 0.22f, 0), y + 11, 0.22f, colorWhite, s, 0, 0, 0 );

        y += 17;

        // Blue team
        sidx = CG_WhichSkin(TEAM_BLUE);
        finalcolor[0] = skinInfos[sidx].ui_miniscoreboard_fill_color[0];
        finalcolor[1] = skinInfos[sidx].ui_miniscoreboard_fill_color[1];
        finalcolor[2] = skinInfos[sidx].ui_miniscoreboard_fill_color[2];
        finalcolor[3] = skinInfos[sidx].ui_miniscoreboard_fill_color[3];

        // Sin wave if the red flag has been taken
        if (cgs.blueflag > 0) finalcolor[3] = 0.5f + (cos((float)cg.time / 100.0f) / 2);

        if (cgs.gametype == GT_UT_CAH) {
            CG_DrawRect ( 524, y, 107, 14, 1, colorGrey );
            CG_FillRect ( 525, y + 1, 78-2, 12, finalcolor );
            CG_FillRect ( 525+76, y + 1, 31-2, 12, colorBlue );
        } else {
            CG_DrawRect ( 524, y, 78, 14, 1, colorGrey );
            CG_DrawRect ( 524+80, y, 27, 14, 1, colorGrey );
            CG_FillRect ( 525, y + 1, 78-2, 12, finalcolor );
            CG_FillRect ( 525+80, y + 1, 27-2, 12, colorBlue );
        }

        if(cg_drawTeamScores.integer == 1) {

        	// removed at 27's request
            // CG_Text_Paint ( 544 + 7, y + 12, 0.22f, colorWhite, va("blue [%d/%d]", teamCountAlive[TEAM_BLUE], teamCount[TEAM_BLUE]), 0, 0, 0 );
            // If we have a valid team name.
            l = strlen(cg.teamNameBlue);

            if (l > 0) {

                if (l < 10) {
                    strcpy(name, cg.teamNameBlue);
                } else {
                    memcpy(name, cg.teamNameBlue, 10);
                    name[9] = '\0';
                    name[8] = '.';
                    name[7] = '.';
                }

                CG_Text_Paint ( 524 + 4, y + 11, 0.22f, skinInfos[sidx].ui_inv_color, name, 0, 0, 0 );
            } else {
                CG_Text_Paint ( 524 + 4, y + 11, 0.22f, skinInfos[sidx].ui_inv_color, skinInfos[sidx].defaultTeamName, 0, 0, 0 );
            }
        }

        if (cgs.gametype == GT_UT_CAH) {
            s = va("%i | %i", cg.snap->ps.persistant[PERS_SCORERB]>>8, cg.snap->ps.persistant[PERS_FLAGSRB]>>8 ); //r00t
        } else {
            s = va("%i", cg.teamScores[1]);
        }

        CG_Text_Paint ( 627 - CG_Text_Width(s, 0.22f, 0), y + 11, 0.22f, colorWhite, s, 0, 0, 0 );

        y += 19;

    }

    if (cg_drawTimer.integer) {

        s = CG_GetTimerString();
	if (cgs.survivor && cgs.g_maxrounds) {
		w = 524;
	} else {
		w = 544;
	}
        CG_DrawPic(w, y, 16, 16, cgs.media.UT_timerShader);
        CG_Text_Paint(w + 20, y + 14, 0.25f, colorWhite, s, 0, 0, 4);

        // B1n: Wave respawn timers when not following player
        if (!cgs.survivor && cgs.waveRespawns && (cgs.gametype >= GT_TEAM) && (cgs.gametype != GT_JUMP))
        {
            s = CG_GetWaveTimerString(TEAM_RED);

            CG_DrawPic(544, y + 17, 16, 16, cgs.media.UT_waveShader[CG_WhichSkin(TEAM_RED)]);
            CG_Text_Paint(544 + 20, y + 17 + 14, 0.25f, colorWhite, s, 0, 0, 4);

            s = CG_GetWaveTimerString(TEAM_BLUE);

            CG_DrawPic(544, y + 34, 16, 16, cgs.media.UT_waveShader[CG_WhichSkin(TEAM_BLUE)]);
            CG_Text_Paint(544 + 20, y + 34 + 14, 0.25f, colorWhite, s, 0, 0, 4);
            y += 34;
        }

        if (!cgs.survivor && (cgs.gametype == GT_CTF) && (cgs.g_hotpotato > 0)) {

            s = CG_GetHotPotatoString();
            CG_DrawPic(544, y + 18, 16, 16, cgs.media.UT_potatoShaders[CG_WhichSkin(TEAM_RED)][CG_WhichSkin(TEAM_BLUE)]);

            CG_Text_Paint(544 + 20, y + 18 + 14, 0.25f, colorWhite, s, 0, 0, 4);
        }


        if (cgs.gametype == GT_JUMP) {

            //////////////////////// RUNNING TIME ////////////////////////////
            y += 18;
            s = "0:00:00.000";    // Initializing empty timer string
            colorNumber = 0;      // Initializing colorNumber -> white

            // Calculating the string to be displayed
            if (cgs.clientinfo[cg.snap->ps.clientNum].jumpStartTime > 0) {
                // Current running time
                msec = cg.time - cgs.clientinfo[cg.snap->ps.clientNum].jumpStartTime;
                s = CG_GetJumpTime(msec);
            } else if (cgs.clientinfo[cg.snap->ps.clientNum].jumpLastEstablishedTime > 0) {
                // Last time established
                s = CG_GetJumpTime(cgs.clientinfo[cg.snap->ps.clientNum].jumpLastEstablishedTime);
            }

            // Calculating the color to be displayed
            if (!cgs.clientinfo[cg.snap->ps.clientNum].jumprun) {

                // Timer not activated
                colorNumber = 4; // -> grey

            } else {

                // Timer activated
                if (cgs.clientinfo[cg.snap->ps.clientNum].jumpBestTime > 0) {

                	// Timer activated and best time established
                    if (cgs.clientinfo[cg.snap->ps.clientNum].jumpStartTime > 0) {

                    	// Timer activated, best time established and run timer running
                        if (msec < cgs.clientinfo[cg.snap->ps.clientNum].jumpBestTime) {

                        	// Timer activated, best time established, run timer running
                            // and it's values is still lower than the best timer one
                            // Display normal/flashing timer according to it's value
                            remainder = (msec / 1000) % 60;
                            if ((remainder >= 0) && (remainder <= 2) && ((msec >> 9) & 1)) {
                                colorNumber = 5; // -> yellow
                            }
                        }
                        else {
                            // Timer activated, best time established, run timer running
                            // but it's value is greater than the best time one
                            colorNumber = 1; // -> red
                        }
                    }

                } else {

                    // Timer activated but no best time established
                    if (cgs.clientinfo[cg.snap->ps.clientNum].jumpStartTime > 0) {

                    	// Timer activated, no best time established and running time running
                        // Display normal/flashing timer according to it's value
                        remainder = (msec / 1000) % 60;
                        if ((remainder >= 0) && (remainder <= 2) && ((msec >> 9) & 1)) {
                            colorNumber = 5; // -> yellow
                        }

                    }
                }
            }

            CG_DrawPic(544, y, 16, 16, cgs.media.UT_jumpRunningTimer);
            CG_Text_Paint(544 + 20, y + 14, 0.25f, jumpRunTimerColors[colorNumber], s, 0, 0, 4);

            //////////////////////// BEST TIME ////////////////////////////
            y += 18;
            s = "0:00:00.000";    // Initializing empty timer string
            colorNumber = 0;      // Initializing colorNumber -> white

            //Calculating the string to be displayed
            if (cgs.clientinfo[cg.snap->ps.clientNum].jumpBestTime > 0) {
                msec = cgs.clientinfo[cg.snap->ps.clientNum].jumpBestTime;
                s = CG_GetJumpTime(msec);
            }

            //Calculating the color to be displayed
            if (!cgs.clientinfo[cg.snap->ps.clientNum].jumprun) {

                //Timer not activated
                if (cgs.clientinfo[cg.snap->ps.clientNum].jumpBestTime > 0) {
                    //Timer not activated and best time established
                    colorNumber = 3; // -> dark green
                } else {
                    //Timer not activated and best time not established
                    colorNumber = 4; // -> grey
                }

            } else {

            	//Timer activated
                if (cgs.clientinfo[cg.snap->ps.clientNum].jumpBestTime > 0) {
                    //Timer activated and best time established
                    colorNumber = 2; // -> light green
                }

            }

            CG_DrawPic(544, y, 16, 16, cgs.media.UT_jumpBestTimer);
            CG_Text_Paint(544 + 20, y + 14, 0.25f, jumpRunTimerColors[colorNumber], s, 0, 0, 4);

        }
    }

    //@barba @holblin
    if (cg_drawTeamOverlay.integer && /* (cgs.gametype >= GT_TEAM) && (cgs.gametype != GT_FFA) && */ (team != TEAM_SPECTATOR)) {   //&& cgs.survivor

        int       i, OurNum;
        int       count = 0;
        int       alivecountblue = 0, alivecountred = 0, alivecountfree = 0;
        int       *alivecount = NULL;
        //vec4_t    *colorTeam;

        OurNum = cg.predictedPlayerState.clientNum;

        //get the counts
        for (i = 0; i < cgs.maxclients; i++) {

            if (cgs.clientinfo[i].name[0] == '\0')
                continue;

            if (!(cgs.clientinfo[i].ghost && (CG_ENTITIES(i)->currentState.eFlags & EF_DEAD))) {
                if (cgs.clientinfo[i].team == TEAM_RED)
                    alivecountred++;
                else if (cgs.clientinfo[i].team == TEAM_BLUE)
                    alivecountblue++;
                else {

                    //@Barbatos bugfix 4.2.010 don't count us as an enemy
                    if(OurNum == i) continue;
                    alivecountfree++;

                }
            }
        }

        y = 420;

        //@Barba: don't draw us if cg_drawteamoverlay = 4
        if(cg_drawTeamOverlay.integer != 4) {

            //draw US
            for (i = 0; i < cgs.maxclients ; i++) {

            	if (!cgs.clientinfo[i].infoValid)
                    continue;

                if (OurNum == i)
                    DrawClientOverlay(i, xx, y, yy, team);

            }
        }

        y -= 13;

        if (team != TEAM_FREE) { // @holblin

            if ((cg_drawTeamOverlay.integer == 3) || (cg_drawTeamOverlay.integer == 4)) {
		sidx = CG_WhichSkin(team);
                CG_FillRect ( xx, y, yy, 11, skinInfos[sidx].ui_miniscoreboard_fill_color );
                CG_Text_Paint( xx + 3, y + 9, 0.18f, skinInfos[sidx].ui_inv_color, va("Friendly: %d", team == TEAM_RED ? alivecountred : alivecountblue), 0, 0, 0 );
                CG_DrawRect(xx, y - 1, yy, 12, 1, colorGrey);
                y -= 11;

            } else {

                //draw our team, everyone BUT us
                for (i = 0; i < cgs.maxclients ; i++) {

                    if (OurNum == i)
                        continue;

                    if (cgs.clientinfo[i].substitute)
                        continue;

                    if (cgs.clientinfo[i].team != team)
                        continue;

                    DrawClientOverlay(i, xx, y, yy, team);
                    y -= 11;
                    count++;

                }
            }
        }

        //draw the remaining enemy
        y -= 5;

        if ((cg_drawTeamOverlay.integer == 2) || (cg_drawTeamOverlay.integer == 3)) {

            if (team == TEAM_RED) {
                alivecount = &alivecountblue;
		sidx = CG_WhichSkin(TEAM_BLUE);
            } else if (team == TEAM_BLUE) {
                alivecount = &alivecountred;
		sidx = CG_WhichSkin(TEAM_RED);
            } else if (team == TEAM_FREE) {
                alivecount = &alivecountfree;
		sidx = CG_WhichSkin(TEAM_FREE);
            }

            CG_FillRect ( xx, y, yy, 11, skinInfos[sidx].ui_miniscoreboard_fill_color ); //dead?colorBlack2:colorBlack );
            CG_Text_Paint( xx + 3, y + 9, 0.18f, skinInfos[sidx].ui_inv_color, va("Enemies: %d", *alivecount), 0, 0, 0 );
            CG_DrawRect(xx, y - 1, yy, 12, 1, colorGrey);

        } else {

            for (i = 0; i < cgs.maxclients ; i++) {

            	if (cgs.clientinfo[i].name[0] == '\0') // @holblin
                    continue;

                if (OurNum == i)
                    continue;

                if (cgs.clientinfo[i].substitute)
                    continue;

                // if (((cgs.clientinfo[i].team != team) || (team == TEAM_FREE)  && (cgs.clientinfo[i].team != TEAM_SPECTATOR) ) )
                if ((((cgs.clientinfo[i].team != team) && (cgs.clientinfo[i].team != TEAM_FREE)) || (team == TEAM_FREE)) && (cgs.clientinfo[i].team != TEAM_SPECTATOR)) {

                    DrawClientOverlay(i, xx, y, yy, team);
                    y -= 11;
                    count++;

                }
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : CG_DrawRadarElement
// Description: Displays an element on the radar
// Author	  : Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
#if 0
static void CG_DrawRadarElement ( float x, float y, float angle, float radius, float width, float height, qhandle_t shader )
{
	x += sin ( angle ) * radius;
	y += cos ( angle ) * radius;

	trap_R_SetColor( NULL );
	CG_DrawPic ( x - width / 2, y - height / 2, width, height, shader );
}
#endif

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : CG_DrawRadar
// Description: Displays the available flags on the radar
// Author	  : Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
#if 0
static void CG_DrawRadar ( float x, float y )
{
	/*
	int flag;

	CG_DrawPic ( x - 64 / 2, y - 64 / 2, 64, 64, cgs.media.UT_Radar );

	for ( flag = 0; cgs.gameFlags[flag].number; flag ++ )
	{
		vec3_t dirLook;
		vec3_t dirFlag;
		float  angleLook;
		float  angleFlag;
		float  distance;

		VectorSubtract ( cg.predictedPlayerState.origin, cgs.gameFlags[flag].origin, dirFlag );
		AngleVectors ( cg.predictedPlayerState.viewangles, dirLook, NULL, NULL );

		dirLook[2] = 0;
		dirFlag[2] = 0;

		VectorNormalize ( dirFlag, distance );
		angleFlag = atan2(dirFlag[0],dirFlag[1]);

		VectorNormalizeNR ( dirLook );
		angleLook = atan2(dirLook[0],dirLook[1]);

		if ( distance > cg_radarRange.value )
			distance = cg_radarRange.value;

		distance = distance / cg_radarRange.value;

		CG_DrawRadarElement ( x + 0.5f, y, angleLook - angleFlag, distance * RADAR_RADIUS, 8, 8, cgs.media.UT_FlagIcon [ cgs.gameFlags[flag].team ] );
	}
	*/
}
#endif

#if 1
/**
 * $(function)
 */
static void CG_DrawPauseStatus(void)
{
	const char	*s;
	char		temp[32];
	int 	colourNumber;
	vec4_t		colours[2] = {
		{ 1.0, 1.0, 0.0, 1.0},
		{ 1.0, 0.0, 0.0, 1.0}
	};

	// Select colour at random.
	if ((cg.time >> 10) & 1)
	{
		colourNumber = 1;
	}
	else
	{
		colourNumber = 0;
	}

	// If we're paused.
	if (cg.pauseState & UT_PAUSE_ON)
	{
		// If we're getting ready to resume.
		if (cg.pauseState & UT_PAUSE_TR)
		{
			// If time is not set.
/*			if (cg.pauseTime == 0)
			{
				// Set time.
				cg.pauseTime = cg.time + 3000;
			} */

			s = va("Game will resume in %d", (int)((cg.pauseTime - cg.time) / 1000) + 1 );

			// Draw string.
			CG_Text_Paint(320 - CG_Text_Width(s, 0.425f, 0) / 2, 220, 0.425f, colours[colourNumber], s, 0, 0, ITEM_TEXTSTYLE_OUTLINED);
		}
		else
		{
			// If time is not set.
			if (cg.pauseTime == 0)
			{
				// If we're not a spectator.
				if (cgs.clientinfo[cg.predictedPlayerState.clientNum].team != TEAM_SPECTATOR)
				{
					// Get m_yaw value.
					trap_Cvar_VariableStringBuffer("m_yaw", temp, sizeof(temp));

					// Backup value.
					trap_Cvar_Set("cg_pauseYaw", temp);

					// Get m_pitch value.
					trap_Cvar_VariableStringBuffer("m_pitch", temp, sizeof(temp));

					// Backup value.
					trap_Cvar_Set("cg_pausePitch", temp);

					// Clear cvar values.
					trap_Cvar_Set("m_yaw", "0");
					trap_Cvar_Set("m_pitch", "0");
				}

				// Set time.
//				cg.pauseTime = cg.time;
			}

			// Set text.
			if (cg.pauseState & UT_PAUSE_RC)
			{
				s = va("Game is paused by ^1red");
			}
			else
			if (cg.pauseState & UT_PAUSE_BC)
			{
				s = va("Game is paused by ^4blue");
			}
			else
			{
				s = va("Game is paused by Admin");
			}

			// Draw string.
			CG_Text_Paint(320 - CG_Text_Width(s, 0.425f, 0) / 2, 220, 0.425f, colours[colourNumber], s, 0, 0, ITEM_TEXTSTYLE_OUTLINED);
		}
	}
	else
	{
		// If we're getting ready to pause.
		if (cg.pauseState & UT_PAUSE_TR)
		{
			// If time is not set.
/*			if (cg.pauseTime == 0)
			{
				// Set time.
				cg.pauseTime = cg.time + 3000;
			}*/

			if (cg.pauseTime > cg.time)
			{
				// If this is a survivor game type.
				if (cgs.survivor)
				{
					if (cg.pauseState & UT_PAUSE_RC)
					{
						s = va("^1Red ^7called a timeout at round end");
					}
					else
					if (cg.pauseState & UT_PAUSE_BC)
					{
						s = va("^4Blue ^7called a timeout at round end");
					}
					else
					{
						s = va("Admin called a pause at round end");
					}
				}
				else
				{
					if (cg.pauseState & UT_PAUSE_RC)
					{
						s = va("^1Red ^7has called a timeout in %d", (int)((cg.pauseTime - cg.time) / 1000) + 1 );
					}
					else
					if (cg.pauseState & UT_PAUSE_BC)
					{
						s = va("^4Blue ^7has called a timeout in %d", (int)((cg.pauseTime - cg.time) / 1000) + 1);
					}
					else
					{
						s = va("Admin pausing in %d", (int)((cg.pauseTime - cg.time) / 1000) + 1);
					}
				}

				// Draw string.
				CG_Text_Paint(320 - CG_Text_Width(s, 0.425f, 0) / 2, 96, 0.425f, colours[colourNumber], s, 0, 0, ITEM_TEXTSTYLE_OUTLINED);
			}
		}
		else
		{
			// If time is not set.
			if (cg.pauseTime == 0)
			{
				// If we're not a spectator.
				if (cgs.clientinfo[cg.predictedPlayerState.clientNum].team != TEAM_SPECTATOR)
				{
					// Get backed up value.
					trap_Cvar_VariableStringBuffer("cg_pauseYaw", temp, sizeof(temp));

					// If we have a valid value.
					if (atof(temp) != 0.0)
					{
						trap_Cvar_Set("m_yaw", temp);
					}

					// Get backed up value.
					trap_Cvar_VariableStringBuffer("cg_pausePitch", temp, sizeof(temp));

					// If we have a valid value.
					if (atof(temp) != 0.0)
					{
						trap_Cvar_Set("m_pitch", temp);
					}
				}

				// Set time.
//				cg.pauseTime = cg.time;
			}
		}
	}
}
#else
/**
 * $(function)
 */
static void CG_DrawPauseStatus(void)
{
	const char	*s;
	char		temp[32];
	int 	colourNumber;
	vec4_t		colours[2] = {
		{ 1.0, 1.0, 0.0, 1.0},
		{ 1.0, 0.0, 0.0, 1.0}
	};

	// Select colour at random.
	if ((cg.time >> 10) & 1)
	{
		colourNumber = 1;
	}
	else
	{
		colourNumber = 0;
	}

	// If we're paused.
	if (cg.pauseState & UT_PAUSE_ON)
	{
		// If we're getting ready to resume.
		if (cg.pauseState & UT_PAUSE_TR)
		{
			// If time is not set.
			if (cg.pauseTime == 0)
			{
				// Set time.
				cg.pauseTime = cg.time + 5000;
			}

			// If it's time to resume the game.
			if (cg.time >= cg.pauseTime)
			{
				s = va("GAME RESUMING NOW!");
			}
			else
			{
				s = va("GAME RESUMES IN: %d", (int)((cg.pauseTime - cg.time) / 1000));
			}

			// Draw string.
			CG_Text_Paint(320 - CG_Text_Width(s, 0.75f, 0) / 2, 200, 0.75f, colours[colourNumber], s, 0, 0, ITEM_TEXTSTYLE_OUTLINED);
		}
		else
		{
			// If time is not set.
			if (cg.pauseTime == 0)
			{
				// If we're not a spectator.
				if (cgs.clientinfo[cg.predictedPlayerState.clientNum].team != TEAM_SPECTATOR)
				{
					// Get m_yaw value.
					trap_Cvar_VariableStringBuffer("m_yaw", temp, sizeof(temp));

					// Backup value.
					trap_Cvar_Set("cg_pauseYaw", temp);

					// Get m_pitch value.
					trap_Cvar_VariableStringBuffer("m_pitch", temp, sizeof(temp));

					// Backup value.
					trap_Cvar_Set("cg_pausePitch", temp);

					// Clear cvar values.
					trap_Cvar_Set("m_yaw", "0");
					trap_Cvar_Set("m_pitch", "0");
				}

				// Set time.
//				cg.pauseTime = cg.time;
			}

			// Set text.
			s = va("GAME IS PAUSED");

			// Draw string.
			CG_Text_Paint(320 - CG_Text_Width(s, 0.75f, 0) / 2, 200, 0.75f, colours[colourNumber], s, 0, 0, ITEM_TEXTSTYLE_OUTLINED);
		}
	}
	else
	{
		// If we're getting ready to pause.
		if (cg.pauseState & UT_PAUSE_TR)
		{
			// If time is not set.
/*			if (cg.pauseTime == 0)
			{
				// Set time.
				cg.pauseTime = cg.time + 5000;
			} */

			// If it's time to pause the game.
			if (cg.time >= cg.pauseTime)
			{
				s = va("GAMES PAUSING NOW!");
			}
			else
			{
				s = va("GAME PAUSE IN: %d", (int)((cg.pauseTime - cg.time) / 1000));
			}

			// Draw string.
			CG_Text_Paint(320 - CG_Text_Width(s, 0.75f, 0) / 2, 200, 0.75f, colours[colourNumber], s, 0, 0, ITEM_TEXTSTYLE_OUTLINED);
		}
		else
		{
			// If time is not set.
			if (cg.pauseTime == 0)
			{
				// If we're not a spectator.
				if (cgs.clientinfo[cg.predictedPlayerState.clientNum].team != TEAM_SPECTATOR)
				{
					// Get backed up value.
					trap_Cvar_VariableStringBuffer("cg_pauseYaw", temp, sizeof(temp));

					// If we have a valid value.
					if (atof(temp) != 0.0)
					{
						trap_Cvar_Set("m_yaw", temp);
					}

					// Get backed up value.
					trap_Cvar_VariableStringBuffer("cg_pausePitch", temp, sizeof(temp));

					// If we have a valid value.
					if (atof(temp) != 0.0)
					{
						trap_Cvar_Set("m_pitch", temp);
					}
				}

				// Set time.
//				cg.pauseTime = cg.time;
			}
		}
	}
}
#endif

/**
 * $(function)
 */
static void CG_DrawMatchStatus(void)
{
	const char	*s;

	// Dont display during warmup or when some team doesn't have any players.
	if (cg.warmup != 0)
	{
		return;
	}

	// If server is not in match mode.
	if (!(cg.matchState & UT_MATCH_ON))
	{
		return;
	}

	// If we shouldn't display message.
	if (!((cg.time >> 10) & 3))
	{
		return;
	}

	// If both teams are not ready.
	if (!(cg.matchState & UT_MATCH_RR) && !(cg.matchState & UT_MATCH_BR))
	{
		// Set text.
		s = va("Teams are not ready");

		// Draw string.
		CG_Text_Paint(320 - CG_Text_Width(s, 0.425f, 0) / 2, 64, 0.425f, colorWhite, s, 0, 0, 4);
	}
	// If red team is not ready.
	else if (!(cg.matchState & UT_MATCH_RR))
	{
		// Set text.
		s = va("^1Red^7 team not ready");

		// Draw string.
		CG_Text_Paint(320 - CG_Text_Width(s, 0.425f, 0) / 2, 64, 0.425f, colorWhite, s, 0, 0, 4);
	}
	// If blue team is not ready.
	else if (!(cg.matchState & UT_MATCH_BR))
	{
		// Set text.
		s = va("^4Blue^7 team not ready");

		// Draw string.
		CG_Text_Paint(320 - CG_Text_Width(s, 0.425f, 0) / 2, 64, 0.425f, colorWhite, s, 0, 0, 4);
	}
}
////////// BREAKOUT //////////

#define BREAKOUT_MAX_FRAME_TIME    1.0 / 30.0

#define BREAKOUT_BORDER_WIDTH	   1

#define BREAKOUT_SMALL_CHAR_WIDTH  16
#define BREAKOUT_SMALL_CHAR_HEIGHT BREAKOUT_SMALL_CHAR_WIDTH * (4 / 3)

#define BREAKOUT_BIG_CHAR_WIDTH    20
#define BREAKOUT_BIG_CHAR_HEIGHT   BREAKOUT_BIG_CHAR_WIDTH * (4 / 3)

#define BREAKOUT_TEXT_X 	   320
#define BREAKOUT_TEXT_Y 	   20

#define BREAKOUT_STATUS_TEXT_X	   320
#define BREAKOUT_STATUS_TEXT_Y		50

#define BREAKOUT_LEVEL_TEXT_X	   145
#define BREAKOUT_LEVEL_TEXT_Y	   70

#define BREAKOUT_LIVES_TEXT_X	   495
#define BREAKOUT_LIVES_TEXT_Y	   70

#define BREAKOUT_BOARD_X	   145
#define BREAKOUT_BOARD_Y	   90
#define BREAKOUT_BOARD_WIDTH	   350
#define BREAKOUT_BOARD_HEIGHT	   350

#define BREAKOUT_BRICK_WIDTH	   35
#define BREAKOUT_BRICK_HEIGHT	   20

#define BREAKOUT_BRICKS_X	   145
#define BREAKOUT_BRICKS_Y	   110
#define BREAKOUT_BRICKS_WIDTH	   BREAKOUT_BRICK_COLUMNS * BREAKOUT_BRICK_WIDTH
#define BREAKOUT_BRICKS_HEIGHT	   BREAKOUT_BRICK_ROWS * BREAKOUT_BRICK_HEIGHT

#define BREAKOUT_PADDLE_X	   320
#define BREAKOUT_PADDLE_Y	   430
#define BREAKOUT_PADDLE_WIDTH	   60
#define BREAKOUT_PADDLE_HEIGHT	   5
#define BREAKOUT_PADDLE_SPEED	   150.0

#define BREAKOUT_BALL_X 	   320
#define BREAKOUT_BALL_Y 	   420
#define BREAKOUT_BALL_WIDTH    10
#define BREAKOUT_BALL_HEIGHT	   10
#define BREAKOUT_BALL_SPEED    125.0
#define BREAKOUT_BALL_MAX_SPEED    300.0

#define BREAKOUT_MAX_LIVES	   3

#define BREAKOUT_DELAY_TIME    1000

/**
 * $(function)
 */
static void breakout_ResetBricks(void)
{
	int  x, y, r;

	// Initialize bricks.
	for (y = 0; y < BREAKOUT_BRICK_ROWS; y++)
	{
		for (x = 0; x < BREAKOUT_BRICK_COLUMNS; x++)
		{
			// Generate random number;
			r = rand() % 51;

			// Initialize brick.
			if (cg.breakout.level > r)
			{
				cg.breakout.bricks[y][x] = 3;
			}
			else if (cg.breakout.level * 2 > r)
			{
				cg.breakout.bricks[y][x] = 2;
			}
			else
			{
				cg.breakout.bricks[y][x] = 1;
			}
		}
	}

	// Reset "dead" bricks number.
	cg.breakout.bricksOut = 0;
}

/**
 * $(function)
 */
static void breakout_ResetPaddle(void)
{
	cg.breakout.paddle.x = BREAKOUT_PADDLE_X;
	cg.breakout.paddle.v = 0;
}

/**
 * $(function)
 */
static void breakout_ResetBall(void)
{
	int  r1, r2;

	// Generate random numbers.
	r1		   = rand() % 301;
	r2		   = rand() % 2;

	cg.breakout.ball.x = BREAKOUT_BALL_X;
	cg.breakout.ball.y = BREAKOUT_BALL_Y;

	// Calculate random direction vector.
	cg.breakout.ball.v[1] = ((float)r1 / 1000.0f) - 0.9f;
	cg.breakout.ball.v[0] = (1.0f + cg.breakout.ball.v[1]) * (r2 ? 1.0f : -1.0f);
}

/**
 * $(function)
 */
static void breakout_Reset(void)
{
	// Reset state.
	cg.breakout.initialized = qtrue;
	cg.breakout.reset	= qfalse;
	cg.breakout.pause	= qfalse;
	cg.breakout.gameover	= qfalse;

	// Reset time.
	cg.breakout.time  = cg.time;
	cg.breakout.delay = 0;

	// Reset status.
	cg.breakout.level = 1;
	cg.breakout.lives = BREAKOUT_MAX_LIVES;

	// Reset bricks.
	breakout_ResetBricks();

	// Reset paddle.
	breakout_ResetPaddle();

	// Reset ball.
	breakout_ResetBall();
}

/**
 * $(function)
 */
static void breakout_UpdatePaddle(float time)
{
	// Calculate new paddle position.
	cg.breakout.paddle.x += cg.breakout.paddle.v * BREAKOUT_PADDLE_SPEED * time;

	// Make sure paddle is not out of left bound.
	if (cg.breakout.paddle.x - (BREAKOUT_PADDLE_WIDTH / 2) < BREAKOUT_BOARD_X)
	{
		// Adjust paddle position.
		cg.breakout.paddle.x = BREAKOUT_BOARD_X + (BREAKOUT_PADDLE_WIDTH / 2);

		// Reset paddle direction.
		cg.breakout.paddle.v = 0;
	}

	// Make sure paddle is not out of right bound.
	if (cg.breakout.paddle.x + (BREAKOUT_PADDLE_WIDTH / 2) > BREAKOUT_BOARD_X + BREAKOUT_BOARD_WIDTH)
	{
		// Adjust paddle position.
		cg.breakout.paddle.x = BREAKOUT_BOARD_X + BREAKOUT_BOARD_WIDTH - (BREAKOUT_PADDLE_WIDTH / 2);

		// Reset paddle direction.
		cg.breakout.paddle.v = 0;
	}
}

/**
 * $(function)
 */
static void breakout_UpdateBall(float time)
{
	float  speed;

	// Calculate ball speed.
	speed = BREAKOUT_BALL_SPEED * (1.0f + (((float)cg.breakout.level - 1.0f) * 0.1f));

	// Clamp speed.
	if (speed > BREAKOUT_BALL_MAX_SPEED)
	{
		speed = BREAKOUT_BALL_MAX_SPEED;
	}

	// Calculate new ball position.
	cg.breakout.ball.x += cg.breakout.ball.v[0] * speed * time;
	cg.breakout.ball.y += cg.breakout.ball.v[1] * speed * time;

	// Make sure ball is not out of left bound.
	if (cg.breakout.ball.x - (BREAKOUT_BALL_WIDTH / 2) < BREAKOUT_BOARD_X)
	{
		// Adjust ball position.
		cg.breakout.ball.x = BREAKOUT_BOARD_X + (BREAKOUT_BALL_WIDTH / 2);

		// Reverse ball direction.
		cg.breakout.ball.v[0] *= -1;
	}

	// Make sure ball is not out of right bound.
	if (cg.breakout.ball.x + (BREAKOUT_BALL_WIDTH / 2) > BREAKOUT_BOARD_X + BREAKOUT_BOARD_WIDTH)
	{
		// Adjust ball position.
		cg.breakout.ball.x = BREAKOUT_BOARD_X + BREAKOUT_BOARD_WIDTH - (BREAKOUT_BALL_WIDTH / 2);

		// Reverse ball direction.
		cg.breakout.ball.v[0] *= -1;
	}

	// If ball hit the upper bound.
	if (cg.breakout.ball.y - (BREAKOUT_BALL_HEIGHT / 2) < BREAKOUT_BOARD_Y)
	{
		// Increase level number.
		cg.breakout.level++;

		// Reset bricks;
		breakout_ResetBricks();

		// Reset paddle.
		breakout_ResetPaddle();

		// Reset ball.
		breakout_ResetBall();

		// Small delay.
		cg.breakout.delay = cg.time + BREAKOUT_DELAY_TIME;

		return;
	}

	// If ball is out of lower bound.
	if (cg.breakout.ball.y + (BREAKOUT_BALL_HEIGHT / 2) > BREAKOUT_PADDLE_Y - (BREAKOUT_PADDLE_HEIGHT / 2))
	{
		// If ball hit the paddle.
		if ((cg.breakout.ball.x - (BREAKOUT_BALL_WIDTH / 2) < cg.breakout.paddle.x + (BREAKOUT_PADDLE_WIDTH / 2)) &&
			(cg.breakout.ball.x + (BREAKOUT_BALL_WIDTH / 2) > cg.breakout.paddle.x - (BREAKOUT_PADDLE_WIDTH / 2)))
		{
			// Adjust ball position.
			cg.breakout.ball.y = BREAKOUT_PADDLE_Y - (BREAKOUT_PADDLE_HEIGHT / 2) - (BREAKOUT_BALL_HEIGHT / 2);

			// Reverse vertical ball directon.
			cg.breakout.ball.v[1] *= -1;
		}
		else
		{
			// Decrease lives number.
			cg.breakout.lives--;

			// If lives number is zero.
			if (!cg.breakout.lives)
			{
				// Game over.
				cg.breakout.gameover = qtrue;

				return;
			}
			else
			{
				// Reset paddle.
				breakout_ResetPaddle();

				// Reset ball.
				breakout_ResetBall();

				// Small delay.
				cg.breakout.delay = cg.time + BREAKOUT_DELAY_TIME;

				return;
			}
		}
	}

	// If ball is near the bricks.
	if ((cg.breakout.ball.y + (BREAKOUT_BALL_HEIGHT / 2) > BREAKOUT_BRICKS_Y) &&
		(cg.breakout.ball.y - (BREAKOUT_BALL_HEIGHT / 2) < BREAKOUT_BRICKS_Y + BREAKOUT_BRICKS_HEIGHT))
	{
		int  u, v;

		// Calculate ball position in the bricks matrix.
		u = (cg.breakout.ball.x - BREAKOUT_BRICKS_X) / BREAKOUT_BRICK_WIDTH;
		v = (cg.breakout.ball.y - BREAKOUT_BRICKS_Y) / BREAKOUT_BRICK_HEIGHT;

		// Adjust in matrix vertical ball position.
		if (cg.breakout.ball.y <= BREAKOUT_BRICKS_Y)
		{
			v = -1;
		}
		else if (cg.breakout.ball.y >= BREAKOUT_BRICKS_Y + BREAKOUT_BRICKS_HEIGHT)
		{
			v = BREAKOUT_BRICK_ROWS;
		}

		// If ball is moving up.
		if (cg.breakout.ball.v[1] < 0.0)
		{
			// If a brick was hit.
			if ((v > 0) && cg.breakout.bricks[v - 1][u] && (cg.breakout.ball.y - (BREAKOUT_BALL_HEIGHT / 2) < BREAKOUT_BRICKS_Y + (v * BREAKOUT_BRICK_HEIGHT)))
			{
				// Decrease brick life.
				cg.breakout.bricks[v - 1][u]--;

				// If brick is "dead".
				if (!cg.breakout.bricks[v - 1][u])
				{
					cg.breakout.bricksOut++;
				}

				// Adjust ball position.
				cg.breakout.ball.y = BREAKOUT_BRICKS_Y + (v * BREAKOUT_BRICK_HEIGHT) + (BREAKOUT_BALL_HEIGHT / 2);

				// Reverse ball direction.
				cg.breakout.ball.v[1] *= -1;
			}
		}
		else
		{
			// If a brick was hit.
			if ((v < BREAKOUT_BRICK_ROWS - 1) && cg.breakout.bricks[v + 1][u] && (cg.breakout.ball.y + (BREAKOUT_BALL_HEIGHT / 2) > BREAKOUT_BRICKS_Y + ((v + 1) * BREAKOUT_BRICK_HEIGHT)))
			{
				// Decrease brick life.
				cg.breakout.bricks[v + 1][u]--;

				// If brick is "dead".
				if (!cg.breakout.bricks[v + 1][u])
				{
					cg.breakout.bricksOut++;
				}

				// Adjust ball position.
				cg.breakout.ball.y = BREAKOUT_BRICKS_Y + ((v + 1) * BREAKOUT_BRICK_HEIGHT) - (BREAKOUT_BALL_HEIGHT / 2);

				// Reverse ball direction.
				cg.breakout.ball.v[1] *= -1;
			}
		}

		// Adjust in matrix vertical ball position.
		if (v == -1)
		{
			v = 0;
		}
		else if (v == BREAKOUT_BRICK_ROWS)
		{
			v = BREAKOUT_BRICK_ROWS - 1;
		}

		// If ball is moving left.
		if (cg.breakout.ball.v[0] < 0.0)
		{
			// If a brick was hit.
			if ((u > 0) && cg.breakout.bricks[v][u - 1] && (cg.breakout.ball.x - (BREAKOUT_BALL_WIDTH / 2) < BREAKOUT_BRICKS_X + (u * BREAKOUT_BRICK_WIDTH)))
			{
				// Decrease brick life.
				cg.breakout.bricks[v][u - 1]--;

				// If brick is "dead".
				if (!cg.breakout.bricks[v][u - 1])
				{
					cg.breakout.bricksOut++;
				}

				// Adjust ball position.
				cg.breakout.ball.x = BREAKOUT_BRICKS_X + (u * BREAKOUT_BRICK_WIDTH) + (BREAKOUT_BALL_WIDTH / 2);

				// Reverse ball direction.
				cg.breakout.ball.v[0] *= -1;
			}
		}
		else
		{
			// If a brick was hit.
			if ((u < BREAKOUT_BRICK_COLUMNS - 1) && cg.breakout.bricks[v][u + 1] && (cg.breakout.ball.x + (BREAKOUT_BALL_WIDTH / 2) > BREAKOUT_BRICKS_X + ((u + 1) * BREAKOUT_BRICK_WIDTH)))
			{
				// Decrease brick life.
				cg.breakout.bricks[v][u + 1]--;

				// If brick is "dead".
				if (!cg.breakout.bricks[v][u + 1])
				{
					cg.breakout.bricksOut++;
				}

				// Adjust ball position.
				cg.breakout.ball.x = BREAKOUT_BRICKS_X + ((u + 1) * BREAKOUT_BRICK_WIDTH) - (BREAKOUT_BALL_WIDTH / 2);

				// Reverse ball direction.
				cg.breakout.ball.v[0] *= -1;
			}
		}

		// If all bricks are "dead".
		if (cg.breakout.bricksOut == BREAKOUT_BRICK_COLUMNS * BREAKOUT_BRICK_ROWS)
		{
			// Increase level number.
			cg.breakout.level++;

			// Reset bricks;
			breakout_ResetBricks();

			// Reset paddle.
			breakout_ResetPaddle();

			// Reset ball.
			breakout_ResetBall();

			// Small delay.
			cg.breakout.delay = cg.time + BREAKOUT_DELAY_TIME;

			return;
		}
	}
}

/**
 * $(function)
 */
static void breakout_Update(void)
{
	float  deltaTime;

	// Calculate time delta.
	deltaTime = (float)(cg.time - cg.breakout.time) / 1000.0f;

	// Clamp time delta.
	if (deltaTime > BREAKOUT_MAX_FRAME_TIME)
	{
		deltaTime = (float)(BREAKOUT_MAX_FRAME_TIME);
	}

	// Update paddle.
	breakout_UpdatePaddle(deltaTime);

	// Update ball.
	breakout_UpdateBall(deltaTime);

	// Reset time.
	cg.breakout.time = cg.time;
}

/**
 * $(function)
 */
static void breakout_DrawBricks(void)
{
	int x, y;
	vec4_t	colourGreen[4] = {
		{ 0, 1, 0, 0.3f},
		{ 0, 1, 0, 0.6f},
		{ 0, 1, 0, 0.9f}
	};

	for (y = 0; y < BREAKOUT_BRICK_ROWS; y++)
	{
		for (x = 0; x < BREAKOUT_BRICK_COLUMNS; x++)
		{
			// If brick exists.
			if (cg.breakout.bricks[y][x])
			{
				// Draw brick.
				CG_FillRect(BREAKOUT_BRICKS_X + (x * BREAKOUT_BRICK_WIDTH), BREAKOUT_BRICKS_Y + (y * BREAKOUT_BRICK_HEIGHT),
						BREAKOUT_BRICK_WIDTH, BREAKOUT_BRICK_HEIGHT, colourGreen[cg.breakout.bricks[y][x] - 1]);
			}
		}
	}
}

/**
 * $(function)
 */
static void breakout_DrawPaddle(void)
{
	vec4_t	colourGreen[4] = {
		{ 0, 1, 0, 1.0f},
		{ 0, 1, 0, 0.3f},
		{ 0, 1, 0, 0.2f},
		{ 0, 1, 0, 0.1f}
	};

	// If paddle is not moving.
	if (cg.breakout.paddle.v == 0.0)
	{
		// Draw the paddle.
		CG_FillRect(cg.breakout.paddle.x - (BREAKOUT_PADDLE_WIDTH / 2), BREAKOUT_PADDLE_Y - (BREAKOUT_PADDLE_HEIGHT / 2),
				BREAKOUT_PADDLE_WIDTH, BREAKOUT_PADDLE_HEIGHT, colourGreen[0]);
	}
	else
	{
		int    i;
		float  u;

		// Draw paddle and motion trail.
		for (i = 3; i > -1; i--)
		{
			// Calculate position offset.
			u = (float)i * cg.breakout.paddle.v * BREAKOUT_PADDLE_WIDTH * 0.07f;

			// Draw the paddle.
			CG_FillRect(cg.breakout.paddle.x - u - (BREAKOUT_PADDLE_WIDTH / 2), BREAKOUT_PADDLE_Y - (BREAKOUT_PADDLE_HEIGHT / 2),
					BREAKOUT_PADDLE_WIDTH, BREAKOUT_PADDLE_HEIGHT, colourGreen[i]);
		}
	}
}

/**
 * $(function)
 */
static void breakout_DrawBall(void)
{
	vec4_t	colourGreen[4] = {
		{ 0, 1, 0, 1.0f},
		{ 0, 1, 0, 0.3f},
		{ 0, 1, 0, 0.2f},
		{ 0, 1, 0, 0.1f}
	};

	// If we're in a delay.
	if (cg.time <= cg.breakout.delay)
	{
		// Draw the ball.
		CG_FillRect(cg.breakout.ball.x - (BREAKOUT_BALL_WIDTH / 2), cg.breakout.ball.y - (BREAKOUT_BALL_HEIGHT / 2),
				BREAKOUT_BALL_WIDTH, BREAKOUT_BALL_HEIGHT, colourGreen[0]);
	}
	else
	{
		int    i;
		float  u, v;

		// Draw ball and motion trail.
		for (i = 3; i > -1; i--)
		{
			// Calculate position offset.
			u = (float)i * cg.breakout.ball.v[0] * BREAKOUT_BALL_WIDTH * 0.3f;
			v = (float)i * cg.breakout.ball.v[1] * BREAKOUT_BALL_HEIGHT * 0.3f;

			// Draw the ball.
			CG_FillRect(cg.breakout.ball.x - u - (BREAKOUT_BALL_WIDTH / 2), cg.breakout.ball.y - v - (BREAKOUT_BALL_HEIGHT / 2),
					BREAKOUT_BALL_WIDTH, BREAKOUT_BALL_HEIGHT, colourGreen[i]);
		}
	}
}

/**
 * $(function)
 */
static void breakout_Draw(void)
{
	vec4_t	colourGrey	= { 0, 0, 0, 0.5f};
	vec4_t	colourGreen = { 0, 1, 0, 1};

	// Dim screen.
	CG_FillRect(0, 0, 640, 480, colourGrey);

	// Draw breakout text.
	CG_DrawStringExt(BREAKOUT_TEXT_X - ((BREAKOUT_BIG_CHAR_WIDTH * CG_DrawStrlen("BREAKOUT")) / 2), BREAKOUT_TEXT_Y,
			 "BREAKOUT", colourGreen, qtrue, qfalse, BREAKOUT_BIG_CHAR_WIDTH, BREAKOUT_BIG_CHAR_HEIGHT, 0);

	// Draw status text.
	if (cg.breakout.gameover)
	{
		CG_DrawStringExt(BREAKOUT_STATUS_TEXT_X - ((BREAKOUT_SMALL_CHAR_WIDTH * CG_DrawStrlen("GAME OVER")) / 2),
				 BREAKOUT_STATUS_TEXT_Y, "GAME OVER", colourGreen, qtrue, qfalse, BREAKOUT_SMALL_CHAR_WIDTH, BREAKOUT_SMALL_CHAR_HEIGHT, 0);
	}
	else if (cg.breakout.pause)
	{
		CG_DrawStringExt(BREAKOUT_STATUS_TEXT_X - ((BREAKOUT_SMALL_CHAR_WIDTH * CG_DrawStrlen("PAUSE")) / 2),
				 BREAKOUT_STATUS_TEXT_Y, "PAUSE", colourGreen, qtrue, qfalse, BREAKOUT_SMALL_CHAR_WIDTH, BREAKOUT_SMALL_CHAR_HEIGHT, 0);
	}

	// Draw level text.
	CG_DrawStringExt(BREAKOUT_LEVEL_TEXT_X, BREAKOUT_LEVEL_TEXT_Y, va("LEVEL:%d", cg.breakout.level),
			 colourGreen, qtrue, qfalse, BREAKOUT_SMALL_CHAR_WIDTH, BREAKOUT_SMALL_CHAR_HEIGHT, 0);

	// Draw lives text.
	CG_DrawStringExt(BREAKOUT_LIVES_TEXT_X - (BREAKOUT_SMALL_CHAR_WIDTH * CG_DrawStrlen(va("LIVES:%d", cg.breakout.lives))),
			 BREAKOUT_LIVES_TEXT_Y, va("LIVES:%d", cg.breakout.lives), colourGreen, qtrue, qfalse,
			 BREAKOUT_SMALL_CHAR_WIDTH, BREAKOUT_SMALL_CHAR_HEIGHT, 0);

	// Draw game board.
	CG_FillRect(BREAKOUT_BOARD_X - BREAKOUT_BORDER_WIDTH, BREAKOUT_BOARD_Y - BREAKOUT_BORDER_WIDTH,
			BREAKOUT_BORDER_WIDTH, BREAKOUT_BOARD_HEIGHT + (BREAKOUT_BORDER_WIDTH * 2), colourGreen);
	CG_FillRect(BREAKOUT_BOARD_X + BREAKOUT_BOARD_WIDTH, BREAKOUT_BOARD_Y - BREAKOUT_BORDER_WIDTH,
			BREAKOUT_BORDER_WIDTH, BREAKOUT_BOARD_HEIGHT + (BREAKOUT_BORDER_WIDTH * 2), colourGreen);

	// Draw bricks.
	breakout_DrawBricks();

	// Draw paddle.
	breakout_DrawPaddle();

	// Draw ball.
	breakout_DrawBall();
}

/**
 * $(function)
 */
void breakout_DoFrame(void)
{
	// If we're not playing.
	if (!cg.breakout.play)
	{
		// Reset time.
		cg.breakout.time = cg.time;

		return;
	}

	// If we need to reset.
	if (!cg.breakout.initialized || cg.breakout.reset)
	{
		breakout_Reset();
	}

	// If we're paused or we need to delay.
	if (cg.breakout.pause || cg.breakout.gameover || (cg.time < cg.breakout.delay))
	{
		// Reset time.
		cg.breakout.time = cg.time;
	}
	else
	{
		// Update game.
		breakout_Update();
	}

	// Draw everything.
	breakout_Draw();
}

/////////// SNAKE ////////////

#define SNAKE_UPDATE_TIME	200
#define SNAKE_UPDATE_MIN_TIME	50

#define SNAKE_BORDER_WIDTH	1

#define SNAKE_SMALL_CHAR_WIDTH	16
#define SNAKE_SMALL_CHAR_HEIGHT SNAKE_SMALL_CHAR_WIDTH * (4 / 3)

#define SNAKE_BIG_CHAR_WIDTH	20
#define SNAKE_BIG_CHAR_HEIGHT	SNAKE_BIG_CHAR_WIDTH * (4 / 3)

#define SNAKE_TEXT_X		320
#define SNAKE_TEXT_Y		20

#define SNAKE_STATUS_TEXT_X 320
#define SNAKE_STATUS_TEXT_Y 50

#define SNAKE_LEVEL_TEXT_X	145
#define SNAKE_LEVEL_TEXT_Y	70

#define SNAKE_LIVES_TEXT_X	495
#define SNAKE_LIVES_TEXT_Y	70

#define SNAKE_BOARD_X		145
#define SNAKE_BOARD_Y		90
#define SNAKE_BOARD_WIDTH	350
#define SNAKE_BOARD_HEIGHT	350

#define SNAKE_CELLS_X		145
#define SNAKE_CELLS_Y		90
#define SNAKE_CELL_WIDTH	10
#define SNAKE_CELL_HEIGHT	10

#define SNAKE_MAX_LIVES 	3

#define SNAKE_DELAY_TIME	1000

/**
 * $(function)
 */
static void snake_ResetCells(void)
{
	int  xs, ys, xw, yw;
	int  x, y;
	int  i, l;

	// Initialize cells.
	for (y = 0; y < SNAKE_CELL_ROWS; y++)
	{
		for (x = 0; x < SNAKE_CELL_COLUMNS; x++)
		{
			cg.snake.cells[y][x] = 0;
		}
	}

	// Calculate number of osbstructions.
	l = rand() & cg.snake.level;

	// Create obstructions.
	for (i = 0; i < l; i++)
	{
		// Calculate position.
		xs = 1 + (rand() % (SNAKE_CELL_COLUMNS - 2));
		ys = 1 + (rand() % (SNAKE_CELL_ROWS - 2));

		// Calculate dimensions.
		xw = 1 + (rand() % SNAKE_CELL_COLUMNS);
		yw = 1 + (rand() % SNAKE_CELL_ROWS);

		if (xw < yw)
		{
			xw = 1;
		}
		else
		{
			yw = 1;
		}

		// Clamp.
		if ((xs + xw) > (SNAKE_CELL_COLUMNS - 1))
		{
			xw = (SNAKE_CELL_COLUMNS - 1) - xs;
		}

		if ((ys + yw) > (SNAKE_CELL_ROWS - 2))
		{
			yw = (SNAKE_CELL_ROWS - 2) - ys;
		}

		// Create obstruction.
		for (y = ys; y < (ys + yw); y++)
		{
			for (x = xs; x < (xs + xw); x++)
			{
				cg.snake.cells[y][x] = 1;
			}
		}
	}

	// Create apples.
	for (i = 0; i < cg.snake.apples; i++)
	{
		// Calculate position.
		while (1) // pwned if apples is > SNAKE_CELL_COLUMNS * SNAKE_CELL_ROWS
		{
			x = rand() % SNAKE_CELL_COLUMNS;
			y = rand() % SNAKE_CELL_ROWS;

			// If this cell is not taken.
			if (cg.snake.cells[y][x] != 2)
			{
				break;
			}
		}

		// Set apple.
		cg.snake.cells[y][x] = 2;

		// Clear obstructions.
		xs = x - 1;
		ys = y - 1;

		// Clamp.
		if (xs < 0)
		{
			xs = 0;
		}

		if (ys < 0)
		{
			ys = 0;
		}

		xw = x + 2;
		yw = y + 2;

		// Clamp.
		if (xw > (SNAKE_CELL_COLUMNS - 1))
		{
			xw = SNAKE_CELL_COLUMNS - 1;
		}

		if (yw > (SNAKE_CELL_ROWS - 1))
		{
			yw = SNAKE_CELL_ROWS - 1;
		}

		for (y = 0; y < SNAKE_CELL_ROWS; y++)
		{
			for (x = xs; x < xw; x++)
			{
				if (cg.snake.cells[y][x] == 1)
				{
					cg.snake.cells[y][x] = 0;
				}
			}
		}

		for (y = ys; y < yw; y++)
		{
			for (x = 0; x < SNAKE_CELL_COLUMNS; x++)
			{
				if (cg.snake.cells[y][x] == 1)
				{
					cg.snake.cells[y][x] = 0;
				}
			}
		}
	}
}

/**
 * $(function)
 */
static void snake_ResetBody(void)
{
	int  i;

	for (i = 0; i < cg.snake.parts; i++)
	{
		cg.snake.body[i].s	  = 0;

		cg.snake.body[i].x	  = cg.snake.parts - i - 1;
		cg.snake.body[i].y	  = SNAKE_CELL_ROWS - 1;

		cg.snake.body[i].v[0] = 1;
		cg.snake.body[i].v[1] = 0;
	}

	if ((cg.snake.cells[cg.snake.body[0].y][cg.snake.body[0].x] == 1) || (cg.snake.cells[cg.snake.body[0].y][cg.snake.body[0].x] == 3))
	{
		cg.snake.cells[cg.snake.body[0].y][cg.snake.body[0].x] = 0;
	}

	if (cg.snake.cells[cg.snake.body[0].y][cg.snake.body[0].x] == 2)
	{
		cg.snake.cells[cg.snake.body[0].y][cg.snake.body[0].x]	   = 0;
		cg.snake.cells[cg.snake.body[0].y][cg.snake.body[0].x + 1] = 2;
	}
}

/**
 * $(function)
 */
static void snake_Reset(void)
{
	// Reset state.
	cg.snake.initialized = qtrue;
	cg.snake.reset		 = qfalse;
	cg.snake.pause		 = qfalse;
	cg.snake.gameover	 = qfalse;

	// Reset time.
	cg.snake.time  = cg.time;
	cg.snake.delay = 0;

	// Reset status.
	cg.snake.level	= 1;
	cg.snake.lives	= SNAKE_MAX_LIVES;
	cg.snake.apples = 1;

	// Reset cells.
	snake_ResetCells();

	// Reset body.
	cg.snake.parts = 1;
	snake_ResetBody();
}

/**
 * $(function)
 */
static void snake_UpdateBody(void)
{
	int  x, y;
	int  v[2];
	int  i;

	// Check for out of bounds.
	if (((cg.snake.body[0].y == 0) && (cg.snake.body[0].v[1] == -1)) || ((cg.snake.body[0].y == SNAKE_CELL_ROWS - 1) && (cg.snake.body[0].v[1] == 1)) ||
		((cg.snake.body[0].x == 0) && (cg.snake.body[0].v[0] == -1)) || ((cg.snake.body[0].x == SNAKE_CELL_COLUMNS - 1) && (cg.snake.body[0].v[0] == 1)))
	{
		// Decrease lives number.
		cg.snake.lives--;

		// If lives number is zero.
		if (!cg.snake.lives)
		{
			// Game over.
			cg.snake.gameover = qtrue;
		}
		else
		{
			// Reset body.
			snake_ResetBody();

			// Small delay.
			cg.snake.delay = cg.time + SNAKE_DELAY_TIME;
		}

		return;
	}

	// Backup last body part position.
	x = cg.snake.body[cg.snake.parts - 1].x;
	y = cg.snake.body[cg.snake.parts - 1].y;

	// Backup last body part vector.
	v[0] = cg.snake.body[cg.snake.parts - 1].v[0];
	v[1] = cg.snake.body[cg.snake.parts - 1].v[1];

	// Move body.
	for (i = 0; i < cg.snake.parts; i++)
	{
		cg.snake.body[i].x += cg.snake.body[i].v[0];
		cg.snake.body[i].y += cg.snake.body[i].v[1];
	}

	// Check for body collision.
	for (i = 1; i < cg.snake.parts; i++)
	{
		if ((cg.snake.body[0].x == cg.snake.body[i].x) && (cg.snake.body[0].y == cg.snake.body[i].y))
		{
			// Decrease lives number.
			cg.snake.lives--;

			// If lives number is zero.
			if (!cg.snake.lives)
			{
				// Game over.
				cg.snake.gameover = qtrue;
			}
			else
			{
				// Reset body.
				snake_ResetBody();

				// Small delay.
				cg.snake.delay = cg.time + SNAKE_DELAY_TIME;
			}

			return;
		}
	}

	// If body hit an obstruction.
	if ((cg.snake.cells[cg.snake.body[0].y][cg.snake.body[0].x] == 1) || (cg.snake.cells[cg.snake.body[0].y][cg.snake.body[0].x] == 3))
	{
		// Decrease lives number.
		cg.snake.lives--;

		// If lives number is zero.
		if (!cg.snake.lives)
		{
			// Game over.
			cg.snake.gameover = qtrue;
		}
		else
		{
			// Reset body.
			snake_ResetBody();

			// Small delay.
			cg.snake.delay = cg.time + SNAKE_DELAY_TIME;
		}

		return;
	}
	// If body hit an apple.
	else if (cg.snake.cells[cg.snake.body[0].y][cg.snake.body[0].x] == 2)
	{
		// Decrease apples number.
		cg.snake.apples--;

		// Remove apple.
		cg.snake.cells[cg.snake.body[0].y][cg.snake.body[0].x] = 0;

		// If we can add a body part.
		if (cg.snake.parts < (SNAKE_MAX_BODY_PARTS - 1))
		{
			// Add body part.
			cg.snake.body[cg.snake.parts].s    = 0;

			cg.snake.body[cg.snake.parts].x    = x;
			cg.snake.body[cg.snake.parts].y    = y;

			cg.snake.body[cg.snake.parts].v[0] = v[0];
			cg.snake.body[cg.snake.parts].v[1] = v[1];

			// Increase parts number.
			cg.snake.parts++;
		}
		else
		{
			// Add an obstruction.
			cg.snake.cells[y][x] = 3;
		}

		// If apples number if zero.
		if (!cg.snake.apples)
		{
			// Increase level number.
			cg.snake.level++;

			// Calculate apples number.
			cg.snake.apples = (int)sqrt(cg.snake.level);

			// Clamp.
			if (cg.snake.apples < 1)
			{
				cg.snake.apples = 1;
			}

			// Reset cells.
			snake_ResetCells();

			// Reset body.
			snake_ResetBody();

			// Small delay.
			cg.snake.delay = cg.time + SNAKE_DELAY_TIME;

			return;
		}
	}

	// Adjust body direction.
	for (i = cg.snake.parts - 1; i > 0; i--)
	{
		if (cg.snake.body[i - 1].s)
		{
			cg.snake.body[i].s	   = 1;

			cg.snake.body[i].v[0]  = cg.snake.body[i - 1].v[0];
			cg.snake.body[i].v[1]  = cg.snake.body[i - 1].v[1];

			cg.snake.body[i - 1].s = 0;
		}
	}
}

/**
 * $(function)
 */
static void snake_Update(void)
{
	float  delta;

	// Calculate time delta.
	delta = SNAKE_UPDATE_TIME * (1.0f - (((float)cg.snake.level - 1.0f) * 0.025f));

	// Clamp.
	if (delta < SNAKE_UPDATE_MIN_TIME)
	{
		delta = SNAKE_UPDATE_MIN_TIME;
	}

	// If it's not yet time to update.
	if ((float)(cg.time - cg.snake.time) < delta)
	{
		return;
	}

	// Update body.
	snake_UpdateBody();

	// Reset time.
	cg.snake.time = cg.time;
}

/**
 * $(function)
 */
static void snake_DrawCells(void)
{
	int x, y;
	vec4_t	colour[3] = {
		{ 0.0f, 1.0f, 0.0f, 0.9f},
		{ 1.0f, 0.0f, 0.0f, 0.9f},
		{ 0.8f, 0.5f, 0.0f, 0.9f}
	};

	// Draw cells.
	for (y = 0; y < SNAKE_CELL_ROWS; y++)
	{
		for (x = 0; x < SNAKE_CELL_COLUMNS; x++)
		{
			if (cg.snake.cells[y][x])
			{
				CG_FillRect((x * SNAKE_CELL_WIDTH) + SNAKE_CELLS_X, (y * SNAKE_CELL_HEIGHT) + SNAKE_CELLS_Y, SNAKE_CELL_WIDTH, SNAKE_CELL_HEIGHT, colour[cg.snake.cells[y][x] - 1]);
			}
		}
	}
}

/**
 * $(function)
 */
static void snake_DrawBody(void)
{
	int i;
	vec4_t	colour[2] = {
		{ 1.0f, 1.0f, 0.0f, 0.9f},
		{ 0.0f, 0.0f, 0.0f, 0.9f}
	};

	// Draw body.
	for (i = 0; i < cg.snake.parts; i++)
	{
		CG_FillRect((cg.snake.body[i].x * SNAKE_CELL_WIDTH) + SNAKE_CELLS_X, (cg.snake.body[i].y * SNAKE_CELL_HEIGHT) + SNAKE_CELLS_Y, SNAKE_CELL_WIDTH, SNAKE_CELL_HEIGHT, colour[0]);

		if (i == 0)
		{
			// If moving up or down.
			if (cg.snake.body[0].v[1] != 0)
			{
				CG_FillRect(
					(cg.snake.body[i].x * SNAKE_CELL_WIDTH) + SNAKE_CELLS_X + (SNAKE_CELL_WIDTH / 3),
					(cg.snake.body[i].y * SNAKE_CELL_HEIGHT) + SNAKE_CELLS_Y + (SNAKE_CELL_HEIGHT / 2),
					1, 1, colour[1]);

				CG_FillRect(
					(cg.snake.body[i].x * SNAKE_CELL_WIDTH) + SNAKE_CELLS_X + ((SNAKE_CELL_WIDTH / 3) * 2),
					(cg.snake.body[i].y * SNAKE_CELL_HEIGHT) + SNAKE_CELLS_Y + (SNAKE_CELL_HEIGHT / 2),
					1, 1, colour[1]);
			}
			// If moving left or right.
			else if (cg.snake.body[0].v[0] != 0)
			{
				CG_FillRect(
					(cg.snake.body[i].x * SNAKE_CELL_WIDTH) + SNAKE_CELLS_X + (SNAKE_CELL_WIDTH / 2),
					(cg.snake.body[i].y * SNAKE_CELL_HEIGHT) + SNAKE_CELLS_Y + (SNAKE_CELL_HEIGHT / 3),
					1, 1, colour[1]);

				CG_FillRect(
					(cg.snake.body[i].x * SNAKE_CELL_WIDTH) + SNAKE_CELLS_X + (SNAKE_CELL_WIDTH / 2),
					(cg.snake.body[i].y * SNAKE_CELL_HEIGHT) + SNAKE_CELLS_Y + ((SNAKE_CELL_HEIGHT / 3) * 2),
					1, 1, colour[1]);
			}
		}
	}
}

/**
 * $(function)
 */
static void snake_Draw(void)
{
	vec4_t	colourGrey	= { 0, 0, 0, 0.5f};
	vec4_t	colourGreen = { 0, 1, 0, 1};

	// Dim screen.
	CG_FillRect(0, 0, 640, 480, colourGrey);

	// Draw breakout text.
	CG_DrawStringExt(SNAKE_TEXT_X - ((SNAKE_BIG_CHAR_WIDTH * CG_DrawStrlen("SNAKE")) / 2), SNAKE_TEXT_Y,
			 "SNAKE", colourGreen, qtrue, qfalse, SNAKE_BIG_CHAR_WIDTH, SNAKE_BIG_CHAR_HEIGHT, 0);

	// Draw status text.
	if (cg.snake.gameover)
	{
		CG_DrawStringExt(SNAKE_STATUS_TEXT_X - ((SNAKE_SMALL_CHAR_WIDTH * CG_DrawStrlen("GAME OVER")) / 2),
				 SNAKE_STATUS_TEXT_Y, "GAME OVER", colourGreen, qtrue, qfalse, SNAKE_SMALL_CHAR_WIDTH, SNAKE_SMALL_CHAR_HEIGHT, 0);
	}
	else if (cg.snake.pause)
	{
		CG_DrawStringExt(SNAKE_STATUS_TEXT_X - ((SNAKE_SMALL_CHAR_WIDTH * CG_DrawStrlen("PAUSE")) / 2),
				 SNAKE_STATUS_TEXT_Y, "PAUSE", colourGreen, qtrue, qfalse, SNAKE_SMALL_CHAR_WIDTH, SNAKE_SMALL_CHAR_HEIGHT, 0);
	}

	// Draw level text.
	CG_DrawStringExt(SNAKE_LEVEL_TEXT_X, SNAKE_LEVEL_TEXT_Y, va("LEVEL:%d", cg.snake.level),
			 colourGreen, qtrue, qfalse, SNAKE_SMALL_CHAR_WIDTH, SNAKE_SMALL_CHAR_HEIGHT, 0);

	// Draw lives text.
	CG_DrawStringExt(SNAKE_LIVES_TEXT_X - (SNAKE_SMALL_CHAR_WIDTH * CG_DrawStrlen(va("LIVES:%d", cg.snake.lives))),
			 SNAKE_LIVES_TEXT_Y, va("LIVES:%d", cg.snake.lives), colourGreen, qtrue, qfalse,
			 SNAKE_SMALL_CHAR_WIDTH, SNAKE_SMALL_CHAR_HEIGHT, 0);

	// Draw game board.
	CG_FillRect(SNAKE_BOARD_X - SNAKE_BORDER_WIDTH, SNAKE_BOARD_Y - SNAKE_BORDER_WIDTH,
			SNAKE_BOARD_WIDTH + (SNAKE_BORDER_WIDTH * 2), SNAKE_BORDER_WIDTH, colourGreen);

	CG_FillRect(SNAKE_BOARD_X - SNAKE_BORDER_WIDTH, SNAKE_BOARD_Y + SNAKE_BOARD_HEIGHT,
			SNAKE_BOARD_WIDTH + (SNAKE_BORDER_WIDTH * 2), SNAKE_BORDER_WIDTH, colourGreen);

	CG_FillRect(SNAKE_BOARD_X - SNAKE_BORDER_WIDTH, SNAKE_BOARD_Y - SNAKE_BORDER_WIDTH,
			SNAKE_BORDER_WIDTH, SNAKE_BOARD_HEIGHT + (SNAKE_BORDER_WIDTH * 2), colourGreen);
	CG_FillRect(SNAKE_BOARD_X + SNAKE_BOARD_WIDTH, SNAKE_BOARD_Y - SNAKE_BORDER_WIDTH,
			SNAKE_BORDER_WIDTH, SNAKE_BOARD_HEIGHT + (SNAKE_BORDER_WIDTH * 2), colourGreen);

	// Draw cells.
	snake_DrawCells();

	// Draw body.
	snake_DrawBody();
}

/**
 * $(function)
 */
void snake_DoFrame(void)
{
	// If we're not playing.
	if (!cg.snake.play)
	{
		// Reset time.
		cg.snake.time = cg.time;

		return;
	}

	// If we need to reset.
	if (!cg.snake.initialized || cg.snake.reset)
	{
		snake_Reset();
	}

	// If we're paused or we need to delay.
	if (cg.snake.pause || cg.snake.gameover || (cg.time < cg.snake.delay))
	{
		// Reset time.
		cg.snake.time = cg.time;
	}
	else
	{
		// Update game.
		snake_Update();
	}

	// Draw everything.
	snake_Draw();
}
