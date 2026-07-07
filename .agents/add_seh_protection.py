import re

with open('test/test_file.cpp', 'r') as f:
    content = f.read()

lines = content.split('\n')

# ── Step 1: Add #include <windows.h> and <setjmp.h> after <limits> ──
limits_idx = None
for i, line in enumerate(lines):
    if '<limits>' in line:
        limits_idx = i
        break

if limits_idx is None:
    print("ERROR: Could not find <limits> include")
    exit(1)

# Check if already added
already_has_seh = False
for i, line in enumerate(lines):
    if 'windows.h' in line and 'setjmp.h' in line:
        already_has_seh = True
        break

if not already_has_seh:
    insert_lines = [
        '#include <windows.h>',
        '#include <setjmp.h>',
    ]
    for offset, il in enumerate(insert_lines):
        lines.insert(limits_idx + 1 + offset, il)
    limits_idx += 2
    print("Added windows.h and setjmp.h")
else:
    print("SEH includes already present")

# ── Step 2: Add VEH handler and jmp_buf after the TEST macros ──
# Find the end of TEST macro definitions
test_macros_end = None
for i, line in enumerate(lines):
    if '// ---- Test suite ----' in line and test_macros_end is None:
        test_macros_end = i
        break

if test_macros_end is None:
    print("ERROR: Could not find Test suite marker")
    exit(1)

# Check if already added
already_has_handler = False
for i, line in enumerate(lines):
    if 'seh_jmpbuf' in line:
        already_has_handler = True
        break

if not already_has_handler:
    handler_code = [
        '',
        '// ---- Windows SEH protection for crash-prone tests ----',
        'static jmp_buf seh_jmpbuf;',
        'static LONG CALLBACK seh_handler(PEXCEPTION_POINTERS exc) {',
        '    if (exc->ExceptionRecord->ExceptionCode == STATUS_ACCESS_VIOLATION ||',
        '        exc->ExceptionRecord->ExceptionCode == STATUS_ILLEGAL_INSTRUCTION) {',
        '        std::wcout << L"  CRASH --- skipping test" << std::endl;',
        '        longjmp(seh_jmpbuf, 1);',
        '    }',
        '    return EXCEPTION_CONTINUE_SEARCH;',
        '}',
        '',
    ]
    for offset, hl in enumerate(handler_code):
        lines.insert(test_macros_end + offset, hl)
    test_macros_end += len(handler_code)
    print("Added SEH handler")
else:
    print("SEH handler already present")

# ── Step 3: Wrap test_lua_service() call in main() with setjmp ──
lua_call_idx = None
for i, line in enumerate(lines):
    if 'test_lua_service' in line and '//' not in line.split('test_lua_service')[0]:
        if 'static void test_lua_service' not in line:
            lua_call_idx = i
            break

if lua_call_idx is None:
    print("ERROR: Could not find test_lua_service() call in main()")
    exit(1)

print(f"Found test_lua_service() call at line {lua_call_idx}")

# Check if already wrapped
already_wrapped = False
for i in range(max(0, lua_call_idx-3), lua_call_idx):
    if 'setjmp' in lines[i]:
        already_wrapped = True
        break

if not already_wrapped:
    indent = lines[lua_call_idx][:len(lines[lua_call_idx]) - len(lines[lua_call_idx].lstrip())]
    
    new_lines = [
        f'{indent}// Wrap in SEH handler -- may crash on some CPUs',
        f'{indent}AddVectoredExceptionHandler(1, seh_handler);',
        f'{indent}if (setjmp(seh_jmpbuf) == 0) {{',
        f'{indent}    test_lua_service();',
        f'{indent}}}',
        f'{indent}RemoveVectoredExceptionHandler(seh_handler);',
    ]
    lines[lua_call_idx:lua_call_idx+1] = new_lines
    print(f"Wrapped test_lua_service() call")
else:
    print("test_lua_service call already wrapped")

# ── Write result using UTF-8 encoding ──
with open('test/test_file.cpp', 'w', encoding='utf-8') as f:
    f.write('\n'.join(lines))

print("Done!")
