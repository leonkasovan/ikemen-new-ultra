// script_service.cpp — Full native implementation of ssz_script/ssz/script.ssz
//
// Phase 5: All 190+ Lua-callable functions implemented. Each function:
//   1. Reads arguments from the Lua stack using the Lua C API
//   2. Delegates to the appropriate native service module
//   3. Pushes results back to the Lua stack
//
// Registration: script_init(lua_State* L) registers all functions as Lua
// globals via lua_register(). Functions that are simple getters/setters for
// CommonData or SdleventState fields are grouped into macros for conciseness.

#include "script_service.hpp"

// Native service headers
#include "common_service.hpp"
#include "sff_service.hpp"
#include "command_service.hpp"
#include "config_service.hpp"
#include "math_service.hpp"
#include "mesdialog_service.hpp"
#include "sdlplugin_service.hpp"
#include "sdlevent_service.hpp"
#include "shell_service.hpp"
#include "sound_resource_service.hpp"
#if IKEMEN_NATIVE_STRING_LIB
#include "string_service.hpp"
#endif
#include "video_service.hpp"
#include "ssz_trace.hpp"

// Lua C API
#include <lua.hpp>

#include <new>
#include <string>
#include <vector>

namespace ikemen::ssz_native {

// =========================================================================
// Module-level state
// =========================================================================

namespace {

ScriptState g_script_state;
lua_State* g_lua_state{nullptr};

// Helper: get Lua argument as int (with error handling matching SSZ numArg).
int lua_get_int(lua_State* L, int idx, int default_val) {
	if (lua_isnumber(L, idx))
		return static_cast<int>(lua_tonumber(L, idx));
	return default_val;
}

// Helper: get Lua argument as bool.
bool lua_get_bool(lua_State* L, int idx, bool default_val) {
	if (lua_isboolean(L, idx))
		return lua_toboolean(L, idx) != 0;
	return default_val;
}

// Helper: get Lua argument as string.
std::string lua_get_string(lua_State* L, int idx, const std::string& default_val) {
	if (lua_isstring(L, idx))
		return lua_tostring(L, idx);
	return default_val;
}

} // anonymous namespace

// =========================================================================
// State accessors
// =========================================================================

ScriptState& script_get_state() {
	return g_script_state;
}

void script_set_lua_state(lua_State* L) {
	g_lua_state = L;
}

// =========================================================================
// Lua-callable function implementations
// =========================================================================

// ── Argument parsing functions ──

static int lua_numArg(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "script::numArg");
	int argc = lua_gettop(L);
	if (argc < 1 || !lua_isnumber(L, 1)) {
		lua_pushstring(L, "argc: The argument is not a number.");
		lua_error(L);
		return 0;
	}
	int idx = static_cast<int>(lua_tonumber(L, 1));
	if (!lua_isnumber(L, idx + 1)) {
		lua_pushstring(L, "The argument is not a number.");
		lua_error(L);
		return 0;
	}
	lua_pushnumber(L, lua_tonumber(L, idx + 1));
	return 1;
}

static int lua_blArg(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "script::blArg");
	int argc = lua_gettop(L);
	if (argc < 1 || !lua_isnumber(L, 1)) {
		lua_pushboolean(L, false);
		return 1;
	}
	int idx = static_cast<int>(lua_tonumber(L, 1));
	if (!lua_isboolean(L, idx + 1)) {
		lua_pushstring(L, "The argument is not a boolean.");
		lua_error(L);
		return 0;
	}
	lua_pushboolean(L, lua_toboolean(L, idx + 1));
	return 1;
}

static int lua_strArg(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "script::strArg");
	int argc = lua_gettop(L);
	if (argc < 1 || !lua_isnumber(L, 1)) {
		lua_pushstring(L, "");
		return 1;
	}
	int idx = static_cast<int>(lua_tonumber(L, 1));
	if (!lua_isstring(L, idx + 1)) {
		lua_pushstring(L, "The argument is not a string.");
		lua_error(L);
		return 0;
	}
	lua_pushstring(L, lua_tostring(L, idx + 1));
	return 1;
}

// refArg(re, argc, type_name, ...) — SSZ refArg pattern equivalent.
// Checks that outer argument at index `re` exists (re <= argc) and passes
// it through. If out of bounds, errors with the type name.
// Usage from Lua:
//   local sff = refArg(1, select("#", ...), "Sff", ...)
static int lua_refArg(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "script::refArg");
	int re = lua_get_int(L, 1, 0);
	int argc = lua_get_int(L, 2, 0);
	std::string type_name = lua_get_string(L, 3, "object");

	if (re <= 0 || re > argc) {
		lua_pushfstring(L, "The argument is not a %s.", type_name.c_str());
		lua_error(L);
		return 0;
	}

	// Forwarded variadic args start at Lua stack position 4
	if (lua_gettop(L) < re + 3) {
		lua_pushnil(L);
		return 1;
	}
	lua_pushvalue(L, re + 3);
	return 1;
}

// =========================================================================
// Userdata wrappers for Lua-accessible objects
//
// Each wrapper type stores a heap-allocated C++ object pointer in a
// Lua full userdata block with a named metatable for type safety and
// __gc finalization.
// =========================================================================

namespace {

// Metatable names (used with luaL_newmetatable / luaL_getmetatable)
constexpr const char* MT_SFF  = "IKEMEN.Sff";
constexpr const char* MT_SND  = "IKEMEN.Snd";
constexpr const char* MT_FONT = "IKEMEN.Font";
constexpr const char* MT_CMD  = "IKEMEN.Command";
constexpr const char* MT_INPUT_DLG = "IKEMEN.InputDialog";

// ── Userdata payload structs ──

struct SffUD { SffData* sff; };
struct SndUD { SoundTable* snd; };
struct FontUD { Font* font; };
struct CmdUD  { CommandListData* cmd; };
struct InputDlgUD {
	std::string result;
	bool done{true};
};

// ── Metatable __gc callbacks ──

static int gc_sff(lua_State* L) {
	auto* ud = static_cast<SffUD*>(lua_touserdata(L, 1));
	if (ud && ud->sff) { delete ud->sff; ud->sff = nullptr; }
	return 0;
}

static int gc_snd(lua_State* L) {
	auto* ud = static_cast<SndUD*>(lua_touserdata(L, 1));
	if (ud && ud->snd) { delete ud->snd; ud->snd = nullptr; }
	return 0;
}

static int gc_font(lua_State* L) {
	auto* ud = static_cast<FontUD*>(lua_touserdata(L, 1));
	if (ud && ud->font) { ud->font->close(); delete ud->font; ud->font = nullptr; }
	return 0;
}

static int gc_cmd(lua_State* L) {
	auto* ud = static_cast<CmdUD*>(lua_touserdata(L, 1));
	if (ud && ud->cmd) { delete ud->cmd; ud->cmd = nullptr; }
	return 0;
}

// ── Helper to create a metatable on a Lua state (called once in script_init) ──

static void create_metatable(lua_State* L, const char* name, lua_CFunction gc_fn) {
	luaL_newmetatable(L, name);
	lua_pushstring(L, "__gc");
	lua_pushcfunction(L, gc_fn);
	lua_settable(L, -3);
	lua_pop(L, 1);
}

} // anonymous namespace

