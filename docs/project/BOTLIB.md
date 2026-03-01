# Botlib — AI Library

> Covers all files in `code/botlib/`. The botlib is a self-contained AI library that provides navigation (AAS area graph) and behavior trees for Q3 bots.

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [be_interface.c — Entry Point & Export Table](#be_interfacec--entry-point--export-table)
3. [AAS — Area Awareness System](#aas--area-awareness-system)
4. [Bot AI Behaviors (be_ai_*)](#bot-ai-behaviors-be_ai_)
5. [Elementary Actions (be_ea.c)](#elementary-actions-be_eac)
6. [Support Libraries (l_*.c)](#support-libraries-l_c)
7. [Key Data Structures](#key-data-structures)
8. [Botlib ↔ Engine Interface](#botlib--engine-interface)

---

## Architecture Overview

```
QVM game code
   │ trap_BotLib*  (via G_BOT_* syscalls)
   ▼
sv_game.c → SV_GameSystemCalls (botlib pass-through)
   │
   ▼
botlib_export_t *botlib_export   ← function table from GetBotLibAPI()
   │
   ├── AAS (Area Awareness System)   ← pathfinding, navigation graph
   │     be_aas_main.c
   │     be_aas_route.c             ← A* routing
   │     be_aas_reach.c             ← reachability analysis
   │     be_aas_cluster.c           ← cluster/portal graph
   │     be_aas_sample.c            ← area sampling (random placement)
   │     be_aas_entity.c            ← entity tracking in AAS
   │     be_aas_move.c              ← movement prediction
   │     be_aas_bspq3.c             ← BSP→AAS bridge
   │     be_aas_file.c              ← .aas file load/save
   │     be_aas_optimize.c          ← simplify AAS graph
   │
   ├── Bot AI (behavior layer)
   │     be_ai_move.c               ← movement decisions
   │     be_ai_goal.c               ← goal selection
   │     be_ai_weap.c               ← weapon selection and aim
   │     be_ai_chat.c               ← chat responses (taunts, commands)
   │     be_ai_char.c               ← personality characteristics
   │     be_ai_gen.c                ← generic utility AI functions
   │     be_ai_weight.c             ← fuzzy weight evaluation
   │
   └── Elementary Actions
         be_ea.c                    ← low-level move commands to engine
```

The botlib is initialized at server startup and persists across map loads. Each bot is a `client_t` slot with `netchan.remoteAddress.type = NA_BOT`.

---

## be_interface.c — Entry Point & Export Table

**File:** `code/botlib/be_interface.c`

### GetBotLibAPI(apiVersion, import)

The single exported symbol. Returns `botlib_export_t*` containing all bot functions:

```c
botlib_export_t *GetBotLibAPI(int apiVersion, botlib_import_t *import) {
    assert(import != NULL);
    botimport = *import;                // save engine callback table
    botlib_export.BotLibSetup = Export_BotLibSetup;
    botlib_export.BotLibShutdown = Export_BotLibShutdown;
    botlib_export.BotLibVarSet = Export_BotLibVarSet;
    // ... fill all exports ...
    return &botlib_export;
}
```

### Export_BotLibSetup()

```
BotLibSetup:
    AAS_Setup()           ← init AAS subsystem
    EA_Setup()            ← init elementary actions
    BotSetupMoveAI()      ← init movement AI
    BotSetupGoalAI()      ← init goal AI
    BotSetupWeaponAI()    ← init weapon AI
    BotSetupChatAI()      ← init chat system (load chat files from botfiles/)
```

### Export_BotLibStartFrame(time)

Called every game frame:
- Updates `botlibglobals.time`
- AAS_StartFrame → updates entity positions in AAS
- Updates dynamic entity tracking

### Export_BotLibLoadMap(mapname)

```
BotLibLoadMap:
    AAS_LoadAASFile(mapname)   ← load precomputed .aas file
    or
    AAS_GenerateAASFile()      ← generate from BSP (slow, usually precomputed)
    AAS_InitAASLinkHeap()
    AAS_InitClusters()
    AAS_InitPortals()
```

---

## AAS — Area Awareness System

The AAS is a graph of convex areas that covers all navigable space. Unlike BSP which is a geometry tree, AAS describes connectivity: which areas you can reach from which other areas.

### .aas File Format

Pre-compiled navigation data stored in `maps/mapname.aas` (inside game PK3). Contains:
- **Areas** — convex navigable regions with bounding boxes
- **Faces** — boundaries between areas
- **Edges** — edges of faces
- **Reachabilities** — explicit movement paths between areas (walk, jump, teleport, elevator, etc.)
- **Clusters** — groups of nearby areas for fast routing
- **Portals** — connections between clusters

### be_aas_route.c — Pathfinding

**File:** `code/botlib/be_aas_route.c`  
**Size:** ~2222 lines — largest botlib file

#### AAS_AreaRouteToGoalArea(fromArea, fromOrigin, toArea, travelflags, traveltime, reachnum)

Dijkstra/A*-style shortest path through the AAS graph. Returns:
- `traveltime` — estimated time to reach goal (in milliseconds)
- `reachnum` — index of first reachability to take

Uses precomputed routing tables (`aasworld.areatraveltimes`) cached per cluster for O(1) lookup for most same-cluster routes.

#### Travel Flags

```c
TFL_WALK        = 0x0001  // normal walking
TFL_CROUCH      = 0x0002  // crouch walk
TFL_BARRIERJUMP = 0x0004  // jump over barrier
TFL_JUMP        = 0x0008  // regular jump
TFL_LADDER      = 0x0010  // climb ladder
TFL_WALKOFFLEDGE= 0x0020  // walk off edge (fall)
TFL_SWIM        = 0x0040  // swimming
TFL_WATERJUMP   = 0x0080  // jump out of water
TFL_TELEPORT    = 0x0100  // teleporter
TFL_ELEVATOR    = 0x0200  // elevator/platform
TFL_ROCKETJUMP  = 0x0400  // rocket jump
TFL_BFGJUMP     = 0x0800  // BFG jump
TFL_GRAPPLEHOOK = 0x1000  // grapple hook
TFL_DOUBLEJUMP  = 0x2000  // double jump (future)
TFL_RAMPJUMP    = 0x4000  // ramp jump
TFL_STRAFEJUMP  = 0x8000  // strafe jump
TFL_JUMPPAD     = 0x010000 // jump pad
TFL_FUNCBOB     = 0x020000 // moving platform
TFL_FLIGHT      = 0x040000 // flight powerup
TFL_AIR         = 0x050000 // in air
TFL_WATER       = 0x060000 // in water
TFL_SLIME       = 0x080000 // in slime
TFL_LAVA        = 0x100000 // in lava
```

### be_aas_reach.c — Reachability Analysis

**File:** `code/botlib/be_aas_reach.c`  
**Size:** ~4544 lines — largest single file in the project

Pre-computes all reachabilities between AAS areas:
- `AAS_CalculateReachabilities()` — main function, called once per map
- For each pair of adjacent areas: tests if a bot can get from one to the other
- Creates a `aas_reachability_t` record for each valid connection

Key reachability types:
- `TRAVEL_WALK` — flat ground connection
- `TRAVEL_CROUCH` — must crouch to pass
- `TRAVEL_BARRIERJUMP` — small hop over barrier
- `TRAVEL_JUMP` — horizontal jump required
- `TRAVEL_TELEPORT` — teleporter
- `TRAVEL_ELEVATOR` — func_plat / func_door elevator
- `TRAVEL_ROCKETJUMP` — requires rocket jump
- `TRAVEL_JUMPPAD` — jump pad launch

### be_aas_cluster.c — Cluster Graph

Divides the map into clusters (groups of nearby areas). Portal areas connect clusters. Used by routing for two-level path search:
1. Coarse: cluster-to-cluster path
2. Fine: area-to-area within cluster

### be_aas_sample.c — Area Sampling

`AAS_PointAreaNum(origin)` — find which AAS area contains a 3D point.

`AAS_NearestGoalAreaNum(origin, travelflags)` — find closest accessible area.

`AAS_RandomGoalArea(startArea, travelflags, goalArea, goalOrigin)` — pick random navigable destination (used for wandering behavior).

### be_aas_entity.c — Entity Tracking

Keeps AAS up-to-date with moving entities:
- Doors: updates blocked areas when door closes
- Elevators: tracks which position the elevator is at
- `AAS_UpdateEntity(entnum, info)` — called each frame for each entity

---

## Bot AI Behaviors (be_ai_*)

### be_ai_move.c — Movement

**File:** `code/botlib/be_ai_move.c`  
**Size:** ~3560 lines

High-level movement toward a goal:

`BotMoveToGoal(moveresult, movestate, goal, travelflags)`:
```
1. Get route: AAS_AreaRouteToGoalArea(botAreaNum, origin, goal.area, ...)
2. Get next reachability from route
3. Select movement strategy based on reachability type:
   - TRAVEL_WALK: BotTravel_Walk → issue EA_Move toward waypoint
   - TRAVEL_JUMP: BotTravel_Jump → crouch + look at landing + EA_Jump
   - TRAVEL_TELEPORT: BotTravel_Teleport → walk toward teleport trigger
   - TRAVEL_ELEVATOR: BotTravel_Elevator → wait for platform, ride it
   - TRAVEL_ROCKETJUMP: BotTravel_RocketJump → fire rocket at feet + jump
   - TRAVEL_JUMPPAD: BotTravel_JumpPad → walk onto pad, let it launch
4. Handle obstacles: BotCheckBlocked → check if stuck, trigger unstuck
```

`BotPredictVisiblePosition(origin, areanum, goal, travelflags, target)`:
Predicts where bot will be in N ms to choose when to shoot.

### be_ai_goal.c — Goal Selection

**File:** `code/botlib/be_ai_goal.c`  
**Size:** ~1830 lines

Evaluates and selects goals from:
- Level items (weapons, health, armor, ammo)
- Enemy players
- Game objectives (flags, keys)

`BotChooseLTGTactic(goal)` — long-term goal selection:
- Score items by fuzzy weight (value × distance attenuation)
- `BotGoalName(goalnum, name, size)` → item name for debug

`BotGetItemGoal(origin, areanum, travelflags, goaltype)`:
Finds best item goal within reachable distance considering travel time.

### be_ai_weap.c — Weapon Selection

Selects optimal weapon based on:
- Distance to target
- Ammo available
- Weapon characteristics from `botfiles/weapons.c`

`BotChooseBestFightWeapon(weaponstate, inventory)` → weapon number.

### be_ai_chat.c — Chat System

**File:** `code/botlib/be_ai_chat.c`  
**Size:** ~3121 lines

Parses `botfiles/match.c` (chat pattern matching) and generates context-appropriate chat responses:

`BotEnterChat(chatstate, client, chattype)` — enter a chat for:
- `CHAT_ENTER` — joining game
- `CHAT_EXIT` — leaving game  
- `CHAT_KILL` — after killing someone
- `CHAT_DEATH` — after dying
- `CHAT_TAUNT` — taunting

Uses pattern matching to respond to specific player chat messages.

### be_ai_char.c — Bot Personality

Loads bot character files from `botfiles/bots/*.c`. Each character defines:
- `skill` — 1-5 (difficulty)
- `aggression` — how aggressive
- `camp` — tendency to camp
- `reaction_time` — aim reaction
- `weapon_weight_file` — which weapon rankings to use
- `chat_file` — which chat scripts to use

---

## Elementary Actions (be_ea.c)

**File:** `code/botlib/be_ea.c`

Low-level bot input generation. The EA (Elementary Action) system translates high-level bot intentions into `usercmd_t`-like inputs:

| Function | Description |
|---|---|
| `EA_Say(client, str)` | Issue say command |
| `EA_SayTeam(client, str)` | Say to team |
| `EA_Command(client, cmd)` | Issue console command |
| `EA_Action(client, action)` | Set action buttons (BUTTON_ATTACK, etc.) |
| `EA_Gesture(client)` | Wave/gesture |
| `EA_Talk(client)` | Toggle talk button |
| `EA_Attack(client)` | Fire weapon |
| `EA_Respawn(client)` | Respawn after death |
| `EA_Crouch(client)` | Crouch |
| `EA_MoveUp(client)` | Jump/swim up |
| `EA_MoveDown(client)` | Swim down |
| `EA_MoveForward(client)` | Move forward |
| `EA_MoveBack(client)` | Move back |
| `EA_MoveLeft(client)` | Move left |
| `EA_MoveRight(client)` | Move right |
| `EA_Move(client, dir, speed)` | Move in direction at speed |
| `EA_View(client, viewangles)` | Set view angles |
| `EA_GetInput(client, time, input)` | Retrieve accumulated input → `bot_input_t` |
| `EA_ResetInput(client)` | Clear accumulated input |

`EA_GetInput` is called by the game module to get the final `bot_input_t` which is then converted to a `usercmd_t` and passed to `SV_ClientThink`.

---

## Support Libraries (l_*.c)

| File | Purpose |
|---|---|
| `l_memory.c` | Bot-specific memory management (wraps `Z_TagMalloc(TAG_BOTLIB)`) |
| `l_log.c` | Bot debug logging to file |
| `l_crc.c` | CRC32 for botfile integrity checks |
| `l_script.c` | Script parser (~1443 lines) — reads botfiles/*.c scripts |
| `l_precomp.c` | Preprocessor for bot scripts (handles #define, #include, #if) |
| `l_struct.c` | Struct serialization for bot config files |
| `l_libvar.c` | Botlib-internal variable system (separate from engine cvars) |

### Script Parser (l_script.c / l_precomp.c)

Botlib ships with a mini C-preprocessor and tokenizer to parse its AI definition files:
- `botfiles/bots/*.c` — bot character definitions
- `botfiles/match.c` — chat pattern matching
- `botfiles/weapons.c` — weapon characteristics
- `botfiles/items.c` — item importance weights

---

## Key Data Structures

### aas_world_t aasworld — AAS Navigation Data

```c
typedef struct aas_s {
    int             loaded;
    int             initialized;
    float           time;
    int             numAreas;       aas_area_t      *areas;
    int             numFaces;       aas_face_t      *faces;
    int             numEdges;       aas_edge_t      *edges;
    int             numVertices;    aas_vertex_t    *vertices;
    int             numReachabilities; aas_reachability_t *reachability;
    int             numClusters;    aas_cluster_t   *clusters;
    int             numPortals;     aas_portal_t    *portals;
    // routing tables (precomputed shortest paths)
    unsigned char   **areatraveltimes;  // [numAreas][numAreas] travel time
    int             *portalTravelTimes;
    // entities
    aas_entityinfo_t *entityinfo;   // per-entity AAS state
} aas_t;
```

### aas_reachability_t — Movement Path

```c
typedef struct aas_reachability_s {
    int     areanum;       // start area
    int     facenum;       // face on boundary of areas
    int     edgenum;       // edge we pass through
    vec3_t  start;         // start of movement
    vec3_t  end;           // end of movement (in next area)
    int     traveltype;    // TRAVEL_* flags
    unsigned short traveltime; // time to traverse (ms)
} aas_reachability_t;
```

### bot_moveresult_t — Movement Output

```c
typedef struct bot_moveresult_s {
    int         failure;           // movement failed
    int         type;              // type of movement
    int         blocked;           // bot is blocked
    int         blockentity;       // entity blocking bot
    int         traveltype;        // current reachability travel type
    int         flags;             // movement flags
} bot_moveresult_t;
```

### bot_input_t — Bot Action State

```c
typedef struct bot_input_s {
    float       thinktime;     // time the bot has been thinking
    vec3_t      dir;           // move direction
    float       speed;         // movement speed (0-400)
    vec3_t      viewangles;    // desired view angles
    int         actionflags;   // BUTTON_ATTACK, BUTTON_WALKING, etc.
    int         weapon;        // desired weapon
} bot_input_t;
```

---

## Botlib ↔ Engine Interface

### botlib_import_t (engine → botlib)

Functions the botlib calls back into the engine:

```c
typedef struct {
    void    (*Print)(int type, char *fmt, ...);      // Com_Printf
    void    (*Trace)(bsp_trace_t*, vec3_t, ...);     // SV_Trace
    int     (*PointContents)(vec3_t);                // SV_PointContents
    int     (*inPVS)(vec3_t, vec3_t);                // SV_inPVS
    void    (*BSPModelMinsMaxsOrigin)(...);
    void    *(*GetMemory)(int size);                 // Z_TagMalloc(TAG_BOTLIB)
    void    (*FreeMemory)(void *ptr);                // Z_Free
    void    *(*HunkAlloc)(int size);                 // Hunk_Alloc
    int     (*FS_FOpenFile)(...);                    // FS_FOpenFileRead
    int     (*FS_Read)(...);                         // FS_Read
    void    (*FS_FCloseFile)(...);                   // FS_FCloseFile
    int     (*FS_Seek)(...);
    int     (*FS_AvailableSpace)(void);
    void    (*FS_WriteFile)(...);
    // debug visualization:
    int     (*DebugPolygonCreate)(...);
    void    (*DebugPolygonDelete)(int);
    // console:
    int     (*Cvar_Set)(const char*, const char*);
    int     (*Cvar_VariableIntegerValue)(const char*);
    void    (*Cvar_VariableStringBuffer)(...);
    // time:
    int     (*Milliseconds)(void);
} botlib_import_t;
```

### botlib_export_t (botlib → engine)

The engine calls these through `botlib_export`:

| Category | Key Functions |
|---|---|
| **Setup** | `BotLibSetup`, `BotLibShutdown`, `BotLibStartFrame`, `BotLibLoadMap` |
| **AAS** | `AAS_EnableRoutingCache`, `AAS_SetAASBlockingEntity`, `AAS_PointAreaNum`, `AAS_AreaInfo`, `AAS_AltRouteGoals`, `AAS_PredictClientMovement`, `AAS_Swimming`, `AAS_PointContents` |
| **AAS Route** | `AAS_AreaRouteToGoalArea`, `AAS_BestReachableArea`, `AAS_ReachabilityIndex`, `AAS_TraceAreas` |
| **EA** | `EA_Say`, `EA_Attack`, `EA_Crouch`, `EA_Jump`, `EA_Move`, `EA_View`, `EA_GetInput`, `EA_ResetInput` |
| **AI Move** | `BotAllocMoveState`, `BotFreeMoveState`, `BotMoveToGoal`, `BotMoveInDirection`, `BotResetMoveState`, `BotMovementViewTarget` |
| **AI Goal** | `BotAllocGoalState`, `BotFreeGoalState`, `BotChooseLTGTactic`, `BotChooseNBGItem`, `BotGetItemGoal`, `BotNextCampSpotGoal` |
| **AI Weap** | `BotAllocWeaponState`, `BotFreeWeaponState`, `BotChooseBestFightWeapon`, `BotGetWeaponInfo`, `BotLoadWeaponWeights` |
| **AI Chat** | `BotAllocChatState`, `BotFreeChatState`, `BotEnterChat`, `BotInitialChat`, `BotChatLength`, `BotNumInitialChats` |
| **AI Char** | `BotAllocCharacter`, `BotFreeCharacter`, `Characteristic_Float`, `Characteristic_BFloat`, `Characteristic_Integer` |
