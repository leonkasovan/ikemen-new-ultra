// palfx_stubs.cpp — Link-time stubs for symbols referenced by common_service.o
// that are not needed by palfx_transform_palette() at runtime.
//
// These stubs satisfy the linker so the parity test can link against the real
// common_service.o. They are never executed — palfx_transform_palette() does
// not call fill() or softFill().

#include <cstdint>

namespace ikemen::ssz_native {

struct SdlRect {
    int x{}, y{}, w{}, h{};
    void set(int x_, int y_, int w_, int h_) {
        x = x_; y = y_; w = w_; h = h_;
    }
};

// Stub: screen fill (called by common_screen_fill, not by palfx_transform_palette)
void fill(const SdlRect&, uint32_t) {}

// Stub: software fill (called by common_rect_fill, not by palfx_transform_palette)
void softFill(const SdlRect&, uint32_t) {}

} // namespace ikemen::ssz_native
