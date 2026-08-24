# Research — Emitting x64 Code in the SSZ JIT

**Scope:** What it takes to move `main/ssz/x86.hpp` (the raw-byte x86 emitter) from
32-bit to 64-bit code generation.
**Date:** 2026-08-25
**Verdict up front:** Feasible, ~4–8 weeks solo, high regression risk, and **no
requirement currently drives it**. Details and migration plan below.

---

## 1. Current Architecture (what exists today)

```
ssz script → sourcetree.hpp (parse) → jitcompiler.hpp (8,886 ln, codegen)
                                          ↓ emits bytes via
                                       x86.hpp BinaryCode (3,681 ln, raw x86 emitter)
                                          ↓ VirtualAlloc RW → memcpy → VirtualProtect RX
                                       JIT block executed in-process
                                          ↓ E8 rel32 calls into
                                       host-compiled plugin functions (static registry)
```

- One flat code buffer per program (`jitcompiler.hpp:8862`): `VirtualAlloc` →
  copy → `PAGE_EXECUTE`. Data (literals, `gre` block) lives separately and is
  reached with **absolute disp32** addresses backpatched at load
  (`GlobalAddress()` = `ModRM(0, reg, 5)` + int32).
- Calls: `FuncCall`/`PluginCall` emit `E8 rel32`; `PluginCall` truncates the
  target to `(int32_t)` — valid today because host exe + JIT block + heap all
  live under the 4 GB ceiling.
- SSZ internal calling convention: args on the stack, **callee-pops**
  (`Return(size, bp)` emits `ret imm16` — stdcall-style), EBP frames,
  `KansuStackSet` reserves the frame.
- Float/double: **x87** (`FldStk`, `FstAdr`, `FaddStk`, `Fcomi`, `FtoI` with
  `fnstcw/fldcw` control-word round-trips — 29 encoder methods).
- 64-bit integers (`long`/`ulong`): **emulated** as EDX:EAX register pairs with
  ~60 dedicated `*64` encoder methods plus helper calls (`Wari64`, `Amari64`,
  `Warui64`, `Amarui64` for div/mod).
- Registers: 8× GP (EAX..EDI), 3 address modes (`AT_LOCAL` EBP+disp,
  `AT_MEMBER` EDI+disp, `AT_SANSHOU` ESI+disp, `AT_GLOBAL` abs disp32).
- 8-bit ops use the legacy H/L register set (`REGISTER8`, 77 references).

## 2. Host-Side Coupling (why this is not just an emitter change)

The JIT emits code that runs **in the engine's own address space** and directly:

- calls host-compiled plugin functions (`TUserFunc` exports, statically
  registered — no DLL loading),
- pokes host structs (`Reference`, `DynamicRef`, `HeapObjHead`, `lua_State*`),
- shares the heap with the engine (`refnew`/`addsize` allocate engine-side).

⇒ **x64 JIT ⇒ x64 engine build.** A 64-bit process cannot execute the 32-bit
machine code the JIT emits today, and vice versa. Everything moves together:

| Layer | Change |
|---|---|
| Toolchain | w64devkit **x86** → x64 variant (or MSYS2 mingw64). Makefile `ARCH_F` is empty today (host default); needs `-m64` or new toolchain paths |
| Dependencies | All **19 static libs** rebuilt from source x64 (zlib 1.2.8, libpng 1.6.55, SDL 2.0.20, freetype 2.10.4, lua 5.2.4, libmodplug, mpg123, opus, FLAC…). Old but portable; MinGW x64 builds are typically clean |
| Runtime DLLs | `libvlc.dll` must be x64 (VLC 2.2.8 x64 build), SDL2.dll if not static |
| ABI | `SSZ_STDCALL` = `__stdcall` → **no-op on x64** (Win64 has one convention) — actually *simplifies* the host side |

## 3. The x64 Delta in the Emitter (`x86.hpp`)

### 3.1 Mechanical (every encoder)

- **REX prefix layer** (`0x40–0x4F`): needed for the 8 new GP registers
  (R8–R15), 64-bit operand size (`REX.W`), and byte regs SPL/BPL/SIL/DIL.
  The 77 `REGISTER8`/`Reg8`/`Hns8` sites must pick encodings carefully —
  `AH/BH/CH/DH` are unusable in a REX-prefixed instruction.
- **64-bit immediates**: `movabs` (B8+REX.W, imm64) for constants/addresses that
  don't fit imm32; most ALU ops stay imm32 sign-extended (cheaper).
- 262 `int32_t` casts in the emitter — offsets stay int32 (frames are small,
  rel32 range ±2 GB), but **pointer-valued** ones must widen.

### 3.2 Structural (the real work)

| Problem | Today | x64 requirement | Difficulty |
|---|---|---|---|
| **Plugin calls** | `E8 rel32`, args pre-pushed on stack, callee-pops | **Win64 ABI**: integer args in RCX,RDX,R8,R9; float/double in XMM0–3; 32-byte shadow space; 16-byte stack alignment; return RAX/XMM0. `bridge.cpp` param census: 224×int32, 144×bool, **129×float, 62×double**, 114×Reference, 63×intptr, 40×lua_State*, 19×SOCKET — every call site needs register-assignment marshalling | ★★★ hardest |
| **Plugin address reach** | `(int32_t)adr` truncation | JIT block (VirtualAlloc heap) vs exe image can be >2 GB apart → `movabs rax, adr; call rax` or a trampoline table | ★★ |
| **Global/literal addressing** | abs disp32, backpatched to `gre.data()` | `ModRM(0,5)` **means RIP-relative in 64-bit** — encoding collision. Fix: allocate data + code in **one** VirtualAlloc block and emit RIP-relative (cleanest), or `movabs` | ★★ |
| **Float math** | x87 stack | x87 still *works* on x64 internally, but plugin boundaries need SSE (`movss/movsd`, XMM args). Cleanest: port FP codegen to SSE2 wholesale | ★★ |
| **`long`/`ulong` emulation** | EDX:EAX pairs, ~60 `*64` methods, 4 div/mod helper calls | Native single-register ops — **deletes code** (the pleasant part) | ★ |
| **`index` type** | `intptr_t` = 4 bytes | 8 bytes → every stack-slot size, `Aligner`/`GetMemberSize` offset, `dup()`, struct member offset in jitcompiler shifts | ★★ |
| **8 new registers** | — | Free wins: pin ESI/EDI roles today; on x64 use R8–R15 for sanshou/member bases and scratch | optional |

