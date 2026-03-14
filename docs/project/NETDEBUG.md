# Sporadic Vanilla-Server Disconnect — Debug Guide

> **Status:** Logging instrumentation added; awaiting real session logs.
> **Relates to:** PR #68 (serverTime safety cap), ongoing sporadics on the single populated vanilla URT4 server.

---

## How to capture a session log

```
set cl_netlog 1      // in console BEFORE connecting
connect <server>     // connect normally and play until disconnect
```

The log file is written to `<fs_basepath>/quake3/netdebug_YYYYMMDD_HHMMSS.log`.
Set `cl_netlog 2` for a heavier trace that includes per-snapshot state.

A `[DISCONNECT TRACE]` line is **always** printed to the console regardless of
`cl_netlog`, so even without a log file you will see the full state dump at
the moment of disconnect.

---

## Log event reference

| Tag | Level | Meaning |
|-----|-------|---------|
| `CONNECT` | 1 | Gamestate received: sv_fps, sv_snapshotFps, seeded snapshotMsec, vanilla/forbids flags |
| `SVCMD` | 1 | Every new server command — seq numbers + first 80 chars of text |
| `SNAP` | 2 | Per-snapshot reliable-cmd sequence state (cmdSeq, relSeq, relAck) |
| `DROP` | 1 | Netchan gap: server UDP packets that never arrived (maps to lagometer black bars) |
| `SNAP:DELTA_INVALID` | 1 | Delta-base snapshot was already invalid — should never happen |
| `SNAP:DELTA_OLD` | 1 | Delta-base too old to reconstruct — usually follows a DROP |
| `SNAP:DELTA_ENTITIES_OLD` | 1 | Entity ring buffer too old — rare |
| `WARN:RELOVERFLOW` | 1 | Client reliable-command buffer overflow recovery (relSeq − relAck wrapped) |
| `WARN:CMDCYCLED` | 1 | Server command cycled out of ring before cgame read it — precedes ERR_DROP |
| `TIMEOUT` | 1 | Timeout counter increment (logs counts 1-6; final fires DISCONNECT) |
| `CAP_RELEASE` | 1 | Safety cap released after ≥ 2 s server silence (rate-limited: 1/500 ms) |
| `RESET` | 1 | CL_AdjustTimeDelta hard-reset (deltaDelta > resetTime) |
| `FAST` | 1 | CL_AdjustTimeDelta fast-adjust (deltaDelta > fastAdjust) |
| `SNAP LATE` | 1 | Snapshot arrived later than 1.5× expected interval |
| `PING JITTER` | 1 | Measured ping changed by more than threshold in one snap |
| `QVM:SET_INTERCEPTED` | 1 | cgame QVM called trap_Cvar_Set on `snaps` or `cg_smoothClients` — silently ignored |
| `QVM:SET_BLOCKED` | 1 | cgame QVM called trap_Cvar_Set on a CVAR_PROTECTED cvar — blocked |
| `OOB:UNKNOWN` | 1 | Unknown connectionless packet from the connected server |
| `PKT:WRONG_SOURCE` | 1 | Sequenced packet received from a different address than the server |
| `DISCONNECT` | 1 | Full state dump immediately before any disconnect/ERR_DROP (also always console) |

---

## What each field in `DISCONNECT` means

```
[HH:MM:SS] DISCONNECT  reason="<string>"
  snapT=<N>  svrT=<N>  dT=<N>  ping=<N>
  cmdSeq=<N>  relSeq=<N>  relAck=<N>
  snapMs=<N>  vanilla=<0|1>  forbidsAdaptive=<0|1>
  timeout=<N>  silenceMs=<N>  capHits=<N>
```

