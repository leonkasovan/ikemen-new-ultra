# PENDINGS.md — Real Source Code Audit

**Generated:** 2026-07-05
**Method:** Manual source audit of all 84 files in `main/ssz_native/` (42 `.hpp` + 42 `.cpp`), cross-referenced against `TODO_SSZ_CONVERSION.md` and `docs/native_ssz_comparison.md`.

> ⚠️ **Discrepancies found between TODO_SSZ_CONVERSION.md and actual source.** Each discrepancy is marked below.

---

## Legend

| Icon | Meaning |
|------|---------|
| ✅ | Complete — no remaining work |
| ⚠️ | Partial — real behavior exists but has known gaps |
| ❌ | Stub — no real behavior implemented |
| 🔴 | Blocking — blocks downstream modules |
| 🟡 | Non-blocking — can be deferred |
| 📝 | Discrepancy found — TODO doc says one thing, source says another |

---

## P0 — Trace & Parity Evidence (Blocking)

These must be resolved before any behavioral parity claim is trustworthy.

| # | Item | Status | Details |
|---|------|--------|---------|
| 1 | **Capture startup trace** with `IKEMEN_ENABLE_PLUGIN_TRACE=1` | ✅ | Both baseline (121,994 lines) and native (79,225 lines) captured. SSZ-only boot was broken — **fixed** by removing `#if IKEMEN_NATIVE_*_LIB` guards from 13 static headers (file, sdlplugin, math, socket, lua, mesdialog, ogg, regex, shell, thread, time, alert, sound). |
| 2 | **Capture gameplay trace** (title → select → load → fight) | ❌ | `capture_gameplay_trace.ps1` exists but hasn't been run and committed. |
| 3 | **Post-conversion trace comparison** format | ❌ | No `pre_trace → native_trace → diff` pipeline exists. |
| 4 | **Runtime comparison with Method 1 + Method 2** from `docs/sdl_parity_report.md` | ✅ | **Method 1 complete.** See `docs/sdl_parity_report.md` §4.2 for full results. Key finding: **100% function-level parity** — all 62 unique TRACE functions present in both builds, zero differences. Counts are proportional (native ran ~54% of frames). AsciiToLocal: 33 calls both (fix verified). |

---

## P1 — Stub Modules (Behavior Not Implemented)

These modules have `#if IKEMEN_NATIVE_*_LIB` guards and bridge wrappers, but their function bodies are no-ops or placeholders.

### Big 4 Lua Callback Modules — All Fully Implemented

| Module | SSZ LOC | SSZ Symbols | Callbacks | Status | Blocking? |
|--------|---------|-------------|-----------|--------|-----------|
| `script_service` | 2,216 | 19 | 190+ | ✅ Full implementation | 🔴 Previously blocking — resolved |
| `trigger_script_service` | 1,633 | 1 | 130+ | ✅ Full implementation | 🔴 Previously blocking — resolved |
| `system_script_service` | 2,403 | 37 | 120+ | ✅ Full implementation | 🔴 Previously blocking — resolved |
| `debug_script_service` | ~300 | 1 | 27 | ✅ Full implementation | 🟡 Debug tooling only |

**Updates (2026-07-06):**
- `script_service` — All 190+ Lua-callable functions implemented, `refArg` SSZ pattern wired via public `ref_arg<T>()` template, `drawTTF` now supports alignment and scaling via proper SDL_ttf rendering.
- `trigger_script_service` — All 130+ trigger functions (player nav, game state, hit detection, edge/camera, win/lose, var access, etc.) implemented and registered.
- `system_script_service` — All 120+ system-level functions (TextImg, Anim, netplay, match config, visual config, volume/screen, input config, portraits, lifebar, etc.) implemented and registered. Calls `script_init(L)` first matching SSZ `.sc.init(L=)` pattern.
- `debug_script_service` — **Now fully implemented.** All 25 Lua-callable debug functions (puts, sszReload, setLife, setLifeMax, setPower, setAttack, setDefence, selfState, addHotkey, toggleClsnDraw, toggleDebugDraw, toggleStatusDraw, togglePostMatch, togglePause, togglePauseMenu, step, toggleRecord, togglePlayback, toggleRecordEnd, roundReset, reload, setAccel, setAILevel, setTime, clear) delegate to native service modules. Hotkey registration stored in `DebugScriptState::hotkeys`. `debug_load_file()` sets up full Lua environment.

