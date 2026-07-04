# TODO_SSZ_CONVERSION.md — Reviewed And Reordered Plan

Last reviewed: **2026-07-04**

## Goal

Convert the SSZ script tree under `ssz_script/` into native C++ while keeping behavior identical. The native code must be able to replace the SSZ layer incrementally, module by module, with rollback flags and parity tests.

## Current Source Review Snapshot

- SSZ files: **45**
- SSZ script lines: **40211**
- `lib ... = <...>` imports: **151**
- `plugin index` declarations: **20**
- `main/ssz_native/` files: **74** (added `config_service.cpp`)
- Native module flags in `Makefile`: **38**
- Native service files with `stub` / `placeholder` / `no-op` markers: **30**
- Marker lines found during review: **56**

## Review Verdict

The project has made strong progress: the native ABI bridge exists, the plugin boundary is largely converted, `main/ssz_native/` has broad coverage, and every SSZ module appears to have at least a native scaffold or related service file.

However, **scaffolding is not the same as conversion**. Large areas are still placeholders or no-op stubs. The highest priority is now shifting from “create files” to “wire real behavior and prove parity.”

### Key Findings

1. **All 45 SSZ files have some native coverage**, but many are currently only DTOs, placeholders, or no-op stubs.
2. **Feature flags exist**, but most script-layer flags are not yet used to route runtime behavior. They mostly compile definitions and print in `native_manifest`.
3. **Static plugin guards are wired for plugin headers**, but gameplay/script service flags still need real runtime call-path integration.
4. **Generated review docs were missing from the uploaded ZIP**, so this review regenerated:
   - `docs/ssz_dependency_graph.txt`
   - `docs/ssz_symbol_manifest.txt`
   - `docs/native_ssz_comparison.md`
5. **Runtime traces are still not present in this ZIP**. Trace capture remains a P0 item because parity cannot be trusted without pre/post behavior comparison.
6. **The next milestone is not more scaffolding.** The next milestone is making the first native end-to-end path run without SSZ for a narrow subsystem.

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

### Static plugin headers already guarded

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

### Script-service runtime integration gap

Only a small number of native services are referenced outside `main/ssz_native/` and tests:

- `math_service.hpp` referenced by: main/math_static.hpp

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
- [ ] Stop marking scaffold-only modules as “complete”; use **Scaffolded**, **Partial**, **Parity-tested**, and **Runtime-routed** status terms.
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
- [ ] `consts.hpp`: verify every constant and sentinel value against `lib/consts.ssz`.

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
- [ ] Replace `sdlplugin_service` stub with native wrappers for surfaces, palettes, images, fonts, renderer info, BGM write, color key, and lifetime cleanup.
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
- [ ] Replace `common_service.cpp` false/no-op helpers with SSZ-equivalent logic.
- [ ] Replace `loader_service.cpp` false/no-op loader functions with real behavior.
- [ ] Replace Lua callback registration stubs with actual registrations and function bodies.
- [ ] Add golden trace tests for statebuilder before implementing it.

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

