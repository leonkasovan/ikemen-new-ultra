#!/usr/bin/env python3
"""
ssz_to_cpp.py — SSZ-to-C++ transpiler (proof of concept).

Converts SSZ script files into:
  1. A native C++ library (functions with the plugin ABI)
  2. A thin SSZ wrapper that keeps type definitions and delegates to the native lib

Control-flow translation:
  branch { cond <expr>: ... else: ... comm: }
    → if/else-if/else chains with goto end_B for break
  loop { <init> while; do: <body> continue: <post> while <cond>: }
    → { init; do { body; post; } while(cond); }
  break → goto end_B (branch) or break (loop)
  break, break → goto inner_end + break/goto outer
  continue → C++ continue (targets while condition, same as SSZ)

ABI conventions:
  - Args arrive reversed: last SSZ param = first C++ param (after PluginUtil*)
  - 32-bit args in low 32 bits of 8-byte slot
  - out-params (type=) arrive as pointers
  - ^/char strings arrive as Reference
"""

import re, sys, os
from dataclasses import dataclass, field
from typing import List, Optional, Tuple

# ─── SSZ → C++ type mapping ────────────────────────────────────────────────

SSZ_TO_CTYPE = {
    "int": "int32_t", "uint": "uint32_t", "bool": "bool",
    "float": "float", "double": "double", "long": "int64_t",
    "short": "int16_t", "byte": "int8_t", "ubyte": "uint8_t",
    "index": "intptr_t", "char": "char", "ushort": "uint16_t",
}

def ssz_to_cpp(t):
    t = t.strip()
    if t.startswith("^/char"): return "Reference"
    if t.startswith("^^"): return "Reference"
    if t.startswith("^"): return "Reference"
    if t.startswith("%"): return "intptr_t"
    return SSZ_TO_CTYPE.get(t, "intptr_t")

# ─── Data classes ───────────────────────────────────────────────────────────

@dataclass
class Field:
    name: str; ssz_type: str; default: str = ""; public: bool = True

@dataclass
class Param:
    name: str; ssz_type: str; is_out: bool = False; default: str = ""

@dataclass
class Method:
    name: str; ret_type: str; params: List[Param]; body: List[str]
    public: bool = True; template: bool = False

@dataclass
class Struct:
    name: str; fields: List[Field]; methods: List[Method]
    public: bool = True; inner: List['Struct'] = field(default_factory=list)

@dataclass
class Func:
    name: str; ret_type: str; params: List[Param]; body: List[str]
    public: bool = True; template: bool = False

@dataclass
class ModVar:
    name: str; ssz_type: str; default: str = ""; public: bool = True

# ─── Parser ─────────────────────────────────────────────────────────────────

def is_type_word(w):
    return w in SSZ_TO_CTYPE or w.startswith("^") or w.startswith("%") or w.startswith("^^")

def split_params(s):
    params, depth, cur = [], 0, ""
    for ch in s:
        if ch == '<': depth += 1
        elif ch == '>': depth -= 1
        elif ch == ',' and depth == 0:
            params.append(cur.strip()); cur = ""; continue
        cur += ch
    if cur.strip(): params.append(cur.strip())
    return params

def parse_param(s):
    s = s.strip()
    if not s: return None
    is_out = s.endswith("=") and not s.endswith("==")
    if is_out: s = s[:-1].strip()
    parts = s.split()
    if len(parts) >= 2:
        return Param(name=parts[-1], ssz_type=parts[0], is_out=is_out)
    elif len(parts) == 1:
        return Param(name=f"p{hash(s) % 1000}", ssz_type=parts[0], is_out=is_out)
    return None

def extract_body(lines, start):
    depth, body, pos, found_open = 0, [], start, False
    while pos < len(lines):
        line = lines[pos]
        for ch in line:
            if ch == '{': depth += 1; found_open = True
            elif ch == '}': depth -= 1
        body.append(line); pos += 1
        if found_open and depth <= 0: break
    return body, pos

