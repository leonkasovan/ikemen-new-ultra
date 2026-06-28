#include <windows.h>
#include <process.h>
#include <stdint.h>
#include <zlib.h>
#include <string>
#include <vector>
static const unsigned char ASCII_FLAG  = 0x01; /* bit 0 set: file probably ascii text */
static const unsigned char HEAD_CRC    = 0x02; /* bit 1 set: header CRC present */
static const unsigned char EXTRA_FIELD = 0x04; /* bit 2 set: extra field present */
static const unsigned char ORIG_NAME   = 0x08; /* bit 3 set: original file name present */
static const unsigned char COMMENT     = 0x10; /* bit 4 set: file comment present */
static const unsigned char RESERVED    = 0xE0; /* bits 5..7: reserved */
#include "locksingler.hpp"
#include "sszdef.h"

// Initialize ghInstance with main module handle
HINSTANCE ghInstance = GetModuleHandle(NULL);
LockSingler g_mtx;
std::wstring g_sharedstring;


bool SSZ_STDCALL YesNo(const std::wstring& r)
{
	return
		MessageBox(
			NULL, r.c_str(), L"Message", MB_YESNO) == IDYES;
}

void SSZ_STDCALL VeryUnsafeCopy(intptr_t size, void *src, void *dst)
{
	memcpy(dst, src, size);
}

std::wstring SSZ_STDCALL GetClipboardStr()
{
	HANDLE hText;
	wchar_t *pText;
	OpenClipboard(NULL);
	hText = GetClipboardData(CF_UNICODETEXT);
	if(hText == NULL) return std::wstring();
	pText = (wchar_t*)GlobalLock(hText);
	std::wstring result(pText);
	GlobalUnlock(hText);
	CloseClipboard();
	return result;
}

intptr_t SSZ_STDCALL TazyuuCheck(const std::wstring& name)
{
	HANDLE hMutex;
	SECURITY_ATTRIBUTES securityatt;
	securityatt.nLength = sizeof(securityatt);
	securityatt.lpSecurityDescriptor = NULL;
	securityatt.bInheritHandle = FALSE;
	hMutex = CreateMutex(&securityatt, FALSE, name.c_str());
	if(GetLastError() == ERROR_ALREADY_EXISTS){
		CloseHandle(hMutex);
		hMutex = NULL;
	}
	return (intptr_t)hMutex;
}

void SSZ_STDCALL CloseTazyuuHandle(intptr_t mutex)
{
	ReleaseMutex((HANDLE)mutex);
	CloseHandle((HANDLE)mutex);
}

std::wstring SSZ_STDCALL GetInifileString(const std::wstring& def, const std::wstring& key, const std::wstring& app, const std::wstring& file)
{
	wchar_t* pws;
	std::wstring tmp1 = file;
	pws = _wfullpath(NULL, tmp1.c_str(), 0);
	if(pws != NULL){
		tmp1 = pws;
		free(pws);
	}
	std::wstring tmp5;
	tmp5.resize(256);
	GetPrivateProfileString(
		app.c_str(), key.c_str(),
		def.c_str(),
		(wchar_t*)tmp5.data(), (DWORD)tmp5.size(), tmp1.c_str());
	return tmp5.c_str();
}

int32_t SSZ_STDCALL GetInifileInt(int32_t def, const std::wstring& key, const std::wstring& app, const std::wstring& file)
{
	wchar_t* pws;
	std::wstring tmp1 = file;
	pws = _wfullpath(NULL, tmp1.c_str(), 0);
	if(pws != NULL){
		tmp1 = pws;
		free(pws);
	}
	return
		GetPrivateProfileInt(
			app.c_str(), key.c_str(),
			def, tmp1.c_str());
}

