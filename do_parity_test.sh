#!/bin/sh
# Compare the two logs to verify that the native SSZ plugin calls match the baseline.
# Test cases for tracing native SSZ plugin calls. Run these scripts to generate trace logs for comparison:

touch main/ssz/bridge.cpp
touch main/main.cpp
make IKEMEN_ENABLE_PLUGIN_TRACE=1 IKEMEN_TRACE_MASK=254 IKEMEN_USE_NATIVE_SSZ=0 CONFIG=Debug -j8 install
cd install; ./ikemen-debug.exe > trace_method1_baseline.log 2>&1 ; cd ..

touch main/ssz/bridge.cpp
touch main/main.cpp
make IKEMEN_ENABLE_PLUGIN_TRACE=1 IKEMEN_TRACE_MASK=254 IKEMEN_USE_NATIVE_SSZ=1 CONFIG=Debug -j8 install
cd install; ./ikemen-debug.exe > trace_method1_native.log 2>&1 ; cd ..