#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ikemen {

// ── Enums ────────────────────────────────────────────────────────────────

enum class EventType : uint32_t {
	NOEVENT = 0, QUIT = 0x100, APP_TERMINATING, APP_LOWMEMORY,
	APP_WILLENTERBACKGROUND, APP_DIDENTERBACKGROUND,
	APP_WILLENTERFOREGROUND, APP_DIDENTERFOREGROUND,
	WINDOWEVENT = 0x200, SYSWMEVENT,
	KEYDOWN = 0x300, KEYUP, TEXTEDITING, TEXTINPUT,
	MOUSEMOTION = 0x400, MOUSEBUTTONDOWN, MOUSEBUTTONUP, MOUSEWHEEL,
	JOYAXISMOTION = 0x600, JOYBALLMOTION, JOYHATMOTION, JOYBUTTONDOWN, JOYBUTTONUP,
	JOYDEVICEADDED, JOYDEVICEREMOVED,
	CONTROLLERAXISMOTION = 0x650, CONTROLLERBUTTONDOWN, CONTROLLERBUTTONUP,
	CONTROLLERDEVICEADDED, CONTROLLERDEVICEREMOVED, CONTROLLERDEVICEREMAPPED,
	FINGERDOWN = 0x700, FINGERUP, FINGERMOTION,
	DOLLARGESTURE = 0x800, DOLLARRECORD, MULTIGESTURE,
	CLIPBOARDUPDATE = 0x900, DROPFILE = 0x1000,
	USEREVENT = 0x8000, LASTEVENT = 0xFFFF,
};

