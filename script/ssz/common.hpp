#pragma once

#include <cstdint>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "../table.hpp"
#include "../alpha/sdlevent.hpp"

namespace ikemen {

inline constexpr int IERR = std::numeric_limits<int32_t>::min();

// ── Team mode ─────────────────────────────────────────────────────────

enum class TeamMode { Single, Simul, Turns };

// ── Integer point ──────────────────────────────────────────────────────

struct IXY {
	int x = IERR, y = IERR;
	void set(int x_, int y_) { x = x_; y = y_; }
};

// ── Float point ────────────────────────────────────────────────────────

struct FXY {
	float x = NAN, y = NAN;
	void set(float x_, float y_) { x = x_; y = y_; }
};

// ── Sprite layout ──────────────────────────────────────────────────────

struct Layout {
	FXY     offset;
	int     displaytime = -2;
	int8_t  facing      = 1;
	int8_t  vfacing     = 1;
	int16_t layerno     = 0;
	FXY     scale;

	Layout();
	void read(const std::wstring& img, class Section& sc);
	void setup();
};

// ── Super-dangerous raw pointer ref (GC bypass) ────────────────────────

struct SuperDangerousRef {
	intptr_t p, pos, l;
};

// ── Key-value section parser ───────────────────────────────────────────

class Section {
public:
	void         parse(const std::vector<std::wstring>& lines, size_t& i);
	std::wstring get(const std::wstring& name);
	std::wstring getText(const std::wstring& name, std::wstring& text);
	std::wstring benri(const std::wstring& head, const std::wstring& name);

private:
	NameTable<std::wstring> params;
};

// ── Camera ─────────────────────────────────────────────────────────────

struct Camera {
	struct Stage {
		bool  own          = false;
		float x            = 0;
		float y            = 0;
		int   startx       = 0;
		float boundhigh    = 0;
		float boundlow     = 0;
		float boundleft    = 0;
		float boundright   = 0;
		float zoomin       = 1;
		float zoomout      = 1;
		float tension      = 0;
		float tensionhigh  = 0;
		float tensionlow   = 0;
		float startzoom    = 0;
		float overdrawhigh = 0;
		float overdrawlow  = 0;
		float overdrawleft = 0;
		float overdrawright= 0;
		float cutoffhigh   = 0;
		float cutofflow    = 0;
		float cutoffleft   = 0;
		float cutoffright  = 0;
		float zoomtime     = 0;
		float zoomspeed    = 1;
		bool  zoomanchor   = false;

		int   localw       = 320;
		int   localh       = 240;
		float localscl     = 1;
		float drawOffsetY  = 0;
		int   zoffset      = 0;
		float ztopscale    = 1;
		float verticalfollow = 0.2f;
		int   floortension = 0;

		void init();
		void update();
	};

	Stage stg;
};

extern Camera cam;

// ── Palette effects ────────────────────────────────────────────────────

struct PalFX {
	int   time = 0;
	float mul_r = 255, mul_g = 255, mul_b = 255;
	float add_r = 0,   add_g = 0,   add_b = 0;
	float sin_x_time = 0, sin_x_amplitude = 0, sin_x_frequency = 1, sin_x_phase = 0;
	float sin_y_time = 0, sin_y_amplitude = 0, sin_y_frequency = 1, sin_y_phase = 0;
	float sin_z_time = 0, sin_z_amplitude = 0, sin_z_frequency = 1, sin_z_phase = 0;

	void clear();
	void step();
	void getFxPal(float& rm, float& gm, float& bm,
	              float& ra, float& ga, float& ba) const;
};

// ── Number parsing ─────────────────────────────────────────────────────

double atof(const std::wstring& s);
int    atoi(const std::wstring& s);

// ── File I/O ───────────────────────────────────────────────────────────

std::wstring loadText(const std::wstring& fn, bool& unicode);
std::wstring readFileName(const std::wstring& f, bool unicode);
std::wstring loadFile(const std::wstring& fn, const std::wstring& ft);

// ── Section / def helpers ──────────────────────────────────────────────

std::wstring sectionName(std::wstring& sec);
void mugenversion(const std::wstring& path, float& version, int& compat,
                  int& mugenver, std::wstring& mugenname,
                  std::wstring& localcoord, bool& utf8, bool& filev2);

// ── Debug print ────────────────────────────────────────────────────────

void printVar(const std::wstring& name, int val);
void printVar(const std::wstring& name, float val);
void printVar(const std::wstring& name, const std::wstring& val);
void printArray(const std::wstring& name, const std::vector<int32_t>& arr);

// ── Input hashing ──────────────────────────────────────────────────────

int eventKeyHash(const SdlEventHandler::Key& k);

} // namespace ikemen
