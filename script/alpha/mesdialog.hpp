#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ikemen {

// ── Code-page identifiers ────────────────────────────────────────────────

enum class CodePage : uint32_t {
	ACP        = 0,
	OEMCP      = 1,
	MACCP      = 2,
	THREAD_ACP = 3,
	SYMBOL     = 42,
	SJIS       = 932,
	ISO_8859_1 = 1252,
	EUC_JP     = 20932,
	UTF7       = 65000,
	UTF8       = 65001,
};

// ── Dialog / system ──────────────────────────────────────────────────────

bool yesNo(const std::wstring& msg);
bool getClipboardStr(std::wstring& out);

// ── Shared clipboard string ──────────────────────────────────────────────

void setSharedString(const std::wstring& s);
void getSharedString(std::wstring& out);

// ── INI file ─────────────────────────────────────────────────────────────

std::wstring getInifileString(const std::wstring& app,
                              const std::wstring& key,
                              const std::wstring& file,
                              const std::wstring& def = L"");
int32_t      getInifileInt(const std::wstring& app,
                           const std::wstring& key,
                           const std::wstring& file,
                           int32_t def = 0);
bool         writeInifileString(const std::wstring& app,
                                const std::wstring& key,
                                const std::wstring& file,
                                const std::wstring& val);

// ── Input dialog ─────────────────────────────────────────────────────────

void inputStr(const std::wstring& title, std::wstring& out);

// ── Compression ──────────────────────────────────────────────────────────

bool uncompress(const std::vector<uint8_t>& src, std::vector<uint8_t>& dst);

// ── Code-page conversion ─────────────────────────────────────────────────

std::wstring ubytesToStr(const std::vector<uint8_t>& src, CodePage cp);
std::vector<uint8_t> strToUbytes(const std::wstring& src, CodePage cp);
std::wstring asciiToLocal(const std::wstring& src);

// ── Exclusive-access (mutex) check ───────────────────────────────────────

bool tajuuCheck(const std::wstring& name);

} // namespace ikemen
