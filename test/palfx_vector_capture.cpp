// palfx_vector_capture.cpp — Known-answer vector capture for PalFX
//
// Captures output from the REAL palfx_transform_palette() into a C++ header
// file (palfx_known_vectors.hpp) that can be included by the parity test
// to validate future implementations against a known-good baseline.
//
// Usage:
//   make capture-vectors
//
// This links against the real common_service.o and captures the actual
// implementation's output for a defined set of PalFX configurations.
//
// Build & run:
//   g++ -std=c++17 -O2 -I main -I main/ssz -I main/ssz_native \
//       -o build/capture_palfx_vectors \
//       test/palfx_vector_capture.cpp \
//       test/palfx_stubs.cpp \
//       build/Debug/main/ssz_native/common_service.o \
//       -static-libgcc -static-libstdc++
//   ./build/capture_palfx_vectors

#include <cstdint>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <vector>
#include <string>
#include <cstring>

#include "common_service.hpp"

using ikemen::ssz_native::PalFXData;
using ikemen::ssz_native::palfx_transform_palette;

// =========================================================================
// Input palette generators
// =========================================================================

// Pattern palette: cycles through 5 specific colors (black, white, red, green, blue)
static std::vector<uint32_t> make_pattern_palette() {
    static const uint32_t colors[] = {
        0x00000000,  // black
        0x00FFFFFF,  // white
        0x00FF0000,  // red
        0x0000FF00,  // green
        0x000000FF   // blue
    };
    std::vector<uint32_t> pal(256);
    for (int i = 0; i < 256; i++)
        pal[i] = colors[i % 5];
    return pal;
}

// Rainbow palette: each channel uses different prime stepping
static std::vector<uint32_t> make_rainbow_palette() {
    std::vector<uint32_t> pal(256);
    for (int i = 0; i < 256; i++) {
        uint32_t r = (i * 7) % 256;
        uint32_t g = (i * 13) % 256;
        uint32_t b = (i * 23) % 256;
        pal[i] = (r << 16) | (g << 8) | b;
    }
    return pal;
}

// Edge palette: boundary values (0/255 combos for first 8, then stepping values)
static std::vector<uint32_t> make_edge_palette() {
    std::vector<uint32_t> pal(256);
    for (int i = 0; i < 256; i++) {
        uint32_t r = (i & 1) ? 255 : 0;
        uint32_t g = (i & 2) ? 255 : 0;
        uint32_t b = (i & 4) ? 255 : 0;
        if (i >= 8) {
            r = (i % 256);
            g = (i * 2) % 256;
            b = (i * 3) % 256;
        }
        pal[i] = (r << 16) | (g << 8) | b;
    }
    return pal;
}

// Gray ramp palette
static std::vector<uint32_t> make_gray_palette() {
    std::vector<uint32_t> pal(256);
    for (int i = 0; i < 256; i++)
        pal[i] = static_cast<uint32_t>(i) * 0x010101;
    return pal;
}

// =========================================================================
// Test case definitions
// =========================================================================

enum PaletteType {
    PAL_PATTERN,
    PAL_RAINBOW,
    PAL_EDGE,
    PAL_GRAY
};

struct TestCase {
    const char* name;
    PalFXData palfx;
    PaletteType palette_type;
    bool nega;
};

static void clear(PalFXData& fx) {
    std::memset(&fx, 0, sizeof(fx));
    fx.emulr = fx.emulg = fx.emulb = 256;
    fx.eaddr = fx.eaddg = fx.eaddb = 0;
    fx.ecolor = 1.0f;
    fx.einvertall = 0;
    fx.enegType = 0;
}

static std::vector<uint32_t> make_palette(PaletteType type) {
    switch (type) {
        case PAL_PATTERN: return make_pattern_palette();
        case PAL_RAINBOW: return make_rainbow_palette();
        case PAL_EDGE:    return make_edge_palette();
        case PAL_GRAY:    return make_gray_palette();
        default:          return make_pattern_palette();
    }
}

// Test cases are defined inline in main() below.

