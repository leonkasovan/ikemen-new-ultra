// action_service.hpp — Native C++ scaffolding for ssz_script/ssz/action.ssz
//
// action.ssz (207 lines) defines animation action frame types:
//   &Rect — axis-aligned rectangle (l, t, r, b)
//   &Frame — single animation frame with clsn boxes, sprite ref, timing
//   &Action — animation action with frame list
//   &DrawnClsn — collision box rendered for debug display

#pragma once

#include <vector>

namespace ikemen::ssz_native {

struct Rect { int l{}, t{}, r{-1}, b{-1}; };

struct Frame {
	std::vector<Rect> clsn;
	int time{-1};
	short group{-1}, number{};
	short x{}, y{};
	unsigned char salpha{255}, dalpha{};
	char h{1}, v{1};
};

struct ActionData {
	int no{};
	// ani: &.sff.Anim (opaque — wired when sff module converts)
};

struct DrawnClsnData {
	// cl, x, y, xs, ys — for debug collision rendering
};

// Phase 4: stub.
void action_init();

} // namespace ikemen::ssz_native
