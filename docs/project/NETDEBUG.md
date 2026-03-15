# Sporadic Vanilla-Server Disconnect — Debug Guide

> **Status:** OOB-disconnect guard added; bad-faith server protections in place; awaiting further real session logs.
> **Relates to:** PR #68 (serverTime safety cap), ongoing sporadics on the single populated vanilla URT4 server.

---

## How to capture a session log

```
set cl_netlog 1      // in console BEFORE connecting
connect <server>     // connect normally and play until disconnect
```

The log file is written to `<fs_basepath>/quake3/netdebug_YYYYMMDD_HHMMSS.log`.
Set `cl_netlog 2` for a heavier trace that includes per-snapshot state, all OOB
packets from the server, every userinfo send, and every console command the QVM
injects.

A `[DISCONNECT TRACE]` line is **always** printed to the console regardless of
`cl_netlog`, so even without a log file you will see the full state dump at
the moment of disconnect.

---

## Key cvars added for this investigation

| Cvar | Default | Description |
|------|---------|-------------|
| `cl_noOOBDisconnect` | `1` | When **1**, OOB `disconnect` packets from the server are logged but **ignored**. The normal `cl_timeout` mechanism handles cleanup if the server genuinely stops sending. Set to **0** to restore vanilla behaviour (immediate disconnect on OOB). |

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
| `WARN:RELWND` | 1 | Reliable send window is ≥ 50 % full before a userinfo send — upstream packet loss suspected |
| `WARN:CMDCYCLED` | 1 | Server command cycled out of ring before cgame read it — precedes ERR_DROP |
| `TIMEOUT` | 1 | Timeout counter increment (logs counts 1-6; final fires DISCONNECT) |
| `CAP_RELEASE` | 1 | Safety cap released after ≥ 2 s server silence (rate-limited: 1/500 ms) |
| `RESET` | 1 | CL_AdjustTimeDelta hard-reset (deltaDelta > resetTime) |
| `FAST` | 1 | CL_AdjustTimeDelta fast-adjust (deltaDelta > fastAdjust) |
| `SNAP LATE` | 1 | Snapshot arrived later than 1.5× expected interval |
| `PING JITTER` | 1 | Measured ping changed by more than threshold in one snap |
| `QVM:SET_INTERCEPTED` | 1 | cgame QVM called trap_Cvar_Set on a cvar in the engine-side block list (`snaps`, `cg_smoothClients`, `net_qport`, `net_dropsim`, `cl_packetdelay`, `cl_packetdup`, `cl_timeout`, `in_mouse`, `cl_noOOBDisconnect`, `rate=0`) — silently ignored |
| `QVM:SET_BLOCKED` | 1 | cgame QVM called trap_Cvar_Set on a CVAR_PROTECTED cvar — blocked by engine |
| `QVM:CONSOLECMD` | 2 | Every console command injected by the cgame QVM via CG_SENDCONSOLECOMMAND |
| `QVM:CONSOLECMD_BLOCKED` | 1 | QVM tried to inject a dangerous command (`disconnect`, `quit`, `exit`, `exec`) — **blocked** |
| `SVCMD:DISCONNECT` | 1 | Server sent an in-band reliable `disconnect` command (different from OOB — carries a reason string) |
| `OOB:PACKET` | 2 | Every connectionless (OOB) packet received from the connected server |
| `OOB:DISCONNECT` | 1 | OOB `disconnect` packet arrived from server — fields show whether it was honored or ignored |
| `OOB:UNKNOWN` | 1 | Unknown connectionless packet from the connected server |
| `PKT:WRONG_SOURCE` | 1 | Sequenced packet received from a different address than the server |
| `USERINFO:SEND` | 2 | Userinfo reliable command sent to server mid-session — shows name, authc, authl, snaps |
| `DISCONNECT` | 1 | Full state dump immediately before any disconnect/ERR_DROP (also always console) |

---

## What each field in `DISCONNECT` means

```
[HH:MM:SS] DISCONNECT  reason="<string>"
  snapT=<N>  svrT=<N>  dT=<N>  ping=<N>
  cmdSeq=<N>  relSeq=<N>  relAck=<N>  relWnd=<N>
  snapMs=<N>  vanilla=<0|1>  forbidsAdaptive=<0|1>
  timeout=<N>  silenceMs=<N>  capHits=<N>  oobIgnored=<N>
```

| Field | What to look for |
|-------|-----------------|
| `reason` | Server-supplied string — should identify why the server dropped us |
| `ping=999` | Client-side ping calculation failure (can happen when a snap is late and the safety cap is active — **does not necessarily mean server-side negative-ping kick**) |
| `ping` high (>400) | High-ping kick if `sv_maxPing` is set on the server |
| `silenceMs` large | Server stopped sending packets before the disconnect — network outage or server-side kick |
| `silenceMs` small | Server sent the disconnect recently — deliberate server action (possibly spurious) |
| `capHits` high | Safety cap was firing often — `cl.serverTime` was being pushed ahead repeatedly |
| `relWnd` | `relSeq − relAck` — fill level of our reliable send window (max 64). High value means the server is not acking our commands fast enough |
| `relSeq - relAck` large | Server not acknowledging our reliable commands — upstream packet loss |
| `timeout > 0` | We were already timing out before the disconnect arrived |
| `vanilla=1` | Vanilla server path confirmed |
| `oobIgnored` | Number of OOB disconnect packets silently ignored this session (see `cl_noOOBDisconnect`) |

---

## What `OOB:DISCONNECT` means vs `DISCONNECT`

`OOB:DISCONNECT` fires when an **out-of-band** `disconnect` packet arrives.
`DISCONNECT` fires when the engine actually tears down the connection.

