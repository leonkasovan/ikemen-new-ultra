# Lua Scripting in Ikemen GO Plus Ultra

## Overview

Ikemen GO Plus Ultra embeds **Lua 5.2** as a scripting layer via a static bridge between the SSZ JIT-compiled engine and the Lua C API. The engine boots with SSZ (`ssz_script/ssz/ikemen.ssz`), which creates a Lua state, registers hundreds of engine functions as Lua globals, then loads and executes the game's Lua scripts.

## Architecture

### Execution Flow

```
main.cpp::main()
  ├─ lua_static_register()       -- registers C bridge functions
  ├─ Run("ssz_script/ssz/ikemen.ssz") -- SSZ JIT-compiles ikemen.ssz
  │    └─ ikemen.ssz::main()
  │         ├─ &.lua.State L;    -- NewState() → luaL_newstate + openlibs
  │         ├─ init(L)           -- registers 7 core callbacks as Lua globals
  │         └─ L.runFile(cfg)    -- luaL_loadfile + lua_pcall
  │              └─ (Lua code executes, calling back into SSZ)
```

### Two-Layer Bridge

| Layer | File | Language | Role |
|---|---|---|---|
| C bridge | `main/lua/lua.cpp` | C++ | Wraps Lua 5.2 C API as plugin exports |
| Static registration | `main/lua_static.hpp` | C++ | Maps function names to pointers via `SSZ_RegisterFunction("lua", ...)` |
| Plugin registry | `main/ssz/static_plugin_registry.hpp` | C++ | Singleton `libraryName -> {funcName -> funcPtr}` map |
| SSZ type wrapper | `ssz_script/lib/alpha/lua.ssz` | SSZ | `&lua.State` class with typed method wrappers |
| SSZ callbacks | `ssz_script/ssz/script.ssz` | SSZ | ~110 functions registered as Lua globals via `L.register()` |
| SSZ callbacks | `ssz_script/ssz/system-script.ssz` | SSZ | ~130 screen/gameplay functions registered as Lua globals |
| SSZ callbacks | `ssz_script/ssz/trigger-script.ssz` | SSZ | ~100 MUGEN trigger functions registered as Lua globals |
| SSZ callbacks | `ssz_script/ssz/debug-script.ssz` | SSZ | ~25 debug/hotkey functions registered as Lua globals |
| Entry point | `ssz_script/ssz/ikemen.ssz` | SSZ | Creates Lua state, registers 7 core callbacks, loads system Lua |

## The `lua.State` Class

Defined in `ssz_script/lib/alpha/lua.ssz`. This is the primary interface for SSZ code to interact with Lua.

### Constructor / Destructor

```ssz
&.lua.State L;        // Creates a Lua state via NewState() → luaL_newstate + openlibs
// L is destroyed when it goes out of scope (Close() → lua_settop to 0 + lua_close)
```

At module load time, `lua.ssz` calls `plugin void Init()` to register the bridge callbacks (`refSetNull`, `refCopy`) that allow the C side to manage SSZ `ref` lifetimes. This is called once per SSZ compilation.

### Stack Operations

```ssz
L.getTop()            -> int              // lua_gettop
L.pop(n)              -> void             // lua_pop
L.pushNumber(n)       -> void             // lua_pushnumber
L.pushBoolean(b)      -> void             // lua_pushboolean
L.pushString(s)       -> void             // lua_pushstring
L.pushRef(r)          -> void             // lua_newuserdata + SSZ ref as metatable
```

### Type Checking

```ssz
L.isNumber(idx)       -> bool             // lua_isnumber
L.isBoolean(idx)      -> bool             // lua_isboolean
L.isString(idx)       -> bool             // lua_isstring
```

### Value Retrieval

```ssz
L.toNumber(idx)       -> double           // lua_tonumber
L.toBoolean(idx)      -> bool             // lua_toboolean
L.toString(idx)       -> ^char            // lua_tostring (SSZ wrapper reads via output param)
L.toRef(idx)          -> ref              // lua_touserdata → DynamicRef copy
```

Note: `ToString` in the C bridge (`lua.cpp`) is `void ToString(int32_t idx, lua_State* L, std::string& output)` — it writes to an output parameter. The SSZ wrapper in `lua.ssz` presents it as a return value by declaring a local `^char tmp`, passing it as an out-parameter, and returning it.

### Execution

```ssz
L.runFile(path)       -> bool             // luaL_loadfile + lua_pcall (0 args, 0 results)
L.runString(str)      -> bool             // luaL_loadstring + lua_pcall (0 args, 0 results)
L.pcall(nargs, nres)  -> bool             // lua_pcall (call a Lua function on the stack)
```

Note: The SSZ wrapper `pcall(nargs, nresults)` reorders parameters compared to the C API. The C bridge signature is `Pcall(int32_t nresults, int32_t nargs, lua_State* L)` — results first, then arguments — matching `lua_pcall(L, nargs, nresults, 0)`. The SSZ wrapper presents the more natural `(nargs, nresults)` order.

