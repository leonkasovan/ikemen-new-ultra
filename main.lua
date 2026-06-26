-- quick_vs.lua - Minimal CPU vs CPU: Kula vs Morrigan on Stage1
math.randomseed(os.time())

-- Engine API (required)
require("script.common")

-- Debug / match / lifebar (required by engine)
loadDebugFont(fontDebug)
setDebugScript("script/match.lua")
loadLifebar(fightDef)

-- Populate t_selChars / t_selStages / t_charDef / t_stageDef from select.def
assert(loadfile("script/loader.lua"))()

-- Look up 0-indexed cel numbers for characters
local kulaCel    = t_charDef["kula/kula.def"]
local morriganCel = t_charDef["morrigan/morrigan.def"]
print("Kula cel: " .. kulaCel)
print("Morrigan cel: " .. morriganCel)

-- Look up 1-indexed stage number
local stageNo = t_stageDef["stages/china.def"]
print("Stage1 number: " .. stageNo)

-- Match config
setGameMode("arcade")
setRoundTime(99)
setRoundsToWin(2)
setLifeMul(1.0)
setTeamMode(1, 0, 1)   -- P1 single, 1 char
setTeamMode(2, 0, 1)   -- P2 single, 1 char
setCom(1, 4)            -- P1 CPU (difficulty 4)
setCom(2, 4)            -- P2 CPU (difficulty 4)

-- Start match session
selectStart()
selectChar(1, kulaCel, 1)
selectChar(2, morriganCel, 1)
setStage(stageNo)
selectStage(stageNo)

-- Run until match ends
winner = game()
