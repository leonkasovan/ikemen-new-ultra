// font_service.hpp — Native C++ implementation for ssz_script/ssz/font.ssz
//
// font.ssz (409 lines) implements font rendering:
//   &Font — font face with character map, atlas, and draw functions
//   FNT v1 — ElecbyteFnt binary format (PCX atlas + regex-based character map)
//   FNT v2 — SFF-based sprite font from .def-style config files
//   drawChar / drawText — per-character and batch text rendering
//   renderFontBatch — atlas-based batch rendering for software renderer

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

#include "sff_service.hpp"   // for SpriteData, Rect
#include "sdlplugin_service.hpp"   // for SdlRect

namespace ikemen::ssz_native {

// =========================================================================
// CharOffset — Per-character info (SSZ &CharOffset in font.ssz)
// =========================================================================
struct CharOffset {
	int ofs{};                         // Pixel offset in the font atlas
	int w{};                           // Width of this character in pixels
	std::vector<SpriteData> img;       // Per-palette-bank sprite copies
};

// =========================================================================
// FontData — Matches SSZ &Font in font.ssz
// =========================================================================
struct FontData {
	// ── Core font atlas ──
	SpriteData img;                          // Base font atlas sprite (PCX loaded)
	std::vector<CharOffset> mpl;             // Character offset list
	std::unordered_map<char, int> map;       // char -> index in mpl

	// ── Font metadata ──
	uint16_t ver{}, ver2{};                  // FNT version fields
	std::string fntType;                     // Font type string (FNT v2)
	int sizex{6}, sizey{8};                  // Default character cell size
	int spacingx{}, spacingy{};              // Inter-character spacing
	int colors{255};                         // Number of palette colors
	int offsetx{}, offsety{};                // Drawing offset
	std::string fntSff;                      // SFF filename (FNT v2)

	// ── Font atlas data (for software renderer batch drawing) ──
	std::vector<uint8_t> fontAtlas;          // Raw atlas pixel data (decoded PCX)
	int fontAtlasStride{};                   // Atlas row stride in bytes

	// ── Font file loading ──
	// Dispatches to loadFontV1 (.fnt) or loadFontV2 (other).
	// Returns empty string on success, error message on failure.
	std::string loadFile(const std::string& filename);

	// FNT v1 binary format loader (ElecbyteFnt).
	// Parses PCX header, reads RLE-encoded pixel data, palette,
	// text section with regex-based character map, and creates
	// per-character sprites with palette banks.
	std::string loadFontV1(const std::string& filename);

	// FNT v2 text-based loader.
	// Reads .def-style config, loads SFF sprite font file.
	std::string loadFontV2(const std::string& filename);

	// ── Character metrics ──
	int charWidth(char ch) const;
	int textWidth(const std::string& txt) const;

	// ── Character sprite access ──
	// Returns a pointer to the sprite for character ch at palette bank.
	// Returns nullptr if the character is not mapped.
	const SpriteData* getCharSpr(int bank, char ch) const;

	// ── Drawing ──
	// Draw a single character at the given position and scale.
	// Returns the width of the drawn character in screen pixels.
	float drawChar(float x, float y, float xscl, float yscl,
		int bank, int salpha, int dalpha, const SdlRect& window,
		char ch, std::vector<uint32_t>& pal);

	// Draw a text string with alignment.
	void drawText(float x, float y, float xscl, float yscl,
		int bank, int salpha, int dalpha, const SdlRect& window,
		int align, const std::string& txt);
};

// =========================================================================
// FontState — Module-level state
// =========================================================================
struct FontState {
	// The debug font (SSZ public ^&Font debugFont)
	// This is a static font that can be used for debug rendering.
	FontData* debugFont{nullptr};
};

// =========================================================================
// Module-level API (called from bridge.cpp)
// =========================================================================

// Initialize the font module.
void font_init();

// Get the module-level font state.
FontState& font_get_state();

// Render text at the given coordinates using the debug font.
// (No-arg overload for bridge compatibility.)
void font_render_text(const std::string& text, int x, int y, uint32_t color);
void font_render_text();

} // namespace ikemen::ssz_native
