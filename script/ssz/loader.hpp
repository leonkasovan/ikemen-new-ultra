#pragma once

#include <string>

namespace ikemen {

// ── Loader state ────────────────────────────────────────────────────────

enum class LoadState { NotYet, Loading, Complete, Error, Cancel };

class Loader {
public:
	LoadState state = LoadState::NotYet;
	std::wstring errorMes;

	void reset();
	bool runThread();

	// Needs game assets to test — requires chars/kfm/ and data/system.def
};

} // namespace ikemen
