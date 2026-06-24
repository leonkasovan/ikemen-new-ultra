#include "ikemen.hpp"
#include "common.hpp"

#include "../alpha/sdlplugin.hpp"
#include "../alpha/lua.hpp"
#include "../save/config.hpp"
#include "../shell.hpp"
#include "../ssz.hpp"
#include "../file.hpp"
#include "sff.hpp"
#include "mugen_sff_loader.hpp"
#include "font.hpp"
#include "sound.hpp"
#include "animation.hpp"
#include "system.hpp"
#include "system-script.hpp"
#include "../string.hpp"

#include "sszdef.h"

#include <cstdlib>
#include <cstring>
#include <ctime>
#include <map>
#include <sstream>

// Lua C API — used directly for pushcfunction / setglobal
extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

// SDL_mixer — used by sndStop() to halt all audio playback
#include <SDL_mixer.h>

namespace ikemen {

// ── SSR Reload ──────────────────────────────────────────────────────────

void sszReload()
{
	LOG_INFO("ENGINE", "sszReload called — restarting");
	ikemen::open(ikemen::config::Executable, L"", L"", false, false);
}

void loaderReset()
{
	LOG_DEBUG("ENGINE", "loaderReset");
}

// ── Event-driven game loop ──────────────────────────────────────────────

static bool g_running = true;
static int  g_matchResult = 0;

// ── Font and text image storage ───────────────────────────────────────

struct TextImgState {
	int fontIndex = -1;
	std::string text;
	float x = 0.0f, y = 0.0f;
	float xscl = 1.0f, yscl = 1.0f;
	int bank = 0;
	int align = 0;
	int salpha = 255, dalpha = 0;
};

static std::vector<GameFont> g_fonts;
static std::vector<TextImgState> g_textImgs;

// ── Sprite cache (must be before doGameLoop) ────────────────────────────
static std::map<uint64_t, Sprite> g_spriteCache;

// ── Sprite cache ─────────────────────────────────────────────────────────
// Uses the full Sprite struct (pxl, colorPallet, pluginbuf, rct, rle)

static uint64_t spriteKey(int grp, int num) { return (uint64_t(grp) << 32) | uint32_t(num); }

static bool loadSpriteFromSFF(Sprite& sp, int grp, int num)
{
	File sf;
	if (!sf.open(L"data/system.sff", L"rb")) return false;

	SffHeader hdr; uint32_t lofs, tofs;
	std::wstring err = hdr.read(sf, lofs, tofs);
	if (!err.empty()) { sf.close(); return false; }

	if (hdr.numberOfSprites == 0) { sf.close(); return false; }

	PalleteList pl;

	if (hdr.ver0 == 2) {
		// V2: preload palettes before scanning (matching loadIkemenSff pattern)
		for (uint32_t i = 0; i < hdr.numberOfPalettes; i++) {
			uint32_t palOfs = hdr.firstPaletteHeaderOffset + i * 16;
			sf.seek(palOfs, Seek::SET);
			uint16_t gn[3] = {}; sf.read(gn[0]); sf.read(gn[1]); sf.read(gn[2]);
			uint16_t link = 0; sf.read(link);
			uint32_t pofs = 0, psiz = 0; sf.read(pofs); sf.read(psiz);
			if (psiz > 0) {
				uint32_t newIdx;
				std::vector<uint32_t>& pal = pl.newPal(newIdx);
				sf.seek(static_cast<int64_t>(lofs) + pofs, Seek::SET);
				int nc = static_cast<int>(psiz) / 4;
				if (nc > 256) nc = 256;
				for (int c = 0; c < nc; c++) {
					uint8_t r, g, b, a;
					sf.read(r); sf.read(g); sf.read(b); sf.read(a);
					if (hdr.ver2 == 0) a = (c == 0) ? 0 : 255;
					// Store as 0xAARRGGBB to match SDL_PIXELFORMAT_ARGB8888 (little-endian)
					pal[static_cast<size_t>(c)] =
						(static_cast<uint32_t>(a) << 24) |
						(static_cast<uint32_t>(r) << 16) |
						(static_cast<uint32_t>(g) << 8) |
						static_cast<uint32_t>(b);
				}
				pl.setSource(static_cast<int>(newIdx), pal);
			}
		}

		// V2: scan sprites with linked sprite resolution (matching loadIkemenSff)
		// Maintain a sprite list so linked sprites (siz==0) can resolve via shareCopy
		std::vector<Sprite> spriteList;
		spriteList.reserve(hdr.numberOfSprites);

		for (uint32_t i = 0; i < hdr.numberOfSprites; i++) {
			uint32_t hdrOfs = hdr.firstSpriteHeaderOffset + i * 28;
			sf.seek(hdrOfs, Seek::SET);
			uint32_t ofs = 0, siz = 0; uint16_t idxlnked = 0;

			Sprite scanSp;
			if (!scanSp.readHeaderV2(sf, ofs, siz, lofs, tofs, idxlnked)) {
				LOG_INFO("ENGINE", "loadSpriteFromSFF: readHeaderV2 failed at sprite[%u] — skipping", i);
				continue;
			}

			if (siz > 0) {
				std::wstring v2err = scanSp.readV2(sf, static_cast<int64_t>(ofs), siz);
				if (!v2err.empty()) {
					LOG_INFO("ENGINE", "loadSpriteFromSFF: readV2 err sprite[%u] (%d,%d): %ls",
					         i, scanSp.imageGroup, scanSp.imageNumber, v2err.c_str());
					continue;
				}
				scanSp.colorPallet = scanSp.getPal(pl);
			} else if (idxlnked < static_cast<uint16_t>(spriteList.size())) {
				// Linked sprite — copy from the previously loaded source
				scanSp.shareCopy(spriteList[idxlnked]);
			}

			// Check if this is the target sprite
			if (scanSp.imageGroup == grp && scanSp.imageNumber == num) {
				sp = std::move(scanSp);
				sf.close();
				return sp.rct.w > 0 && sp.rct.h > 0;
			}

			// Store for potential future linked sprite resolution
			spriteList.push_back(std::move(scanSp));
		}
	} else {
		// V1: headers interleaved with data
		// Each header's first field (bytes 0-3) is the file offset to the NEXT header
		// Maintain prevSprite tracking for palette sharing (matching loadIkemenSff)
		uint32_t shofs = hdr.firstSpriteHeaderOffset;
		Sprite* prevSprite = nullptr;
		std::vector<Sprite> loaded;
		loaded.reserve(hdr.numberOfSprites); // prevents pointer invalidation on push_back

		for (uint32_t i = 0; i < hdr.numberOfSprites && shofs != 0; i++) {
			sf.seek(shofs, Seek::SET);
			uint32_t nextHdrOfs = 0, sz = 0; uint16_t prev = 0;
			sf.read(nextHdrOfs); sf.read(sz);

			Sprite scanSp;
			scanSp.readHeader(sf);
			sf.read(prev);

			if (scanSp.imageGroup == grp && scanSp.imageNumber == num) {
				int64_t dataOfs = static_cast<int64_t>(shofs) + 32;
				std::wstring readErr = scanSp.read(sf, hdr, dataOfs, sz, nextHdrOfs,
				                                   prevSprite, pl, false);
				if (readErr.empty()) {
					scanSp.colorPallet = scanSp.getPal(pl);
					sp = std::move(scanSp);
					sf.close();
					return sp.rct.w > 0 && sp.rct.h > 0;
				} else {
					LOG_INFO("ENGINE", "Sprite::read error for (%d,%d): %ls", grp, num, readErr.c_str());
					sf.close();
					return false;
				}
			}

			// Load non-target sprites too, to keep prevSprite valid for palette sharing
			if (sz > 0) {
				int64_t dataOfs = static_cast<int64_t>(shofs) + 32;
				std::wstring readErr = scanSp.read(sf, hdr, dataOfs, sz, nextHdrOfs,
				                                   prevSprite, pl, true);
				if (readErr.empty()) {
					scanSp.colorPallet = scanSp.getPal(pl);
					loaded.push_back(std::move(scanSp));
					prevSprite = &loaded.back();
				}
			}

			shofs = nextHdrOfs;
		}
	}
	sf.close();
	return false;
}

static int l_sprNew(lua_State* L)
{
	int grp = (int)luaL_checkinteger(L, 1);
	int num = (int)luaL_checkinteger(L, 2);
	uint64_t key = spriteKey(grp, num);

	if (g_spriteCache.find(key) == g_spriteCache.end()) {
		Sprite sp;
		if (loadSpriteFromSFF(sp, grp, num)) {
			g_spriteCache[key] = std::move(sp);
			LOG_DEBUG("ENGINE", "sprNew(%d,%d) loaded %dx%d", grp, num,
			          g_spriteCache[key].rct.w, g_spriteCache[key].rct.h);
		} else {
			LOG_DEBUG("ENGINE", "sprNew(%d,%d) NOT FOUND in system.sff", grp, num);
		}
	}
	lua_pushinteger(L, (lua_Integer)key);
	return 1;
}

static int l_sprDraw(lua_State* L)
{
	uint64_t key = (uint64_t)luaL_checkinteger(L, 1);
	int px = (int)luaL_optinteger(L, 2, 0);
	int py = (int)luaL_optinteger(L, 3, 0);
	int sx = (int)luaL_optinteger(L, 4, 256);
	int sy = (int)luaL_optinteger(L, 5, 256);

	auto it = g_spriteCache.find(key);
	if (it != g_spriteCache.end() && it->second.rct.w > 0) {
		auto& sp = it->second;
		int dw = sp.rct.w * sx / 256;
		int dh = sp.rct.h * sy / 256;
		ikemen::Rect dr = {px - dw/2, py - dh/2, dw, dh};
		ikemen::Rect sr = {0, 0, sp.rct.w, sp.rct.h};
		ikemen::Rect tile = {0, 0, 0, 0};
		std::vector<int8_t> buf = sp.pluginbuf;

		renderMugenZoom(dr, 0, 0, sp.pxl, sp.colorPallet,
		                0, sr, 0, 0, tile,
		                1.0f, 1.0f, 1.0f, 1.0f,
		                0, 255, sp.rle, buf);
	}
	lua_pushboolean(L, 1);
	return 1;
}

static int l_sprUpdate(lua_State* L)
{
	lua_pushboolean(L, 1);
	return 1;
}

std::vector<SffFile> g_sffFiles;

bool SffFile::loadAll(const std::wstring& path)
{
	// ── Runtime A/B toggle ────────────────────────────────────────
	// Set SFF_LOADER=mugen in the environment to use the reference mugen_sff decoder.
	// Any other value (or unset) uses the built-in Ikemen decoder.
	const char* envLoader = std::getenv("SFF_LOADER");
	bool useMugenLoader = (envLoader != nullptr && strcmp(envLoader, "mugen") == 0);

	if (useMugenLoader) {
		LOG_INFO("ENGINE", "SFF: using MUGEN reference loader (SFF_LOADER=mugen)");
		filename = std::string(path.begin(), path.end());
		std::string narrowFile;
		bool ok = loadMugenSff(path, sprites, narrowFile);
		if (ok) filename = narrowFile;
		LOG_INFO("ENGINE", "SFF: mugen loader returned %s (%zu sprites)",
		         ok ? "ok" : "FAILED", sprites.size());
		return ok;
	}

	LOG_INFO("ENGINE", "SFF: using built-in Ikemen decoder");

	File sf;
	if (!sf.open(path, L"rb")) return false;

	SffHeader hdr; uint32_t lofs, tofs;
	std::wstring err = hdr.read(sf, lofs, tofs);
	if (!err.empty()) { sf.close(); return false; }

	PalleteList pl;

	if (hdr.ver0 == 2) {
		LOG_INFO("ENGINE", "SFF v2: starting load, firstHdr=%u, numSprites=%u, numPalettes=%u",
		         hdr.firstSpriteHeaderOffset, hdr.numberOfSprites, hdr.numberOfPalettes);

		// Load v2 palettes first
		LOG_INFO("ENGINE", "SFF v2: loading %u palettes...", hdr.numberOfPalettes);
		for (uint32_t i = 0; i < hdr.numberOfPalettes; i++) {
			uint32_t palOfs = hdr.firstPaletteHeaderOffset + i * 16;
			sf.seek(palOfs, Seek::SET);
			uint16_t gn[3] = {};
			sf.read(gn[0]); sf.read(gn[1]); sf.read(gn[2]);
			uint16_t link = 0;
			sf.read(link);
			uint32_t pofs = 0, psiz = 0;
			sf.read(pofs); sf.read(psiz);

			uint32_t pi;
			if (psiz == 0) {
				pi = link;
			} else {
				pi = i;
				uint32_t newIdx;
				std::vector<uint32_t> pal(256, 0);
				pl.newPal(newIdx);
				pi = newIdx;
				sf.seek(static_cast<int64_t>(lofs) + pofs, Seek::SET);
				int numColors = static_cast<int>(psiz) / 4;
				if (numColors > 256) numColors = 256;
				for (int c = 0; c < numColors; c++) {
					uint8_t r, g, b, a;
					sf.read(r); sf.read(g); sf.read(b); sf.read(a);
					if (hdr.ver2 == 0) a = (c == 0) ? 0 : 255;
				pal[static_cast<size_t>(c)] =
					(static_cast<uint32_t>(a) << 24) |
					(static_cast<uint32_t>(r) << 16) |
					(static_cast<uint32_t>(g) << 8)  |
					(static_cast<uint32_t>(b));
			}
			pl.setSource(static_cast<int>(pi), pal);
			}
		}

		LOG_INFO("ENGINE", "SFF v2: palettes loaded, loading %u sprites...", hdr.numberOfSprites);
		uint32_t spriteCount = 0;
		std::vector<Sprite> spriteList;
		try { spriteList.resize(hdr.numberOfSprites); } catch (...) {
			LOG_INFO("ENGINE", "SFF v2: FAILED to allocate %u sprites", hdr.numberOfSprites);
			sf.close(); return false;
		}
		LOG_INFO("ENGINE", "SFF v2: sprite list allocated, starting loop");
		for (uint32_t i = 0; i < hdr.numberOfSprites; i++) {
			uint32_t hdrOfs = hdr.firstSpriteHeaderOffset + i * 28;
			if (i < 5) LOG_INFO("ENGINE", "SFF v2: load sprite[%u]", i);
			sf.seek(hdrOfs, Seek::SET);
			uint32_t ofs = 0, siz = 0; uint16_t idxlnked = 0;
			Sprite& sp = spriteList[i];
			if (!sp.readHeaderV2(sf, ofs, siz, lofs, tofs, idxlnked)) {
				LOG_INFO("ENGINE", "SFF v2: readHeaderV2 failed at sprite[%u] — skipping", i);
				continue;
			}
			if (siz == 0) {
				if (idxlnked < i) {
					sp.shareCopy(spriteList[idxlnked]);
				} else {
					sp.palidx = 0;
				}
			} else {
				int64_t dataOfs = static_cast<int64_t>(ofs);
				if (i < 5) LOG_INFO("ENGINE", "SFF v2: sprite[%u] readV2 ofs=%d siz=%u", i, (int)dataOfs, siz);
				std::wstring v2err = sp.readV2(sf, dataOfs, siz);
				if (!v2err.empty()) {
					LOG_INFO("ENGINE", "SFF v2: readV2 error at sprite[%u] grp=%d num=%d fmt=%d cd=%d: %ls",
					         i, sp.imageGroup, sp.imageNumber, -sp.rle, sp.coldepth, v2err.c_str());
					continue;
				}
			}
			sp.colorPallet = sp.getPal(pl);
			if (i < 5) LOG_INFO("ENGINE", "SFF v2: sprite[%u] getPal done", i);
			if (sp.rct.w > 0 && sp.rct.h > 0) {
				uint64_t key = spriteKey(sp.imageGroup, sp.imageNumber);
				sprites[key] = sp;
				spriteCount++;
			}
			if ((i + 1) % 100 == 0) {
				LOG_INFO("ENGINE", "SFF v2: loaded %u/%u sprites...", i + 1, hdr.numberOfSprites);
			}
		}
		LOG_INFO("ENGINE", "SFF v2: loaded %u sprites", spriteCount);
	} else {
		uint32_t shofs = hdr.firstSpriteHeaderOffset;
		uint32_t spriteCount = 0;
		// Get file size to prevent reading beyond end
		std::fseek(sf.handle(), 0, SEEK_END);
		int64_t fileSize = _ftelli64(sf.handle());
		std::fseek(sf.handle(), 0, SEEK_SET);
		LOG_INFO("ENGINE", "SFF v1: starting load, firstHdr=%u, numSprites=%u, fileSize=%lld",
		         shofs, hdr.numberOfSprites, fileSize);
		Sprite* prevSprite = nullptr;
		for (uint32_t i = 0; i < hdr.numberOfSprites && shofs != 0 && shofs < (uint32_t)fileSize; i++) {
			sf.seek(shofs, Seek::SET);
			uint32_t nextHdrOfs = 0, sz = 0; uint16_t prev = 0;
			sf.read(nextHdrOfs); sf.read(sz);
			Sprite sp;
			sp.readHeader(sf);
			sf.read(prev);
			// File position is now at shofs + 18
			// Sprite::read() will read ps from current position (byte 18 of header)
			// then seek to dataOfs for PCX header
			int64_t dataOfs = static_cast<int64_t>(shofs) + 32;
			LOG_INFO("ENGINE", "SFF v1: sprite[%u] grp=%d num=%d",
			         i, sp.imageGroup, sp.imageNumber);
			err = sp.read(sf, hdr, dataOfs, sz, nextHdrOfs, prevSprite, pl, true);
			if (err.empty()) {
				sp.colorPallet = sp.getPal(pl);
				if (sp.rct.w > 0 && sp.rct.h > 0) {
					uint64_t key = spriteKey(sp.imageGroup, sp.imageNumber);
					sprites[key] = std::move(sp);
					prevSprite = &sprites[key];
					spriteCount++;
				}
			} else {
				LOG_INFO("ENGINE", "SFF v1: sprite[%u] read error: %ls", i, err.c_str());
			}
			shofs = nextHdrOfs;
		}
		LOG_INFO("ENGINE", "SFF v1: loaded %u sprites", spriteCount);
	}
	sf.close();
	return !sprites.empty();
}

Sprite* SffFile::getSprite(int grp, int num)
{
	uint64_t key = spriteKey(grp, num);
	auto it = sprites.find(key);
	return (it != sprites.end()) ? &it->second : nullptr;
}

// ── Event-driven game loop ──────────────────────────────────────────────

static int doGameLoop()
{
	LOG_INFO("ENGINE", "Game loop: starting (ESC to exit)");
	int frameCount = 0;

	while (g_running) {
		ikemen::Event ev{};
		while (ikemen::pollEvent(ev)) {
			if (ev.etype == ikemen::EventType::QUIT) { g_running = false; break; }
			if (ev.etype == ikemen::EventType::KEYDOWN) {
				if (ev.key.ks.sym == ikemen::K::ESCAPE) { g_running = false; break; }
			}
		}

		ikemen::Rect r = {0, 0, ikemen::config::Width, ikemen::config::Height};
		ikemen::softFill(r, 0x0F0F23);

		// Update and draw all animations
		for (auto& anim : g_anims) {
			anim.update();
			anim.draw();
		}

		// Draw loaded sprites (fallback for direct sprite cache)
		int ypos = 10;
		for (auto& [key, sp] : g_spriteCache) {
			if (sp.rct.w <= 0 || sp.rct.h <= 0) continue;
			ikemen::Rect dr = {10 + sp.rct.x, ypos + sp.rct.y, sp.rct.w, sp.rct.h};
			ikemen::Rect sr = {0, 0, sp.rct.w, sp.rct.h};
			ikemen::Rect tile = {0, 0, 0, 0};
			std::vector<int8_t> buf = sp.pluginbuf;
			renderMugenZoom(dr, 0, 0, sp.pxl, sp.colorPallet,
			                0, sr, 0, 0, tile,
			                1.0f, 1.0f, 1.0f, 1.0f,
			                0, 255, sp.rle, buf);
			ypos += sp.rct.h + 10;
		}

		ikemen::flip();
		ikemen::delay(16);
		frameCount++;
	}

	LOG_INFO("ENGINE", "Game loop: exited after %d frames", frameCount);
	return g_matchResult;
}

// ── Sound storage (loaded .snd files) ───────────────────────────────────

static std::vector<ikemen::Snd> g_sndFiles;

// Current BGM filename — updated by l_playBGM, read by l_getBGM
static std::string g_currentBGM;

// ── Volume state — tracked here because the engine only exposes a setter
static float g_masterVol = 0.8f;
static float g_seVol     = 0.8f;
static float g_bgmVol    = 0.5f;

// ── BGM control — stop all audio playback
// Uses SDL_mixer directly to halt music and all SFX channels.

static void stopAllAudio()
{
	Mix_HaltMusic();
	Mix_HaltChannel(-1);
}

// ── Sound Lua Callbacks ─────────────────────────────────────────────────

static int l_sndNew(lua_State* L)
{
	const char* path = luaL_checkstring(L, 1);
	std::wstring wpath(path, path + strlen(path));

	ikemen::Snd snd;
	std::wstring err = snd.loadFile(wpath);
	if (!err.empty()) {
		LOG_INFO("ENGINE", "sndNew: %ls", err.c_str());
		lua_pushnil(L);
		return 1;
	}

	g_sndFiles.push_back(std::move(snd));
	lua_pushinteger(L, (int)g_sndFiles.size() - 1);
	return 1;
}

static int l_sndPlay(lua_State* L)
{
	int idx = (int)luaL_checkinteger(L, 1);
	int group = (int)luaL_checkinteger(L, 2);
	int number = (int)luaL_checkinteger(L, 3);

	if (idx >= 0 && idx < (int)g_sndFiles.size()) {
		ikemen::Wave* wav = g_sndFiles[idx].getSound(group, number);
		if (wav) {
			ikemen::addWave(wav);
			ikemen::playSound();
		}
	}
	return 0;
}

static int l_playBGM(lua_State* L)
{
	const char* path = luaL_checkstring(L, 1);
	g_currentBGM = path ? path : "";
	std::wstring wpath(path, path + strlen(path));
	bool ok = ikemen::playBGM(L"", wpath);
	lua_pushboolean(L, ok ? 1 : 0);
	return 1;
}

// ── getBGM() returns the filename of the currently playing BGM

static int l_getBGM(lua_State* L)
{
	lua_pushstring(L, g_currentBGM.c_str());
	return 1;
}

// ── sndStop() halts all audio (BGM + SFX)

static int l_sndStop(lua_State* L)
{
	(void)L;
	stopAllAudio();
	return 0;
}

// ── fadeInBGM / fadeOutBGM — fade BGM volume in/out over time (ms)

static int l_fadeInBGM(lua_State* L)
{
	int time = (int)luaL_checkinteger(L, 1);
	ikemen::fadeInBGM(time);
	return 0;
}

static int l_fadeOutBGM(lua_State* L)
{
	int time = (int)luaL_checkinteger(L, 1);
	ikemen::fadeOutBGM(time);
	return 0;
}

// ── setVolume(master, se, bgm) — set audio volume levels (0.0 – 1.0)
// Master controls overall output, SE is sound effects, BGM is background music.
// Calling with 1 arg sets all three to the same value.

static int l_setVolume(lua_State* L)
{
	float gv = (float)luaL_checknumber(L, 1);
	float wv = (float)luaL_optnumber(L, 2, gv);
	float bv = (float)luaL_optnumber(L, 3, gv);

	// Clamp to [0.0, 1.0]
	if (gv < 0.0f) gv = 0.0f; else if (gv > 1.0f) gv = 1.0f;
	if (wv < 0.0f) wv = 0.0f; else if (wv > 1.0f) wv = 1.0f;
	if (bv < 0.0f) bv = 0.0f; else if (bv > 1.0f) bv = 1.0f;

	g_masterVol = gv;
	g_seVol     = wv;
	g_bgmVol    = bv;

	ikemen::setVolume(gv, wv, bv);
	return 0;
}

// ── getVolume() returns three volume values: master, se, bgm

static int l_getVolume(lua_State* L)
{
	lua_pushnumber(L, g_masterVol);
	lua_pushnumber(L, g_seVol);
	lua_pushnumber(L, g_bgmVol);
	return 3;
}

// ── Animation Lua Callbacks ─────────────────────────────────────────────

static int l_sffNew(lua_State* L)
{
	const char* path = luaL_checkstring(L, 1);
	LOG_INFO("ENGINE", "sffNew: loading %s", path);
	std::wstring wpath(path, path + strlen(path));

	SffFile sff;
	sff.filename = path;
	if (!sff.loadAll(wpath)) {
		LOG_INFO("ENGINE", "sffNew: failed to load %s", path);
		lua_pushnil(L);
		return 1;
	}

	LOG_INFO("ENGINE", "sffNew: loaded %s (%zu sprites)", path, sff.sprites.size());
	g_sffFiles.push_back(std::move(sff));
	lua_pushinteger(L, (int)g_sffFiles.size() - 1);
	return 1;
}

static int l_animNew(lua_State* L)
{
	int sffIdx = (int)luaL_checkinteger(L, 1);
	const char* frameData = luaL_checkstring(L, 2);

	if (sffIdx < 0 || sffIdx >= (int)g_sffFiles.size()) {
		lua_pushnil(L);
		return 1;
	}

	ScreenAnim anim;
	anim.sffIndex = sffIdx;
	if (!parseFrameData(frameData, anim.frames)) {
		lua_pushnil(L);
		return 1;
	}

	g_anims.push_back(std::move(anim));
	lua_pushinteger(L, (int)g_anims.size() - 1);
	return 1;
}

static int l_animAddPos(lua_State* L)
{
	int idx = (int)luaL_checkinteger(L, 1);
	int x = (int)luaL_checkinteger(L, 2);
	int y = (int)luaL_checkinteger(L, 3);
	if (idx >= 0 && idx < (int)g_anims.size()) {
		g_anims[idx].addPosX += x;
		g_anims[idx].addPosY += y;
	}
	return 0;
}

static int l_animSetPos(lua_State* L)
{
	int idx = (int)luaL_checkinteger(L, 1);
	int x = (int)luaL_checkinteger(L, 2);
	int y = (int)luaL_checkinteger(L, 3);
	if (idx >= 0 && idx < (int)g_anims.size()) {
		g_anims[idx].posX = x;
		g_anims[idx].posY = y;
	}
	return 0;
}

static int l_animSetAlpha(lua_State* L)
{
	int idx = (int)luaL_checkinteger(L, 1);
	int src = (int)luaL_checkinteger(L, 2);
	int dst = (int)luaL_checkinteger(L, 3);
	if (idx >= 0 && idx < (int)g_anims.size()) {
		g_anims[idx].srcAlpha = src;
		g_anims[idx].dstAlpha = dst;
		g_anims[idx].alphaSet = true;
	}
	return 0;
}

static int l_animSetColorKey(lua_State* L)
{
	int idx = (int)luaL_checkinteger(L, 1);
	int key = (int)luaL_checkinteger(L, 2);
	if (idx >= 0 && idx < (int)g_anims.size()) {
		g_anims[idx].colorKey = key;
	}
	return 0;
}

static int l_animSetScale(lua_State* L)
{
	int idx = (int)luaL_checkinteger(L, 1);
	float xs = (float)luaL_checknumber(L, 2);
	float ys = (float)luaL_optnumber(L, 3, xs);
	if (idx >= 0 && idx < (int)g_anims.size()) {
		g_anims[idx].scaleX = xs;
		g_anims[idx].scaleY = ys;
	}
	return 0;
}

static int l_animSetTile(lua_State* L)
{
	int idx = (int)luaL_checkinteger(L, 1);
	int tx = (int)luaL_checkinteger(L, 2);
	int ty = (int)luaL_checkinteger(L, 3);
	if (idx >= 0 && idx < (int)g_anims.size()) {
		g_anims[idx].tileX = tx;
		g_anims[idx].tileY = ty;
	}
	return 0;
}

static int l_animSetWindow(lua_State* L)
{
	int idx = (int)luaL_checkinteger(L, 1);
	int x = (int)luaL_checkinteger(L, 2);
	int y = (int)luaL_checkinteger(L, 3);
	int w = (int)luaL_checkinteger(L, 4);
	int h = (int)luaL_checkinteger(L, 5);
	if (idx >= 0 && idx < (int)g_anims.size()) {
		g_anims[idx].window = {x, y, w, h};
		g_anims[idx].hasWindow = true;
	}
	return 0;
}

static int l_animReset(lua_State* L)
{
	int idx = (int)luaL_checkinteger(L, 1);
	if (idx >= 0 && idx < (int)g_anims.size()) {
		auto& a = g_anims[idx];
		a.currentFrame = 0;
		a.frameTimer = 0;
		a.finished = false;
		a.addPosX = 0;
		a.addPosY = 0;
	}
	return 0;
}

static int l_animVelocity(lua_State* L)
{
	int idx = (int)luaL_checkinteger(L, 1);
	float vx = (float)luaL_checknumber(L, 2);
	float vy = (float)luaL_checknumber(L, 3);
	if (idx >= 0 && idx < (int)g_anims.size()) {
		g_anims[idx].velX = vx;
		g_anims[idx].velY = vy;
	}
	lua_pushinteger(L, idx);  // return same anim index for chaining
	return 1;
}

static int l_animUpdate(lua_State* L)
{
	int idx = (int)luaL_checkinteger(L, 1);
	if (idx >= 0 && idx < (int)g_anims.size()) {
		g_anims[idx].update();
	}
	return 0;
}

static int l_animDraw(lua_State* L)
{
	int idx = (int)luaL_checkinteger(L, 1);
	if (idx >= 0 && idx < (int)g_anims.size()) {
		g_anims[idx].draw();
	}
	return 0;
}

static int l_animPosDraw(lua_State* L)
{
	int idx = (int)luaL_checkinteger(L, 1);
	int x = (int)luaL_optinteger(L, 2, 0);
	int y = (int)luaL_optinteger(L, 3, 0);
	if (idx >= 0 && idx < (int)g_anims.size()) {
		g_anims[idx].draw(x, y);
	}
	return 0;
}

// ── Font / Text Image Lua Callbacks ──────────────────────────────────

static int l_fontNew(lua_State* L)
{
	const char* path = luaL_checkstring(L, 1);
	std::wstring wpath(path, path + strlen(path));

	GameFont fnt;
	std::wstring err = fnt.loadFile(wpath);
	if (!err.empty()) {
		LOG_INFO("ENGINE", "fontNew: %ls", err.c_str());
		lua_pushnil(L);
		return 1;
	}

	g_fonts.push_back(std::move(fnt));
	lua_pushinteger(L, (int)g_fonts.size() - 1);
	return 1;
}

static int l_textImgNew(lua_State* L)
{
	TextImgState ti;
	g_textImgs.push_back(std::move(ti));
	lua_pushinteger(L, (int)g_textImgs.size() - 1);
	return 1;
}

static int l_textImgSetFont(lua_State* L)
{
	int idx = (int)luaL_checkinteger(L, 1);
	int fontIdx = (int)luaL_checkinteger(L, 2);
	if (idx >= 0 && idx < (int)g_textImgs.size()) {
		g_textImgs[idx].fontIndex = fontIdx;
	}
	return 0;
}

static int l_textImgSetText(lua_State* L)
{
	int idx = (int)luaL_checkinteger(L, 1);
	const char* txt = luaL_checkstring(L, 2);
	if (idx >= 0 && idx < (int)g_textImgs.size()) {
		g_textImgs[idx].text = txt;
	}
	return 0;
}

static int l_textImgSetPos(lua_State* L)
{
	int idx = (int)luaL_checkinteger(L, 1);
	float x = (float)luaL_checknumber(L, 2);
	float y = (float)luaL_checknumber(L, 3);
	if (idx >= 0 && idx < (int)g_textImgs.size()) {
		g_textImgs[idx].x = x;
		g_textImgs[idx].y = y;
	}
	return 0;
}

static int l_textImgAddPos(lua_State* L)
{
	int idx = (int)luaL_checkinteger(L, 1);
	float x = (float)luaL_checknumber(L, 2);
	float y = (float)luaL_checknumber(L, 3);
	if (idx >= 0 && idx < (int)g_textImgs.size()) {
		g_textImgs[idx].x += x;
		g_textImgs[idx].y += y;
	}
	return 0;
}

static int l_textImgSetBank(lua_State* L)
{
	int idx = (int)luaL_checkinteger(L, 1);
	int bank = (int)luaL_checkinteger(L, 2);
	if (idx >= 0 && idx < (int)g_textImgs.size()) {
		g_textImgs[idx].bank = bank;
	}
	return 0;
}

static int l_textImgSetAlign(lua_State* L)
{
	int idx = (int)luaL_checkinteger(L, 1);
	int align = (int)luaL_checkinteger(L, 2);
	if (idx >= 0 && idx < (int)g_textImgs.size()) {
		g_textImgs[idx].align = align;
	}
	return 0;
}

static int l_textImgSetScale(lua_State* L)
{
	int idx = (int)luaL_checkinteger(L, 1);
	float xscl = (float)luaL_checknumber(L, 2);
	float yscl = (float)luaL_optnumber(L, 3, xscl);
	if (idx >= 0 && idx < (int)g_textImgs.size()) {
		g_textImgs[idx].xscl = xscl;
		g_textImgs[idx].yscl = yscl;
	}
	return 0;
}

static int l_textImgSetAlpha(lua_State* L)
{
	int idx = (int)luaL_checkinteger(L, 1);
	int sa = (int)luaL_checkinteger(L, 2);
	int da = (int)luaL_checkinteger(L, 3);
	if (idx >= 0 && idx < (int)g_textImgs.size()) {
		g_textImgs[idx].salpha = sa;
		g_textImgs[idx].dalpha = da;
	}
	return 0;
}

static int l_textImgGetWidth(lua_State* L)
{
	int idx = (int)luaL_checkinteger(L, 1);
	if (idx >= 0 && idx < (int)g_textImgs.size()) {
		auto& ti = g_textImgs[idx];
		if (ti.fontIndex >= 0 && ti.fontIndex < (int)g_fonts.size()) {
			float w = g_fonts[ti.fontIndex].textWidth(ti.text) * ti.xscl;
			lua_pushnumber(L, w);
			return 1;
		}
	}
	lua_pushnumber(L, 0.0);
	return 1;
}

static int l_textImgDraw(lua_State* L)
{
	int idx = (int)luaL_checkinteger(L, 1);
	LOG_DEBUG("ENGINE", "textImgDraw idx=%d textImgs.size=%zu", idx, g_textImgs.size());
	if (idx >= 0 && idx < (int)g_textImgs.size()) {
		auto& ti = g_textImgs[idx];
		LOG_DEBUG("ENGINE", "textImgDraw fontIdx=%d fonts.size=%zu text='%s' x=%.0f y=%.0f",
		          ti.fontIndex, g_fonts.size(), ti.text.c_str(), ti.x, ti.y);
		if (ti.fontIndex >= 0 && ti.fontIndex < (int)g_fonts.size()) {
			Rect emptyWindow = {0, 0, 0, 0};
			LOG_DEBUG("ENGINE", "textImgDraw -> calling drawText");
			g_fonts[ti.fontIndex].drawText(ti.x, ti.y, ti.xscl, ti.yscl,
			                                ti.bank, ti.salpha, ti.dalpha,
			                                emptyWindow, ti.align, ti.text);
			LOG_DEBUG("ENGINE", "textImgDraw <- drawText returned OK");
		} else {
			LOG_DEBUG("ENGINE", "textImgDraw fontIndex OUT OF BOUNDS");
		}
	} else {
		LOG_DEBUG("ENGINE", "textImgDraw idx OUT OF BOUNDS");
	}
	return 0;
}

// ── drawString(fontIdx, align, text, x, y, scaleX, scaleY, alpha) ───
// One-shot text renderer. Renders text using the loaded font at fontIdx,
// with optional scaling and alpha, at the given position. Unlike the
// textImg path, this does NOT need a pre-created TextImgState — useful
// for quick debug overlays, FPS counters, and temporary labels.

static int l_drawString(lua_State* L)
{
	int fontIdx = (int)luaL_checkinteger(L, 1);
	int align   = (int)luaL_optinteger(L, 2, 0);
	const char* text = luaL_checkstring(L, 3);
	float x = (float)luaL_checknumber(L, 4);
	float y = (float)luaL_checknumber(L, 5);
	float xscl = (float)luaL_optnumber(L, 6, 1.0);
	float yscl = (float)luaL_optnumber(L, 7, xscl);
	int salpha = (int)luaL_optinteger(L, 8, 255);

	if (fontIdx >= 0 && fontIdx < (int)g_fonts.size()) {
		Rect emptyWindow = {0, 0, 0, 0};
		g_fonts[fontIdx].drawText(x, y, xscl, yscl, 0, salpha, 0,
		                           emptyWindow, align, text);
	}
	return 0;
}

// ── Lua screenpack refresh callback ──────────────────────────────────
// Called each frame by the Lua screenpack's while-true loops.
// Flips the screen, clears the backbuffer, and applies frame delay.
// NOTE: Event polling is handled by cmdInput(), so refresh() no longer
// polls events. QUIT detection is done via g_running flag.

static int l_refresh(lua_State* L)
{
	// ── Frame counter (log every ~60th frame ≈ once per second) ──
	static int refreshFrame = 0;
	if ((++refreshFrame % 60) == 1) {
		LOG_INFO("ENGINE", "refresh() called — frame %d (screenpack loop is running)", refreshFrame);
	}

	// Flip the screen buffer (shows what Lua drew this frame)
	ikemen::flip();

	// Clear the backbuffer to black for the next frame
	// (matching original SSZ refresh() behavior)
	ikemen::Rect clearRect = {0, 0, ikemen::config::Width, ikemen::config::Height};
	ikemen::softFill(clearRect, 0xFF001020);

	// Check if QUIT was received by cmdInput()
	if (!g_running) {
		return luaL_error(L, "quit");
	}

	// Frame delay for ~60 FPS (16ms per frame)
	ikemen::delay(16);

	return 0;
}

// ── Game config state ───────────────────────────────────────────────────
// Backed by C globals so the screenpack's getCredits(), setGameMode(), etc.
// are real stateful functions, not auto-stubs returning '' (truthy).

static int        g_credits      = 0;
static int        g_roundTime    = 99 * 6;   // default 99 seconds in ticks
static int        g_roundsToWin  = 2;
static int        g_countdown    = 0;
static int        g_homeTeam     = 1;
static std::string g_gameMode;
static std::string g_playerSide;

// ── Game config Lua callbacks ───────────────────────────────────────────

static int l_setGameMode(lua_State* L)
{
	const char* mode = luaL_checkstring(L, 1);
	g_gameMode = mode ? mode : "";
	return 0;
}

static int l_getGameMode(lua_State* L)
{
	lua_pushstring(L, g_gameMode.c_str());
	return 1;
}

static int l_setCredits(lua_State* L)
{
	g_credits = (int)luaL_checkinteger(L, 1);
	return 0;
}

static int l_getCredits(lua_State* L)
{
	lua_pushinteger(L, g_credits);
	return 1;
}

static int l_setRoundTime(lua_State* L)
{
	g_roundTime = (int)luaL_checkinteger(L, 1);
	return 0;
}

static int l_getRoundTime(lua_State* L)
{
	lua_pushinteger(L, g_roundTime);
	return 1;
}

static int l_setRoundsToWin(lua_State* L)
{
	g_roundsToWin = (int)luaL_checkinteger(L, 1);
	return 0;
}

static int l_getRoundsToWin(lua_State* L)
{
	lua_pushinteger(L, g_roundsToWin);
	return 1;
}

static int l_setCountdown(lua_State* L)
{
	g_countdown = (int)luaL_checkinteger(L, 1);
	return 0;
}

static int l_getCountdown(lua_State* L)
{
	lua_pushinteger(L, g_countdown);
	return 1;
}

static int l_setPlayerSide(lua_State* L)
{
	const char* side = luaL_checkstring(L, 1);
	g_playerSide = side ? side : "";
	return 0;
}

static int l_getPlayerSide(lua_State* L)
{
	lua_pushstring(L, g_playerSide.c_str());
	return 1;
}

static int l_setHomeTeam(lua_State* L)
{
	g_homeTeam = (int)luaL_checkinteger(L, 1);
	return 0;
}

static int l_getHomeTeam(lua_State* L)
{
	lua_pushinteger(L, g_homeTeam);
	return 1;
}

static int l_remapInput(lua_State* L)
{
	// int p1 = (int)luaL_checkinteger(L, 1);
	// int p2 = (int)luaL_checkinteger(L, 2);
	// Input remapping stored — no-op for now, full implementation later
	return 0;
}

// ── getTicks() — returns milliseconds since engine init (wraps SDL_GetTicks)
// Used by the screenpack for timing, frame-rate-independent animations, etc.

static int l_getTicks(lua_State* L)
{
	lua_pushnumber(L, (lua_Number)ikemen::getTicks());
	return 1;
}

// ── getTime() — returns seconds since Unix epoch (wraps time())
// Used by the screenpack for seeding random number generators and timestamps.

static int l_getTime(lua_State* L)
{
	(void)L;
	lua_pushnumber(L, (lua_Number)std::time(nullptr));
	return 1;
}

// ── sleep(ms) — suspends execution for the given number of milliseconds
// Uses ikemen::delay() which wraps SDL_Delay. The screenpack calls this
// for pause/delay effects (e.g. attract mode countdown, transition waits).

static int l_sleep(lua_State* L)
{
	uint32_t ms = (uint32_t)luaL_checkinteger(L, 1);
	ikemen::delay(ms);
	return 0;
}

// ── Lua C callbacks ─────────────────────────────────────────────────────

static int l_loadStart(lua_State* L)
{ LOG_INFO("ENGINE", "Lua -> loadStart() called"); lua_pushboolean(L, 1); return 1; }

static int l_selectStart(lua_State* L)
{ LOG_DEBUG("ENGINE", "Lua -> selectStart()"); lua_pushboolean(L, 1); return 1; }

static int l_game(lua_State* L)
{ LOG_DEBUG("ENGINE", "Lua -> game()"); int r = doGameLoop(); lua_pushnumber(L, r); return 1; }

static int l_sszReload(lua_State* L)
{ LOG_INFO("ENGINE", "Lua -> sszReload()"); sszReload(); return 0; }

static int l_getOS(lua_State* L)
{
#ifdef _WIN32
	lua_pushstring(L, "WIN32");
#else
	lua_pushstring(L, "UNIX");
#endif
	return 1;
}

static int l_getPlatform(lua_State* L)
{ lua_pushstring(L, "x86_64"); return 1; }

static int l_engineVersion(lua_State* L)
{ lua_pushstring(L, "2.0.0"); return 1; }

static int l_inputDialogNew(lua_State* L)
{ lua_pushnil(L); return 1; }

static int l_inputDialogSelect(lua_State* L)
{ lua_pushnil(L); return 1; }

// ── Text input buffer — accumulates characters for inputText()
static std::string g_inputBuf;

// Forward declaration for scancodeToAscii (defined after clipboard helpers)
static char scancodeToAscii(SDLKey sc, bool shift);

// ── Key state tracking ────────────────────────────────────────────────
// cmdInput() polls SDL events and tracks which keys are pressed/held/released
// each frame. Other callbacks (esc, f1Key, commandGetState, btnPalNo) read
// this tracked state to detect user input.

static bool g_keyHeld[512];      // currently held
static bool g_keyPressed[512];   // just pressed this frame (transition edge)
static bool g_keyReleased[512];  // just released this frame (transition edge)

// Map common M.U.G.E.N command names to SDL scancodes
// This approximates default key bindings so the screenpack works without
// a full command engine (commandNew / commandAdd / commandInput)
static SDLKey commandNameToKey(const char* name)
{
	if (strcmp(name, "u") == 0) return SDLKey::UP;
	if (strcmp(name, "d") == 0) return SDLKey::DOWN;
	if (strcmp(name, "l") == 0) return SDLKey::LEFT;
	if (strcmp(name, "r") == 0) return SDLKey::RIGHT;
	if (strcmp(name, "s") == 0) return SDLKey::SPACE;         // start
	if (strcmp(name, "e") == 0) return SDLKey::ESCAPE;
	if (strcmp(name, "a") == 0) return SDLKey::z;            // LP
	if (strcmp(name, "b") == 0) return SDLKey::x;            // LK
	if (strcmp(name, "c") == 0) return SDLKey::c;            // HP
	if (strcmp(name, "x") == 0) return SDLKey::a;            // HK
	if (strcmp(name, "y") == 0) return SDLKey::s;
	if (strcmp(name, "z") == 0) return SDLKey::d;
	if (strcmp(name, "q") == 0) return SDLKey::q;
	if (strcmp(name, "w") == 0) return SDLKey::w;
	return (SDLKey)-1;
}

// ── inputText(mode, allowDot) ───────────────────────────────────────-
// Blocking text input: polls SDL events, accumulates typed characters
// using getLastChar(), and returns when Enter is pressed.
// mode = "num" allows only digits, "" allows all printable chars.
// allowDot enables '.' in numeric mode.
// Called by the screenpack for IP address / hostname entry.

static int l_inputText(lua_State* L)
{
	const char* mode = luaL_optstring(L, 1, "");
	bool allowDot = lua_toboolean(L, 2) != 0;

	g_inputBuf.clear();

	bool done = false;
	while (!done) {
		ikemen::Event ev{};
		while (ikemen::pollEvent(ev)) {
			if (ev.etype == ikemen::EventType::QUIT) {
				g_running = false;
				done = true;
				break;
			}
			if (ev.etype == ikemen::EventType::KEYDOWN) {
				SDLKey sc = ev.key.ks.scancode;
				ikemen::K sym = ev.key.ks.sym;

				if (sym == ikemen::K::RETURN || sym == ikemen::K::KP_ENTER) {
					done = true;
					break;
				}
				if (sym == ikemen::K::BACKSPACE) {
					if (!g_inputBuf.empty())
						g_inputBuf.pop_back();
					continue;
				}

				// Map scancode to ASCII character (shift-aware)
				bool shift = ev.key.ks.mod & (KMOD_LSHIFT | KMOD_RSHIFT);
				char c = scancodeToAscii(sc, shift);
				if (c == '\0') continue;

				// Filter by mode
				bool ok = true;
				if (strcmp(mode, "num") == 0) {
					ok = (c >= '0' && c <= '9');
					if (allowDot && c == '.') ok = true;
				}
				if (ok) {
					g_inputBuf.push_back(c);
				}
			}
		}
		// Small delay to avoid busy-waiting
		ikemen::delay(16);
	}
	lua_pushstring(L, g_inputBuf.c_str());
	return 1;
}

// ── setInputText(str) — set the input buffer to a specific string
// Used by the screenpack when clipboard paste provides a pre-filled value.

static int l_setInputText(lua_State* L)
{
	const char* str = luaL_checkstring(L, 1);
	g_inputBuf = str ? str : "";
	return 0;
}

// ── clearInputText() — clears the input buffer

static int l_clearInputText(lua_State* L)
{
	(void)L;
	g_inputBuf.clear();
	return 0;
}

// ── clipboardPaste() — returns true if Ctrl+V was pressed this frame
// Checks our tracked key state for LCTRL/RCTRL + V combo.

static int l_clipboardPaste(lua_State* L)
{
	(void)L;
	bool ctrlHeld = g_keyHeld[(int)SDLKey::LCTRL] || g_keyHeld[(int)SDLKey::RCTRL];
	bool vPressed = g_keyPressed[(int)SDLKey::v];
	lua_pushboolean(L, (ctrlHeld && vPressed) ? 1 : 0);
	return 1;
}

// ── getClipboardText() — reads Unicode text from the Win32 clipboard

static int l_getClipboardText(lua_State* L)
{
	(void)L;
	std::string result;
	if (OpenClipboard(nullptr)) {
		HANDLE hData = GetClipboardData(CF_UNICODETEXT);
		if (hData) {
			const wchar_t* wstr = (const wchar_t*)GlobalLock(hData);
			if (wstr) {
				// Convert wide string to UTF-8
				int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1,
				                              nullptr, 0, nullptr, nullptr);
				if (len > 0) {
					result.resize(len - 1); // exclude null terminator
					WideCharToMultiByte(CP_UTF8, 0, wstr, -1,
					                    &result[0], len, nullptr, nullptr);
				}
				GlobalUnlock(hData);
			}
		}
		CloseClipboard();
	}
	lua_pushstring(L, result.c_str());
	return 1;
}

