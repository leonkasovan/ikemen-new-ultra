// char_service.cpp — Real implementations for char.ssz (Phase 5).
// Character state machine, animation, collision, and gameplay lifecycle.

#include "char_service.hpp"
#include "common_service.hpp"
#include "command_service.hpp"
#include "sff_service.hpp"
#include "action_service.hpp"
#include "sdlplugin_service.hpp"
#include "fight_service.hpp"
#include "stage_service.hpp"

#include <algorithm>
#include <cmath>
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
void ProjectileData::update(int playerNo) {
	// Update position based on velocity (frame-step physics)
	(void)playerNo;
	x += xvel;
	y += yvel;
}
void ProjectileData::hitCheck(ProjectileData& pr) {
	// projectile-vs-projectile collision detection
	// Deferred — needs full hitdef/hitbox system
	(void)pr;
}
void ProjectileData::projClsn(int playerNo) {
	// projectile-vs-character hitbox collision
	// Deferred — needs full ClsnHanteiData system
	(void)playerNo;
}
void ProjectileData::tick(int playerNo) {
	// Decrement projectile lifetime counters
	// Deferred — needs hitCount/hitCountMax, owner state reference
	(void)playerNo;
}
void ProjectileData::anime(bool oVer, int playerNo) {
	// Advance projectile animation frame
	// Deferred — needs anim->updateSprite() with projectile-specific logic
	(void)oVer; (void)playerNo;
}

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
	if (!anim) return;
	if (!anim->spr) {
		anim->updateSprite();
		if (!anim->spr) return;
	}

	// Get the current frame
	FrameData* frame = anim->drawFrame();
	if (!frame) return;

	// Compute world-space position (char pos + offset)
	float drawX = x;
	float drawY = y;

	// Screen-space: use character facing and scale
	float xs = static_cast<float>(facing);
	float ys = 1.0f;

	// Queue via addAnimList for priority-sorted rendering
	// SSZ Char::drawAnim calls .addAnimList() directly
	// For simplicity, render immediately via AnimData::draw
	const auto& cd = common_get_state();
	const auto& cam = cd.cam;

	// Compute screen position following SSZ drawAnim code path:
	// For non-screen, non-angle sprites:
	//   px = cam.xOffset / cs - (camX - charX);
	//   py = (cam.groundLevel() + cam.yOffset) / cs - (camY - charY);
	float cs = cd.cam.scale / cam_base_scale(cam); // scl ≈ scale / baseScale
	float px = cam.xOffset / cs - (cd.cam.x - drawX);
	float py = (camera_ground_level(cam) + cam.yOffset) / cs - (cd.cam.y - drawY);

	// Render directly
	anim->draw(
		256,                // alpha (0-256, SSZ default)
		px, py,             // screen position (x, y)
		cs, cs,             // base scale (xs, ys)
		xs,                 // xts (extra x scale = facing)
		ys,                 // xbs (extra y scale = base)
		1.0f,               // yss (final y scale)
		0.0f,               // rxadd (raster x add)
		static_cast<float>(cd.GameWidth) / 2.0f,  // agl (angle/offset)
		0                   // trans (blend mode)
	);
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

