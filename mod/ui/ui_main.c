// Copyright (C) 1999-2000 Id Software, Inc.

//
/*
=======================================================================

USER INTERFACE MAIN

=======================================================================
*/

// use this to get a demo build without an explicit demo build, i.e. to get the demo ui files to build
#include "ui_local.h"
#include "../common/c_bmalloc.h"
#include "../game/bg_gear.h"

uiInfo_t	   uiInfo;

// reserve 8MB memory for bmalloc
#define MALLOC_POOL_SIZE (1024*1024*8)
char ui_malloc_pool[MALLOC_POOL_SIZE];

static char    defferedConfig[20]	   = "";

#ifdef USE_AUTH
//@Barbatos //@Kalish

/*
The initializer doesn't work for initializing as far as I can tell .. so we still init at runtime
*BUT* .. not putting an initializer triggers an x86_64 JIT (vm_ui 2) bug where the variable is read-only to some random data
 */
//netadr_t	auth_server_address = { 0, { 0, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, { 0, 0 }, 0 };

//@r00t
// Let's do it this way
static char auth_adr_data[sizeof(netadr_t)] = "";
netadr_t *auth_server_address = (netadr_t*)auth_adr_data;


vmCvar_t		authc;
vmCvar_t		cl_auth;
vmCvar_t		cl_auth_enable;
vmCvar_t		cl_auth_login;
vmCvar_t		cl_auth_notoriety;
vmCvar_t		cl_auth_key;
vmCvar_t		cl_auth_engine;
vmCvar_t		cl_auth_status;
vmCvar_t		cl_auth_time;

vmCvar_t	ui_mainMenu;

#endif


static const int   ItemTable[ITEM_MAX + 2] = {
	UT_ITEM_NONE,
	UT_ITEM_KNIFE,
	UT_ITEM_BERETTA,
	UT_ITEM_DEAGLE,
	UT_ITEM_SPAS12,
	UT_ITEM_MP5K,
	UT_ITEM_UMP45,
	UT_ITEM_HK69,
	UT_ITEM_LR,
	UT_ITEM_G36,
	UT_ITEM_AK103,
	UT_ITEM_PSG1,
	UT_ITEM_GRENADE_HE,
	UT_ITEM_GRENADE_FLASH,
	UT_ITEM_GRENADE_SMOKE,
	UT_ITEM_VEST,
	UT_ITEM_NVG,
	UT_ITEM_MEDKIT,
	UT_ITEM_SILENCER,
	UT_ITEM_LASER,
	UT_ITEM_HELMET,
	UT_ITEM_AMMO,
	UT_ITEM_APR,
	UT_ITEM_SR8,
	UT_ITEM_NEGEV,
	UT_ITEM_GRENADE_FRAG,
	UT_ITEM_M4,
	UT_ITEM_GLOCK,
	UT_ITEM_COLT1911,
	UT_ITEM_MAC11,
	/*UT_ITEM_P90,
	UT_ITEM_BENELLI,
	UT_ITEM_MAGNUM,

	UT_ITEM_DUAL_BERETTA,
	UT_ITEM_DUAL_DEAGLE,
	UT_ITEM_DUAL_GLOCK,
	UT_ITEM_DUAL_MAGNUM,*/
};

static const int   GearTable[GEAR_MAX][20] = {
	{
		UT_ITEM_LR,
		UT_ITEM_G36,
		UT_ITEM_AK103,
		UT_ITEM_PSG1,
		UT_ITEM_SR8,
		UT_ITEM_HK69,
		UT_ITEM_NEGEV,
		UT_ITEM_M4,
		UT_ITEM_SPAS12,
		UT_ITEM_MP5K,
		UT_ITEM_UMP45,
		UT_ITEM_MAC11,
		//UT_ITEM_BENELLI,
		//UT_ITEM_P90,
		UT_ITEM_NONE,
		-1,
	},

	{
		UT_ITEM_SPAS12,
		UT_ITEM_MP5K,
		UT_ITEM_UMP45,
		UT_ITEM_MAC11,
		//UT_ITEM_BENELLI,
		//UT_ITEM_P90,
		UT_ITEM_NONE,
		/*UT_ITEM_DUAL_BERETTA,
		UT_ITEM_DUAL_DEAGLE,
		UT_ITEM_DUAL_GLOCK,
		UT_ITEM_DUAL_MAGNUM,*/
		-1,
	},

	{
		UT_ITEM_BERETTA,
		UT_ITEM_DEAGLE,
		UT_ITEM_GLOCK,
		UT_ITEM_COLT1911,
		//UT_ITEM_MAGNUM,
		-1,
	},

	{
		UT_ITEM_GRENADE_HE,
		UT_ITEM_GRENADE_FLASH,
		UT_ITEM_GRENADE_SMOKE,
		UT_ITEM_NONE,
		-1,
	},

	{
		UT_ITEM_VEST,
		UT_ITEM_MEDKIT,
		UT_ITEM_HELMET,
		UT_ITEM_SILENCER,
		UT_ITEM_LASER,
		UT_ITEM_NVG,
		UT_ITEM_AMMO,
		-1,
	},

	{
		UT_ITEM_NONE,
		UT_ITEM_VEST,
		UT_ITEM_MEDKIT,
		UT_ITEM_HELMET,
		UT_ITEM_SILENCER,
		UT_ITEM_LASER,
		UT_ITEM_NVG,
		UT_ITEM_AMMO,
		-1,
	},

	{
		UT_ITEM_NONE,
		UT_ITEM_VEST,
		UT_ITEM_MEDKIT,
		UT_ITEM_HELMET,
		UT_ITEM_SILENCER,
		UT_ITEM_LASER,
		UT_ITEM_NVG,
		UT_ITEM_AMMO,
		-1,
	},
};

static const char  *MonthAbbrev[] = {
	"Jan", "Feb", "Mar",
	"Apr", "May", "Jun",
	"Jul", "Aug", "Sep",
	"Oct", "Nov", "Dec"
};

static const char  *STATS_CATAGORYS[] = {
	"SUMMARY",	  //0
	"AWARDS",
	"MISC DEATHS",
	"MELEE",
	"EXPLOSIONS",
	"G36",
	"AK103",
	"LR300",
	"M4",
	"MP5K",
	"UMP",
	"SR8",
	"PSG-1",
	"DE",
	"BERETTA",
	"NEGEV",
	"SPAS",
	"GLOCK",
	"COLT",
	"MAC11",
	/*"P90",
	"BENELLI",
	"MAGNUM",*/
	"CTF",
	"BOMBMODE"	 //21
};
#define MAX_STATS_CATAGORYS sizeof(STATS_CATAGORYS)/sizeof(const char *)

#define MAX_STATS_TEXT		16

char				 STATS_TEXTLINES[MAX_STATS_TEXT][128];

static const char		 *skillLevels[] = {
	"I Can Win",
	"Bring It On",
	"Hurt Me Plenty",
	"Hardcore",
	"Nightmare"
};

static const int		 numSkillLevels = sizeof(skillLevels) / sizeof(const char *);

static const char		 *netSources[]	= {
	"Local",
	"Mplayer",
	"Internet",
	"Favorites"
};
static const int		 numNetSources	 = sizeof(netSources) / sizeof(const char *);

static const serverFilter_t  serverFilters[] = {
	{ "Urban Terror", GAME_NAME },
};

static const char	   *teamArenaGameTypes[] = {
	"FFA",
	"LMS",
//	"SP",
	"TRAIN",
	"TEAM DM",
	"SURVIVOR",
	"FOLLOWLEADER",
	"CAPnHOLD",
	"CTF",
	"BOMB",
	"JUMP"
};

static int const		 numTeamArenaGameTypes = sizeof(teamArenaGameTypes) / sizeof(const char *);

static const int		 numServerFilters	   = sizeof(serverFilters) / sizeof(serverFilter_t);

/*
static const char		 *sortKeys[]	   = {
	"Server Name",
	"Map Name",
	"Open Player Spots",
	"Game Type",
	"Ping Time"
};
*/
//static const int		 numSortKeys = sizeof(sortKeys) / sizeof(const char *);

static char 		 *netnames[] = {
	"???",
	"UDP",
	"IPX",
	NULL
};

//static char			   quake3worldMessage[] = "Visit www.quake3world.com - News, Community, Events, Files";

static int			 gamecodetoui[]   = { 4, 2, 3, 0, 5, 1, 6};
static int			 uitogamecode[] = { 4, 6, 2, 3, 1, 5, 7};

static void 		 UI_StartServerRefresh(qboolean full);
static void 		 UI_StopServerRefresh( void );
static void 		 UI_DoServerRefresh( void );
static void 		 UI_FeederSelection(float feederID, int index);
static void 		 UI_BuildServerDisplayList(qboolean force);
static void 		 UI_BuildServerStatus(qboolean force);
static void 		 UI_BuildFindPlayerList(qboolean force);
static int QDECL	 UI_ServersQsortCompare( const void *arg1, const void *arg2 );
static int			 UI_MapCountByGameType(qboolean singlePlayer);
static void 		 UI_ParseGameInfo(const char *teamFile);
static void 		 UI_ParseTeamInfo(const char *teamFile);
static const char *  UI_SelectedMap(int index, int *actual);
static int			 UI_GetIndexFromSelection(int actual);

int 	   ProcessNewUI( int command, int arg0, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6 );

vmCvar_t	 ui_new;
vmCvar_t	 ui_debug;
vmCvar_t	 ui_initialized;
vmCvar_t	 ui_teamArenaFirstRun;

static qboolean  updateModel = qtrue;
static qboolean  q3Model	 = qfalse;

void	 _UI_Init( qboolean );
void	 _UI_Shutdown( void );
void	 _UI_KeyEvent( int key, qboolean down );
void	 _UI_MouseEvent( int dx, int dy );
void	 _UI_Refresh( int realtime );
qboolean _UI_IsFullscreen( void );


/**
 * vmMain
 *
 * This is the only way control passes into the module.
 * This must be the very first function compiled into the .qvm file
 *
 */

DLLEXPORT int vmMain( int command, int arg0, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8, int arg9, int arg10, int arg11  )
{
	switch (command)
	{
		case UI_GETAPIVERSION:
			return UI_API_VERSION;

		case UI_INIT:
			_UI_Init(arg0);
			return 0;

		case UI_SHUTDOWN:
			_UI_Shutdown();
			return 0;

		case UI_KEY_EVENT:
			_UI_KeyEvent( arg0, arg1 );
			return 0;

		case UI_MOUSE_EVENT:
			_UI_MouseEvent( arg0, arg1 );
			return 0;

		case UI_REFRESH:
			_UI_Refresh( arg0 );
			return 0;

		case UI_IS_FULLSCREEN:
			return _UI_IsFullscreen();

		case UI_SET_ACTIVE_MENU:
			_UI_SetActiveMenu( arg0 );
			return 0;

		case UI_CONSOLE_COMMAND:
			return UI_ConsoleCommand(arg0);

		case UI_DRAW_CONNECT_SCREEN:
			UI_DrawConnectScreen( arg0 );
			return 0;

		case UI_HASUNIQUECDKEY: 				// mod authors need to observe this
			return qfalse;					// dokta8: changed to false for UT

		#ifdef USE_AUTH
		//@Barbatos @Kalish
		case UI_AUTHSERVER_PACKET:
			CL_Authserver_Get_Packet();
			return 0;
		#endif

	}

	return -1;
}



#ifdef USE_AUTH
//@Barbatos //@Kalish

#include "../game/auth_shared.c"

/**
 * CL_Auth_Key_Check
*/

int CL_Auth_Key_Check( char* string )
{
	int 	i;

	#ifdef DEBUG
	Com_Printf( "CL_Auth_Key_Check() \n");
	#endif

	if( !Q_stricmp( string, AUTH_KEY_DEFAULT ) || !Q_stricmp( string, "0" ) )
		return -1;

	if( strlen( string ) != AUTH_KEY_SIZE  ) {
		return 0;
	} else {
		for(i=0;i<AUTH_KEY_SIZE;string++,i++) {
			if( string[0]<0x30 || ( string[0]>0x39 && string[0]<0x41 ) || string[0]>0x5A ) {
				return 0;
			}
		}
	}
	return 1;
}

/**
 * CL_Auth_Key_Read
*/

int CL_Auth_Key_Read( void )
{
	fileHandle_t	f;
	char			fpath[MAX_OSPATH];
	char			buffer[AUTH_KEY_SIZE+1];
	int 			i;

	#ifdef DEBUG
	Com_Printf( "CL_Auth_Key_Read() \n");
	#endif

	// if( !strcmp(UI_Cvar_VariableString("cl_auth_key","0") )

	if( cl_auth.integer==1 ) // read only once
	{

		Com_sprintf(fpath, sizeof( fpath ), "%s", AUTH_KEY_FILE);

		trap_FS_FOpenFile( fpath, &f, FS_READ );
		if ( !f )
		{
			Com_Printf( "The key file \"%s\" doesn't exist. \n", fpath );
			trap_Cvar_Set("cl_auth_key", "");
			CL_Auth_Cvars_Set( -10, "" ); // error
			return -1;
		}

		memset( buffer, 0, sizeof(buffer) );

		trap_FS_Read( buffer, AUTH_KEY_SIZE, f );
		trap_FS_FCloseFile( f );

		i = CL_Auth_Key_Check( buffer );

		if ( i==1 )
		{
			#ifdef DEBUG
			Com_Printf( "The key in the file \"%s\" seems to be valid. \n", fpath );
			#endif
			trap_Cvar_Set("cl_auth_key", buffer);
			// CL_Auth_Cvars_Set( 1, "" ); // same
			return 1;
		}
		else if ( i==-1 )
		{
			Com_Printf( "The key in the file \"%s\" is empty. \n", fpath );
			trap_Cvar_Set("cl_auth_key", "");
			CL_Auth_Cvars_Set( -11, "" ); // error
			return 0;
		}
		else
		{
			Com_Printf( "The key in the file \"%s\" is not valid. \n", fpath );
			trap_Cvar_Set("cl_auth_key", "");
			CL_Auth_Cvars_Set( -12, "" ); // error
			return -1;
		}
	}

	return 0;
}

/**
 * CL_Auth_Key_Write
*/

void CL_Auth_Key_Write( char* string )
{
	fileHandle_t	f;
	char			fpath[MAX_OSPATH];
	char			buffer[1024];
	int 			test;
	/*
	#ifndef _WIN32
		mode_t		savedumask;
	#endif
	*/

	#ifdef DEBUG
	Com_Printf( "CL_Auth_Key_Write() \n");
	#endif

	test = CL_Auth_Key_Check( string );

	if ( test==-1 )
	{
		CL_Auth_Cvars_Set( -11, "" );
	}
	else if( cl_auth.integer>0 )
	{
		Com_Printf( AUTH_PREFIX" Your key has already been sent. You need to restart the game to change it. \n");
		return;
	}

	if( test==1 || test==-1 )
	{
		if ( test==-1 )
		{
			Com_Printf( AUTH_PREFIX" Your key is now deleted. \n");
			Q_strncpyz( string, AUTH_KEY_DEFAULT, AUTH_KEY_SIZE+3 );
		}


		/*
		#ifndef _WIN32
			savedumask = umask(0077);
		#endif
		*/

		Com_sprintf(fpath, sizeof( fpath ), "%s", AUTH_KEY_FILE);

		if(trap_FS_FOpenFile( fpath, &f, FS_WRITE) < 0)
		{
			Com_Printf (AUTH_PREFIX" Error, couldn't write key to \"%s\".\n", fpath );
			CL_Auth_Cvars_Set( -13, "" );
			goto out;
		}

		Com_sprintf(
			buffer, sizeof( buffer ),
			"%s\n\n// Generated locally by Urban Terror \n// Do NOT modify. Do NOT give this file to ANYONE \n// More info : http://" AUTH_WEBSITE "\n",
			string
		);

		trap_FS_Write( buffer, strlen( buffer ), f );
		trap_FS_FCloseFile( f );

		out:

		/*
		#ifndef _WIN32
			*umask(savedumask);
		#endif
		*/

		return;
	}
}

/**
 * CL_Auth_Cvars_Set
*/

void CL_Auth_Cvars_Set( int step, const char *string )
{
	/*

	VALID:

	0		default at init
	1		engine auth capable
	2		sent to authserver (try 1)
	..		sent to authserver (try x)
	10		validated by authserver

	CLIENT ERRORS:

	-1		engine not auth capable
	-2		cl_auth_enable 0
	-3		dedicated server
	-9		code error

	FILE ERRORS :

	-10 	no key file
	-11 	emty key file
	-12 	not valid key file
	-13 	key file cant be created

	AUTH ERRORS:

	-20 	authserver time out
	-21 	rejected by authserver
	-22 	update required
	-23 	authserver adress problem
	-24 	invalid request
	-25 	invalid header
	-26 	crypt too long

	*/

  // TMP
	#ifdef DEBUG
	Com_Printf( "CL_Auth_Cvars_Set(%i,\"%s%s\") \n", step, string, S_COLOR_WHITE);
	#endif

	trap_Cvar_SetValue("cl_auth", step);
	cl_auth.integer = step;

	trap_Cvar_Set("cl_auth_status", string);

	if(step<1)
	{
		trap_Cvar_Set("authc", "0");
		trap_Cvar_Set("cl_auth_login", "0");
		trap_Cvar_Set("cl_auth_notoriety", "0");
		trap_Cvar_Set("cl_auth_key", "0");
	}

}

/**
 * CL_Auth_Refresh
*/

void CL_Auth_Refresh( void )
{
	static int auth_no_spam = 0; //@r00t: The first two auth lines were spammed like 100 times when starting local server

	#ifdef DEBUG
	Com_Printf( "CL_Auth_Refresh() \n");
	#endif

	// auth init : part 1

	if( cl_auth.integer==0 )
	{
		if(trap_Cvar_VariableValue("dedicated") > 0)
		{
			CL_Auth_Cvars_Set( -3, "" );
		}
		else if(cl_auth_engine.integer!=1)
		{
			// engine not auth ready
			Com_Printf( "------ Urban Terror Auth System ------\n");
			Com_Printf( "This game client is not auth capable. \nYou will have to use the official game client instead to be able to auth. \n");
			Com_Printf( "--------------------------------------\n");
			CL_Auth_Cvars_Set( -1, "" );
		}
		else if( cl_auth_enable.integer!=1 )
		{
			Com_Printf( "------ Urban Terror Auth System ------\n");
			Com_Printf( "Auth functions are disabled. \nTo enable them, set cl_auth_enable to 1 and restart the game. \n");
			Com_Printf( "--------------------------------------\n");
			CL_Auth_Cvars_Set( -2, "Auth disabled" );
		}
		else
		{
			CL_Auth_Cvars_Set( 1, "");
		}
		auth_no_spam = 0;
	}

	// auth init : part 2

	if( cl_auth.integer==1 )
	{
		if (!auth_no_spam) Com_Printf( "------ Urban Terror Auth System ------\n");

		CL_Auth_Key_Read(); // check key, load cl_auth_key, change cl_auth on error

		if( cl_auth.integer!=1 )
		{
			if ( trap_Key_GetCatcher( ) & KEYCATCH_CONSOLE )
			{
				Com_Printf( "Your auth key is not valid. \n");
				Com_Printf( "You can edit it using the command /"AUTH_CMD_CL" \n");
				Com_Printf( "You can delete it using the command /"AUTH_CMD_CL" 0 \n");
				Com_Printf( "You can find also help on the website: \nhttp://"AUTH_WEBSITE" \n");
			}
			/*
			else
			{
				if( ui_mainMenu.integer==1 )
					Com_Error (ERR_DROP,"Your auth key is not valid. \n\nYou can edit it using the login/logout menu. \n\nYou can also find help on the website http://"AUTH_WEBSITE" ");
			}
			*/
		}
		else
		{
			CL_Auth_Request_Authorization();
		}

		if (!auth_no_spam) Com_Printf( "--------------------------------------\n");
		auth_no_spam = 1;
	}

	CL_Auth_Request_Authorization();

	// display auth infos

	if( ui_mainMenu.integer==1 )
	{
		UI_Text_Paint(10, 405, 0.23f, colorWhite, UI_Cvar_VariableString("cl_auth_status"), 0, 0, 0);
	}
}

/**
 * CL_Auth_Cmd_f
*/

void CL_Auth_Cmd_f( char *auth_arg )
{
	int 	test;
	char	buffer[AUTH_KEY_SIZE+AUTH_KEY_SIZE];

	#ifdef DEBUG
	Com_Printf( "CL_Auth_Cmd_f() \n");
	#endif

	if(cl_auth.integer>0 || cl_auth.integer<=-10)
	{
		if(!Q_stricmp(auth_arg, "__USE_ARGV__")) {
			if (trap_Argc() != 2) {
				Com_Printf (AUTH_CMD_CL" <text>\n");
				return;
			}
			Q_strncpyz(buffer, UI_Argv(1), sizeof(buffer));
		} else {
			Q_strncpyz(buffer, auth_arg, sizeof(buffer));
		}

		test = CL_Auth_Key_Check( buffer );

		if( test==0 )
		{
			if ( trap_Key_GetCatcher( ) & KEYCATCH_CONSOLE )
			{
				Com_Printf( "------ Urban Terror Auth System ------\n");
				Com_Printf( "Your auth key is not valid. \nYou can find help on the website: http://"AUTH_WEBSITE" \n");
				Com_Printf( "--------------------------------------\n");
			}
			else
			{
				Com_Error (ERR_DROP,"Your auth key is not valid. \n\nYou can find help on the website http://"AUTH_WEBSITE" ");
			}
		}
		else if( test==1 )
		{
			CL_Auth_Key_Write( buffer );
			if(cl_auth.integer>1) {
				// @kalish WE WANT EXE RESTART : we don't want them to be able to test keys easely
				if ( trap_Key_GetCatcher( ) & KEYCATCH_CONSOLE )
				{
					Com_Printf( "------ Urban Terror Auth System ------\n");
					Com_Printf( "Your new auth key is saved. \nTo prevent auth abuses, you cant use it now. \nTo auth with this new key, you will have restart the game. \n");
					Com_Printf( "--------------------------------------\n");
				} else {
					Com_Error (ERR_DROP,"Your new auth key is saved. \n\nTo prevent auth abuses, you can't use it now. \n\nTo auth with this new key, you will have restart the game.");
				}
			} else {
				// let auth !
				CL_Auth_Cvars_Set( 1, "");
				CL_Auth_Refresh();
			}
		}
		else
		{
			CL_Auth_Key_Write( buffer );
			Com_Printf( AUTH_PREFIX" You are logged out. \n");

			if ( trap_Key_GetCatcher( ) & KEYCATCH_CONSOLE ) {
				Com_Printf( "------ Urban Terror Auth System ------\n");
				Com_Printf( "Your auth key will be disabled the next \ntime you will restart the game. \n");
				Com_Printf( "--------------------------------------\n");
			} else {
				Com_Error (ERR_DROP,"Your auth key will be disabled the next time you will restart the game.");
			}

		}
	}
}

/**
 * CL_Authserver_Send_Packet
 */

void QDECL CL_Authserver_Send_Packet( const char *cmd, const char *args )
{
	const char *to;
	char game[16];
	int test;
	char string[AUTH_MAX_STRING_CHARS];
	char crypt[AUTH_MAX_STRING_CRYPT];
	int len;

	trap_Cvar_VariableStringBuffer("fs_game", game, sizeof(game) );

	// build request

	string[0]='C';
	string[1]='L';
	string[2]=':';
	string[3]='\0';

	test = 1;

	if(test>0) test = Auth_cat_str( string, sizeof(string), cmd, 0);
	if(test>0) test = Auth_cat_str( string, sizeof(string), PRODUCT_VERSION, 1);
	if(test>0) test = Auth_cat_str( string, sizeof(string), PLATFORM_VERSION, 1);
	if(test>0) test = Auth_cat_int( string, sizeof(string), trap_Cvar_VariableValue("protocol"), 0);
	if(test>0) test = Auth_cat_str( string, sizeof(string), game, 1);
	if(test>0) test = Auth_cat_str( string, sizeof(string), GAME_NAME, 1);
	if(test>0) test = Auth_cat_str( string, sizeof(string), GAME_VERSION, 1);
	if(test>0) test = Auth_cat_str( string, sizeof(string), UI_Cvar_VariableString("cl_guid"), 1);
	if(test>0) test = Auth_cat_str( string, sizeof(string), UI_Cvar_VariableString("cl_auth_key"), 1);
	if(test>0) test = Auth_cat_str( string, sizeof(string), args, -1);

	if(test<1)
	{
		Com_Printf( AUTH_PREFIX" not sent - malformed");
		CL_Auth_Cvars_Set( -25, "^1Auth error^7: can't send the request (malformed)" ); // error
	}
	else
	{
		// crypt

		len = strlen(string);

		if( 16 + len*4 > sizeof(crypt) ) {
			Com_Printf( AUTH_PREFIX" not sent - packet overruns max length (%i"), sizeof(crypt) );
			CL_Auth_Cvars_Set( -26, "^1Auth error^7: can't send the request (too big)" ); // error
			return;
		}

		Com_sprintf( crypt, sizeof(crypt), "CL:AUTH %i %s", AUTH_PROTOCOL, Auth_encrypt( string ) );

		// send

		to = va("%i.%i.%i.%i:%i", auth_server_address->ip[0], auth_server_address->ip[1],
			auth_server_address->ip[2], auth_server_address->ip[3], AUTH_SERVER_PORT) ;

		trap_NET_SendPacket( NS_CLIENT, strlen( crypt ), crypt, to );
	}
}


/**
 * NET_OutOfBandPrint
 */

void QDECL NET_OutOfBandPrint( netsrc_t sock, netadr_t adr, const char *format, ... )
{
	va_list 	argptr;
	char		string[16384];
	const char *to;

	// set the header
	string[0] = -1;
	string[1] = -1;
	string[2] = -1;
	string[3] = -1;

	va_start( argptr, format );
	vsprintf( string+4, format, argptr );
	va_end( argptr );

	to = va("%i.%i.%i.%i:%i", adr.ip[0], adr.ip[1],
		adr.ip[2], adr.ip[3], (unsigned short *)&adr.port[0]) ;

	// send the datagram
	trap_NET_SendPacket( sock, strlen( string ), string, to );
}

/**
 * CL_Auth_Request_Authorization
*/

void CL_Auth_Request_Authorization( void )
{
	int auth_challenge;
	int test;
	char buffer[128];

	unsigned short *pport;

	//uiClientState_t  cstate;

#ifdef DEBUG
	Com_Printf( "CL_Auth_Request_Authorization() cl_auth %i \n", cl_auth.integer);
#endif

	// check it

	if( cl_auth.integer >= 10 || cl_auth.integer < 1 )
	{
		// not auth enabled, or error, or already validated
		return;
	}

	if( cl_auth.integer > 3 ) // 9 max
	{
		Com_Printf( AUTH_PREFIX" the authserver doesn't answer after %i tries \n", cl_auth.integer-1 );
		Com_Printf( AUTH_PREFIX" please, retry later \n");
		CL_Auth_Cvars_Set( -20, "^1Auth error^7: authserver not available" );
		return;
	}

	if( uiInfo.uiDC.realTime - cl_auth_time.integer < 1000 * Auth_pow( 2, cl_auth.integer ) )
	{
		// wait !
		// Auth_pow( 2, 2 to 9 ) will retry at
		// 4, 8, 16, 32 seconds, 1, 2, 4, 8 minutes
		return;
	}

	pport = (unsigned short *)&auth_server_address->port[0];

	// do it

	if( cl_auth.integer==1 )
	{
		// make sure to initialize
	memset( auth_server_address, 0, sizeof( netadr_t ) );
	if ( *pport != 0 ) {
			// I suspect we're not done with this quite yet, so better have a quick check
			trap_Error( "BUGGED!\n" );
	}
		CL_Auth_Cvars_Set( 2, "Sending request to authserver..." );
	}
	else
	{
		Com_sprintf(buffer, sizeof(buffer), "Sending request to authserver... (try %i)", cl_auth.integer);
		CL_Auth_Cvars_Set( cl_auth.integer + 1, buffer );
	}

	trap_Cvar_SetValue("cl_auth_time", uiInfo.uiDC.realTime );
	cl_auth_time.integer = uiInfo.uiDC.realTime;

	if ( *pport == 0 )
	{
		Com_Printf( AUTH_PREFIX" resolving authserver address \n");

		if (!trap_NET_StringToAdr( AUTH_SERVER_NAME, auth_server_address))
		{
			CL_Auth_Cvars_Set( -23, "^1Auth error^7: couldn't resolve address" );
			Com_Printf( AUTH_PREFIX" couldn't resolve address \n");
			Com_Printf( AUTH_PREFIX" the authserver can't be contacted \n");
			return;
		}

		*pport = AUTH_SERVER_PORT;
		Com_Printf( AUTH_PREFIX" resolved \n", AUTH_PREFIX );

		#ifdef DEBUG
		Com_Printf( AUTH_PREFIX" %s resolved to %i.%i.%i.%i:%i\n", AUTH_SERVER_NAME,
			auth_server_address->ip[0], auth_server_address->ip[1],
			auth_server_address->ip[2], auth_server_address->ip[3],
			AUTH_SERVER_PORT );
		#endif
	}

	if ( auth_server_address->type == NA_BAD )
	{
		CL_Auth_Cvars_Set( -23, "^1Auth error^7: authserver address error" );
		Com_Printf( AUTH_PREFIX" the authserver can't be contacted \n");
		return;
	}

	auth_challenge = Auth_rand_int(1000,1000000000);
	trap_Cvar_Set("authc", va("%i", auth_challenge));

	Com_Printf( AUTH_PREFIX" sending authorize request for your key (try %i.\n"), cl_auth.integer-1 );

	buffer[0] = '\0'; test = 1;

	if(test>0) test = Auth_cat_str( buffer, sizeof(buffer), UI_Cvar_VariableString("name"), 1);
	if(test>0) test = Auth_cat_str( buffer, sizeof(buffer), "0", 1);
	if(test>0) test = Auth_cat_int( buffer, sizeof(buffer), auth_challenge, 1);
	if(test>0) test = Auth_cat_str( buffer, sizeof(buffer), UI_Cvar_VariableString("cl_time"), 1); // r00t: Send hax status on login

	if(test<1)
		CL_Auth_Cvars_Set( -24, "^1Auth error^7: your cvars have invalid content" );
	else
		CL_Authserver_Send_Packet( "CHECK", buffer );
}

/**
 * CL_Authserver_Get_Packet
*/

void CL_Authserver_Get_Packet( void )
{
	char m[1024];
	char s[1024];
	char l[1024];
	char r[1024];
	char u[1024];
	char a[1024];
	char buffer[AUTH_MAX_STRING_CHARS];

	#ifdef DEBUG
	Com_Printf( "CL_Authserver_Get_Packet() \n");
	#endif

	if(atoi(UI_Argv(1)) != AUTH_PROTOCOL)
	{
		Com_Error (ERR_DROP, AUTH_PREFIX" protocol error (%i - contact an admin at http://"AUTH_WEBSITE" \n"), atoi(UI_Argv(1)));
		return;
	}

	Q_strncpyz(buffer, UI_Argv(2), sizeof(buffer));

	Auth_decrypt_args(buffer);

	if(auth_argc<2) {
		Com_Error (ERR_DROP, AUTH_PREFIX" decrypt error - contact an admin at http://"AUTH_WEBSITE" \n");
		return;
	}

	if(auth_argc<8) {
		Com_Error (ERR_DROP, AUTH_PREFIX" auth args error - contact an admin at http://"AUTH_WEBSITE" \n");
		return;
	}

	Q_strncpyz(m, Auth_argv(0), sizeof(m));
	Q_strncpyz(s, Auth_argv(3), sizeof(s));
	Q_strncpyz(l, Auth_argv(4), sizeof(l));
	Q_strncpyz(r, Auth_argv(5), sizeof(r));
	Q_strncpyz(u, Auth_argv(6), sizeof(u));
	Q_strncpyz(a, Auth_argv(7), sizeof(a));

	if ( !Q_stricmp( m, "AUTH:CLIENT" ))
	{
		if ( !Q_stricmp( s, "REJECT" ) )
		{
			CL_Auth_Cvars_Set( -22, Auth_Stripslashes(a) );
			Com_Printf( AUTH_PREFIX" key rejected - account: %s - reason: %s \n", l, r );
			if ( trap_Key_GetCatcher( ) & KEYCATCH_CONSOLE ) {
				//
			} else {
				Com_Error (ERR_DROP, "%s\n", Auth_Stripslashes(u));
			}
			return;
		}

		if ( !Q_stricmp( s, "CHALLENGE" ) )
		{
			CL_Auth_Cvars_Set( 10, Auth_Stripslashes(a));
			trap_Cvar_Set("cl_auth_login", l);
			trap_Cvar_Set("cl_auth_notoriety", r ); // numeric value - can be used it game browser
			// trap_Cvar_Set("cl_auth_notoriety", Auth_Stripslashes(u));
			Com_Printf( AUTH_PREFIX" validated - account: %s - notoriety: %s (%s\n"), l, u, r );
			return;
		}

		if ( !Q_stricmp( s, "UPDATE" ) )
		{
			CL_Auth_Cvars_Set( -21, Auth_Stripslashes(a));
			Com_Printf( AUTH_PREFIX" game outdated - %s \n", Auth_Stripslashes(u));
			if ( trap_Key_GetCatcher( ) & KEYCATCH_CONSOLE ) {
				//
			} else {
				Com_Error (ERR_DROP, "Game outdated - %s\n", Auth_Stripslashes(u));
			}

			//@Barbatos: TO CHECK - SECURITY HOLE!
			/*#if defined __linux__
			trap_Sys_StartProcess("./UrTUpdater.i386", qtrue);
			#elif defined __MACOS__
			trap_Sys_StartProcess("UrTUpdater.app", qtrue);
			#else
			trap_Sys_StartProcess("UrTUpdater.exe", qtrue);
			#endif*/

			//Com_Error (ERR_DROP, "%s\n", Auth_Stripslashes(u));
			return;
		}
	}

	// authorization bad response
	Com_Error (ERR_DROP, AUTH_PREFIX" request error - contact an admin at http://"AUTH_WEBSITE" \n");

}
#endif



