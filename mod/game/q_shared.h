// Copyright (C) 1999-2000 Id Software, Inc.
//
#ifndef __Q_SHARED_H
#define __Q_SHARED_H

// q_shared.h -- included first by ALL program modules.
// A user mod should never modify this file

// #define Q3_VERSION	"Q3 1.27g"

#define GAME_VERSION			"4.2.015"
#define GAME_VERSION_UPDATER	42015
// it's the same as GAME_VERSION without the dots

#define GAME_NAME				"q3urt42" // q3ut42
#define GAME_DIR				"q3ut4"

#ifdef USE_AUTH

//@Barbatos //@Kalish

#define PRODUCT_VERSION 		"unknown"	// we want the real game client version
#define PLATFORM_VERSION		"unknown"	// we want the system name

#define AUTH_PROTOCOL			2
#define AUTH_SERVER_NAME		"authserver.urbanterror.info"
#define AUTH_SERVER_PORT		27952
#define AUTH_WEBSITE			"www.urbanterror.info"

#define AUTH_KEY_FILE			"authkey"
#define AUTH_KEY_SIZE			32
#define AUTH_KEY_DEFAULT		"00000000000000000000000000000000"

#define AUTH_TIMEOUT			1000
#define AUTH_MAX_REQUEST		5
#define AUTH_MAX_STRING_CHARS	2 * MAX_INFO_STRING
#define AUTH_MAX_STRING_CRYPT	4 * AUTH_MAX_STRING_CHARS // Auth_crypt() takes 4* more chars
#define AUTH_MAX_ARGS			8

#define AUTH_MAX_RENAMES		5

#define AUTH_CMD				"auth"
#define AUTH_CMD_PRE			AUTH_CMD "-"
#define AUTH_CMD_SV 			AUTH_CMD
#define AUTH_CMD_CL 			AUTH_CMD_PRE "set"

#define AUTH_PREFIX 			AUTH_CMD ":"
#define AUTH_PRINT_PREFIX		"^5[" AUTH_CMD "] "
#define AUTH_PM_PREFIX			"^7[pm] "

#endif

#define  PITCH_FUCKER 2000
#define  BOMB_RADIUS  64

//#define NEW_ANIMS
#define MAX_TEAMNAME 32

#ifdef WIN32
  #pragma warning(disable : 4706) // assignment within conditional expression
#endif


#ifdef _WIN32

 #pragma warning(disable : 4018) // signed/unsigned mismatch
 #pragma warning(disable : 4032)
 #pragma warning(disable : 4051)
 #pragma warning(disable : 4057) // slightly different base types
 #pragma warning(disable : 4100) // unreferenced formal parameter
 #pragma warning(disable : 4115)
 #pragma warning(disable : 4125) // decimal digit terminates octal escape sequence
 #pragma warning(disable : 4127) // conditional expression is constant
 #pragma warning(disable : 4136)
 #pragma warning(disable : 4152) // nonstandard extension, function/data pointer conversion in expression
//#pragma warning(disable : 4201)
//#pragma warning(disable : 4214)
 #pragma warning(disable : 4244)
 #pragma warning(disable : 4142) // benign redefinition
//#pragma warning(disable : 4305)		// truncation from const double to float
//#pragma warning(disable : 4310)		// cast truncates constant value
//#pragma warning(disable:	4505)	// unreferenced local function has been removed
 #pragma warning(disable : 4514)
 #pragma warning(disable : 4702) // unreachable code
 #pragma warning(disable : 4711) // selected for automatic inline expansion
 #pragma warning(disable : 4220) // varargs matches remaining parameters
#endif

#if defined(ppc) || defined(__ppc) || defined(__ppc__) || defined(__POWERPC__)
 #define idppc 1
#endif

/**********************************************************************
  VM Considerations

  The VM can not use the standard system headers because we aren't really
  using the compiler they were meant for.  We use bg_lib.h which contains
  prototypes for the functions we define for our own use in bg_lib.c.

  When writing mods, please add needed headers HERE, do not start including
  stuff like <stdio.h> in the various .c files that make up each of the VMs
  since you will be including system headers files can will have issues.

  Remember, if you use a C library function that is not defined in bg_lib.c,
  you will have to add your own version for support in the VM.

 **********************************************************************/

#ifdef Q3_VM

#include "bg_lib.h"

#else

 #include <assert.h>
 #include <math.h>
 #include <stdio.h>
 #include <stdarg.h>
 #include <string.h>
 #include <stdlib.h>
 #include <time.h>
 #include <ctype.h>
 #include <limits.h>

#endif

#ifdef _WIN32

//#pragma intrinsic( memset, memcpy )

#endif

// this is the define for determining if we have an asm version of a C function
#if (defined _M_IX86 || defined __i386__) && !defined __sun__ && !defined __LCC__
 #define id386 1
#else
 #define id386 0
#endif

// for windows fastcall option

#define QDECL

//======================= WIN32 DEFINES =================================

#ifdef WIN32

 #define MAC_STATIC

 #undef QDECL
 #define QDECL __cdecl

// buildstring will be incorporated into the version string
 #ifdef NDEBUG
  #ifdef _M_IX86
   #define CPUSTRING "win-x86"
  #elif defined _M_ALPHA
   #define CPUSTRING "win-AXP"
  #endif
 #else
  #ifdef _M_IX86
   #define CPUSTRING "win-x86-debug"
  #elif defined _M_ALPHA
   #define CPUSTRING "win-AXP-debug"
  #endif
 #endif

 #define PATH_SEP '\\'

#endif

//======================= MAC OS X SERVER DEFINES =====================

#if defined(__MACH__) && defined(__APPLE__)

 #define MAC_STATIC

 #ifdef __ppc__
  #define CPUSTRING "MacOSXS-ppc"
 #elif defined __i386__
  #define CPUSTRING "MacOSXS-i386"
 #else
  #define CPUSTRING "MacOSXS-other"
 #endif

 #define PATH_SEP '/'

 #define GAME_HARD_LINKED
 #define CGAME_HARD_LINKED
 #define UI_HARD_LINKED
 #define BOTLIB_HARD_LINKED

#endif

//======================= MAC DEFINES =================================

#ifdef __MACOS__

 #include <MacTypes.h>
 #define MAC_STATIC static

 #define CPUSTRING	"MacOS-PPC"

 #define PATH_SEP	':'

 #define GAME_HARD_LINKED
 #define CGAME_HARD_LINKED
 #define UI_HARD_LINKED
 #define BOTLIB_HARD_LINKED

void Sys_PumpEvents( void );

#endif

//======================= LINUX DEFINES =================================

// the mac compiler can't handle >32k of locals, so we
// just waste space and make big arrays static...
#ifdef __linux__

 #define MAC_STATIC

 #ifdef __i386__
  #define CPUSTRING "linux-i386"
 #elif defined __axp__
  #define CPUSTRING "linux-alpha"
 #else
  #define CPUSTRING "linux-other"
 #endif

 #define PATH_SEP '/'

#endif

//=============================================================


#ifdef Q3_VM

#define DLLEXPORT

#else

#ifndef _WIN32
#define DLLEXPORT __attribute__((visibility ("default")))
#else
#define DLLEXPORT __declspec(dllexport)
#endif // _WIN32
#endif

typedef unsigned char		  byte;

typedef enum { qfalse, qtrue} qboolean;

typedef int 		  qhandle_t;
typedef int 		  sfxHandle_t;
typedef int 		  fileHandle_t;
typedef int 		  clipHandle_t;

#ifndef NULL
 #define NULL ((void *)0)
#endif

#define MAX_QINT 0x7fffffff
#define MIN_QINT (-MAX_QINT - 1)

// angle indexes
#define PITCH 0 						// up / down
#define YAW   1 						// left / right
#define ROLL  2 						// fall over

// the game guarantees that no string from the network will ever
// exceed MAX_STRING_CHARS
#define MAX_STRING_CHARS  1024		// max length of a string passed to Cmd_TokenizeString
#define MAX_STRING_TOKENS 1024		// max tokens resulting from Cmd_TokenizeString
#define MAX_TOKEN_CHARS   1024		// max length of an individual token

