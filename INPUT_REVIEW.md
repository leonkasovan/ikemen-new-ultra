# Input Processing Review

## Architecture Overview

```
Hardware Layer (C++)
├── SDL Keyboard State    → SDL_GetKeyboardState()
├── SDL Joystick Buttons  → SDL_JoystickGetButton()
├── SDL Joystick Axes     → SDL_JoystickGetAxis() (threshold ±3200)
├── SDL Joystick Hats     → SDL_JoystickGetHat()
├── Win32 WM_KEYDOWN      → g_newChar (text input via TranslateMessage)
└── Joystick class        → Joystick::getState(joy, btn)

SSZ Script Layer
├── lib/alpha/sdlevent.ssz   → PollEvent() loop, Key state tracking, hotkeys
├── ssz/command.ssz          → CommandList, Buffer, KeyBuffer, KeyInfo
├── ssz/script.ssz           → Lua bridge (commandGetState, commandInput, etc.)
├── ssz/common.ssz           → addHotkey(), inputRemap[], eventKeyHash()
└── ssz/system-script.ssz    → remapInput(), setInputConfig()

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
   ├── NetInput.update()     [network mode]
   ├── FileInput.update()    [replay mode]
   ├── PlaybackInput.update() [record/playback]
   └── se.event(60)          [normal mode]

3. cmd.input(input, facing)  [per player]
   ├── if input < 0: AI → aiil[input].x()
   ├── if input >= 0: Hardware poll
   │   ├── cfg.in[in].jn  → keyboard/joystick device number
   │   ├── cfg.in[in].u/d/l/r → button mappings
   │   ├── cfg.in[in+2].* → secondary pad mappings
   │   ├── modKeyState()  → ctrl+key modifier logic
   │   └── JoystickButtonState(jn, btn) → SDL poll
   └── Buffer.input(B, D, F, U, a,b,c,x,y,z,q,w,e,s)

4. cmd.step(facing, ai, hitpause, buftime)
   └── Command.match() → input command sequences → commandGetState()
```

## Key Files

| File | Purpose |
|------|---------|
| `main/sdlplugin/sdlplugin.cpp` | C++ hardware input: Joystick class, PollEvent, KeyState, JoystickButtonState, PollInputBitmask |
| `main/ssz/bridge.cpp` | SSZ↔C++ bridge declarations |
| `ssz_script/lib/alpha/sdlplugin.ssz` | SSZ plugin declarations (PollInputBitmask FFI) |
| `ssz_script/lib/alpha/sdlevent.ssz` | Event loop, key flags, hotkey dispatch, frame timing |
| `ssz_script/ssz/command.ssz` | Buffer, KeyInfo, KeyBuffer, CommandList, Command input/match |
| `ssz_script/ssz/script.ssz` | Lua bridge: commandGetState, commandInput, startButton |
| `ssz_script/ssz/common.ssz` | addHotkey(), inputRemap[], eventKeyHash() |
| `ssz_script/ssz/system-script.ssz` | remapInput(), setInputConfig(), setInputDisplay() |
| `ssz_script/ssz/fighting.ssz` | Match loop hotkey execution, round control |

## Issues Found

### 🔴 Critical

#### 1. `Joystick::init()` is completely disabled

**File:** `main/sdlplugin/sdlplugin.cpp:625`

```cpp
// g_js.init();  // Disabled - requires SDL_INIT_JOYSTICK which can hang on Windows
```

`g_js` has **zero joysticks**. Every `JoystickButtonState(joy, btn)` call where `joy >= 0`
returns `false`. **Gamepad/controller input is non-functional.** Only keyboard input works
(via `joy == -1` → `SDL_GetKeyboardState`).

#### 2. `Joystick::init()` never called — `SDL_INIT_JOYSTICK` not initialized

**File:** `main/sdlplugin/sdlplugin.cpp:621`

```cpp
INIT_LOG("Skipping joystick initialization (can hang on Windows)");
INIT_LOG("Skipping joystick enumeration (SDL_INIT_JOYSTICK not enabled)");
```

Even if `g_js.init()` were uncommented, `SDL_NumJoysticks()` would return 0 because
`SDL_INIT_JOYSTICK` was never passed to `SDL_Init`. Both the init subsystem flag and
`g_js.init()` must be re-enabled together.

