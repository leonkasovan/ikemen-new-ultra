#!/bin/sh
# Math module parity test.
# Compares MATH trace categories (mask=32) between SSZ bridge and native C++.
# Only MATH traces are captured, so logs stay small (~7k lines per 15s run).

BASELINE_LOG="trace_math_baseline.log"
NATIVE_LOG="trace_math_native.log"
DIFF_LOG="trace_math_diff.log"
RESULTS_LOG="trace_math_results.txt"

run_with_timeout() {
    local logfile="$1"
    shift
    echo "Running: $@ > $logfile 2>&1 (timeout 15s)"
    "$@" > "$logfile" 2>&1 &
    sleep 15
    taskkill /f /im ikemen-debug.exe >/dev/null 2>&1
    echo "Finished (killed after 15s). Log written to $logfile"
}

echo "============================================"
echo " Math Parity Test — Phase 1: Baseline (SSZ bridge)"
echo "============================================"
touch main/ssz/bridge.cpp
touch main/main.cpp
make IKEMEN_ENABLE_PLUGIN_TRACE=1 IKEMEN_TRACE_MASK=32 IKEMEN_USE_NATIVE_SSZ=0 CONFIG=Debug -j8 install
cd install
run_with_timeout "$BASELINE_LOG" ./ikemen-debug.exe
cd ..

echo ""
echo "============================================"
echo " Math Parity Test — Phase 2: Native C++"
echo "============================================"
touch main/ssz/bridge.cpp
touch main/main.cpp
make IKEMEN_ENABLE_PLUGIN_TRACE=1 IKEMEN_TRACE_MASK=32 IKEMEN_USE_NATIVE_SSZ=1 CONFIG=Debug -j8 install
cd install
run_with_timeout "$NATIVE_LOG" ./ikemen-debug.exe
cd ..

echo ""
echo "============================================"
echo " Comparison Results"
echo "============================================"

# Extract just the [TRACE] lines for clean comparison
echo "Extracting MATH trace lines..."
grep "^\[TRACE\]" "install/$BASELINE_LOG" > /tmp/math_baseline_clean.txt 2>/dev/null
grep "^\[TRACE\]" "install/$NATIVE_LOG" > /tmp/math_native_clean.txt 2>/dev/null

echo "" > "install/$RESULTS_LOG"
echo "=== Math Parity Test Results ===" >> "install/$RESULTS_LOG"
echo "Date: $(date)" >> "install/$RESULTS_LOG"
echo "" >> "install/$RESULTS_LOG"

echo "--- Baseline trace counts ---" >> "install/$RESULTS_LOG"
sed -n 's/^\[TRACE\] //p' /tmp/math_baseline_clean.txt 2>/dev/null | sort | uniq -c | sort -rn >> "install/$RESULTS_LOG"
echo "" >> "install/$RESULTS_LOG"

echo "--- Native trace counts ---" >> "install/$RESULTS_LOG"
sed -n 's/^\[TRACE\] //p' /tmp/math_native_clean.txt 2>/dev/null | sort | uniq -c | sort -rn >> "install/$RESULTS_LOG"
echo "" >> "install/$RESULTS_LOG"

# Compare trace call counts
echo "--- Function-by-function comparison ---" >> "install/$RESULTS_LOG"
{
    echo "FUNCTION BASELINE NATIVE MATCH"
    for func in Sin Cos Tan ASin ACos ATan Log Ln Exp Sqrt Ceil Floor IsFinite IsInf IsNaN Random Rand RandI RandF SRand; do
        bc=$(grep -c "\[TRACE\] $func" /tmp/math_baseline_clean.txt 2>/dev/null || echo 0)
        nc=$(grep -c "\[TRACE\] $func" /tmp/math_native_clean.txt 2>/dev/null || echo 0)
        if [ "$bc" = "$nc" ]; then
            match="YES"
        else
            match="NO"
        fi
        printf "%-12s %8s %8s %s\n" "$func" "$bc" "$nc" "$match"
    done
} >> "install/$RESULTS_LOG"

echo "" >> "install/$RESULTS_LOG"
echo "--- Total trace lines ---" >> "install/$RESULTS_LOG"
wc -l /tmp/math_baseline_clean.txt /tmp/math_native_clean.txt >> "install/$RESULTS_LOG"

echo "" >> "install/$RESULTS_LOG"
echo "--- Raw diff (unified) ---" >> "install/$RESULTS_LOG"
diff -u /tmp/math_baseline_clean.txt /tmp/math_native_clean.txt >> "install/$RESULTS_LOG" 2>&1
DIFF_EXIT=$?
if [ $DIFF_EXIT -eq 0 ]; then
    echo "SUCCESS: MATH trace logs are IDENTICAL." >> "install/$RESULTS_LOG"
else
    echo "WARNING: MATH trace logs DIFFER (exit code $DIFF_EXIT)." >> "install/$RESULTS_LOG"
fi

# Summary to stdout
echo ""
echo "=== Summary ==="
echo "Baseline MATH traces: $(wc -l < /tmp/math_baseline_clean.txt 2>/dev/null || echo 0)"
echo "Native MATH traces:   $(wc -l < /tmp/math_native_clean.txt 2>/dev/null || echo 0)"
echo "Full results: install/$RESULTS_LOG"
echo "Raw diff:     install/$DIFF_LOG"
echo ""
cat "install/$RESULTS_LOG" | head -50
echo "..."
echo "See install/$RESULTS_LOG for full report."
