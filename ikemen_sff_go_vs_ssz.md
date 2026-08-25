# SFF Loader Comparison — Go (`image.go`) vs SSZ (`sff.ssz`)

**Reference:** `C:\Projects\ikemen-develop-update\src\image.go` (Ikemen GO, 2,355 lines)
**Port:** `ssz_script/ssz/sff.ssz` (1,427 lines)
**Purpose:** The SSZ implementation is a line-by-line port of the Go loader. This
doc maps every piece, then lists the **functional differences** — some are
deliberate (renderer conditionals), some are **gaps worth porting**.

---

## 1. Structure Mapping

| Go (`image.go`) | SSZ (`sff.ssz`) | Notes |
|---|---|---|
| `SffHeader` struct + `Read()` | `&SffHeader` + `read()` :23 | Same fields, same v1/v2 dispatch |
| `PaletteList` struct | `&PalleteList` :76 | Same API surface |
| `Sprite` struct :576 | `&Sprite` :122 | Go adds `coldepth`, `palhash`, `pendingData` (lazy GPU) |
| `Sff` struct :1562 | `&Sff` :665 | Go: `map[[2]uint16]*Sprite`; SSZ: `IntTable!uint, &.Sprite?` keyed `group<<16\|number` |
| `preloadSff()` :1833 | `Sff.loadFile()` :685 | Go's selective preload vs SSZ full load |
| `loadFromSff()` :619 (**commented out** in Go) | `Sprite.loadFromSff()` :464 | Single-sprite on-demand load — SSZ keeps it alive (used by stage portraits) |
| `loadPalettes()` :2124 | `Sff.loadFile()` palette loop :694 | See §4 |
| `ReadPalette()` :2200 | inline palette read :716/:550 | See §4 |
| `TransType`/`PalFX` :18/:58 | `.com.PalFX` (common.ssz) | Out of scope here |

---

## 2. Header Parsing — Equivalent

Both: 12-byte signature `"ElecbyteSpr\0"` → version bytes (lo3,lo2,lo1,hi) →
v1: palettes=0, nsprites, spr-off, dummy / v2: 4 dummies, spr-off, nsprites,
pal-off, npalettes, lofs, dummy, tofs. Byte-identical logic.

---

## 3. Sprite Reading — Differences

### 3.1 PCX palette location (v1) — **Go is more robust**

- **Go** `read():979-999`: when the palette is new, scans **backwards** from
  `blockEnd-769` down to `pcxDataStart` for the PCX `0x0C` marker, falls back
  to `blockEnd-769`. Handles padded/oversized subheaders.
- **SSZ** `read():236-255`: no scan — reads RLE from `offset+128` sized
  `lof-128-768`, then palette immediately after. If the subheader has padding
  between RLE data and palette, the palette is read from the wrong offset.

**Worth porting:** the backwards `0x0C` scan (~15 lines).

### 3.2 Palette alpha — **Go stores RGBA, SSZ stores RGB**

- **Go**: `pal[i] = alpha<<24 | b<<16 | g<<8 | r`, index 0 forced transparent
  (`alpha=0`), others 255; SFF `Version[2]==0` forces this, otherwise the
  file's alpha byte is kept (`ReadPalette():2240-2246`).
- **SSZ**: `pal[i] = r<<16 | g<<8 | b` — no alpha channel at all
  (`sff.ssz:253`, `:726`, `:560`).

Transparency in SSZ comes from the colorkey index (`mask`), not palette alpha.
Consequence: Go-style per-palette alpha (SFFv2 RGBA palettes) is **not
supported** by the SSZ loader. Fine for the current renderers; would matter
for a GPU-alpha port.

### 3.3 `readV2` format coverage — **Go handles more**

