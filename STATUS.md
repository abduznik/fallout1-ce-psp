# Fallout 1 CE — PSP Port Status

> **Date:** 13 July 2026
> **Repository:** `github.com/abduznik/fallout1-ce-psp`
> **HEAD:** `9ce9d84`
> **Branch:** `main`

---

## Concept

Port [alexbatalov/fallout1-ce](https://github.com/alexbatalov/fallout1-ce) — a
from-scratch C++ reimplementation of Fallout 1 — to Sony PSP using the
[pspdev](https://github.com/pspdev/pspdev) toolchain. The PSP port follows the
same pattern established by existing community forks (Vita, 3DS, Xbox):
add an `if(PSP)` block to `CMakeLists.txt` plus new platform files under
`os/psp/`, without touching core game logic.

---

## Pipeline

```
┌─────────────┐     buf_to_buf      ┌──────────────────┐
│ Game Engine │──────────────────→  │   gSdlSurface    │
│ (GNW95_*)   │   (copy pixel rect) │  INDEX8 640×480  │
└──────┬──────┘                     └────────┬─────────┘
       │                                     │
       │  psp_convert_index8_to_rgb565()     │
       │  (manual palette lookup per pixel)  │
       ▼                                     ▼
┌──────────────────────┐             ┌──────────────────┐
│  gSdlTextureSurface  │ ←──────────┘  palette updated  │
│  RGB565 640×480      │   via GNW95_SetPalette*()      │
│  (intermediate)      │                                 │
└──────────┬───────────┘                                 │
           │                                              │
           │  SDL_BlitScaled (RGB565→RGB565, no palette)  │
           ▼                                              │
┌──────────────────────┐                                  │
│  gSdlWindowSurface   │                                  │
│  RGB565 480×272      │                                  │
│  (PSP framebuffer)   │                                  │
└──────────┬───────────┘                                  │
           │                                               │
           │  SDL_UpdateWindowSurface                      │
           ▼                                               │
      PSP Display  ◄───────────────────────────────────────┘
```

---

## What Was Done

### Step 1 — Setup & Toolchain
- Forked `alexbatalov/fallout1-ce` to `abduznik/fallout1-ce-psp`
- Cross-compilation via pspdev on homelab Ubuntu 24.04 (SSH: `arb`)
- SDL2 confirmed packaged in pspdev, `find_package(SDL2)` works
- Reference: `Northfear/fallout1-ce-vita` for platform branch pattern

### Step 2 — CMakeLists.txt
- `if(PSP)` block with MIPS/Allegrex compile flags
- `find_package(SDL2)` (Vita pattern, not `add_subdirectory`)
- Link modules: `pspdebug`, `psputility`, `pspctrl`, `pspaudiolib`
- `include(os/psp/psp.cmake)` for `create_pbp_file()` (EBOOT.PBP packaging)

### Step 3 — Render Pipeline (4 major fixes)

| Fix | Commit | Problem |
|:----|:-------|:--------|
| **Surface-based renderer** | `4fb1476` | PSP SDL2 `SDL_CreateTexture` caps textures at 512×512 (hardware constraint of PSP Media Engine). Our 640×480 exceeds this → `exit(1)`. Fix: use `SDL_GetWindowSurface` + `SDL_UpdateWindowSurface` instead. |
| **Two-step downscale** | `07d05b8`→`5c45ed0` | Raw 640×480 INDEX8 doesn't fit 480×272 PSP display. Need INDEX8→RGB565 conversion + scale. Approach: `gSdlTextureSurface` (intermediate RGB565 640×480) for palette conv, then `SDL_BlitScaled` → window surface. |
| **Manual INDEX8→RGB565** | `65b72c5` ★ | `SDL_BlitSurface(gSdlSurface → gSdlTextureSurface)` returns 0 but writes NOTHING on PSP SDL2 when destination is a `SDL_CreateRGBSurfaceWithFormat` surface. Fix: replace with manual pixel-loop conversion. |
| **Auto-dismiss dialogs** | `58aaad6` | PSP system dialogs block the main thread. Auto-dismiss via `sceUtilityMsgDialogAbort` so the game can reach its main loop. |

### Step 4 — Input (NOT YET WIRED)
PSP has only one analog stick. Control layout is an open design decision.
See [open decisions](#open-design-decisions) below.

### Step 5 — Build Loop
Clean cross-compile confirmed. EBOOT.PBP deploys to `ms0:/PSP/GAME/FOUT00002/`
(6.5MB). Game boots to intros on PPSSPP v1.20.3.

---

## Root Cause Summary

### Bug: Grayscale + Torn Full-Frame Images

**Evidence from diagnostic logging** (commit `3b57974`):

```
GNW95_SetPaletteEntries start=1 count=254
  gSdlSurface:  fmt=0x13000801 (INDEX8)  pitch=640  pal=0x909a5b0
  gSdlTextureSurface:  fmt=0x15551002 (RGB565)  pitch=1280
  colors[0]: r=4 g=0 b=0  colors[127]: r=140 g=136 b=100
  fullBlit ret=0  err=""
  postBlit px[0]=0x0000  px[320]=0x0000  px[639]=0x0000  ← ALL BLACK
```

Three independent data points proved the bug:

1. **`SDL_BlitSurface` returned 0 (success)** — no error from SDL
2. **Palette was correctly set** — `colors[127] = (140, 136, 100)` is a real palette value
3. **Intermediate surface pixels = 0x0000** — the blit wrote nothing

The text overlay rendered correctly because it uses `SDL_BlitSurface` with a
**non-NULL source rect**, which triggers a different blitter path in PSP SDL2
that apparently works. The full-frame palette-update path uses `NULL` rects
(= full surface), which triggers the broken path.

### Previous Bug: Black Screen at Startup

`SDL_CreateTexture(640×480)` exceeds PSP's 512×512 hardware texture limit.
The game called `exit(1)` in `svga_init()` before any render code ran.
Fix: surface-based rendering entirely bypasses the texture pipeline.

---

## Verified Working

- [x] pspdev toolchain cross-compile
- [x] `svga_init()` passes (surface-based renderer)
- [x] Main loop runs 43+ minutes with no crash
- [x] Display scaling 640×480 → 480×272 via two-step blit chain
- [x] Intros render (Interplay logo, "Brian Fargo presents")
- [x] Color logos confirmed via PPSSPP screenshot
- [x] Palette manipulation functions call manual convert code

## Not Yet Verified

- [ ] Full-frame FMV/background image colors after fix
- [ ] Main menu rendering
- [ ] Character creation screen
- [ ] In-game world rendering

## Known Not Working

- [ ] Input — completely unwired (Step 4 not started)
- [ ] Audio — SDL2/Audiolib linked but untested
- [ ] Save/load — depends on input progress

---

## Open Design Decisions

### Control Layout (Step 4 — Pending Decision)

PSP has only **one analog stick** (vs Vita's two + touch + back touch).
This creates a budget conflict for map-scrolling, cursor movement, and menu
shortcuts.

| Feature | Vita | PSP budget |
|:--------|:-----|:-----------|
| Cursor movement | Left stick ✅ | Same stick |
| Map scrolling | Right stick ✅ | **Competes with cursor** |
| Confirm/Interact | Cross | Cross |
| Cancel/Back | Circle | Circle |
| Skilldex | Triangle | Triangle |
| Inventory | Square | Square |
| Character screen | D-Pad Up | D-Pad Up |
| Pip-Boy | D-Pad Down | D-Pad Down |
| Combat start | D-Pad Right | D-Pad Right |
| Combat end | D-Pad Left | D-Pad Left |

**Proposal A — Hold Mode:** Hold L-trigger to switch left stick from cursor
to map-scroll mode. D-Pad unchanged. Intuitive but requires L-trigger
coordination.

**Proposal B — Tap Mode:** Tap left stick (click L3-equiv via Homebrew)
to toggle cursor↔scroll. Fewer simultaneous keys but accidental toggles.

**Proposal C — Hybrid:** L-trigger held = scroll. R-trigger held = shortcut
menu (face buttons become quick-slot actions). D-Pad always controls
character/combat actions.

**Deferred.** Awaiting visual UI confirmation before implementing.

---

## Commits (chronological)

```
65b72c5 ★ psp: manual INDEX8→RGB565 conversion
3b57974   psp: log ShowRect unconditionally + postBlit pixel dump
8d32788   psp: add error logging to renderPresent
5c45ed0   psp: two-step rendering (intermediate surface)
b034454   psp: change chdir to FOUT00002
07d05b8   psp: add SDL_BlitScaled in renderPresent
4fb1476   psp: switch to surface-based rendering
10d943a   psp: isolate createRenderer logging
c18066c   psp: per-step logging in createRenderer
30644e6   psp: per-step error logging in svga_init
84e822b   psp: fix include order
58aaad6   psp: auto-dismiss system dialogs
d039051   psp: remove pspdebug library link
39466f9   psp: fix debug — no pspDebugScreenInit
d3d22c3   psp: link pspdebug library
f2ba9fa   psp: add missing pspiofilemgr.h include
0e6a453   psp: sceIoWrite + pspDebugScreenPrintf
```

---

## File Map

| File | Purpose |
|:-----|:--------|
| `CMakeLists.txt` | `if(PSP)` block: MIPS flags, SDL2 find_package, link modules, include psp.cmake |
| `os/psp/psp.cmake` | EBOOT/PBP packaging via `create_pbp_file()` |
| `os/psp/icon.png` | PSP EBOOT icon (270 bytes) |
| `src/plib/gnw/svga.cc` | ★ All PSP render changes: surface-based path, manual INDEX8→RGB565, two-step scaling |
| `src/plib/gnw/winmain.cc` | chdir path, auto-dismiss thread, debug init |
| `src/platform_compat.cc` | PSP branch for system compat stubs |
| `PSP_PORT_ARCHITECTURE.html` | Visual architecture diagram (this file) |
| `STATUS.md` | This document |

---

## Deployment

- **PPSSPP path:** `F:\emulators\ppsspp\memstick\PSP\GAME\FOUT00002\`
- **Data files:** `MASTER.DAT` (339MB) + `CRITTER.DAT` (157MB) + `DATA/` dir
- **Debug output:** `ms0:/psp_debug.txt`, `ms0:/create_renderer_debug.txt`
- **Build host:** `arb@homelab` (Ubuntu 24.04, pspdev at `/opt/pspdev/`)
