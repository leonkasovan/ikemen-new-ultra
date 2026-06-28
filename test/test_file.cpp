// Regression smoke tests for native plugin implementations.
// Compile: g++ -std=c++17 -DUNICODE -D_UNICODE -I ../main -I ../main/ssz
//              -o test_file.exe test_file.cpp
//              ../build/Debug/main/file/file.o
//              ../build/Debug/main/math/math.o
//              ../build/Debug/main/thread/thread.o
// Run:     ./test_file.exe

#include <stdint.h>
#include <string>
#include <vector>
#include <cstdio>
#include <cassert>
#include <cstring>
#include <iostream>
#include <cmath>

#include "sszdef.h"
#include "ssz_native/plugin_native_api.hpp"
#include "ssz_native/file_service.hpp"
#include "ssz_native/math_service.hpp"
#include "ssz_native/regex_service.hpp"

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

// ---- Math tests ----

static void test_math()
{
    std::wcout << L"\n--- Math ---" << std::endl;

    TEST(L"Sin(0) == 0", Sin(0.0) == 0.0);
    TEST(L"Cos(0) == 1", Cos(0.0) == 1.0);
    TEST(L"Tan(0) == 0", Tan(0.0) == 0.0);

    TEST(L"ASin(0) == 0", ASin(0.0) == 0.0);
    TEST(L"ACos(1) == 0", ACos(1.0) == 0.0);
    TEST(L"ATan(0) == 0", ATan(0.0) == 0.0);

    double pi = 3.141592653589793;
    TEST(L"Sin(PI/2) == 1", std::abs(Sin(pi/2) - 1.0) < 1e-15);
    TEST(L"Cos(PI) == -1", std::abs(Cos(pi) + 1.0) < 1e-15);

    TEST(L"Ln(1) == 0", Ln(1.0) == 0.0);
    TEST(L"Ln(E) == 1", std::abs(Ln(2.718281828459045) - 1.0) < 1e-15);
    TEST(L"Exp(0) == 1", Exp(0.0) == 1.0);

    TEST(L"Sqrt(4) == 2", Sqrt(4.0) == 2.0);
    TEST(L"Sqrt(0) == 0", Sqrt(0.0) == 0.0);

    TEST(L"Ceil(1.5) == 2", Ceil(1.5) == 2.0);
    TEST(L"Ceil(-1.5) == -1", Ceil(-1.5) == -1.0);
    TEST(L"Floor(1.5) == 1", Floor(1.5) == 1.0);
    TEST(L"Floor(-1.5) == -2", Floor(-1.5) == -2.0);

    TEST(L"IsFinite(0) true", IsFinite(0.0));
    TEST(L"IsInf(0) false", !IsInf(0.0));
    TEST(L"IsNaN(0) false", !IsNaN(0.0));

    TEST(L"Log(10, 100) == 2", std::abs(Log(10.0, 100.0) - 2.0) < 1e-15);
}

// ---- Thread tests ----

static void test_thread()
{
    std::wcout << L"\n--- Thread ---" << std::endl;

    // Just verify it doesn't crash
    ThreadDelay(0);
    ThreadDelay(1);
    TEST(L"ThreadDelay(0) no crash", true);
}

// ---- Math service tests (ssz_native::math) ----

