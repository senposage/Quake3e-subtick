# Networking — UDP, Channels, and Message Encoding

> Covers `code/qcommon/net_ip.c`, `net_chan.c`, `msg.c`, and the server/client netchan wrappers `server/sv_net_chan.c`, `client/cl_net_chan.c`.

---

## Table of Contents

1. [Layer Stack Overview](#layer-stack-overview)
2. [net_ip.c — OS Socket Abstraction](#net_ipc--os-socket-abstraction)
3. [net_chan.c — Reliable Channel Protocol](#net_chanc--reliable-channel-protocol)
4. [msg.c — Message Serialization](#msgc--message-serialization)
5. [LZSS Compression](#lzss-compression)
6. [Server / Client Netchan Wrappers](#server--client-netchan-wrappers)
7. [Out-of-Band (OOB) Packets](#out-of-band-oob-packets)
8. [Packet Flow Diagrams](#packet-flow-diagrams)
9. [Debug & Simulation Cvars](#debug--simulation-cvars)
10. [Key Data Structures](#key-data-structures)

---

## Layer Stack Overview

```
Application (server/client game logic)
        │
        ▼
msg_t  ─── message bit-packing / delta compression
        │
        ▼
netchan_t ─ sequence numbers, reliability, fragmentation
        │
        ▼
NET_*  ─── UDP socket send/receive (IPv4 + IPv6)
        │
        ▼
OS network stack
```

Each layer knows nothing about the ones above it. The QVM only ever touches the top layer.

---

## net_ip.c — OS Socket Abstraction

**File:** `code/qcommon/net_ip.c`  
**Size:** ~1946 lines  
**Custom change:** `net_dropsim` cvar changed from `CVAR_TEMP` to `CVAR_CHEAT`

### Key Types

```c
typedef enum {
    NA_BAD = 0,         // invalid
    NA_LOOPBACK,        // loopback (listen server)
    NA_BROADCAST,       // LAN broadcast
    NA_IP,              // IPv4
    NA_IP6,             // IPv6
    NA_MULTICAST6,      // IPv6 multicast
    NA_UNSPEC,          // unspecified (for binding)
    NA_BOT              // bot client (no network)
} netadrtype_t;

typedef struct {
    netadrtype_t type;
    byte         ip[4];    // IPv4
    byte         ip6[16];  // IPv6
    unsigned short port;   // network byte order
    unsigned long scope_id; // IPv6 scope
} netadr_t;
```

### Initialization

`NET_Init()` → `NET_Config(enableNetworking=true)`:
- Reads cvars: `net_port` (default 27960), `net_ip`, `net_ip6`, `net_enabled`
- Opens UDP sockets for IPv4 and/or IPv6
- Sets up IPv6 multicast group for LAN server discovery

### Key Functions

| Function | What It Does |
|---|---|
| `NET_Init()` | Open sockets, register cvars |
| `NET_Shutdown()` | Close all sockets |
| `Sys_SendPacket(len, data, to)` | Send raw UDP packet to address |
| `NET_Sleep(timeout_usec)` | `select()` wait for incoming data |
| `NET_GetLoopPacket(sock, from, msg)` | Read from loopback queue (listen server) |
| `NET_SendLoopPacket(sock, len, data)` | Write to loopback queue |
| `NET_FlushPacketQueue()` | Send all queued delayed packets |
| `NET_CompareAdr(a, b)` | Full address compare (IP + port) |
| `NET_CompareBaseAdr(a, b)` | IP-only compare (ignore port) |
| `NET_IsLocalAddress(adr)` | True for loopback or LAN address |
| `Sys_StringToAdr(s, a, family)` | DNS resolve + parse "host:port" |
| `NET_OutOfBandPrint(sock, to, fmt, ...)` | Send OOB string packet |

### Packet Delay Simulation (`cl_packetdelay`, `sv_packetdelay`)

```c
// A queue of pending outgoing packets
typedef struct {
    netadr_t    to;
    int         release;  // Sys_Milliseconds() when to actually send
    int         length;
    byte        data[MAX_PACKETLEN];
} bufferedPacket_t;
```

`NET_QueuePacket(len, data, to, offset_ms)` stores the packet; `NET_FlushPacketQueue()` sends all packets whose `release` time has passed. Called from `Com_Frame`.

### net_dropsim [CUSTOM]

`net_dropsim` simulates packet loss. Changed from `CVAR_TEMP` to `CVAR_CHEAT` to match `cl_packetdelay`. Both are testing tools and should be equally restricted on game servers.

```c
// In NET_Event, each received packet is checked:
if (net_dropsim->value > 0.0f && net_dropsim->value <= 100.0f) {
    if (rand() < (int)(RAND_MAX / 100.0 * net_dropsim->value))
        continue; // drop this packet
}
```

---

## net_chan.c — Reliable Channel Protocol

**File:** `code/qcommon/net_chan.c`  
**Size:** ~688 lines

### Packet Header Layout

```
4 bytes:  outgoing sequence (high bit set = fragmented message)
2 bytes:  qport (client→server only)
[if fragmented:]
  2 bytes: fragment start offset
  2 bytes: fragment length
```

If sequence == `-1` (all 1s), the packet is an out-of-band message handled directly (no channel state).

### Sequence Numbers

Each `netchan_t` maintains:
- `outgoingSequence` — incremented on each fully-sent message
- `incomingSequence` — last sequence received without gap
- Received packets with sequence ≤ `incomingSequence` are dropped (duplicate/old)
- Reliable channel: the sequence itself IS the reliability mechanism. Lost packets are detected when the next packet's sequence jumps by more than 1.

### Fragmentation

Large messages (> `FRAGMENT_SIZE` = ~1300 bytes) are split and reassembled:
- Each fragment shares the same sequence number + FRAGMENT_BIT
- `unsentFragmentStart` tracks how far through the unsent buffer we are
- `Netchan_TransmitNextFragment()` sends one fragment per call
- Receiver reassembles into `fragmentBuffer` until `fragmentLength < FRAGMENT_SIZE` (final fragment)

### Reliability / Dropped Packet Detection

Q3 uses a **fire-and-forget** model for game updates — snapshots are sent unreliably. Only the reliable command channel (text commands) uses a separate reliable buffer system inside the packet.

The server tracks `cl->reliableSequence` and `cl->reliableAcknowledge`. Reliable messages are retransmitted until acknowledged. The `reliable` field in the packet header contains the latest unacked reliable message(s).

### Key Functions

| Function | What It Does |
|---|---|
| `Netchan_Init(port)` | Register showpackets/showdrop/qport cvars |
| `Netchan_Setup(sock, chan, adr, port, challenge, compat)` | Initialize a channel to a remote address |
| `Netchan_Transmit(chan, len, data)` | Send message; fragments if > FRAGMENT_SIZE |
| `Netchan_TransmitNextFragment(chan)` | Send next pending fragment |
| `Netchan_Process(chan, msg)` | Receive and validate; returns false if out-of-order/dropped |
| `NET_GetLoopPacket(sock, from, msg)` | Receive from loopback |
| `NET_SendLoopPacket(sock, len, data)` | Send to loopback |
| `NET_OutOfBandPrint(sock, to, fmt, ...)` | Send OOB text packet |
| `NET_OutOfBandCompress(sock, to, data, len)` | Send OOB compressed data |
| `NET_StringToAdr(s, a, family)` | DNS lookup for connection |

### qport

A random 16-bit port number set at startup (`net_qport`). Included in every client→server packet to disambiguate clients behind NAT when their source port changes. The server uses qport + IP to identify a connection even if the external UDP port changes mid-session.

---

## msg.c — Message Serialization

**File:** `code/qcommon/msg.c`  
**Size:** ~2004 lines

### msg_t Structure

```c
typedef struct {
    qboolean    allowoverflow;
    qboolean    overflowed;
    qboolean    oob;          // out-of-band mode (no huffman)
    byte        *data;
    int         maxsize;
    int         cursize;      // bytes written/read so far
    int         readcount;
    int         bit;          // bit position within current byte
} msg_t;
```

### Bit-Level Packing

`MSG_WriteBits(msg, value, bits)` / `MSG_ReadBits(msg, bits)`:
- Writes exactly `abs(bits)` bits, packing multiple values into bytes
- Negative `bits` = signed value (sign bit first)
- `oob=true` bypasses bit packing (byte-aligned only, faster)

### Primitive Writers/Readers

| Write | Read | Size |
|---|---|---|
| `MSG_WriteChar(v)` | `MSG_ReadChar()` | 8 bits |
| `MSG_WriteByte(v)` | `MSG_ReadByte()` | 8 bits unsigned |
| `MSG_WriteShort(v)` | `MSG_ReadShort()` | 16 bits |
| `MSG_WriteLong(v)` | `MSG_ReadLong()` | 32 bits |
| `MSG_WriteFloat(v)` | `MSG_ReadFloat()` | 32 bits IEEE |
| `MSG_WriteString(s)` | `MSG_ReadString(buf,n)` | NUL-terminated |
| `MSG_WriteBigString(s)` | `MSG_ReadBigString(buf,n)` | Up to BIG_INFO_STRING |
| `MSG_WriteAngle(f)` | `MSG_ReadAngle()` | 8 bits (256 divisions) |
| `MSG_WriteAngle16(f)` | `MSG_ReadAngle16()` | 16 bits (65536 divisions) |

### Delta Key Encoding

All game-state fields use delta+key encoding to reduce bandwidth:

```c
MSG_WriteDeltaKey(msg, key, oldV, newV, bits):
    if oldV == newV: write 0 bit (no change)
    else: write 1 bit + (newV XOR key)  // XOR with key for anti-cheat

MSG_ReadDeltaKey(msg, key, oldV, bits):
    if read 0 bit: return oldV
    else: return read_bits(bits) XOR (key & bitmask[bits-1])
```

The `key` is derived from the message sequence number and server time — makes it hard to inject fake delta values.

### Delta Usercmd Encoding

`MSG_WriteDeltaUsercmdKey` / `MSG_ReadDeltaUsercmdKey`:
- serverTime delta: if < 256ms → 8 bits; else → 32 bits (saves 3 bytes typically)
- Each field (angles[3], forwardmove, rightmove, upmove, buttons, weapon) individually delta-encoded
- Full no-change detection: if nothing changed, only 1 bit written

### Delta Entity State Encoding

`MSG_WriteDeltaEntity` / `MSG_ReadDeltaEntity`:
- Uses `netField_t` table of all `entityState_t` fields with their bit widths
- Changed-field bitmap written first (1 bit per field)
- Only changed fields transmitted
- Float fields transmitted as 32-bit IEEE; int fields as N bits each
- `force=true` sends all fields regardless (used for baseline)

### Player State Delta Encoding

`MSG_WriteDeltaPlayerstate` / `MSG_ReadDeltaPlayerstate`:
- Same principle as entity state but for `playerState_t`
- Separate change mask, more fields, larger struct
- Velocity, origin sent as 16-bit fixed point at 1/8-unit precision

### Engine-to-Client msg_t Function: `MSG_PlayerStateToEntityState`

`MSG_PlayerStateToEntityState(ps, es, snap, sm)`:
- Copies player state to entity state for the entity-state delta system
- Called by `SV_BuildCommonSnapshot` at QVM game frame rate
- Sets `es->pos.trType = TR_INTERPOLATE`, `es->pos.trBase = ps->origin`
- **This is the stock path** — the custom sv_extrapolate / sv_smoothClients code in `sv_snapshot.c` runs AFTER this and overwrites `trType` and `trBase` as needed

---

## LZSS Compression

**In:** `code/qcommon/msg.c` (bottom ~200 lines)

Used to compress large server messages (primarily snapshot packets at high snapshot rates). Q3e's custom addition:

```c
LZSS_InitContext(lzctx_t *ctx)   — init sliding window
LZSS_CompressToStream(ctx, stream, in, len) — compress into lzstream_t
MSG_WriteLZStream(msg, stream)    — serialize compressed stream into msg_t
LZSS_Expand(ctx, msg, out, maxsize, charbits) — decompress from msg_t
```

**When used:** `SV_BuildCompressedBuffer` (sv_snapshot.c) uses LZSS when snapshot payload exceeds threshold. The client's `CL_ParseSnapshot` detects the compressed flag and calls `LZSS_Expand`.

---

## Server / Client Netchan Wrappers

### sv_net_chan.c — Server Side

`SV_Netchan_Transmit(client_t *cl, msg_t *msg)`:
- Calls `Netchan_Transmit` for reliable channel
- Also handles `cl_packetdelay` / `sv_packetdelay` if set
- Tracks bytes sent for per-client rate limiting

`SV_Netchan_Process(client_t *cl, msg_t *msg)`:
- Validates sequence numbers
- Updates `cl->lastPacketTime` for timeout detection

### cl_net_chan.c — Client Side

`CL_Netchan_Transmit(netchan_t *chan, msg_t *msg)`:
- Adds demo recording byte injection if recording a demo

`CL_Netchan_Process(netchan_t *chan, msg_t *msg)`:
- Calls `Netchan_Process`
- If recording a demo, writes the packet to demo file via `CL_WriteDemoMessage`

---

## Out-of-Band (OOB) Packets

OOB packets have sequence number = -1 (0xFFFFFFFF). They bypass the channel entirely and are used for:

| Direction | Purpose | Packet |
|---|---|---|
| Client → Master | Server listing request | `getservers` |
| Server → Master | Registration heartbeat | `heartbeat` |
| Client → Server | Connection request | `getchallenge` |
| Server → Client | Challenge response | `challengeResponse` |
| Client → Server | Connection | `connect` |
| Server → Client | Connection response | `connectResponse` |
| Any → Any | Ping | `ping` / `ack` |
| Client → Server | Status / info request | `getstatus` / `getinfo` |

OOB handlers are registered in `CL_ConnectionlessPacket` (client) and `SV_ConnectionlessPacket` (server).

---

## Packet Flow Diagrams

### Client → Server (usercmd)

```
CL_SendCmd()
  build usercmd from input state
  MSG_WriteDeltaUsercmdKey() for each pending usercmd
  SV_Netchan_Transmit() / Netchan_Transmit()
    → fragmented if > FRAGMENT_SIZE
    → NET_SendPacket() → UDP socket
```

### Server → Client (snapshot)

```
SV_Frame() → SV_SendClientMessages() → SV_SendClientSnapshot()
  SV_BuildClientSnapshot()  — PVS culling, entity list
  SV_WriteSnapshotToClient() — MSG_WriteDeltaPlayerstate + MSG_WriteDeltaEntity ×N
  [optional: LZSS_CompressToStream if large]
  SV_Netchan_Transmit()
    → fragmented if > FRAGMENT_SIZE
    → NET_SendPacket() → UDP socket

Client receives:
  CL_Netchan_Process() → CL_ParseSnapshot()
    MSG_ReadDeltaPlayerstate()
    MSG_ReadDeltaEntity() × N entities
    cl.newSnapshots = true → CL_AdjustTimeDelta()
```

---

## Debug & Simulation Cvars

| Cvar | Default | Notes |
|---|---|---|
| `showpackets` | 0 | Print every packet sent/received |
| `showdrop` | 0 | Print dropped out-of-order packets |
| `net_dropsim` | "" | % chance to drop incoming packets (CHEAT) **[CUSTOM]** |
| `cl_packetdelay` | 0 | Add N ms delay to client outgoing packets |
| `sv_packetdelay` | 0 | Add N ms delay to server outgoing packets |
| `net_port` | 27960 | UDP port to listen on |
| `net_ip` | "localhost" | Bind address for IPv4 |
| `net_ip6` | "::" | Bind address for IPv6 |
| `net_enabled` | 1 | Bitmask: 1=IPv4, 2=IPv6 |
| `net_qport` | random | Client qport for NAT traversal |

---

## Key Data Structures

### netchan_t — Channel State

```c
typedef struct {
    netsrc_t        sock;               // NS_CLIENT or NS_SERVER
    netadr_t        remoteAddress;
    int             qport;
    int             incomingSequence;   // last received in order
    int             outgoingSequence;   // next to send
    int             challenge;          // for checksum
    qboolean        compat;             // protocol compat flag
    qboolean        isLANAddress;
    // fragmentation state:
    qboolean        unsentFragments;
    int             unsentFragmentStart;
    int             unsentLength;
    byte            unsentBuffer[MAX_MSGLEN];
    // reassembly state:
    qboolean        fragmentSequence;
    int             fragmentLength;
    byte            fragmentBuffer[MAX_MSGLEN];
} netchan_t;
```

### netadr_t — Network Address

```c
typedef struct {
    netadrtype_t    type;      // NA_IP, NA_IP6, NA_BOT, etc.
    byte            ip[4];     // IPv4
    byte            ip6[16];   // IPv6
    unsigned short  port;
    unsigned long   scope_id;  // IPv6 link-scope
} netadr_t;
```

### msg_t — Message Buffer

```c
typedef struct {
    qboolean    allowoverflow;
    qboolean    overflowed;    // write past end
    qboolean    oob;           // out-of-band (no huffman, byte-aligned)
    byte        *data;
    int         maxsize;
    int         cursize;
    int         readcount;
    int         bit;
} msg_t;
```

### netField_t — Entity/PlayerState Field Descriptor

```c
typedef struct {
    const char  *name;
    const int    offset;   // byte offset in entityState_t
    const int    bits;     // 0 = float, >0 = integer bit count
} netField_t;
```
