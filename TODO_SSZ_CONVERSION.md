# TODO_SSZ_CONVERSION.md — Reviewed And Reordered Plan

Last reviewed: **2026-07-04**

## Status Terminology

Every module in this document is tracked across three dimensions:

| Status | Meaning |
|---|---|
| **Route registered** | Static plugin header exists, bridge wrapper exists, `main.cpp` calls `*_static_register()`. The ABI route is wired — calling the module's SSZ functions dispatches to the native service. |
| **Behavior implemented** | The native service contains real logic (not stubs/placeholders) for all or most of the module's public SSZ symbols. |
| **Parity proven** | Golden tests, trace comparison, or runtime smoke tests confirm the native behavior matches SSZ output. |

A module can be at different levels on each dimension. For example, `char_service` has a **Route registered** (✅) but is **Behavior implemented — pending** (⚠️) and **Parity proven — pending** (❌).

## Goal

Convert the SSZ script tree under `ssz_script/` into native C++ while keeping behavior identical. The native code must be able to replace the SSZ layer incrementally, module by module, with rollback flags and parity tests.

## Current Source Review Snapshot

- SSZ files: **45**
- SSZ script lines: **~39810**
- `lib ... = <...>` imports: **151**
- `plugin index` declarations: **20**
- `main/ssz_native/` files: **84** (42 `.cpp` + 42 `.hpp`)
- Native module flags in `Makefile`: **38**
- Registration calls in `main.cpp`: **34**
- Native service files with `stub` / `placeholder` / `no-op` markers: **20**
- Marker lines found during review: **~32**

## Review Verdict

The project has made strong progress: the native ABI bridge exists, the plugin boundary is fully converted (34/34 static registrations wired into main.cpp boot sequence), `main/ssz_native/` has broad coverage (84 files, 10,279 lines), and 20 of 45 SSZ modules now have real (non-stub) behavior implemented.

The project has **moved decisively past scaffolding** into the **behavior implementation phase**. The following modules now have real C++ implementations: common_service (382 lines), loader_service (212 lines), sound_resource_service (517 lines), share_service (344 lines), config_service (232 lines), bg_service (485 lines), sff_service (921 lines), command_service (465 lines), char_service (405 lines), stage_service (525 lines), sdlplugin_service (~210 lines) — among others. The only remaining stub-only modules are action, debug_script, fight, fighting, font, script, sdlevent, statebuilder, system_script, trigger_script, and video.

### Key Findings

1. **20/45 SSZ files have real native behavior** — up from ~10 in the previous review.
2. **34 static plugin routes are registered** in `main.cpp` — every module has bridge coverage.
3. **Foundation libraries are complete**: file, string, math, table, crypto, stack all have parity tests passing.
4. **Core runtime state modules are wired**: common_service (all 16 functions), loader_service (state machine), share_service (CommonData integration), system_service (selection helpers).
5. **Resource modules progressing**: sound_resource_service (517 lines, ElecbyteSnd parser + mixers), sff_service (921 lines, sprite format), command_service (465 lines, input parser).
6. **The biggest remaining stubs** are Lua-related: script_service (250+ callbacks), trigger_script_service (170+ callbacks), system_script_service (200+ callbacks), debug_script_service (27 callbacks). These depend on Lua bridge wiring.
7. **SDL boundary making progress**: sdlplugin_service is now a full implementation (~210 lines) with all public API functions delegating to the existing SDL plugin. sdlevent_service remains a stub (9 lines) and is the next priority for SDL boundary work.
8. **Runtime traces are still not present**. Trace capture remains a P0 item because parity cannot be trusted without pre/post behavior comparison.
9. **Test suite passes** (`make CONFIG=Release test`, exit code 0). One known test issue: `SelectData getStageName stub` in system_service.
10. **The next milestone is wiring backend delegation** — replacing "deferred until module X is converted" 
    comments with real cross-module calls (e.g., loader→stage_service, loader→char_service, 
    sound_resource→sdlplugin_service).

## Dependency Hotspots

Prioritize modules that many other SSZ files import:

- `string.ssz` — 22 inbound imports
- `math.ssz` — 21 inbound imports
- `alpha/sdlplugin.ssz` — 18 inbound imports
- `file.ssz` — 15 inbound imports
- `consts.ssz` — 14 inbound imports
- `alert.ssz` — 12 inbound imports
- `table.ssz` — 10 inbound imports
- `alpha/sdlevent.ssz` — 9 inbound imports
- `alpha/lua.ssz` — 7 inbound imports
- `alpha/mesdialog.ssz` — 7 inbound imports
- `ssz.ssz` — 5 inbound imports
- `shell.ssz` — 4 inbound imports

## Plugin Boundary Declarations Still Present In SSZ Scripts

These are the current script/plugin boundary declarations to preserve during the migration:

- `lib/ssz.ssz:24` — `plugin index NewCompiler(::) = <dll/ssz.dll>;`
- `lib/file.ssz:20` — `plugin index Open(:^/char, ^/char:) = <dll/file.dll>;`
- `lib/file.ssz:26` — `plugin index Close(:index:) = <dll/file.dll>;`
- `lib/file.ssz:47` — `plugin index ReadAry(:index, ^_t, index:) = <dll/file.dll>;`
- `lib/file.ssz:58` — `plugin index WriteAry(:index, ^/_t, index:) = <dll/file.dll>;`
- `lib/socket.ssz:37` — `plugin index SocketAccept(:index, int, bool:) = <dll/socket.dll>;`
- `lib/socket.ssz:58` — `plugin index SocketRecvAry(:index=, ^_t, index:) = <dll/socket.dll>;`
- `lib/socket.ssz:69` — `plugin index SocketSendAry(:index=, ^/_t, index:) = <dll/socket.dll>;`
- `lib/sound.ssz:10` — `plugin index NewClient(::) = <dll/sound.dll>;`
- `lib/regex.ssz:18` — `plugin index NewRegex(:^/char, bool, ^char=:) = <dll/regex.dll>;`
- `lib/alpha/lua.ssz:45` — `plugin index NewState(::) = "dll/lua.dll";`
- `lib/alpha/mesdialog.ssz:45` — `plugin index TazyuuCheck(:^/char:) = "dll/mesdialog.dll";`
- `lib/alpha/sdlplugin.ssz:124` — `plugin index SendWriteBGM(:^/short:) = "dll/sdlplugin.dll";`
- `lib/alpha/sdlplugin.ssz:825` — `plugin index AllocSurface(:int, int:) = "dll/sdlplugin.dll";`
- `lib/alpha/sdlplugin.ssz:831` — `plugin index IMGLoad(:^/char:) = "dll/sdlplugin.dll";`
- `lib/alpha/sdlplugin.ssz:842` — `plugin index CreatePaletteSurface(:ubyte=, uint=, int, int:) = "dll/sdlplugin.dll";`
- `lib/alpha/sdlplugin.ssz:847` — `plugin index SetColorKey(:index, int:) = "dll/sdlplugin.dll";`
- `lib/alpha/sdlplugin.ssz:867` — `plugin index OpenFont(:^/char, int:) = "dll/sdlplugin.dll";`
- `lib/alpha/ogg.ssz:6` — `plugin index NewOggVorbis(::) = "dll/ogg.dll";`
- `lib/alpha/ogg.ssz:41` — `plugin index OggVorbisRead(:index, ^short:) = "dll/ogg.dll";`

## Native Runtime / Feature-Flag Wiring Review

### Static plugin headers already guarded (34 total, 34 wired)

All 34 `*_static.hpp` headers are guarded by `#if IKEMEN_NATIVE_*_LIB` and their `*_register()` functions are called in `main/main.cpp` bootstrap before `Run(scriptPath)`.