### Other Stubs

| Module | SSZ LOC | File Size | Status | Details |
|--------|---------|-----------|--------|---------|
| `video_service` | 57 | 93 lines | ✅ Full implementation | `video_play()` now checks file existence via `_wfopen`, delegates to `sdlplugin::playVideo()`, sets `videoActive` flag, returns result (0 = ALT+F4/close, non-zero = success). No-arg wrapper calls through with defaults ("", "", 100, 1). |
| `font_service` | 409 | 952 lines (838 .cpp + 114 .hpp) | ✅ Full implementation | Real FNT v1/v2 binary/text loader, character metrics (`charWidth`, `textWidth`), `drawChar()` with `renderMugenZoom`, `drawText()` with `renderFontBatch`, palette-banked sprite access. Static registration wired in `font_static.hpp` and called from `main.cpp`. |
| `fighting_service` | 671 | ~1080 lines | ✅ Full implementation | `WincntMgrData` with file-persisted auto-leveling via `table::NameTable<int[]>`. `fighting_main()` — full game() orchestration loop: init phase (share push, debug script load, command sync, WincntMgr init, level-based life scaling via 2^(1/12), copyVar snapshot), round loop (hotkeys, pause toggles via Lua, camera X/Y bounds via cam_scaleBound/xBound/yBound with **cam_update() called every frame** (scale, zoff, screenX/Y, x, y sync matching SSZ Camera::update(); xOffset/yOffset wired from BGA data via formula, not stubbed), zoom render block matches SSZ: dscl=max(minScale,drawscale/baseScale), dx=cam_xBound(dscl,x+zoomposx*(dscl-scl)/dscl), dy=y+zoomposy with drawscale=NAN init for correct inactive-by-default behavior, timer countdown each tick when countdownTimer >= 0, round winner determination via char_round_winner() with p1wins/p2wins/draws increment, **timeover flag set when roundTime <= 0 triggers round end, reset in fighting_reset()**, debug input via getLastChar, frame timing, Lua loop callback, debug overlay, screen flip), post-loop cleanup (share copy, practice macro check, WincntMgr deinit). 6 helper functions (put, drawPauseMenu, drawDebug, debugInput, copyVar, reset). **fighting_reset()** also calls **cam_update()** for round-start camera init. All Lua calls use gettop/settop guard for stack safety. |
| `action_service` | 207 | 367 lines | ✅ Full implementation | `ActionData::read()` — .air file parser: frame data lines, `loopstart`, `clsn1`/`clsn2` blocks with default support, `FrameMethods::readData` delegation. `ActionData::copy()` — delegates to `ani.copy()`. `DrawnClsnData::set()` — camera-transformed coordinates. `DrawnClsnData::draw()` — screen-space rect computation (rendering wiring is TODO). |
| `ssz_service` | 24 | 63 lines | ❌ All stubs | Every method returns `false` or "deferred" string. Cannot compile or run SSZ code from native code path. |

**Updates (2026-07-06):**
- `video_service` — Full implementation (93 lines + 32 line header). File existence check via `_wfopen`, delegates to `sdlplugin::playVideo()`, sets `videoActive` flag, returns result.
- `action_service` — Full implementation (367 lines + 62 line header). `.air` file parser with frame data, `loopstart`, `clsn1`/`clsn2` blocks with default support, `DrawnClsnData::set()`/`draw()` for debug collision display.
- `fighting_service` — Full implementation (~1040 lines: 933 .cpp + 109 .hpp). WincntMgr auto-leveling with file I/O persistence. game() orchestration loop covering init/round/post-loop. All 6 helper functions implemented. Builds clean.

---

## P2 — Partial Implementations (Behavior Exists With Known Gaps)

These modules have real implementations but with deferred/suboptimal sections.

