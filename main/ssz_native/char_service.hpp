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

#include "sff_service.hpp"  // for Rect, FrameData
#include "common_service.hpp" // for PalFXData
#include "action_service.hpp"  // for ActionData (used in actionMap)

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
// Stores and tests world-space collision rectangles transformed from
// animation frame Clsn data.  Each FrameData carries two clsn sets:
//   clsn[0] = Clsn1 (hit/attack boxes)
//   clsn[1] = Clsn2 (guard/collision boxes)
// ClsnHanteiData holds two sets of transformed world-space rects and
// provides AABB overlap testing between them.
//
// Usage for projectile-vs-character:
//   ClsnHanteiData hantei;
//   hantei.setFromFrame(0, projFrame, 0, projX, projY, facing, sx, sy);
//   hantei.setFromFrame(1, charFrame, 1, charX, charY, facing, sx, sy);
//   if (hantei.testOverlap()) { /* hit */ }
struct ClsnHanteiData {
	std::vector<Rect> clsn1;   // Set 1 world-space rects (e.g. projectile clsn1)
	std::vector<Rect> clsn2;   // Set 2 world-space rects (e.g. character clsn2)

	// ── Stored transform parameters (SSZ ClsnHantei fields) ──
	// Set by set() and consumed by testRects() / hantei().
	// xscl/yscl = scale factors, xofs/yofs = position offsets,
	// facing = -1 (left) or 1 (right) — determines L/R flip.
	float xscl1{}, yscl1{}, xofs1{}, yofs1{}; int facing1{1};
	float xscl2{}, yscl2{}, xofs2{}, yofs2{}; int facing2{1};

	void clear();

	/// SSZ-compatible parameter storage — stores scale, offset, and facing
	/// for use by testRects() (equivalent to SSZ hantei()).
	/// (xs, ys = scale; xo, yo = offset; lr = facing direction)
	void set(float xs1, float ys1, float xo1, float yo1, int lr1,
		float xs2, float ys2, float xo2, float yo2, int lr2);

	/// Populate one set of rects from an animation frame's Clsn data.
	/// @param clsnSet  0 = populate clsn1, 1 = populate clsn2
	/// @param frame    Animation frame carrying clsn[0..1] rect data
	/// @param frameClsnIdx  Which frame clsn to extract: 0=clsn1(hit), 1=clsn2(guard)
	/// @param posX, posY  Entity world position
	/// @param facing    Direction (-1 left, 1 right) — flips x for clsn rects
	/// @param scaleX, scaleY  Sprite scale for rect expansion/contraction
	void setFromFrame(int clsnSet, const FrameData& frame, int frameClsnIdx,
		float posX, float posY, int facing,
		float scaleX = 1.0f, float scaleY = 1.0f);

	/// Check if any rect in clsn1 overlaps with any in clsn2 (AABB).
	/// Returns true on first detected overlap, false if no overlap found.
	bool testOverlap() const;

	/// SSZ hantei() — test a single rect pair using stored transform params.
	/// Applies facing-aware L/R swapping, scale, and offset, then checks
	/// AABB overlap between the two transformed rects.
	/// @param c1  First Rect (local/clsn1-space, typically attacker/projectile)
	/// @param c2  Second Rect (local/clsn2-space, typically target)
	/// @return    true if the two rects overlap after transform
	bool testRects(const Rect& c1, const Rect& c2) const;

	/// Static: AABB overlap test between two world-space rectangles.
	static bool rectsOverlap(const Rect& a, const Rect& b);

	/// Static: Transform a local-space Clsn Rect into world space.
	/// Applies position offset, facing flip on x, and scale.
	static Rect toWorldRect(const Rect& local,
		float posX, float posY, int facing,
		float scaleX = 1.0f, float scaleY = 1.0f);
};

// =========================================================================
// Fall — Fall state
// =========================================================================
struct FallData {
	AnimTy animtype{AnimTy::Unknown};
	float xvelocity{NAN};
	float yvelocity{-4.5f};
	int recover{};
	int recovertime{};
	int damage{};
	int kill{};
	int envshake_time{};
	float envshake_freq{};
	int envshake_ampl{};
	float envshake_phase{};

	void clear();
	void setDefault();
};

