// sdlplugin_service.hpp — Native C++ implementation for ssz_script/lib/alpha/sdlplugin.ssz
//
// sdlplugin.ssz (1022 lines) implements SDL rendering wrappers — Flip, Fill,
// SoftFill, RenderMugenZoom, BlitSurface, font rendering, sprite loading,
// input handling, audio playback, and all SDL plugin entry points.
//
// Phase 5: All public API functions implemented as thin wrappers around
// existing main/sdlplugin/sdlplugin.cpp implementations. Enums, structs,
// and constants match SSZ definitions exactly.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Forward declarations for SSZ types used in bridge layer
struct Reference;

// Prevent Windows macro conflicts with SSZ enum values
#ifdef DELETE
#undef DELETE
#endif
#ifdef OUT
#undef OUT
#endif
#ifdef NEAR
#undef NEAR
#endif
#ifdef FAR
#undef FAR
#endif
#ifdef UNKNOWN
#undef UNKNOWN
#endif
#ifdef IN
#undef IN
#endif

namespace ikemen::ssz_native {

// =========================================================================
// Constants
// =========================================================================
constexpr int SNDFREQ = 44100;
constexpr intptr_t SNDBUFLEN = 4096;

constexpr uint8_t RELEASED = 0;
constexpr uint8_t PRESSED = 1;
constexpr uint8_t BUTTON_LEFT = 1;
constexpr uint8_t BUTTON_MIDDLE = 2;
constexpr uint8_t BUTTON_RIGHT = 3;
constexpr uint8_t BUTTON_WHEELUP = 4;
constexpr uint8_t BUTTON_WHEELDOWN = 5;

constexpr uint16_t KMOD_NONE      = 0x0000;
constexpr uint16_t KMOD_LSHIFT    = 0x0001;
constexpr uint16_t KMOD_RSHIFT    = 0x0002;
constexpr uint16_t KMOD_LCTRL     = 0x0040;
constexpr uint16_t KMOD_RCTRL     = 0x0080;
constexpr uint16_t KMOD_LALT      = 0x0100;
constexpr uint16_t KMOD_RALT      = 0x0200;
constexpr uint16_t KMOD_LGUI      = 0x0400;
constexpr uint16_t KMOD_RGUI      = 0x0800;
constexpr uint16_t KMOD_NUM       = 0x1000;
constexpr uint16_t KMOD_CAPS      = 0x2000;
constexpr uint16_t KMOD_MODE      = 0x4000;
constexpr uint16_t KMOD_RESERVED  = 0x8000;
constexpr uint16_t KMOD_CTRL      = (KMOD_LCTRL | KMOD_RCTRL);
constexpr uint16_t KMOD_SHIFT     = (KMOD_LSHIFT | KMOD_RSHIFT);
constexpr uint16_t KMOD_ALT       = (KMOD_LALT | KMOD_RALT);
constexpr uint16_t KMOD_GUI       = (KMOD_LGUI | KMOD_RGUI);

constexpr int K_SCANCODE_MASK = (1 << 30);

// =========================================================================
// Enum types
// =========================================================================

enum class EventType : uint16_t {
    FIRSTEVENT              = 0,
    NOEVENT                 = 0,
    QUIT                    = 0x100,
    APP_TERMINATING,
    APP_LOWMEMORY,
    APP_WILLENTERBACKGROUND,
    APP_DIDENTERBACKGROUND,
    APP_WILLENTERFOREGROUND,
    APP_DIDENTERFOREGROUND,
    WINDOWEVENT             = 0x200,
    SYSWMEVENT,
    KEYDOWN                 = 0x300,
    KEYUP,
    TEXTEDITING,
    TEXTINPUT,
    MOUSEMOTION             = 0x400,
    MOUSEBUTTONDOWN,
    MOUSEBUTTONUP,
    MOUSEWHEEL,
    JOYAXISMOTION           = 0x600,
    JOYBALLMOTION,
    JOYHATMOTION,
    JOYBUTTONDOWN,
    JOYBUTTONUP,
    JOYDEVICEADDED,
    JOYDEVICEREMOVED,
    CONTROLLERAXISMOTION    = 0x650,
    CONTROLLERBUTTONDOWN,
    CONTROLLERBUTTONUP,
    CONTROLLERDEVICEADDED,
    CONTROLLERDEVICEREMOVED,
    CONTROLLERDEVICEREMAPPED,
    FINGERDOWN              = 0x700,
    FINGERUP,
    FINGERMOTION,
    DOLLARGESTURE           = 0x800,
    DOLLARRECORD,
    MULTIGESTURE,
    CLIPBOARDUPDATE         = 0x900,
    DROPFILE                = 0x1000,
    USEREVENT               = 0x8000,
    LASTEVENT               = 0xFFFF
};

enum class SDLKey : int32_t {
    UNKNOWN = 0,
    a = 4, b = 5, c = 6, d = 7, e = 8, f = 9, g = 10, h = 11, i = 12,
    j = 13, k = 14, l = 15, m = 16, n = 17, o = 18, p = 19, q = 20,
    r = 21, s = 22, t = 23, u = 24, v = 25, w = 26, x = 27, y = 28, z = 29,
    _1 = 30, _2 = 31, _3 = 32, _4 = 33, _5 = 34, _6 = 35, _7 = 36, _8 = 37,
    _9 = 38, _0 = 39,
    RETURN = 40, ESCAPE = 41, BACKSPACE = 42, TAB = 43, SPACE = 44,
    MINUS = 45, EQUALS = 46, LEFTBRACKET = 47, RIGHTBRACKET = 48,
    BACKSLASH = 49, NONUSHASH = 50, SEMICOLON = 51, APOSTROPHE = 52,
    GRAVE = 53, COMMA = 54, PERIOD = 55, SLASH = 56, CAPSLOCK = 57,
    F1 = 58, F2 = 59, F3 = 60, F4 = 61, F5 = 62, F6 = 63, F7 = 64,
    F8 = 65, F9 = 66, F10 = 67, F11 = 68, F12 = 69,
    PRINTSCREEN = 70, SCROLLLOCK = 71, PAUSE = 72, INSERT = 73,
    HOME = 74, PAGEUP = 75, DELETE = 76, END = 77, PAGEDOWN = 78,
    RIGHT = 79, LEFT = 80, DOWN = 81, UP = 82,
    NUMLOCKCLEAR = 83, KP_DIVIDE = 84, KP_MULTIPLY = 85, KP_MINUS = 86,
    KP_PLUS = 87, KP_ENTER = 88, KP_1 = 89, KP_2 = 90, KP_3 = 91,
    KP_4 = 92, KP_5 = 93, KP_6 = 94, KP_7 = 95, KP_8 = 96, KP_9 = 97,
    KP_0 = 98, KP_PERIOD = 99,
    NONUSBACKSLASH = 100, APPLICATION = 101, POWER = 102, KP_EQUALS = 103,
    F13 = 104, F14 = 105, F15 = 106, F16 = 107, F17 = 108, F18 = 109,
    F19 = 110, F20 = 111, F21 = 112, F22 = 113, F23 = 114, F24 = 115,
    EXECUTE = 116, HELP = 117, MENU = 118, SELECT = 119, STOP = 120,
    AGAIN = 121, UNDO = 122, CUT = 123, COPY = 124, PASTE = 125,
    FIND = 126, MUTE = 127, VOLUMEUP = 128, VOLUMEDOWN = 129,
    KP_COMMA = 133, KP_EQUALSAS400 = 134,
    INTERNATIONAL1 = 135, INTERNATIONAL2 = 136, INTERNATIONAL3 = 137,
    INTERNATIONAL4 = 138, INTERNATIONAL5 = 139, INTERNATIONAL6 = 140,
    INTERNATIONAL7 = 141, INTERNATIONAL8 = 142, INTERNATIONAL9 = 143,
    LANG1 = 144, LANG2 = 145, LANG3 = 146, LANG4 = 147, LANG5 = 148,
    LANG6 = 149, LANG7 = 150, LANG8 = 151, LANG9 = 152,
    ALTERASE = 153, SYSREQ = 154, CANCEL = 155, CLEAR = 156, PRIOR = 157,
    RETURN2 = 158, SEPARATOR = 159, OUT = 160, OPER = 161,
    CLEARAGAIN = 162, CRSEL = 163, EXSEL = 164,
    KP_00 = 176, KP_000 = 177, THOUSANDSSEPARATOR = 178,
    DECIMALSEPARATOR = 179, CURRENCYUNIT = 180, CURRENCYSUBUNIT = 181,
    KP_LEFTPAREN = 182, KP_RIGHTPAREN = 183, KP_LEFTBRACE = 184,
    KP_RIGHTBRACE = 185, KP_TAB = 186, KP_BACKSPACE = 187,
    KP_A = 188, KP_B = 189, KP_C = 190, KP_D = 191, KP_E = 192, KP_F = 193,
    KP_XOR = 194, KP_POWER = 195, KP_PERCENT = 196, KP_LESS = 197,
    KP_GREATER = 198, KP_AMPERSAND = 199, KP_DBLAMPERSAND = 200,
    KP_VERTICALBAR = 201, KP_DBLVERTICALBAR = 202, KP_COLON = 203,
    KP_HASH = 204, KP_SPACE = 205, KP_AT = 206, KP_EXCLAM = 207,
    KP_MEMSTORE = 208, KP_MEMRECALL = 209, KP_MEMCLEAR = 210,
    KP_MEMADD = 211, KP_MEMSUBTRACT = 212, KP_MEMMULTIPLY = 213,
    KP_MEMDIVIDE = 214, KP_PLUSMINUS = 215, KP_CLEAR = 216,
    KP_CLEARENTRY = 217, KP_BINARY = 218, KP_OCTAL = 219,
    KP_DECIMAL = 220, KP_HEXADECIMAL = 221,
    LCTRL = 224, LSHIFT = 225, LALT = 226, LGUI = 227,
    RCTRL = 228, RSHIFT = 229, RALT = 230, RGUI = 231,
    MODE = 257, AUDIONEXT = 258, AUDIOPREV = 259, AUDIOSTOP = 260,
    AUDIOPLAY = 261, AUDIOMUTE = 262, MEDIASELECT = 263,
    WWW = 264, MAIL = 265, CALCULATOR = 266, COMPUTER = 267,
    AC_SEARCH = 268, AC_HOME = 269, AC_BACK = 270, AC_FORWARD = 271,
    AC_STOP = 272, AC_REFRESH = 273, AC_BOOKMARKS = 274,
    BRIGHTNESSDOWN = 275, BRIGHTNESSUP = 276, DISPLAYSWITCH = 277,
    KBDILLUMTOGGLE = 278, KBDILLUMDOWN = 279, KBDILLUMUP = 280,
    EJECT = 281, SLEEP = 282, APP1 = 283, APP2 = 284, PAD1 = 285,
    NUM_SCANCODES = 512
};

enum class K : int32_t {
    UNKNOWN = 0,
    RETURN = '\r', ESCAPE = 0x1B, BACKSPACE = '\x08', TAB = '\t', SPACE = ' ',
    EXCLAIM = '!', QUOTEDBL = '"', HASH = '#', PERCENT = '%', DOLLAR = '$',
    AMPERSAND = '&', QUOTE = '\'', LEFTPAREN = '(', RIGHTPAREN = ')',
    ASTERISK = '*', PLUS = '+', COMMA = ',', MINUS = '-', PERIOD = '.',
    SLASH = '/', _0 = '0', _1 = '1', _2 = '2', _3 = '3', _4 = '4',
    _5 = '5', _6 = '6', _7 = '7', _8 = '8', _9 = '9',
    COLON = ':', SEMICOLON = ';', LESS = '<', EQUALS = '=', GREATER = '>',
    QUESTION = '?', AT = '@',
    LEFTBRACKET = '[', BACKSLASH = '\\', RIGHTBRACKET = ']',
    CARET = '^', UNDERSCORE = '_', BACKQUOTE = '`',
    a = 'a', b = 'b', c = 'c', d = 'd', e = 'e', f = 'f', g = 'g',
    h = 'h', i = 'i', j = 'j', k = 'k', l = 'l', m = 'm', n = 'n',
    o = 'o', p = 'p', q = 'q', r = 'r', s = 's', t = 't', u = 'u',
    v = 'v', w = 'w', x = 'x', y = 'y', z = 'z',
    CAPSLOCK = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::CAPSLOCK),
    F1 = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::F1),
    F2 = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::F2),
    F3 = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::F3),
    F4 = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::F4),
    F5 = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::F5),
    F6 = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::F6),
    F7 = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::F7),
    F8 = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::F8),
    F9 = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::F9),
    F10 = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::F10),
    F11 = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::F11),
    F12 = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::F12),
    PRINTSCREEN = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::PRINTSCREEN),
    SCROLLLOCK = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::SCROLLLOCK),
    PAUSE = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::PAUSE),
    INSERT = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::INSERT),
    HOME = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::HOME),
    PAGEUP = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::PAGEUP),
    DELETE = 0x7F,
    END = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::END),
    PAGEDOWN = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::PAGEDOWN),
    RIGHT = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::RIGHT),
    LEFT = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::LEFT),
    DOWN = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::DOWN),
    UP = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::UP),
    NUMLOCKCLEAR = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::NUMLOCKCLEAR),
    KP_DIVIDE = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_DIVIDE),
    KP_MULTIPLY = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_MULTIPLY),
    KP_MINUS = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_MINUS),
    KP_PLUS = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_PLUS),
    KP_ENTER = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_ENTER),
    KP_1 = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_1),
    KP_2 = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_2),
    KP_3 = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_3),
    KP_4 = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_4),
    KP_5 = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_5),
    KP_6 = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_6),
    KP_7 = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_7),
    KP_8 = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_8),
    KP_9 = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_9),
    KP_0 = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_0),
    KP_PERIOD = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_PERIOD),
    APPLICATION = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::APPLICATION),
    POWER = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::POWER),
    KP_EQUALS = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_EQUALS),
    F13 = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::F13),
    F14 = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::F14),
    F15 = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::F15),
    F16 = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::F16),
    F17 = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::F17),
    F18 = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::F18),
    F19 = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::F19),
    F20 = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::F20),
    F21 = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::F21),
    F22 = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::F22),
    F23 = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::F23),
    F24 = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::F24),
    EXECUTE = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::EXECUTE),
    HELP = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::HELP),
    MENU = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::MENU),
    SELECT = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::SELECT),
    STOP = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::STOP),
    AGAIN = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::AGAIN),
    UNDO = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::UNDO),
    CUT = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::CUT),
    COPY = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::COPY),
    PASTE = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::PASTE),
    FIND = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::FIND),
    MUTE = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::MUTE),
    VOLUMEUP = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::VOLUMEUP),
    VOLUMEDOWN = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::VOLUMEDOWN),
    KP_COMMA = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_COMMA),
    KP_EQUALSAS400 = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_EQUALSAS400),
    ALTERASE = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::ALTERASE),
    SYSREQ = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::SYSREQ),
    CANCEL = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::CANCEL),
    CLEAR = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::CLEAR),
    PRIOR = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::PRIOR),
    RETURN2 = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::RETURN2),
    SEPARATOR = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::SEPARATOR),
    OUT = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::OUT),
    OPER = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::OPER),
    CLEARAGAIN = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::CLEARAGAIN),
    CRSEL = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::CRSEL),
    EXSEL = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::EXSEL),
    KP_00 = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_00),
    KP_000 = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_000),
    THOUSANDSSEPARATOR = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::THOUSANDSSEPARATOR),
    DECIMALSEPARATOR = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::DECIMALSEPARATOR),
    CURRENCYUNIT = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::CURRENCYUNIT),
    CURRENCYSUBUNIT = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::CURRENCYSUBUNIT),
    KP_LEFTPAREN = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_LEFTPAREN),
    KP_RIGHTPAREN = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_RIGHTPAREN),
    KP_LEFTBRACE = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_LEFTBRACE),
    KP_RIGHTBRACE = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_RIGHTBRACE),
    KP_TAB = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_TAB),
    KP_BACKSPACE = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_BACKSPACE),
    KP_A = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_A),
    KP_B = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_B),
    KP_C = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_C),
    KP_D = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_D),
    KP_E = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_E),
    KP_F = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_F),
    KP_XOR = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_XOR),
    KP_POWER = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_POWER),
    KP_PERCENT = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_PERCENT),
    KP_LESS = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_LESS),
    KP_GREATER = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_GREATER),
    KP_AMPERSAND = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_AMPERSAND),
    KP_DBLAMPERSAND = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_DBLAMPERSAND),
    KP_VERTICALBAR = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_VERTICALBAR),
    KP_DBLVERTICALBAR = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_DBLVERTICALBAR),
    KP_COLON = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_COLON),
    KP_HASH = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_HASH),
    KP_SPACE = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_SPACE),
    KP_AT = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_AT),
    KP_EXCLAM = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_EXCLAM),
    KP_MEMSTORE = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_MEMSTORE),
    KP_MEMRECALL = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_MEMRECALL),
    KP_MEMCLEAR = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_MEMCLEAR),
    KP_MEMADD = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_MEMADD),
    KP_MEMSUBTRACT = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_MEMSUBTRACT),
    KP_MEMMULTIPLY = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_MEMMULTIPLY),
    KP_MEMDIVIDE = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_MEMDIVIDE),
    KP_PLUSMINUS = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_PLUSMINUS),
    KP_CLEAR = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_CLEAR),
    KP_CLEARENTRY = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_CLEARENTRY),
    KP_BINARY = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_BINARY),
    KP_OCTAL = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_OCTAL),
    KP_DECIMAL = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_DECIMAL),
    KP_HEXADECIMAL = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KP_HEXADECIMAL),
    LCTRL = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::LCTRL),
    LSHIFT = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::LSHIFT),
    LALT = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::LALT),
    LGUI = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::LGUI),
    RCTRL = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::RCTRL),
    RSHIFT = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::RSHIFT),
    RALT = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::RALT),
    RGUI = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::RGUI),
    MODE = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::MODE),
    AUDIONEXT = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::AUDIONEXT),
    AUDIOPREV = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::AUDIOPREV),
    AUDIOSTOP = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::AUDIOSTOP),
    AUDIOPLAY = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::AUDIOPLAY),
    AUDIOMUTE = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::AUDIOMUTE),
    MEDIASELECT = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::MEDIASELECT),
    WWW = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::WWW),
    MAIL = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::MAIL),
    CALCULATOR = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::CALCULATOR),
    COMPUTER = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::COMPUTER),
    AC_SEARCH = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::AC_SEARCH),
    AC_HOME = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::AC_HOME),
    AC_BACK = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::AC_BACK),
    AC_FORWARD = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::AC_FORWARD),
    AC_STOP = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::AC_STOP),
    AC_REFRESH = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::AC_REFRESH),
    AC_BOOKMARKS = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::AC_BOOKMARKS),
    BRIGHTNESSDOWN = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::BRIGHTNESSDOWN),
    BRIGHTNESSUP = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::BRIGHTNESSUP),
    DISPLAYSWITCH = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::DISPLAYSWITCH),
    KBDILLUMTOGGLE = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KBDILLUMTOGGLE),
    KBDILLUMDOWN = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KBDILLUMDOWN),
    KBDILLUMUP = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::KBDILLUMUP),
    EJECT = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::EJECT),
    SLEEP = K_SCANCODE_MASK | static_cast<int32_t>(SDLKey::SLEEP)
};

} // namespace ikemen::ssz_native

