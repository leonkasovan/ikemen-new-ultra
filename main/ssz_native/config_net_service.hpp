// config_net_service.hpp — Native C++ equivalent of ssz_script/save/configNet.ssz
//
// configNet.ssz (179 lines) defines netplay engine configuration constants.
// Reuses ConfigData from config_service.hpp with net-specific defaults.

#pragma once

#include "config_service.hpp"

namespace ikemen::ssz_native {

inline ConfigData make_default_config_net() {
	ConfigData c = make_default_config();
	c.Renderer = 0;
	c.Width = 1280;
	c.Height = 800;
	c.CharWinnerPortraitIndex = 2;
	c.CharLoserPortraitIndex = 3;
	c.CharVSPortraitIndex = 5;
	c.CharResultsPortraitIndex = 6;
	return c;
}

} // namespace ikemen::ssz_native
