# g_antiwarp → Engine-Side Feasibility Analysis

## Bottom Line

**At `sv_gameHz 20`, g_antiwarp works correctly at any `sv_fps` (40, 60, 120+).**
It does not insert stale or unnecessary latency regardless of how high `sv_fps` is
set. `sv_fps` controls the engine tick rate (input sampling, snapshot cadence); it
does not affect the game-frame cadence that antiwarp operates on.

The `sv_gameHz` decoupling is the critical piece: `GAME_RUN_FRAME` fires at exactly
20Hz regardless of `sv_fps`, so `G_RunClient` sees a 50ms game frame every time it
fires. The hardcoded `serverTime += 50` blank command matches that 50ms frame exactly
— correct by construction.

**At `sv_gameHz 0` (disabled), g_antiwarp is broken at any `sv_fps != 20`.**
When `sv_gameHz <= 0`, the effective game-frame rate falls back to `sv_fps`, so
`level.time` advances by `1000/sv_fps` ms each tick. The hardcoded 50ms blank cmd
is then mismatched:

| sv_fps (with sv_gameHz 0) | gameMsec | Injected cmd | Effect |
|---------------------------|----------|--------------|--------|
| 20 | 50ms | 50ms | Correct — accidental match |
| 40 | 25ms | 50ms | **Double** — 2× intended step per frame |
| 60 | 16ms | 50ms | **3× intended** — lagging players teleport |
| 120 | 8ms | 50ms | **6× intended** — severe teleport |

This is the same class of breakage as raising `sv_gameHz` above 20. **Do not use
`sv_gameHz 0` with UT QVM unless `sv_fps` is also set to 20.**

The 50ms hardcode also becomes a problem if `sv_gameHz` is **raised above 20** (e.g.
to 40Hz), where each game frame is only 25ms but the blank cmd is still 50ms —
double the intended step. That is an entirely separate concern from `sv_fps`.

The rest of this document analyses feasibility of moving or modifying the antiwarp
logic, which is relevant if `sv_gameHz 0` is used at `sv_fps != 20`, or if
`sv_gameHz > 20` is ever enabled.

---

## What g_antiwarp Does

The UT4.2 QVM implements antiwarp in `g_active.c:G_RunClient()`. When the server
runs a game frame and a client has **not** sent a fresh usercmd (i.e. the client is
lagging), the QVM manufactures a blank command and runs `ClientThink_real` anyway:

```c
// g_active.c ~line 1806 (UT4.2 reference source)
// G_RunClient — called once per game frame per client
if ( level.time - client->pers.cmd.serverTime > 200 ) {
    // Client is badly lagging; teleport them forward so they don't
    // appear frozen to everyone else.
    client->pers.cmd.serverTime = level.time;
} else {
    // Normal lag: inject a blank usercmd one game-frame forward
    client->pers.cmd.serverTime += 50;  // HARDCODED: 1 game frame = 50ms
}
ClientThink_real( ent );
```

**What it produces:** a zero-input Pmove step of exactly `gameMsec` (50ms at
`sv_gameHz 20`). The lagging player moves forward at their last known velocity for
one game frame — preventing them from appearing frozen and giving them approximate
positional continuity.

**What it does NOT do:** it is not a cheat-detection system. It does not kick or
penalise players. It is purely a visual/physics continuity measure.

---

## Why This Is Hard to Move Engine-Side (As-Is)

### 1. The engine does not call ClientThink_real — the QVM does