### Global Registration

```ssz
L.getGlobal(var)      -> void             // lua_getglobal (pushes value onto stack)
L.register(name, fn)  -> void             // lua_pushcclosure with SSZ function as upvalue
```

Note: The C bridge signature is `Register(intptr_t func, const std::string& var, lua_State* L)` — the function pointer comes first, then the name. The SSZ wrapper presents the more natural `(name, fn)` order.

The `register` method is the core bridge mechanism. It wraps any SSZ function as a Lua C closure. When Lua calls that global, the `funcCall` callback in `lua.cpp` routes back into the SSZ runtime.

## SSZ Callback Convention

Every SSZ function registered as a Lua global follows this signature pattern:

```ssz
void myCallback(&.lua.State L=, int re=)
```

- `L=` — the Lua state (passed by reference)
- `re=` — return value: set to number of return values pushed, or **-1 on error**
- Arguments are read from the Lua stack using helper functions

### Argument Helper Functions

```ssz
// Read arguments from the Lua stack with error reporting
numArg(L=, re=, argc=, nresults)  -> double    // reads L.toNumber(++argc)
blArg(L=, re=, argc=, nresults)   -> bool      // reads L.toBoolean(++argc)
strArg(L=, re=, argc=, nresults)  -> ^char     // reads L.toString(++argc)
refArg!&Type?(L=, re=, argc=, nresults) -> &Type // reads L.toRef(++argc), casts to type
```

Typical usage:
```ssz
void someFunc(&.lua.State L=, int re=)
{
  int argc = 0, nret = 1;           // start at arg 0, expect 1 return value
  ^/char path = .strArg(L=, re=, argc=, nret);
  if(re < 0) ret;                   // error: argument not a string
  int n = (int).numArg(L=, re=, argc=, nret);
  if(re < 0) ret;
  // ... do work ...
  L.pushNumber(result);             // push the return value
}
```

### Returning Errors

Set `re = -1` and push an error string:
```ssz
void myFunc(&.lua.State L=, int re=)
{
  re = -1;
  L.pushString("Something went wrong.");
}
```

The C bridge (`funcCall` in `lua.cpp:38-43`) reads the `re` value after the callback returns. When `re < 0`, it reads the error string from the Lua stack via `lua_tostring(L, -1)` and passes it to `luaL_error(L, "%s", ...)`, which raises a Lua error. The error string pushed by the callback is what Lua sees as the error message.

---

## Complete Lua API Reference

All functions below are registered as Lua globals from `ssz_script/ssz/script.ssz`, `ssz_script/ssz/system-script.ssz`, `ssz_script/ssz/trigger-script.ssz`, and `ssz_script/ssz/debug-script.ssz`.

### Callbacks registered in `ikemen.ssz::init()`

| Lua Name | SSZ Function | Returns | Description |
|---|---|---|---|
| `loadStart` | `loadStart(L, re)` | — | Resets match state, starts loader thread |
| `selectStart` | `selectStart(L, re)` | — | Resets selection screen, calls loadStart |
| `game` | `scGame(L, re)` | `number` | Runs the fight engine loop |
| `sszReload` | `sszReload(L, re)` | — | Restarts the engine executable |
| `procMemMark` | `procMemMark(L, re)` | — | Records a memory profiling marker |
| `memMarkBefore` | `memMarkBefore(L, re)` | — | Records memory usage before a phase |
| `memMarkAfter` | `memMarkAfter(L, re)` | — | Records memory usage after a phase |

### Callbacks registered in `script.ssz::init()`

**Resource loading**
| Lua Name | Parameters | Returns | Description |
|---|---|---|---|
| `sffNew(file)` | string | userdata | Loads an SFF sprite file, returns Sff object |
| `sndNew(file)` | string | userdata | Loads an SND sound file, returns Snd object |
| `fontNew(file)` | string | userdata | Loads an FNT font file, returns Font object |
| `commandNew()` | — | userdata | Creates a new Command object |
| `loadVideo(file, capturepath, volume, audiotrack)` | string,string,int,int | — | Loads and plays a video |
| `playBGM(file)` | string | — | Plays background music |
| `getBGM()` | — | string | Returns current BGM filename |
| `fadeInBGM(time)` | int | — | Fades BGM in over time (ms) |
| `fadeOutBGM(time)` | int | — | Fades BGM out over time (ms) |
| `startButton(pn)` | int | bool | Checks if player pressed start |
| `drawTTF(file, align, text, x, y, scaleX, scaleY, r, g, b, alpha)` | string,int,string,int,int,float,float,int,int,int,int | — | Renders TTF text |

**Sound**
| Lua Name | Parameters | Returns | Description |
|---|---|---|---|
| `sndPlay(snd, group, index)` | userdata,int,int | — | Plays a sound from a loaded Snd |
| `sndStop()` | — | — | Stops all sounds |

