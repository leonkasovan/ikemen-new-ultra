#include "string.hpp"
#include "math.hpp"

#include <algorithm>
#include <sstream>

namespace ikemen {
namespace {

const std::wstring BLANK = L" \t\r\n";

wchar_t hexChar(uint32_t u, bool upper)
{
	uint32_t h = u & 0xF;
	if (h < 10) return static_cast<wchar_t>(L'0' + h);
	return static_cast<wchar_t>((upper ? L'A' : L'a') + h - 10);
}

} // namespace

// ── Number → string ──────────────────────────────────────────────────────

std::wstring uToSo(uint64_t u)
{
	int shift = 63;
	while (shift != 0 && (u >> shift) == 0) shift -= 3;
	std::wstring buf;
	while (true) {
		buf += static_cast<wchar_t>(L'0' + ((u >> shift) & 0x7));
		shift -= 3;
		if (shift < 0) break;
	}
	return buf;
}

static std::wstring uToSxXImpl(uint64_t u, bool upper)
{
	int shift = 60;
	while (shift != 0 && (u >> shift) == 0) shift -= 4;
	std::wstring buf;
	while (true) {
		buf += hexChar(static_cast<uint32_t>(u >> shift), upper);
		shift -= 4;
		if (shift < 0) break;
	}
	return buf;
}

std::wstring uToSx(uint64_t u) { return uToSxXImpl(u, false); }
std::wstring uToSX(uint64_t u) { return uToSxXImpl(u, true); }

// ── Equality / case ──────────────────────────────────────────────────────

bool equ(const std::wstring& a, const std::wstring& b)
{
	return a == b;
}

wchar_t toLowerChar(wchar_t c)
{
	return (L'A' <= c && c <= L'Z') ? c + (L'a' - L'A') : c;
}

std::wstring toLower(const std::wstring& s)
{
	std::wstring r(s.size(), L'\0');
	for (size_t i = 0; i < s.size(); i++)
		r[i] = toLowerChar(s[i]);
	return r;
}

// ── Lines ────────────────────────────────────────────────────────────────

int nextLine(size_t& i, const std::wstring& s)
{
	while (i < s.size()) {
		if (s[i] == L'\n') return 1;
		if (s[i] == L'\r') {
			if (i + 1 < s.size() && s[i + 1] == L'\n') return 2;
			return 1;
		}
		i++;
	}
	return 0;
}

std::vector<std::wstring> splitLines(const std::wstring& s)
{
	std::vector<std::wstring> out;
	for (size_t i = 0; i < s.size(); ) {
		size_t start = i;
		int r = nextLine(i, s);
		if (r > 0 && i < s.size()) {
			out.push_back(s.substr(start, i - start));
			i += static_cast<size_t>(r);
		} else {
			out.push_back(s.substr(start));
			break;
		}
	}
	return out;
}

// ── Trim / split / join / find ───────────────────────────────────────────

std::wstring trim(const std::wstring& s)
{
	size_t b = 0;
	while (b < s.size() && BLANK.find(s[b]) != std::wstring::npos) b++;
	size_t e = s.size();
	while (e > 0 && BLANK.find(s[e - 1]) != std::wstring::npos) e--;
	if (b >= e) return L"";
	return s.substr(b, e - b);
}

std::vector<std::wstring> split(const std::wstring& delim, const std::wstring& src)
{
	std::vector<std::wstring> out;
	size_t i = 0;
	while (i < src.size()) {
		auto pos = src.find(delim, i);
		if (pos == std::wstring::npos) {
			out.push_back(src.substr(i));
			break;
		}
		out.push_back(src.substr(i, pos - i));
		i = pos + delim.size();
		if (delim.empty()) {
			if (i < src.size()) i++;
			else break;
		}
	}
	return out;
}

std::wstring join(const std::wstring& delim, const std::vector<std::wstring>& parts)
{
	if (parts.empty()) return L"";
	std::wstring r = parts[0];
	for (size_t i = 1; i < parts.size(); i++) {
		r += delim;
		r += parts[i];
	}
	return r;
}

intptr_t find(const std::wstring& ptn, const std::wstring& s)
{
	auto pos = s.find(ptn);
	return pos == std::wstring::npos ? -1 : static_cast<intptr_t>(pos);
}

intptr_t cFind(const std::wstring& cclass, const std::wstring& s)
{
	auto pos = s.find_first_of(cclass);
	return pos == std::wstring::npos ? -1 : static_cast<intptr_t>(pos);
}

// ── UTF-8 ↔ UTF-16 ──────────────────────────────────────────────────────

std::vector<uint8_t> sToU8(const std::wstring& s)
{
	std::vector<uint8_t> utf8;
	for (size_t i = 0; i < s.size(); i++) {
		uint32_t c = static_cast<uint32_t>(s[i]);
		if ((c >> 10) == 0x36 && i + 1 < s.size()
		 && (static_cast<uint32_t>(s[i + 1]) >> 10) == 0x37) {
			c = ((c & 0x3FF) << 10 | (static_cast<uint32_t>(s[++i]) & 0x3FF)) + 0x10000;
		}
		if (c < 0x80) {
			utf8.push_back(static_cast<uint8_t>(c));
		} else if (c < 0x800) {
			utf8.push_back(static_cast<uint8_t>((c >> 6) | 0xC0));
			utf8.push_back(static_cast<uint8_t>((c & 0x3F) | 0x80));
		} else if (c < 0x10000) {
			utf8.push_back(static_cast<uint8_t>((c >> 12) | 0xE0));
			utf8.push_back(static_cast<uint8_t>(((c >> 6) & 0x3F) | 0x80));
			utf8.push_back(static_cast<uint8_t>((c & 0x3F) | 0x80));
		} else {
			utf8.push_back(static_cast<uint8_t>((c >> 18) | 0xF0));
			utf8.push_back(static_cast<uint8_t>(((c >> 12) & 0x3F) | 0x80));
			utf8.push_back(static_cast<uint8_t>(((c >> 6) & 0x3F) | 0x80));
			utf8.push_back(static_cast<uint8_t>((c & 0x3F) | 0x80));
		}
	}
	return utf8;
}

std::wstring u8ToS(const std::vector<uint8_t>& utf8)
{
	std::wstring s;
	for (size_t i = 0; i < utf8.size(); i++) {
		uint32_t c = utf8[i];
		if (c < 0xC0) {
			// 1-byte sequence
		} else if (c < 0xE0) {
			c &= 0x1F;
			c = (c << 6) | (utf8[++i] & 0x3F);
		} else if (c < 0xF0) {
			c &= 0x0F;
			c = (c << 6) | (utf8[++i] & 0x3F);
			c = (c << 6) | (utf8[++i] & 0x3F);
		} else if (c < 0xF8) {
			c &= 0x07;
			c = (c << 6) | (utf8[++i] & 0x3F);
			c = (c << 6) | (utf8[++i] & 0x3F);
			c = (c << 6) | (utf8[++i] & 0x3F);
		}
		if (c < 0x10000) {
			s += static_cast<wchar_t>(c);
		} else {
			c -= 0x10000;
			s += static_cast<wchar_t>(((c >> 10) & 0x3FF) | 0xD800);
			s += static_cast<wchar_t>((c & 0x3FF) | 0xDC00);
		}
	}
	return s;
}

// ── Percent encoding ─────────────────────────────────────────────────────

std::wstring percentEnc(const std::wstring& s)
{
	auto utf8 = sToU8(s);
	std::wstring out;
	for (size_t i = 0; i < utf8.size(); i++) {
		wchar_t c = static_cast<wchar_t>(utf8[i]);
		if ((L'A' <= c && c <= L'Z') || (L'a' <= c && c <= L'z')
		 || (L'0' <= c && c <= L'9') || c == L'-' || c == L'.'
		 || c == L'_' || c == L'~') {
			out += c;
		} else {
			wchar_t hi = static_cast<wchar_t>(utf8[i] >> 4);
			wchar_t lo = static_cast<wchar_t>(utf8[i] & 0xF);
			out += L'%';
			out += hi < 10 ? L'0' + hi : L'A' + hi - 10;
			out += lo < 10 ? L'0' + lo : L'A' + lo - 10;
		}
	}
	return out;
}

std::wstring percentDec(const std::wstring& s)
{
	std::wstring out;
	std::vector<uint8_t> utf8;
	for (size_t i = 0; i < s.size(); ) {
		if (s[i] == L'%' && i + 2 < s.size()) {
			i++;
			uint8_t ub = 0;
			auto hexVal = [](wchar_t c) -> uint8_t {
				if (L'0' <= c && c <= L'9') return static_cast<uint8_t>(c - L'0');
				if (L'A' <= c && c <= L'F') return static_cast<uint8_t>(c - L'A' + 10);
				if (L'a' <= c && c <= L'f') return static_cast<uint8_t>(c - L'a' + 10);
				return 0;
			};
			ub = (hexVal(s[i]) << 4) | hexVal(s[i + 1]);
			utf8.push_back(ub);
			i++;
		} else {
			if (!utf8.empty()) {
				out += u8ToS(utf8);
				utf8.clear();
			}
			out += s[i++];
		}
	}
	if (!utf8.empty()) out += u8ToS(utf8);
	return out;
}

// ── Formatter class ──────────────────────────────────────────────────────

wchar_t Format::set(const std::wstring& fmt)
{
	m_fmt = fmt;
	out.clear();
	m_next = L'\0';
	return setNext();
}

bool Format::isError() const
{
	return m_next == L'\x7F';
}

wchar_t Format::setError()
{
	return m_next = L'\x7F';
}

void Format::putSpace(int n)
{
	for (int i = 0; i < n; i++) out += L' ';
}

void Format::putStr(const std::wstring& s)
{
	if (!m_left && static_cast<int>(s.size()) < m_width)
		putSpace(m_width - static_cast<int>(s.size()));
	out += s;
	if (m_left && static_cast<int>(s.size()) < m_width)
		putSpace(m_width - static_cast<int>(s.size()));
}

wchar_t Format::setNext()
{
	if (isError()) return m_next;

	auto fmtFind = [](const std::wstring& haystack, const std::wstring& needle) -> intptr_t {
		auto pos = haystack.find(needle);
		return pos == std::wstring::npos ? -1 : static_cast<intptr_t>(pos);
	};

	while (true) {
		intptr_t peridx = fmtFind(m_fmt, L"%");
		if (peridx >= 0)
			out += m_fmt.substr(0, static_cast<size_t>(peridx));
		else
			out += m_fmt;
		if (peridx < 0) {
			m_fmt.clear();
			return m_next = L'\0';
		}
		m_fmt = m_fmt.substr(static_cast<size_t>(peridx) + 1);
		if (m_fmt.empty()) return setError();

		m_sign = 0;
		m_zero = m_left = m_sharp = false;
		m_width = 0;
		m_acc = -1;

		size_t i = 0;
		while (i < m_fmt.size()) {
			switch (m_fmt[i]) {
			case L'0': m_zero = true; break;
			case L'-': m_left = true; break;
			case L'+': m_sign = 1; break;
			case L' ':
				if (m_sign == 0) m_sign = -1; break;
			case L'#': m_sharp = true; break;
			default: goto flags_done;
			}
			i++;
		}
		flags_done:

		while (i < m_fmt.size() && L'0' <= m_fmt[i] && m_fmt[i] <= L'9') {
			m_width = m_width * 10 + static_cast<int>(m_fmt[i] - L'0');
			i++;
		}
		if (i < m_fmt.size() && m_fmt[i] == L'.') {
			i++;
			m_acc = 0;
			while (i < m_fmt.size() && L'0' <= m_fmt[i] && m_fmt[i] <= L'9') {
				m_acc = m_acc * 10 + static_cast<int>(m_fmt[i] - L'0');
				i++;
			}
		}
		if (i < m_fmt.size() && (m_fmt[i] == L'h' || m_fmt[i] == L'l' || m_fmt[i] == L'L')) i++;
		if (i < m_fmt.size() && m_fmt[i] == L'%') {
			m_fmt = m_fmt.substr(i + 1);
			continue;
		}
		if (i >= m_fmt.size()) return setError();
		m_next = m_fmt[i];
		if (m_next != L'd' && m_next != L'i' && m_next != L'u' && m_next != L'o'
		 && m_next != L'x' && m_next != L'X' && m_next != L'c' && m_next != L's'
		 && m_next != L'f' && m_next != L'F' && m_next != L'e' && m_next != L'E'
		 && m_next != L'g' && m_next != L'G') {
			return setError();
		}
		m_fmt = m_fmt.substr(i + 1);
		return m_next;
	}
}

static bool charIn(wchar_t c, const wchar_t* set)
{
	while (*set) { if (c == *set) return true; set++; }
	return false;
}

wchar_t Format::d(int64_t v)
{
	if (!charIn(m_next, L"di")) {
		if (charIn(m_next, L"uoxX")) return u(static_cast<uint64_t>(v));
		if (charIn(m_next, L"fFeEgG")) return f(static_cast<double>(v));
		if (m_next == L'c') return c(static_cast<wchar_t>(v));
		if (m_next == L's') {
			auto ws = std::to_wstring(v);
			return s(ws);
		}
		return setError();
	}

	std::wostringstream ss;
	if (v >= 0 && m_sign != 0)
		ss << (m_sign > 0 ? L'+' : L' ');

	std::wstring numStr;
	if (v == INT64_MIN) {
		numStr = L"9223372036854775808";
		ss << L'-';
	} else {
		bool neg = v < 0;
		if (neg) v = -v;
		numStr = std::to_wstring(v);
		if (neg) ss << L'-';
	}
	ss << numStr;

	std::wstring buf = ss.str();

	if (m_zero && m_acc < 0) {
		int targetW = m_width - static_cast<int>(buf.size()) + static_cast<int>(numStr.size());
		if (targetW > static_cast<int>(numStr.size())) {
			std::wstring pad(targetW - static_cast<int>(numStr.size()), L'0');
			auto pos = buf.rfind(numStr[0]);
			if (pos != std::wstring::npos) buf.insert(pos, pad);
		}
	}
	putStr(buf);
	return setNext();
}

wchar_t Format::u(uint64_t v)
{
	if (!charIn(m_next, L"uoxX")) {
		if (charIn(m_next, L"di")) return d(static_cast<int64_t>(v));
		if (charIn(m_next, L"fFeEgG")) return f(static_cast<double>(v));
		if (m_next == L'c') return c(static_cast<wchar_t>(v));
		if (m_next == L's') {
			auto ws = std::to_wstring(v);
			return s(ws);
		}
		return setError();
	}

	std::wstring numStr;
	if (m_next == L'u') numStr = std::to_wstring(v);
	else if (m_next == L'o') numStr = uToSo(v);
	else if (m_next == L'x') numStr = uToSx(v);
	else numStr = uToSX(v);

	std::wstring buf;
	if (m_sign != 0) buf += (m_sign > 0 ? L'+' : L' ');

	if (m_next == L'o' && m_sharp && (m_acc < 0 || static_cast<int>(numStr.size()) < m_acc))
		buf += L'0';
	else if (m_next == L'x' && m_sharp) buf += L"0x";
	else if (m_next == L'X' && m_sharp) buf += L"0X";

	if (static_cast<int>(numStr.size()) < m_acc) {
		buf += std::wstring(m_acc - static_cast<int>(numStr.size()), L'0');
	}
	buf += numStr;
	putStr(buf);
	return setNext();
}

wchar_t Format::f(double v)
{
	if (!charIn(m_next, L"fFeEgG")) {
		if (charIn(m_next, L"di")) return d(static_cast<int64_t>(v));
		if (charIn(m_next, L"uoxX")) return u(static_cast<uint64_t>(v));
		if (m_next == L'c') return c(static_cast<wchar_t>(v));
		return setError();
	}

	std::wostringstream ss;

	if (ikemen::isnan(v)) {
		ss << (m_next >= L'a' && m_next <= L'z' ? L"nan" : L"NAN");
		putStr(ss.str());
		return setNext();
	}
	if (!ikemen::isfinite(v)) {
		ss << (m_next >= L'a' && m_next <= L'z' ? L"inf" : L"INF");
		putStr(ss.str());
		return setNext();
	}

	// Use standard formatting for simplicity
	if (m_acc >= 0) { ss.precision(m_acc); ss << std::fixed; }
	ss << v;
	putStr(ss.str());
	return setNext();
}

wchar_t Format::c(wchar_t v)
{
	if (m_next != L'c' && m_next != L's') {
		if (charIn(m_next, L"di")) return d(static_cast<int64_t>(v));
		if (charIn(m_next, L"uoxX")) return u(static_cast<uint64_t>(v));
		if (charIn(m_next, L"fFeEgG")) return f(static_cast<double>(v));
		return setError();
	}
	putStr(std::wstring(1, v));
	return setNext();
}

wchar_t Format::s(const std::wstring& v)
{
	if (m_next != L's') return setError();
	putStr(v);
	return setNext();
}

} // namespace ikemen
