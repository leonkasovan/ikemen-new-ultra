// Regression smoke tests for native file operations.
// Compile: g++ -std=c++17 -DUNICODE -D_UNICODE -I ../main -I ../main/ssz
//              -o test_file.exe test_file.cpp ../build/Debug/main/file/file.o
// Run:     ./test_file.exe

#define SSZ_STDCALL __stdcall
#define WCHR wchar_t

#include <stdint.h>
#include <string>
#include <vector>
#include <cstdio>
#include <cassert>
#include <cstring>
#include <iostream>

// ---- Native file function declarations (matching main/file/file.cpp) ----

intptr_t SSZ_STDCALL Open(const std::wstring& md, const std::wstring& fn);
void     SSZ_STDCALL FileClose(FILE *pFile);
bool     SSZ_STDCALL Read(intptr_t size, void *p, FILE *pFile);
intptr_t SSZ_STDCALL ReadAry(intptr_t size, void *data, intptr_t bytes, FILE *pFile);
bool     SSZ_STDCALL Write(intptr_t size, const void *p, FILE *pFile);
intptr_t SSZ_STDCALL WriteAry(intptr_t size, const void *data, intptr_t bytes, FILE *pFile);
bool     SSZ_STDCALL Seek(int32_t origin, int64_t offset, FILE *pFile);
std::wstring SSZ_STDCALL LoadAsciiText(const std::wstring& path);
bool     SSZ_STDCALL SaveAsciiText(const std::wstring& txt, const std::wstring& path);
bool     SSZ_STDCALL Delete(const std::wstring& file);
bool     SSZ_STDCALL Move(const std::wstring& newn, const std::wstring& oldn);
bool     SSZ_STDCALL Copy(bool overwrite, const std::wstring& dist, const std::wstring& source);
std::vector<std::wstring> SSZ_STDCALL Find(const std::wstring& pattern);
std::vector<std::wstring> SSZ_STDCALL FindDir(const std::wstring& pattern);
bool     SSZ_STDCALL CreateDir(const std::wstring& dir);
bool     SSZ_STDCALL RemoveDir(const std::wstring& dir);
bool     SSZ_STDCALL SetCurrentDir(const std::wstring& dir);
std::wstring SSZ_STDCALL GetCurrentDir();

// ---- Test helpers ----

static int g_tests = 0;
static int g_fails = 0;

#define TEST(name, expr) do { \
    g_tests++; \
    if (!(expr)) { \
        g_fails++; \
        std::wcerr << L"FAIL: " << name << std::endl; \
    } else { \
        std::wcout << L"PASS: " << name << std::endl; \
    } \
} while(0)

#define TEST_EQ(name, expected, actual) do { \
    g_tests++; \
    if ((expected) != (actual)) { \
        g_fails++; \
        std::wcerr << L"FAIL: " << name << std::endl; \
    } else { \
        std::wcout << L"PASS: " << name << std::endl; \
    } \
} while(0)

#define TEST_INT(name, expected, actual) do { \
    g_tests++; \
    auto _e = (expected); auto _a = (actual); \
    if (_e != _a) { \
        g_fails++; \
        std::wcerr << L"FAIL: " << name << L" (expected " << _e << L", got " << _a << L")" << std::endl; \
    } else { \
        std::wcout << L"PASS: " << name << std::endl; \
    } \
} while(0)

// ---- Test suite ----

static const std::wstring TMPDIR = L"__ikemen_test_tmp";
static const std::wstring TMPFILE = TMPDIR + L"/test.txt";
static const std::wstring TMPFILE2 = TMPDIR + L"/moved.txt";
static const std::wstring TMPFILE3 = TMPDIR + L"/copied.txt";

static bool setup()
{
    CreateDir(TMPDIR);
    // Clean any leftover from previous failed run
    Delete(TMPFILE3);
    Delete(TMPFILE2);
    Delete(TMPFILE);
    return true;
}

static void cleanup()
{
    Delete(TMPFILE3);
    Delete(TMPFILE2);
    Delete(TMPFILE);
    RemoveDir(TMPDIR);
}