// ── scancodeToAscii(sc, shift) — maps SDL scancode to ASCII character
// Used by inputText() instead of getLastChar() to avoid the one-event lag
// between SDL_PollEvent updating g_lastChar and the character being readable.

static char scancodeToAscii(SDLKey sc, bool shift)
{
	// Letters a-z
	if (sc >= SDLKey::a && sc <= SDLKey::z) {
		int idx = (int)sc - (int)SDLKey::a;
		return shift ? (char)('A' + idx) : (char)('a' + idx);
	}
	// Digits 0-9 — scancodes are sequential _1=30 through _0=39
	if (sc >= SDLKey::_1 && sc <= SDLKey::_0) {
		int n = (int)sc - (int)SDLKey::_1 + 1;  // 1..10
		if (n == 10) n = 0;  // _0 wraps to 0
		return (char)('0' + n);
	}
	// Punctuation
	if (sc == SDLKey::SPACE)   return ' ';
	if (sc == SDLKey::MINUS)   return shift ? '_' : '-';
	if (sc == SDLKey::EQUALS)  return shift ? '+' : '=';
	if (sc == SDLKey::PERIOD)  return '.';
	if (sc == SDLKey::COMMA)   return ',';
	if (sc == SDLKey::SLASH)   return shift ? '?' : '/';
	if (sc == SDLKey::SEMICOLON) return shift ? ':' : ';';
	if (sc == SDLKey::APOSTROPHE) return '\'';
	if (sc == SDLKey::GRAVE)   return shift ? '~' : '`';
	if (sc == SDLKey::LEFTBRACKET)  return shift ? '{' : '[';
	if (sc == SDLKey::RIGHTBRACKET) return shift ? '}' : ']';
	if (sc == SDLKey::BACKSLASH)    return shift ? '|' : '\\';
	return '\0';
}

