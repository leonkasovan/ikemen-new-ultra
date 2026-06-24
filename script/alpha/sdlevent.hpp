#pragma once

#include "sdlplugin.hpp"

#include <cstdint>
#include <vector>

namespace ikemen {

class SdlEventHandler {
public:
	SdlEventHandler();

	bool eventUpdate();
	bool event(int fps);

	// ── State booleans ───────────────────────────────────────────────────
	bool end = false, fskip = false, full = false, fullReal = false, aspect = false;
	bool esc = false, paste = false, printscreenKey = false;
	bool nonushashKey = false, apostropheKey = false, graveKey = false;
	bool upKey = false, downKey = false, leftKey = false, rightKey = false;
	bool leftbracketKey = false, rightbracketKey = false, backslashKey = false;
	bool aKey=false, bKey=false, cKey=false, dKey=false, eKey=false, fKey=false, gKey=false, hKey=false;
	bool iKey=false, jKey=false, kKey=false, lKey=false, mKey=false, nKey=false, oKey=false, pKey=false;
	bool qKey=false, rKey=false, sKey=false, tKey=false, uKey=false, vKey=false, wKey=false, xKey=false, yKey=false, zKey=false;
	bool kzeroKey=false, koneKey=false, ktwoKey=false, kthreeKey=false, kfourKey=false, kfiveKey=false, ksixKey=false, ksevenKey=false, keightKey=false, knineKey=false;
	bool zeroKey=false, oneKey=false, twoKey=false, threeKey=false, fourKey=false, fiveKey=false, sixKey=false, sevenKey=false, eightKey=false, nineKey=false;
	bool returnKey=false, backspaceKey=false, spaceKey=false, lshiftKey=false, rshiftKey=false, tabKey=false;
	bool kdivideKey=false, kmultiplyKey=false, kminusKey=false, kplusKey=false, kenterKey=false, kperiodKey=false;
	bool minusKey=false, equalsKey=false, semicolonKey=false, commaKey=false, periodKey=false, slashKey=false;
	bool f1Key=false, f2Key=false, f3Key=false, f4Key=false, f5Key=false, f9Key=false, f10Key=false, f11Key=false, f12Key=false;
	bool insertKey=false, homeKey=false, pageupKey=false, deleteKey=false, end_Key=false, pagedownKey=false;
	bool getGamepadKeyA=false, getGamepadKeyB=false, getGamepadKeyC=false;

	// ── Key bindings ─────────────────────────────────────────────────────
	struct Key {
		K      key   = K::UNKNOWN;
		bool   shift = false, ctrl = false, alt = false;
		bool   down  = false;
		void   reset() { down = false; }
		void   checkDown(K k, uint16_t m);
	};

	std::vector<Key> eventKeys;

private:
	void resetKeys();
	void handleKeyDown();

	Event  m_event{};
	uint32_t m_nextTime = 0, m_lastDraw = 0;
	float  m_nextTimeFrac = 0.0f;
};

} // namespace ikemen
