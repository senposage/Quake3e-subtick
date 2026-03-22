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

static qboolean	scr_initialized;		// ready to draw

cvar_t		*cl_timegraph;
static cvar_t		*cl_debuggraph;
static cvar_t		*cl_graphheight;
static cvar_t		*cl_graphscale;
static cvar_t		*cl_graphshift;

// Net monitor widget cvars
cvar_t		*cl_netgraph;
cvar_t		*cl_netgraph_x;
cvar_t		*cl_netgraph_y;
cvar_t		*cl_netgraph_scale;
cvar_t		*cl_netlog;
cvar_t		*cl_laggotannounce;

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

// Cap-hit, extrap, choke, and serverTimeDelta range tracking (reset each second)
static int	netMonCapHits;
static int	netMonExtrapCount;
static int	netMonChokeCount;
static int	netMonDtMin;
static int	netMonDtMax;
static qboolean	netMonDtValid;

// Ping jitter tracking (reset each second)
static int	netMonPingSum;
static int	netMonPingCount;
static int	netMonPingMin;
static int	netMonPingMax;
static qboolean	netMonPingValid;

// FAST/RESET adjustment counts (reset each second)
static int	netMonFastCount;
static int	netMonResetCount;
// Slow-path net drift (signed: +1 per up-commit, -1 per down-commit; reset each second)
static int	netMonSlowCount;
// Per-second abs(netMonSlowCount) snapshot used by the display widget
static int	netMonSlowRate;

// Per-second display snapshots (survive into widget rendering)
static int	netMonDispPingMin;
static int	netMonDispPingMax;
static int	netMonDispCapHits;
static int	netMonDispExtrapCnt;
static int	netMonDispSnapGapAvg;
static int	netMonDispSnapGapMax;
static int	netMonDispSnapCount;	// snaps received in the last second (for accurate Hz display)
static int	netMonDispFtAvg;
static int	netMonDispFtMax;
static float	netMonDispFiAvg;

// Per-frame fI accumulator (for 1-second average)
static float	netMonFiSum;
static int	netMonFiCount;

// Laggot announce cooldown (realtime of last announcement)
static int	laggotLastAnnounce;

// 30-second ring buffer for laggot announce (stores per-second snapshots)
#define LAGGOT_HISTORY 30
typedef struct {
	int dropRate;
	int fastCnt;
	int snapGapAvg;
	int snapGapMax;
	int extrapCnt;
	int ftAvg;
	int chokeCnt;
} laggotSample_t;
static laggotSample_t	laggotHistory[LAGGOT_HISTORY];
static int		laggotHistoryIdx;

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


//===============================================================================

/*
=================
SCR_DrawDemoRecording
=================
*/
static void SCR_DrawDemoRecording( void ) {
	char	string[sizeof(clc.recordNameShort)+32];
	int		pos;

	if ( !clc.demorecording ) {
		return;
	}
	if ( clc.spDemoRecording ) {
		return;
	}

	pos = FS_FTell( clc.recordfile );

	if (cl_drawRecording->integer == 1) {
		sprintf(string, "RECORDING %s: %ik", clc.recordNameShort, pos / 1024);
		SCR_DrawStringExt(320 - strlen(string) * 4, 20, 8, string, g_color_table[ColorIndex(COLOR_WHITE)], qtrue, qfalse);
	} else if (cl_drawRecording->integer == 2) {
		sprintf(string, "RECORDING: %ik", pos / 1024);
		SCR_DrawStringExt(320 - strlen(string) * 4, 20, 8, string, g_color_table[ColorIndex(COLOR_WHITE)], qtrue, qfalse);
	}
}


#ifdef USE_VOIP
/*
=================
SCR_DrawVoipMeter
=================
*/
static void SCR_DrawVoipMeter( void ) {
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
void SCR_DebugGraph( float value )
{
	values[current] = value;
	current = (current + 1) % ARRAY_LEN(values);
}


/*
==============
SCR_DrawDebugGraph
==============
*/
static void SCR_DrawDebugGraph( void )
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
  cl_netlog        0 = off, 1 = log FAST/RESET delta events, 2 = also log periodic stats

Command
  netgraph_dump    write a full stats snapshot to the session log file

===============================================================================
*/

/* ----- public hooks called from cl_parse.c / cl_cgame.c / cl_input.c ----- */

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
	// Accumulate frameInterpolation for 1-second average
	netMonFiSum   += cl.frameInterpolation;
	netMonFiCount += 1;
}

void SCR_NetMonitorAddSnapInterval( int measured, int expected ) {
	int gap = measured - expected;
	if ( gap < 0 ) gap = -gap;
	netMonSnapGapSum += gap;
	netMonSnapGapCount++;
	if ( gap > netMonSnapGapMax )
		netMonSnapGapMax = gap;
}

// Per-connection cap-hit total; resets in SCR_LogConnectInfo on each new gamestate.
static int netMonCapHitsSession;

// Per-connection count of OOB disconnect packets that were ignored.
static int netMonOOBIgnoredSession;

void SCR_NetMonitorAddCapHit( void ) {
	netMonCapHits++;
	netMonCapHitsSession++;
}

void SCR_OOBIgnoredIncrement( void ) {
	netMonOOBIgnoredSession++;
}

