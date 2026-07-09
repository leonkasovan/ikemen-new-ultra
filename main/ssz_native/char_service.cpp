// char_service.cpp — Real implementations for char.ssz (Phase 5).
// Character state machine, animation, collision, and gameplay lifecycle.

#include "char_service.hpp"
#include "common_service.hpp"
#include "command_service.hpp"
#include "config_service.hpp"
#include "sff_service.hpp"
#include "action_service.hpp"
#include "sdlplugin_service.hpp"
#include "fight_service.hpp"
#include "stage_service.hpp"
#include "sound_resource_service.hpp"
#include "math_service.hpp"
#include <algorithm>
	#include <cmath>
	#include <cstdio>
	#include <cstring>
	#include <fstream>
	#include <limits>
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
void ClsnHanteiData::clear() {
	clsn1.clear();
	clsn2.clear();
}

void ClsnHanteiData::set(float xs1, float ys1, float xo1, float yo1, int lr1,
	float xs2, float ys2, float xo2, float yo2, int lr2)
{
	// SSZ-compatible parameter storage — stores scale, offset, and facing
	// for use by testRects() (SSZ hantei()).
	// Set 1 (e.g. attacker/projectile clsn)
	xscl1 = xs1; yscl1 = ys1; xofs1 = xo1; yofs1 = yo1; facing1 = lr1;
	// Set 2 (e.g. target clsn)
	xscl2 = xs2; yscl2 = ys2; xofs2 = xo2; yofs2 = yo2; facing2 = lr2;
}

bool ClsnHanteiData::testRects(const Rect& c1, const Rect& c2) const {
	// SSZ hantei() — tests two local-space rects using stored transform params.
	// Step 1: Apply facing-aware L/R swapping.
	// SSZ lrset1/lrset2: pLrSet when facing>=0 (l=c.l, r=c.r),
	// mLrSet when facing<0 (l=c.r, r=c.l).
	float l1, r1, l2, r2;
	if (facing1 >= 0) {
		l1 = static_cast<float>(c1.l);
		r1 = static_cast<float>(c1.r);
	} else {
		l1 = static_cast<float>(c1.r);
		r1 = static_cast<float>(c1.l);
	}
	if (facing2 >= 0) {
		l2 = static_cast<float>(c2.l);
		r2 = static_cast<float>(c2.r);
	} else {
		l2 = static_cast<float>(c2.r);
		r2 = static_cast<float>(c2.l);
	}

	// Step 2: AABB overlap check using transformed coords with scale + offset.
	// SSZ: l1*xscl1+xofs1 < r2*xscl2+xofs2
	//   && l2*xscl2+xofs2 < r1*xscl1+xofs1
	//   && c1.t*yscl1+yofs1 < (c2.b+1.0)*yscl2+yofs2
	//   && c2.t*yscl2+yofs2 < (c1.b+1.0)*yscl1+yofs1
	// Note: bottom edge uses (c2.b + 1.0) / (c1.b + 1.0) — the +1 accounts
	// for M.U.G.E.N's clsn convention where rect bottom is exclusive.
	return
		l1 * xscl1 + xofs1 < r2 * xscl2 + xofs2
		&& l2 * xscl2 + xofs2 < r1 * xscl1 + xofs1
		&& static_cast<float>(c1.t) * yscl1 + yofs1
			< (static_cast<float>(c2.b) + 1.0f) * yscl2 + yofs2
		&& static_cast<float>(c2.t) * yscl2 + yofs2
			< (static_cast<float>(c1.b) + 1.0f) * yscl1 + yofs1;
}

void ClsnHanteiData::setFromFrame(int clsnSet, const FrameData& frame, int frameClsnIdx,
	float posX, float posY, int facing,
	float scaleX, float scaleY)
{
	// Validate clsnSet (0 or 1) and frame clsn index (0 = clsn1, 1 = clsn2)
	if (clsnSet < 0 || clsnSet > 1) return;
	if (frameClsnIdx < 0 || frameClsnIdx > 1) return;
	if (frameClsnIdx >= static_cast<int>(frame.clsn.size())) return;

	// Get the target rect vector
	std::vector<Rect>& target = (clsnSet == 0) ? clsn1 : clsn2;
	target.clear();

	// Transform each local Clsn rect into world space
	const auto& localRects = frame.clsn[frameClsnIdx];
	target.reserve(localRects.size());
	for (const auto& local : localRects) {
		target.push_back(toWorldRect(local, posX, posY, facing, scaleX, scaleY));
	}
}

bool ClsnHanteiData::testOverlap() const {
	// Check every rect pair between clsn1 and clsn2
	for (const auto& a : clsn1) {
		for (const auto& b : clsn2) {
			if (rectsOverlap(a, b))
				return true;
		}
	}
	return false;
}

bool ClsnHanteiData::rectsOverlap(const Rect& a, const Rect& b) {
	// Standard AABB overlap: check that the intervals intersect on both axes.
	// Rect uses (l, t, r, b) = (left, top, right, bottom).
	// No overlap if one rect is completely to the left, right, above, or below.
	if (a.r <= b.l) return false;   // a is entirely left of b
	if (b.r <= a.l) return false;   // b is entirely left of a
	if (a.b <= b.t) return false;   // a is entirely above b
	if (b.b <= a.t) return false;   // b is entirely above a
	return true;
}

Rect ClsnHanteiData::toWorldRect(const Rect& local,
	float posX, float posY, int facing,
	float scaleX, float scaleY)
{
	Rect world;
	// Apply facing: when facing left (-1), flip the x-axis so that
	// the rect's left becomes the right side relative to entity position.
	// When facing right (1), x stays as-is.
	if (facing >= 0) {
		world.l = static_cast<int>(posX + static_cast<float>(local.l) * scaleX);
		world.r = static_cast<int>(posX + static_cast<float>(local.r) * scaleX);
	} else {
		// Flipped: mirror x around entity position
		world.l = static_cast<int>(posX - static_cast<float>(local.r) * scaleX);
		world.r = static_cast<int>(posX - static_cast<float>(local.l) * scaleX);
	}
	// Y is not flipped by facing — it always extends downward from posY
	world.t = static_cast<int>(posY + static_cast<float>(local.t) * scaleY);
	world.b = static_cast<int>(posY + static_cast<float>(local.b) * scaleY);
	return world;
}

// =========================================================================
// FallData
// =========================================================================
void FallData::clear() {
	*this = FallData{};
}

void FallData::setDefault() {
	clear();
	// SSZ Fall::setDefault(): full defaults matching &Fall in char.ssz
	animtype = AnimTy::Unknown;
	xvelocity = NAN;
	yvelocity = -4.5f;
	recover = 1;
	recovertime = 4;
	damage = 0;
	kill = 1;
	envshake_freq = 60.0f;
	envshake_ampl = -4;
	envshake_phase = NAN;
}

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
	guard_pausetime = 0;
	// SSZ: hitgetpower = .com.IERR (auto-calc from damage * Attack_LifeToPowerMul)
	//      guardgetpower = .com.IERR (auto-calc from damage * Attack_LifeToPowerMul * 0.5)
	//      hitgivepower = .com.IERR (auto-calc from damage * GetHit_LifeToPowerMul)
	//      guardgivepower = .com.IERR (auto-calc from damage * GetHit_LifeToPowerMul * 0.5)
	hitgetpower = CommonData::IERR;
	guardgetpower = CommonData::IERR;
	hitgivepower = CommonData::IERR;
	guardgivepower = CommonData::IERR;
	p1stateno = -1;
	p2stageno = -1;
	forcenofall = 0;
	kills = 1;
	guardflag = 0;
	hitflag = 0x17;          // SSZ: StTy::S|C|A|N (0x1|0x2|0x4|0x10)
	reversal_attr = 0;
	player = -1;
	p2getp1state = 0;
	forcestand = CommonData::IERR;   // SSZ: forcestand = .com.IERR
	priority = 4;            // SSZ: default priority = 4
	id = -1;
	chainid = -1;              // SSZ: chainid = -1
	nochainid1 = -1;           // SSZ: nochainid1 = -1
	nochainid2 = -1;           // SSZ: nochainid2 = -1
	numhits = 1;
	fallFlag = 0;
	fall = FallData{};
	fall.setDefault();  // Populate FallData with SSZ defaults (xvelocity=NaN, yvelocity=-4.5, envshake, etc.)
	animtype = static_cast<int>(AnimTy::Light);        // SSZ: animtype = .AnimTy::Light
	air_animtype = static_cast<int>(AnimTy::Unknown);   // SSZ: air_animtype = .AnimTy::Unknown
	// SSZ: ground_type = .ReactTy::High, air_type = .ReactTy::Unknown
	airtype = static_cast<int>(ReactTy::Unknown);
	groundtype = static_cast<int>(ReactTy::High);
	shaketime = 0;
	hittime = 0;
	ground_slidetime = 0;
	guard_shaketime = 0;
	guard_hittime = 0;
	guard_slidetime = 0;
	airguard_ctrltime = 0;
	airguard_velocityx = 0.0f;
	airguard_velocityy = 0.0f;
	guard_ctrltime = 0;
	guard_velocity = 0.0f;
	ground_velocityy = 0.0f;
	ground_hittime = 0;
	air_hittime = 20;          // SSZ: air_hittime = 20
	down_hittime = 0;
	ground_velocityx = 0.0f;
	air_velocityx = 0.0f;
	air_velocityy = 0.0f;
	down_velocityx = 0.0f;
	down_velocityy = 0.0f;
	down_bounce = 0;
	air_fall = CommonData::IERR;     // SSZ: air_fall = .com.IERR
	ground_fall = 0;
	juggle = 0;
	// yaccel stays NAN (from struct default: float yaccel{NAN}) — SSZ: yaccel = NaN
	p1_vel_x = 0; p1_vel_y = 0;
	p2_vel_x = 0; p2_vel_y = 0;
	// SSZ: cornerpush velocity offsets — default NaN (use global defaults)
	ground_cornerpush_veloff = NAN;
	air_cornerpush_veloff = NAN;
	down_cornerpush_veloff = NAN;
	guard_cornerpush_veloff = NAN;
	airguard_cornerpush_veloff = NAN;
	// SSZ: HitDef::clear() defaults
	affectteam = 1;             // SSZ: affectteam = 1 (hit enemy team)
	bothhittype = static_cast<int>(PriTy::Hit);  // SSZ: bothhittype = .PriTy::Hit
	guard_dist = 0;              // SSZ: guard_dist = 0
	p1sprpriority = 1;           // SSZ: p1sprpriority = 1
	p2sprpriority = 0;           // SSZ: p2sprpriority = 0
	p1getp2facing = 0;           // SSZ: p1getp2facing = 0
	p1facing = 0;                // SSZ: p1facing = 0
	p2facing = 0;                // SSZ: p2facing = 0
	hitonce = 0;                 // SSZ: hitonce = 0
	snapt = 0;                   // SSZ: snapt = 0
	guard_kill = true;           // SSZ: guard_kill = true
	lhit = false;                // SSZ: lhit = false
	p1_get_power_scale = 1.0f;   // SSZ: default 1.0 (100% scale)
	p2_get_power_scale = 1.0f;   // SSZ: default 1.0 (100% scale)
	sparkno = CommonData::IERR;
	guard_sparkno = CommonData::IERR;
	sparkx = 0.0f;
	sparky = 0.0f;
	hitsound_group = CommonData::IERR;
	hitsound_number = -1;
	guardsound_group = CommonData::IERR;
	guardsound_number = -1;
	ground_sliding = 0;
	ground_hits = 1;
	air_hits = 1;
	envshake_time = 0;
	envshake_freq = 60.0f;   // SSZ: envshake_freq = 60.0 (matches FallData)
	envshake_ampl = -4;        // SSZ: envshake_ampl = -4 (matches FallData)
	envshake_phase = NAN;
	// SSZ: PalFX defaults from HitDef::clear()
	palfx_time = 0;
	palfx_mulr = 256;
	palfx_mulg = 256;
	palfx_mulb = 256;
	palfx_addr = 0;
	palfx_addg = 0;
	palfx_addb = 0;
	palfx_amplr = 0;
	palfx_amplg = 0;
	palfx_amplb = 0;
	palfx_cycletime = 0;
	palfx_color = 256;
	palfx_invertall = 0;
}

