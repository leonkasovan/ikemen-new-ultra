# Input Processing Review

> Revised after source verification against `main/sdlplugin/sdlplugin.cpp`,
> `ssz_script/ssz/command.ssz`, `ssz_script/lib/alpha/sdlevent.ssz`, and
> callers. One new Critical issue was found that the first draft missed
> (`JoystickButtonState` argument order); two claims were wrong (#3 text-input
> breakage, #8 race); several severities adjusted.

## Architecture Overview

```
Hardware Layer (C++)
├── SDL Keyboard State    → SDL_GetKeyboardState()
├── SDL Joystick Buttons  → SDL_JoystickGetButton()
├── SDL Joystick Axes     → SDL_JoystickGetAxis() (threshold ±3200)
├── SDL Joystick Hats     → SDL_JoystickGetHat()
├── Win32 WM_KEYDOWN      → wrapProc() → TranslateMessage → g_newChar
└── Joystick class        → Joystick::getState(joy, btn)

SSZ Script Layer
├── lib/alpha/sdlevent.ssz   → PollEvent() loop, Key state tracking, hotkeys
├── ssz/command.ssz          → CommandList, Buffer, KeyBuffer, KeyInfo,
│                              modKeyState, PollInputBitmask call ([OPT3])
├── ssz/script.ssz           → Lua bridge (commandGetState, commandInput, …)
├── ssz/common.ssz           → addHotkey(), inputRemap[], eventKeyHash()
└── ssz/system-script.ssz    → remapInput(), setInputConfig(), pad scanning

Lua Layer
├── match.lua::loop()        → calls cmdInput(), commandGetState()
├── main.lua                 → addHotkey() for debug controls
└── common.lua               → shared menu navigation
```

## Data Flow (Per Frame)

```
1. se.event(fps)
   └── PollEvent() → SDL_PollEvent → g_lastChar / eventKeys
       └── Dispatches: KEYDOWN → check hotkeys + key flags
       └── Clears per-frame flags (esc, upKey, etc.)

2. cmd.update()
   ├── NetInput.update()      [network mode]
   ├── FileInput.update()     [replay mode]
   ├── PlaybackInput.update() [record/playback]
   └── se.event(60)           [normal mode]

3. cmd.input(input, facing)  [per player]
   ├── if input < 0: AI → aiil[input].x()
   ├── if input >= 0:
   │   ├── [OPT3] PollInputBitmask(jn, u,d,l,r, a..s, jn2, u2..s2, sec)
   │   │   → single FFI call, 14-bit mask per pad set
   │   │   (legacy path: individual JoystickButtonState calls — see issue #1)
   │   ├── modKeyState()  → Ctrl-combo suppression logic
   │   └── Buffer.input(B, D, F, U, a,b,c,x,y,z,q,w,e,s)

4. cmd.step(facing, ai, hitpause, buftime)
   └── Command.match() → input command sequences → commandGetState()
```

## Key Files

| File | Purpose |
|------|---------|
| `main/sdlplugin/sdlplugin.cpp:405-435` | wrapProc (Win32 hook), winProcInit |
| `main/sdlplugin/sdlplugin.cpp:480-520` | Joystick class, getState (axis/hat/button) |
| `main/sdlplugin/sdlplugin.cpp:594-626` | sndjoyinit: joystick init skipped |
| `main/sdlplugin/sdlplugin.cpp:1538/1674/1778` | RendererInit: SDL_Init(SDL_INIT_VIDEO) only |
| `main/sdlplugin/sdlplugin.cpp:2454-2519` | PollEvent, GetLastChar, KeyState, JoystickButtonState |
| `main/sdlplugin/sdlplugin.cpp:2526-2571` | PollInputBitmask ([OPT3] batched poll) |
| `ssz_script/lib/alpha/sdlplugin.ssz:57-74` | FFI declarations incl. 31-param bitmask |
| `ssz_script/lib/alpha/sdlevent.ssz` | Event loop, key flags, hotkey dispatch |
| `ssz_script/ssz/command.ssz:243-263` | modKeyState (Ctrl suppression) |
| `ssz_script/ssz/command.ssz:297-302,1371-1385` | Input config offset, bitmask call, legacy poll block |
| `ssz_script/ssz/common.ssz:105,178` | sysControls, inputRemap |
| `ssz_script/ssz/system-script.ssz:921-955` | Pad-config button scanning |

## Issues Found

### 🔴 Critical

#### 1. `JoystickButtonState` argument order mismatched with every SSZ caller

**Files:** `main/sdlplugin/sdlplugin.cpp:2517-2519` (C++),
`ssz_script/ssz/command.ssz:245,256,1371-1385,1516,1532-1565`,
`ssz_script/ssz/system-script.ssz:921-955`

```cpp
// C++: parameters are (btn, joy), swapped into getState(joy, btn)
bool SSZ_STDCALL JoystickButtonState(int32_t btn, int32_t joy)
{
    return g_js.getState(joy, btn);
}
```

But **every SSZ caller passes device-first**:

```ssz
.sdl.JoystickButtonState(:jn, .cfg.in[in].l:)          // command.ssz:1371+
.sdl.JoystickButtonState(:cn, i:)                      // system-script.ssz:921 (pad config scan)
bool ctrl = .sdl.JoystickButtonState(:-1, 224:) || ... // command.ssz:245 (intends getState(-1, LCTRL))
```

The bridge maps positionally, so C++ receives `btn = <device>, joy = <scancode>`
and looks up the **wrong device index**. For keyboard (`jn = -1`) the result is a
joystick lookup on index `<scancode>` → always false; the intended
`getState(-1, <scancode>)` keyboard path is unreachable through this function.

Why nothing visibly breaks today:
- The hot fight path uses `PollInputBitmask` ([OPT3], sdlplugin.cpp:2526-2571),
  which orders its own args correctly (`g_js.getState(jn, u)`).
- Joysticks are disabled entirely (issue #2), so joystick-indexed lookups are
  false regardless.

What IS silently broken right now:
- `modKeyState`'s Ctrl detection (command.ssz:245) → always false → the
  Ctrl+C/V/S/P/R/L debug-suppression never triggers (see issue #4).
- The legacy individual-poll block (command.ssz:1371-1385) and start/any-button
  checks (:1516, :1532-1565) read all-false from their primary term.
- Pad-config scanning (system-script.ssz:921+) would never detect buttons even
  after joysticks are re-enabled.

**Fix (one line):** change the C++ signature to `(int32_t joy, int32_t btn)`
to match every caller. No SSZ changes needed. Must land before/independently
of issue #2's fix — otherwise re-enabling joysticks activates these paths in
broken form.

#### 2. Joystick support fully disabled (init + subsystem flag)

**File:** `main/sdlplugin/sdlplugin.cpp:625` and RendererInit SDL_Init sites

```cpp
// g_js.init();  // Disabled - requires SDL_INIT_JOYSTICK which can hang on Windows
```

Startup initializes `SDL_INIT_VIDEO` only (sdlplugin.cpp:1538/1674/1778);
`SDL_INIT_JOYSTICK` is never set, and `g_js.init()` is commented out. Result:
`SDL_NumJoysticks()` is 0 and `Joystick::getState(joy >= 0, ...)` always false.
Gamepad/controller input is non-functional; only keyboard works (`joy == -1`
via `SDL_GetKeyboardState`).

This is a **deliberate workaround** (INIT_LOG documents the hang), not an
overlooked bug — severity reflects missing feature, not regression. Re-enabling
requires both the subsystem flag and `g_js.init()` together, plus issue #1's
fix, ideally via a config-gated try/catch-style guard or `SDL_INIT_GAMECONTROLLER`.

### 🟡 Medium

#### 3. `modKeyState` Ctrl-suppression is currently dead code; scancode map undocumented

**File:** `ssz_script/ssz/command.ssz:243-263`

The double-negation design is intentional and correct as written: called as
`!.modKeyState(pressed, jn, key)`, it returns true (= "suppress") both when the
key isn't pressed and when a Ctrl-combo should be eaten by debug hotkeys during
a fight.

However:
- It never actually suppresses anything because its `ctrl` detection is dead
  (issue #1).
- The magic numbers are SDL scancodes: **6=C, 15=L, 19=P, 21=R, 22=S, 25=V**
  (plus 224=LCTRL, 228=RCTRL). Most gate on `gameState == 1` (fight), V on
  `gameState == 0`. Document these — do not guess the letter mapping.

Once #1 lands, verify the suppression behaves as intended in-match.

### 🟢 Low

#### 4. Double `TranslateMessage` in wrapProc posts duplicate WM_CHAR

**File:** `main/sdlplugin/sdlplugin.cpp:409-422`

```cpp
TranslateMessage(&m);              // posts WM_CHAR (for character keys)
if(TranslateMessage(&m)            // posts ANOTHER WM_CHAR, returns TRUE
    && PeekMessage(&m, hWnd, WM_CHAR, WM_CHAR, PM_REMOVE))
{
    g_newChar = m.wParam;
}
```

Contrary to the first draft: `g_newChar` **does** get set reliably — each
`TranslateMessage` *posts* a WM_CHAR and the `PeekMessage(PM_REMOVE)` removes
the first. The real effect of the duplicate is a second WM_CHAR left in the
queue per character keypress (dispatched later to SDL's wndproc; harmless
today since nothing consumes it, but it would double-fire text input if
anything ever does). Dead-key state can also be translated twice.

**Fix:** delete the first bare `TranslateMessage(&m);` line. Behavior unchanged,
queue clean.

#### 5. `inputRemap` mixes Lua 1-based and internal 0-based indices

**Files:** `ssz_script/ssz/common.ssz:178`, `ssz_script/ssz/system-script.ssz` (remap setters)

`.inputRemap.new(.maxSimul*2)` internally vs `inputRemap[pn-1]+1.0` /
`src-1, dest-1` conversions at the Lua boundary; the sentinel `.com.IERR`
(playback-mode input kill) must survive every round trip. Works, but fragile —
worth an assert or named helpers if touched.

#### 6. `Buffer::input()` sign-based edge tracking is undocumented

**File:** `ssz_script/ssz/command.ssz:53-65`

Sign of each direction field tracks previous-held state; on transition the
count resets and sign flips; `keyState()` blocks Back while Up/Down held
(no simultaneous up+back). Inherited Ikemen design, functions correctly —
documentation nit only.

#### 7. `sysControls` offsets config indices with no bounds check

**Files:** `ssz_script/ssz/common.ssz:105`, `ssz_script/ssz/command.ssz:297`

`pn = in + .com.sysControls` shifts fight controls (0-1 base) to menu controls
(10-11) MUGEN-style. Nothing validates `pn` against `cfg.in`'s length; safe
only while the config array keeps ≥12 entries.

#### 8. Hotkey execution compiles Lua on every press

**File:** `ssz_script/ssz/fighting.ssz` (hotkey dispatch loop)

`runString()` compiles + executes on each hotkey down-event, no caching.
Fine at current scale (fires on discrete keydown only).

#### 9. `PollInputBitmask` takes 31 positional params

**Files:** `main/sdlplugin/sdlplugin.cpp:2526-2536`, `ssz_script/lib/alpha/sdlplugin.ssz:73-74`

Confirmed exactly 31 (jn + 14 keys + jn2 + 14 keys + sec). Deliberate FFI
optimization avoiding struct marshaling (~27 round-trips saved per player per
frame). Fragile but justified; keep the SSZ wrapper as the single construction
site.

#### 10. `eventUpdate` per-letter key boilerplate

**File:** `ssz_script/lib/alpha/sdlevent.ssz:69-120`

Each key has its own case; could be table-driven. Cosmetic.

#### 11. Hardcoded ±3200 axis threshold

**File:** `main/sdlplugin/sdlplugin.cpp:506-510`

~10% of ±32768 range. Historical constant, fine for most pads; candidates for
config exposure only if real controllers misbehave.

## Verified Non-Issues

- **`g_newChar` timing is not a race.** `wrapProc` runs on the same thread as
  `PollEvent`: SDL pumps Win32 messages synchronously inside `SDL_PollEvent` on
  the caller's thread (sdlplugin.cpp:2454-2474). Worst case is a designed ≤1-frame
  delay (`g_lastChar = g_newChar` per poll, cleared when the queue drains), not
  cross-thread contention.

## Summary

| # | Issue | Severity | Status | Impact |
|---|-------|----------|--------|--------|
| 1 | `JoystickButtonState` arg-order mismatch | 🔴 Critical | ✅ Fixed | All individual polls query wrong device; Ctrl-suppression + pad-config dead |
| 2 | Joystick init + subsystem disabled | 🔴 Critical* | ✅ Fixed | No gamepad support (deliberate hang workaround) |
| 3 | modKeyState undocumented scancodes | 🟡 Medium | ✅ Fixed | Debug-combo suppression never fires |
| 4 | Double TranslateMessage → stray WM_CHAR | 🟢 Low | ✅ Fixed | Harmless queue cruft; would double-fire text input if consumed |
| 5 | inputRemap 1-based/0-based mixing | 🟢 Low | — | Works; fragile conversions |
| 6 | Buffer sign-tracking undocumented | 🟢 Low | — | Documentation only |
| 7 | sysControls offset unchecked | 🟢 Low | ✅ Fixed | Safe while cfg.in stays ≥12 entries |
| 8 | Hotkey runString uncached | 🟢 Low | — | Acceptable |
| 9 | PollInputBitmask 31 params | 🟢 Low | — | Justified FFI tradeoff |
| 10 | eventUpdate boilerplate | 🟢 Low | — | Cosmetic |
| 11 | Axis threshold hardcoded | 🟢 Low | — | Fine for most pads |

\* feature gap by intent, not unnoticed breakage.

## Implemented Fixes (2026-08-24)

### 1. 🔴 Critical: `JoystickButtonState` parameter order

**Files:** `main/sdlplugin/sdlplugin.cpp`, `main/ssz/bridge.cpp`, `main/ssz/old_bridge.cpp`

Swapped C++ signature from `(int32_t btn, int32_t joy)` to `(int32_t joy, int32_t btn)`.
No SSZ changes needed — all callers already pass `(jn, key)`. Fixes:
- `modKeyState` Ctrl detection (was always false for keyboard)
- Pad-config button scanning in `system-script.ssz`
- Legacy individual-poll block in `command.ssz`

### 2. 🟢 Low: Delete duplicate `TranslateMessage`

**File:** `main/sdlplugin/sdlplugin.cpp:405-422`

Removed the first bare `TranslateMessage(&m)` in `wrapProc`. The second one
(with `PeekMessage`) was always the one that worked. Eliminates stray WM_CHAR
posts per keypress.

### 3. 🟡 Medium: Document `modKeyState` scancodes

**File:** `ssz_script/ssz/command.ssz:243-263`

Added comment: `6=C, 15=L, 19=P, 21=R, 22=S, 25=V, 224=LCTRL, 228=RCTRL`.
Also added inline comment explaining the Ctrl+C/V/S/P/R/L suppression logic.

### 4. 🟢 Low: Bounds check for `sysControls` offset

**File:** `ssz_script/ssz/command.ssz:297`

Added `if(pn < 0 || pn >= #.cfg.in) ret false;` after computing `pn = in + sysControls`.
Prevents out-of-bounds access if config array is shorter than expected.

### 5. 🔴 Critical: Joystick support re-enabled behind config flag

**Files:** `main/sdlplugin/sdlplugin.cpp`, `main/ssz/bridge.cpp`,
`main/sdlplugin_static.hpp`, `ssz_script/lib/alpha/sdlplugin.ssz`, `ssz_script/ssz/ikemen.ssz`,
`ssz_script/save/config.ssz`, `ssz_script/save/configNet.ssz`

Added `const bool UseJoystick = false;` to config files. Added `EnableJoystick(bool)`
C++ function that calls `SDL_InitSubSystem(SDL_INIT_JOYSTICK)` + `g_js.init()` (enable)
or `g_js.close()` + `SDL_QuitSubSystem(SDL_INIT_JOYSTICK)` (disable).
Called from `ikemen.ssz` after renderer init. Default `false` — set to `true` in
config.ssz to enable gamepad support.

## Remaining Recommendations

1. **Cache compiled hotkey scripts** — compile once at registration if
   hotkey count grows.
