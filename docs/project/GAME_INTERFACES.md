# Game Interfaces — QVM Syscall Tables

> Covers `code/game/g_public.h` (engine↔game), `code/cgame/cg_public.h` (engine↔cgame), `code/game/bg_public.h` (shared types), and `code/ui/ui_public.h` (engine↔ui).
>
> These headers define the complete contract between the engine and each QVM module. The engine side is in `sv_game.c` (for game) and `cl_cgame.c`/`cl_ui.c` (for cgame/ui).

---

## Table of Contents

1. [How Syscalls Work](#how-syscalls-work)
2. [g_public.h — Game Module Interface](#g_publich--game-module-interface)
3. [cg_public.h — cgame Module Interface](#cg_publich--cgame-module-interface)
4. [ui_public.h — UI Module Interface](#ui_publich--ui-module-interface)
5. [bg_public.h — Shared Game Types](#bg_publich--shared-game-types)
6. [Key Shared Types](#key-shared-types)

---

## How Syscalls Work

When QVM code calls a negative function number, the VM dispatch calls the registered syscall handler. The pattern:

**QVM → Engine (trap calls):**
```c
// In game QVM code:
trap_Trace(results, start, mins, maxs, end, passEnt, mask);
// compiles to QVM: OP_CALL with callnum = -G_TRACE
// → engine: SV_GameSystemCalls(args) case G_TRACE:
```

**Engine → QVM (entry points):**
```c
// Engine calls:
VM_Call(gvm, 1, GAME_RUN_FRAME, sv.gameTime)
// → QVM receives: vmMain(GAME_RUN_FRAME, sv.gameTime)
```

All pointer arguments from QVM to engine are validated with `VM_CHECKBOUNDS(vm, ptr, size)` before use — prevents QVM from reading/writing outside its data segment.

---

## g_public.h — Game Module Interface

### Engine → Game (Entry Points)

Called by the engine via `VM_Call(gvm, N, callnum, ...)`:

| Callnum | Engine Usage | Arguments |
|---|---|---|
| `GAME_INIT` | `SV_InitGameProgs()` | `levelTime, randomSeed, restart` |
| `GAME_SHUTDOWN` | `SV_ShutdownGameProgs()` | `restart` |
| `GAME_CLIENT_CONNECT` | Client connects | `clientNum, firstTime, isBot` |
| `GAME_CLIENT_BEGIN` | Client enters world | `clientNum` |
| `GAME_CLIENT_USERINFO_CHANGED` | Userinfo updated | `clientNum` |
| `GAME_CLIENT_DISCONNECT` | Client disconnects | `clientNum` |
| `GAME_CLIENT_COMMAND` | Client console command | `clientNum` |
| `GAME_CLIENT_THINK` | Per-tick client update | `clientNum` |
| `GAME_RUN_FRAME` | Per-game-frame update | `levelTime` |
| `GAME_CONSOLE_COMMAND` | Server console command | — |

**GAME_INIT `restart` parameter:** `0` = fresh start, `1` = map_restart. The QVM uses this to preserve scores on restart.

### Game → Engine (Trap Calls)

Called by game QVM as trap_* functions. Handled in `sv_game.c:SV_GameSystemCalls`:

**Output / Communication:**

| Call | Handler | Notes |
|---|---|---|
| `G_PRINT(str)` | `Com_Printf` | Print to server console |
| `G_ERROR(str)` | `Com_Error(ERR_DROP)` | Fatal game error |
| `G_SEND_SERVER_COMMAND(clientNum, text)` | `SV_GameSendServerCommand` | Reliable text to client(s) (-1=all) |
| `G_SET_CONFIGSTRING(index, val)` | `SV_SetConfigstring` | Update configstring → push to clients |
| `G_GET_CONFIGSTRING(index, buf, size)` | Copy from `sv.configstrings` | Read a configstring |
| `G_DROP_CLIENT(clientNum, reason)` | `SV_GameDropClient` | Kick a client |

**Time:**

| Call | Handler | Notes |
|---|---|---|
| `G_MILLISECONDS` | `Sys_Milliseconds()` | Wall-clock ms (not game time) |

**Cvars:**

| Call | Handler | Notes |
|---|---|---|
| `G_CVAR_REGISTER(vmCvar, name, default, flags)` | `Cvar_Register(priv=CVAR_PRIVATE)` | Create or find cvar |
| `G_CVAR_UPDATE(vmCvar)` | `Cvar_Update(priv=CVAR_PRIVATE)` | Sync vmCvar_t from engine |
| `G_CVAR_SET(name, value)` | `Cvar_SetSafe` | Set cvar (CVAR_PROTECTED blocks) |
| `G_CVAR_VARIABLE_INTEGER_VALUE(name)` | `Cvar_VariableIntegerValue` | Quick int read |
| `G_CVAR_VARIABLE_STRING_BUFFER(name, buf, size)` | `Cvar_VariableStringBufferSafe` | Quick string read |

**Command / Args:**

| Call | Handler | Notes |
|---|---|---|
| `G_ARGC` | `Cmd_Argc()` | Number of args from current command |
| `G_ARGV(n, buf, size)` | `Cmd_ArgvBuffer` | Get argument n |
| `G_SEND_CONSOLE_COMMAND(when, text)` | `Cbuf_ExecuteText` | Queue console text |

**Entity Management:**

| Call | Handler | Notes |
|---|---|---|
| `G_LOCATE_GAME_DATA(ents, num, sizeEnt, clients, sizeClient)` | `SV_LocateGameData` | Called once in GAME_INIT |
| `G_SET_BRUSH_MODEL(ent, name)` | `SV_SetBrushModel` | Assign inline model to entity |
| `G_LINKENTITY(ent)` | `SV_LinkEntity` | Register entity in world spatial index |
| `G_UNLINKENTITY(ent)` | `SV_UnlinkEntity` | Remove from spatial index |
| `G_AREA_ENTITIES(mins, maxs, list, maxcount)` | `SV_AreaEntities` | Entities in AABB |
| `G_ADJUST_AREA_PORTAL_STATE(ent, open)` | `CM_AdjustAreaPortalState` | Door open/close |
| `G_AREAS_CONNECTED(a1, a2)` | `CM_AreasConnected` | Check portal connectivity |
| `G_IN_PVS(p1, p2)` | `SV_inPVS` | Both points visible to each other? |
| `G_IN_PVS_IGNORE_PORTALS(p1, p2)` | `SV_inPVS` w/ ignore flag | Same but ignore area portals |

**Physics / Collision:**

| Call | Handler | Notes |
|---|---|---|
| `G_TRACE(results, start, mins, maxs, end, passEnt, mask)` | **`SV_Antilag_InterceptTrace`** or `SV_Trace` | **INTERCEPTED by antilag** |
| `G_TRACECAPSULE(...)` | Same as G_TRACE but capsule | Used for player traces |
| `G_POINT_CONTENTS(point, passEnt)` | `SV_PointContents` | Content flags at point |

**Userinfo / Client:**

| Call | Handler | Notes |
|---|---|---|
| `G_GET_USERINFO(clientNum, buf, size)` | Copy `cl->userinfo` | Client's info string |
| `G_SET_USERINFO(clientNum, buf)` | `SV_SetUserinfo` + `SV_UserinfoChanged` | Update and re-process |
| `G_GET_USERCMD(clientNum, cmd)` | Copy `cl->lastUsercmd` | Client's last movement command |
| `G_GET_ENTITY_TOKEN(buf, size)` | `COM_Parse` from entity string | Parse map entity lump |
| `G_GET_SNAPSHOT_ENTITY(snapNum, entNum)` | Read from snapshot | Entity state for given snapshot |

**File System:**

| Call | Handler | Notes |
|---|---|---|
| `G_FS_FOPEN_FILE(path, &handle, mode)` | `FS_VM_OpenFile(H_QAGAME)` | Sandboxed file open |
| `G_FS_READ(buf, len, handle)` | `FS_VM_ReadFile` | File read |
| `G_FS_WRITE(buf, len, handle)` | `FS_VM_WriteFile` | File write |
| `G_FS_FCLOSE_FILE(handle)` | `FS_VM_CloseFile` | File close |
| `G_FS_GETFILELIST(path, ext, buf, size)` | `FS_GetFileList` | Directory listing |
| `G_FS_SEEK(handle, offset, origin)` | `FS_VM_SeekFile` | File seek |

**Bot Library:**

| Call | Handler | Notes |
|---|---|---|
| `G_BOT_ALLOCATE_CLIENT` | `SV_BotAllocateClient()` | Create a bot client slot |
| `G_BOT_FREE_CLIENT(num)` | `SV_BotFreeClient()` | Free bot slot |
| Various `G_BOT_LIBRARY_*` | Pass-through to `botlib_export` | All bot functions |

### sharedEntity_t — Entity Memory Layout

The game module uses `gentity_t` (defined in game code) which begins with `entityShared_t`:

```c
typedef struct {
    entityState_t   s;             // communicated over network
    qboolean        linked;        // in world sector
    int             linkcount;     // changed when linked/unlinked (for stale check)
    int             svFlags;       // SVF_NOCLIENT, SVF_CLIENTMASK, SVF_BOT, etc.
    int             singleClient;  // only send to this client
    qboolean        bmodel;        // if false, use abs* for SVF_USE_CURRENT_ORIGIN
    vec3_t          mins, maxs;
    int             contents;      // CONTENTS_TRIGGER, CONTENTS_SOLID, etc.
    vec3_t          absmin, absmax; // derived from origin + mins/maxs
    vec3_t          currentOrigin;
    vec3_t          currentAngles;
    int             ownerNum;      // for projectile owner exclusion in traces
} entityShared_t;
```

### SVF Flags

| Flag | Value | Meaning |
|---|---|---|
| `SVF_NOCLIENT` | 0x001 | Don't send to any client |
| `SVF_BOT` | 0x008 | This entity is a bot |
| `SVF_BROADCAST` | 0x020 | Send to all clients regardless of PVS |
| `SVF_PORTAL` | 0x040 | Send to clients beyond portal |
| `SVF_USE_CURRENT_ORIGIN` | 0x080 | Use entity's currentOrigin instead of s.origin for PVS |
| `SVF_SINGLECLIENT` | 0x100 | Only send to entity->r.singleClient |
| `SVF_NOSERVERINFO` | 0x200 | Don't include in server info |
| `SVF_CLIENTMASK` | 0x400 | Send to specific set of clients |

---

## cg_public.h — cgame Module Interface

### Engine → cgame (Entry Points)

| Callnum | When Called | Arguments |
|---|---|---|
| `CG_INIT` | Map loaded | `serverMessageNum, serverCommandSequence, clientNum` |
| `CG_SHUTDOWN` | Map unloaded | — |
| `CG_CONSOLE_COMMAND` | Console cmd to cgame | — |
| `CG_DRAW_ACTIVE_FRAME` | Every rendered frame | `serverTime, stereoView, demoPlayback` |
| `CG_CROSSHAIR_PLAYER` | UI: name for crosshair target | — |
| `CG_LAST_ATTACKER` | UI: name of last attacker | — |
| `CG_KEY_EVENT` | Key press/release | `key, down` |
| `CG_MOUSE_EVENT` | Mouse move | `dx, dy` |
| `CG_EVENT_HANDLING` | Prompt for event handling mode | — |

### cgame → Engine (Trap Calls)

**Renderer:**

| Call | Notes |
|---|---|
| `CG_R_REGISTERMODEL(name)` | Register model → qhandle_t |
| `CG_R_REGISTERSKIN(name)` | Register skin |
| `CG_R_REGISTERSHADER(name)` | Register shader |
| `CG_R_CLEARSCENE()` | Reset scene |
| `CG_R_ADDREFENTITYTOSCENE(refEnt)` | Add entity to scene |
| `CG_R_ADDPOLYTOSCENE(shader, verts, n)` | Add polygon (decals) |
| `CG_R_ADDLIGHTTOSCENE(org, int, r,g,b)` | Add dynamic light |
| `CG_R_RENDERSCENE(refdef)` | Render the 3D scene |
| `CG_R_DRAWSTRETCHPIC(...)` | 2D blit |
| `CG_R_SETCOLOR(rgba)` | Set 2D color |
| `CG_R_MODELBOUNDS(model, mins, maxs)` | Get model bounds |
| `CG_R_LERPTAG(tag, model, frame0, frame1, frac, name)` | Interpolate model tag |
| `CG_R_INPVS(p1, p2)` | Visibility check |

**Sound:**

| Call | Notes |
|---|---|
| `CG_S_REGISTERSOUND(name, compressed)` | Load sound → sfxHandle_t |
| `CG_S_STARTSOUND(origin, ent, chan, sfx)` | Play positional sound |
| `CG_S_STARTLOCALSOUND(sfx, chan)` | Play non-positional (UI/weapon) |
| `CG_S_STARTBACKGROUNDTRACK(intro, loop)` | Start music |
| `CG_S_STOPBACKGROUNDTRACK()` | Stop music |
| `CG_S_UPDATEENTITYPOSITION(ent, origin)` | Update sound source position |
| `CG_S_RESPATIALIZE(ent, origin, axis, inwater)` | Update listener |
| `CG_S_RAWSAMPLES(n, rate, width, ch, data, vol)` | Stream raw PCM (video audio) |

**Cgame State:**

| Call | Notes |
|---|---|
| `CG_GETSNAPSHOT(snapNum, snap)` | Copy snapshot to QVM |
| `CG_GETGAMESTATE(gs)` | Copy gamestate (configstrings) |
| `CG_GETCURRENTSNAPSHOTNUMBER(snap, time)` | Current snapshot number |
| `CG_GETCURRENTCMDNUMBER` | Last sent usercmd number |
| `CG_GETUSERCMD(cmdNum, cmd)` | Get specific usercmd |
| `CG_SETUSERCMDVALUE(value, sensitivity)` | Set weapon/sensitivity |
| `CG_MEMORY_REMAINING` | Hunk memory left |

**Collision (client-side prediction):**

| Call | Notes |
|---|---|
| `CG_CM_LOADMAP(mapname)` | Load CM data (same as server) |
| `CG_CM_NUMINLINEMODELS` | Count inline models |
| `CG_CM_INLINEMODEL(index)` | Get inline model handle |
| `CG_CM_TEMPBOXMODEL(mins, maxs)` | Create temp box for prediction |
| `CG_CM_BOXTRACE(results, ...)` | Trace (client-side prediction) |
| `CG_CM_TRACECAPSULE(...)` | Capsule trace |
| `CG_CM_POINTCONTENTS(point, model)` | Content flags |

**Custom cgame→engine changes [CUSTOM]:**

| Call | Custom Handler | Notes |
|---|---|---|
| `CG_CVAR_SET(name, value)` | `Cvar_SetSafe` instead of `Cvar_Set` | Respects CVAR_PROTECTED |
| `CG_CONSOLE_COMMAND` | Custom handler for `clientScreenshot` server command | `#ifdef USE_FTWGL` |

This prevents the cgame QVM from overriding `snaps` or `cg_smoothClients`.

The `clientScreenshot` command is sent to cgame as a server command. The engine handler in `cl_cgame.c` intercepts it and calls `screenshot silent [filename]`.

---

## ui_public.h — UI Module Interface

### Engine → UI (Entry Points)

| Callnum | When Called |
|---|---|
| `UI_INIT` | UI module loaded |
| `UI_SHUTDOWN` | UI module unloaded |
| `UI_KEY_EVENT(key, down)` | Key event when UI is active |
| `UI_MOUSE_EVENT(dx, dy)` | Mouse movement |
| `UI_REFRESH(realtime)` | Every frame |
| `UI_IS_FULLSCREEN` | Query if UI covers full screen |
| `UI_SET_ACTIVE_MENU(menu)` | Push a specific menu screen |
| `UI_DRAW_CONNECT_SCREEN(overlay)` | Connection screen |
| `UI_HASUNIQUECDKEY` | CD key validation |

### UI → Engine (Trap Calls)

The UI module has the same renderer, sound, filesystem, and cvar access as cgame. Key additions:

| Call | Notes |
|---|---|
| `UI_LAN_GETSERVERCOUNT(source)` | Count servers in LAN browser |
| `UI_LAN_GETSERVERADDRESSSTRING(source, n, buf, size)` | Get server address |
| `UI_LAN_GETSERVERINFO(source, n, buf, size)` | Get server infostring |
| `UI_LAN_GETPINGINFO(n, buf, size)` | Get ping result |
| `UI_LAN_MARKSERVERVISIBLE(source, n, visible)` | Show/hide in list |
| `UI_LAN_UPDATEVISIBLEPINGS(source)` | Trigger ping update |
| `UI_LAN_CLEARPING(n)` | Clear ping entry |
| `UI_LAN_ADDSERVER(source, name, addr)` | Add favorite server |
| `UI_LAN_REMOVESERVER(source, addr)` | Remove favorite |
| `UI_GETSTORAGE(...)` | Read ui/storage.bin |
| `UI_SETSTORAGE(...)` | Write ui/storage.bin |

### FTWGL-Specific UI Entry Points [CUSTOM]

| Callnum | When Called | Guard |
|---|---|---|
| `UI_AUTHSERVER_PACKET` | Auth server packet received (AUTH:CLIENT) | `#ifdef USE_AUTH` |

`UI_AUTHSERVER_PACKET` is called by the engine when the auth server sends a `CLIENT` packet. The engine forwards it to the UI QVM which handles accept/reject display. Defined in `code/ui/ui_public.h`.

---

## bg_public.h — Shared Game Types

**File:** `code/game/bg_public.h`

This header is included by both the game QVM AND the engine. Defines types that must match exactly on both sides.

### Configstring Indices

```c
// System configstrings (defined in q_shared.h)
#define CS_SERVERINFO   0     // sv_* SERVERINFO cvars
#define CS_SYSTEMINFO   1     // engine SYSTEMINFO cvars (sv_pure, etc.)

// Game configstrings (defined in bg_public.h)
#define CS_MUSIC          2   // background music filename
#define CS_MESSAGE        3   // map worldspawn message
#define CS_MOTD           4   // server MOTD
#define CS_WARMUP         5   // warmup countdown
#define CS_SCORES1        6   // red team / player1 score
#define CS_SCORES2        7   // blue team / player2 score
#define CS_VOTE_TIME      8   // vote expiry time
#define CS_VOTE_STRING    9   // vote text
#define CS_VOTE_YES       10  // yes votes
#define CS_VOTE_NO        11  // no votes
#define CS_GAME_VERSION   20  // GAME_VERSION string
#define CS_LEVEL_START_TIME 21 // level timer base
// ...
#define CS_ITEMS          27  // item presence bitmask
#define CS_MODELS         32  // model names (up to MAX_MODELS entries)
#define CS_SOUNDS         (CS_MODELS + MAX_MODELS) // sound names
#define CS_PLAYERS        (CS_SOUNDS + MAX_SOUNDS) // player names/teams
// ...
```

### Physics Types

#### trType_t — Trajectory Type

| Value | Name | Meaning |
|---|---|---|
| 0 | `TR_STATIONARY` | Not moving; `trBase` is position |
| 1 | `TR_INTERPOLATE` | Client interpolates between snapshots |
| 2 | `TR_LINEAR` | Extrapolate: `pos = trBase + trDelta * (t - trTime) * 0.001` |
| 3 | `TR_LINEAR_STOP` | `TR_LINEAR` but stops at `trTime + trDuration` |
| 4 | `TR_SINE` | `pos = trBase + sin((t - trTime) / trDuration) * trDelta` |
| 5 | `TR_GRAVITY` | Ballistic: `pos = trBase + trDelta*t - gravity*t²` |

**Custom usage in this fork:**
- `TR_INTERPOLATE` — default for player entities (set by `MSG_PlayerStateToEntityState`)
- `TR_LINEAR` — set by `sv_smoothClients` path in `SV_BuildCommonSnapshot` when velocity > 10 ups

#### trajectory_t — Position Descriptor

```c
typedef struct {
    trType_t    trType;
    int         trTime;         // reference time (ms)
    int         trDuration;     // stop time offset (for TR_LINEAR_STOP, TR_SINE)
    vec3_t      trBase;         // position at trTime
    vec3_t      trDelta;        // velocity (units/sec for TR_LINEAR; amplitude for TR_SINE)
} trajectory_t;
```

`BG_EvaluateTrajectory(tr, atTime, result)` — compute position at `atTime`:
```c
// TR_LINEAR: result = trBase + trDelta * (atTime - trTime) / 1000.0
// TR_GRAVITY: result = trBase + trDelta * dt - 0.5 * gravity * dt²
// TR_SINE: result = trBase + sin(dt/trDuration * 2π) * trDelta
// TR_INTERPOLATE/TR_STATIONARY: result = trBase (client does the lerp)
// Note: in the URT43 cgame with cl_urt43cgPatches bit 2 enabled, TR_INTERPOLATE
// is redirected to the TR_LINEAR handler. sv_snapshot.c anchors pos.trTime = sv.time
// for every TR_INTERPOLATE snapshot entity so that dt ≈ 0 at snap.serverTime
// (preserving normal interpolation) and dt > 0 at cg.time (forward extrapolation).
```

### Pmove — Player Movement

```c
typedef struct {
    // input/output
    playerState_t   *ps;           // updated in-place
    usercmd_t       cmd;           // input command
    int             tracemask;     // usually MASK_PLAYERSOLID
    int             debugLevel;
    qboolean        noFootsteps;
    qboolean        gauntletHit;

    // results
    int             numtouch;
    int             touchents[MAXTOUCH]; // entities touched this frame

    vec3_t          mins, maxs;    // bounding box (set from pm_type)
    int             watertype;
    int             waterlevel;    // 0 = none, 1 = up to waist, 2 = up to head, 3 = full

    // callbacks
    void            (*trace)(trace_t*, vec3_t, vec3_t, vec3_t, vec3_t, int, int);
    int             (*pointcontents)(vec3_t, int);
} pmove_t;
```

`Pmove(pmove_t *pm)` — the core player movement function. Executed in the game QVM with:
- Engine trace callback (`pm->trace` = `trap_Trace`)
- Engine pointcontents (`pm->pointcontents` = `trap_PointContents`)

The engine calls `GAME_CLIENT_THINK(clientNum)` which calls `ClientThink_real(ent)` → `Pmove(pm)`.

For multi-step Pmove (sv_pmoveMsec), the engine calls `GAME_CLIENT_THINK` multiple times with progressively advanced `cmd.serverTime`.

### entityState_t — Network Entity State

```c
typedef struct entityState_s {
    int         number;       // entity index (0 to MAX_GENTITIES-1)
    int         eType;        // ET_GENERAL, ET_PLAYER, ET_ITEM, ET_MISSILE, etc.
    int         eFlags;       // EF_DEAD, EF_FIRING, EF_BOUNCE, etc.

    trajectory_t pos;         // [CRITICAL] position trajectory — delta-compressed over network
    trajectory_t apos;        // angle trajectory

    int         time, time2;

    vec3_t      origin, origin2;
    vec3_t      angles, angles2;

    int         otherEntityNum;
    int         otherEntityNum2;
    int         groundEntityNum;

    int         constantLight;
    int         loopSound;
    int         modelindex, modelindex2;
    int         clientNum;    // for ET_PLAYER entities
    int         frame;

    int         solid;        // SOLID_BMODEL for brush entities
    int         event, eventParm;

    // ET_PLAYER fields:
    int         powerups;
    int         weapon;
    int         legsAnim, torsoAnim;
    int         generic1;
} entityState_t;
```

**pos.trType usage by entity type:**
- **Players:** `TR_INTERPOLATE` (or `TR_LINEAR` with sv_smoothClients)
- **Missiles:** `TR_LINEAR` (extrapolate)
- **Falling items:** `TR_GRAVITY`
- **Static items:** `TR_STATIONARY`
- **Doors:** `TR_LINEAR_STOP` or `TR_STATIONARY`

### playerState_t — Complete Player State

The full player state (server-authoritative). Delta-compressed via `MSG_WriteDeltaPlayerstate`.

```c
typedef struct playerState_s {
    int     commandTime;    // ps.commandTime = cmd.serverTime of last executed cmd
    int     pm_type;        // PM_NORMAL, PM_DEAD, PM_SPECTATOR, PM_NOCLIP
    int     pm_flags;       // PMF_DUCKED, PMF_JUMP_HELD, PMF_TIME_KNOCKBACK, ...
    int     pm_time;

    vec3_t  origin;         // authoritative position
    vec3_t  velocity;       // current velocity

    int     weaponTime;     // fire rate timer
    int     gravity;        // local gravity (may differ from DEFAULT_GRAVITY)
    int     speed;          // movement speed multiplier

    int     delta_angles[3]; // view angle offsets from teleport/rotation
    int     groundEntityNum; // ENTITYNUM_NONE when airborne

    int     legsAnim, torsoAnim;
    int     movementDir;    // direction of movement (0-7)
    vec3_t  grapplePoint;   // grapple attachment point

    int     eFlags;
    int     eventSequence;
    int     events[2];
    int     eventParms[2];
    int     externalEvent, externalEventParm, externalEventTime;

    int     clientNum;
    int     weapon;
    int     weaponstate;

    vec3_t  viewangles;     // view direction (for rendering)
    int     viewheight;     // 26 = standing, 12 = crouched, -16 = dead

    // damage indicators:
    int     damageEvent, damageYaw, damagePitch, damageCount;

    int     stats[MAX_STATS];       // health, armor, score, etc.
    int     persistant[MAX_PERSISTANT]; // items that survive death
    int     powerups[MAX_POWERUPS]; // level.time when powerup expires
    int     ammo[MAX_WEAPONS];      // ammo counts

    int     generic1;
    int     loopSound;
    int     jumppad_ent;
} playerState_t;
```

### usercmd_t — Client Input

Sent from client to server every frame (delta-compressed):
```c
typedef struct usercmd_s {
    int     serverTime;       // cmd is valid at this game time
    int     angles[3];        // view angles (as int16 packed to int)
    int     buttons;          // BUTTON_ATTACK, BUTTON_WALKING, etc.
    byte    weapon;           // current weapon number
    signed char forwardmove;  // -127 to +127 (forward/back)
    signed char rightmove;    // -127 to +127 (strafe)
    signed char upmove;       // -127 to +127 (jump/crouch)
} usercmd_t;
```

**Multi-step Pmove [CUSTOM]:** When `sv_pmoveMsec > 0`, the server may call `GAME_CLIENT_THINK` multiple times with progressively advanced `lastUsercmd.serverTime` values, each incrementing by at most `sv_pmoveMsec` ms. The `commandTime` in `playerState_t` advances accordingly. This ensures consistent step sizes regardless of server frame rate.

---

## Platform Layer (PLATFORM.md summary)

> Full details: `docs/project/PLATFORM.md` (not yet written — see code/unix/, code/win32/, code/sdl/)

| Directory | Platform | Key Files |
|---|---|---|
| `code/unix/` | Linux/macOS | `unix_main.c` (main), `linux_glimp.c` (X11/Wayland GL), `linux_qvk.c` (Vulkan) |
| `code/win32/` | Windows | `win_main.c`, `win_glimp.c` (WGL), `win_qvk.c` (Vulkan), `win_input.c` (DirectInput) |
| `code/sdl/` | SDL2 (cross-platform) | `sdl_glimp.c`, `sdl_input.c`, `sdl_snd.c` |

The platform layer provides:
- `main()` → `Com_Init` + `Com_Frame` loop
- Window creation and GL/VK context
- Input event generation → `Sys_QueEvent`
- Audio device open → fills `dma_t` structure
- Timing: `Sys_Milliseconds()`, `Sys_Microseconds()`
- CPU affinity: `Sys_SetAffinityMask(mask)`
- File system utilities: `Sys_FOpen`, `Sys_Mkdir`, `Sys_ListFiles`
___BEGIN___COMMAND_DONE_MARKER___0
