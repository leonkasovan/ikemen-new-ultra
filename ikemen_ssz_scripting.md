# SSZ Scripting Language Reference

SSZ is the scripting language used by I.K.E.M.E.N. (the M.U.G.E.N game engine fork).
It is a **statically typed**, **JIT-compiled** language: scripts are parsed into
syntax trees (`main/ssz/sourcetree.hpp`) and compiled directly to native x86 machine
code at runtime (`main/ssz/jitcompiler.hpp` + `main/ssz/x86.hpp`). There is no
interpreter and no bytecode VM — a `.ssz` file becomes executable code as it loads.

This document describes the language as implemented. Examples are taken from the
bundled scripts in `ssz_script/`.

---

## 1. Files, Modules and Execution

- A script is a UTF-8 text file with the `.ssz` extension, parsed as UTF-16 internally.
- The engine entry script is `ssz/ikemen.ssz`, passed on the command line
  (`ikemen-debug.exe ssz/ikemen.ssz`).
- Each `.ssz` file is an independent **module** (a *file root*). The file that gets
  compiled first is the *root module*; everything it imports becomes part of the
  same program.
- `lib name = <path.ssz>;` imports another module. `name` is the local alias used to
  reference the imported module's public members:

  ```ssz
  lib consts = <consts.ssz>;
  lib s = <string.ssz>;
  ```

  The path in `<...>` is relative to the importing file.
- One program = one compiled root. All `lib` imports are compiled into the same
  address space; global variables are shared across the program.

### Name lookup prefixes

| Prefix | Meaning |
|--------|---------|
| *(none)* | local variable → enclosing scope → member/global in scope |
| `.` | member of the **root** module scope (must be public) |
| `@` | member of the **current file** scope (public within this file) |
| `` ` `` | member of the **enclosing class instance** (`self`) |
| `~` | member access **through a class reference** (`^&Class` value) |

Examples:

```ssz
.m.PI           // global constant PI in the imported "m" (math) module
@com.sectionName // public member of this file's scope
`x = 0;         // write member x of the current class instance
p~c~stVal.hit=   // access field c then stVal.hit through reference p
```

### Comments

```ssz
// line comment
/* block comment */
```

### Conditional comments (`?/* ... */`)

A compile-time `#if`-like construct. Inside a block comment that starts with
`?/*`, branches are selected by constant boolean expressions:

```ssz
/?/*typeid(_t) < 0:
    ret false;
  /*true:
    x = x;                     // runs only when the condition above is false
  /*?*/
```

- Starts with `?/*`, then `/* <constexpr>: ...` marks a branch.
- Each branch's condition is a constant expression (`true:`, `false:`, `typeid(_t) < 0:`, ...).
- `/*?*/` closes the construct.
- Only the first branch whose condition is true is compiled.

---

## 2. Lexical Elements

### Identifiers

