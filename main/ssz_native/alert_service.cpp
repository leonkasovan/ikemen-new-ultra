#include "alert_service.hpp"

#ifndef SSZ_STDCALL
#define SSZ_STDCALL __stdcall
#endif

// Native alert plugin function (defined in main/alert/alert.cpp).
// This declaration is provided by plugin_native_api.hpp (which is the
// single source of truth for shared declarations). It is tracked in
// plugin_native_api.hpp's M4 TODO for eventual consolidation.
void SSZ_STDCALL Alert(const std::wstring& title, const std::wstring& mes);

namespace ikemen::ssz_native::alert {

void alert(const std::wstring& title, const std::wstring& message) {
    Alert(title, message);
}

} // namespace ikemen::ssz_native::alert
