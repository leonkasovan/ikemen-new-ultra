# Makefile — Static build of Ikemen Plus Ultra using w64devkit
# All 19 external libraries compiled from source alongside the application.
#
# Usage (command prompt with w64devkit in PATH, or MSYS2 shell):
#   make                      # Release (64-bit)
#   make CONFIG=Debug         # Debug build
#   make clean
#
# Requires: w64devkit x86 (https://github.com/skeeto/w64devkit/releases/download/v2.8.0/w64devkit-x86-2.8.0.7z.exe)

CONFIG ?= Release
ARCH   ?= x86_64

.DEFAULT_GOAL := all

# ---- Toolchain: w64devkit (https://github.com/skeeto/w64devkit) ----
W64DEVKIT = C:/x86devkit
CXX       = $(W64DEVKIT)/bin/g++
CC        = $(W64DEVKIT)/bin/gcc
AR        = $(W64DEVKIT)/bin/ar
ARCH_F    = 
ARFLAGS   = rcs
SHELL     = $(W64DEVKIT)/bin/sh.exe

# Ensure w64devkit tools (as, ld) are in PATH for the compiler driver
export PATH := $(W64DEVKIT)/bin:$(PATH)

# ---- Directories ----
ROOT       = .
EXT        = $(ROOT)/external
MAIN       = $(ROOT)/main
SSZ        = $(MAIN)/ssz
SCRIPT     = $(ROOT)/script
TEST       = $(ROOT)/test
BLD        = $(ROOT)/build/$(CONFIG)


# ---- Common flags ----
CXXFLAGS   = -std=c++17 -fpermissive -fno-operator-names \
             -Wall -Wno-unused-function -Wno-unused-variable \
             -Wno-attributes -Wno-switch -Wno-sign-compare \
             -Wno-parentheses -Wno-narrowing -Wno-class-memaccess \
             -fno-strict-aliasing -DUNICODE -D_UNICODE $(ARCH_F)

CFLAGS     = -std=gnu11 -Wall -Wno-unused-function -Wno-unused-variable \
             -fno-strict-aliasing $(ARCH_F)

ifeq ($(CONFIG),Debug)
  CXXFLAGS += -O1 -g -DDEBUG
  CFLAGS   += -O1 -g -DDEBUG
  TARGET     = $(BLD)/ikemen-debug.exe
else
  CXXFLAGS += -O3 -DNDEBUG
  CFLAGS   += -O3 -DNDEBUG
  TARGET     = $(BLD)/ikemen.exe
endif

# ---- Global include paths ----
GLOBAL_INC  = -I $(MAIN) -I $(SSZ)
GLOBAL_INC += -I $(EXT)/SDL2-2.0.20/include
GLOBAL_INC += -I $(EXT)/SDL2_image-2.0.5
GLOBAL_INC += -I $(EXT)/SDL2_ttf-2.0.18
GLOBAL_INC += -I $(EXT)/SDL2_mixer-2.0.4
GLOBAL_INC += -I $(EXT)/SDL2_mixer-2.0.4/VisualC/external/include
GLOBAL_INC += -I $(EXT)/glew-2.2.0/include
GLOBAL_INC += -I $(EXT)/lpng1655
GLOBAL_INC += -I $(EXT)/zlib-1.2.8
GLOBAL_INC += -I $(EXT)/jpeg-9b
GLOBAL_INC += -I $(EXT)/libwebp-1.0.2/src
GLOBAL_INC += -I $(EXT)/libwebp-1.0.2
GLOBAL_INC += -I $(EXT)/freetype-2.10.4/include
GLOBAL_INC += -I $(EXT)/lua-5.2.4
GLOBAL_INC += -I $(EXT)/libogg-1.3.6/include
GLOBAL_INC += -I $(EXT)/libvorbis-1.3.7/include
GLOBAL_INC += -I $(EXT)/portaudio/include
GLOBAL_INC += -I $(EXT)/portaudio/src/common
GLOBAL_INC += -I $(EXT)/portaudio/src/os/win
GLOBAL_INC += -I $(EXT)/mpg123-1.25.6/src
GLOBAL_INC += -I $(EXT)/mpg123-1.25.6/src/compat
GLOBAL_INC += -I $(EXT)/mpg123-1.25.6/src/libmpg123
GLOBAL_INC += -I $(EXT)/opus-1.6.1/include
GLOBAL_INC += -I $(EXT)/opus-1.6.1/src
GLOBAL_INC += -I $(EXT)/opus-1.6.1/celt
GLOBAL_INC += -I $(EXT)/opus-1.6.1/silk
GLOBAL_INC += -I $(EXT)/opus-1.6.1/silk/float
GLOBAL_INC += -I $(EXT)/opusfile-0.12/include
GLOBAL_INC += -I $(EXT)/flac-1.3.2/include
GLOBAL_INC += -I $(EXT)/flac-1.3.2/src/libFLAC/include
GLOBAL_INC += -I $(EXT)/libmodplug-sezero/src
GLOBAL_INC += -I $(EXT)/libmodplug-sezero/src/libmodplug
GLOBAL_INC += -I $(EXT)/opus-1.6.1/win32
GLOBAL_INC += -I $(EXT)/vlc-2.2.8/include

GLOBAL_DEFS  = -D_WIN32 -DWIN32 -DGLEW_STATIC -D_CRT_SECURE_NO_WARNINGS

CXXFLAGS += $(GLOBAL_INC) $(GLOBAL_DEFS)
CFLAGS   += $(GLOBAL_INC) $(GLOBAL_DEFS)

# ---- Linker ----
LDFLAGS = -static-libgcc -static-libstdc++ $(ARCH_F)
LDLIBS  = -lwinmm -lole32 -lshell32 -lws2_32 -lopengl32 -lglu32 -lgdi32 \
          -lshlwapi -lsetupapi -limm32 -lversion -ldinput8 -ldxguid -luuid -loleaut32

# ============================================================
#  MAIN APPLICATION SOURCES
# ============================================================
MAIN_SRCS = \
  $(MAIN)/main.cpp \
  $(SSZ)/ssz.cpp \
  $(MAIN)/lua/lua.cpp \
  $(MAIN)/file/file.cpp \
  $(MAIN)/sound/sound.cpp \
  $(MAIN)/math/math.cpp \
  $(MAIN)/sdlplugin/sdlplugin.cpp \
  $(MAIN)/shell/shell.cpp \
  $(MAIN)/socket/socket.cpp \
  $(MAIN)/thread/thread.cpp \
  $(MAIN)/time/time.cpp \
  $(MAIN)/alert/alert.cpp \
  $(MAIN)/mesdialog/mesdialog.cpp \
  $(MAIN)/ogg/ogg.cpp \
  $(MAIN)/regex/regex.cpp