class SSZParser:
    def __init__(self, source, filename=""):
        self.source = source; self.filename = filename
        self.lines = source.splitlines()
        self.imports, self.module_vars, self.structs, self.functions = [], [], [], []
        self.enums, self.constants, self.top_code = [], [], []

    def parse(self):
        pos = 0
        while pos < len(self.lines):
            line = self.lines[pos]; s = line.strip()
            if s == "" or s.startswith("//") or s.startswith("/*") or s.startswith("/?/") or s.startswith("/*?*/"):
                pos += 1; continue
            if s.startswith("lib "):
                self.imports.append(s); pos += 1; continue
            if s.startswith("const "):
                self.constants.append(s); pos += 1; continue
            if s.startswith("|") and "{" in s:
                pos = self._parse_enum(pos); continue
            if re.match(r"^(public\s+)?&\w+", s):
                struct, pos = self._parse_struct(pos); self.structs.append(struct); continue
            if self._looks_like_func_def(s):
                func, pos = self._parse_function(pos)
                if func: self.functions.append(func)
                continue
            if self._looks_like_var_decl(s):
                mv, pos = self._parse_module_var(pos)
                if mv: self.module_vars.append(mv)
                continue
            self.top_code.append(s); pos += 1

    def _looks_like_func_def(self, s):
        s2 = s.lstrip("/").strip()
        if s2.startswith("public "): s2 = s2[7:].strip()
        if s2.startswith(".."): return True
        m = re.match(r"^(\S+)\s+(\w+)\s*[\(\<]", s2)
        if m and "(" in s2: return True
        return False

    def _looks_like_var_decl(self, s):
        s2 = s.lstrip("/").strip()
        if s2.startswith("public "): s2 = s2[7:].strip()
        m = re.match(r"^(\S+)\s+(\w+)\s*[=;]", s2)
        if m: return is_type_word(m.group(1)) or m.group(1).startswith("^") or m.group(1).startswith("%")
        return False

    def _parse_enum(self, pos):
        start, depth = pos, 0
        while pos < len(self.lines):
            for ch in self.lines[pos]:
                if ch == '{': depth += 1
                elif ch == '}': depth -= 1
            pos += 1
            if depth <= 0: break
        self.enums.append("\n".join(self.lines[start:pos]))
        return pos

    def _parse_struct(self, pos):
        s = self.lines[pos].strip(); pos += 1
        public = s.startswith("public ")
        name = s.replace("public", "").replace("&", "").strip()
        while pos < len(self.lines) and "{" not in self.lines[pos]: pos += 1
        body_lines, pos = extract_body(self.lines, pos)
        fields, methods, inners = [], [], []
        i = 0
        while i < len(body_lines):
            ls = body_lines[i].strip()
            if ls == "" or ls.startswith("//") or ls.startswith("/*"): i += 1; continue
            if re.match(r"^(public\s+)?&\w+", ls):
                inner, i = self._parse_inner_struct(body_lines, i); inners.append(inner); continue
            if ls.startswith("type "): i += 1; continue
            if "(" in ls:
                method, i = self._parse_method_from_lines(body_lines, i)
                if method: methods.append(method)
                continue
            field = self._parse_field_line(ls)
            if field: fields.append(field)
            i += 1
        return Struct(name=name, fields=fields, methods=methods, public=public, inner=inners), pos

    def _parse_inner_struct(self, lines, pos):
        return self._parse_struct_from_lines(lines, pos)

    def _parse_struct_from_lines(self, lines, pos):
        s = lines[pos].strip(); pos += 1
        public = s.startswith("public ")
        name = s.replace("public", "").replace("&", "").strip()
        while pos < len(lines) and "{" not in lines[pos]: pos += 1
        if pos >= len(lines):
            return Struct(name=name, fields=[], methods=[], public=public), pos
        body_lines, pos = extract_body(lines, pos)
        fields, methods, inners = [], [], []
        i = 0
        while i < len(body_lines):
            ls = body_lines[i].strip()
            if ls == "" or ls.startswith("//") or ls.startswith("/*"): i += 1; continue
            if re.match(r"^(public\s+)?&\w+", ls):
                inner, i = self._parse_struct_from_lines(body_lines, i); inners.append(inner); continue
            if ls.startswith("type "): i += 1; continue
            if "(" in ls:
                method, i = self._parse_method_from_lines(body_lines, i)
                if method: methods.append(method)
                continue
            field = self._parse_field_line(ls)
            if field: fields.append(field)
            i += 1
        return Struct(name=name, fields=fields, methods=methods, public=public, inner=inners), pos

    def _parse_field_line(self, s):
        s = s.strip().rstrip(",;")
        public = s.startswith("public ")
        if public: s = s[7:].strip()
        if s.startswith("/"): s = s[1:].strip(); public = True
        m = re.match(r"^(\S+)\s+(\w+)\s*(?:=\s*(.*))?$", s)
        if m: return Field(name=m.group(2), ssz_type=m.group(1), default=(m.group(3) or "").strip(), public=public)
        return None

    def _parse_method_from_lines(self, lines, pos):
        s = lines[pos].strip(); pos += 1
        public = s.startswith("public ")
        if public: s = s[7:].strip()
        if s.startswith("/"): s = s[1:].strip(); public = True
        is_template = "_t" in s and (".." in s or "!" in s)
        m = re.match(r"^(?:\.\.)?(\w+)\s*\(([^)]*)\)\s*\{?$", s)
        if m:
            name, param_str = m.group(1), m.group(2)
            ret_type = "void"
        else:
            m = re.match(r"^(?:\.\.)?(\S+?)\s+(\w+)\s*\(([^)]*)\)\s*\{?$", s)
            if not m: return None, pos
            ret_type, name, param_str = m.group(1), m.group(2), m.group(3)
        params = [p for p in (parse_param(x) for x in split_params(param_str)) if p]
        body_lines, pos = extract_body(lines, pos)
        return Method(name=name, ret_type=ret_type, params=params, body=body_lines, public=public, template=is_template), pos

    def _parse_function(self, pos):
        s = self.lines[pos].strip(); pos += 1
        public = s.startswith("public ")
        if public: s = s[7:].strip()
        if s.startswith("/"): s = s[1:].strip(); public = True
        is_template = ".." in s and "_t" in s
        m = re.match(r"^(?:\.\.)?(\S+?)\s+(\w+)\s*\(([^)]*)\)\s*\{?$", s)
        if not m: return None, pos
        ret_type, name, param_str = m.group(1), m.group(2), m.group(3)
        params = [p for p in (parse_param(x) for x in split_params(param_str)) if p]
        body_lines, pos = extract_body(self.lines, pos)
        return Func(name=name, ret_type=ret_type, params=params, body=body_lines, public=public, template=is_template), pos

    def _parse_module_var(self, pos):
        s = self.lines[pos].strip(); pos += 1
        public = s.startswith("public ")
        if public: s = s[7:].strip()
        if s.startswith("/"): s = s[1:].strip(); public = True
        m = re.match(r"^(\S+)\s+(\w+)\s*(?:=\s*(.*))?$", s.rstrip(",;"))
        if m: return ModVar(name=m.group(2), ssz_type=m.group(1), default=(m.group(3) or "").strip(), public=public), pos
        return None, pos


