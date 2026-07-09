// font_service.cpp — Real implementations for ssz_script/ssz/font.ssz
//
// Implements:
//   FontData::loadFile() / loadFontV1() / loadFontV2() — FNT file loaders
//   FontData::charWidth() / textWidth() / getCharSpr() — character metrics
//   FontData::drawChar() — per-character sprite rendering via renderMugenZoom
//   FontData::drawText() — batch text rendering via renderFontBatch
//   Module-level font_init() / font_get_state() / font_render_text()

#include "font_service.hpp"
#include "file_service.hpp"
#include "common_service.hpp"
#include "sdlplugin_service.hpp"
#include "sff_service.hpp"
#include "ssz_trace.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace ikemen::ssz_native {

// =========================================================================
// Section parsing helpers (SSZ-equivalent to .com.Section / .com.sectionName)
// =========================================================================
namespace {

// Extract section name from a [SectionName] line.
// Returns empty string if not a valid section header.
std::string section_name(const std::string& line) {
	if (line.empty() || line[0] != '[') return {};
	auto end = line.find(']');
	if (end == std::string::npos) return {};
	return line.substr(1, end - 1);
}

// Simple Section parser that reads key=value pairs from a block of lines.
// SSZ equivalent: &.com.Section sc.parse(lines, i=)
struct SimpleSection {
	// Stores keys and values parsed from the section
	std::vector<std::pair<std::string, std::string>> entries;

	// Parse key=value pairs starting from lines[i] until the next section
	// header (starts with '[') or end of lines.
	void parse(const std::vector<std::string>& lines, int& i) {
		while (i < static_cast<int>(lines.size())) {
			const std::string& raw = lines[i];
			if (raw.empty() || raw[0] == '[') break;

			// Strip inline comment
			std::string line = raw;
			auto cmt = line.find(';');
			if (cmt != std::string::npos) line = line.substr(0, cmt);

			// Trim
			auto start = line.find_first_not_of(" \t\r\n");
			if (start == std::string::npos) { i++; continue; }
			auto end = line.find_last_not_of(" \t\r\n");
			line = line.substr(start, end - start + 1);

			// Find '='
			auto eq = line.find('=');
			if (eq != std::string::npos) {
				std::string key = line.substr(0, eq);
				// Trim key
				auto ks = key.find_first_not_of(" \t");
				if (ks != std::string::npos) {
					auto ke = key.find_last_not_of(" \t");
					key = key.substr(ks, ke - ks + 1);
				}
				std::string val = line.substr(eq + 1);
				// Trim val
				auto vs = val.find_first_not_of(" \t");
				if (vs != std::string::npos) {
					auto ve = val.find_last_not_of(" \t");
					val = val.substr(vs, ve - vs + 1);
				}
				entries.emplace_back(key, val);
			}
			i++;
		}
	}

	// Get value for a key (case-insensitive comparison).
	std::string get(const std::string& key) const {
		for (const auto& e : entries) {
			if (e.first.size() == key.size() &&
				std::equal(e.first.begin(), e.first.end(), key.begin(),
					[](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b)); }))
			{
				return e.second;
			}
		}
		return {};
	}
};

// Parse a comma/space/tab-separated pair of ints (SSZ .com.readPair equivalent).
// Returns true if both values were parsed.
bool read_pair(int& v1, int& v2, const std::string& data) {
	if (data.empty()) return false;
	std::string s = data;
	// Find a separator (comma, tab, or space)
	auto pos = s.find(',');
	if (pos == std::string::npos) {
		pos = s.find('\t');
	}
	if (pos == std::string::npos) {
		pos = s.find(' ');
	}
	if (pos == std::string::npos) {
		v1 = common_atoi(s);
		v2 = v1;
		return true;
	}
	v1 = common_atoi(s.substr(0, pos));
	v2 = common_atoi(s.substr(pos + 1));
	return true;
}

// Hex character to int (SSZ: .m.inRange check + arithmetic)
int hex_char_to_int(char c) {
	if (c >= '0' && c <= '9') return c - '0';
	c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	return 0;
}