SCRIPT_SRCS = \
  $(SCRIPT)/alert.cpp \
  $(SCRIPT)/alpha/lua.cpp \
  $(SCRIPT)/alpha/mesdialog.cpp \
  $(SCRIPT)/alpha/ogg.cpp \
  $(SCRIPT)/alpha/sdlevent.cpp \
  $(SCRIPT)/alpha/sdlplugin.cpp \
  $(SCRIPT)/arcfour.cpp \
  $(SCRIPT)/ssz/animation.cpp \
  $(SCRIPT)/ssz/action.cpp \
  $(SCRIPT)/ssz/bg.cpp \
  $(SCRIPT)/ssz/command.cpp \
  $(SCRIPT)/ssz/common.cpp \
  $(SCRIPT)/ssz/debug-script.cpp \
  $(SCRIPT)/ssz/fight.cpp \
  $(SCRIPT)/ssz/fighting.cpp \
  $(SCRIPT)/ssz/font.cpp \
  $(SCRIPT)/ssz/ikemen.cpp \
  $(SCRIPT)/ssz/loader.cpp \
  $(SCRIPT)/ssz/script.cpp \
  $(SCRIPT)/ssz/sff.cpp \
  $(SCRIPT)/ssz/share.cpp \
  $(SCRIPT)/ssz/stage.cpp \
  $(SCRIPT)/ssz/sound.cpp \
  $(SCRIPT)/ssz/char.cpp \
  $(SCRIPT)/ssz/statebuilder.cpp \
  $(SCRIPT)/ssz/system-script.cpp \
  $(SCRIPT)/ssz/trigger-script.cpp \
  $(SCRIPT)/ssz/system.cpp \
  $(SCRIPT)/ssz/video.cpp \
  $(SCRIPT)/base64.cpp \
  $(SCRIPT)/file.cpp \
  $(SCRIPT)/math.cpp \
  $(SCRIPT)/md5.cpp \
  $(SCRIPT)/regex.cpp \
  $(SCRIPT)/shell.cpp \
  $(SCRIPT)/socket.cpp \
  $(SCRIPT)/sound.cpp \
  $(SCRIPT)/ssz.cpp \
  $(SCRIPT)/ssz/mugen_sff_loader.cpp \
  $(SCRIPT)/string.cpp \
  $(SCRIPT)/thread.cpp \
  $(SCRIPT)/time.cpp

SCRIPT_OBJS = $(patsubst $(SCRIPT)/%.cpp,$(BLD)/script/%.o,$(SCRIPT_SRCS))

MAIN_OBJS = $(patsubst $(MAIN)/%.cpp,$(BLD)/main/%.o,$(MAIN_SRCS))