# ─── Body Translator ────────────────────────────────────────────────────────

class BodyTranslator:
    """Translates SSZ method body lines to C++ code.

    SSZ control flow semantics (from JIT compiler source):

    branch { cond <expr>: <body> cond <expr>: <body> ... else: <body> comm: }
      - if/else-if/else chain. First matching cond wins.
      - break → exits the branch block (jumps to comm:)
      - break, break; → first break exits inner scope, second exits outer

    loop { <init> while; do: <body> continue: <post-body> while <cond>: }
      - while; = no condition (infinite loop, exit via break)
      - while <cond>: = conditional loop
      - do: = start of loop body
      - continue: = start of post-body (runs after body, before condition check)
      - C++ translation: { init; do { body; post; } while(cond); }

    break
      - Inside branch → goto branch end label
      - Inside loop (not in branch) → C++ break
      - break, break; → exits two scopes

    continue
      - Always → C++ continue (targets while condition, same as SSZ)
    """

    def __init__(self, struct_fields=None, param_names=None):
        self.struct_fields = struct_fields or set()
        self.param_names = param_names or set()
        self._counter = 0

    def _next_id(self):
        self._counter += 1
        return self._counter

    def translate_body(self, body_lines):
        """Translate a list of SSZ body lines to C++ code lines."""
        self._counter = 0
        # Strip leading/trailing braces and blanks
        lines = [l.rstrip() for l in body_lines]
        result = self._translate_block(lines, 0, len(lines), in_branch=False, in_loop=False)
        return result

    def _is_blank_or_comment(self, s):
        s = s.strip()
        return s == "" or s.startswith("//") or s.startswith("/*") or s.startswith("/?/") or s.startswith("/*?*/")

    def _is_branch_start(self, s):
        return s.strip() in ("branch{", "branch {")

    def _is_loop_start(self, s):
        return s.strip() in ("loop{", "loop {")

    def _is_cond_start(self, s):
        return s.strip().startswith("cond ")

    def _is_else_colon(self, s):
        return s.strip().startswith("else:")

    def _is_comm_colon(self, s):
        return s.strip() == "comm:"

    def _is_do_colon(self, s):
        return s.strip().startswith("do:")

    def _is_continue_colon(self, s):
        return s.strip().startswith("continue:")

    def _is_while_line(self, s):
        """Check if line is while; or while <cond>: (but not while inside an expression)."""
        s = s.strip()
        if s == "while;": return True
        if re.match(r"^while\s+.+:$", s): return True
        return False

    def _find_matching_brace(self, lines, start):
        """Find the line index of the matching } for the { found at start."""
        depth = 0
        found = False
        for i in range(start, len(lines)):
            for ch in lines[i]:
                if ch == '{': depth += 1; found = True
                elif ch == '}': depth -= 1
            if found and depth <= 0:
                return i
        return len(lines) - 1

    def _translate_block(self, lines, start, end, in_branch, in_loop, branch_end_label=None):
        """Translate a block of lines, handling nested branch/loop constructs.

        in_branch: True if we're inside a branch block (break targets branch end)
        in_loop: True if we're inside a loop (break targets loop, continue targets while)
        branch_end_label: label for the innermost branch's end (for break)
        """
        result = []
        i = start
        while i < end:
            s = lines[i].strip()

            # Skip blanks and comments
            if self._is_blank_or_comment(s):
                i += 1; continue

            # ── Branch block ───────────────────────────────────────────
            if self._is_branch_start(s):
                # Find matching } for this branch{
                brace_end = self._find_matching_brace(lines, i)
                branch_body = lines[i+1:brace_end]  # lines inside branch{ ... }
                branch_result = self._translate_branch(
                    branch_body, 0, len(branch_body),
                    in_loop=in_loop, outer_branch_end=branch_end_label
                )
                result.extend(branch_result)
                i = brace_end + 1
                continue

            # ── Loop block ─────────────────────────────────────────────
            if self._is_loop_start(s):
                lid = self._next_id()
                brace_end = self._find_matching_brace(lines, i)
                loop_body = lines[i+1:brace_end]
                loop_result = self._translate_loop(loop_body, 0, len(loop_body), lid)
                result.extend(loop_result)
                i = brace_end + 1
                continue

            # ── break ──────────────────────────────────────────────────
            if s.startswith("break"):
                result.extend(self._emit_break(s, in_branch, in_loop, branch_end_label))
                i += 1; continue

            # ── continue ───────────────────────────────────────────────
            if s.startswith("continue;") or s == "continue":
                result.append("continue;  // SSZ continue")
                i += 1; continue

            # ── Regular statement ──────────────────────────────────────
            translated = self._translate_stmt(s)
            if translated is not None:
                result.append(translated)
            i += 1

        return result

    def _translate_branch(self, lines, start, end, in_loop, outer_branch_end):
        """Translate a branch{...} block to C++ if/else-if/else.

        SSZ branch structure:
          cond <expr>: <body>
          cond <expr>: <body>
          ...
          else: <body>
          (implicit comm: at the end)

        break inside this branch → goto end_B{bid}
        """
        bid = self._next_id()
        end_label = f"end_B{bid}"
        result = []
        cond_count = 0

        i = start
        while i < end:
            s = lines[i].strip()

            if self._is_blank_or_comment(s):
                i += 1; continue

            # ── cond <expr>: ───────────────────────────────────────
            if s.startswith("cond "):
                expr = s[5:].strip()
                if expr.endswith(":"): expr = expr[:-1].strip()
                if cond_count == 0:
                    result.append(f"if ({self._translate_expr(expr)}) {{")
                else:
                    result.append(f"}} else if ({self._translate_expr(expr)}) {{")
                cond_count += 1
                i += 1
                # Consume body until next cond/else:/comm: or end
                body_lines = self._collect_branch_arm(lines, i, end)
                body_result = self._translate_block(
                    body_lines, 0, len(body_lines),
                    in_branch=True, in_loop=in_loop, branch_end_label=end_label
                )
                for l in body_result:
                    result.append(f"  {l}")
                i += len(body_lines)
                continue

            # ── else: ──────────────────────────────────────────────
            if s.startswith("else:"):
                if cond_count == 0:
                    result.append("{  // else")
                else:
                    result.append("} else {")
                i += 1
                # Consume body until end of branch block
                body_lines = self._collect_branch_arm(lines, i, end)
                body_result = self._translate_block(
                    body_lines, 0, len(body_lines),
                    in_branch=True, in_loop=in_loop, branch_end_label=end_label
                )
                for l in body_result:
                    result.append(f"  {l}")
                i += len(body_lines)
                continue

            # ── Nested branch inside this branch ───────────────────
            if self._is_branch_start(s):
                inner_result = self._translate_branch(
                    lines, i + 1, end, in_loop, outer_branch_end=end_label
                )
                # Find the matching } for the inner branch
                brace_end = self._find_matching_brace(lines, i)
                result.extend(inner_result)
                i = brace_end + 1
                continue

            # ── Nested loop inside branch ──────────────────────────
            if self._is_loop_start(s):
                lid = self._next_id()
                brace_end = self._find_matching_brace(lines, i)
                loop_body = lines[i+1:brace_end]
                loop_result = self._translate_loop(loop_body, 0, len(loop_body), lid)
                result.extend(loop_result)
                i = brace_end + 1
                continue

            # ── break inside branch ────────────────────────────────
            if s.startswith("break"):
                result.extend(self._emit_break(s, in_branch=True, in_loop=in_loop,
                                               branch_end_label=end_label))
                i += 1; continue

            # ── Regular statement inside branch arm ────────────────
            translated = self._translate_stmt(s)
            if translated is not None:
                result.append(translated)
            i += 1

        # Close the last if/else block
        if cond_count > 0:
            result.append("}")

        return result

    def _collect_branch_arm(self, lines, start, branch_end):
        """Collect lines belonging to one arm of a branch (until cond/else:/comm:})."""
        result = []
        i = start
        depth = 0
        while i < branch_end:
            s = lines[i].strip()

            # Track brace depth for inner blocks
            for ch in s:
                if ch == '{': depth += 1
                elif ch == '}': depth -= 1

            # Stop at branch-level keywords (only at depth 0)
            if depth <= 0:
                if self._is_cond_start(s):
                    break
                if self._is_else_colon(s):
                    break
                if self._is_comm_colon(s):
                    break
                # depth < 0 means we went past the branch's closing }
                if depth < 0:
                    depth = 0  # reset
                    break

            result.append(lines[i])
            i += 1

        return result

    def _translate_loop(self, lines, start, end, lid):
        """Translate a loop{...} block to C++ do-while.

        SSZ loop structure (from JIT source — LoopCompi):
          loop{
            <init vars / declarations>
            while;              ← no condition (infinite loop)
            or while <cond>:    ← conditional entry
          do:
            <body>
          continue:
            <post-body>
          while <cond>:         ← loop condition (at the end)
          }

        C++ translation:
          {  // loop
            <init>
            do {
              <body>
              <post-body>
            } while (<cond>);
          }

        The key insight from the JIT:
          - do: sets the loop body address (backpatch target for continue)
          - continue: marks where the condition check happens
          - while <cond>: at the end is the actual condition
          - break exits the loop entirely (C++ break)
          - continue jumps to the while condition (C++ continue)
        """
        result = []
        init_lines = []
        condition = None

        # Phase 1: Parse init (before do:)
        # During init, nested loop/branch constructs must be delegated to
        # their own translators (not collected as raw lines).
        i = start
        while i < end:
            s = lines[i].strip()
            if self._is_blank_or_comment(s):
                i += 1; continue

            # while; or while <cond>: — condition (may appear before do:)
            if self._is_while_line(s):
                if s == "while;":
                    condition = None  # infinite loop
                elif s.startswith("while ") and s.endswith(":"):
                    condition = self._translate_expr(s[6:].rstrip(":").strip())
                i += 1; continue

            # do: — start of loop body
            if self._is_do_colon(s):
                i += 1
                break

            # Nested loop/branch inside init — delegate to translators
            if self._is_loop_start(s):
                inner_lid = self._next_id()
                brace_end = self._find_matching_brace(lines, i)
                loop_body = lines[i+1:brace_end]
                loop_result = self._translate_loop(loop_body, 0, len(loop_body), inner_lid)
                result.extend(["  " + l for l in loop_result])
                i = brace_end + 1
                continue

            if self._is_branch_start(s):
                brace_end = self._find_matching_brace(lines, i)
                branch_body = lines[i+1:brace_end]
                branch_result = self._translate_branch(
                    branch_body, 0, len(branch_body),
                    in_loop=True
                )
                result.extend(["  " + l for l in branch_result])
                i = brace_end + 1
                continue

            # Variable declarations / init
            init_lines.append(self._translate_stmt(s) or s)
            i += 1

        # Phase 2: Parse body (after do:, until continue: or while <cond>: or loop-closing })
        body_lines = []
        post_lines = []
        in_post = False
        body_depth = 0  # brace depth tracking for inner blocks
        while i < end:
            s = lines[i].strip()
            if self._is_blank_or_comment(s):
                i += 1; continue

            # Track brace depth for inner blocks (if/else/etc)
            for ch in s:
                if ch == '{': body_depth += 1
                elif ch == '}': body_depth -= 1

            # Closing } at depth 0 = end of loop body
            if body_depth < 0 and s.startswith("}"):
                body_depth = 0  # reset
                break

            # continue: — marks post-body section
            if self._is_continue_colon(s):
                in_post = True
                i += 1; continue

            # while <cond>: — loop condition (at the end)
            if self._is_while_line(s) and body_depth == 0:
                if s == "while;":
                    i += 1; continue
                cond = s[6:].rstrip(":").strip()
                condition = self._translate_expr(cond)
                i += 1
                continue

            if in_post:
                post_lines.append(lines[i])
            else:
                body_lines.append(lines[i])
            i += 1

        # Phase 3: Translate body and post-body
        body_result = self._translate_block(
            body_lines, 0, len(body_lines),
            in_branch=False, in_loop=True
        )
        post_result = self._translate_block(
            post_lines, 0, len(post_lines),
            in_branch=False, in_loop=True
        )

        # Phase 4: Emit C++ do-while
        result.append("{  // loop")
        for l in init_lines:
            result.append(f"  {l}")
        result.append("  do {")
        for l in body_result:
            result.append(f"    {l}")
        for l in post_result:
            result.append(f"    {l}")
        if condition:
            result.append(f"  }} while ({condition});")
        else:
            result.append("  } while (true);")
        result.append("}")

        return result

    def _emit_break(self, s, in_branch, in_loop, branch_end_label):
        """Emit C++ break for SSZ break statements.

        SSZ semantics (from JIT NormalCompi::Break):
          - break inside branch → exit the branch block (goto end_B)
          - break inside loop → exit the loop (C++ break)
          - break inside function → return (goto function end)
          - break, break; → comma-separated = nested scope exits

        C++:
          - break in branch → goto end_label
          - break in loop → break
          - break in function → return
          - break, break → goto inner_end + break/goto outer
        """
        parts = [p.strip() for p in s.replace(";", "").split(",")]
        n_breaks = len(parts)
        result = []

        if n_breaks == 1:
            if in_branch and branch_end_label:
                result.append(f"goto {branch_end_label};  // break branch")
            elif in_loop:
                result.append("break;  // break loop")
            else:
                result.append("return;  // break function")
        else:
            # Multiple breaks: first (n-1) exit branch scopes, last exits loop/function
            for j in range(n_breaks):
                is_last = (j == n_breaks - 1)
                if is_last:
                    if in_loop:
                        result.append("break;  // break loop (outer)")
                    else:
                        result.append("return;  // break function")
                else:
                    if branch_end_label:
                        result.append(f"goto {branch_end_label};  // break branch (inner)")
                    elif in_loop:
                        result.append("break;  // break loop (inner)")
                    else:
                        result.append("return;  // break function (inner)")

        return result

    def _translate_stmt(self, s):
        """Translate a single SSZ statement to C++."""
        s = s.strip()
        if not s or s.startswith("//"):
            return None

        # ret expr → return expr;
        if s.startswith("ret "):
            expr = s[4:].strip().rstrip(";")
            return f"return {self._translate_expr(expr)};"
        if s == "ret;":
            return "return;"

        # Assignment / expression statement
        t = self._translate_expr(s)
        if t:
            if not t.endswith(";") and not t.endswith("{") and not t.endswith("}"):
                return t + ";"
            return t
        return s

    def _translate_expr(self, expr):
        """Translate an SSZ expression to C++."""
        r = expr.strip()
        r = r.rstrip(";").strip()

        # Backtick: `field → just the field name (passed as out-param in C++ ABI)
        # In the plugin ABI, struct fields are passed as pointer params,
        # so `foo becomes just foo (the pointer param name)
        def backtick_replace(m):
            return m.group(1)
        r = re.sub(r"`(\w+)", backtick_replace, r)

        # Module calls: .lib.func() → lib_func()
        r = re.sub(r"\.(\w+)\.(\w+)\(", lambda m: f"{m.group(1)}_{m.group(2)}(", r)

        # #expr (length operator): #`x → x.size()
        r = re.sub(r"#`(\w+)", r"\1.size()", r)

        # operator <, (SSZ temp-ref syntax) — leave as-is for now
        # .field member access — leave as-is (needs per-module extern declarations)

        return r