// Was 1024 for 3.0
// increaesd for some maps in 3.1 (4096)
//@Barbatos: back to 1024 for 4.2 because the value in the engine (q3, ioq3...) is 1024
#define MAX_INFO_STRING 1024
#define MAX_INFO_KEY	1024 //@Barbatos: back to 1024 for 4.2 (from 2048)
#define MAX_INFO_VALUE	1024 //@Barbatos: back to 1024 for 4.2 (from 2048)

// Urban Terror 3.0
#define BIG_INFO_STRING 8192		  // used for system info key only
#define BIG_INFO_KEY	8192
#define BIG_INFO_VALUE	8192

// Urban Terror 3.0a 16384
//#define BIG_INFO_STRING		16384
//#define BIG_INFO_KEY			8192
//#define BIG_INFO_VALUE		8192

#define MAX_QPATH				64				// max length of a quake game pathname
#define MAX_OSPATH				256 			// max length of a filesystem pathname

#define MAX_NAME_LENGTH 		32			// max length of a client name

#define MAX_SAY_TEXT			150

// Fenix: 4.2.013
#define MAX_MAPLIST_SIZE    8       // Maximum number of maps to display upon partial name multiple match
#define MAX_MAPLIST_STRING  8192    // Length of the string retrieved using FS_GetFileList

// parameters for command buffer stuffing
typedef enum
{
	EXEC_NOW,			// don't return until completed, a VM should NEVER use this,
	// because some commands might cause the VM to be unloaded...
	EXEC_INSERT,			// insert at current position, but don't run yet
	EXEC_APPEND 	// add to end of the command buffer (normal case)
} cbufExec_t;

//
// these aren't needed by any of the VMs.  put in another header?
//
#define MAX_MAP_AREA_BYTES 32				// bit vector of area visibility

// print levels from renderer (FIXME: set up for game / cgame?)
typedef enum
{
	PRINT_ALL,
	PRINT_DEVELOPER,		// only print when "developer 1"
	PRINT_WARNING,
	PRINT_ERROR
} printParm_t;

#ifdef ERR_FATAL
 #undef ERR_FATAL			// this is be defined in malloc.h
#endif

// parameters to the main Error routine
typedef enum
{
	ERR_FATAL,					// exit the entire game with a popup window
	ERR_DROP,					// print to console and disconnect from game
	ERR_SERVERDISCONNECT,				// don't kill server
	ERR_DISCONNECT, 				// client disconnected from the server
	ERR_NEED_CD 				// pop up the need-cd dialog
} errorParm_t;

// font rendering values used by ui and cgame

#define PROP_GAP_WIDTH		  3
#define PROP_SPACE_WIDTH	  8
#define PROP_HEIGHT 		  27
#define PROP_SMALL_SIZE_SCALE 0.75

#define BLINK_DIVISOR		  200
#define PULSE_DIVISOR		  75

#define UI_LEFT 		  0x00000000	// default
#define UI_CENTER		  0x00000001
#define UI_RIGHT		  0x00000002
#define UI_FORMATMASK	  0x00000007
#define UI_SMALLFONT	  0x00000010
#define UI_BIGFONT		  0x00000020	// default
#define UI_GIANTFONT	  0x00000040
#define UI_UTTINYFONT	  0x00000080 // used in the loading... screen
#define UI_DROPSHADOW	  0x00000800
#define UI_BLINK		  0x00001000
#define UI_INVERSE		  0x00002000
#define UI_PULSE		  0x00004000

#if defined(_DEBUG) && !defined(BSPC)
 #define HUNK_DEBUG
#endif

typedef enum
{
	h_high,
	h_low,
	h_dontcare
} ha_pref;

#ifdef HUNK_DEBUG
 #define Hunk_Alloc( size, preference ) Hunk_AllocDebug(size, preference, # size, __FILE__, __LINE__)
void *Hunk_AllocDebug( int size, ha_pref preference, char *label, char *file, int line );
#else
void *Hunk_Alloc( int size, ha_pref preference );
#endif

#define Com_Memset memset
#define Com_Memcpy memcpy

#define CIN_system 1
#define CIN_loop   2
#define CIN_hold   4
#define CIN_silent 8
#define CIN_shader 16

/*
==============================================================

MATHLIB

==============================================================
*/

typedef float vec_t;
typedef vec_t vec2_t[2];
typedef vec_t vec3_t[3];
typedef vec_t vec4_t[4];
typedef vec_t vec5_t[5];

typedef int   fixed4_t;
typedef int   fixed8_t;
typedef int   fixed16_t;

#ifndef M_PI
 #define M_PI 3.14159265358979323846f		// matches value in gcc v2 math.h
#endif

#define NUMVERTEXNORMALS 162
extern vec3_t  bytedirs[NUMVERTEXNORMALS];

// all drawing is done to a 640*480 virtual screen size
// and will be automatically scaled to the real resolution
#define SCREEN_WIDTH	 640
#define SCREEN_HEIGHT	 480

#define TINYCHAR_WIDTH	 (SMALLCHAR_WIDTH)
#define TINYCHAR_HEIGHT  (SMALLCHAR_HEIGHT / 2)

#define SMALLCHAR_WIDTH  8
#define SMALLCHAR_HEIGHT 16

#define BIGCHAR_WIDTH	 16
#define BIGCHAR_HEIGHT	 16

#define GIANTCHAR_WIDTH  32
#define GIANTCHAR_HEIGHT 48

extern vec4_t  colorBlack;
extern vec4_t  colorRed;
extern vec4_t  colorGreen;
extern vec4_t  colorBlue;
extern vec4_t  colorYellow;
extern vec4_t  colorMagenta;
extern vec4_t  colorCyan;
extern vec4_t  colorWhite;
extern vec4_t  colorLtGrey;
extern vec4_t  colorMdGrey;
extern vec4_t  colorDkGrey;

#define Q_COLOR_ESCAPE '^'
#define Q_IsColorString(p) (p && *(p) == Q_COLOR_ESCAPE && *((p) + 1) && *((p) + 1) != Q_COLOR_ESCAPE)

#define COLOR_BLACK   '0'
#define COLOR_RED	  '1'
#define COLOR_GREEN   '2'
#define COLOR_YELLOW  '3'
#define COLOR_BLUE	  '4'
#define COLOR_CYAN	  '5'
#define COLOR_MAGENTA '6'
#define COLOR_WHITE   '7'
#define COLOR_ORANGE   '8'
#define COLOR_OLIVE   '9'
#define ColorIndex(c) (((c) - '0') % 10)

#define S_COLOR_BLACK	"^0"
#define S_COLOR_RED "^1"
#define S_COLOR_GREEN	"^2"
#define S_COLOR_YELLOW	"^3"
#define S_COLOR_BLUE	"^4"
#define S_COLOR_CYAN	"^5"
#define S_COLOR_MAGENTA "^6"
#define S_COLOR_WHITE	"^7"
#define S_COLOR_ORANGE	"^8"
#define S_COLOR_OLIVE	"^9"

extern vec4_t  g_color_table[10];

#define MAKERGB( v, r, g, b )	  v[0] = r; v[1] = g; v[2] = b
#define MAKERGBA( v, r, g, b, a ) v[0] = r; v[1] = g; v[2] = b; v[3] = a

#define DEG2RAD( a )		  (((a) * M_PI) / 180.0F)
#define RAD2DEG( a )		  (((a) * 180.0f) / M_PI)

struct cplane_s;

extern vec3_t  vec3_origin;
extern vec3_t  axisDefault[3];

#define nanmask (255 << 23)

#define IS_NAN(x) (((*(int *)&x) & nanmask) == nanmask)

float Q_fabs( float f );
float Q_rsqrt( float f );		// reciprocal square root

#define SQRTFAST( x ) (1.0f / Q_rsqrt( x ))

signed char  ClampChar( int i );
signed short ClampShort( int i );

// this isn't a real cheap function to call!
int  DirToByte( vec3_t dir );
void ByteToDir( int b, vec3_t dir );