**Command/Input**
| Lua Name | Parameters | Returns | Description |
|---|---|---|---|
| `commandAdd(cmd, name, command, [time])` | userdata,string,string,int | — | Adds a command to a CommandList |
| `commandGetState(cmd, name)` | userdata,string | bool | Checks if a command state is active |
| `commandInput(cmd, pn)` | userdata,int | — | Feeds player input into command engine |
| `commandBufReset(cmd)` | userdata | — | Resets command buffer |
| `getSysCtrl()` | — | number | Returns system control flags |
| `setSysCtrl(v)` | int | — | Sets system control flags |

**Input dialog**
| Lua Name | Parameters | Returns | Description |
|---|---|---|---|
| `inputDialogNew()` | — | userdata | Creates a new InputDialog |
| `inputDialogPopup(dialog, title)` | userdata,string | — | Shows an input dialog |
| `inputDialogIsDone(dialog)` | userdata | bool | Checks if dialog input is complete |
| `inputDialogGetStr(dialog)` | userdata | string | Gets the entered text |
| `inputText(mode, allowDot)` | string,bool | string | Captures keyboard input (mode: "text"/"num"/"all") |
| `setInputText(str)` | string | — | Sets the input buffer |
| `clearInputText()` | — | — | Clears the input buffer |
| `clipboardPaste()` | — | bool | Checks if paste is available |
| `getClipboardText()` | — | string | Gets clipboard text |

**System/Shell**
| Lua Name | Parameters | Returns | Description |
|---|---|---|---|
| `sszOpen(dir, file)` | string,string | — | Opens a file in the SSZ directory |
| `batOpen(dir, file)` | string,string | — | Opens a batch file |
| `webOpen(url)` | string | — | Opens a URL in the default browser |
| `setSharedString(str)` | string | — | Sets special engine strings ("end", "reload") |
| `exitMatch()` | — | — | Triggers match exit |

**Input state (keyboard keys)**
| Lua Name | Returns | Description |
|---|---|---|
| `esc()` | bool | Escape key |
| `upKey()` / `downKey()` / `leftKey()` / `rightKey()` | bool | Arrow keys |
| `aKey()` through `zKey()` | bool | Letter keys |
| `zeroKey()` through `nineKey()` | bool | Number row keys |
| `kzeroKey()` through `knineKey()` | bool | Numpad keys |
| `f1Key()` through `f12Key()` | bool | Function keys (note: f6–f8 are not registered) |
| `returnKey()` / `backspaceKey()` / `spaceKey()` | bool | Special keys |
| `lshiftKey()` / `rshiftKey()` / `tabKey()` | bool | Modifier keys |
| `minusKey()` / `equalsKey()` | bool | Punctuation keys |
| `leftbracketKey()` / `rightbracketKey()` / `backslashKey()` | bool | Bracket/escape keys |
| `semicolonKey()` / `commaKey()` / `periodKey()` / `slashKey()` | bool | Punctuation keys |
| `insertKey()` / `homeKey()` / `pageupKey()` / `deleteKey()` / `endKey()` / `pagedownKey()` | bool | Navigation keys |
| `printscreenKey()` | bool | Print Screen |
| `kdivideKey()` / `kmultiplyKey()` / `kminusKey()` / `kplusKey()` / `kenterKey()` / `kperiodKey()` | bool | Numpad operators |

**Gamepad**
| Lua Name | Parameters | Returns | Description |
|---|---|---|---|
| `getGamepadKeyA(pn)` | int | bool | Gamepad A button |
| `getGamepadKeyB(pn)` | int | bool | Gamepad B button |
| `getGamepadKeyC(pn)` | int | bool | Gamepad C button |

