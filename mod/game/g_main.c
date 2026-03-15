// Copyright (C) 1999-2000 Id Software, Inc.
//

#include "../common/c_cvars.h"
#include "../common/c_bmalloc.h"
#include "g_aries.h"
#include "g_local.h"

extern int		   TSnumGroups;

level_locals_t	   level;

int 			   num_levelspawn;
int 			   cur_levelspawn;		// same as levelspawn on loadup

typedef struct {
	vmCvar_t  *vmCvar;
	char	  *cvarName;
	char	  *defaultString;
	int 	  cvarFlags;
	int 	  modificationCount;		// for tracking changes
	qboolean  trackChange;				// track this variable, and announce if changed
	qboolean  teamShader;				// track and if changed, update shader state
} cvarTable_t;

#ifdef USE_AUTH

//@Barbatos //@Kalish

int         auth_mode;
char        auth_header[512];
char        auth_heartbeat[MAX_INFO_STRING];
char        auth_players[MAX_INFO_STRING];

vmCvar_t    auth;
vmCvar_t    auth_status;
vmCvar_t    auth_enable;
vmCvar_t    auth_verbosity;
vmCvar_t    auth_cheaters;
vmCvar_t    auth_tags;
vmCvar_t    auth_notoriety;
vmCvar_t    auth_groups;
vmCvar_t    auth_owners;

/*
vmCvar_t    auth_nicknames;
vmCvar_t    auth_cmds_authed;
vmCvar_t    auth_cmds_anonymous;
vmCvar_t    auth_cmds_friend;
vmCvar_t    auth_cmds_member;
vmCvar_t    auth_cmds_referee;
*/

// FIXME: using the initializer squashes gameCvarTable memory
netadr_t    auth_server_address; // = { 0, { 0, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, { 0, 0 }, 0 };

#endif


// reserve 16MB memory for bmalloc
#define MALLOC_POOL_SIZE (1024*1024*32)
char g_malloc_pool[MALLOC_POOL_SIZE];

gentity_t  g_entities[MAX_GENTITIES];

gclient_t  g_clients[MAX_CLIENTS];

vmCvar_t   g_gametype;
vmCvar_t   g_dmflags;
vmCvar_t   g_fraglimit;
vmCvar_t   g_timelimit;
vmCvar_t   g_capturelimit;
vmCvar_t   g_friendlyFire;
vmCvar_t   g_password;
vmCvar_t   g_needpass;
vmCvar_t   g_maxclients;
vmCvar_t   g_maxGameClients;
vmCvar_t   g_dedicated;
vmCvar_t   g_dedAutoChat;
vmCvar_t   g_speed;
vmCvar_t   g_gravity;
vmCvar_t   g_cheats;
vmCvar_t   g_knockback;
vmCvar_t   g_forcerespawn;
vmCvar_t   g_inactivity;
vmCvar_t   g_debugMove;
vmCvar_t   g_motd;
vmCvar_t   g_synchronousClients;
vmCvar_t   g_warmup;
vmCvar_t   g_restarted;
vmCvar_t   g_log;
vmCvar_t   g_logroll;
vmCvar_t   g_logSync;
vmCvar_t   g_blood;
vmCvar_t   g_podiumDist;
vmCvar_t   g_podiumDrop;
vmCvar_t   g_allowVote;
vmCvar_t   g_teamAutoJoin;
vmCvar_t   g_teamForceBalance;
//vmCvar_t	g_banIPs;
vmCvar_t   g_filterBan;
vmCvar_t	g_smoothClients;
vmCvar_t   g_antilagvis;
//vmCvar_t	 g_noSpread;

//vmCvar_t	pmove_fixed;
//vmCvar_t	pmove_msec;
vmCvar_t  g_listEntity;
vmCvar_t  g_singlePlayer;
vmCvar_t  g_enableDust;
vmCvar_t  g_enableBreath;
vmCvar_t  g_loghits;

// Urban terror
vmCvar_t  g_survivor;
vmCvar_t  g_mapCycle;
vmCvar_t  g_cahTime;
vmCvar_t  g_RoundTime;
vmCvar_t  g_enablePrecip;
//vmCvar_t	g_precipAmount;
vmCvar_t  g_respawnDelay;
//vmCvar_t	g_surfacedefault;
//vmCvar_t	g_skins;
vmCvar_t  g_gear;
vmCvar_t  g_hotpotato;
vmCvar_t  g_armbands;
vmCvar_t  g_NextMap;

vmCvar_t  g_survivorrule;
vmCvar_t  g_suddendeath;

/*
vmCvar_t	g_allowBulletPrediction;
*/

vmCvar_t  g_allowChat;
vmCvar_t  g_refPass;
vmCvar_t  g_refClient;
vmCvar_t  g_referee;
vmCvar_t  g_refNoBan;
vmCvar_t  g_respawnProtection;
//vmCvar_t	g_followEnemy;
//vmCvar_t	g_followForced;
vmCvar_t  g_followStrict;
vmCvar_t  g_maxteamkills;
vmCvar_t  g_teamkillsForgetTime;
vmCvar_t  g_removeBodyTime;
vmCvar_t  g_bulletPredictionThreshold;
vmCvar_t  g_maintainTeam;
vmCvar_t  g_failedVoteTime;
vmCvar_t  g_newMapVoteTime;

vmCvar_t  g_fps;
vmCvar_t  g_flagReturnTime;

vmCvar_t  g_ClientReconnectMin;
// MOTD
vmCvar_t  g_JoinMessage;

// For BOMB/DEFUSE mode
vmCvar_t  g_bombExplodeTime;
vmCvar_t  g_bombDefuseTime;
vmCvar_t  g_bombPlantTime;

vmCvar_t  g_matchMode;
vmCvar_t  g_healthReport;

vmCvar_t  g_timeouts;
vmCvar_t  g_timeoutLength;

vmCvar_t  g_nameRed;
vmCvar_t  g_nameBlue;

vmCvar_t  g_antiwarp;
vmCvar_t  g_antiwarptol;

vmCvar_t  g_waveRespawns;
vmCvar_t  g_redWave;
vmCvar_t  g_blueWave;

vmCvar_t  g_pauseLength;

//vmCvar_t g_bombRules;
vmCvar_t  g_maxrounds;
vmCvar_t  g_swaproles;
vmCvar_t  g_deadchat;
vmCvar_t  g_ctfUnsubWait;

//Jump mode cvars
vmCvar_t  g_walljumps;
vmCvar_t  g_stamina;
vmCvar_t  g_noDamage;
vmCvar_t  g_allowPosSaving;
vmCvar_t  g_persistentPositions;
vmCvar_t  g_allowGoto;
vmCvar_t  g_jumpruns;

vmCvar_t  g_skins;
vmCvar_t  g_funstuff;
vmCvar_t  g_inactivityAction;
vmCvar_t  g_shuffleNoRestart;

/* New in 4.3 */
vmCvar_t  g_instagib;
vmCvar_t  g_hardcore;
vmCvar_t  g_randomorder;
vmCvar_t  g_thawTime;
vmCvar_t  g_meltdownTime;
vmCvar_t  g_stratTime;
vmCvar_t  g_autobalance;
vmCvar_t  g_vest;
vmCvar_t  g_helmet;
vmCvar_t  g_noVest;
vmCvar_t  g_modversion;
vmCvar_t  g_nextCycleMap;
vmCvar_t  g_nextMap;
vmCvar_t  g_refNoExec;
vmCvar_t  g_teamScores;
vmCvar_t  g_spSkill;
vmCvar_t  g_mrsentry;

cvarTable_t  gameCvarTable[] = {
    // don't override the cheat state set by the system

    { &g_cheats,                   "sv_cheats",                   "",                     0,                                                  0, qfalse    },

    // noset vars
    { &g_fps,                      "sv_fps",                      "128",                   0,                                                  0, qfalse    },
    { NULL,                        "gameid",                      "20000",                CVAR_ROM,                                           0, qfalse    },

    { NULL,                        "gamename",                    GAME_NAME,              CVAR_SERVERINFO | CVAR_ROM,                         0, qfalse    },
    { NULL,                        "gamedate",                    __DATE__,               CVAR_ROM,                                           0, qfalse    },
    { &g_restarted,                "g_restarted",                 "0",                    CVAR_ROM,                                           0, qfalse    },
    { NULL,                        "sv_mapname",                  "",                     CVAR_SERVERINFO | CVAR_ROM,                         0, qfalse    },

    // latched vars
    { &g_gametype,                 "g_gametype",                  "8",                    CVAR_SERVERINFO | CVAR_USERINFO | CVAR_LATCH,       0, qfalse    },
    { &g_maxclients,               "sv_maxclients",               "12",                   CVAR_SERVERINFO | CVAR_LATCH | CVAR_ARCHIVE,        0, qfalse    },
    { &g_maxGameClients,           "g_maxGameClients",            "0",                    CVAR_SERVERINFO | CVAR_LATCH | CVAR_ARCHIVE,        0, qfalse    },

    // change anytime vars
    { &g_dmflags,                  "dmflags",                     "0",                    CVAR_SERVERINFO | CVAR_ARCHIVE,                     0, qtrue     },
    { &g_fraglimit,                "fraglimit",                   "0",                    CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_NORESTART,    0, qtrue     },
    { &g_timelimit,                "timelimit",                   "20",                   CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_NORESTART,    0, qtrue     },
    { &g_capturelimit,             "capturelimit",                "0",                    CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_NORESTART,    0, qtrue     },

    { &g_synchronousClients,       "g_synchronousClients",        "0",                    CVAR_SYSTEMINFO,                                    0, qfalse    },
    { &g_friendlyFire,             "g_friendlyFire",              "0",                    CVAR_ARCHIVE | CVAR_SERVERINFO,                     0, qtrue     },
    { &g_teamAutoJoin,             "g_teamAutoJoin",              "0",                    CVAR_ARCHIVE                                                     },
    { &g_teamForceBalance,         "g_teamForceBalance",          "0",                    CVAR_ARCHIVE                                                     },
    { &g_warmup,                   "g_warmup",                    "5",                    CVAR_ARCHIVE,                                       0, qtrue     },

    { &g_logroll,                  "g_logroll",                   "0",                    CVAR_ARCHIVE,                                       0, qfalse    },
    { &g_log,                      "g_log",                       "games.log",            CVAR_ARCHIVE,                                       0, qfalse    },
    { &g_loghits,                  "g_loghits",                   "0",                    CVAR_ARCHIVE,                                       0, qfalse    },
    { &g_logSync,                  "g_logSync",                   "0",                    CVAR_ARCHIVE,                                       0, qfalse    },

    { &g_password,                 "g_password",                  "",                     CVAR_USERINFO,                                      0, qfalse    },

//  { &g_banIPs,                   "g_banIPs",                    "",                     CVAR_ARCHIVE,                                       0, qfalse    },
    { &g_filterBan,                "g_filterBan",                 "1",                    CVAR_ARCHIVE,                                       0, qfalse    },

    { &g_needpass,                 "g_needpass",                  "0",                    CVAR_SERVERINFO | CVAR_ROM,                         0, qfalse    },

    { &g_dedicated,                "dedicated",                   "0",                    0,                                                  0, qfalse    },
    { &g_dedAutoChat,              "g_dedAutoChat",               "1",                    CVAR_ARCHIVE,                                       0, qfalse    },

    { &g_speed,                    "g_speed",                     "240",                  0,                                                  0, qtrue     },
    { &g_gravity,                  "g_gravity",                   "800",                  0,                                                  0, qtrue     },
    { &g_knockback,                "g_knockback",                 "1000",                 0,                                                  0, qtrue     },
    { &g_forcerespawn,             "g_forcerespawn",              "20",                   0,                                                  0, qtrue     },
    { &g_inactivity,               "g_inactivity",                "0",                                        0,                              0, qtrue     },
    { &g_debugMove,                "g_debugMove",                 "0",                                        0,                              0, qfalse    },
    { &g_motd,                     "g_motd",                      "Urban Terror, Presented by FrozenSand",    0,                              0, qfalse    },

    { &g_blood,                    "com_blood",                   "1",                    0,                                                  0, qfalse    },

    { &g_podiumDist,               "g_podiumDist",                "80",                   0,                                                  0, qfalse    },
    { &g_podiumDrop,               "g_podiumDrop",                "70",                   0,                                                  0, qfalse    },

    { &g_allowVote,                "g_allowVote",                 "1",                    CVAR_ARCHIVE | CVAR_SERVERINFO,                     0, qfalse    },
    { &g_listEntity,               "g_listEntity",                "0",                    0,                                                  0, qfalse    },

    { &g_singlePlayer,             "ui_singlePlayerActive",       "",                     0,                                                  0, qfalse, qfalse },

    { &g_enableDust,               "g_enableDust",                "0",                    CVAR_SERVERINFO,                                    0, qtrue, qfalse  },
    { &g_enableBreath,             "g_enableBreath",              "0",                    CVAR_SERVERINFO,                                    0, qtrue, qfalse  },

    { &g_antilagvis,               "g_antilagvis",                "0",                    CVAR_CHEAT | CVAR_SERVERINFO,                       0, qfalse   },
//  { &g_noSpread,                 "g_noSpread",                  "0",                    CVAR_CHEAT | CVAR_SERVERINFO,                       0, qfalse   },
//  { &pmove_fixed,                "pmove_fixed",                 "0",                    CVAR_SYSTEMINFO,                                    0, qfalse   },
//  { &pmove_msec,                 "pmove_msec",                  "8",                    CVAR_SYSTEMINFO,                                    0, qfalse   },

    // Urban terror

    { &g_survivor,                 "g_survivor",                  "1",                    CVAR_SERVERINFO | CVAR_ROM,                         0, qfalse   },
    { &g_cahTime,                  "g_cahTime",                   "60",                   CVAR_ARCHIVE,                                       0, qfalse   }, // CVAR_LATCH
    { &g_RoundTime,                "g_RoundTime",                 "3",                    CVAR_ARCHIVE | CVAR_SERVERINFO,                     0, qtrue    }, // CVAR_LATCH
    { &g_enablePrecip,             "g_enablePrecip",              "0",                    CVAR_SERVERINFO,                                    0, qfalse   },
//  { &g_precipAmount,             "g_precipAmount",              "0",                    CVAR_SERVERINFO,                                    0, qfalse   },
    { &g_respawnDelay,             "g_respawnDelay",              "10",                   CVAR_ARCHIVE | CVAR_SERVERINFO,                     0, qtrue    }, // CVAR_LATCH
//  { &g_surfacedefault,           "g_surfacedefault",            "0",                    CVAR_SERVERINFO | CVAR_ROM,                         0, qfalse   },
//  { &g_skins,                    "g_skins",                     "0",                    CVAR_SERVERINFO | CVAR_ROM,                         0, qfalse   },
    { &g_gear,                     "g_gear",                      "0",                    CVAR_ARCHIVE | CVAR_SERVERINFO,                     0, qtrue    },
    { &g_survivorrule,             "g_survivorrule",              "0",                    CVAR_ARCHIVE,                                       0, qfalse   },
    { &g_suddendeath,              "g_suddendeath",               "1",                    CVAR_ARCHIVE | CVAR_SERVERINFO,                     0, qfalse   },

//  { &g_allowBulletPrediction,    "g_allowBulletPrediction",     "1",                    CVAR_ARCHIVE | CVAR_SERVERINFO,                     0, qfalse   },

    { &g_mapCycle,                 "g_mapCycle",                  "mapcycle.txt",         CVAR_ARCHIVE,                                       0, qfalse   },
    { &g_allowChat,                "g_allowChat",                 "2",                    CVAR_ARCHIVE,                                       0, qfalse   },

    { &g_refPass,                  "g_refPass",                   "",                     CVAR_ARCHIVE,                                       0, qfalse   },
    { &g_refClient,                "g_refClient",                 "-1",                   CVAR_ROM | CVAR_ARCHIVE,                            0, qfalse   },
    { &g_referee,                  "g_referee",                   "1",                    CVAR_ARCHIVE,                                       0, qfalse   },
    { &g_refNoBan,                 "g_refNoBan",                  "",                     CVAR_ARCHIVE,                                       0, qfalse   },

    { &g_respawnProtection,        "g_respawnProtection",         "2",                    CVAR_ARCHIVE,                                       0, qfalse   },
//  { &g_followEnemy,              "g_followEnemy",               "1",                    CVAR_ARCHIVE | CVAR_SERVERINFO,                     0, qfalse   }, //27 modified so that it's published to the clients
//  { &g_followForced,             "g_followForced",              "0",                    CVAR_ARCHIVE,                                       0, qfalse   },
    { &g_followStrict,             "g_followStrict",              "0",                    CVAR_ARCHIVE | CVAR_SERVERINFO,                     0, qtrue    },

    { &g_maxteamkills,             "g_maxteamkills",              "3",                    CVAR_ARCHIVE,                                       0, qfalse   },
    { &g_teamkillsForgetTime,      "g_teamkillsForgetTime",       "120",                  CVAR_ARCHIVE,                                       0, qfalse   },
    { &g_removeBodyTime,           "g_removeBodyTime",            "300",                  CVAR_ARCHIVE,                                       0, qfalse   },
    { &g_bulletPredictionThreshold,"g_bulletPredictionThreshold", "5",                    CVAR_ARCHIVE,                                       0, qfalse   },

    { &g_maintainTeam,             "g_maintainTeam",              "1",                    CVAR_ARCHIVE,                                       0, qfalse   },
    { &g_failedVoteTime,           "g_failedVoteTime",            "300",                  CVAR_ARCHIVE,                                       0, qfalse   }, // As per Oswald's request, this has been increased was 120!
    { &g_newMapVoteTime,           "g_newMapVoteTime",            "30",                   CVAR_ARCHIVE,                                       0, qfalse   },

    { &g_flagReturnTime,           "g_flagReturnTime",            "30",                   CVAR_ARCHIVE,                                       0, qtrue    },

    { &g_ClientReconnectMin,       "g_ClientReconnectMin",        "100",                  CVAR_ARCHIVE,                                       0, qfalse   },

    { &g_JoinMessage,              "sv_JoinMessage",              "Welcome To Urban Terror 4.3",        CVAR_ARCHIVE,                         0, qtrue    },

    { &g_bombExplodeTime,          "g_bombExplodeTime",           "40",                   CVAR_ARCHIVE | CVAR_SERVERINFO,                     0, qtrue    }, // CVAR_LATCH
    { &g_bombDefuseTime,           "g_bombDefuseTime",            "10",                   CVAR_ARCHIVE | CVAR_SERVERINFO,                     0, qtrue    }, // CVAR_LATCH
    { &g_bombPlantTime,            "g_bombPlantTime",             "5",                    CVAR_LATCH,                                         0, qtrue    }, // CVAR_LATCH

    { &g_antiwarp,                 "g_antiwarp",                  "0",                    CVAR_ARCHIVE,                                       0, qtrue    },
    { &g_antiwarptol,              "g_antiwarptol",               "50",                   CVAR_ARCHIVE,                                       0, qtrue    },

    { &g_matchMode,                "g_matchMode",                 "0",                    CVAR_ARCHIVE | CVAR_SERVERINFO | CVAR_LATCH,        0, qtrue    },
    { &g_healthReport,             "g_healthReport",              "1",                    CVAR_ARCHIVE,                                       0, qfalse   },

    { &g_nameRed,              "g_nameRed",               "",                     CVAR_ARCHIVE | CVAR_SERVERINFO,                     0, qtrue    },
    { &g_nameBlue,             "g_nameBlue",              "",                     CVAR_ARCHIVE | CVAR_SERVERINFO,                     0, qtrue    },

    { &g_timeouts,                 "g_timeouts",                  "3",                    CVAR_ARCHIVE,                                       0, qtrue    }, // CVAR_LATCH
    { &g_timeoutLength,            "g_timeoutLength",             "30",                   CVAR_ARCHIVE,                                       0, qtrue    }, // CVAR_LATCH

    { &g_waveRespawns,             "g_waveRespawns",              "1",                    CVAR_ARCHIVE | CVAR_SERVERINFO,                     0, qtrue    }, // CVAR_LATCH
    { &g_redWave,                  "g_redWave",                   "15",                   CVAR_ARCHIVE | CVAR_SERVERINFO,                     0, qtrue    }, // CVAR_LATCH
    { &g_blueWave,                 "g_blueWave",                  "15",                   CVAR_ARCHIVE | CVAR_SERVERINFO,                     0, qtrue    }, // CVAR_LATCH

    { &g_pauseLength,              "g_pauseLength",               "0",                    CVAR_ARCHIVE,                                       0, qfalse   }, // CVAR_LATCH
    { &g_maxrounds,                "g_maxrounds",                 "0",                    CVAR_SERVERINFO | CVAR_ARCHIVE,                     0, qtrue    },
    { &g_swaproles,                "g_swaproles",                 "0",                    CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_LATCH,        0, qtrue    },
    { &g_deadchat,                 "g_deadchat",                  "2",                    CVAR_SERVERINFO | CVAR_ARCHIVE,                     0, qtrue    },
    { &g_hotpotato,                "g_hotpotato",                 "2",                    CVAR_ARCHIVE | CVAR_SERVERINFO,                     0, qtrue    },
    { &g_armbands,                 "g_armbands",                  "0",                    CVAR_ARCHIVE | CVAR_SERVERINFO,                     0, qtrue    }, // CVAR_LATCH
    { &g_NextMap,                  "g_NextMap",                   "",                     CVAR_ARCHIVE | CVAR_SERVERINFO,                     0, qtrue    }, // CVAR_LATCH
    { &g_ctfUnsubWait,             "g_ctfUnsubWait",              "0",                    CVAR_ARCHIVE,                                       0, qtrue    }, // CVAR_LATCH
//  { &g_bombRules,                "g_bombRules",                 "0",                    CVAR_ARCHIVE,                                       0, qfalse   },

    { &g_walljumps,                "g_walljumps",                 "3",                    CVAR_SERVERINFO | CVAR_ARCHIVE,                     0, qtrue    },
    { &g_noDamage,                 "g_noDamage",                  "0",                    CVAR_ARCHIVE,                                       0, qtrue    },
    { &g_stamina,                  "g_stamina",                   "0",                    CVAR_SERVERINFO | CVAR_ARCHIVE,                     0, qtrue    },
    { &g_allowPosSaving,           "g_allowPosSaving",            "0",                    CVAR_ARCHIVE,                                       0, qtrue    },
    { &g_persistentPositions,      "g_persistentPositions",       "0",                    CVAR_ARCHIVE,                                       0, qtrue    },
    { &g_allowGoto,                "g_allowGoto",                 "0",                    CVAR_ARCHIVE,                                       0, qtrue    },
    { &g_jumpruns,                 "g_jumpruns",                  "0",                    CVAR_ARCHIVE,                                       0, qtrue    },

    { &g_skins,                    "g_skins",                     "1",                    CVAR_ARCHIVE | CVAR_SERVERINFO,                     0, qtrue    },
    { &g_funstuff,                 "g_funstuff",                  "1",                    CVAR_ARCHIVE | CVAR_SERVERINFO,                     0, qtrue    },

    { &g_inactivityAction,		   "g_inactivityAction",		  "0",					  CVAR_ARCHIVE,										  0, qfalse	  },
    { &g_shuffleNoRestart,		   "g_shuffleNoRestart",		  "0",					  CVAR_ARCHIVE,										  0, qfalse	  },
    /* New in 4.3 */
    { &g_instagib,                 "g_instagib",                  "0",                    CVAR_ARCHIVE | CVAR_SERVERINFO,                     0, qtrue    },
    { &g_hardcore,                 "g_hardcore",                  "0",                    CVAR_ARCHIVE | CVAR_SERVERINFO,                     0, qtrue    },
    { &g_randomorder,              "g_randomorder",               "0",                    CVAR_ARCHIVE,                                       0, qtrue    },
    { &g_thawTime,                 "g_thawTime",                  "3",                    CVAR_ARCHIVE | CVAR_SERVERINFO,                     0, qtrue    },
    { &g_meltdownTime,             "g_meltdownTime",              "10",                   CVAR_ARCHIVE | CVAR_SERVERINFO,                     0, qtrue    },
    { &g_stratTime,                "g_stratTime",                 "0",                    CVAR_ARCHIVE,                                       0, qfalse   },
    { &g_autobalance,              "g_autobalance",               "0",                    CVAR_ARCHIVE,                                       0, qfalse   },
    { &g_vest,                     "g_vest",                      "1",                    CVAR_ARCHIVE | CVAR_SERVERINFO,                     0, qtrue    },
    { &g_helmet,                   "g_helmet",                    "1",                    CVAR_ARCHIVE | CVAR_SERVERINFO,                     0, qtrue    },
    { &g_noVest,                   "g_noVest",                    "0",                    CVAR_ARCHIVE,                                       0, qfalse   },
    { &g_nameRed,                  "g_nameRed",                   "Red",                  CVAR_ARCHIVE | CVAR_SERVERINFO,                     0, qfalse   },
    { &g_nameBlue,                 "g_nameBlue",                  "Blue",                 CVAR_ARCHIVE | CVAR_SERVERINFO,                     0, qfalse   },
    { &g_modversion,               "g_modversion",                "4.3.4",                CVAR_ROM | CVAR_SERVERINFO,                         0, qfalse   },
    { &g_nextCycleMap,             "g_nextCycleMap",              "",                     CVAR_ARCHIVE,                                       0, qfalse   },
    { &g_nextMap,                  "g_nextMap",                   "",                     CVAR_ARCHIVE,                                       0, qfalse   },
    { &g_refNoExec,                "g_refNoExec",                 "0",                    CVAR_ARCHIVE,                                       0, qfalse   },
    { &g_teamScores,               "g_teamScores",                "0",                    CVAR_SERVERINFO | CVAR_ROM,                         0, qfalse   },
    { &g_spSkill,                  "g_spSkill",                   "2",                    CVAR_ARCHIVE,                                       0, qfalse   },
    { &g_mrsentry,                 "ut_mrsentry",                 "0",                    CVAR_ARCHIVE | CVAR_SERVERINFO,                     0, qfalse   },

    #ifdef USE_AUTH

    //@Barbatos //@Kalish

    { &auth,                       "auth",                        "0",                    CVAR_SERVERINFO | CVAR_ROM,                         0, qtrue    },
    { &auth_status,                "auth_status",                 "init",                 CVAR_SERVERINFO | CVAR_ROM,                         0, qtrue    },
    { &auth_enable,                "auth_enable",                 "1",                    CVAR_ARCHIVE,                                       0, qtrue    },
    { &auth_cheaters,              "auth_cheaters",               "1",                    CVAR_ARCHIVE,                                       0, qtrue    },
    { &auth_tags,                  "auth_tags",                   "1",                    CVAR_ARCHIVE,                                       0, qtrue    },
    { &auth_notoriety,             "auth_notoriety",              "0",                    CVAR_ARCHIVE,                                       0, qtrue    },
    { &auth_groups,                "auth_groups",                 "",                     CVAR_ARCHIVE,                                       0, qtrue    },
    { &auth_owners,                "auth_owners",                 "",                     CVAR_ARCHIVE,                                       0, qtrue    },
    { &auth_verbosity,             "auth_verbosity",              "1",                    CVAR_ARCHIVE,                                       0, qtrue    },

    /*
    { &auth_nicknames,             "auth_nicknames",              "0",                    CVAR_ARCHIVE,                                       0, qtrue    },
    { &auth_cmds_anonymous,        "auth_cmds_anonymous",         AUTH_CMD_SV),            CVAR_ARCHIVE,                                       0, qtrue    },
    { &auth_cmds_authed,           "auth_cmds_authed",            AUTH_CMD_SV " " AUTH_CMD_PRE "status"),    CVAR_ARCHIVE,                     0, qtrue    },
    { &auth_cmds_friend,           "auth_cmds_friend",            AUTH_CMD_PRE "whois " AUTH_CMD_PRE "kick restart nextmap"), CVAR_ARCHIVE,    0, qtrue    },
    { &auth_cmds_member,           "auth_cmds_member",            AUTH_CMD_PRE "ban map"),         CVAR_ARCHIVE,                               0, qtrue    },
    { &auth_cmds_referee,          "auth_cmds_referee",           AUTH_CMD_PRE "say devmap exec bigtext"), CVAR_ARCHIVE,                       0, qtrue    },
    */

    #endif
};

