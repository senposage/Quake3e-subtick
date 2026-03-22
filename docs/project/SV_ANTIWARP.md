# sv_antiwarp — Engine-Side Antiwarp System

> **Quick answer:** Use `sv_antiwarp 2` with `sv_antiwarpDecay 150`. This replaces QVM `g_antiwarp`, works at any `sv_gameHz`, and eliminates the 50ms-hardcoded-step constraint.

---

## Table of Contents

1. [What the Problem Is](#what-the-problem-is)
2. [QVM g_antiwarp and Why It Breaks](#qvm-g_antiwarp-and-why-it-breaks)
3. [Engine Antiwarp — Design](#engine-antiwarp--design)
4. [Mode 1 — Constant](#mode-1--constant)
5. [Mode 2 — Decay (Recommended)](#mode-2--decay-recommended)
6. [Implementation in sv_main.c](#implementation-in-sv_mainc)
7. [map_restart awLastThinkTime bug](#map_restart-awlastthinktimebug)
8. [Cvars Reference](#cvars-reference)
9. [Interaction with sv_pmoveMsec](#interaction-with-sv_pmovemsec)
10. [Interaction with sv_antilag](#interaction-with-sv_antilag)
11. [Configuration Profiles](#configuration-profiles)
12. [Timing Diagram](#timing-diagram)

---

## What the Problem Is

In Quake 3 / Urban Terror, packet loss and network jitter cause a client's
usercmd stream to arrive late or in bursts. Without any antiwarp mechanism,
a lagging player stops receiving `GAME_CLIENT_THINK` calls for the duration
of the gap — then resumes as if no time passed. The result from other
players' perspective: the lagging player appears to **teleport** forward,
skipping over the space they would have occupied during the lag spike.

Example: a player is lagging for 200 ms at `sv_fps 60` / `sv_gameHz 20`.

```
Game frame 0 (t=0):    GAME_CLIENT_THINK fires — player at X=100
Game frame 1 (t=50):   no packets arrived     — ClientThink SKIPPED
Game frame 2 (t=100):  no packets arrived     — ClientThink SKIPPED
Game frame 3 (t=150):  no packets arrived     — ClientThink SKIPPED
Game frame 4 (t=200):  packet burst arrives   — GAME_CLIENT_THINK fires — player at X=180
```

The 80-unit jump is invisible to other players until the snapshot at
t=200 arrives. The player has effectively warped.

---

## QVM g_antiwarp and Why It Breaks

The UrbanTerror QVM includes `g_antiwarp` in `g_active.c:G_RunClient`:

```c
// QVM source (g_active.c)
if ( level.time - ent->client->pers.lastCmdTime > g_antiWarpTol.integer ) {
    ent->client->pers.cmd.serverTime += 50;   // ← hardcoded 50ms step
    level.time = ent->client->pers.cmd.serverTime;
    ClientThink_real( ent );
}
```

It synthesises a fake usercmd with `serverTime += 50`, forcing a
`ClientThink` call to keep the player physically present.

**Why it breaks at `sv_gameHz 0` / non-20Hz:**

`serverTime += 50` assumes game frames are exactly 50 ms (20 Hz). At
`sv_fps 60` with `sv_gameHz 0`, game frames are 16 ms:

```
awTol = 50 ms
Every 16ms tick: sv_gameHz 0 → gap = 16ms → 16 < 50 → no injection
After 50ms (3 ticks): gap = 50ms → injection fires → adds 50ms step
                       gap = 50ms → injection fires again next tick!
                       gap = 50ms → fires again!
```

The 50ms step is 3× larger than the 16ms frame budget. The player's
`commandTime` leaps ahead of `sv.gameTime`, causing jump heights,
weapon timing, and strafejump arcs to break. At extreme `sv_fps` values
the server may crash the QVM with a runaway loop.

**Summary:** `g_antiwarp` with `sv_antiwarp 0` works **only** when
`sv_gameHz 20` (50ms game frames). For any other configuration, disable
`g_antiwarp` and use engine `sv_antiwarp` instead.

---

## Engine Antiwarp — Design

Engine antiwarp runs inside the `sv_gameHz` game-frame loop in
`SV_Frame`, **before** `GAME_RUN_FRAME`. It uses `_gameMsec` (the
actual game frame duration, e.g. 16 ms at sv_gameHz 60) instead of a
hardcoded 50 ms step.

```
For each active non-bot client:
    awGap = sv.gameTime - cl->awLastThinkTime

    if awGap > awTol:
        cl->lastUsercmd.serverTime += _gameMsec    ← frame-aware step
        [Mode 2 only]: scale or zero movement inputs based on awGap
        VM_Call(gvm, GAME_CLIENT_THINK, clientNum)
        cl->awLastThinkTime = sv.gameTime
```

`awLastThinkTime` is set to `sv.gameTime` when a **real** usercmd is
received (in `SV_ClientThink`). Engine antiwarp fires only when real
usercmds have stopped arriving — it is never triggered while packets are
flowing normally.

**Key invariant:** the engine calls `GAME_CLIENT_THINK` with the
synthesised usercmd before `GAME_RUN_FRAME`. The QVM's
`G_RunClient`/`ClientThink_real` runs the full Pmove, so the player
remains physically present in the world between real usercmds.

---

## Mode 1 — Constant

```
sv_antiwarp 1
```

Identical behaviour to QVM `g_antiwarp` but frame-rate aware:
- Keeps the last received movement inputs indefinitely
- Player continues moving at the same velocity as before the lag spike
- No input decay — the player coasts at constant velocity until real
  usercmds resume

**Good for:** minimal player disruption during short spikes. The player
keeps moving instead of stopping, which looks natural but can allow
exploiting lag to pass through areas they shouldn't reach.

---

## Mode 2 — Decay (Recommended)

```
sv_antiwarp 2
sv_antiwarpDecay 150
```

Three-phase response to a lag gap:

```
Phase 1 — Extrapolate  [0 … awTol + awExtraMs]
    Same as Mode 1: last inputs kept at full strength.
    Player continues at existing velocity.

Phase 2 — Decay        [awTol + awExtraMs … awTol + awExtraMs + awDecayMs]
    Movement inputs (forwardmove, rightmove, upmove) linearly scaled
    from 100% down to 0% over awDecayMs milliseconds.
    scale = 127 - (127 × elapsed / awDecayMs)
    Player decelerates progressively.

Phase 3 — Friction     [awTol + awExtraMs + awDecayMs … ∞]
    All movement inputs zeroed.
    Pmove's ground friction and air deceleration bring the player to a stop.
    Player remains physically present at the position they stopped at.
```

**Timeline example** (default settings: awTol=16ms, awExtraMs=16ms, awDecayMs=150ms):

```
Gap  0ms: real usercmds flowing — no injection
Gap 16ms: tolerance reached → Phase 1 starts (inject at full inputs)
Gap 32ms: extrapolation window ends → Phase 2 starts (inputs decaying)
Gap 182ms: decay window ends → Phase 3 (inputs zeroed, friction stops player)
Gap 300ms+: player stationary; resumes from that position when packets return
```

**Why decay is better than constant:**
- Prevents exploiting lag to maintain velocity through areas/players
- Stops the player cleanly without a jarring snap
- The 150ms decay window is long enough that short spikes (< 150ms) feel
  smooth, but deliberate lag exploitation is curtailed

---

## Implementation in sv_main.c

**Location:** `SV_Frame()` → inner game-frame loop, immediately before
`GAME_RUN_FRAME`.

```c
// sv_main.c ~line 1533

if ( sv_antiwarp && sv_antiwarp->integer ) {
    int awMode    = sv_antiwarp->integer;
    int awTol     = sv_antiwarpTol->integer > 0
                    ? sv_antiwarpTol->integer : _gameMsec;
    int awExtraMs = sv_antiwarpExtra->integer > 0
                    ? sv_antiwarpExtra->integer : awTol;
    int awDecayMs = sv_antiwarpDecay->integer > 0
                    ? sv_antiwarpDecay->integer : 0;

    for each active non-bot client (awCl):

        if ( awCl->awLastThinkTime == 0 )
            continue;   // ← skip: never received a real command yet

        awGap = sv.gameTime - awCl->awLastThinkTime;

        if ( awGap > awTol ) {
            awCl->lastUsercmd.serverTime += _gameMsec;   // ← frame-aware step

            if ( awMode >= 2 ) {
                int decayStart = awTol + awExtraMs;
                int elapsed    = awGap - decayStart;

                if ( elapsed > 0 ) {
                    if ( awDecayMs > 0 && elapsed < awDecayMs ) {
                        // Phase 2: linear decay
                        int scale = 127 - (127 * elapsed / awDecayMs);
                        awCl->lastUsercmd.forwardmove *= scale / 127;
                        awCl->lastUsercmd.rightmove   *= scale / 127;
                        awCl->lastUsercmd.upmove      *= scale / 127;
                    } else {
                        // Phase 3: zero inputs, let friction stop player
                        awCl->lastUsercmd.forwardmove = 0;
                        awCl->lastUsercmd.rightmove   = 0;
                        awCl->lastUsercmd.upmove      = 0;
                    }
                }
            }

            VM_Call( gvm, 1, GAME_CLIENT_THINK, awIdx );
            awCl->awLastThinkTime = sv.gameTime;
        }
    }
}
```

**`awLastThinkTime` is set** in `SV_ClientThink` (called by
`SV_UserMove`) when a real usercmd arrives:

```c
// sv_client.c (SV_ClientThink)
cl->awLastThinkTime = sv.gameTime;
```

It is **zeroed** in `SV_ClientEnterWorld` (client joins / spawns) so the
guard `awLastThinkTime == 0` correctly skips clients who haven't yet
sent their first usercmd.

It is **reset to `sv.gameTime`** in `SV_MapRestart_f` after the warmup
frames — see [map_restart bug](#map_restart-awlastthinktimebug) below.

**`SV_TrackCvarChanges` — auto-disable g_antiwarp:**

```c
if ( sv_antiwarp->modified ) {
    if ( sv_antiwarp->integer ) {
        Cvar_Set( "g_antiwarp", "0" );   // prevent double injection
        Com_Printf( "sv_antiwarp enabled — forcing g_antiwarp 0\n" );
    }
    sv_antiwarp->modified = qfalse;
}
```

This fires whenever `sv_antiwarp` is changed at runtime (e.g. via rcon).

---

## map_restart `awLastThinkTime` bug

### Symptom

After `map_restart`, antiwarp fires spuriously for **every connected
client** on the first few game frames, injecting a blank think before
any real usercmd has arrived.

### Root cause

`SV_MapRestart_f` resets `sv.gameTime` to `sv.time` (line 310) then runs
**4 × 100 ms warmup frames**, advancing both `sv.time` and `sv.gameTime`
by 400 ms. `lastUsercmd` is correctly reset in the final cleanup loop.
`awLastThinkTime` was **not** reset, leaving it at the pre-restart game
time.

On the first `SV_Frame` tick after restart:

```
sv.gameTime      = old_sv.time + 400 + frameMsec   (post-warmup + first tick)
awLastThinkTime  = old_sv.time                      (stale, never reset)
awGap            = 400 + frameMsec  >>  awTol       ← fires for every client
```

### Fix (`sv_ccmds.c` — `SV_MapRestart_f`)

`awLastThinkTime` is reset alongside `lastUsercmd` in the final client
loop (after the last warmup frame):

```c
// sv_ccmds.c — SV_MapRestart_f, final client loop
for ( i = 0; i < sv.maxclients; i++ ) {
    client = &svs.clients[i];
    if ( client->state >= CS_PRIMED ) {
        Com_Memset( &client->lastUsercmd, 0x0, sizeof( client->lastUsercmd ) );
        client->lastUsercmd.serverTime = sv.time - 1;

        // Reset antiwarp baseline to current game time.
        // Without this, awGap ≈ 400 ms >> awTol on the first post-restart
        // tick — antiwarp fires spuriously for every connected client.
        client->awLastThinkTime = sv.gameTime;
    }
}
```

Setting `awLastThinkTime = sv.gameTime` makes `awGap = 0` on that first
tick. Antiwarp will only engage if the client genuinely misses a frame
after reconnecting.

### Why `SV_SpawnServer` (full map change) is unaffected

`SV_ClearServer()` calls `Com_Memset(&sv, 0, ...)` which zeros `sv.gameTime`
(to 0 or the preserved `sv.time` depending on `sv_levelTimeReset`). Any
connected client with a large `awLastThinkTime` will produce a **negative**
`awGap` (`sv.gameTime − awLastThinkTime < 0`), which is always `< awTol`.
Antiwarp never fires until the client sends a real usercmd and resets
`awLastThinkTime` to the new (small) `sv.gameTime`.

---

| Cvar | Default | Flags | Description |
|------|---------|-------|-------------|
| `sv_antiwarp` | `0` | `CVAR_ARCHIVE` | `0`=off, `1`=constant, `2`=decay |
| `sv_antiwarpTol` | `0` | `CVAR_ARCHIVE` | Gap tolerance (ms) before injection starts. `0`=auto: uses `gameMsec` (one game frame) |
| `sv_antiwarpExtra` | `0` | `CVAR_ARCHIVE` | Extrapolation window (ms) for mode 2 before decay begins. `0`=auto: same as `awTol` |
| `sv_antiwarpDecay` | `150` | `CVAR_ARCHIVE` | Decay duration (ms) for mode 2. Inputs linearly fade to zero over this window |

### Auto values

With all defaults at `0`/`150` and `sv_fps 50` / `sv_gameHz 0`:

| Cvar | Auto value | Source |
|------|-----------|--------|
| `awTol` | 20 ms | `_gameMsec` = 1000 / sv_fps = 20 ms |
| `awExtraMs` | 20 ms | = `awTol` |
| `awDecayMs` | 150 ms | `sv_antiwarpDecay` explicit |

Total antiwarp window before player stops: `20 + 20 + 150 = 190 ms`.

---

## Interaction with sv_pmoveMsec

`sv_pmoveMsec` caps the Pmove step size during **normal** (non-antiwarp)
usercmd processing. Engine antiwarp bypasses `sv_pmoveMsec` — it always
calls `GAME_CLIENT_THINK` once per game frame with a `_gameMsec` step,
regardless of `sv_pmoveMsec`.

This is correct behaviour: `sv_pmoveMsec` limits how much a single real
usercmd advances `commandTime`; antiwarp injections are synthesised
one-per-game-frame by the engine and are already at the correct step size.

---

## Interaction with sv_antilag

Engine antiwarp and shadow antilag are **independent systems**:

- `sv_antiwarp` keeps the player physically present between usercmds
  (affects their position in the world)
- `sv_antilag` rewinds entities for hit registration
  (affects traces during weapon fire)

They do not conflict. With both enabled:

1. Lagging player continues moving (antiwarp)
2. When shooter fires: shadow history contains the antiwarp-injected
   positions → rewind places lagging player where antiwarp left them

Recommended combined config:

```
sv_antiwarp      2
sv_antiwarpDecay 150
sv_antilag       1
sv_antilagMaxMs  200
```

---

## Configuration Profiles

### UT 4.3.4 — Recommended (engine antiwarp)

```
sv_gameHz            0       // sv_fps rate game frames (no sub-rate)
sv_fps               50
sv_antiwarp          2       // decay mode
sv_antiwarpDecay     150
// g_antiwarp auto-forced to 0 by engine
```

### UT 4.3.4 — Legacy (QVM antiwarp)

```
sv_gameHz            20      // REQUIRED — g_antiwarp hardcodes 50ms steps
sv_fps               50
sv_antiwarp          0       // engine antiwarp off
// set g_antiwarp 1 in your map config or game QVM
```

### UT 4.0–4.2

```
sv_gameHz            20      // REQUIRED
sv_fps               50
sv_antiwarp          0       // QVM antiwarp handles it
// g_antiwarp 1 in QVM config
```

### High-tick (125 Hz)

```
sv_gameHz            0
sv_fps               125
sv_antiwarp          2
sv_antiwarpDecay     150
// awTol auto = 8ms (1000/125), awExtraMs = 8ms
// total window: 8 + 8 + 150 = 166ms before player stops
```

---

## Timing Diagram

```
sv_antiwarp 2  ·  sv_fps 50  ·  sv_gameHz 0
awTol=20ms  awExtraMs=20ms  awDecayMs=150ms

Real usercmds flowing:
  t=0   ClientThink(real cmd)   awLastThinkTime=0
  t=20  ClientThink(real cmd)   awLastThinkTime=20
  t=40  ClientThink(real cmd)   awLastThinkTime=40

Lag spike starts at t=40:

  t=60   awGap=20ms → not > awTol(20) → no inject yet
  t=80   awGap=40ms → > awTol(20) → INJECT  (Phase 1, full: 40 < decayStart=40)
                       lastUsercmd.serverTime += 20
                       ClientThink(injected)

  t=100  awGap=60ms → > awTol → INJECT  (Phase 2: elapsed = 60-40 = 20ms > awExtraMs)
         scale = 127 - (127×20/150) ≈ 110  →  inputs at ~87%

  t=120  awGap=80ms  scale ≈ 93   →  inputs at ~73%
  t=170  awGap=130ms scale ≈ 17   →  inputs at ~13%

  t=190  awGap=150ms  elapsed=150 ≥ awDecayMs → Phase 3
         forwardmove = rightmove = upmove = 0
         Pmove ground friction decelerates player to rest

  t=400  Packets resume:
         real ClientThink fires, awLastThinkTime resets to t=400
         player continues from stopped position
```

**Contrast with mode 1** (constant):
```
  t=400  Packets resume:
         player has been moving at original velocity for 368ms
         may have crossed significant distance or entered bad geometry
```
