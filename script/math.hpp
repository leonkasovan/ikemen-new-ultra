#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace ikemen {

// ── Constants ────────────────────────────────────────────────────────────

const double PI = 3.141592653589793238462643383279502884197;
const double E  = 2.718281828459045235360287471352662497757;

// ── Trigonometric ────────────────────────────────────────────────────────

double sin(double x);
double cos(double x);
double tan(double x);
double asin(double x);
double acos(double x);
double atan(double x);

// ── Logarithmic / exponential ────────────────────────────────────────────

double log(double base, double x);
double ln(double x);
double exp(double x);

// ── Power / rounding / classification ────────────────────────────────────

double sqrt(double x);
double ceil(double x);
double floor(double x);
double round(double x);
bool   isfinite(double x);
bool   isinf(double x);
bool   isnan(double x);

// ── Pseudo-random numbers (Park–Miller LCG) ──────────────────────────────

int   random();
void  srand(int seed);
int   rand(int min, int max);
int   randI(int x, int y);
float randF(float x, float y);

// ── Generic utilities ────────────────────────────────────────────────────

template<typename T> T min(T x, T y) { return x < y ? x : y; }
template<typename T> T max(T x, T y) { return x > y ? x : y; }

template<typename T>
bool inRange(T lo, T hi, T x) { return lo <= x && x <= hi; }

template<typename T> void limMax(T& x, T y) { if (y < x) x = y; }
template<typename T> void limMin(T& x, T y) { if (x < y) x = y; }

template<typename T>
void limRange(T& x, T lo, T hi) { limMin(limMax(x, hi), lo); }

template<typename T>
void swap(T& x, T& y) { T tmp = x; x = y; y = tmp; }

} // namespace ikemen