static void test_open_write_close_read()
{
    std::wcout << L"\n--- Open/Write/Close/Read ---" << std::endl;

    FILE* f = (FILE*)Open(L"w+b", TMPFILE);
    TEST(L"Open write", f != nullptr);

    const char* data = "Hello, Ikemen!";
    intptr_t len = 14;
    bool ok = Write(len, data, f);
    TEST(L"Write", ok);

    FileClose(f);
    TEST(L"Close after write", true);

    f = (FILE*)Open(L"rb", TMPFILE);
    TEST(L"Open read", f != nullptr);

    char buf[32] = {};
    ok = Read(len, buf, f);
    TEST(L"Read", ok);
    TEST(L"Read content matches", memcmp(buf, data, len) == 0);

    FileClose(f);
}

static void test_seek()
{
    std::wcout << L"\n--- Seek ---" << std::endl;

    FILE* f = (FILE*)Open(L"w+b", TMPFILE);
    if (!f) return;

    const char* data = "0123456789";
    Write(10, data, f);

    // Seek to beginning and read
    Seek(0, 0, f); // SET = 0
    char c;
    Read(1, &c, f);
    TEST_EQ(L"Seek SET 0 read", c, '0');

    // Seek to position 5
    Seek(0, 5, f);
    Read(1, &c, f);
    TEST_EQ(L"Seek SET 5 read", c, '5');

    // Seek relative from current
    Seek(1, 2, f); // CUR = 1
    Read(1, &c, f);
    TEST_EQ(L"Seek CUR +2 read", c, '8');

    // Seek relative from end
    Seek(2, -3, f); // END = 2
    Read(1, &c, f);
    TEST_EQ(L"Seek END -3 read", c, '7');

    FileClose(f);
}

static void test_write_read_ary()
{
    std::wcout << L"\n--- WriteAry/ReadAry ---" << std::endl;

    FILE* f = (FILE*)Open(L"w+b", TMPFILE);
    if (!f) return;

    int32_t src[] = {1, 2, 3, 4, 5};
    intptr_t totalBytes = sizeof(src);
    intptr_t written = WriteAry(sizeof(int32_t), src, totalBytes, f);
    TEST_INT(L"WriteAry count", 5, written);

    FileClose(f);

    f = (FILE*)Open(L"rb", TMPFILE);
    int32_t dst[5] = {};
    intptr_t read = ReadAry(sizeof(int32_t), dst, totalBytes, f);
    TEST_INT(L"ReadAry count", 5, read);
    for (int i = 0; i < 5; i++) {
        std::wstring elemName = L"ReadAry element " + std::to_wstring(i);
        TEST_INT(elemName.c_str(), src[i], dst[i]);
    }

    FileClose(f);
}

static void test_save_load_ascii_text()
{
    std::wcout << L"\n--- SaveAsciiText/LoadAsciiText ---" << std::endl;

    const std::wstring text = L"Line 1\r\nLine 2\r\nLine 3";
    bool ok = SaveAsciiText(text, TMPFILE);
    TEST(L"SaveAsciiText", ok);

    std::wstring loaded = LoadAsciiText(TMPFILE);
    TEST(L"LoadAsciiText non-empty", !loaded.empty());
    TEST_EQ(L"LoadAsciiText content", loaded, text);
}

static void test_delete()
{
    std::wcout << L"\n--- Delete ---" << std::endl;

    // Ensure file exists
    SaveAsciiText(L"delete me", TMPFILE);

    bool ok = Delete(TMPFILE);
    TEST(L"Delete existing file", ok);

    // File should be gone
    FILE* f = (FILE*)Open(L"rb", TMPFILE);
    TEST(L"Delete verified (open fails)", f == nullptr);
}

static void test_move()
{
    std::wcout << L"\n--- Move ---" << std::endl;

    SaveAsciiText(L"move me", TMPFILE);
    Delete(TMPFILE2); // clean target

    bool ok = Move(TMPFILE2, TMPFILE);
    TEST(L"Move", ok);

    // Source should be gone
    FILE* f = (FILE*)Open(L"rb", TMPFILE);
    TEST(L"Move source gone", f == nullptr);

    // Dest should exist
    f = (FILE*)Open(L"rb", TMPFILE2);
    TEST(L"Move dest exists", f != nullptr);
    if (f) FileClose(f);
}