int gameCvarTableSize = sizeof(gameCvarTable) / sizeof(gameCvarTable[0]);

void G_InitGame( int levelTime, int randomSeed, int restart );
void G_RunFrame( int levelTime );
void G_ShutdownGame( int restart );
void CheckExitRules( void );

/*
================
vmMain

This is the only way control passes into the module.
This must be the very first function compiled into the .q3vm file
================
*/

DLLEXPORT int vmMain( int command, int arg0, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8, int arg9, int arg10, int arg11  )
{
	switch (command)
	{
		case GAME_INIT:
			G_InitGame( arg0, arg1, arg2 );
			return 0;

		case GAME_SHUTDOWN:
			G_ShutdownGame( arg0 );
			return 0;

		case GAME_CLIENT_CONNECT:
			return (intptr_t)ClientConnect( arg0, arg1, arg2 );

		case GAME_CLIENT_THINK:
			ClientThink( arg0 );
			return 0;

		case GAME_CLIENT_USERINFO_CHANGED:
			ClientUserinfoChanged( arg0 );
			return 0;

		case GAME_CLIENT_DISCONNECT:
			ClientDisconnect( arg0 );
			return 0;

		case GAME_CLIENT_BEGIN:
			ClientBegin( arg0 );
			return 0;

		case GAME_CLIENT_COMMAND:
			ClientCommand( arg0 );
			return 0;

		case GAME_RUN_FRAME:
			G_RunFrame( arg0 );
			return 0;

		case GAME_CONSOLE_COMMAND:
			return ConsoleCommand();

		case BOTAI_START_FRAME:
			return BotAIStartFrame( arg0 );

#ifdef USE_AUTH
		//@Barbatos @Kalish

		case GAME_AUTHSERVER_HEARTBEAT:
			SV_Authserver_Heartbeat();
			return 0;

		case GAME_AUTHSERVER_SHUTDOWN:
			SV_Authserver_Shutdown();
			return 0;

		case GAME_AUTHSERVER_PACKET:
			SV_Authserver_Get_Packet( );
			return 0;

		case GAME_AUTH_WHOIS:
			SV_Authserver_Whois( arg0 );
			return 0;

		case GAME_AUTH_BAN:
			SV_Authserver_Ban( arg0, arg1, arg2, arg3 );
			return 0;

#endif

	}

	return -1;
}



#ifdef USE_AUTH
//@Barbatos //@Kalish

/*
===============
SV_Auth_Init
===============
*/
void SV_Auth_Init (void)
{
	char game[16];
	int test;
	int netenabled;

	// auth_mode & auth pubic cvar
	if( auth_enable.integer != 1 )
	{
		auth_mode = 0;
		trap_Cvar_Set( "auth", "0" );
		trap_Cvar_Set( "auth_status", "off" );
		return;
	}
	else if( trap_Cvar_VariableIntegerValue("sv_auth_engine") != 1)
	{
		auth_mode = 0;
		trap_Cvar_Set( "auth", "0" );
		trap_Cvar_Set( "auth_status", "off (bad engine)" );
		return;
	}
	else if( g_dedicated.integer != 2 )
	{
		auth_mode = 0;
		trap_Cvar_Set( "auth", "0" );
		trap_Cvar_Set( "auth_status", "off (local)" );
		return;
	}
	else if( auth_cheaters.integer != 1
	&& auth_tags.integer != 1
	&& auth_verbosity.integer < 1
	&& auth_notoriety.integer < 1
	&& strlen(auth_groups.string) < 1
	&& strlen(auth_owners.string) < 1
	)
	{
		auth_mode = 0;
		trap_Cvar_Set( "auth", "0" );
		trap_Cvar_Set( "auth_status", "off (no cvar)" );
		return;
	}
	else if( strlen(auth_groups.string) > 0 )
	{
		auth_mode = 2;
		trap_Cvar_Set( "auth", "2" );
		trap_Cvar_Set( "auth_status", "private" );
		trap_Cvar_Set( "auth_notoriety", "0" );
	}
	else if( auth_notoriety.integer > 0 )
	{
		auth_mode = 3;
		trap_Cvar_Set( "auth", va("%i", -auth_notoriety.integer) );
		trap_Cvar_Set( "auth_status", "notoriety" );
	}
	else
	{
		auth_mode = 1;
		trap_Cvar_Set( "auth", "1" );
		trap_Cvar_Set( "auth_status", "public" );
	}

	// auth server

	if( auth_mode > 0 )
	{
		if ( auth_server_address.type == NA_BAD )
		{
			netenabled = trap_Cvar_VariableIntegerValue("net_enabled");
			//if(netenabled & NET_ENABLEV4) // ???? kalish
			{
				Com_Printf( AUTH_PREFIX" resolving authserver address \n");
				if ( !trap_NET_StringToAdr( AUTH_SERVER_NAME, &auth_server_address ) )
				{
					Com_Printf( AUTH_PREFIX" authserver has no IPv4 address.\n");
				}
				else
				{
					unsigned short * pport = (unsigned short *)&auth_server_address.port[0];
					*pport = AUTH_SERVER_PORT;
					Com_Printf( AUTH_PREFIX" resolved \n");
					#ifdef DEBUG
						Com_Printf( AUTH_PREFIX" %s resolved to %i.%i.%i.%i:%i\n", AUTH_SERVER_NAME, auth_server_address.ip[0], auth_server_address.ip[1], auth_server_address.ip[2], auth_server_address.ip[3], *pport );
					#endif
					trap_Cvar_Set("sv_authServerIP", va("%i.%i.%i.%i", auth_server_address.ip[0], auth_server_address.ip[1], auth_server_address.ip[2], auth_server_address.ip[3]));
				}
			}
			if( auth_server_address.type == NA_BAD )
			{
				// if the address failed to resolve, clear it
				// so we don't take repeated dns hits
				Com_Printf( AUTH_PREFIX" couldn't resolve address\n");
				Com_Printf( AUTH_PREFIX" authentication system is now disabled.\n");
				auth_mode = 0;
				trap_Cvar_Set( "auth_status", "Off (no authserver)" );
				return;
			}
		}
	}

	// auth_header

	if( auth_mode > 0 )
	{
		// Auth Init Log

		G_LogPrintf("InitAuth: \\auth\\%i\\auth_status\\%s\\auth_cheaters\\%i\\auth_tags\\%i\\auth_notoriety\\%i\\auth_groups\\%s\\auth_owners\\%s\\auth_verbosity\\%i\n",
			auth.integer, auth_status.string, auth_cheaters.integer, auth_tags.integer, auth_notoriety.integer, auth_groups.string, auth_owners.string, auth_verbosity.integer
		);

		// init vars

		memset(auth_heartbeat,0,sizeof(auth_heartbeat));
		memset(auth_players,0,sizeof(auth_players));
		memset(auth_header,0,sizeof(auth_header));

		// auth_header

		trap_Cvar_VariableStringBuffer("fs_game", game, sizeof(game) );

		test = 1;

		if(test>0) test = Auth_cat_str( auth_header, sizeof(auth_header), PRODUCT_VERSION, 1);
		if(test>0) test = Auth_cat_str( auth_header, sizeof(auth_header), PLATFORM_VERSION, 1);
		if(test>0) test = Auth_cat_int( auth_header, sizeof(auth_header), trap_Cvar_VariableIntegerValue("protocol"), 0);
		if(test>0) test = Auth_cat_str( auth_header, sizeof(auth_header), game, 1);
		if(test>0) test = Auth_cat_str( auth_header, sizeof(auth_header), GAME_NAME, 1);
		if(test>0) test = Auth_cat_str( auth_header, sizeof(auth_header), GAME_VERSION, 1);
		if(test>0) test = Auth_cat_int( auth_header, sizeof(auth_header), auth_cheaters.integer, 0);
		if(test>0) test = Auth_cat_int( auth_header, sizeof(auth_header), 0/*auth_nicknames.integer*/, 0);
		if(test>0) test = Auth_cat_int( auth_header, sizeof(auth_header), auth_tags.integer, 0);
		if(test>0) test = Auth_cat_int( auth_header, sizeof(auth_header), auth_notoriety.integer, 0);
		if(test>0) test = Auth_cat_str( auth_header, sizeof(auth_header), auth_groups.string, 1);
		if(test>0) test = Auth_cat_str( auth_header, sizeof(auth_header), auth_owners.string, 1);
		if(test>0) test = Auth_cat_int( auth_header, sizeof(auth_header), g_matchMode.integer, 0);

		SV_Authserver_Heartbeat();
	}
}

/*
===============
SV_Authserver_Send_Packet
===============
*/
void QDECL SV_Authserver_Send_Packet( const char *cmd, const char *args )
{
	const char *to;
	char game[16];
	int test;
	char string[AUTH_MAX_STRING_CHARS];
	char crypt[AUTH_MAX_STRING_CRYPT];
	int len;

	trap_Cvar_VariableStringBuffer("fs_game", game, sizeof(game) );

	// build request

	string[0]='S';
	string[1]='V';
	string[2]=':';
	string[3]='\0';

	test = 1;

	if(test>0) test = Auth_cat_str( string, sizeof(string), cmd, 0);
	if(test>0) test = Auth_cat_str( string, sizeof(string), auth_header, -1);
	if(test>0) test = Auth_cat_str( string, sizeof(string), args, -1);

	if(test<1)
	{
		Com_Printf( AUTH_PREFIX" not sent - malformed");
	}
	else
	{
		// crypt

		len = strlen(string);

		if( 16 + len*4 > sizeof(crypt) ) {
			Com_Printf( AUTH_PREFIX" not sent - packet overruns max length (%i"), sizeof(crypt) );
			return;
		}

		Com_sprintf( crypt, sizeof(crypt), "SV:AUTH %i %s", AUTH_PROTOCOL, Auth_encrypt( string ) );

		// send

		to = va("%i.%i.%i.%i:%i", auth_server_address.ip[0], auth_server_address.ip[1],
			auth_server_address.ip[2], auth_server_address.ip[3], AUTH_SERVER_PORT) ;

		trap_NET_SendPacket( NS_CLIENT, strlen( crypt ), crypt, to );
	}
}

/*
===============
SV_Authserver_Heartbeat
===============
*/
void SV_Authserver_Heartbeat (void)
{
	gclient_t *cl;
	char	serverinfo[MAX_INFO_STRING];
	char	status[MAX_INFO_STRING];
	char	player[MAX_INFO_STRING];
	int 	playerLength;
	int 	statusLength;
	int i;

	if ( auth_mode > 0 )
	{
		trap_GetServerinfo( serverinfo, sizeof(serverinfo));

		if( Q_stricmp(serverinfo, auth_heartbeat) )
		{
			Q_strncpyz(auth_heartbeat, serverinfo, sizeof(auth_heartbeat));
			Com_Printf( AUTH_PREFIX" sending heartbeat \n");
			SV_Authserver_Send_Packet( "HEARTBEAT", serverinfo );
		}

		status[0] = 0;
		statusLength = 0;

		for (i=0 ; i < level.maxclients ; i++)
		{
			cl = &level.clients[i];
			if (cl->pers.connected == CON_CONNECTED)
			{
				int clientNum = cl->ps.clientNum;
				Com_sprintf (player, sizeof(player), " \"%i %i %i %s\"",
					i, cl->ps.ping, cl->ps.persistant[PERS_SCORE], cl->pers.netname);
				playerLength = strlen(player);
				if (statusLength + playerLength >= sizeof(status) ) {
					break;		// can't hold any more
				}
				strcpy (status + statusLength, player);
				statusLength += playerLength;
			}
		}

		if ( Q_stricmp(status, auth_players) )
		{
			Q_strncpyz(auth_players, status, sizeof(auth_players));
			Com_Printf( AUTH_PREFIX" sending players list \n");
			SV_Authserver_Send_Packet( "PLAYERS", status );
		}
	}
}

/*
===============
SV_Authserver_Shutdown
===============
*/
void SV_Authserver_Shutdown (void)
{
	if ( auth_mode > 0 )
	{
		Com_Printf( AUTH_PREFIX" sending shutdown \n");
		SV_Authserver_Send_Packet( "SHUTDOWN", "" );
	}

	return;
}

/*
===============
G_Argv
===============
*/
char *G_Argv( int arg )
{
	static char  buffer[MAX_STRING_CHARS];

	trap_Argv( arg, buffer, sizeof(buffer));

	return buffer;
}


/*
===============
SV_Authserver_Whois
===============
*/
void SV_Authserver_Whois( int idClient )
{
	gclient_t *cl;

	if( auth_mode > 0 )
	{
		cl = &level.clients[idClient];

		if(!cl)
			return;

		SV_Auth_Print_User_Infos_f(cl, 2);
	}
}

/*
===============
SV_Authserver_Ban
===============
*/
void SV_Authserver_Ban( int clientNum, int days, int hours, int mins )
{
	gclient_t *cl;
	char buffer[AUTH_MAX_STRING_CHARS];
	char userinfo[MAX_INFO_STRING];
	int test;

	if ( auth_mode < 1	)
	{
		Com_Printf( AUTH_PREFIX" auth system disabled.\n" );
		return;
	}

	if(auth_owners.integer<1)
	{
		Com_Printf( AUTH_PREFIX" not banlist available. Please set correctly auth_owners.\n" );
		return;
	}

	cl = &level.clients[clientNum];

	if(!cl)
	{
		Com_Printf( AUTH_PREFIX" client not found.\n" );
		return;
	}

	/*
	if( strlen(cl->sess.auth_login) < 2 )
	{
		Com_Printf( AUTH_PREFIX" ban not possible for players without account. Use the standard IP ban.\n" );
		return;
	}
	*/

	trap_GetUserinfo( clientNum, userinfo, sizeof(userinfo));

	buffer[0] = '\0'; test = 1;

	if(test>0) test = Auth_cat_str( buffer, sizeof(buffer), Info_ValueForKey(userinfo, "ip"), 1);
	if(test>0) test = Auth_cat_str( buffer, sizeof(buffer), Info_ValueForKey(userinfo, "authc"), 1);
	if(test>0) test = Auth_cat_str( buffer, sizeof(buffer), cl->sess.auth_login, 1);
	if(test>0) test = Auth_cat_int( buffer, sizeof(buffer), clientNum, 0);
	if(test>0) test = Auth_cat_int( buffer, sizeof(buffer), days, 0);
	if(test>0) test = Auth_cat_int( buffer, sizeof(buffer), hours, 0);
	if(test>0) test = Auth_cat_int( buffer, sizeof(buffer), mins, 0);
	if(test>0) test = Auth_cat_int( buffer, sizeof(buffer), 0, 0); // rcon owner id num
	if(test>0) test = Auth_cat_str( buffer, sizeof(buffer), "", 1); //	rcon owner account login

	if(test<1) return;

	SV_Authserver_Send_Packet( "BAN", buffer );

	Com_Printf( AUTH_PREFIX" sending ban for slot %i : %s\n", clientNum, cl->pers.netname);

	G_LogPrintf("AccountBan: %i - %s - %i:%i:%i\n", clientNum, cl->sess.auth_login, days, hours, mins);

	SV_Auth_DropClient( clientNum, "Ban kick", "You have been banned by an admin.");

	return;
}

#endif

/**
 * $(function)
 */
void G_BroadcastRoundWinner ( team_t teamWon, const char *how )
{
	int  teamLost;

//	int 	diff;
//	char*	status;


	// gsigms: client-side skin forcing:
	//   UPDATE THE CLIENT-SIDE COUNTER PART IF YOU CHANGE THE MESSAGES
	switch (teamWon)
	{
		case TEAM_FREE:
			trap_SetConfigstring( CS_ROUNDWINNER, "Round Drawn" );
			return;

		case TEAM_BLUE:
			teamLost = TEAM_RED;
			break;

		case TEAM_RED:
			teamLost = TEAM_BLUE;
			break;

		default:
			return;
	}

	if (how)
	{
		trap_SetConfigstring( CS_ROUNDWINNER,
					  va("%s" S_COLOR_WHITE " team %s and wins", ColoredTeamName(teamWon), how ));
	}
	else
	{
		if ((rand() % 100))
		{
			trap_SetConfigstring( CS_ROUNDWINNER,
						  va("%s" S_COLOR_WHITE " team wins", ColoredTeamName(teamWon)));
		}
		else
		{
			trap_SetConfigstring( CS_ROUNDWINNER,
						  va("%s" S_COLOR_WHITE " team is teh un-lose!", ColoredTeamName(teamWon)));
		}
	}

/*
	diff = level.teamScores[ teamWon ] - level.teamScores [ teamLost ];

	if ( diff > 0 )
		status = "increa";
	else if ( diff < 0 )
		status = "still trails";
	else
		status = NULL;

	if ( status )
		trap_SetConfigstring( CS_ROUNDWINNER,
							  va("%s" S_COLOR_WHITE " team wins, now %s %s" S_COLOR_WHITE " by %i", ColoredTeamName(teamWon), status, ColoredTeamName(teamLost), abs(diff) ) );
	else
		trap_SetConfigstring( CS_ROUNDWINNER,
							  va("%s" S_COLOR_WHITE " team wins, game is now tied", ColoredTeamName(teamWon) ) );
*/
}

