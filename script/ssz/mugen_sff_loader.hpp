#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace ikemen {

struct Sprite;

// ── Mugen SFF Loader ────────────────────────────────────────────────────
// Drop-in alternative to SffFile::loadAll(). Opens the file with FILE* I/O
// (matching the original mugen_sff.cpp reference implementation) and produces
// ikemen::Sprite objects identical to the built-in loader.
//
// Usage (from SffFile::loadAll):
//   std::map<uint64_t, Sprite> sprites;
//   if (loadMugenSff(path, sprites, filename)) { ... }

bool loadMugenSff(const std::wstring& path,
                  std::map<uint64_t, Sprite>& sprites,
                  std::string& filename);

} // namespace ikemen
