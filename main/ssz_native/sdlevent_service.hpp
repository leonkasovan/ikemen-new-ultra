// sdlevent_service.hpp — Native C++ implementation for ssz_script/lib/alpha/sdlevent.ssz
//
// sdlevent.ssz (599 lines) implements SDL event processing and key state
// tracking — polling events, tracking per-key state, fullscreen toggles,
// clipboard paste, frame timing, and related engine event handling.
//
// Phase 5: Full implementation. All 63 key state booleans, Key struct with
// checkDown(), eventUpdate() polling loop, and event() frame timing.

#pragma once

#include <cstdint>
#include <vector>

#include "sdlplugin_service.hpp"

namespace ikemen::ssz_native {

// =========================================================================
// Key — tracked key with modifier awareness
// =========================================================================
struct SdleKey {
    K key{K::UNKNOWN};
    bool shift{false}, ctrl{false}, alt{false};
    bool down{false};

    void reset() { down = false; }

    void checkDown(K k, uint16_t mod) {
        down |=
            key == k
            && shift == ((mod & (KMOD_LSHIFT | KMOD_RSHIFT)) != 0)
            && ctrl  == ((mod & (KMOD_LCTRL | KMOD_RCTRL)) != 0)
            && alt   == ((mod & (KMOD_LALT | KMOD_RALT)) != 0);
    }
};

// =========================================================================
// SdleventState — all module-level variables from sdlevent.ssz
// =========================================================================
struct SdleventState {
    // Timing
    uint32_t nexttime{0};
    uint32_t lastdraw{0};
    float nexttimeFractionalPart{0.0f};

    // SDL event storage
    Event sdle;

    // Engine flags
    bool end{false};
    bool fskip{false};
    bool full{false};
    bool fullReal{false};
    bool aspect{false};
    bool esc{false};
    bool paste{false};

    // Per-key state (pressed this frame — reset each frame)
    bool printscreenKey{false};
    bool nonushashKey{false};
    bool apostropheKey{false};
    bool graveKey{false};
    bool upKey{false};
    bool downKey{false};
    bool leftKey{false};
    bool rightKey{false};
    bool leftbracketKey{false};
    bool rightbracketKey{false};
    bool backslashKey{false};

    bool aKey{false}, bKey{false}, cKey{false}, dKey{false}, eKey{false};
    bool fKey{false}, gKey{false}, hKey{false}, iKey{false}, jKey{false};
    bool kKey{false}, lKey{false}, mKey{false}, nKey{false}, oKey{false};
    bool pKey{false}, qKey{false}, rKey{false}, sKey{false}, tKey{false};
    bool uKey{false}, vKey{false}, wKey{false}, xKey{false}, yKey{false}, zKey{false};

    bool kzeroKey{false}, koneKey{false}, ktwoKey{false}, kthreeKey{false}, kfourKey{false};
    bool kfiveKey{false}, ksixKey{false}, ksevenKey{false}, keightKey{false}, knineKey{false};

    bool zeroKey{false}, oneKey{false}, twoKey{false}, threeKey{false}, fourKey{false};
    bool fiveKey{false}, sixKey{false}, sevenKey{false}, eightKey{false}, nineKey{false};

    bool returnKey{false}, backspaceKey{false}, spaceKey{false};
    bool lshiftKey{false}, rshiftKey{false}, tabKey{false};

    bool kdivideKey{false}, kmultiplyKey{false}, kminusKey{false}, kplusKey{false};
    bool kenterKey{false}, kperiodKey{false};

    bool minusKey{false}, equalsKey{false}, semicolonKey{false};
    bool commaKey{false}, periodKey{false}, slashKey{false};

    bool f1Key{false}, f2Key{false}, f3Key{false}, f4Key{false}, f5Key{false};
    bool f9Key{false}, f10Key{false}, f11Key{false}, f12Key{false};

    bool insertKey{false}, homeKey{false}, pageupKey{false};
    bool deleteKey{false}, endKey{false}, pagedownKey{false};

    bool getGamepadKeyA{false}, getGamepadKeyB{false}, getGamepadKeyC{false};

    // Registered event keys (array of Key with modifier bindings)
    std::vector<SdleKey> eventKeys;

    // Reset all per-frame key booleans
    void resetFrameKeys();
};

// =========================================================================
// Module-level API
// =========================================================================

/// Get the global sdlevent state (singleton).
SdleventState& sdlevent_get_state();

/// Poll all pending SDL events and update key states.
/// Returns false if quit was requested.
bool sdlevent_event_update();

/// Main event/frame-tick function. Called once per frame with the target FPS.
/// Manages timing (delay/skip), resets per-frame keys, and calls eventUpdate().
/// Returns false if quit was requested.
bool sdlevent_event(int fps);

} // namespace ikemen::ssz_native
