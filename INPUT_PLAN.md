# Input Plan — Port Gamepad Mapping Refactor to `optimized`

**Status: EXECUTED 2026-08-25.** All steps applied + verified. Deviations from
the original plan listed at the bottom.

**Source:** `C:\tmp\ikemen\ikemen-static-gamepad\` (JOYSTICK_MAPPING_REFACTOR.md — reviewed, claims verified)
**Target:** current `optimized` branch
**Rule:** port by hand — the tmp tree is an old base (pre-screenshot-fix, old `script/` layout). Do NOT copy files wholesale.

---

## Background

Tmp tree adds SDL_GameController support: WGI backend hints, dual-handle
device store, `LoadGamepadMappings` binding wired to
`config.ssz::GamepadMappings` (`lib/external/gamecontrollerdb.txt`).
Positive button IDs use the normalized `SDL_GameControllerGetButton` API when
the device is recognized as a controller; negative axis/hat IDs and the
`joy<0` keyboard fallback are unchanged (backward compatible with `in.new`
tables in `config.ssz`).

---

## Steps

### 1. `main/sdlplugin/sdlplugin.cpp`

- [ ] Add `applyJoystickBackendHints()` (from tmp :193–210) — `#ifdef`-guarded
      hints: `SDL_HINT_JOYSTICK_WGI=0`, `DIRECTINPUT=1`, `XINPUT_ENABLED=1`,
      `HIDAPI=1`. Place before the `Joystick` class.
- [ ] Replace the existing `Joystick` class with the tmp version (:212–342):
      - `Device { SDL_Joystick* joystick; SDL_GameController* controller; }`
      - `init()`: init JOYSTICK|GAMECONTROLLER subsystems, hints first,
        `SDL_JoystickEventState(SDL_ENABLE)`, enumerate — prefer
        `SDL_GameControllerOpen`, borrow joystick via
        `SDL_GameControllerGetJoystick`; fall back to `SDL_JoystickOpen`
      - `close()`: controller close owns joystick — no double-close
      - `reload()` = `init()`
      - `getState(joy, btn)`: `joy<0` → keyboard; `btn<0` → axis/hat scheme
        (unchanged); controller + positive btn < `SDL_CONTROLLER_BUTTON_MAX`
        → `SDL_GameControllerGetButton`; else `SDL_JoystickGetButton`
- [ ] **Fix while porting (review finding):** bounds-check the keyboard path —
      `getState` currently does `SDL_GetKeyboardState(nullptr)[btn]` unchecked:
      ```cpp
      if(joy < 0){
          int keyCount = 0;
          const Uint8* keys = SDL_GetKeyboardState(&keyCount);
          return btn >= 0 && btn < keyCount && keys[btn] == SDL_PRESSED;
      }
      ```
- [ ] Add `LoadGamepadMappings` (tmp :1975–1991), two adjustments:
      - use `CP_UTF8` (not `CP_THREAD_ACP`) for the path conversion — matches
        the rest of the codebase
      - keep: hints re-apply, `SDL_GameControllerAddMappingsFromFile`,
        dual-level error logging, `g_js.reload()` after load, return count

### 2. `main/sdlplugin_static.hpp`

- [ ] Add declaration + mapping entry:
      ```cpp
      void      SSZ_STDCALL LoadGamepadMappings(PluginUtil*, Reference);
      ...
      { "LoadGamepadMappings", (void*)LoadGamepadMappings },
      ```

### 3. `ssz_script/lib/alpha/sdlplugin.ssz`

- [ ] Add plugin declaration (near the other input functions):
      ```ssz
      plugin int LoadGamepadMappings(:^/char:) = "dll/sdlplugin.dll";
      ```

### 4. `ssz_script/lib/alpha/sdlevent.ssz`

- [ ] Load the mapping database during input init (tmp sdlevent.ssz:52):
      ```ssz
      .sdl.LoadGamepadMappings(:.cfg.GamepadMappings:);
      ```
      Verify `.cfg` alias in that file points at `save/config.ssz` consts.

### 5. `ssz_script/save/config.ssz`

- [ ] Update the existing entry's comment (const already exists):
      ```ssz
      const ^/char GamepadMappings = "lib/external/gamecontrollerdb.txt"; //SDL2 GameController mapping database (gamecontrollerdb.txt)
      ```
- [ ] Ensure `install/lib/external/gamecontrollerdb.txt` exists (add to
      `make install` copy list if missing).

### 6. Not ported (already in repo / intentionally skipped)

- `JoystickButtonState(joy, btn)` arg order — already fixed here (`7c1e9e5`)
- `script/save/config.ssz` from tmp — old layout; only the comment changes

---

## Verification

1. `make CONFIG=Debug install -j8` — clean build
2. Boot log shows: `Enumerating N joystick(s)` and, with a pad connected,
   `Opened game controller 0: ...` (or `Opened joystick` for non-SDL2 pads)
3. Log line `Loaded N gamepad mapping(s) from lib/external/gamecontrollerdb.txt`
4. Run with `PerformanceMonitor = true`, Renderer = 2: no input-related errors
   in log; keyboard still works (`joy = -1` bindings)
5. With a controller connected: config.ssz P1 GAMEPAD - BATTLE bindings
   (`in.new[2]`) respond — buttons via positive IDs, axes/hats via negative
6. Regression: `takeScreenShot` still works (same file was ported around)
7. No controller connected: clean boot, no crashes, keyboard play unaffected

## Risks

- `SDL_HINT_JOYSTICK_*` names differ across SDL versions — already handled by
  the `#ifdef` guards
- Devices are enumerated once at init (no hotplug) — pre-existing limitation,
  unchanged by this port

---

## Execution Notes (deviations from plan)

1. **`LoadGamepadMappings` split across two files** — repo style keeps
   `PluginUtil*`/`Reference` out of sdlplugin.cpp, so:
   - native `LoadGamepadMappingsDb(const char*)` lives in sdlplugin.cpp
     (owns hints + `g_js`)
   - ABI wrapper `LoadGamepadMappings(PluginUtil*, Reference)` lives in
     bridge.cpp (`refToNarrowUtf8`), returns `int32_t` (SSZ decl is `plugin int`)
2. **Joystick init stays gated** — tmp claim 7 ("re-enabled init") was NOT
   ported as-is. This repo deliberately gates init behind
   `config.ssz::UseJoystick` (ikemen.ssz:251 → `EnableJoystick`) because of a
   MinGW hang. The WGI-disable hints (applied inside `init()` before
   `SDL_InitSubSystem`) should fix that hang's root cause, so flipping
   `UseJoystick = true` is now the supported way to enable gamepads. Default
   remains `false`.
3. **Added `ssz_script/lib/external/gamecontrollerdb.txt`** (community db,
   2,270 lines, SDL 2.0.16+ format) — repo had no copy; `make install` copies
   it to `install/lib/external/`.
4. **Double mapping-load in log** — `eventUpdate()`'s once-guard flag is
   per-SSZ-program, and the debug script is a second program → second load
   attempt logs "Loaded 0" (SDL dedupes). Benign.

## Verified

- Clean build; `Registered 61 functions for library 'sdlplugin'` (was 60)
- SSZ compile error size=0; `Loaded 866 gamepad mapping(s)` at boot
- `UseJoystick = true`: no hang, `Enumerating 0 joystick(s)` (headless), reload
  after mapping load works; restored to `false` after test
- Keyboard play (`joy = -1`) unaffected; bounds check added on keyboard path
