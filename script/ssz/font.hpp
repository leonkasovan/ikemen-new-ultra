#pragma once

#include "sff.hpp"
#include "../alpha/sdlplugin.hpp"
#include "../table.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace ikemen {

// ── Font loader ─────────────────────────────────────────────────────────

class GameFont {
public:
	struct CharOffset {
		int ofs = 0;
		int w = 0;
		Sprite img;
	};

	Sprite img;
	std::vector<CharOffset> mpl;
	IntTable<char16_t, CharOffset> map;
	uint16_t ver = 0, ver2 = 0;
	std::wstring fntType;
	int sizex = 6, sizey = 8;
	int spacingx = 0, spacingy = 0;
	int colors = 255;
	int offsetx = 0, offsety = 0;
	std::wstring fntSff;
	std::vector<uint8_t> fontAtlas;
	int fontAtlasStride = 0;

	std::wstring loadFile(const std::wstring& filename);
	std::wstring loadFontV1(const std::wstring& filename);
	std::wstring loadFontV2(const std::wstring& filename);

	// Text rendering
	void drawText(float x, float y, float xscl, float yscl, int bank,
	              int salpha, int dalpha, const Rect& window, int align,
	              const std::string& text);
	float textWidth(const std::string& text) const;
};

} // namespace ikemen
