// Copyright (C) 1999-2000 Id Software, Inc.
//
// cg_main.c -- initialization and primary entry point for cgame
#include "../common/c_cvars.h"
#include "../common/c_bmalloc.h"
#include "cg_drawaries.h"
#include "cg_local.h"

#ifdef MISSIONPACK
 #include "../ui/ui_shared.h"
// display context for new ui stuff
displayContextDef_t  cgDC;
#endif

extern vec3_t  bg_colors[20];

// reserve 32MB memory for bmalloc
#define MALLOC_POOL_SIZE (1024*1024*32)
char cg_malloc_pool[MALLOC_POOL_SIZE];

int  forceModelModificationCount            = -1;

void CG_Init( int serverMessageNum, int serverCommandSequence, int clientNum );
void CG_Shutdown( void );

int  crosshairRGBModificationCount          = -1;
int  crosshairFriendRGBModificationCount    = -1;
int  scopeRGBModificationCount              = -1;
int  scopeFriendRGBModificationCount        = -1;
//int   armbandModCount                     = -1;
int  skinAllyModificationCount              = -1;
int  skinEnemyModificationCount             = -1;

// For cg_predict
unsigned char predict_xor1;
unsigned char predict_xor2;


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
        case CG_INIT:
            CG_Init( arg0, arg1, arg2 );

            // Apoxol: Ok, huge hack here..  This will prevent any console messages from
            // being displayed on the screen.  This is so we can move the chat window
            if (!cg_standardChat.integer)
            {
                trap_Cvar_Set ( "con_notifytime", "-2" );
            }

            return 0;

        case CG_SHUTDOWN:
            CG_Shutdown();
            return 0;

        case CG_CONSOLE_COMMAND:
            return CG_ConsoleCommand();

        case CG_DRAW_ACTIVE_FRAME:
            CG_DrawActiveFrame( arg0, arg1, arg2 );
            return 0;

        case CG_CROSSHAIR_PLAYER:
            return CG_CrosshairPlayer();

        case CG_LAST_ATTACKER:
            return CG_LastAttacker();

        case CG_KEY_EVENT:
            CG_KeyEvent(arg0, arg1);
            return 0;

        case CG_MOUSE_EVENT:
#ifdef MISSIONPACK
            cgDC.cursorx = cgs.cursorX;
            cgDC.cursory = cgs.cursorY;
#endif
            CG_MouseEvent(arg0, arg1);
            return 0;

        case CG_EVENT_HANDLING:
            CG_EventHandling(arg0);
            return 0;

        default:
            CG_Error( "vmMain: unknown command %i", command );
            break;
    }
    return -1;
}

cg_t          cg;
cgs_t         cgs;
centity_t     cg_entities_new[MAX_GENTITIES];
weaponInfo_t  cg_weapons[MAX_WEAPONS];
itemInfo_t    cg_items[MAX_ITEMS];

vmCvar_t      cg_centertime;
vmCvar_t      cg_runpitch;
vmCvar_t      cg_runroll;
vmCvar_t      cg_bobup;
vmCvar_t      cg_bobpitch;
vmCvar_t      cg_bobroll;
vmCvar_t      cg_swingSpeed;
vmCvar_t      cg_shadows;
vmCvar_t      cg_drawTimer;
vmCvar_t      cg_drawFPS;
vmCvar_t      cg_drawSnapshot;
vmCvar_t      cg_drawCrosshair;
vmCvar_t      cg_drawCrosshairNames;
vmCvar_t      cg_crosshairNamesType;
vmCvar_t      cg_crosshairNamesSize;
vmCvar_t      cg_drawRewards;
vmCvar_t      cg_crosshairSize;
vmCvar_t      cg_crosshairX;
vmCvar_t      cg_crosshairY;
vmCvar_t      cg_crosshairRGB;
vmCvar_t      cg_crosshairFriendRGB;
vmCvar_t      cg_scopeRGB;
vmCvar_t      cg_scopeFriendRGB;
//vmCvar_t  cg_scopeType;
vmCvar_t      cg_scopeG36;
vmCvar_t      cg_scopePSG;
vmCvar_t      cg_scopeSR8;
vmCvar_t      cg_scopering;
vmCvar_t      cg_nvg;
vmCvar_t      cg_draw2D;
vmCvar_t      cg_drawStatus;
vmCvar_t      cg_animSpeed;
vmCvar_t      cg_debugAnim;
vmCvar_t      cg_debugPosition;
vmCvar_t      cg_debugEvents;
vmCvar_t      cg_errorDecay;
vmCvar_t      cg_nopredict;
vmCvar_t      cg_noweaponpredict;
//vmCvar_t  cg_antilag;
vmCvar_t      cg_noPlayerAnims;
vmCvar_t      cg_showmiss;
vmCvar_t      cg_servertime;

vmCvar_t      cg_footsteps;
//vmCvar_t  cg_addMarks;
vmCvar_t      cg_viewsize;
vmCvar_t      cg_gun_frame;
vmCvar_t      cg_gun_x;
vmCvar_t      cg_gun_y;
vmCvar_t      cg_gun_z;
vmCvar_t      cg_tracerChance;
vmCvar_t      cg_tracerWidth;
vmCvar_t      cg_tracerLength;
vmCvar_t      cg_autoswitch;

/*
vmCvar_t    cg_simpleItems;
*/

vmCvar_t  cg_fov;
vmCvar_t  cg_demoFov; //@Barbatos: special fov for the demos
vmCvar_t  cg_zoomFov;
vmCvar_t  cg_thirdPerson;
vmCvar_t  cg_thirdPersonRange;
vmCvar_t  cg_thirdPersonAngle;
vmCvar_t  cg_stereoSeparation;
vmCvar_t  cg_lagometer;
vmCvar_t  cg_synchronousClients;
vmCvar_t  cg_teamChatTime;
vmCvar_t  cg_teamChatHeight;
vmCvar_t  cg_buildScript;
vmCvar_t  cg_paused;
vmCvar_t  cg_blood;
vmCvar_t  cg_drawTeamOverlay;
vmCvar_t  cg_drawTeamOverlayScores;

vmCvar_t  cg_drawFriend;
vmCvar_t  cg_teamChatsOnly;
vmCvar_t  cg_chatSound;
vmCvar_t  cg_cleanFollow;
vmCvar_t  cg_noci;
vmCvar_t  cg_noVoiceChats;
vmCvar_t  cg_noVoiceText;
vmCvar_t  cg_hudFiles;
vmCvar_t  cg_smoothClients;
//vmCvar_t  pmove_fixed;
//vmCvar_t  cg_pmove_fixed;
//vmCvar_t  pmove_msec;
//vmCvar_t  cg_pmove_msec;
vmCvar_t  cg_cameraMode;
vmCvar_t  cg_cameraOrbit;
vmCvar_t  cg_cameraOrbitDelay;
vmCvar_t  cg_timescaleFadeEnd;
vmCvar_t  cg_timescaleFadeSpeed;
vmCvar_t  cg_timescale;
vmCvar_t  cg_smallFont;
vmCvar_t  cg_bigFont;
vmCvar_t  cg_noTaunt;
vmCvar_t  cg_singlePlayer;
vmCvar_t  cg_enableDust;
vmCvar_t  cg_enableBreath;
vmCvar_t  cg_singlePlayerActive;
vmCvar_t  cg_recordSPDemo;
vmCvar_t  cg_recordSPDemoName;

// Urban Terror cvars
vmCvar_t  cg_maxFragments;
vmCvar_t  cg_spaceInvaders;
vmCvar_t  cg_markTotaltime;
vmCvar_t  cg_doWarmup;
vmCvar_t  cg_visibleBleeding;
vmCvar_t  cg_hitsound;

vmCvar_t  cg_skinAlly;
vmCvar_t  cg_skinEnemy;
vmCvar_t  cg_funstuff;

vmCvar_t  cg_drawHands;
//vmCvar_t  cg_debugAW;
vmCvar_t  cg_standardChat;
vmCvar_t  cg_chatHeight;
vmCvar_t  cg_chatTime;
vmCvar_t  cg_msgHeight;
vmCvar_t  cg_msgTime;
//vmCvar_t  cg_maxPrecip;
//vmCvar_t  cg_boundsoptimize;

vmCvar_t  cg_gunSize;
//vmCvar_t  cg_gunCorrectFOV;
vmCvar_t  cg_drawTeamScores;
vmCvar_t  cg_weapmodes;

vmCvar_t  cg_gear;
vmCvar_t  cg_fun;
vmCvar_t  cg_race;
vmCvar_t  cg_armbandrgb;

vmCvar_t  cg_showBulletHits;

//vmCvar_t  cg_radar;
vmCvar_t  cg_viewBlob;
//vmCvar_t  cg_radarRange;
vmCvar_t  cg_autoScreenshot;
vmCvar_t  cg_zoomWrap;

// Special effects enable / disable
vmCvar_t  cg_sfxMuzzleFlash;
vmCvar_t  cg_sfxSurfaceImpacts;
vmCvar_t  cg_sfxBreathing;
vmCvar_t  cg_sfxBrassTime;
vmCvar_t  cg_sfxShowDamage;
vmCvar_t  cg_sfxParticles;
vmCvar_t  cg_sfxVisibleItems;
vmCvar_t  cg_autoRadio;

//mini map stuff
vmCvar_t  cg_mapsize;        //size, 0-512
vmCvar_t  cg_mappos;         //0, left, 1, right, 2, center screen etc

vmCvar_t  cg_mapalpha;        // 0 -- 1
vmCvar_t  cg_maparrowscale;   // 0 -- 1;
vmCvar_t  cg_maptoggle;       // 0/1

vmCvar_t  cg_speedo;         //0, left, 1, right, 2, center screen etc

// Cvars to be locked
vmCvar_t  cg_shadows;
vmCvar_t  cl_timeNudge;
vmCvar_t  r_singleShader;
vmCvar_t  r_fullBright;
vmCvar_t  r_showTris;
vmCvar_t  r_showNormals;
vmCvar_t  rate;
vmCvar_t  snaps;
vmCvar_t  cl_maxpackets;
vmCvar_t  r_maxpolys;
vmCvar_t  r_maxpolyverts;
vmCvar_t  r_clear;
vmCvar_t  r_debugSort;
vmCvar_t  r_debugSurface;
vmCvar_t  r_directedScale;
vmCvar_t  r_drawworld;
vmCvar_t  r_lockpvs;
vmCvar_t  r_nocull;
vmCvar_t  r_nocurves;
vmCvar_t  r_noportals;
vmCvar_t  r_novis;
vmCvar_t  r_showcluster;
vmCvar_t  r_picmip;
vmCvar_t  r_overbrightbits;
vmCvar_t  r_ambientscale;
vmCvar_t  r_vertexlight;
vmCvar_t  r_mapOverBrightBits;
vmCvar_t  r_lightmap;
vmCvar_t  r_intensity;
vmCvar_t  r_subdivisions;
vmCvar_t  r_colormiplevels;
vmCvar_t  com_maxfps;
vmCvar_t  cl_pitchspeed;
vmCvar_t  cl_packetdup;
vmCvar_t  ut_timenudge;
vmCvar_t  r_fastsky;
vmCvar_t  cl_nodelta;
vmCvar_t  ui_team;
vmCvar_t  r_lodCurveError; //@Barbatos
vmCvar_t  cl_guidServerUniq; //@Barbatos

vmCvar_t  ut_dummy;

//
// Shoutcasting Cvar
//
vmCvar_t  cg_SpectatorShoutcaster;
/***
    s.e.t.i. 8/20/2010: adding new shoutcaster follow capabilities
***/
vmCvar_t  cg_followpowerup;
vmCvar_t  cg_followkiller;

//
// Weapon Mode
//
vmCvar_t  cg_WeaponModes;

vmCvar_t  cg_autoRecordMatch;

vmCvar_t  cg_pauseYaw;
vmCvar_t  cg_pausePitch;

vmCvar_t  cg_hudWeaponInfo;

vmCvar_t  cg_physics;
vmCvar_t  cg_optimize;

vmCvar_t  cg_autoPickup;

/**
 * Aries
 */
vmCvar_t  r_debugAries;

vmCvar_t  cg_drawKillLog;

//vmCvar_t  r_primitives;

//@Barbatos
//vmCvar_t  r_smp;

//Fenix: jump mode
vmCvar_t cg_walljumps;  // Synced with server: maximum number of allowed walljumps
vmCvar_t cg_stamina;    // Synced with server: stamina behavior (0 = default, 1 = regain, 2 = infinite)
vmCvar_t cg_ghost;

vmCvar_t cl_yawspeed;
vmCvar_t cg_time1; // @r00t: Haxxor detection
vmCvar_t cg_time2; // @r00t: Haxxor detection
vmCvar_t cl_time; // @r00t: Haxxor detection

typedef struct {
    vmCvar_t  *vmCvar;
    char      *cvarName;
    char      *defaultString;
    int        cvarFlags;
} cvarTable_t;

