/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// cl_scrn.c -- master for refresh, status bar, console, chat, notify, etc

#include "client.h"

qboolean	scr_initialized;		// ready to draw

cvar_t		*cl_timegraph;
cvar_t		*cl_debuggraph;
cvar_t		*cl_graphheight;
cvar_t		*cl_graphscale;
cvar_t		*cl_graphshift;

// Net monitor widget cvars
cvar_t		*cl_netgraph;
cvar_t		*cl_netgraph_x;
cvar_t		*cl_netgraph_y;
cvar_t		*cl_netgraph_scale;
cvar_t		*cl_netlog;

// Net monitor rate tracking (updated per second)
static int	netMonInBytes;
static int	netMonOutBytes;
static int	netMonInRate;
static int	netMonOutRate;
static int	netMonDropsWindow;
static int	netMonDropRate;
static int	netMonLastUpdate;

// Frame-time and snap-jitter tracking (reset each second)
static int	netMonFtSum;
static int	netMonFtCount;
static int	netMonFtMin;
static int	netMonFtMax;
static qboolean	netMonFtValid;
static int	netMonSnapGapSum;
static int	netMonSnapGapCount;
static int	netMonSnapGapMax;

// Cap-hit, extrap, and serverTimeDelta range tracking (reset each second)
static int	netMonCapHits;
static int	netMonExtrapCount;
static int	netMonDtMin;
static int	netMonDtMax;
static qboolean	netMonDtValid;

// Ping jitter tracking (reset each second)
static int	netMonPingSum;
static int	netMonPingCount;
static int	netMonPingMin;
static int	netMonPingMax;
static qboolean	netMonPingValid;

// FAST/RESET adjustment counts (reset each second; counted regardless of log level)
static int	netMonFastCount;
static int	netMonResetCount;
// Slow-path ms-commit count (reset each second).
// With the fractional-accumulator fix, this is 0 at 60Hz equilibrium — a non-zero
// value here means serverTimeDelta is genuinely drifting (expected after late packets)
// or, if combined with PING JITTER events, that the oscillation issue has recurred.
static int	netMonSlowCount;

// Session log file (opened lazily when cl_netlog > 0)
static fileHandle_t	netLogFile;

/*
================
SCR_DrawNamedPic

Coordinates are 640*480 virtual values
=================
*/
void SCR_DrawNamedPic( float x, float y, float width, float height, const char *picname ) {
	qhandle_t	hShader;

	assert( width != 0 );

	hShader = re.RegisterShader( picname );
	SCR_AdjustFrom640( &x, &y, &width, &height );
	re.DrawStretchPic( x, y, width, height, 0, 0, 1, 1, hShader );
}


/*
================
SCR_AdjustFrom640

Adjusted for resolution and screen aspect ratio
================
*/
void SCR_AdjustFrom640( float *x, float *y, float *w, float *h ) {
	float	xscale;
	float	yscale;

#if 0
		// adjust for wide screens
		if ( cls.glconfig.vidWidth * 480 > cls.glconfig.vidHeight * 640 ) {
			*x += 0.5 * ( cls.glconfig.vidWidth - ( cls.glconfig.vidHeight * 640 / 480 ) );
		}
#endif

	// scale for screen sizes
	xscale = cls.glconfig.vidWidth / 640.0;
	yscale = cls.glconfig.vidHeight / 480.0;
	if ( x ) {
		*x *= xscale;
	}
	if ( y ) {
		*y *= yscale;
	}
	if ( w ) {
		*w *= xscale;
	}
	if ( h ) {
		*h *= yscale;
	}
}

/*
================
SCR_FillRect

Coordinates are 640*480 virtual values
=================
*/
void SCR_FillRect( float x, float y, float width, float height, const float *color ) {
	re.SetColor( color );

	SCR_AdjustFrom640( &x, &y, &width, &height );
	re.DrawStretchPic( x, y, width, height, 0, 0, 0, 0, cls.whiteShader );

	re.SetColor( NULL );
}


/*
================
SCR_DrawPic

Coordinates are 640*480 virtual values
=================
*/
void SCR_DrawPic( float x, float y, float width, float height, qhandle_t hShader ) {
	SCR_AdjustFrom640( &x, &y, &width, &height );
	re.DrawStretchPic( x, y, width, height, 0, 0, 1, 1, hShader );
}


/*
** SCR_DrawChar
** chars are drawn at 640*480 virtual screen size
*/
static void SCR_DrawChar( int x, int y, float size, int ch ) {
	int row, col;
	float frow, fcol;
	float	ax, ay, aw, ah;

	ch &= 255;

	if ( ch == ' ' ) {
		return;
	}

	if ( y < -size ) {
		return;
	}

	ax = x;
	ay = y;
	aw = size;
	ah = size;
	SCR_AdjustFrom640( &ax, &ay, &aw, &ah );

	row = ch>>4;
	col = ch&15;

	frow = row*0.0625;
	fcol = col*0.0625;
	size = 0.0625;

	re.DrawStretchPic( ax, ay, aw, ah,
					   fcol, frow, 
					   fcol + size, frow + size, 
					   cls.charSetShader );
}


/*
** SCR_DrawSmallChar
** small chars are drawn at native screen resolution
*/
void SCR_DrawSmallChar( int x, int y, int ch ) {
	int row, col;
	float frow, fcol;
	float size;

	ch &= 255;

	if ( ch == ' ' ) {
		return;
	}

	if ( y < -smallchar_height ) {
		return;
	}

	row = ch>>4;
	col = ch&15;

	frow = row*0.0625;
	fcol = col*0.0625;
	size = 0.0625;

	re.DrawStretchPic( x, y, smallchar_width, smallchar_height,
					   fcol, frow, 
					   fcol + size, frow + size, 
					   cls.charSetShader );
}


