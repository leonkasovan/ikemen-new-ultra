#include "bg.hpp"
#include "common.hpp"
#include "../string.hpp"

namespace ikemen {

void BgDef::clear()
{
	bgType.clear();
	layers.clear();
	xscale = yscale = 1;
	xoffset = yoffset = 0;
}

void BgLayer::clear()
{
	anim.reset();
	id = 0; typ = 0; toplayer = false; positionlink = false;
	startx = starty = 0; startvx = startvy = 0;
	deltax = deltay = 1; actionno = 0;
	rasterxtspeed = rasterxbspeed = 1;
	yscalestart = 100; yscaledelta = 0;
	twidth = bwidth = 0;
	velocityx = velocityy = 0;
	enabled = true;
}

void BgLayer::read(Section& sc, BgLayer* link)
{
	std::wstring data;
	float fval;
	int ival;

	typ = 4; // error default
	if (!(data = sc.get(L"type")).empty()) {
		auto d = ikemen::toLower(data);
		if (d.compare(0, 1, L"n") == 0) typ = 0;
		else if (d.compare(0, 1, L"a") == 0) typ = 1;
		else if (d.compare(0, 1, L"p") == 0) typ = 2;
		else if (d.compare(0, 1, L"d") == 0) typ = 3;
	}
	if (typ == 4) return;

	if (!(data = sc.get(L"layerno")).empty()) {
		try { ival = std::stoi(data); toplayer = (ival == 1); if (ival < 0 || ival > 1) typ = 3; } catch (...) {}
	}

	if ((typ == 0 || typ == 2) && !(data = sc.get(L"spriteno")).empty()) {
		anim.frames.resize(1);
		auto parts = ikemen::split(L",", data);
		if (parts.size() >= 2) {
			try { anim.frames[0].group = static_cast<int16_t>(std::stoi(parts[0])); } catch (...) {}
			try { anim.frames[0].number = static_cast<int16_t>(std::stoi(parts[1])); } catch (...) {}
		}
		anim.frames[0].time = -1;
	}

	if (typ == 1 && !(data = sc.get(L"actionno")).empty()) {
		try { actionno = std::stoi(data); } catch (...) {}
		anim.mask = 0;
	}

	if (!(data = sc.get(L"positionlink")).empty()) {
		try { positionlink = std::stoi(data) != 0; } catch (...) {}
		if (positionlink && link) {
			startvx = link->startvx;
			startvy = link->startvy;
			deltax = link->deltax;
			deltay = link->deltay;
		}
	}

	if (!(data = sc.get(L"start")).empty()) {
		auto p = ikemen::split(L",", data);
		if (p.size() >= 2) { try { startx = std::stof(p[0]); starty = std::stof(p[1]); } catch (...) {} }
	}
	if (!(data = sc.get(L"delta")).empty()) {
		auto p = ikemen::split(L",", data);
		if (p.size() >= 2) { try { deltax = std::stof(p[0]); deltay = std::stof(p[1]); } catch (...) {} }
	}

	if (typ != 1 && !(data = sc.get(L"mask")).empty()) {
		try { anim.mask = std::stoi(data) != 0 ? 0 : -1; } catch (...) {}
	}

	if (typ != 1 && !(data = sc.get(L"trans")).empty()) {
		auto d = ikemen::toLower(data);
		if (d == L"add")           { anim.mask = 0; anim.salpha = 255; anim.dalpha = 255; }
		else if (d == L"add1")      { anim.mask = 0; anim.salpha = 255; anim.dalpha = 0; /*!255*/ }
		else if (d == L"addalpha")  {
			anim.mask = 0;
			if (!(data = sc.get(L"alpha")).empty()) {
				auto p = ikemen::split(L",", data);
				if (p.size() >= 2) {
					try { anim.salpha = static_cast<int16_t>(std::stoi(p[0])); } catch (...) {}
					try { anim.dalpha = static_cast<int16_t>(std::stoi(p[1])); } catch (...) {}
				}
			}
		}
		else if (d == L"sub")       { anim.mask = 0; anim.salpha = 1; anim.dalpha = 255; }
		else if (d == L"none")      { anim.salpha = -1; anim.dalpha = 0; }
	}

	if (!(data = sc.get(L"tile")).empty()) {
		auto p = ikemen::split(L",", data);
		// tiling stored via anim.tile
	}

	if (!(data = sc.get(L"velocity")).empty()) {
		auto p = ikemen::split(L",", data);
		if (p.size() >= 2) { try { velocityx = std::stof(p[0]); velocityy = std::stof(p[1]); } catch (...) {} }
	}

	if (typ == 2 && !(data = sc.get(L"xscale")).empty()) {
		auto p = ikemen::split(L",", data);
		if (p.size() >= 2) { try { rasterxtspeed = std::stof(p[0]); rasterxbspeed = std::stof(p[1]); } catch (...) {} }
	}
	if (typ == 2 && !(data = sc.get(L"yscalestart")).empty()) {
		try { yscalestart = std::stof(data); } catch (...) {}
	}
	if (typ == 2 && !(data = sc.get(L"yscaledelta")).empty()) {
		try { yscaledelta = std::stof(data); } catch (...) {}
	}
}

void Background::clear()
{
	def.clear();
	layers.clear();
	m_al.clear();
	m_top = nullptr;
}

void Background::add(BGCtrl& c) { m_al.push_back(c); }

void Background::step()
{
	if (m_top && m_top->waittime <= 0) {
		for (auto& c : m_top->bgcList) add(c);
		m_top = m_top->nextnode;
	}
	if (m_top) m_top->waittime--;
}

} // namespace ikemen