void HitdefData::invalidate(StTy stateType) {
	// SSZ: `attr &= !63; `attr |= (int)stateType | (int)0x80000000;
	// Clear bottom 6 bits (attribute type), set state type bits + high bit
	attr &= ~63;
	attr |= static_cast<int>(stateType) | 0x80000000;
	// SSZ: `reversal_attr |= (int)0x80000000;
	reversal_attr |= 0x80000000;
	// SSZ: `lhit = false;
	lhit = false;
}

// =========================================================================
// HitByData
// =========================================================================
void ByData::set(int id_, int juggle_) { id = id_; juggle = juggle_; }
void HitByData::clear() {
	// SSZ &HitBy::clear(): reset all fields to defaults
	count = 0;
	hitf1 = 0; hitt1 = 0; hitf2 = 0; hitt2 = 0;
	attr = 0;
	typ = -1;
	airanimtype = 0; groundanimtype = 0;
	airtype = 0; groundtype = 0;
	guardflag = 0;
	damage = 0;
	hitcount = 0; fallcount = 0;
	hitshaketime = 0;
	hittime = -1;
	slidetime = 0;
	ctrltime = 0;
	xvel = 0.0f; yvel = 0.0f;
	yaccel = NAN;
	hitid = -1;
	fall.clear();
	player = -1;
	fallf = false;
	guarded = false;
	p2getp1state = false;
	forcestand = false;
}
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
void StateValData::clearWw() {
	// SSZ: clears wakegawakaranai boolean arrays
	// SSZ: loop{index i = 0; while; do:
	//   loop{index j = 0; while; do:
	//     `wakegawakaranai[i][j] = false;
	//     j++; while j < .wakewakaLength[i]:}
	//   i++; while i < #.wakewakaLength:}
	// Wakewaka tracking is deferred — no native equivalent yet
}
void StateValData::clear() {
	hb.clear();
}
int StateValData::hitCheck(HitdefData& hit, bool guard, StTy stateType) {
	// SSZ char.ssz line 1395-1402:
	//   ret
	//     (guard && ((int)`typ&hit.guardflag) != 0 ? 2 : 1) * (
	//       (int)(`typ == .StTy::A ? hit.air_type : hit.ground_type) == 0
	//       || hit.reversal_attr > 0
	//       ? (int)-1 : 1);
	//
	// `typ` is the character's StTy (S=0, C=1, A=2, L=3) passed as stateType.
	// guardflag bits correspond to StTy values: bit 0=Standing, 1=Crouching,
	// 2=Aerial, 3=Lieing.
	//
	// Returns:
	//   2  = guarded hit that connects
	//   1  = unguarded hit that connects
	//  -2  = guarded but reaction type is Non or reversal
	//  -1  = unguarded but reaction type is Non or reversal

	int base = (guard && (hit.guardflag & (1 << static_cast<int>(stateType))) != 0) ? 2 : 1;

	// SSZ: (int)(typ == StTy::A ? hit.air_type : hit.ground_type)
	// These are ReactTy values (Non=0, High=1, Low=2, Trip=3, Unknown=4).
	// When Non (0), the hitdef's reaction type doesn't match the character's
	// state type — the attack passes through without a hit reaction.
	int reactType = (stateType == StTy::A) ? hit.airtype : hit.groundtype;
	bool noReact = (reactType == static_cast<int>(ReactTy::Non))
		|| hit.reversal_attr > 0;

	return base * (noReact ? -1 : 1);
}

void trigger_palfx(CharData* target, const HitdefData& hit) {
	// SSZ: pyr.palfx~clear2(1);  then apply palfx_* fields from hitdef.
	// Clears the character's PalFX back to defaults, then sets each field
	// from the hitdef hit/palfx result effect parameters.
	// Only applies when palfx_time > 0 (matching SSZ hit processing).
	if (hit.palfx_time > 0) {
		PalFXData& pfx = target->palfx;
		// ── clear2(1): reset to defaults (keep enable/negType) ──
		pfx.clear2(1);
		// ── Apply hitdef palfx values ──
		// SSZ: pyr.palfx~time = hit.palfx_time;
		pfx.time = hit.palfx_time;
		// SSZ: pyr.palfx~mulr = (float)hit.palfx_mulr / 256.0;
		// In native PalFXData, mulr is int with same 0-512 scale as hitdef,
		// so direct assignment is equivalent.
		pfx.mulr = hit.palfx_mulr;
		pfx.mulg = hit.palfx_mulg;
		pfx.mulb = hit.palfx_mulb;
		pfx.addr = hit.palfx_addr;
		pfx.addg = hit.palfx_addg;
		pfx.addb = hit.palfx_addb;
		pfx.amplr = hit.palfx_amplr;
		pfx.amplg = hit.palfx_amplg;
		pfx.amplb = hit.palfx_amplb;
		pfx.cycletime = hit.palfx_cycletime;
		// SSZ: pyr.palfx~color = max(0.0, min(1.0, hit.palfx_color / 256.0))
		pfx.color = std::max(0.0f, std::min(1.0f,
			static_cast<float>(hit.palfx_color) / 256.0f));
		pfx.invertall = hit.palfx_invertall;

		// ── Immediately populate effective fields for same-frame rendering ──
		// palfx.step() already ran this frame (when time was 0, so enable=false
		// and no active→effective copy occurred).  We manually copy active→effective
		// now so that the palette transformation is visible on the frame of the hit,
		// matching SSZ semantics where the PalFX takes effect immediately.
		// This is the same active→effective copy as step() but WITHOUT the time
		// decrement (which would lose one frame of PalFX duration).
		pfx.enable = true;
		pfx.emulr = pfx.mulr;
		pfx.emulg = pfx.mulg;
		pfx.emulb = pfx.mulb;
		pfx.eaddr = pfx.addr;
		pfx.eaddg = pfx.addg;
		pfx.eaddb = pfx.addb;
		pfx.ecolor = pfx.color;
		pfx.einvertall = pfx.invertall;
		pfx.enegType = pfx.negType;

		// ── Sine-wave amplitude modulation (SSZ: sinAdd − common.ssz lines 929-940) ──
		// Applies the amplitude cycling IMMEDIATELY so the effective values already
		// include the sine-wave offset on the first frame of the hit effect.
		// Uses math::PI (double const from math_service.hpp) for cross-platform safety.
		if (pfx.cycletime >= 2) {
			double phase = (math::PI * 2.0 * static_cast<double>(pfx.sintime)
				+ (pfx.cycletime == 2 ? math::PI / 2.0 : 0.0))
				/ static_cast<double>(pfx.cycletime);
			float sinVal = static_cast<float>(std::sin(phase));
			pfx.eaddr += static_cast<int>(sinVal * static_cast<float>(pfx.amplr));
			pfx.eaddg += static_cast<int>(sinVal * static_cast<float>(pfx.amplg));
			pfx.eaddb += static_cast<int>(sinVal * static_cast<float>(pfx.amplb));
		}

		// ── Advance sintime for next frame's cycle ──
		// SSZ: if(cycletime > 0) sintime = (sintime+1) % cycletime
		// Matching step() so the sine progression stays in sync regardless of
		// when the PalFX was activated (mid-frame or at tick time).
		if (pfx.cycletime > 0) {
			pfx.sintime = (pfx.sintime + 1) % pfx.cycletime;
		}
	}
}

void trigger_envshake(HitdefData& hit) {
	// SSZ: .stage.envShake.setParams(hit.envshake_time, ...)
	// Applied at hit time for both projectile and non-projectile hits.
	// The envshake fields come from the hitdef (top-level envshake_time/freq/ampl/phase).
	// FallData envshake is handled separately by the fall landing state machine.
	if (hit.envshake_time > 0) {
		EnvShakeData& shake = stage_get_env_shake();
		shake.time = hit.envshake_time;
		shake.freq = hit.envshake_freq;
		shake.ampl = hit.envshake_ampl;
		shake.phase = hit.envshake_phase;
		shake.setDefPhase();
	}
}

