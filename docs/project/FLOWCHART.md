# Quake3e-subtick System Flowchart

## 1. Server Main Loop (sv_main.c — SV_Frame)

```
  Com_Frame() calls SV_Frame(msec)
  |
  v
  +=============================================================+
  |                    SV_Frame( msec )                          |
  +=============================================================+
  |                                                             |
  |  SV_CheckTimeouts()          (once per Com_Frame)           |
  |  SV_TrackCvarChanges()       (clamp residuals, flush        |
  |                                buffers on cvar change)      |
  |                                                             |
  |  sv.timeResidual += msec                                    |
  |  frameMsec = 1000 / sv_fps                                  |
  |                                                             |
  |  +========================================================+ |
  |  | WHILE sv.timeResidual >= frameMsec    [sv_fps tick loop]| |
  |  |========================================================| |
  |  |                                                        | |
  |  |  sv.timeResidual -= frameMsec                          | |
  |  |  svs.time += frameMsec                                 | |
  |  |  sv.time  += frameMsec                                 | |
  |  |                                                        | |
  |  |  +--------------------------------------------------+  | |
  |  |  | GAME FRAME (sv_gameHz inner loop)                |  | |
  |  |  |                                                  |  | |
  |  |  | sv.gameTimeResidual += frameMsec                 |  | |
  |  |  | gameMsec = 1000 / sv_gameHz (or frameMsec if 0)  |  | |
  |  |  |                                                  |  | |
  |  |  | WHILE sv.gameTimeResidual >= gameMsec:           |  | |
  |  |  |   sv.gameTimeResidual -= gameMsec                |  | |
  |  |  |   sv.gameTime += gameMsec                        |  | |
  |  |  |                                                  |  | |
  |  |  |   [ENGINE ANTIWARP — sv_antiwarp 1 or 2]         |  | |
  |  |  |   for each active non-bot client:               |  | |
  |  |  |     awGap = sv.gameTime - awLastThinkTime       |  | |
  |  |  |     if awGap > sv_antiwarpTol (auto=gameMsec):  |  | |
  |  |  |       lastUsercmd.serverTime += gameMsec        |  | |
  |  |  |       mode 2 phases:                            |  | |
  |  |  |         gap <= tol+extra: keep inputs (extrap)  |  | |
  |  |  |         gap <= tol+extra+decay: fade to zero    |  | |
  |  |  |         gap > that: zero inputs (friction)      |  | |
  |  |  |       VM_Call(GAME_CLIENT_THINK, i)             |  | |
  |  |  |       awLastThinkTime = sv.gameTime             |  | |
  |  |  |                                                  |  | |
  |  |  |   SV_BotFrame( sv.gameTime )                     |  | |
  |  |  |   VM_Call( gvm, GAME_RUN_FRAME, sv.gameTime )    |  | |
  |  |  +--------------------------------------------------+  | |
  |  |                                                        | |
  |  |  +--------------------------------------------------+  | |
  |  |  | ANTILAG RECORDING (after game frame)             |  | |
  |  |  | SV_Antilag_RecordPositions()                     |  | |
  |  |  |                                                  |  | |
  |  |  | Records each active player's position into       |  | |
  |  |  | per-client ring buffer at sv_fps Hz.             |  | |
  |  |  | Runs AFTER the game frame so shadow[T] holds     |  | |
  |  |  | post-game positions == what snapshot[T] shows.   |  | |
  |  |  | Only runs if game frame executed this tick        |  | |
  |  |  | (_gameFrameRan guard — prevents duplicate        |  | |
  |  |  | entries when sv_gameHz > 0 skips a tick).        |  | |
  |  |  | Human clients only — bots excluded (FIFO        |  | |
  |  |  | antilag handles bot targets; including bots      |  | |
  |  |  | creates a double-rewind conflict).               |  | |
  |  |                                                        | |
  |  |  +--------------------------------------------------+  | |
  |  |  | PER-TICK SNAPSHOT DISPATCH                       |  | |
  |  |  |                                                  |  | |
  |  |  | SV_IssueNewSnapshot()                            |  | |
  |  |  | SV_SendClientMessages()                          |  | |
  |  |  |                                                  |  | |
  |  |  | Each sv_fps tick dispatches its own snapshot.     |  | |
  |  |  | (Old: once per Com_Frame → OS jitter caused      |  | |
  |  |  | double-tick gaps. Now: true 60Hz delivery.)      |  | |
  |  |  +--------------------------------------------------+  | |
  |  |                                                        | |
  |  +========================================================+ |
  +=============================================================+
```

