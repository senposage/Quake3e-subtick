// Copyright (C) 1999-2000 Id Software, Inc.
//
// cg_info.c -- display information while data is being loading

#include "cg_local.h"

#define MAX_LOADING_PLAYER_ICONS 16
#define MAX_LOADING_ITEM_ICONS	 26

static int	  loadingPlayerIconCount;
static int	  loadingItemIconCount;
static qhandle_t  loadingPlayerIcons[MAX_LOADING_PLAYER_ICONS];
static qhandle_t  loadingItemIcons[MAX_LOADING_ITEM_ICONS];

#if 0
/*
===================
CG_DrawLoadingIcons
===================
*/
static void CG_DrawLoadingIcons( void )
{
	int  n;
	int  x, y;

	for(n = 0; n < loadingPlayerIconCount; n++)
	{
		x = 16 + n * 78;
		y = 324 - 40;
		CG_DrawPic( x, y, 64, 64, loadingPlayerIcons[n] );
	}

	for(n = 0; n < loadingItemIconCount; n++)
	{
		y = 400 - 40;

		if(n >= 13)
		{
			y += 40;
		}
		x = 16 + n % 13 * 48;
		CG_DrawPic( x, y, 32, 32, loadingItemIcons[n] );
	}
}
#endif

/*
======================
CG_LoadingString

======================
*/
#define MAXCOUNTER 49
/**
 * $(function)
 */
void CG_LoadingString( const char *s )
{
	float  a;
	int    b;

	cg.percentLoadedCounter++;
	a		 = ((float)cg.percentLoadedCounter / MAXCOUNTER) * 100.0f;
	b		 = a;
	cg.percentLoaded = b;

	Q_strncpyz( cg.infoScreenText, s, sizeof(cg.infoScreenText));

	trap_UpdateScreen();
}

/*
===================
CG_LoadingItem
===================
*/
void CG_LoadingItem( int itemNum )
{
	gitem_t  *item;

	item = &bg_itemlist[itemNum];

	if (item->icon && (loadingItemIconCount < MAX_LOADING_ITEM_ICONS))
	{
		loadingItemIcons[loadingItemIconCount++] = trap_R_RegisterShaderNoMip( item->icon );
	}

	if (item->giTag != UT_WP_GRENADE_FLASH)
	{
		CG_LoadingString( item->pickup_name );
	}
}

/*
===================
CG_LoadingClient
===================
*/
void CG_LoadingClient( int clientNum )
{
	const char  *info;
	char	    personality[MAX_QPATH];
	char	    iconName[MAX_QPATH];

	info = CG_ConfigString( CS_PLAYERS + clientNum );

	if (loadingPlayerIconCount < MAX_LOADING_PLAYER_ICONS)
	{
//		Q_strncpyz( model, Info_ValueForKey( info, "model" ), sizeof( model ) );
//		skin = Q_strrchr( model, '/' );
//		if ( skin ) {
//			*skin++ = '\0';
//		} else {
//			skin = "default";
//		}

//		Com_sprintf( iconName, MAX_QPATH, "models/players/%s/icon_%s.tga", model, skin );

		loadingPlayerIcons[loadingPlayerIconCount] = trap_R_RegisterShaderNoMip( iconName );

		if (!loadingPlayerIcons[loadingPlayerIconCount])
		{
			Com_sprintf( iconName, MAX_QPATH, "models/players/%s/icon_%s.tga", DEFAULT_MODEL, "default" );
			loadingPlayerIcons[loadingPlayerIconCount] = trap_R_RegisterShaderNoMip( iconName );
		}

		if (loadingPlayerIcons[loadingPlayerIconCount])
		{
			loadingPlayerIconCount++;
		}
	}

	Q_strncpyz( personality, Info_ValueForKey( info, "n" ), sizeof(personality));
	Q_CleanStr( personality );

	CG_LoadingString( personality );
}

/*
====================
CG_DrawInformation

Draw all the status / pacifier stuff during level loading
====================
*/

#define TINY_PROP_HEIGHT 18

/**
 * $(function)
 */