| Format | Go | SSZ |
|---|---|---|
| fmt 0 (uncompressed, `rle==0`) | `coldepth` 8/24/32 raw read + `SetRaw` | **missing** — `rle==0` skips the whole block, `px` stays nil → blank sprite |
| fmt 2 (RLE8) | decode | decode (`rle8Decode`) |
| fmt 3 (RLE5) | decode | decode (`rle5Decode`) |
| fmt 4 (LZ5) | decode | decode (`lz5Decode`) |
| fmt 10 (paletted PNG) | `png.Decode` → `*image.Paletted.Pix` | `.sdl.decodePNG8` plugin |
| fmt 11/12 (RGBA PNG) | `png.Decode` → RGBA → `SetRaw` 32-bit, `isRaw` skips `SetPxl` | GL renderers: `loadPngTexture`, `rle=-12`; **SW/DirectX: sprite blanked** (`rct.w=rct.h=0`) |

Two gaps:
1. **Uncompressed SFFv2 sprites** (`fmt 0`, produced by some tools) load blank in SSZ.
2. **Truecolor PNG sprites (11/12) are invisible on Renderer 0/4** — the
   `?/*.cfg.Renderer == 0 || == 4` branch zeroes the rect. Deliberate
   (software blitter is 8-bit only), but a known capability gap vs Go.

### 3.4 Decode-into vs decode-copy

Go splits each decoder into `xxxDecodeInto(rle, dst)` + allocating wrapper;
SSZ decoders take `^ubyte px=` and reallocate in place. Semantically equal;
Go's split avoids one copy in `read()` paths that decode into a pre-sized dst.

### 3.5 Robustness of decoders

Go's decoders bound-check `i < len(rle)-1` before every read and `j < len(dst)`
before every write. SSZ versions index `rle[i++]` unchecked — a malformed SFF
reads out of bounds (SSZ refs have no hard bounds guarantee on `/ubyte` reads).
**Worth porting** the guard style if crash-on-bad-SFF reports appear.

### 3.6 `SaveMemory` gate — SSZ-only

SSZ `read():256-261`: skips PCX RLE decode when `SaveMemory && Renderer==0 &&
sprite is large` — keeps RLE bytes, defers decode to render time
(`renderMugenZoom` handles `rle>0`). Go has no equivalent (always decodes).

---

## 4. Palette Loading — Differences

### 4.1 SFFv1

Both load per-sprite palettes inline during sprite iteration. Same.

### 4.2 SFFv2 palette table

- **Go** `loadPalettes()`: **dedupes** duplicate (group,number) palette keys
  via `uniquePals` map (logs WARN), tracks `duplicatePals`, stores per-palette
  `numcols`, allocates palette depth as power-of-2 clamped 16–256 (Respect
  original color count for RemapPal), forces alpha per `Version[2]`.
- **SSZ** `loadFile():694-735`: loads every palette entry (256 or `siz/4`
  colors), `link` resolves through `palList.get`, **no dedupe, no numcols, no
  alpha**. Duplicate keys overwrite silently in `palTable`.

**Worth porting:** dedupe + WARN (memory + RemapPal correctness for pads with
duplicate palette keys).

### 4.3 Selectable palette pre-allocation