## 2. Clock System

```
  === sv_gameHz 0 (default, recommended for UT 4.3.4) ===

  sv_fps = 60 — single unified clock
  +--------------------------------------------------+
  |  sv.time == sv.gameTime  (always equal)           |
  |  Both advance every 16.6ms (= 1000/sv_fps)       |
  |                                                   |
  |  Drives everything:                               |
  |    - Usercmd processing                           |
  |    - GAME_RUN_FRAME (every tick)                  |
  |    - Bot AI (SV_BotFrame)                         |
  |    - Antilag recording                            |
  |    - Antiwarp injection (sv_antiwarp)             |
  |    - Snapshot building + dispatch                 |
  +--------------------------------------------------+

  tick: |--16ms--|--16ms--|--16ms--|--16ms--|--16ms--|-->
  game: |--16ms--|--16ms--|--16ms--|--16ms--|--16ms--|-->
         ^ game frame fires every tick — positions always fresh
         ^ sv_extrapolate/sv_bufferMs are no-ops
         ^ sv_antiwarp uses gameMsec = 16ms steps

  sv_fps can be raised freely (60, 80, 125, etc.) when:
    - sv_antiwarp is on (1 or 2) — engine antiwarp is frame-rate aware, OR
    - g_antiwarp is off — no QVM antiwarp constraint at all
  QVM g_antiwarp is broken at sv_gameHz 0 (hardcoded 50ms steps) —
  use sv_antiwarp 2 instead.

  === sv_gameHz 20 (legacy — UT 4.0-4.2 or QVM g_antiwarp) ===

  sv_fps = 60 (engine tick)          sv_gameHz = 20 (QVM game rate)
  +-----------------------------+    +-------------------------------+
  |  sv.time                    |    |  sv.gameTime                  |
  |  Advances every 16.6ms     |    |  Advances every 50ms          |
  |  Drives:                   |    |  Drives:                      |
  |    - Usercmd processing    |    |    - GAME_RUN_FRAME           |
  |    - Antilag recording     |    |    - Bot AI (SV_BotFrame)     |
  |    - Snapshot building     |    |    - level.time in QVM        |
  +-----------------------------+    +-------------------------------+

  tick: |--16ms--|--16ms--|--16ms--|--16ms--|--16ms--|--16ms--|-->
  game:                   |------50ms------|------50ms------|-->
                           ^ 3 ticks between game frames
                           ^ sv_extrapolate essential (fixes stale positions)
                           ^ sv_bufferMs useful (smooths 20Hz gaps)

  sv_fps can be raised freely here too — sv_gameHz 20 locks the GAME
  frame rate, not the engine tick rate. sv_fps only affects input
  sampling, antilag, and snapshot cadence. QVM g_antiwarp fires at
  20Hz regardless of sv_fps, so the 50ms injection is always correct.

  Why sv_gameHz 20 (legacy only):
  - QVM g_antiwarp hardcodes serverTime += 50 (assumes 50ms game frames)
  - UT 4.0-4.2: game code broadly intolerant of non-20Hz rates
  - Use sv_antiwarp 1 or 2 instead to eliminate the sv_gameHz 20 constraint

  See docs/project/SV_GAMEHZ.md for the full legacy reference.
```

## 3. Multi-Step Pmove (sv_client.c)