enum class SDLKey : int32_t {
	UNKNOWN=0, a=4,b=5,c=6,d=7,e=8,f=9,g=10,h=11,i=12,j=13,k=14,l=15,m=16,n=17,o=18,
	p=19,q=20,r=21,s=22,t=23,u=24,v=25,w=26,x=27,y=28,z=29,
	_1=30,_2=31,_3=32,_4=33,_5=34,_6=35,_7=36,_8=37,_9=38,_0=39,
	RETURN=40,ESCAPE=41,BACKSPACE=42,TAB=43,SPACE=44,MINUS=45,EQUALS=46,
	LEFTBRACKET=47,RIGHTBRACKET=48,BACKSLASH=49,NONUSHASH=50,SEMICOLON=51,
	APOSTROPHE=52,GRAVE=53,COMMA=54,PERIOD=55,SLASH=56,CAPSLOCK=57,
	F1=58,F2=59,F3=60,F4=61,F5=62,F6=63,F7=64,F8=65,F9=66,F10=67,F11=68,F12=69,
	PRINTSCREEN=70,SCROLLLOCK=71,PAUSE=72,INSERT=73,HOME=74,PAGEUP=75,
	DELETE=76,END=77,PAGEDOWN=78,RIGHT=79,LEFT=80,DOWN=81,UP=82,
	NUMLOCKCLEAR=83,KP_DIVIDE=84,KP_MULTIPLY=85,KP_MINUS=86,KP_PLUS=87,
	KP_ENTER=88,KP_1=89,KP_2=90,KP_3=91,KP_4=92,KP_5=93,KP_6=94,KP_7=95,
	KP_8=96,KP_9=97,KP_0=98,KP_PERIOD=99,NONUSBACKSLASH=100,APPLICATION=101,
	POWER=102,KP_EQUALS=103,F13=104,F14=105,F15=106,F16=107,F17=108,F18=109,
	F19=110,F20=111,F21=112,F22=113,F23=114,F24=115,EXECUTE=116,HELP=117,
	MENU=118,SELECT=119,STOP=120,AGAIN=121,UNDO=122,CUT=123,COPY=124,PASTE=125,
	FIND=126,MUTE=127,VOLUMEUP=128,VOLUMEDOWN=129,KP_COMMA=133,KP_EQUALSAS400=134,
	INTERNATIONAL1=135,INTERNATIONAL2=136,INTERNATIONAL3=137,INTERNATIONAL4=138,
	INTERNATIONAL5=139,INTERNATIONAL6=140,INTERNATIONAL7=141,INTERNATIONAL8=142,
	INTERNATIONAL9=143,LANG1=144,LANG2=145,LANG3=146,LANG4=147,LANG5=148,
	LANG6=149,LANG7=150,LANG8=151,LANG9=152,ALTERASE=153,SYSREQ=154,CANCEL=155,
	CLEAR=156,PRIOR=157,RETURN2=158,SEPARATOR=159,OUT=160,OPER=161,CLEARAGAIN=162,
	CRSEL=163,EXSEL=164,KP_00=176,KP_000=177,THOUSANDSSEPARATOR=178,
	DECIMALSEPARATOR=179,CURRENCYUNIT=180,CURRENCYSUBUNIT=181,KP_LEFTPAREN=182,
	KP_RIGHTPAREN=183,KP_LEFTBRACE=184,KP_RIGHTBRACE=185,KP_TAB=186,
	KP_BACKSPACE=187,KP_A=188,KP_B=189,KP_C=190,KP_D=191,KP_E=192,KP_F=193,
	KP_XOR=194,KP_POWER=195,KP_PERCENT=196,KP_LESS=197,KP_GREATER=198,
	KP_AMPERSAND=199,KP_DBLAMPERSAND=200,KP_VERTICALBAR=201,KP_DBLVERTICALBAR=202,
	KP_COLON=203,KP_HASH=204,KP_SPACE=205,KP_AT=206,KP_EXCLAM=207,
	KP_MEMSTORE=208,KP_MEMRECALL=209,KP_MEMCLEAR=210,KP_MEMADD=211,
	KP_MEMSUBTRACT=212,KP_MEMMULTIPLY=213,KP_MEMDIVIDE=214,KP_PLUSMINUS=215,
	KP_CLEAR=216,KP_CLEARENTRY=217,KP_BINARY=218,KP_OCTAL=219,KP_DECIMAL=220,
	KP_HEXADECIMAL=221,LCTRL=224,LSHIFT=225,LALT=226,LGUI=227,RCTRL=228,
	RSHIFT=229,RALT=230,RGUI=231,MODE=257,AUDIONEXT=258,AUDIOPREV=259,
	AUDIOSTOP=260,AUDIOPLAY=261,AUDIOMUTE=262,MEDIASELECT=263,WWW=264,MAIL=265,
	CALCULATOR=266,COMPUTER=267,AC_SEARCH=268,AC_HOME=269,AC_BACK=270,
	AC_FORWARD=271,AC_STOP=272,AC_REFRESH=273,AC_BOOKMARKS=274,
	BRIGHTNESSDOWN=275,BRIGHTNESSUP=276,DISPLAYSWITCH=277,KBDILLUMTOGGLE=278,
	KBDILLUMDOWN=279,KBDILLUMUP=280,EJECT=281,SLEEP=282,APP1=283,APP2=284,PAD1=285,
	NUM_SCANCODES=512,
};

constexpr int32_t K_SCANCODE_MASK = 1 << 30;