# ============================================================
#  SDL2  (167 sources — Windows backend only)
# ============================================================
SDL2_DIR    = $(EXT)/SDL2-2.0.20
SDL2_SRCS   = \
  $(SDL2_DIR)/src/atomic/SDL_atomic.c \
  $(SDL2_DIR)/src/atomic/SDL_spinlock.c \
  $(SDL2_DIR)/src/audio/directsound/SDL_directsound.c \
  $(SDL2_DIR)/src/audio/disk/SDL_diskaudio.c \
  $(SDL2_DIR)/src/audio/dummy/SDL_dummyaudio.c \
  $(SDL2_DIR)/src/audio/SDL_audio.c \
  $(SDL2_DIR)/src/audio/SDL_audiocvt.c \
  $(SDL2_DIR)/src/audio/SDL_audiodev.c \
  $(SDL2_DIR)/src/audio/SDL_audiotypecvt.c \
  $(SDL2_DIR)/src/audio/SDL_mixer.c \
  $(SDL2_DIR)/src/audio/SDL_wave.c \
  $(SDL2_DIR)/src/audio/winmm/SDL_winmm.c \
  $(SDL2_DIR)/src/audio/wasapi/SDL_wasapi.c \
  $(SDL2_DIR)/src/audio/wasapi/SDL_wasapi_win32.c \
  $(SDL2_DIR)/src/core/windows/SDL_hid.c \
  $(SDL2_DIR)/src/core/windows/SDL_windows.c \
  $(SDL2_DIR)/src/core/windows/SDL_xinput.c \
  $(SDL2_DIR)/src/cpuinfo/SDL_cpuinfo.c \
  $(SDL2_DIR)/src/dynapi/SDL_dynapi.c \
  $(SDL2_DIR)/src/events/SDL_clipboardevents.c \
  $(SDL2_DIR)/src/events/SDL_displayevents.c \
  $(SDL2_DIR)/src/events/SDL_dropevents.c \
  $(SDL2_DIR)/src/events/SDL_events.c \
  $(SDL2_DIR)/src/events/SDL_gesture.c \
  $(SDL2_DIR)/src/events/SDL_keyboard.c \
  $(SDL2_DIR)/src/events/SDL_mouse.c \
  $(SDL2_DIR)/src/events/SDL_quit.c \
  $(SDL2_DIR)/src/events/SDL_touch.c \
  $(SDL2_DIR)/src/events/SDL_windowevents.c \
  $(SDL2_DIR)/src/file/SDL_rwops.c \
  $(SDL2_DIR)/src/filesystem/windows/SDL_sysfilesystem.c \
  $(SDL2_DIR)/src/haptic/dummy/SDL_syshaptic.c \
  $(SDL2_DIR)/src/haptic/SDL_haptic.c \
  $(SDL2_DIR)/src/haptic/windows/SDL_dinputhaptic.c \
  $(SDL2_DIR)/src/haptic/windows/SDL_windowshaptic.c \
  $(SDL2_DIR)/src/haptic/windows/SDL_xinputhaptic.c \
  $(SDL2_DIR)/src/hidapi/SDL_hidapi.c \
  $(SDL2_DIR)/src/joystick/dummy/SDL_sysjoystick.c \
  $(SDL2_DIR)/src/joystick/hidapi/SDL_hidapijoystick.c \
  $(SDL2_DIR)/src/joystick/hidapi/SDL_hidapi_gamecube.c \
  $(SDL2_DIR)/src/joystick/hidapi/SDL_hidapi_luna.c \
  $(SDL2_DIR)/src/joystick/hidapi/SDL_hidapi_ps4.c \
  $(SDL2_DIR)/src/joystick/hidapi/SDL_hidapi_ps5.c \
  $(SDL2_DIR)/src/joystick/hidapi/SDL_hidapi_rumble.c \
  $(SDL2_DIR)/src/joystick/hidapi/SDL_hidapi_stadia.c \
  $(SDL2_DIR)/src/joystick/hidapi/SDL_hidapi_switch.c \
  $(SDL2_DIR)/src/joystick/hidapi/SDL_hidapi_xbox360.c \
  $(SDL2_DIR)/src/joystick/hidapi/SDL_hidapi_xbox360w.c \
  $(SDL2_DIR)/src/joystick/hidapi/SDL_hidapi_xboxone.c \
  $(SDL2_DIR)/src/joystick/SDL_gamecontroller.c \
  $(SDL2_DIR)/src/joystick/SDL_joystick.c \
  $(SDL2_DIR)/src/joystick/virtual/SDL_virtualjoystick.c \
  $(SDL2_DIR)/src/joystick/windows/SDL_dinputjoystick.c \
  $(SDL2_DIR)/src/joystick/windows/SDL_windowsjoystick.c \
  $(SDL2_DIR)/src/joystick/windows/SDL_xinputjoystick.c \
  $(SDL2_DIR)/src/joystick/windows/SDL_rawinputjoystick.c \
  $(SDL2_DIR)/src/joystick/windows/sdl_wgi_stub.c \
  $(SDL2_DIR)/src/libm/e_atan2.c \
  $(SDL2_DIR)/src/libm/e_exp.c \
  $(SDL2_DIR)/src/libm/e_fmod.c \
  $(SDL2_DIR)/src/libm/e_log.c \
  $(SDL2_DIR)/src/libm/e_log10.c \
  $(SDL2_DIR)/src/libm/e_pow.c \
  $(SDL2_DIR)/src/libm/e_rem_pio2.c \
  $(SDL2_DIR)/src/libm/e_sqrt.c \
  $(SDL2_DIR)/src/libm/k_cos.c \
  $(SDL2_DIR)/src/libm/k_rem_pio2.c \
  $(SDL2_DIR)/src/libm/k_sin.c \
  $(SDL2_DIR)/src/libm/k_tan.c \
  $(SDL2_DIR)/src/libm/s_atan.c \
  $(SDL2_DIR)/src/libm/s_copysign.c \
  $(SDL2_DIR)/src/libm/s_cos.c \
  $(SDL2_DIR)/src/libm/s_fabs.c \
  $(SDL2_DIR)/src/libm/s_floor.c \
  $(SDL2_DIR)/src/libm/s_scalbn.c \
  $(SDL2_DIR)/src/libm/s_sin.c \
  $(SDL2_DIR)/src/libm/s_tan.c \
  $(SDL2_DIR)/src/loadso/windows/SDL_sysloadso.c \
  $(SDL2_DIR)/src/locale/SDL_locale.c \
  $(SDL2_DIR)/src/locale/windows/SDL_syslocale.c \
  $(SDL2_DIR)/src/misc/SDL_url.c \
  $(SDL2_DIR)/src/misc/windows/SDL_sysurl.c \
  $(SDL2_DIR)/src/power/SDL_power.c \
  $(SDL2_DIR)/src/power/windows/SDL_syspower.c \
  $(SDL2_DIR)/src/render/opengl/SDL_render_gl.c \
  $(SDL2_DIR)/src/render/opengl/SDL_shaders_gl.c \
  $(SDL2_DIR)/src/render/direct3d/SDL_render_d3d.c \
  $(SDL2_DIR)/src/render/direct3d/SDL_shaders_d3d.c \
  $(SDL2_DIR)/src/render/SDL_d3dmath.c \
  $(SDL2_DIR)/src/render/direct3d11/SDL_render_d3d11.c \
  $(SDL2_DIR)/src/render/direct3d11/SDL_shaders_d3d11.c \
  $(SDL2_DIR)/src/render/SDL_render.c \
  $(SDL2_DIR)/src/render/SDL_yuv_sw.c \
  $(SDL2_DIR)/src/render/software/SDL_blendfillrect.c \
  $(SDL2_DIR)/src/render/software/SDL_blendline.c \
  $(SDL2_DIR)/src/render/software/SDL_blendpoint.c \
  $(SDL2_DIR)/src/render/software/SDL_drawline.c \
  $(SDL2_DIR)/src/render/software/SDL_drawpoint.c \
  $(SDL2_DIR)/src/render/software/SDL_render_sw.c \
  $(SDL2_DIR)/src/render/software/SDL_rotate.c \
  $(SDL2_DIR)/src/render/software/SDL_triangle.c \
  $(SDL2_DIR)/src/SDL.c \
  $(SDL2_DIR)/src/SDL_assert.c \
  $(SDL2_DIR)/src/SDL_dataqueue.c \
  $(SDL2_DIR)/src/SDL_error.c \
  $(SDL2_DIR)/src/SDL_hints.c \
  $(SDL2_DIR)/src/SDL_log.c \
  $(SDL2_DIR)/src/sensor/dummy/SDL_dummysensor.c \
  $(SDL2_DIR)/src/sensor/SDL_sensor.c \
  $(SDL2_DIR)/src/sensor/windows/SDL_windowssensor.c \
  $(SDL2_DIR)/src/stdlib/SDL_crc32.c \
  $(SDL2_DIR)/src/stdlib/SDL_getenv.c \
  $(SDL2_DIR)/src/stdlib/SDL_iconv.c \
  $(SDL2_DIR)/src/stdlib/SDL_malloc.c \
  $(SDL2_DIR)/src/stdlib/SDL_qsort.c \
  $(SDL2_DIR)/src/stdlib/SDL_stdlib.c \
  $(SDL2_DIR)/src/stdlib/SDL_string.c \
  $(SDL2_DIR)/src/stdlib/SDL_strtokr.c \
  $(SDL2_DIR)/src/thread/generic/SDL_syscond.c \
  $(SDL2_DIR)/src/thread/SDL_thread.c \
  $(SDL2_DIR)/src/thread/windows/SDL_syscond_cv.c \
  $(SDL2_DIR)/src/thread/windows/SDL_sysmutex.c \
  $(SDL2_DIR)/src/thread/windows/SDL_syssem.c \
  $(SDL2_DIR)/src/thread/windows/SDL_systhread.c \
  $(SDL2_DIR)/src/thread/windows/SDL_systls.c \
  $(SDL2_DIR)/src/timer/SDL_timer.c \
  $(SDL2_DIR)/src/timer/windows/SDL_systimer.c \
  $(SDL2_DIR)/src/video/dummy/SDL_nullevents.c \
  $(SDL2_DIR)/src/video/dummy/SDL_nullframebuffer.c \
  $(SDL2_DIR)/src/video/dummy/SDL_nullvideo.c \
  $(SDL2_DIR)/src/video/windows/SDL_windowsvulkan.c \
  $(SDL2_DIR)/src/video/SDL_blit.c \
  $(SDL2_DIR)/src/video/SDL_blit_0.c \
  $(SDL2_DIR)/src/video/SDL_blit_1.c \
  $(SDL2_DIR)/src/video/SDL_blit_A.c \
  $(SDL2_DIR)/src/video/SDL_blit_auto.c \
  $(SDL2_DIR)/src/video/SDL_blit_copy.c \
  $(SDL2_DIR)/src/video/SDL_blit_N.c \
  $(SDL2_DIR)/src/video/SDL_blit_slow.c \
  $(SDL2_DIR)/src/video/SDL_bmp.c \
  $(SDL2_DIR)/src/video/SDL_clipboard.c \
  $(SDL2_DIR)/src/video/SDL_fillrect.c \
  $(SDL2_DIR)/src/video/SDL_pixels.c \
  $(SDL2_DIR)/src/video/SDL_rect.c \
  $(SDL2_DIR)/src/video/SDL_RLEaccel.c \
  $(SDL2_DIR)/src/video/SDL_shape.c \
  $(SDL2_DIR)/src/video/SDL_stretch.c \
  $(SDL2_DIR)/src/video/SDL_surface.c \
  $(SDL2_DIR)/src/video/SDL_video.c \
  $(SDL2_DIR)/src/video/SDL_yuv.c \
  $(SDL2_DIR)/src/video/SDL_vulkan_utils.c \
  $(SDL2_DIR)/src/video/windows/SDL_windowsclipboard.c \
  $(SDL2_DIR)/src/video/windows/SDL_windowsevents.c \
  $(SDL2_DIR)/src/video/windows/SDL_windowsframebuffer.c \
  $(SDL2_DIR)/src/video/windows/SDL_windowskeyboard.c \
  $(SDL2_DIR)/src/video/windows/SDL_windowsmessagebox.c \
  $(SDL2_DIR)/src/video/windows/SDL_windowsmodes.c \
  $(SDL2_DIR)/src/video/windows/SDL_windowsmouse.c \
  $(SDL2_DIR)/src/video/windows/SDL_windowsopengl.c \
  $(SDL2_DIR)/src/video/windows/SDL_windowsshape.c \
  $(SDL2_DIR)/src/video/windows/SDL_windowsvideo.c \
  $(SDL2_DIR)/src/video/windows/SDL_windowswindow.c \
  $(SDL2_DIR)/src/video/yuv2rgb/yuv_rgb.c
