#include "shell_service.hpp"
#include "plugin_native_api.hpp"

#ifndef SSZ_STDCALL
#define SSZ_STDCALL __stdcall
#endif

// Shell plugin functions are declared in plugin_native_api.hpp (single source of truth).
// Duplicate declarations removed 2026-07-06.

namespace ikemen::ssz_native::shell {

bool open(const std::wstring& file, const std::wstring& arg,
          const std::wstring& cdir, bool waitfor, bool active) {
    return ShellOpen(active, waitfor, cdir, arg, file);
}

bool move_to_trash(const std::wstring& file) {
    return MoveTrash(file);
}

} // namespace ikemen::ssz_native::shell