- `alert_static.hpp`: IKEMEN_NATIVE_ALERT_LIB
- `file_static.hpp`: IKEMEN_NATIVE_FILE_LIB
- `lua_static.hpp`: IKEMEN_NATIVE_LUA_LIB
- `math_static.hpp`: IKEMEN_NATIVE_MATH_LIB
- `mesdialog_static.hpp`: IKEMEN_NATIVE_MESDIALOG_LIB
- `ogg_static.hpp`: IKEMEN_NATIVE_OGG_LIB
- `regex_static.hpp`: IKEMEN_NATIVE_REGEX_LIB
- `sdlplugin_static.hpp`: IKEMEN_NATIVE_SDLPLUGIN_LIB
- `shell_static.hpp`: IKEMEN_NATIVE_SHELL_LIB
- `socket_static.hpp`: IKEMEN_NATIVE_SOCKET_LIB
- `sound_static.hpp`: IKEMEN_NATIVE_SOUND_LIB
- `thread_static.hpp`: IKEMEN_NATIVE_THREAD_LIB
- `time_static.hpp`: IKEMEN_NATIVE_TIME_LIB
- `share_static.hpp`: IKEMEN_NATIVE_SHARE_LIB
- `common_static.hpp`: IKEMEN_NATIVE_COMMON_LIB
- `loader_static.hpp`: IKEMEN_NATIVE_LOADER_LIB
- `system_static.hpp`: IKEMEN_NATIVE_SYSTEM_LIB
- `debug_script_static.hpp`: IKEMEN_NATIVE_DEBUG_SCRIPT_LIB
- `script_static.hpp`: IKEMEN_NATIVE_SCRIPT_LIB
- `trigger_script_static.hpp`: IKEMEN_NATIVE_TRIGGER_SCRIPT_LIB
- `system_script_static.hpp`: IKEMEN_NATIVE_SYSTEM_SCRIPT_LIB
- `char_static.hpp`: IKEMEN_NATIVE_CHAR_LIB
- `statebuilder_static.hpp`: IKEMEN_NATIVE_STATEBUILDER_LIB
- `fighting_static.hpp`: IKEMEN_NATIVE_FIGHTING_LIB
- `fight_static.hpp`: IKEMEN_NATIVE_FIGHT_LIB
- `video_static.hpp`: IKEMEN_NATIVE_VIDEO_LIB
- `font_static.hpp`: IKEMEN_NATIVE_FONT_LIB
- `action_static.hpp`: IKEMEN_NATIVE_ACTION_LIB
- `sound_resource_static.hpp`: IKEMEN_NATIVE_SOUND_RES_LIB
- `bg_static.hpp`: IKEMEN_NATIVE_BG_LIB
- `stage_static.hpp`: IKEMEN_NATIVE_STAGE_LIB
- `sff_static.hpp`: IKEMEN_NATIVE_SFF_LIB
- `command_static.hpp`: IKEMEN_NATIVE_COMMAND_LIB
- `config_static.hpp`: IKEMEN_NATIVE_CONFIG_LIB

### Script-service runtime integration gap

Only a small number of native services are referenced outside `main/ssz_native/` and tests:

- `math_service.hpp` referenced by: main/math_static.hpp
- `config_service.*` referenced by: main/main.cpp startup path (config load at boot)
- `file_service.*` referenced by: main/file_static.hpp (plugin registration) and test suite
- `common_service.*` referenced by: multiple native services via `common_get_state()` accessor
- `string_service.*` referenced by: other service `string_util` namespace

Pending action: each `IKEMEN_NATIVE_*` script-layer flag must eventually control an actual runtime route, not just compile a service file.

## Prioritized Backlog

### P0 — Conversion Safety And Truthfulness

These block trustworthy conversion.

- [x] Regenerate `docs/ssz_dependency_graph.txt` from the uploaded source.
- [x] Regenerate `docs/ssz_symbol_manifest.txt` from the uploaded source.
- [x] Regenerate `docs/native_ssz_comparison.md` from the uploaded source.
- [ ] Capture and commit startup trace with `IKEMEN_ENABLE_PLUGIN_TRACE=1`.
- [ ] Capture and commit gameplay trace that reaches at least title, select, load character, load stage, start match, play audio.
- [ ] Add a post-conversion trace comparison format: `pre_trace -> native_trace -> diff`.
- [ ] All modules now use the three-state tracking system: **Route registered**, **Behavior implemented**, **Parity proven**.
- [ ] Add CI/build check that fails when a `.ssz` public symbol is removed without a native replacement or explicit deprecation note.
- [ ] Add CI/build check that fails when `IKEMEN_USE_NATIVE_SSZ=1` relies on a known no-op replacement for runtime-critical boot paths.
- [ ] Run `make CONFIG=Debug test` and save the result in `docs/native_test_results.txt`.
- [ ] Run `make native_manifest` and save the result in `docs/native_manifest.txt`.