SDL2_DEFS  = -DSDL_VIDEO_OPENGL_EGL=0 -DSDL_VIDEO_OPENGL_ES2=0 -DSDL_VIDEO_RENDER_OGL_ES2=0 -DHAVE_LIBC -D_CRT_SECURE_NO_WARNINGS -DSDL_HIDAPI_DISABLED -DSDL_JOYSTICK_WGI=0
SDL2_OBJS  = $(patsubst $(SDL2_DIR)/src/%.c,$(BLD)/sdl2/%.o,$(SDL2_SRCS))

# ---- SDL2 image ----
SDL2IMG_DIR  = $(EXT)/SDL2_image-2.0.5
SDL2IMG_SRCS = $(addprefix $(SDL2IMG_DIR)/, IMG.c IMG_bmp.c IMG_gif.c IMG_jpg.c IMG_lbm.c IMG_pcx.c IMG_png.c IMG_pnm.c IMG_svg.c IMG_tga.c IMG_tif.c IMG_xcf.c IMG_xpm.c IMG_xv.c IMG_webp.c)
SDL2IMG_DEFS = -DLOAD_BMP -DLOAD_GIF -DLOAD_JPG -DLOAD_LBM -DLOAD_PCX -DLOAD_PNG -DLOAD_PNM -DLOAD_SVG -DLOAD_TGA -DLOAD_XCF -DLOAD_XPM -DLOAD_XV -DLOAD_WEBP
SDL2IMG_OBJS = $(patsubst $(SDL2IMG_DIR)/%.c,$(BLD)/sdl2img/%.o,$(SDL2IMG_SRCS))

# ---- SDL2 ttf ----
SDL2TTF_DIR  = $(EXT)/SDL2_ttf-2.0.18
SDL2TTF_SRCS = $(SDL2TTF_DIR)/SDL_ttf.c
SDL2TTF_OBJS = $(patsubst $(SDL2TTF_DIR)/%.c,$(BLD)/sdl2ttf/%.o,$(SDL2TTF_SRCS))

# ---- SDL2 mixer ----
SDL2MIX_DIR  = $(EXT)/SDL2_mixer-2.0.4
SDL2MIX_SRCS = $(addprefix $(SDL2MIX_DIR)/, effects_internal.c effect_position.c effect_stereoreverse.c load_aiff.c load_voc.c mixer.c music.c music_cmd.c music_flac.c music_modplug.c music_mpg123.c music_nativemidi.c music_ogg.c music_opus.c music_wav.c native_midi/native_midi_win32.c native_midi/native_midi_common.c)
SDL2MIX_DEFS = -DMUSIC_WAV -DMUSIC_MOD_MODPLUG -DMODPLUG_STATIC -DMUSIC_OGG -DMUSIC_OPUS -DMUSIC_FLAC -DFLAC__NO_DLL -DMUSIC_MP3_MPG123 -DMUSIC_MID_NATIVE
SDL2MIX_OBJS = $(patsubst $(SDL2MIX_DIR)/%.c,$(BLD)/sdl2mix/%.o,$(SDL2MIX_SRCS))

# ---- Lua ----
LUA_DIR   = $(EXT)/lua-5.2.4
LUA_SRCS  = $(addprefix $(LUA_DIR)/, lapi.c lauxlib.c lbaselib.c lbitlib.c lcode.c lcorolib.c lctype.c ldblib.c ldebug.c ldo.c ldump.c lfunc.c lgc.c linit.c liolib.c llex.c lmathlib.c lmem.c loadlib.c lobject.c lopcodes.c loslib.c lparser.c lstate.c lstring.c lstrlib.c ltable.c ltablib.c ltm.c lundump.c lvm.c lzio.c lfs.c \
              ffi/ffi.c ffi/call.c ffi/ctype.c ffi/parser.c \
              lpeg-1.1.0/lpcap.c lpeg-1.1.0/lpcode.c lpeg-1.1.0/lpcset.c lpeg-1.1.0/lpprint.c lpeg-1.1.0/lptree.c lpeg-1.1.0/lpvm.c)
LUA_DEFS  = -DLUA_COMPAT_LOADSTRING -DLUA_COMPAT_MODULE
LUA_OBJS  = $(patsubst $(LUA_DIR)/%.c,$(BLD)/lua/%.o,$(LUA_SRCS))