Identifiers start with a letter, `_`, or any non-ASCII character (`>= 0xC0`), and
continue with letters, digits, `_`, or non-ASCII characters. Identifiers **may not
start with a digit**. Names ending in `_t` are reserved for template parameters.
Identifiers starting with a digit are rejected ("Identifiers starting with numbers
cannot be used.").

### Keywords

```
bool     branch   break    byte     case     cast     char     comm
cond     const    continue core     default  delete   diff     do
double   else     false    float    func     if       index    len
lib      list     lock     long     loop     method   new      plugin
public   ref      ret      self     short    signature switch  thread
true     type     typeid   typesize ubyte    uint     ulong    ushort
void     while
```

Special tags (used as `<tag>(...)`):

```
consteval   idname   wait
```

> Note: `len` has a `LEN_TOKEN` in the token kind enum (`tokenkind.h`) but is **not
> recognized by the tokenizer** (`source.hpp::MojiretsuToken` has no case for it)
> and is **not implemented** by the compiler. Do not use `len(...)` — use `#ref`
> for the length of a ref/list.

### Number literals

| Form | Example | Meaning |
|------|---------|---------|
| `0x...` | `0xFF`, `0x0` | hexadecimal (prefix `x`) |
| `0b...` | `0b11000000` | binary (prefix `b`) |
| `0o...` | `0o755` | octal (prefix `o`) |
| `0d...` | `0d63`, `0d10` | explicit decimal (prefix `d`) |
| plain | `42`, `0` | decimal integer (`long`) |
| float | `3.14`, `1.5e10`, `2.0E-3` | decimal point / exponent |

- A number with a decimal point or exponent is a `double`.
- `!0x0` is bitwise-NOT zero = all-bits-one (`-1` for signed, `MAX` for unsigned) —
  commonly used for sentinel values.

### Character literals

Single quotes: `'a'`, `'0'`, `'\n'`, `'\x7f'`. Type is `char` (a UTF-16 code unit).
A char literal can be used wherever an `int` is accepted.

### String literals

Double quotes: `"abc"`, `""` (empty). Type is `^/char` (read-only reference to char
array). Strings may contain escape sequences (`\n`, `\xNN`, ...).

### Boolean literals

`true`, `false`.

---

## 3. Types

### Primitive types

| Type | Size | C equivalent |
|------|------|--------------|
| `byte` | 1 | `int8_t` |
| `ubyte` | 1 | `uint8_t` |
| `short` | 2 | `int16_t` |
| `ushort` | 2 | `uint16_t` |
| `int` | 4 | `int32_t` |
| `uint` | 4 | `uint32_t` |
| `long` | 8 | `int64_t` |
| `ulong` | 8 | `uint64_t` |
| `char` | 2 | `char16_t` (UTF-16 unit) |
| `bool` | 1 | `bool` |
| `float` | 4 | `float` |
| `double` | 8 | `double` |
| `index` | pointer | `intptr_t` (address / pointer-sized) |
| `void` | 0 | — |

All primitive types can be cast between each other with `(type)expr`:

```ssz
(uint)h << 0d10
(char)(u >> 0d4)
(long)i
(float).random()
```

Enums are also castable to/from `int`.

### Class types (`&`)

`&Name` is a class type. Class objects live on the heap and are managed by
reference counting plus a circular collector — you never free them manually.

```ssz
&Compiler          // a class object
&.soc.Socket       // a class from another module ("soc")
&NameTable!_t?     // a template class instantiation
```

A **reference to a class object** is written `^&Name` (ref-to-class). Member access
through such a reference uses `~`:

```ssz
^.soc.Socket s;
s.close();
ret s.soc != .INVALID_SOCKET;
```

### Enum types (`|`)

`|Name` is an enum type, stored as a 32-bit integer. Members are accessed with `::`:

```ssz
|.NetState st;
st = .NetState::Playing;
switch(`st<>){
case .NetState::Error: ...
}
```

### Reference types (`^`, `%`)

- `^T` — a **reference**: a fixed-length, heap-allocated array of `T`, with an
  implicit length. Assigning copies the array (deep copy); `^` types are
  value-semantic containers.
- `%T` — a **list**: a growable dynamic array of `T`.
- `^/T` — a **read-only reference**: elements may be read but not written.
- A `^`/`%` may also hold refs/lists of refs: `^/^/char`, `%^/char`, `%&.KeyBuffer`.
- The **element type** of a ref/list can be any type, including class types
  (`^&Char`, `%&Node`), enums, other refs, threads, or template parameters.

`^` vs `%`:

| Operation | `^T` (ref) | `%T` (list) |
|-----------|------------|-------------|
| length | `#r` | `#l` |
| index | `r[i]` | `l[i]` |
| slice | `r[i..j]` | `l[i..j]` |
| allocate | `r.new(n)` | `l.new(n)` |
| append one | — | `l .= x` |
| grow by 1 and index | `r.new[-1]` | `l.new[-1]` |
| concatenate (`+`) | `r1 + r2` | `l1 + l2` |

Assigning a `^T` copies the data. Assigning a `%T` copies the list.

### Dynamic types (`ref`, `list`)

`ref T` / `list T` are **dynamically-typed** references: they store a value plus a
runtime type tag. Useful for heterogeneous containers and for interacting with
plugin APIs. Runtime operations:

| Expression | Meaning |
|------------|---------|
| `d.cast(var=)` | copy into a typed ref `var`; returns `bool` (false if types mismatch) |
| `typeid(d)` | the runtime type id as an `int` |
| `typesize(d)` | size in bytes of one element |

```ssz
ref r = L.toRef(argc);
^_t obj;
if(r.cast(obj=)) { ... }
```

### Function and delegate types

- `func!name?` — a value referring to the function `name`.
- `method!name?` — a value referring to a method.
- `$ret(params)` — a **signature** (function type), e.g. `$void(^/char)`.
- `~$ret(params)` — a **delegate** type (function pointer + optional captured env).
- `thread!func?` — a thread running function `func`.
- `thread!.method?` — a thread running method `method` of a class.

Delegate variables:

```ssz
~$void(^`_t=) d;     // delegate taking ^_t by reference
~$void(^/char, ^`_t) d;
d(:ary[i]=:);         // invoke
```

Anonymous functions (lambdas/closures) are written `[params]{ body }`:

```ssz
el:<-[void(i=){`add(i);}];          // side-effect lambda
spl:<-[_t(s){ret .atonOF!_t?(s);}]; // mapping lambda
[bool(&.Hitdef h=){ ... }]          // lambda taking a class object by reference
```

### Thread types

```ssz
thread!load? loadThread;      // thread that runs function "load"
thread!.sendThread? sen;      // thread that runs method "sendThread" of the enclosing class
```

Launch a thread with the `..` (double-dot) postfix operator on the thread variable; wait with `<wait>(t)`:

```ssz
.loadThread..();              // start the thread (.. is postfix on the variable)
<wait>(.loadThread);          // block until the thread finishes
```

A thread function must return `void` (or the return value is discarded). Threads
cannot be passed by reference or returned from functions that return references.

### Type aliases (`type`)

```ssz
type table_t = &Table!_t, self?;
type byte_t  = &Signed!byte?;
```

`type name = <type>;` introduces a shorthand usable anywhere a type is expected.

---

## 4. Variables and Scope

### Global variables

Declared at file top level. A global constant must be initialized:

```ssz
const double PI = 3.1415926535897932;
const int RANDMAX = .consts.int_t::MAX;
int randseed = (time.unixTime() ^ (long)time.tickCount()<<16) & RANDMAX;
```

- Unadorned globals are **private to the file**; `public` (or `@` for same-file)
  exports them.
- Global initializers are constant expressions (compile-time evaluable).
- **Order matters**: a function may only use global variables that are initialized
  *before* it in the file, or it is a compile error
  ("A function that uses global variables that may not have been initialized.").
- A `ret` at global scope (with a value) stops subsequent global definitions.

### Member variables (class fields)

```ssz
public &Format {
  ^char fmt;
  public %char out;
  /char next = '\0';
  /index soc = .INVALID_SOCKET;
}
```

- Fields may have default values (constant expressions).
- Access from methods via `` `name ``.
- `public` / `@` fields are accessible from outside; `/` fields are read-only from
  outside the class (writes only within the class).

