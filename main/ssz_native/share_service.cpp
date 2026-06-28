// share_service.cpp — Stub implementations for ShareData copy/push.
//
// Phase 3: These are stubs.  Each copy/push field assignment will be
// wired to the corresponding native state accessor as that SSZ module
// is converted.

#include "share_service.hpp"

namespace ikemen::ssz_native {

void share_copy(ShareData&) {
    // Phase 3: read from native char/com/cmd/fnt/snd/cfg/se/sc/stage state
}

void share_push(const ShareData&) {
    // Phase 3: write to native char/com/cmd/fnt/snd/cfg/se/sc/stage state
}

} // namespace ikemen::ssz_native
