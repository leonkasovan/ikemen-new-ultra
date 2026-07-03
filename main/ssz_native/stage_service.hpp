// stage_service.hpp — Native C++ scaffolding for ssz_script/ssz/stage.ssz
//
// stage.ssz (736 lines) implements stage data management — background layers,
// camera bounds, music, and stage loading.

#pragma once

#include <string>

namespace ikemen::ssz_native {

struct StageData {
	std::string def;
	std::string name;
	std::string music;
	// bg, camera bounds, etc. (opaque — wired when bg/com modules convert)
};

} // namespace ikemen::ssz_native