/*
** SCR_DrawSmallString
** small string are drawn at native screen resolution
*/
void SCR_DrawSmallString( int x, int y, const char *s, int len ) {
	int row, col, ch, i;
	float frow, fcol;
	float size;

	if ( y < -smallchar_height ) {
		return;
	}

	size = 0.0625;

	for ( i = 0; i < len; i++ ) {
		ch = *s++ & 255;
		row = ch>>4;
		col = ch&15;

		frow = row*0.0625;
		fcol = col*0.0625;

		re.DrawStretchPic( x, y, smallchar_width, smallchar_height,
						   fcol, frow, fcol + size, frow + size, 
						   cls.charSetShader );

		x += smallchar_width;
	}
}


/*
==================
SCR_DrawBigString[Color]

Draws a multi-colored string with a drop shadow, optionally forcing
to a fixed color.

Coordinates are at 640 by 480 virtual resolution
==================
*/
void SCR_DrawStringExt( int x, int y, float size, const char *string, const float *setColor, qboolean forceColor,
		qboolean noColorEscape ) {
	vec4_t		color;
	const char	*s;
	int			xx;

	// draw the drop shadow
	color[0] = color[1] = color[2] = 0.0;
	color[3] = setColor[3];
	re.SetColor( color );
	s = string;
	xx = x;
	while ( *s ) {
		if ( !noColorEscape && Q_IsColorString( s ) ) {
			s += 2;
			continue;
		}
		SCR_DrawChar( xx+2, y+2, size, *s );
		xx += size;
		s++;
	}


	// draw the colored text
	s = string;
	xx = x;
	re.SetColor( setColor );
	while ( *s ) {
		if ( Q_IsColorString( s ) ) {
			if ( !forceColor ) {
				Com_Memcpy( color, g_color_table[ ColorIndexFromChar( *(s+1) ) ], sizeof( color ) );
				color[3] = setColor[3];
				re.SetColor( color );
			}
			if ( !noColorEscape ) {
				s += 2;
				continue;
			}
		}
		SCR_DrawChar( xx, y, size, *s );
		xx += size;
		s++;
	}
	re.SetColor( NULL );
}


/*
==================
SCR_DrawBigString
==================
*/
void SCR_DrawBigString( int x, int y, const char *s, float alpha, qboolean noColorEscape ) {
	float	color[4];

	color[0] = color[1] = color[2] = 1.0;
	color[3] = alpha;
	SCR_DrawStringExt( x, y, BIGCHAR_WIDTH, s, color, qfalse, noColorEscape );
}


/*
==================
SCR_DrawSmallString[Color]

Draws a multi-colored string with a drop shadow, optionally forcing
to a fixed color.
==================
*/
void SCR_DrawSmallStringExt( int x, int y, const char *string, const float *setColor, qboolean forceColor,
		qboolean noColorEscape ) {
	vec4_t		color;
	const char	*s;
	int			xx;

	// draw the colored text
	s = string;
	xx = x;
	re.SetColor( setColor );
	while ( *s ) {
		if ( Q_IsColorString( s ) ) {
			if ( !forceColor ) {
				Com_Memcpy( color, g_color_table[ ColorIndexFromChar( *(s+1) ) ], sizeof( color ) );
				color[3] = setColor[3];
				re.SetColor( color );
			}
			if ( !noColorEscape ) {
				s += 2;
				continue;
			}
		}
		SCR_DrawSmallChar( xx, y, *s );
		xx += smallchar_width;
		s++;
	}
	re.SetColor( NULL );
}


/*
** SCR_Strlen -- skips color escape codes
*/
static int SCR_Strlen( const char *str ) {
	const char *s = str;
	int count = 0;

	while ( *s ) {
		if ( Q_IsColorString( s ) ) {
			s += 2;
		} else {
			count++;
			s++;
		}
	}

	return count;
}


/*
** SCR_GetBigStringWidth
*/ 
int SCR_GetBigStringWidth( const char *str ) {
	return SCR_Strlen( str ) * BIGCHAR_WIDTH;
}

int SCR_FontWidth(const char* text, float scale) {
	if (!cls.fontFont)
		return 0;

	int 		 count, len;
	float		 out;
	glyphInfo_t* glyph;
	float		 useScale;
	const char* s = text;
	fontInfo_t* font = &cls.font;

	useScale = scale * font->glyphScale;
	out = 0;

	if (text) {
		len = strlen(text);
		count = 0;

		while (s && *s && count < len) {
			if (Q_IsColorString(s)) {
				s += 2;
				continue;
			}

			glyph = &font->glyphs[(int)*s];
			out += glyph->xSkip;
			s++;
			count++;
		}
	}
	return out * useScale;
}

void SCR_DrawFontChar(float x, float y, float width, float height, float scale, float s, float t, float s2, float t2, qhandle_t hShader) {
	if (!cls.fontFont)
		return;

	float  w, h;

	w = width * scale;
	h = height * scale;
	SCR_AdjustFrom640(&x, &y, &w, &h);
	re.DrawStretchPic(x, y, w, h, s, t, s2, t2, hShader);
}