# ---- zlib ----
ZLIB_DIR  = $(EXT)/zlib-1.2.8
ZLIB_SRCS = $(addprefix $(ZLIB_DIR)/, adler32.c compress.c contrib/inflate86/inffas86.c crc32.c deflate.c inflate.c inftrees.c trees.c zutil.c)
ZLIB_OBJS = $(patsubst $(ZLIB_DIR)/%.c,$(BLD)/zlib/%.o,$(ZLIB_SRCS))

# ---- libogg ----
OGG_DIR  = $(EXT)/libogg-1.3.6
OGG_SRCS = $(addprefix $(OGG_DIR)/src/, bitwise.c framing.c)
OGG_OBJS = $(patsubst $(OGG_DIR)/src/%.c,$(BLD)/ogg/%.o,$(OGG_SRCS))

# ---- libvorbis ----
VORBIS_DIR  = $(EXT)/libvorbis-1.3.7
VORBIS_SRCS = $(addprefix $(VORBIS_DIR)/lib/, analysis.c bitrate.c block.c codebook.c envelope.c floor0.c floor1.c info.c lookup.c lpc.c lsp.c mapping0.c mdct.c psy.c registry.c res0.c sharedbook.c smallft.c synthesis.c vorbisenc.c window.c vorbisfile.c)
VORBIS_OBJS = $(patsubst $(VORBIS_DIR)/lib/%.c,$(BLD)/vorbis/%.o,$(VORBIS_SRCS))
VORBIS_CFLAGS = -I $(VORBIS_DIR)/lib

# ---- libpng ----
PNG_DIR  = $(EXT)/lpng1655
PNG_SRCS = $(addprefix $(PNG_DIR)/, png.c pngerror.c pngget.c pngmem.c pngpread.c pngread.c pngrio.c pngrtran.c pngrutil.c pngset.c pngtrans.c pngwio.c pngwrite.c pngwtran.c pngwutil.c)
PNG_OBJS = $(patsubst $(PNG_DIR)/%.c,$(BLD)/png/%.o,$(PNG_SRCS))

# ---- GLEW ----
GLEW_DIR  = $(EXT)/glew-2.2.0
GLEW_SRCS = $(GLEW_DIR)/src/glew.c
GLEW_DEFS = -DGLEW_STATIC -DWIN32_LEAN_AND_MEAN
GLEW_OBJS = $(patsubst $(GLEW_DIR)/src/%.c,$(BLD)/glew/%.o,$(GLEW_SRCS))

# ---- FreeType ----
FT_DIR  = $(EXT)/freetype-2.10.4
FT_SRCS = $(addprefix $(FT_DIR)/, \
  src/autofit/autofit.c src/base/ftbase.c src/base/ftbbox.c src/base/ftbdf.c \
  src/base/ftbitmap.c src/base/ftcid.c src/base/ftfstype.c src/base/ftgasp.c \
  src/base/ftglyph.c src/base/ftgxval.c src/base/ftinit.c src/base/ftmm.c \
  src/base/ftotval.c src/base/ftpatent.c src/base/ftpfr.c src/base/ftstroke.c \
  src/base/ftsynth.c src/base/ftsystem.c src/base/fttype1.c src/base/ftwinfnt.c \
  src/bdf/bdf.c src/cache/ftcache.c src/cff/cff.c src/cid/type1cid.c \
  src/gzip/ftgzip.c src/lzw/ftlzw.c src/pcf/pcf.c src/pfr/pfr.c \
  src/psaux/psaux.c src/pshinter/pshinter.c src/psnames/psmodule.c \
  src/raster/raster.c src/sfnt/sfnt.c src/smooth/smooth.c src/truetype/truetype.c \
  src/type1/type1.c src/type42/type42.c src/winfonts/winfnt.c src/base/ftdebug.c)
FT_DEFS  = -DFT2_BUILD_LIBRARY
FT_OBJS  = $(patsubst $(FT_DIR)/%.c,$(BLD)/ft/%.o,$(FT_SRCS))

# ---- jpeg ----
JPEG_DIR  = $(EXT)/jpeg-9b
JPEG_SRCS = $(addprefix $(JPEG_DIR)/, \
  jcapimin.c jcapistd.c jdapimin.c jdapistd.c jcomapi.c jcparam.c jctrans.c jdtrans.c \
  jcinit.c jcmaster.c jcmainct.c jcprepct.c jccoefct.c jccolor.c jcsample.c jcdctmgr.c \
  jfdctint.c jfdctfst.c jfdctflt.c jchuff.c jcarith.c jcmarker.c jdatadst.c jdmaster.c \
  jdinput.c jdmainct.c jdcoefct.c jdpostct.c jdmarker.c jdhuff.c jdarith.c jddctmgr.c \
  jidctint.c jidctfst.c jidctflt.c jdsample.c jdcolor.c jdmerge.c jquant1.c jquant2.c \
  jdatasrc.c jaricom.c jerror.c jmemmgr.c jutils.c jmemansi.c)
JPEG_OBJS = $(patsubst $(JPEG_DIR)/%.c,$(BLD)/jpeg/%.o,$(JPEG_SRCS))

# ---- webp ----
WEBP_DIR  = $(EXT)/libwebp-1.0.2
WEBP_SRCS = $(addprefix $(WEBP_DIR)/src/, \
  utils/bit_reader_utils.c utils/bit_writer_utils.c utils/color_cache_utils.c \
  utils/filters_utils.c utils/huffman_encode_utils.c utils/huffman_utils.c \
  utils/quant_levels_dec_utils.c utils/quant_levels_utils.c utils/random_utils.c \
  utils/rescaler_utils.c utils/thread_utils.c utils/utils.c \
  enc/alpha_enc.c enc/analysis_enc.c enc/backward_references_cost_enc.c \
  enc/backward_references_enc.c enc/config_enc.c enc/cost_enc.c enc/filter_enc.c \
  enc/frame_enc.c enc/histogram_enc.c enc/iterator_enc.c enc/near_lossless_enc.c \
  enc/picture_csp_enc.c enc/picture_enc.c enc/picture_psnr_enc.c \
  enc/picture_rescale_enc.c enc/picture_tools_enc.c enc/predictor_enc.c \
  enc/quant_enc.c enc/syntax_enc.c enc/token_enc.c enc/tree_enc.c enc/vp8l_enc.c \
  enc/webp_enc.c \
  dsp/alpha_processing.c dsp/cost.c dsp/cpu.c dsp/dec.c dsp/dec_clip_tables.c \
  dsp/enc.c dsp/filters.c dsp/lossless.c dsp/lossless_enc.c dsp/rescaler.c \
  dsp/ssim.c dsp/upsampling.c dsp/yuv.c \
  dsp/alpha_processing_sse2.c dsp/cost_sse2.c dsp/dec_sse2.c dsp/enc_sse2.c \
  dsp/filters_sse2.c dsp/lossless_sse2.c dsp/lossless_enc_sse2.c \
  dsp/rescaler_sse2.c dsp/ssim_sse2.c dsp/upsampling_sse2.c dsp/yuv_sse2.c \
  dsp/alpha_processing_sse41.c dsp/dec_sse41.c dsp/enc_sse41.c \
  dsp/lossless_enc_sse41.c dsp/upsampling_sse41.c dsp/yuv_sse41.c \
  dec/alpha_dec.c dec/buffer_dec.c dec/frame_dec.c dec/idec_dec.c dec/io_dec.c \
  dec/quant_dec.c dec/tree_dec.c dec/vp8_dec.c dec/vp8l_dec.c dec/webp_dec.c)