| SSZ module | Native file(s) | Priority | Current status | Next action |
|---|---|---:|---|---|
| `lib/consts.ssz` | `consts.hpp` | P1 foundation | **Mostly complete** | constexpr/type constants exist; needs parity audit against original values. |
| `lib/math.ssz` | `math_service.*` | P1 foundation | **Parity-tested** | Full SSZ parity tests for PRNG, trig, rounding, utility templates. |
| `lib/string.ssz` | `string_service.*` | P1 foundation | **Parity-tested** | All major utilities implemented and tested; `&Format` object still pending. |
| `lib/table.ssz` | `table_service.hpp` | P1 foundation | **Parity-tested** | NameTable + IntTable + intHash; operate/each_value added. |
| `lib/stack.ssz` | `stack_service.hpp` | P1 foundation | **Parity-tested** | push/pop/top/clear/empty/size all implemented. |
| `lib/base64.ssz, lib/arcfour.ssz, lib/md5.ssz` | `crypto_service.*` | P1 foundation | **Parity-tested** | Base64, Arcfour (with getByte), MD5 all tested with known-answer vectors. |
| `lib/file.ssz` | `file_service.*` | P2 plugin wrapper | **Parity-tested** | RAII FileHandle, all free functions, generic read_all_as<T> template. |
| `lib/regex.ssz` | `regex_service.*` | P2 plugin wrapper | **Mostly complete** | RAII Regex exists; needs SSZ match-shape parity tests. |
| `lib/socket.ssz` | `socket_service.*` | P2 plugin wrapper | **Partial** | RAII wrapper exists; needs mocked and live socket tests. |
| `lib/sound.ssz` | `sound_service.*` | P2 plugin wrapper | **Partial** | Thin wrapper exists; real audio smoke still pending. |
| `lib/alpha/ogg.ssz` | `ogg_service.*` | P2 plugin wrapper | **Partial** | Wrapper exists; needs sample decode/seek parity. |
| `lib/alpha/mesdialog.ssz` | `mesdialog_service.*` | P2 plugin wrapper | **Partial** | Thin wrapper exists; dialog/INI/encoding parity pending. |
| `lib/alert.ssz, thread.ssz, time.ssz, shell.ssz` | `alert/thread/time/shell_service.*` | P2 simple wrappers | **Mostly complete** | Thin wrappers exist; low risk after static/header wiring. |
| `lib/alpha/lua.ssz` | `lua_service.*` | P2 bridge | **Partial** | RAII LuaState exists; Lua callback registration parity not done. |
| `lib/alpha/sdlevent.ssz` | `sdlevent_service.*` | P2 SDL boundary | **Scaffold only** | Stub; prioritize because SDL events drive input and menus. |
| `lib/alpha/sdlplugin.ssz` | `sdlplugin_service.*` | P2 SDL boundary | **Scaffold only** | Stub; high fan-in and rendering/resource lifetime risk. |
| `lib/ssz.ssz` | `plugin/native runtime API` | P2 runtime wrapper | **Partial** | Native plugin ABI exists; script-level compiler facade still needs parity tests. |
| `ssz/share.ssz` | `share_service.*` | P3 core state | **Scaffold only** | DTO + copy/push stubs; high priority data backbone. |
| `ssz/system.ssz` | `system_service.hpp` | P3 core state | **Partial** | Some selection helpers wired; many methods still stubs. |
| `ssz/common.ssz` | `common_service.*` | P3 core state | **Scaffold only** | Many no-op functions; high priority because many modules depend on common state. |
| `ssz/loader.ssz` | `loader_service.*` | P3 core state | **Scaffold only** | No-op loader functions; must be wired before replacing runtime loading paths. |
| `ssz/debug-script.ssz` | `debug_script_service.*` | P3 Lua callbacks | **Scaffold only** | 27 no-op callbacks/loaders. |
| `ssz/script.ssz` | `script_service.*` | P3 Lua callbacks | **Scaffold only** | script_init stub; 250+ callbacks not implemented. |
| `ssz/trigger-script.ssz` | `trigger_script_service.*` | P3 Lua callbacks | **Scaffold only** | register_function stub; 170+ trigger callbacks not implemented. |
| `ssz/system-script.ssz` | `system_script_service.*` | P3 Lua callbacks | **Scaffold only** | system_script_init stub; 200+ callbacks not implemented. |
| `ssz/statebuilder.ssz` | `statebuilder_service.*` | P3 compiler/state | **Scaffold only** | Largest file; keep late, but design trace/golden tests now. |
| `ssz/video.ssz` | `video_service.*` | P4 resource/gameplay | **Scaffold only** | play stub. |
| `ssz/font.ssz` | `font_service.*` | P4 resource/gameplay | **Scaffold only** | render/init stubs. |
| `ssz/action.ssz` | `action_service.*` | P4 resource/gameplay | **Scaffold only** | Structs only; parser/behavior pending. |
| `ssz/sound.ssz` | `sound_resource_service.hpp` | P4 resource/gameplay | **Struct-only** | Resource data structs only. |
| `ssz/bg.ssz` | `bg_service.hpp` | P4 resource/gameplay | **Placeholder** | State placeholder only. |
| `ssz/stage.ssz` | `stage_service.hpp` | P4 resource/gameplay | **Struct-only** | StageData only; parser/loader pending. |
| `ssz/sff.ssz` | `sff_service.hpp` | P4 resource/gameplay | **Placeholder** | State placeholder only. |
| `ssz/command.ssz` | `command_service.hpp` | P4 resource/gameplay | **Placeholder** | State placeholder only; input parser pending. |
| `ssz/fighting.ssz` | `fighting_service.*` | P4 resource/gameplay | **Scaffold only** | fight orchestration stub. |
| `ssz/fight.ssz` | `fight_service.hpp` | P4 resource/gameplay | **Placeholder** | FightState only. |
| `ssz/char.ssz` | `char_service.hpp` | P4 resource/gameplay | **Placeholder** | CharState only; largest gameplay risk. |
| `save/config.ssz, save/configNet.ssz` | `config_service.hpp, config_service.cpp, config_net_service.hpp` | P5 config | **Parity-tested** | KeyBindings, input bindings, IgnoreMostErrors, load/save INI roundtrip, net portrait defaults fixed. |