**Match configuration**
| Lua Name | Parameters | Returns | Description |
|---|---|---|---|
| `sszRandom()` | — | number | Random number [0, 1) |
| `setAutoguard(pn, on)` | int,bool | — | Enables/disables autoguard |
| `setPowerShare(team, on)` | int,bool | — | Enables/disables power sharing |
| `setHomeTeam(n)` | int | — | Sets home team |
| `setCoins(n)` | int | — | Sets coin count |
| `getCoins()` | — | number | Gets coin count |
| `setCredits(n)` | int | — | Sets credit count |
| `getCredits()` | — | number | Gets credit count |
| `setRoundTime(t)` | int | — | Sets round time |
| `getRoundTime()` | — | number | Gets round time |
| `setRoundsToWin(n)` | int | — | Sets rounds to win |
| `getRoundsToWin()` | — | number | Gets rounds to win |
| `p1RoundsWon()` / `p2RoundsWon()` | — | number | Rounds won by each player |
| `setP1winsDisplay(str)` / `setP2winsDisplay(str)` | string | — | Sets win display text |
| `p1winsDisplay()` / `p2winsDisplay()` | — | string | Gets win display text |
| `setP1winsFormatted(str)` / `setP2winsFormatted(str)` | string | — | Sets formatted win text |
| `setP1matchWins(n)` / `setP2matchWins(n)` | int | — | Sets match wins count |
| `getP1matchWins()` / `getP2matchWins()` | — | number | Gets match wins count |
| `setTimerFormatted(str)` / `setTimerDisplay(str)` | string | — | Sets timer display text |
| `timerDisplay()` | — | string | Gets timer display text |
| `setTimer(t)` | int | — | Sets timer value |
| `timerTotal()` | — | number | Gets total timer value |
| `setCountdownFormatted(str)` / `setCountdownDisplay(str)` | string | — | Sets countdown display text |
| `countdownDisplay()` | — | string | Gets countdown display text |
| `setCountdown(n)` / `countdown()` | int | number | Countdown value setter/getter |
| `setScoreDisplay(str)` / `scoreDisplay()` | string | — | Score display text |
| `setScore(n)` / `score()` | int | number | Score value setter/getter |
| `setScoreTotal(n)` / `scoreTotal()` | int | number | Total score value setter/getter |
| `setP1Score(n)` / `setP2Score(n)` / `p1score()` / `p2score()` | int | — | Per-player score |
| `setMatchInfo(...)` / `setMatchnoDisplay(str)` / `matchnoDisplay()` | string | — | Match number display |
| `setMatchNo(n)` / `getMatchNo()` | int | — | Match number (setter/getter) |
| `setLastMatch(n)` / `getLastMatch()` | int | — | Last match index |
| `setCPULevel(pn, level)` / `cpuLevel(pn)` | int | number | CPU AI level for player |
| `setAilevelDisplay(str)` / `ailevelDisplay()` | string | — | AI level display text |
| `setGameModeDisplay(str)` / `gamemodeDisplay()` | string | — | Game mode display text |
| `setRewardFormatted(str)` / `setRewardDisplay(str)` / `rewardDisplay()` | string | — | Reward display |
| `setTourneyState(v)` / `getTourneyState()` | int | number | Tournament state |
| `setFTNo(v)` / `getFTNo()` | int | number | Fight number |
| `setFirstAttackCount(n)` / `firstAttackCount()` | — | — | First attack counter |
| `setConsecutiveWins(n)` / `consecutiveWins()` | — | — | Consecutive wins counter |
| `setWinTimeCount(n)` / `winTimeCount()` | — | — | Win by time counter |
| `setWinPerfectCount(n)` / `winPerfectCount()` | — | — | Perfect win counter |
| `setWinSpecialCount(n)` / `winSpecialCount()` | — | — | Special win counter |
| `setWinPerfectSpecialCount(n)` / `winPerfectSpecialCount()` | — | — | Perfect special win counter |
| `setWinHyperCount(n)` / `winHyperCount()` | — | — | Hyper win counter |
| `setWinPerfectHyperCount(n)` / `winPerfectHyperCount()` | — | — | Perfect hyper win counter |
| `setWinThrowCount(n)` / `winThrowCount()` | — | — | Throw win counter |
| `setWinPerfectThrowCount(n)` / `winPerfectThrowCount()` | — | — | Perfect throw win counter |
| `setPersistLife(n)` / `persistLife()` | int | number | Life persistence for survival |
| `setLifePersistence(n)` / `getLifePersistence()` | int | number | Life persistence per round |
| `setPersistPower(n)` / `persistPower()` | int | number | Power persistence |
| `setPowerPersistence(n)` / `getPowerPersistence()` | int | number | Power persistence per round |
| `setPersistRoundTime(n)` / `persistRoundtime()` | int | number | Round time persistence |
| `setTimePersistence(n)` / `getTimePersistence()` | int | number | Time persistence per round |
| `setPlayerLife(pn, n)` / `getPlayerLife(pn)` | int | number | Player life |
| `setPlayerPower(pn, n)` / `getPlayerPower(pn)` | int | number | Player power |
| `setPlayerAttack(pn, n)` / `getPlayerAttack(pn)` | int | number | Player attack multiplier |
| `setPlayerDefence(pn, n)` / `getPlayerDefence(pn)` | int | number | Player defence multiplier |
| `setPlayerReward(pn, n)` / `getPlayerReward(pn)` | int | number | Player reward index |
| `setAbyssDepth(n)` / `getAbyssDepth()` | int | number | Abyss mode depth |
| `setAbyssDepthBoss(n)` / `getAbyssDepthBoss()` | int | number | Abyss boss depth |
| `setAbyssDepthBossSpecial(n)` / `getAbyssDepthBossSpecial()` | int | number | Abyss special boss depth |
| `setAbyssBossFight(n)` / `getAbyssBossFight()` | int | number | Abyss boss fight index |
| `setAbyssFinalDepth(n)` / `getAbyssFinalDepth()` | int | number | Abyss final depth |
| `setAbyssSP1(n)` / `getAbyssSP1()` ... `setAbyssSP4(n)` / `getAbyssSP4()` | int | number | Abyss special parameters |

