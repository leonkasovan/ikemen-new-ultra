// command_service.hpp — Native C++ implementation of ssz_script/ssz/command.ssz
//
// command.ssz (1571 lines) implements character command input processing —
// command definitions, key buffers, state machine for command execution,
// netplay input, and replay recording/playback.
//
// Phase 5: Real implementation for Buffer, KeyInfo, Command, CommandList,
// and module-level input processing. Networking and replay deferred.

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>

namespace ikemen::ssz_native {

// =========================================================================
// Key enum — matches |Key in command.ssz
// =========================================================================
enum class Key : int8_t {
	B, D, F, U, DB, UB, DF, UF,
	nB, nD, nF, nU, nDB, nUB, nDF, nUF,
	Bs, Ds, Fs, Us, DBs, UBs, DFs, UFs,
	nBs, nDs, nFs, nUs, nDBs, nUBs, nDFs, nUFs,
	a, b, c, x, y, z, q, w, e, s,
	na, nb, nc, nx, ny, nz, nq, nw, ne, ns,
};

// =========================================================================
// PlaybackState enum — matches |PlaybackState
// =========================================================================
enum class PlaybackState : int8_t {
	None, CtrlP2, Record, Play, Loop, Wait, End
};

// =========================================================================
// NetState enum — matches |NetState
// =========================================================================
enum class NetState : int8_t {
	Stop, Playing, End, Stoped, Error
};

// =========================================================================
// Buffer — Input state buffer (matches &Buffer)
// =========================================================================
struct BufferData {
	// Direction buffers
	int Bb{}, Db{}, Fb{}, Ub{};
	// Button buffers
	int ab{}, bb{}, cb{}, xb{}, yb{}, zb{}, qb{}, wb{}, eb{}, sb{};
	// Direction states (1 or -1)
	int8_t B{-1}, D{-1}, F{-1}, U{-1};
	// Button states
	int8_t a{-1}, b{-1}, c{-1}, x{-1}, y{-1}, z{-1};
	int8_t q{-1}, w{-1}, e{-1}, s{-1}, esc{-1};

	void input(bool B_, bool D_, bool F_, bool U_,
		bool a_, bool b_, bool c_, bool x_, bool y_, bool z_,
		bool q_, bool w_, bool e_, bool s_);
	void inputStat(int stat, int f);
	void reset();
	int keyState(Key k) const;
	int keyState2(Key k) const;
	int lastDirectionTime() const;
	int lastChangeTime() const;
};

// =========================================================================
// KeyInfo — Per-player key configuration (matches &KeyInfo)
// =========================================================================
struct KeyInfoData {
	int localIn{};

	void setLocalIn(int in);
	void input(BufferData& b, int f) const;
	bool anybutton() const;
	bool startbutton() const;
};

// =========================================================================
// CmdButton — Single command button definition
// =========================================================================
struct CmdButtonData {
	Key key{};
	int buf{};
	bool hold{};

	bool isDirection() const;
	void clear();
};

// =========================================================================
// Command — A named command (matches &Command)
// =========================================================================
struct CommandData {
	std::string name;
	std::vector<CmdButtonData> cmd;
	int time{};
	int buffertime{};

	void readCmd(const std::string& cmdstr);
	void step(BufferData& kbuf, bool ai, bool hitpause, int buftime);
	void clear();
	void copy(const CommandData& other);

	// Runtime state
	int currentStep{};
	int holdTime{};
};

// =========================================================================
// CommandList — List of commands (matches &CommandList)
// =========================================================================
struct CommandListData {
	std::vector<CommandData> list;

	void clear();
	void copyList(const CommandListData& other);
	void input(int inputBits, int facing);
	void step(int facing, bool ai, bool hitpause, int buftime);
	void bufReset();
	void add(const CommandData& c);
	CommandData* get(const std::string& name);
	CommandData* at(int i);
	int size() const;
};

// =========================================================================
// Module-level state
// =========================================================================

struct CommandState {
	bool disablePadP1{}, disablePadP2{};
	PlaybackState pbState{PlaybackState::None};
	int pbCfgRecSlot{1}, pbCfgPlaySlot{1}, pbCfgPlayOrder{};
	bool pbCfgPlayLoop{};
	// ^bool pbCfgSlot — array of 5 flags
	std::vector<bool> pbCfgSlot{std::vector<bool>(5, false)};
	int rFacing{}, pbFacing{};
	// Input buffers per player
	BufferData buf[2];
	KeyInfoData keyInfo[2];
	// sBuf: ^int sBuf.new(2)
	int sBuf[2]{};
};

// =========================================================================
// Module-level API
// =========================================================================

void command_init();
bool command_mod_key_state(bool keyState, int jn, int key);
void command_reset_read_keymap();
bool command_update();
bool command_synchronize();
bool command_start_button(int pn);
bool command_intro_button();
bool command_any_button();

// Accessors for internal state
CommandState& command_get_state();

// Backward-compatible alias for existing test code.
using CommandStateOld = CommandState;

} // namespace ikemen::ssz_native