The blank-command injection and subsequent `ClientThink_real` call are entirely
inside the QVM's `G_RunClient`. The engine calls `GAME_RUN_FRAME` → QVM
`G_RunFrame` → `G_RunClient` for every active client. The engine has no visibility
into whether `G_RunClient` decided to inject or not, and it has no way to call
`ClientThink_real` directly (that's a QVM-internal function).

### 2. The hardcoded 50ms is the problem, not the injection concept

The injection itself is sound. The problem is that the 50ms hardcode means the
blank command spans exactly one 20Hz game frame. At any other `sv_gameHz`:

| sv_gameHz | gameMsec | Injected cmd | Effect |
|-----------|----------|--------------|--------|
| 20 | 50ms | 50ms | Correct — one full game frame of coasting |
| 40 | 25ms | 50ms | **Double** — teleports forward 2× intended distance per frame |
| 10 | 100ms | 50ms | **Half** — lagging player barely moves |
| 0 (sv_fps 60) | 16ms | 50ms | **3× intended** — same breakage as sv_gameHz 60 |
| 0 (sv_fps 20) | 50ms | 50ms | Correct — accidental match only |

This is a QVM-side value. The engine cannot patch it without modifying the QVM
binary (Ghidra patch) or having the QVM read it from a cvar.

### 3. The engine cannot suppress the QVM's injection

The engine could detect that a client sent no new usercmd this game frame (by
comparing `cl->lastUsercmd.serverTime` to `sv.gameTime`). But it cannot prevent
the QVM from running `G_RunClient` with the injected 50ms blank cmd — that all
happens inside the single `VM_Call(gvm, GAME_RUN_FRAME, ...)`.

---

## What Could Be Done Engine-Side

### Option A — Cvar-feed the frame duration (feasible, low risk)

The QVM could be patched (Ghidra binary patch) to read the blank-cmd step size
from a cvar instead of the hardcoded 50:

```c
// Patched version in QVM binary
client->pers.cmd.serverTime += trap_Cvar_VariableIntegerValue("sv_gameHz_msec");
```

The engine would register `sv_gameHz_msec` as a read-only cvar and set it to
`1000 / sv_gameHz->integer` on each frame. This is the **lowest-risk approach**:

- Engine change: trivial (one cvar, updated each frame tick in `SV_Frame`)
- QVM change: one Ghidra patch to replace the literal `50` with a cvar read
- No semantic change to antiwarp logic — same injection, correct duration

**Feasibility: HIGH** — if the literal `50` can be located and patched in the
UT4.3.4 QVM bytecode.

### Option B — Engine-side blank-cmd injection before GAME_CLIENT_THINK (feasible, medium risk)

Instead of the QVM doing it inside `G_RunClient`, the engine could detect a lagging
client and pre-fill `cl->lastUsercmd.serverTime` with the correct game-frame-aligned
value before `GAME_CLIENT_THINK` fires. The QVM would then see a fresh command with
the right timestamp and skip its own injection path.

```c
// In SV_Frame, inside the GAME_RUN_FRAME inner loop — BEFORE VM_Call(GAME_RUN_FRAME)
for ( i = 0; i < sv_maxclients->integer; i++ ) {
    client_t *cl = &svs.clients[i];
    if ( cl->state != CS_ACTIVE ) continue;
    if ( cl->netchan.remoteAddress.type == NA_BOT ) continue;
    playerState_t *ps = SV_GameClientNum( i );
    // If no new cmd arrived since last game frame, inject a blank one
    // at the current game time so the QVM sees a 'fresh' cmd of exactly gameMsec.
    if ( ps->commandTime < sv.gameTime - _gameMsec ) {
        cl->lastUsercmd.serverTime = sv.gameTime;
        // zero the movement fields but preserve buttons if needed
    }
}
```

**Risks:**
- The QVM may STILL run its own injection if `pers.cmd.serverTime` does not match
  what it expects. The engine and QVM would double-inject.
- Without knowing exactly how UT4.3.4's `G_RunClient` determines "no fresh cmd",
  there is a risk of off-by-one mismatches between the engine-side pre-fill and the
  QVM path.
- Needs careful testing to confirm the QVM's injection is actually suppressed when
  it sees a cmd at `level.time`.

**Feasibility: MEDIUM** — workable but requires in-game testing to confirm the QVM
path is correctly bypassed.

### Option C — Engine-side antiwarp tracking for *observability only* (feasible, zero risk)

Even without fixing the QVM, the engine can track when it believes a client is in
an antiwarp-injection frame by comparing `cl->lastUsercmd.serverTime` against
`sv.gameTime`. This is already implied by the multi-step Pmove logic in
`sv_client.c`. Extending it to log/count "antiwarp frames per client" would give
useful diagnostic data:

- How often is each client triggering antiwarp injection?
- Is the 50ms hardcode causing visible position jumps at higher `sv_gameHz`?
- Is UT4.3.4 handling it differently than UT4.2?

This is pure instrumentation — zero gameplay risk.

**Feasibility: HIGH, immediate**

---

## What Cannot Be Done Engine-Side (Hard Limits)

| Desired change | Why it can't be done engine-side |
|----------------|----------------------------------|
| Fix the 50ms hardcode | Literal inside QVM bytecode; needs Ghidra patch |
| Suppress the injection entirely | Runs inside `GAME_RUN_FRAME` call, not interceptable |
| Replace with engine-native antiwarp | Would still need QVM to not double-inject; same problem |
| Read the antiwarp threshold (`level.time - 200`) | QVM-internal state, not exposed via any syscall |

---

## Relationship to sv_antilag.c

The existing `sv_antilag.c` is a good structural home for Option C (observability),
and could host Option B (engine-side blank-cmd pre-fill) alongside the existing
shadow history and rewind machinery. Option A is a QVM-side change and belongs in
the Ghidra patch documentation (`docs/ghidra-cgame-patches.md` or a new
`docs/ghidra-qagame-patches.md`).

A new `SV_Antiwarp_TrackClient()` function in `sv_antilag.c` could:

1. Detect each frame whether the client's `commandTime` is stale (no new usercmd
   arrived this game frame)
2. Increment a per-client counter and log when debug is enabled
3. (Option B) Pre-fill `cl->lastUsercmd.serverTime` if the engine-side injection is
   deemed safe after testing

---

## Recommended Approach (in order)

1. **Now (zero risk):** Add per-client antiwarp-frame counter to `sv_antilag.c`,
   exposed via `sv_antilagDebug`. Gives data needed to assess whether the 50ms
   hardcode is actually causing visible problems at the target `sv_gameHz`.

2. **After UT4.3.4 testing confirms the issue:** If 50ms injection at higher
   `sv_gameHz` is confirmed broken, pursue Option A (Ghidra patch of the literal
   `50` in the qagame QVM). This is the cleanest fix with no engine risk.

3. **Only if Ghidra patch is not viable:** Prototype Option B (engine pre-fill),
   test extensively for double-injection edge cases.

---

## Current Status

The engine's `sv_gameHz` decoupling already prevents the **rate** of injections from
multiplying (at `sv_gameHz 20`, the QVM still fires `G_RunClient` at 20Hz regardless
of `sv_fps`). The `serverTime += 50` hardcode is therefore only a problem if
`sv_gameHz` itself is raised above 20 — which is gated on the 4.3.4 testing
described in the main docs.

**At `sv_gameHz 20` (current default), g_antiwarp works correctly at any `sv_fps`.**