enum class K : int32_t {
	UNKNOWN=0, RETURN='\r', ESCAPE=27, BACKSPACE='\b', TAB='\t', SPACE=' ',
	EXCLAIM='!',QUOTEDBL='"',HASH='#',PERCENT='%',DOLLAR='$',AMPERSAND='&',
	QUOTE='\'',LEFTPAREN='(',RIGHTPAREN=')',ASTERISK='*',PLUS='+',COMMA=',',
	MINUS='-',PERIOD='.',SLASH='/',_0='0',_1='1',_2='2',_3='3',_4='4',_5='5',
	_6='6',_7='7',_8='8',_9='9',COLON=':',SEMICOLON=';',LESS='<',EQUALS='=',
	GREATER='>',QUESTION='?',AT='@',LEFTBRACKET='[',BACKSLASH='\\',RIGHTBRACKET=']',
	CARET='^',UNDERSCORE='_',BACKQUOTE='`',a='a',b='b',c='c',d='d',e='e',f='f',
	g='g',h='h',i='i',j='j',k='k',l='l',m='m',n='n',o='o',p='p',q='q',r='r',
	s='s',t='t',u='u',v='v',w='w',x='x',y='y',z='z',
	CAPSLOCK    = K_SCANCODE_MASK|static_cast<int32_t>(SDLKey::CAPSLOCK),
	F1          = K_SCANCODE_MASK|static_cast<int32_t>(SDLKey::F1),
	F2          = K_SCANCODE_MASK|static_cast<int32_t>(SDLKey::F2),
	F3          = K_SCANCODE_MASK|static_cast<int32_t>(SDLKey::F3),
	F4          = K_SCANCODE_MASK|static_cast<int32_t>(SDLKey::F4),
	F5          = K_SCANCODE_MASK|static_cast<int32_t>(SDLKey::F5),
	F6          = K_SCANCODE_MASK|static_cast<int32_t>(SDLKey::F6),
	F7          = K_SCANCODE_MASK|static_cast<int32_t>(SDLKey::F7),
	F8          = K_SCANCODE_MASK|static_cast<int32_t>(SDLKey::F8),
	F9          = K_SCANCODE_MASK|static_cast<int32_t>(SDLKey::F9),
	F10         = K_SCANCODE_MASK|static_cast<int32_t>(SDLKey::F10),
	F11         = K_SCANCODE_MASK|static_cast<int32_t>(SDLKey::F11),
	F12         = K_SCANCODE_MASK|static_cast<int32_t>(SDLKey::F12),
	PRINTSCREEN = K_SCANCODE_MASK|static_cast<int32_t>(SDLKey::PRINTSCREEN),
	SCROLLLOCK  = K_SCANCODE_MASK|static_cast<int32_t>(SDLKey::SCROLLLOCK),
	PAUSE       = K_SCANCODE_MASK|static_cast<int32_t>(SDLKey::PAUSE),
	INSERT      = K_SCANCODE_MASK|static_cast<int32_t>(SDLKey::INSERT),
	HOME        = K_SCANCODE_MASK|static_cast<int32_t>(SDLKey::HOME),
	PAGEUP      = K_SCANCODE_MASK|static_cast<int32_t>(SDLKey::PAGEUP),
	DEL         = 127,
	END         = K_SCANCODE_MASK|static_cast<int32_t>(SDLKey::END),
	PAGEDOWN    = K_SCANCODE_MASK|static_cast<int32_t>(SDLKey::PAGEDOWN),
	RIGHT       = K_SCANCODE_MASK|static_cast<int32_t>(SDLKey::RIGHT),
	LEFT        = K_SCANCODE_MASK|static_cast<int32_t>(SDLKey::LEFT),
	DOWN        = K_SCANCODE_MASK|static_cast<int32_t>(SDLKey::DOWN),
	UP          = K_SCANCODE_MASK|static_cast<int32_t>(SDLKey::UP),
	KP_0        = K_SCANCODE_MASK|static_cast<int32_t>(SDLKey::KP_0),
	KP_1        = K_SCANCODE_MASK|static_cast<int32_t>(SDLKey::KP_1),
	KP_2        = K_SCANCODE_MASK|static_cast<int32_t>(SDLKey::KP_2),
	KP_3        = K_SCANCODE_MASK|static_cast<int32_t>(SDLKey::KP_3),
	KP_4        = K_SCANCODE_MASK|static_cast<int32_t>(SDLKey::KP_4),
	KP_5        = K_SCANCODE_MASK|static_cast<int32_t>(SDLKey::KP_5),
	KP_6        = K_SCANCODE_MASK|static_cast<int32_t>(SDLKey::KP_6),
	KP_7        = K_SCANCODE_MASK|static_cast<int32_t>(SDLKey::KP_7),
	KP_8        = K_SCANCODE_MASK|static_cast<int32_t>(SDLKey::KP_8),
	KP_9        = K_SCANCODE_MASK|static_cast<int32_t>(SDLKey::KP_9),
	KP_DIVIDE   = K_SCANCODE_MASK|static_cast<int32_t>(SDLKey::KP_DIVIDE),
	KP_MULTIPLY = K_SCANCODE_MASK|static_cast<int32_t>(SDLKey::KP_MULTIPLY),
	KP_MINUS    = K_SCANCODE_MASK|static_cast<int32_t>(SDLKey::KP_MINUS),
	KP_PLUS     = K_SCANCODE_MASK|static_cast<int32_t>(SDLKey::KP_PLUS),
	KP_ENTER    = K_SCANCODE_MASK|static_cast<int32_t>(SDLKey::KP_ENTER),
	KP_PERIOD   = K_SCANCODE_MASK|static_cast<int32_t>(SDLKey::KP_PERIOD),
	LSHIFT      = K_SCANCODE_MASK|static_cast<int32_t>(SDLKey::LSHIFT),
	RSHIFT      = K_SCANCODE_MASK|static_cast<int32_t>(SDLKey::RSHIFT),
	// ... truncated for brevity — full list in .ssz source
};

