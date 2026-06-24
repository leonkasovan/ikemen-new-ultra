#pragma once
// ---------------------------------------------------------------------------
// renderer.h  –  Multi-backend rendering abstraction for I.K.E.M.E.N. Plus Ultra
// ---------------------------------------------------------------------------
// Supported backends:
//   SDL2       – CPU-based software rendering (SDL2 Renderer + streaming texture)
//   OpenGL     – GPU accelerated: auto-detects 4.6 > 3.3 > 2.1
//   OpenGL ES  – GPU accelerated: auto-detects 3.2 > 3.1 > 3.0  (ARM Linux)
//   Vulkan     – GPU accelerated (stub – future implementation)
// ---------------------------------------------------------------------------

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <windows.h>

// ---------------------------------------------------------------------------
// Renderer type enumeration
// ---------------------------------------------------------------------------
enum RendererType
{
	RENDERER_UNKNOWN  = 0,
	RENDERER_SDL2     = 1,
	RENDERER_OPENGL   = 2,
	RENDERER_OPENGLES = 3,
	RENDERER_VULKAN   = 4
};

// ---------------------------------------------------------------------------
// OpenGL version detected
// ---------------------------------------------------------------------------
enum GLVersion
{
	GL_VER_NONE  = 0,
	GL_VER_2_1   = 21,
	GL_VER_3_3   = 33,
	GL_VER_4_6   = 46
};

enum GLESVersion
{
	GLES_VER_NONE = 0,
	GLES_VER_3_0  = 30,
	GLES_VER_3_1  = 31,
	GLES_VER_3_2  = 32
};

enum SpritePathFlags
{
	SPRITE_PATH_NONE          = 0,
	SPRITE_PATH_RLE           = 1 << 0,
	SPRITE_PATH_ROTATED       = 1 << 1,
	SPRITE_PATH_TILED         = 1 << 2,
	SPRITE_PATH_TRAPEZOID     = 1 << 3,
	SPRITE_PATH_SIMPLE_RECT   = 1 << 4,
	SPRITE_PATH_FAST_ADD_RECT = 1 << 5
};

// ---------------------------------------------------------------------------
// Performance counters  –  reset each frame, accumulated during rendering
// ---------------------------------------------------------------------------
struct PerfCounters
{
	// Per-frame stats (reset at Flip/SwapBuffers)
	uint32_t drawCalls;
	uint32_t spriteCount;
	uint32_t fillCalls;
	uint32_t textureBinds;
	uint32_t shaderSwitches;
	uint32_t textCacheHits;
	uint32_t textCacheMisses;
	uint32_t blitCacheHits;
	uint32_t blitCacheMisses;
	uint32_t shadowCount;
	uint32_t fastAddRectBlits;
	uint32_t addAlphaSimpleSprites;
	uint64_t totalPixelArea;    // sum of dest w*h across all RenderMugenZoom calls

	// Worst sprite info (the single slowest RenderMugenZoom call)
	double   worstSpriteUs;
	int      worstSpriteSrcW;
	int      worstSpriteSrcH;
	int      worstSpriteW;
	int      worstSpriteH;
	int32_t  worstSpriteAlpha; // raw alpha param
	uint32_t worstSpritePathFlags;

	// High-resolution timing (microseconds via QueryPerformanceCounter)
	LARGE_INTEGER qpcFreq;         // counts per second
	LARGE_INTEGER qpcFrameStart;   // frame start timestamp
	double spriteTimeUs;           // total RenderMugenZoom time (us)
	double shadowTimeUs;           // total RenderMugenShadow time (us)
	double fillTimeUs;             // total SoftFill time (us)
	double flipTimeUs;             // Flip (unlock+copy+present) time (us)

	// Timing
	uint32_t frameStartTick;    // SDL_GetTicks at frame start
	float    lastFrameTimeMs;   // milliseconds for last completed frame (QPC-based)
	float    fps;               // smoothed FPS
	float    fpsAccum;          // accumulator for FPS averaging
	uint32_t fpsFrameCount;     // frames counted in current second
	uint32_t fpsLastTick;       // last tick when FPS was computed

	// Lifetime totals
	uint64_t totalFrames;
	uint64_t totalDrawCalls;

