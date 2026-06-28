#include "shell_service.hpp"

#ifndef SSZ_STDCALL
#define SSZ_STDCALL __stdcall
#endif

// Native shell plugin functions (defined in main/shell/shell.cpp).
// These declarations are provided by plugin_native_api.hpp (which is the
// single source of truth for shared declarations). Tracked in M4 TODO.
bool SSZ_STDCALL ShellOpen(bool act, bool wait, const std::wstring& direct,
                           const std::wstring& param, const std::wstring& file);
bool SSZ_STDCALL MoveTrash(const std::wstring& file);

namespace ikemen::ssz_native::shell {

bool open(const std::wstring& file, const std::wstring& arg,
          const std::wstring& cdir, bool waitfor, bool active) {
    return ShellOpen(active, waitfor, cdir, arg, file);
}

bool move_to_trash(const std::wstring& file) {
    return MoveTrash(file);
}

} // namespace ikemen::ssz_native::shell