// Compute alpha from salpha/dalpha/brightness (SSZ branch logic)
int compute_alpha(int salpha, int dalpha, int brightness) {
	if ((salpha == 1 && dalpha >= 255) || salpha == -2) return -2;
	if (brightness < 255) return 255; // fallback
	if (salpha + dalpha >= 510) return -1;
	if (salpha + dalpha == 255 || salpha + dalpha == 256 || salpha < 0)
		return salpha;
	return (brightness * salpha >> 8) | (dalpha << 10) | (1 << 9);
}

// Copy a rectangle from a source pixel buffer into a destination buffer.
// SSZ equivalent: copyCharRect(ubyte dst, int dw, ^ubyte src, int x, int w, int h)
void copy_char_rect(std::vector<uint8_t>& dst, int dw,
	const std::vector<uint8_t>& src, int x, int w_src, int h)
{
	for (int i = 0; i < h; i++) {
		int src_offset = w_src * i + x;
		int dst_offset = dw * i;
		int copy_count = std::min(dw, w_src - x);
		if (copy_count <= 0) break;
		if (dst_offset + copy_count > static_cast<int>(dst.size())) {
			copy_count = static_cast<int>(dst.size()) - dst_offset;
		}
		if (src_offset + copy_count > static_cast<int>(src.size())) {
			copy_count = static_cast<int>(src.size()) - src_offset;
		}
		if (copy_count > 0) {
			std::memcpy(&dst[dst_offset], &src[src_offset],
				static_cast<size_t>(copy_count));
		}
	}
}

} // anonymous namespace

// =========================================================================
// FontData::loadFile
// =========================================================================
// SSZ equivalent:
//   if(.s.equ(.s.toLower(filename[#filename-4 .. -1]), ".fnt"))
//     ret `loadFontV1(filename);
//   ret `loadFontV2(filename);

std::string FontData::loadFile(const std::string& filename) {
	SSZ_TRACE_CAT(TRACE_SYS, "FontData::loadFile");
	if (filename.size() >= 4) {
		std::string ext = filename.substr(filename.size() - 4);
		// Convert to lowercase for comparison
		for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		if (ext == ".fnt") return loadFontV1(filename);
	}
	return loadFontV2(filename);
}

// =========================================================================
// FontData::loadFontV1 — ElecbyteFnt binary format parser
// =========================================================================
// SSZ equivalent (font.ssz loadFontV1, ~120 lines):
//   Reads 12-byte "ElecbyteFnt\0" signature
//   Reads version, pcx/txt offsets+lengths
//   Reads PCX header, RLE-decoded pixel data, 768-byte RGB palette
//   Parses text section with regex-based character map
//   Creates per-character sprites with palette banks