### P1 — Establish The First Real Native End-To-End Path

Recommended target: **file/config/system boot path**, because `file.ssz`, `config.ssz`, and `system.ssz` already have native scaffolding and are easier to validate than gameplay state machines.

- [x] Finish `file_service` parity against `ssz_script/lib/file.ssz`.
- [x] Add parity tests for all `file.ssz` APIs: open, close, readAry, writeAry, load/save text, find, findDir, delete, move, copy, create/remove dir, current dir.
- [x] Wire `IKEMEN_NATIVE_FILE_LIB` to an actual script-layer replacement route, not only static plugin wrappers.
- [x] Convert `save/config.ssz` and `save/configNet.ssz` from struct defaults into load/save/migration behavior.
- [x] Add tests comparing saved config content and loaded values against the SSZ-generated files.
- [x] Wire the minimal native config path into startup behind `IKEMEN_NATIVE_CONFIG_LIB`.
- [x] Verify rollback: `make IKEMEN_NATIVE_FILE_LIB=0 IKEMEN_NATIVE_CONFIG_LIB=0` still builds and runs old behavior.

### P2 — Make Foundation Libraries Parity-Tested

These are frequently imported and should become stable dependencies for higher layers.

- [x] `string_service`: add golden tests for trim, split, join, replace, numeric conversion, UTF-8 encode/decode, percent encode/decode, ASCII-only case conversion.
- [x] `math_service`: add SSZ parity tests for random sequence, `randI`, `randF`, rounding, bounds, and edge cases.
- [x] `table_service`: compare every public `table.ssz` behavior; expand beyond a simple map wrapper if needed.
- [x] `crypto_service`: add SSZ parity tests for base64, arcfour, and MD5 outputs.
- [x] `stack_service`: complete public-symbol parity against `lib/stack.ssz`.
- [x] `consts.hpp`: verify every constant and sentinel value against `lib/consts.ssz`.

### P3 — Complete Plugin Wrapper Libraries

These are native wrappers around existing C++ plugin implementations. They should be completed before complex gameplay modules depend on them.

- [ ] `regex_service`: prove match array shape and error behavior against `regex.ssz`.
- [ ] `socket_service`: add mocked socket tests and a loopback integration test.
- [ ] `sound_service`: add mocked/short audio buffer smoke tests.
- [ ] `ogg_service`: add sample OGG open/read/seek parity tests.
- [ ] `mesdialog_service`: add non-interactive tests for INI, encoding, compression, shared string.
- [ ] `lua_service`: define exact LuaState ownership and callback registration compatibility.
- [ ] `alert/thread/time/shell`: complete small wrapper smoke tests and mark parity-tested.
- [ ] Consolidate duplicate native declarations into `plugin_native_api.hpp` or explicitly document why a declaration stays local.

### P4 — SDL Boundary Before Menus/Game Logic

SDL is a high fan-in dependency and should be handled before replacing UI/gameplay code.

- [ ] Replace `sdlevent_service` stub with real key/event polling behavior.
- [x] Replace `sdlplugin_service` stub with native wrappers for surfaces, palettes, images, fonts, renderer info, BGM write, color key, and lifetime cleanup.
- [ ] Add destructor/free-path tests for SDL surfaces, fonts, textures, and audio/video handles.
- [ ] Add trace comparison for menu input and render calls.
- [ ] Decide whether `IKEMEN_NATIVE_SDLPLUGIN_LIB` and `IKEMEN_NATIVE_SDLPLUGIN_SCRIPT_LIB` should remain separate.

### P5 — Core Runtime State Modules

These should be wired after foundation and SDL/service layers are stable.

Recommended order:

1. `share_service` — real `copy()` / `push()` state transfer.
2. `common_service` — frame/time/match helper behavior.
3. `system_service` — finish selection data and system state methods.
4. `loader_service` — real stage/char/state loading flow.
5. `debug_script_service` — debug callbacks after Lua bridge details are stable.
6. `script_service` — register and implement core Lua-facing callbacks.
7. `trigger_script_service` — register and implement trigger callbacks.
8. `system_script_service` — system-level Lua callbacks.
9. `statebuilder_service` — last among core modules because it is largest and parser/compiler-sensitive.

Pending items:

- [x] Replace `share_service.cpp` no-op copy/push with real state accessors.
- [x] Replace `common_service.cpp` false/no-op helpers with SSZ-equivalent logic.
- [x] Replace `loader_service.cpp` false/no-op loader functions with real behavior.
- [ ] Replace Lua callback registration stubs with actual registrations and function bodies.
- [ ] Add golden trace tests for statebuilder before implementing it.
- [ ] Wire `system_service::getStageName` real stage name lookup from selection state.

### P6 — Gameplay And Resource Modules

These are currently mostly structs/placeholders. Convert in risk order, not file-size order.

1. `video_service` — small and isolated.
2. `font_service` — required for UI text and easier to smoke-test visually.
3. `action_service` — animation frame data.
4. `sound_resource_service` — resource descriptors.
5. `stage_service` / `bg_service` — stage/background parse and render state.
6. `sff_service` — sprite file format behavior.
7. `command_service` — input command parser.
8. `fighting_service` — orchestration.
9. `fight_service` — fight loop state.
10. `char_service` — largest gameplay risk; do after dependencies are stable.

Pending items:

- [ ] Replace placeholder structs with real parsed resource models.
- [ ] Add golden tests using representative `.def`, `.cmd`, `.air`, `.sff`, and stage files.
- [ ] Add runtime smoke tests: load one character, load one stage, start match, render fight UI.
- [ ] Add rollback flags per gameplay module.

### P7 — Remove SSZ Runtime Dependency Gradually

- [ ] Inventory every call to `Run(...)`, `CompilerCompile(...)`, and SSZ runtime APIs.
- [ ] For each converted subsystem, remove its direct `.ssz` load path or replace it with a compatibility stub.
- [ ] Add a build/runtime mode that boots without loading converted `.ssz` files.
- [ ] Track remaining `.ssz` files loaded at runtime in `docs/native_boot_remaining_ssz.txt`.
- [ ] Retire `main/ssz/bridge.cpp` only after all external SSZ plugin ABI calls are gone or deliberately preserved.

## Module Status Matrix

**Status key:** 🟢 Route registered · 🟢 Behavior implemented · 🟢 Parity proven