void SCR_DrawFontText(float x, float y, float scale, vec4_t color, const char* text, int style) {
	if (!cls.fontFont)
		return;

	int 	 len, count;
	vec4_t		 newColor;
	vec4_t		 black = { 0.0f, 0.0f, 0.0f, 1.0f };
	vec4_t       grey = { 0.2f, 0.2f, 0.2f, 1.0f };
	glyphInfo_t* glyph;
	float		 useScale;
	fontInfo_t* font = &cls.font;

	useScale = scale * font->glyphScale;

	if (text) {
		const char* s = text;
		re.SetColor(color);
		memcpy(&newColor[0], &color[0], sizeof(vec4_t));
		len = strlen(text);

		count = 0;

		while (s && *s && count < len) {
			glyph = &font->glyphs[(int)*s];

			if (Q_IsColorString(s)) {
				memcpy(newColor, g_color_table[ColorIndex(*(s + 1))], sizeof(newColor));
				newColor[3] = color[3];
				re.SetColor(newColor);
				s += 2;
				continue;
			}

			float  yadj = useScale * glyph->top;

			if ((style == ITEM_TEXTSTYLE_SHADOWED) || (style == ITEM_TEXTSTYLE_SHADOWEDLESS)) {
				black[3] = newColor[3];

				if (style == ITEM_TEXTSTYLE_SHADOWEDLESS)
					black[3] *= 0.7;

				if (newColor[0] == 0.0f && newColor[1] == 0.0f && newColor[2] == 0.0f) {
					grey[3] = black[3];
					re.SetColor(grey);
				}
				else {
					re.SetColor(black);
				}

				SCR_DrawFontChar(x + 1, y - yadj + 1,
					glyph->imageWidth,
					glyph->imageHeight,
					useScale,
					glyph->s,
					glyph->t,
					glyph->s2,
					glyph->t2,
					glyph->glyph);

				colorBlack[3] = 1.0;
				re.SetColor(newColor);
			}

			SCR_DrawFontChar(x, y - yadj,
				glyph->imageWidth,
				glyph->imageHeight,
				useScale,
				glyph->s,
				glyph->t,
				glyph->s2,
				glyph->t2,
				glyph->glyph);
			x += (glyph->xSkip * useScale);
			s++;
			count++;
		}
		re.SetColor(NULL);
	}
}


//===============================================================================

/*
=================
SCR_DrawDemoRecording
=================
*/
void SCR_DrawDemoRecording( void ) {
	char	string[sizeof(clc.recordNameShort)+32];
	int		pos;

	if ( !clc.demorecording ) {
		return;
	}
	if ( clc.spDemoRecording ) {
		return;
	}

	pos = FS_FTell( clc.recordfile );
	sprintf( string, "demo: %ik", pos / 1024 );

	SCR_DrawStringExt( 320 - strlen( string ) * 4, 0, 8, string, g_color_table[ ColorIndex( COLOR_WHITE ) ], qtrue, qfalse );
}


#ifdef USE_VOIP
/*
=================
SCR_DrawVoipMeter
=================
*/
void SCR_DrawVoipMeter( void ) {
	char	buffer[16];
	char	string[256];
	int limit, i;

	if (!cl_voipShowMeter->integer)
		return;  // player doesn't want to show meter at all.
	else if (!cl_voipSend->integer)
		return;  // not recording at the moment.
	else if (clc.state != CA_ACTIVE)
		return;  // not connected to a server.
	else if (!clc.voipEnabled)
		return;  // server doesn't support VoIP.
	else if (clc.demoplaying)
		return;  // playing back a demo.
	else if (!cl_voip->integer)
		return;  // client has VoIP support disabled.

	limit = (int) (clc.voipPower * 10.0f);
	if (limit > 10)
		limit = 10;

	for (i = 0; i < limit; i++)
		buffer[i] = '*';
	while (i < 10)
		buffer[i++] = ' ';
	buffer[i] = '\0';

	sprintf( string, "VoIP: [%s]", buffer );
	SCR_DrawStringExt( 320 - strlen( string ) * 4, 10, 8, string, g_color_table[ ColorIndex( COLOR_WHITE ) ], qtrue, qfalse );
}
#endif


/*
===============================================================================

DEBUG GRAPH

===============================================================================
*/

static	int			current;
static	float		values[1024];

/*
==============
SCR_DebugGraph
==============
*/
void SCR_DebugGraph (float value)
{
	values[current] = value;
	current = (current + 1) % ARRAY_LEN(values);
}


/*
==============
SCR_DrawDebugGraph
==============
*/
void SCR_DrawDebugGraph (void)
{
	int		a, x, y, w, i, h;
	float	v;

	//
	// draw the graph
	//
	w = cls.glconfig.vidWidth;
	x = 0;
	y = cls.glconfig.vidHeight;
	re.SetColor( g_color_table[ ColorIndex( COLOR_BLACK ) ] );
	re.DrawStretchPic(x, y - cl_graphheight->integer, 
		w, cl_graphheight->integer, 0, 0, 0, 0, cls.whiteShader );
	re.SetColor( NULL );

	for (a=0 ; a<w ; a++)
	{
		i = (ARRAY_LEN(values)+current-1-(a % ARRAY_LEN(values))) % ARRAY_LEN(values);
		v = values[i];
		v = v * cl_graphscale->integer + cl_graphshift->integer;
		
		if (v < 0)
			v += cl_graphheight->integer * (1+(int)(-v / cl_graphheight->integer));
		h = (int)v % cl_graphheight->integer;
		re.DrawStretchPic( x+w-1-a, y - h, 1, h, 0, 0, 0, 0, cls.whiteShader );
	}
}

//=============================================================================
/*
===============================================================================

NET MONITOR WIDGET

Displays a live overlay of client network / timing stats.  All data comes
from the already-available cl / clc structures so there is no extra polling
overhead beyond computing 1-second byte-rate windows.

CVars
  cl_netgraph      0 = off, 1 = show widget
  cl_netgraph_x/y  position in virtual 640x480 coords (default top-right)
  cl_netgraph_scale  text/box scale multiplier (default 1.0)
  cl_netlog        0 = off, 1 = log console cmds + FAST/RESET delta events, 2 = also log periodic stats

Command
  netgraph_dump    write a full stats snapshot + all CS_SERVERINFO cvars to
                   the session log file

===============================================================================
*/

/* ----- public hooks called from cl_parse.c / cl_input.c ----- */

void SCR_NetMonitorAddIncoming( int bytes, int drops ) {
	netMonInBytes     += bytes;
	netMonDropsWindow += drops;
}

void SCR_NetMonitorAddOutgoing( int bytes ) {
	netMonOutBytes += bytes;
}

