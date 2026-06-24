#include "stage.hpp"
#include "common.hpp"

#include "../file.hpp"
#include "../string.hpp"
#include "../math.hpp"

#include <cmath>

namespace ikemen {

// ── EnvShake ─────────────────────────────────────────────────────────────

void EnvShake::clear() { time = 0; freq = 1.04719755f; ampl = -4; phase = NAN; }

void EnvShake::setDefPhase()
{
	if (!ikemen::isnan(phase)) return;
	phase = (freq >= 1.57079633f ? 1.57079633f : 0.0f); // PI/2 check
}

void EnvShake::next()
{
	if (time <= 0) return;
	time--;
	phase += freq;
}

float EnvShake::getOffset()
{
	if (time <= 0) return 0.0f;
	return static_cast<float>(ampl) * 0.5f * ikemen::sin(static_cast<double>(phase));
}

EnvShake g_envShake;

// ── Stage ────────────────────────────────────────────────────────────────

void Stage::parseInfo(Section& sc)
{
	std::wstring data;
	sc.getText(L"name", data);
	if (!data.empty()) name = data; else name = defFile;

	sc.getText(L"displayname", data);
	if (!data.empty()) displayName = data; else displayName = name;

	sc.getText(L"author", data);
	if (!data.empty()) author = data;

	nameLow = ikemen::toLower(name);
	displayNameLow = ikemen::toLower(displayName);
	authorLow = ikemen::toLower(author);
}

void Stage::parseCamera(Section& sc)
{
	std::wstring data;
	int ival;
	float fval;

	auto getInt = [&](const wchar_t* key, int& out) {
		if (!(data = sc.get(key)).empty()) { try { ival = std::stoi(data); out = ival; } catch (...) {} }
	};
	auto getFloat = [&](const wchar_t* key, float& out) {
		if (!(data = sc.get(key)).empty()) { try { fval = std::stof(data); out = fval; } catch (...) {} }
	};

	getInt(L"startx",   cam.stg.startx);
	getInt(L"boundleft",  ival); cam.stg.boundleft  = static_cast<float>(ival);
	getInt(L"boundright", ival); cam.stg.boundright = static_cast<float>(ival);
	getInt(L"boundhigh",  ival); cam.stg.boundhigh  = static_cast<float>(ival);
	getFloat(L"verticalfollow", cam.stg.verticalfollow);
	getInt(L"tension",  ival); cam.stg.tension = static_cast<float>(ival);
	getInt(L"floortension",  cam.stg.floortension);
	getInt(L"overdrawlow",  ival); cam.stg.overdrawlow = static_cast<float>(ival);
}

void Stage::parseScaling(Section& sc)
{
	std::wstring data;
	float fval;
	if (!(data = sc.get(L"topscale")).empty()) {
		try { fval = std::stof(data); cam.stg.ztopscale = fval; } catch (...) {}
	}
}

void Stage::parsePlayerInfo(Section& sc)
{
	std::wstring data;
	int val;
	auto getInt = [&](const wchar_t* key, int& out) {
		if (!(data = sc.get(key)).empty()) { val = std::stoi(data); out = val; }
	};
	getInt(L"p1startx", p1.startx);
	getInt(L"p1starty", p1.starty);
	getInt(L"p1facing", p1.facing);
	getInt(L"p2startx", p2.startx);
	getInt(L"p2starty", p2.starty);
	getInt(L"p2facing", p2.facing);
	getInt(L"p3startx", p3.startx);
	getInt(L"p3starty", p3.starty);
	getInt(L"p3facing", p3.facing);
	getInt(L"p4startx", p4.startx);
	getInt(L"p4starty", p4.starty);
	getInt(L"p4facing", p4.facing);

	float lr;
	getInt(L"leftbound", val); leftbound = static_cast<float>(val);
	getInt(L"rightbound", val); rightbound = static_cast<float>(val);
}

void Stage::parseBound(Section& sc)
{
	std::wstring data; int val;
	auto getInt = [&](const wchar_t* key, int& out) {
		if (!(data = sc.get(key)).empty()) { val = std::stoi(data); out = val; }
	};
	getInt(L"screenleft", screenleft);
	getInt(L"screenright", screenright);
}

void Stage::parseStageInfo(Section& sc)
{
	std::wstring data;
	int ival;

	auto getInt = [&](const wchar_t* key, int& out) {
		if (!(data = sc.get(key)).empty()) { try { ival = std::stoi(data); out = ival; } catch (...) {} }
	};

	if (!(data = sc.get(L"zoffset")).empty()) {
		try { cam.stg.zoffset = std::stoi(data); } catch (...) {}
	}

	getInt(L"hires", ival); hires = ival != 0;
	getInt(L"resetbg", ival); resetBg = ival != 0;

	if (!(data = sc.get(L"localcoord")).empty()) {
		auto parts = ikemen::split(L",", data);
		if (parts.size() >= 2) {
			try { cam.stg.localw = std::stoi(parts[0]); } catch (...) {}
			try { cam.stg.localh = std::stoi(parts[1]); } catch (...) {}
		}
	}

	float fval;
	if (!(data = sc.get(L"xscale")).empty()) {
		try { fval = std::stof(data); xscale = fval; } catch (...) {}
	}
	if (!(data = sc.get(L"yscale")).empty()) {
		try { fval = std::stof(data); yscale = fval; } catch (...) {}
	}
}

void Stage::parseShadow(Section& sc)
{
	std::wstring data; int val;
	if (!(data = sc.get(L"intensity")).empty()) {
		try { val = std::stoi(data); sdw.intensity = ikemen::min(255, ikemen::max(0, val)); } catch (...) {}
	}
	if (!(data = sc.get(L"yscale")).empty()) {
		try { sdw.yscale = std::stof(data); } catch (...) {}
	}
}

void Stage::parseReflection(Section& sc)
{
	std::wstring data; int val;
	if (!(data = sc.get(L"intensity")).empty()) {
		try { val = std::stoi(data); reflection = ikemen::min(255, ikemen::max(0, val)); } catch (...) {}
	}
}

void Stage::parseMusic(Section& sc, bool unicode)
{
	std::wstring data;
	if (!(data = sc.get(L"bgmusic")).empty()) {
		bgmusic = data;
	}
}

void Stage::parseBgDef(Section& sc, bool unicode)
{
	std::wstring data;
	if (!(data = sc.get(L"spr")).empty()) {
		sprFile = data;
	}
	if (!(data = sc.get(L"debugbg")).empty()) {
		try { debugBg = std::stoi(data) != 0; } catch (...) {}
	}
}

void Stage::parseBgCtrlDef(Section& sc)
{
	std::wstring data;
	// BGCtrl defaults — ctrlbg list and looptime
	if (!(data = sc.get(L"looptime")).empty()) {
		try { /* stored in bgcdef */ } catch (...) {}
	}
}

void Stage::parseBgCtrl(Section& sc)
{
	std::wstring data;
	// BGCtrl section — read ctrlid, time, etc.
}

void Stage::clear()
{
	name.clear(); displayName.clear(); author.clear();
	bgmusic.clear(); sprFile.clear();
	bgLayers.clear();
	sdw = Shadow{};
	p1 = p2 = p3 = p4 = PlayerPos{};
	reflection = 0; hires = false; resetBg = true; debugBg = false;
	xscale = yscale = 1;
	cam.stg.init();
}

void Stage::reset()
{
	clear();
	cam.stg.init();
}

void Stage::parseBg(Section& sc, int& linkIdx)
{
	auto& bg = bgLayers;
	int link = linkIdx;
	if (!bg.empty() && !bg.back().positionlink)
		link = static_cast<int>(bg.size()) - 1;
	bg.emplace_back();
	bg.back().read(sc, (link >= 0 && link < static_cast<int>(bg.size())) ? &bg[static_cast<size_t>(link)] : nullptr);
	linkIdx = static_cast<int>(bg.size()) - 1;
}

// ── Main load function ──────────────────────────────────────────────────

std::wstring Stage::load(const std::wstring& def)
{
	clear();
	defFile = def;
	bool unicode = false;

	auto text = ikemen::loadText(def, unicode);
	if (text.empty()) return L"Failed to open " + def;

	auto lines = ikemen::splitLines(text);

	bool infoFlg = true, cameraFlg = true, playerFlg = true;
	bool boundFlg = true, stageFlg = true, shadowFlg = true;
	bool reflFlg = true, musicFlg = true, bgdefFlg = true;
	bool scalingFlg = true;
	int  linkIdx = 0;

	for (size_t i = 0; i < lines.size(); i++) {
		std::wstring sec = lines[i];
		std::wstring secname;
		{
			// Extract section name
			auto t = ikemen::trim(sec);
			if (t.empty() || t[0] != L'[') continue;
			size_t semi = t.find(L';');
			if (semi != std::wstring::npos) t = t.substr(0, semi);
			t = ikemen::trim(t);
		if (t.size() < 3 || t[0] != L'[' || t.back() != L']') continue;
		secname = ikemen::toLower(t.substr(1, t.size() - 2));
		}

		if (secname == L"begin action") continue; // skip animation sections

		if (infoFlg && secname == L"info") {
			i++; Section sc; sc.parse(lines, i); parseInfo(sc); infoFlg = false;
		} else if (cameraFlg && secname == L"camera") {
			i++; Section sc; sc.parse(lines, i); parseCamera(sc); cameraFlg = false;
		} else if (playerFlg && secname == L"playerinfo") {
			i++; Section sc; sc.parse(lines, i); parsePlayerInfo(sc); playerFlg = false;
		} else if (boundFlg && secname == L"bound") {
			i++; Section sc; sc.parse(lines, i); parseBound(sc); boundFlg = false;
		} else if (stageFlg && secname == L"stageinfo") {
			i++; Section sc; sc.parse(lines, i); parseStageInfo(sc); stageFlg = false;
		} else if (shadowFlg && secname == L"shadow") {
			i++; Section sc; sc.parse(lines, i); parseShadow(sc); shadowFlg = false;
		} else if (reflFlg && secname == L"reflection") {
			i++; Section sc; sc.parse(lines, i); parseReflection(sc); reflFlg = false;
		} else if (scalingFlg && secname == L"scaling") {
			i++; Section sc; sc.parse(lines, i); parseScaling(sc); scalingFlg = false;
		} else if (musicFlg && secname == L"music") {
			i++; Section sc; sc.parse(lines, i); parseMusic(sc, unicode); musicFlg = false;
		} else if (bgdefFlg && secname == L"bgdef") {
			i++; Section sc; sc.parse(lines, i); parseBgDef(sc, unicode); bgdefFlg = false;
		} else if (secname.compare(0, 2, L"bg") == 0) {
			i++; Section sc; sc.parse(lines, i); parseBg(sc, linkIdx);
		} else {
			// Unknown section — skip header line only
		}
	}

	// Second pass: bgctrldef / bgctrl
	for (size_t i = 0; i < lines.size(); i++) {
		std::wstring sec = lines[i];
		auto t = ikemen::trim(sec);
		if (t.empty() || t[0] != L'[') continue;
		size_t semi = t.find(L';');
		if (semi != std::wstring::npos) t = t.substr(0, semi);
		t = ikemen::trim(t);
		if (t.size() < 3 || t[0] != L'[' || t.back() != L']') continue;
		auto secname = ikemen::toLower(t.substr(1, t.size() - 2));

		if (secname == L"bgctrldef") {
			i++; Section sc; sc.parse(lines, i); parseBgCtrlDef(sc);
		} else if (secname == L"bgctrl") {
			i++; Section sc; sc.parse(lines, i); parseBgCtrl(sc);
		}
	}

	return L"";
}

std::wstring g_stageBgMusic;

} // namespace ikemen
