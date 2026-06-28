#include "file_service.hpp"

#include <cstring>

#ifdef _WIN32
#include <io.h>        // _ftelli64
#endif

#include "sszdef.h" // for SSZ_STDCALL — must come before plugin_native_api to set the project's platform ABI first
#include "ssz_native/plugin_native_api.hpp"

namespace ikemen::ssz_native {

// ---- FileHandle ----

bool FileHandle::open(const std::wstring& path, const std::wstring& mode) {
    close();
    fp_ = reinterpret_cast<FILE*>(Open(mode, path));
    return fp_ != nullptr;
}

void FileHandle::close() {
    if (fp_) {
        FileClose(fp_);
        fp_ = nullptr;
    }
}

bool FileHandle::read(void* data, intptr_t size) {
    if (!fp_) return false;
    return Read(size, data, fp_);
}

intptr_t FileHandle::read_array(void* data, intptr_t elem_size, intptr_t count) {
    if (!fp_) return -1;
    return ReadAry(elem_size, data, count * elem_size, fp_);
}

bool FileHandle::write(const void* data, intptr_t size) {
    if (!fp_) return false;
    return Write(size, data, fp_);
}

intptr_t FileHandle::write_array(const void* data, intptr_t elem_size, intptr_t count) {
    if (!fp_) return -1;
    return WriteAry(elem_size, data, count * elem_size, fp_);
}

bool FileHandle::seek(int64_t offset, SeekOrigin origin) {
    if (!fp_) return false;
    return Seek(static_cast<int32_t>(origin), offset, fp_);
}

// ---- Free functions ----

SszBytes read_all(const std::wstring& path) {
    SszBytes result;
    FILE* f = reinterpret_cast<FILE*>(Open(L"rb", path));
    if (!f) return result;

    // Determine file size
    Seek(2, 0, f); // END
#ifdef _WIN32
    int64_t size = _ftelli64(f);
#else
    // TODO: Linux: use ftello(f) instead of _ftelli64
    int64_t size = ftello(f);
#endif
    Seek(0, 0, f); // SET

    if (size > 0) {
        result.data.resize(static_cast<size_t>(size));
        ReadAry(1, result.data.data(), static_cast<intptr_t>(size), f);
    }

    FileClose(f);
    return result;
}

std::wstring load_ascii_text(const std::wstring& path) {
    return LoadAsciiText(path);
}

bool save_ascii_text(const std::wstring& path, const std::wstring& text) {
    return SaveAsciiText(text, path);
}

bool remove_file(const std::wstring& path) {
    return Delete(path);
}

bool move_file(const std::wstring& old_path, const std::wstring& new_path) {
    return Move(new_path, old_path);
}

bool copy_file(const std::wstring& source, const std::wstring& dest, bool overwrite) {
    return Copy(overwrite, dest, source);
}

SszArray<std::wstring> find_files(const std::wstring& pattern) {
    auto result = Find(pattern);
    return SszArray<std::wstring>(result.begin(), result.end());
}

SszArray<std::wstring> find_directories(const std::wstring& pattern) {
    auto result = FindDir(pattern);
    return SszArray<std::wstring>(result.begin(), result.end());
}

bool create_directory(const std::wstring& path) {
    return CreateDir(path);
}

bool remove_directory(const std::wstring& path) {
    return RemoveDir(path);
}

bool set_current_directory(const std::wstring& path) {
    return SetCurrentDir(path);
}

std::wstring get_current_directory() {
    return GetCurrentDir();
}

} // namespace ikemen::ssz_native
