// system_service.hpp — Native C++ scaffolding for ssz_script/ssz/system.ssz
//
// system.ssz implements the character/stage select screen logic:
//   &Select       — character and stage list management, portrait loading
//   &SelectInfo   — player selection state, random character assignment
//   &System       — top-level wrapper with fight pointer and select info
//
// Design note: SelectData, SelectInfoData, and SystemData live directly in
// ikemen::ssz_native (like ShareData in share_service.hpp) as DTO structs.
// The free functions (select_*, select_info_*, system_*) use matching prefix
// naming rather than a nested namespace for consistency with share_service.
//
// Phase 3: All function bodies are stubs.  They will be wired as each
// dependent SSZ module (file, sff, math, string, alert, com, action, fight,
// common) is converted.
//
// Phase 3 note (M40): This file is header-only.  When stub methods exceed ~5
// lines of real logic, create system_service.cpp and move them out of the
// header to avoid inline bloat and ODR surprises.

#pragma once

#include <string>
#include <vector>
#include <cctype>
#include "common_service.hpp"

namespace ikemen::ssz_native {

// ── SelectChar ──
// Maps to &.Select::Char.  Holds a single selectable character's metadata
// and loaded portraits (SFF sprites).
struct SelectCharData {
	std::string def;              // ^/char — .def path
	std::string name;             // ^/char — display name
	std::string sprite;           // ^/char — sprite file path
	std::string anim;             // ^/char — anim file path
	// Portrait sprites: opaque &.sff.Sprite references (wired when sff module converts)
	// facePortrait, bigPortrait, orderPortrait, vsPortrait
	// winPortrait, loserPortrait, resultPortrait, extraPortrait
};

// ── SelectStage ──
// Maps to &.Select::Stage.  Holds a single selectable stage's metadata and
// loaded portraits.
struct SelectStageData {
	std::string def;              // ^/char — .def path
	std::string name;             // ^/char — display name
	std::string sprite;           // ^/char — sprite file path
	std::string anim;             // ^/char — anim file path
	// Portrait sprites: opaque &.sff.Sprite references
	// iconStagePortrait, bigStagePortrait, vsStagePortrait
	// winStagePortrait, extraStagePortrait
};

// ── SelectData ──
// Maps to &.Select.  Manages character/stage lists and config-driven display.
struct SelectData {
	std::vector<SelectCharData> charlist;   // %&Char
	std::vector<SelectStageData> stagelist; // %&Stage

	// Layout
	int columns{5};
	int rows{2};
	float cellsizex{29.0f};
	float cellsizey{29.0f};
	float cellscalex{1.0f};
	float cellscaley{1.0f};

	// Portrait group/index config (read from cfg at select screen init)
	int faceGroup{}, faceIndex{};
	int bigGroup{}, bigIndex{};
	int winGroup{}, winIndex{};
	int loserGroup{}, loserIndex{};
	int orderGroup{}, orderIndex{};
	int vsGroup{}, vsIndex{};
	int resultGroup{}, resultIndex{};
	int extraGroup{}, extraIndex{};
	int iconStageGroup{}, iconStageIndex{};
	int bigStageGroup{}, bigStageIndex{};
	int vsStageGroup{}, vsStageIndex{};
	int winStageGroup{}, winStageIndex{};
	int extraStageGroup{}, extraStageIndex{};

	// Random sprite config
	// randomspr: ^&.sff.Sprite (opaque)
	float randxscl{1.0f};
	float randyscl{1.0f};

	int curStageNo{};
	int selectedStageNo{-1};

	// Methods — stubs
	int getCharNo(int i) const;
	const SelectCharData* getChar(int i) const;
	int getStageNo(int i) const;
	const SelectStageData* getStage(int i) const;
	int setStageNo(int i);
	void selectStage(int no);
	std::string getStageName(int i) const;
	bool addChar(const std::string& def);
	std::string addStage(const std::string& def);
};

// ── SelectInfoData ──
// Maps to &.SelectInfo.  Tracks player selection state.
struct SelectInfoData {
	struct Selected {
		int i{};
		int pal{};
	};
	struct Player {
		std::vector<Selected> selchr;
		void reset();
	};
	std::vector<Player> p;  // [0]=P1, [1]=P2
	SelectData* sel{};

	// Methods — stubs
	bool addSelchr(int pn, int cn, int pl);
	void reset();
};

// ── SystemData ──
// Maps to &.System.  Top-level select screen wrapper.
struct SystemData {
	// fig: ^&.fgt.Fight (opaque — wired when fight module converts)
	SelectInfoData selinf;

