# tmpbugs.md — Code Issues (re-impla branch)

Issues found while auditing `code/server/` against the documentation.
None of these are committed code fixes — awaiting go-ahead.

---

## BUG-1 — `sv_physicsScale` is a dead/orphaned cvar — **RESOLVED (Option B)**

**Resolution:** `sv_physicsScale` removed from `sv_antilag.c`, `sv_antilag.h`, and all
documentation. Shadow recording runs at `sv_fps` Hz (one sample per engine tick).
At sv_fps 60 or 125 the interpolation error is already sub-frame; a multiplier
would produce wrong results by making the interpolator assume entries are spaced
more closely than they are. See commit history for full rationale.

---

## BUG-2 — `awLastThinkTime` not zeroed on `map_restart`

**Severity:** Low-Medium  
**Files:** `code/server/sv_ccmds.c` (`SV_MapRestart_f`), `code/server/sv_client.c` (`SV_ClientEnterWorld`)

### What the code does

`SV_ClientThink` (called when a real usercmd arrives from the network) sets:

```c
// sv_client.c:2265
cl->awLastThinkTime = sv.gameTime;
```

`SV_MapRestart_f` resets `lastUsercmd` for each active client but does **not** reset `awLastThinkTime`:

```c
// sv_ccmds.c — bottom of SV_MapRestart_f
for ( i = 0; i < sv.maxclients; i++ ) {
    client = &svs.clients[i];
    if ( client->state >= CS_PRIMED ) {
        Com_Memset( &client->lastUsercmd, 0x0, sizeof( client->lastUsercmd ) );
        client->lastUsercmd.serverTime = sv.time - 1;
        // ← awLastThinkTime NOT reset here
    }
}
```

`SV_ClientEnterWorld` (also called during restart for active clients) does **not** reset `awLastThinkTime` either.

### Why this matters

During `SV_MapRestart_f`, four settlement game frames run:

```c
sv.gameTime = sv.time;          // sync before restart
sv.gameTimeResidual = 0;
SV_RestartGameProgs();
for (i = 0; i < 3; i++) {
    sv.time += 100; sv.gameTime += 100;   // +300 ms
    VM_Call(gvm, GAME_RUN_FRAME, sv.gameTime);
}
sv.time += 100; sv.gameTime += 100;       // +100 ms more (= +400 ms total)
VM_Call(gvm, GAME_RUN_FRAME, sv.gameTime);
```

After the restart, `sv.gameTime` is ~400 ms ahead of the last recorded `awLastThinkTime` (set at the old `sv.gameTime` before restart). On the very first real game tick after clients reconnect:

```
awGap = sv.gameTime_new - awLastThinkTime_old  ≈ 400 ms
awTol = _gameMsec ≈ 16 ms  (auto, at sv_fps 60)
400 >> 16 → antiwarp fires for every client
```

All connected clients get one spurious antiwarp injection on the first tick after `map_restart`. In mode 2 this means their movement inputs are immediately zeroed (since 400 ms >> `awTol + awExtraMs + awDecayMs` = 182 ms at defaults).

Note: fresh map loads (`SV_SpawnServer`) are **not** affected — `svs.clients` is `memset` to zero, which zeros `awLastThinkTime`.

### Proposed fix

In `SV_ClientEnterWorld` (called for active clients during `map_restart`):

```c
// sv_client.c — SV_ClientEnterWorld, after client->oldServerTime = 0;
client->awLastThinkTime = 0;   // ← add this line
```

The existing guard `if ( awCl->awLastThinkTime == 0 ) continue;` in the antiwarp loop will then correctly skip the client until the first real usercmd arrives.

---

## BUG-3 — `sv_physicsScale` extern in `sv_antilag.h` is unused by callers — **RESOLVED**

**Resolution:** `extern cvar_t *sv_physicsScale` removed from `sv_antilag.h` along with
the cvar definition and registration (see BUG-1 resolution).

---

