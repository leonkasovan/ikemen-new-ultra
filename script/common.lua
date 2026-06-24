-- ===========================================================
-- COMMON.LUA — shared helpers and globals for the screenpack
-- ===========================================================
-- Loaded by: require("script.common") from main.lua
-- Also shared with pause.lua

-- Resolution / layout
local scrW = 320
local scrH = 240
resolutionWidth  = scrW * 2   -- 640
resolutionHeight = scrH * 2   -- 480

-- Fade defaults
MainFadeInTime  = 12
MainFadeOutTime = 12

-- Attract mode defaults
attractSeconds   = 30
gameTick         = 60
attractTimer     = attractSeconds * gameTick
attractDemoTimer = 0

-- Font / sprite paths (relative to engine root)
fontDebug       = "font.fnt"
fontSys         = "font.fnt"
fontNum         = "font.fnt"
fightDef        = "data/fight.def"
saveCfgPath     = "script/save/config.lua"

-- Command objects (indices used by the C bridge)
p1Cmd = 1
p2Cmd = 2

-- Sound system reference (set after sndNew())
sndSys = 0

-- Sprite references
sprFade = -1          -- fade animation handle
sprSys  = -1          -- system sprite handle

-- ===========================================================
-- TEXT / ANIM / SPRITE OBJECT CREATION
-- ===========================================================

-- Create a text image and return its ID
function createTextImg(font, bank, align, text, x, y, sx, sy)
    local id = textImgNew()
    textImgSetFont(id, font or 0)
    textImgSetText(id, text or "")
    textImgSetPos(id, x or 0, y or 0)
    textImgSetBank(id, bank or 0)
    textImgSetAlign(id, align or 0)
    if sx then textImgSetScale(id, sx, sy or sx) end
    return id
end

-- Create an animation from an SFF file + frame data
function createAnim(sffIdx, frameData, x, y)
    local id = animNew(sffIdx, frameData)
    if id ~= nil then
        animSetPos(id, x or 0, y or 0)
    end
    return id
end

-- ===========================================================
-- DRAW HELPERS called every frame
-- ===========================================================

-- Bottom menu sprites (background bar, decorations)
function drawBottomMenuSP()
    -- Screenpack background elements — override in motif.lua
end

-- Middle menu sprites (header bar, separators)
function drawMiddleMenuSP()
    -- Screenpack middle layer — override in motif.lua
end

-- Title screen input hints
function drawTitleInputHints()
    -- Override in motif.lua
end

-- Main menu input hints (shown below menu)
function drawMainMenuInputHints()
    -- Override in motif.lua
end

-- Attract-mode status overlay
function drawAttractStatus()
    -- Override in motif.lua
end

-- ===========================================================
-- TEXT RENDERING HELPERS
-- ===========================================================

-- Update a text image and return its ID (for chaining in textImgDraw)
function f_updateTextImg(id, font, bank, align, text, x, y)
    textImgSetFont(id, font or 0)
    textImgSetText(id, text or "")
    textImgSetPos(id, x or 0, y or 0)
    textImgSetBank(id, bank or 0)
    textImgSetAlign(id, align or 0)
    return id
end

-- Draw text at position with optional scaling
function f_textRender(txtId, txt, align, x, y, scaleX, scaleY)
    textImgSetAlign(txtId, align or 0)
    textImgSetPos(txtId, x or 0, y or 0)
    if scaleX then
        textImgSetScale(txtId, scaleX or 1, scaleY or scaleX or 1)
    end
    textImgSetText(txtId, txt or "")
    textImgDraw(txtId)
end

-- Draw a system time string
function f_sysTime()
    -- Override in motif.lua to show real time
end

-- Draw the title text
function f_titleText()
    -- Override in motif.lua
end

-- Draw credits on attract mode
function f_attractCredits()
    -- Override in motif.lua
end

-- ===========================================================
-- FADE ANIMATIONS
-- ===========================================================

-- Create a fade animation
function f_fadeAnim(time, method, color, spr)
    return spr or 0
end

-- ===========================================================
-- ANIMATION HELPERS
-- ===========================================================

