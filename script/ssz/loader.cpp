#include "loader.hpp"
#include "common.hpp"

namespace ikemen {

void Loader::reset()
{
	state = LoadState::Cancel;
	// <wait>(loadThread) — simplified
	state = LoadState::NotYet;
	errorMes.clear();
}

bool Loader::runThread()
{
	if (state != LoadState::NotYet) return false;
	state = LoadState::Loading;
	// loadThread.run() — simplified
	return true;
}

} // namespace ikemen