```
  Client sends usercmd with delta = 48ms
  sv_pmoveMsec = 8ms
  |
  v
  +----------------------------------------------------------+
  |  SV_ClientThink( cl, &cmd )                               |
  +----------------------------------------------------------+
  |                                                          |
  |  total_delta = cmd.serverTime - cl->lastUsercmd.serverTime|
  |  (e.g. 48ms)                                             |
  |                                                          |
  |  Is this a bot?  ----YES---->  goto single_call           |
  |       |                        (bots use 50ms steps,      |
  |       NO                        clamping breaks them)     |
  |       |                                                   |
  |       v                                                   |
  |  remaining = 48ms                                         |
  |                                                          |
  |  +-----------------------------------------------------+ |
  |  | LOOP while remaining > sv_pmoveMsec:                 | |
  |  |                                                     | |
  |  |  step_cmd = cmd                                     | |
  |  |  step_cmd.serverTime = last + 8ms                   | |
  |  |                                                     | |
  |  |  VM_Call( GAME_CLIENT_THINK, step_cmd )             | |
  |  |                                                     | |
  |  |  FIRST CALL CHECK: did commandTime advance?         | |
  |  |    NO (intermission/pause) --> goto single_call     | |
  |  |    YES --> continue loop                            | |
  |  |                                                     | |
  |  |  remaining -= 8ms                                   | |
  |  +-----------------------------------------------------+ |
  |                                                          |
  |  Final call with remaining delta (0-8ms)                  |
  |  VM_Call( GAME_CLIENT_THINK, cmd )   [original cmd]       |
  |                                                          |
  |  Result: 48ms delta = 6 QVM calls of 8ms each            |
  |  QVM sees 8ms per call -> bleed/bandage timers tick       |
  |  correctly (not 6x too fast from one 48ms call)           |
  +----------------------------------------------------------+
```

## 4. Snapshot Building & Visual Smoothing (sv_snapshot.c)

```
  SV_BuildCommonSnapshot()
  |
  v
  For each client entity (player slot):
  |
  +-- Is it a BOT? --YES--> Use raw entity state from BG_PlayerStateToEntityState
  |                         No prediction, no smoothing.
  |                         At sv_gameHz 0: updates every tick (fine).
  |                         At sv_gameHz 20: updates at 20Hz only —
  |                           cgame lerps between game-frame snapshots.
  |
  +-- Is it a REAL PLAYER?
      |
      v
      Read actual playerState_t from game module:
        ps = SV_GameClientNum( entityNum )
        origin = ps->origin      (true post-Pmove position)
        velocity = ps->velocity  (current velocity)
      |
      |  At sv_gameHz 0: same value BG_PlayerStateToEntityState wrote
      |    (harmless redundancy — positions already fresh every tick).
      |  At sv_gameHz 20: this is the key fix — BG_PlayerStateToEntityState
      |    only runs at 20Hz, but ps->origin updates every usercmd.
      |
      v
      +-- sv_bufferMs > 0?  (position delay mode)
      |     |
      |     YES --> SV_SmoothGetPosition( time - bufferMs )
      |             Read delayed position from ring buffer.
      |             Adds latency but smooths jitter.
      |
      +-- sv_smoothClients 1?  (TR_LINEAR velocity mode)
      |     |
      |     YES --> SV_SmoothGetAverageVelocity( sv_velSmooth window )
      |             Exponential weighted average (alpha=0.5/step) over ring buffer.
      |             Set entity to TR_LINEAR with EWA velocity.
      |             No latency cost, cgame evaluates trajectory continuously.
      |             EWA prevents uniform-cancel edge cases:
      |               - Rapid L/R strafe: alternating ±v cancels to 0 uniformly,
      |                 EWA yields non-zero (unequal weights) → stays TR_LINEAR.
      |               - Jump in place (no fwd vel): Z near 0 at peak but older
      |                 non-zero samples retain partial weight → stays TR_LINEAR.
      |
      +-- Neither? --> Write ps->origin directly into entity state.
                       TR_INTERPOLATE. Standard behavior.
```

## 5. Smooth Ring Buffer (sv_snapshot.c)

```
  Per-client ring buffer: 32 slots (SV_SMOOTH_MAX_SLOTS)

  SV_SmoothRecord() — called every sv_fps tick for active players
  |
  +---> slots[ head % 32 ] = { origin, velocity, time }
        head++
        (safe double-mod: ((head % N + N) % N) handles int overflow)

  SV_SmoothGetPosition( targetTime )     SV_SmoothGetAverageVelocity( windowMs )
  |                                       |
  v                                       v
  Walk backwards from head,               Exponential weighted average (EWA)
  find two slots bracketing               of velocity entries in window.
  targetTime, lerp between them.          alpha=0.5/step: newest ~53% weight.
  Returns delayed position.               Fixes two flip-flop edge cases:
                                          - Rapid L/R strafe: ±v alternation
                                            only partially cancels in EWA
                                            (uniform average gives exactly 0).
                                          - Jump peak (no fwd vel): older
                                            non-zero Z samples retain weight,
                                            keeping average above idle threshold.

  Ring buffer only records when:
  - (sv_bufferMs > 0 OR sv_velSmooth > 0) AND sv_extrapolate is on
  - Client is a real player (not bot)
```