// ── cmdInput() ─────────────────────────────────────────────────────────
// Called every frame by the screenpack's while loops.
// Polls all pending SDL events and updates key state arrays.
// QUIT events set g_running = false (checked by refresh()).

static int l_cmdInput(lua_State* L)
{
	(void)L;
	// Clear transient edge flags (pressed/released)
	memset(g_keyPressed, 0, sizeof(g_keyPressed));
	memset(g_keyReleased, 0, sizeof(g_keyReleased));

	ikemen::Event ev{};
	while (ikemen::pollEvent(ev)) {
		if (ev.etype == ikemen::EventType::QUIT) {
			g_running = false;
			break;
		}
		if (ev.etype == ikemen::EventType::KEYDOWN) {
			int sc = (int)ev.key.ks.scancode;
			if (sc >= 0 && sc < 512) {
				if (!g_keyHeld[sc]) g_keyPressed[sc] = true;
				g_keyHeld[sc] = true;
			}
		}
		if (ev.etype == ikemen::EventType::KEYUP) {
			int sc = (int)ev.key.ks.scancode;
			if (sc >= 0 && sc < 512) {
				g_keyHeld[sc] = false;
				g_keyReleased[sc] = true;
			}
		}
	}
	return 0;
}

// ── Individual key checks ─────────────────────────────────────────────

