#!/usr/bin/env python3
"""Edit test_file.cpp to add #if IKEMEN_NATIVE_TIME_LIB guards."""

path = 'test/test_file.cpp'

with open(path, 'r', encoding='utf-8', newline='') as f:
    content = f.read()

# 1. Guard the #include of time_service
old_inc = '#include "ssz_native/thread_service.hpp"\n#include "ssz_native/time_service.hpp"\n#include "ssz_native/shell_service.hpp"'
new_inc = '#include "ssz_native/thread_service.hpp"\n#if IKEMEN_NATIVE_TIME_LIB\n#include "ssz_native/time_service.hpp"\n#endif\n#include "ssz_native/shell_service.hpp"'

assert old_inc in content, 'Include pattern not found!'
content = content.replace(old_inc, new_inc, 1)

# 2. Guard the call to test_time_service()
old_call = '    test_thread_service();\n    test_time_service();\n    test_table_service();'
new_call = '    test_thread_service();\n#if IKEMEN_NATIVE_TIME_LIB\n    test_time_service();\n#endif\n    test_table_service();'

assert old_call in content, 'Call pattern not found!'
content = content.replace(old_call, new_call, 1)

with open(path, 'w', encoding='utf-8', newline='') as f:
    f.write(content)

print('Done - test_file.cpp updated with IKEMEN_NATIVE_TIME_LIB guards')