### Callbacks registered in `system-script.ssz::init()`

Note: `system-script.ssz::init()` calls `.sc.init(L=)` first, which registers all functions from `script.ssz`. The functions below are **additional** functions registered only by `system-script.ssz`.

**Thread**
| Lua Name | Parameters | Returns | Description |
|---|---|---|---|
| `sleep(ms)` | number | — | Suspends the current thread for `ms` milliseconds. Wraps `thread.ThreadDelay` (→ `Sleep(ms)`). |

**Text Image**
| Lua Name | Parameters | Returns | Description |
|---|---|---|---|
| `textImgNew()` | — | userdata | Creates a TextImg object |
| `textImgGetWidth(ti)` | userdata | number | Returns text width |
| `textImgSetFont(ti, font)` | userdata,userdata | — | Sets font |
| `textImgSetBank(ti, bank)` | userdata,int | — | Sets font bank |
| `textImgSetAlpha(ti, a)` | userdata,int | — | Sets alpha |
| `textImgSetWindow(ti, x, y, w, h)` | userdata,int,int,int,int | — | Sets clipping window |
| `textImgSetAlign(ti, a)` | userdata,int | — | Sets text alignment |
| `textImgSetText(ti, str)` | userdata,string | — | Sets text content |
| `textImgSetPos(ti, x, y)` | userdata,int,int | — | Sets position |
| `textImgAddPos(ti, x, y)` | userdata,int,int | — | Adds to position |
| `textImgSetScale(ti, x, y)` | userdata,float,float | — | Sets scale |
| `textImgDraw(ti)` | userdata | — | Draws the text image |

**Animation**
| Lua Name | Parameters | Returns | Description |
|---|---|---|---|
| `animNew()` | — | userdata | Creates an Anim object |
| `animSetPos(anim, x, y)` | userdata,int,int | — | Sets position |
| `animAddPos(anim, x, y)` | userdata,int,int | — | Adds to position |
| `animSetTile(anim, x, y)` | userdata,int,int | — | Sets tiling |
| `animSetColorKey(anim, r, g, b)` | userdata,int,int,int | — | Sets color key |
| `animSetPal(anim, pal)` | userdata,userdata | — | Sets palette |
| `animSetAlpha(anim, a)` | userdata,int | — | Sets alpha |
| `animSetScale(anim, x, y)` | userdata,float,float | — | Sets scale |
| `animSetWindow(anim, x, y, w, h)` | userdata,int,int,int,int | — | Sets clipping window |
| `animGetFrame(anim)` | userdata | number | Gets current frame number |
| `animUpdate(anim)` | userdata | — | Advances animation frame |
| `animReset(anim)` | userdata | — | Resets animation to frame 0 |
| `animDraw(anim)` | userdata | — | Draws the animation |

**Network / Replay**
| Lua Name | Parameters | Returns | Description |
|---|---|---|---|
| `enterNetPlay()` | — | — | Enters netplay mode |
| `enterReplay()` | — | — | Enters replay mode |
| `exitNetPlay()` | — | — | Exits netplay mode |
| `exitReplay()` | — | — | Exits replay mode |
| `netplay()` | — | bool | Returns whether in netplay mode |
| `replay()` | — | bool | Returns whether in replay mode |
| `synchronize()` | — | bool | Returns sync state |
| `setListenPort(p)` / `getListenPort()` | int | number | Sets/gets listen port |
| `setUserName(n)` / `getUserName()` | string | string | Sets/gets user name |

**Character / Stage Selection**
| Lua Name | Parameters | Returns | Description |
|---|---|---|---|
| `addChar(path)` | string | — | Adds a character to selection |
| `addStage(path)` | string | — | Adds a stage to selection |
| `setRandomSpr(n)` | int | — | Sets random select sprite |
| `setSelColRow(c, r)` | int,int | — | Sets selection grid columns/rows |
| `setSelCellSize(w, h)` | int,int | — | Sets cell size |
| `setSelCellScale(x, y)` | float,float | — | Sets cell scale |
| `numSelCells()` | — | number | Returns number of selectable cells |
| `setStage(n)` | int | — | Sets stage by index |
| `selectStage(n)` | int | — | Selects stage |
| `setStgMusic(n)` | int | — | Sets stage music |
| `setTeamMode(pn, mode)` | int,int | — | Sets team mode |
| `getCharName(idx)` | int | string | Gets character display name |
| `getCharFileName(idx)` | int | string | Gets character file name |
| `selectChar(pn, idx)` | int,int | — | Selects character for player |
| `getCharVar(pn, idx)` / `setCharVar(pn, idx, v)` | int,int | number | Character variable get/set |
| `getHelperVar(pn, idx)` / `setHelperVar(pn, idx, v)` | int,int | number | Helper variable get/set |
| `getStageName(idx)` | int | string | Gets stage display name |

