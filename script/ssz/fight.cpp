#include "fight.hpp"
#include "common.hpp"

namespace ikemen {

void WinInfo::reset()
{
	winType = 0; winTime = 0;
	winPerfect = winSpecial = winHyper = winThrow = false;
}

void RoundInfo::reset()
{
	active = 0; order = 0;
	drawgame = false; ko = false; koDraw = false; over = false;
}

void ComboInfo::reset()
{
	counter = 0; damage = 0; hits = 0; display = false;
}

void TimerInfo::reset()
{
	time = 0; countdown = -1; display = false;
}

} // namespace ikemen
