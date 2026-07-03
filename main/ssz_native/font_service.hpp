// font_service.hpp — Native C++ scaffolding for ssz_script/ssz/font.ssz
//
// font.ssz (409 lines) implements font rendering — text layout, sprite font
// rendering, and font face management via SDL_ttf.

#pragma once

#include <cstdint>
#include <string>

namespace ikemen::ssz_native {

struct FontData {
	// Placeholder — populated when font module is wired.
};

struct FontState {
	// Module-level state placeholder.
};

// Phase 4: stubs.
void font_init();
void font_render_text(const std::string& text, int x, int y, uint32_t color);

} // namespace ikemen::ssz_native