**Game mode / Controller**
| Lua Name | Parameters | Returns | Description |
|---|---|---|---|
| `setCom(pn, level)` | int,int | — | Sets CPU level for player |
| `setTag(pn, tag)` | int,int | — | Sets tag mode |
| `setAutoLevel(pn, level)` | int,int | — | Sets auto difficulty level |
| `setGameType(t)` | int | — | Sets game type |
| `setGameMode(m)` / `getGameMode()` | int | number | Game mode (setter/getter) |
| `setService(s)` / `getService()` | int | number | Service mode |
| `setPlayerSide(pn, side)` / `getPlayerSide(pn)` | int,int | number | Player side |
| `setPauseVar(v)` / `getPauseVar()` | int | number | Pause menu variable |
| `swapController(a, b)` / `swapGamepad(a, b)` / `disableGamepad(pn)` | int | — | Controller configuration |
| `getInputKeyboard(pn)` / `getInputID(pn)` | int | number | Input device info |
| `inputReset()` | — | — | Resets input configuration |
| `setInputConfig(keyboardID, player)` | int,int | — | Binds keyboard to player |
| `resetRemapInput(pn)` / `getRemapInput(pn, btn)` / `remapInput(pn, btn, key)` | int | — | Input remapping |

**Display / Video**
| Lua Name | Parameters | Returns | Description |
|---|---|---|---|
| `setBrightness(v)` / `getBrightness()` | int | number | Screen brightness |
| `setOpacity(v)` | int | — | Window opacity |
| `setVolume(v)` | int | — | Master volume |
| `setVideoVolume(v)` / `getVideoVolume()` | int | number | Video volume |
| `setPanStr(str)` | string | — | Sets pan string |
| `getWidth()` / `getHeight()` | — | number | Screen dimensions |
| `setGameRes(w, h)` | int,int | — | Sets game resolution |
| `setFullScreenMode(m)` / `getFullScreenMode()` | int | number | Fullscreen mode |
| `setScreenMode(m)` / `getScreenMode()` | int | number | Screen mode |
| `setAspectRatio(b)` / `getAspectRatio()` | bool | bool | Aspect ratio lock |
| `setWindowType(t)` / `getWindowType()` | int | number | Window type |
| `takeScreenShot()` | — | — | Takes screenshot |
| `getScreenshotsPath()` | — | string | Gets screenshots directory |
| `getWindowTitle()` | — | string | Gets window title |
| `setLifeMul(m)` / `setPowerMul(m)` | float | — | Life/power multipliers |
| `setTeam1VS2Life(f)` | float | — | 1v2 life ratio |
| `setTurnsRecoveryRate(f)` | float | — | Turns mode recovery rate |
| `setZoom(v)` / `setZoomMin(v)` / `setZoomMax(v)` / `setZoomSpeed(v)` | float | — | Camera zoom |

**Lifebar / HUD**
| Lua Name | Parameters | Returns | Description |
|---|---|---|---|
| `loadLifebar(path, ...)` | string | — | Loads lifebar from file |
| `loadDebugFont(path)` | string | — | Loads debug font |
| `setDebugScript(script, pn)` | string,int | — | Sets debug script for player |
| `drawFace(pn, x, y, s)` / `drawPortrait(pn, x, y, s)` etc. | int,int,int,int | — | Draws character portraits |
| `drawTourneyPortrait(...)` / `drawFacePortrait(...)` / `drawOrderPortrait(...)` | various | — | Additional portrait types |
| `drawVSPortrait(...)` / `drawWinPortrait(...)` / `drawLoserPortrait(...)` | various | — | VS/Win/Loser portraits |
| `drawResultPortrait(...)` / `drawExtraPortrait(...)` | various | — | Result/Extra portraits |
| `drawStageIcon(x, y, s)` / `drawStagePortrait(x, y, s)` etc. | int,int,int | — | Draws stage icons/portraits |
| `drawVSStagePortrait(...)` / `drawWinStagePortrait(...)` / `drawExtraStagePortrait(...)` | various | — | Additional stage portrait types |

**Match control**
| Lua Name | Parameters | Returns | Description |
|---|---|---|---|
| `getQuoteID(pn)` | int | number | Returns quote ID for player |
| `setLifebarDisplay(str)` | string | — | Sets lifebar display text |
| `setInputDisplay(str)` | string | — | Sets input display text |
| `setAttackDisplay(str)` | string | — | Sets attack display text |
| `setPowerStateP1(v)` / `setPowerStateP2(v)` | int | — | Power state for player |
| `setLifeStateP1(v)` / `setLifeStateP2(v)` | int | — | Life state for player |
| `setDummyState(v)` / `setDummyDistance(v)` / `setDummyGuard(v)` / `setDummyRecovery(v)` | int | — | Training dummy config |
| `setCounterHit(b)` | bool | — | Force counter-hit state |
| `setSharedLife(b)` | bool | — | Shared life (simul mode) |
| `setPlaybackCfg(str)` / `getPlaybackCfg()` | string | string | Input playback config |
| `startDummyRecord()` / `endDummyPlayback()` / `playDummyRecord()` | — | — | Input recording/playback |
| `configModified()` | — | — | Flags config as modified |
| `setConfigTVars(v)` / `getConfigTVars()` | int | number | Config tourney variables |
| `setOS(v)` / `getOS()` | int | number | OS detection |
| `setSuaveMode(b)` | bool | — | Suave mode flag |
| `refresh()` | — | — | Refreshes the screen |