std::string FontData::loadFontV1(const std::string& filename) {
	SSZ_TRACE_CAT(TRACE_SYS, "FontData::loadFontV1");

	FileHandle f;
	if (!f.open(std::wstring(filename.begin(), filename.end()), L"rb"))
		return "File open error";

	// ── Read 12-byte signature "ElecbyteFnt\0" ──
	char sig[12];
	uint8_t ub;
	for (int i = 0; i < 12; i++) {
		if (!f.read(&ub, 1)) return "File read error";
		sig[i] = static_cast<char>(ub);
	}
	if (std::memcmp(sig, "ElecbyteFnt", 12) != 0)
		return "Unrecognized FNT file";

	// ── Read version + offsets ──
	uint16_t ver_val, ver2_val;
	if (!f.read(&ver_val, 2)) return "File read error";
	if (!f.read(&ver2_val, 2)) return "File read error";
	ver = ver_val;
	ver2 = ver2_val;

	uint32_t pcxDataOffset, pcxDataLength;
	uint32_t txtDataOffset, txtDataLength;
	if (!f.read(&pcxDataOffset, 4)) return "File read error";
	if (!f.read(&pcxDataLength, 4)) return "File read error";
	if (!f.read(&txtDataOffset, 4)) return "File read error";
	if (!f.read(&txtDataLength, 4)) return "File read error";

	// ── Read PCX header via SpriteData ──
	std::string error = img.readPcxHeader(f, static_cast<int64_t>(pcxDataOffset));
	if (!error.empty()) return error;

	// ── Read pixel data (PCX data minus 128-byte header minus 768-byte palette) ──
	int64_t pxOffset = static_cast<int64_t>(pcxDataOffset) + 128;
	f.seek(pxOffset, SeekOrigin::Set);

	int pxSize = static_cast<int>(pcxDataLength) - 128 - 768;
	if (pxSize < 0) return "Invalid FNT file: PCX data too small";
	std::vector<uint8_t> px(static_cast<size_t>(pxSize));
	f.read_array(px.data(), 1, pxSize);

	// ── Read 768-byte palette (256 RGB entries) ──
	img.colorPallet.resize(256);
	for (int i = 0; i < 256; i++) {
		uint8_t r, g, b;
		if (!f.read(&r, 1)) return "File read error";
		if (!f.read(&g, 1)) return "File read error";
		if (!f.read(&b, 1)) return "File read error";
		img.colorPallet[i] = (static_cast<uint32_t>(r) << 16)
			| (static_cast<uint32_t>(g) << 8)
			| static_cast<uint32_t>(b);
	}

	// ── RLE decode PCX pixel data ──
	img.rlePcxDecode(px);
	fontAtlas = px;
	fontAtlasStride = static_cast<int>(img.rct_w);

	// ── Read text section ──
	f.seek(static_cast<int64_t>(txtDataOffset), SeekOrigin::Set);
	std::string buf;
	buf.reserve(txtDataLength);
	for (uint32_t i = 0; i < txtDataLength; i++) {
		if (!f.read(&ub, 1)) return "File read error";
		buf += static_cast<char>(ub);
	}

	// ── Parse text section (split lines, trim) ──
	std::vector<std::string> lines = common_split_lines(buf);
	for (auto& l : lines) {
		// Trim each line
		auto s = l.find_first_not_of(" \t\r\n");
		if (s != std::string::npos) {
			auto e = l.find_last_not_of(" \t\r\n");
			l = l.substr(s, e - s + 1);
		} else {
			l.clear();
		}
	}

	// Process sections and character map.
	// Note: after each section handler, we do i-- to compensate for the
	// outer for loop's i++, because the handlers already advanced i to
	// point at the next [header] (or past end).
	bool mapflg = true;
	for (int i = 0; i < static_cast<int>(lines.size()); i++) {
		const std::string& raw = lines[i];
		if (raw.empty() || raw[0] != '[') continue;

		std::string sec = section_name(raw);
		if (sec.empty()) continue;

		i++;
		if (sec == "map" || sec == "Map" || sec == "MAP") {
			if (mapflg) {
				// ── Parse character map ──
				// SSZ regex: "(\\S+)(?:\\s+(\\S+)(?:\\s+(\\S+))?)?"
				// Matches: char_code [offset] [width]
				// Implemented via manual whitespace tokenization.

				while (i < static_cast<int>(lines.size())) {
					const std::string& line = lines[i];
					if (line.empty() || line[0] == '[') {
						i--;
						break;
					}

					// Strip inline comment
					std::string clean = line;
					auto cmt = line.find(';');
					if (cmt != std::string::npos)
						clean = line.substr(0, cmt);
					// Trim
					auto s = clean.find_first_not_of(" \t");
					if (s == std::string::npos) { i++; continue; }
					auto e = clean.find_last_not_of(" \t\r\n");
					clean = clean.substr(s, e - s + 1);
					if (clean.empty()) { i++; continue; }

					// Split by whitespace (regex equivalent)
					std::vector<std::string> tokens;
					size_t pos = 0;
					while (pos < clean.size()) {
						while (pos < clean.size() && (clean[pos] == ' ' || clean[pos] == '\t'))
							pos++;
						if (pos >= clean.size()) break;
						size_t tok_start = pos;
						while (pos < clean.size() && clean[pos] != ' ' && clean[pos] != '\t')
							pos++;
						tokens.push_back(clean.substr(tok_start, pos - tok_start));
					}

					if (tokens.empty()) { i++; continue; }

					// Parse character code from tokens[0]
					char c = 0;
					const std::string& code = tokens[0];
					if (code.size() >= 2 && code[0] == '0' &&
						(code[1] == 'x' || code[1] == 'X')) {
						// Hex: 0xNN
						for (size_t j = 2; j < code.size(); j++) {
							c = static_cast<char>(
								(static_cast<int>(c) << 4) + hex_char_to_int(code[j]));
						}
					} else {
						// Direct char (take first char)
						c = code[0];
					}

					// Parse optional offset (tokens[1])
					int ofs = 0;
					if (tokens.size() > 1) {
						ofs = common_atoi(tokens[1]);
					}

					// Add character to map
					CharOffset co;
					co.ofs = ofs;
					if (tokens.size() > 2) {
						co.w = common_atoi(tokens[2]);
						ofs += co.w - sizex;
					} else {
						co.w = sizex;
					}

					mpl.push_back(std::move(co));
					map[static_cast<char>(c)] = static_cast<int>(mpl.size()) - 1;

					ofs += sizex;
					i++;
				}
				mapflg = false;
			}
			// Compensate for outer for loop i++ which would skip the next
			// section header. The handler already advanced i to point at
			// the next [header] line (or past end).
			i--;
		} else if (sec == "def" || sec == "Def" || sec == "DEF") {
			// Parse def section
			SimpleSection ss;
			ss.parse(lines, i);

			std::string data;
			data = ss.get("size");
			if (!data.empty()) read_pair(sizex, sizey, data);

			data = ss.get("spacing");
			if (!data.empty()) read_pair(spacingx, spacingy, data);

			data = ss.get("colors");
			if (!data.empty()) {
				colors = std::min(255, common_atoi(data));
			}

			data = ss.get("offset");
			if (!data.empty()) read_pair(offsetx, offsety, data);

			// Compensate for outer for loop i++ which would skip the next
			// section header. ss.parse() left i pointing at the next [header].
			i--;
		}
		// Other sections: skip (i stays as-is, for loop i++ skips)
	}

	// ── Create palette banks ──
	// SSZ: pal.new(255 / colors);
	int numPals = (colors > 0) ? (255 / colors) : 0;
	if (numPals < 1) numPals = 1;
	// Ensure mpl has pal entries before this step
	if (numPals > 0 && !mpl.empty()) {
		// Build palette banks
		struct PalBank { std::vector<uint32_t> colors; };
		std::vector<PalBank> palBanks(static_cast<size_t>(numPals));

		for (int p = 0; p < numPals; p++) {
			palBanks[p].colors.resize(256);

			// Copy first (256 - colors) entries from original
			int keep = 256 - colors;
			for (int k = 0; k < keep && k < 256; k++) {
				palBanks[p].colors[k] = img.colorPallet[k];
			}

			// Copy shifted entries
			int shiftStart = 256 - colors;
			int srcStart = 256 - colors * (p + 1);
			if (srcStart < 0) srcStart = 0;
			for (int k = shiftStart; k < 256; k++) {
				int srcIdx = srcStart + (k - shiftStart);
				if (srcIdx < static_cast<int>(img.colorPallet.size()) && srcIdx >= 0)
					palBanks[p].colors[k] = img.colorPallet[srcIdx];
				else
					palBanks[p].colors[k] = 0;
			}
		}

		// ── Create per-character sprites with palette banks ──
		int atlasW = static_cast<int>(img.rct_w);
		int atlasH = static_cast<int>(img.rct_h);

		for (size_t ci = 0; ci < mpl.size(); ci++) {
			mpl[ci].img.resize(static_cast<size_t>(numPals));
			for (int p = 0; p < numPals; p++) {
				SpriteData& sp = mpl[ci].img[p];
				if (p == 0) {
					sp.shareCopy(img);
					sp.rct_w = static_cast<int32_t>(mpl[ci].w);
					sp.rct_x = 0;
					sp.rct_y = 0;

					// Copy pixel rect from atlas
					int charW = mpl[ci].w;
					int charH = atlasH;
					if (charW > 0 && charH > 0) {
						std::vector<uint8_t> px2(
							static_cast<size_t>(charW) * static_cast<size_t>(charH), 0);
						copy_char_rect(px2, charW, fontAtlas, mpl[ci].ofs,
							atlasW, charH);
						sp.setPxl(std::move(px2));
					}
				} else {
					sp.shareCopy(mpl[ci].img[0]);
					sp.rct_w = static_cast<int32_t>(mpl[ci].w);
				}
				sp.colorPallet = palBanks[p].colors;
			}
		}
	}

	return {};
}