void SCR_NetMonitorAddFrametime( int ft ) {
	netMonFtSum   += ft;
	netMonFtCount += 1;
	if ( !netMonFtValid ) {
		netMonFtMin   = ft;
		netMonFtMax   = ft;
		netMonFtValid = qtrue;
	} else {
		if ( ft < netMonFtMin ) netMonFtMin = ft;
		if ( ft > netMonFtMax ) netMonFtMax = ft;
	}
}

void SCR_NetMonitorAddSnapInterval( int measured, int expected ) {
	int gap = measured - expected;
	if ( gap < 0 ) gap = -gap;
	netMonSnapGapSum += gap;
	netMonSnapGapCount++;
	if ( gap > netMonSnapGapMax )
		netMonSnapGapMax = gap;
}

void SCR_NetMonitorAddCapHit( void ) {
	netMonCapHits++;
}

void SCR_NetMonitorAddExtrap( void ) {
	netMonExtrapCount++;
}

void SCR_NetMonitorAddTimeDelta( int dT ) {
	if ( !netMonDtValid ) {
		netMonDtMin   = dT;
		netMonDtMax   = dT;
		netMonDtValid = qtrue;
	} else {
		if ( dT < netMonDtMin ) netMonDtMin = dT;
		if ( dT > netMonDtMax ) netMonDtMax = dT;
	}
}

void SCR_NetMonitorAddPing( int ping ) {
	if ( ping <= 0 || ping >= 999 )
		return; /* skip invalid / unknown pings */
	netMonPingSum += ping;
	netMonPingCount++;
	if ( !netMonPingValid ) {
		netMonPingMin   = ping;
		netMonPingMax   = ping;
		netMonPingValid = qtrue;
	} else {
		if ( ping < netMonPingMin ) netMonPingMin = ping;
		if ( ping > netMonPingMax ) netMonPingMax = ping;
	}
}

void SCR_NetMonitorAddFastAdjust( void ) {
	netMonFastCount++;
}

void SCR_NetMonitorAddResetAdjust( void ) {
	netMonResetCount++;
}

void SCR_NetMonitorAddSlowAdjust( void ) {
	netMonSlowCount++;
}

/* ----- session log helpers ----- */

static void SCR_OpenNetLog( void ) {
	qtime_t t;
	char    path[MAX_OSPATH];
	char    header[256];

	if ( netLogFile )
		return; /* already open */

	Com_RealTime( &t );
	Com_sprintf( path, sizeof(path), "netdebug_%04d%02d%02d_%02d%02d%02d.log",
		1900 + t.tm_year, 1 + t.tm_mon, t.tm_mday,
		t.tm_hour, t.tm_min, t.tm_sec );

	netLogFile = FS_FOpenFileWrite( path );
	if ( netLogFile ) {
		Com_sprintf( header, sizeof(header),
			"=== Quake3e Net Debug Log  %04d-%02d-%02d %02d:%02d:%02d ===\n"
			"  cl_netlog=%d  (1=cmds, 2=cmds+periodic stats)\n"
			"  Use 'netgraph_dump' in-game for a full on-demand snapshot.\n\n",
			1900 + t.tm_year, 1 + t.tm_mon, t.tm_mday,
			t.tm_hour, t.tm_min, t.tm_sec,
			cl_netlog->integer );
		FS_Write( header, strlen(header), netLogFile );
		Com_Printf( "Net debug log opened: %s\n", path );
	}
}

static void SCR_WriteLog( const char *line ) {
	if ( netLogFile )
		FS_Write( line, strlen(line), netLogFile );
}

/* public – called by cl_keys.c when the user submits a console line */
void SCR_LogConsoleInput( const char *cmd ) {
	qtime_t t;
	char    line[MAX_STRING_CHARS + 64];

	if ( !cl_netlog || !cl_netlog->integer )
		return;

	SCR_OpenNetLog();
	Com_RealTime( &t );
	Com_sprintf( line, sizeof(line), "[%02d:%02d:%02d] CMD: %s\n",
		t.tm_hour, t.tm_min, t.tm_sec, cmd );
	SCR_WriteLog( line );
}

/* public – called by timing subsystem to record significant delta events */
void SCR_LogTimingEvent( const char *tag, int serverTimeDelta, int deltaDelta ) {
	qtime_t t;
	char    line[128];

	if ( !cl_netlog || !cl_netlog->integer )
		return;

	SCR_OpenNetLog();
	Com_RealTime( &t );
	Com_sprintf( line, sizeof(line), "[%02d:%02d:%02d] DELTA %s  dT=%dms  dd=%dms\n",
		t.tm_hour, t.tm_min, t.tm_sec, tag, serverTimeDelta, deltaDelta );
	SCR_WriteLog( line );
}

void SCR_LogSnapLate( int measured, int expected ) {
	qtime_t t;
	char    line[128];

	if ( !cl_netlog || !cl_netlog->integer )
		return;

	SCR_OpenNetLog();
	Com_RealTime( &t );
	Com_sprintf( line, sizeof(line), "[%02d:%02d:%02d] SNAP LATE  +%dms  (expected %dms  got %dms)\n",
		t.tm_hour, t.tm_min, t.tm_sec, measured - expected, expected, measured );
	SCR_WriteLog( line );
}

void SCR_LogPingJitter( int ping, int prevPing ) {
	qtime_t t;
	char    line[128];
	int     delta = ping - prevPing;

	if ( !cl_netlog || !cl_netlog->integer )
		return;

	SCR_OpenNetLog();
	Com_RealTime( &t );
	Com_sprintf( line, sizeof(line), "[%02d:%02d:%02d] PING JITTER  %dms->%dms  (%+dms)\n",
		t.tm_hour, t.tm_min, t.tm_sec, prevPing, ping, delta );
	SCR_WriteLog( line );
}