-- Set velocity on an animation and return it (for chaining)
function f_animVelocity(anim, vx, vy)
    animVelocity(anim, vx or 0, vy or 0)
    return anim
end

-- Dynamic alpha tween
function f_dynamicAlpha(anim, minA, maxA, speed, srcA, dstA)
    -- Override in motif.lua for per-anim blinking
end

-- ===========================================================
-- MENU NAVIGATION
-- ===========================================================

-- Call a function stored in a menu entry
function f_gotoFunction(entry)
    if entry and entry.func then
        entry.func()
    elseif entry and type(entry) == "table" then
        -- Try calling the entry itself as a function
    end
end

-- Draw a single menu item with offset
function drawMenuItem(menuTable, selectedIdx, offset, font, bank, align, x, y)
    local itemIdx = selectedIdx + offset
    if itemIdx < 1 then itemIdx = itemIdx + #menuTable end
    if itemIdx > #menuTable then itemIdx = itemIdx - #menuTable end
    local item = menuTable[itemIdx]
    if item then
        local isCenter = (offset == 0)
        textImgDraw(f_updateTextImg(
            item.id, font,
            isCenter and 5 or bank or 0,
            align or 0,
            item.text or "",
            x or 159,
            y or 174
        ))
    end
end

-- Reset temporary state
function f_resetTemp()
    closeText = 1
    infoScreen = false
    infoboxScreen = false
    sideScreen = false
    charsInfo = false
    stagesInfo = false
    configInfo = false
    setGameMode("")
end

-- Reset menu arrow positions
function f_resetMenuArrowsPos()
    -- Override in motif.lua
end