// =========================================================================
// FontData::loadFontV2 — SFF-based sprite font loader
// =========================================================================
// SSZ equivalent (font.ssz loadFontV2):
//   Loads a .def-style text config file
//   Parses [def] section for type, size, spacing, offset, file
//   Loads the SFF file from "font/" + fntSff

std::string FontData::loadFontV2(const std::string& filename) {
	SSZ_TRACE_CAT(TRACE_SYS, "FontData::loadFontV2");

	// Load the .def file
	bool unicode = false;
	std::string mainbuf = common_load_text(filename, unicode);
	if (mainbuf.empty())
		return "Cannot load file:" + filename;

	// Split into lines and trim
	std::vector<std::string> lines = common_split_lines(mainbuf);
	for (auto& l : lines) {
		auto s = l.find_first_not_of(" \t\r\n");
		if (s != std::string::npos) {
			auto e = l.find_last_not_of(" \t\r\n");
			l = l.substr(s, e - s + 1);
		} else {
			l.clear();
		}
	}

	// Parse sections
	// Note: after the def handler, we do i-- to compensate for the
	// outer for loop's i++, because ss.parse() already advanced i
	// to point at the next [header] (or past end).
	bool defflg = true;
	for (int i = 0; i < static_cast<int>(lines.size()); i++) {
		const std::string& raw = lines[i];
		if (raw.empty() || raw[0] != '[') continue;

		std::string sec = section_name(raw);
		if (sec.empty()) continue;

		i++;
		if (sec == "def" || sec == "Def" || sec == "DEF") {
			if (defflg) {
				SimpleSection ss;
				ss.parse(lines, i);

				std::string data;

				data = ss.get("type");
				if (!data.empty()) fntType = data;

				data = ss.get("size");
				if (!data.empty()) read_pair(sizex, sizey, data);

				data = ss.get("spacing");
				if (!data.empty()) read_pair(spacingx, spacingy, data);

				data = ss.get("offset");
				if (!data.empty()) read_pair(offsetx, offsety, data);

				data = ss.get("file");
				if (!data.empty()) fntSff = data;
			}
			defflg = false;
			// Compensate for outer for loop i++ which would skip the next
			// section header. ss.parse() left i pointing at the next [header].
			i--;
		}
		// Other sections: skip (i stays as-is, for loop i++ skips)
	}

	// Load the SFF font file from "font/" + fntSff
	if (fntSff.empty())
		return "No font file specified";

	std::string sffPath = "font/" + fntSff;

	// Create an SffData to load the sprite font
	SffData sf;
	sf.init();
	std::string error = sf.loadFile(sffPath, false);
	if (!error.empty())
		return error;

	// Transfer sprites from the loaded SFF to our internal map
	// FNT v2 fonts typically use a naming convention: each character is
	// a sprite with group=0, number=character_code.
	// We create CharOffset entries for each sprite found.
	for (const auto& [key, sprite] : sf.spriteTable) {
		short group = static_cast<short>(key >> 16);
		short number = static_cast<short>(key & 0xFFFF);
		if (group == 0) {
			char ch = static_cast<char>(number);
			if (ch == 0) continue; // Skip sprite 0,0

			CharOffset co;
			co.ofs = 0;
			co.w = static_cast<int>(sprite.rct_w);

			// Create a sprite copy with 1 palette bank
			co.img.emplace_back();
			co.img.back().shareCopy(sprite);
			co.img.back().rct_x = 0;
			co.img.back().rct_y = 0;
			co.img.back().colorPallet = sprite.colorPallet;

			mpl.push_back(std::move(co));
			map[ch] = static_cast<int>(mpl.size()) - 1;
		}
	}

	// Also handle single-sprite-case: if the SFF has one sprite at (0,0),
	// it might be the entire atlas and map entries describe sub-rects.
	// For now, rely on SFF sprites by character code.

	return {};
}

