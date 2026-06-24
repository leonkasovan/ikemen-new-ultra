#include "math.hpp"

#include <chrono>
#include <cmath>

namespace ikemen {

// ── Trigonometric ────────────────────────────────────────────────────────

double sin(double x)   { return ::std::sin(x); }
double cos(double x)   { return ::std::cos(x); }
double tan(double x)   { return ::std::tan(x); }
double asin(double x)  { return ::std::asin(x); }
double acos(double x)  { return ::std::acos(x); }
double atan(double x)  { return ::std::atan(x); }

// ── Logarithmic / exponential ────────────────────────────────────────────

double log(double base, double x) { return ::std::log(x) / ::std::log(base); }
double ln(double x)               { return ::std::log(x); }
double exp(double x)              { return ::std::exp(x); }

// ── Power / rounding / classification ────────────────────────────────────

double sqrt(double x)    { return ::std::sqrt(x); }
double ceil(double x)    { return ::std::ceil(x); }
double floor(double x)   { return ::std::floor(x); }

double round(double x)
{
	return x < 0.0 ? floor(0.5 - x) : floor(0.5 + x);
}

bool isfinite(double x)  { return ::std::isfinite(x); }
bool isinf(double x)     { return ::std::isinf(x); }
bool isnan(double x)     { return ::std::isnan(x); }

// ── Pseudo-random numbers (Park–Miller LCG) ──────────────────────────────

namespace {

const int32_t RANDMAX = std::numeric_limits<int32_t>::max();

int32_t& randseed()
{
	static int32_t seed = []() -> int32_t {
		auto now = std::chrono::system_clock::now().time_since_epoch();
		int64_t t = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
		return static_cast<int32_t>(t & RANDMAX);
	}();
	return seed;
}

} // namespace

int random()
{
	int32_t& rs = randseed();
	int32_t w = rs / 127773;
	rs = (rs - w * 127773) * 16807 - w * 2836;
	if (rs <= 0) {
		rs += RANDMAX - (rs == 0 ? 1 : 0);
	}
	return rs;
}

void srand(int s)
{
	randseed() = s;
}

int rand(int min, int max)
{
	return min + random() / (RANDMAX / (max - min + 1) + 1);
}

int randI(int x, int y)
{
	if (y < x) {
		if (x - y < 0) return y + static_cast<int64_t>(random()) * (static_cast<int64_t>(x) - y) / RANDMAX;
		return rand(y, x);
	}
	if (y - x < 0) return x + static_cast<int64_t>(random()) * (static_cast<int64_t>(y) - x) / RANDMAX;
	return rand(x, y);
}

float randF(float x, float y)
{
	return x + static_cast<float>(random()) * (y - x) / static_cast<float>(RANDMAX);
}

} // namespace ikemen
