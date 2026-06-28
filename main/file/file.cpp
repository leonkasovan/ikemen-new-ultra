#ifdef _WIN32
#include <stdio.h>
#else
#include <limits.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <fnmatch.h>
#endif

#include "sszdef.h"

#include <vector>

#ifndef _WIN32
#include "pluginutil.hpp"
#endif

#ifndef _WIN32

#define MOVEFILE_WRITE_THROUGH 0x00000008

static errno_t _wfopen_s(FILE **pFile, const WCHR *filename, const WCHR *mode)
{
	*pFile =
		fopen(PluginUtil::wToA(filename).c_str(), PluginUtil::wToA(mode).c_str());
	return *pFile ? 0 : EINVAL;
}
static int _fseeki64(FILE *stream, int64_t offset, int origin)
{
	if(offset < LONG_MIN || offset > LONG_MAX) return EINVAL;
	return fseek(stream, (long)offset, origin);
}
static BOOL DeleteFile(const WCHR *lpFileName)
{
	return remove(PluginUtil::wToA(lpFileName).c_str()) == 0;
}
static BOOL MoveFileEx(
	const WCHR *lpExistingFileName, const WCHR *lpNewFileName, DWORD)
{
	return
		rename(
			PluginUtil::wToA(lpExistingFileName).c_str(),
			PluginUtil::wToA(lpNewFileName).c_str()) == 0;
}
static BOOL CopyFile(
	const WCHR *lpExistingFileName, const WCHR *lpNewFileName,
	BOOL bFailIfExists)
{
	auto efile = open(PluginUtil::wToA(lpExistingFileName).c_str(), O_RDONLY);
	if(efile == -1) return false;
	auto nfn = PluginUtil::wToA(lpNewFileName);
	if(!bFailIfExists) remove(nfn.c_str());
	struct stat stat_buf;
	int nfile;
	if(
		fstat(efile, &stat_buf) || !access(nfn.c_str(), F_OK)
		|| (nfile = open(nfn.c_str(), O_WRONLY | O_CREAT, stat_buf.st_mode)) == -1)
	{
		close(efile);
		return false;
	}
	off_t offset = 0;
	auto ret = sendfile(nfile, efile, &offset, stat_buf.st_size) != -1;
	close(efile);
	close(nfile);
	return ret;
}
static BOOL CreateDirectory(const WCHR *lpPathName, void*)
{
	return mkdir(PluginUtil::wToA(lpPathName).c_str(), 0777) == 0;
}
static BOOL RemoveDirectory(const WCHR *lpFileName)
{
	return rmdir(PluginUtil::wToA(lpFileName).c_str()) == 0;
}
static BOOL SetCurrentDirectory(const WCHR *lpFileName)
{
	return chdir(PluginUtil::wToA(lpFileName).c_str()) == 0;
}
#endif

intptr_t SSZ_STDCALL Open(const std::wstring& md, const std::wstring& fn)
{
	FILE *pFile = NULL;
	_wfopen_s(&pFile, fn.c_str(), md.c_str());
	return (intptr_t)pFile;
}

void SSZ_STDCALL FileClose(FILE *pFile)
{
	if(pFile != NULL) fclose(pFile);
}

bool SSZ_STDCALL Read(intptr_t size, void *p, FILE *pFile)
{
	if(pFile == NULL) return false;
	return fread(p, size, 1, pFile) == 1;
}

intptr_t SSZ_STDCALL ReadAry(intptr_t size, void *data, intptr_t bytes, FILE *pFile)
{
	if(pFile == NULL) return -1;
	if(bytes == 0) return 0;
	return fread(data, size, bytes / size, pFile);
}

bool SSZ_STDCALL Write(intptr_t size, const void *p, FILE *pFile)
{
	if(pFile == NULL) return false;
	return fwrite(p, size, 1, pFile) == 1;
}

intptr_t SSZ_STDCALL WriteAry(intptr_t size, const void *data, intptr_t bytes, FILE *pFile)
{
	if(pFile == NULL) return -1;
	if(bytes == 0) return 0;
	return fwrite(data, size, bytes / size, pFile);
}

bool SSZ_STDCALL Seek(int32_t origin, int64_t offset, FILE *pFile)
{
	if(pFile == NULL) return false;
	return _fseeki64(pFile, offset, origin) != 0;
}

std::wstring SSZ_STDCALL LoadAsciiText(const std::wstring& path)
{
	std::string tmp;
	FILE *pFile = NULL;
	if(_wfopen_s(&pFile, path.c_str(), L("rb")) != 0) return std::wstring();
	char ch;
	while(fread(&ch, sizeof(char), 1, pFile) == 1) tmp += ch;
	fclose(pFile);
	if(tmp.size() == 0) return std::wstring();
	std::wstring result;
	result.resize(tmp.size());
	for(intptr_t i = 0; i < (intptr_t)tmp.size(); i++){
		result[i] = (WCHR)(unsigned char)tmp[i];
	}
	return result;
}