// =========================================================================
// FontData::charWidth
// =========================================================================
// SSZ equivalent:
//   if(ch == ' ') ret `sizex;
//   ^&`CharOffset m = `map.get(ch);
//   if(#m == 0) ret 0;
//   ret m~w;

int FontData::charWidth(char ch) const {
	if (ch == ' ') return sizex;
	auto it = map.find(ch);
	if (it == map.end()) return 0;
	if (it->second < 0 || it->second >= static_cast<int>(mpl.size())) return 0;
	return mpl[it->second].w;
}

// =========================================================================
// FontData::textWidth
// =========================================================================
// SSZ equivalent:
//   int w = 0;
//   txt:<-[void(c){w += `charWidth(c) + `spacingx;}];
//   ret w;

int FontData::textWidth(const std::string& txt) const {
	int w = 0;
	for (char c : txt) {
		w += charWidth(c) + spacingx;
	}
	return w;
}

// =========================================================================
// FontData::getCharSpr
// =========================================================================
// SSZ equivalent:
//   ^&`CharOffset m = `map.get(ch);
//   if(#m == 0) ret .consts.null!&.sff.Sprite?();
//   ret m~img[bank..bank+1];

const SpriteData* FontData::getCharSpr(int bank, char ch) const {
	auto it = map.find(ch);
	if (it == map.end()) return nullptr;
	if (it->second < 0 || it->second >= static_cast<int>(mpl.size())) return nullptr;
	const CharOffset& co = mpl[it->second];
	if (bank < 0 || bank >= static_cast<int>(co.img.size())) return nullptr;
	return &co.img[bank];
}