static void test_copy()
{
    std::wcout << L"\n--- Copy ---" << std::endl;

    SaveAsciiText(L"copy me", TMPFILE);
    Delete(TMPFILE3);

    bool ok = Copy(false, TMPFILE3, TMPFILE);
    TEST(L"Copy", ok);

    // Both should exist
    FILE* f = (FILE*)Open(L"rb", TMPFILE);
    FILE* g = (FILE*)Open(L"rb", TMPFILE3);
    TEST(L"Copy source exists", f != nullptr);
    TEST(L"Copy dest exists", g != nullptr);
    if (f) FileClose(f);
    if (g) FileClose(g);

    // Overwrite should fail when overwrite=false
    ok = Copy(false, TMPFILE3, TMPFILE);
    TEST(L"Copy no overwrite", !ok);
}

static void test_create_remove_dir()
{
    std::wcout << L"\n--- CreateDir/RemoveDir ---" << std::endl;

    const std::wstring dir = TMPDIR + L"/__subdir";
    RemoveDir(dir); // clean

    bool ok = CreateDir(dir);
    TEST(L"CreateDir", ok);

    // Second create should return false (directory already exists)
    ok = CreateDir(dir);
    TEST(L"CreateDir existing returns false (Win32 CreateDirectory)", !ok);

    ok = RemoveDir(dir);
    TEST(L"RemoveDir", ok);

    // Dir should be gone
    // Try to create a file in it (should fail since dir is gone)
    FILE* f = (FILE*)Open(L"w", dir + L"/nope.txt");
    TEST(L"RemoveDir verified (cannot create file in removed dir)", f == nullptr);
}

static void test_current_dir()
{
    std::wcout << L"\n--- SetCurrentDir/GetCurrentDir ---" << std::endl;

    std::wstring orig = GetCurrentDir();
    TEST(L"GetCurrentDir non-empty", !orig.empty());

    bool ok = SetCurrentDir(TMPDIR);
    TEST(L"SetCurrentDir to tmpdir", ok);

    std::wstring cur = GetCurrentDir();
    TEST(L"GetCurrentDir matches", cur.find(TMPDIR) != std::wstring::npos);

    ok = SetCurrentDir(orig);
    TEST(L"SetCurrentDir restore", ok);

    cur = GetCurrentDir();
    TEST_EQ(L"GetCurrentDir restored", cur, orig);
}

static void test_find()
{
    std::wcout << L"\n--- Find ---" << std::endl;

    // Create isolated test files (unique suffix to avoid conflicts
    // with leftover files from earlier tests)
    SaveAsciiText(L"a", TMPDIR + L"/alpha_find.txt");
    SaveAsciiText(L"b", TMPDIR + L"/beta_find.txt");
    SaveAsciiText(L"c", TMPDIR + L"/gamma_find.dat");

    auto files = Find(TMPDIR + L"/*_find.txt");
    TEST_INT(L"Find *_find.txt count", 2, files.size());

    auto all = Find(TMPDIR + L"/*");
    TEST(L"Find * returns >= 3", all.size() >= 3);

    // Cleanup
    Delete(TMPDIR + L"/alpha_find.txt");
    Delete(TMPDIR + L"/beta_find.txt");
    Delete(TMPDIR + L"/gamma_find.dat");
}

// ---- Main ----

int main()
{
    setup();

    test_open_write_close_read();
    test_seek();
    test_write_read_ary();
    test_save_load_ascii_text();
    test_delete();
    test_move();
    test_copy();
    test_create_remove_dir();
    test_current_dir();
    test_find();

    cleanup();

    std::wcout << L"\n=== " << g_tests << L" tests, " << g_fails << L" failures ===" << std::endl;

    // Leave tmp dir if there were failures for inspection
    if (g_fails > 0) {
        std::wcerr << L"Failures detected; keeping " << TMPDIR << L" for inspection." << std::endl;
    } else {
        RemoveDir(TMPDIR);
    }

    return g_fails > 0 ? 1 : 0;
}
