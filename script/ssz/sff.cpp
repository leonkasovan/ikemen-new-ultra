#include "sff.hpp"
#include "common.hpp"

#include "../file.hpp"
#include "../string.hpp"
#include "../math.hpp"
#include "../save/config.hpp"
#include "../alpha/sdlplugin.hpp"

#include <cstring>

namespace ikemen {

std::wstring SffHeader::read(File& f, uint32_t& lofs, uint32_t& tofs)
{
	uint8_t ub;
	{
		std::wstring s;
		for (int i = 0; i < 12; i++) {
			if (!f.read(ub)) return L"File could not be loaded";
			s += static_cast<wchar_t>(ub);
		}
		if (s.compare(0, 11, L"ElecbyteSpr") != 0) return L"Not Elecbyte Sprite";
	}

	if (!f.read(ver3)) return L"File could not be loaded";
	if (!f.read(ver2)) return L"File could not be loaded";
	if (!f.read(ver1)) return L"File could not be loaded";
	if (!f.read(ver0)) return L"File could not be loaded";

	uint32_t dummy;

	switch (ver0) {
	case 1:
		if (!f.read(dummy)) return L"File could not be loaded";
		numberOfPalettes = 0;
		if (!f.read(numberOfSprites)) return L"File could not be loaded";
		if (!f.read(firstSpriteHeaderOffset)) return L"File could not be loaded";
		if (!f.read(dummy)) return L"File could not be loaded";
		firstPaletteHeaderOffset = 0;
		break;
	case 2:
		if (!f.read(dummy)) return L"File could not be loaded";
		if (!f.read(dummy)) return L"File could not be loaded";
		if (!f.read(dummy)) return L"File could not be loaded";
		if (!f.read(dummy)) return L"File could not be loaded";
		if (!f.read(dummy)) return L"File could not be loaded";
		if (!f.read(firstSpriteHeaderOffset)) return L"File could not be loaded";
		if (!f.read(numberOfSprites)) return L"File could not be loaded";
		if (!f.read(firstPaletteHeaderOffset)) return L"File could not be loaded";
		if (!f.read(numberOfPalettes)) return L"File could not be loaded";
		if (!f.read(lofs)) return L"File could not be loaded";
		if (!f.read(dummy)) return L"File could not be loaded";
		if (!f.read(tofs)) return L"File could not be loaded";
		break;
	default:
		return L"Invalid version";
	}
	return L"";
}

void PalleteList::clear()
{
	m_palettes.clear();
	m_palidxs.clear();
}

std::vector<uint32_t>& PalleteList::newPal(uint32_t& pi)
{
	pi = static_cast<uint32_t>(m_palettes.size());
	m_palidxs.push_back(static_cast<int>(pi));
	m_palettes.emplace_back(256, 0);
	return m_palettes.back();
}

void PalleteList::setSource(int pi, const std::vector<uint32_t>& pal)
{
	if (pi < 0) return;
	while (static_cast<size_t>(pi) >= m_palettes.size())
		m_palettes.emplace_back();
	while (static_cast<size_t>(pi) >= m_palidxs.size())
		m_palidxs.push_back(static_cast<int>(m_palidxs.size()));
	m_palettes[static_cast<size_t>(pi)] = pal;
	m_palidxs[static_cast<size_t>(pi)] = pi;
}

std::vector<uint32_t>& PalleteList::get(int pi)
{
	static std::vector<uint32_t> empty;
	if (pi < 0 || pi >= static_cast<int>(m_palidxs.size())) return empty;
	int idx = m_palidxs[pi];
	if (idx < 0 || idx >= static_cast<int>(m_palettes.size())) return empty;
	return m_palettes[static_cast<size_t>(idx)];
}

void PalleteList::remap(int source, int dest)
{
	if (source >= 0 && source < static_cast<int>(m_palidxs.size()))
		m_palidxs[source] = dest;
}

void PalleteList::resetRemap()
{
	for (size_t i = 0; i < m_palidxs.size(); i++)
		m_palidxs[i] = static_cast<int>(i);
}

bool PalleteList::swapPalMap(std::vector<int>& map)
{
	if (map.size() != m_palidxs.size()) return false;
	std::swap(map, m_palidxs);
	return true;
}

void Sprite::shareCopy(Sprite& sp)
{
	colorPallet = sp.colorPallet;
	pxl = sp.pxl;
	pluginbuf = sp.pluginbuf;
	rct = sp.rct;
	imageGroup = sp.imageGroup;
	imageNumber = sp.imageNumber;
	palidx = sp.palidx;
	rle = sp.rle;
	coldepth = sp.coldepth;
}

void Sprite::copy(Sprite& sp)
{
	shareCopy(sp);
}

std::vector<uint32_t>& Sprite::getPal(PalleteList& pl)
{
	if (!colorPallet.empty() || rle == -12) return colorPallet;
	return pl.get(palidx);
}

bool Sprite::readHeader(File& f)
{
	int16_t x, y;
	if (!f.read(x)) return false; rct.x = x;
	if (!f.read(y)) return false; rct.y = y;
	if (!f.read(imageGroup)) return false;
	if (!f.read(imageNumber)) return false;
	return true;
}

std::wstring Sprite::readPcxHeader(File& f, int64_t offset)
{
	f.seek(offset, Seek::SET);
	uint8_t dummy, encoding, bpp;
	if (!f.read(dummy))    return L"File could not be loaded";
	if (!f.read(dummy))    return L"File could not be loaded";
	if (!f.read(encoding)) return L"File could not be loaded";
	if (!f.read(bpp))      return L"File could not be loaded";
	if (bpp != 8) return L"not 256 colors";

	uint16_t x, y, w, h;
	if (!f.read(x)) return L"File could not be loaded";
	if (!f.read(y)) return L"File could not be loaded";
	if (!f.read(w)) return L"File could not be loaded";
	if (!f.read(h)) return L"File could not be loaded";

	f.seek(offset + 66, Seek::SET);
	uint16_t bpl;
	if (!f.read(bpl)) return L"File could not be loaded";

	rct.w = static_cast<int>(w - x + 1);
	rct.h = static_cast<int>(h - y + 1);
	rle = (encoding == 1) ? static_cast<int>(bpl) : 0;
	return L"";
}

bool Sprite::readHeaderV2(File& f, uint32_t& ofs, uint32_t& siz,
                           uint32_t lofs, uint32_t tofs, uint16_t& idxlnked)
{
	uint16_t w, h;
	int16_t x, y;

	if (!f.read(imageGroup))   return false;
	if (!f.read(imageNumber))  return false;
	if (!f.read(w)) return false; rct.w = static_cast<int>(w);
	if (!f.read(h)) return false; rct.h = static_cast<int>(h);
	if (!f.read(x)) return false; rct.x = x;
	if (!f.read(y)) return false; rct.y = y;

	if (!f.read(idxlnked)) return false;

	uint8_t fmt;
	if (!f.read(fmt))       return false; rle = -static_cast<int>(fmt);
	if (!f.read(coldepth))  return false;
	if (!f.read(ofs))       return false;
	if (!f.read(siz))       return false;

	uint16_t pali, flg;
	if (!f.read(pali)) return false; palidx = static_cast<int32_t>(pali);
	if (!f.read(flg))  return false;

	ofs += ((flg & 1) == 0) ? lofs : tofs;
	return true;
}

void Sprite::setPxl(std::vector<uint8_t>& px)
{
	if (ikemen::config::RenderMode != 0) {
		GlTexture tex;
		tex.load8bitTexture(px, rct.w, rct.h);
	} else {
		pxl = px;
	}
}

void Sprite::rlePcxDecode(std::vector<uint8_t>& px)
{
	if (px.empty() || rle <= 0) return;
	std::vector<uint8_t> rleBuf = px;
	int w = rct.w;
	size_t dstLen = static_cast<size_t>(rct.w) * static_cast<size_t>(rct.h);
	px.assign(dstLen, 0);
	size_t srcLen = rleBuf.size();
	size_t i = 0, j = 0, k = 0;

	while (j < dstLen && i < srcLen) {
		int n = 1;
		uint8_t d = rleBuf[i++];
		if (d >= 0xC0) { n = (d & 0x3F); d = rleBuf[i++]; }
		for (; n > 0; n--) {
			if (j < dstLen) px[j] = d;
			j += (k < static_cast<size_t>(w)) ? 1 : 0;
			if (++k == static_cast<size_t>(rle)) { k = 0; n = 1; }
		}
	}
	rle = 0;
}

void Sprite::rle8Decode(std::vector<uint8_t>& px)
{
	if (px.empty()) return;
	std::vector<uint8_t> rleBuf = px;
	size_t dstLen = static_cast<size_t>(rct.w) * static_cast<size_t>(rct.h);
	if (dstLen == 0) return;
	px.assign(dstLen, 0);
	size_t srcLen = rleBuf.size();
	size_t i = 0, j = 0;
	while (j < dstLen && i < srcLen) {
		int n = 1;
		uint8_t d = rleBuf[i++];
		if (i < srcLen && (d & 0xC0) == 0x40) {
			n = d & 0x3F;
			if (i < srcLen) d = rleBuf[i++];
		}
		for (; n > 0; n--) {
			if (j < dstLen) px[j++] = d;
		}
	}
}

void Sprite::rle5Decode(std::vector<uint8_t>& px)
{
	if (px.empty()) return;
	std::vector<uint8_t> rleBuf = px;
	size_t dstLen = static_cast<size_t>(rct.w) * static_cast<size_t>(rct.h);
	if (dstLen == 0) return;
	px.assign(dstLen, 0);
	size_t srcLen = rleBuf.size();
	size_t i = 0, j = 0;
	while (j < dstLen && i < srcLen) {
		int rl = static_cast<int>(rleBuf[i++]);
		int dl = static_cast<int>(rleBuf[i] & 0x7F);
		uint8_t c = 0;
		if ((rleBuf[i] >> 7) != 0) {
			if (i < srcLen) i++;
			c = rleBuf[i];
		}
		if (i < srcLen) i++;
		for (;;) {
			if (j < dstLen) px[j++] = c;
			rl--;
			if (rl < 0) {
				dl--;
				if (dl < 0) break;
				c = rleBuf[i] & 0x1F;
				rl = static_cast<int>(rleBuf[i] >> 5);
				if (i < srcLen) i++;
			}
		}
	}
}

void Sprite::lz5Decode(std::vector<uint8_t>& px)
{
	if (px.empty()) return;
	std::vector<uint8_t> rleBuf = px;
	size_t dstLen = static_cast<size_t>(rct.w) * static_cast<size_t>(rct.h);
	if (dstLen == 0) return;
	px.assign(dstLen, 0);
	size_t srcLen = rleBuf.size();
	size_t i = 1, j = 0;
	uint8_t ct = rleBuf[0];
	uint32_t s = 0, rbc = 0;
	uint8_t rb = 0;

	while (j < dstLen && i < srcLen) {
		uint32_t d;
		int n;

		if ((ct & (1u << s)) != 0) {
			d = rleBuf[i++];
			if ((d & 0x3F) == 0) {
				d = ((d << 2) | rleBuf[i++]) + 1;
				n = static_cast<int>(rleBuf[i++]) + 2;
			} else {
				rb |= static_cast<uint8_t>((d & 0xC0) >> rbc);
				rbc += 2;
				n = static_cast<int>(d & 0x3F);
				if (rbc < 8) {
					d = static_cast<uint32_t>(rleBuf[i++]) + 1;
				} else {
					d = static_cast<uint32_t>(rb) + 1;
					rbc = 0; rb = 0;
				}
			}
			for (;;) {
				if (j < dstLen && j >= d) px[j] = px[j - d];
				j++;
				n--;
				if (n < 0) break;
			}
		} else {
			d = rleBuf[i++];
			if ((d & 0xE0) == 0) {
				n = static_cast<int>(rleBuf[i++]) + 8;
			} else {
				n = static_cast<int>(d >> 5);
				d &= 0x1F;
			}
			for (; n > 0; n--) {
				if (j < dstLen) px[j] = static_cast<uint8_t>(d);
				j++;
			}
		}
		if (++s >= 8) { s = 0; if (i < srcLen) ct = rleBuf[i++]; }
	}
}

std::wstring Sprite::read(File& f, SffHeader& sh, int64_t offset, uint32_t loh,
                           uint32_t nsh, Sprite* prev, PalleteList& pl, bool c00)
{
	uint32_t subLen = loh;
	if (nsh > static_cast<uint32_t>(offset)) subLen = nsh - static_cast<uint32_t>(offset);

	int8_t ps;
	if (!f.read(ps)) return L"Could not read palette flag";
	bool palletSame = ps != 0;
	if (!prev) palletSame = false;

	std::wstring err = readPcxHeader(f, offset);
	if (!err.empty()) return err;

	f.seek(offset + 128, Seek::SET);
	size_t dataSize = static_cast<size_t>(subLen) - (128 + ((c00 || palletSame) ? 0 : 768));
	pxl.resize(dataSize);
	f.readAry(pxl);

	if (palletSame) {
		if (prev) palidx = prev->palidx;
		if (palidx < 0) pl.newPal(reinterpret_cast<uint32_t&>(palidx));
	} else {
		auto& pal = pl.newPal(reinterpret_cast<uint32_t&>(palidx));
		if (c00) f.seek(offset + static_cast<int64_t>(subLen) - 768, Seek::SET);
		for (int i = 0; i < 256; i++) {
			uint8_t r, g, b;
			if (!f.read(r)) return L"File could not be loaded";
			if (!f.read(g)) return L"File could not be loaded";
			if (!f.read(b)) return L"File could not be loaded";
			// Convert PCX RGB palette to engine internal 0xAABBGGRR format
			// The renderer expects Alpha in the high byte, then B, G, R.
			// Index 0 is transparent background.
			uint32_t a = (i == 0) ? 0 : 0xFF;
			pal[i] = (a << 24) | (static_cast<uint32_t>(r) << 16) |
			         (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
		}
	}

	if (!ikemen::config::SaveMemory || ikemen::config::RenderMode != 0
	 || rct.w * rct.h < (rct.w >= 256 ? (static_cast<int>(pxl.size()) / 256) * rct.w : static_cast<int>(pxl.size())))
	{
		rlePcxDecode(pxl);
	}
	setPxl(pxl);
	return L"";
}

std::wstring Sprite::readV2(File& f, int64_t ofs, uint32_t dsz)
{
	std::vector<uint8_t> px;

	if (rle > 0) {
		return L"";
	} else if (rle == 0) {
		if (dsz == 0) return L"";
		f.seek(ofs, Seek::SET);
		px.resize(static_cast<size_t>(dsz));
		f.readAry(px);
		if (coldepth == 8) {
			rlePcxDecode(px);
			setPxl(px);
		} else if (coldepth == 24 || coldepth == 32) {
			pxl = px;
		} else {
			return L"unknown color depth";
		}
		return L"";
	}

	int format = -rle;
	f.seek(ofs + 4, Seek::SET);

	switch (format) {
	case 2: case 3: case 4:
		if (dsz > 4) {
			px.resize(static_cast<size_t>(dsz) - 4);
			f.readAry(px);
		}
		break;
	case 10: {
		px = ikemen::decodePNG8(rct.w, rct.h, f);
		break;
	}
	case 11: case 12:
		if (ikemen::config::RenderMode != 0) {
			rle = -12;
			pxl.resize(1);
		} else {
			rct.w = rct.h = 0;
		}
		return L"";
	default:
		return L"unknown format";
	}

	switch (format) {
	case 2:  rle8Decode(px);  break;
	case 3:  rle5Decode(px);  break;
	case 4:  lz5Decode(px);   break;
	default: break;
	}
	setPxl(px);
	return L"";
}

} // namespace ikemen