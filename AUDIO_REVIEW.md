# Audio / Sound System Review

## Architecture Overview

The audio system has two independent subsystems:

```
SFX (Sound Effects)
├── C++ layer: SDL audio callback + normalization
│   ├── SDL_OpenAudio (44100 Hz, stereo, S16, 2048 samples)
│   ├── sndcallback() → drains g_sndbuf into SDL stream
│   └── SetSndBuf() → script pushes mixed buffer, normalize + copy to callback
├── SSZ layer: sound.ssz
│   ├── Snd → loads Elecbyte SND files (MUGEN format)
│   ├── Sound → per-channel state (volume, pan, freq, loop)
│   ├── 4 mix functions: mix_s16, mix_m16, mix_s8, mix_m8
│   ├── mixSounds() → iterates 16 channels, mixes into sndbuf
│   └── playSound() → calls SetSndBuf() to push to C++
└── Lua bridge: sndPlay(), sndStop()

BGM (Background Music)
├── C++ layer: SDL_mixer
│   ├── Mix_OpenAudio (44100 Hz, stereo, S16, 2048)
│   ├── PlayBGM() → Mix_LoadMUS + Mix_PlayMusic (loops forever)
│   ├── PauseBGM() → Mix_Pause/ResumeMusic
│   ├── FadeIn/FadeOut → Mix_FadeIn/FadeOutMusic
│   └── SetVolume() → Mix_VolumeMusic
└── SSZ layer: sound.ssz
    ├── Bgm → thin wrapper calling sdl.playBGM()
    └── Lua bridge: playBGM()
```

## Key Files

| File | Purpose |
|------|---------|
| `main/sdlplugin/sdlplugin.cpp:435-476` | Audio globals, sndcallback, bgmclear |
| `main/sdlplugin/sdlplugin.cpp:593-620` | sndjoyinit: audio device init |
| `main/sdlplugin/sdlplugin.cpp:1855-1870` | End(): audio shutdown |
| `main/sdlplugin/sdlplugin.cpp:2995-3055` | NormalizeVar + normalize() |
| `main/sdlplugin/sdlplugin.cpp:3062-3200` | SetSndBuf, PlayBGM, PauseBGM, FadeIn/Out, SetVolume |
| `main/ssz/ssz.cpp:830-850` | SndCacheGet/Put (sample caching) |
| `ssz_script/ssz/sound.ssz` | Snd, Wave, Sound, Bgm classes, mix functions |
| `ssz_script/ssz/script.ssz:110-160` | Snd wrapper, sndPlay/sndStop Lua bridge |
| `ssz_script/lib/alpha/sdlplugin.ssz` | SSZ→C++ plugin declarations |

## Data Flow (Per Frame)

```
1. sndPlay(group, number)          [Lua → SSZ]
   └── Snd.play(group, number)
       └── getChannel(-1) → Sound object
           └── Sound.sound = wave data

2. playSound()                     [SSZ → C++]
   ├── setSndBuf(sndbuf)           [SSZ calls C++]
   │   └── SetSndBuf(buf)          [C++]
   │       ├── if(g_snddata == g_sndbuf) return false;  [busy-wait guard]
   │       ├── for each sample: normalize(sample * wav_vol) * g_vol
   │       ├── g_snddata = g_sndbuf  [swap pointer]
   │       └── return true
   ├── sndbufClear()                [zero the buffer]
   └── mixSounds()                  [mix all active Sound channels]
       └── for i in 0..15: sounds[i].mix(sndbuf, -160, 160)
           └── mix_s16/mix_m16/mix_s8/mix_m8  [per-channel mixing]

3. sndcallback(stream, len)         [SDL audio thread]
   ├── memcpy(g_snddata → stream)  [drain callback buffer]
   └── g_snddata = g_sndzero       [reset to silence]

4. PlayBGM(filename)                [Lua → SSZ → C++]
   └── bgmclear() → Mix_LoadMUS → Mix_PlayMusic(-1)  [loop forever]
```

## Issues Found

### 🔴 Critical

#### 1. SFX double-buffering race condition — sound cuts/duplicates

**File:** `main/sdlplugin/sdlplugin.cpp:3066-3070`

```cpp
bool SSZ_STDCALL SetSndBuf(int32_t* buf)
{
    if(g_snddata == g_sndbuf) return false;  // ← busy-wait guard
    for(int i = 0; i < g_samples*2; i++){
        g_sndbuf[i] = (int16_t)(normalize(...) * 32767.0 * g_vol);
    }
    g_snddata = g_sndbuf;  // ← swap: audio thread now reads g_sndbuf
    return true;
}
```

