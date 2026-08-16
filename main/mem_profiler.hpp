#pragma once

#include <stdint.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif

// =========================================================================
//  Memory Profiler  –  track per-subsystem allocation deltas
//
//  g_allocBytes / g_freeBytes must be defined in the allocator (ssz.cpp)
//  and declared extern in sszdef.h.
// =========================================================================

extern volatile int64_t g_allocBytes;
extern volatile int64_t g_freeBytes;

// Peak logically-live SSZ heap bytes (max of live = alloc - free);
// maintained by the allocator in ssz.cpp.
extern volatile int64_t g_peakLiveBytes;

// --------------------------------------------------
// Get current live (logically allocated) byte count
// --------------------------------------------------
static inline uint64_t GetLiveMemory()
{
	return (uint64_t)(g_allocBytes - g_freeBytes);
}

// --------------------------------------------------
// Peak live SSZ heap bytes seen so far
// --------------------------------------------------
static inline uint64_t GetPeakLiveMemory()
{
	return (uint64_t)g_peakLiveBytes;
}

// =========================================================================
//  Process-wide memory  –  what Task Manager shows (working set / private)
//
//  Samples WorkingSetSize (Task Manager "Memory" column) and private
//  committed bytes (PagefileUsage) via GetProcessMemoryInfo, dynamically
//  resolved from kernel32 (K32GetProcessMemoryInfo) so no extra linker lib
//  is required. Used to explain the gap between the SSZ-heap accounting and
//  the ~200 MB resident footprint the engine shows next to real M.U.G.E.N.
// =========================================================================

struct ProcessMemSample
{
	uint64_t workingSet;   // bytes (Task Manager "Memory" column)
	uint64_t privateBytes; // bytes (committed private, "Commit Size" minus shared)
	uint64_t frame;        // frame counter at sample time
	uint32_t tickMs;       // SDL_GetTicks() at sample time
};

struct ProcessMemMilestone
{
	std::string name;
	uint64_t workingSet;
	uint64_t privateBytes;
	uint32_t tickMs;
};

extern std::vector<ProcessMemSample>    g_memTimeline;
extern std::vector<ProcessMemMilestone> g_memMilestones;

// Query the OS for current working set / private committed bytes.
// Returns false when unavailable (non-Windows or API missing).
static inline bool MemQueryProcess(uint64_t& workingSet, uint64_t& privateBytes)
{
#ifdef _WIN32
	typedef BOOL (WINAPI* GetProcessMemoryInfoFn)(HANDLE, PPROCESS_MEMORY_COUNTERS, DWORD);
	static GetProcessMemoryInfoFn fn = nullptr;
	if (fn == nullptr)
	{
		fn = (GetProcessMemoryInfoFn)GetProcAddress(
			GetModuleHandleA("kernel32.dll"), "K32GetProcessMemoryInfo");
		if (fn == nullptr)
			fn = (GetProcessMemoryInfoFn)GetProcAddress(
				GetModuleHandleA("psapi.dll"), "GetProcessMemoryInfo");
	}
	if (fn == nullptr) return false;

	PROCESS_MEMORY_COUNTERS pmc;
	memset(&pmc, 0, sizeof(pmc));
	pmc.cb = sizeof(pmc);
	if (!fn(GetCurrentProcess(), &pmc, sizeof(pmc))) return false;
	workingSet   = pmc.WorkingSetSize;
	privateBytes = pmc.PagefileUsage;
	return true;
#else
	(void)workingSet; (void)privateBytes;
	return false;
#endif
}

// Record a timeline sample (call ~once per second from the frame loop).
static inline void MemSampleProcess(uint64_t frame, uint32_t tickMs)
{
	uint64_t ws = 0, priv = 0;
	if (!MemQueryProcess(ws, priv)) return;
	ProcessMemSample s;
	s.workingSet   = ws;
	s.privateBytes = priv;
	s.frame        = frame;
	s.tickMs       = tickMs;
	g_memTimeline.push_back(s);
}

// Record a named milestone snapshot (e.g. "STAGE-LOADED") — see MemMarkProcess.
static inline void MemMarkProcess(const char* name)
{
	uint64_t ws = 0, priv = 0;
	if (!MemQueryProcess(ws, priv)) return;
	ProcessMemMilestone m;
	m.name         = name;
	m.workingSet   = ws;
	m.privateBytes = priv;
	m.tickMs       = 0;
	g_memMilestones.push_back(m);
}