#### 3. `wrapProc` has double `TranslateMessage` — breaks text input

**File:** `main/sdlplugin/sdlplugin.cpp:413-416`

```cpp
TranslateMessage(&m);          // ← first call
if(
    TranslateMessage(&m)       // ← second call (duplicate!)
    && PeekMessage(&m, hWnd, WM_CHAR, WM_CHAR, PM_REMOVE))
{
    g_newChar = m.wParam;
}
```

`TranslateMessage` is called twice on the same `WM_KEYDOWN`. The first call may generate
the `WM_CHAR` message; the second call is redundant. The `PeekMessage` then tries to
read the `WM_CHAR`, but it may already be consumed. **Net effect:** `g_newChar` may
never be set properly, breaking text input in dialogs (`InputStr`).

**Correct pattern:**
```cpp
TranslateMessage(&m);
if (PeekMessage(&m, hWnd, WM_CHAR, WM_CHAR, PM_REMOVE))
    g_newChar = m.wParam;
```

### 🟡 Structural Issues

#### 4. `modKeyState()` uses double-negation — confusing but intentional

**File:** `ssz_script/ssz/command.ssz:243-270`

```ssz
public bool modKeyState(bool keyState, int jn, int key)
{
  bool ctrl = .sdl.JoystickButtonState(:-1, 224:) || .sdl.JoystickButtonState(:-1, 228:);
  branch{
  cond !keyState: ret true;    // key NOT pressed → returns true
  else:
    branch{
    cond !.str.equ(.com.gameMode,"practice") && .com.gameState == 1:
      ret false;               // during fight, modifier kills input
    cond jn > -1:
      ret false;               // joystick → no modifier support
    cond key != 6 && key != 15 && key != 19 && key != 21 && key != 22 && key != 25:
      ret false;               // only D,S,X,A,C,V allowed
    cond !(ctrl && ...):
      ret false;
    else: ret true;
    }
  }
}
```

Called as `!.modKeyState(JoystickButtonState(jn, btn), jn, btn)` — note the **negation**.

- Key IS pressed (`true`) + allowed modifier → `modKeyState=true` → `!true=false` → input suppressed
- Key NOT pressed → `modKeyState=true` → `!true=false` → input suppressed

**Effect:** During fight mode, pressing Ctrl+D/Ctrl+S/Ctrl+X/Ctrl+A/Ctrl+C/Ctrl+V
suppresses those keys from game input. Purpose: prevent debug shortcuts from triggering
game actions. The magic numbers (6, 15, 19, 21, 22, 25) are SDL scancodes with no documentation.

#### 5. `inputRemap` uses 1-based Lua ↔ 0-based internal conversion

**Files:** `ssz_script/ssz/common.ssz:178`, `ssz_script/ssz/system-script.ssz:2060,2076`

```ssz
.inputRemap.new(.maxSimul*2);                              // 0-indexed

L.pushNumber((double).com.inputRemap[pn-1]+1.0);           // Lua 1-based
.com.inputRemap[src-1] = dest-1;                           // Lua→internal
```

The special value `.com.IERR` (used in playback mode to kill human input) must be
carefully handled in all conversions. Works but error-prone.

#### 6. `Buffer::input()` uses inverted sign for "just pressed" detection

**File:** `ssz_script/ssz/command.ssz:53-65`

