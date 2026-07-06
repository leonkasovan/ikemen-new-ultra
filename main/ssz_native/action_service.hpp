// action_service.hpp — Native C++ implementation for ssz_script/ssz/action.ssz
//
// action.ssz (207 lines) defines animation action frame types:
//   &Rect — axis-aligned rectangle (l, t, r, b)   [in sff_service.hpp]
//   &Frame — single animation frame with clsn boxes, sprite ref, timing
//   &Action — animation action with frame list and .air parser
//   &DrawnClsn — collision box rendered for debug display

#pragma once

#include <string>
#include <vector>

#include "sff_service.hpp"  // for Rect, FrameData, AnimData

namespace ikemen::ssz_native {

// =========================================================================
// ActionData — matches SSZ &Action in action.ssz
// =========================================================================
struct ActionData {
	int no{};              // Action number
	AnimData ani;          // Animation data (frames, timing, loop state)

	int numFrames() const { return static_cast<int>(ani.frames.size()); }
	void copy(const ActionData& a);

	// Parse action data from .air file lines.
	// lines: all lines from the .air file
	// i: index into lines (updated to end of parsed action section)
	// Returns the action number parsed, or 0 on failure.
	int read(const std::vector<std::string>& lines, int& i);
};

// =========================================================================
// DrawnClsnData — matches SSZ &DrawnClsn in action.ssz
// Used for rendering collision boxes in debug mode.
// =========================================================================
struct DrawnClsnData {
	const std::vector<Rect>* clsn{nullptr};
	float x{}, y{}, xscale{}, yscale{};

	// Set from camera-transformed coordinates.
	// cl: pointer to collision box array (from a FrameData)
	// x_, y_: world position
	// xs, ys: world scale
	void set(const std::vector<Rect>* cl, float x_, float y_, float xs, float ys);

	// Draw each collision box rectangle using the given sprite.
	// spr: sprite to use for rendering (may be null for debug-only)
	// alpha: transparency value
	void draw(const SpriteData* spr, int alpha);
};

// =========================================================================
// Free functions
// =========================================================================

// Module initialization (currently a no-op — state is in the structs).
void action_init();

} // namespace ikemen::ssz_native