/**
 * $(function)
 */
void QDECL G_Printf( const char *fmt, ... )
{
	va_list  argptr;
	char	 text[1024];

	va_start (argptr, fmt);
	vsprintf (text, fmt, argptr);
	va_end (argptr);

	trap_Printf( text );
}

/**
 * $(function)
 */
void QDECL G_Error( const char *fmt, ... )
{
	va_list  argptr;
	char	 text[1024];

	va_start (argptr, fmt);
	vsprintf (text, fmt, argptr);
	va_end (argptr);

	trap_Error( text );
}

/*
================
G_FindTeams

Chain together all entities with a matching team field.
Entity teams are used for item groups and multi-entity mover groups.

All but the first will have the FL_TEAMSLAVE flag set and teammaster field set
All but the last will have the teamchain field set to the next one
================
*/
void G_FindTeams( void )
{
	gentity_t  *e, *e2;
	int 	   i, j;
	int 	   c, c2;

	c  = 0;
	c2 = 0;

	for (i = 1, e = g_entities + i ; i < level.num_entities ; i++, e++)
	{
		if (!e->inuse)
		{
			continue;
		}

		if (!e->team)
		{
			continue;
		}

		if (e->flags & FL_TEAMSLAVE)
		{
			continue;
		}
		e->teammaster = e;
		c++;
		c2++;

		for (j = i + 1, e2 = e + 1 ; j < level.num_entities ; j++, e2++)
		{
			if (!e2->inuse)
			{
				continue;
			}

			if (!e2->team)
			{
				continue;
			}

			if (e2->flags & FL_TEAMSLAVE)
			{
				continue;
			}

			if (!strcmp(e->team, e2->team))
			{
				c2++;
				e2->teamchain  = e->teamchain;
				e->teamchain   = e2;
				e2->teammaster = e;
				e2->flags	  |= FL_TEAMSLAVE;

				// make sure that targets only point at the master
				if (e2->targetname)
				{
					e->targetname  = e2->targetname;
					e2->targetname = NULL;
				}
			}
		}
	}

	G_Printf ("%i teams with %i entities\n", c, c2);
}

/**
 * $(function)
 */
void G_RemapTeamShaders(void)
{
/*
#ifdef MISSIONPACK
	char string[1024];
	float f = level.time * 0.001;
	Com_sprintf( string, sizeof(string), "team_icon/%s_red", g_redteam.string );
	AddRemap("textures/ctf2/redteam01", string, f);
	AddRemap("textures/ctf2/redteam02", string, f);
	Com_sprintf( string, sizeof(string), "team_icon/%s_blue", g_blueteam.string );
	AddRemap("textures/ctf2/blueteam01", string, f);
	AddRemap("textures/ctf2/blueteam02", string, f);
	trap_SetConfigstring(CS_SHADERSTATE, BuildShaderStateConfig());
#endif
*/
}

//Makes Sure g_survivor is set right
void G_InitSurvivor ( void )
{
	switch (g_gametype.integer)
	{
		case GT_FFA:
		case GT_JUMP:
		case GT_TEAM:
		case GT_CTF:
			trap_Cvar_Set ( "g_survivor", "0" );
			g_survivor.integer = 0;
			break;

		case GT_UT_CAH:
			trap_Cvar_Set ( "g_survivor", "0" );
			g_survivor.integer = 0;

			if (g_cahTime.integer == 0)
			{
				trap_Cvar_Set ( "g_cahTime", "60" );
			}

			break;

		case GT_UT_SURVIVOR:		// TS
		case GT_UT_ASSASIN: 	// FTL
		case GT_UT_BOMB:		// BOMB
		case GT_LASTMAN:
		default:
			trap_Cvar_Set ( "g_survivor", "1" );
			g_survivor.integer = 1;

			if (g_RoundTime.integer == 0)
			{
				trap_Cvar_Set ( "g_RoundTime", "3" );
			}
			// Bomb Game start stuff
			level.BombHolderLastRound = -1;
			level.UseLastRoundBomber  = qfalse;
			break;
	}
}

/*
=================
G_RegisterCvars
=================
*/
void G_RegisterCvars( void )
{
	int 		 i;
	cvarTable_t  *cv;
	qboolean	 remapped = qfalse;

	for (i = 0, cv = gameCvarTable ; i < gameCvarTableSize ; i++, cv++) {

		trap_Cvar_Register( cv->vmCvar, cv->cvarName, cv->defaultString, cv->cvarFlags );

		if (cv->vmCvar) {
			cv->modificationCount = cv->vmCvar->modificationCount;
		}

		// Lock sv_fps to MAX_SERVER_FPS
		if (Q_stricmp ( cv->cvarName, "sv_fps" ) == 0) {
			if (g_fps.integer > MAX_SERVER_FPS) {
				G_Printf ( S_COLOR_YELLOW "WARNING: sv_fps cannot be set higher than %i\n", MAX_SERVER_FPS );
				trap_Cvar_Set ( "sv_fps", va("%i", MAX_SERVER_FPS));
			}
		}

		if (cv->teamShader) {
			remapped = qtrue;
		}
	}

	//Iain
	/*
	// Dok8: removed, was causing serious server lag
	// if we want to do this we need to be careful when we set these vars
	trap_Cvar_Register(NULL, "g_redTeamList", "0", CVAR_SERVERINFO|CVAR_ROM);
	trap_Cvar_Register(NULL, "g_BlueTeamList", "0", CVAR_SERVERINFO|CVAR_ROM);
	trap_Cvar_Register(NULL, "g_teamScores", "0:0", CVAR_SERVERINFO|CVAR_ROM);
	trap_Cvar_Register(NULL, "g_nextCycleMap", "none", CVAR_SERVERINFO|CVAR_ROM);
	// trap_Cvar_Register(NULL, "sv_master1", "master.urbanterror.net", CVAR_ROM);

	*/
	//trap_Cvar_Register(NULL, "sv_master5", "master.urbanterror.net", CVAR_ROM);

	//@Barbatos
	//trap_Cvar_Register(NULL, "sv_master1", "master.urbanterror.info", CVAR_ROM);
	trap_Cvar_Register(NULL, "g_modversion", GAME_VERSION, CVAR_SERVERINFO | CVAR_ROM);

	if (remapped) {
		G_RemapTeamShaders();
	}

	// check some things
	if ((g_gametype.integer < 0) || (g_gametype.integer >= GT_MAX_GAME_TYPE)) {
		G_Printf( "g_gametype %i is out of range, defaulting to 0\n", g_gametype.integer );
		trap_Cvar_Set( "g_gametype", "0" );
	}

	// Fenix: check that some CVARs are
	// defaulted if we are playing jump mode
	if (g_gametype.integer != GT_JUMP) {

	    // Defaulting g_stamina
	    if (g_stamina.integer != 0) {
	        trap_Cvar_Set("g_stamina", "0");
	    }

		// Defaulting g_maxWallJumps to 3
		if (g_walljumps.integer != 3) {
		    trap_Cvar_Set("g_walljumps", "3");
		}

	}

	level.warmupModificationCount = g_warmup.modificationCount;
}

/*
=================
G_UpdateCvars
=================
*/
void G_UpdateCvars( void )
{
	int 		 i;
	cvarTable_t  *cv;
	qboolean	 remapped = qfalse;

	for (i = 0, cv = gameCvarTable ; i < gameCvarTableSize ; i++, cv++) {

		if (cv->vmCvar) {

			trap_Cvar_Update( cv->vmCvar );

			if (cv->modificationCount != cv->vmCvar->modificationCount) {
				const char *cvarName = cv->cvarName;
				cv->modificationCount = cv->vmCvar->modificationCount;

				if (cv->trackChange) {

					if (!Q_stricmp(cvarName, "g_gear")) {
						trap_SendServerCommand(-1, va("print \"Server: %s changed to %s (%s)\n\"",
										   	   	   cvarName, cv->vmCvar->string, allowedGear(atoi(cv->vmCvar->string))));
					} else {

						if (cv->cvarFlags & CVAR_LATCH) {
							trap_SendServerCommand(-1, va("print \"Server: %s changed for next map.\n\"",
											   	       cvarName, cv->vmCvar->value));
						} else {
							trap_SendServerCommand(-1, va("print \"Server: %s changed to %s\n\"",
											   	  	   cvarName, cv->vmCvar->string ));
						}
					}
				}

				if (cv->teamShader) {
					remapped = qtrue;
				}

				// Lock sv_fps to MAX_SERVER_FPS
				if (!Q_stricmp(cvarName, "sv_fps")) {
					/*if ( g_fps.integer > MAX_SERVER_FPS ) {
						G_Printf ( S_COLOR_YELLOW "WARNING: sv_fps cannot be set higher than %i\n", MAX_SERVER_FPS );
						trap_Cvar_Set ( "sv_fps", va("%i",MAX_SERVER_FPS) );
					}*/

					//if (g_fps.integer != 20) {
					//	G_Printf(S_COLOR_YELLOW "WARNING: sv_fps must be 20\n");
					//	trap_Cvar_Set("sv_fps", "20");
					//}
				}

				if (!Q_stricmp(cvarName, "g_warmup")) {
					if (g_warmup.integer < 4) {
						G_Printf(S_COLOR_YELLOW "WARNING: g_warmup cannot be lower then 4\n");
						trap_Cvar_Set("g_warmup", "4");
					}
				}

				if (!Q_stricmp(cvarName, "g_bombplanttime")) {
					if (g_bombPlantTime.integer < 2) {
						G_Printf(S_COLOR_YELLOW "WARNING: g_bombPlantTime cannot be lower then 2\n");
						trap_Cvar_Set("g_bombPlantTime", "2");
					} else {
						bg_weaponlist[UT_WP_BOMB].modes[0].fireTime = g_bombPlantTime.integer * 1000;
						trap_SetConfigstring( CS_BOMBPLANTTIME, va("%i", g_bombPlantTime.integer));
					}
				}

				// Fenix: check g_stamina CVAR value
				// Default it to 0 if we are not playing Jump mode
				if(!Q_stricmp(cvarName, "g_stamina")) {

				    if((g_stamina.integer != 0) && (g_gametype.integer != GT_JUMP)) {
				        G_Printf(S_COLOR_YELLOW "WARNING: g_stamina can be changed only in Jump mode\n");
				        trap_Cvar_Set("g_stamina", "0");
				        g_stamina.integer = 0;
				    }

				}


				// Fenix: check g_walljumps CVAR value
				// Default it to 3 if we are not playing Jump mode
				// and be sure the value is clamped between 3 and 100
				if (!Q_stricmp(cvarName, "g_walljumps")) {

					// Checking correct g_gametype
				    if ((g_walljumps.integer != 3) && (g_gametype.integer != GT_JUMP)) {
				         G_Printf(S_COLOR_YELLOW "WARNING: g_walljumps can be changed only in Jump mode\n");
				         trap_Cvar_Set("g_walljumps", "3");
				         g_walljumps.integer = 3;
				    }

					// Clamping CVAR value
				    if ((g_walljumps.integer < 3) || (g_walljumps.integer > 100)) {
				        G_Printf(S_COLOR_YELLOW "WARNING: g_walljumps must be between 3 and 100\n");
				        trap_Cvar_Set("g_walljumps", "3");
				        g_walljumps.integer = 3;
				    }

				}

				// Fenix: check that g_noDamage is used only in Jump mode
				if (!Q_stricmp(cvarName, "g_noDamage")) {

					// Checking correct g_gametype
					if (g_gametype.integer != GT_JUMP) {
						G_Printf(S_COLOR_YELLOW "WARNING: g_noDamage can be used only in Jump mode\n");
						trap_Cvar_Set("g_noDamage", "0");
						g_noDamage.integer = 0;
					}

					trap_SetConfigstring(CS_NODAMAGE, va("%i", g_noDamage.integer));
				}

				// Fenix: check correct value for funstuff
				if (!Q_stricmp(cvarName, "g_funstuff")) {

				    // Check correct value for the funstuff flag
				    if (g_funstuff.integer > 1) g_funstuff.integer = 1;
				    if (g_funstuff.integer < 0) g_funstuff.integer = 0;

				    // Send the config string so clients can update the flag
				    trap_SetConfigstring(CS_FUNSTUFF, va("%i", g_funstuff.integer));

				}
			}
		}
	}

	// If autodownload is enabled.
	if (trap_Cvar_VariableIntegerValue("sv_allowDownload")) {
		trap_Cvar_Set("sv_allowDownload", "0");
	}

	if (remapped) {
		G_RemapTeamShaders();
	}
}

/*
============
G_InitGame

============
*/
void G_InitGame( int levelTime, int randomSeed, int restart )
{
	int   i;
	char  filename[MAX_QPATH];
	char  temp[MAX_NAME_LENGTH];
	char  serverinfo[MAX_INFO_STRING];

	G_Printf ("------- Game Initialization -------\n");
	G_Printf ("name: " GAME_NAME "\n");
	G_Printf ("version: " GAME_VERSION "\n");
	G_Printf ("date: " __DATE__ "\n");

	srand( randomSeed );

	// Add pool memory
	bclear();
	add_pool(g_malloc_pool, MALLOC_POOL_SIZE);

	C_RegisterCvars();
	G_RegisterCvars();

	G_InitSurvivor();
	G_ProcessIPBans();

	C_PlayerAnimationsInit();
	G_AriesInit();

	if (!restart)
	{
		char  buf[128];
		int   seed;

		// First get the id that we generated last time
		trap_Cvar_VariableStringBuffer("gameid", buf, sizeof(buf));

		// Now stuff that game id into the string being send to the server
		//trap_SetConfigstring( CS_GAME_ID, buf );

		// Now use the old game id to generate a new one.
		seed  = atoi ( buf );
		seed += (g_gametype.integer * 2027);
		seed += (g_timelimit.integer * 27);
		seed += (g_fraglimit.integer * 1345);
		seed += (g_capturelimit.integer * 456);

		// Use the new see we generated using whacky means to
		// cacluate the gameid for the next round and save that
		// in the cvar
		srand ( seed );

		trap_Cvar_Set ( "gameid", va("%i", rand ( )));
	}

	// Instead of sending gameid, send the level time.
	trap_SetConfigstring(CS_GAME_ID, va("%i", levelTime));

	if (g_logroll.integer)
	{
		if (restart)
		{
			Com_sprintf ( filename, MAX_QPATH, "%04i_%s", g_logroll.integer - 1, g_log.string );
		}
		else
		{
			Com_sprintf ( filename, MAX_QPATH, "%04i_%s", g_logroll.integer, g_log.string );
			trap_Cvar_Set ( "g_logroll", va("%i", g_logroll.integer + 1 ));
		}
	}
	else
	{
		Com_sprintf ( filename, MAX_QPATH, "%s", g_log.string );
	}

	// set some level globals
	memset( &level, 0, sizeof(level));
	//chop the time, config string it to notify clients.
//	 level.timechop=levelTime;
//	 trap_SetConfigstring(CS_TIMECHOP, va("%i",level.timechop));

	//set the time to zip all.
	//	levelTime=0;
	////////////////////////////////

	level.time	 = levelTime;
	level.startTime  = levelTime;
	level.pausedTime = 0 ;
	level.snd_fry	 = G_SoundIndex("sound/player/fry.wav"); // FIXME standing in lava / slime

	trap_GetServerinfo( serverinfo, sizeof(serverinfo));

	if (filename[0])
	{
		if (g_logSync.integer)
		{
			trap_FS_FOpenFile( filename, &level.logFile, FS_APPEND_SYNC );
		}
		else
		{
			trap_FS_FOpenFile( filename, &level.logFile, FS_APPEND );
		}

		if (!level.logFile)
		{
			G_Printf( "WARNING: Couldn't open logfile: %s\n", filename );
		}
		else
		{
			G_LogPrintf("------------------------------------------------------------\n" );
			G_LogPrintf("InitGame: %s\n", serverinfo );
		}
	}
	else
	{
		G_Printf( "Not logging to disk.\n" );
	}

#ifdef USE_AUTH
	BG_RandomInit(serverinfo);
#endif

	// initialize all entities for this game
	memset( g_entities, 0, MAX_GENTITIES * sizeof(g_entities[0]));
	level.gentities = g_entities;

	// initialize all clients for this game
	level.maxclients = g_maxclients.integer;
	memset( g_clients, 0, MAX_CLIENTS * sizeof(g_clients[0]));
	level.clients	 = g_clients;

	// set client fields on player ents
	for (i = 0 ; i < level.maxclients ; i++)
	{
		g_entities[i].client			   = level.clients + i;
		g_entities[i].client->LastClientUpdate = 0;
		g_entities[i].client->goalbits		   = 0;
	}

	// always leave room for the max number of clients,
	// even if they aren't all used, so numbers inside that
	// range are NEVER anything but clients
	level.num_entities = MAX_CLIENTS;

	// let the server system know where the entites are
	trap_LocateGameData( level.gentities, level.num_entities, sizeof(gentity_t),
				 &level.clients[0].ps, sizeof(level.clients[0]));

	// reserve some spots for dead player bodies
//	InitBodyQue();

	ClearRegisteredItems();

	num_levelspawn		  = 0;
	cur_levelspawn		  = 0;
	level.NumBombSites	  = 0;
	level.RoundCount	  = 0;
	level.BombMapAble	  = qfalse; // not about to play bomb mode!
	level.survivorWinTeam = 0;
	level.RoundRespawn	  = qfalse;

	// Pause stuff.
	level.pauseState = 0;
	level.pauseTime  = 0;

	trap_SetConfigstring(CS_PAUSE_STATE, va("%d %i %i", level.pauseState, level.pauseTime, level.pausedTime));

	// Match stuff.
	if (restart)
	{
		// If we're in match mode and we're playing a team based gametype.
		if (trap_Cvar_VariableIntegerValue("g_matchMode") && (trap_Cvar_VariableIntegerValue("g_gametype") >= GT_TEAM) && (trap_Cvar_VariableIntegerValue("g_gametype") != GT_JUMP))
		{
			// Get config string value.
			trap_GetConfigstring(CS_MATCH_STATE, temp, sizeof(temp));

			level.matchState = atoi(temp);
		}
		else
		{
			level.matchState = 0;

			trap_SetConfigstring(CS_MATCH_STATE, va("%d", level.matchState));
		}
	}
	else
	{
		level.matchState = 0;

		// If we're in match mode and we're playing a team based gametype.
		if (trap_Cvar_VariableIntegerValue("g_matchMode") && (trap_Cvar_VariableIntegerValue("g_gametype") >= GT_TEAM) && (trap_Cvar_VariableIntegerValue("g_gametype") != GT_JUMP))
		{
			level.matchState |= UT_MATCH_ON;
		}

		trap_SetConfigstring(CS_MATCH_STATE, va("%d", level.matchState));
	}

	// Attacking team
	level.AttackingTeam = TEAM_RED;

	// If we're in match mode.
/*	if (g_matchMode.integer)
	{
		// Get red team name.
		trap_Cvar_VariableStringBuffer("g_nameRed", temp, sizeof(temp));

		// If name is set.
		if (strlen(temp))
		{
			// Set config string.
//			trap_SetConfigstring(CS_TEAM_NAME_RED, temp);
		}

		// Get blue team name.
		trap_Cvar_VariableStringBuffer("g_nameBlue", temp, sizeof(temp));

		// If name is set.
		if (strlen(temp))
		{
			// Set config string.
		//	trap_SetConfigstring(CS_TEAM_NAME_BLUE, temp);
		}
	} */

	// Get sv_master5 value.
	//trap_Cvar_VariableStringBuffer("sv_master2", temp, sizeof(temp));

	//if (!strlen(temp))
	//{
	//	trap_Cvar_Set("sv_master2", "master.urbanterror.info");
	//}
	//trap_Cvar_VariableStringBuffer("sv_master3", temp, sizeof(temp));

	//if (!strlen(temp))
	//{
	//	trap_Cvar_Set("sv_master3", "master2.urbanterror.info");
	//}
	//trap_Cvar_VariableStringBuffer("sv_master4", temp, sizeof(temp));

	//if (!strlen(temp))
	//{
	//	trap_Cvar_Set("sv_master4", "master.quake3arena.com");
	//}

	// parse the key/value pairs and spawn gentities
	G_SpawnEntitiesFromString();

	// general initialization
	TSnumGroups = 0; // only gets cleared on a map restart..
	G_FindTeams();
	G_InitSurvivor();
	level.survivorStartTime = level.time;

	// Initialize the UT spawn points, try different ones if the
	// real gametype fails
	if (!G_InitUTSpawnPoints ( g_gametype.integer ))
	{
		if (!G_InitUTSpawnPoints ( GT_UT_SURVIVOR ))
		{
			int  gametype;

			for (gametype = GT_MAX_GAME_TYPE - 1; gametype >= GT_TEAM; gametype--)
			{
				// We know this one doesnt work
				if ((gametype == g_gametype.integer) || (gametype == GT_UT_SURVIVOR))
				{
					continue;
				}

				if (G_InitUTSpawnPoints ( gametype ))
				{
					G_Printf ( S_COLOR_YELLOW "Warning: spawn groups from gametype %i used in place of missing gametype %i spawns.\n", gametype, g_gametype.integer );
					break;
				}
			}
		}
		else
		{
			G_Printf ( S_COLOR_YELLOW "Warning: TS spawns used instead of gametype %i spawns.\n", g_gametype.integer );
		}
	}

	// make sure we have flags for CTF, etc
	if((g_gametype.integer >= GT_TEAM) && (g_gametype.integer != GT_JUMP))
	{
		G_CheckTeamItems();
	}

	// Initialize the UT bot long term item goals
	G_InitUTBotItemGoals ( );

//	SaveRegisteredItems();

	G_Printf ("-----------------------------------\n");

	G_RemapTeamShaders();

	// Initialize the UT tracing
//	if(qfalse == utTraceInitialize ( ) )
//		G_Error ( "Could not initialize the hit detection system." );

	// Initialize the winner to nobody
	if(g_survivor.integer)
	{
		trap_SetConfigstring( CS_ROUNDWINNER, "" );
	}

	level.survivorBlueDeaths = 0;
	level.survivorRedDeaths  = 0;

//GottaBeKD: Moved up right above G_SpawnEntitiyesFromStrin() call
	if (trap_Cvar_VariableIntegerValue( "bot_enable" ))
	{
		BotAISetup( restart );
		BotAILoadMap( restart );
		G_InitBots( restart );
	}

	//Set the rounds to 0
	level.g_roundcount = 0;
	trap_SetConfigstring( CS_ROUNDCOUNT, va("%i", level.g_roundcount ));

	// Set the bomb plant time
	trap_SetConfigstring( CS_BOMBPLANTTIME, va("%i", g_bombPlantTime.integer));
	bg_weaponlist[UT_WP_BOMB].modes[0].fireTime = g_bombPlantTime.integer * 1000;

	// Fenix: jump mode config strings
	if (g_gametype.integer == GT_JUMP) {
		trap_SetConfigstring( CS_NODAMAGE, va("%i", g_noDamage.integer ));
	}

	trap_SetConfigstring(CS_FUNSTUFF, va("%i", g_funstuff.integer));

#ifdef USE_AUTH
	SV_Auth_Init();
	InitHaxNetwork(); //@r00t
#endif
}