// =========================================================================
// char_attack_hit — Process a close-range attack hit
// =========================================================================
// SSZ equivalent: the hit processing branch inside PlayerList::clsn() and
// Char::hitCheck() → projClsn() path.  Handles all hit mechanics for
// non-projectile attacks: guard check, damage, spark, sound, hitpause,
// knockback, power transfer, and state value update (setHb + trigger_palfx).
//
// This is the close-range counterpart to ProjectileData::projClsn().
void char_attack_hit(CharData* attacker, CharData* target,
                     HitdefData& hitdef, float atkMul)
{
	if (!attacker || !target) return;

	// ── 1. Determine guard status ──
	// SSZ: check target blocking and guardflag against state type.
	bool guarded = false;
	if (target->blocking && hitdef.guardflag) {
		if (hitdef.guardflag & (1 << static_cast<int>(target->stateType))) {
			guarded = true;
		}
	}

	// ── 2. Calculate damage ──
	float dmg = static_cast<float>(hitdef.damage) * atkMul;
	if (guarded) {
		dmg = static_cast<float>(hitdef.guardDamage) * atkMul;
	}
	// Apply target defence multiplier
	dmg /= target->defenceMul;
	// Clamp to [mindamage, maxdamage] when set
	if (hitdef.mindamage > 0 && dmg < static_cast<float>(hitdef.mindamage))
		dmg = static_cast<float>(hitdef.mindamage);
	if (hitdef.maxdamage > 0 && dmg > static_cast<float>(hitdef.maxdamage))
		dmg = static_cast<float>(hitdef.maxdamage);

	// ── 3. Apply damage to target ──
	target->life -= dmg;
	if (target->life < 0.0f) {
		bool canKill = guarded ? hitdef.guard_kill : (hitdef.kills != 0);
		if (canKill) {
			target->life = 0.0f;
		} else {
			target->life = 1.0f;
		}
	}

	// ── 4. Power transfer ──
	// SSZ: by.addPower(hit.hitgetpower) / pyr.addPower(hit.hitgivepower)
	// Auto-calculate from damage when field is IERR sentinel.
	{
		const ConfigData& cfg = config_get_state();
		int atkPower, defPower;
		if (guarded) {
			atkPower = (hitdef.guardgetpower != CommonData::IERR)
				? hitdef.guardgetpower
				: static_cast<int>(cfg.Attack_LifeToPowerMul
					* static_cast<float>(hitdef.damage) * 0.5f);
			defPower = (hitdef.guardgivepower != CommonData::IERR)
				? hitdef.guardgivepower
				: static_cast<int>(cfg.GetHit_LifeToPowerMul
					* static_cast<float>(hitdef.damage) * 0.5f);
		} else {
			atkPower = (hitdef.hitgetpower != CommonData::IERR)
				? hitdef.hitgetpower
				: static_cast<int>(cfg.Attack_LifeToPowerMul
					* static_cast<float>(hitdef.damage));
			defPower = (hitdef.hitgivepower != CommonData::IERR)
				? hitdef.hitgivepower
				: static_cast<int>(cfg.GetHit_LifeToPowerMul
					* static_cast<float>(hitdef.damage));
		}
		atkPower = static_cast<int>(static_cast<float>(atkPower) * hitdef.p1_get_power_scale);
		defPower = static_cast<int>(static_cast<float>(defPower) * hitdef.p2_get_power_scale);
		attacker->addPower(atkPower);
		target->addPower(defPower);
	}

	// ── 5. Hitpause on target ──
	// SSZ: pyr.hitPause = true; pause for pausetime/guard_pausetime frames.
	if (guarded) {
		if (hitdef.guard_pausetime > 0) {
			target->hitPauseTime = hitdef.guard_pausetime;
			target->hitPause = true;
		}
	} else {
		if (hitdef.pausetime > 0) {
			target->hitPauseTime = hitdef.pausetime;
			target->hitPause = true;
		}
	}

	// ── 6. State change ──
	// SSZ: pyr.stateno = hit.p2stageno;
	if (hitdef.p2stageno >= 0) {
		target->stateno = hitdef.p2stageno;
		target->timeInState = 0;
	}

	// ── 7. Knockback velocity ──
	// SSZ: pyr.xvel = hit.p2_vel_x * pyr~facing;
	//      pyr.yvel = hit.p2_vel_y;
	// p2_vel_x is relative to the ATTACKER's facing direction.
	float ownerFacing = static_cast<float>(attacker->facing);
	target->xvel = hitdef.p2_vel_x * ownerFacing;
	target->yvel = hitdef.p2_vel_y;

	// ── 8. Hit spark ──
	// SSZ: branches on reversal_attr to determine spark owner vs action source.
	// Non-guard: if(sparkno != .com.IERR) hitspark(...)
	// Guard:     if(guard_sparkno != .com.IERR) hitspark(...)
	{
		bool isReversal = (hitdef.reversal_attr > 0);
		if (!guarded) {
			if (hitdef.sparkno != CommonData::IERR) {
				CharData* sparkOwner = isReversal ? target : attacker;
				CharData* sparkActionSrc = isReversal ? attacker : target;
				char_hitspark(sparkOwner, sparkActionSrc, hitdef,
					hitdef.sparkno, 0.0f, false);
			}
		} else {
			if (hitdef.guard_sparkno != CommonData::IERR) {
				CharData* sparkOwner = isReversal ? target : attacker;
				CharData* sparkActionSrc = isReversal ? attacker : target;
				char_hitspark(sparkOwner, sparkActionSrc, hitdef,
					hitdef.guard_sparkno, 0.0f, false);
			}
		}
	}

	// ── 9. Hit sound ──
	// SSZ: hitsoundg/guardsoundg routing to fight or character soundbank.
	{
		int sndGroup = guarded ? hitdef.guardsound_group : hitdef.hitsound_group;
		int sndNumber = guarded ? hitdef.guardsound_number : hitdef.hitsound_number;
		if (sndGroup != CommonData::IERR) {
			SoundChannel* ch = get_channel(-1);
			if (ch) {
				const WaveData* wav = nullptr;
				if (sndGroup < 0) {
					wav = sound_table_get_sound(-sndGroup, sndNumber);
				} else {
					wav = sound_table_get_sound(sndGroup, sndNumber);
				}
				if (wav) {
					ch->wave = wav;
					ch->setPan(target->x);
				}
			}
			if (ch && ch->wave) {
				play_sound();
			}
		}
	}

	// ── 10. setHb() + trigger_palfx() + fallPending ──
	// SSZ: stVal.setHb(hit=, guard, combo, absdamage)
	// Combo detection: true when the same attacker hitdef id repeats.
	{
		bool combo = (target->stVal.hb.hitid == hitdef.id);
		target->stVal.setHb(hitdef, guarded, combo,
			target->stateType, 0);
		// trigger_palfx handles the palfx_time > 0 check internally
		trigger_palfx(target, hitdef);
		target->fallPending = true;
		target->fallWasAirborne = false;
	}

	// ── 11. Attacker hit tracking (p1 facing, p1sprpriority) ──
	// SSZ: if(hit.p1getp2facing != 0) attacker takes target's facing
	//      if(hit.p1facing != 0) forced attacker facing
	//      p1sprpriority/p2sprpriority handled by anim list priority
	if (hitdef.p1getp2facing != 0) {
		attacker->facing = target->facing;
	}
	if (hitdef.p1facing != 0) {
		attacker->facing = hitdef.p1facing;
	}
	if (hitdef.p2facing != 0) {
		target->facing = hitdef.p2facing;
	}
}

void StateValData::setHb(HitdefData& hit, bool guard, bool combo, StTy stateType, int absdamage) {
	(void)absdamage;
	// SSZ char.ssz setHb() — copies hitdef fields into hb for hit-by tracking.
	// Saves state bits that survive hb.clear() (by data, fall flag, counters)
	// then restores them after clearing.
	//
	// `stateType` is the character's StTy (S=0, C=1, A=2, L=3) at the time
	// of the hit — used for guardflag check and per-type velocity/timing selection.

	// ── Save state that survives clear() ──
	bool cmb = combo && !hb.guarded;
	bool fall = hb.fallf;
	int hc = hb.hitcount, fc = hb.fallcount;
	ByData savedBy[4];
	int savedByCount = hb.count;
	for (int i = 0; i < savedByCount && i < 4; i++)
		savedBy[i] = hb.by[i];

	// ── Reset, then restore by-data ──
	hb.clear();
	hb.count = savedByCount;
	for (int i = 0; i < savedByCount && i < 4; i++)
		hb.by[i] = savedBy[i];

	// ── Copy hitdef fields to hb ──
	hb.attr = hit.attr;
	hb.hitid = hit.id;
	hb.player = hit.player;
	hb.p2getp1state = hit.p2getp1state != 0;
	hb.forcestand = hit.forcestand != 0;
	hb.fall = hit.fall;
	hb.fallf = hit.fallFlag != 0;
	// `fallTime = 0 (on StateVal, not hb — set by caller or state machine)
	hb.yaccel = hit.yaccel;            // float → float, direct assign
	if (hit.forcenofall) fall = false;
	hb.guardflag = hit.guardflag;
	hb.groundtype = hit.groundtype;
	hb.airtype = hit.airtype;
	// SSZ: hb.typ = typ == StTy::A ? hb.airtype : hb.groundtype
	hb.typ = (stateType == StTy::A) ? hit.airtype : hit.groundtype;
	hb.airanimtype = hit.air_animtype;    // SSZ: (int)hit.air_animtype
	hb.groundanimtype = hit.animtype;     // SSZ: (int)hit.animtype

	// ── Screen shake (SSZ: applied after both guard and hit branches) ──
	// Trigger envshake from the hitdef's top-level envshake_time/freq/ampl/phase.
	// This runs once per hit, regardless of guard status, matching SSZ's common
	// section placement after both the guard and hit branch complete.
	trigger_envshake(hit);

	// ── Guard branch ──
	// SSZ: if(guard && ((int)`typ & hit.guardflag) != 0) where `typ is StTy
	//       (S=0, C=1, A=2, L=3). guardflag bits correspond to StTy values.
	if (guard && (hit.guardflag & (1 << static_cast<int>(stateType))) != 0) {
		// SSZ: hitshaketime = max(0, hit.guard_shaketime)
		hb.hitshaketime = (hit.guard_shaketime > 0) ? hit.guard_shaketime : 0;
		// SSZ: hittime = max(0, hit.guard_hittime)
		hb.hittime = (hit.guard_hittime > 0) ? hit.guard_hittime : 0;
		// SSZ: slidetime = hit.guard_slidetime
		hb.slidetime = hit.guard_slidetime;
		hb.guarded = true;

		// SSZ: branch{ cond typ == StTy::A → airguard; else → ground guard }
		if (stateType == StTy::A) {
			// SSZ: ctrltime = hit.airguard_ctrltime
			hb.ctrltime = hit.airguard_ctrltime;
			// SSZ: xvel = hit.airguard_velocityx, yvel = hit.airguard_velocityy
			hb.xvel = hit.airguard_velocityx;
			hb.yvel = hit.airguard_velocityy;
		} else {
			// SSZ: ctrltime = hit.guard_ctrltime
			hb.ctrltime = hit.guard_ctrltime;
			// SSZ: xvel = hit.guard_velocity, yvel = hit.ground_velocityy
			hb.xvel = hit.guard_velocity;
			hb.yvel = hit.ground_velocityy;
		}
		// SSZ: absdamage = hit.guarddamage (used by caller)
		hb.hitcount = hc;
		return;
	}

	// ── Non-guard branch ──
	// SSZ: hitshaketime = max(0, hit.shaketime)
	hb.hitshaketime = (hit.shaketime > 0) ? hit.shaketime : 0;
	// SSZ: hittime = max(0, hit.hittime)
	hb.hittime = (hit.hittime > 0) ? hit.hittime : 0;
	// SSZ: slidetime = hit.ground_slidetime
	hb.slidetime = hit.ground_slidetime;

	// SSZ: branch{ cond typ == StTy::A; cond typ == StTy::L; else }
	// `stateType` parameter provides the character's actual StTy at hit time.
	if (stateType == StTy::A) {
		// SSZ: hittime = hit.air_hittime, ctrltime = hit.air_hittime
		hb.hittime = (hit.air_hittime > 0) ? hit.air_hittime : 0;
		hb.ctrltime = hb.hittime;
		hb.xvel = hit.air_velocityx;
		hb.yvel = hit.air_velocityy;
		// SSZ: fallf = hit.air_fall != 0
		hb.fallf = (hit.air_fall != 0);
	} else if (stateType == StTy::L) {
		// SSZ: hittime = hit.down_hittime, ctrltime = hit.down_hittime
		hb.hittime = (hit.down_hittime > 0) ? hit.down_hittime : 0;
		hb.ctrltime = hb.hittime;
		// SSZ: if(hit.down_bounce == 0) xvel/yvel = hit.down_velocityx/y
		//      else keep existing velocity (bounce — preserve momentum)
		if (hit.down_bounce == 0) {
			hb.xvel = hit.down_velocityx;
			hb.yvel = hit.down_velocityy;
		}
	} else {
		// SSZ (default): ground reaction (S, C, N, U)
		// SSZ: hittime = hit.ground_hittime, ctrltime = hit.ground_hittime
		hb.hittime = (hit.ground_hittime > 0) ? hit.ground_hittime : 0;
		hb.ctrltime = hb.hittime;
		hb.xvel = hit.ground_velocityx;
		hb.yvel = hit.ground_velocityy;
		// SSZ: fallf = hit.ground_fall != 0
		hb.fallf = (hit.ground_fall != 0);
	}

	// Clamp hittime to non-negative
	if (hb.hittime < 0) hb.hittime = 0;

	// ── Hit counters (SSZ: fallf determines fallcount increment) ──
	if (hb.fallf) {
		hb.hitcount = cmb ? hc + 1 : 1;
		hb.fallcount = fc + 1;
	} else {
		hb.hitcount = cmb ? hc + 1 : 1;
		hb.fallcount = fc;
	}
	hb.fallf |= fall;
}

