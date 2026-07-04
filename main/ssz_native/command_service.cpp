// command_service.cpp — Real implementations for command.ssz
//
// Phase 5: Buffer input processing, KeyInfo, Command parsing/stepping,
// CommandList management, and module-level state.

#include "command_service.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>

namespace ikemen::ssz_native {

// =========================================================================
// Utility helpers
// =========================================================================

static int max_i(int x, int y) { return x > y ? x : y; }
static int min_i(int x, int y) { return x < y ? x : y; }

// =========================================================================
// Module-level state
// =========================================================================
static CommandState g_cmd_state;

CommandState& command_get_state() { return g_cmd_state; }

// =========================================================================
// BufferData
// =========================================================================

void BufferData::input(bool B_, bool D_, bool F_, bool U_,
	bool a_, bool b_, bool c_, bool x_, bool y_, bool z_,
	bool q_, bool w_, bool e_, bool s_)
{
	auto update_dir = [](bool pressed, int8_t& state, int& buf) {
		if (pressed != (state > 0)) {
			buf = 0; state = static_cast<int8_t>(-state);
		}
		buf += state;
	};
	update_dir(B_, B, Bb);
	update_dir(D_, D, Db);
	update_dir(F_, F, Fb);
	update_dir(U_, U, Ub);

	auto update_btn = [](bool pressed, int8_t& state, int& buf) {
		if (pressed != (state > 0)) {
			buf = 0; state = static_cast<int8_t>(-state);
		}
		buf += state;
	};
	update_btn(a_, a, ab);
	update_btn(b_, b, bb);
	update_btn(c_, c, cb);
	update_btn(x_, x, xb);
	update_btn(y_, y, yb);
	update_btn(z_, z, zb);
	update_btn(q_, q, qb);
	update_btn(w_, w, wb);
	update_btn(e_, e, eb);
	update_btn(s_, s, sb);
}

void BufferData::inputStat(int stat, int f) {
	input(
		f < 0 ? (stat & 8) != 0 : (stat & 4) != 0,  // B
		(stat & 2) != 0,                                // D
		f < 0 ? (stat & 4) != 0 : (stat & 8) != 0,    // F
		(stat & 1) != 0,                                // U
		(stat & 16) != 0,   (stat & 32) != 0,   (stat & 64) != 0,
		(stat & 128) != 0,  (stat & 256) != 0,  (stat & 512) != 0,
		(stat & 1024) != 0, (stat & 2048) != 0, (stat & 4096) != 0,
		(stat & 8192) != 0);
}

void BufferData::reset() {
	Bb = Db = Fb = Ub = 0;
	ab = bb = cb = xb = yb = zb = qb = wb = eb = sb = 0;
	B = D = F = U = -1;
	a = b = c = x = y = z = q = w = e = s = -1;
	esc = -1;
}

int BufferData::keyState(Key k) const {
	switch (k) {
	case Key::B:  return -max_i(max_i(Db, Ub), Bb);
	case Key::D:  return -max_i(max_i(Bb, Fb), Db);
	case Key::F:  return -max_i(max_i(Db, Ub), Fb);
	case Key::U:  return -max_i(max_i(Bb, Fb), Ub);
	case Key::DB: return min_i(Db, Bb);
	case Key::UB: return min_i(Ub, Bb);
	case Key::DF: return min_i(Db, Fb);
	case Key::UF: return min_i(Ub, Fb);
	case Key::Bs: return Bb;
	case Key::Ds: return Db;
	case Key::Fs: return Fb;
	case Key::Us: return Ub;
	case Key::DBs: return -max_i(max_i(Ub, Fb), max_i(Db, Bb));
	case Key::UBs: return -max_i(max_i(Db, Fb), max_i(Ub, Bb));
	case Key::DFs: return -max_i(max_i(Ub, Bb), max_i(Db, Fb));
	case Key::UFs: return -max_i(max_i(Db, Bb), max_i(Ub, Fb));
	case Key::a: return ab;
	case Key::b: return bb;
	case Key::c: return cb;
	case Key::x: return xb;
	case Key::y: return yb;
	case Key::z: return zb;
	case Key::q: return qb;
	case Key::w: return wb;
	case Key::e: return eb;
	case Key::s: return sb;
	case Key::nB:  return -(-max_i(max_i(Db, Ub), Bb));
	case Key::nD:  return -(-max_i(max_i(Bb, Fb), Db));
	case Key::nF:  return -(-max_i(max_i(Db, Ub), Fb));
	case Key::nU:  return -(-max_i(max_i(Bb, Fb), Ub));
	case Key::nDB: return -min_i(Db, Bb);
	case Key::nUB: return -min_i(Ub, Bb);
	case Key::nDF: return -min_i(Db, Fb);
	case Key::nUF: return -min_i(Ub, Fb);
	case Key::nBs: return -Bb;
	case Key::nDs: return -Db;
	case Key::nFs: return -Fb;
	case Key::nUs: return -Ub;
	case Key::nDBs: return -(-max_i(max_i(Ub, Fb), max_i(Db, Bb)));
	case Key::nUBs: return -(-max_i(max_i(Db, Fb), max_i(Ub, Bb)));
	case Key::nDFs: return -(-max_i(max_i(Ub, Bb), max_i(Db, Fb)));
	case Key::nUFs: return -(-max_i(max_i(Db, Bb), max_i(Ub, Fb)));
	case Key::na: return -ab;
	case Key::nb: return -bb;
	case Key::nc: return -cb;
	case Key::nx: return -xb;
	case Key::ny: return -yb;
	case Key::nz: return -zb;
	case Key::nq: return -qb;
	case Key::nw: return -wb;
	case Key::ne: return -eb;
	case Key::ns: return -sb;
	default: return 0;
	}
}

int BufferData::keyState2(Key k) const {
	auto foo = [](int a, int b, int c) -> int {
		if (a > 0) return -max_i(b, c);
		if (b > 0) return -max_i(a, c);
		if (c > 0) return -max_i(a, b);
		return -max_i(a, max_i(b, c));
	};
	switch (k) {
	case Key::Bs: return Bb < 0 ? Bb : min_i(std::abs(Bb), min_i(std::abs(Db), std::abs(Ub)));
	case Key::Ds: return Db < 0 ? Db : min_i(std::abs(Db), min_i(std::abs(Bb), std::abs(Fb)));
	case Key::Fs: return Fb < 0 ? Fb : min_i(std::abs(Fb), min_i(std::abs(Db), std::abs(Ub)));
	case Key::Us: return Ub < 0 ? Ub : min_i(std::abs(Ub), min_i(std::abs(Bb), std::abs(Fb)));
	case Key::DBs: { int ks = keyState(Key::DBs); return ks < 0 ? ks : min_i(std::abs(Db), std::abs(Bb)); }
	case Key::UBs: { int ks = keyState(Key::UBs); return ks < 0 ? ks : min_i(std::abs(Ub), std::abs(Bb)); }
	case Key::DFs: { int ks = keyState(Key::DFs); return ks < 0 ? ks : min_i(std::abs(Db), std::abs(Fb)); }
	case Key::UFs: { int ks = keyState(Key::UFs); return ks < 0 ? ks : min_i(std::abs(Ub), std::abs(Fb)); }
	case Key::nBs: return foo(keyState(Key::B),  keyState(Key::DB), keyState(Key::UB));
	case Key::nDs: return foo(keyState(Key::D),  keyState(Key::DB), keyState(Key::DF));
	case Key::nFs: return foo(keyState(Key::F),  keyState(Key::DF), keyState(Key::UF));
	case Key::nUs: return foo(keyState(Key::U),  keyState(Key::UB), keyState(Key::UF));
	case Key::nDBs: return foo(keyState(Key::DB), keyState(Key::DBs), keyState(Key::B));
	case Key::nUBs: return foo(keyState(Key::UB), keyState(Key::UBs), keyState(Key::U));
	case Key::nDFs: return foo(keyState(Key::DF), keyState(Key::DFs), keyState(Key::D));
	case Key::nUFs: return foo(keyState(Key::UF), keyState(Key::UFs), keyState(Key::U));
	default: return keyState(k);
	}
}

int BufferData::lastDirectionTime() const {
	// From SSZ: #.keyState(.Key::B) ...
	// Returns the absolute value of the smallest non-zero direction key state
	int b = keyState(Key::B);
	int d = keyState(Key::D);
	int f = keyState(Key::F);
	int u = keyState(Key::U);
	// Find the one with smallest absolute value that's non-zero
	int result = 0;
	if (b != 0 && (result == 0 || std::abs(b) < std::abs(result))) result = b;
	if (d != 0 && (result == 0 || std::abs(d) < std::abs(result))) result = d;
	if (f != 0 && (result == 0 || std::abs(f) < std::abs(result))) result = f;
	if (u != 0 && (result == 0 || std::abs(u) < std::abs(result))) result = u;
	return std::abs(result);
}

int BufferData::lastChangeTime() const {
	// SSZ: ret .lastDirectionTime();
	return lastDirectionTime();
}

// =========================================================================
// KeyInfoData
// =========================================================================

void KeyInfoData::setLocalIn(int in) { localIn = in; }

void KeyInfoData::input(BufferData& b, int f) const {
	b.inputStat(localIn, f);
}

bool KeyInfoData::anybutton() const {
	return (localIn & 0x3FF0) != 0;  // Check any button bits
}

bool KeyInfoData::startbutton() const {
	return (localIn & 0x10) != 0;  // Check a-button (or start)
}

// =========================================================================
// CmdButtonData
// =========================================================================

bool CmdButtonData::isDirection() const {
	return key >= Key::B && key <= Key::nUFs;
}

void CmdButtonData::clear() {
	key = Key::a;
	buf = 0;
	hold = false;
}

// =========================================================================
// CommandData
// =========================================================================

void CommandData::readCmd(const std::string& cmdstr) {
	// Parse command string into CmdButton sequence
	// Format: "~D, DF, F, a" etc.
	cmd.clear();
	currentStep = 0;
	holdTime = 0;
	buffertime = 0;

	// Simple parser: split by comma, parse each token
	std::string s = cmdstr;
	std::vector<std::string> tokens;
	size_t pos = 0;
	while ((pos = s.find(',')) != std::string::npos) {
		std::string token = s.substr(0, pos);
		// Trim
		size_t start = token.find_first_not_of(" \t");
		if (start != std::string::npos) token = token.substr(start);
		size_t end = token.find_last_not_of(" \t");
		if (end != std::string::npos) token = token.substr(0, end + 1);
		if (!token.empty()) tokens.push_back(token);
		s.erase(0, pos + 1);
	}
	// Last token
	{
		size_t start = s.find_first_not_of(" \t");
		if (start != std::string::npos) s = s.substr(start);
		size_t end = s.find_last_not_of(" \t");
		if (end != std::string::npos) s = s.substr(0, end + 1);
		if (!s.empty()) tokens.push_back(s);
	}

	for (const auto& token : tokens) {
		if (token.empty()) continue;
		if (token[0] == '~') {
			// Hold/timed command
			CmdButtonData btn;
			btn.hold = true;
			// Parse rest as key
			std::string rest = token.substr(1);
			if (rest == "B") btn.key = Key::B;
			else if (rest == "D") btn.key = Key::D;
			else if (rest == "F") btn.key = Key::F;
			else if (rest == "U") btn.key = Key::U;
			else if (rest == "DB") btn.key = Key::DB;
			else if (rest == "UB") btn.key = Key::UB;
			else if (rest == "DF") btn.key = Key::DF;
			else if (rest == "UF") btn.key = Key::UF;
			else if (rest == "a") btn.key = Key::a;
			else if (rest == "b") btn.key = Key::b;
			else if (rest == "c") btn.key = Key::c;
			else if (rest == "x") btn.key = Key::x;
			else if (rest == "y") btn.key = Key::y;
			else if (rest == "z") btn.key = Key::z;
			else if (rest == "q") btn.key = Key::q;
			else if (rest == "w") btn.key = Key::w;
			else if (rest == "e") btn.key = Key::e;
			else if (rest == "s") btn.key = Key::s;
			else continue;
			cmd.push_back(btn);
		} else if (token[0] == '>') {
			// Release direction
			std::string rest = token.substr(1);
			if (rest == "B") { cmd.push_back({Key::nB}); }
			else if (rest == "D") { cmd.push_back({Key::nD}); }
			else if (rest == "F") { cmd.push_back({Key::nF}); }
			else if (rest == "U") { cmd.push_back({Key::nU}); }
		} else if (token[0] == '/' || token[0] == '&') {
			// Buffer time specification
			std::string rest = token.substr(1);
			buffertime = std::atoi(rest.c_str());
		} else {
			// Simple key press
			CmdButtonData btn;
			if (token == "B") btn.key = Key::B;
			else if (token == "D") btn.key = Key::D;
			else if (token == "F") btn.key = Key::F;
			else if (token == "U") btn.key = Key::U;
			else if (token == "DB") btn.key = Key::DB;
			else if (token == "UB") btn.key = Key::UB;
			else if (token == "DF") btn.key = Key::DF;
			else if (token == "UF") btn.key = Key::UF;
			else if (token == "a") btn.key = Key::a;
			else if (token == "b") btn.key = Key::b;
			else if (token == "c") btn.key = Key::c;
			else if (token == "x") btn.key = Key::x;
			else if (token == "y") btn.key = Key::y;
			else if (token == "z") btn.key = Key::z;
			else if (token == "q") btn.key = Key::q;
			else if (token == "w") btn.key = Key::w;
			else if (token == "e") btn.key = Key::e;
			else if (token == "s") btn.key = Key::s;
			else continue;
			cmd.push_back(btn);
		}
	}
}

void CommandData::step(BufferData& kbuf, bool ai, bool hitpause, int buftime) {
	if (cmd.empty()) return;
	if (currentStep >= static_cast<int>(cmd.size())) return;

	// Check current step
	auto& btn = cmd[currentStep];
	int ks = kbuf.keyState(btn.key);

	if (btn.hold) {
		// Hold-type: must be held for the hold time
		if (ks > 0) {
			holdTime++;
		} else {
			holdTime = 0;
		}
	} else {
		// Tap-type: just check if the key is pressed this frame
		if (ks > 0 && !hitpause) {
			currentStep++;
			if (currentStep >= static_cast<int>(cmd.size())) {
				// Command completed — set buffertime
				time = buftime > 0 ? buftime : buffertime;
			}
		}
	}
}

void CommandData::clear() {
	cmd.clear();
	time = 0;
	currentStep = 0;
	holdTime = 0;
	buffertime = 0;
	name.clear();
}

void CommandData::copy(const CommandData& other) {
	name = other.name;
	cmd = other.cmd;
	time = other.time;
	buffertime = other.buffertime;
	currentStep = other.currentStep;
	holdTime = other.holdTime;
}

// =========================================================================
// CommandListData
// =========================================================================

void CommandListData::clear() { list.clear(); }

void CommandListData::copyList(const CommandListData& other) {
	list = other.list;
}

void CommandListData::input(int inputBits, int facing) {
	// Forward raw input bits to each command's buffer
	// In SSZ this updates the global buffer via KeyInfo
	(void)inputBits; (void)facing;
}

void CommandListData::step(int facing, bool ai, bool hitpause, int buftime) {
	for (auto& cmd : list) {
		cmd.step(g_cmd_state.buf[0], ai, hitpause, buftime);
	}
}

void CommandListData::bufReset() {
	for (auto& cmd : list) {
		cmd.currentStep = 0;
		cmd.holdTime = 0;
		cmd.time = 0;
	}
}

void CommandListData::add(const CommandData& c) {
	list.push_back(c);
}

CommandData* CommandListData::get(const std::string& name) {
	for (auto& cmd : list) {
		if (cmd.name == name) return &cmd;
	}
	return nullptr;
}

CommandData* CommandListData::at(int i) {
	if (i >= 0 && i < static_cast<int>(list.size()))
		return &list[i];
	return nullptr;
}

int CommandListData::size() const { return static_cast<int>(list.size()); }

// =========================================================================
// Module-level API
// =========================================================================

void command_init() {
	g_cmd_state = CommandState{};
}

bool command_mod_key_state(bool keyState, int jn, int key) {
	// Deferred — depends on SDL event processing
	(void)keyState; (void)jn; (void)key;
	return false;
}

void command_reset_read_keymap() {
	// Deferred — depends on config/key mapping
}

bool command_update() {
	// SSZ: main update loop — reads input, updates buffers, syncs netplay
	// For now, just return true
	return true;
}

bool command_synchronize() {
	// SSZ: synchronize netplay state
	return true;
}

bool command_start_button(int pn) {
	if (pn < 0 || pn > 1) return false;
	return g_cmd_state.keyInfo[pn].startbutton();
}

bool command_intro_button() {
	return g_cmd_state.keyInfo[0].anybutton();
}

bool command_any_button() {
	return g_cmd_state.keyInfo[0].anybutton() || g_cmd_state.keyInfo[1].anybutton();
}

} // namespace ikemen::ssz_native