## 6. Antilag System (sv_antilag.c)

```
  RECORDING (once per sv_fps tick):

  SV_Antilag_RecordPositions()
  |
  for each active human client (bots excluded — FIFO antilag handles bot targets):
    shadowHistory[client].slots[ head % historySlots ] = {
      origin (from entity state),
      mins/maxs (bounding box),
      time (sv.time)
    }
    head++

  History depth: sv_fps * windowMs / 1000
  (e.g. 60 * 300ms / 1000 = 18 slots per second, ~300ms covered)

  REWINDING (on weapon trace):

  trap_Trace() from QVM
  |
  v
  SV_Antilag_InterceptTrace()
  |
  +-- contentmask has CONTENTS_PLAYERCLIP? --YES--> Skip (movement trace)
  |                                                 (Prevents Pmove from
  |                                                  colliding with rewound
  |                                                  historical positions)
  +-- NO (weapon trace, MASK_SHOT)
      |
      v
      For each client in the trace path:
        targetTime = attacker's commandTime - their ping
        SV_Antilag_GetPositionAtTime( victim, targetTime )
        |
        v
        Walk ring buffer, find bracketing slots,
        lerp position at exact targetTime.
        Temporarily move victim hitbox to historical position.
      |
      v
      Run trace against rewound positions.
      Restore all positions.
```

## 7. Client Time Sync (cl_cgame.c — CL_AdjustTimeDelta)

```
  On each new snapshot arrival:

  newDelta = cl.snap.serverTime - cls.realtime
  deltaDelta = abs( newDelta - cl.serverTimeDelta )
  |
  v
  +-- deltaDelta > RESET_TIME (snapshotMsec * 10, min 200ms)?
  |     |
  |     YES --> HARD RESET
  |             cl.serverTimeDelta = newDelta
  |             cl.serverTime = cl.snap.serverTime
  |             slowFrac = 0  (clear stale accumulator)
  |
  +-- deltaDelta > fastAdjust (snapshotMsec * 2)?
  |     |
  |     YES --> FAST CORRECTION
  |             cl.serverTimeDelta = average(old, new)
  |             slowFrac = 0  (clear stale accumulator)
  |
  +-- else: SLOW DRIFT (fractional accumulator)
        |
        v
  +================================================================+
  |  FRACTIONAL ACCUMULATOR (eliminates ±1ms oscillation)          |
  |================================================================|
  |                                                                |
  |  Units: 4 = 1ms.  Steps are ½ms (2 units) or 1ms (4 units).  |
  |                                                                |
  |  if extrapolatedSnapshot:                                      |
  |      slowFrac -= 2   (high rate, snapshotMsec < 30)           |
  |      slowFrac -= 4   (low rate, snapshotMsec >= 30)           |
  |  else:                                                         |
  |      slowFrac += 2   (always ½ms)                              |
  |                                                                |
  |  if slowFrac >= +4:  commit (see below), slowFrac -= 4         |
  |  if slowFrac <= -4:  commit (see below), slowFrac += 4         |
  |                                                                |
  |  COMMIT STEP (cl_adaptiveTiming mode):                         |
  |    mode 0-1: step = 1ms always (conservative, jitter-safe)    |
  |    mode 2:   step = deltaDelta/4 when deltaDelta > snapshotMsec|
  |              (25% of error, capped at 50%. Proportional        |
  |               recovery: 0.3s vs 1.3s for 40ms disturbance.    |
  |               Trades jitter resistance for recovery speed.)    |
  |                                                                |
  |  EQUILIBRIUM (net change = 0 per second):                      |
  |                                                                |
  |    60Hz: -2*E + 2*(60-E) = 0  -->  E = 30  -->  50% extrap    |
  |    20Hz: -4*E + 2*(20-E) = 0  -->  E = 6.7 -->  33% extrap    |
  |                                                                |
  |  At equilibrium, slowFrac oscillates in [-2, +2] range,        |
  |  NEVER reaching ±4 commit threshold.                           |
  |  serverTimeDelta stays perfectly stable.                       |
  |                                                                |
  |  OLD BEHAVIOR (direct ±1ms):                                   |
  |    Every snap committed. Alternating +1/-1 = ping oscillation. |
  |                                                                |
  +================================================================+
```

