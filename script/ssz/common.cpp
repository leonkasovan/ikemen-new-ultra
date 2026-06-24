#include "common.hpp"

#include "../math.hpp"
#include "../string.hpp"
#include "../file.hpp"
#include "../save/config.hpp"
#include "../alpha/sdlplugin.hpp"

#include <algorithm>
#include <cstdio>
#include <cwctype>

namespace ikemen {

Camera cam;

// ── Layout ─────────────────────────────────────────────────────────────

Layout::Layout()
	: offset{0, 0}
	, displaytime(-2)
	, facing(1)
	, vfacing(1)
	, layerno(0)
	, scale{1, 1}
{
}

void Layout::read(const std::wstring& img, Section& sc)
{
	offset.x = sToN<float>(sc.get(L"offset.x"));
	offset.y = sToN<float>(sc.get(L"offset.y"));

	auto dt = sc.get(L"displaytime");
	if (!dt.empty()) displaytime = sToN<int>(dt);

	auto f = sc.get(L"facing");
	if (!f.empty()) facing = sToN<int>(f) != 0 ? 1 : -1;

	auto vf = sc.get(L"vfacing");
	if (!vf.empty()) vfacing = sToN<int>(vf) != 0 ? 1 : -1;

	auto ln = sc.get(L"layerno");
	if (!ln.empty()) layerno = static_cast<int16_t>(sToN<int>(ln));

	scale.x = sToN<float>(sc.get(L"scale.x"));
	scale.y = sToN<float>(sc.get(L"scale.y"));
}

void Layout::setup()
{
	if (offset.x == static_cast<float>(IERR)) offset.x = 0;
	if (offset.y == static_cast<float>(IERR)) offset.y = 0;
	if (scale.x == static_cast<float>(IERR) || scale.x == 0) scale.x = 1;
	if (scale.y == static_cast<float>(IERR) || scale.y == 0) scale.y = 1;
}

// ── Section ────────────────────────────────────────────────────────────

void Section::parse(const std::vector<std::wstring>& lines, size_t& i)
{
	while (i < lines.size()) {
		auto line = trim(lines[i]);
		if (line.empty() || line[0] == L'[') break;

		auto eq = line.find(L'=');
		if (eq == std::wstring::npos) { i++; continue; }

		auto key = toLower(trim(line.substr(0, eq)));
		auto val = trim(line.substr(eq + 1));

		// Handle quoted values
		if (!val.empty() && val.front() == L'"') {
			val = val.substr(1);
			while (val.find(L'"') == std::wstring::npos && i + 1 < lines.size()) {
				i++;
				val += L"\n";
				val += trim(lines[i]);
			}
			auto dq = val.find(L'"');
			if (dq != std::wstring::npos) val = val.substr(0, dq);
		}

		params.set(key, val);
		i++;
	}
}

std::wstring Section::get(const std::wstring& name)
{
	auto val = params.get(name);
	if (!val.empty()) return val;

	// Try lower-case fallback
	auto lower = toLower(name);
	if (lower != name) return params.get(lower);
	return L"";
}

std::wstring Section::getText(const std::wstring& name, std::wstring& text)
{
	text = get(name);
	return text;
}

std::wstring Section::benri(const std::wstring& head, const std::wstring& name)
{
	auto full = head + L"." + name;
	auto val = params.get(full);
	if (!val.empty()) return val;
	return params.get(name);
}

// ── Camera::Stage ──────────────────────────────────────────────────────

void Camera::Stage::init()
{
	x = 0; y = 0;
	startx = 0;
	boundhigh = boundlow = boundleft = boundright = 0;
	zoomin = zoomout = 1;
	tension = tensionhigh = tensionlow = 0;
	startzoom = 0;
	overdrawhigh = overdrawlow = overdrawleft = overdrawright = 0;
	cutoffhigh = cutofflow = cutoffleft = cutoffright = 0;
	localw = 320; localh = 240;
	localscl = 1; drawOffsetY = 0;
	zoffset = 0; ztopscale = 1;
	verticalfollow = 0.2f; floortension = 0;
}

void Camera::Stage::update()
{
	if (zoomtime > 0) {
		zoomtime -= 1.0f / static_cast<float>(config::GameSpeed);
		float t = zoomspeed > 0 ? (1.0f - zoomtime) : zoomtime;
		zoomin = startzoom + t * (zoomin - startzoom);
		zoomout = startzoom + t * (zoomout - startzoom);
		if (zoomtime <= 0) zoomtime = 0;
	}
}

// ── PalFX ──────────────────────────────────────────────────────────────

void PalFX::clear()
{
	time = 0;
	mul_r = mul_g = mul_b = 255;
	add_r = add_g = add_b = 0;
	sin_x_time = sin_x_amplitude = sin_x_frequency = sin_x_phase = 0;
	sin_y_time = sin_y_amplitude = sin_y_frequency = sin_y_phase = 0;
	sin_z_time = sin_z_amplitude = sin_z_frequency = sin_z_phase = 0;
}

void PalFX::step()
{
	if (time <= 0) return;
	time--;
}

void PalFX::getFxPal(float& rm, float& gm, float& bm,
                     float& ra, float& ga, float& ba) const
{
	float t = static_cast<float>(time);
	float sx = sin_x_amplitude * ikemen::sin(sin_x_frequency * t + sin_x_phase);
	float sy = sin_y_amplitude * ikemen::sin(sin_y_frequency * t + sin_y_phase);
	float sz = sin_z_amplitude * ikemen::sin(sin_z_frequency * t + sin_z_phase);

	rm = mul_r + add_r + sx;
	gm = mul_g + add_g + sy;
	bm = mul_b + add_b + sz;

	rm = std::max(0.0f, std::min(510.0f, rm));
	gm = std::max(0.0f, std::min(510.0f, gm));
	bm = std::max(0.0f, std::min(510.0f, bm));
	ra = ga = ba = 0;
}

// ── Number parsing ─────────────────────────────────────────────────────

double atof(const std::wstring& s)
{
	if (s.empty()) return 0.0;
	wchar_t* end = nullptr;
	double v = std::wcstod(s.c_str(), &end);
	if (end == s.c_str()) return 0.0;
	return v;
}

int atoi(const std::wstring& s)
{
	if (s.empty()) return 0;
	wchar_t* end = nullptr;
	long v = std::wcstol(s.c_str(), &end, 0);
	if (end == s.c_str()) return 0;
	return static_cast<int>(v);
}

// ── File I/O ───────────────────────────────────────────────────────────

std::wstring loadText(const std::wstring& fn, bool& unicode)
{
	auto data = readAll<uint8_t>(fn);
	if (data.empty()) return L"";

	unicode = false;

	// BOM detection
	if (data.size() >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
		// UTF-8 with BOM
		data.erase(data.begin(), data.begin() + 3);
		unicode = true;
		return u8ToS(data);
	}
	if (data.size() >= 2 && data[0] == 0xFF && data[1] == 0xFE) {
		// UTF-16 LE
		unicode = true;
		auto* wptr = reinterpret_cast<wchar_t*>(data.data() + 2);
		size_t wlen = (data.size() - 2) / sizeof(wchar_t);
		return std::wstring(wptr, wlen);
	}
	if (data.size() >= 2 && data[0] == 0xFE && data[1] == 0xFF) {
		// UTF-16 BE — convert to LE
		unicode = true;
		std::vector<uint8_t> swapped(data.begin() + 2, data.end());
		for (size_t i = 0; i + 1 < swapped.size(); i += 2) {
			std::swap(swapped[i], swapped[i + 1]);
		}
		auto* wptr = reinterpret_cast<wchar_t*>(swapped.data());
		size_t wlen = swapped.size() / sizeof(wchar_t);
		return std::wstring(wptr, wlen);
	}

	// Check for UTF-8 without BOM by scanning first bytes
	bool likelyUtf8 = false;
	size_t scanEnd = std::min(data.size(), size_t(512));
	for (size_t i = 0; i < scanEnd; i++) {
		if (data[i] >= 0xC0) { likelyUtf8 = true; break; }
	}
	if (likelyUtf8) {
		unicode = true;
		return u8ToS(data);
	}

	// ASCII / ANSI
	unicode = false;
	std::wstring out;
	for (auto c : data) out += static_cast<wchar_t>(c);
	return out;
}

std::wstring readFileName(const std::wstring& f, bool /*unicode*/)
{
	size_t pos = 0;
	while (pos < f.size() && std::iswspace(f[pos])) pos++;
	if (pos >= f.size()) return L"";

	if (f[pos] == L'"') {
		pos++;
		size_t end = f.find(L'"', pos);
		if (end == std::wstring::npos) return f.substr(pos);
		return f.substr(pos, end - pos);
	}

	// Unquoted — read until whitespace
	size_t start = pos;
	while (pos < f.size() && !std::iswspace(f[pos])) pos++;
	return f.substr(start, pos - start);
}

std::wstring loadFile(const std::wstring& fn, const std::wstring& ft)
{
	bool unicode;
	auto text = loadText(fn, unicode);
	if (text.empty()) return L"";

	// Handle different file type conversions if needed
	auto ftype = toLower(ft);
	if (ftype == L"utf8raw") {
		auto raw = readAll<uint8_t>(fn);
		if (raw.size() >= 3 && raw[0] == 0xEF && raw[1] == 0xBB && raw[2] == 0xBF)
			raw.erase(raw.begin(), raw.begin() + 3);
		return u8ToS(raw);
	}

	return text;
}

// ── Section / def helpers ──────────────────────────────────────────────

std::wstring sectionName(std::wstring& sec)
{
	auto open = sec.find(L'[');
	auto close = sec.find(L']');
	if (open == std::wstring::npos || close == std::wstring::npos || close <= open)
		return L"";

	auto name = sec.substr(open + 1, close - open - 1);
	sec.erase(0, close + 1);
	return trim(name);
}

void mugenversion(const std::wstring& path, float& version, int& compat,
                  int& mugenver, std::wstring& mugenname,
                  std::wstring& localcoord, bool& utf8, bool& filev2)
{
	version = 1.0f;
	compat = 2;
	mugenver = 0;
	mugenname.clear();
	localcoord = L"320,240";
	utf8 = false;
	filev2 = false;

	bool unicode;
	auto text = loadText(path, unicode);
	if (text.empty()) return;
	utf8 = unicode;

	auto lines = splitLines(text);
	for (size_t i = 0; i < lines.size(); i++) {
		auto line = trim(lines[i]);
		if (line.empty() || line[0] == L'[') {
			if (!line.empty() && line[0] == L'[') {
				auto sec = toLower(line);
				if (sec.find(L"files") != std::wstring::npos) filev2 = true;
			}
			continue;
		}

		auto eq = line.find(L'=');
		if (eq == std::wstring::npos) continue;

		auto key = toLower(trim(line.substr(0, eq)));
		auto val = trim(line.substr(eq + 1));
		if (val.size() >= 2 && val.front() == L'"' && val.back() == L'"')
			val = val.substr(1, val.size() - 2);

		if (key == L"mugenversion") {
			// Parse date string: "04,14,2001"
			auto parts = split(L",", val);
			if (parts.size() >= 3) {
				int mo = sToN<int>(parts[0]);
				int dy = sToN<int>(parts[1]);
				int yr = sToN<int>(parts[2]);
				if (yr >= 2001) { mugenver = 1; version = 1.0f; }
				if (yr >= 2002) { mugenver = 2; version = 1.1f; }
			}
		} else if (key == L"localcoord") {
			compat = 2;
			localcoord = val;
		} else if (key == L"name" || key == L"displayname") {
			mugenname = val;
		}
	}
}

// ── Debug print ────────────────────────────────────────────────────────

void printVar(const std::wstring& name, int val)
{
	std::fwprintf(stderr, L"%s = %d\n", name.c_str(), val);
}

void printVar(const std::wstring& name, float val)
{
	std::fwprintf(stderr, L"%s = %g\n", name.c_str(), static_cast<double>(val));
}

void printVar(const std::wstring& name, const std::wstring& val)
{
	std::fwprintf(stderr, L"%s = %s\n", name.c_str(), val.c_str());
}

void printArray(const std::wstring& name, const std::vector<int32_t>& arr)
{
	std::fwprintf(stderr, L"%s = [", name.c_str());
	for (size_t i = 0; i < arr.size(); i++) {
		if (i > 0) std::fwprintf(stderr, L", ");
		std::fwprintf(stderr, L"%d", arr[i]);
	}
	std::fwprintf(stderr, L"]\n");
}

// ── Input hashing ──────────────────────────────────────────────────────

int eventKeyHash(const SdlEventHandler::Key& k)
{
	uint32_t sc = static_cast<uint32_t>(k.key);
	uint32_t mod = (k.shift ? 1u : 0u) | (k.ctrl ? 2u : 0u) | (k.alt ? 4u : 0u);
	uint32_t h = (sc << 3) ^ (mod * 0x9E3779B9u) ^ (sc >> 5);
	return static_cast<int>(h);
}

} // namespace ikemen
