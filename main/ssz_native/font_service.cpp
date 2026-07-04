// font_service.cpp — Stub for font.ssz scaffolding (Phase 4).

#include "font_service.hpp"

#include <cstdint>

namespace ikemen::ssz_native {

void font_init() {}
void font_render_text(const std::string&, int, int, uint32_t) {}
void font_render_text() { font_render_text("", 0, 0, 0); }

} // namespace ikemen::ssz_native
