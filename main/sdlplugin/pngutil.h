#pragma once
//
// pngutil.h — dimension/size validation for decoded images.
//
// Every PNG decode path in sdlplugin.cpp used to size its destination buffer
// with a 32-bit product of two libpng `png_uint_32` dimensions, e.g.
//
//     out.resize(height * width);          // both operands 32-bit → wraps
//     new png_byte[width * height * 4];    // wraps even earlier
//
// libpng's default limits allow 1,000,000 pixels per side and impose no cap on
// the total, so a small, highly compressible 65536x65536 PNG makes the product
// wrap to 0 and then png_read_image() writes gigabytes into a zero-sized
// buffer. The helpers below are the single place where a decoded image is
// validated; they take 64-bit arguments so no arithmetic can wrap, and they are
// unit-tested in test/test_image_size.cpp.
//
// Any decode path must call pngCheckImageSize() *before* allocating, and pass
// PNG_MAX_DIM to png_set_user_limits() so libpng rejects oversized headers as
// early as possible.
//

#include <stdint.h>
#include <stddef.h>

// --- Policy -------------------------------------------------------------
// Per-side pixel cap: above every sane M.U.G.E.N sprite/background (the
// largest stock assets are ~4000 px wide) and at the maximum texture size of
// old GL 2.1 hardware.
static const uint64_t PNG_MAX_DIM = 16384;
// Total pixel cap (16 Mi-pixel = 4096x4096).
static const uint64_t PNG_MAX_PIXELS = 4096ull * 4096ull;
// Absolute byte cap for one decoded image (4 channels at the pixel cap = 64 MiB;
// the slack absorbs future 16-bit/RGBA16 expansion).
static const uint64_t PNG_MAX_BYTES = 256ull * 1024ull * 1024ull;

/// Validate a decoded image size and compute its byte count.
///
/// @param w,h       dimensions reported by libpng (may be attacker-controlled)
/// @param channels  bytes per pixel the destination expects (1, 2, 3, 4, 8)
/// @param outBytes  receives w*h*channels on success
/// @return false if the image is empty, oversized, or the byte count overflows.
///
/// All arithmetic is 64-bit, so no product can wrap.
static inline bool pngCheckImageSize(
	uint64_t w, uint64_t h, uint64_t channels, uint64_t* outBytes)
{
	if(!outBytes) return false;
	if(w == 0 || h == 0) return false;
	if(channels == 0 || channels > 8) return false;
	if(w > PNG_MAX_DIM || h > PNG_MAX_DIM) return false;
	// Both factors are <= 16384 here, so w*h <= 2^28: no wrap.
	if(w * h > PNG_MAX_PIXELS) return false;
	const uint64_t bytes = w * h * channels; // <= 2^28 * 8
	if(bytes > PNG_MAX_BYTES) return false;
	*outBytes = bytes;
	return true;
}