// =========================================================================
// Hitdef — Hit definition
// =========================================================================
struct HitdefData {
	int attr{}, damage{}, guardDamage{}, pausetime{}, guard_pausetime{};
	int hitgetpower{}, guardgetpower{}, hitgivepower{}, guardgivepower{}, p1stateno{}, p2stageno{};
	int forcenofall{}, kills{}, guardflag{}, hitflag{};
	int reversal_attr{};
	int player{-1};
	int affectteam{};               // SSZ: affectteam (1 = hit enemy, -1 = hit teammate, 0 = hit both)
	int bothhittype{};              // SSZ: bothhittype (PriTy, 0=Hit, 1=Miss, 2=Dodge)
	int p2getp1state{};
	int forcestand{};
	int priority{}, id{}, chainid{}, nochainid1{}, nochainid2{};
	int numhits{}, hits_per_cont{}, hitcount{};
	int fallFlag{};                 // Whether this hit causes a fall (renamed from int fall)
	FallData fall;                  // Fall state (velocity, recovery, envshake) — matches SSZ &Hitdef.fall
	int animtype{}, air_animtype{}, airtype{}, groundtype{};
	int shaketime{}, hittime{}, ground_slidetime{};
	int guard_shaketime{}, guard_hittime{}, guard_slidetime{};
	int airguard_ctrltime{};
	float airguard_velocityx{}, airguard_velocityy{};
	int guard_ctrltime{};
	float guard_velocity{}, ground_velocityy{};
	int ground_hittime{}, air_hittime{}, down_hittime{};
	float ground_velocityx{}, air_velocityx{}, air_velocityy{};
	float down_velocityx{}, down_velocityy{};
	int down_bounce{};
	int air_fall{}, ground_fall{};
	int guard_dist{};              // SSZ: guard_dist (distance the character can block from)
	int envshake_time{};
	float envshake_freq{}, envshake_ampl{}, envshake_phase{};
	// SSZ: PalFX fields from HitDef::clear()
	int palfx_time{};
	int palfx_mulr{}, palfx_mulg{}, palfx_mulb{};
	int palfx_addr{}, palfx_addg{}, palfx_addb{};
	int palfx_amplr{}, palfx_amplg{}, palfx_amplb{};
	int palfx_cycletime{};
	int palfx_color{};
	int palfx_invertall{};
	// SSZ: cornerpush velocity offsets (one per state type, NaN = use default)
	float ground_cornerpush_veloff{};
	float air_cornerpush_veloff{};
	float down_cornerpush_veloff{};
	float guard_cornerpush_veloff{};
	float airguard_cornerpush_veloff{};
	float yaccel{NAN}, juggle{}, damageyaccel{};
	float p1_vel_x{}, p1_vel_y{}, p2_vel_x{}, p2_vel_y{};
	float p1_get_power_scale{}, p2_get_power_scale{};
	int sparkno{}, guard_sparkno{};
	float sparkx{}, sparky{};
	int hitsound_group{}, hitsound_number{};
	int guardsound_group{}, guardsound_number{};
	int p1sprpriority{};            // SSZ: p1sprpriority (attacker sprite priority offset)
	int p2sprpriority{};            // SSZ: p2sprpriority (target sprite priority offset)
	int p1getp2facing{};            // SSZ: p1getp2facing (attacker takes target's facing)
	int p1facing{};                 // SSZ: p1facing (forced attacker facing)
	int p2facing{};                 // SSZ: p2facing (forced target facing)
	int hitonce{};                  // SSZ: hitonce (attack is consumed on first hit)
	int snapt{};                    // SSZ: snapt (snap target to position, frames)
	bool guard_kill{};              // SSZ: guard_kill (can KO through guard)
	bool lhit{};                    // SSZ: lhit (last-hit flag, attack attribute auto-update)
	int ground_sliding{}, ground_hits{};
	int air_hits{}, down_hits{}, snk_hits{};
	int snk_velocity_x{}, snk_velocity_y{};
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
	int hitf1{}, hitt1{}, hitf2{}, hitt2{};
	int attr{};
	int typ{-1};
	int airanimtype{}, groundanimtype{};
	int airtype{}, groundtype{};
	int guardflag{};
	int damage{};
	int hitcount{};
	int fallcount{};
	int hitshaketime{};
	int hittime{-1};
	int slidetime{};
	int ctrltime{};
	float xvel{}, yvel{};
	float yaccel{NAN};
	int hitid{-1};
	float xoff{}, yoff{};
	FallData fall;              // Fall state from the hitdef that caused this hit
	int player{-1};
	bool fallf{};
	bool guarded{};
	bool p2getp1state{};
	bool forcestand{};

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
	HitByData hb;               // Hit-by tracking data (carried from the last hit)

