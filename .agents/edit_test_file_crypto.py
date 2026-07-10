#!/usr/bin/env python3
"""Edit test_file.cpp to add #if IKEMEN_NATIVE_CRYPTO_LIB guards."""

path = 'test/test_file.cpp'

with open(path, 'rb') as f:
    content = f.read()

changes = 0

# Guard the test function call
old_call = b'    test_alert_service();\r\n    test_crypto_service();\r\n    test_shell_service();'
new_call = b'    test_alert_service();\r\n#if IKEMEN_NATIVE_CRYPTO_LIB\r\n    test_crypto_service();\r\n#endif\r\n    test_shell_service();'

if old_call in content:
    content = content.replace(old_call, new_call, 1)
    print("call guard applied!")
    changes += 1
else:
    print("call pattern NOT found!")
    idx = content.find(b'test_crypto_service();')
    if idx >= 0:
        print(f"Found 'test_crypto_service();' at offset {idx}")
        print(f"Context: {repr(content[max(0,idx-60):idx+80])}")

print(f"Total changes: {changes}")

with open(path, 'wb') as f:
    f.write(content)

print("Done!")