// ── SFF functions ──

static int lua_sffNew(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "script::sffNew");
	std::string file = lua_get_string(L, 1, "");

	SffData* sff = new SffData();
	std::string err = sff->loadFile(file, false);
	if (!err.empty()) {
		delete sff;
		lua_pushstring(L, (file + " " + err).c_str());
		lua_error(L);
		return 0;
	}

	auto* ud = static_cast<SffUD*>(lua_newuserdata(L, sizeof(SffUD)));
	ud->sff = sff;
	luaL_getmetatable(L, MT_SFF);
	lua_setmetatable(L, -2);
	return 1;
}

// ── Sound functions ──

static int lua_sndNew(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "script::sndNew");
	std::string file = lua_get_string(L, 1, "");

	SoundTable* snd = new SoundTable();
	std::string err = snd->loadFile(file);
	if (!err.empty()) {
		delete snd;
		lua_pushstring(L, (file + " " + err).c_str());
		lua_error(L);
		return 0;
	}

	auto* ud = static_cast<SndUD*>(lua_newuserdata(L, sizeof(SndUD)));
	ud->snd = snd;
	luaL_getmetatable(L, MT_SND);
	lua_setmetatable(L, -2);
	return 1;
}

static int lua_sndPlay(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "script::sndPlay");
	// sndPlay(snd_ud, group, number)
	auto* ud = ref_arg<SndUD>(L, 1, MT_SND);
	int g = lua_get_int(L, 2, 0);
	int n = lua_get_int(L, 3, 0);
	const WaveData* wave = ud->snd->getSound(g, n);
	if (wave) {
		SoundChannel* ch = get_channel(-1);
		if (ch) {
			ch->wave = wave;
			ch->setDefaultParameter();
			play_sound();
		}
	}
	return 0;
}

static int lua_sndStop(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "script::sndStop");
	(void)L;
	stop_sound();
	return 0;
}

// ── Font functions ──

static int lua_fontNew(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "script::fontNew");
	std::string file = lua_get_string(L, 1, "");

	Font* font = new Font();
	font->open(file, 12); // default point size
	// Note: Font wraps SDL_ttf TTF_Font. FNT sprite fonts not yet handled
	// (requires font_service full wiring)

	auto* ud = static_cast<FontUD*>(lua_newuserdata(L, sizeof(FontUD)));
	ud->font = font;
	luaL_getmetatable(L, MT_FONT);
	lua_setmetatable(L, -2);
	return 1;
}

// ── Command functions ──

static int lua_modKeyState(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "script::modKeyState");
	bool keyState = lua_get_bool(L, 1, false);
	int jn = lua_get_int(L, 2, -1);
	int key = lua_get_int(L, 3, 0);
	lua_pushboolean(L, command_mod_key_state(keyState, jn, key));
	return 1;
}

static int lua_commandNew(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "script::commandNew");
	auto* cmd = new CommandListData();

	auto* ud = static_cast<CmdUD*>(lua_newuserdata(L, sizeof(CmdUD)));
	ud->cmd = cmd;
	luaL_getmetatable(L, MT_CMD);
	lua_setmetatable(L, -2);
	return 1;
}

static int lua_commandAdd(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "script::commandAdd");
	auto* ud = ref_arg<CmdUD>(L, 1, MT_CMD);
	std::string name = lua_get_string(L, 2, "");
	std::string com = lua_get_string(L, 3, "");
	int time = lua_get_int(L, 4, 1);

	CommandData c;
	c.name = name;
	command_reset_read_keymap();
	c.readCmd(com);
	c.time = time;
	ud->cmd->add(c);
	return 0;
}

static int lua_commandGetState(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "script::commandGetState");
	auto* ud = ref_arg<CmdUD>(L, 1, MT_CMD);
	std::string name = lua_get_string(L, 2, "");
	CommandData* cmd = ud->cmd->get(name);
	// SSZ checks cl[i].curbuftime > 0 — holdTime tracks how long
	// the command has been held/active in the buffer.
	lua_pushboolean(L, cmd != nullptr && cmd->holdTime > 0);
	return 1;
}

static int lua_commandInput(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "script::commandInput");
	auto* ud = ref_arg<CmdUD>(L, 1, MT_CMD);
	int pn = lua_get_int(L, 2, 1);
	(void)pn; // reserved for per-player input (both read same keyboard for now)

	// Build input bits from current keyboard state
	auto& sdle = sdlevent_get_state();
	int bits = 0;
	if (sdle.upKey)    bits |= 0x01;
	if (sdle.downKey)  bits |= 0x02;
	if (sdle.leftKey)  bits |= 0x04;
	if (sdle.rightKey) bits |= 0x08;
	if (sdle.zKey)     bits |= 0x010;  // A
	if (sdle.xKey)     bits |= 0x020;  // B
	if (sdle.cKey)     bits |= 0x040;  // C
	if (sdle.aKey)     bits |= 0x080;  // X
	if (sdle.sKey)     bits |= 0x100;  // Y
	if (sdle.dKey)     bits |= 0x200;  // Z
	if (sdle.qKey)     bits |= 0x400;  // Q
	if (sdle.wKey)     bits |= 0x800;  // W
	if (sdle.eKey)     bits |= 0x1000; // E
	if (sdle.returnKey) bits |= 0x2000; // S/start

	ud->cmd->input(bits, 1);
	ud->cmd->step(1, false, false, 0);
	return 0;
}

static int lua_commandBufReset(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "script::commandBufReset");
	auto* ud = ref_arg<CmdUD>(L, 1, MT_CMD);
	ud->cmd->bufReset();
	return 0;
}

// ── System control functions ──

static int lua_startButton(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "script::startButton");
	int pn = lua_get_int(L, 1, 1);
	lua_pushboolean(L, command_start_button(pn));
	return 1;
}

static int lua_getSysCtrl(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "script::getSysCtrl");
	(void)L;
	lua_pushnumber(L, static_cast<double>(common_get_state().sysControls));
	return 1;
}

static int lua_setSysCtrl(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "script::setSysCtrl");
	int v = lua_get_int(L, 1, 0);
	auto& state = common_get_state();
	if (state.sysControls != v)
		state.sysControls = v;
	return 0;
}

// ── Input dialog functions ──

static int lua_inputDialogNew(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "script::inputDialogNew");
	auto* ud = static_cast<InputDlgUD*>(lua_newuserdata(L, sizeof(InputDlgUD)));
	new (ud) InputDlgUD(); // placement new for std::string member
	ud->done = true;
	luaL_getmetatable(L, MT_INPUT_DLG);
	lua_setmetatable(L, -2);
	return 1;
}