// Now include SDL headers AFTER all our type definitions
#include <SDL.h>
#include <SDL_ttf.h>

namespace ikemen::ssz_native {

// =========================================================================
// Data structs
// =========================================================================

struct Keysym {
    SDLKey scancode{SDLKey::UNKNOWN};
    K sym{K::UNKNOWN};
    uint16_t mod{0};
    uint16_t padding{0};
    uint32_t unused{0};
};

struct KeyboardEvent {
    uint32_t windowID{0};
    uint8_t state{0};
    uint8_t repeat{0};
    uint8_t padding1{0}, padding2{0};
    Keysym keysym;
};

struct MouseMotionEvent {
    uint32_t windowID{0};
    uint32_t which{0};
    uint32_t state{0};
    int x{0}, y{0};
    int xrel{0}, yrel{0};
};

struct MouseButtonEvent {
    int32_t etype{0};  // EventType as int32_t (matches PollEvent output)
    uint32_t which{0};
    uint8_t button{0};
    uint8_t state{0};
    uint8_t padding1{0}, padding2{0};
    int x{0}, y{0};
};

struct Event {
    int32_t etype{0};  // EventType as int32_t (matches PollEvent output format)
    uint32_t timestamp{0};
    KeyboardEvent key;
    MouseMotionEvent motion;
    MouseButtonEvent button;
};

struct SdlRect {
    int x{0}, y{0};
    int w{0}, h{0};
    
