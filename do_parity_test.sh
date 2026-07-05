#!/bin/sh
# Compare the two logs to verify that the native SSZ plugin calls match the baseline.
# Test cases for tracing native SSZ plugin calls. Run these scripts to generate trace logs for comparison:

BASELINE_LOG="trace_method1_baseline.log"
NATIVE_LOG="trace_method1_native.log"
DIFF_LOG="trace_parity_diff.log"

run_with_timeout() {
    local logfile="$1"
    shift
    echo "Running: $@ > $logfile 2>&1 (timeout 15s)"
    # Start GUI process in background (&), wait, then force-kill via Windows taskkill
    "$@" > "$logfile" 2>&1 &
    sleep 15
    taskkill /f /im ikemen-debug.exe >/dev/null 2>&1
    echo "Finished (killed after 15s). Log written to $logfile"
}

touch main/ssz/bridge.cpp
touch main/main.cpp
make IKEMEN_ENABLE_PLUGIN_TRACE=1 IKEMEN_TRACE_MASK=254 IKEMEN_USE_NATIVE_SSZ=0 CONFIG=Debug -j8 install
cd install
run_with_timeout "$BASELINE_LOG" ./ikemen-debug.exe
cd ..

touch main/ssz/bridge.cpp
touch main/main.cpp
make IKEMEN_ENABLE_PLUGIN_TRACE=1 IKEMEN_TRACE_MASK=254 IKEMEN_USE_NATIVE_SSZ=1 CONFIG=Debug -j8 install
cd install
run_with_timeout "$NATIVE_LOG" ./ikemen-debug.exe
cd ..

echo ""
echo "=== Comparing baseline vs native logs ==="
diff -u "install/$BASELINE_LOG" "install/$NATIVE_LOG" > "install/$DIFF_LOG" 2>&1
if [ $? -eq 0 ]; then
    echo "SUCCESS: Logs are IDENTICAL."
else
    echo "WARNING: Logs DIFFER. See install/$DIFF_LOG for details."
    echo "Summary of differences:"
    diff --stat "install/$BASELINE_LOG" "install/$NATIVE_LOG"
fi