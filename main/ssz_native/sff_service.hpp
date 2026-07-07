// sff_service.hpp — Native C++ implementation matching ssz_script/ssz/sff.ssz
//
// sff.ssz (1413 lines) implements SFF sprite/animation file loading and
// rendering — sprite groups, animation frames, palette management, and
// sprite rendering via sdlplugin.
//
// Phase 5: Real implementation for SFF v1/v2 parsing, sprite decoding
// (PCX/RLE8/RLE5/LZ5), palette management, animation system, and AIR file
// parsing. Hardware-accelerated rendering (glDraw) deferred until sdlplugin
// is converted.

#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>

#include "file_service.hpp"   // for FileHandle

namespace ikemen::ssz_native {

// =========================================================================
// Rect — Axis-aligned rectangle (from action.ssz)
// =========================================================================
struct Rect {
	int l{}, t{}, r{-1}, b{-1};
};

// =========================================================================
// Constants
// =========================================================================
inline constexpr int kNumCharPalettes = 12;
inline constexpr double kSffPi = 3.14159265358979323846;

// =========================================================================
// SffHeaderData — SFF file header (.sff files)
// =========================================================================
struct SffHeaderData {
	uint8_t ver0{}, ver1{}, ver2{}, ver3{};
	uint32_t firstSpriteHeaderOffset{};
	uint32_t firstPaletteHeaderOffset{};
	uint32_t numberOfPalettes{};
	uint32_t numberOfSprites{};

	/// Read header from an already-open file. Returns empty on success.
	std::string read(FileHandle& f, uint32_t& lofs, uint32_t& tofs);
};

// =========================================================================
// PaletteListData — Color palette management
// =========================================================================
struct PaletteListData {
	std::vector<std::vector<uint32_t>> palettes;  // %^uint palletes
	std::vector<int> palIdxs;                      // %index palidxs
	std::unordered_map<uint32_t, int> palTable;    // IntTable<uint, index?>

	void clear();
	std::vector<uint32_t>* newPal(int& pi);
	void setSource(int pi, const std::vector<uint32_t>& pal);
	std::vector<uint32_t>* get(int pi);
	void remap(int source, int dest);
	void resetRemap();
	std::vector<int> getPalMap() const;
	bool swapPalMap(std::vector<int>& map);
};

// =========================================================================
// SpriteData — A single sprite with pixel data
// =========================================================================
struct SDL_Rect; // forward from SDL headers

struct SpriteData {
	// Pixel data / texture (opaque handles)
	// colorPallet: shared palette array
	// pxl: pixel data (raw ubyte[] or GL texture)
	// pluginbuf: software renderer buffer
	std::vector<uint32_t> colorPallet;

	// Software pixel buffer — used when cfg.Renderer == 0
	std::vector<uint8_t> pxl;

	// Sprite rectangle (position + size)
	int16_t rct_x{}, rct_y{};
	int32_t rct_w{}, rct_h{};
	short imageGroup{}, imageNumber{};
	int palidx{-1};
	int rle{0};           // RLE encoding type (0=none, >0=PCX, <0=format)

	SpriteData() = default;
	SpriteData(const SpriteData&) = delete;
	SpriteData& operator=(const SpriteData&) = delete;
	SpriteData(SpriteData&&) = default;
	SpriteData& operator=(SpriteData&&) = default;

	void shareCopy(const SpriteData& sp);
	void copy(const SpriteData& sp);
	std::vector<uint32_t>* getPal(PaletteListData& pl);
	bool readHeader(FileHandle& f);
	std::string readPcxHeader(FileHandle& f, int64_t offset);
	bool readHeaderV2(FileHandle& f, uint32_t& ofs, uint32_t& siz,
		uint32_t lofs, uint32_t tofs, uint16_t& idxlnked);

	std::string read(FileHandle& f, SffHeaderData& sh, int64_t offset,
		uint32_t loh, uint32_t nsh, SpriteData* prev,
		PaletteListData& pl, bool c00);

	std::string readV2(FileHandle& f, int64_t ofs, uint32_t dsz);

	void setPxl(std::vector<uint8_t> px);

	// RLE decoders
	void rlePcxDecode(std::vector<uint8_t>& px);
	void rle8Decode(std::vector<uint8_t>& px);
	void rle5Decode(std::vector<uint8_t>& px);
	void lz5Decode(std::vector<uint8_t>& px);

	// Load a single sprite from an SFF file
	std::string loadFromSff(const std::string& fn, short ig, short in);
};

// =========================================================================
// FrameData — A single animation frame (action.ssz &Frame)
// =========================================================================
struct FrameData {
	// Collision boxes: clsn[0] = clsn1 (hit), clsn[1] = clsn2 (guard)
	// Each element is itself a vector of Rect (SSZ: ^^&.Rect)
	std::vector<std::vector<Rect>> clsn;
	short group{-1}, number{};
	short x{}, y{};
	int time{-1};
	std::vector<float> ex;   // extra params: x-scale, y-scale, angle
	char h{1}, v{1};
	uint8_t salpha{255}, dalpha{};
};

// =========================================================================
// AnimData — Animation state machine (sff.ssz &Anim)
// =========================================================================
struct SffData; // forward
struct PalFXData; // forward (defined in common_service.hpp)

struct AnimData {
	std::vector<FrameData> frames;
	int loopstart{0}, current{0}, drawidx{0};
	int time{0}, sumtime{0};
	int totaltime{0}, looptime{0}, nazotime{0};
	short mask{-1};
	short salpha{-1}, dalpha{0};
	int spal{-1}, dpal{0};
	bool newframe{true}, loopend{false};

	// References to owning SffData (for sprite lookup)
	SffData* sff{nullptr};
	SpriteData* spr{nullptr};

	void reset();
	void copy(const AnimData& a);
	void setFrames(const std::vector<FrameData>& f, int l);

	FrameData* currentFrame();
	FrameData* drawFrame();
	int animTime() const;
	int animElemTime(int elem) const;
	int animElemNo(int time) const;
	void setAnimElem(int e);
	void animSeek(int elem);
	void updateSprite();
	void action();
	int alphaFoo() const;

	void draw(int alpha, float x, float y, float xs, float ys,
		float xts, float xbs, float yss, float rxadd, float agl, int trans,
		const PalFXData* pal = nullptr);
};

// =========================================================================
// Sff — Main SFF file class
// =========================================================================
struct SffData {
	SffHeaderData head;
	std::unordered_map<uint32_t, SpriteData> spriteTable; // key = (group<<16)|number
	PaletteListData palList;

	void init();
	void clear();
	std::string loadFile(const std::string& filename, bool chr);
	SpriteData* getSprite(short group, short number);
	SpriteData* getOwnPalSprite(short group, short number);

	// Animation action management
	// actionList: vector<bg::Action>
	// actionTable: IntTable<int, bg::Action*>
};

// =========================================================================
// FrameMethods — Static helpers for frame operations
// =========================================================================
struct FrameMethods {
	static void action(AnimData& ani);
	static void readData(FrameData& frame, const std::vector<int>& ary,
		const std::string& line);
};

// =========================================================================
// Module-level API
// =========================================================================
void sff_init();

// Backward-compatible alias for existing test code.
using SffState = SffData;

} // namespace ikemen::ssz_native
