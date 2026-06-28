
#include <windows.h>
#include <emmintrin.h>   // SSE2: _mm_adds_epu8, _mm_loadu/storeu_si128
#include <locale.h>
#include <process.h>
#include <stdint.h>
#include <shlobj.h>
#include <math.h>
#include <float.h>
#include <iostream>
#include <sstream>
#include <ctime>
#include <map>

#include <GL/glew.h>

#include <png.h>
#include <SDL.h>
#include <SDL_syswm.h>
#include <SDL_vulkan.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <SDL_mixer.h>
#include <vlc/vlc.h>

// Winamp plugin SDK no longer needed – BGM uses SDL_mixer
// #include "IN2.H"
// #define IN_VER_UNICODE 0x0F000100

#include "sszdef.h"
#include "mem_profiler.hpp"
#include "arrayandref.hpp"
#include "renderer.h"

// ---------------------------------------------------------------------------
// Renderer global state
// ---------------------------------------------------------------------------
RendererType  g_rendererType       = RENDERER_SDL2;
GLVersion     g_glVersion          = GL_VER_NONE;
GLESVersion   g_glesVersion        = GLES_VER_NONE;
RendererInfo  g_rendererInfo;
PerfCounters  g_perfCounters;
bool          g_perfMonitorEnabled = false;
static uint32_t g_lastRenderPathFlags = SPRITE_PATH_NONE;

// ---------------------------------------------------------------------------
// OpenGL 3.3 / 4.6 shader state
// ---------------------------------------------------------------------------
// GL 3.3+ core-profile programs
static GLuint g_gl33_paletteProg   = 0;
static GLuint g_gl33_fullcolorProg = 0;
static GLuint g_gl33_shadowProg    = 0;
static GLuint g_gl33_vao           = 0;
static GLuint g_gl33_vbo           = 0;

// GL 3.3+ uniform locations
static GLint g_gl33_uPal      = -1;
static GLint g_gl33_uMask     = -1;
static GLint g_gl33_uAlpha    = -1;
static GLint g_gl33_uProjMat  = -1;

static GLint g_gl33fc_uNeg    = -1;
static GLint g_gl33fc_uGray   = -1;
static GLint g_gl33fc_uAdd    = -1;
static GLint g_gl33fc_uMul    = -1;
static GLint g_gl33fc_uAlpha  = -1;
static GLint g_gl33fc_uProjMat= -1;

static GLint g_gl33s_uColor   = -1;
static GLint g_gl33s_uAlpha   = -1;
static GLint g_gl33s_uProjMat = -1;

int32_t ransuutane = 0;
SDL_Window* g_window = nullptr;
SDL_Renderer* g_renderer = nullptr;
SDL_Texture* g_target = nullptr;
uint32_t* g_pix = nullptr;
SDL_GLContext g_gl = nullptr;
int g_pitch = 0;
int g_w = 640, g_h = 480;
float w_opacity = 1.0;
uint32_t g_scrflag = 0;
SDL_AudioSpec g_desired = {};
HGLRC g_hglrc = nullptr, g_hglrc2 = nullptr;
HDC g_hdc = nullptr;
DWORD g_mainTreadId = 0;

WNDPROC g_orgProc = nullptr;
char16_t g_lastChar = '\0', g_newChar = '\0';

void lockTarget()
{
	if(g_target) SDL_LockTexture(g_target, nullptr, (void**)&g_pix, &g_pitch);
}

void unlockTarget()
{
	if(g_target) SDL_UnlockTexture(g_target);
}

std::string WstrToStr(const std::wstring& wstr)
{
	if (wstr.empty()) return std::string();
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
	std::string strTo(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
	return strTo;
}

LRESULT CALLBACK wrapProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	MSG m;
	switch(msg){
	case WM_KEYDOWN:
		m.hwnd = hWnd;
		m.message = msg;
		m.wParam = wParam;
		m.lParam = lParam;
		m.time = 0;
		TranslateMessage(&m);
		if(
			TranslateMessage(&m)
			&& PeekMessage(&m, hWnd, WM_CHAR, WM_CHAR, PM_REMOVE))
		{
			g_newChar = m.wParam;
		}
		break;
	}
	return CallWindowProc(g_orgProc, hWnd, msg, wParam, lParam);
}

void winProcInit()
{
	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	SDL_GetWindowWMInfo(g_window, &info);
	g_orgProc = (WNDPROC)GetWindowLong(info.info.win.window, GWL_WNDPROC);
	if(g_orgProc == wrapProc) return;
	SetWindowLong(info.info.win.window, GWL_WNDPROC, (LONG)wrapProc);
}

const int g_samples = 2048;
const int g_sndfreq = 44100;
int16_t g_sndzero[g_samples*2] = {0};
int16_t g_sndbuf[g_samples*2] = {0};
int16_t *g_snddata = g_sndzero;

float g_vol = 1.0;
float wav_vol = 1.0;
float bgm_vol = 1.0;

const double PI = 3.14159265358979323846264338327950288;

// ---------------------------------------------------------------------------
// BGM state – SDL_mixer handles all music playback (WAV, MP3, OGG, FLAC, MOD…)
// ---------------------------------------------------------------------------
Mix_Music* g_bgmMusic = nullptr;
bool       bgm_paused = false;

// ---------------------------------------------------------------------------
// SFX callback – the script mixes raw int16 stereo data into g_sndbuf.
// SetSndBuf swaps it in; the SDL audio callback drains it.
// ---------------------------------------------------------------------------
void sndcallback(void* unused, Uint8* stream, int len)
{
	int i;
	for(i = 0; i < g_samples*2; i++){
		((int16_t*)stream)[i] = g_snddata[i];
	}
	g_snddata = g_sndzero;
}

// Stop and free the current BGM track.
void bgmclear()
{
	if(g_bgmMusic != nullptr){
		Mix_HaltMusic();
		Mix_FreeMusic(g_bgmMusic);
		g_bgmMusic = nullptr;
		bgm_paused = false;
		LOG_DEBUG("SDL", "bgmclear: SDL_mixer music stopped and freed");
	}
}

class Joystick
{
	std::basic_string<SDL_Joystick*> joys;
public:
	void init()
	{
		intptr_t i;
		joys.clear();
		for(i = 0; i < SDL_NumJoysticks(); i++){
			joys += SDL_JoystickOpen(i);
		}
	}
	void close()
	{
		intptr_t i;
		for(i = 0; i < (intptr_t)joys.size(); i++){
			if(joys[i] != nullptr){
				SDL_JoystickClose(joys[i]);
			}
		}
		joys.clear();
	}
	bool getState(int32_t joy, int32_t btn)
	{
		if(joy < 0) return SDL_GetKeyboardState(nullptr)[btn] == SDL_PRESSED;
		if(joy >= (int32_t)joys.size() || joys[joy] == nullptr){
			return false;
		}
		if(btn < 0) switch((-btn-1) & 7){
		case 0:
			return
				SDL_JoystickGetAxis(joys[joy],
				((-btn-1) >> 3)*2) < -3200;
		case 1:
			return
				SDL_JoystickGetAxis(joys[joy],
				((-btn-1) >> 3)*2) > 3200;
		case 2:
			return
				SDL_JoystickGetAxis(joys[joy],
				((-btn-1) >> 3)*2 + 1) < -3200;
		case 3:
			return
				SDL_JoystickGetAxis(joys[joy],
				((-btn-1) >> 3)*2 + 1) > 3200;
		case 4:
			return
				(
					SDL_JoystickGetHat(joys[joy],
					(-btn-1) >> 3) & SDL_HAT_LEFT)
				&& !(
					SDL_JoystickGetHat(joys[joy],
					(-btn-1) >> 3) & SDL_HAT_RIGHT);
		case 5:
			return
				!(
					SDL_JoystickGetHat(joys[joy],
					(-btn-1) >> 3) & SDL_HAT_LEFT)
				&& (
					SDL_JoystickGetHat(joys[joy],
					(-btn-1) >> 3) & SDL_HAT_RIGHT);
		case 6:
			return
				(
					SDL_JoystickGetHat(joys[joy],
					(-btn-1) >> 3) & SDL_HAT_UP)
				&& !(
					SDL_JoystickGetHat(joys[joy],
					(-btn-1) >> 3) & SDL_HAT_DOWN);
		case 7:
			return
				!(
					SDL_JoystickGetHat(joys[joy],
					(-btn-1) >> 3) & SDL_HAT_UP)
				&& (
					SDL_JoystickGetHat(joys[joy],
					(-btn-1) >> 3) & SDL_HAT_DOWN);
		}
		return SDL_JoystickGetButton(joys[joy], btn) == SDL_PRESSED;
	}
};
Joystick g_js;

GLhandleARB g_mugenshader = 0;
GLhandleARB g_mugenshaderFc = 0;
GLhandleARB g_mugenshaderFcS = 0;
static GLint g_uniformPal = 0;
static GLint g_uniformMsk = 0;
static GLint g_uniformNeg = 0;
static GLint g_uniformGray = 0;
static GLint g_uniformAdd = 0;
static GLint g_uniformMul = 0;
static GLint g_uniformColor = 0;
// [OPT] Cached "a" (alpha) uniform locations per shader program
// Avoids repeated glGetUniformLocation() calls every draw (was ~6-7 lookups per sprite)
static GLint g_uniformAlpha = 0;    // for g_mugenshader (indexed/paletted)
static GLint g_uniformAlphaFc = 0;  // for g_mugenshaderFc (full-color)
static GLint g_uniformAlphaFcS = 0; // for g_mugenshaderFcS (solid-color shadow)
static GLuint g_paltex = 0;
static bool g_paltexInitialized = false; // [OPT] Track palette texture allocation state
// [OPT] Track whether ortho projection is already set for this frame
// Avoids redundant glPushMatrix/glOrtho/glPopMatrix per sprite (~50-200+ sprites/frame)
static bool g_orthoProjectionSet = false;

void sndjoyinit()
{
	INIT_LOG("Initializing audio subsystem...");
	SDL_InitSubSystem(SDL_INIT_AUDIO);
	INIT_LOG("Audio subsystem initialized");
	
	// Skip joystick initialization - can hang on some Windows systems
	INIT_LOG("Skipping joystick initialization (can hang on Windows)");
	
	// SFX callback device (raw int16 stereo pushed by SetSndBuf)
	g_desired.freq = g_sndfreq;
	g_desired.format = AUDIO_S16;
	g_desired.channels = 2;
	g_desired.samples = g_samples;
	g_desired.callback = sndcallback;
	g_desired.userdata = nullptr;
	
	INIT_LOG("Opening SFX audio device (freq=%d, ch=2, samples=%d)...", g_sndfreq, g_samples);
	SDL_OpenAudio(&g_desired, nullptr);
	INIT_LOG("SFX audio device opened");
	
	SDL_PauseAudio(0);
	INIT_LOG("SFX audio started");

	// BGM device via SDL_mixer (WAV, MP3, OGG, FLAC, MOD, etc.)
	INIT_LOG("Initializing SDL_mixer...");
	int mixFlags = MIX_INIT_MP3 | MIX_INIT_OGG | MIX_INIT_FLAC | MIX_INIT_MOD;
	int mixInited = Mix_Init(mixFlags);
	LOG_DEBUG("SDL", "sndjoyinit: Mix_Init requested=0x%X got=0x%X", mixFlags, mixInited);
	INIT_LOG("SDL_mixer initialized");
	
	INIT_LOG("Opening BGM audio device...");
	if(Mix_OpenAudio(g_sndfreq, AUDIO_S16SYS, 2, g_samples) < 0){
		LOG_DEBUG("SDL", "sndjoyinit: Mix_OpenAudio failed: %s", Mix_GetError());
		INIT_LOG("BGM audio device FAILED: %s", Mix_GetError());
	}else{
		LOG_DEBUG("SDL", "sndjoyinit: Mix_OpenAudio OK (freq=%d, ch=2, samples=%d)", g_sndfreq, g_samples);
		INIT_LOG("BGM audio device opened");
	}

	INIT_LOG("Skipping joystick enumeration (SDL_INIT_JOYSTICK not enabled)");
	// g_js.init();  // Disabled - requires SDL_INIT_JOYSTICK which can hang on Windows
	INIT_LOG("Joystick skipped");
}

void TestIMG()
{
	if (SDL_Init(SDL_INIT_VIDEO) != 0)
	{
		return; //SDL_Init Error
	}
	if (IMG_Init(IMG_INIT_PNG) == 0)
	{
	//IMG_Init Error
		SDL_Quit();
		return;
	}
//Create a window and renderer to show stuff
	SDL_Window* window = SDL_CreateWindow("IMG Load Test", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, 0);
	if (!window)
	{
	//SDL_CreateWindow Error
		IMG_Quit();
		SDL_Quit();
		return;
	}
	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	if (!renderer)
	{
	//SDL_CreateRenderer Error
		SDL_DestroyWindow(window);
		IMG_Quit();
		SDL_Quit();
		return;
	}
//Load IMG
	const char* imagePath = "tools/test.jpg"; //For some reason PNG loading is not working
	SDL_Surface* imageSurface = IMG_Load(imagePath);
	if (!imageSurface)
	{
	//IMG_Load Error
		SDL_DestroyRenderer(renderer);
		SDL_DestroyWindow(window);
		IMG_Quit();
		SDL_Quit();
		return;
	}
//Create surface texture
	SDL_Texture* imageTexture = SDL_CreateTextureFromSurface(renderer, imageSurface);
	SDL_FreeSurface(imageSurface);
	if (!imageTexture)
	{
	//SDL_CreateTextureFromSurface Error
		SDL_DestroyRenderer(renderer);
		SDL_DestroyWindow(window);
		IMG_Quit();
		SDL_Quit();
		return;
	}
//Get Texture Size
	int texW = 0, texH = 0;
	SDL_QueryTexture(imageTexture, NULL, NULL, &texW, &texH);
//Clean Screen
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	SDL_RenderClear(renderer);
//Draw texture in middle
	SDL_Rect dstRect = { (640 - texW) / 2, (480 - texH) / 2, texW, texH };
	SDL_RenderCopy(renderer, imageTexture, NULL, &dstRect);
	SDL_RenderPresent(renderer);
//Time to display Test Window
	SDL_Delay(3000);
//Close
	SDL_DestroyTexture(imageTexture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	IMG_Quit();
	SDL_Quit();
}

void TestTTF()
{
//Init SDL and SDL_ttf
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		return; //Error SDL_Init
	}
	if (TTF_Init() == -1)
	{
		SDL_Quit(); //Error TTF_Init
		return;
	}
//Font Path
	const char* fontPath = "font/TTF/shanghai.ttf";
	int fontSize = 24;
//Load Font
	TTF_Font* font = TTF_OpenFont(fontPath, fontSize);
	if (!font)
	{
	//Error loading TTF Font
		TTF_Quit();
		SDL_Quit();
		return;
	}
//Render Text
	SDL_Color color = {255, 255, 255, 255}; //Text Color
	SDL_Surface* textSurface = TTF_RenderText_Solid(font, "Hello World!", color);
	if (!textSurface)
	{
	//Error rendering Text
		TTF_CloseFont(font);
		TTF_Quit();
		SDL_Quit();
		return;
	}
//Create a window and renderer to show stuff
	SDL_Window* window = SDL_CreateWindow("TTF Test", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, 0);
	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
//Create surface texture
	SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, textSurface);
//Clean Screen
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	SDL_RenderClear(renderer);
//Draw texture
	SDL_Rect dstRect = {50, 50, textSurface->w, textSurface->h};
	SDL_RenderCopy(renderer, texture, NULL, &dstRect);
	SDL_RenderPresent(renderer);
//Time to display Test Window
	SDL_Delay(3000);
//Close
	SDL_DestroyTexture(texture);
	SDL_FreeSurface(textSurface);
	TTF_CloseFont(font);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	TTF_Quit();
	SDL_Quit();
}

const int defaultTTFSize = 32;
void SSZ_STDCALL DrawTTF(int32_t alpha, int32_t b, int32_t g, int32_t r, float scaleY, float scaleX, int32_t y, int32_t x, const std::wstring& text, int32_t align, const std::wstring& fontPath)
{
	if (!g_renderer)
	{
	//Render is not available, can't draw.
		return;
	}
//Strings extraction
	std::wstring wFontPath = fontPath;
	std::string sFontPath = WstrToStr(wFontPath);
	std::wstring wText = text;
	std::string sText = WstrToStr(wText);
//Load Font
	TTF_Font* font = TTF_OpenFont(sFontPath.c_str(), defaultTTFSize);
	if (!font)
	{
	//Error Loading Font
		return;
	}
//Text Render
	SDL_Color ttfColor = {
		(uint8_t)r,
		(uint8_t)g,
		(uint8_t)b,
		(uint8_t)alpha //Opacity
	};
	//SDL_Surface* textSurface = TTF_RenderText_Solid(font, sText.c_str(), ttfColor);
	SDL_Surface* textSurface = TTF_RenderText_Blended(font, sText.c_str(), ttfColor); //Compatible when alpha is < 255
	if (!textSurface)
	{
	//Error rendering font
		TTF_CloseFont(font);
		return;
	}
//Texture creation
	SDL_Texture* texture = SDL_CreateTextureFromSurface(g_renderer, textSurface);
	SDL_FreeSurface(textSurface); //Release surface after create Texture
	if (!texture)
	{
		TTF_CloseFont(font);
		return;
	}
//Draw Pos Coords
	int originalWidth, originalHeight;
	SDL_QueryTexture(texture, NULL, NULL, &originalWidth, &originalHeight); //Get base size
	SDL_Rect dstRect = {0, 0, 0, 0};
//Set Scale
	dstRect.w = (int)(originalWidth * scaleX);
	dstRect.h = (int)(originalHeight * scaleY);
//Set Align (Editing X Pos)
	if (align == 1) //Left
	{
		dstRect.x = x - (dstRect.w / 2);
	}
	else if (align == -1) //Right
	{
		dstRect.x = x - dstRect.w;
	}
	else //Default: Center (align == 0)
	{
		dstRect.x = x;
	}
	dstRect.y = y; //Set Y Pos
//Draw Texture
	SDL_RenderCopy(g_renderer, texture, NULL, &dstRect);
	SDL_RenderPresent(g_renderer);
//Clear to avoid Memory Leak
	SDL_DestroyTexture(texture);
	TTF_CloseFont(font);
}

void TestRoom()
{
	//TestTTF();
	//TestIMG();
}

//Software Render
bool SSZ_STDCALL Init(bool mugen, int32_t h, int32_t w, const std::wstring& cap)
{
	//TestRoom(); //To test SDL Stuff
	if(SDL_Init(SDL_INIT_EVERYTHING) < 0)
	{
		return false;
	}
	else
	{
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear"); //Nearest Filter(0): SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest"); Linear Filter(1): SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
		//SDL_SetHint(SDL_HINT_RENDER_VSYNC, "0"); //VSYNC TEST
		TTF_Init(); //Initialize TTF loading
		g_scrflag = SDL_SWSURFACE;
		g_window = SDL_CreateWindow(WstrToStr(cap).c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h, g_scrflag);
		if(!g_window) return false;
		g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED);
		if(mugen)
		{
			g_target = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
			SDL_SetTextureBlendMode(g_target, SDL_BLENDMODE_NONE);
			lockTarget();
		}
		winProcInit();
		g_mainTreadId = GetCurrentThreadId();
		sndjoyinit();
	}
	g_w = w;
	g_h = h;
	return true;
}

//OpenGL Render
int isOpenGL = 0;
bool SSZ_STDCALL GlInit(int32_t h, int32_t w, const std::wstring& cap)
{
	if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
	{
		return false;;
	}
	else
	{
		isOpenGL = 1;
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
		TTF_Init();
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1); //3
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1); //1
		g_scrflag = SDL_WINDOW_OPENGL;
		g_window = SDL_CreateWindow(WstrToStr(cap).c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h, g_scrflag);
		if(!g_window) return false;
		g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED);
		g_gl = SDL_GL_CreateContext(g_window);
		if(glewInit() != GLEW_OK) return false;
		winProcInit();
		if(h == 0) h = 1; 
		glShadeModel(GL_SMOOTH);
		glClearColor(0.0, 0.0, 0.0, 1.0);
		glEnable(GL_DEPTH_TEST);
		glClearDepth(1.0);
		glDepthFunc(GL_LEQUAL);
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
		glViewport(0, 0, w, h);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		gluPerspective(45.0, (double)w/(double)h, 0.1, 1000.0);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		g_hglrc = wglGetCurrentContext();
		g_hdc = wglGetCurrentDC();
		g_hglrc2 = wglCreateContext(g_hdc);
		wglShareLists(g_hglrc, g_hglrc2);
		g_mainTreadId = GetCurrentThreadId();
		sndjoyinit();
	}
	g_w = w;
	g_h = h;
	return true;
}

// ---------------------------------------------------------------------------
// OpenGL version detection  –  probes the actual GL context
// ---------------------------------------------------------------------------
static GLVersion detectGLVersion()
{
	const char* verStr = (const char*)glGetString(GL_VERSION);
	if (!verStr) return GL_VER_NONE;
	INIT_LOG("OpenGL version string: %s", verStr);

	int major = 0, minor = 0;
	if (sscanf(verStr, "%d.%d", &major, &minor) < 2)
		return GL_VER_NONE;

	if (major > 4 || (major == 4 && minor >= 6)) return GL_VER_4_6;
	if (major > 3 || (major == 3 && minor >= 3)) return GL_VER_3_3;
	if (major > 2 || (major == 2 && minor >= 1)) return GL_VER_2_1;
	return GL_VER_NONE;
}

// ---------------------------------------------------------------------------
// GL 3.3+ shader compilation helpers
// ---------------------------------------------------------------------------
static GLuint compileShader(GLenum type, const char* src)
{
	GLuint s = glCreateShader(type);
	glShaderSource(s, 1, &src, NULL);
	glCompileShader(s);
	GLint ok = 0;
	glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
	if (!ok)
	{
		char log[512];
		glGetShaderInfoLog(s, sizeof(log), NULL, log);
		REND_LOG("Shader compile error: %s", log);
		glDeleteShader(s);
		return 0;
	}
	return s;
}

static GLuint linkProgram(GLuint vs, GLuint fs)
{
	GLuint prog = glCreateProgram();
	glAttachShader(prog, vs);
	glAttachShader(prog, fs);
	glLinkProgram(prog);
	GLint ok = 0;
	glGetProgramiv(prog, GL_LINK_STATUS, &ok);
	if (!ok)
	{
		char log[512];
		glGetProgramInfoLog(prog, sizeof(log), NULL, log);
		REND_LOG("Program link error: %s", log);
		glDeleteProgram(prog);
		return 0;
	}
	return prog;
}

// ---------------------------------------------------------------------------
// GL 3.3+ core-profile shaders  (GLSL 330)
// ---------------------------------------------------------------------------

// Palette-indexed sprite vertex shader (GL 3.3+)
static const char* g_gl33_palVS =
	"#version 330 core\n"
	"layout(location=0) in vec2 aPos;\n"
	"layout(location=1) in vec2 aUV;\n"
	"uniform mat4 uProj;\n"
	"out vec2 vUV;\n"
	"void main(){\n"
	"  vUV = aUV;\n"
	"  gl_Position = uProj * vec4(aPos, 0.0, 1.0);\n"
	"}\n";

// Palette-indexed sprite fragment shader (GL 3.3+)
static const char* g_gl33_palFS =
	"#version 330 core\n"
	"in vec2 vUV;\n"
	"uniform sampler2D tex;\n"
	"uniform sampler1D pal;\n"
	"uniform int msk;\n"
	"uniform float a;\n"
	"out vec4 FragColor;\n"
	"void main(){\n"
	"  float r = texture(tex, vUV).r;\n"
	"  if(int(255.0*r) == msk) discard;\n"
	"  vec4 c = texture(pal, r*0.9961);\n"
	"  FragColor = vec4(c.b, c.g, c.r, a);\n"
	"}\n";

// Full-color sprite fragment shader (GL 3.3+)
static const char* g_gl33_fcFS =
	"#version 330 core\n"
	"in vec2 vUV;\n"
	"uniform sampler2D tex;\n"
	"uniform bool neg;\n"
	"uniform float gray;\n"
	"uniform vec3 add;\n"
	"uniform vec3 mul;\n"
	"uniform float a;\n"
	"out vec4 FragColor;\n"
	"void main(){\n"
	"  vec4 c = texture(tex, vUV);\n"
	"  if(neg) c.rgb = vec3(1.0) - c.rgb;\n"
	"  c.rgb += (vec3((c.r+c.g+c.b)/3.0) - c.rgb)*gray + add;\n"
	"  c.rgb *= mul;\n"
	"  c.a *= a;\n"
	"  FragColor = c;\n"
	"}\n";

// Shadow sprite fragment shader (GL 3.3+)
static const char* g_gl33_shadowFS =
	"#version 330 core\n"
	"in vec2 vUV;\n"
	"uniform sampler2D tex;\n"
	"uniform vec3 color;\n"
	"uniform float a;\n"
	"out vec4 FragColor;\n"
	"void main(){\n"
	"  vec4 c = texture(tex, vUV);\n"
	"  c.rgb = color * c.a;\n"
	"  c.a *= a;\n"
	"  FragColor = c;\n"
	"}\n";

// ---------------------------------------------------------------------------
// GL 4.6 shaders (GLSL 460) – uses same logic but with GL 4.6 features
// ---------------------------------------------------------------------------
static const char* g_gl46_palVS =
	"#version 460 core\n"
	"layout(location=0) in vec2 aPos;\n"
	"layout(location=1) in vec2 aUV;\n"
	"layout(location=0) uniform mat4 uProj;\n"
	"out vec2 vUV;\n"
	"void main(){\n"
	"  vUV = aUV;\n"
	"  gl_Position = uProj * vec4(aPos, 0.0, 1.0);\n"
	"}\n";

static const char* g_gl46_palFS =
	"#version 460 core\n"
	"in vec2 vUV;\n"
	"layout(binding=0) uniform sampler2D tex;\n"
	"layout(binding=1) uniform sampler1D pal;\n"
	"layout(location=4) uniform int msk;\n"
	"layout(location=5) uniform float a;\n"
	"out vec4 FragColor;\n"
	"void main(){\n"
	"  float r = texture(tex, vUV).r;\n"
	"  if(int(255.0*r) == msk) discard;\n"
	"  vec4 c = texture(pal, r*0.9961);\n"
	"  FragColor = vec4(c.b, c.g, c.r, a);\n"
	"}\n";

static const char* g_gl46_fcFS =
	"#version 460 core\n"
	"in vec2 vUV;\n"
	"layout(binding=0) uniform sampler2D tex;\n"
	"layout(location=4) uniform bool neg;\n"
	"layout(location=5) uniform float gray;\n"
	"layout(location=6) uniform vec3 add;\n"
	"layout(location=7) uniform vec3 mul;\n"
	"layout(location=8) uniform float a;\n"
	"out vec4 FragColor;\n"
	"void main(){\n"
	"  vec4 c = texture(tex, vUV);\n"
	"  if(neg) c.rgb = vec3(1.0) - c.rgb;\n"
	"  c.rgb += (vec3((c.r+c.g+c.b)/3.0) - c.rgb)*gray + add;\n"
	"  c.rgb *= mul;\n"
	"  c.a *= a;\n"
	"  FragColor = c;\n"
	"}\n";

static const char* g_gl46_shadowFS =
	"#version 460 core\n"
	"in vec2 vUV;\n"
	"layout(binding=0) uniform sampler2D tex;\n"
	"layout(location=4) uniform vec3 color;\n"
	"layout(location=5) uniform float a;\n"
	"out vec4 FragColor;\n"
	"void main(){\n"
	"  vec4 c = texture(tex, vUV);\n"
	"  c.rgb = color * c.a;\n"
	"  c.a *= a;\n"
	"  FragColor = c;\n"
	"}\n";

