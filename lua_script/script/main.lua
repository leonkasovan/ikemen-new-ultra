-- Quick match
math.randomseed(os.time())

-- Engine API (required)
require("script.common")

-- Debug / match / lifebar (required by engine)
loadDebugFont(fontDebug)
setDebugScript("script/match.lua")
loadLifebar(fightDef)

-- Populate t_selChars / t_selStages from select.def
-- Try full loader.lua first (rich character/stage metadata), fall back to minimal inline parser
local ok, err = pcall(dofile, "script/loader.lua")
if not ok then
	print("[main.lua] loader.lua failed: " .. tostring(err))
	print("[main.lua] Using minimal inline parser...")
	t_selChars = {}
	t_selStages = {}
	local section = 0
	local file = io.open(selectDef, "r")
	if file then
		local content = file:read("*all")
		file:close()
		content = content:gsub('([^\r\n]*)%s*;[^\r\n]*', '%1')
		for line in content:gmatch('[^\r\n]+') do
			line = line:lower()
			line = line:gsub('%s+', ' ')
			if line:match('^%s*%[%s*characters%s*%]') then
				section = 1
			elseif line:match('^%s*%[%s*extrastages%s*%]') then
				section = 2
			elseif section == 1 and line:match('%S') and not line:match('^%s*%[') then
				local c = line:gsub('^%s*(.-)%s*$', '%1')
				c = c:gsub(',.*$', ''):gsub('\\', '/')
				if c ~= '' then
					local row = #t_selChars + 1
					t_selChars[row] = {char = c, unlock = "true"}
					addChar(c)
					print("[main.lua] Loading char " .. c)
				end
			elseif section == 2 and line:match('%S') and not line:match('^%s*%[') then
				local c = line:gsub('^%s*(.-)%s*$', '%1')
				c = c:gsub(',.*$', ''):gsub('\\', '/')
				if c ~= '' and c:match('%.[Dd][Ee][Ff]') then
					local row = #t_selStages + 1
					t_selStages[row] = {stage = c, unlock = "true"}
					addStage(c)
					print("[main.lua] Loading stage " .. c)
				end
			end
		end
	end
end

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
