# qcommon — Core Engine Subsystem

> Covers all files in `code/qcommon/`. This is the foundation of the engine — every other subsystem depends on it. No game-specific or platform-specific code lives here.

---

## Table of Contents

1. [common.c — Engine Loop & Memory](#commonc--engine-loop--memory)
2. [cvar.c — Console Variables](#cvarc--console-variables)
3. [cmd.c — Command System](#cmdc--command-system)
4. [files.c — Virtual Filesystem](#filesc--virtual-filesystem)
5. [q_shared.h/c — Shared Math & Utilities](#q_sharedhc--shared-math--utilities)
6. [q_math.c — Math Library](#q_mathc--math-library)
7. [huffman.c — Huffman Compression](#huffmanc--huffman-compression)
8. [md4.c / md5.c — Hash Functions](#md4c--md5c--hash-functions)
9. [Key Data Structures](#key-data-structures)

> Networking (msg.c, net_chan.c, net_ip.c), Collision (cm_*.c), and VM (vm*.c) are covered in their own section docs.

---

## common.c — Engine Loop & Memory

**File:** `code/qcommon/common.c`  
**Size:** ~4757 lines  
**Purpose:** The central spine — `Com_Init`, `Com_Frame`, all memory allocators, event queue, timing, error handling.

### Com_Init(char *commandLine)
The full engine startup sequence, called once from `main()` (or platform equivalent):
```
Com_Init:
  Com_InitPushEvent()        — zero event ring buffer
  Com_InitSmallZoneMemory()  — ~512KB small-object heap (UI strings etc.)
  Com_InitZoneMemory()       — main heap (~32MB default, com_zoneMegs)
  Cvar_Init()                — cvar hash table
  Cmd_Init()                 — command table + built-ins (exec, echo, etc.)
  Com_ParseCommandLine()     — parse +command and +set from argv
  FS_InitFilesystem()        — search paths, PAK/PK3 loading
  Com_InitHunkMemory()       — large one-time hunk (~128MB, com_hunkMegs)
  Com_EarlyParseCmdLine()    — vid_xpos/vid_ypos, console title
  NET_Init()                 — sockets open
  Netchan_Init(port)         — channel layer
  VM_Init()                  — QVM dispatch table
  SV_Init()                  — server cvars, antilag [CUSTOM]
  CL_Init()                  — client subsystem (renderer, sound, UI)
  Com_AddStartupCommands()   — execute +set / +exec from command line
```

### Com_Frame(qboolean noDelay)
The main game loop tick. Called once per frame by platform main loop:
```
Com_Frame:
  1. Rate-limiting: spin/sleep until minMsec elapsed
     - dedicated server: SV_FrameMsec() [CUSTOM: respects sv_busyWait]
     - client: com_maxfps, com_maxfpsUnfocused
  2. com_frameTime = Com_EventLoop()   — drain all queued events
     (input, network, console — all Sys_QueEvent sources)
  3. Cbuf_Execute()                    — run queued console commands
  4. msec = Com_ModifyMsec(realMsec)   — clamp runaway frames
  5. SV_Frame(msec)                    — SERVER TICK
  6. CL_Frame(msec)                    — CLIENT TICK
     (skipped on dedicated servers)
  7. Write configuration if modified
```

### Event System

Events flow from platform drivers → `Sys_QueEvent()` → ring buffer → `Com_EventLoop()` dispatches them.

| `sysEventType_t` | Meaning |
|---|---|
| `SE_KEY` | Key press/release |
| `SE_CHAR` | Character input (typed text) |
| `SE_MOUSE` | Mouse motion |
| `SE_JOYSTICK_AXIS` | Joystick axis |
| `SE_CONSOLE` | Console text line (stdin on dedicated) |
| `SE_PACKET` | Network packet arrived |

`Com_EventLoop()` returns the time of the last event processed, which becomes `com_frameTime`.

### Memory System

Three separate allocators — each for a different lifetime:

#### Zone Allocator (`Z_Malloc`, `Z_Free`, `Z_TagMalloc`)
- **For:** General-purpose dynamic allocations (UI, strings, effects)
- **Mechanism:** Two zones: `mainzone` (~32 MB) + `smallzone` (~512 KB)
- **Each block:** `memblock_t` header with size, tag, prev/next
- **Free list:** Segregated by 8-byte size classes for O(1) allocation
- **Tags:** `TAG_FREE=0`, `TAG_GENERAL=1`, `TAG_BOTLIB`, `TAG_RENDERER`, etc. — `Z_FreeTags(tag)` frees entire category
- **Key functions:**
  - `Z_Malloc(size)` → allocates from mainzone, TAG_GENERAL
  - `Z_TagMalloc(size, tag)` → allocates with specific tag
  - `Z_Free(ptr)` → free single block, merges adjacent free blocks
  - `Z_FreeTags(tag)` → free all blocks with matching tag
  - `Z_AvailableMemory()` → bytes free in mainzone
  - `S_Malloc(size)` → small zone allocation

#### Hunk Allocator (`Hunk_Alloc`, `Hunk_AllocateTempMemory`)
- **For:** Map data, models, sounds — large per-map allocations
- **Mechanism:** Linear bump allocator. Allocates from both ends: permanent from low end (`h_low`), temp from high end (`h_high`)
- **Mark system:** `Hunk_SetMark()` / `Hunk_ClearToMark()` to free map-specific allocations on map change
- **Temp memory:** `Hunk_AllocateTempMemory(size)` / `Hunk_FreeTempMemory(buf)` — stack-discipline from high end
- **Never individually freed** — only cleared by `Hunk_Clear()` (map load/restart)
- **Key functions:**
  - `Hunk_Alloc(size, h_low/h_high)` → permanent or temp allocation
  - `Hunk_AllocateTempMemory(size)` → temp block from high end
  - `Hunk_FreeTempMemory(buf)` → free temp block (must be LIFO order)
  - `Hunk_SetMark()` / `Hunk_ClearToMark()` → save/restore hunk position
  - `Hunk_Clear()` → wipe everything (full restart)
  - `Hunk_MemoryRemaining()` → bytes left

#### Stack Allocator (`alloca` / `TempMemory`)
- Used for small per-function temporaries — not tracked by engine

### Error Handling

`Com_Error(errorParm_t code, fmt, ...)` — non-recoverable handler:

| Code | Action |
|---|---|
| `ERR_FATAL` | Print to console, `longjmp` out of frame, exit process |
| `ERR_DROP` | Drop to main menu, `longjmp` out of frame (map error) |
| `ERR_DISCONNECT` | Drop client connection |
| `ERR_SERVERDISCONNECT` | Server kicked client |
| `ERR_NEED_CD` | Missing game CD/assets |

Uses `Q_setjmp(abortframe)` in `Com_Frame` to catch `ERR_DROP` without crashing.

### Timing Functions

| Function | Returns | Notes |
|---|---|---|
| `Com_Milliseconds()` | int ms since engine start | Wraps `Sys_Milliseconds()` |
| `Com_RealTime(qtime_t*)` | calendar time struct | `time_t` → `qtime_t` |
| `Com_Microseconds()` (Sys) | int64 μs | For profiling |

### Console Output / Redirection

`Com_Printf(fmt, ...)` — all console output goes here:
- Checks `rd_buffer` first (used by rcon redirect)
- Writes to developer console
- Logs to `qconsole.log` if `com_logfile` set
- Calls `CL_ConsolePrint` (client) or `Sys_Print` (dedicated)

`Com_DPrintf` — only prints if `developer 1`

`Com_BeginRedirect(buf, flush)` / `Com_EndRedirect()` — captures all output into buffer for rcon responses

---

## cvar.c — Console Variables

**File:** `code/qcommon/cvar.c`  
**Size:** ~2128 lines

### What Is a Cvar

A `cvar_t` is a named string/float/int variable that can be:
- Set by console, config files, command line, or code
- Read by any subsystem
- Protected with flags (ROM, CHEAT, SERVERINFO, SYSTEMINFO, LATCH, etc.)
- Grouped (`cvarGroup_t`) for batch change detection

### cvar_t Structure

```c
typedef struct cvar_s {
    char        *name;
    char        *string;
    char        *resetString;
    char        *latchedString;  // value waiting for restart
    int          flags;
    qboolean     modified;       // set whenever value changes
    float        value;
    int          integer;
    struct cvar_s *next;         // global list
    struct cvar_s *hashNext;     // hash bucket
} cvar_t;
```

### Flags

| Flag | Meaning |
|---|---|
| `CVAR_ARCHIVE` | Save to `quake3.cfg` on exit |
| `CVAR_USERINFO` | Include in client userinfo string sent to server |
| `CVAR_SERVERINFO` | Include in server info string shown to clients |
| `CVAR_SYSTEMINFO` | Include in system info (replicated to clients) |
| `CVAR_INIT` | Set only at engine init; changes ignored after |
| `CVAR_LATCH` | Change takes effect only after vid_restart / map load |
| `CVAR_ROM` | Read-only — can only be set by engine code |
| `CVAR_TEMP` | Not saved; ephemeral |
| `CVAR_CHEAT` | Only changeable when sv_cheats is enabled |
| `CVAR_PROTECTED` | Blocks QVM from setting it (see CG_CVAR_SET intercept) |
| `CVAR_ARCHIVE_ND` | Archive but not in default config |

### Key Functions

| Function | What It Does |
|---|---|
| `Cvar_Get(name, default, flags)` | Create or find cvar; sets default if new |
| `Cvar_Set(name, value)` | Set string value; respects LATCH |
| `Cvar_SetValue(name, float)` | Set from float |
| `Cvar_SetIntegerValue(name, int)` | Set from integer |
| `Cvar_VariableIntegerValue(name)` | Read integer without cvar_t pointer |
| `Cvar_VariableStringBuffer(name, buf, size)` | Read string safely |
| `Cvar_SetDescription(var, text)` | Attach help text for `\help` command |
| `Cvar_CheckRange(var, min, max, type)` | Clamp to valid range |
| `Cvar_SetGroup(var, group)` | Assign to change-detection group |
| `Cvar_CheckGroup(group)` | Returns non-zero if any cvar in group was modified |
| `Cvar_ResetGroup(group, resetFlags)` | Clear modified flags for group |
| `Cvar_Register(vmCvar, name, default, flags, priv)` | Register from QVM |
| `Cvar_Update(vmCvar, priv)` | Sync QVM-side vmCvar_t from master |
| `Cvar_WriteVariables(f)` | Write all `CVAR_ARCHIVE` cvars to file |
| `Cvar_InfoString(flags, NULL)` | Build info string of matching cvars |

### Change Tracking

The global `cvar_modifiedFlags` int is OR'd with a cvar's flags whenever it's set. `SV_Frame` and `CL_Frame` check `CVAR_SERVERINFO` and `CVAR_SYSTEMINFO` bits to push updated configstrings.

---

## cmd.c — Command System

**File:** `code/qcommon/cmd.c`  
**Size:** ~1073 lines

### Command Buffer

All console input goes into a command buffer before execution:

```
Cbuf_AddText(text)     — append to end (deferred)
Cbuf_InsertText(text)  — insert at start (immediate-ish)
Cbuf_Execute()         — parse and execute all buffered commands
```

`Cbuf_Execute` splits text at `;` and `\n`, calls `Cmd_ExecuteString` on each token.

### Cmd_ExecuteString(text)

Execution priority order:
1. Check if it's a registered command (`cmd_function_t`)
2. Check if it's a cvar name → expand/set/print
3. Forward to server console (if dedicated) or client (remote)

### Key Functions

| Function | What It Does |
|---|---|
| `Cmd_AddCommand(name, func)` | Register a new console command |
| `Cmd_RemoveCommand(name)` | Unregister a command |
| `Cmd_ExecuteString(text)` | Execute a command string now |
| `Cbuf_AddText(text)` | Queue text for deferred execution |
| `Cbuf_Execute()` | Drain and execute command buffer |
| `Cmd_Argc()` / `Cmd_Argv(n)` | Access parsed argument tokens |
| `Cmd_TokenizeString(text)` | Parse text into argc/argv |

### Built-in Commands

| Command | Function |
|---|---|
| `exec` | Execute a config file |
| `vstr` | Execute the contents of a cvar as commands |
| `echo` | Print text to console |
| `wait` | Skip N frames before continuing buffer |
| `cmdlist` | List all registered commands |
| `cvarlist` | List all cvars |
| `set` / `seta` / `sets` / `setu` | Set cvars with different flags |
| `reset` | Reset cvar to default |
| `toggle` | Toggle boolean cvar |
| `inc` / `dec` / `math` | Arithmetic on cvars |

---

## files.c — Virtual Filesystem

**File:** `code/qcommon/files.c`  
**Size:** ~5836 lines — the largest qcommon file.

### Search Path Architecture

The VFS builds a search path list at startup. Files are found by scanning paths in order:

```
searchpath list (highest priority first):
  1. $HOME/.q3a/<gamedir>/          (user writable, always first)
  2. $basepath/<gamedir>/*.pk3      (sorted by name, newer first)
  3. $basepath/baseq3/*.pk3         (base game)
```

PK3 files are ZIP archives. Files inside them have checksums for pure-server verification.

### Key Data Structures

```c
typedef struct pack_s {
    char    pakPathname[MAX_OSPATH];
    char    pakFilename[MAX_OSPATH];
    char    pakBasename[MAX_OSPATH];
    int     checksum;
    int     numfiles;
    fileInPack_t *hashTable[PK3_HASH_SIZE];
    fileInPack_t *buildBuffer;
} pack_t;
```

### Key Functions

| Function | What It Does |
|---|---|
| `FS_InitFilesystem()` | Build search path list |
| `FS_FOpenFileRead(path, &f, unique)` | Open file for reading (searches all paths) |
| `FS_FOpenFileWrite(path)` | Open file for writing (always in homepath) |
| `FS_Read(buf, len, f)` | Read bytes from open file |
| `FS_Write(buf, len, f)` | Write bytes to open file |
| `FS_FCloseFile(f)` | Close file handle |
| `FS_ReadFile(path, &buf)` | Read entire file into heap buffer |
| `FS_FreeFile(buf)` | Free buffer from FS_ReadFile |
| `FS_WriteFile(path, buf, size)` | Write entire buffer to file |
| `FS_FileExists(path)` | Check existence without opening |
| `FS_GetFileList(path, ext, buf, size)` | List files matching pattern |
| `FS_BuildOSPath(base, game, qpath)` | Assemble OS path from parts |
| `FS_PureServerSetLoadedPaks(checksums, names)` | Server sends pure paks to client |
| `FS_ComparePaks(neededPaks, len, dlstring)` | Client checks which paks it needs |
| `FS_VM_OpenFile / ReadFile / CloseFile` | QVM-safe file access wrappers |

### Pure Server System

When `sv_pure 1`:
1. Server sends loaded pak checksums to client via `CS_SYSTEMINFO`
2. Client calls `FS_ComparePaks()` to find missing paks
3. Client must download missing paks before entering game
4. All file opens go through pak verification

---

## q_shared.h/c — Shared Math & Utilities

**File:** `code/qcommon/q_shared.h` (~1451 lines), `q_shared.c`

The single header included by absolutely everything. Defines:

### Basic Types

```c
typedef int         qboolean;    // qtrue=1, qfalse=0
typedef float       vec_t;
typedef vec_t       vec2_t[2];
typedef vec_t       vec3_t[3];
typedef vec_t       vec4_t[4];
typedef unsigned char byte;
typedef int         fileHandle_t;
typedef int         qhandle_t;
```

### Important Macros

| Macro | Effect |
|---|---|
| `VectorCopy(a, b)` | `b[0]=a[0]; b[1]=a[1]; b[2]=a[2]` |
| `VectorAdd(a, b, c)` | Component-wise add |
| `VectorSubtract(a, b, c)` | Component-wise subtract |
| `VectorScale(v, s, o)` | Multiply by scalar |
| `VectorMA(v, s, b, o)` | `o = v + s*b` |
| `DotProduct(a, b)` | Dot product → scalar |
| `CrossProduct(a, b, c)` | Cross product → vector |
| `VectorLength(v)` | Euclidean length |
| `VectorNormalize(v)` | In-place normalize, returns old length |
| `AngleVectors(a, f, r, u)` | Euler angles → forward/right/up axes |
| `Q_min/Q_max/Q_clamp(a,lo,hi)` | Min, max, clamp |
| `Com_Memset/Com_Memcpy` | Aliases for `memset/memcpy` |

### String Functions

| Function | Notes |
|---|---|
| `Q_strncpyz(dst, src, n)` | Always null-terminates, never overflows |
| `Q_strncmp/Q_stricmp` | Case-sensitive/insensitive compare |
| `Q_strupr / Q_strlwr` | In-place case conversion |
| `Q_strtok_r` | Thread-safe strtok |
| `Com_sprintf(dst, size, fmt, ...)` | Safe sprintf, warns on truncation |
| `va(fmt, ...)` | Returns static string (cycle of 4 buffers) |

### Info Strings

A compact key=value format used for server info, userinfo, system info:
`key\value\key2\value2\...`

| Function | Purpose |
|---|---|
| `Info_ValueForKey(s, key)` | Extract value for key |
| `Info_SetValueForKey(s, key, val)` | Set or update a key |
| `Info_RemoveKey(s, key)` | Delete a key |
| `Info_Validate(s)` | Check for invalid characters |

---

## q_math.c — Math Library

**File:** `code/qcommon/q_math.c`

### Trigonometry
Uses lookup tables (`sinTable[1024]`) for speed:
- `sin_lut(f)` / `cos_lut(f)` — table lookup
- `atan2(y, x)` — standard lib

### Key Functions

| Function | Notes |
|---|---|
| `VectorNormalize(v)` | Returns old magnitude; modifies in place |
| `VectorNormalize2(v, out)` | Non-destructive version |
| `VectorNormalizeFast(v)` | Approximate (Quake-style rsqrt trick) |
| `AnglesToAxis(angles, axis[3])` | Euler → 3×3 rotation matrix |
| `AxisToAngles(axis[3], angles)` | Rotation matrix → Euler |
| `vectoangles(dir, angles)` | Direction vector → yaw/pitch |
| `RotatePointAroundVector(dst, dir, point, degrees)` | Rotate point around axis |
| `PlaneFromPoints(plane, a, b, c)` | Compute plane from 3 points |
| `BoxOnPlaneSide(mins, maxs, plane)` | Classify AABB vs plane: 1=front, 2=back, 3=cross |
| `RadiusFromBounds(mins, maxs)` | Bounding sphere radius |
| `DirToByte(dir)` / `ByteToDir(b, dir)` | Quantize direction to 1 byte (256 directions) |
| `Q_rand(seed)` | Linear congruential PRNG |
| `ColorIndexFromChar(c)` | Map color code character to color index |

---

## huffman.c — Huffman Compression

**File:** `code/qcommon/huffman.c`

Used to compress the header bytes of UDP packets (the first ~16 bytes). Reduces bandwidth on frequently-repeated opcode patterns.

### Key Functions

| Function | Notes |
|---|---|
| `Huff_Init(huff_t*)` | Initialize an adaptive Huffman tree |
| `Huff_Compress(msg_t*, offset)` | Compress msg starting at `offset` bytes in |
| `Huff_Decompress(msg_t*, offset)` | Decompress from `offset` bytes |

**Adaptive:** The tree updates frequency counts dynamically as symbols are seen — no fixed codebook needed. Both encoder and decoder maintain identical trees.

---

## md4.c / md5.c — Hash Functions

**File:** `code/qcommon/md4.c`, `md5.c`

### Uses in the Engine

| Hash | Used For |
|---|---|
| MD4 | PAK file checksums; `Com_BlockChecksum(data, len)` |
| MD5 | Client GUID generation (`cl_guid`); `Com_MD5Addr(netadr, timestamp)` |

**Note:** These are not used for security — MD4/MD5 are broken cryptographically. They're used only for checksums and non-security-critical identification.

### Key Functions

| Function | Notes |
|---|---|
| `Com_BlockChecksum(buf, len)` | MD4 hash → 32-bit checksum |
| `Com_MD5Init()` | Initialize MD5 seeding |
| `Com_MD5Addr(netadr_t*, timestamp)` | Generate MD5-based client ID |

---

## Key Data Structures

### memzone_t (Zone allocator block)
```c
typedef struct memzone_s {
    int      size;           // total zone size
    int      used;           // bytes in use
    memblock_t *rover;       // allocation rover pointer
    memblock_t *blocklist;   // linked list head
} memzone_t;
```

### memblock_t (Individual allocation)
```c
typedef struct memblock_s {
    int              size;    // includes header
    memtag_t         tag;     // 0 = free
    struct memblock_s *next, *prev;
    int              id;      // sanity check
} memblock_t;
```

### cvar_t (Console variable)
```c
typedef struct cvar_s {
    char    *name;
    char    *string;
    char    *resetString;     // original default
    char    *latchedString;   // pending restart value
    int      flags;           // CVAR_* flags
    qboolean modified;        // changed since last check
    float    value;
    int      integer;
    struct cvar_s *next, *hashNext;
    char    *description;
    cvarGroup_t group;
} cvar_t;
```

### fileHandleData_t (Open file)
```c
typedef struct {
    FILE        *handleFiles;   // OS file handle (or NULL if pak)
    qboolean     handleSync;
    int          fileSize;
    int          zipFilePos;
    unzFile      zipFile;
    char         name[MAX_ZPATH];
    handleOwner_t owner;        // HO_NONE, HO_Q3, HO_CGAME, HO_UI
} fileHandleData_t;
```