static cvarTable_t  cvarTable[] = {
    { &cg_drawKillLog,         "cg_drawKillLog",              "1",           CVAR_ARCHIVE             },
    { &cg_autoswitch,          "cg_autoswitch",               "1",           CVAR_ARCHIVE             },
    { &cg_zoomFov,             "cg_zoomfov",                  "22.5",        CVAR_ARCHIVE             },
    { &cg_fov,                 "cg_fov",                      "90",          CVAR_ARCHIVE             },
    { &cg_demoFov,             "cg_demoFov",                  "90",          CVAR_ARCHIVE             },
    { &cg_viewsize,            "cg_viewsize",                 "100",         CVAR_ARCHIVE             },
    { &cg_stereoSeparation,    "cg_stereoSeparation",         "0.4",         CVAR_ARCHIVE             },

    { &cg_draw2D,              "cg_draw2D",                   "1",           CVAR_ARCHIVE             },
    { &cg_drawStatus,          "cg_drawStatus",               "1",           CVAR_ARCHIVE             },
    { &cg_drawTimer,           "cg_drawTimer",                "1",           CVAR_ARCHIVE             },
    { &cg_drawFPS,             "cg_drawFPS",                  "1",           CVAR_ARCHIVE             },

    { &cg_drawSnapshot,        "cg_drawSnapshot",             "0",           CVAR_ARCHIVE             },
    { &cg_drawCrosshair,       "cg_drawCrosshair",            "11",          CVAR_ARCHIVE             },
    { &cg_drawCrosshairNames,  "cg_drawCrosshairNames",       "1",           CVAR_ARCHIVE             },
    { &cg_crosshairNamesType,  "cg_crosshairNamesType",       "3",           CVAR_ARCHIVE             },
    { &cg_crosshairNamesSize,  "cg_crosshairNamesSize",       "0.3",         CVAR_ARCHIVE             },
    { &cg_crosshairSize,       "cg_crosshairSize",            "5",           CVAR_ARCHIVE             },
    { &cg_crosshairX,          "cg_crosshairX",               "0",           CVAR_ARCHIVE             },

    { &cg_crosshairY,          "cg_crosshairY",               "0",           CVAR_ARCHIVE             },
    { &cg_crosshairRGB,        "cg_crosshairRGB",             "1 1 1 1",     CVAR_ARCHIVE             },
    { &cg_crosshairFriendRGB,  "cg_crosshairFriendRGB",       "1 0 0 1",     CVAR_ARCHIVE             },
    { &cg_scopeRGB,            "cg_scopeRGB",                 "0 0 0 1",     CVAR_ARCHIVE             },
    { &cg_scopeFriendRGB,      "cg_scopeFriendRGB",           "1 0 0 1",     CVAR_ARCHIVE             },
    //{ &cg_scopeType,         "cg_scopeType",                "1",           CVAR_ARCHIVE             },
    { &cg_scopeG36,            "cg_scopeG36",                 "0",           CVAR_ARCHIVE             },
    { &cg_scopePSG,            "cg_scopePSG",                 "0",           CVAR_ARCHIVE             },
    { &cg_scopering,           "cg_scopering",                "1",           CVAR_ARCHIVE             },
    { &cg_nvg,                 "cg_nvg",                      "0",           CVAR_ARCHIVE             },
    { &cg_scopeSR8,            "cg_scopeSR8",                 "0",           CVAR_ARCHIVE             },
/*
    { &cg_simpleItems,         "cg_simpleItems",              "0",           CVAR_ARCHIVE             },
*/
//  { &cg_addMarks,            "cg_marks",                    "1",           CVAR_ARCHIVE             },
    { &cg_lagometer,           "cg_lagometer",                "1",           CVAR_ARCHIVE             },
    { &cg_gun_x,               "cg_gunX",                     "0",           CVAR_CHEAT               },

    { &cg_gun_y,               "cg_gunY",                     "0",           CVAR_CHEAT               },
    { &cg_gun_z,               "cg_gunZ",                     "0",           CVAR_CHEAT               },
    { &cg_centertime,          "cg_centertime",               "3",           CVAR_CHEAT               },
    { &cg_runpitch,            "cg_runpitch",                 "0.000",       CVAR_ARCHIVE             },
    { &cg_runroll,             "cg_runroll",                  "0.000",       CVAR_ARCHIVE             },

    { &cg_bobup,               "cg_bobup",                    "0.000",       CVAR_ARCHIVE             },
    { &cg_bobpitch,            "cg_bobpitch",                 "0.000",       CVAR_ARCHIVE             },
    { &cg_bobroll,             "cg_bobroll",                  "0.000",       CVAR_ARCHIVE             },
    { &cg_swingSpeed,          "cg_swingSpeed",               "0.3",         CVAR_CHEAT               },
    { &cg_animSpeed,           "cg_animspeed",                "1",           CVAR_CHEAT               },

    { &cg_debugAnim,           "cg_debuganim",                "0",           CVAR_CHEAT               },
    { &cg_debugPosition,       "cg_debugposition",            "0",           CVAR_CHEAT               },
#ifndef ENCRYPTED
    { &cg_debugEvents,         "cg_debugevents",              "0",           CVAR_CHEAT               }, //@r00t: Disabled in release
#endif
    { &cg_errorDecay,          "cg_errordecay",               "100",         0                        },
    { &cg_nopredict,           "cg_nopredict",                "0",           0                        },
//  { &cg_noweaponpredict,     "cg_noweaponpredict",          "0",           0                        },
//  { &cg_antilag,             "cg_antilag",                  "1",           CVAR_ARCHIVE             }, // | CVAR_USERINFO

    { &cg_noPlayerAnims,       "cg_noplayeranims",            "0",           CVAR_CHEAT               },
    { &cg_showmiss,            "cg_showmiss",                 "0",           0                        },
    { &cg_servertime,          "cg_servertime",               "0",           0                        },

    { &cg_footsteps,           "cg_footsteps",                "1",           CVAR_CHEAT               },
    { &cg_tracerChance,        "cg_tracerchance",             "0.8",         CVAR_CHEAT               },
    { &cg_tracerWidth,         "cg_tracerwidth",              "1",           CVAR_CHEAT               },

    { &cg_tracerLength,        "cg_tracerlength",             "100",         CVAR_CHEAT               },
    { &cg_thirdPersonRange,    "cg_thirdPersonRange",         "40",          CVAR_CHEAT               },
    { &cg_thirdPersonAngle,    "cg_thirdPersonAngle",         "0",           CVAR_CHEAT               },
    { &cg_thirdPerson,         "cg_thirdPerson",              "0",           CVAR_CHEAT               },
    { &cg_teamChatTime,        "cg_teamChatTime",             "3000",        CVAR_ARCHIVE             },

    { &cg_teamChatHeight,      "cg_teamChatHeight",           "0",           CVAR_ARCHIVE             },
    { &cg_drawTeamOverlay,     "cg_drawTeamOverlay",          "1",           CVAR_ARCHIVE             },
    { &cg_drawTeamOverlayScores,"cg_drawTeamOverlayScores",   "1",           CVAR_ARCHIVE             },

    { &cg_drawFriend,          "cg_drawFriend",               "1",           CVAR_ARCHIVE             },
    { &cg_teamChatsOnly,       "cg_teamChatsOnly",            "0",           CVAR_ARCHIVE             },
    { &cg_chatSound,           "cg_chatSound",                "2",           CVAR_ARCHIVE             }, // B1n: No sounds on chat
    { &cg_cleanFollow,         "cg_cleanFollow",              "0",           CVAR_ARCHIVE             }, // B1n: Cleaner HUD for following
    { &cg_noci,                "cg_noci",                     "0",           CVAR_ARCHIVE             }, // B1n: Don't show CI message
    { &cg_noVoiceChats,        "cg_noVoiceChats",             "0",           CVAR_ARCHIVE             },
    { &cg_noVoiceText,         "cg_noVoiceText",              "0",           CVAR_ARCHIVE             },
    { &cg_buildScript,         "com_buildScript",             "0",           0                        }, // force loading of all possible data amd error on failures

    { &cg_paused,              "cl_paused",                   "0",           CVAR_ROM                 },
    { &cg_blood,               "com_blood",                   "3",           CVAR_ARCHIVE             },
    { &cg_synchronousClients,  "g_synchronousClients",        "0",           0 }, // communicated by systeminfo

    { &cg_singlePlayer,        "ui_singlePlayerActive",       "0",           0}, // CVAR_USERINFO
    { &cg_enableDust,          "g_enableDust",                "0",           CVAR_SERVERINFO          },
    { &cg_enableBreath,        "g_enableBreath",              "0",           CVAR_SERVERINFO          },
    { &cg_recordSPDemo,        "ui_recordSPDemo",             "0",           CVAR_ARCHIVE             },

    { &cg_recordSPDemoName,    "ui_recordSPDemoName",         "",            CVAR_ARCHIVE             },
    { &cg_hudFiles,            "cg_hudFiles",                 "ui/hud.txt",  CVAR_ARCHIVE             },
    { &cg_cameraOrbit,         "cg_cameraOrbit",              "0",           CVAR_CHEAT               },
    { &cg_cameraOrbitDelay,    "cg_cameraOrbitDelay",         "50",          CVAR_ARCHIVE             },
    { &cg_timescaleFadeEnd,    "cg_timescaleFadeEnd",         "1",           0                        },
    { &cg_timescaleFadeSpeed,  "cg_timescaleFadeSpeed",       "0",           0                        },
    { &cg_timescale,           "timescale",                   "1",           0                        },
    { &cg_smoothClients,       "cg_smoothClients",            "0",           CVAR_ARCHIVE             }, // | CVAR_USERINFO
    { &cg_cameraMode,          "com_cameraMode",              "0",           CVAR_CHEAT               },

//  { &pmove_fixed,            "pmove_fixed",                 "0",           0                        },
//  { &pmove_msec,             "pmove_msec",                  "8",           0                        },
    { &cg_noTaunt,             "cg_noTaunt",                  "0",           CVAR_ARCHIVE             },
    { &cg_smallFont,           "ui_smallFont",                "0.25",        CVAR_ARCHIVE             },
    { &cg_bigFont,             "ui_bigFont",                  "0.4",         CVAR_ARCHIVE             },

    // Urban Terror cvars
    { &cg_maxFragments,        "cg_maxFragments",             "32",          CVAR_ARCHIVE             },
//  { &cg_spaceInvaders,       "cg_spaceInvaders",            "0",           0                        },
    { &cg_markTotaltime,       "cg_markTotaltime",            "20000",       0                        },

    { &cg_doWarmup,            "g_doWarmup",                  "0",           CVAR_SERVERINFO          },
    { &cg_visibleBleeding,     "cg_visibleBleeding",          "1",           CVAR_ARCHIVE             },
    { &cg_hitsound,            "cg_hitsound",                 "0",           CVAR_ARCHIVE             },
    { &cg_skinAlly,            "cg_skinAlly",                 "0",           CVAR_ARCHIVE             },
    { &cg_skinEnemy,           "cg_skinEnemy",                "0",           CVAR_ARCHIVE             },
    { &cg_funstuff,            "cg_funstuff",                 "1",           CVAR_ARCHIVE             },

    { &cg_drawHands,           "cg_drawHands",                "1",           CVAR_ARCHIVE             },
//  { &cg_debugAW,             "cg_debugAW",                  "0",           CVAR_CHEAT               },

    { &cg_chatHeight,          "cg_chatHeight",               "4",           CVAR_ARCHIVE             },
    { &cg_chatTime,            "cg_chatTime",                 "4000",        CVAR_ARCHIVE             },
    { &cg_msgHeight,           "cg_msgHeight",                "4",           CVAR_ARCHIVE             },
    { &cg_msgTime,             "cg_msgTime",                  "4000",        CVAR_ARCHIVE             },
//  { &cg_maxPrecip,           "cg_maxPrecip",                "128",         CVAR_ARCHIVE             },
//  { &cg_boundsoptimize,      "cg_boundsoptimize",           "1",           CVAR_ARCHIVE             },

    { &cg_gunSize,             "cg_gunSize",                  "0",           CVAR_ARCHIVE             },
//  { &cg_gunCorrectFOV,       "cg_gunCorrectFOV",            "1",           CVAR_ARCHIVE             },
    { &cg_sfxMuzzleFlash,      "cg_sfxMuzzleFlash",           "1",           CVAR_ARCHIVE             },

    { &cg_sfxSurfaceImpacts,   "cg_sfxSurfaceImpacts",        "1",           CVAR_ARCHIVE             },
    { &cg_sfxBreathing,        "cg_sfxBreathing",             "1",           CVAR_ARCHIVE             },
    { &cg_sfxShowDamage,       "cg_sfxShowDamage",            "1",           CVAR_ARCHIVE             },

    { &cg_sfxBrassTime,        "cg_sfxBrassTime",             "2500",        CVAR_ARCHIVE             },

    { &cg_showBulletHits,      "cg_showBulletHits",           "2",           CVAR_ARCHIVE             },

    { &cg_standardChat,        "cg_standardChat",             "0",           CVAR_ARCHIVE             },

    { &cg_drawTeamScores,      "cg_drawTeamScores",           "1",           CVAR_ARCHIVE             },

    //{ &cg_radar,             "cg_radar",                    "0",           CVAR_ARCHIVE             },
    //{ &cg_radarRange,        "cg_radarRange",               "2000",        CVAR_ARCHIVE             },
    { &cg_autoScreenshot,      "cg_autoScreenshot",           "0",           CVAR_ARCHIVE             },

    { &cg_sfxParticles,        "cg_sfxParticles",             "1",           CVAR_ARCHIVE             },
    { &cg_sfxVisibleItems,     "cg_sfxVisibleItems",          "1",           CVAR_ARCHIVE             },
    { &cg_viewBlob,            "cg_viewBlob",                 "1",           CVAR_ARCHIVE             },
    { &cg_zoomWrap,            "cg_zoomWrap",                 "1",           CVAR_ARCHIVE             },
    { &cg_autoRadio,           "cg_autoRadio",                "0",           CVAR_ARCHIVE             },
    { &cg_maptoggle,           "cg_maptoggle",                "1",           CVAR_ARCHIVE             },
    { &cg_mapsize,             "cg_mapsize",                  "150",         CVAR_ARCHIVE             },
    { &cg_mappos,              "cg_mappos",                   "1",           CVAR_ARCHIVE             },
    { &cg_speedo,              "cg_speedo",                   "0",           CVAR_ARCHIVE             },
    { &cg_mapalpha,            "cg_mapalpha",                 "0.7",         CVAR_ARCHIVE             },
    { &cg_maparrowscale,       "cg_maparrowscale",            "3.0",         CVAR_ARCHIVE             },
    { &cg_SpectatorShoutcaster,"cg_SpectatorShoutcaster",     "1",           CVAR_ARCHIVE             },
/***
    s.e.t.i. : cg_followpowerup and cg_followkiller 8/20/2010
***/
    { &cg_followpowerup,       "cg_followpowerup",            "0",           CVAR_ARCHIVE             },
    { &cg_followkiller,        "cg_followkiller",             "0",           CVAR_ARCHIVE             },

    { &cg_WeaponModes,         "weapmodes_save",              "0100011022000002001", CVAR_ARCHIVE     },

    { &cg_autoRecordMatch,     "cg_autoRecordMatch",          "1",           CVAR_ARCHIVE             },

    { &cg_pauseYaw,            "cg_pauseYaw",                 "0",           CVAR_ARCHIVE             },
    { &cg_pausePitch,          "cg_pausePitch",               "0",           CVAR_ARCHIVE             },

    { &cg_hudWeaponInfo,       "cg_hudWeaponInfo",            "0",           CVAR_ARCHIVE             },

    { &cg_physics,             "cg_physics",                  "1",           CVAR_ARCHIVE | CVAR_USERINFO },
    { &cg_optimize,            "cg_optimize",                 "1",           CVAR_ARCHIVE             },

    { &cg_autoPickup,          "cg_autoPickup",               "-1",          CVAR_ARCHIVE | CVAR_USERINFO },

    // protected cvars
    { &cl_timeNudge,           "cl_timeNudge",                "0",           0                        },
    { &r_singleShader,         "r_singleShader",              "0",           0                        },
    { &r_fullBright,           "r_fullBright",                "0",           0                        },
    { &r_showTris,             "r_showTris",                  "0",           0                        },
    { &cg_shadows,             "cg_shadows",                  "0",           0                        },
    { &r_showNormals,          "r_showNormals",               "0",           0                        },
    { &rate,                   "rate",                        "25000",       CVAR_ARCHIVE             },
    { &snaps,                  "snaps",                       "20",          CVAR_TEMP               },
    { &cl_maxpackets,          "cl_maxpackets",               "60",          CVAR_ARCHIVE             },
    { &r_maxpolys,             "r_maxpolys",                  "1800",        CVAR_ARCHIVE             },
    { &r_maxpolyverts,         "r_maxpolyverts",              "9000",        CVAR_ARCHIVE             },
    { &r_clear,                "r_clear",                     "0",           0                        },
    { &r_debugSort,            "r_debugSort",                 "0",           0                        },
    { &r_debugSurface,         "r_debugSurface",              "0",           0                        },
    { &r_directedScale,        "r_directedScale",             "0",           CVAR_ARCHIVE             },
    { &r_drawworld,            "r_drawworld",                 "1",           0                        },
    { &r_lockpvs,              "r_lockpvs",                   "0",           0                        },
    { &r_nocull,               "r_nocull",                    "0",           0                        },
    { &r_nocurves,             "r_nocurves",                  "0",           0                        },
    { &r_noportals,            "r_noportals",                 "0",           0                        },
    { &r_novis,                "r_novis",                     "0",           0                        },
    { &r_showcluster,          "r_showcluster",               "0",           0                        }, //@r00t:was mistyped (r_showclusters)
    { &r_picmip,               "r_picmip",                    "0",           CVAR_ARCHIVE             },
    { &r_overbrightbits,       "r_overbrightbits",            "0",           CVAR_ARCHIVE             },
    { &r_ambientscale,         "r_ambientscale",              "0.8",         CVAR_ARCHIVE             },

    { &r_vertexlight,          "r_vertexlight",               "0",           CVAR_ARCHIVE             },
    { &r_mapOverBrightBits,    "r_mapOverBrightBits",         "1",           CVAR_ARCHIVE             },
    { &r_lightmap,             "r_lightmap",                  "0",           CVAR_CHEAT               },
   // { &r_smp,                  "r_smp",                       "0",           CVAR_ROM                 }, //@Barbatos
    { &r_intensity,            "r_intensity",                 "1",           CVAR_ROM                 }, //@Barbatos: locked for 4.2
    { &r_subdivisions,         "r_subdivisions",              "4",           CVAR_ARCHIVE             },
    { &r_colormiplevels,       "r_colormiplevels",            "0",           CVAR_ARCHIVE             },
    { &com_maxfps,             "com_maxfps",                  "60",          CVAR_ARCHIVE             },
    { &cl_pitchspeed,          "cl_pitchspeed",               "140",         CVAR_ARCHIVE             },
    { &cl_packetdup,           "cl_packetdup",                "1",           CVAR_ARCHIVE             },
    { &ut_timenudge,           "ut_timeNudge",                "25",          CVAR_ARCHIVE | CVAR_USERINFO },
    { &r_fastsky,              "r_fastsky",                   "0",           CVAR_ARCHIVE             },
    { &ui_team,                "ui_team",                     "0",           CVAR_ROM                 },
    { &r_debugAries,           "r_debugAries",                "0",           CVAR_CHEAT               },
    { &r_lodCurveError,        "r_lodCurveError",             "800",         CVAR_ARCHIVE             },
    { &cl_guidServerUniq,      "cl_guidServerUniq",           "0",           CVAR_ROM                 },
    { &cl_nodelta ,            "cl_nodelta",                  "0",           0                        },
    //{ &r_primitives,           "r_primitives",                "0",           0                        },
    { &cg_walljumps,           "g_walljumps",                 "3",           CVAR_SERVERINFO          },     // Fenix
    { &cg_stamina,             "g_stamina",                   "0",           CVAR_SERVERINFO          },     // Fenix
    { &cg_ghost,               "cg_ghost",                    "0",           CVAR_ARCHIVE | CVAR_USERINFO }, // Fenix
    { &cl_yawspeed,            "cl_yawspeed",                 "140",         CVAR_ARCHIVE             }, // @r00t: 180deg macro

    { &cl_time,                "cl_time",                     "0",           CVAR_ARCHIVE|CVAR_USERINFO}, // @r00t: Haxxor detection
    { &cg_time1,               "cg_time1",                    "0",           CVAR_ARCHIVE             }, // @r00t: Haxxor detection
    { &cg_time2,               "cg_time2",                    "0",           CVAR_ARCHIVE             }, // @r00t: Haxxor detection

    //note: If you are adding anything to this table, first search if it's not there already.
};

static int      cvarTableSize = sizeof(cvarTable) / sizeof(cvarTable[0]);

// RR2DO2's data structs for CVAR locking

#define LOCK_FLOAT      (1 << 0)    // determines if value is integer or float
#define LOCK_VIDRESTART (1 << 1)    // should this cvar cause a restart if changed?

#define LOCK_MAXCHANGES 2     // number of times a LOCK_VIDRESTART cvar
                  //  is allowed to change before we enforce
                  //  a vid_restart


typedef struct {
    vmCvar_t  *vmCvar;
    char      *cvarName;
    int       defaultValue;
    int       min;
    int       max;

    // type now holds bit-flags.  Thaddeus
    int  type;          // D8: 0 - int, 1 - float

    // these aren't really useful for cvars that allow big values
    int  includeValues;
    int  excludeValues;
    int  haxcheck;     // make sure this cvar is used by engine (r00t)
} cvarLimitTable_t;

/*
Cvars not present in original quake3.exe:
 cg_demoFov cg_bobup cg_bobpitch ut_timenudge cg_markTotaltime cg_sfxbrasstime cg_physics cl_guidServerUniq
Cvars not in Barbatos client:
 cg_demoFov cg_bobup cg_bobpitch ut_timenudge cg_markTotaltime cg_sfxbrasstime cg_physics

These MUST NOT be marked with haxcheck=1, otherwise will be false positives!
If you add any cvars that are not in original quake3.exe, don't use haxcheck.
They are marked with "!inQ3" comment below.
*/
static float float_limits[3] = {0,1,0.8};
#define FL0  0
#define FL1  1
#define FL08 2
// we have only few float vars, so make all values int (so we don't have to do string->value conversion every time)
// and for floats, use int as index to table above

static cvarLimitTable_t  cvarLimitTable[] = {
    //{ &,                  "",                default,     min,    max,                type , 0,  0 },
    { &cg_shadows,         "cg_shadows",          0,     0,    0,                    0, 0, 0, 1 },
    { &cg_demoFov,         "cg_demoFov",          90,    70,   140,                  0, 0, 0, 0 }, //!inQ3
    { &cl_timeNudge,       "cl_timeNudge",        0,     0,    0,                    0, 0, 0, 1 },
    { &r_singleShader,     "r_singleShader",      0,     0,    0,                    0, 0, 0, 1 },
    { &r_fullBright,       "r_fullBright",        0,     0,    0,                    0, 0, 0, 1 },
    { &r_showTris,         "r_showTris",          0,     0,    -1,                   0, 0, 0, 1 },
    { &r_showNormals,      "r_showNormals",       0,     0,    -1,                   0, 0, 0, 1 },
    { &rate,               "rate",                16000, 8000, 32000,                0, 0, 0, 1 }, //@r00t: was 25000, but that would limit: 1548(max packet)*20(sv_fps)=30960 (rounded to 32000 = 256kbit/sec)
    { &snaps,              "snaps",               20,    20,   125,                   0, 0, 0, 1 },
    { &cl_maxpackets,      "cl_maxpackets",       80,    30,   125,    LOCK_VIDRESTART, 0, 0, 1 }, // Max lowered
    { &r_maxpolys,         "r_maxpolys",          1800,  1800, -1,                   0, 0, 0, 1 },
    { &r_maxpolyverts,     "r_maxpolyverts",      9000,  9000, -1,                   0, 0, 0, 1 },
    { &r_clear,            "r_clear",             0,     0,    0,                    0, 0, 0, 1 },
    { &r_debugSort,        "r_debugSort",         0,     0,    0,                    0, 0, 0, 1 },
    { &r_debugSurface,     "r_debugSurface",      0,     0,    0,                    0, 0, 0, 1 },
    { &r_directedScale,    "r_directedScale",     0,     0,    2,                    0, 0, 0, 1 },
    { &r_drawworld,        "r_drawworld",         1,     1,    1,                    0, 0, 0, 1 },
    { &r_lockpvs,          "r_lockpvs",           0,     0,    0,                    0, 0, 0, 1 },
    { &r_nocull,           "r_nocull",            0,     0,    0,                    0, 0, 0, 1 },
    { &r_nocurves,         "r_nocurves",          0,     0,    0,                    0, 0, 0, 1 },
    { &r_noportals,        "r_noportals",         0,     0,    0,                    0, 0, 0, 1 },
    { &r_novis,            "r_novis",             0,     0,    0,                    0, 0, 0, 1 },
    { &r_showcluster,      "r_showcluster",       0,     0,    0,                    0, 0, 0, 1 }, //@r00t:was mistyped (r_showclusters)
    { &r_picmip,           "r_picmip",            0,     0,    2,                    0, 0, 0, 1 },
    { &r_overbrightbits,   "r_overbrightbits",    0,     0,    0,                    0, 0, 0, 1 },
    { &r_ambientscale,     "r_ambientscale",      FL08,  FL08, FL08,        LOCK_FLOAT, 0, 0, 1 },
    { &r_vertexlight,      "r_vertexlight",       0,     0,    0,                    0, 0, 0, 1 },
    { &r_mapOverBrightBits,"r_mapOverBrightBits", 0,     0,    0,                    0, 0, 0, 1 },
//  { &r_lightmap,         "r_lightmap",          0,     0,    0,                    0, 0, 0, 0    },
    { &cg_bobup,           "cg_bobup",            FL0,   FL0,  FL0,         LOCK_FLOAT, 0, 0, 0 }, //!inQ3
    { &cg_bobpitch,        "cg_bobpitch",         FL0,   FL0,  FL0,         LOCK_FLOAT, 0, 0, 0 }, //!inQ3
    { &r_intensity,        "r_intensity",         FL1,   FL1,  FL1,         LOCK_FLOAT | LOCK_VIDRESTART, 0, 0, 1 },
   // { &r_smp,              "r_smp",               FL0,   FL0,  FL0,         LOCK_FLOAT | LOCK_VIDRESTART, 0, 0, 1 }, //@Barba
    { &r_subdivisions,     "r_subdivisions",      4,     4,    4,                    0, 0, 0, 1 },
    { &r_colormiplevels,   "r_colormiplevels",    0,     0,    0,                    0, 0, 0, 1 },
    { &com_maxfps,         "com_maxfps",          30,    30,   1000,                  0, 0, 0, 1 },
    { &cl_pitchspeed,      "cl_pitchspeed",       140,   140,  -1,                   0, 0, 0, 1 },
    { &cl_packetdup,       "cl_packetdup",        2,     1,    3,                    0, 0, 0, 1 },
    { &ut_timenudge,       "ut_timenudge",        0,     0,    50,     LOCK_VIDRESTART, 0, 0, 0 }, //!inQ3
    { &r_fastsky,          "r_fastsky",           0,     0,    0,                    0, 0, 0, 1 },
    { &cl_nodelta,         "cl_nodelta",          0,     0,    0,                    0, 0, 0, 1 },
    { &cg_markTotaltime,   "cg_markTotaltime",    0,     0,    20000,                0, 0, 0, 0 }, //!inQ3
    { &cg_sfxBrassTime,    "cg_sfxbrasstime",     0,     0,    20000,                0, 0, 0, 0 }, //!inQ3
    { &cg_physics,         "cg_physics",          FL1,   FL1,  FL1,    LOCK_VIDRESTART | LOCK_FLOAT, 0, 0, 0 },
    { &r_lodCurveError,    "r_lodCurveError",     800,   250,  10000,                0, 0, 0, 1 },
    { &cl_guidServerUniq,  "cl_guidServerUniq",   0,     0,    0,      LOCK_VIDRESTART, 0, 0, 0 }, //!inQ3
    //{ &r_primitives,       "r_primitives",        0,     0,    2,      LOCK_VIDRESTART, 0, 0, 1 }, //@Barbatos - locked for 4.2, a value of 3 or more will make the game crash or look like a wallhack

    { &cl_yawspeed,        "cl_yawspeed",         140,   0,    500,                  0, 0, 0, 0 }, // @r00t: 180deg script/bind
    //note: If you add anything to this table, you MUST add it also to cvarTable above, or lock will NOT work!
};