### Callbacks registered in `trigger-script.ssz::registerFunction()`

These are MUGEN trigger functions exposed to Lua for querying game state. They are typically called during fight script evaluation.

**Player entity access**
| Lua Name | Parameters | Returns | Description |
|---|---|---|---|
| `player(id)` | int | userdata | Gets player by ID |
| `parent()` | — | userdata | Gets parent entity |
| `root()` | — | userdata | Gets root entity |
| `helper(id)` | int | userdata | Gets helper by ID |
| `target(id)` | int | userdata | Gets target by ID |
| `partner()` | — | userdata | Gets partner entity |
| `enemy()` | — | userdata | Gets enemy entity |
| `enemynear()` | — | userdata | Gets nearest enemy |
| `playerid(id)` | int | userdata | Gets player by exact ID |

**Game state queries**
| Lua Name | Parameters | Returns | Description |
|---|---|---|---|
| `credits()` | — | number | Current credits |
| `coins()` | — | number | Current coins |
| `playerlife(pn)` | int | number | Player life |
| `playerpower(pn)` | int | number | Player power |
| `playerattack(pn)` | int | number | Player attack |
| `playerdefence(pn)` | int | number | Player defence |
| `playerreward(pn)` | int | number | Player reward |
| `abyssdepth()` | — | number | Abyss depth |
| `abyssdepthboss()` | — | number | Abyss boss depth |
| `abyssdepthbossspecial()` | — | number | Abyss special boss depth |
| `abyssbossfight()` | — | number | Abyss boss fight index |
| `abyssfinaldepth()` | — | number | Abyss final depth |
| `abysssp1()` through `abysssp4()` | — | number | Abyss special parameters |
| `ailevel()` | — | number | AI level |
| `tagmode()` | — | number | Tag mode |

Note: This section documents only the most commonly used trigger functions. `trigger-script.ssz` registers ~100 functions total, covering standard MUGEN triggers like `alive`, `anim`, `animelemno`, `animelemtime`, `animexist`, `animtime`, `authorname`, `backedge`, and many more.

### Callbacks registered in `debug-script.ssz::init()`

These functions are registered when a debug script is loaded via `setDebugScript()`.

| Lua Name | Parameters | Returns | Description |
|---|---|---|---|
| `puts(str)` | string | — | Prints debug output |
| `setLife(pn, n)` | int,int | — | Sets player life |
| `setLifeMax(pn, n)` | int,int | — | Sets player max life |
| `setPower(pn, n)` | int,int | — | Sets player power |
| `setAttack(pn, n)` | int,int | — | Sets player attack |
| `setDefence(pn, n)` | int,int | — | Sets player defence |
| `selfState(state)` | int | — | Sets current state |
| `addHotkey(key, cmd)` | string,string | — | Adds a debug hotkey |
| `toggleClsnDraw()` | — | — | Toggles collision box display |
| `toggleDebugDraw()` | — | — | Toggles debug overlay |
| `toggleStatusDraw()` | — | — | Toggles status display |
| `togglePostMatch()` | — | — | Toggles post-match screen |
| `togglePause()` | — | — | Toggles pause |
| `togglePauseMenu()` | — | — | Toggles pause menu |
| `step()` | — | — | Advances one frame |
| `toggleRecord()` | — | — | Toggles input recording |
| `toggleRecordEnd()` | — | — | Ends input recording |
| `togglePlayback()` | — | — | Toggles input playback |
| `roundReset()` | — | — | Resets the current round |
| `reload()` | — | — | Reloads the match |
| `setAccel(n)` | int | — | Sets game speed acceleration |
| `setAILevel(n)` | int | — | Sets AI level |
| `procMemMark(str)` | string | — | Records a memory profiling marker |
| `memMarkBefore(str)` | string | — | Records memory before phase |
| `memMarkAfter(str)` | string | — | Records memory after phase |

## SSZ Ref Passing (userdata)

SSZ `ref` objects can be passed to and from Lua as full userdata with a metatable.

```
Lua: ref = lua.toRef(1)    -- pops a userdata from stack
Lua: lua.pushRef(ref)      -- pushes a DynamicRef as userdata
```

The metatable has a `__gc` metamethod (`refGc` in `lua.cpp:32`) that releases the SSZ reference when Lua garbage-collects the userdata. The release is routed through the SSZ callback mechanism (`g_refDestroy`) to safely decrement the refcount on the SSZ side.