	void reset()
	{
		drawCalls = 0;
		spriteCount = 0;
		fillCalls = 0;
		textureBinds = 0;
		shaderSwitches = 0;
		textCacheHits = 0;
		textCacheMisses = 0;
		blitCacheHits = 0;
		blitCacheMisses = 0;
		shadowCount = 0;
		fastAddRectBlits = 0;
		addAlphaSimpleSprites = 0;
		totalPixelArea = 0;
		worstSpriteUs = 0.0;
		worstSpriteSrcW = 0;
		worstSpriteSrcH = 0;
		worstSpriteW = 0;
		worstSpriteH = 0;
		worstSpriteAlpha = 0;
		worstSpritePathFlags = 0;
		spriteTimeUs = 0.0;
		shadowTimeUs = 0.0;
		fillTimeUs = 0.0;
		flipTimeUs = 0.0;
	}

	void init()
	{
		memset(this, 0, sizeof(*this));
		QueryPerformanceFrequency(&qpcFreq);
	}
};

// ---------------------------------------------------------------------------
// Renderer info  –  filled once during initialisation, read-only afterwards
// ---------------------------------------------------------------------------
struct RendererInfo
{
	RendererType type;
	GLVersion    glVersion;
	GLESVersion  glesVersion;
	char         backendName[64];   // e.g. "OpenGL 4.6"
	char         deviceName[128];   // GPU name
	char         driverInfo[128];   // driver version
	int          maxTextureSize;

	void clear()
	{
		memset(this, 0, sizeof(*this));
	}
};

// ---------------------------------------------------------------------------
// Logging helpers — LOG_INFO/LOG_DEBUG defined in sszdef.h
// Shorthand convenience macros (use these instead of raw LOG_INFO)
// ---------------------------------------------------------------------------
#define REND_LOG(fmt, ...)  LOG_INFO("Renderer", fmt, ##__VA_ARGS__)
#define PERF_LOG(fmt, ...)  LOG_INFO("Perf", fmt, ##__VA_ARGS__)
#define INIT_LOG(fmt, ...)  LOG_INFO("Init", fmt, ##__VA_ARGS__)
#define ASSET_LOG(fmt, ...) LOG_INFO("Asset", fmt, ##__VA_ARGS__)
#define DBG_LOG(fmt, ...)   LOG_DEBUG("Debug", fmt, ##__VA_ARGS__)

// ---------------------------------------------------------------------------
// Global state accessors  (defined in sdlplugin.cpp)
// ---------------------------------------------------------------------------
extern RendererType  g_rendererType;
extern GLVersion     g_glVersion;
extern GLESVersion   g_glesVersion;
extern RendererInfo  g_rendererInfo;
extern PerfCounters  g_perfCounters;
extern bool          g_perfMonitorEnabled;

// ---------------------------------------------------------------------------
// Parse renderer name from config string
// ---------------------------------------------------------------------------
inline RendererType parseRendererType(const char* name)
{
	if (!name) return RENDERER_SDL2;
	// Case-insensitive comparison
	if (_stricmp(name, "SDL2") == 0 || _stricmp(name, "Software") == 0)
		return RENDERER_SDL2;
	if (_stricmp(name, "OpenGL") == 0 || _stricmp(name, "GL") == 0)
		return RENDERER_OPENGL;
	if (_stricmp(name, "OpenGL ES") == 0 || _stricmp(name, "GLES") == 0 || _stricmp(name, "OpenGLES") == 0)
		return RENDERER_OPENGLES;
	if (_stricmp(name, "Vulkan") == 0 || _stricmp(name, "VK") == 0)
		return RENDERER_VULKAN;
	return RENDERER_SDL2; // default fallback
}

inline const char* rendererTypeName(RendererType t)
{
	switch (t) {
	case RENDERER_SDL2:     return "SDL2";
	case RENDERER_OPENGL:   return "OpenGL";
	case RENDERER_OPENGLES: return "OpenGL ES";
	case RENDERER_VULKAN:   return "Vulkan";
	default:                return "Unknown";
	}
}

inline const char* glVersionName(GLVersion v)
{
	switch (v) {
	case GL_VER_2_1: return "2.1";
	case GL_VER_3_3: return "3.3";
	case GL_VER_4_6: return "4.6";
	default:         return "N/A";
	}
}

inline const char* glesVersionName(GLESVersion v)
{
	switch (v) {
	case GLES_VER_3_0: return "3.0";
	case GLES_VER_3_1: return "3.1";
	case GLES_VER_3_2: return "3.2";
	default:           return "N/A";
	}
}

// ---------------------------------------------------------------------------
// High-resolution timer helper
// ---------------------------------------------------------------------------
inline double qpcElapsedUs(const LARGE_INTEGER& start, const LARGE_INTEGER& end, const LARGE_INTEGER& freq)
{
	return (double)(end.QuadPart - start.QuadPart) * 1000000.0 / (double)freq.QuadPart;
}