#if 1

 #define DotProduct(x, y)		 ((x)[0] * (y)[0] + (x)[1] * (y)[1] + (x)[2] * (y)[2])
 #define VectorSubtract(a, b, c) ((c)[0] = (a)[0] - (b)[0], (c)[1] = (a)[1] - (b)[1], (c)[2] = (a)[2] - (b)[2])
 #define VectorAdd(a, b, c) 	 ((c)[0] = (a)[0] + (b)[0], (c)[1] = (a)[1] + (b)[1], (c)[2] = (a)[2] + (b)[2])
 #define VectorCopy(a, b)		 ((b)[0] = (a)[0], (b)[1] = (a)[1], (b)[2] = (a)[2])
 #define VectorScale(v, s, o)	 ((o)[0] = (v)[0] * (s), (o)[1] = (v)[1] * (s), (o)[2] = (v)[2] * (s))
 #define VectorMA(v, s, b, o)	 ((o)[0] = (v)[0] + (b)[0] * (s), (o)[1] = (v)[1] + (b)[1] * (s), (o)[2] = (v)[2] + (b)[2] * (s))

#else

 #define DotProduct(x, y)		 _DotProduct(x, y)
 #define VectorSubtract(a, b, c) _VectorSubtract(a, b, c)
 #define VectorAdd(a, b, c) 	 _VectorAdd(a, b, c)
 #define VectorCopy(a, b)		 _VectorCopy(a, b)
 #define VectorScale(v, s, o)	 _VectorScale(v, s, o)
 #define VectorMA(v, s, b, o)	 _VectorMA(v, s, b, o)

#endif

#ifdef __LCC__
 #ifdef VectorCopy
  #undef VectorCopy
// this is a little hack to get more efficient copies in our interpreter
typedef struct
{
	float  v[3];
} vec3struct_t;
  #define VectorCopy(a, b) * (vec3struct_t *)b = *(vec3struct_t *)a;
 #endif
#endif

#define VectorClear(a)		  ((a)[0] = (a)[1] = (a)[2] = 0)
#define VectorNegate(a, b)	  ((b)[0] = -(a)[0], (b)[1] = -(a)[1], (b)[2] = -(a)[2])
#define VectorSet(v, x, y, z) ((v)[0] = (x), (v)[1] = (y), (v)[2] = (z))
#define Vector4Copy(a, b)	  ((b)[0] = (a)[0], (b)[1] = (a)[1], (b)[2] = (a)[2], (b)[3] = (a)[3])

#define SnapVector(v)		  { v[0] = ((int)(v[0])); v[1] = ((int)(v[1])); v[2] = ((int)(v[2]));}
// just in case you do't want to use the macros
vec_t	 _DotProduct( const vec3_t v1, const vec3_t v2 );
void	 _VectorSubtract( const vec3_t veca, const vec3_t vecb, vec3_t out );
void	 _VectorAdd( const vec3_t veca, const vec3_t vecb, vec3_t out );
void	 _VectorCopy( const vec3_t in, vec3_t out );
void	 _VectorScale( const vec3_t in, float scale, vec3_t out );
void	 _VectorMA( const vec3_t veca, float scale, const vec3_t vecb, vec3_t vecc );

unsigned ColorBytes3 (float r, float g, float b);
unsigned ColorBytes4 (float r, float g, float b, float a);

float	 NormalizeColor( const vec3_t in, vec3_t out );

float	 RadiusFromBounds( const vec3_t mins, const vec3_t maxs );
void	 ClearBounds( vec3_t mins, vec3_t maxs );
void	 AddPointToBounds( const vec3_t v, vec3_t mins, vec3_t maxs );
int 	 VectorCompare( const vec3_t v1, const vec3_t v2 );

#define VectorLength(v) sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2])

//vec_t    VectorLength( const vec3_t v );
vec_t	 VectorLengthSquared( const vec3_t v );
vec_t	 Distance( const vec3_t p1, const vec3_t p2 );
vec_t	 DistanceSquared( const vec3_t p1, const vec3_t p2 );
//void	   CrossProduct( const vec3_t v1, const vec3_t v2, vec3_t cross );

#define CrossProduct( v1, v2, cross ) do{\
	cross[0] = v1[1] * v2[2] - v1[2] * v2[1];\
	cross[1] = v1[2] * v2[0] - v1[0] * v2[2];\
	cross[2] = v1[0] * v2[1] - v1[1] * v2[0];\
	}while(0)

#define VectorNormalizeNR(_v) do{\
	float length = sqrt(_v[0]*_v[0]+_v[1]*_v[1]+_v[2]*_v[2]);\
	if (length) {\
	 length = 1/length;\
	 _v[0]	 *= length;\
	 _v[1]	*= length;\
	 _v[2]	 *= length;\
	}\
	   }while(0)

#define VectorNormalize2NR(_v,_o) do{\
	float length = sqrt(_v[0]*_v[0]+_v[1]*_v[1]+_v[2]*_v[2]);\
	if (length) {\
	 length = 1/length;\
	 _o[0] = _v[0] * length;\
	 _o[1] = _v[1] * length;\
	 _o[2] = _v[2] * length;\
	} else {\
	 _o[0] = _o[1] = _o[2] = 0;\
	}\
	   }while(0)

#define VectorNormalize(_v,_r) do{\
	_r = sqrt(_v[0]*_v[0]+_v[1]*_v[1]+_v[2]*_v[2]);\
	if (_r) {\
	 float length = 1/_r;\
	 _v[0]	 *= length;\
	 _v[1]	 *= length;\
	 _v[2]	 *= length;\
	}\
	   }while(0)


//vec_t    VectorNormalize (vec3_t v);		  // returns vector length
void	 VectorNormalizeFast(vec3_t v); 	// does NOT return vector length, uses rsqrt approximation
vec_t	 VectorNormalize2( const vec3_t v, vec3_t out );
void	 VectorInverse (vec3_t v);
void	 Vector4Scale( const vec4_t in, vec_t scale, vec4_t out );
void	 VectorRotate( vec3_t in, vec3_t matrix[3], vec3_t out );
int 	 Q_log2(int val);

float	 Q_acos(float c);

int 	 Q_rand( int *seed );
float	 Q_random( int *seed );
float	 Q_crandom( int *seed );

#if 0 // poor random
#define RANDTYPE int
#define _rndstep(seed) (*(seed))=(69069*(*(seed))+1)
#define _rand(seed) *(seed)
#define _random(seed)  (((seed)[0] & 0xffff) / (float)0x10000)
#define _crandom(seed) (2.0 * (( ( (seed)[0] & 0xffff) / (float)0x10000 ) - 0.5))
#endif

#if 1 // 32bit xorshift random with 2^32 period and good entropy
#define RANDTYPE unsigned int
#define _rndstep(seed) do{ (seed)[0]^=(seed)[0]<<5;(seed)[0]^=(seed)[0]<<21;(seed)[0]^=(seed)[0]>>27; }while(0)
#define _rand(seed) *(seed)
#define _random(seed)  (((seed)[0] & 0xffff) / (float)0x10000)
#define _crandom(seed) (2.0 * (( ( (seed)[0] & 0xffff) / (float)0x10000 ) - 0.5))
#endif


#define random()  ((rand () & 0x7fff) / ((float)0x7fff))
#define crandom() (2.0 * (random() - 0.5))

void vectoangles( const vec3_t value1, vec3_t angles);
//void	AnglesToAxis( const vec3_t angles, vec3_t axis[3] );

#define AngleVectors( angles, forward, right, up) do{\
  float  angle;\
  float  sr, sp, sy, cr, cp, cy;\
  angle = (angles)[YAW] * (M_PI * 2 / 360);\
  sy	= sin(angle);\
  cy	= cos(angle);\
  angle = (angles)[PITCH] * (M_PI * 2 / 360);\
  sp	= sin(angle);\
  cp	= cos(angle);\
  angle = (angles)[ROLL] * (M_PI * 2 / 360);\
  sr	= sin(angle);\
  cr	= cos(angle);\
  (forward)[0] = cp * cy;\
  (forward)[1] = cp * sy;\
  (forward)[2] = -sp;\
  (right)[0] = (-1 * sr * sp * cy + - 1 * cr * -sy);\
  (right)[1] = (-1 * sr * sp * sy + - 1 * cr * cy);\
  (right)[2] = -1 * sr * cp;\
  (up)[0] = (cr * sp * cy + - sr * -sy);\
  (up)[1] = (cr * sp * sy + - sr * cy);\
  (up)[2] = cr * cp;\
 }while(0)

