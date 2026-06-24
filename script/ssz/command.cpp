#include "command.hpp"
#include "common.hpp"

#include "../file.hpp"
#include "../string.hpp"
#include "../math.hpp"

namespace ikemen {

// ── InputBuffer ──────────────────────────────────────────────────────────

InputBuffer::InputBuffer() { reset(); }

void InputBuffer::input(bool B, bool D, bool F, bool U, bool a, bool b, bool c,
                         bool x, bool y, bool z, bool q, bool w, bool e, bool s_)
{
	if ((B & !F) != (m_B > 0)) { m_Bb = 0; m_B = static_cast<int8_t>(-m_B); }
	if ((D & !U) != (m_D > 0)) { m_Db = 0; m_D = static_cast<int8_t>(-m_D); }
	if ((F & !B) != (m_F > 0)) { m_Fb = 0; m_F = static_cast<int8_t>(-m_F); }
	if ((U & !D) != (m_U > 0)) { m_Ub = 0; m_U = static_cast<int8_t>(-m_U); }
	m_Bb += m_B; m_Db += m_D; m_Fb += m_F; m_Ub += m_U;

	auto btn = [](bool p, int8_t& st, int& buf) {
		if (p != (st > 0)) { buf = 0; st = static_cast<int8_t>(-st); } buf += st;
	};
	btn(a, m_a, m_ab); btn(b, m_b, m_bb); btn(c, m_c, m_cb);
	btn(x, m_x, m_xb); btn(y, m_y, m_yb); btn(z, m_z, m_zb);
	btn(q, m_q, m_qb); btn(w, m_w, m_wb); btn(e, m_e, m_eb);
	btn(s_, m_s, m_sb);
}

void InputBuffer::inputStat(int stat, int f)
{
	input(
		f < 0 ? (stat & 8) != 0 : (stat & 4) != 0,
		(stat & 2) != 0,
		f < 0 ? (stat & 4) != 0 : (stat & 8) != 0,
		(stat & 1) != 0,
		(stat & 16) != 0, (stat & 32) != 0, (stat & 64) != 0,
		(stat & 128) != 0, (stat & 256) != 0, (stat & 512) != 0,
		(stat & 1024) != 0, (stat & 2048) != 0, (stat & 4096) != 0, (stat & 8192) != 0);
}

void InputBuffer::reset()
{
	m_Bb=0;m_Db=0;m_Fb=0;m_Ub=0;m_ab=0;m_bb=0;m_cb=0;m_xb=0;m_yb=0;m_zb=0;m_qb=0;m_wb=0;m_eb=0;m_sb=0;
	m_B=-1;m_D=-1;m_F=-1;m_U=-1;m_a=-1;m_b=-1;m_c=-1;m_x=-1;m_y=-1;m_z=-1;m_q=-1;m_w=-1;m_e=-1;m_s=-1;m_esc=-1;
}

int InputBuffer::keyState(CmdKey k)
{
	auto mx = [](int a, int b) { return a > b ? a : b; };
	auto mn = [](int a, int b) { return a < b ? a : b; };

	switch (k) {
	case CmdKey::B:  return mn(-mx(m_Db, m_Ub), m_Bb);
	case CmdKey::D:  return mn(-mx(m_Bb, m_Fb), m_Db);
	case CmdKey::F:  return mn(-mx(m_Db, m_Ub), m_Fb);
	case CmdKey::U:  return mn(-mx(m_Bb, m_Fb), m_Ub);
	case CmdKey::DB: return mn(m_Db, m_Bb);
	case CmdKey::UB: return mn(m_Ub, m_Bb);
	case CmdKey::DF: return mn(m_Db, m_Fb);
	case CmdKey::UF: return mn(m_Ub, m_Fb);
	case CmdKey::Bs: return m_Bb;
	case CmdKey::Ds: return m_Db;
	case CmdKey::Fs: return m_Fb;
	case CmdKey::Us: return m_Ub;
	case CmdKey::DBs: return mn(-mx(m_Ub, m_Fb), mx(m_Db, m_Bb));
	case CmdKey::UBs: return mn(-mx(m_Db, m_Fb), mx(m_Ub, m_Bb));
	case CmdKey::DFs: return mn(-mx(m_Ub, m_Bb), mx(m_Db, m_Fb));
	case CmdKey::UFs: return mn(-mx(m_Db, m_Bb), mx(m_Ub, m_Fb));
	case CmdKey::a: return m_ab; case CmdKey::b: return m_bb;
	case CmdKey::c: return m_cb; case CmdKey::x: return m_xb;
	case CmdKey::y: return m_yb; case CmdKey::z: return m_zb;
	case CmdKey::q: return m_qb; case CmdKey::w: return m_wb;
	case CmdKey::e: return m_eb; case CmdKey::s: return m_sb;
	case CmdKey::nB: return -keyState(CmdKey::B);
	case CmdKey::nD: return -keyState(CmdKey::D);
	case CmdKey::nF: return -keyState(CmdKey::F);
	case CmdKey::nU: return -keyState(CmdKey::U);
	case CmdKey::nDB: return -keyState(CmdKey::DB);
	case CmdKey::nUB: return -keyState(CmdKey::UB);
	case CmdKey::nDF: return -keyState(CmdKey::DF);
	case CmdKey::nUF: return -keyState(CmdKey::UF);
	case CmdKey::nBs: return -keyState(CmdKey::Bs);
	case CmdKey::nDs: return -keyState(CmdKey::Ds);
	case CmdKey::nFs: return -keyState(CmdKey::Fs);
	case CmdKey::nUs: return -keyState(CmdKey::Us);
	default: return 0;
	}
}

// ── CommandElement ───────────────────────────────────────────────────────

static CmdKey parseKey(const std::wstring& token)
{
	if (token == L"B")  return CmdKey::B;   if (token == L"D")  return CmdKey::D;
	if (token == L"F")  return CmdKey::F;   if (token == L"U")  return CmdKey::U;
	if (token == L"DB") return CmdKey::DB;  if (token == L"UB") return CmdKey::UB;
	if (token == L"DF") return CmdKey::DF;  if (token == L"UF") return CmdKey::UF;
	if (token == L"/B") return CmdKey::nB;  if (token == L"/D") return CmdKey::nD;
	if (token == L"/F") return CmdKey::nF;  if (token == L"/U") return CmdKey::nU;
	if (token == L"/DB")return CmdKey::nDB; if (token == L"/UB")return CmdKey::nUB;
	if (token == L"/DF")return CmdKey::nDF; if (token == L"/UF")return CmdKey::nUF;
	if (token == L"a")  return CmdKey::a;   if (token == L"b")  return CmdKey::b;
	if (token == L"c")  return CmdKey::c;   if (token == L"x")  return CmdKey::x;
	if (token == L"y")  return CmdKey::y;   if (token == L"z")  return CmdKey::z;
	if (token == L"q")  return CmdKey::q;   if (token == L"w")  return CmdKey::w;
	if (token == L"e")  return CmdKey::e;   if (token == L"s")  return CmdKey::s;
	if (token == L"/a") return CmdKey::na;  if (token == L"/b") return CmdKey::nb;
	if (token == L"/c") return CmdKey::nc;  if (token == L"/x") return CmdKey::nx;
	if (token == L"/y") return CmdKey::ny;  if (token == L"/z") return CmdKey::nz;
	if (token == L"$B") return CmdKey::Bs;  if (token == L"$D") return CmdKey::Ds;
	if (token == L"$F") return CmdKey::Fs;  if (token == L"$U") return CmdKey::Us;
	return CmdKey::B;
}

bool CommandElem::parse(const std::wstring& token)
{
	if (token.empty()) return false;
	time = 0;
	auto t = ikemen::trim(token);
	if (t.empty()) return false;

	// Check for time suffix like "F~30" or "/a"
	size_t tilde = t.find(L'~');
	if (tilde != std::wstring::npos) {
		try { time = std::stoi(t.substr(tilde + 1)); } catch (...) {}
		t = t.substr(0, tilde);
	}
	key = parseKey(ikemen::trim(t));
	return true;
}

bool Command::parse(const std::wstring& line)
{
	auto parts = ikemen::split(L",", line);
	if (parts.size() < 1) return false;

	name = ikemen::trim(parts[0]);
	if (name.empty()) return false;

	time = 15;
	buftime = 1;

	size_t start = 1;
	// Check for "time =" and "buffer.time =" prefix
	if (parts.size() > 2 && ikemen::trim(parts[1]).find(L"time") != std::wstring::npos) {
		try { time = std::stoi(ikemen::trim(parts[2])); } catch (...) {}
		start = 3;
	}

	elements.clear();
	for (size_t i = start; i < parts.size(); i++) {
		CommandElem e;
		if (e.parse(parts[i])) elements.push_back(e);
	}
	return !elements.empty();
}

// ── CommandList ──────────────────────────────────────────────────────────

std::wstring CommandList::loadFile(const std::wstring& filename)
{
	bool unicode;
	auto text = ikemen::loadText(filename, unicode);
	if (text.empty()) return L"Failed to open " + filename;

	auto lines = ikemen::splitLines(text);
	commands.clear();

	for (size_t i = 0; i < lines.size(); i++) {
		auto line = ikemen::trim(lines[i]);
		if (line.empty() || line[0] == L';' || line[0] == L'[') continue;

		// Check for "name = ..." format
		size_t eq = line.find(L'=');
		if (eq == std::wstring::npos) continue;

		auto namePart = ikemen::trim(line.substr(0, eq));
		auto valPart  = ikemen::trim(line.substr(eq + 1));

		if (namePart == L"name") {
			Command cmd;
			if (cmd.parse(valPart)) commands.push_back(cmd);
		} else {
			// "command = ..." line
			if (namePart.find(L"command") != std::wstring::npos) {
				Command cmd;
				if (cmd.parse(valPart)) commands.push_back(cmd);
			}
		}
	}
	return L"";
}

} // namespace ikemen
