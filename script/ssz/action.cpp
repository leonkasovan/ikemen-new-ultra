#include "action.hpp"
#include "common.hpp"

#include "../string.hpp"

namespace ikemen {

// ── Frame ────────────────────────────────────────────────────────────────

CsnRect* Frame::clsn1() { return clsn.empty() ? nullptr : &clsn[0]; }
CsnRect* Frame::clsn2() { return clsn.size() > 1 ? &clsn[1] : nullptr; }

// ── Action ───────────────────────────────────────────────────────────────

Action::Action() { ani.mask = 0; }

void Action::copy(Action& a) { no = a.no; ani.copy(a.ani); }

void Action::read(const std::vector<std::wstring>& lines, size_t& i)
{
	ani.frames.clear();
	int ols = 0;

	while (i < lines.size()) {
		if (!lines[i].empty() && lines[i][0] == L'[') {
			if (i > 0) i--;
			break;
		}

		std::wstring line = ikemen::toLower(ikemen::trim(lines[i]));
		if (line.empty()) { i++; continue; }

		// Frame data line: starts with number or '-'
		if ((line[0] >= L'0' && line[0] <= L'9') || line[0] == L'-') {
			// Parse comma-separated ints using common::ctaOF
			// Simplified: just advance
		}
		// clsn definitions
		else if (line.compare(0, 4, L"clsn") == 0) {
			// collision box parsing — skip for now
		}
		// loopstart
		else if (line.compare(0, 9, L"loopstart") == 0) {
			ani.loopstart = static_cast<int>(ani.frames.size());
		}

		i++;
	}

	if (ani.loopstart >= static_cast<int>(ani.frames.size()))
		ani.loopstart = ols;
}

// ── DrawnClsn ────────────────────────────────────────────────────────────

void DrawnClsn::set(const std::vector<CsnRect>& cl, float x_, float y_, float xs, float ys)
{
	// clsn = cl; // simplified — copy reference
	x = x_;
	y = y_;
	xscale = xs;
	yscale = ys;
}

} // namespace ikemen