## Standard Lua Libraries

The following libraries are opened individually in `lua.cpp::NewState()` (not via `luaL_openlibs()`):

**Standard Lua 5.2 libraries:**
- `base` (print, type, pairs, ipairs, tostring, tonumber, etc.)
- `package` (module loading, `require`)
- `coroutine` (coroutine.create, resume, yield)
- `table` (table.insert, remove, sort, etc.)
- `io` (file, open, read, write)
- `os` (time, date, clock, difftime)
- `string` (string manipulation)
- `bit32` (Lua 5.2 bitwise operations)
- `math` (math functions)
- `debug` (debugging hooks)

**Additional libraries:**
- `lfs` — LuaFileSystem (file system operations: directories, attributes)
- `ffi` — LuaJIT FFI (foreign function interface for calling C functions from Lua)
- `lpeg` — LPeg pattern matching library (alternative to Lua patterns)

## Lua State Lifecycle and Crash Workaround

Closing a Lua state while SSZ callback frames are on the stack can crash due to FFI cdata finalization accessing freed SSZ memory. The engine works around this with a **deferred quarantine** mechanism:

1. `Close()` checks `g_luaCallbackDepth` — if non-zero, the state is pushed onto `g_deferredLuaClose` instead of being closed immediately
2. When callback depth returns to 0, `LuaProcessDeferredClose()` runs `LuaSafeQuarantine()` on all pending states
3. `LuaSafeQuarantine()` calls `ffi.cleanup()` to invalidate cdata objects, then clears the Lua stack
4. After quarantine, `lua_close()` is called normally

This prevents the FFI/SSZ ownership bug from crashing while keeping memory bounded.

## System Lua Script Location

The Lua script executed on startup is determined by the `system` field in `ssz_script/save/config.ssz`. Typically:

```lua
-- data/system.lua (loaded by ikemen.ssz: L.runFile(.cfg.system))
```

There are no `.lua` files distributed with the engine by default — the entire game is implemented in SSZ. Lua scripting is an **extension layer** for screenpacks and game modes that want to use Lua instead of (or alongside) SSZ.

## Example

### SSZ Side — Registering a Callback

```ssz
// In script.ssz or system-script.ssz
void myCustomFunc(&.lua.State L=, int re=)
{
  int argc = 0, nret = 1;
  ^/char name = .strArg(L=, re=, argc=, nret); if(re < 0) ret;
  ^char result = "Hello, " + name + "!";
  L.pushString(result);
}

// In init():
L.register("myCustomFunc", .myCustomFunc);
```

### Lua Side — Using the Callback

```lua
-- In any Lua script loaded by the engine
local greeting = myCustomFunc("World")
print(greeting)  -- "Hello, World!"
```

### Lua Side — Calling Back to SSZ

```lua
-- The "game", "loadStart", "selectStart", "sszReload" globals
-- are SSZ functions registered by ikemen.ssz

game()  -- Starts the fight engine loop
```

## Error Handling

- When an SSZ callback sets `re = -1`, it must also push an error string via `L.pushString()` onto the Lua stack.
- After the callback returns, the C bridge (`funcCall` in `lua.cpp:38-43`) reads the `re` value. If `re < 0`, it reads the error string from `lua_tostring(L, -1)` and calls `luaL_error(L, "%s", errorString)`, raising a Lua error.
- Lua errors propagate back through `L.runFile()` or `L.runString()` as a `false` return value.
- The engine checks the return: if `runFile` fails, it reads the error from `L.toString(-1)` and displays an alert.

```ssz
if(!L.runFile(.cfg.system)){
  ^/char err = L.toString(-1);
  if(!.s.equ(err[#err-10..-1], "<game end>"))
    .error!self?(err);
}
```

## Key Source Files

| File | Purpose |
|---|---|
| `main/lua/lua.cpp` | C bridge: wraps Lua 5.2 C API as plugin exports |
| `main/lua_static.hpp` | Static registration table for the `lua` plugin library |
| `main/ssz/static_plugin_registry.hpp` | Singleton registry resolving `plugin` declarations to function pointers |
| `main/ssz/pluginutil.hpp` | `TUserFunc` macro, `PluginUtil` string conversion helpers |
| `ssz_script/lib/alpha/lua.ssz` | SSZ `&lua.State` class — typed wrapper for the plugin bridge |
| `ssz_script/ssz/ikemen.ssz` | Entry point: creates Lua state, registers 7 core callbacks, runs system Lua |
| `ssz_script/ssz/script.ssz` | ~110 engine functions registered as Lua globals |
| `ssz_script/ssz/system-script.ssz` | ~130 screen/gameplay functions registered as Lua globals (+ `sleep`) |
| `ssz_script/ssz/trigger-script.ssz` | ~100 MUGEN trigger functions registered as Lua globals |
| `ssz_script/ssz/debug-script.ssz` | ~25 debug/hotkey functions registered as Lua globals |
