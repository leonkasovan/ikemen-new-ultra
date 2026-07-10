#!/usr/bin/env python3
"""Fix the pre-existing Format '% d' test expectation in test_file.cpp."""
import os

path = 'test/test_file.cpp'

with open(path, 'r', encoding='utf-8', newline='') as f:
    content = f.read()

old = (
    '    // Space sign\n'
    '    {\n'
    '        Fmt fmt;\n'
    '        fmt.set(L"% d");\n'
    '        fmt.d(42);\n'
    '        TEST(L"Format \'% d\': \'  42\'", fmt.out == L"  42");\n'
    '    }'
)

new = (
    '    // Space sign -- SSZ produces " 42" (space + "42", width=0 so no padding)\n'
    '    {\n'
    '        Fmt fmt;\n'
    '        fmt.set(L"% d");\n'
    '        fmt.d(42);\n'
    '        TEST(L"Format \'% d\': \' 42\'", fmt.out == L" 42");\n'
    '    }'
)

assert old in content, 'ERROR: old string not found in test_file.cpp!'
content = content.replace(old, new, 1)

with open(path, 'w', encoding='utf-8', newline='') as f:
    f.write(content)

print('Done - Format test expectation fixed')