	void clearWw();
	void clear();
	int hitCheck(HitdefData& hit, bool guard, StTy stateType);
	void setHb(HitdefData& hit, bool guard, bool combo, StTy stateType, int absdamage = 0);
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
// Matches SSZ `&AfterImage` in char.ssz (line 502).
// Implements the afterimage trail effect — records past frames and renders
// them with a fading palette effect cascade.
struct AfterImageImgInfo {
	int animNo{};           // Action number of captured frame
	float x{}, y{};         // World position
	float xscl{1.0f}, yscl{1.0f};
	float angle{0.0f}, axscl{1.0f}, ayscl{1.0f};
	bool oVer{false};
};

struct AfterImageData {
	// ── SSZ AfterImage fields (char.ssz lines 507-531) ──
	int time{};              // Remaining afterimage active frames
	int length{20};          // Trail length (# of captured frames to show)
	int postbrightr{}, postbrightg{}, postbrightb{};
	int addr{10}, addg{10}, addb{25};
	float mulr{0.65f}, mulg{0.65f}, mulb{0.75f};
	int timegap{1};          // Frames between captures
	int framegap{6};         // Frames between rendered ghost sprites
	int alphas{-1}, alphad{0};
	int imgidx{};            // Circular buffer write index
	int restgap{};           // Counter until next capture
	int reccount{};          // Number of captured frames in buffer
	// ── Sub-elements ──
	std::vector<AfterImageImgInfo> imgs;  // Captured frame buffer (SSZ: 64 slots)
	std::vector<PalFXData> palfx;        // Per-ghost palette effects (SSZ: AfterImageMax)

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
	void recAfterImg(float x, float y, float xs, float ys, float an, bool oVer, float ax, float ay);
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
	int ontop{};                 // Render on top layer (1 = topanims)
	int sprpriority{};           // Sprite priority for rendering order
	int supermovetime{};         // -1 = ignore super pause
	int pausemovetime{};         // -1 = ignore normal pause
	// Animation state
	AnimData* anim{};           // Pointer to active animation data (may point to sparkAction.ani)
	ActionData sparkAction;    // Owned action storage for sparks and dynamic effects
	int time{};

	void clear();
	void setX(float x_);
	void setY(float y_);
	bool isExpired() const;     // True when animation has ended and removeTime == -2
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
	bool enemy_hit{};        // SSZ: set true when projectile lands a non-guard hit
	bool enemy_guarded{};     // SSZ: set true when projectile is guarded
	int hitpause{};          // SSZ: remaining frames the projectile freezes after a hit
	AnimData* anim{};
	HitdefData hitdef;          // Per-projectile hit definition

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
	int hitPauseTime{};         // Remaining hitpause frames; decremented each tick
	StTy stateType{StTy::S};    // Standing / Crouching / Aerial / Lieing
	bool blocking{};            // Actively holding back to guard

	// ── Life / power ──
	float life{1000.0f};
	float lifeMax{1000.0f};
	int power{};

	// ── Bind state (for throws/grabs) ──
	int bindTime{};            // SSZ: sysivar[iBINDTIME] — remaining bind frames, 0 = not bound
	int bindToId{-1};          // SSZ: sysivar[iBINDTOID] — target character playerNo
	float bindPosX{NAN};       // SSZ: sysfvar[fBINDPOSX] — X offset from target, NaN = no X bind
	float bindPosY{NAN};       // SSZ: sysfvar[fBINDPOSY] — Y offset from target, NaN = no Y bind
	int bindFacing{};          // SSZ: sysivar[iBINDFACING] — 0=no facing sync, 1=mirror, 2=opposite

	// ── Attack / defence ──
	float attackMul{1.0f};
	float defenceMul{1.0f};