/* public – close log on engine shutdown / explicit request */
void SCR_CloseNetLog( void ) {
	if ( netLogFile ) {
		SCR_WriteLog( "=== Session End ===\n" );
		FS_FCloseFile( netLogFile );
		netLogFile = 0;
	}
}

/* ----- netgraph_dump command ----- */

static void SCR_NetgraphDump_f( void ) {
	const char *serverInfo;
	char        key[BIG_INFO_KEY];
	char        value[BIG_INFO_VALUE];
	char        line[512];
	qtime_t     t;
	int         snapHz;

	if ( cls.state != CA_ACTIVE ) {
		Com_Printf( "netgraph_dump: not connected to a server.\n" );
		return;
	}

	SCR_OpenNetLog();
	if ( !netLogFile ) {
		Com_Printf( "netgraph_dump: could not open log file.\n" );
		return;
	}

	Com_RealTime( &t );
	snapHz = ( cl.snapshotMsec > 0 ) ? ( 1000 / cl.snapshotMsec ) : 0;

	Com_sprintf( line, sizeof(line),
		"\n=== netgraph_dump  %04d-%02d-%02d %02d:%02d:%02d ===\n",
		1900 + t.tm_year, 1 + t.tm_mon, t.tm_mday,
		t.tm_hour, t.tm_min, t.tm_sec );
	SCR_WriteLog( line );

	/* --- client timing & network stats --- */
	Com_sprintf( line, sizeof(line), "Snapshot Rate : %d Hz  (%d ms interval EMA)\n",  snapHz, cl.snapshotMsec );               SCR_WriteLog( line );
	Com_sprintf( line, sizeof(line), "Ping          : %d ms\n",                         cl.snap.ping );                          SCR_WriteLog( line );
	Com_sprintf( line, sizeof(line), "Interp Mode   : fI=%.3f  INTERPOLATING\n",
		cl.frameInterpolation );
	SCR_WriteLog( line );
	Com_sprintf( line, sizeof(line), "Server Time   : %d  (delta %d ms)\n",             cl.snap.serverTime, cl.serverTimeDelta );SCR_WriteLog( line );
	Com_sprintf( line, sizeof(line), "Snap Seq      : #%d  (delta from #%d, gap %d)\n", cl.snap.messageNum, cl.snap.deltaNum, cl.snap.messageNum - cl.snap.deltaNum ); SCR_WriteLog( line );
	Com_sprintf( line, sizeof(line), "Drop Rate     : %d pkt/s\n",                      netMonDropRate );                        SCR_WriteLog( line );
	Com_sprintf( line, sizeof(line), "In Rate       : %d B/s  (%.2f KB/s)\n",           netMonInRate,  netMonInRate  / 1024.0f );SCR_WriteLog( line );
	Com_sprintf( line, sizeof(line), "Out Rate      : %d B/s  (%.2f KB/s)\n",           netMonOutRate, netMonOutRate / 1024.0f );SCR_WriteLog( line );

	/* --- CS_SERVERINFO cvars (sv_fps, sv_gameHz, etc.) --- */
	SCR_WriteLog( "\n--- CS_SERVERINFO cvars ---\n" );
	serverInfo = cl.gameState.stringData + cl.gameState.stringOffsets[ CS_SERVERINFO ];
	while ( serverInfo && *serverInfo ) {
		serverInfo = Info_NextPair( serverInfo, key, value );
		if ( key[0] == '\0' )
			break;
		Com_sprintf( line, sizeof(line), "  %-28s = %s\n", key, value );
		SCR_WriteLog( line );
	}

	SCR_WriteLog( "=== end dump ===\n\n" );
	Com_Printf( "netgraph_dump written to log file.\n" );
}

/* ----- widget drawing ----- */

/* Ping colour thresholds (ms).  Reasonable competitive defaults: */
#define NM_PING_HIGH   150   /* red   — noticeably laggy              */
#define NM_PING_MEDIUM  80   /* yellow — mildly elevated               */
/* < NM_PING_MEDIUM    green — acceptable                             */

/* Widget columns and rows (character cells). */
#define NM_COLS 21
#define NM_ROWS 10

/*
 * NM_DrawRow — draw a NUL-terminated string starting at (*tx, ty) using
 * character-cell size charW, then advance ty by charH and reset tx.
 * Having this as a file-scope static function keeps the variable mutation
 * explicit and avoids macro hygiene pitfalls.
 */
static void NM_DrawRow( float *tx, float *ty, float bx_pad,
                        float charW, float charH,
                        const float *col, const char *str ) {
	const char *p;
	re.SetColor( col );
	for ( p = str; *p; p++ ) {
		SCR_DrawChar( (int)*tx, (int)*ty, charW, *p );
		*tx += charW;
	}
	*tx  = bx_pad;
	*ty += charH;
}