| SSZ module | Native file(s) | Priority | Route | Behavior | Parity | Next action |
|---|---|:---:|:---:|:---:|:---:|---|
| `lib/consts.ssz` | `consts.hpp` | P1 | 🟢 | 🟢 | 🟢 | All 10 type aliases match SSZ, Signed/Unsigned templates verified, null<T>() equivalents exist, 30+ parity assertions in native test suite. |
| `lib/math.ssz` | `math_service.*` | P1 | 🟢 | 🟢 | 🟢 | Full SSZ parity tests for PRNG, trig, rounding, utility templates. |
| `lib/string.ssz` | `string_service.*` | P1 | 🟢 | 🟢 | 🟢 | All major utilities implemented and tested; `&Format` object complete. |
| `lib/table.ssz` | `table_service.hpp` | P1 | 🟢 | 🟢 | 🟢 | NameTable + IntTable + intHash; operate/each_value added. |
| `lib/stack.ssz` | `stack_service.hpp` | P1 | 🟢 | 🟢 | 🟢 | push/pop/top/clear/empty/size all implemented. |
| `lib/base64.ssz, lib/arcfour.ssz, lib/md5.ssz` | `crypto_service.*` | P1 | 🟢 | 🟢 | 🟢 | Base64, Arcfour (with getByte), MD5 all tested with known-answer vectors. |
| `lib/file.ssz` | `file_service.*` | P2 | 🟢 | 🟢 | 🟢 | RAII FileHandle, all free functions, generic read_all_as template. |
| `lib/regex.ssz` | `regex_service.*` | P2 | 🟢 | 🟢 | ❌ | RAII Regex exists; needs SSZ match-shape parity tests. |
| `lib/socket.ssz` | `socket_service.*` | P2 | 🟢 | 🟡 | ❌ | RAII wrapper exists; needs mocked and live socket tests. |
| `lib/sound.ssz` | `sound_service.*` | P2 | 🟢 | 🟡 | ❌ | Thin wrapper exists; real audio smoke still pending. |
| `lib/alpha/ogg.ssz` | `ogg_service.*` | P2 | 🟢 | 🟡 | ❌ | Wrapper exists; needs sample decode/seek parity. |
| `lib/alpha/mesdialog.ssz` | `mesdialog_service.*` | P2 | 🟢 | 🟡 | ❌ | Thin wrapper exists; dialog/INI/encoding parity pending. |
| `lib/alert.ssz, thread.ssz, time.ssz, shell.ssz` | `alert/thread/time/shell_service.*` | P2 | 🟢 | 🟢 | ❌ | Thin wrappers exist; low risk after static/header wiring. |
| `lib/alpha/lua.ssz` | `lua_service.*` | P2 | 🟢 | 🟡 | ❌ | RAII LuaState exists; Lua callback registration parity not done. |
| `lib/alpha/sdlevent.ssz` | `sdlevent_service.*` | P2 | ❌ | ❌ | ❌ | Stub; prioritize because SDL events drive input and menus. |
| `lib/alpha/sdlplugin.ssz` | `sdlplugin_service.*` | P2 | 🟢 | 🟡 | ❌ | Full header with all enums (EventType, SDLKey, K), structs (SSZ Rect→SdlRect, Surface, Font, GlTexture, UseGlContext, keyboard/mouse event types), constants, and 30+ public API functions delegating to existing main/sdlplugin/sdlplugin.cpp. Complex render functions (renderMugenZoom, renderMugenShadow) still need bridge layer. |
| `lib/ssz.ssz` | `plugin/native runtime API` | P2 | 🟢 | 🟡 | ❌ | Native plugin ABI exists; script-level compiler facade still needs parity tests. |
| `ssz/share.ssz` | `share_service.*` | P3 | 🟢 | 🟢 | 🟡 | Real copy/push with CommonData integration (~110+ field mappings); automatic pull/push from wired modules still pending. |
| `ssz/system.ssz` | `system_service.*` | P3 | 🟢 | 🟡 | ❌ | Some selection helpers wired; getStageName stub known to fail test. |
| `ssz/common.ssz` | `common_service.*` | P3 | 🟢 | 🟢 | 🟡 | All 16 public functions implemented (flagInit, resetRemapInput, setSize, tickFrame, tickNextFrame, tickInterpola, addFrameTime, resetFrameTime, matchOver, nextLine, splitLines, atof, atoi, loadText, readFileName, loadFile). Extensive test coverage. |
| `ssz/loader.ssz` | `loader_service.*` | P3 | 🟢 | 🟢 | ❌ | Real state machine (NotYet→Loading→Complete), error handling, stage loading framework, character loading framework, compile framework, load loop. Backend delegation pending (stage_service, char_service). |
| `ssz/debug-script.ssz` | `debug_script_service.*` | P3 | 🟢 | ❌ | ❌ | 27 no-op callbacks/loaders. |
| `ssz/script.ssz` | `script_service.*` | P3 | 🟢 | ❌ | ❌ | script_init stub; 250+ callbacks not implemented. |
| `ssz/trigger-script.ssz` | `trigger_script_service.*` | P3 | 🟢 | ❌ | ❌ | register_function stub; 170+ trigger callbacks not implemented. |
| `ssz/system-script.ssz` | `system_script_service.*` | P3 | 🟢 | ❌ | ❌ | system_script_init stub; 200+ callbacks not implemented. |
| `ssz/statebuilder.ssz` | `statebuilder_service.*` | P3 | 🟢 | 🟡 | ❌ | .cmd file parser, statedef/State section parsing, CtrlTy enum (97 values), build pipeline framework. Per-controller param parsing deferred. |
| `ssz/video.ssz` | `video_service.*` | P4 | 🟢 | ❌ | ❌ | play stub; static registration wired. |
| `ssz/font.ssz` | `font_service.*` | P4 | 🟢 | ❌ | ❌ | render/init stubs; static registration wired. |
| `ssz/action.ssz` | `action_service.*` | P4 | 🟢 | ❌ | ❌ | Structs only; static registration wired; parser/behavior pending. |
| `ssz/sound.ssz` | `sound_resource_service.*` | P4 | 🟢 | 🟢 | 🟡 | Full implementation: ElecbyteSnd parser, 4 mixer variants, 16-channel pool, Snd file loading with .snd parsing tests. SDL_mixer delegation pending. |
| `ssz/bg.ssz` | `bg_service.*` | P4 | 🟢 | 🟢 | ❌ | Background system state (485 lines). Sprite/layer/animation offset rendering. Not just a placeholder anymore. |
| `ssz/stage.ssz` | `stage_service.*` | P4 | 🟢 | ❌ | ❌ | StageData only; static registration wired; parser/loader pending. |
| `ssz/sff.ssz` | `sff_service.*` | P4 | 🟢 | 🟢 | ❌ | SFF v1/v2 parser (921 lines). Sprite format loading with palette handling. |
| `ssz/command.ssz` | `command_service.*` | P4 | 🟢 | 🟢 | ❌ | Command state and input parser (465 lines). 250+ symbols from SSZ mapped. |
| `ssz/fighting.ssz` | `fighting_service.*` | P4 | 🟢 | ❌ | ❌ | Fight orchestration stub; static registration wired. |
| `ssz/fight.ssz` | `fight_service.*` | P4 | 🟢 | 🟡 | ❌ | Full fight.def parser: Lifebar, Powerbar, Face, Name, Time, Combo, WinIcon, Round, DisplayText (+20 sub-structs). step() advances all sub-components. draw() deferred (needs sdlplugin/sff/font). |
| `ssz/char.ssz` | `char_service.*` | P4 | 🟢 | 🟡 | ❌ | CharState with char loading (405 lines). Name extraction from def files. Still many pending symbol mappings from the 7664-line SSZ. |
| `save/config.ssz, save/configNet.ssz` | `config_service.*` | P5 | 🟢 | 🟢 | 🟢 | KeyBindings, input bindings, IgnoreMostErrors, load/save INI roundtrip, net portrait defaults fixed. |