# ─── C++ Generator ──────────────────────────────────────────────────────────

class CppGen:
    def __init__(self, parser: SSZParser, lib_name: str):
        self.p = parser
        self.lib = lib_name

    def gen_cpp(self):
        lines = []
        L = lines.append

        L(f"// ====================================================================")
        L(f"// {self.lib}.cpp — native C++ implementation of SSZ `{self.lib}`")
        L(f"// Transpiled from {self.p.filename}")
        L(f"// ====================================================================")
        L("")
        L('#include <cstdint>')
        L('#include <string>')
        L('#include <vector>')
        L("")
        L('#include "sszdef.h"')
        L('#include "native_lib.hpp"')
        L('#include "arrayandref.hpp"')
        L('#include "pluginutil.hpp"')
        L("")
        L("struct PluginUtil;")
        L("")

        # Cross-module extern declarations
        self._gen_cross_module_decls(lines)

        # Struct fields layout comments
        for s in self.p.structs:
            self._gen_struct_comment(lines, s)

        # Module-level functions
        for func in self.p.functions:
            if func.template:
                L(f"// {func.name} — template, stays in SSZ wrapper")
                L("")
                continue
            self._gen_function(lines, func)
            L("")

        # Struct methods
        for s in self.p.structs:
            for m in s.methods:
                if m.template:
                    L(f"// {s.name}::{m.name} — template, stays in SSZ wrapper")
                    L("")
                    continue
                self._gen_method(lines, s, m)
                L("")

        # Registration
        self._gen_register(lines)

        return "\n".join(lines)

    def _gen_cross_module_decls(self, lines):
        modules = set()
        for s in self.p.structs:
            for m in s.methods:
                for bl in m.body:
                    for match in re.finditer(r"\.(\w+)\.(\w+)", bl):
                        modules.add(match.group(1))
        for func in self.p.functions:
            for bl in func.body:
                for match in re.finditer(r"\.(\w+)\.(\w+)", bl):
                    modules.add(match.group(1))
        for mod in sorted(modules):
            lines.append(f"// Cross-module: .{mod}.* calls")
        if modules:
            lines.append("")

    def _gen_struct_comment(self, lines, s):
        lines.append(f"// ── {s.name} ──────────────────────────────────────────────")
        lines.append(f"// Fields (out-params in C++ ABI):")
        for f in s.fields:
            default = f" = {f.default}" if f.default else ""
            lines.append(f"//   {f.ssz_type} {f.name}{default}")
        lines.append("")

    def _gen_function(self, lines, func):
        cpp_ret = ssz_to_cpp(func.ret_type)
        params_rev = list(reversed(func.params))
        cpp_params = ["PluginUtil* pu"]
        for p in params_rev:
            ct = ssz_to_cpp(p.ssz_type)
            if p.is_out: ct += "*"
            cpp_params.append(f"{ct} {p.name}")

        sig = f"static {cpp_ret} SSZ_STDCALL {func.name}(\n\t{', '.join(cpp_params)})"

        translator = BodyTranslator(param_names={p.name for p in func.params})
        cpp_body = translator.translate_body(func.body)

        body_lines = ["{"]
        for l in cpp_body:
            body_lines.append(f"\t{l}")
        body_lines.append("}")

        lines.append(f"{sig}")
        lines.extend(body_lines)

    def _gen_method(self, lines, struct, method):
        cpp_ret = ssz_to_cpp(method.ret_type)
        field_params = self._find_field_refs(struct, method)

        all_params = []
        for f in struct.fields:
            if f.name in field_params:
                all_params.append(f"{ssz_to_cpp(f.ssz_type)}* {f.name}")
        for p in reversed(method.params):
            ct = ssz_to_cpp(p.ssz_type)
            if p.is_out: ct += "*"
            all_params.append(f"{ct} {p.name}")

        full_params = ["PluginUtil* pu"] + all_params
        sig = f"static {cpp_ret} SSZ_STDCALL {struct.name}_{method.name}(\n\t{', '.join(full_params)})"

        param_names = {p.name for p in method.params}
        translator = BodyTranslator(
            struct_fields={f.name for f in struct.fields},
            param_names=param_names
        )
        cpp_body = translator.translate_body(method.body)

        body_lines = ["{"]
        for l in cpp_body:
            body_lines.append(f"\t{l}")
        body_lines.append("}")

        lines.append(f"{sig}")
        lines.extend(body_lines)

    def _find_field_refs(self, struct, method):
        refs = set()
        for bl in method.body:
            for m in re.finditer(r"`(\w+)", bl):
                if any(f.name == m.group(1) for f in struct.fields):
                    refs.add(m.group(1))
        return refs

    def _gen_register(self, lines):
        lines.append(f'extern "C" bool {self.lib}_lib_register()')
        lines.append("{")
        lines.append(f"\tstatic const NativeLib::NativeFunction funcs[] = {{")

        for s in self.p.structs:
            for m in s.methods:
                if m.template: continue
                sig = self._make_sig(s, m)
                lines.append(f'\t\t{{ "{s.name}_{m.name}", "{sig}", (void*){s.name}_{m.name} }},')
        for func in self.p.functions:
            if func.template: continue
            sig = self._make_func_sig(func)
            lines.append(f'\t\t{{ "{func.name}", "{sig}", (void*){func.name} }},')

        lines.append("\t};")
        lines.append(f"\tNativeLib::NativeLibrary lib;")
        lines.append(f'\tlib.name = "{self.lib}";')
        lines.append(f"\tfor(size_t i = 0; i < sizeof(funcs)/sizeof(funcs[0]); i++)")
        lines.append(f"\t\tlib.functions.push_back(funcs[i]);")
        lines.append(f"\treturn NativeLib::RegisterLibrary(lib);")
        lines.append("}")

    def _make_sig(self, struct, method):
        ret = method.ret_type.strip() if method.ret_type.strip() else "void"
        field_params = self._find_field_refs(struct, method)
        parts = []
        for f in struct.fields:
            if f.name in field_params:
                parts.append(f"{f.ssz_type}=")
        for p in method.params:
            if p.is_out: parts.append(f"{p.ssz_type}=")
            else: parts.append(p.ssz_type)
        return f"{ret} ({', '.join(parts)})"

    def _make_func_sig(self, func):
        ret = func.ret_type.strip() if func.ret_type.strip() else "void"
        parts = []
        for p in func.params:
            if p.is_out: parts.append(f"{p.ssz_type}=")
            else: parts.append(p.ssz_type)
        return f"{ret} ({', '.join(parts)})"

    def gen_wrapper(self):
        lines = []
        L = lines.append
        L(f"// ====================================================================")
        L(f"// {self.lib}.ssz — thin wrapper for native {self.lib} library")
        L(f"// Transpiled from {self.p.filename}")
        L(f"// Types stay here; method bodies delegate to C++.")
        L(f"// ====================================================================")
        L("")
        for imp in self.p.imports: L(imp)
        L("")
        L(f"lib {self.lib}_native = <{self.lib}>;")
        L("")
        for c in self.p.constants: L(c)
        for e in self.p.enums: L(e); L("")
        for v in self.p.module_vars:
            pub = "public " if v.public else ""
            default = f" = {v.default}" if v.default else ""
            L(f"{pub}{v.ssz_type} {v.name}{default};")
        L("")
        for s in self.p.structs:
            L(self._wrapper_struct(s)); L("")
        for func in self.p.functions:
            if func.template: L(self._wrapper_full_func(func))
            else: L(self._wrapper_delegate_func(func))
            L("")
        return "\n".join(lines)

    def _wrapper_struct(self, s):
        lines = []
        pub = "public " if s.public else ""
        lines.append(f"{pub}&{s.name}")
        lines.append("{")
        for f in s.fields:
            fp = "public " if f.public else ""
            default = f" = {f.default}" if f.default else ""
            lines.append(f"  {fp}{f.ssz_type} {f.name}{default};")
        for m in s.methods:
            if m.template: lines.append(self._wrapper_full_method(s, m))
            else: lines.append(self._wrapper_delegate_method(s, m))
        lines.append("}")
        return "\n".join(lines)

    def _wrapper_delegate_method(self, s, m):
        lines = []
        pub = "public " if m.public else ""
        pp = []
        for p in m.params:
            if p.is_out: pp.append(f"{p.ssz_type} {p.name}=")
            elif p.default: pp.append(f"{p.ssz_type} {p.name} = {p.default}")
            else: pp.append(f"{p.ssz_type} {p.name}")
        params = ", ".join(pp)
        field_params = self._find_field_refs(s, m)
        fargs = [f"`{f.name}=" for f in s.fields if f.name in field_params]
        margs = [f"{p.name}=" if p.is_out else p.name for p in m.params]
        args = ", ".join(fargs + margs)
        ret = m.ret_type.strip()
        if ret in ("void", ""):
            lines.append(f"  {pub}void {m.name}({params})")
            lines.append(f"  {{")
            lines.append(f"    .{self.lib}_native.{s.name}_{m.name}({args});")
            lines.append(f"  }}")
        else:
            lines.append(f"  {pub}{ret} {m.name}({params})")
            lines.append(f"  {{")
            lines.append(f"    ret .{self.lib}_native.{s.name}_{m.name}({args});")
            lines.append(f"  }}")
        return "\n".join(lines)

    def _wrapper_full_method(self, s, m):
        lines = []
        pub = "public " if m.public else ""
        pp = []
        for p in m.params:
            if p.is_out: pp.append(f"{p.ssz_type} {p.name}=")
            elif p.default: pp.append(f"{p.ssz_type} {p.name} = {p.default}")
            else: pp.append(f"{p.ssz_type} {p.name}")
        params = ", ".join(pp)
        ret = m.ret_type.strip()
        if ret in ("void", ""):
            lines.append(f"  {pub}void {m.name}({params})")
        else:
            lines.append(f"  {pub}{ret} {m.name}({params})")
        lines.append("  {")
        for bl in m.body: lines.append(f"  {bl}")
        return "\n".join(lines)

    def _wrapper_delegate_func(self, func):
        lines = []
        pub = "public " if func.public else ""
        pp = []
        for p in func.params:
            if p.is_out: pp.append(f"{p.ssz_type} {p.name}=")
            elif p.default: pp.append(f"{p.ssz_type} {p.name} = {p.default}")
            else: pp.append(f"{p.ssz_type} {p.name}")
        params = ", ".join(pp)
        margs = [f"{p.name}=" if p.is_out else p.name for p in func.params]
        args = ", ".join(margs)
        ret = func.ret_type.strip()
        if ret in ("void", ""):
            lines.append(f"{pub}void {func.name}({params})")
            lines.append(f"{{")
            lines.append(f"  .{self.lib}_native.{func.name}({args});")
            lines.append(f"}}")
        else:
            lines.append(f"{pub}{ret} {func.name}({params})")
            lines.append(f"{{")
            lines.append(f"  ret .{self.lib}_native.{func.name}({args});")
            lines.append(f"}}")
        return "\n".join(lines)

    def _wrapper_full_func(self, func):
        lines = []
        pub = "public " if func.public else ""
        pp = []
        for p in func.params:
            if p.is_out: pp.append(f"{p.ssz_type} {p.name}=")
            elif p.default: pp.append(f"{p.ssz_type} {p.name} = {p.default}")
            else: pp.append(f"{p.ssz_type} {p.name}")
        params = ", ".join(pp)
        ret = func.ret_type.strip()
        if ret in ("void", ""):
            lines.append(f"{pub}void {func.name}({params})")
        else:
            lines.append(f"{pub}{ret} {func.name}({params})")
        lines.append("{")
        for bl in func.body: lines.append(bl)
        return "\n".join(lines)


