# Issues — Screenshot System (`TakeScreenShot`)

**File:** `main/sdlplugin/sdlplugin.cpp:2042–2107`
**Reviewed:** 2026-08-25
**Status:** All issues fixed — see each section.

---

## Architecture

The screenshot system has two paths:

| Path | Condition | Method |
|---|---|---|
| Software renderer | `isOpenGL == 0` | `SDL_RenderReadPixels` → `IMG_SavePNG` |
| OpenGL renderer | `isOpenGL != 0` | `glReadPixels` → manual PNG write via libpng |

A third path exists inside the VLC video player (`PlayVideo`, sdlplugin.cpp) that uses `libvlc_video_take_snapshot` for screenshots during video playback.

---

## Issues

### ✅ [HIGH] No NULL check on `SDL_CreateRGBSurface` — crash on OOM — **FIXED**

**Location:** Software path, all 3 branches (old lines 2064, 2072, 2081)

`SDL_CreateRGBSurface` can return `NULL` on allocation failure (confirmed in bundled SDL2, SDL_surface.c — OOM and bad-format paths). Old code dereferenced `screenshot->pixels` unchecked → crash.

**Fix applied:** NULL check + early return with log. The three software branches were also collapsed into one (they all read the same `g_w × g_h` viewport), so only a single call site remains.

---

### ✅ [MEDIUM] Maximized window produced black-band / corrupted screenshot — **FIXED**

**Original claim (corrected):** old code created a screen-sized surface (`GetSystemMetrics`) while `SDL_RenderReadPixels` reads exactly the renderer viewport (`g_w × g_h`). The original write-up said "out-of-bounds read / crash" — **mechanism was wrong**: SDL clamps the read rect against the viewport before reading (bundled SDL2, SDL_render.c:4245–4252), so no OOB read or crash occurred. The oversized surface was zero-filled, so the real symptom was a screenshot with a black band at bottom/right whenever maximized/fullscreen window size differed from desktop size.

**Fix applied:** Surface is now always `g_w × g_h`, matching what `SDL_RenderReadPixels` actually reads. The `winMaximized` global and the maximized/fullscreen special cases were removed entirely (verified: `winMaximized` had no other users). Correct image dimensions in every window state; also removes the Windows-only `GetSystemMetrics` dependency from this function.

---

### ✅ [MEDIUM→NOTE] `GetSystemMetrics` is Windows-only — **MOOT**

Technically true but irrelevant: the whole plugin already hard-depends on Win32 (`#include <windows.h>` line 2, unconditional `WideCharToMultiByte` in `WstrToStr`, `wrapProc(HWND...)`). A Linux build of this file was never possible regardless. As a side effect of the fix above, `TakeScreenShot` itself no longer calls `GetSystemMetrics`; the remaining uses are the separate screen-size helpers at lines ~1995/~2000 used by display setup.

---

### ✅ [LOW] Parameter named `dir` is actually a full file path — **FIXED**

Renamed to `path`. Local `std::string path` in the GL branch renamed to `pathStr` to avoid shadowing.

---

### ✅ [LOW] No success/failure logging in main `TakeScreenShot` — **FIXED**

Added `LOG_INFO` (matches VLC-path house style, which also uses `LOG_INFO` for errors):
- Software path: success/failure of `IMG_SavePNG` (checks its `int` return), plus failure of surface creation.
- OpenGL path: success after libpng write, failure when `fopen` fails.

Note: `new[]` in the GL path throws `std::bad_alloc` rather than returning NULL, so it needed no check.

---

## Summary

| Severity | Issue | Status |
|---|---|---|
| 🔴 High | No NULL check on `SDL_CreateRGBSurface` | Fixed — single guarded call site |
| 🟡 Medium | Screen-sized surface vs `g_w × g_h` viewport → black bands (not OOB/crash) | Fixed — always `g_w × g_h`, branches collapsed |
| 🟡 Medium | `GetSystemMetrics` Windows-only | Moot — gone from this function as side effect |
| 🟢 Low | `dir` param misnamed | Fixed → `path` |
| 🟢 Low | No logging in main `TakeScreenShot` | Fixed — software + GL paths |