static int cvarLimitTableSize = sizeof(cvarLimitTable) / sizeof(cvarLimitTable[0]);

// End RR2DO2

// GREEN, BLUE, RED, PURPLE
skinInfo_t skinInfos[] = {
	// GREEN
	{
	  { 0.f, 255.f, 0., 128.f },
	  "Green",
	  "Griffins",
	  { 0.13f, 0.52f, 0.13f, 1.f },
	  S_COLOR_GREEN,
	  { 1.f, 1.f, 1.f, 1.f },
	  { 0.5f, 0.5f, 0.5f, 1.0f },
	  { 0.f, 1.f, 0.f, 1.0f },
	  { 0.0f, 0.37f, 0.09f, 1.0f },
	  { 0.0f, 0.37f, 0.09f, 0.75f },
	  { 0.0f, 0.37f, 0.09f, 1.0f },
	  { .6f, .6f, .6f, 1.f },
	  { .17f, 0.f, 0.f, 1.f},
	  { 0.6f, 0.9f, 0.6f, 1.0f },
	  { 128.f, 255.f, 128.f, 1.0f },
	  { 1.0f, 1.0f, 0.6f, 1.0f },
	  "Green Flag",
	  "models/flags/green_flag.md3",
	  "models/flags/green_flag_a",
	  "gfx/misc/Flag_Green.tga"
	},
	// BLUE
	{
	  { 0.f, 0., 255.f, 128.f },
	  "Blue",
	  "SWAT",
	  { 0.13f, 0.11f, 0.52f, 1.f },
	  S_COLOR_BLUE,
	  { 1.f, 1.f, 1.f, 1.f },
	  { 0.5f, 0.5f, 0.5f, 1.0f },
	  { 0.f, 0.f, 1.f, 1.0f },
	  { 0.0f, 0.0f, 0.5f, 1.0f },
	  { 0.0f, 0.0f, 0.5f, 0.75f },
	  { 0.61f, 0.81f, 1.0f, 1.0f },
	  { .6f, .6f, .6f, 1.f },
	  { .17f, 0.f, 0.f, 1.f},
	  { 0.6f, 0.6f, 0.9f, 1.0f },
	  { 128.f, 128.f, 255.f, 1.0f },
	  { 0.61f, 0.81f, 1.0f, 1.0f },
	  "Blue Flag",
	  "models/flags/b_flag.md3",
	  "models/flags/b_flag_a",
	  "gfx/misc/Flag_Blue.tga"
	},
	// RED
	{
	  { 255.f, 0.f, 0., 128.f },
	  "Red",
	  "Red Dragons",
	  { 0.51f, 0.13f, 0.13f, 1.f },
	  S_COLOR_RED,
	  { 1.f, 1.f, 1.f, 1.f },
	  { 0.5f, 0.5f, 0.5f, 1.0f },
	  { 1.f, 0.f, 0.f, 1.0f },
	  { 0.5f, 0.0f, 0.0f, 1.0f },
	  { 0.5f, 0.0f, 0.0f, 0.75f },
	  { 0.87f, 0.29f, 0.30f, 1.0f },
	  { .6f, .6f, .6f, 1.f },
	  { .17f, 0.f, 0.f, 1.f},
	  { 0.9f, 0.5f, 0.5f, 1.0f },
	  { 255.f, 128.f, 128.f, 1.0f },
	  { 0.87f, 0.29f, 0.30f, 1.0f },
	  "Red Flag",
	  "models/flags/r_flag.md3",
	  "models/flags/r_flag_a",
	  "gfx/misc/Flag_Red.tga"
	},
	// PURPLE
	{
	  { 107.f, 56.f, 135., 128.f },
	  "Purple",
	  "Purple Haze",
	  { 0.42f, 0.22f, 0.53f, 1.f },
	  S_COLOR_MAGENTA,
	  { 1.f, 1.f, 1.f, 1.f },
	  { 0.5f, 0.5f, 0.5f, 1.0f },
	  { 0.42f, 0.22f, 0.53f, 1.f },
	  { 0.42f, 0.22f, 0.53f, 1.0f },
	  { 0.42f, 0.22f, 0.53f, 0.75f },
	  { 0.42f, 0.22f, 0.53f, 1.0f },
	  { .6f, .6f, .6f, 1.f },
	  { .17f, 0.f, 0.f, 1.f},
	  { 0.4f, 0.2f, 0.5f, 1.0f },
	  { 120.f, 54.f, 128.f, 1.0f },
	  { 0.42f, 0.22f, 0.53f, 1.0f },
	  "Purple Flag",
	  "models/flags/purple_flag.md3",
	  "models/flags/purple_flag_a",
	  "gfx/misc/Flag_Purple.tga"
	},
	// ORANGE
	{
	  { 227.f, 102.f, 52.f, 128.f },
	  "Orange",
	  "Fugitives",
	  { 0.88f, 0.31f, 0.13f, 1.f },
	  S_COLOR_ORANGE,
	  { 1.f, 1.f, 1.f, 1.f },
	  { 0.5f, 0.5f, 0.5f, 1.0f },
	  { 0.88f, 0.31f, 0.13f, 1.f },
	  { 0.88f, 0.31f, 0.13f, 1.f },
	  { 0.88f, 0.31f, 0.13f, 0.75f },
	  { 0.88f, 0.31f, 0.13f, 1.f },
	  { .6f, .6f, .6f, 1.f },
	  { .17f, 0.f, 0.f, 1.f},
	  { 0.8f, 0.5f, 0.5f, 1.0f },
	  { 196.f, 128.f, 128.f, 1.0f },
	  { 0.88f, 0.31f, 0.13f, 1.f },
	  "Orange Flag",
	  "models/flags/orange_flag.md3",
	  "models/flags/orange_flag_a",
	  "gfx/misc/Flag_Orange.tga"
	},
	// OLIVE
	{
	  { 74.f, 77.f, 50.f, 128.f },
	  "Olive",
	  "Oilers",
	  { 0.29f, 0.30f, 0.19f, 1.f },
	  S_COLOR_OLIVE,
	  { 1.f, 1.f, 1.f, 1.f },
	  { 0.5f, 0.5f, 0.5f, 1.0f },
	  { 0.29f, 0.30f, 0.19f, 1.f },
	  { 0.4f, 0.42f, 0.12f, 1.f },
	  { 0.4f, 0.42f, 0.12f, 0.75f },
	  { 0.29f, 0.30f, 0.19f, 1.f },
	  { .6f, .6f, .6f, 1.f },
	  { .17f, 0.f, 0.f, 1.f},
	  { 0.5f, 0.5f, 0.5f, 1.0f },
	  { 128.f, 128.f, 128.f, 1.0f },
	  { 0.29f, 0.30f, 0.19f, 1.f },
	  "Olive Flag",
	  "models/flags/olive_flag.md3",
	  "models/flags/olive_flag_a",
	  "gfx/misc/Flag_Olive.tga"
	},
	// WHITE
	{
	  { 255.f, 255.f, 255.f, 128.f },
	  "White",
	  "Ice Dragons",
	  { 0.85f, 0.85f, 0.85f, 1.f },
	  S_COLOR_WHITE,
	  { 0.f, 0.f, 0.f, 1.f },
	  { 0.5f, 0.5f, 0.5f, 1.0f },
	  { 1.f, 1.f, 1.f, 1.f },
	  {  0.85f, 0.85f, 0.85f, 1.f },
	  {  1.f,  1.f,  1.f, 0.75f },
	  {  1.f,  1.f,  1.f, 1.f },
	  { .4f, .3f, .3f, 1.f },
	  { .17f, 0.f, 0.f, 1.f},
	  { 0.9f, 0.9f, 0.9f, 1.0f },
	  { 255.f, 255.f, 255.f, 1.0f },
	  {  1.f,  1.f,  1.f, 1.f },
	  "White Flag",
	  "models/flags/white_flag.md3",
	  "models/flags/white_flag_a",
	  "gfx/misc/Flag_White.tga"
	},
	// BLACK
	{
	  { 0.f, 0.f, 0.f, 128.f },
	  "Black",
	  "Black Ice",
	  { 0.05f, 0.05f, 0.05f, 1.f },
	  S_COLOR_BLACK,
	  { 1.f, 1.f, 1.f, 1.f },
	  { 0.5f, 0.5f, 0.5f, 1.0f },
	  { 0.f, 0.f, 0.f, 1.f },
	  { 0.f, 0.f, 0.f, 1.f },
	  { 0.05f, 0.05f, 0.05f, 0.75f },
	  { 0.1f, 0.1f, 0.1f, 1.f },
	  { .6f, .6f, .6f, 1.f },
	  { .17f, 0.f, 0.f, 1.f},
	  { 0.3f, 0.3f, 0.3f, 1.0f },
	  { 48.f, 48.f, 48.f, 1.0f },
	  { 0.9f, 0.9f, 0.9f, 1.f },
	  "Black Flag",
	  "models/flags/black_flag.md3",
	  "models/flags/black_flag_a",
	  "gfx/misc/Flag_Black.tga"
	},
	// DESERT
	{
	  { 156.f, 131.f, 109.f, 128.f },
	  "Desert",
	  "Sand Demons",
	  { 0.61f, 0.51f, 0.42f, 1.f },
	  S_COLOR_YELLOW,
	  { 1.f, 1.f, 1.f, 1.f },
	  { 0.5f, 0.5f, 0.5f, 1.0f },
	  { 0.61f, 0.51f, 0.42f, 1.f },
	  { 0.61f, 0.51f, 0.42f, 1.f },
	  { 0.61f, 0.51f, 0.42f, 0.75f },
	  { 0.61f, 0.51f, 0.42f, 1.f },
	  { .6f, .6f, .6f, 1.f },
	  { .17f, 0.f, 0.f, 1.f},
	  { 0.6f, 0.5f, 0.4f, 1.0f },
	  { 145.f, 128.f, 96.f, 1.0f },
	  { 0.61f, 0.51f, 0.42f, 1.f },
	  "Desert Flag",
	  "models/flags/desert_flag.md3",
	  "models/flags/desert_flag_a",
	  "gfx/misc/Flag_Desert.tga"
	},
	// Cowboy
	{
	  { 255.f, 255.f, 255.f, 128.f },
	  "Cowboy",
	  "The Cowboys",
	  { 0.85f, 0.85f, 0.85f, 1.f },
	  S_COLOR_WHITE,
	  { 0.f, 0.f, 0.f, 1.f },
	  { 0.5f, 0.5f, 0.5f, 1.0f },
	  { 1.f, 1.f, 1.f, 1.f },
	  {  0.85f, 0.85f, 0.85f, 1.f },
	  {  1.f,  1.f,  1.f, 0.75f },
	  {  1.f,  1.f,  1.f, 1.f },
	  { .4f, .3f, .3f, 1.f },
	  { .17f, 0.f, 0.f, 1.f},
	  { 0.9f, 0.9f, 0.9f, 1.0f },
	  { 255.f, 255.f, 255.f, 1.0f },
	  {  1.f,  1.f,  1.f, 1.f },
	  "Cowboy Flag",
	  "models/flags/cowboy_flag.md3",
	  "models/flags/cowboy_flag_a",
	  "gfx/misc/Flag_Cowboy.tga"
	},
	// Cavalry
	{
	  { 112.f, 133.f, 153.f, 128.f },
	  "Cavalry",
	  "Cavalry",
	  { 0.44f, 0.52f, 0.6f, 1.f },
	  S_COLOR_BLUE,
	  { 1.f, 1.f, 1.f, 1.f },
	  { 0.7f, 0.7f, 0.7f, 1.0f },
	  { 0.44f, 0.52f, 0.6f, 1.f },
	  { 0.44f, 0.52f, 0.6f, 1.f },
	  { 0.44f, 0.52f, 0.6f, 1.f },
	  { 0.44f, 0.52f, 0.6f, 1.f },
	  { 0.22f, 0.25f, 0.3f, 1.f },
	  { .17f, 0.f, 0.f, 1.f},
	  { 0.4f, 0.4f, 0.6f, 1.0f },
	  { 112.f, 133.f, 153.f, 1.f },
	  { 0.44f, 0.52f, 0.6f, 1.f },
	  "Cavalry Flag",
	  "models/flags/cavalry_flag.md3",
	  "models/flags/cavalry_flag_a",
	  "gfx/misc/Flag_Cavalry.tga"
	},
	// DROOGS
	{
	  { 250.f, 250.f, 250.f, 128.f },
	  "Droogs",
	  "Droogs",
	  { 0.825f, 0.825f, 0.825f, 1.f },
	  S_COLOR_WHITE,
	  { 0.f, 0.f, 0.f, 1.f },
	  { 0.5f, 0.5f, 0.5f, 1.0f },
	  { 0.95f, 0.95f, 0.95f, 1.f },
	  {  0.825f, 0.825f, 0.825f, 1.f },
	  {  0.95f,  0.95f,  0.95f, 0.75f },
	  {  0.95f,  0.95f,  0.95f, 1.f },
	  { .4f, .3f, .3f, 1.f },
	  { .17f, 0.f, 0.f, 1.f},
	  { 0.875f, 0.875f, 0.875f, 1.0f },
	  { 250.f, 250.f, 250.f, 1.0f },
	  {  0.95f,  0.95f,  0.95f, 1.f },
	  "Droogs Flag",
	  "models/flags/droogs_flag.md3",
	  "models/flags/droogs_flag_a",
	  "gfx/misc/Flag_Droogs.tga"
	},
};


#ifdef ENCRYPTED
char tsync_string[32] = {0};
void SendHaxValues()
// Send current values to server
{
 if (tsync_string[0]) trap_SendClientCommand(tsync_string);
}
#endif


/*
=================
CG_RegisterCvars
=================
*/
void CG_RegisterCvars( void )
{
    int      i;
    cvarTable_t  *cv;
    cvarLimitTable_t  *cvl;
    char         var[MAX_TOKEN_CHARS];

#if 0
    ////////////////////////////////////////////////////////////////////
    //@r00t: Cvar tables check. Enable to check cvar tables consistency
    for (i = 0, cvl = cvarLimitTable; i < cvarLimitTableSize ; i++, cvl++)
    {
        int j;
        cvarLimitTable_t  *cvl2;
        int found = 0;
        for (j = 0, cvl2 = cvarLimitTable; j < cvarLimitTableSize ; j++, cvl2++)
        {
            if (i!=j && !Q_stricmp(cvl->cvarName,cvl2->cvarName))
            {
                CG_Printf("CVAR %s is defined MORE THAN ONCE in limit table!\n",cvl->cvarName);
                break;
            }
        }

        for (j = 0, cv = cvarTable ; j < cvarTableSize ; j++, cv++)
        {
            if (!Q_stricmp(cvl->cvarName,cv->cvarName))
            {
                found = 1;
                break;
            }
        }
        if (!found)
        {
            CG_Printf("CVAR %s is in limit table but not in cvartable! CVAR not locked!\n",cvl->cvarName);
        }
    }
    for (i = 0, cv = cvarTable ; i < cvarTableSize ; i++, cv++)
    {
        int j;
        cvarTable_t  *cv2;
        for (j = 0, cv2 = cvarTable ; j < cvarTableSize ; j++, cv2++)
        {
            if (i!=j && !Q_stricmp(cv->cvarName,cv2->cvarName))
            {
                CG_Printf("CVAR %s is defined MORE THAN ONCE in cvar table!\n",cv->cvarName);
                break;
            }
        }
    }
    ////////////////////////////////////////////////////////////////////
#endif

    //@r00t: Fake cvar detection. All engine cvars are registered by engine at this point. First call to Cvar_Register
    //       will override cvar default value. This way we can detect if this is the first time cvar is Registered.
    //       For cvars registed by engine, default value will not be changed.
    for (i = 0, cvl = cvarLimitTable; i < cvarLimitTableSize ; i++, cvl++) if (cvl->haxcheck)
    {
       int val;
       char *n = cvl->cvarName;
       trap_Cvar_VariableStringBuffer( n, var, sizeof(var));  // Backup original value
       trap_Cvar_Register( cvl->vmCvar, n, "-666", 0 );   // Register cvar with unique default
       trap_Cvar_Set(n,NULL);                                 // Same as "/reset CVAR", sets cvar value to default
       trap_Cvar_Update(cvl->vmCvar);                         // Propagate changes to VM
       val = cvl->vmCvar->integer;                            // Save the value for later, we must do cleanup first
       trap_Cvar_Set(n,var);                                  // Set cvar back to original value
       trap_Cvar_Update(cvl->vmCvar);                         // Propagate original value to VM

       if (val==-666)                                         // If value is our unique value, it's user only cvar
       {
          CG_Error("Engine linkage error #%i\n",i/*cvl->cvarName,var*/);         // FIXME: Silently report? Better error message?
       }
    }


    for (i = 0, cv = cvarTable ; i < cvarTableSize ; i++, cv++)
    {
       trap_Cvar_Register( cv->vmCvar, cv->cvarName, cv->defaultString, cv->cvarFlags );
    }


    // see if we are also running the server on this machine
    trap_Cvar_VariableStringBuffer( "sv_running", var, sizeof(var));
    cgs.localServer = atoi( var );

    trap_Cvar_Register(&cg_race, "racered", DEFAULT_RACE, CVAR_USERINFO | CVAR_ARCHIVE  );
    trap_Cvar_Register(&cg_race, "raceblue", DEFAULT_RACE, CVAR_USERINFO | CVAR_ARCHIVE  );
    trap_Cvar_Register(&cg_race, "racefree", DEFAULT_RACE, CVAR_USERINFO | CVAR_ARCHIVE ); // @Barbatos - free team race
    trap_Cvar_Register(&cg_race, "racejump", DEFAULT_RACE, CVAR_USERINFO | CVAR_ARCHIVE ); // @Barbatos - jump gametype race
    trap_Cvar_Register(&cg_fun, "funred", "", CVAR_USERINFO | CVAR_ARCHIVE  );
    trap_Cvar_Register(&cg_fun, "funblue", "", CVAR_USERINFO | CVAR_ARCHIVE  );
    trap_Cvar_Register(&cg_fun, "funfree", "", CVAR_USERINFO | CVAR_ARCHIVE  );

    trap_Cvar_Register(&cg_armbandrgb, "cg_rgb", "128 128 128", CVAR_USERINFO | CVAR_ARCHIVE  );

    trap_Cvar_Register(&cg_gear, "gear", "GLAORWA", CVAR_USERINFO | CVAR_ARCHIVE );
    trap_Cvar_Register(&cg_weapmodes, "weapmodes", "", CVAR_USERINFO | CVAR_ROM );
    trap_Cvar_Register(NULL, "weapmodes_save", "00000110220000020002", CVAR_ARCHIVE | CVAR_ROM );

    //UT_WP_NUM_WEAPONS = 16
    // Set the weapon modes with the value saved in old.
    memset ( cg.weaponModes, 0, UT_WP_NUM_WEAPONS + 1  );
    trap_Cvar_VariableStringBuffer( "weapmodes_save", cg.weaponModes, UT_WP_NUM_WEAPONS + 1 );

    // Fill in the weapon modes with zeros to ensure they are all represented
    for (i = 0; i < UT_WP_NUM_WEAPONS; i++)
    {
        if (cg.weaponModes[i] == 0)
        {
            cg.weaponModes[i] = '0';
        }
        else if (cg.weaponModes[i] - '0' < 0 || cg.weaponModes[i] - '0' >= MAX_WEAPON_MODES)
        {
            cg.weaponModes[i] = '0';
        }
        else if (!bg_weaponlist[i].modes[cg.weaponModes[i] - '0'].name)
        {
            cg.weaponModes[i] = '0';
        }
    }

    // Send this data to the server.
    trap_Cvar_Set ( "weapmodes", cg.weaponModes );
#ifdef ENCRYPTED
    hax_detect_flags|=(cgs.localServer&1)<<3; // r00t: mark localhost server
#endif
    /* Register Cvars */
    C_RegisterCvars();
}

