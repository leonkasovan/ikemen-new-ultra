// sdlplugin_service.cpp — Native C++ implementation for ssz_script/lib/alpha/sdlplugin.ssz
//
// All public API functions delegate to existing main/sdlplugin/sdlplugin.cpp
// implementations via extern "C" declarations. This provides a clean native
// C++ API layer over the SDL plugin system.

#include "sdlplugin_service.hpp"
#include "ssz_native/plugin_native_api.hpp"

// SDL headers needed for type casts in implementation
#include <SDL.h>
#include <SDL_ttf.h>

// SSZ VM types needed for Reference-based render functions
#include "sszdef.h"
#include "arrayandref.hpp"

namespace ikemen::ssz_native {

namespace {

/// Helper: create a temporary Reference from a std::vector<uint8_t>.
/// The caller must call ref.releaseanddelete() when done.
void vectorToRef(const std::vector<uint8_t>& src, Reference& ref) {
    ref.init();
    if (src.empty()) return;
    ref.refnew((intptr_t)src.size(), (intptr_t)sizeof(uint8_t));
    if (ref.len() > 0) {
        memcpy(ref.atpos(), src.data(), src.size());
    }
}

/// Helper: create a temporary Reference from a std::vector<int8_t>.
void vectorToRefS8(const std::vector<int8_t>& src, Reference& ref) {
    ref.init();
    if (src.empty()) return;
    ref.refnew((intptr_t)src.size(), (intptr_t)sizeof(int8_t));
    if (ref.len() > 0) {
        memcpy(ref.atpos(), src.data(), src.size());
    }
}

/// Helper: copy a Reference's data back into a std::vector<int8_t>.
void refToVectorS8(const Reference& ref, std::vector<int8_t>& dst) {
    if (ref.null() || ref.len() <= 0) return;
    intptr_t cnt = ref.len();
    if (static_cast<intptr_t>(dst.size()) < cnt)
        dst.resize(cnt);
    memcpy(dst.data(), ref.atpos(), cnt);
}

} // anonymous namespace

// =========================================================================
// Surface methods
// =========================================================================

void Surface::free() {
    if (surface) {
        FreeSurface(surface);
        surface = nullptr;
    }
}

void Surface::allocSurface(int w, int h) {
    free();
    surface = reinterpret_cast<SDL_Surface*>(AllocSurface(h, w));
}

void Surface::imgLoad(const std::string& fn) {
    free();
    // Convert UTF-8 to wide string for Windows API
    std::wstring wfn(fn.begin(), fn.end());
    surface = reinterpret_cast<SDL_Surface*>(IMGLoad(wfn));
}

void Surface::blitToWin(const SdlRect& dr) {
    if (!surface) return;
    SDL_Rect rect = { dr.x, dr.y, dr.w, dr.h };
    BlitSurface(&rect, surface);
}

void Surface::createPaletteSurface(const std::vector<uint8_t>& img, 
                                   const std::vector<uint32_t>& pal, 
                                   int w, int h) {
    free();
    if (img.empty() || pal.empty()) return;
    
    // Convert palette to SDL_Color array
    std::vector<SDL_Color> colors(pal.size());
    for (size_t i = 0; i < pal.size(); i++) {
        uint32_t c = pal[i];
        colors[i].r = (c >> 16) & 0xFF;
        colors[i].g = (c >> 8) & 0xFF;
        colors[i].b = c & 0xFF;
        colors[i].a = 255;
    }
    
    surface = reinterpret_cast<SDL_Surface*>(
        CreatePaletteSurface(h, w, colors.data(), const_cast<uint8_t*>(img.data()))
    );
}

void Surface::setColorKey(uint32_t key) {
    if (surface) {
        SetColorKey(key, surface);
    }
}

// =========================================================================
// Font methods
// =========================================================================

void Font::close() {
    if (font) {
        CloseFont(font);
        font = nullptr;
    }
}

void Font::open(const std::string& fn, int size) {
    close();
    std::wstring wfn(fn.begin(), fn.end());
    font = reinterpret_cast<TTF_Font*>(OpenFont(size, wfn));
}

void Font::render(uint32_t color, int x, int y, const std::string& str) {
    if (!font) return;
    std::wstring wstr(str.begin(), str.end());
    SDL_Color c;
    c.r = (color >> 16) & 0xFF;
    c.g = (color >> 8) & 0xFF;
    c.b = color & 0xFF;
    c.a = 255;
    RenderFont(wstr, y, x, c, font);
}

// =========================================================================
// GlTexture methods
// =========================================================================

void GlTexture::clear() {
    if (id != 0) {
        DeleteGlTexture(id);
        id = 0;
    }
}

bool GlTexture::load8bitTexture(const std::vector<uint8_t>& pxl, int w, int h) {
    if (pxl.empty() || static_cast<intptr_t>(pxl.size()) != w * h) {
        return false;
    }
    clear();
    id = Load8bitTexture(h, w, const_cast<uint8_t*>(pxl.data()));
    return id != 0;
}

bool GlTexture::loadPngTexture(int w, int h, FILE* fp) {
    clear();
    id = LoadPngTexture(fp, &h, &w);
    return id != 0;
}

// =========================================================================
// Module-level API - Rendering
// =========================================================================

void flip() {
    Flip();
}

void fill(const SdlRect& r, uint32_t c) {
    SDL_Rect rect = { r.x, r.y, r.w, r.h };
    Fill(c, &rect);
}

void softFill(const SdlRect& r, uint32_t c) {
    SDL_Rect rect = { r.x, r.y, r.w, r.h };
    SoftFill(c, &rect);
}

bool renderMugenZoom(const SdlRect& dr, float rcx, float rcy,
                     const std::vector<uint8_t>& pxl,
                     const std::vector<uint32_t>& pal,
                     int16_t ckey, const SdlRect& sr,
                     float cx, float ty, const SdlRect& tile,
                     float xtopscl, float xbotscl, float yscl,
                     float rasterxadd, uint32_t roto, int alpha,
                     int rle, std::vector<int8_t>& pluginbuf) {
    // Create temporary Reference from pixel data (img)
    Reference imgRef;
    vectorToRef(pxl, imgRef);
    
    // Create temporary Reference for scratch buffer (pluginbuf)
    Reference pluginbufRef;
    vectorToRefS8(pluginbuf, pluginbufRef);
    
    // Convert SdlRects to SDL_Rects
    SDL_Rect sdl_dr = { dr.x, dr.y, dr.w, dr.h };
    SDL_Rect sdl_sr = { sr.x, sr.y, sr.w, sr.h };
    SDL_Rect sdl_tile = { tile.x, tile.y, tile.w, tile.h };
    
    // Palette: pad to 256 entries if needed (native expects 256-color palette)
    // If empty, pass nullptr (native handles null palette)
    uint32_t* palPtr = nullptr;
    std::vector<uint32_t> palBuf;
    if (!pal.empty()) {
        palBuf.resize(256, 0);
        size_t copyCount = (pal.size() < 256) ? pal.size() : 256;
        memcpy(palBuf.data(), pal.data(), copyCount * sizeof(uint32_t));
        palPtr = palBuf.data();
    }
    
    // Call native function
    bool result = RenderMugenZoom(
        &pluginbufRef, rle, rcy, rcx,
        &sdl_dr, alpha, roto, rasterxadd, yscl, xbotscl, xtopscl,
        &sdl_tile, ty, cx, &sdl_sr,
        ckey, palPtr, imgRef
    );
    
    // Copy scratch buffer back
    if (pluginbufRef.len() > 0) {
        refToVectorS8(pluginbufRef, pluginbuf);
    }
    
    // Clean up temporary References
    imgRef.releaseanddelete();
    pluginbufRef.releaseanddelete();
    
    return result;
}

bool renderMugenShadow(const SdlRect& dr, float rcx, float rcy,
                       const std::vector<uint8_t>& pxl, uint32_t color,
                       const SdlRect& sr, float cx, float ty,
                       float xscl, float yscl, float vscl, uint32_t roto,
                       int alpha, int rle, std::vector<int8_t>& pluginbuf) {
    // Create temporary Reference from pixel data (img)
    Reference imgRef;
    vectorToRef(pxl, imgRef);
    
    // Create temporary Reference for scratch buffer (pluginbuf)
    Reference pluginbufRef;
    vectorToRefS8(pluginbuf, pluginbufRef);
    
    // Convert SdlRects to SDL_Rects
    SDL_Rect sdl_dr = { dr.x, dr.y, dr.w, dr.h };
    SDL_Rect sdl_sr = { sr.x, sr.y, sr.w, sr.h };
    
    // Call native function (no palette — uses uint32_t color directly)
    bool result = RenderMugenShadow(
        &pluginbufRef, rle, rcy, rcx,
        &sdl_dr, alpha, roto, vscl, yscl, xscl,
        ty, cx, &sdl_sr, color, imgRef
    );
    
    // Copy scratch buffer back
    if (pluginbufRef.len() > 0) {
        refToVectorS8(pluginbufRef, pluginbuf);
    }
    
    // Clean up temporary References
    imgRef.releaseanddelete();
    pluginbufRef.releaseanddelete();
    
    return result;
}

bool renderFontBatch(const std::vector<uint8_t>& atlas, float baseX, float baseY,
                     const std::vector<uint32_t>& pal, int atlasStride,
                     int glyphH, int alpha, const SdlRect& window,
                     float xscl, float yscl, float spacing,
                     const std::vector<int>& glyphData, int count) {
    if (atlas.empty() || pal.empty() || glyphData.empty()) {
        return false;
    }
    
    SDL_Rect winRect = { window.x, window.y, window.w, window.h };
    
    return RenderFontBatch(
        count,
        const_cast<int*>(glyphData.data()),
        spacing, yscl, xscl,
        &winRect,
        alpha, glyphH, atlasStride,
        const_cast<uint32_t*>(pal.data()),
        baseY, baseX,
        const_cast<uint8_t*>(atlas.data())
    );
}

// =========================================================================
// Module-level API - Input
// =========================================================================

char16_t getLastChar() {
    return GetLastChar();
}

std::vector<uint8_t> decodePNG8(int w, int h, FILE* file) {
    std::vector<uint8_t> result;
    if (!file) return result;
    
    DecodePNG8(file, &h, &w, result);
    return result;
}

bool keyState(SDLKey key) {
    return KeyState(static_cast<int32_t>(key));
}

bool joystickButtonState(int32_t joy, int32_t btn) {
    return JoystickButtonState(btn, joy);
}

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
    int sec) {
    return PollInputBitmask(
        jn, u, d, l, r, a, b, c, x, y, z, q, w, e, s,
        jn2, u2, d2, l2, r2, a2, b2, c2, x2, y2, z2, q2, w2, e2, s2,
        sec
    );
}

