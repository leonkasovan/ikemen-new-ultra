// sdlevent_service.cpp — Native C++ implementation for ssz_script/lib/alpha/sdlevent.ssz
//
// All public API functions are implemented with real event polling, key state
// tracking, and frame timing logic mirroring the SSZ sdlevent.ssz behavior.

#include "sdlevent_service.hpp"
#include "ssz_native/plugin_native_api.hpp"

#include <SDL.h>       // for SDL_PollEvent, SDL_GetKeyboardState
#include <algorithm>
#include <cstring>

namespace ikemen::ssz_native {

// =========================================================================
// Module-level state
// =========================================================================
static SdleventState g_state;

SdleventState& sdlevent_get_state() {
    return g_state;
}

// =========================================================================
// SdleventState
// =========================================================================

void SdleventState::resetFrameKeys() {
    esc = false;
    paste = false;
    upKey = false;
    downKey = false;
    leftKey = false;
    rightKey = false;
    aKey = bKey = cKey = dKey = eKey = false;
    fKey = gKey = hKey = iKey = jKey = false;
    kKey = lKey = mKey = nKey = oKey = false;
    pKey = qKey = rKey = sKey = tKey = false;
    uKey = vKey = wKey = xKey = yKey = zKey = false;
    kzeroKey = koneKey = ktwoKey = kthreeKey = kfourKey = false;
    kfiveKey = ksixKey = ksevenKey = keightKey = knineKey = false;
    zeroKey = oneKey = twoKey = threeKey = fourKey = false;
    fiveKey = sixKey = sevenKey = eightKey = nineKey = false;
    returnKey = backspaceKey = spaceKey = lshiftKey = rshiftKey = tabKey = false;
    kdivideKey = kmultiplyKey = kminusKey = kplusKey = kenterKey = kperiodKey = false;
    minusKey = equalsKey = semicolonKey = commaKey = periodKey = slashKey = false;
    leftbracketKey = rightbracketKey = backslashKey = false;
    f1Key = f2Key = f3Key = f4Key = f5Key = f9Key = f10Key = f11Key = f12Key = false;
    printscreenKey = false;
    insertKey = homeKey = pageupKey = deleteKey = endKey = pagedownKey = false;
    getGamepadKeyA = getGamepadKeyB = getGamepadKeyC = false;
}

// =========================================================================
// Module-level API
// =========================================================================

bool sdlevent_event_update() {
    // Reset all event keys
    for (auto& ek : g_state.eventKeys) {
        ek.reset();
    }

    // Poll all pending events
    while (PollEvent(reinterpret_cast<int8_t*>(&g_state.sdle))) {
        int32_t type = g_state.sdle.etype;

        // Quit event
        if (type == static_cast<int32_t>(EventType::QUIT)) {
            g_state.end = true;
            return false;
        }

        // Key down events
        if (type == static_cast<int32_t>(EventType::KEYDOWN)) {
            // Check registered event keys
            for (auto& ek : g_state.eventKeys) {
                ek.checkDown(g_state.sdle.key.keysym.sym, g_state.sdle.key.keysym.mod);
            }

            uint16_t mod = g_state.sdle.key.keysym.mod;
            K sym = g_state.sdle.key.keysym.sym;

            // Alt+Enter: toggle fullscreen
            if ((mod & (KMOD_LALT | KMOD_RALT)) != 0) {
                if (sym == K::RETURN) {
                    if (FullScreen(!g_state.full))
                        CursorShow(!g_state.full || (g_state.full && !g_state.fullReal));
                }
                if (sym == K::F4) {
                    g_state.end = true;
                    return false;
                }
            }

            // Ctrl+V: paste
            if ((mod & (KMOD_LCTRL | KMOD_RCTRL)) != 0) {
                if (sym == K::v) {
                    g_state.paste = true;
                }
            }

            // Track individual key presses (mirrors SSZ switch-cascade)
            switch (sym) {
            case K::ESCAPE:    g_state.esc = true; break;
            case K::UP:        g_state.upKey = true; break;
            case K::DOWN:      g_state.downKey = true; break;
            case K::LEFT:      g_state.leftKey = true; break;
            case K::RIGHT:     g_state.rightKey = true; break;
            case K::a: g_state.aKey = true; break; case K::b: g_state.bKey = true; break;
            case K::c: g_state.cKey = true; break; case K::d: g_state.dKey = true; break;
            case K::e: g_state.eKey = true; break; case K::f: g_state.fKey = true; break;
            case K::g: g_state.gKey = true; break; case K::h: g_state.hKey = true; break;
            case K::i: g_state.iKey = true; break; case K::j: g_state.jKey = true; break;
            case K::k: g_state.kKey = true; break; case K::l: g_state.lKey = true; break;
            case K::m: g_state.mKey = true; break; case K::n: g_state.nKey = true; break;
            case K::o: g_state.oKey = true; break; case K::p: g_state.pKey = true; break;
            case K::q: g_state.qKey = true; break; case K::r: g_state.rKey = true; break;
            case K::s: g_state.sKey = true; break; case K::t: g_state.tKey = true; break;
            case K::u: g_state.uKey = true; break; case K::v: g_state.vKey = true; break;
            case K::w: g_state.wKey = true; break; case K::x: g_state.xKey = true; break;
            case K::y: g_state.yKey = true; break; case K::z: g_state.zKey = true; break;
            case K::KP_0: g_state.kzeroKey = true; break;
            case K::KP_1: g_state.koneKey = true; break;
            case K::KP_2: g_state.ktwoKey = true; break;
            case K::KP_3: g_state.kthreeKey = true; break;
            case K::KP_4: g_state.kfourKey = true; break;
            case K::KP_5: g_state.kfiveKey = true; break;
            case K::KP_6: g_state.ksixKey = true; break;
            case K::KP_7: g_state.ksevenKey = true; break;
            case K::KP_8: g_state.keightKey = true; break;
            case K::KP_9: g_state.knineKey = true; break;
            case K::_0: g_state.zeroKey = true; break;
            case K::_1: g_state.oneKey = true; break;
            case K::_2: g_state.twoKey = true; break;
            case K::_3: g_state.threeKey = true; break;
            case K::_4: g_state.fourKey = true; break;
            case K::_5: g_state.fiveKey = true; break;
            case K::_6: g_state.sixKey = true; break;
            case K::_7: g_state.sevenKey = true; break;
            case K::_8: g_state.eightKey = true; break;
            case K::_9: g_state.nineKey = true; break;
            case K::RETURN:      g_state.returnKey = true; break;
            case K::BACKSPACE:   g_state.backspaceKey = true; break;
            case K::SPACE:       g_state.spaceKey = true; break;
            case K::LSHIFT:      g_state.lshiftKey = true; break;
            case K::RSHIFT:      g_state.rshiftKey = true; break;
            case K::TAB:         g_state.tabKey = true; break;
            case K::MINUS:       g_state.minusKey = true; break;
            case K::EQUALS:      g_state.equalsKey = true; break;
            case K::LEFTBRACKET: g_state.leftbracketKey = true; break;
            case K::RIGHTBRACKET: g_state.rightbracketKey = true; break;
            case K::BACKSLASH:   g_state.backslashKey = true; break;
            case K::SEMICOLON:   g_state.semicolonKey = true; break;
            case K::COMMA:       g_state.commaKey = true; break;
            case K::PERIOD:      g_state.periodKey = true; break;
            case K::SLASH:       g_state.slashKey = true; break;
            case K::F1:  g_state.f1Key = true; break;
            case K::F2:  g_state.f2Key = true; break;
            case K::F3:  g_state.f3Key = true; break;
            case K::F4:  g_state.f4Key = true; break;
            case K::F5:  g_state.f5Key = true; break;
            case K::F9:  g_state.f9Key = true; break;
            case K::F10: g_state.f10Key = true; break;
            case K::F11: g_state.f11Key = true; break;
            case K::F12: g_state.f12Key = true; break;
            case K::PRINTSCREEN: g_state.printscreenKey = true; break;
            case K::INSERT:   g_state.insertKey = true; break;
            case K::HOME:     g_state.homeKey = true; break;
            case K::PAGEUP:   g_state.pageupKey = true; break;
            case K::DELETE:   g_state.deleteKey = true; break;
            case K::END:      g_state.endKey = true; break;
            case K::PAGEDOWN: g_state.pagedownKey = true; break;
            case K::KP_DIVIDE:   g_state.kdivideKey = true; break;
            case K::KP_MULTIPLY: g_state.kmultiplyKey = true; break;
            case K::KP_MINUS:    g_state.kminusKey = true; break;
            case K::KP_PLUS:     g_state.kplusKey = true; break;
            case K::KP_ENTER:    g_state.kenterKey = true; break;
            case K::KP_PERIOD:   g_state.kperiodKey = true; break;
            default: break;
            }
        }
    }

    // Joystick/gamepad buttons
    if (JoystickButtonState(0, 0)) g_state.getGamepadKeyA = true;
    if (JoystickButtonState(1, 1)) g_state.getGamepadKeyB = true;
    if (JoystickButtonState(2, 2)) g_state.getGamepadKeyC = true;

    return !g_state.end;
}

bool sdlevent_event(int fps) {
    uint32_t uWait = 1000 / static_cast<uint32_t>(fps);

    // Inner helper: advance nexttime by one frame
    auto nexttimeNext = [&]() {
        g_state.nexttime += uWait;
        g_state.nexttimeFractionalPart += static_cast<float>(1000 % fps) / static_cast<float>(fps);
        if (g_state.nexttimeFractionalPart >= 1.0f) {
            g_state.nexttime++;
            g_state.nexttimeFractionalPart -= 1.0f;
        }
    };

    // SSZ branch: compute timing and decide skip/delay
    uint32_t now = GetTicks();
    uint32_t dif = g_state.nexttime - now;   // unsigned wraparound matches SSZ
    nexttimeNext();

    bool skipCommon = false;

    if (dif < uWait + 2) {
        // Ahead of schedule: delay to match target
        if (dif > 0) Delay(dif);
    } else if (now - g_state.lastdraw > 250) {
        // Stalled for more than 250ms — don't skip
    } else if (dif + 17 < 17) {
        // Slightly behind (dif between -1 and -16) — don't skip
    } else {
        // Significantly behind: skip this frame
        if (static_cast<int32_t>(-dif) > 150) {
            g_state.nexttime = now;
            nexttimeNext();
        }
        g_state.fskip = true;
        skipCommon = true;
    }

    // Common block (runs unless break/skipCommon)
    if (!skipCommon) {
        g_state.lastdraw = now;
        g_state.fskip = false;
    }

    // Reset all per-frame key booleans (mirrors SSZ reset block)
    g_state.resetFrameKeys();

    // Poll events and return
    return sdlevent_event_update();
}

} // namespace ikemen::ssz_native