/*
=================
CG_UpdateCvars
=================
*/
void CG_UpdateCvars( void )
{
    int      i;
    cvarTable_t  *cv;
    char         temp[32];
    double       yaw, pitch;

    CG_LimitCvars();

    for (i = 0, cv = cvarTable ; i < cvarTableSize ; i++, cv++)
    {
        trap_Cvar_Update( cv->vmCvar );
    }

    /* Update cvars */
    C_UpdateCvars();

    // If the crosshair RGB was modified then update the globals
    if (crosshairRGBModificationCount != cg_crosshairRGB.modificationCount)
    {
        if (strchr ( cg_crosshairRGB.string, ',' ))
        {
            sscanf ( cg_crosshairRGB.string, "%f,%f,%f,%f",
                 &cg.crosshairRGB[0],
                 &cg.crosshairRGB[1],
                 &cg.crosshairRGB[2],
                 &cg.crosshairRGB[3] );
        }
        else
        {
            sscanf ( cg_crosshairRGB.string, "%f %f %f %f",
                 &cg.crosshairRGB[0],
                 &cg.crosshairRGB[1],
                 &cg.crosshairRGB[2],
                 &cg.crosshairRGB[3] );
        }

        crosshairRGBModificationCount = cg_crosshairRGB.modificationCount;
    }

    // If the crosshair RGB was modified then update the globals
    if (crosshairFriendRGBModificationCount != cg_crosshairFriendRGB.modificationCount)
    {
        if (strchr ( cg_crosshairFriendRGB.string, ',' ))
        {
            sscanf ( cg_crosshairFriendRGB.string, "%f,%f,%f,%f",
                 &cg.crosshairFriendRGB[0],
                 &cg.crosshairFriendRGB[1],
                 &cg.crosshairFriendRGB[2],
                 &cg.crosshairFriendRGB[3] );
        }
        else
        {
            sscanf ( cg_crosshairFriendRGB.string, "%f %f %f %f",
                 &cg.crosshairFriendRGB[0],
                 &cg.crosshairFriendRGB[1],
                 &cg.crosshairFriendRGB[2],
                 &cg.crosshairFriendRGB[3] );
        }

        crosshairFriendRGBModificationCount = cg_crosshairFriendRGB.modificationCount;
    }

    // If the scope RGB was modified then update the globals
    if (scopeRGBModificationCount != cg_scopeRGB.modificationCount)
    {
        if (strchr ( cg_scopeRGB.string, ',' ))
        {
            sscanf ( cg_scopeRGB.string, "%f,%f,%f,%f",
                 &cg.scopeRGB[0],
                 &cg.scopeRGB[1],
                 &cg.scopeRGB[2],
                 &cg.scopeRGB[3] );
        }
        else
        {
            sscanf ( cg_scopeRGB.string, "%f %f %f %f",
                 &cg.scopeRGB[0],
                 &cg.scopeRGB[1],
                 &cg.scopeRGB[2],
                 &cg.scopeRGB[3] );
        }

        scopeRGBModificationCount = cg_scopeRGB.modificationCount;
    }

    // If the scope friend RGB was modified then update the globals
    if (scopeFriendRGBModificationCount != cg_scopeFriendRGB.modificationCount)
    {
        if (strchr ( cg_scopeFriendRGB.string, ',' ))
        {
            sscanf ( cg_scopeFriendRGB.string, "%f,%f,%f,%f",
                 &cg.scopeFriendRGB[0],
                 &cg.scopeFriendRGB[1],
                 &cg.scopeFriendRGB[2],
                 &cg.scopeFriendRGB[3] );
        }
        else
        {
            sscanf ( cg_scopeFriendRGB.string, "%f %f %f %f",
                 &cg.scopeFriendRGB[0],
                 &cg.scopeFriendRGB[1],
                 &cg.scopeFriendRGB[2],
                 &cg.scopeFriendRGB[3] );
        }

        scopeFriendRGBModificationCount = cg_scopeFriendRGB.modificationCount;
    }
    // check for modications here

    // If team overlay is on, ask for updates from the server.    If its off,
    // let the server know so we don't receive it
    if (drawTeamOverlayModificationCount != cg_drawTeamOverlay.modificationCount)
    {
        drawTeamOverlayModificationCount = cg_drawTeamOverlay.modificationCount;
/*
        if (cg_drawTeamOverlay.integer > 0)
        {
            trap_Cvar_Set( "teamoverlay", "1" );
        }
        else
        {
            trap_Cvar_Set( "teamoverlay", "0" );
        }
*/
        // FIXME E3 HACK
        trap_Cvar_Set( "teamoverlay", "1" );
    }

    // Check for standard chat changing
    {
        static int standardChatModificationCount = -1;

        if (cg_standardChat.modificationCount != standardChatModificationCount)
        {
            if (cg_standardChat.integer)
            {
                trap_Cvar_Set ( "con_notifytime", "3" );
            }
            else
            {
                trap_Cvar_Set ( "con_notifytime", "-2" );
            }

            standardChatModificationCount = cg_standardChat.modificationCount;
        }
    }

    // If it's time to check yaw/pitch values.
    if (!(cg.pauseState & UT_PAUSE_ON) && !(cg.pauseState & UT_PAUSE_TR) && ((cg.lastYawPitchCheck - cg.time) < 0))
    {
        // Get yaw value.
        trap_Cvar_VariableStringBuffer("m_yaw", temp, sizeof(temp));
        yaw = atof(temp);

        // Get pitch value.
        trap_Cvar_VariableStringBuffer("m_pitch", temp, sizeof(temp));
        pitch = atof(temp);

        if (pitch < 0)
        {
            pitch = -pitch;
        }

        // If either is zero.
        if ((yaw == 0.0) || (pitch == 0.0))
        {
            // Notify.
            CG_Printf("m_yaw and/or m_pitch cannot be zero, reseting to defaults.\n");

            // Reset to defaults.
            trap_Cvar_Set("m_yaw", "0.022");
            trap_Cvar_Set("m_pitch", "0.022");
        }
        else
        {
            // If yaw is smaller.
            if (yaw < pitch)
            {
                // If invalid ratio.
                if ((yaw / pitch) < 0.5)
                {
                    // Notify.
                    CG_Printf("m_yaw/m_pitch invalid ratio, reseting to defaults.\n");

                    // Reset to defaults.
                    trap_Cvar_Set("m_yaw", "0.022");
                    trap_Cvar_Set("m_pitch", "0.022");
                }
            }
            else
            {
                // If invalid ratio.
                if ((pitch / yaw) < 0.5)
                {
                    // Notify.
                    CG_Printf("m_pitch/m_yaw invalid ratio, reseting to defaults.\n");

                    // Reset to defaults.
                    trap_Cvar_Set("m_yaw", "0.022");
                    trap_Cvar_Set("m_pitch", "0.022");
                }
            }
        }

        // Set new check time.
        cg.lastYawPitchCheck = cg.time + 10000;
    }

	// skins forcing changed
	if ( cg.warmup
		|| cg.demoPlayback
		|| cg.loading
		|| cgs.clientinfo[cg.clientNum].team == TEAM_SPECTATOR
		|| cgs.gametype == GT_JUMP
		|| ((cg.matchState & UT_MATCH_ON) && !((cg.matchState & UT_MATCH_RR) && (cg.matchState & UT_MATCH_BR))) ) {

		CG_UpdateSkins(0);
	}

/*   if ( cg.predictedPlayerState.pm_flags  & PMF_WATCHING_GTV)
   {
    trap_SendConsoleCommand("cl_timenudge 300\n");
   } */
}

void CG_UpdateSkins( int force ) {
//@r00t: Force used on map load, we really need to have good skin combination to not have missing textures
	int i;
	if ( skinAllyModificationCount != cg_skinAlly.modificationCount
	    || skinEnemyModificationCount != cg_skinEnemy.modificationCount || force ) {

		skinAllyModificationCount = cg_skinAlly.modificationCount;
		skinEnemyModificationCount = cg_skinEnemy.modificationCount;

		if (CG_IsSkinSelectionValid()) {
			cg.cg_skinAlly = cg_skinAlly.integer;
			cg.cg_skinEnemy = cg_skinEnemy.integer;
		} else {
			Com_Printf("Invalid skin settings, keeping last valid selection\n");
		}

		CG_LoadSkinOnDemand(CG_WhichSkin(TEAM_RED)); //@r00t: Load skins on demand
		CG_LoadSkinOnDemand(CG_WhichSkin(TEAM_BLUE));

		for (i = 0; i < MAX_CLIENTS; i++) {
			if (cgs.clientinfo[i].infoValid) {
				CG_SetupPlayerGFX(i, &cgs.clientinfo[i]);
				CG_ReloadHandSkins(i);
			}
		}
	}
}

// RR2DO2's cvar limiting function
// modified to support floats by D8
/*
=================
CG_LimitCvars
=================
*/
void CG_LimitCvars( void )
{
    int       i, miniv, maxiv;
    cvarLimitTable_t  *cvl;
    /*
    char var[MAX_TOKEN_CHARS];
    const char *s;
    */
    float     minfv, maxfv;
    qboolean  change = qfalse;

    // 27: disable developer mode
    /*
    s = Info_ValueForKey( CG_ConfigString( CS_SYSTEMINFO ), "sv_cheats" );
    trap_Cvar_VariableStringBuffer( "developer", var, sizeof( var ) );
    if ( s[0] && atoi(var) )
    {
        return;
    }
    */

    for (i = 0, cvl = cvarLimitTable; i < cvarLimitTableSize ; i++, cvl++)
    {
        change = qfalse;
        switch ((cvl->type & LOCK_FLOAT))
        {
            default:
            case 0: // integer values
                miniv = cvl->min;
                maxiv = cvl->max;
                if(miniv != -1)
                {
                    if(cvl->vmCvar->integer < miniv)
                    {
                        if(cvl->includeValues && (cvl->includeValues & (1 << cvl->vmCvar->integer)))
                        {
                            continue;
                        }

                        trap_Cvar_Set( cvl->cvarName, va( "%i", miniv ));
                        cvl->vmCvar->integer = miniv;

                        if ((cvl->type & LOCK_VIDRESTART) && (cvl->vmCvar->modificationCount > LOCK_MAXCHANGES))
                        {
                            trap_SendConsoleCommand("vid_restart");
                        }
                        change = qtrue;
                        continue;
                    }
                }

                if(maxiv != -1)
                {
                    if(cvl->vmCvar->integer > maxiv)
                    {
                        if(cvl->includeValues && (cvl->includeValues & (1 << cvl->vmCvar->integer)))
                        {
                            continue;
                        }

                        trap_Cvar_Set( cvl->cvarName, va( "%i", maxiv ));
                        cvl->vmCvar->integer = maxiv;

                        if ((cvl->type & LOCK_VIDRESTART) && (cvl->vmCvar->modificationCount > LOCK_MAXCHANGES))
                        {
                            trap_SendConsoleCommand("vid_restart");
                        }
                        change = qtrue;
                        continue;
                    }
                }

                if(cvl->excludeValues && (cvl->excludeValues & (1 << cvl->vmCvar->integer)))
                {
                    trap_Cvar_Set( cvl->cvarName, va( "%s", cvl->defaultValue ));
                    cvl->vmCvar->integer = cvl->defaultValue;

                    if ((cvl->type & LOCK_VIDRESTART) && (cvl->vmCvar->modificationCount > LOCK_MAXCHANGES))
                    {
                        trap_SendConsoleCommand("vid_restart");
                    }
                    change = qtrue;
                    continue;
                }
                break;

            case 1: // float values

                minfv = float_limits[ cvl->min ];
                maxfv = float_limits[ cvl->max ];

                if(minfv != -1.0f)
                {
                    if(cvl->vmCvar->value < minfv)
                    {
                        trap_Cvar_Set( cvl->cvarName, va( "%f", minfv ));
                        cvl->vmCvar->value = minfv;

                        if ((cvl->type & LOCK_VIDRESTART) && (cvl->vmCvar->modificationCount > LOCK_MAXCHANGES))
                        {
                            trap_SendConsoleCommand("vid_restart");
                        }
                        change = qtrue;
                        continue;
                    }
                }

                if(maxfv != -1.0f)
                {
                    if(cvl->vmCvar->value > maxfv)
                    {
                        trap_Cvar_Set( cvl->cvarName, va( "%f", maxfv ));
                        cvl->vmCvar->value = maxfv;

                        if ((cvl->type & LOCK_VIDRESTART) && (cvl->vmCvar->modificationCount > LOCK_MAXCHANGES))
                        {
                            trap_SendConsoleCommand("vid_restart");
                        }
                        change = qtrue;
                        continue;
                    }
                }

                break;
        }

        if (change)
        {
            CG_Printf( "CG_LimitCvars Clamped: %s\n", cvl->cvarName );
        }
    }
}
// end RR2DO2

qboolean CG_IsSkinSelectionValid( void ) {
	switch (cgs.gametype) {
		case GT_FFA: case GT_LASTMAN: case GT_SINGLE_PLAYER: case GT_JUMP:
			return !(cg_skinEnemy.integer > UT_SKIN_COUNT);
		default:
			return !(cg_skinAlly.integer > UT_SKIN_COUNT)
			    && !(cg_skinEnemy.integer > UT_SKIN_COUNT)
			    && !(cg_skinAlly.integer && cg_skinAlly.integer == cg_skinEnemy.integer)
			    && !(!cg_skinAlly.integer && (cg_skinEnemy.integer == 2 || cg_skinEnemy.integer == 3))
			    && !(!cg_skinEnemy.integer && (cg_skinAlly.integer == 2 || cg_skinAlly.integer == 3));
	}
}

int CG_CrosshairPlayer( void )
{
    if (cg.time > (cg.crosshairClientTime + 1000))
    {
        return -1;
    }
    return cg.crosshairClientNum;
}

/**
 * $(function)
 */
int CG_LastAttacker( void )
{
    if (!cg.attackerTime)
    {
        return -1;
    }
    return cg.snap->ps.persistant[PERS_ATTACKER];
}

/**
 * $(function)
 */
void QDECL CG_Printf( const char *msg, ... )
{
    va_list  argptr;
    char     text[1024];

    va_start (argptr, msg);
    vsprintf (text, msg, argptr);
    va_end (argptr);

    CG_AddMessageText ( text );

    Q_CleanStr( text );

    trap_Print( text );
    trap_Print( "\n" );
}

/**
 * $(function)
 */
void QDECL CG_Error( const char *msg, ... )
{
    va_list  argptr;
    char     text[1024];

    va_start (argptr, msg);
    vsprintf (text, msg, argptr);
    va_end (argptr);

    trap_Error( text );
}

// this is only here so the functions in q_shared.c and bg_*.c can link (FIXME)

void QDECL Com_Error( int level, const char *error, ... )
{
    va_list  argptr;
    char     text[1024];

    va_start (argptr, error);
    vsprintf (text, error, argptr);
    va_end (argptr);

    CG_Error( "%s", text);
}

/**
 * Com_Printf
 */
void QDECL Com_Printf( const char *msg, ... )
{
    va_list  argptr;
    char     text[1024];

    va_start (argptr, msg);
    vsprintf (text, msg, argptr);
    va_end (argptr);

    CG_Printf ("%s", text);
}

/*
================
CG_Argv
================
*/
const char *CG_Argv( int arg )
{
    static char  buffer[MAX_STRING_CHARS];

    trap_Argv( arg, buffer, sizeof(buffer));

    return buffer;
}

//========================================================================

/*
=================
CG_RegisterItemSounds

The server says this item is used on this level
=================
*/
static void CG_RegisterItemSounds( int itemNum )
{
    gitem_t  *item;
    char   data[MAX_QPATH];
    char     *s, *start;
    int  len;

    item = &bg_itemlist[itemNum];

    if(item->pickup_sound)
    {
        trap_S_RegisterSound( item->pickup_sound, qfalse );
    }

    // parse the space seperated precache string for other media
    s = item->sounds;

    if (!s || !s[0])
    {
        return;
    }

    while (*s)
    {
        start = s;

        while (*s && *s != ' ')
        {
            s++;
        }

        len = s - start;

        if ((len >= MAX_QPATH) || (len < 5))
        {
            CG_Error( "PrecacheItem: %s has bad precache string",
                  item->classname);
            return;
        }
        memcpy (data, start, len);
        data[len] = 0;

        if (*s)
        {
            s++;
        }

        if (!strcmp(data + len - 3, "wav" ))
        {
            trap_S_RegisterSound( data, qfalse );
        }
    }
}