With `cl_noOOBDisconnect 1` (default) these are **two separate events**:
- `OOB:DISCONNECT honored=0` — packet arrived, was logged, was **ignored**
- No `DISCONNECT` line follows unless the server also stops sending snapshots
  and `cl_timeout` fires

With `cl_noOOBDisconnect 0` they fire in sequence:
- `OOB:DISCONNECT honored=1` — packet arrived
- `DISCONNECT reason="OOB disconnect from server"` — engine disconnected

An `SVCMD:DISCONNECT` is a **different path** — the server sent a reliable
in-band `disconnect` command. This carries an explicit reason string and is
**always honoured** regardless of `cl_noOOBDisconnect`.

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

> **Note on ping=0 / ping=999 in logs:** When a snapshot arrives late (SNAP LATE)
> while the safety cap is active, the client-side ping calculation may return 0 ms
> (same-frame command match) or 999 (no match). These are calculation artefacts,
> **not** evidence of a server-side negative-ping kick. Check `OOB:DISCONNECT` and
> `SVCMD:DISCONNECT` events to identify the actual disconnect trigger.

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
Cross-reference with `USERINFO:SEND` entries at level 2 to see if a mid-session
userinfo update preceded the disconnect.

#### 2f. Spurious OOB disconnect (undocumented server behaviour)  
Some vanilla URT servers send an OOB `disconnect` packet as an undocumented
server-side action with no matching in-band goodbye and no clear pattern.
The client previously disconnected immediately. With `cl_noOOBDisconnect 1`
(now the default) the packet is logged as `OOB:DISCONNECT honored=0` and
ignored; the connection continues until `cl_timeout` fires if the server
genuinely stops sending.

**What to look for:**  
`OOB:DISCONNECT honored=0` entries. If `oobIgnored` in the final `DISCONNECT`
is > 0, at least one of these spurious packets was received this session.
Note the `silenceMs` in the `OOB:DISCONNECT` entry — if small (< 200 ms) the
server was still actively sending snapshots immediately before the OOB packet,
which strongly suggests spurious / undocumented behaviour rather than a real
kick.

#### 2g. Bad-faith server injecting commands via cgame  
A malicious or buggy server could send reliable commands that cause the cgame
QVM to call `trap_SendConsoleCommand` with `disconnect`, `quit`, or `exec`.
The engine now **blocks** these (logged as `QVM:CONSOLECMD_BLOCKED`).
Similarly, `trap_Cvar_Set` attempts to zero `rate` or change `net_qport` are
intercepted and logged.

**What to look for:**  
`QVM:CONSOLECMD_BLOCKED` entries. Any such entry means the server attempted
to force a client action. Cross-reference with surrounding `SVCMD` entries to
identify which server command triggered the QVM call.

---

## Priority reading order for the first log

1. Jump straight to the last `DISCONNECT` block.
2. Read `reason` — if it contains any server-supplied text, that is the primary clue.
3. Check `oobIgnored` — if > 0, look for preceding `OOB:DISCONNECT` entries.
4. Check `silenceMs` — if > 500 ms, there was a network gap before the disconnect.
5. Check `ping` — note that 999 or 0 can be a calculation artefact, not necessarily a server ping-kick.
6. Scroll back ~5 seconds to find the last `DROP`, `SNAP:DELTA_*`, or `OOB:PACKET` burst.
7. Look for any `QVM:SET_INTERCEPTED`, `QVM:SET_BLOCKED`, or `QVM:CONSOLECMD_BLOCKED` events.
8. Look for `FAST`, `RESET`, or `SVCMD:DISCONNECT` events in the window before disconnect.
9. At level 2: check `USERINFO:SEND` to see if a mid-session userinfo update preceded the disconnect.

---

## Files changed for this instrumentation

| File | What was added |
|------|---------------|
| `code/client/cl_scrn.c` | `SCR_LogConnectInfo`, `SCR_LogServerCmd`, `SCR_LogSnapState`, `SCR_LogTimeout`, `SCR_LogNote`, `SCR_LogPacketDrop`, `SCR_LogCapRelease`, `SCR_LogOOBPacket`, `SCR_LogOOBDisconnect`, `SCR_LogUserinfoSend`, `SCR_OOBIgnoredIncrement`, `SCR_LogDisconnect`; `netMonCapHitsSession`+`netMonOOBIgnoredSession` per-connection counters |
| `code/client/client.h` | Declarations for all new log functions; `cl_noOOBDisconnect` extern |
| `code/client/cl_parse.c` | Call sites: `SCR_LogConnectInfo`, `SCR_LogServerCmd`, `SCR_LogDisconnect` (early disconnect), `SCR_LogSnapState`, `SCR_LogPacketDrop`, `SCR_LogNote` × 4 (delta failures + reloverflow); fixed stale "cap will be disabled" comment |
| `code/client/cl_cgame.c` | `SVCMD:DISCONNECT` note for in-band disconnect; `CG_SENDCONSOLECOMMAND` guard (log all, block dangerous); `CG_CVAR_SET` expanded intercept list (`net_qport`, `rate=0`, `net_dropsim`, `cl_packetdelay`, `cl_packetdup`, `cl_timeout`, `in_mouse`, `cl_noOOBDisconnect`) |
| `code/client/cl_main.c` | `cl_noOOBDisconnect` cvar (`CVAR_ARCHIVE\|CVAR_PROTECTED`); `SCR_LogOOBPacket` at top of `CL_ConnectionlessPacket`; `SCR_LogOOBDisconnect` in OOB disconnect handler; `SCR_LogUserinfoSend` + `WARN:RELWND` in `CL_CheckUserinfo` |