WEBP_OBJS = $(patsubst $(WEBP_DIR)/src/%.c,$(BLD)/webp/%.o,$(WEBP_SRCS))

# ---- PortAudio ----
PA_DIR  = $(EXT)/portaudio
PA_SRCS = $(addprefix $(PA_DIR)/src/, \
  common/pa_allocation.c common/pa_converters.c common/pa_cpuload.c \
  common/pa_debugprint.c common/pa_dither.c common/pa_front.c common/pa_process.c \
  common/pa_ringbuffer.c common/pa_stream.c common/pa_trace.c \
  hostapi/wasapi/pa_win_wasapi.c os/win/pa_win_coinitialize.c \
  os/win/pa_win_hostapis.c os/win/pa_win_util.c os/win/pa_win_waveformat.c)
PA_DEFS  = -DPA_USE_WASAPI
PA_OBJS  = $(patsubst $(PA_DIR)/src/%.c,$(BLD)/pa/%.o,$(PA_SRCS))

# ---- mpg123 ----
MPG_DIR  = $(EXT)/mpg123-1.25.6
MPG_SRCS = $(addprefix $(MPG_DIR)/src/, \
  common.c compat/compat.c compat/compat_str.c \
  libmpg123/dct64.c libmpg123/equalizer.c libmpg123/feature.c libmpg123/format.c \
  libmpg123/frame.c libmpg123/icy.c libmpg123/icy2utf8.c libmpg123/id3.c \
  libmpg123/index.c libmpg123/layer1.c libmpg123/layer2.c libmpg123/layer3.c \
  libmpg123/libmpg123.c libmpg123/ntom.c libmpg123/optimize.c libmpg123/parse.c \
  libmpg123/readers.c libmpg123/stringbuf.c libmpg123/synth.c \
  libmpg123/synth_8bit.c libmpg123/synth_real.c libmpg123/synth_s32.c \
  libmpg123/tabinit.c msvc.c)
MPG_DEFS  = -DOPT_GENERIC -include winsock2.h
MPG_OBJS  = $(patsubst $(MPG_DIR)/src/%.c,$(BLD)/mpg123/%.o,$(MPG_SRCS))

# ---- Opus ----
OPUS_DIR  = $(EXT)/opus-1.6.1
OPUS_SRCS = $(addprefix $(OPUS_DIR)/, \
  src/opus.c src/opus_decoder.c src/opus_encoder.c src/extensions.c \
  src/opus_multistream.c src/opus_multistream_encoder.c src/opus_multistream_decoder.c \
  src/repacketizer.c src/opus_projection_encoder.c src/opus_projection_decoder.c \
  src/mapping_matrix.c src/analysis.c src/mlp.c src/mlp_data.c \
  celt/bands.c celt/celt.c celt/celt_encoder.c celt/celt_decoder.c celt/cwrs.c \
  celt/entcode.c celt/entdec.c celt/entenc.c celt/kiss_fft.c celt/laplace.c \
  celt/mathops.c celt/mdct.c celt/modes.c celt/pitch.c celt/celt_lpc.c \
  celt/quant_bands.c celt/rate.c celt/vq.c \
  silk/CNG.c silk/code_signs.c silk/init_decoder.c silk/decode_core.c \
  silk/decode_frame.c silk/decode_parameters.c silk/decode_indices.c \
  silk/decode_pulses.c silk/decoder_set_fs.c silk/dec_API.c silk/enc_API.c \
  silk/encode_indices.c silk/encode_pulses.c silk/gain_quant.c silk/interpolate.c \
  silk/LP_variable_cutoff.c silk/NLSF_decode.c silk/NSQ.c silk/NSQ_del_dec.c \
  silk/PLC.c silk/shell_coder.c silk/tables_gain.c silk/tables_LTP.c \
  silk/tables_NLSF_CB_NB_MB.c silk/tables_NLSF_CB_WB.c silk/tables_other.c \
  silk/tables_pitch_lag.c silk/tables_pulses_per_block.c silk/VAD.c \
  silk/control_audio_bandwidth.c silk/quant_LTP_gains.c silk/VQ_WMat_EC.c \
  silk/HP_variable_cutoff.c silk/NLSF_encode.c silk/NLSF_VQ.c silk/NLSF_unpack.c \
  silk/NLSF_del_dec_quant.c silk/process_NLSFs.c silk/stereo_LR_to_MS.c \
  silk/stereo_MS_to_LR.c silk/check_control_input.c silk/control_SNR.c \
  silk/init_encoder.c silk/control_codec.c silk/A2NLSF.c silk/ana_filt_bank_1.c \
  silk/biquad_alt.c silk/bwexpander_32.c silk/bwexpander.c silk/debug.c \
  silk/decode_pitch.c silk/inner_prod_aligned.c silk/lin2log.c silk/log2lin.c \
  silk/LPC_analysis_filter.c silk/LPC_inv_pred_gain.c silk/table_LSF_cos.c \
  silk/NLSF2A.c silk/NLSF_stabilize.c silk/NLSF_VQ_weights_laroia.c \
  silk/pitch_est_tables.c silk/resampler.c silk/resampler_down2_3.c \
  silk/resampler_down2.c silk/resampler_private_AR2.c silk/resampler_private_down_FIR.c \
  silk/resampler_private_IIR_FIR.c silk/resampler_private_up2_HQ.c \
  silk/resampler_rom.c silk/sigm_Q15.c silk/sort.c silk/sum_sqr_shift.c \
  silk/stereo_decode_pred.c silk/stereo_encode_pred.c silk/stereo_find_predictor.c \
  silk/stereo_quant_pred.c silk/LPC_fit.c \
  silk/float/apply_sine_window_FLP.c silk/float/corrMatrix_FLP.c \
  silk/float/encode_frame_FLP.c silk/float/find_LPC_FLP.c silk/float/find_LTP_FLP.c \
  silk/float/find_pitch_lags_FLP.c silk/float/find_pred_coefs_FLP.c \
  silk/float/LPC_analysis_filter_FLP.c silk/float/LTP_analysis_filter_FLP.c \
  silk/float/LTP_scale_ctrl_FLP.c silk/float/noise_shape_analysis_FLP.c \
  silk/float/process_gains_FLP.c silk/float/regularize_correlations_FLP.c \
  silk/float/residual_energy_FLP.c silk/float/warped_autocorrelation_FLP.c \
  silk/float/wrappers_FLP.c silk/float/autocorrelation_FLP.c \
  silk/float/burg_modified_FLP.c silk/float/bwexpander_FLP.c silk/float/energy_FLP.c \
  silk/float/inner_product_FLP.c silk/float/k2a_FLP.c \
  silk/float/LPC_inv_pred_gain_FLP.c silk/float/pitch_analysis_core_FLP.c \
  silk/float/scale_copy_vector_FLP.c silk/float/scale_vector_FLP.c \
  silk/float/schur_FLP.c silk/float/sort_FLP.c)