/*
=================
CG_RegisterSounds

called during a precache command
=================
*/
static void CG_RegisterSounds( void )
{
    int           i, j;
    char          name[MAX_QPATH];
    const char    *soundName;
    char          demoName[1024];
    fileHandle_t  f;
    int           len;

    trap_Cvar_VariableStringBuffer("cg_demoName", demoName, sizeof(demoName));
    len = trap_FS_FOpenFile( va("sound/democasts/%s.wav", demoName), &f, FS_READ );

    if (f) {
        trap_FS_FCloseFile( f );
        cgs.media.ut_demoCast = trap_S_RegisterSound( va("sound/democasts/%s.wav", demoName), qfalse );
        trap_SendConsoleCommand( va("echo Loaded sound/democasts/%s.wav\n", demoName));
        cg.demoCast = qtrue;
    } else {
        cg.demoCast = qfalse;
    }

    trap_Cvar_Set( "g_demoName", "" );

    cgs.media.tracerSound[0]     = trap_S_RegisterSound( "sound/weapons/whiz/whiz1.wav", qfalse );
    cgs.media.tracerSound[1]     = trap_S_RegisterSound( "sound/weapons/whiz/whiz2.wav", qfalse );

    cgs.media.basetracerSound[0] = trap_S_RegisterSound( "sound/weapons/machinegun/ric1.wav", qfalse );
    cgs.media.basetracerSound[1] = trap_S_RegisterSound( "sound/weapons/machinegun/ric2.wav", qfalse );
    cgs.media.basetracerSound[2] = trap_S_RegisterSound( "sound/weapons/machinegun/ric3.wav", qfalse );

    cgs.media.selectSound        = trap_S_RegisterSound( "sound/weapons/UTchange.wav", qfalse );
    cgs.media.gibSound           = trap_S_RegisterSound( "sound/player/gibsplt1.wav", qfalse );
    cgs.media.gibBounce1Sound    = trap_S_RegisterSound( "sound/player/gibimp1.wav", qfalse );
    cgs.media.gibBounce2Sound    = trap_S_RegisterSound( "sound/player/gibimp2.wav", qfalse );
    cgs.media.gibBounce3Sound    = trap_S_RegisterSound( "sound/player/gibimp3.wav", qfalse );

    cgs.media.talkSound          = trap_S_RegisterSound( "sound/player/talk.wav", qfalse );
    cgs.media.landSound          = trap_S_RegisterSound( "sound/player/land1.wav", qfalse);

    cgs.media.watrInSound        = trap_S_RegisterSound( "sound/player/watr_in.wav", qfalse);
    cgs.media.watrInSoundFast    = trap_S_RegisterSound( "sound/player/bodysplash.wav", qfalse);
    cgs.media.watrOutSound       = trap_S_RegisterSound( "sound/player/watr_out.wav", qfalse);
    cgs.media.watrUnSound        = trap_S_RegisterSound( "sound/player/watr_un.wav", qfalse);

    for (i = 0 ; i < 4 ; i++) {

        Com_sprintf (name, sizeof(name), "sound/surfaces/footsteps/splash%i.wav", i + 1);
        cgs.media.footsteps[FOOTSTEP_SPLASH][i] = trap_S_RegisterSound (name, qfalse);

        // Fenix: load also FOOTSTEP_NORMAL. Used in Jump Mode for EV_FALL_MEDIUM / EV_FALL_FAR
        // when g_noDamage is set to 1 -> we have to bypass the pain sound, so let's play this.
        Com_sprintf (name, sizeof(name), "sound/surfaces/footsteps/concrete%i.wav", i + 1);
        cgs.media.footsteps[FOOTSTEP_NORMAL][i] = trap_S_RegisterSound (name, qfalse);

    }

    for (i = 1 ; i < bg_numItems ; i++) {
        CG_RegisterItemSounds( i );
    }

    for (i = 1 ; i < MAX_SOUNDS ; i++) {
        soundName = CG_ConfigString( CS_SOUNDS + i );

        if (!soundName[0]) {
            break;
        }

        if (soundName[0] == '*') {
            continue;       // custom sound
        }

        cgs.gameSounds[i] = trap_S_RegisterSound( soundName, qfalse );
    }

    cgs.media.sfx_ric1              = trap_S_RegisterSound ("sound/weapons/machinegun/ric1.wav", qfalse);
    cgs.media.sfx_ric2              = trap_S_RegisterSound ("sound/weapons/machinegun/ric2.wav", qfalse);
    cgs.media.sfx_ric3              = trap_S_RegisterSound ("sound/weapons/machinegun/ric3.wav", qfalse);
    cgs.media.sfx_plasmaexp         = trap_S_RegisterSound ("sound/weapons/plasma/plasmx1a.wav", qfalse);
    cgs.media.plasmaExplosionShader = trap_R_RegisterShader( "plasmaExplosion" );

    cgs.media.hgrenb1aSound         = trap_S_RegisterSound("sound/weapons/grenade/hgrenb1a.wav", qfalse);
    cgs.media.hgrenb2aSound         = trap_S_RegisterSound("sound/weapons/grenade/hgrenb2a.wav", qfalse);

    cgs.media.hgrenb1bSound         = trap_S_RegisterSound("sound/weapons/grenade/hgrenb1b.wav", qfalse);
    cgs.media.hgrenb2bSound         = trap_S_RegisterSound("sound/weapons/grenade/hgrenb2b.wav", qfalse);

    cgs.media.bombBounce1           = trap_S_RegisterSound("sound/bomb/bomb_hit_floor_1.wav", qfalse);
    cgs.media.bombBounce2           = trap_S_RegisterSound("sound/bomb/bomb_hit_floor_2.wav", qfalse);

    // Urban Terror sounds
    cgs.media.UT_glassShatterSound   = trap_S_RegisterSound( "sound/breakable/glass1.wav", qfalse );
    cgs.media.UT_glassBounceSound    = trap_S_RegisterSound( "sound/breakable/glass2.wav", qfalse );
    cgs.media.UT_woodShatterSound    = trap_S_RegisterSound( "sound/breakable/wood1.wav", qfalse );
    cgs.media.UT_woodBounceSound     = trap_S_RegisterSound( "sound/breakable/wood2.wav", qfalse );
    cgs.media.UT_ceramicShatterSound = trap_S_RegisterSound( "sound/breakable/ceramic1.wav", qfalse );
    cgs.media.UT_ceramicBounceSound  = trap_S_RegisterSound( "sound/breakable/ceramic2.wav", qfalse );
    cgs.media.UT_metalShatterSound = trap_S_RegisterSound("sound/breakable/metal_breaks.wav", qfalse);
    cgs.media.UT_stoneShatterSound = trap_S_RegisterSound("sound/breakable/stone_breaks.wav", qfalse);

    // Explode sound
    cgs.media.UT_explodeSound    = trap_S_RegisterSound ( "sound/weapons/gl/gl_expl.wav", qfalse );
    cgs.media.UT_explodeH2OSound = trap_S_RegisterSound ( "sound/weapons/gl/gl_expl_h2o.wav", qfalse );

    // Zoom sound
    cgs.media.UT_zoomSound = trap_S_RegisterSound( "sound/zoom.wav", qfalse );

    // Zoom sound
    cgs.media.UT_smokegrensound = trap_S_RegisterSound( "sound/smokegren.wav", qfalse );
    cgs.media.sndUnderWater     = trap_S_RegisterSound( "sound/world/underwater.wav", qfalse );

    cgs.media.UT_radiomenuclick = trap_S_RegisterSound( "sound/misc/menu2.wav", qfalse );

    //Iain new ctf sounds.
    //@Fenix - add flag sounds also for JUMP mode
    if((cgs.gametype == GT_CTF) || (cgs.gametype == GT_UT_CAH) || (cgs.gametype == GT_JUMP)) {
        cgs.media.UT_CTF_FlagTaken   = trap_S_RegisterSound( "sound/ctf/stolen.wav", qfalse );
        cgs.media.UT_CTF_FlagReturn  = trap_S_RegisterSound( "sound/ctf/return.wav", qfalse );
        cgs.media.UT_CTF_CaptureBlue = trap_S_RegisterSound( "sound/ctf/capture-blue.wav", qfalse );
        cgs.media.UT_CTF_CaptureRed  = trap_S_RegisterSound( "sound/ctf/capture-red.wav", qfalse );
    }

    // Bandage sound
    cgs.media.UT_bandageSound    = trap_S_RegisterSound("sound/bandage.wav", qfalse);
    cgs.media.UT_bodyImpactSound = trap_S_RegisterSound( "sound/body_impact.wav", qfalse );

    // kicking a player
    cgs.media.kickSound = trap_S_RegisterSound( "sound/kick1.wav", qfalse );

    // bullet brass hitting ground
    cgs.media.UT_brassBounceSound[0] = trap_S_RegisterSound( "sound/weapons/brass1.wav", qfalse );
    cgs.media.UT_brassBounceSound[1] = trap_S_RegisterSound( "sound/weapons/brass2.wav", qfalse );
    cgs.media.UT_shellBounceSound[0] = trap_S_RegisterSound( "sound/weapons/shell1.wav", qfalse );
    cgs.media.UT_shellBounceSound[1] = trap_S_RegisterSound( "sound/weapons/shell2.wav", qfalse );
    cgs.media.UT_shellBounceSound[2] = trap_S_RegisterSound( "sound/weapons/shell3.wav", qfalse );

    cgs.media.UT_dropPistol             = trap_S_RegisterSound ( "sound/weapons/droppistol.wav", qfalse );
    cgs.media.UT_dropRifle              = trap_S_RegisterSound ( "sound/weapons/droprifle.wav", qfalse );
    cgs.media.UT_dropWater              = trap_S_RegisterSound ( "sound/weapons/dropwater.wav", qfalse );
    //
    // Round Sounds - DensitY
    //
    cgs.media.UT_RedWinsRound  = trap_S_RegisterSound( "sound/misc/redwins.wav", qfalse );
    cgs.media.UT_BlueWinsRound = trap_S_RegisterSound( "sound/misc/bluewins.wav", qfalse );
    cgs.media.UT_DrawnRound    = trap_S_RegisterSound( "sound/misc/draw.wav", qfalse );
    cgs.media.UT_LaughRound    = trap_S_RegisterSound( "sound/misc/laugh.wav", qfalse );

    // BOMB beeping sounds
    // TODO: only do this in bomb mode?!
    for(i = 0; i < 3; i++) {
        cgs.media.ut_BombBeep[i] = trap_S_RegisterSound( va( "sound/bomb/beep_stage%i.wav", i + 1 ), qfalse );
    }

    cgs.media.stomp = trap_S_RegisterSound ( "sound/stomp.wav", qfalse );
    cgs.media.hit   = trap_S_RegisterSound ( "sound/feedback/hit.wav", qfalse );

    //@Fenix - jump timer sounds
    cgs.media.UT_JumpTimerStart     = trap_S_RegisterSound( "sound/jump/timerstart.wav", qfalse );
    cgs.media.UT_JumpTimerStop      = trap_S_RegisterSound( "sound/jump/timerstop.wav", qfalse );
    cgs.media.UT_JumpTimerCancel    = trap_S_RegisterSound( "sound/jump/timercancel.wav", qfalse );

    //@Barbatos - register and load all sounds to avoid having to register them when playing (this causes huge lags)
    cgs.media.ledgegrab             = trap_S_RegisterSound( "sound/player/ledgegrab.wav", qfalse );
    cgs.media.water1                = trap_S_RegisterSound( "sound/surfaces/bullets/water1.wav", qfalse );
    cgs.media.laserOnOff            = trap_S_RegisterSound( "sound/items/laseronoff.wav", qfalse );
    cgs.media.nvgOn                 = trap_S_RegisterSound( "sound/items/nvgon.wav", qfalse );
    cgs.media.nvgOff                = trap_S_RegisterSound( "sound/items/nvgoff.wav", qfalse );
    cgs.media.flashLight            = trap_S_RegisterSound( "sound/items/flashlight.wav", qfalse );
    cgs.media.noAmmoSound           = trap_S_RegisterSound( "sound/weapons/beretta/92G_noammo.wav", qfalse );
    cgs.media.bombDisarmSound       = trap_S_RegisterSound( "sound/bomb/Bomb_disarm.wav", qfalse );
    cgs.media.bombExplosionSound    = trap_S_RegisterSound( "sound/bomb/Explode01.wav", qfalse );
    cgs.media.blastWind             = trap_S_RegisterSound( "sound/misc/blast_wind.wav", qfalse );
    cgs.media.blastFire             = trap_S_RegisterSound( "sound/misc/blast_fire.wav", qfalse );
    cgs.media.kcactionSound         = trap_S_RegisterSound( "sound/misc/kcaction.wav", qfalse );

    //@Barbatos - load the radio sounds : [gender][group][msg]
    //@Fenix - Add checks for file existence to prevent failures
    for(i = 1; i < 10; i++) {
        for(j = 1; j < 10; j++) {

        	// Registering female radio sounds
        	len = trap_FS_FOpenFile( va("sound/radio/female/female%i%i.wav", i, j), &f, FS_READ );
        	if (f) {
        		trap_FS_FCloseFile( f );
        		cgs.media.radioSounds[GENDER_FEMALE][i][j] = trap_S_RegisterSound(va("sound/radio/female/female%i%i.wav", i, j), qfalse);
        	}

        	// Registering male radio sounds
			len = trap_FS_FOpenFile( va("sound/radio/male/male%i%i.wav", i, j), &f, FS_READ );
			if (f) {
				trap_FS_FCloseFile( f );
				cgs.media.radioSounds[GENDER_MALE][i][j] = trap_S_RegisterSound(va("sound/radio/male/male%i%i.wav", i, j), qfalse);
			}
        }
    }
}

//===================================================================================