// =========================================================================
// char_round_over() — Check if the current round is over
// =========================================================================
//
// SSZ semantics (chr.roundOver() in char.ssz):
//   Returns true when the round should end. The fight engine orchestration
//   (fighting.ssz game()) uses this to drive round transitions.
//
// Native implementation:
//   Checks if all characters on at least one team have life <= 0 after the
//   intro phase has completed. In the full SSZ, this also checks fight
//   round state machine (roundState() >= 3) and KO flags (sfOVER/sfKO).
//
bool char_round_over() {
	CommonData& cd = common_get_state();

	// Forced round over (e.g., debug skip or exitMatch)
	if (cd.forceOver) return true;

	// Round isn't over during intro phase (first ~20 frames)
	// SSZ: .com.intro < -.fight~ro.over_hittime check via roundEnd()
	if (cd.intro > 0) return false;

	// Don't check during loading state
	if (cd.gameState == 0) return false;

	// Check each team (0 = P1 side, 1 = P2 side)
	// Track both KO status AND whether the team has any characters
	bool team0AllKO = true;
	bool team1AllKO = true;
	bool team0HasChar = false;
	bool team1HasChar = false;

	for (int i = 0; i < 4; i++) {
		CharData* ch = g_char_state.chars[i];
		if (!ch) continue;

		int team = i & 1; // Even = team 0 (P1), Odd = team 1 (P2)
		bool isKO = (ch->life <= 0.0f);

		if (team == 0) {
			team0HasChar = true;
			if (!isKO) team0AllKO = false;
		} else {
			team1HasChar = true;
			if (!isKO) team1AllKO = false;
		}
	}

	// A team can only be considered KO'd if it has at least one character
	// AND all of its characters have life <= 0
	if ((team0HasChar && team0AllKO) || (team1HasChar && team1AllKO)) {
		return true;
	}

	// Timer expiration — when roundTime counts down to 0, time-over ends the round
	// In the SSZ: tscri.cwc~roundState() >= 3 also covers this case
	if (cd.roundTime <= 0) {
		// Round timer has expired — time-over condition
		// This is valid regardless of whether characters are loaded on both teams
		cd.timeover = true;  // Signal to lifebar to display "Time"
		return true;
	}

	return false;
}

// =========================================================================
// char_round_winner() — Determine which team won the round
// =========================================================================
//
// Returns 0 for P1 win, 1 for P2 win, or -1 for draw.
//
// For KO: the team that still has at least one character with life > 0 wins.
// For timeover: the team with more total remaining life wins.
//   If total life is equal, returns -1 (draw).
//
int char_round_winner() {
	CommonData& cd = common_get_state();

	// Sum remaining life per team and count alive characters
	float team0Life = 0.0f;
	float team1Life = 0.0f;
	int team0Alive = 0;
	int team1Alive = 0;
	int team0Total = 0;
	int team1Total = 0;

	for (int i = 0; i < 4; i++) {
		CharData* ch = g_char_state.chars[i];
		if (!ch) continue;

		if ((i & 1) == 0) {
			// Team 0 (P1 side: slots 0, 2)
			team0Life += ch->life;
			team0Total++;
			if (ch->life > 0.0f)
				team0Alive++;
		} else {
			// Team 1 (P2 side: slots 1, 3)
			team1Life += ch->life;
			team1Total++;
			if (ch->life > 0.0f)
				team1Alive++;
		}
	}

	// ── KO check: team with no alive characters loses ──
	// Handle double KO first (both simultaneously KO'd → draw)
	bool team0KO = (team0Total > 0 && team0Alive == 0);
	bool team1KO = (team1Total > 0 && team1Alive == 0);
	if (team0KO && team1KO)
		return -1; // Double KO = draw
	if (team0KO)
		return 1; // P2 wins (team 0 all KO'd)
	if (team1KO)
		return 0; // P1 wins (team 1 all KO'd)

	// ── Timeover check: team with more total life wins ──
	if (cd.roundTime <= 0) {
		if (team0Life > team1Life)
			return 0; // P1 wins
		if (team1Life > team0Life)
			return 1; // P2 wins
		// Equal life → draw
		return -1;
	}

	// If neither KO nor timeover, return -1 (undetermined)
	return -1;
}

// =========================================================================
// char_add_anim_list — Insert a sprite into the anim list by priority
// =========================================================================
// SSZ: binary insertion sort by priority into the anim sprite array.
void char_add_anim_list(
	std::vector<AnimSpriteData>& list,
	ActionData* action, int priority,
	float x, float y, bool screen,
	float xscl, float yscl, float angle, bool oVer,
	float axscl, float ayscl, int salpha, int dalpha,
	bool bright)
{
	if (!action) return;

	AnimSpriteData as;
	as.anim = action;
	as.priority = priority;
	as.x = x;
	as.y = y;
	as.screen = screen;
	as.xscl = xscl;
	as.yscl = yscl;
	as.angle = angle;
	as.oVer = oVer;
	as.axscl = axscl;
	as.ayscl = ayscl;
	as.salpha = salpha;
	as.dalpha = dalpha;
	as.bright = bright;

	// Binary insertion by priority (ascending)
	size_t lo = 0, hi = list.size();
	while (lo < hi) {
		size_t mid = lo + (hi - lo) / 2;
		if (priority < list[mid].priority) {
			hi = mid;
		} else {
			lo = mid + 1;
		}
	}
	list.insert(list.begin() + static_cast<std::ptrdiff_t>(lo), std::move(as));
}