    void set(int x, int y, int w, int h) {
        this->x = x;
        this->y = y;
        this->w = w;
        this->h = h;
    }
};

struct Surface {
    SDL_Surface* surface{nullptr};
    
    bool isNull() const { return surface == nullptr; }
    
    void free();
    void allocSurface(int w, int h);
    void imgLoad(const std::string& fn);
    void blitToWin(const SdlRect& dr);
    void createPaletteSurface(const std::vector<uint8_t>& img, 
                              const std::vector<uint32_t>& pal, 
                              int w, int h);
    void setColorKey(uint32_t key);
};

struct Font {
    TTF_Font* font{nullptr};
    
    void close();
    void open(const std::string& fn, int size);
    void render(uint32_t color, int x, int y, const std::string& str);
};

struct GlTexture {
    uint32_t id{0};
    
    void clear();
    bool load8bitTexture(const std::vector<uint8_t>& pxl, int w, int h);
    bool loadPngTexture(int w, int h, FILE* fp);
};

// Forward declarations for UseGlContext
bool bindGlContext();
bool unbindGlContext();

struct UseGlContext {
    UseGlContext() { bindGlContext(); }
    ~UseGlContext() { unbindGlContext(); }
};

// =========================================================================
// Module-level API
// =========================================================================

// Rendering
void flip();
void fill(const SdlRect& r, uint32_t c);
void softFill(const SdlRect& r, uint32_t c);
bool renderMugenZoom(const SdlRect& dr, float rcx, float rcy, 
                     const std::vector<uint8_t>& pxl, 
                     const std::vector<uint32_t>& pal, 
                     int16_t ckey, const SdlRect& sr, 
                     float cx, float ty, const SdlRect& tile,
                     float xtopscl, float xbotscl, float yscl, 
                     float rasterxadd, uint32_t roto, int alpha, 
                     int rle, std::vector<int8_t>& pluginbuf);
bool renderMugenShadow(const SdlRect& dr, float rcx, float rcy,
                       const std::vector<uint8_t>& pxl, uint32_t color,
                       const SdlRect& sr, float cx, float ty,
                       float xscl, float yscl, float vscl, uint32_t roto,
                       int alpha, int rle, std::vector<int8_t>& pluginbuf);
bool renderFontBatch(const std::vector<uint8_t>& atlas, float baseX, float baseY,
                     const std::vector<uint32_t>& pal, int atlasStride,
                     int glyphH, int alpha, const SdlRect& window,
                     float xscl, float yscl, float spacing,
                     const std::vector<int>& glyphData, int count);

// Input
char16_t getLastChar();
std::vector<uint8_t> decodePNG8(int w, int h, FILE* file);
bool keyState(SDLKey key);
bool joystickButtonState(int32_t joy, int32_t btn);
int32_t pollInputBitmask(
    int jn,
    int u, int d, int l, int r,
    int a, int b, int c,
    int x, int y, int z,
    int q, int w, int e, int s,
    int jn2,
    int u2, int d2, int l2, int r2,
    int a2, int b2, int c2,
    int x2, int y2, int z2,
    int q2, int w2, int e2, int s2,
    int sec);

// Audio
bool setSndBuf(const std::vector<int>& buf);
int playVideo(int audiotrack, int volume, const std::string& captures, const std::string& fn);
bool playBGM(const std::string& pldir, const std::string& fn);
void pauseBGM(bool pause);
bool sendOpenBGM(int rate, int channels);
void sendCloseBGM();
intptr_t sendWriteBGM(const std::vector<int16_t>& buffer);
void fadeInBGM(int time);
void fadeOutBGM(int time);
void setVolume(float gvol, float wvol, float bvol);
void setOpacity(float wo);

// Window/Display
bool init(const std::string& t, int w, int h, int renderer, bool mugen);
int getWidth();
int getHeight();
void windowSize(int w, int h);
void fullScreenMode(bool fullReal);
bool fullScreen(bool full);
void setWindowType(int state);
void keepAspectRatio(bool aspect);
void takeScreenShot(const std::string& dir);
void showCursor(bool show);

// OpenGL
bool bindGlContext();
bool unbindGlContext();
void enablePerfMonitor(bool enable);
void getRendererInfo();

} // namespace ikemen::ssz_native
