
> Covers all files in `code/server/`. This is the most heavily modified subsystem in the fork. Custom changes are marked **[CUSTOM]**.

---

## Table of Contents

1. [File Overview](#file-overview)
2. [sv_main.c — Frame Loop [CUSTOM]](#sv_mainc--frame-loop-custom)
3. [sv_init.c — Initialization & Cvars [CUSTOM]](#sv_initc--initialization--cvars-custom)
4. [sv_client.c — Client Connection & Commands [CUSTOM]](#sv_clientc--client-connection--commands-custom)
5. [sv_snapshot.c — Snapshot Building & Smoothing [CUSTOM]](#sv_snapshotc--snapshot-building--smoothing-custom)
6. [sv_antilag.c — Engine Antilag System [CUSTOM/NEW]](#sv_antilagc--engine-antilag-system-customnew)
7. [sv_game.c — QVM Game Interface](#sv_gamec--qvm-game-interface)
8. [sv_world.c — Entity Spatial Index](#sv_worldc--entity-spatial-index)
9. [sv_ccmds.c — Server Console Commands [CUSTOM]](#sv_ccmdsc--server-console-commands-custom)
9a. [URT Server Additions [CUSTOM/URT]](#urt-server-additions-customurt)
10. [sv_bot.c — Bot Integration](#sv_botc--bot-integration)
11. [sv_net_chan.c — Server Netchan](#sv_net_chanc--server-netchan)
12. [sv_filter.c — IP Filter/Ban System](#sv_filterc--ip-filterban-system)
13. [sv_rankings.c — Statistics Tracking](#sv_rankingsc--statistics-tracking)
14. [Key Data Structures (server.h)](#key-data-structures-serverh)
15. [All Server Cvars](#all-server-cvars)
16. [Complete Call Graph — Server Tick](#complete-call-graph--server-tick)
17. [Auth and Demo Cvars [URT]](#auth-and-demo-cvars-urt)

---

## File Overview

| File | Lines | Custom? | Purpose |
|------|-------|---------|---------|
| `sv_main.c` | 1780 | **YES** | Frame loop, time management, rate control |
| `sv_init.c` | 1100 | **YES** | SV_Init, SV_SpawnServer, all cvar registration |
| `sv_client.c` | 2510 | **YES** | Connection, usercmd handling, multi-step Pmove |
| `sv_snapshot.c` | 1846 | **YES** | Snapshot building, position extrapolation, ring buffer |
| `sv_antilag.c` | ~700 | **NEW** | Engine-side shadow antilag |
| `sv_antilag.h` | ~80 | **NEW** | Antilag public API |
| `sv_game.c` | 1170 | no | QVM syscall handler, G_TRACE intercept |
| `sv_world.c` | 660 | no | Entity world sectors, SV_Trace, SV_LinkEntity |
| `sv_ccmds.c` | ~3200 | **YES** | Server console commands, map_restart fix, server demo, clientScreenshot |
| `sv_bot.c` | 450 | no | Bot client allocation, BotLib interface |
| `sv_net_chan.c` | 300 | no | Server netchan encode/decode |
| `sv_filter.c` | 1100 | no | IP ban/filter expression engine |
| `sv_rankings.c` | 1537 | no | Player statistics/rankings |
| `server.h` | ~500 | **YES** | Server-internal types with gameTime, auth, demo fields |

---

## sv_main.c — Frame Loop [CUSTOM]

**File:** `code/server/sv_main.c`  
**Size:** ~1780 lines

### The Two-Rate Architecture

The most important custom change. The server decouples the engine tick rate from the QVM game frame rate:

```
sv_fps   = engine ticks/sec (default 60)  — input sampling, antilag, snapshots
sv_gameHz = QVM GAME_RUN_FRAME rate (default 20) — what level.time sees
```

**sv_gameHz compatibility:** All UT versions hardcode `serverTime += 50` in QVM antiwarp code, assuming 50ms game frames. **REQUIRED for UT 4.0-4.2:** set to 20 (game code broadly intolerant of non-20Hz rates). **Any version with QVM g_antiwarp enabled:** set to 20 (antiwarp misfires at other rates). **UT 4.3+ with g_antiwarp off:** sv_gameHz 0 is safe. **Alternative:** use `sv_antiwarp 1` — engine-side antiwarp uses frame-rate-aware step sizes and works at any sv_gameHz.

### SV_Frame(int msec) [CUSTOM]

```
SV_Frame(msec):
  if (!sv_running): return

  SV_CheckCvars()      ← detect cvar changes, call SV_TrackCvarChanges()

  svs.time += msec
  sv.timeResidual += msec

  CLAMP sv.timeResidual to 2×frameMsec   ← absorb OS scheduler jitter

  frameMsec = 1000 / sv_fps
  gameMsec  = 1000 / sv_gameHz  (or frameMsec if sv_gameHz <= 0)

  while (sv.timeResidual >= frameMsec):
      sv.timeResidual -= frameMsec
      sv.time += frameMsec

      [ANTILAG] SV_Antilag_RecordPositions()        ← shadow position snapshots (all active clients)

      [GAME FRAME LOOP]
      sv.gameTimeResidual += frameMsec
      while (sv.gameTimeResidual >= gameMsec):
          sv.gameTimeResidual -= gameMsec
          sv.gameTime += gameMsec

          [ENGINE ANTIWARP — sv_antiwarp 1 or 2]
          for each active non-bot client:
              if sv.gameTime - awLastThinkTime > tolerance:
                  lastUsercmd.serverTime += gameMsec
                  mode 2: decay movement inputs based on gap phase
                    extrapolate (tol+extra) → decay (antiwarpDecay) → friction
                  VM_Call(gvm, GAME_CLIENT_THINK, i)
                  awLastThinkTime = sv.gameTime

          SV_BotFrame(sv.gameTime)
          VM_Call(gvm, 1, GAME_RUN_FRAME, sv.gameTime)  ← QVM sees this

      [RING BUFFER]
      if (smoothing_active):
          SV_SmoothRecordAll()               ← record position ring buffer

      SV_IssueNewSnapshot()                  ← build entity state for this tick
      SV_SendClientMessages()                ← dispatch to clients (rate-controlled)

  SV_CheckTimeouts()                         ← once per Com_Frame, not per tick
```

**Key invariant:** `SV_SendClientMessages` is INSIDE the tick loop. This prevents a 2× packet gap when OS jitter causes `Com_Frame` to skip a beat and catch up with two ticks at once.

### SV_TrackCvarChanges() [CUSTOM]

Called when any server cvar is modified:

```
if sv_antiwarp modified:
    if sv_antiwarp enabled: Cvar_Set("g_antiwarp", "0")  ← prevent double injection
if sv_fps modified or sv_gameHz modified:
    clamp sv.timeResidual to 2×new_frameMsec  ← prevent burst ticking
if sv_fps modified or sv_gameHz modified or sv_bufferMs modified or sv_velSmooth modified:
    SV_SmoothInit()                           ← clear ring buffers
if sv_fps modified or sv_snapshotFps modified:
    recalculate per-client snapshotMsec
    sv_snapshotFps->modified = qfalse         ← [FIX: was never cleared]
```

### SV_FrameMsec()

Returns the sleep interval for `Com_Frame`'s rate-limiter:
- If `sv_busyWait > 0`: returns `frameMsec - sv_busyWait` → wakes early to spin-wait
- Otherwise: returns `frameMsec` exactly

### SV_CalcPings()

Called once per `SV_Frame` entry (before tick loop). Recomputes `cl->ping` for all connected clients from packet timing:
```
ping = svs.time - cl->frames[cl->ping_frame & PACKET_MASK].messageSent
```

### SV_CheckTimeouts()

Called once per `Com_Frame` (not per tick). Disconnects clients where:
- `svs.time - cl->lastPacketTime > sv_timeout * 1000`

---

## sv_init.c — Initialization & Cvars [CUSTOM]

**File:** `code/server/sv_init.c`  
**Size:** ~1100 lines

### SV_Init()

Called once at engine startup from `Com_Init`:
```
SV_Init:
  SV_AddOperatorCommands()    ← register map, kick, status, etc.
  SV_InitOperatorCommands()
  Register all cvars (see cvar list below)
  SV_Antilag_Init()           ← register antilag cvars + state [CUSTOM]
  Cvar_Set("sv_running", "0")
```

### SV_SpawnServer(mapname, killBots) [CUSTOM]

Called on every map load or restart. The most complex initialization function:

```
SV_SpawnServer:
  SV_SendClientMessages()     ← flush before shutdown
  SV_ShutdownGameProgs()      ← free old QVM
  SV_ClearServer()            ← clear server_t (sv)
  CM_LoadMap(mapname)         ← load BSP collision
  sv.gameTime = 0
  sv.gameTimeResidual = 0     ← [CUSTOM: sync game clock]
  sv.serverId = com_frameTime
  SV_SetConfigstring(CS_SERVERINFO, Cvar_InfoString(CVAR_SERVERINFO))
  SV_SetConfigstring(CS_SYSTEMINFO, Cvar_InfoString(CVAR_SYSTEMINFO))
  SV_CreateBaseline()         ← build entity baselines for delta compression
  SV_SetSnapshotParams()      ← compute snapshot storage sizes
  SV_InitGameProgs()          ← load qagame.qvm, call GAME_INIT
  [settlement: run N extra GAME_RUN_FRAME calls to warm up entity state]
  SV_SmoothInit()             ← clear all ring buffers [CUSTOM]
  for each connected client:
      SV_ClientEnterWorld()   ← send new gamestate
```

### SV_SetSnapshotParams() [CUSTOM]

Computes `sv_snapshotFps`-aware snapshot storage sizes. Adjusts `NUM_SNAPSHOT_FRAMES` based on configured rates to ensure enough history for client delta compression.

### SV_BoundMaxClients()

Clamps `sv_maxclients` to [1, MAX_CLIENTS]. Called on map load.

### SV_SendConfigstring(client, index)

Sends one configstring to a specific client as a reliable server command.

### SV_SetConfigstring(index, val)

Sets a configstring globally and marks it modified so `SV_UpdateConfigstrings` will push it to all clients.

---

## sv_client.c — Client Connection & Commands [CUSTOM]

**File:** `code/server/sv_client.c`  
**Size:** ~2510 lines

### Connection State Machine

```
CS_FREE → CS_CONNECTED (SV_DirectConnect)
       → CS_PRIMED     (SV_SendClientGameState)
       → CS_ACTIVE     (SV_ClientEnterWorld)
       → CS_ZOMBIE     (SV_DropClient, brief delay before CS_FREE)
```

### SV_DirectConnect(netadr_t *from) 

The initial OOB `connect` handler. Validates:
- Challenge matches
- Not IP-banned
- Max clients not exceeded
- Pure server pak checksum match

Creates `client_t`, assigns slot, calls `SV_GetChallenge` path.

### SV_ClientEnterWorld(client_t *cl, usercmd_t *cmd)

Called when client transitions from CS_PRIMED to CS_ACTIVE:
- Sets `cl->state = CS_ACTIVE`
- Calls `VM_Call(gvm, 2, GAME_CLIENT_BEGIN, clientNum)` → QVM game init for this client
- Triggers first snapshot

### SV_ClientThink(client_t *cl, usercmd_t *cmd) [CUSTOM]

The central usercmd execution function. Contains multi-step Pmove:

```
SV_ClientThink(cl, cmd):
    cl->lastUsercmd = *cmd

    if cl->state != CS_ACTIVE: return

    if sv_pmoveMsec > 0 AND NOT bot (netchan.remoteAddress.type != NA_BOT):
        delta = cmd.serverTime - ps->commandTime
        if delta > sv_pmoveMsec:
            realTime = cmd.serverTime
            while (ps->commandTime + sv_pmoveMsec < realTime):
                cl->lastUsercmd.serverTime = ps->commandTime + sv_pmoveMsec
                VM_Call(gvm, 1, GAME_CLIENT_THINK, clientNum)  ← QVM
                safety: if commandTime didn't advance → goto single_call
            cl->lastUsercmd.serverTime = realTime
            VM_Call(gvm, 1, GAME_CLIENT_THINK, clientNum)      ← final step
            return

    single_call:
    VM_Call(gvm, 1, GAME_CLIENT_THINK, clientNum)
```

**Why multi-step Pmove:**
- Without it: a 333fps client sends 3ms usercmds. At `sv_fps 60` (16ms frames), the QVM might accumulate a 16ms delta in one call. Jump height, strafejumping, and weapon timing all depend on step size.
- With `sv_pmoveMsec 8`: step size is at most 8ms (equivalent to 125fps). All clients behave consistently regardless of their own frame rate.

**Safety goto:** If `commandTime` doesn't advance after `GAME_CLIENT_THINK`, the QVM is in intermission or a pause state where `Pmove` doesn't run. Fall through to single_call to avoid infinite loop.

**Bot exclusion:** Bots (`NA_BOT`) always go to `single_call`. They get 50ms steps from `level.time` directly.

### SV_UserinfoChanged(client_t *cl, ...) [CUSTOM]

Called on connect and on `userinfo` command. Computes `cl->snapshotMsec`:

```
if bot:
    snapHz = (sv_snapshotFps == -1) ? sv_fps : sv_snapshotFps
else:
    sv_snapshotFps == -1 → snapHz = sv_fps
    sv_snapshotFps == 0  → snapHz = clamp(cl_snaps_userinfo, 1, sv_fps)
    sv_snapshotFps > 0   → snapHz = min(sv_snapshotFps, sv_fps)
cl->snapshotMsec = 1000 / snapHz
```

Also parses userinfo: name, rate, cl_maxpackets, cl_timenudge, etc.

### SV_DropClient(client_t *drop, reason)

```
SV_DropClient:
    VM_Call(gvm, 2, GAME_CLIENT_DISCONNECT, clientNum)
    send "disconnect" reliable command to client
    cl->state = CS_ZOMBIE
    cl->lastDisconnectTime = svs.time
```

### SV_FreeClient(client_t *client)

Called after zombie timeout. Resets slot to CS_FREE.

### SV_FloodProtect(client_t *cl)

Leaky-bucket rate limiter for client commands. Returns `qtrue` if client is flooding; the command is silently dropped.

### Challenge System

`SV_CreateChallenge(timestamp, from)` / `SV_VerifyChallenge(received, from)`:
- MD5-based challenge/response to prevent IP spoofing during connection
- Challenge encodes `from` address + current timestamp → only valid for ~30 seconds

### SV_SendClientGameState(client_t *client)

Sends the full game state (configstrings, baselines) to a newly connected client. Transitions client from CS_CONNECTED to CS_PRIMED.

### SV_ExecuteClientCommand(client_t *cl, s)

Routes client commands:
- `userinfo` → `SV_UserinfoChanged`
- `disconnect` → `SV_DropClient`
- `download` → file download system
- `nextdl`, `stopdl`, `donedl` → download protocol
- `mv` → multiview
- Anything else → `VM_Call(gvm, GAME_CLIENT_COMMAND, clientNum)`

---

## sv_snapshot.c — Snapshot Building & Smoothing [CUSTOM]

**File:** `code/server/sv_snapshot.c`  
**Size:** ~1846 lines

### Position Smoothing Ring Buffer (lines 1-280) [CUSTOM]

#### svSmoothHistory_t

32 slots per client, one record per tick:
```c
typedef struct {
    vec3_t  origin;
    vec3_t  velocity;
    int     serverTime;
    qboolean valid;
} svSmoothPos_t;

typedef struct {
    svSmoothPos_t slots[32];
    int head;     // next write index
    int count;    // valid entries (min(writes, 32))
} svSmoothHistory_t;
```

32 slots = 256ms at sv_fps 125, 533ms at sv_fps 60.

#### SV_SmoothInit()

Zeroes all ring buffers for all clients. Called on cvar change or map load.

#### SV_SmoothRecordAll() [CUSTOM]

Called from `SV_Frame` when `(sv_extrapolate || sv_smoothClients) AND (sv_bufferMs != 0 OR sv_velSmooth > 0)`.

Records one slot per active client. For bots: velocity-extrapolates from entity position. For real players: reads directly from `ps->origin`.

**Active condition:** If both `sv_bufferMs == 0` and `sv_velSmooth == 0`, this function is NEVER called. Ring buffer data is only accumulated when a consumer needs it.

#### SV_SmoothGetPosition(clientNum, targetTime, out) [CUSTOM]

Walks ring buffer backward from `head`, finds two entries bracketing `targetTime`, linearly interpolates between them. Returns `qfalse` if history doesn't reach `targetTime`.

#### SV_SmoothGetAverageVelocity(clientNum, windowMs, outVelocity) [CUSTOM]

Exponential weighted average of velocity entries within the last `windowMs` milliseconds. The most-recent sample carries weight 1.0; each older step is multiplied by 0.5 (half-life = 1 sample). At `sv_velSmooth=64` / `sv_fps=60` this yields ~4 samples with weights ≈ 53 % / 26 % / 13 % / 7 %, so the latest velocity dominates while enough history is present to suppress per-tick physics noise.

**Why exponential instead of uniform:** a uniform sliding window gives every sample equal weight. As the window slides by one tick the dropped oldest sample and the newly added current sample can differ substantially (e.g. opposite directions during a turn), flipping `trDelta` noticeably between consecutive snapshots. The exponential weighting makes each new snapshot's `trDelta` a smooth continuation of the previous one, eliminating the trajectory kink at every snapshot boundary that manifests as blur/micro-stutter for fast-moving players.

### Snapshot System

#### SV_InitSnapshotStorage()

Allocates `svs.snapshotEntities` pool. Called during `SV_SpawnServer`.

#### SV_IssueNewSnapshot()

Called once per tick. Increments `svs.snapshotFrame` and calls `SV_BuildCommonSnapshot()`.

#### SV_BuildCommonSnapshot() [CUSTOM]

The core of the position correction pipeline. Called once per tick to build entity data for ALL clients:

```
for each entity (0 to sv.num_entities):
    es = &sv.gentities[i].s

    if es->number >= sv_maxclients: continue  (not a player)
    if es->pos.trType != TR_INTERPOLATE: continue  (already has motion)

    [PHASE 1: resolve position source]

    if sv_bufferMs > 0:
        bufMs = (sv_bufferMs == -1) ? (1000/sv_fps) : sv_bufferMs
        targetTime = sv.time - bufMs
        usedBuffer = SV_SmoothGetPosition(es->number, targetTime, &origin, &vel)
    
    if !usedBuffer:
        if bot AND sv.time > sv.gameTime:     ← only when sv_gamehz creates a gap
            // extrapolate from last game-frame boundary using ps->velocity
            dt     = (sv.time - sv.gameTime) * 0.001
            origin = trBase + ps->velocity * dt
            vel    = ps->velocity
        else (bot, sv_gamehz 0):
            origin = trBase          ← already fresh every tick
            vel    = trDelta

        if real player:
            // ps->origin only advances when a usercmd is processed (GAME_CLIENT_THINK)
            // or when G_RunClient fires (at sv_gamehz rate).  Between game frames,
            // the observed player's ps->origin can lag sv.time by up to one packet
            // interval → identical trBase in 2-3 consecutive snapshots → stutter.
            // Fix: extrapolate from last Pmove time (ps->commandTime) to sv.time.
            // Cap dt to the game-frame gap (sv.time - sv.gameTime) to avoid
            // over-extrapolation.  At sv_gamehz 0, G_RunClient keeps
            // ps->commandTime == sv.time every tick → dt == 0 → no-op.
            if sv.time > sv.gameTime AND ps->commandTime < sv.time:
                dt = min(sv.time - ps->commandTime, sv.time - sv.gameTime) * 0.001
                origin = ps->origin + ps->velocity * dt
            else:
                origin = ps->origin  ← fresh (sv_gamehz 0 or usercmd this tick)
            vel = ps->velocity

    [PHASE 2: apply trajectory]

    if bot:
        // Keep TR_INTERPOLATE — avoids visual/server mismatch when bots
        // change direction at a game-frame boundary (TR_LINEAR would show
        // stale-velocity extrapolation until the next game frame).
        // Gate: only anchor when sv_gamehz creates a gap; at sv_gamehz 0
        // trBase is already fresh every tick so no change is needed.
        if sv.time > sv.gameTime:
            VectorCopy(origin, es->pos.trBase)

    deadZone = DotProduct(vel, vel) > 100.0f  (speed > ~10 ups)

    else if sv_smoothClients:
        if deadZone:
            es->pos.trType  = TR_LINEAR
            es->pos.trBase  = origin
            es->pos.trDelta = (sv_velSmooth > 0) ? SV_SmoothGetAverageVelocity(...) : vel
            es->pos.trTime  = sv.time
        // else: stay TR_INTERPOLATE with anchored position

    else if sv_extrapolate:
        if usedBuffer AND deadZone:
            VectorCopy(origin, es->pos.trBase)
        else if real player AND deadZone:
            VectorCopy(ps->origin, es->pos.trBase)
```

**Dead-zone explanation:** `DotProduct(vel, vel) > 100.0f` means `|vel| > 10 ups`. This filters Pmove ground-snap oscillations on stationary players. Without it, a player standing still would flutter between TR_LINEAR and TR_INTERPOLATE every few ticks, causing vibration.

#### SV_BuildClientSnapshot(client_t *client)

Builds per-client snapshot (not shared):
1. Determines which cluster/area the client can see from
2. `SV_AddEntitiesVisibleFromPoint` — walks PVS, adds visible entities
3. Handles portal views (merges second PVS)
4. Copies `playerState_t` for this client's POV

#### SV_WriteSnapshotToClient(client_t *client, msg_t *msg)

Serializes snapshot into msg_t:
1. Write deltabase sequence (client acknowledges which old snapshot to delta from)
2. `MSG_WriteDeltaPlayerstate` — player state delta
3. `SV_EmitPacketEntities` — entity state deltas for all visible entities
4. Optional: `SV_BuildCompressedBuffer` (LZSS for large snapshots)

#### SV_SendClientSnapshot(client_t *client)

1. `SV_BuildClientSnapshot` — build entity list
2. `SV_WriteSnapshotToClient` — serialize
3. `SV_SendMessageToClient` — dispatch via netchan

#### SV_SendClientMessages() [CUSTOM]

Called once per tick (inside the tick loop):
```
for each active client:
    if (svs.time - cl->lastSnapshotTime >= cl->snapshotMsec):
        SV_SendClientSnapshot(cl)
        cl->lastSnapshotTime = svs.time
        SV_Antilag_NoteSnapshot(clientNum)   ← rate monitoring
```

Rate-of-send is per-client via `cl->snapshotMsec`, not a global rate.

---

## sv_antilag.c — Engine Antilag System [CUSTOM/NEW]

**File:** `code/server/sv_antilag.c` (new file)  
**Header:** `code/server/sv_antilag.h`

See `ARCHITECTURE.md` Section 7 for the full design rationale. This documents the complete function API.

### Shadow History

```c
typedef struct {
    vec3_t  origin;
    vec3_t  absmin;
    vec3_t  absmax;
    int     serverTime;
    qboolean valid;
} svShadowPos_t;

typedef struct {
    svShadowPos_t slots[256];  // ring buffer
    int head;
    int count;
} svShadowHistory_t;
```

Up to 256 slots per client. At sv_fps 60 → 60 Hz recording → 256/60 = ~4.3 seconds of history (capped by sv_antilagMaxMs).

### Public API

#### void SV_Antilag_Init()

Called from `SV_Init()`. Registers cvars: `sv_antilag`, `sv_antilagMaxMs`, `sv_antilagDebug`, `sv_antilagRateDebug`. Calls `SV_Antilag_ComputeConfig()`.

#### void SV_Antilag_RecordPositions()

Called once per engine tick from `SV_Frame()`. Records `gent->r.currentOrigin`, `absmin`, `absmax` for all active clients — including bots. Bots are recorded so that human shooters get accurate lag compensation when targeting bots (the shooter sees the bot at a position from ~(ping + snapshotMsec) ms ago, same as for human targets). `sv_antilag 1` auto-forces `g_antilag 0`, so no QVM FIFO pre-setup interferes with the shadow rewind.

#### void SV_Antilag_NoteSnapshot(int clientNum)

Called from `SV_SendClientMessages()` after each snapshot send. Tracks snapshot send rate for debugging. Prints per-client rate report when `sv_antilagRateDebug > 0`.

#### qboolean SV_Antilag_InterceptTrace(...)

**The main entry point.** Called from `sv_game.c:SV_GameSystemCalls()` for G_TRACE and G_TRACECAPSULE syscalls.

```
SV_Antilag_InterceptTrace:
    if !sv_antilag: return qfalse  (caller uses normal SV_Trace)

    if passEntityNum is invalid or bot: return qfalse

    fireTime = SV_Antilag_GetClientFireTime(passEntityNum)
        = svs.time - cl->ping, clamped to [svs.time - sv_antilagMaxMs, svs.time]

    rewound = SV_Antilag_RewindAll(passEntityNum, fireTime)
        for each active non-shooter client (humans and bots):
            ① save gent->r.currentOrigin/absmin/absmax
            ② apply SV_Antilag_GetPositionAtTime(i, fireTime)
            ③ SV_LinkEntity(gent)

    SV_Trace(results, start, mins, maxs, end, passEntityNum, contentmask, capsule)

    SV_Antilag_RestoreAll(passEntityNum)  ← ALWAYS runs
        for each rewound client:
            restore from sv_shadowSaved[i]
            SV_LinkEntity(gent)

    return qtrue  (caller skips its normal SV_Trace call)
```

**QVM Interaction Note:** The UT QVM runs `G_TimeShiftAllClients` before calling `trap_Trace`. Step ① overrides those positions with our more-accurate shadow history. After our restore, the QVM runs `G_UnTimeShiftAllClients` from its own saved state — this is safe because we already restored.

### Private Functions

| Function | Purpose |
|---|---|
| `SV_Antilag_ComputeConfig()` | Compute shadowHz and historySlots from cvars |
| `SV_Antilag_GetClientFireTime(shooterNum)` | sv.time - ping/2, clamped |
| `SV_Antilag_GetPositionAtTime(clientNum, t, out)` | Interpolated position at time t |
| `SV_Antilag_RewindAll(shooterNum, targetTime)` | Save + rewind all non-shooter clients |
| `SV_Antilag_RestoreAll(shooterNum)` | Unconditional restore from sv_shadowSaved[] |

---

## sv_game.c — QVM Game Interface

**File:** `code/server/sv_game.c`  
**Size:** ~1170 lines

### SV_GameSystemCalls(intptr_t *args)

The syscall handler for `qagame.qvm`. Every `trap_*` call from the QVM lands here.

Key cases:

| Syscall | Handler |
|---|---|
| `G_PRINT` | `Com_Printf` |
| `G_ERROR` | `Com_Error(ERR_DROP)` |
| `G_MILLISECONDS` | `Sys_Milliseconds()` |
| `G_CVAR_REGISTER/UPDATE/SET` | `Cvar_Register/Update/SetSafe` |
| `G_LOCATE_GAME_DATA` | `SV_LocateGameData()` |
| `G_DROP_CLIENT` | `SV_GameDropClient()` |
| `G_SEND_SERVER_COMMAND` | `SV_GameSendServerCommand()` |
| `G_SET_CONFIGSTRING` | `SV_SetConfigstring()` |
| `G_SET_BRUSH_MODEL` | `SV_SetBrushModel()` |
| `G_TRACE` | **`SV_Antilag_InterceptTrace()` [CUSTOM]** or `SV_Trace()` |
| `G_TRACECAPSULE` | Same as G_TRACE with capsule=true |
| `G_POINT_CONTENTS` | `SV_PointContents()` |
| `G_IN_PVS` | `SV_inPVS()` |
| `G_LINK_ENTITY` | `SV_LinkEntity()` |
| `G_UNLINK_ENTITY` | `SV_UnlinkEntity()` |
| `G_AREA_ENTITIES` | `SV_AreaEntities()` |
| `G_BOT_ALLOCATE_CLIENT` | `SV_BotAllocateClient()` |
| `G_GET_USERCMD` | Copy `cl->lastUsercmd` to QVM |
| `G_FS_FOPEN/READ/WRITE/CLOSE` | Sandboxed FS access (H_QAGAME owner) |

### G_TRACE Intercept Flow

```
SV_GameSystemCalls(G_TRACE):
    if SV_Antilag_InterceptTrace(results, start, mins, maxs, end, passEntityNum, mask, capsule):
        return 0    ← antilag ran, results already set
    SV_Trace(results, ...)  ← normal path
    return 0
```

### SV_LocateGameData(gEnts, numEntities, sizeofGEntity, clients, sizeofGameClient)

Called from `GAME_INIT`. Stores pointers to the QVM's entity and client arrays into engine globals:
```c
sv.gentities = gEnts;
sv.gentitySize = sizeofGEntity;
sv.num_entities = numEntities;
sv.gameClients = clients;
sv.gameClientSize = sizeofGameClient;
```

After this, `SV_GentityNum(n)` = `(sharedEntity_t*)((byte*)sv.gentities + n * sv.gentitySize)`.

---

## sv_world.c — Entity Spatial Index

**File:** `code/server/sv_world.c`  
**Size:** ~660 lines

### World Sectors

Entities are stored in a 2D spatial hash (world sectors) for fast area queries:
```c
typedef struct worldSector_s {
    int     axis;              // split axis (0=x, 1=y)
    float   dist;              // split plane
    struct worldSector_s *children[2];
    svEntity_t *entities;      // linked list of entities here
} worldSector_t;
```

`SV_ClearWorld()` — called on map load, builds the 2D sector grid from map bounds.

### SV_LinkEntity(sharedEntity_t *gEnt)

Called by QVM after moving an entity. Updates:
1. Computes `r.absmin/absmax` from `r.currentOrigin + r.mins/maxs`
2. Finds which clusters the entity's bounding box occupies (`CM_BoxLeafnums`)
3. Stores cluster numbers in `svEnt->clusternums`
4. Inserts entity into the world sector linked list

**Used by antilag:** `SV_Antilag_RewindAll` calls `SV_LinkEntity` after repositioning each entity to update the spatial index.

### SV_UnlinkEntity(sharedEntity_t *gEnt)

Removes entity from world sector linked list.

### SV_AreaEntities(mins, maxs, list, maxcount)

Returns entity numbers whose bounding boxes overlap the query region. Uses world sector tree for fast rejection.

### SV_Trace(results, start, mins, maxs, end, passEntityNum, contentmask, capsule)

```
SV_Trace:
  CM_BoxTrace(results, start, end, mins, maxs, 0, contentmask, capsule)
    ← world geometry

  if passEntityNum != ENTITYNUM_NONE:
      SV_ClipMoveToEntities(clip)
          for each entity in area:
              skip self / owner / other missiles
              SV_ClipToEntity() ← CM_TransformedBoxTrace on entity model
              if closer hit: update results
```

The entity list is obtained from `SV_AreaEntities` and entity-specific clip models use `CM_TempBoxModel` (bbox) or inline model (for brush entities).

### SV_PointContents(p, passEntityNum)

Returns content flags at point. Checks both world geometry (`CM_PointContents`) and entity bounds.

---

## sv_ccmds.c — Server Console Commands [CUSTOM]

**File:** `code/server/sv_ccmds.c`  
**Size:** ~3200 lines

### SV_MapRestart_f() [CUSTOM FIX]

Before the fix, `sv.gameTime` could be lagged behind `sv.time` at the moment of `map_restart`, causing `GAME_RUN_FRAME` to receive a jumped timestamp on the first post-restart call.

Fix applied:
```c
sv.gameTime = sv.time;       // ← ADDED
sv.gameTimeResidual = 0;     // ← ADDED
SV_RestartGameProgs();
```

### Other Console Commands

| Command | Function | Notes |
|---|---|---|
| `map <name>` | `SV_Map_f()` | Load a new map |
| `devmap <name>` | `SV_Map_f()` | Load map with cheats enabled |
| `map_restart` | `SV_MapRestart_f()` | Restart current map |
| `killserver` | `SV_KillServer_f()` | Shutdown server |
| `status` | `SV_Status_f()` | Print connected clients |
| `clientkick <n>` | `SV_Kick_f()` | Kick client by slot |
| `kick <name>` | `SV_KickName_f()` | Kick client by name |
| `kickbots` | `SV_KickBots_f()` | Kick all bots |
| `banaddr <ip>` | `SV_AddIP_f()` | Add IP to ban list |
| `removeip <ip>` | `SV_RemoveIP_f()` | Remove from ban list |
| `listip` | `SV_ListIP_f()` | Show ban list |
| `sectorlist` | `SV_SectorList_f()` | Debug: dump world sector stats |
| `heartbeat` | `SV_Heartbeat_f()` | Force master server update |
| `serverinfo` | `SV_Serverinfo_f()` | Print SERVERINFO cvars |
| `systeminfo` | `SV_Systeminfo_f()` | Print SYSTEMINFO cvars |
| `tell <n> <msg>` | `SV_Tell_f()` | Private message |
| `say <msg>` | `SV_ConSay_f()` | Server broadcast |

---

## URT Server Additions [CUSTOM/URT]

These additions are all guarded by compile-time flags and integrate UrbanTerror-specific server functionality.

### Auth System (sv_init.c, sv_main.c, sv_client.c, server.h) [USE_AUTH]

The engine provides hooks for an external auth server. The auth system is primarily QVM-side; the engine provides socket-level forwarding only.

#### Cvars registered (sv_init.c)

| Cvar | Flags | Notes |
|---|---|---|
| `sv_authServerIP` | `CVAR_TEMP\|CVAR_ROM` | Auth server IP address |
| `sv_auth_engine` | `CVAR_ROM` | Auth engine version (read by QVM) |

#### server.h additions

`client_t` gains:
```c
#ifdef USE_AUTH
    char    auth[MAX_NAME_LENGTH];   // auth token for this client
#endif
```

#### SV_Auth_DropClient(client_t *drop, const char *reason, const char *message)

Defined in `sv_client.c`. Sends separate `reason` (for log) and `message` (for client display) strings, then calls `SV_DropClient`. Used by the auth server packet handler when a player fails authentication.

### Server Demo Recording (sv_ccmds.c, sv_snapshot.c, sv_client.c, server.h) [USE_SERVER_DEMO]

Records server-side demos — captures the exact stream sent to each client. Demos can be played back in any client.

#### server.h additions

`client_t` gains:
```c
#ifdef USE_SERVER_DEMO
    qboolean    demo_recording;   // currently recording this client?
    fileHandle_t demo_file;       // open file handle
    qboolean    demo_waiting;     // waiting for first keyframe
    int         demo_backoff;     // exponential backoff counter
    playerState_t demo_deltas;    // previous frame for delta compression
#endif
```

Extern cvars:
```c
#ifdef USE_SERVER_DEMO
extern cvar_t *sv_demonotice;
extern cvar_t *sv_demofolder;
#endif
```

#### sv_ccmds.c functions

| Function | Description |
|---|---|
| `SVD_StartDemoFile(client, name)` | Open file, write gamestate header + configstrings + baselines |
| `SVD_WriteDemoFile(client, msg)` | Per-snapshot recording with svc_EOF append |
| `SVD_StopDemoFile(client)` | Write trailer markers, close file |
| `SVD_CleanPlayerName(name, out)` | Sanitize player name for filesystem use |
| `SV_StartRecordOne(client, name)` | Start recording one client |
| `SV_StartRecordAll(name)` | Start recording all clients |
| `SV_StopRecordOne(client)` | Stop recording one client |
| `SV_StopRecordAll()` | Stop recording all clients |
| `SV_StartServerDemo_f()` | Console command: `startserverdemo [client\|all] [name]` |
| `SV_StopServerDemo_f()` | Console command: `stopserverdemo [client\|all]` |

Demo files are written to `sv_demofolder` (default: `serverdemos/`). Filenames are sanitized player names with timestamp.

#### USE_URT_DEMO conditional

When `USE_URT_DEMO` is also defined, demo files use:
- `.urtdemo` extension instead of `.dm_68`
- URT protocol header (modversion + protocol + 2 reserved ints)
- Backward-play size markers for URT4 demo playback compat

#### sv_snapshot.c hook

`SV_SendMessageToClient` calls `SVD_WriteDemoFile(client, msg)` after snapshot is built and before netchan transmit. Uses exponential backoff on the `demo_waiting` flag to force full keyframes at start of recording.

### Client Screenshot (sv_ccmds.c, cl_cgame.c) [USE_FTWGL]

Allows the server admin to request a screenshot from a connected client.

#### sv_ccmds.c: SV_ClientScreenshot_f()

Registered as `clientScreenshot` server console command. Usage: `clientScreenshot <clientNum>`.
Sends `clientScreenshot [filename]` as a reliable server command to the specified client.

#### cl_cgame.c: clientScreenshot handler

`CL_CgameSystemCalls` intercepts the `clientScreenshot` server command and calls `screenshot silent [filename]` locally. Guarded by `#ifdef USE_FTWGL`.

### Updated Console Commands (sv_ccmds.c)

| Command | Function | Notes |
|---|---|---|
| `startserverdemo [client\|all] [name]` | `SV_StartServerDemo_f()` | Start server-side demo recording |
| `stopserverdemo [client\|all]` | `SV_StopServerDemo_f()` | Stop server-side demo recording |
| `clientScreenshot <clientNum>` | `SV_ClientScreenshot_f()` | Request screenshot from client |

### Auth and Demo Cvars [URT]

#### Auth Cvars (sv_init.c, USE_AUTH)

| Cvar | Default | Flags | Notes |
|------|---------|-------|-------|
| `sv_authServerIP` | "" | `CVAR_TEMP\|CVAR_ROM` | Auth server address |
| `sv_auth_engine` | "1" | `CVAR_ROM` | Auth engine version |

#### Demo Cvars (sv_ccmds.c, USE_SERVER_DEMO)

| Cvar | Default | Notes |
|------|---------|-------|
| `sv_demonotice` | "" | Message to send to client on auto-record start |
| `sv_demofolder` | "serverdemos" | Directory for demo files |

---

## sv_bot.c — Bot Integration

**File:** `code/server/sv_bot.c`  
**Size:** ~450 lines

Provides the bridge between the server and the Botlib AI library.

### SV_BotAllocateClient()

Called from QVM via `G_BOT_ALLOCATE_CLIENT`. Allocates a client_t slot with `netchan.remoteAddress.type = NA_BOT`. Bot clients never receive snapshots over the network.

### SV_BotFreeClient(clientNum)

Frees a bot client slot.

### BotImport_* Functions

The Botlib expects a table of engine functions (`botlib_import_t`). These wrappers translate Botlib calls to engine calls:

| BotImport function | Engine call |
|---|---|
| `BotImport_Trace` | `SV_Trace` |
| `BotImport_EntityTrace` | `SV_ClipToEntity` |
| `BotImport_PointContents` | `SV_PointContents` |
| `BotImport_inPVS` | `SV_inPVS` |
| `BotImport_GetMemory` | `Z_TagMalloc(TAG_BOTLIB)` |
| `BotImport_FreeMemory` | `Z_Free` |
| `BotImport_HunkAlloc` | `Hunk_Alloc` |
| `BotImport_DebugPolygon*` | Debug visualization |

---

## sv_net_chan.c — Server Netchan

**File:** `code/server/sv_net_chan.c`  
**Size:** ~300 lines

### SV_Netchan_Transmit(client_t *cl, msg_t *msg)

Encodes and sends a packet to a client. Handles:
- sv_packetdelay simulation
- Rate limiting: updates `cl->rate` tracking
- Fragmented sends via `Netchan_TransmitNextFragment` for large messages

### SV_Netchan_Process(client_t *cl, msg_t *msg)

Receives and validates a packet from a client:
- Calls `Netchan_Process` for sequence validation
- Updates `cl->lastPacketTime`
- Returns false for out-of-order/duplicate packets

### SV_Netchan_FreeQueue(client_t *cl)

Drains the netchan fragment queue (used when disconnecting).

---

## sv_filter.c — IP Filter/Ban System

**File:** `code/server/sv_filter.c`  
**Size:** ~1100 lines

A full expression-based IP filter system (more powerful than vanilla Q3's simple IP list):
- Supports `AND`, `OR`, `NOT` logic expressions
- Time-based bans (`+1h`, `+7d`, etc.)
- TLD (country code) filtering
- Ban file loading from `filter.txt`

### Key Functions

| Function | Notes |
|---|---|
| `SV_LoadFilters(filename)` | Load/reload ban file |
| `SV_AddFilter_f()` | Console command: add a filter rule |
| `SV_AddFilterCmd_f()` | Console command: more complex filter |
| `SV_IsBanned(netadr, isException)` | Check if address is banned |

Filters are evaluated as an expression tree. The IP and TLD are tested against each node.

---

## sv_rankings.c — Statistics Tracking

**File:** `code/server/sv_rankings.c`  
**Size:** ~1537 lines

Optional statistics/rankings server integration. Tracks kills, deaths, damage dealt, etc. per client. Data can be submitted to an external rankings server.

Not critical for gameplay — disabled by default (`sv_rankings 0`).

---

## Key Data Structures (server.h)

### server_t sv — Per-Map State

```c
typedef struct {
    serverState_t   state;          // SS_DEAD, SS_LOADING, SS_GAME
    int             serverId;
    int             checksumFeed;
    int             snapshotCounter;
    int             timeResidual;   // tick accumulator for sv_fps loop
    char            *configstrings[MAX_CONFIGSTRINGS];
    svEntity_t      svEntities[MAX_GENTITIES];
    sharedEntity_t  *gentities;     // QVM entity array pointer
    int             gentitySize;    // sizeof(gentity_t) in QVM
    int             num_entities;
    playerState_t   *gameClients;   // QVM client array pointer
    int             gameClientSize;
    int             time;           // engine tick time
    int             gameTime;       // [CUSTOM] QVM-visible level.time (sv_gameHz rate)
    int             gameTimeResidual; // [CUSTOM] accumulator for gameTime sub-tick
    byte            baselineUsed[MAX_GENTITIES];
} server_t;
```

### serverStatic_t svs — Persistent State

```c
typedef struct {
    qboolean    initialized;
    int         time;               // same as sv.time but persists across maps
    int         snapFlagServerBit;  // XORed into snapFlags
    client_t    *clients;           // [sv_maxclients] array
    int         numSnapshotEntities;
    entityState_t *snapshotEntities; // entity state pool
    int         nextHeartbeatTime;
} serverStatic_t;
```

### client_t — Per-Client State

Key fields (see full definition in server.h):

```c
typedef struct client_s {
    clientState_t  state;            // CS_FREE through CS_ACTIVE
    char           userinfo[MAX_INFO_STRING];
    char           reliableCommands[MAX_RELIABLE_COMMANDS][MAX_STRING_CHARS];
    int            reliableSequence;
    int            reliableAcknowledge;
    usercmd_t      lastUsercmd;      // [CUSTOM] last received usercmd
    sharedEntity_t *gentity;         // pointer into sv.gentities
    int            ping;             // measured round-trip time (ms)
    int            rate;             // bytes/sec cap
    int            snapshotMsec;     // [CUSTOM] snapshot send interval
    int            lastSnapshotTime; // svs.time of last snapshot sent
    netchan_t      netchan;          // netchan.remoteAddress.type == NA_BOT for bots
    clientSnapshot_t frames[PACKET_BACKUP]; // snapshot history for delta
    // ... download state, flood protection ...
    // URT additions:
    // #ifdef USE_AUTH:    auth[MAX_NAME_LENGTH]
    // #ifdef USE_SERVER_DEMO: demo_recording, demo_file, demo_waiting, demo_backoff, demo_deltas
} client_t;
```

---

## All Server Cvars

### Stock Cvars (sv_init.c)

| Cvar | Default | Flags | Description |
|------|---------|-------|-------------|
| `sv_maxclients` | 8 | SERVERINFO, LATCH | Max simultaneous players |
| `sv_hostname` | "noname" | SERVERINFO, ARCHIVE | Server display name |
| `sv_timeout` | 200 | TEMP | Seconds before auto-disconnect |
| `sv_zombietime` | 2 | TEMP | Seconds in zombie state after disconnect |
| `sv_pure` | 1 | SYSTEMINFO, LATCH | Enforce pak checksum matching |
| `sv_allowDownload` | 1 | SERVERINFO | Allow clients to download files |
| `sv_maxRate` | 0 | ARCHIVE | Max bytes/sec per client |
| `sv_minPing` | 0 | ARCHIVE, SERVERINFO | Min acceptable ping |
| `sv_maxPing` | 0 | ARCHIVE, SERVERINFO | Max acceptable ping |
| `sv_floodProtect` | 1 | ARCHIVE, SERVERINFO | Flood protection |
| `sv_dlRate` | 100 | ARCHIVE, SERVERINFO | Max download rate (KB/s) |

### Custom Cvars [CUSTOM] (sv_init.c)

| Cvar | Default | Notes |
|------|---------|-------|
| `sv_fps` | 60 | Engine tick + input rate |
| `sv_gameHz` | 20 | QVM GAME_RUN_FRAME rate |
| `sv_snapshotFps` | -1 | Snapshot send rate (-1=sv_fps, 0=client snaps) |
| `sv_pmoveMsec` | 8 | Max Pmove step size (8=125fps equivalent) |
| `sv_extrapolate` | 1 | Engine-side position extrapolation |
| `sv_smoothClients` | 1 | TR_LINEAR trajectory mode |
| `sv_bufferMs` | 0 | Ring buffer delay (0=off, -1=auto, 1-100=manual ms) |
| `sv_velSmooth` | 64 | Velocity smoothing window (ms); EWA alpha=0.5/step |
| `sv_busyWait` | 0 | Spin-wait last N ms per frame |
| `sv_antiwarp` | 0 | Engine antiwarp (0=off, 1=constant, 2=decay) |
| `sv_antiwarpTol` | 0 | Antiwarp tolerance ms (0=auto=gameMsec) |
| `sv_antiwarpExtra` | 0 | Mode 2 extrapolation window ms (0=auto=awTol) |
| `sv_antiwarpDecay` | 150 | Mode 2 decay duration ms |
| `sv_allowClientAdaptiveTiming` | 1 | SERVERINFO, ARCHIVE. Allow connected clients to use `cl_adaptiveTiming`. Set to `0` to force all clients onto vanilla Q3e timing regardless of their local setting. Broadcast in SERVERINFO so clients read it from the gamestate on connect. |

> **Full antiwarp documentation:** See [`SV_ANTIWARP.md`](SV_ANTIWARP.md) for mode details, decay timeline, interaction with sv_antilag, and configuration profiles.

### Antilag Cvars [CUSTOM] (sv_antilag.c)

| Cvar | Default | Notes |
|------|---------|-------|
| `sv_antilag` | 0 | Engine antilag on/off (default off, set 1 to enable) |
| `sv_antilagMaxMs` | 200 | Max rewind window (ms) |
| `sv_antilagDebug` | 0 | Debug verbosity (0-2) |
| `sv_antilagRateDebug` | 0 | Print per-client snap rate |

---

## Complete Call Graph — Server Tick

```
Com_Frame(msec)
└── SV_Frame(msec)
    ├── SV_CalcPings()
    ├── SV_CheckCvars() → SV_TrackCvarChanges()
    │
    └── [tick loop: while timeResidual >= frameMsec]
        ├── sv.time++
        │
        ├── [game frame loop: while gameTimeResidual >= gameMsec]
        │   ├── SV_BotFrame(sv.gameTime)
        │   └── VM_Call(gvm, GAME_RUN_FRAME, sv.gameTime)
        │         └── QVM: level.time advances, entities update positions
        │               └── trap_Trace → G_TRACE → SV_Antilag_InterceptTrace
        │                   ├── SV_Antilag_RewindAll(shooter, fireTime)
        │                   ├── SV_Trace()  ← hit detection at rewound state
        │                   └── SV_Antilag_RestoreAll()
        │
        ├── SV_Antilag_RecordPositions()  ← AFTER game frame (shadow[T] == snapshot[T])
        │     └── for each active human client (bots excluded — FIFO antilag handles bot targets): record origin/absmin/absmax to shadow history
        │
        ├── [if ring buffer active]:
        │   SV_SmoothRecordAll()
        │     └── for each client: record origin+velocity to smooth ring buffer
        │
        ├── SV_IssueNewSnapshot()
        │   └── SV_BuildCommonSnapshot()
        │         └── for each player entity:
        │               Phase1: resolve position (buffer / ps->origin / extrapolated)
        │               Phase2: set TR_INTERPOLATE or TR_LINEAR with corrected position
        │
        └── SV_SendClientMessages()
            └── for each client where (svs.time - lastSnap) >= snapshotMsec:
                SV_SendClientSnapshot()
                  ├── SV_BuildClientSnapshot()  ← PVS culling
                  └── SV_WriteSnapshotToClient() ← delta encode + transmit
                SV_Antilag_NoteSnapshot(clientNum)
                #ifdef USE_SERVER_DEMO:
                  SVD_WriteDemoFile(cl, msg)  ← server demo recording
```