## BUG-4 — Fire time under-compensation (`sv.time - ping/2` should be `sv.time - ping`) — **RESOLVED**

**Resolution:** Changed `fireTime = sv.time - cl->ping / 2` to
`fireTime = sv.time - cl->ping` in `SV_Antilag_GetClientFireTime`.

**Root cause:** PR #37 introduced `ping/2` reasoning that "ping/2 approximates
one-way latency (server→client)". This is true but incomplete: the usercmd also
travels one-way back (client→server), so the total lag from when the snapshot
was recorded to when the trace runs is the full RTT (`= cl->ping`), not half.

**Timing proof:**
- `T0`: position recorded; snapshot sent (`messageSent = Sys_Milliseconds()`)
- `T0 + RTT/2`: client receives snapshot, aims at position `T0`
- `T0 + RTT`: server receives usercmd (`sv.time ≈ T0 + RTT`)
- Correct rewind: `sv.time - ping = T0` ✓
- PR #37 formula: `sv.time - ping/2 = T0 + RTT/2` ✗ (target is `RTT/2` too far forward)

**Impact at 40 ms ping:**
- Old error: ~20 ms positional lead required (~128 units at 320 ups)
- New error: ~0 ms (exact snapshot compensation)

*(Superseded by BUG-5 which adds snapshotMsec compensation on top.)*

---

## BUG-5 — Shadow recording before game frame misaligns with snapshot; missing snapshotMsec compensation — **RESOLVED**

**Resolution (two-part):**

### Part A — Recording order fixed

`SV_Antilag_RecordPositions()` in `sv_main.c` was called **before**
`GAME_RUN_FRAME`. Entity positions are updated by the game frame, so
`shadow[T]` held pre-game positions while `snapshot[T]` held post-game
positions — a one-tick mismatch at all `sv_fps` rates.

Moved `SV_Antilag_RecordPositions()` to **after** the game frame (but
before `SV_SendClientMessages()`). Now `shadow[T] == snapshot[T]`.

### Part B — snapshotMsec compensation in fireTime

Even with the recording-order fix, `fireTime = sv.time - ping` rewound to
the exact snapshot send time `T0`. But the Q3/URT client interpolates one
snapshot interval behind its latest received snapshot (`cl_interp ≈
cl->snapshotMsec`). The target was visually at `T0 - snapshotMsec`.

Changed `fireTime = sv.time - cl->ping - cl->snapshotMsec`.

`cl->snapshotMsec = 1000 / snapHz` is per-client and driven by
`sv_fps`/`sv_snapshotFps` — the formula is correct at any `sv_fps` rate.

**Timing proof (both parts applied):**
- `T0`: shadow records (post-game) + snapshot sent → `shadow[T0] == snapshot[T0]`
- `T0 + RTT/2`: client receives snapshot, sees target at `T0`
- `T0 + RTT/2`: client **renders** target at `T0 - snapshotMsec` (cl_interp)
- `T0 + RTT`: usercmd arrives, `sv.time ≈ T0 + RTT`, `cl->ping = RTT`
- Correct `fireTime = sv.time - ping - snapshotMsec = T0 - snapshotMsec` ✓

**Impact at 40 ms ping, 60 Hz snaps (snapshotMsec = 16 ms):**
- Previous formula: rewinds 40 ms → points at T0, target was at T0 − 16 ms → 16 ms error
- New formula: rewinds 56 ms → points at T0 − 16 ms ✓

**Known limitation — dropped packets:**
If a snapshot is dropped the client's effective `cl_interp` grows by
`snapshotMsec`. The server cannot detect this — the rewind will be
`snapshotMsec` too shallow for that shot. Not fixable without a
client-side interp report.

---

## NOTES (not bugs, design decisions)

### `sv_antilag` default is `0` (disabled)

By design — explicit opt-in. Server admins must set `sv_antilag 1` in their config. The `docs/Example_server.cfg` documents this. Not a bug, but new admins who don't read the docs will run with no engine antilag.