/*
=================
CG_RegisterGraphics

This function may execute for a couple of minutes with a slow disk.
=================
*/
static void CG_RegisterGraphics( void )
{
    int          i;
    static char  *sb_nums[11] = {
            "gfx/2d/numbers/zero_32b",
            "gfx/2d/numbers/one_32b",
            "gfx/2d/numbers/two_32b",
            "gfx/2d/numbers/three_32b",
            "gfx/2d/numbers/four_32b",
            "gfx/2d/numbers/five_32b",
            "gfx/2d/numbers/six_32b",
            "gfx/2d/numbers/seven_32b",
            "gfx/2d/numbers/eight_32b",
            "gfx/2d/numbers/nine_32b",
            "gfx/2d/numbers/minus_32b",
    };

    // clear any references to old media
    memset( &cg.refdef, 0, sizeof(cg.refdef));
    trap_R_ClearScene();

    CG_LoadingString( cgs.mapname );

    trap_R_LoadWorldMap( cgs.mapname );

    // precache status bar pics
    CG_LoadingString( "game media" );

    for (i = 0 ; i < 11 ; i++)
    {
        cgs.media.numberShaders[i] = trap_R_RegisterShader( sb_nums[i] );
    }

    cgs.media.botSkillShaders[0]     = trap_R_RegisterShader( "menu/art/skill1.tga" );
    cgs.media.botSkillShaders[1]     = trap_R_RegisterShader( "menu/art/skill2.tga" );
    cgs.media.botSkillShaders[2]     = trap_R_RegisterShader( "menu/art/skill3.tga" );
    cgs.media.botSkillShaders[3]     = trap_R_RegisterShader( "menu/art/skill4.tga" );
    cgs.media.botSkillShaders[4]     = trap_R_RegisterShader( "menu/art/skill5.tga" );

    cgs.media.viewBloodShader        = trap_R_RegisterShader( "viewBloodBlend" );
    cgs.media.quadShader             = trap_R_RegisterShader("powerups/quad" );

    cgs.media.deferShader            = trap_R_RegisterShaderNoMip( "gfx/2d/defer.tga" );

    cgs.media.smokePuffShader        = trap_R_RegisterShader( "smokePuff" );
    cgs.media.smokePuffRageProShader = trap_R_RegisterShader( "smokePuffRagePro" );
    cgs.media.shotgunSmokePuffShader = trap_R_RegisterShader( "shotgunSmokePuff" );
    cgs.media.grenadeSmokePuffShader = trap_R_RegisterShader( "grenadeSmokePuff" ); //r00t: Must be initialized here, otherwise it blocks NVG (WTF bug)
    cgs.media.viewsmokeshader        = 0;//trap_R_RegisterShader( "viewsmokepuff" ); //r00t: wallhack detection, do it later

    cgs.media.UT_glassFragment       = trap_R_RegisterShader( "glassshardshader" );
    cgs.media.UT_woodFragment        = trap_R_RegisterShader( "woodshardshader" );
    cgs.media.UT_metalFragment       = trap_R_RegisterShader( "metalshardshader" );
    cgs.media.UT_stoneFragment       = trap_R_RegisterShader( "stoneshardshader" );

    cgs.media.bloodTrailShader       = trap_R_RegisterShader( "bloodTrail" );
    cgs.media.lagometerShader        = trap_R_RegisterShader("lagometer" );
    cgs.media.connectionShader       = trap_R_RegisterShader( "disconnected" );
    cgs.media.snowShader             = 0;//trap_R_RegisterShader( "snow" );  //r00t: used wallhack detection

    cgs.media.waterBubbleShader      = trap_R_RegisterShader( "waterBubble" );

    cgs.media.tracerShader           = trap_R_RegisterShader( "gfx/misc/tracer" );
    cgs.media.selectShader           = trap_R_RegisterShader( "gfx/2d/select" );

    cgs.media.backTileShader         = trap_R_RegisterShader( "gfx/2d/backtile" );
    cgs.media.noammoShader           = trap_R_RegisterShader( "icons/noammo" );

    for (i = 0; i < UT_SKIN_COUNT; i++) {
	cgs.media.flagModels[i] = trap_R_RegisterModel( skinInfos[i].asset_flag_model_path );
	cgs.media.colorFlagShaders[i][0] = trap_R_RegisterShaderNoMip( skinInfos[i].asset_color_flag_shader );
	cgs.media.UT_FlagIcon[i] = trap_R_RegisterShaderNoMip ( skinInfos[i].asset_flag_icon );
    }

    cgs.media.UT_FlagNeutralIcon            = trap_R_RegisterShaderNoMip ( "gfx/misc/Flag_Free.tga" );

    cgs.media.UT_FlagMedic                  = trap_R_RegisterShaderNoMip ( "gfx/misc/Flag_Medic.tga" );

    cgs.media.jumpGoalModel                 = trap_R_RegisterModel( "models/goal/goal.md3" );

    cgs.media.sparkBoom                     = trap_R_RegisterShaderNoMip( "hitSpark" ); //@r00t:Blood -> Sparks
    cgs.media.bloodBoom                     = trap_R_RegisterShaderNoMip( "bloodParticle1" );

    if ((cgs.gametype >= GT_TEAM) || cg_buildScript.integer)
    {
        //cgs.media.friendShader  = trap_R_RegisterShader( "sprites/foe" ); //@Barbatos: looks like this one is unused?
        cgs.media.teamStatusBar = trap_R_RegisterShader( "gfx/2d/colorbar.tga" );
    }

    if ((cgs.gametype == GT_UT_CAH) || (cgs.gametype == GT_UT_ASSASIN))
    {
        for (i = 0; i < UT_SKIN_COUNT; i++) {
          cgs.media.flagBaseModels[i] = trap_R_RegisterModel( skinInfos[i].asset_flag_model_path );
        }

        cgs.media.neutralFlagBaseModel   = trap_R_RegisterModel( "models/flags/n_flag.md3" );

        cgs.media.UT_Radar                = trap_R_RegisterShaderNoMip ( "gfx/misc/Radar.tga" );
    }

    cgs.media.machinegunBrassModel  = trap_R_RegisterModel( "models/weapons2/shells/m_shell.md3" );
    cgs.media.shotgunBrassModel     = trap_R_RegisterModel( "models/weapons2/shells/s_shell.md3" );

    cgs.media.gibAbdomen            = trap_R_RegisterModel( "models/gibs/abdomen.md3" );
    cgs.media.gibArm                = trap_R_RegisterModel( "models/gibs/arm.md3" );
    cgs.media.gibChest              = trap_R_RegisterModel( "models/gibs/chest.md3" );
    cgs.media.gibFist               = trap_R_RegisterModel( "models/gibs/fist.md3" );
    cgs.media.gibFoot               = trap_R_RegisterModel( "models/gibs/foot.md3" );
    cgs.media.gibForearm            = trap_R_RegisterModel( "models/gibs/forearm.md3" );
    cgs.media.gibIntestine          = trap_R_RegisterModel( "models/gibs/intestine.md3" );
    cgs.media.gibLeg                = trap_R_RegisterModel( "models/gibs/leg.md3" );
    cgs.media.gibSkull              = trap_R_RegisterModel( "models/gibs/skull.md3" );
    cgs.media.gibBrain              = trap_R_RegisterModel( "models/gibs/brain.md3" );

    cgs.media.mrSentryBase          = trap_R_RegisterModel( "models/mrsentry/mrsentrybase.md3" );
    cgs.media.mrSentryBarrel        = trap_R_RegisterModel( "models/mrsentry/mrsentrybarrel.md3" );
    cgs.media.mrSentryRed           = trap_R_RegisterShader ( "models/mrsentry/mrsentry_red" );
    cgs.media.mrSentryBlue          = trap_R_RegisterShader ( "models/mrsentry/mrsentry_blue" );
    cgs.media.mrSentryFire          = trap_S_RegisterSound  ( "models/mrsentry/mrsentryfire.wav", qfalse );
    cgs.media.mrSentrySpinUp        = trap_S_RegisterSound  ( "models/mrsentry/mrsentryspinup.wav", qfalse );
    cgs.media.mrSentrySpinDown      = trap_S_RegisterSound  ( "models/mrsentry/mrsentryspindown.wav", qfalse );

    cgs.media.smoke2            = trap_R_RegisterModel( "models/weapons2/shells/s_shell.md3" );

    cgs.media.bloodExplosionShader  = trap_R_RegisterShader( "bloodParticle" );
    cgs.media.bloodExplosionShader1 = trap_R_RegisterShader( "bloodParticle1" );

    cgs.media.medicshader           = trap_R_RegisterShader( "mediccall" );
    cgs.media.plantingShader        = trap_R_RegisterShader("planting");
    cgs.media.defusingShader        = trap_R_RegisterShader("defusing");

    cgs.media.ut_BombSiteMarker     = trap_R_RegisterShader( "mini_bomb" );
    cgs.media.ut_BombMiniMapMarker  = trap_R_RegisterShader( "minimap_bomb" );

    cgs.media.sphereLoResModel      = trap_R_RegisterModel("models/misc/sphere_low.md3");
    cgs.media.sphereHiResModel      = trap_R_RegisterModel("models/misc/sphere_high.md3");

    memset( cg_items, 0, sizeof(cg_items));
    memset( cg_weapons, 0, sizeof(cg_weapons));

    for (i = 1 ; i < bg_numItems ; i++)
    {
        CG_LoadingItem( i );
        CG_RegisterItemVisuals( i );
    }

    CG_RegisterItemVisuals ( UT_ITEM_HELMET );
    CG_RegisterItemVisuals ( UT_ITEM_NVG );

    // wall marks
    cgs.media.bulletMarkShader   = trap_R_RegisterShader( "gfx/damage/mark" );

    cgs.media.shadowMarkShader   = trap_R_RegisterShader( "markShadow" );

    cgs.media.burnMarkShader     = trap_R_RegisterShader( "gfx/damage/burn_med_mrk" );
    cgs.media.holeMarkShader     = trap_R_RegisterShader( "gfx/damage/hole" );
    cgs.media.wakeMarkShader     = trap_R_RegisterShader( "wake" );
    cgs.media.bloodMarkShader[0] = trap_R_RegisterShader( "bloodMark" );
    cgs.media.bloodMarkShader[1] = trap_R_RegisterShader( "bloodMark2" );
    cgs.media.bloodMarkShader[2] = trap_R_RegisterShader( "bloodMark3" );
    cgs.media.bloodMarkShader[3] = trap_R_RegisterShader( "bloodMark4" );
    cgs.media.dishFlashModel     = trap_R_RegisterModel("models/weaphits/boom01.md3");

    // register the inline models
    cgs.numInlineModels = trap_CM_NumInlineModels();

    for (i = 1 ; i < cgs.numInlineModels ; i++)
    {
        char    name[10];
        vec3_t  mins, maxs;
        int     j;

        Com_sprintf( name, sizeof(name), "*%i", i );
        cgs.inlineDrawModel[i] = trap_R_RegisterModel( name );
        trap_R_ModelBounds( cgs.inlineDrawModel[i], mins, maxs );

        for (j = 0 ; j < 3 ; j++)
        {
           cgs.inlineModelMidpoints[i][j] = mins[j] + 0.5 * (maxs[j] - mins[j]);
        }
    }

    // register all the server specified models
    for (i = 1 ; i < MAX_MODELS ; i++)
    {
        const char  *modelName;

        modelName = CG_ConfigString( CS_MODELS + i );

        if (!modelName[0])
        {
           break;
        }
        cgs.gameModels[i] = trap_R_RegisterModel( modelName );
    }

    // Urban Terror graphics
    cgs.media.nvgStatic  = trap_R_RegisterShader( "nvgStatic" );
    cgs.media.nvgBrightA = trap_R_RegisterShader( "nvgBrightA" );
    cgs.media.nvgBrightB = trap_R_RegisterShader( "nvgBrightB" );

    cgs.media.nvgScope   = trap_R_RegisterShader( "nvgScope2" );


    // dokta8 Register UT Breakable Glass models
    cgs.media.glassFragments[0] = trap_R_RegisterModel( "models/breakable/glass/glass1.md3" );
    cgs.media.glassFragments[1] = trap_R_RegisterModel( "models/breakable/glass/glass2.md3" );
    cgs.media.glassFragments[2] = trap_R_RegisterModel( "models/breakable/glass/glass3.md3" );

    cgs.media.metalFragments[0] = trap_R_RegisterModel("models/breakable/metal/metal1.md3");
    cgs.media.metalFragments[1] = trap_R_RegisterModel("models/breakable/metal/metal2.md3");
    cgs.media.metalFragments[2] = trap_R_RegisterModel("models/breakable/metal/metal3.md3");

    cgs.media.stoneFragments[0] = trap_R_RegisterModel("models/breakable/stone/stone1.md3");
    cgs.media.stoneFragments[1] = trap_R_RegisterModel("models/breakable/stone/stone2.md3");
    cgs.media.stoneFragments[2] = trap_R_RegisterModel("models/breakable/stone/stone3.md3");

    // Fragments
    cgs.media.UT_fragmentShader             = trap_R_RegisterShader ( "gfx/misc/fragment");

    cgs.media.UT_Backpack                   = trap_R_RegisterModel ( "models/players/gear/backpack.md3" );
    cgs.media.UT_BackpackNormal             = trap_R_RegisterSkin ( "models/players/gear/backpack_black.skin" );
    cgs.media.UT_BackpackExplosives         = trap_R_RegisterSkin ( "models/players/gear/backpack_explosives.skin" );
    cgs.media.UT_BackpackMedic              = trap_R_RegisterSkin ( "models/players/gear/backpack_medic.skin" );

    cgs.media.UT_bloodPool[0]               = trap_R_RegisterShader ( "bloodPool1" );
    cgs.media.UT_bloodPool[1]               = trap_R_RegisterShader ( "bloodPool2" );
    cgs.media.UT_bloodPool[2]               = trap_R_RegisterShader ( "bloodPool3" );

    cgs.media.UT_ParticleShader             = trap_R_RegisterShader ( "particle1" );

    cgs.media.UT_chunkShader                = trap_R_RegisterShader ( "gfx/particles/chunk" );
    cgs.media.UT_grassShader                = trap_R_RegisterShader ( "gfx/particles/grass" );

    cgs.media.UT_laserShaderRed             = trap_R_RegisterShaderNoMip ( "gfx/laser/red" );
    cgs.media.UT_laserShaderGreen           = trap_R_RegisterShaderNoMip ( "gfx/laser/green" );

    cgs.media.UT_timerShader                = trap_R_RegisterShaderNoMip("icons/timer.tga");

    cgs.media.UT_waveShader[UT_SKIN_GREEN]  = trap_R_RegisterShaderNoMip("icons/wave_green.tga");
    cgs.media.UT_waveShader[UT_SKIN_BLUE]   = trap_R_RegisterShaderNoMip("icons/wave_blue.tga");
    cgs.media.UT_waveShader[UT_SKIN_RED]    = trap_R_RegisterShaderNoMip("icons/wave_red.tga");
    cgs.media.UT_waveShader[UT_SKIN_PURPLE] = trap_R_RegisterShaderNoMip("icons/wave_purple.tga");
    cgs.media.UT_waveShader[UT_SKIN_ORANGE] = trap_R_RegisterShaderNoMip("icons/wave_orange.tga");
    cgs.media.UT_waveShader[UT_SKIN_OLIVE]  = trap_R_RegisterShaderNoMip("icons/wave_olive.tga");
    cgs.media.UT_waveShader[UT_SKIN_WHITE]  = trap_R_RegisterShaderNoMip("icons/wave_white.tga");
    cgs.media.UT_waveShader[UT_SKIN_BLACK]  = trap_R_RegisterShaderNoMip("icons/wave_black.tga");
    cgs.media.UT_waveShader[UT_SKIN_DESERT] = trap_R_RegisterShaderNoMip("icons/wave_desert.tga");
    cgs.media.UT_waveShader[UT_SKIN_COWBOY] = trap_R_RegisterShaderNoMip("icons/wave_cowboy.tga");
    cgs.media.UT_waveShader[UT_SKIN_CAVALRY] = trap_R_RegisterShaderNoMip("icons/wave_cavalry.tga");
    cgs.media.UT_waveShader[UT_SKIN_DROOGS] = trap_R_RegisterShaderNoMip("icons/wave_droogs.tga");

#define SET_POTATO(a,b,s) cgs.media.UT_potatoShaders[a][b] = cgs.media.UT_potatoShaders[b][a] = trap_R_RegisterShaderNoMip(s);

	SET_POTATO(UT_SKIN_GREEN,UT_SKIN_BLUE,"icons/potato_blue_green.tga");
	SET_POTATO(UT_SKIN_GREEN,UT_SKIN_RED,"icons/potato_green_red.tga");
	SET_POTATO(UT_SKIN_GREEN,UT_SKIN_PURPLE,"icons/potato_purple_green.tga");
	SET_POTATO(UT_SKIN_GREEN,UT_SKIN_ORANGE,"icons/potato_orange_green.tga");
	SET_POTATO(UT_SKIN_GREEN,UT_SKIN_OLIVE,"icons/potato_olive_green.tga");
	SET_POTATO(UT_SKIN_GREEN,UT_SKIN_WHITE,"icons/potato_green_white.tga");
	SET_POTATO(UT_SKIN_GREEN,UT_SKIN_BLACK,"icons/potato_green_black.tga");
	SET_POTATO(UT_SKIN_GREEN,UT_SKIN_DESERT,"icons/potato_green_desert.tga");
	SET_POTATO(UT_SKIN_GREEN,UT_SKIN_COWBOY,"icons/potato_green_cowboy.tga");
	SET_POTATO(UT_SKIN_GREEN,UT_SKIN_CAVALRY,"icons/potato_green_cavalry.tga");
	SET_POTATO(UT_SKIN_GREEN,UT_SKIN_DROOGS,"icons/potato_green_droogs.tga");

	SET_POTATO(UT_SKIN_BLUE,UT_SKIN_RED,"icons/potato.tga");
	SET_POTATO(UT_SKIN_BLUE,UT_SKIN_PURPLE,"icons/potato_blue_purple.tga");
	SET_POTATO(UT_SKIN_BLUE,UT_SKIN_ORANGE,"icons/potato_blue_orange.tga");
	SET_POTATO(UT_SKIN_BLUE,UT_SKIN_OLIVE,"icons/potato_blue_olive.tga");
	SET_POTATO(UT_SKIN_BLUE,UT_SKIN_WHITE,"icons/potato_blue_white.tga");
	SET_POTATO(UT_SKIN_BLUE,UT_SKIN_BLACK,"icons/potato_blue_black.tga");
	SET_POTATO(UT_SKIN_BLUE,UT_SKIN_DESERT,"icons/potato_blue_desert.tga");
	SET_POTATO(UT_SKIN_BLUE,UT_SKIN_COWBOY,"icons/potato_blue_cowboy.tga");
	SET_POTATO(UT_SKIN_BLUE,UT_SKIN_CAVALRY,"icons/potato_blue_cavalry.tga");
	SET_POTATO(UT_SKIN_BLUE,UT_SKIN_DROOGS,"icons/potato_blue_droogs.tga");

	SET_POTATO(UT_SKIN_RED,UT_SKIN_PURPLE,"icons/potato_purple_red.tga");
	SET_POTATO(UT_SKIN_RED,UT_SKIN_ORANGE,"icons/potato_orange_red.tga");
	SET_POTATO(UT_SKIN_RED,UT_SKIN_OLIVE,"icons/potato_olive_red.tga");
	SET_POTATO(UT_SKIN_RED,UT_SKIN_WHITE,"icons/potato_red_white.tga");
	SET_POTATO(UT_SKIN_RED,UT_SKIN_BLACK,"icons/potato_red_black.tga");
	SET_POTATO(UT_SKIN_RED,UT_SKIN_DESERT,"icons/potato_red_desert.tga");
	SET_POTATO(UT_SKIN_RED,UT_SKIN_COWBOY,"icons/potato_red_cowboy.tga");
	SET_POTATO(UT_SKIN_RED,UT_SKIN_CAVALRY,"icons/potato_red_cavalry.tga");
	SET_POTATO(UT_SKIN_RED,UT_SKIN_DROOGS,"icons/potato_red_droogs.tga");

	SET_POTATO(UT_SKIN_PURPLE,UT_SKIN_ORANGE,"icons/potato_purple_orange.tga");
	SET_POTATO(UT_SKIN_PURPLE,UT_SKIN_OLIVE,"icons/potato_olive_purple.tga");
	SET_POTATO(UT_SKIN_PURPLE,UT_SKIN_WHITE,"icons/potato_purple_white.tga");
	SET_POTATO(UT_SKIN_PURPLE,UT_SKIN_BLACK,"icons/potato_purple_black.tga");
	SET_POTATO(UT_SKIN_PURPLE,UT_SKIN_DESERT,"icons/potato_purple_desert.tga");
	SET_POTATO(UT_SKIN_PURPLE,UT_SKIN_COWBOY,"icons/potato_purple_cowboy.tga");
	SET_POTATO(UT_SKIN_PURPLE,UT_SKIN_CAVALRY,"icons/potato_purple_cavalry.tga");
	SET_POTATO(UT_SKIN_PURPLE,UT_SKIN_DROOGS,"icons/potato_purple_droogs.tga");

	SET_POTATO(UT_SKIN_ORANGE,UT_SKIN_OLIVE,"icons/potato_olive_orange.tga");
	SET_POTATO(UT_SKIN_ORANGE,UT_SKIN_WHITE,"icons/potato_orange_white.tga");
	SET_POTATO(UT_SKIN_ORANGE,UT_SKIN_BLACK,"icons/potato_orange_black.tga");
	SET_POTATO(UT_SKIN_ORANGE,UT_SKIN_DESERT,"icons/potato_orange_desert.tga");
	SET_POTATO(UT_SKIN_ORANGE,UT_SKIN_COWBOY,"icons/potato_orange_cowboy.tga");
	SET_POTATO(UT_SKIN_ORANGE,UT_SKIN_CAVALRY,"icons/potato_orange_cavalry.tga");
	SET_POTATO(UT_SKIN_ORANGE,UT_SKIN_DROOGS,"icons/potato_orange_droogs.tga");

	SET_POTATO(UT_SKIN_OLIVE,UT_SKIN_WHITE,"icons/potato_olive_white.tga");
	SET_POTATO(UT_SKIN_OLIVE,UT_SKIN_BLACK,"icons/potato_olive_black.tga");
	SET_POTATO(UT_SKIN_OLIVE,UT_SKIN_DESERT,"icons/potato_olive_desert.tga");
	SET_POTATO(UT_SKIN_OLIVE,UT_SKIN_COWBOY,"icons/potato_olive_cowboy.tga");
	SET_POTATO(UT_SKIN_OLIVE,UT_SKIN_CAVALRY,"icons/potato_olive_cavalry.tga");
	SET_POTATO(UT_SKIN_OLIVE,UT_SKIN_DROOGS,"icons/potato_olive_droogs.tga");

	SET_POTATO(UT_SKIN_WHITE,UT_SKIN_BLACK,"icons/potato_black_white.tga");
	SET_POTATO(UT_SKIN_WHITE,UT_SKIN_DESERT,"icons/potato_white_desert.tga");
	SET_POTATO(UT_SKIN_WHITE,UT_SKIN_COWBOY,"icons/potato_white_cowboy.tga");
	SET_POTATO(UT_SKIN_WHITE,UT_SKIN_CAVALRY,"icons/potato_white_cavalry.tga");
	SET_POTATO(UT_SKIN_WHITE,UT_SKIN_DROOGS,"icons/potato_white_droogs.tga");

	SET_POTATO(UT_SKIN_BLACK,UT_SKIN_DESERT,"icons/potato_black_desert.tga");
	SET_POTATO(UT_SKIN_BLACK,UT_SKIN_COWBOY,"icons/potato_black_cowboy.tga");
	SET_POTATO(UT_SKIN_BLACK,UT_SKIN_CAVALRY,"icons/potato_black_cavalry.tga");
	SET_POTATO(UT_SKIN_BLACK,UT_SKIN_DROOGS,"icons/potato_black_droogs.tga");

	SET_POTATO(UT_SKIN_DESERT,UT_SKIN_COWBOY,"icons/potato_desert_cowboy.tga");
	SET_POTATO(UT_SKIN_DESERT,UT_SKIN_CAVALRY,"icons/potato_desert_cavalry.tga");
	SET_POTATO(UT_SKIN_DESERT,UT_SKIN_DROOGS,"icons/potato_desert_droogs.tga");

	SET_POTATO(UT_SKIN_COWBOY,UT_SKIN_CAVALRY,"icons/potato_cowboy_cavalry.tga");
	SET_POTATO(UT_SKIN_COWBOY,UT_SKIN_DROOGS,"icons/potato_cowboy_droogs.tga");

  SET_POTATO(UT_SKIN_CAVALRY,UT_SKIN_DROOGS,"icons/potato_cavalry_droogs.tga");
#undef SET_POTATO

    //@Fenix - jump mode timer icons
    cgs.media.UT_jumpRunningTimer           = trap_R_RegisterShaderNoMip("icons/running_time.tga");
    cgs.media.UT_jumpBestTimer              = trap_R_RegisterShaderNoMip("icons/best_time.tga");

    // Load in the damage indicators
    memset ( &cgs.media.UT_DamageIcon, 0, sizeof(cgs.media.UT_DamageIcon));

    //Fenix: damage icons for new hitloc [added for 4.2.013]
    cgs.media.UT_DamageIcon[HL_HEAD]      = trap_R_RegisterShaderNoMip("gfx/hud/damage/head.tga");
    cgs.media.UT_DamageIcon[HL_HELMET]    = cgs.media.UT_DamageIcon[HL_HEAD];
    cgs.media.UT_DamageIcon[HL_TORSO]     = trap_R_RegisterShaderNoMip("gfx/hud/damage/all_chest.tga");
    cgs.media.UT_DamageIcon[HL_VEST]      = cgs.media.UT_DamageIcon[HL_TORSO];
    cgs.media.UT_DamageIcon[HL_ARML]      = trap_R_RegisterShaderNoMip("gfx/hud/damage/right_arm.tga");
    cgs.media.UT_DamageIcon[HL_ARMR]      = trap_R_RegisterShaderNoMip("gfx/hud/damage/left_arm.tga");
    cgs.media.UT_DamageIcon[HL_GROIN]     = trap_R_RegisterShaderNoMip("gfx/hud/damage/waist.tga");
    cgs.media.UT_DamageIcon[HL_BUTT]      = cgs.media.UT_DamageIcon[HL_GROIN];
    cgs.media.UT_DamageIcon[HL_LEGUL]     = trap_R_RegisterShaderNoMip("gfx/hud/damage/right_thigh.tga");
    cgs.media.UT_DamageIcon[HL_LEGUR]     = trap_R_RegisterShaderNoMip("gfx/hud/damage/left_thigh.tga");
    cgs.media.UT_DamageIcon[HL_LEGLL]     = trap_R_RegisterShaderNoMip("gfx/hud/damage/right_calf.tga");
    cgs.media.UT_DamageIcon[HL_LEGLR]     = trap_R_RegisterShaderNoMip("gfx/hud/damage/left_calf.tga");
    cgs.media.UT_DamageIcon[HL_FOOTL]     = trap_R_RegisterShaderNoMip("gfx/hud/damage/right_foot.tga");
    cgs.media.UT_DamageIcon[HL_FOOTR]     = trap_R_RegisterShaderNoMip("gfx/hud/damage/left_foot.tga");

    cgs.media.UT_PlayerStatusOutline        = trap_R_RegisterShaderNoMip ( "gfx/hud/outline.tga" );
    cgs.media.UT_PlayerStatusStamina[0]     = trap_R_RegisterShaderNoMip ( "gfx/hud/stamina1.tga" );
    cgs.media.UT_PlayerStatusStamina[1]     = trap_R_RegisterShaderNoMip ( "gfx/hud/stamina2.tga" );
    cgs.media.UT_PlayerStatusStamina[2]     = trap_R_RegisterShaderNoMip ( "gfx/hud/stamina3.tga" );
    cgs.media.UT_PlayerStatusStamina[3]     = trap_R_RegisterShaderNoMip ( "gfx/hud/stamina4.tga" );
    cgs.media.UT_PlayerStatusStamina[4]     = trap_R_RegisterShaderNoMip ( "gfx/hud/stamina5.tga" );
    cgs.media.UT_PlayerStatusStamina[5]     = trap_R_RegisterShaderNoMip ( "gfx/hud/stamina6.tga" );
    cgs.media.UT_PlayerStatusStamina[6]     = trap_R_RegisterShaderNoMip ( "gfx/hud/stamina7.tga" );
    cgs.media.UT_PlayerStatusStamina[7]     = trap_R_RegisterShaderNoMip ( "gfx/hud/stamina8.tga" );
    cgs.media.UT_PlayerStatusStamina[8]     = trap_R_RegisterShaderNoMip ( "gfx/hud/stamina9.tga" );

    //27 added this in to preload all walk,impact and bullethole sfx
    CG_LoadingString( "space monsters" );
    preloadAllSurfaceEffects();
    preloadAllClientData();
//{
//int a0 = trap_R_RegisterShader( "gfx/crosshairs/scopes/scopering" );
//int a1 = trap_R_RegisterShader( "viewsmokepuff" );
//int a2 = trap_R_RegisterShader( "grenadeSmokePuff" );
//trap_Print(va("SHADERS1: %d %d %d\n",a0,a1,a2));
//}

}

/*
===================
CG_RegisterClients
===================
*/
static void CG_RegisterClients( void )
{
    int  i;

    CG_LoadingClient(cg.clientNum);
    CG_NewClientInfo(cg.clientNum);

    for (i = 0 ; i < MAX_CLIENTS ; i++)
    {
        const char  *clientInfo;

        if (cg.clientNum == i)
        {
            continue;
        }

        clientInfo = CG_ConfigString( CS_PLAYERS + i );

        if (!clientInfo[0])
        {
            continue;
        }
        CG_LoadingClient( i );
        CG_NewClientInfo( i );
    }
}

//===========================================================================

/*
=================
CG_ConfigString
=================
*/
const char *CG_ConfigString( int index )
{
    if ((index < 0) || (index >= MAX_CONFIGSTRINGS))
    {
        CG_Error( "CG_ConfigString: bad index: %i", index );
    }
    return cgs.gameState.stringData + cgs.gameState.stringOffsets[index];
}

//==================================================================

/*
======================
CG_StartMusic

======================
*/
void CG_StartMusic( void )
{
    // start the background music
/*  s = (char *)CG_ConfigString( CS_MUSIC );
    Q_strncpyz( parm1, COM_Parse( &s ), sizeof( parm1 ) );
    Q_strncpyz( parm2, COM_Parse( &s ), sizeof( parm2 ) );

    trap_S_StartBackgroundTrack( parm1, parm2 ); */
}
#ifdef MISSIONPACK
/**
 * $(function)
 */