```ssz
public void input(bool B, bool D, bool F, bool U, ...)
{
    if((B&!F) != (`B>0)){`Bb = 0; `B *= -1;} `Bb += `B;
    // ...
}
```

Sign of each state field tracks whether the key was already held. When state changes,
buffer count resets and sign flips. Initial state is `-1` (not pressed). The `keyState()`
function returns `min(-max(Db, Ub), Bb)` — blocks back if up or down is held
(no simultaneous up+back / down+back).

#### 7. `sysControls` acts as input offset for menu vs fight mode

**File:** `ssz_script/ssz/common.ssz:105`, `ssz_script/ssz/command.ssz:297`

```ssz
public int sysControls = 0; //0 = Fight, 10 = Menu
// ...
int pn = in + .com.sysControls;
int jn = .cfg.in[pn].jn;
```

Shifts input config index by 10 so menu controls (indices 10-11) are separate from
fight controls (0-1). MUGEN convention. Never validated that `cfg.in` has enough entries.

#### 8. `wrapProc` g_newChar timing is undefined

**File:** `main/sdlplugin/sdlplugin.cpp:405-425`, `main/sdlplugin/sdlplugin.cpp:2460`

`wrapProc` is called from the Windows message pump. `g_lastChar = g_newChar` is set in
`PollEvent()` which is called from SSZ scripts. The timing between these two paths is
undefined — `g_newChar` could be overwritten by a second WM_KEYDOWN before `PollEvent`
reads it.

### 🟢 Minor Issues

#### 9. Hotkey execution compiles Lua on every invocation

**File:** `ssz_script/ssz/fighting.ssz:525-533`

```ssz
loop{index i = 0; while; do:
    if(.se.eventKeys[i].down){
        ^^/char scri = .com.hotkeys.get(.com.eventKeyHash(.se.eventKeys[i]));
        if(#scri > 0 && !.dscri.L.runString(scri<>)){
            .al.alert!self?(.dscri.L.toString(-1));
        }
    }
    i++
while i < #.se.eventKeys:}
```

`runString()` compiles + executes Lua code on every hotkey press. No caching.
Acceptable for current usage (30-40 hotkeys, only fires on keydown).

#### 10. `PollInputBitmask` has 31 parameters

**File:** `main/sdlplugin/sdlplugin.cpp:2526`

FFI optimization to avoid struct marshaling. The `sec` flag controls whether
secondary pad is OR'd in. Works but makes the interface fragile.

#### 11. `eventUpdate` has ~80 lines of boilerplate per-letter key

**File:** `ssz_script/lib/alpha/sdlevent.ssz:69-120`

Each key (a-z, 0-9, F1-F12, arrows, etc.) has a separate `switch` case.
Could be a loop over a table. No functional impact.

#### 12. Axis threshold is hardcoded

**File:** `main/sdlplugin/sdlplugin.cpp:506-510`

```cpp
SDL_JoystickGetAxis(joys[joy], ...) < -3200;  // left/up
SDL_JoystickGetAxis(joys[joy], ...) > 3200;   // right/down
```

Threshold ±3200 of ±32768 range (~10%). Not configurable. Works for most controllers
but some have different deadzones.

## Summary

| # | Issue | Severity | Impact |
|---|-------|----------|--------|
| 1 | Joystick init disabled | 🔴 Critical | No gamepad support |
| 2 | SDL_INIT_JOYSTICK not called | 🔴 Critical | Even re-enabling init wouldn't work |
| 3 | Double TranslateMessage in wrapProc | 🔴 Critical | Broken text input (`g_newChar`) |
| 4 | modKeyState double negation | 🟡 Medium | Confusing but intentional |
| 5 | inputRemap 1-based/0-based inconsistency | 🟡 Medium | Works but error-prone |
| 6 | Buffer sign-based state tracking | 🟡 Medium | Clever but undocumented |
| 7 | sysControls hardcoded 0/10 offset | 🟡 Medium | No bounds validation |
| 8 | g_newChar timing undefined | 🟡 Medium | Potential race between WM and SDL |
| 9 | Hotkey runString no cache | 🟢 Low | Acceptable for current usage |
| 10 | PollInputBitmask 31 params | 🟢 Low | FFI optimization tradeoff |
| 11 | eventUpdate boilerplate | 🟢 Low | Code smell, no functional impact |
| 12 | Hardcoded axis threshold | 🟢 Low | Works for most controllers |

## Recommended Fixes (Priority Order)

1. **Fix `wrapProc` double TranslateMessage** — remove the duplicate `TranslateMessage` call
2. **Re-enable joystick** — add `SDL_INIT_JOYSTICK` to `SDL_Init` and uncomment `g_js.init()`
   with a configurable flag, or use `SDL_INIT_GAMECONTROLLER` (newer API, auto-mapping)
3. **Document `modKeyState` magic numbers** — add comments mapping 6→D, 15→S, 19→X,
   21→A, 22→C, 25→V
4. **Add bounds check for `sysControls`** — validate `pn + sysControls < #cfg.in`
5. **Cache hotkey Lua scripts** — compile once at registration, reuse at execution