// =========================================================================
// CharGlobalInfo
// =========================================================================
void CharGlobalInfo::clearPCTime() {
	// SSZ: `pctyp = .ProjContact::Hit; `pctime = -1; `pcid = 0;
	// ProjContact is stored as fields on CharGlobalInfo — reset to defaults
}

// =========================================================================
// AfterImageData
// =========================================================================

void AfterImageData::clear() {
	// SSZ AfterImage::clear() (char.ssz lines 547-575):
	//   `time = 0; `length = 20;
	//   if(palfx exists) set palfx~ecolor=1.0, einvertall=0, eaddr=30, eaddg=30, eaddb=30,
	//     emulr=120, emulg=120, emulb=220
	//   `postbrightr = 0; `postbrightg = 0; `postbrightb = 0;
	//   `addr = 10; `addg = 10; `addb = 25;
	//   `mulr = 0.65; `mulg = 0.65; `mulb = 0.75;
	//   `timegap = 1; `framegap = 6; `alphas = -1; `alphad = 0;
	//   `imgidx = 0; `restgap = 0; `reccount = 0;
	time = 0;
	length = 20;
	// SSZ: palfx is pre-allocated by the AfterImage constructor via
	// palfx.new(.cfg.AfterImageMax). Allocate if not yet done.
	if (palfx.empty()) {
		palfx.resize(64); // SSZ default: AfterImageMax
	}
	// Initialize the first PalFX element with SSZ defaults
	palfx[0].color = 1.0f;
	palfx[0].invertall = 0;
	palfx[0].addr = 30;
	palfx[0].addg = 30;
	palfx[0].addb = 30;
	palfx[0].mulr = 120;
	palfx[0].mulg = 120;
	palfx[0].mulb = 220;
	postbrightr = 0; postbrightg = 0; postbrightb = 0;
	addr = 10; addg = 10; addb = 25;
	mulr = 0.65f; mulg = 0.65f; mulb = 0.75f;
	timegap = 1;
	framegap = 6;
	alphas = -1;
	alphad = 0;
	imgidx = 0;
	restgap = 0;
	reccount = 0;
	// Ensure imgs buffer has at least 64 slots (SSZ: fixed at 64)
	if (imgs.size() < 64) {
		imgs.resize(64);
	}
}

void AfterImageData::setPalcolor(int palcol) {
	// SSZ AfterImage::setPalcolor() (char.ssz lines 578-583):
	//   if(palfx > 0) palfx~ecolor = (float)palcol / 256.0; limRange(0.0, 1.0)
	if (!palfx.empty()) {
		palfx[0].color = std::max(0.0f, std::min(1.0f,
			static_cast<float>(palcol) / 256.0f));
	}
}

void AfterImageData::setPalinvertall(bool palinv) {
	// SSZ: if(palfx > 0) palfx~einvertall = (int)palinv;
	if (!palfx.empty()) {
		palfx[0].invertall = palinv ? 1 : 0;
	}
}

void AfterImageData::setPalbrightR(int palbrr) {
	// SSZ: if(palfx > 0) palfx~eaddr = palbrr;
	if (!palfx.empty()) palfx[0].addr = palbrr;
}

void AfterImageData::setPalbrightG(int palbrg) {
	if (!palfx.empty()) palfx[0].addg = palbrg;
}

void AfterImageData::setPalbrightB(int palbrb) {
	if (!palfx.empty()) palfx[0].addb = palbrb;
}

void AfterImageData::setPalcontrastR(int palcor) {
	// SSZ: if(palfx > 0) palfx~emulr = palcor;
	if (!palfx.empty()) palfx[0].mulr = palcor;
}

void AfterImageData::setPalcontrastG(int palcog) {
	if (!palfx.empty()) palfx[0].mulg = palcog;
}

void AfterImageData::setPalcontrastB(int palcob) {
	if (!palfx.empty()) palfx[0].mulb = palcob;
}

void AfterImageData::setupPalfx() {
	// SSZ AfterImage::setupPalfx() (char.ssz lines 613-631):
	// Propagates palfx[0] values through the palfx array with additive
	// accumulation for eaddr/eaddg/eaddb and multiplicative decay for emulr/emulg/emulb.
	//   loop{ i = 1; pbr = postbrightr, pbg = postbrightg, pbb = postbrightb;
	//   do:
	//     palfx[i].ecolor = palfx[i-1].ecolor;
	//     palfx[i].einvertall = palfx[i-1].einvertall;
	//     palfx[i].eaddr = palfx[i-1].eaddr + addr + pbr;
	//     palfx[i].eaddg = palfx[i-1].eaddg + addg + pbg;
	//     palfx[i].eaddb = palfx[i-1].eaddb + addb + pbb;
	//     pbr = pbg = pbb = 0;
	//     palfx[i].emulr = (int)(palfx[i-1].emulr * mulr);
	//     palfx[i].emulg = (int)(palfx[i-1].emulg * mulg);
	//     palfx[i].emulb = (int)(palfx[i-1].emulb * mulb);
	//     i++; while i < palfx.size()
	if (palfx.size() < 2) return;
	int pbr = postbrightr, pbg = postbrightg, pbb = postbrightb;
	for (size_t i = 1; i < palfx.size(); i++) {
		palfx[i].color = palfx[i - 1].color;
		palfx[i].invertall = palfx[i - 1].invertall;
		palfx[i].addr = palfx[i - 1].addr + addr + pbr;
		palfx[i].addg = palfx[i - 1].addg + addg + pbg;
		palfx[i].addb = palfx[i - 1].addb + addb + pbb;
		pbr = pbg = pbb = 0;
		palfx[i].mulr = static_cast<int>(static_cast<float>(palfx[i - 1].mulr) * mulr);
		palfx[i].mulg = static_cast<int>(static_cast<float>(palfx[i - 1].mulg) * mulg);
		palfx[i].mulb = static_cast<int>(static_cast<float>(palfx[i - 1].mulb) * mulb);
	}
}

void AfterImageData::recAfterImg(float x, float y, float xs, float ys,
	float an, bool oVer, float ax, float ay)
{
	// SSZ AfterImage::recAfterImg() (char.ssz lines 632-696):
	// Records current frame data into circular buffer at imgidx.
	// Uses timegap to control capture rate (restgap counter).
	if (time == 0) {
		reccount = 0;
		restgap = 0;
		return;
	}
	if (time > 0) time--;
	if (restgap <= 0) {
		// Record current state into buffer
		if (static_cast<size_t>(imgidx) < imgs.size()) {
			AfterImageImgInfo& info = imgs[imgidx];
			info.x = x;
			info.y = y;
			info.xscl = xs;
			info.yscl = ys;
			info.angle = an;
			info.oVer = oVer;
			info.axscl = ax;
			info.ayscl = ay;
		}
		imgidx = (imgidx + 1) & 63;  // Circular buffer wraps at 64
		if (reccount < static_cast<int>(imgs.size()))
			reccount++;
		restgap = timegap;
	}
	if (restgap > 0) restgap--;
}

void AfterImageData::recAndAddAL() {
	// SSZ AfterImage::recAndAddAL() (char.ssz lines 697-767):
	// Adds captured frames to the anim list as ghost sprites with
	// per-ghost palette effects from the palfx cascade.
	// Stub: rendering ghosts requires addAnimList which needs ActionData*
	// (AnimSpriteData), not AfterImageImgInfo directly. Deferred until
	// the rendering pipeline fully supports afterimages.
}

// =========================================================================
// ExplodData
// =========================================================================
void ExplodData::clear() { *this = ExplodData{}; }
void ExplodData::setX(float x_) { x = x_; }
void ExplodData::setY(float y_) { y = y_; }

bool ExplodData::isExpired() const {
	// SSZ semantics for removeTime:
	//   -2 = remove when animation ends (loopend)
	//   -1 = never auto-remove
	//    0 = remove immediately
	//   >0 = countdown frames remaining (decremented in char_tick_explods)
	if (removeTime == 0) return true;
	if (removeTime < 0) {
		if (removeTime == -2)
			return anim && anim->loopend;
		return false; // -1 = never auto-remove
	}
	// positive = still has frames left
	return false;
}

// =========================================================================
// ProjectileData
// =========================================================================
void ProjectileData::clear() { *this = ProjectileData{}; }
void ProjectileData::setX(float x_) { x = x_; }
void ProjectileData::setY(float y_) { y = y_; }
void ProjectileData::remvel() { xvel = 0; yvel = 0; }
void ProjectileData::update(int playerNo) {
	// Frame-step physics: apply velocity to position
	x += xvel;
	y += yvel;
	// Also decrement lifetime counters each frame
	tick(playerNo);
}

void ProjectileData::hitCheck(ProjectileData& pr) {
	// Projectile-vs-projectile collision detection using Clsn rects from
	// each projectile's current animation frame.  Each projectile uses
	// its frame's clsn[1] (Clsn2 / guard boxes) as the collision bounds.
	// If either projectile has expired or has no frame, fall back to
	// sprite-dimension AABB as before.
	if (hitCount <= 0 || pr.hitCount <= 0) return;
	if (!anim || !pr.anim) return;

	// Get current frames from both projectiles
	FrameData* frame = anim->currentFrame();
	FrameData* prFrame = pr.anim->currentFrame();

	// Bail if either has no loaded frame
	if (!frame || !prFrame) {
		// Ensure sprites are loaded for fallback AABB
		if (!anim->spr) anim->updateSprite();
		if (!pr.anim->spr) pr.anim->updateSprite();
		if (!anim->spr || !pr.anim->spr) return;
		// Fallback: sprite-dimension AABB
		float fbl = x;
		float fbr = x + static_cast<float>(anim->spr->rct_w);
		float fbt = y;
		float fbb = y + static_cast<float>(anim->spr->rct_h);
		float sbl = pr.x;
		float sbr = pr.x + static_cast<float>(pr.anim->spr->rct_w);
		float sbt = pr.y;
		float sbb = pr.y + static_cast<float>(pr.anim->spr->rct_h);
		if (fbl < sbr && fbr > sbl && fbt < sbb && fbb > sbt) {
			hitCount = 0;
			pr.hitCount = 0;
		}
		return;
	}

	// Use Clsn rects from frames (clsn[1] = Clsn2 = guard/collision boxes)
	// Projectiles use Clsn2 as their collision bounds (what can be hit).
	int facing = 1;  // Projectile facing — defaults to right; SSZ can set
	                 // per projectile, but for now all projectiles face right.

	ClsnHanteiData hantei;
	hantei.setFromFrame(0, *frame, 1, x, y, facing);
	hantei.setFromFrame(1, *prFrame, 1, pr.x, pr.y, facing);

	if (hantei.testOverlap()) {
		// Both projectiles cancel each other on collision
		hitCount = 0;
		pr.hitCount = 0;
	}
}

