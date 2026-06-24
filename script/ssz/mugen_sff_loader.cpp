// script/ssz/mugen_sff_loader.cpp
// Self-contained mugen_sff decoder producing ikemen::Sprite output.
// Based on test_sff.cpp's loadMugenSff() which matches mugen_sff.cpp exactly.

#include "mugen_sff_loader.hpp"
#include "sff.hpp"
#include "common.hpp"
#include "sszdef.h"

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <string>
#include <map>
#include <vector>
#include <array>

namespace ikemen {

// ═══════════════════════════════════════════════════════════════════════
// Internal structures matching mugen_sff.h
// ═══════════════════════════════════════════════════════════════════════

#pragma pack(push, 1)
struct rgb_t { uint8_t r, g, b; };
#pragma pack(pop)

struct SffFileHeader {
	uint8_t  Ver3, Ver2, Ver1, Ver0;
	uint32_t FirstSpriteHeaderOffset;
	uint32_t FirstPaletteHeaderOffset;
	uint32_t NumberOfSprites;
	uint32_t NumberOfPalettes;
};

// ═══════════════════════════════════════════════════════════════════════
// Decoders (exact matches from mugen_sff.cpp via test_sff.cpp)
// ═══════════════════════════════════════════════════════════════════════

static uint8_t* rlePcxDecode(int w, int h, int rle, const uint8_t* srcPx, size_t srcLen)
{
	if (srcLen == 0) return NULL;
	size_t dstLen = static_cast<size_t>(w) * static_cast<size_t>(h);
	uint8_t* dstPx = static_cast<uint8_t*>(malloc(dstLen));
	if (!dstPx) return NULL;

	if (rle <= 0) {
		size_t i = 0, j = 0;
		while (i < srcLen && j < dstLen) {
			uint8_t byte = srcPx[i++];
			int count = 1;
			if ((byte & 0xC0) == 0xC0) {
				count = byte & 0x3F;
				if (i < srcLen) byte = srcPx[i++];
			}
			while (count-- > 0 && j < dstLen)
				dstPx[j++] = byte;
		}
		if (j < dstLen) memset(dstPx + j, 0, dstLen - j);
		return dstPx;
	}

	size_t i = 0, j = 0, k = 0;
	while (j < dstLen) {
		int n = 1;
		uint8_t d = srcPx[i];
		if (i < srcLen - 1) i++;
		if (d >= 0xC0) {
			n = static_cast<int>(d & 0x3F);
			d = srcPx[i];
			if (i < srcLen - 1) i++;
		}
		for (; n > 0; n--) {
			if (static_cast<int>(k) < w && j < dstLen) {
				dstPx[j] = d;
				j++;
			}
			k++;
			if (k == static_cast<size_t>(rle)) {
				k = 0;
				n = 1;
			}
		}
	}
	return dstPx;
}

static uint8_t* rle8Decode(int w, int h, const uint8_t* srcPx, size_t srcLen)
{
	if (srcLen == 0) return NULL;
	size_t dstLen = static_cast<size_t>(w) * static_cast<size_t>(h);
	uint8_t* dstPx = static_cast<uint8_t*>(malloc(dstLen));
	if (!dstPx) return NULL;

	size_t i = 0, j = 0;
	while (j < dstLen) {
		long n = 1;
		uint8_t d = srcPx[i];
		if (i < srcLen - 1) i++;
		if ((d & 0xC0) == 0x40) {
			n = d & 0x3F;
			d = srcPx[i];
			if (i < srcLen - 1) i++;
		}
		while (n-- > 0 && j < dstLen)
			dstPx[j++] = d;
	}
	return dstPx;
}

static uint8_t* rle5Decode(int w, int h, const uint8_t* srcPx, size_t srcLen)
{
	if (srcLen == 0) return NULL;
	size_t dstLen = static_cast<size_t>(w) * static_cast<size_t>(h);
	uint8_t* dstPx = static_cast<uint8_t*>(malloc(dstLen));
	if (!dstPx) return NULL;

	size_t i = 0, j = 0;
	while (j < dstLen) {
		int rl = static_cast<int>(srcPx[i]);
		if (i < srcLen - 1) i++;
		int dl = static_cast<int>(srcPx[i] & 0x7F);
		uint8_t c = 0;
		if (srcPx[i] >> 7 != 0) {
			if (i < srcLen - 1) i++;
			c = srcPx[i];
		}
		if (i < srcLen - 1) i++;
		for (;;) {
			if (j < dstLen) dstPx[j++] = c;
			rl--;
			if (rl < 0) {
				dl--;
				if (dl < 0) break;
				c = srcPx[i] & 0x1F;
				rl = static_cast<int>(srcPx[i] >> 5);
				if (i < srcLen - 1) i++;
			}
		}
	}
	return dstPx;
}

static uint8_t* lz5Decode(int w, int h, const uint8_t* srcPx, size_t srcLen)
{
	if (srcLen == 0) return NULL;
	size_t dstLen = static_cast<size_t>(w) * static_cast<size_t>(h);
	uint8_t* dstPx = static_cast<uint8_t*>(malloc(dstLen));
	if (!dstPx) return NULL;

	size_t i = 0, j = 0;
	uint8_t ct = srcPx[i], cts = 0, rb = 0, rbc = 0;
	if (i < srcLen - 1) i++;

	while (j < dstLen) {
		int d = static_cast<int>(srcPx[i]);
		if (i < srcLen - 1) i++;

		if (ct & (1 << cts)) {
			if ((d & 0x3F) == 0) {
				d = (d << 2 | static_cast<int>(srcPx[i])) + 1;
				if (i < srcLen - 1) i++;
				long n = static_cast<int>(srcPx[i]) + 2;
				if (i < srcLen - 1) i++;
				for (;;) {
					if (j < dstLen && j >= static_cast<size_t>(d))
						dstPx[j] = dstPx[j - static_cast<size_t>(d)];
					j++;
					n--;
					if (n < 0) break;
				}
			} else {
				rb = static_cast<uint8_t>((rb & ~(0xC0 >> rbc)) | ((d & 0xC0) >> rbc));
				rbc += 2;
				long n = d & 0x3F;
				if (rbc < 8) {
					d = static_cast<int>(srcPx[i]) + 1;
					if (i < srcLen - 1) i++;
				} else {
					d = static_cast<int>(rb) + 1;
					rb = 0; rbc = 0;
				}
				for (;;) {
					if (j < dstLen && j >= static_cast<size_t>(d))
						dstPx[j] = dstPx[j - static_cast<size_t>(d)];
					j++;
					n--;
					if (n < 0) break;
				}
			}
		} else {
			if ((d & 0xE0) == 0) {
				long n = static_cast<int>(srcPx[i]) + 8;
				if (i < srcLen - 1) i++;
				while (n-- > 0 && j < dstLen) {
					dstPx[j] = static_cast<uint8_t>(d);
					j++;
				}
			} else {
				long n = d >> 5;
				d &= 0x1F;
				while (n-- > 0 && j < dstLen) {
					dstPx[j] = static_cast<uint8_t>(d);
					j++;
				}
			}
		}
		cts++;
		if (cts >= 8) {
			ct = srcPx[i];
			cts = 0;
			if (i < srcLen - 1) i++;
		}
	}
	return dstPx;
}

// ═══════════════════════════════════════════════════════════════════════
// Static helpers
// ═══════════════════════════════════════════════════════════════════════

static bool isPaletted(int rle)
{
	return (rle == -1 || rle == -2 || rle == -3 || rle == -4 || rle == -10);
}

static void spriteCopy(Sprite& dst, const Sprite& src)
{
	dst.imageGroup  = src.imageGroup;
	dst.imageNumber = src.imageNumber;
	dst.rct         = src.rct;
	dst.palidx      = src.palidx;
	dst.rle         = src.rle;
	dst.coldepth    = src.coldepth;
	dst.pxl         = src.pxl;
	dst.colorPallet = src.colorPallet;
	dst.pluginbuf   = src.pluginbuf;
}

// ═══════════════════════════════════════════════════════════════════════
// SFF header reader (FILE* based, matching mugen_sff.cpp)
// ═══════════════════════════════════════════════════════════════════════

static int readSffHeader(SffFileHeader* hdr, FILE* file, uint32_t* lofs, uint32_t* tofs)
{
	char check[12];
	if (fread(check, 12, 1, file) != 1) return -1;
	if (memcmp(check, "ElecbyteSpr\0", 12) != 0) return -1;

	if (fread(&hdr->Ver3, 1, 1, file) != 1) return -1;
	if (fread(&hdr->Ver2, 1, 1, file) != 1) return -1;
	if (fread(&hdr->Ver1, 1, 1, file) != 1) return -1;
	if (fread(&hdr->Ver0, 1, 1, file) != 1) return -1;

	uint32_t dummy;
	if (fread(&dummy, sizeof(uint32_t), 1, file) != 1) return -1;

	if (hdr->Ver0 == 2) {
		for (int i = 0; i < 4; i++)
			if (fread(&dummy, sizeof(uint32_t), 1, file) != 1) return -1;
		if (fread(&hdr->FirstSpriteHeaderOffset, sizeof(uint32_t), 1, file) != 1) return -1;
		if (fread(&hdr->NumberOfSprites, sizeof(uint32_t), 1, file) != 1) return -1;
		if (fread(&hdr->FirstPaletteHeaderOffset, sizeof(uint32_t), 1, file) != 1) return -1;
		if (fread(&hdr->NumberOfPalettes, sizeof(uint32_t), 1, file) != 1) return -1;
		if (fread(lofs, sizeof(uint32_t), 1, file) != 1) return -1;
		if (fread(&dummy, sizeof(uint32_t), 1, file) != 1) return -1;
		if (fread(tofs, sizeof(uint32_t), 1, file) != 1) return -1;
	} else if (hdr->Ver0 == 1) {
		if (fread(&hdr->NumberOfSprites, sizeof(uint32_t), 1, file) != 1) return -1;
		if (fread(&hdr->FirstSpriteHeaderOffset, sizeof(uint32_t), 1, file) != 1) return -1;
		hdr->FirstPaletteHeaderOffset = 0;
		hdr->NumberOfPalettes = 0;
		*lofs = 0;
		*tofs = 0;
	} else {
		return -1;
	}
	return 0;
}

// ═══════════════════════════════════════════════════════════════════════
// PCX header reader
// ═══════════════════════════════════════════════════════════════════════

static int readPcxHeader(int& w, int& h, int& rleOut, FILE* file, uint64_t offset)
{
	fseek(file, static_cast<long>(offset), SEEK_SET);
	uint16_t dummy16;
	if (fread(&dummy16, sizeof(uint16_t), 1, file) != 1) return -1;
	uint8_t encoding, bpp;
	if (fread(&encoding, sizeof(uint8_t), 1, file) != 1) return -1;
	if (fread(&bpp, sizeof(uint8_t), 1, file) != 1) return -1;
	if (bpp != 8) return -1;
	uint16_t rect[4];
	if (fread(rect, sizeof(uint16_t), 4, file) != 4) return -1;
	fseek(file, static_cast<long>(offset + 66), SEEK_SET);
	uint16_t bpl;
	if (fread(&bpl, sizeof(uint16_t), 1, file) != 1) return -1;
	w = rect[2] - rect[0] + 1;
	h = rect[3] - rect[1] + 1;
	rleOut = bpl;
	return 0;
}

// ═══════════════════════════════════════════════════════════════════════
// V1 sprite header reader
// ═══════════════════════════════════════════════════════════════════════

static int readSpriteHeaderV1(int16_t& group, int16_t& num,
	int16_t& ox, int16_t& oy,
	uint32_t* ofs, uint32_t* size, uint16_t* link, FILE* file)
{
	if (fread(ofs, sizeof(uint32_t), 1, file) != 1) return -1;
	if (fread(size, sizeof(uint32_t), 1, file) != 1) return -1;
	if (fread(&ox, sizeof(int16_t), 1, file) != 1) return -1;
	if (fread(&oy, sizeof(int16_t), 1, file) != 1) return -1;
	if (fread(&group, sizeof(int16_t), 1, file) != 1) return -1;
	if (fread(&num, sizeof(int16_t), 1, file) != 1) return -1;
	if (fread(link, sizeof(uint16_t), 1, file) != 1) return -1;
	return 0;
}

// ═══════════════════════════════════════════════════════════════════════
// V2 sprite header reader
// ═══════════════════════════════════════════════════════════════════════

static int readSpriteHeaderV2(int16_t& group, int16_t& num,
	int16_t& w, int16_t& h, int16_t& ox, int16_t& oy,
	uint32_t* ofs, uint32_t* size, int& rle, uint8_t& coldepth,
	uint32_t lofs, uint32_t tofs, uint16_t* link, FILE* file)
{
	if (fread(&group, sizeof(int16_t), 1, file) != 1) return -1;
	if (fread(&num, sizeof(int16_t), 1, file) != 1) return -1;
	if (fread(&w, sizeof(int16_t), 1, file) != 1) return -1;
	if (fread(&h, sizeof(int16_t), 1, file) != 1) return -1;
	if (fread(&ox, sizeof(int16_t), 1, file) != 1) return -1;
	if (fread(&oy, sizeof(int16_t), 1, file) != 1) return -1;
	if (fread(link, sizeof(uint16_t), 1, file) != 1) return -1;
	char fmt;
	if (fread(&fmt, sizeof(char), 1, file) != 1) return -1;
	rle = -fmt;
	if (fread(&coldepth, sizeof(uint8_t), 1, file) != 1) return -1;
	if (fread(ofs, sizeof(uint32_t), 1, file) != 1) return -1;
	if (fread(size, sizeof(uint32_t), 1, file) != 1) return -1;
	uint16_t tmp;
	if (fread(&tmp, sizeof(uint16_t), 1, file) != 1) return -1;
	int palidx_ = static_cast<int>(tmp);
	if (fread(&tmp, sizeof(uint16_t), 1, file) != 1) return -1;
	if ((tmp & 1) == 0)
		*ofs += lofs;
	else
		*ofs += tofs;
	return palidx_;
}

// ═══════════════════════════════════════════════════════════════════════
// V1 sprite data reader
// ═══════════════════════════════════════════════════════════════════════

static uint8_t* readSpriteDataV1(int& w, int& h, int& rle, int& palidx,
	std::vector<uint32_t>& palOut,
	FILE* file, uint64_t offset, uint32_t datasize,
	uint32_t nextSubheader, Sprite* prev, bool c00)
{
	if (nextSubheader > static_cast<uint32_t>(offset))
		datasize = nextSubheader - static_cast<uint32_t>(offset);

	uint8_t ps;
	if (fread(&ps, sizeof(uint8_t), 1, file) != 1) return NULL;
	bool paletteSame = (ps != 0 && prev != NULL);

	if (readPcxHeader(w, h, rle, file, offset) != 0) return NULL;

	fseek(file, static_cast<long>(offset + 128), SEEK_SET);
	uint32_t palSize = (c00 || paletteSame) ? 0 : 768;
	if (datasize < 128 + palSize)
		datasize = 128 + palSize;

	size_t srcLen = static_cast<size_t>(datasize) - (128 + palSize);
	uint8_t* srcPx = static_cast<uint8_t*>(malloc(srcLen));
	if (!srcPx) return NULL;
	if (fread(srcPx, srcLen, 1, file) != 1) { free(srcPx); return NULL; }

	uint8_t* px = NULL;
	if (paletteSame) {
		if (prev != NULL)
			palidx = prev->palidx;
		if (palidx < 0) { free(srcPx); return NULL; }
		px = rlePcxDecode(w, h, rle, srcPx, srcLen);
		rle = -1;
	} else {
		if (c00)
			fseek(file, static_cast<long>(offset + datasize - 768), SEEK_SET);
		rgb_t pal_rgb[256];
		if (fread(pal_rgb, sizeof(pal_rgb), 1, file) != 1) { free(srcPx); return NULL; }
		palOut.resize(256, 0);
		for (int i = 0; i < 256; i++) {
			// Store as 0x00RRGGBB (matching Sprite::read() format)
			palOut[static_cast<size_t>(i)] =
				(static_cast<uint32_t>(pal_rgb[i].r) << 16) |
				(static_cast<uint32_t>(pal_rgb[i].g) << 8) |
				static_cast<uint32_t>(pal_rgb[i].b);
		}
		palidx = 0;
		px = rlePcxDecode(w, h, rle, srcPx, srcLen);
		rle = -1;
	}
	free(srcPx);
	return px;
}

// ═══════════════════════════════════════════════════════════════════════
// V2 sprite data reader
// ═══════════════════════════════════════════════════════════════════════

static uint8_t* readSpriteDataV2(int w, int h, int rle, uint8_t coldepth,
	uint8_t*& pxOut, size_t& pxOutLen,
	FILE* file, uint64_t offset, uint32_t datasize)
{
	uint8_t* px = NULL;
	if (rle > 0) return NULL;

	if (rle == 0) {
		if (datasize == 0) return NULL;
		px = static_cast<uint8_t*>(malloc(datasize));
		if (!px) return NULL;
		fseek(file, static_cast<long>(offset), SEEK_SET);
		if (fread(px, datasize, 1, file) != 1) { free(px); return NULL; }
		pxOut = px;
		pxOutLen = datasize;
		return px;
	}

	fseek(file, static_cast<long>(offset + 4), SEEK_SET);
	int format = -rle;
	if (datasize < 4) datasize = 4;
	size_t srcLen = static_cast<size_t>(datasize) - 4;
	uint8_t* srcPx = static_cast<uint8_t*>(malloc(srcLen));
	if (!srcPx) return NULL;
	if (fread(srcPx, srcLen, 1, file) != 1) { free(srcPx); return NULL; }

	switch (format) {
	case 2:  px = rle8Decode(w, h, srcPx, srcLen); break;
	case 3:  px = rle5Decode(w, h, srcPx, srcLen); break;
	case 4:  px = lz5Decode(w, h, srcPx, srcLen);  break;
	case 10: case 11: case 12:
		// PNG not supported in the mugen loader reference for this test
		break;
	default: break;
	}
	free(srcPx);

	if (px) {
		pxOut = px;
		pxOutLen = static_cast<size_t>(w) * static_cast<size_t>(h);
	}
	return px;
}

// ═══════════════════════════════════════════════════════════════════════
// Main entry point
// ═══════════════════════════════════════════════════════════════════════

bool loadMugenSff(const std::wstring& path,
                  std::map<uint64_t, Sprite>& sprites,
                  std::string& filename)
{
	// Convert wide path to narrow for fopen
	char narrowPath[512];
	size_t n = wcstombs(narrowPath, path.c_str(), sizeof(narrowPath) - 1);
	if (n == (size_t)-1) {
		LOG_INFO("ENGINE", "loadMugenSff: path conversion failed");
		return false;
	}
	narrowPath[n] = '\0';

	FILE* file = fopen(narrowPath, "rb");
	if (!file) {
		LOG_INFO("ENGINE", "loadMugenSff: cannot open %s", narrowPath);
		return false;
	}

	filename = narrowPath;

	uint32_t lofs = 0, tofs = 0;
	SffFileHeader hdr;
	if (readSffHeader(&hdr, file, &lofs, &tofs) != 0) {
		LOG_INFO("ENGINE", "loadMugenSff: header read failed");
		fclose(file);
		return false;
	}

	bool isV2 = (hdr.Ver0 == 2);

	// ── V2: pre-load palettes ──────────────────────────────────────
	std::vector<std::vector<uint32_t>> v2Palettes;
	std::map<std::array<int, 2>, int> uniquePals;

	if (isV2 && hdr.NumberOfPalettes > 0) {
		v2Palettes.resize(hdr.NumberOfPalettes);
		for (uint32_t i = 0; i < hdr.NumberOfPalettes; i++) {
			fseek(file, static_cast<long>(hdr.FirstPaletteHeaderOffset + i * 16), SEEK_SET);
			int16_t gn[3];
			if (fread(gn, sizeof(uint16_t), 3, file) != 3) {
				fclose(file); return false;
			}
			uint16_t link;
			if (fread(&link, sizeof(uint16_t), 1, file) != 1) {
				fclose(file); return false;
			}
			uint32_t pofs, psiz;
			if (fread(&pofs, sizeof(uint32_t), 1, file) != 1) {
				fclose(file); return false;
			}
			if (fread(&psiz, sizeof(uint32_t), 1, file) != 1) {
				fclose(file); return false;
			}

			std::array<int, 2> key = { gn[0], gn[1] };
			if (uniquePals.find(key) == uniquePals.end()) {
				fseek(file, static_cast<long>(lofs + pofs), SEEK_SET);
				uint32_t rgba[256];
				if (fread(rgba, sizeof(uint32_t), 256, file) != 256) {
					fclose(file); return false;
				}
				v2Palettes[i].resize(256, 0);
				for (int c = 0; c < 256; c++) {
					uint8_t r = static_cast<uint8_t>(rgba[c] & 0xFF);
					uint8_t g = static_cast<uint8_t>((rgba[c] >> 8) & 0xFF);
					uint8_t b = static_cast<uint8_t>((rgba[c] >> 16) & 0xFF);
					uint8_t a = static_cast<uint8_t>((rgba[c] >> 24) & 0xFF);
					// Old V2 format: no native alpha, index 0 transparent
					if (hdr.Ver2 == 0) a = (c == 0) ? 0 : 255;
					// Store as 0xAABBGGRR (matching ikemen V2 palette format)
					v2Palettes[i][static_cast<size_t>(c)] =
						(static_cast<uint32_t>(a) << 24) |
						(static_cast<uint32_t>(b) << 16) |
						(static_cast<uint32_t>(g) << 8)  |
						static_cast<uint32_t>(r);
				}
				uniquePals[key] = static_cast<int>(i);
			} else {
				v2Palettes[i] = v2Palettes[static_cast<size_t>(uniquePals[key])];
			}
		}
	}

	// ── Load sprites ───────────────────────────────────────────────
	std::vector<Sprite> spriteList(hdr.NumberOfSprites);
	Sprite* prev = NULL;
	long shofs = static_cast<long>(hdr.FirstSpriteHeaderOffset);

	for (uint32_t i = 0; i < hdr.NumberOfSprites; i++) {
		uint32_t xofs = 0, size = 0;
		uint16_t idxlnked = 0;
		Sprite& sp = spriteList[i];

		if (isV2) {
			int16_t w = 0, h = 0, ox = 0, oy = 0;
			int16_t grp = 0, num = 0;
			int rle = 0;
			uint8_t coldepth = 0;

			fseek(file, shofs, SEEK_SET);
			int palIdx = readSpriteHeaderV2(grp, num, w, h, ox, oy,
				&xofs, &size, rle, coldepth,
				lofs, tofs, &idxlnked, file);
			if (palIdx < 0) {
				fclose(file); return false;
			}

			sp.imageGroup  = grp;
			sp.imageNumber = num;
			sp.rct.x = ox;
			sp.rct.y = oy;
			sp.rct.w = (w < 0) ? 0 : w;
			sp.rct.h = (h < 0) ? 0 : h;
			sp.rle   = rle;
			sp.coldepth = coldepth;
			sp.palidx = palIdx;

			if (size == 0) {
				if (idxlnked < static_cast<uint16_t>(i)) {
					spriteCopy(sp, spriteList[idxlnked]);
				} else {
					sp.palidx = 0;
				}
			} else {
				uint8_t* pxData = NULL;
				size_t pxLen = 0;
				uint8_t* rawPx = readSpriteDataV2(sp.rct.w, sp.rct.h, rle, coldepth,
					pxData, pxLen, file,
					static_cast<uint64_t>(xofs), size);
				if (rawPx) {
					sp.pxl.assign(pxData, pxData + pxLen);
					free(rawPx);
				}
				// Copy palette from pre-loaded V2 palettes
				if (isPaletted(rle) && palIdx >= 0 &&
				    palIdx < static_cast<int>(v2Palettes.size())) {
					sp.colorPallet = v2Palettes[static_cast<size_t>(palIdx)];
				}
			}
		} else {
			// V1
			int16_t grp = 0, num = 0, ox = 0, oy = 0;
			uint32_t nextHdrOfs = 0, sz = 0;
			uint16_t link = 0;

			fseek(file, shofs, SEEK_SET);
			if (readSpriteHeaderV1(grp, num, ox, oy, &nextHdrOfs, &sz, &link, file) != 0) {
				fclose(file); return false;
			}

			sp.imageGroup  = grp;
			sp.imageNumber = num;
			sp.rct.x = ox;
			sp.rct.y = oy;

			// Data starts at shofs + 32 for V1
			int64_t dataOffset = static_cast<int64_t>(shofs) + 32;
			int pcxW = 0, pcxH = 0, pcxRle = 0;
			int palIdx = -1;
			std::vector<uint32_t> v1Pal;

			uint8_t* pxData = readSpriteDataV1(pcxW, pcxH, pcxRle, palIdx,
				v1Pal, file,
				static_cast<uint64_t>(dataOffset),
				sz, nextHdrOfs, prev, true);
			if (pxData) {
				sp.rct.w = pcxW;
				sp.rct.h = pcxH;
				sp.rle = pcxRle;
				sp.coldepth = 8;
				sp.palidx = palIdx;
				sp.colorPallet = v1Pal;
				size_t pxLen = static_cast<size_t>(pcxW) * static_cast<size_t>(pcxH);
				sp.pxl.assign(pxData, pxData + pxLen);
				free(pxData);
			}

			// Track prev sprite (matching mugen_sff.cpp logic)
			if (grp == 9000 && num == 0)
				prev = &spriteList[i];
			else
				prev = &spriteList[i];

			shofs = static_cast<long>(nextHdrOfs);
			continue;
		}

		// Track prev for V2 (matching mugen_sff.cpp logic)
		if (isV2 && (sp.imageGroup == 9000 && sp.imageNumber == 0))
			prev = &spriteList[i];
		else if (isV2)
			prev = &spriteList[i];

		// Advance to next header
		if (isV2)
			shofs += 28;
	}

	fclose(file);

	// ── Copy into output map ───────────────────────────────────────
	sprites.clear();
	for (uint32_t i = 0; i < hdr.NumberOfSprites; i++) {
		Sprite& sp = spriteList[i];
		if (sp.rct.w <= 0 || sp.rct.h <= 0) continue;
		if (sp.pxl.empty()) continue;
		uint64_t key = (static_cast<uint64_t>(sp.imageGroup) << 32) |
		               static_cast<uint32_t>(sp.imageNumber);
		sprites[key] = sp;
	}

	LOG_INFO("ENGINE", "loadMugenSff: loaded %zu sprites from %s", sprites.size(), narrowPath);
	return true;
}

} // namespace ikemen