// ---------------------------------------------------------------------------
// Build GL 3.3+ / 4.6 shader programs
// ---------------------------------------------------------------------------
static bool initGL33Shaders()
{
	const char* vsSource;
	const char* palFSSource;
	const char* fcFSSource;
	const char* shadowFSSource;

	if (g_glVersion >= GL_VER_4_6)
	{
		INIT_LOG("Building GLSL 460 shader pipeline...");
		vsSource       = g_gl46_palVS;
		palFSSource    = g_gl46_palFS;
		fcFSSource     = g_gl46_fcFS;
		shadowFSSource = g_gl46_shadowFS;
	}
	else
	{
		INIT_LOG("Building GLSL 330 shader pipeline...");
		vsSource       = g_gl33_palVS;
		palFSSource    = g_gl33_palFS;
		fcFSSource     = g_gl33_fcFS;
		shadowFSSource = g_gl33_shadowFS;
	}

	GLuint vs = compileShader(GL_VERTEX_SHADER, vsSource);
	if (!vs) return false;

	// Palette program
	GLuint palFS = compileShader(GL_FRAGMENT_SHADER, palFSSource);
	if (!palFS) { glDeleteShader(vs); return false; }
	g_gl33_paletteProg = linkProgram(vs, palFS);
	glDeleteShader(palFS);
	if (!g_gl33_paletteProg) { glDeleteShader(vs); return false; }

	g_gl33_uProjMat = glGetUniformLocation(g_gl33_paletteProg, "uProj");
	g_gl33_uPal     = glGetUniformLocation(g_gl33_paletteProg, "pal");
	g_gl33_uMask    = glGetUniformLocation(g_gl33_paletteProg, "msk");
	g_gl33_uAlpha   = glGetUniformLocation(g_gl33_paletteProg, "a");
	INIT_LOG("  Palette shader: program=%u", g_gl33_paletteProg);

	// Full-color program
	GLuint fcFS = compileShader(GL_FRAGMENT_SHADER, fcFSSource);
	if (!fcFS) { glDeleteShader(vs); return false; }
	g_gl33_fullcolorProg = linkProgram(vs, fcFS);
	glDeleteShader(fcFS);
	if (!g_gl33_fullcolorProg) { glDeleteShader(vs); return false; }

	g_gl33fc_uProjMat = glGetUniformLocation(g_gl33_fullcolorProg, "uProj");
	g_gl33fc_uNeg     = glGetUniformLocation(g_gl33_fullcolorProg, "neg");
	g_gl33fc_uGray    = glGetUniformLocation(g_gl33_fullcolorProg, "gray");
	g_gl33fc_uAdd     = glGetUniformLocation(g_gl33_fullcolorProg, "add");
	g_gl33fc_uMul     = glGetUniformLocation(g_gl33_fullcolorProg, "mul");
	g_gl33fc_uAlpha   = glGetUniformLocation(g_gl33_fullcolorProg, "a");
	INIT_LOG("  Full-color shader: program=%u", g_gl33_fullcolorProg);

	// Shadow program
	GLuint shFS = compileShader(GL_FRAGMENT_SHADER, shadowFSSource);
	if (!shFS) { glDeleteShader(vs); return false; }
	g_gl33_shadowProg = linkProgram(vs, shFS);
	glDeleteShader(shFS);
	if (!g_gl33_shadowProg) { glDeleteShader(vs); return false; }

	g_gl33s_uProjMat = glGetUniformLocation(g_gl33_shadowProg, "uProj");
	g_gl33s_uColor   = glGetUniformLocation(g_gl33_shadowProg, "color");
	g_gl33s_uAlpha   = glGetUniformLocation(g_gl33_shadowProg, "a");
	INIT_LOG("  Shadow shader: program=%u", g_gl33_shadowProg);

	glDeleteShader(vs);

	// Create VAO/VBO for core profile
	if (GLEW_ARB_vertex_array_object || g_glVersion >= GL_VER_3_3)
	{
		glGenVertexArrays(1, &g_gl33_vao);
		glGenBuffers(1, &g_gl33_vbo);
		glBindVertexArray(g_gl33_vao);
		glBindBuffer(GL_ARRAY_BUFFER, g_gl33_vbo);
		// 4 vertices * (2 pos + 2 uv) * float
		glBufferData(GL_ARRAY_BUFFER, 4 * 4 * sizeof(float), NULL, GL_DYNAMIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2*sizeof(float)));
		glBindVertexArray(0);
		INIT_LOG("  VAO=%u VBO=%u created", g_gl33_vao, g_gl33_vbo);
	}

	return true;
}

static void cleanupGL33Shaders()
{
	if (g_gl33_paletteProg)   { glDeleteProgram(g_gl33_paletteProg); g_gl33_paletteProg = 0; }
	if (g_gl33_fullcolorProg) { glDeleteProgram(g_gl33_fullcolorProg); g_gl33_fullcolorProg = 0; }
	if (g_gl33_shadowProg)    { glDeleteProgram(g_gl33_shadowProg); g_gl33_shadowProg = 0; }
	if (g_gl33_vao) { glDeleteVertexArrays(1, &g_gl33_vao); g_gl33_vao = 0; }
	if (g_gl33_vbo) { glDeleteBuffers(1, &g_gl33_vbo); g_gl33_vbo = 0; }
}

// ---------------------------------------------------------------------------
// Fill RendererInfo from current GL context
// ---------------------------------------------------------------------------
static void fillRendererInfo()
{
	g_rendererInfo.clear();
	g_rendererInfo.type = g_rendererType;
	g_rendererInfo.glVersion = g_glVersion;
	g_rendererInfo.glesVersion = g_glesVersion;

	if (g_rendererType == RENDERER_SDL2)
	{
		sprintf(g_rendererInfo.backendName, "SDL2 Software");
		sprintf(g_rendererInfo.deviceName, "CPU");
		sprintf(g_rendererInfo.driverInfo, "SDL %d.%d.%d",
			SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL);
	}
	else if (g_rendererType == RENDERER_OPENGL)
	{
		const char* ver = (const char*)glGetString(GL_VERSION);
		const char* ren = (const char*)glGetString(GL_RENDERER);
		const char* ven = (const char*)glGetString(GL_VENDOR);
		sprintf(g_rendererInfo.backendName, "OpenGL %s", glVersionName(g_glVersion));
		if (ren) strncpy(g_rendererInfo.deviceName, ren, sizeof(g_rendererInfo.deviceName)-1);
		if (ver) strncpy(g_rendererInfo.driverInfo, ver, sizeof(g_rendererInfo.driverInfo)-1);
		glGetIntegerv(GL_MAX_TEXTURE_SIZE, &g_rendererInfo.maxTextureSize);
		INIT_LOG("GPU: %s (%s)", ren ? ren : "?", ven ? ven : "?");
		INIT_LOG("Max texture size: %d", g_rendererInfo.maxTextureSize);
	}
	else if (g_rendererType == RENDERER_VULKAN)
	{
		sprintf(g_rendererInfo.backendName, "Vulkan (stub)");
		sprintf(g_rendererInfo.deviceName, "N/A");
		sprintf(g_rendererInfo.driverInfo, "N/A");
	}
}

// ---------------------------------------------------------------------------
// RendererInit  –  Unified initialisation for all backends
// ---------------------------------------------------------------------------
bool SSZ_STDCALL RendererInit(const std::wstring& rendererName, int32_t h, int32_t w, const std::wstring& cap)
{
	std::string rendStr = WstrToStr(rendererName);
	g_rendererType = parseRendererType(rendStr.c_str());
	g_perfCounters.init();
	g_perfCounters.fpsLastTick = SDL_GetTicks();

	INIT_LOG("========================================");
	INIT_LOG("I.K.E.M.E.N. Plus Ultra - Renderer Init");
	INIT_LOG("========================================");
	INIT_LOG("Requested renderer: \"%s\" -> %s", rendStr.c_str(), rendererTypeName(g_rendererType));
	INIT_LOG("Resolution: %dx%d", w, h);

	// --- Vulkan stub ---
	if (g_rendererType == RENDERER_VULKAN)
	{
		INIT_LOG("Initializing Vulkan backend (stub)...");
		if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
		{
			INIT_LOG("SDL_Init failed: %s", SDL_GetError());
			return false;
		}
		INIT_LOG("SDL initialized (Vulkan mode)");
		TTF_Init();
		INIT_LOG("SDL_ttf initialized");

		// Check for Vulkan support
		if (SDL_Vulkan_LoadLibrary(NULL) != 0)
		{
			REND_LOG("Vulkan not available: %s", SDL_GetError());
			REND_LOG("Falling back to OpenGL");
			g_rendererType = RENDERER_OPENGL;
			// Fall through to OpenGL init below
		}
		else
		{
			SDL_Vulkan_UnloadLibrary();
			g_scrflag = SDL_WINDOW_VULKAN;
			g_window = SDL_CreateWindow(
				WstrToStr(cap).c_str(),
				SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
				w, h, g_scrflag);
			if (!g_window)
			{
				REND_LOG("Failed to create Vulkan window: %s", SDL_GetError());
				REND_LOG("Falling back to OpenGL");
				g_rendererType = RENDERER_OPENGL;
			}
			else
			{
				INIT_LOG("Vulkan window created");
				REND_LOG("Vulkan rendering pipeline is not yet implemented");
				REND_LOG("Using Vulkan window with SDL2 software fallback for now");

				// Create SDL renderer as fallback for actual rendering
				g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED);
				g_target = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_ARGB8888,
					SDL_TEXTUREACCESS_STREAMING, w, h);
				SDL_SetTextureBlendMode(g_target, SDL_BLENDMODE_NONE);
				lockTarget();

				winProcInit();
				g_mainTreadId = GetCurrentThreadId();
				sndjoyinit();
				g_w = w;
				g_h = h;
				fillRendererInfo();
				INIT_LOG("Vulkan stub initialized (SDL2 fallback active)");
				INIT_LOG("Renderer ready: %s", g_rendererInfo.backendName);
				return true;
			}
		}
	}

	// --- OpenGL / OpenGL ES ---
	if (g_rendererType == RENDERER_OPENGL || g_rendererType == RENDERER_OPENGLES)
	{
		INIT_LOG("Initializing %s backend...", rendererTypeName(g_rendererType));
		if (SDL_Init(SDL_INIT_VIDEO) < 0)
		{
			INIT_LOG("SDL_Init(SDL_INIT_VIDEO) failed: %s", SDL_GetError());
			return false;
		}
		INIT_LOG("SDL initialized");

		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
		TTF_Init();
		INIT_LOG("SDL_ttf initialized");

		if (g_rendererType == RENDERER_OPENGLES)
		{
			// Request OpenGL ES context
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
			// Try highest first: 3.2
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
			INIT_LOG("Requesting OpenGL ES 3.2 context...");
		}
		else
		{
			// Request desktop OpenGL  –  try to get highest version
			// We'll request a compatibility profile so legacy code still works
			// and detect the actual version after context creation
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
			INIT_LOG("Requesting OpenGL context (will detect version)...");
		}

		isOpenGL = 1;
		g_scrflag = SDL_WINDOW_OPENGL;
		g_window = SDL_CreateWindow(
			WstrToStr(cap).c_str(),
			SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
			w, h, g_scrflag);
		if (!g_window)
		{
			INIT_LOG("Failed to create OpenGL window: %s", SDL_GetError());
			return false;
		}
		INIT_LOG("Window created (%dx%d)", w, h);

		g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED);
		g_gl = SDL_GL_CreateContext(g_window);
		if (!g_gl)
		{
			INIT_LOG("Failed to create GL context: %s", SDL_GetError());
			return false;
		}
		INIT_LOG("GL context created");

		if (glewInit() != GLEW_OK)
		{
			INIT_LOG("GLEW initialization failed");
			return false;
		}
		INIT_LOG("GLEW initialized");

		// Detect actual GL version
		if (g_rendererType == RENDERER_OPENGLES)
		{
			const char* verStr = (const char*)glGetString(GL_VERSION);
			INIT_LOG("OpenGL ES version: %s", verStr ? verStr : "unknown");
			// Parse ES version
			int major = 0, minor = 0;
			if (verStr && sscanf(verStr, "OpenGL ES %d.%d", &major, &minor) >= 2)
			{
				if (major >= 3 && minor >= 2) g_glesVersion = GLES_VER_3_2;
				else if (major >= 3 && minor >= 1) g_glesVersion = GLES_VER_3_1;
				else if (major >= 3) g_glesVersion = GLES_VER_3_0;
			}
			INIT_LOG("Detected OpenGL ES version: %s", glesVersionName(g_glesVersion));
		}
		else
		{
			g_glVersion = detectGLVersion();
			INIT_LOG("Detected OpenGL version: %s", glVersionName(g_glVersion));
		}

		winProcInit();
		INIT_LOG("Window procedure hook installed");

		if (h == 0) h = 1;
		glShadeModel(GL_SMOOTH);
		glClearColor(0.0, 0.0, 0.0, 1.0);
		glEnable(GL_DEPTH_TEST);
		glClearDepth(1.0);
		glDepthFunc(GL_LEQUAL);
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glViewport(0, 0, w, h);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		gluPerspective(45.0, (double)w / (double)h, 0.1, 1000.0);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		INIT_LOG("OpenGL state configured");

		// Build GL 3.3+/4.6 shaders if supported
		if (g_glVersion >= GL_VER_3_3)
		{
			INIT_LOG("Initializing modern shader pipeline (GL %s)...", glVersionName(g_glVersion));
			if (initGL33Shaders())
			{
				INIT_LOG("Modern shader pipeline ready");
			}
			else
			{
				REND_LOG("Modern shaders failed, will use legacy ARB path");
			}
		}

		g_hglrc = wglGetCurrentContext();
		g_hdc = wglGetCurrentDC();
		g_hglrc2 = wglCreateContext(g_hdc);
		wglShareLists(g_hglrc, g_hglrc2);
		INIT_LOG("Secondary GL context created for threading");

		g_mainTreadId = GetCurrentThreadId();
		sndjoyinit();
		INIT_LOG("Audio and joystick initialized");

		g_w = w;
		g_h = h;
		fillRendererInfo();
		INIT_LOG("Renderer ready: %s", g_rendererInfo.backendName);
		return true;
	}

	// --- SDL2 Software ---
	INIT_LOG("Initializing SDL2 software renderer...");
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		INIT_LOG("SDL_Init(SDL_INIT_VIDEO) failed: %s", SDL_GetError());
		return false;
	}
	INIT_LOG("SDL video initialized");

	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
	TTF_Init();
	INIT_LOG("SDL_ttf initialized");

	g_scrflag = SDL_SWSURFACE;
	g_window = SDL_CreateWindow(
		WstrToStr(cap).c_str(),
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		w, h, g_scrflag);
	if (!g_window)
	{
		INIT_LOG("Failed to create window: %s", SDL_GetError());
		return false;
	}
	INIT_LOG("Window created (%dx%d)", w, h);

	g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED);
	INIT_LOG("SDL renderer created");

	g_target = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_ARGB8888,
		SDL_TEXTUREACCESS_STREAMING, w, h);
	SDL_SetTextureBlendMode(g_target, SDL_BLENDMODE_NONE);
	lockTarget();
	INIT_LOG("Streaming texture created (%dx%d ARGB8888)", w, h);

	winProcInit();
	INIT_LOG("Window procedure hook installed");

	INIT_LOG("Getting main thread ID...");
	g_mainTreadId = GetCurrentThreadId();
	INIT_LOG("Main thread ID: %lu", g_mainTreadId);
	
	INIT_LOG("About to call sndjoyinit()...");
	sndjoyinit();
	INIT_LOG("Audio and joystick initialized");

	g_w = w;
	g_h = h;
	fillRendererInfo();
	INIT_LOG("Renderer ready: %s", g_rendererInfo.backendName);
	return true;
}

// ---------------------------------------------------------------------------
// GetRendererInfo  –  Return current renderer info as a string
// ---------------------------------------------------------------------------
void SSZ_STDCALL GetRendererInfo()
{
	char buf[512];
	sprintf(buf, "Backend: %s\nDevice: %s\nDriver: %s\nMax Texture: %d",
		g_rendererInfo.backendName,
		g_rendererInfo.deviceName,
		g_rendererInfo.driverInfo,
		g_rendererInfo.maxTextureSize);
	// Note: returning info via console since SSZ string return is complex
	REND_LOG("%s", buf);
}

// ---------------------------------------------------------------------------
// EnablePerfMonitor  –  Toggle performance monitoring output
// ---------------------------------------------------------------------------
void SSZ_STDCALL EnablePerfMonitor(bool enable)
{
	g_perfMonitorEnabled = enable;
	PERF_LOG("Performance monitoring %s", enable ? "ENABLED" : "DISABLED");
}

// Forward declarations for cache cleanup (defined after BlitSurface/RenderFont)
static void textCacheClear();
static void blitCacheClear();
static void textCacheEvictFont(TTF_Font* font);

void SSZ_STDCALL End()
{
	INIT_LOG("Shutting down renderer...");
	textCacheClear();
	blitCacheClear();
	cleanupGL33Shaders();
	wglDeleteContext(g_hglrc2);
	g_js.close();
	bgmclear();
	Mix_CloseAudio();
	Mix_Quit();
	SDL_PauseAudio(1);
	SDL_CloseAudio();
//Destroy Render
	if (g_target)
	{
		unlockTarget();
		SDL_DestroyTexture(g_target);
		g_target = nullptr;
	}
	if (g_gl)
	{
		SDL_GL_DeleteContext(g_gl);
		g_gl = nullptr;
	}
	SDL_DestroyRenderer(g_renderer);
	g_renderer = nullptr;
//Destroy Window
	SDL_DestroyWindow(g_window);
	g_window = nullptr;
//Quit SDL subsystems
	TTF_Quit();
	SDL_Quit();
	INIT_LOG("Renderer shutdown complete");
}

int fsMode = 0;
void SSZ_STDCALL FullScreenExclusive(bool fsr) //FullScreenExclusive need to be register in sdlplugin.def to work in lib/alpha/sdlplugin.ssz
{
	if (fsr == true)
	{
		fsMode = SDL_WINDOW_FULLSCREEN;
		//SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "1");
	}
	else
	{
		fsMode = SDL_WINDOW_FULLSCREEN_DESKTOP;
		SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0"); //Don't Minimize SDL_Window if it loses key focus when in fullscreen mode. Defaults to true.
	}
	(fsr ? fsMode : fsMode);
}

bool fullscreenChecker = false;
bool SSZ_STDCALL FullScreen(bool fs)
{
	//fullscreenChecker = !fullscreenChecker; //Change the value of fullscreen Checker to its opposite
	if (fs == true)
	{
		fullscreenChecker = true;
	}
	else
	{
		fullscreenChecker = false;
	}
	return SDL_SetWindowFullscreen(g_window, fs ? fsMode : 0) == 0; //flags may be SDL_WINDOW_FULLSCREEN, for "real" fullscreen with a videomode change; SDL_WINDOW_FULLSCREEN_DESKTOP for "fake" fullscreen that takes the size of the desktop; and 0 for windowed mode.
}