static int gc_input_dlg(lua_State* L) {
	auto* ud = static_cast<InputDlgUD*>(lua_touserdata(L, 1));
	if (ud) ud->~InputDlgUD(); // explicit destructor for std::string
	return 0;
}

static int lua_inputDialogPopup(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "script::inputDialogPopup");
	auto* ud = ref_arg<InputDlgUD>(L, 1, MT_INPUT_DLG);
	std::string title = lua_get_string(L, 2, "");
	if (!ud->done)
		return 0; // already active — SSZ returns false in this case
	ud->done = false;
	// Show the dialog (blocks until user responds)
	std::wstring wtitle(title.begin(), title.end());
	std::wstring result = mesdialog::input_str(wtitle);
	ud->result = std::string(result.begin(), result.end());
	ud->done = true;
	return 0;
}

static int lua_inputDialogIsDone(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "script::inputDialogIsDone");
	auto* ud = ref_arg<InputDlgUD>(L, 1, MT_INPUT_DLG);
	lua_pushboolean(L, ud->done);
	return 1;
}

static int lua_inputDialogGetStr(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "script::inputDialogGetStr");
	auto* ud = ref_arg<InputDlgUD>(L, 1, MT_INPUT_DLG);
	if (!ud->done) {
		lua_pushnil(L); // null!char?() equivalent
	} else {
		lua_pushstring(L, ud->result.c_str());
	}
	return 1;
}

// ── Text input functions ──

static int lua_inputText(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "script::inputText");
	std::string mod = lua_get_string(L, 1, "");
	bool dot = lua_get_bool(L, 2, false);
	char16_t lastChar = getLastChar();
	std::string& line = g_script_state.line;

	switch (lastChar) {
	case 0:
	case 0x8: // backspace
		if (!line.empty())
			line.pop_back();
		break;
	default:
		if (mod == "text") {
			if ((lastChar >= 0x20 && lastChar < 0x30) ||
			    (lastChar >= 0x3a && lastChar < 0x7f))
				line += static_cast<char>(lastChar);
		} else if (mod == "num") {
			if ((lastChar >= 0x30 && lastChar < 0x3a) ||
			    (lastChar == 0x2e && dot))
				line += static_cast<char>(lastChar);
		} else {
			if (lastChar >= 0x20 && lastChar < 0x7f)
				line += static_cast<char>(lastChar);
		}
		break;
	}
	lua_pushstring(L, line.c_str());
	return 1;
}

static int lua_setInputText(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "script::setInputText");
	g_script_state.line = lua_get_string(L, 1, "");
	return 0;
}

static int lua_clearInputText(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "script::clearInputText");
	(void)L;
	g_script_state.line.clear();
	return 0;
}

// ── Clipboard functions ──

static int lua_clipboardPaste(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "script::clipboardPaste");
	(void)L;
	lua_pushboolean(L, sdlevent_get_state().paste);
	return 1;
}

static int lua_getClipboardText(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "script::getClipboardText");
	std::wstring clip = mesdialog::get_clipboard_str();
	std::string narrow(clip.begin(), clip.end());
	lua_pushstring(L, narrow.c_str());
	return 1;
}

// ── Rendering functions ──

static int lua_drawTTF(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "script::drawTTF");
	// drawTTF(file, align, text, x, y, scaleX, scaleY, r, g, b, alpha)
	std::string file  = lua_get_string(L, 1, "");
	int         align = lua_get_int(L, 2, 0);
	std::string text  = lua_get_string(L, 3, "");
	int         x     = lua_get_int(L, 4, 0);
	int         y     = lua_get_int(L, 5, 0);
	float       scaleX = static_cast<float>(lua_tonumber(L, 6));
	float       scaleY = static_cast<float>(lua_tonumber(L, 7));
	int         r     = lua_get_int(L, 8, 255);
	int         g     = lua_get_int(L, 9, 255);
	int         b     = lua_get_int(L, 10, 255);
	int         alpha = lua_get_int(L, 11, 255);

	// Clamp scale to valid range
	if (scaleX <= 0.0f) scaleX = 1.0f;
	if (scaleY <= 0.0f) scaleY = 1.0f;

	draw_ttf(file, align, text, x, y, scaleX, scaleY, r, g, b, alpha);
	return 0;
}

// ── Video/BGM functions ──

static int lua_loadVideo(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "script::loadVideo");
	std::string file = lua_get_string(L, 1, "");
	std::string capturePath = lua_get_string(L, 2, "");
	int volume = lua_get_int(L, 3, 100);
	int audioTrack = lua_get_int(L, 4, 1);
	video_play(file, capturePath, volume, audioTrack);
	return 0;
}

static int lua_playBGM(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "script::playBGM");
	std::string file = lua_get_string(L, 1, "");
	sound_resource_get_state().bgm.play(file);
	common_get_state().bgmName = file;
	return 0;
}

static int lua_getBGM(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "script::getBGM");
	(void)L;
	lua_pushstring(L, common_get_state().bgmName.c_str());
	return 1;
}

static int lua_fadeInBGM(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "script::fadeInBGM");
	int time = lua_get_int(L, 1, 0);
	fadeInBGM(time);
	return 0;
}

static int lua_fadeOutBGM(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "script::fadeOutBGM");
	int time = lua_get_int(L, 1, 0);
	fadeOutBGM(time);
	return 0;
}

// ── File opening functions ──

static int lua_sszOpen(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "script::sszOpen");
	std::string filename = lua_get_string(L, 1, "");
	std::string dirname = lua_get_string(L, 2, "");
	std::wstring wdir(dirname.begin(), dirname.end());
	std::wstring wfile(filename.begin(), filename.end());
	shell::open(wdir, L"", wfile, false, true);
	return 0;
}

static int lua_batOpen(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "script::batOpen");
	std::string filename = lua_get_string(L, 1, "");
	std::string dirname = lua_get_string(L, 2, "");
	std::wstring wdir(dirname.begin(), dirname.end());
	std::wstring wfile(filename.begin(), filename.end());
	shell::open(wdir, L"", wfile, true, false);
	return 0;
}

static int lua_webOpen(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "script::webOpen");
	std::string url = lua_get_string(L, 1, "");
	std::wstring wurl(url.begin(), url.end());
	shell::open(wurl, L"", L"", false, false);
	return 0;
}

// ── Key state functions ──
// Each reads a boolean from SdleventState and pushes it to Lua.

#define DEFINE_KEY_FUNC(name, field) \
	static int lua_##name(lua_State* L) { \
		SSZ_TRACE_CAT(TRACE_SYS, "script::" #name); \
		(void)L; \
		lua_pushboolean(L, sdlevent_get_state().field); \
		return 1; \
	}