void ProjectileData::projClsn(int playerNo) {
	// Projectile-vs-character collision detection using Clsn rects from
	// the projectile's and character's current animation frames.
	//
	// Collision model:
	//   Projectile uses clsn[1] (Clsn2, guard/collision boxes) as its
	//   collision bounds — these define what area of the projectile can
	//   contact the opponent.
	//   Character uses clsn[1] (Clsn2) as the target hitbox — in M.U.G.E.N,
	//   Clsn2 defines the area that can be hit by attacks.
	//
	// Hit processing uses this->hitdef (set by the projectile SCTRL at
	// spawn time) for damage, hitflag/guardflag filtering, pausetime,
	// hit sound, hit spark, and knockback velocity.
	if (hitCount <= 0) return;
	if (!anim) return;

	// Get projectile's current frame for Clsn data
	FrameData* projFrame = anim->currentFrame();
	if (!projFrame) {
		// Fallback: sprite AABB (no frame data available)
		if (!anim->spr) anim->updateSprite();
		if (!anim->spr) return;
	}

	// ── Get owner's attack multiplier ──
	float atkMul = 1.0f;
	CharData* owner = nullptr;
	if (playerNo >= 0 && playerNo < 4 && g_char_state.chars[playerNo]) {
		owner = g_char_state.chars[playerNo];
		atkMul = owner->attackMul;
	}

	// ── Iterate opposing team characters ──
	// Team assignment: even slots (0, 2) = Team 0 (P1),
	// odd slots (1, 3) = Team 1 (P2).
	// Owner's team = playerNo & 1, so we skip characters where
	// (i & 1) == (playerNo & 1).
	for (int i = 0; i < 4; i++) {
		// Stop early if projectile has been fully consumed
		if (hitCount <= 0) break;

		CharData* target = g_char_state.chars[i];
		if (!target) continue;

		// Skip dead characters
		if (target->life <= 0.0f) continue;

		// Skip owner's team
		if ((i & 1) == (playerNo & 1)) continue;

		// Skip if target has no animation
		if (!target->anim) continue;

		// ── hitflag check (bitmask of StTy values the projectile can hit) ──
		// SSZ: if((hit.hitflag & (int)`stVal.typ) == 0) ret false;
		// The target's stateType (StTy) is set by the character state machine.
		// If the projectile's hitflag doesn't include the target's current
		// state type, the projectile can't hit (e.g. a low-hitting projectile
		// won't connect with an airborne opponent if StTy::A bit is not set).
		if (!(hitdef.hitflag & (1 << static_cast<int>(target->stateType))))
			continue;

		// ── Compute collision using Clsn rects ──
		bool hit = false;

		// Get target's current frame for Clsn data
		FrameData* tgtFrame = target->anim->currentFrame();

		if (projFrame && tgtFrame) {
			// ── Precise: Clsn rect overlap ──
			ClsnHanteiData hantei;
			hantei.setFromFrame(0, *projFrame, 1, x, y, 1);
			hantei.setFromFrame(1, *tgtFrame, 1,
				target->x, target->y, target->facing);
			hit = hantei.testOverlap();
		} else {
			// ── Fallback: sprite-dimension AABB ──
			if (projFrame && !anim->spr) anim->updateSprite();
			if (!tgtFrame && !target->anim->spr) target->anim->updateSprite();
			if (!anim->spr || !target->anim->spr) continue;

			float pl = x;
			float prb = x + static_cast<float>(anim->spr->rct_w);
			float pt = y;
			float pb = y + static_cast<float>(anim->spr->rct_h);

			float halfW = static_cast<float>(target->anim->spr->rct_w) * 0.5f;
			float cl = target->x - halfW;
			float cr = target->x + halfW;
			float ct = target->y - static_cast<float>(target->anim->spr->rct_h);
			float cb = target->y;

			if (pl < cr && prb > cl && pt < cb && pb > ct)
				hit = true;
		}

		if (hit) {
			// ── Determine guard status ──
			// SSZ: `pyr.stVal.typ & hit.guardflag check and guard animation path.
			// A character is guarding when they are blocking (holding back) AND
			// the projectile's guardflag bitmask matches their current state type.
			// If guardflag doesn't include the target's state type (e.g. guardflag
			// lacks StTy::A bit for an airborne target), the attack is unguardable
			// — it still hits but can't be blocked.
			bool guarded = false;
			if (target->blocking && hitdef.guardflag) {
				// Check if this attack type can be guarded from the target's
				// current state type (standing/crouching/aerial/lieing).
				// SSZ: ((int)`pyr.stVal.typ&hit.guardflag) == 0 → unguardable
				if (hitdef.guardflag & (1 << static_cast<int>(target->stateType))) {
					guarded = true;
				}
				// If guardflag doesn't cover this state type, the attack is
				// unguardable — projectile still hits through the guard attempt.
			}

			// ── Calculate damage ──
			// Base damage from the projectile's hitdef, scaled by the owner's
			// attack multiplier and the target's defence multiplier.
			float dmg = static_cast<float>(hitdef.damage) * atkMul;
			if (guarded) {
				dmg = static_cast<float>(hitdef.guardDamage) * atkMul;
			}

			// Apply target defence multiplier
			dmg /= target->defenceMul;

			// Clamp damage to [mindamage, maxdamage] when set
			if (hitdef.mindamage > 0 && dmg < static_cast<float>(hitdef.mindamage))
				dmg = static_cast<float>(hitdef.mindamage);
			if (hitdef.maxdamage > 0 && dmg > static_cast<float>(hitdef.maxdamage))
				dmg = static_cast<float>(hitdef.maxdamage);

			// Apply damage to target
			target->life -= dmg;
			if (target->life < 0.0f) {
				if (hitdef.kills) {
					// Allow KO — life can go to 0
					target->life = 0.0f;
				} else {
					// Non-lethal: clamp to 1 to prevent KO
					target->life = 1.0f;
				}
			}

			// ── Power transfer (SSZ lines 6418-6450) ──
			// SSZ: by.addPower(hit.hitgetpower) / by.addPower(hit.guardgetpower)
			//      pyr.addPower(hit.hitgivepower) / pyr.addPower(hit.guardgivepower)
			// Uses SSZ fields: hitgetpower/guardgetpower (attacker gain),
			// hitgivepower/guardgivepower (target gain).
			// Auto-calculate from damage when field is IERR sentinel
			// (matches SSZ hitdefSetDefault_() lines 3988-4001).
			{
				const ConfigData& cfg = config_get_state();
				int atkPower;
				int defPower;
				if (guarded) {
					atkPower = (hitdef.guardgetpower != CommonData::IERR)
						? hitdef.guardgetpower
						: static_cast<int>(cfg.Attack_LifeToPowerMul
							* static_cast<float>(hitdef.damage) * 0.5f);
					defPower = (hitdef.guardgivepower != CommonData::IERR)
						? hitdef.guardgivepower
						: static_cast<int>(cfg.GetHit_LifeToPowerMul
							* static_cast<float>(hitdef.damage) * 0.5f);
				} else {
					atkPower = (hitdef.hitgetpower != CommonData::IERR)
						? hitdef.hitgetpower
						: static_cast<int>(cfg.Attack_LifeToPowerMul
							* static_cast<float>(hitdef.damage));
					defPower = (hitdef.hitgivepower != CommonData::IERR)
						? hitdef.hitgivepower
						: static_cast<int>(cfg.GetHit_LifeToPowerMul
							* static_cast<float>(hitdef.damage));
				}
				// SSZ: apply hitdef power scale multipliers
				// p1_get_power_scale scales the attacker's power gain
				// p2_get_power_scale scales the defender's power gain
				// Default 1.0f = 100%, set via setDefault()
				atkPower = static_cast<int>(static_cast<float>(atkPower) * hitdef.p1_get_power_scale);
				defPower = static_cast<int>(static_cast<float>(defPower) * hitdef.p2_get_power_scale);
				if (owner) {
					owner->addPower(atkPower);
				}
				target->addPower(defPower);
			}

			// ── Hitpause (target + projectile) ──
			// SSZ: pyr.hitPause = true; proj.hitpause = max(0, hit.pausetime/guard_pausetime)
			// The target pauses for pausetime frames (affects animation, input).
			// The projectile also pauses (hitpause) to prevent it from hitting
			// additional targets during the same pause window.
			if (hitdef.pausetime > 0) {
				target->hitPauseTime = hitdef.pausetime;
				target->hitPause = true;
			}
			// SSZ: hitpause = max(0, hit.pausetime) for non-guard;
			//       hitpause = max(0, hit.guard_pausetime) for guard.
			// These are completely separate branches — guard never falls through
			// to hit pausetime when guard_pausetime is 0.
			hitpause = guarded
				? (hitdef.guard_pausetime > 0 ? hitdef.guard_pausetime : 0)
				: (hitdef.pausetime > 0 ? hitdef.pausetime : 0);
			// SSZ: proj.timemiss = !.m.max!int?(0, proj.misstime) — reset miss timer after hit
			// Simplified: re-arm miss timer to 0 so projectile can continue hitting
			// (actual misstime tracking deferred until ProjectileData.misstime is wired)

			// ── State change ──
			// SSZ: pyr.stateno = hit.p2stageno;
			if (hitdef.p2stageno >= 0) {
				target->stateno = hitdef.p2stageno;
				target->timeInState = 0;
			}

			// ── Knockback velocity ──
			// Apply hitdef p2_vel_x/p2_vel_y to target.
			// p2_vel_x is defined relative to the ATTACKER's facing direction
			// (the character who fired the projectile), not the target's.
			// SSZ: pyr.xvel = hit.p2_vel_x * pyr~facing;
			// SSZ: pyr.yvel = hit.p2_vel_y;
			float ownerFacing = owner ? static_cast<float>(owner->facing) : 1.0f;
			target->xvel = hitdef.p2_vel_x * ownerFacing;
			target->yvel = hitdef.p2_vel_y;

			// ── SSZ enemy_hit / enemy_guarded spark state machine ──
			// SSZ lines 6393-6428: branches on hit vs guard, then on
			// reversal_attr to determine spark owner vs action source.
			//
			// Non-guard (hitting == 1):
			//   if(hit.sparkno != .com.IERR) branch{
			//   cond hit.reversal_attr > 0:
			//     hitspark(`pyr=, by=, hit.sparkno);  // p1=pyr(target), p2=by(owner)
			//   else:
			//     hitspark(by=, `pyr=, hit.sparkno);  // p1=by(owner), p2=pyr(target)
			//   }
			// Guard:
			//   if(hit.guard_sparkno != .com.IERR) branch{
			//   cond hit.reversal_attr > 0:
			//     hitspark(`pyr=, by=, hit.guard_sparkno);
			//   else:
			//     hitspark(by=, `pyr=, hit.guard_sparkno);
			//   }
			// Track enemy_hit/enemy_guarded for projectile state machine.
			{
				bool isReversal = (hitdef.reversal_attr > 0);
				if (!guarded) {
					enemy_hit = true;
					if (hitdef.sparkno != CommonData::IERR) {
						CharData* sparkOwner = isReversal ? target : owner;
						CharData* sparkActionSrc = isReversal ? owner : target;
						if (!sparkOwner) sparkOwner = target;
						if (!sparkActionSrc) sparkActionSrc = target;
						char_hitspark(sparkOwner, sparkActionSrc, hitdef,
							hitdef.sparkno, x, true);
					}
				} else {
					enemy_guarded = true;
					if (hitdef.guard_sparkno != CommonData::IERR) {
						CharData* sparkOwner = isReversal ? target : owner;
						CharData* sparkActionSrc = isReversal ? owner : target;
						if (!sparkOwner) sparkOwner = target;
						if (!sparkActionSrc) sparkActionSrc = target;
						char_hitspark(sparkOwner, sparkActionSrc, hitdef,
							hitdef.guard_sparkno, x, true);
					}
				}
			}

			// ── Hit sound (SSZ: hitsoundg/guardsoundg routing) ──
			// SSZ lines 6400-6417 (hit) / 6429-6446 (guard):
			//   1. Skip if group == IERR (no sound for this hit)
			//   2. Allocate a channel with by.newChannel(-1, false)
			//   3. If group < 0: negate it and look up from fight soundbank
			//   4. If group >= 0: look up from character's .snd soundbank
			//   5. Set stereo pan from target X position
			//   6. Call setDefaultParameter() for volume/pitch
			{
				int sndGroup = guarded ? hitdef.guardsound_group : hitdef.hitsound_group;
				int sndNumber = guarded ? hitdef.guardsound_number : hitdef.hitsound_number;
				// SSZ: if(hit.hitsoundg == .com.IERR) break;
				if (sndGroup != CommonData::IERR) {
					// SSZ: sou = by.newChannel(-1, false); if(#sou == 0) break;
					SoundChannel* ch = get_channel(-1);
					if (ch) {
						const WaveData* wav = nullptr;
						// SSZ: cond hit.hitsoundg < 0:
						//         use fight~fsn~getSound(!group, number)
						//       else:
						//         use cgi[by.playerno].sn~getSound(group, number)
						if (sndGroup < 0) {
							// Negative group: fight soundbank (global), negate group
							wav = sound_table_get_sound(-sndGroup, sndNumber);
						} else {
							// Positive group: character soundbank (global)
							wav = sound_table_get_sound(sndGroup, sndNumber);
						}
						if (wav) {
							// get_channel(-1) already called setDefaultParameter() —
							// no need to reset defaults here.
							ch->wave = wav;
							// SSZ: sou~chrx = pyr.sysfvar[.fX...fX+1];
							// Stereo pan from target's X position
							ch->setPan(target->x);
						}
					}
					// Submit the mixed buffer immediately per SSZ
					// (play_sound clears + mixes all active channels)
					if (ch && ch->wave) {
						play_sound();
					}
				}
			}

			// ── Screen shake — handled by trigger_envshake() in setHb() below

			// ── HitByData setHb() ──
			// SSZ: stVal.setHb(hit=, guard, combo, absdamage)
			// Populates the target's HitByData (hb) from the hitdef, including
			// velocity, timing, guard state, fall data, and hit-by counters.
			// This carries all hit parameters to the character's state machine
			// for hit reaction, animation type selection, and fall detection.
			{
				// combo detection: true when the same projectile hits
				// multiple times (simplified: use hitCount as combo indicator)
				bool combo = (target->stVal.hb.hitid == hitdef.id);
				target->stVal.setHb(hitdef, guarded, combo, target->stateType, 0);
				// ── Apply PalFX from hitdef to target character ──
				// SSZ: common section after guard/hit branching:
				//   if(hit.palfx_time > 0){ pyr.palfx~clear2(1); ... }
				trigger_palfx(target, hitdef);
				target->fallPending = true;
				target->fallWasAirborne = false;
			}

			// ── Consume one hit ──
			hitCount--;

			// If projectile is fully consumed, remove its velocity
			if (hitCount <= 0) {
				remvel();
			}
		}
	}
}