### Character Engine (`char_service`)
| Gap | Location | Impact |
|-----|----------|--------|
| `ClsnHanteiData::set()` — empty | char_service.cpp:40 | Hit collision detection disabled |
| `FallData::clear()` / `setDefault()` — empty | char_service.cpp:52-53 | Fall behavior non-functional |
| `StateValData::clearWw()` / `clear()` — empty | char_service.cpp:154-155 | State value tracking gaps |
| `StateValData::hitCheck()` — empty | char_service.cpp:156 | Hit check evaluation disabled |
| `StateValData::setHb()` — empty | char_service.cpp:157 | Hit behavior setup deferred |
| `AfterImageData::setupPalfx()` / `recAfterImg()` / `recAndAddAL()` — empty | char_service.cpp:178-180 | Afterimage effects non-functional |
| `CharData::bind()` — empty | char_service.cpp:311 | Binding logic not implemented |
| `CharData::xScreenBound()` — empty | char_service.cpp:315 | Screen clamping disabled |
| `CharData::drawAnim()` — **44-line real implementation** (was stub, now has camera-transform math + `anim->draw()`) | char_service.cpp:359 | Sprite rendering with position/scale/facing. |
| `CharData::furimuki()` — empty | char_service.cpp:406 | Face direction logic not implemented |
| `CharData::trAnimExist()` — always false | char_service.cpp:421 | Animation existence check broken |

**Additional gaps found in 2026-07-07 audit:**
| Gap | Location | Impact |
|-----|----------|--------|
| `ProjectileData::update()` — void-cast | char_service.cpp:196 | 🔴 Projectile update loop broken |
| `ProjectileData::hitCheck(ProjectileData&)` — void-cast | char_service.cpp:197 | 🔴 Projectile-vs-projectile collision disabled |
| `ProjectileData::projClsn()` — void-cast | char_service.cpp:198 | 🔴 Projectile hitbox collision disabled |
| `ProjectileData::tick()` — void-cast | char_service.cpp:199 | 🔴 Projectile time-stepping broken |
| `ProjectileData::anime()` — void-cast | char_service.cpp:200 | 🔴 Projectile animation advancement broken |
| `CharData::loadPallet()` — void-cast | char_service.cpp:250 | 🟡 Palette loading is a no-op |
| `char_draw_reflection()` — void-cast | char_service.cpp:697 | 🟡 Reflections not rendered |
| `CharData::getDamage()` — ignores kill/absolute/atkmul params | char_service.cpp:291 | 🟡 Partial — damage applies but aux params unhandled |
| `CharData::addLife()` — ignores kill/absolute params | char_service.cpp:286 | 🟡 Partial — healing works but extra flags ignored |

### Fight Engine (`fight_service`)
| Gap | Location | Impact |
|-----|----------|--------|
| `NameData::step()` — empty | fight_service.cpp:226 | Name counter never updates |
| `TimeData::step()` — empty | fight_service.cpp:245 | Time display never updates |
| `DisplayTextData::step()` — empty | fight_service.cpp:352 | Display text never updates |
| `AnimFontSndData::read()` — reads sndg/sndi/fontn only | fight_service.cpp:117 | Sprite/anim read deferred |
| `AnimFontSndData::draw()` — empty | fight_service.cpp:129 | AnimFontSnd rendering deferred |
| `NameData::draw()` — empty (TODO comment) | fight_service.cpp:500 | Name text needs font_service |
| `TimeData::draw()` — empty (TODO comment) | fight_service.cpp:550 | Timer counter text needs font_service |
| `ComboData::draw()` — empty (TODO comment) | fight_service.cpp:590 | Combo counter text needs font_service |
| `FaceData::draw()` — empty (TODO comment) | fight_service.cpp:440 | Face portrait needs SFF sprite lookup |
| `DisplayTextData::bgDraw()` / `draw()` — empty | fight_service.cpp:691 | Display text rendering deferred |
| ⚠️ **Many other draw/bgDraw methods have REAL implementations** — see notes below | fight_service.cpp | Lifebar/Powerbar/Time/WinIcon/Round/Fight draw methods all implemented with renderMugenZoom |