/**
 * Gear_SetItem
 *
 * Sets an item in the gear list.
 *
 */
void Gear_SetItem ( int gear, int itemid )
{
	int  i;

	for (i = 0; GearTable[gear][i] != -1; i++)
	{
		if (GearTable[gear][i] == itemid)
		{
			uiInfo.gearSelect[gear] = i;
			break;
		}
	}

	// No grenades with hk69
	/*	if ( gear == GEAR_PRIMARY )
			   if ( itemid == UT_ITEM_HK69 )
				   if ( GearTable[GEAR_GRENADE][uiInfo.gearSelect[GEAR_GRENADE]] != UT_ITEM_NONE )
				   {
					   Gear_SetItem ( GEAR_GRENADE, UT_ITEM_NONE );
				   }

		   // No hk69 with grenades
		   if ( gear == GEAR_GRENADE && itemid != UT_ITEM_NONE )
			   if ( GearTable[GEAR_PRIMARY][uiInfo.gearSelect[GEAR_PRIMARY]] == UT_ITEM_HK69 )
			   {
				   if ( GearTable[GEAR_SECONDARY][uiInfo.gearSelect[GEAR_SECONDARY]] == UT_ITEM_M4 )
					   Gear_SetItem ( GEAR_PRIMARY, UT_ITEM_G36 );
				   else
					   Gear_SetItem ( GEAR_PRIMARY, UT_ITEM_M4 );
			   } */

	if (gear == GEAR_PRIMARY || gear == GEAR_SECONDARY || gear == GEAR_GRENADE)
	{
		// Re-calculate how many items we can see.
		uiInfo.gearItemCount = 1;

		if (GearTable[GEAR_SECONDARY][uiInfo.gearSelect[GEAR_SECONDARY]] == UT_ITEM_NONE)
		{
			uiInfo.gearItemCount++;
		}

		if (GearTable[GEAR_PRIMARY][uiInfo.gearSelect[GEAR_PRIMARY]] == UT_ITEM_NONE)
		{
			uiInfo.gearItemCount++;
		}

		if (GearTable[GEAR_GRENADE][uiInfo.gearSelect[GEAR_GRENADE]] == UT_ITEM_NONE)
		{
			uiInfo.gearItemCount++;
		}

		trap_Cvar_Set ( "ui_gearItemCount", va("%i", uiInfo.gearItemCount ));
	}

	switch (gear)
	{
		case GEAR_PRIMARY:
			trap_Cvar_Set ( "ui_gearPrimary", va("%i", GearTable[GEAR_PRIMARY][uiInfo.gearSelect[GEAR_PRIMARY]] ));
			break;

		case GEAR_SECONDARY:
			trap_Cvar_Set ( "ui_gearSecondary", va("%i", GearTable[GEAR_SECONDARY][uiInfo.gearSelect[GEAR_SECONDARY]] ));
			break;

		case GEAR_SIDEARM:
			trap_Cvar_Set ( "ui_gearSidearm", va("%i", GearTable[GEAR_SIDEARM][uiInfo.gearSelect[GEAR_SIDEARM]] ));
			break;

		case GEAR_GRENADE:
			trap_Cvar_Set ( "ui_gearGrenade", va("%i", GearTable[GEAR_GRENADE][uiInfo.gearSelect[GEAR_GRENADE]] ));
			break;

		case GEAR_ITEM_1:
			trap_Cvar_Set ( "ui_gearItem1", va("%i", GearTable[GEAR_ITEM_1][uiInfo.gearSelect[GEAR_ITEM_1]] ));
			break;

		case GEAR_ITEM_2:
			trap_Cvar_Set ( "ui_gearItem2", va("%i", GearTable[GEAR_ITEM_2][uiInfo.gearSelect[GEAR_ITEM_2]] ));
			break;

		case GEAR_ITEM_3:
			trap_Cvar_Set ( "ui_gearItem3", va("%i", GearTable[GEAR_ITEM_3][uiInfo.gearSelect[GEAR_ITEM_3]] ));
			break;
	}
}

/**
 * AssetCache
 */
void AssetCache(void)
{
	//if (Assets.textFont == NULL) {
	//}
	//Assets.background = trap_R_RegisterShaderNoMip( ASSET_BACKGROUND );
	//Com_Printf("Menu Size: %i bytes\n", sizeof(Menus));
	uiInfo.uiDC.Assets.gradientBar		   = trap_R_RegisterShaderNoMip( ASSET_GRADIENTBAR );
	uiInfo.uiDC.Assets.fxBasePic		   = trap_R_RegisterShaderNoMip( ART_FX_BASE );
	uiInfo.uiDC.Assets.fxPic[0] 	   = trap_R_RegisterShaderNoMip( ART_FX_RED );
	uiInfo.uiDC.Assets.fxPic[1] 	   = trap_R_RegisterShaderNoMip( ART_FX_YELLOW );
	uiInfo.uiDC.Assets.fxPic[2] 	   = trap_R_RegisterShaderNoMip( ART_FX_GREEN );
	uiInfo.uiDC.Assets.fxPic[3] 	   = trap_R_RegisterShaderNoMip( ART_FX_TEAL );
	uiInfo.uiDC.Assets.fxPic[4] 	   = trap_R_RegisterShaderNoMip( ART_FX_BLUE );
	uiInfo.uiDC.Assets.fxPic[5] 	   = trap_R_RegisterShaderNoMip( ART_FX_CYAN );
	uiInfo.uiDC.Assets.fxPic[6] 	   = trap_R_RegisterShaderNoMip( ART_FX_WHITE );
	uiInfo.uiDC.Assets.scrollBar		   = trap_R_RegisterShaderNoMip( ASSET_SCROLLBAR );
	uiInfo.uiDC.Assets.scrollBarArrowDown  = trap_R_RegisterShaderNoMip( ASSET_SCROLLBAR_ARROWDOWN );
	uiInfo.uiDC.Assets.scrollBarArrowUp    = trap_R_RegisterShaderNoMip( ASSET_SCROLLBAR_ARROWUP );
	uiInfo.uiDC.Assets.scrollBarArrowLeft  = trap_R_RegisterShaderNoMip( ASSET_SCROLLBAR_ARROWLEFT );
	uiInfo.uiDC.Assets.scrollBarArrowRight = trap_R_RegisterShaderNoMip( ASSET_SCROLLBAR_ARROWRIGHT );
	uiInfo.uiDC.Assets.scrollBarThumb	   = trap_R_RegisterShaderNoMip( ASSET_SCROLL_THUMB );
	uiInfo.uiDC.Assets.sliderBar		   = trap_R_RegisterShaderNoMip( ASSET_SLIDER_BAR );
	uiInfo.uiDC.Assets.sliderThumb		   = trap_R_RegisterShaderNoMip( ASSET_SLIDER_THUMB );
}

/**
 * _UI_DrawSides
 */
void _UI_DrawSides(float x, float y, float w, float h, float size)
{
	UI_AdjustFrom640( &x, &y, &w, &h );
	size *= uiInfo.uiDC.xscale;
	trap_R_DrawStretchPic( x, y, size, h, 0, 0, 0, 0, uiInfo.uiDC.whiteShader );
	trap_R_DrawStretchPic( x + w - size, y, size, h, 0, 0, 0, 0, uiInfo.uiDC.whiteShader );
}

/**
 * _UI_DrawTopBottom
 */
void _UI_DrawTopBottom(float x, float y, float w, float h, float size)
{
	UI_AdjustFrom640( &x, &y, &w, &h );
	size *= uiInfo.uiDC.yscale;
	trap_R_DrawStretchPic( x, y, w, size, 0, 0, 0, 0, uiInfo.uiDC.whiteShader );
	trap_R_DrawStretchPic( x, y + h - size, w, size, 0, 0, 0, 0, uiInfo.uiDC.whiteShader );
}
/**
 * UI_DrawRect
 *
 * Coordinates are 640*480 virtual values
 *
 */
void _UI_DrawRect( float x, float y, float width, float height, float size, const float *color )
{
	trap_R_SetColor( color );

	_UI_DrawTopBottom(x, y, width, height, size);
	_UI_DrawSides(x, y, width, height, size);

	trap_R_SetColor( NULL );
}


/**
 * UI_ShowPostGame
 */
void UI_ShowPostGame(qboolean newHigh)
{
	trap_Cvar_Set ("cg_cameraOrbit", "0");
	trap_Cvar_Set("cg_thirdPerson", "0");
	trap_Cvar_Set( "sv_killserver", "1" );
	uiInfo.soundHighScore = newHigh;
	_UI_SetActiveMenu(UIMENU_POSTGAME);
}

/**
 * UI_DrawCenteredPic
 */
void UI_DrawCenteredPic(qhandle_t image, int w, int h)
{
	int  x, y;

	x = (SCREEN_WIDTH - w) / 2;
	y = (SCREEN_HEIGHT - h) / 2;
	UI_DrawHandlePic(x, y, w, h, image);
}

#define UI_FPS_FRAMES 4

/**
 * _UI_Refresh
 */
void _UI_Refresh( int realtime )
{
	static int	index;
	static int	previousTimes[UI_FPS_FRAMES];

	//if ( !( trap_Key_GetCatcher() & KEYCATCH_UI ) ) {
	//	return;
	//}

	uiInfo.uiDC.frameTime = realtime - uiInfo.uiDC.realTime;
	uiInfo.uiDC.realTime  = realtime;

	statsTick();  //See if theres a pending command
	parseStats(); //Might want to optimize this

	previousTimes[index % UI_FPS_FRAMES] = uiInfo.uiDC.frameTime;
	index++;

	if (index > UI_FPS_FRAMES)
	{
		int  i, total;
		// average multiple frames together to smooth changes out a bit
		total = 0;

		for (i = 0 ; i < UI_FPS_FRAMES ; i++)
		{
			total += previousTimes[i];
		}

		if (!total)
		{
			total = 1;
		}
		uiInfo.uiDC.FPS = 1000 * UI_FPS_FRAMES / total;
	}

	UI_UpdateCvars();

	if (Menu_Count() > 0)
	{
		// paint all the menus
		Menu_PaintAll();
		// refresh server browser list
		UI_DoServerRefresh();
		// refresh server status
		UI_BuildServerStatus(qfalse);
		// refresh find player list
		UI_BuildFindPlayerList(qfalse);
	}

	#ifdef USE_AUTH
	//@Barbatos //@Kalish
	CL_Auth_Refresh();
	#endif

	// draw cursor
	UI_SetColor( NULL );

	// @Barbatos - draw the cursor only when using the UI
	// it fixes the cursor being displayed when loading a map
	if ((Menu_Count() > 0) && (trap_Key_GetCatcher() & KEYCATCH_UI))
	{
		UI_DrawHandlePic( uiInfo.uiDC.cursorx - 16, uiInfo.uiDC.cursory - 16, 32, 32, uiInfo.uiDC.Assets.cursor);
	}
}

/**
 * _UI_Shutdown
 */
void _UI_Shutdown( void )
{
	trap_LAN_SaveCachedServers();
}

char  *defaultMenu = NULL;

/**
 * GetMenuBuffer
 */
char *GetMenuBuffer(const char *filename)
{
	int 	  len;
	fileHandle_t  f;
	static char   buf[MAX_MENUFILE];

	len = trap_FS_FOpenFile( filename, &f, FS_READ );

	if (!f)
	{
		trap_Print( va( S_COLOR_RED "menu file not found: %s, using default\n", filename ));
		return defaultMenu;
	}

	if (len >= MAX_MENUFILE)
	{
		trap_Print( va( S_COLOR_RED "menu file too large: %s is %i, max allowed is %i", filename, len, MAX_MENUFILE ));
		trap_FS_FCloseFile( f );
		return defaultMenu;
	}

	trap_FS_Read( buf, len, f );
	buf[len] = 0;
	trap_FS_FCloseFile( f );
	//COM_Compress(buf);
	return buf;
}

/**
 * Asset_Parse
 */
qboolean Asset_Parse(int handle)
{
	pc_token_t	token;
	const char	*tempStr;

	if (!trap_PC_ReadToken(handle, &token))
	{
		return qfalse;
	}

	if (Q_stricmp(token.string, "{") != 0)
	{
		return qfalse;
	}

	while (1)
	{
		memset(&token, 0, sizeof(pc_token_t));

		if (!trap_PC_ReadToken(handle, &token))
		{
			return qfalse;
		}

		if (Q_stricmp(token.string, "}") == 0)
		{
			return qtrue;
		}

		// font
		if (Q_stricmp(token.string, "font") == 0)
		{
			int  pointSize;

			if (!PC_String_Parse(handle, &tempStr) || !PC_Int_Parse(handle, &pointSize))
			{
				return qfalse;
			}
			trap_R_RegisterFont(tempStr, pointSize, &uiInfo.uiDC.Assets.textFont);
			uiInfo.uiDC.Assets.fontRegistered = qtrue;
			continue;
		}

		if (Q_stricmp(token.string, "smallFont") == 0)
		{
			int  pointSize;

			if (!PC_String_Parse(handle, &tempStr) || !PC_Int_Parse(handle, &pointSize))
			{
				return qfalse;
			}
			trap_R_RegisterFont(tempStr, pointSize, &uiInfo.uiDC.Assets.smallFont);
			continue;
		}

		if (Q_stricmp(token.string, "bigFont") == 0)
		{
			int  pointSize;

			if (!PC_String_Parse(handle, &tempStr) || !PC_Int_Parse(handle, &pointSize))
			{
				return qfalse;
			}
			trap_R_RegisterFont(tempStr, pointSize, &uiInfo.uiDC.Assets.bigFont);
			continue;
		}

		// gradientbar
		if (Q_stricmp(token.string, "gradientbar") == 0)
		{
			if (!PC_String_Parse(handle, &tempStr))
			{
				return qfalse;
			}
			uiInfo.uiDC.Assets.gradientBar = trap_R_RegisterShaderNoMip(tempStr);
			continue;
		}

		// enterMenuSound
		if (Q_stricmp(token.string, "menuEnterSound") == 0)
		{
			if (!PC_String_Parse(handle, &tempStr))
			{
				return qfalse;
			}
			uiInfo.uiDC.Assets.menuEnterSound = trap_S_RegisterSound( tempStr, qfalse );
			continue;
		}

		// exitMenuSound
		if (Q_stricmp(token.string, "menuExitSound") == 0)
		{
			if (!PC_String_Parse(handle, &tempStr))
			{
				return qfalse;
			}
			uiInfo.uiDC.Assets.menuExitSound = trap_S_RegisterSound( tempStr, qfalse );
			continue;
		}

		// itemFocusSound
		if (Q_stricmp(token.string, "itemFocusSound") == 0)
		{
			if (!PC_String_Parse(handle, &tempStr))
			{
				return qfalse;
			}
			uiInfo.uiDC.Assets.itemFocusSound = trap_S_RegisterSound( tempStr, qfalse );
			continue;
		}

		// menuBuzzSound
		if (Q_stricmp(token.string, "menuBuzzSound") == 0)
		{
			if (!PC_String_Parse(handle, &tempStr))
			{
				return qfalse;
			}
			uiInfo.uiDC.Assets.menuBuzzSound = trap_S_RegisterSound( tempStr, qfalse );
			continue;
		}

		if (Q_stricmp(token.string, "cursor") == 0)
		{
			if (!PC_String_Parse(handle, &uiInfo.uiDC.Assets.cursorStr))
			{
				return qfalse;
			}
			uiInfo.uiDC.Assets.cursor = trap_R_RegisterShaderNoMip( uiInfo.uiDC.Assets.cursorStr);
			continue;
		}

		if (Q_stricmp(token.string, "fadeClamp") == 0)
		{
			if (!PC_Float_Parse(handle, &uiInfo.uiDC.Assets.fadeClamp))
			{
				return qfalse;
			}
			continue;
		}

		if (Q_stricmp(token.string, "fadeCycle") == 0)
		{
			if (!PC_Int_Parse(handle, &uiInfo.uiDC.Assets.fadeCycle))
			{
				return qfalse;
			}
			continue;
		}

		if (Q_stricmp(token.string, "fadeAmount") == 0)
		{
			if (!PC_Float_Parse(handle, &uiInfo.uiDC.Assets.fadeAmount))
			{
				return qfalse;
			}
			continue;
		}

		if (Q_stricmp(token.string, "shadowX") == 0)
		{
			if (!PC_Float_Parse(handle, &uiInfo.uiDC.Assets.shadowX))
			{
				return qfalse;
			}
			continue;
		}

		if (Q_stricmp(token.string, "shadowY") == 0)
		{
			if (!PC_Float_Parse(handle, &uiInfo.uiDC.Assets.shadowY))
			{
				return qfalse;
			}
			continue;
		}

		if (Q_stricmp(token.string, "shadowColor") == 0)
		{
			if (!PC_Color_Parse(handle, &uiInfo.uiDC.Assets.shadowColor))
			{
				return qfalse;
			}
			uiInfo.uiDC.Assets.shadowFadeClamp = uiInfo.uiDC.Assets.shadowColor[3];
			continue;
		}
	}
	return qfalse;
}

/**
 * Font_Report
 */
void Font_Report(void)
{
	int  i;

	Com_Printf("Font Info\n");
	Com_Printf("=========\n");

	for (i = 32; i < 96; i++)
	{
		Com_Printf("Glyph handle %i: %i\n", i, uiInfo.uiDC.Assets.textFont.glyphs[i].glyph);
	}
}

/**
 * UI_Report
 */
void UI_Report(void)
{
	String_Report();
	//Font_Report();
}

/**
 * UI_ParseMenu
 */
void UI_ParseMenu(const char *menuFile)
{
	int 	handle;
	pc_token_t	token;

	//Com_Printf("Parsing menu file:%s\n", menuFile);

	handle = trap_PC_LoadSource(menuFile);

	if (!handle)
	{
		return;
	}

	while (1)
	{
		memset(&token, 0, sizeof(pc_token_t));

		if (!trap_PC_ReadToken( handle, &token ))
		{
			break;
		}

		//if ( Q_stricmp( token, "{" ) ) {
		//	Com_Printf( "Missing { in menu file\n" );
		//	break;
		//}

		//if ( menuCount == MAX_MENUS ) {
		//	Com_Printf( "Too many menus!\n" );
		//	break;
		//}

		if (token.string[0] == '}')
		{
			break;
		}

		if (Q_stricmp(token.string, "assetGlobalDef") == 0)
		{
			if (Asset_Parse(handle))
			{
				continue;
			}
			else
			{
				break;
			}
		}

		if (Q_stricmp(token.string, "menudef") == 0)
		{
			// start a new menu
			Menu_New(handle);
		}
	}
	trap_PC_FreeSource(handle);
}

/**
 * Load_Menu
 */
qboolean Load_Menu(int handle)
{
	pc_token_t	token;

	if (!trap_PC_ReadToken(handle, &token))
	{
		return qfalse;
	}

	if (token.string[0] != '{')
	{
		return qfalse;
	}

	while (1)
	{
		if (!trap_PC_ReadToken(handle, &token))
		{
			return qfalse;
		}

		if (token.string[0] == 0)
		{
			return qfalse;
		}

		if (token.string[0] == '}')
		{
			return qtrue;
		}

		UI_ParseMenu(token.string);
	}
	return qfalse;
}

/**
 * UI_LoadMenus
 */
void UI_LoadMenus(const char *menuFile, qboolean reset)
{
	pc_token_t	token;
	int 	handle;
	int 	start;

	start  = trap_Milliseconds();

	handle = trap_PC_LoadSource( menuFile );

	if (!handle)
	{
		trap_Error( va( S_COLOR_YELLOW "menu file not found: %s, using default\n", menuFile ));
		handle = trap_PC_LoadSource( "ui/menus.txt" );

		if (!handle)
		{
			trap_Error( va( S_COLOR_RED "default menu file not found: ui/menus.txt, unable to continue!\n", menuFile ));
		}
	}

	ui_new.integer = 1;

	if (reset)
	{
		Menu_Reset();
	}

	while (1)
	{
		if (!trap_PC_ReadToken(handle, &token))
		{
			break;
		}

		if((token.string[0] == 0) || (token.string[0] == '}'))
		{
			break;
		}

		if (token.string[0] == '}')
		{
			break;
		}

		if (Q_stricmp(token.string, "loadmenu") == 0)
		{
			if (Load_Menu(handle))
			{
				continue;
			}
			else
			{
				break;
			}
		}
	}

	Com_Printf("UI menu load time = %d milli seconds\n", trap_Milliseconds() - start);

	trap_PC_FreeSource( handle );
}

/**
 * UI_Load
 */
void UI_Load(void)
{
	char	   lastName[1024];
	menuDef_t  *menu	= Menu_GetFocused();
	char	   *menuSet = UI_Cvar_VariableString("ui_menuFiles");

	if (menu && menu->window.name)
	{
		strcpy(lastName, menu->window.name);
	}

	if ((menuSet == NULL) || (menuSet[0] == '\0'))
	{
		menuSet = "ui/menus.txt";
	}

	String_Init();

	UI_ParseGameInfo("gameinfo.txt");
	UI_LoadArenas();

	UI_LoadMenus(menuSet, qtrue);
	Menus_CloseAll();
	Menus_ActivateByName(lastName);
}

/**
 * UI_SetCapFragLimits
 */
static void UI_SetCapFragLimits(qboolean uiVars)
{
	int  cap  = 5;
	int  frag = 10;

	if (uiVars)
	{
		trap_Cvar_Set("ui_captureLimit", va("%d", cap));
		trap_Cvar_Set("ui_fragLimit", va("%d", frag));
	}
	else
	{
		trap_Cvar_Set("capturelimit", va("%d", cap));
		trap_Cvar_Set("fraglimit", va("%d", frag));
	}
}
// ui_gameType assumes gametype 0 is -1 ALL and will not show
static void UI_DrawGameType(rectDef_t *rect, float scale, vec4_t color, int textStyle)
{
	UI_Text_Paint(rect->x, rect->y, scale, color, uiInfo.gameTypes[ui_gameType.integer].gameType, 0, 0, textStyle);
}

/**
 * UI_DrawNetGameType
 */
static void UI_DrawNetGameType(rectDef_t *rect, float scale, vec4_t color, int textStyle)
{
	if ((ui_netGameType.integer < 0) || (ui_netGameType.integer > uiInfo.numGameTypes))
	{
		trap_Cvar_Set("ui_netGameType", "0");
		trap_Cvar_Set("ui_actualNetGameType", "0");
	}
	UI_Text_Paint(rect->x, rect->y, scale, color, uiInfo.gameTypes[ui_netGameType.integer].gameType, 0, 0, textStyle);
}

/**
 * UI_DrawJoinGameType
 */
static void UI_DrawJoinGameType(rectDef_t *rect, float scale, vec4_t color, int textStyle)
{
	if ((ui_joinGameType.integer < 0) || (ui_joinGameType.integer > uiInfo.numJoinGameTypes))
	{
		trap_Cvar_Set("ui_joinGameType", "0");
	}
	UI_Text_Paint(rect->x, rect->y, scale, color, uiInfo.joinGameTypes[ui_joinGameType.integer].gameType, 0, 0, textStyle);
}

/**
 * UI_TeamIndexFromName
 */
static int UI_TeamIndexFromName(const char *name)
{
	int  i;

	if (name && *name)
	{
		for (i = 0; i < uiInfo.teamCount; i++)
		{
			if (Q_stricmp(name, uiInfo.teamList[i].teamName) == 0)
			{
				return i;
			}
		}
	}

	return 0;
}

/**
 * UI_DrawPreviewCinematic
 */
static void UI_DrawPreviewCinematic(rectDef_t *rect, float scale, vec4_t color)
{
	if (uiInfo.previewMovie > -2)
	{
		uiInfo.previewMovie = trap_CIN_PlayCinematic(va("%s.roq", uiInfo.movieList[uiInfo.movieIndex]), 0, 0, 0, 0, (CIN_loop | CIN_silent));

		if (uiInfo.previewMovie >= 0)
		{
			trap_CIN_RunCinematic(uiInfo.previewMovie);
			trap_CIN_SetExtents(uiInfo.previewMovie, rect->x, rect->y, rect->w, rect->h);
			trap_CIN_DrawCinematic(uiInfo.previewMovie);
		}
		else
		{
			uiInfo.previewMovie = -2;
		}
	}
}

/**
 * UI_DrawSkill
 */
static void UI_DrawSkill(rectDef_t *rect, float scale, vec4_t color, int textStyle)
{
	int  i;

	i = trap_Cvar_VariableValue( "g_spSkill" );

	if ((i < 1) || (i > numSkillLevels))
	{
		i = 1;
	}
	UI_Text_Paint(rect->x, rect->y, scale, color, skillLevels[i - 1], 0, 0, textStyle);
}

/**
 * UI_DrawTeamName
 */
static void UI_DrawTeamName(rectDef_t *rect, float scale, vec4_t color, qboolean blue, int textStyle)
{
	int  i;

	i = UI_TeamIndexFromName(UI_Cvar_VariableString((blue) ? "ui_blueTeam" : "ui_redTeam"));

	if ((i >= 0) && (i < uiInfo.teamCount))
	{
		UI_Text_Paint(rect->x, rect->y, scale, color, va("%s: %s", (blue) ? "Blue" : "Red", uiInfo.teamList[i].teamName), 0, 0, textStyle);
	}
}

/**
 * UI_DrawEffects
 */
static void UI_DrawEffects(rectDef_t *rect, float scale, vec4_t color)
{
	UI_DrawHandlePic( rect->x, rect->y - 14, 128, 8, uiInfo.uiDC.Assets.fxBasePic );
	UI_DrawHandlePic( rect->x + uiInfo.effectsColor * 16 + 8, rect->y - 16, 16, 12, uiInfo.uiDC.Assets.fxPic[uiInfo.effectsColor] );
}

/**
 * UI_DrawMapPreview
 */
static void UI_DrawMapPreview(rectDef_t *rect, float scale, vec4_t color, qboolean net)
{
	int  map = (net) ? ui_currentNetMap.integer : ui_currentMap.integer;

	if ((map < 0) || (map > uiInfo.mapCount))
	{
		if (net)
		{
			ui_currentNetMap.integer = 0;
			trap_Cvar_Set("ui_currentNetMap", "0");
		}
		else
		{
			ui_currentMap.integer = 0;
			trap_Cvar_Set("ui_currentMap", "0");
		}
		map = 0;
	}

	if (uiInfo.mapList[map].levelShot == -1)
	{
		uiInfo.mapList[map].levelShot = trap_R_RegisterShaderNoMip(uiInfo.mapList[map].imageName);
	}

	if (uiInfo.mapList[map].levelShot > 0)
	{
		UI_DrawHandlePic( rect->x, rect->y, rect->w, rect->h, uiInfo.mapList[map].levelShot);
	}
	else
	{
		UI_DrawHandlePic( rect->x, rect->y, rect->w, rect->h, trap_R_RegisterShaderNoMip("menu/art/unknownmap"));
	}
}

/**
 * UI_DrawMapTimeToBeat
 */
static void UI_DrawMapTimeToBeat(rectDef_t *rect, float scale, vec4_t color, int textStyle)
{
	int  minutes, seconds, time;

	if ((ui_currentMap.integer < 0) || (ui_currentMap.integer > uiInfo.mapCount))
	{
		ui_currentMap.integer = 0;
		trap_Cvar_Set("ui_currentMap", "0");
	}

	time	= uiInfo.mapList[ui_currentMap.integer].timeToBeat[uiInfo.gameTypes[ui_gameType.integer].gtEnum];

	minutes = time / 60;
	seconds = time % 60;

	UI_Text_Paint(rect->x, rect->y, scale, color, va("%02i:%02i", minutes, seconds), 0, 0, textStyle);
}

/**
 * UI_DrawMapCinematic
 */
static void UI_DrawMapCinematic(rectDef_t *rect, float scale, vec4_t color, qboolean net)
{
	int  map = (net) ? ui_currentNetMap.integer : ui_currentMap.integer;

	if ((map < 0) || (map > uiInfo.mapCount))
	{
		if (net)
		{
			ui_currentNetMap.integer = 0;
			trap_Cvar_Set("ui_currentNetMap", "0");
		}
		else
		{
			ui_currentMap.integer = 0;
			trap_Cvar_Set("ui_currentMap", "0");
		}
		map = 0;
	}

	if (uiInfo.mapList[map].cinematic >= -1)
	{
		if (uiInfo.mapList[map].cinematic == -1)
		{
			uiInfo.mapList[map].cinematic = trap_CIN_PlayCinematic(va("%s.roq", uiInfo.mapList[map].mapLoadName), 0, 0, 0, 0, (CIN_loop | CIN_silent));
		}

		if (uiInfo.mapList[map].cinematic >= 0)
		{
			trap_CIN_RunCinematic(uiInfo.mapList[map].cinematic);
			trap_CIN_SetExtents(uiInfo.mapList[map].cinematic, rect->x, rect->y, rect->w, rect->h);
			trap_CIN_DrawCinematic(uiInfo.mapList[map].cinematic);
		}
		else
		{
			uiInfo.mapList[map].cinematic = -2;
		}
	}
	else
	{
		UI_DrawMapPreview(rect, scale, color, net);
	}
}

/**
 * UI_DrawPlayerModel
 */