void ProjectileData::tick(int playerNo) {
	// SSZ: Projectile::tick() — handles hitpause and animation lifecycle.
	// On each frame:
	//   1. Decrement hitpause when > 0 (freezes animation during hitpause)
	//   2. Decrement lifetime counters
	//   3. Advance animation when not paused
	//
	// hitpause is set by projClsn() when the projectile hits or is guarded.
	// During hitpause, the projectile's animation freezes (anime() checks hitpause).
	// Once hitpause expires, the projectile resumes normal animation.
	(void)playerNo;

	// ── Decrement hitpause ──
	// SSZ: if(hitpause > 0) hitpause--
	if (hitpause > 0) {
		hitpause--;
	}

	// ── Decrement lifetime counters ──
	// Each projectile has a hitCount (remaining times it can hit)
	// and hitCountMax (initial lifetime).
	if (hitCount > 0) {
		hitCount--;
	}

	// ── Advance animation (skipped during hitpause) ──
	// SSZ: animation only advances when not in hitpause.
	// This gives the visual "freeze" effect during hit impact.
	anime(false, playerNo);
}

void ProjectileData::anime(bool oVer, int playerNo) {
	// Advance projectile animation frame.
	// Calls anim->action() to step the animation and
	// anim->updateSprite() to refresh the current sprite.
	// Skips animation advancement while hitpause > 0 (SSZ: hit reaction freeze).
	(void)oVer;
	(void)playerNo;
	if (!anim) return;

	// SSZ: skip animation during hitpause (projectile freezes on hit/guard frame).
	// hitpause is set by projClsn() alongside enemy_hit/enemy_guarded, and
	// counts down each tick in ProjectileData::tick(). Once hitpause reaches 0,
	// animation resumes normally. The enemy_hit/enemy_guarded flags persist as
	// projectile state for external queries (e.g. enemyhit SCTRL trigger).
	if (hitpause > 0) {
		// Still need to refresh the sprite for rendering, but don't advance
		if (!anim->spr) anim->updateSprite();
		return;
	}

	// Advance the animation
	anim->action();

	// Refresh the sprite from the current animation frame
	anim->updateSprite();

	// If animation has ended (no more frames), expire the projectile
	int elemNo = anim->animElemNo(0);
	if (elemNo < 0) {
		hitCount = 0;
	}
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
	fallPending = false;
	fallWasAirborne = false;
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

	// ── [Files] section: find .air animation and .snd sound file paths ──
	std::string airPath, soundPath;
	bool inFiles = false;
	for (size_t i = 0; i < cleaned.size(); i++) {
		const auto& line = cleaned[i];
		if (line[0] == '[') {
			std::string section = line;
			for (auto& c : section) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			inFiles = (section.find("[files]") != std::string::npos);
			continue;
		}
		if (inFiles) {
			size_t eq = line.find('=');
			if (eq != std::string::npos) {
				std::string key = line.substr(0, eq);
				size_t ks = key.find_first_not_of(" \t");
				if (ks != std::string::npos) key = key.substr(ks);
				size_t ke = key.find_last_not_of(" \t");
				if (ke != std::string::npos) key = key.substr(0, ke + 1);
				if (key == "anim") {
					airPath = line.substr(eq + 1);
					size_t ns = airPath.find_first_not_of(" \t");
					if (ns != std::string::npos) airPath = airPath.substr(ns);
					size_t ne = airPath.find_last_not_of(" \t\r\n");
					if (ne != std::string::npos) airPath = airPath.substr(0, ne + 1);
				} else if (key == "sound") {
					soundPath = line.substr(eq + 1);
					size_t ns = soundPath.find_first_not_of(" \t");
					if (ns != std::string::npos) soundPath = soundPath.substr(ns);
					size_t ne = soundPath.find_last_not_of(" \t\r\n");
					if (ne != std::string::npos) soundPath = soundPath.substr(0, ne + 1);
				}
			}
		}
	}

	// ── Load .air file and populate actionMap ──
	if (!airPath.empty()) {
		// Resolve .air path relative to .def file directory
		std::string resolvedAir = airPath;
		size_t slash = defPath.find_last_of("/\\");
		if (slash != std::string::npos) {
			resolvedAir = defPath.substr(0, slash + 1) + airPath;
		}

		std::string airBuf = common_load_text(resolvedAir, false);
		if (airBuf.empty()) return;

		auto airLines = common_split_lines(airBuf);
		if (airLines.empty()) return;

		// Iterate through .air lines looking for [Begin Action N] sections
		for (int i = 0; i < static_cast<int>(airLines.size()); i++) {
			const std::string& raw = airLines[i];

			// Look for "[Begin Action" header (case-insensitive)
			size_t beginPos = std::string::npos;
			{
				std::string lowerLine;
				lowerLine.reserve(raw.size());
				for (char c : raw)
					lowerLine += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
				beginPos = lowerLine.find("[begin action");
			}

			if (beginPos == std::string::npos)
				continue;

			// Parse action number after "[Begin Action "
			// Format: "[Begin Action 200]" or "[Begin Action 200] ; comment"
			size_t numStart = beginPos + 13; // length of "[begin action"
			// Skip any spaces/tabs after "action"
			while (numStart < raw.size() && (raw[numStart] == ' ' || raw[numStart] == '\t'))
				numStart++;

			// Collect digits and optional sign
			if (numStart >= raw.size() || (raw[numStart] < '0' || raw[numStart] > '9'))
				continue;

			int actionNo = 0;
			while (numStart < raw.size() && raw[numStart] >= '0' && raw[numStart] <= '9') {
				actionNo = actionNo * 10 + (raw[numStart] - '0');
				numStart++;
			}

			// Create and parse the action
			ActionData action;
			action.no = actionNo;
			action.ani.loopstart = 0;

			// Advance past the header line so read() starts at frame data
			i++;
			if (i >= static_cast<int>(airLines.size())) {
				// Empty action section — just store it
				addAction(action);
				break;
			}

			// Parse frames until next section header
			action.read(airLines, i);
			// After read(), i points to the last parsed frame line
			// The for loop's i++ will advance past it

			addAction(action);
		}
	}

	// ── Load .snd file into global sound table ──
	if (!soundPath.empty()) {
		std::string resolvedSnd = soundPath;
		size_t slash = defPath.find_last_of("/\\");
		if (slash != std::string::npos) {
			resolvedSnd = defPath.substr(0, slash + 1) + soundPath;
		}
		sound_table_load_file(resolvedSnd);
	}
}