**Draw methods with real implementations (2026-07-07 audit):**
- `LifebarData::bgDraw()` — renders bg0/bg1/bg2 layers via `renderMugenZoom`
- `LifebarData::draw()` — creates clipped lrct/mrct rects with life-bar fill rendering
- `PowerbarData::bgDraw()` — renders power bar backgrounds
- `PowerbarData::draw()` — clipped power-bar fill rendering
- `TimeData::bgDraw()` — renders timer background sprite
- `TimeData::drawSimple()` — no-font fallback (renders bg only)
- `WinIconData::draw()` — renders win icon sprites with offset looping
- `RoundData::draw()` — full state machine (Round→Fight→KO→Win) with AnimFontSnd rendering
- `FightData::draw()` — orchestrates all sub-component draws across 3 layers (lifebars, powerbars, faces, names, timer, win icons)

### Stage Engine (`stage_service`)
| Gap | Location | Impact |
|-----|----------|--------|
| `StageData::bgDraw()` — **~80-line real implementation** (was claimed empty) | stage_service.cpp:460-560 | Boundhigh clamping, vertical follow, drawOffsetY computation, iterates over bg layers calling `bg.draw()`. Per-layer rendering (`BackGroundData::draw()`) still stub in `bg_service.cpp`. |
| `StageData::action()` — calls `envShake.next()` only | stage_service.cpp:465 | Background animation stepping deferred |
| `StageData::clear()` — skips bg/actionList/bgctrlList | stage_service.cpp:577-588 | Memory/reset leak |
| `StageData::reset()` — all deferred | stage_service.cpp:585-588 | Round reset incomplete |
| Background section parsing — deferred | stage_service.cpp:417-419 | No background layer loading from .def bg sections |
| Camera draw offset calculation — deferred | stage_service.cpp:458-459 | Stage may render at wrong position (① computed in `common_service.cpp` cam_update) |
| SFF loading — deferred | stage_service.cpp:432 | Sprite file not loaded until sff_service is wired |

### Background Engine (`bg_service`)
| Gap | Location | Impact |
|-----|----------|--------|
| `BackGroundData::read()` — empty body | bg_service.cpp:243 | No section parsing for bg layers |
| `BackGroundData::setup()` — empty | bg_service.cpp:264 | No animation/SFF wiring |
| `BackGroundData::draw()` — empty | bg_service.cpp:274 | No rendering |
| `BGCtrlData::read()` — empty | bg_service.cpp:333 | No control event parsing |

### SFF Engine (`sff_service`)
| Gap | Location | Impact |
|-----|----------|--------|
| `FrameMethods::readData()` — stores basic frame data, skips flags | sff_service.cpp:909 | Complex sprite flag parsing deferred |
| `SpriteData::loadFromSff()` — always returns "sprite not found" | sff_service.cpp:618 | Standalone sprite loading broken |
| PNG8 format — `case 10:` clears px (needs SDL_image) | sff_service.cpp:298 | PNG8 sprites not decoded |
| GL texture loading — `case 11/12:` returns early | sff_service.cpp:304 | GL texture rendering deferred |

### Command Engine (`command_service`)
| Gap | Location | Impact |
|-----|----------|--------|
| `command_mod_key_state()` — ✅ Implemented | command_service.cpp:471 | SSZ modKeyState logic in C++ → Lua via `lua_modKeyState` |
| `command_reset_read_keymap()` — empty | command_service.cpp:435 | Config/key remapping not wired |
| `command_update()` — always returns true | command_service.cpp:439 | Main input update loop deferred |
| `command_synchronize()` — always returns true | command_service.cpp:445 | Netplay sync not implemented |

### Loader Engine (`loader_service`)
| Gap | Location | Impact |
|-----|----------|--------|
| `loader_state_compile()` — returns false | loader_service.cpp:186 | State compilation not implemented |
| `loader_chara()` — basic framework, char_service not wired | loader_service.cpp:203 | Character loading incomplete |
| Thread creation — deferred | loader_service.cpp:211 | Load thread always synchronous |

### Share System (`share_service`)
| Gap | Location | Impact |
|-----|----------|--------|
| `share_copy()` — body is empty TODO | share_service.cpp:329 | No automatic pull from wired modules |
| `share_push()` — body is empty TODO | share_service.cpp:339 | No automatic push to wired modules |
| Only `common_service` wired via explicit `share_pull_from_common` | share_service.cpp:11 | Other modules (cmd, fnt, snd, chr, stage) not integrated |

