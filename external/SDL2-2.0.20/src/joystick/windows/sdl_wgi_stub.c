/* Stub for SDL_WGI_JoystickDriver — WGI not available in MinGW builds.
 * Provides safe stub functions so the zero-initialized struct (all NULL
 * function pointers) never appears in the driver array.  Previously the
 * tentative definition "SDL_JoystickDriver SDL_WGI_JoystickDriver;" produced
 * a struct with all NULL pointers, causing a crash at 0x00000000 when
 * SDL_JoystickInit() called driver->Init().
 *
 * Stub functions safely return empty/default values.
 */
#include "SDL_internal.h"
#include "../SDL_sysjoystick.h"

#if defined(SDL_JOYSTICK_WGI) && SDL_JOYSTICK_WGI
#undef SDL_JOYSTICK_WGI
#define SDL_JOYSTICK_WGI 0
#endif

/* ---- stub functions ---- */

static int  WGI_StubInit(void)                        { return 0; }
static int  WGI_StubGetCount(void)                     { return 0; }
static void WGI_StubDetect(void)                       { }
static const char *WGI_StubGetDeviceName(int idx)      { (void)idx; return NULL; }
static int  WGI_StubGetDevicePlayerIndex(int idx)      { (void)idx; return -1; }
static void WGI_StubSetDevicePlayerIndex(int idx, int pi) { (void)idx; (void)pi; }
static SDL_JoystickGUID WGI_StubGetDeviceGUID(int idx) { (void)idx; SDL_JoystickGUID g; SDL_zero(g); return g; }
static SDL_JoystickID WGI_StubGetDeviceInstanceID(int idx) { (void)idx; return -1; }
static int  WGI_StubOpen(SDL_Joystick *j, int idx)    { (void)j; (void)idx; return -1; }
static int  WGI_StubRumble(SDL_Joystick *j, Uint16 l, Uint16 h) { (void)j; (void)l; (void)h; return -1; }
static int  WGI_StubRumbleTriggers(SDL_Joystick *j, Uint16 l, Uint16 r) { (void)j; (void)l; (void)r; return -1; }
static Uint32 WGI_StubGetCapabilities(SDL_Joystick *j) { (void)j; return 0; }
static int  WGI_StubSetLED(SDL_Joystick *j, Uint8 r, Uint8 g, Uint8 b) { (void)j; (void)r; (void)g; (void)b; return -1; }
static int  WGI_StubSendEffect(SDL_Joystick *j, const void *d, int s) { (void)j; (void)d; (void)s; return -1; }
static int  WGI_StubSetSensorsEnabled(SDL_Joystick *j, SDL_bool e) { (void)j; (void)e; return -1; }
static void WGI_StubUpdate(SDL_Joystick *j)            { (void)j; }
static void WGI_StubClose(SDL_Joystick *j)             { (void)j; }
static void WGI_StubQuit(void)                         { }
static SDL_bool WGI_StubGetGamepadMapping(int idx, SDL_GamepadMapping *out) { (void)idx; (void)out; return SDL_FALSE; }

/* Fully populated driver struct — all function pointers are non-NULL */
SDL_JoystickDriver SDL_WGI_JoystickDriver =
{
    WGI_StubInit,
    WGI_StubGetCount,
    WGI_StubDetect,
    WGI_StubGetDeviceName,
    WGI_StubGetDevicePlayerIndex,
    WGI_StubSetDevicePlayerIndex,
    WGI_StubGetDeviceGUID,
    WGI_StubGetDeviceInstanceID,
    WGI_StubOpen,
    WGI_StubRumble,
    WGI_StubRumbleTriggers,
    WGI_StubGetCapabilities,
    WGI_StubSetLED,
    WGI_StubSendEffect,
    WGI_StubSetSensorsEnabled,
    WGI_StubUpdate,
    WGI_StubClose,
    WGI_StubQuit,
    WGI_StubGetGamepadMapping
};
