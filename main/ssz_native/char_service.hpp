// char_service.hpp — Native C++ implementation for ssz_script/ssz/char.ssz
//
// char.ssz (7665 lines) implements the character engine — character state
// machine, animation, sprite rendering, hitbox/collision detection, AI,
// and character data loading.
//
// Phase 5: Core data structures, character loading, lifecycle (init/tick/
// action/draw), collision system, and helper/explod/projectile support.
// Full state machine and AI logic deferred.

#pragma once

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include <functional>

namespace ikemen::ssz_native {

// Forward declarations
struct AnimData;
struct SpriteData;
struct CommandListData;
struct CommonData;

// =========================================================================
// Enums
// =========================================================================
enum class VarTy : int8_t { Int, Float, SFalse };
enum class StTy : int8_t  { S, C, A, L, N, U };
enum class AtTy : int8_t  { NA, NT, NP, SA, ST, SP, HA, HT, HP };
enum class MovTy : int8_t { I, A, H, U };
enum class AnimTy : int8_t { Light, Medium, Hard, Back, Up, Diagup, Unknown };
enum class PriTy : int8_t  { Hit, Miss, Dodge };
enum class ReactTy : int8_t { Non, High, Low, Trip, Unknown };
enum class MoveContact : int8_t { Hit, Guarded, Reversed };
enum class ProjContact : int8_t { Hit, Guarded, Cancel };

// =========================================================================
// Var — Variable storage (matches &.Var in char.ssz)
// =========================================================================
struct VarData {
	int i{};
	float f{};
	bool b{};
	VarTy ty{VarTy::SFalse};

	void setI(int v);
	void setF(float v);
	void setB(bool v);
	void setSF();
};

// =========================================================================
// ClsnHantei — Collision detection helper
// =========================================================================
struct ClsnHanteiData {
	void set(float xs1, float ys1, float xo1, float yo1, int lr1,
		float xs2, float ys2, float xo2, float yo2, int lr2);
};

// =========================================================================
// Fall — Fall state
// =========================================================================
struct FallData {
	void clear();
	void setDefault();
};

// =========================================================================
// Hitdef — Hit definition
// =========================================================================
struct HitdefData {
	int attr{}, damage{}, guardDamage{}, pausetime{};
	int getpower{}, givepower{}, p1stateno{}, p2stageno{};
	int forcenofall{}, kills{}, guardflag{}, hitflag{};
	int priority{}, id{}, chainid{}, nochainid{};
	int numhits{}, hits_per_cont{}, hitcount{};
	int fall{}, fall_x_vel{}, fall_y_vel{}, fall_recover{};
	int fall_recover_time{}, fall_damage{}, fall_kill{};
	int animtype{}, airtype{}, groundtype{}, envshake_time{};
	float envshake_freq{}, envshake_ampl{}, envshake_phase{};
	int yaccel{}, juggle{}, damageyaccel{};
	float p1_vel_x{}, p1_vel_y{}, p2_vel_x{}, p2_vel_y{};
	float p1_get_power_scale{}, p2_get_power_scale{};
	int sparkno{}, guard_sparkno{}, sparkxy{};
	int hitsound_group{}, hitsound_number{};
	int guardsound_group{}, guardsound_number{};
	int ground_sliding{}, ground_hits{};
	int air_hits{}, air_velocity_x{}, air_velocity_y{};
	int down_hits{}, down_velocity_x{}, down_velocity_y{};
	int snk_hits{}, snk_velocity_x{}, snk_velocity_y{};
	int damage_amplify{}, mindamage{}, maxdamage{};
	int damage_per_hit{};

	void clear();
	void setDefault();
	void invalidate(StTy stateType);
};

// =========================================================================
// HitBy / ByData — Hit-by tracking
// =========================================================================
struct ByData {
	int id{}, juggle{};
	void set(int id_, int juggle_);
};

struct HitByData {
	ByData by[4]; int count{};