SSZ `Sff.clear():674-684` pre-creates `NumCharPalletes` (12) palettes and
registers `palTable[1<<16|i]`. Go **removed** this (commented: "Pre-allocation
creates false positives when checking if a palette exists") and instead resolves
selectable palettes via `SelectablePalIndex(palNum)`. Behavioral difference in
RemapPal edge cases when a char has fewer than 12 palettes.

---

## 5. Sprite Storage & Linked Sprites

- **Go**: `map[[2]uint16]*Sprite`, duplicate keys → WARN + first-wins
  (`image.go:1741-1745`). Linked sprite (`size==0`): `shareCopy(src)` only when
  `indexOfPrevious < i`, else `palidx = 0` fallback (`:1708-1720`).
- **SSZ**: `IntTable.operate(key, lambda)` first-wins silently. `shareCopy`
  from `indexOfPrevious` **without the forward-link guard** — a malformed
  forward link reads `spriteList[negative]` (out of range → null sprite, but
  no diagnostic).

**Worth porting:** the `indexOfPrevious < i` guard + duplicate WARN.

---

## 6. GPU Texture Lifecycle — Biggest Architectural Gap

| | Go | SSZ |
|---|---|---|
| At load | pixels kept in `pendingData`, **no GPU upload** | GL renderers: `load8bitTexture`/`loadPngTexture` **immediately** |
| At render | `ensureTex()` uploads lazily on first draw | texture already resident |
| Large sprites | >128 KB uploaded eagerly in batches of 10, yielding to main thread (`image.go:1764-1815`) | n/a |
| Release | `releaseTextures()`, `Tex.Release()` | textures pooled (`texPool`/atlas) |
| Palette textures | per-sprite `CachePalTex` + FNV-64 `hashPal` change detection | palette atlas 256×256 rows, FNV-32 hash, LRU (`gl33PalSlotFor`) |

SSZ's palette atlas is actually the more advanced design (shared rows vs
Go's per-sprite palette textures); Go's lazy-upload is the more advanced
memory design. Different tradeoffs, both valid.

---

## 7. SSZ-Only Features (no Go equivalent)

- **`SffV2CacheGet/Put`** (`sff.ssz:272-273`): cross-compilation decoded-pixel
  cache — the menu and match are two separate SSZ programs; the cache shares
  decoded `^ubyte` buffers so `ikemen.sff` decodes once. Go's `SffCache` is
  commented out; Go doesn't need it (single process, GC'd pointers).
- **Renderer conditionals** (`?/*.cfg.Renderer ...`): compile-time per-backend
  pixel representation (`^&.sdl.GlTexture` vs `^ubyte`+`pluginbuf`). Go is
  GPU-only, single path.
- **`SaveMemory` gate** (§3.6).
- **`loadFromSff`** single-sprite loader kept alive for on-demand loads.

---

## 8. Porting Checklist (prioritized)

1. **fmt 0 uncompressed sprite support** in `readV2` — currently loads blank.
   Read `coldepth` (byte after fmt), handle 8 (raw indexes) like Go; 24/32
   needs a truecolor path that SW/DirectX renderers lack — 8-bit alone is a
   ~6-line fix.
2. **PCX `0x0C` palette-marker scan** in `read()` — fixes palettes on padded
   SFFv1 subheaders.
3. **Linked-sprite forward-link guard** in `loadFile` (`indexOfPrevious < i`
   else `palidx = 0`) — prevents null-sprite chains on malformed files.
4. **SFFv2 duplicate palette dedupe + WARN** — memory + RemapPal correctness.
5. (Optional) decoder bounds guards — crash-hardening for bad files.
6. (Optional, large) truecolor sprite path for Renderer 0/4 — Go's `SetRaw`
   equivalent; blocked on a 24/32-bit software blitter.

---

## 9. SSZ Idiom Notes (per `ikemen_ssz_scripting.md`)

- Error handling follows the repo convention: `^/char` error strings tested via
  `#call(...)=>err > 0` (e.g. `sff.ssz:472`, `:693`), vs Go's `error` returns.
- `plugin bool SffV2CacheGet(...)` — static-plugin call, no DLL (§10 of the
  language reference); the `<dll/ssz.dll>` path is only a registry key.
- `?/*.cfg.Renderer ...` conditional comments are compile-time: each renderer
  build compiles only one pixel representation into `&Sprite`.
- `spriteTable.operate(key, [void(^&.Sprite s=){...}])` = Go's
  "get-or-insert" map idiom; the lambda receives the existing entry or a fresh
  null ref.
- `%uint newSubHeaderOffset .= shofs` — list init-copy syntax (`%T .= scalar`
  seeds all future `.=` appends), equivalent to Go's
  `append(newSubHeaderOffset, shofs)` seeding.
