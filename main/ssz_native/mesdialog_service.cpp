#include "mesdialog_service.hpp"
#include "plugin_native_api.hpp"

#ifndef SSZ_STDCALL
#define SSZ_STDCALL __stdcall
#endif

#ifdef _WIN32
#include <windows.h>  // UINT
#else
typedef unsigned int UINT;
#endif

// Native mesdialog plugin functions — declared in plugin_native_api.hpp (single source of truth).
// Duplicate declarations removed 2026-07-06 — use the shared header instead.

namespace ikemen::ssz_native::mesdialog {

bool yes_no(const std::wstring& title) {
    return YesNo(title);
}

std::wstring input_str(const std::wstring& title) {
    return InputStr(title);
}

std::wstring get_clipboard_str() {
    return GetClipboardStr();
}

std::wstring get_inifile_string(const std::wstring& def, const std::wstring& key,
                                const std::wstring& app, const std::wstring& file) {
    return GetInifileString(def, key, app, file);
}

int32_t get_inifile_int(int32_t def, const std::wstring& key,
                        const std::wstring& app, const std::wstring& file) {
    return GetInifileInt(def, key, app, file);
}

bool write_inifile_string(const std::wstring& str, const std::wstring& key,
                          const std::wstring& app, const std::wstring& file) {
    return WriteInifileString(str, key, app, file);
}

std::wstring ubytes_to_str(const void* data, intptr_t bytes, CodePage cp) {
    std::wstring output;
    UbytesToStr(data, bytes, static_cast<UINT>(cp), output);
    return output;
}

std::vector<uint8_t> str_to_ubytes(const void* data, intptr_t bytes, CodePage cp) {
    std::vector<uint8_t> output;
    StrToUbytes(data, bytes, static_cast<UINT>(cp), output);
    return output;
}

std::wstring ascii_to_local(const void* data, intptr_t bytes) {
    std::wstring output;
    AsciiToLocal(data, bytes, output);
    return output;
}

std::vector<uint8_t> uncompress(const void* data, intptr_t bytes) {
    std::vector<uint8_t> output;
    bool ok = UnCompress(data, bytes, output);
    if (!ok) output.clear();
    return output;
}

void set_shared_string(const std::wstring& str) {
    SetSharedString(str);
}

std::wstring get_shared_string() {
    return GetSharedString();
}

} // namespace ikemen::ssz_native::mesdialog