bool SSZ_STDCALL WriteInifileString(const std::wstring& str, const std::wstring& key, const std::wstring& app, const std::wstring& file)
{
	wchar_t* pws;
	std::wstring tmp1 = file;
	pws = _wfullpath(NULL, tmp1.c_str(), 0);
	if(pws != NULL){
		tmp1 = pws;
		free(pws);
	}
	return
		WritePrivateProfileString(
			app.c_str(), key.c_str(),
			str.c_str(), tmp1.c_str()) != 0;
}


int HeadCheck(const char *const buf, intptr_t len)
{
	int i = 0;
	if(len < 10 || buf[i++] != '\x1f' || buf[i++] != '\x8b'){
		return -1;
	}
	int method = buf[i++];
	int flags  = buf[i++];
	if(method != Z_DEFLATED || (flags & RESERVED) != 0){
		return -1;
	}
	i += 6;
	if(flags & EXTRA_FIELD){
		int len = (unsigned char)buf[i++];
		len += ((int)(unsigned char)buf[i++] << 8);
		i += len;
	}
	if(flags & ORIG_NAME){
		while(i < len) if(buf[i++] == 0) break;
	}
	if(flags & COMMENT){
		while(i < len) if(buf[i++] == 0) break;
	}
	if(flags & HEAD_CRC){
		i += 2;
	}
	if(i > len) return -1;
	return i;
}

bool SSZ_STDCALL UnCompress(const void* data, intptr_t bytes, std::vector<uint8_t>& output)
{
	z_stream z;
	intptr_t hsize;
	char outbuf[4096];
	int status;
	if(bytes == 0) return false;
	if((hsize = HeadCheck((const char*)data, bytes)) < 0) hsize = 0;
	z.zalloc = Z_NULL;
	z.zfree = Z_NULL;
	z.opaque = Z_NULL;
	z.next_in = Z_NULL;
	z.avail_in = 0;
	if(hsize > 0){
		if(inflateInit2(&z, -MAX_WBITS) != Z_OK) return false;
	}else{
		if(inflateInit(&z) != Z_OK) return false;
	}
	z.next_in = (BYTE *)data + hsize;
	z.avail_in = (uInt)(bytes - hsize);
	z.next_out = (BYTE *)outbuf; 
	z.avail_out = (uInt)sizeof(outbuf);
	do{
		if(z.avail_out == 0){
			output.insert(output.end(), outbuf, outbuf + sizeof(outbuf));
			z.next_out = (Bytef*)outbuf;
			z.avail_out = sizeof(outbuf);
		}
		status = inflate(&z, Z_NO_FLUSH);
	}while(status == Z_OK);
	if(status != Z_STREAM_END){
		inflateEnd(&z);
		return false;
	}
	output.insert(output.end(), outbuf, outbuf + sizeof(outbuf) - z.avail_out);
	inflateEnd(&z);
	return true;
}

void SSZ_STDCALL UbytesToStr(const void* data, intptr_t bytes, UINT cp, std::wstring& output)
{
	if(bytes == 0) return;
	int len = MultiByteToWideChar(cp, 0, (LPCSTR)data, (int)bytes, NULL, 0);
	if(len <= 0) return;
	output.resize(len);
	MultiByteToWideChar(cp, 0, (LPCSTR)data, (int)bytes, &output[0], len);
}

void SSZ_STDCALL StrToUbytes(const void* data, intptr_t bytes, UINT cp, std::vector<uint8_t>& output)
{
	if(bytes == 0) return;
	int len = WideCharToMultiByte(cp, 0, (wchar_t*)data, (int)bytes / (int)sizeof(wchar_t), NULL, 0, NULL, NULL);
	if(len <= 0) return;
	output.resize(len);
	WideCharToMultiByte(cp, 0, (wchar_t*)data, (int)bytes / (int)sizeof(wchar_t), (LPSTR)output.data(), len, NULL, NULL);
}

