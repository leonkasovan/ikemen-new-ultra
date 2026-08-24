# Audio / Sound System Review

> Revised after source verification against `main/sdlplugin/sdlplugin.cpp`,
> `main/ssz/ssz.cpp`, and `ssz_script/ssz/sound.ssz`. Two claims from the
> first draft were wrong (#6 math, #12 was not a bug); several severities
> were adjusted.

## Architecture Overview

The audio system has two independent subsystems:

```
SFX (Sound Effects)
├── C++ layer: SDL audio callback + normalization
│   ├── SDL_OpenAudio (44100 Hz, stereo, S16, 2048 samples)
│   ├── sndcallback() → drains g_snddata into SDL stream
│   └── SetSndBuf() → script pushes mixed buffer, normalize + copy to callback
├── SSZ layer: sound.ssz
│   ├── Snd → loads Elecbyte SND files (MUGEN format)
│   ├── Sound → per-channel state (volume, pan, freq, loop)
│   ├── 4 mix functions: mix_s16/mix_s8 (stereo), mix_m16/mix_m8 (mono)
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
| `main/sdlplugin/sdlplugin.cpp:437-478` | Audio globals, sndcallback, bgmclear |
| `main/sdlplugin/sdlplugin.cpp:594-622` | sndjoyinit: both audio devices opened here |
| `main/sdlplugin/sdlplugin.cpp:3004-3059` | NormalizeVar + normalize() |
| `main/sdlplugin/sdlplugin.cpp:3066-3080` | SetSndBuf |
| `main/sdlplugin/sdlplugin.cpp:3087-3204` | PlayBGM, PauseBGM, SetVolume, FadeIn/OutBGM |
| `main/sdlplugin/sdlplugin.cpp:3144-3161` | Legacy SendOpen/Write/Close (deprecated no-ops) |
| `main/ssz/ssz.cpp:836-854` | SndCacheGet/Put (sample caching) |
| `ssz_script/ssz/sound.ssz` | Snd, Wave, Sound, Bgm classes, mix functions |
| `ssz_script/lib/alpha/sdlplugin.ssz:112-125` | Legacy BGM streaming wrappers |

## Data Flow (Per Frame)

```
1. sndPlay(group, number)          [Lua → SSZ]
   └── Snd.play(group, number)
       └── getChannel(-1) → Sound object
           └── Sound.sound = wave data

2. playSound()                     [sound.ssz:408-414]
   ├── setSndBuf(sndbuf)           [SSZ calls C++]
   │   └── SetSndBuf(buf)          [C++]
   │       ├── if(g_snddata == g_sndbuf) return false;  [undelivered guard]
   │       ├── for each sample: normalize(sample * wav_vol) * g_vol
   │       ├── g_snddata = g_sndbuf  [swap pointer]
   │       └── return true
   ├── sndbufClear()                [zero the buffer]
   └── mixSounds()                  [mix all active Sound channels]
       └── for i in 0..15: sounds[i].mix(sndbuf, -160, 160)
           └── mix_s16/mix_m16/mix_s8/mix_m8  [per-channel mixing]

3. sndcallback(stream, len)         [SDL audio thread, sdlplugin.cpp:459-466]
   ├── copy g_snddata → stream      [drain, element loop]
   └── g_snddata = g_sndzero        [reset to silence]

4. PlayBGM(filename)                [Lua → SSZ → C++]
   └── bgmclear() → Mix_LoadMUS → Mix_PlayMusic(-1)  [loop forever]
```

## Issues Found

### 🔴 Critical

#### 1. SFX hand-off between main thread and SDL audio thread is unsynchronized

**File:** `main/sdlplugin/sdlplugin.cpp:3066-3080` (SetSndBuf),
`main/sdlplugin/sdlplugin.cpp:459-466` (sndcallback)

```cpp
bool SSZ_STDCALL SetSndBuf(int32_t* buf)     // main thread
{
    if(g_snddata == g_sndbuf) return false;
    for(int i = 0; i < g_samples*2; i++){
        g_sndbuf[i] = ...;                    // writes buffer
    }
    g_snddata = g_sndbuf;                     // publishes pointer
    return true;
}

void sndcallback(void*, Uint8* stream, int len)  // audio thread
{
    for(int i = 0; i < g_samples*2; i++){
        ((int16_t*)stream)[i] = g_snddata[i]; // reads through live pointer
    }
    g_snddata = g_sndzero;                    // consumes
}
```

`g_snddata` is a plain `int16_t*` written by two threads with no atomic,
mutex, or barrier. Note the protocol itself is mostly sound — the guard does
prevent the naive races (the callback never sees `g_sndbuf` before the swap,
because only `SetSndBuf` ever stores it). The real failure modes are:

- **Tear during drain:** callback starts copying while `g_snddata ==
  g_sndzero`; mid-loop the main thread completes `SetSndBuf` and swaps the
  pointer → second half of that callback reads the new frame (or partially
  written data if preempted inside the write loop). Result: half-silence/
  half-audio output, plus the frame is consumed (`g_snddata = g_sndzero`)
  even though the main thread received `return true`.
- **Compiler reordering (UB):** nothing constrains store order across
  threads. The compiler may legally sink `g_snddata = g_sndbuf` above the
  buffer writes or hoist/break up the loads in the callback. x86 TSO saves
  you at the hardware level; the optimizer does not.

**Impact:** occasional half-buffer glitches and silently dropped frames;
worst case under -O2 with unlucky scheduling, sustained corruption.
~21.5 callbacks/sec at 44100 Hz / 2048 samples.

**Fix (small):** make it `std::atomic<int16_t*>` — release store on publish
in `SetSndBuf`, acquire load once at callback entry into a local, then index
the local. ~5 lines, no locks.

### 🟡 Medium

#### 2. Adaptive limiter state is global and never resets

**File:** `main/sdlplugin/sdlplugin.cpp:3004-3059`

`normalize()` is an inherited adaptive limiter (5 state vars: `bai`, `heri`,
`herihenka`, `fue`, `heikin`) driven by the global `g__nvAll`. After a loud
transient drives `bai` down, recovery takes on the order of seconds, so
quiet sounds that follow get ducked (pumping). This is inherent to any
limiter and is original Ikemen design, not a regression — hence Medium, not
Critical. Optional improvement: decay `bai` back toward 1.0 when
`SetSndBuf()` returns false (idle frames), or expose the recovery rate.

### 🟢 Low

#### 3. Two separate SDL audio devices (SFX raw callback + SDL_mixer)

**File:** `main/sdlplugin/sdlplugin.cpp:594-622`

`SDL_OpenAudio` and `Mix_OpenAudio` each open a device with identical specs.
On WASAPI shared mode this works fine — the OS mixer combines them; practical
latency/resource impact is negligible.

**Do NOT "fix" this by routing SFX through Mix_PlayChannel/Mix_Chunk**: SFX
mixing is procedural in script (sound.ssz:214-342) — per-frame `freqmul`
resampling, custom loop points, pan computed at mix time every frame. That
does not map onto pre-rendered Mix_Chunks without losing features or
re-architecting the mixer. Leave as-is unless a concrete problem shows up.

#### 4. No SIMD alignment on `g_sndbuf` / `g_sndzero`

**File:** `main/sdlplugin/sdlplugin.cpp:439-441`

True but immaterial: 8 KB copies of int16 data saturate memory bandwidth
regardless of alignment; `alignas(32)` buys nothing measurable. Non-issue.

#### 5. `sndbufClear()` is an element loop in SSZ

**File:** `ssz_script/ssz/sound.ssz:369-374`

4096 JIT'd stores ≈ microseconds per frame. Negligible on its own. The
actually expensive per-frame work is the four mix loops themselves
(sound.ssz:214-316) — if any audio work moves to C++, move those, not the
clear.

#### 6. Linear pan law — and stereo waves bypass panning entirely

**File:** `ssz_script/ssz/sound.ssz:337-339`

```ssz
`mix_m16(buf, fidxadd,
  (int)((float)lv-(`x*.panstr)),
  (int)((float)rv+(`x*.panstr)));
```

Mono sources use a linear pan law: at center both channels run at full
volume → **+3 dB power bump at center** (equivalently −3 dB at the
extremes). Equal-power (`cos`/`sin`) would keep perceived loudness constant.

Separately (and more audible): the `channels == 2` branch (sound.ssz:327)
never applies `x` at all, so **stereo waves ignore pan completely**. If pan
matters for gameplay (side-specific cues), fixing the stereo bypass matters
more than the pan law shape.

#### 7. Mix function naming (`s`/`m` prefix)

`s` = stereo, `m` = mono, trailing digits = bits per sample. Heritage naming,
correctly implemented interleaved-stereo handling (stride 4 bytes in
`mix_s16`). Readability nit only.

#### 8. `SndCacheGet`/`SndCachePut` parameter order looks mismatched

**File:** `main/ssz/ssz.cpp:836-854`, `ssz_script/ssz/sound.ssz:56-57,109,113`

SSZ declares `(:^/char, int, int, ^ubyte=:)` and calls `(filename, group,
number, wav)`; C++ receives `(pu, wav*, number, group, file)`. Whatever the
bridge's positional mapping is, **Get and Put use identical orderings on
both sides**, so the cache key is self-consistent and cannot break — worst
case the group/number roles are transposed relative to their names, which is
harmless for a cache. Confusing to read, zero runtime impact.

#### 9. Legacy `SendOpenBGM`/`SendCloseBGM`/`SendWriteBGM` are dead

**File:** `main/sdlplugin/sdlplugin.cpp:3144-3161` (C++ no-ops),
`ssz_script/lib/alpha/sdlplugin.ssz:112-125` (still declared/wrapped)

Deletable together with the SSZ declarations. Cosmetic cleanup.

#### 10. `Bgm.write()` is a no-op

**File:** `ssz_script/ssz/sound.ssz:353-356`

Called per frame from `mixSounds()`. Negligible cost; deletable along with
its call site.

#### 11. Hardcoded 16 SFX channels, silent drop when full

**File:** `ssz_script/ssz/sound.ssz:367,377-388`

`getChannel(-1)` scans channels 15→0 for a free slot; when none are free,
`addWave()` quietly returns false. 16 is adequate for fighting games; the
silent drop is worth a debug log line before considering raising the limit.

## Verified Non-Issues

- **`FadeInBGM` volume ordering is correct.** SDL_mixer's fade interpolates
  against the *live* music volume on every audio tick — it does not snapshot
  the volume inside `Mix_FadeInMusic`. The sequence `Mix_VolumeMusic(0)` →
  `Mix_FadeInMusic(...)` → `Mix_VolumeMusic(targetVol)` (sdlplugin.cpp:3192-3194)
  fades 0 → targetVol exactly as intended; the leading zero-set is merely
  redundant. Worst case is one sub-buffer mixed at volume 0 (inaudible).

## Summary

| # | Issue | Severity | Impact |
|---|-------|----------|--------|
| 1 | Unsynchronized `g_snddata` hand-off (tear + UB) | 🔴 Critical | Half-buffer glitches, dropped frames, possible corruption under optimization |
| 2 | Global limiter state never resets | 🟡 Medium | Pumping/ducking after loud transients |
| 3 | Two separate SDL audio devices | 🟢 Low | Negligible in practice; merging via Mix_Chunks is impractical |
| 4 | No SIMD alignment on buffers | 🟢 Low | Immaterial |
| 5 | `sndbufClear` element loop | 🟢 Low | µs/frame; real cost is the mix loops |
| 6 | Linear pan law; stereo waves ignore pan | 🟡 Medium | Center +3 dB bump; pan dead for stereo sources |
| 7 | Mix function naming | 🟢 Low | Readability |
| 8 | SndCache param order confusion | 🟢 Low | Self-consistent, cannot break |
| 9 | Legacy Send* BGM stubs | 🟢 Low | Dead code both sides |
| 10 | `Bgm.write()` no-op | 🟢 Low | Dead code |
| 11 | 16 SFX channels, silent drop | 🟢 Low | Adequate; add log line |

## Recommended Fixes (Priority Order)

1. **Fix the SFX hand-off** — `std::atomic<int16_t*> g_snddata`: release
   store after the buffer writes in `SetSndBuf`, acquire load once at
   sndcallback entry into a local variable. No spinlocks needed; the
   existing guard logic stays.
2. **Equal-power pan + apply pan to stereo sources** — cos/sin gains in
   `mix()`; passing panned `lv`/`rv` into the stereo mixers covers both the
   law and the bypass with one change.
3. **Limiter idle recovery** — decay `bai` toward 1.0 when `SetSndBuf`
   returns false (or add an explicit reset hook); preserves the limiter's
   character during continuous audio.
4. **Delete dead code** — Send*BGM trio + SSZ declarations, `Bgm.write()`.
5. **Log dropped SFX** — one LOG_DEBUG in `addWave()` when channel alloc
   fails.