### 3.3 jitcompiler.hpp assumptions (8,886 lines, 125 `int32_t` sites)

- Stack frame layout (`KansuStackSet`, param offsets, `Return(Aligner(...))`) —
  all slot sizes derive from `sizeof(intptr_t)`-equivalents; audit each.
- `Reference`/`DynamicRef`/`HeapObjHead` raw offsets — these are host structs;
  they **double automatically** when the host rebuilds x64, and the JIT must
  follow (they're compiled from the same headers, so mostly automatic — but any
  hardcoded `+4`/`+8` pokes break silently).
- Lua bridge: `lua_State*` passed by value into plugin calls — pointer, fine;
  `funcCall`'s packed `{int32_t* ret; lua_State** pL}` arg block is x86-size
  dependent (`#pragma pack(1)`, pointer member doubles on x64) — SSZ-side
  callback signatures `(&.lua.State L=, int re=)` must match the new layout.

### 3.4 What gets *simpler*

- One calling convention (no stdcall/cdecl split).
- Native 64-bit integer ops — ~60 emulated methods + 4 helper functions deleted.
- No far/near pointer distinction; `intptr_t` ops are native.
- x87 control-word juggling (`PushFcw`/`PopFcw`) disappears if FP moves to SSE.

## 4. Migration Plan (if pursued)

**Strategy: dual-arch emitter, phase-gated.** Keep x86 emission working at every
step; select target via a `BinaryCode` policy (template param or `#define`).

0. **Gate on a reason.** No current feature needs x64 (engine WS ≈ 100–300 MB;
   640×480 2D). Do this only for: >4 GB mods/asset packs, x64-only dependencies,
   or modernization mandate.
1. **Host x64 build, JIT still x86** — impossible as a running system (64-bit
   process can't execute 32-bit code), so Phase 1 is *toolchain + 19 deps +
   Makefile* only, validated by compiling, not running.
2. **Emitter core**: REX layer + reg64 + movabs + RIP-relative data addressing
   (single VirtualAlloc block for code+data) + `movabs/call rax` plugin calls.
   Milestone: "hello world" SSZ script runs x64.
3. **Win64 plugin-call marshalling**: register args, XMM float args, shadow
   space, alignment. Milestone: all 14 plugins callable; lua bridge round-trips.
4. **SSE float codegen** replacing x87 (or x87-interim + SSE at boundaries).
5. **Native 64-bit int ops** replacing EDX:EAX emulation (delete code).
6. **Full regression**: all `ssz_script/` compiles + runs, 607 Lua globals
   exercised, all 5 renderers, netplay/replay. **No automated test suite exists**
   — budget time for building one first (script snapshot diffs), or this phase
   is manual and slow.

**Estimate:** Phases 2–3 are the core (2–4 weeks); total 4–8 weeks solo with
regression risk concentrated in plugin ABI marshalling and silent struct-offset
breaks.

## 5. Risks

- **Silent miscompiles**: raw-byte emitters fail loudly only sometimes; wrong
  REX/ModRM usually corrupts a *later* instruction. A disassembler check pass
  (capstone) on emitted blocks would be worth adding during the port.
- **Old dependency versions on x64 MinGW** — all should build; budget for
  patches (mpg123/opus win32 trees especially).
- **VLC x64** — 2.2.8 x64 builds exist; the dynamic loader path
  (`LoadLibraryA("libvlc.dll")`) is arch-matched, so just ship the right dll.
- **No test suite** — biggest schedule risk, not the codegen itself.

## 6. Cheaper Alternatives (if the underlying goal is X)

- Goal *more memory* → 4GB / LARGEADDRESSAWARE flag on the current x86 exe
  (link flag only; boot-time gain to ~3–4 GB user space with 64-bit OS).
- Goal *faster long/ulong math* → not available in 32-bit mode (needs REX);
  only micro-optimizable via better x86 sequences.
- Goal *modern deps* → some libs can be updated in-place while staying x86.
- Goal *new functions* → unrelated to arch; plugin ABI already handles it.

## 7. Key Files

| File | Role | x64 impact |
|---|---|---|
| `main/ssz/x86.hpp` | raw-byte x86 emitter (`BinaryCode`) | rewrite/extend ~60–70% |
| `main/ssz/jitcompiler.hpp` | codegen driving the emitter; frame layout; plugin-call marshalling | 125 int32 sites; stack offsets; call marshalling |
| `main/ssz/ssz.cpp:202` | `VirtualAlloc` shim | fine as-is |
| `main/ssz/jitcompiler.hpp:8862` | code block alloc + RX protect | merge data block for RIP-relative |
| `main/ssz/sszdef.h` | `SSZ_STDCALL`/`THREADCALL` macros | no-op on x64 — simplify |
| `main/ssz/pluginutil.hpp` | `TUserFunc` plugin export macro | works; ABI enforced by host compiler |
| `Makefile` | toolchain + deps | x64 toolchain, `-m64`, 19 lib rebuilds |