// ── Key modifiers ────────────────────────────────────────────────────────

constexpr uint16_t KMOD_NONE  = 0x0000;
constexpr uint16_t KMOD_LSHIFT= 0x0001;
constexpr uint16_t KMOD_RSHIFT= 0x0002;
constexpr uint16_t KMOD_LCTRL = 0x0040;
constexpr uint16_t KMOD_RCTRL = 0x0080;
constexpr uint16_t KMOD_LALT  = 0x0100;
constexpr uint16_t KMOD_RALT  = 0x0200;
constexpr uint16_t KMOD_LGUI  = 0x0400;
constexpr uint16_t KMOD_RGUI  = 0x0800;
constexpr uint16_t KMOD_NUM   = 0x1000;
constexpr uint16_t KMOD_CAPS  = 0x2000;
constexpr uint16_t KMOD_MODE  = 0x4000;
constexpr uint16_t KMOD_CTRL  = KMOD_LCTRL|KMOD_RCTRL;
constexpr uint16_t KMOD_SHIFT = KMOD_LSHIFT|KMOD_RSHIFT;
constexpr uint16_t KMOD_ALT   = KMOD_LALT|KMOD_RALT;
constexpr uint16_t KMOD_GUI   = KMOD_LGUI|KMOD_RGUI;

// ── Structs ──────────────────────────────────────────────────────────────

struct alignas(4) Rect { int32_t x, y, w, h; };
struct alignas(4) keysym { SDLKey scancode; K sym; uint16_t mod; uint16_t pad; uint32_t unused; };
struct alignas(4) KeyboardEvent { uint32_t windowID; uint8_t state, repeat, pad1, pad2; keysym ks; };
struct alignas(4) MouseMotionEvent { uint32_t windowID, which, state; int32_t x, y, xrel, yrel; };
struct alignas(4) MouseButtonEvent { EventType etype; uint32_t which; uint8_t button, state, pad1, pad2; int32_t x, y; };
struct alignas(4) Event { EventType etype; uint32_t timestamp; KeyboardEvent key; MouseMotionEvent motion; MouseButtonEvent button; };

// ── Mouse button constants ───────────────────────────────────────────────

constexpr uint8_t RELEASED = 0;
constexpr uint8_t PRESSED = 1;
constexpr uint8_t BUTTON_LEFT = 1;
constexpr uint8_t BUTTON_MIDDLE = 2;
constexpr uint8_t BUTTON_RIGHT = 3;
constexpr uint8_t BUTTON_WHEELUP = 4;
constexpr uint8_t BUTTON_WHEELDOWN = 5;

// ── Audio constants ──────────────────────────────────────────────────────

constexpr int32_t SNDFREQ = 44100;
constexpr intptr_t SNDBUFLEN = 4096;

// ── Classes ──────────────────────────────────────────────────────────────

class Surface {
public:
	~Surface();
	bool     null() const;
	void     free();
	void     allocSurface(int w, int h);
	void     imgLoad(const std::wstring& fn);
	void     createPaletteSurface(const std::vector<uint8_t>& img, const std::vector<uint32_t>& pal, int w, int h);
	void     setColorKey(int key);
	intptr_t handle() const { return m_sur; }
private:
	intptr_t m_sur = 0;
};

