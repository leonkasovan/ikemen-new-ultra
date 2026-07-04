// statebuilder_service.hpp — Native C++ implementation for statebuilder.ssz
//
// statebuilder.ssz (9334 lines) implements the state machine builder engine —
// parsing character state definitions, compiling state controllers, and
// generating executable state code. The CtrlTy enum defines all 90+
// M.U.G.E.N. state controller types.
//
// Phase 5: Core data structures, CtrlTy enum, State/StateCtrl parsing,
// and StateBuilder framework. Per-controller parsing (for each CtrlTy
// variant) deferred.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ikemen::ssz_native {

// =========================================================================
// CtrlTy — State controller type enum
// =========================================================================
enum class CtrlTy : uint16_t {
	Unknown,
	ChangeState, SelfState, ChangeAnim, ChangeAnim2,
	PosSet, PosAdd, VelSet, VelAdd, VelMul, Turn,
	VarSet, VarAdd, ParentVarSet, ParentVarAdd, VarRangeSet, VarRandom,
	CtrlSet, StateTypeSet, SprPriority,
	HitDef, Projectile, ReversalDef, AttackDist, HitOverride,
	Pause, SuperPause,
	LifeAdd, LifeSet, PowerAdd, PowerSet,
	TargetLifeAdd, TargetBind, BindToTarget, TargetState,
	TargetVelSet, TargetVelAdd, TargetFacing, TargetPowerAdd, TargetDrop,
	HitBy, NotHitBy, AttackMulSet, DefenceMulSet, MoveHitReset,
	AssertSpecial, PosFreeze, PlayerPush, Gravity,
	BindToParent, BindToRoot, Helper, DestroySelf,
	Explod, GameMakeAnim, ModifyExplod, ExplodBindTime, RemoveExplod,
	AfterImage, AfterImageTime, MakeDust,
	AngleDraw, AngleAdd, AngleMul, AngleSet,
	PalFX, AllPalFX, BGPalFX, Trans, Offset,
	HitVelSet, HitFallSet, HitFallVel, HitFallDamage, FallEnvShake,
	EnvColor, EnvShake, Width, ScreenBound, HitAdd,
	PlaySnd, StopSnd, SndPan,
	DisplayToClipboard, AppendToClipboard, ClearClipboard,
	TagIn, TagOut, ForceFeedback, VictoryQuote, RemapPal, Zoom,
	PlayerRewardAdd, ScoreAdd, AbyssDepthAdd, ExitMatch, PlayBGM,
	ForceCommand, Null
};

// =========================================================================
// StateCtrl — A single state controller
// =========================================================================
struct StateCtrlData {
	CtrlTy type{CtrlTy::Null};
	std::vector<std::string> trigger;     // trigger expressions
	std::vector<std::string> triggerall;  // global trigger conditions
	std::vector<std::string> task;        // task statements
	
	// Parsed controller parameters (varies by type)
	std::string params;
};

// =========================================================================
// State — A single character state definition
// =========================================================================
struct StateData {
	int no{};                          // state number
	std::string name;                  // optional state name
	std::string type;                  // S, C, A, L, N, U
	std::string movetype;              // I, A, H, U
	std::string physics;               // S, C, A, N, U
	std::string anim;                  // action number
	std::string sprPriority;           // sprite priority
	std::vector<StateCtrlData> ctrls;  // state controllers
	
	void clear();
	void tmpreset();
};

// =========================================================================
// Data — Character state builder data
// =========================================================================
struct StateBuilderData {
	void reset();
};

// =========================================================================
// Size — Character size definition
// =========================================================================
struct StateBuilderSize {
	void reset();
};

// =========================================================================
// Velocity — Character velocity definition
// =========================================================================
struct StateBuilderVelocity {
	void reset();
};

// =========================================================================
// Movement — Character movement definition
// =========================================================================
struct StateBuilderMovement {
	void reset();
};

// =========================================================================
// Const — Character constants
// =========================================================================
struct StateBuilderConst {
	void reset();
};

// =========================================================================
// StateBuilder — Main state machine builder
// =========================================================================
struct StateBuilder {
	// State number map
	struct StateNo {
		void set(int no, bool i, const std::string& proc);
	};
	
	/// Parse a [statedef] section. Returns true on success.
	bool statedef(const std::string& sec, const std::string& name,
		const std::string& data);
	
	/// Parse a single state definition section. Returns true on success.
	bool state(const std::string& sec, const std::string& name,
		const std::string& data);
	
	/// Build compiled state code for a character.
	/// no = character player number, def = .def file path,
	/// code = output code buffer (appended).
	/// Returns error string (empty = success).
	std::string build(int no, const std::string& def, std::string& code);
};

// =========================================================================
// Module-level API
// =========================================================================

/// Initialize the statebuilder module.
void statebuilder_init();

} // namespace ikemen::ssz_native
