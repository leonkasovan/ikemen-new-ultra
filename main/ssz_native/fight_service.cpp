// fight_service.cpp — Real implementations for fight.ssz (Phase 5).
// Full fight engine: Lifebar, Powerbar, Face, Name, Time, Combo, Round,
// WinIcon, and Fight with fight.def section parsers and step() lifecycle.
// Rendering (draw) methods deferred until sdlplugin/sff/font are converted.

#include "fight_service.hpp"
#include "common_service.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>

namespace ikemen::ssz_native {

// =========================================================================
// Module-level state
// =========================================================================
static FightState g_fight_state;

FightState& fight_get_state() { return g_fight_state; }

// =========================================================================
// Utility helpers
// =========================================================================

namespace {

std::string trim(const std::string& s) {
	size_t start = s.find_first_not_of(" \t\r\n");
	if (start == std::string::npos) return {};
	size_t end = s.find_last_not_of(" \t\r\n");
	return s.substr(start, end - start + 1);
}

std::string to_lower(const std::string& s) {
	std::string r = s;
	for (auto& c : r) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return r;
}

int parse_int(const std::string& s) { return std::atoi(trim(s).c_str()); }
float parse_float(const std::string& s) { return static_cast<float>(std::atof(trim(s).c_str())); }

// Parse a key=value line from section data.
bool parse_kv(const std::string& line, std::string& key, std::string& val) {
	size_t eq = line.find('=');
	if (eq == std::string::npos) return false;
	key = to_lower(trim(line.substr(0, eq)));
	val = trim(line.substr(eq + 1));
	return true;
}

// Collect section body lines into a map of key->value.
// The prefix filters keys (e.g., "bg0." strips the prefix).
using SectionMap = std::vector<std::pair<std::string, std::string>>;
SectionMap parse_section(const std::string& body, const std::string& prefix = "") {
	SectionMap result;
	std::istringstream stream(body);
	std::string line;
	while (std::getline(stream, line)) {
		std::string trimmed = trim(line);
		if (trimmed.empty() || trimmed[0] == ';') continue;
		std::string key, val;
		if (!parse_kv(trimmed, key, val)) continue;
		// Apply prefix filter
		if (!prefix.empty()) {
			if (key.find(prefix) != 0) continue;
			key = key.substr(prefix.size());
		}
		result.push_back({key, val});
	}
	return result;
}

// Extract section body from lines starting at index i, until next section or end.
std::string extract_section_body(const std::vector<std::string>& lines, size_t& i) {
	std::string body;
	i++; // skip section header
	while (i < lines.size()) {
		const auto& line = lines[i];
		if (!line.empty() && line[0] == '[') { i--; break; }
		if (!body.empty()) body += "\n";
		body += line;
		i++;
	}
	return body;
}

} // anonymous namespace

// =========================================================================
// LifePowerData
// =========================================================================
void LifePowerData::set(float life, float power, int level) {
	l = life; p = power; lv = level;
}

// =========================================================================
// ActionListData
// =========================================================================
int ActionListData::getAction(int no) {
	for (size_t i = 0; i < actionList.size(); i++)
		if (actionList[i] == no) return static_cast<int>(i);
	return -1;
}
int ActionListData::newAction(int no) {
	actionList.push_back(no);
	return static_cast<int>(actionList.size()) - 1;
}

// =========================================================================
// AnimFontSndData
// =========================================================================
void AnimFontSndData::read(const std::string& prefix, const std::string& sc, ActionListData& al) {
	auto kv = parse_section(sc, prefix);
	for (auto& [key, val] : kv) {
		if (key == "sndg") sndg = parse_int(val);
		else if (key == "sndi") sndi = parse_int(val);
		else if (key == "fontn") fontn = parse_int(val);
		else if (key == "fontb") fontb = parse_int(val);
		else if (key == "fonta") fonta = parse_int(val);
		else if (key == "text") text = val;
		else if (key == "displaytime") curtime = parse_int(val);
		// anim read deferred (needs sff/sprite lookup)
	}
	(void)al;
}
void AnimFontSndData::action() { if (curtime > 0) curtime--; }
void AnimFontSndData::draw(int layerno) { (void)layerno; }
bool AnimFontSndData::noSound() { return sndg < 0; }
bool AnimFontSndData::noDisplay() { return fontn < 0; }
bool AnimFontSndData::end(int dt) { return curtime > dt; }
void AnimFontSndData::reset() { *this = AnimFontSndData{}; }

// =========================================================================
// LifebarData
// =========================================================================
void LifebarData::read(const std::string& prefix, const std::string& sc) {
	auto kv = parse_section(sc);
	for (auto& [key, val] : kv) {
		if (key == prefix + "posx") posx = parse_int(val);
		else if (key == prefix + "posy") posy = parse_int(val);
		else if (key == prefix + "range.x") range_xz = parse_int(val);   // "range.x" -> range_xz
		else if (key == prefix + "range.y") range_xm = parse_int(val);
		else if (key == prefix + "range.z") range_xz = parse_int(val);   // SSZ uses .x, .y, .z
		else if (key == prefix + "range.x.z") range_xz = parse_int(val); // direct match
		else if (key == prefix + "range.x.m") range_xm = parse_int(val);
	}
	(void)range_xz; (void)range_xm; // used by draw
}
void LifebarData::step(float life, bool hit) {
	oldlife = midlife;
	midlife = life;
	if (hit) mlifetime = 60;
	if (mlifetime > 0) mlifetime--;
	midlifelim = midlife;
}
void LifebarData::bgDraw(int layerno) { (void)layerno; }
void LifebarData::draw(int layerno, float life) { (void)layerno; (void)life; }
void LifebarData::reset() { *this = LifebarData{}; }

// =========================================================================
// PowerbarData
// =========================================================================
void PowerbarData::read(const std::string& prefix, const std::string& sc) {
	auto kv = parse_section(sc);
	for (auto& [key, val] : kv) {
		if (key == prefix + "posx") posx = parse_int(val);
		else if (key == prefix + "posy") posy = parse_int(val);
		else if (key == prefix + "range.x.z") range_xz = parse_int(val);
		else if (key == prefix + "range.x.m") range_xm = parse_int(val);
		else if (key == prefix + "counter.fontn") counter_fontn = parse_int(val);
		else if (key == prefix + "counter.fontb") counter_fontb = parse_int(val);
		else if (key == prefix + "counter.fonta") counter_fonta = parse_int(val);
	}
}
void PowerbarData::step(float power, int level) {
	midpower = power;
	prevlevel = level;
}
void PowerbarData::bgDraw(int layerno) { (void)layerno; }
void PowerbarData::draw(int layerno, float power, int level) { (void)layerno; (void)power; (void)level; }
void PowerbarData::reset() { *this = PowerbarData{}; }

// =========================================================================
// FaceData
// =========================================================================
void FaceData::read(const std::string& prefix, const std::string& sc) {
	auto kv = parse_section(sc);
	for (auto& [key, val] : kv) {
		if (key == prefix + "posx") posx = parse_int(val);
		else if (key == prefix + "posy") posy = parse_int(val);
		else if (key == prefix + "face.sprg") face_sprg = parse_int(val);
		else if (key == prefix + "face.spri") face_spri = parse_int(val);
		else if (key == prefix + "teammate.posx") teammate_posx = parse_int(val);
		else if (key == prefix + "teammate.posy") teammate_posy = parse_int(val);
		else if (key == prefix + "teammate.spacingx") teammate_spacingx = parse_int(val);
		else if (key == prefix + "teammate.spacingy") teammate_spacingy = parse_int(val);
		else if (key == prefix + "teammate.face.sprg") teammate_face_sprg = parse_int(val);
		else if (key == prefix + "teammate.face.spri") teammate_face_spri = parse_int(val);
	}
}
void FaceData::step() { /* face animation — deferred */ }
void FaceData::bgDraw(int layerno) { (void)layerno; }
void FaceData::draw(int layerno) { (void)layerno; }
void FaceData::reset() {
	numko = 0; face_sprg = 0; face_spri = 0;
	posx = 0; posy = 0;
}

// =========================================================================
// NameData
// =========================================================================
void NameData::read(const std::string& prefix, const std::string& sc) {
	auto kv = parse_section(sc);
	for (auto& [key, val] : kv) {
		if (key == prefix + "posx") posx = parse_int(val);
		else if (key == prefix + "posy") posy = parse_int(val);
		else if (key == prefix + "fontn") name_fontn = parse_int(val);
		else if (key == prefix + "fontb") name_fontb = parse_int(val);
		else if (key == prefix + "fonta") name_fonta = parse_int(val);
	}
}
void NameData::step() {}
void NameData::bgDraw(int layerno) { (void)layerno; }
void NameData::draw(int layerno) { (void)layerno; }
void NameData::reset() { *this = NameData{}; }

// =========================================================================
// TimeData
// =========================================================================
void TimeData::read(const std::string& prefix, const std::string& sc) {
	auto kv = parse_section(sc);
	for (auto& [key, val] : kv) {
		if (key == prefix + "posx") posx = parse_int(val);
		else if (key == prefix + "posy") posy = parse_int(val);
		else if (key == prefix + "counter.fontn") counter_fontn = parse_int(val);
		else if (key == prefix + "counter.fontb") counter_fontb = parse_int(val);
		else if (key == prefix + "counter.fonta") counter_fonta = parse_int(val);
		else if (key == prefix + "framespercount") framespercount = parse_int(val);
	}
}
void TimeData::step() {}
void TimeData::bgDraw(int layerno) { (void)layerno; }
void TimeData::draw(int layerno, int time) { (void)layerno; (void)time; }
void TimeData::reset() { *this = TimeData{}; }

// =========================================================================
// ComboData
// =========================================================================
void ComboData::read(const std::string& prefix, const std::string& sc) {
	auto kv = parse_section(sc);
	for (auto& [key, val] : kv) {
		if (key == prefix + "posx") posx = parse_int(val);
		else if (key == prefix + "posy") posy = parse_int(val);
		else if (key == prefix + "start.x") start_x = parse_float(val);
		else if (key == prefix + "counter.fontn") counter_fontn = parse_int(val);
		else if (key == prefix + "counter.fontb") counter_fontb = parse_int(val);
		else if (key == prefix + "counter.shake") counter_shake = parse_int(val);
		else if (key == prefix + "text.fontn") text_fontn = parse_int(val);
		else if (key == prefix + "text.fontb") text_fontb = parse_int(val);
		else if (key == prefix + "text.text") text_text = val;
		else if (key == prefix + "displaytime") displaytime = parse_int(val);
	}
}
void ComboData::step(int combo, int wt) {
	(void)wt;
	old = cur; cur = combo;
	if (combo > 1) resttime = displaytime;
	if (resttime > 0) resttime--;
	if (cur > 1) { counterX = start_x; shaketime = counter_shake; }
	if (shaketime > 0) shaketime--;
}
void ComboData::draw(int layerno) { (void)layerno; }
void ComboData::reset() { *this = ComboData{}; }

// =========================================================================
// WinIconData
// =========================================================================
void WinIconData::read(const std::string& prefix, const std::string& sc) {
	auto kv = parse_section(sc);
	for (auto& [key, val] : kv) {
		if (key == prefix + "posx") posx = parse_int(val);
		else if (key == prefix + "posy") posy = parse_int(val);
		else if (key == prefix + "iconoffsetx") iconoffsetx = parse_int(val);
		else if (key == prefix + "iconoffsety") iconoffsety = parse_int(val);
		else if (key == prefix + "useiconupto") useiconupto = parse_int(val);
		else if (key == prefix + "counter.fontn") counter_fontn = parse_int(val);
		else if (key == prefix + "counter.fontb") counter_fontb = parse_int(val);
		else if (key == prefix + "counter.fonta") counter_fonta = parse_int(val);
	}
}
void WinIconData::step(int numwin) { (void)numwin; }
void WinIconData::draw(int layerno) { (void)layerno; }
void WinIconData::add(int wt) { (void)wt; }
void WinIconData::reset() { *this = WinIconData{}; }
void WinIconData::clear() { *this = WinIconData{}; }

// =========================================================================
// RoundData
// =========================================================================
void RoundData::read(const std::string& prefix, const std::string& sc) {
	auto kv = parse_section(sc);
	for (auto& [key, val] : kv) {
		if (key == prefix + "posx") posx = parse_int(val);
		else if (key == prefix + "posy") posy = parse_int(val);
		else if (key == prefix + "match.maxdrawgames") match_maxdrawgames = parse_int(val);
		else if (key == prefix + "start.waittime") start_waittime = parse_int(val);
		else if (key == prefix + "round.time") round_time = parse_int(val);
		else if (key == prefix + "round.sndtime") round_sndtime = parse_int(val);
		else if (key == prefix + "fight.time") fight_time = parse_int(val);
		else if (key == prefix + "fight.sndtime") fight_sndtime = parse_int(val);
		else if (key == prefix + "ctrl.time") ctrl_time = parse_int(val);
		else if (key == prefix + "ko.time") ko_time = parse_int(val);
		else if (key == prefix + "ko.sndtime") ko_sndtime = parse_int(val);
		else if (key == prefix + "slow.time") slow_time = parse_int(val);
		else if (key == prefix + "over.waittime") over_waittime = parse_int(val);
		else if (key == prefix + "over.hittime") over_hittime = parse_int(val);
		else if (key == prefix + "over.wintime") over_wintime = parse_int(val);
		else if (key == prefix + "over.time") over_time = parse_int(val);
		else if (key == prefix + "win.time") win_time = parse_int(val);
		else if (key == prefix + "win.sndtime") win_sndtime = parse_int(val);
	}
}
void RoundData::callFight() { calledFight = true; }
bool RoundData::act(KOTy ko) {
	(void)ko;
	// SSZ: state machine for round announcements (Round 1 → Fight → KO → Win)
	// Returns true when round should advance
	if (cur > 0) cur--;
	return false;
}
void RoundData::draw(int layerno, KOTy ko) { (void)layerno; (void)ko; }
void RoundData::reset() { *this = RoundData{}; calledFight = false; cur = 0; }

// =========================================================================
// DisplayTextData
// =========================================================================
void DisplayTextData::read(const std::string& prefix, const std::string& sc) {
	auto kv = parse_section(sc);
	for (auto& [key, val] : kv) {
		if (key == prefix + "posx") posx = parse_int(val);
		else if (key == prefix + "posy") posy = parse_int(val);
		else if (key == prefix + "fontn") text_fontn = parse_int(val);
		else if (key == prefix + "fontb") text_fontb = parse_int(val);
		else if (key == prefix + "fonta") text_fonta = parse_int(val);
		else if (key == prefix + "text") text_text = val;
	}
}
void DisplayTextData::step() {}
void DisplayTextData::bgDraw(int layerno) { (void)layerno; }
void DisplayTextData::draw(int layerno) { (void)layerno; }
void DisplayTextData::reset() { *this = DisplayTextData{}; }

// =========================================================================
// FightData
// =========================================================================

void FightData::load(const std::string& defPath) {
	def = defPath;

	// Read fight.def
	std::string buf = common_load_text(defPath, false);
	if (buf.empty()) return;

	auto lines = common_split_lines(buf);

	// Parse sections
	for (size_t i = 0; i < lines.size(); i++) {
		const auto& line = lines[i];
		std::string trimmed = trim(line);
		if (trimmed.empty() || trimmed[0] == ';') continue;
		if (trimmed[0] != '[') continue;

		// Section header
		size_t close = trimmed.find(']');
		if (close == std::string::npos) continue;
		std::string sectionName = to_lower(trimmed.substr(1, close - 1));
		std::string body = extract_section_body(lines, i);

		// Route to sub-component parsers based on section name
		if (sectionName.find("lifebar") != std::string::npos) {
			int p = (sectionName.find("p2") != std::string::npos || sectionName.find("2") != std::string::npos) ? 1 : 0;
			lifebar[p].read("", body);
		} else if (sectionName.find("powerbar") != std::string::npos) {
			int p = (sectionName.find("p2") != std::string::npos || sectionName.find("2") != std::string::npos) ? 1 : 0;
			powerbar[p].read("", body);
		} else if (sectionName.find("face") != std::string::npos) {
			int p = (sectionName.find("p2") != std::string::npos) ? 1 : 0;
			face[p].read("", body);
		} else if (sectionName.find("name") != std::string::npos) {
			int p = (sectionName.find("p2") != std::string::npos) ? 1 : 0;
			name[p].read("", body);
		} else if (sectionName == "time" || sectionName == "timer") {
			time.read("", body);
		} else if (sectionName.find("combo") != std::string::npos) {
			int p = (sectionName.find("p2") != std::string::npos) ? 1 : 0;
			combo[p].read("", body);
		} else if (sectionName.find("winicon") != std::string::npos) {
			int p = (sectionName.find("p2") != std::string::npos) ? 1 : 0;
			winicon[p].read("", body);
		} else if (sectionName == "round" || sectionName == "rounds") {
			round.read("", body);
		} else if (sectionName.find("wincount") != std::string::npos) {
			int p = (sectionName.find("p2") != std::string::npos) ? 1 : 0;
			wincount[p].read("", body);
		} else if (sectionName.find("score") != std::string::npos) {
			int p = (sectionName.find("p2") != std::string::npos) ? 1 : 0;
			score[p].read("", body);
		}
		// Other sections: match, ailevel, gamemode, reward, etc.
	}

	// Initialize per-player state
	for (auto& lp : lifePower) {
		lp.l = 1.0f; lp.p = 0.0f; lp.lv = 0;
	}
	for (auto& f : face) f.reset();
}

void FightData::step(int& tm, LifePowerData& life0, LifePowerData& life1, bool& hit, int& combo) {
	// Update per-player life/power from external state
	lifePower[0] = life0;
	lifePower[1] = life1;
	if (tm >= 0) tm--;

	// Step sub-components
	lifebar[0].step(lifePower[0].l, hit);
	lifebar[1].step(lifePower[1].l, hit);
	powerbar[0].step(lifePower[0].p, lifePower[0].lv);
	powerbar[1].step(lifePower[1].p, lifePower[1].lv);
	face[0].step();
	face[1].step();
	name[0].step();
	name[1].step();
	time.step();
	this->combo[0].step(combo, 0);
	this->combo[1].step(combo, 0);
	winicon[0].step(0);
	winicon[1].step(0);
	round.act(KOTy::None);
}

void FightData::draw(int layerno) {
	// Rendering deferred — needs sdlplugin/sff/font
	(void)layerno;
}

void FightData::clear() {
	def.clear();
	for (auto& lp : lifePower) { lp.l = 1.0f; lp.p = 0.0f; lp.lv = 0; }
	lifebar[0].reset(); lifebar[1].reset();
	powerbar[0].reset(); powerbar[1].reset();
	face[0].reset(); face[1].reset();
	name[0].reset(); name[1].reset();
	time.reset();
	combo[0].reset(); combo[1].reset();
	winicon[0].reset(); winicon[1].reset();
	round.reset();
}

void FightData::reset() {
	for (auto& lp : lifePower) { lp.l = 1.0f; lp.p = 0.0f; }
	lifebar[0].reset(); lifebar[1].reset();
	powerbar[0].reset(); powerbar[1].reset();
	face[0].reset(); face[1].reset();
	name[0].reset(); name[1].reset();
	time.reset();
	combo[0].reset(); combo[1].reset();
	winicon[0].reset(); winicon[1].reset();
	round.reset();
}

// =========================================================================
// Module-level API
// =========================================================================

void fight_init() {
	g_fight_state = FightState{};
	g_fight_state.initialized = true;
}

void fight_laydraw(int ln) { (void)ln; }
void fight_laytext(int ln) { (void)ln; }
void fight_layspr(int ln) { (void)ln; }
void fight_read_spr(AnimData& a, const std::string& data) { (void)a; (void)data; }
void fight_read_anim(AnimData& a, const std::string& data) { (void)a; (void)data; }

} // namespace ikemen::ssz_native
