-- Quick match
math.randomseed(os.time())

-- Engine API (required)
require("script.common")

-- Debug / match / lifebar (required by engine)
loadDebugFont(fontDebug)
setDebugScript("script/match.lua")
loadLifebar(fightDef)

-- Populate t_selChars / t_selStages / t_charDef / t_stageDef from select.def
assert(loadfile("script/loader.lua"))()

-- Randomly select characters (t_selChars is 1-indexed)
print("Total characters: " .. #t_selChars)
local player1 = math.random(#t_selChars)
local player2 = math.random(#t_selChars)
print("Player1: " .. player1)
print("Player2: " .. player2)

-- Randomly select stage (t_selStages is 1-indexed)
print("Total stages: " .. #t_selStages)
local stageNo = math.random(#t_selStages)
print("Selected stage: " .. stageNo)

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
selectChar(1, player1, 1)
selectChar(2, player2, 1)
setStage(stageNo)
selectStage(stageNo)

-- Run until match ends
winner = game()
