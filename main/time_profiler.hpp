#pragma once

#include <stdint.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <chrono>

#include "crash_log.hpp"

// =========================================================================
//  Time Profiler  –  cumulative per-region CPU time, ranked at exit
//
//  The CPU analogue of mem_profiler.hpp. Accumulates total time spent in
//  named regions over the whole session, then TimePrintRanking() prints a
//  sorted report (largest total first) — the go-pprof-style "top" list.
//
//  Usage:
//    TIME_SCOPE(tag)                       RAII — time a whole function/block
//    TIME_MARK_BEFORE(tag) ...             manual start/end around a span
//    TIME_MARK_AFTER(tag)
//    TimeAccumulateMs("name", ms)          add raw timings (e.g. per frame)
//
//  Cost model (H4): recording is gated on the runtime flag g_profilerLog and
//  name lookup goes through a hash index, so a disabled profiler costs one
//  predictable branch and an enabled one costs O(1) per mark instead of a
//  linear scan of std::string comparisons. Scripts call .ssz.profBegin/End 25
//  times per frame, so this was measurable in Release builds.
//
//  Nested regions double-count wall time (scope-in-scope adds both totals),
//  which is fine for a ranked overview. Tags must be unique per function
//  (the macro derives the variable name from the tag).
// =========================================================================

struct TimeSample
{
	std::string name;
	double      totalMs;   // cumulative milliseconds
	uint64_t    calls;
};

// Defined once in ssz.cpp, alongside g_memEvents
extern std::vector<TimeSample> g_timeSamples;
// name -> index into g_timeSamples (defined in ssz.cpp)
extern std::unordered_map<std::string, size_t> g_timeIndex;

// --------------------------------------------------
// Accumulate a named timing (single-threaded game loop — no locking)
// --------------------------------------------------
static inline void TimeAccumulateMs(const char* name, double ms)
{
	if (!g_profilerLog) return;   // H4: recording, not just printing, is gated
	if (ms < 0.0) ms = 0.0;
	auto it = g_timeIndex.find(name);
	if (it != g_timeIndex.end())
	{
		TimeSample& s = g_timeSamples[it->second];
		s.totalMs += ms;
		s.calls++;
	}
	else
	{
		TimeSample s;
		s.name    = name;
		s.totalMs = ms;
		s.calls   = 1;
		g_timeIndex[s.name] = g_timeSamples.size();
		g_timeSamples.push_back(s);
	}
	CrashRecordTime(name, ms);   // C4: allocation-free mirror for the crash path
}

static inline void TimeAccumulateUs(const char* name, double us)
{
	TimeAccumulateMs(name, us / 1000.0);
}

// --------------------------------------------------
// RAII scope timer
// --------------------------------------------------
struct TimeScope
{
	const char* m_name;
	std::chrono::steady_clock::time_point m_start;

	TimeScope(const char* name)
		: m_name(name), m_start(std::chrono::steady_clock::now())
	{
	}

	~TimeScope()
	{
		double ms = (double)std::chrono::duration_cast<std::chrono::microseconds>(
			std::chrono::steady_clock::now() - m_start).count() / 1000.0;
		TimeAccumulateMs(m_name, ms);
	}
};

// Use like:  TIME_SCOPE(load_stage);
#define TIME_SCOPE(tag) \
	TimeScope _time_scope_##tag(#tag)

// --------------------------------------------------
// Manual block marks — use like:
//   TIME_MARK_BEFORE(load_stage);
//   ... work ...
//   TIME_MARK_AFTER(load_stage);
// --------------------------------------------------
#define TIME_MARK_BEFORE(tag) \
	std::chrono::steady_clock::time_point _t0_##tag = std::chrono::steady_clock::now();

#define TIME_MARK_AFTER(tag) \
	TimeAccumulateMs(#tag, (double)std::chrono::duration_cast<std::chrono::microseconds>( \
		std::chrono::steady_clock::now() - _t0_##tag).count() / 1000.0);

// --------------------------------------------------
// Print ranked report (largest total first)
// --------------------------------------------------
static inline void TimePrintRanking()
{
	if (!g_profilerLog) return;
	if (g_timeSamples.empty())
	{
		LOG_INFO("Time", "==== TIME USAGE RANKING ====");
		LOG_INFO("Time", "(no samples recorded)");
		return;
	}

	std::sort(g_timeSamples.begin(), g_timeSamples.end(),
		[](const TimeSample& a, const TimeSample& b) {
			return a.totalMs > b.totalMs;
		});
	// Indices are stale after the sort.
	g_timeIndex.clear();

	LOG_INFO("Time", "==== TIME USAGE RANKING ====");
	LOG_INFO("Time", "  %-32s %12s %10s %12s", "region", "total(ms)", "calls", "avg(ms)");
	for (size_t i = 0; i < g_timeSamples.size(); i++)
	{
		const TimeSample& e = g_timeSamples[i];
		double avg = e.calls > 0 ? e.totalMs / (double)e.calls : 0.0;
		LOG_INFO("Time", "  %-32s %12.2f %10llu %12.4f",
			e.name.c_str(), e.totalMs,
			(unsigned long long)e.calls, avg);
	}
}