OPUS_DEFS  = -DOPUS_BUILD -DUSE_ALLOCA
OPUS_OBJS  = $(patsubst $(OPUS_DIR)/%.c,$(BLD)/opus/%.o,$(OPUS_SRCS))

# ---- Opusfile ----
OPUSF_DIR  = $(EXT)/opusfile-0.12
OPUSF_SRCS = $(addprefix $(OPUSF_DIR)/src/, http.c info.c internal.c opusfile.c stream.c wincerts.c)
OPUSF_OBJS = $(patsubst $(OPUSF_DIR)/src/%.c,$(BLD)/opusf/%.o,$(OPUSF_SRCS))

# ---- modplug ----
MOD_DIR  = $(EXT)/libmodplug-sezero
MOD_SRCS = $(addprefix $(MOD_DIR)/src/, \
  fastmix.cpp load_669.cpp load_abc.cpp load_amf.cpp load_ams.cpp load_dbm.cpp \
  load_dmf.cpp load_dsm.cpp load_far.cpp load_gdm.cpp load_it.cpp load_mdl.cpp \
  load_med.cpp load_mid.cpp load_mod.cpp load_mt2.cpp load_mtm.cpp load_okt.cpp \
  load_pat.cpp load_psm.cpp load_ptm.cpp load_s3m.cpp load_stm.cpp load_ult.cpp \
  load_umx.cpp load_wav.cpp load_xm.cpp mmcmp.cpp modplug.cpp snd_dsp.cpp \
  snd_flt.cpp snd_fx.cpp sndfile.cpp sndmix.cpp)
MOD_DEFS  = -DMODPLUG_STATIC -DMODPLUG_BUILD -DMMCMP_SUPPORT -DNO_CXX_EXPORTS
MOD_OBJS  = $(patsubst $(MOD_DIR)/src/%.cpp,$(BLD)/modplug/%.o,$(MOD_SRCS))

# ---- FLAC ----
FLAC_DIR  = $(EXT)/flac-1.3.2
FLAC_SRCS = $(addprefix $(FLAC_DIR)/src/libFLAC/, \
  bitmath.c bitreader.c bitwriter.c cpu.c crc.c fixed.c fixed_intrin_sse2.c \
  fixed_intrin_ssse3.c float.c format.c lpc.c lpc_intrin_avx2.c lpc_intrin_sse.c \
  lpc_intrin_sse2.c lpc_intrin_sse41.c md5.c memory.c metadata_iterators.c \
  metadata_object.c ogg_decoder_aspect.c ogg_encoder_aspect.c ogg_helper.c \
  ogg_mapping.c stream_decoder.c stream_encoder.c stream_encoder_framing.c \
  stream_encoder_intrin_avx2.c stream_encoder_intrin_sse2.c \
  stream_encoder_intrin_ssse3.c window.c windows_unicode_filenames.c)
FLAC_DEFS  = -DFLAC__NO_DLL -DFLAC__HAS_OGG=1 -DHAVE_FSEEKO -DHAVE_FTELLO -DHAVE_LROUND
FLAC_OBJS  = $(patsubst $(FLAC_DIR)/src/libFLAC/%.c,$(BLD)/flac/%.o,$(FLAC_SRCS))

# ---- Per-library static archives (to avoid "arg list too long") ----
LIB_SDL2    = $(BLD)/libsdl2.a
LIB_SDL2IMG = $(BLD)/libsdl2img.a
LIB_SDL2TTF = $(BLD)/libsdl2ttf.a
LIB_SDL2MIX = $(BLD)/libsdl2mix.a
LIB_LUA     = $(BLD)/liblua.a
LIB_ZLIB    = $(BLD)/libzlib.a
LIB_OGG     = $(BLD)/libogg.a
LIB_VORBIS  = $(BLD)/libvorbis.a
LIB_PNG     = $(BLD)/libpng.a
LIB_GLEW    = $(BLD)/libglew.a
LIB_FT      = $(BLD)/libft.a
LIB_JPEG    = $(BLD)/libjpeg.a
LIB_WEBP    = $(BLD)/libwebp.a
LIB_PA      = $(BLD)/libpa.a
LIB_MPG123  = $(BLD)/libmpg123.a
LIB_OPUS    = $(BLD)/libopus.a
LIB_OPUSF   = $(BLD)/libopusf.a
LIB_MODPLUG = $(BLD)/libmodplug.a
LIB_FLAC    = $(BLD)/libflac.a
ALL_LIBS    = $(LIB_SDL2) $(LIB_SDL2IMG) $(LIB_SDL2TTF) $(LIB_SDL2MIX) \
              $(LIB_LUA) $(LIB_GLEW) $(LIB_PA) $(LIB_MPG123) \
              $(LIB_OPUSF) $(LIB_MODPLUG) $(LIB_JPEG) $(LIB_WEBP) \
              $(LIB_VORBIS) $(LIB_FLAC) $(LIB_OPUS) $(LIB_FT) $(LIB_PNG) \
              $(LIB_ZLIB) $(LIB_OGG)

$(LIB_SDL2):    $(SDL2_OBJS)
	@mkdir -p $(dir $@)
	$(AR) $(ARFLAGS) $@ $^
$(LIB_SDL2IMG): $(SDL2IMG_OBJS)
	@mkdir -p $(dir $@)
	$(AR) $(ARFLAGS) $@ $^
$(LIB_SDL2TTF): $(SDL2TTF_OBJS)
	@mkdir -p $(dir $@)
	$(AR) $(ARFLAGS) $@ $^
$(LIB_SDL2MIX): $(SDL2MIX_OBJS)
	@mkdir -p $(dir $@)
	$(AR) $(ARFLAGS) $@ $^