DEFINE_KEY_FUNC(esc, esc)
DEFINE_KEY_FUNC(upKey, upKey)
DEFINE_KEY_FUNC(downKey, downKey)
DEFINE_KEY_FUNC(leftKey, leftKey)
DEFINE_KEY_FUNC(rightKey, rightKey)
DEFINE_KEY_FUNC(aKey, aKey)
DEFINE_KEY_FUNC(bKey, bKey)
DEFINE_KEY_FUNC(cKey, cKey)
DEFINE_KEY_FUNC(dKey, dKey)
DEFINE_KEY_FUNC(eKey, eKey)
DEFINE_KEY_FUNC(fKey, fKey)
DEFINE_KEY_FUNC(gKey, gKey)
DEFINE_KEY_FUNC(hKey, hKey)
DEFINE_KEY_FUNC(iKey, iKey)
DEFINE_KEY_FUNC(jKey, jKey)
DEFINE_KEY_FUNC(kKey, kKey)
DEFINE_KEY_FUNC(lKey, lKey)
DEFINE_KEY_FUNC(mKey, mKey)
DEFINE_KEY_FUNC(nKey, nKey)
DEFINE_KEY_FUNC(oKey, oKey)
DEFINE_KEY_FUNC(pKey, pKey)
DEFINE_KEY_FUNC(qKey, qKey)
DEFINE_KEY_FUNC(rKey, rKey)
DEFINE_KEY_FUNC(sKey, sKey)
DEFINE_KEY_FUNC(tKey, tKey)
DEFINE_KEY_FUNC(uKey, uKey)
DEFINE_KEY_FUNC(vKey, vKey)
DEFINE_KEY_FUNC(wKey, wKey)
DEFINE_KEY_FUNC(xKey, xKey)
DEFINE_KEY_FUNC(yKey, yKey)
DEFINE_KEY_FUNC(zKey, zKey)
DEFINE_KEY_FUNC(kzeroKey, kzeroKey)
DEFINE_KEY_FUNC(koneKey, koneKey)
DEFINE_KEY_FUNC(ktwoKey, ktwoKey)
DEFINE_KEY_FUNC(kthreeKey, kthreeKey)
DEFINE_KEY_FUNC(kfourKey, kfourKey)
DEFINE_KEY_FUNC(kfiveKey, kfiveKey)
DEFINE_KEY_FUNC(ksixKey, ksixKey)
DEFINE_KEY_FUNC(ksevenKey, ksevenKey)
DEFINE_KEY_FUNC(keightKey, keightKey)
DEFINE_KEY_FUNC(knineKey, knineKey)
DEFINE_KEY_FUNC(zeroKey, zeroKey)
DEFINE_KEY_FUNC(oneKey, oneKey)
DEFINE_KEY_FUNC(twoKey, twoKey)
DEFINE_KEY_FUNC(threeKey, threeKey)
DEFINE_KEY_FUNC(fourKey, fourKey)
DEFINE_KEY_FUNC(fiveKey, fiveKey)
DEFINE_KEY_FUNC(sixKey, sixKey)
DEFINE_KEY_FUNC(sevenKey, sevenKey)
DEFINE_KEY_FUNC(eightKey, eightKey)
DEFINE_KEY_FUNC(nineKey, nineKey)
DEFINE_KEY_FUNC(returnKey, returnKey)
DEFINE_KEY_FUNC(backspaceKey, backspaceKey)
DEFINE_KEY_FUNC(spaceKey, spaceKey)
DEFINE_KEY_FUNC(lshiftKey, lshiftKey)
DEFINE_KEY_FUNC(rshiftKey, rshiftKey)
DEFINE_KEY_FUNC(tabKey, tabKey)
DEFINE_KEY_FUNC(minusKey, minusKey)
DEFINE_KEY_FUNC(equalsKey, equalsKey)
DEFINE_KEY_FUNC(leftbracketKey, leftbracketKey)
DEFINE_KEY_FUNC(rightbracketKey, rightbracketKey)
DEFINE_KEY_FUNC(backslashKey, backslashKey)
DEFINE_KEY_FUNC(semicolonKey, semicolonKey)
DEFINE_KEY_FUNC(commaKey, commaKey)
DEFINE_KEY_FUNC(periodKey, periodKey)
DEFINE_KEY_FUNC(slashKey, slashKey)
DEFINE_KEY_FUNC(f1Key, f1Key)
DEFINE_KEY_FUNC(f2Key, f2Key)
DEFINE_KEY_FUNC(f3Key, f3Key)
DEFINE_KEY_FUNC(f4Key, f4Key)
DEFINE_KEY_FUNC(f5Key, f5Key)
DEFINE_KEY_FUNC(f9Key, f9Key)
DEFINE_KEY_FUNC(f10Key, f10Key)
DEFINE_KEY_FUNC(f11Key, f11Key)
DEFINE_KEY_FUNC(f12Key, f12Key)
DEFINE_KEY_FUNC(printscreenKey, printscreenKey)
DEFINE_KEY_FUNC(insertKey, insertKey)
DEFINE_KEY_FUNC(homeKey, homeKey)
DEFINE_KEY_FUNC(pageupKey, pageupKey)
DEFINE_KEY_FUNC(deleteKey, deleteKey)
DEFINE_KEY_FUNC(endKey, endKey)
DEFINE_KEY_FUNC(pagedownKey, pagedownKey)
DEFINE_KEY_FUNC(kdivideKey, kdivideKey)
DEFINE_KEY_FUNC(kmultiplyKey, kmultiplyKey)
DEFINE_KEY_FUNC(kminusKey, kminusKey)
DEFINE_KEY_FUNC(kplusKey, kplusKey)
DEFINE_KEY_FUNC(kenterKey, kenterKey)
DEFINE_KEY_FUNC(kperiodKey, kperiodKey)
DEFINE_KEY_FUNC(getGamepadKeyA, getGamepadKeyA)
DEFINE_KEY_FUNC(getGamepadKeyB, getGamepadKeyB)
DEFINE_KEY_FUNC(getGamepadKeyC, getGamepadKeyC)

#undef DEFINE_KEY_FUNC

// ── Random function ──

static int lua_sszRandom(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "script::sszRandom");
	(void)L;
	lua_pushnumber(L, static_cast<double>(math::random()));
	return 1;
}

// ── Game state getter/setter functions ──
// These read/write CommonData fields.

// Helper macros for CommonData getter/setter pattern.
#define DEFINE_GETSET_INT(name, field) \
	static int lua_set##name(lua_State* L) { \
		SSZ_TRACE_CAT(TRACE_SYS, "script::set" #name); \
		common_get_state().field = lua_get_int(L, 1, 0); \
		return 0; \
	} \
	static int lua_get##name(lua_State* L) { \
		SSZ_TRACE_CAT(TRACE_SYS, "script::get" #name); \
		(void)L; \
		lua_pushnumber(L, static_cast<double>(common_get_state().field)); \
		return 1; \
	}