| Field | What to look for |
|-------|-----------------|
| `reason` | Server-supplied string — should identify why the server dropped us |
| `ping=999` | Server-side negative-ping kick: `usercmd.serverTime > sv.time` at the server |
| `ping` high (>400) | High-ping kick if `sv_maxPing` is set on the server |
| `silenceMs` large | Server stopped sending packets before the disconnect — network outage or server-side kick |
| `silenceMs` small | Server sent the disconnect recently — deliberate server action |
| `capHits` high | Safety cap was firing often — `cl.serverTime` was being pushed ahead repeatedly |
| `relSeq - relAck` large | Server not acknowledging our reliable commands — upstream packet loss |
| `timeout > 0` | We were already timing out before the disconnect arrived |
| `vanilla=1` | Vanilla server path confirmed |

---

## Suspect list and current working theories

### 1. Black-bars-then-disconnect (netgraph correlated)

**Sequence:**  
`DROP` events → `SNAP:DELTA_OLD` → extrapolation → `FAST` or `RESET` adjustment → disconnect.

**Mechanism being logged:** Each `DROP` maps to one UDP packet loss. If the drop
knocks out a snapshot, `CL_ParseSnapshot` invalidates the delta chain
(`SNAP:DELTA_OLD`) which forces full-snapshot retransmit. During this gap the
client extrapolates; the lagometer shows black bars. When the burst resumes,
`CL_AdjustTimeDelta` may fire a `FAST` adjust which momentarily pushes
`cl.serverTime` ahead, causing the safety cap to fire repeatedly.

**What to look for in logs:**  
`DROP` count ≥ 1 → `SNAP:DELTA_OLD` within the same second → `FAST` or
`RESET` within 1-2 snapshots → `DISCONNECT`.

---

### 2. Clean-connection disconnect (no netgraph warning)

**This is the harder case.** No DROP, no SNAP:DELTA_OLD, no TIMEOUT — yet the
server sends a disconnect. Possible causes:

#### 2a. Server-side reliable-command overflow  
The vanilla server queues print/cs-update reliable commands for our client.
If the server sends a burst of commands (e.g. many player-join events) and
our client's acks do not reach the server fast enough (even 1-2% packet loss
upstream), the server's `cl->reliableSequence − cl->reliableAcknowledge`
reaches `MAX_RELIABLE_COMMANDS` (128) and the server calls
`SV_DropClient("reliable overflow")`.

**What to look for:**  
`reason="reliable overflow"` in `DISCONNECT`. High `capHits` or elevated
`silenceMs` at time of disconnect. `WARN:RELOVERFLOW` in our own buffer
(though that is client→server direction, not server→client).

#### 2b. Frametime hitch → CL_AdjustTimeDelta FAST/RESET → cap fires  
A system-level hitch (OS scheduler, GC, disk, v-sync stall) causes
`cls.realtime` to jump by 100–400 ms in a single frame. For a remote client
`Com_ModifyMsec` allows up to **5 000 ms** without clamping (line ~4195 in
`common.c`). The jump in `cls.realtime` propagates into `cl.serverTime`
immediately, the safety cap fires, and on the next snapshot `CL_AdjustTimeDelta`
fires a `FAST` adjust. This is **benign by itself** — the cap prevents negative
ping — but if `sv_maxPing` is tight on the server, the transient high ping
during the FAST-adjust phase could trigger a ping-kick.

**What to look for:**  
A `FAST` or `RESET` log line with `deltaDelta` ≈ 100–500 followed within
1-3 snapshots by `DISCONNECT` with `ping` close to `sv_maxPing`. Also
look for `capHits` spiking in the second before disconnect.

The **5 000 ms remote-client clamp** in `Com_ModifyMsec` (`common.c:4195`)
is worth revisiting: upstream Quake3e uses the same value but the URT vanilla
server's QVM ping-kick threshold may be lower than typical servers. Consider
whether adding a tighter client-side hitch guard (e.g. 200 ms) when `cl_maxfps`
is set would reduce transient ping spikes — **but only change this once we
have log evidence that hitches are the cause**.