## 8. Extrapolation Detection (cl_cgame.c)

```
  Every client render frame (CL_SetCGameTime):

  diff = cls.realtime + cl.serverTimeDelta - cl.snap.serverTime
       = (how far cl.serverTime is past the latest snapshot)

  cl_adaptiveTiming 1:
    extrapolateThresh = snapshotMsec / 3  (clamped [3, 16])
    60Hz: thresh = 5ms     20Hz: thresh = 16ms

    if (diff * 4 + slowFrac >= -(thresh * 4)):
      cl.extrapolatedSnapshot = true
      (We're within thresh ms of the snapshot edge — close enough
       to count as "extrapolating" for the feedback loop)

  cl_adaptiveTiming 0 (vanilla):
    Hardcoded 5ms threshold (same formula, thresh=5 always)

  serverTime cap (adaptive only):
    cl.serverTime capped at cl.snap.serverTime + snapshotMsec
    Prevents frameInterpolation > 1.0 overshoot.

  Timeline visualization (60Hz, 16ms intervals):

  prevSnap        cl.snap (latest)         next snap (future)
    |                 |                         |
    |----16ms---------|--------16ms-------------|
    |                 |    ^         ^           |
    |  INTERPOLATING  | thresh=5ms  |           |
    |  (green zone)   |<---------->|           |
    |                 | extrap detection zone   |
    |                 |             |           |
    |            cl.serverTime     |           |
    |            (targeted here    |           |
    |             at ~50% mark)    |           |
```

## 9. Net Monitor Widget (cl_scrn.c)

```
  Data Collection: SCR_NetMonUpdate()
  Runs EVERY FRAME at CA_ACTIVE (regardless of cl_netgraph)
  Aggregates into 1-second buckets:
  |
  +---> Frame time (min/avg/max)
  +---> Snap gap (interval between snapshots)
  +---> Ping (min/avg/max)
  +---> Bandwidth (in/out bytes)
  +---> Drop count, extrap count, cap hits
  +---> Slow drift commits, fast resets
  +---> Choke count (SNAPFLAG_RATE_DELAYED)
  +---> frameInterpolation average
  |
  v
  Every 1 second: snapshot into display variables
  |
  +-- cl_netgraph 1? --YES--> Record into 30-second ring buffer
  |                           (for laggot announce)
  |
  v
  Widget Display (10 rows, only when cl_netgraph 1):

  +---------------------------+
  | Snap: 60Hz ping:32ms      |  Measured snap rate + current ping
  | Ping: 28..36ms            |  Min..max ping over last second
  | FrmI:0.48 FrmT:8/11ms    |  Interpolation fraction + frame time
  | DeltaT: +32ms             |  Current serverTimeDelta
  | Drop:0 Ext:30 Clp:0      |  Drops, extrapolations, clamps
  | I:4.2KB/s O:1.1KB/s      |  Bandwidth
  | SnapJitt:1/3ms            |  Arrival jitter (avg/max)
  | Seq: #48231 d:48230       |  Sequence number + delta base
  | Adj: slo=2 fst=0         |  Slow drift commits + fast resets
  +---------------------------+

  Color thresholds (scaled to snapshotMsec):
    Ext:   Green < 60% snapHz | Yellow 60-75% | Red > 75%
           (50% is normal equilibrium at high rates — sits in green)
    Slow:  Green < 10% snapHz | Yellow > 10%  | Red = fst > 0
    Drop:  Any > 0 = Red
    Jitt:  Red if max > snapMs | Yellow if avg > half

  cl_adaptiveTiming modes:
    0 = vanilla thresholds (hardcoded). slowFrac accumulator active.
    1 = snapshotMsec-scaled thresholds. 1ms commits. (default)
    2 = proportional. Commits scale to 25% of error when large.
        Best on stable wired connections.
```