	// ── Hit / damage ──
	int hitCount{};
	int comboCount{};
	int juggleCount{};

	// ── Fall detection (knockdown landing tracking) ──
	bool fallPending{};          // Fall envshake is pending from last hit with fall!=0
	bool fallWasAirborne{};      // Character went airborne after fallPending was set

	// ── Animation ──
	AnimData* anim{};

	// ── Command input ──
	CommandListData* cmdList{};

	// ── Hit definitions & tracking ──
	HitdefData hitdef;
	StateValData stVal;

	// ── Palette effects ──
	PalFXData palfx;             // Active PalFX (set on hit from hitdef palfx_* fields)          // State value data (hit-by tracking, state flags)

	// ── Sub-elements ──
	std::vector<ExplodData> explods;
	std::vector<ProjectileData> projectiles;
	std::vector<AfterImageData> afterImages;

	// ── Action storage ──
	// Maps action number → ActionData for animation lookup (sparks, effects, etc.)
	// Populated during character .air loading. When full native action loading
	// is wired, actions are stored here and looked up by getAction().
	std::unordered_map<int, ActionData> actionMap;

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

	void nextRound();
	void rootInit();
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

	// ── Action lookup ──
	/// Look up an ActionData by action number.
	/// Returns pointer to stored action, or nullptr if not found.
	/// The returned pointer is valid until the owning CharData is destroyed
	/// or the next addAction() call invalidates it (map rehashing).
	ActionData* getAction(int no);

	/// Store or replace an ActionData by action number.
	/// Makes a copy internally — the caller retains ownership of the source.
	void addAction(const ActionData& action);
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
	PalFXData* fx{nullptr};      // SSZ: ^&.com.PalFX fx — palette effect for this sprite
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
	bool bright,
	PalFXData* fx = nullptr);

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

/// Create a hit spark explod matching SSZ char.hitspark() semantics.
/// SSZ: hitspark(p1=, p2=, animNo)
/// @param attacker  Character who created the attack (spark explod owner)
/// @param target    Character being hit (used for position calc)
/// @param hit       The hitdef with sparkx/sparky and sparkno
/// @param animNo    Spark action number (sparkno or guard_sparkno)
/// @param projX     Projectile X position (ignored if isProjectile=false)
/// @param isProjectile  Whether this is a projectile hit
void char_hitspark(CharData* attacker, CharData* target,
                   HitdefData& hit, int animNo,
                   float projX = 0.0f,
                   bool isProjectile = false);

/// Draw reflections.
/// SSZ: .drawReflection(x, y, scl)
void char_draw_reflection(float x, float y, float scl);

/// Process a close-range (non-projectile) attack hit from attacker onto target.
/// Handles guard detection, damage calculation, hit sound, hit spark,
/// hitpause, knockback velocity, power transfer, setHb(), and trigger_palfx().
/// SSZ equivalent: the character-to-character hit branch in projClsn() hit processing.
///
/// @param attacker  The character delivering the attack (uses hitdef, facing, attackMul)
/// @param target    The character receiving the hit (stVal.setHb, palfx, fallPending)
/// @param hitdef    The hit definition from the attacker (hitdef from hit SCTRL)
/// @param atkMul    Attack multiplier from the attacker's attackMul/attackmult
void char_attack_hit(CharData* attacker, CharData* target,
                     HitdefData& hitdef, float atkMul = 1.0f);

/// Update blocking state for all characters by checking command input.
/// Should be called once per frame before projectile collision checks.
void char_update_blocking();

/// Tick all explods across all characters — advance animations, update
/// timers, remove expired explods.  Called once per game tick from
/// fighting_main() after character updates.
void char_tick_explods();

/// Queue active explod animations into the anim list for rendering.
/// Called from char_draw() before the anim list is rendered.
void char_draw_explods(float x, float y, float scl);

/// Module-level draw function matching SSZ char.draw(x, y, scl).
/// Renders all players, anim lists, edge fading, and fight UI.
/// Called by fighting_service each frame with camera-adjusted coords.
/// SSZ: .chr.draw(dx, dy, dscl);
void char_draw(float x, float y, float scl);

} // namespace ikemen::ssz_native