#### 2c. QVM cvar intercept side-effects  
Our engine silently ignores `trap_Cvar_Set("snaps", …)` calls from the cgame
QVM. The URT4 vanilla cgame.qvm almost certainly sets `snaps` once on map load
to match `sv_fps`. With our fork, `snaps` stays at 60 when the QVM reads it
back. This is harmless for a 20 Hz server (server clamps delivery to `sv_fps`
anyway), but if the QVM makes any decision based on the current value of
`snaps` — for example computing expected snapshot intervals for a timeout check
— the wrong value could matter.

**What to look for:**  
`QVM:SET_INTERCEPTED` lines in the log. Note how many times and with what
value the QVM tries to set `snaps`. If the QVM keeps retrying repeatedly
(dozens of times per second) it may indicate a logic loop triggered by the
unexpected value.

#### 2d. CVAR_PROTECTED cvar blocked by QVM  
`cl_timeNudge` is `CVAR_PROTECTED` in our fork (upstream: `CVAR_TEMP`). If
the vanilla URT QVM attempts to set `cl_timeNudge` (which some mods do to
tune timing), `Cvar_SetSafe` will block it and print a yellow console warning.
The QVM receives no error indication and simply continues with the old value.
Depending on what the QVM expects `cl_timeNudge` to be, this could lead to
incorrect timing behaviour at the QVM level.

**What to look for:**  
`QVM:SET_BLOCKED` lines. Pay attention to which cvar is blocked and what
value the QVM wanted.

#### 2e. AUTH userinfo fields on vanilla server  
`authc` and `authl` are `CVAR_USERINFO` in our fork. Every client sends
`\authc\0\authl\` in the userinfo string to the server. The vanilla URT4
server QVM receives this and may react to `authc=0` as "unauthenticated user"
if it has any auth-gating logic, kicking with a reason like "not authenticated"
or "auth required".

**What to look for:**  
`reason` string in `DISCONNECT` containing "auth". Also check whether the
disconnect happens consistently on connect (within the first few seconds)
which would point to a connect-time auth check rather than a gameplay event.

---

## Priority reading order for the first log

1. Jump straight to the last `DISCONNECT` block.
2. Read `reason` — if it contains any server-supplied text, that is the primary clue.
3. Check `ping` — 999 means negative ping at server; normal range = positive.
4. Check `silenceMs` — if > 500 ms, there was a network gap before the disconnect.
5. Scroll back ~5 seconds to find the last `DROP` or `SNAP:DELTA_*` event.
6. Look for any `QVM:SET_INTERCEPTED` or `QVM:SET_BLOCKED` events.
7. Look for `FAST` or `RESET` events in the window before disconnect.

---

## Files changed for this instrumentation

| File | What was added |
|------|---------------|
| `code/client/cl_scrn.c` | `SCR_LogConnectInfo`, `SCR_LogServerCmd`, `SCR_LogSnapState`, `SCR_LogTimeout`, `SCR_LogNote`, `SCR_LogPacketDrop`, `SCR_LogCapRelease`, `SCR_LogDisconnect`; `netMonCapHitsSession` per-connection counter |
| `code/client/client.h` | Declarations for all new log functions |
| `code/client/cl_parse.c` | Call sites: `SCR_LogConnectInfo`, `SCR_LogServerCmd`, `SCR_LogDisconnect` (early disconnect), `SCR_LogSnapState`, `SCR_LogPacketDrop`, `SCR_LogNote` × 4 (delta failures + reloverflow); fixed stale "cap will be disabled" comment |
| `code/client/cl_cgame.c` | Call sites: `SCR_LogDisconnect` (disconnect cmd + cycled-out), `SCR_LogCapRelease` (2 s release), `QVM:SET_INTERCEPTED`/`QVM:SET_BLOCKED` logging in `CG_CVAR_SET` |
| `code/client/cl_main.c` | Call sites: `SCR_LogTimeout` + `SCR_LogDisconnect` (timeout), `SCR_LogDisconnect` (OOB disconnect), `SCR_LogNote` (unknown OOB, wrong-source packet) |