// ---------------------------------------------------------------------------
// Perf monitoring: call at frame boundaries
// ---------------------------------------------------------------------------
inline void perfFrameBegin(PerfCounters& pc)
{
	pc.reset();
	QueryPerformanceCounter(&pc.qpcFrameStart);
	// frameStartTick set by caller using SDL_GetTicks()
}

inline void perfFrameEnd(PerfCounters& pc, uint32_t currentTick)
{
	// Use QPC for sub-millisecond accuracy; SDL_GetTicks only for FPS calculation
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);
	pc.lastFrameTimeMs = (float)(qpcElapsedUs(pc.qpcFrameStart, now, pc.qpcFreq) / 1000.0);
	pc.totalFrames++;
	pc.totalDrawCalls += pc.drawCalls;
	pc.fpsFrameCount++;

	// Update FPS every second using SDL_GetTicks (low precision but sufficient)
	uint32_t elapsed = currentTick - pc.fpsLastTick;
	if (elapsed >= 1000)
	{
		pc.fps = (float)pc.fpsFrameCount * 1000.0f / (float)elapsed;
		pc.fpsFrameCount = 0;
		pc.fpsAccum = 0.0f;
		pc.fpsLastTick = currentTick;
	}
}

inline void perfPrintFrame(const PerfCounters& pc, const RendererInfo& ri)
{
	// Only print every ~60 frames (approximately once per second at 60fps)
	if (pc.totalFrames % 60 != 0) return;

	double spriteAvgUs = pc.spriteCount > 0 ? pc.spriteTimeUs / pc.spriteCount : 0.0;
	double totalRenderUs = pc.spriteTimeUs + pc.shadowTimeUs + pc.fillTimeUs;
	double accountedMs = (totalRenderUs + pc.flipTimeUs) / 1000.0;
	double scriptMs = (double)pc.lastFrameTimeMs - accountedMs;
	if (scriptMs < 0.0) scriptMs = 0.0;
	uint32_t mpix = (uint32_t)(pc.totalPixelArea / 1000);

	// Determine blend mode label from raw alpha parameter
	const char* blendLabel = "opaque";
	if (pc.worstSpriteAlpha == -1)       blendLabel = "add";
	else if (pc.worstSpriteAlpha == -2)  blendLabel = "sub";
	else if (pc.worstSpriteAlpha < 255 && pc.worstSpriteAlpha > 0) blendLabel = "alpha";
	else if (pc.worstSpriteAlpha >= 512) blendLabel = "add+alpha";

	char pathLabel[64];
	_snprintf(pathLabel, sizeof(pathLabel), "%s%s%s%s%s%s",
		(pc.worstSpritePathFlags & SPRITE_PATH_RLE) ? "rle" : "pal",
		(pc.worstSpritePathFlags & SPRITE_PATH_ROTATED) ? "/rot" : "",
		(pc.worstSpritePathFlags & SPRITE_PATH_TILED) ? "/tile" : "",
		(pc.worstSpritePathFlags & SPRITE_PATH_TRAPEZOID) ? "/trap" : "",
		(pc.worstSpritePathFlags & SPRITE_PATH_SIMPLE_RECT) ? "/simple" : "",
		(pc.worstSpritePathFlags & SPRITE_PATH_FAST_ADD_RECT) ? "/fastadd" : "");
	pathLabel[sizeof(pathLabel) - 1] = '\0';

	PERF_LOG("FPS: %.1f | Frame: %.2fms [render:%.2f script:%.2f flip:%.2f] | "
		"Sprites: %u (%.2fms, avg %.0fus) | Worst: %.0fus %dx%d<-%dx%d %s %s | "
		"Shadows: %u (%.2fms) | Fills: %u | "
		"Pixels: %uK | AddRect: %u | AAlphaSimple: %u | TxtC: %u/%u | BlitC: %u/%u | %s",
		pc.fps, pc.lastFrameTimeMs,
		totalRenderUs / 1000.0, scriptMs, pc.flipTimeUs / 1000.0,
		pc.spriteCount, pc.spriteTimeUs / 1000.0, spriteAvgUs,
		pc.worstSpriteUs, pc.worstSpriteW, pc.worstSpriteH,
		pc.worstSpriteSrcW, pc.worstSpriteSrcH, blendLabel, pathLabel,
		pc.shadowCount, pc.shadowTimeUs / 1000.0,
		pc.fillCalls,
		mpix,
		pc.fastAddRectBlits,
		pc.addAlphaSimpleSprites,
		pc.textCacheHits, pc.textCacheMisses,
		pc.blitCacheHits, pc.blitCacheMisses,
		ri.backendName);
}