/*
=================
G_ShutdownGame
=================
*/
void G_ShutdownGame( int restart )
{
	G_Printf ("==== ShutdownGame ====\n");

	if (level.logFile)
	{
		G_LogPrintf("ShutdownGame:\n" );
		G_LogPrintf("------------------------------------------------------------\n" );
		trap_FS_FCloseFile( level.logFile );
	}

	// write all the client session data so we can get it back
//	if(g_survivor.integer && level.survivorRestart)
//		level.survivorStartTime = (level.time - level.startTime) + level.survivorStartTime;
//	else
//		level.survivorStartTime = 0;

	G_WriteSessionData();

	if (trap_Cvar_VariableIntegerValue( "bot_enable" ))
	{
		BotAIShutdown( restart );
	}
}

//===================================================================

// this is only here so the functions in q_shared.c and bg_*.c can link

void QDECL Com_Error ( int level, const char *error, ... )
{
	va_list  argptr;
	char	 text[1024];

	va_start (argptr, error);
	vsprintf (text, error, argptr);
	va_end (argptr);

	G_Error( "%s", text);
}

/**
 * $(function)
 */
void QDECL Com_Printf( const char *msg, ... )
{
	va_list  argptr;
	char	 text[1024];

	va_start (argptr, msg);
	vsprintf (text, msg, argptr);
	va_end (argptr);

	G_Printf ("%s", text);
}

/*
========================================================================

PLAYER COUNTING / SCORE SORTING

========================================================================
*/

/*
=============
AddTournamentPlayer

If there are less than two tournament players, put a
spectator in the game and restart
=============
*/
void AddTournamentPlayer( void )
{
	int    i;
	gclient_t  *client;
	gclient_t  *nextInLine;

	if (level.numPlayingClients >= 2)
	{
		return;
	}

	// never change during intermission
	if (level.intermissiontime)
	{
		return;
	}

	nextInLine = NULL;

	for (i = 0 ; i < level.maxclients ; i++)
	{
		client = &level.clients[i];

		if (client->pers.connected != CON_CONNECTED)
		{
			continue;
		}

		if (client->sess.sessionTeam != TEAM_SPECTATOR)
		{
			continue;
		}

		// never select the dedicated follow or scoreboard clients
		if ((client->sess.spectatorState == SPECTATOR_SCOREBOARD) ||
			(client->sess.spectatorClient < 0))
		{
			continue;
		}

		if (!nextInLine || (client->sess.spectatorTime < nextInLine->sess.spectatorTime))
		{
			nextInLine = client;
		}
	}

	if (!nextInLine)
	{
		return;
	}

	level.warmupTime = -1;

	// set them to free-for-all team
	SetTeam( &g_entities[nextInLine - level.clients], "f", qfalse);
}

/*
=======================
RemoveTournamentLoser

Make the loser a spectator at the back of the line
=======================
*/
void RemoveTournamentLoser( void )
{
	int  clientNum;

	if (level.numPlayingClients != 2)
	{
		return;
	}

	clientNum = level.sortedClients[1];

	if (level.clients[clientNum].pers.connected != CON_CONNECTED)
	{
		return;
	}

	// make them a spectator
	SetTeam( &g_entities[clientNum], "s", qfalse );
}

/*
=======================
RemoveTournamentWinner
=======================
*/
void RemoveTournamentWinner( void )
{
	int  clientNum;

	if (level.numPlayingClients != 2)
	{
		return;
	}

	clientNum = level.sortedClients[0];

	if (level.clients[clientNum].pers.connected != CON_CONNECTED)
	{
		return;
	}

	// make them a spectator
	SetTeam( &g_entities[clientNum], "s", qfalse );
}

/*
=======================
AdjustTournamentScores
=======================
*/
void AdjustTournamentScores( void )
{
	int  clientNum;

	clientNum = level.sortedClients[0];

	if (level.clients[clientNum].pers.connected == CON_CONNECTED)
	{
		level.clients[clientNum].sess.wins++;
		ClientUserinfoChanged( clientNum );
	}

	clientNum = level.sortedClients[1];

	if (level.clients[clientNum].pers.connected == CON_CONNECTED)
	{
		level.clients[clientNum].sess.losses++;
		ClientUserinfoChanged( clientNum );
	}
}

/*
=============
SortRanks

=============
*/
int QDECL SortRanks( const void *a, const void *b )
{
	gclient_t  *ca, *cb;

	ca = &level.clients[*(int *)a];
	cb = &level.clients[*(int *)b];

	// sort special clients last
	if ((ca->sess.spectatorState == SPECTATOR_SCOREBOARD) || (ca->sess.spectatorClient < 0)) {
		return 1;
	}

	if ((cb->sess.spectatorState == SPECTATOR_SCOREBOARD) || (cb->sess.spectatorClient < 0)) {
		return -1;
	}

	// then connecting clients
	if (ca->pers.connected == CON_CONNECTING) {
		return 1;
	}

	if (cb->pers.connected == CON_CONNECTING) {
		return -1;
	}

	// then spectators
	if ((ca->sess.sessionTeam == TEAM_SPECTATOR) && (cb->sess.sessionTeam == TEAM_SPECTATOR)) {
		if (ca->sess.spectatorTime < cb->sess.spectatorTime) {
			return -1;
		}

		if (ca->sess.spectatorTime > cb->sess.spectatorTime) {
			return 1;
		}
		return 0;
	}

	if (ca->sess.sessionTeam == TEAM_SPECTATOR) {
		return 1;
	}

	if (cb->sess.sessionTeam == TEAM_SPECTATOR) {
		return -1;
	}


	//@Fenix - different sorting mechanism for jump mode
	if (g_gametype.integer == GT_JUMP) {
		if (((ca->ps.persistant[PERS_JUMPBESTTIME] != 0) && (cb->ps.persistant[PERS_JUMPBESTTIME] == 0)) ||
			((ca->ps.persistant[PERS_JUMPBESTTIME] < cb->ps.persistant[PERS_JUMPBESTTIME]) &&
			((ca->ps.persistant[PERS_JUMPBESTTIME] != 0) && (cb->ps.persistant[PERS_JUMPBESTTIME] != 0)))) {
			return -1;
		}
		else if (((ca->ps.persistant[PERS_JUMPBESTTIME] == 0) && (cb->ps.persistant[PERS_JUMPBESTTIME] != 0)) ||
				 ((ca->ps.persistant[PERS_JUMPBESTTIME] > cb->ps.persistant[PERS_JUMPBESTTIME]) &&
				 ((ca->ps.persistant[PERS_JUMPBESTTIME] != 0) && (cb->ps.persistant[PERS_JUMPBESTTIME] != 0)))) {
			return 1;
		}
		else if (((ca->ps.persistant[PERS_JUMPBESTTIME] == 0) && (cb->ps.persistant[PERS_JUMPBESTTIME] == 0)) ||
				 (ca->ps.persistant[PERS_JUMPBESTTIME] == cb->ps.persistant[PERS_JUMPBESTTIME])) {
			return 0;
		}
	}

	//@Fenix - do not sort by score in JUMP mode
	if ((ca->ps.persistant[PERS_SCORE] > cb->ps.persistant[PERS_SCORE]) && (g_gametype.integer != GT_JUMP)){
		return -1;
	}

	//@Fenix - do not sort by score in jump mode
	if ((ca->ps.persistant[PERS_SCORE] < cb->ps.persistant[PERS_SCORE]) && (g_gametype.integer != GT_JUMP)) {
		return 1;
	}

	// Sort by deaths now, the one with more deaths comes last
	// @Fenix - do not count deaths in jump mode
	if ((ca->ps.persistant[PERS_KILLED] > cb->ps.persistant[PERS_KILLED]) && (g_gametype.integer != GT_JUMP)) {
		return 1;
	}

	// Sort by deaths now, the one with more deaths comes last
	// @Fenix - do not count deaths in jump mode
	if ((ca->ps.persistant[PERS_KILLED] < cb->ps.persistant[PERS_KILLED]) && (g_gametype.integer != GT_JUMP)) {
		return -1;
	}

	return 0;
}

/*
============
CalculateRanks

Recalculates the score ranks of all players
This will be called on every client connect, begin, disconnect, death,
and team change.
============
*/
void CalculateRanks( void )
{
	int 		i;
	int 		rank;
	int 		score;
	int 		newScore;
	gclient_t  *cl;

	level.follow1				 = -1;
	level.follow2				 = -1;
	level.numConnectedClients	 = 0;
	level.numNonSpectatorClients = 0;
	level.numPlayingClients 	 = 0;
	level.numVotingClients		 = 0;	// don't count bots

	for (i = 0; i < TEAM_NUM_TEAMS; i++) {
		level.numteamVotingClients[i] = 0;
	}

	for (i = 0 ; i < level.maxclients ; i++) {
		if (level.clients[i].pers.connected != CON_DISCONNECTED) {
			level.sortedClients[level.numConnectedClients] = i;
			level.numConnectedClients++;

			if (level.clients[i].sess.sessionTeam != TEAM_SPECTATOR) {
				level.numNonSpectatorClients++;

				// decide if this should be auto-followed
				if (level.clients[i].pers.connected == CON_CONNECTED) {
					level.numPlayingClients++;

					if (!(g_entities[i].r.svFlags & SVF_BOT)) {
						level.numVotingClients++;

						if (level.clients[i].sess.sessionTeam == TEAM_RED) {
							level.numteamVotingClients[0]++;
						} else if (level.clients[i].sess.sessionTeam == TEAM_BLUE) {
							level.numteamVotingClients[1]++;
						}
					}

					if (level.follow1 == -1) {
						level.follow1 = i;
					} else if (level.follow2 == -1) {
						level.follow2 = i;
					}
				}
			}
		}
	}

	qsort( level.sortedClients, level.numConnectedClients, sizeof(level.sortedClients[0]), SortRanks );

	// set the rank value for all clients that are connected and not spectators
	if ((g_gametype.integer >= GT_TEAM) && (g_gametype.integer != GT_JUMP)) {
		// in team games, rank is just the order of the teams, 0=red, 1=blue, 2=tied
		for (i = 0;  i < level.numConnectedClients; i++) {
			cl = &level.clients[level.sortedClients[i]];

			if (level.teamScores[TEAM_RED] == level.teamScores[TEAM_BLUE]) {
				cl->ps.persistant[PERS_RANK] = 2;
			} else if (level.teamScores[TEAM_RED] > level.teamScores[TEAM_BLUE]) {
				cl->ps.persistant[PERS_RANK] = 0;
			} else {
				cl->ps.persistant[PERS_RANK] = 1;
			}
		}
	}
	else {
		rank  = -1;
		score = 0;

		for (i = 0;  i < level.numPlayingClients; i++) {
			cl = &level.clients[level.sortedClients[i]];
			newScore = (g_gametype.integer != GT_JUMP) ? cl->ps.persistant[PERS_SCORE] : cl->ps.persistant[PERS_JUMPBESTTIME];
			if ((i == 0) || (newScore != score)) {
				// assume we aren't tied until the next client is checked
				rank								   = i;
				level.clients[level.sortedClients[i]].ps.persistant[PERS_RANK] = rank;
			}
			else {
				// we are tied with the previous client
				level.clients[level.sortedClients[i - 1]].ps.persistant[PERS_RANK] = rank | RANK_TIED_FLAG;
				level.clients[level.sortedClients[i]].ps.persistant[PERS_RANK]	   = rank | RANK_TIED_FLAG;
			}
			score = newScore;
		}
	}

/*
	// set the CS_SCORES1/2 configstrings, which will be visible to everyone
	if ( g_gametype.integer >= GT_TEAM ) {
			trap_SetConfigstring( CS_SCORES1, va("%i", level.teamScores[TEAM_RED] ) );
			trap_SetConfigstring( CS_SCORES2, va("%i", level.teamScores[TEAM_BLUE] ) );
	} else {
			if ( level.numConnectedClients == 0 ) {
					trap_SetConfigstring( CS_SCORES1, va("%i", SCORE_NOT_PRESENT) );
					trap_SetConfigstring( CS_SCORES2, va("%i", SCORE_NOT_PRESENT) );
			} else if ( level.numConnectedClients == 1 ) {
					trap_SetConfigstring( CS_SCORES1, va("%i", level.clients[ level.sortedClients[0] ].ps.persistant[PERS_SCORE] ) );
					trap_SetConfigstring( CS_SCORES2, va("%i", SCORE_NOT_PRESENT) );
			} else {
					trap_SetConfigstring( CS_SCORES1, va("%i", level.clients[ level.sortedClients[0] ].ps.persistant[PERS_SCORE] ) );
					trap_SetConfigstring( CS_SCORES2, va("%i", level.clients[ level.sortedClients[1] ].ps.persistant[PERS_SCORE] ) );
			}
	}
*/

	// see if it is time to end the level
	CheckExitRules();

	// if we are at the intermission, send the new info to everyone
	if (level.intermissiontime) {
		SendScoreboardMessageToAllClients();
	}
}

/*
========================================================================

MAP CHANGING

========================================================================
*/

/*
========================
SendScoreboardMessageToAllClients

Do this at BeginIntermission time and whenever ranks are recalculated
due to enters/exits/forced team changes
========================
*/
void SendScoreboardMessageToAllClients( void ) {

	int  i;

	for (i = 0 ; i < level.maxclients ; i++) {
		if (level.clients[i].pers.connected == CON_CONNECTED) {
			Cmd_Score_f( g_entities + i );
		}
	}
}

/**
 * $(function)
 */
void SendScoreboardSingleMessageToAllClients(gentity_t *entity) {

	int  i;

	// If an invalid pointer.
	if (!entity) {
		return;
	}

	// If an invalid entity.
	if (!entity->inuse || !entity->client) {
		return;
	}

	// Loop through clients.
	for (i = 0; i < level.maxclients; i++) {
		// If this client is connected.
		if (level.clients[i].pers.connected == CON_CONNECTED) {
			// Send scores.
			Cmd_ScoreSingle_f(entity, g_entities + i);
		}
	}
}

/**
 * $(function)
 */
void SendScoreboardDoubleMessageToAllClients(gentity_t *ent1, gentity_t *ent2) {

	int  i;

	// If an invalid pointer.
	if (!ent1 || !ent2) {
		return;
	}

	// If an invalid entity.
	if (!ent1->inuse || !ent1->client || !ent2->inuse || !ent2->client) {
		return;
	}

	// Loop through clients.
	for (i = 0; i < level.maxclients; i++) {
		// If this client is connected.
		if (level.clients[i].pers.connected == CON_CONNECTED) {
			// Send scores.
			Cmd_ScoreDouble_f(ent1, ent2, g_entities + i);
		}
	}
}

/**
 * $(function)
 */
void SendScoreboardGlobalMessageToAllClients(void)
{
	int  i;

	// Loop through clients.
	for (i = 0; i < level.maxclients; i++)
	{
		// If this client is connected.
		if (level.clients[i].pers.connected == CON_CONNECTED)
		{
			// Send scores.
			Cmd_ScoreGlobal_f(g_entities + i);
		}
	}
}

/*
========================
MoveClientToIntermission

When the intermission starts, this will be called for all players.
If a new client connects, this will be called after the spawn function.
========================
*/
void MoveClientToIntermission( gentity_t *ent )
{
	// take out of follow mode if needed
	if ((ent->client->sess.spectatorState == SPECTATOR_FOLLOW) || !ent->client->ghost)
	{
//		StopFollowing( ent, qtrue );
	}

	// move to the spot
	VectorCopy( level.intermission_origin, ent->s.origin );
	VectorCopy( level.intermission_origin, ent->client->ps.origin );
	VectorCopy (level.intermission_angle, ent->client->ps.viewangles);
	VectorClear ( ent->client->ps.velocity );
	//if( ent->client->sess.sessionTeam != TEAM_SPECTATOR )
	ent->client->ps.pm_type = PM_INTERMISSION;

	ent->client->ps.eFlags	= 0;
	ent->s.eFlags			= 0;
	ent->s.eType			= ET_GENERAL;
	ent->s.modelindex		= 0;
	ent->s.loopSound		= 0;
	ent->s.event			= 0;
	ent->r.contents 		= 0;
}

/*
==================
FindIntermissionPoint

This is also used for spectator spawns
==================
*/
void FindIntermissionPoint( void )
{
	gentity_t  *ent, *target;
	vec3_t	   dir;

	// find the intermission spot
	ent = G_FindClassHash (NULL, HSH_info_player_intermission);

	if (!ent)	// the map creator forgot to put in an intermission point...
	{
		SelectSpawnPoint ( vec3_origin, level.intermission_origin, level.intermission_angle );
	}
	else
	{
		VectorCopy (ent->s.origin, level.intermission_origin);
		VectorCopy (ent->s.angles, level.intermission_angle);

		// if it has a target, look towards it
		if (ent->target)
		{
			target = G_PickTarget( ent->target );

			if (target)
			{
				VectorSubtract( target->s.origin, level.intermission_origin, dir );
				vectoangles( dir, level.intermission_angle );
			}
		}
	}
}

/*
==================
BeginIntermission
==================
*/
void BeginIntermission( void )	 // Temp Fixes
{ int  i;

  if (level.intermissiontime)
  {
	  return;		// already active
  }

	// if in tournement mode, change the wins / losses
  level.intermissiontime = level.time;
  FindIntermissionPoint();

#ifdef MISSIONPACK
  if (g_singlePlayer.integer)
  {
	  trap_Cvar_Set("ui_singlePlayerActive", "0");
	  UpdateTournamentInfo();
  }
#else
	// if single player game
  if (g_gametype.integer == GT_SINGLE_PLAYER)
  {
	  UpdateTournamentInfo();
	  SpawnModelsOnVictoryPads();
  }
#endif

	// move all clients to the intermission point
/*	for (i=0 ; i< level.maxclients ; i++) {
		client = g_entities + i;
		if (!client->inuse)
			continue;
		// respawn if dead
		if( client->client->sess.sessionTeam != TEAM_SPECTATOR && client->health <= 0 ) {
			client->client->ghost = qfalse;
			respawn(client);

		}
		MoveClientToIntermission( client );
	} */
  for(i = 0; i < level.maxclients; i++)
  {
	  gclient_t  *cl;
	  gentity_t  *gt;

	  cl = &level.clients[i];

	  if(cl->pers.connected != CON_CONNECTED)
	  {
		  continue;
	  }

	  if(cl->ps.clientNum != i)
	  {
		  cl->ps.clientNum = cl->savedclientNum;
	  }
	  else
	  {
//			cl->savedclientNum = cl->ps.clientNum;//no need. never changes
	  }
	  gt			= &g_entities[i];
	  gt->client->ghost = qfalse;
	  ClientSpawn( gt );
	  // ClientSpawn( &g_entities[ i ] );
	  MoveClientToIntermission( gt );
  }

	// send the current scoring to all clients
  SendScoreboardMessageToAllClients();
}

/*
=============
ExitLevel

When the intermission has been exited, the server is either killed
or moved to a new level based on the "nextmap" cvar

=============
*/
void ExitLevel (void)
{
	int    i;
	gclient_t  *cl;

#ifdef USE_AUTH
	SendHaxReport(); //@r00t: Send final hax report. Clients send final data on first frame of intermission.
#endif

	if ((level.swaproles > 0) && (g_gametype.integer >= GT_TEAM) && (g_gametype.integer != GT_JUMP))
	{
		//Swap sides...

		//first check to see if we even played...
		if (g_matchMode.integer)
		{
			if (!((level.matchState & UT_MATCH_RR) && (level.matchState & UT_MATCH_BR)))
			{
				level.swaproles = 1;
				Svcmd_Restart_f(qtrue, qfalse);
				return;
			}
		}

		level.swaproles = 0;

		//Turn off intermission
		level.intermissiontime	 = 0;
		level.intermissionQueued = 0;

		//Stash the scores and send them to the client
		level.oldRedScore		= level.teamScores[TEAM_BLUE];
		level.oldBlueScore		= level.teamScores[TEAM_RED];

		level.teamScores[TEAM_RED]	= 0;
		level.teamScores[TEAM_BLUE] = 0;

		//unready both teams
		level.matchState &= ~UT_MATCH_RR;
		level.matchState &= ~UT_MATCH_BR;
		trap_SetConfigstring(CS_MATCH_STATE, va("%d", level.matchState));

		trap_SetConfigstring( CS_ROUNDSTAT, va("1 %i %i", level.oldRedScore, level.oldBlueScore ));

		Svcmd_SwapRoles();

		return;
	}

	//bot interbreeding
	BotInterbreedEndMatch();

	g_CycleMap();

	level.changemap 	   = NULL;
	level.intermissiontime = 0;

	// reset all the scores so we don't enter the intermission again
	level.teamScores[TEAM_RED]	= 0;
	level.teamScores[TEAM_BLUE] = 0;

	for (i = 0 ; i < g_maxclients.integer ; i++)
	{
		cl = level.clients + i;

		if (cl->pers.connected != CON_CONNECTED)
		{
			continue;
		}
		cl->ps.persistant[PERS_SCORE] = 0;

		//@Fenix - clear timer values
		if (g_gametype.integer == GT_JUMP) {
			cl->ps.persistant[PERS_JUMPSTARTTIME] = 0;
			cl->ps.persistant[PERS_JUMPBESTTIME] = 0;
		}
	}

	// we need to do this here before chaning to CON_CONNECTING
	G_WriteSessionData();

	// change all client states to connecting, so the early players into the
	// next level will know the others aren't done reconnecting
	for (i = 0 ; i < g_maxclients.integer ; i++)
	{
		if (level.clients[i].pers.connected == CON_CONNECTED)
		{
			level.clients[i].pers.connected = CON_CONNECTING;
		}
	}
}

