#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ikemen {

// ── Input key constants ─────────────────────────────────────────────────

enum class CmdKey : int {
	B, D, F, U, DB, UB, DF, UF,
	nB, nD, nF, nU, nDB, nUB, nDF, nUF,
	Bs, Ds, Fs, Us, DBs, UBs, DFs, UFs,
	nBs, nDs, nFs, nUs, nDBs, nUBs, nDFs, nUFs,
	a, b, c, x, y, z, q, w, e, s,
	na, nb, nc, nx, ny, nz, nq, nw, ne, ns,
};

// ── Input buffer tracker ────────────────────────────────────────────────

class InputBuffer {
public:
	InputBuffer();

	void input(bool B, bool D, bool F, bool U, bool a, bool b, bool c,
	           bool x, bool y, bool z, bool q, bool w, bool e, bool s);
	void inputStat(int stat, int facing);
	void reset();
	int  keyState(CmdKey k);

private:
	int m_Bb=0, m_Db=0, m_Fb=0, m_Ub=0;
	int m_ab=0, m_bb=0, m_cb=0, m_xb=0, m_yb=0, m_zb=0, m_qb=0, m_wb=0, m_eb=0, m_sb=0;
	int8_t m_B=0, m_D=0, m_F=0, m_U=0;
	int8_t m_a=0, m_b=0, m_c=0, m_x=0, m_y=0, m_z=0, m_q=0, m_w=0, m_e=0, m_s=0, m_esc=0;
};

// ── Command element ─────────────────────────────────────────────────────

struct CommandElem {
	CmdKey key = CmdKey::B;
	int    time = 0;

	bool parse(const std::wstring& token);
};

// ── Command ─────────────────────────────────────────────────────────────

struct Command {
	std::wstring name;
	int          time = 15;    // buffer time
	int          buftime = 1;
	std::vector<CommandElem> elements;

	bool parse(const std::wstring& line);
};

// ── Command list ────────────────────────────────────────────────────────

struct CommandList {
	std::vector<Command> commands;
	std::wstring loadFile(const std::wstring& filename);
};

} // namespace ikemen