/*
==============
SCR_DrawNetMonitor

Draws a small always-on-top overlay (virtual 640x480 coords, scaled) showing
real-time client network/timing diagnostics.  Toggle with cl_netgraph 1.
==============
*/
static void SCR_DrawNetMonitor( void ) {
	static const float bgColor[4]     = { 0.0f, 0.0f, 0.0f, 0.65f };
	static const float colorWhite[4]  = { 1.0f, 1.0f, 1.0f, 1.0f  };
	static const float colorGreen[4]  = { 0.2f, 1.0f, 0.2f, 1.0f  };
	static const float colorYellow[4] = { 1.0f, 1.0f, 0.2f, 1.0f  };
	static const float colorRed[4]    = { 1.0f, 0.3f, 0.3f, 1.0f  };

	char         line[48];
	const float *col;
	float        scale, charW, charH, pad;
	float        bw, bh, bx, by, tx, ty;
	int          snapHz;

	if ( cls.state != CA_ACTIVE )
		return;

	/* ---- update 1-second rate window ---- */
	if ( netMonLastUpdate == 0 || cls.realtime - netMonLastUpdate >= 1000 ) {
		int ftMin      = netMonFtValid   ? netMonFtMin  : 0;
		int ftAvg      = ( netMonFtCount > 0 ) ? ( netMonFtSum / netMonFtCount ) : 0;
		int ftMax      = netMonFtValid   ? netMonFtMax  : 0;
		int snapGapAvg = ( netMonSnapGapCount > 0 ) ? ( netMonSnapGapSum / netMonSnapGapCount ) : 0;
		int snapGapMax = netMonSnapGapMax;
		int capHits    = netMonCapHits;
		int extrapCnt  = netMonExtrapCount;
		int dtMin      = netMonDtValid   ? netMonDtMin  : cl.serverTimeDelta;
		int dtMax      = netMonDtValid   ? netMonDtMax  : cl.serverTimeDelta;
		int pingAvg    = ( netMonPingCount > 0 ) ? ( netMonPingSum / netMonPingCount ) : cl.snap.ping;
		int pingMin    = netMonPingValid  ? netMonPingMin : cl.snap.ping;
		int pingMax    = netMonPingValid  ? netMonPingMax : cl.snap.ping;
		int fastCnt    = netMonFastCount;
		int resetCnt   = netMonResetCount;
		int slowCnt    = netMonSlowCount;

		netMonInRate      = netMonInBytes;
		netMonOutRate     = netMonOutBytes;
		netMonDropRate    = netMonDropsWindow;
		netMonInBytes     = 0;
		netMonOutBytes    = 0;
		netMonDropsWindow = 0;
		netMonLastUpdate  = cls.realtime;
		netMonFtSum       = 0;
		netMonFtCount     = 0;
		netMonFtMin       = 0;
		netMonFtMax       = 0;
		netMonFtValid     = qfalse;
		netMonSnapGapSum  = 0;
		netMonSnapGapCount = 0;
		netMonSnapGapMax  = 0;
		netMonCapHits     = 0;
		netMonExtrapCount = 0;
		netMonDtValid     = qfalse;
		netMonPingSum     = 0;
		netMonPingCount   = 0;
		netMonPingValid   = qfalse;
		netMonFastCount   = 0;
		netMonResetCount  = 0;
		netMonSlowCount   = 0;

		/* optional periodic stats line in the log */
		if ( cl_netlog->integer >= 2 && netLogFile ) {
			qtime_t t;
			char    logline[256];
			Com_RealTime( &t );
			snapHz = ( cl.snapshotMsec > 0 ) ? ( 1000 / cl.snapshotMsec ) : 0;
			Com_sprintf( logline, sizeof(logline),
				"[%02d:%02d:%02d] STATS  snap=%dHz  ping=%d(%d..%d)ms  fI=%.3f(INTERP)"
				"  dT=%d..%dms  drop=%d/s  in=%dB/s  out=%dB/s"
				"  ft=%d/%d/%dms  snapgap=%d/%dms  caps=%d  extrap=%d"
				"  fast=%d  reset=%d  slow=%d\n",
				t.tm_hour, t.tm_min, t.tm_sec,
				snapHz, pingAvg, pingMin, pingMax,
				cl.frameInterpolation,
				dtMin, dtMax, netMonDropRate,
				netMonInRate, netMonOutRate,
				ftMin, ftAvg, ftMax,
				snapGapAvg, snapGapMax,
				capHits, extrapCnt,
				fastCnt, resetCnt, slowCnt );
			SCR_WriteLog( logline );
		}
	}

	/* ---- widget geometry (virtual 640x480 coords) ---- */
	scale = cl_netgraph_scale->value;
	if ( scale <= 0.0f ) scale = 1.0f;

	charW = 8.0f * scale;   /* character cell width  (virtual units) */
	charH = 8.0f * scale;   /* character cell height (virtual units) */
	pad   = charW * 0.5f;   /* inner padding */

	bw = NM_COLS * charW + pad * 2.0f;
	bh = NM_ROWS * charH + pad * 2.0f;

	bx = cl_netgraph_x->value;
	by = cl_netgraph_y->value;

	/* clamp to virtual screen bounds (2-pixel margin) */
	if ( bx + bw > SCREEN_WIDTH  - 2 ) bx = SCREEN_WIDTH  - 2 - bw;
	if ( bx < 2.0f                    ) bx = 2.0f;
	if ( by + bh > SCREEN_HEIGHT - 2 ) by = SCREEN_HEIGHT - 2 - bh;
	if ( by < 2.0f                    ) by = 2.0f;

	/* semi-transparent background */
	SCR_FillRect( bx, by, bw, bh, bgColor );

	tx = bx + pad;
	ty = by + pad;

	snapHz = ( cl.snapshotMsec > 0 ) ? ( 1000 / cl.snapshotMsec ) : 0;

	/* row 1 – title */
	NM_DrawRow( &tx, &ty, bx + pad, charW, charH, colorWhite,
		"== NET MONITOR ==" );

	/* row 2 – snapshot rate */
	Com_sprintf( line, sizeof(line), "Snap: %3dHz %3dms", snapHz, cl.snapshotMsec );
	NM_DrawRow( &tx, &ty, bx + pad, charW, charH, colorWhite, line );

	/* row 3 – ping (colour-coded by threshold) */
	col = ( cl.snap.ping >= NM_PING_HIGH   ) ? colorRed    :
	      ( cl.snap.ping >= NM_PING_MEDIUM ) ? colorYellow : colorGreen;
	Com_sprintf( line, sizeof(line), "Ping: %dms", cl.snap.ping );
	NM_DrawRow( &tx, &ty, bx + pad, charW, charH, col, line );

	/* row 4 – smoothed fI: EMA (alpha=0.2) reduces per-frame flicker at high
	 * snap rates where the raw value cycles 0→1 faster than the eye can read */
	{
		static float smoothFI = 0.0f;
		smoothFI = smoothFI * 0.8f + cl.frameInterpolation * 0.2f;
		col = colorGreen;
		Com_sprintf( line, sizeof(line), "fI:   %.2f INTERP", smoothFI );
		NM_DrawRow( &tx, &ty, bx + pad, charW, charH, col, line );
	}

	/* row 5 – server time delta */
	Com_sprintf( line, sizeof(line), "dT:   %+dms", cl.serverTimeDelta );
	NM_DrawRow( &tx, &ty, bx + pad, charW, charH, colorWhite, line );

	/* row 6 – dropped packets/s */
	col = ( netMonDropRate > 0 ) ? colorRed : colorGreen;
	Com_sprintf( line, sizeof(line), "Drop: %d/s", netMonDropRate );
	NM_DrawRow( &tx, &ty, bx + pad, charW, charH, col, line );

	/* row 7 – in rate */
	if ( netMonInRate >= 1024 )
		Com_sprintf( line, sizeof(line), "In:   %.1fKB/s", netMonInRate / 1024.0f );
	else
		Com_sprintf( line, sizeof(line), "In:   %dB/s", netMonInRate );
	NM_DrawRow( &tx, &ty, bx + pad, charW, charH, colorWhite, line );

	/* row 8 – out rate */
	if ( netMonOutRate >= 1024 )
		Com_sprintf( line, sizeof(line), "Out:  %.1fKB/s", netMonOutRate / 1024.0f );
	else
		Com_sprintf( line, sizeof(line), "Out:  %dB/s", netMonOutRate );
	NM_DrawRow( &tx, &ty, bx + pad, charW, charH, colorWhite, line );

	/* row 9 – snapshot sequence / delta compression detail */
	Com_sprintf( line, sizeof(line), "Seq:  #%d d:%d",
		cl.snap.messageNum,
		cl.snap.messageNum - cl.snap.deltaNum );
	NM_DrawRow( &tx, &ty, bx + pad, charW, charH, colorWhite, line );

	/* row 10 – time-delta adjustment counts (current second so far).
	 * slow = slow-path ms-commits; with the ½ms accumulator fix this is 0 at
	 * 60Hz equilibrium.  A non-zero slow paired with PING JITTER in the log
	 * means the oscillation issue has recurred.
	 * fast = FAST-path fires (large snap-to-snap delta > 2×snapshotMsec). */
	{
		col = ( netMonFastCount > 0 ) ? colorRed :
		      ( netMonSlowCount > 0 ) ? colorYellow : colorGreen;
		Com_sprintf( line, sizeof(line), "Adj: slo=%d fst=%d", netMonSlowCount, netMonFastCount );
		NM_DrawRow( &tx, &ty, bx + pad, charW, charH, col, line );
	}

	re.SetColor( NULL );
}

