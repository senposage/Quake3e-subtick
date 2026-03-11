# Shadow Antilag — Changelog & Review Notes

Reference document for manual review of engine-side shadow antilag fixes
applied in **PR #35** (merged), **PR #36** (merged), **PR #37** (merged),
**PR #38** (merged), **PR #39** (merged), and **PR #40** (current).

All changes are in `code/server/sv_antilag.c` and/or `code/server/sv_main.c`
unless noted otherwise.

---

## Table of Contents

1. [Architecture recap](#architecture-recap)
2. [PR #35 — Fire time & shadow history config fixes](#pr-35--fire-time--shadow-history-config-fixes)
3. [PR #36 — sv.time / svs.time domain fix](#pr-36--svtime--svstime-domain-fix)
4. [PR #37 — Fire time formula revision (ping/2)](#pr-37--fire-time-formula-revision-ping2)
5. [PR #38 — Fire time corrected to full RTT; sv_physicsScale removed](#pr-38--fire-time-corrected-to-full-rtt-sv_physicsscale-removed)
6. [PR #39 — Recording order fix; snapshot-interval compensation](#pr-39--recording-order-fix-snapshot-interval-compensation)
7. [PR #40 — Antiwarp map-restart fix; ring-buffer early-exit; cleanups](#pr-40--antiwarp-map-restart-fix-ring-buffer-early-exit-cleanups)
8. [Combined before/after summary](#combined-beforeafter-summary)
9. [Review checklist](#review-checklist)

---

## Architecture recap

```
GAME TICK  (sv_gameHz Hz)        → QVM sees this. level.time advances here.
SHADOW RECORDING  (sv_fps Hz)    → Engine only. Records entity positions
                                    into svShadowHistory[] ring buffer.
HIT REGISTRY  (on G_TRACE)      → Engine intercepts trap_Trace from QVM.
                                    Rewinds entities → runs trace → restores.
                                    QVM never sees rewound state.
```

Two independent clocks:
- **`sv.time`** — resets on map change; sent to clients in snapshots
- **`svs.time`** — persists across map changes; wall-clock server time

Clients calibrate their `serverTime` estimate from snapshots, so
`lastUsercmd.serverTime` is in the **`sv.time`** domain.

---

## PR #35 — Fire time & shadow history config fixes

**Merged:** 2026-03-03 · **Files:** `sv_antilag.c`, `sv_antilag.h`, `sv_main.c`

### Fix 1: Fire time over-rewind (primary)

**Bug:** `SV_Antilag_GetClientFireTime` used `svs.time - cl->ping` as the
rewind target. `cl->ping` is the full round-trip time, but the client's
perception of server time is approximately half RTT in the past. A 40 ms
ping player got a 40 ms rewind when the correct value is ~20 ms.

```c
// BEFORE (original)
fireTime = svs.time - cl->ping;

// AFTER (PR #35)
fireTime = cl->lastUsercmd.serverTime;
```

**Why `lastUsercmd.serverTime`:**
- Set from the usercmd *before* `GAME_CLIENT_THINK` fires
- Available during weapon traces (QVM sees it as `AttackTime`)
- Already incorporates `cl_timeNudge` / `cl_autoNudge` from the client
- Makes `ut_timenudge` correctly irrelevant (shadow rewind overrides FIFO)

### ~~Fix 2: Bots excluded from shadow history recording~~ (REVERTED)

**Original change:** Removed the `NA_BOT` filter so bots were recorded
into shadow history, reasoning that humans shooting at bots needed lag
compensation.

**Reverted:** Bots are correctly excluded from shadow recording. The
QVM's FIFO antilag already handles bot targets (FIFO skips bots — they
have zero lag — so their position is already correct at trace time).
Recording bots into shadow history causes a double-rewind conflict:
FIFO shifts the bot to position A, then shadow overwrites with position B
computed from a different time formula (`sv.time - ping/2` vs FIFO's
`level.time` domain). The trace runs against a position neither system
intended.

Bots are still **skipped as shooters** in `InterceptTrace` (zero lag).

### Fix 3: sv_fps change detection race

**Bug:** `sv_fps->modified` is cleared by `SV_TrackCvarChanges` before
`SV_Antilag_RecordPositions` runs, so `sv_fps` changes were never detected.

```c
// BEFORE
if ( sv_physicsScale->modified || sv_antilagMaxMs->modified || sv_fps->modified )

// AFTER — track integer value directly
static int sv_antilag_lastFpsValue = 0;
int currentFps = sv_fps ? sv_fps->integer : 40;
qboolean fpsChanged = ( currentFps != sv_antilag_lastFpsValue );
if ( sv_physicsScale->modified || sv_antilagMaxMs->modified || fpsChanged )
```

### Fix 4: Stale history on cvar round-trip

**Bug:** Ring buffer flush was conditional on slot count changing. A
round-trip like `sv_fps 60→40→60` keeps the same slot count but leaves
entries with wrong timestamp spacing, producing incorrect interpolation.

```c
// BEFORE — only flush if slot count changed
if ( sv_shadowHistorySlots != oldSlots ) {
    Com_Memset( sv_shadowHistory, 0, sizeof( sv_shadowHistory ) );
}

// AFTER — always flush on any timing change
Com_Memset( sv_shadowHistory, 0, sizeof( sv_shadowHistory ) );
```

### Fix 5: Duplicate-timestamp entries from physicsScale loop

**Bug:** `SV_Frame` called `RecordPositions` in a `physicsScale` loop
(e.g. 3×), but all calls used the same `svs.time`, filling 2/3 of the
ring buffer with duplicate timestamps and reducing effective history
coverage to ~175 ms instead of 200 ms (24 slots / 3 per tick = 8 unique
timestamps spanning 7 × 25 ms).

```c
// BEFORE (sv_main.c) — recorded 3× per tick with same timestamp
for ( _s = 0; _s < _scale; _s++ ) {
    SV_Antilag_RecordPositions();
}

// AFTER (sv_main.c) — record once per engine tick
SV_Antilag_RecordPositions();
```

### Fix 6: Config/recording rate mismatch

**Bug:** After removing the physicsScale loop, `ComputeConfig` still
calculated slots for `fps × scale` Hz instead of the actual `fps` Hz
recording rate.

```c
// BEFORE
int shadowHz = fps * scale;
sv_shadowTickMs = 1000 / shadowHz;
sv_shadowHistorySlots = ( shadowHz * winMs ) / 1000;

// AFTER — based on fps alone
sv_shadowTickMs = 1000 / fps;
sv_shadowHistorySlots = ( fps * winMs ) / 1000 + 1;
```

### Fix 7: Off-by-one in slot count

**Bug:** `(Hz * windowMs) / 1000` under-counts by 1 because N slots span
N−1 intervals. At defaults (40 Hz, 200 ms) this gave 8 slots = 175 ms
instead of 9 slots = 200 ms.

```c
// BEFORE
sv_shadowHistorySlots = ( shadowHz * winMs ) / 1000;

// AFTER — +1 to cover full window
sv_shadowHistorySlots = ( fps * winMs ) / 1000 + 1;
```

---

## PR #36 — sv.time / svs.time domain fix

**Status:** Merged · **Files:** `sv_antilag.c`

### The core problem

PR #35 correctly changed the fire time formula to
`cl->lastUsercmd.serverTime`, but all the **surrounding comparisons**
(clamps, fallback, recording timestamps) still used `svs.time`.

Since `lastUsercmd.serverTime` is calibrated from snapshots (which send
`sv.time`), the value is in the `sv.time` domain. After a map change the
two clocks diverge:

```
svs.time = 500100      // persisted from previous map
sv.time  = 408         // reset on map load
fireTime = ~400        // from lastUsercmd.serverTime, calibrated via sv.time

svs.time - fireTime = 499700  >>  maxRewind (200)
→ every shot clamped to 200 ms rewind regardless of actual ping
```

### Fix 1: Recording timestamps — `svs.time` → `sv.time`

Shadow history must be in the same time domain as `fireTime` so the
interpolation lookup finds correct entries.

```c
// BEFORE (after PR #35)
SV_Antilag_RecordClient( i, svs.time );

// AFTER (PR #36)
SV_Antilag_RecordClient( i, sv.time );
```

### Fix 2: Fire time clamps — `svs.time` → `sv.time`

```c
// BEFORE — fallback and clamps used svs.time
if ( shooterNum < 0 || shooterNum >= sv_maxclients->integer )
    return svs.time;
...
if ( svs.time - fireTime > maxRewind )
    fireTime = svs.time - maxRewind;
if ( fireTime > svs.time )
    fireTime = svs.time;

// AFTER — all in sv.time domain
if ( shooterNum < 0 || shooterNum >= sv_maxclients->integer )
    return sv.time;
...
if ( sv.time - fireTime > maxRewind )
    fireTime = sv.time - maxRewind;
if ( fireTime > sv.time )
    fireTime = sv.time;
```

### Fix 3: Save/restore simplification

**Before:** `RewindAll` called `SV_Antilag_GetMostRecentPosition` to
get the "true" position from shadow history, saving that instead of the
entity's actual current position. This was originally to override FIFO
state, but:
- For bots (no FIFO shift), shadow-history "true pos" == current pos
- For humans, the FIFO has already shifted them; saving FIFO state is
  correct because `G_UnTimeShiftAllClients` expects to undo exactly that

**After:** Save `gent->r.currentOrigin` directly. Removed
`SV_Antilag_GetMostRecentPosition()` entirely.

```c
// BEFORE — two-path save via shadow history lookup
if ( SV_Antilag_GetMostRecentPosition( i, trueOrigin, trueAbsmin, trueAbsmax ) ) {
    sv_shadowSaved[i].saved = qtrue;
    VectorCopy( trueOrigin, sv_shadowSaved[i].origin );
    ...
} else {
    sv_shadowSaved[i].saved = qtrue;
    VectorCopy( gent->r.currentOrigin, sv_shadowSaved[i].origin );
    ...
}

// AFTER — always save current entity position
sv_shadowSaved[i].saved = qtrue;
VectorCopy( gent->r.currentOrigin, sv_shadowSaved[i].origin );
VectorCopy( gent->r.absmin,        sv_shadowSaved[i].absmin );
VectorCopy( gent->r.absmax,        sv_shadowSaved[i].absmax );
```

### Fix 4: No-history fallback simplification

**Before:** When no shadow history existed for `targetTime`, the code
force-wrote the saved position back into the entity and re-linked it
(a no-op write that still cost a `SV_LinkEntity` call).

**After:** Just leave the entity at its current position. The trace runs
against whatever position the QVM has set.

### Fix 5: Debug output — `svs.time` → `sv.time`

```c
// BEFORE
int rewindMs = svs.time - fireTime;
Com_Printf( "... (svs.time=%d fireTime=%d)\n", ..., svs.time, fireTime );

// AFTER
int rewindMs = sv.time - fireTime;
Com_Printf( "... (sv.time=%d fireTime=%d)\n", ..., sv.time, fireTime );
```

### Intentionally NOT changed: Rate tracking

`SV_Antilag_NoteSnapshot` uses `svs.time` to measure snapshot send rate.
This is correct — it measures wall-clock intervals, not game time. The
rate tracking system is unrelated to the rewind calculations.

---

## PR #37 — Fire time formula revision (ping/2)

**Status:** Merged · **Files:** `sv_antilag.c`, `sv_antilag.h`, `docs/Example_server.cfg`

### The core problem

PR #35 changed the fire time from `svs.time - cl->ping` to
`cl->lastUsercmd.serverTime`. While `lastUsercmd.serverTime` is in the
correct `sv.time` domain, it includes the client's **local interpolation
delay** (cl_timeNudge, cl_autoNudge, snapshot buffering) on top of the
network latency. This produces excessive rewind:

```
ping = 45 ms        → expected half-RTT rewind ≈ 22 ms
lastUsercmd rewind  → actual rewind = 69–77 ms  (1.5× full RTT)
```

The extra ~47 ms comes from the client's interpolation pipeline:
- `cl_autoNudge` adds approximately `snapInterval / 2` (~16 ms)
- Snapshot smoothing / `CL_AdjustTimeDelta` adds another ~16–25 ms
- These offsets are correct for the QVM's FIFO antilag (which operates in
  the `level.time` domain where they cancel out), but in the shadow system
  they double-up because shadow history already records at `sv.time`

The result: hits feel delayed and spongy compared to the QVM's own FIFO
antilag. Reverting to `sv_antilagEnable 0` restores responsiveness because
the FIFO system's `level.time` domain absorbs the interpolation delay
naturally.

### Fix 1: Fire time — `lastUsercmd.serverTime` → `sv.time - ping/2`

```c
// BEFORE (after PR #35 + PR #36)
fireTime = cl->lastUsercmd.serverTime;

// AFTER (PR #37)
fireTime = sv.time - cl->ping / 2;
```

**Why `sv.time - cl->ping / 2`:**
- `cl->ping` is the full round-trip time, measured by the engine
- `ping / 2` approximates one-way latency (server → client)
- Shadow history records at `sv.time`, so rewinding by one-way latency
  gives the position the client was seeing when they fired
- Does not include client interpolation delay → rewind is tight and
  responsive
- For bots (ping = 0): `sv.time - 0 = sv.time` → no rewind (correct,
  bots are already skipped as shooters in `InterceptTrace`)

**Example improvement (45 ms ping player):**

| Formula | Rewind | Feel |
|---------|--------|------|
| `svs.time - cl->ping` (original) | 45 ms, wrong domain | broken after map change |
| `cl->lastUsercmd.serverTime` (PR #35) | 69–77 ms | spongy, delayed |
| `sv.time - cl->ping / 2` (PR #37) | 22 ms | responsive, accurate |

### Fix 2: Header cvar name mismatch

`sv_antilag.h` declared `sv_antilagTraceLog` and `sv_snapRateLog`, but
`sv_antilag.c` defines `sv_antilagDebug` and `sv_antilagRateDebug`.
Fixed the header externs to match the implementation.

```c
// BEFORE (sv_antilag.h)
extern cvar_t *sv_antilagTraceLog;
extern cvar_t *sv_snapRateLog;

// AFTER
extern cvar_t *sv_antilagDebug;
extern cvar_t *sv_antilagRateDebug;
```

### Fix 3: Example config cvar names

`docs/Example_server.cfg` referenced stale names (`sv_antilagEnable`)
and wrong header names. Updated:
- `sv_antilagEnable` → `sv_antilag` (matches the registered cvar name)
- `sv_antilagRateDebug` was already correct

---

## PR #38 — Fire time corrected to full RTT; sv_physicsScale removed

**Files:** `sv_antilag.c`, `sv_antilag.h`, `docs/Example_server.cfg`, `docs/project/SERVER.md`, `docs/project/FLOWCHART.md`

### Fix 7: Fire time under-compensation (ping/2 → ping)

**Bug:** PR #37's formula `sv.time - cl->ping / 2` only compensated for
one network hop (server→client), ignoring the return hop (client→server).

**Timing proof:**

```
T0         position recorded in shadow history; snapshot built; sent
           messageSent = Sys_Milliseconds() = T0_wall
T0+RTT/2   client receives snapshot; sees target at sv.time = T0
T0+RTT/2   client fires; sends usercmd
T0+RTT     server receives usercmd
           messageAcked = Sys_Milliseconds() = T0_wall + RTT
           cl->ping = messageAcked - messageSent = RTT  (server-measured)
           sv.time ≈ T0 + RTT  (advances at 1 ms per real-ms)
```

The target was at position `T0` when the client aimed. `sv.time` when the
trace runs is `T0 + RTT`. Correct rewind = **full RTT = `cl->ping`**.

`ping/2` only subtracts one hop, leaving the target `RTT/2 ≈ ping/2` ms
too far forward — the shooter must visually lead the target by half their
own ping. At 40 ms ping this is a 20 ms positional error (~128 units at
320 ups), noticeable and physically wrong.

```c
// BEFORE (PR #37)
fireTime = sv.time - cl->ping / 2;

// AFTER (PR #38)
fireTime = sv.time - cl->ping;
```

**Why not `lastUsercmd.serverTime`:**
`cl->lastUsercmd.serverTime` is the client's reported render time. It
already contains the RTT *plus* the client's interpolation buffer
(`cl_autoNudge`, `CL_AdjustTimeDelta` — typically +30–50 ms), so it
over-compensates to ~69–77 ms for a 45 ms ping player (confirmed by PR
#37 measurement). The full-RTT formula `sv.time - ping` gives the exact
snapshot time (45 ms for 45 ms ping) without including the client-side
interpolation lag — which the shadow system already handles through linear
interpolation between recorded entries.

**Example (45 ms ping):**

| Formula | Rewind | Assessment |
|---------|--------|------------|
| `svs.time - cl->ping` (original) | 45 ms — wrong domain | domain bug |
| `cl->lastUsercmd.serverTime` (PR #35) | 69–77 ms | over-compensates (interp buffer) |
| `sv.time - cl->ping / 2` (PR #37) | 22 ms | under-compensates (half RTT) |
| `sv.time - cl->ping` (PR #38) | **45 ms** | **exact** |

### Fix 8: sv_physicsScale cvar removed

The `sv_physicsScale` cvar was registered and watched for `->modified`
but its value was never read. The physicsScale loop in `SV_Frame` was
removed in Fix 5 (PR #35), and `ComputeConfig` uses `sv_fps` Hz only.

- Removed from `sv_antilag.c` (declaration, `Cvar_Get`, `->modified` watch)
- Removed from `sv_antilag.h` (extern)
- Removed from `docs/Example_server.cfg`, `docs/project/SERVER.md`,
  `docs/project/FLOWCHART.md`

---

## PR #39 — Recording order fix; snapshot-interval compensation

**Files:** `sv_main.c`, `sv_antilag.c`

### Fix 9: Shadow recording order (before-game-frame → after-game-frame) + sv_gameHz guard

**Bug:** `SV_Antilag_RecordPositions()` ran **before** `GAME_RUN_FRAME` in
each engine tick. This meant `shadow[T]` held entity positions from the
end of the *previous* tick (pre-game-frame state), while the snapshot
sent at tick T carried post-game-frame positions. Every fire-time lookup
was pointing at positions one tick stale relative to what the snapshot
showed.

```
BEFORE (sv_main.c per-tick order):
  sv.time += frameMsec
  SV_Antilag_RecordPositions()   ← records pre-game positions at sv.time = T
  GAME_RUN_FRAME                 ← entities move; weapon traces fire here
  SV_SendClientMessages()        ← snapshot carries post-game positions at T

  Result: shadow[T] = P_{N-1}  but  snapshot[T] = P_N  (mismatch)

AFTER:
  sv.time += frameMsec
  _gameFrameRan = qfalse
  GAME_RUN_FRAME (inner loop)    ← entities move; _gameFrameRan = qtrue
  if _gameFrameRan:
    SV_Antilag_RecordPositions() ← records post-game positions at sv.time = T
  SV_SendClientMessages()        ← snapshot carries same post-game positions

  Result: shadow[T] = P_N  ==  snapshot[T] = P_N  ✓
```

**sv_gameHz guard:** When `sv_gameHz > 0`, the inner game loop runs at a
slower rate than `sv_fps`. On ticks where `sv.gameTimeResidual < gameMsec`
the inner loop does not fire — entities have not moved. Recording on such
ticks would add a duplicate shadow entry (stale positions, new sv.time
tag) that does not correspond to any real entity state change, polluting
the history with incorrect timestamps. The `_gameFrameRan` flag is set
inside the inner loop and used to skip recording on no-game-frame ticks.

When `sv_gameHz <= 0` (disabled), `gameMsec = frameMsec` and the inner
loop fires every sv_fps tick, so `_gameFrameRan` is always true and
behaviour is unchanged.

With this fix, `shadow[T]` and `snapshot[T]` always hold the same entity
state, so fire-time lookups are consistent at all `sv_fps` and `sv_gameHz`
rates.

### Fix 10: Snapshot-interval compensation in fire time

**Bug:** `fireTime = sv.time - cl->ping` rewound to the exact snapshot
time `T0`. But the Q3/URT client interpolates targets **one snapshot
interval behind** its latest received snapshot (`cl_interp ≈ snapshotMsec`
by default). The visual position of a target was therefore at
`T0 − snapshotMsec`, not `T0`.

**Full timing path (after Fix 9):**

```
T0          shadow records positions (after game frame, post-Fix 9)
T0          snapshot sent;  messageSent = Sys_Milliseconds()
            shadow[T0] == snapshot[T0]  ← guaranteed by Fix 9
T0+RTT/2    client receives snapshot; latest server time = T0
T0+RTT/2    client RENDERS target at T0 − snapshotMsec (cl_interp)
T0+RTT/2    client fires; sends usercmd
T0+RTT      server receives usercmd;  messageAcked = Sys_Milliseconds()
            cl->ping        = RTT       (server-measured)
            cl->snapshotMsec = 1000/snapHz  (per-client, from sv_fps / sv_snapshotFps)
            sv.time ≈ T0 + RTT
```

Target position the client aimed at: `shadow[T0 − snapshotMsec]`

```c
// BEFORE (PR #38)
fireTime = sv.time - cl->ping;
// → rewound to T0  (client was seeing T0 − snapshotMsec)

// AFTER (PR #39)
fireTime = sv.time - cl->ping - cl->snapshotMsec;
// = (T0 + RTT) − RTT − snapshotMsec = T0 − snapshotMsec  ✓
```

`cl->snapshotMsec` is **per-client** (`1000 / snapHz`, set from
`sv_fps` or `sv_snapshotFps` at connect/reconfigure). The formula is
therefore correct at any `sv_fps` rate — 20 Hz, 60 Hz, 125 Hz, etc.

**Limitation — dropped / late snapshots:**
If a snapshot is dropped in transit the client interpolates across a
two-snapshot gap. Its effective `cl_interp` increases by `snapshotMsec`
for that shot. The server has no visibility into which snapshot the client
used for aiming, so the rewind will be `snapshotMsec` too shallow and the
shot may miss the shadow interpolation window. This is unavoidable without
a client-side interp report.

**Example (40 ms ping, 60 Hz snaps → snapshotMsec = 16 ms):**

| Formula | Rewind | Assessment |
|---------|--------|------------|
| `sv.time - cl->ping / 2` (PR #37) | 20 ms | under-compensates |
| `sv.time - cl->ping` (PR #38) | 40 ms | misses interp offset |
| `sv.time - cl->ping - cl->snapshotMsec` (PR #39) | **56 ms** | **exact** |

---

## PR #40 — Antiwarp map-restart fix; ring-buffer early-exit; cleanups

**Files:** `sv_ccmds.c`, `sv_antilag.c`, `sv_snapshot.c`, `sv_main.c`,
`docs/project/SV_ANTIWARP.md`, `docs/shadow-antilag-changelog.md`

### Fix 11: Antiwarp spurious injection after map_restart (`sv_ccmds.c`)

**Bug:** `SV_MapRestart_f` runs 4 × 100 ms warmup frames, advancing
`sv.gameTime` by 400 ms from the restart point. The final cleanup loop
already resets `client->lastUsercmd`, but `client->awLastThinkTime` was
not reset. On the first `SV_Frame` tick after restart:

```
sv.gameTime     = old_sv.time + 400 + frameMsec
awLastThinkTime = old_sv.time                    ← stale, never reset
awGap           = ~400 ms  >>  awTol             ← fires for EVERY client
```

Every connected client received a spurious antiwarp inject before they
had a chance to send a real usercmd after the map restart.

```c
// BEFORE — awLastThinkTime not reset, causing spurious antiwarp on restart
for ( i = 0; i < sv.maxclients; i++ ) {
    client = &svs.clients[i];
    if ( client->state >= CS_PRIMED ) {
        Com_Memset( &client->lastUsercmd, 0x0, sizeof( client->lastUsercmd ) );
        client->lastUsercmd.serverTime = sv.time - 1;
    }
}

// AFTER — reset antiwarp baseline alongside lastUsercmd
for ( i = 0; i < sv.maxclients; i++ ) {
    client = &svs.clients[i];
    if ( client->state >= CS_PRIMED ) {
        Com_Memset( &client->lastUsercmd, 0x0, sizeof( client->lastUsercmd ) );
        client->lastUsercmd.serverTime = sv.time - 1;
        client->awLastThinkTime = sv.gameTime;  // reset antiwarp baseline
    }
}
```

**Why `SV_SpawnServer` (full map change) is unaffected:** `SV_ClearServer`
zeros `sv.gameTime` (to 0 or preserved `sv.time`). Old large
`awLastThinkTime` values yield a negative `awGap`, which is always
`< awTol`. Antiwarp stays inactive until each client sends their first
real usercmd (which calls `SV_ClientThink` → `awLastThinkTime = sv.gameTime`).

### Fix 12: Dead variable `sv_shadowAccumulator` removed (`sv_antilag.c`)

`sv_shadowAccumulator` was declared as a static `int`, initialised to 0
in `SV_Antilag_Init`, and never read or written again anywhere. Leftover
from an earlier accumulator-based shadow-recording approach. Removed both
the declaration and the initialisation.

### Fix 13: Early exit in `SV_Antilag_GetPositionAtTime` history scan (`sv_antilag.c`)

Both history-scan loops iterate the ring buffer **newest-first**
(backward from head). Once both the `before` bracket (most-recent entry ≤
targetTime) and the `after` bracket (most-recent entry > targetTime) are
found, and the current entry's `serverTime` is now older than
`before->serverTime`, no further iteration can produce a better bracket.

```c
// BEFORE — always walks the full ring buffer (up to 256 entries per trace)
for ( i = 0; i < hist->count; i++ ) { ... }

// AFTER — exits as soon as both brackets are locked
for ( i = 0; i < hist->count; i++ ) {
    ...
    // Early exit: iterating newest-first.  Once we have both brackets
    // and have passed below before's timestamp, nothing further can
    // improve either bracket.
    if ( before && after && s->serverTime < before->serverTime )
        break;
}
```

### Fix 14: Same early exit applied to `SV_SmoothGetPosition` (`sv_snapshot.c`)

`SV_SmoothGetPosition` (used by `sv_bufferMs` and `sv_velSmooth`) has the
same newest-first iteration pattern over its 32-slot ring buffer. The
identical early-exit condition was added for consistency and to avoid
full-buffer scans on every snapshot build.

### Fix 15: `sv_bufferMs` recording intentionally ungated — comment added (`sv_main.c`)

`SV_SmoothRecordAll()` runs every `sv_fps` tick, **not** gated on
`_gameFrameRan`. This is correct: `sv_bufferMs` delays positions by N
wall-clock milliseconds. It needs one ring-buffer entry per sv_fps tick
so that a "position T − bufferMs" lookup always finds a tightly-bracketed
timestamp pair. Gating on `_gameFrameRan` would collapse the ring buffer
to game-frame rate (e.g. 20 Hz when `sv_gameHz 20`) and make bufferMs
lookups inaccurate. A detailed comment was added to prevent a future
well-intentioned "fix" that would break this.

Contrast with `SV_Antilag_RecordPositions()`, which **is** gated on
`_gameFrameRan`: shadow entries are indexed by fire-time and carry
meaning only when entity positions actually changed (i.e., after a real
game frame).

### Fix 16: `snapMsec` field added to `sv_antilagDebug` shot log (`sv_antilag.c`)

```c
// BEFORE
Com_Printf( "AL shot cl[%d] ping=%d rewind=%dms (sv.time=%d fireTime=%d)\n",
    passEntityNum, shooter->ping, sv.time - fireTime, sv.time, fireTime );

// AFTER — fireTime = sv.time - ping - snapshotMsec; show both components
Com_Printf( "AL shot cl[%d] ping=%d snapMsec=%d rewind=%dms (sv.time=%d fireTime=%d)\n",
    passEntityNum, shooter->ping, shooter->snapshotMsec,
    sv.time - fireTime, sv.time, fireTime );
```

The `rewind` value is `ping + snapshotMsec` combined. Without `snapMsec`
visible, distinguishing a legitimate large-ping rewind from a misconfigured
`sv_snapshotFps` / `cl_snapshotRate` contribution required mental
arithmetic. The new field makes both components explicit.

### Fix 17: Bot shadow recording and rewind enabled (`sv_antilag.c`, `sv_main.c`)

**What changed:**
- Bots are now **recorded** in shadow history (`RecordPositions` — `NA_BOT` filter removed)
- Bots are now **included** in the shadow rewind (`RewindAll` — `NA_BOT` filter removed)
- `SV_TrackCvarChanges` now **auto-forces `g_antilag 0`** when `sv_antilag 1` is set,
  analogous to how `sv_antiwarp` already forces `g_antiwarp 0`
- Bots remain **excluded as shooters** in `InterceptTrace` — bots have zero
  network lag and need no antilag when firing; this guard is unchanged

**Why bots were excluded (original rationale):**

The original comment claimed: *"bots have zero network lag so their current
position is always correct at trace time"* and warned of a double-rewind
conflict with QVM FIFO antilag.

**Why that rationale was wrong:**

The shooter has non-zero latency. A shooter with 100 ms ping and 16 ms
`snapshotMsec` is seeing every entity — including bots — at its position
from 116 ms ago. Without shadow rewind for bots, the trace runs against
the bot's *current* position. If the bot moved 100 ms worth of distance
since the shooter last saw it, the shot misses even though the crosshair
was perfectly placed. This is unfair hit registration for every human
vs. bot engagement at non-trivial ping.

**Why the double-rewind concern was overstated:**

The engine shadow rewind fires *inside* the `G_TRACE` syscall intercept —
after the QVM has already set up entity positions (via FIFO if `g_antilag 1`
is active). The save/restore cycles are independent:

```
1. QVM FIFO: may move entities to FIFO-computed positions
2. Engine saves current state (post-FIFO)
3. Engine applies shadow rewind → trace → restores to post-FIFO state
4. QVM FIFO restores to original positions
```

The trace always runs against shadow-recorded positions; entities end at
their original positions. Both systems commute correctly. No conflict.

**The `g_antilag 0` auto-force makes this unambiguous:**

With `g_antilag 0` forced, the QVM FIFO never runs. Step 1 is a no-op.
The engine shadow rewind works directly against post-game-frame entity
state, with no FIFO pre-setup to worry about. The auto-force fires in
`SV_TrackCvarChanges` whenever `sv_antilag` is turned on (at startup or
via rcon), and prints a console notice:

```c
// sv_main.c — SV_TrackCvarChanges
// NULL guard required: SV_TrackCvarChanges runs at SV_Init time,
// before SV_Antilag_Init registers sv_antilag.
if ( sv_antilag && sv_antilag->modified ) {
    if ( sv_antilag->integer ) {
        Cvar_Set( "g_antilag", "0" );
        Com_Printf( "sv_antilag enabled — forcing g_antilag 0\n" );
    }
    sv_antilag->modified = qfalse;
}
```

`sv_antilag` is added to `CVG_SERVER` in `sv_init.c` immediately after
`SV_Antilag_Init()` returns:

```c
// sv_init.c — SV_Init, after SV_Antilag_Init()
SV_Antilag_Init();
Cvar_SetGroup( sv_antilag, CVG_SERVER );
```

This is necessary because `SV_TrackCvarChanges` is only called when
`Cvar_CheckGroup(CVG_SERVER)` returns non-zero (i.e. some `CVG_SERVER`
cvar changed). Without `sv_antilag` being in `CVG_SERVER`, a runtime
`rcon sv_antilag 1` would set `sv_antilag->modified` but never trigger
the handler.

`g_antilag 1` remains the recommended value in `Example_server.cfg` as
the **fallback** used when `sv_antilag` is off (e.g. server temporarily
disabled engine antilag via rcon). With `sv_antilag 1` it is overridden
automatically, so its value at that point is irrelevant.

---

## Combined before/after summary

| Area | Original code | After PR #35 | After PR #36 | After PR #37 | After PR #38 | After PR #39 | After PR #40 |
|------|--------------|--------------|--------------|--------------|--------------|--------------|--------------|
| **Fire time formula** | `svs.time - cl->ping` | `cl->lastUsercmd.serverTime` | *(unchanged)* | `sv.time - cl->ping / 2` | `sv.time - cl->ping` | **`sv.time - cl->ping - cl->snapshotMsec`** |
| **Fire time clamps** | vs `svs.time` | vs `svs.time` | vs **`sv.time`** | *(unchanged)* | *(unchanged)* | *(unchanged)* |
| **Recording order in SV_Frame** | before game frame | *(unchanged)* | *(unchanged)* | *(unchanged)* | *(unchanged)* | **after game frame** |
| **Recording timestamp** | `svs.time` (bots excluded) | `svs.time` (bots ~~included~~ reverted) | **`sv.time`** (bots excluded) | *(unchanged)* | *(unchanged)* | *(unchanged)* | **bots included** |
| **sv_fps detection** | `sv_fps->modified` (race) | Integer value tracking | *(unchanged)* | *(unchanged)* | *(unchanged)* | *(unchanged)* |
| **History flush** | Only if slot count changed | Always on timing change | *(unchanged)* | *(unchanged)* | *(unchanged)* | *(unchanged)* |
| **Recording loop** | `physicsScale×` per tick | 1× per tick | *(unchanged)* | *(unchanged)* | *(unchanged)* | *(unchanged)* |
| **Slot calculation** | `fps × scale` Hz, off-by-one | `fps` Hz, +1 | *(unchanged)* | *(unchanged)* | *(unchanged)* | *(unchanged)* |
| **sv_physicsScale cvar** | registered + watched | *(unchanged)* | *(unchanged)* | *(unchanged)* | **removed** | *(unchanged)* |
| **Save/restore** | Via `GetMostRecentPosition` | Via `GetMostRecentPosition` | Direct `gent->r.currentOrigin` | *(unchanged)* | *(unchanged)* | *(unchanged)* |
| **No-history fallback** | Force-write saved pos back | Force-write saved pos back | Leave entity at current pos | *(unchanged)* | *(unchanged)* | *(unchanged)* |
| **Debug output time** | `svs.time` | `svs.time` | **`sv.time`** | *(unchanged)* | *(unchanged)* | *(unchanged)* |
| **Header cvar names** | `sv_antilagTraceLog` | *(unchanged)* | *(unchanged)* | **`sv_antilagDebug`** | *(unchanged)* | *(unchanged)* |
| **Enable cvar name** | — | `sv_antilagEnable` (docs only) | *(unchanged)* | **`sv_antilag`** (code + docs) | *(unchanged)* | *(unchanged)* |

---

## Review checklist

All items verified in the current codebase:

- [x] Fire time uses `sv.time - cl->ping - cl->snapshotMsec`.
      `cl->ping` = full RTT (server-measured); `cl->snapshotMsec` = per-client
      snapshot interval (`1000/snapHz`, from `sv_fps`/`sv_snapshotFps`) which
      represents the Q3/URT client's default `cl_interp` delay. Together they
      match the exact position the shooter was rendering when they fired.
      Works at any `sv_fps` rate because `snapshotMsec` is per-client.
      Known limitation: a dropped snapshot increases the client's effective
      `cl_interp` by one `snapshotMsec`; the server cannot detect this.
- [x] Shadow recording runs **after** the game frame in `SV_Frame`, guarded
      by `_gameFrameRan`. When `sv_gameHz > 0` the inner game loop may not
      fire every sv_fps tick; the flag prevents recording duplicate shadow
      entries (stale positions at a new sv.time) on no-game-frame ticks.
      When `sv_gameHz <= 0`, `gameMsec = frameMsec` so the loop always
      fires and `_gameFrameRan` is always true. In all cases `shadow[T]`
      == post-game-frame entity state == what snapshot T delivers to clients.
- [x] `sv.time` is used consistently in all shadow antilag paths that
      compare against fire time or shadow history timestamps
- [x] `svs.time` is **only** used in `NoteSnapshot` rate tracking (wall-clock)
- [x] Bots are **included** in shadow history recording and rewind
      (`NA_BOT` filters removed from `RecordPositions` and `RewindAll`).
      The human shooter sees bots through the same snapshot/interpolation
      pipeline as human targets: a 100 ms shooter is aiming at the bot's
      position from ~(ping + snapshotMsec) ms ago. Excluding bots from
      shadow rewind caused systematic misses in human vs. bot engagements
      at non-trivial ping. The original "double-rewind conflict" concern
      (QVM FIFO + shadow rewind interacting) is resolved by the `g_antilag 0`
      auto-force: when `sv_antilag 1`, `SV_TrackCvarChanges` forces `g_antilag 0`
      so no FIFO pre-setup occurs. Even without that, the two save/restore
      cycles commute correctly and the trace always uses shadow-recorded positions.
      Bots are still **excluded as shooters** in `InterceptTrace` (zero latency).
- [x] `ComputeConfig` slot count uses `sv_fps` Hz (not `fps × scale`)
      with `+1` to avoid off-by-one
- [x] History is always flushed on any timing config change (not just
      when slot count differs)
- [x] Recording happens once per engine tick (no `physicsScale` loop in
      `SV_Frame`)
- [x] `RewindAll` saves `gent->r.currentOrigin` directly — no
      `GetMostRecentPosition` lookup
- [x] `RestoreAll` restores saved position unconditionally after trace
- [x] The QVM contract is never violated: `level.time` is never touched,
      entities are restored before control returns to the VM
- [x] Header `sv_antilag.h` externs match `sv_antilag.c` definitions
      (`sv_antilag`, `sv_antilagMaxMs`, `sv_antilagDebug`,
      `sv_antilagRateDebug`). `sv_physicsScale` removed — cvar was dead
      (registered but never read; physicsScale loop removed in Fix 5).
- [x] Enable cvar is `sv_antilag` (not `sv_antilagEnable`); default is `0`
      (disabled — set `sv_antilag 1` in server config to enable)