#define DEFINE_GETSET_BOOL(name, field) \
	static int lua_set##name(lua_State* L) { \
		SSZ_TRACE_CAT(TRACE_SYS, "script::set" #name); \
		common_get_state().field = lua_get_bool(L, 1, false); \
		return 0; \
	} \
	static int lua_get##name(lua_State* L) { \
		SSZ_TRACE_CAT(TRACE_SYS, "script::get" #name); \
		(void)L; \
		lua_pushboolean(L, common_get_state().field); \
		return 1; \
	}

#define DEFINE_GETSET_STR(name, field) \
	static int lua_set##name(lua_State* L) { \
		SSZ_TRACE_CAT(TRACE_SYS, "script::set" #name); \
		common_get_state().field = lua_get_string(L, 1, ""); \
		return 0; \
	} \
	static int lua_get##name(lua_State* L) { \
		SSZ_TRACE_CAT(TRACE_SYS, "script::get" #name); \
		(void)L; \
		lua_pushstring(L, common_get_state().field.c_str()); \
		return 1; \
	}

#define DEFINE_SET_INT(name, field) \
	static int lua_set##name(lua_State* L) { \
		SSZ_TRACE_CAT(TRACE_SYS, "script::set" #name); \
		common_get_state().field = lua_get_int(L, 1, 0); \
		return 0; \
	}

#define DEFINE_GET_INT(name, field) \
	static int lua_get##name(lua_State* L) { \
		SSZ_TRACE_CAT(TRACE_SYS, "script::get" #name); \
		(void)L; \
		lua_pushnumber(L, static_cast<double>(common_get_state().field)); \
		return 1; \
	}

#define DEFINE_SET_BOOL(name, field) \
	static int lua_set##name(lua_State* L) { \
		SSZ_TRACE_CAT(TRACE_SYS, "script::set" #name); \
		common_get_state().field = lua_get_bool(L, 1, false); \
		return 0; \
	}

#define DEFINE_GET_BOOL(name, field) \
	static int lua_get##name(lua_State* L) { \
		SSZ_TRACE_CAT(TRACE_SYS, "script::get" #name); \
		(void)L; \
		lua_pushboolean(L, common_get_state().field); \
		return 1; \
	}

#define DEFINE_SET_STR(name, field) \
	static int lua_set##name(lua_State* L) { \
		SSZ_TRACE_CAT(TRACE_SYS, "script::set" #name); \
		common_get_state().field = lua_get_string(L, 1, ""); \
		return 0; \
	}

// Helper for functions that don't match the simple field pattern.

// setAutoguard(pn, ag)
static int lua_setAutoguard(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "script::setAutoguard");
	int pn = lua_get_int(L, 1, 0);
	auto& state = common_get_state();
	if (pn >= 1 && pn <= static_cast<int>(state.autoguard.size())) {
		state.autoguard[pn - 1] = lua_get_bool(L, 2, false);
	}
	return 0;
}

// setPowerShare(pn, ps)
static int lua_setPowerShare(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "script::setPowerShare");
	int pn = lua_get_int(L, 1, 0);
	auto& state = common_get_state();
	if (pn >= 1 && pn <= static_cast<int>(state.powerShare.size())) {
		state.powerShare[pn - 1] = lua_get_bool(L, 2, false);
	}
	return 0;
}

// setHomeTeam(tn)
static int lua_setHomeTeam(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "script::setHomeTeam");
	int tn = lua_get_int(L, 1, 1);
	if (tn >= 1 && tn <= 2)
		common_get_state().home = tn - 1;
	return 0;
}

// The SSZ uses separate set/get + display functions for each win counter.
// We use the macros for the simple ones.

// Simple int getter/setters (generates lua_set<Name> and lua_get<Name>)
DEFINE_GETSET_INT(Credits, credits)
DEFINE_GETSET_INT(Coins, coins)
DEFINE_GETSET_INT(RoundTime, roundTime)
DEFINE_GETSET_INT(RoundsToWin, roundsToWin)
DEFINE_GETSET_INT(P1matchWins, p1matchWins)
DEFINE_GETSET_INT(P2matchWins, p2matchWins)
DEFINE_GETSET_INT(Score, score)
DEFINE_GETSET_INT(ScoreTotal, scoreTotal)
DEFINE_GETSET_INT(P1Score, p1score)
DEFINE_GETSET_INT(P2Score, p2score)
DEFINE_GETSET_INT(MatchNo, match)
DEFINE_GETSET_INT(LastMatch, lastMatch)
DEFINE_GETSET_INT(CPULevel, cpuLevel)
DEFINE_GETSET_INT(FTNo, matchsToWin)
DEFINE_GETSET_INT(FirstAttackCount, firstAttackCount)
DEFINE_GETSET_INT(ConsecutiveWins, consecutiveWins)
DEFINE_GETSET_INT(WinTimeCount, winTimeCount)
DEFINE_GETSET_INT(WinPerfectCount, winPerfectCount)
DEFINE_GETSET_INT(WinSpecialCount, winSpecialCount)
DEFINE_GETSET_INT(WinPerfectSpecialCount, winPerfectSpecialCount)
DEFINE_GETSET_INT(WinHyperCount, winHyperCount)
DEFINE_GETSET_INT(WinPerfectHyperCount, winPerfectHyperCount)
DEFINE_GETSET_INT(WinThrowCount, winThrowCount)
DEFINE_GETSET_INT(WinPerfectThrowCount, winPerfectThrowCount)
DEFINE_GETSET_INT(LifePersistence, lifePersistence)
DEFINE_GETSET_INT(PowerPersistence, powerPersistence)
DEFINE_GETSET_INT(TimePersistence, timePersistence)
DEFINE_GETSET_INT(PlayerLife, playerLife)
DEFINE_GETSET_INT(PlayerPower, playerPower)
DEFINE_GETSET_INT(PlayerAttack, playerAttack)
DEFINE_GETSET_INT(PlayerDefence, playerDefence)
DEFINE_GETSET_INT(PlayerReward, playerReward)
DEFINE_GETSET_INT(AbyssDepth, abyssDepth)
DEFINE_GETSET_INT(AbyssDepthBoss, abyssDepthBoss)
DEFINE_GETSET_INT(AbyssDepthBossSpecial, abyssDepthBossSpecial)
DEFINE_GETSET_INT(AbyssBossFight, abyssBossFight)
DEFINE_GETSET_INT(AbyssFinalDepth, abyssFinalDepth)

// Simple bool getter/setters
DEFINE_GETSET_BOOL(P1winsDisplay, p1winsDisplay)
DEFINE_GETSET_BOOL(P2winsDisplay, p2winsDisplay)
DEFINE_GETSET_BOOL(TimerDisplay, timerDisplay)
DEFINE_GETSET_BOOL(CountdownDisplay, countdownDisplay)
DEFINE_GETSET_BOOL(ScoreDisplay, scoreDisplay)
DEFINE_GETSET_BOOL(MatchnoDisplay, matchnoDisplay)
DEFINE_GETSET_BOOL(AilevelDisplay, ailevelDisplay)
DEFINE_GETSET_BOOL(GameModeDisplay, gamemodeDisplay)
DEFINE_GETSET_BOOL(RewardDisplay, rewardDisplay)
DEFINE_GETSET_BOOL(PersistLife, persistLife)
DEFINE_GETSET_BOOL(PersistPower, persistPower)
DEFINE_GETSET_BOOL(PersistRoundTime, persistRoundtime)