bool SSZ_STDCALL SaveAsciiText(const std::wstring& txt, const std::wstring& path)
{
	std::string tmp;
	tmp.resize(txt.size());
	if(tmp.size() == 0) return true;
	for(intptr_t i = 0; i < (intptr_t)tmp.size(); i++){
		tmp[i] = (unsigned char)txt[i];
	}
	FILE *pFile = NULL;
	if(_wfopen_s(&pFile, path.c_str(), L("wb")) != 0) return false;
	bool ret =
		fwrite(tmp.data(), sizeof(char), tmp.size(), pFile) == tmp.size();
	fclose(pFile);
	return ret;
}

bool SSZ_STDCALL Delete(const std::wstring& file)
{
	return DeleteFile(file.c_str()) != 0;
}

bool SSZ_STDCALL Move(const std::wstring& newn, const std::wstring& oldn)
{
	return
		MoveFileEx(
			oldn.c_str(), newn.c_str(),
			MOVEFILE_WRITE_THROUGH) != 0;
}

bool SSZ_STDCALL Copy(bool overwrite, const std::wstring& dist, const std::wstring& source)
{
	return
		CopyFile(
			source.c_str(), dist.c_str(),
			!overwrite) != 0;
}

std::vector<std::wstring> SSZ_STDCALL Find(const std::wstring& pattern)
{
	std::vector<std::wstring> files;
#ifdef _WIN32
	HANDLE fh;
	WIN32_FIND_DATA fd;
	fh = FindFirstFile(pattern.c_str(), &fd);
	if(fh == INVALID_HANDLE_VALUE) return files;
	do{
		if(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
		files.push_back(fd.cFileName);
	}while(FindNextFile(fh, &fd));
	FindClose(fh);
#else
	auto file = PluginUtil::wToA(pattern);
	auto i = file.find_last_of('/');
	std::string dstr;
	DIR *pDir;
	if(i != std::string::npos){
		dstr.append(file.data(), i + 1);
		file.erase(0, i + 1);
		pDir = opendir(dstr.c_str());
	}else{
		pDir = opendir(".");
	}
	if(pDir == nullptr) return files;
	struct dirent *ent;
	while((ent = readdir(pDir)) != nullptr){
		if(
			(ent->d_type & DT_DIR)
			|| fnmatch(file.c_str(), ent->d_name, 0)) continue;
		files.push_back(PluginUtil::aToW(ent->d_name));
	}
	closedir(pDir);
#endif
	return files;
}

std::vector<std::wstring> SSZ_STDCALL FindDir(const std::wstring& pattern)
{
	std::vector<std::wstring> dirs;
#ifdef _WIN32
	HANDLE fh;
	WIN32_FIND_DATA fd;
	fh = FindFirstFile(pattern.c_str(), &fd);
	if(fh == INVALID_HANDLE_VALUE) return dirs;
	do{
		if((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) continue;
		if(fd.cFileName[0] == L'.'){
			if(fd.cFileName[1] == L'\0') continue;
			if(fd.cFileName[1] == L'.' && fd.cFileName[2] == L'\0') continue;
		}
		dirs.push_back(fd.cFileName);
	}while(FindNextFile(fh, &fd));
	FindClose(fh);
#else
	auto file = PluginUtil::wToA(pattern);
	auto i = file.find_last_of('/');
	std::string dstr;
	DIR *pDir;
	if(i != std::string::npos){
		dstr.append(file.data(), i + 1);
		file.erase(0, i + 1);
		pDir = opendir(dstr.c_str());
	}else{
		pDir = opendir(".");
	}
	if(pDir == nullptr) return dirs;
	struct dirent *ent;
	while((ent = readdir(pDir)) != nullptr){
		if(
			!(ent->d_type & DT_DIR)
			|| fnmatch(file.c_str(), ent->d_name, 0)) continue;
		if(ent->d_name[0] == '.'){
			if(ent->d_name[1] == '\0') continue;
			if(ent->d_name[1] == '.' && ent->d_name[2] == '\0') continue;
		}
		dirs.push_back(PluginUtil::aToW(ent->d_name));
	}
	closedir(pDir);
#endif
	return dirs;
}

bool SSZ_STDCALL CreateDir(const std::wstring& dir)
{
	return CreateDirectory(dir.c_str(), nullptr) != 0;
}

bool SSZ_STDCALL RemoveDir(const std::wstring& dir)
{
	return RemoveDirectory(dir.c_str()) != 0;
}

bool SSZ_STDCALL SetCurrentDir(const std::wstring& dir)
{
	return SetCurrentDirectory(dir.c_str()) != 0;
}

std::wstring SSZ_STDCALL GetCurrentDir()
{
	std::wstring dir;
#ifdef _WIN32
	dir.resize(GetCurrentDirectory(0, nullptr));
	if(
		dir.size() <= 0
		|| GetCurrentDirectory(
			(DWORD)dir.size(), &dir[0]) == 0)
	{
		return std::wstring();
	}
	// Remove null terminator that GetCurrentDirectory includes
	dir.resize(wcslen(dir.c_str()));
#else
	char *str = getcwd(nullptr, 0);
	dir = PluginUtil::aToW(str);
	free(str);
#endif
	return dir;
}