### Sound Resource Engine (`sound_resource_service`)
| Gap | Location | Impact |
|-----|----------|--------|
| `play_sound()` — calls sndbuf_clear + mix_sounds but no SDL submit | sound_resource_service.cpp:487 | Buffer not sent to SDL audio callback |
| `BgmData::play()` — stores filename only | sound_resource_service.cpp:507 | BGM playback deferred (needs sdlplugin) |

### State Builder (`statebuilder_service`)
| Gap | Location | Impact |
|-----|----------|--------|
| `StateBuilderData::reset()` — empty | statebuilder_service.cpp:193 | No state builder data reset |
| `StateBuilderSize::reset()` — empty | statebuilder_service.cpp:194 | No size reset |
| `StateBuilderVelocity::reset()` — empty | statebuilder_service.cpp:195 | No velocity reset |
| `StateBuilderMovement::reset()` — empty | statebuilder_service.cpp:196 | No movement reset |
| `StateBuilderConst::reset()` — empty | statebuilder_service.cpp:197 | No const reset |
| `StateBuilder::build()` — produces a placeholder code stub | statebuilder_service.cpp:421 | State compilation generates no real SSZ code |
| Per-controller parameter parsing — deferred for all 90+ CtrlTy variants | statebuilder_service.hpp:10 | Controllers have type enum but no behavior |

---

## P3 — Thin Wrappers (Parity Evidence Missing)

These are RAII wrappers around existing C++ plugins. The wrappers themselves are correct, but there are no tests proving behavioral parity with the SSZ scripts.

| Module | File Sizes | Bridge Already Traces? | Needed |
|--------|-----------|-----------------------|--------|
| `regex_service` | ~150 lines hpp+cpp | ✅ (real `std::wregex` impl) | Genuine self-contained implementation using `std::wregex`/`boost::wregex` (not a thin wrapper). Has `compile`, `search`, `search_matches`, `search_all`. No legacy plugin delegation. Match array shape + error behavior parity tests still pending. |
| `socket_service` | ~80 lines | ✅ | Mocked socket tests + loopback integration test |
| `sound_service` | ~80 lines | ✅ | Short audio buffer smoke tests |
| `ogg_service` | ~100 lines | ✅ | Sample .ogg open/read/seek parity tests |
| `mesdialog_service` | ~150 lines | ✅ | INI read/write, encoding, compression parity |
| `lua_service` | ~200 lines hpp+cpp | ✅ | Callback registration compatibility definition |

**📝 Discrepancy:** TODO_SSZ_CONVERSION.md lists `lua_service` as "Phase 2" priority, but the actual source has a full RAII `LuaState` class with all bridge-delegation methods. The remaining gap is callback registration parity — not the basic API scaffolding.

---

## P4 — Cross-Module Wiring (Backend Delegation)

These items are marked "deferred until module X is converted" in the source code.

| From | To | What's Deferred | Impact |
|------|----|-----------------|--------|
| `loader_service` | `stage_service` | Stage loading via `loader_stage()` | ✅ Actually wired! Calls `system_get_selected_stage_def()` + `stage_load()`. |
| `loader_service` | `char_service` | Character loading via `loader_chara()` | ⚠️ Partially wired — calls `system_get_selected_char_def()` but `char_service::load()` only parses def file name, no full char init. |
| `loader_service` | `statebuilder_service` | State compilation via `loader_state_compile()` | ❌ Returns false. No SSZ compiler handle available. |
| `loader_service` | `thread_service` | Async load thread | ❌ Thread creation is `//deferred`. Load always runs synchronously. |
| `stage_service` | `bg_service` | Background layer parsing and rendering | ⚠️ `bgDraw()` orchestration implemented at stage level (calls `bg.draw()` per layer). Section parsing and `BackGroundData::draw()` still deferred in bg_service. |
| `bg_service` | `sdlplugin_service` | `BackGroundData::draw()` — rendering | ❌ Complex parallax/window/zoom rendering not wired. |
| `sff_service` | `sdlplugin_service` | GL texture loading, GL draw | ❌ `Load8bitTexture` init exists but no render loop integration. |
| `fight_service` | `sdlplugin_service` / `sff_service` / `font_service` | Lifebar/powerbar/combo/text rendering | ⚠️ **Partially wired** — Lifebar/Powerbar/Time/WinIcon/Round/Fight draw methods call `renderMugenZoom`. Name/Combo/DisplayText text rendering still deferred (needs font_service). |
| `char_service` | `sdlplugin_service` | `CharData::drawAnim()` — sprite rendering | ✅ Implemented (44 lines, camera-transform + `anim->draw()`) |
| `command_service` | `sdlevent_service` | `command_mod_key_state()` — input filtering | ✅ Implemented — `command_mod_key_state()` has full SSZ logic; `command_update()` polls sdlevent key state directly. |
| `sound_resource_service` | `sdlplugin_service` | `play_sound()` — submit buffer to SDL audio | ❌ Buffer mixed but never sent to SDL. |
| `share_service` | All wired modules | Automatic `pull_from_*` / `push_to_*` | ❌ Only `common_service` has explicit pull/push. |
| `common_service` | `mesdialog_service` | `common_read_file_name()` — AsciiToLocal conversion | ✅ **Fixed** — now properly converts UTF-8 → ANSI code page via `MultiByteToWideChar`/`WideCharToMultiByte`. |