// =========================================================================
// Module-level API - Audio
// =========================================================================

bool setSndBuf(const std::vector<int>& buf) {
    if (static_cast<intptr_t>(buf.size()) != SNDBUFLEN) {
        return false;
    }
    return SetSndBuf(const_cast<int*>(buf.data()));
}

int playVideo(int audiotrack, int volume, const std::string& captures, const std::string& fn) {
    std::wstring wcaptures(captures.begin(), captures.end());
    std::wstring wfn(fn.begin(), fn.end());
    return PlayVideo(wfn, wcaptures, volume, audiotrack);
}

bool playBGM(const std::string& pldir, const std::string& fn) {
    std::wstring wpldir(pldir.begin(), pldir.end());
    std::wstring wfn(fn.begin(), fn.end());
    return PlayBGM(wfn, wpldir);
}

void pauseBGM(bool pause) {
    PauseBGM(pause);
}

bool sendOpenBGM(int rate, int channels) {
    return SendOpenBGM(channels, rate);
}

void sendCloseBGM() {
    SendCloseBGM();
}

intptr_t sendWriteBGM(const std::vector<int16_t>& buffer) {
    // Deprecated: OGG streaming is now handled by PlayBGM via SDL_mixer.
    // The native SendWriteBGM() ignores its parameter — the buffer is discarded.
    // This wrapper exists for ABI compatibility only.
    (void)buffer;
    return SendWriteBGM();
}

