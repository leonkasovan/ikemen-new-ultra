#pragma once

// shell_service.hpp — Native equivalent of ssz_script/lib/shell.ssz
//
// Provides:
// - Shell open (ShellExecute)
// - Move to trash
//
// Design note: shell_service delegates to the SSZ native plugin layer
// (main/shell/shell.cpp). The native plugin wraps Windows Shell API.

#include <string>

namespace ikemen::ssz_native::shell {

// Open a file/folder with ShellExecute.
bool open(const std::wstring& file, const std::wstring& arg,
          const std::wstring& cdir, bool waitfor, bool active);

// Move a file to the recycle bin.
bool move_to_trash(const std::wstring& file);

} // namespace ikemen::ssz_native::shell