#define AngleVectorsFU( angles, forward, right, up) do{\
  float  angle;\
  float  sr, sp, sy, cr, cp, cy;\
  angle = (angles)[YAW] * (M_PI * 2 / 360);\
  sy	= sin(angle);\
  cy	= cos(angle);\
  angle = (angles)[PITCH] * (M_PI * 2 / 360);\
  sp	= sin(angle);\
  cp	= cos(angle);\
  angle = (angles)[ROLL] * (M_PI * 2 / 360);\
  sr	= sin(angle);\
  cr	= cos(angle);\
  (forward)[0] = cp * cy;\
  (forward)[1] = cp * sy;\
  (forward)[2] = -sp;\
  (up)[0] = (cr * sp * cy + - sr * -sy);\
  (up)[1] = (cr * sp * sy + - sr * cy);\
  (up)[2] = cr * cp;\
 }while(0)

#define AngleVectorsRU( angles, forward, right, up) do{\
  float  angle;\
  float  sr, sp, sy, cr, cp, cy;\
  angle = (angles)[YAW] * (M_PI * 2 / 360);\
  sy	= sin(angle);\
  cy	= cos(angle);\
  angle = (angles)[PITCH] * (M_PI * 2 / 360);\
  sp	= sin(angle);\
  cp	= cos(angle);\
  angle = (angles)[ROLL] * (M_PI * 2 / 360);\
  sr	= sin(angle);\
  cr	= cos(angle);\
  (right)[0] = (-1 * sr * sp * cy + - 1 * cr * -sy);\
  (right)[1] = (-1 * sr * sp * sy + - 1 * cr * cy);\
  (right)[2] = -1 * sr * cp;\
  (up)[0] = (cr * sp * cy + - sr * -sy);\
  (up)[1] = (cr * sp * sy + - sr * cy);\
  (up)[2] = cr * cp;\
 }while(0)


#define AngleVectorsFR( angles, forward, right, up) do{\
  float  angle;\
  float  sr, sp, sy, cr, cp, cy;\
  angle = (angles)[YAW] * (M_PI * 2 / 360);\
  sy	= sin(angle);\
  cy	= cos(angle);\
  angle = (angles)[PITCH] * (M_PI * 2 / 360);\
  sp	= sin(angle);\
  cp	= cos(angle);\
  angle = (angles)[ROLL] * (M_PI * 2 / 360);\
  sr	= sin(angle);\
  cr	= cos(angle);\
  (forward)[0] = cp * cy;\
  (forward)[1] = cp * sy;\
  (forward)[2] = -sp;\
  (right)[0] = (-1 * sr * sp * cy + - 1 * cr * -sy);\
  (right)[1] = (-1 * sr * sp * sy + - 1 * cr * cy);\
  (right)[2] = -1 * sr * cp;\
 }while(0)

#define AngleVectorsU( angles, forward, right, up) do{\
  float  angle;\
  float  sr, sp, sy, cr, cp, cy;\
  angle = (angles)[YAW] * (M_PI * 2 / 360);\
  sy	= sin(angle);\
  cy	= cos(angle);\
  angle = (angles)[PITCH] * (M_PI * 2 / 360);\
  sp	= sin(angle);\
  cp	= cos(angle);\
  angle = (angles)[ROLL] * (M_PI * 2 / 360);\
  sr	= sin(angle);\
  cr	= cos(angle);\
  (up)[0] = (cr * sp * cy + - sr * -sy);\
  (up)[1] = (cr * sp * sy + - sr * cy);\
  (up)[2] = cr * cp;\
 }while(0)


#define AngleVectorsF( angles, forward, right, up) do{\
  float  angle;\
  float  sp, sy, cp, cy;\
  angle = angles[YAW] * (M_PI * 2 / 360);\
  sy	= sin(angle);\
  cy	= cos(angle);\
  angle = angles[PITCH] * (M_PI * 2 / 360);\
  sp	= sin(angle);\
  cp	= cos(angle);\
  forward[0] = cp * cy;\
  forward[1] = cp * sy;\
  forward[2] = -sp;\
 }while(0)

#define AnglesToAxis(angles, axis) do{\
	vec3_t	right;\
	AngleVectors( angles, axis[0], right, axis[2] );\
	VectorSubtract( vec3_origin, right, axis[1] );\
	   }while(0)


void  AxisClear( vec3_t axis[3] );
void  AxisCopy( vec3_t in[3], vec3_t out[3] );

void SetPlaneSignbits( struct cplane_s *out );
int  BoxOnPlaneSide (vec3_t emins, vec3_t emaxs, struct cplane_s *plane);

#define AngleMod(a) (float)((360.0 / 65536) * ((int)(((float)a) * (65536 / 360.0)) & 65535))

//float AngleMod(float a);
float	 LerpAngle (float from, float to, float frac);
void	 LerpVector ( vec3_t from, vec3_t to, float lerp, vec3_t out );

#define AngleSubtract( a1, a2, a ) do{\
	a = a1 - a2;\
	while (a > 180) a -= 360; \
	while (a < -180) a += 360; \
	}while(0)


//float    AngleSubtract( float a1, float a2 );

#define AnglesSubtract( v1, v2, v3 )  AngleSubtract( v1[0], v2[0], v3[0] );\
					  AngleSubtract( v1[1], v2[1], v3[1] );\
					  AngleSubtract( v1[2], v2[2], v3[2] )

//void	   AnglesSubtract( vec3_t v1, vec3_t v2, vec3_t v3 );

float	 AngleNormalize360 ( float angle );
float	 AngleNormalize180 ( float angle );
float	 AngleDelta ( float angle1, float angle2 );

qboolean PlaneFromPoints( vec4_t plane, const vec3_t a, const vec3_t b, const vec3_t c );
void	 ProjectPointOnPlane( vec3_t dst, const vec3_t p, const vec3_t normal );
void	 RotatePointAroundVector( vec3_t dst, const vec3_t dir, const vec3_t point, float degrees );
void	 RotateAroundDirection( vec3_t axis[3], float yaw );
void	 RotateAroundDirection2( vec3_t axis[3], float yaw );
void	 MakeNormalVectors( const vec3_t forward, vec3_t right, vec3_t up );
// perpendicular vector could be replaced by this

//int	PlaneTypeForNormal (vec3_t normal);

//void MatrixMultiply(float in1[3][3], float in2[3][3], float out[3][3]);
//void MatrixMultiply4(float in1[4][4], float in2[4][4], float out[4][4]);
void MatrixRotate4(float in1[4][4], float in2[3][3], float out[4][4]);
//void MatrixConvert34(float in[3][3], float out[4][4]);
//void AngleVectors( const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up);
//void PerpendicularVector( vec3_t dst, const vec3_t src );

#define ProjectPointOnPlane(dst, _p, normal ) do{\
	float	d;\
	float	inv_denom;\
	inv_denom = 1.0f / DotProduct( normal, normal );\
	inv_denom*= inv_denom;\
	d	  = DotProduct( normal, _p ) * inv_denom;\
	dst[0]	  = _p[0] - d * normal[0];\
	dst[1]	  = _p[1] - d * normal[1];\
	dst[2]	  = _p[2] - d * normal[2];\
	   }while(0);

#define PerpendicularVector(dst,src) do{\
	int pos;\
	float	minelem = 1.0F;\
	float	fa;\
	vec3_t	tempvec;\
	pos = 0;\
	minelem = fabs(src[0]);\
	fa = fabs(src[1]);if (fa<minelem) { pos = 1; minelem = fa; }\
	fa = fabs(src[2]);if (fa<minelem) { pos = 2; minelem = fa; }\
	tempvec[0] = tempvec[1] = tempvec[2] = 0.0F;\
	tempvec[pos] = 1.0F;\
	ProjectPointOnPlane( dst, tempvec, src );\
	VectorNormalizeNR( dst );\
	   }while(0);


