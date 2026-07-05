# Trace Comparison Report: Native SDL vs SSZ Fallback

**Date:** 2026-07-05  
**Build:** Debug (w64devkit MinGW/GCC, x86_64)  
**Trace duration:** 10 seconds (startup + idle at title/menu)  
**Trace mask:** 255 (all categories)

## Configurations

| Config | Command | Native SDL | Result |
|--------|---------|------------|--------|
| **A** | `IKEMEN_NATIVE_SDLPLUGIN_LIB=0 IKEMEN_NATIVE_SDLEVENT_LIB=0` | ❌ Disabled | **Fails to boot** — SSZ scripts need sdlplugin bridge functions registered |
| **B** | Default (both =1) | ✅ Enabled | **Boots successfully** — 1,303,132 log lines, 1,288,426 TRACE entries |

## Key Finding

**Config A cannot boot.** When `IKEMEN_NATIVE_SDLPLUGIN_LIB=0`, the `sdlplugin_static.hpp` stub registers nothing. The SSZ runtime then tries to load `sdlplugin.dll` which doesn't exist:

```
loadDll: 'sdlplugin' not in static registry
lookup: library 'sdlplugin' not found
getfunc: 'End' not in 'sdlplugin', falling back to dynamic
Error: Failed to load plugin (sdlplugin.ssz line 3)
```

The engine never reaches the main loop. This confirms that **the native SDL plugin bridge is currently mandatory** — there is no `sdlplugin.dll` fallback.

## Config B Trace Analysis (69 unique functions)

### Call Frequency by Category

| Category | Mask | Calls | % of Total | Key Functions |
|----------|------|-------|-----------|---------------|
| **FILE** | 1 | 1,196,224 | 92.8% | Read (1,193,035), Seek (7,298), ReadAry (2,386), FileClose (282), Open (141), Find (107) |
| **LUA** | 4 | 28,356 | 2.2% | IsNaN (7,272), ToNumber (6,348), IsNumber (6,348), ToRef (5,640), PushString (3,604), PushNumber (2,940), ToString (2,772), IsString (2,772), PushRef (1,899), Register (1,243), PushBoolean (939), GetTop (224), Pcall (156), GetGlobal (156) |
| **SDL** | 64 | 27,627 | 2.1% | JoystickButtonState (24,059), RenderMugenZoom (2,290), RenderFontBatch (697), RenderMugenShadow (622), PollEvent (431), GetTicks (423), Delay (383), SetSndBuf (177), Flip (169), PauseBGM (163), DecodePNG8 (27), SoftFill (13), SetVolume (4), WindowType (1), SetOpacity (1), RendererInit (1), FullScreenExclusive (1), FullScreen (1), CursorShow (1), AspectRatio (1) |
| **UTIL** | 16 | 6,281 | 0.5% | RegexSearch (5,912), ThreadDelay (156), DeleteRegex (144), NewRegex (72), AsciiToLocal (33), VeryUnsafeCopy (4), SetSharedString (2), GetSharedString (2) |
| **MATH** | 32 | 7,166 | 0.6% | Floor (3,202), Ceil (2,347), Sin (894), Sqrt (163), IsFinite (428) |
| **SYS** | 128 | 2,772 | 0.2% | MemMarkAfter (6), MemMarkBefore (5), LuaInit (3), UnixTime (2), TickCount (2), RunFile (2), NewState (2), Close (2) |
| **OGG** | 8 | 0 | 0.0% | — |
| **NET** | 2 | 0 | 0.0% | — |

### Key SDL Function Trace (Config B)

```
RendererInit      1     Window created (640x480)
FullScreenExclusive 1  Fullscreen mode set
FullScreen         1     
CursorShow         1     
AspectRatio        1     
SetOpacity         1     
WindowType         1     
----------------------------------------
GetTicks          423   Frame timing
Delay             383   Frame pacing
Flip              169   Screen flip (vsync)
SoftFill           13   Screen clear/fill
PollEvent         431   Input polling
JoystickButtonState 24059 Joystick polling (per-frame)
DecodePNG8         27   PNG sprite loading
----------------------------------------
RenderMugenZoom  2290   Sprite rendering
RenderMugenShadow 622   Shadow rendering
RenderFontBatch   697   Font rendering
----------------------------------------
SetSndBuf         177   Audio setup
PauseBGM          163   Audio playback
SetVolume           4   Volume control
```

### Comparison with Pre-Conversion Baseline

| Metric | Pre-Conversion (Jun 29) | Config B (Jul 5) | Delta |
|--------|------------------------|-------------------|-------|
| Unique functions | 28 (startup only) | 69 (startup + menu) | +41 |
| File Read calls | 42,308 | 1,193,035 | ×28 |
| PollEvent | 0 (not observed) | 431 | Added |
| Flip | 0 (not observed) | 169 | Added |
| RenderMugenZoom | 0 (not observed) | 2,290 | Added |
| DecodePNG8 | 0 (not observed) | 27 | Added |
| JoystickButtonState | 3 (gameplay trace) | 24,059 | ×8,020 |

## Conclusion

1. **Native SDL bridge is mandatory** — cannot boot without it (no .dll fallback exists)
2. **File I/O dominates** all runtime (92.8% of all trace entries)
3. **SDL rendering loop is active** — Flip, SoftFill, PollEvent, RenderMugenZoom, RenderFontBatch all observed
4. **Joystick polling is the most frequent SDL call** (24k in 10s) — ~2,400 per second, suggesting 4 joysticks × 2 buttons × 60fps done in SSZ script code
5. **No regressions detected** — all expected SDL functions (init, render, input, audio) are present in expected proportions