/**
 * $(function)
 */
void g_CycleMap(void)
{
	if (strlen(g_NextMap.string) > 0)
	{
		char  mapname[128];

		//Make sure we go back here after we're done
		trap_Cvar_VariableStringBuffer( "mapname", mapname, sizeof(mapname));
		trap_Cvar_Set("g_nextCycleMap", mapname);

		//Jump to this map
		strcpy(mapname, g_NextMap.string);
		trap_Cvar_Set("g_NextMap", "");
		trap_SendConsoleCommand( EXEC_APPEND, va("map %s\n", mapname));
	}
	else
	{
		if (!utMapCycleGotoNext ( ))
		{
		}
	}
}

/*
=================
G_LogPrintf

Print to the logfile with a time stamp if it is open
=================
*/
void QDECL G_LogPrintf( const char *fmt, ... )
{
	va_list  argptr;
	char	 string[2048]; //@Barbatos: was 1024
	int  min, tens, sec;

	sec  = level.time / 1000;

	min  = sec / 60;
	sec -= min * 60;
	tens = sec / 10;
	sec -= tens * 10;

	Com_sprintf( string, sizeof(string), "%3i:%i%i ", min, tens, sec );

	va_start( argptr, fmt );
	vsprintf( string + 7, fmt, argptr );
	va_end( argptr );

	if (g_dedicated.integer)
	{
		G_Printf( "%s", string + 7 );
	}

	if (!level.logFile)
	{
		return;
	}

	trap_FS_Write( string, strlen( string ), level.logFile );
}

/*
================
LogExit

Append information about this game to the log file
================
*/
void LogExit( const char *string )
{
	int    i, numSorted;
	gclient_t  *cl;

	G_LogPrintf( "Exit: %s\n", string );

	// Make sure we clear the survivor stuff so it doesnt reset before we quit
//	level.survivorRestart = qfalse;
//	level.survivorStartTime = 0;
	//level.survivorRoundRestartTime = 0;

	level.intermissionQueued = level.time - 50;

	// this will keep the clients from playing any voice sounds
	// that will get cut off when the queued intermission starts
	trap_SetConfigstring( CS_INTERMISSION, "1" );

	// don't send more than 32 scores (FIXME?)
	numSorted = level.numConnectedClients;

	if (numSorted > 32)
	{
		numSorted = 32;
	}

	if ((g_gametype.integer >= GT_TEAM) && (g_gametype.integer != GT_JUMP))
	{
		G_LogPrintf( "red:%i  blue:%i\n",
				 level.teamScores[TEAM_RED], level.teamScores[TEAM_BLUE] );
	}

	for (i = 0 ; i < numSorted ; i++)
	{
		int  ping;

		cl = &level.clients[level.sortedClients[i]];

		if (cl->sess.sessionTeam == TEAM_SPECTATOR)
		{
			continue;
		}

		if (cl->pers.connected != CON_CONNECTED)
		{
			continue;
		}

		ping = cl->ps.ping < 999 ? cl->ps.ping : 999;

		G_LogPrintf( "score: %i  ping: %i  client: %i %s\n", cl->ps.persistant[PERS_SCORE], ping, level.sortedClients[i], cl->pers.netname );
	}
}

/*
=================
CheckIntermissionExit

The level will stay at the intermission for a minimum of 5 seconds
If all players wish to continue, the level will then exit.
If one or more players have not acknowledged the continue, the game will
wait 10 seconds before going on.
=================
*/
void CheckIntermissionExit( void )
{
	int    ready, notReady;
	int    i;
	gclient_t  *cl;
	int    readyMask;

	// see which players are ready
	ready	  = 0;
	notReady  = 0;
	readyMask = 0;

	for (i = 0 ; i < g_maxclients.integer ; i++)
	{
		cl = level.clients + i;

		if (cl->pers.connected != CON_CONNECTED)
		{
			continue;
		}

		if (g_entities[cl->ps.clientNum].r.svFlags & SVF_BOT)
		{
			continue;
		}

		if (cl->readyToExit)
		{
			ready++;

			if (i < 16)
			{
				readyMask |= 1 << i;
			}
		}
		else
		{
			notReady++;
		}
	}

	// copy the readyMask to each player's stats so
	// it can be displayed on the scoreboard
/*	for (i=0 ; i< g_maxclients.integer ; i++)
   {
		cl = level.clients + i;
		if ( cl->pers.connected != CON_CONNECTED ) {
			continue;
		}
		cl->ps.stats[STAT_CLIENTS_READY] = readyMask;
	} */

	// never exit in less than five seconds
	if (level.time < level.intermissiontime + 5000)
	{
		return;
	}

	// if nobody wants to go, clear timer
/*
	if ( !ready ) {
		level.readyToExit = qfalse;
		return;
	}
*/

	// if everyone wants to go, go now
	if (!notReady)
	{
		ExitLevel();
		return;
	}

	// the first person to ready starts the ten second timeout
	if (!level.readyToExit)
	{
		level.readyToExit = qtrue;
		level.exitTime	  = level.time;
	}

	// if we have waited ten seconds since at least one player
	// wanted to exit, go ahead
	if (level.time < level.exitTime + 10000)
	{
		return;
	}

	ExitLevel();
}

/*
=============
ScoreIsTied
=============
*/
qboolean ScoreIsTied( void )
{
	int  a, b;

	if (level.numPlayingClients < 2)
	{
		return qfalse;
	}

	if (g_suddendeath.integer == 0)
	{
		return qfalse;
	}

	if ((g_gametype.integer >= GT_TEAM) && (g_gametype.integer != GT_JUMP))
	{
		return level.teamScores[TEAM_RED] == level.teamScores[TEAM_BLUE];
	}

	if (g_gametype.integer == GT_JUMP) {
		a = level.clients[level.sortedClients[0]].ps.persistant[PERS_JUMPBESTTIME];
		b = level.clients[level.sortedClients[1]].ps.persistant[PERS_JUMPBESTTIME];
	} else {
		a = level.clients[level.sortedClients[0]].ps.persistant[PERS_SCORE];
		b = level.clients[level.sortedClients[1]].ps.persistant[PERS_SCORE];
	}

	if (a == b) {
		return qtrue;
	} else {
		return qfalse;
	}
}

/*
=================
CheckExitRules

There will be a delay between the time the exit is qualified for
and the time everyone is moved to the intermission spot, so you
can see the last frag.
=================
*/
void CheckExitRules( void )
{
	int    i;
	gclient_t  *cl;
	qboolean   fraglimit	= qfalse;
	qboolean   capturelimit = qfalse;

	// if at the intermission, wait for all non-bots to
	// signal ready, then go to next level
	if (level.intermissiontime)
	{
		CheckIntermissionExit ();
		return;
	}

	if (level.intermissionQueued)
	{
#ifdef MISSIONPACK
		int  time = (g_singlePlayer.integer) ? SP_INTERMISSION_DELAY_TIME : INTERMISSION_DELAY_TIME;

		if (level.time - level.intermissionQueued >= time)
		{
			level.intermissionQueued = 0;
			BeginIntermission();
		}
#else
		if (level.time - level.intermissionQueued >= INTERMISSION_DELAY_TIME)
		{
			level.intermissionQueued = 0;
			BeginIntermission();
		}
#endif
		return;
	}

	if (ScoreIsTied())
	{
		return;
	}

	if (g_maxrounds.integer && g_survivor.integer)
	{
		if ((level.g_roundcount >= g_maxrounds.integer) && (level.survivorRoundRestartTime - 4500 > level.time))
		{
			trap_SendServerCommand( -1, "print \"Roundlimit hit.\n\"");
			LogExit( "Roundlimit hit." );
			return;
		}
	}
	else
	if (g_timelimit.integer && !level.warmupTime)
	{
		// If we are in survivor then only let the timelimit hit on the end of a round
		if (!g_survivor.integer || level.survivorRoundRestartTime)
		{
			if (level.time - level.survivorStartTime >= g_timelimit.integer * 60000)
			{
				trap_SendServerCommand( -1, "print \"Timelimit hit.\n\"");
				LogExit( "Timelimit hit." );
				return;
			}
		}
	}

	if (level.numPlayingClients < 2)
	{
		return;
	}

	// check for sudden death
	// d8 24FIX we don't want suddent death because it stops
	// CTF game modes from working if the flags can;t be capped
	// if we want sudden death it needs to be done more
	// elegantly on a per-game mode basis

	if ((g_gametype.integer <= GT_UT_ASSASIN) && g_fraglimit.integer)
	{
		fraglimit = qtrue;
	}
	else if (g_gametype.integer > GT_UT_ASSASIN && g_capturelimit.integer)
	{
		capturelimit = qtrue;
	}

	if(fraglimit)
	{
		if (level.teamScores[TEAM_RED] >= g_fraglimit.integer)
		{
			trap_SendServerCommand( -1, "print \"Red hit the fraglimit.\n\"" );
			LogExit( "Fraglimit hit." );
			return;
		}

		if (level.teamScores[TEAM_BLUE] >= g_fraglimit.integer)
		{
			trap_SendServerCommand( -1, "print \"Blue hit the fraglimit.\n\"" );
			LogExit( "Fraglimit hit." );
			return;
		}

		for (i = 0 ; i < g_maxclients.integer ; i++)
		{
			cl = level.clients + i;

			if (cl->pers.connected != CON_CONNECTED)
			{
				continue;
			}

			if (cl->sess.sessionTeam != TEAM_FREE)
			{
				continue;
			}

			if (cl->ps.persistant[PERS_SCORE] >= g_fraglimit.integer)
			{
				LogExit( "Fraglimit hit." );
				trap_SendServerCommand( -1, va("print \"%s" S_COLOR_WHITE " hit the fraglimit.\n\"",
								   cl->pers.netname ));
				return;
			}
		}
	}

	if (capturelimit)
	{
		if (level.teamScores[TEAM_RED] >= g_capturelimit.integer)
		{
			trap_SendServerCommand( -1, "print \"Red hit the capturelimit.\n\"" );
			LogExit( "Capturelimit hit." );
			return;
		}

		if (level.teamScores[TEAM_BLUE] >= g_capturelimit.integer)
		{
			trap_SendServerCommand( -1, "print \"Blue hit the capturelimit.\n\"" );
			LogExit( "Capturelimit hit." );
			return;
		}
	}
}

/*
========================================================================

FUNCTIONS CALLED EVERY FRAME

========================================================================
*/

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : CheckCaptureAndHold
// Description: Adds capture and hold scores for each capture point held
// Author	  : Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
void CheckCaptureAndHold ( void )
{
	gentity_t  *te	  = NULL;
	int 	   j	  = 0;

	if (g_gametype.integer != GT_UT_CAH)
	{
		return;
	}

	if (g_survivor.integer)
	{
		return;
	}

	if (level.intermissionQueued || level.intermissiontime)
	{
		return;
	}

	if ((level.time < level.nextScoreTime))
	{
		return;
	}

	AddTeamScore ( TEAM_RED, level.redCapturePointsHeld );
	AddTeamScore ( TEAM_BLUE, level.blueCapturePointsHeld );

	/*if(red || blue)
	{
		te = G_TempEntity( origin, EV_UT_CAPTURE_SCORE );
		te->r.svFlags |= SVF_BROADCAST;
		te->s.time = level.redCapturePointsHeld;
		te->s.time2 = level.blueCapturePointsHeld;
	}*/

	// Add scores again next minute
	level.nextScoreTime = level.time + g_cahTime.integer * 1000;
	//level.nextScoreTime = level.time + 1000;

	// Set the next score time
	trap_SetConfigstring( CS_CAPTURE_SCORE_TIME, va("%i", level.nextScoreTime ) );

	//reset all the flags
	for (j = 0; j < MAX_GENTITIES; j++)
	{
		te = &g_entities[j];

		if (!te->inuse)
		{
			continue;
		}

		// If it doesnt have a classname or isnt a mover, skip it
		if(!te->classname)
		{
			continue;
		}

//		  if(!Q_stricmp(te->classname, "team_CTF_neutralflag"))
		if(te->classhash==HSH_team_CTF_neutralflag)
		{
			te->s.modelindex = TEAM_FREE;
			//te->s.time2 = j;
			trap_SetConfigstring( CS_FLAGS + te->s.time2,
								  va("%i %i %f %f %f",
									 te->s.number,
									 te->s.modelindex,
									 te->s.origin[0],
									 te->s.origin[1],
									 te->s.origin[2] ));
		}
	}

	level.blueCapturePointsHeld=0;
	level.redCapturePointsHeld=0;
	Team_ReturnFlagSound(NULL,TEAM_RED);
	Team_ReturnFlagSound(NULL,TEAM_BLUE);
}

/**
 * $(function)
 */