## 10. Laggot Announce (cl_scrn.c)

```
  GATED: cl_netgraph 1 AND cl_laggotannounce 1
  (Both ring buffer recording and announce require cl_netgraph 1.
   No data accumulates when cl_netgraph is off.)

  30-second ring buffer stores per-second worst values:
    { dropRate, fastCnt, snapGapAvg, snapGapMax,
      extrapCnt, ftAvg, chokeCnt }

  Every 30 seconds (cooldown):
  |
  v
  Scan ring buffer for worst values across all 30 samples
  |
  v
  Check triggers (any fires = send message):
  +-- Drop > 0?                        "Drop:N/s"
  +-- FastRst > 0?                     "FastRst:N"
  +-- SnapJitt max > snapshotMsec?     "SnapJitt:avg/maxms"
  +-- Ext > 75% snapHz?                "Ext:N/Nhz"
  +-- LowFPS < half snap rate?         "LowFPS:Nfps"
  +-- Choke > 0?                       "Choke:N"
  |
  v
  CL_AddReliableCommand( "say [NET] Drop:2/s SnapJitt:4/18ms ..." )
```

## 11. Rate Override (sv_client.c, sv_init.c)

```
  Problem: UT QVM force-clamps client rate to 32000 bps
           At 60Hz snapshots: 32000 / (avg_snap_size) ≈ 21 snaps/sec
           Server chokes 60% of snapshots!

  Fix chain:
  +------------------------------------------------------------------+
  | sv_minRate = 500000  (CVAR_PROTECTED — QVM can't override)       |
  | sv_maxRate           (CVAR_PROTECTED)                             |
  | client rate default = 90000                                       |
  | client snaps default = 60                                         |
  +------------------------------------------------------------------+

  Rate clamping order (SV_RateMsec):
    1. Client's rate cvar (set by QVM to 32000)
    2. sv_maxRate caps it from above
    3. sv_minRate overrides from below  <-- always wins
    Result: effective rate = max(sv_minRate, min(client, sv_maxRate))
           = max(500000, min(32000, ...)) = 500000
```

## 12. Full Frame Pipeline (end-to-end)

```
  CLIENT                          NETWORK              SERVER
  ======                          =======              ======

  Player input (mouse/keys)                            Bots: SV_BotFrame()
       |                                                     |
       v                                                     v
  Build usercmd                                        GAME_RUN_FRAME
  (cl_maxpackets rate)                                 (every sv_fps tick at
       |                                                sv_gameHz 0)
       v                                                     |
  =============>  usercmd packet  =============>             |
                                                             v
                                               sv_antiwarp: inject blank cmds
                                               for lagging clients (mode 2:
                                               extrapolate → decay → friction)
                                                     |
                                                     v
                                               SV_ClientThink()
                                               Multi-step Pmove
                                               (sv_pmoveMsec chunks)
                                                     |
                                                     v
                                               playerState updated
                                               (true position every usercmd)
                                                     |
                                                     v
                                               Antilag records position
                                               (sv_fps Hz, all clients)
                                                     |
                                                     v
                                               SV_BuildCommonSnapshot()
                                               - Read ps->origin (real pos)
                                               - sv_smoothClients: TR_LINEAR
                                               - sv_velSmooth: avg velocity
                                               - Bot: raw entity state
                                                     |
                                                     v
  <=============  snapshot packet  <=============  SV_SendClientSnapshot()
       |                                          (every sv_fps tick)
       v
  CL_ParseSnapshot()
  - Measure snapshotMsec (EMA)
  - Count choke (SNAPFLAG_RATE_DELAYED)
       |
       v
  CL_SetCGameTime()
  - Advance cl.serverTime
  - Cap at snap.serverTime + snapshotMsec
  - Detect extrapolation
       |
       v
  CL_AdjustTimeDelta()  (on new snapshot)
  - RESET / FAST / slow drift
  - Fractional accumulator
       |
       v
  cgame renders frame
  - Interpolates between prev/current snap
  - frameInterpolation = 0.0 .. ~0.94
       |
       v
  Screen
```