// =========================================================================
// FontData::drawChar
// =========================================================================
// SSZ equivalent (font.ssz drawChar):
//   if(ch == ' ') ret (float)`sizex * xscl;
//   ^&.sff.Sprite spr = `getCharSpr(bank, ch);
//   if(#spr == 0 || #spr~pxl == 0) ret 0.0;
//   tile.set(0,0,0,0);
//   int alpha = [computed from salpha/dalpha/brightness];
//   RenderMugenGl(spr~pxl<>, pal, 0, spr~rct=, -x*WidthScale, -y*HeightScale,
//                  tile=, xscl*WidthScale, xscl*WidthScale, yscl*HeightScale,
//                  1.0, 0.0, 0.0, alpha, window=, 0.0, 0.0);
//   ret spr~rct.w * xscl;

float FontData::drawChar(float x, float y, float xscl, float yscl,
	int bank, int salpha, int dalpha, const SdlRect& window,
	char ch, std::vector<uint32_t>& pal)
{
	// Space: return cell width * scale
	if (ch == ' ') return static_cast<float>(sizex) * xscl;

	const SpriteData* spr = getCharSpr(bank, ch);
	if (!spr || spr->pxl.empty()) return 0.0f;

	// Build tile rect (zero-size: no tiling)
	SdlRect tile;
	tile.set(0, 0, 0, 0);

	// Compute alpha
	const auto& com = common_get_state();
	int alpha = compute_alpha(salpha, dalpha, com.brightness);

	// Build source rect from sprite dimensions
	SdlRect src_rect;
	src_rect.set(spr->rct_x, spr->rct_y, spr->rct_w, spr->rct_h);

	// Local plugin buffer
	std::vector<int8_t> pluginbuf;
	pluginbuf.reserve(256);

	// Use the palette if provided, else sprite's own palette
	const std::vector<uint32_t>& palRef = (!pal.empty()) ? pal : spr->colorPallet;

	// Call renderMugenZoom (unified render path — matches SSZ's renderMugenZoom)
	renderMugenZoom(
		window,                                                     // dr
		0.0f, 0.0f,                                                 // rcx, rcy
		spr->pxl,                                                    // pxl
		palRef,                                                      // pal
		0,                                                           // ckey
		src_rect,                                                    // sr
		-x * com.WidthScale,                                         // cx
		-y * com.HeightScale,                                        // ty
		tile,                                                        // tile
		xscl * com.WidthScale,                                       // xtopscl
		xscl * com.WidthScale,                                       // xbotscl
		yscl * com.HeightScale,                                      // yscl
		0.0f,                                                        // rasterxadd
		0u,                                                          // roto
		alpha,                                                       // alpha
		0,                                                           // rle
		pluginbuf                                                    // pluginbuf
	);

	return static_cast<float>(spr->rct_w) * xscl;
}

// =========================================================================
// FontData::drawText
// =========================================================================
// SSZ equivalent (font.ssz drawText):
//   1. Compute starting dx, dy with offset and alignment
//   2. Apply palette FX (allPalFX)
//   3. For software renderer: build glyph data array and call renderFontBatch
//   4. Fallback: per-character drawChar loop

