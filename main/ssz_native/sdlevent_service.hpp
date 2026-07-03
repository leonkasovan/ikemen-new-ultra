// sdlevent_service.hpp — Native C++ scaffolding for ssz_script/lib/alpha/sdlevent.ssz
//
// sdlevent.ssz (599 lines) implements SDL event processing and key state
// tracking — polling events, tracking per-key state, fullscreen toggles,
// clipboard paste, and related engine event handling.

#pragma once

namespace ikemen::ssz_native {

struct SdlEventState {
	// Key state booleans (a-z, 0-9, F1-F12, modifiers, etc.)
	// Phase 2 deferred: populated when SDL event module is converted.
};

void sdlevent_poll();

} // namespace ikemen::ssz_native
