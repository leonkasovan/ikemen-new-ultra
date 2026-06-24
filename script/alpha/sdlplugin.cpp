#include "sdlplugin.hpp"

#include "sszdef.h"
#include "typeid.h"
#include "arrayandref.hpp"
#include "pluginutil.hpp"

#include "../file.hpp"

extern "C" {
	void     SSZ_STDCALL End(PluginUtil*);
	bool     SSZ_STDCALL PollEvent(PluginUtil*, ikemen::Event*);
	bool     SSZ_STDCALL UpdateGLViewport(PluginUtil*, ikemen::Event*);
	void     SSZ_STDCALL Flip(PluginUtil*);
	void     SSZ_STDCALL Fill(PluginUtil*, uint32_t, ikemen::Rect*);
	void     SSZ_STDCALL SoftFill(PluginUtil*, uint32_t, ikemen::Rect*);
	bool     SSZ_STDCALL RenderMugenZoom(PluginUtil*, Reference*, int32_t, float, float, ikemen::Rect*, int32_t, uint32_t, float, float, float, float, ikemen::Rect*, float, float, ikemen::Rect*, uint16_t, uint32_t*, Reference);
	bool     SSZ_STDCALL RenderMugenShadow(PluginUtil*, Reference*, int32_t, float, float, ikemen::Rect*, int32_t, uint32_t, float, float, float, ikemen::Rect*, float, float, ikemen::Rect*, uint32_t, Reference);
	bool     SSZ_STDCALL RenderFontBatch(PluginUtil*, uint8_t*, float, float, uint32_t*, int32_t, int32_t, int32_t, ikemen::Rect*, float, float, float, int32_t*, int32_t);
	char16_t SSZ_STDCALL GetLastChar(PluginUtil*);
	void     SSZ_STDCALL DecodePNG8(PluginUtil*, FILE*, int32_t*, int32_t*, Reference*);
	bool     SSZ_STDCALL KeyState(PluginUtil*, int32_t);
	bool     SSZ_STDCALL JoystickButtonState(PluginUtil*, int32_t, int32_t);
	int32_t  SSZ_STDCALL PollInputBitmask(PluginUtil*, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t);
	void     SSZ_STDCALL Delay(PluginUtil*, uint32_t);
	uint32_t SSZ_STDCALL GetTicks(PluginUtil*);
	bool     SSZ_STDCALL SetSndBuf(PluginUtil*, int32_t*);
	int      SSZ_STDCALL PlayVideo(PluginUtil*, Reference, Reference, int32_t, int32_t);
	bool     SSZ_STDCALL PlayBGM(PluginUtil*, Reference, Reference);
	void     SSZ_STDCALL PauseBGM(PluginUtil*, bool);
	bool     SSZ_STDCALL SendOpenBGM(PluginUtil*, int32_t, int32_t);
	void     SSZ_STDCALL SendCloseBGM(PluginUtil*);
	intptr_t SSZ_STDCALL SendWriteBGM(PluginUtil*, Reference);
	void     SSZ_STDCALL FadeInBGM(PluginUtil*, int);
	void     SSZ_STDCALL FadeOutBGM(PluginUtil*, int);
	void     SSZ_STDCALL SetVolume(PluginUtil*, float, float, float);
	void     SSZ_STDCALL SetOpacity(PluginUtil*, float);
	void     SSZ_STDCALL FreeSurface(PluginUtil*, intptr_t);
	intptr_t SSZ_STDCALL AllocSurface(PluginUtil*, int32_t, int32_t);
	intptr_t SSZ_STDCALL IMGLoad(PluginUtil*, Reference);
	void     SSZ_STDCALL BlitSurface(PluginUtil*, intptr_t, ikemen::Rect*);
	intptr_t SSZ_STDCALL CreatePaletteSurface(PluginUtil*, uint8_t*, uint32_t*, int32_t, int32_t);
	void     SSZ_STDCALL SetColorKey(PluginUtil*, intptr_t, int32_t);
	void     SSZ_STDCALL CloseFont(PluginUtil*, intptr_t);
	intptr_t SSZ_STDCALL OpenFont(PluginUtil*, int32_t, Reference);
	void     SSZ_STDCALL RenderFont(PluginUtil*, intptr_t, uint32_t, int32_t, int32_t, Reference);
	void     SSZ_STDCALL DrawTTF(PluginUtil*, Reference, int32_t, Reference, int32_t, int32_t, float, float, int32_t, int32_t, int32_t, int32_t);
	bool     SSZ_STDCALL RendererInit(PluginUtil*, Reference, int32_t, int32_t, Reference);
	bool     SSZ_STDCALL GlInit(PluginUtil*, int32_t, int32_t, Reference);
	bool     SSZ_STDCALL Init(PluginUtil*, bool, int32_t, int32_t, Reference);
	int      SSZ_STDCALL GetWidth(PluginUtil*);
	int      SSZ_STDCALL GetHeight(PluginUtil*);
	void     SSZ_STDCALL WindowSize(PluginUtil*, int, int);
	void     SSZ_STDCALL FullScreenExclusive(PluginUtil*, bool);
	bool     SSZ_STDCALL FullScreen(PluginUtil*, bool);
	void     SSZ_STDCALL WindowType(PluginUtil*, int);
	void     SSZ_STDCALL AspectRatio(PluginUtil*, bool);
	void     SSZ_STDCALL TakeScreenShot(PluginUtil*, Reference);
	void     SSZ_STDCALL CursorShow(PluginUtil*, bool);
	void     SSZ_STDCALL DeleteGlTexture(PluginUtil*, uint32_t);
	uint32_t SSZ_STDCALL Load8bitTexture(PluginUtil*, uint8_t*, int32_t, int32_t);
	uint32_t SSZ_STDCALL LoadPngTexture(PluginUtil*, int32_t*, int32_t*, void*);
	void     SSZ_STDCALL MugenFillGl(PluginUtil*, int32_t, uint32_t, ikemen::Rect);
	bool     SSZ_STDCALL InitMugenGl(PluginUtil*);
	bool     SSZ_STDCALL RenderMugenGl(PluginUtil*, uint32_t, uint32_t*, int32_t, ikemen::Rect*, float, float, ikemen::Rect*, float, float, float, float, float, float, int32_t, ikemen::Rect*, float, float);
	bool     SSZ_STDCALL RenderMugenGlFc(PluginUtil*, uint32_t, ikemen::Rect*, float, float, ikemen::Rect*, float, float, float, float, float, float, int32_t, ikemen::Rect*, float, float, bool, float, float, float, float, float, float, float);
	bool     SSZ_STDCALL RenderMugenGlFcS(PluginUtil*, uint32_t, ikemen::Rect*, float, float, ikemen::Rect*, float, float, float, float, float, float, int32_t, ikemen::Rect*, float, float, uint32_t);
	void     SSZ_STDCALL GlSwapBuffers(PluginUtil*);
	bool     SSZ_STDCALL BindGlContext(PluginUtil*);
	bool     SSZ_STDCALL UnbindGlContext(PluginUtil*);
	void     SSZ_STDCALL EnablePerfMonitor(PluginUtil*, bool);
	void     SSZ_STDCALL GetRendererInfo(PluginUtil*, Reference*);
}