**Problem:** `g_snddata` is a raw pointer swapped between the main thread (SSZ script)
and the audio callback thread (SDL). There is **no mutex, atomic, or memory barrier**.

- **Race 1:** If `sndcallback` fires mid-copy (while `for` loop is writing to `g_sndbuf`),
  the callback reads partially-written data → **audio glitch/corruption**
- **Race 2:** If `SetSndBuf` writes to `g_sndbuf` while `sndcallback` is still reading
  the previous frame's data from `g_sndbuf` → **data torn**
- **The guard** `if(g_snddata == g_sndbuf) return false` tries to detect if the callback
  hasn't consumed the previous buffer yet, but this is a TOCTOU race — the callback
  could fire between the check and the copy.

**Impact:** Occasional audio pops, clicks, or repeated/dropped frames. At 44100 Hz with
2048 samples, the callback fires ~21.5 times/sec. The script must complete mix + SetSndBuf
within 46.4 ms or the buffer is lost.

#### 2. SFX normalization is expensive and stateful — no reset on track change

**File:** `main/sdlplugin/sdlplugin.cpp:2997-3050`

```cpp
struct NormalizeVar {
    static const double shitsu;  // = 32.0
    double bai, heri, herihenka, fue, heikin;
    NormalizeVar() :
        bai(1.0), heri(1.0), herihenka(0.0), fue(1.0), heikin(1.0/shitsu)
    {}
};
NormalizeVar g__nvAll;
```

The `normalize()` function is an adaptive limiter with 5 state variables (`bai`, `heri`,
`herihenka`, `fue`, `heikin`). It:

1. Detects peaks above ±1.0 and applies `pow(1.0/peak, heri)` gain reduction
2. Uses `herihenka` to ramp the limiter ratio smoothly
3. Tracks `heikin` (running mean) and `fue` (trend indicator)

**Problem:** `g__nvAll` is a single global instance shared across all frames. Its state
persists across track changes and silence periods. If a loud sound pushes the limiter
hard (`bai` drops low), subsequent quiet sounds will be attenuated until `bai` recovers
at a rate governed by `shitsu/(chs*sps) = 32/(2*44100) ≈ 0.00036` per sample — meaning
recovery takes **thousands of samples** (~2+ seconds).

**Also:** The `bai *= pow(1.0/peak, heri)` branch can make `bai` very small for a
loud transient, then the `bai += bai*(...)` branch in the quiet region recovers slowly.
This is a deliberate design (smooth limiter) but it means:
- After a loud hit, quiet sounds are ducked for ~1-2 seconds
- The normalization state should ideally be reset when `SetSndBuf` returns false
  (no new data this frame)

### 🟡 Structural Issues

#### 3. Two separate SDL audio devices — SFX and BGM compete for resources

**File:** `main/sdlplugin/sdlplugin.cpp:596-616`

```cpp
// SFX: SDL_OpenAudio (raw callback)
g_desired.freq = g_sndfreq;      // 44100
g_desired.format = AUDIO_S16;
g_desired.channels = 2;
g_desired.samples = g_samples;   // 2048
g_desired.callback = sndcallback;
SDL_OpenAudio(&g_desired, nullptr);

// BGM: Mix_OpenAudio (SDL_mixer)
Mix_OpenAudio(g_sndfreq, AUDIO_S16SYS, 2, g_samples);  // 44100, S16, 2, 2048
```

**Two separate audio devices are opened** with identical parameters. On Windows (WASAPI),
this means:
- Two separate audio streams are created
- The OS mixer combines them
- Potential for different latencies, sample rate mismatches, or format conversions
- Double the memory usage for audio buffers

**Recommendation:** Use a single SDL audio device. Route both SFX and BGM through
SDL_mixer's channel system (Mix_PlayChannel for SFX, Mix_PlayMusic for BGM).

#### 4. `g_sndbuf` and `g_sndzero` are stack-like globals — no alignment guarantee

**File:** `main/sdlplugin/sdlplugin.cpp:439-441`

```cpp
int16_t g_sndzero[g_samples*2] = {0};  // 8192 bytes
int16_t g_sndbuf[g_samples*2] = {0};   // 8192 bytes
int16_t *g_snddata = g_sndzero;
```