	void clear();
	void clearOff();
	void dropByid(int id);
	void addByid(int id_, int juggle_);
};

// =========================================================================
// HitOverride
// =========================================================================
struct HitOverrideData {
	int time{}, stateno{}, attr{}, time2{};
	int forceFlag{}, pauseFlag{};
	void clear();
};

// =========================================================================
// ConfigTVars
// =========================================================================
struct ConfigTVarsData {
	int st{}, dst{}, ag{}, ge{}, rec{}, recdir{}, dis{}, inp{}, ru{}, vc{}, lb{};
	void set(int st_, int dst_, int ag_, int ge_, int rec_, int recdir_,
		int dis_, int inp_, int ru_, int vc_, int lb_);
};

// =========================================================================
// StateVal
// =========================================================================
struct StateValData {
	void clearWw();
	void clear();
	void hitCheck(HitdefData& hit, bool guard);
	void setHb(HitdefData& hit, bool guard, bool combo, int absdamage = 0);
};

// =========================================================================
// CharGlobalInfo
// =========================================================================
struct CharGlobalInfo {
	void clearPCTime();
};

// =========================================================================
// AfterImage
// =========================================================================
struct AfterImageData {
	void clear();
	void setPalcolor(int palcol);
	void setPalinvertall(bool palinv);
	void setPalbrightR(int palbrr);
	void setPalbrightG(int palbrg);
	void setPalbrightB(int palbrb);
	void setPalcontrastR(int palcor);
	void setPalcontrastG(int palcog);
	void setPalcontrastB(int palcob);
	void setupPalfx();
	void recAfterImg();
	void recAndAddAL();
};

// =========================================================================
// Explod — Explosion/effect instance
// =========================================================================
struct ExplodData {
	int id{}, posType{}, removeTime{};
	float x{}, y{}, xvel{}, yvel{};
	int sprGroup{}, sprIndex{}, bindTime{};
	int ownpal{}, remappallno{};
	int facing{1};
	// Animation state
	AnimData* anim{};
	int time{};

	void clear();
	void setX(float x_);
	void setY(float y_);
};

// =========================================================================
// Projectile
// =========================================================================
struct ProjectileData {
	int id{}, ownerId{};
	float x{}, y{}, xvel{}, yvel{};
	int hitCount{}, hitCountMax{};
	int remapPalNo{};
	bool hitFlag{}, guardFlag{};
	AnimData* anim{};

	void clear();
	void setX(float x_);
	void setY(float y_);
	void remvel();
	void update(int playerNo);
	void hitCheck(ProjectileData& pr);
	void projClsn(int playerNo);
	void tick(int playerNo);
	void anime(bool oVer, int playerNo);
};

// =========================================================================
// CharData — A single character instance
// =========================================================================
struct CharData {
	// ── Identity ──
	int playerNo{};
	std::string def;          // .def file path
	std::string name;

	// ── Position & movement ──
	float x{}, y{};
	float xvel{}, yvel{};
	float pos_x{}, pos_y{};
	int facing{1};

	// ── Physics ──
	float gravity{0.5f};
	int groundLevel{};
	int width{};

	// ── State machine ──
	int stateno{0};
	int timeInState{0};
	bool ctrl{};
	bool hitPause{};

	// ── Life / power ──
	float life{1000.0f};
	float lifeMax{1000.0f};
	int power{};

	// ── Attack / defence ──
	float attackMul{1.0f};
	float defenceMul{1.0f};

	// ── Hit / damage ──
	int hitCount{};
	int comboCount{};
	int juggleCount{};

	// ── Animation ──
	AnimData* anim{};

	// ── Command input ──
	CommandListData* cmdList{};

	// ── Hit definitions ──
	HitdefData hitdef;

	// ── Sub-elements ──
	std::vector<ExplodData> explods;
	std::vector<ProjectileData> projectiles;
	std::vector<AfterImageData> afterImages;

	// ── Lifecycle ──
	void init(const std::string& defPath, int playerNo_);
	void load(const std::string& defPath);
	void loadPallet(const std::string& defPath, int no);
	void clearDef();

	void tick();
	void action();
	void posUpdate();
	void gravityStep();
	void bind();
	void xScreenBound();

	void update();
	void posReset();
	void setLife(float l);
	void addLife(float l, bool kill, bool absolute);
	int  getDamage(double damage, bool kill, bool absolute, float atkmul);
	void setPower(int p);
	void addPower(int p);
	void setLifeMax(int l);
	void setAttack(int a);
	void setDefence(int d);
	void setFacing(int f);
	void setCtrl(bool c);
	bool canCtrl();
	void setPosX(float x_);
	void setPosY(float y_);
	void setX(float x_);
	void setY(float y_);
	void setXV(float xv);
	void setYV(float yv);

	void drawAnim();
	void furimuki();
	void posUpdateSub();