// =========================================================================
// char_draw_anim_list — Render all sprites in the anim list
// =========================================================================
// SSZ: drawAnimList(anims, x, y, scl * cam.baseScale())
void char_draw_anim_list(
	const std::vector<AnimSpriteData>& list,
	float x, float y, float scl)
{
	const auto& cd = common_get_state();
	const auto& cam = cd.cam;

	for (const auto& as : list) {
		if (!as.anim) continue;
		if (!as.anim->ani.spr) as.anim->ani.updateSprite();
		if (!as.anim->ani.spr) continue;

		float cs = as.screen ? 1.0f : scl;

		float px, py;
		if (as.angle != 0.0f) {
			// Angle draw path
			if (as.screen) {
				px = as.x;
				py = as.y + static_cast<float>(cd.GameHeight - 240);
			} else {
				px = cam.xOffset - (x - as.x) * cs;
				py = camera_ground_level(cam) + cam.yOffset
					- (y - as.y) * cs;
			}
			// Angle draw deferred — use normal draw as fallback
			as.anim->ani.draw(
				256, px, py, cs * as.xscl * as.axscl,
				cs * as.yscl * as.ayscl,
				1.0f, 1.0f, 0.0f, 0.0f,
				static_cast<float>(cd.GameWidth) / 2.0f, 0);
		} else {
			// Normal draw path
			if (as.screen) {
				px = as.x;
				py = as.y + static_cast<float>(cd.GameHeight - 240);
			} else {
				px = cam.xOffset / cs - (x - as.x);
				py = (camera_ground_level(cam) + cam.yOffset) / cs - (y - as.y);
			}
			as.anim->ani.draw(
				256, px, py, cs, cs,
				as.xscl, as.xscl, as.yscl,
				0.0f, static_cast<float>(cd.GameWidth) / 2.0f, 0);
		}
	}
}

// =========================================================================
// char_add_shadow_list — Add a shadow sprite to the shadow list
// =========================================================================
void char_add_shadow_list(
	AnimSpriteData* as, int color, float offset, int alpha, float fadeoffset)
{
	if (!as) return;
	auto& shadows = char_get_state().shadows;
	ShadowSpriteData ss;
	ss.as = as;
	ss.color = color;
	ss.alpha = alpha;
	ss.offsety = offset * (as->oVer ? 1.5f : 1.0f);
	ss.fadeoffset = fadeoffset;

	// Sort by shadow's sprite priority
	int p = as->priority;
	size_t lo = 0, hi = shadows.size();
	while (lo < hi) {
		size_t mid = lo + (hi - lo) / 2;
		if (p <= shadows[mid].as->priority) {
			hi = mid;
		} else {
			lo = mid + 1;
		}
	}
	shadows.insert(shadows.begin() + static_cast<std::ptrdiff_t>(lo), std::move(ss));
}

// =========================================================================
// char_draw_shadow_list — Render all shadow sprites
// =========================================================================
// SSZ: drawShadowList(x, y, scl) — iterates shadows and renders each
void char_draw_shadow_list(float x, float y, float scl) {
	const auto& cd = common_get_state();
	const auto& cam = cd.cam;
	(void)scl;

	for (const auto& ss : char_get_state().shadows) {
		if (!ss.as || !ss.as->anim) continue;
		AnimData* ani = &ss.as->anim->ani;
		if (!ani->spr) continue;

		// Compute screen position (simplified — matches SSZ shadowDraw logic)
		float px = cam.xOffset - (x - ss.as->x) * scl;
		float py = camera_ground_level(cam) + cam.yOffset
			- (y + ss.as->y - ss.offsety) * scl;

		// Shadow rendering via renderMugenShadow
		SdlRect dr;
		dr.set(0, 0, cd.GameWidth, cd.GameHeight);
		SdlRect sr;
		sr.set(ani->spr->rct_x, ani->spr->rct_y, ani->spr->rct_w, ani->spr->rct_h);

		uint32_t color = static_cast<uint32_t>(ss.color);
		int alphaVal = ss.alpha;
		if (alphaVal > 255) alphaVal = 255;
		if (alphaVal < 0) alphaVal = 0;

		std::vector<int8_t> pluginbuf;
		pluginbuf.reserve(1024);

		renderMugenShadow(
			dr, 0.0f, 0.0f,
			ani->spr->pxl, color,
			sr,
			-px * cd.WidthScale, -py * cd.HeightScale,
			scl * ss.as->xscl * ss.as->axscl,
			scl * -ss.as->yscl * ss.as->ayscl,
			1.0f, 0u,
			alphaVal,
			ani->spr->rle,
			pluginbuf
		);
	}
}

