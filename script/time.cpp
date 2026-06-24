#include "time.hpp"

#include <chrono>

#ifdef _WIN32
#include <windows.h>
#endif

namespace ikemen {

// ── OS time / tick ───────────────────────────────────────────────────────

uint32_t tickCount()
{
#ifdef _WIN32
	return GetTickCount();
#else
	auto now = std::chrono::steady_clock::now().time_since_epoch();
	return static_cast<uint32_t>(
		std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
#endif
}

int64_t unixTime()
{
	return static_cast<int64_t>(std::time(nullptr));
}

// ── Floor division & modulo (always toward −∞) ──────────────────────────

int64_t div(int64_t x, int64_t y)
{
	if ((x < 0) ^ (y < 0)) {
		int64_t absY = y < 0 ? -y : y;
		return (x + (absY - 1) * (x < 0 ? -1 : 1)) / y;
	}
	return x / y;
}

int64_t mod(int64_t x, int64_t y)
{
	int64_t a = x % y;
	return a + (a < 0 ? (y < 0 ? -y : y) : 0);
}

// ── Calendar conversion ──────────────────────────────────────────────────

namespace {

const std::array<int32_t, 12> MDAYS = {31, 28, 31, 30, 31, 30,
                                        31, 31, 30, 31, 30, 31};

const int64_t EPOCHDAYS = 1969LL * 365 + 1969 / 4 - 1969 / 100 + 1969 / 400;

} // namespace

int64_t days(int64_t unix)
{
	return div(unix, 24 * 3600) + EPOCHDAYS;
}

std::vector<int32_t> ymdhms(int64_t unix)
{
	std::vector<int32_t> out(6);
	out[5] = static_cast<int32_t>(mod(unix, 60));
	out[4] = static_cast<int32_t>(mod(unix, 3600) / 60);
	out[3] = static_cast<int32_t>(mod(div(unix, 3600), 24));

	int64_t day = days(unix);

	const int64_t y400days = 400LL * 365 + 100 - 4 + 1;
	out[0] = static_cast<int32_t>(1 + div(day, y400days) * 400);
	out[2] = mod(day, y400days);

	const int64_t y100days = 100LL * 365 + 25 - 1;
	out[0] += static_cast<int32_t>(div(out[2], y100days) * 100);
	out[2] = mod(out[2], y100days);

	const int64_t y4days = 4LL * 365 + 1;
	out[0] += static_cast<int32_t>(div(out[2], y4days) * 4);
	out[2] = mod(out[2], y4days);

	out[0] += static_cast<int32_t>(div(out[2], 365));
	out[2] = mod(out[2], 365);

	out[1] = 0;
	for (int i = 0; i < 12; i++) {
		int32_t tmp = MDAYS[i] + (i == 1
			&& out[0] % 4 == 0
			&& (out[0] % 100 != 0 || out[0] % 400 == 0) ? 1 : 0);
		if (out[2] < tmp)
			break;
		out[2] -= tmp;
		out[1]++;
	}
	out[2] += 1;
	out[1] += 1;

	return out;
}

int64_t ymdhmsToUnixTime(int64_t Y, int8_t M, int64_t D,
                          int64_t h, int64_t m, int64_t s)
{
	int64_t y = Y - 1;
	int64_t unix =
		(y * 365 + div(y, 4) - div(y, 100) + div(y, 400) - EPOCHDAYS + D - 1)
		* (24 * 3600)
		+ h * 3600 + m * 60 + s;

	for (int i = 0; i < M - 1; i++) {
		unix += (24 * 3600) * (
			MDAYS[i] + (i == 1 && Y % 4 == 0
			             && (Y % 100 != 0 || Y % 400 == 0) ? 1 : 0));
	}
	return unix;
}

} // namespace ikemen
