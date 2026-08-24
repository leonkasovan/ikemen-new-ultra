# Input & Sound Processing Optimization Plan

> Generated from code audit of `main/sdlplugin/sdlplugin.cpp`, `ssz_script/ssz/sound.ssz`, and `ssz_script/ssz/command.ssz`. Revised after line-level verification of every claim.

---

## Audio Processing

### S1. Replace normalize() with inline clamp 🔴 High

**Current:** `SetSndBuf()` calls `normalize()` 4096× per frame (2048 samples × 2 channels). Each call does 2× `pow()` on the non-clipping path (`tmp2` term), ~6 FP mul/div, and state updates.

**Key finding:** `normalize()` is behaviorally a pure clamp. Line 3054 reads `sam *= 1.0;` — the AGC gain (`v.bai`) and all tracking state (`fue`, `heikin`, `heri`, `herihenka`) are computed but **never applied to the output**, and `g__nvAll` is write-only (only referenced by `SetSndBuf` + `normalizeIdleRecovery`). The return value depends solely on its input: `clamp(sam, -1, 1)`.

Therefore no two-pass peak scan is needed — replace the loop body with an inline clamp. Exact same output, zero `pow()`, one pass:

```cpp
bool SSZ_STDCALL SetSndBuf(int32_t* buf)
{
    if(g_snddata.load(std::memory_order_relaxed) == g_sndbuf){
        normalizeIdleRecovery(g__nvAll);
        return false;
    }
    // ponytail: normalize()'s AGC state (bai) is dead since sam *= 1.0 at :3054 —
    // output is a pure clamp. If bai is ever re-enabled, restore the full call.
    for(int i = 0; i < g_samples*2; i++){
        double s = (double)(buf[i] / 2 * wav_vol) / 32768.0;
        if(s > 1.0) s = 1.0; else if(s < -1.0) s = -1.0;
        g_sndbuf[i] = (int16_t)(s * 32767.0 * g_vol);
    }
    std::atomic_thread_fence(std::memory_order_release);
    g_snddata.store(g_sndbuf, std::memory_order_relaxed);
    return true;
}
```

Notes:
- Preserves current semantics exactly: integer `buf[i]/2` truncation, clamp before `*g_vol` (matters when `wav_vol > 1.0`).
- The rejected two-pass design skipped clamping on the fast path, which diverges when volume >100%.
- Optional follow-up: delete `normalize()`/`NormalizeVar` entirely once this ships.

**Estimated savings:** all ~8192 `pow()` calls/frame eliminated plus branch/state overhead. Audio CPU cost drops to a vectorizable multiply+clamp pass.

**File:** `main/sdlplugin/sdlplugin.cpp:3101-3119`

---

### S2. Replace sndcallback loop with memcpy 🟡 Medium

**Current:** `sndcallback()` copies 4096 int16 values one at a time in a C++ `for` loop:
```cpp
for(int i = 0; i < g_samples*2; i++){
    ((int16_t*)stream)[i] = data[i];
}
```

**Proposed:** Replace with `memcpy`:
```cpp
void sndcallback(void* unused, Uint8* stream, int len)
{
    int16_t* data = g_snddata.load(std::memory_order_acquire);
    int copyBytes = len < (int)(g_samples*2*sizeof(int16_t))
                    ? len : (int)(g_samples*2*sizeof(int16_t));
    memcpy(stream, data, copyBytes);
    g_snddata.store(g_sndzero, std::memory_order_release);
}
```

**Why:** `memcpy` compiles to `rep movsb` on x86-64 which is optimized in hardware (up to 32 bytes/cycle on modern CPUs). The loop version requires the compiler to generate individual load/store pairs. The 4096-byte copy drops from ~100 cycles to ~20 cycles. Called 21.5×/sec, so saves ~1700 cycles/sec — not huge, but it's free.

**File:** `main/sdlplugin/sdlplugin.cpp:459-466`

---

## Input Processing

### I1. Pre-read Ctrl state before key loop 🔴 High

**Current:** `modKeyState()` is called 14× per player per frame (l,r,u,d,a,b,c,x,y,z,q,w,e,s). Each call polls Ctrl unconditionally at entry (command.ssz:246) — *before* the `!keyState` early-out:
```ssz
bool ctrl = .sdl.JoystickButtonState(:-1, 224:) || .sdl.JoystickButtonState(:-1, 228:);
```

