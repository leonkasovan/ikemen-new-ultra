#pragma once

#include <stdint.h>
#include <string>
#include <vector>
#include <algorithm>

// =========================================================================
//  Memory Profiler  –  track per-subsystem allocation deltas
//
//  g_allocBytes / g_freeBytes must be defined in the allocator (ssz.cpp)
//  and declared extern in sszdef.h.
// =========================================================================

extern volatile int64_t g_allocBytes;
extern volatile int64_t g_freeBytes;

// --------------------------------------------------
// Get current live (logically allocated) byte count
// --------------------------------------------------
static inline uint64_t GetLiveMemory()
{
	return (uint64_t)(g_allocBytes - g_freeBytes);
}

// --------------------------------------------------
// Snapshot struct
// --------------------------------------------------
struct MemorySnapshot
{
	std::string name;
	uint64_t before;
	uint64_t after;
	int64_t delta;
};

extern std::vector<MemorySnapshot> g_memEvents;

// --------------------------------------------------
// Record a snapshot pair
// --------------------------------------------------
static inline void MemRecord(const char* name, uint64_t before, uint64_t after)
{
	MemorySnapshot s;
	s.name   = name;
	s.before = before;
	s.after  = after;
	s.delta  = (int64_t)(after - before);
	g_memEvents.push_back(s);
}

// --------------------------------------------------
// Print ranked report (largest delta first)
// --------------------------------------------------
static inline void MemPrintRanking()
{
	if (g_memEvents.empty())
	{
		LOG_INFO("Memory", "==== MEMORY USAGE RANKING ====");
		LOG_INFO("Memory", "(no events recorded)");
		return;
	}

	std::sort(g_memEvents.begin(), g_memEvents.end(),
		[](const MemorySnapshot& a, const MemorySnapshot& b) {
			return a.delta > b.delta;
		});

	LOG_INFO("Memory", "==== MEMORY USAGE RANKING ====");
	for (size_t i = 0; i < g_memEvents.size(); i++)
	{
		const auto& e = g_memEvents[i];
		LOG_INFO("Memory",
			"  %s : delta=%+lld bytes  (before=%llu  after=%llu)",
			e.name.c_str(), (long long)e.delta,
			(unsigned long long)e.before,
			(unsigned long long)e.after);
	}

	uint64_t live = GetLiveMemory();
	LOG_INFO("Memory", "==== LIVE MEMORY: %llu bytes ====",
		(unsigned long long)live);
}

// --------------------------------------------------
// Macros – use like:
//
//   void loadSomething() {
//       MEM_MARK_BEFORE(SOME_TAG);
//       ... do loading ...
//       MEM_MARK_AFTER(SOME_TAG);
//   }
// --------------------------------------------------
#define MEM_MARK_BEFORE(tag) \
	uint64_t mem_before_##tag = GetLiveMemory(); \
	LOG_INFO("Memory", "[%s] BEFORE = %llu", #tag, (unsigned long long)mem_before_##tag);

#define MEM_MARK_AFTER(tag) \
	uint64_t mem_after_##tag = GetLiveMemory(); \
	int64_t mem_delta_##tag = (int64_t)(mem_after_##tag - mem_before_##tag); \
	LOG_INFO("Memory", "[%s] AFTER  = %llu  (delta=%+lld)", \
		#tag, (unsigned long long)mem_after_##tag, (long long)mem_delta_##tag); \
	MemRecord(#tag, mem_before_##tag, mem_after_##tag);

// Named variants – include a per-instance identifier (e.g. file path)
#define MEM_MARK_BEFORE_NAMED(tag, name) \
	uint64_t mem_before_##tag = GetLiveMemory(); \
	LOG_INFO("Memory", "[%s %s] BEFORE = %llu", #tag, name, (unsigned long long)mem_before_##tag);

#define MEM_MARK_AFTER_NAMED(tag, name) \
	uint64_t mem_after_##tag = GetLiveMemory(); \
	int64_t mem_delta_##tag = (int64_t)(mem_after_##tag - mem_before_##tag); \
	LOG_INFO("Memory", "[%s %s] AFTER  = %llu  (delta=%+lld)", \
		#tag, name, (unsigned long long)mem_after_##tag, (long long)mem_delta_##tag); \
	{ std::string _mem_tag = std::string(#tag) + " " + name; \
		MemRecord(_mem_tag.c_str(), mem_before_##tag, mem_after_##tag); }
