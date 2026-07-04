// char_service.cpp — Real implementations for char.ssz (Phase 5).
// Character state machine, animation, collision, and gameplay lifecycle.

#include "char_service.hpp"
#include "common_service.hpp"
#include "command_service.hpp"
#include "sff_service.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

namespace ikemen::ssz_native {

// =========================================================================
// Module-level state
// =========================================================================
static CharModuleState g_char_state;

CharModuleState& char_get_state() { return g_char_state; }

// =========================================================================
// VarData
// =========================================================================
void VarData::setI(int v) { i = v; ty = VarTy::Int; }
void VarData::setF(float v) { f = v; ty = VarTy::Float; }
void VarData::setB(bool v) { b = v; ty = VarTy::Int; }
void VarData::setSF() { ty = VarTy::SFalse; }

// =========================================================================
// ClsnHanteiData
// =========================================================================
void ClsnHanteiData::set(float xs1, float ys1, float xo1, float yo1, int lr1,
	float xs2, float ys2, float xo2, float yo2, int lr2)
{
	// SSZ match: compare two collision boxes, considering facing
	// Deferred — full implementation needs the Clsn rect data
	(void)xs1; (void)ys1; (void)xo1; (void)yo1; (void)lr1;
	(void)xs2; (void)ys2; (void)xo2; (void)yo2; (void)lr2;
}

// =========================================================================
// FallData
// =========================================================================
void FallData::clear() {}
void FallData::setDefault() {}

// =========================================================================
// HitdefData
// =========================================================================
void HitdefData::clear() {
	*this = HitdefData{};
}

void HitdefData::setDefault() {
	clear();
	// Set M.U.G.E.N. defaults
	pausetime = 12;
	getpower = 36;
	givepower = 36;
	p1stateno = -1;
	p2stageno = -1;
	forcenofall = 0;
	kills = 1;
	guardflag = 0;
	hitflag = 0xFFFF;
	priority = 1;
	id = -1;
	numhits = 1;
	fall = 0;
	fall_x_vel = 0;
	fall_y_vel = 0;
	fall_recover = 1;
	fall_recover_time = 0;
	fall_damage = 0;
	fall_kill = 1;
	animtype = 0;
	airtype = 0;
	groundtype = 0;
	juggle = 0;
	yaccel = 0;
	p1_vel_x = 0; p1_vel_y = 0;
	p2_vel_x = 0; p2_vel_y = 0;
	sparkno = -1;
	guard_sparkno = -1;
	sparkxy = 0;
	hitsound_group = -1;
	hitsound_number = -1;
	guardsound_group = -1;
	guardsound_number = -1;
	ground_sliding = 0;
	ground_hits = 1;
	air_hits = 1;
	envshake_time = 0;
	envshake_freq = 0;
	envshake_ampl = 0;
	envshake_phase = 0;
}

void HitdefData::invalidate(StTy stateType) {
	(void)stateType;
	// SSZ: invalidate hitdef for given state type
}

// =========================================================================
// HitByData
// =========================================================================
void ByData::set(int id_, int juggle_) { id = id_; juggle = juggle_; }
void HitByData::clear() { count = 0; }
void HitByData::clearOff() { clear(); }
void HitByData::dropByid(int id_) {
	for (int i = 0; i < count; i++) {
		if (by[i].id == id_) {
			for (int j = i; j < count - 1; j++) by[j] = by[j + 1];
			count--;
			return;
		}
	}
}
void HitByData::addByid(int id_, int juggle_) {
	if (count >= 4) return;
	by[count].set(id_, juggle_);
	count++;
}

// =========================================================================
// HitOverrideData
// =========================================================================
void HitOverrideData::clear() {
	*this = HitOverrideData{};
}

// =========================================================================
// ConfigTVarsData
// =========================================================================
void ConfigTVarsData::set(int st_, int dst_, int ag_, int ge_, int rec_,
	int recdir_, int dis_, int inp_, int ru_, int vc_, int lb_)
{
	st = st_; dst = dst_; ag = ag_; ge = ge_;
	rec = rec_; recdir = recdir_; dis = dis_; inp = inp_;
	ru = ru_; vc = vc_; lb = lb_;
}

// =========================================================================
// StateValData
// =========================================================================
void StateValData::clearWw() {}
void StateValData::clear() {}
void StateValData::hitCheck(HitdefData& hit, bool guard) { (void)hit; (void)guard; }
void StateValData::setHb(HitdefData& hit, bool guard, bool combo, int absdamage) {
	(void)hit; (void)guard; (void)combo; (void)absdamage;
}

// =========================================================================
// CharGlobalInfo
// =========================================================================
void CharGlobalInfo::clearPCTime() {}

// =========================================================================
// AfterImageData
// =========================================================================
void AfterImageData::clear() { *this = AfterImageData{}; }
void AfterImageData::setPalcolor(int palcol) { (void)palcol; }
void AfterImageData::setPalinvertall(bool palinv) { (void)palinv; }
void AfterImageData::setPalbrightR(int palbrr) { (void)palbrr; }
void AfterImageData::setPalbrightG(int palbrg) { (void)palbrg; }
void AfterImageData::setPalbrightB(int palbrb) { (void)palbrb; }
void AfterImageData::setPalcontrastR(int palcor) { (void)palcor; }
void AfterImageData::setPalcontrastG(int palcog) { (void)palcog; }
void AfterImageData::setPalcontrastB(int palcob) { (void)palcob; }
void AfterImageData::setupPalfx() {}
void AfterImageData::recAfterImg() {}
void AfterImageData::recAndAddAL() {}

// =========================================================================
// ExplodData
// =========================================================================
void ExplodData::clear() { *this = ExplodData{}; }
void ExplodData::setX(float x_) { x = x_; }
void ExplodData::setY(float y_) { y = y_; }

// =========================================================================
// ProjectileData
// =========================================================================
void ProjectileData::clear() { *this = ProjectileData{}; }
void ProjectileData::setX(float x_) { x = x_; }
void ProjectileData::setY(float y_) { y = y_; }
void ProjectileData::remvel() { xvel = 0; yvel = 0; }
void ProjectileData::update(int playerNo) { (void)playerNo; }
void ProjectileData::hitCheck(ProjectileData& pr) { (void)pr; }
void ProjectileData::projClsn(int playerNo) { (void)playerNo; }
void ProjectileData::tick(int playerNo) { (void)playerNo; }
void ProjectileData::anime(bool oVer, int playerNo) { (void)oVer; (void)playerNo; }

// =========================================================================
// CharData
// =========================================================================

void CharData::init(const std::string& defPath, int playerNo_) {
	playerNo = playerNo_;
	def = defPath;
	x = 0; y = 0;
	xvel = 0; yvel = 0;
	pos_x = 0; pos_y = 0;
	facing = 1;
	life = 1000.0f;
	lifeMax = 1000.0f;
	power = 0;
	ctrl = false;
	hitPause = false;
	stateno = 0;
	timeInState = 0;
	gravity = 0.5f;
	attackMul = 1.0f;
	defenceMul = 1.0f;
	hitdef.setDefault();
	explods.clear();
	projectiles.clear();
	afterImages.clear();
}

void CharData::load(const std::string& defPath) {
	// Parse character .def file
	// Uses common_service's section parsing
	init(defPath, playerNo);
	name = defPath; // placeholder — extract from [Info] section

	// Read .def file
	std::string buf = common_load_text(defPath, false);
	if (buf.empty()) return;

	auto lines = common_split_lines(buf);
	// Trim and remove comments
	std::vector<std::string> cleaned;
	for (auto& line : lines) {
		size_t start = line.find_first_not_of(" \t\r\n");
		if (start == std::string::npos) continue;
		line = line.substr(start);
		if (line.empty() || line[0] == ';') continue;
		cleaned.push_back(line);
	}

	// Parse [Info] section for name
	bool inInfo = false;
	for (size_t i = 0; i < cleaned.size(); i++) {
		const auto& line = cleaned[i];
		if (line[0] == '[') {
			std::string section = line;
			// Convert to lowercase
			for (auto& c : section) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			inInfo = (section.find("[info]") != std::string::npos);
			continue;
		}
		if (inInfo) {
			size_t eq = line.find('=');
			if (eq != std::string::npos) {
				std::string key = line.substr(0, eq);
				// Trim
				size_t ks = key.find_first_not_of(" \t");
				if (ks != std::string::npos) key = key.substr(ks);
				size_t ke = key.find_last_not_of(" \t");
				if (ke != std::string::npos) key = key.substr(0, ke + 1);
				if (key == "name" || key == "displayname") {
					name = line.substr(eq + 1);
					size_t ns = name.find_first_not_of(" \t");
					if (ns != std::string::npos) name = name.substr(ns);
					size_t ne = name.find_last_not_of(" \t\r\n");
					if (ne != std::string::npos) name = name.substr(0, ne + 1);
				}
			}
		}
	}
}

void CharData::loadPallet(const std::string& defPath, int no) {
	(void)defPath; (void)no;
}

void CharData::clearDef() {
	def.clear();
}

void CharData::tick() {
	// Called each game tick
	timeInState++;
}

void CharData::action() {
	// Advance animation
	if (anim) anim->action();
}

void CharData::posUpdate() {
	x += xvel;
	y += yvel;
	pos_x = x;
	pos_y = y;
}

void CharData::gravityStep() {
	yvel += gravity;
}

void CharData::bind() {
	// Binding logic deferred
}

void CharData::xScreenBound() {
	// Screen boundary clamping deferred (needs CommonData)
}

void CharData::update() {
	tick();
	action();
	posUpdate();
	gravityStep();
}

void CharData::posReset() {
	x = 0; y = 0;
	xvel = 0; yvel = 0;
}

void CharData::setLife(float l) { life = l; }
void CharData::addLife(float l, bool kill, bool absolute) {
	(void)kill; (void)absolute;
	life += l;
	if (life > lifeMax) life = lifeMax;
}

int CharData::getDamage(double damage, bool kill, bool absolute, float atkmul) {
	(void)kill; (void)absolute; (void)atkmul;
	life -= static_cast<float>(damage);
	return 0;
}

void CharData::setPower(int p) { power = p; }
void CharData::addPower(int p) { power += p; }
void CharData::setLifeMax(int l) { lifeMax = static_cast<float>(l); }
void CharData::setAttack(int a) { attackMul = static_cast<float>(a) / 100.0f; }
void CharData::setDefence(int d) { defenceMul = static_cast<float>(d) / 100.0f; }
void CharData::setFacing(int f) { facing = f; }
void CharData::setCtrl(bool c) { ctrl = c; }
bool CharData::canCtrl() { return ctrl && !hitPause; }
void CharData::setPosX(float x_) { pos_x = x = x_; }
void CharData::setPosY(float y_) { pos_y = y = y_; }
void CharData::setX(float x_) { x = x_; }
void CharData::setY(float y_) { y = y_; }
void CharData::setXV(float xv) { xvel = xv; }
void CharData::setYV(float yv) { yvel = yv; }

void CharData::drawAnim() {
	// Sprite rendering deferred (needs sdlplugin)
}

void CharData::furimuki() {
	// Face direction logic deferred
}

void CharData::posUpdateSub() {
	posUpdate();
}

// ── Transform helpers ──
int CharData::trAnimTime() { return anim ? anim->animTime() : 0; }
int CharData::trAnimElemTime(int e) { return anim ? anim->animElemTime(e) : 0; }
int CharData::trAnimElemNo(int tim) { return anim ? anim->animElemNo(tim) : 0; }
void CharData::trChangeAnim(int no) { setAnimElem(no); }
void CharData::trChangeAnim2(int no) { setAnimElem(no); }
void CharData::setAnimElem(int e) { if (anim) anim->setAnimElem(e); }
bool CharData::trAnimExist(int pid) {
	// Check if action exists — deferred
	(void)pid;
	return false;
}

// =========================================================================
// PlayerListData
// =========================================================================
void PlayerListData::clear() { players.clear(); }
CharData* PlayerListData::get(int id) {
	for (auto* p : players) if (p->playerNo == id) return p;
	return nullptr;
}
void PlayerListData::add(CharData* c) { players.push_back(c); }
void PlayerListData::destroy(int id) {
	for (auto it = players.begin(); it != players.end(); ++it) {
		if ((*it)->playerNo == id) { players.erase(it); return; }
	}
}
void PlayerListData::action() { for (auto* p : players) p->action(); }
void PlayerListData::update() { for (auto* p : players) p->update(); }
void PlayerListData::tick() { for (auto* p : players) p->tick(); }

// =========================================================================
// Module-level API
// =========================================================================

void char_init() {
	g_char_state = CharModuleState{};
}

} // namespace ikemen::ssz_native