static void UI_DrawPlayerModel(rectDef_t *rect)
{
	static playerInfo_t  info;
	char			 model[MAX_QPATH];
	char			 team[256];
	char			 head[256];
	vec3_t			 viewangles;
	vec3_t			 moveangles;

	if (UI_Cvar_VariableString("ui_currentmodel"))
	{
		strcpy(model, UI_Cvar_VariableString("ui_currentmodel"));

		if (!q3Model)
		{
			q3Model 	= qtrue;
			updateModel = qtrue;
		}
		team[0] = '\0';
	}
	else
	{
		strcpy(model, UI_Cvar_VariableString("model"));

		if (q3Model)
		{
			q3Model 	= qfalse;
			updateModel = qtrue;
		}
	}

/*
	if(uiInfo.teamIndex==0)
	{
		strcpy ( model, "athena/red" );
		strcpy ( head, "athena/red" );
	}
	else
	{
		strcpy ( model, "athena/blue" );
		strcpy ( head, "athena/blue" );
	}
*/

	if (updateModel)
	{
		int  torsoAnim = TORSO_STAND_RIFLE;

		memset( &info, 0, sizeof(playerInfo_t));

		info.weapon   = UT_WP_LR;

		viewangles[YAW]   = 180 - 10;
		viewangles[PITCH] = 120;
		viewangles[ROLL]  = 0;
		VectorClear( moveangles );
		UI_PlayerInfo_SetModel( &info, model, head, team, info.weapon);
		UI_PlayerInfo_SetInfo( &info, LEGS_IDLE, torsoAnim, viewangles, vec3_origin, info.weapon, qfalse );
//		UI_RegisterClientModelname( &info, model, head, team);
		updateModel = qfalse;
	}

	UI_DrawPlayer( rect->x, rect->y, rect->w, rect->h, &info, uiInfo.uiDC.realTime / 2);
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : UI_DrawKIOption
// Description: Draws the text for a keyboard interface menu option
// Author	  : GottaBeKD
///////////////////////////////////////////////////////////////////////////////////////////
static void UI_DrawKIOption ( int index, rectDef_t *rect, float scale, vec4_t color, int textStyle )
{
	if(index >= KI_OPTION1 && index <= KI_OPTION5 && ui_keyboardInterfaceOptions[index - KI_OPTION1])
	{
		if(Q_stricmp ( ui_keyboardInterfaceOptions[index - KI_OPTION1], "(null)" ) != 0)
		{
			UI_Text_Paint(rect->x, rect->y, scale, color, ui_keyboardInterfaceOptions[index-KI_OPTION1], 0, 0, textStyle);
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : UI_DrawKIIndex
// Description: Draws the option index for a keyboard interface menu option
// Author	  : GottaBeKD
///////////////////////////////////////////////////////////////////////////////////////////
static void UI_DrawKIIndex ( int index, rectDef_t *rect, float scale, vec4_t color, int textStyle )
{
	char kiIndex[3];
	if(index >= KI_OPTION1 && index <= KI_OPTION5 && ui_keyboardInterfaceOptions[index - KI_OPTION1])
	{
		if(Q_stricmp ( ui_keyboardInterfaceOptions[index - KI_OPTION1], "(null)" ) != 0)
		{
			kiIndex[0] = index-KI_OPTION1+'1';
			kiIndex[1] = '.';
			kiIndex[2] = 0;
			UI_Text_Paint(rect->x, rect->y, scale, color, kiIndex, 0, 0, textStyle);
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : UI_DrawGearName
// Description: Draws the text for a particular gear type.
// Author	  : Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
static void UI_DrawGearName ( int index, rectDef_t *rect, float scale, vec4_t color, int textStyle )
{
	UI_Text_Paint(rect->x, rect->y, scale, color, bg_itemlist[GearTable[index][uiInfo.gearSelect[index]]].pickup_name, 0, 0, textStyle);
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : UI_DrawGearImage
// Description: Draws the image for a particular gear type.
// Author	  : Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
static void UI_DrawGearImage ( int index, rectDef_t *rect, float scale, vec4_t color, int textStyle )
{
	UI_DrawHandlePic ( rect->x,
			   rect->y,
			   rect->w,
			   rect->h,
			   trap_R_RegisterShaderNoMip(bg_itemlist[GearTable[index][uiInfo.gearSelect[index]]].icon));
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : UI_DrawItemName
// Description: Draws the name for a particular item type.
// Author	  : Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
static void UI_DrawItemName ( int index, rectDef_t *rect, float scale, vec4_t color, int textStyle )
{
	UI_Text_Paint(rect->x, rect->y, scale, color, bg_itemlist[ItemTable[index]].pickup_name, 0, 0, textStyle);
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : UI_DrawItemImage
// Description: Draws the image for a particular item type.
// Author	  : Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
static void UI_DrawItemImage ( int index, rectDef_t *rect, float scale, vec4_t color, int textStyle )
{
	UI_DrawHandlePic ( rect->x,
			   rect->y,
			   rect->w,
			   rect->h,
			   trap_R_RegisterShaderNoMip(bg_itemlist[ItemTable[index]].icon));
}

/**
 * UI_DrawNetSource
 */
static void UI_DrawNetSource(rectDef_t *rect, float scale, vec4_t color, int textStyle)
{
	if ((ui_netSource.integer < 0) || (ui_netSource.integer > uiInfo.numGameTypes))
	{
		ui_netSource.integer = 0;
	}
	UI_Text_Paint(rect->x, rect->y, scale, color, va("Source: %s", netSources[ui_netSource.integer]), 0, 0, textStyle);
}

/**
 * UI_DrawNetMapPreview
 */
static void UI_DrawNetMapPreview(rectDef_t *rect, float scale, vec4_t color)
{
	if (uiInfo.serverStatus.currentServerPreview > 0)
	{
		UI_DrawHandlePic( rect->x, rect->y, rect->w, rect->h, uiInfo.serverStatus.currentServerPreview);
	}
	else
	{
		UI_DrawHandlePic( rect->x, rect->y, rect->w, rect->h, trap_R_RegisterShaderNoMip("menu/art/unknownmap"));
	}
}

/**
 * UI_DrawNetMapCinematic
 */
static void UI_DrawNetMapCinematic(rectDef_t *rect, float scale, vec4_t color)
{
	if ((ui_currentNetMap.integer < 0) || (ui_currentNetMap.integer > uiInfo.mapCount))
	{
		ui_currentNetMap.integer = 0;
		trap_Cvar_Set("ui_currentNetMap", "0");
	}

	if (uiInfo.serverStatus.currentServerCinematic >= 0)
	{
		trap_CIN_RunCinematic(uiInfo.serverStatus.currentServerCinematic);
		trap_CIN_SetExtents(uiInfo.serverStatus.currentServerCinematic, rect->x, rect->y, rect->w, rect->h);
		trap_CIN_DrawCinematic(uiInfo.serverStatus.currentServerCinematic);
	}
	else
	{
		UI_DrawNetMapPreview(rect, scale, color);
	}
}

/**
 * UI_DrawNetFilter
 */
static void UI_DrawNetFilter(rectDef_t *rect, float scale, vec4_t color, int textStyle)
{
	if ((ui_serverFilterType.integer < 0) || (ui_serverFilterType.integer > numServerFilters))
	{
		ui_serverFilterType.integer = 0;
	}
	UI_Text_Paint(rect->x, rect->y, scale, color, va("Filter: %s", serverFilters[ui_serverFilterType.integer].description), 0, 0, textStyle);
}

/**
 * UI_DrawAllMapsSelection
 */
static void UI_DrawAllMapsSelection(rectDef_t *rect, float scale, vec4_t color, int textStyle, qboolean net)
{
	int  map = (net) ? ui_currentNetMap.integer : ui_currentMap.integer;

	if ((map >= 0) && (map < uiInfo.mapCount))
	{
		UI_Text_Paint(rect->x, rect->y, scale, color, uiInfo.mapList[map].mapName, 0, 0, textStyle);
	}
}


/**
 * UI_OwnerDrawWidth
 */
static int UI_OwnerDrawWidth(int ownerDraw, float scale)
{
	int 	i;
	const char	*s = NULL;

	switch (ownerDraw)
	{
		case UI_GAMETYPE:
			s = uiInfo.gameTypes[ui_gameType.integer].gameType;
			break;

		case UI_SKILL:
			i = trap_Cvar_VariableValue( "g_spSkill" );

			if ((i < 1) || (i > numSkillLevels))
			{
				i = 1;
			}
			s = skillLevels[i - 1];
			break;

		case UI_BLUETEAMNAME:
			i = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_blueTeam"));

			if ((i >= 0) && (i < uiInfo.teamCount))
			{
				s = va("%s: %s", "Blue", uiInfo.teamList[i].teamName);
			}
			break;

		case UI_REDTEAMNAME:
			i = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_redTeam"));

			if ((i >= 0) && (i < uiInfo.teamCount))
			{
				s = va("%s: %s", "Red", uiInfo.teamList[i].teamName);
			}
			break;

		case UI_NETSOURCE:

			if ((ui_netSource.integer < 0) || (ui_netSource.integer > uiInfo.numJoinGameTypes))
			{
				ui_netSource.integer = 0;
			}
			s = va("Source: %s", netSources[ui_netSource.integer]);
			break;

		case UI_NETFILTER:

			if ((ui_serverFilterType.integer < 0) || (ui_serverFilterType.integer > numServerFilters))
			{
				ui_serverFilterType.integer = 0;
			}
			s = va("Filter: %s", serverFilters[ui_serverFilterType.integer].description );
			break;

		case UI_ALLMAPS_SELECTION:
			break;

		case UI_KEYBINDSTATUS:

			if (Display_KeyBindPending())
			{
				s = "Waiting for new key... Press ESCAPE to cancel";
			}
			else
			{
				s = "Press ENTER or CLICK to change, Press BACKSPACE to clear";
			}
			break;

		case UI_SERVERREFRESHDATE:
			s = UI_Cvar_VariableString(va("ui_lastServerRefresh_%i", ui_netSource.integer));
			break;

		default:
			break;
	}

	if (s)
	{
		return UI_Text_Width(s, scale, 0);
	}
	return 0;
}

/**
 * UI_DrawCrosshair
 */
static void UI_DrawCrosshair(rectDef_t *rect, float scale, vec4_t color)
{
	crosshair_t  *crosshair;

	trap_R_SetColor(color);

	if ((uiInfo.currentCrosshair < 0) || (uiInfo.currentCrosshair >= utCrosshairGetCount( )))
	{
		uiInfo.currentCrosshair = 0;
	}

	if (uiInfo.currentCrosshair < 8)
	{
		crosshair = utCrosshairGetAt ( uiInfo.currentCrosshair );

		if (crosshair->center)
		{
			UI_DrawHandlePic( rect->x, rect->y - rect->h, rect->w, rect->h, crosshair->center);
		}

		if (crosshair->right)
		{
			UI_DrawHandlePic( rect->x + 16, rect->y - rect->h, rect->w, rect->h, crosshair->right );
		}

		if (crosshair->left)
		{
			UI_DrawHandlePic( rect->x - 16, rect->y - rect->h, rect->w, rect->h, crosshair->left );
		}

		if (crosshair->top)
		{
			UI_DrawHandlePic( rect->x, rect->y - rect->h - 16, rect->w, rect->h, crosshair->top );
		}

		if (crosshair->bottom)
		{
			UI_DrawHandlePic( rect->x, rect->y - rect->h + 16, rect->w, rect->h, crosshair->bottom );
		}
	}
	else
	{
		switch (uiInfo.currentCrosshair)
		{
			case 8:
				// Left.
				UI_DrawHandlePic(rect->x, rect->y - (rect->h / 2), rect->w / 3, 1, uiInfo.uiDC.whiteShader);

				// Right.
				UI_DrawHandlePic(rect->x + ((rect->w / 3) * 2) + 2, rect->y - (rect->h / 2), rect->w / 3, 1, uiInfo.uiDC.whiteShader);

				// Up.
				UI_DrawHandlePic(rect->x + (rect->w / 2), rect->y - rect->h, 1, rect->h / 3, uiInfo.uiDC.whiteShader);

				// Down.
				UI_DrawHandlePic(rect->x + (rect->w / 2), rect->y - rect->h + ((rect->h / 3) * 2) + 2, 1, rect->h / 3, uiInfo.uiDC.whiteShader);

				break;

			case 9:
			case 10:
			case 11:

				if (uiInfo.currentCrosshair != 9)
				{
					UI_DrawHandlePic(rect->x + (rect->w / 2), rect->y - (rect->h / 2), 1, 1, uiInfo.uiDC.whiteShader);
				}

				// Left.
				UI_DrawHandlePic(rect->x, rect->y - (rect->h / 2), rect->w / 4, 1, uiInfo.uiDC.whiteShader);

				// Right.
				UI_DrawHandlePic(rect->x + ((rect->w / 4) * 3) + 2, rect->y - (rect->h / 2), rect->w / 4, 1, uiInfo.uiDC.whiteShader);

				// Up.
				if (uiInfo.currentCrosshair != 11)
				{
					UI_DrawHandlePic(rect->x + (rect->w / 2), rect->y - rect->h, 1, rect->h / 4, uiInfo.uiDC.whiteShader);
				}

				// Down.
				UI_DrawHandlePic(rect->x + (rect->w / 2), rect->y - rect->h + ((rect->h / 4) * 3) + 2, 1, rect->h / 4, uiInfo.uiDC.whiteShader);

				break;

			case 12:
				UI_DrawHandlePic(rect->x + (rect->w / 2), rect->y - (rect->h / 2), 1, 1, uiInfo.uiDC.whiteShader);
				break;
		}
	}

	trap_R_SetColor(NULL);
}

/*
===============
UI_BuildPlayerList
===============
*/
static void UI_BuildPlayerList(void)
{
	uiClientState_t  cs;
	int 	 n, count, team, team2, playerTeamNumber;
	char		 info[MAX_INFO_STRING];

	trap_GetClientState( &cs );
	trap_GetConfigString( CS_PLAYERS + cs.clientNum, info, MAX_INFO_STRING );
	uiInfo.playerNumber = cs.clientNum;
	uiInfo.teamLeader	= atoi(Info_ValueForKey(info, "tl"));
	team			= atoi(Info_ValueForKey(info, "t"));
	trap_GetConfigString( CS_SERVERINFO, info, sizeof(info));
	count			= atoi( Info_ValueForKey( info, "sv_maxclients" ));
	uiInfo.playerCount	= 0;
	uiInfo.myTeamCount	= 0;
	playerTeamNumber	= 0;

	for(n = 0; n < count; n++)
	{
		trap_GetConfigString( CS_PLAYERS + n, info, MAX_INFO_STRING );

		if (info[0])
		{
			Q_strncpyz( uiInfo.playerNames[uiInfo.playerCount], Info_ValueForKey( info, "n" ), MAX_NAME_LENGTH );
			Q_CleanStr( uiInfo.playerNames[uiInfo.playerCount] );
			uiInfo.playerClientNum[uiInfo.playerCount] = n;
			uiInfo.playerCount++;
			team2					   = atoi(Info_ValueForKey(info, "t"));

			if (team2 == team)
			{
				Q_strncpyz( uiInfo.teamNames[uiInfo.myTeamCount], Info_ValueForKey( info, "n" ), MAX_NAME_LENGTH );
				Q_CleanStr( uiInfo.teamNames[uiInfo.myTeamCount] );
				uiInfo.teamClientNums[uiInfo.myTeamCount] = n;

				if (uiInfo.playerNumber == n)
				{
					playerTeamNumber = uiInfo.myTeamCount;
				}
				uiInfo.myTeamCount++;
			}
		}
	}

	if (!uiInfo.teamLeader)
	{
		trap_Cvar_Set("cg_selectedPlayer", va("%d", playerTeamNumber));
	}

	n = trap_Cvar_VariableValue("cg_selectedPlayer");

	if ((n < 0) || (n > uiInfo.myTeamCount))
	{
		n = 0;
	}

	if (n < uiInfo.myTeamCount)
	{
		trap_Cvar_Set("cg_selectedPlayerName", uiInfo.teamNames[n]);
	}
}

/**
 * UI_DrawSelectedPlayer
 */
static void UI_DrawSelectedPlayer(rectDef_t *rect, float scale, vec4_t color, int textStyle)
{
	if (uiInfo.uiDC.realTime > uiInfo.playerRefresh)
	{
		uiInfo.playerRefresh = uiInfo.uiDC.realTime + 3000;
		UI_BuildPlayerList();
	}
	UI_Text_Paint(rect->x, rect->y, scale, color, (uiInfo.teamLeader) ? UI_Cvar_VariableString("cg_selectedPlayerName") : UI_Cvar_VariableString("name"), 0, 0, textStyle);
}

/**
 * UI_DrawServerRefreshDate
 */
static void UI_DrawServerRefreshDate(rectDef_t *rect, float scale, vec4_t color, int textStyle)
{
	if (uiInfo.serverStatus.refreshActive)
	{
		vec4_t	lowLight, newColor;
		lowLight[0] = 0.8 * color[0];
		lowLight[1] = 0.8 * color[1];
		lowLight[2] = 0.8 * color[2];
		lowLight[3] = 0.8 * color[3];
		LerpColor(color, lowLight, newColor, 0.5 + 0.5 * sin(uiInfo.uiDC.realTime / PULSE_DIVISOR));

		//Iain: new addition for ms down check.
		if(trap_LAN_GetServerCount(ui_netSource.integer) < 0)
		{
			UI_Text_Paint(rect->x, rect->y, scale, newColor, "No Server List From Master Server.", 0, 0, textStyle);
		}
		else
		{
			UI_Text_Paint(rect->x, rect->y, scale, newColor, va("Getting info for %d servers (ESC to cancel)", trap_LAN_GetServerCount(ui_netSource.integer)), 0, 0, textStyle);
		}
	}
	else
	{
		char  buff[64];
		Q_strncpyz(buff, UI_Cvar_VariableString(va("ui_lastServerRefresh_%i", ui_netSource.integer)), 64);
		UI_Text_Paint(rect->x, rect->y, scale, color, va("Refresh Time: %s", buff), 0, 0, textStyle);
	}
}

/**
 * UI_DrawServerMOTD
 */
static void UI_DrawServerMOTD(rectDef_t *rect, float scale, vec4_t color)
{
	if (uiInfo.serverStatus.motdLen)
	{
		float  maxX;

		if (uiInfo.serverStatus.motdWidth == -1)
		{
			uiInfo.serverStatus.motdWidth	= 0;
			uiInfo.serverStatus.motdPaintX	= rect->x + 1;
			uiInfo.serverStatus.motdPaintX2 = -1;
		}

		if (uiInfo.serverStatus.motdOffset > uiInfo.serverStatus.motdLen)
		{
			uiInfo.serverStatus.motdOffset	= 0;
			uiInfo.serverStatus.motdPaintX	= rect->x + 1;
			uiInfo.serverStatus.motdPaintX2 = -1;
		}

		if (uiInfo.uiDC.realTime > uiInfo.serverStatus.motdTime)
		{
			uiInfo.serverStatus.motdTime = uiInfo.uiDC.realTime + 10;

			if (uiInfo.serverStatus.motdPaintX <= rect->x + 2)
			{
				if (uiInfo.serverStatus.motdOffset < uiInfo.serverStatus.motdLen)
				{
					uiInfo.serverStatus.motdPaintX += UI_Text_Width(&uiInfo.serverStatus.motd[uiInfo.serverStatus.motdOffset], scale, 1) - 1;
					uiInfo.serverStatus.motdOffset++;
				}
				else
				{
					uiInfo.serverStatus.motdOffset = 0;

					if (uiInfo.serverStatus.motdPaintX2 >= 0)
					{
						uiInfo.serverStatus.motdPaintX = uiInfo.serverStatus.motdPaintX2;
					}
					else
					{
						uiInfo.serverStatus.motdPaintX = rect->x + rect->w - 2;
					}
					uiInfo.serverStatus.motdPaintX2 = -1;
				}
			}
			else
			{
				//serverStatus.motdPaintX--;
				uiInfo.serverStatus.motdPaintX -= 2;

				if (uiInfo.serverStatus.motdPaintX2 >= 0)
				{
					//serverStatus.motdPaintX2--;
					uiInfo.serverStatus.motdPaintX2 -= 2;
				}
			}
		}

		maxX = rect->x + rect->w - 2;
		UI_Text_Paint_Limit(&maxX, uiInfo.serverStatus.motdPaintX, rect->y + rect->h - 3, scale, color, &uiInfo.serverStatus.motd[uiInfo.serverStatus.motdOffset], 0, 0);

		if (uiInfo.serverStatus.motdPaintX2 >= 0)
		{
			float  maxX2 = rect->x + rect->w - 2;
			UI_Text_Paint_Limit(&maxX2, uiInfo.serverStatus.motdPaintX2, rect->y + rect->h - 3, scale, color, uiInfo.serverStatus.motd, 0, uiInfo.serverStatus.motdOffset);
		}

		if (uiInfo.serverStatus.motdOffset && (maxX > 0))
		{
			// if we have an offset ( we are skipping the first part of the string ) and we fit the string
			if (uiInfo.serverStatus.motdPaintX2 == -1)
			{
				uiInfo.serverStatus.motdPaintX2 = rect->x + rect->w - 2;
			}
		}
		else
		{
			uiInfo.serverStatus.motdPaintX2 = -1;
		}
	}
}

/**
 * UI_DrawKeyBindStatus
 */
static void UI_DrawKeyBindStatus(rectDef_t *rect, float scale, vec4_t color, int textStyle)
{
	if (Display_KeyBindPending())
	{
		UI_Text_Paint(rect->x, rect->y, scale, color, "Waiting for new key... Press ESCAPE to cancel", 0, 0, textStyle);
	}
	else
	{
		UI_Text_Paint(rect->x, rect->y, scale, color, "Press ENTER or CLICK to change, Press BACKSPACE to clear", 0, 0, textStyle);
	}
}

/**
 * UI_DrawGLInfo
 */
static void UI_DrawGLInfo(rectDef_t *rect, float scale, vec4_t color, int textStyle)
{
	char		*eptr;
	char		buff[4096];
	const char	*lines[64];
	int 	y, numLines, i;

	UI_Text_Paint(rect->x + 2, rect->y, scale, color, va("VENDOR: %s", uiInfo.uiDC.glconfig.vendor_string), 0, 30, textStyle);
	UI_Text_Paint(rect->x + 2, rect->y + 15, scale, color, va("VERSION: %s: %s", uiInfo.uiDC.glconfig.version_string, uiInfo.uiDC.glconfig.renderer_string), 0, 30, textStyle);
	UI_Text_Paint(rect->x + 2, rect->y + 30, scale, color, va ("PIXELFORMAT: color(%d-bits) Z(%d-bits) stencil(%d-bits)", uiInfo.uiDC.glconfig.colorBits, uiInfo.uiDC.glconfig.depthBits, uiInfo.uiDC.glconfig.stencilBits), 0, 30, textStyle);

	// build null terminated extension strings
	Q_strncpyz(buff, uiInfo.uiDC.glconfig.extensions_string, 4096);
	eptr	 = buff;
	y	 = rect->y + 45;
	numLines = 0;

	while (y < rect->y + rect->h && *eptr)
	{
		while (*eptr && *eptr == ' ')
		{
			*eptr++ = '\0';
		}

		// track start of valid string
		if (*eptr && (*eptr != ' '))
		{
			lines[numLines++] = eptr;
		}

		while (*eptr && *eptr != ' ')
		{
			eptr++;
		}
	}

	i = 0;

	while (i < numLines)
	{
		UI_Text_Paint(rect->x + 2, y, scale, color, lines[i++], 0, 20, textStyle);

		if (i < numLines)
		{
			UI_Text_Paint(rect->x + rect->w / 2, y, scale, color, lines[i++], 0, 20, textStyle);
		}
		y += 10;

		if (y > rect->y + rect->h - 11)
		{
			break;
		}
	}
}

// FIXME: table drive
//
static void UI_OwnerDraw( float x, float y, float w, float h, float text_x, float text_y, int ownerDraw, int ownerDrawFlags, int ownerDrawParam, int ownerDrawParam2, int align, float special, float scale, vec4_t color, qhandle_t shader, int textStyle)
{
	rectDef_t  rect;

	rect.x = x + text_x;
	rect.y = y + text_y;
	rect.w = w;
	rect.h = h;

	switch (ownerDraw)
	{
		// Urban terror additions
		case UI_KI_OPTION:
			UI_DrawKIOption ( ownerDrawParam, &rect, scale, color, textStyle );
			break;

		case UI_KI_INDEX:
			UI_DrawKIIndex ( ownerDrawParam, &rect, scale, color, textStyle );
			break;

		case UI_GEAR_IMAGE:
			UI_DrawGearImage ( ownerDrawParam, &rect, scale, color, textStyle );
			break;

		case UI_GEAR_NAME:
			UI_DrawGearName ( ownerDrawParam, &rect, scale, color, textStyle  );
			break;

		case UI_ITEM_IMAGE:
			UI_DrawItemImage ( ownerDrawParam, &rect, scale, color, textStyle );
			break;

		case UI_ITEM_NAME:
			UI_DrawItemName ( ownerDrawParam, &rect, scale, color, textStyle  );
			break;

		// Q3 defaults
		case UI_EFFECTS:
			UI_DrawEffects(&rect, scale, color);
			break;

		case UI_PLAYERMODEL:
			UI_DrawPlayerModel(&rect);
			break;

		case UI_PREVIEWCINEMATIC:
			UI_DrawPreviewCinematic(&rect, scale, color);
			break;

		case UI_GAMETYPE:
			UI_DrawGameType(&rect, scale, color, textStyle);
			break;

		case UI_NETGAMETYPE:
			UI_DrawNetGameType(&rect, scale, color, textStyle);
			break;

		case UI_JOINGAMETYPE:
			UI_DrawJoinGameType(&rect, scale, color, textStyle);
			break;

		case UI_MAPPREVIEW:
			UI_DrawMapPreview(&rect, scale, color, qtrue);
			break;

		case UI_MAP_TIMETOBEAT:
			UI_DrawMapTimeToBeat(&rect, scale, color, textStyle);
			break;

		case UI_MAPCINEMATIC:
			UI_DrawMapCinematic(&rect, scale, color, qfalse);
			break;

		case UI_STARTMAPCINEMATIC:
			UI_DrawMapCinematic(&rect, scale, color, qtrue);
			break;

		case UI_SKILL:
			UI_DrawSkill(&rect, scale, color, textStyle);
			break;

		case UI_BLUETEAMNAME:
			UI_DrawTeamName(&rect, scale, color, qtrue, textStyle);
			break;

		case UI_REDTEAMNAME:
			UI_DrawTeamName(&rect, scale, color, qfalse, textStyle);
			break;

		case UI_NETSOURCE:
			UI_DrawNetSource(&rect, scale, color, textStyle);
			break;

		case UI_NETMAPPREVIEW:
			UI_DrawNetMapPreview(&rect, scale, color);
			break;

		case UI_NETMAPCINEMATIC:
			UI_DrawNetMapCinematic(&rect, scale, color);
			break;

		case UI_NETFILTER:
			UI_DrawNetFilter(&rect, scale, color, textStyle);
			break;

		case UI_ALLMAPS_SELECTION:
			UI_DrawAllMapsSelection(&rect, scale, color, textStyle, qtrue);
			break;

		case UI_MAPS_SELECTION:
			UI_DrawAllMapsSelection(&rect, scale, color, textStyle, qfalse);
			break;

		case UI_CROSSHAIR:
			UI_DrawCrosshair(&rect, scale, color);
			break;

		case UI_SELECTEDPLAYER:
			UI_DrawSelectedPlayer(&rect, scale, color, textStyle);
			break;

		case UI_SERVERREFRESHDATE:
			UI_DrawServerRefreshDate(&rect, scale, color, textStyle);
			break;

		case UI_SERVERMOTD:
			UI_DrawServerMOTD(&rect, scale, color);
			break;

		case UI_GLINFO:
			UI_DrawGLInfo(&rect, scale, color, textStyle);
			break;

		case UI_KEYBINDSTATUS:
			UI_DrawKeyBindStatus(&rect, scale, color, textStyle);
			break;

		default:
			break;
	}
}

/**
 * UI_OwnerDrawVisible
 */
static qboolean UI_OwnerDrawVisible(int flags)
{
	qboolean  vis = qtrue;

	while (flags)
	{
		if (flags & UI_SHOW_FFA)
		{
			if (trap_Cvar_VariableValue("g_gametype") >= GT_TEAM)
			{
				vis = qfalse;
			}
			flags &= ~UI_SHOW_FFA;
		}

		if (flags & UI_SHOW_NOTFFA)
		{
			if (trap_Cvar_VariableValue("g_gametype") < GT_TEAM)
			{
				vis = qfalse;
			}
			flags &= ~UI_SHOW_NOTFFA;
		}

		if (flags & UI_SHOW_LEADER)
		{
			// these need to show when this client can give orders to a player or a group
			if (!uiInfo.teamLeader)
			{
				vis = qfalse;
			}
			else
			{
				// if showing yourself
				if ((ui_selectedPlayer.integer < uiInfo.myTeamCount) && (uiInfo.teamClientNums[ui_selectedPlayer.integer] == uiInfo.playerNumber))
				{
					vis = qfalse;
				}
			}
			flags &= ~UI_SHOW_LEADER;
		}

		if (flags & UI_SHOW_NOTLEADER)
		{
			// these need to show when this client is assigning their own status or they are NOT the leader
			if (uiInfo.teamLeader)
			{
				// if not showing yourself
				if (!((ui_selectedPlayer.integer < uiInfo.myTeamCount) && (uiInfo.teamClientNums[ui_selectedPlayer.integer] == uiInfo.playerNumber)))
				{
					vis = qfalse;
				}
			}
			flags &= ~UI_SHOW_NOTLEADER;
		}

		if (flags & UI_SHOW_FAVORITESERVERS)
		{
			// this assumes you only put this type of display flag on something showing in the proper context
			if (ui_netSource.integer != AS_FAVORITES)
			{
				vis = qfalse;
			}
			flags &= ~UI_SHOW_FAVORITESERVERS;
		}

		if (flags & UI_SHOW_NOTFAVORITESERVERS)
		{
			// this assumes you only put this type of display flag on something showing in the proper context
			if (ui_netSource.integer == AS_FAVORITES)
			{
				vis = qfalse;
			}
			flags &= ~UI_SHOW_NOTFAVORITESERVERS;
		}

		if (flags & UI_SHOW_ANYTEAMGAME)
		{
			if (uiInfo.gameTypes[ui_gameType.integer].gtEnum <= GT_TEAM)
			{
				vis = qfalse;
			}
			flags &= ~UI_SHOW_ANYTEAMGAME;
		}

		if (flags & UI_SHOW_ANYNONTEAMGAME)
		{
			if (uiInfo.gameTypes[ui_gameType.integer].gtEnum > GT_TEAM)
			{
				vis = qfalse;
			}
			flags &= ~UI_SHOW_ANYNONTEAMGAME;
		}

		if (flags & UI_SHOW_NETANYTEAMGAME)
		{
			if (uiInfo.gameTypes[ui_netGameType.integer].gtEnum <= GT_TEAM)
			{
				vis = qfalse;
			}
			flags &= ~UI_SHOW_NETANYTEAMGAME;
		}

		if (flags & UI_SHOW_NETANYNONTEAMGAME)
		{
			if (uiInfo.gameTypes[ui_netGameType.integer].gtEnum > GT_TEAM)
			{
				vis = qfalse;
			}
			flags &= ~UI_SHOW_NETANYNONTEAMGAME;
		}

		if (flags & UI_SHOW_NEWHIGHSCORE)
		{
			if (uiInfo.newHighScoreTime < uiInfo.uiDC.realTime)
			{
				vis = qfalse;
			}
			else
			{
				if (uiInfo.soundHighScore)
				{
					if (trap_Cvar_VariableValue("sv_killserver") == 0)
					{
						// wait on server to go down before playing sound
						trap_S_StartLocalSound(uiInfo.newHighScoreSound, CHAN_ANNOUNCER);
						uiInfo.soundHighScore = qfalse;
					}
				}
			}
			flags &= ~UI_SHOW_NEWHIGHSCORE;
		}

		if (flags & UI_SHOW_NEWBESTTIME)
		{
			if (uiInfo.newBestTime < uiInfo.uiDC.realTime)
			{
				vis = qfalse;
			}
			flags &= ~UI_SHOW_NEWBESTTIME;
		}

		if (flags & UI_SHOW_DEMOAVAILABLE)
		{
			if (!uiInfo.demoAvailable)
			{
				vis = qfalse;
			}
			flags &= ~UI_SHOW_DEMOAVAILABLE;
		}
		else
		{
			flags = 0;
		}
	}
	return vis;
}

/**
 * UI_Effects_HandleKey
 */
static qboolean UI_Effects_HandleKey(int flags, float *special, int key)
{
	if ((key == K_MOUSE1) || (key == K_MOUSE2) || (key == K_ENTER) || (key == K_KP_ENTER))
	{
		if (key == K_MOUSE2)
		{
			uiInfo.effectsColor--;
		}
		else
		{
			uiInfo.effectsColor++;
		}

		if(uiInfo.effectsColor > 6)
		{
			uiInfo.effectsColor = 0;
		}
		else if (uiInfo.effectsColor < 0)
		{
			uiInfo.effectsColor = 6;
		}

		trap_Cvar_SetValue( "color", uitogamecode[uiInfo.effectsColor] );
		return qtrue;
	}
	return qfalse;
}

/**
 * UI_GameType_HandleKey
 */
static qboolean UI_GameType_HandleKey(int flags, float *special, int key, qboolean resetMap)
{
	if ((key == K_MOUSE1) || (key == K_MOUSE2) || (key == K_ENTER) || (key == K_KP_ENTER))
	{
		int  oldCount = UI_MapCountByGameType(qtrue);

		// hard coded mess here
		if (key == K_MOUSE2)
		{
			ui_gameType.integer--;

			if (ui_gameType.integer == 2)
			{
				ui_gameType.integer = 1;
			}
			else if (ui_gameType.integer < 2)
			{
				ui_gameType.integer = uiInfo.numGameTypes - 1;
			}
		}
		else
		{
			ui_gameType.integer++;

			if (ui_gameType.integer >= uiInfo.numGameTypes)
			{
				ui_gameType.integer = 1;
			}
			else if (ui_gameType.integer == 2)
			{
				ui_gameType.integer = 3;
			}
		}

		trap_Cvar_Set("ui_Q3Model", "0");

		trap_Cvar_Set("ui_gameType", va("%d", ui_gameType.integer));
		UI_SetCapFragLimits(qtrue);
		UI_LoadBestScores(uiInfo.mapList[ui_currentMap.integer].mapLoadName, uiInfo.gameTypes[ui_gameType.integer].gtEnum);

		if (resetMap && (oldCount != UI_MapCountByGameType(qtrue)))
		{
			trap_Cvar_Set( "ui_currentMap", "0");
			Menu_SetFeederSelection(NULL, FEEDER_MAPS, 0, NULL);
		}
		return qtrue;
	}
	return qfalse;
}

/**
 * UI_NetGameType_HandleKey
 */
static qboolean UI_NetGameType_HandleKey(int flags, float *special, int key)
{
	if ((key == K_MOUSE1) || (key == K_MOUSE2) || (key == K_ENTER) || (key == K_KP_ENTER))
	{
		if (key == K_MOUSE2)
		{
			ui_netGameType.integer--;
		}
		else
		{
			ui_netGameType.integer++;
		}

		if (ui_netGameType.integer < 0)
		{
			ui_netGameType.integer = uiInfo.numGameTypes - 1;
		}
		else if (ui_netGameType.integer >= uiInfo.numGameTypes)
		{
			ui_netGameType.integer = 0;
		}

		trap_Cvar_Set( "ui_netGameType", va("%d", ui_netGameType.integer));
		trap_Cvar_Set( "ui_actualnetGameType", va("%d", uiInfo.gameTypes[ui_netGameType.integer].gtEnum));
		trap_Cvar_Set( "ui_currentNetMap", "0");
		UI_MapCountByGameType(qfalse);
		Menu_SetFeederSelection(NULL, FEEDER_ALLMAPS, 0, NULL);
		return qtrue;
	}
	return qfalse;
}

/**
 * UI_JoinGameType_HandleKey
 */
static qboolean UI_JoinGameType_HandleKey(int flags, float *special, int key)
{
	if ((key == K_MOUSE1) || (key == K_MOUSE2) || (key == K_ENTER) || (key == K_KP_ENTER))
	{
		if (key == K_MOUSE2)
		{
			ui_joinGameType.integer--;
		}
		else
		{
			ui_joinGameType.integer++;
		}

		if (ui_joinGameType.integer < 0)
		{
			ui_joinGameType.integer = uiInfo.numJoinGameTypes - 1;
		}
		else if (ui_joinGameType.integer >= uiInfo.numJoinGameTypes)
		{
			ui_joinGameType.integer = 0;
		}

		trap_Cvar_Set( "ui_joinGameType", va("%d", ui_joinGameType.integer));
		UI_BuildServerDisplayList(qtrue);
		return qtrue;
	}
	return qfalse;
}

/**
 * UI_Skill_HandleKey
 */
static qboolean UI_Skill_HandleKey(int flags, float *special, int key)
{
	if ((key == K_MOUSE1) || (key == K_MOUSE2) || (key == K_ENTER) || (key == K_KP_ENTER))
	{
		int  i = trap_Cvar_VariableValue( "g_spSkill" );

		if (key == K_MOUSE2)
		{
			i--;
		}
		else
		{
			i++;
		}

		if (i < 1)
		{
			i = numSkillLevels;
		}
		else if (i > numSkillLevels)
		{
			i = 1;
		}

		trap_Cvar_Set("g_spSkill", va("%i", i));
		return qtrue;
	}
	return qfalse;
}

/**
 * UI_TeamName_HandleKey
 */
static qboolean UI_TeamName_HandleKey(int flags, float *special, int key, qboolean blue)
{
	if ((key == K_MOUSE1) || (key == K_MOUSE2) || (key == K_ENTER) || (key == K_KP_ENTER))
	{
		int  i;
		i = UI_TeamIndexFromName(UI_Cvar_VariableString((blue) ? "ui_blueTeam" : "ui_redTeam"));

		if (key == K_MOUSE2)
		{
			i--;
		}
		else
		{
			i++;
		}

		if (i >= uiInfo.teamCount)
		{
			i = 0;
		}
		else if (i < 0)
		{
			i = uiInfo.teamCount - 1;
		}

		trap_Cvar_Set((blue) ? "ui_blueTeam" : "ui_redTeam", uiInfo.teamList[i].teamName);

		return qtrue;
	}
	return qfalse;
}

/**
 * $(function)
 */
static qboolean UI_NetSource_HandleKey(int flags, float *special, int key)
{
	if ((key == K_MOUSE1) || (key == K_MOUSE2) || (key == K_ENTER) || (key == K_KP_ENTER))
	{
		if (key == K_MOUSE2)
		{
			ui_netSource.integer--;

			if (ui_netSource.integer == 1)
			{
				ui_netSource.integer = 0;
			}
		}
		else
		{
			ui_netSource.integer++;

			if (ui_netSource.integer == 1)
			{
				ui_netSource.integer = 2;
			}
		}

		if (ui_netSource.integer >= numNetSources)
		{
			ui_netSource.integer = 0;
		}
		else if (ui_netSource.integer < 0)
		{
			ui_netSource.integer = numNetSources - 1;
		}

		UI_BuildServerDisplayList(qtrue);

		if (ui_netSource.integer != AS_GLOBAL)
		{
			UI_StartServerRefresh(qtrue);
		}
		trap_Cvar_Set( "ui_netSource", va("%d", ui_netSource.integer));
		return qtrue;
	}
	return qfalse;
}

/**
 * $(function)
 */
static qboolean UI_NetFilter_HandleKey(int flags, float *special, int key)
{
	if ((key == K_MOUSE1) || (key == K_MOUSE2) || (key == K_ENTER) || (key == K_KP_ENTER))
	{
		if (key == K_MOUSE2)
		{
			ui_serverFilterType.integer--;
		}
		else
		{
			ui_serverFilterType.integer++;
		}

		if (ui_serverFilterType.integer >= numServerFilters)
		{
			ui_serverFilterType.integer = 0;
		}
		else if (ui_serverFilterType.integer < 0)
		{
			ui_serverFilterType.integer = numServerFilters - 1;
		}
		UI_BuildServerDisplayList(qtrue);
		return qtrue;
	}
	return qfalse;
}

/**
 * UI_RedBlue_HandleKey
 */
static qboolean UI_RedBlue_HandleKey(int flags, float *special, int key)
{
	if ((key == K_MOUSE1) || (key == K_MOUSE2) || (key == K_ENTER) || (key == K_KP_ENTER))
	{
		uiInfo.redBlue ^= 1;
		return qtrue;
	}
	return qfalse;
}

/**
 * UI_Crosshair_HandleKey
 */
static qboolean UI_Crosshair_HandleKey(int flags, float *special, int key)
{
	if ((key == K_MOUSE1) || (key == K_MOUSE2) || (key == K_ENTER) || (key == K_KP_ENTER))
	{
		if (key == K_MOUSE2)
		{
			uiInfo.currentCrosshair--;
		}
		else
		{
			uiInfo.currentCrosshair++;
		}

		if (uiInfo.currentCrosshair >= utCrosshairGetCount ( ))
		{
			uiInfo.currentCrosshair = 0;
		}
		else if (uiInfo.currentCrosshair < 0)
		{
			uiInfo.currentCrosshair = utCrosshairGetCount ( ) - 1;
		}
		trap_Cvar_Set("cg_drawCrosshair", va("%i", uiInfo.currentCrosshair + 1));
		return qtrue;
	}
	return qfalse;
}

/**
 * UI_SelectedPlayer_HandleKey
 */
static qboolean UI_SelectedPlayer_HandleKey(int flags, float *special, int key)
{
	if ((key == K_MOUSE1) || (key == K_MOUSE2) || (key == K_ENTER) || (key == K_KP_ENTER))
	{
		int  selected;

		UI_BuildPlayerList();

		if (!uiInfo.teamLeader)
		{
			return qfalse;
		}
		selected = trap_Cvar_VariableValue("cg_selectedPlayer");

		if (key == K_MOUSE2)
		{
			selected--;
		}
		else
		{
			selected++;
		}

		if (selected > uiInfo.myTeamCount)
		{
			selected = 0;
		}
		else if (selected < 0)
		{
			selected = uiInfo.myTeamCount;
		}

		if (selected == uiInfo.myTeamCount)
		{
			trap_Cvar_Set( "cg_selectedPlayerName", "Everyone");
		}
		else
		{
			trap_Cvar_Set( "cg_selectedPlayerName", uiInfo.teamNames[selected]);
		}
		trap_Cvar_Set( "cg_selectedPlayer", va("%d", selected));
	}
	return qfalse;
}

/**
 * UI_OwnerDrawHandleKey
 */
static qboolean UI_OwnerDrawHandleKey(int ownerDraw, int flags, float *special, int key)
{
	switch (ownerDraw)
	{
		case UI_EFFECTS:
			return UI_Effects_HandleKey(flags, special, key);

			break;

		case UI_GAMETYPE:
			return UI_GameType_HandleKey(flags, special, key, qtrue);

			break;

		case UI_NETGAMETYPE:
			return UI_NetGameType_HandleKey(flags, special, key);

			break;

		case UI_JOINGAMETYPE:
			return UI_JoinGameType_HandleKey(flags, special, key);

			break;

		case UI_SKILL:
			return UI_Skill_HandleKey(flags, special, key);

			break;

		case UI_BLUETEAMNAME:
			return UI_TeamName_HandleKey(flags, special, key, qtrue);

			break;

		case UI_REDTEAMNAME:
			return UI_TeamName_HandleKey(flags, special, key, qfalse);

			break;

		case UI_NETSOURCE:
			UI_NetSource_HandleKey(flags, special, key);
			break;

		case UI_NETFILTER:
			UI_NetFilter_HandleKey(flags, special, key);
			break;

		case UI_REDBLUE:
			UI_RedBlue_HandleKey(flags, special, key);
			break;

		case UI_CROSSHAIR:
			UI_Crosshair_HandleKey(flags, special, key);
			break;

		case UI_SELECTEDPLAYER:
			UI_SelectedPlayer_HandleKey(flags, special, key);
			break;

		default:
			break;
	}

	return qfalse;
}

/**
 * UI_GetValue
 */
static float UI_GetValue(int ownerDraw)
{
	return 0;
}

/*
=================
UI_ServersQsortCompare
=================
*/
static int QDECL UI_ServersQsortCompare( const void *arg1, const void *arg2 )
{
	return trap_LAN_CompareServers( ui_netSource.integer, uiInfo.serverStatus.sortKey, uiInfo.serverStatus.sortDir, *(int *)arg1, *(int *)arg2);
}

/*
=================
UI_ServersSort
=================
*/
void UI_ServersSort(int column, qboolean force)
{
	if (!force)
	{
		if (uiInfo.serverStatus.sortKey == column)
		{
			return;
		}
	}

	uiInfo.serverStatus.sortKey = column;
	trap_Cvar_SetValue("ui_browserSortKey", uiInfo.serverStatus.sortKey);
	trap_Cvar_SetValue("ui_browserSortDir", uiInfo.serverStatus.sortDir);
	qsort( &uiInfo.serverStatus.displayServers[0], uiInfo.serverStatus.numDisplayServers, sizeof(int), UI_ServersQsortCompare);
}

/*
===============
UI_LoadMods
===============
*/
static void UI_LoadMods(void)
{
	int   numdirs;
	char  dirlist[2048];
	char  *dirptr;
	char  *descptr;
	int   i;
	int   dirlen;

	uiInfo.modCount = 0;
	numdirs 	= trap_FS_GetFileList( "$modlist", "", dirlist, sizeof(dirlist));
	dirptr		= dirlist;

	for(i = 0; i < numdirs; i++)
	{
		dirlen					 = strlen( dirptr ) + 1;
		descptr 				 = dirptr + dirlen;
		uiInfo.modList[uiInfo.modCount].modName  = String_Alloc(dirptr);
		uiInfo.modList[uiInfo.modCount].modDescr = String_Alloc(descptr);
		dirptr					+= dirlen + strlen(descptr) + 1;
		uiInfo.modCount++;

		if (uiInfo.modCount >= MAX_MODS)
		{
			break;
		}
	}
}

/*
===============
UI_LoadTeams
===============
*/
static void UI_LoadTeams(void)
{
	char  teamList[4096];
	char  *teamName;
	int   i, len, count;

	count = trap_FS_GetFileList( "", "team", teamList, 4096 );

	if (count)
	{
		teamName = teamList;

		for (i = 0; i < count; i++)
		{
			len   = strlen( teamName );
			UI_ParseTeamInfo(teamName);
			teamName += len + 1;
		}
	}
}

/*
===============
UI_LoadMovies
===============
*/
static void UI_LoadMovies(void)
{
	char  movielist[4096];
	char  *moviename;
	int   i, len;

	uiInfo.movieCount = trap_FS_GetFileList( "video", "roq", movielist, 4096 );

	if (uiInfo.movieCount)
	{
		if (uiInfo.movieCount > MAX_MOVIES)
		{
			uiInfo.movieCount = MAX_MOVIES;
		}
		moviename = movielist;

		for (i = 0; i < uiInfo.movieCount; i++)
		{
			len = strlen( moviename );

			if (!Q_stricmp(moviename + len - 4, ".roq"))
			{
				moviename[len - 4] = '\0';
			}
			Q_strupr(moviename);
			uiInfo.movieList[i] = String_Alloc(moviename);
			moviename	   += len + 1;
		}
	}
}

/*
===============
UI_LoadDemos
===============
*/
static void UI_LoadDemos(void)	 // Added Democast Checks - DensitY
{ char		demolist[4096];
  char		demoExt[32];
  char		*demoname;
  int		i, len;
  fileHandle_t	File;

  Com_sprintf(demoExt, sizeof(demoExt), "urtdemo" );

  uiInfo.demoCount = trap_FS_GetFileList( "demos", demoExt, demolist, 4096 );

  Com_sprintf(demoExt, sizeof(demoExt), ".urtdemo" );

  if (uiInfo.demoCount)
  {
	  if (uiInfo.demoCount > MAX_DEMOS)
	  {
		  uiInfo.demoCount = MAX_DEMOS;
	  }
	  demoname = demolist;

	  for (i = 0; i < uiInfo.demoCount; i++)
	  {
		  len = strlen( demoname );

		  if (!Q_stricmp(demoname + len - strlen(demoExt), demoExt))
		  {
			  demoname[len - strlen(demoExt)] = '\0';
		  }
		  uiInfo.demoList[i] = String_Alloc(demoname);
		  demoname		+= len + 1;
		  trap_FS_FOpenFile( va( "sound/democasts/%s.wav", uiInfo.demoList[i] ), &File, FS_READ );

		  if(File)
		  {
			  uiInfo.uiDC.DemoCaster[i] = qtrue;
			  trap_FS_FCloseFile( File );
		  }
		  else
		  {
			  trap_FS_FCloseFile( File );
			  uiInfo.uiDC.DemoCaster[i] = qfalse;
		  }
	  }
  }
}

/**
 * UI_Update
 */
static void UI_Update(const char *name)
{
	int  val = trap_Cvar_VariableValue(name);

	if (Q_stricmp(name, "ui_SetName") == 0)
	{
		trap_Cvar_Set( "name", UI_Cvar_VariableString("ui_Name"));
	}
	else if (Q_stricmp(name, "ui_setRate") == 0)
	{
/*		float rate = trap_Cvar_VariableValue("rate");
		if (rate >= 5000) {
			trap_Cvar_Set("cl_maxpackets", "30");
			trap_Cvar_Set("cl_packetdup", "1");
		} else if (rate >= 4000) {
			trap_Cvar_Set("cl_maxpackets", "15");
			trap_Cvar_Set("cl_packetdup", "2"); 	// favor less prediction errors when there's packet loss
		} else {
			trap_Cvar_Set("cl_maxpackets", "15");
			trap_Cvar_Set("cl_packetdup", "1"); 	// favor lower bandwidth
		} */
	}
	else if (Q_stricmp(name, "ui_GetName") == 0)
	{
		trap_Cvar_Set( "ui_Name", UI_Cvar_VariableString("name"));
	}
	else if (Q_stricmp(name, "r_colorbits") == 0)
	{
		switch (val)
		{
			case 0:
				trap_Cvar_SetValue( "r_depthbits", 0 );
				trap_Cvar_SetValue( "r_stencilbits", 0 );
				break;

			case 16:
				trap_Cvar_SetValue( "r_depthbits", 16 );
				trap_Cvar_SetValue( "r_stencilbits", 0 );
				break;

			case 32:
				trap_Cvar_SetValue( "r_depthbits", 24 );
				break;
		}
	}
	else if (Q_stricmp(name, "r_lodbias") == 0)
	{
		switch (val)
		{
			case 0:
				trap_Cvar_SetValue( "r_subdivisions", 4 );
				break;

			case 1:
				trap_Cvar_SetValue( "r_subdivisions", 12 );
				break;

			case 2:
				trap_Cvar_SetValue( "r_subdivisions", 20 );
				break;
		}
	}
	else if (Q_stricmp(name, "ui_glCustom") == 0)
	{
		switch (val)
		{
			case 0: // high quality
				trap_Cvar_SetValue( "r_fullScreen", 1 );
				trap_Cvar_SetValue( "r_subdivisions", 4 );
				trap_Cvar_SetValue( "r_vertexlight", 0 );
				trap_Cvar_SetValue( "r_lodbias", 0 );
				trap_Cvar_SetValue( "r_colorbits", 32 );
				trap_Cvar_SetValue( "r_depthbits", 24 );
				trap_Cvar_SetValue( "r_picmip", 0 );
				trap_Cvar_SetValue( "r_mode", 4 );
				trap_Cvar_SetValue( "r_texturebits", 32 );
				trap_Cvar_SetValue( "r_fastSky", 0 );
				trap_Cvar_SetValue( "r_inGameVideo", 1 );
				trap_Cvar_SetValue( "cg_shadows", 1 );
				trap_Cvar_SetValue( "cg_brassTime", 2500 );
				trap_Cvar_Set( "r_texturemode", "GL_LINEAR_MIPMAP_LINEAR" );
				break;

			case 1: // normal
				trap_Cvar_SetValue( "r_fullScreen", 1 );
				trap_Cvar_SetValue( "r_subdivisions", 12 );
				trap_Cvar_SetValue( "r_vertexlight", 0 );
				trap_Cvar_SetValue( "r_lodbias", 0 );
				trap_Cvar_SetValue( "r_colorbits", 0 );
				trap_Cvar_SetValue( "r_depthbits", 24 );
				trap_Cvar_SetValue( "r_picmip", 0 );
				trap_Cvar_SetValue( "r_mode", 3 );
				trap_Cvar_SetValue( "r_texturebits", 0 );
				trap_Cvar_SetValue( "r_fastSky", 0 );
				trap_Cvar_SetValue( "r_inGameVideo", 1 );
				trap_Cvar_SetValue( "cg_brassTime", 2500 );
				trap_Cvar_Set( "r_texturemode", "GL_LINEAR_MIPMAP_LINEAR" );
				trap_Cvar_SetValue( "cg_shadows", 0 );
				break;

			case 2: // fast
				trap_Cvar_SetValue( "r_fullScreen", 1 );
				trap_Cvar_SetValue( "r_subdivisions", 8 );
				trap_Cvar_SetValue( "r_vertexlight", 0 );
				trap_Cvar_SetValue( "r_lodbias", 1 );
				trap_Cvar_SetValue( "r_colorbits", 0 );
				trap_Cvar_SetValue( "r_depthbits", 0 );
				trap_Cvar_SetValue( "r_picmip", 1 );
				trap_Cvar_SetValue( "r_mode", 3 );
				trap_Cvar_SetValue( "r_texturebits", 0 );
				trap_Cvar_SetValue( "cg_shadows", 0 );
				trap_Cvar_SetValue( "r_fastSky", 1 );
				trap_Cvar_SetValue( "r_inGameVideo", 0 );
				trap_Cvar_SetValue( "cg_brassTime", 0 );
				trap_Cvar_Set( "r_texturemode", "GL_LINEAR_MIPMAP_NEAREST" );
				break;

			case 3: // fastest
				trap_Cvar_SetValue( "r_fullScreen", 1 );
				trap_Cvar_SetValue( "r_subdivisions", 20 );
				trap_Cvar_SetValue( "r_vertexlight", 1 );
				trap_Cvar_SetValue( "r_lodbias", 2 );
				trap_Cvar_SetValue( "r_colorbits", 16 );
				trap_Cvar_SetValue( "r_depthbits", 16 );
				trap_Cvar_SetValue( "r_mode", 3 );
				trap_Cvar_SetValue( "r_picmip", 2 );
				trap_Cvar_SetValue( "r_texturebits", 16 );
				trap_Cvar_SetValue( "cg_shadows", 0 );
				trap_Cvar_SetValue( "cg_brassTime", 0 );
				trap_Cvar_SetValue( "r_fastSky", 1 );
				trap_Cvar_SetValue( "r_inGameVideo", 0 );
				trap_Cvar_Set( "r_texturemode", "GL_LINEAR_MIPMAP_NEAREST" );
				break;
		}
	}
	else if (Q_stricmp(name, "ui_mousePitch") == 0)
	{
		if (val == 0)
		{
			trap_Cvar_SetValue( "m_pitch", 0.022f );
		}
		else
		{
			trap_Cvar_SetValue( "m_pitch", -0.022f );
		}
	}
}

/*
	Urban Terror 3.0

	New Menu script functions
	DensitY, 2003

	* Menu Based Functions

		Rcon_Kick
		Rcon_Slap
		Rcon_Ban
*/

static void UI_RunMenuScript(itemDef_t *item, char * *args)
{
	const char	*name, *name2;
	char		buff[1024];

	if (String_Parse(args, &name))
	{
		//
		// New 3.0 Functions - DensitY
		//
		if(!Q_stricmp( name, "Rcon_Kick" ))
		{
			trap_Cmd_ExecuteText( EXEC_APPEND, va( "rcon clientkick %d\n", uiInfo.playerClientNum[uiInfo.playerIndex] ));
		}
		else if(!Q_stricmp( name, "Rcon_Mute" ))
		{
			trap_Cmd_ExecuteText( EXEC_APPEND, va( "rcon mute %d\n", uiInfo.playerClientNum[uiInfo.playerIndex] ));
		}
		else if(!Q_stricmp( name, "Rcon_Ban2" ))
		{
			trap_Cmd_ExecuteText( EXEC_APPEND, va( "rcon addip %d\n", uiInfo.playerClientNum[uiInfo.playerIndex] ));
		}
		else if(!Q_stricmp( name, "Rcon_Slap" ))
		{
			trap_Cmd_ExecuteText( EXEC_APPEND, va( "rcon slap %d\n", uiInfo.playerClientNum[uiInfo.playerIndex] ));
		}
		//@Fenix - new Nuke button
		else if(!Q_stricmp( name, "Rcon_Nuke" ))
		{
			trap_Cmd_ExecuteText( EXEC_APPEND, va( "rcon nuke %d\n", uiInfo.playerClientNum[uiInfo.playerIndex] ));
		}
		//@Fenix - new Smite button
		else if(!Q_stricmp( name, "Rcon_Smite" ))
		{
			trap_Cmd_ExecuteText( EXEC_APPEND, va( "rcon smite %d\n", uiInfo.playerClientNum[uiInfo.playerIndex] ));
		}
		else if(!Q_stricmp( name, "Rcon_Ban" ))
		{
			// Note: PB must be enabled for this to work! (use ban2)
			trap_Cmd_ExecuteText( EXEC_APPEND, va( "rcon pb_sv_ban %s\n", uiInfo.playerNames[uiInfo.playerIndex] ));
		}
		//
		// End.
		//
		else if (Q_stricmp ( name, "BuildHeadList" ) == 0)
		{
			//UI_BuildQ3Model_List ( );
		}
		else if (Q_stricmp ( name, "getStats" ) == 0)
		{
			trap_Cvar_Set( "cl_paused", "0" );
			getStats();
		}
		else if (Q_stricmp(name, "StartServer") == 0)
		{
			int    i, clients;
			float  skill;
			trap_Cvar_Set("cg_thirdPerson", "0");
			trap_Cvar_Set("cg_cameraOrbit", "0");
			trap_Cvar_Set("ui_singlePlayerActive", "0");
			trap_Cvar_SetValue( "dedicated", Com_Clamp( 0, 2, ui_dedicated.integer ));
			trap_Cvar_SetValue( "g_gametype", Com_Clamp( 0, GT_MAX_GAME_TYPE, uiInfo.gameTypes[ui_netGameType.integer].gtEnum ));
			trap_Cmd_ExecuteText( EXEC_APPEND, va( "wait ; wait ; map %s\n", uiInfo.mapList[ui_currentNetMap.integer].mapLoadName ));
			skill = trap_Cvar_VariableValue( "g_spSkill" );

			// set max clients based on spots
			clients = 0;

			for (i = 0; i < PLAYERS_PER_TEAM; i++)
			{
				int  bot = trap_Cvar_VariableValue( va("ui_blueteam%i", i + 1));

				if (bot >= 0)
				{
					clients++;
				}
				bot = trap_Cvar_VariableValue( va("ui_redteam%i", i + 1));

				if (bot >= 0)
				{
					clients++;
				}
			}

			if (clients == 0)
			{
				clients = 8;
			}

			if (clients > trap_Cvar_VariableValue( "sv_maxClients" ))
			{
				trap_Cvar_Set("sv_maxClients", va("%d", clients));
			}

			for (i = 0; i < PLAYERS_PER_TEAM; i++)
			{
				int  bot = trap_Cvar_VariableValue( va("ui_blueteam%i", i + 1));

				if (bot > 1)
				{
					if (ui_actualNetGameType.integer >= GT_TEAM)
					{
//						Com_sprintf( buff, sizeof(buff), "addbot %s %f %s\n", uiInfo.characterList[bot-2].name, skill, "Blue");
						Com_sprintf( buff, sizeof(buff), "addbot %s %f %s\n", UI_GetBotNameByNumber(bot - 2), skill, "Blue");
					}
					else
					{
						Com_sprintf( buff, sizeof(buff), "addbot %s %f \n", UI_GetBotNameByNumber(bot - 2), skill);
					}
					trap_Cmd_ExecuteText( EXEC_APPEND, buff );
				}
				bot = trap_Cvar_VariableValue( va("ui_redteam%i", i + 1));

				if (bot > 1)
				{
					if (ui_actualNetGameType.integer >= GT_TEAM)
					{
//						Com_sprintf( buff, sizeof(buff), "addbot %s %f %s\n", uiInfo.characterList[bot-2].name, skill, "Red");
						Com_sprintf( buff, sizeof(buff), "addbot %s %f %s\n", UI_GetBotNameByNumber(bot - 2), skill, "Red");
					}
					else
					{
						Com_sprintf( buff, sizeof(buff), "addbot %s %f \n", UI_GetBotNameByNumber(bot - 2), skill);
					}
					trap_Cmd_ExecuteText( EXEC_APPEND, buff );
				}
			}
		}
		else if (Q_stricmp(name, "updateSPMenu") == 0)
		{
			UI_SetCapFragLimits(qtrue);
			UI_MapCountByGameType(qtrue);
			ui_mapIndex.integer = UI_GetIndexFromSelection(ui_currentMap.integer);
			trap_Cvar_Set("ui_mapIndex", va("%d", ui_mapIndex.integer));
			Menu_SetFeederSelection(NULL, FEEDER_MAPS, ui_mapIndex.integer, "skirmish");
			UI_GameType_HandleKey(0, 0, K_MOUSE1, qfalse);
			UI_GameType_HandleKey(0, 0, K_MOUSE2, qfalse);
		}
		else if (Q_stricmp(name, "resetDefaults") == 0)
		{
			trap_Cmd_ExecuteText( EXEC_APPEND, "exec default.cfg\n");
			trap_Cmd_ExecuteText( EXEC_APPEND, "cvar_restart\n");
			Controls_SetDefaults();
			trap_Cvar_Set("com_introPlayed", "1" );
			trap_Cmd_ExecuteText( EXEC_APPEND, "vid_restart\n" );
		}
		else if (Q_stricmp(name, "loadArenas") == 0)
		{
			UI_LoadArenas();

			UI_MapCountByGameType(qfalse);
			Menu_SetFeederSelection(NULL, FEEDER_ALLMAPS, 0, "createserver");
		}
		else if (Q_stricmp(name, "saveControls") == 0)
		{
			Controls_SetConfig(qtrue);
		}
		else if (Q_stricmp(name, "loadControls") == 0)
		{
			Controls_GetConfig();
		}
		else if (Q_stricmp(name, "clearError") == 0)
		{
			trap_Cvar_Set("com_errorMessage", "");
		}
		else if (Q_stricmp(name, "loadGameInfo") == 0)
		{
#ifdef PRE_RELEASE_TADEMO
			UI_ParseGameInfo("demogameinfo.txt");
#else
			UI_ParseGameInfo("gameinfo.txt");
#endif
			UI_LoadBestScores(uiInfo.mapList[ui_currentMap.integer].mapLoadName, uiInfo.gameTypes[ui_gameType.integer].gtEnum);
		}
		else if (Q_stricmp(name, "resetScores") == 0)
		{
			UI_ClearScores();
		}
		else if (Q_stricmp(name, "RefreshServers") == 0)
		{
			UI_StartServerRefresh(qtrue);
			UI_BuildServerDisplayList(qtrue);
		}
		else if (Q_stricmp(name, "RefreshFilter") == 0)
		{
			UI_StartServerRefresh(qfalse);
			UI_BuildServerDisplayList(qtrue);
		}
		else if (Q_stricmp(name, "RunSPDemo") == 0)
		{
			if (uiInfo.demoAvailable)
			{
				trap_Cmd_ExecuteText( EXEC_APPEND, va("demo %s_%i", uiInfo.mapList[ui_currentMap.integer].mapLoadName, uiInfo.gameTypes[ui_gameType.integer].gtEnum));
			}
		}
		else if (Q_stricmp(name, "LoadDemos") == 0)
		{
			UI_LoadDemos();
		}
		else if (Q_stricmp(name, "LoadMovies") == 0)
		{
			UI_LoadMovies();
		}
		else if (Q_stricmp(name, "LoadMods") == 0)
		{
			UI_LoadMods();
		}
		else if (Q_stricmp(name, "playMovie") == 0)
		{
			if (uiInfo.previewMovie >= 0)
			{
				trap_CIN_StopCinematic(uiInfo.previewMovie);
			}
			trap_Cmd_ExecuteText( EXEC_APPEND, va("cinematic %s.roq 2\n", uiInfo.movieList[uiInfo.movieIndex]));
		}
		else if (Q_stricmp(name, "RunMod") == 0)
		{
			trap_Cvar_Set( "fs_game", uiInfo.modList[uiInfo.modIndex].modName);
			trap_Cmd_ExecuteText( EXEC_APPEND, "vid_restart;" );
		}
		else if (Q_stricmp(name, "Rundemo") == 0)	  // changed
		{
			char  demoname[1024];
			strncpy( demoname, uiInfo.demoList[uiInfo.demoIndex], strlen( uiInfo.demoList[uiInfo.demoIndex] ) + 1 );
			trap_Cvar_Set ("cg_demoName", demoname);
			trap_Cmd_ExecuteText( EXEC_APPEND, va("demo %s\n", uiInfo.demoList[uiInfo.demoIndex]));
		}
		else if (Q_stricmp(name, "Quake3") == 0)
		{
			trap_Cvar_Set( "fs_game", "");
			trap_Cmd_ExecuteText( EXEC_APPEND, "vid_restart;" );
		}
		else if (Q_stricmp(name, "closeJoin") == 0)
		{
			if (uiInfo.serverStatus.refreshActive)
			{
				UI_StopServerRefresh();
				uiInfo.serverStatus.nextDisplayRefresh = 0;
				uiInfo.nextServerStatusRefresh		   = 0;
				uiInfo.nextFindPlayerRefresh		   = 0;
				UI_BuildServerDisplayList(qtrue);
			}
			else
			{
				Menus_CloseByName("joinserver");
				Menus_OpenByName("main");
			}
		}
		else if (Q_stricmp(name, "StopRefresh") == 0)
		{
			UI_StopServerRefresh();
			uiInfo.serverStatus.nextDisplayRefresh = 0;
			uiInfo.nextServerStatusRefresh		   = 0;
			uiInfo.nextFindPlayerRefresh		   = 0;
		}

#ifdef USE_AUTH
		//@Barbatos: create or update the authentication key
		else if (Q_stricmp(name, "updateAuthKey") == 0)
		{
			CL_Auth_Cmd_f( UI_Cvar_VariableString("cl_auth_key") );
		}

		//@Barbatos: called when the player wants to delete his auth key
		else if (Q_stricmp(name, "deleteAuthKey") == 0)
		{
			CL_Auth_Cmd_f( "0" );
		}
#endif

		else if (Q_stricmp(name, "UpdateFilter") == 0)
		{
			uiInfo.serverStatus.sortKey = trap_Cvar_VariableValue("ui_browserSortKey");
			uiInfo.serverStatus.sortDir = trap_Cvar_VariableValue("ui_browserSortDir");

			if (ui_netSource.integer == AS_LOCAL)
			{
				UI_StartServerRefresh(qtrue);
			}
			UI_BuildServerDisplayList(qtrue);
			UI_FeederSelection(FEEDER_SERVERS, 0);

			UI_ServersSort(trap_Cvar_VariableValue("ui_browserSortKey"), qtrue);
		}
		else if (Q_stricmp(name, "ServerStatus") == 0)
		{
			trap_LAN_GetServerAddressString(ui_netSource.integer, uiInfo.serverStatus.displayServers[uiInfo.serverStatus.currentServer], uiInfo.serverStatusAddress, sizeof(uiInfo.serverStatusAddress));
			UI_BuildServerStatus(qtrue);
		}
		else if (Q_stricmp(name, "FoundPlayerServerStatus") == 0)
		{
			Q_strncpyz(uiInfo.serverStatusAddress, uiInfo.foundPlayerServerAddresses[uiInfo.currentFoundPlayerServer], sizeof(uiInfo.serverStatusAddress));
			UI_BuildServerStatus(qtrue);
			Menu_SetFeederSelection(NULL, FEEDER_FINDPLAYER, 0, NULL);
		}
		else if (Q_stricmp(name, "FindPlayer") == 0)
		{
			UI_BuildFindPlayerList(qtrue);
			// clear the displayed server status info
			uiInfo.serverStatusInfo.numLines = 0;
			Menu_SetFeederSelection(NULL, FEEDER_FINDPLAYER, 0, NULL);
		}
		else if (Q_stricmp(name, "JoinServer") == 0)
		{
			trap_Cvar_Set("cg_thirdPerson", "0");
			trap_Cvar_Set("cg_cameraOrbit", "0");
			trap_Cvar_Set("ui_singlePlayerActive", "0");

			if ((uiInfo.serverStatus.currentServer >= 0) && (uiInfo.serverStatus.currentServer < uiInfo.serverStatus.numDisplayServers))
			{
				trap_LAN_GetServerAddressString(ui_netSource.integer, uiInfo.serverStatus.displayServers[uiInfo.serverStatus.currentServer], buff, 1024);
				trap_Cmd_ExecuteText( EXEC_APPEND, va( "connect %s\n", buff ));
			}
		}
		else if (Q_stricmp(name, "FoundPlayerJoinServer") == 0)
		{
			trap_Cvar_Set("ui_singlePlayerActive", "0");

			if ((uiInfo.currentFoundPlayerServer >= 0) && (uiInfo.currentFoundPlayerServer < uiInfo.numFoundPlayerServers))
			{
				trap_Cmd_ExecuteText( EXEC_APPEND, va( "connect %s\n", uiInfo.foundPlayerServerAddresses[uiInfo.currentFoundPlayerServer] ));
			}
		}
		else if (Q_stricmp(name, "Quit") == 0)
		{
			trap_Cvar_Set("ui_singlePlayerActive", "0");
			trap_Cmd_ExecuteText( EXEC_NOW, "quit");
		}
		else if (Q_stricmp(name, "Controls") == 0)
		{
			trap_Cvar_Set( "cl_paused", "1" );
			trap_Key_SetCatcher( KEYCATCH_UI );
			Menus_CloseAll();
			Menus_ActivateByName("setup_menu2");
		}
		else if (Q_stricmp(name, "Leave") == 0)
		{
			trap_Cmd_ExecuteText( EXEC_APPEND, "disconnect\n" );
			trap_Key_SetCatcher( KEYCATCH_UI );
			Menus_CloseAll();
			Menus_ActivateByName("main");
		}
		else if (Q_stricmp(name, "ServerSort") == 0)
		{
			int  sortColumn;

			if (Int_Parse(args, &sortColumn))
			{
				// if same column we're already sorting on then flip the direction
				if (sortColumn == uiInfo.serverStatus.sortKey)
				{
					uiInfo.serverStatus.sortDir = !uiInfo.serverStatus.sortDir;
				}
				// make sure we sort again
				UI_ServersSort(sortColumn, qtrue);
			}
		}
		else if (Q_stricmp(name, "closeingame") == 0)
		{
			trap_Key_SetCatcher( trap_Key_GetCatcher() & ~KEYCATCH_UI );
			trap_Key_ClearStates();
			trap_Cvar_Set( "cl_paused", "0" );
			Menus_CloseAll();
		}
		else if (Q_stricmp(name, "voteMap") == 0)
		{
			if ((ui_currentNetMap.integer >= 0) && (ui_currentNetMap.integer < uiInfo.mapCount))
			{
				trap_Cmd_ExecuteText( EXEC_APPEND, va("callvote map %s\n", uiInfo.mapList[ui_currentNetMap.integer].mapLoadName));
			}
		}
		else if (Q_stricmp(name, "voteNextMap") == 0)
		{
			if ((ui_currentNetMap.integer >= 0) && (ui_currentNetMap.integer < uiInfo.mapCount))
			{
				trap_Cmd_ExecuteText( EXEC_APPEND, va("callvote nextmap %s\n", uiInfo.mapList[ui_currentNetMap.integer].mapLoadName));
			}
		}
		else if (Q_stricmp(name, "voteKick") == 0)
		{
			if ((uiInfo.playerIndex >= 0) && (uiInfo.playerIndex < uiInfo.playerCount))
			{
				//@Fenix - fix empty reason
				//char	reason[MAX_STRING_CHARS];
				//trap_Cvar_VariableStringBuffer("ui_kickreason", reason, sizeof(reason));
				//trap_Cvar_Set("ui_kickreason", "");

				trap_Cmd_ExecuteText( EXEC_APPEND, va("callvote kick %d\n", uiInfo.playerClientNum[uiInfo.playerIndex]));
			}
		}
		else if (Q_stricmp(name, "voteGame") == 0)
		{
			if ((ui_netGameType.integer >= 0) && (ui_netGameType.integer < uiInfo.numGameTypes))
			{
				trap_Cmd_ExecuteText( EXEC_APPEND, va("callvote g_gametype %i\n", uiInfo.gameTypes[ui_netGameType.integer].gtEnum));
			}
		}
		else if (Q_stricmp(name, "voteLeader") == 0)
		{
			if ((uiInfo.teamIndex >= 0) && (uiInfo.teamIndex < uiInfo.myTeamCount))
			{
				trap_Cmd_ExecuteText( EXEC_APPEND, va("callteamvote leader %s\n", uiInfo.teamNames[uiInfo.teamIndex]));
			}
		}
		else if (Q_stricmp(name, "addBot") == 0)
		{
//			if (trap_Cvar_VariableValue("g_gametype") >= GT_TEAM) {
//				trap_Cmd_ExecuteText( EXEC_APPEND, va("addbot %s %i %s\n", uiInfo.characterList[uiInfo.botIndex].name, uiInfo.skillIndex+1, (uiInfo.redBlue == 0) ? "Red" : "Blue") );
//			} else {
			trap_Cmd_ExecuteText( EXEC_APPEND, va("addbot %s %i %s\n", UI_GetBotNameByNumber(uiInfo.botIndex), uiInfo.skillIndex + 1, (uiInfo.redBlue == 0) ? "Red" : "Blue"));
//			}
		}
		else if (Q_stricmp(name, "addFavorite") == 0)
		{
			if (ui_netSource.integer != AS_FAVORITES)
			{
				char  name[MAX_NAME_LENGTH];
				char  addr[MAX_NAME_LENGTH];
				int   res;

				trap_LAN_GetServerInfo(ui_netSource.integer, uiInfo.serverStatus.displayServers[uiInfo.serverStatus.currentServer], buff, MAX_STRING_CHARS);
				name[0] = addr[0] = '\0';
				Q_strncpyz(name, Info_ValueForKey(buff, "hostname"), MAX_NAME_LENGTH);
				Q_strncpyz(addr, Info_ValueForKey(buff, "addr"), MAX_NAME_LENGTH);

				if ((strlen(name) > 0) && (strlen(addr) > 0))
				{
					res = trap_LAN_AddServer(AS_FAVORITES, name, addr);

					if (res == 0)
					{
						// server already in the list
						Com_Printf("Favorite already in list\n");
					}
					else if (res == -1)
					{
						// list full
						Com_Printf("Favorite list full\n");
					}
					else
					{
						// successfully added
						Com_Printf("Added favorite server %s\n", addr);
					}
				}
			}
		}
		else if (Q_stricmp(name, "deleteFavorite") == 0)
		{
			if (ui_netSource.integer == AS_FAVORITES)
			{
				char  addr[MAX_NAME_LENGTH];
				trap_LAN_GetServerInfo(ui_netSource.integer, uiInfo.serverStatus.displayServers[uiInfo.serverStatus.currentServer], buff, MAX_STRING_CHARS);
				addr[0] = '\0';
				Q_strncpyz(addr, Info_ValueForKey(buff, "addr"), MAX_NAME_LENGTH);

				if (strlen(addr) > 0)
				{
					trap_LAN_RemoveServer(AS_FAVORITES, addr);
				}
			}
		}
		else if (Q_stricmp(name, "createFavorite") == 0)
		{
			if (ui_netSource.integer == AS_FAVORITES)
			{
				char  name[MAX_NAME_LENGTH];
				char  addr[MAX_NAME_LENGTH];
				int   res;

				name[0] = addr[0] = '\0';
				Q_strncpyz(name, UI_Cvar_VariableString("ui_favoriteName"), MAX_NAME_LENGTH);
				Q_strncpyz(addr, UI_Cvar_VariableString("ui_favoriteAddress"), MAX_NAME_LENGTH);

				if ((strlen(name) > 0) && (strlen(addr) > 0))
				{
					res = trap_LAN_AddServer(AS_FAVORITES, name, addr);

					if (res == 0)
					{
						// server already in the list
						Com_Printf("Favorite already in list\n");
					}
					else if (res == -1)
					{
						// list full
						Com_Printf("Favorite list full\n");
					}
					else
					{
						// successfully added
						Com_Printf("Added favorite server %s\n", addr);
					}
				}
			}
		}
		else if (Q_stricmp(name, "orders") == 0)
		{
			const char	*orders;

			if (String_Parse(args, &orders))
			{
				int  selectedPlayer = trap_Cvar_VariableValue("cg_selectedPlayer");

				if (selectedPlayer < uiInfo.myTeamCount)
				{
					strcpy(buff, orders);
					trap_Cmd_ExecuteText( EXEC_APPEND, va(buff, uiInfo.teamClientNums[selectedPlayer]));
					trap_Cmd_ExecuteText( EXEC_APPEND, "\n" );
				}
				else
				{
					int  i;

					for (i = 0; i < uiInfo.myTeamCount; i++)
					{
						if (Q_stricmp(UI_Cvar_VariableString("name"), uiInfo.teamNames[i]) == 0)
						{
							continue;
						}
						strcpy(buff, orders);
						trap_Cmd_ExecuteText( EXEC_APPEND, va(buff, uiInfo.teamNames[i]));
						trap_Cmd_ExecuteText( EXEC_APPEND, "\n" );
					}
				}
				trap_Key_SetCatcher( trap_Key_GetCatcher() & ~KEYCATCH_UI );
				trap_Key_ClearStates();
				trap_Cvar_Set( "cl_paused", "0" );
				Menus_CloseAll();
			}
		}
		else if (Q_stricmp(name, "voiceOrdersTeam") == 0)
		{
			const char	*orders;

			if (String_Parse(args, &orders))
			{
				int  selectedPlayer = trap_Cvar_VariableValue("cg_selectedPlayer");

				if (selectedPlayer == uiInfo.myTeamCount)
				{
					trap_Cmd_ExecuteText( EXEC_APPEND, orders );
					trap_Cmd_ExecuteText( EXEC_APPEND, "\n" );
				}
				trap_Key_SetCatcher( trap_Key_GetCatcher() & ~KEYCATCH_UI );
				trap_Key_ClearStates();
				trap_Cvar_Set( "cl_paused", "0" );
				Menus_CloseAll();
			}
		}
		else if (Q_stricmp(name, "voiceOrders") == 0)
		{
			const char	*orders;

			if (String_Parse(args, &orders))
			{
				int  selectedPlayer = trap_Cvar_VariableValue("cg_selectedPlayer");

				if (selectedPlayer < uiInfo.myTeamCount)
				{
					strcpy(buff, orders);
					trap_Cmd_ExecuteText( EXEC_APPEND, va(buff, uiInfo.teamClientNums[selectedPlayer]));
					trap_Cmd_ExecuteText( EXEC_APPEND, "\n" );
				}
				trap_Key_SetCatcher( trap_Key_GetCatcher() & ~KEYCATCH_UI );
				trap_Key_ClearStates();
				trap_Cvar_Set( "cl_paused", "0" );
				Menus_CloseAll();
			}
		}
		else if (Q_stricmp(name, "glCustom") == 0)
		{
			trap_Cvar_Set("ui_glCustom", "4");
		}
		else if (Q_stricmp(name, "update") == 0)
		{
			if (String_Parse(args, &name2))
			{
				UI_Update(name2);
			}
		}
		else if (Q_stricmp(name, "gearRead" ) == 0)
		{
			utGear_t  gear;

			if (!defferedConfig[0])
			{
				trap_Cvar_VariableStringBuffer ( "gear", defferedConfig, 20 );
			}

			utGear_Decompress ( defferedConfig, &gear );

			Gear_SetItem ( GEAR_PRIMARY, gear.primary );
			Gear_SetItem ( GEAR_SECONDARY, gear.secondary );
			Gear_SetItem ( GEAR_SIDEARM, gear.sidearm );
			Gear_SetItem ( GEAR_GRENADE, gear.grenade );
			Gear_SetItem ( GEAR_ITEM_1, gear.item1 );
			Gear_SetItem ( GEAR_ITEM_2, gear.item2 );
			Gear_SetItem ( GEAR_ITEM_3, gear.item3 );
		}
		else if (Q_stricmp(name, "gearSetActive" ) == 0)
		{
			uiInfo.gearActive = item->window.ownerDrawParam;
		}
		else if (Q_stricmp(name, "gearSetItem" ) == 0)
		{
			utGear_t  gear;

			Gear_SetItem ( uiInfo.gearActive, ItemTable[item->window.ownerDrawParam] );

			if(GearTable[GEAR_ITEM_2][uiInfo.gearSelect[GEAR_ITEM_2]] == GearTable[GEAR_ITEM_1][uiInfo.gearSelect[GEAR_ITEM_1]])
			{
				Gear_SetItem ( GEAR_ITEM_2, UT_ITEM_NONE );
			}

			if(GearTable[GEAR_ITEM_3][uiInfo.gearSelect[GEAR_ITEM_3]] == GearTable[GEAR_ITEM_1][uiInfo.gearSelect[GEAR_ITEM_1]])
			{
				Gear_SetItem ( GEAR_ITEM_3, UT_ITEM_NONE );
			}

			if(GearTable[GEAR_ITEM_3][uiInfo.gearSelect[GEAR_ITEM_3]] == GearTable[GEAR_ITEM_2][uiInfo.gearSelect[GEAR_ITEM_2]])
			{
				Gear_SetItem ( GEAR_ITEM_3, UT_ITEM_NONE );
			}

			if(GearTable[GEAR_SECONDARY][uiInfo.gearSelect[GEAR_SECONDARY]] == GearTable[GEAR_PRIMARY][uiInfo.gearSelect[GEAR_PRIMARY]])
			{
				Gear_SetItem ( GEAR_SECONDARY, UT_ITEM_NONE );
			}

			gear.primary   = GearTable[GEAR_PRIMARY][uiInfo.gearSelect[GEAR_PRIMARY]];
			gear.secondary = GearTable[GEAR_SECONDARY][uiInfo.gearSelect[GEAR_SECONDARY]];
			gear.sidearm   = GearTable[GEAR_SIDEARM][uiInfo.gearSelect[GEAR_SIDEARM]];
			gear.grenade   = GearTable[GEAR_GRENADE][uiInfo.gearSelect[GEAR_GRENADE]];
			gear.item1	   = GearTable[GEAR_ITEM_1][uiInfo.gearSelect[GEAR_ITEM_1]];
			gear.item2	   = uiInfo.gearItemCount > 1 ? GearTable[GEAR_ITEM_2][uiInfo.gearSelect[GEAR_ITEM_2]] : UT_ITEM_NONE;
			gear.item3	   = uiInfo.gearItemCount > 2 ? GearTable[GEAR_ITEM_3][uiInfo.gearSelect[GEAR_ITEM_3]] : UT_ITEM_NONE;

			if (gear.primary == UT_ITEM_NEGEV)
			{
				gear.secondary = UT_ITEM_NONE;
			}

			if(!gear.primary)
				gear.primary = UT_ITEM_G36;

			utGear_Compress ( &gear, defferedConfig );
		}
		else if(Q_stricmp(name, "gearWrite") == 0)
		{
			trap_Cvar_Set ( "gear", defferedConfig );
		}
		else if (Q_stricmp(name, "keyboardSelectOption" ) == 0)
		{
			if (ui_debug.integer)
				Com_Printf("ut_interface %d %d\n", currStopID, item->window.ownerDrawParam);
			trap_Cmd_ExecuteText( EXEC_APPEND, va("ut_interface %d %d\n", currStopID, item->window.ownerDrawParam));
		}
		else
		{
			Com_Printf("unknown UI script %s\n", name);
		}
	}
}

/**
 * UI_GetTeamColor
 */
static void UI_GetTeamColor(vec4_t *color)
{
}

/*
==================
UI_MapCountByGameType
==================
*/
static int UI_MapCountByGameType(qboolean singlePlayer)
{
	int  i, c, game;

	c	 = 0;
	game = singlePlayer ? uiInfo.gameTypes[ui_gameType.integer].gtEnum : uiInfo.gameTypes[ui_netGameType.integer].gtEnum;

	if ((game == GT_TEAM) || (game == GT_LASTMAN))
	{
		game = GT_FFA;
	}

	for (i = 0; i < uiInfo.mapCount; i++)
	{
		uiInfo.mapList[i].active = qfalse;

		if (uiInfo.mapList[i].typeBits & (1 << game))
		{
			c++;
			uiInfo.mapList[i].active = qtrue;
		}
	}
	return c;
}

/*
==================
UI_InsertServerIntoDisplayList
==================
*/
static void UI_InsertServerIntoDisplayList(int num, int position)
{
	int  i;

	if ((position < 0) || (position > uiInfo.serverStatus.numDisplayServers))
	{
		return;
	}
	//
	uiInfo.serverStatus.numDisplayServers++;

	for (i = uiInfo.serverStatus.numDisplayServers; i > position; i--)
	{
		uiInfo.serverStatus.displayServers[i] = uiInfo.serverStatus.displayServers[i - 1];
	}
	uiInfo.serverStatus.displayServers[position] = num;
}

/*
==================
UI_RemoveServerFromDisplayList
==================
*/
static void UI_RemoveServerFromDisplayList(int num)
{
	int  i, j;

	for (i = 0; i < uiInfo.serverStatus.numDisplayServers; i++)
	{
		if (uiInfo.serverStatus.displayServers[i] == num)
		{
			uiInfo.serverStatus.numDisplayServers--;

			for (j = i; j < uiInfo.serverStatus.numDisplayServers; j++)
			{
				uiInfo.serverStatus.displayServers[j] = uiInfo.serverStatus.displayServers[j + 1];
			}
			return;
		}
	}
}

/*
==================
UI_BinaryServerInsertion
==================
*/
static void UI_BinaryServerInsertion(int num)
{
	int  mid, offset, res, len;

	// use binary search to insert server
	len    = uiInfo.serverStatus.numDisplayServers;
	mid    = len;
	offset = 0;
	res    = 0;

	while(mid > 0)
	{
		mid = len >> 1;
		//
		res = trap_LAN_CompareServers( ui_netSource.integer, uiInfo.serverStatus.sortKey,
						   uiInfo.serverStatus.sortDir, num, uiInfo.serverStatus.displayServers[offset + mid]);

		// if equal
		if (res == 0)
		{
			UI_InsertServerIntoDisplayList(num, offset + mid);
			return;
		}
		// if larger
		else if (res == 1)
		{
			offset += mid;
			len    -= mid;
		}
		// if smaller
		else
		{
			len -= mid;
		}
	}

	if (res == 1)
	{
		offset++;
	}
	UI_InsertServerIntoDisplayList(num, offset);
}

/*
==================
UI_BuildServerDisplayList
==================
*/
static void UI_BuildServerDisplayList(qboolean force)
{
	int 	i, count, clients, len, visible, ping, s;
	char		info[MAX_STRING_CHARS];

	if (!(force || (uiInfo.uiDC.realTime > uiInfo.serverStatus.nextDisplayRefresh)))
	{
		return;
	}

	// if we shouldn't reset
	if (force == 2)
	{
		force = 0;
	}

	// do motd updates here too
	trap_Cvar_VariableStringBuffer( "cl_motdString", uiInfo.serverStatus.motd, sizeof(uiInfo.serverStatus.motd));
	len = strlen(uiInfo.serverStatus.motd);

	if (len == 0)
	{
		strcpy(uiInfo.serverStatus.motd, "Welcome to Urban Terror!");
		len = strlen(uiInfo.serverStatus.motd);
	}

	if (len != uiInfo.serverStatus.motdLen)
	{
		uiInfo.serverStatus.motdLen   = len;
		uiInfo.serverStatus.motdWidth = -1;
	}

	if (force)
	{
		// clear number of displayed servers
		uiInfo.serverStatus.numDisplayServers	= 0;
		uiInfo.serverStatus.numPlayersOnServers = 0;
		// set list box index to zero
		Menu_SetFeederSelection(NULL, FEEDER_SERVERS, 0, NULL);
		// mark all servers as visible so we store ping updates for them
		trap_LAN_MarkServerVisible(ui_netSource.integer, -1, qtrue);
	}

	// get the server count (comes from the master)
	count = trap_LAN_GetServerCount(ui_netSource.integer);

	if ((count == -1) || ((ui_netSource.integer == AS_LOCAL) && (count == 0)))
	{
		// still waiting on a response from the master
		uiInfo.serverStatus.numDisplayServers	= 0;
		uiInfo.serverStatus.numPlayersOnServers = 0;
		uiInfo.serverStatus.nextDisplayRefresh	= uiInfo.uiDC.realTime + 500;
		return;
	}

	visible = qfalse;

	for (i = 0; i < count; i++)
	{
		// if we already got info for this server
		if (!trap_LAN_ServerIsVisible(ui_netSource.integer, i))
		{
			continue;
		}
		visible = qtrue;
		// get the ping for this server
		ping	= trap_LAN_GetServerPing(ui_netSource.integer, i);

		if ((ping > 0) || (ui_netSource.integer == AS_FAVORITES))
		{
			trap_LAN_GetServerInfo(ui_netSource.integer, i, info, MAX_STRING_CHARS);
			clients = atoi(Info_ValueForKey(info, "clients"));

			uiInfo.serverStatus.numPlayersOnServers += clients;

			//@Barbatos - auth system
            // gsigms: any | strict | open
			if (ui_browserShowAccounts.integer != 0)
			{
				s = atoi(Info_ValueForKey(info, "auth"));

				if ( (ui_browserShowAccounts.integer == 1 && s == 0) ||
                     (ui_browserShowAccounts.integer == 2 && s != 0) )
				{
					trap_LAN_MarkServerVisible(ui_netSource.integer, i, qfalse);
					continue;
				}
			}

			//@Barbatos - passworded servers filter
            //gsigms: inverted the original logic
			if (ui_browserShowPrivate.integer != 0)
			{
				s = atoi(Info_ValueForKey(info, "password"));

				if ( (ui_browserShowPrivate.integer == 1 && s == 1) ||
                     (ui_browserShowPrivate.integer == 2 && s != 1) )
				{
					trap_LAN_MarkServerVisible(ui_netSource.integer, i, qfalse);
					continue;
				}

				#ifdef USE_AUTH

				s = atoi(Info_ValueForKey(info, "auth"));

				if( s==2 ) // reserved to group(s)
				{
					trap_LAN_MarkServerVisible(ui_netSource.integer, i, qfalse);
					continue;
				}

				if( s<0 && cl_auth_notoriety.integer<-s ) // reserved to notoriety
				{
					trap_LAN_MarkServerVisible(ui_netSource.integer, i, qfalse);
					continue;
				}
				#endif
			}

			if (ui_browserShowEmpty.integer == 0)
			{
				if (clients == 0)
				{
					trap_LAN_MarkServerVisible(ui_netSource.integer, i, qfalse);
					continue;
				}
			}

			if (ui_browserShowFull.integer == 0)
			{
				s = atoi(Info_ValueForKey(info, "sv_maxclients"));

				if (clients == s)
				{
					trap_LAN_MarkServerVisible(ui_netSource.integer, i, qfalse);
					continue;
				}
			}

			if (uiInfo.joinGameTypes[ui_joinGameType.integer].gtEnum != -1)
			{
				s = atoi(Info_ValueForKey(info, "gametype"));

				if (s != uiInfo.joinGameTypes[ui_joinGameType.integer].gtEnum)
				{
					trap_LAN_MarkServerVisible(ui_netSource.integer, i, qfalse);
					continue;
				}
			}

			// check game name

			// if (Q_stricmp(Info_ValueForKey(info, "game"), serverFilters[ui_serverFilterType.integer].basedir) != 0)

			if (Q_stricmp(Info_ValueForKey(info, "game"), GAME_DIR) )
			{
				trap_LAN_MarkServerVisible(ui_netSource.integer, i, qfalse);
				continue;
			}

			// check game version

			if (Q_stricmp(Info_ValueForKey(info, "modversion"), GAME_VERSION))
			{
#ifdef USE_AUTH
				if(cl_auth_engine.integer == 0)
					continue;
#endif

				trap_LAN_MarkServerVisible(ui_netSource.integer, i, qfalse);
				continue;
			}

			// make sure we never add a favorite server twice
			if (ui_netSource.integer == AS_FAVORITES)
			{
				UI_RemoveServerFromDisplayList(i);
			}


			// insert the server into the list
			UI_BinaryServerInsertion(i);


			// done with this server
			if (ping > 0)
			{
				trap_LAN_MarkServerVisible(ui_netSource.integer, i, qfalse);
			}
		}
	}

	uiInfo.serverStatus.refreshtime = uiInfo.uiDC.realTime;

	// if there were no servers visible for ping updates
	if (!visible)
	{
//		UI_StopServerRefresh();
//		uiInfo.serverStatus.nextDisplayRefresh = 0;
	}
}

typedef struct
{
	char  *name, *altName;
} serverStatusCvar_t;

serverStatusCvar_t	serverStatusCvars[] = {
	{ "sv_hostname", "Name" 	},
	{ "Address",	 "" 	},
	{ "gamename",	 "Game name"},
	{ "g_gametype",  "Game type"},
	{ "mapname",	 "Map"		},
	{ "version",	 "" 	},
	{ "protocol",	 "" 	},
	{ "timelimit",	 "" 	},
	{ "fraglimit",	 "" 	},
	{ NULL, 	 NULL		}
};

/*
==================
UI_SortServerStatusInfo
==================
*/
static void UI_SortServerStatusInfo( serverStatusInfo_t *info )
{
	int   i, j, index;
	char  *tmp1, *tmp2;

	// FIXME: if "gamename" == "baseq3" or "missionpack" then
	// replace the gametype number by FFA, CTF etc.
	//
	index = 0;

	for (i = 0; serverStatusCvars[i].name; i++)
	{
		for (j = 0; j < info->numLines; j++)
		{
			if (!info->lines[j][1] || info->lines[j][1][0])
			{
				continue;
			}

			if (!Q_stricmp(serverStatusCvars[i].name, info->lines[j][0]))
			{
				// swap lines
				tmp1			  = info->lines[index][0];
				tmp2			  = info->lines[index][3];
				info->lines[index][0] = info->lines[j][0];
				info->lines[index][3] = info->lines[j][3];
				info->lines[j][0]	  = tmp1;
				info->lines[j][3]	  = tmp2;

				//
				if (strlen(serverStatusCvars[i].altName))
				{
					info->lines[index][0] = serverStatusCvars[i].altName;
				}
				index++;
			}
		}
	}
}

/*
==================
UI_GetServerStatusInfo
==================
*/
static int UI_GetServerStatusInfo( const char *serverAddress, serverStatusInfo_t *info )
{
	char  *p, *score, *ping, *name;
	int   i, len;

	if (!info)
	{
		trap_LAN_ServerStatus( serverAddress, NULL, 0);
		return qfalse;
	}
	memset(info, 0, sizeof(*info));

	if (trap_LAN_ServerStatus( serverAddress, info->text, sizeof(info->text)))
	{
		Q_strncpyz(info->address, serverAddress, sizeof(info->address));
		p				   = info->text;
		info->numLines			   = 0;
		info->lines[info->numLines][0] = "Address";
		info->lines[info->numLines][1] = "";
		info->lines[info->numLines][2] = "";
		info->lines[info->numLines][3] = info->address;
		info->numLines++;

		// get the cvars
		while (p && *p)
		{
			p = strchr(p, '\\');

			if (!p)
			{
				break;
			}
			*p++ = '\0';

			if (*p == '\\')
			{
				break;
			}
			info->lines[info->numLines][0] = p;
			info->lines[info->numLines][1] = "";
			info->lines[info->numLines][2] = "";
			p				   = strchr(p, '\\');

			if (!p)
			{
				break;
			}
			*p++				   = '\0';
			info->lines[info->numLines][3] = p;

			info->numLines++;

			if (info->numLines >= MAX_SERVERSTATUS_LINES)
			{
				break;
			}
		}

		// get the player list
		if (info->numLines < MAX_SERVERSTATUS_LINES - 3)
		{
			// empty line
			info->lines[info->numLines][0] = "";
			info->lines[info->numLines][1] = "";
			info->lines[info->numLines][2] = "";
			info->lines[info->numLines][3] = "";
			info->numLines++;
			// header
			info->lines[info->numLines][0] = "num";
			info->lines[info->numLines][1] = "score";
			info->lines[info->numLines][2] = "ping";
			info->lines[info->numLines][3] = "name";
			info->numLines++;
			// parse players
			i	= 0;
			len = 0;

			while (p && *p)
			{
				if (*p == '\\')
				{
					*p++ = '\0';
				}

				if (!p)
				{
					break;
				}
				score = p;
				p	  = strchr(p, ' ');

				if (!p)
				{
					break;
				}
				*p++ = '\0';
				ping = p;
				p	 = strchr(p, ' ');

				if (!p)
				{
					break;
				}
				*p++				   = '\0';
				name				   = p;
				Com_sprintf(&info->pings[len], sizeof(info->pings) - len, "%d", i);
				info->lines[info->numLines][0] = &info->pings[len];
				len 			  += strlen(&info->pings[len]) + 1;
				info->lines[info->numLines][1] = score;
				info->lines[info->numLines][2] = ping;
				info->lines[info->numLines][3] = name;
				info->numLines++;

				if (info->numLines >= MAX_SERVERSTATUS_LINES)
				{
					break;
				}
				p = strchr(p, '\\');

				if (!p)
				{
					break;
				}
				*p++ = '\0';
				//
				i++;
			}
		}
		UI_SortServerStatusInfo( info );
		return qtrue;
	}
	return qfalse;
}

/*
==================
stristr
==================
*/
static char *stristr(char *str, char *charset)
{
	int  i;

	while(*str)
	{
		for (i = 0; charset[i] && str[i]; i++)
		{
			if (toupper(charset[i]) != toupper(str[i]))
			{
				break;
			}
		}

		if (!charset[i])
		{
			return str;
		}
		str++;
	}
	return NULL;
}

/*
==================
UI_BuildFindPlayerList
==================
*/
static void UI_BuildFindPlayerList(qboolean force)
{
	static int		numFound, numTimeOuts;
	int 		i, j, resend;
	serverStatusInfo_t	info;
	char			name[MAX_NAME_LENGTH + 2];
	char			infoString[MAX_STRING_CHARS];

	if (!force)
	{
		if (!uiInfo.nextFindPlayerRefresh || (uiInfo.nextFindPlayerRefresh > uiInfo.uiDC.realTime))
		{
			return;
		}
	}
	else
	{
		memset(&uiInfo.pendingServerStatus, 0, sizeof(uiInfo.pendingServerStatus));
		uiInfo.numFoundPlayerServers	= 0;
		uiInfo.currentFoundPlayerServer = 0;
		trap_Cvar_VariableStringBuffer( "ui_findPlayer", uiInfo.findPlayerName, sizeof(uiInfo.findPlayerName));
		Q_CleanStr(uiInfo.findPlayerName);

		// should have a string of some length
		if (!strlen(uiInfo.findPlayerName))
		{
			uiInfo.nextFindPlayerRefresh = 0;
			return;
		}
		// set resend time
		resend = ui_serverStatusTimeOut.integer / 2 - 10;

		if (resend < 50)
		{
			resend = 50;
		}
		trap_Cvar_Set("cl_serverStatusResendTime", va("%d", resend));
		// reset all server status requests
		trap_LAN_ServerStatus( NULL, NULL, 0);
		//
		uiInfo.numFoundPlayerServers = 1;
		Com_sprintf(uiInfo.foundPlayerServerNames[uiInfo.numFoundPlayerServers - 1],
				sizeof(uiInfo.foundPlayerServerNames[uiInfo.numFoundPlayerServers - 1]),
				"searching %d...", uiInfo.pendingServerStatus.num);
		numFound = 0;
		numTimeOuts++;
	}

	for (i = 0; i < MAX_SERVERSTATUSREQUESTS; i++)
	{
		// if this pending server is valid
		if (uiInfo.pendingServerStatus.server[i].valid)
		{
			// try to get the server status for this server
			if (UI_GetServerStatusInfo( uiInfo.pendingServerStatus.server[i].adrstr, &info ))
			{
				//
				numFound++;

				// parse through the server status lines
				for (j = 0; j < info.numLines; j++)
				{
					// should have ping info
					if (!info.lines[j][2] || !info.lines[j][2][0])
					{
						continue;
					}
					// clean string first
					Q_strncpyz(name, info.lines[j][3], sizeof(name));
					Q_CleanStr(name);

					// if the player name is a substring
					if (stristr(name, uiInfo.findPlayerName))
					{
						// add to found server list if we have space (always leave space for a line with the number found)
						if (uiInfo.numFoundPlayerServers < MAX_FOUNDPLAYER_SERVERS - 1)
						{
							//
							Q_strncpyz(uiInfo.foundPlayerServerAddresses[uiInfo.numFoundPlayerServers - 1],
								   uiInfo.pendingServerStatus.server[i].adrstr,
								   sizeof(uiInfo.foundPlayerServerAddresses[0]));
							Q_strncpyz(uiInfo.foundPlayerServerNames[uiInfo.numFoundPlayerServers - 1],
								   uiInfo.pendingServerStatus.server[i].name,
								   sizeof(uiInfo.foundPlayerServerNames[0]));
							uiInfo.numFoundPlayerServers++;
						}
						else
						{
							// can't add any more so we're done
							uiInfo.pendingServerStatus.num = uiInfo.serverStatus.numDisplayServers;
						}
					}
				}
				Com_sprintf(uiInfo.foundPlayerServerNames[uiInfo.numFoundPlayerServers - 1],
						sizeof(uiInfo.foundPlayerServerNames[uiInfo.numFoundPlayerServers - 1]),
						"searching %d/%d...", uiInfo.pendingServerStatus.num, numFound);
				// retrieved the server status so reuse this spot
				uiInfo.pendingServerStatus.server[i].valid = qfalse;
			}
		}

		// if empty pending slot or timed out
		if (!uiInfo.pendingServerStatus.server[i].valid ||
			(uiInfo.pendingServerStatus.server[i].startTime < uiInfo.uiDC.realTime - ui_serverStatusTimeOut.integer))
		{
			if (uiInfo.pendingServerStatus.server[i].valid)
			{
				numTimeOuts++;
			}
			// reset server status request for this address
			UI_GetServerStatusInfo( uiInfo.pendingServerStatus.server[i].adrstr, NULL );
			// reuse pending slot
			uiInfo.pendingServerStatus.server[i].valid = qfalse;

			// if we didn't try to get the status of all servers in the main browser yet
			if (uiInfo.pendingServerStatus.num < uiInfo.serverStatus.numDisplayServers)
			{
				uiInfo.pendingServerStatus.server[i].startTime = uiInfo.uiDC.realTime;
				trap_LAN_GetServerAddressString(ui_netSource.integer, uiInfo.serverStatus.displayServers[uiInfo.pendingServerStatus.num],
								uiInfo.pendingServerStatus.server[i].adrstr, sizeof(uiInfo.pendingServerStatus.server[i].adrstr));
				trap_LAN_GetServerInfo(ui_netSource.integer, uiInfo.serverStatus.displayServers[uiInfo.pendingServerStatus.num], infoString, sizeof(infoString));
				Q_strncpyz(uiInfo.pendingServerStatus.server[i].name, Info_ValueForKey(infoString, "hostname"), sizeof(uiInfo.pendingServerStatus.server[0].name));
				uiInfo.pendingServerStatus.server[i].valid = qtrue;
				uiInfo.pendingServerStatus.num++;
				Com_sprintf(uiInfo.foundPlayerServerNames[uiInfo.numFoundPlayerServers - 1],
						sizeof(uiInfo.foundPlayerServerNames[uiInfo.numFoundPlayerServers - 1]),
						"searching %d/%d...", uiInfo.pendingServerStatus.num, numFound);
			}
		}
	}

	for (i = 0; i < MAX_SERVERSTATUSREQUESTS; i++)
	{
		if (uiInfo.pendingServerStatus.server[i].valid)
		{
			break;
		}
	}

	// if still trying to retrieve server status info
	if (i < MAX_SERVERSTATUSREQUESTS)
	{
		uiInfo.nextFindPlayerRefresh = uiInfo.uiDC.realTime + 25;
	}
	else
	{
		// add a line that shows the number of servers found
		if (!uiInfo.numFoundPlayerServers)
		{
			Com_sprintf(uiInfo.foundPlayerServerNames[uiInfo.numFoundPlayerServers - 1], sizeof(uiInfo.foundPlayerServerAddresses[0]), "no servers found");
		}
		else
		{
			Com_sprintf(uiInfo.foundPlayerServerNames[uiInfo.numFoundPlayerServers - 1], sizeof(uiInfo.foundPlayerServerAddresses[0]),
					"%d server%s found with player %s", uiInfo.numFoundPlayerServers - 1,
					uiInfo.numFoundPlayerServers == 2 ? "" : "s", uiInfo.findPlayerName);
		}
		uiInfo.nextFindPlayerRefresh = 0;
		// show the server status info for the selected server
		UI_FeederSelection(FEEDER_FINDPLAYER, uiInfo.currentFoundPlayerServer);
	}
}

/*
==================
UI_BuildServerStatus
==================
*/
static void UI_BuildServerStatus(qboolean force)
{
	if (uiInfo.nextFindPlayerRefresh)
	{
		return;
	}

	if (!force)
	{
		if (!uiInfo.nextServerStatusRefresh || (uiInfo.nextServerStatusRefresh > uiInfo.uiDC.realTime))
		{
			return;
		}
	}
	else
	{
		Menu_SetFeederSelection(NULL, FEEDER_SERVERSTATUS, 0, NULL);
		uiInfo.serverStatusInfo.numLines = 0;
		// reset all server status requests
		trap_LAN_ServerStatus( NULL, NULL, 0);
	}

	if ((uiInfo.serverStatus.currentServer < 0) || (uiInfo.serverStatus.currentServer > uiInfo.serverStatus.numDisplayServers) || (uiInfo.serverStatus.numDisplayServers == 0))
	{
		return;
	}

	if (UI_GetServerStatusInfo( uiInfo.serverStatusAddress, &uiInfo.serverStatusInfo ))
	{
		uiInfo.nextServerStatusRefresh = 0;
		UI_GetServerStatusInfo( uiInfo.serverStatusAddress, NULL );
	}
	else
	{
		uiInfo.nextServerStatusRefresh = uiInfo.uiDC.realTime + 500;
	}
}

/*
==================
UI_FeederCount
==================
*/
static int UI_FeederCount(float feederID)
{
	if (feederID == FEEDER_HEADS)
	{
		return uiInfo.characterCount;
	}
	else if (feederID == FEEDER_Q3HEADS)
	{
		return uiInfo.q3HeadCount[TEAM_FREE];
		//Iain:
	}
	else if (feederID == FEEDER_Q3HEADS_RED)
	{
		return uiInfo.q3HeadCount[TEAM_RED];
	}
	else if (feederID == FEEDER_Q3HEADS_BLUE)
	{
		return uiInfo.q3HeadCount[TEAM_BLUE];
	}
	else if (feederID == FEEDER_CINEMATICS)
	{
		return uiInfo.movieCount;
	}
	else if (feederID == FEEDER_MAPS || feederID == FEEDER_ALLMAPS)
	{
		return UI_MapCountByGameType(feederID == FEEDER_MAPS ? qtrue : qfalse);
	}
	else if (feederID == FEEDER_SERVERS)
	{
		return uiInfo.serverStatus.numDisplayServers;
	}
	else if (feederID == FEEDER_SERVERSTATUS)
	{
		return uiInfo.serverStatusInfo.numLines;
	}
	else if (feederID == FEEDER_FINDPLAYER)
	{
		return uiInfo.numFoundPlayerServers;
	}
	else if (feederID == FEEDER_PLAYER_LIST)
	{
		if (uiInfo.uiDC.realTime > uiInfo.playerRefresh)
		{
			uiInfo.playerRefresh = uiInfo.uiDC.realTime + 3000;
			UI_BuildPlayerList();
		}
		return uiInfo.playerCount;
	}
	else if (feederID == FEEDER_STATS_PLAYER_LIST)
	{
		if (uiInfo.uiDC.realTime > uiInfo.playerRefresh)
		{
			uiInfo.playerRefresh = uiInfo.uiDC.realTime + 3000;
			UI_BuildPlayerList();
		}
		return uiInfo.playerCount;
	}
	else if (feederID == FEEDER_TEAM_LIST)
	{
		if (uiInfo.uiDC.realTime > uiInfo.playerRefresh)
		{
			uiInfo.playerRefresh = uiInfo.uiDC.realTime + 3000;
			UI_BuildPlayerList();
		}
		return uiInfo.myTeamCount;
	}
	else if (feederID == FEEDER_STATS_WEAPONS_LIST)
	{
		return MAX_STATS_CATAGORYS;
	}
	else if (feederID == FEEDER_MODS)
	{
		return uiInfo.modCount;
	}
	else if (feederID == FEEDER_STATS_TEXT_LIST)
	{
		return MAX_STATS_TEXT;
	}
	else if (feederID == FEEDER_DEMOS)
	{
		return uiInfo.demoCount;
	}
	return 0;
}

/**
 * UI_SelectedMap
 */
static const char *UI_SelectedMap(int index, int *actual)
{
	int  i, c;

	c	= 0;
	*actual = 0;

	for (i = 0; i < uiInfo.mapCount; i++)
	{
		if (uiInfo.mapList[i].active)
		{
			if (c == index)
			{
				*actual = i;
				return uiInfo.mapList[i].mapName;
			}
			else
			{
				c++;
			}
		}
	}
	return "";
}

/**
 * UI_GetIndexFromSelection
 */
static int UI_GetIndexFromSelection(int actual)
{
	int  i, c;

	c = 0;

	for (i = 0; i < uiInfo.mapCount; i++)
	{
		if (uiInfo.mapList[i].active)
		{
			if (i == actual)
			{
				return c;
			}
			c++;
		}
	}
	return 0;
}

/**
 * UI_UpdatePendingPings
 */
static void UI_UpdatePendingPings(void)
{
	trap_LAN_ResetPings(ui_netSource.integer);
	uiInfo.serverStatus.refreshActive = qtrue;
	uiInfo.serverStatus.refreshtime   = uiInfo.uiDC.realTime + 1000;
}

/**
 * UI_FeederItemText
 */
static const char *UI_FeederItemText(float feederID, int index, int column, qhandle_t *handle)
{
	static char  info[MAX_STRING_CHARS];
	static char  hostname[1024];
	static char  clientBuff[32];
	static int	 lastColumn = -1;
	static int	 lastTime	= 0;

	*handle = -1;

	if (feederID == FEEDER_HEADS)
	{
		if ((index >= 0) && (index < uiInfo.characterCount))
		{
			return uiInfo.characterList[index].name;
		}
	}
	else if (feederID == FEEDER_Q3HEADS)
	{
		if ((index >= 0) && (index < uiInfo.q3HeadCount[TEAM_FREE]))
		{
			return uiInfo.q3HeadNames[TEAM_FREE][index];
		}
		//Iain:
	}
	else if (feederID == FEEDER_Q3HEADS_RED)
	{
		if ((index >= 0) && (index < uiInfo.q3HeadCount[TEAM_RED]))
		{
			return uiInfo.q3HeadNames[TEAM_RED][index];
		}
	}
	else if (feederID == FEEDER_Q3HEADS_BLUE)
	{
		if ((index >= 0) && (index < uiInfo.q3HeadCount[TEAM_BLUE]))
		{
			return uiInfo.q3HeadNames[TEAM_BLUE][index];
		}
	}
	else if (feederID == FEEDER_MAPS || feederID == FEEDER_ALLMAPS)
	{
		int  actual;
		return UI_SelectedMap(index, &actual);
	}
	else if (feederID == FEEDER_SERVERS)
	{
		if ((index >= 0) && (index < uiInfo.serverStatus.numDisplayServers))
		{
			int  ping, game;

			if ((lastColumn != column) || (lastTime > uiInfo.uiDC.realTime + 5000))
			{
				trap_LAN_GetServerInfo(ui_netSource.integer, uiInfo.serverStatus.displayServers[index], info, MAX_STRING_CHARS);
				lastColumn = column;
				lastTime   = uiInfo.uiDC.realTime;
			}
			ping = atoi(Info_ValueForKey(info, "ping"));

			if (ping == -1)
			{
				// if we ever see a ping that is out of date, do a server refresh
				// UI_UpdatePendingPings();
			}

			switch (column)
			{
				case SORT_HOST:

					if (ping <= 0)
					{
						return Info_ValueForKey(info, "addr");
					}
					else
					{
						if (ui_netSource.integer == AS_LOCAL)
						{
							Com_sprintf( hostname, sizeof(hostname), "%s [%s]",
									 Info_ValueForKey(info, "hostname"),
									 netnames[atoi(Info_ValueForKey(info, "nettype"))] );
							return hostname;
						}
						else
						{
							return Info_ValueForKey(info, "hostname");
						}
					}

				case SORT_MAP:
					return Info_ValueForKey(info, "mapname");

				case SORT_CLIENTS:
					Com_sprintf( clientBuff, sizeof(clientBuff), "%s (%s)", Info_ValueForKey(info, "clients"), Info_ValueForKey(info, "sv_maxclients"));
					return clientBuff;

				case SORT_GAME:
					game = atoi(Info_ValueForKey(info, "gametype"));

					if ((game >= 0) && (game < numTeamArenaGameTypes))
					{
						return teamArenaGameTypes[game];
					}
					else
					{
						return "Unknown";
					}

				case SORT_PING:

					if (ping <= 0)
					{
						return "...";
					}
					else
					{
						return Info_ValueForKey(info, "ping");
					}
			}
		}
	}
	else if (feederID == FEEDER_SERVERSTATUS)
	{
		if ((index >= 0) && (index < uiInfo.serverStatusInfo.numLines))
		{
			if ((column >= 0) && (column < 4))
			{
				return uiInfo.serverStatusInfo.lines[index][column];
			}
		}
	}
	else if (feederID == FEEDER_FINDPLAYER)
	{
		if ((index >= 0) && (index < uiInfo.numFoundPlayerServers))
		{
			//return uiInfo.foundPlayerServerAddresses[index];
			return uiInfo.foundPlayerServerNames[index];
		}
	}
	else if (feederID == FEEDER_PLAYER_LIST)
	{
		if ((index >= 0) && (index < uiInfo.playerCount))
		{
			return uiInfo.playerNames[index];
		}
	}
	else if (feederID == FEEDER_STATS_PLAYER_LIST)
	{
		if ((index >= 0) && (index < uiInfo.playerCount))
		{
			return uiInfo.playerNames[index];
		}
	}
	else if (feederID == FEEDER_STATS_WEAPONS_LIST)
	{
		if ((index >= 0) && (index < MAX_STATS_CATAGORYS))
		{
			return va("Stats for: %s", STATS_CATAGORYS[index]);
		}
	}
	else if (feederID == FEEDER_STATS_TEXT_LIST)
	{
		if ((index >= 0) && (index < MAX_STATS_TEXT))
		{
			return STATS_TEXTLINES[index];
		}
	}
	else if (feederID == FEEDER_TEAM_LIST)
	{
		if ((index >= 0) && (index < uiInfo.myTeamCount))
		{
			return uiInfo.teamNames[index];
		}
	}
	else if (feederID == FEEDER_MODS)
	{
		if ((index >= 0) && (index < uiInfo.modCount))
		{
			if (uiInfo.modList[index].modDescr && *uiInfo.modList[index].modDescr)
			{
				return uiInfo.modList[index].modDescr;
			}
			else
			{
				return uiInfo.modList[index].modName;
			}
		}
	}
	else if (feederID == FEEDER_CINEMATICS)
	{
		if ((index >= 0) && (index < uiInfo.movieCount))
		{
			return uiInfo.movieList[index];
		}
	}
	else if (feederID == FEEDER_DEMOS)
	{
		if ((index >= 0) && (index < uiInfo.demoCount))
		{
			return uiInfo.demoList[index];
		}
	}

	return "";
}

/**
 * $(function)
 */
static qhandle_t UI_FeederItemImage(float feederID, int index)
{
	if (feederID == FEEDER_HEADS)
	{
		if ((index >= 0) && (index < uiInfo.characterCount))
		{
			if (uiInfo.characterList[index].headImage == -1)
			{
				uiInfo.characterList[index].headImage = trap_R_RegisterShaderNoMip(uiInfo.characterList[index].imageName);
			}
			return uiInfo.characterList[index].headImage;
		}
	}
	else if (feederID == FEEDER_Q3HEADS)
	{
		if ((index >= 0) && (index < uiInfo.q3HeadCount[TEAM_FREE]))
		{
			return uiInfo.q3HeadIcons[TEAM_FREE][index];
		}
		//Iain:
	}
	else if (feederID == FEEDER_Q3HEADS_RED)
	{
		if ((index >= 0) && (index < uiInfo.q3HeadCount[TEAM_RED]))
		{
			return uiInfo.q3HeadIcons[TEAM_RED][index];
		}
	}
	else if (feederID == FEEDER_Q3HEADS_BLUE)
	{
		if ((index >= 0) && (index < uiInfo.q3HeadCount[TEAM_BLUE]))
		{
			return uiInfo.q3HeadIcons[TEAM_BLUE][index];
		}
	}
	else if (feederID == FEEDER_ALLMAPS || feederID == FEEDER_MAPS)
	{
		int  actual;
		UI_SelectedMap(index, &actual);
		index = actual;

		if ((index >= 0) && (index < uiInfo.mapCount))
		{
			if (uiInfo.mapList[index].levelShot == -1)
			{
				uiInfo.mapList[index].levelShot = trap_R_RegisterShaderNoMip(uiInfo.mapList[index].imageName);
			}
			return uiInfo.mapList[index].levelShot;
		}
	}
	return 0;
}

/**
 * UI_FeederSelection
 */
static void UI_FeederSelection(float feederID, int index)
{
	static char  info[MAX_STRING_CHARS];

	if (feederID == FEEDER_HEADS)
	{
		if ((index >= 0) && (index < uiInfo.characterCount))
		{
//	trap_Cvar_Set( "team_model", uiInfo.characterList[index].female ? "janet" : "james");
			//	 trap_Cvar_Set( "model", uiInfo.characterList[index].female ? "da" : "da");
			// trap_Cvar_Set( "team_headmodel", va("*%s", uiInfo.characterList[index].name));
			updateModel = qtrue;
		}
	}
	else if (feederID == FEEDER_Q3HEADS)
	{
		if ((index >= 0) && (index < uiInfo.q3HeadCount[TEAM_FREE]))
		{
//		trap_Cvar_Set("ui_currentmodel", uiInfo.q3HeadNames[TEAM_FREE][index]);
//		trap_Cvar_Set( "model", uiInfo.q3HeadNames[TEAM_FREE][index]);
//		trap_Cvar_Set( "headmodel", uiInfo.q3HeadNames[TEAM_FREE][index]);
			updateModel = qtrue;
		}
	}
	else if (feederID == FEEDER_Q3HEADS_RED)
	{
		//Iain addition gametype check
		if ((index >= 0) && (index < uiInfo.q3HeadCount[TEAM_RED]))
		{
			if(trap_Cvar_VariableValue("g_gametype") >= 3)
			{
//			trap_Cvar_Set("ui_currentmodel", uiInfo.q3HeadNames[TEAM_RED][index]);
			}
//		trap_Cvar_Set( "headmodel", uiInfo.q3HeadNames[TEAM_RED][index]);
//		trap_Cvar_Set( "team_model_red", uiInfo.q3HeadNames[TEAM_RED][index]);
			updateModel = qtrue;
		}
	}
	else if (feederID == FEEDER_Q3HEADS_BLUE)
	{
		if ((index >= 0) && (index < uiInfo.q3HeadCount[TEAM_BLUE]))
		{
			if(trap_Cvar_VariableValue("g_gametype") >= 3)
			{
//			trap_Cvar_Set("ui_currentmodel", uiInfo.q3HeadNames[TEAM_BLUE][index]);
			}
//		trap_Cvar_Set( "headmodel", uiInfo.q3HeadNames[TEAM_BLUE][index]);
//		trap_Cvar_Set( "team_model_blue", uiInfo.q3HeadNames[TEAM_BLUE][index]);
			updateModel = qtrue;
		}
	}
	else if (feederID == FEEDER_MAPS || feederID == FEEDER_ALLMAPS)
	{
		int  actual, map;
		map = (feederID == FEEDER_ALLMAPS) ? ui_currentNetMap.integer : ui_currentMap.integer;

		if (uiInfo.mapList[map].cinematic >= 0)
		{
			trap_CIN_StopCinematic(uiInfo.mapList[map].cinematic);
			uiInfo.mapList[map].cinematic = -1;
		}
		UI_SelectedMap(index, &actual);
		trap_Cvar_Set("ui_mapIndex", va("%d", index));
		ui_mapIndex.integer = index;

		if (feederID == FEEDER_MAPS)
		{
			ui_currentMap.integer				= actual;
			trap_Cvar_Set("ui_currentMap", va("%d", actual));
			uiInfo.mapList[ui_currentMap.integer].cinematic = trap_CIN_PlayCinematic(va("%s.roq", uiInfo.mapList[ui_currentMap.integer].mapLoadName), 0, 0, 0, 0, (CIN_loop | CIN_silent));
			UI_LoadBestScores(uiInfo.mapList[ui_currentMap.integer].mapLoadName, uiInfo.gameTypes[ui_gameType.integer].gtEnum);
			trap_Cvar_Set("ui_opponentModel", uiInfo.mapList[ui_currentMap.integer].opponentName);
		}
		else
		{
			ui_currentNetMap.integer			   = actual;
			trap_Cvar_Set("ui_currentNetMap", va("%d", actual));
			uiInfo.mapList[ui_currentNetMap.integer].cinematic = trap_CIN_PlayCinematic(va("%s.roq", uiInfo.mapList[ui_currentNetMap.integer].mapLoadName), 0, 0, 0, 0, (CIN_loop | CIN_silent));
		}
	}
	else if (feederID == FEEDER_SERVERS)
	{
		const char	*mapName = NULL;
		uiInfo.serverStatus.currentServer = index;
		trap_LAN_GetServerInfo(ui_netSource.integer, uiInfo.serverStatus.displayServers[index], info, MAX_STRING_CHARS);

//		uiInfo.serverStatus.currentServerPreview = trap_R_RegisterShaderNoMip(va("levelshots/%s_small", Info_ValueForKey(info, "mapname")));
		if (uiInfo.serverStatus.currentServerCinematic >= 0)
		{
			trap_CIN_StopCinematic(uiInfo.serverStatus.currentServerCinematic);
			uiInfo.serverStatus.currentServerCinematic = -1;
		}
		mapName = Info_ValueForKey(info, "mapname");

		if (mapName && *mapName)
		{
			uiInfo.serverStatus.currentServerCinematic = trap_CIN_PlayCinematic(va("%s.roq", mapName), 0, 0, 0, 0, (CIN_loop | CIN_silent));
		}
	}
	else if (feederID == FEEDER_SERVERSTATUS)
	{
		//
	}
	else if (feederID == FEEDER_FINDPLAYER)
	{
		uiInfo.currentFoundPlayerServer = index;

		//
		if (index < uiInfo.numFoundPlayerServers - 1)
		{
			// build a new server status for this server
			Q_strncpyz(uiInfo.serverStatusAddress, uiInfo.foundPlayerServerAddresses[uiInfo.currentFoundPlayerServer], sizeof(uiInfo.serverStatusAddress));
			Menu_SetFeederSelection(NULL, FEEDER_SERVERSTATUS, 0, NULL);
			UI_BuildServerStatus(qtrue);
		}
	}
	else if (feederID == FEEDER_PLAYER_LIST)
	{
		uiInfo.playerIndex = index;
	}
	else if (feederID == FEEDER_STATS_PLAYER_LIST)
	{
		uiInfo.statsplayerIndex = index;

		if (index > uiInfo.playerCount - 1)
		{
			Menu_SetFeederSelection(NULL, FEEDER_STATS_PLAYER_LIST, uiInfo.playerCount - 1, NULL);
		}
		else
		{
			getStats();
		}
	}
	else if (feederID == FEEDER_STATS_WEAPONS_LIST)
	{
		uiInfo.statsweaponsIndex = index;

		if (index > MAX_STATS_CATAGORYS - 1)
		{
			Menu_SetFeederSelection(NULL, FEEDER_STATS_WEAPONS_LIST, MAX_STATS_CATAGORYS - 1, NULL);
		}
		else
		{
			getStats();
		}
	}
	else if (feederID == FEEDER_TEAM_LIST)
	{
		uiInfo.teamIndex = index;
	}
	else if (feederID == FEEDER_MODS)
	{
		uiInfo.modIndex = index;
	}
	else if (feederID == FEEDER_CINEMATICS)
	{
		uiInfo.movieIndex = index;

		if (uiInfo.previewMovie >= 0)
		{
			trap_CIN_StopCinematic(uiInfo.previewMovie);
		}
		uiInfo.previewMovie = -1;
	}
	else if (feederID == FEEDER_DEMOS)
	{
		uiInfo.demoIndex = index;
	}
}

/**
 * $(function)
 */
static qboolean Team_Parse(char * *p)
{
	char		*token;
	const char	*tempStr;
	int 	i;

	token = COM_ParseExt(p, qtrue);

	if (token[0] != '{')
	{
		return qfalse;
	}

	while (1)
	{
		token = COM_ParseExt(p, qtrue);

		if (Q_stricmp(token, "}") == 0)
		{
			return qtrue;
		}

		if (!token || (token[0] == 0))
		{
			return qfalse;
		}

		if (token[0] == '{')
		{
			// seven tokens per line, team name and icon, and 5 team member names
			if (!String_Parse(p, &uiInfo.teamList[uiInfo.teamCount].teamName) || !String_Parse(p, &tempStr))
			{
				return qfalse;
			}

			uiInfo.teamList[uiInfo.teamCount].imageName  = tempStr;
			uiInfo.teamList[uiInfo.teamCount].teamIcon	 = trap_R_RegisterShaderNoMip(uiInfo.teamList[uiInfo.teamCount].imageName);
			uiInfo.teamList[uiInfo.teamCount].teamIcon_Metal = trap_R_RegisterShaderNoMip(va("%s_metal", uiInfo.teamList[uiInfo.teamCount].imageName));
			uiInfo.teamList[uiInfo.teamCount].teamIcon_Name  = trap_R_RegisterShaderNoMip(va("%s_name", uiInfo.teamList[uiInfo.teamCount].imageName));

			uiInfo.teamList[uiInfo.teamCount].cinematic  = -1;

			for (i = 0; i < TEAM_MEMBERS; i++)
			{
				uiInfo.teamList[uiInfo.teamCount].teamMembers[i] = NULL;

				if (!String_Parse(p, &uiInfo.teamList[uiInfo.teamCount].teamMembers[i]))
				{
					return qfalse;
				}
			}

			Com_Printf("Loaded team %s with team icon %s.\n", uiInfo.teamList[uiInfo.teamCount].teamName, tempStr);

			if (uiInfo.teamCount < MAX_TEAMS)
			{
				uiInfo.teamCount++;
			}
			else
			{
				Com_Printf("Too many teams, last team replaced!\n");
			}
			token = COM_ParseExt(p, qtrue);

			if (token[0] != '}')
			{
				return qfalse;
			}
		}
	}

	return qfalse;
}

/**
 * $(function)
 */
static qboolean Character_Parse(char * *p)
{
	char		*token;
	const char	*tempStr;

	token = COM_ParseExt(p, qtrue);

	if (token[0] != '{')
	{
		return qfalse;
	}

	while (1)
	{
		token = COM_ParseExt(p, qtrue);

		if (Q_stricmp(token, "}") == 0)
		{
			return qtrue;
		}

		if (!token || (token[0] == 0))
		{
			return qfalse;
		}

		if (token[0] == '{')
		{
			// two tokens per line, character name and sex
			if (!String_Parse(p, &uiInfo.characterList[uiInfo.characterCount].name) || !String_Parse(p, &tempStr))
			{
				return qfalse;
			}

			uiInfo.characterList[uiInfo.characterCount].headImage = -1;
			uiInfo.characterList[uiInfo.characterCount].imageName = String_Alloc(va("models/players/heads/%s/icon_default.tga", uiInfo.characterList[uiInfo.characterCount].name));

			if (tempStr && ((tempStr[0] == 'f') || (tempStr[0] == 'F')))
			{
				uiInfo.characterList[uiInfo.characterCount].female = qtrue;
			}
			else
			{
				uiInfo.characterList[uiInfo.characterCount].female = qfalse;
			}

			Com_Printf("Loaded %s character %s.\n", tempStr, uiInfo.characterList[uiInfo.characterCount].name);

			if (uiInfo.characterCount < MAX_HEADS)
			{
				uiInfo.characterCount++;
			}
			else
			{
				Com_Printf("Too many characters, last character replaced!\n");
			}

			token = COM_ParseExt(p, qtrue);

			if (token[0] != '}')
			{
				return qfalse;
			}
		}
	}

	return qfalse;
}

/**
 * $(function)
 */
static qboolean Alias_Parse(char * *p)
{
	char  *token;

	token = COM_ParseExt(p, qtrue);

	if (token[0] != '{')
	{
		return qfalse;
	}

	while (1)
	{
		token = COM_ParseExt(p, qtrue);

		if (Q_stricmp(token, "}") == 0)
		{
			return qtrue;
		}

		if (!token || (token[0] == 0))
		{
			return qfalse;
		}

		if (token[0] == '{')
		{
			// three tokens per line, character name, bot alias, and preferred action a - all purpose, d - defense, o - offense
			if (!String_Parse(p, &uiInfo.aliasList[uiInfo.aliasCount].name) || !String_Parse(p, &uiInfo.aliasList[uiInfo.aliasCount].ai) || !String_Parse(p, &uiInfo.aliasList[uiInfo.aliasCount].action))
			{
				return qfalse;
			}

			Com_Printf("Loaded character alias %s using character ai %s.\n", uiInfo.aliasList[uiInfo.aliasCount].name, uiInfo.aliasList[uiInfo.aliasCount].ai);

			if (uiInfo.aliasCount < MAX_ALIASES)
			{
				uiInfo.aliasCount++;
			}
			else
			{
				Com_Printf("Too many aliases, last alias replaced!\n");
			}

			token = COM_ParseExt(p, qtrue);

			if (token[0] != '}')
			{
				return qfalse;
			}
		}
	}

	return qfalse;
}

// mode
// 0 - high level parsing
// 1 - team parsing
// 2 - character parsing
static void UI_ParseTeamInfo(const char *teamFile)
{
	char  *token;
	char  *p;
	char  *buff = NULL;

	buff = GetMenuBuffer(teamFile);

	if (!buff)
	{
		return;
	}

	p = buff;

	while (1)
	{
		token = COM_ParseExt( &p, qtrue );

		if(!token || (token[0] == 0) || (token[0] == '}'))
		{
			break;
		}

		if (Q_stricmp( token, "}" ) == 0)
		{
			break;
		}

		if (Q_stricmp(token, "teams") == 0)
		{
			if (Team_Parse(&p))
			{
				continue;
			}
			else
			{
				break;
			}
		}

		if (Q_stricmp(token, "characters") == 0)
		{
			Character_Parse(&p);
		}

		if (Q_stricmp(token, "aliases") == 0)
		{
			Alias_Parse(&p);
		}
	}
}

/**
 * $(function)
 */
static qboolean GameType_Parse(char * *p, qboolean join)
{
	char  *token;

	token = COM_ParseExt(p, qtrue);

	if (token[0] != '{')
	{
		return qfalse;
	}

	if (join)
	{
		uiInfo.numJoinGameTypes = 0;
	}
	else
	{
		uiInfo.numGameTypes = 0;
	}

	while (1)
	{
		token = COM_ParseExt(p, qtrue);

		if (Q_stricmp(token, "}") == 0)
		{
			return qtrue;
		}

		if (!token || (token[0] == 0))
		{
			return qfalse;
		}

		if (token[0] == '{')
		{
			// two tokens per line, character name and sex
			if (join)
			{
				if (!String_Parse(p, &uiInfo.joinGameTypes[uiInfo.numJoinGameTypes].gameType) || !Int_Parse(p, &uiInfo.joinGameTypes[uiInfo.numJoinGameTypes].gtEnum))
				{
					return qfalse;
				}
			}
			else
			{
				if (!String_Parse(p, &uiInfo.gameTypes[uiInfo.numGameTypes].gameType) || !Int_Parse(p, &uiInfo.gameTypes[uiInfo.numGameTypes].gtEnum))
				{
					return qfalse;
				}
			}

			if (join)
			{
				if (uiInfo.numJoinGameTypes < MAX_GAMETYPES)
				{
					uiInfo.numJoinGameTypes++;
				}
				else
				{
					Com_Printf("Too many net game types, last one replace!\n");
				}
			}
			else
			{
				if (uiInfo.numGameTypes < MAX_GAMETYPES)
				{
					uiInfo.numGameTypes++;
				}
				else
				{
					Com_Printf("Too many game types, last one replace!\n");
				}
			}

			token = COM_ParseExt(p, qtrue);

			if (token[0] != '}')
			{
				return qfalse;
			}
		}
	}
	return qfalse;
}

/**
 * $(function)
 */
static qboolean MapList_Parse(char * *p)
{
	char  *token;

	token = COM_ParseExt(p, qtrue);

	if (token[0] != '{')
	{
		return qfalse;
	}

	uiInfo.mapCount = 0;

	while (1)
	{
		token = COM_ParseExt(p, qtrue);

		if (Q_stricmp(token, "}") == 0)
		{
			return qtrue;
		}

		if (!token || (token[0] == 0))
		{
			return qfalse;
		}

		if (token[0] == '{')
		{
			if (!String_Parse(p, &uiInfo.mapList[uiInfo.mapCount].mapName) || !String_Parse(p, &uiInfo.mapList[uiInfo.mapCount].mapLoadName)
				|| !Int_Parse(p, &uiInfo.mapList[uiInfo.mapCount].teamMembers))
			{
				return qfalse;
			}

			if (!String_Parse(p, &uiInfo.mapList[uiInfo.mapCount].opponentName))
			{
				return qfalse;
			}

			uiInfo.mapList[uiInfo.mapCount].typeBits = 0;

			while (1)
			{
				token = COM_ParseExt(p, qtrue);

				if ((token[0] >= '0') && (token[0] <= '9'))
				{
					uiInfo.mapList[uiInfo.mapCount].typeBits |= (1 << (token[0] - 0x030));

					if (!Int_Parse(p, &uiInfo.mapList[uiInfo.mapCount].timeToBeat[token[0] - 0x30]))
					{
						return qfalse;
					}
				}
				else
				{
					break;
				}
			}

			//mapList[mapCount].imageName = String_Alloc(va("levelshots/%s", mapList[mapCount].mapLoadName));
			//if (uiInfo.mapCount == 0) {
			// only load the first cinematic, selection loads the others
			//	uiInfo.mapList[uiInfo.mapCount].cinematic = trap_CIN_PlayCinematic(va("%s.roq",uiInfo.mapList[uiInfo.mapCount].mapLoadName), qfalse, qfalse, qtrue, 0, 0, 0, 0);
			//}
			uiInfo.mapList[uiInfo.mapCount].cinematic = -1;
			uiInfo.mapList[uiInfo.mapCount].levelShot = trap_R_RegisterShaderNoMip(va("levelshots/%s", uiInfo.mapList[uiInfo.mapCount].mapLoadName)); //_small

			if (uiInfo.mapCount < MAX_MAPS)
			{
				uiInfo.mapCount++;
			}
			else
			{
				Com_Printf("Too many maps, last one replaced!\n");
			}
		}
	}
	return qfalse;
}

/*
=================
PlayerModel_BuildList
=================
*/
static void UI_BuildQ3Model_List( void )
{
	return ; //fIXME
/*	trap_Cvar_VariableStringBuffer ( "team_model_red", compare[TEAM_RED], MAX_QPATH );
	trap_Cvar_VariableStringBuffer ( "team_model_blue", compare[TEAM_BLUE], MAX_QPATH );
	trap_Cvar_VariableStringBuffer ( "model", compare[TEAM_FREE], MAX_QPATH );

	for(C_TEAM=TEAM_FREE; C_TEAM<TEAM_NUM_TEAMS; C_TEAM++)
	{
		uiInfo.q3HeadCount[C_TEAM] = 0;
		while ( bg_teamSkins[C_TEAM][uiInfo.q3HeadCount[C_TEAM]] != ( int )NULL )
		{
			char	model[MAX_QPATH];
			char*	skin;
			strcpy ( model, ( char *)bg_teamSkins[C_TEAM][uiInfo.q3HeadCount[C_TEAM]] );

			if ( Q_stricmp ( model, compare[C_TEAM] ) == 0 )
				sel = uiInfo.q3HeadCount[C_TEAM];

			skin = strchr ( model, '/' );
			if ( !skin )
				continue;

			*skin = '\0';
			skin++;

			uiInfo.q3HeadIcons[C_TEAM][uiInfo.q3HeadCount[C_TEAM]] = trap_R_RegisterShaderNoMip(va("models/players/%s/icon_%s",model,skin));
			Com_sprintf( uiInfo.q3HeadNames[C_TEAM][uiInfo.q3HeadCount[C_TEAM]], sizeof(uiInfo.q3HeadNames[C_TEAM][uiInfo.q3HeadCount[C_TEAM]]), "%s", bg_teamSkins[C_TEAM][uiInfo.q3HeadCount[C_TEAM]]);
			uiInfo.q3HeadCount[C_TEAM]++;
		}

		switch (C_TEAM)
		{
			case TEAM_RED:
			{
				Menu_SetFeederSelection(NULL, FEEDER_Q3HEADS_RED, sel, "ingame_ut_select_player");
				Menu_SetFeederSelection(NULL, FEEDER_Q3HEADS_RED, sel, "psetup_menu");
				break;
			}
			case TEAM_BLUE:
			{
				Menu_SetFeederSelection(NULL, FEEDER_Q3HEADS_BLUE, sel, "ingame_ut_select_player");
				Menu_SetFeederSelection(NULL, FEEDER_Q3HEADS_BLUE, sel, "psetup_menu");
				break;
			}
			case TEAM_FREE:
			{
				Menu_SetFeederSelection(NULL, FEEDER_Q3HEADS,sel, "ingame_ut_select_player");
				Menu_SetFeederSelection(NULL, FEEDER_Q3HEADS, sel, "psetup_menu");
				break;
			}
		}
	}*/
}
//	Menu_SetFeederSelection(NULL, FEEDER_Q3HEADS, sel, "ingame_ut_select_player");
//	Menu_SetFeederSelection(NULL, FEEDER_Q3HEADS, sel, "psetup_menu");

/*
	int 	numdirs;
	int 	numfiles;
	char	dirlist[2048];
	char	filelist[2048];
	char	skinname[64];
	char*	dirptr;
	char*	fileptr;
	int 	i;
	int 	j;
	int 	dirlen;
	int 	filelen;

	uiInfo.q3HeadCount = 0;

	// iterate directory of all player models
	numdirs = trap_FS_GetFileList("models/players", "/", dirlist, 2048 );
	dirptr	= dirlist;
	for (i=0; i<numdirs && uiInfo.q3HeadCount < MAX_PLAYERMODELS; i++,dirptr+=dirlen+1)
	{
		dirlen = strlen(dirptr);

		if (dirlen && dirptr[dirlen-1]=='/') dirptr[dirlen-1]='\0';

		if (!strcmp(dirptr,".") || !strcmp(dirptr,".."))
			continue;

		// iterate all skin files in directory
		numfiles = trap_FS_GetFileList( va("models/players/%s",dirptr), "tga", filelist, 2048 );
		fileptr  = filelist;
		for (j=0; j<numfiles && uiInfo.q3HeadCount < MAX_PLAYERMODELS;j++,fileptr+=filelen+1)
		{
			filelen = strlen(fileptr);

			COM_StripExtension(fileptr,skinname);

			// look for icon_????
			if (Q_stricmpn(skinname, "icon_", 5) == 0 && !(Q_stricmp(skinname,"icon_blue") == 0 || Q_stricmp(skinname,"icon_red") == 0))
			{
				if (Q_stricmp(skinname, "icon_default") == 0) {
					Com_sprintf( uiInfo.q3HeadNames[uiInfo.q3HeadCount], sizeof(uiInfo.q3HeadNames[uiInfo.q3HeadCount]), dirptr);
				} else {
					Com_sprintf( uiInfo.q3HeadNames[uiInfo.q3HeadCount], sizeof(uiInfo.q3HeadNames[uiInfo.q3HeadCount]), "%s/%s",dirptr, skinname + 5);
				}
				uiInfo.q3HeadIcons[uiInfo.q3HeadCount++] = trap_R_RegisterShaderNoMip(va("models/players/%s/%s",dirptr,skinname));
			}

		}
	}
*/

static void UI_ParseGameInfo(const char *teamFile)
{
	char  *token;
	char  *p;
	char  *buff = NULL;

	buff = GetMenuBuffer(teamFile);

	if (!buff)
	{
		return;
	}

	p = buff;

	while (1)
	{
		token = COM_ParseExt( &p, qtrue );

		if(!token || (token[0] == 0) || (token[0] == '}'))
		{
			break;
		}

		if (Q_stricmp( token, "}" ) == 0)
		{
			break;
		}

		if (Q_stricmp(token, "gametypes") == 0)
		{
			if (GameType_Parse(&p, qfalse))
			{
				continue;
			}
			else
			{
				break;
			}
		}

		if (Q_stricmp(token, "joingametypes") == 0)
		{
			if (GameType_Parse(&p, qtrue))
			{
				continue;
			}
			else
			{
				break;
			}
		}

		if (Q_stricmp(token, "maps") == 0)
		{
			// start a new menu
			MapList_Parse(&p);
		}
	}
}

/**
 * UI_Pause
 */
static void UI_Pause(qboolean b)
{
	if (b)
	{
		// pause the game and set the ui keycatcher
		trap_Cvar_Set( "cl_paused", "1" );
		trap_Key_SetCatcher( KEYCATCH_UI );
	}
	else
	{
		// unpause the game and clear the ui keycatcher
		trap_Key_SetCatcher( trap_Key_GetCatcher() & ~KEYCATCH_UI );
		trap_Key_ClearStates();
		trap_Cvar_Set( "cl_paused", "0" );
	}
}

/**
 * UI_OwnerDraw_Width
 */
//static int UI_OwnerDraw_Width(int ownerDraw)
//{
//	return 0;
//}

/**
 * UI_PlayCinematic
 */
static int UI_PlayCinematic(const char *name, float x, float y, float w, float h)
{
	return trap_CIN_PlayCinematic(name, x, y, w, h, (CIN_loop | CIN_silent));
}

/**
 * UI_StopCinematic
 */
static void UI_StopCinematic(int handle)
{
	if (handle >= 0)
	{
		trap_CIN_StopCinematic(handle);
	}
	else
	{
		handle = abs(handle);

		if (handle == UI_MAPCINEMATIC)
		{
			if (uiInfo.mapList[ui_currentMap.integer].cinematic >= 0)
			{
				trap_CIN_StopCinematic(uiInfo.mapList[ui_currentMap.integer].cinematic);
				uiInfo.mapList[ui_currentMap.integer].cinematic = -1;
			}
		}
		else if (handle == UI_NETMAPCINEMATIC)
		{
			if (uiInfo.serverStatus.currentServerCinematic >= 0)
			{
				trap_CIN_StopCinematic(uiInfo.serverStatus.currentServerCinematic);
				uiInfo.serverStatus.currentServerCinematic = -1;
			}
		}
	}
}

/**
 * UI_DrawCinematic
 */
static void UI_DrawCinematic(int handle, float x, float y, float w, float h)
{
	trap_CIN_SetExtents(handle, x, y, w, h);
	trap_CIN_DrawCinematic(handle);
}

/**
 * UI_RunCinematicFrame
 */
static void UI_RunCinematicFrame(int handle)
{
	trap_CIN_RunCinematic(handle);
}

/**
 * UI_Init
 */
void _UI_Init( qboolean inGameLoad )
{
//	const char	*menuSet;
	int 	start;

	// Add pool memory
	add_pool(ui_malloc_pool, MALLOC_POOL_SIZE);

	start = trap_Milliseconds();

	//uiInfo.inGameLoad = inGameLoad;

	UI_RegisterCvars();
	UI_InitMemory();

	// cache redundant calulations
	trap_GetGlconfig( &uiInfo.uiDC.glconfig );

	// for 640x480 virtualized screen
	uiInfo.uiDC.yscale = uiInfo.uiDC.glconfig.vidHeight / 480.0;
	uiInfo.uiDC.xscale = uiInfo.uiDC.glconfig.vidWidth / 640.0;

	if (uiInfo.uiDC.glconfig.vidWidth * 480 > uiInfo.uiDC.glconfig.vidHeight * 640)
	{
		// wide screen
		uiInfo.uiDC.bias = 0.5 * (uiInfo.uiDC.glconfig.vidWidth - (uiInfo.uiDC.glconfig.vidHeight * (640.0 / 480.0)));
	}
	else
	{
		// no wide screen
		uiInfo.uiDC.bias = 0;
	}

	//UI_Load();
	uiInfo.uiDC.registerShaderNoMip  = &trap_R_RegisterShaderNoMip;
	uiInfo.uiDC.setColor		 = &UI_SetColor;
	uiInfo.uiDC.drawHandlePic	 = &UI_DrawHandlePic;
	uiInfo.uiDC.drawStretchPic	 = &trap_R_DrawStretchPic;
	uiInfo.uiDC.drawText		 = &UI_Text_Paint;
	uiInfo.uiDC.textWidth		 = &UI_Text_Width;
	uiInfo.uiDC.textHeight		 = &UI_Text_Height;
	uiInfo.uiDC.registerModel	 = &trap_R_RegisterModel;
	uiInfo.uiDC.registerSkin	 = &trap_R_RegisterSkin;
	uiInfo.uiDC.modelBounds 	 = &trap_R_ModelBounds;
	uiInfo.uiDC.fillRect		 = &UI_FillRect;
	uiInfo.uiDC.drawRect		 = &_UI_DrawRect;
	uiInfo.uiDC.drawSides		 = &_UI_DrawSides;
	uiInfo.uiDC.drawTopBottom	 = &_UI_DrawTopBottom;
	uiInfo.uiDC.clearScene		 = &trap_R_ClearScene;
	uiInfo.uiDC.drawSides		 = &_UI_DrawSides;
	uiInfo.uiDC.addRefEntityToScene  = &trap_R_AddRefEntityToScene;
	uiInfo.uiDC.renderScene 	 = &trap_R_RenderScene;
	uiInfo.uiDC.registerFont	 = &trap_R_RegisterFont;
	uiInfo.uiDC.ownerDrawItem	 = &UI_OwnerDraw;
	uiInfo.uiDC.getValue		 = &UI_GetValue;
	uiInfo.uiDC.ownerDrawVisible	 = &UI_OwnerDrawVisible;
	uiInfo.uiDC.runScript		 = &UI_RunMenuScript;
	uiInfo.uiDC.getTeamColor	 = &UI_GetTeamColor;
	uiInfo.uiDC.setCVar 	 = trap_Cvar_Set;
	uiInfo.uiDC.updateCVars 	 = UI_UpdateCvars;
	uiInfo.uiDC.getCVarString	 = trap_Cvar_VariableStringBuffer;
	uiInfo.uiDC.getCVarValue	 = trap_Cvar_VariableValue;
	uiInfo.uiDC.drawTextWithCursor	 = &UI_Text_PaintWithCursor;
	uiInfo.uiDC.setOverstrikeMode	 = &trap_Key_SetOverstrikeMode;
	uiInfo.uiDC.getOverstrikeMode	 = &trap_Key_GetOverstrikeMode;
	uiInfo.uiDC.startLocalSound  = &trap_S_StartLocalSound;
	uiInfo.uiDC.ownerDrawHandleKey	 = &UI_OwnerDrawHandleKey;
	uiInfo.uiDC.feederCount    = &UI_FeederCount;
	uiInfo.uiDC.feederItemImage  = &UI_FeederItemImage;
	uiInfo.uiDC.feederItemText	 = &UI_FeederItemText;
	uiInfo.uiDC.feederSelection  = &UI_FeederSelection;
	uiInfo.uiDC.setBinding		 = &trap_Key_SetBinding;
	uiInfo.uiDC.getBindingBuf	 = &trap_Key_GetBindingBuf;
	uiInfo.uiDC.keynumToStringBuf	 = &trap_Key_KeynumToStringBuf;
	uiInfo.uiDC.executeText 	 = &trap_Cmd_ExecuteText;
	uiInfo.uiDC.Error		 = &Com_Error;
	uiInfo.uiDC.Print		 = &Com_Printf;
	uiInfo.uiDC.Pause		 = &UI_Pause;
	uiInfo.uiDC.ownerDrawWidth	 = &UI_OwnerDrawWidth;
	uiInfo.uiDC.registerSound	 = &trap_S_RegisterSound;
	uiInfo.uiDC.startBackgroundTrack = &trap_S_StartBackgroundTrack;
	uiInfo.uiDC.stopBackgroundTrack  = &trap_S_StopBackgroundTrack;
	uiInfo.uiDC.playCinematic	 = &UI_PlayCinematic;
	uiInfo.uiDC.stopCinematic	 = &UI_StopCinematic;
	uiInfo.uiDC.drawCinematic	 = &UI_DrawCinematic;
	uiInfo.uiDC.runCinematicFrame	 = &UI_RunCinematicFrame;

	Init_Display(&uiInfo.uiDC);

	String_Init();

	uiInfo.uiDC.cursor	= trap_R_RegisterShaderNoMip( "menu/art/3_cursor2" );
	uiInfo.uiDC.whiteShader = trap_R_RegisterShaderNoMip( "white" );

	AssetCache();

	uiInfo.teamCount	  = 0;
	uiInfo.characterCount = 0;
	uiInfo.aliasCount	  = 0;

	UI_ParseTeamInfo("teaminfo.txt");
	UI_LoadTeams();
	UI_ParseGameInfo("gameinfo.txt");

//	menuSet = UI_Cvar_VariableString("ui_menuFiles"); // not used for anything

//	if ((menuSet == NULL) || (menuSet[0] == '\0'))
//	{
//		menuSet = "ui/menus.txt";
//	}

	UI_LoadMenus("ui/menus.txt", qtrue);
	UI_LoadMenus("ui/ingame.txt", qfalse);

	Menus_CloseAll();

	trap_LAN_LoadCachedServers();
	UI_LoadBestScores(uiInfo.mapList[0].mapLoadName, uiInfo.gameTypes[ui_gameType.integer].gtEnum);

	UI_BuildQ3Model_List();

	UI_LoadBots();

	// sets defaults for ui temp cvars
	uiInfo.effectsColor = gamecodetoui[(int)trap_Cvar_VariableValue("color") - 1];
	uiInfo.currentCrosshair = (int)trap_Cvar_VariableValue("cg_drawCrosshair") - 1;
	trap_Cvar_Set("ui_mousePitch", (trap_Cvar_VariableValue("m_pitch") >= 0) ? "0" : "1");

	uiInfo.serverStatus.currentServerCinematic = -1;
	uiInfo.previewMovie 		   = -1;

	if (trap_Cvar_VariableValue("ui_TeamArenaFirstRun") == 0)
	{
		trap_Cvar_Set("s_volume", "0.8");
		trap_Cvar_Set("s_musicvolume", "0.5");
		trap_Cvar_Set("ui_TeamArenaFirstRun", "1");
	}

	trap_Cvar_Register(NULL, "debug_protocol", "", 0 );

	utCrosshairLoadAll ( );

	BuildMiniMapShaders();
}

/*
=================
UI_KeyEvent
=================
*/
void _UI_KeyEvent( int key, qboolean down )
{
	if (Menu_Count() > 0)
	{
		menuDef_t  *menu = Menu_GetFocused();

		if (menu)
		{
			if ((key == K_ESCAPE) && down && !Menus_AnyFullScreenVisible())
			{
				Menus_CloseAll();
			}
			else
			{
				Menu_HandleKey(menu, key, down );
			}
		}
		else
		{
			trap_Key_SetCatcher( trap_Key_GetCatcher() & ~KEYCATCH_UI );
			trap_Key_ClearStates();
			trap_Cvar_Set( "cl_paused", "0" );
		}
	}

	//if ((s > 0) && (s != menu_null_sound)) {
	//	trap_S_StartLocalSound( s, CHAN_LOCAL_SOUND );
	//}
}

/*
=================
UI_MouseEvent
=================
*/
void _UI_MouseEvent( int dx, int dy )
{
	// update mouse screen position
	uiInfo.uiDC.cursorx += dx;

	if (uiInfo.uiDC.cursorx < 0)
	{
		uiInfo.uiDC.cursorx = 0;
	}
	else if (uiInfo.uiDC.cursorx > SCREEN_WIDTH)
	{
		uiInfo.uiDC.cursorx = SCREEN_WIDTH;
	}

	uiInfo.uiDC.cursory += dy;

	if (uiInfo.uiDC.cursory < 0)
	{
		uiInfo.uiDC.cursory = 0;
	}
	else if (uiInfo.uiDC.cursory > SCREEN_HEIGHT)
	{
		uiInfo.uiDC.cursory = SCREEN_HEIGHT;
	}

	if (Menu_Count() > 0)
	{
		//menuDef_t *menu = Menu_GetFocused();
		//Menu_HandleMouseMove(menu, uiInfo.uiDC.cursorx, uiInfo.uiDC.cursory);
		Display_MouseMove(NULL, uiInfo.uiDC.cursorx, uiInfo.uiDC.cursory);
	}
}

/**
 * UI_LoadNonIngame
 */
void UI_LoadNonIngame(void)
{
	const char	*menuSet = UI_Cvar_VariableString("ui_menuFiles");

	if ((menuSet == NULL) || (menuSet[0] == '\0'))
	{
		menuSet = "ui/menus.txt";
	}
	UI_LoadMenus(menuSet, qfalse);
	uiInfo.inGameLoad = qfalse;
}

/**
 * _UI_SetActiveMenu
 */
void _UI_SetActiveMenu( uiMenuCommand_t menu )
{
	char  buf[256];
	char  gender[20];

	// this should be the ONLY way the menu system is brought up
	// enusure minumum menu data is cached
	if (Menu_Count() > 0)
	{
		vec3_t	v;
		v[0] = v[1] = v[2] = 0;

		switch (menu)
		{
			case UIMENU_NONE:
				trap_Key_SetCatcher( trap_Key_GetCatcher() & ~KEYCATCH_UI );
				trap_Key_ClearStates();
				trap_Cvar_Set( "cl_paused", "0" );
				Menus_CloseAll();
				return;

			case UIMENU_MAIN:
				//trap_Cvar_Set( "sv_killserver", "1" );
				trap_Key_SetCatcher( KEYCATCH_UI );

				//trap_S_StartLocalSound( trap_S_RegisterSound("sound/misc/menu_background.wav", qfalse) , CHAN_LOCAL_SOUND );
				//trap_S_StartBackgroundTrack("sound/misc/menu_background.wav", NULL);
				if (uiInfo.inGameLoad)
				{
					UI_LoadNonIngame();
				}

				Menus_CloseAll();
				Menus_ActivateByName("main");

				trap_Cvar_VariableStringBuffer("com_errorMessage", buf, sizeof(buf));

				if (strlen(buf))
				{
					if (!ui_singlePlayerActive.integer)
					{
						Menus_ActivateByName("error_popmenu");
					}
					else
					{
						trap_Cvar_Set("com_errorMessage", "");
					}
				}
				return;

			case UIMENU_TEAM:
				trap_Key_SetCatcher( KEYCATCH_UI );
				Menus_ActivateByName("team");
				return;

			case UIMENU_POSTGAME:
				//trap_Cvar_Set( "sv_killserver", "1" );
				trap_Key_SetCatcher( KEYCATCH_UI );

				if (uiInfo.inGameLoad)
				{
					UI_LoadNonIngame();
				}
				Menus_CloseAll();
				Menus_ActivateByName("endofgame");
				return;

			case UIMENU_INGAME:
				trap_Cvar_Set( "cl_paused", "1" );
				trap_Key_SetCatcher( KEYCATCH_UI );
				UI_BuildPlayerList();
				Menus_CloseAll();
				Menus_ActivateByName("ingame");
				return;

			case UIMENU_SELECT_GEAR:
				trap_Key_SetCatcher( KEYCATCH_UI );
				Menus_CloseAll();
				Menus_ActivateByName("ingame_ut_select_gear");
				return;

			case UIMENU_SELECT_TEAM:
				trap_Key_SetCatcher( KEYCATCH_UI );
				Menus_CloseAll();
				Menus_ActivateByName("ingame_ut_select_team");
				return;

			case UIMENU_KEYBOARD_INTERFACE:
				trap_Key_SetCatcher( KEYCATCH_UI );
				Menus_CloseAll();
				Menus_ActivateByName("ingame_ut_keyboard_interface");
				return;

			/*		case UIMENU_RADIO:
						trap_Cvar_VariableStringBuffer ( "ui_gender", gender, 20 );

						trap_Key_SetCatcher( KEYCATCH_UI );
						Menus_CloseAll();
						Menus_ActivateByName(va("ut_radio_%s",gender));
						return; */

			case UIMENU_RADIO_0:
				trap_Cvar_VariableStringBuffer ( "ui_gender", gender, 20 );
				trap_Key_SetCatcher( KEYCATCH_UI );
				Menus_CloseAll();
				Menus_ActivateByName(va("ut_radio_%s_1", gender));
				return;

			case UIMENU_RADIO_1:
				trap_Cvar_VariableStringBuffer ( "ui_gender", gender, 20 );
				trap_Key_SetCatcher( KEYCATCH_UI );
				Menus_CloseAll();
				Menus_ActivateByName(va("ut_radio_%s_2", gender));
				return;

			case UIMENU_RADIO_2:
				trap_Cvar_VariableStringBuffer ( "ui_gender", gender, 20 );
				trap_Key_SetCatcher( KEYCATCH_UI );
				Menus_CloseAll();
				Menus_ActivateByName(va("ut_radio_%s_3", gender));
				return;

			case UIMENU_RADIO_3:
				trap_Cvar_VariableStringBuffer ( "ui_gender", gender, 20 );
				trap_Key_SetCatcher( KEYCATCH_UI );
				Menus_CloseAll();
				Menus_ActivateByName(va("ut_radio_%s_4", gender));
				return;

			default:
				return;
		}
	}
}

/**
 * _UI_IsFullscreen
 */
qboolean _UI_IsFullscreen( void )
{
	return Menus_AnyFullScreenVisible();
}

static connstate_t	lastConnState;
static char 	lastLoadingText[MAX_INFO_VALUE];

/**
 * UI_ReadableSize
 */
static void UI_ReadableSize ( char *buf, int bufsize, int value )
{
	if (value > 1024 * 1024 * 1024)   // gigs
	{
		Com_sprintf( buf, bufsize, "%d", value / (1024 * 1024 * 1024));
		Com_sprintf( buf + strlen(buf), bufsize - strlen(buf), ".%02d GB",
				 (value % (1024 * 1024 * 1024)) * 100 / (1024 * 1024 * 1024));
	}
	else if (value > 1024 * 1024)	  // megs
	{
		Com_sprintf( buf, bufsize, "%d", value / (1024 * 1024));
		Com_sprintf( buf + strlen(buf), bufsize - strlen(buf), ".%02d MB",
				 (value % (1024 * 1024)) * 100 / (1024 * 1024));
	}
	else if (value > 1024)		// kilos
	{
		Com_sprintf( buf, bufsize, "%d KB", value / 1024 );
	}
	else	 // bytes
	{
		Com_sprintf( buf, bufsize, "%d bytes", value );
	}
}

// Assumes time is in msec
static void UI_PrintTime ( char *buf, int bufsize, int time )
{
	time /= 1000;  // change to seconds

	if (time > 3600)   // in the hours range
	{
		Com_sprintf( buf, bufsize, "%d hr %d min", time / 3600, (time % 3600) / 60 );
	}
	else if (time > 60) // mins
	{
		Com_sprintf( buf, bufsize, "%d min %d sec", time / 60, time % 60 );
	}
	else	  // secs
	{
		Com_sprintf( buf, bufsize, "%d sec", time );
	}
}

/**
 * $(function)
 */
void UI_Text_PaintCenter(float x, float y, float scale, vec4_t color, const char *text, float adjust)
{
	int  len = UI_Text_Width(text, scale, 0);

	UI_Text_Paint(x - len / 2, y, scale, color, text, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);
}

/**
 * UI_DisplayDownloadInfo
 */
static void UI_DisplayDownloadInfo( const char *downloadName, float centerPoint, float yStart, float scale )
{
	//static char  dlText[]	= "Downloading:";
	//static char  etaText[]	= "Estimated time left:";
	//static char  xferText[] = "Transfer rate:";

	int 	 downloadSize, downloadCount, downloadTime;
	char		 dlSizeBuf[64], totalSizeBuf[64], xferRateBuf[64], dlTimeBuf[64];
	int 	 xferRate;
	int 	 leftWidth;
	const char	 *s;

	scale		 *= 0.75;
	downloadSize  = trap_Cvar_VariableValue( "cl_downloadSize" );
	downloadCount = trap_Cvar_VariableValue( "cl_downloadCount" );
	downloadTime  = trap_Cvar_VariableValue( "cl_downloadTime" );

	leftWidth	  = 320;

	UI_SetColor(colorWhite);
//	UI_Text_PaintCenter(centerPoint, yStart + 112, scale, colorWhite, dlText, 0);
//	UI_Text_PaintCenter(centerPoint, yStart + 144, scale, colorWhite, etaText, 0);
//	UI_Text_PaintCenter(centerPoint, yStart + 208, scale, colorWhite, xferText, 0);

	if (downloadSize > 0)
	{
		s = va( "%s (%d%%)", downloadName, (int)((float)downloadCount * 100.f / (float)downloadSize));
	}
	else
	{
		s = downloadName;
	}
	UI_Text_PaintCenter(leftWidth, 256 + 32, 0.4f, colorWhite, s, 0);

	UI_ReadableSize( dlSizeBuf, sizeof dlSizeBuf, downloadCount );
	UI_ReadableSize( totalSizeBuf, sizeof totalSizeBuf, downloadSize );

	if ((downloadCount < 4096) || !downloadTime)
	{
		UI_Text_PaintCenter(leftWidth, 192, 0.6f, colorWhite, "estimating", 0);
		UI_Text_PaintCenter(leftWidth, 224, 0.6f, colorWhite, va("(%s of %s copied)", dlSizeBuf, totalSizeBuf), 0);
	}
	else
	{
		if ((uiInfo.uiDC.realTime - downloadTime) / 1000)
		{
			xferRate = downloadCount / ((uiInfo.uiDC.realTime - downloadTime) / 1000);
		}
		else
		{
			xferRate = 0;
		}
		UI_ReadableSize( xferRateBuf, sizeof xferRateBuf, xferRate );

		// Extrapolate estimated completion time
		if (downloadSize && xferRate)
		{
			int  n = downloadSize / xferRate; // estimated time for entire d/l in secs

			// We do it in K (/1024) because we'd overflow around 4MB
			UI_PrintTime ( dlTimeBuf, sizeof dlTimeBuf,
					   (n - (((downloadCount / 1024) * n) / (downloadSize / 1024))) * 1000);

			UI_Text_PaintCenter(leftWidth, 224, 0.6f, colorWhite, dlTimeBuf, 0);
			UI_Text_PaintCenter(leftWidth, 256, 0.6f, colorWhite, va("(%s of %s copied)", dlSizeBuf, totalSizeBuf), 0);
		}
		else
		{
			UI_Text_PaintCenter(leftWidth, 192, 0.6f, colorWhite, "estimating", 0);

			if (downloadSize)
			{
				UI_Text_PaintCenter(leftWidth, 192, 0.6f, colorWhite, va("(%s of %s copied)", dlSizeBuf, totalSizeBuf), 0);
			}
			else
			{
				UI_Text_PaintCenter(leftWidth, 192, 0.6f, colorWhite, va("(%s copied)", dlSizeBuf), 0);
			}
		}

		if (xferRate)
		{
			UI_Text_PaintCenter(leftWidth, 192, 0.6f, colorWhite, va("%s/Sec", xferRateBuf), 0);
		}
	}
}

/*
========================
UI_DrawConnectScreen

This will also be overlaid on the cgame info screen during loading
to prevent it from blinking away too rapidly on local or lan games.
========================
*/
void UI_DrawConnectScreen( qboolean overlay )
{
	char		 *s;
	uiClientState_t  cstate;
	char		 info[MAX_INFO_VALUE];
	char		 text[256];
	float		 centerPoint, yStart, scale;

	menuDef_t	 *menu = Menus_FindByName("Connect");

	if (!overlay && menu)
	{
		Menu_Paint(menu, qtrue);
	}

	if (!overlay)
	{
		centerPoint = 320;
		yStart		= 130;
		scale		= 0.35f;
	}
	else
	{
		centerPoint = 320;
		yStart		= 32;
		scale		= 0.35f;
		return;
	}

	// see what information we should display
	trap_GetClientState( &cstate );

	// Save this data for the ui
	{
		static char  server [128] = "";

		if (Q_stricmp ( server, cstate.servername ))
		{
			Com_sprintf ( server, 128, "%s", cstate.servername );
			trap_Cvar_Set ( "cstate_servername", cstate.servername);
		}
	}

	info[0] = '\0';

	if(trap_GetConfigString( CS_SERVERINFO, info, sizeof(info)))
	{
		UI_Text_PaintCenter(centerPoint, yStart + 32, scale, colorWhite, va( "Loading %s", Info_ValueForKey( info, "mapname" )), 0);
	}

	if (!Q_stricmp(cstate.servername, "localhost"))
	{
		UI_Text_Paint(10, 130, scale, colorWhite, va("Starting up..."), 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);
	}
	else
	{
		strcpy(text, va("Connecting to %s", cstate.servername));
		UI_Text_Paint(10, 130, scale, colorWhite, text, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);
	}

	//UI_DrawProportionalString( 320, 96, "Press Esc to abort", UI_CENTER|UI_SMALLFONT|UI_DROPSHADOW, menu_text_color );

	// display global MOTD at bottom
	UI_Text_PaintCenter(centerPoint, 600, scale, colorWhite, Info_ValueForKey( cstate.updateInfoString, "motd" ), 0);

	// print any server info (server full, bad version, etc)
	if (cstate.connState < CA_CONNECTED)
	{
		char  sub[129];
		char  *cp;
		int   r = 0;
		memset(sub, 0, sizeof(sub));
		cp = &cstate.messageString[0];

		while (cp < &cstate.messageString[0] + strlen(cstate.messageString))
		{
			memcpy(&sub[0], cp, 61);
			cp = cp + 61;
			UI_Text_PaintCenter(centerPoint, yStart + 156 + (r * 20), scale * 0.75, colorWhite, sub, 0);
			r++;
		}
	}

	if (lastConnState > cstate.connState)
	{
		lastLoadingText[0] = '\0';
	}
	lastConnState = cstate.connState;

	switch (cstate.connState)
	{
		case CA_CONNECTING:
			s = va("Awaiting connection...%i", cstate.connectPacketCount);
			break;

		case CA_CHALLENGING:
			s = va("Awaiting challenge...%i", cstate.connectPacketCount);
			break;

		case CA_CONNECTED:
			{
				char  downloadName[MAX_INFO_VALUE];

				trap_Cvar_VariableStringBuffer( "cl_downloadName", downloadName, sizeof(downloadName));

				if (*downloadName)
				{
					UI_DisplayDownloadInfo( downloadName, centerPoint, yStart, scale );
					return;
				}
			}
			s = "Awaiting gamestate...";
			break;

		case CA_LOADING:
			return;

		case CA_PRIMED:
			return;

		default:
			return;
	}

	if (Q_stricmp(cstate.servername, "localhost"))
	{
		UI_Text_Paint(10, 320 + 48, scale, colorWhite, s, 0, 0, 0);
	}

	// password required / connection rejected information goes here
}

/*
================
cvars
================
*/

typedef struct
{
	vmCvar_t  *vmCvar;
	char	  *cvarName;
	char	  *defaultString;
	int   cvarFlags;
} cvarTable_t;

vmCvar_t  ui_ffa_fraglimit;
vmCvar_t  ui_ffa_timelimit;

vmCvar_t  ui_tourney_fraglimit;
vmCvar_t  ui_tourney_timelimit;

vmCvar_t  ui_team_fraglimit;
vmCvar_t  ui_team_timelimit;
vmCvar_t  ui_team_friendly;

vmCvar_t  ui_ctf_capturelimit;
vmCvar_t  ui_ctf_timelimit;
vmCvar_t  ui_ctf_friendly;

vmCvar_t  statsoutput0;
vmCvar_t  statsoutput1;

vmCvar_t  ui_arenasFile;

vmCvar_t  ui_botsFile;
vmCvar_t  ui_spScores1;
vmCvar_t  ui_spScores2;
vmCvar_t  ui_spScores3;
vmCvar_t  ui_spScores4;
vmCvar_t  ui_spScores5;
vmCvar_t  ui_spAwards;
vmCvar_t  ui_spVideos;
vmCvar_t  ui_spSkill;

vmCvar_t  ui_spSelection;
vmCvar_t  arenasFile;

vmCvar_t  ui_browserMaster;
vmCvar_t  ui_browserGameType;
vmCvar_t  ui_browserSortKey;
vmCvar_t  ui_browserSortDir;
vmCvar_t  ui_browserShowFull;
vmCvar_t  ui_browserShowEmpty;
vmCvar_t  ui_browserShowAccounts; //@Barbatos - auth system
vmCvar_t  ui_browserShowPrivate; //@Barbatos - passworded servers

vmCvar_t  ui_brassTime;
vmCvar_t  ui_drawCrosshair;
vmCvar_t  ui_drawCrosshairNames;
vmCvar_t  ui_marks;

vmCvar_t  ui_server1;
vmCvar_t  ui_server2;
vmCvar_t  ui_server3;
vmCvar_t  ui_server4;
vmCvar_t  ui_server5;
vmCvar_t  ui_server6;
vmCvar_t  ui_server7;
vmCvar_t  ui_server8;
vmCvar_t  ui_server9;
vmCvar_t  ui_server10;
vmCvar_t  ui_server11;
vmCvar_t  ui_server12;
vmCvar_t  ui_server13;
vmCvar_t  ui_server14;
vmCvar_t  ui_server15;
vmCvar_t  ui_server16;

vmCvar_t  ui_redteam;
vmCvar_t  ui_redteam1;
vmCvar_t  ui_redteam2;
vmCvar_t  ui_redteam3;
vmCvar_t  ui_redteam4;
vmCvar_t  ui_redteam5;
vmCvar_t  ui_blueteam;
vmCvar_t  ui_blueteam1;
vmCvar_t  ui_blueteam2;
vmCvar_t  ui_blueteam3;
vmCvar_t  ui_blueteam4;
vmCvar_t  ui_blueteam5;
vmCvar_t  ui_teamName;
vmCvar_t  ui_dedicated;
vmCvar_t  ui_gameType;
vmCvar_t  ui_netGameType;
vmCvar_t  ui_actualNetGameType;
vmCvar_t  ui_joinGameType;
vmCvar_t  ui_netSource;
vmCvar_t  ui_serverFilterType;
vmCvar_t  ui_opponentName;
vmCvar_t  ui_menuFiles;
vmCvar_t  ui_currentMap;
vmCvar_t  ui_currentNetMap;
vmCvar_t  ui_mapIndex;
vmCvar_t  ui_selectedPlayer;
vmCvar_t  ui_selectedPlayerName;
vmCvar_t  ui_lastServerRefresh_0;
vmCvar_t  ui_lastServerRefresh_1;
vmCvar_t  ui_lastServerRefresh_2;
vmCvar_t  ui_lastServerRefresh_3;
vmCvar_t  ui_singlePlayerActive;
vmCvar_t  ui_scoreAccuracy;
vmCvar_t  ui_scoreImpressives;
vmCvar_t  ui_scoreExcellents;
vmCvar_t  ui_scoreCaptures;
vmCvar_t  ui_scoreDefends;
vmCvar_t  ui_scoreAssists;
vmCvar_t  ui_scoreGauntlets;
vmCvar_t  ui_scoreScore;
vmCvar_t  ui_scorePerfect;
vmCvar_t  ui_scoreTeam;
vmCvar_t  ui_scoreBase;
vmCvar_t  ui_scoreTimeBonus;
vmCvar_t  ui_scoreSkillBonus;
vmCvar_t  ui_scoreShutoutBonus;
vmCvar_t  ui_scoreTime;
vmCvar_t  ui_captureLimit;
vmCvar_t  ui_fragLimit;
vmCvar_t  ui_smallFont;
vmCvar_t  ui_bigFont;
vmCvar_t  ui_findPlayer;
vmCvar_t  ui_Q3Model;
vmCvar_t  ui_hudFiles;
vmCvar_t  ui_recordSPDemo;
vmCvar_t  ui_realCaptureLimit;
vmCvar_t  ui_realWarmUp;
vmCvar_t  ui_serverStatusTimeOut;

vmCvar_t  ui_gearItemCount;

vmCvar_t  ui_modversion; //@Barbatos

//Iain addtion of changed ui head...
vmCvar_t	 ui_currentmodel;

vmCvar_t	 ui_ingameMaster;

//@Barbatos



cvarTable_t  cvarTable[] = {
    { &ui_ffa_fraglimit,       "ui_ffa_fraglimit",            "20",               CVAR_ARCHIVE                    },
    { &ui_ffa_timelimit,       "ui_ffa_timelimit",            "0",                CVAR_ARCHIVE                    },

    { &ui_tourney_fraglimit,   "ui_tourney_fraglimit",        "0",                CVAR_ARCHIVE                    },
    { &ui_tourney_timelimit,   "ui_tourney_timelimit",        "15",               CVAR_ARCHIVE                    },

    { &ui_team_fraglimit,       "ui_team_fraglimit",          "0",                CVAR_ARCHIVE                    },
    { &ui_team_timelimit,       "ui_team_timelimit",          "20",               CVAR_ARCHIVE                    },
    { &ui_team_friendly,        "ui_team_friendly",           "1",                CVAR_ARCHIVE                    },

    { &ui_ctf_capturelimit,     "ui_ctf_capturelimit",        "8",                CVAR_ARCHIVE                    },
    { &ui_ctf_timelimit,        "ui_ctf_timelimit",           "30",               CVAR_ARCHIVE                    },
    { &ui_ctf_friendly,         "ui_ctf_friendly",            "0",                CVAR_ARCHIVE                    },

    { &ui_arenasFile,           "g_arenasFile",               "",                 CVAR_INIT | CVAR_ROM            },
    { &ui_botsFile,             "g_botsFile",                 "",                 CVAR_INIT | CVAR_ROM            },
    { &ui_spScores1,            "g_spScores1",                "",                 CVAR_ARCHIVE | CVAR_ROM         },
    { &ui_spScores2,            "g_spScores2",                "",                 CVAR_ARCHIVE | CVAR_ROM         },
    { &ui_spScores3,            "g_spScores3",                "",                 CVAR_ARCHIVE | CVAR_ROM         },
    { &ui_spScores4,            "g_spScores4",                "",                 CVAR_ARCHIVE | CVAR_ROM         },
    { &ui_spScores5,            "g_spScores5",                "",                 CVAR_ARCHIVE | CVAR_ROM         },
    { &ui_spAwards,             "g_spAwards",                 "",                 CVAR_ARCHIVE | CVAR_ROM         },
    { &ui_spVideos,             "g_spVideos",                 "",                 CVAR_ARCHIVE | CVAR_ROM         },
    { &ui_spSkill,              "g_spSkill",                  "2",                CVAR_ARCHIVE                    },

    { &ui_spSelection,          "ui_spSelection",             "",                 CVAR_ROM                        },

    { &ui_browserMaster,        "ui_browserMaster",           "1",                CVAR_ARCHIVE                    }, // 1
    { &ui_browserGameType,      "ui_browserGameType",         "4",                CVAR_ARCHIVE                    }, // 4
    { &ui_browserSortKey,       "ui_browserSortKey",          "4",                CVAR_ARCHIVE                    },
    { &ui_browserSortDir,       "ui_browserSortDir",          "0",                CVAR_ARCHIVE                    },
    { &ui_browserShowFull,      "ui_browserShowFull",         "1",                CVAR_ARCHIVE                    },
    { &ui_browserShowEmpty,     "ui_browserShowEmpty",        "1",                CVAR_ARCHIVE                    },
    { &ui_browserShowAccounts,  "ui_browserShowAccounts",     "1",                CVAR_ARCHIVE                                    }, //@Barbatos
    { &ui_browserShowPrivate,   "ui_browserShowPrivate",      "1",                CVAR_ARCHIVE                                    }, //@Barbatos

    { &ui_brassTime,            "cg_brassTime",               "2500",             CVAR_ARCHIVE                    },
    { &ui_drawCrosshair,        "cg_drawCrosshair",           "11",               CVAR_ARCHIVE                    },
    { &ui_drawCrosshairNames,   "cg_drawCrosshairNames",      "1",                CVAR_ARCHIVE                    },
    { &ui_marks,                "cg_marks",                   "1",                CVAR_ARCHIVE                    },

    { &ui_server1,              "server1",                    "",                 CVAR_ARCHIVE                    },
    { &ui_server2,              "server2",                    "",                 CVAR_ARCHIVE                    },
    { &ui_server3,              "server3",                    "",                 CVAR_ARCHIVE                    },
    { &ui_server4,              "server4",                    "",                 CVAR_ARCHIVE                    },
    { &ui_server5,              "server5",                    "",                 CVAR_ARCHIVE                    },
    { &ui_server6,              "server6",                    "",                 CVAR_ARCHIVE                    },
    { &ui_server7,              "server7",                    "",                 CVAR_ARCHIVE                    },
    { &ui_server8,              "server8",                    "",                 CVAR_ARCHIVE                    },
    { &ui_server9,              "server9",                    "",                 CVAR_ARCHIVE                    },
    { &ui_server10,             "server10",                   "",                 CVAR_ARCHIVE                    },
    { &ui_server11,             "server11",                   "",                 CVAR_ARCHIVE                    },
    { &ui_server12,             "server12",                   "",                 CVAR_ARCHIVE                    },
    { &ui_server13,             "server13",                   "",                 CVAR_ARCHIVE                    },
    { &ui_server14,             "server14",                   "",                 CVAR_ARCHIVE                    },
    { &ui_server15,             "server15",                   "",                 CVAR_ARCHIVE                    },
    { &ui_server16,             "server16",                   "",                 CVAR_ARCHIVE                    },
    { &ui_new,                  "ui_new",                     "0",                CVAR_TEMP                       },
    { &ui_debug,                "ui_debug",                   "0",                CVAR_TEMP                       },
    { &ui_initialized,          "ui_initialized",             "0",                CVAR_TEMP                       },
    { &ui_teamName,             "ui_teamName",                "Pagans",           CVAR_ARCHIVE                    },
    { &ui_opponentName,         "ui_opponentName",            "Stroggs",          CVAR_ARCHIVE                    },
    { &ui_redteam,              "ui_redteam",                 "Pagans",           CVAR_ARCHIVE                    },
    { &ui_blueteam,             "ui_blueteam",                "Stroggs",          CVAR_ARCHIVE                    },
    { &ui_dedicated,            "ui_dedicated",               "0",                CVAR_ARCHIVE                    },
    { &ui_gameType,             "ui_gametype",                "3",                CVAR_ARCHIVE                    },
    { &ui_joinGameType,         "ui_joinGametype",            "0",                CVAR_ARCHIVE                    },
    { &ui_netGameType,          "ui_netGametype",             "3",                CVAR_ARCHIVE                    },
    { &ui_actualNetGameType,    "ui_actualNetGametype",       "0",                CVAR_ARCHIVE                    },
    { &ui_redteam1,             "ui_redteam1",                "0",                CVAR_ARCHIVE                    },
    { &ui_redteam2,             "ui_redteam2",                "0",                CVAR_ARCHIVE                    },
    { &ui_redteam3,             "ui_redteam3",                "0",                CVAR_ARCHIVE                    },
    { &ui_redteam4,             "ui_redteam4",                "0",                CVAR_ARCHIVE                    },
    { &ui_redteam5,             "ui_redteam5",                "0",                CVAR_ARCHIVE                    },
    { &ui_blueteam1,            "ui_blueteam1",               "0",                CVAR_ARCHIVE                    },
    { &ui_blueteam2,            "ui_blueteam2",               "0",                CVAR_ARCHIVE                    },
    { &ui_blueteam3,            "ui_blueteam3",               "0",                CVAR_ARCHIVE                    },
    { &ui_blueteam4,            "ui_blueteam4",               "0",                CVAR_ARCHIVE                    },
    { &ui_blueteam5,            "ui_blueteam5",               "0",                CVAR_ARCHIVE                    },
    { &ui_netSource,            "ui_netSource",               "2",                CVAR_ARCHIVE                    },
    { &ui_menuFiles,            "ui_menuFiles",               "ui/menus.txt",     CVAR_ARCHIVE                    },
    { &ui_currentMap,           "ui_currentMap",              "0",                CVAR_ARCHIVE                    },
    { &ui_currentNetMap,        "ui_currentNetMap",           "0",                CVAR_ARCHIVE                    },
    { &ui_mapIndex,             "ui_mapIndex",                "0",                CVAR_ARCHIVE                    },
    { &ui_selectedPlayer,       "cg_selectedPlayer",          "0",                CVAR_ARCHIVE                    },
    { &ui_selectedPlayerName,   "cg_selectedPlayerName",      "",                 CVAR_ARCHIVE                    },
    { &ui_lastServerRefresh_0,  "ui_lastServerRefresh_0",     "",                 CVAR_ARCHIVE                    },
    { &ui_lastServerRefresh_1,  "ui_lastServerRefresh_1",     "",                 CVAR_ARCHIVE                    },
    { &ui_lastServerRefresh_2,  "ui_lastServerRefresh_2",     "",                 CVAR_ARCHIVE                    },
    { &ui_lastServerRefresh_3,  "ui_lastServerRefresh_3",     "",                 CVAR_ARCHIVE                    },
    { &ui_singlePlayerActive,   "ui_singlePlayerActive",      "0",                0},
    { &ui_scoreAccuracy,        "ui_scoreAccuracy",           "0",                CVAR_ARCHIVE                    },
    { &ui_scoreImpressives,     "ui_scoreImpressives",        "0",                CVAR_ARCHIVE                    },
    { &ui_scoreExcellents,      "ui_scoreExcellents",         "0",                CVAR_ARCHIVE                    },
    { &ui_scoreCaptures,        "ui_scoreCaptures",           "0",                CVAR_ARCHIVE                    },
    { &ui_scoreDefends,         "ui_scoreDefends",            "0",                CVAR_ARCHIVE                    },
    { &ui_scoreAssists,         "ui_scoreAssists",            "0",                CVAR_ARCHIVE                    },
    { &ui_scoreGauntlets,       "ui_scoreGauntlets",          "0",                CVAR_ARCHIVE                    },
    { &ui_scoreScore,           "ui_scoreScore",              "0",                CVAR_ARCHIVE                    },
    { &ui_scorePerfect,         "ui_scorePerfect",            "0",                CVAR_ARCHIVE                    },
    { &ui_scoreTeam,            "ui_scoreTeam",               "0 to 0",           CVAR_ARCHIVE                    },
    { &ui_scoreBase,            "ui_scoreBase",               "0",                CVAR_ARCHIVE                    },
    { &ui_scoreTime,            "ui_scoreTime",               "00:00",            CVAR_ARCHIVE                    },
    { &ui_scoreTimeBonus,       "ui_scoreTimeBonus",          "0",                CVAR_ARCHIVE                    },
    { &ui_scoreSkillBonus,      "ui_scoreSkillBonus",         "0",                CVAR_ARCHIVE                    },
    { &ui_scoreShutoutBonus,    "ui_scoreShutoutBonus",       "0",                CVAR_ARCHIVE                    },
    { &ui_fragLimit,            "ui_fragLimit",               "10",               0                               },
    { &ui_captureLimit,         "ui_captureLimit",            "5",                0                               },
    { &ui_smallFont,            "ui_smallFont",               "0.25",             CVAR_ARCHIVE                    },
    { &ui_bigFont,              "ui_bigFont",                 "0.4",              CVAR_ARCHIVE                    },
    { &ui_findPlayer,           "ui_findPlayer",              "Sarge",            CVAR_ARCHIVE                    },
    { &ui_Q3Model,              "ui_q3model",                 "0",                CVAR_ARCHIVE                    },
    { &ui_hudFiles,             "cg_hudFiles",                "ui/hud.txt",       CVAR_ARCHIVE                    },
    { &ui_recordSPDemo,         "ui_recordSPDemo",            "0",                CVAR_ARCHIVE                    },
    { &ui_teamArenaFirstRun,    "ui_teamArenaFirstRun",       "0",                CVAR_ARCHIVE                    },
    { &ui_realWarmUp,           "g_warmup",                   "20",               CVAR_ARCHIVE                    },
    { &ui_realCaptureLimit,     "capturelimit",               "8",                CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_NORESTART},
    { &ui_serverStatusTimeOut,  "ui_serverStatusTimeOut",     "7000",             CVAR_ARCHIVE                    },
    { &ui_gearItemCount,        "ui_gearItemCount",           "1",                0                               },

    //Iain:
    { &ui_currentmodel,         "ui_currentmodel",            "",                 CVAR_ARCHIVE                    },
    { &ui_ingameMaster,         "ui_ingameMaster",            "0",                CVAR_ARCHIVE                    },
    { &statsoutput0,            "statsoutput0",               "",                 CVAR_ROM                        },
    { &statsoutput1,            "statsoutput1",               "",                 CVAR_ROM                        },

    //@Barbatos - let the user know his modversion
    { &ui_modversion,           "ui_modversion",              GAME_VERSION,       CVAR_ROM                        },

    #ifdef USE_AUTH
    //@Barbatos //@Kalish
    { &authc,                   "authc",                      "0",                CVAR_ROM                        },
    { &cl_auth,                 "cl_auth",                    "0",                CVAR_ROM                        },
    { &cl_auth_enable,          "cl_auth_enable",             "1",                CVAR_ARCHIVE                    },
    { &cl_auth_login,           "cl_auth_login",              "0",                CVAR_ROM                        },
    { &cl_auth_notoriety,       "cl_auth_notoriety",          "0",                CVAR_ROM                        },
    { &cl_auth_key,             "cl_auth_key",                "0",                CVAR_ROM                        },
    { &cl_auth_engine,          "cl_auth_engine",             "0",                CVAR_ROM                        },
    { &cl_auth_status,          "cl_auth_status",             "",                 CVAR_ROM                        },
    { &cl_auth_time,            "cl_auth_time",               "0",                CVAR_ROM                        },
    { &ui_mainMenu,             "ui_mainMenu",                "0",                CVAR_ROM                        },
    #endif
};

int cvarTableSize = sizeof(cvarTable) / sizeof(cvarTable[0]);

/*
=================
UI_RegisterCvars
=================
*/
void UI_RegisterCvars( void )
{
	int 	 i;
	cvarTable_t  *cv;

	for (i = 0, cv = cvarTable ; i < cvarTableSize ; i++, cv++)
	{
		trap_Cvar_Register( cv->vmCvar, cv->cvarName, cv->defaultString, cv->cvarFlags );
	}

	//Iain: new addition

	//trap_Cvar_Set("sv_master1", "master.urbanterror.info");
	//trap_Cvar_Set("sv_master2", "master2.urbanterror.info");
//
//	tempf = trap_Cvar_VariableValue( "r_overbrightbits" );
//	if( tempf > 1.0f ) {
//		trap_Cvar_Set( "r_overbrightbits", "1" );
//	}
	//
	// DensitY 2.6RC1 request By BladeKiller: we'll leave this in
	// UPDATE: requested by QA to remove.. bastards!
	//
//	trap_Cvar_Set("r_overbrightbits", "0" );
	//
	//
}

/*
=================
UI_UpdateCvars
=================
*/
void UI_UpdateCvars( void )
{
	int 	 i;
	cvarTable_t  *cv;

	for (i = 0, cv = cvarTable ; i < cvarTableSize ; i++, cv++)
	{
		trap_Cvar_Update( cv->vmCvar );
	}
}

/*
=================
ArenaServers_StopRefresh
=================
*/
static void UI_StopServerRefresh( void )
{
	int  count;

	if (!uiInfo.serverStatus.refreshActive)
	{
		// not currently refreshing
		return;
	}
	uiInfo.serverStatus.refreshActive = qfalse;
	Com_Printf("%d servers listed in browser with %d players.\n",
		   uiInfo.serverStatus.numDisplayServers,
		   uiInfo.serverStatus.numPlayersOnServers);
	count = trap_LAN_GetServerCount(ui_netSource.integer);

	if (count - uiInfo.serverStatus.numDisplayServers > 0)
	{
		Com_Printf("%d servers not listed due to filters, packet loss or pings higher than %d\n",
			   count - uiInfo.serverStatus.numDisplayServers,
			   (int)trap_Cvar_VariableValue("cl_maxPing"));
	}
}

/*
=================
UI_DoServerRefresh
=================
*/
static void UI_DoServerRefresh( void )
{
	qboolean  wait = qfalse;

	if (!uiInfo.serverStatus.refreshActive)
	{
		return;
	}

	if (ui_netSource.integer != AS_FAVORITES)
	{
		if (ui_netSource.integer == AS_LOCAL)
		{
			if (!trap_LAN_GetServerCount(ui_netSource.integer))
			{
				wait = qtrue;
			}
		}
		else
		{
			if (trap_LAN_GetServerCount(ui_netSource.integer) < 0)
			{
				wait = qtrue;
			}
		}
	}

	if (uiInfo.uiDC.realTime < uiInfo.serverStatus.refreshtime)
	{
		if (wait)
		{
			return;
		}
	}

	// if still trying to retrieve pings
	if (trap_LAN_UpdateVisiblePings(ui_netSource.integer))
	{
		uiInfo.serverStatus.refreshtime = uiInfo.uiDC.realTime + 1000;
	}
	else if (!wait)
	{
		// get the last servers in the list
		UI_BuildServerDisplayList(2);
		// stop the refresh
		UI_StopServerRefresh();
	}
	//
	UI_BuildServerDisplayList(qfalse);
}

/*
=================
UI_StartServerRefresh
=================
*/
static void UI_StartServerRefresh(qboolean full)
{
	char	 *ptr;

	qtime_t  q;

	trap_RealTime(&q);
	trap_Cvar_Set( va("ui_lastServerRefresh_%i", ui_netSource.integer), va("%s-%i, %i at %i:%i", MonthAbbrev[q.tm_mon], q.tm_mday, 1900 + q.tm_year, q.tm_hour, q.tm_min));

	if (!full)
	{
		UI_UpdatePendingPings();
		return;
	}

	uiInfo.serverStatus.refreshActive	= qtrue;
	uiInfo.serverStatus.nextDisplayRefresh	= uiInfo.uiDC.realTime + 1000;
	// clear number of displayed servers
	uiInfo.serverStatus.numDisplayServers	= 0;
	uiInfo.serverStatus.numPlayersOnServers = 0;
	// mark all servers as visible so we store ping updates for them
	trap_LAN_MarkServerVisible(ui_netSource.integer, -1, qtrue);
	// reset all the pings
	trap_LAN_ResetPings(ui_netSource.integer);

	//
	if(ui_netSource.integer == AS_LOCAL)
	{
		trap_Cmd_ExecuteText( EXEC_NOW, "localservers\n" );
		uiInfo.serverStatus.refreshtime = uiInfo.uiDC.realTime + 1000;
		return;
	}

	uiInfo.serverStatus.refreshtime = uiInfo.uiDC.realTime + 5000;

	if((ui_netSource.integer == AS_GLOBAL) || (ui_netSource.integer == AS_MPLAYER))
	{
		ptr = UI_Cvar_VariableString("debug_protocol");

		if (strlen(ptr))
		{
			trap_Cmd_ExecuteText( EXEC_NOW, va( "globalservers %d %s full empty\n", ui_ingameMaster.integer, ptr));
		}
		else
		{
			trap_Cmd_ExecuteText( EXEC_NOW, va( "globalservers %d %d full empty\n", ui_ingameMaster.integer, (int)trap_Cvar_VariableValue( "protocol" )));
		}
	}
}

/**
 * BuildMiniMapShaders
 */
void BuildMiniMapShaders(void)
{
	char		  tganame[MAX_QPATH];
	char		  mapname[MAX_QPATH];
	char		  searchMap[10 * 255];

	char		  filename[MAX_QPATH] = { "scripts/allbsps.shader"};
	int 	  foot, j;
	fileHandle_t  ShaderFile;
	char		  contents[1024];
	int 	  searchCount;
	int 	  startofcopy;
	int 	  endofcopy;

	searchCount = trap_FS_GetFileList("maps", "*.bsp", searchMap, 10 * 255);

	trap_FS_FOpenFile( filename, &ShaderFile, FS_READ );

	if (ShaderFile != 0)
	{
		//If the file exists we assume its right.
		trap_FS_FCloseFile(ShaderFile);
		return;
	}

	trap_FS_FOpenFile( filename, &ShaderFile, FS_WRITE );

	Com_sprintf(contents, 255, "// AUTOGENERATED BY BuildMiniMapShaders\n\n\n");
	trap_FS_Write( contents, strlen(contents), ShaderFile);

	startofcopy = 0;
	endofcopy	= 0;

	for (j = 0; j < searchCount; j++)
	{
		//tokenize level names
		while (searchMap[endofcopy] != 0)
		{
			endofcopy++;
		}
		memcpy(mapname, &searchMap[startofcopy], (endofcopy - startofcopy) + 1);
		endofcopy++;
		startofcopy = endofcopy;

		strcpy(tganame, mapname);
		foot		  = strlen(tganame);
		tganame[foot - 3] = 't';
		tganame[foot - 2] = 'g';
		tganame[foot - 1] = 'a';

		Com_sprintf(contents, 1024, "%s\n{\nnopicmip\ncullnone\n{\nmap maps/%s\nblendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA\nalphaGen vertex\n}\n}\n\n\n", tganame, tganame);

		trap_FS_Write(	contents, strlen(contents), ShaderFile);
		trap_Print( va( "%s MiniMap shader written.\n", tganame ));
	}

	trap_FS_FCloseFile(ShaderFile);
}

/**
 * statsTick
 */
void statsTick(void)
{
	int  time = uiInfo.uiDC.realTime;

	if (strlen(uiInfo.StatsCommand) > 0 && trap_Cvar_VariableValue("cl_paused")==0)
	{
		if (time > uiInfo.safeUpdate)
		{
			uiInfo.safeUpdate = time + 1100;
			trap_Cmd_ExecuteText( EXEC_APPEND, uiInfo.StatsCommand);
		}
	}
}

/**
 * getStats
 */
void getStats(void)
{
	if (trap_Cvar_VariableValue("cl_paused")==0)
	{
		trap_Cvar_Set("statsoutput0", "Loading..\n");
		trap_Cvar_Set("statsoutput1", "");
		strcpy(uiInfo.StatsCommand, va("stats %i %s\n", uiInfo.playerClientNum[uiInfo.statsplayerIndex], STATS_CATAGORYS[uiInfo.statsweaponsIndex]));
	}
	else
	{
		Com_Printf("Stats cannot be retrieved while client is paused.\n");
		trap_Cvar_Set("statsoutput0","\nStats cannot be retrieved while client is paused.\n");
	}
}

//splits up statsoutput into usable strings
void parseStats(void)
{
	int   j;
	int   index = 0;
	int   start;
	char  buffer[2048];
	char  buffer2[256];

	int   final;

	trap_Cvar_VariableStringBuffer("statsoutput0", &buffer[0], 2048);
	trap_Cvar_VariableStringBuffer("statsoutput1", &buffer2[0], 256);
	Q_strcat(buffer, 2048, buffer2);

	for (j = 0; j < MAX_STATS_TEXT - 1; j++)
	{
		STATS_TEXTLINES[j][0] = 0;
	}

	final = strlen(buffer) + 1;
	start = 0;

	for (j = 0; j < final; j++)
	{
		if ((buffer[j] == '\n') || (j == final - 1))
		{
			memcpy(&STATS_TEXTLINES[index], &buffer[start], j - start);
			STATS_TEXTLINES[index][(j - start)] = 0;
			start					= j + 1;
			index++;
		}
	}
}