static int l_esc(lua_State* L)
{
	(void)L;
	lua_pushboolean(L, g_keyPressed[(int)SDLKey::ESCAPE] ? 1 : 0);
	return 1;
}

static int l_f1Key(lua_State* L)
{
	(void)L;
	lua_pushboolean(L, g_keyPressed[(int)SDLKey::F1] ? 1 : 0);
	return 1;
}

static int l_returnKey(lua_State* L)
{
	(void)L;
	lua_pushboolean(L, g_keyPressed[(int)SDLKey::RETURN] ? 1 : 0);
	return 1;
}

// ── btnPalNo(cmd, noDefault) ──────────────────────────────────────────
// Returns the button palette number (or 1) if any attack/action button
// was pressed. Used by the screenpack to detect confirm/action input.

static int l_btnPalNo(lua_State* L)
{
	(void)L;
	// Check standard M.U.G.E.N attack buttons
	static const SDLKey attackKeys[] = {
		SDLKey::z, SDLKey::x, SDLKey::c, SDLKey::a, SDLKey::s, SDLKey::d,
		SDLKey::SPACE, SDLKey::RETURN,
	};
	for (auto sc : attackKeys) {
		if (g_keyPressed[(int)sc]) {
			lua_pushinteger(L, 1);
			return 1;
		}
	}
	lua_pushinteger(L, 0);
	return 1;
}

