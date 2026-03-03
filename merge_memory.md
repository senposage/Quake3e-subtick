# Merge Memory - Upstream Sync Progress

## GROUND RULES (from user)
1. **Trust the CODE, not the documentation**
2. **Do NOT trust /patches/ files** - they are OUT OF DATE
3. **ALL merges go into `Quake3e-upstreammerge` branch, NEVER master**
4. Break work into separate PRs to avoid hitting max agent time
5. Write everything to this merge_memory.md file

## Project Overview
- **Repo**: senposage/Quake3e-subtick (fork of ec-/Quake3e)
- **Purpose**: Quake3e engine with subtick networking, antilag, antiwarp modifications
- **Upstream**: ec-/Quake3e at tag `latest` (commit 46add7d088adf831d91bf3153f2be45b477560ea)
- **Build system**: GNU Make (Makefile), also has CMakeLists.txt and MSVC solutions
- **Master branch sha**: d22f38b (tag: latest)
- **Quake3e-upstreammerge branch sha**: b05d00c (merge commit of PR #38)

## Branch Topology
```
master (d22f38b) ── the original fork code, all fork features working
  └── Quake3e-upstreammerge (b05d00c) ── PR #38 merged here, has upstream sync + 93 corrupted files
```

## Previous Merge Work (PR #38 - MERGED into Quake3e-upstreammerge)
- Title: "Sync with upstream ec-/Quake3e latest tag (46add7d)"
- Replaced ~390 files with upstream content
- Re-applied fork patches (auth, multiview, ftwgl, subtick, antiwarp, antilag)
- 443 files changed, 57276 additions, 63289 deletions
- **Result: 93 files got corrupted with "404: Not Found" content (14 bytes each)**

## Build Configuration (from Makefile on master)
- USE_SDL=0, USE_VULKAN=1, USE_OPENGL=0, USE_VULKAN_API=1
- USE_AUTH=1, USE_URT_DEMO=1, NO_DMAHD=0, USE_FTWGL=1
- USE_SERVER_DEMO=1, USE_RENDERER_DLOPEN=0
- Output names: ftwgl-{gitrev}.x64 (client), quake3e.ded-{gitrev}.x64 (server)
- **Master branch builds successfully on Linux x86_64**

---

## CONFIRMED: 93 Corrupted Files on Quake3e-upstreammerge

All contain literally `404: Not Found` (14 bytes). This happened because PR #38 
tried to download these files from upstream ec-/Quake3e via raw.githubusercontent.com,
but **THESE FILES DO NOT EXIST IN UPSTREAM** - they are fork-specific or from a 
different path structure.

### ROOT CAUSE
The files are **fork-specific** or from **fork-only directories** that don't exist 
in upstream ec-/Quake3e. The upstream repo does NOT have:
- `code/client/snd_dmahd.*` (fork's DMAHD sound system)
- `code/qcommon/cm_load_bsp1.c`, `cm_load_bsp2.*` (fork's BSP loading, upstream has `cm_load.c`)
- `code/renderervk/tr_font.c` (upstream has it in `code/renderer/` not `code/renderervk/`)
- `code/libsdl/windows/include/SDL2/` (entire directory doesn't exist upstream)
- `code/win32/msvc2019/` (entire directory doesn't exist upstream)
- `code/win32/rounded.png`, `code/win32/win_resource.aps` (fork-only binary assets)

### FIX: Restore from master branch
All 93 files have CORRECT versions on the `master` branch (d22f38b). The fix is to 
copy these files from master back to `Quake3e-upstreammerge`.

### Complete List of Corrupted Files (93 total)

#### Fork-Specific Engine Source (5 files) - CRITICAL
```
code/client/snd_dmahd.c          (1467 lines on master)
code/client/snd_dmahd.h          (17 lines on master)
code/qcommon/cm_load_bsp1.c      (988 lines on master)
code/qcommon/cm_load_bsp2.c      (625 lines on master)
code/qcommon/cm_load_bsp2.h      (234 lines on master)
```

#### Fork-Specific Renderer (1 file) - CRITICAL
```
code/renderervk/tr_font.c        (562 lines on master)
```

#### SDL2 Windows Headers (74 files) - Windows build only
```
code/libsdl/windows/include/SDL2/SDL.h
code/libsdl/windows/include/SDL2/SDL_assert.h
code/libsdl/windows/include/SDL2/SDL_atomic.h
code/libsdl/windows/include/SDL2/SDL_audio.h
code/libsdl/windows/include/SDL2/SDL_bits.h
code/libsdl/windows/include/SDL2/SDL_blendmode.h
code/libsdl/windows/include/SDL2/SDL_clipboard.h
code/libsdl/windows/include/SDL2/SDL_config.h
code/libsdl/windows/include/SDL2/SDL_cpuinfo.h
code/libsdl/windows/include/SDL2/SDL_egl.h
code/libsdl/windows/include/SDL2/SDL_endian.h
code/libsdl/windows/include/SDL2/SDL_error.h
code/libsdl/windows/include/SDL2/SDL_events.h
code/libsdl/windows/include/SDL2/SDL_filesystem.h
code/libsdl/windows/include/SDL2/SDL_gamecontroller.h
code/libsdl/windows/include/SDL2/SDL_gesture.h
code/libsdl/windows/include/SDL2/SDL_haptic.h
code/libsdl/windows/include/SDL2/SDL_hints.h
code/libsdl/windows/include/SDL2/SDL_joystick.h
code/libsdl/windows/include/SDL2/SDL_keyboard.h
code/libsdl/windows/include/SDL2/SDL_keycode.h
code/libsdl/windows/include/SDL2/SDL_loadso.h
code/libsdl/windows/include/SDL2/SDL_log.h
code/libsdl/windows/include/SDL2/SDL_main.h
code/libsdl/windows/include/SDL2/SDL_messagebox.h
code/libsdl/windows/include/SDL2/SDL_mouse.h
code/libsdl/windows/include/SDL2/SDL_mutex.h
code/libsdl/windows/include/SDL2/SDL_name.h
code/libsdl/windows/include/SDL2/SDL_opengl.h
code/libsdl/windows/include/SDL2/SDL_opengl_glext.h
code/libsdl/windows/include/SDL2/SDL_opengles.h
code/libsdl/windows/include/SDL2/SDL_opengles2.h
code/libsdl/windows/include/SDL2/SDL_opengles2_gl2.h
code/libsdl/windows/include/SDL2/SDL_opengles2_gl2ext.h
code/libsdl/windows/include/SDL2/SDL_opengles2_gl2platform.h
code/libsdl/windows/include/SDL2/SDL_opengles2_khrplatform.h
code/libsdl/windows/include/SDL2/SDL_pixels.h
code/libsdl/windows/include/SDL2/SDL_platform.h
code/libsdl/windows/include/SDL2/SDL_power.h
code/libsdl/windows/include/SDL2/SDL_quit.h
code/libsdl/windows/include/SDL2/SDL_rect.h
code/libsdl/windows/include/SDL2/SDL_render.h
code/libsdl/windows/include/SDL2/SDL_revision.h
code/libsdl/windows/include/SDL2/SDL_rwops.h
code/libsdl/windows/include/SDL2/SDL_scancode.h
code/libsdl/windows/include/SDL2/SDL_sensor.h
code/libsdl/windows/include/SDL2/SDL_shape.h
code/libsdl/windows/include/SDL2/SDL_stdinc.h
code/libsdl/windows/include/SDL2/SDL_surface.h
code/libsdl/windows/include/SDL2/SDL_system.h
code/libsdl/windows/include/SDL2/SDL_syswm.h
code/libsdl/windows/include/SDL2/SDL_test.h
code/libsdl/windows/include/SDL2/SDL_test_assert.h
code/libsdl/windows/include/SDL2/SDL_test_common.h
code/libsdl/windows/include/SDL2/SDL_test_compare.h
code/libsdl/windows/include/SDL2/SDL_test_crc32.h
code/libsdl/windows/include/SDL2/SDL_test_font.h
code/libsdl/windows/include/SDL2/SDL_test_fuzzer.h
code/libsdl/windows/include/SDL2/SDL_test_harness.h
code/libsdl/windows/include/SDL2/SDL_test_images.h
code/libsdl/windows/include/SDL2/SDL_test_log.h
code/libsdl/windows/include/SDL2/SDL_test_md5.h
code/libsdl/windows/include/SDL2/SDL_test_memory.h
code/libsdl/windows/include/SDL2/SDL_test_random.h
code/libsdl/windows/include/SDL2/SDL_thread.h
code/libsdl/windows/include/SDL2/SDL_timer.h
code/libsdl/windows/include/SDL2/SDL_touch.h
code/libsdl/windows/include/SDL2/SDL_types.h
code/libsdl/windows/include/SDL2/SDL_version.h
code/libsdl/windows/include/SDL2/SDL_video.h
code/libsdl/windows/include/SDL2/SDL_vulkan.h
code/libsdl/windows/include/SDL2/begin_code.h
code/libsdl/windows/include/SDL2/close_code.h
```

#### MSVC 2019 Project Files (12 files) - Windows build only
```
code/win32/msvc2019/.editorconfig
code/win32/msvc2019/.editorconfig.inferred
code/win32/msvc2019/Header.h
code/win32/msvc2019/botlib.vcxproj
code/win32/msvc2019/botlib.vcxproj.filters
code/win32/msvc2019/libjpeg.vcxproj
code/win32/msvc2019/quake3e-ded.vcxproj
code/win32/msvc2019/quake3e.sln
code/win32/msvc2019/quake3e.vcxproj
code/win32/msvc2019/quake3e.vcxproj.filters
code/win32/msvc2019/renderer.vcxproj
code/win32/msvc2019/renderer2.vcxproj
code/win32/msvc2019/renderervk.vcxproj
```

#### Binary/Other (2 files + 1 to delete)
```
code/win32/rounded.png            (icon, 9413 bytes on master)
code/win32/win_resource.aps       (resource file, 190428 bytes on master)
code/renderervk/shaders/spirv/.shader_data.c.swp  (VIM SWAP FILE - DELETE THIS)
```

---

## NEXT PR INSTRUCTIONS

### PR should:
- Branch FROM `Quake3e-upstreammerge` 
- Target `Quake3e-upstreammerge` as base
- Title: "Fix 93 corrupted files from upstream sync"

### Exact Fix Commands (run on a branch from Quake3e-upstreammerge):
```bash
# Step 0: Make sure you have master available
git fetch origin master --depth=1

# Step 1: Restore the 6 critical engine/renderer source files
git show origin/master:code/client/snd_dmahd.c > code/client/snd_dmahd.c
git show origin/master:code/client/snd_dmahd.h > code/client/snd_dmahd.h
git show origin/master:code/qcommon/cm_load_bsp1.c > code/qcommon/cm_load_bsp1.c
git show origin/master:code/qcommon/cm_load_bsp2.c > code/qcommon/cm_load_bsp2.c
git show origin/master:code/qcommon/cm_load_bsp2.h > code/qcommon/cm_load_bsp2.h
git show origin/master:code/renderervk/tr_font.c > code/renderervk/tr_font.c

# Step 2: Restore 74 SDL2 Windows headers
for f in $(git ls-tree --name-only origin/master code/libsdl/windows/include/SDL2/); do
  git show "origin/master:$f" > "$f"
done

# Step 3: Restore 12 MSVC 2019 project files
for f in $(git ls-tree --name-only origin/master code/win32/msvc2019/); do
  git show "origin/master:$f" > "$f"
done

# Step 4: Restore 2 binary files
git show origin/master:code/win32/rounded.png > code/win32/rounded.png
git show origin/master:code/win32/win_resource.aps > code/win32/win_resource.aps

# Step 5: Delete vim swap file (should NOT be in repo)
git rm code/renderervk/shaders/spirv/.shader_data.c.swp

# Step 6: Verify - find any remaining corrupted files
git ls-tree -r HEAD --name-only | while read f; do
  size=$(git cat-file -s "HEAD:$f" 2>/dev/null)
  if [ "$size" = "14" ]; then
    content=$(git show "HEAD:$f" 2>/dev/null)
    if [ "$content" = "404: Not Found" ]; then
      echo "STILL CORRUPTED: $f"
    fi
  fi
done

# Step 7: Build test (Linux)
make clean && make
```

### Suggested PR breakdown (to stay within agent time limits):
- **PR A**: Fix ALL 93 corrupted files in one shot (preferred if agent has time)
- OR split into:
  - **PR A**: Fix 6 critical engine source files (snd_dmahd, cm_load_bsp, tr_font)
  - **PR B**: Fix 74 SDL2 Windows headers  
  - **PR C**: Fix 12 MSVC 2019 project files + 2 binary files + delete .swp

### How to verify the fix worked:
```bash
# Check no more 404 files remain
find . -name "*.c" -o -name "*.h" | xargs grep -l "^404: Not Found$" 2>/dev/null
# Should return empty

# Build test
make clean && make 2>&1 | tail -5
# Should end with LD build/release-linux-x86_64/ftwgl-XXXXX.x64
```

---

## UPSTREAM REPO REFERENCE

### ec-/Quake3e structure (at tag latest / 46add7d)
- Does NOT have: `code/client/snd_dmahd.*`
- Does NOT have: `code/qcommon/cm_load_bsp1.c`, `cm_load_bsp2.*`
- Does NOT have: `code/renderervk/tr_font.c`
- Does NOT have: `code/libsdl/windows/` directory at all
- Does NOT have: `code/win32/msvc2019/` directory at all
- Does NOT have: `code/win32/rounded.png`
- Has `code/qcommon/cm_load.c` (single file, fork split it into bsp1/bsp2)
- Has `code/renderer/tr_font.c` (not in renderervk)
- Has MSVC2017 project files in `code/win32/msvc2017/`

### Fork-specific features (verified from code, NOT from /patches/ which are outdated)
The fork adds these features on top of upstream:
- Subtick networking (server frame loop, cvars, pmove, extrapolation, map restart)
- Shadow antilag system (`code/server/sv_antilag.h`, `sv_antilag.c`)
- Antiwarp system
- DMAHD sound (`code/client/snd_dmahd.c/.h`)
- BSP2 map format support (`cm_load_bsp1.c`, `cm_load_bsp2.c/.h`)
- FTWGL client name
- Auth system (USE_AUTH)
- URT demo support (USE_URT_DEMO)
- Server-side demo recording (USE_SERVER_DEMO)
- Custom MSVC 2019 project files
- Bundled SDL2 Windows headers
- Custom Vulkan renderer tr_font.c

---

## SESSION HISTORY (PR #39 - this session)

### What was done:
1. Explored repo structure, understood it's a fork of ec-/Quake3e
2. Built the project successfully on master branch (Linux x86_64)
3. Fetched `Quake3e-upstreammerge` branch (b05d00c)
4. Compared current branch (master) vs Quake3e-upstreammerge: 444 files different
5. Found 2 suspicious binary files (14 bytes = corrupted)
6. Ran full scan: found ALL 93 corrupted files containing "404: Not Found"
7. Investigated upstream ec-/Quake3e: confirmed these files DON'T EXIST upstream
8. Confirmed all 93 files have correct content on master branch
9. Documented everything in this file

### What was NOT done (for next PR):
- Did not modify any code on Quake3e-upstreammerge
- Did not fix any corrupted files (user said "don't do it locally, I'll open a new PR")
- Did not verify fork patches are correctly applied (beyond confirming build works)
- Did not do Windows build testing

### Key learnings:
- The sandbox blocks raw GitHub downloads (DNS monitoring proxy)
- Must use `git show origin/master:<path>` to get file content, not curl
- GitHub MCP `get_file_contents` works for upstream repo structure browsing
- The /patches/ directory is OUT OF DATE and should NOT be trusted

---

## AFTER FIXING CORRUPTED FILES - FUTURE WORK

Once the 93 corrupted files are fixed, these tasks remain:
1. **Verify Linux build** on Quake3e-upstreammerge branch
2. **Verify Windows build** (needs MSVC or MinGW cross-compile)
3. **Diff Quake3e-upstreammerge vs master** to confirm only intentional upstream changes remain
4. **Test fork features** still work (subtick, antilag, antiwarp, DMAHD, BSP2)
5. **Eventually merge Quake3e-upstreammerge into master** when everything is verified

---

## Key Directories
- `code/server/` - Server-side code (most fork changes are here)
- `code/client/` - Client-side code
- `code/qcommon/` - Common/shared code
- `code/game/` - Game logic (QVM)
- `code/cgame/` - Client game logic
- `code/renderervk/` - Vulkan renderer
- `code/renderer/` - OpenGL renderer
- `code/renderer2/` - OpenGL2 renderer
- `code/libjpeg/` - Bundled libjpeg
- `code/unix/` - Unix/Linux platform code
- `code/win32/` - Windows platform code
