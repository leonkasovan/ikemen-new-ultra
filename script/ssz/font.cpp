#include "font.hpp"
#include "common.hpp"

#include "sszdef.h"
#include "../file.hpp"
#include "../string.hpp"
#include "../math.hpp"
#include "../save/config.hpp"
#include "../alpha/sdlplugin.hpp"

#include <cstring>
#include <cctype>
#include <sstream>

namespace ikemen {

#define FONT_LOG(fmt, ...) do{}while(0)

std::wstring GameFont::loadFile(const std::wstring& filename)
{
	// Detect format by reading the first 12 bytes
	File f;
	if (!f.open(filename, L"rb")) return L"Failed to open file";
	
	char sig[12];
	if (std::fread(sig, 1, 12, f.handle()) != 12) {
		f.close();
		return L"File could not be loaded";
	}
	f.close();

	// Binary ElecbyteFnt format -> V1
	if (memcmp(sig, "ElecbyteFnt", 11) == 0 && sig[11] == 0)
		return loadFontV1(filename);

	// Text-based or SFF-referencing format -> V2
	return loadFontV2(filename);
}

std::wstring GameFont::loadFontV1(const std::wstring& filename)
{
	// Binary "ElecbyteFnt" format:
	//   12 bytes: signature "ElecbyteFnt\0"
	//   2 bytes:  ver (uint16)
	//   2 bytes:  ver2 (uint16)
	//   4 bytes:  pcxDataOffset (uint32)
	//   4 bytes:  pcxDataLength (uint32)
	//   4 bytes:  txtDataOffset (uint32)
	//   4 bytes:  txtDataLength (uint32)
	//   At pcxDataOffset: 128-byte PCX header + RLE data + 768-byte palette
	//   At txtDataOffset: ASCII text with [Info][Def][Map] sections

	File f;
	if (!f.open(filename, L"rb")) return L"Failed to open file";

	char sig[12];
	if (std::fread(sig, 1, 12, f.handle()) != 12) return L"File could not be loaded";
	if (memcmp(sig, "ElecbyteFnt", 11) != 0 || sig[11] != 0)
		return L"Not Elecbyte Font";

	if (!f.read(ver))  return L"File could not be loaded";
	if (!f.read(ver2)) return L"File could not be loaded";

	uint32_t pcxDataOffset = 0, pcxDataLength = 0;
	uint32_t txtDataOffset = 0, txtDataLength = 0;

	if (!f.read(pcxDataOffset)) return L"File could not be loaded";
	if (!f.read(pcxDataLength)) return L"File could not be loaded";
	if (!f.read(txtDataOffset)) return L"File could not be loaded";
	if (!f.read(txtDataLength)) return L"File could not be loaded";

	FONT_LOG("V1 binary font: ver=%u,%u pcxOff=%u pcxLen=%u txtOff=%u txtLen=%u",
	         (unsigned)ver, (unsigned)ver2,
	         (unsigned)pcxDataOffset, (unsigned)pcxDataLength,
	         (unsigned)txtDataOffset, (unsigned)txtDataLength);

	if (pcxDataLength == 0) { f.close(); return L"No PCX data in font"; }
	if (txtDataLength == 0) { f.close(); return L"No map data in font"; }

	// ── Read PCX header for atlas dimensions and bytesPerLine ──────
	// bytesPerLine (at PCX header offset 66) tells how many bytes each
	// scanline occupies in the file, which may include padding/alignment
	// bytes beyond the actual image width. The RLE decoder uses this to
	// skip padding and decode only the visible pixels per scanline.
	uint8_t pcxEncoding, pcxBpp;
	uint16_t pcxBytesPerLine = 0;
	{
		f.seek((int64_t)pcxDataOffset, Seek::SET);
		uint8_t dummy;
		if (!f.read(dummy) || !f.read(dummy)) return L"File could not be loaded";
		if (!f.read(pcxEncoding)) return L"File could not be loaded";
		if (!f.read(pcxBpp)) return L"File could not be loaded";
		if (pcxBpp != 8) return L"not 256 colors";
		uint16_t xmin, ymin, xmax, ymax;
		if (!f.read(xmin) || !f.read(ymin) || !f.read(xmax) || !f.read(ymax))
			return L"File could not be loaded";
		img.rct.w = (int)(xmax - xmin + 1);
		img.rct.h = (int)(ymax - ymin + 1);
		// Read bytesPerLine from PCX header offset 66 (from PCX start)
		f.seek((int64_t)pcxDataOffset + 66, Seek::SET);
		if (!f.read(pcxBytesPerLine)) return L"File could not be loaded";
		FONT_LOG("PCX: atlas=%dx%d bpl=%d encoding=%d",
		         img.rct.w, img.rct.h, pcxBytesPerLine, pcxEncoding);
	}

	// ── Load font atlas (raw compressed data) ──────────────────────
	uint32_t pcxImgSize = pcxDataLength - 128 - 768;
	fontAtlas.assign(pcxImgSize, 0);
	f.seek((int64_t)pcxDataOffset + 128, Seek::SET);
	f.readAry(fontAtlas);
	fontAtlasStride = img.rct.w;

	// ── Read palette ───────────────────────────────────────────────
	std::vector<uint32_t> palette(256, 0);
	f.seek((int64_t)pcxDataOffset + pcxDataLength - 768, Seek::SET);
	for (int i = 0; i < 256; i++) {
		uint8_t r, g, b;
		if (!f.read(r) || !f.read(g) || !f.read(b)) break;
		palette[i] = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
	}

	// ── Read text sections (Info, Def, Map) ────────────────────────
	f.seek((int64_t)txtDataOffset, Seek::SET);
	std::string text(txtDataLength, '\0');
	size_t nread = std::fread(&text[0], 1, txtDataLength, f.handle());
	f.close();
	if (nread != txtDataLength) return L"Failed to read font map data";

	// Debug: dump the raw text section (first 200 chars) for diagnosis
	{
		std::string dump = text.substr(0, 200);
		// Replace non-printable chars for display
		for (auto& chd : dump) { if (chd < 32 && chd != '\n' && chd != '\r' && chd != '\t') chd = '.'; }
		FONT_LOG("Raw text section: %s", dump.c_str());
	}

	// Helper: extract section content between [SectionName] markers
	auto findSection = [&](const std::string& name) -> std::string {
		std::string marker = "[" + name + "]";
		size_t start = text.find(marker);
		if (start == std::string::npos) return "";
		start += marker.size();
		while (start < text.size() && (text[start] == '\n' || text[start] == '\r')) start++;
		size_t end = text.find('[', start);
		if (end == std::string::npos) end = text.size();
		return text.substr(start, end - start);
	};

	auto trim = [](const std::string& s) -> std::string {
		size_t a = s.find_first_not_of(" \t\r\n");
		if (a == std::string::npos) return "";
		size_t b = s.find_last_not_of(" \t\r\n");
		return s.substr(a, b - a + 1);
	};

	// ── Parse metadata section: try [Info] first, fall back to [Def] ───
	{
		std::string meta = findSection("Info");
		std::string metaType = "Info";
		if (meta.empty()) { meta = findSection("Def"); metaType = "Def"; }

		if (!meta.empty()) {
			auto getVal = [&](const std::string& key) -> std::string {
				// Case-insensitive key search: scan every position for a match
				if (key.empty()) return "";
				char firstLow = (char)std::tolower((unsigned char)key[0]);
				char firstUp  = (char)std::toupper((unsigned char)key[0]);
				size_t p = std::string::npos;
				for (size_t si = 0; si < meta.size(); si++) {
					if (meta[si] != firstLow && meta[si] != firstUp) continue;
					bool match = true;
					for (size_t ci = 0; ci < key.size() && si + ci < meta.size(); ci++) {
						if (std::tolower((unsigned char)meta[si + ci]) != std::tolower((unsigned char)key[ci])) {
							match = false; break;
						}
					}
					if (match) { p = si; break; }
				}
				if (p == std::string::npos) return "";
				p += key.size();
				while (p < meta.size() && (meta[p] == ' ' || meta[p] == '=')) p++;
				size_t e = p;
				while (e < meta.size() && meta[e] != '\n' && meta[e] != '\r') e++;
				return meta.substr(p, e - p);
			};

			auto parsePair = [](const std::string& s, int& a, int& b) {
				auto comma = s.find(',');
				if (comma != std::string::npos) {
					a = std::stoi(s.substr(0, comma));
					b = std::stoi(s.substr(comma + 1));
				}
			};

			std::string sz = getVal("size");
			if (!sz.empty()) parsePair(sz, sizex, sizey);

			std::string sp = getVal("spacing");
			if (!sp.empty()) parsePair(sp, spacingx, spacingy);

			std::string ov = getVal("offset");
			if (!ov.empty()) parsePair(ov, offsetx, offsety);

			std::string cl = getVal("colors");
			if (!cl.empty()) colors = std::stoi(cl);

			std::string file = getVal("file");
			if (!file.empty()) fntSff = std::wstring(file.begin(), file.end());

			FONT_LOG("Parsed [%s] section: size=%dx%d spacing=%d,%d colors=%d offset=%d,%d",
			         metaType.c_str(), sizex, sizey, spacingx, spacingy, colors, offsetx, offsety);
		} else {
			FONT_LOG("No [Info] or [Def] section found in text section");
		}
	}

	// ── Parse character map: try [Map] section, fall back to sequential ───
	{
		std::string mapText = findSection("Map");

		if (!mapText.empty()) {
			// Debug: show first 300 chars of [Map] content
			{
				std::string md = mapText.substr(0, 300);
				for (auto& chd : md) { if (chd < 32 && chd != '\n' && chd != '\r' && chd != '\t') chd = '.'; }
				FONT_LOG("[Map] section found (%zu bytes): %s", mapText.size(), md.c_str());
			}

			// Split into lines and parse each
			size_t pos = 0;
			while (pos < mapText.size()) {
				size_t nl = mapText.find('\n', pos);
				std::string line = mapText.substr(pos, nl - pos);
				line = trim(line);
				if (!line.empty() && line[0] != ';') {
					// Remove inline comment
					size_t semi = line.find(';');
					if (semi != std::string::npos) line = line.substr(0, semi);
					line = trim(line);

					size_t eq = line.find('=');
					if (eq != std::string::npos) {
						std::string charStr = trim(line.substr(0, eq));
						std::string valStr = trim(line.substr(eq + 1));

						// Parse character code (hex or literal)
						char16_t ch = 0;
						if (charStr.size() >= 2 && charStr[0] == '0' &&
						    (charStr[1] == 'x' || charStr[1] == 'X')) {
							ch = (char16_t)std::stoul(charStr, nullptr, 16);
						} else if (!charStr.empty()) {
							ch = (char16_t)charStr[0];
						}

						// Parse offset and width
						int ofs = 0, w = sizex;
						auto comma = valStr.find(',');
						if (comma != std::string::npos) {
							ofs = std::stoi(valStr.substr(0, comma));
							w = std::stoi(valStr.substr(comma + 1));
						} else if (!valStr.empty()) {
							ofs = std::stoi(valStr);
						}

						mpl.emplace_back();
						CharOffset& co = mpl.back();
						co.ofs = ofs;
						co.w = w;
						map.set(ch, co);
					} else {
						// No `=` on this line — try Format 2: space-separated "char offset width"
						// (most common Elecbyte font format)
						{
							std::istringstream iss(line);
							std::string chStr;
							int spOfs = 0, spW = 0;
							if (iss >> chStr >> spOfs >> spW && spW > 0) {
								char16_t ch = 0;
								if (chStr.size() == 1) {
									ch = (char16_t)(unsigned char)chStr[0];
								} else if (chStr.size() >= 2 && chStr[0] == '0' &&
								           (chStr[1] == 'x' || chStr[1] == 'X')) {
									ch = (char16_t)std::stoul(chStr, nullptr, 16);
								}
								if (ch > 0) {
									mpl.emplace_back();
									CharOffset& co = mpl.back();
									co.ofs = spOfs;
									co.w = spW;
									map.set(ch, co);
								}
							}
						}
					}
				}
				if (nl == std::string::npos) break;
				pos = nl + 1;
			}

			if (mpl.size() >= 5) {
				FONT_LOG("[Map] space-separated parser found %zu chars", mpl.size());
			}
		}

		// ── Fallback: generate sequential character entries ─────────
		// If the [Map] section was missing, empty, or produced very few
		// characters (< 5), generate characters for printable ASCII range
		// 0x21-0x7E laid out sequentially in the atlas at intervals of sizex.
		// This handles fonts that don't have [Map] sections or use a
		// format our parser doesn't recognize (e.g. space-separated lists).
		if (mpl.size() < 5 && sizex > 0 && img.rct.w > 0) {
			int numCols = img.rct.w / sizex;
			int numChars = (numCols > 94) ? 94 : numCols;
			if (numChars > (int)mpl.size()) {
				size_t oldCount = mpl.size();
				// Clear whatever the [Map] section parsed — it was incomplete
				mpl.clear();
				map.clear();
				FONT_LOG("Generating %d sequential chars (0x21-0x%x) from atlas (was %zu from [Map])",
				         numChars, 0x21 + numChars - 1, oldCount);
				for (int ci = 0; ci < numChars; ci++) {
					char16_t ch = (char16_t)(0x21 + ci);
					int ofs = ci * sizex;
					int w = sizex;
					mpl.emplace_back();
					CharOffset& co = mpl.back();
					co.ofs = ofs;
					co.w = w;
					map.set(ch, co);
				}
			}
		}
	}

	// ── Decode PCX atlas and extract character sprites ─────────────────
	// The font atlas is stored as RLE-compressed PCX image data.
	// We decode it to raw pixels, then for each character in mpl,
	// extract its pixel data and create a Sprite.

	if (img.rct.w > 0 && img.rct.h > 0 && !fontAtlas.empty() && !mpl.empty()) {
		// RLE decode the font atlas with PCX scanline padding awareness.
		// Uses bytesPerLine from the PCX header (pcxBytesPerLine) so that
		// padding/alignment bytes at the end of each scanline are consumed
		// from the RLE stream but not written to the decoded output.
		// This matches the reference decoder in test_font.cpp::decodePcxRle().
		int bpl = (pcxBytesPerLine > 0) ? (int)pcxBytesPerLine : img.rct.w;
		std::vector<uint8_t> decodedAtlas(static_cast<size_t>(img.rct.w) * img.rct.h, 0);
		size_t srcPos = 0;
		size_t dstIdx = 0;  // index into decodedAtlas (only advanced within width)
		size_t col     = 0;  // column within current scanline
		while (dstIdx < decodedAtlas.size() && srcPos < fontAtlas.size()) {
			int n = 1;
			uint8_t d = fontAtlas[srcPos++];
			// PCX RLE marker: top 2 bits set (byte >= 0xC0)
			if (d >= 0xC0) { n = (int)(d & 0x3F); d = (srcPos < fontAtlas.size()) ? fontAtlas[srcPos++] : 0; }
			for (; n > 0 && dstIdx < decodedAtlas.size(); n--) {
				if ((int)col < img.rct.w) {
					decodedAtlas[dstIdx] = d;
					dstIdx++;
				}
				col++;
				if ((int)col >= bpl) { col = 0; break; }  // next scanline, break run if needed
			}
		}

		// Convert palette from RGB to 0xAABBGGRR format
		// Index 0 is transparent (palette[0] = background color, alpha=0)
		std::vector<uint32_t> spPalette(256, 0);
		for (int pi = 0; pi < 256; pi++) {
			// Read RGB from palette stored earlier
			uint32_t rawPal = palette[pi];
			uint8_t pr = (uint8_t)(rawPal >> 16);
			uint8_t pg = (uint8_t)(rawPal >> 8);
			uint8_t pb = (uint8_t)(rawPal);
			uint32_t a = (pi == 0) ? 0 : 0xFF;
			spPalette[pi] = (a << 24) | ((uint32_t)pr << 16) | ((uint32_t)pg << 8) | (uint32_t)pb;
		}

		// Extract each character's pixels from the decoded atlas
		for (auto& co : mpl) {
			if (co.w <= 0) continue;
			int srcX = co.ofs;
			int charW = co.w;
			int charH = sizey;
			if (srcX + charW > img.rct.w || charH > img.rct.h) continue;

			co.img.rct = {0, 0, charW, charH};
			co.img.rle = 0;  // uncompressed pixel data
			co.img.colorPallet = spPalette;

			std::vector<uint8_t> charPixels(static_cast<size_t>(charW) * charH, 0);
			for (int row = 0; row < charH; row++) {
				for (int col = 0; col < charW; col++) {
					size_t srcIdx = static_cast<size_t>(row) * fontAtlasStride + (srcX + col);
					if (srcIdx < decodedAtlas.size()) {
						charPixels[static_cast<size_t>(row) * charW + col] = decodedAtlas[srcIdx];
					}
				}
			}
			co.img.pxl = std::move(charPixels);
		}
	}

	FONT_LOG("Loaded font: %dx%d spacing=%d,%d colors=%d, %zu chars, atlas=%dx%d",
	         sizex, sizey, spacingx, spacingy, colors, mpl.size(),
	         img.rct.w, img.rct.h);

	return L"";
}

std::wstring GameFont::loadFontV2(const std::wstring& filename)
{
	bool unicode = false;
	auto text = ikemen::loadText(filename, unicode);
	if (text.empty()) return L"File could not be loaded or is binary";

	auto lines = ikemen::splitLines(text);
	if (lines.empty()) return L"File could not be loaded";
	if (lines[0].find(L'[') == std::wstring::npos)
		return L"Not a recognized font format";

	bool defflg = true;
	size_t i = 0;

	while (i < lines.size()) {
		auto line = ikemen::trim(lines[i]);
		if (line.empty() || line[0] == L';') { i++; continue; }

		if (line[0] == L'[') {
			std::wstring sec = line;
			size_t f = sec.find(L';');
			if (f != std::wstring::npos) sec = sec.substr(0, f);
			sec = ikemen::toLower(ikemen::trim(sec));
			sec = sec.substr(1, sec.find(L']') - 1);

			i++;
			Section sc;
			sc.parse(lines, i);

			std::wstring data;

			if (sec == L"info" || (sec == L"def" && defflg)) {
				if (!(data = sc.get(L"type")).empty()) fntType = data;
				if (!(data = sc.get(L"size")).empty()) {
					auto comma = data.find(L',');
					if (comma != std::wstring::npos) {
						sizex = std::stoi(data.substr(0, comma));
						sizey = std::stoi(data.substr(comma + 1));
					}
				}
				if (!(data = sc.get(L"spacing")).empty()) {
					auto comma = data.find(L',');
					if (comma != std::wstring::npos) {
						spacingx = std::stoi(data.substr(0, comma));
						spacingy = std::stoi(data.substr(comma + 1));
					}
				}
				if (!(data = sc.get(L"offset")).empty()) {
					auto comma = data.find(L',');
					if (comma != std::wstring::npos) {
						offsetx = std::stoi(data.substr(0, comma));
						offsety = std::stoi(data.substr(comma + 1));
					}
				}
				if (!(data = sc.get(L"colors")).empty()) colors = std::stoi(data);
				if (!(data = sc.get(L"file")).empty()) fntSff = data;
				if (sec == L"def") defflg = false;
			}
		}
		i++;
	}

	if (!fntSff.empty())
		FONT_LOG("V2 font references SFF: %ls", fntSff.c_str());

	FONT_LOG("V2 text font: %dx%d spacing=%d,%d colors=%d file=%ls",
	         sizex, sizey, spacingx, spacingy, colors, fntSff.c_str());
	return L"";
}

// ── Text rendering ────────────────────────────────────────────────────

void GameFont::drawText(float x, float y, float xscl, float yscl, int bank,
                         int salpha, int dalpha, const Rect& window, int align,
                         const std::string& text)
{
	LOG_DEBUG("FONT", "drawText: text='%s' len=%zu x=%.0f y=%.0f xscl=%.2f yscl=%.2f align=%d",
	          text.c_str(), text.size(), x, y, xscl, yscl, align);
	// Calculate total width for alignment
	float totalWidth = textWidth(text) * xscl;
	float startX = x;
	if (align == 1) startX = x - totalWidth / 2.0f;  // center
	else if (align == 2) startX = x - totalWidth;     // right

	float cursorX = startX;
	(void)bank; // bank remapping not implemented in this renderer
	(void)window; // clipping not implemented yet

	for (size_t i = 0; i < text.size(); i++) {
		unsigned char c = (unsigned char)text[i];
		CharOffset* co = map.getPtr((char16_t)c);
		if (!co && c < mpl.size()) {
			co = &mpl[c];
		}

		if (co && co->img.rct.w > 0 && co->img.rct.h > 0) {
			LOG_DEBUG("FONT", "drawText char[%zu]='%c'(0x%02X) co=%p w=%d h=%d pxl.len=%zu pal.len=%zu",
			          i, c, c, co, co->img.rct.w, co->img.rct.h, co->img.pxl.size(), co->img.colorPallet.size());
			float charX = cursorX + (co->ofs + offsetx) * xscl;
			float charY = y + offsety * yscl;
			Rect dr = {(int)charX, (int)charY,
			           (int)(co->img.rct.w * xscl), (int)(co->img.rct.h * yscl)};
			Rect sr = {0, 0, co->img.rct.w, co->img.rct.h};
			Rect tileRect = {0, 0, 0, 0};
			std::vector<int8_t> bufCopy = co->img.pluginbuf;
			// TEMP: skip renderMugenZoom to test
			//renderMugenZoom(dr, 0, 0, co->img.pxl, co->img.colorPallet,
			//                (int16_t)0, sr, 0, 0, tileRect,
			//                1.0f, 1.0f, 1.0f, 1.0f,
			//                0, salpha, co->img.rle, bufCopy);
		} else {
			LOG_DEBUG("FONT", "drawText char[%zu]='%c'(0x%02X) co=%p SKIPPED (no glyph)", i, c, c, co);
		}
		if (co) cursorX += (float)co->w * xscl;
		else    cursorX += (float)(sizex + spacingx) * xscl;
	}
	LOG_DEBUG("FONT", "drawText: done rendering %zu chars", text.size());
}

float GameFont::textWidth(const std::string& text) const
{
	float width = 0.0f;
	for (size_t i = 0; i < text.size(); i++) {
		unsigned char c = (unsigned char)text[i];
		const CharOffset* co = map.getPtr((char16_t)c);
		if (co) {
			width += (float)co->w;
		} else if (c < mpl.size()) {
			width += (float)mpl[c].w;
		} else {
			width += (float)(sizex + spacingx);
		}
	}
	return width;
}

} // namespace ikemen
