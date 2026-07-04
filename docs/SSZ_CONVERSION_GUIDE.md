# SSZ → C++ Native Conversion Guide

This guide describes the process of converting Ikemen GO SSZ scripts (`.ssz` files) into native C++ implementations. It is written for AI coding assistants and developers working on the native conversion project.

---

## Table of Contents

1. [Project Architecture](#1-project-architecture)
2. [SSZ Language Primer](#2-ssz-language-primer)
3. [Conversion Checklist](#3-conversion-checklist)
4. [File Templates](#4-file-templates)
5. [Type Mapping Reference](#5-type-mapping-reference)
6. [Common SSZ Patterns → C++](#6-common-ssz-patterns--c)
7. [Static Plugin Registration](#7-static-plugin-registration)
8. [The Bridge Layer](#8-the-bridge-layer)
9. [Build System](#9-build-system)
10. [Testing](#10-testing)
11. [Phase Reference](#11-phase-reference)
12. [Troubleshooting](#12-troubleshooting)

---

## 1. Project Architecture

### Directory Layout

```
ikemen-new-ultra/
├── main/                          # C++ source root
│   ├── main.cpp                   # Entry point — wires static registrations
│   ├── ssz/                       # SSZ JIT compiler
│   │   ├── bridge.cpp             # Old ABI → native C++ wrappers
│   │   ├── bridge.hpp             # Bridge helpers (refToWstring, etc.)
│   │   └── ...                    # Compiler internals
│   ├── ssz_native/                # ** YOUR TARGET ** — native services
│   │   ├── xxx_service.hpp        # Header: structs, functions
│   │   ├── xxx_service.cpp        # Implementation
│   │   └── ...
│   ├── xxx_static.hpp             # Static plugin registration header
│   └── ... (original .cpp files)
├── ssz_script/                    # ** INPUT ** — SSZ source scripts
│   ├── lib/                       # Library modules (file, math, string...)
│   └── ssz/                       # Gameplay modules (char, fight, stage...)
├── test/
│   └── test_file.cpp              # Test suite
├── Makefile                       # Build system
└── docs/
    ├── native_ssz_comparison.md   # Per-module scaffold status
    ├── TODO_SSZ_CONVERSION.md     # Detailed conversion status
    └── SSZ_CONVERSION_GUIDE.md    # THIS FILE
```

### Key Concepts

- **Static Plugin Architecture**: The engine has 14 subsystems (plugins). Each registers exported functions with the SSZ runtime via `SSZ_RegisterFunction()`.
- **Feature Flags**: Each module has an `IKEMEN_NATIVE_<NAME>_LIB=0/1` flag. Set to 0 to fall back to the original SSZ script. This enables A/B testing.
- **Bridge Layer**: `bridge.cpp` contains extern "C" wrappers that convert between the old SSZ plugin ABI (`PluginUtil*`, `Reference`) and the native C++ API.

### Conversion Flow

```
ssz_script/ssz/XXX.ssz    # 1. READ the SSZ source
        │
        ▼
main/ssz_native/xxx_service.hpp    # 2. CREATE header with data structures
main/ssz_native/xxx_service.cpp    # 3. CREATE implementation
main/xxx_static.hpp                # 4. CREATE/UPDATE static registration
main/ssz/bridge.cpp                # 5. UPDATE bridge wrappers
main/main.cpp                      # 6. UPDATE if new _static.hpp added
Makefile                           # 7. UPDATE if new .cpp files added
test/test_file.cpp                 # 8. ADD tests
```

---

## 2. SSZ Language Primer

SSZ is a custom scripting language with M.U.G.E.N.-specific features.

### Key Syntax

```ssz
// ── Module imports ──
lib m = <math.ssz>;           // Import from lib/
lib com = "common.ssz";        // Import from script/ssz/

// ── Variable declarations ──
public int coins = 0;                  // Module-level public variable
public float life = 1.0;
public ^/char name;                    // String pointer
public ^&.Sprite sprite;              // Object pointer (nullable)
public %int list;                      // Dynamic array (list)
public ^int ary;                       // Fixed-length array

// ── Enum definitions ──
|Key { B, D, F, U, a, b, c }          // Pipe prefix = enum type

// ── Struct definitions ──
public &Buffer                         // Ampersand prefix = struct type
{
  public int Bb, Db;                   // Public field
  /int internalField;                  // Slash = private field
  
  public void input(bool B, bool D)    // Method
  {
    // Method body
    if(B != (`B>0)){ `Bb = 0; `B *= -1; }
    `Bb += `B;                         // Backtick = `this` reference
  }
  
  public int keyState(|Key k)          // Enum parameter
  {
    switch(k){
    case .Key::B: ret `Bb;             // Dot notation = module member access
    case .Key::D: ret `Db;
    }
    ret 0;
  }
}

// ── Function declarations ──
public bool update()                   // Module-level function
{
  ret true;                            // `ret` = return
}

// ── Plugin bridge calls ──
plugin void MemMarkBefore(:^/char:) = <dll/ssz.dll>;
  // Calls a function from the SSZ runtime DLL

// ── Templates (generics) ──
public &Stack<_t>                      // Template struct
{
  public void push(^`_t data)          // Template parameter `_t`
  {
    // ...
  }
}

// ── Control flow ──
branch{                                // Switch/match
cond condition:
  doSomething();
else:
  doOther();
}

loop{index i = 0; while; do:          // Loop with index
  process(i);
  i++;
while i < 10:}                         // Loop condition (colon-terminated)

// ── Callbacks ──
~$void(^/char) callback = [void(^/char x){ process(x); }];
  // Lambda/delegate

// ── Null safety ──
if(#ptr == 0) ret;                     // `#` = length/null check
```

### SSZ Type System

| SSZ Type | C++ Equivalent | Notes |
|----------|---------------|-------|
| `int` | `int32_t` | 32-bit signed |
| `float` | `float` | 32-bit IEEE 754 |
| `bool` | `bool` | |
| `byte` | `int8_t` | 8-bit signed |
| `short` | `int16_t` | 16-bit signed |
| `uint` | `uint32_t` | 32-bit unsigned |
| `ubyte` | `uint8_t` | 8-bit unsigned |
| `ushort` | `uint16_t` | 16-bit unsigned |
| `long` | `int64_t` | 64-bit signed |
| `index` | `intptr_t` | Pointer-width |
| `^/char` | `std::string` | Null-terminated string |
| `^_t` | `T*` | Pointer / nullable reference |
| `%_t` | `std::vector<T>` | Dynamic array |
| `^_t` (array) | `std::vector<T>` | Also used for arrays |
| `&NameTable<_t>` | `table::NameTable<T>` | String-keyed hash table |
| `&IntTable<K,V>` | `table::IntTable<Key,T>` | Integer-keyed hash table |
| `\|EnumName` | `enum class EnumName` | C++ scoped enum |

---

## 3. Conversion Checklist

For each SSZ module being converted, follow these steps:

### Step 1: Read the SSZ Source
- Read the full `.ssz` file
- Identify ALL public declarations (functions, variables, structs)
- Identify ALL enum types and their values
- Understand the dependencies (which other modules it imports)
- Identify the internal state and how it's initialized

### Step 2: Create the Header (`xxx_service.hpp`)
- [ ] Add `#pragma once` and necessary `#include`s
- [ ] Define all enum types (use `enum class` with appropriate underlying type)
- [ ] Define all struct types with ALL fields matching SSZ
- [ ] Define all public methods on structs
- [ ] Declare module-level functions
- [ ] Declare module-level state struct
- [ ] Add backward-compatible type aliases if names changed

### Step 3: Create the Implementation (`xxx_service.cpp`)
- [ ] Implement ALL public functions (not just init)
- [ ] Implement ALL struct methods
- [ ] Initialize module-level static state
- [ ] Handle edge cases (empty input, null pointers, boundary values)
- [ ] Process: `implementation.cpp`

### Step 4: Create/Update Static Registration (`xxx_static.hpp`)
- [ ] Use `#if IKEMEN_NATIVE_<NAME>_LIB` guard
- [ ] Forward-declare `PluginUtil` and `Reference` if used
- [ ] Declare extern "C" bridge functions
- [ ] Create `xxx_static_register()` function
- [ ] Register ALL public bridge functions with `SSZ_RegisterFunction()`

### Step 5: Update Bridge (`bridge.cpp`)
- [ ] Add `#include "ssz_native/xxx_service.hpp"` inside `#if IKEMEN_NATIVE_<NAME>_LIB`
- [ ] For each public function that SSZ scripts call:
  - Write an `extern "C"` wrapper matching the old ABI
  - Use `ikemen::ssz_bridge::refToWstring(pu, ref)` to convert `Reference` → `std::wstring`
  - Use `pu->setSSZFunc()` + `pu->astrToRef(CP_UTF8, *out, str)` to return strings
  - Delegate to the native `ikemen::ssz_native::xxx_function()`

### Step 6: Wire in main.cpp
- [ ] Add `#include "xxx_static.hpp"` if new static header
- [ ] Add `if (!xxx_static_register()) { ... }` call

### Step 7: Update Makefile
- [ ] Add `$(SSZ_NATIVE)/xxx_service.cpp` to `MAIN_SRCS`

### Step 8: Add Tests
- [ ] Include the service header in `test/test_file.cpp`
- [ ] Create a `test_xxx_service()` function
- [ ] Add a call to it in `main()`
- [ ] Test:
  - Default initialization (structs have correct defaults)
  - Public API functions (return correct values)
  - Edge cases (empty input, boundary values, invalid params)

### Step 9: Update Documentation
- [ ] Run `tools/ssz_native_compare.ps1` to regenerate `docs/native_ssz_comparison.md`
- [ ] Update `TODO_SSZ_CONVERSION.md` with new behavior status

---

## 4. File Templates

### 4.1 Header Template (`xxx_service.hpp`)

```cpp
// xxx_service.hpp — Native C++ implementation for ssz_script/ssz/xxx.ssz
//
// xxx.ssz (NNNN lines) implements ... (one-line description).
//
// Phase 5: (what's implemented, what's deferred).

#pragma once

#include <string>
#include <vector>

namespace ikemen::ssz_native {

// ── Forward declarations ──
struct SomeOtherType;  // from another service header

// =========================================================================
// Enum types
// =========================================================================
enum class MyEnum : uint8_t {
    Value1, Value2, Value3
};

// =========================================================================
// Data structs
// =========================================================================
struct MyData {
    // ── Fields (match SSZ exactly) ──
    int field1{42};         // Default value matches SSZ default
    float field2{1.0f};
    std::string name;
    std::vector<int> list;  // SSZ %int

    // ── Methods ──
    void reset();
    void process(int arg);
};

// =========================================================================
// Module-level API
// =========================================================================
void xxx_init();
void xxx_do_something(const std::string& arg);
MyData& xxx_get_state();

} // namespace ikemen::ssz_native
```

### 4.2 Implementation Template (`xxx_service.cpp`)

```cpp
// xxx_service.cpp — Real implementations for xxx.ssz (Phase 5).
// Description of what was implemented.

#include "xxx_service.hpp"
#include "common_service.hpp"  // If you need common_load_text, etc.

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace ikemen::ssz_native {

// =========================================================================
// Module-level state
// =========================================================================
static MyData g_state;

// =========================================================================
// MyData
// =========================================================================

void MyData::reset() {
    field1 = 42;
    field2 = 1.0f;
    name.clear();
    list.clear();
}

void MyData::process(int arg) {
    // SSZ equivalent logic goes here
    field1 += arg;
}

// =========================================================================
// Module-level API
// =========================================================================

void xxx_init() {
    g_state = MyData{};
    g_state.reset();
}

void xxx_do_something(const std::string& arg) {
    // Implementation
    g_state.name = arg;
}

MyData& xxx_get_state() {
    return g_state;
}

} // namespace ikemen::ssz_native
```

### 4.3 Static Registration Template (`xxx_static.hpp`)

```cpp
// xxx_static.hpp — Static plugin registration for ssz_script/ssz/xxx.ssz
//
// When IKEMEN_NATIVE_XXX_LIB=1, replaces the SSZ xxx.ssz module
// with native C++ entry points registered in the static plugin table.

#pragma once
#include "static_plugin_registry.hpp"

#if IKEMEN_NATIVE_XXX_LIB

#include <cstdint>
struct PluginUtil;   // forward (from pluginutil.hpp)
struct Reference;    // forward (from arrayandref.hpp)

extern "C" {

void SSZ_STDCALL XxxInit(PluginUtil* pu);
void SSZ_STDCALL XxxDoSomething(PluginUtil* pu, Reference arg, Reference* out);

} // extern "C"

static inline bool xxx_static_register()
{
    SSZ_FunctionEntry entries[] = {
        { "init",        (void*)&XxxInit        },
        { "doSomething", (void*)&XxxDoSomething },
    };
    return SSZ_RegisterFunction("xxx", entries, 2);
}

#else

static inline bool xxx_static_register()
{
    return true; // Stub — SSX xxx.ssz used instead
}

#endif
```

### 4.4 Bridge Template (add to `main/ssz/bridge.cpp`)

```cpp
// =========================================================================
// XXX wrappers — old ABI -> native C++
// =========================================================================

#if IKEMEN_NATIVE_XXX_LIB
#include "ssz_native/xxx_service.hpp"

extern "C" void SSZ_STDCALL XxxInit(PluginUtil* pu)
{
    //SSZ_TRACE("XxxInit");
    (void)pu;
    ikemen::ssz_native::xxx_init();
}

extern "C" void SSZ_STDCALL XxxDoSomething(PluginUtil* pu, Reference arg, Reference* out)
{
    //SSZ_TRACE("XxxDoSomething");
    pu->setSSZFunc();
    std::wstring warg = ikemen::ssz_bridge::refToWstring(pu, arg);
    std::string argStr(warg.begin(), warg.end());
    ikemen::ssz_native::xxx_do_something(argStr);
}

#else
// When IKEMEN_NATIVE_XXX_LIB=0, the bridge wrappers don't exist
// and the xxx_static.hpp stubs provide no-op registration.
#endif
```

### 4.5 Test Template (add to `test/test_file.cpp`)

```cpp
// ---- XXX service tests (ssz_native::xxx) ----

static void test_xxx_service()
{
    std::wcout << L"\n--- XXX service ---" << std::endl;
    using namespace ikemen::ssz_native;

    xxx_init();
    TEST(L"xxx_init resets state", true);

    // Test field defaults
    MyData& state = xxx_get_state();
    TEST(L"MyData field default", state.field1 == 42);

    // Test methods
    state.process(5);
    TEST(L"MyData process", state.field1 == 47);

    // Test module-level API
    xxx_do_something("hello");
    TEST(L"xxx_do_something", state.name == "hello");
}
```

And in `main()`:
```cpp
    test_xxx_service();
```

---

## 5. Type Mapping Reference

### SSZ → C++ Type Mapping

| SSZ Expression | C++ Type | Example |
|---------------|----------|---------|
| `int` | `int32_t` or `int` | `int count;` |
| `float` | `float` | `float scale;` |
| `bool` | `bool` | `bool enabled;` |
| `byte` | `int8_t` | `int8_t b;` |
| `short` | `int16_t` | `int16_t s;` |
| `ubyte` | `uint8_t` | `uint8_t ub;` |
| `ushort` | `uint16_t` | `uint16_t us;` |
| `uint` | `uint32_t` | `uint32_t ui;` |
| `long` | `int64_t` | `int64_t l;` |
| `index` | `intptr_t` | `intptr_t idx;` |
| `^/char` | `std::string` | String |
| `^/char` (wide) | `std::wstring` | Wide string (for bridge) |
| `^int` | `int*` | Pointer to int (rare) |
| `%int` | `std::vector<int>` | Dynamic int array |
| `^^/char` | `std::vector<std::string>` | String array |
| `^%int` | `std::vector<int>*` | Pointer to vector |
| `^&.Type` | `Type*` | Nullable pointer to struct |
| `&.Type` | `Type` | Value type |
| `|Enum` | `enum class Enum` | Enum |
| `%^&.Type` | `std::vector<Type*>` | Array of pointers |
| `^uint` | `std::vector<uint32_t>` | uint32_t array (palette data) |
| `ubyte` (raw) | `std::vector<uint8_t>` | Byte array (pixel data) |

### Array Indexing

SSZ arrays are 0-indexed with length/safety built in:
```ssz
if(#arr > 0)           // Check not empty — C++: if (!arr.empty())
i %= #arr;             // Wrap index — C++: i %= arr.size()
ret arr[n];            // Access — C++: return arr[n];
arr.new[-1] = val;     // Append — C++: arr.push_back(val);
arr.new(N);            // Resize — C++: arr.resize(N);
#arr                   // Length — C++: arr.size()
```

### String Operations

```ssz
.s.equ(a, b)           // String equality — C++: a == b
.s.toLower(a)          // Lowercase — C++: to_lower(a)
.s.trim(a)             // Trim whitespace — C++: trim(a)
.s.find(p, s)          // Find — C++: s.find(p) or string::npos
#str                   // String length — C++: str.size()
```

---

## 6. Common SSZ Patterns → C++

### Pattern 1: Section Parser (reading .def / .cfg files)

**SSZ:**
```ssz
public void info(&.com.Section sc=)
{
  ^/char data;
  if(#(data = sc.get("name")) > 0){
    `name = data;
  }
  if(#(data = sc.get("displayname")) > 0){
    `displayname = data;
  }
}
```

**C++:**
```cpp
void parse_info(const std::string& body) {
    auto lines = common_split_lines(body);
    for (const auto& line : lines) {
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));
        if (key == "name") name = val;
        else if (key == "displayname") displayname = val;
    }
}
```

### Pattern 2: Plugin Bridge Call

**SSZ:**
```ssz
plugin void CompilerCompile(:index, ^/char, ^/char=:) = <dll/ssz.dll>;
^/char error;
CompilerCompile(:`ptr, file, error=:);
ret error;
```

**C++ (bridge):**
```cpp
extern "C" void SSZ_STDCALL CompilerCompile(
    PluginUtil*, Reference*, Reference, CompilerState*);

// Native call:
CompilerState* state = ...;
Reference fileRef, errorRef;
CompilerCompile(nullptr, &errorRef, fileRef, state);
std::string error = /* convert Reference to string */;
```

### Pattern 3: Enum Switch

**SSZ:**
```ssz
int keyState(|.Key k)
{
  switch(k){
  case .Key::B: ret `Bb;
  case .Key::D: ret `Db;
  case .Key::F: ret `Fb;
  case .Key::U: ret `Ub;
  }
  ret 0;
}
```

**C++:**
```cpp
int keyState(Key k) const {
    switch (k) {
    case Key::B: return Bb;
    case Key::D: return Db;
    case Key::F: return Fb;
    case Key::U: return Ub;
    default: return 0;
    }
}
```

### Pattern 4: Hash/NameTable

**SSZ:**
```ssz
public &.tbl.IntTable!uint, index? palTable;
```

**C++:**
```cpp
table::IntTable<uint32_t, int> palTable;
// Usage:
palTable.set(key, value);
int* v = palTable.get(key);
palTable.remove(key);
```

### Pattern 5: File I/O

**SSZ:**
```ssz
&.file.File f;
if(!f.open(fn, "rb")) ret .com.FileOpenError;
ubyte ub;
if(!f.read!ubyte?(ub=)) ret .com.FileReadError;
f.seek(offset, .file.Seek::SET);
```

**C++:**
```cpp
FileHandle f;
if (!f.open(to_wstring(fn), L"rb"))
    return "File open error";
uint8_t ub;
if (!read_pod(f, ub))
    return "File read error";
f.seek(offset, SeekOrigin::Set);
```

### Pattern 6: RLE/LZ Decompression

**SSZ:**
```ssz
public void rle8Decode(^ubyte px=)
{
  if(#px == 0) ret;
  ^/ubyte rle = px;
  px.new(`rct.w * `rct.h);
  loop{
    int leng = #px;
    index i = 0, j = 0;
  while;
  do:
    int size;
    ubyte d = rle[i++];
    if((d & 0xC0) == 0x40){
      size = (int)(d & 0x3F);
      d = rle[i++];
    } else {
      size = 1;
    }
    while; do: px[j++] = d; while --size >= 0:
  while j < leng:}
}
```

**C++:**
```cpp
void rle8Decode(std::vector<uint8_t>& px) {
    if (px.empty()) return;
    std::vector<uint8_t> src = std::move(px);
    px.assign(rct_w * rct_h, 0);
    size_t len = px.size();
    size_t i = 0, j = 0;
    while (j < len) {
        int size;
        uint8_t d = src[i++];
        if ((d & 0xC0) == 0x40) {
            size = static_cast<int>(d & 0x3F);
            d = src[i++];
        } else {
            size = 1;
        }
        do { px[j++] = d; } while (--size >= 0);
    }
}
```

### Pattern 7: Module State with Accessor

**SSZ:** Module-level variables are accessed directly.

**C++:**
```cpp
// In .cpp file:
static MyModuleState g_state;

// Public accessor:
MyModuleState& module_get_state() { return g_state; }
```

### Pattern 8: Template (Generic) Struct

**SSZ:**
```ssz
public &Stack<_t>
{
  public void push(^`_t data) { ... }
  public ^_t pop() { ... }
}
```

**C++:**
```cpp
template<typename T>
struct Stack {
    std::vector<T> data;
    void push(const T& item) { data.push_back(item); }
    T pop() { T t = std::move(data.back()); data.pop_back(); return t; }
};
```

### Pattern 9: Math Operations

| SSZ | C++ |
|-----|-----|
| `.m.sin(x)` | `std::sin(x)` |
| `.m.cos(x)` | `std::cos(x)` |
| `.m.max(a, b)` | `std::max(a, b)` |
| `.m.min(a, b)` | `std::min(a, b)` |
| `()` | `static_cast<T>()` |
| `#x` (abs) | `std::abs(x)` |
| `.m.rand(0, n)` | `math::rand(0, n)` (PRNG) |
| `.m.PI` | `math::PI` |
| `.m.E` | `math::E` |

### Pattern 10: Functions That Require SSZ Reference Types

Some native functions (e.g., `renderMugenZoom`, `renderMugenShadow`) take
`Reference` objects — SSZ VM types that hold pixel data or scratch buffers.
To call them from the native C++ API, construct temporary `Reference` objects
from `std::vector` data using `refnew()` and `memcpy()`, then clean up with
`releaseanddelete()` after the synchronous call returns.

**When to stub vs. wrap:**
- If the function is called by SSZ scripts via a `plugin void` bridge call,
  it goes through the bridge layer and the native service wrapper is reached
  only if called from other C++ code.
- When wrapping, include `sszdef.h` and `arrayandref.hpp` to access the
  `Reference` type and `sszrefnewfunc` allocator.

**SSZ:**
```ssz
plugin bool RenderMugenZoom(:^/ubyte, uint=, short, &.Rect=, float, float,
    &.Rect=, float, float, float, float, uint, int,
    &.Rect=, float, float, int, %byte=:) = "dll/sdlplugin.dll";
```

**C++ (native service, implementing with temporary References):**
```cpp
#include "sszdef.h"
#include "arrayandref.hpp"

static void vectorToRef(const std::vector<uint8_t>& src, Reference& ref) {
    ref.init();
    if (src.empty()) return;
    ref.refnew((intptr_t)src.size(), (intptr_t)sizeof(uint8_t));
    if (ref.len() > 0)
        memcpy(ref.atpos(), src.data(), src.size());
}

bool renderMugenZoom(const SdlRect& dr, float rcx, float rcy,
                     const std::vector<uint8_t>& pxl,
                     const std::vector<uint32_t>& pal,
                     int16_t ckey, const SdlRect& sr, ...,
                     std::vector<int8_t>& pluginbuf) {
    Reference imgRef, pluginbufRef;
    vectorToRef(pxl, imgRef);

    // Pad palette to 256 entries (native expects 256-color palette)
    std::vector<uint32_t> palBuf(256, 0);
    if (!pal.empty())
        memcpy(palBuf.data(), pal.data(),
               std::min(pal.size(), size_t(256)) * sizeof(uint32_t));

    // Convert SdlRect → SDL_Rects, then call native function
    SDL_Rect sdl_dr = { dr.x, dr.y, dr.w, dr.h };
    SDL_Rect sdl_sr = { sr.x, sr.y, sr.w, sr.h };
    // ... (parameter conversion for remaining SdlRects, floats, ints)

    bool result = RenderMugenZoom(&pluginbufRef, rle, rcy, rcx,
        &sdl_dr, alpha, roto, rasterxadd, yscl, xbotscl, xtopscl,
        &sdl_tile, ty, cx, &sdl_sr,
        ckey, palBuf.data(), imgRef);

    // Copy scratch buffer back to pluginbuf
    if (pluginbufRef.len() > 0) { /* refToVector pluginbufRef → pluginbuf */ }

    imgRef.releaseanddelete();
    pluginbufRef.releaseanddelete();
    return result;
}
```

**C++ (bridge, passes Reference directly):**
```cpp
extern "C" bool SSZ_STDCALL RenderMugenZoom(PluginUtil* pu,
    Reference* pluginbuf, int32_t rle, float rcy, float rcx,
    SDL_Rect* pdstr, int32_t alpha, ..., Reference img)
{
    (void)pu;
    return RenderMugenZoom(pluginbuf, rle, rcy, rcx, pdstr, alpha, ..., img);
}
```

---

## 7. Static Plugin Registration

### How It Works

1. Each module registers functions with the SSZ runtime via `SSZ_RegisterFunction()`
2. The function is called from `main.cpp` during startup
3. When `IKEMEN_NATIVE_<NAME>_LIB=1`, native functions replace SSZ scripts
4. When `=0`, the original SSZ script runs unchanged

### Feature Flag Pattern

In `Makefile`:
```makefile
IKEMEN_NATIVE_XXX_LIB ?= $(IKEMEN_USE_NATIVE_SSZ)
CXXFLAGS += -DIKEMEN_NATIVE_XXX_LIB=$(IKEMEN_NATIVE_XXX_LIB)
```

In `xxx_static.hpp`:
```cpp
#if IKEMEN_NATIVE_XXX_LIB
    // Real registration
#else
    // No-op stub
#endif
```

### Registration Call in main.cpp

```cpp
#include "xxx_static.hpp"

int main() {
    // ... startup ...
    if (!xxx_static_register()) {
        LOG_INFO("Ikemen", "Failed to register XXX functions");
        return 1;
    }
    // ... rest of startup ...
}
```

---

## 8. The Bridge Layer

### Purpose

`bridge.cpp` converts between:
- **Old SSZ ABI**: `PluginUtil*` + `Reference` (SSZ VM types)
- **New Native ABI**: Plain C++ types (`std::string`, `int`, `float`)

### Key Bridge Helpers

```cpp
namespace ikemen::ssz_bridge {

// Convert SSZ Reference → std::wstring
std::wstring refToWstring(PluginUtil* pu, Reference r);

// Convert SSZ Reference → UTF-8 std::string
std::string refToNarrowUtf8(PluginUtil* pu, Reference r);

} // namespace ikemen::ssz_bridge

// Convert std::string → SSZ Reference (output)
pu->setSSZFunc();
pu->astrToRef(CP_UTF8, *out, myString);

// Buffer management for binary data
pu->setSSZFunc();
pu->setArySize(*out, size);
memcpy(out->atpos(), data, size);
```

### Bridge Function Signature Pattern

```cpp
extern "C" ReturnType SSZ_STDCALL XxxFunction(
    PluginUtil* pu,        // Always first
    InputType arg,         // Input parameters
    Reference* out         // Output parameter (if any)
)
{
    pu->setSSZFunc();
    // Convert input
    std::wstring warg = ikemen::ssz_bridge::refToWstring(pu, arg);
    std::string nativeArg(warg.begin(), warg.end());
    
    // Call native
    std::string result = ikemen::ssz_native::xxx_function(nativeArg);
    
    // Convert output
    if (!result.empty())
        pu->astrToRef(CP_UTF8, *out, result);
}
```

---

## 9. Build System

### Building the Project

```powershell
# Set toolchain
$env:PATH = "C:\x86devkit\bin;$env:PATH"

# Release build
make CONFIG=Release -j8

# Debug build
make CONFIG=Debug install -j8

# Run tests
make CONFIG=Debug test -j8

# Build with specific modules disabled
make IKEMEN_NATIVE_SOME_MODULE_LIB=0 CONFIG=Debug -j8

# Show which modules are active
make native_manifest CONFIG=Debug

# Clean
make clean CONFIG=Debug
```

### Adding New Source Files

If you create a new `.cpp` file in `main/ssz_native/`, add it to `MAIN_SRCS` in the Makefile:

```makefile
MAIN_SRCS = \
  ... \
  $(SSZ_NATIVE)/new_service.cpp \
  ...
```

The Makefile automatically compiles `.cpp` files from `main/ssz_native/` to `.o` files in `build/<CONFIG>/main/ssz_native/`.

For the test binary, add to `TEST_FILE_OBJS`:

```makefile
TEST_FILE_OBJS = ... \
  $(BLD)/main/ssz_native/new_service.o \
  ...
```

---

## 10. Testing

### Test Framework

Tests use a simple macro-based framework in `test/test_file.cpp`:

```cpp
#define TEST(name, expr) do { \
    g_tests++; \
    if (!(expr)) { \
        g_fails++; \
        std::wcerr << L"FAIL: " << name << std::endl; \
    } else { \
        std::wcout << L"PASS: " << name << std::endl; \
    } \
} while(0)

#define TEST_EQ(name, expected, actual) do { ... }
#define TEST_INT(name, expected, actual) do { ... }
```

### Test Function Pattern

```cpp
static void test_xxx_service()
{
    std::wcout << L"\n--- XXX service ---" << std::endl;
    using namespace ikemen::ssz_native;

    // 1. Module init
    xxx_init();
    TEST(L"xxx_init", true);

    // 2. Default values
    MyData data;
    TEST(L"Default field1", data.field1 == 42);

    // 3. Method behavior
    data.process(5);
    TEST(L"process adds", data.field1 == 47);

    // 4. Edge cases
    data.reset();
    TEST(L"reset restores", data.field1 == 42);

    // 5. Module-level API
    xxx_do_something("test");
    TEST(L"module function", xxx_get_state().name == "test");
}
```

### Call the test from main():

```cpp
int main() {
    setup();
    // ... other tests ...
    test_xxx_service();
    // ... other tests ...
    cleanup();
    return g_fails > 0 ? 1 : 0;
}
```

### Compiling and Running the Test Binary

```bash
# Build and run all tests
make CONFIG=Debug test -j8

# Run just the test binary
./build/Debug/test_file.exe
```

---

## 11. Phase Reference

### Current Conversion Phases

| Phase | Description | Status |
|-------|-------------|--------|
| **P1** | Foundation: consts, math, string, table, stack, crypto | 🟢 Complete |
| **P2** | I/O: file, regex, socket, sound, ogg, mesdialog, alert, thread, time, shell, lua, sdlevent, sdlplugin, ssz | 🟡 Mostly done |
| **P3** | Game state: share, system, common, loader, debug-script, script, trigger-script, system-script, statebuilder | 🟡 Mostly done |
| **P4** | Gameplay: video, font, action, sound_resource, bg, stage, sff, command, fighting, fight, char | 🟡 Mostly done |
| **P5** | Config: save/config.ssz, save/configNet.ssz | 🟢 Complete |
| **P6** | Runtime: Retire bridge.cpp, remove SSZ dependency | ❌ Pending |
| **P7** | Parity: Behavioral equivalence verification | ❌ Pending |

### Behavior Status Key

| Symbol | Meaning |
|--------|---------|
| 🟢 | All public functions have real implementations and tests |
| 🟡 | Core functions implemented; some sub-features or edge cases deferred |
| ❌ | Stub-only (empty init function, placeholder data) |

### Module Dependency Map

```
consts → math → string → table → stack
                                          
file ─────────────────────────────────────────→ common → share
regex, socket, sound, ogg, mesdialog ────────→ common
alert, thread, time, shell ───────────────────→ common
lua, sdlevent, sdlplugin ────────────────────→ common

common → system → loader → char
                          → stage → bg
                          → fight → fighting
                          → sff → action
                          → command
                          → statebuilder
                          → font → video
                          → sound_resource

system_script, trigger_script, script ────────→ common + lua
debug_script ────────────────────────────────→ common + lua
```

---

## 12. Troubleshooting

### Build Errors

| Error | Likely Cause | Fix |
|-------|-------------|-----|
| `undefined reference to ikemen::ssz_native::Xxx::method()` | Missing .cpp file or not in Makefile `MAIN_SRCS` | Add file to Makefile |
| `cannot convert 'std::string' to 'const std::wstring&'` | `FileHandle::open()` takes `wstring` | Wrap with `to_wstring()` helper |
| `invalid use of incomplete type 'struct Xxx'` | Forward decl without full definition | Include the correct header or reorder definitions |
| `'M_PI' was not declared` | MSVC needs `_USE_MATH_DEFINES` | Use `math::PI` or define `_USE_MATH_DEFINES` |
| `'string' is not a member of 'std'` | Missing `#include <string>` | Add the include |
| `SSZ_STDCALL not declared` | Missing `sszdef.h` include | Include `sszdef.h` or `static_plugin_registry.hpp` |
| `PluginUtil not declared` | Missing forward declaration | Add `struct PluginUtil;` |
| `Reference not declared` | Missing forward declaration | Add `struct Reference;` (from arrayandref.hpp) |
| `Native service function returns false at runtime` | Function takes SSZ `Reference` objects internally | Keep as documented stub; SSZ scripts call through bridge layer, bypassing the native service |

### Runtime Issues

| Symptom | Likely Cause | Fix |
|---------|-------------|-----|
| SSZ module not found | `IKEMEN_NATIVE_XXX_LIB=0` or registration not called | Check feature flag, add `xxx_static_register()` to `main.cpp` |
| Bridge function not called | Function not registered in `SSZ_RegisterFunction` | Add function name to the entries array in `xxx_static.hpp` |
| Wrong function resolved | Name mismatch in registration | Registration name must match the name the SSZ script uses |
| Crash in bridge | Incorrect `Reference` conversion | Check `pu->setSSZFunc()` is called before string operations |
| Test binary linker errors | Missing `.o` in `TEST_FILE_OBJS` | Add object file to Makefile test section |

### Common Mistakes

1. **Forgetting to call `pu->setSSZFunc()`** before accessing `Reference` objects in bridge functions
2. **Using wrong string type**: `FileHandle::open()` needs `std::wstring`, not `std::string`
3. **Missing forward declarations**: `PluginUtil` and `Reference` must be forward-declared in `xxx_static.hpp`
4. **SSZ `#` operator**: In SSZ, `#` on an integer returns absolute value (`abs()`). Use `std::abs()` in C++.
5. **SSZ modulo**: `a % b` in SSZ uses `#a` (absolute) on the result. Always handle negative modulo correctly.
6. **Array bounds**: SSZ often wraps indices: `i %= #arr`. Do the same in C++.
7. **Nested namespaces**: Some services use `ikemen::ssz_native` (most), others use `ikemen::ssz_native::table` (table_service), `ikemen::ssz_native::math` (math_service). Check each.
8. **SSZ Reference wrappers**: Functions that take `Reference` objects (SSZ VM types) can be wrapped by including `arrayandref.hpp` and constructing temporary `Reference` objects from `std::vector` data using `refnew()` + `memcpy()`. Clean up with `releaseanddelete()` after the synchronous call.
9. **SSZ references**: `^&.Type` is a nullable pointer. Always check for null before dereferencing.
10. **Default values**: Match SSZ defaults exactly (e.g., `int x = -1`, `float y = 0.0`).

---

## Appendix A: Quick Reference

### File Index by Module

| SSZ Module | Header | Implementation | Static Reg | Test Order |
|-----------|--------|---------------|------------|------------|
| `lib/consts.ssz` | `consts.hpp` | — | — | — |
| `lib/math.ssz` | `math_service.hpp` | `math_service.cpp` | `math_static.hpp` | Early |
| `lib/string.ssz` | `string_service.hpp` | `string_service.cpp` | `string_static.hpp` | Mid |
| `lib/table.ssz` | `table_service.hpp` | — | — | Mid |
| `lib/stack.ssz` | `stack_service.hpp` | `stack_service.cpp` | `stack_static.hpp` | Late |
| `lib/file.ssz` | `file_service.hpp` | `file_service.cpp` | `file_static.hpp` | Early |
| `lib/sound.ssz` | `sound_service.hpp` | `sound_service.cpp` | `sound_static.hpp` | Late |
| `lib/alpha/lua.ssz` | `lua_service.hpp` | `lua_service.cpp` | `lua_static.hpp` | Late |
| `lib/alpha/sdlplugin.ssz` | `sdlplugin_service.hpp` | `sdlplugin_service.cpp` | `sdlplugin_static.hpp` | Late |
| `lib/alpha/sdlevent.ssz` | `sdlevent_service.hpp` | `sdlevent_service.cpp` | `sdlevent_static.hpp` | Late |
| `ssz/common.ssz` | `common_service.hpp` | `common_service.cpp` | `common_static.hpp` | Mid |
| `ssz/share.ssz` | `share_service.hpp` | `share_service.cpp` | `share_static.hpp` | Mid |
| `ssz/system.ssz` | `system_service.hpp` | `system_service.cpp` | `system_static.hpp` | Mid |
| `ssz/loader.ssz` | `loader_service.hpp` | `loader_service.cpp` | `loader_static.hpp` | Late |
| `ssz/stage.ssz` | `stage_service.hpp` | `stage_service.cpp` | `stage_static.hpp` | Late |
| `ssz/sff.ssz` | `sff_service.hpp` | `sff_service.cpp` | `sff_static.hpp` | Late |
| `ssz/bg.ssz` | `bg_service.hpp` | `bg_service.cpp` | `bg_static.hpp` | Late |
| `ssz/char.ssz` | `char_service.hpp` | `char_service.cpp` | `char_static.hpp` | Late |
| `ssz/command.ssz` | `command_service.hpp` | `command_service.cpp` | `command_static.hpp` | Late |
| `ssz/fight.ssz` | `fight_service.hpp` | `fight_service.cpp` | `fight_static.hpp` | Late |
| `ssz/fighting.ssz` | `fighting_service.hpp` | `fighting_service.cpp` | `fighting_static.hpp` | Late |
| `ssz/statebuilder.ssz` | `statebuilder_service.hpp` | `statebuilder_service.cpp` | `statebuilder_static.hpp` | Late |
| `ssz/action.ssz` | `action_service.hpp` | `action_service.cpp` | `action_static.hpp` | Late |
| `ssz/sound.ssz` | `sound_resource_service.hpp` | `sound_resource_service.cpp` | `sound_resource_static.hpp` | Late |
| `ssz/font.ssz` | `font_service.hpp` | `font_service.cpp` | `font_static.hpp` | Late |
| `ssz/video.ssz` | `video_service.hpp` | `video_service.cpp` | `video_static.hpp` | Late |

### Test Execution Order

Tests run in a specific order to ensure dependencies are loaded:
1. `test_math_service()` (P1)
2. `test_string_service()` 
3. `test_math()` (original C math)
4. `test_consts_service()` (P1)
5. `test_table_service()` (P1)
6. `test_stack_service()` (P1)
7. `test_share_service()` (P3)
8. `test_system_service()` (P3)
9. `test_common_service()` (P3)
10. `test_loader_service()` (P3)
11. `test_fight_service()` (P4)
12. `test_char_service()` (P4)
13. `test_command_service()` (P4)
14. `test_stage_service()` (P4)
15. `test_bg_service()` (P4)
16. `test_sff_service()` (P4)
17. `test_action_service()` (P4)
18. `test_sound_resource_service()` (P4)
19. `test_fighting_service()` (P4)
20. `test_statebuilder_service()` (P4)
21. `test_font_service()` (P4)
22. `test_video_service()` (P4)
23. `test_startup_parity()` (P6)

## Appendix B: SSZ Reserved Keywords

These are SSZ keywords that should NOT be used as C++ identifiers:
- `ret` → `return`
- `branch` → `switch/if-else`
- `cond` → `case`
- `loop` → `while/for`
- `new` → constructor or `resize()`
- `break` → Same in C++
- `continue` → Same (but SSZ uses `continue` in a special way in loops)
- `lock` → mutex guard

---

*Generated: 2026-07-05*
*For more details, see [TODO_SSZ_CONVERSION.md](./TODO_SSZ_CONVERSION.md) and [docs/native_ssz_comparison.md](./native_ssz_comparison.md)*