# ─── Main ───────────────────────────────────────────────────────────────────

def transpile(input_path, lib_name=None, output_dir=None):
    with open(input_path, "r", encoding="utf-8-sig") as f:
        source = f.read()

    filename = os.path.basename(input_path)
    if lib_name is None: lib_name = os.path.splitext(filename)[0]
    if output_dir is None: output_dir = os.path.dirname(input_path) or "."

    print(f"Transpiling {filename} (lib: {lib_name})...")

    parser = SSZParser(source, filename)
    parser.parse()

    print(f"  Parsed: {len(parser.structs)} structs, "
          f"{len(parser.functions)} functions, "
          f"{len(parser.module_vars)} module vars, "
          f"{len(parser.enums)} enums")

    for s in parser.structs:
        print(f"    &{s.name}: {len(s.fields)} fields, {len(s.methods)} methods")
        for f in s.fields:
            print(f"      field: {f.ssz_type} {f.name}" + (f" = {f.default}" if f.default else ""))
        for m in s.methods:
            print(f"      method: {m.ret_type} {m.name}({len(m.params)} params, {len(m.body)} body lines)")

    gen = CppGen(parser, lib_name)
    cpp = gen.gen_cpp()
    wrapper = gen.gen_wrapper()

    os.makedirs(output_dir, exist_ok=True)

    cpp_path = os.path.join(output_dir, f"{lib_name}.cpp")
    wrap_path = os.path.join(output_dir, f"{lib_name}_wrapper.ssz")

    with open(cpp_path, "w", encoding="utf-8") as f:
        f.write(cpp)
    print(f"  Generated: {cpp_path}")

    with open(wrap_path, "w", encoding="utf-8") as f:
        f.write(wrapper)
    print(f"  Generated: {wrap_path}")

    return cpp, wrapper

def main():
    import argparse
    ap = argparse.ArgumentParser(description="SSZ-to-C++ transpiler")
    ap.add_argument("input", help="Input .ssz file")
    ap.add_argument("--lib-name", help="Native library name")
    ap.add_argument("--output-dir", help="Output directory")
    args = ap.parse_args()
    transpile(args.input, args.lib_name, args.output_dir)

if __name__ == "__main__":
    main()