-- ===========================================================
-- GAME STATE SETTERS (C-backed callbacks, Lua display stubs)
-- ===========================================================
-- setGameMode, setCredits, getCredits, setRoundTime, setRoundsToWin,
-- setCountdown, setPlayerSide, getPlayerSide, setHomeTeam, getHomeTeam,
-- remapInput are registered as C callbacks in initLua().
-- Only display-flag stubs remain in Lua (these don't need C state).

function setLifebarDisplay(v)    end
function setScoreDisplay(v)      end
function setMatchnoDisplay(v)    end
function setAilevelDisplay(v)    end
function setTimerDisplay(v)      end
function setCountdownDisplay(v)  end
function setRewardDisplay(v)     end
function setP1winsDisplay(v)     end
function setP2winsDisplay(v)     end
function setPersistLife(v)       end
function setPersistPower(v)      end
function setPersistRoundTime(v)  end

-- ===========================================================
-- SELECT SCREEN (simplified)
-- ===========================================================

function f_selectSimple()
    -- Minimal select — jumps directly to fight
end

function f_selectAdvance()
    -- Advanced select with character selection
end

function f_selectInit()
    -- Initialize select screen
end

function f_selectReset()
    -- Reset select screen state
end

function f_rosterReset()
    -- Reset roster
end

function f_makeRoster()
    -- Build character roster
end

-- ===========================================================
-- INFO / INFOBOX SCREENS
-- ===========================================================

function f_infoMenu()
    -- Override in motif.lua
end

function f_infoReset()
    infoScreen = false
    charsInfo = false
    stagesInfo = false
    configInfo = false
    towerInfo = false
    bossInfo = false
    bonusInfo = false
end

function f_infoboxMenu()
    -- Override in motif.lua
end

function f_infoboxReset()
    infoboxScreen = false
end

-- ===========================================================
-- SIDE SELECT (P1/P2 side picker)
-- ===========================================================

function f_sideSelect()
    -- Override in motif.lua
end

function f_sideReset()
    sideScreen = false
end

-- ===========================================================
-- CONFIRM / EXIT MENUS
-- ===========================================================

function f_confirmMenu(txt, font, bank, x, y, scaleX, scaleY, spacing, limit)
    -- Override in motif.lua
    return nil, nil
end

function f_confirmReset()
end

function f_exitMenu()
    -- Exit game
    sszReload()
end

-- ===========================================================
-- SECRET / COMING SOON
-- ===========================================================

function f_secret()
    f_comingSoon()
end

function f_comingSoon()
    -- Override in motif.lua
end

-- ===========================================================
-- DEFAULTS
-- ===========================================================

function f_default()
    -- Reset game settings to defaults
    data = data or {}
    data.gameMode = ""
    data.recordMode = "versus"
    data.attractMode = false
    data.aiFight = false
    data.versusScreen = true
    data.victoryscreen = true
    data.serviceScreen = false
    data.nextStage = false
    data.stageMenu = false
    data.stage = ""
    data.bgm = ""
    data.p1In = 2
    data.p2In = 2
    data.p1SelectMenu = true
    data.p2SelectMenu = true
    data.p1TeamMenu = {mode = 0, chars = 1}
    data.p2TeamMenu = {mode = 0, chars = 1}
    data.p1Char = {}
    data.p2Char = {}
    data.coop = false
    data.p2Faces = false
    data.difficulty = 4
    data.fadeTitle = 0
    data.arcadeIntro = false
    data.arcadeEnding = false
    data.debugMode = false
end

-- ===========================================================
-- MENU MUSIC
-- ===========================================================

function f_menuMusic()
    -- Play menu BGM if not already playing
end

-- ===========================================================
-- SOUNDTRACK TABLES
-- ===========================================================

function f_soundtrack()
    -- Load soundtrack tables — override in motif.lua
end

-- ===========================================================
-- STORYBOARD
-- ===========================================================

function f_storyboard(sb)
    -- Play a storyboard — override in motif.lua
end

function f_resetFadeBGM()
    -- Reset BGM fade state
end

-- ===========================================================
-- STATS / SAVE / UNLOCK
-- ===========================================================

stats = {
    firstRun = false,
    continueCount = 0,
    modes = {
        arcade = {clear = 0, progress = 0},
        survival = {clear = 0, progress = 0},
        boss = {clear = 0, progress = 0},
        training = {clear = 0, progress = 0},
    },
}

function f_saveStats()
end

function f_generateUnlocks()
end

function f_unlock(silent)
end

function f_updateUnlocks()
end

-- ===========================================================
-- MENU TABLES
-- ===========================================================

-- Menu tables use FUNCTION WRAPPERS so the real function is resolved
-- at call time, not at require("script.common") time. This avoids
-- the auto-stub capturing stale references before main.lua defines them.

-- Main Menu items
t_mainMenu = {
    {text = "ARCADE",       func = function() f_arcadeBoot() end},
    {text = "VERSUS",       func = function() f_vsBoot() end},
    {text = "PRACTICE",     func = function() f_practiceMenu() end},
    {text = "ARCADE MODE",  func = function() f_arcadeMenu() end},
    {text = "VERSUS MODE",  func = function() f_vsMenu() end},
    {text = "TRAINING",     func = function() f_training() end},
    {text = "WATCH",        func = function() f_watchMenu() end},
    {text = "OPTIONS",      func = function() f_optionsMenu() end},
    {text = "EXIT",         func = function() f_exitMenu() end},
}

-- Arcade sub-menu
t_arcadeMenu = {
    {text = "ARCADE",          func = function() arcadeHumanvsCPU() end},
    {text = "ARCADE CO-OP",    func = function() arcadeP1P2vsCPU() end},
    {text = "TOWER",           func = function() f_towerBoot() end},
    {text = "BOSS RUSH",       func = function() f_bossrushBoot() end},
    {text = "BOSS SELECT",     func = function() f_bossChars() end},
}

-- Versus sub-menu
t_vsMenu = {
    {text = "VS MODE",       func = function() freeHumanvsCPU() end},
    {text = "QUICK MATCH",   func = function() f_quickvsBoot() end},
    {text = "FREE BATTLE",   func = function() f_vsBoot() end},
    {text = "SURVIVAL",      func = function() f_survivalMenu() end},
    {text = "SCORE ATTACK",  func = function() f_scoreattackMenu() end},
}

-- Practice sub-menu
t_practiceMenu = {
    {text = "TRAINING",         func = function() f_training() end},
    {text = "CHALLENGES",       func = function() f_challengeMenu() end},
    {text = "TIME ATTACK",      func = function() f_timeattackMenu() end},
    {text = "SURVIVAL",         func = function() f_survivalMenu() end},
    {text = "BONUS GAMES",      func = function() f_bonusMenu() end},
}

-- Watch sub-menu
t_watchMenu = {
    {text = "REPLAYS",          func = function() f_replayMenu() end},
    {text = "PLAYER PROFILE",   func = function() f_profileMenu() end},
    {text = "SOUND TEST",       func = function() f_songMenu() end},
    {text = "LICENSES",         func = function() f_licenseMenu() end},
}

-- Empty sub-menus (filled by motif/loader)
t_bonusMenu      = {}
t_bonusExtras    = {}
t_challengeMenu  = {}
t_survivalMenu   = {}
t_scoreattackMenu = {}
t_timeattackMenu = {}
t_extrasMenu     = {}
t_profileMenu    = {}

-- Training chars
t_trainingChar = {}
t_bossChars = {}
t_bossSingle = {}
t_selTower = {}

-- ===========================================================
-- COMMON CONSTANTS
-- ===========================================================

-- Fade palette sprite
sprFade = 0

-- Default font handle
jgFnt = 0

-- Text object IDs (created on first use)
txt_titleFt      = createTextImg(0, 0, 0, "", 0, 0)
txt_mainTitle    = createTextImg(0, 0, 0, "", 0, 0)
txt_gameFt       = createTextImg(0, 0, 0, "", 0, 0)
txt_version      = createTextImg(0, 0, 0, "", 0, 0)
txt_f1           = createTextImg(0, 0, 0, "", 0, 0)
txt_attractTimer = createTextImg(0, 0, 0, "", 0, 0)
txt_attractCopyrightCfg = createTextImg(0, 0, 0, "", 0, 0)
txt_attractCopyright    = createTextImg(0, 0, 0, "", 0, 0)
txt_mainSelect  = createTextImg(0, 0, 0, "", 0, 0)
txt_msgIce      = createTextImg(0, 0, 0, "", 0, 0)
txt_charsNumpa   = createTextImg(0, 0, 0, "", 0, 0)

-- BGM paths
bgmIntro   = "sound/intro.ogg"
bgmIntroJP = "sound/intro_jp.ogg"
bgmTitle   = "sound/title.ogg"

-- Sound paths
sndSysPath = "sound/system.snd"

-- Animation / sprite handles (created on first use)
cursorBox      = 0
menuArrowUp    = 0
menuArrowDown  = 0
titleBG0       = 0
titleBG1       = 0
titleBG2       = 0
titleBG3       = 0
titleBG4       = 0
titleBG5       = 0
inputHintsBG   = 0

-- Data table (global state)
data = data or {}

-- ===========================================================
-- BOOT — load system assets
-- ===========================================================

-- Load the system sound bank
local function initSound()
    local snd = sndNew(sndSysPath)
    if snd ~= nil then
        sndSys = snd
    end
end

-- Try to load system.sff for sprites
local function initSprites()
    local sff = sffNew("data/system.sff")
    if sff ~= nil then
        sprSys = sff
    end
end

-- Try to load font
local function initFonts()
    local fnt = fontNew(fontDebug)
    if fnt ~= nil then
        jgFnt = fnt
    end
end

-- Load assets silently (may fail on first boot, that's OK)
initSound()
initSprites()
initFonts()

-- Set version text
textImgSetText(txt_version, "v2.0.0")
textImgSetText(txt_f1, "F1 - INFO")

-- Exit menu palette text
t_exitMenu = {
    {text = "YES"},
    {text = "NO"},
}

-- ===========================================================
-- PLAYER STATS/REPLAY (minimal stubs)
-- ===========================================================

function f_replayMenu()
    f_comingSoon()
end

function f_licenseMenu()
    f_comingSoon()
end

function f_songMenu()
    f_comingSoon()
end

function f_profileMenu()
    f_comingSoon()
end

-- ===========================================================
-- INITIALIZE DEFAULTS
-- ===========================================================

f_default()
