#!/usr/bin/env python3
"""Edit test_file.cpp to add #if IKEMEN_NATIVE_STRING_LIB guards."""

import os

path = 'test/test_file.cpp'

with open(path, 'rb') as f:
    content = f.read()

# First replacement: guard the #include
old_inc = b'#include "ssz_native/sound_service.hpp"\r\n#include "ssz_native/string_service.hpp"\r\n#include "ssz_native/ogg_service.hpp"'
new_inc = b'#include "ssz_native/sound_service.hpp"\r\n#if IKEMEN_NATIVE_STRING_LIB\r\n#include "ssz_native/string_service.hpp"\r\n#endif\r\n#include "ssz_native/ogg_service.hpp"'

assert old_inc in content, "include pattern not found!"
content = content.replace(old_inc, new_inc, 1)

# Second replacement: guard the test function calls
# Find the exact location after test_math_service()
old_calls = b'    test_math_service();\r\n    test_string_service();\r\n    test_format_service();\r\n    test_thread_service();'
new_calls = b'    test_math_service();\r\n#if IKEMEN_NATIVE_STRING_LIB\r\n    test_string_service();\r\n    test_format_service();\r\n#endif\r\n    test_thread_service();'

assert old_calls in content, "calls pattern not found!\nFound at: " + str(content.find(b'test_string_service'))
content = content.replace(old_calls, new_calls, 1)

with open(path, 'wb') as f:
    f.write(content)

print("All replacements applied successfully!")
