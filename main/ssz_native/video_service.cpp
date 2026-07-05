// video_service.cpp — Stub for video.ssz scaffolding (Phase 4).

#include "video_service.hpp"
#include "ssz_trace.hpp"

namespace ikemen::ssz_native {

void video_play(const std::string&, const std::string&, int, int) {
	SSZ_TRACE_CAT(TRACE_SYS, "video_play");
}
void video_play() {
	SSZ_TRACE_CAT(TRACE_SYS, "video_play (no-arg)");
	video_play("", "", 100, 1);
}

} // namespace ikemen::ssz_native
