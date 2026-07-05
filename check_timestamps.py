import os, datetime, sys

files = [
    r'c:\Projects\ikemen-new-ultra\install\test_result_install.log',
    r'c:\Projects\ikemen-new-ultra\build\Debug\test_file.exe'
]

with open(r'c:\Projects\ikemen-new-ultra\timestamp_output.txt', 'w') as out:
    for f in files:
        if os.path.exists(f):
            mtime = os.path.getmtime(f)
            dt = datetime.datetime.fromtimestamp(mtime)
            out.write(f"{f}\n")
            out.write(f"  Exists: Yes, Modified: {dt}\n")
            out.write(f"  Size: {os.path.getsize(f)} bytes\n")
        else:
            out.write(f"{f}: NOT FOUND\n")

    # Compare
    log = r'c:\Projects\ikemen-new-ultra\install\test_result_install.log'
    exe = r'c:\Projects\ikemen-new-ultra\build\Debug\test_file.exe'
    if os.path.exists(log) and os.path.exists(exe):
        if os.path.getmtime(exe) > os.path.getmtime(log):
            out.write("RESULT: Binary (test_file.exe) is NEWER than log\n")
        else:
            out.write("RESULT: Binary (test_file.exe) is OLDER or same age as log\n")