void FontData::drawText(float x, float y, float xscl, float yscl,
	int bank, int salpha, int dalpha, const SdlRect& window,
	int align, const std::string& txt)
{
	if (mpl.empty()) return;

	const auto& com = common_get_state();

	// Compute starting position (SSZ equivalent)
	float dx = x + static_cast<float>(offsetx) * xscl
		+ static_cast<float>(com.GameWidth - 320) / 2.0f;
	float dy = y + static_cast<float>(offsety - (sizey - 1)) * yscl
		+ static_cast<float>(com.GameHeight - 240);

	// Alignment
	int textW = textWidth(txt);
	if (align == 0) {
		dx -= std::round(static_cast<double>(textW) * xscl) * 0.5f;
	} else if (align < 0) {
		dx -= static_cast<float>(textW) * xscl;
	}

	// Get palette from the first mapped character's bank palette
	std::vector<uint32_t> pal;
	if (!mpl.empty() && bank < static_cast<int>(mpl[0].img.size())) {
		pal = mpl[0].img[bank].colorPallet;
	}

	// Apply palette FX (allPalFX)
	// SSZ: if(.sff.allPalFX~enable) pal = .sff.allPalFX~getFxPal(pal, false);
	{
		PalFXData& allFX = sff_get_all_palfx();
		if (allFX.enable) {
			pal = palfx_transform_palette(allFX, pal, false);
		}
	}

	// ── Software renderer atlas-batch path ──
	if (!fontAtlas.empty() && !txt.empty()) {
		int alpha = compute_alpha(salpha, dalpha, com.brightness);

		// Build glyph data array: for each character, store (offset, width)
		std::vector<int> gdata(txt.size() * 2, 0);
		int gc = 0;

		for (char c : txt) {
			if (c == ' ') {
				gdata[gc * 2] = -1;
				gdata[gc * 2 + 1] = sizex;
			} else {
				auto it = map.find(c);
				if (it != map.end() && it->second >= 0 &&
					it->second < static_cast<int>(mpl.size())) {
					gdata[gc * 2] = mpl[it->second].ofs;
					gdata[gc * 2 + 1] = mpl[it->second].w;
				} else {
					gdata[gc * 2] = -1;
					gdata[gc * 2 + 1] = 0;
				}
			}
			gc++;
		}

		// Call renderFontBatch
		renderFontBatch(
			fontAtlas,
			dx * com.WidthScale,
			dy * com.HeightScale,
			pal,
			fontAtlasStride,
			sizey,
			alpha,
			window,
			xscl * com.WidthScale,
			yscl * com.HeightScale,
			static_cast<float>(spacingx) * xscl * com.WidthScale,
			gdata,
			gc
		);
	} else {
		// ── Fallback: per-character rendering ──
		float curX = dx;
		for (char c : txt) {
			curX += drawChar(curX, dy, xscl, yscl, bank,
				salpha, dalpha, window, c, pal)
				+ static_cast<float>(spacingx) * xscl;
		}
	}
}

// =========================================================================
// Module-level state and API
// =========================================================================

static FontState g_font_state;

void font_init() {
	SSZ_TRACE_CAT(TRACE_SYS, "font_init");
	// Ensure FontState is initialized (debugFont starts as nullptr)
	g_font_state = FontState{};
}

FontState& font_get_state() {
	return g_font_state;
}

void font_render_text(const std::string& text, int x, int y, uint32_t color) {
	SSZ_TRACE_CAT(TRACE_SYS, "font_render_text");
	if (!g_font_state.debugFont) return;

	// Basic debug text rendering using the debug font
	// SSZ equivalent: debugFont~drawText(x, y, 1.0, 1.0, 0, 255, 0, scrrect, -1, text)
	// For now, we use default parameters with alignment = -1 (left align)
	SdlRect window;
	const auto& com = common_get_state();
	window.set(0, 0, com.GameWidth, com.GameHeight);

	g_font_state.debugFont->drawText(
		static_cast<float>(x), static_cast<float>(y),
		1.0f, 1.0f, 0, 255, 0, window, -1, text);
}

void font_render_text() {
	SSZ_TRACE_CAT(TRACE_SYS, "font_render_text (no-arg)");
	font_render_text("", 0, 0, 0);
}

} // namespace ikemen::ssz_native