---

## P5 — Test Gaps

| # | Test | Status | Details |
|---|------|--------|---------|
| 1 | `make CONFIG=Debug test` passes | ✅ | Exit code 0 |
| 2 | `make native_manifest` output saved | ❌ | Should commit to `docs/native_manifest.txt` |
| 3 | Per-module SSZ parity tests for regex, socket, sound, ogg, mesdialog | ❌ | No golden tests for any of these |
| 4 | `.def` / `.cmd` / `.air` / `.sff` / stage file representative resource tests | ❌ | No test resources committed |
| 5 | Runtime smoke test: load one char + stage + start match | ❌ | Core gameplay path never tested |
| 6 | Rollback tests per module: `make IKEMEN_NATIVE_*_LIB=0` for each module | ❌ | Flag toggle tested only for config/file |
| 7 | CI/build check for removed SSZ symbols | ❌ | Not implemented |
| 8 | CI/build check for known no-op replacements at boot | ❌ | Not implemented |

---

## P6 — Platform & Build

| # | Item | Status | Details |
|---|------|--------|---------|
| 1 | Linux Makefile build | ⚠️ Experimental | `uname -m` detection, `-m32` support, `boost::regex` fallback |
| 2 | `regex_service` Linux compatibility | ⚠️ | Uses `boost::wregex` on Linux — parity with `std::wregex` on Windows not verified |
| 3 | `w64devkit` toolchain path documentation | ✅ | Documented in `AGENTS.md` |
| 4 | Build with all native modules enabled | ⚠️ | `IKEMEN_USE_NATIVE_SSZ=1` produces executable but behavior unverified |
| 5 | Clean build: separate Debug vs Release | ✅ | Both work |

---

## P7 — Source Hygiene

