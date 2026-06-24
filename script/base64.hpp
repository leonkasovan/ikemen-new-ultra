#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ikemen {

std::string encBase64(const std::vector<uint8_t>& data);
std::vector<uint8_t> decBase64(const std::string& b64);

} // namespace ikemen