These are 16 KB total, allocated as plain globals. No SIMD alignment (SSE/AVX requires
16/32-byte alignment). The `sndcallback` does a plain `memcpy`-equivalent loop which
could be optimized with `_mm_loadu_si128`/`_mm_storeu_si128` if aligned.

#### 5. `sndbufClear()` is a byte-by-byte loop, not memset

**File:** `ssz_script/ssz/sound.ssz:390-394`

```ssz
public void sndbufClear()
{
  loop{index i = 0; do:
    .sndbuf[i++] = 0;
  while  i < .sdl.SNDBUFLEN:}
}
```

Clears 4096 `int32_t` values (16 KB) one at a time in SSZ interpreted code. This runs
every frame. A `memset(0)` in C++ would be ~100x faster.

#### 6. `mixSounds()` pan law is linear — no equal-power curve

**File:** `ssz_script/ssz/sound.ssz:314-316`

```ssz
`mix_m16(buf, fidxadd,
  (int)((float)lv-(`x*.panstr)),
  (int)((float)rv+(`x*.panstr)));
```

Panning is `left = vol - x*panstr`, `right = vol + x*panstr`. This is a **linear pan law**
which causes a 6 dB volume drop at center (when `x=0`, both channels are at full volume,
but the perceived loudness is +3 dB). A proper equal-power pan would use:
```
left  = vol * cos(angle)
right = vol * sin(angle)
```

Also, `panstr` (panning strength) defaults to `cfg.PanStr` but is never documented.

#### 7. `mix_s16` reads interleaved stereo as if it's mono — but handles it correctly by accident

**File:** `ssz_script/ssz/sound.ssz:203-218`

```ssz
void mix_s16(^int buf, float fidxadd, int lv, int rv)
{
  // ...
  buf[i]   += ((int)w[iidx]   | (int)(byte)w[iidx+1]<<8)  * lv >> 8;
  buf[i+1] += ((int)w[iidx+2] | (int)(byte)w[iidx+3]<<8)  * rv >> 8;
  i += 2;
  // ...
  iidx = (int)`fidx * 4;  // ← stride 4 bytes per sample (stereo 16-bit)
}
```

This is the **stereo** mix function (channels == 2). The index `iidx = fidx * 4` skips
4 bytes per frame (2 bytes × 2 channels). The left sample is read from `w[iidx..iidx+1]`
and right from `w[iidx+2..iidx+3]`. This is correct for interleaved stereo 16-bit PCM.

But the function is called `mix_s16` (the `s` prefix seems to mean "stereo"), while `mix_m16`
is the mono version. The naming convention (`s`/`m` prefix) is non-obvious.

#### 8. `SndCacheGet` parameter order mismatch with SSZ declaration

**File:** `main/ssz/ssz.cpp:836`, `ssz_script/ssz/sound.ssz:88-89`

```cpp
// C++ implementation:
extern "C" bool SSZ_STDCALL SndCacheGet(
    PluginUtil* pu, Reference* wav, int32_t number, int32_t group, Reference file)

// SSZ declaration:
plugin bool SndCacheGet(:^/char, int, int, ^ubyte=:) = <dll/ssz.dll>;
```

The SSZ call is `SndCacheGet(:filename, group, number, wav=:)` but the C++ signature
is `(pu, wav, number, group, file)`. The SSZ bridge auto-maps `^/char` → `Reference`,
but the **parameter order** differs: SSZ passes `(filename, group, number)` while C++
receives `(number, group)` (after `wav`). This works because the SSZ bridge passes
parameters positionally and the C++ side reads them in the order they appear on the stack.

**However**, `SndCachePut` has the same pattern and the comment says "keyed by file+group+number"
which suggests the order is intentional. Still, the mismatch is confusing.

### 🟢 Minor Issues

#### 9. Legacy SendOpen/Write/Close are dead code

**File:** `main/sdlplugin/sdlplugin.cpp:3146-3165`

```cpp
bool SSZ_STDCALL SendOpenBGM(int32_t channels, int32_t rate)
{
    LOG_DEBUG("SDL", "SendOpenBGM: deprecated (SDL_mixer handles all BGM)");
    return false;
}
void SSZ_STDCALL SendCloseBGM()
{
    LOG_DEBUG("SDL", "SendCloseBGM: deprecated");
}
intptr_t SSZ_STDCALL SendWriteBGM()
{
    LOG_DEBUG("SDL", "SendWriteBGM: deprecated");
    return 0;
}
```

These three functions are kept for ABI compatibility but do nothing. They could be
removed if the SSZ scripts are updated to not call them.

#### 10. `Bgm.write()` is a no-op

**File:** `ssz_script/ssz/sound.ssz:353-354`

```ssz
public void write()
{
  // BGM playback is handled by SDL_mixer – nothing to pump.
}
```

Called every frame from `mixSounds()`. The overhead is negligible (function call + return)
but it's dead code.

#### 11. Hardcoded 16-channel limit for SFX

**File:** `ssz_script/ssz/sound.ssz:374`

```ssz
/^&Sound sounds.new(16);
```

And `getChannel()`:
```ssz
public ^&.Sound getChannel(int ch)
{
  index c = .m.min!int?(15, ch);
  if(c >= 0) ret .sounds[c..c+1];
  loop{index i = 15; do:
    if(#.sounds[i].sound == 0){
      .sounds[i].setDefaultParameter();
      ret .sounds[i..i+1];
    }
  while --i >= 0:}
  ret .consts.null!&.Sound?();
}
```

16 channels, searched linearly from the end. If all 16 are occupied, new sounds are
silently dropped. In practice 16 is adequate for fighting games, but busy scenes with
many simultaneous effects (stage hazards + multiple characters + announcer) could
clip.

#### 12. `bgm_vol` is set in `SetVolume` but `FadeInBGM` ignores it

**File:** `main/sdlplugin/sdlplugin.cpp:3184-3194`

```cpp
void SSZ_STDCALL FadeInBGM(int time)
{
    int targetVol = (int)(bgm_vol * g_vol * MIX_MAX_VOLUME);
    Mix_VolumeMusic(0);                    // set to 0
    Mix_FadeInMusic(g_bgmMusic, -1, time); // fade in (uses current volume)
    Mix_VolumeMusic(targetVol);            // set target volume
}
```

`Mix_FadeInMusic` fades from 0 to the current music volume. But `Mix_VolumeMusic(targetVol)`
is called **after** `Mix_FadeInMusic`, which means the fade target is whatever volume was
set before the call, not `targetVol`. The sequence should be:
1. Set volume to 0
2. Set target volume to `targetVol`
3. Then `Mix_FadeInMusic` will fade from 0 to `targetVol`

As written, the fade-in target volume is unpredictable.

## Summary

| # | Issue | Severity | Impact |
|---|-------|----------|--------|
| 1 | SFX double-buffer race condition | 🔴 Critical | Audio corruption, pops, clicks |
| 2 | Normalization state persists across tracks | 🔴 Critical | Quiet sounds ducked after loud ones |
| 3 | Two separate SDL audio devices | 🟡 Medium | Double resource usage, latency |
| 4 | No SIMD alignment for audio buffers | 🟡 Medium | Suboptimal memcpy in callback |
| 5 | sndbufClear is SSZ interpreted loop | 🟡 Medium | ~100x slower than memset |
| 6 | Linear pan law (6dB center drop) | 🟡 Medium | Volume inconsistency |
| 7 | mix function naming (s/m prefix) | 🟢 Low | Code readability |
| 8 | SndCacheGet parameter order mismatch | 🟢 Low | Works but confusing |
| 9 | Legacy SendOpen/Write/Close dead code | 🟢 Low | ABI cruft |
| 10 | Bgm.write() is no-op | 🟢 Low | Dead code |
| 11 | Hardcoded 16 SFX channels | 🟢 Low | Adequate for current use |
| 12 | FadeInBGM volume target race | 🟡 Medium | Unpredictable fade behavior |

## Recommended Fixes (Priority Order)

1. **Fix SFX double-buffer race** — use `std::atomic<int16_t*>` for `g_snddata`, or a
   simple spinlock around the swap, or double-buffer with atomic flag
2. **Reset normalization state on silence** — add a frame counter to `NormalizeVar`; if
   no `SetSndBuf` call for N frames, reset `bai=1.0`
3. **Merge audio devices** — use a single SDL audio device and route SFX through
   SDL_mixer channels instead of a separate callback
4. **Move sndbufClear to C++** — add a `SndBufClear()` plugin function that does
   `memset(sndbuf, 0, sizeof(int32_t)*4096)`
5. **Fix FadeInBGM volume** — set target volume before calling `Mix_FadeInMusic`
6. **Add equal-power pan law** — use `cos/sin` instead of linear offset
