#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace ikemen {

enum class Seek { SET = 0, CUR = 1, END = 2 };

class File {
public:
	File() = default;
	~File() { close(); }

	bool isOpened() const { return m_fh != nullptr; }
	bool open(const std::wstring& fn, const std::wstring& mode);
	void close();

	template<typename T> bool read(T& out) {
		if (!m_fh) return false;
		return std::fread(&out, sizeof(T), 1, m_fh) == 1;
	}
	template<typename T> intptr_t readAry(std::vector<T>& out) {
		if (!m_fh) return -1;
		return static_cast<intptr_t>(std::fread(out.data(), sizeof(T), out.size(), m_fh));
	}
	template<typename T> bool write(const T& val) {
		if (!m_fh) return false;
		return std::fwrite(&val, sizeof(T), 1, m_fh) == 1;
	}
	template<typename T> intptr_t writeAry(const std::vector<T>& val) {
		if (!m_fh) return 0;
		return static_cast<intptr_t>(std::fwrite(val.data(), sizeof(T), val.size(), m_fh));
	}
	bool seek(int64_t offset, Seek origin);

	std::FILE* handle() { return m_fh; }

private:
	std::FILE* m_fh = nullptr;
};

template<typename T>
std::vector<T> readAll(const std::wstring& fn)
{
	File f;
	if (!f.open(fn, L"rb")) return {};
	std::fseek(f.handle(), 0, SEEK_END);
	int64_t sz = _ftelli64(f.handle());
	std::fseek(f.handle(), 0, SEEK_SET);
	std::vector<T> buf(static_cast<size_t>(sz) / sizeof(T) + 1);
	auto n = f.readAry(buf);
	buf.resize(static_cast<size_t>(n));
	return buf;
}

std::wstring                loadAsciiText(const std::wstring& fn);
bool                        saveAsciiText(const std::wstring& fn, const std::wstring& txt);
bool                        remove(const std::wstring& fn);
bool                        move(const std::wstring& oldn, const std::wstring& newn);
bool                        copy(const std::wstring& src, const std::wstring& dst, bool overwrite);
std::vector<std::wstring>     find(const std::wstring& pattern);
std::vector<std::wstring>     findDir(const std::wstring& pattern);
bool                        createDir(const std::wstring& dir);
bool                        removeDir(const std::wstring& dir);
bool                        setCurrentDir(const std::wstring& dir);
std::wstring                getCurrentDir();

} // namespace ikemen
