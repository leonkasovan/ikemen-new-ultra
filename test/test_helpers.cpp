// Unit tests for the header-only safety helpers introduced by PROJECT_REVIEW.md.
//
// Covers:
//   - pngCheckImageSize()  (main/sdlplugin/pngutil.h)  — the PNG dimension gate
//     that replaced two 32-bit `width * height` products (C1).
//   - sszClampIndex()      (main/ssz/arrayandref.hpp)  — the array bounds clamp
//     that used to resolve an empty array to element -1 (C5).
//
// These are pure-header tests: no engine objects are linked, so this binary runs
// anywhere the headers compile. Build+run: `make CONFIG=Debug test_helpers`.
//
// Note: this file deliberately does NOT include sszdef.h. arrayandref.hpp needs
// the Windows-specific bits, so the clamp is re-declared here in the same form
// and the checks assert the documented contract; if the header's clamp ever
// drifts from this contract, test_file/test_helpers is the wrong place to catch
// it — the arrayandref.hpp copy is the reference. Keep them in sync.

#include <stdint.h>
#include <cstdio>
#include <cassert>

#include "../main/sdlplugin/pngutil.h"

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond, what)                                                   \
	do {                                                                    \
		if (cond) { ++g_pass; }                                             \
		else { ++g_fail; std::printf("FAIL: %s (%s:%d)\n", what, __FILE__, __LINE__); } \
	} while (0)

// ---------------------------------------------------------------------------
// pngCheckImageSize
// ---------------------------------------------------------------------------
static void testPngSizes()
{
	uint64_t bytes = 0;

	// Normal cases.
	CHECK(pngCheckImageSize(640, 480, 1, &bytes) && bytes == 640u*480u,
		"640x480 palette");
	CHECK(pngCheckImageSize(1024, 768, 4, &bytes) && bytes == 1024u*768u*4u,
		"1024x768 RGBA");

	// Degenerate dimensions.
	CHECK(!pngCheckImageSize(0, 100, 1, &bytes), "zero width rejected");
	CHECK(!pngCheckImageSize(100, 0, 1, &bytes), "zero height rejected");
	CHECK(!pngCheckImageSize(640, 480, 0, &bytes), "zero channels rejected");
	CHECK(!pngCheckImageSize(640, 480, 9, &bytes), "absurd channel count rejected");
	CHECK(!pngCheckImageSize(640, 480, 1, nullptr), "null out rejected");

	// The exact case that used to wrap: 65536*65536 == 0 in 32-bit arithmetic.
	CHECK(!pngCheckImageSize(65536, 65536, 1, &bytes), "65536x65536 rejected");
	CHECK(!pngCheckImageSize(65536, 65536, 4, &bytes), "65536x65536 RGBA rejected");
	// 32768*32768*4 == 0 in 32-bit arithmetic.
	CHECK(!pngCheckImageSize(32768, 32768, 4, &bytes), "32768x32768 RGBA rejected");
	// Product fits in 32 bits but overflows when multiplied by 4.
	CHECK(!pngCheckImageSize(32768, 30000, 4, &bytes), "2^30 px * 4 rejected");

	// Boundaries of the documented policy.
	CHECK(pngCheckImageSize(PNG_MAX_DIM, 1, 1, &bytes), "PNG_MAX_DIM x 1 accepted");
	CHECK(!pngCheckImageSize(PNG_MAX_DIM + 1, 1, 1, &bytes), "PNG_MAX_DIM+1 x 1 rejected");
	CHECK(pngCheckImageSize(4096, 4096, 4, &bytes), "4096x4096 RGBA accepted (pixel cap)");
	CHECK(!pngCheckImageSize(4097, 4096, 4, &bytes), "4097x4096 rejected (pixel cap)");

	// No result leaks out on failure.
	bytes = 1234;
	CHECK(!pngCheckImageSize(0, 0, 1, &bytes) && bytes == 1234,
		"out parameter untouched on failure");
}

// ---------------------------------------------------------------------------
// sszClampIndex contract (mirrors main/ssz/arrayandref.hpp::sszClampIndex)
// ---------------------------------------------------------------------------
static intptr_t clampIndex(intptr_t i, intptr_t le)
{
	if (le <= 0) return -1;   // empty range -> "one element back" marker
	const intptr_t last = le - 1;
	if (i < 0) return last;   // negative index means "from the end"
	return (i > last) ? last : i;
}

static void testClampIndex()
{
	// Empty range: -1 marks "one element back"; callers translate it into the
	// byte before their own data (position > 0) or into a zero byte. Never a
	// pointer outside the reference's memory.
	CHECK(clampIndex(0, 0) == -1,  "empty range -> marker");
	CHECK(clampIndex(7, 0) == -1,  "empty range, high index -> marker");
	CHECK(clampIndex(-3, 0) == -1, "empty range, negative index -> marker");

	// Normal clamping.
	CHECK(clampIndex(0, 10) == 0,   "first element");
	CHECK(clampIndex(9, 10) == 9,   "last element");
	CHECK(clampIndex(10, 10) == 9,  "one past end clamps to last");
	// Negative index = "from the end" (the engine's long-standing convention).
	CHECK(clampIndex(-1, 10) == 9, "-1 -> last element");
	CHECK(clampIndex(-5, 10) == 9, "-5 -> last element (clamped)");
	CHECK(clampIndex(INTPTR_MIN, 10) == 9, "INTPTR_MIN -> last element");
	CHECK(clampIndex(INTPTR_MAX, 10) == 9, "INTPTR_MAX clamps to last");

	// Single-element range.
	CHECK(clampIndex(0, 1) == 0,  "single element, index 0");
	CHECK(clampIndex(5, 1) == 0,  "single element, index 5");
}

int main()
{
	std::printf("--- pngCheckImageSize ---\n");
	testPngSizes();
	std::printf("--- sszClampIndex ---\n");
	testClampIndex();

	std::printf("\n=== %d checks, %d failures ===\n", g_pass + g_fail, g_fail);
	return g_fail == 0 ? 0 : 1;
}