void CG_DrawInformation( void )
{
	const char  *s;
	const char  *info;
	const char  *sysInfo;
	int	    y;
	int	    value;
	qhandle_t   levelshot;
	qhandle_t   overlay;

//	qhandle_t	header;
//	qhandle_t	detail;
	char   buf[1024];
	float  x;

	info	  = CG_ConfigString( CS_SERVERINFO );
	sysInfo   = CG_ConfigString( CS_SYSTEMINFO );

	s	  = Info_ValueForKey( info, "mapname" );
	levelshot = trap_R_RegisterShaderNoMip( va( "levelshots/%s.tga", s ));

	if (!levelshot)
	{
		levelshot = trap_R_RegisterShaderNoMip( "menu/art/unknownmap" );
	}
	overlay = trap_R_RegisterShaderNoMip( "overlay" );

//	header = trap_R_RegisterShaderNoMip ( "ui/assets/utlogo.tga" );

	trap_R_SetColor( NULL );
	CG_DrawPic( 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, levelshot );

	x = ((float)SCREEN_WIDTH * (float)cg.percentLoaded) / 100.0f;
	CG_DrawPic( 0, 0, x, SCREEN_HEIGHT, overlay );

//	if ( header )
//	{
//		trap_R_SetColor( NULL );
//		CG_DrawPic ( 0, 0, 256, 64, header );
//	}

/* URBAN TERROR - Dont draw the icons any more
	// blend a detail texture over it
	detail = trap_R_RegisterShader( "levelShotDetail" );
	trap_R_DrawStretchPic( 0, 0, cgs.glconfig.vidWidth, cgs.glconfig.vidHeight, 0, 0, 2.5, 2, detail );

	// draw the icons of things as they are loaded
	CG_DrawLoadingIcons();
*/

	// the first 150 rows are reserved for the client connection
	// screen to write into
	if (cg.infoScreenText[0])
	{
		UI_DrawProportionalString( 10, 128 - 14, va("Loading... %s", cg.infoScreenText),
					   UI_LEFT | UI_UTTINYFONT | UI_DROPSHADOW, colorWhite );
	}
	else
	{
		UI_DrawProportionalString( 10, 128 - 14, "Awaiting snapshot...",
					   UI_LEFT | UI_UTTINYFONT | UI_DROPSHADOW, colorWhite );
	}

	// draw info string information

	y = 320 + 32;

	// don't print server lines if playing a local game
	trap_Cvar_VariableStringBuffer( "sv_running", buf, sizeof(buf));

	if (!atoi( buf ))
	{
		// server hostname
		Q_strncpyz(buf, Info_ValueForKey( info, "sv_hostname" ), 1024);
		Q_CleanStr(buf);
		UI_DrawProportionalString( 10, y, buf,
					   UI_LEFT | UI_UTTINYFONT | UI_DROPSHADOW, colorWhite );
		y += TINY_PROP_HEIGHT;

		// pure server
		s = Info_ValueForKey( sysInfo, "sv_pure" );

		if (s[0] == '1')
		{
			UI_DrawProportionalString( 10, y, "Pure Server",
						   UI_LEFT | UI_UTTINYFONT | UI_DROPSHADOW, colorWhite );
			y += TINY_PROP_HEIGHT;
		}

		// server-specific message of the day
		s = CG_ConfigString( CS_MOTD );

		if (s[0])
		{
			UI_DrawProportionalString( 10, y, s,
						   UI_LEFT | UI_UTTINYFONT | UI_DROPSHADOW, colorWhite );
			y += TINY_PROP_HEIGHT;
		}

		// some extra space after hostname and motd
		y += 7;
	}

	// map-specific message (long map name)
	s = CG_ConfigString( CS_MESSAGE );

	if (s[0])
	{
		UI_DrawProportionalString( 10, y, s,
					   UI_LEFT | UI_UTTINYFONT | UI_DROPSHADOW, colorWhite );
		y += TINY_PROP_HEIGHT;
	}

	// cheats warning
	s = Info_ValueForKey( sysInfo, "sv_cheats" );

	if (s[0] == '1')
	{
		cgs.cheats = qtrue;
		UI_DrawProportionalString( 10, y, "CHEATS ARE ENABLED",
					   UI_LEFT | UI_UTTINYFONT | UI_DROPSHADOW, colorWhite );
		y += TINY_PROP_HEIGHT;
	}

	// game type
	switch (cgs.gametype)
	{
		case GT_FFA:
			s = "Free For All";
			break;

		case GT_LASTMAN:
			s = "Last Man Standing";
			break;

		case GT_JUMP:
			s = "Jump";
			break;

		case GT_TEAM:
			s = "Team Deathmatch";
			break;

		case GT_CTF:
			s = "Capture The Flag";
			break;

		/*	case GT_UT_BTB:
				s = "Search and Destroy";
				break;
			case GT_UT_C4:
				s = "Infiltrate";
				break; */
		case GT_UT_ASSASIN:
			s = "Follow the Leader";
			break;

		case GT_UT_SURVIVOR:
			s = "Team Survivor";
			break;

		case GT_UT_CAH:
			s = "Capture and Hold";
			break;

		case GT_UT_BOMB:
			s = "Bomb Mode";
			break;

		default:
			s = "Unknown Gametype";
			break;
	}

	UI_DrawProportionalString( 10, y, s,
				   UI_LEFT | UI_UTTINYFONT | UI_DROPSHADOW, colorWhite );
	y    += TINY_PROP_HEIGHT;

	value = atoi( Info_ValueForKey( info, "g_maxrounds" ));

	if (value && !cgs.survivor)
	{
		UI_DrawProportionalString( 10, y, va( "roundlimit %i", value ),
					   UI_LEFT | UI_UTTINYFONT | UI_DROPSHADOW, colorWhite );
		y += TINY_PROP_HEIGHT;
	}
	else
	{
		value = atoi( Info_ValueForKey( info, "timelimit" ));

		if (value)
		{
			UI_DrawProportionalString( 10, y, va( "timelimit %i", value ),
						   UI_LEFT | UI_UTTINYFONT | UI_DROPSHADOW, colorWhite );
			y += TINY_PROP_HEIGHT;
		}
	}

	value = atoi( Info_ValueForKey( info, "g_matchmode" ));

	if (value)
	{
		UI_DrawProportionalString( 10, y, "matchmode",
					   UI_LEFT | UI_UTTINYFONT | UI_DROPSHADOW, colorWhite );
		y += TINY_PROP_HEIGHT;
	}

	if (cgs.gametype <= GT_UT_ASSASIN)
	{
		value = atoi( Info_ValueForKey( info, "fraglimit" ));

		if (value)
		{
			UI_DrawProportionalString( 10, y, va( "fraglimit %i", value ),
						   UI_LEFT | UI_UTTINYFONT | UI_DROPSHADOW, colorWhite );
			y += TINY_PROP_HEIGHT;
		}
	}

	// DensitY: fixed for Bomb mode now, only need this in CTF :)
	if(cgs.gametype == GT_CTF)
	{
		value = atoi( Info_ValueForKey( info, "capturelimit" ));

		if (value)
		{
			UI_DrawProportionalString( 10, y, va( "capturelimit %i", value ),
						   UI_LEFT | UI_UTTINYFONT | UI_DROPSHADOW, colorWhite );
			y += TINY_PROP_HEIGHT;
		}
	}

	value = atoi( Info_ValueForKey( info, "g_gear" ));

	if (value > 0)
	{
		y  = TINY_PROP_HEIGHT;
		UI_DrawProportionalString( 10, y, va( "NADES: %s",
			(value & GEAR_NADES) ? "0" : "1"),
			UI_LEFT | UI_UTTINYFONT | UI_DROPSHADOW, colorWhite);
		y += TINY_PROP_HEIGHT;
		UI_DrawProportionalString( 10, y, va( "SNIPERS: %s",
			(value & GEAR_SNIPERS) ? "0" : "1"),
			UI_LEFT | UI_UTTINYFONT | UI_DROPSHADOW, colorWhite);
		y += TINY_PROP_HEIGHT;
		UI_DrawProportionalString( 10, y, va( "SPAS: %s",
			(value & GEAR_SPAS) ? "0" : "1"),
			UI_LEFT | UI_UTTINYFONT | UI_DROPSHADOW, colorWhite);
		y  = TINY_PROP_HEIGHT;
		UI_DrawProportionalString( 200, y, va( "PISTOLS: %s",
			(value & GEAR_PISTOLS) ? "0" : "1"),
			UI_LEFT | UI_UTTINYFONT | UI_DROPSHADOW, colorWhite);
		y += TINY_PROP_HEIGHT;
		UI_DrawProportionalString( 200, y, va( "AUTOS: %s",
			(value & GEAR_AUTOS) ? "0" : "1"),
			UI_LEFT | UI_UTTINYFONT | UI_DROPSHADOW, colorWhite);
		y += TINY_PROP_HEIGHT;
		UI_DrawProportionalString( 200, y, va( "NEGEV: %s",
			(value & GEAR_NEGEV) ? "0" : "1"),
			UI_LEFT | UI_UTTINYFONT | UI_DROPSHADOW, colorWhite);
		y += TINY_PROP_HEIGHT;
#ifdef FLASHNADES
		UI_DrawProportionalString( 200, y, va( "FLASH: %s",
			(value & GEAR_FLASH) ? "0" : "1"),
			UI_LEFT | UI_UTTINYFONT | UI_DROPSHADOW, colorWhite);
		y += TINY_PROP_HEIGHT;
#endif
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name       : CG_GetColoredClientName
// Description: Given the client info gets a colored version of the name
// Author     : Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
const char *CG_GetColoredClientName ( clientInfo_t *ci )
{
	char  *color = NULL;

	switch (ci->team)
	{
		case TEAM_RED:
		case TEAM_BLUE:
			color = skinInfos[CG_WhichSkin(ci->team)].ui_text_color;
			break;

		default:
			color = S_COLOR_YELLOW;
			break;
	}

	return va("%s%s%s", color, ci->name, S_COLOR_WHITE );
}
