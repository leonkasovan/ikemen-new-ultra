// share_service.cpp — Real implementations for ShareData copy/push.
//
// Phase 3: share_copy and share_push now maintain an internal snapshot
// and provide helper functions (pull_from_common, push_to_common) that
// other native modules call to integrate their state with the share system.
//
// As each dependent SSZ module (cmd, fnt, snd, chr, stage, etc.) is
// converted, add its pull/push helpers here and include its header.
//
// Currently wired: CommonData (common_service) — ~110+ field mappings.
// Pending modules: cmd, fnt, snd, chr, stage, cfg, se, sc.

#include "share_service.hpp"
#include "common_service.hpp"

#include <cstring>

namespace ikemen::ssz_native {

// ── Module integration helpers ─────────────────────────────────────────

void share_pull_from_common(const CommonData& src, ShareData& dst) {
    // Game mode / match state
    dst.tm      = src.home;
    dst.nt      = src.match;
    dst.re      = src.round;
    dst.chr_home   = src.home;
    dst.chr_match  = src.match;
    dst.chr_round  = src.round;

    // Wins / counters (p1/p2 tracked per-player in CommonData;
    // share uses single-player naming from SSZ conventions)
    dst.chr_p1mw = src.p1mw;
    dst.chr_p2mw = src.p2mw;
    dst.chr_p1w  = src.p1wins;
    dst.chr_p2w  = src.p2wins;
    dst.chr_drw  = src.draws;

    dst.consecutiveWins      = src.consecutiveWins;
    dst.firstAttackCount     = src.firstAttackCount;
    dst.winTimeCount         = src.winTimeCount;
    dst.winSpecialCount      = src.winSpecialCount;
    dst.winHyperCount        = src.winHyperCount;
    dst.winThrowCount        = src.winThrowCount;
    dst.winPerfectCount      = src.winPerfectCount;
    dst.winPerfectSpecialCount = src.winPerfectSpecialCount;
    dst.winPerfectHyperCount = src.winPerfectHyperCount;
    dst.winPerfectThrowCount = src.winPerfectThrowCount;

    // Display flags
    dst.p1winsDisplay       = src.p1winsDisplay;
    dst.p2winsDisplay       = src.p2winsDisplay;
    dst.rewardDisplay       = src.rewardDisplay;
    dst.scoreDisplay        = src.scoreDisplay;
    dst.timerDisplay        = src.timerDisplay;
    dst.countdownDisplay    = src.countdownDisplay;
    dst.ailevelDisplay      = src.ailevelDisplay;
    dst.matchnoDisplay      = src.matchnoDisplay;
    dst.gamemodeDisplay     = src.gamemodeDisplay;

    // Score
    dst.score        = src.score;
    dst.scoreTotal   = src.scoreTotal;
    dst.p1score      = src.p1score;
    dst.p2score      = src.p2score;

    // Timer
    dst.timer           = src.timer;
    dst.countdownTimer  = src.countdownTimer;

    // CPU level
    dst.cpuLevel = src.cpuLevel;

    // Vectors
    dst.com        = src.com;
    dst.taglevel   = src.taglevel;
    dst.autoguard  = src.autoguard;
    dst.powsh      = src.powerShare;
    dst.inputRemap = src.inputRemap;

    // Playback / recording
    dst.recordState   = src.recordState;
    dst.playbackState = src.playbackState;

    // Round
    dst.roundTime   = src.roundTime;
    dst.roundsToWin = src.roundsToWin;
    dst.lastMatch   = src.lastMatch;
    dst.matchsToWin = src.matchsToWin;
    dst.p1matchWins = src.p1matchWins;
    dst.p2matchWins = src.p2matchWins;

    // Life / power / stats
    dst.life               = src.life;
    dst.power              = src.power;
    dst.attack             = src.attack;
    dst.defence            = src.defence;
    dst.team1VS2Life       = src.team1VS2Life;
    dst.turnsRecoveryRate  = src.turnsRecoveryRate;
    dst.sharedLife         = src.sharedLife;

    // Camera / zoom
    dst.zoom      = src.cam.zoom;
    dst.zoomMin   = src.cam.zoomMin;
    dst.zoomMax   = src.cam.zoomMax;
    dst.zoomSpeed = src.cam.zoomSpeed;

    // Simulation
    dst.numSimul = src.numSimul.empty() ? 0 : src.numSimul[0];

    // System strings
    dst.operatingSystem = src.operatingSystem;
    dst.gameMode        = src.gameMode;
    dst.gameService     = src.gameService;
    dst.playerSide      = src.playerSide;
    dst.pauseVar        = src.pauseVar;
    dst.tourneyState    = src.tourneyState;

    // Display / type
    dst.lifebarDisplay = src.lifebarDisplay;
    dst.gameType       = src.gameType;

    // Persistence
    dst.persistLife      = src.persistLife;
    dst.lifePersistence  = src.lifePersistence;
    dst.persistPower     = src.persistPower;
    dst.powerPersistence = src.powerPersistence;
    dst.persistRoundtime = src.persistRoundtime;
    dst.timePersistence  = src.timePersistence;

    // Credits / coins
    dst.credits = src.credits;
    dst.coins   = src.coins;

    // Player rewards
    dst.playerReward  = src.playerReward;
    dst.playerLife    = src.playerLife;
    dst.playerPower   = src.playerPower;
    dst.playerAttack  = src.playerAttack;
    dst.playerDefence = src.playerDefence;

    // Abyss
    dst.abyssDepth           = src.abyssDepth;
    dst.abyssDepthBoss       = src.abyssDepthBoss;
    dst.abyssDepthBossSpecial = src.abyssDepthBossSpecial;
    dst.abyssBossFight       = src.abyssBossFight;
    dst.abyssFinalDepth      = src.abyssFinalDepth;
    dst.abyssSP1             = src.abyssSP1;
    dst.abyssSP2             = src.abyssSP2;
    dst.abyssSP3             = src.abyssSP3;
    dst.abyssSP4             = src.abyssSP4;

    // Display toggles
    dst.inputDisplay   = src.inputDisplay;
    dst.attackDisplay  = src.attackDisplay;
    dst.powerStateP1   = src.powerStateP1;
    dst.powerStateP2   = src.powerStateP2;
    dst.lifeStateP1    = src.lifeStateP1;
    dst.lifeStateP2    = src.lifeStateP2;
    dst.dummyState     = src.dummyState;
    dst.dummyDistance  = src.dummyDistance;
    dst.dummyGuard     = src.dummyGuard;
    dst.dummyRecovery  = src.dummyRecovery;
    dst.counterHit     = src.counterHit;

    // Debug / toggles
    dst.dbgdw    = src.debugdraw;
    dst.clsndw   = src.clsndraw;
    dst.stsdw    = src.statusDraw;
    dst.alvl     = src.autolevel;
    dst.suaveMode = src.suaveMode;
    dst.exitMatch = src.exitMatch;

    // Debug script path
    dst.dlua = src.debugScript;
}

void share_push_to_common(const ShareData& src, CommonData& dst) {
    dst.home    = src.chr_home;
    dst.match   = src.chr_match;
    dst.round   = src.chr_round;

    dst.p1mw = src.chr_p1mw;
    dst.p2mw = src.chr_p2mw;
    dst.p1wins = src.chr_p1w;
    dst.p2wins = src.chr_p2w;
    dst.draws  = src.chr_drw;

    dst.consecutiveWins      = src.consecutiveWins;
    dst.firstAttackCount     = src.firstAttackCount;
    dst.winTimeCount         = src.winTimeCount;
    dst.winSpecialCount      = src.winSpecialCount;
    dst.winHyperCount        = src.winHyperCount;
    dst.winThrowCount        = src.winThrowCount;
    dst.winPerfectCount      = src.winPerfectCount;
    dst.winPerfectSpecialCount = src.winPerfectSpecialCount;
    dst.winPerfectHyperCount = src.winPerfectHyperCount;
    dst.winPerfectThrowCount = src.winPerfectThrowCount;

    dst.p1winsDisplay    = src.p1winsDisplay;
    dst.p2winsDisplay    = src.p2winsDisplay;
    dst.rewardDisplay    = src.rewardDisplay;
    dst.scoreDisplay     = src.scoreDisplay;
    dst.timerDisplay     = src.timerDisplay;
    dst.countdownDisplay = src.countdownDisplay;
    dst.ailevelDisplay   = src.ailevelDisplay;
    dst.matchnoDisplay   = src.matchnoDisplay;
    dst.gamemodeDisplay  = src.gamemodeDisplay;

    dst.score      = src.score;
    dst.scoreTotal = src.scoreTotal;
    dst.p1score    = src.p1score;
    dst.p2score    = src.p2score;

    dst.timer          = src.timer;
    dst.countdownTimer = src.countdownTimer;

    dst.cpuLevel = src.cpuLevel;

    dst.com        = src.com;
    dst.taglevel   = src.taglevel;
    dst.autoguard  = src.autoguard;
    dst.powerShare  = src.powsh;
    dst.inputRemap = src.inputRemap;

    dst.recordState   = src.recordState;
    dst.playbackState = src.playbackState;

    dst.roundTime   = src.roundTime;
    dst.roundsToWin = src.roundsToWin;
    dst.lastMatch   = src.lastMatch;
    dst.matchsToWin = src.matchsToWin;
    dst.p1matchWins = src.p1matchWins;
    dst.p2matchWins = src.p2matchWins;

    dst.life              = src.life;
    dst.power             = src.power;
    dst.attack            = src.attack;
    dst.defence           = src.defence;
    dst.team1VS2Life      = src.team1VS2Life;
    dst.turnsRecoveryRate = src.turnsRecoveryRate;
    dst.sharedLife        = src.sharedLife;

    dst.cam.zoom      = src.zoom;
    dst.cam.zoomMin   = src.zoomMin;
    dst.cam.zoomMax   = src.zoomMax;
    dst.cam.zoomSpeed = src.zoomSpeed;

    dst.numSimul.assign(1, src.numSimul);

    dst.operatingSystem = src.operatingSystem;
    dst.gameMode        = src.gameMode;
    dst.gameService     = src.gameService;
    dst.playerSide      = src.playerSide;
    dst.pauseVar        = src.pauseVar;
    dst.tourneyState    = src.tourneyState;

    dst.lifebarDisplay = src.lifebarDisplay;
    dst.gameType       = src.gameType;

    dst.persistLife      = src.persistLife;
    dst.lifePersistence  = src.lifePersistence;
    dst.persistPower     = src.persistPower;
    dst.powerPersistence = src.powerPersistence;
    dst.persistRoundtime = src.persistRoundtime;
    dst.timePersistence  = src.timePersistence;

    dst.credits = src.credits;
    dst.coins   = src.coins;

    dst.playerReward  = src.playerReward;
    dst.playerLife    = src.playerLife;
    dst.playerPower   = src.playerPower;
    dst.playerAttack  = src.playerAttack;
    dst.playerDefence = src.playerDefence;

    dst.abyssDepth           = src.abyssDepth;
    dst.abyssDepthBoss       = src.abyssDepthBoss;
    dst.abyssDepthBossSpecial = src.abyssDepthBossSpecial;
    dst.abyssBossFight       = src.abyssBossFight;
    dst.abyssFinalDepth      = src.abyssFinalDepth;
    dst.abyssSP1             = src.abyssSP1;
    dst.abyssSP2             = src.abyssSP2;
    dst.abyssSP3             = src.abyssSP3;
    dst.abyssSP4             = src.abyssSP4;

    dst.inputDisplay   = src.inputDisplay;
    dst.attackDisplay  = src.attackDisplay;
    dst.powerStateP1   = src.powerStateP1;
    dst.powerStateP2   = src.powerStateP2;
    dst.lifeStateP1    = src.lifeStateP1;
    dst.lifeStateP2    = src.lifeStateP2;
    dst.dummyState     = src.dummyState;
    dst.dummyDistance  = src.dummyDistance;
    dst.dummyGuard     = src.dummyGuard;
    dst.dummyRecovery  = src.dummyRecovery;
    dst.counterHit     = src.counterHit;

    dst.debugdraw   = src.dbgdw;
    dst.clsndraw    = src.clsndw;
    dst.statusDraw  = src.stsdw;
    dst.autolevel   = src.alvl;
    dst.powerShare  = src.powsh;
    dst.suaveMode   = src.suaveMode;
    dst.exitMatch   = src.exitMatch;

    dst.debugScript = src.dlua;
}

// ── Internal snapshot state ────────────────────────────────────────────
// Other modules (common_service, etc.) can push their state into this
// snapshot via share_pull_from_*, and restore from it via share_push_to_*.
static ShareData g_snapshot;

void share_copy(ShareData& dst) {
    dst = g_snapshot;
}

void share_push(const ShareData& src) {
    g_snapshot = src;
}

// No-arg convenience wrappers:
//   share_copy() — update g_snapshot from all wired module pull functions.
//   share_push()  — push g_snapshot to all wired module push functions.
//
// These are called by the SSZ bridge (ShareCopy/SharePush) and will be
// progressively wired as each dependent SSZ module is converted.
// Currently a no-op because g_snapshot is populated by explicit
// share_pull_from_* calls from converted modules, not automatically.

void share_copy() {
    // TODO: Pull from all wired modules into g_snapshot.
    // Once common_service is fully integrated:
    //   share_pull_from_common(g_commonDataRef, g_snapshot);
}

void share_push() {
    // TODO: Push g_snapshot to all wired modules.
    // Once common_service is fully integrated:
    //   share_push_to_common(g_snapshot, g_commonDataRef);
}

} // namespace ikemen::ssz_native