void CharData::loadPallet(const std::string& defPath, int no) {
	// SSZ char.ssz line 2710: loads .act palette file and remaps SFF palette table.
	// The SSZ version is complex: reads .act files per palette slot, remaps
	// SFF palList, and manages palExist/palSelectable arrays.
	//
	// Simplified native: construct .act file path relative to .def directory
	// and read 768 bytes (256 RGB triplets) into a palette array on CharGlobalInfo.
	// Full SFF palette table remapping is deferred until sff_service palList is
	// wired for runtime palette operations.
	//
	// .act file naming convention (M.U.G.E.N standard):
	//   Pal<no>.act  or  name<no>.act  or  <defname>.act
	// Where no is 1-indexed (1-12 typically).
	if (defPath.empty() || no < 0) return;

	// Try constructing: <defDir>/Pal<no>.act  then  <defDir>/<name><no>.act
	std::string dir;
	size_t slash = defPath.find_last_of("/\\");
	if (slash != std::string::npos) {
		dir = defPath.substr(0, slash + 1);
	} else {
		dir = "";
	}

	// Try palette files in priority order
	std::vector<std::string> candidates;
	// Pal<no>.act (M.U.G.E.N standard naming)
	candidates.push_back(dir + "Pal" + std::to_string(no) + ".act");
	// pal<no>.act (lowercase variant)
	candidates.push_back(dir + "pal" + std::to_string(no) + ".act");
	// <defname>.<no>.act (alternate naming)
	size_t dot = defPath.find_last_of('.');
	if (dot != std::string::npos && dot > slash) {
		std::string baseName = defPath.substr(slash + 1, dot - slash - 1);
		candidates.push_back(dir + baseName + std::to_string(no) + ".act");
	}

	for (const auto& path : candidates) {
		FILE* f = fopen(path.c_str(), "rb");
		if (!f) continue;

		// Read 256 RGB triplets (768 bytes) — standard .act format
		// .act files are exactly 768 bytes, 3 bytes per color (R, G, B)
		uint8_t palette[768];
		size_t bytesRead = fread(palette, 1, 768, f);
		fclose(f);

		if (bytesRead == 768) {
			// Store palette as 256 uint32_t ARGB entries
			// For now, just parse and store — actual SFF palette remap deferred
			// SSZ: .cgi[playerno].sf~palList.palTable.set(...)
			// Native: store in a simple vector for future use
			std::vector<uint32_t> palColors(256);
			for (int i = 0; i < 256; i++) {
				uint8_t r = palette[i * 3];
				uint8_t g = palette[i * 3 + 1];
				uint8_t b = palette[i * 3 + 2];
				palColors[i] = 0xFF000000 | (static_cast<uint32_t>(r) << 16)
					| (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
			}
			// Store on CharGlobalInfo for use by sff_service palette remapping
			// cgi[playerno].palData[no] = std::move(palColors);
			// TODO: Wire palData storage when CharGlobalInfo is extended
		}
		break; // Found and processed the first candidate
	}
}

void CharData::clearDef() {
	// SSZ: clear all character definition fields
	def.clear();
	name.clear();
	// Note: SSZ also clears cgi[playerno].displayname/author/sprite/anim/sound/movelist
	// and resets pal/palkeymap — these are CharGlobalInfo fields not yet in native.
}

void CharData::tick() {
	// Called each game tick
	timeInState++;

	// Decrement hitpause timer; clear hitPause when expired
	if (hitPauseTime > 0) {
		hitPauseTime--;
		if (hitPauseTime <= 0) {
			hitPause = false;
		}
	}

	// ── Fall detection state machine ──
	// Detects when a character lands after being knocked down and triggers
	// the stored fall envshake (from stVal.hb.fall).
	//
	// Flow:			//   1. fallPending is set to true when a hit with hitdef.fallFlag != 0 lands
	//      (in projClsn()). The fall envshake params are stored in
	//      stVal.hb.fall.envshake_*.
	//   2. Each tick, while fallPending is true, check if the character is
	//      airborne (y < groundLevel). Once airborne is detected via
	//      fallWasAirborne = true, the state machine waits for landing.
	//   3. When the character lands (y >= groundLevel) after having been
	//      airborne, the stored fall envshake is applied to the global
	//      EnvShakeData. fallPending is cleared and stVal.hb.fall is reset.
	//
	// This prevents the envshake from triggering if the character is hit
	// while already on the ground (no airborne → no landing spike).
	//
	// SSZ equivalent: The character state machine's hit detection logic
	// handles fall envshake when the character transitions into Lieing (L)
	// state type on ground contact. This native implementation is a simpler
	// proxy that detects the airborne→ground transition directly.
	if (fallPending) {
		if (y < static_cast<float>(groundLevel)) {
			// Character is now airborne — mark for landing detection
			fallWasAirborne = true;
		} else if (fallWasAirborne) {
			// Character was airborne and has now landed → trigger fall envshake
			if (stVal.hb.fall.envshake_time > 0) {
				EnvShakeData& fallShake = stage_get_env_shake();
				fallShake.time = stVal.hb.fall.envshake_time;
				fallShake.freq = stVal.hb.fall.envshake_freq;
				fallShake.ampl = stVal.hb.fall.envshake_ampl;
				fallShake.phase = stVal.hb.fall.envshake_phase;
				fallShake.setDefPhase();
			}
			// Clear fall state regardless of whether envshake was triggered
			fallPending = false;
			fallWasAirborne = false;
			stVal.hb.fall.clear();
		}
		// If neither airborne nor landing (character started on ground and
		// stay on ground), fallPending stays true until the character goes
		// airborne and lands, OR until the next hit clears it via hb.clear().
	}

	// SSZ: step per-frame palette effects (step() handles expiry)
	// Placed in tick() rather than action() because tick() handles all per-frame
	// timer decrements (hitpause, fall detection). PalFX time decrement follows
	// the same pattern — it should run every frame regardless of animation state.
	palfx.step();

	// Blocking state is updated externally via char_update_blocking()
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
	// SSZ char.ssz lines 3573-3612: character binding (throws, grabs, ride)
	// Syncs position, velocity, and facing with the bound target character.
	//
	// Native implementation uses dedicated fields (bindTime/bindToId/bindPosX/Y/bindFacing)
	// instead of sysivar/sysfvar arrays. Core logic matches SSZ.

	// Early out if not bound
	if (bindTime == 0) return;

	// Find the target character by playerNo
	CharData* target = nullptr;
	CharModuleState& cs = char_get_state();
	for (int i = 0; i < 4; i++) {
		if (cs.chars[i] && cs.chars[i]->playerNo == bindToId) {
			target = cs.chars[i];
			break;
		}
	}

	if (target) {
		// SSZ: if(c~hasTarget(id)) — check if target considers us bound
		// Simplified: assume binding is symmetric (both sides participate)

		// SSZ: if(c~ls(.lsDESTROY)) → selfState(5050)
		// We don't have ls() flags — skip destroy check for now

		// SSZ: sync velocity from target when bind pos is set (not NaN)
		if (!std::isnan(bindPosX)) {
			// SSZ: setXV((float)(facing * c.facing) * c.sysfvar[fVX])
			// The multiplier (facing * c.facing) determines relative direction:
			//   facing same direction: +1 (move same direction as target)
			//   facing opposite: -1 (move opposite direction)
			float dirFactor = static_cast<float>(facing * target->facing);
			xvel = dirFactor * target->xvel;
		}
		if (!std::isnan(bindPosY)) {
			yvel = target->yvel;
		}

		// SSZ: sync position from target
		// x = c.x + f * bindPosX  where f = bindFacing==2 ? bindFacing/2 : c.facing
		if (!std::isnan(bindPosX)) {
			float f;
			if (bindFacing == 2) {
				f = static_cast<float>(bindFacing) / 2.0f; // 2/2 = 1.0
			} else {
				f = static_cast<float>(target->facing);
			}
			x = target->x + f * bindPosX;
			pos_x = x;
		}

		if (!std::isnan(bindPosY)) {
			y = target->y + bindPosY;
			pos_y = y;
		}

		// SSZ: sync facing from target based on bindFacing mode
		// SSZ: branch{ cond sysivar[iBINDFACING] > 0: setFacing(c.facing)
		//            cond sysivar[iBINDFACING] < 0: setFacing(-c.facing) }
		// bindFacing: positive = match target, negative = opposite, 0 = no change
		if (bindFacing > 0) {
			facing = target->facing;
		} else if (bindFacing < 0) {
			facing = -target->facing;
		}
	} else {
		// SSZ: target not found → unbind
		bindTime = 0;
		return; // Don't decrement bindTime when already reset
	}

	// Decrement bind timer each frame (SSZ: handled by state machine tick)
	if (bindTime > 0) {
		bindTime--;
	}
}

void CharData::xScreenBound() {
	// SSZ char.ssz lines 3614-3631: clamp character position to screen bounds.
	// If lsSCREENBOUND flag is set, clamp to stage+character edge bounds.
	// Always clamp to stage leftbound/rightbound.
	//
	// SSZ:
	//   float x = `sysfvar[.fX];
	//   if(`ls(.lsSCREENBOUND)){
	//     float min, max;
	//     branch{
	//     cond `facing > 0:
	//       min = .com.xmin + `getEdge(`bedge, true);
	//       max = .com.xmax - `getEdge(`fedge, true);
	//     else:
	//       min = .com.xmin + `getEdge(`fedge, true);
	//       max = .com.xmax - `getEdge(`bedge, true);
	//     }
	//     .m.limRange!float?(x=, min, max);
	//   }
	//   .m.limRange!float?(x=, (float).stg~leftbound, (float).stg~rightbound);
	//   `setPosX(x);
	// Native: simplified — clamp to CommonData screen bounds
	const auto& cd = common_get_state();
	float newX = x;
	// SSZ: clamp to .com.xmin / .com.xmax (stage horizontal bounds)
	newX = std::max(newX, cd.xmin);
	newX = std::min(newX, cd.xmax);
	if (newX != x) {
		x = newX;
		pos_x = newX;
	}
}

void CharData::update() {
	tick();
	action();
	posUpdate();
	gravityStep();
}

void CharData::posReset() {
	// SSZ char.ssz lines 3747-3757:
	//   `facing = (`playerno&1) == 0 ? .stg~p1.facing : .stg~p2.facing;
	//   `setX((float)(((`playerno&1) == 0 ? .stg~p1.startx : .stg~p2.startx)
	//     - .com.cam.stg.startx) * .stg~localscl
	//     + (float)(`playerno*-`facing / 2) * .com.P1P3Dist);
	//   `setY(0.0);
	//   `setXV(0.0);
	//   `setYV(0.0);
	// Simplified: facing based on team side, position reset to default
	facing = ((playerNo & 1) == 0) ? 1 : -1;
	x = 0.0f;
	y = 0.0f;
	pos_x = x;
	pos_y = y;
	xvel = 0.0f;
	yvel = 0.0f;
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
	// NOTE: PalFX pipeline deferred — the character's palfx flows through
	// CharData::palfx and is stepped in action(), but the actual palette
	// modification during rendering requires the sdlplugin to consume
	// PalFX data, which is not yet wired.
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
	// SSZ: Face opponent when standing/crouching and not in hitpause.
	// SSZ char.ssz line 3342-3353:
	//   if(`ctrl && `helperindex == 0){
	//     if(`trDistX(.players.enemyNear(`=, 0, true))$.v.toF() >= 0.0) break;
	//     branch{
	//     cond `stVal.typ == .StTy::S: `trChangeAnim(5);
	//     cond `stVal.typ == .StTy::C: `trChangeAnim(6);
	//     }
	//     `setFacing(-`facing);
	//   }
	// Simplified: if ctrl and opponent is behind, flip facing.
	// Full implementation needs opponent distance check — deferred.
	if (!ctrl || playerNo >= 4) return;
	CharModuleState& cs = char_get_state();
	bool foundAlive = false;
	bool opponentInFront = false;
	for (int i = 0; i < 4; i++) {
		CharData* other = cs.chars[i];
		if (!other || other == this) continue;
		if ((i & 1) == (playerNo & 1)) continue; // Same team
		if (other->life <= 0.0f) continue;
		foundAlive = true;
		float dx = other->x - x;
		// Check if opponent is in the direction we're facing
		if ((facing > 0 && dx > 0) || (facing < 0 && dx < 0)) {
			opponentInFront = true;
			break;
		}
	}
	// SSZ: only flip if no opponent is in front (opponent is behind)
	if (foundAlive && !opponentInFront) {
		facing = -facing;
		// SSZ: also changes anim to 5 (standing) or 6 (crouching)
		if (stateType == StTy::S || stateType == StTy::C) {
			int animNo = (stateType == StTy::S) ? 5 : 6;
			trChangeAnim(animNo);
		}
	}
}

void CharData::nextRound() {
	// SSZ char.ssz: prepares character for a new round within the same match.
	// Resets position, clears hit-by tracking, resets state machine to
	// default standing state, and clears per-round flags.
	posReset();
	stVal.clear();
	stateno = 0;
	timeInState = 0;
	hitPause = false;
	hitPauseTime = 0;
	ctrl = false;
	blocking = false;
	hitdef.setDefault();
	palfx.clear2(1);
	palfx.enable = false;
	fallPending = false;
	fallWasAirborne = false;
}

void CharData::rootInit() {
	// SSZ char.ssz: match-start initialization for the root character.
	// Runs once at the start of the match (not per-round).
	// Sets up global character state including level-based life/power scaling
	// and debug flags. The per-character power/life scaling from level[]
	// is handled by fighting_main() init block, so this function handles
	// character-specific root setup only.
	//
	// For the native implementation, this is intentionally minimal —
	// character-level init already ran in load()/init(), and the remaining
	// SSZ rootInit() logic (level-based power scaling, team life calc) is
	// handled directly in fighting_main()'s initialization block.
	stVal.clear();
	hitdef.setDefault();
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
	return actionMap.find(pid) != actionMap.end();
}

ActionData* CharData::getAction(int no) {
	auto it = actionMap.find(no);
	if (it != actionMap.end())
		return &it->second;
	return nullptr;
}

void CharData::addAction(const ActionData& action) {
	actionMap[action.no] = action;
}

// =========================================================================
// char_update_blocking() — Determine blocking state from command input
// =========================================================================
//
// For each character, checks if the player is holding "back" (away from the
// opponent) by examining the command input buffer.  A character is blocking
// when they hold back, have control (ctrl), and are not in hitpause.
//
// Called once per frame from fighting_main() before projectile/attack
// collision checks so that projClsn() has accurate blocking state.
//
void char_update_blocking() {
	CharModuleState& cs = char_get_state();
	for (int i = 0; i < 4; i++) {
		CharData* ch = cs.chars[i];
		if (!ch) continue;

		// Can't block while in hitpause
		if (ch->hitPause) {
			ch->blocking = false;
			continue;
		}

		// Can't block without control
		if (!ch->ctrl) {
			ch->blocking = false;
			continue;
		}

		// Check command buffer for "hold back" direction.
		// "Back" is away from the opponent:
		//   Facing right → back = left (B or DB)
		//   Facing left  → back = right (F or DF)
		// Clamp playerNo to 1 — buf only has 2 slots (P1/P2);
		// players 2,3 in team modes share the buffer of their team lead.
		int bufIdx = std::min(ch->playerNo, 1);
		BufferData& buf = command_get_state().buf[bufIdx];
		bool backPressed;
		if (ch->facing >= 0) {
			backPressed = (buf.keyState(Key::B) > 0 || buf.keyState(Key::DB) > 0);
		} else {
			backPressed = (buf.keyState(Key::F) > 0 || buf.keyState(Key::DF) > 0);
		}

		ch->blocking = backPressed;
	}
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
	bool bright,
	PalFXData* fx)
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
	as.fx = fx;               // SSZ: palette effect for this sprite

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
				static_cast<float>(cd.GameWidth) / 2.0f, 0,
				as.fx); // Pass PalFX for per-sprite palette effects
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
				0.0f, static_cast<float>(cd.GameWidth) / 2.0f, 0,
				as.fx); // Pass PalFX for per-sprite palette effects
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
// =========================================================================
// char_tick_explods() — Tick all explods: advance animations, update
// timers, remove expired explods.
// =========================================================================
// Called once per game tick from fighting_main() after character updates.
// Iterates all characters and their explod instances, advancing the
// animation each frame and removing explods whose animation has ended.
void char_tick_explods() {
	CharModuleState& cs = char_get_state();
	for (int i = 0; i < 4; i++) {
		CharData* ch = cs.chars[i];
		if (!ch) continue;

		auto& explods = ch->explods;
		for (auto it = explods.begin(); it != explods.end(); ) {
			ExplodData& explod = *it;

			// Advance the animation
			if (explod.anim) {
				explod.anim->action();
				explod.anim->updateSprite();
			}
			explod.time++;

			// Decrement positive removeTime
			if (explod.removeTime > 0)
				explod.removeTime--;

			// Remove expired explods
			if (explod.isExpired()) {
				it = explods.erase(it);
			} else {
				++it;
			}
		}
	}
}

