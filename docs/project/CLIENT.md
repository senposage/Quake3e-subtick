# Client Subsystem

> Covers all files in `code/client/`. Custom changes are marked **[CUSTOM]**.

---

## Table of Contents

1. [File Overview](#file-overview)
2. [cl_main.c — Client Lifecycle](#cl_mainc--client-lifecycle)
3. [cl_parse.c — Network Message Parser [CUSTOM]](#cl_parsec--network-message-parser-custom)
4. [cl_cgame.c — cgame QVM Interface [CUSTOM]](#cl_cgamec--cgame-qvm-interface-custom)
5. [cl_input.c — Input & Usercmd Generation [CUSTOM]](#cl_inputc--input--usercmd-generation-custom)
6. [cl_keys.c — Key Binding](#cl_keysc--key-binding)
7. [cl_console.c — In-Game Console](#cl_consolec--in-game-console)
8. [cl_scrn.c — Screen Layout](#cl_scrnc--screen-layout)
9. [cl_ui.c — UI QVM Interface](#cl_uic--ui-qvm-interface)
10. [cl_cin.c — Cinematic Video](#cl_cinc--cinematic-video)
11. [cl_net_chan.c — Client Netchan](#cl_net_chanc--client-netchan)
12. [cl_avi.c — AVI Capture](#cl_avic--avi-capture)
13. [cl_curl.c — HTTP Downloads](#cl_curlc--http-downloads)
14. [Key Data Structures](#key-data-structures)
15. [Client State Machine](#client-state-machine)
16. [Complete Call Graph — Client Tick](#complete-call-graph--client-tick)

---

## File Overview

| File | Lines | Custom? | Purpose |
|------|-------|---------|---------|
| `cl_main.c` | 5463 | no | Connection, demo recording, init/shutdown |
| `cl_parse.c` | 1300 | **YES** | Parse server messages, snapshot EMA |
| `cl_cgame.c` | 1050 | **YES** | cgame QVM syscall handler, time sync |
| `cl_input.c` | 820 | **YES** | Key→usercmd, download packet throttle |
| `cl_keys.c` | 640 | no | Key binding management |
| `cl_console.c` | 580 | no | In-game console |
| `cl_scrn.c` | 560 | no | Screen layout, loading screens |
| `cl_ui.c` | 550 | no | UI QVM interface |
| `cl_cin.c` | 1744 | no | RoQ video decoder |
| `cl_net_chan.c` | 200 | no | Client netchan (+ demo filter) |
| `cl_avi.c` | 580 | no | AVI video capture |
| `cl_curl.c` | 800 | no | HTTP download (libcurl) |
| `cl_jpeg.c` | 160 | no | JPEG screenshot writer |
| `client.h` | 700 | **YES** | clientActive_t with snapshotMsec |

---

## cl_main.c — Client Lifecycle

**File:** `code/client/cl_main.c`  
**Size:** ~5463 lines — largest source file in the project

### CL_Init()

Called once from `Com_Init`. Registers all client cvars, loads renderer, initializes sound, starts UI QVM.

```
CL_Init:
  CL_SharedInit()           ← common input/key init
  CL_InitRef()              ← load renderer DLL (OpenGL or Vulkan)
  S_Init()                  ← sound system
  CL_StartHunkUsers()       ← load cgame + renderer maps + UI
  CL_RegisterCvars()        ← register all client cvars
  Cmd_AddCommand()          ← connect, disconnect, demo, record, etc.
```

### CL_Frame(msec)

Called from `Com_Frame` every frame:

```
CL_Frame(msec):
  if (cls.state == CA_DISCONNECTED): return
  cls.realtime += msec    ← monotonic wall clock

  CL_CheckTimeout()       ← disconnect on timeout
  CL_CheckPaused()
  Com_EventLoop()         ← drain remaining events

  CL_SendCmd()            ← build usercmd + send packet if ready
  CL_SetCGameTime()       ← compute cl.serverTime from serverTimeDelta
  CL_CGameRendering()     ← call cgame DrawActiveFrame
  SCR_UpdateScreen()      ← renderer frame
  S_Update(msec)          ← sound mixing
  CL_CheckForResend()     ← retransmit connection requests
  CL_Download_f()         ← file download progress
```

### CL_Connect_f()

`connect <address>` handler:
1. Resolve address via `NET_StringToAdr`
2. Disconnect from any current server
3. Send OOB `getchallenge` packet
4. Transition to `CA_CONNECTING`

### CL_CheckForResend()

Retransmit logic during connection:
- `CA_CONNECTING` → resend `getchallenge` if `RETRANSMIT_TIMEOUT` (3s) elapsed
- `CA_CHALLENGING` → resend `connect` with challenge
- `CA_PRIMED` → resend pure checksum request

### Demo Recording

`CL_Record_f()` — start recording to a `.dm_68` file:
- `CL_WriteGamestate(true)` — write full gamestate first
- Subsequent packets stored by `CL_WriteDemoMessage` in `cl_net_chan.c`

`CL_StopRecord_f()` — flush and close demo file.

`CL_PlayDemo_f()` — load and play a demo:
- Opens `.dm_68` (or `.dm_67`, `.dm_84`, `.dm_91` for older protocols)
- Reads messages as if they were incoming network packets
- Demo time driven by `cls.realtime`

`CL_ReadDemoMessage()` — read next demo message, simulating real-time delivery.

### CL_Disconnect(showMainMenu)

```
CL_Disconnect:
  if CA_ACTIVE or CA_PRIMED: SV_GameCommand("disconnect") + send reliable disconnect
  CL_ShutdownCGame()      ← free cgame QVM
  CL_ShutdownUI() (if showMainMenu): ← reload UI to main menu
  CL_ClearState()         ← zero clientActive_t
  cls.state = CA_DISCONNECTED
```

### Renderer Initialization

`CL_InitRef()` — dynamically loads either:
- `renderer_opengl1_x86_64.so` (OpenGL)
- `renderer_vulkan_x86_64.so` (Vulkan)

Selected by `r_renderAPI` cvar (0=OpenGL, 1=Vulkan). The renderer is loaded as a shared library that returns a `refexport_t` function table.

### CL_Vid_Restart(keepWindow)

Hot-restarts the renderer:
1. `CL_ShutdownRef(REF_KEEP_WINDOW or REF_DESTROY_WINDOW)`
2. `CL_InitRef()` — reload renderer
3. `CL_StartHunkUsers()` — reload all renderer assets

---

## cl_parse.c — Network Message Parser [CUSTOM]

**File:** `code/client/cl_parse.c`  
**Size:** ~1300 lines

### CL_ParseServerMessage(msg_t *msg)

Top-level handler for every server→client message. Dispatches on message type byte:

| Type | Handler |
|---|---|
| `svc_nop` | skip |
| `svc_serverCommand` | `CL_ParseCommandString` — queue reliable command |
| `svc_gamestate` | `CL_ParseGamestate` — new map/configstrings |
| `svc_snapshot` | `CL_ParseSnapshot` — game state update |
| `svc_download` | `CL_ParseDownload` — file download chunk |

### CL_ParseSnapshot(msg, multiview) [CUSTOM]

The most important parser function. After reading the snapshot:

```
measure snapshot interval:
    measured = newSnap.serverTime - cl.snap.serverTime
    maxMeasured = cl.snapshotMsec * 4 (or 200 if uninitialized)
    if (measured >= 1 AND measured <= maxMeasured):
        if cl.snapshotMsec == 0:
            cl.snapshotMsec = measured                     ← first value
        else:
            cl.snapshotMsec = (snapshotMsec*3 + measured) >> 2  ← EMA α=0.25
        clamp to [8, 100]
    if still 0: cl.snapshotMsec = 50  ← fallback until first valid measurement
```

**Why EMA with α=0.25:** Exponential moving average gives more weight to recent snapshots while smoothing over occasional dropped packets (which would show as 2× interval). The `4×snapshotMsec` outlier rejection prevents a single 3-dropped-packet event from pushing the EMA to 4× and mistuning all the time-sync thresholds.

**Clamping to [8, 100]ms:**
- Min 8ms = max 125Hz (fastest server anyone would run)
- Max 100ms = min 10Hz (below this the time-sync falls apart anyway)

### CL_ParseGamestate(msg) [CUSTOM]

Reads the full game state on connect. Custom addition:
```c
// Extract sv_snapshotFps from CS_SERVERINFO for early snapshotMsec seeding.
// This gives accurate snapshotMsec from the first frame before EMA converges.
int snapFps = atoi(Info_ValueForKey(serverInfo, "sv_snapshotFps"));
// sv_snapshotFps == -1 means "use sv_fps"; fall back to sv_fps in that case.
if (snapFps > 0 && snapFps <= 125):
    cl.snapshotMsec = 1000 / snapFps  ← initialized before first snapshot arrives
```

**Why this matters:** `CL_AdjustTimeDelta`'s thresholds depend on `cl.snapshotMsec`. Without this seed, the first 4-5 snapshots use `snapshotMsec = 50` (20Hz default), causing the 60Hz thresholds to be 3× too conservative. With the seed, every threshold is correctly tuned from frame 1.

### CL_ParseDownload(msg)

Handles file download chunks:
- Reads chunk number and data
- Writes to `clc.download` file handle
- Sends `nextdl` acknowledgment
- On EOF: close file, proceed with pak loading

### CL_ParseCommandString(msg)

Queues a reliable server command for ordered execution before the next snapshot becomes active (`newSnap.serverCommandNum`).

---

## cl_cgame.c — cgame QVM Interface [CUSTOM]

**File:** `code/client/cl_cgame.c`  
**Size:** ~1050 lines

### CL_InitCGame()

Loads `cgame.qvm` via `VM_Create(VM_CGAME, ...)`:
- Builds cgame QVM
- Calls `VM_Call(cgvm, CG_INIT, serverMessageNum, serverCommandSequence, clientNum)`
- `CL_ConfigstringModified()` — send all configstrings to cgame

### CL_ShutdownCGame()

Calls `VM_Call(cgvm, CG_SHUTDOWN)`, then `VM_Free(cgvm)`.

### CL_CGameRendering(stereoFrame)

Called from `CL_Frame`:
```
VM_Call(cgvm, 4, CG_DRAW_ACTIVE_FRAME, cl.serverTime, stereoFrame, clc.demoplaying)
```

### CL_SetCGameTime() [CUSTOM]

Computes `cl.serverTime` for the cgame. The most change-heavy function in the client:

```
CL_SetCGameTime:
    if !cl.snap.valid: handle extrapolation
    
    [CUSTOM: proportional clamp]
    // Clamp serverTime so it can never get more than snapshotMsec ahead
    // of the latest snapshot. Vanilla hardcoded 5ms which worked at 20Hz
    // but caused overshooting at 60Hz.
    if (cl.serverTime > cl.snap.serverTime + cl.snapshotMsec):
        cl.serverTime = cl.snap.serverTime + cl.snapshotMsec

    [CUSTOM: proportional extrapolate threshold]
    // Vanilla: if serverTime >= snap.serverTime - 5ms: set extrapolatedSnapshot
    // Custom: threshold is snapshotMsec/3 instead of hardcoded 5ms
    extrapolateThresh = cl.snapshotMsec / 3
    if (cls.realtime + cl.serverTimeDelta - cl.snap.serverTime >= -extrapolateThresh):
        cl.extrapolatedSnapshot = qtrue

    if (cl.newSnapshots):
        CL_AdjustTimeDelta()        ← drift correction
```

### CL_AdjustTimeDelta() [CUSTOM]

Adjusts `cl.serverTimeDelta` to track server clock:

```
resetTime  = max(cl.snapshotMsec * 10, 500)   // ← was hardcoded 500ms
fastAdjust = cl.snapshotMsec * 2               // ← was hardcoded 100ms

deltaDelta = abs(newDelta - cl.serverTimeDelta)

if deltaDelta > resetTime:   hard reset to newDelta
elif deltaDelta > fastAdjust: cl.serverTimeDelta = (old + new) / 2
else (slow drift):
    if extrapolatedSnapshot:
        cl.serverTimeDelta -= (snapshotMsec < 30) ? 1 : 2  ← ← CUSTOM: was always -2
    else:
        cl.serverTimeDelta++
```

**The `-1 vs -2` fix at 60Hz:** Vanilla always subtracts 2ms when extrapolating. At 20Hz (50ms windows) this creates a +1/-2 equilibrium that drifts toward slightly-late, correcting quickly. At 60Hz (16ms windows) the same equilibrium fires 3× as often, creating a visible sawtooth oscillation in the netgraph. Using -1ms at 60Hz creates a +1/-1 balance that settles to zero drift.

### CG_CVAR_SET Intercept [CUSTOM]

Inside `CL_CgameSystemCalls`:
```c
case CG_CVAR_SET:
    // Forward to Cvar_SetSafe (respects CVAR_PROTECTED)
    // This prevents the cgame QVM from overriding engine cvars like
    // snaps (snapshot rate) and cg_smoothClients
    Cvar_SetSafe(VMA(1), VMA(2));
    return 0;
```

**What this blocks:** The UT4.3 cgame.qvm sets `snaps` to `sv_fps/2` on connect. If the server runs at 60Hz, this would force 30Hz snapshots regardless of `sv_snapshotFps`. Marking `snaps` as `CVAR_PROTECTED` (done in `sv_init.c`) ensures the QVM's set is silently ignored.

### CL_CgameSystemCalls Key Cases

| Syscall | Handler |
|---|---|
| `CG_PRINT` | `Com_Printf` |
| `CG_ERROR` | `Com_Error(ERR_DROP)` |
| `CG_MILLISECONDS` | `Sys_Milliseconds()` |
| `CG_CVAR_SET` | `Cvar_SetSafe` **[CUSTOM: was Cvar_Set]** |
| `CG_CM_LOADMAP` | `CL_CM_LoadMap()` |
| `CG_CM_BOXTRACE` | `CM_BoxTrace()` |
| `CG_S_STARTSOUND` | `S_StartSound()` |
| `CG_R_CLEARSCENE` | `re.ClearScene()` |
| `CG_R_ADDREFENTITYTOSCENE` | `re.AddRefEntityToScene()` |
| `CG_R_RENDERSCENE` | `re.RenderScene()` |
| `CG_GETSNAPSHOT` | Copy `cl.snapshots[n]` to QVM |
| `CG_GETGAMESTATE` | Copy `cl.gameState` to QVM |
| `CG_GETCURRENTSNAPSHOTNUMBER` | Return `cl.snap.messageNum` |
| `CG_GETCURRENTCMDNUMBER` | Return `cl.cmdNumber` |
| `CG_GETUSERCMD` | Copy `cl.cmds[n]` to QVM |
| `CG_SETUSERCMDVALUE` | Set `cl.cgameUserCmdValue` (weapon) |
| `CG_MEMORY_REMAINING` | `Hunk_MemoryRemaining()` |

---

## cl_input.c — Input & Usercmd Generation [CUSTOM]

**File:** `code/client/cl_input.c`  
**Size:** ~820 lines

### Input → kbutton_t → usercmd_t Pipeline

1. **Hardware events** (key, mouse) → `Com_EventLoop` → `CL_KeyEvent` / `CL_MouseEvent`
2. **kbutton_t** — each action key has a state struct:
   ```c
   typedef struct {
       int         down[2];     // key codes holding it down
       unsigned    downtime;
       unsigned    msec;        // accumulated held time
       qboolean    active;
       qboolean    wasPressed;
   } kbutton_t;
   ```
3. **IN_KeyDown / IN_KeyUp** — update kbutton_t state + timestamp
4. **CL_CmdButtons()** — sample all kbutton_t states → write to `cmd->buttons`, `cmd->weapon`
5. **CL_MouseMove()** — accumulate `cl.mouseDx/Dy` → `cmd->angles`
6. **CL_FinishMove(cmd)** — finalize: forwardmove, rightmove, upmove from kbutton states

### CL_CreateNewCommands()

Called from `CL_SendCmd()` every frame. Builds a new `usercmd_t` in `cl.cmds[cl.cmdNumber]`:
```
CL_CreateNewCommands:
  ++cl.cmdNumber
  cmd = &cl.cmds[cl.cmdNumber & CMD_MASK]
  CL_InitCmd(cmd)
  IN_CmdButtons(cmd)     ← buttons, weapon
  CL_MouseMove(cmd)      ← view angles from mouse delta
  IN_Move(cmd)           ← keyboard movement
  CL_KeyMove(cmd)        ← analog stick / directional keys
  cmd->serverTime = ...  ← cl.serverTime or prediction
  CL_FinishMove(cmd)     ← compute move magnitudes
```

### CL_SendCmd()

Decides whether to send a packet this frame, then sends:
```
CL_SendCmd:
  CL_CreateNewCommands()     ← always build the command
  if !CL_ReadyToSendPacket(): return   ← rate limiting
  CL_WritePacket()            ← serialize and send
    MSG_WriteDeltaUsercmdKey() × last 4 cmds
    Netchan_Transmit()
```

### CL_ReadyToSendPacket() [CUSTOM]

Controls packet send rate:
```c
// [CUSTOM] Download throttle uses measured snapshotMsec instead of hardcoded 50ms
int throttleMs = cl.snapshotMsec > 0 ? cl.snapshotMsec : 50;
if (*clc.downloadTempName && cls.realtime - clc.lastPacketSentTime < throttleMs)
    return qfalse;
```

**Why this matters:** During download, the client sends ACK packets. Vanilla throttled at 50ms (matching 20Hz). At 60Hz snapshots, 50ms throttle = client receives 3 snapshots before sending one ACK → download stalls. Using `snapshotMsec` ensures ACKs are sent every snapshot interval regardless of rate.

### Kbutton Actions (registered as console commands)

| Action | Command | Kbutton |
|---|---|---|
| Move forward | `+forward` | `in_forward` |
| Move back | `+back` | `in_back` |
| Move left (strafe) | `+moveleft` | `in_moveleft` |
| Move right (strafe) | `+moveright` | `in_moveright` |
| Jump/swim up | `+moveup` | `in_up` |
| Crouch/swim down | `+movedown` | `in_down` |
| Turn left | `+left` | `in_left` |
| Turn right | `+right` | `in_right` |
| Look up | `+lookup` | `in_lookup` |
| Look down | `+lookdown` | `in_lookdown` |
| Run modifier | `+speed` | `in_speed` |
| Strafe modifier | `+strafe` | `in_strafe` |
| Mouse look | `+mlook` | `in_mlooking` |
| Attack | `+attack` (button0) | `in_buttons[0]` |
| Use item | `+button1` | `in_buttons[1]` |
| Walk | `+button2` | `in_buttons[2]` |
| Gesture | `+button3` | `in_buttons[3]` |

---

## cl_keys.c — Key Binding

**File:** `code/client/cl_keys.c`  
**Size:** ~640 lines

### Key States

`keys[MAX_KEYS]` array — one entry per keycode:
```c
typedef struct {
    qboolean    down;
    int         repeats;  // if > 1, key is being held
    char        *binding; // bound command string
} qkey_t;
```

### CL_KeyEvent(key, down, time)

Called from event loop for every key press/release:
1. Record key state in `keys[key]`
2. If down and has binding: `Cbuf_AddText(binding)` or `Cbuf_AddText(binding + "\n")`
3. Route to console / chat / or game (`VM_Call(cgvm, CG_KEY_EVENT, ...)`)

### CL_CharEvent(key)

Handles typed characters (after OS key mapping):
- If console open: `Con_CharEvent`
- If chat open: `Field_CharEvent`
- Otherwise: `VM_Call(cgvm, CG_KEY_EVENT, key, qtrue)`

### Key Names

`Key_KeynumToString(keynum)` / `Key_StringToKeynum(s)`:
- Maps between keycode integers and human-readable names ("F1", "CTRL", "MOUSE1", etc.)

---

## cl_console.c — In-Game Console

**File:** `code/client/cl_console.c`  
**Size:** ~580 lines

### Console State

```c
typedef struct {
    vec4_t  color;
    int     times[CON_TEXTSIZE]; // scroll history
    int     current;             // current line
    int     display;             // view position
    float   finalFrac;           // scroll target (0=closed, 1=fully open)
    float   currentFrac;         // animated position
} console_t;
```

### Key Functions

| Function | Notes |
|---|---|
| `Con_Init()` | Register `toggleconsole` command |
| `Con_Toggle_f()` | Open/close console |
| `Con_DrawConsole()` | Render console overlay (called from SCR_DrawConsole) |
| `Con_PrintAll()` | Print entire history to stdout |
| `Con_ClearNotify()` | Clear notification area (used on disconnect) |

### Console Input

`g_consoleField` is the input field at the bottom. History browsing uses `historyLine` ring buffer. Tab completion calls `Field_AutoComplete()` → `Cmd_CommandCompletion` + `Cvar_CommandCompletion`.

---

## cl_scrn.c — Screen Layout

**File:** `code/client/cl_scrn.c`  
**Size:** ~560 lines

Manages the overall screen layout for non-cgame frames:

| Function | Notes |
|---|---|
| `SCR_DrawScreenField()` | Top-level per-frame screen renderer |
| `SCR_DrawLoading()` | Loading screen (filename + progress) |
| `SCR_DrawDemoRecording()` | Demo recording indicator |
| `SCR_DrawFPS()` | FPS counter (if `cg_drawFPS`) |
| `SCR_DrawConsole()` | Draw console overlay |
| `SCR_UpdateScreen()` | Called from `CL_Frame`: `re.BeginFrame` → draw → `re.EndFrame` |
| `SCR_Init()` | Register screen cvars |
| `SCR_AdjustFrom640(x, y, w, h)` | Scale from 640×480 reference to screen coordinates |

---

## cl_ui.c — UI QVM Interface

**File:** `code/client/cl_ui.c`  
**Size:** ~550 lines

### CL_InitUI()

Loads `ui.qvm` via `VM_Create(VM_UI, ...)` and calls `VM_Call(uivm, UI_INIT, ...)`.

### CL_UISystemCalls

Key syscalls:

| Syscall | Handler |
|---|---|
| `UI_CVAR_SET` | `Cvar_Set` |
| `UI_DRAW_CHAR` | `re.DrawStretchPic` (character rendering) |
| `UI_DRAW_STRETCHPIC` | `re.DrawStretchPic` |
| `UI_UPDATESCREEN` | `SCR_UpdateScreen()` |
| `UI_CM_LOADMAP` | `CM_LoadMap()` |
| `UI_LAN_GETSERVERCOUNT` | LAN server list |
| `UI_LAN_GETSERVERSTATUS` | Server status query |
| `UI_FS_GETFILELIST` | `FS_GetFileList()` |
| `UI_GETSTORAGE` | `FS_ReadFile("ui/storage.bin")` |
| `UI_SETSTORAGE` | `FS_WriteFile("ui/storage.bin")` |

---

## cl_cin.c — Cinematic Video

**File:** `code/client/cl_cin.c`  
**Size:** ~1744 lines

RoQ video decoder for Quake 3's `*.roq` cinematic files.

### Key Functions

| Function | Notes |
|---|---|
| `CIN_PlayCinematic(file, x, y, w, h, flags)` | Open and start playing an ROQ file |
| `CIN_RunCinematic(handle)` | Advance decoder by one frame |
| `CIN_DrawCinematic(handle)` | Upload current frame to renderer |
| `CIN_StopCinematic(handle)` | Close and free |
| `CIN_UploadCinematic(handle)` | `re.UploadCinematic()` — upload raw RGBA to GPU |
| `CL_PlayCinematic_f()` | Console command handler |

RoQ is a lossy video codec (DCT blocks + VQ codebook). The decoder implements:
- RoQ_INFO → set dimensions
- RoQ_QUAD_CODEBOOK → load VQ codebook
- RoQ_QUAD_VQ → decode video frame using codebook
- RoQ_SOUND_MONO/STEREO → DPCM audio decode

---

## cl_net_chan.c — Client Netchan

**File:** `code/client/cl_net_chan.c`  
**Size:** ~200 lines

Thin wrapper over `Netchan_Process`/`Netchan_Transmit` that adds demo recording:

`CL_Netchan_Process(chan, msg)`:
- If recording a demo and `clc.demowaiting` is false: `CL_WriteDemoMessage(msg, 0)`
- Then calls `Netchan_Process`

`CL_Netchan_Transmit(chan, msg)`:
- Simply calls `Netchan_Transmit` (no demo recording for outgoing packets)

---

## cl_avi.c — AVI Capture

**File:** `code/client/cl_avi.c`  
**Size:** ~580 lines

Uncompressed AVI video/audio recording:

| Function | Notes |
|---|---|
| `CL_OpenAVI(filename)` | Open AVI file, write headers |
| `CL_TakeVideoFrame()` | Capture current frame → `re.TakeVideoFrame()` |
| `CL_WriteAVIVideoFrame(data, size)` | Append video chunk |
| `CL_WriteAVIAudioFrame(data, size)` | Append audio chunk |
| `CL_CloseAVI()` | Finalize index, close |

Uses `cl_avidemo` cvar to control FPS. Configured via `video` command.

---

## cl_curl.c — HTTP Downloads

**File:** `code/client/cl_curl.c` (only compiled with `USE_CURL`)  
**Size:** ~800 lines

Wraps libcurl for downloading missing PK3 files from servers that advertise `sv_dlURL`:

| Function | Notes |
|---|---|
| `CL_cURL_Init()` | `curl_global_init()` |
| `CL_cURL_Shutdown()` | `curl_global_cleanup()` |
| `CL_ParseDownloadURL(url)` | Parse download URL from CS_SERVERINFO |
| `CL_cURL_PerformDownload()` | Start async download |
| `CL_cURL_Run()` | Poll in-progress downloads |
| `CL_cURL_Done(handle)` | Complete download, check hash |

Files are downloaded to the homepath temp directory, then moved to the final PK3 location on success.

---

## Key Data Structures

### clientActive_t cl — Active Connection State

```c
typedef struct {
    int         timeoutcount;
    clSnapshot_t snap;              // latest received snapshot

    int         serverTime;         // interpolated time for cgame
    int         oldServerTime;
    int         serverTimeDelta;    // cl.serverTime = cls.realtime + serverTimeDelta
    qboolean    extrapolatedSnapshot;
    qboolean    newSnapshots;

    int         snapshotMsec;       // [CUSTOM] EMA of snapshot interval

    gameState_t gameState;          // configstrings from server
    char        mapname[MAX_QPATH];

    usercmd_t   cmds[CMD_BACKUP];   // outgoing command ring buffer
    int         cmdNumber;          // current command index
    vec3_t      viewangles;

    clSnapshot_t snapshots[PACKET_BACKUP]; // received snapshot ring buffer
    entityState_t entityBaselines[MAX_GENTITIES];
    entityState_t parseEntities[MAX_PARSE_ENTITIES];
} clientActive_t;
```

### clientConnection_t clc — Connection Parameters

```c
typedef struct {
    int         clientNum;
    netadr_t    serverAddress;
    int         connectTime;
    int         challenge;
    netchan_t   netchan;

    // demo recording
    fileHandle_t recordfile;
    qboolean     demoplaying;
    qboolean     demowaiting;
    fileHandle_t demofile;

    // downloads
    char        downloadTempName[MAX_OSPATH];
    char        downloadName[MAX_QPATH];
    fileHandle_t download;
    int         downloadSize;
    int         downloadCount;
    int         lastPacketSentTime;

    // reliable commands
    int         reliableSequence;
    int         reliableAcknowledge;
    char        reliableCommands[MAX_RELIABLE_COMMANDS][MAX_STRING_CHARS];
    int         serverCommandSequence;
    char        serverCommands[MAX_RELIABLE_COMMANDS][MAX_STRING_CHARS];
} clientConnection_t;
```

### clientStatic_t cls — Persistent Client State

```c
typedef struct {
    connstate_t state;    // CA_UNINITIALIZED through CA_ACTIVE
    int         realtime; // monotonic wall clock (ms)
    int         frametime;
    qboolean    rendererStarted;
    qboolean    soundStarted;
    qboolean    uiStarted;
    // ... various flags
} clientStatic_t;
```

---

## Client State Machine

```
CA_UNINITIALIZED (0)
    ↓ CL_Init()
CA_DISCONNECTED
    ↓ CL_Connect_f() → getchallenge OOB
CA_CONNECTING
    ↓ challengeResponse → connect OOB
CA_CHALLENGING
    ↓ connectResponse + gamestate send starts
CA_CONNECTED
    ↓ pure checksum exchange, pak downloads complete
CA_PRIMED
    ↓ first non-SNAPFLAG_NOT_ACTIVE snapshot
CA_ACTIVE ← normal gameplay
    ↓ CL_Disconnect() or timeout
CA_DISCONNECTED

CA_LOADING (during map load)
CA_CINEMATIC (full-screen cinematic playing)
```

---

## Complete Call Graph — Client Tick

```
Com_Frame()
└── CL_Frame(msec)
    ├── cls.realtime += msec
    ├── CL_CheckTimeout()
    ├── Com_EventLoop()            ← drain keyboard, mouse, network events
    │     ├── CL_KeyEvent()
    │     ├── CL_MouseEvent()
    │     └── [SE_PACKET] → CL_PacketEvent()
    │           └── CL_Netchan_Process() → CL_ParseServerMessage()
    │                 ├── CL_ParseSnapshot()  ← EMA update [CUSTOM]
    │                 ├── CL_ParseGamestate() ← snapshotMsec seed [CUSTOM]
    │                 └── CL_ParseCommandString()
    │
    ├── CL_SendCmd()
    │     ├── CL_CreateNewCommands()      ← usercmd from input state
    │     ├── CL_ReadyToSendPacket()      ← snapshotMsec throttle [CUSTOM]
    │     └── CL_WritePacket()
    │           └── MSG_WriteDeltaUsercmdKey() × 4 cmds
    │               Netchan_Transmit()
    │
    ├── CL_SetCGameTime()                 ← compute cl.serverTime [CUSTOM]
    │     ├── (clamp to snap + snapshotMsec) ← [CUSTOM]
    │     ├── (extrapolateThresh = snapshotMsec/3) ← [CUSTOM]
    │     └── CL_AdjustTimeDelta()        ← drift correction [CUSTOM]
    │
    ├── CL_CGameRendering()
    │     └── VM_Call(cgvm, CG_DRAW_ACTIVE_FRAME, cl.serverTime, ...)
    │           └── cgame QVM → re.* syscalls → renderer
    │
    ├── SCR_UpdateScreen()
    │     ├── re.BeginFrame()
    │     ├── SCR_DrawScreenField()       ← UI or loading screen
    │     └── re.EndFrame()
    │
    └── S_Update(msec)                    ← sound mixing
```