// ── commandBufReset(cmd) — resets the command input buffer
// Our simplified bridge doesn't track per-command history, so this is a
// no-op. The screenpack calls it to clear stale inputs before mode boots.

static int l_cmdBufReset(lua_State* L)
{
	(void)L;
	return 0;
}

// ── sszRandom() — returns a pseudo-random float in [0, 1)
// Uses C rand() seeded by the screenpack via math.randomseed(os.time()).

static int l_sszRandom(lua_State* L)
{
	(void)L;
	lua_pushnumber(L, (double)std::rand() / (double)RAND_MAX);
	return 1;
}

// ── commandGetState(cmd, name) ───────────────────────────────────────
// Approximates the command engine by mapping common command names
// ('u','d','l','r','s','e','a','b','c','x','y','z','holdu','holdd', etc.)
// directly to SDL scancodes. The first argument (cmd ref) is ignored.

static int l_cmdGetState(lua_State* L)
{
	// Arg 1 is the command ref (from commandNew) — ignored for simplified bridge
	// Arg 2 is the command name
	const char* name = luaL_checkstring(L, 2);

	bool isHold = false;
	const char* base = name;

	// Check for "hold" prefix (e.g. "holdu", "holdd", "holdl", "holdr")
	if (strncmp(name, "hold", 4) == 0) {
		isHold = true;
		base = name + 4;
	}

	SDLKey key = commandNameToKey(base);
	int sc = (int)key;

	if (sc >= 0 && sc < 512) {
		bool state = isHold ? g_keyHeld[sc] : g_keyPressed[sc];
		lua_pushboolean(L, state ? 1 : 0);
	} else {
		lua_pushboolean(L, 0);
	}
	return 1;
}