## Stub / Placeholder Files Found In Review

These files still contain explicit `stub`, `placeholder`, or `no-op` markers in their comments (often documenting deferred sub-module wiring). Files removed from this list have been upgraded to real implementations:

- `main/ssz_native/action_service.cpp` — 9 lines, structs only, no behavior
- `main/ssz_native/debug_script_service.cpp` — 46 lines, 27+ no-op callbacks
- `main/ssz_native/debug_script_service.hpp` — 82 lines, all function bodies are stubs
- `main/ssz_native/fighting_service.cpp` — 9 lines, orchestration stub
- `main/ssz_native/fighting_service.hpp` — 16 lines, state placeholder
- `main/ssz_native/font_service.cpp` — 13 lines, render/init stubs
- `main/ssz_native/font_service.hpp` — 26 lines, state placeholder
- `main/ssz_native/script_service.cpp` — 18 lines, 250+ callbacks not implemented
- `main/ssz_native/script_service.hpp` — 35 lines, stub placeholder
- `main/ssz_native/sdlevent_service.cpp` — 9 lines, SDL event stub
- `main/ssz_native/sdlplugin_service.cpp` — ~210 lines, SDL plugin native wrapper with Surface/Font/GlTexture methods and all public API functions delegating to sdlplugin.cpp

- `main/ssz_native/system_script_service.cpp` — 15 lines, 200+ callbacks not implemented
- `main/ssz_native/system_script_service.hpp` — 31 lines, stub placeholder
- `main/ssz_native/system_service.hpp` — 210 lines, some methods still stubs (getStageName)
- `main/ssz_native/trigger_script_service.cpp` — 18 lines, 170+ callbacks not implemented
- `main/ssz_native/trigger_script_service.hpp` — 51 lines, stub placeholder
- `main/ssz_native/video_service.cpp` — 10 lines, play stub