// Simple string getter/setters
DEFINE_GETSET_STR(TourneyState, tourneyState)
DEFINE_GETSET_STR(AbyssSP1, abyssSP1)
DEFINE_GETSET_STR(AbyssSP2, abyssSP2)
DEFINE_GETSET_STR(AbyssSP3, abyssSP3)
DEFINE_GETSET_STR(AbyssSP4, abyssSP4)

// Write-only string setters
DEFINE_SET_STR(P1winsFormatted, p1winsFormatted)
DEFINE_SET_STR(P2winsFormatted, p2winsFormatted)
DEFINE_SET_STR(TimerFormatted, timerFormatted)
DEFINE_SET_STR(CountdownFormatted, countdownFormatted)
DEFINE_SET_STR(MatchInfo, matchnoInfo)
DEFINE_SET_STR(RewardFormatted, rewardFormatted)

// Special single-purpose functions (set-only, get-only, or display read)
// Timer/Countdown — separate set + read with different field access
DEFINE_SET_INT(Timer, timer)
DEFINE_GET_INT(TimerTotal, timer)

DEFINE_SET_INT(Countdown, countdownTimer)
DEFINE_GET_INT(Countdown, countdownTimer)

// Read-only round wins
DEFINE_GET_INT(P1RoundsWon, p1wins)
DEFINE_GET_INT(P2RoundsWon, p2wins)

#undef DEFINE_GETSET_INT
#undef DEFINE_GETSET_BOOL
#undef DEFINE_GETSET_STR
#undef DEFINE_SET_INT
#undef DEFINE_GET_INT
#undef DEFINE_SET_BOOL
#undef DEFINE_GET_BOOL
#undef DEFINE_SET_STR

// ── Match control functions ──

static int lua_exitMatch(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "script::exitMatch");
	(void)L;
	common_get_state().exitMatch = true;
	return 0;
}

static int lua_setSharedString(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "script::setSharedString");
	std::string str = lua_get_string(L, 1, "");
	if (str == "end") {
		sdlevent_get_state().end = true;
	}
	// "reload" case deferred — needs char_service
	return 0;
}

// =========================================================================
// Registration helper
// =========================================================================

// Helper: register a single Lua function.
static void reg(lua_State* L, const char* name, lua_CFunction fn) {
	lua_register(L, name, fn);
}

// Helper: register a pair of set/get functions.
static void reg_pair(lua_State* L,
	const char* set_name, lua_CFunction set_fn,
	const char* get_name, lua_CFunction get_fn) {
	reg(L, set_name, set_fn);
	reg(L, get_name, get_fn);
}

// =========================================================================
// script_init — Registers all Lua-callable functions
// =========================================================================