## Stub / Placeholder Files Found In Review

These files still contain explicit `stub`, `placeholder`, or `no-op` markers and should not be considered functionally converted:

- `main/ssz_native/action_service.cpp`
- `main/ssz_native/bg_service.hpp`
- `main/ssz_native/char_service.hpp`
- `main/ssz_native/command_service.hpp`
- `main/ssz_native/common_service.cpp`
- `main/ssz_native/common_service.hpp`
- `main/ssz_native/debug_script_service.cpp`
- `main/ssz_native/debug_script_service.hpp`
- `main/ssz_native/fight_service.hpp`
- `main/ssz_native/fighting_service.cpp`
- `main/ssz_native/fighting_service.hpp`
- `main/ssz_native/font_service.cpp`
- `main/ssz_native/font_service.hpp`
- `main/ssz_native/loader_service.cpp`
- `main/ssz_native/loader_service.hpp`
- `main/ssz_native/script_service.cpp`
- `main/ssz_native/script_service.hpp`
- `main/ssz_native/sdlevent_service.cpp`
- `main/ssz_native/sdlplugin_service.cpp`
- `main/ssz_native/sff_service.hpp`
- `main/ssz_native/share_service.cpp`
- `main/ssz_native/share_service.hpp`
- `main/ssz_native/statebuilder_service.cpp`
- `main/ssz_native/statebuilder_service.hpp`
- `main/ssz_native/system_script_service.cpp`
- `main/ssz_native/system_script_service.hpp`
- `main/ssz_native/system_service.hpp`
- `main/ssz_native/trigger_script_service.cpp`
- `main/ssz_native/trigger_script_service.hpp`
- `main/ssz_native/video_service.cpp`

## Definition Of Done For Each SSZ Module

A module is **not converted** merely because a native file exists. It is converted only when all of these are true:

- [ ] Native C++ implementation exists.
- [ ] Public symbols are mapped or explicitly deprecated.
- [ ] Old SSZ behavior has golden tests or trace comparison.
- [ ] Module feature flag controls a real runtime route.
- [ ] Rollback path works.
- [ ] Runtime smoke test passes with the native route enabled.
- [ ] No critical function body is a no-op/stub unless intentionally documented as unsupported.

## Whole-Migration Exit Criteria

- [ ] Normal engine boot does not require SSZ runtime loading for converted modules.
- [ ] Character select and match start work with native replacement modules enabled.
- [ ] Representative resources load through native code.
- [ ] Lua-facing APIs still behave the same.
- [ ] Trace differences are reviewed and accepted.
- [ ] All runtime-critical `main/ssz_native/*stub*` markers are removed or justified.
- [ ] `make CONFIG=Debug test` passes.
- [ ] `make CONFIG=Release` builds.

The first vertical slice is complete:

1. ✅ `file_service` parity finished (generic `read_all_as<T>`, all 17 SSZ symbols covered).
2. ✅ Native config load/save wired (`KeyBindings`, INI serializer/deserializer, net portrait defaults fixed).
3. ✅ Tiny native boot-side path proven with rollback (`IKEMEN_NATIVE_CONFIG_LIB` loads config at startup).
4. ✅ `string/math/table/crypto/stack` parity hardened (missing symbols added, tests expanded).
5. ✅ `string_service` `&Format` object implemented (printf-style formatter with %d/%i/%u/%o/%x/%X/%c/%s/%f/%F/%e/%E/%g/%G, flags, width, precision).
6. ✅ `share_service` real copy/push with CommonData integration (~110+ field mappings), internal snapshot state, and module integration helpers.
7. ✅ `share_static.hpp` — native share plugin registration guarded by `IKEMEN_NATIVE_SHARE_LIB`, wired into `main.cpp` bootstrap, with rollback verified.
8. ✅ `common_static.hpp` — native common plugin registration guarded by `IKEMEN_NATIVE_COMMON_LIB` (12 bridge functions: FlagInit, ResetRemapInput, SetSize, TickFrame, TickNextFrame, TickInterpola, AddFrameTime, ResetFrameTime, MatchOver, Atoi, Atof, LoadText), wired into `main.cpp` bootstrap.
9. ✅ `loader_static.hpp` — native loader plugin registration guarded by `IKEMEN_NATIVE_LOADER_LIB` (7 bridge functions: Error, Stage, Chara, StateCompile, Load, Reset, RunTread), wired into `main.cpp` bootstrap.
10. ✅ `system_static.hpp` — native system plugin registration guarded by `IKEMEN_NATIVE_SYSTEM_LIB` (7 bridge functions: AddChar, AddStage, GetStageName, SetStageNo, SelectStage, AddSelchr, SelReset), wired into `main.cpp` bootstrap.
11. ✅ `debug_script_static.hpp` — native debug_script plugin registration guarded by `IKEMEN_NATIVE_DEBUG_SCRIPT_LIB` (2 bridge functions: DebugLoadFile, DebugRunFile), wired into `main.cpp` bootstrap.
12. ✅ `script_static.hpp` — native script plugin registration guarded by `IKEMEN_NATIVE_SCRIPT_LIB` (1 bridge function: ScriptInit), wired into `main.cpp` bootstrap.

## Immediate Next Step

1. ✅ Wire `IKEMEN_NATIVE_FILE_LIB` to an actual script-layer replacement route (not only static plugin wrappers).
2. ✅ Verify rollback: `make IKEMEN_NATIVE_FILE_LIB=0 IKEMEN_NATIVE_CONFIG_LIB=0` still builds and runs old behavior.
3. Capture and commit startup/gameplay traces (`IKEMEN_ENABLE_PLUGIN_TRACE=1`).
4. ✅ Implement `string_service` `&Format` object (printf-style formatter, ~300 lines of SSZ).
5. ✅ Begin P3: `share_service` real `copy()`/`push()` state transfer.
6. ✅ Wire `IKEMEN_NATIVE_SHARE_LIB` to an actual script-layer replacement route (share static registration, bridge wrappers, main.cpp bootstrap).
7. ✅ Wire `IKEMEN_NATIVE_COMMON_LIB` to an actual script-layer replacement route (common static registration, bridge wrappers, main.cpp bootstrap).
8. ✅ Wire `IKEMEN_NATIVE_LOADER_LIB` to an actual script-layer replacement route (loader static registration, bridge wrappers, main.cpp bootstrap).
9. ✅ Wire `IKEMEN_NATIVE_SYSTEM_LIB` to an actual script-layer replacement route (system static registration, bridge wrappers, main.cpp bootstrap).
10. ✅ Wire `IKEMEN_NATIVE_DEBUG_SCRIPT_LIB` to an actual script-layer replacement route (debug_script static registration, bridge wrappers, main.cpp bootstrap).
11. ✅ Wire `IKEMEN_NATIVE_SCRIPT_LIB` to an actual script-layer replacement route (script static registration, bridge wrappers, main.cpp bootstrap).