namespace ikemen {

static PluginUtil makePU() { static PluginSSZFuncs f={nullptr,nullptr,nullptr}; static PluginUtil pu(&f,nullptr); return pu; }

// ── Surface ──────────────────────────────────────────────────────────────

Surface::~Surface() { free(); }
bool Surface::null() const { return m_sur == 0; }

void Surface::free() {
	if (m_sur) { auto pu=makePU(); FreeSurface(&pu,m_sur); m_sur=0; }
}
void Surface::allocSurface(int w, int h) {
	free(); auto pu=makePU(); m_sur=AllocSurface(&pu,w,h);
}
void Surface::imgLoad(const std::wstring& fn) {
	Reference r; r.init(); PluginUtil::wstrToRef(r,fn);
	free(); auto pu=makePU(); m_sur=IMGLoad(&pu,r); r.releaseanddelete();
}
void Surface::createPaletteSurface(const std::vector<uint8_t>& img, const std::vector<uint32_t>& pal, int w, int h) {
	auto pu=makePU();
	free();
	m_sur=CreatePaletteSurface(&pu, const_cast<uint8_t*>(img.data()), const_cast<uint32_t*>(pal.data()), w, h);
}
void Surface::setColorKey(int key) { auto pu=makePU(); SetColorKey(&pu,m_sur,key); }

// ── Font ─────────────────────────────────────────────────────────────────

Font::~Font() { close(); }
void Font::close() {
	if (m_font) { auto pu=makePU(); CloseFont(&pu,m_font); m_font=0; }
}
void Font::open(const std::wstring& fn, int size) {
	close();
	Reference r; r.init(); PluginUtil::wstrToRef(r,fn);
	auto pu=makePU(); m_font=OpenFont(&pu,size,r); r.releaseanddelete();
}
void Font::render(uint32_t color, int x, int y, const std::wstring& str) {
	Reference r; r.init(); PluginUtil::wstrToRef(r,str);
	auto pu=makePU(); RenderFont(&pu,m_font,color,x,y,r); r.releaseanddelete();
}

// ── GlTexture ────────────────────────────────────────────────────────────

GlTexture::~GlTexture() { clear(); }
void GlTexture::clear() {
	if (m_id) { auto pu=makePU(); DeleteGlTexture(&pu,m_id); m_id=0; }
}
bool GlTexture::load8bitTexture(const std::vector<uint8_t>& pxl, int w, int h) {
	if (pxl.empty() || static_cast<int>(pxl.size()) != w*h) return false;
	clear();
	auto pu=makePU();
	m_id=Load8bitTexture(&pu, const_cast<uint8_t*>(pxl.data()), w, h);
	return m_id != 0;
}
bool GlTexture::loadPngTexture(int& w, int& h, File& file) {
	clear();
	auto pu=makePU();
	m_id=LoadPngTexture(&pu, &w, &h, file.handle());
	return m_id != 0;
}

// ── UseGlContext ─────────────────────────────────────────────────────────

UseGlContext::UseGlContext()  { bindGlContext(); }
UseGlContext::~UseGlContext() { unbindGlContext(); }

// ── Standalone functions ─────────────────────────────────────────────────

void flip() { auto pu=makePU(); Flip(&pu); }
void fill(Rect& r, uint32_t c) { auto pu=makePU(); Fill(&pu, c, &r); }
void softFill(Rect& r, uint32_t c) { auto pu=makePU(); SoftFill(&pu, c, &r); }
wchar_t getLastChar() { auto pu=makePU(); return static_cast<wchar_t>(GetLastChar(&pu)); }

bool keyState(SDLKey key) { auto pu=makePU(); return KeyState(&pu, static_cast<int32_t>(key)); }
bool joystickButtonState(int btn, int joy) { auto pu=makePU(); return JoystickButtonState(&pu,btn,joy); }
void delay(uint32_t ms) { auto pu=makePU(); Delay(&pu,ms); }
uint32_t getTicks() { auto pu=makePU(); return GetTicks(&pu); }

bool pollEvent(Event& ev) { auto pu=makePU(); return ::PollEvent(&pu,&ev); }
void updateGLViewport(Event& ev) { auto pu=makePU(); ::UpdateGLViewport(&pu,&ev); }
void end() { auto pu=makePU(); ::End(&pu); }

bool renderMugenZoom(Rect& dr, float rcx, float rcy, const std::vector<uint8_t>& pxl,
                     const std::vector<uint32_t>& pal, int16_t ckey, Rect& sr,
                     float cx, float ty, Rect& tile, float xtopscl, float xbotscl,
                     float yscl, float rasterxadd, uint32_t roto, int alpha, int rle,
                     std::vector<int8_t>& pluginbuf)
{
	printf("[RENDER] renderMugenZoom wrapper: ENTER pxl.len=%zu pal.len=%zu\n", pxl.size(), pal.size()); fflush(stdout);
	auto pu = makePU();
	Reference pxlRef; pxlRef.init();
	pxlRef.refnew((intptr_t)pxl.size(), 1);
	printf("[RENDER] renderMugenZoom wrapper: pxlRef.pointer=%p null=%d\n", (void*)pxlRef.pointer, pxlRef.null() ? 1 : 0); fflush(stdout);
	if (!pxlRef.null()) memcpy(pxlRef.atpos(), pxl.data(), pxl.size());
	printf("[RENDER] renderMugenZoom wrapper: memcpy done\n"); fflush(stdout);

	// Matches native: (PluginUtil*, Reference* pluginbuf, int32_t rle, float rcy, float rcx,
	//   Rect* dr, int32_t alpha, uint32_t roto, float rasterxadd, float yscl, float xbotscl,
	//   float xtopscl, Rect* tile, float ty, float cx, Rect* sr, uint16_t ckey, uint32_t* pal, Reference img)
	Reference bufRef; bufRef.init();
	bufRef.refnew((intptr_t)pluginbuf.size(), 1);
	if (!bufRef.null() && !pluginbuf.empty())
		memcpy(bufRef.atpos(), pluginbuf.data(), pluginbuf.size());

	// Fix double-free: RenderMugenZoom takes img by value; its copy destructor
	// frees the data. Transfer ownership: countup before call so the copy's
	// destructor decrements to 1 (not 0), and pxlRef.releaseanddelete() at
	// the end frees the data. This prevents the wrapper's data from being
	// freed while mRender still needs it.
	pxlRef.countup();
	printf("[RENDER] renderMugenZoom wrapper: countup done, refcnt=%d\n", pxlRef.refcnt()); fflush(stdout);

	bool ok = ::RenderMugenZoom(&pu, &bufRef, rle, rcy, rcx,
		&dr, alpha, roto, rasterxadd, yscl, xbotscl, xtopscl,
		&tile, ty, cx, &sr, (uint16_t)ckey, (uint32_t*)pal.data(), pxlRef);
	printf("[RENDER] renderMugenZoom wrapper: RenderMugenZoom returned %d\n", ok); fflush(stdout);

	// After mRender returns, the data is no longer needed. Release the
	// extra reference. This calls releaseanddelete which decrements the
	// refcount. Since we countup'd earlier, the refcount is now 1, and
	// releaseanddelete will free the data.
	pxlRef.releaseanddelete();
	// Fix double-free: also release bufRef
	bufRef.releaseanddelete();
	return ok;
}

int32_t pollInputBitmask(int jn, int u,int d,int l,int r, int a,int b,int c, int x,int y,int z, int q,int w,int e,int s,
                         int jn2, int u2,int d2,int l2,int r2, int a2,int b2,int c2, int x2,int y2,int z2, int q2,int w2,int e2,int s2, int sec) {
	auto pu=makePU();
	return PollInputBitmask(&pu, jn, u,d,l,r, a,b,c, x,y,z, q,w,e,s, jn2, u2,d2,l2,r2, a2,b2,c2, x2,y2,z2, q2,w2,e2,s2, sec);
}

bool setSndBuf(std::vector<int32_t>& buf) {
	if (static_cast<intptr_t>(buf.size()) != SNDBUFLEN) return false;
	auto pu=makePU(); return SetSndBuf(&pu, buf.data());
}

int playVideo(int audiotrack, int volume, const std::wstring& captures, const std::wstring& fn) {
	Reference rC; rC.init(); Reference rF; rF.init();
	PluginUtil::wstrToRef(rC,captures); PluginUtil::wstrToRef(rF,fn);
	auto pu=makePU(); int r=PlayVideo(&pu, rF, rC, audiotrack, volume);
	rC.releaseanddelete(); rF.releaseanddelete(); return r;
}

bool playBGM(const std::wstring& pldir, const std::wstring& fn) {
	Reference rD; rD.init(); Reference rF; rF.init();
	PluginUtil::wstrToRef(rD,pldir); PluginUtil::wstrToRef(rF,fn);
	auto pu=makePU(); bool ok=PlayBGM(&pu,rD,rF);
	rD.releaseanddelete(); rF.releaseanddelete(); return ok;
}

void pauseBGM(bool pause) { auto pu=makePU(); PauseBGM(&pu,pause); }
bool sendOpenBGM(int rate, int channels) { auto pu=makePU(); return SendOpenBGM(&pu,rate,channels); }
void sendCloseBGM() { auto pu=makePU(); SendCloseBGM(&pu); }

intptr_t sendWriteBGM(const std::vector<int16_t>& buffer) {
	Reference r; r.init();
	r.refnew(static_cast<intptr_t>(buffer.size()), sizeof(int16_t));
	if (!r.null()) memcpy(r.atpos(), buffer.data(), buffer.size()*sizeof(int16_t));
	auto pu=makePU(); intptr_t n=SendWriteBGM(&pu,r); r.releaseanddelete(); return n;
}

void fadeInBGM(int time) { auto pu=makePU(); FadeInBGM(&pu,time); }
void fadeOutBGM(int time) { auto pu=makePU(); FadeOutBGM(&pu,time); }
void setVolume(float g, float w, float b) { auto pu=makePU(); SetVolume(&pu,g,w,b); }
void setOpacity(float wo) { auto pu=makePU(); SetOpacity(&pu,wo); }

std::vector<uint8_t> decodePNG8(int& w, int& h, File& file) {
	Reference img; img.init();
	auto pu=makePU();
	DecodePNG8(&pu, file.handle(), &h, &w, &img);
	std::vector<uint8_t> out;
	if (img.len()>0) { out.resize(static_cast<size_t>(img.len())); memcpy(out.data(), img.atpos(), static_cast<size_t>(img.len())); }
	img.releaseanddelete(); return out;
}

bool init(const std::wstring& title, int w, int h, int renderer, bool mugen) {
	Reference rT; rT.init(); PluginUtil::wstrToRef(rT,title);
	const wchar_t* rendererStr = L"SDL2";
	if (renderer==1) rendererStr = L"OpenGL";
	else if (renderer==2) rendererStr = L"OpenGL ES";
	else if (renderer==3) rendererStr = L"Vulkan";
	Reference rR; rR.init(); PluginUtil::wstrToRef(rR,rendererStr);
	auto pu=makePU(); bool ok=RendererInit(&pu,rR,h,w,rT);
	rT.releaseanddelete(); rR.releaseanddelete(); return ok;
}

int getWidth() { auto pu=makePU(); return GetWidth(&pu); }
int getHeight() { auto pu=makePU(); return GetHeight(&pu); }
void windowSize(int w, int h) { auto pu=makePU(); WindowSize(&pu,w,h); }
void fullScreenMode(bool real) { auto pu=makePU(); FullScreenExclusive(&pu,real); }
bool fullScreen(bool full) { auto pu=makePU(); return FullScreen(&pu,full); }
void setWindowType(int s) { auto pu=makePU(); WindowType(&pu,s); }
void keepAspectRatio(bool a) { auto pu=makePU(); AspectRatio(&pu,a); }

void takeScreenShot(const std::wstring& dir) {
	Reference r; r.init(); PluginUtil::wstrToRef(r,dir);
	auto pu=makePU(); TakeScreenShot(&pu,r); r.releaseanddelete();
}

void showCursor(bool show) { auto pu=makePU(); CursorShow(&pu,show); }
bool bindGlContext() { auto pu=makePU(); return BindGlContext(&pu); }
bool unbindGlContext() { auto pu=makePU(); return UnbindGlContext(&pu); }
void enablePerfMonitor(bool e) { auto pu=makePU(); EnablePerfMonitor(&pu,e); }
void getRendererInfo() { Reference r; r.init(); auto pu=makePU(); GetRendererInfo(&pu,&r); r.releaseanddelete(); }

void drawTTF(const std::wstring& path, int align, const std::wstring& text,
             int x, int y, float sx, float sy, int r, int g, int b, int a) {
	Reference rP; rP.init(); Reference rT; rT.init();
	PluginUtil::wstrToRef(rP,path); PluginUtil::wstrToRef(rT,text);
	auto pu=makePU(); DrawTTF(&pu,rP,align,rT,x,y,sx,sy,r,g,b,a);
	rP.releaseanddelete(); rT.releaseanddelete();
}

// ── OpenGL stubs (forward to native) ─────────────────────────────────────

void mugenFillGl(Rect& r, uint32_t color, int alpha) { auto pu=makePU(); MugenFillGl(&pu,alpha,color,r); }
bool initMugenGl() { auto pu=makePU(); return InitMugenGl(&pu); }
void glSwapBuffers() { auto pu=makePU(); GlSwapBuffers(&pu); }

} // namespace ikemen