That's **42 FFI round-trips per player per frame** (14 primary key polls + 28 ctrl polls), plus up to 5 more inside the else-branch when a key matches scancodes 6/15/19/21/22/25 (command.ssz:258). Most ctrl polls are wasted: movement keys rarely match hotkey scancodes.

**Proposed:** Compute `ctrl` once before the 14-key polling block, pass it as a parameter:

```ssz
// In command.input(), before the key polling block:
bool ctrl = .sdl.JoystickButtonState(:-1, 224:) || .sdl.JoystickButtonState(:-1, 228:);

// In modKeyState(), add ctrl parameter:
public bool modKeyState(bool keyState, int jn, int key, bool ctrl)
{
  branch{
  cond !keyState: ret true;
  else:
    // ... use ctrl directly instead of re-polling ...
  }
}
```

**Call site changes** (command.ssz ~1374-1388):
```ssz
l = !.modKeyState(.sdl.JoystickButtonState(:jn, .cfg.in[in].l:),jn,.cfg.in[in].l,ctrl)
  || (sec && .sdl.JoystickButtonState(:jn2, .cfg.in[in+2].l:));
// ... same for r, u, d, a, b, c, x, y, z, q, w, e, s
```

Ctrl sampled once per frame instead of 14× is semantically identical within a synchronous polling loop.

**Estimated savings:** 28 ctrl FFI calls eliminated per player per frame → 56/frame for 2 players. At ~100ns per FFI call, saves ~6µs/frame.

**Files:** `ssz_script/ssz/command.ssz:243-265,1366-1395`

---

### I2. ~~Remove legacy JoystickButtonState polling block~~ ❌ DROPPED

Original claim was that command.ssz:1374-1388 duplicates `PollInputBitmask` via `setLocalIn()` and is dead code. **Verification disproved both premises:**

1. **Not dead code.** `localUpdate()`→`setLocalIn()`→`PollInputBitmask` only runs in netplay (command.ssz:596,608) and replay/watch (command.ssz:833,844) paths. In normal local play, the block at 1374-1388 is the **only** input source — removing it removes player input entirely.
2. **Not equivalent where both run.** `PollInputBitmask` (sdlplugin.cpp:2556-2601) does raw button reads with no Ctrl-suppression. The direct block routes through `modKeyState`, which suppresses C/L/P/R/S/V during fights so debug hotkeys don't fire commands. Swapping one for the other regresses hotkey suppression.

Do not remove. A safe redesign (route through pre-polled bitmask + replicate modKeyState filtering in SSZ) would save ~14 FFI/player/frame but changes behavior surface — not worth it for µs.

---

### I3. Cache frame-invariant state before key loop 🟢 Low

**Current:** Inside `modKeyState()`, these expressions are re-evaluated 14× per call:
```ssz
.str.equ(.com.gameMode, "practice")  // string comparison × 14
.com.gameState == 1                   // field access × 14
```

**Proposed:** Cache before the loop:
```ssz
bool isPractice = .str.equ(.com.gameMode, "practice");
bool inFight = .com.gameState == 1;
// Then use isPractice/inFight in modKeyState via pass-by-value or class fields
```

**Estimated savings:** Negligible (~µs). Only worth doing as part of the I1 refactor.

**File:** `ssz_script/ssz/command.ssz:243-265`

---

## Implementation Priority

| Phase | Items | Est. Time | Impact |
|-------|-------|-----------|--------|
| **1** | S1 (inline clamp) | 10 min | 🔴 Eliminates all ~8k pow()/frame |
| **2** | I1 (Ctrl pre-read) + I3 (cache) | 20 min | 🔴 Eliminates 56 FFI calls/frame |
| **3** | S2 (memcpy) | 5 min | 🟡 Free improvement |

---

## Verification

After each phase:
1. `make CONFIG=Debug install -j8` — must compile clean
2. 10s timed test — check for audio glitches, input responsiveness
3. Frame count comparison — measure FPS delta before/after each phase

For S1 specifically: play loud content (hits, supers, many overlapping SFX) and confirm no clipping/distortion vs. before — the clamp must behave identically.

---

## Risk Assessment

| Change | Risk | Mitigation |
|--------|------|------------|
| S1 inline clamp | Low — provably identical output while `sam *= 1.0` stands | Comment marks the invariant; test loud SFX |
| S2 memcpy | Very low | Drop-in replacement; identical semantics |
| I1 Ctrl pre-read | Low | Same logic, hoisted; test Ctrl+C/V in practice mode |
| I3 gameState cache | Very low | Pure refactor |