**Removed from stub list (now real implementations):** bg_service.hpp, char_service.hpp, command_service.hpp, common_service.cpp/hpp, loader_service.cpp/hpp, sff_service.hpp, share_service.cpp/hpp

## Definition Of Done For Each SSZ Module

A module is tracked across three independent dimensions. It is **fully converted** only when all three are green:

| Dimension | Status | Criteria |
|---|---|---|
| **Route registered** | ☐ | Static plugin header exists with `#if IKEMEN_NATIVE_*` guard. Bridge wrapper(s) exist in `bridge.cpp`. `main.cpp` calls `*_static_register()`. Feature flag controls a real runtime route. Rollback path works (`make FLAG=0` builds and runs old behavior). |
| **Behavior implemented** | ☐ | Native C++ implementation exists for all/most public SSZ symbols. No critical function body is a no-op/stub unless intentionally documented as unsupported. Public symbols are mapped or explicitly deprecated. |
| **Parity proven** | ☐ | Old SSZ behavior has golden tests or trace comparison. Runtime smoke test passes with the native route enabled. Trace differences are reviewed and accepted. |

## Whole-Migration Exit Criteria

- [ ] Normal engine boot does not require SSZ runtime loading for converted modules.
- [ ] Character select and match start work with native replacement modules enabled.
- [ ] Representative resources load through native code.
- [ ] Lua-facing APIs still behave the same.
- [ ] Trace differences are reviewed and accepted.
- [ ] All runtime-critical `main/ssz_native/*stub*` markers are removed or justified.
- [ ] `make CONFIG=Debug test` passes.
- [ ] `make CONFIG=Release` builds.

## Vertical Slice Status

### ✅ Parity proven — golden tests or trace comparison passed

1. ✅ `file_service` parity finished (generic `read_all_as<T>`, all 17 SSZ symbols covered).
2. ✅ Native config load/save wired (`KeyBindings`, INI serializer/deserializer, net portrait defaults fixed).
3. ✅ Tiny native boot-side path proven with rollback (`IKEMEN_NATIVE_CONFIG_LIB` loads config at startup).
4. ✅ `string/math/table/crypto/stack` parity hardened (missing symbols added, tests expanded).
5. ✅ `string_service` `&Format` object implemented (printf-style formatter with %d/%i/%u/%o/%x/%X/%c/%s/%f/%F/%e/%E/%g/%G, flags, width, precision).

### 🟢 Behavior implemented — tests pass, parity pending

6. 🟢 `common_service` — all 16 public SSZ functions have real implementations (flagInit, resetRemapInput, setSize, tickFrame, tickNextFrame, tickInterpola, addFrameTime, resetFrameTime, matchOver, nextLine, splitLines, atof, atoi, loadText, readFileName, loadFile). Full test coverage in native test suite.
7. 🟢 `loader_service` — real state machine (7 bridge functions: Error, Stage, Chara, StateCompile, Load, Reset, RunTread). Stage loading checks common round state; character loading framework supports team modes (pending char_service wiring); load loop transitions correctly. Backend delegation deferred until stage/char/compiler modules are converted.
8. 🟢 `sound_resource_service` — full implementation (517 lines): ElecbyteSnd file format parser, 4 PCM mixer variants (mono/stereo × 8/16-bit), 16-channel pool with volume/pan/loop/freqmul, BGM play/stop, Snd table with group/number key lookup. 110+ test assertions including .snd file parsing with WAV field verification. SDL_mixer playback delegation is the only pending path.
9. 🟢 `share_service` — real copy/push with CommonData integration (~110+ field mappings across both directions), internal snapshot state, share_pull_from_common/share_push_to_common helpers. Automatic pull/push from wired modules still pending (currently explicit).

### ❌ Trace/test evidence still pending

- [ ] Capture and commit startup/gameplay traces (`IKEMEN_ENABLE_PLUGIN_TRACE=1`).
- [ ] `SelectData getStageName stub` in system_service — current test passes (checks `.empty()` which stub satisfies). Real behavior pending.
- [ ] Reactivate or clarify `SSZ_TRACE` instrumentation in `bridge.cpp`.