// =========================================================================
// Main: generate vector header
// =========================================================================

int main() {
    // ── Define all test cases ──
    // Each case: name, PalFXData setup, palette type, nega
    struct CaseDef {
        const char* name;
        void (*setup)(PalFXData&);
        PaletteType palette;
        bool nega;
    };

    // Helper to setup a PalFXData
    auto set_default = [](PalFXData& fx) { /* defaults are fine */ };
    auto set_invert  = [](PalFXData& fx) { fx.einvertall = 1; };
    auto set_ecolor_0   = [](PalFXData& fx) { fx.ecolor = 0.0f; };
    auto set_ecolor_25  = [](PalFXData& fx) { fx.ecolor = 0.25f; };
    auto set_ecolor_50  = [](PalFXData& fx) { fx.ecolor = 0.5f; };
    auto set_ecolor_75  = [](PalFXData& fx) { fx.ecolor = 0.75f; };
    auto set_ecolor_100 = [](PalFXData& fx) { fx.ecolor = 1.0f; };
    auto set_sub_neg1   = [](PalFXData& fx) { fx.eaddr = -1; fx.eaddg = -1; fx.eaddb = -1; };
    auto set_sub_neg50  = [](PalFXData& fx) { fx.eaddr = -50; fx.eaddg = -50; fx.eaddb = -50; };
    auto set_sub_neg128 = [](PalFXData& fx) { fx.eaddr = -128; fx.eaddg = -128; fx.eaddb = -128; };
    auto set_sub_neg255 = [](PalFXData& fx) { fx.eaddr = -255; fx.eaddg = -255; fx.eaddb = -255; };
    auto set_sub_asym   = [](PalFXData& fx) { fx.eaddr = -1; fx.eaddg = -5; fx.eaddb = -10; };
    auto set_mul_1x     = [](PalFXData& fx) { fx.emulr = 256; fx.emulg = 256; fx.emulb = 256; };
    auto set_mul_half   = [](PalFXData& fx) { fx.emulr = 128; fx.emulg = 128; fx.emulb = 128; };
    auto set_mul_2x     = [](PalFXData& fx) { fx.emulr = 512; fx.emulg = 512; fx.emulb = 512; };
    auto set_mul_asym   = [](PalFXData& fx) { fx.emulr = 64; fx.emulg = 128; fx.emulb = 256; };
    auto set_mul_add    = [](PalFXData& fx) {
        fx.emulr = 200; fx.emulg = 180; fx.emulb = 220;
        fx.eaddr = -10; fx.eaddg = -20; fx.eaddb = -30;
    };
    auto set_full_pipe  = [](PalFXData& fx) {
        fx.einvertall = 1;
        fx.ecolor = 0.5f;
        fx.eaddr = -10; fx.eaddg = -20; fx.eaddb = -30;
        fx.emulr = 200; fx.emulg = 180; fx.emulb = 220;
    };
    auto set_neg_mode   = [](PalFXData& fx) {
        fx.enegType = 0;
        fx.emulr = 128; fx.emulg = 128; fx.emulb = 128;
        fx.eaddr = 30; fx.eaddg = 30; fx.eaddb = 30;
    };
    auto set_brighten   = [](PalFXData& fx) {
        fx.ecolor = 0.25f;
        fx.eaddr = 40; fx.eaddg = 30; fx.eaddb = 20;
        fx.emulr = 300; fx.emulg = 280; fx.emulb = 260;
    };
    auto set_zero_mulr  = [](PalFXData& fx) {
        fx.emulr = 0; fx.emulg = 256; fx.emulb = 256;
        fx.eaddr = 10; fx.eaddg = 10; fx.eaddb = 10;
    };

    CaseDef cases[] = {
        // ── Identity ──
        { "identity_pattern",       set_default,  PAL_PATTERN, false },
        { "identity_rainbow",       set_default,  PAL_RAINBOW, false },
        { "identity_edge",          set_default,  PAL_EDGE,    false },
        { "identity_gray",          set_default,  PAL_GRAY,    false },

        // ── Invert all ──
        { "invert_pattern",         set_invert,   PAL_PATTERN, false },
        { "invert_rainbow",         set_invert,   PAL_RAINBOW, false },
        { "invert_edge",            set_invert,   PAL_EDGE,    false },

        // ── ecolor blend ──
        { "ecolor_0.0_rainbow",     set_ecolor_0,   PAL_RAINBOW, false },
        { "ecolor_0.25_rainbow",    set_ecolor_25,  PAL_RAINBOW, false },
        { "ecolor_0.5_rainbow",     set_ecolor_50,  PAL_RAINBOW, false },
        { "ecolor_0.75_rainbow",    set_ecolor_75,  PAL_RAINBOW, false },
        { "ecolor_1.0_rainbow",     set_ecolor_100, PAL_RAINBOW, false },

        // ── Saturated subtraction ──
        { "sub_neg1_edge",          set_sub_neg1,   PAL_EDGE, false },
        { "sub_neg50_edge",         set_sub_neg50,  PAL_EDGE, false },
        { "sub_neg128_edge",        set_sub_neg128, PAL_EDGE, false },
        { "sub_neg255_edge",        set_sub_neg255, PAL_EDGE, false },
        { "sub_asym_edge",          set_sub_asym,   PAL_EDGE, false },

        // ── Per-channel multiply ──
        { "mul_1x_rainbow",         set_mul_1x,     PAL_RAINBOW, false },
        { "mul_half_rainbow",       set_mul_half,   PAL_RAINBOW, false },
        { "mul_2x_rainbow",         set_mul_2x,     PAL_RAINBOW, false },
        { "mul_asym_rainbow",       set_mul_asym,   PAL_RAINBOW, false },

        // ── Combined operations ──
        { "mul_add_combined",       set_mul_add,    PAL_RAINBOW, false },
        { "full_pipeline",          set_full_pipe,  PAL_RAINBOW, false },
        { "brighten_ecolor",        set_brighten,   PAL_RAINBOW, false },
        { "zero_mulr",              set_zero_mulr,  PAL_RAINBOW, false },

        // ── Neg mode (nega=true with enegType=0) ──
        { "neg_mode_pattern",       set_neg_mode,   PAL_PATTERN, true },
    };

    const int num_cases = sizeof(cases) / sizeof(cases[0]);

    // ── Generate header file ──
    printf("// palfx_known_vectors.hpp — Auto-generated by capture_palfx_vectors.\n");
    printf("// DO NOT EDIT. Regenerate with: make capture-vectors\n");
    printf("#pragma once\n");
    printf("#include <cstdint>\n\n");
    printf("// %d known test vectors\n", num_cases);
    printf("// Generated by palfx_vector_capture.cpp on %s %s\n\n",
           __DATE__, __TIME__);
    printf("struct VectorEntry {\n");
    printf("    const char* name;\n");
    printf("    uint32_t expected[256];\n");
    printf("};\n\n");
    printf("static const VectorEntry kKnownVectors[%d] = {\n", num_cases);

    int total_vectors = 0;

    for (int i = 0; i < num_cases; i++) {
        const auto& c = cases[i];

        // Build PalFXData
        PalFXData fx;
        clear(fx);
        c.setup(fx);

        // Generate input palette
        std::vector<uint32_t> pal = make_palette(c.palette);

        // Call the real implementation
        const std::vector<uint32_t>& result = palfx_transform_palette(fx, pal, c.nega);

        // Output as C++ initializer
        printf("    { \"%s\", {\n", c.name);

        for (int j = 0; j < 256; j++) {
            printf("        0x%08X", result[j]);
            if (j < 255) printf(",");
            if ((j + 1) % 4 == 0 || j == 255) {
                printf("\n");
            }
        }

        printf("    } },\n");
        total_vectors++;
    }

    printf("};\n\n");
    printf("static const int kKnownVectorCount = %d;\n", total_vectors);

    fprintf(stderr, "Generated %d known-answer vectors\n", total_vectors);
    return 0;
}
