#pragma once

#include <string>

namespace ikemen {

bool open(const std::wstring& file, const std::wstring& arg,
          const std::wstring& cdir, bool waitfor, bool active);
bool moveToTrash(const std::wstring& file);

} // namespace ikemen