class Font {
public:
	~Font();
	void    close();
	void    open(const std::wstring& fn, int size);
	void    render(uint32_t color, int x, int y, const std::wstring& str);
private:
	intptr_t m_font = 0;
};

class GlTexture {
public:
	~GlTexture();
	void clear();
	bool load8bitTexture(const std::vector<uint8_t>& pxl, int w, int h);
	bool loadPngTexture(int& w, int& h, class File& file);
	uint32_t id() const { return m_id; }
private:
	uint32_t m_id = 0;
};

class UseGlContext {
public:
	UseGlContext();
	~UseGlContext();
};

// ── Standalone functions ─────────────────────────────────────────────────

void    flip();
void    fill(Rect& r, uint32_t c);
void    softFill(Rect& r, uint32_t c);
bool    renderMugenZoom(Rect& dr, float rcx, float rcy, const std::vector<uint8_t>& pxl,
                        const std::vector<uint32_t>& pal, int16_t ckey, Rect& sr,
                        float cx, float ty, Rect& tile, float xtopscl, float xbotscl,
                        float yscl, float rasterxadd, uint32_t roto, int alpha, int rle,
                        std::vector<int8_t>& pluginbuf);
bool    renderMugenShadow(Rect& dr, float rcx, float rcy, const std::vector<uint8_t>& pxl,
                          uint32_t color, Rect& sr, float cx, float ty,
                          float xscl, float yscl, float vscl, uint32_t roto,
                          int alpha, int rle, std::vector<int8_t>& pluginbuf);
bool    renderFontBatch(const std::vector<uint8_t>& atlas, float baseX, float baseY,
                        const std::vector<uint32_t>& pal, int atlasStride, int glyphH,
                        int alpha, Rect& window, float xscl, float yscl, float spacing,
                        const std::vector<int32_t>& glyphData, int count);
wchar_t getLastChar();
std::vector<uint8_t> decodePNG8(int& w, int& h, File& file);
int32_t pollInputBitmask(int jn, int u,int d,int l,int r, int a,int b,int c, int x,int y,int z, int q,int w,int e,int s,
                         int jn2, int u2,int d2,int l2,int r2, int a2,int b2,int c2, int x2,int y2,int z2, int q2,int w2,int e2,int s2, int sec);
bool    setSndBuf(std::vector<int32_t>& buf);
int     playVideo(int audiotrack, int volume, const std::wstring& captures, const std::wstring& fn);
bool    playBGM(const std::wstring& pldir, const std::wstring& fn);
void    pauseBGM(bool pause);
bool    sendOpenBGM(int rate, int channels);
void    sendCloseBGM();
intptr_t sendWriteBGM(const std::vector<int16_t>& buffer);
void    fadeInBGM(int time);
void    fadeOutBGM(int time);
void    setVolume(float gvol, float wvol, float bvol);
void    setOpacity(float wo);
bool    init(const std::wstring& title, int w, int h, int renderer, bool mugen);
int     getWidth();
int     getHeight();
void    windowSize(int w, int h);
void    fullScreenMode(bool fullReal);
bool    fullScreen(bool full);
void    setWindowType(int state);
void    keepAspectRatio(bool aspect);
void    takeScreenShot(const std::wstring& dir);
void    showCursor(bool show);
bool    bindGlContext();
bool    unbindGlContext();
void    enablePerfMonitor(bool enable);
void    getRendererInfo();
void    drawTTF(const std::wstring& path, int align, const std::wstring& text,
                int x, int y, float scaleX, float scaleY, int r, int g, int b, int alpha);

bool keyState(SDLKey key);
bool joystickButtonState(int btn, int joy);
void delay(uint32_t ms);
uint32_t getTicks();
bool pollEvent(Event& ev);
void updateGLViewport(Event& ev);
void end();

// ── OpenGL helpers ───────────────────────────────────────────────────────
void mugenFillGl(Rect& r, uint32_t color, int alpha);
bool initMugenGl();
void glSwapBuffers();

} // namespace ikemen
