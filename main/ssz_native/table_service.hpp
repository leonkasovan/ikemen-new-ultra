#pragma once

// table_service.hpp — Native equivalent of ssz_script/lib/table.ssz
//
// Provides:
// - String hash function
// - Generic NameTable<T> mapping string keys to values
//
// Design note: table_service implements data structures directly via the C++
// standard library (std::unordered_map). No SSZ plugin dependency — this is
// pure algorithm/container code.

#include <cstdint>
#include <string>
#include <unordered_map>
#include <functional>
#include <vector>

namespace ikemen::ssz_native::table {

// ---- Hash function ----
// Simple string hash matching SSZ behavior (sum of index ^ char).
inline uint32_t hash(const std::wstring& str) {
    uint32_t h = 0;
    for (size_t i = 0; i < str.size(); i++)
        h += static_cast<uint32_t>(i) ^ static_cast<uint32_t>(str[i]);
    return h;
}

// ---- NameTable ----
// Generic string-keyed hash table.
// Mirrors the &NameTable<_t> object from ssz_script/lib/table.ssz.
template <typename T>
class NameTable {
public:
    using Map = std::unordered_map<std::wstring, T>;

    NameTable() = default;

    // Set a key-value pair.
    void set(const std::wstring& name, const T& item) {
        map_[name] = item;
    }

    // Get a value by key. Returns nullptr if not found.
    const T* get(const std::wstring& name) const {
        auto it = map_.find(name);
        if (it != map_.end()) return &it->second;
        return nullptr;
    }

    // Remove a key-value pair. Returns true if the key existed.
    bool remove(const std::wstring& name) {
        return map_.erase(name) > 0;
    }

    // Returns true if the key exists.
    bool contains(const std::wstring& name) const {
        return map_.find(name) != map_.end();
    }

    // Get the number of entries.
    size_t size() const { return map_.size(); }

    // Remove all entries.
    void clear() { map_.clear(); }

    // Get all keys.
    std::vector<std::wstring> keys() const {
        std::vector<std::wstring> result;
        result.reserve(map_.size());
        for (const auto& [key, _] : map_)
            result.push_back(key);
        return result;
    }

    // Get all values.
    std::vector<T> values() const {
        std::vector<T> result;
        result.reserve(map_.size());
        for (const auto& [_, val] : map_)
            result.push_back(val);
        return result;
    }

    // Iterate over all entries.
    void for_each(const std::function<void(const std::wstring&, const T&)>& callback) const {
        for (const auto& [key, val] : map_)
            callback(key, val);
    }

private:
    Map map_;
};

} // namespace ikemen::ssz_native::table
