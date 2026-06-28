#include "mesdialog_service.hpp"

#ifndef SSZ_STDCALL
#define SSZ_STDCALL __stdcall
#endif

#ifdef _WIN32
#include <windows.h>  // UINT
#else
typedef unsigned int UINT;
#endif

// Native mesdialog plugin functions (defined in main/mesdialog/mesdialog.cpp).
// These declarations duplicate bridge.cpp:85-99. They are tracked in
// plugin_native_api.hpp's M4 TODO for eventual consolidation.
// TODO: Move to plugin_native_api.hpp when mesdialog is migrated (Phase 2).
bool        SSZ_STDCALL YesNo(const std::wstring& r);
void        SSZ_STDCALL VeryUnsafeCopy(intptr_t size, void* src, void* dst);
std::wstring SSZ_STDCALL GetClipboardStr();
intptr_t    SSZ_STDCALL TazyuuCheck(const std::wstring& name);
void        SSZ_STDCALL CloseTazyuuHandle(intptr_t mutex);
// Note: VeryUnsafeCopy, TazyuuCheck, and CloseTazyuuHandle are declared for
// completeness with the bridge's forward-declaration block. They are not
// exposed in the mesdialog_service public API (internal/unsafe functions).
std::wstring SSZ_STDCALL GetInifileString(const std::wstring& def, const std::wstring& key, const std::wstring& app, const std::wstring& file);
int32_t     SSZ_STDCALL GetInifileInt(int32_t def, const std::wstring& key, const std::wstring& app, const std::wstring& file);
bool        SSZ_STDCALL WriteInifileString(const std::wstring& str, const std::wstring& key, const std::wstring& app, const std::wstring& file);
bool        SSZ_STDCALL UnCompress(const void* data, intptr_t bytes, std::vector<uint8_t>& output);
void        SSZ_STDCALL UbytesToStr(const void* data, intptr_t bytes, UINT cp, std::wstring& output);
void        SSZ_STDCALL StrToUbytes(const void* data, intptr_t bytes, UINT cp, std::vector<uint8_t>& output);
void        SSZ_STDCALL AsciiToLocal(const void* data, intptr_t bytes, std::wstring& output);
void        SSZ_STDCALL SetSharedString(const std::wstring& str);
std::wstring SSZ_STDCALL GetSharedString();
std::wstring SSZ_STDCALL InputStr(const std::wstring& title);

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