void SCR_NetMonitorAddExtrap( void ) {
	netMonExtrapCount++;
}

void SCR_NetMonitorAddChoke( void ) {
	netMonChokeCount++;
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
		return;
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

void SCR_NetMonitorAddSlowAdjust( int delta ) {
	netMonSlowCount += delta;
}

/* ----- session log helpers ----- */

static void SCR_OpenNetLog( void ) {
	qtime_t t;
	char    path[MAX_OSPATH];
	char    header[256];

	if ( netLogFile )
		return;

	Com_RealTime( &t );
	Com_sprintf( path, sizeof(path), "netdebug_%04d%02d%02d_%02d%02d%02d.log",
		1900 + t.tm_year, 1 + t.tm_mon, t.tm_mday,
		t.tm_hour, t.tm_min, t.tm_sec );

	netLogFile = FS_FOpenFileWrite( path );
	if ( netLogFile ) {
		Com_sprintf( header, sizeof(header),
			"=== Quake3e Net Debug Log  %04d-%02d-%02d %02d:%02d:%02d ===\n"
			"  cl_netlog=%d"
			"  (1=CONNECT+SVCMD+FAST/RESET+SNAPLATE+TIMEOUT+DISCONNECT"
			"  2=+SNAP+STATS)\n\n",
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

void SCR_CloseNetLog( void ) {
	if ( netLogFile ) {
		SCR_WriteLog( "=== Session End ===\n" );
		FS_FCloseFile( netLogFile );
		netLogFile = 0;
	}
}

/*
========================
SCR_LogConnectInfo

Called from CL_ParseServerInfo on every gamestate receive.
Logs the detected server type and timing seed.  Level 1.
========================
*/
void SCR_LogConnectInfo( const char *svFps, const char *snapFps,
                          const char *allowAt, int snapshotMsec,
                          qboolean vanilla, qboolean forbids ) {
	qtime_t t;
	char line[256];

	// Always reset per-session counters so SCR_LogDisconnect reports
	// accurate totals even when cl_netlog is toggled mid-session.
	netMonCapHitsSession    = 0;
	netMonOOBIgnoredSession = 0;

	if ( !cl_netlog || !cl_netlog->integer )
		return;

	SCR_OpenNetLog();
	Com_RealTime( &t );
	Com_sprintf( line, sizeof(line),
		"[%02d:%02d:%02d] CONNECT  sv_fps=%s  sv_snapshotFps=%s"
		"  sv_allowClientAdaptiveTiming=%s"
		"  snapshotMsec=%d  vanilla=%d  forbidsAdaptive=%d\n",
		t.tm_hour, t.tm_min, t.tm_sec,
		svFps[0]    ? svFps    : "(none)",
		snapFps[0]  ? snapFps  : "(none)",
		allowAt[0]  ? allowAt  : "(none)",
		snapshotMsec, (int)vanilla, (int)forbids );
	SCR_WriteLog( line );
}


/*
========================
SCR_LogServerCmd

Called from CL_ParseCommandString for every new server command.
Logs sequence numbers and the command text (first 80 chars).  Level 1.
========================
*/
void SCR_LogServerCmd( int seq, int storedSeq, const char *text ) {
	qtime_t t;
	char line[256];
	char truncated[81];

	if ( !cl_netlog || !cl_netlog->integer )
		return;

	SCR_OpenNetLog();
	Com_RealTime( &t );
	Q_strncpyz( truncated, text, sizeof(truncated) );
	Com_sprintf( line, sizeof(line),
		"[%02d:%02d:%02d] SVCMD  seq=%d  stored=%d  \"%s\"\n",
		t.tm_hour, t.tm_min, t.tm_sec,
		seq, storedSeq, truncated );
	SCR_WriteLog( line );
}


/*
========================
SCR_LogSnapState

Called from CL_ParseSnapshot after each accepted snapshot.
Logs reliable-command sequence state to detect command-buffer flooding.  Level 2.
========================
*/
void SCR_LogSnapState( int snapTime, int ping, int cmdTime, int msgSeq,
                        int cmdSeq, int relSeq, int relAck ) {
	qtime_t t;
	char line[224];

	if ( !cl_netlog || cl_netlog->integer < 2 )
		return;

	SCR_OpenNetLog();
	Com_RealTime( &t );
	Com_sprintf( line, sizeof(line),
		"[%02d:%02d:%02d] SNAP  t=%d  ping=%d  cmdTime=%d  msg=%d"
		"  cmdSeq=%d  relSeq=%d  relAck=%d\n",
		t.tm_hour, t.tm_min, t.tm_sec,
		snapTime, ping, cmdTime, msgSeq, cmdSeq, relSeq, relAck );
	SCR_WriteLog( line );
}


/*
========================
SCR_LogTimeout

Called from CL_CheckTimeout on every timeout counter increment.
Logs count, elapsed silence, and the configured limit.  Level 1.
========================
*/
void SCR_LogTimeout( int count, int elapsed, int limit ) {
	qtime_t t;
	char line[128];

	if ( !cl_netlog || !cl_netlog->integer )
		return;

	SCR_OpenNetLog();
	Com_RealTime( &t );
	Com_sprintf( line, sizeof(line),
		"[%02d:%02d:%02d] TIMEOUT  count=%d  elapsed=%dms  limit=%dms\n",
		t.tm_hour, t.tm_min, t.tm_sec, count, elapsed, limit );
	SCR_WriteLog( line );
}


/*
========================
SCR_LogNote

Generic one-line event annotation.  Level 1.
Used for warning-level events that are not themselves disconnects
(e.g. reliable-command overflow recovery).
========================
*/
void SCR_LogNote( const char *tag, const char *msg ) {
	qtime_t t;
	char line[256];

	if ( !cl_netlog || !cl_netlog->integer )
		return;

	SCR_OpenNetLog();
	Com_RealTime( &t );
	Com_sprintf( line, sizeof(line), "[%02d:%02d:%02d] %s  %s\n",
		t.tm_hour, t.tm_min, t.tm_sec, tag, msg );
	SCR_WriteLog( line );
}


/*
========================
SCR_LogPacketDrop

Called from CL_ParseServerMessage when clc.netchan.dropped > 0.
Netchan drops mean sequence-number gaps: the server sent UDP packets that
never arrived at the client.  Each dropped server-to-client packet may
contain a snapshot; missing snapshots force delta-invalidation, which the
QVM lagometer shows as black bars.  Level 1.

Fields:
  dropped  -- netchan gap (packets lost in this delivery)
  seq      -- clc.serverMessageSequence (sequence number of the packet we DID receive)
  ping     -- cl.snap.ping at the moment of detection
  snapMs   -- cl.snapshotMsec (helps judge how many snap intervals were lost)
========================
*/
void SCR_LogPacketDrop( int dropped, int seq ) {
	qtime_t t;
	char line[128];

	if ( !cl_netlog || !cl_netlog->integer )
		return;

	SCR_OpenNetLog();
	Com_RealTime( &t );
	Com_sprintf( line, sizeof(line),
		"[%02d:%02d:%02d] DROP  dropped=%d  seq=%d  ping=%d  snapMs=%d\n",
		t.tm_hour, t.tm_min, t.tm_sec,
		dropped, seq, cl.snap.ping, cl.snapshotMsec );
	SCR_WriteLog( line );
}


/*
========================
SCR_LogCapRelease

Called from CL_SetCGameTime when the safety cap releases because the
server has been silent for >= 2 000 ms.  At this point cl.serverTime
is allowed to advance freely beyond snap.serverTime until the next
snapshot triggers a hard-reset via CL_AdjustTimeDelta.  Level 1,
rate-limited to one entry per 500 ms to avoid per-frame spam.
========================
*/
void SCR_LogCapRelease( int drift ) {
	static int lastLogRealtime = -99999;
	qtime_t t;
	char line[128];

	if ( !cl_netlog || !cl_netlog->integer )
		return;

	/* rate-limit: log at most once per 500 ms */
	if ( cls.realtime - lastLogRealtime < 500 )
		return;
	lastLogRealtime = cls.realtime;

	SCR_OpenNetLog();
	Com_RealTime( &t );
	Com_sprintf( line, sizeof(line),
		"[%02d:%02d:%02d] CAP_RELEASE  drift=%dms  snapMs=%d  snapT=%d  svrT=%d\n",
		t.tm_hour, t.tm_min, t.tm_sec,
		drift, cl.snapshotMsec, cl.snap.serverTime, cl.serverTime );
	SCR_WriteLog( line );
}


/*
========================
SCR_LogOOBPacket

Called from CL_ConnectionlessPacket whenever a connectionless (OOB) packet
arrives from the currently connected server.  Gives a full trace of every
OOB command the server sends so we can see what leads up to a disconnect.
Level 2.

Fields:
  cmd  -- tokenised command token (first word of the OOB string)
========================
*/
void SCR_LogOOBPacket( const char *cmd ) {
	qtime_t t;
	char    line[128];

	if ( !cl_netlog || cl_netlog->integer < 2 )
		return;

	SCR_OpenNetLog();
	Com_RealTime( &t );
	Com_sprintf( line, sizeof(line),
		"[%02d:%02d:%02d] OOB:PACKET  cmd='%s'\n",
		t.tm_hour, t.tm_min, t.tm_sec, cmd );
	SCR_WriteLog( line );
}


/*
========================
SCR_LogOOBDisconnect

Called every time an OOB "disconnect" packet arrives from the server,
regardless of whether it is honoured or ignored.  This is a dedicated
entry separate from SCR_LogDisconnect so we can distinguish the *arrival*
of the packet from the actual engine-level disconnect action, and so we
can correlate it with the sequence of events in the surrounding log.
Level 1.

Fields:
  honored      -- 1 if Com_Error was called; 0 if packet was ignored
  snapT        -- cl.snap.serverTime
  svrT         -- cl.serverTime
  ping         -- cl.snap.ping
  relSeq       -- clc.reliableSequence  (last reliable cmd we sent)
  relAck       -- clc.reliableAcknowledge (last ack from server)
  relWnd       -- relSeq - relAck (fill level of our reliable window)
  silenceMs    -- ms since last packet from server at arrival time
  sessionCount -- running total of OOB disconnects seen this session
========================
*/
void SCR_LogOOBDisconnect( qboolean honored ) {
	qtime_t t;
	char    line[320];
	int     silenceMs, relWnd;

	silenceMs = ( clc.lastPacketTime > 0 ) ? ( cls.realtime - clc.lastPacketTime ) : -1;
	relWnd    = clc.reliableSequence - clc.reliableAcknowledge;

	/* Count first so both the console print and the log file see the same value. */
	if ( !honored )
		netMonOOBIgnoredSession++;

	/* Always print to console regardless of cl_netlog. */
	Com_Printf( S_COLOR_YELLOW
		"[OOB:DISCONNECT] honored=%d  snapT=%d  ping=%d"
		"  relSeq=%d  relAck=%d  relWnd=%d  silenceMs=%d  sessionCount=%d\n",
		(int)honored,
		cl.snap.serverTime, cl.snap.ping,
		clc.reliableSequence, clc.reliableAcknowledge, relWnd,
		silenceMs, netMonOOBIgnoredSession );

	if ( !cl_netlog || !cl_netlog->integer )
		return;

	SCR_OpenNetLog();
	Com_RealTime( &t );
	Com_sprintf( line, sizeof(line),
		"[%02d:%02d:%02d] OOB:DISCONNECT"
		"  honored=%d"
		"  snapT=%d  svrT=%d  ping=%d"
		"  relSeq=%d  relAck=%d  relWnd=%d"
		"  silenceMs=%d  sessionCount=%d\n",
		t.tm_hour, t.tm_min, t.tm_sec,
		(int)honored,
		cl.snap.serverTime, cl.serverTime, cl.snap.ping,
		clc.reliableSequence, clc.reliableAcknowledge, relWnd,
		silenceMs, netMonOOBIgnoredSession );
	SCR_WriteLog( line );
}


/*
========================
SCR_LogUserinfoSend

Called from CL_CheckUserinfo every time a "userinfo" reliable command is
sent to the server during an active session (i.e. after the initial
connection handshake).  Logs the auth-relevant and identity fields so we
can correlate userinfo changes with server reactions (e.g. auth re-checks
that may precede spurious disconnects).  Level 2.

Fields:
  name   -- player name
  authc  -- auth-code field (should be "0" in this fork; server may react)
  authl  -- auth-level field
  snaps  -- requested snapshot rate
========================
*/
void SCR_LogUserinfoSend( const char *info ) {
	qtime_t t;
	char    line[384];

	if ( !cl_netlog || cl_netlog->integer < 2 )
		return;

	SCR_OpenNetLog();
	Com_RealTime( &t );
	Com_sprintf( line, sizeof(line),
		"[%02d:%02d:%02d] USERINFO:SEND"
		"  name=\"%s\"  authc=\"%s\"  authl=\"%s\"  snaps=\"%s\"\n",
		t.tm_hour, t.tm_min, t.tm_sec,
		Info_ValueForKey( info, "name"  ),
		Info_ValueForKey( info, "authc" ),
		Info_ValueForKey( info, "authl" ),
		Info_ValueForKey( info, "snaps" ) );
	SCR_WriteLog( line );
}


/*
========================
SCR_LogDisconnect

Called immediately before any client disconnect or ERR_DROP/ERR_SERVERDISCONNECT
that is triggered by server action.  Always prints the context dump to the console
(visible even when cl_netlog is 0) and also writes to the session log file when
cl_netlog >= 1.

Fields logged:
  reason        - server-supplied disconnect reason string
  snapT         - cl.snap.serverTime (last received snapshot time)
  svrT          - cl.serverTime (computed cgame time at disconnect)
  dT            - cl.serverTimeDelta
  ping          - cl.snap.ping (note: can be 999 due to snap-timing calculation
                  failure, not necessarily a server-side negative-ping kick)
  cmdSeq        - clc.serverCommandSequence (last server command received)
  relSeq        - clc.reliableSequence (last client reliable command sent)
  relAck        - clc.reliableAcknowledge (last client cmd acked by server)
  relWnd        - relSeq - relAck (reliable window fill level; MAX=64)
  snapMs        - cl.snapshotMsec (EMA of snapshot interval)
  vanilla       - cl.vanillaServer (1 = server lacks sv_snapshotFps)
  forbids       - cl.serverForbidsAdaptiveTiming
  timeout       - cl.timeoutcount
  silenceMs     - ms since last packet from server
  capHits       - cap firings since last gamestate (cl.serverTime was capped)
  oobIgnored    - OOB disconnect packets ignored this session (cl_noOOBDisconnect)
========================
*/
void SCR_LogDisconnect( const char *reason ) {
	qtime_t t;
	char line[768];
	int elapsed, relWnd;

	elapsed = ( clc.lastPacketTime > 0 ) ? ( cls.realtime - clc.lastPacketTime ) : -1;
	relWnd  = clc.reliableSequence - clc.reliableAcknowledge;

	/* Always print context to console so it is visible even without cl_netlog. */
	Com_Printf( S_COLOR_YELLOW
		"[DISCONNECT TRACE] reason=\"%s\""
		" snapT=%d svrT=%d dT=%d ping=%d"
		" cmdSeq=%d relSeq=%d relAck=%d relWnd=%d"
		" snapMs=%d vanilla=%d forbids=%d"
		" timeout=%d silence=%dms caps=%d oobIgnored=%d\n",
		reason ? reason : "(none)",
		cl.snap.serverTime, cl.serverTime, cl.serverTimeDelta, cl.snap.ping,
		clc.serverCommandSequence, clc.reliableSequence, clc.reliableAcknowledge, relWnd,
		cl.snapshotMsec, (int)cl.vanillaServer, (int)cl.serverForbidsAdaptiveTiming,
		cl.timeoutcount, elapsed, netMonCapHitsSession, netMonOOBIgnoredSession );

	if ( !cl_netlog || !cl_netlog->integer )
		return;

	SCR_OpenNetLog();
	Com_RealTime( &t );
	Com_sprintf( line, sizeof(line),
		"[%02d:%02d:%02d] DISCONNECT  reason=\"%s\"\n"
		"  snapT=%d  svrT=%d  dT=%d  ping=%d\n"
		"  cmdSeq=%d  relSeq=%d  relAck=%d  relWnd=%d\n"
		"  snapMs=%d  vanilla=%d  forbidsAdaptive=%d\n"
		"  timeout=%d  silenceMs=%d  capHits=%d  oobIgnored=%d\n",
		t.tm_hour, t.tm_min, t.tm_sec,
		reason ? reason : "(none)",
		cl.snap.serverTime, cl.serverTime, cl.serverTimeDelta, cl.snap.ping,
		clc.serverCommandSequence, clc.reliableSequence, clc.reliableAcknowledge, relWnd,
		cl.snapshotMsec, (int)cl.vanillaServer, (int)cl.serverForbidsAdaptiveTiming,
		cl.timeoutcount, elapsed, netMonCapHitsSession, netMonOOBIgnoredSession );
	SCR_WriteLog( line );
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
	snapHz = (netMonDispSnapCount > 0) ? netMonDispSnapCount : ((cl.snapshotMsec > 0) ? (1000 / cl.snapshotMsec) : 0);

	Com_sprintf( line, sizeof(line),
		"\n=== netgraph_dump  %04d-%02d-%02d %02d:%02d:%02d ===\n",
		1900 + t.tm_year, 1 + t.tm_mon, t.tm_mday,
		t.tm_hour, t.tm_min, t.tm_sec );
	SCR_WriteLog( line );

	Com_sprintf( line, sizeof(line), "Snapshot Rate : %d Hz  (%d ms interval EMA)\n", snapHz, cl.snapshotMsec ); SCR_WriteLog( line );
	Com_sprintf( line, sizeof(line), "Ping          : %d ms\n", cl.snap.ping ); SCR_WriteLog( line );
	Com_sprintf( line, sizeof(line), "Interp Mode   : fI=%.3f\n", cl.frameInterpolation ); SCR_WriteLog( line );
	Com_sprintf( line, sizeof(line), "Server Time   : %d  (delta %d ms)\n", cl.snap.serverTime, cl.serverTimeDelta ); SCR_WriteLog( line );
	Com_sprintf( line, sizeof(line), "Snap Seq      : #%d  (delta from #%d, gap %d)\n", cl.snap.messageNum, cl.snap.deltaNum, cl.snap.messageNum - cl.snap.deltaNum ); SCR_WriteLog( line );
	Com_sprintf( line, sizeof(line), "Drop Rate     : %d pkt/s\n", netMonDropRate ); SCR_WriteLog( line );
	Com_sprintf( line, sizeof(line), "In Rate       : %d B/s  (%.2f KB/s)\n", netMonInRate, netMonInRate / 1024.0f ); SCR_WriteLog( line );
	Com_sprintf( line, sizeof(line), "Out Rate      : %d B/s  (%.2f KB/s)\n", netMonOutRate, netMonOutRate / 1024.0f ); SCR_WriteLog( line );

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

#define NM_PING_HIGH   150
#define NM_PING_MEDIUM  80
#define NM_COLS 24
#define NM_ROWS 10

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
SCR_NetMonUpdate

Runs every frame at CA_ACTIVE regardless of cl_netgraph.
Aggregates 1-second stats.
==============
*/
static void SCR_NetMonUpdate( void ) {
	if ( cls.state != CA_ACTIVE )
		return;

	if ( netMonLastUpdate == 0 || cls.realtime - netMonLastUpdate >= 1000 ) {
		int ftAvg      = ( netMonFtCount > 0 ) ? ( netMonFtSum / netMonFtCount ) : 0;
		int ftMax      = netMonFtValid   ? netMonFtMax  : 0;
		int snapGapAvg = ( netMonSnapGapCount > 0 ) ? ( netMonSnapGapSum / netMonSnapGapCount ) : 0;
		int snapGapMax = netMonSnapGapMax;
		int snapCount  = netMonSnapGapCount; // snaps received this second (for accurate Hz display)
		int capHits    = netMonCapHits;
		int extrapCnt  = netMonExtrapCount;
		int pingAvg    = ( netMonPingCount > 0 ) ? ( netMonPingSum / netMonPingCount ) : cl.snap.ping;
		int pingMin    = netMonPingValid  ? netMonPingMin : cl.snap.ping;
		int pingMax    = netMonPingValid  ? netMonPingMax : cl.snap.ping;
		int chokeCnt   = netMonChokeCount;
		int fastCnt    = netMonFastCount;
		int resetCnt   = netMonResetCount;
		int slowCnt    = netMonSlowCount;
		int ftMin      = netMonFtValid   ? netMonFtMin  : 0;
		int dtMin      = netMonDtValid   ? netMonDtMin  : cl.serverTimeDelta;
		int dtMax      = netMonDtValid   ? netMonDtMax  : cl.serverTimeDelta;
		int snapHz;

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
		netMonChokeCount  = 0;
		netMonDtValid     = qfalse;
		netMonPingSum     = 0;
		netMonPingCount   = 0;
		netMonPingValid   = qfalse;
		netMonFastCount   = 0;
		netMonResetCount  = 0;
		netMonSlowRate    = slowCnt < 0 ? -slowCnt : slowCnt;
		netMonSlowCount   = 0;

		netMonDispPingMin    = pingMin;
		netMonDispPingMax    = pingMax;
		netMonDispCapHits    = capHits;
		netMonDispExtrapCnt  = extrapCnt;
		netMonDispSnapGapAvg = snapGapAvg;
		netMonDispSnapGapMax = snapGapMax;
		netMonDispSnapCount  = snapCount;
		netMonDispFtAvg      = ftAvg;
		netMonDispFtMax      = ftMax;
		netMonDispFiAvg      = ( netMonFiCount > 0 ) ? ( netMonFiSum / netMonFiCount ) : 0.0f;
		netMonFiSum          = 0.0f;
		netMonFiCount        = 0;

		/* record into 30-second ring buffer for laggot announce */
		if ( cl_netgraph->integer ) {
			laggotSample_t *s = &laggotHistory[ laggotHistoryIdx % LAGGOT_HISTORY ];
			s->dropRate   = netMonDropRate;
			s->fastCnt    = fastCnt;
			s->snapGapAvg = snapGapAvg;
			s->snapGapMax = snapGapMax;
			s->extrapCnt  = extrapCnt;
			s->ftAvg      = ftAvg;
			s->chokeCnt   = chokeCnt;
			laggotHistoryIdx++;
		}

		/* optional periodic stats line in the log */
		if ( cl_netlog->integer >= 2 && netLogFile ) {
			qtime_t t;
			char    logline[256];
			Com_RealTime( &t );
			snapHz = (snapCount > 0) ? snapCount : ((cl.snapshotMsec > 0) ? (1000 / cl.snapshotMsec) : 0);
			Com_sprintf( logline, sizeof(logline),
				"[%02d:%02d:%02d] STATS  snap=%dHz  ping=%d(%d..%d)ms  fI=%.3f"
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

		/* laggot announce: scan 30s ring buffer for worst values */
		if ( cl_netgraph->integer && cl_laggotannounce->integer && cls.realtime - laggotLastAnnounce >= 30000 ) {
			int  sHz = (snapCount > 0) ? snapCount : ((cl.snapshotMsec > 0) ? (1000 / cl.snapshotMsec) : 60);
			int  extrapThresh = sHz * 3 / 4;
			int  worstDrop = 0, worstFast = 0, worstSnapGapAvg = 0, worstSnapGapMax = 0;
			int  worstExtrap = 0, worstFtAvg = 0, worstChoke = 0;
			int  i, count;
			char msg[128];
			int  len = 0;
			qboolean fire = qfalse;

			count = laggotHistoryIdx < LAGGOT_HISTORY ? laggotHistoryIdx : LAGGOT_HISTORY;
			for ( i = 0; i < count; i++ ) {
				laggotSample_t *s = &laggotHistory[ ( laggotHistoryIdx - count + i ) % LAGGOT_HISTORY ];
				if ( s->dropRate   > worstDrop )       worstDrop       = s->dropRate;
				if ( s->fastCnt    > worstFast )       worstFast       = s->fastCnt;
				if ( s->snapGapAvg > worstSnapGapAvg ) worstSnapGapAvg = s->snapGapAvg;
				if ( s->snapGapMax > worstSnapGapMax ) worstSnapGapMax = s->snapGapMax;
				if ( s->extrapCnt  > worstExtrap )     worstExtrap     = s->extrapCnt;
				if ( s->ftAvg      > worstFtAvg )      worstFtAvg      = s->ftAvg;
				if ( s->chokeCnt   > worstChoke )      worstChoke      = s->chokeCnt;
			}

			len += Com_sprintf( msg + len, sizeof(msg) - len, "say [NET]" );

			if ( worstDrop > 0 ) {
				len += Com_sprintf( msg + len, sizeof(msg) - len, " Drop:%d/s", worstDrop );
				fire = qtrue;
			}
			if ( worstFast > 0 ) {
				len += Com_sprintf( msg + len, sizeof(msg) - len, " FastRst:%d", worstFast );
				fire = qtrue;
			}
			if ( worstSnapGapMax > cl.snapshotMsec ) {
				len += Com_sprintf( msg + len, sizeof(msg) - len, " SnapJitt:%d/%dms", worstSnapGapAvg, worstSnapGapMax );
				fire = qtrue;
			}
			if ( worstExtrap > extrapThresh ) {
				len += Com_sprintf( msg + len, sizeof(msg) - len, " Ext:%d/%dHz", worstExtrap, sHz );
				fire = qtrue;
			}
			if ( worstFtAvg > cl.snapshotMsec * 2 ) {
				len += Com_sprintf( msg + len, sizeof(msg) - len, " LowFPS:%dfps", worstFtAvg > 0 ? 1000 / worstFtAvg : 0 );
				fire = qtrue;
			}
			if ( worstChoke > 0 ) {
				len += Com_sprintf( msg + len, sizeof(msg) - len, " Choke:%d/s", worstChoke );
				fire = qtrue;
			}

			if ( fire ) {
				CL_AddReliableCommand( msg, qfalse );
				laggotLastAnnounce = cls.realtime;
			}
		}
	}
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

	scale = cl_netgraph_scale->value;
	if ( scale <= 0.0f ) scale = 1.0f;

	charW = 8.0f * scale;
	charH = 8.0f * scale;
	pad   = charW * 0.5f;

	bw = NM_COLS * charW + pad * 2.0f;
	bh = NM_ROWS * charH + pad * 2.0f;

	bx = cl_netgraph_x->value;
	by = cl_netgraph_y->value;

	if ( bx + bw > SCREEN_WIDTH  - 2 ) bx = SCREEN_WIDTH  - 2 - bw;
	if ( bx < 2.0f                    ) bx = 2.0f;
	if ( by + bh > SCREEN_HEIGHT - 2 ) by = SCREEN_HEIGHT - 2 - bh;
	if ( by < 2.0f                    ) by = 2.0f;

	SCR_FillRect( bx, by, bw, bh, bgColor );

	tx = bx + pad;
	ty = by + pad;

	snapHz = (netMonDispSnapCount > 0) ? netMonDispSnapCount : ((cl.snapshotMsec > 0) ? (1000 / cl.snapshotMsec) : 0);

	/* row 1 - title */
	NM_DrawRow( &tx, &ty, bx + pad, charW, charH, colorWhite,
		"== NET MONITOR ==" );

	/* row 2 - snapshot rate */
	Com_sprintf( line, sizeof(line), "Snap: %3dHz %3dms", snapHz, cl.snapshotMsec );
	NM_DrawRow( &tx, &ty, bx + pad, charW, charH, colorWhite, line );

	/* row 3 - ping with min..max range */
	col = ( cl.snap.ping >= NM_PING_HIGH   ) ? colorRed    :
	      ( cl.snap.ping >= NM_PING_MEDIUM ) ? colorYellow : colorGreen;
	Com_sprintf( line, sizeof(line), "Ping:%d(%d..%d)ms",
		cl.snap.ping, netMonDispPingMin, netMonDispPingMax );
	NM_DrawRow( &tx, &ty, bx + pad, charW, charH, col, line );

	/* row 4 - frame interpolation + client frame time */
	col = ( netMonDispFtMax > cl.snapshotMsec ) ? colorYellow : colorGreen;
	Com_sprintf( line, sizeof(line), "FrmI:.%02d FrmT:%d/%dms",
		(int)( netMonDispFiAvg * 100.0f ),
		netMonDispFtAvg, netMonDispFtMax );
	NM_DrawRow( &tx, &ty, bx + pad, charW, charH, col, line );

	/* row 5 - server time delta */
	Com_sprintf( line, sizeof(line), "DeltaT:  %+dms", cl.serverTimeDelta );
	NM_DrawRow( &tx, &ty, bx + pad, charW, charH, colorWhite, line );

	/* row 6 - drops + extrapolations + caps */
	{
		int nmSnapHz = (netMonDispSnapCount > 0) ? netMonDispSnapCount : ((cl.snapshotMsec > 0) ? (1000 / cl.snapshotMsec) : 60);
		int extrapYellow, extrapRed;
		// At high fps (snapshotMsec < 30, e.g. 60Hz) the Ext equilibrium is ~snapsHz/2
		// (design intent: stable serverTimeDelta).  Raise alert thresholds accordingly
		// to avoid spurious yellow/red for normal steady-state behaviour.
		// At low fps (snapshotMsec >= 30) equilibrium is ~snapsHz/3; keep tighter thresholds.
		if ( cl.snapshotMsec > 0 && cl.snapshotMsec < 30 ) {
			extrapYellow = nmSnapHz * 3 / 4; // ~1.5x the 50% equilibrium
			extrapRed    = nmSnapHz;          // every frame = definitely broken
		} else {
			extrapYellow = nmSnapHz * 3 / 5; // ~1.8x the 33% equilibrium (unchanged)
			extrapRed    = nmSnapHz * 3 / 4; // ~2.25x the 33% equilibrium (unchanged)
		}
		col = ( netMonDropRate > 0 ) ? colorRed :
		      ( netMonDispExtrapCnt > extrapRed ) ? colorRed :
		      ( netMonDispExtrapCnt > extrapYellow ) ? colorYellow : colorGreen;
	}
	Com_sprintf( line, sizeof(line), "Drop:%d Ext:%d Clp:%d",
		netMonDropRate, netMonDispExtrapCnt, netMonDispCapHits );
	NM_DrawRow( &tx, &ty, bx + pad, charW, charH, col, line );

	/* row 7 - bandwidth in/out */
	if ( netMonInRate >= 1024 || netMonOutRate >= 1024 )
		Com_sprintf( line, sizeof(line), "I:%.0fK O:%.0fK",
			netMonInRate / 1024.0f, netMonOutRate / 1024.0f );
	else
		Com_sprintf( line, sizeof(line), "I:%dB O:%dB",
			netMonInRate, netMonOutRate );
	NM_DrawRow( &tx, &ty, bx + pad, charW, charH, colorWhite, line );

	/* row 8 - snap interval jitter */
	col = ( netMonDispSnapGapMax > cl.snapshotMsec ) ? colorRed :
	      ( netMonDispSnapGapAvg > cl.snapshotMsec / 2 ) ? colorYellow : colorGreen;
	Com_sprintf( line, sizeof(line), "SnapJitt:%d/%dms",
		netMonDispSnapGapAvg, netMonDispSnapGapMax );
	NM_DrawRow( &tx, &ty, bx + pad, charW, charH, col, line );

	/* row 9 - snapshot sequence / delta compression */
	Com_sprintf( line, sizeof(line), "Seq:  #%d d:%d",
		cl.snap.messageNum,
		cl.snap.messageNum - cl.snap.deltaNum );
	NM_DrawRow( &tx, &ty, bx + pad, charW, charH, colorWhite, line );

	/* row 10 - slow-path drift + fast resets */
	{
		int slowThresh = snapHz / 10;
		if ( slowThresh < 1 ) slowThresh = 1;
		col = ( netMonFastCount > 0 ) ? colorRed :
		      ( netMonSlowRate > slowThresh ) ? colorYellow : colorGreen;
		Com_sprintf( line, sizeof(line), "Adj: slo=%d fst=%d", netMonSlowRate, netMonFastCount );
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
	cl_debuggraph = Cvar_Get ("debuggraph", "0", CVAR_CHEAT);
	cl_graphheight = Cvar_Get ("graphheight", "32", CVAR_CHEAT);
	cl_graphscale = Cvar_Get ("graphscale", "1", CVAR_CHEAT);
	cl_graphshift = Cvar_Get ("graphshift", "0", CVAR_CHEAT);

	cl_netgraph = Cvar_Get( "cl_netgraph", "0", CVAR_PRIVATE );
	Cvar_SetDescription( cl_netgraph,
		"Show the net monitor overlay.\n"
		"0 = off, 1 = on\n"
		"Default: 0" );

	cl_netgraph_x = Cvar_Get( "cl_netgraph_x", "460", CVAR_PRIVATE );
	Cvar_SetDescription( cl_netgraph_x, "Net monitor X position in virtual 640x480 coords.\nDefault: 460" );

	cl_netgraph_y = Cvar_Get( "cl_netgraph_y", "4", CVAR_PRIVATE );
	Cvar_SetDescription( cl_netgraph_y, "Net monitor Y position in virtual 640x480 coords.\nDefault: 4" );

	cl_netgraph_scale = Cvar_Get( "cl_netgraph_scale", "1.0", CVAR_PRIVATE );
	Cvar_SetDescription( cl_netgraph_scale, "Net monitor text/box scale multiplier.\nDefault: 1.0" );

	cl_netlog = Cvar_Get( "cl_netlog", "0", CVAR_PRIVATE );
	Cvar_SetDescription( cl_netlog,
		"Net debug session logging to netdebug_YYYYMMDD_HHMMSS.log.\n"
		"0 = off\n"
		"1 = CONNECT info, every server command (SVCMD), FAST/RESET delta,\n"
		"    SNAP LATE, PING JITTER, TIMEOUT counter, DISCONNECT context dump\n"
		"2 = level 1 + per-snapshot state (SNAP) + periodic per-second stats\n"
		"Note: DISCONNECT context is always printed to console regardless of this setting.\n"
		"Default: 0" );

	cl_laggotannounce = Cvar_Get( "cl_laggotannounce", "1", CVAR_PRIVATE );
	Cvar_SetDescription( cl_laggotannounce,
		"Auto-announce network issues to the server via say.\n"
		"0 = off, 1 = on\n"
		"Default: 1" );

	Cmd_AddCommand( "netgraph_dump", SCR_NetgraphDump_f );

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
static void SCR_DrawScreenField( stereoFrame_t stereoFrame ) {
	qboolean uiFullscreen;

	re.BeginFrame( stereoFrame );

	uiFullscreen = (uivm && VM_Call( uivm, 0, UI_IS_FULLSCREEN ));

	// wide aspect ratio screens need to have the sides cleared
	// unless they are displaying game renderings
	if ( uiFullscreen || cls.state < CA_LOADING ) {
		if ( cls.glconfig.vidWidth * 480 > cls.glconfig.vidHeight * 640 ) {
			// draw vertical bars on sides for legacy mods
			const int w = (cls.glconfig.vidWidth - ((cls.glconfig.vidHeight * 640) / 480)) /2;
			re.SetColor( g_color_table[ ColorIndex( COLOR_BLACK ) ] );
			re.DrawStretchPic( 0, 0, w, cls.glconfig.vidHeight, 0, 0, 0, 0, cls.whiteShader );
			re.DrawStretchPic( cls.glconfig.vidWidth - w, 0, w, cls.glconfig.vidHeight, 0, 0, 0, 0, cls.whiteShader );
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
			SCR_NetMonUpdate();
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
