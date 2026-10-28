#pragma once
//
// crash_log.hpp — allocation-free crash reporting for the profilers.
//
// C4 in PROJECT_REVIEW.md: the old crash path called MemPrintRanking() /
// TimePrintRanking() from a Vectored Exception Handler. Those functions
// std::sort a std::vector<std::string> and printf. When the fault was heap
// corruption — precisely the case the tool exists to diagnose — the handler
// re-entered a corrupt allocator, which turns "crash" into "hang with no
// output". The TLS callback had a second problem: it fires at
// DLL_PROCESS_DETACH, i.e. after the namespace-scope vectors have been
// destroyed, so it read freed memory on every clean exit.
//
// This header mirrors the profiler totals into a fixed POD table as they are
// recorded (no allocation ever), so the handler can dump a useful summary
// using only stack buffers and WriteFile(). The rich, sorted reports still run
// from atexit, where the C++ runtime is fully alive.
//

#include <stdint.h>
#include <stddef.h>

struct CrashRecord
{
	char     name[40];   // truncated, NUL-terminated
	double   ms;         // cumulative milliseconds
	uint64_t calls;
};

static const int CRASH_RECORD_MAX = 128;

extern CrashRecord   g_crashRecords[CRASH_RECORD_MAX];
extern volatile long g_crashRecordCount;

/// Mirror one timing sample into the crash table (called from TimeAccumulateMs).
void CrashRecordTime(const char* name, double ms);

/// Dump a crash summary to stdout (falls back to stderr) using only
/// pre-formatted text and WriteFile — no allocation, no locks, no STL.
/// `reason` is a short tag such as "access violation"; `faultAddress` may be
/// NULL. Safe to call from an exception handler.
void CrashDumpReport(const char* reason, void* faultAddress);
