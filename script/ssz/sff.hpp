#pragma once

#include "../alpha/sdlplugin.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace ikemen {

struct File; // forward

constexpr int NUM_CHAR_PALETTES = 12;

// ── SFF Header ──────────────────────────────────────────────────────────

struct SffHeader {
	uint8_t  ver0 = 0, ver1 = 0, ver2 = 0, ver3 = 0;
	uint32_t firstSpriteHeaderOffset = 0, firstPaletteHeaderOffset = 0;
	uint32_t numberOfPalettes = 0, numberOfSprites = 0;

	std::wstring read(File& f, uint32_t& lofs, uint32_t& tofs);
};

// ── Palette list ────────────────────────────────────────────────────────

class PalleteList {
public:
	void           clear();
	std::vector<uint32_t>& newPal(uint32_t& pi);
	void           setSource(int pi, const std::vector<uint32_t>& pal);
	std::vector<uint32_t>& get(int pi);
	void           remap(int source, int dest);
	void           resetRemap();
	bool           swapPalMap(std::vector<int>& map);

private:
	std::vector<std::vector<uint32_t>> m_palettes;
	std::vector<int>             m_palidxs;
};

// ── Sprite ───────────────────────────────────────────────────────────────

struct Sprite {
	std::vector<uint32_t> colorPallet;
	std::vector<uint8_t>  pxl;
	std::vector<int8_t>   pluginbuf;
	Rect         rct{};
	int16_t      imageGroup = 0, imageNumber = 0;
	int32_t      palidx = -1;
	int          rle = 0;
	uint8_t      coldepth = 0;

	void         shareCopy(Sprite& sp);
	void         copy(Sprite& sp);
	std::vector<uint32_t>& getPal(PalleteList& pl);

	bool         readHeader(File& f);
	std::wstring readPcxHeader(File& f, int64_t offset);
	bool         readHeaderV2(File& f, uint32_t& ofs, uint32_t& siz,
	                          uint32_t lofs, uint32_t tofs, uint16_t& idxlnked);
	std::wstring read(File& f, SffHeader& sh, int64_t offset, uint32_t loh,
	                  uint32_t nsh, Sprite* prev, PalleteList& pl, bool c00);
	std::wstring readV2(File& f, int64_t ofs, uint32_t dsz);
	void         setPxl(std::vector<uint8_t>& px);
	void         rlePcxDecode(std::vector<uint8_t>& px);
	void         rle8Decode(std::vector<uint8_t>& px);
	void         rle5Decode(std::vector<uint8_t>& px);
	void         lz5Decode(std::vector<uint8_t>& px);
};

// ── SFF File Manager ────────────────────────────────────────────────────

struct SffFile {
	std::string filename;
	std::map<uint64_t, Sprite> sprites;
	bool loadAll(const std::wstring& path);
	Sprite* getSprite(int grp, int num);
};

} // namespace ikemen

// ── FrameMethods (template) ─────────────────────────────────────────────

namespace ikemen {

template<typename FrameT> struct Anim;

template<typename FrameT>
struct FrameMethods {
	void action(Anim<FrameT>& ani) {}
	void readData(std::vector<int>& ary, const std::wstring& line) {}
};

// ── Anim ────────────────────────────────────────────────────────────────

template<typename FrameT>
struct Anim {
	std::vector<FrameT> frames;
	int                loopstart = 0, current = 0, drawidx = 0;
	int                time = 0, sumtime = 0, totaltime = 0, looptime = 0;
	int16_t            mask = -1, salpha = -1, dalpha = 0;
	int                spal = -1, dpal = 0;
	bool               newframe = true, loopend = false;

	FrameT currentFrame() { return frames.empty() ? FrameT{} : frames[static_cast<size_t>(current)]; }
	FrameT drawFrame()   { return frames.empty() ? FrameT{} : frames[static_cast<size_t>(drawidx)]; }
	int    animTime()    { return sumtime - totaltime; }
	void   reset()       { current=drawidx=0; time=sumtime=0; newframe=true; loopend=false; }
	void   copy(Anim& a) { frames=a.frames; loopstart=a.loopstart; current=a.current; time=a.time; sumtime=a.sumtime; totaltime=a.totaltime; looptime=a.looptime; mask=a.mask; salpha=a.salpha; dalpha=a.dalpha; spal=a.spal; dpal=a.dpal; newframe=a.newframe; loopend=a.loopend; }
};

} // namespace ikemen