	// Methods — stubs
	void selReset();
};

// ── Free-function API ──

// No-arg convenience functions for bridge/SSZ ABI.
// These operate on an internal static SystemData instance.
void system_add_char();
void system_add_stage();
int  system_set_stage_no(int i);
void system_select_stage(int no);
bool system_add_selchr(int pn, int cn, int pl);
void system_sel_reset();

// Parameterized versions — receive the def file path from the bridge.
bool system_add_char(const std::string& defPath);
std::string system_add_stage(const std::string& defPath);
std::string system_get_stage_name(int i);

// Access the selected stage def path from internal state.
std::string system_get_selected_stage_def();
int system_get_selected_stage_no();

// Get the def path for a selected character by player slot.
std::string system_get_selected_char_def(int pn);

// select_* — methods on SelectData
inline int SelectData::getCharNo(int i) const {
	int n = static_cast<int>(charlist.size());
	if (n > 0) {
		i %= n;
		if (i < 0) i += n;
	}
	return i;
}
inline const SelectCharData* SelectData::getChar(int i) const {
	int n = getCharNo(i);
	if (n >= 0 && n < static_cast<int>(charlist.size()))
		return &charlist[n];
	return nullptr;
}
inline int SelectData::getStageNo(int i) const {
	int n = static_cast<int>(stagelist.size());
	if (n > 0) {
		i %= n;
		if (i < 0) i += n;
	}
	return i;
}
inline const SelectStageData* SelectData::getStage(int i) const {
	int n = getStageNo(i);
	if (n >= 0 && n < static_cast<int>(stagelist.size()))
		return &stagelist[n];
	return nullptr;
}
inline int SelectData::setStageNo(int i) {
	int n = static_cast<int>(stagelist.size());
	curStageNo = i % (n + 1);
	if (curStageNo < 0) curStageNo += n + 1;
	return curStageNo;
}
inline void SelectData::selectStage(int no) { selectedStageNo = no; }
inline std::string SelectData::getStageName(int i) const {
	int n = static_cast<int>(stagelist.size());
	if (n == 0) return {};
	i %= n + 1;
	if (i < 0) i += n + 1;
	if (i == 0) return "RANDOM";
	return stagelist[i - 1].name;
}
inline bool SelectData::addChar(const std::string& defPath) {
	if (defPath.empty()) return false;
	// Parse .def file to extract character info
	SelectCharData ch;
	ch.def = defPath;
	ch.name = defPath; // fallback — real name from [Info] section
	
	// Try to read the .def file for name
	std::string buf = common_load_text(defPath, false);
	if (!buf.empty()) {
		auto lines = common_split_lines(buf);
		bool inInfo = false;
		for (const auto& line : lines) {
			if (line.empty()) continue;
			if (line[0] == '[') {
				std::string sec = line;
				for (auto& c : sec) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
				inInfo = (sec.find("[info]") != std::string::npos);
				continue;
			}
			if (inInfo) {
				size_t eq = line.find('=');
				if (eq != std::string::npos) {
					std::string key = line.substr(0, eq);
					// trim key
					{
						size_t s = key.find_first_not_of(" \t");
						if (s != std::string::npos) key = key.substr(s);
						size_t e = key.find_last_not_of(" \t");
						if (e != std::string::npos) key = key.substr(0, e + 1);
					}
					if (key == "name" || key == "displayname") {
						ch.name = line.substr(eq + 1);
						// trim value
						{
							size_t s = ch.name.find_first_not_of(" \t");
							if (s != std::string::npos) ch.name = ch.name.substr(s);
							size_t e = ch.name.find_last_not_of(" \t\r\n");
							if (e != std::string::npos) ch.name = ch.name.substr(0, e + 1);
						}
					} else if (key == "sprite") {
						ch.sprite = line.substr(eq + 1);
						{
							size_t s = ch.sprite.find_first_not_of(" \t");
							if (s != std::string::npos) ch.sprite = ch.sprite.substr(s);
							size_t e = ch.sprite.find_last_not_of(" \t\r\n");
							if (e != std::string::npos) ch.sprite = ch.sprite.substr(0, e + 1);
						}
					} else if (key == "anim") {
						ch.anim = line.substr(eq + 1);
						{
							size_t s = ch.anim.find_first_not_of(" \t");
							if (s != std::string::npos) ch.anim = ch.anim.substr(s);
							size_t e = ch.anim.find_last_not_of(" \t\r\n");
							if (e != std::string::npos) ch.anim = ch.anim.substr(0, e + 1);
						}
					}
				}
			}
		}
	}
	charlist.push_back(std::move(ch));
	return true;
}
inline std::string SelectData::addStage(const std::string& defPath) {
	if (defPath.empty()) return {};
	SelectStageData st;
	st.def = defPath;
	st.name = defPath;
	// Parse .def for stage name
	std::string buf = common_load_text(defPath, false);
	if (!buf.empty()) {
		auto lines = common_split_lines(buf);
		bool inInfo = false;
		for (const auto& line : lines) {
			if (line.empty()) continue;
			if (line[0] == '[') {
				std::string sec = line;
				for (auto& c : sec) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
				inInfo = (sec.find("[info]") != std::string::npos);
				continue;
			}
			if (inInfo) {
				size_t eq = line.find('=');
				if (eq != std::string::npos) {
					std::string key = line.substr(0, eq);
	                {
						size_t s = key.find_first_not_of(" \t");
						if (s != std::string::npos) key = key.substr(s);
						size_t e = key.find_last_not_of(" \t");
						if (e != std::string::npos) key = key.substr(0, e + 1);
					}
					if (key == "name" || key == "displayname") {
						st.name = line.substr(eq + 1);
						{
							size_t s = st.name.find_first_not_of(" \t");
							if (s != std::string::npos) st.name = st.name.substr(s);
							size_t e = st.name.find_last_not_of(" \t\r\n");
							if (e != std::string::npos) st.name = st.name.substr(0, e + 1);
						}
					}
				}
			}
		}
	}
	stagelist.push_back(std::move(st));
	return stagelist.back().name;
}

// select_info_* — methods on SelectInfoData
inline void SelectInfoData::Player::reset() { selchr.clear(); }
inline bool SelectInfoData::addSelchr(int pn, int cn, int pl) {
	if (pn < 0 || pn >= static_cast<int>(p.size())) return false;
	if (!sel) return false;
	int n = sel->getCharNo(cn);
	if (n < 0 || n >= static_cast<int>(sel->charlist.size())) return false;
	if (sel->charlist[n].def.empty()) return false;
	p[pn].selchr.push_back({n, pl});
	return true;
}
inline void SelectInfoData::reset() {
	for (auto& pl : p) pl.reset();
	if (sel) sel->selectedStageNo = -1;
}

// system_* — methods on SystemData
inline void SystemData::selReset() { selinf.reset(); }

} // namespace ikemen::ssz_native
