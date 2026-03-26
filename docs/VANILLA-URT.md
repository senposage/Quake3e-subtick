# Vanilla-urt Branch

Base: upstream ec-/quake3e (clean Quake3e, no URT modifications)

Only the following URT-specific features are grafted on top, all gated
behind build preprocessor flags so a flag-off build produces a
stock Quake3e binary:

## Features ported from senposage/Quake3e-urt master

### USE_FTWGL (default: on)
- Binary / window name: FTWGL (CNAME = ftwgl-20211117 in Makefile)
- Q3_VERSION string: "FTWGL"
- Window titles: "FTWGL: Urban Terror" / "FTWGL: UrT"
- BASEGAME: "q3ut4"  (baseq3 when off)
- Master / update / authorize servers point to urbanterror.info
- MAX_RELIABLE_COMMANDS: 128  (64 when off -- upstream default)
- fs_game defaults to BASEGAME; FS_CheckIdPaks skipped (no Q3A pak
  checksums required)
- Client screenshot server command (sv_ccmds.c) and intercept
  (cl_cgame.c)
- Least-used client slot assignment for stable client IDs (sv_main.c)

### USE_AUTH (default: on, requires USE_FTWGL)
- Urban Terror player auth server: authserver.urbanterror.info
- Server-side: sv_authServerIP / sv_auth_engine cvars, AUTH:SV packet
  handler, SV_Auth_DropClient
- Client-side: cl_auth / cl_authc / cl_authl / cl_auth_engine cvars,
  AUTH:CL packet handler
- QVM interface: UI_AUTHSERVER_PACKET, GAME_AUTHSERVER entries

### USE_OPENAL (default: on)
- Full OpenAL Soft backend (snd_openal.c) from master:
  Kaiser-windowed sinc resampler, EFX reverb/occlusion, per-category
  volume mixer, async thread pool, HRTF support

### sv_smoothClients (always on)
- Server-side ring-buffer position smoother for TR_LINEAR entities
- Provides SV_SmoothInit / SV_SmoothClearClient / SV_SmoothRecordAll /
  SV_SmoothGetPosition in sv_snapshot.c
- Controlled by sv_smoothClients cvar (default 1)
