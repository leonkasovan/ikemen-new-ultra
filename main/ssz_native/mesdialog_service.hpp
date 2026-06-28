#pragma once

// mesdialog_service.hpp — Native equivalent of ssz_script/lib/alpha/mesdialog.ssz
//
// Provides:
// - Dialog functions: YesNo, InputStr, GetClipboardStr
// - INI file functions: GetInifileString, GetInifileInt, WriteInifileString
// - Encoding functions: UbytesToStr, StrToUbytes, AsciiToLocal
// - Compression: UnCompress
// - Shared string: SetSharedString, GetSharedString
//
// Design note: mesdialog_service delegates to the SSZ native plugin layer
// (main/mesdialog/mesdialog.cpp). The native plugin handles platform-specific
// dialogs, INI file access, code page conversion, and compression.

#include <cstdint>
#include <string>
#include <vector>

namespace ikemen::ssz_native::mesdialog {

// Code page constants matching SSZ |CodePage enum.
// ACP=0 (default ANSI code page) and UTF8=65001 are the most commonly used.
enum CodePage : uint32_t {
    ACP        = 0,       // default ANSI code page
    OEMCP      = 1,       // default OEM code page
    MACCP      = 2,       // default MAC code page
    THREAD_ACP = 3,       // current thread's ANSI code page
    SYMBOL     = 42,      // SYMBOL translations
    SJIS       = 932,     // Japanese Shift-JIS
    ISO_8859_1 = 1252,    // Latin-1
    EUC_JP     = 20932,   // Japanese EUC
    UTF7       = 65000,   // UTF-7
    UTF8       = 65001,   // UTF-8 (most common for modern interop)
};

// Show a Yes/No dialog with the given title. Returns true if Yes was pressed.
bool yes_no(const std::wstring& title);

// Show an input dialog with the given title. Returns the entered text.
std::wstring input_str(const std::wstring& title);

// Get clipboard text.
std::wstring get_clipboard_str();

// ---- INI file operations ----

// Read a string value from an INI file.
std::wstring get_inifile_string(const std::wstring& def, const std::wstring& key,
                                const std::wstring& app, const std::wstring& file);

// Read an integer value from an INI file.
int32_t get_inifile_int(int32_t def, const std::wstring& key,
                        const std::wstring& app, const std::wstring& file);

// Write a string value to an INI file.
bool write_inifile_string(const std::wstring& str, const std::wstring& key,
                          const std::wstring& app, const std::wstring& file);

// ---- Encoding ----

// Convert bytes to wide string using the given code page.
std::wstring ubytes_to_str(const void* data, intptr_t bytes, CodePage cp);

// Convert a wide string to bytes using the given code page.
std::vector<uint8_t> str_to_ubytes(const void* data, intptr_t bytes, CodePage cp);

// Convert ASCII/UTF-8 data to local encoding.
std::wstring ascii_to_local(const void* data, intptr_t bytes);

// ---- Compression ----

// Decompress data. Returns the decompressed bytes, or empty on failure.
std::vector<uint8_t> uncompress(const void* data, intptr_t bytes);

// ---- Shared string ----

void set_shared_string(const std::wstring& str);
std::wstring get_shared_string();

} // namespace ikemen::ssz_native::mesdialog