#undef NM_PING_HIGH
#undef NM_PING_MEDIUM
#undef NM_COLS
#undef NM_ROWS

//=============================================================================

/*
==================
SCR_Init
==================
*/
void SCR_Init( void ) {
	cl_timegraph = Cvar_Get ("timegraph", "0", CVAR_CHEAT);
    Cvar_SetDescription(cl_timegraph, "Display the time graph\nDefault: 0");

    cl_debuggraph = Cvar_Get ("debuggraph", "0", CVAR_CHEAT);
    Cvar_SetDescription(cl_debuggraph, "Display the debug graph\nDefault: 0");

    cl_graphheight = Cvar_Get ("graphheight", "32", CVAR_CHEAT);
    Cvar_SetDescription(cl_graphheight, "Set the height of the graph\nDefault: 32");

    cl_graphscale = Cvar_Get ("graphscale", "1", CVAR_CHEAT);
    Cvar_SetDescription(cl_graphscale, "Set the scale of the size\nDefault: 1");

    cl_graphshift = Cvar_Get ("graphshift", "0", CVAR_CHEAT);
    Cvar_SetDescription(cl_graphshift, "Set the shift of the graph\nDefault: 0");

    cl_netgraph = Cvar_Get( "cl_netgraph", "0", 0 );
    Cvar_SetDescription( cl_netgraph, "Show the net monitor overlay.\n0 = off, 1 = on\nDefault: 0" );

    cl_netgraph_x = Cvar_Get( "cl_netgraph_x", "460", 0 );
    Cvar_SetDescription( cl_netgraph_x, "Net monitor X position in virtual 640x480 coords.\nDefault: 460 (near top-right)" );

    cl_netgraph_y = Cvar_Get( "cl_netgraph_y", "4", 0 );
    Cvar_SetDescription( cl_netgraph_y, "Net monitor Y position in virtual 640x480 coords.\nDefault: 4 (near top)" );

    cl_netgraph_scale = Cvar_Get( "cl_netgraph_scale", "1.0", 0 );
    Cvar_SetDescription( cl_netgraph_scale, "Net monitor text/box scale multiplier.\nDefault: 1.0" );

    cl_netlog = Cvar_Get( "cl_netlog", "0", 0 );
    Cvar_SetDescription( cl_netlog,
        "Net debug session logging.\n"
        "0 = off\n"
        "1 = log FAST/RESET delta events + SNAP LATE events + PING JITTER events\n"
        "    PING JITTER fires when per-snap ping change >= max(snapshotMsec/2, 10ms);\n"
        "    rapid alternating events (e.g. 32ms->40ms / 40ms->32ms every snap) paired\n"
        "    with slow>0 in STATS means the serverTimeDelta oscillation has recurred.\n"
        "2 = log level 1 events + periodic per-second stats\n"
        "  STATS fields: snap Hz, ping=avg(min..max), fI, dT=min..max, drop, in/out rates,\n"
        "                ft=min/avg/max client frame-time, snapgap=avg/max snap-interval jitter,\n"
        "                caps=serverTime cap fires, extrap=extrapolated-frame count,\n"
        "                fast=FAST-adjust count, reset=RESET-adjust count,\n"
        "                slow=slow-path ms-commits (0 at 60Hz equilibrium = fix working;\n"
        "                     non-zero = genuine drift or oscillation regression)\n"
        "Log file written to netdebug_<date>_<time>.log in the game folder.\n"
        "Default: 0" );

    Cmd_AddCommand( "netgraph_dump", SCR_NetgraphDump_f );
    Cmd_SetDescription( "netgraph_dump",
        "Write a full timestamped net-stats snapshot (including all CS_SERVERINFO/sv_ cvars)\n"
        "to the net debug log file.  Requires an active server connection.\n"
        "Enable cl_netlog 1 first to open the log, or the dump command will open it automatically." );

    scr_initialized = qtrue;
}