#define MatrixConvert34(in, out) do{\
	out[0][0] = in[0][0]; out[0][1] = in[0][1]; out[0][2] = in[0][2]; out[3][3] = 0.0f;\
	out[1][0] = in[1][0]; out[1][1] = in[1][1]; out[1][2] = in[1][2]; out[3][3] = 0.0f;\
	out[2][0] = in[2][0]; out[2][1] = in[2][1]; out[2][2] = in[2][2]; out[3][3] = 0.0f;\
	out[3][0] = 0.0f;	  out[3][1] = 0.0f; 	out[3][2] = 0.0f;	  out[3][3] = 1.0f;\
	   }while(0);


#define MatrixMultiply(in1,in2,out) do{\
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] + in1[0][2] * in2[2][0];\
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] + in1[0][2] * in2[2][1];\
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] + in1[0][2] * in2[2][2];\
	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] + in1[1][2] * in2[2][0];\
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] + in1[1][2] * in2[2][1];\
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] + in1[1][2] * in2[2][2];\
	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] + in1[2][2] * in2[2][0];\
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] + in1[2][2] * in2[2][1];\
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] + in1[2][2] * in2[2][2];\
	   }while(0);


#define MatrixMultiply4(in1,in2,out) do{\
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] + in1[0][2] * in2[2][0] + in1[0][3] * in2[3][0];\
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] + in1[0][2] * in2[2][1] + in1[0][3] * in2[3][1];\
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] + in1[0][2] * in2[2][2] + in1[0][3] * in2[3][2];\
	out[0][3] = in1[0][0] * in2[0][3] + in1[0][1] * in2[1][3] + in1[0][2] * in2[2][3] + in1[0][3] * in2[3][3];\
	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] + in1[1][2] * in2[2][0] + in1[1][3] * in2[3][0];\
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] + in1[1][2] * in2[2][1] + in1[1][3] * in2[3][1];\
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] + in1[1][2] * in2[2][2] + in1[1][3] * in2[3][2];\
	out[1][3] = in1[1][0] * in2[0][3] + in1[1][1] * in2[1][3] + in1[1][2] * in2[2][3] + in1[1][3] * in2[3][3];\
	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] + in1[2][2] * in2[2][0] + in1[2][3] * in2[3][0];\
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] + in1[2][2] * in2[2][1] + in1[2][3] * in2[3][1];\
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] + in1[2][2] * in2[2][2] + in1[2][3] * in2[3][2];\
	out[2][3] = in1[2][0] * in2[0][3] + in1[2][1] * in2[1][3] + in1[2][2] * in2[2][3] + in1[2][3] * in2[3][3];\
	out[3][0] = in1[2][0] * in2[0][0] + in1[3][1] * in2[1][0] + in1[2][2] * in2[2][0] + in1[3][3] * in2[3][0];\
	out[3][1] = in1[2][0] * in2[0][1] + in1[3][1] * in2[1][1] + in1[2][2] * in2[2][1] + in1[3][3] * in2[3][1];\
	out[3][2] = in1[2][0] * in2[0][2] + in1[3][1] * in2[1][2] + in1[2][2] * in2[2][2] + in1[3][3] * in2[3][2];\
	out[3][3] = in1[2][0] * in2[0][3] + in1[3][1] * in2[1][3] + in1[2][2] * in2[2][3] + in1[3][3] * in2[3][3];\
	   }while(0);


//=============================================

float Com_Clamp( float min, float max, float value );

char *COM_SkipPath( char *pathname );
void  COM_StripExtension( const char *in, char *out );
void  COM_StripFilename ( const char *in, char *out );
void  COM_DefaultExtension( char *path, int maxSize, const char *extension );

void  COM_BeginParseSession( const char *name );
int   COM_GetCurrentParseLine( void );
int   COM_IncrementCurrentParseLine( void );
char *COM_Parse( char * *data_p );
char *COM_ParseExt( char * *data_p, qboolean allowLineBreak );
int   COM_Compress( char *data_p );
void  COM_ParseError( char *format, ... );
void  COM_ParseWarning( char *format, ... );
//int		COM_ParseInfos( char *buf, int max, char infos[][MAX_INFO_STRING] );
char *COM_SkipWhitespace( char *data, qboolean *hasNewLines );

#define MAX_TOKENLENGTH 1024

#ifndef TT_STRING
//token types
 #define TT_STRING	1							// string
 #define TT_LITERAL 2							// literal
 #define TT_NUMBER	3							// number
 #define TT_NAME	4							// name
 #define TT_PUNCTUATION 5						// punctuation
#endif

typedef struct pc_token_s
{
	int    type;
	int    subtype;
	int    intvalue;
	float  floatvalue;
	char   string[MAX_TOKENLENGTH];
} pc_token_t;

// data is an in/out parm, returns a parsed out token

void	   COM_MatchToken( char * *buf_p, char *match );

void	   SkipBracedSection (char * *program);
void	   SkipRestOfLine ( char * *data );

void	   Parse1DMatrix (char * *buf_p, int x, float *m);
void	   Parse2DMatrix (char * *buf_p, int y, int x, float *m);
void	   Parse3DMatrix (char * *buf_p, int z, int y, int x, float *m);

int  QDECL Com_sprintf (char *dest, int size, const char *fmt, ...);

// mode parm for FS_FOpenFile
typedef enum
{
	FS_READ,
	FS_WRITE,
	FS_APPEND,
	FS_APPEND_SYNC
} fsMode_t;

typedef enum
{
	FS_SEEK_CUR,
	FS_SEEK_END,
	FS_SEEK_SET
} fsOrigin_t;

//=============================================

int Q_isprint( int c );
int Q_islower( int c );
int Q_isupper( int c );
int Q_isalpha( int c );

// portable case insensitive compare
//int		Q_stricmp (const char *s1, const char *s2);

#define Q_stricmp(s1, s2) (int)Q_stricmpn ((const char *)(s1), (const char *)(s2), 99999)

int   Q_strncmp (const char *s1, const char *s2, int n);
int   Q_stricmpn (const char *s1, const char *s2, int n);
char *Q_strlwr( char *s1 );
char *Q_strupr( char *s1 );
char *Q_strrchr( const char *string, int c );

// Fenix: for substring matching
int Q_strsub (const char *s1, const char *s2);
int Q_strisub (const char *s1, const char *s2);

// buffer size safe library replacements
void Q_strncpyz( char *dest, const char *src, int destsize );
void Q_strcat( char *dest, int size, const char *src );

// strlen that discounts Quake color sequences
int   Q_PrintStrlen( const char *string );
// removes color sequences from string
char *Q_CleanStr( char *string );

//=============================================

// 64-bit integers for global rankings interface
// implemented as a struct for qvm compatibility
typedef struct
{
	byte  b0;
	byte  b1;
	byte  b2;
	byte  b3;
	byte  b4;
	byte  b5;
	byte  b6;
	byte  b7;
} qint64;

//=============================================

short		BigShort(short l);
short		LittleShort(short l);
int 	BigLong (int l);
int 	LittleLong (int l);
qint64		BigLong64 (qint64 l);
qint64		LittleLong64 (qint64 l);
float		BigFloat (float l);
float		LittleFloat (float l);

void		Swap_Init (void);
char *QDECL va(char *format, ...);

//=============================================

//
// key / value info strings
//
char *	 Info_ValueForKey( const char *s, const char *key );
void	 Info_RemoveKey( char *s, const char *key );
void	 Info_RemoveKey_big( char *s, const char *key );
void	 Info_SetValueForKey( char *s, const char *key, const char *value );
void	 Info_SetValueForKey_Big( char *s, const char *key, const char *value );
qboolean Info_Validate( const char *s );
void	 Info_NextPair( const char * *s, char *key, char *value );