void fadeInBGM(int time) {
    FadeInBGM(time);
}

void fadeOutBGM(int time) {
    FadeOutBGM(time);
}

void setVolume(float gvol, float wvol, float bvol) {
    SetVolume(bvol, wvol, gvol);
}

void setOpacity(float wo) {
    SetOpacity(wo);
}

// =========================================================================
// Module-level API - Window/Display
// =========================================================================

bool init(const std::string& t, int w, int h, int renderer, bool mugen) {
    std::wstring wt(t.begin(), t.end());
    
    std::wstring rendererStr = L"SDL2";
    if (renderer == 1) rendererStr = L"OpenGL";
    else if (renderer == 2) rendererStr = L"OpenGL ES";
    else if (renderer == 3) rendererStr = L"Vulkan";
    
    return RendererInit(rendererStr, h, w, wt);
}

int getWidth() {
    return GetWidth();
}

int getHeight() {
    return GetHeight();
}

void windowSize(int w, int h) {
    WindowSize(h, w);
}

void fullScreenMode(bool fullReal) {
    FullScreenExclusive(fullReal);
}

bool fullScreen(bool full) {
    return FullScreen(full);
}

void setWindowType(int state) {
    WindowType(state);
}

void keepAspectRatio(bool aspect) {
    AspectRatio(aspect);
}

void takeScreenShot(const std::string& dir) {
    std::wstring wdir(dir.begin(), dir.end());
    TakeScreenShot(wdir);
}

void showCursor(bool show) {
    CursorShow(show);
}

// =========================================================================
// Module-level API - OpenGL
// =========================================================================

bool bindGlContext() {
    return BindGlContext();
}

bool unbindGlContext() {
    return UnbindGlContext();
}

void enablePerfMonitor(bool enable) {
    EnablePerfMonitor(enable);
}

void getRendererInfo() {
    GetRendererInfo();
}

} // namespace ikemen::ssz_native
