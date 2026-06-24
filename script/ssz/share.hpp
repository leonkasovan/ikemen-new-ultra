#pragma once
#include "char.hpp"

namespace ikemen {
struct Share {
	int stt=0; int matchNo=0; int roundNo=0; int p1Wins=0, p2Wins=0;
	WinInfo wi[2];
	void copy() {}
	void push() {}
};
} // namespace ikemen