// =========================================================================
// char_draw_reflection — Render reflections
// =========================================================================
// SSZ: drawReflection(x, y, scl) — renders shadow-like reflections
void char_draw_reflection(float x, float y, float scl) {
	// Reflections are a type of shadow rendering — deferred
	// The SSZ implementation is similar to drawShadowList but with
	// vertical flip (negative yscl) and alpha adjustments.
	// For now, render as shadows with flipped Y.
	(void)x;
	(void)y;
	(void)scl;
	// Reflection rendering deferred until shadow pipeline is verified
}

// =========================================================================
// char_draw — Main module-level draw function
// =========================================================================
// SSZ: char.draw(x, y, scl) — renders the entire fight scene including
// characters, shadows, reflections, fight UI, edge fading, and effects.
// This is the native equivalent of the ~170-line SSZ function at line 7406
// of char.ssz. Subsystems that are not yet converted are stubbed with
// comments.
void char_draw(float x, float y, float scl) {
	CommonData& cd = common_get_state();
	CharModuleState& cs = char_get_state();

	// ── 1. Brightness: darken during super moves ──
	// SSZ: .com.brightness = 256 >> (int)(super > 0 && superdarken != 0);
	// The brightness field is not yet wired to rendering — skip for now.

	// ── 2. Background ──
	// SSZ: if NOBG → solid fill with palFX color; else stg~bgDraw(false, bgx, bgy, scl)
	// bgx = x / localscl, bgy = y / localscl (SSZ char.draw() line 7440)
	{
		float bgx = x / cd.cam.stg.localscl;
		float bgy = y / cd.cam.stg.localscl;
		stage_bg_draw(false, bgx, bgy, scl);
	}

	// ── 3. Reflections and shadows ──
	// SSZ: if(reflection > 0) drawReflection(...); drawShadowList(...)
	// Shadow/reflection rendering deferred until stage data is wired.
	// char_draw_shadow_list(x, y, scl);
	// char_draw_reflection(x, y, scl);

	// ── 4. Edge fading (screen borders to hide void space) ──
	// SSZ: computes fade rects for top/bottom/left/right screen edges
	// based on camera bounds and scroll position.
	{
		float off = stage_get_env_shake().getOffset();
		const CameraData& cam = cd.cam;

		// Vertical yofs/yofs2 compute the screen-space offset caused by zoom
		// (scl > 1 means zoomed in, exposing void at screen edges).
		float yofs = (scl > 1.0f && cam.stg.verticalfollow > 0.0f)
			? (cam.screenZoff + static_cast<float>(cd.GameHeight - 240))
			: static_cast<float>(cd.GameHeight);
		yofs *= (1.0f / scl - 1.0f);

		float yofs2 = (scl > 1.0f && cam.stg.verticalfollow > 0.0f)
			? (240.0f - cam.screenZoff) * (1.0f - 1.0f / scl)
			: 0.0f;

		SdlRect rect;
		rect.set(0, 0, cd.GameWidth, cd.GameHeight);

		// ── Top edge: envShake pulls screen down, revealing void above ──
		if (off < (yofs - y + cam.boundH) * scl) {
			rect.h = (
				static_cast<int>(std::ceil(
					((yofs - y + cam.boundH) * scl - off)
					* static_cast<float>(cd.GameHeight)))
				+ (cd.GameHeight - 1))
				/ cd.GameHeight;
			common_rect_fill(rect, 0x000000, 255);
		}

		// ── Bottom edge: envShake pushes screen up, revealing void below ──
		if (off > (-y + yofs2) * scl) {
			rect.h = (
				static_cast<int>(std::ceil(
					((y - yofs2) * scl + off)
					* static_cast<float>(cd.GameHeight)))
				+ (cd.GameHeight - 1))
				/ cd.GameHeight;
			rect.y = cd.GameHeight - rect.h;
			common_rect_fill(rect, 0x000000, 255);
		}

		// ── Horizontal bounds ──
		float bl = std::min(x, cam.boundL);
		float br = std::max(x, cam.boundR);
		float xofs = static_cast<float>(cd.GameWidth) * (1.0f / scl - 1.0f) / 2.0f;

		rect.set(0, 0, cd.GameWidth, cd.GameHeight);

		// ── Left edge: camera scrolled left past stage left bound ──
		if (x - xofs < bl) {
			rect.w = (
				static_cast<int>(std::ceil(
					(bl - (x - xofs)) * scl * static_cast<float>(cd.GameWidth)))
				+ (cd.GameWidth - 1))
				/ cd.GameWidth;
			common_rect_fill(rect, 0x000000, 255);
		}

		// ── Right edge: camera scrolled right past stage right bound ──
		if (x + xofs > br) {
			rect.w = (
				static_cast<int>(std::ceil(
					((x + xofs) - br) * scl * static_cast<float>(cd.GameWidth)))
				+ (cd.GameWidth - 1))
				/ cd.GameWidth;
			rect.x = cd.GameWidth - rect.w;
			common_rect_fill(rect, 0x000000, 255);
		}
	}

	// ── Collect char data from character state ──
	LifePowerData lifeBuf[4]{};
	std::string nameBuf[4]{};
	int lifeCount = 0;
	int nameCount = 0;
	for (int i = 0; i < 4; i++) {
		CharData* ch = cs.chars[i];
		if (ch) {
			lifeBuf[lifeCount].l = ch->life / ch->lifeMax;
			lifeBuf[lifeCount].p = static_cast<float>(ch->power) / 1000.0f;
			lifeBuf[lifeCount].lv = 0;
			lifeCount++;
			
			nameBuf[nameCount] = ch->name;
			nameCount++;
		}
	}
	bool nbd = false;
	int superplayer = -1;
	
	// ── 5. Fight UI layer 0 ──
	{
		fight_get_state().fight.draw(0, lifeBuf, lifeCount, nameBuf, nameCount, nbd, superplayer);
		fight_get_state().fight.round.draw(0, KOTy::None, nameBuf, nameCount);
	}

	// ── 6. Anim list (character sprites) ──
	// SSZ: .drawAnimList(.anims=, x, y, scl * cam.baseScale())
	char_draw_anim_list(cs.anims, x, y, scl * cam_base_scale(cd.cam));

	// ── 7. Foreground background ──
	// SSZ: if(!NOFG) stg~bgDraw(true, bgx, bgy, scl)
	{
		float bgx = x / cd.cam.stg.localscl;
		float bgy = y / cd.cam.stg.localscl;
		stage_bg_draw(true, bgx, bgy, scl);
	}

	// ── 8. Fight UI layer 1 ──
	{
		fight_get_state().fight.draw(1, lifeBuf, lifeCount, nameBuf, nameCount, nbd, superplayer);
		fight_get_state().fight.round.draw(1, KOTy::None, nameBuf, nameCount);
	}

	// ── 9. Top anims ──
	// SSZ: .drawAnimList(.topanims=, x, y, scl * cam.baseScale())
	char_draw_anim_list(cs.topanims, x, y, scl * cam_base_scale(cd.cam));

	// ── 10. Fight UI layer 2 ──
	{
		fight_get_state().fight.draw(2, lifeBuf, lifeCount, nameBuf, nameCount, nbd, superplayer);
		fight_get_state().fight.round.draw(2, KOTy::None, nameBuf, nameCount);
	}

	// ── 11. Screen fade (intro/outro transitions) ──
	// SSZ: fade animation for intro, KO, and outro screens
	// Uses fight~ro data for timing — not yet converted.

	// ── 12. Shutter effect ──
	// SSZ: shuttertime > 0 draws bars from top and bottom
	// shuttertime not yet wired — skip.

	// ── 13. Cleanup ──
	// SSZ: .com.brightness = ob; restore brightness
	// if(clsndraw) drawClsn();
}

} // namespace ikemen::ssz_native