### Local variables

Declared with a type anywhere inside a block:

```ssz
int w = 0;
^/char tmp = .trim(s);
%char buf;
loop{index i = 0; ... }
```

- Local refs/lists/class objects are automatically released (reference count
  decremented, `delete()` run for class objects) when the block exits, on `ret`,
  `break`, etc. You do not free them manually.
- `const` locals must be initialized and cannot be assigned.

### `self` and `\`self`

Inside a class, `` ` `` refers to the enclosing instance. The type of the enclosing
class is written `` `self ``:

```ssz
public bool accept(`self s=, int timeout, bool nodelay) { ... }
```

---

## 5. Operators

### Precedence (high → low)

1. primary / postfix: `()`, `[]`, `.`, `~`, `<>`, `..()`, `.new`, `:<-`
2. unary prefix: `+ - ! ~ # ? ''` and casts `(T)expr`
3. `**`
4. `* / %`
5. `+ -`
6. `<< >>`
7. `< <= > >=`
8. `== !=`
9. `&`
10. `^`
11. `|`
12. `&&`
13. `||`
14. `? :` (ternary)
15. assignment: `= += -= *= /= %= **= <<= >>= &= ^= |= .= =>`
16. `,` (also used for the `break, break` multi-statement)

### Arithmetic

| Operator | Meaning |
|----------|---------|
| `+ - * /` | add/sub/mul/div (all numeric types; sized integer ops) |
| `%` | remainder (integer types) |
| `**` | power — **float/double only** (`10.0**(double)-accu`) |
| `++ --` | pre/post increment/decrement |
| `#x` | **absolute value** when `x` is a number |

Division/modulo are signed or unsigned depending on the operand type; `index`/`int`
use 32-bit ops, `long`/`ulong` 64-bit ops, `byte`/`short` sized ops.

### Comparison / logical

| Operator | Meaning |
|----------|---------|
| `== !=` | equality/inequality |
| `< <= > >=` | ordering |
| `&&` | logical AND (short-circuit) |
| `||` | logical OR (short-circuit) |
| `!` | logical NOT (`!x`, `!flag`) |
| `!!` | toggle (XOR 1) |
| `? :` | ternary conditional: `x > 0 ? a : b` |

`#bool` is identity; `!0` on numbers is bitwise complement (all ones).

### Bitwise

`& | ^ ~ << >>` — bitwise AND/OR/XOR/complement/shifts. `~x` on an integer is
bitwise NOT. Signed right shift is arithmetic.

### Reference / list operators

| Operator | Meaning |
|----------|---------|
| `#r` | element count of a ref/list |
| `r[i]` | element at index `i` (negative = from the end, `r[-1]` is last) |
| `r[i..j]` | slice `i` to `j` (negative indices count from the end; `r[1..-1]`) |
| `r + s` | concatenation (new ref/list) |
| `l .= x` | append element `x` to list `l` |
| `l .= r` | append all elements of ref `r` to list `l` |
| `r.new(n)` | (re)allocate to `n` elements |
| `l.new(n)` | resize list to `n` elements |
| `r.new[-1]` | grow by one and address the last element |
| `l.new[expr] = v` | grow list to index `expr` and store |
| `x<>` | lvalue reference to the **first element** of ref/list `x` (data pointer + element type). Readable, writable (`pxl<> = 0`), and usable for member access (`ref<>~field`). Passing a ref directly to a plugin call passes its data buffer. |
| `x~m` | access member `m` through a `^&Class` reference |
| `list:<-lambda` | apply `lambda` to each element, collect results |
| `list:<-:lambda` | apply to elements of elements (nest one level) |
| `#expr=>var` | assign result to `var`, keep value (error propagation idiom) |

Slicing copies the selected range into a new ref.

### The `=>` operator

`expr => lvalue` evaluates `expr`, stores it into `lvalue`, and the whole expression
evaluates to the stored value. Used heavily to capture an error string while
testing its length:

```ssz
if(#.com.loadText(def, unicode=)=>mainbuf == 0) ...
#chara.build(p~playerno, cha, .code[pn]=)=>error > 0
```

### Assignment

`=` and compound `+= -= *= /= %= **= <<= >>= &= ^= |= .=`. The list-append `.=`
appends. Strings/refs support `+=`? No — use `+` for concatenation and `.=` for
append:

```ssz
buf .= '0' + (char)(uinte >> shift & 0x7);   // append a char
jol .= src[i];                                // append an element
str = "a" + "b";                              // concatenate
`out .= `fmt[0 .. peridx];                    // append a range
```

### `?` temporary reference / stringify

- `''value` converts a scalar to a string (`^/char`): `''i`, `''u`, `''#ne`.
- `?value` creates a temporary reference to the value; commonly used to pass a
  scalar where a string/ref is expected (e.g. `putStr(?c)` prints the single char `c`).

### Address and pointer operators

- `x<>` — lvalue reference to the first element of a ref/list; used to touch the
  buffer or to reach a class object stored in an array (`arr[i]<>~field`).
- Passing a `^T` / `%T` value directly to a plugin function passes its data buffer;
  a scalar passed by reference (`x=`) passes its address.

---

## 6. Statements

### Expression statement

```ssz
`x += 1;
foo();
```

### Variable / const declaration

```ssz
int n = 0;
^/char s = "abc";
const int MAX = 100;
```

### Block

`{ ... }` opens a new scope; locals declared inside are destroyed at `}`.

### `if` / `else`

```ssz
if(cond){ ... }
if(cond){ ... } else { ... }
```

`if` does not require braces around the body — the body runs until `;` or a block
(also see `break`-driven forms below).

> **⚠ Compiler quirk:** `if(cond){...} else {...}` blocks are **unreliable**
> inside class methods when they contain local variable declarations, `loop{}`
> blocks, or appear nested inside `branch{}` / `loop{}` constructs. The compiler
> reports `"You can't do that here."` or `"Syntax error."` at the `}else{` line.
> Use `branch{cond ... else: ...}` instead (see below) — it handles all these
> cases correctly. Simple `if(cond){...}` without `else` is always safe.

### `switch`

```ssz
switch(expr){
case value1, value2:
  ...;
default:
  ...
}
```

- The switch value can be any primitive, char, bool, float/double, or enum.
- **Cases do not fall through** — each case body is entered only for its values.
  Compile the desired fallthrough explicitly (e.g. by chaining conditions).
- `case` values must be unique constants. A case may list several comma-separated
  values.
- `default:` is optional.
- Inside a switch, `break` exits the switch. `ret`, `break`, `continue` are valid.

### `branch` (multi-way conditional, switch-like)

```ssz
branch{
  ^/char a = .s.trim(str);
cond<minus> #a > 0 && a<> == '-':
  a = a[1 .. -1];
cond<plus> #a > 0 && a<> == '+':
  a = a[1 .. -1];
comm:
  loop{ ... }                 // common code after any cond
diff<minus>:
  n *= -1;                    // extra code only if <minus> was chosen
}
```

- `cond<label> expr:` — labeled condition. Evaluate `expr`; if true, run the body
  and record `label` as chosen.
- `cond expr:` — unlabeled condition.
- `else:` — body runs when no condition matched.
- `comm:` — common body run after whichever condition matched (or `else`).
- `diff<label>:` — body run only when the specific label was chosen.
- The compiler builds a jump table; each condition is evaluated only until one
  matches. Local declarations are allowed at the top of the branch block.
- `break` exits the whole `branch`.

> **Preferred over `if/else`** for all conditional logic inside class methods.
> `branch{cond ... else: ...}` is a reliable two-way conditional, unlike
> `if/else` which has compiler quirks (see §6 `if / else` above). It also
> supports local variable declarations and nested `loop{}` blocks without
> issues. Use `branch{cond<skip> true: ...}` for an unconditional skip-out if
> needed.

### `loop` (universal loop)

```ssz
loop{...}
loop{ index i = 0; while; do:
  ...;
while i < n:}
```

`loop{...}` is an infinite loop unless `break` is used. The full form has three
parts, introduced by `while;` / `do:` / `while cond:`:

```ssz
loop{
  index i = 0;        // init (before first while)
  while;              // (optional) test marker
do:
  body;               // body
continue:             // (optional) continue marker
  i++;
while i < #str:       // (optional) loop condition
}
```

The pieces can appear in any order and be repeated:

```ssz
loop{index i = 0; do:
  spl.new[-1] = ...;
  i++;
while i < #str:}

loop{continue; do:
  shift -= 0d3;
continue:
  buf .= '0' + (char)(uinte >> shift & 0x7);
while shift != 0x0:}
```

- `while;` at the top runs the following condition before the first iteration.
- `do:` marks the start of the body.
- `continue:` marks code that runs before the loop condition on each iteration.
- `while cond:` is the loop condition.
- `break` exits the loop (running cleanup). `break, break` breaks nested blocks
  (see multi-statements). `continue` jumps to the loop condition.
- `break, do;` jumps to the `do:` body (used to re-enter the loop from a `break`
  inside a nested block).

> **⚠ Variable declarations in `do:` blocks:** variables declared with
> initialization (`uint x = expr;`) inside a `loop{...do:...}` block's `do:`
> section cause `"It cannot be defined here."` errors. Declare variables
> **before** the `loop{}` block (in the init section or the enclosing scope),
> then assign inside `do:`. Bare declarations without initialization
> (`uint x;`) at the top of the loop block (before `while;`) are fine.

### `ret` (return)

```ssz
ret expr;      // return a value
ret;           // return void
```

`ret` at global scope (with a value) ends global initialization. Inside a function
`ret` unwinds all enclosing blocks, releasing locals (calling `delete()` for class
objects).

### `break` / `continue`

```ssz
break;        // exit innermost breakable block
continue;     // next loop iteration
break, break; // exit two levels (e.g. switch inside loop)
break, do;    // exit current block and jump to the loop's do: body
```

Multiple statements can be chained with commas — a statement list like
`break, break;` or `break, do;` performs several control-flow actions. `break`
also appears after `case` bodies implicitly (each case exits the switch).

### `lock`

```ssz
lock(expr, ...){ ... }
```

Acquires a lock on each referenced object (ref/list/dynamic ref/thread) for the
duration of the block. Used to synchronize access from threads:

```ssz
lock(`p){
  `p[pn].selchr.new[-1];
  ...
}
lock(.sc.syst.selinf.p){ ... }
```

### `delete`

The `delete` statement is generated automatically by the compiler when a scope ends,
a `break`/`ret` fires, or a reference count drops: it releases refs/lists and calls
the class destructor (`delete()` method). You generally do not write it; the
compiler inserts it for every non-`const` ref/list/class local as the block closes.

### `cast`

`dynref.cast(typedvar=)` — copy a dynamic ref into a typed ref, checking the runtime
type; returns `bool`:

```ssz
ref r = L.toRef(argc);
^_t obj;
if(r.cast(obj=)){ ... }
```

---

## 7. Functions

### Declaration

```ssz
[decorator] returntype name(param, ...) { body }
```

```ssz
public double sin(double x){ ... }
void sffNew(&.lua.State L=, int re=){ ... }
public bool sToNumber<_t>(_t d=, ^/char s){ ... }
```

- Decorators: `public` (exported to other modules), `@` (public in this file),
  or none (private to the file). `/` marks read-only refs in signatures.
- Return type is declared before the name.
- `void` functions may omit a value in `ret`.

### Parameters

- Parameters are passed **by value**, unless marked with a trailing `=` for
  **pass-by-reference** (the callee can modify the caller's variable):

  ```ssz
  public void limMax<_t>(_t x=, _t y){ x = .min!_t?(x, y); }
  ```

  A `=` argument requires the caller to pass a variable, not a constant
  ("Must be passed by reference.").
- Function pointer/ref parameters and thread parameters have restrictions: threads
  cannot be passed by reference; anonymous functions cannot be passed to threads.
- There are no default argument values; instead pass-by-reference (`=`) with an
  optional out value is the common idiom.

### Calling

```ssz
Sin(:x:);                        // plugin / native call syntax
.m.min!double?((double)x, y);    // template function call
d(:ary[i]=:);                    // delegate invocation (args delimited by (: :)
.r.load(cha)=>error;             // call, assign error out, test
swap!(a, b);                     // template call
`setNext();                      // method call on self
p~c~stVal.hit=;                  // method call through class reference
```

- `(: arg1, arg2 :)` is the explicit argument-list syntax used for plugin functions
  and delegates; arguments may carry `=` for by-reference passing.
- A function whose name collides with a lib alias can be called via `@name(...)`.

### Template functions

Declared with `..` before the name and `<tparams>` after it; the return type is
written **after** the parameter list:

```ssz
public ..min<_t>(_t x, _t y) _t { ret x < y ? x : y; }
public ..sToN<_t>(^/char s) _t { ... }
public ..clone<_t>(^/_t src) ^_t { ... }
```

- `_t` is a type parameter (the `_t` suffix is reserved).
- Instantiation uses `!args?` (or `!arg1, arg2?`) at the call site:

  ```ssz
  .min!_t?(x, y)
  .uToSxX!.hex?(uinte)
  .max!double?(...)
  ```

- In type positions: `&Table!_t, self?`, `&Signed!byte?`.
- Template arguments can be `typeid`-tested with `typeid(_t)`.

### Return-value rules

- The compiler inserts cleanup of locals before every `ret`/`break`.
- A function returning a reference must return a temporary-compatible value; the
  returned ref's refcount is transferred, not copied.
- Functions used as threads must return `void` or a non-reference value.

---

## 8. Classes

### Declaration

```ssz
[decorator] &Name<tparams?> {
  [decorator] type member;              // fields
  [decorator] returntype method(params){ ... }
  new(){ ... }                          // constructor
  delete(){ ... }                       // destructor
}
```

```ssz
public &Socket {
  /index soc = .INVALID_SOCKET;
  delete(){ `close(); }
  void setSoc(index s){ `soc = s; }
  public bool connect(^/char host, ^/char port, int timeout, bool nodelay){ ... }
}
```

- `new()` runs when an instance is created (`&Name var = new()`, or `var.new(1)`
  for refs); `delete()` runs automatically when the last reference to the instance
  is released (block exit, `ret`, refcount drop, list removal, ...).
- `delete()` is the **destructor** — it does not free memory (refcounting does);
  it releases owned resources.
- Class objects are only ever referenced through refs: `^&Socket`, `%&Socket`,
  `&Socket` returned from `new()`.
- Methods access fields with `` ` ``.

### Creating instances

```ssz
&.soc.Socket s;
s.new(1);                      // allocate one Socket, run new()
^.soc.Socket s;  s.new(1);
%&.KeyBuffer kb;  kb.new(#.com.com);   // list of class refs
`liso.new(1);
```

A class object stored directly in a ref is created with `.new(count)`; the
constructor `new()` runs once per element. `x.new[-1]` grows a list by one and
constructs the new element.

### Methods

Methods are declared inside the class body. Nested/related class methods can be
implemented with the `TypeName::method` form:

```ssz
public &Node<_t> {
  public &Table<_t, node_t> {
    public void clear(){ `nodes.new(0); }
  }
  type table_t = &Table!_t, self?;
  public void table_t::getNede(^`node_t n=, ^/char name, uint hash){ ... }
}
```

`table_t::getNede` defines the method `getNede` for the class named by the type
alias `table_t`. This is how you attach methods to a nested/instantiated class type.

### Template classes

```ssz
public &NameTable<_t> { ... }
public &IntTable<int_t, _t> { ... }
type table_t = &Table!_t, self?;      // "self" = the enclosing template class
core &Node!_t?::Table!_t, &Node!_t?? t;
```

- `&Name!_t?` instantiates a template class with type argument `_t`.
- `self` inside a template refers to the enclosing class instantiation.
- `::` reaches nested types: `&Node!_t?::Table!_t` = the nested `Table` of `Node!_t?`.
- `core` declares a member whose type is a template instantiation of a class that
  recursively references this one (breaks circular-type ordering). Core members may
  only appear inside a class.

### Member visibility

| Marker | Effect |
|--------|--------|
| `public` | accessible from any module |
| `@` | accessible within the declaring file |
| `/` | read-only from outside the class (writes allowed inside) |
| *(none)* | private to the class |

Access from outside: `obj.publicField`, `ref~publicField`, `ref~method(args)`.
Access within the class: `` `field ``, `` `method(args) ``.

---

## 9. Enums

```ssz
public |NetState { ... }   // or |NetState { Error, Stoped, Playing, ... }
```

- Enumerators are declared as bare names, optionally with explicit values
  (`Name = 5`); values auto-increment.
- Enum constants are accessed as `|EnumName::member`, or through a module:
  `.NetState::Error`.
- Enums are 32-bit and cast to/from `int` freely; `switch` on an enum works.
- Enumerators are stored as `const int32` fields of the enum "type".
- Access via `|.NetState` in declarations:

  ```ssz
  ^|.NetState st;
  st = .NetState::Stop;
  ```

---

## 10. Libraries and Plugins

### Importing modules

```ssz
lib consts = <consts.ssz>;
lib s = <string.ssz>;
```

Public functions/classes/enums/constants of the imported file are reached through
the alias: `.s.trim(...)`, `.consts.int_t::MAX`, `.m.PI`.

### Native plugin functions

SSZ calls into C/C++ plugin libraries with the `plugin` declaration:

```ssz
plugin double Sin(:double:) = <dll/math.dll>;
ret Sin(:x:);
```

- `plugin <rettype> <name>(:<paramtypes>:) = <path>;`
- The path is relative to the script file; parameters are declared by type only.
- The plugin function is then callable as a normal function: `Sin(:x:)`.
- Arguments in the call use the `(: ... :)` delimiters; `=` marks by-ref args:
  `CompilerCompile(:`ptr, file, error=:)`, `SocketRecv(:`soc=, x=, typesize(_t):)`.
-  In this engine the plugin libraries are **statically linked and registered** at
  startup (`StaticPluginRegistry`), so the `<dll/...>` path is only used to name the
  library for resolution — no DLL is loaded. The bundled libraries (14 total) are:

  | Library | Registration file | Approx. functions |
  |---------|-------------------|-------------------|
  | `sdlplugin` | `sdlplugin_static.hpp` | ~62 (rendering, input, audio, GL) |
  | `ssz` | `ssz_static.hpp` | ~50 (compiler, memory, file I/O) |
  | `mesdialog` | `mesdialog_static.hpp` | ~30 (message dialogs) |
  | `lua` | `lua_static.hpp` | ~21 (Lua bridge) |
  | `socket` | `socket_static.hpp` | ~15 (networking) |
  | `ogg` | `ogg_static.hpp` | ~15 (audio decoding) |
  | `file` | `file_static.hpp` | ~15 (file operations) |
  | `sound` | `sound_static.hpp` | ~10 (audio playback) |
  | `math` | `math_static.hpp` | ~10 (math functions) |
  | `regex` | `regex_static.hpp` | ~8 (regular expressions) |
  | `thread` | `thread_static.hpp` | ~5 (threading) |
  | `time` | `time_static.hpp` | ~5 (time functions) |
  | `shell` | `shell_static.hpp` | ~3 (shell commands) |
  | `alert` | `alert_static.hpp` | ~3 (alert dialogs) |

Example wrapper style:

```ssz
public double sqrt(double x){
  plugin double Sqrt(:double:) = <dll/math.dll>;
  ret Sqrt(:x:);
}
```

---

## 11. Strings

Strings are `^/char` (read-only reference to `char`). String operations:

```ssz
^/char s = "hello";
#s                       // length in chars
s[0], s[-1]              // first / last char
s[1 .. -1]               // substring from 1 to end
s[0 .. peridx]           // substring
"a" + "b" + ''n          // concatenation with stringified numbers
'x' + ...                // char literals concatenate with strings/refs
?c                       // single char -> string reference
''i, ''u, ''f, ''d       // scalar -> string conversion
```

Common string helpers live in `lib/string.ssz` (`trim`, `split`, `find`, `equ`,
`toLower`, ...).

---

## 12. Template Instantiation Syntax

Template *uses* — both in types and in call expressions — are written with
`!`-arguments and a trailing `?`:

```ssz
.min!_t?(x, y)                    // call
&Table!_t, self?                  // type
&Signed!byte?                     // type
.uToSxX!.hex?(uinte)              // nested call
&Node!_t?::Table!_t, &Node!_t??   // nested template types
.typeid(func_t)                   // runtime type of a template-parametered type
```

The `!`-list can contain multiple comma-separated arguments; the whole list is
terminated by `?`.

> Note: The `TokentToStr()` debug function in `tokenkind.h` has typos for
> `SIGNATURE_TOKEN` (`L("signatutre")`) and `THREAD_TOKEN` (`L("thred")`).
> These are display-only strings — the actual keyword recognition in
> `source.hpp::MojiretsuToken` correctly matches `signature` (9 chars) and
> `thread` (6 chars).

---

## 13. Compile-Time Features

### `<consteval>(dest=, expr, ...)`

Tries to evaluate the expression(s) as compile-time constants. On success assigns
the result(s) to the by-ref destination(s) and evaluates to `true`; on failure
evaluates to `false` and leaves destinations untouched.

```ssz
if(!<consteval>(key=, ".sdl.K::" + skey)) ret false;
cond <consteval>(tmp=, buf) && tmp >= 0:
ret <consteval>(d=, #buf == 0 ? tmp : (buf .= tmp));
<consteval>(init=, "`stateInit" + ''`playerno + "P");
```

Use it to fold constant strings/expressions at compile time (the generated code
embeds the constant).

### `<idname>(typeid)`

Returns the name of a type as a string:

```ssz
Alert(:mes, <idname>(typeid(_t)):);
```

### `typeid(expr)` and `typesize(expr)`

- `typeid(x)` — integer type id of the runtime value (dynamic refs) or of a
  compile-time type (`typeid(_t)`, `typeid(`stateBulid:<-state)`).
- `typesize(x)` — byte size of one element of a dynamic ref (`ref`/`list`).
  **Limited to dynamic refs/lists only** — the compiler returns an error if the
  top of the type stack is not `DYNREF_TYPEID` or `DYNLIST_TYPEID`.
- `index` constants like `typeid(byte)` are ordered so relational tests work:
  `typeid(_t) <= typeid(byte) && typeid(_t) >= typeid(long)`.

### `sizeof(T)` (compiler intrinsic)

`sizeof(T)` returns the byte size of a compile-time type. It is **not a keyword
in the tokenizer** — the tokenizer has no `SIZEOF_TOKEN`. Instead, `sizeof` is
handled as a compiler-intrinsic special form, resolved at compile time when the
type argument is a template parameter or other compile-time type:

```ssz
while j < sizeof(_t)*8:   // used in lib/string.ssz
```

Unlike `typesize` (which only works on dynamic refs/lists), `sizeof` works on
any compile-time type including primitives, class types, and template parameters.

---

## 14. Concurrency

### Threads

Declare with `thread!func?` (function) or `thread!.method?` (method), launch with
`var..(...)`, wait with `<wait>(var, ...)`:

```ssz
thread!load? loadThread;
public bool runTread(){
  .loadThread..();
  ret true;
}
public void reset(){
  <wait>(.loadThread);
}
```

Restrictions enforced by the compiler:
- Thread functions cannot return reference types; their return value is discarded.
- Thread arguments that are refs/lists are refcounted for the duration of the call.
- You cannot pass threads by reference; anonymous functions cannot be passed to threads.

### Locks

```ssz
lock(expr, ...){ ... }
```

Blocks until all referenced objects are locked, runs the body, then unlocks.
Objects: `^T`, `%T`, `ref`/`list`, and `thread` values.

---

## 15. Memory Management

- **Refs** (`^`) and **lists** (`%`) are heap objects with reference counts.
- **Class objects** are reference-counted too; a class's `delete()` runs when the
  last reference is released.
- A **circular collector** (`CircularGC`) handles reference cycles between class
  objects (checked at compile time via `IsJunkanAble`); classes that can participate
  in cycles get the collector's pointer table registered.
- Locals are cleaned up automatically at block exit, `ret`, `break`, and `continue`
  — including nested blocks inside a `switch`/`branch`/`loop`.
- The compiler emits cleanup code (refcount decrements and `delete()` calls) for you;
  `delete var;` is an internal construct, not something you normally write.
- `ret` from a function triggers the same cleanup for all its local scopes, plus the
  parameter frame.
- Threading interacts with memory: a thread keeps its arguments' refcounts up for the
  thread's lifetime and releases them on completion.

---

## 16. Compiler / Runtime Notes (from the source)

- Parsing: `SourceTree::MakeTree()` tokenizes and builds nested `SourceTree` blocks
  (`FUNC`, `CLASS`, `ENUM`, `NORMAL`, `LOOP`, `SWITCH`, `BRANCH`, `LOCK`,
  `DELEGATE`). Statements are stored as token vectors and compiled per-block by
  `JITer`'s `NormalCompi` / `SwitchCompi` / `BranchCompi` / `LoopCompi`.
- Codegen: `JITer` emits x86 machine code through `BinaryCode` (`x86.hpp`). Function
  addresses are backpatched; calls go through direct addresses or jump tables.
- `switch` compiles to a nibble jump table (`CaseTable`), `branch` to a similar
  label table (`CondTable`), both with a fallback linear search.
- Constant expressions (`ConstShiki`) are folded at compile time; `consteval` runs
  the constant evaluator and inlines the result.
- Type checks are strict but allow *loose* (`YuruiKataCheck`) matching for refs/lists
  of refs (e.g. passing `^/char` where `^/^/char`'s element type is expected).
- Errors are collected with file/line info (`Source::addErrMes`) and reported after
  compilation; a `tokubetuerr` flag aborts parsing on severe errors.

---

## 17. Coding Conventions Observed in the Bundled Scripts

- Wrap every native function behind a `public` SSZ function in a `lib/*.ssz` module.
- Return error state as a `^/char` error string (empty = success) and test it with
  `#func(...)=>err > 0`.
- Use `public` for anything used across modules; `@` for same-file helpers.
- Template helpers live in `lib/string.ssz`, `lib/math.ssz`, `lib/table.ssz`.
- Global mutable state is kept in one module (e.g. `math.ssz`'s `randseed`) and
  mutated only from that module's public functions.
- Guard cross-thread access to shared objects with `lock(...)`.
- Use `branch` for multiple exclusive conditions with shared cleanup (`comm:`) and
  per-case epilogue (`diff<label>:`).
- **Prefer `branch{cond ... else: ...}` over `if(cond){...} else {...}`** inside
  class methods — `if/else` is unreliable with variable declarations, `loop{}`
  blocks, or `branch{}` nesting (compiler quirk: `"You can't do that here."` /
  `"Syntax error."` at the `}else{` line). Simple `if(cond){...}` without `else`
  is always safe.
- Declare variables at the top of the enclosing scope, not inside `loop{}` `do:`
  blocks — declarations with initialization there cause
  `"It cannot be defined here."` errors.

---

## Appendix A. Quick Operator Reference

| Token | Role |
|-------|------|
| `( )` | grouping / call |
| `(: ... :)` | by-ref argument list |
| `{ }` | block |
| `[ ]` | index / lambda |
| `,` | comma / statement separator |
| `;` | statement terminator |
| `.` | root-scope member access |
| `@` | file-scope access / public-in-file marker |
| `` ` `` | self access / parent-scope type lookup |
| `..` | slice range / template prefix / thread launch suffix |
| `~` | class-ref member access / delegate type |
| `<>` | raw address of ref/list |
| `::` | nested type / enum member access |
| `:<-` | apply delegate to each element (map) |
| `=>` | assign-and-yield (error capture) |
| `+= -= *= /= %= **= <<= >>= &= ^= |=` | compound assignment |
| `.=` | list append |
| `++ --` | increment / decrement |
| `! != !!` | not / not-equal / toggle |
| `#` | length (ref/list) / absolute value (number) |
| `$` | signature type prefix |
| `?` | ternary / temp-ref prefix |
| `&& \|\|` | short-circuit logical |
| `& \| ^ ~` | bitwise |
| `<< >>` | shift |
| `**` | power (float/double) |
| `== != < <= > >=` | comparison |
| `?/* ... */` | conditional comment |

## Appendix B. Type Prefix Quick Reference

| Prefix | Meaning |
|--------|---------|
| `^T` | reference (fixed array of `T`) |
| `%T` | list (dynamic array of `T`) |
| `^/T` | read-only reference |
| `&T` | class type |
| `|T` | enum type |
| `ref` / `list` | dynamic (runtime-typed) reference/list |
| `~$sig` | delegate |
| `$sig` | function signature |
| `func!f?` / `method!m?` | function/method value |
| `thread!f?` / `thread!.m?` | thread |
| `` `T `` | type resolved in the enclosing class scope |
| `_t` | template parameter |

## Appendix C. Keywords by Category

- **Control:** `if else switch case default branch cond comm diff else ret break continue loop while do`
- **Types:** `void byte ubyte short ushort int uint long ulong char bool float double index`
- **Type construction:** `ref list func method signature thread`
- **Declarations:** `const public type lib plugin new delete cast`
- **Introspection:** `typeid typesize idname`
- **Compile-time:** `consteval`
- **Constants:** `true false self`
- **Misc:** `core`

> Note: `sizeof` is a compiler-intrinsic (not a keyword token) — see Section 13.
> `len` has a token kind defined but is not recognized by the tokenizer or
> compiler — do not use it.