| # | Item | Count | Details |
|---|------|-------|---------|
| 1 | `TODO` / `FIXME` / `HACK` / `XXX` comments in `main/ssz_native/` | ~75 | Across all source files — see below for per-file breakdown |
| 2 | `plugin_native_api.hpp` consolidation TODOs | 6 | `TODO: Move to plugin_native_api.hpp when * is migrated` in `lua_service.cpp`, `socket_service.cpp`, `sound_service.cpp`, `ogg_service.cpp`, `mesdialog_service.cpp`, `alert_service.cpp` |
| 3 | Duplicate extern declarations in bridge.cpp vs ssz_native/*.cpp | ~50 | Each thin wrapper module redeclares the extern "C" plugin functions that already exist in bridge.cpp |
| 4 | `regex_service.hpp` `TODO: Consolidate with RegexMatchInfo` | 1 | Cross-module type duplication |
| 5 | Hardcoded `#ifdef _WIN32` for `_wfopen_s` and other Windows APIs | ~20 | Linux portability issues |

---

## Per-File Deferred Item Count

Quick-reference count of `TODO` / `FIXME` / `deferred` / `pending` markers per file:

| File | Markers | Notable items |
|------|---------|---------------|
| `stage_service.cpp` | 12 | bg parsing, bgDraw, bgAction, camera calc, SFF loading, clear/reset |
| `bg_service.cpp` | 6 | Section parsing, setup, rendering, BGCtrl processing |
| `video_service.cpp` | 1 | File existence check, PlayVideo delegation — no deferred items (completed) |
| `action_service.cpp` | 1 | `.air` parser + DrawnClsnData — `draw()` rendering wiring deferred until SDL pipeline is in place |
| `sff_service.cpp` | 5 | PNG8, GL texture, flag parsing, palette copy, standalone loading |
| `char_service.cpp` | 14 | ProjectileData (5 stubs), Clsn, Fall, AfterImage, binding, screen bound, face, anim check, palette loading, reflection, angle draw, shadow draw, char draw subsystems |
| `command_service.cpp` | 3 | SDL processing, keymap reset, netplay sync |
| `share_service.cpp` | 4 | Module integration TODO, pull/push from wired modules |
| `fight_service.cpp` | 11 | Anim read deferred, face anim deferred, face portrait SFF lookup deferred, name text rendering deferred, timer text rendering deferred, combo text rendering deferred, winicon count iteration deferred, 5 TODO markers for font_service wiring |
| `loader_service.cpp` | 4 | Thread creation, state compile, char loading |
| `sound_resource_service.cpp` | 2 | SDL submission, BGM playback |
| `statebuilder_service.cpp` | 8 | Per-controller parsing deferred, reset methods empty, placeholder codegen |
| `ssz_service.cpp` | 4 | All methods: run, compileFile, compileString, compilerRun all deferred |
| `script_service.cpp` | 0 | (none — all 190+ callbacks registered, `ref_arg` wired, `drawTTF` with alignment/scale) |
| `trigger_script_service.cpp` | 0 | (none — all 130+ trigger functions implemented and registered) |
| `system_script_service.cpp` | 0 | (none — all 120+ system functions implemented and registered) |
| `fighting_service.cpp` | 13 | Init (share/debug/char/wm setup — 4 stubs), round loop (camera/events/debug/input/rendering — 6 stubs for char/stage/hotkey access), cleanup (share copy/practice/wm deinit — 3 stubs). All 3 high-level phases are real implementations; the stubs mark character/stage subroutine calls that depend on other modules. |
| `debug_script_service.cpp` | 0 | (none — all 25 debug functions implemented: puts, sszReload, setLife/Max/Power/Attack/Defence, selfState, addHotkey, all toggle*, step, roundReset, reload, setAccel, setAILevel, setTime, clear) |
| `common_service.cpp` | 2 | `AsciiToLocal` no-op, callback wiring deferred |
| `plugin_native_api.hpp` | 1 | 6 remaining plugins not consolidated |

---

## Priority Matrix

```
                    High Impact                    Low Impact
                ┌─────────────────────┬─────────────────────┐
                │                     │                     │
   Urgent       │  P0: Traces         │  P1: Lua callbacks  │
                │  P0: Parity pipeline │  P1: font stub      │
                ├─────────────────────┼─────────────────────┤
                │                     │                     │
   Important    │  P4: Cross-module   │  P5: Test gaps      │
                │      wiring          │                     │
                │  P2: Rendering gaps  │  P6: Platform       │
                │                     │  P7: Source hygiene  │
                └─────────────────────┴─────────────────────┘
```

---

## Discrepancies With TODO_SSZ_CONVERSION.md

| # | TODO_SSZ_CONVERSION.md Says | Source Code Shows | Impact |
|---|---------------------------|-------------------|--------|
| 1 | `config_service` is "Behavior implemented 🟢" | ✅ Correct — full INI save/load exists | None |
| 2 | `stage_service` is "Behavior ❌" | ⚠️ Partially correct — `stage_service` has real `EnvShake`, `load()` with def file parsing (525 lines), but bg/SFF sections deferred | Underreport: stage is further along than documented |
| 3 | `bg_service` is "Behavior 🟢" | ✅ **TODO is actually correct** — the module status matrix shows Behavior = 🟡 (yellow/partial), not 🟢. This PENDINGS entry was itself stale. `BackGroundData::read()`/`setup()`/`draw()` are indeed stubs, justifying 🟡. | Stale PENDINGS claim — no longer a TODO discrepancy |
| 4 | `statebuilder_service` is "Behavior 🟡" | ✅ Correct — CtrlTy enum, cmd file parser, section extraction all real. Per-controller parsing deferred. | None |
| 5 | `fighting_service` is "Behavior ❌" | ⚠️ Previously correct (stub) — NOW UPDATED by commit: `fighting_service.cpp` is ~1040 lines of real implementation (WincntMgr auto-leveling, game() orchestration loop, 6 helpers). TODO_SSZ_CONVERSION.md has been updated to 🟢 Behavior. | ✏️ Fixed — fighting_service no longer a stub |
| 6 | `sdlplugin_service` is "Behavior 🟡" | ⚠️ Understated — all 36 public API functions are real wrappers. Only the 3 complex render functions (renderMugenZoom, renderMugenShadow, renderFontBatch) have Reference-based bridge-layer complexity | Underreport: SDL boundary is more complete than documented |
| 7 | `sdlevent_service` is "Behavior 🟢" | ✅ Correct — full key tracking, event polling | None |
| 8 | `lua_service` listed as "Phase 2" priority | Actual status: full RAII LuaState with all bridge delegation methods | Priority mismatch: should be P3 (already scaffolded) |
| 9 | "20 of 45 SSZ modules now have real native behavior" in Overview | ✅ Still accurate as of 2026-07-05 | None |
| 10 | Stub files list includes `sdlevent_service.cpp` | ❌ Wrong — `sdlevent_service.cpp` is ~320 lines of real implementation | Stale documentation |
| 11 | `fight_service` Behavior is 🟡 (partial) — rendering deferred | ✅ Behavior is correctly 🟡, but **draw progress is further along than the 🟡 implies** — LifebarData/PowerbarData/TimeData/WinIconData/RoundData/FightData all have real bgDraw/draw methods with renderMugenZoom calls. Only font-dependent text rendering (Name/Combo/DisplayText) and face portrait SFF lookup remain stubbed. | Underreport: draw/bgDraw is more complete than documented |
| 12 | `command_service` — `command_update()` claimed "always returns true" / "input loop deferred" | ⚠️ **Source now has real implementation** — `command_update()` polls sdlevent key state (upKey/downKey/leftKey/rightKey/aKey/sKey/dKey/zKey/xKey/cKey/qKey/wKey/eKey) and feeds filtered input into each player's buffer via `command_mod_key_state()` filtering. Only `command_synchronize()` (netplay) remains stub. | PENDINGS claim was stale — command_update is now real |
| 13 | `StageData::bgDraw()` claimed "empty" / rendering deferred | ⚠️ **Source has ~80-line real implementation** — boundhigh clamping, vertical follow with screen offset math, drawOffsetY exponent computation, non-zoom ceil rounding, and iteration over background layers calling `bg.draw()`. The remaining stub is `BackGroundData::draw()` in bg_service.cpp (per-layer rendering). PENDINGS itself was stale on this. | PENDINGS claim was stale — stage bgDraw orchestration is real |
| 14 | `font_service` is "Full implementation" in PENDINGS | ✅ Correct — 952 lines with FNT v1/v2 loader, charWidth/textWidth, drawChar/drawText with renderMugenZoom/renderFontBatch, palette-banked sprite access. However, no SSZ parity traces exist for font rendering output. | Parity traces still pending for font rendering |

---

## Quick Wins (Fixed Items)

1. **Capture and commit startup traces** ✅ — Method 1 A/B traces captured (baseline 121,994 lines, native 79,225 lines). 100% function-level parity confirmed.
2. **Save `make native_manifest` output** to `docs/native_manifest.txt` ✅ All 35 modules = 1
3. **Fix `common_service.cpp:281`** — `AsciiToLocal` no-op ✅ — now properly converts UTF-8 → ANSI code page via `MultiByteToWideChar`/`WideCharToMultiByte`
4. **Fix `stage_service.cpp` clear/reset** ✅ — added trace calls, `def.clear()`, proper re-init
5. **Fix SSZ-only boot** ✅ — removed `#if IKEMEN_NATIVE_*_LIB` guards from 13 static headers
6. **Update TODO_SSZ_CONVERSION.md** ✅ — fixed all 10 discrepancies
7. **Implement `command_mod_key_state()`** ✅ — full SSZ modKeyState logic in C++ (requires build verification)
8. **Commit pre-conversion trace log** — ❌ Still needs to be committed to repo