void WindowDecoration(bool wd)
{
	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	if (SDL_GetWindowWMInfo(g_window, &info))
	{
		HWND hwnd = info.info.win.window;
	//Get current window style
		LONG style = GetWindowLong(hwnd, GWL_STYLE);
	//Restore Buttons
		if (wd)
		{
			//style |= WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME;
			//style |= WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
			style |= WS_MINIMIZEBOX;
		}
	//Remove minimize/maximize buttons
		else
		{
			//style &= ~(WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME);
			style &= ~(WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
		}
		SetWindowLong(hwnd, GWL_STYLE, style);
	//Apply Changes
		SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
	}
}

void SSZ_STDCALL WindowType(int state) //Can't change the Window state of a fullscreen window.
{
//Window Resizable (Minimize + Maximize & Close Box)
	if(state == 1)
	{
		//WindowDecoration(true);
		SDL_SetWindowResizable(g_window, SDL_TRUE);
		SDL_SetWindowBordered(g_window, SDL_TRUE);
	}
//Window Bordered A (Original State: Minimize & Close Box)
	else if (state == 2)
	{
		WindowDecoration(true);
		SDL_SetWindowResizable(g_window, SDL_FALSE);
		SDL_SetWindowBordered(g_window, SDL_TRUE);
	}
//Window Bordered B (Only Show Close Box)
	else if (state == 3)
	{
		WindowDecoration(false);
		SDL_SetWindowResizable(g_window, SDL_FALSE);
		SDL_SetWindowBordered(g_window, SDL_TRUE);
	}
//No Window Decorations (Remove decoration from actual window)
	else if (state == 4)
	{
		WindowDecoration(false);
		SDL_SetWindowResizable(g_window, SDL_FALSE);
		SDL_SetWindowBordered(g_window, SDL_FALSE);
	}
//Load Default Resizable Setting
	else
	{
		WindowDecoration(true);
		SDL_SetWindowResizable(g_window, SDL_TRUE);
		SDL_SetWindowBordered(g_window, SDL_TRUE);
	}
}

int SSZ_STDCALL GetWidth()
{
	return GetSystemMetrics(SM_CXSCREEN);
}

int SSZ_STDCALL GetHeight()
{
	return GetSystemMetrics(SM_CYSCREEN);
}

void SSZ_STDCALL WindowSize(int height, int width)
{
	//g_w = width;
	//g_h = height;
	SDL_SetWindowSize(g_window, width, height);
}

bool isAspectRatio = false;
void SSZ_STDCALL AspectRatio(bool aspect)
{
	if (aspect == true)
	{
		isAspectRatio = true;
		SDL_RenderSetLogicalSize(g_renderer, g_w, g_h);
		//SDL_SetWindowSize(g_window, g_w, g_h); //Adjust window size
		SDL_RenderClear(g_renderer); //To refresh the screen
	}
	else
	{
		isAspectRatio = false;
		//SDL_SetWindowSize(g_window, original_w, original_h); //Restore original window size
		SDL_RenderSetLogicalSize(g_renderer, 0, 0); //Clear LogicalSize to restore the window
	}
}

void SSZ_STDCALL SetOpacity(float wo)
{
	w_opacity = wo;
	if (w_opacity < 0.0)
	{
		w_opacity = 0.0;
	}
	else if (w_opacity > 1.0)
	{
		w_opacity = 1.0;
	}
	SDL_SetWindowOpacity(g_window, w_opacity); //(0.0f - transparent, 1.0f - opaque)
}

bool winMaximized = false;
void SSZ_STDCALL TakeScreenShot(const std::wstring& dir)
{
//Software Render
	if (isOpenGL == 0)
	{
	//Window Mode
		if (fullscreenChecker == false)
		{
		//Check Window State
			Uint32 flags = SDL_GetWindowFlags(g_window);
			if (flags & SDL_WINDOW_MAXIMIZED)
			{
				winMaximized = true; //Window Maximized
			}
			else
			{
				winMaximized = false;
			}
		//Window is not Maximized
			if (winMaximized == false)
			{
				SDL_Surface *screenshot = SDL_CreateRGBSurface(0, g_w, g_h, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
				SDL_RenderReadPixels(g_renderer, NULL, SDL_PIXELFORMAT_ARGB8888, screenshot->pixels, screenshot->pitch);
				IMG_SavePNG(screenshot, WstrToStr(dir).c_str());
				SDL_FreeSurface(screenshot);
			}
		//Window is Maximized (Reuse Fullscreen screenshot logic to avoid crash)
			else
			{
				SDL_Surface *screenshot = SDL_CreateRGBSurface(0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
				SDL_RenderReadPixels(g_renderer, NULL, SDL_PIXELFORMAT_ARGB8888, screenshot->pixels, screenshot->pitch);
				IMG_SavePNG(screenshot, WstrToStr(dir).c_str());
				SDL_FreeSurface(screenshot);
			}
		}
	//Fullscreen Mode or Maximized Screen
		else
		{
			SDL_Surface *screenshot = SDL_CreateRGBSurface(0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
			SDL_RenderReadPixels(g_renderer, NULL, SDL_PIXELFORMAT_ARGB8888, screenshot->pixels, screenshot->pitch);
			IMG_SavePNG(screenshot, WstrToStr(dir).c_str());
			SDL_FreeSurface(screenshot);
		}
	}
//OpenGL Render
	else
	{
	//Capture using glReadPixels
		glReadBuffer(GL_FRONT); //buffer can be GL_FRONT o GL_BACK
		unsigned char* pixels = new unsigned char[g_w * g_h * 4];
		glReadPixels(0, 0, g_w, g_h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	//Since OpenGL reads from the bottom, the image will be vertically inverted. Here we can fix that flipping
		unsigned char* flippedPixels = new unsigned char[g_w * g_h * 4];
		for (int y = 0; y < g_h; y++)
		{
			memcpy(flippedPixels + y * g_w * 4, pixels + (g_h - 1 - y) * g_w * 4, g_w * 4);
		}
	//Save using libpng
		{
			std::string path = WstrToStr(dir);
			FILE* fp = fopen(path.c_str(), "wb");
			if(fp){
				png_structp png_ptr = png_create_write_struct(
					PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
				png_infop info_ptr = png_ptr
					? png_create_info_struct(png_ptr) : nullptr;
				if(png_ptr && info_ptr && !setjmp(png_jmpbuf(png_ptr))){
					png_init_io(png_ptr, fp);
					png_set_IHDR(png_ptr, info_ptr,
						g_w, g_h, 8, PNG_COLOR_TYPE_RGBA,
						PNG_INTERLACE_NONE,
						PNG_COMPRESSION_TYPE_DEFAULT,
						PNG_FILTER_TYPE_DEFAULT);
					png_write_info(png_ptr, info_ptr);
					for(int y = 0; y < g_h; y++)
						png_write_row(png_ptr, flippedPixels + y * g_w * 4);
					png_write_end(png_ptr, nullptr);
				}
				if(png_ptr) png_destroy_write_struct(&png_ptr, &info_ptr);
				fclose(fp);
			}
		}
		delete[] pixels;
		delete[] flippedPixels;
	}
}

//To update window resize in OpenGL context
bool SSZ_STDCALL UpdateGLViewport(const SDL_Event& event)
{
	if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_MAXIMIZED)
	{
		int32_t new_w = event.window.data1;
		int32_t new_h = event.window.data2;
		if (new_h == 0) new_h = 1;
		glViewport(0, 0, new_w, new_h);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		gluPerspective(45.0, (double)new_w / (double)new_h, 0.1, 1000.0);
		glMatrixMode(GL_MODELVIEW);
		g_w = new_w;
		g_h = new_h;
		return true; //Resize ok
	}
	return false; //Is not a resize event
}

//Video Player Definition
typedef libvlc_instance_t *(*PFN_libvlc_new)(int argc, const char *const *argv);
typedef void (*PFN_libvlc_release)(libvlc_instance_t *p_instance);
typedef libvlc_media_t *(*PFN_libvlc_media_new_path)(libvlc_instance_t *p_instance, const char *path);
typedef void (*PFN_libvlc_media_release)(libvlc_media_t *p_md);
typedef libvlc_media_player_t *(*PFN_libvlc_media_player_new_from_media)(libvlc_media_t *p_md);
typedef void (*PFN_libvlc_media_player_release)(libvlc_media_player_t *p_mi);
typedef void (*PFN_libvlc_media_player_set_hwnd)(libvlc_media_player_t *p_mi, void *drawable);

typedef void (*PFN_libvlc_media_player_play)(libvlc_media_player_t *p_mi);
typedef void (*PFN_libvlc_media_player_stop)(libvlc_media_player_t *p_mi);
typedef libvlc_state_t (*PFN_libvlc_media_player_get_state)(libvlc_media_player_t *p_mi);

typedef void (*PFN_libvlc_video_set_aspect_ratio)(libvlc_media_player_t *p_mi, const char *psz_aspect);
typedef void (*PFN_libvlc_video_set_scale)(libvlc_media_player_t *p_mi, float f_zoom);

typedef int (*PFN_libvlc_audio_set_volume)(libvlc_media_player_t *p_mi, int volume);
typedef int (*PFN_libvlc_audio_get_track_count)(libvlc_media_player_t *p_mi);
typedef int (*PFN_libvlc_audio_set_track)(libvlc_media_player_t *p_mi, int i_track);

typedef int (*PFN_libvlc_spu_get_track_count)(libvlc_media_player_t *p_mi);
typedef int (*PFN_libvlc_spu_set_track)(libvlc_media_player_t *p_mi, int i_track);

typedef int (*libvlc_video_take_snapshot_t)(libvlc_media_player_t *p_mi, unsigned int i_num, const char *psz_filepath, unsigned int i_width, unsigned int i_height);

//Structure to store the DLL handle and all pointers
struct VLCLoader
{
//Using the constructor for initialization
	VLCLoader() : hVlcDll(NULL), 
		libvlc_new(nullptr), 
		libvlc_release(nullptr), 
		libvlc_media_new_path(nullptr),
		libvlc_media_release(nullptr),
		libvlc_media_player_new_from_media(nullptr),
		libvlc_media_player_release(nullptr),
		libvlc_media_player_set_hwnd(nullptr),
		libvlc_media_player_play(nullptr),
		libvlc_media_player_stop(nullptr),
		libvlc_media_player_get_state(nullptr),
		libvlc_video_set_aspect_ratio(nullptr),
		libvlc_video_set_scale(nullptr),
		libvlc_audio_set_volume(nullptr),
		libvlc_audio_get_track_count(nullptr),
		libvlc_audio_set_track(nullptr),
		libvlc_spu_get_track_count(nullptr),
		libvlc_spu_set_track(nullptr),
		libvlc_video_take_snapshot(nullptr) {}
	HINSTANCE hVlcDll; //Declaration only, no initialization here
	libvlc_video_take_snapshot_t libvlc_video_take_snapshot;
	PFN_libvlc_new libvlc_new;
	PFN_libvlc_release libvlc_release;
	PFN_libvlc_media_new_path libvlc_media_new_path;
	PFN_libvlc_media_release libvlc_media_release;
	PFN_libvlc_media_player_new_from_media libvlc_media_player_new_from_media;
	PFN_libvlc_media_player_release libvlc_media_player_release;
	PFN_libvlc_media_player_set_hwnd libvlc_media_player_set_hwnd;
	PFN_libvlc_media_player_play libvlc_media_player_play;
	PFN_libvlc_media_player_stop libvlc_media_player_stop;
	PFN_libvlc_media_player_get_state libvlc_media_player_get_state;
	PFN_libvlc_video_set_aspect_ratio libvlc_video_set_aspect_ratio;
	PFN_libvlc_video_set_scale libvlc_video_set_scale;
	PFN_libvlc_audio_set_volume libvlc_audio_set_volume;
	PFN_libvlc_audio_get_track_count libvlc_audio_get_track_count;
	PFN_libvlc_audio_set_track libvlc_audio_set_track;
	PFN_libvlc_spu_get_track_count libvlc_spu_get_track_count;
	PFN_libvlc_spu_set_track libvlc_spu_set_track;
};

VLCLoader g_vlc; //Global instance to be used by PlayVLCVideo()

//Help macro for resolving pointers
#define LOAD_VLC_FUNC(name) \
	g_vlc.name = (PFN_##name)GetProcAddress(g_vlc.hVlcDll, #name); \
	if (!g_vlc.name) { \
		/* Error handling if a function does not resolve */ \
		FreeLibrary(g_vlc.hVlcDll); \
		g_vlc.hVlcDll = NULL; \
		return false; \
	}

bool LoadVLCFunctions()
{
	if (g_vlc.hVlcDll) return true; //Already Loaded
//Load DLL (libvlc.dll)
	g_vlc.hVlcDll = LoadLibraryA("libvlc.dll");
	if (g_vlc.hVlcDll == NULL)
	{
		//Failed to load libvlc.dll (may not be in the executable path)
		return false;
	}
//Load libvlc_video_take_snapshot
	g_vlc.libvlc_video_take_snapshot = (libvlc_video_take_snapshot_t)GetProcAddress(g_vlc.hVlcDll, "libvlc_video_take_snapshot");
//Check if load correctly
	if (!g_vlc.libvlc_video_take_snapshot)
	{
		//return false;
	}
//Resolve all function pointers
	LOAD_VLC_FUNC(libvlc_new);
	LOAD_VLC_FUNC(libvlc_release);
	LOAD_VLC_FUNC(libvlc_media_new_path);
	LOAD_VLC_FUNC(libvlc_media_release);
	LOAD_VLC_FUNC(libvlc_media_player_new_from_media);
	LOAD_VLC_FUNC(libvlc_media_player_release);
	LOAD_VLC_FUNC(libvlc_media_player_set_hwnd);
	LOAD_VLC_FUNC(libvlc_media_player_play);
	LOAD_VLC_FUNC(libvlc_media_player_stop);
	LOAD_VLC_FUNC(libvlc_media_player_get_state);
	LOAD_VLC_FUNC(libvlc_video_set_aspect_ratio);
	LOAD_VLC_FUNC(libvlc_video_set_scale);
	LOAD_VLC_FUNC(libvlc_audio_set_volume);
	LOAD_VLC_FUNC(libvlc_audio_get_track_count);
	LOAD_VLC_FUNC(libvlc_audio_set_track);
/*
	LOAD_VLC_FUNC(libvlc_spu_get_track_count);
	LOAD_VLC_FUNC(libvlc_spu_set_track);
*/
	return true; //Success
}

void UnloadVLCFunctions()
{
	if (g_vlc.hVlcDll)
	{
		FreeLibrary(g_vlc.hVlcDll);
		g_vlc.hVlcDll = NULL;
	}
}

int PlayVLCVideo(const std::string& videoPath, const std::string& capturePath, int volume, int audioTrack)//, int subtitleTrack)
{
//Load DLL and VLC Functions
	if (!LoadVLCFunctions())
	{
		//std::cerr << "Error: Cannot load libvlc.dll or his functions." << std::endl;
		return -1;
	}
//Get the window HWND (Windows handle) for VLC Player
	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	if (!SDL_GetWindowWMInfo(g_window, &info))
	{
		//std::cerr << "Error SDL_GetWindowWMInfo: " << SDL_GetError() << std::endl;
		UnloadVLCFunctions();
		return -1;
	}
	HWND hwnd = info.info.win.window;
//Since for some reason the track is offset by 1, fix manually:
	int audioTrackFix = audioTrack;
	if (audioTrack > 0)
	{
		audioTrackFix = audioTrack - 1;
	}
	else if (audioTrack == 0)
	{
		//audioTrackFix = 1;
	}
//Write Audio Track prefix and value to the stream
	std::stringstream ss;
	ss << "--audio-track=" << audioTrackFix;
	std::string audioTrackArg = ss.str(); //Get Final String
//Initializing and Loading libVLC (Dynamic Loading)	
	const char *const vlc_args[] = {
		"--plugin-path=./plugins", //Plugins Path
		audioTrackArg.c_str() //Audio Track argument
	};
	int arg_count = sizeof(vlc_args) / sizeof(vlc_args[0]);
	libvlc_instance_t *vlcInstance = g_vlc.libvlc_new(arg_count, vlc_args);
	if (!vlcInstance)
	{
		UnloadVLCFunctions(); 
		return -1;
	}
//Video Load
	libvlc_media_t *media = g_vlc.libvlc_media_new_path(vlcInstance, videoPath.c_str());
	if (!media)
	{
		//std::cerr << "Error loading media: " << videoPath << std::endl;
		LOG_INFO("SSZ", "Error loading media: %s", videoPath.c_str());
		g_vlc.libvlc_release(vlcInstance);
		UnloadVLCFunctions(); 
		return -1;
	}
//Create and configure the video player
	libvlc_media_player_t *mediaPlayer = g_vlc.libvlc_media_player_new_from_media(media);
	g_vlc.libvlc_media_release(media);
	if (!mediaPlayer)
	{
		//std::cerr << "Error creating video player" << std::endl;
		LOG_INFO("SSZ", "Error creating video player");
		g_vlc.libvlc_release(vlcInstance);
		UnloadVLCFunctions(); 
		return -1;
	}
//Volume Control
	int videoVolume = volume; //Range: 0-200
	if (g_vlc.libvlc_audio_set_volume)
	{
		g_vlc.libvlc_audio_set_volume(mediaPlayer, videoVolume);
	}
//Link video output to window handle
	g_vlc.libvlc_media_player_set_hwnd(mediaPlayer, hwnd);
//Play Video
	g_vlc.libvlc_media_player_play(mediaPlayer);
//Apply Aspect Ratio (if enabled)
	if (isAspectRatio)
	{
		std::stringstream ssAR;
		ssAR << g_w << ":" << g_h;
		std::string aspectRatioStr = ssAR.str();
		g_vlc.libvlc_video_set_aspect_ratio(mediaPlayer, aspectRatioStr.c_str());
	}
//Get Window Name for Screenshot
	const char* titleCStr = SDL_GetWindowTitle(g_window);
	std::string sdlWindowTitle = (titleCStr != nullptr) ? std::string(titleCStr) : "IKEMEN VIDEO"; //Set a default name in case that nil
//Events Process (This keep the video active)
	SDL_Event event;
	bool quit = false;
	bool fullExit = false;
	while (!quit)
	{
		while (SDL_PollEvent(&event))
		{
		//End via window close or ALT+F4
			if (event.type == SDL_QUIT)
			{
				//quit = true;
				fullExit = true;
			}
		//Check which key is pressed to Skip/End video
			if (event.type == SDL_KEYDOWN)
			{
				SDL_Keycode key = event.key.keysym.sym;
				if (key == SDLK_RETURN || key == SDLK_KP_ENTER || key == SDLK_ESCAPE || key == SDLK_BACKSPACE)
				{
					quit = true;
				}
			//Screenshots logic
				if (key == SDLK_PRINTSCREEN && g_vlc.libvlc_video_take_snapshot)
				{
				//Get Current Time
					std::time_t now = std::time(nullptr);
					std::tm localTime;
					if (localtime_s(&localTime, &now) != 0)
					{
						//Error getting time..
						return -1;
					}
				//Set format: %Y=Year, %m=Month, %d=Day, %I=Hour (12h), %M=Minute, %p=AM/PM, %S=Second.
					char timeBuffer[80];
					std::strftime(timeBuffer, sizeof(timeBuffer), " %Y-%m-%d %I-%M%p-%S", &localTime);
				//Set Path ("capturePath/WINDOW TITLE 2025-10-27 11-00PM-33.png")
					std::stringstream ssPath;
					ssPath << capturePath; //Add folder separator "/" if it doesn't exist at the end of capturePath
					if (capturePath.back() != '/' && capturePath.back() != '\\')
					{
						ssPath << "/";
					}
					ssPath << sdlWindowTitle << timeBuffer << ".png";
					std::string finalScreenshotPath = ssPath.str();
				//Take screenshot
					int result = g_vlc.libvlc_video_take_snapshot(mediaPlayer, 0, finalScreenshotPath.c_str(), 0, 0);
					if (result == 0)
					{
						LOG_INFO("SSZ", "Screenshot saved in: %s", finalScreenshotPath.c_str());
					}
					else
					{
						LOG_INFO("SSZ", "Error taking screenshot from VLC.");
					}
				}
			}
		}
	//Check if video ended
		if (!quit && g_vlc.libvlc_media_player_get_state(mediaPlayer) == libvlc_Ended)
		{
			quit = true;
		}
		SDL_Delay(10); //To avoid CPU use increase and close video player properly
	}
//Close
	g_vlc.libvlc_media_player_stop(mediaPlayer);
	g_vlc.libvlc_media_player_release(mediaPlayer);
	g_vlc.libvlc_release(vlcInstance);
	UnloadVLCFunctions();
	if (fullExit)
	{
		return 0; //Send to ssz
	}
	return 1; //Success
}

int SSZ_STDCALL PlayVideo(const std::wstring& fn, const std::wstring& screenshotPath, int volume, int audioTrack)//, int subtitleTrack)
{
	std::wstring wVideoPath = fn; //Get video path as wide string
	std::string videoPath = WstrToStr(wVideoPath); //Convert the path to narrow string (std::string) for libvlc_media_new_path
//Same for screenshots path
	std::wstring wShotPath = screenshotPath;
	std::string capturePath = WstrToStr(wShotPath);
	MEM_MARK_BEFORE_NAMED(VIDEO, videoPath.c_str());
	int result = PlayVLCVideo(videoPath, capturePath, volume, audioTrack);//, subtitleTrack;
	MEM_MARK_AFTER_NAMED(VIDEO, videoPath.c_str());
	return result;
}

bool SSZ_STDCALL PollEvent(int8_t* pb)
{
	if(pb == nullptr){
		LOG_INFO("SSZ", "sdlplugin::PollEvent pb is null");
		return false;
	}
	SDL_Event ev;

	const int comsz = sizeof(ev.common);

	const int keyofs     =           0;

	const int motionofs  =    keyofs+sizeof(ev.key)-comsz;

	const int buttonofs  =  motionofs+sizeof(ev.motion)-comsz;

	bool ret;
	SDL_JoystickUpdate();
	ret = SDL_PollEvent(&ev) != 0;
	g_lastChar = g_newChar;
	if(!ret) g_newChar = '\0';

	*(int32_t*)pb = ev.type;              pb += sizeof(int32_t);
	*(uint32_t*)pb = ev.common.timestamp; pb += sizeof(uint32_t);
	switch(ev.type){
	case SDL_KEYDOWN:
	case SDL_KEYUP:
		pb += keyofs;
		memcpy(pb, (int8_t*)&ev.key+comsz, sizeof(ev.key)-comsz);
		break;
	case SDL_MOUSEMOTION:
		pb += motionofs;
		memcpy(pb, (int8_t*)&ev.motion+comsz, sizeof(ev.motion)-comsz);
		break;
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
		pb += buttonofs;
		memcpy(pb, (int8_t*)&ev.button+comsz, sizeof(ev.button)-comsz);
		break;
	}
	return ret;
}

char16_t SSZ_STDCALL GetLastChar()
{
	return g_lastChar;
}

bool SSZ_STDCALL KeyState(int32_t key)
{
	int keyCount = 0;
	const uint8_t* state = SDL_GetKeyboardState(&keyCount);
	if(state == nullptr){
		LOG_INFO("SSZ", "sdlplugin::KeyState state is null key=%d", (int)key);
		return false;
	}
	if(key < 0 || key >= keyCount){
		LOG_INFO("SSZ", "sdlplugin::KeyState out-of-range key=%d count=%d", (int)key, keyCount);
		return false;
	}
	return state[key] == SDL_PRESSED;
}

bool SSZ_STDCALL JoystickButtonState(int32_t btn, int32_t joy)
{
	return g_js.getState(joy, btn);
}

// ---------------------------------------------------------------------------
// Batched input poll — returns a 14-bit bitmask of all button states.    [OPT3]
// Eliminates 27 FFI round-trips per player per frame vs individual calls.
// ---------------------------------------------------------------------------
int32_t SSZ_STDCALL PollInputBitmask(int32_t jn,
	int32_t u,  int32_t d,  int32_t l,  int32_t r,
	int32_t a,  int32_t b,  int32_t c,
	int32_t x,  int32_t y,  int32_t z,
	int32_t q,  int32_t w,  int32_t e,  int32_t s,
	int32_t jn2,
	int32_t u2, int32_t d2, int32_t l2, int32_t r2,
	int32_t a2, int32_t b2, int32_t c2,
	int32_t x2, int32_t y2, int32_t z2,
	int32_t q2, int32_t w2, int32_t e2, int32_t s2,
	int32_t sec)
{
	int32_t mask =
		(g_js.getState(jn, u) ? (1<<0) : 0) |
		(g_js.getState(jn, d) ? (1<<1) : 0) |
		(g_js.getState(jn, l) ? (1<<2) : 0) |
		(g_js.getState(jn, r) ? (1<<3) : 0) |
		(g_js.getState(jn, a) ? (1<<4) : 0) |
		(g_js.getState(jn, b) ? (1<<5) : 0) |
		(g_js.getState(jn, c) ? (1<<6) : 0) |
		(g_js.getState(jn, x) ? (1<<7) : 0) |
		(g_js.getState(jn, y) ? (1<<8) : 0) |
		(g_js.getState(jn, z) ? (1<<9) : 0) |
		(g_js.getState(jn, q) ? (1<<10) : 0) |
		(g_js.getState(jn, w) ? (1<<11) : 0) |
		(g_js.getState(jn, e) ? (1<<12) : 0) |
		(g_js.getState(jn, s) ? (1<<13) : 0);
	if (sec) {
		mask |=
			(g_js.getState(jn2, u2) ? (1<<0) : 0) |
			(g_js.getState(jn2, d2) ? (1<<1) : 0) |
			(g_js.getState(jn2, l2) ? (1<<2) : 0) |
			(g_js.getState(jn2, r2) ? (1<<3) : 0) |
			(g_js.getState(jn2, a2) ? (1<<4) : 0) |
			(g_js.getState(jn2, b2) ? (1<<5) : 0) |
			(g_js.getState(jn2, c2) ? (1<<6) : 0) |
			(g_js.getState(jn2, x2) ? (1<<7) : 0) |
			(g_js.getState(jn2, y2) ? (1<<8) : 0) |
			(g_js.getState(jn2, z2) ? (1<<9) : 0) |
			(g_js.getState(jn2, q2) ? (1<<10) : 0) |
			(g_js.getState(jn2, w2) ? (1<<11) : 0) |
			(g_js.getState(jn2, e2) ? (1<<12) : 0) |
			(g_js.getState(jn2, s2) ? (1<<13) : 0);
	}
	return mask;
}

// ---------------------------------------------------------------------------
// Software pixel-buffer fill (writes directly into g_pix)
// Used by the SDL2 software-renderer path for opaque fills.
// ---------------------------------------------------------------------------
void SSZ_STDCALL SoftFill(uint32_t color, SDL_Rect* prect)
{
	LARGE_INTEGER t0, t1;
	QueryPerformanceCounter(&t0);
	g_perfCounters.fillCalls++;
	g_perfCounters.drawCalls++;
	if (!g_pix) return;

	int stride = g_pitch / 4;
	int x0 = prect->x;
	int y0 = prect->y;
	int x1 = x0 + prect->w;
	int y1 = y0 + prect->h;

	// Clip to screen
	if (x0 < 0) x0 = 0;
	if (y0 < 0) y0 = 0;
	if (x1 > g_w) x1 = g_w;
	if (y1 > g_h) y1 = g_h;
	if (x0 >= x1 || y0 >= y1) return;

	int rowPixels = x1 - x0;
	if (color == 0)
	{
		// Black/transparent: fast memset
		for (int y = y0; y < y1; y++)
			memset(g_pix + y * stride + x0, 0, rowPixels * 4);
	}
	else
	{
		for (int y = y0; y < y1; y++)
		{
			uint32_t* row = g_pix + y * stride + x0;
			for (int i = 0; i < rowPixels; i++)
				row[i] = color;
		}
	}
	QueryPerformanceCounter(&t1);
	g_perfCounters.fillTimeUs += qpcElapsedUs(t0, t1, g_perfCounters.qpcFreq);
}

void SSZ_STDCALL Fill(uint32_t color, SDL_Rect* prect)
{
	g_perfCounters.fillCalls++;
	g_perfCounters.drawCalls++;
	SDL_SetRenderDrawColor(g_renderer, color>>16&0xff, color>>8&0xff, color&0xff, 0xff); //g_renderer, 0, 0, 0, 255);
	SDL_RenderFillRect(g_renderer, prect);
}

intptr_t SSZ_STDCALL IMGLoad(const std::wstring& fn)
{
	return (intptr_t)IMG_Load(WstrToStr(fn).c_str());
}

void SSZ_STDCALL DecodePNG8(FILE* fp, int32_t* h, int32_t* w, std::vector<uint8_t>& out)
{
	out.clear();
	*w = *h = 0;
	if(!fp) return;
	uint8_t header[8] = {0};
	fread(header, 1, 8, fp);
	if(png_sig_cmp(header, 0, 8)) return;
	auto png_ptr =
		png_create_read_struct(
			PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
	if(!png_ptr) return;
	auto info_ptr = png_create_info_struct(png_ptr);
	if(!info_ptr){
		png_destroy_read_struct(&png_ptr, nullptr, nullptr);
		return;
	}
	png_init_io(png_ptr, fp);
	png_set_sig_bytes(png_ptr, 8);
	png_read_info(png_ptr, info_ptr);

	if(!setjmp(png_jmpbuf(png_ptr)))
	{
		png_uint_32 width, height;
		int bit_depth, color_type;
		if(
			png_get_IHDR(
				png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,
				nullptr, nullptr, nullptr)
			&& color_type == PNG_COLOR_TYPE_PALETTE && bit_depth <= 8)
		{
			*w = width;
			*h = height;
			out.resize(height * width);
			if (out.size() < (size_t)(height * width)) out.resize(height * width);
	auto p = out.data();
			auto pp = new png_bytep[height];
			for(int i = height-1; i >= 0; i--) pp[i] = p + width*i;
			png_read_image(png_ptr, pp);
			switch(bit_depth){
			case 1:
				for(uint32_t y = 0; y < height; y++){
					for(int i = width-1; i >= 0; i--){
						pp[y][i] = (pp[y][i>>3] & 1 << (i&7)) >> (i&7);
					}
				}
				break;
			case 2:
				for(uint32_t y = 0; y < height; y++){
					for(int i = width-1; i >= 0; i--){
						pp[y][i] = (pp[y][i>>2] & 3 << (i&3)*2) >> (i&3)*2;
					}
				}
				break;
			case 4:
				for(uint32_t y = 0; y < height; y++){
					for(int i = width-1; i >= 0; i--){
						pp[y][i] = (pp[y][i>>1] & 15 << (i&1)*4) >> (i&1)*4;
					}
				}
				break;
			}
			delete [] pp;
		}
	}
	png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
}

// ---------------------------------------------------------------------------
// Blit texture cache — avoids per-frame SDL_CreateTextureFromSurface
// ---------------------------------------------------------------------------
static const size_t BLIT_CACHE_MAX = 64;

struct BlitCacheEntry
{
	SDL_Texture* tex;
	uint64_t     lastFrame;
};

static std::map<SDL_Surface*, BlitCacheEntry> g_blitCache;

static void blitCacheEvict(SDL_Surface* key)
{
	std::map<SDL_Surface*, BlitCacheEntry>::iterator it = g_blitCache.find(key);
	if (it != g_blitCache.end())
	{
		SDL_DestroyTexture(it->second.tex);
		g_blitCache.erase(it);
	}
}

static void blitCacheClear()
{
	for (std::map<SDL_Surface*, BlitCacheEntry>::iterator it = g_blitCache.begin();
		 it != g_blitCache.end(); ++it)
	{
		SDL_DestroyTexture(it->second.tex);
	}
	g_blitCache.clear();
}

static void blitCacheTrim()
{
	while (g_blitCache.size() > BLIT_CACHE_MAX)
	{
		// Evict oldest entry
		std::map<SDL_Surface*, BlitCacheEntry>::iterator oldest = g_blitCache.begin();
		uint64_t oldestFrame = oldest->second.lastFrame;
		for (std::map<SDL_Surface*, BlitCacheEntry>::iterator it = g_blitCache.begin();
			 it != g_blitCache.end(); ++it)
		{
			if (it->second.lastFrame < oldestFrame)
			{
				oldest = it;
				oldestFrame = it->second.lastFrame;
			}
		}
		SDL_DestroyTexture(oldest->second.tex);
		g_blitCache.erase(oldest);
	}
}

void SSZ_STDCALL BlitSurface(SDL_Rect* prect, SDL_Surface* psrcs)
{
	SDL_Texture* tex;
	std::map<SDL_Surface*, BlitCacheEntry>::iterator it = g_blitCache.find(psrcs);
	if (it != g_blitCache.end())
	{
		it->second.lastFrame = g_perfCounters.totalFrames;
		tex = it->second.tex;
		g_perfCounters.blitCacheHits++;
	}
	else
	{
		tex = SDL_CreateTextureFromSurface(g_renderer, psrcs);
		BlitCacheEntry entry;
		entry.tex = tex;
		entry.lastFrame = g_perfCounters.totalFrames;
		g_blitCache[psrcs] = entry;
		blitCacheTrim();
		g_perfCounters.blitCacheMisses++;
	}
	SDL_RenderCopy(g_renderer, tex, nullptr, prect);
}

intptr_t SSZ_STDCALL CreatePaletteSurface(int32_t h, int32_t w, SDL_Color* ppl, uint8_t* ppx)
{
	SDL_Surface* psrc = SDL_CreateRGBSurfaceFrom(ppx, w, h, 8, w, 0, 0, 0, 0);
	SDL_SetPaletteColors(psrc->format->palette, ppl, 0, 256);
	SDL_Surface* pdst = SDL_ConvertSurface(psrc, psrc->format, SDL_SWSURFACE);
	SDL_FreeSurface(psrc);
	return (intptr_t)pdst;
}

void SSZ_STDCALL SetColorKey(uint32_t key, SDL_Surface* psur)
{
	SDL_SetColorKey(psur, key < 256, key);
}

void SSZ_STDCALL Flip()
{
	LARGE_INTEGER flipT0, flipT1;

	QueryPerformanceCounter(&flipT0);
	if(g_target)
	{
		unlockTarget();
		SDL_RenderCopy(g_renderer, g_target, nullptr, nullptr);
		lockTarget();
	}
	SDL_RenderPresent(g_renderer);
	QueryPerformanceCounter(&flipT1);
	g_perfCounters.flipTimeUs = qpcElapsedUs(flipT0, flipT1, g_perfCounters.qpcFreq);

	// End current frame perf tracking (includes flip time)
	if (g_perfMonitorEnabled)
	{
		uint32_t now = SDL_GetTicks();
		perfFrameEnd(g_perfCounters, now);
		perfPrintFrame(g_perfCounters, g_rendererInfo);
	}

	// Begin next frame perf tracking
	if (g_perfMonitorEnabled)
	{
		g_perfCounters.frameStartTick = SDL_GetTicks();
		perfFrameBegin(g_perfCounters);
	}
}

intptr_t SSZ_STDCALL AllocSurface(int32_t h, int32_t w)
{
	return
		(intptr_t)SDL_CreateRGBSurface(
			SDL_SWSURFACE, w, h, 32, 0x00FF0000,
			0x0000FF00, 0x000000FF, 0xFF000000);
}

void SSZ_STDCALL FreeSurface(SDL_Surface* ps)
{
	blitCacheEvict(ps);
	SDL_FreeSurface(ps);
}

void SSZ_STDCALL Delay(uint32_t ms)
{
	SDL_Delay(ms);
}

uint32_t SSZ_STDCALL GetTicks()
{
	return SDL_GetTicks();
}

void SSZ_STDCALL CursorShow(bool show)
{
	SDL_ShowCursor(show ? SDL_ENABLE : SDL_DISABLE);
}

intptr_t SSZ_STDCALL OpenFont(int32_t size, const std::wstring& font)
{
	TTF_Font* pf;
	pf = TTF_OpenFont(WstrToStr(font).c_str(), size);
	return (intptr_t)pf;
}

void SSZ_STDCALL CloseFont(TTF_Font* pf)
{
	textCacheEvictFont(pf);
	TTF_CloseFont(pf);
}

// ---------------------------------------------------------------------------
// Text texture cache — avoids per-frame TTF render + texture creation
// Key: (font pointer, text hash, packed color)
// ---------------------------------------------------------------------------
static const size_t TEXT_CACHE_MAX = 128;

static uint32_t djb2Hash(const wchar_t* str)
{
	uint32_t h = 5381;
	while (*str)
	{
		h = ((h << 5) + h) + (uint32_t)*str;
		++str;
	}
	return h;
}

struct TextCacheKey
{
	TTF_Font*  font;
	uint32_t   colorPack;
	uint32_t   textHash;

	bool operator<(const TextCacheKey& o) const
	{
		if (font != o.font) return font < o.font;
		if (colorPack != o.colorPack) return colorPack < o.colorPack;
		return textHash < o.textHash;
	}
};

struct TextCacheEntry
{
	SDL_Texture* tex;
	int          w, h;
	uint64_t     lastFrame;
};

static std::map<TextCacheKey, TextCacheEntry> g_textCache;

static void textCacheEvictFont(TTF_Font* font)
{
	std::map<TextCacheKey, TextCacheEntry>::iterator it = g_textCache.begin();
	while (it != g_textCache.end())
	{
		if (it->first.font == font)
		{
			SDL_DestroyTexture(it->second.tex);
			g_textCache.erase(it++);
		}
		else
		{
			++it;
		}
	}
}

static void textCacheClear()
{
	for (std::map<TextCacheKey, TextCacheEntry>::iterator it = g_textCache.begin();
		 it != g_textCache.end(); ++it)
	{
		SDL_DestroyTexture(it->second.tex);
	}
	g_textCache.clear();
}

static void textCacheTrim()
{
	while (g_textCache.size() > TEXT_CACHE_MAX)
	{
		std::map<TextCacheKey, TextCacheEntry>::iterator oldest = g_textCache.begin();
		uint64_t oldestFrame = oldest->second.lastFrame;
		for (std::map<TextCacheKey, TextCacheEntry>::iterator it = g_textCache.begin();
			 it != g_textCache.end(); ++it)
		{
			if (it->second.lastFrame < oldestFrame)
			{
				oldest = it;
				oldestFrame = it->second.lastFrame;
			}
		}
		SDL_DestroyTexture(oldest->second.tex);
		g_textCache.erase(oldest);
	}
}

void SSZ_STDCALL RenderFont(const std::wstring& str, int32_t y, int32_t x, SDL_Color c, TTF_Font* pf)
{
	std::wstring wstr = str;
	uint32_t colorPack = ((uint32_t)c.r << 24) | ((uint32_t)c.g << 16)
	                   | ((uint32_t)c.b << 8)  | (uint32_t)c.a;

	TextCacheKey key;
	key.font = pf;
	key.colorPack = colorPack;
	key.textHash = djb2Hash(wstr.c_str());

	SDL_Texture* tex;
	int tw, th;

	std::map<TextCacheKey, TextCacheEntry>::iterator it = g_textCache.find(key);
	if (it != g_textCache.end())
	{
		it->second.lastFrame = g_perfCounters.totalFrames;
		tex = it->second.tex;
		tw = it->second.w;
		th = it->second.h;
		g_perfCounters.textCacheHits++;
	}
	else
	{
		SDL_Surface* psrc =
			TTF_RenderUNICODE_Blended(
				pf, (Uint16*)wstr.c_str(), c);
		tw = psrc->w;
		th = psrc->h;
		tex = SDL_CreateTextureFromSurface(g_renderer, psrc);
		SDL_FreeSurface(psrc);

		TextCacheEntry entry;
		entry.tex = tex;
		entry.w = tw;
		entry.h = th;
		entry.lastFrame = g_perfCounters.totalFrames;
		g_textCache[key] = entry;
		textCacheTrim();
		g_perfCounters.textCacheMisses++;
	}

	SDL_Rect dest;
	dest.x = x;
	dest.y = y;
	dest.w = tw;
	dest.h = th;
	SDL_RenderCopy(g_renderer, tex, nullptr, &dest);
}

struct NormalizeVar
{
	static const double shitsu;
	double bai, heri, herihenka, fue, heikin;
	NormalizeVar() :
		bai(1.0), heri(1.0), herihenka(0.0), fue(1.0), heikin(1.0/shitsu)
	{
	}
};

const double NormalizeVar::shitsu = 32.0;
NormalizeVar g__nvAll;
double normalize(double sam, const int chs, const int sps, NormalizeVar& v)
{
	if(v.bai > 8.0) v.bai = 8.0;
	sam *= 1.0;
	if(sam < 0.0){
		if(sam < -1.0){
			v.bai *= pow(1.0/-sam, v.heri);
			v.herihenka += v.shitsu*(1.0 - v.heri) / ((double)sps+v.shitsu);
			sam = -1.0;
		}else{
			double tmp2 = (1.0 - pow(1.0 - -sam, 64.0)) * pow(0.5 - -sam, 3.0);
			v.bai += v.bai*(
				v.heri*(1.0/v.shitsu - v.heikin) / v.fue
				+ tmp2*v.fue*(1.0 - v.heri) / v.shitsu
			) / (double)(chs*sps/8+1);
			v.herihenka -= (0.5 - v.heikin)*v.heri / (double)(chs*sps);
		}
		v.fue +=
			(v.shitsu*v.fue*(1.0/v.fue - -sam) - v.fue)
			/ (v.shitsu*(double)(chs*sps));
		v.heikin += (-sam - v.heikin) / (double)(chs*sps);
	}else{
		if(sam > 1.0){
			v.bai *= pow(1.0/sam, v.heri);
			v.herihenka += v.shitsu*(1.0 - v.heri) / ((double)sps+v.shitsu);
			sam = 1.0;
		}else{
			double tmp2 = (1.0 - pow(1.0 - sam, 64.0)) * pow(0.5 - sam, 3.0);
			v.bai += v.bai*(
				v.heri*(1.0/v.shitsu - v.heikin) / v.fue
				+ tmp2*v.fue*(1.0 - v.heri) / v.shitsu
			) / (double)(chs*sps/8+1);
			v.herihenka -= (0.5 - v.heikin)*v.heri / (double)(chs*sps);
		}
		v.fue +=
			(v.shitsu*v.fue*(1.0/v.fue - sam) - v.fue)
			/ (v.shitsu*(double)(chs*sps));
		v.heikin += (sam - v.heikin) / (double)(chs*sps);
	}
	v.heri += v.herihenka;
	if(v.heri < 0.0) v.heri = 0.0;
	else if(v.heri > 1.0) v.heri = 1.0;
	return sam;
}

// ---------------------------------------------------------------------------
// SetSndBuf – SFX only. The script mixes all Wave channels into `buf`,
// then this function normalises and copies into the callback buffer.
// BGM is handled entirely by SDL_mixer on its own device.
// ---------------------------------------------------------------------------
bool SSZ_STDCALL SetSndBuf(int32_t* buf)
{
	if(g_snddata == g_sndbuf) return false;
	for(int i = 0; i < g_samples*2; i++){
		g_sndbuf[i] =
			(int16_t)(
				normalize(
					(double)(buf[i] / 2 * wav_vol) / 32768.0,
					2, 44100, g__nvAll)
				* 32767.0
				* g_vol);
	}
	g_snddata = g_sndbuf;
	return true;
}

// ---------------------------------------------------------------------------
// PlayBGM – loads and plays any format SDL_mixer supports:
//   WAV, MP3, OGG Vorbis, FLAC, MOD/S3M/XM/IT, MIDI, …
// The second parameter (pldir) is kept for ABI compat but ignored.
// ---------------------------------------------------------------------------
bool SSZ_STDCALL PlayBGM(const std::wstring& fn, const std::wstring& pldir)
{
	LOG_DEBUG("SDL", "PlayBGM: Starting");
	bgmclear();
	if(fn.empty()){
		LOG_DEBUG("SDL", "PlayBGM: Empty filename, returning true");
		return true;
	}
	// Convert wide filename to multibyte
	std::string fname = WstrToStr(fn);
	// Resolve absolute path
	char* pc = _fullpath(nullptr, fname.c_str(), 0);
	if(pc == nullptr){
		LOG_DEBUG("SDL", "PlayBGM: Failed to resolve full path for: %s", fname.c_str());
		return false;
	}
	fname = pc;
	free(pc);
	LOG_DEBUG("SDL", "PlayBGM: Full path: %s", fname.c_str());
	// Load and play via SDL_mixer
	MEM_MARK_BEFORE_NAMED(MUSIC, fname.c_str());
	g_bgmMusic = Mix_LoadMUS(fname.c_str());
	if(g_bgmMusic == nullptr){
		LOG_DEBUG("SDL", "PlayBGM: Mix_LoadMUS failed: %s", Mix_GetError());
		MEM_MARK_AFTER_NAMED(MUSIC, fname.c_str());
		return false;
	}
	// Apply current BGM volume before playback
	Mix_VolumeMusic((int)(bgm_vol * g_vol * MIX_MAX_VOLUME));
	if(Mix_PlayMusic(g_bgmMusic, -1) < 0){
		LOG_DEBUG("SDL", "PlayBGM: Mix_PlayMusic failed: %s", Mix_GetError());
		Mix_FreeMusic(g_bgmMusic);
		g_bgmMusic = nullptr;
		MEM_MARK_AFTER_NAMED(MUSIC, fname.c_str());
		return false;
	}
	bgm_paused = false;
	LOG_DEBUG("SDL", "PlayBGM: Successfully started playback");
	MEM_MARK_AFTER_NAMED(MUSIC, fname.c_str());
	return true;
}

void SSZ_STDCALL PauseBGM(bool pause)
{
	if(pause == bgm_paused) return;
	bgm_paused = pause;
	if(g_bgmMusic != nullptr){
		if(pause){
			Mix_PauseMusic();
			LOG_DEBUG("SDL", "PauseBGM: Music paused");
		}else{
			Mix_ResumeMusic();
			LOG_DEBUG("SDL", "PauseBGM: Music resumed");
		}
	}
}

// Legacy SendOpen/Write/Close kept as no-ops for ABI compatibility.
// OGG streaming is now handled by PlayBGM via SDL_mixer.
bool SSZ_STDCALL SendOpenBGM(int32_t channels, int32_t rate)
{
	LOG_DEBUG("SDL", "SendOpenBGM: deprecated (SDL_mixer handles all BGM)");
	return false;
}

void SSZ_STDCALL SendCloseBGM()
{
	LOG_DEBUG("SDL", "SendCloseBGM: deprecated");
}

intptr_t SSZ_STDCALL SendWriteBGM()
{
	LOG_DEBUG("SDL", "SendWriteBGM: deprecated");
	return 0;
}

void SSZ_STDCALL SetVolume(float bv, float wv, float gv)
{
	bgm_vol = bv;
	wav_vol = wv;
	g_vol   = gv;
	// Clamp BGM volume
	if(bgm_vol < 0.0f) bgm_vol = 0.0f;
	else if(bgm_vol > 1.0f) bgm_vol = 1.0f;
	// Clamp SFX volume
	if(wav_vol < 0.0f) wav_vol = 0.0f;
	else if(wav_vol > 1.0f) wav_vol = 1.0f;
	// Clamp master volume
	if(g_vol < 0.0f) g_vol = 0.0f;
	else if(g_vol > 1.0f) g_vol = 1.0f;
	// Push combined BGM volume to SDL_mixer
	int mixVol = (int)(bgm_vol * g_vol * MIX_MAX_VOLUME);
	Mix_VolumeMusic(mixVol);
	// LOG_DEBUG("SDL", "SetVolume: bgm=%.2f wav=%.2f master=%.2f -> mixer=%d",
	// 	bgm_vol, wav_vol, g_vol, mixVol);
}

void SSZ_STDCALL FadeInBGM(int time)
{
	if(g_bgmMusic == nullptr){
		LOG_DEBUG("SDL", "FadeInBGM: No music loaded");
		return;
	}
	int targetVol = (int)(bgm_vol * g_vol * MIX_MAX_VOLUME);
	LOG_DEBUG("SDL", "FadeInBGM: fading in over %d ms to volume %d", time, targetVol);
	Mix_VolumeMusic(0);
	Mix_FadeInMusic(g_bgmMusic, -1, time);
	Mix_VolumeMusic(targetVol);
}

void SSZ_STDCALL FadeOutBGM(int time)
{
	if(g_bgmMusic == nullptr){
		LOG_DEBUG("SDL", "FadeOutBGM: No music loaded");
		return;
	}
	LOG_DEBUG("SDL", "FadeOutBGM: fading out over %d ms", time);
	Mix_FadeOutMusic(time);
}

void kaiten(float& x, float& y, float angle, float rcx, float rcy, float vscl)
{
	float temp = (y - rcy) / vscl;
	float length = sqrt((x - rcx)*(x - rcx) + temp*temp);
	if(x - rcx == 0.0f){
		angle += (float)(y - rcy > 0.0f ? (float)PI/2.0f : -(float)PI/2.0f);
		x = rcx + (float)(length*cos(angle));
		y = rcy + (float)(length*sin(angle)) * vscl;
		return;
	}
	double kakudo =
		atan(temp / (x - rcx)) + (x - rcx < 0 ? (float)PI : 0.0f) + angle;
	x = rcx + (float)(length*cos(kakudo));
	y = rcy + (float)(length*sin(kakudo)) * vscl;
}

struct PalletColorImg
{
	uint8_t* data;
	uint8_t* end;
	int currentx;
	int currenty;
	int width;
	int color;
	void setImg(Reference& r, int w)
	{
		data = (uint8_t*)r.atpos();
		end = data + r.len();
		width = w;
		currentx = width-1;
		currenty = -1;
		nextPixel();
	}
	void nextPixel()
	{
		if((unsigned int)++currentx >= (unsigned int)width){
			currenty++;
			if(data >= end){
				end = nullptr;
				currentx = -2;
				return;
			}
			currentx = 0;
		}
		color = *data++;
	}
	bool finished()
	{
		return end == nullptr;
	}
	void skip(int n)
	{
		while(n > 0 && !finished()){
			if(width-currentx  > n){
				data += n;
				currentx += n;
				color = *(data-1);
				return;
			}
			n -= width - currentx;
			data += (width-1)-currentx;
			currentx = width-1;
			nextPixel();
		}
	}
	void nextLine()
	{
		if(finished()) return;
		data += (width-1)-currentx;
		currentx = width-1;
		nextPixel();
	}
};

struct PcxRleImg
{
	struct LineInfo
	{
		uint8_t* data;
	};
	uint8_t* data;
	uint8_t* end;
	int currentx;
	int currenty;
	int width;
	int datawidth;
	int color;
	int restcount;
	Reference* nlbuf;
	void setImg(Reference& r, int w, int dw, Reference* b)
	{
		data = (uint8_t*)r.atpos();
		end = data + r.len();
		width = w;
		datawidth = (std::max)(width, dw);
		nlbuf = b;
		restcount = 0;
		currentx = datawidth-1;
		currenty = -1;
		nextPixel();
	}
	void forward1()
	{
		restcount--;
		while(restcount <= 0){
			color = *data++;
			if(color >= 0xC0){
				restcount = color & 0x3F;
				color = *data++;
			}else{
				restcount = 1;
				break;
			}
		}
	}
	void nextPixel()
	{
		if((unsigned int)++currentx >= (unsigned int)width){
			if(!newLine()) return;
		}
		forward1();
	}
	bool newLine()
	{
		while((unsigned int)currentx < (unsigned int)datawidth){
			currentx++;
			forward1();
		}
		currenty++;
		currentx = 0;
		restcount = 0;
		if(data >= end){
			end = nullptr;
			currentx = -2;
			return false;
		}
		return true;
	}
	bool finished()
	{
		return end == nullptr;
	}
	void skip(int n)
	{
		while(n > 0 && !finished()){
			if(datawidth-currentx  > restcount){
				if(n < restcount){
					currentx += n;
					restcount -= n;
					return;
				}
				n -= restcount;
				currentx += restcount-1;
				restcount = 1;
			}else if((datawidth-1) - currentx < restcount){
				if(n < datawidth - currentx){
					currentx += n;
					restcount -= n;
					return;
				}
				n -= datawidth - currentx;
				restcount -= (datawidth-1) - currentx;
				currentx = datawidth-1;
			}else{
				n--;
			}
			nextPixel();
		}
	}
	void nextLine()
	{
		if(currenty < nlbuf->len()/(int)sizeof(LineInfo)){
			data = ((LineInfo*)nlbuf->atpos())[currenty].data;
			currenty++;
			currentx = 0;
			restcount = 0;
			if(data >= end){
				end = nullptr;
				currentx = -2;
			}else{
				forward1();
			}
			return;
		}
		if(finished()) return;
		skip(width - 1 - currentx);
		currentx++;
		if(!newLine()) return;
		if(currenty-1 == nlbuf->len()/(int)sizeof(LineInfo)){
			nlbuf->addsize(1, sizeof(LineInfo), nullptr, nullptr);
			((LineInfo*)nlbuf->atpos())[currenty-1].data = data;
		}
		forward1();
	}
};

#include "rz.h"
typedef void (*copycolorproc)(uint32_t&, uint32_t, uint32_t);
template<typename Img> class Funcs
{
public:
	typedef void (*mzllporc)(
		uint32_t*, Img&, uint32_t*, uint32_t,
		int, SDL_Rect, int, int, int, int, int);
	typedef void (*mrllporc)(
		uint32_t*, int, int, int, Img, uint32_t*, uint32_t,
		bool, uint32_t, int, int, int, int, int, int, int, int);
	typedef void (*mzlslporc)(
		uint32_t*, Img&, uint32_t, uint32_t,
		int, int, int, int, int, int);
	typedef void (*mrlslporc)(
		uint32_t*, int, int, int, Img, uint32_t, uint32_t,
		bool, uint32_t, int, int, int, int, int, int, int, int, int);
};

void mTrans(uint32_t& dst, uint32_t color, uint32_t colorkey)
{
	dst = color;
}

void mAddTrans(uint32_t& dst, uint32_t color, uint32_t colorkey)
{
	uint32_t tmp =
		((dst & color) + (((dst ^ color) >> 1) & 0x7f7f7f7f)) & 0x80808080;
	uint32_t msk = (tmp << 1) - (tmp >> 7);
	dst = ((dst + color) - msk) | msk;
}

void mAdd1Trans(uint32_t& dst, uint32_t color, uint32_t colorkey)
{
	uint32_t tmpm = (1 << (8 - (colorkey >> 16))) - 1;
	tmpm |= tmpm << 8 | tmpm << 16;
	uint32_t tmpd = dst >> (colorkey >> 16) & tmpm;
	uint32_t tmp =
		((tmpd & color) + (((tmpd ^ color) >> 1) & 0x7f7f7f7f)) & 0x80808080;
	uint32_t msk = (tmp << 1) - (tmp >> 7);
	dst = ((tmpd + color) - msk) | msk;
}

void mSubTrans(uint32_t& dst, uint32_t color, uint32_t colorkey)
{
	uint32_t tmp =
		(((~dst & color) << 1) + ((~dst ^ color) & 0xfefefefe)) & 0x01010100;
	uint32_t msk = tmp - (tmp >> 8);
	dst = (dst - color + tmp) & ~msk;
}

void mAlphaTrans(uint32_t& dst, uint32_t color, uint32_t colorkey)
{
	uint64_t tmpd =
		((uint64_t)(dst&0xff0000) << 16)
		| ((uint64_t)(dst&0xff00) << 8) | (uint64_t)(dst&0xff);
	uint64_t tmps =
		((uint64_t)(color&0xff0000) << 16)
		| ((uint64_t)(color&0xff00) << 8) | (uint64_t)(color&0xff);
	tmpd *= colorkey >> 24;
	tmps *= colorkey >> 16 & 0xff;
	uint64_t tmp =
		((tmpd & tmps) + (((tmpd ^ tmps) >> 1) & 0x7fff7fff7fff7fffL))
		& 0x8000800080008000L;
	uint64_t msk = (tmp << 1) - (tmp >> 15);
	tmpd = ((tmpd + tmps) - msk) | msk;
	dst =
		(uint32_t)((tmpd&0xff0000000000L)>>24
		| (tmpd&0xff000000L)>>16 | (tmpd&0xff00L)>>8);
}

void mShadowTrans(uint32_t& dst, uint32_t color, uint32_t alpha)
{
	mSubTrans(dst, color, 0);
	uint64_t tmpd =
		((uint64_t)(dst&0xff0000) << 16)
		| ((uint64_t)(dst&0xff00) << 8) | (uint64_t)(dst&0xff);
	tmpd *= alpha;
	dst =
		(uint32_t)((tmpd&0xff0000000000L)>>24
		| (tmpd&0xff000000L)>>16 | (tmpd&0xff00L)>>8);
}

template<typename Img, copycolorproc ccp> void mzlLoop(
	uint32_t* pdpx, Img& pri, uint32_t* pspl, uint32_t colorkey,
	int xsign, SDL_Rect tile, int ix, int dxend, int ifx, int ixcl, int sx)
{
	Img tmppri = pri;
	tmppri.skip(sx);
	if(tile.w == 1){
		if(
			xsign*dxend
			> xsign*((ifx + ((tmppri.width-1) - tmppri.currentx)*ixcl) >> 16))
		{
			if(sx != 0 && tmppri.currentx <= 0) return;
			if(-65536 <= ixcl && ixcl <= 65536){
				for(;;){
					int n = (int)(ix == ifx>>16) - 1 & xsign;
					if(n != 0 && (uint16_t)colorkey != tmppri.color){
						ccp(pdpx[ix], pspl[tmppri.color], colorkey);
					}
					ix += n;
					tmppri.nextPixel();
					if(tmppri.currentx <= 0) return;
					ifx += ixcl;
				}
			}else{
				for(;;){
					if(ix == ifx>>16){
						tmppri.nextPixel();
						if(tmppri.currentx <= 0) return;
						ifx += ixcl;
					}
					if(((uint16_t)colorkey != tmppri.color)){
						ccp(pdpx[ix], pspl[tmppri.color], colorkey);
					}
					ix += xsign;
				}
			}
		}else{
			if(-65536 <= ixcl && ixcl <= 65536){
				for(;;){
					int n = (int)(ix == ifx>>16) - 1 & xsign;
					if(n != 0 && (uint16_t)colorkey != tmppri.color){
						ccp(pdpx[ix], pspl[tmppri.color], colorkey);
					}
					if((ix += n) == dxend) return;
					tmppri.nextPixel();
					ifx += ixcl;
				}
			}else{
				for(;;){
					if(ix == ifx>>16){
						tmppri.nextPixel();
						ifx += ixcl;
					}
					if(((uint16_t)colorkey != tmppri.color)){
						ccp(pdpx[ix], pspl[tmppri.color], colorkey);
					}
					if((ix += xsign) == dxend) return;
				}
			}
		}
	}else{
		int bix = ix;
		if(-65536 <= ixcl && ixcl <= 65536){
			for(;;){
				int n = (int)(ix == ifx>>16) - 1 & xsign;
				if(n != 0 && (uint16_t)colorkey != tmppri.color){
					ccp(pdpx[ix], pspl[tmppri.color], colorkey);
				}
				if((ix += n) == dxend) return;
				tmppri.nextPixel();
				if(tmppri.currentx <= 0){
					ifx += ixcl*tile.x;
					ix = ifx>>16;
					if(xsign*ix < xsign*bix) for(;;){
						ix = (ifx+ixcl)>>16;
						if(xsign*ix >= xsign*bix) break;
						ifx += ixcl;
					}
					if(--tile.w == 0 || xsign*ix >= xsign*dxend) return;
					tmppri = pri;
				}
				ifx += ixcl;
			}
		}else{
			for(;;){
				if(ix == ifx>>16){
					tmppri.nextPixel();
					if(tmppri.currentx <= 0){
						ifx += ixcl*tile.x;
						ix = ifx>>16;
						if(xsign*ix < xsign*bix) for(;;){
							ix = (ifx+ixcl)>>16;
							if(xsign*ix >= xsign*bix) break;
							ifx += ixcl;
						}
						if(--tile.w == 0 || xsign*ix >= xsign*dxend) return;
						tmppri = pri;
					}
					ifx += ixcl;
				}
				if(((uint16_t)colorkey != tmppri.color)){
					ccp(pdpx[ix], pspl[tmppri.color], colorkey);
				}
				if((ix += xsign) == dxend) return;
			}
		}
	}
}

// Priority 2: fast specialization for PalletColorImg + mTrans (opaque).
// Replaces per-pixel nextPixel() overhead with direct uint8_t* access.
// tile.w==1 (no tiling) uses a tight loop; tiling falls back with hardcoded write.
template<> void mzlLoop<PalletColorImg, mTrans>(
	uint32_t* pdpx, PalletColorImg& pri, uint32_t* pspl, uint32_t colorkey,
	int xsign, SDL_Rect tile, int ix, int dxend, int ifx, int ixcl, int sx)
{
	PalletColorImg tmppri = pri;
	tmppri.skip(sx);
	uint16_t ck = (uint16_t)colorkey;
	if(tile.w == 1){
		// Fast path: direct pointer, no nextPixel() call per pixel
		if(sx != 0 && tmppri.currentx <= 0) return;
		uint8_t* srcPtr = tmppri.data - 1; // points to current source pixel
		int srcRem = tmppri.width - tmppri.currentx;
		if(-65536 <= ixcl && ixcl <= 65536){
			// SSE2 fast path: ixcl==65536 means 1:1 scale, n is provably always 1.
			// Process 4 dest pixels per iteration: palette expand + optional mask + store.
			if(ixcl == 65536 && xsign == 1 && ix < dxend){
				int count = srcRem < (dxend - ix) ? srcRem : (dxend - ix);
				int i = 0;
				for(; i + 4 <= count; i += 4){
					uint8_t b0 = srcPtr[i], b1 = srcPtr[i+1];
					uint8_t b2 = srcPtr[i+2], b3 = srcPtr[i+3];
					// Load dest for colorkey-preserving merge
					__m128i d = _mm_loadu_si128((const __m128i*)(pdpx + ix + i));
					uint32_t dArr[4]; _mm_storeu_si128((__m128i*)dArr, d);
					uint32_t r0 = (b0 != ck) ? pspl[b0] : dArr[0];
					uint32_t r1 = (b1 != ck) ? pspl[b1] : dArr[1];
					uint32_t r2 = (b2 != ck) ? pspl[b2] : dArr[2];
					uint32_t r3 = (b3 != ck) ? pspl[b3] : dArr[3];
					_mm_storeu_si128((__m128i*)(pdpx + ix + i),
						_mm_setr_epi32((int)r0, (int)r1, (int)r2, (int)r3));
				}
				for(; i < count; i++){
					uint8_t c = srcPtr[i];
					if(c != ck) pdpx[ix + i] = pspl[c];
				}
				return;
			}
			// Source-driven (1:1 with non-unit x or downscale)
			for(; srcRem > 0; srcRem--, ifx += ixcl){
				int n = (int)(ix == (ifx >> 16)) - 1 & xsign;
				uint8_t c = *srcPtr++;
				if(n != 0 && c != ck)
					pdpx[ix] = pspl[c];
				ix += n;
				if(ix == dxend) return;
			}
		}else{
			// Dest-driven (upscale)
			uint8_t cur = *srcPtr++;
			srcRem--;
			for(; ix != dxend; ix += xsign){
				if(ix == (ifx >> 16)){
					ifx += ixcl;
					if(srcRem <= 0) return;
					cur = *srcPtr++;
					srcRem--;
				}
				if(cur != ck)
					pdpx[ix] = pspl[cur];
			}
		}
	}else{
		// Tiling: keep nextPixel() for row tracking, hardcode write
		int bix = ix;
		if(-65536 <= ixcl && ixcl <= 65536){
			for(;;){
				int n = (int)(ix == ifx>>16) - 1 & xsign;
				if(n != 0 && ck != (uint16_t)tmppri.color)
					pdpx[ix] = pspl[tmppri.color];
				if((ix += n) == dxend) return;
				tmppri.nextPixel();
				if(tmppri.currentx <= 0){
					ifx += ixcl*tile.x;
					ix = ifx>>16;
					if(xsign*ix < xsign*bix) for(;;){
						ix = (ifx+ixcl)>>16;
						if(xsign*ix >= xsign*bix) break;
						ifx += ixcl;
					}
					if(--tile.w == 0 || xsign*ix >= xsign*dxend) return;
					tmppri = pri;
				}
				ifx += ixcl;
			}
		}else{
			for(;;){
				if(ix == ifx>>16){
					tmppri.nextPixel();
					if(tmppri.currentx <= 0){
						ifx += ixcl*tile.x;
						ix = ifx>>16;
						if(xsign*ix < xsign*bix) for(;;){
							ix = (ifx+ixcl)>>16;
							if(xsign*ix >= xsign*bix) break;
							ifx += ixcl;
						}
						if(--tile.w == 0 || xsign*ix >= xsign*dxend) return;
						tmppri = pri;
					}
					ifx += ixcl;
				}
				if(ck != (uint16_t)tmppri.color)
					pdpx[ix] = pspl[tmppri.color];
				if((ix += xsign) == dxend) return;
			}
		}
	}
}

// Priority 2: fast specialization for PalletColorImg + mAddTrans (additive blend).
template<> void mzlLoop<PalletColorImg, mAddTrans>(
	uint32_t* pdpx, PalletColorImg& pri, uint32_t* pspl, uint32_t colorkey,
	int xsign, SDL_Rect tile, int ix, int dxend, int ifx, int ixcl, int sx)
{
	PalletColorImg tmppri = pri;
	tmppri.skip(sx);
	uint16_t ck = (uint16_t)colorkey;
	if(tile.w == 1){
		if(sx != 0 && tmppri.currentx <= 0) return;
		uint8_t* srcPtr = tmppri.data - 1;
		int srcRem = tmppri.width - tmppri.currentx;
		if(-65536 <= ixcl && ixcl <= 65536){
			// SSE2 fast path: ixcl==65536 means 1:1 scale, n is provably always 1.
			// _mm_adds_epu8 = single PADDUSB instruction = correct saturating add.
			if(ixcl == 65536 && xsign == 1 && ix < dxend){
				int count = srcRem < (dxend - ix) ? srcRem : (dxend - ix);
				int i = 0;
				for(; i + 4 <= count; i += 4){
					uint8_t b0 = srcPtr[i], b1 = srcPtr[i+1];
					uint8_t b2 = srcPtr[i+2], b3 = srcPtr[i+3];
					// Zero out colorkey palette entries so adding 0 = no effect
					uint32_t c0 = (b0 != ck) ? pspl[b0] : 0;
					uint32_t c1 = (b1 != ck) ? pspl[b1] : 0;
					uint32_t c2 = (b2 != ck) ? pspl[b2] : 0;
					uint32_t c3 = (b3 != ck) ? pspl[b3] : 0;
					__m128i s = _mm_setr_epi32((int)c0, (int)c1, (int)c2, (int)c3);
					__m128i d = _mm_loadu_si128((const __m128i*)(pdpx + ix + i));
					// PADDUSB: per-channel saturating add, correct additive blend
					_mm_storeu_si128((__m128i*)(pdpx + ix + i), _mm_adds_epu8(d, s));
				}
				for(; i < count; i++){
					uint8_t c = srcPtr[i];
					if(c != ck){
						uint32_t color = pspl[c];
						uint32_t& dst = pdpx[ix + i];
						uint32_t tmp = ((dst & color) + (((dst ^ color) >> 1) & 0x7f7f7f7f)) & 0x80808080;
						uint32_t msk = (tmp << 1) - (tmp >> 7);
						dst = ((dst + color) - msk) | msk;
					}
				}
				return;
			}
			for(; srcRem > 0; srcRem--, ifx += ixcl){
				int n = (int)(ix == (ifx >> 16)) - 1 & xsign;
				uint8_t c = *srcPtr++;
				if(n != 0 && c != ck){
					uint32_t color = pspl[c];
					uint32_t& dst = pdpx[ix];
					uint32_t tmp = ((dst & color) + (((dst ^ color) >> 1) & 0x7f7f7f7f)) & 0x80808080;
					uint32_t msk = (tmp << 1) - (tmp >> 7);
					dst = ((dst + color) - msk) | msk;
				}
				ix += n;
				if(ix == dxend) return;
			}
		}else{
			uint8_t cur = *srcPtr++;
			srcRem--;
			for(; ix != dxend; ix += xsign){
				if(ix == (ifx >> 16)){
					ifx += ixcl;
					if(srcRem <= 0) return;
					cur = *srcPtr++;
					srcRem--;
				}
				if(cur != ck){
					uint32_t color = pspl[cur];
					uint32_t& dst = pdpx[ix];
					uint32_t tmp = ((dst & color) + (((dst ^ color) >> 1) & 0x7f7f7f7f)) & 0x80808080;
					uint32_t msk = (tmp << 1) - (tmp >> 7);
					dst = ((dst + color) - msk) | msk;
				}
			}
		}
	}else{
		int bix = ix;
		if(-65536 <= ixcl && ixcl <= 65536){
			// SSE2 segment fast path: 1:1 forward scale, tiled.
			// Each tile is a contiguous source byte run; process with PADDUSB, then skip
			// tile.x gap pixels and reset source to pri for the next repetition.
			if(ixcl == 65536 && xsign == 1 && ix < dxend){
				for(;;){
					uint8_t* srcPtr = tmppri.data - 1;
					int segLen = tmppri.width - tmppri.currentx;
					int dstRem = dxend - ix;
					int count = (segLen < dstRem) ? segLen : dstRem;
					int i = 0;
					for(; i + 4 <= count; i += 4){
						uint8_t b0 = srcPtr[i], b1 = srcPtr[i+1];
						uint8_t b2 = srcPtr[i+2], b3 = srcPtr[i+3];
						uint32_t c0 = (b0 != ck) ? pspl[b0] : 0;
						uint32_t c1 = (b1 != ck) ? pspl[b1] : 0;
						uint32_t c2 = (b2 != ck) ? pspl[b2] : 0;
						uint32_t c3 = (b3 != ck) ? pspl[b3] : 0;
						__m128i s = _mm_setr_epi32((int)c0, (int)c1, (int)c2, (int)c3);
						__m128i d = _mm_loadu_si128((const __m128i*)(pdpx + ix + i));
						_mm_storeu_si128((__m128i*)(pdpx + ix + i), _mm_adds_epu8(d, s));
					}
					for(; i < count; i++){
						uint8_t c = srcPtr[i];
						if(c != ck){
							uint32_t color = pspl[c];
							uint32_t& dst = pdpx[ix + i];
							uint32_t tmp = ((dst & color) + (((dst ^ color) >> 1) & 0x7f7f7f7f)) & 0x80808080;
							uint32_t msk = (tmp << 1) - (tmp >> 7);
							dst = ((dst + color) - msk) | msk;
						}
					}
					ix += count;
					if(ix >= dxend) return;
					// Skip inter-tile gap and advance to next tile repetition
					ix += tile.x;
					if(ix >= dxend || --tile.w == 0) return;
					tmppri = pri;  // Reset source to pixel[0] for full-width tile
				}
			}
			for(;;){
				int n = (int)(ix == ifx>>16) - 1 & xsign;
				if(n != 0 && ck != (uint16_t)tmppri.color){
					uint32_t color = pspl[tmppri.color];
					uint32_t& dst = pdpx[ix];
					uint32_t tmp = ((dst & color) + (((dst ^ color) >> 1) & 0x7f7f7f7f)) & 0x80808080;
					uint32_t msk = (tmp << 1) - (tmp >> 7);
					dst = ((dst + color) - msk) | msk;
				}
				if((ix += n) == dxend) return;
				tmppri.nextPixel();
				if(tmppri.currentx <= 0){
					ifx += ixcl*tile.x;
					ix = ifx>>16;
					if(xsign*ix < xsign*bix) for(;;){
						ix = (ifx+ixcl)>>16;
						if(xsign*ix >= xsign*bix) break;
						ifx += ixcl;
					}
					if(--tile.w == 0 || xsign*ix >= xsign*dxend) return;
					tmppri = pri;
				}
				ifx += ixcl;
			}
		}else{
			// Run-expansion fast path for zoom-in (ixcl > 65536), forward xsign==1.
			// Iterate source pixels directly and expand each source pixel to its dest run,
			// avoiding per-dest-pixel nextPixel() overhead in Debug builds.
			if(xsign == 1 && ix < dxend){
				uint8_t* src0 = pri.data - 1;
				uint8_t* src = tmppri.data - 1;
				int srcRem = tmppri.width - tmppri.currentx;
				uint8_t cur = src[0];
				int si = 1;
				for(;;){
					for(;;){
						int runEnd = ifx >> 16;
						if(runEnd > dxend) runEnd = dxend;
						int run = runEnd - ix;
						if(run > 0 && cur != ck){
							uint32_t color = pspl[cur];
							int j = 0;
							if(run >= 4){
								__m128i vs = _mm_set1_epi32((int)color);
								for(; j + 4 <= run; j += 4){
									__m128i d = _mm_loadu_si128((const __m128i*)(pdpx + ix + j));
									_mm_storeu_si128((__m128i*)(pdpx + ix + j), _mm_adds_epu8(d, vs));
								}
							}
							for(; j < run; j++){
								uint32_t& dst = pdpx[ix + j];
								uint32_t tmp = ((dst & color) + (((dst ^ color) >> 1) & 0x7f7f7f7f)) & 0x80808080;
								uint32_t msk = (tmp << 1) - (tmp >> 7);
								dst = ((dst + color) - msk) | msk;
							}
						}
						ix = runEnd;
						srcRem--;
						if(srcRem == 0) break;
						ifx += ixcl;
						cur = src[si++];
						if(ix >= dxend) return;
					}
					if(ix >= dxend) return;
					ifx += ixcl * tile.x;
					ix = ifx >> 16;
					if(ix >= dxend || --tile.w == 0) return;
					src = src0;
					srcRem = pri.width;
					cur = src[0];
					si = 1;
					ifx += ixcl;
				}
			}
			for(;;){
				if(ix == ifx>>16){
					tmppri.nextPixel();
					if(tmppri.currentx <= 0){
						ifx += ixcl*tile.x;
						ix = ifx>>16;
						if(xsign*ix < xsign*bix) for(;;){
							ix = (ifx+ixcl)>>16;
							if(xsign*ix >= xsign*bix) break;
							ifx += ixcl;
						}
						if(--tile.w == 0 || xsign*ix >= xsign*dxend) return;
						tmppri = pri;
					}
					ifx += ixcl;
				}
				if(ck != (uint16_t)tmppri.color){
					uint32_t color = pspl[tmppri.color];
					uint32_t& dst = pdpx[ix];
					uint32_t tmp = ((dst & color) + (((dst ^ color) >> 1) & 0x7f7f7f7f)) & 0x80808080;
					uint32_t msk = (tmp << 1) - (tmp >> 7);
					dst = ((dst + color) - msk) | msk;
				}
				if((ix += xsign) == dxend) return;
			}
		}
	}
}

// Priority 2: fast specialization for PalletColorImg + mSubTrans.
template<> void mzlLoop<PalletColorImg, mSubTrans>(
	uint32_t* pdpx, PalletColorImg& pri, uint32_t* pspl, uint32_t colorkey,
	int xsign, SDL_Rect tile, int ix, int dxend, int ifx, int ixcl, int sx)
{
	PalletColorImg tmppri = pri;
	tmppri.skip(sx);
	uint16_t ck = (uint16_t)colorkey;
	if(tile.w == 1){
		if(sx != 0 && tmppri.currentx <= 0) return;
		uint8_t* srcPtr = tmppri.data - 1;
		int srcRem = tmppri.width - tmppri.currentx;
		if(-65536 <= ixcl && ixcl <= 65536){
			for(; srcRem > 0; srcRem--, ifx += ixcl){
				int n = (int)(ix == (ifx >> 16)) - 1 & xsign;
				uint8_t c = *srcPtr++;
				if(n != 0 && c != ck){
					uint32_t color = pspl[c];
					uint32_t& dst = pdpx[ix];
					uint32_t tmp = (((~dst & color) << 1) + ((~dst ^ color) & 0xfefefefe)) & 0x01010100;
					uint32_t msk = tmp - (tmp >> 8);
					dst = (dst - color + tmp) & ~msk;
				}
				ix += n;
				if(ix == dxend) return;
			}
		}else{
			uint8_t cur = *srcPtr++;
			srcRem--;
			for(; ix != dxend; ix += xsign){
				if(ix == (ifx >> 16)){
					ifx += ixcl;
					if(srcRem <= 0) return;
					cur = *srcPtr++;
					srcRem--;
				}
				if(cur != ck){
					uint32_t color = pspl[cur];
					uint32_t& dst = pdpx[ix];
					uint32_t tmp = (((~dst & color) << 1) + ((~dst ^ color) & 0xfefefefe)) & 0x01010100;
					uint32_t msk = tmp - (tmp >> 8);
					dst = (dst - color + tmp) & ~msk;
				}
			}
		}
	}else{
		int bix = ix;
		if(-65536 <= ixcl && ixcl <= 65536){
			for(;;){
				int n = (int)(ix == ifx>>16) - 1 & xsign;
				if(n != 0 && ck != (uint16_t)tmppri.color){
					uint32_t color = pspl[tmppri.color];
					uint32_t& dst = pdpx[ix];
					uint32_t tmp = (((~dst & color) << 1) + ((~dst ^ color) & 0xfefefefe)) & 0x01010100;
					uint32_t msk = tmp - (tmp >> 8);
					dst = (dst - color + tmp) & ~msk;
				}
				if((ix += n) == dxend) return;
				tmppri.nextPixel();
				if(tmppri.currentx <= 0){
					ifx += ixcl*tile.x;
					ix = ifx>>16;
					if(xsign*ix < xsign*bix) for(;;){
						ix = (ifx+ixcl)>>16;
						if(xsign*ix >= xsign*bix) break;
						ifx += ixcl;
					}
					if(--tile.w == 0 || xsign*ix >= xsign*dxend) return;
					tmppri = pri;
				}
				ifx += ixcl;
			}
		}else{
			for(;;){
				if(ix == ifx>>16){
					tmppri.nextPixel();
					if(tmppri.currentx <= 0){
						ifx += ixcl*tile.x;
						ix = ifx>>16;
						if(xsign*ix < xsign*bix) for(;;){
							ix = (ifx+ixcl)>>16;
							if(xsign*ix >= xsign*bix) break;
							ifx += ixcl;
						}
						if(--tile.w == 0 || xsign*ix >= xsign*dxend) return;
						tmppri = pri;
					}
					ifx += ixcl;
				}
				if(ck != (uint16_t)tmppri.color){
					uint32_t color = pspl[tmppri.color];
					uint32_t& dst = pdpx[ix];
					uint32_t tmp = (((~dst & color) << 1) + ((~dst ^ color) & 0xfefefefe)) & 0x01010100;
					uint32_t msk = tmp - (tmp >> 8);
					dst = (dst - color + tmp) & ~msk;
				}
				if((ix += xsign) == dxend) return;
			}
		}
	}
}

// Priority 2: fast specialization for PalletColorImg + mAlphaTrans (alpha/add+alpha blend).
// mAlphaTrans uses 64-bit arithmetic per pixel — by far the most expensive path in Debug.
// colorkey packs: bits[23:16] = src weight, bits[31:24] = dst weight.
template<> void mzlLoop<PalletColorImg, mAlphaTrans>(
	uint32_t* pdpx, PalletColorImg& pri, uint32_t* pspl, uint32_t colorkey,
	int xsign, SDL_Rect tile, int ix, int dxend, int ifx, int ixcl, int sx)
{
	PalletColorImg tmppri = pri;
	tmppri.skip(sx);
	uint16_t ck = (uint16_t)colorkey;
	// Pre-extract blend weights from colorkey (set by caller)
	uint32_t sw = (colorkey >> 16) & 0xff; // source weight
	uint32_t dw = colorkey >> 24;           // dest weight
	if(tile.w == 1){
		if(sx != 0 && tmppri.currentx <= 0) return;
		uint8_t* srcPtr = tmppri.data - 1;
		int srcRem = tmppri.width - tmppri.currentx;
		if(-65536 <= ixcl && ixcl <= 65536){
			for(; srcRem > 0; srcRem--, ifx += ixcl){
				int n = (int)(ix == (ifx >> 16)) - 1 & xsign;
				uint8_t c = *srcPtr++;
				if(n != 0 && c != ck){
					uint32_t color = pspl[c];
					uint32_t& dst = pdpx[ix];
					uint64_t tmpd =
						((uint64_t)(dst   & 0xff0000) << 16)
						| ((uint64_t)(dst   & 0xff00)   << 8)
						|  (uint64_t)(dst   & 0xff);
					uint64_t tmps =
						((uint64_t)(color & 0xff0000) << 16)
						| ((uint64_t)(color & 0xff00)   << 8)
						|  (uint64_t)(color & 0xff);
					tmpd *= dw;
					tmps *= sw;
					uint64_t tmp =
						((tmpd & tmps) + (((tmpd ^ tmps) >> 1) & 0x7fff7fff7fff7fffL))
						& 0x8000800080008000L;
					uint64_t msk = (tmp << 1) - (tmp >> 15);
					tmpd = ((tmpd + tmps) - msk) | msk;
					dst = (uint32_t)(
						((tmpd & 0xff0000000000L) >> 24)
						| ((tmpd & 0xff000000L) >> 16)
						| ((tmpd & 0xff00L) >> 8));
				}
				ix += n;
				if(ix == dxend) return;
			}
		}else{
			uint8_t cur = *srcPtr++;
			srcRem--;
			for(; ix != dxend; ix += xsign){
				if(ix == (ifx >> 16)){
					ifx += ixcl;
					if(srcRem <= 0) return;
					cur = *srcPtr++;
					srcRem--;
				}
				if(cur != ck){
					uint32_t color = pspl[cur];
					uint32_t& dst = pdpx[ix];
					uint64_t tmpd =
						((uint64_t)(dst   & 0xff0000) << 16)
						| ((uint64_t)(dst   & 0xff00)   << 8)
						|  (uint64_t)(dst   & 0xff);
					uint64_t tmps =
						((uint64_t)(color & 0xff0000) << 16)
						| ((uint64_t)(color & 0xff00)   << 8)
						|  (uint64_t)(color & 0xff);
					tmpd *= dw;
					tmps *= sw;
					uint64_t tmp =
						((tmpd & tmps) + (((tmpd ^ tmps) >> 1) & 0x7fff7fff7fff7fffL))
						& 0x8000800080008000L;
					uint64_t msk = (tmp << 1) - (tmp >> 15);
					tmpd = ((tmpd + tmps) - msk) | msk;
					dst = (uint32_t)(
						((tmpd & 0xff0000000000L) >> 24)
						| ((tmpd & 0xff000000L) >> 16)
						| ((tmpd & 0xff00L) >> 8));
				}
			}
		}
	}else{
		int bix = ix;
		if(-65536 <= ixcl && ixcl <= 65536){
			for(;;){
				int n = (int)(ix == ifx>>16) - 1 & xsign;
				if(n != 0 && ck != (uint16_t)tmppri.color){
					uint32_t color = pspl[tmppri.color];
					uint32_t& dst = pdpx[ix];
					uint64_t tmpd =
						((uint64_t)(dst   & 0xff0000) << 16)
						| ((uint64_t)(dst   & 0xff00)   << 8)
						|  (uint64_t)(dst   & 0xff);
					uint64_t tmps =
						((uint64_t)(color & 0xff0000) << 16)
						| ((uint64_t)(color & 0xff00)   << 8)
						|  (uint64_t)(color & 0xff);
					tmpd *= dw; tmps *= sw;
					uint64_t tmp =
						((tmpd & tmps) + (((tmpd ^ tmps) >> 1) & 0x7fff7fff7fff7fffL))
						& 0x8000800080008000L;
					uint64_t msk = (tmp << 1) - (tmp >> 15);
					tmpd = ((tmpd + tmps) - msk) | msk;
					dst = (uint32_t)(
						((tmpd & 0xff0000000000L) >> 24)
						| ((tmpd & 0xff000000L) >> 16)
						| ((tmpd & 0xff00L) >> 8));
				}
				if((ix += n) == dxend) return;
				tmppri.nextPixel();
				if(tmppri.currentx <= 0){
					ifx += ixcl*tile.x;
					ix = ifx>>16;
					if(xsign*ix < xsign*bix) for(;;){
						ix = (ifx+ixcl)>>16;
						if(xsign*ix >= xsign*bix) break;
						ifx += ixcl;
					}
					if(--tile.w == 0 || xsign*ix >= xsign*dxend) return;
					tmppri = pri;
				}
				ifx += ixcl;
			}
		}else{
			for(;;){
				if(ix == ifx>>16){
					tmppri.nextPixel();
					if(tmppri.currentx <= 0){
						ifx += ixcl*tile.x;
						ix = ifx>>16;
						if(xsign*ix < xsign*bix) for(;;){
							ix = (ifx+ixcl)>>16;
							if(xsign*ix >= xsign*bix) break;
							ifx += ixcl;
						}
						if(--tile.w == 0 || xsign*ix >= xsign*dxend) return;
						tmppri = pri;
					}
					ifx += ixcl;
				}
				if(ck != (uint16_t)tmppri.color){
					uint32_t color = pspl[tmppri.color];
					uint32_t& dst = pdpx[ix];
					uint64_t tmpd =
						((uint64_t)(dst   & 0xff0000) << 16)
						| ((uint64_t)(dst   & 0xff00)   << 8)
						|  (uint64_t)(dst   & 0xff);
					uint64_t tmps =
						((uint64_t)(color & 0xff0000) << 16)
						| ((uint64_t)(color & 0xff00)   << 8)
						|  (uint64_t)(color & 0xff);
					tmpd *= dw; tmps *= sw;
					uint64_t tmp =
						((tmpd & tmps) + (((tmpd ^ tmps) >> 1) & 0x7fff7fff7fff7fffL))
						& 0x8000800080008000L;
					uint64_t msk = (tmp << 1) - (tmp >> 15);
					tmpd = ((tmpd + tmps) - msk) | msk;
					dst = (uint32_t)(
						((tmpd & 0xff0000000000L) >> 24)
						| ((tmpd & 0xff000000L) >> 16)
						| ((tmpd & 0xff00L) >> 8));
				}
				if((ix += xsign) == dxend) return;
			}
		}
	}
}

// Priority 2: fast specialization for PalletColorImg + mAdd1Trans.
// colorkey bits[18:16] = shift count for the brightness multiplier.
template<> void mzlLoop<PalletColorImg, mAdd1Trans>(
	uint32_t* pdpx, PalletColorImg& pri, uint32_t* pspl, uint32_t colorkey,
	int xsign, SDL_Rect tile, int ix, int dxend, int ifx, int ixcl, int sx)
{
	PalletColorImg tmppri = pri;
	tmppri.skip(sx);
	uint16_t ck = (uint16_t)colorkey;
	uint32_t shift = (colorkey >> 16) & 0xff;
	uint32_t tmpm = (1u << (8 - shift)) - 1u;
	tmpm |= tmpm << 8 | tmpm << 16;
	if(tile.w == 1){
		if(sx != 0 && tmppri.currentx <= 0) return;
		uint8_t* srcPtr = tmppri.data - 1;
		int srcRem = tmppri.width - tmppri.currentx;
		if(-65536 <= ixcl && ixcl <= 65536){
			for(; srcRem > 0; srcRem--, ifx += ixcl){
				int n = (int)(ix == (ifx >> 16)) - 1 & xsign;
				uint8_t c = *srcPtr++;
				if(n != 0 && c != ck){
					uint32_t color = pspl[c];
					uint32_t& dst = pdpx[ix];
					uint32_t tmpd = dst >> shift & tmpm;
					uint32_t tmp = ((tmpd & color) + (((tmpd ^ color) >> 1) & 0x7f7f7f7f)) & 0x80808080;
					uint32_t msk = (tmp << 1) - (tmp >> 7);
					dst = ((tmpd + color) - msk) | msk;
				}
				ix += n;
				if(ix == dxend) return;
			}
		}else{
			uint8_t cur = *srcPtr++;
			srcRem--;
			for(; ix != dxend; ix += xsign){
				if(ix == (ifx >> 16)){
					ifx += ixcl;
					if(srcRem <= 0) return;
					cur = *srcPtr++;
					srcRem--;
				}
				if(cur != ck){
					uint32_t color = pspl[cur];
					uint32_t& dst = pdpx[ix];
					uint32_t tmpd = dst >> shift & tmpm;
					uint32_t tmp = ((tmpd & color) + (((tmpd ^ color) >> 1) & 0x7f7f7f7f)) & 0x80808080;
					uint32_t msk = (tmp << 1) - (tmp >> 7);
					dst = ((tmpd + color) - msk) | msk;
				}
			}
		}
	}else{
		int bix = ix;
		if(-65536 <= ixcl && ixcl <= 65536){
			for(;;){
				int n = (int)(ix == ifx>>16) - 1 & xsign;
				if(n != 0 && ck != (uint16_t)tmppri.color){
					uint32_t color = pspl[tmppri.color];
					uint32_t& dst = pdpx[ix];
					uint32_t tmpd = dst >> shift & tmpm;
					uint32_t tmp = ((tmpd & color) + (((tmpd ^ color) >> 1) & 0x7f7f7f7f)) & 0x80808080;
					uint32_t msk = (tmp << 1) - (tmp >> 7);
					dst = ((tmpd + color) - msk) | msk;
				}
				if((ix += n) == dxend) return;
				tmppri.nextPixel();
				if(tmppri.currentx <= 0){
					ifx += ixcl*tile.x;
					ix = ifx>>16;
					if(xsign*ix < xsign*bix) for(;;){
						ix = (ifx+ixcl)>>16;
						if(xsign*ix >= xsign*bix) break;
						ifx += ixcl;
					}
					if(--tile.w == 0 || xsign*ix >= xsign*dxend) return;
					tmppri = pri;
				}
				ifx += ixcl;
			}
		}else{
			for(;;){
				if(ix == ifx>>16){
					tmppri.nextPixel();
					if(tmppri.currentx <= 0){
						ifx += ixcl*tile.x;
						ix = ifx>>16;
						if(xsign*ix < xsign*bix) for(;;){
							ix = (ifx+ixcl)>>16;
							if(xsign*ix >= xsign*bix) break;
							ifx += ixcl;
						}
						if(--tile.w == 0 || xsign*ix >= xsign*dxend) return;
						tmppri = pri;
					}
					ifx += ixcl;
				}
				if(ck != (uint16_t)tmppri.color){
					uint32_t color = pspl[tmppri.color];
					uint32_t& dst = pdpx[ix];
					uint32_t tmpd = dst >> shift & tmpm;
					uint32_t tmp = ((tmpd & color) + (((tmpd ^ color) >> 1) & 0x7f7f7f7f)) & 0x80808080;
					uint32_t msk = (tmp << 1) - (tmp >> 7);
					dst = ((tmpd + color) - msk) | msk;
				}
				if((ix += xsign) == dxend) return;
			}
		}
	}
}

template<typename Img> void mzLineBilt(
	typename Funcs<Img>::mzllporc loop, uint32_t* pdpx, SDL_Rect& dr,
	float dcx, Img& pri, uint32_t* pspl, float cx, SDL_Rect& til,
	float xscl, uint32_t colorkey)
{
	float fx = dcx - cx*xscl;
	if(
		abs(fx) > 1.0e5f || abs(xscl) < 0.001f
		|| abs((float)pri.width * xscl) + (float)g_w > 32767.0f)
	{
		return;
	}
	int xsign = (xscl < 0.0f ? -1 : 1);
	int ix;
	int sx = 0;
	int dxend;
	SDL_Rect tile = til;
	if(xsign < 0){
		dxend = dr.x-1;
		if(til.w == 1 || (uint32_t)til.w > UINT16_MAX){
			tile.w = UINT16_MAX;
			if(floor(fx) < (float)(dr.x+dr.w-1)){
				fx +=
					ceil(
						((float)(dr.x+dr.w-1)-floor(fx))
						/ ((float)(pri.width+tile.x)*-xscl))
					* (float)(pri.width+tile.x)*-xscl;
			}
		}else if(til.w == 0){
			tile.w = 1;
		}
		ix = (int)floor(fx);
		if(ix > (int)dr.x+dr.w-1){
FOOFOOFOO:
			ix = (int)dr.x+dr.w-1;
			float n = floor(((float)ix-floor(fx))/xscl);
			fx += n*xscl;
			if(floor(fx+xscl) > (float)ix){
				fx += xscl;
				n += 1.0f;
			}
			sx += (int)n;
			if(sx >= pri.width+tile.x){
				if(sx >= (pri.width+tile.x)*tile.w) return;
				tile.w -= sx/(pri.width+tile.x);
				sx = sx%(pri.width+tile.x);
			}
			if(sx >= pri.width){
				if(--tile.w == 0) return;
				fx += xscl*(float)((pri.width+tile.x)-sx);
				ix = (int)floor(fx);
				sx = 0;
				if(ix > (int)dr.x+dr.w-1) goto FOOFOOFOO;
			}
		}
	}else{
		dxend = dr.x+dr.w;
		if(til.w == 1 || (uint32_t)til.w > UINT16_MAX){
			tile.w = UINT16_MAX;
			if(floor(fx) > (float)dr.x){
				fx -=
					ceil(
						(floor(fx)-(float)dr.x)
						/ ((float)(pri.width+tile.x)*xscl))
					* (float)(pri.width+tile.x)*xscl;
			}
		}else if(til.w == 0){
			tile.w = 1;
		}
		ix = (int)floor(fx);
		if(ix < dr.x){
BARBARBAR:
			ix = dr.x;
			float n = floor(((float)ix-floor(fx))/xscl);
			fx += n*xscl;
			if(floor(fx+xscl) < (float)ix){
				fx += xscl;
				n += 1.0f;
			}
			sx += (int)n;
			if(sx >= pri.width+tile.x){
				if(sx >= (pri.width+tile.x)*tile.w) return;
				tile.w -= sx/(pri.width+tile.x);
				sx = sx%(pri.width+tile.x);
			}
			if(sx >= pri.width){
				if(--tile.w == 0) return;
				fx += xscl*(float)((pri.width+tile.x)-sx);
				ix = (int)floor(fx);
				sx = 0;
				if(ix < dr.x) goto BARBARBAR;
			}
		}
	}
	if(xsign*ix >= xsign*dxend) return;
	fx += xscl;
	int ifx = (int)floor(fx*65536.0f);
	int ixcl = (int)(xscl*65536.0f);
	loop(pdpx, pri, pspl, colorkey, xsign, tile, ix, dxend, ifx, ixcl, sx);
}

void getdxdy(int& dx, int& dy, const Zurashi* zt, uint8_t ztofs, uint32_t roto)
{
	if((roto & 0x80) == 0){
		dx = zt[ztofs].dx;
		dy = zt[ztofs].dy;
	}else{
		dx = -zt[ztofs].dy;
		dy = -zt[ztofs].dx;
	}
}

template<int sign> void inclrxy(
	int& rx, int& ry, const Zurashi* xzt, uint8_t xztofs, uint32_t roto)
{
	int xp1 = (roto+256)>>9 & 1;
	int xmask = (int)(xp1 == 0) - 1;
	int yp1 = roto>>9 & 1;
	int ymask = (int)(yp1 == 0) - 1;
	int dx, dy;
	getdxdy(dx, dy, xzt, xztofs, roto);
	if((roto & 0x100) != 0){
		ry += sign*((dx^xmask) + xp1);
		rx += sign*((dy^ymask) + yp1);
	}else{
		rx += sign*((dx^xmask) + xp1);
		ry += sign*((dy^ymask) + yp1);
	}
}

template<typename Img, copycolorproc ccp> void mzrlLoop(
	uint32_t* pdpx, int dstw, int rx, int ry, Img pri, uint32_t* pspl,
	uint32_t roto, bool biltflg, uint32_t colorkey, int rxsrt, int rxend,
	int rysrt, int ryend, int rxlimmask, int rylimmask, int ifx, int ixcl)
{
	const Zurashi *xzt =
		RotoZurasiTable[(roto&0x80) == 0 ? roto & 0x7f : 128 - (roto & 0x7f)];
	uint8_t xztofs = 0;
	int ix = ifx>>16;
	ifx += ixcl;
	int tx = 1;
	if(ixcl > 0){
		if(ixcl < 65536){
			for(;;){
				if(ix >= ifx>>16 && tx > 0){
					pri.nextPixel();
					tx = pri.currentx;
					ifx += ixcl;
				}else{
					if(
						tx <= 0 || (rx^rxlimmask)+(rxlimmask&1) >= rxsrt
						&& (ry^rylimmask)+(rylimmask&1) >= rysrt)
					{
						break;
					}
					xztofs++;
					inclrxy<1>(rx, ry, xzt, xztofs, roto);
					ix++;
				}
			}
			for(;;){
				if(ix >= ifx>>16 && tx > 0){
					pri.nextPixel();
					tx = pri.currentx;
					ifx += ixcl;
				}else{
					if(
						tx <= 0 || (rx^rxlimmask)+(rxlimmask&1) >= rxend
						|| (ry^rylimmask)+(rylimmask&1) >= ryend)
					{
						return;
					}
					if(
						(uint16_t)colorkey != pri.color
						&& (
							biltflg
							|| (xzt[xztofs].dy == 0) == (xzt[xztofs].dx == 0))
						&& (xzt[xztofs].dy != 0 || xzt[xztofs].dx != 0))
					{
						ccp(pdpx[rx + ry*dstw], pspl[pri.color], colorkey);
					}
					xztofs++;
					inclrxy<1>(rx, ry, xzt, xztofs, roto);
					ix++;
				}
			}
		}else{
			for(;;){
				if(
					tx <= 0 || (rx^rxlimmask)+(rxlimmask&1) >= rxsrt
					&& (ry^rylimmask)+(rylimmask&1) >= rysrt)
				{
					break;
				}
				xztofs++;
				inclrxy<1>(rx, ry, xzt, xztofs, roto);
				if(++ix >= ifx>>16){
					pri.nextPixel();
					tx = pri.currentx;
					ifx += ixcl;
				}
			}
			for(;;){
				if(
					tx <= 0 || (rx^rxlimmask)+(rxlimmask&1) >= rxend
					|| (ry^rylimmask)+(rylimmask&1) >= ryend)
				{
					return;
				}
				if(
					(uint16_t)colorkey != pri.color
					&& (
						biltflg
						|| (xzt[xztofs].dy == 0) == (xzt[xztofs].dx == 0))
					&& (xzt[xztofs].dy != 0 || xzt[xztofs].dx != 0))
				{
					ccp(pdpx[rx + ry*dstw], pspl[pri.color], colorkey);
				}
				xztofs++;
				inclrxy<1>(rx, ry, xzt, xztofs, roto);
				if(++ix >= ifx>>16){
					pri.nextPixel();
					tx = pri.currentx;
					ifx += ixcl;
				}
			}
		}
	}else{
		if(ixcl > -65536){
			for(;;){
				if(ix <= ifx>>16 && tx > 0){
					pri.nextPixel();
					tx = pri.currentx;
					ifx += ixcl;
				}else{
					if(
						tx <= 0 || (rx^rxlimmask)+(rxlimmask&1) >= rxsrt
						&& (ry^rylimmask)+(rylimmask&1) >= rysrt)
					{
						break;
					}
					xztofs--;
					inclrxy<-1>(rx, ry, xzt, xztofs, roto);
					ix--;
				}
			}
			for(;;){
				if(ix <= ifx>>16 && tx > 0){
					pri.nextPixel();
					tx = pri.currentx;
					ifx += ixcl;
				}else{
					if(
						tx <= 0 || (rx^rxlimmask)+(rxlimmask&1) >= rxend
						|| (ry^rylimmask)+(rylimmask&1) >= ryend)
					{
						return;
					}
					xztofs--;
					if(
						(uint16_t)colorkey != pri.color
						&& (
							biltflg
							|| (xzt[xztofs].dy == 0) == (xzt[xztofs].dx == 0))
						&& (xzt[xztofs].dy != 0 || xzt[xztofs].dx != 0))
					{
						ccp(pdpx[rx + ry*dstw], pspl[pri.color], colorkey);
					}
					inclrxy<-1>(rx, ry, xzt, xztofs, roto);
					ix--;
				}
			}
		}else{
			for(;;){
				if(
					tx <= 0 || (rx^rxlimmask)+(rxlimmask&1) >= rxsrt
					&& (ry^rylimmask)+(rylimmask&1) >= rysrt)
				{
					break;
				}
				xztofs--;
				inclrxy<-1>(rx, ry, xzt, xztofs, roto);
				if(--ix <= ifx>>16){
					pri.nextPixel();
					tx = pri.currentx;
					ifx += ixcl;
				}
			}
			for(;;){
				if(
					tx <= 0 || (rx^rxlimmask)+(rxlimmask&1) >= rxend
					|| (ry^rylimmask)+(rylimmask&1) >= ryend)
				{
					return;
				}
				xztofs--;
				if(
					(uint16_t)colorkey != pri.color
					&& (
						biltflg
						|| (xzt[xztofs].dy == 0) == (xzt[xztofs].dx == 0))
					&& (xzt[xztofs].dy != 0 || xzt[xztofs].dx != 0))
				{
					ccp(pdpx[rx + ry*dstw], pspl[pri.color], colorkey);
				}
				inclrxy<-1>(rx, ry, xzt, xztofs, roto);
				if(--ix <= ifx>>16){
					pri.nextPixel();
					tx = pri.currentx;
					ifx += ixcl;
				}
			}
		}
	}
}

template<typename Img> void mzrLineBilt(
	typename Funcs<Img>::mrllporc loop, uint32_t* pdpx, int dstw,
	int rx, int ry, int xlim, int ylim, float fx, Img& pri, uint32_t* pspl,
	float xscl, uint32_t roto, bool biltflg, uint32_t colorkey)
{
	if(abs(fx) > 16383.0f) return;
	int rxsrt, rxend, rysrt, ryend;
	int rxlimmask, rylimmask;
	if(xscl < 0.0f){
		if(roto < 256){
			if(roto == 0 && ry < 0) return;
			rxsrt = -xlim+1; rxend = 1;    rxlimmask = -1;
			rysrt = 0;       ryend = ylim; rylimmask = 0;
		}else if(roto < 512){
			if(roto == 256 && rx < 0) return;
			rxsrt = 0;       rxend = xlim; rxlimmask = 0;
			rysrt = 0;       ryend = ylim; rylimmask = 0;
		}else if(roto < 768){
			if(roto == 512 && ry >= ylim) return;
			rxsrt = 0;       rxend = xlim; rxlimmask = 0;
			rysrt = -ylim+1; ryend = 1;    rylimmask = -1;
		}else{
			if(roto == 768 && rx >= xlim) return;
			rxsrt = -xlim+1; rxend = 1;    rxlimmask = -1;
			rysrt = -ylim+1; ryend = 1;    rylimmask = -1;
		}
	}else{
		if(roto < 256){
			if(roto == 0 && ry >= ylim) return;
			rxsrt = 0;       rxend = xlim; rxlimmask = 0;
			rysrt = -ylim+1; ryend = 1;    rylimmask = -1;
		}else if(roto < 512){
			if(roto == 256 && rx >= xlim) return;
			rxsrt = -xlim+1; rxend = 1;    rxlimmask = -1;
			rysrt = -ylim+1; ryend = 1;    rylimmask = -1;
		}else if(roto < 768){
			if(roto == 512 && ry < 0) return;
			rxsrt = -xlim+1; rxend = 1;    rxlimmask = -1;
			rysrt = 0;       ryend = ylim; rylimmask = 0;
		}else{
			if(roto == 768 && rx < 0) return;
			rxsrt = 0;       rxend = xlim; rxlimmask = 0;
			rysrt = 0;       ryend = ylim; rylimmask = 0;
		}
	}
	int ifx = (int)floor(fx*65536.0f);
	int ixcl = (int)(xscl*65536.0f);
	loop(
		pdpx, dstw, rx, ry, pri, pspl, roto, biltflg, colorkey,
		rxsrt, rxend, rysrt, ryend, rxlimmask, rylimmask, ifx, ixcl);
}

template<typename Img> void mzScreenBilt(
	typename Funcs<Img>::mzllporc loop, SDL_Rect& dr,
	float rcx, Img pri, uint32_t *ppal, SDL_Rect& srcr, float cx, float ty,
	SDL_Rect& tile, float xtopscl, float xbotscl, float yscl,
	float rasterxadd, uint32_t colorkey)
{
	if(abs(yscl) < 0.001f) return;
	if(dr.x < 0){
		dr.w += dr.x;
		dr.x = 0;
	}
	if((int)dr.x+dr.w > g_w) dr.w -= dr.x+dr.w - g_w;
	if((int16_t)dr.w <= 0) return;
	if(dr.y < 0){
		dr.h += dr.y;
		dr.y = 0;
	}
	if((int)dr.y+dr.h > g_h) dr.h -= dr.y+dr.h - g_h;
	if((int16_t)dr.h <= 0) return;
	float fcx = cx / abs(xtopscl);
	uint32_t* pdpx = g_pix;
	int dstw = g_pitch / sizeof(uint32_t);
	int ysign;
	int dybgn;
	int dyend;
	if(yscl < 0.0f){
		ysign = -1;
		dybgn = dr.y+dr.h-1;
		dyend = dr.y-1;
		ty -= (float)(dr.y+(int)dr.h);
	}else{
		ysign = 1;
		dybgn = dr.y;
		dyend = dr.y+dr.h;
		ty += (float)dr.y;
	}
	float xscdf = (xbotscl - xtopscl) / ((float)(ysign*srcr.h) * yscl);
	float xscl = xtopscl + xscdf*0.5f;
	float dcx = rcx + (xscl < 0.0 ? -0.5f : 0.5f);
	float fy = (float)dybgn - (float)ysign*ty + 0.5f;
	int iy = dybgn;
	int sy = 0;
	if(ty < 0.0f){
		if(tile.h != 1){
			iy = (int)floor(fy);
		}else{
			xscl += xscdf*ty;
			dcx += rasterxadd*ty;
			float n = floor((float)ysign*ty/yscl);
			fy += n*yscl;
			if((float)ysign*floor(fy+yscl) < (float)(ysign*iy)){
				fy += yscl;
				n += 1.0f;
			}
			sy = (sy + (int)n) % (srcr.h+tile.y);
			if(sy < 0) sy += srcr.h+tile.y;
		}
		if(tile.h == 0){
			tile.h = 1;
		}else if(tile.h == 1 || (uint32_t)tile.h > UINT16_MAX){
			tile.h = UINT16_MAX;
		}
	}else{
		xscl += xscdf*ty;
		dcx += rasterxadd*ty;
		float n = floor((float)ysign*ty/yscl);
		fy += n*yscl;
		if((float)ysign*floor(fy+yscl) < (float)(ysign*iy)){
			fy += yscl;
			n += 1.0f;
		}
		sy += (int)n;
		if(tile.h == 0){
			tile.h = 1;
		}else if(tile.h == 1 || (uint32_t)tile.h > UINT16_MAX){
			tile.h = UINT16_MAX;
		}
		if(sy >= srcr.h+tile.y){
			if(sy >= (int)(srcr.h+tile.y)*tile.h) return;
			tile.h -= sy/(srcr.h+tile.y);
			sy = sy%(srcr.h+tile.y);
		}
	}
	if(sy >= srcr.h){
		fy += yscl*(float)(tile.y - (sy-srcr.h));
		xscl += xscdf*(float)(ysign*((int)floor(fy)-iy));
		dcx += rasterxadd*(float)(ysign*((int)floor(fy)-iy));
		iy = (int)floor(fy);
		sy = 0;
		if(--tile.h == 0) return;
	}
	if(ysign*iy >= ysign*dyend) return;
	pdpx += dstw*iy;
	Img newpri = pri;
	int i;
	for(i = 0; i < sy; i++) newpri.nextLine();
	fy += yscl;
	if(newpri.finished()){
		xscl += (float)ysign*(floor(fy)-(float)iy)*xscdf;
		dcx += (float)ysign*(floor(fy)-(float)iy)*rasterxadd;
		pdpx += ((int)floor(fy)-iy)*dstw;
		iy = (int)floor(fy);
	}
	float dcx2 = dcx;
	while(ysign*iy < ysign*dyend){
		if(iy == (int)floor(fy)){
			do{
				newpri.nextLine();
				if(newpri.currenty >= srcr.h || newpri.finished()){
					fy += yscl*(float)(tile.y+srcr.h-newpri.currenty);
					xscl += xscdf*(float)(ysign*((int)floor(fy)-iy));
					dcx += rasterxadd*(float)(ysign*((int)floor(fy)-iy));
					pdpx += ((int)floor(fy) - iy)*dstw;
					iy = (int)floor(fy);
					if(--tile.h == 0 || ysign*iy >= ysign*dyend) return;
					newpri = pri;
				}
				fy += yscl;
			}while(iy == (int)floor(fy));
			dcx2 = dcx;
		}
		if(ysign*iy >= ysign*dybgn){
			mzLineBilt(
				loop, pdpx, dr, dcx2, newpri, ppal, fcx, tile, xscl, colorkey);
		}
		xscl += xscdf;
		dcx += rasterxadd;
		iy += ysign;
		pdpx += ysign*dstw;
	}
}

template<typename Img> void mzrScreenBilt(
	typename Funcs<Img>::mrllporc loop,
	float rcx, float rcy, Img& pri, uint32_t* ppal, SDL_Rect& srcr,
	float fx, float fy, float xscl, float yscl,
	uint32_t roto, uint32_t colorkey)
{
	if(yscl < 0.0f){
		xscl *= -1.0f;
		yscl *= -1.0f;
		roto = roto + 512 & 0x3ff;
	}
	const Zurashi *yzt =
		RotoZurasiTable[
			(roto-256 & 0x80) == 0
			? roto-256 & 0x7f : 128 - (roto-256 & 0x7f)];
	uint8_t yztofs = 0;
	uint32_t* pdpx = g_pix;
	int dstw = g_pitch / sizeof(uint32_t);
	int xlim = g_w;
	int ylim = g_h;
	float tmpx = fx = rcx + (xscl < 0.0f ? fx : -fx);
	float tmpy = fy = rcy - fy;
	kaiten(tmpx, tmpy, -((float)PI*(float)roto/512.0f), rcx, rcy, 1.0);
	int rx = (int)floor(tmpx + 0.5f), ry = (int)floor(tmpy + 0.5f);
	intptr_t pmask = (int32_t)((roto-256)<<(31-8))>>31;
	int xp1 = roto>>9 & 1;
	int xmask = (int)(xp1 == 0) - 1;
	int yp1 = (roto-256)>>9 & 1;
	int ymask = (int)(yp1 == 0) - 1;
	fx += xscl < 0.0 ? -0.5f : 0.5f;
	fy += 0.5f;
	int iy = (int)floor(fy);
	fy += yscl;
	int tmpdx = 0, tmpdy = 1;
	*(int*)(((intptr_t)&rx&pmask) | ((intptr_t)&ry&~pmask)) -=
		((tmpdy^ymask) + yp1);
	for(;;){
		while(iy == (int)floor(fy)){
			pri.nextLine();
			if(pri.currenty >= srcr.h || pri.finished()) return;
			fy += yscl;
		}
		if(tmpdx != 0){
			*(int*)(((intptr_t)&rx&~pmask) | ((intptr_t)&ry&pmask)) +=
				((tmpdx^xmask) + xp1);
			mzrLineBilt(
				loop, pdpx, dstw, rx, ry, xlim, ylim, fx,
				pri, ppal, xscl, roto, tmpdy == 0, colorkey);
		}
		if(tmpdy != 0){
			*(int*)(((intptr_t)&rx&pmask) | ((intptr_t)&ry&~pmask)) +=
				((tmpdy^ymask) + yp1);
			mzrLineBilt(loop, pdpx, dstw, rx, ry, xlim, ylim, fx,
				pri, ppal, xscl, roto, true, colorkey);
		}
		getdxdy(tmpdx, tmpdy, yzt, yztofs, roto-256);
		yztofs++;
		iy++;
	}
}

static bool fastSimpleAddRectBlit(
	SDL_Rect dr, Reference img, SDL_Rect psrcr, uint32_t* ppal, uint16_t colorkey)
{
	if(
		dr.w <= 0 || dr.h <= 0 || psrcr.w <= 0 || psrcr.h <= 0
		|| img.len() < psrcr.w * psrcr.h)
	{
		return false;
	}
	int visX0 = (std::max)(dr.x, 0);
	int visY0 = (std::max)(dr.y, 0);
	int visX1 = (std::min)(dr.x + dr.w, g_w);
	int visY1 = (std::min)(dr.y + dr.h, g_h);
	int visW = visX1 - visX0;
	int visH = visY1 - visY0;
	if(visW <= 0 || visH <= 0) return true;

	const uint8_t* srcBase = (const uint8_t*)img.atpos();
	const uint16_t ck = colorkey;
	const int dstPitch = g_pitch / (int)sizeof(uint32_t);
	const int startDx = visX0 - dr.x;
	const int startDy = visY0 - dr.y;
	const int64_t yProd = (int64_t)startDy * psrcr.h;
	int srcY = (int)(yProd / dr.h);
	int yErr = (int)(yProd % dr.h);
	if(yErr < 0){
		yErr += dr.h;
		srcY--;
	}
	int y;
	for(y = 0; y < visH; ++y){
		const uint8_t* srcRow = srcBase + srcY * psrcr.w;
		uint32_t* dstRow = g_pix + (visY0 + y) * dstPitch + visX0;
		const int64_t xProd = (int64_t)startDx * psrcr.w;
		int srcX = (int)(xProd / dr.w);
		int xErr = (int)(xProd % dr.w);
		if(xErr < 0){
			xErr += dr.w;
			srcX--;
		}
		int x = 0;
		for(; x + 4 <= visW; x += 4){
			int sx0 = srcX;
			xErr += psrcr.w; while(xErr >= dr.w){ xErr -= dr.w; ++srcX; }
			int sx1 = srcX;
			xErr += psrcr.w; while(xErr >= dr.w){ xErr -= dr.w; ++srcX; }
			int sx2 = srcX;
			xErr += psrcr.w; while(xErr >= dr.w){ xErr -= dr.w; ++srcX; }
			int sx3 = srcX;
			xErr += psrcr.w; while(xErr >= dr.w){ xErr -= dr.w; ++srcX; }
			uint8_t b0 = srcRow[sx0], b1 = srcRow[sx1];
			uint8_t b2 = srcRow[sx2], b3 = srcRow[sx3];
			uint32_t c0 = (b0 != ck) ? ppal[b0] : 0;
			uint32_t c1 = (b1 != ck) ? ppal[b1] : 0;
			uint32_t c2 = (b2 != ck) ? ppal[b2] : 0;
			uint32_t c3 = (b3 != ck) ? ppal[b3] : 0;
			__m128i s = _mm_setr_epi32((int)c0, (int)c1, (int)c2, (int)c3);
			__m128i d = _mm_loadu_si128((const __m128i*)(dstRow + x));
			_mm_storeu_si128((__m128i*)(dstRow + x), _mm_adds_epu8(d, s));
		}
		for(; x < visW; ++x){
			uint8_t c = srcRow[srcX];
			if(c != ck){
				uint32_t color = ppal[c];
				uint32_t& dst = dstRow[x];
				uint32_t tmp = ((dst & color) + (((dst ^ color) >> 1) & 0x7f7f7f7f)) & 0x80808080;
				uint32_t msk = (tmp << 1) - (tmp >> 7);
				dst = ((dst + color) - msk) | msk;
			}
			xErr += psrcr.w;
			while(xErr >= dr.w){ xErr -= dr.w; ++srcX; }
		}
		yErr += psrcr.h;
		while(yErr >= dr.h){ yErr -= dr.h; ++srcY; }
	}
	g_perfCounters.fastAddRectBlits++;
	g_lastRenderPathFlags |= SPRITE_PATH_FAST_ADD_RECT;
	return true;
}

template<copycolorproc ccp> void mRender(
	SDL_Rect dr, float rcx, float rcy, const Reference& img,
	uint32_t *ppal, SDL_Rect psrcr, float cx, float ty, SDL_Rect tile,
	float xtopscl, float xbotscl, float yscl, float rasterxadd,
	uint32_t roto, uint32_t colorkey, int rle, Reference *pluginbuf)
{
	roto &= 0x3ff;
	LOG_DEBUG("RENDER", "mRender: dr=(%d,%d,%d,%d) src=(%d,%d) rle=%d img.len=%d", dr.x, dr.y, dr.w, dr.h, psrcr.w, psrcr.h, rle, img.len());
	if(!g_pix) { LOG_DEBUG("RENDER", "mRender: g_pix is NULL!"); return; }
	if(rle > 0){
		PcxRleImg pri;
		pri.setImg(const_cast<Reference&>(img), psrcr.w, rle, pluginbuf);
		if(roto == 0){
			mzScreenBilt(
				mzlLoop<PcxRleImg, ccp>, dr, rcx, pri, ppal, psrcr,
				cx, ty, tile, xtopscl, xbotscl, yscl, rasterxadd, colorkey);
		}else{
			mzrScreenBilt(
				mzrlLoop<PcxRleImg, ccp>, rcx, rcy, pri, ppal, psrcr,
				cx, ty, xtopscl, yscl, roto, colorkey);
		}
	}else{
		PalletColorImg pri;
		pri.setImg(const_cast<Reference&>(img), psrcr.w);
		if(roto == 0){
			if(
				ccp == mAddTrans
				&& (g_lastRenderPathFlags & SPRITE_PATH_SIMPLE_RECT) != 0
				&& (uint64_t)dr.w * (uint64_t)dr.h >= (uint64_t)g_w * (uint64_t)g_h
				&& fastSimpleAddRectBlit(dr, img, psrcr, ppal, (uint16_t)colorkey))
			{
				return;
			}
			LOG_DEBUG("RENDER", "mRender: calling mzScreenBilt");
			mzScreenBilt(
				mzlLoop<PalletColorImg, ccp>, dr, rcx, pri, ppal, psrcr,
				cx, ty, tile, xtopscl, xbotscl, yscl, rasterxadd, colorkey);
			LOG_DEBUG("RENDER", "mRender: mzScreenBilt returned");
		}else{
			mzrScreenBilt(
				mzrlLoop<PalletColorImg, ccp>, rcx, rcy, pri, ppal,
				psrcr, cx, ty, xtopscl, yscl, roto, colorkey);
		}
	}
	LOG_DEBUG("RENDER", "mRender: done");
}

int foobar(int n)
{
	if(n == 127 || n == 128) return 1;
	if(n == 63 || n == 64) return 2;
	if(n == 31 || n == 32) return 3;
	if(n == 15 || n == 16) return 4;
	if(n == 7 || n == 8) return 5;
	if(n == 3 || n == 4) return 6;
	if(n == 1 || n == 2) return 7;
	return 0;
}

bool SSZ_STDCALL RenderMugenZoom(Reference* pluginbuf, int32_t rle,
	float rcy, float rcx, SDL_Rect* pdstr, int32_t alpha,
	uint32_t roto, float rasterxadd, float yscl, float xbotscl, float xtopscl,
	SDL_Rect* tile, float ty, float cx, SDL_Rect* psrcr,
	uint16_t ckey, uint32_t* ppal, Reference img)
{
	LARGE_INTEGER t0, t1;
	QueryPerformanceCounter(&t0);
	g_perfCounters.spriteCount++;
	g_perfCounters.drawCalls++;
	SDL_Rect tl = *tile;
	if(tl.x > 0) tl.x -= psrcr->w;
	if(tl.y > 0) tl.y -= psrcr->h;
	if(tl.w == 0) tl.x = 0;
	if(tl.h == 0) tl.y = 0;
	g_lastRenderPathFlags = (rle > 0) ? SPRITE_PATH_RLE : SPRITE_PATH_NONE;
	if(roto != 0) g_lastRenderPathFlags |= SPRITE_PATH_ROTATED;
	if(tl.x != 0 || tl.y != 0 || tl.w != 1 || tl.h != 1)
		g_lastRenderPathFlags |= SPRITE_PATH_TILED;
	if(fabs(xtopscl - xbotscl) >= 0.001f || fabs(rasterxadd) >= 0.001f)
		g_lastRenderPathFlags |= SPRITE_PATH_TRAPEZOID;
	if(
		rle <= 0 && roto == 0
		&& tl.x == 0 && tl.y == 0 && tl.w == 1 && tl.h == 1
		&& fabs(xtopscl - xbotscl) < 0.001f
		&& fabs(rasterxadd) < 0.001f)
	{
		g_lastRenderPathFlags |= SPRITE_PATH_SIMPLE_RECT;
	}
	LOG_DEBUG("RENDER", "RenderMugenZoom: dr=(%d,%d,%d,%d) src=(%d,%d) rle=%d alpha=%d img.len=%d ppal=%p",
	          pdstr->x, pdstr->y, pdstr->w, pdstr->h, psrcr->w, psrcr->h, rle, alpha, img.len(), (void*)ppal);
	if(
		img.len() == 0
		|| tl.x <= -(int)psrcr->w || tl.y <= -(int)psrcr->h
		|| _finite(cx+ty+rcx+rcy+xtopscl+xbotscl+yscl+rasterxadd) == 0
		|| abs(rcx) > 1.0e5f || abs(rcy) > 1.0e5f
		|| abs(cx) > 1.0e5f || abs(ty) > 1.0e5f
		|| abs(xtopscl) > 16383.0f || abs(xbotscl) > 16383.0f
		|| abs(yscl) > 16383.0f) {
		LOG_DEBUG("RENDER", "RenderMugenZoom: EARLY RETURN (bounds check failed)");
		return false;
	}
	uint32_t pal[256];
	int i;
	if(
		(
			127 <= alpha && alpha <= 254 && foobar(255 - alpha)
			&& (alpha |=  1 << 9 | (255 - alpha) << 10, true))
		|| (
			alpha >= 512 && (
				(alpha&0x3fc00) >> 10 == 0 || (alpha&0x3fc00) >> 10 == 255
				|| ((alpha&0xff) != 255 && foobar((alpha&0x3fc00) >> 10)))))
	{
		uint64_t tmps;
		for(i = 0; i < 256; i++){
			tmps =
				((uint64_t)(ppal[i]&0xff0000) << 16)
				| ((uint64_t)(ppal[i]&0xff00) << 8) | (uint64_t)(ppal[i]&0xff);
			tmps *= alpha&0xff;
			pal[i] =
				(uint32_t)((tmps&0xff0000000000L)>>24
				| (tmps&0xff000000L)>>16 | (tmps&0xff00L)>>8);
		}
		ppal = pal;
		if((alpha&0x3fc00) >> 10 == 0){
			alpha = 255;
		}else if((alpha&0x3fc00) >> 10 == 255){
			alpha = -1;
		}else{
			alpha = 255 | 1 << 9 | (alpha&0x3fc00);
		}
	}
	if(alpha >= 512 && (g_lastRenderPathFlags & SPRITE_PATH_SIMPLE_RECT) != 0)
		g_perfCounters.addAlphaSimpleSprites++;
	if(alpha == -1){
		mRender<mAddTrans>(
			*pdstr, rcx, rcy, img, ppal, *psrcr, cx, ty, tl,
			xtopscl, xbotscl, yscl, rasterxadd, roto, ckey, rle, pluginbuf);
	}else if(alpha == -2){
		mRender<mSubTrans>(
			*pdstr, rcx, rcy, img, ppal, *psrcr, cx, ty, tl,
			xtopscl, xbotscl, yscl, rasterxadd, roto, ckey, rle, pluginbuf);
	}else if(alpha <= 0){
	}else if(alpha < 255){
		uint32_t ck = ckey;
		ck |= (uint32_t)(alpha&0xff) << 16;
		ck |= (uint32_t)(256-alpha) << 24;
		mRender<mAlphaTrans>(
			*pdstr, rcx, rcy, img, ppal, *psrcr, cx, ty, tl,
			xtopscl, xbotscl, yscl, rasterxadd, roto, ck, rle, pluginbuf);
	}else if(alpha < 512){
		mRender<mTrans>(
			*pdstr, rcx, rcy, img, ppal, *psrcr, cx, ty, tl,
			xtopscl, xbotscl, yscl, rasterxadd, roto, ckey, rle, pluginbuf);
	}else{
		if((alpha&0xff) == 255 && foobar((alpha&0x3fc00) >> 10)){
			uint32_t ck = ckey;
			ck |= foobar((alpha&0x3fc00) >> 10) << 16;
			mRender<mAdd1Trans>(
				*pdstr, rcx, rcy, img, ppal, *psrcr, cx, ty, tl,
				xtopscl, xbotscl, yscl, rasterxadd, roto, ck, rle, pluginbuf);
		}else{
			uint32_t ck = ckey;
			ck |= (uint32_t)(alpha&0xff) << 16;
			ck |= (uint32_t)(alpha&0x3fc00) << 14;
			mRender<mAlphaTrans>(
				*pdstr, rcx, rcy, img, ppal, *psrcr, cx, ty, tl,
				xtopscl, xbotscl, yscl, rasterxadd, roto, ck, rle, pluginbuf);
		}
	}
	LOG_DEBUG("RENDER", "RenderMugenZoom: mRender returned, img.pointer=%p img.len=%d",
	          (void*)img.pointer, img.len());
	QueryPerformanceCounter(&t1);
	LOG_DEBUG("RENDER", "RenderMugenZoom: QueryPerformanceCounter OK");
	{
		double elapsed = qpcElapsedUs(t0, t1, g_perfCounters.qpcFreq);
		g_perfCounters.spriteTimeUs += elapsed;
		int dw = pdstr->w > 0 ? pdstr->w : 0;
		int dh = pdstr->h > 0 ? pdstr->h : 0;
		g_perfCounters.totalPixelArea += (uint64_t)(dw * dh);
		if (elapsed > g_perfCounters.worstSpriteUs)
		{
			g_perfCounters.worstSpriteUs = elapsed;
			g_perfCounters.worstSpriteSrcW = psrcr->w;
			g_perfCounters.worstSpriteSrcH = psrcr->h;
			g_perfCounters.worstSpriteW = dw;
			g_perfCounters.worstSpriteH = dh;
			g_perfCounters.worstSpriteAlpha = alpha;
			g_perfCounters.worstSpritePathFlags = g_lastRenderPathFlags;
		}
	}
	LOG_DEBUG("RENDER", "RenderMugenZoom: perf counter update OK, returning true");
	return true;
}

// ---------------------------------------------------------------------------
// RenderFontBatch — Batched software font rendering (OPT-F2)
// Renders all glyphs of a text string in a single FFI call, eliminating
// per-glyph RenderMugenZoom overhead (path flags, alpha dispatch, QPC timing).
// ---------------------------------------------------------------------------
bool SSZ_STDCALL RenderFontBatch(int32_t count,
	int32_t* glyphData,
	float spacing,
	float yscl,
	float xscl,
	SDL_Rect* window,
	int32_t alpha,
	int32_t glyphH,
	int32_t atlasStride,
	uint32_t* ppal,
	float baseY,
	float baseX,
	uint8_t* atlasPixels)
{
	if(count <= 0 || !atlasPixels || !glyphData || !window || !ppal || glyphH <= 0) return false;
	if(_finite(baseX + baseY + xscl + yscl + spacing) == 0) return false;
	if(fabsf(xscl) < 0.001f || fabsf(yscl) < 0.001f) return true;

	LARGE_INTEGER t0, t1;
	QueryPerformanceCounter(&t0);
	g_perfCounters.drawCalls++;

	int winX0 = 0, winY0 = 0, winX1 = 0, winY1 = 0;
	int firstAtlasOfs = 0, firstGlyphW = 0;
#ifdef _MSC_VER
	__try {
		winX0 = (int)window->x;
		winY0 = (int)window->y;
		winX1 = winX0 + (int)window->w;
		winY1 = winY0 + (int)window->h;
		if(count > 0){
			firstAtlasOfs = glyphData[0];
			firstGlyphW = glyphData[1];
		}
	} __except(EXCEPTION_EXECUTE_HANDLER) {
		REND_LOG(
			"RenderFontBatch: pointer probe threw exception atlasPixels=%p glyphData=%p window=%p ppal=%p",
			atlasPixels, glyphData, window, ppal);
		return false;
	}
#else
	{
		winX0 = (int)window->x;
		winY0 = (int)window->y;
		winX1 = winX0 + (int)window->w;
		winY1 = winY0 + (int)window->h;
	}
#endif
	if(atlasStride <= 0){
		REND_LOG("RenderFontBatch: invalid atlasStride=%d", atlasStride);
		return false;
	}
	const int dstPitch = g_pitch / (int)sizeof(uint32_t);
	const float absXscl = fabsf(xscl);
	const float absYscl = fabsf(yscl);
	int i;
	
	// --- Alpha pre-processing (mirrors RenderMugenZoom logic) ---
	uint32_t localPal[256];
	int alphaMode;
	if(alpha == -1){
		alphaMode = -1;
	}else if(alpha == -2){
		alphaMode = -2;
	}else if(alpha <= 0){
		// Invisible
		QueryPerformanceCounter(&t1);
		g_perfCounters.spriteTimeUs += qpcElapsedUs(t0, t1, g_perfCounters.qpcFreq);
		return true;
	}else if(alpha < 255){
		// Direct semi-transparent alpha
		alphaMode = alpha;
	}else if(alpha < 512){
		alphaMode = 255;
	}else{
		// Packed alpha: src=(alpha&0xff), dst=(alpha>>10)&0xff
		int srcAlpha = alpha & 0xff;
		int dstAlpha = (alpha & 0x3fc00) >> 10;
		if(
			(
				127 <= srcAlpha && srcAlpha <= 254 && foobar(256 - srcAlpha))
			|| (
				dstAlpha == 0 || dstAlpha == 255
				|| (srcAlpha != 255 && foobar(dstAlpha))))
		{
			uint64_t tmps;
			for(i = 0; i < 256; i++){
				tmps =
					((uint64_t)(ppal[i]&0xff0000) << 16)
					| ((uint64_t)(ppal[i]&0xff00) << 8) | (uint64_t)(ppal[i]&0xff);
				tmps *= srcAlpha;
				localPal[i] =
					(uint32_t)((tmps&0xff0000000000LL)>>24
					| (tmps&0xff000000LL)>>16 | (tmps&0xff00LL)>>8);
			}
			ppal = localPal;
			if(dstAlpha == 0){
				alphaMode = 255;
			}else if(dstAlpha == 255){
				alphaMode = -1;
			}else{
				alphaMode = 255 | 1 << 9 | (dstAlpha << 10);
			}
		}else{
			// Non-optimisable packed alpha — treat as alpha blend
			alphaMode = alpha;
		}
	}

	// For mAlphaTrans-style blend, compute shift factors
	uint32_t ckSrc = 0, ckDst = 0;
	if(alphaMode > 0 && alphaMode < 255){
		ckSrc = (uint32_t)alphaMode;
		ckDst = (uint32_t)(256 - alphaMode);
	}else if(alphaMode >= 512){
		ckSrc = (uint32_t)(alphaMode & 0xff);
		if(ckSrc == 0) ckSrc = 256;
		ckDst = (uint32_t)((alphaMode & 0x3fc00) >> 10);
	}

	// --- Compute dest Y range (same for all glyphs) ---
	int destH = (int)(glyphH * absYscl + 0.5f);
	if(destH <= 0){
		QueryPerformanceCounter(&t1);
		g_perfCounters.spriteTimeUs += qpcElapsedUs(t0, t1, g_perfCounters.qpcFreq);
		return true;
	}
	int screenY = (int)floorf(baseY + 0.5f);
	int clipY0 = screenY < winY0 ? winY0 : screenY;
	if(clipY0 < 0) clipY0 = 0;
	int clipY1 = screenY + destH;
	if(clipY1 > winY1) clipY1 = winY1;
	if(clipY1 > g_h) clipY1 = g_h;
	if(clipY0 >= clipY1){
		QueryPerformanceCounter(&t1);
		g_perfCounters.spriteTimeUs += qpcElapsedUs(t0, t1, g_perfCounters.qpcFreq);
		return true;
	}

	// --- Render each glyph ---
	float dx = baseX;

	for(int g = 0; g < count; g++){
		int atlasOfs = glyphData[g * 2];
		int glyphW   = glyphData[g * 2 + 1];
		// if(g < 4){
		// 	REND_LOG(
		// 		"RenderFontBatch: glyph[%d] ofs=%d w=%d dx=%.3f",
		// 		g, atlasOfs, glyphW, dx);
		// }

		if(atlasOfs >= 0 && glyphW > 0){
			if(atlasOfs >= atlasStride || atlasOfs + glyphW > atlasStride){
				REND_LOG(
					"RenderFontBatch: glyph[%d] out of atlas row bounds ofs=%d w=%d stride=%d",
					g, atlasOfs, glyphW, atlasStride);
				continue;
			}
			int destW = (int)(glyphW * absXscl + 0.5f);
			int screenX = (int)floorf(dx + 0.5f);

			if(destW > 0){
				int clipX0 = screenX < winX0 ? winX0 : screenX;
				if(clipX0 < 0) clipX0 = 0;
				int clipX1 = screenX + destW;
				if(clipX1 > winX1) clipX1 = winX1;
				if(clipX1 > g_w) clipX1 = g_w;

				if(clipX0 < clipX1){
					g_perfCounters.spriteCount++;

					for(int dy = clipY0; dy < clipY1; dy++){
						int srcY = ((dy - screenY) * glyphH) / destH;
						if((unsigned)srcY >= (unsigned)glyphH){
							REND_LOG(
								"RenderFontBatch: glyph[%d] bad srcY=%d glyphH=%d dy=%d screenY=%d destH=%d",
								g, srcY, glyphH, dy, screenY, destH);
							continue;
						}
						const uint8_t* srcRow = atlasPixels + srcY * atlasStride + atlasOfs;
						uint32_t* dstRow = g_pix + dy * dstPitch;

						if(alphaMode == 255){
							// --- Opaque (mTrans) ---
							if(destW == glyphW){
								// 1:1 horizontal scale fast path
								int sx = clipX0 - screenX;
								for(int dxx = clipX0; dxx < clipX1; dxx++, sx++){
									uint8_t idx = srcRow[sx];
									if(idx != 0) dstRow[dxx] = ppal[idx];
								}
							}else{
								for(int dxx = clipX0; dxx < clipX1; dxx++){
									int srcX = ((dxx - screenX) * glyphW) / destW;
									uint8_t idx = srcRow[srcX];
									if(idx != 0) dstRow[dxx] = ppal[idx];
								}
							}
						}else if(alphaMode == -1){
							// --- Additive (mAddTrans) ---
							for(int dxx = clipX0; dxx < clipX1; dxx++){
								int srcX = (destW == glyphW)
									? (dxx - screenX)
									: ((dxx - screenX) * glyphW) / destW;
								uint8_t idx = srcRow[srcX];
								if(idx != 0){
									uint32_t s = ppal[idx];
									uint32_t d = dstRow[dxx];
									uint32_t tmp =
										((d & s) + (((d ^ s) >> 1) & 0x7f7f7f7f))
										& 0x80808080;
									uint32_t msk = (tmp << 1) - (tmp >> 7);
									dstRow[dxx] = ((d + s) - msk) | msk;
								}
							}
						}else if(alphaMode == -2){
							// --- Subtractive (mSubTrans) ---
							for(int dxx = clipX0; dxx < clipX1; dxx++){
								int srcX = (destW == glyphW)
									? (dxx - screenX)
									: ((dxx - screenX) * glyphW) / destW;
								uint8_t idx = srcRow[srcX];
								if(idx != 0){
									uint32_t s = ppal[idx];
									uint32_t d = dstRow[dxx];
									uint32_t tmp =
										(((~d & s) << 1)
										+ ((~d ^ s) & 0xfefefefe)) & 0x01010100;
									uint32_t msk = tmp - (tmp >> 8);
									dstRow[dxx] = (d - s + tmp) & ~msk;
								}
							}
						}else{
							// --- Alpha blend (mAlphaTrans) ---
							for(int dxx = clipX0; dxx < clipX1; dxx++){
								int srcX = (destW == glyphW)
									? (dxx - screenX)
									: ((dxx - screenX) * glyphW) / destW;
								uint8_t idx = srcRow[srcX];
								if(idx != 0){
									uint32_t color = ppal[idx];
									uint32_t d = dstRow[dxx];
									uint64_t tmpd =
										((uint64_t)(d&0xff0000) << 16)
										| ((uint64_t)(d&0xff00) << 8)
										| (uint64_t)(d&0xff);
									uint64_t tmps =
										((uint64_t)(color&0xff0000) << 16)
										| ((uint64_t)(color&0xff00) << 8)
										| (uint64_t)(color&0xff);
									tmpd *= ckDst;
									tmps *= ckSrc;
									uint64_t t2 =
										((tmpd & tmps)
										+ (((tmpd ^ tmps) >> 1) & 0x7fff7fff7fff7fffLL))
										& 0x8000800080008000LL;
									uint64_t m2 = (t2 << 1) - (t2 >> 15);
									tmpd = ((tmpd + tmps) - m2) | m2;
									dstRow[dxx] =
										(uint32_t)((tmpd&0xff0000000000LL)>>24
										| (tmpd&0xff000000LL)>>16
										| (tmpd&0xff00LL)>>8);
								}
							}
						}
					}
				}
			}
		}

		dx += glyphW * absXscl + spacing;
	}

	QueryPerformanceCounter(&t1);
	g_perfCounters.spriteTimeUs += qpcElapsedUs(t0, t1, g_perfCounters.qpcFreq);
	return true;
}

template<typename Img> void mzlShadowLoop(
	uint32_t* pdpx, Img& pri, uint32_t color, uint32_t alpha,
	int xsign, int ix, int dxend, int ifx, int ixcl, int sx)
{
	Img tmppri = pri;
	tmppri.skip(sx);
	if(
		xsign*dxend
		> xsign*((ifx + ((tmppri.width-1) - tmppri.currentx)*ixcl) >> 16))
	{
		if(sx != 0 && tmppri.currentx <= 0) return;
		if(-65536 <= ixcl && ixcl <= 65536){
			for(;;){
				int n = (int)(ix == ifx>>16) - 1 & xsign;
				if(n != 0 && tmppri.color != 0){
					mShadowTrans(pdpx[ix], color, alpha);
				}
				ix += n;
				tmppri.nextPixel();
				if(tmppri.currentx <= 0) return;
				ifx += ixcl;
			}
		}else{
			for(;;){
				if(ix == ifx>>16){
					tmppri.nextPixel();
					if(tmppri.currentx <= 0) return;
					ifx += ixcl;
				}
				if(tmppri.color != 0){
					mShadowTrans(pdpx[ix], color, alpha);
				}
				ix += xsign;
			}
		}
	}else{
		if(-65536 <= ixcl && ixcl <= 65536){
			for(;;){
				int n = (int)(ix == ifx>>16) - 1 & xsign;
				if(n != 0 && tmppri.color != 0){
					mShadowTrans(pdpx[ix], color, alpha);
				}
				if((ix += n) == dxend) return;
				tmppri.nextPixel();
				ifx += ixcl;
			}
		}else{
			for(;;){
				if(ix == ifx>>16){
					tmppri.nextPixel();
					ifx += ixcl;
				}
				if(tmppri.color != 0){
					mShadowTrans(pdpx[ix], color, alpha);
				}
				if((ix += xsign) == dxend) return;
			}
		}
	}
}

template<typename Img> void mzShadowLineBilt(
	typename Funcs<Img>::mzlslporc loop, uint32_t* pdpx, SDL_Rect& dr,
	float fx, Img& pri, uint32_t color, float xscl, uint32_t alpha)
{
	if(
		abs(fx) > 1.0e5f || abs(xscl) < 0.001f
		|| abs((float)pri.width * xscl) + (float)g_w > 32767.0f)
	{
		return;
	}
	int xsign = (xscl < 0.0f ? -1 : 1);
	int ix;
	int sx = 0;
	int dxend;
	if(xsign < 0){
		dxend = dr.x-1;
		ix = (int)floor(fx);
		if(ix > (int)dr.x+dr.w-1){
			ix = (int)dr.x+dr.w-1;
			float n = floor(((float)ix-floor(fx))/xscl);
			fx += n*xscl;
			if(floor(fx+xscl) > (float)ix){
				fx += xscl;
				n += 1.0f;
			}
			sx += (int)n;
			if(sx >= pri.width) return;
		}
	}else{
		dxend = dr.x+dr.w;
		ix = (int)floor(fx);
		if(ix < dr.x){
			ix = dr.x;
			float n = floor(((float)ix-floor(fx))/xscl);
			fx += n*xscl;
			if(floor(fx+xscl) < (float)ix){
				fx += xscl;
				n += 1.0f;
			}
			sx += (int)n;
			if(sx >= pri.width) return;
		}
	}
	if(xsign*ix >= xsign*dxend) return;
	fx += xscl;
	int ifx = (int)floor(fx*65536.0f);
	int ixcl = (int)(xscl*65536.0f);
	loop(pdpx, pri, color, alpha, xsign, ix, dxend, ifx, ixcl, sx);
}

template<int sign> void inclrxyShadow(
	int& rx, int& ry, const Zurashi* xzt, uint8_t xztofs, uint32_t roto,
	int vscl)
{
	int xp1 = (roto+256)>>9 & 1;
	int xmask = (int)(xp1 == 0) - 1;
	int yp1 = roto>>9 & 1;
	int ymask = (int)(yp1 == 0) - 1;
	int dx, dy;
	getdxdy(dx, dy, xzt, xztofs, roto);
	if((roto & 0x100) != 0){
		ry += sign*((dx^xmask) + xp1)*vscl;
		rx += sign*((dy^ymask) + yp1);
	}else{
		rx += sign*((dx^xmask) + xp1);
		ry += sign*((dy^ymask) + yp1)*vscl;
	}
}

template<typename Img> void mzrlShadowLoop(
	uint32_t* pdpx, int dstw, int rx, int ry, Img pri, uint32_t color,
	uint32_t roto, bool biltflg, uint32_t alpha, int rxsrt, int rxend,
	int rysrt, int ryend, int rxlimmask, int rylimmask, int ifx,
	int ixcl, int ivcl)
{
	const Zurashi* xzt =
		RotoZurasiTable[(roto&0x80) == 0 ? roto & 0x7f : 128 - (roto & 0x7f)];
	uint8_t xztofs = 0;
	int ix = ifx>>16;
	ifx += ixcl;
	int tx = 1;
	if(ixcl > 0){
		if(ixcl < 65536){
			for(;;){
				if(ix >= ifx>>16 && tx > 0){
					pri.nextPixel();
					tx = pri.currentx;
					ifx += ixcl;
				}else{
					if(
						tx <= 0 || (rx^rxlimmask)+(rxlimmask&1) >= rxsrt
						&& (ry>>16^rylimmask)+(rylimmask&1) >= rysrt)
					{
						break;
					}
					xztofs++;
					inclrxyShadow<1>(rx, ry, xzt, xztofs, roto, ivcl);
					ix++;
				}
			}
			for(;;){
				if(ix >= ifx>>16 && tx > 0){
					pri.nextPixel();
					tx = pri.currentx;
					ifx += ixcl;
				}else{
					if(
						tx <= 0 || (rx^rxlimmask)+(rxlimmask&1) >= rxend
						|| (ry>>16^rylimmask)+(rylimmask&1) >= ryend)
					{
						return;
					}
					if(
						pri.color != 0 && abs(ry - ((ry>>16)<<16)) <= abs(ivcl)
						&& (
							biltflg
							|| (xzt[xztofs].dy == 0) == (xzt[xztofs].dx == 0))
						&& (xzt[xztofs].dy != 0 || xzt[xztofs].dx != 0))
					{
						mShadowTrans(pdpx[rx + (ry>>16)*dstw], color, alpha);
					}
					xztofs++;
					inclrxyShadow<1>(rx, ry, xzt, xztofs, roto, ivcl);
					ix++;
				}
			}
		}else{
			for(;;){
				if(
					tx <= 0 || (rx^rxlimmask)+(rxlimmask&1) >= rxsrt
					&& (ry>>16^rylimmask)+(rylimmask&1) >= rysrt)
				{
					break;
				}
				xztofs++;
				inclrxyShadow<1>(rx, ry, xzt, xztofs, roto, ivcl);
				if(++ix >= ifx>>16){
					pri.nextPixel();
					tx = pri.currentx;
					ifx += ixcl;
				}
			}
			for(;;){
				if(
					tx <= 0 || (rx^rxlimmask)+(rxlimmask&1) >= rxend
					|| (ry>>16^rylimmask)+(rylimmask&1) >= ryend)
				{
					return;
				}
				if(
					pri.color != 0 && abs(ry - ((ry>>16)<<16)) <= abs(ivcl)
					&& (
						biltflg
						|| (xzt[xztofs].dy == 0) == (xzt[xztofs].dx == 0))
					&& (xzt[xztofs].dy != 0 || xzt[xztofs].dx != 0))
				{
					mShadowTrans(pdpx[rx + (ry>>16)*dstw], color, alpha);
				}
				xztofs++;
				inclrxyShadow<1>(rx, ry, xzt, xztofs, roto, ivcl);
				if(++ix >= ifx>>16){
					pri.nextPixel();
					tx = pri.currentx;
					ifx += ixcl;
				}
			}
		}
	}else{
		if(ixcl > -65536){
			for(;;){
				if(ix <= ifx>>16 && tx > 0){
					pri.nextPixel();
					tx = pri.currentx;
					ifx += ixcl;
				}else{
					if(
						tx <= 0 || (rx^rxlimmask)+(rxlimmask&1) >= rxsrt
						&& (ry>>16^rylimmask)+(rylimmask&1) >= rysrt)
					{
						break;
					}
					xztofs--;
					inclrxyShadow<-1>(rx, ry, xzt, xztofs, roto, ivcl);
					ix--;
				}
			}
			for(;;){
				if(ix <= ifx>>16 && tx > 0){
					pri.nextPixel();
					tx = pri.currentx;
					ifx += ixcl;
				}else{
					if(
						tx <= 0 || (rx^rxlimmask)+(rxlimmask&1) >= rxend
						|| (ry>>16^rylimmask)+(rylimmask&1) >= ryend)
					{
						return;
					}
					xztofs--;
					if(
						pri.color != 0 && abs(ry - ((ry>>16)<<16)) <= abs(ivcl)
						&& (
							biltflg
							|| (xzt[xztofs].dy == 0) == (xzt[xztofs].dx == 0))
						&& (xzt[xztofs].dy != 0 || xzt[xztofs].dx != 0))
					{
						mShadowTrans(pdpx[rx + (ry>>16)*dstw], color, alpha);
					}
					inclrxyShadow<-1>(rx, ry, xzt, xztofs, roto, ivcl);
					ix--;
				}
			}
		}else{
			for(;;){
				if(
					tx <= 0 || (rx^rxlimmask)+(rxlimmask&1) >= rxsrt
					&& (ry>>16^rylimmask)+(rylimmask&1) >= rysrt)
				{
					break;
				}
				xztofs--;
				inclrxyShadow<-1>(rx, ry, xzt, xztofs, roto, ivcl);
				if(--ix <= ifx>>16){
					pri.nextPixel();
					tx = pri.currentx;
					ifx += ixcl;
				}
			}
			for(;;){
				if(
					tx <= 0 || (rx^rxlimmask)+(rxlimmask&1) >= rxend
					|| (ry>>16^rylimmask)+(rylimmask&1) >= ryend)
				{
					return;
				}
				xztofs--;
				if(
					pri.color != 0 && abs(ry - ((ry>>16)<<16)) <= abs(ivcl)
					&& (
						biltflg
						|| (xzt[xztofs].dy == 0) == (xzt[xztofs].dx == 0))
					&& (xzt[xztofs].dy != 0 || xzt[xztofs].dx != 0))
				{
					mShadowTrans(pdpx[rx + (ry>>16)*dstw], color, alpha);
				}
				inclrxyShadow<-1>(rx, ry, xzt, xztofs, roto, ivcl);
				if(--ix <= ifx>>16){
					pri.nextPixel();
					tx = pri.currentx;
					ifx += ixcl;
				}
			}
		}
	}
}

template<typename Img> void mzrShadowLineBilt(
	typename Funcs<Img>::mrlslporc loop, uint32_t* pdpx, int dstw,
	int rx, int ry, int xlim, int ylim, float fx, Img& pri, uint32_t color,
	float xscl, float vscl, uint32_t roto, bool biltflg, uint32_t alpha)
{
	if(abs(fx) > 16383.0f) return;
	int rxsrt, rxend, rysrt, ryend;
	int rxlimmask, rylimmask;
	if(xscl < 0.0f){
		if(roto < 256){
			if(roto == 0 && ry < 0) return;
			rxsrt = -xlim+1; rxend = 1;    rxlimmask = -1;
			rysrt = 0;       ryend = ylim; rylimmask = 0;
		}else if(roto < 512){
			if(roto == 256 && rx < 0) return;
			rxsrt = 0;       rxend = xlim; rxlimmask = 0;
			rysrt = 0;       ryend = ylim; rylimmask = 0;
		}else if(roto < 768){
			if(roto == 512 && ry>>16 >= ylim) return;
			rxsrt = 0;       rxend = xlim; rxlimmask = 0;
			rysrt = -ylim+1; ryend = 1;    rylimmask = -1;
		}else{
			if(roto == 768 && rx >= xlim) return;
			rxsrt = -xlim+1; rxend = 1;    rxlimmask = -1;
			rysrt = -ylim+1; ryend = 1;    rylimmask = -1;
		}
	}else{
		if(roto < 256){
			if(roto == 0 && ry>>16 >= ylim) return;
			rxsrt = 0;       rxend = xlim; rxlimmask = 0;
			rysrt = -ylim+1; ryend = 1;    rylimmask = -1;
		}else if(roto < 512){
			if(roto == 256 && rx >= xlim) return;
			rxsrt = -xlim+1; rxend = 1;    rxlimmask = -1;
			rysrt = -ylim+1; ryend = 1;    rylimmask = -1;
		}else if(roto < 768){
			if(roto == 512 && ry < 0) return;
			rxsrt = -xlim+1; rxend = 1;    rxlimmask = -1;
			rysrt = 0;       ryend = ylim; rylimmask = 0;
		}else{
			if(roto == 768 && rx < 0) return;
			rxsrt = 0;       rxend = xlim; rxlimmask = 0;
			rysrt = 0;       ryend = ylim; rylimmask = 0;
		}
	}
	int ifx = (int)floor(fx*65536.0f);
	int ixcl = (int)(xscl*65536.0f);
	int ivcl = (int)(vscl*65536.0f);
	loop(
		pdpx, dstw, rx, ry, pri, color, roto, biltflg, alpha,
		rxsrt, rxend, rysrt, ryend, rxlimmask, rylimmask, ifx, ixcl, ivcl);
}

template<typename Img> void mzShadowScreenBilt(
	typename Funcs<Img>::mzlslporc loop, SDL_Rect& dr,
	Img pri, uint32_t color, SDL_Rect& srcr,
	float fx, float fy, float xscl, float yscl, uint32_t alpha)
{
	if(abs(yscl) < 0.001f) return;
	if(dr.x < 0){
		dr.w += dr.x;
		dr.x = 0;
	}
	if((int)dr.x+dr.w > g_w) dr.w -= dr.x+dr.w - g_w;
	if((int16_t)dr.w <= 0) return;
	if(dr.y < 0){
		dr.h += dr.y;
		dr.y = 0;
	}
	if((int)dr.y+dr.h > g_h) dr.h -= dr.y+dr.h - g_h;
	if((int16_t)dr.h <= 0) return;
	int dstw = g_pitch / sizeof(uint32_t);
	int ysign = yscl < 0.0f ? -1 : 1;
	fy += yscl < 0.0f ? -0.5f : 0.5f;
	fx += xscl < 0.0f ? -0.5f : 0.5f;
	int iy = (int)floor(fy);
	if((iy < dr.y && ysign < 0) || (iy >= dr.h && ysign > 0)) return;
	while(iy < dr.y || iy >= dr.h){
		pri.nextLine();
		if(pri.currenty >= srcr.h || pri.finished()) return;
		fy += yscl;
		iy = (int)floor(fy);
	}
	uint32_t* pdpx = g_pix + dstw*iy;
	fy += yscl;
	while(iy >= dr.y && iy < dr.h){
		while(iy == (int)floor(fy)){
			pri.nextLine();
			if(pri.currenty >= srcr.h || pri.finished()) return;
			fy += yscl;
		}
		mzShadowLineBilt(loop, pdpx, dr, fx, pri, color, xscl, alpha);
		iy += ysign;
		pdpx = g_pix + iy*dstw;
	}
}

template<typename Img> void mzrShadowScreenBilt(
	typename Funcs<Img>::mrlslporc loop, float rcx,
	float rcy, Img& pri, uint32_t color, SDL_Rect& srcr, float fx, float fy,
	float xscl, float yscl, float vscl, uint32_t roto, uint32_t alpha)
{
	if(vscl < 0.0f){
		vscl *= -1;
		yscl *= -1;
		roto = (0 - roto) & 0x3ff;
	}
	if(yscl < 0.0f){
		xscl *= -1.0f;
		yscl *= -1.0f;
		roto = roto + 512 & 0x3ff;
	}
	const Zurashi* yzt =
		RotoZurasiTable[
			(roto-256 & 0x80) == 0
			? roto-256 & 0x7f : 128 - (roto-256 & 0x7f)];
	uint8_t yztofs = 0;
	uint32_t* pdpx = g_pix;
	int dstw = g_pitch / sizeof(uint32_t);
	int xlim = g_w;
	int ylim = g_h;
	float tmpx = fx = rcx + (xscl < 0.0f ? fx : -fx);
	float tmpy = rcy - fy*vscl;
	fy = rcy - fy;
	kaiten(tmpx, tmpy, -((float)PI*(float)roto/512.0f), rcx, rcy, vscl);
	int rx = (int)floor(tmpx + 0.5f), ry = (int)floor(tmpy*65536 + 0.5f);
	bool kakudoToKa = (int32_t)((roto-256)<<(31-8))>>31 == 0;
	int xmul = (roto>>9 & 1) == 0 ? 1 : -1;
	int ymul = ((roto-256)>>9 & 1) == 0 ? 1 : -1;
	int ivscl = (int)(vscl*65536);
	fx += xscl < 0.0 ? -0.5f : 0.5f;
	fy += 0.5f;
	int iy = (int)floor(fy);
	fy += yscl;
	int tmpdx = 0, tmpdy = 1;
	if(kakudoToKa){
		ry -= tmpdy*ymul * ivscl;
	}else{
		rx -= tmpdy*ymul;
	}
	for(;;){
		while(iy == (int)floor(fy)){
			pri.nextLine();
			if(pri.currenty >= srcr.h || pri.finished()) return;
			fy += yscl;
		}
		if(tmpdx != 0){
			if(kakudoToKa){
				rx += tmpdx*xmul;
			}else{
				ry += tmpdx*xmul * ivscl;
			}
			mzrShadowLineBilt(
				loop, pdpx, dstw, rx, ry, xlim, ylim, fx,
				pri, color, xscl, vscl, roto, tmpdy == 0, alpha);
		}
		if(tmpdy != 0){
			if(kakudoToKa){
				ry += tmpdy*ymul * ivscl;
			}else{
				rx += tmpdy*ymul;
			}
			mzrShadowLineBilt(
				loop, pdpx, dstw, rx, ry, xlim, ylim, fx,
				pri, color, xscl, vscl, roto, true, alpha);
		}
		getdxdy(tmpdx, tmpdy, yzt, yztofs, roto-256);
		yztofs++;
		iy++;
	}
}

void mShadowRender(
	SDL_Rect dr, float rcx, float rcy, Reference img,
	uint32_t color, SDL_Rect srcr, float cx, float ty,
	float xscl, float yscl, float vscl,
	uint32_t roto, uint32_t alpha, int rle, Reference* pluginbuf)
{
	roto &= 0x3ff;
	alpha = 256 - alpha;
	if(roto == 0){
		if(xscl >= 0) cx = -cx;
		cx += rcx;
		if(yscl*vscl >= 0) ty = -ty;
		ty = rcy + ty * abs(vscl);
	}
	if(rle > 0){
		PcxRleImg pri;
		pri.setImg(img, srcr.w, rle, pluginbuf);
		if(roto == 0){
			mzShadowScreenBilt(
				mzlShadowLoop<PcxRleImg>, dr, pri, color,
				srcr, cx, ty, xscl, yscl*vscl, alpha);
		}else{
			mzrShadowScreenBilt(
				mzrlShadowLoop<PcxRleImg>, rcx, rcy, pri, color,
				srcr, cx, ty, xscl, yscl, vscl, roto, alpha);
		}
	}else{
		PalletColorImg pri;
		pri.setImg(img, srcr.w);
		if(roto == 0){
			mzShadowScreenBilt(
				mzlShadowLoop<PalletColorImg>, dr, pri,
				color, srcr, cx, ty, xscl, yscl*vscl, alpha);
		}else{
			mzrShadowScreenBilt(
				mzrlShadowLoop<PalletColorImg>, rcx, rcy, pri,
				color, srcr, cx, ty, xscl, yscl, vscl, roto, alpha);
		}
	}
}

bool SSZ_STDCALL RenderMugenShadow(Reference* pluginbuf, int32_t rle,
	float rcy, float rcx, SDL_Rect* pdstr, int32_t alpha,
	uint32_t roto, float vscl, float yscl, float xscl,
	float ty, float cx, SDL_Rect* psrcr, uint32_t color, Reference img)
{
	LARGE_INTEGER t0, t1;
	QueryPerformanceCounter(&t0);
	g_perfCounters.spriteCount++;
	g_perfCounters.drawCalls++;
	g_perfCounters.shadowCount++;
	if(
		img.len() == 0
		|| _finite(cx+ty+rcx+rcy+xscl+vscl+yscl) == 0
		|| abs(rcx) > 1.0e5f || abs(rcy) > 1.0e5f
		|| abs(cx) > 1.0e5f || abs(ty) > 1.0e5f
		|| abs(xscl) > 16383.0f
		|| abs(yscl) > 16383.0f || abs(vscl) > 16383.0f) return false;
	mShadowRender(
		*pdstr, rcx, rcy, img, color, *psrcr, cx, ty,
		xscl, yscl, vscl, roto, alpha, rle, pluginbuf);
	QueryPerformanceCounter(&t1);
	g_perfCounters.shadowTimeUs += qpcElapsedUs(t0, t1, g_perfCounters.qpcFreq);
	return true;
}

uint32_t SSZ_STDCALL Load8bitTexture(int32_t h, int32_t w, uint8_t* ppxl)
{
	// ASSET_LOG("Loading 8-bit texture: %dx%d", w, h);
	uint32_t texid;
	glGenTextures(1, &texid);
	glBindTexture(GL_TEXTURE_2D, texid);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(
		GL_TEXTURE_2D, 0, GL_LUMINANCE,
		w, h, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, ppxl);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	return texid;
}

uint32_t SSZ_STDCALL LoadPngTexture(FILE* fp, int32_t* h, int32_t* w)
{
	*w = *h = 0;
	if(!fp) return 0;
	uint8_t header[8] = {0};
	fread(header, 1, 8, fp);
	if(png_sig_cmp(header, 0, 8)) return 0;
	auto png_ptr =
		png_create_read_struct(
			PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
	if(!png_ptr) return 0;
	auto info_ptr = png_create_info_struct(png_ptr);
	if(!info_ptr){
		png_destroy_read_struct(&png_ptr, nullptr, nullptr);
		return 0;
	}
	png_init_io(png_ptr, fp);
	png_set_sig_bytes(png_ptr, 8);
	png_read_info(png_ptr, info_ptr);
	png_uint_32 width, height;
	int bit_depth, color_type;
	uint32_t texid = 0;
	if(
		png_get_IHDR(
			png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,
			nullptr, nullptr, nullptr))
	{
		if(bit_depth > 8) png_set_strip_16(png_ptr);
		png_set_expand(png_ptr);
		if((color_type & PNG_COLOR_MASK_ALPHA) == 0){
			png_set_add_alpha(png_ptr, 0xFF, PNG_FILLER_AFTER);
		}
		*w = width;
		*h = height;
		auto buff = new png_byte[width * height * 4];
		auto p = buff;
		auto pp = new png_bytep[height];
		for(int i = height-1; i >= 0; i--) pp[i] = p + width*i*4;
		png_read_image(png_ptr, pp);
		delete [] pp;
		glGenTextures(1, &texid);
		glBindTexture(GL_TEXTURE_2D, texid);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexImage2D(
			GL_TEXTURE_2D, 0, GL_RGBA,
			width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, buff);
		delete [] buff;
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	}
	png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
	return texid;
}

void SSZ_STDCALL DeleteGlTexture(uint32_t texid)
{
	if(texid != 0) glDeleteTextures(1, &texid);
}

void SSZ_STDCALL GlSwapBuffers()
{
	// End previous frame perf tracking
	if (g_perfMonitorEnabled)
	{
		uint32_t now = SDL_GetTicks();
		perfFrameEnd(g_perfCounters, now);
		perfPrintFrame(g_perfCounters, g_rendererInfo);
	}

	SDL_GL_SwapWindow(g_window);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	// [OPT] Set up ortho projection once per frame instead of per sprite.
	// All render calls (RenderMugenGl, RenderMugenGlFc, RenderMugenGlFcS,
	// MugenFillGl) use the same projection: glOrtho(0, g_w, 0, g_h, -1, 1).
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, g_w, 0, g_h, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	g_orthoProjectionSet = true;
	// Begin next frame perf tracking
	if (g_perfMonitorEnabled)
	{
		g_perfCounters.frameStartTick = SDL_GetTicks();
		perfFrameBegin(g_perfCounters);
	}
}

bool SSZ_STDCALL InitMugenGl()
{
	const GLchar* vertShader =
		"void main(void){"
			"gl_TexCoord[0] = gl_TextureMatrix[0] * gl_MultiTexCoord0;"
			"gl_Position = ftransform();"
		"}";
	const GLchar* fragShader =
		"uniform float a;"
		"uniform sampler2D tex;"
		"uniform sampler1D pal;"
		"uniform int msk;"
		"void main(void){"
		"float r = texture2D(tex, gl_TexCoord[0].st).r;"
		"vec4 c;"
		"gl_FragColor ="
		"int(255.0*r) == msk ? vec4(0.0)"
		": (c = texture1D(pal, r*0.9961), vec4(c.b, c.g, c.r, a));"
		"}";
	const GLchar* fragShaderFc =
		"uniform float a;"
		"uniform sampler2D tex;"
		"uniform bool neg;"
		"uniform float gray;"
		"uniform vec3 add;"
		"uniform vec3 mul;"
		"void main(void){"
		"vec4 c = texture2D(tex, gl_TexCoord[0].st);"
		"if(neg) c.rgb = vec3(1.0) - c.rgb;"
		"c.rgb += (vec3((c.r + c.g + c.b) / 3.0) - c.rgb) * gray + add;"
		"c.rgb *= mul;"
		"c.a *= a;"
		"gl_FragColor = c;"
		"}";
	const GLchar* fragShaderFcS =
		"uniform float a;"
		"uniform sampler2D tex;"
		"uniform vec3 color;"
		"void main(void){"
		"vec4 c = texture2D(tex, gl_TexCoord[0].st);"
		"c.rgb = color * c.a;"
		"c.a *= a;"
		"gl_FragColor = c;"
		"}";
	if(
		!GLEW_ARB_shader_objects || !GLEW_ARB_vertex_shader
		|| !GLEW_ARB_fragment_shader
		|| g_mugenshader != 0 || g_mugenshaderFc != 0 || g_mugenshaderFcS != 0)
	{
		return false;
	}
	g_mugenshader = glCreateProgramObjectARB();
	g_mugenshaderFc = glCreateProgramObjectARB();
	g_mugenshaderFcS = glCreateProgramObjectARB();
	GLhandleARB hVertShaderObject = glCreateShaderObjectARB(GL_VERTEX_SHADER);
	GLhandleARB hFragShaderObject =
		glCreateShaderObjectARB(GL_FRAGMENT_SHADER);
	int vert_compiled = 0;
	int frag_compiled = 0;
	int linked = 0;
	GLint length;
	length = strlen((char*)vertShader);
	glShaderSourceARB(
		hVertShaderObject, 1, (const GLchar**)&vertShader, &length);
	glCompileShaderARB(hVertShaderObject);
	glGetObjectParameterivARB(
		hVertShaderObject, GL_OBJECT_COMPILE_STATUS_ARB, &vert_compiled);
	if(vert_compiled == 0) goto fail;

	length = strlen((char*)fragShader);
	glShaderSourceARB(
		hFragShaderObject, 1, (const GLchar**)&fragShader, &length);
	glCompileShaderARB(hFragShaderObject);
	glGetObjectParameterivARB(
		hFragShaderObject, GL_OBJECT_COMPILE_STATUS_ARB, &frag_compiled);
	if(frag_compiled == 0) goto fail;
	glAttachObjectARB(g_mugenshader, hVertShaderObject);
	glAttachObjectARB(g_mugenshader, hFragShaderObject);
	glLinkProgramARB(g_mugenshader);
	glGetObjectParameterivARB(
		g_mugenshader, GL_OBJECT_LINK_STATUS_ARB, &linked);
	if(linked == 0) goto fail;
	g_uniformPal = glGetUniformLocationARB(g_mugenshader, "pal");
	g_uniformMsk = glGetUniformLocationARB(g_mugenshader, "msk");
	// [OPT] Cache "a" uniform at init time instead of per-draw lookup
	g_uniformAlpha = glGetUniformLocationARB(g_mugenshader, "a");

	glDeleteObjectARB(hFragShaderObject);
	hFragShaderObject =
		glCreateShaderObjectARB(GL_FRAGMENT_SHADER);
	length = strlen((char*)fragShaderFc);
	glShaderSourceARB(
		hFragShaderObject, 1, (const GLchar**)&fragShaderFc, &length);
	glCompileShaderARB(hFragShaderObject);
	glGetObjectParameterivARB(
		hFragShaderObject, GL_OBJECT_COMPILE_STATUS_ARB, &frag_compiled);
	if(frag_compiled == 0) goto fail;
	glAttachObjectARB(g_mugenshaderFc, hVertShaderObject);
	glAttachObjectARB(g_mugenshaderFc, hFragShaderObject);
	glLinkProgramARB(g_mugenshaderFc);
	glGetObjectParameterivARB(
		g_mugenshaderFc, GL_OBJECT_LINK_STATUS_ARB, &linked);
	if(linked == 0) goto fail;
	g_uniformNeg = glGetUniformLocationARB(g_mugenshaderFc, "neg");
	g_uniformGray = glGetUniformLocationARB(g_mugenshaderFc, "gray");
	g_uniformAdd = glGetUniformLocationARB(g_mugenshaderFc, "add");
	g_uniformMul = glGetUniformLocationARB(g_mugenshaderFc, "mul");
	// [OPT] Cache "a" uniform at init time instead of per-draw lookup
	g_uniformAlphaFc = glGetUniformLocationARB(g_mugenshaderFc, "a");

	glDeleteObjectARB(hFragShaderObject);
	hFragShaderObject =
		glCreateShaderObjectARB(GL_FRAGMENT_SHADER);
	length = strlen((char*)fragShaderFcS);
	glShaderSourceARB(
		hFragShaderObject, 1, (const GLchar**)&fragShaderFcS, &length);
	glCompileShaderARB(hFragShaderObject);
	glGetObjectParameterivARB(
		hFragShaderObject, GL_OBJECT_COMPILE_STATUS_ARB, &frag_compiled);
	if(frag_compiled == 0) goto fail;
	glAttachObjectARB(g_mugenshaderFcS, hVertShaderObject);
	glAttachObjectARB(g_mugenshaderFcS, hFragShaderObject);
	glLinkProgramARB(g_mugenshaderFcS);
	glGetObjectParameterivARB(
		g_mugenshaderFcS, GL_OBJECT_LINK_STATUS_ARB, &linked);
	if(linked == 0) goto fail;
	g_uniformColor = glGetUniformLocationARB(g_mugenshaderFcS, "color");
	// [OPT] Cache "a" uniform at init time instead of per-draw lookup
	g_uniformAlphaFcS = glGetUniformLocationARB(g_mugenshaderFcS, "a");

	glDeleteObjectARB(hVertShaderObject);
	glDeleteObjectARB(hFragShaderObject);
	// [OPT] Set persistent GL state that never changes during rendering.
	// Depth testing is always disabled for 2D sprite rendering.
	// Blending is always required. Setting once avoids per-call overhead.
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	return true;

fail:
	glDeleteObjectARB(g_mugenshader);
	glDeleteObjectARB(g_mugenshaderFc);
	glDeleteObjectARB(g_mugenshaderFcS);
	g_mugenshader = g_mugenshaderFc = g_mugenshaderFcS = 0;
	glDeleteObjectARB(hVertShaderObject);
	glDeleteObjectARB(hFragShaderObject);
	return false;
}

void drawQuads(
	float x1, float y1, float x2, float y2, float x3, float y3,
	float x4, float y4, float r, float g, float b, float a, float pers)
{
	glColor4f(r, g, b, a);
	glBegin(GL_TRIANGLE_STRIP);
	glTexCoord2f(0, 1);
	glVertex2f(x1, y1);
	glTexCoord2f(0, 0);
	glVertex2f(x4, y4);
	int n =
		(int)(
			(
				pers > 1.0
				? (1-1/(pers*pers))*abs(x3-x4) : (1-(pers*pers))*abs(x1-x2))
			* (g_h>>5) / (abs(y1-y4) + (g_h>>5)));
	for(int i = 1; i < n; i++){
		glTexCoord2f((float)i/n, 1);
		glVertex2f(x1 + (x2 - x1)*i/n, y1 + (y2 - y1)*i/n);
		glTexCoord2f((float)i/n, 0);
		glVertex2f(x4 + (x3 - x4)*i/n, y4 + (y3 - y4)*i/n);
	}
	glTexCoord2f(1, 1);
	glVertex2f(x2, y2);
	glTexCoord2f(1, 0);
	glVertex2f(x3, y3);
	glEnd();
}

void drawTileHolizon(
	float x1, float y1, float x2, float y2, float x3, float y3,
	float x4, float y4, float xtw, float xbw,
	float xtopscl, float xbotscl, SDL_Rect tile, float rcx,
	float r, float g, float b, float a, float pers)
{
	float topbtwn = xtw + xtopscl*(float)tile.x;
	float db =
		((xbw + xbotscl*(float)tile.x) - topbtwn) * (rcx - x4) / abs(topbtwn);
	x1 -= db;
	x2 -= db;
	if(tile.w == 1){
		float x1d=x1, x2d=x2, x3d=x3, x4d=x4;
		for(;;){
			x2d = x1d - xbotscl*(float)tile.x;
			x3d = x4d - xtopscl*(float)tile.x;
			x4d = x3d - xtw;
			x1d = x2d - xbw;
			if(abs(topbtwn) < 0.01) break;
			if(topbtwn < 0){
				if(
					x1d >= (float)g_w && x2d >= (float)g_w
					&& x3d >= (float)g_w && x4d >= (float)g_w) break;
			}else{
				if(x1d <= 0.0 && x2d <= 0.0 && x3d <= 0.0 && x4d <= 0.0) break;
			}
			if(
				(
					(0.0 < x1d || 0.0 < x2d)
					&& (x1d < (float)g_w || x2d < (float)g_w))
				|| (
					(0.0 < x3d || 0.0 < x4d)
					&& (x3d < (float)g_w || x4d < (float)g_w)))
			{
				drawQuads(
					x1d, y1, x2d, y2, x3d, y3, x4d, y4, r, g, b, a, pers);
			}
		}
	}
	int n = tile.w;
	for(;;){
		if(topbtwn > 0){
			if(
				x1 >= (float)g_w && x2 >= (float)g_w
				&& x3 >= (float)g_w && x4 >= (float)g_w) break;
		}else{
			if(x1 <= 0.0 && x2 <= 0.0 && x3 <= 0.0 && x4 <= 0.0) break;
		}
		if(
			((0.0 < x1 || 0.0 < x2) && (x1 < (float)g_w || x2 < (float)g_w))
			|| (
				(0.0 < x3 || 0.0 < x4)
				&& (x3 < (float)g_w || x4 < (float)g_w)))
		{
			drawQuads(x1, y1, x2, y2, x3, y3, x4, y4, r, g, b, a, pers);
		}
		if(tile.w != 1 && n != 0) n--;
		if(n == 0) break;
		x4 = x3 + xtopscl*(float)tile.x;
		x1 = x2 + xbotscl*(float)tile.x;
		x2 = x1 + xbw;
		x3 = x4 + xtw;
		if(abs(topbtwn) < 0.01) break;
	}
}

void drawTile(
	uint16_t w, uint16_t h, float x, float y,
	SDL_Rect tile, float xtopscl, float xbotscl,  float yscl, float vscl,
	float rasterxadd, float angle, float rcx, float rcy,
	float r, float g, float b, float a)
{
	float x1, y1, x2, y2, x3, y3, x4, y4;
	x1 = x + rasterxadd*yscl*(float)h;
	y1 = rcy + ((y - yscl*(float)h) - rcy) * vscl;
	x2 = x1 + xbotscl*(float)w;
	y2 = y1;
	x3 = x + xtopscl*(float)w;
	y3 = rcy + (y - rcy) * vscl;
	x4 = x;
	y4 = y3;
	float pers = abs(x3 - x4) / abs(x2 - x1);
	if(angle != 0.0f){
		kaiten(x1, y1, angle, rcx, rcy, vscl);
		kaiten(x2, y2, angle, rcx, rcy, vscl);
		kaiten(x3, y3, angle, rcx, rcy, vscl);
		kaiten(x4, y4, angle, rcx, rcy, vscl);
		drawQuads(x1, y1, x2, y2, x3, y3, x4, y4, r, g, b, a, pers);
	}else{
		if(tile.h == 1){
			float x1d=x1, y1d=y1, x2d=x2, y2d=y2, x3d=x3;
			float y3d=y3, x4d=x4, y4d=y4;
			for(;;){
				x1d = x4d;
				y1d = y4d + yscl*vscl*(float)tile.y;
				x2d = x3d;
				y2d = y1d;
				x3d =
					(x4d - rasterxadd*yscl*(float)h)
					+ (xtopscl/xbotscl)*(x3d - x4d);
				y3d = y2d + yscl*vscl*(float)h;
				x4d = x4d - rasterxadd*yscl*(float)h;
				if(abs(y3d - y4d) < 0.01) break;
				y4d = y3d;
				if(yscl*((float)h + (float)tile.y) < 0){
					if(y1d <= (float)-g_h && y4d <= (float)-g_h) break;
				}else{
					if(y1d >= 0.0 && y4d >= 0.0) break;
				}
				if(
					((0.0 > y1d || 0.0 > y4d)
					&& (y1d > (float)-g_h || y4d > (float)-g_h)))
				{
					drawTileHolizon(
						x1d, y1d, x2d, y2d, x3d, y3d, x4d, y4d,
						x3d-x4d, x2d-x1d, (x3d-x4d)/(float)w,
						(x2d-x1d)/(float)w, tile, rcx, r, g, b, a, pers);
				}
			}
		}
		int n = tile.h;
		for(;;){
			if(yscl*((float)h + (float)tile.y) > 0){
				if(y1 <= (float)-g_h && y4 <= (float)-g_h) break;
			}else{
				if(y1 >= 0.0 && y4 >= 0.0) break;
			}
			if(
				((0.0 > y1 || 0.0 > y4)
				&& (y1 > (float)-g_h || y4 > (float)-g_h)))
			{
				drawTileHolizon(
					x1, y1, x2, y2, x3, y3, x4, y4, x3-x4, x2-x1,
					(x3-x4)/(float)w, (x2-x1)/(float)w,
					tile, rcx, r, g, b, a, pers);
			}
			if(tile.h != 1 && n != 0) n--;
			if(n == 0) break;
			x4 = x1;
			y4 = y1 - yscl*vscl*(float)tile.y;
			x3 = x2;
			y3 = y4;
			x2 = x1 + rasterxadd*yscl*(float)h + (xbotscl/xtopscl)*(x2 - x1);
			y2 = y3 - yscl*vscl*(float)h;
			x1 = x1 + rasterxadd*yscl*(float)h;
			if(abs(y1 - y2) < 0.01) break;
			y1 = y2;
		}
	}
}

// [OPT] Added alphaUniform parameter - uses cached uniform location instead of
// calling glGetUniformLocation(shader, "a") on every alpha branch (was 6-7 lookups/sprite)
void renderMugenGl(
	float rcy, float rcx, int alpha, float angle, float rasterxadd,
	float vscl, float yscl, float xbotscl, float xtopscl, const SDL_Rect& tl,
	float y, float x, const SDL_Rect& r, GLhandleARB shader, GLint alphaUniform)
{
	// [OPT] Skip projection setup if already configured for this frame.
	// Saves 2 matrix push/pops + glOrtho + glMatrixMode switches per sprite.
	if(!g_orthoProjectionSet){
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0, g_w, 0, g_h, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	}
	glPushMatrix();
	glTranslated(0, g_h, 0);
	if (shader == g_mugenshaderFc) {
		drawQuads(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1);
	}
	if(alpha == -1){
		glUniform1fARB(alphaUniform, 1.0);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		drawTile(
			r.w, r.h, x, y, tl, xtopscl, xbotscl, yscl, vscl, rasterxadd,
			angle, rcx, rcy, 1, 1, 1, 1);
	}else if(alpha == -2){
		glUniform1fARB(alphaUniform, 1.0);
		glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
		drawTile(
			r.w, r.h, x, y, tl, xtopscl, xbotscl, yscl, vscl, rasterxadd,
			angle, rcx, rcy, 1, 1, 1, 1);
	}else if(alpha <= 0){
	}else if(alpha < 255){
		glUniform1fARB(alphaUniform, (GLfloat)alpha / 255.0f);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		drawTile(
			r.w, r.h, x, y, tl, xtopscl, xbotscl, yscl, vscl, rasterxadd,
			angle, rcx, rcy, 1, 1, 1, (GLfloat)alpha / 255.0f);
	}else if(alpha < 512){
		glUniform1fARB(alphaUniform, 1.0);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		drawTile(
			r.w, r.h, x, y, tl, xtopscl, xbotscl, yscl, vscl, rasterxadd,
			angle, rcx, rcy, 1, 1, 1, 1);
	}else{
		int src = alpha & 0xff;
		int dst = (alpha & 0x3fc00) >> 10;
		if(dst < 255){
			glUniform1fARB(alphaUniform, 1.0f - (GLfloat)dst / 255.0f);
			glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
			drawTile(
				r.w, r.h, x, y, tl, xtopscl, xbotscl, yscl, vscl, rasterxadd,
				angle, rcx, rcy, 1, 1, 1, 1.0f - (GLfloat)dst / 255.0f);
		}
		if(src > 0){
			glUniform1fARB(alphaUniform, (GLfloat)src / 255.0f);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
			drawTile(
				r.w, r.h, x, y, tl, xtopscl, xbotscl, yscl, vscl, rasterxadd,
				angle, rcx, rcy, 1, 1, 1, (GLfloat)src / 255.0f);
		}
	}
	glPopMatrix();
	// [OPT] Only pop projection if we pushed it (fallback path)
	if(!g_orthoProjectionSet){
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	}
}

bool SSZ_STDCALL RenderMugenGl(float rcy, float rcx, SDL_Rect* dstr, int alpha,
	float angle, float rasterxadd, float vscl, float yscl,
	float xbotscl, float xtopscl, SDL_Rect* tile, float y, float x,
	SDL_Rect* rect, int mask, uint8_t* ppal, uint32_t texid)
{
	g_perfCounters.spriteCount++;
	g_perfCounters.drawCalls++;
	g_perfCounters.textureBinds++;
	g_perfCounters.shaderSwitches++;
	if(
		texid == 0
		|| _finite(
			x+y+rcx+rcy+xtopscl+xbotscl+yscl+vscl+rasterxadd+angle) == 0)
	{
		return false;
	}
	if(vscl < 0.0f){
		vscl *= -1;
		yscl *= -1;
		angle *= -1;
	}
	SDL_Rect r = *rect, tl = *tile;
	if(tl.x > 0) tl.x -= r.w;
	if(tl.y > 0) tl.y -= r.h;
	if(tl.w == 0) tl.x = 0;
	if(tl.h == 0) tl.y = 0;
	if(xtopscl >= 0) x = -x;
	x += rcx;
	rcy = -rcy;
	if(yscl < 0) y = -y;
	y += rcy;
	glUseProgramObjectARB(g_mugenshader);
	glUniform1iARB(g_uniformPal, 1);
	glUniform1iARB(g_uniformMsk, mask);
	glEnable(GL_TEXTURE_1D);
	glEnable(GL_TEXTURE_2D);
	// [OPT] GL_DEPTH_TEST disabled persistently in InitMugenGl
	glEnable(GL_SCISSOR_TEST);
	glScissor(dstr->x, g_h - (dstr->y+dstr->h), dstr->w, dstr->h);
	glActiveTexture(GL_TEXTURE1);
	// [OPT] Reuse palette texture object via glTexSubImage1D instead of
	// delete+gen+create cycle every draw call. First call allocates, subsequent
	// calls only upload the 1 KB palette data without GPU-side reallocation.
	if (ppal) {
		if(!g_paltexInitialized) {
		glGenTextures(1, &g_paltex);
		glBindTexture(GL_TEXTURE_1D, g_paltex);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexImage1D(
			GL_TEXTURE_1D, 0, GL_RGBA, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, ppal);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			g_paltexInitialized = true;
		} else {
			glBindTexture(GL_TEXTURE_1D, g_paltex);
			glTexSubImage1D(
				GL_TEXTURE_1D, 0, 0, 256, GL_RGBA, GL_UNSIGNED_BYTE, ppal);
		}
	}
	else {
		glBindTexture(GL_TEXTURE_1D, g_paltex);
	}
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texid);
	renderMugenGl(
		rcy, rcx, alpha, angle, rasterxadd, vscl, yscl, xbotscl, xtopscl,
		tl, y, x, r, g_mugenshader, g_uniformAlpha);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_TEXTURE_1D);
	glUseProgramObjectARB(0);
	return true;
}

bool SSZ_STDCALL RenderMugenGlFc(float mulb, float mulg, float mulr,
	float addb, float addg, float addr, float color, bool neg,
	float rcy, float rcx, SDL_Rect* dstr, int alpha,
	float angle, float rasterxadd, float vscl, float yscl,
	float xbotscl, float xtopscl, SDL_Rect* tile, float y, float x,
	SDL_Rect* rect, uint32_t texid)
{
	g_perfCounters.spriteCount++;
	g_perfCounters.drawCalls++;
	g_perfCounters.textureBinds++;
	g_perfCounters.shaderSwitches++;
	if(
		texid == 0
		|| _finite(
			x+y+rcx+rcy+xtopscl+xbotscl+yscl+vscl+rasterxadd+angle) == 0)
	{
		return false;
	}
	if(vscl < 0.0f){
		vscl *= -1;
		yscl *= -1;
		angle *= -1;
	}
	SDL_Rect r = *rect, tl = *tile;
	if(tl.x > 0) tl.x -= r.w;
	if(tl.y > 0) tl.y -= r.h;
	if(tl.w == 0) tl.x = 0;
	if(tl.h == 0) tl.y = 0;
	if(xtopscl >= 0) x = -x;
	x += rcx;
	rcy = -rcy;
	if(yscl < 0) y = -y;
	y += rcy;
	glUseProgramObjectARB(g_mugenshaderFc);
	glUniform1iARB(g_uniformNeg, neg);
	glUniform1fARB(g_uniformGray, 1 - color);
	glUniform3fARB(g_uniformAdd, addr, addg, addb);
	glUniform3fARB(g_uniformMul, mulr, mulg, mulb);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, texid);
	// [OPT] GL_DEPTH_TEST disabled persistently in InitMugenGl
	glEnable(GL_SCISSOR_TEST);
	glScissor(dstr->x, g_h - (dstr->y+dstr->h), dstr->w, dstr->h);
	renderMugenGl(
		rcy, rcx, alpha, angle, rasterxadd, vscl, yscl, xbotscl, xtopscl,
		tl, y, x, r, g_mugenshaderFc, g_uniformAlphaFc);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_TEXTURE_2D);
	glUseProgramObjectARB(0);
	return true;
}

bool SSZ_STDCALL RenderMugenGlFcS(uint32_t color,
	float rcy, float rcx, SDL_Rect* dstr, int alpha,
	float angle, float rasterxadd, float vscl, float yscl,
	float xbotscl, float xtopscl, SDL_Rect* tile, float y, float x,
	SDL_Rect* rect, uint32_t texid)
{
	g_perfCounters.spriteCount++;
	g_perfCounters.drawCalls++;
	g_perfCounters.textureBinds++;
	g_perfCounters.shaderSwitches++;
	if(
		texid == 0
		|| _finite(
			x+y+rcx+rcy+xtopscl+xbotscl+yscl+vscl+rasterxadd+angle) == 0)
	{
		return false;
	}
	if(vscl < 0.0f){
		vscl *= -1;
		yscl *= -1;
		angle *= -1;
	}
	SDL_Rect r = *rect, tl = *tile;
	if(tl.x > 0) tl.x -= r.w;
	if(tl.y > 0) tl.y -= r.h;
	if(tl.w == 0) tl.x = 0;
	if(tl.h == 0) tl.y = 0;
	if(xtopscl >= 0) x = -x;
	x += rcx;
	rcy = -rcy;
	if(yscl < 0) y = -y;
	y += rcy;
	glUseProgramObjectARB(g_mugenshaderFcS);
	glUniform3fARB(
		g_uniformColor, (float)(color >> 16 & 0xff) / 255, (float)(color >> 8 & 0xff) / 255,
		(float)(color & 0xff) / 255);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, texid);
	// [OPT] GL_DEPTH_TEST disabled persistently in InitMugenGl
	glEnable(GL_SCISSOR_TEST);
	glScissor(dstr->x, g_h - (dstr->y+dstr->h), dstr->w, dstr->h);
	renderMugenGl(
		rcy, rcx, alpha, angle, rasterxadd, vscl, yscl, xbotscl, xtopscl,
		tl, y, x, r, g_mugenshaderFcS, g_uniformAlphaFcS);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_TEXTURE_2D);
	glUseProgramObjectARB(0);
	return true;
}

void rectFillGl(float r, float g, float b, float a, SDL_Rect rect)
{
	glBegin(GL_QUADS);
	{
		glColor4f(r, g, b, a);
		glVertex2f((float)rect.x, -(float)(rect.y+rect.h));
		glVertex2f((float)(rect.x+rect.w), -(float)(rect.y+rect.h));
		glVertex2f((float)(rect.x+rect.w), -(float)rect.y);
		glVertex2f((float)rect.x, -(float)rect.y);
	}
	glEnd();
}

void SSZ_STDCALL MugenFillGl(int32_t alpha, uint32_t color, SDL_Rect rect)
{
	g_perfCounters.fillCalls++;
	g_perfCounters.drawCalls++;
	float r = (float)(color>>16&0xff)/255.0f;
	float g = (float)(color>>8&0xff)/255.0f;
	float b = (float)(color&0xff)/255.0f;
	// [OPT] GL_DEPTH_TEST disabled persistently in InitMugenGl
	// [OPT] Skip projection setup if already configured for this frame
	if(!g_orthoProjectionSet){
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0, g_w, 0, g_h, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	}
	glPushMatrix();
	glTranslated(0, g_h, 0);
	if(alpha == -1){
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		rectFillGl(r, g, b, 1.0f, rect);
	}else if(alpha == -2){
		glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
		rectFillGl(r, g, b, 1.0f, rect);
	}else if(alpha <= 0){
	}else if(alpha < 255){
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		rectFillGl(r, g, b, (GLfloat)alpha / 256.0f, rect);
	}else if(alpha < 512){
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		rectFillGl(r, g, b, 1.0f, rect);
	}else{
		int src = alpha & 0xff;
		int dst = (alpha & 0x3fc00) >> 10;
		glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
		rectFillGl(r, g, b, 1.0f - (GLfloat)dst / 255.0f, rect);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		rectFillGl(r, g, b, (GLfloat)src / 255.0f, rect);
	}
	glPopMatrix();
	// [OPT] Only pop projection if we pushed it (fallback path)
	if(!g_orthoProjectionSet){
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	}
}

bool SSZ_STDCALL BindGlContext()
{
	return
		wglMakeCurrent(
			g_hdc,
			g_mainTreadId == GetCurrentThreadId() ? g_hglrc : g_hglrc2) != 0;
}

bool SSZ_STDCALL UnbindGlContext()
{
	return wglMakeCurrent(nullptr, nullptr) != 0;
}