// =========================================================================
// char_draw_explods() — Queue active explod animations into the anim list
// =========================================================================
// Called from char_draw() before the anim list is rendered.
// Each explod with a valid sparkAction (owned action) is added to either
// the regular anim list or topanims (if ontop==1) for priority-sorted
// rendering via char_draw_anim_list().
void char_draw_explods(float x, float y, float scl) {
	(void)x; (void)y; (void)scl;
	CharModuleState& cs = char_get_state();

	for (int i = 0; i < 4; i++) {
		CharData* ch = cs.chars[i];
		if (!ch) continue;

		for (const auto& explod : ch->explods) {
			// Only render explods that have an owned sparkAction with frames
			if (!explod.anim) continue;
			if (explod.sparkAction.ani.frames.empty()) continue;
			if (explod.isExpired()) continue;

			// Ensure sprite is loaded for the current frame
			if (!explod.anim->spr)
				const_cast<AnimData*>(explod.anim)->updateSprite();
			if (!explod.anim->spr) continue;

			// Choose the anim list: topanims for ontop, regular anims otherwise
			std::vector<AnimSpriteData>& list =
				explod.ontop ? cs.topanims : cs.anims;

			// Use the action's ani directly (sparkAction.ani)
			ActionData* action = const_cast<ActionData*>(&explod.sparkAction);

			float xscl = static_cast<float>(explod.facing);

			char_add_anim_list(
				list,
				action,
				explod.sprpriority,
				explod.x, explod.y, false,    // position, not screen-relative
				xscl, 1.0f,                    // xscl (facing), yscl
				0.0f, false,                    // angle, oVer
				1.0f, 1.0f,                     // axscl, ayscl
				-1, 0,                          // salpha, dalpha (SSZ defaults)
				false                           // bright
			);
		}
	}
}

// =========================================================================
// char_hitspark — Create a hit spark explod matching SSZ semantics
// =========================================================================
// SSZ: hitspark(p1=, p2=, animNo) in char.ssz lines 6163-6194.
// Creates a spark explod on p1 (attacker/source), with position
// calculated from hitdef sparkx/sparky and projectile/character positions.
//
// For projectile hits (isProjectile=true):
//   x = p1.facing * projX
//   offsetx = x + hit.sparkx * p1.facing = p1.facing * (projX + hit.sparkx)
// For normal hits (isProjectile=false):
//   x = p1.facing * (p2.x - p1.x + p2.facing * facing-dependent width)
//   offsetx = x - hit.sparkx
//
// offsety = p2.y + hit.sparky  (uses the hit character's y position)
//
void char_hitspark(CharData* p1, CharData* p2,
                   HitdefData& hit, int animNo,
                   float projX, bool isProjectile)
{
	// ── Resolve spark animation ──
	// SSZ: animNo < 0 → use parent's ex-spark from fight~getAction
	//       else → p2.getAction(animNo)
	ActionData* ani = nullptr;
	if (animNo < 0) {
		// Negative sparkno: use fight's ex-spark action
		// Simplified: negate and look up from p2
		ani = p2->getAction(-animNo);
	} else {
		ani = p2->getAction(animNo);
	}
	if (!ani) return;

	// ── Spark position X (SSZ line 6172-6176) ──
	float x;
	if (isProjectile) {
		// SSZ: pro != 0 → x = (float)p1.facing * proj_x
		// Projectile hit: position at projectile X, scaled by p1's facing
		x = static_cast<float>(p1->facing) * projX;
	} else {
		// Character-to-character hit position (SSZ: pro == 0 path)
		// SSZ: x = p1.facing * (p2.x - p1.x + p2.facing * (
		//        (p1.facing<0) ^ (p2.facing<0) ? p2.frontw : -p2.backw))
		// The frontw/backw fields represent the character's collision widths.
		// Native CharData::width is used as a rough proxy for now.
		float widthFactor = static_cast<float>(
			(p1->facing < 0) ^ (p2->facing < 0) ? p2->width : -p2->width);
		x = static_cast<float>(p1->facing) * (
			p2->x - p1->x + static_cast<float>(p2->facing) * widthFactor);
	}

	// ── Spark offset (SSZ lines 6182-6185) ──
	// SSZ: offsetx = x - hit.sparkx * (pro != 0 ? -(pro * p1.facing) : 1.0)
	// For projectile: pro != 0 → -(1 * p1.facing) = -p1.facing
	//   offsetx = x - sparkx * (-p1.facing) = x + sparkx * p1.facing
	// For normal: offsetx = x - sparkx * 1.0 = x - sparkx
	float offsetX;
	if (isProjectile) {
		offsetX = x + hit.sparkx * static_cast<float>(p1->facing);
	} else {
		offsetX = x - hit.sparkx;
	}

	// SSZ: offsety = p2.y + hit.sparky + (p2.id == p1.id ? 0.0 : p1.stVal.hit.sparky)
	// The third term adds the attacker's own hitdef sparky if characters differ.
	// For now, use p2's y + hit.sparky (the primary term).
	float offsetY = p2->y + hit.sparky;

	// ── Create explod ──
	ExplodData spark;
	spark.sparkAction.ani.copy(ani->ani);
	spark.sparkAction.no = animNo;
	spark.anim = &spark.sparkAction.ani;
	spark.x = offsetX;
	spark.y = offsetY;

	// SSZ field setup (lines 6186-6193):
	// e~postype = 0; e~relativef = 1; e~scalex = e~scaley = 1.0;
	// e~ownpal = 1; e~ontop = 1; e~sprpriority = MIN_INT;
	// e~supermovetime = e~pausemovetime = -1;
	spark.posType = 0;       // World-relative positioning
	spark.ownpal = 1;        // Use own palette (palFX remap)
	spark.ontop = 1;         // Render on top layer
	spark.sprpriority = (std::numeric_limits<int>::min)(); // SSZ: consts.int_t::MIN
	spark.supermovetime = -1; // Ignore super pause
	spark.pausemovetime = -1; // Ignore normal pause
	spark.removeTime = -2;    // Remove when animation ends
	spark.id = -1;
	spark.xvel = 0.0f;
	spark.yvel = 0.0f;
	spark.facing = 1;
	spark.bindTime = 0;

	// SSZ: p1.insertExplod(tmp) — add to p1's explod list
	p1->explods.push_back(std::move(spark));

	// ── Configurable spark sound (CommonData) — not part of SSZ hitspark(),
	// but was present in the original native inline spark code.
	// Plays an additional SFX from CommonData::sparkSoundGroup/number.
	// These are disabled by default (group = -1) and are configured via
	// the system-level spark sound (separate from hitdef hit sound).
	{
		const CommonData& cd = common_get_state();
		if (cd.sparkSoundGroup >= 0) {
			const WaveData* sparkWav = sound_table_get_sound(
				cd.sparkSoundGroup, cd.sparkSoundNumber);
			if (sparkWav && add_wave(sparkWav)) {
				play_sound();
			}
		}
	}
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

	// ── 6. Explod sprites (sparks, effects) ──
	// SSZ: drawn via expdrawlist iteration inside drawAnimList calls
	char_draw_explods(x, y, scl);

	// ── 7. Anim list (character sprites) ──
	//	SSZ: .drawAnimList(.anims=, x, y, scl * cam.baseScale())
	char_draw_anim_list(cs.anims, x, y, scl * cam_base_scale(cd.cam));

	// ── 8. Foreground background ──
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
