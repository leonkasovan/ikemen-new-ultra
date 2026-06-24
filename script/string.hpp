#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace ikemen {

// ── Number → string ──────────────────────────────────────────────────────

std::wstring uToSo(uint64_t u);
std::wstring uToSx(uint64_t u);
std::wstring uToSX(uint64_t u);

// ── String → number ──────────────────────────────────────────────────────

template<typename T>
bool sToNumber(T& d, const std::wstring& s);

template<typename T>
T sToN(const std::wstring& s);

template<typename T>
std::vector<T> svToAry(const std::wstring& delim, const std::wstring& v);

// ── Equality / case ──────────────────────────────────────────────────────

bool            equ(const std::wstring& a, const std::wstring& b);
wchar_t         toLowerChar(wchar_t c);
std::wstring    toLower(const std::wstring& s);

// ── Lines ────────────────────────────────────────────────────────────────

int             nextLine(size_t& i, const std::wstring& s);
std::vector<std::wstring> splitLines(const std::wstring& s);

// ── Trim / split / join / find ───────────────────────────────────────────

std::wstring    trim(const std::wstring& s);
std::vector<std::wstring> split(const std::wstring& delim, const std::wstring& src);
std::wstring    join(const std::wstring& delim, const std::vector<std::wstring>& src);
intptr_t        find(const std::wstring& ptn, const std::wstring& s);

// ── Character class ──────────────────────────────────────────────────────

template<typename T>
bool cMatch(const std::vector<T>& cclass, T item);

intptr_t        cFind(const std::wstring& cclass, const std::wstring& s);

// ── Array utilities ──────────────────────────────────────────────────────

template<typename T>
void copy(std::vector<T>& dst, const std::vector<T>& src);

template<typename T>
std::vector<T> clone(const std::vector<T>& src);

template<typename T>
std::vector<T> each(const std::function<void(T&)>& fn, std::vector<T> ary);

// ── Binary ↔ hex string ──────────────────────────────────────────────────

template<typename T>
std::wstring toHex(const std::vector<T>& src);

template<typename T>
std::vector<uint8_t> toUbyte(const std::vector<T>& src);

// ── UTF-8 ↔ UTF-16 ──────────────────────────────────────────────────────

std::vector<uint8_t> sToU8(const std::wstring& s);
std::wstring         u8ToS(const std::vector<uint8_t>& utf8);

// ── Percent encoding ─────────────────────────────────────────────────────

std::wstring percentEnc(const std::wstring& s);
std::wstring percentDec(const std::wstring& s);

// ── Formatter class (printf-style) ───────────────────────────────────────

class Format {
public:
	wchar_t set(const std::wstring& fmt);
	bool    isError() const;

	wchar_t d(int64_t  v);
	wchar_t u(uint64_t v);
	wchar_t f(double   v);
	wchar_t c(wchar_t  v);
	wchar_t s(const std::wstring& v);

	std::wstring out;

private:
	wchar_t setNext();
	wchar_t setError();
	void    putSpace(int n);
	void    putStr(const std::wstring& s);

	std::wstring m_fmt;
	wchar_t      m_next = L'\0';
	int16_t      m_acc  = -1;
	int32_t      m_width = 0;
	int8_t       m_sign = 0;
	bool         m_zero = false;
	bool         m_left = false;
	bool         m_sharp = false;
};

} // namespace ikemen

// ── Template implementations ─────────────────────────────────────────────

#include <algorithm>
#include <cmath>
#include <sstream>

namespace ikemen {

// sToNumber
template<typename T>
bool sToNumber(T& d, const std::wstring& s)
{
	if (s.empty()) return false;
	try {
		if constexpr (std::is_floating_point_v<T>) {
			d = static_cast<T>(std::stold(s));
		} else if constexpr (std::is_signed_v<T>) {
			d = static_cast<T>(std::stoll(s));
		} else {
			d = static_cast<T>(std::stoull(s));
		}
		return true;
	} catch (...) {
		return false;
	}
}

template<typename T>
T sToN(const std::wstring& s)
{
	T d{};
	sToNumber(d, s);
	return d;
}

template<typename T>
std::vector<T> svToAry(const std::wstring& delim, const std::wstring& v)
{
	auto parts = split(delim, v);
	std::vector<T> ary(parts.size());
	for (size_t i = 0; i < parts.size(); i++) {
		ary[i] = sToN<T>(parts[i]);
	}
	return ary;
}

template<typename T>
bool cMatch(const std::vector<T>& cclass, T item)
{
	for (const auto& c : cclass) {
		if (c == item) return true;
	}
	return false;
}

template<typename T>
void copy(std::vector<T>& dst, const std::vector<T>& src)
{
	size_t n = std::min(dst.size(), src.size());
	for (size_t i = 0; i < n; i++) {
		dst[i] = src[i];
	}
}

template<typename T>
std::vector<T> clone(const std::vector<T>& src)
{
	return src;
}

template<typename T>
std::vector<T> each(const std::function<void(T&)>& fn, std::vector<T> ary)
{
	for (auto& x : ary) fn(x);
	return ary;
}

template<typename T>
std::wstring toHex(const std::vector<T>& src)
{
	std::wstring s;
	for (size_t i = 0; i < src.size(); i++) {
		auto val = static_cast<uint64_t>(src[i]);
		int shift = static_cast<int>(sizeof(T) * 8 - 4);
		while (shift >= 0) {
			uint64_t nib = (val >> shift) & 0xF;
			s += nib < 10 ? static_cast<wchar_t>(L'0' + nib)
			              : static_cast<wchar_t>(L'a' + nib - 10);
			shift -= 4;
		}
	}
	return s;
}

template<typename T>
std::vector<uint8_t> toUbyte(const std::vector<T>& src)
{
	std::vector<uint8_t> ub;
	for (size_t i = 0; i < src.size(); i++) {
		auto val = static_cast<uint64_t>(src[i]);
		for (int j = 0; j < static_cast<int>(sizeof(T) * 8); j += 8) {
			ub.push_back(static_cast<uint8_t>((val >> j) & 0xFF));
		}
	}
	return ub;
}

} // namespace ikemen