// ── Lua init ────────────────────────────────────────────────────────────

static bool initLua(LuaState& L)
{
	lua_State* ls = reinterpret_cast<lua_State*>(L.handle());

	// Game callbacks
	lua_pushcfunction(ls, l_loadStart);     lua_setglobal(ls, "loadStart");
	lua_pushcfunction(ls, l_selectStart);   lua_setglobal(ls, "selectStart");
	lua_pushcfunction(ls, l_game);          lua_setglobal(ls, "game");
	lua_pushcfunction(ls, l_sszReload);     lua_setglobal(ls, "sszReload");

	// System functions needed by script/common.lua
	lua_pushcfunction(ls, l_getOS);         lua_setglobal(ls, "getOS");
	lua_pushcfunction(ls, l_getPlatform);   lua_setglobal(ls, "getPlatform");
	lua_pushcfunction(ls, l_engineVersion); lua_setglobal(ls, "getEngineVersion");
	lua_pushcfunction(ls, l_inputDialogNew); lua_setglobal(ls, "inputDialogNew");
	lua_pushcfunction(ls, l_inputDialogSelect); lua_setglobal(ls, "inputDialogSelect");

	// Time/clock
	lua_pushcfunction(ls, l_getTicks);       lua_setglobal(ls, "getTicks");
	lua_pushcfunction(ls, l_getTime);        lua_setglobal(ls, "getTime");

	// Sleep/delay
	lua_pushcfunction(ls, l_sleep);          lua_setglobal(ls, "sleep");

	// Game config — backed by C state so getCredits() returns a real number
	lua_pushcfunction(ls, l_setGameMode);    lua_setglobal(ls, "setGameMode");
	lua_pushcfunction(ls, l_getGameMode);    lua_setglobal(ls, "getGameMode");
	lua_pushcfunction(ls, l_setCredits);     lua_setglobal(ls, "setCredits");
	lua_pushcfunction(ls, l_getCredits);     lua_setglobal(ls, "getCredits");
	lua_pushcfunction(ls, l_setRoundTime);   lua_setglobal(ls, "setRoundTime");
	lua_pushcfunction(ls, l_getRoundTime);   lua_setglobal(ls, "getRoundTime");
	lua_pushcfunction(ls, l_setRoundsToWin); lua_setglobal(ls, "setRoundsToWin");
	lua_pushcfunction(ls, l_getRoundsToWin); lua_setglobal(ls, "getRoundsToWin");
	lua_pushcfunction(ls, l_setCountdown);   lua_setglobal(ls, "setCountdown");
	lua_pushcfunction(ls, l_getCountdown);   lua_setglobal(ls, "getCountdown");
	lua_pushcfunction(ls, l_setPlayerSide);  lua_setglobal(ls, "setPlayerSide");
	lua_pushcfunction(ls, l_getPlayerSide);  lua_setglobal(ls, "getPlayerSide");
	lua_pushcfunction(ls, l_setHomeTeam);    lua_setglobal(ls, "setHomeTeam");
	lua_pushcfunction(ls, l_getHomeTeam);    lua_setglobal(ls, "getHomeTeam");
	lua_pushcfunction(ls, l_remapInput);     lua_setglobal(ls, "remapInput");

	// Input system — cmdInput polls events, other functions read key state
	// Text input — for hostname / IP / edit fields
	lua_pushcfunction(ls, l_inputText);       lua_setglobal(ls, "inputText");
	lua_pushcfunction(ls, l_setInputText);    lua_setglobal(ls, "setInputText");
	lua_pushcfunction(ls, l_clearInputText);  lua_setglobal(ls, "clearInputText");
	lua_pushcfunction(ls, l_clipboardPaste);  lua_setglobal(ls, "clipboardPaste");
	lua_pushcfunction(ls, l_getClipboardText);lua_setglobal(ls, "getClipboardText");

	lua_pushcfunction(ls, l_cmdInput);      lua_setglobal(ls, "cmdInput");
	lua_pushcfunction(ls, l_esc);           lua_setglobal(ls, "esc");
	lua_pushcfunction(ls, l_f1Key);         lua_setglobal(ls, "f1Key");
	lua_pushcfunction(ls, l_returnKey);      lua_setglobal(ls, "returnKey");
	lua_pushcfunction(ls, l_btnPalNo);      lua_setglobal(ls, "btnPalNo");
	lua_pushcfunction(ls, l_cmdBufReset);   lua_setglobal(ls, "commandBufReset");
	lua_pushcfunction(ls, l_sszRandom);      lua_setglobal(ls, "sszRandom");
	lua_pushcfunction(ls, l_cmdGetState);   lua_setglobal(ls, "commandGetState");

	// Sound
	lua_pushcfunction(ls, l_sndNew);         lua_setglobal(ls, "sndNew");
	lua_pushcfunction(ls, l_sndPlay);        lua_setglobal(ls, "sndPlay");
	lua_pushcfunction(ls, l_sndStop);        lua_setglobal(ls, "sndStop");
	lua_pushcfunction(ls, l_playBGM);        lua_setglobal(ls, "playBGM");
	lua_pushcfunction(ls, l_getBGM);         lua_setglobal(ls, "getBGM");
	lua_pushcfunction(ls, l_fadeInBGM);      lua_setglobal(ls, "fadeInBGM");
	lua_pushcfunction(ls, l_fadeOutBGM);     lua_setglobal(ls, "fadeOutBGM");
	lua_pushcfunction(ls, l_setVolume);       lua_setglobal(ls, "setVolume");
	lua_pushcfunction(ls, l_getVolume);       lua_setglobal(ls, "getVolume");

	// Font and text rendering
	lua_pushcfunction(ls, l_fontNew);        lua_setglobal(ls, "fontNew");
	lua_pushcfunction(ls, l_textImgNew);     lua_setglobal(ls, "textImgNew");
	lua_pushcfunction(ls, l_textImgDraw);    lua_setglobal(ls, "textImgDraw");
	lua_pushcfunction(ls, l_textImgSetFont); lua_setglobal(ls, "textImgSetFont");
	lua_pushcfunction(ls, l_textImgSetText); lua_setglobal(ls, "textImgSetText");
	lua_pushcfunction(ls, l_textImgSetPos);  lua_setglobal(ls, "textImgSetPos");
	lua_pushcfunction(ls, l_textImgAddPos);  lua_setglobal(ls, "textImgAddPos");
	lua_pushcfunction(ls, l_textImgSetBank); lua_setglobal(ls, "textImgSetBank");
	lua_pushcfunction(ls, l_textImgSetAlign);lua_setglobal(ls, "textImgSetAlign");
	lua_pushcfunction(ls, l_textImgSetScale);lua_setglobal(ls, "textImgSetScale");
	lua_pushcfunction(ls, l_textImgSetAlpha);lua_setglobal(ls, "textImgSetAlpha");
	lua_pushcfunction(ls, l_textImgGetWidth);lua_setglobal(ls, "textImgGetWidth");
	lua_pushcfunction(ls, l_drawString);      lua_setglobal(ls, "drawString");

	// Sprite rendering
	lua_pushcfunction(ls, l_sprNew);    lua_setglobal(ls, "sprNew");
	lua_pushcfunction(ls, l_sprDraw);   lua_setglobal(ls, "sprDraw");
	lua_pushcfunction(ls, l_sprUpdate); lua_setglobal(ls, "sprUpdate");

	// Animation control
	lua_pushcfunction(ls, l_animReset);      lua_setglobal(ls, "animReset");
	lua_pushcfunction(ls, l_animVelocity);   lua_setglobal(ls, "f_animVelocity");

	// Screenpack refresh callback (called each frame by Lua while-true loops)
	lua_pushcfunction(ls, l_refresh);         lua_setglobal(ls, "refresh");

	// SFF file and animation system
	lua_pushcfunction(ls, l_sffNew);          lua_setglobal(ls, "sffNew");
	lua_pushcfunction(ls, l_animNew);         lua_setglobal(ls, "animNew");
	lua_pushcfunction(ls, l_animAddPos);      lua_setglobal(ls, "animAddPos");
	lua_pushcfunction(ls, l_animSetPos);      lua_setglobal(ls, "animSetPos");
	lua_pushcfunction(ls, l_animSetAlpha);    lua_setglobal(ls, "animSetAlpha");
	lua_pushcfunction(ls, l_animSetColorKey); lua_setglobal(ls, "animSetColorKey");
	lua_pushcfunction(ls, l_animSetScale);    lua_setglobal(ls, "animSetScale");
	lua_pushcfunction(ls, l_animSetTile);     lua_setglobal(ls, "animSetTile");
	lua_pushcfunction(ls, l_animSetWindow);   lua_setglobal(ls, "animSetWindow");
	lua_pushcfunction(ls, l_animUpdate);      lua_setglobal(ls, "animUpdate");
	lua_pushcfunction(ls, l_animDraw);        lua_setglobal(ls, "animDraw");
	lua_pushcfunction(ls, l_animPosDraw);     lua_setglobal(ls, "animPosDraw");

	// System — character/stage selection and lifebar
	registerSystemScriptCallbacks(ls);

	// Auto-stub: undefined globals become stubs supporting all operations
	{
		const char* stubLua =
			"local function stub_ret0() return 0 end\n"
			"local function stub_retS() return '' end\n"
			"local mt = {}\n"
			"mt.__index = function(t, k)\n"
			"  if type(k) == 'string' and k:match('^[a-z]') then\n"
			"    local stub = setmetatable({}, {\n"
			"      __call     = stub_retS,\n"
			"      __tostring = stub_retS,\n"
			"      __concat   = function(a,b) return tostring(a)..tostring(b) end,\n"
			"      __add  = stub_ret0, __sub  = stub_ret0, __mul  = stub_ret0, __div  = stub_ret0,\n"
			"      __mod  = stub_ret0, __pow  = stub_ret0, __unm  = stub_ret0,\n"
			"      __eq   = function() return true end,\n"
			"      __lt   = function() return false end, __le = function() return true end,\n"
			"      __len  = stub_ret0,\n"
			"      __index = stub_retS,\n"
			"    })\n"
			"    rawset(_G, k, stub)\n"
			"    return stub\n"
			"  end\n"
			"end\n"
			"setmetatable(_G, mt)\n";
		int rc = luaL_dostring(ls, stubLua);
		LOG_DEBUG("ENGINE", "Auto-stub dostring returned %d", rc);
	}

	// Lua File System (lfs) — needed by loader.lua for dir listing
	lua_newtable(ls);
	lua_pushcfunction(ls, [](lua_State* L) -> int {
		lua_pushnil(L); return 1;
	}); lua_setfield(ls, -2, "attributes");
	lua_pushcfunction(ls, [](lua_State* L) -> int {
		lua_newtable(L); return 1;
	}); lua_setfield(ls, -2, "dir");
	lua_pushcfunction(ls, [](lua_State* L) -> int {
		lua_pushboolean(L, 1); return 1;
	}); lua_setfield(ls, -2, "mkdir");
	lua_pushcfunction(ls, [](lua_State* L) -> int {
		lua_pushboolean(L, 1); return 1;
	}); lua_setfield(ls, -2, "rmdir");
	lua_setglobal(ls, "lfs");

	// Sync default volume to the engine so ikemen::setVolume() matches
	// the Lua globals gl_vol, se_vol, bgm_vol set below.
	ikemen::setVolume(0.8f, 0.8f, 0.5f);

	// Common value globals (not functions — scripts use them as numbers/strings)
	auto setNum = [&](const char* name, double v) {
		lua_pushnumber(ls, v); lua_setglobal(ls, name);
	};
	auto setStr = [&](const char* name, const char* v) {
		lua_pushstring(ls, v); lua_setglobal(ls, name);
	};
	auto setBool = [&](const char* name, bool v) {
		lua_pushboolean(ls, v); lua_setglobal(ls, name);
	};

	setNum("gl_vol", 0.8);     setNum("se_vol", 0.8);     setNum("bgm_vol", 0.5);
	setNum("vid_vol", 100);     setNum("videoVol", 100);
	setNum("panStr",  0.8);
	setNum("gameSpeed", 60);    setNum("lifebarFontScale", 1.0);
	setNum("playerProjectileMax", 50);  setNum("helperMax", 56);
	setNum("explodMax", 256);   setNum("afterImageMax", 8);
	setNum("gameWidth", 640);   setNum("gameHeight", 480);
	setNum("roundTime", 999*6); setNum("roundsToWin", 2);
	setNum("matchWins", 2);     setNum("maxSimul", 2);
	setNum("numSimul", 2);      setNum("numTurns", 2);
	setNum("life", 1000);       setNum("power", 3000);
	setNum("attack", 100);      setNum("defence", 100);
	setNum("lifeMul", 1.0);     setNum("attackMul", 1.0);
	setNum("defenceMul", 1.0);
	setNum("timer", 99);        setNum("intro", 20);
	setNum("brightness", 256);
	setBool("saveMemory", false); setBool("fullScreen", false);
	setBool("aspectRatio", false); setBool("debugMode", false);
	setStr("motifName", "system");
	setStr("screenpackName", "system");
	setStr("osTarget", "WIN32");

	// motif — screenpack config table (filled by screenpack.lua)
	lua_newtable(ls);
	lua_setglobal(ls, "motif");

	// main — script entry table
	lua_newtable(ls);
	lua_setglobal(ls, "main");

	LOG_INFO("ENGINE", "Registered Lua globals: lfs, motif, main + auto-stub");
	return true;
}

