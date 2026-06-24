#pragma once

#include <cstdint>

namespace ikemen::confignet {

// ── Video ────────────────────────────────────────────────────────────────
constexpr bool    OpenGL               = false;
constexpr bool    AspectRatio          = false;
constexpr bool    FullScreen           = false;
constexpr bool    FullScreenExclusive  = false;
constexpr int     WindowType           = 1;
constexpr int     Width                = 1280;
constexpr int     Height               = 800;
constexpr int     Brightness           = 256;
constexpr float   Opacity              = 1.0f;

// ── Audio ────────────────────────────────────────────────────────────────
constexpr float   GlVol   = 0.8f;
constexpr float   SEVol   = 0.8f;
constexpr float   BGMVol  = 0.5f;
constexpr float   PanStr  = 0.8f;
constexpr int     VideoVol = 100;

// ── Performance ──────────────────────────────────────────────────────────
constexpr bool    SaveMemory           = false;
constexpr int     HelperMax            = 56;
constexpr int     PlayerProjectileMax  = 50;
constexpr int     ExplodMax            = 256;
constexpr int     AfterImageMax        = 8;

// ── Game ─────────────────────────────────────────────────────────────────
constexpr int     GameSpeed                = 60;
constexpr float   AttackLifeToPowerMul     = 0.7f;
constexpr float   GetHitLifeToPowerMul     = 0.6f;
constexpr float   SuperTargetDefenceMul    = 1.5f;
constexpr float   LifebarFontScale         = 1.0f;

// ── Portraits ────────────────────────────────────────────────────────────
constexpr int     CharPortraitsGroup       = 9000;
constexpr int     CharFacePortraitIndex    = 0;
constexpr int     CharBigPortraitIndex     = 1;
constexpr int     CharWinnerPortraitIndex  = 2;
constexpr int     CharLoserPortraitIndex   = 3;
constexpr int     CharOrderPortraitIndex   = 4;
constexpr int     CharVSPortraitIndex      = 5;
constexpr int     CharResultsPortraitIndex = 6;
constexpr int     CharExtraPortraitIndex   = 7;

constexpr int     StagePortraitsGroup      = 9000;
constexpr int     StageIconPortraitIndex   = 0;
constexpr int     StageBigPortraitIndex    = 1;
constexpr int     StageVSPortraitIndex     = 2;
constexpr int     StageWinPortraitIndex    = 3;
constexpr int     StageExtraPortraitIndex  = 4;

// ── System ───────────────────────────────────────────────────────────────
constexpr const wchar_t* Executable      = L"Ikemen Plus Ultra.exe";
constexpr const wchar_t* WindowTitle     = L"I.K.E.M.E.N. PLUS ULTRA";
constexpr const wchar_t* ScreenshotFolder = L"screenshots";
constexpr const wchar_t* listenPort      = L"7500";
constexpr const wchar_t* UserName        = L"Nickname";
constexpr const wchar_t* GlobalAnims     = L"data/common.air";
constexpr const wchar_t* GlobalCommands  = L"data/common.cmd";
constexpr const wchar_t* GlobalMatch     = L"data/match.cns";
constexpr const wchar_t* systemScript    = L"script/main.lua";
constexpr const wchar_t* GamepadMappings = L"lib/external/gamecontrollerdb.txt";

constexpr bool    IgnoreMostErrors       = true;

// ── Input binding ────────────────────────────────────────────────────────

struct Keys {
	int jn = -2, u, d, l, r, a, b, c, x, y, z, q, w, e, s;
	void set(int jn_, int u_, int d_, int l_, int r_, int a_, int b_, int c_,
	         int x_, int y_, int z_, int q_, int w_, int e_, int s_) {
		jn=jn_; u=u_; d=d_; l=l_; r=r_; a=a_; b=b_; c=c_; x=x_; y=y_; z=z_; q=q_; w=w_; e=e_; s=s_;
	}
};

} // namespace ikemen::confignet