/*
==================
SCR_Done
==================
*/
void SCR_Done( void ) {
	SCR_CloseNetLog();
	Cmd_RemoveCommand( "netgraph_dump" );
	scr_initialized = qfalse;
}


//=======================================================

/*
==================
SCR_DrawScreenField

This will be called twice if rendering in stereo mode
==================
*/
void SCR_DrawScreenField( stereoFrame_t stereoFrame ) {
	qboolean uiFullscreen;

	re.BeginFrame( stereoFrame );

	uiFullscreen = (uivm && VM_Call( uivm, 0, UI_IS_FULLSCREEN ));

	// wide aspect ratio screens need to have the sides cleared
	// unless they are displaying game renderings
	if ( uiFullscreen || cls.state < CA_LOADING ) {
		if ( cls.glconfig.vidWidth * 480 > cls.glconfig.vidHeight * 640 ) {
			re.SetColor( g_color_table[ ColorIndex( COLOR_BLACK ) ] );
			re.DrawStretchPic( 0, 0, cls.glconfig.vidWidth, cls.glconfig.vidHeight, 0, 0, 0, 0, cls.whiteShader );
			re.SetColor( NULL );
		}
	}

	// if the menu is going to cover the entire screen, we
	// don't need to render anything under it
	if ( uivm && !uiFullscreen ) {
		switch( cls.state ) {
		default:
			Com_Error( ERR_FATAL, "SCR_DrawScreenField: bad cls.state" );
			break;
		case CA_CINEMATIC:
			SCR_DrawCinematic();
			break;
		case CA_DISCONNECTED:
			// force menu up
			S_StopAllSounds();
			VM_Call( uivm, 1, UI_SET_ACTIVE_MENU, UIMENU_MAIN );
			break;
		case CA_CONNECTING:
		case CA_CHALLENGING:
		case CA_CONNECTED:
			// connecting clients will only show the connection dialog
			// refresh to update the time
			VM_Call( uivm, 1, UI_REFRESH, cls.realtime );
			VM_Call( uivm, 1, UI_DRAW_CONNECT_SCREEN, qfalse );
			break;
		case CA_LOADING:
		case CA_PRIMED:
			// draw the game information screen and loading progress
			if ( cgvm ) {
				CL_CGameRendering( stereoFrame );
			}
			// also draw the connection information, so it doesn't
			// flash away too briefly on local or lan games
			// refresh to update the time
			VM_Call( uivm, 1, UI_REFRESH, cls.realtime );
			VM_Call( uivm, 1, UI_DRAW_CONNECT_SCREEN, qtrue );
			break;
		case CA_ACTIVE:
			// always supply STEREO_CENTER as vieworg offset is now done by the engine.
			CL_CGameRendering( stereoFrame );
			SCR_DrawDemoRecording();
			if ( cl_netgraph->integer )
				SCR_DrawNetMonitor();
#ifdef USE_VOIP
			SCR_DrawVoipMeter();
#endif
			break;
		}
	}

	// the menu draws next
	if ( Key_GetCatcher( ) & KEYCATCH_UI && uivm ) {
		VM_Call( uivm, 1, UI_REFRESH, cls.realtime );
	}

	// console draws next
	Con_DrawConsole ();

	// debug graph can be drawn on top of anything
	if ( cl_debuggraph->integer || cl_timegraph->integer || cl_debugMove->integer ) {
		SCR_DrawDebugGraph ();
	}
}


/*
==================
SCR_UpdateScreen

This is called every frame, and can also be called explicitly to flush
text to the screen.
==================
*/
void SCR_UpdateScreen( void ) {
	static int recursive;
	static int framecount;
	static int next_frametime;

	if ( !scr_initialized )
		return; // not initialized yet

	if ( framecount == cls.framecount ) {
		int ms = Sys_Milliseconds();
		if ( next_frametime && ms - next_frametime < 0 ) {
			re.ThrottleBackend();
		} else {
			next_frametime = ms + 16; // limit to 60 FPS
		}
	} else {
		next_frametime = 0;
		framecount = cls.framecount;
	}

	if ( ++recursive > 2 ) {
		Com_Error( ERR_FATAL, "SCR_UpdateScreen: recursively called" );
	}
	recursive = 1;

	// If there is no VM, there are also no rendering commands issued. Stop the renderer in
	// that case.
	if ( uivm )
	{
		// XXX
		int in_anaglyphMode = Cvar_VariableIntegerValue("r_anaglyphMode");
		// if running in stereo, we need to draw the frame twice
		if ( cls.glconfig.stereoEnabled || in_anaglyphMode) {
			SCR_DrawScreenField( STEREO_LEFT );
			SCR_DrawScreenField( STEREO_RIGHT );
		} else {
			SCR_DrawScreenField( STEREO_CENTER );
		}

		if ( com_speeds->integer ) {
			re.EndFrame( &time_frontend, &time_backend );
		} else {
			re.EndFrame( NULL, NULL );
		}
	}

	recursive = 0;
}