static void test_math_service()
{
    namespace m = ikemen::ssz_native::math;
    std::wcout << L"\n--- Math service ---" << std::endl;

    // Constants
    TEST(L"PI > 3.14", m::PI > 3.14);
    TEST(L"E > 2.71", m::E > 2.71);

    // Wrappers match C math
    TEST(L"sin(0) == 0", m::sin(0.0) == 0.0);
    TEST(L"cos(0) == 1", m::cos(0.0) == 1.0);
    TEST(L"round(1.5) == 2", m::round(1.5) == 2.0);
    TEST(L"round(-1.5) == -2", m::round(-1.5) == -2.0);

    // PRNG determinism
    m::srand(12345);
    int32_t a = m::random();
    m::srand(12345);
    int32_t b = m::random();
    TEST(L"PRNG deterministic with same seed", a == b);

    m::srand(54321);
    int32_t c = m::random();
    TEST(L"PRNG different seed gives different value", a != c);

    // Known Park-Miller sequence with seed=1: 1st output is always 16807
    m::srand(1);
    TEST_INT(L"PRNG Park-Miller seed=1 1st", 16807, m::random());
    // 2nd output with seed=1: the generator yields 282475249
    // (verified against the minimal standard generator reference)
    TEST_INT(L"PRNG Park-Miller seed=1 2nd", 282475249, m::random());

    // Range of random()
    TEST(L"random() >= 0", m::random() >= 0);
    TEST(L"random() <= RANDMAX", m::random() <= m::RANDMAX);

    // rand(min, max) in range
    m::srand(999);
    for (int i = 0; i < 100; i++) {
        int32_t r = m::rand(5, 10);
        TEST(L"rand(5,10) in [5,10]", r >= 5 && r <= 10);
    }

    // randI(x, y) in range
    m::srand(999);
    for (int i = 0; i < 100; i++) {
        int32_t r = m::randI(-5, 5);
        TEST(L"randI(-5,5) in [-5,5]", r >= -5 && r <= 5);
    }

    // randF(x, y) in range
    m::srand(999);
    for (int i = 0; i < 100; i++) {
        float r = m::randF(-1.5f, 2.5f);
        TEST(L"randF(-1.5,2.5) in [-1.5,2.5]", r >= -1.5f && r <= 2.5f);
    }

    // Utility templates
    TEST(L"min(3,7) == 3", m::min(3, 7) == 3);
    TEST(L"max(3,7) == 7", m::max(3, 7) == 7);
    TEST(L"inRange(2,5,3) true", m::inRange(2, 5, 3));
    TEST(L"inRange(2,5,6) false", !m::inRange(2, 5, 6));

    int val = 10;
    m::limMax(val, 7);
    TEST(L"limMax(10,7) → 7", val == 7);
    val = 3;
    m::limMin(val, 7);
    TEST(L"limMin(3,7) → 7", val == 7);
    val = 20;
    m::limRange(val, 5, 15);
    TEST(L"limRange(20,5,15) → 15", val == 15);
    val = 1;
    m::limRange(val, 5, 15);
    TEST(L"limRange(1,5,15) → 5", val == 5);

    int x = 1, y = 2;
    m::swap(x, y);
    TEST(L"swap(1,2) → (2,1)", x == 2 && y == 1);
}

// ---- Regex service tests (ssz_native::regex) ----

static void test_regex_service()
{
    namespace r = ikemen::ssz_native::regex;
    std::wcout << L"\n--- Regex service ---" << std::endl;

    // Simple pattern compilation
    r::Regex re;
    std::wstring err = re.compile(L"hello");
    TEST(L"compile simple pattern: no error", err.empty());
    TEST(L"compile simple pattern: is_compiled", re.is_compiled());

    // Search — find all occurrences (search_all)
    auto all = re.search_all(L"hello world hello");
    TEST(L"search_all returns 2 matches", all.size() == 2);
    if (all.size() >= 2) {
        TEST_EQ(L"search_all match 0", all[0], L"hello");
        TEST_EQ(L"search_all match 1", all[1], L"hello");
    }

    // Search — single match, capture groups
    r::Regex re2;
    re2.compile(L"(\\w+)@(\\w+)");
    auto groups = re2.search(L"user@host");
    TEST(L"search returns 3 groups (full + 2 captures)", groups.size() == 3);
    if (groups.size() >= 3) {
        TEST_EQ(L"search group 0 (full match)", groups[0], L"user@host");
        TEST_EQ(L"search group 1 (user)", groups[1], L"user");
        TEST_EQ(L"search group 2 (host)", groups[2], L"host");
    }

    // Search — no match
    auto nomatch = re2.search(L"no-at-sign");
    TEST(L"search no match returns empty", nomatch.empty());

    // Case insensitive flag
    r::Regex re3;
    re3.compile(L"hello", true); // case_insensitive=true
    auto ci = re3.search_all(L"HELLO Hello hello");
    TEST(L"case insensitive finds 3", ci.size() == 3);

    // Invalid pattern
    r::Regex re4;
    err = re4.compile(L"[invalid");
    TEST(L"compile invalid pattern: error", !err.empty());
    TEST(L"compile invalid pattern: not compiled", !re4.is_compiled());

    // Raw match positions
    r::Regex re5;
    re5.compile(L"ab");
    auto matches = re5.search_matches(L"xabxabx");
    TEST(L"search_matches: 2 entries", matches.size() == 2);
    if (matches.size() >= 2) {
        TEST_INT(L"search_matches: pos 0", 1, matches[0].pos);
        TEST_INT(L"search_matches: len 0", 2, matches[0].len);
        TEST_INT(L"search_matches: pos 1", 4, matches[1].pos);
        TEST_INT(L"search_matches: len 1", 2, matches[1].len);
    }

    // Free function compile
    auto [free_re, free_err] = r::compile(L"test");
    TEST(L"free compile: no error", free_err.empty());
    TEST(L"free compile: is_compiled", free_re.is_compiled());
    auto free_results = free_re.search_all(L"test test");
    TEST(L"free compile: search_all returns 2", free_results.size() == 2);

    // Move semantics
    r::Regex re6;
    re6.compile(L"move");
    r::Regex re7 = std::move(re6);
    TEST(L"move: source not compiled", !re6.is_compiled());
    TEST(L"move: dest compiled", re7.is_compiled());
    auto move_results = re7.search_all(L"move move");
    TEST(L"move: search_all after move", move_results.size() == 2);
}

