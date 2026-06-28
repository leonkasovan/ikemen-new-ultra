#pragma once

#include <cstdio>
#include <string>
#include <vector>

#include "ssz_value.hpp"

namespace ikemen::ssz_native {

// Seek origin constants matching SSZ |Seek enum
enum class SeekOrigin : int32_t {
    Set = 0,
    Cur = 1,
    End = 2,
};

// RAII wrapper for a file handle.
// Mirrors the &file.File object from ssz_script/lib/file.ssz.
class FileHandle {
public:
    FileHandle() = default;

    // Opens the file; returns true on success.
    // mode: "rb", "wb", "w+b", etc.
    bool open(const std::wstring& path, const std::wstring& mode);

    // Closes the file if open.
    void close();

    bool is_open() const { return fp_ != nullptr; }

    // Read one element of size bytes; returns true on success.
    bool read(void* data, intptr_t size);

    // Read up to `count` elements of `elem_size` bytes each.
    // Returns the number of elements actually read.
    intptr_t read_array(void* data, intptr_t elem_size, intptr_t count);

    // Write one element of size bytes; returns true on success.
    bool write(const void* data, intptr_t size);

    // Write up to `count` elements of `elem_size` bytes each.
    // Returns the number of elements actually written.
    intptr_t write_array(const void* data, intptr_t elem_size, intptr_t count);

    // Seek to offset relative to origin.
    bool seek(int64_t offset, SeekOrigin origin);

    // Destructor closes the file if open.
    ~FileHandle() { close(); }

    // Non-copyable, movable.
    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;
    FileHandle(FileHandle&& other) noexcept : fp_(other.fp_) { other.fp_ = nullptr; }
    FileHandle& operator=(FileHandle&& other) noexcept {
        if (this != &other) {
            close();
            fp_ = other.fp_;
            other.fp_ = nullptr;
        }
        return *this;
    }

private:
    FILE* fp_ = nullptr;
};

// ---- Free functions matching lib/file.ssz module-level API ----

// Read entire file into a byte vector.
SszBytes read_all(const std::wstring& path);

// Text file I/O
std::wstring load_ascii_text(const std::wstring& path);
bool save_ascii_text(const std::wstring& path, const std::wstring& text);

// File/directory operations
bool remove_file(const std::wstring& path);
bool move_file(const std::wstring& old_path, const std::wstring& new_path);
bool copy_file(const std::wstring& source, const std::wstring& dest, bool overwrite);

// Pattern matching (Find/FindDir)
SszArray<std::wstring> find_files(const std::wstring& pattern);
SszArray<std::wstring> find_directories(const std::wstring& pattern);

// Directory operations
bool create_directory(const std::wstring& path);
bool remove_directory(const std::wstring& path);
bool set_current_directory(const std::wstring& path);
std::wstring get_current_directory();

} // namespace ikemen::ssz_native
