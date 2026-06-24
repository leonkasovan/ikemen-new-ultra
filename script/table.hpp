#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace ikemen {

uint32_t hash(const std::wstring& str);

template<typename T>
class NameTable {
public:
	void set(const std::wstring& name, const T& item) { m_map[name] = item; }
	T    get(const std::wstring& name) const {
		auto it = m_map.find(name);
		return it != m_map.end() ? it->second : T{};
	}
	bool remove(const std::wstring& name) { return m_map.erase(name) > 0; }

	void each(const std::function<void(const T&)>& fn) const {
		for (auto& p : m_map) fn(p.second);
	}
	void forEach(const std::function<void(const std::wstring&, const T&)>& fn) const {
		for (auto& p : m_map) fn(p.first, p.second);
	}

private:
	std::unordered_map<std::wstring, T> m_map;
};

template<typename T>
int64_t intHash(T n) {
	int64_t h = static_cast<int64_t>(n);
	int64_t step = static_cast<int64_t>(sizeof(T) * 8);
	for (int64_t i = 8; (i >> 3) < step; i += 8) {
		h ^= h >> i;
	}
	return h;
}

template<typename IntKey, typename T>
class IntTable {
public:
	void set(IntKey key, const T& item) { m_map[key] = item; }
	T    get(IntKey key) const {
		auto it = m_map.find(key);
		return it != m_map.end() ? it->second : T{};
	}
	bool remove(IntKey key) { return m_map.erase(key) > 0; }

	void each(const std::function<void(const T&)>& fn) const {
		for (auto& p : m_map) fn(p.second);
	}
	void forEach(const std::function<void(IntKey, const T&)>& fn) const {
		for (auto& p : m_map) fn(p.first, p.second);
	}
	void clear() { m_map.clear(); }
	T*   getPtr(IntKey key) { auto it = m_map.find(key); return it != m_map.end() ? &it->second : nullptr; }
	const T* getPtr(IntKey key) const { auto it = m_map.find(key); return it != m_map.end() ? &it->second : nullptr; }

private:
	std::unordered_map<IntKey, T> m_map;
};

} // namespace ikemen