$(LIB_LUA):     $(LUA_OBJS)
	@mkdir -p $(dir $@)
	$(AR) $(ARFLAGS) $@ $^
$(LIB_ZLIB):    $(ZLIB_OBJS)
	@mkdir -p $(dir $@)
	$(AR) $(ARFLAGS) $@ $^
$(LIB_OGG):     $(OGG_OBJS)
	@mkdir -p $(dir $@)
	$(AR) $(ARFLAGS) $@ $^
$(LIB_VORBIS):  $(VORBIS_OBJS)
	@mkdir -p $(dir $@)
	$(AR) $(ARFLAGS) $@ $^
$(LIB_PNG):     $(PNG_OBJS)
	@mkdir -p $(dir $@)
	$(AR) $(ARFLAGS) $@ $^
$(LIB_GLEW):    $(GLEW_OBJS)
	@mkdir -p $(dir $@)
	$(AR) $(ARFLAGS) $@ $^
$(LIB_FT):      $(FT_OBJS)
	@mkdir -p $(dir $@)
	$(AR) $(ARFLAGS) $@ $^
$(LIB_JPEG):    $(JPEG_OBJS)
	@mkdir -p $(dir $@)
	$(AR) $(ARFLAGS) $@ $^
$(LIB_WEBP):    $(WEBP_OBJS)
	@mkdir -p $(dir $@)
	$(AR) $(ARFLAGS) $@ $^
$(LIB_PA):      $(PA_OBJS)
	@mkdir -p $(dir $@)
	$(AR) $(ARFLAGS) $@ $^
$(LIB_MPG123):  $(MPG_OBJS)
	@mkdir -p $(dir $@)
	$(AR) $(ARFLAGS) $@ $^
$(LIB_OPUS):    $(OPUS_OBJS)
	@mkdir -p $(dir $@)
	$(AR) $(ARFLAGS) $@ $^
$(LIB_OPUSF):   $(OPUSF_OBJS)
	@mkdir -p $(dir $@)
	$(AR) $(ARFLAGS) $@ $^
$(LIB_MODPLUG): $(MOD_OBJS)
	@mkdir -p $(dir $@)
	$(AR) $(ARFLAGS) $@ $^
$(LIB_FLAC):    $(FLAC_OBJS)
	@mkdir -p $(dir $@)
	$(AR) $(ARFLAGS) $@ $^

.PHONY: all clean install
all: $(TARGET)
	@echo "=== Built: $(TARGET) ($(CONFIG), $(ARCH)) ==="

$(TARGET): $(MAIN_OBJS) $(SCRIPT_OBJS) $(ALL_LIBS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $(MAIN_OBJS) $(SCRIPT_OBJS) $(ALL_LIBS) $(LDFLAGS) $(LDLIBS)
ifeq ($(CONFIG),Release)
	$(W64DEVKIT)/bin/strip --strip-all $@
endif
	@echo "=== Built: $@ ($(CONFIG), $(ARCH)) ==="

# ---- Main application (C++) ----
$(BLD)/main/%.o: $(MAIN)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# ---- Script layer (C++) ----
$(BLD)/script/%.o: $(SCRIPT)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# ---- SDL2 (C) ----
$(BLD)/sdl2/%.o: $(SDL2_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I $(SDL2_DIR)/src $(SDL2_DEFS) -c -o $@ $<

# ---- SDL2_image (C) ----
$(BLD)/sdl2img/%.o: $(SDL2IMG_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(SDL2IMG_DEFS) -c -o $@ $<

# ---- SDL2_ttf (C) ----
$(BLD)/sdl2ttf/%.o: $(SDL2TTF_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

# ---- SDL2_mixer (C) ----
$(BLD)/sdl2mix/%.o: $(SDL2MIX_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(SDL2MIX_DEFS) -c -o $@ $<

# ---- Lua (C) ----
$(BLD)/lua/%.o: $(LUA_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(LUA_DEFS) -c -o $@ $<

# ---- zlib (C) ----
$(BLD)/zlib/%.o: $(ZLIB_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

# ---- libogg (C) ----
$(BLD)/ogg/%.o: $(OGG_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

# ---- libvorbis (C) ----
$(BLD)/vorbis/%.o: $(VORBIS_DIR)/lib/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(VORBIS_CFLAGS) -c -o $@ $<

# ---- libpng (C) ----
$(PNG_DIR)/pnglibconf.h: $(PNG_DIR)/scripts/pnglibconf.h.prebuilt
	cp $< $@

$(BLD)/png/%.o: $(PNG_DIR)/%.c $(PNG_DIR)/pnglibconf.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

# ---- GLEW (C) ----
$(BLD)/glew/%.o: $(GLEW_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(GLEW_DEFS) -c -o $@ $<

# ---- FreeType (C) ----
$(BLD)/ft/%.o: $(FT_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(FT_DEFS) -c -o $@ $<

# ---- jpeg (C) ----
$(BLD)/jpeg/%.o: $(JPEG_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

# ---- webp (C) ----
$(BLD)/webp/%.o: $(WEBP_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -DWEBP_DISABLE_SSE2 -c -o $@ $<

# ---- PortAudio (C) ----
$(BLD)/pa/%.o: $(PA_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(PA_DEFS) -c -o $@ $<

# ---- mpg123 (C) ----
$(BLD)/mpg123/%.o: $(MPG_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(MPG_DEFS) -c -o $@ $<

# ---- Opus (C) ----
$(BLD)/opus/%.o: $(OPUS_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(OPUS_DEFS) -c -o $@ $<

# ---- Opusfile (C) ----
$(BLD)/opusf/%.o: $(OPUSF_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

# ---- modplug (C++) ----
$(BLD)/modplug/%.o: $(MOD_DIR)/src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(MOD_DEFS) -c -o $@ $<

# ---- FLAC (C) ----
$(BLD)/flac/%.o: $(FLAC_DIR)/src/libFLAC/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(FLAC_DEFS) -c -o $@ $<

clean:
	rm -rf $(BLD)

install: $(TARGET)
	@if [ ! -d "install" ]; then \
		echo "=== Downloading install.zip... ==="; \
		wget -q "https://github.com/leonkasovan/ikemen-new-ultra/releases/download/v1.0.0.0/install.zip" -O install.zip; \
		echo "=== Extracting... ==="; \
		unzip -q install.zip; \
		rm -f install.zip; \
	fi
	cp $(TARGET) "install/"
	cp -rf ssz_script/lib/ install/
	cp -rf ssz_script/ssz/ install/
	cp -rf ssz_script/save/ install/
	cp -rf lua_script/lib/ install/
	cp -rf lua_script/script/ install/
	cp -rf lua_script/save/ install/
	@echo "=== Installed to install/ ==="
  