void CycleAssassinsLeader ( team_t team )
{
	int  leader;
	int  i;

	if (g_gametype.integer != GT_UT_ASSASIN)
	{
		return;
	}

	leader = TeamLeader ( team );

	// Take away leadership from the guy who died
	if (leader != -1)
	{
		level.clients[leader].sess.teamLeader = qfalse;
	}

	// Find a new team leader now
	for (i = 0, leader++; i < MAX_CLIENTS; i++, leader++)
	{
		if (leader >= MAX_CLIENTS)
		{
			leader = 0;
		}

		// The next team leader was found
		if ((level.clients[leader].pers.connected == CON_CONNECTED) &&
			(level.clients[leader].sess.sessionTeam == team) && (!level.clients[leader].pers.substitute))
		{
			break;
		}
	}

	// If we found a new team leader then set it
	if (i < MAX_CLIENTS)
	{
		level.clients[leader].sess.teamLeader = qtrue;
	}
	// Hack check
	// remove any other leaders
/*	for( i = 0; i < MAX_CLIENTS; i++ ) {
		if( level.clients[ i ].sess.sessionTeam ==	TEAM_RED ) {
			if( level.clients[ i ].sess.teamLeader && j <= 0 )
				j++;
			else
				level.clients[ i ].sess.teamLeader = qfalse;
		}
		else if( level.clients[ i ].sess.sessionTeam ==  TEAM_BLUE ) {
			if( level.clients[ i ].sess.teamLeader && k <= 0 )
				k++;
			else
				level.clients[ i ].sess.teamLeader = qfalse;
		}
	} */
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : CheckAssasinRoundOver
// Description: Checks to see if an assasin round is over
// Author	  : Apoxol - Modded by DensitY
///////////////////////////////////////////////////////////////////////////////////////////
/*
	Below Rules Code needs Rewriting! currently she's a mess - DensitY
*/

/*=---- Check if the winning team Owned the losing team ----=*/

int CheckForOwned(void)
{
	int  IsOwned;
	int  Difference;

	IsOwned = 0;

	if(level.survivorBlueDeaths == level.survivorRedDeaths)
	{
		return IsOwned;
	}

	if((level.survivorBlueDeaths != 0) && (level.survivorRedDeaths != 0))
	{
		return IsOwned;
	}
	Difference = level.survivorBlueDeaths - level.survivorRedDeaths;

	if(Difference < 0)
	{
		Difference = -Difference;
	}

	if(Difference >= 2)
	{
		int  RandValue = (rand() % 200);

		if((RandValue >= 140) && (RandValue <= 150))
		{
			IsOwned = 2;
		}
		else
		{
			IsOwned = 1;
		}
	}
	return IsOwned;
}

/**
 * $(function)
 */
qboolean CheckAssasinRoundOver ( void ) // Added Logging
{
	int   i;
	qboolean  redWin	 = qfalse;
	qboolean  blueWin	 = qfalse;

	// Dont even bother checking anything until the game is 5 seconds in
	if (level.time - level.startTime < 5 * 1000)
	{
		return qfalse;
	}

	// If there is no team leader
/*	if ( -1 == blueLeader || -1 == redLeader )
	{
		CheckTeamLeader ( TEAM_RED );
		CheckTeamLeader ( TEAM_BLUE );

		blueLeader = TeamLeader ( TEAM_BLUE );
		redLeader  = TeamLeader ( TEAM_RED );
	} */

	// See if the red team is a winner
	if (level.redCapturePointsHeld)
	{
		redWin = qtrue;
	}

	// See if the blue team is a winner
	if (level.blueCapturePointsHeld)
	{
		blueWin = qtrue;
	}

	// See if anyone won
	if (redWin && blueWin)
	{
		G_BroadcastRoundWinner ( TEAM_FREE, NULL );
		G_LogPrintf( "AssassinWinner: Draw\n" );
		return qtrue;
	}
	else
	if (redWin || blueWin)
	{
		int leader;

		leader = TeamLeader ( redWin ? TEAM_RED : TEAM_BLUE );
		AddTeamScore ( redWin ? TEAM_RED : TEAM_BLUE, 5 );
		G_BroadcastRoundWinner ( redWin ? TEAM_RED : TEAM_BLUE, "reached the flag" );
		level.survivorWinTeam = redWin ? TEAM_RED : TEAM_BLUE;

		if(redWin)
		{
			G_LogPrintf( "AssassinWinner: Red Reached the Flag\n" );
			level.clients[leader].ps.persistant[PERS_SCORE] += 3; // 3 frags for capping
		}
		else if(blueWin)
		{
			G_LogPrintf( "AssassinWinner: Blue Reached the Flag\n" );
			level.clients[leader].ps.persistant[PERS_SCORE] += 3; // 3 frags for capping
		}

//		CycleAssassinsLeader ( redWin?TEAM_BLUE:TEAM_RED );
		for (i = 0 ; i < level.maxclients ; i++)
		{
			gclient_t  *client;

			client = &level.clients[i];

			if (client->pers.connected != CON_CONNECTED)
			{
				continue;
			}

			if(redWin) // No owned in Flag cap wins
			{
				G_AddEvent( &g_entities[client->ps.clientNum], EV_UT_REDWINSROUND_SOUND, 0 );	// CheckForOwned()
			}
			else if(blueWin)
			{
				G_AddEvent( &g_entities[client->ps.clientNum], EV_UT_BLUEWINSROUND_SOUND, 0 );
			}
		}
		// If the winning teams leader died, then cycle them too
/*		if ( -1 != (leader = TeamLeader ( redWin?TEAM_RED:TEAM_BLUE ) ) )
	  {
			if ( level.clients[leader].pers.connected != CON_CONNECTED ||
				level.clients[leader].ps.pm_type != PM_NORMAL ) {
//				CycleAssassinsLeader ( redWin?TEAM_RED:TEAM_BLUE );
			}
	  } */

		return qtrue;
	}

	return qfalse;
}

/*=---- Bomb gamemode round end check ----=*/

qboolean CheckBombRoundOver(void)
{
	qboolean  RedWins  = qfalse;
	qboolean  BlueWins = qfalse;
	int   i;

	if(level.BombGoneOff == qtrue)
	{
		G_BroadcastRoundWinner( level.AttackingTeam, "Blew up the target" );

		// Bomb exploded.
//	if (g_bombRules.integer == 0)
		{
			AddTeamScore(level.AttackingTeam, 1);
		}
//	else
//	{
//	   AddTeamScore(level.AttackingTeam, 2);
//	}

		RedWins = qtrue;
	}
	else if(level.BombDefused == qtrue)
	{
		G_BroadcastRoundWinner((level.AttackingTeam == TEAM_RED ? TEAM_BLUE : TEAM_RED), "defused the bomb" );
		AddTeamScore ( (level.AttackingTeam == TEAM_RED ? TEAM_BLUE : TEAM_RED), 1 );
		BlueWins = qtrue;
	}

	for (i = 0 ; i < level.maxclients ; i++)
	{
		gclient_t  *client;

		client = &level.clients[i];

		if(client->pers.connected != CON_CONNECTED)
		{
			continue;
		}

		if(RedWins == qtrue)
		{
			G_AddEvent( &g_entities[client->ps.clientNum], EV_UT_REDWINSROUND_SOUND, 0 );
		}
		else if(BlueWins == qtrue)
		{
			G_AddEvent( &g_entities[client->ps.clientNum], EV_UT_BLUEWINSROUND_SOUND, 0 );
		}
	}

	if((RedWins == qtrue) || (BlueWins == qtrue))
	{
		return qtrue;
	}
	return qfalse;
}

//27: Lil function to pop dead players back into the game.
void RespawnDeadPlayers(void)
{
	int    j;
	gentity_t  *who;

	for(j = 0; j < MAX_CLIENTS; j++)
	{
		who = g_entities + j;

		if (who->client && who->inuse)
		{
			if (who->client->pers.connected != CON_CONNECTED)
			{
				continue;
			}

			//@Barbatos: if a client is a sub
			if(who->client->pers.substitute)
				continue;

			// If client is ! a  spectator.
			if ((who->client->sess.sessionTeam == TEAM_RED) ||
				(who->client->sess.sessionTeam == TEAM_BLUE) ||
				((who->client->sess.sessionTeam == TEAM_FREE) && (g_gametype.integer < GT_TEAM))) //@Barbatos: added for LMS
			{
				if (level.time - (who->client->respawnTime) > 0)
				{
					//If client is dead
					if ((who->client->ps.pm_type >= PM_DEAD) || who->client->ghost)
					{
						if (!who->client->pers.substitute)
						{
							respawn(who); //so long as they're not a sub
						}
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : CheckSurvivorRoundOver
// Description: Checks to see if the current round is over and restarts the map
// Author	  : Apoxol & DensitY
///////////////////////////////////////////////////////////////////////////////////////////

qboolean CheckSurvivorRoundOver(void)
{
	vec3_t	   origin	= { 0, 0, 0};
	qboolean   roundOver	= qfalse;
	qboolean   BombContinue = qfalse;		   // temp hack
	int 	   i;
	gentity_t  *te;
	gclient_t  *client;

	//@Barbatos: commented out, wtf is that?
	/*if (g_matchMode.integer && !(level.matchState & UT_MATCH_BR && level.matchState & UT_MATCH_RR))
		return qfalse;*/

	if ((g_gametype.integer < GT_TEAM) || (g_gametype.integer == GT_JUMP))
	{
		int  blue	   = TeamCount ( -1, TEAM_FREE );
		int  blueGhost = GhostCount ( -1, TEAM_FREE );

		if (blue - blueGhost == 1)
		{
			//we  have a winner - but who?
			for (i = 0 ; i < level.maxclients ; i++)
			{
				client = &level.clients[i];

				if (client->pers.connected == CON_DISCONNECTED)
				{
					continue;
				}

				//@Barbatos
				if(client->pers.substitute)
					continue;

				//@Barbatos: bugfix when a spectator can be chosen as a winner
				if(client->sess.sessionTeam == TEAM_SPECTATOR)
					continue;

				if (client->ghost || (client->ps.stats[STAT_HEALTH] <= 0))
				{
					continue;
				}

				trap_SetConfigstring( CS_ROUNDWINNER, va( "Round Won by %s", client->pers.netname));

				//@Barbatos
				if(g_gametype.integer == GT_LASTMAN) {
					AddScore(&g_entities[client->ps.clientNum], g_entities[client->ps.clientNum].r.currentOrigin, LMS_BONUS);
					//@Fenix - added SurvivorWinner log for LMS
					G_LogPrintf("SurvivorWinner: %d\n", client->ps.clientNum);
				}

				break;
			}
			/*te		  = G_TempEntity( origin, EV_GLOBAL_TEAM_SOUND );
			te->s.eventParm = GTS_RED_CAPTURE;
			te->r.svFlags  |= SVF_BROADCAST;*/
			roundOver	= qtrue;
		}
		else if (blue == blueGhost)
		{
			trap_SetConfigstring( CS_ROUNDWINNER, va( "Round Drawn"));
			te		= G_TempEntity( origin, EV_UT_DRAWNROUND_SOUND );
			te->s.eventParm = GTS_RED_CAPTURE;
			te->r.svFlags  |= SVF_BROADCAST;
			roundOver	= qtrue;
		}
	}

	if ((g_gametype.integer >= GT_TEAM) && (g_gametype.integer != GT_JUMP))
	{
		int  blue	   = TeamCount ( -1, TEAM_BLUE );
		int  red	   = TeamCount ( -1, TEAM_RED );
		int  blueGhost = GhostCount ( -1, TEAM_BLUE );
		int  redGhost  = GhostCount ( -1, TEAM_RED );

		// If the round isnt over yet then check survivor win
		// conditions from other gametypes.
		if (red && blue)
		{
			switch (g_gametype.integer)
			{
				// Check for an assasin victory
				case GT_UT_ASSASIN:
					roundOver = CheckAssasinRoundOver ( );
					break;

				// Check for a bomb victory
				case GT_UT_BOMB:

					if(level.BombMapAble == qtrue)
					{
						roundOver = CheckBombRoundOver();
					}
					break;
			}
		}

		if (!roundOver)
		{
			if((blue && (blue - blueGhost == 0)) || ((red && red - redGhost) == 0))
			{
				int  alive = GetAliveClient ( 0 );

				if((alive != -1) && (level.clients[alive].sess.sessionTeam != TEAM_SPECTATOR))
				{
					if (red && blue)
					{
						// Look for second point in an assasss game
						if (g_gametype.integer == GT_UT_ASSASIN)
						{
							int  leader;
							leader = TeamLeader ( level.clients[alive].sess.sessionTeam );

							if (leader != -1)
							{
								if ((level.clients[leader].pers.connected != CON_DISCONNECTED) &&
									(level.clients[leader].ps.pm_type == PM_NORMAL))
								{
									AddTeamScore ( level.clients[alive].sess.sessionTeam, 2 );
								}
							}

							// Loosing team always cycles leaders
							//CycleAssassinsLeader ( level.clients[alive].sess.sessionTeam==TEAM_BLUE?TEAM_RED:TEAM_BLUE );

							// If the winning teams leader died, then cycle them too
							if (-1 != (leader = TeamLeader ( level.clients[alive].sess.sessionTeam )))
							{
								if ((level.clients[leader].pers.connected != CON_CONNECTED) ||
									(level.clients[leader].ps.pm_type != PM_NORMAL))
								{
								}
							}
							//CycleAssassinsLeader ( level.clients[alive].sess.sessionTeam );
						}

						if(level.clients[alive].sess.sessionTeam == TEAM_RED)
						{
							level.survivorWinTeam = TEAM_RED;
							level.BombDefused	  = qtrue;
							G_BroadcastRoundWinner ( level.survivorWinTeam, NULL );
							BombContinue		  = qfalse;
							G_LogPrintf( "SurvivorWinner: Red\n" );
						}
						else
						if(level.clients[alive].sess.sessionTeam == TEAM_BLUE && level.BombPlanted == qfalse)
						{
							level.survivorWinTeam = TEAM_BLUE;
							level.BombDefused	  = qtrue;
							BombContinue		  = qfalse;
							G_BroadcastRoundWinner ( level.survivorWinTeam, NULL );
							G_LogPrintf( "SurvivorWinner: Blue\n" );
						}
						else
						{
							level.survivorWinTeam = TEAM_SPECTATOR;
							//level.BombDefused = qtrue;
							BombContinue		  = qtrue;
							//G_BroadcastRoundWinner ( level.survivorWinTeam, NULL );
							//G_LogPrintf( "SurvivorWinner: Red\n" );
						}

						for (i = 0 ; i < level.maxclients ; i++)
						{
							gclient_t  *client;

							client = &level.clients[i];

							if (client->pers.connected != CON_CONNECTED)
							{
								continue;
							}

							switch(level.survivorWinTeam)
							{
								case TEAM_RED:
									G_AddEvent( &g_entities[client->ps.clientNum], EV_UT_REDWINSROUND_SOUND, CheckForOwned());
									break;

								case TEAM_BLUE:
									G_AddEvent( &g_entities[client->ps.clientNum], EV_UT_BLUEWINSROUND_SOUND, CheckForOwned());
									break;

								case TEAM_SPECTATOR:
								default:
									break;
							}
						}

						if(g_gametype.integer == GT_UT_BOMB)
						{
							if(!BombContinue)
							{
								// Red/Blue team killed the other team.
								if (level.AttackingTeam == level.clients[alive].sess.sessionTeam)
								{
//				 if (g_bombRules.integer == 0)
									{
										AddTeamScore(level.clients[alive].sess.sessionTeam, 1);
									}
/*				 else
			   {
				  // If bomb was planted.
				  if (level.BombPlanted)
				  {
				 AddTeamScore(level.clients[alive].sess.sessionTeam, 2);
				  }
				  else
				  {
				 AddTeamScore(level.clients[alive].sess.sessionTeam, 1);
				  }
			   } */
								}
								else
								{
//				 if (g_bombRules.integer == 0)
									{
										AddTeamScore(level.clients[alive].sess.sessionTeam, 1);
									}
									//			   else
									//			 {
									//			  AddTeamScore(level.clients[alive].sess.sessionTeam, 2);
									//			 }
								}

								roundOver = qtrue;
							}
						}
						else
						{
							AddTeamScore ( level.clients[alive].sess.sessionTeam, 1 );
							roundOver = qtrue;
						}
					}
				}
				else
				{
					if((level.BombPlanted == qtrue) && (g_gametype.integer == GT_UT_BOMB)) // special check if bomb is planted then red wins!
					{
						G_BroadcastRoundWinner ( TEAM_RED, NULL );
						G_LogPrintf( "SurvivorWinner: Red\n" );

						for(i = 0; i < level.maxclients; i++)
						{
							gclient_t  *client;

							client = &level.clients[i];

							if(client->pers.connected != CON_CONNECTED)
							{
								continue;
							}
							G_AddEvent( &g_entities[client->ps.clientNum], EV_UT_REDWINSROUND_SOUND, 0 );
						}
						AddTeamScore ( TEAM_RED, 1 );
						roundOver = qtrue;
					}
					else
					{
						G_BroadcastRoundWinner ( TEAM_FREE, NULL );
						G_LogPrintf( "SurvivorWinner: Draw\n" );

						for(i = 0; i < level.maxclients; i++)
						{
							gclient_t  *client;

							client = &level.clients[i];

							if(client->pers.connected != CON_CONNECTED)
							{
								continue;
							}
							G_AddEvent( &g_entities[client->ps.clientNum], EV_UT_DRAWNROUND_SOUND, 0 );
						}
						roundOver = qtrue;
					}
				}
			}
		}
	}

	// See if the round ended due to time
	if((level.time > level.roundEndTime) && (level.BombPlanted == qfalse) && (level.BombGoneOff == qfalse))
	{
		qboolean  IsADraw = qtrue;

		//Gamemode specific stuff
		switch (g_gametype.integer)
		{
			case GT_UT_CAH:
				{
					//Red has more points
					if(level.redCapturePointsHeld > level.blueCapturePointsHeld)
					{
						G_BroadcastRoundWinner ( TEAM_RED, NULL );
						level.survivorWinTeam = TEAM_RED;
						G_LogPrintf( "SurvivorWinner: Red\n" );

						for(i = 0 ; i < level.maxclients ; i++)
						{
							gclient_t  *client;
							client = &level.clients[i];

							if(client->pers.connected != CON_CONNECTED)
							{
								continue;
							}
							G_AddEvent( &g_entities[client->ps.clientNum], EV_UT_REDWINSROUND_SOUND, 0 );
						}
					}
					else //Blue has more points
					if(level.blueCapturePointsHeld > level.redCapturePointsHeld)
					{
						G_BroadcastRoundWinner ( TEAM_BLUE, NULL );
						level.survivorWinTeam = TEAM_BLUE;
						G_LogPrintf( "SurvivorWinner: Blue\n" );

						for(i = 0; i < level.maxclients; i++)
						{
							gclient_t  *client;
							client = &level.clients[i];

							if(client->pers.connected != CON_CONNECTED)
							{
								continue;
							}
							G_AddEvent( &g_entities[client->ps.clientNum], EV_UT_BLUEWINSROUND_SOUND, 0 );
						}
					}
					else //Else it was a draw
					{
						G_BroadcastRoundWinner ( TEAM_FREE, NULL );
						G_LogPrintf( "SurvivorWinner: Draw\n" );

						for(i = 0; i < level.maxclients; i++)
						{
							gclient_t  *client;
							client = &level.clients[i];

							if(client->pers.connected != CON_CONNECTED)
							{
								continue;
							}
							G_AddEvent( &g_entities[client->ps.clientNum], EV_UT_DRAWNROUND_SOUND, 0 );
						}
					}
					AddTeamScore ( TEAM_RED, level.redCapturePointsHeld);
					AddTeamScore ( TEAM_BLUE, level.blueCapturePointsHeld);
					break;
				}

			case GT_UT_BOMB:
			case GT_UT_SURVIVOR:
				{
					if (g_gametype.integer == GT_UT_BOMB)
					{
						// Draw/Time ran out.
//		 if (g_bombRules.integer == 0)
						{
							IsADraw = qfalse;
						}
					}

					if (!level.survivorNoJoin && (level.time - level.startTime > 30 * 1000))
					{
						level.survivorNoJoin = qtrue;
					}

									//See if we use the TS rules or the Bomb Mode Rules
					if (level.BombMapAble == qtrue) //wont ever be true in TS
					{
						if(level.BombPlanted == qtrue) // NEver Happens!
						{
							G_BroadcastRoundWinner( TEAM_RED, "bomb wasn't not defused in time" );
							level.BombDefused = qtrue;

							for(i = 0; i < level.maxclients; i++)
							{
								gclient_t  *client;
								client = &level.clients[i];

								if(client->pers.connected != CON_CONNECTED)
								{
									continue;
								}
								G_AddEvent( &g_entities[client->ps.clientNum], EV_UT_REDWINSROUND_SOUND, 0 );
							}
						}
						else // Do a team survivor style thing?
						{
							// Draw/Time ran out.
//			if (g_bombRules.integer == 0)
							{
								G_BroadcastRoundWinner((level.AttackingTeam == TEAM_RED ? TEAM_BLUE : TEAM_RED), "stopped Attack team from completing objective" );
								level.BombDefused = qtrue;
								AddTeamScore ( (level.AttackingTeam == TEAM_RED ? TEAM_BLUE : TEAM_RED), 1 );

								for(i = 0; i < level.maxclients; i++)
								{
									gclient_t  *client;
									client = &level.clients[i];

									if(client->pers.connected != CON_CONNECTED)
									{
										continue;
									}
									G_AddEvent( &g_entities[client->ps.clientNum], (level.AttackingTeam == TEAM_RED ? EV_UT_BLUEWINSROUND_SOUND : EV_UT_REDWINSROUND_SOUND), 0 );
								}
							}
							/*		  else
									{
									   level.BombDefused = qtrue;
									} */
						}
					}
					else
					{
						//#27 for 2.4 - TEAM SURVIVOR TIE
						//Figure out who had the least ammount of players dead
						if (g_survivorrule.integer)
						{
							if (level.survivorRedDeaths < level.survivorBlueDeaths) //red had less dead than blue
							{
								G_BroadcastRoundWinner ( TEAM_RED, "had the least casualties" );
								G_LogPrintf( "SurvivorWinner: Red\n" );
								AddTeamScore ( TEAM_RED, 1);

								for(i = 0; i < level.maxclients; i++)
								{
									gclient_t  *client;
									client = &level.clients[i];

									if(client->pers.connected != CON_CONNECTED)
									{
										continue;
									}
									G_AddEvent( &g_entities[client->ps.clientNum], EV_UT_REDWINSROUND_SOUND, 0 );
								}
								IsADraw = qfalse;
							}
							else
							if (level.survivorRedDeaths < level.survivorBlueDeaths) //blue had less dead than red
							{
								G_BroadcastRoundWinner ( TEAM_BLUE, "had the least casualties" );
								G_LogPrintf( "SurvivorWinner: Blue\n" );
								AddTeamScore ( TEAM_BLUE, 1);

								for(i = 0; i < level.maxclients; i++)
								{
									gclient_t  *client;
									client = &level.clients[i];

									if(client->pers.connected != CON_CONNECTED)
									{
										continue;
									}
									G_AddEvent( &g_entities[client->ps.clientNum], EV_UT_BLUEWINSROUND_SOUND, 0 );
								}
								IsADraw = qfalse;
							}
						}
					}
					break;
				}
		}

		if (IsADraw)
		{
			G_BroadcastRoundWinner ( TEAM_FREE, NULL ); 	//draw
			G_LogPrintf( "SurvivorWinner: Draw\n" );

			for(i = 0; i < level.maxclients; i++)
			{
				gclient_t  *client;
				client = &level.clients[i];

				if(client->pers.connected != CON_CONNECTED)
				{
					continue;
				}
				G_AddEvent( &g_entities[client->ps.clientNum], EV_UT_DRAWNROUND_SOUND, 0 );
			}
		}
		roundOver = qtrue;
	}

	// IF the round is over then set the round restart time so everyone can
	// see who won before the next round starts
	if(roundOver)
	{
		int  i;

		// Clear the deaths flags
		// Send the score message to everyone playing
		for (i = 0 ; i < level.maxclients ; i++)
		{
			gclient_t  *client;

			client = &level.clients[i];

			if (client->pers.connected != CON_CONNECTED)
			{
				continue;
			}

			// If client is alive.
			if (!client->ghost && (client->ps.stats[STAT_HEALTH] > 0))
			{
				// Send scores.
				SendScoreboardSingleMessageToAllClients(g_entities + i);
			}
		}

		SendScoreboardGlobalMessageToAllClients();

		// If this is bomb game type.
		if (g_gametype.integer == GT_UT_BOMB)
		{
			// If bomb exploded.
			if (level.BombGoneOff)
			{
				// Restart in 12 seconds.
				level.survivorRoundRestartTime = level.time + 10000;
			}
			else
			{
				// Restart in 5 seconds.
				level.survivorRoundRestartTime = level.time + 5000;
			}
		}
		else
		{
			// Restart in 5 seconds.
			level.survivorRoundRestartTime = level.time + 5000;
		}

		// Reset cg.warmup on the clients.
		trap_SetConfigstring(CS_WARMUP, va("%i", 0)); //0

		return qtrue;
	}
	return qfalse;
}

/**
 * $(function)
 */
qboolean SecondHalf(void)
{
	if (g_swaproles.integer && (level.swaproles == 0))
	{
		return qtrue;
	}
	return qfalse;
}

/**
 * $(function)
 */
qboolean NotEnoughPlayersToPlay(void)
{
	int 	  counts[TEAM_NUM_TEAMS];
	qboolean  notEnough = qfalse;

	if ((g_gametype.integer == GT_FFA) || (g_gametype.integer == GT_JUMP))
	{
		return qfalse; // We can always play FFA
	}

	if (g_gametype.integer == GT_LASTMAN)
	{
		counts[TEAM_FREE] = TeamCount(-1, TEAM_FREE);

		if (counts[TEAM_FREE] > 1)
		{
			return qfalse;
		}
		return qtrue;
	}

	counts[TEAM_BLUE] = TeamCount( -1, TEAM_BLUE );
	counts[TEAM_RED]  = TeamCount( -1, TEAM_RED );

	/*if (g_gametype.integer == GT_JUMP)
	{
		if ((counts[TEAM_RED] > 0) || (counts[TEAM_BLUE] > 0))
		{
			return qfalse;
		}
	}*/

	if ((counts[TEAM_RED] < 1) || (counts[TEAM_BLUE] < 1))
	{
		notEnough = qtrue;
	}
	else
	if (level.numPlayingClients < 2)
	{
		notEnough = qtrue;
	}
	return notEnough;
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : CheckSurvivor
// Description: Checsk the status of a survivor game
// Author	  : Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
void CheckSurvivor ( void )
{
//	int 		counts[TEAM_NUM_TEAMS];
	qboolean  notEnough = qfalse;

	// Well, no players, no round to check
	if (level.numPlayingClients == 0)
	{
		return;
	}

	if (level.intermissionQueued || level.intermissiontime)
	{
		return;
	}

	// If we are waiting for the next round then there isnt anything to check.
	if(level.survivorRoundRestartTime)
	{
		// HACK! nasty last day release stuff :(
		if((level.warmupTime > 0) && (level.warmupTime < level.survivorRoundRestartTime))
		{
			level.warmupTime = level.survivorRoundRestartTime + 1000;
		}

		if(level.time > level.survivorRoundRestartTime)
		{
			G_InitWorldSession();
			level.survivorRoundRestartTime = 0;
			level.warmupTime		   = 0;
		}

		return;
	}

	// Determine how many people are on each team and whether we have enough
	// for a game.
	notEnough = NotEnoughPlayersToPlay();

	// If there arent enough players then make sure they dont play :)
	if (notEnough)
	{
		RespawnDeadPlayers();

		if (level.warmupTime != -1)
		{
			level.warmupTime = -1;
			trap_SetConfigstring( CS_WARMUP, va("%i", level.warmupTime));
			G_LogPrintf( "Warmup:\n" );  //Waiting for players
		}
		return;
	}

	if (level.warmupTime == 0)
	{
		CheckSurvivorRoundOver();
		return;
	}

	// if the warmup is changed at the console, restart it
	if (g_warmup.modificationCount != level.warmupModificationCount)
	{
		level.warmupModificationCount = g_warmup.modificationCount;
		level.warmupTime		  = -1;
	}

	// if all players have arrived, start the countdown
	if (level.warmupTime < 0)
	{
		// fudge by -1 to account for extra delays
		level.warmupTime = level.time + (g_warmup.integer) * 1000;
		trap_SetConfigstring( CS_WARMUP, va("%i", level.warmupTime));
		return;
	}

	// if the warmup time has counted down, restart
	if (level.time > level.warmupTime - 1000)
	{
		level.warmupTime = 0;

		//Instead of just restarting the round, restart the whole map
		Svcmd_Restart_f(qfalse, SecondHalf());
		//G_InitWorldSession();
	}
}

/*
=============
CheckTournament

Once a frame, check for changes in tournament player state
=============
*/
void CheckTournament( void ) {

	// check because we run 3 game frames before calling Connect and/or ClientBegin
	// for clients on a map_restart
	if (level.numPlayingClients == 0) {
		return;
	}

	if (level.warmupTime != 0) {

		qboolean  notEnough = NotEnoughPlayersToPlay();

		if (notEnough) {
			RespawnDeadPlayers();

			if (level.warmupTime != -1) {
				level.warmupTime = -1;
				trap_SetConfigstring( CS_WARMUP, va("%i", level.warmupTime));
				G_LogPrintf( "Warmup:\n" );
			}
			return; // still waiting for team members
		}

		if (level.warmupTime == 0) {
			return;
		}

		// if the warmup is changed at the console, restart it
		if (g_warmup.modificationCount != level.warmupModificationCount) {
			level.warmupModificationCount = g_warmup.modificationCount;
			level.warmupTime		  = -1;
		}

		// if all players have arrived, start the countdown
		if (level.warmupTime < 0) {

			// fudge by -1 to account for extra delays
			if ( g_warmup.integer > 1 ) {
			     level.warmupTime = level.time + ( g_warmup.integer - 1 ) * 1000;
			} else {
			     level.warmupTime = 0;
			}

			trap_SetConfigstring( CS_WARMUP, va("%i", level.warmupTime));
			return;
		}

		// if the warmup time has counted down, restart
		if (level.time > level.warmupTime - 1000) {
			// COMMENTED OUT BY HIGHSEA; WE DONT USE MAP_RESTART!
			//level.warmupTime += 10000;
			//trap_Cvar_Set( "g_restarted", "1" );
			//trap_SendConsoleCommand( EXEC_APPEND, "map_restart 0\n" );
			//level.restarted = qtrue;
			level.warmupTime = 0;
			Svcmd_Restart_f(qfalse, SecondHalf()); //G_InitWorldSession();
			return;
		}
	}
}


/*
==================
CheckVote
==================
*/
#if 1 // New Version - NOTE: Stull foobars, writing external test.. using oringal system

void CheckVote(qboolean veto)
{
	gentity_t  *entity;
	int 	   numVoteCapable;
	int 	   i;

	if (level.voteExecuteTime && (level.voteExecuteTime < level.time))
	{
		level.voteExecuteTime = 0;
		level.voteClientArg = -1;
		trap_SendConsoleCommand(EXEC_APPEND, va("%s\n", level.voteString));

		return;
	}

	if (!level.voteTime)
	{
		return;
	}

	numVoteCapable = 0;

	for (i = 0; i < level.maxclients; i++)
	{
		entity = &g_entities[i];

		if (!entity->client)
		{
			continue;
		}

		if (entity->client->pers.connected != CON_CONNECTED)
		{
			continue;
		}

		if (entity->r.svFlags & SVF_BOT)
		{
			continue;
		}

		//@Fenix - excluding spectators from the count only if they haven't voted yet
		//A player might have voted while in-game and then switched to spectators team.
		if ((entity->client->sess.sessionTeam == TEAM_SPECTATOR) && (!entity->client->voted)) {
			continue;
		}

		/*
		 if (entity->client->ps.clientNum != level.voteClient &&
			entity->client->sess.sessionTeam == TEAM_SPECTATOR)
			continue;
		*/

		numVoteCapable++;
	}

	if (((level.time - level.voteTime) >= VOTE_TIME) || ((level.voteYes + level.voteNo) == numVoteCapable) || (veto == qtrue))
	{
		if ((level.voteYes > level.voteNo) && (veto == qfalse))
		{
			trap_SendServerCommand(-1, "print \"Vote passed.\n\"");

			level.voteExecuteTime = level.time + 3000;

			if (level.voteClient >= 0)
			{
				level.clients[level.voteClient].sess.lastFailedVote = 0;
			}

			level.voteTime	 = 0;
			level.voteClient = -1;

			trap_SetConfigstring(CS_VOTE_TIME, "");
		}
		else
		{
			trap_SendServerCommand(-1, "print \"Vote failed.\n\"");

			if(level.voteClient >= 0)
			{
				level.clients[level.voteClient].sess.lastFailedVote = level.time;
			}

			level.voteTime	 = 0;
			level.voteClient = -1;

			trap_SetConfigstring(CS_VOTE_TIME, "");
		}

		for (i = 0; i < level.maxclients; i++)
		{
			entity = &g_entities[i];

			if (!entity->client)
			{
				continue;
			}

			entity->client->voted = qfalse;
		}

		level.voteClientArg = -1;
	}
}

/*=---- Checks the Current Vote in progress ----=*/
/*
void CheckVote(void) {
	int 			i;
	int 			NumOfVoters = 0;
	int 			NumOfBots = 0;
	int 			NumOfSpectators = 0;
	gentity_t		*ent;

	if( level.voteExecuteTime && level.voteExecuteTime < level.time ) {
		level.voteExecuteTime = 0;
		trap_SendConsoleCommand( EXEC_APPEND, va("%s\n", level.voteString ) );
	}
	if( !level.voteTime )
		return;
	for( i = 0; i < level.numConnectedClients; i++ ) {
		ent = &g_entities[ i ];
		if( ent->r.svFlags & SVF_BOT ) {
			NumOfBots++;
		}
		if( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
			NumOfSpectators++;
		}
		if( ent->client && !( ent->r.svFlags & SVF_BOT ) ) {
			if( ent->client->ps.eFlags & EF_VOTED ) {
				NumOfVoters++;
			}
		}
	}
	if( level.time - level.voteTime >= VOTE_TIME  ||  ( NumOfVoters + ( NumOfBots + NumOfSpectators ) ) > level.numConnectedClients ) {
		if( level.voteYes > ( float )( NumOfVoters / 2 ) && ( level.voteYes > level.voteNo ) ) {
			if( level.numConnectedClients == 2 && level.voteYes != 2 ) {
				trap_SendServerCommand( -1, "print \"Vote failed.\n\"" ); // Vote fails, we need 100% accept in this case!
				level.voteTime = 0;
				level.voteClient = -1;
				trap_SetConfigstring( CS_VOTE_TIME, "" );
				return;
			}
			else {
				trap_SendServerCommand( -1, "print \"Vote passed.\n\"" );
				level.voteExecuteTime = level.time + 3000;
				if( level.voteClient >= 0 ) {
					level.clients[ level.voteClient ].sess.lastFailedVote = 0;
				}
				level.voteTime = 0;
				level.voteClient = -1;
				trap_SetConfigstring( CS_VOTE_TIME, "" );
			}

		}
		else { //if( level.voteNo >= NumOfVoters / 2 ) {
			trap_SendServerCommand( -1, "print \"Vote failed.\n\"" );
			if( level.voteClient >= 0 ) {
				level.clients[ level.voteClient ].sess.lastFailedVote = level.time;
			}
			level.voteTime = 0;
			level.voteClient = -1;
			trap_SetConfigstring( CS_VOTE_TIME, "" );
		}
	}
}
*/

#else // Old Version

/**
 * $(function)
 */
void CheckVote( void )
{
	if (level.voteExecuteTime && (level.voteExecuteTime < level.time))
	{
		level.voteExecuteTime = 0;
		trap_SendConsoleCommand( EXEC_APPEND, va("%s\n", level.voteString ));
	}

	if (!level.voteTime)
	{
		return;
	}

	if (level.time - level.voteTime >= VOTE_TIME)
	{
		trap_SendServerCommand( -1, "print \"Vote failed.\n\"" );

		if (level.voteClient >= 0)
		{
			level.clients[level.voteClient].sess.lastFailedVote = level.time;
		}
	}
	else
	{
		if (level.voteYes > level.numVotingClients / 2)
		{
			// don't let one person win if only 2 are on
			if ((level.numConnectedClients == 2) && (level.voteYes != 2))
			{
				return;
			}

			// execute the command, then remove the vote
			trap_SendServerCommand( -1, "print \"Vote passed.\n\"" );
			level.voteExecuteTime = level.time + 3000;

			if (level.voteClient >= 0)
			{
				level.clients[level.voteClient].sess.lastFailedVote = 0;
			}
		}
		else if (level.voteNo >= level.numVotingClients / 2)
		{
			// same behavior as a timeout
			trap_SendServerCommand( -1, "print \"Vote failed.\n\"" );

			if (level.voteClient >= 0)
			{
				level.clients[level.voteClient].sess.lastFailedVote = level.time;
			}
		}
		else
		{
			// still waiting for a majority
			return;
		}
	}
	level.voteTime	 = 0;
	level.voteClient = -1;
	trap_SetConfigstring( CS_VOTE_TIME, "" );
}

#endif

/*
==================
PrintTeam
==================
*/
void PrintTeam(team_t team, char *message)
{
	int  i;

	for (i = 0 ; i < level.maxclients ; i++)
	{
		if(level.clients[i].pers.connected != CON_CONNECTED)
		{
			continue;
		}

		if (level.clients[i].sess.sessionTeam != team)
		{
			continue;
		}
		trap_SendServerCommand( i, message );
	}
}

/*
==================
SetLeader
==================
*/
void SetLeader(team_t team, int client)
{
	int  i;

	if (g_gametype.integer != GT_UT_ASSASIN)
	{
		return;
	}

	if (level.clients[client].pers.connected == CON_DISCONNECTED)
	{
		PrintTeam(team, va("print \"%s is not connected\n\"", level.clients[client].pers.netname));
		return;
	}

	if (level.clients[client].sess.sessionTeam != team)
	{
		PrintTeam(team, va("print \"%s is not on the team anymore\n\"", level.clients[client].pers.netname));
		return;
	}

	//@Barbatos: bugfix #83 - "Follow The Leader bug, a subbed player can be chosen as a leader"
	if (level.clients[client].pers.substitute)
	{
		return;
	}

	for (i = 0 ; i < level.maxclients ; i++)
	{
		if (level.clients[i].sess.sessionTeam != team)
		{
			continue;
		}

		if (level.clients[i].sess.teamLeader)
		{
			level.clients[i].sess.teamLeader = qfalse;
			ClientUserinfoChanged(i);
		}
	}
	level.clients[client].sess.teamLeader = qtrue;
	ClientUserinfoChanged( client );

//	PrintTeam(team, va("print \"%s is the new team leader\n\"", level.clients[client].pers.netname) );

	for (i = 0 ; i < level.maxclients ; i++)
	{
		if(level.clients[i].pers.connected != CON_CONNECTED)
		{
			continue;
		}

		if (level.clients[i].sess.sessionTeam != team)
		{
			continue;
		}

		if (i == client)
		{
			trap_SendServerCommand( i, va("cp \"You are the team leader.\"" ));
		}
		else
		{
			trap_SendServerCommand( i, va("cp \"%s%s%s is the team leader.\"", team == TEAM_RED ? S_COLOR_RED : S_COLOR_BLUE, level.clients[client].pers.netname, S_COLOR_WHITE ));
		}
	}
}

/*
==================
CheckTeamLeader
==================
*/
void CheckTeamLeader( team_t team )
{
	int  i, j;
	int  NumRed  = TeamCount( -1, TEAM_RED );
	int  NumBlue = TeamCount( -1, TEAM_BLUE );
	int  RandPickRed;
	int  RandPickBlue;

	if((NumRed <= 0) || (NumBlue <= 0))
	{
		return;
	}
	RandPickRed  = (rand() % NumRed);
	RandPickBlue = (rand() % NumBlue);

	for (i = 0 ; i < level.maxclients ; i++)
	{
		if (level.clients[i].sess.sessionTeam != team)
		{
			continue;
		}

		if (level.clients[i].sess.teamLeader)
		{
			break;
		}

		//@Barbatos
		if (level.clients[i].pers.substitute)
		{
			continue;
		}
	}

	if(team == TEAM_RED)
	{
		for(i = 0, j = 0; i < level.maxclients; i++)
		{
			if(level.clients[i].sess.sessionTeam != team)
			{
				continue;
			}

			//@Barbatos: bugfix #83 - "Follow The Leader bug, a subbed player can be chosen as a leader"
			if(level.clients[i].pers.substitute)
			{
				continue;
			}

			if(j == RandPickRed)
			{
				SetLeader( team, i );
			}
			j++;
		}
	}
	else if(team == TEAM_BLUE)
	{
		for(i = 0, j = 0; i < level.maxclients; i++)
		{
			if(level.clients[i].sess.sessionTeam != team)
			{
				continue;
			}

			//@Barbatos: bugfix #83 - "Follow The Leader bug, a subbed player can be chosen as a leader"
			if(level.clients[i].pers.substitute)
			{
				continue;
			}

			if(j == RandPickBlue)
			{
				SetLeader( team, i );
			}
			j++;
		}
	}
/*	if (i >= level.maxclients) {
		j = k = 0;
		for ( i = 0 ; i < level.maxclients ; i++ ) {
			if (level.clients[i].sess.sessionTeam != team)
				continue;
			if (!(g_entities[i].r.svFlags & SVF_BOT)) {
				level.clients[i].sess.teamLeader = qtrue;
				//SetLeader( team, i );
				break;
			}
		}
		j = k = 0;
		for( i = 0 ; i < level.maxclients ; i++ ) {
			if (level.clients[i].sess.sessionTeam != team)
				continue;
			level.clients[i].sess.teamLeader = qtrue;

				//SetLeader( team, i );
			break;
		}
	}  */
}

/*
==================
CheckTeamVote
==================
*/
void CheckTeamVote( team_t team )
{
	int  cs_offset;

	if (team == TEAM_RED)
	{
		cs_offset = 0;
	}
	else if (team == TEAM_BLUE)
	{
		cs_offset = 1;
	}
	else
	{
		return;
	}

	if (!level.teamVoteTime[cs_offset])
	{
		return;
	}

	if (level.time - level.teamVoteTime[cs_offset] >= VOTE_TIME)
	{
		trap_SendServerCommand( -1, "print \"Team vote failed.\n\"" );
	}
	else
	{
		if (level.teamVoteYes[cs_offset] > level.numteamVotingClients[cs_offset] / 2)
		{
			// execute the command, then remove the vote
			trap_SendServerCommand( -1, "print \"Team vote passed.\n\"" );

			//
			if (!Q_strncmp( "leader", level.teamVoteString[cs_offset], 6))
			{
				//set the team leader
				SetLeader(team, atoi(level.teamVoteString[cs_offset] + 7));
			}
			else
			{
				trap_SendConsoleCommand( EXEC_APPEND, va("%s\n", level.teamVoteString[cs_offset] ));
			}
		}
		else if (level.teamVoteNo[cs_offset] >= level.numteamVotingClients[cs_offset] / 2)
		{
			// same behavior as a timeout
			trap_SendServerCommand( -1, "print \"Team vote failed.\n\"" );
		}
		else
		{
			// still waiting for a majority
			return;
		}
	}
	level.teamVoteTime[cs_offset] = 0;
	trap_SetConfigstring( CS_TEAMVOTE_TIME + cs_offset, "" );
}

/*
==================
CheckCvars
==================
*/
void CheckCvars( void )
{
	static int	lastMod = -1;

	if (g_password.modificationCount != lastMod)
	{
		lastMod = g_password.modificationCount;

		if (*g_password.string && Q_stricmp( g_password.string, "none" ))
		{
			trap_Cvar_Set( "g_needpass", "1" );
		}
		else
		{
			trap_Cvar_Set( "g_needpass", "0" );
		}
	}
}

/*
=============
G_RunThink

Runs thinking code for this frame if necessary
=============
*/
void G_RunThink (gentity_t *ent)
{
	float  thinktime;

	thinktime = ent->nextthink;

	if (thinktime <= 0)
	{
		return;
	}

	if (thinktime > level.time)
	{
		return;
	}

	// TODO Check is in use or not
	if(!ent->inuse)
	{
		return;
	}
	ent->nextthink = 0;

	if(!ent->think)
	{
		return;
	}
	else
	{
		ent->think( ent );
	}
}

#if 1
/**
 * $(function)
 */
static void CheckPause(int deltaTime)
{
	gentity_t  *entity;
	int 	   timeoutLength;
	int 	   i;

	// If we're paused.
	if (level.pauseState & UT_PAUSE_ON)
	{
		// If warmup count down is in progress.
		if (level.warmupTime > level.time)
		{
			// Adjust warmup time for pause.
			level.warmupTime += deltaTime;
		}

		// Adjust timelimit for pause.
		level.survivorStartTime += deltaTime;
		level.roundEndTime	+= deltaTime;
		level.nextScoreTime += deltaTime;

		// Adjust round time for pause.
		// @Fenix moved out here since it's needed for all the gametypes
		level.startTime += deltaTime;

		//adjust hotpotato for pause
		level.hotpotatotime += deltaTime;

		level.pausedTime	+= deltaTime;

		// If this is a survivor game type.
		if (g_survivor.integer)
		{
			// If round restart is pending.
			if (level.survivorRoundRestartTime > level.time)
			{
				// Adjust round restart time for pause.
				level.survivorRoundRestartTime += deltaTime;
			}
			else
			{
				// For Bomb Mode.
				if (g_gametype.integer == GT_UT_BOMB)
				{
					// If Bomb is planted.
					if (level.BombPlanted == qtrue)
					{
						entity = G_FindClassHash(NULL, HSH_ut_planted_bomb);

						if(!entity)
						{
							G_Error("Bomb planted but not found!");
						}
						else
						{
							entity->timestamp += deltaTime;
						}
					}
				}
			}
		}

		// Loop through entities.
		for (i = 0, entity = g_entities; i < level.num_entities; i++, entity++)
		{
			// If this is a client.
			if (entity->client)
			{
				// If not a spectator.
				if (entity->client->sess.sessionTeam != TEAM_SPECTATOR)
				{
					// If dead.
					if (entity->client->ps.stats[STAT_HEALTH] <= 0)
					{
						// If player is supposed to respawn.
						if (entity->client->respawnTime > level.time)
						{
							// Adjust respawn time for pause.
							entity->client->respawnTime += deltaTime;
						}
					}
					else
					{
						// Adjust protection time.
						entity->client->protectionTime += deltaTime;
					}
				}
				// Make sure client is given inactivity grace
				entity->client->sess.inactivityTime += deltaTime;
			}
			else if (entity->inuse)
			{
				// If this entity is a missile or an item.
				if ((entity->s.eType == ET_MISSILE) || (entity->s.eType == ET_ITEM))
				{
					if ((entity->nextthink > level.time) && (entity->nextthink < (INT_MAX - deltaTime)))
					{
						entity->nextthink += deltaTime;
					}

					entity->s.pos.trTime  += deltaTime;
					entity->s.apos.trTime += deltaTime;
				}

				// If this entity is a mover.
				if ((entity->s.eType == ET_MOVER) && (
//					!Q_stricmp(entity->classname, "func_door") || !Q_stricmp(entity->classname, "func_train") ||
//					!Q_stricmp(entity->classname, "func_rotating_door") || !Q_stricmp(entity->classname, "func_ut_train")
				 entity->classhash==HSH_func_door || entity->classhash==HSH_func_train ||
				 entity->classhash==HSH_func_ut_train || entity->classhash==HSH_func_rotating_door
				))
				{
					if ((entity->nextthink > level.time) && (entity->nextthink < (INT_MAX - deltaTime)))
					{
						entity->nextthink += deltaTime;
					}

					// Restore state.
					SetMoverState(entity, entity->mover->moverState, level.time + entity->timestamp);
				}
			}
		}

		//@Fenix - resend time config strings - fix matchmode timer display
		//Apparently when a new client join a paused match his timer is fucked.
		//We need to send updates for cgs.levelStartTime and cgs.roundEndTime every
		//frame in order to display a correct timer string.
		trap_SetConfigstring( CS_LEVEL_START_TIME, va("%i", level.startTime ));
		trap_SetConfigstring( CS_ROUND_END_TIME, va("%i", level.roundEndTime ));
		trap_SetConfigstring(CS_PAUSE_STATE, va("%d %i %i", level.pauseState, level.pauseTime, level.pausedTime));

		// Get timeout length.
		timeoutLength = g_timeoutLength.integer;

		// Clamp.
		if (timeoutLength < 3)
		{
			timeoutLength = 3;
		}
		else if (timeoutLength > 300)
		{
			timeoutLength = 300;
		}

		// If pause was called by a team and it's time to resume.
		if (!(level.pauseState & UT_PAUSE_TR) && ((level.pauseState & UT_PAUSE_RC) || (level.pauseState & UT_PAUSE_BC)) && (level.time >= level.pauseTime + (timeoutLength * 1000)))
		{
			// Set flags.
			level.pauseState |= UT_PAUSE_TR;

			// Set time to resume.
			level.pauseTime = level.time + 10000;

			// Set config string.
			trap_SetConfigstring(CS_PAUSE_STATE, va("%d %i %i", level.pauseState, level.pauseTime, level.pausedTime));
		}

		if (g_pauseLength.integer > 0)
		{
			// If pause was called by the admin and it's time to resume.
			if (!(level.pauseState & UT_PAUSE_TR) && (level.pauseState & UT_PAUSE_AC) && (g_pauseLength.integer > 0) && (level.time >= level.pauseTime + (g_pauseLength.integer * 1000)))
			{
				// Set flags.
				level.pauseState |= UT_PAUSE_TR;

				// Set time to resume.
				level.pauseTime = level.time + 10000;

				// Set config string.
				trap_SetConfigstring(CS_PAUSE_STATE, va("%d %i %i", level.pauseState, level.pauseTime, level.pausedTime));
			}
		}

		// If we need to resume.
		if ((level.pauseState & UT_PAUSE_TR) && (level.time >= level.pauseTime))
		{
			// Clear flags.
			level.pauseState = 0;

			// Clear time.
			level.pauseTime = 0;

			// Set config string.
			trap_SetConfigstring(CS_PAUSE_STATE, va("%d %i %i", level.pauseState, level.pauseTime, level.pausedTime));

			//@Barbatos: resend to the client the hotpotatotime or it'll be fucked up after a timeout (FIXME)
			trap_SetConfigstring (CS_HOTPOTATOTIME, va ("%i", level.hotpotatotime));

		}
	}
	else
	{
		// If we need to pause.
		if ((level.pauseState & UT_PAUSE_TR) && ((g_survivor.integer && (level.survivorRoundRestartTime >= level.time)) || (!g_survivor.integer && (level.time >= level.pauseTime))))
		{
			// Loop through entities.
			for (i = 0, entity = g_entities; i < level.num_entities; i++, entity++)
			{
				// If entity is not in use.
				if (!entity->inuse)
				{
					continue;
				}

				// If this entity is a mover.
				if ((entity->s.eType == ET_MOVER) && (
//				   !Q_stricmp(entity->classname, "func_door") || !Q_stricmp(entity->classname, "func_train") ||
//				   !Q_stricmp(entity->classname, "func_rotating_door") || !Q_stricmp(entity->classname, "func_ut_train")
				 entity->classhash==HSH_func_door || entity->classhash==HSH_func_train ||
				 entity->classhash==HSH_func_ut_train || entity->classhash==HSH_func_rotating_door
				))
				{
					// Backup time value.
					entity->timestamp = entity->s.pos.trTime - level.time;	// pos.trTrime and apos.trTime should be equal.
				}
			}

			// Set/Clear flags.
			level.pauseState |= UT_PAUSE_ON;
			level.pauseState &= ~UT_PAUSE_TR;

			// Set time of pause.
			level.pauseTime = level.time;

			// Set config string.
			trap_SetConfigstring(CS_PAUSE_STATE, va("%d %i %i", level.pauseState, level.pauseTime, level.pausedTime));
		}
	}
}
#else
/**
 * $(function)
 */
static void CheckPause(int deltaTime)
{
	gentity_t  *ent;
	int 	   i;

	// If we're paused.
	if (level.pauseState & UT_PAUSE_ON)
	{
		// If warmup count down is in progress.
		if (level.warmupTime > level.time)
		{
			// Adjust warmup time for pause.
			level.warmupTime += deltaTime;
		}
		else
		{
			// Adjust timelimit for pause.
			level.survivorStartTime += deltaTime;

			// If this is a survivor game type.
			if (g_survivor.integer)
			{
				// If round testart is pending.
				if (level.survivorRoundRestartTime > level.time)
				{
					// Adjust round restart time for pause.
					level.survivorRoundRestartTime += deltaTime;
				}
				else
				{
					// Adjust round time for pause.
					level.startTime += deltaTime;

					// For Bomb Mode
					if(g_gametype.integer == GT_UT_BOMB)
					{
						// If Bomb is planted
						if(level.BombPlanted == qtrue)
						{
							ent = G_FindClassHash( NULL, HSH_ut_planted_bomb);

							if(!ent)
							{
								G_Error( "Bomb planted but not found!" );
							}
							else
							{
								ent->timestamp += deltaTime;
							}

							if(level.StartDefuseTime > 0)	// Will be zero if not set!
							{
								level.StartDefuseTime += deltaTime;
							}
						}
					}
				}
			}
		}

		// Loop through entities.
		for (i = 0, ent = g_entities; i < level.num_entities; i++, ent++)
		{
			// If entity is not in use.
			if (!ent->inuse)
			{
				continue;
			}

			// If this entity is a player.
			if (ent->s.eType == ET_PLAYER)
			{
				// If player is supposed to respawn.
				if ((ent->client->ps.stats[STAT_HEALTH] <= 0) && (ent->client->respawnTime > level.time))
				{
					// Adjust respawn time for pause.
					ent->client->respawnTime += deltaTime;
				}
				else if (ent->client->ps.stats[STAT_HEALTH] > 0)	   // if alive, you defuse and dead then can't do
				{	// Adjust Bomb defuse time
//					if( g_gametype.integer == GT_UT_BOMB && ent->client->IsDefusing == qtrue ) {
//						ent->client->StartDefuseTime += deltaTime;
				}
			}
			entity->client->sess.inactivityTime += deltaTime;
		}

		// If this entity is a missile or an item.
		if ((ent->s.eType == ET_MISSILE) || (ent->s.eType == ET_ITEM))
		{
			if ((ent->nextthink > level.time) && (ent->nextthink < (INT_MAX - deltaTime)))
			{
				ent->nextthink += deltaTime;
			}

			ent->s.pos.trTime  += deltaTime;
			ent->s.apos.trTime += deltaTime;
		}

		// If this entity is a mover.
		if (ent->s.eType == ET_MOVER)
		{
			if ((ent->nextthink > level.time) && (ent->nextthink < (INT_MAX - deltaTime)))
			{
				ent->nextthink += deltaTime;
			}

			// Restore state.
			SetMoverState(ent, ent->mover->moverState, level.time + ent->timestamp);
		}
	}

	// If we need to resume.
	if (level.pauseState & UT_PAUSE_TR && (level.time >= level.pauseTime))
	{
		// Clear flags.
		level.pauseState = 0;

		// Clear time.
		level.pauseTime = 0;

		// Set config string.
		trap_SetConfigstring(CS_PAUSE_STATE, va("%d %i %i", level.pauseState, level.pausetime, level.pausedTime));
	}
}
else
{
	// If we need to pause.
	if (level.pauseState & UT_PAUSE_TR && (level.time >= level.pauseTime))
	{
		// Loop through entities.
		for (i = 0, ent = g_entities; i < level.num_entities; i++, ent++)
		{
			// If entity is not in use.
			if (!ent->inuse)
			{
				continue;
			}

			// If this entity is a mover.
			if (ent->s.eType == ET_MOVER)
			{
				// Backup time value.
				ent->timestamp = ent->s.pos.trTime - level.time;		// pos.trTrime and apos.trTime should be equal.
			}
		}

		// Set/Clear flags.
		level.pauseState |= UT_PAUSE_ON;
		level.pauseState &= ~UT_PAUSE_TR;

		// Clear time.
		level.pauseTime = 0;

		// Set config string.
		trap_SetConfigstring(CS_PAUSE_STATE, va("%d %i %i", level.pauseState, level.pausetime, level.pausedTime));
	}
}
}
#endif

/**
 * $(function)
 */
static void CheckMatch(void) {

    int numRedPlayers, numBluePlayers;

    // If we're not in match mode
    if (!(level.matchState & UT_MATCH_ON)) {
        return;
    }

    // If neither team is ready
    if (!(level.matchState & UT_MATCH_RR) && !(level.matchState & UT_MATCH_BR)) {
        return;
    }

    // Get team counts
    numRedPlayers  = TeamCount(-1, TEAM_RED);
    numBluePlayers = TeamCount(-1, TEAM_BLUE);

    // If both teams have enough players.
    if ((numRedPlayers > 0) && (numBluePlayers > 0)) {
        return;
    }

    // If both teams are empty
    if ((numRedPlayers == 0) && (numBluePlayers == 0)) {
        // Clear team names
        trap_Cvar_Set("g_nameRed", "");
        trap_Cvar_Set("g_nameBlue", "");
    }

    // If red team is ready but not enough players
    if ((level.matchState & UT_MATCH_RR) && (numRedPlayers < 1)) {
        level.matchState &= ~UT_MATCH_RR;
        level.pauseState = 0;
        level.pauseTime = 0;
    }

    // If blue team is ready but not enough players
    if ((level.matchState & UT_MATCH_BR) && (numBluePlayers < 1)) {
        level.matchState &= ~UT_MATCH_BR;
        level.pauseState = 0;
        level.pauseTime = 0;
    }

    // Set config string
    trap_SetConfigstring(CS_MATCH_STATE, va("%d", level.matchState));
    trap_SetConfigstring(CS_PAUSE_STATE, va("%d %i %i", level.pauseState, level.pauseTime, level.pausedTime));

}

//////////////////////////////////////////////////////////////////////////////////////////
// Name       : G_StaminaRegeneration
// Author     : Barbatos, Fenix
// Description: Refill player stamina if he has the toggle enabled and he's standing still
//////////////////////////////////////////////////////////////////////////////////////////
void G_StaminaRegeneration(void) {

    int i;

    // Invalid gametype
    if (g_gametype.integer != GT_JUMP) {
        return;
    }

    // Default behavior or infinite stamina set on the server
    // In both cases we don't have to refill the player stamina
    if (g_stamina.integer != 1) {
        return;
    }

    for (i = 0 ; i < level.maxclients ; i++) {

        if (level.clients[i].pers.connected != CON_CONNECTED) {
            continue;
        }

        if (level.clients[i].sess.sessionTeam == TEAM_SPECTATOR) {
            continue;
        }

        if (!level.clients[i].sess.regainstamina) {
            continue;
        }

        if (level.clients[i].sess.jumpRun > 0) {
            continue;
        }

        if ((level.clients[i].ps.velocity[0] == 0) && (level.clients[i].ps.velocity[1] == 0) && (level.clients[i].ps.velocity[2] == 0)) {
            level.clients[i].ps.stats[STAT_STAMINA] = level.clients[i].ps.stats[STAT_HEALTH] * UT_STAM_MUL;
        }
    }
}
/////////////////////////////////////////////////////////

static void CheckBombExplosion(void)
{
	gentity_t *entity;
	vec3_t	  blank = { 0, 0, 0};
	float	  radius, damage;
	int 	  i;

	// If bomb didn't go off.
	if (!level.BombGoneOff)
	{
		return;
	}

	// If it's too soon for another check.
	//if ((level.time - level.BombExplosionCheckTime) < 250)
	//	return;

	// Check for clients who are too close to ground zero.
	if ((level.time - level.BombExplosionTime) < 8000)
	{
		for (i = 0; i < level.maxclients; i++)
		{
			// Get entity pointer.
			entity = &g_entities[i];

			// If not a client.
			if (!entity->client)
			{
				continue;
			}

			// If not connected.
			if (entity->client->pers.connected != CON_CONNECTED)
			{
				continue;
			}

			// If dead.
			if (entity->client->ghost || (entity->client->ps.stats[STAT_HEALTH] <= 0))
			{
				continue;
			}

			// If not on any team.
			if ((entity->client->sess.sessionTeam != TEAM_RED) && (entity->client->sess.sessionTeam != TEAM_BLUE))
			{
				continue;
			}

			// If we're too close.
			if (Distance(entity->client->ps.origin, level.BombExplosionOrigin) < 500.0f)
			{
				// Damage Player.
				G_Damage(entity, entity, entity, blank, entity->client->ps.origin, 5, DAMAGE_RADIUS, UT_MOD_BOMBED, HL_UNKNOWN);
			}
		}
	}

	// Update time.
	//level.BombExplosionCheckTime = level.time;

	// Calculate explosion radius.
	radius = (level.time - level.BombExplosionTime) * 1.0f;

	// If radius is maxed out.
	if (radius >= 5000.0f)
	{
		return;
	}

	// Calculate 100HP damage at 500 units.
	damage = 100.0f / (float)(1.0f + sin(M_PI + ((M_PI / 2) * (500.0f / 5000.0f))));

	// Damage entities in radius.
	G_BombDamage(level.BombExplosionOrigin, damage, radius, 5000.0f, UT_MOD_BOMBED);
}

/////////////////////////////////////////////////////////

/*
================
G_RunFrame

Advances the non-player objects in the world
================
*/
void G_RunFrame( int levelTime )
{
	int 		i;
	gentity_t	*ent;
	int 		msec;
	int 		start, end;
	gclient_t	*cl; // @Kalish

/*	for (i = 0; i < MAX_GENTITIES; i++)
	{
		if (g_entities[i].inuse)
		{
			if (g_entities[i].s.pos.trType==TR_INTERPOLATE)
			{
				g_entities[i].s.pos.trBase[0]/=1000;
				g_entities[i].s.pos.trBase[1]/=1000;
				g_entities[i].s.pos.trBase[2]/=1000;
			}
		}
	} */
	//NT - store the time the frame started (for antilag)
	level.frameStartTime = trap_Milliseconds();

	// if we are waiting for the level to restart, do nothing
	if (level.restarted)
	{
		return;
	}

	//////////////////
//	 levelTime-=level.timechop;

	/////////////////////////

	level.framenum++;
	level.previousTime = level.time;
	level.time	   = levelTime;

	msec		   = level.time - level.previousTime;

	// get any cvar changes
	G_UpdateCvars();

	// Pause.
	CheckPause(msec);

	// Bomb explosion.
	CheckBombExplosion();

	if ((level.time > level.domoveoutcalltime) && (level.domoveoutcalltime != 0))
	{
		G_DoMoveOutCall();
		level.domoveoutcalltime = 0;
	}

	if ((g_gametype.integer == GT_CTF) && (level.hotpotato == 1))
	{
		if (level.time > level.hotpotatotime)
		{
			//Reset the flags any players might have on them
			gentity_t *ent;

			for (i = 0 ; i < g_maxclients.integer ; i++)
			{
				ent = g_entities + i;

				if (ent->client->pers.connected != CON_CONNECTED)
				{
					continue;
				}

				if (utPSGetItemByID( &ent->client->ps, UT_ITEM_REDFLAG ) != -1)
				{
					Drop_Item(ent, &bg_itemlist[UT_ITEM_REDFLAG], 0, qtrue);
				}

				if (utPSGetItemByID( &ent->client->ps, UT_ITEM_BLUEFLAG ) != -1)
				{
					Drop_Item(ent, &bg_itemlist[UT_ITEM_BLUEFLAG], 0, qtrue);
				}
			}
			//returns any world flags to the pedestals
			Team_ResetFlag(TEAM_RED, 1);
			Team_ResetFlag(TEAM_BLUE, 1);
			trap_SendServerCommand( -1, va("cp \"Hot Potato\""));

			//@Barbatos
			G_LogPrintf("Hotpotato:\n");
			level.hotpotato = 0;

			Team_ReturnFlagSound(NULL, 0);
		}
	}

	//
	// go through all allocated objects
	//
	start = trap_Milliseconds();
	ent   = &g_entities[0];

	for (i = 0 ; i < level.num_entities ; i++, ent++)
	{
		if (!ent->inuse)
		{
			continue;
		}

		// clear events that are too old
		if (level.time - ent->eventTime > EVENT_VALID_MSEC)
		{
			if (ent->s.event)
			{
				ent->s.event = CSE_ENC(0);	// &= EV_EVENT_BITS;

				if (ent->client)
				{
					ent->client->ps.externalEvent = CSE_ENC(0);
					// predicted events should never be set to zero
					//ent->client->ps.events[0] = 0;
					//ent->client->ps.events[1] = 0;
				}
			}

			if (ent->freeAfterEvent)
			{
				// tempEntities or dropped items completely go away after their event
				G_FreeEntity( ent );
				continue;
			}
			else if (ent->unlinkAfterEvent)
			{
				// items that will respawn will hide themselves after their pickup event
				ent->unlinkAfterEvent = qfalse;
				trap_UnlinkEntity( ent );
			}
		}

		// temporary entities don't think
		if (ent->freeAfterEvent)
		{
			continue;
		}

		if (!ent->r.linked && ent->neverFree)
		{
			continue;
		}

		if (ent->s.eType == ET_MISSILE)
		{
			G_RunMissile( ent );
			continue;
		}

		if (ent->s.eType == ET_CORPSE)
		{
			G_RunCorpse( ent );
//			continue;
		}

		if ((ent->s.eType == ET_ITEM) || ent->physicsObject)
		{
			G_RunItem( ent );
			continue;
		}

		if (ent->s.eType == ET_MOVER)
		{
			G_RunMover( ent );
			continue;
		}

		if (ent->s.eType == ET_MR_SENTRY)
		{
			G_MrSentryThink( ent );
			continue;
		}

		if (i < MAX_CLIENTS)
		{
			G_RunClient( ent );
			continue;
		}
		G_RunThink( ent );
	}
	end   = trap_Milliseconds();

	start = trap_Milliseconds();
	// perform final fixups on the players
	ent   = &g_entities[0];

	for (i = 0 ; i < level.maxclients ; i++, ent++)
	{
		if (ent->inuse)
		{
			ClientEndFrame( ent );
		}
	}
	end = trap_Milliseconds();

	CheckExitRules();

	if (level.warmupIntro)
	{
		if (level.time > level.warmupTime)
		{
			// COMMENTED OUT BY HIGHSEA; WE DONT USE MAP_RESTART!
			// BTW, I DONT THINK THIS CODE EVER GETS EXECUTED
			/*
			trap_Cvar_Set( "g_restarted", "1" );
			trap_SendConsoleCommand( EXEC_APPEND, "map_restart 0\n" );
			level.restarted = qtrue;
			level.survivorRestart = qtrue;
			level.survivorRoundRestartTime = 0;
			level.startTime = level.time;

			trap_SetConfigstring( CS_LEVEL_START_TIME, va("%i", level.startTime ) );
			*/
			level.warmupIntro = qfalse;
			level.warmupTime  = 0;
			G_InitWorldSession();
		}
	}
	else
	{
		// Match.
		CheckMatch();

		if (g_survivor.integer)
		{
			CheckSurvivor ( );
		}
		else
		{
			CheckTournament ( );
		}
	}

	if (g_gametype.integer == GT_UT_CAH)
	{
		CheckCaptureAndHold ( );
	}

	// DENSITY
//	CheckFlags();
	// see if it is time to end the level

	// cancel vote if timed out
	CheckVote(qfalse);

	// check team votes
	CheckTeamVote( TEAM_RED );
	CheckTeamVote( TEAM_BLUE );

	// for tracking changes
	CheckCvars();

	if (g_listEntity.integer)
	{
		for (i = 0; i < MAX_GENTITIES; i++)
		{
			G_Printf("%4i: %s\n", i, g_entities[i].classname);
		}
		trap_Cvar_Set("g_listEntity", "0");
	}

	/*
	for (i = 0; i < MAX_GENTITIES; i++)
	{
		if (g_entities[i].inuse)
		{
			if (g_entities[i].s.pos.trType==TR_INTERPOLATE)
			{
				g_entities[i].s.pos.trBase[0]*=1000;
				g_entities[i].s.pos.trBase[1]*=1000;
				g_entities[i].s.pos.trBase[2]*=1000;
			}
		}
	}
	*/

#ifdef USE_AUTH
	// @Kalish
	for (i = 0; i < level.maxclients; i++)
	{
		cl = &level.clients[i];

		if( !cl )
		{
			continue;
		}

		if( cl->pers.connected != CON_CONNECTED )
		{
			continue;
		}

		if( cl->sess.droptime < 1 )
		{
			continue;
		}

		if( level.time - cl->sess.droptime > 100 )
		{
			#ifdef DEBUG
			Com_Printf( AUTH_PREFIX" drop executed after %i ms - slot: %i\n", level.time - cl->sess.droptime, i );
			#endif
			SV_Auth_DropClient( i, cl->sess.dropreason, cl->sess.dropmessage );
		}
	}
#endif

	//@Barbatos - jump mode - check if we want to regenerate the stamina
	if(g_gametype.integer == GT_JUMP)
		G_StaminaRegeneration();

	//@Fenix - send a notice when muted players get automatically unmuted.
	for (i=0; i < level.maxclients; i++) {
		cl = &level.clients[i];
		if (!cl) continue;
		if (cl->pers.connected != CON_CONNECTED) continue;
		if ((cl->sess.muteTimer > 0) && (cl->sess.muteTimer < level.time)) {
			// This guy was muted and the mute expire time passed.
			cl->sess.muteTimer = 0;
			trap_SendServerCommand(cl->ps.clientNum, "print \"You have been unmuted\"\n");
			trap_SendServerCommand(-1, va("print \"%s has been ^2unmuted^7\n\"", cl->pers.netname));
		}
	}
}


#ifdef USE_AUTH

#define MAX_HAX_REPORT_PACKET (512-8)
char hax_udp_target[32] = {0};

void SendHaxReportPacket(unsigned char *data, int len, int frag)
// add header, encrypt, send. frag = fragment nr.
{
	unsigned char report[MAX_HAX_REPORT_PACKET+8];
	unsigned iv;
	static int report_seq = 0;
	int sum = 0;
	int i;

	if (!frag) report_seq++;

	report[4]=1;            // version
	report[5]=report_seq;   // report nr. (1..n)
	report[6]=frag;         // report fragment nr. (0..n)

// Create some decent key
	iv = level.time ^ 0xFE8811E6;
	for(i=0;i<(rand()&15)+7;i++) {
		iv+=rand()<<9;
		iv^=iv>>2;
		iv^=iv>>9;
		iv^=iv<<21;
	}

	*(unsigned int*)report = iv; // crypto key

	for(i=4;i<7;i++) { // encrypt header
		iv^=iv>>2;
		iv^=iv>>9;
		iv^=iv<<21;
		sum+=report[i];
		report[i]^=iv&0xFF;
	}

	for(i=0;i<len;i++) { // encrypt data
		iv^=iv>>2;
		iv^=iv>>9;
		iv^=iv<<21;
		sum+=data[0];
		report[7+i]=(iv&0xFF)^data[0];
		data++;
	}
	report[len+7]=sum; // last byte is checksum of plaintext

	trap_NET_SendPacket( NS_CLIENT, 8+len, report, hax_udp_target );
}

void SendHaxReport() //@r00t: Send hax report to master server
{
	gclient_t  *cl;
	int idnum, i;
	unsigned char report[MAX_HAX_REPORT_PACKET+128];
// max size: (33(name)+33(auth)+16(IP)+16(guid)+9(report))=107*32(max players)=3424 = max 7 packets
	int rp = 0;
	int frag = 0;
	if (!g_dedicated.integer || !hax_udp_target[0]) return; // need dedicated server or have no destination address

	for (cl = level.clients,idnum=0; idnum < level.maxclients;cl++,idnum++) {
		int before = rp;
		if (cl->pers.connected != CON_CONNECTED || !cl->sess.hax_report_time) {
			continue;
		}
		
		i = strlen(cl->pers.netname);
		report[rp++] = i; memcpy(report+rp,cl->pers.netname,i); rp+=i;
		i = strlen(cl->sess.auth_login);
		report[rp++] = i; memcpy(report+rp,cl->sess.auth_login,i); rp+=i;
		i = strlen(cl->sess.ip);
		report[rp++] = i; memcpy(report+rp,cl->sess.ip,i); rp+=i;
		memcpy(report+rp,cl->sess.guid_bin,16); rp+=16;

//		trap_SendServerCommand(-1, va("print \"HAX: %s [%d %d %d %d %d] %d\n\"",
//   cl->pers.netname, cl->sess.hax_report[0],cl->sess.hax_report[1],cl->sess.hax_report[2],cl->sess.hax_report[3],cl->sess.hax_report[4], level.time - cl->sess.hax_report_time));

		if (cl->sess.hax_report[5]==1 || (cl->sess.hax_report[5]==2 && !cl->sess.hax_report[0] && !cl->sess.hax_report[1])) {
// always send last game info at least once or when no current info is available
			memcpy(report+rp,&cl->sess.hax_report[3],8); rp+=8;
			report[rp++] = 255;
			cl->sess.hax_report[5]=2; // flag as sent
		} else {
			memcpy(report+rp,cl->sess.hax_report,8); rp+=8;
			report[rp++] = cl->sess.hax_report[2];
			if (cl->sess.hax_report[2]<254) cl->sess.hax_report[2]++; // increase data age counter
		}

		if (rp>MAX_HAX_REPORT_PACKET) { // fragment overflow
			SendHaxReportPacket(report,before,frag); frag++;
			memmove(report,report+before,rp-before);
			rp-=before;
		}
	}

	if (rp) SendHaxReportPacket(report,rp,frag);
}

void InitHaxNetwork()
{
	char tmp[sizeof(netadr_t)]; // it's ugly because of LCC bug workaround
	netadr_t *hax_server_address = (netadr_t *)&tmp;

	memset(hax_server_address,0,sizeof(netadr_t));
	hax_udp_target[0]=0;

	if (trap_Cvar_VariableIntegerValue("sv_auth_engine") != 1) {
		return;
	}

	if (!trap_NET_StringToAdr(HAX_SERVER_NAME,hax_server_address) || (hax_server_address->type == NA_BAD)) {
//		Com_Printf( "Failed to resolve hax server: %s\n",HAX_SERVER_NAME);
		return;
	}

	Com_sprintf( hax_udp_target, sizeof(hax_udp_target), "%i.%i.%i.%i:%i",
		hax_server_address->ip[0], hax_server_address->ip[1], hax_server_address->ip[2], hax_server_address->ip[3], HAX_SERVER_PORT);
//	Com_Printf( "Hax server: %s\n",hax_udp_target);
}

#endif

