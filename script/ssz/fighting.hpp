#pragma once
#include "char.hpp"
#include <vector>
namespace ikemen {
struct Fighting {
	struct Fight { WinInfo wi[2]; void clear() { wi[0].reset(); wi[1].reset(); } };
	struct FaceInfo { int face_sprg=9000, face_spri=1; int numko=0; std::vector<Sprite*> teammate_face_spr; };
	struct Fighters { FaceInfo fa[2][4]; };
	Fight fight;
	Fighters* fa = nullptr;
	void main();
};
} // namespace ikemen