	// ── Transform helpers ──
	int  trAnimTime();
	int  trAnimElemTime(int e);
	int  trAnimElemNo(int tim);
	void trChangeAnim(int no);
	void trChangeAnim2(int no);
	void setAnimElem(int e);
	bool trAnimExist(int pid);
};

// =========================================================================
// PlayerList — List of characters
// =========================================================================
struct PlayerListData {
	std::vector<CharData*> players;

	void clear();
	CharData* get(int id);
	void add(CharData* c);
	void destroy(int id);
	void action();
	void update();
	void tick();
};

// =========================================================================
// AnimSprite — A sprite queued for rendering via the anim list system
// =========================================================================
// Matches SSZ `&AnimSprite` in char.ssz — the rendering queue entry.
// Each AnimSprite references an ActionData animation and carries all
// the transform, blending, and priority data needed for drawAnimList.
struct ActionData; // forward

struct AnimSpriteData {
	ActionData* anim{nullptr};   // Owning action (^.act.Action)
	int priority{0};
	float x{}, y{};
	float xscl{1.0f}, yscl{1.0f};
	float angle{0.0f}, axscl{1.0f}, ayscl{1.0f};
	bool padding{}, oVer{false}, screen{false}, bright{false};
	int salpha{-1}, dalpha{0};
	// PalFX pointer deferred (SSZ: ^&.com.PalFX fx)
};

// =========================================================================
// ShadowSprite — A shadow entry to be drawn after the main sprite
// =========================================================================
// Matches SSZ `&ShadowSprite` in char.ssz.
// CAUTION: `as` stores a raw pointer into the `anims` vector. Any insertion
// into `anims` may reallocate the vector and invalidate all shadow pointers.
// The SSZ managed this with reference-type semantics; native callers must
// ensure shadows are added only when the anims vector is stable.
struct ShadowSpriteData {
	AnimSpriteData* as{nullptr};
	int color{0};
	int alpha{256};
	float offsety{0.0f};
	float fadeoffset{0.0f};
};

// =========================================================================
// Module-level state
// =========================================================================

struct CharModuleState {
	CharData* chars[4]{};       // Up to 4 players
	CharGlobalInfo cgi;
	// fight, stg, etc. are opaque pointers
	int numPlayers{};

	// ── Anim list (sprite rendering queue) ──
	// SSZ: %^&AnimSprite anims, topanims;
	std::vector<AnimSpriteData> anims;
	std::vector<AnimSpriteData> topanims;
	std::vector<ShadowSpriteData> shadows;
};

// =========================================================================
// Module-level API
// =========================================================================

void char_init();
CharModuleState& char_get_state();

/// Check if the current round is over.
/// Returns true when all characters on at least one team have life <= 0
/// (KO condition) and the intro phase has completed.
bool char_round_over();

/// Determine which team won the current round.
/// Returns 0 for P1 win, 1 for P2 win, or -1 for draw.
/// Handles both KO (team still alive wins) and timeover (team with more
/// total remaining life wins; equal life = draw).
int char_round_winner();

// =========================================================================
// Drawing functions
// =========================================================================

/// Add a sprite to the anim list (sorted by priority, binary insertion).
/// SSZ: .addAnimList(anims, action, priority, x, y, screen, xs, ys, ...)
void char_add_anim_list(
	std::vector<AnimSpriteData>& list,
	ActionData* action, int priority,
	float x, float y, bool screen,
	float xscl, float yscl, float angle, bool oVer,
	float axscl, float ayscl, int salpha, int dalpha,
	bool bright);

/// Draw all sprites in an anim list.
/// SSZ: .drawAnimList(anims, x, y, scl)
void char_draw_anim_list(
	const std::vector<AnimSpriteData>& list,
	float x, float y, float scl);

/// Add a shadow sprite to the shadow list.
/// SSZ: .addShadowList(AnimSprite, color, offset, alpha, fadeoffset)
void char_add_shadow_list(
	AnimSpriteData* as, int color, float offset, int alpha, float fadeoffset);

/// Draw all shadows in the shadow list.
/// SSZ: .drawShadowList(x, y, scl)
void char_draw_shadow_list(float x, float y, float scl);

/// Draw reflections.
/// SSZ: .drawReflection(x, y, scl)
void char_draw_reflection(float x, float y, float scl);

/// Module-level draw function matching SSZ char.draw(x, y, scl).
/// Renders all players, anim lists, edge fading, and fight UI.
/// Called by fighting_service each frame with camera-adjusted coords.
/// SSZ: .chr.draw(dx, dy, dscl);
void char_draw(float x, float y, float scl);

} // namespace ikemen::ssz_native