char *CG_GetMenuBuffer(const char *filename)
{
    int       len;
    fileHandle_t  f;
    static char   buf[MAX_MENUFILE];

    len = trap_FS_FOpenFile( filename, &f, FS_READ );

    if (!f)
    {
        trap_Print( va( S_COLOR_RED "menu file not found: %s, using default\n", filename ));
        return NULL;
    }

    if (len >= MAX_MENUFILE)
    {
        trap_Print( va( S_COLOR_RED "menu file too large: %s is %i, max allowed is %i", filename, len, MAX_MENUFILE ));
        trap_FS_FCloseFile( f );
        return NULL;
    }

    trap_FS_Read( buf, len, f );
    buf[len] = 0;
    trap_FS_FCloseFile( f );

    return buf;
}

//
// ==============================
// new hud stuff ( mission pack )
// ==============================
//
qboolean CG_Asset_Parse(int handle)
{
    pc_token_t  token;
    const char  *tempStr;

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
            cgDC.registerFont(tempStr, pointSize, &cgDC.Assets.textFont);
            continue;
        }

        // smallFont
        if (Q_stricmp(token.string, "smallFont") == 0)
        {
            int  pointSize;

            if (!PC_String_Parse(handle, &tempStr) || !PC_Int_Parse(handle, &pointSize))
            {
                return qfalse;
            }
            cgDC.registerFont(tempStr, pointSize, &cgDC.Assets.smallFont);
            continue;
        }

        // font
        if (Q_stricmp(token.string, "bigfont") == 0)
        {
            int  pointSize;

            if (!PC_String_Parse(handle, &tempStr) || !PC_Int_Parse(handle, &pointSize))
            {
                return qfalse;
            }
            cgDC.registerFont(tempStr, pointSize, &cgDC.Assets.bigFont);
            continue;
        }

        // gradientbar
        if (Q_stricmp(token.string, "gradientbar") == 0)
        {
            if (!PC_String_Parse(handle, &tempStr))
            {
                return qfalse;
            }
            cgDC.Assets.gradientBar = trap_R_RegisterShaderNoMip(tempStr);
            continue;
        }

        // enterMenuSound
        if (Q_stricmp(token.string, "menuEnterSound") == 0)
        {
            if (!PC_String_Parse(handle, &tempStr))
            {
                return qfalse;
            }
            cgDC.Assets.menuEnterSound = trap_S_RegisterSound( tempStr, qfalse );
            continue;
        }

        // exitMenuSound
        if (Q_stricmp(token.string, "menuExitSound") == 0)
        {
            if (!PC_String_Parse(handle, &tempStr))
            {
                return qfalse;
            }
            cgDC.Assets.menuExitSound = trap_S_RegisterSound( tempStr, qfalse );
            continue;
        }

        // itemFocusSound
        if (Q_stricmp(token.string, "itemFocusSound") == 0)
        {
            if (!PC_String_Parse(handle, &tempStr))
            {
                return qfalse;
            }
            cgDC.Assets.itemFocusSound = trap_S_RegisterSound( tempStr, qfalse );
            continue;
        }

        // menuBuzzSound
        if (Q_stricmp(token.string, "menuBuzzSound") == 0)
        {
            if (!PC_String_Parse(handle, &tempStr))
            {
                return qfalse;
            }
            cgDC.Assets.menuBuzzSound = trap_S_RegisterSound( tempStr, qfalse );
            continue;
        }

        if (Q_stricmp(token.string, "cursor") == 0)
        {
            if (!PC_String_Parse(handle, &cgDC.Assets.cursorStr))
            {
                return qfalse;
            }
            cgDC.Assets.cursor = trap_R_RegisterShaderNoMip( cgDC.Assets.cursorStr);
            continue;
        }

        if (Q_stricmp(token.string, "fadeClamp") == 0)
        {
            if (!PC_Float_Parse(handle, &cgDC.Assets.fadeClamp))
            {
                return qfalse;
            }
            continue;
        }

        if (Q_stricmp(token.string, "fadeCycle") == 0)
        {
            if (!PC_Int_Parse(handle, &cgDC.Assets.fadeCycle))
            {
                return qfalse;
            }
            continue;
        }

        if (Q_stricmp(token.string, "fadeAmount") == 0)
        {
            if (!PC_Float_Parse(handle, &cgDC.Assets.fadeAmount))
            {
                return qfalse;
            }
            continue;
        }

        if (Q_stricmp(token.string, "shadowX") == 0)
        {
            if (!PC_Float_Parse(handle, &cgDC.Assets.shadowX))
            {
                return qfalse;
            }
            continue;
        }

        if (Q_stricmp(token.string, "shadowY") == 0)
        {
            if (!PC_Float_Parse(handle, &cgDC.Assets.shadowY))
            {
                return qfalse;
            }
            continue;
        }

        if (Q_stricmp(token.string, "shadowColor") == 0)
        {
            if (!PC_Color_Parse(handle, &cgDC.Assets.shadowColor))
            {
                return qfalse;
            }
            cgDC.Assets.shadowFadeClamp = cgDC.Assets.shadowColor[3];
            continue;
        }
    }

    return qtrue;
}

/**
 * $(function)
 */