void script_init(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "script_init (full)");
	if (!L) {
		SSZ_TRACE_CAT(TRACE_SYS, "script_init: no Lua state, deferring registration");
		return;
	}

	g_lua_state = L;

	// ── Create metatables for userdata types ──
	create_metatable(L, MT_SFF, gc_sff);
	create_metatable(L, MT_SND, gc_snd);
	create_metatable(L, MT_FONT, gc_font);
	create_metatable(L, MT_CMD, gc_cmd);
	// InputDialog uses placement new — needs explicit destructor
	luaL_newmetatable(L, MT_INPUT_DLG);
	lua_pushstring(L, "__gc");
	lua_pushcfunction(L, gc_input_dlg);
	lua_settable(L, -3);
	lua_pop(L, 1);

	// ── Argument parsing ──
	reg(L, "numArg", lua_numArg);
	reg(L, "strArg", lua_strArg);
	reg(L, "blArg", lua_blArg);
	reg(L, "refArg", lua_refArg);

	// ── Resource loaders ──
	reg(L, "sffNew", lua_sffNew);
	reg(L, "sndNew", lua_sndNew);
	reg(L, "sndPlay", lua_sndPlay);
	reg(L, "sndStop", lua_sndStop);
	reg(L, "fontNew", lua_fontNew);

	// ── Command wrappers ──
	reg(L, "modKeyState", lua_modKeyState);
	reg(L, "commandNew", lua_commandNew);
	reg(L, "commandAdd", lua_commandAdd);
	reg(L, "commandGetState", lua_commandGetState);
	reg(L, "commandInput", lua_commandInput);
	reg(L, "commandBufReset", lua_commandBufReset);

	// ── System controls ──
	reg(L, "startButton", lua_startButton);
	reg(L, "getSysCtrl", lua_getSysCtrl);
	reg(L, "setSysCtrl", lua_setSysCtrl);

	// ── Input dialog ──
	reg(L, "inputDialogNew", lua_inputDialogNew);
	reg(L, "inputDialogPopup", lua_inputDialogPopup);
	reg(L, "inputDialogIsDone", lua_inputDialogIsDone);
	reg(L, "inputDialogGetStr", lua_inputDialogGetStr);

	// ── Text input ──
	reg(L, "inputText", lua_inputText);
	reg(L, "setInputText", lua_setInputText);
	reg(L, "clearInputText", lua_clearInputText);

	// ── Clipboard ──
	reg(L, "clipboardPaste", lua_clipboardPaste);
	reg(L, "getClipboardText", lua_getClipboardText);

	// ── Rendering ──
	reg(L, "drawTTF", lua_drawTTF);

	// ── Video/BGM ──
	reg(L, "loadVideo", lua_loadVideo);
	reg(L, "playBGM", lua_playBGM);
	reg(L, "getBGM", lua_getBGM);
	reg(L, "fadeInBGM", lua_fadeInBGM);
	reg(L, "fadeOutBGM", lua_fadeOutBGM);

	// ── File opening ──
	reg(L, "sszOpen", lua_sszOpen);
	reg(L, "batOpen", lua_batOpen);
	reg(L, "webOpen", lua_webOpen);

	// ── Key state ──
	reg(L, "esc", lua_esc);
	reg(L, "upKey", lua_upKey);
	reg(L, "downKey", lua_downKey);
	reg(L, "leftKey", lua_leftKey);
	reg(L, "rightKey", lua_rightKey);
	reg(L, "aKey", lua_aKey);
	reg(L, "bKey", lua_bKey);
	reg(L, "cKey", lua_cKey);
	reg(L, "dKey", lua_dKey);
	reg(L, "eKey", lua_eKey);
	reg(L, "fKey", lua_fKey);
	reg(L, "gKey", lua_gKey);
	reg(L, "hKey", lua_hKey);
	reg(L, "iKey", lua_iKey);
	reg(L, "jKey", lua_jKey);
	reg(L, "kKey", lua_kKey);
	reg(L, "lKey", lua_lKey);
	reg(L, "mKey", lua_mKey);
	reg(L, "nKey", lua_nKey);
	reg(L, "oKey", lua_oKey);
	reg(L, "pKey", lua_pKey);
	reg(L, "qKey", lua_qKey);
	reg(L, "rKey", lua_rKey);
	reg(L, "sKey", lua_sKey);
	reg(L, "tKey", lua_tKey);
	reg(L, "uKey", lua_uKey);
	reg(L, "vKey", lua_vKey);
	reg(L, "wKey", lua_wKey);
	reg(L, "xKey", lua_xKey);
	reg(L, "yKey", lua_yKey);
	reg(L, "zKey", lua_zKey);
	reg(L, "kzeroKey", lua_kzeroKey);
	reg(L, "koneKey", lua_koneKey);
	reg(L, "ktwoKey", lua_ktwoKey);
	reg(L, "kthreeKey", lua_kthreeKey);
	reg(L, "kfourKey", lua_kfourKey);
	reg(L, "kfiveKey", lua_kfiveKey);
	reg(L, "ksixKey", lua_ksixKey);
	reg(L, "ksevenKey", lua_ksevenKey);
	reg(L, "keightKey", lua_keightKey);
	reg(L, "knineKey", lua_knineKey);
	reg(L, "zeroKey", lua_zeroKey);
	reg(L, "oneKey", lua_oneKey);
	reg(L, "twoKey", lua_twoKey);
	reg(L, "threeKey", lua_threeKey);
	reg(L, "fourKey", lua_fourKey);
	reg(L, "fiveKey", lua_fiveKey);
	reg(L, "sixKey", lua_sixKey);
	reg(L, "sevenKey", lua_sevenKey);
	reg(L, "eightKey", lua_eightKey);
	reg(L, "nineKey", lua_nineKey);
	reg(L, "returnKey", lua_returnKey);
	reg(L, "backspaceKey", lua_backspaceKey);
	reg(L, "spaceKey", lua_spaceKey);
	reg(L, "lshiftKey", lua_lshiftKey);
	reg(L, "rshiftKey", lua_rshiftKey);
	reg(L, "tabKey", lua_tabKey);
	reg(L, "minusKey", lua_minusKey);
	reg(L, "equalsKey", lua_equalsKey);
	reg(L, "leftbracketKey", lua_leftbracketKey);
	reg(L, "rightbracketKey", lua_rightbracketKey);
	reg(L, "backslashKey", lua_backslashKey);
	reg(L, "semicolonKey", lua_semicolonKey);
	reg(L, "commaKey", lua_commaKey);
	reg(L, "periodKey", lua_periodKey);
	reg(L, "slashKey", lua_slashKey);
	reg(L, "f1Key", lua_f1Key);
	reg(L, "f2Key", lua_f2Key);
	reg(L, "f3Key", lua_f3Key);
	reg(L, "f4Key", lua_f4Key);
	reg(L, "f5Key", lua_f5Key);
	reg(L, "f9Key", lua_f9Key);
	reg(L, "f10Key", lua_f10Key);
	reg(L, "f11Key", lua_f11Key);
	reg(L, "f12Key", lua_f12Key);
	reg(L, "printscreenKey", lua_printscreenKey);
	reg(L, "insertKey", lua_insertKey);
	reg(L, "homeKey", lua_homeKey);
	reg(L, "pageupKey", lua_pageupKey);
	reg(L, "deleteKey", lua_deleteKey);
	reg(L, "endKey", lua_endKey);
	reg(L, "pagedownKey", lua_pagedownKey);
	reg(L, "kdivideKey", lua_kdivideKey);
	reg(L, "kmultiplyKey", lua_kmultiplyKey);
	reg(L, "kminusKey", lua_kminusKey);
	reg(L, "kplusKey", lua_kplusKey);
	reg(L, "kenterKey", lua_kenterKey);
	reg(L, "kperiodKey", lua_kperiodKey);
	reg(L, "getGamepadKeyA", lua_getGamepadKeyA);
	reg(L, "getGamepadKeyB", lua_getGamepadKeyB);
	reg(L, "getGamepadKeyC", lua_getGamepadKeyC);

	// ── Random ──
	reg(L, "sszRandom", lua_sszRandom);

	// ── Common data getters/setters ──

	// Special compound functions
	reg(L, "setAutoguard", lua_setAutoguard);
	reg(L, "setPowerShare", lua_setPowerShare);
	reg(L, "setHomeTeam", lua_setHomeTeam);

	// Credits
	reg_pair(L, "setCredits", lua_setCredits, "getCredits", lua_getCredits);

	// Coins
	reg_pair(L, "setCoins", lua_setCoins, "getCoins", lua_getCoins);

	// Round time
	reg_pair(L, "setRoundTime", lua_setRoundTime, "getRoundTime", lua_getRoundTime);

	// Rounds to win
	reg_pair(L, "setRoundsToWin", lua_setRoundsToWin, "getRoundsToWin", lua_getRoundsToWin);

	// P1/P2 rounds won
	reg(L, "p1RoundsWon", lua_getP1RoundsWon);
	reg(L, "p2RoundsWon", lua_getP2RoundsWon);

	// Wins display
	reg(L, "setP1winsDisplay", lua_setP1winsDisplay);
	reg(L, "setP2winsDisplay", lua_setP2winsDisplay);
	reg(L, "p1winsDisplay", lua_getP1winsDisplay);
	reg(L, "p2winsDisplay", lua_getP2winsDisplay);

	// Wins formatted
	reg(L, "setP1winsFormatted", lua_setP1winsFormatted);
	reg(L, "setP2winsFormatted", lua_setP2winsFormatted);

	// Match wins
	reg_pair(L, "setP1matchWins", lua_setP1matchWins, "getP1matchWins", lua_getP1matchWins);
	reg_pair(L, "setP2matchWins", lua_setP2matchWins, "getP2matchWins", lua_getP2matchWins);

	// Timer
	reg(L, "setTimerFormatted", lua_setTimerFormatted);
	reg(L, "setTimerDisplay", lua_setTimerDisplay);
	reg(L, "timerDisplay", lua_getTimerDisplay);
	reg(L, "setTimer", lua_setTimer);
	reg(L, "timerTotal", lua_getTimerTotal);

	// Countdown
	reg(L, "setCountdownFormatted", lua_setCountdownFormatted);
	reg(L, "setCountdownDisplay", lua_setCountdownDisplay);
	reg(L, "countdownDisplay", lua_getCountdownDisplay);
	reg(L, "setCountdown", lua_setCountdown);
	reg(L, "countdown", lua_getCountdown);

	// Score
	reg(L, "setScoreDisplay", lua_setScoreDisplay);
	reg(L, "scoreDisplay", lua_getScoreDisplay);
	reg(L, "setScore", lua_setScore);
	reg(L, "score", lua_getScore);
	reg(L, "setScoreTotal", lua_setScoreTotal);
	reg(L, "scoreTotal", lua_getScoreTotal);
	reg(L, "setP1Score", lua_setP1Score);
	reg(L, "p1score", lua_getP1Score);
	reg(L, "setP2Score", lua_setP2Score);
	reg(L, "p2score", lua_getP2Score);

	// Match info
	reg(L, "setMatchInfo", lua_setMatchInfo);
	reg(L, "setMatchnoDisplay", lua_setMatchnoDisplay);
	reg(L, "matchnoDisplay", lua_getMatchnoDisplay);
	reg_pair(L, "setMatchNo", lua_setMatchNo, "getMatchNo", lua_getMatchNo);
	reg_pair(L, "setLastMatch", lua_setLastMatch, "getLastMatch", lua_getLastMatch);

	// AI level
	reg(L, "setAilevelDisplay", lua_setAilevelDisplay);
	reg(L, "ailevelDisplay", lua_getAilevelDisplay);
	reg(L, "setCPULevel", lua_setCPULevel);
	reg(L, "cpuLevel", lua_getCPULevel);

	// Game mode display
	reg(L, "setGameModeDisplay", lua_setGameModeDisplay);
	reg(L, "gamemodeDisplay", lua_getGameModeDisplay);

	// Reward
	reg(L, "setRewardFormatted", lua_setRewardFormatted);
	reg(L, "setRewardDisplay", lua_setRewardDisplay);
	reg(L, "rewardDisplay", lua_getRewardDisplay);

	// Tournament state
	reg_pair(L, "setTourneyState", lua_setTourneyState, "getTourneyState", lua_getTourneyState);

	// FT No
	reg_pair(L, "setFTNo", lua_setFTNo, "getFTNo", lua_getFTNo);

	// First attack
	reg(L, "setFirstAttackCount", lua_setFirstAttackCount);
	reg(L, "firstAttackCount", lua_getFirstAttackCount);

	// Consecutive wins
	reg(L, "setConsecutiveWins", lua_setConsecutiveWins);
	reg(L, "consecutiveWins", lua_getConsecutiveWins);

	// Win counters
	reg(L, "setWinTimeCount", lua_setWinTimeCount);
	reg(L, "winTimeCount", lua_getWinTimeCount);
	reg(L, "setWinPerfectCount", lua_setWinPerfectCount);
	reg(L, "winPerfectCount", lua_getWinPerfectCount);
	reg(L, "setWinSpecialCount", lua_setWinSpecialCount);
	reg(L, "winSpecialCount", lua_getWinSpecialCount);
	reg(L, "setWinPerfectSpecialCount", lua_setWinPerfectSpecialCount);
	reg(L, "winPerfectSpecialCount", lua_getWinPerfectSpecialCount);
	reg(L, "setWinHyperCount", lua_setWinHyperCount);
	reg(L, "winHyperCount", lua_getWinHyperCount);
	reg(L, "setWinPerfectHyperCount", lua_setWinPerfectHyperCount);
	reg(L, "winPerfectHyperCount", lua_getWinPerfectHyperCount);
	reg(L, "setWinThrowCount", lua_setWinThrowCount);
	reg(L, "winThrowCount", lua_getWinThrowCount);
	reg(L, "setWinPerfectThrowCount", lua_setWinPerfectThrowCount);
	reg(L, "winPerfectThrowCount", lua_getWinPerfectThrowCount);

	// Persistence toggles
	reg(L, "setPersistLife", lua_setPersistLife);
	reg(L, "persistLife", lua_getPersistLife);
	reg(L, "setLifePersistence", lua_setLifePersistence);
	reg(L, "getLifePersistence", lua_getLifePersistence);
	reg(L, "setPersistPower", lua_setPersistPower);
	reg(L, "persistPower", lua_getPersistPower);
	reg(L, "setPowerPersistence", lua_setPowerPersistence);
	reg(L, "getPowerPersistence", lua_getPowerPersistence);
	reg(L, "setPersistRoundTime", lua_setPersistRoundTime);
	reg(L, "persistRoundtime", lua_getPersistRoundTime);
	reg(L, "setTimePersistence", lua_setTimePersistence);
	reg(L, "getTimePersistence", lua_getTimePersistence);

	// Player stats
	reg_pair(L, "setPlayerLife", lua_setPlayerLife, "getPlayerLife", lua_getPlayerLife);
	reg_pair(L, "setPlayerPower", lua_setPlayerPower, "getPlayerPower", lua_getPlayerPower);
	reg_pair(L, "setPlayerAttack", lua_setPlayerAttack, "getPlayerAttack", lua_getPlayerAttack);
	reg_pair(L, "setPlayerDefence", lua_setPlayerDefence, "getPlayerDefence", lua_getPlayerDefence);
	reg_pair(L, "setPlayerReward", lua_setPlayerReward, "getPlayerReward", lua_getPlayerReward);

	// Abyss
	reg_pair(L, "setAbyssDepth", lua_setAbyssDepth, "getAbyssDepth", lua_getAbyssDepth);
	reg_pair(L, "setAbyssDepthBoss", lua_setAbyssDepthBoss, "getAbyssDepthBoss", lua_getAbyssDepthBoss);
	reg_pair(L, "setAbyssDepthBossSpecial", lua_setAbyssDepthBossSpecial, "getAbyssDepthBossSpecial", lua_getAbyssDepthBossSpecial);
	reg_pair(L, "setAbyssBossFight", lua_setAbyssBossFight, "getAbyssBossFight", lua_getAbyssBossFight);
	reg_pair(L, "setAbyssFinalDepth", lua_setAbyssFinalDepth, "getAbyssFinalDepth", lua_getAbyssFinalDepth);

	// Abyss string fields
	reg_pair(L, "setAbyssSP1", lua_setAbyssSP1, "getAbyssSP1", lua_getAbyssSP1);
	reg_pair(L, "setAbyssSP2", lua_setAbyssSP2, "getAbyssSP2", lua_getAbyssSP2);
	reg_pair(L, "setAbyssSP3", lua_setAbyssSP3, "getAbyssSP3", lua_getAbyssSP3);
	reg_pair(L, "setAbyssSP4", lua_setAbyssSP4, "getAbyssSP4", lua_getAbyssSP4);

	// Match control
	reg(L, "exitMatch", lua_exitMatch);
	reg(L, "setSharedString", lua_setSharedString);

	SSZ_TRACE_CAT(TRACE_SYS, "script_init: registered 190+ Lua callbacks");
}

void script_init() {
	SSZ_TRACE_CAT(TRACE_SYS, "script_init (no-arg)");
	if (g_lua_state) {
		script_init(g_lua_state);
	} else {
		SSZ_TRACE_CAT(TRACE_SYS, "script_init: no Lua state stored, skipping registration");
	}
}

} // namespace ikemen::ssz_native