// this is only here so the functions in q_shared.c and bg_*.c can link
void QDECL Com_Error( int level, const char *error, ... );
void QDECL Com_Printf( const char *msg, ... );

#ifndef CVAR_DEF

/*
==========================================================

CVARS (console variables)

Many variables can be used for cheating purposes, so when
cheats is zero, force all unspecified variables to their
default values.
==========================================================
*/

#define CVAR_ARCHIVE		1		   // set to cause it to be saved to vars.rc
// used for system variables, not for player
// specific configurations
#define CVAR_USERINFO		2			// sent to server on connect or change
#define CVAR_SERVERINFO 	4			// sent in response to front end requests
#define CVAR_SYSTEMINFO 	8			// these cvars will be duplicated on all clients
#define CVAR_INIT			16			// don't allow change from console at all,
// but can be set from the command line
#define CVAR_LATCH			32			// will only change when C code next does
// a Cvar_Get(), so it can't be changed
// without proper initialization.  modified
// will be set, even though the value hasn't
// changed yet
#define CVAR_ROM			64			  // display only, cannot be set by user at all
#define CVAR_USER_CREATED	128 		  // created by a set command
#define CVAR_TEMP			256 		  // can be set even when cheats are disabled, but is not archived
#define CVAR_CHEAT			512 		  // can not be changed if cheats are disabled
#define CVAR_NORESTART		1024		  // do not clear when a cvar_restart is issued

// nothing outside the Cvar_*() functions should modify these fields!
typedef struct cvar_s
{
	char		   *name;
	char		   *string;
	char		   *resetString;			// cvar_restart will reset to this value
	char		   *latchedString;			// for CVAR_LATCH vars
	int 		   flags;
	qboolean	   modified;				// set each time the cvar is changed
	int 		   modificationCount;		// incremented each time the cvar is changed
	float		   value;					// atof( string )
	int 		   integer; 				// atoi( string )
	struct cvar_s  *next;
	struct cvar_s  *hashNext;
} cvar_t;

#define MAX_CVAR_VALUE_STRING 256

typedef int cvarHandle_t;

// the modules that run in the virtual machine can't access the cvar_t directly,
// so they must ask for structured updates
typedef struct
{
	cvarHandle_t  handle;
	int 		  modificationCount;
	float		  value;
	int 		  integer;
	char		  string[MAX_CVAR_VALUE_STRING];
} vmCvar_t;

#endif

/*
==============================================================

COLLISION DETECTION

==============================================================
*/

#include "surfaceflags.h"			// shared with the q3map utility

// plane types are used to speed some tests
// 0-2 are axial planes
#define PLANE_X 	0
#define PLANE_Y 	1
#define PLANE_Z 	2
#define PLANE_NON_AXIAL 3

/*
=================
PlaneTypeForNormal
=================
*/

#define PlaneTypeForNormal(x) (x[0] == 1.0 ? PLANE_X : (x[1] == 1.0 ? PLANE_Y : (x[2] == 1.0 ? PLANE_Z : PLANE_NON_AXIAL)))

// plane_t structure
// !!! if this is changed, it must be changed in asm code too !!!
typedef struct cplane_s
{
	vec3_t	normal;
	float	dist;
	byte	type;			// for fast side tests: 0,1,2 = axial, 3 = nonaxial
	byte	signbits;		// signx + (signy<<1) + (signz<<2), used as lookup during collision
	byte	pad[2];
} cplane_t;

// a trace is returned when a box is swept through the world
typedef struct
{
	qboolean  allsolid; 		// if true, plane is not valid
	qboolean  startsolid;		// if true, the initial point was in a solid area
	float	  fraction; 		// time completed, 1.0 = didn't hit anything
	vec3_t	  endpos;			// final position
	cplane_t  plane;			// surface normal at impact, transformed to world space
	int 	  surfaceFlags; 	// surface hit
	int 	  contents; 		// contents on other side of surface hit
	int 	  entityNum;		// entity the contacted sirface is a part of
} trace_t;

// trace->entityNum can also be 0 to (MAX_GENTITIES-1)
// or ENTITYNUM_NONE, ENTITYNUM_WORLD

// markfragments are returned by CM_MarkFragments()
typedef struct
{
	int  firstPoint;
	int  numPoints;
} markFragment_t;

typedef struct
{
	vec3_t	origin;
	vec3_t	axis[3];
} orientation_t;

//=====================================================================

// in order from highest priority to lowest
// if none of the catchers are active, bound key strings will be executed
#define KEYCATCH_CONSOLE 0x0001
#define KEYCATCH_UI 	 0x0002
#define KEYCATCH_MESSAGE 0x0004
#define KEYCATCH_CGAME	 0x0008

// sound channels
// channel 0 never willingly overrides
// other channels will allways override a playing sound on that channel
typedef enum
{
	CHAN_AUTO,
	CHAN_LOCAL, 	// menu sounds, etc
	CHAN_WEAPON,
	CHAN_VOICE,
	CHAN_ITEM,
	CHAN_BODY,
	CHAN_LOCAL_SOUND,	// chat messages, etc
	CHAN_ANNOUNCER, 	// announcer voices, etc
	CHAN_FX
} soundChannel_t;

/*
========================================================================

  ELEMENTS COMMUNICATED ACROSS THE NET

========================================================================
*/

#define ANGLE2SHORT(x) ((int)((x) * 65536 / 360) & 65535)
#define SHORT2ANGLE(x) ((x) * (360.0 / 65536))

#define SNAPFLAG_RATE_DELAYED 1
#define SNAPFLAG_NOT_ACTIVE   2 		// snapshot used during connection and for zombies
#define SNAPFLAG_SERVERCOUNT  4 		// toggled every map_restart so transitions can be detected
#define SNAPFLAG_ANTIWARPED   8 		// Is on when you're a warper

//
// per-level limits
//
#define MAX_CLIENTS 64				// absolute limit
#define MAX_LOCATIONS	360			// @r00t: Changed from 256, big maps have many locations

#define GENTITYNUM_BITS 10			// don't need to send any more
#define MAX_GENTITIES	(1 << GENTITYNUM_BITS)

// entitynums are communicated with GENTITY_BITS, so any reserved
// values thatare going to be communcated over the net need to
// also be in this range
#define ENTITYNUM_NONE		 (MAX_GENTITIES - 1)
#define ENTITYNUM_WORLD 	 (MAX_GENTITIES - 2)
#define ENTITYNUM_MAX_NORMAL (MAX_GENTITIES - 2)

#define MAX_MODELS		 256			// these are sent over the net as 8 bits
#define MAX_SOUNDS		 256			// so they cannot be blindly increased
#define MAX_FLAGS		 32
#define MAX_UTEXT		 16

#define MAX_CONFIGSTRINGS	 1024

// these are the only configstrings that the system reserves, all the
// other ones are strictly for servergame to clientgame communication
#define CS_SERVERINFO		   0		// an info string with all the serverinfo cvars
#define CS_SYSTEMINFO		   1		// an info string for server system to client system configuration (timescale, etc)

#define RESERVED_CONFIGSTRINGS 2	// game can't modify below this, only the system can

#define MAX_GAMESTATE_CHARS    16000
typedef struct
{
	int   stringOffsets[MAX_CONFIGSTRINGS];
	char  stringData[MAX_GAMESTATE_CHARS];
	int   dataCount;
} gameState_t;

//=========================================================

// bit field limits
#define MAX_STATS			   16
#define MAX_PERSISTANT		   16
#define MAX_POWERUPS		   16
#define MAX_WEAPONS 		   255

#define UT_MAX_WEAPONS		   16				 // WE DONT USE THIS as a bit field anymore
#define UT_MAX_ITEMS		   16

#define MAX_PS_EVENTS		   2

#define PS_PMOVEFRAMECOUNTBITS 6

/*
typedef struct {

	byte			weaponid;
	byte			bullets;
	byte			mode;
	byte			clips;

} utWeapon_t;
*/

/*
typedef struct {

	byte			itemid;
	byte			flags;
	byte			reserved1;			// Do not set data here.
	byte			reserved2;			// Do not set data here.

} utItem_t;
*/

