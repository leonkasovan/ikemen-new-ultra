#pragma once

// alert_service.hpp — Native equivalent of ssz_script/lib/alert.ssz
//
// Provides:
// - Show a message box with title and message text
//
// Design note: alert_service delegates to the SSZ native plugin layer
// (main/alert/alert.cpp). The native plugin handles platform-specific
// dialog display (MessageBox on Windows, stderr on Linux).

#include <string>

namespace ikemen::ssz_native::alert {

// Show a message box with the given title and message.
void alert(const std::wstring& title, const std::wstring& message);

} // namespace ikemen::ssz_native::alert
