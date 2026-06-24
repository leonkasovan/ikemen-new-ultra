#pragma once

#include <string>

struct Reference;

namespace ikemen {

class Alert {
public:
	static void alert(const Reference& mes, const std::string& typeName);
};

} // namespace ikemen