// playerState_t is the information needed by both the client and server
// to predict player motion and actions
// nothing outside of pmove should modify these, or some degree of prediction error
// will occur

// you can't add anything to this without modifying the code in msg.c

// playerState_t is a full superset of entityState_t as it is used by players,
// so if a playerState_t is transmitted, the entityState_t can be fully derived
// from it.
//
// Original DLL/QVM SIZE: 468 bytes (3744 bits)
// Possible (w/o touching the big arrays): 358 bytes (2858 bits)
//
// ### WARNING: Bit information here is OUTDATED and far from reality!!! ###
typedef struct playerState_s
{
	int commandTime;						// 32bit - 32bit	// cmd->serverTime of last executed command
	int pm_type;							// 32bit - 3bit
	int bobCycle;							// 32bit - 8bit 	// for view bobbing and footstep generation
	int pm_flags;							// 32bit - 16bit	// ducked, jump_held, etc
	int pm_time;							// 32bit - 32bit

	vec3_t	origin; 						// 3 x 32bit - 3 x 16bit
	vec3_t	velocity;						// 3 x 32bit - 3 x 16bit
	int weaponTime; 						// 32bit - 32bit (maybe 16bit)
	int gravity;							// 32bit - 10bit (range: 0 - 1023)
	int speed;								// 32bit - 9bit (range: 0 - 511)
	int delta_angles[3];						// 3 x 32bit - 3 x 16bit	// add to command angles to get view direction
	// changed by spawns, rotating objects, and teleporters

	int  groundEntityNum;						// 32bit - 10bit	// ENTITYNUM_NONE = in air

	int  legsTimer; 							// 32bit - 32bit	// don't change low priority animations until this runs out
	int  legsAnim;								// 32bit - 8bit 	// mask off ANIM_TOGGLEBIT

	int  torsoTimer;							// 32bit - 32bit	// don't change low priority animations until this runs out
	int  torsoAnim; 							// 32bit - 8bit 	// mask off ANIM_TOGGLEBIT

	int  movementDir;							// 32bit - 3bit 	// a number 0 to 7 that represents the reletive angle
	// of movement to the view angle (axial and diagonals)
	// when at rest, the value will remain unchanged
	// used to twist the legs during strafing

	vec3_t	grapplePoint;						// 3 x 32bit - 0bit (not used in UT)	// location of grapple to pull towards if PMF_GRAPPLE_PULL

	int eFlags; 								// 32bit - 20bit	// copied to entityState_t->eFlags

	int eventSequence;							// 32bit - 2bit 	// pmove generated events
	int events[MAX_PS_EVENTS];					// 2 x 32bit - 2 x 10bit (range: 0 - 1023)
	int eventParms[MAX_PS_EVENTS];				// 2 x 32bit - 2 x 32bit

	int externalEvent;							// 32bit - 10bit (range: 0 - 1023)	// events set on player from another source
	int externalEventParm;						// 32bit - 32bit
	int externalEventTime;						// 32bit - 32bit

	int clientNum;								// 32bit - 5bit // ranges from 0 to MAX_CLIENTS-1
	int weaponslot; 							// 32bit - 32bit (for now; untill we decide how we save weap data)	// copied to entityState_t->weapon
	int weaponstate;							// 32bit - 5bit (range: 0 - 18; MAX_WEAPON_ANIMATIONS == 18)

	vec3_t	viewangles; 						// 3 x 32bit - 3 x 9bit (range: 0 - 360 or -180 - 180)	// for fixed views
	int viewheight; 							// 32bit - 8bit

	// damage feedback
	int  damageEvent;							// 32bit - 32bit (possible 16 or 8 bit) // when it changes, latch the other parms
	int  damageYaw; 							// 32bit - 9bit (range: 0 - 360 or -180 - 180)
	int  damagePitch;							// 32bit - 9bit (range: 0 - 360 or -180 - 180)
	int  damageCount;							// 32bit - 32bit (possible 16bit)

	int  stats[MAX_STATS];						// 16 x 32bit - 16 x 32bit
	int  persistant[MAX_PERSISTANT];			// 16 x 32bit - 16 x 32bit	// stats that aren't cleared on death
	int  weaponData[UT_MAX_WEAPONS];			// 16 x 32bit - 16 x 32bit	///UT_WP_NUM_WEAPONS // is not defined so fuckit
	int  itemData[UT_MAX_ITEMS];				// 16 x 32bit - 16 x 32bit

	int  generic1;								// 32bit - 32bit (possible 8bit)
	int  loopSound; 							// 32bit - 32bit (possible 8bit)
	int  jumppad_ent;							// 32bit - 0bit (always empty)	// jumppad entity hit this frame

	// not communicated over the net at all
	// Is this communicated or not??
	int  ping;									// 32bit - 10bit (range: 0 - 1023)	// server to game info for scoreboard
	int  pmove_framecount;						// 32bit - 8bit (not sure maybe more maybe less..) // FIXME: don't transmit over the network
	int  jumppad_frame; 						// 32bit - 8bit (same size as pmove_framecount)
	int  entityEventSequence;					// 32bit - 2bit
} playerState_t;

//====================================================================

//
// usercmd_t->button bits, many of which are generated by the client system,
// so they aren't game/cgame only definitions
//
#define BUTTON_ATTACK		(1 << 0)
#define BUTTON_TALK 		(1 << 1)				// displays talk balloon and disables actions
#define BUTTON_USE_HOLDABLE (1 << 2)
#define BUTTON_WEAPON_MODE	(1 << 3)
#define BUTTON_WALKING		(1 << 4)			// walking can't just be infered from MOVE_RUN
// because a key pressed late in the frame will
// only generate a small move value for that frame
// walking will use different animations and
// won't generate footsteps
#define BUTTON_RELOAD		(1 << 5)
#define BUTTON_BANDAGE		(1 << 6)

#define BUTTON_USE			(1 << 7)
#define BUTTON_SPRINT		(1 << 8)
#define BUTTON_FOLLOWME 	(1 << 10)
#define BUTTON_MINIMAP		(1 << 12)	// was DEFUSE, but not used! :D
// use zoom buttons as a 3-bit number (allows up to 8 zoom levels)
// so server knows if player is zoomed
#define BUTTON_ANY			(1 << 11)					 // any key whatsoever

/*#define BUTTON_ZOOM1		(1<<12)
#define BUTTON_ZOOM2		(1<<13)
#define BUTTON_ZOOM3		(1<<14) */

#define BUTTON_ZOOM1		(1 << 13)
#define BUTTON_ZOOM2		(1 << 14)

#define MOVE_RUN	 120					// if forwardmove or rightmove are >= MOVE_RUN,
// then BUTTON_WALKING should be set

// usercmd_t is sent to the server each client frame
typedef struct usercmd_s
{
	int 		 serverTime;
	int 		 angles[3];
	int 		 buttons;
	byte		 weapon;			  // weapon
	signed char  forwardmove, rightmove, upmove;
} usercmd_t;

//===================================================================

// if entityState->solid == SOLID_BMODEL, modelindex is an inline model number
#define SOLID_BMODEL 0xffffff

typedef enum
{
	TR_STATIONARY,
	TR_INTERPOLATE, 			// non-parametric, but interpolate between snapshots
	TR_LINEAR,
	TR_LINEAR_STOP,
	TR_SINE,					// value = base + sin( time / duration ) * delta
	TR_GRAVITY,

	// Urban Terror
	TR_GRAVITY2,
	TR_WATER,
	TR_FEATHER,
	TR_EJECTA,
	TR_EJECTA2,
	TR_SNOWFLAKE
} trType_t;

typedef struct
{
	trType_t  trType;
	int 	  trTime;
	int 	  trDuration;				// if non 0, trTime + trDuration = stop time
	vec3_t	  trBase;
	vec3_t	  trDelta;			// velocity, etc
} trajectory_t;

// entityState_t is the information conveyed from the server
// in an update message about entities that the client will
// need to render in some way
// Different eTypes may use the information in different ways
// The messages are delta compressed, so it doesn't really matter if
// the structure size is fairly large