void CG_ParseMenu(const char *menuFile)
{
    pc_token_t  token;
    int     handle;

    handle = trap_PC_LoadSource(menuFile);

    if (!handle)
    {
        handle = trap_PC_LoadSource("ui/testhud.menu");
    }

    if (!handle)
    {
        return;
    }

    while (1)
    {
        if (!trap_PC_ReadToken( handle, &token ))
        {
            break;
        }

        //if ( Q_stricmp( token, "{" ) ) {
        //    Com_Printf( "Missing { in menu file\n" );
        //  break;
        //}

        //if ( menuCount == MAX_MENUS ) {
        //  Com_Printf( "Too many menus!\n" );
        //  break;
        //}

        if (token.string[0] == '}')
        {
            break;
        }

        if (Q_stricmp(token.string, "assetGlobalDef") == 0)
        {
            if (CG_Asset_Parse(handle))
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
 * $(function)
 */
qboolean CG_Load_Menu(char * *p)
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

        CG_ParseMenu(token);
    }
    return qfalse;
}

/**
 * $(function)
 */
void CG_LoadMenus(const char *menuFile)
{
    char          *token;
    char          *p;
    int       len, start;
    fileHandle_t  f;
    static char   buf[MAX_MENUDEFFILE];

    start = trap_Milliseconds();

    len   = trap_FS_FOpenFile( menuFile, &f, FS_READ );

    if (!f)
    {
        trap_Error( va( S_COLOR_YELLOW "menu file not found: %s, using default\n", menuFile ));
        len = trap_FS_FOpenFile( "ui/hud.txt", &f, FS_READ );

        if (!f)
        {
            trap_Error( va( S_COLOR_RED "default menu file not found: ui/hud.txt, unable to continue!\n", menuFile ));
        }
    }

    if (len >= MAX_MENUDEFFILE)
    {
        trap_Error( va( S_COLOR_RED "menu file too large: %s is %i, max allowed is %i", menuFile, len, MAX_MENUDEFFILE ));
        trap_FS_FCloseFile( f );
        return;
    }

    trap_FS_Read( buf, len, f );
    buf[len] = 0;
    trap_FS_FCloseFile( f );

    COM_Compress(buf);

    Menu_Reset();

    p = buf;

    while (1)
    {
        token = COM_ParseExt( &p, qtrue );

        if(!token || (token[0] == 0) || (token[0] == '}'))
        {
            break;
        }

        //if ( Q_stricmp( token, "{" ) ) {
        //  Com_Printf( "Missing { in menu file\n" );
        //  break;
        //}

        //if ( menuCount == MAX_MENUS ) {
        //  Com_Printf( "Too many menus!\n" );
        //  break;
        //}

        if (Q_stricmp( token, "}" ) == 0)
        {
            break;
        }

        if (Q_stricmp(token, "loadmenu") == 0)
        {
            if (CG_Load_Menu(&p))
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
}

/**
 * $(function)
 */
static qboolean CG_OwnerDrawHandleKey(int ownerDraw, int flags, float *special, int key)
{
    return qfalse;
}

/**
 * $(function)
 */
static int CG_FeederCount(float feederID)
{
    int  i, count;

    count = 0;

    if (feederID == FEEDER_REDTEAM_LIST)
    {
        for (i = 0; i < cg.numScores; i++)
        {
            if (cg.scores[i].team == TEAM_RED)
            {
                count++;
            }
        }
    }
    else if (feederID == FEEDER_BLUETEAM_LIST)
    {
        for (i = 0; i < cg.numScores; i++)
        {
            if (cg.scores[i].team == TEAM_BLUE)
            {
                count++;
            }
        }
    }
    else if (feederID == FEEDER_SCOREBOARD)
    {
        return cg.numScores;
    }
    return count;
}

/**
 * $(function)
 */
void CG_SetScoreSelection(void *p)
{
    menuDef_t      *menu = (menuDef_t *)p;
    playerState_t  *ps   = &cg.snap->ps;
    int        i, red, blue;

    red = blue = 0;

    for (i = 0; i < cg.numScores; i++)
    {
        if (cg.scores[i].team == TEAM_RED)
        {
            red++;
        }
        else if (cg.scores[i].team == TEAM_BLUE)
        {
            blue++;
        }

        if (ps->clientNum == cg.scores[i].client)
        {
            cg.selectedScore = i;
        }
    }

    if (menu == NULL)
    {
        // just interested in setting the selected score
        return;
    }

    if ((cgs.gametype >= GT_TEAM) && (cgs.gametype != GT_JUMP))
    {
        int  feeder = FEEDER_REDTEAM_LIST;
        i = red;

        if (cg.scores[cg.selectedScore].team == TEAM_BLUE)
        {
            feeder = FEEDER_BLUETEAM_LIST;
            i      = blue;
        }
        Menu_SetFeederSelection(menu, feeder, i, NULL);
    }
    else
    {
        Menu_SetFeederSelection(menu, FEEDER_SCOREBOARD, cg.selectedScore, NULL);
    }
}

// FIXME: might need to cache this info
static clientInfo_t *CG_InfoFromScoreIndex(int index, int team, int *scoreIndex)
{
    int  i, count;

    if ((cgs.gametype >= GT_TEAM) && (cgs.gametype != GT_JUMP))
    {
        count = 0;

        for (i = 0; i < cg.numScores; i++)
        {
            if (cg.scores[i].team == team)
            {
                if (count == index)
                {
                    *scoreIndex = i;
                    return &cgs.clientinfo[cg.scores[i].client];
                }
                count++;
            }
        }
    }
    *scoreIndex = index;
    return &cgs.clientinfo[cg.scores[index].client];
}

/**
 * $(function)
 */
static const char *CG_FeederItemText(float feederID, int index, int column, qhandle_t *handle)
{
    gitem_t       *item;
    int       scoreIndex = 0;
    clientInfo_t  *info  = NULL;
    int       team   = -1;
    score_t       *sp    = NULL;

    *handle = -1;

    if (feederID == FEEDER_REDTEAM_LIST)
    {
        team = TEAM_RED;
    }
    else if (feederID == FEEDER_BLUETEAM_LIST)
    {
        team = TEAM_BLUE;
    }

    info = CG_InfoFromScoreIndex(index, team, &scoreIndex);
    sp   = &cg.scores[scoreIndex];

    if (info && info->infoValid)
    {
        switch (column)
        {
            case 0:

                if (info->powerups & (1 << PW_NEUTRALFLAG))
                {
                    item    = BG_FindItemForPowerup( PW_NEUTRALFLAG );
                    *handle = cg_items[ITEM_INDEX(item)].icon;
                }
                else if (info->powerups & (1 << PW_REDFLAG))
                {
                    item    = BG_FindItemForPowerup( PW_REDFLAG );
                    *handle = cg_items[ITEM_INDEX(item)].icon;
                }
                else if (info->powerups & (1 << PW_BLUEFLAG))
                {
                    item    = BG_FindItemForPowerup( PW_BLUEFLAG );
                    *handle = cg_items[ITEM_INDEX(item)].icon;
                }
                else
                {
                    if ((info->botSkill > 0) && (info->botSkill <= 5))
                    {
                        *handle = cgs.media.botSkillShaders[info->botSkill - 1];
                    }
                    else if (info->handicap < 100)
                    {
                        return va("%i", info->handicap );
                    }
                }
                break;

            case 1:

                if (team == -1)
                {
                    return "";
                }
                else
                {
                    *handle = CG_StatusHandle(info->teamTask);
                }
                break;

            case 2:

//              if ( cg.snap->ps.stats[ STAT_CLIENTS_READY ] & ( 1 << sp->client ) )
//        {
//                  return "Ready";
//              }
                if (team == -1)
                {
                    if (info->infoValid && (info->team == TEAM_SPECTATOR))
                    {
                        return "Spectator";
                    }
                    else
                    {
                        return info->ghost ? "DEAD" : "";
                    }
                }
                else
                {
                    if (info->teamLeader)
                    {
                        return "Leader";
                    }
                    else
                    {
                        return info->ghost ? "DEAD" : "";
                    }
                }

                break;

            case 3:
                return info->name;

                break;

            case 4:
                return va("%i", info->score);

                break;

            case 5:
                return va("%4i", sp->time);

                break;

            case 6:

                if (sp->ping == -1)
                {
                    return "connecting";
                }
                return va("%4i", sp->ping);

                break;
        }
    }

    return "";
}

/**
 * $(function)
 */
static qhandle_t CG_FeederItemImage(float feederID, int index)
{
    return 0;
}

/**
 * $(function)
 */
static void CG_FeederSelection(float feederID, int index)
{
    if ((cgs.gametype >= GT_TEAM) && (cgs.gametype != GT_JUMP))
    {
        int  i, count;
        int  team = (feederID == FEEDER_REDTEAM_LIST) ? TEAM_RED : TEAM_BLUE;
        count = 0;

        for (i = 0; i < cg.numScores; i++)
        {
            if (cg.scores[i].team == team)
            {
                if (index == count)
                {
                    cg.selectedScore = i;
                }
                count++;
            }
        }
    }
    else
    {
        cg.selectedScore = index;
    }
}
#endif
/**
 * $(function)
 */
static float CG_Cvar_Get(const char *cvar)
{
    char  buff[128];

    memset(buff, 0, sizeof(buff));
    trap_Cvar_VariableStringBuffer(cvar, buff, sizeof(buff));
    return atof(buff);
}

#ifdef MISSIONPACK
/**
 * $(function)
 */
void CG_Text_PaintWithCursor(float x, float y, float scale, vec4_t color, const char *text, int cursorPos, char cursor, int limit, int style)
{
    CG_Text_Paint(x, y, scale, color, text, 0, limit, style);
}

/**
 * $(function)
 */
static int CG_OwnerDrawWidth(int ownerDraw, float scale)
{
    switch (ownerDraw)
    {
        case CG_GAME_TYPE:
            return CG_Text_Width(CG_GameTypeString(), scale, 0);

        case CG_GAME_STATUS:
            return CG_Text_Width(CG_GetGameStatusText(), scale, 0);

            break;

        case CG_GAME_STATUS_OLD:
            return CG_Text_Width(CG_GetGameStatusTextOld(), scale, 0);

            break;

        case CG_KILLER:
            return CG_Text_Width(CG_GetKillerText(), scale, 0);

            break;

        case CG_RED_NAME:
            return CG_Text_Width("RED", scale, 0);

            break;

        case CG_BLUE_NAME:
            return CG_Text_Width("BLUE", scale, 0);

            break;
    }
    return 0;
}

/**
 * $(function)
 */
static int CG_PlayCinematic(const char *name, float x, float y, float w, float h)
{
    return trap_CIN_PlayCinematic(name, x, y, w, h, CIN_loop);
}

/**
 * $(function)
 */
static void CG_StopCinematic(int handle)
{
    trap_CIN_StopCinematic(handle);
}

/**
 * $(function)
 */
static void CG_DrawCinematic(int handle, float x, float y, float w, float h)
{
    trap_CIN_SetExtents(handle, x, y, w, h);
    trap_CIN_DrawCinematic(handle);
}

/**
 * $(function)
 */
static void CG_RunCinematicFrame(int handle)
{
    trap_CIN_RunCinematic(handle);
}

void CG_GetRectOffset(int offsetID, rectDef_t *rect );

/*
=================
CG_LoadHudMenu();

=================
*/
void trap_R_AddRefEntityToSceneFunc( const refEntity_t *R) { trap_R_AddRefEntityToSceneX(R); }
void CG_LoadHudMenu(void)
{
//  char buff[1024];
    const char    *hudSet;

    cgDC.registerShaderNoMip = &trap_R_RegisterShaderNoMip;
    cgDC.setColor            = &trap_R_SetColor;
    cgDC.drawHandlePic       = &CG_DrawPic;
    cgDC.drawStretchPic      = &trap_R_DrawStretchPic;
    cgDC.drawText            = &CG_Text_Paint;
    cgDC.textWidth           = &CG_Text_Width;
    cgDC.textHeight          = &CG_Text_Height;
    cgDC.registerModel       = &trap_R_RegisterModel;
    cgDC.modelBounds         = &trap_R_ModelBounds;
    cgDC.fillRect            = &CG_FillRect;
    cgDC.drawRect            = &CG_DrawRect;
    cgDC.drawSides           = &CG_DrawSides;
    cgDC.drawTopBottom       = &CG_DrawTopBottom;
    cgDC.clearScene          = &trap_R_ClearScene;
    cgDC.addRefEntityToScene = &trap_R_AddRefEntityToSceneFunc;
    cgDC.renderScene         = &trap_R_RenderScene;
    cgDC.registerFont        = &trap_R_RegisterFont;
    cgDC.ownerDrawItem       = &CG_OwnerDraw;
    cgDC.getValue            = &CG_GetValue;
    cgDC.ownerDrawVisible    = &CG_OwnerDrawVisible;
    cgDC.runScript           = &CG_RunMenuScript;
    cgDC.getTeamColor        = &CG_GetTeamColor;
    cgDC.setCVar             = trap_Cvar_Set;
    cgDC.updateCVars         = CG_UpdateCvars;
    cgDC.getCVarString       = trap_Cvar_VariableStringBuffer;
    cgDC.getCVarValue        = CG_Cvar_Get;
    cgDC.drawTextWithCursor  = &CG_Text_PaintWithCursor;
    //cgDC.setOverstrikeMode = &trap_Key_SetOverstrikeMode;
    //cgDC.getOverstrikeMode = &trap_Key_GetOverstrikeMode;
    cgDC.startLocalSound     = &trap_S_StartLocalSound;
    cgDC.ownerDrawHandleKey  = &CG_OwnerDrawHandleKey;
    cgDC.feederCount         = &CG_FeederCount;
    cgDC.feederItemImage     = &CG_FeederItemImage;
    cgDC.feederItemText      = &CG_FeederItemText;
    cgDC.feederSelection     = &CG_FeederSelection;
    //cgDC.setBinding        = &trap_Key_SetBinding;
    //cgDC.getBindingBuf     = &trap_Key_GetBindingBuf;
    //cgDC.keynumToStringBuf = &trap_Key_KeynumToStringBuf;
    //cgDC.executeText       = &trap_Cmd_ExecuteText;
    cgDC.Error               = &Com_Error;
    cgDC.Print               = &Com_Printf;
    cgDC.ownerDrawWidth      = &CG_OwnerDrawWidth;
    //cgDC.Pause             = &CG_Pause;
    cgDC.registerSound       = &trap_S_RegisterSound;
    cgDC.startBackgroundTrack= &trap_S_StartBackgroundTrack;
    cgDC.stopBackgroundTrack = &trap_S_StopBackgroundTrack;
    cgDC.playCinematic       = &CG_PlayCinematic;
    cgDC.stopCinematic       = &CG_StopCinematic;
    cgDC.drawCinematic       = &CG_DrawCinematic;
    cgDC.runCinematicFrame   = &CG_RunCinematicFrame;
    cgDC.getRectOffset       = &CG_GetRectOffset;

    Init_Display(&cgDC);

    Menu_Reset();

/*
    trap_Cvar_VariableStringBuffer("cg_hudFiles", buff, sizeof(buff));
    hudSet = buff;
    if (hudSet[0] == '\0') {
        hudSet = "ui/hud.txt";
    }
*/

    hudSet = "ui/hud.txt";
    CG_LoadMenus(hudSet);
    utCrosshairLoadAll ( );
}

/**
 * $(function)
 */
void CG_AssetCache(void)
{
    //if (Assets.textFont == NULL) {
    //  trap_R_RegisterFont("fonts/arial.ttf", 72, &Assets.textFont);
    //}
    //Assets.background = trap_R_RegisterShaderNoMip( ASSET_BACKGROUND );
    //Com_Printf("Menu Size: %i bytes\n", sizeof(Menus));
    cgDC.Assets.gradientBar               = trap_R_RegisterShaderNoMip( ASSET_GRADIENTBAR );
    cgDC.Assets.fxBasePic                 = trap_R_RegisterShaderNoMip( ART_FX_BASE );
    cgDC.Assets.fxPic[0]                  = trap_R_RegisterShaderNoMip( ART_FX_RED );
    cgDC.Assets.fxPic[1]                  = trap_R_RegisterShaderNoMip( ART_FX_YELLOW );
    cgDC.Assets.fxPic[2]                  = trap_R_RegisterShaderNoMip( ART_FX_GREEN );
    cgDC.Assets.fxPic[3]                  = trap_R_RegisterShaderNoMip( ART_FX_TEAL );
    cgDC.Assets.fxPic[4]                  = trap_R_RegisterShaderNoMip( ART_FX_BLUE );
    cgDC.Assets.fxPic[5]                  = trap_R_RegisterShaderNoMip( ART_FX_CYAN );
    cgDC.Assets.fxPic[6]                  = trap_R_RegisterShaderNoMip( ART_FX_WHITE );
    cgDC.Assets.scrollBar                 = trap_R_RegisterShaderNoMip( ASSET_SCROLLBAR );
    cgDC.Assets.scrollBarArrowDown        = trap_R_RegisterShaderNoMip( ASSET_SCROLLBAR_ARROWDOWN );
    cgDC.Assets.scrollBarArrowUp          = trap_R_RegisterShaderNoMip( ASSET_SCROLLBAR_ARROWUP );
    cgDC.Assets.scrollBarArrowLeft        = trap_R_RegisterShaderNoMip( ASSET_SCROLLBAR_ARROWLEFT );
    cgDC.Assets.scrollBarArrowRight       = trap_R_RegisterShaderNoMip( ASSET_SCROLLBAR_ARROWRIGHT );
    cgDC.Assets.scrollBarThumb            = trap_R_RegisterShaderNoMip( ASSET_SCROLL_THUMB );
    cgDC.Assets.sliderBar                 = trap_R_RegisterShaderNoMip( ASSET_SLIDER_BAR );
    cgDC.Assets.sliderThumb     =          trap_R_RegisterShaderNoMip( ASSET_SLIDER_THUMB );
}
#endif
/*
=================
CG_Init

Called after every level change or subsystem restart
Will perform callbacks to make the loading info screen update.
=================
*/

/*ariesModel_t* g_pAriesUpper[ARIES_MODEL_COUNT] = { 0 };
ariesModel_t*   g_pAriesLower[ARIES_MODEL_COUNT] = { 0 };
ariesModel_t*   g_pAriesHead[ARIES_MODEL_COUNT] = { 0 };
ariesModel_t*   g_pAriesHelmet[ARIES_MODEL_COUNT] = { 0 }; */

//@Barbatos
#define MISMATCH_ERROR "Client/Server game mismatch: The server you're trying to connect to is no up to date. Please join another server."

void CG_Init( int serverMessageNum, int serverCommandSequence, int clientNum )
{
    int     i, gameVersion;
    const char  *s;
    char        temp[64];
    /*
    unsigned int rnd;

    // Try to get unique random seed value
    trap_Cvar_VariableStringBuffer("cl_guid", temp, sizeof(temp));
    rnd^=(unsigned int)HashStr(temp);
    rnd^=rnd<<13;rnd^=rnd>>3;rnd^=rnd<<27;
    trap_Cvar_VariableStringBuffer("authc", temp, sizeof(temp));
    rnd^=(unsigned int)HashStr(temp);
    rnd^=rnd<<13;rnd^=rnd>>3;rnd^=rnd<<27;
    {
     qtime_t time;
     trap_RealTime(&time);
     for(i=0;i<sizeof(time)/4;i++) rnd^=((int*)&time)[i];
    }
    rnd^=rnd<<13;rnd^=rnd>>3;rnd^=rnd<<27;
    predict_xor1 = rnd&0xFF;
    predict_xor2 = (rnd>>16)&0xFF;
    rnd^=rnd<<13;rnd^=rnd>>3;rnd^=rnd<<27;
    */


    /*  CG_LoadingString( "aries" ); */
    //music
//  trap_S_StartBackgroundTrack("music/mainmenu.wav","true");

    // clear everything
    memset( &cgs, 0, sizeof(cgs));
    memset( &cg, 0, sizeof(cg));

    memset( cg_entities_new, 0, sizeof(cg_entities_new));
    // r00t: Make random real_ptr pointers
    /*
    for(i=0;i<MAX_GENTITIES;i++) {
     int j,r=0;
     while(1) {
      r++;
      rnd^=rnd<<13;
      rnd^=rnd>>3;
      rnd^=rnd<<27;
      j = rnd&(MAX_GENTITIES-1);
      if (!cg_entities_new[j].real_ptr) break;
     }
     cg_entities_new[j].real_ptr = &cg_entities_new[i];
    }
    */
    CG_Printf( "----- CGame Initialization -----\n" );

    memset( cg_weapons, 0, sizeof(cg_weapons));
    memset( cg_items, 0, sizeof(cg_items));
#ifdef ENCRYPTED
    hax_detect_flags = 0;
#endif
    bclear();
    add_pool(cg_malloc_pool, MALLOC_POOL_SIZE);

    cg.clientNum          = clientNum;

    cgs.processedSnapshotNum  = serverMessageNum;
    cgs.serverCommandSequence = serverCommandSequence;

    // load a few needed things before we do any screen updates
    cgs.media.charsetShader    = trap_R_RegisterShader( "gfx/2d/bigchars" );
    cgs.media.ringShader       = trap_R_RegisterShader( "gfx/crosshairs/scopes/scopering" );
    cgs.media.whiteHGrad       = trap_R_RegisterShader( "gfx/2d/hgrad.tga" );
    cgs.media.whiteShader      = trap_R_RegisterShader( "white" );
    cgs.media.whiteShaderSolid = trap_R_RegisterShader( "whitesolid" );
    cgs.media.charsetProp      = trap_R_RegisterShaderNoMip( "menu/art/font1_prop.tga" );
    cgs.media.charsetPropGlow  = trap_R_RegisterShaderNoMip( "menu/art/font1_prop_glo.tga" );
    cgs.media.charsetPropB     = trap_R_RegisterShaderNoMip( "menu/art/font2_prop.tga" );

    cgs.media.laserShader      = trap_R_RegisterShader( "laserShader" );
    cgs.media.trailShader      = trap_R_RegisterShader( "trail" );

    /* CG_LoadingString( "vars" ); */

    CG_RegisterCvars();

    // moved this down a line because it was disturbing the unitialized cvars
    //  --Thaddeus (2002/8/12)

    CG_InitConsoleCommands();

    cg.weaponSelect = 0;

    cgs.redflag = cgs.blueflag = -1; // For compatibily, default to unset for
    cgs.flagStatus  = -1;
    // old servers

    // get the rendering configuration from the client system
    trap_GetGlconfig( &cgs.glconfig );
    cgs.screenXScale = cgs.glconfig.vidWidth / 640.0;
    cgs.screenYScale = cgs.glconfig.vidHeight / 480.0;

    // get the gamestate from the client system
    trap_GetGameState( &cgs.gameState );

    // check version
    s = CG_ConfigString( CS_GAME_VERSION_UPDATER );
    gameVersion = atoi( s );

    //@Barbatos: moved to CG_DrawActiveFrame() cg_view.c
    /*if (strcmp( s, GAME_VERSION ))
    {
        CG_Error( "Client/Server game mismatch: %s/%s. This is usually caused when you try to connect to a server that is running a newer or older version of Urban Terror.  Check www.urbanterror.info for the latest version", GAME_VERSION, s );
    }*/


    //@Barbatos
    /*{
        const char *info, *s, *s2, *s3;

        info      = CG_ConfigString( CS_SERVERINFO );

        // check if g_modversion is correct
        s     = Info_ValueForKey( info, "g_modversion" );
        if(s[0] && (s[0] != GAME_VERSION))
        {
            CG_Error( MISMATCH_ERROR );
        }

        // check for utx zombie mod config
        s = Info_ValueForKey( info, "utx_config" );
        if(s[0])
            CG_Error( MISMATCH_ERROR );

        // check for lux mod config
        s = Info_ValueForKey( info, "ai_Website");
        s2 = Info_ValueForKey( info, "ai_binversion");
        s3 = Info_ValueForKey( info, "lux_config");
        if(s[0] || s2[0] || s3[0])
            CG_Error( MISMATCH_ERROR);


    }*/

    //timechop
//  s = CG_ConfigString( CS_TIMECHOP );
//  cg.timechop = atoi( s );
//  cg.time-=cg.timechop;

    s                  = CG_ConfigString( CS_ROUNDCOUNT );
    cgs.g_roundcount   = atoi( s );

    s                  = CG_ConfigString( CS_ROUNDSTAT );
    strcpy(cg.roundstat, s);

    s                  = CG_ConfigString( CS_LEVEL_START_TIME );
    cgs.levelStartTime = atoi( s );

    s                  = CG_ConfigString( CS_ROUND_END_TIME );
    cgs.roundEndTime   = atoi( s );

    s                  = CG_ConfigString( CS_CAPTURE_SCORE_TIME );
    cg.cahTime         = atoi ( s );

    s                  = CG_ConfigString ( CS_GAME_ID );
    cgs.gameid         = atoi ( s );

    s                  = CG_ConfigString ( CS_NODAMAGE );
    cgs.g_noDamage     = atoi ( s );

    s                  = CG_ConfigString(CS_PAUSE_STATE);
    sscanf (s, "%i %i %i", &cg.pauseState, &cg.pauseTime, &cg.pausedTime);

    s                  = CG_ConfigString(CS_MATCH_STATE);
    cg.matchState      = atoi(s);

/*  s = CG_ConfigString(CS_TEAM_NAME_RED);
    strcpy(cg.teamNameRed, s);

    s = CG_ConfigString(CS_TEAM_NAME_BLUE);
    strcpy(cg.teamNameBlue, s); */

    // Get value.
    trap_Cvar_VariableStringBuffer("m_yaw", temp, sizeof(temp));

    // If value is zero.
    if (atof(temp) == 0.0) {
        trap_Cvar_Set("m_yaw", cg_pauseYaw.string);
    }
#ifdef ENCRYPTED
    {
     // r00t: Send last game hax stats on next connect (cl_time is in userinfo) and clear them
     trap_Cvar_Set("cl_time",va("%i %i",cg_time1.integer,cg_time2.integer));
     trap_Cvar_Set("cg_time1","");
     trap_Cvar_Set("cg_time2","");
    }
#endif
    // Clear temp yaw cvar.
    trap_Cvar_Set("cg_pauseYaw", "0");

    // Get value.
    trap_Cvar_VariableStringBuffer("m_pitch", temp, sizeof(temp));

    // If value is zero.
    if (atof(temp) == 0.0) {
        trap_Cvar_Set("m_pitch", cg_pausePitch.string);
    }

    // Clear temp pitch cvar.
    trap_Cvar_Set("cg_pausePitch", "0");

    for (i = 0; i < MAX_FLAGS; i++) {

        s = CG_ConfigString ( CS_FLAGS + i );

        if ((NULL == s) || (*s == 0)) {
            break;
        }

        sscanf ( s, "%i %i %f %f %f",
             &cgs.gameFlags[i].number,
             &cgs.gameFlags[i].team,
             &cgs.gameFlags[i].origin[0],
             &cgs.gameFlags[i].origin[1],
             &cgs.gameFlags[i].origin[2]  );
    }

    CG_ParseServerinfo();

#ifdef USE_AUTH
    BG_RandomInit(CG_ConfigString(CS_SERVERINFO));
#endif

    // load the new map
    CG_LoadingString( "collision map" );
    trap_CM_LoadMap( cgs.mapname );

#ifdef MISSIONPACK
    String_Init();
#endif

    cg.loading = qtrue;     // force players to load instead of defer

    CG_LoadingString( "hud map" );
    CG_LoadMiniMap();

    CG_LoadingString( "sounds" );
    CG_RegisterSounds();

    CG_LoadingString( "graphics" );
    CG_RegisterGraphics();

    CG_LoadingString( "clients" );

    CG_RegisterClients();       // if low on memory, some clients will be deferred

    CG_LoadingString( "asset cache" );

#ifdef MISSIONPACK
    CG_AssetCache();

    CG_LoadingString( "hud menu" );

    CG_LoadHudMenu();      // load new hud stuff
#endif

    cg.cg_skinAlly = 0;
    cg.cg_skinEnemy = 0;
    if (CG_IsSkinSelectionValid()) {
        cg.cg_skinAlly = cg_skinAlly.integer;
        cg.cg_skinEnemy = cg_skinEnemy.integer;
	skinAllyModificationCount = cg_skinAlly.modificationCount;
	skinEnemyModificationCount = cg_skinEnemy.modificationCount;
    } else {
        Com_Printf("Invalid settings, restoring default settings\n");
    }

    //@r00t: This needs to be done when everything else is initialized so I can use WhichSkin
    CG_UpdateSkins(1);

    cg.loading = qfalse;    // future players will be deferred

    CG_LoadingString( "local entities" );

    CG_InitLocalEntities();

    CG_LoadingString( "mark polys" );

    CG_InitMarkPolys();

    // remove the last loading update
    cg.infoScreenText[0] = 0;

    CG_LoadingString( "config values" );

    // Make sure we have update values (scores)
    CG_SetConfigValues();

    CG_LoadingString( "music" );

    CG_StartMusic();

    CG_LoadingString( "" );

    CG_ShaderStateChanged();

    trap_S_ClearLoopingSounds( qtrue );

    CG_InitPrecipitation();

    cg.IsOwnedSound    = qfalse;    // for No laughing sounds at start up

    cg.DefuseOverlay   = qfalse;    // no defuse overlay yet
    cg.BombDefused     = qfalse;
    cg.DefuseStartTime = 0;
    cg.DefuseLength    = 0;
    cg.DefuseEndTime   = 0;

    cg.OwnedType       = 0;

    cg.overlayFade     = 0.0f;  // overlay Fading

    //CG_SetColorOverlay( 0.0f, 0.0f, 0.0f, 0.0f, 0, 0, 0 );

    /* Initialise aries */
    C_PlayerAnimationsInit();
    C_AriesInit();
}

/*
=================
CG_Shutdown

Called before every level change or subsystem restart
=================
*/
void CG_Shutdown( void )
{
    // some mods may need to do cleanup work here,
    // like closing files or archiving session data
}

/*
==================
CG_EventHandling
==================
 type 0 - no event handling
      1 - team menu
      2 - hud editor

*/
#ifndef MISSIONPACK
void CG_EventHandling(int type)
{
}

/**
 * $(function)
 */
void CG_KeyEvent(int key, qboolean down)
{
}

/**
 * $(function)
 */
void CG_MouseEvent(int x, int y)
{
}
#endif


/* r00t's hax detection and reporting */
#ifdef ENCRYPTED

unsigned int ent_offsets[] = {
 ((unsigned int)&(((refEntity_t*)0)->shaderRGBA)),
 ((unsigned int)&(((refEntity_t*)0)->renderfx)),
 ((unsigned int)&(((refEntity_t*)0)->customShader)),
};

unsigned int refdef_offsets[2] = {
 ((unsigned int)&(((refdef_t*)0)->fov_x)),
 ((unsigned int)&(((refdef_t*)0)->fov_y)),
};

unsigned int refdef_latch[1] = {0};

unsigned int ent_latch[3] = {0};
int hax_detect_flags; //  1 = no scope texture
                      //  2 = no view smoke shader
                      //  4 = no grenade smoke shader
                      //  8 = localhost game
                      //  upper 4 bits = count of bad cvars detected


int hits_num[HL_MAX] = {0}; // 0=total sum of all hits, 1..HL_MAX = areas


void UpdateHaxCounters()
// Collects various info and increments counters, encoding them into cg_time1/2 and tsync string
{
 char temp[256];
 unsigned char temp2[256];
 int sl;
 unsigned int v1=0;
 unsigned int v2=0;
 unsigned int scr;
 unsigned int hits_head;
 unsigned int hits_torso;
 unsigned int hits_legs;
 static unsigned int last_hax_time = 0;
 unsigned int ms = trap_Milliseconds();

 if (ms-last_hax_time>6000) { // counters are 8bit, 6*256 = 25m:36s of activity to saturate
  last_hax_time = ms;
 } else return;

 // Decrypt current values
 trap_Cvar_Update(&cg_time1);
 trap_Cvar_Update(&cg_time2);
 v1 = cg_time1.integer;
 v2 = cg_time2.integer;
 scr = 0xB7E7B3E6^(v1&0xFF);
 scr^=scr<<9;
 scr^=scr>>5;
 scr^=scr<<1;
 v1^=scr;
 scr^=scr<<9;
 scr^=scr>>5;
 scr^=scr<<1;
 v2^=scr;

 // Collect hit areas, normalized to 0..63
 hits_head  = (hits_num[HL_HEAD]+hits_num[HL_HELMET])*63;
 hits_torso = (hits_num[HL_TORSO]+hits_num[HL_VEST]+hits_num[HL_GROIN]+hits_num[HL_BUTT])*63;
 hits_legs  = (hits_num[HL_LEGUL]+hits_num[HL_LEGUR]+hits_num[HL_LEGLL]+hits_num[HL_LEGLR]+hits_num[HL_FOOTL]+hits_num[HL_FOOTR])*63;

 // Unpack words into bytes
 temp2[0]=v1>>8;
 temp2[1]=v1>>16;
 temp2[2]=v1>>24;
 temp2[3]=v2;
 temp2[4]=v2>>8;
 temp2[5]=v2>>16;
 temp2[6]=v2>>24;

// Com_Printf(va("V [%d %d] = %d %d %d | %d %d %d | %d\n",v1,v2,temp2[0],temp2[1],temp2[2],temp2[3],temp2[4],temp2[5],temp2[6]));

 // If checksum is invalid, initialize to zero
 if (((temp2[0]^temp2[1]^temp2[2]^temp2[3]^temp2[4]^temp2[5])&0xFF) != temp2[6]) {
  memset(temp2,0,6);
 }

 // Get area with most hits
 if (hits_num[0]>5) {
  if (hits_head >=hits_torso && hits_head >=hits_legs) scr=(1<<6)|(hits_head/hits_num[0]);  else
  if (hits_torso>=hits_head  && hits_torso>=hits_legs) scr=(2<<6)|(hits_torso/hits_num[0]); else
  if (hits_legs >=hits_torso && hits_legs >=hits_head) scr=(3<<6)|(hits_legs/hits_num[0]);  else scr=63; // something is wrong
 } else scr=0; // not enough data

 // Increase counters based on detected stuff
 if (ent_latch[0] && temp2[0]<0xFF) temp2[0]++; ent_latch[0]=0; // shaderRGBA
 if (ent_latch[1] && temp2[1]<0xFF) temp2[1]++; ent_latch[1]=0; // renderfx
 if (ent_latch[2] && temp2[2]<0xFF) temp2[2]++; ent_latch[2]=0; // customshader
 if (refdef_latch[0] && temp2[3]<0xFF) temp2[3]++; refdef_latch[0]=0; // FOV override
 temp2[4]=scr; // max hits (2 bit = area (1=head,2=torso,3=legs) + 6 bits percent (in 1.5625% steps)
 temp2[5]=hax_detect_flags;

 // Update checksum
 temp2[6] = temp2[0]^temp2[1]^temp2[2]^temp2[3]^temp2[4]^temp2[5];

 // Encrypt again
 v1=(ms^rand())&0xFF;
 scr = 0xB7E7B3E6^v1;
 scr^=scr<<9;
 scr^=scr>>5;
 scr^=scr<<1;
 v1|= (((temp2[0]<<8)|(temp2[1]<<16)|(temp2[2]<<24))^scr)&0xFFFFFF00;
 scr^=scr<<9;
 scr^=scr>>5;
 scr^=scr<<1;
 v2 = (temp2[3]|(temp2[4]<<8)|(temp2[5]<<16)|(temp2[6]<<24))^scr;

 // Update cvars
 trap_Cvar_Set("cg_time1",va("%i",v1));
 trap_Cvar_Set("cg_time2",va("%i",v2));
 Com_sprintf(tsync_string, sizeof(tsync_string),"tsync %i %i",v1,v2); // will be sent to server when player dies
}
#endif