// ── Main ────────────────────────────────────────────────────────────────

int ikemenMain()
{
	LOG_INFO("ENGINE", "=== I.K.E.M.E.N. Plus Ultra ===");

	if (!ikemen::init(ikemen::config::WindowTitle,
	                  ikemen::config::Width,
	                  ikemen::config::Height,
	                  ikemen::config::RenderMode, true))
	{
		LOG_INFO("ENGINE", "SDL init FAILED");
		return 1;
	}

	LOG_INFO("ENGINE", "Window: %dx%d, Renderer: %d",
		ikemen::config::Width, ikemen::config::Height, ikemen::config::RenderMode);

	if (ikemen::config::RenderMode != 0) {
		if (!ikemen::initMugenGl()) {
			LOG_INFO("ENGINE", "OpenGL init FAILED");
			ikemen::end();
			return 1;
		}
	}

	ikemen::fullScreenMode(ikemen::config::FullScreenExclusive);
	ikemen::keepAspectRatio(ikemen::config::AspectRatio);
	ikemen::setOpacity(ikemen::config::Opacity);
	ikemen::setWindowType(ikemen::config::WindowType);
	ikemen::showCursor(true);
	if (ikemen::config::FullScreen)
		ikemen::fullScreen(ikemen::config::FullScreen);

	memMarkBefore(L"EXIT");

	// ── Diagnostic: test pixel pipeline ───────────────────────────────
	// Fill screen with solid red and flip. If the window shows red,
	// g_pix is valid and the softFill→flip pipeline works.
	// If it stays black, the pixel pipeline is broken at a lower level.
	LOG_INFO("ENGINE", "DIAG: Testing pixel pipeline — filling with 0xFFFF0000 (red)");
	{
		ikemen::Rect fullRect = {0, 0, ikemen::config::Width, ikemen::config::Height};
		ikemen::softFill(fullRect, 0xFFFF0000);
		ikemen::flip();
	}

	// ── Boot Lua runtime ────────────────────────────────────────────
	LOG_INFO("ENGINE", "Booting Lua runtime...");
	LuaState L;

	if (!initLua(L)) {
		LOG_INFO("ENGINE", "Lua callback init failed");
		memMarkAfter(L"EXIT");
		ikemen::end();
		return 1;
	}

	const wchar_t* scriptPaths[] = {
		L"script/main.lua",
		L"test/script/main.lua",
	};

	for (auto& path : scriptPaths) {
		File f;
		if (f.open(path, L"rb")) {
			f.close();
			LOG_INFO("ENGINE", "Found: %ls", path);
			bool ok = L.runFile(path);
			LOG_INFO("ENGINE", "runFile returned: %s", ok ? "true" : "false");

			// Get Lua error message via C API
			if (!ok) {
				lua_State* ls = reinterpret_cast<lua_State*>(L.handle());
				const char* err = lua_tostring(ls, -1);
				if (err) LOG_INFO("ENGINE", "Lua error: %s", err);
				else     LOG_INFO("ENGINE", "Lua error: (no message)");
			}
			break;
		}
	}

	// ── Start Lua screenpack ────────────────────────────────────
	// Call f_mainStart() to enter the Lua-driven screenpack loop.
	// The screenpack handles input, animation updates, and rendering.
	// Each frame ends with refresh() which flips the screen.
	// ESC or QUIT causes refresh() to throw a Lua error, unrolling here.
	LOG_INFO("ENGINE", "Starting Lua screenpack via f_mainStart()...");
	{
		lua_State* ls = reinterpret_cast<lua_State*>(L.handle());
		lua_getglobal(ls, "f_mainStart");
		if (lua_isfunction(ls, -1)) {
			int result = lua_pcall(ls, 0, 0, 0);
			if (result != LUA_OK) {
				const char* err = lua_tostring(ls, -1);
				LOG_INFO("ENGINE", "Screenpack exited with error: %s", err ? err : "unknown");
				lua_pop(ls, 1);
			} else {
				LOG_INFO("ENGINE", "Screenpack f_mainStart() returned normally (no error)");
			}
		} else {
			LOG_INFO("ENGINE", "f_mainStart not found, falling back to game loop");
			lua_pop(ls, 1);
			doGameLoop();
		}
	}

	// ── Cleanup ─────────────────────────────────────────────────────
	memMarkAfter(L"EXIT");
	ikemen::end();

	LOG_INFO("ENGINE", "Shutdown complete");
	return 0;
}

} // namespace ikemen