void SSZ_STDCALL AsciiToLocal(const void* data, intptr_t bytes, std::wstring& output)
{
	if(bytes == 0) return;
	std::string tmp;
	intptr_t i, l = bytes / (intptr_t)sizeof(wchar_t);
	for(i = 0; i < l; i++) tmp += (char)((wchar_t*)data)[i];
	int len = MultiByteToWideChar(CP_THREAD_ACP, 0, tmp.data(), (int)tmp.size(), NULL, 0);
	if(len <= 0) return;
	output.resize(len);
	MultiByteToWideChar(CP_THREAD_ACP, 0, tmp.data(), (int)tmp.size(), &output[0], len);
}

void SSZ_STDCALL SetSharedString(const std::wstring& str)
{
	AutoLocker al(&g_mtx);
	g_sharedstring = str;
}

std::wstring SSZ_STDCALL GetSharedString()
{
	AutoLocker al(&g_mtx);
	return g_sharedstring;
}





#include "mydll.h"

// Data Structures
typedef struct tagXFERBUFFER2
{
	wchar_t* lpszTitle;
	LPTSTR* lpszBuffer;
	int* length;
}XFERBUFFER2;
//Prototyping
BOOL WINAPI InputBoxDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
int WINAPI WEP(int nShutDownFlag);
/*******************************************/ 
/* THE FOLLOWING FUNCTION  (WEP) IS    */ 
/* NEEDED FOR EVERY .DLL                   */ 
/*******************************************/ 
int WINAPI WEP(int nShutDownFlag)
{
	return 1;
}
/*******************************************/ 
/* THE FOLLOWING FUNCTION  (MyInputBox) IS*/ 
/* WHAT THE FOXPRO .PRG CALLS*/ 
/*******************************************/ 
/*******************************************/ 
INT_PTR WINAPI MyInputBox(HWND hWndParent, const wchar_t*  lpszTitle, LPTSTR *Buffer, int *Length)
{
	DLGPROC lpfnInputBoxDlgProc;
	XFERBUFFER2 XferBuffer;
	INT_PTR Result;
	XferBuffer.lpszTitle = (wchar_t*)lpszTitle;
	XferBuffer.lpszBuffer = Buffer;
	XferBuffer.length = Length;
	lpfnInputBoxDlgProc = (DLGPROC)MakeProcInstance((FARPROC)InputBoxDlgProc,
		ghInstance);
	Result=DialogBoxParam(ghInstance,L"INPUTDIALOG",
		hWndParent,lpfnInputBoxDlgProc,(LPARAM)&XferBuffer);
	FreeProcInstance((FARPROC)lpfnInputBoxDlgProc);
	return Result;
}
/*******************************************/ 
BOOL WINAPI InputBoxDlgProc(HWND hDlg, UINT message,WPARAM wParam, LPARAM lParam)
{
	static XFERBUFFER2 *XferBuffer;
	switch(message)
	{
	case WM_INITDIALOG :
		{
			XferBuffer = (XFERBUFFER2*)lParam;
			SetWindowText(hDlg, XferBuffer->lpszTitle);
			return TRUE; 
		}
	case WM_COMMAND :
		{
			switch(wParam)
			{
			case IDOK :
				{
					int NumChars;
					*XferBuffer->length = GetWindowTextLength(GetDlgItem(hDlg,IDD_EDIT));
					*XferBuffer->lpszBuffer = new wchar_t[*XferBuffer->length + 1];
					NumChars=GetDlgItemText(hDlg,IDD_EDIT,
						*XferBuffer->lpszBuffer,
						*XferBuffer->length + 1);
					EndDialog(hDlg,NumChars);
					break; 
				}
			case IDCANCEL :
				{
					EndDialog(hDlg, 0);
					break;
				}
			}
		}
	}
	return FALSE;
}

std::wstring SSZ_STDCALL InputStr(const std::wstring& title)
{
	wchar_t* str = NULL;
	int len = -1;
	MyInputBox(NULL, title.c_str(), &str, &len);
	if(len >= 0 && str){
		std::wstring result(str, len);
		delete[] str;
		return result;
	}
	return std::wstring();
}
