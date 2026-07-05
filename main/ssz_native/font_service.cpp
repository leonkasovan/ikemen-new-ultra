// font_service.cpp — Stub for font.ssz scaffolding (Phase 4).

#include "font_service.hpp"
#include "ssz_trace.hpp"

#include <cstdint>

namespace ikemen::ssz_native {

void font_init() {
	SSZ_TRACE_CAT(TRACE_SYS, "font_init");
}
void font_render_text(const std::string&, int, int, uint32_t) {
	SSZ_TRACE_CAT(TRACE_SYS, "font_render_text");
}
void font_render_text() {
	SSZ_TRACE_CAT(TRACE_SYS, "font_render_text (no-arg)");
	font_render_text("", 0, 0, 0);
}

} // namespace ikemen::ssz_native