// Print the process-memory report: peaks, milestones with deltas, timeline.
static inline void MemPrintProcess()
{
	// Peaks are derived from the recorded samples/milestones (shared vectors)
	// rather than maintained in per-TU statics, so every translation unit
	// sees the same numbers.
	uint64_t peakWs = 0, peakPriv = 0;
	for (size_t i = 0; i < g_memTimeline.size(); i++)
	{
		if (g_memTimeline[i].workingSet   > peakWs)   peakWs   = g_memTimeline[i].workingSet;
		if (g_memTimeline[i].privateBytes > peakPriv) peakPriv = g_memTimeline[i].privateBytes;
	}
	for (size_t i = 0; i < g_memMilestones.size(); i++)
	{
		if (g_memMilestones[i].workingSet   > peakWs)   peakWs   = g_memMilestones[i].workingSet;
		if (g_memMilestones[i].privateBytes > peakPriv) peakPriv = g_memMilestones[i].privateBytes;
	}

	LOG_INFO("Memory", "==== PROCESS MEMORY (Task Manager working set) ====");
	uint64_t ws = 0, priv = 0;
	bool ok = MemQueryProcess(ws, priv);
	if (ok)
	{
		LOG_INFO("Memory", "  final   working set : %7.1f MB   private : %7.1f MB",
			(double)ws / 1048576.0, (double)priv / 1048576.0);
	}
	LOG_INFO("Memory", "  peak    working set : %7.1f MB   private : %7.1f MB",
		(double)peakWs / 1048576.0, (double)peakPriv / 1048576.0);
	LOG_INFO("Memory", "  peak SSZ live heap  : %7.1f MB   (allocator-tracked)",
		(double)GetPeakLiveMemory() / 1048576.0);
	LOG_INFO("Memory", "  JIT machine code     : %7.1f MB   (VirtualAlloc, cumulative)",
		(double)g_jitCodeBytes / 1048576.0);
	LOG_INFO("Memory", "  JIT literal data     : %7.1f MB   (per-func gre buffers)",
		(double)g_jitDataBytes / 1048576.0);
	LOG_INFO("Memory", "  timeline samples    : %llu", (unsigned long long)g_memTimeline.size());

	if (!g_memMilestones.empty())
	{
		LOG_INFO("Memory", "  %-24s %10s %10s %10s", "milestone", "ws(MB)", "priv(MB)", "dws(MB)");
		int64_t prevWs = 0;
		for (size_t i = 0; i < g_memMilestones.size(); i++)
		{
			const ProcessMemMilestone& e = g_memMilestones[i];
			double dws = i == 0 ? 0.0 : (double)((int64_t)e.workingSet - prevWs) / 1048576.0;
			LOG_INFO("Memory", "  %-24s %10.1f %10.1f %+10.1f",
				e.name.c_str(),
				(double)e.workingSet / 1048576.0,
				(double)e.privateBytes / 1048576.0,
				dws);
			prevWs = (int64_t)e.workingSet;
		}
	}

	if (!g_memTimeline.empty())
	{
		LOG_INFO("Memory", "  timeline (working set / private MB, ~1 sample/sec):");
		// Print in compact rows of 4 samples; t(ms) is the offset from start
		uint32_t t0 = g_memTimeline[0].tickMs;
		for (size_t i = 0; i < g_memTimeline.size(); i += 4)
		{
			char row[256];
			int len = 0;
			for (size_t j = i; j < i + 4 && j < g_memTimeline.size(); j++)
			{
				const ProcessMemSample& s = g_memTimeline[j];
				len += _snprintf(row + len, sizeof(row) - len, "[%4.1fs %6.1f/%6.1f] ",
					(double)(s.tickMs - t0) / 1000.0,
					(double)s.workingSet / 1048576.0,
					(double)s.privateBytes / 1048576.0);
			}
			row[len] = '\0';
			LOG_INFO("Memory", "    %s", row);
		}
	}
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

// Defined in ssz.cpp — cumulative SSZ alloc-size histogram (printed by
// MemPrintRanking at exit).
void MemPrintHistogram();

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
	LOG_INFO("Memory", "==== LIVE MEMORY: %llu bytes (%llu KB) ====",
		(unsigned long long)live, (unsigned long long)(live / 1024));
	LOG_INFO("Memory", "==== PEAK SSZ LIVE HEAP: %llu bytes (%llu KB) ====",
		(unsigned long long)GetPeakLiveMemory(),
		(unsigned long long)(GetPeakLiveMemory() / 1024));
	MemPrintHistogram();
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