typedef struct entityState_s
{
	int 		  number;			// entity index
	int 		  eType;			// entityType_t
	int 		  eFlags;

	trajectory_t  pos;	// for calculating position
	trajectory_t  apos; // for calculating angles

	int 		  time;
	int 		  time2;

	vec3_t		  origin;
	vec3_t		  origin2;

	vec3_t		  angles;
	vec3_t		  angles2;

	int 		  otherEntityNum;	// shotgun sources, etc
	int 		  otherEntityNum2;

	int 		  groundEntityNum;		// -1 = in air

	int 		  constantLight;		// r + (g<<8) + (b<<16) + (intensity<<24)
	int 		  loopSound;		// constantly loop this sound

	int 		  modelindex;
	int 		  modelindex2;
	int 		  clientNum;		// 0 to (MAX_CLIENTS - 1), for players and corpses
	int 		  frame;

	int 		  solid;			// for client side prediction, trap_linkentity sets this properly

	int 		  event;			// impulse events -- muzzle flashes, footsteps, etc
	int 		  eventParm;

	// for players
	int 		  powerups; 			 // bit flags
	int 		  weapon;				 // determines weapon and flash model, etc
	int 		  legsAnim; 			 // mask off ANIM_TOGGLEBIT
	int 		  torsoAnim;			 // mask off ANIM_TOGGLEBIT

	int 		  generic1;
} entityState_t;

typedef enum
{
	CA_UNINITIALIZED,
	CA_DISCONNECTED,		// not talking to a server
	CA_AUTHORIZING, 		// not used any more, was checking cd key
	CA_CONNECTING,			// sending request packets to the server
	CA_CHALLENGING, 		// sending challenge packets to the server
	CA_CONNECTED,			// netchan_t established, getting gamestate
	CA_LOADING, 			// only during cgame initialization, never during main loop
	CA_PRIMED,				// got gamestate, waiting for first frame
	CA_ACTIVE,				// game views should be displayed
	CA_CINEMATIC			// playing a cinematic or a static pic, not connected to a server
} connstate_t;

// font support

#define GLYPH_START 0
#define GLYPH_END	255
#define GLYPH_CHARSTART 32
#define GLYPH_CHAREND	127
#define GLYPHS_PER_FONT GLYPH_END - GLYPH_START + 1
typedef struct
{
	int 	   height;		   // number of scan lines
	int 	   top; 		   // top of glyph in buffer
	int 	   bottom;		   // bottom of glyph in buffer
	int 	   pitch;		   // width for copying
	int 	   xSkip;		   // x adjustment
	int 	   imageWidth;	   // width of actual image
	int 	   imageHeight;    // height of actual image
	float	   s;			   // x offset in image where glyph starts
	float	   t;			   // y offset in image where glyph starts
	float	   s2;
	float	   t2;
	qhandle_t  glyph;		   // handle to the shader with the glyph
	char	   shaderName[32];
} glyphInfo_t;

typedef struct
{
	glyphInfo_t  glyphs [GLYPHS_PER_FONT];
	float		 glyphScale;
	char		 name[MAX_QPATH];
} fontInfo_t;

#define Square(x) ((x) * (x))

// real time
//=============================================

typedef struct qtime_s
{
	int  tm_sec;	/* seconds after the minute - [0,59] */
	int  tm_min;	/* minutes after the hour - [0,59] */
	int  tm_hour;	/* hours since midnight - [0,23] */
	int  tm_mday;	/* day of the month - [1,31] */
	int  tm_mon;	/* months since January - [0,11] */
	int  tm_year;	/* years since 1900 */
	int  tm_wday;	/* days since Sunday - [0,6] */
	int  tm_yday;	/* days since January 1 - [0,365] */
	int  tm_isdst;	/* daylight savings time flag */
} qtime_t;

// server browser sources
#define AS_LOCAL	 0
#define AS_MPLAYER	 1
#define AS_GLOBAL	 2
#define AS_FAVORITES 3

// cinematic states
typedef enum
{
	FMV_IDLE,
	FMV_PLAY,		// play
	FMV_EOF,		// all other conditions, i.e. stop/EOF/abort
	FMV_ID_BLT,
	FMV_ID_IDLE,
	FMV_LOOPED,
	FMV_ID_WAIT
} e_status;

typedef enum _flag_status
{
	FLAG_ATBASE = 0,
	FLAG_TAKEN, 		// CTF
	FLAG_TAKEN_RED, 	// One Flag CTF
	FLAG_TAKEN_BLUE,	// One Flag CTF
	FLAG_DROPPED
} flagStatus_t;

typedef enum {
	DS_NONE,

	DS_PLAYBACK,
	DS_RECORDING,

	DS_NUM_DEMO_STATES
} demoState_t;

#define MAX_GLOBAL_SERVERS			4096
#define MAX_OTHER_SERVERS			128
#define MAX_PINGREQUESTS			32
#define MAX_SERVERSTATUSREQUESTS	16

#define SAY_ALL 		 0
#define SAY_TEAM		 1
#define SAY_TELL		 2
#define SAY_NPC 		 3

#define CDKEY_LEN		 16
#define CDCHKSUM_LEN	 2

#define UT_StdRand(a, b) ((a) + random() * (b))


#define utTestIntersectionInl(_vert,_orig,_dir,_u,_v,_t ) {\
	vec3_t	_edge[2];\
	vec3_t	_pvec;\
	vec3_t	_tvec;\
	vec3_t	_qvec;\
	float	_det;\
	float	_inv_det;\
	VectorSubtract ( _vert[1], _vert[0], _edge[0]);\
	VectorSubtract ( _vert[2], _vert[0], _edge[1]);\
	CrossProduct ( _dir, _edge[1], _pvec );\
	_det = DotProduct ( _edge[0], _pvec );\
	if ((_det <= -0.000001) || (_det >= 0.000001))\
	{\
	 _inv_det = 1.0 / _det;\
	 VectorSubtract ( _orig, _vert[0], _tvec );\
	 _u = DotProduct(_tvec, _pvec) * _inv_det;\
	 if ((_u >= 0.0) && (_u <= 1.0))\
	 {\
	  CrossProduct ( _tvec, _edge[0], _qvec );\
	  _v = DotProduct ( _dir, _qvec ) * _inv_det;\
	  if ((_v >= 0.0) && (_u + _v <= 1.0))\
	  {\
	  _t = DotProduct (_edge[1], _qvec ) * _inv_det;


//int		utTestIntersection		( vec3_t vert[3], vec3_t orig, vec3_t dir, float *u, float *v, float *t );
// DensitY function - Gah gah :p
float utSquaredDistance3dSegment( vec3_t SegPoint1, vec3_t SegPoint2, vec3_t Origin );

void  AnglesAddDrift		  ( vec3_t in, int time, int maxtime, int offset, int maxoffset, float scale, vec3_t out );
void  AnglesAddSpreads		  ( vec3_t in, int spread, int recoil, float rise, vec3_t out );

#define DMGL_NONE		  0
#define DMGL_LEGS		  (1 << 0)
#define DMGL_STOMACH	  (1 << 1)
#define DMGL_CHEST		  (1 << 2)
#define DMGL_NECK		  (1 << 3)
#define DMGL_HEAD		  (1 << 4)

#define UT_ITEMFLAG_ON	  (1 << 0)

#define MAX_CHARCONFIG_COMPRESSED 100
#define MAX_CHARCONFIG_NAME 	  25


//1, 2,4,8,16

void   M4Mul(float mat[4][4], float m[4][4], float out[4][4]) ;
int    M4Inv(float mat[16], float DEST[16]);
float  m3_det( float mat[9] );
void   m4_submat( float mr[16], float mb[9], int i, int j );
float  m4_det( float mr[16] );
void   M4Trans(float mat[4][4], vec3_t v, vec3_t out);

char *OnOff(int val);

// func_ut_train definitions
#define MAX_TRAINS 4
#define MAX_STOPS  5

#endif	// __Q_SHARED_H
