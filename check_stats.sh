#!/bin/sh
ls -la "C:/Projects/ikemen-new-ultra/install/test_result_install.log" "C:/Projects/ikemen-new-ultra/build/Debug/test_file.exe"
echo "---"
grep -c "^PASS" "C:/Projects/ikemen-new-ultra/install/test_result_install.log"
echo "PASS count printed above"
grep -c "^FAIL" "C:/Projects/ikemen-new-ultra/install/test_result_install.log"
echo "FAIL count printed above"