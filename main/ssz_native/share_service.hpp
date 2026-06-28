// share_service.hpp — Native C++ equivalent of ssz_script/ssz/share.ssz
//
// share.ssz is a snapshot object that copies game state from various SSZ
// subsystems (com, chr, cmd, fnt, snd, cfg, se, sc, stage) and pushes it
// back.  The native equivalent is ShareData — a plain struct + copy/push
// operations.
//
// Design note: ShareData, share_copy, and share_push live directly in
// ikemen::ssz_native (not ikemen::ssz_native::share) because ShareData is
// a data-transfer object (DTO) similar to SszString/SszBytes in
// ssz_value.hpp.  Free-function API services (thread, alert, crypto) each
// have their own nested namespace, but DTO types use the root namespace
// for ergonomic access from consuming modules.
//
// Phase 3: copy() and push() are stubs here.  They will be wired to real
// native state accessors as each dependent SSZ module is converted.

#pragma once

#include <string>
#include <vector>

namespace ikemen::ssz_native {

struct ShareData {
	// --- chr subsystem ---
	// c: &chr.Char         (opaque, populated when char module converted)
	// cgi: &chr.CharGlobalInfo
	// f: &chr.fgt.Fight
	// st: &stage.Stage

	// --- com subsystem ---
	int tm{};
	int nt{};
	int re{};
	int chr_home{};
	int chr_match{};
	int chr_round{};
	int lastMatch{};
	int matchsToWin{};
	int p1matchWins{};
	int p2matchWins{};
	int chr_p1mw{};
	int chr_p2mw{};
	int chr_p1w{};
	int chr_p2w{};
	int chr_drw{};
	int consecutiveWins{};
	int firstAttackCount{};
	int winTimeCount{};
	int winSpecialCount{};
	int winHyperCount{};
	int winThrowCount{};
	int winPerfectCount{};
	int winPerfectSpecialCount{};
	int winPerfectHyperCount{};
	int winPerfectThrowCount{};
	int score{};
	int scoreTotal{};
	int p1score{};
	int p2score{};
	int timer{};
	int countdownTimer{};
	int cpuLevel{};
	bool p1winsDisplay{};
	bool p2winsDisplay{};
	bool rewardDisplay{};
	bool scoreDisplay{};
	bool timerDisplay{};
	bool countdownDisplay{};
	bool ailevelDisplay{};
	bool matchnoDisplay{};
	bool gamemodeDisplay{};
	bool persistLife{};
	bool persistPower{};
	bool persistRoundtime{};
	int lifePersistence{};
	int powerPersistence{};
	int timePersistence{};
	int stt{};
	int recordState{};
	int playbackState{};
	int roundTime{};
	int roundsToWin{};
	int credits{};
	int coins{};
	int playerReward{};
	int playerLife{};
	int playerPower{};
	int playerAttack{};
	int playerDefence{};
	int abyssDepth{};
	int abyssDepthBoss{};
	int abyssDepthBossSpecial{};
	int abyssBossFight{};
	int abyssFinalDepth{};
	int inputDisplay{};
	int attackDisplay{};
	int powerStateP1{};
	int powerStateP2{};
	int lifeStateP1{};
	int lifeStateP2{};
	int dummyState{};
	int dummyDistance{};
	int dummyGuard{};
	int dummyRecovery{};
	int counterHit{};
	int lifebarDisplay{};
	int gameType{};
	int suaveMode{};
	bool exitMatch{};
	float life{};
	int power{};
	float attack{};
	float defence{};
	float team1VS2Life{};
	float turnsRecoveryRate{};
	bool sharedLife{};
	bool zoom{};
	float zoomMin{};
	float zoomMax{};
	float zoomSpeed{};
	std::vector<int> com;           // ^int
	std::vector<int> taglevel;      // ^int
	std::vector<bool> autoguard;    // ^bool
	std::vector<int> inputRemap;    // ^int
	int numSimul{};
	int wakewakaLength{};
	std::string operatingSystem;    // ^/char
	std::string gameMode;
	std::string gameService;
	std::string playerSide;
	std::string pauseVar;
	std::string tourneyState;
	std::string abyssSP1;
	std::string abyssSP2;
	std::string abyssSP3;
	std::string abyssSP4;
	float panStr{};
	bool disablePadP1{};
	bool disablePadP2{};
	int recSlot{};
	int playSlot{};
	bool playLoop{};
	std::vector<bool> includeSlot;  // ^bool
	bool cfgMod{};
	bool trnMod{};
	// in: &cfg.Keys          (opaque until cfg module converted)
	// net: &.cmd.NetInput    (opaque)
	// rep: &.cmd.FileInput   (opaque)
	// dfnt: &.fnt.Font       (opaque)
	// dlua: ^/char
	std::string dlua;
	// bgm: &.snd.Bgm         (opaque)
	bool dbgdw{};
	bool clsndw{};
	bool stsdw{};
	bool fullscr{};
	float bgmusic{};
	// camstg: &.com.Camera::Stage (opaque)
};

// copy() — snapshot current game state into dst.
// Phase 3: stub — does nothing until dependent modules are converted.
void share_copy(ShareData& dst);

// push() — restore game state from src.
// Phase 3: stub — does nothing until dependent modules are converted.
void share_push(const ShareData& src);

} // namespace ikemen::ssz_native