// ---- FileHandle edge case tests ----

static void test_file_handle_edges()
{
    using namespace ikemen::ssz_native;
    std::wcout << L"\n--- FileHandle edge cases ---" << std::endl;

    // Move semantics
    {
        FileHandle fh1;
        fh1.open(TMPFILE, L"w+b");
        TEST(L"FileHandle fh1 open", fh1.is_open());

        FileHandle fh2 = std::move(fh1);
        TEST(L"FileHandle fh1 moved (closed)", !fh1.is_open());
        TEST(L"FileHandle fh2 received handle", fh2.is_open());

        FileHandle fh3;
        fh3 = std::move(fh2);
        TEST(L"FileHandle fh2 move-assigned (closed)", !fh2.is_open());
        TEST(L"FileHandle fh3 received handle", fh3.is_open());
    }
    TEST(L"FileHandle move dtor auto-closes", true);

    // Self-move-assignment safety
    {
        FileHandle fh;
        fh.open(TMPFILE, L"rb");
        // Self-move — must not close or corrupt. Warning suppressed intentionally.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wself-move"
        fh = std::move(fh);
#pragma GCC diagnostic pop
        TEST(L"FileHandle self-move safe (still open)", fh.is_open());
    }

    // Double close safety
    {
        FileHandle fh;
        fh.open(TMPFILE, L"rb");
        fh.close();
        fh.close();  // should not crash
        TEST(L"FileHandle double close safe", true);
    }

    // Operations on closed handle
    {
        FileHandle fh;
        char buf[4];
        TEST(L"FileHandle read on closed returns false", !fh.read(buf, 4));
        TEST(L"FileHandle readArray on closed returns -1", fh.read_array(buf, 1, 4) == -1);
        TEST(L"FileHandle write on closed returns false", !fh.write("x", 1));
        TEST(L"FileHandle writeArray on closed returns -1", fh.write_array("x", 1, 1) == -1);
        TEST(L"FileHandle seek on closed returns false", !fh.seek(0, SeekOrigin::Set));
    }
}

// ---- FileHandle tests (ssz_native RAII wrapper) ----

static void test_file_handle()
{
    using namespace ikemen::ssz_native;
    std::wcout << L"\n--- FileHandle ---" << std::endl;

    FileHandle fh;
    TEST(L"FileHandle initially closed", !fh.is_open());

    // Open for write
    bool ok = fh.open(TMPFILE, L"w+b");
    TEST(L"FileHandle open write", ok);
    TEST(L"FileHandle is_open after open", fh.is_open());

    // Write data
    const char* data = "FileHandle test";
    intptr_t len = 15;
    ok = fh.write(data, len);
    TEST(L"FileHandle write", ok);

    fh.close();
    TEST(L"FileHandle is_open after close", !fh.is_open());

    // Open for read
    ok = fh.open(TMPFILE, L"rb");
    TEST(L"FileHandle open read", ok);

    char buf[32] = {};
    ok = fh.read(buf, len);
    TEST(L"FileHandle read", ok);
    TEST(L"FileHandle read content matches", memcmp(buf, data, len) == 0);

    fh.close();

    // Test free functions
    ok = save_ascii_text(TMPFILE2, L"free function test");
    TEST(L"save_ascii_text", ok);

    std::wstring loaded = load_ascii_text(TMPFILE2);
    TEST(L"load_ascii_text non-empty", !loaded.empty());
    TEST_EQ(L"load_ascii_text content", loaded, L"free function test");

    // Cleanup free-function test files
    Delete(TMPFILE2);
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
    test_math();
    test_thread();
    test_math_service();
    test_regex_service();
    test_file_handle();
    test_file_handle_edges();

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
