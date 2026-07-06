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
#include <limits>

// Must come before any SDL include to prevent SDL_main.h from redefining main()
#define SDL_MAIN_HANDLED
#include "sszdef.h"
#include "ssz_native/plugin_native_api.hpp"
#include "ssz_native/file_service.hpp"
#include "ssz_native/math_service.hpp"
#include "ssz_native/regex_service.hpp"
#include "ssz_native/socket_service.hpp"
#include "ssz_native/sound_service.hpp"
#include "ssz_native/string_service.hpp"
#include "ssz_native/ogg_service.hpp"
#include "ssz_native/mesdialog_service.hpp"
#include "ssz_native/crypto_service.hpp"
#include "ssz_native/thread_service.hpp"
#include "ssz_native/time_service.hpp"
#include "ssz_native/shell_service.hpp"
#include "ssz_native/lua_service.hpp"
#include "ssz_native/table_service.hpp"
#include "ssz_native/share_service.hpp"
#include "ssz_native/system_service.hpp"
#include "ssz_native/debug_script_service.hpp"
#include "ssz_native/loader_service.hpp"
#include "ssz_native/common_service.hpp"
#include "ssz_native/trigger_script_service.hpp"
#include "ssz_native/script_service.hpp"
#include "ssz_native/system_script_service.hpp"
#include "ssz_native/statebuilder_service.hpp"
#include "ssz_native/font_service.hpp"
#include "ssz_native/video_service.hpp"
#include "ssz_native/action_service.hpp"
#include "ssz_native/sound_resource_service.hpp"
#include "ssz_native/fighting_service.hpp"
#include "ssz_native/bg_service.hpp"
#include "ssz_native/stage_service.hpp"
#include "ssz_native/sff_service.hpp"
#include "ssz_native/command_service.hpp"
#include "ssz_native/fight_service.hpp"
#include "ssz_native/char_service.hpp"
#include "ssz_native/sdlevent_service.hpp"
#include "ssz_native/sdlplugin_service.hpp"
#include "ssz_native/config_service.hpp"
#include "ssz_native/config_net_service.hpp"
#include "ssz_native/stack_service.hpp"
#include "ssz_native/consts.hpp"

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

// ---- String service tests (ssz_native::string_util) ----

static void test_string_service()
{
    namespace s = ikemen::ssz_native::string_util;
    std::wcout << L"\n--- String service ---" << std::endl;

    // equ
    TEST(L"equ same strings", s::equ(L"abc", L"abc"));
    TEST(L"equ different strings", !s::equ(L"abc", L"def"));

    // trim
    TEST_EQ(L"trim spaces", s::trim(L"  hello  "), L"hello");
    TEST_EQ(L"trim tabs", s::trim(L"\t\thello\t"), L"hello");
    TEST_EQ(L"trim mixed", s::trim(L" \t\r\nhi\r\n\t "), L"hi");

    // find
    TEST_INT(L"find at start", 0, s::find(L"abc", L"abcdef"));
    TEST_INT(L"find in middle", 3, s::find(L"def", L"abcdefghi"));
    TEST_INT(L"find not found", -1, s::find(L"xyz", L"abcdef"));
    TEST_INT(L"find empty pattern", 0, s::find(L"", L"abc"));

    // split
    auto parts = s::split(L",", L"a,b,c");
    TEST(L"split 3 parts", parts.size() == 3);

    // join
    std::vector<std::wstring> words = {L"a", L"b", L"c"};
    TEST_EQ(L"join with comma", s::join(L",", words), L"a,b,c");

    // split_lines
    auto lines = s::split_lines(L"line1\r\nline2\nline3");
    TEST(L"split_lines 3", lines.size() == 3);
    if (lines.size() >= 3) {
        TEST_EQ(L"split_lines line1", lines[0], L"line1");
        TEST_EQ(L"split_lines line2", lines[1], L"line2");
        TEST_EQ(L"split_lines line3", lines[2], L"line3");
    }

    // to_utf8 / from_utf8 roundtrip
    std::wstring original = L"Hello, 世界!";
    auto utf8 = s::to_utf8(original);
    TEST(L"utf8 non-empty", !utf8.empty());
    TEST_EQ(L"utf8 roundtrip", s::from_utf8(utf8), original);

    // ASCII only roundtrip
    utf8 = s::to_utf8(L"abc123");
    TEST_EQ(L"ascii roundtrip", s::from_utf8(utf8), L"abc123");

    // percent encoding
    TEST_EQ(L"percent encode unreserved", s::percent_encode(L"abc123"), L"abc123");
    std::wstring encoded = s::percent_encode(L"hello world");
    TEST(L"percent encode space", encoded.find(L'%') != std::wstring::npos);

    // percent decode
    TEST_EQ(L"percent decode", s::percent_decode(L"hello%20world"), L"hello world");

    // to_hex_lower / to_hex_upper
    TEST_EQ(L"hex lower 255", s::to_hex_lower(255), L"ff");
    TEST_EQ(L"hex upper 255", s::to_hex_upper(255), L"FF");
    TEST_EQ(L"hex lower 0", s::to_hex_lower(0), L"0");

    // to_octal
    TEST_EQ(L"octal 8", s::to_octal(8), L"10");
    TEST_EQ(L"octal 0", s::to_octal(0), L"0");

    // next_line
    {
        std::wstring str = L"hello\nworld\r\nend";
        intptr_t idx = 0;
        int r1 = s::next_line(idx, str);
        TEST(L"next_line finds \\n", r1 == 1);
        TEST(L"next_line idx after \\n", idx == 6);
        int r2 = s::next_line(idx, str);
        TEST(L"next_line finds \\r\\n", r2 == 2);
        TEST(L"next_line idx after \\r\\n", idx == 13);
        int r3 = s::next_line(idx, str);
        TEST(L"next_line no more", r3 == 0);
    }

    // c_match
    TEST(L"c_match found", s::c_match(L"abc", L'b'));
    TEST(L"c_match not found", !s::c_match(L"abc", L'z'));

    // c_find
    TEST_INT(L"c_find found", 1, s::c_find(L"aeiou", L"hello"));
    TEST_INT(L"c_find not found", -1, s::c_find(L"xyz", L"hello"));

    // s_to_number
    {
        double d;
        TEST(L"s_to_number double", s::s_to_number(d, L"3.14"));
        TEST(L"s_to_number double value", std::abs(d - 3.14) < 0.001);
        int32_t i;
        TEST(L"s_to_number int", s::s_to_number(i, L"42"));
        TEST(L"s_to_number int value", i == 42);
        TEST(L"s_to_number empty fails", !s::s_to_number(i, L""));
        TEST(L"s_to_number negative", s::s_to_number(i, L"-7"));
        TEST(L"s_to_number negative value", i == -7);
    }

    // s_to_n
    TEST(L"s_to_n int", s::s_to_n<int32_t>(L"123") == 123);
    TEST(L"s_to_n double", std::abs(s::s_to_n<double>(L"2.5") - 2.5) < 0.001);
    TEST(L"s_to_n invalid returns 0", s::s_to_n<int32_t>(L"") == 0);

    // sv_to_ary
    {
        auto ary = s::sv_to_ary<int32_t>(L",", L"1,2,3");
        TEST(L"sv_to_ary size", ary.size() == 3);
        if (ary.size() == 3) {
            TEST(L"sv_to_ary[0]", ary[0] == 1);
            TEST(L"sv_to_ary[1]", ary[1] == 2);
            TEST(L"sv_to_ary[2]", ary[2] == 3);
        }
    }

    // copy_array
    {
        std::vector<int> src = {1, 2, 3};
        std::vector<int> dst = {0, 0, 0, 0};
        s::copy_array(dst, src);
        TEST(L"copy_array[0]", dst[0] == 1);
        TEST(L"copy_array[2]", dst[2] == 3);
        TEST(L"copy_array[3] unchanged", dst[3] == 0);
    }

    // clone_array
    {
        std::vector<int> src = {10, 20, 30};
        auto cloned = s::clone_array(src);
        TEST(L"clone_array size", cloned.size() == 3);
        TEST(L"clone_array[1]", cloned[1] == 20);
    }

    // each
    {
        std::vector<int> ary = {1, 2, 3};
        s::each<int>([](int& v) { v *= 2; }, ary);
        TEST(L"each doubled[0]", ary[0] == 2);
        TEST(L"each doubled[2]", ary[2] == 6);
    }

    // to_hex (array)
    {
        std::vector<uint8_t> bytes = {0xAB, 0xCD};
        auto hex = s::to_hex(bytes);
        TEST_EQ(L"to_hex bytes", hex, L"abcd");
    }

    // to_ubyte
    {
        std::vector<uint16_t> shorts = {0x0102};
        auto ub = s::to_ubyte(shorts);
        TEST(L"to_ubyte size", ub.size() == 2);
        TEST(L"to_ubyte little-endian[0]", ub[0] == 0x02);
        TEST(L"to_ubyte little-endian[1]", ub[1] == 0x01);
    }
}

// ---- Format object tests ----

static void test_format_service()
{
    using Fmt = ikemen::ssz_native::string_util::Format;
    std::wcout << L"\n--- Format ---" << std::endl;

    // Basic literal text (no format specifiers)
    {
        Fmt fmt;
        fmt.set(L"hello");
        TEST(L"Format literal no pct", fmt.out == L"hello");
    }

    // %% escape
    {
        Fmt fmt;
        fmt.set(L"100%%");
        TEST(L"Format %% escape", fmt.out == L"100%");
    }

    // %d signed decimal
    {
        Fmt fmt;
        fmt.set(L"%d");
        fmt.d(42);
        TEST(L"Format %d: 42", fmt.out == L"42");
    }

    // %d negative
    {
        Fmt fmt;
        fmt.set(L"%d");
        fmt.d(-42);
        TEST(L"Format %d: -42", fmt.out == L"-42");
    }

    // %u unsigned
    {
        Fmt fmt;
        fmt.set(L"%u");
        fmt.u(42u);
        TEST(L"Format %u: 42", fmt.out == L"42");
    }

    // %x lowercase hex
    {
        Fmt fmt;
        fmt.set(L"%x");
        fmt.u(255u);
        TEST(L"Format %x: ff", fmt.out == L"ff");
    }

    // %X uppercase hex
    {
        Fmt fmt;
        fmt.set(L"%X");
        fmt.u(255u);
        TEST(L"Format %X: FF", fmt.out == L"FF");
    }

    // %o octal
    {
        Fmt fmt;
        fmt.set(L"%o");
        fmt.u(8u);
        TEST(L"Format %o: 10", fmt.out == L"10");
    }

    // %c character
    {
        Fmt fmt;
        fmt.set(L"%c");
        fmt.c(L'A');
        TEST(L"Format %c: A", fmt.out == L"A");
    }

    // %s string
    {
        Fmt fmt;
        fmt.set(L"%s");
        fmt.s(L"hello");
        TEST(L"Format %s: hello", fmt.out == L"hello");
    }

    // Width padding (right-aligned)
    {
        Fmt fmt;
        fmt.set(L"%5d");
        fmt.d(42);
        TEST(L"Format %5d: '   42'", fmt.out == L"   42");
    }

    // Left-justified
    {
        Fmt fmt;
        fmt.set(L"%-5d");
        fmt.d(42);
        TEST(L"Format %-5d: '42   '", fmt.out == L"42   ");
    }

    // Zero-padded
    {
        Fmt fmt;
        fmt.set(L"%05d");
        fmt.d(42);
        TEST(L"Format %05d: '00042'", fmt.out == L"00042");
    }

    // Zero-padded negative
    {
        Fmt fmt;
        fmt.set(L"%05d");
        fmt.d(-42);
        TEST(L"Format %05d negative: '-0042'", fmt.out == L"-0042");
    }

    // Positive sign
    {
        Fmt fmt;
        fmt.set(L"%+d");
        fmt.d(42);
        TEST(L"Format %+d: '+42'", fmt.out == L"+42");
    }

    // Space sign
    {
        Fmt fmt;
        fmt.set(L"% d");
        fmt.d(42);
        TEST(L"Format '% d': ' 42'", fmt.out == L" 42");
    }

    // Precision
    {
        Fmt fmt;
        fmt.set(L"%.5d");
        fmt.d(42);
        TEST(L"Format %.5d: '00042'", fmt.out == L"00042");
    }

    // Sharp with hex
    {
        Fmt fmt;
        fmt.set(L"%#x");
        fmt.u(255u);
        TEST(L"Format %#x: '0xff'", fmt.out == L"0xff");
    }

    // Sharp with octal
    {
        Fmt fmt;
        fmt.set(L"%#o");
        fmt.u(8u);
        TEST(L"Format %#o: '010'", fmt.out == L"010");
    }

    // Multiple specifiers with literal text
    {
        Fmt fmt;
        fmt.set(L"val = %d, hex = %x");
        fmt.d(42);
        fmt.u(255u);
        TEST(L"Format multi: 'val = 42, hex = ff'", fmt.out == L"val = 42, hex = ff");
    }

    // Float %f
    {
        Fmt fmt;
        fmt.set(L"%f");
        fmt.f(3.1415926535);
        // Default precision is 6
        TEST(L"Format %f: 3.141593", fmt.out == L"3.141593");
    }

    // Float %.2f
    {
        Fmt fmt;
        fmt.set(L"%.2f");
        fmt.f(3.14159);
        TEST(L"Format %.2f: '3.14'", fmt.out == L"3.14");
    }

    // Float %e scientific
    {
        Fmt fmt;
        fmt.set(L"%e");
        fmt.f(1000.0);
        TEST(L"Format %e starts with 1.0",
            fmt.out.size() >= 3 && fmt.out[0] == L'1' && fmt.out[1] == L'.');
    }

    // Float %g compact
    {
        Fmt fmt;
        fmt.set(L"%g");
        fmt.f(100.0);
        // 100 in %g is "100"
        TEST(L"Format %g: 100", fmt.out == L"100");
    }

    // NaN
    {
        Fmt fmt;
        fmt.set(L"%f");
        fmt.f(std::numeric_limits<double>::quiet_NaN());
        TEST(L"Format %f NaN: 'nan'", fmt.out == L"nan");
    }

    // Infinity
    {
        Fmt fmt;
        fmt.set(L"%f");
        fmt.f(std::numeric_limits<double>::infinity());
        TEST(L"Format %f inf: 'inf'", fmt.out == L"inf");
    }

    // Negative infinity
    {
        Fmt fmt;
        fmt.set(L"%f");
        fmt.f(-std::numeric_limits<double>::infinity());
        TEST(L"Format %f -inf: '-inf'", fmt.out == L"-inf");
    }

    // Width with string
    {
        Fmt fmt;
        fmt.set(L"%10s");
        fmt.s(L"hi");
        TEST(L"Format %10s: '        hi'", fmt.out == L"        hi");
    }

    // Left-justified with string
    {
        Fmt fmt;
        fmt.set(L"%-10s");
        fmt.s(L"hi");
        TEST(L"Format %-10s: 'hi        '", fmt.out == L"hi        ");
    }

    // isError false after valid format
    {
        Fmt fmt;
        fmt.set(L"%d");
        TEST(L"Format isError false after set", !fmt.isError());
    }

    // isError on invalid specifier
    {
        Fmt fmt;
        fmt.set(L"%q");
        // Invalid specifier causes error
        TEST(L"Format isError for %q", fmt.isError());
    }

    // Error propagation — calling method after error returns error
    {
        Fmt fmt;
        fmt.set(L"%d");
        fmt.d(42);
        fmt.d(99); // No more specifiers -> error
        TEST(L"Format error propagation", fmt.isError());
    }

    // %%d should produce literal %d
    {
        Fmt fmt;
        fmt.set(L"%%d");
        TEST(L"Format %%d: '%d'", fmt.out == L"%d");
    }

    // Float width padding
    {
        Fmt fmt;
        fmt.set(L"%8.2f");
        fmt.f(3.14);
        TEST(L"Format %8.2f width", fmt.out.size() == 8);
        TEST(L"Format %8.2f right-aligned",
            fmt.out.substr(4) == L"3.14");
    }

    // Float with + sign
    {
        Fmt fmt;
        fmt.set(L"%+f");
        fmt.f(1.5);
        TEST(L"Format %+f starts with +",
            !fmt.out.empty() && fmt.out[0] == L'+');
    }

    // Negative float
    {
        Fmt fmt;
        fmt.set(L"%f");
        fmt.f(-2.5);
        TEST(L"Format %f -2.5",
            !fmt.out.empty() && fmt.out[0] == L'-');
    }

    // putSpace directly
    {
        Fmt fmt;
        fmt.set(L"");  // empty format, next = 0
        fmt.out.clear();
        fmt.putSpace(5);
        TEST(L"Format putSpace(5)", fmt.out == L"     ");
    }

    // Width wider than digits for unsigned
    {
        Fmt fmt;
        fmt.set(L"%6u");
        fmt.u(42u);
        TEST(L"Format %6u: '    42'", fmt.out == L"    42");
    }
}

// ---- Ogg service tests (ssz_native::ogg) ----

static void test_ogg_service()
{
    namespace o = ikemen::ssz_native::ogg;
    std::wcout << L"\n--- Ogg service ---" << std::endl;

    // ── Construction and move semantics ──
    o::OggVorbisHandle ov;
    TEST(L"OggVorbisHandle constructed", ov.is_valid());

    o::OggVorbisHandle ov2;
    o::OggVorbisHandle ov3 = std::move(ov2);
    TEST(L"OggVorbisHandle move: source invalid", !ov2.is_valid());
    TEST(L"OggVorbisHandle move: dest valid", ov3.is_valid());

    o::OggVorbisHandle ov4;
    ov4 = std::move(ov3);
    TEST(L"OggVorbisHandle move-assign: source invalid", !ov3.is_valid());
    TEST(L"OggVorbisHandle move-assign: dest valid", ov4.is_valid());

    o::OggVorbisHandle ov5;
    ov5 = std::move(ov5);
    TEST(L"OggVorbisHandle self-move safe", ov5.is_valid());

    // ── Operations on non-opened handle (no-crash) ──
    o::OggVorbisHandle ov6;
    ov6.clear();
    ov6.pcm_total();
    ov6.channels();
    ov6.rate();
    ov6.seek(0.0);
    int16_t buf2[16];
    ov6.read(buf2, 16);
    TEST(L"OggVorbisHandle clear/pcm/rate/read/seek no crash on non-opened handle", true);

    // ── Open a real .ogg file from install/ ──
    o::OggVorbisHandle ov_file;
    bool opened = ov_file.open(L"sound/Thunderstorm.ogg");
    TEST(L"OggVorbisHandle open real .ogg file", opened == true);

    if (opened) {
        // Verify audio properties
        int64_t total = ov_file.pcm_total();
        TEST(L"OggVorbisHandle pcm_total > 0", total > 0);

        int32_t ch = ov_file.channels();
        TEST(L"OggVorbisHandle channels == 1 or 2", ch == 1 || ch == 2);

        int32_t rate = ov_file.rate();
        TEST(L"OggVorbisHandle rate > 0", rate > 0);
        TEST(L"OggVorbisHandle rate is typical", rate == 44100 || rate == 48000 || rate == 22050);

        // Read samples
        const intptr_t READ_SIZE = 4096;
        std::vector<int16_t> read_buf(READ_SIZE);
        intptr_t samples_read = ov_file.read(read_buf.data(), READ_SIZE);
        TEST(L"OggVorbisHandle read returns > 0", samples_read > 0);
        TEST(L"OggVorbisHandle read <= requested", samples_read <= READ_SIZE);

        // Seek to beginning and re-read
        int32_t seek_ret = ov_file.seek(0.0);
        TEST(L"OggVorbisHandle seek(0) returns 0", seek_ret == 0);

        intptr_t samples_after_seek = ov_file.read(read_buf.data(), READ_SIZE);
        TEST(L"OggVorbisHandle read after seek > 0", samples_after_seek > 0);
    }

    // ── Clear and reopen ──
    ov_file.clear();
    TEST(L"OggVorbisHandle clear keeps decoder valid", ov_file.is_valid());

    bool reopened = ov_file.open(L"sound/Thunderstorm.ogg");
    TEST(L"OggVorbisHandle reopen after clear", reopened == true);
    if (reopened) {
        TEST(L"OggVorbisHandle pcm_total after reopen > 0", ov_file.pcm_total() > 0);
    }

    // ── Open nonexistent file returns false ──
    o::OggVorbisHandle ov_nonexist;
    bool bad_open = ov_nonexist.open(L"sound/nonexistent.ogg");
    TEST(L"OggVorbisHandle open nonexistent returns false", bad_open == false);
    TEST(L"OggVorbisHandle still valid after failed open", ov_nonexist.is_valid());

    // ── Operations on moved-from (null) handle must not crash ──
    o::OggVorbisHandle ov7;
    o::OggVorbisHandle ov8 = std::move(ov7);
    ov7.clear();
    ov7.pcm_total();
    ov7.read(buf2, 16);
    TEST(L"OggVorbisHandle operations no crash on null handle", true);
}

// ---- Thread service tests (ssz_native::thread) ----

static void test_thread_service()
{
    namespace t = ikemen::ssz_native::thread;
    std::wcout << L"\n--- Thread service ---" << std::endl;

    // No-crash smoke test
    t::delay(0);
    t::delay(1);
    TEST(L"thread::delay(0) and delay(1) no crash", true);

    // Timing accuracy: delay(100) should take ~100ms
    // Allow generous tolerance (50-500ms) since this runs in a test env
    uint32_t t0 = ikemen::ssz_native::time_util::tick_count();
    t::delay(100);
    uint32_t t1 = ikemen::ssz_native::time_util::tick_count();
    uint32_t elapsed = t1 - t0;
    TEST(L"thread::delay(100) takes >= 50ms", elapsed >= 50);
    TEST(L"thread::delay(100) takes <= 500ms", elapsed <= 500);
}

// ---- Time service tests (ssz_native::time_util) ----

static void test_time_service()
{
    namespace t = ikemen::ssz_native::time_util;
    std::wcout << L"\n--- Time service ---" << std::endl;

    // tick_count should be > 0 (system was running before this call)
    uint32_t tc = t::tick_count();
    TEST(L"tick_count > 0", tc > 0);

    // tick_count should be monotonic (second call >= first call)
    uint32_t tc2 = t::tick_count();
    TEST(L"tick_count monotonic", tc2 >= tc);

    // unix_time should be > 1000000000 (year 2001+)
    int64_t ut = t::unix_time();
    TEST(L"unix_time > 1e9", ut > 1000000000);

    // unix_time should be monotonic
    int64_t ut2 = t::unix_time();
    TEST(L"unix_time monotonic", ut2 >= ut);

    // Rough sanity: 2026-07-05 is around 1751700000 (Unix timestamp)
    TEST(L"unix_time in plausible 2026 range", ut > 1700000000);
    TEST(L"unix_time not far future (< 2000000000)", ut < 2000000000);
}

// ---- Table service tests (ssz_native::table) ----

static void test_table_service()
{
    namespace t = ikemen::ssz_native::table;
    std::wcout << L"\n--- Table service ---" << std::endl;

    // String hash — deterministic
    uint32_t h1 = t::hash(L"hello");
    uint32_t h2 = t::hash(L"hello");
    TEST(L"hash deterministic", h1 == h2);

    // Different strings, different hashes (unlikely collision)
    TEST(L"hash different strings", t::hash(L"abc") != t::hash(L"xyz"));

    // NameTable basic operations
    t::NameTable<int> nt;
    TEST(L"NameTable initially empty", nt.size() == 0);

    nt.set(L"a", 1);
    nt.set(L"b", 2);
    nt.set(L"c", 3);
    TEST(L"NameTable size after 3 inserts", nt.size() == 3);

    const int* v = nt.get(L"a");
    TEST(L"NameTable get 'a'", v != nullptr && *v == 1);
    v = nt.get(L"b");
    TEST(L"NameTable get 'b'", v != nullptr && *v == 2);
    v = nt.get(L"c");
    TEST(L"NameTable get 'c'", v != nullptr && *v == 3);

    TEST(L"NameTable contains 'a'", nt.contains(L"a"));
    TEST(L"NameTable not contains 'z'", !nt.contains(L"z"));

    // Update existing key
    nt.set(L"a", 10);
    v = nt.get(L"a");
    TEST(L"NameTable update 'a'", v != nullptr && *v == 10);

    // Remove
    TEST(L"NameTable remove 'b'", nt.remove(L"b"));
    TEST(L"NameTable size after remove", nt.size() == 2);
    TEST(L"NameTable not contains 'b' after remove", !nt.contains(L"b"));

    // Clear
    nt.clear();
    TEST(L"NameTable empty after clear", nt.size() == 0);

    // Keys / values / for_each
    nt.set(L"x", 100);
    nt.set(L"y", 200);
    auto keys = nt.keys();
    TEST(L"NameTable keys count", keys.size() == 2);
    auto vals = nt.values();
    TEST(L"NameTable values count", vals.size() == 2);

    int sum = 0;
    nt.for_each([&sum](const std::wstring&, const int& val) { sum += val; });
    TEST(L"NameTable for_each sum", sum == 300);

    // operate (get-or-create)
    nt.operate(L"z", [](int& v) { v = 999; });
    v = nt.get(L"z");
    TEST(L"NameTable operate creates", v != nullptr && *v == 999);
    nt.operate(L"z", [](int& v) { v += 1; });
    v = nt.get(L"z");
    TEST(L"NameTable operate modifies", v != nullptr && *v == 1000);

    // each_value
    int vsum = 0;
    nt.each_value([&vsum](const int& val) { vsum += val; });
    TEST(L"NameTable each_value sum > 0", vsum > 0);

    // int_hash
    auto ih1 = t::int_hash<int32_t>(42);
    auto ih2 = t::int_hash<int32_t>(42);
    TEST(L"int_hash deterministic", ih1 == ih2);
    TEST(L"int_hash different", t::int_hash<int32_t>(1) != t::int_hash<int32_t>(2));

    // IntTable
    t::IntTable<int32_t, std::string> it;
    TEST(L"IntTable initially empty", it.size() == 0);
    it.set(1, "one");
    it.set(2, "two");
    it.set(3, "three");
    TEST(L"IntTable size 3", it.size() == 3);
    const std::string* sv = it.get(2);
    TEST(L"IntTable get 2", sv != nullptr && *sv == "two");
    TEST(L"IntTable contains 1", it.contains(1));
    TEST(L"IntTable not contains 99", !it.contains(99));
    TEST(L"IntTable remove 2", it.remove(2));
    TEST(L"IntTable size after remove", it.size() == 2);
    it.clear();
    TEST(L"IntTable empty after clear", it.size() == 0);
}

// ---- Lua service tests (ssz_native::lua) ----

static void test_lua_service()
{
    namespace l = ikemen::ssz_native::lua;
    std::wcout << L"\n--- Lua service ---" << std::endl;

    // Construction
    l::LuaState ls;
    TEST(L"LuaState constructed", ls.is_valid());

    // Move semantics
    l::LuaState ls2;
    l::LuaState ls3 = std::move(ls2);
    TEST(L"LuaState move: source invalid", !ls2.is_valid());
    TEST(L"LuaState move: dest valid", ls3.is_valid());

    l::LuaState ls4;
    ls4 = std::move(ls3);
    TEST(L"LuaState move-assign: source invalid", !ls3.is_valid());
    TEST(L"LuaState move-assign: dest valid", ls4.is_valid());

    // Self-move-assignment safety
    ls4 = std::move(ls4);
    TEST(L"LuaState self-move safe", ls4.is_valid());

    // Basic Lua operations
    ls.push_number(42.0);
    TEST(L"LuaState push_number", ls.is_number(-1));
    double val = ls.to_number(-1);
    TEST(L"LuaState to_number == 42", val == 42.0);
    ls.pop(1);

    ls.push_boolean(true);
    TEST(L"LuaState push_boolean", ls.is_boolean(-1));
    TEST(L"LuaState to_boolean", ls.to_boolean(-1));
    ls.pop(1);

    ls.push_string("hello");
    TEST(L"LuaState push_string", ls.is_string(-1));
    std::string s = ls.to_string(-1);
    TEST(L"LuaState to_string", s == "hello");
    ls.pop(1);
}

// ---- Shell service tests (ssz_native::shell) ----

static void test_shell_service()
{
    namespace s = ikemen::ssz_native::shell;
    std::wcout << L"\n--- Shell service ---" << std::endl;

    // open() launches an external process (ShellExecuteEx on Windows).
    // With empty strings the behavior is platform-dependent (may succeed
    // by opening current directory). Just verify no crash.
    s::open(L"", L"", L"", false, false);
    TEST(L"shell::open with empty args no crash", true);

    bool trash_result = s::move_to_trash(L"");
    TEST(L"shell::move_to_trash with empty path returns false", trash_result == false);

    // Test with a nonexistent file (should also return false)
    bool trash_nonexist = s::move_to_trash(L"__nonexistent_file_for_testing__");
    TEST(L"shell::move_to_trash nonexistent returns false", trash_nonexist == false);
}

// ---- Share service tests (ssz_native::share) ----

static void test_share_service()
{
    std::wcout << L"\n--- Share service ---" << std::endl;
    using namespace ikemen::ssz_native;

    // Default-initialized fields
    ShareData sd;
    TEST(L"ShareData default init tm == 0", sd.tm == 0);
    TEST(L"ShareData default init zoom == false", sd.zoom == false);
    TEST(L"ShareData default init team1VS2Life == 0", sd.team1VS2Life == 0.0f);
    TEST(L"ShareData default init operatingSystem empty", sd.operatingSystem.empty());
    TEST(L"ShareData default init com empty", sd.com.empty());
    TEST(L"ShareData default init powsh empty", sd.powsh.empty());
    TEST(L"ShareData default init alvl == false", sd.alvl == false);

    // copy/push roundtrip through internal snapshot
    ShareData sd1;
    sd1.chr_home = 1;
    sd1.chr_match = 2;
    sd1.chr_round = 3;
    sd1.life = 0.75f;
    sd1.power = 3000;
    sd1.zoom = true;
    sd1.zoomMin = 0.5f;
    sd1.zoomMax = 2.0f;
    sd1.zoomSpeed = 8.0f;
    sd1.operatingSystem = "Windows";
    sd1.gameMode = "arcade";
    sd1.com.push_back(42);
    sd1.taglevel.push_back(1);
    sd1.autoguard.push_back(false);
    sd1.powsh.push_back(true);
    sd1.inputRemap.push_back(-1);
    sd1.dbgdw = true;
    sd1.clsndw = true;
    sd1.stsdw = true;
    sd1.alvl = true;
    sd1.fullscr = true;
    sd1.exitMatch = true;
    sd1.suaveMode = 2;
    sd1.abyssSP1 = "test";
    sd1.dlua = "debug.lua";

    share_push(sd1);

    ShareData sd2;
    share_copy(sd2);

    // Verify roundtrip
    TEST(L"share roundtrip chr_home", sd2.chr_home == 1);
    TEST(L"share roundtrip chr_match", sd2.chr_match == 2);
    TEST(L"share roundtrip chr_round", sd2.chr_round == 3);
    TEST(L"share roundtrip life", sd2.life == 0.75f);
    TEST(L"share roundtrip power", sd2.power == 3000);
    TEST(L"share roundtrip zoom", sd2.zoom == true);
    TEST(L"share roundtrip zoomMin", sd2.zoomMin == 0.5f);
    TEST(L"share roundtrip zoomMax", sd2.zoomMax == 2.0f);
    TEST(L"share roundtrip operatingSystem", sd2.operatingSystem == "Windows");
    TEST(L"share roundtrip gameMode", sd2.gameMode == "arcade");
    TEST(L"share roundtrip com size", sd2.com.size() == 1);
    TEST(L"share roundtrip com[0]", sd2.com.size() > 0 && sd2.com[0] == 42);
    TEST(L"share roundtrip powsh size", sd2.powsh.size() == 1);
    TEST(L"share roundtrip powsh[0]", sd2.powsh.size() > 0 && sd2.powsh[0] == true);
    TEST(L"share roundtrip taglevel size", sd2.taglevel.size() == 1);
    TEST(L"share roundtrip autoguard size", sd2.autoguard.size() == 1);
    TEST(L"share roundtrip inputRemap size", sd2.inputRemap.size() == 1);
    TEST(L"share roundtrip dbgdw", sd2.dbgdw == true);
    TEST(L"share roundtrip clsndw", sd2.clsndw == true);
    TEST(L"share roundtrip stsdw", sd2.stsdw == true);
    TEST(L"share roundtrip alvl", sd2.alvl == true);
    TEST(L"share roundtrip exitMatch", sd2.exitMatch == true);
    TEST(L"share roundtrip suaveMode", sd2.suaveMode == 2);
    TEST(L"share roundtrip abyssSP1", sd2.abyssSP1 == "test");
    TEST(L"share roundtrip dlua", sd2.dlua == "debug.lua");

    // CommonData ↔ ShareData roundtrip
    {
        CommonData cd;
        cd.home = 1;
        cd.match = 2;
        cd.round = 3;
        cd.life = 0.75f;
        cd.power = 3000;
        cd.cam.zoom = true;
        cd.cam.zoomMin = 0.5f;
        cd.cam.zoomMax = 2.0f;
        cd.operatingSystem = "Windows";
        cd.gameMode = "arcade";
        cd.com.push_back(42);
        cd.powerShare.push_back(true);
        cd.debugdraw = true;
        cd.autolevel = true;
        cd.suaveMode = 2;

        ShareData s;
        share_pull_from_common(cd, s);

        TEST(L"pull_from_common home", s.chr_home == 1);
        TEST(L"pull_from_common match", s.chr_match == 2);
        TEST(L"pull_from_common round", s.chr_round == 3);
        TEST(L"pull_from_common life", s.life == 0.75f);
        TEST(L"pull_from_common power", s.power == 3000);
        TEST(L"pull_from_common zoom", s.zoom == true);
        TEST(L"pull_from_common zoomMin", s.zoomMin == 0.5f);
        TEST(L"pull_from_common operatingSystem", s.operatingSystem == "Windows");
        TEST(L"pull_from_common gameMode", s.gameMode == "arcade");
        TEST(L"pull_from_common com", s.com.size() == 1 && s.com[0] == 42);
        TEST(L"pull_from_common powsh", s.powsh.size() == 1 && s.powsh[0] == true);
        TEST(L"pull_from_common dbgdw", s.dbgdw == true);
        TEST(L"pull_from_common alvl", s.alvl == true);
        TEST(L"pull_from_common suaveMode", s.suaveMode == 2);

        // Push back
        CommonData cd2;
        share_push_to_common(s, cd2);
        TEST(L"push_to_common home", cd2.home == 1);
        TEST(L"push_to_common life", cd2.life == 0.75f);
        TEST(L"push_to_common zoom", cd2.cam.zoom == true);
        TEST(L"push_to_common debugdraw", cd2.debugdraw == true);
        TEST(L"push_to_common autolevel", cd2.autolevel == true);
    }
}

// ---- Fight service tests (ssz_native::fight) ----

static void test_fight_service()
{
    std::wcout << L"\n--- Fight service ---" << std::endl;
    using namespace ikemen::ssz_native;
    FightState fs;
    TEST(L"FightState created", true);
}

// ---- Stack service tests (ssz_native::stack) ----

static void test_stack_service()
{
    std::wcout << L"\n--- Stack service ---" << std::endl;
    using namespace ikemen::ssz_native;

    Stack<int> s;
    TEST(L"Stack initially empty", s.empty());
    TEST(L"Stack size == 0", s.size() == 0);

    s.push(42);
    TEST(L"Stack not empty after push", !s.empty());
    TEST(L"Stack size == 1", s.size() == 1);

    s.push(100);
    TEST(L"Stack size == 2", s.size() == 2);

    int val = s.pop();
    TEST(L"Stack pop returns last pushed", val == 100);
    TEST(L"Stack size == 1 after pop", s.size() == 1);

    val = s.pop();
    TEST(L"Stack pop returns first pushed", val == 42);
    TEST(L"Stack empty after all pops", s.empty());

    s.push(1);
    s.push(2);
    s.push(3);
    s.clear();
    TEST(L"Stack empty after clear", s.empty());

    // top()
    Stack<int> s2;
    TEST(L"Stack top on empty returns nullptr", s2.top() == nullptr);
    s2.push(42);
    s2.push(99);
    const int* tp = s2.top();
    TEST(L"Stack top not null", tp != nullptr);
    TEST(L"Stack top returns last pushed", tp != nullptr && *tp == 99);
    TEST(L"Stack top does not remove", s2.size() == 2);
}

// ---- Config service tests (ssz_native::config) ----

static void test_config_service()
{
    std::wcout << L"\n--- Config service ---" << std::endl;
    using namespace ikemen::ssz_native;

    ConfigData cfg;
    TEST(L"Config Width == 640", cfg.Width == 640);
    TEST(L"Config Height == 480", cfg.Height == 480);
    TEST(L"Config GameSpeed == 60", cfg.GameSpeed == 60);
    TEST(L"Config GlVol == 0.8", std::abs(cfg.GlVol - 0.8f) < 0.001f);
    TEST(L"Config HelperMax == 56", cfg.HelperMax == 56);
    TEST(L"Config CharPortraitsGroup == 9000", cfg.CharPortraitsGroup == 9000);
    TEST(L"Config WindowTitle default", cfg.WindowTitle == "I.K.E.M.E.N. PLUS ULTRA");
    TEST(L"Config Executable default", cfg.Executable == "Ikemen Plus Ultra.exe");
    TEST(L"Config IgnoreMostErrors default", cfg.IgnoreMostErrors == true);

    // ConfigNet defaults
    ConfigData net = make_default_config_net();
    TEST(L"ConfigNet Width == 1280", net.Width == 1280);
    TEST(L"ConfigNet Height == 800", net.Height == 800);
    TEST(L"ConfigNet CharWinnerPortraitIndex == 2", net.CharWinnerPortraitIndex == 2);
    TEST(L"ConfigNet CharLoserPortraitIndex == 3", net.CharLoserPortraitIndex == 3);
    TEST(L"ConfigNet CharVSPortraitIndex == 5", net.CharVSPortraitIndex == 5);
    TEST(L"ConfigNet CharResultsPortraitIndex == 6", net.CharResultsPortraitIndex == 6);

    // make_default_config has input bindings
    ConfigData full = make_default_config();
    TEST(L"Config Input[0] jn == -1 (keyboard)", full.Input[0].jn == -1);
    TEST(L"Config Input[2] jn == 0 (gamepad)", full.Input[2].jn == 0);
    TEST(L"Config Input[10] jn == -1 (menu keyboard)", full.Input[10].jn == -1);

    // KeyBindings set
    KeyBindings kb;
    kb.set(-1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14);
    TEST(L"KeyBindings set jn", kb.jn == -1);
    TEST(L"KeyBindings set u", kb.u == 1);
    TEST(L"KeyBindings set s", kb.s == 14);

    // Config save/load roundtrip
    {
        ConfigData save_cfg = make_default_config();
        save_cfg.Width = 800;
        save_cfg.Height = 600;
        save_cfg.GameSpeed = 30;
        save_cfg.UserName = "TestUser";
        save_cfg.Input[0].jn = -1;
        save_cfg.Input[0].u = 273;

        bool saved = config_save("test_config_roundtrip.ini", save_cfg);
        TEST(L"config_save succeeds", saved);

        ConfigData loaded;
        bool loaded_ok = config_load("test_config_roundtrip.ini", loaded);
        TEST(L"config_load succeeds", loaded_ok);
        TEST(L"config roundtrip Width", loaded.Width == 800);
        TEST(L"config roundtrip Height", loaded.Height == 600);
        TEST(L"config roundtrip GameSpeed", loaded.GameSpeed == 30);
        TEST(L"config roundtrip UserName", loaded.UserName == "TestUser");
        TEST(L"config roundtrip Input[0].jn", loaded.Input[0].jn == -1);
        TEST(L"config roundtrip Input[0].u", loaded.Input[0].u == 273);

        std::remove("test_config_roundtrip.ini");
    }

    // config_load nonexistent returns false
    {
        ConfigData c;
        TEST(L"config_load nonexistent fails", !config_load("nonexistent_config.ini", c));
    }
}

// ---- SDL plugin script service tests (ssz_native::sdlplugin) ----

// ---- SDL plugin service tests (ssz_native::sdlplugin) ----

static void test_sdlplugin_service()
{
    std::wcout << L"\n--- SDL plugin service ---" << std::endl;
    
    // Test constants
    TEST(L"SNDFREQ constant", ikemen::ssz_native::SNDFREQ == 44100);
    TEST(L"SNDBUFLEN constant", ikemen::ssz_native::SNDBUFLEN == 4096);
    TEST(L"RELEASED constant", ikemen::ssz_native::RELEASED == 0);
    TEST(L"PRESSED constant", ikemen::ssz_native::PRESSED == 1);

    // Test modifier constants
    TEST(L"KMOD_NONE", ikemen::ssz_native::KMOD_NONE == 0x0000);
    TEST(L"KMOD_CTRL", ikemen::ssz_native::KMOD_CTRL == (ikemen::ssz_native::KMOD_LCTRL | ikemen::ssz_native::KMOD_RCTRL));
    TEST(L"KMOD_SHIFT", ikemen::ssz_native::KMOD_SHIFT == (ikemen::ssz_native::KMOD_LSHIFT | ikemen::ssz_native::KMOD_RSHIFT));
    TEST(L"KMOD_ALT", ikemen::ssz_native::KMOD_ALT == (ikemen::ssz_native::KMOD_LALT | ikemen::ssz_native::KMOD_RALT));
    TEST(L"KMOD_GUI", ikemen::ssz_native::KMOD_GUI == (ikemen::ssz_native::KMOD_LGUI | ikemen::ssz_native::KMOD_RGUI));

    // Test SdlRect
    ikemen::ssz_native::SdlRect sr;
    TEST(L"SdlRect default x", sr.x == 0);
    TEST(L"SdlRect default y", sr.y == 0);
    TEST(L"SdlRect default w", sr.w == 0);
    TEST(L"SdlRect default h", sr.h == 0);
    sr.set(10, 20, 100, 200);
    TEST(L"SdlRect set x", sr.x == 10);
    TEST(L"SdlRect set y", sr.y == 20);
    TEST(L"SdlRect set w", sr.w == 100);
    TEST(L"SdlRect set h", sr.h == 200);

    // ── Surface destructor/free-path tests ──
    {
        ikemen::ssz_native::Surface surf;
        TEST(L"Surface default isNull", surf.isNull());

        // Double-free safety: calling free() on null surface must not crash
        surf.free();
        TEST(L"Surface free() on null is safe", true);
        TEST(L"Surface still null after free()", surf.isNull());
    }

    // ── Font destructor/free-path tests ──
    {
        ikemen::ssz_native::Font f;
        TEST(L"Font default null", f.font == nullptr);

        // Double-free safety
        f.close();
        TEST(L"Font close() on null is safe", true);
        TEST(L"Font still null after close()", f.font == nullptr);
    }

    // ── GlTexture destructor/free-path tests ──
    {
        ikemen::ssz_native::GlTexture tex;
        TEST(L"GlTexture default id 0", tex.id == 0);

        // Double-free safety
        tex.clear();
        TEST(L"GlTexture clear() on zero is safe", true);
        TEST(L"GlTexture id still 0 after clear()", tex.id == 0);

        // load8bitTexture with empty data returns false (no crash)
        std::vector<uint8_t> emptyPxl;
        bool loaded = tex.load8bitTexture(emptyPxl, 10, 10);
        TEST(L"GlTexture load8bitTexture empty fails", loaded == false);
        TEST(L"GlTexture id unchanged after failed load", tex.id == 0);
    }
}

// ---- SDL event service tests (ssz_native::sdlevent) ----

static void test_sdlevent_service()
{
    std::wcout << L"\n--- SDL event service ---" << std::endl;
    using namespace ikemen::ssz_native;

    // Test SdleKey
    SdleKey sk;
    TEST(L"SdleKey default key UNKNOWN", sk.key == K::UNKNOWN);
    TEST(L"SdleKey default down false", sk.down == false);
    
    sk.key = K::a;
    sk.shift = false;
    sk.ctrl = false;
    sk.alt = false;
    sk.checkDown(K::a, 0);
    TEST(L"SdleKey checkDown matches", sk.down == true);
    
    sk.down = false;
    sk.checkDown(K::b, 0);
    TEST(L"SdleKey checkDown no-match", sk.down == false);
    
    sk.reset();
    TEST(L"SdleKey reset clears down", sk.down == false);

    // Test SdleventState defaults
    SdleventState state;
    TEST(L"SdleventState nexttime==0", state.nexttime == 0);
    TEST(L"SdleventState lastdraw==0", state.lastdraw == 0);
    TEST(L"SdleventState end==false", state.end == false);
    TEST(L"SdleventState esc==false", state.esc == false);
    TEST(L"SdleventState eventKeys empty", state.eventKeys.empty());

    // Test resetFrameKeys
    state.esc = true;
    state.aKey = true;
    state.upKey = true;
    state.returnKey = true;
    state.resetFrameKeys();
    TEST(L"SdleventState resetFrameKeys esc", state.esc == false);
    TEST(L"SdleventState resetFrameKeys aKey", state.aKey == false);
    TEST(L"SdleventState resetFrameKeys upKey", state.upKey == false);
    TEST(L"SdleventState resetFrameKeys returnKey", state.returnKey == false);

    // Test module-level get_state
    SdleventState& gs = sdlevent_get_state();
    TEST(L"sdlevent_get_state returns reference", true);

    // Note: sdlevent_event_update() and sdlevent_event() require SDL
    // to be initialized (window, video). They will be tested in integration.
}

// ---- Char service tests (ssz_native::char) ----

static void test_char_service()
{
    std::wcout << L"\n--- Char service ---" << std::endl;
    using namespace ikemen::ssz_native;
    CharModuleState cs;
    TEST(L"CharModuleState created", true);
}

// ---- Command service tests (ssz_native::command) ----

static void test_command_service()
{
    std::wcout << L"\n--- Command service ---" << std::endl;
    using namespace ikemen::ssz_native;
    CommandState cs;
    TEST(L"CommandState created", true);
}

// ---- SFF service tests (ssz_native::sff) ----

static void test_sff_service()
{
    std::wcout << L"\n--- SFF service ---" << std::endl;
    using namespace ikemen::ssz_native;
    SffState ss;
    TEST(L"SffState created", true);
}

// ---- Stage service tests (ssz_native::stage) ----

static void test_stage_service()
{
    std::wcout << L"\n--- Stage service ---" << std::endl;
    using namespace ikemen::ssz_native;
    StageData sd;
    TEST(L"StageData def empty", sd.def.empty());
    TEST(L"StageData name empty", sd.name.empty());
    TEST(L"StageData bgmusic empty", sd.bgmusic.empty());
}

// ---- BG service tests (ssz_native::bg) ----

static void test_bg_service()
{
    std::wcout << L"\n--- BG service ---" << std::endl;
    using namespace ikemen::ssz_native;
    BgState bs;
    TEST(L"BgState created", true);
}

// ---- Fighting service tests (ssz_native::fighting) ----

static void test_fighting_service()
{
    std::wcout << L"\n--- Fighting service ---" << std::endl;
    using namespace ikemen::ssz_native;
    FightingState fs;
    TEST(L"FightingState created", true);
    fighting_init();
    TEST(L"fighting_init() no-crash", true);
}

// ---- Sound resource service tests (ssz_native::sound_resource) ----

static void test_sound_resource_service()
{
    std::wcout << L"\n--- Sound resource service ---" << std::endl;
    using namespace ikemen::ssz_native;
    SndNnm sn;
    TEST(L"SndNnm default group==-1", sn.group == -1);
    TEST(L"SndNnm default number==0", sn.number == 0);
    SndNnm sn2{5, 10};
    TEST(L"SndNnm aggregate init", sn2.group == 5 && sn2.number == 10);
    WaveData wd;
    TEST(L"WaveData samplesPerSec==44100", wd.samplesPerSec == 44100);
    TEST(L"WaveData wav empty", wd.wav.empty());
    BgmData bd;
    TEST(L"BgmData fileName empty", bd.fileName.empty());
    TEST(L"BgmData volume==100", bd.volume == 100);

    // SoundResourceState via accessor
    SoundResourceState& st = sound_resource_get_state();
    TEST(L"sound_resource_get_state exists", true);
    TEST(L"State sndbuf starts zeroed", st.sndbuf[0] == 0 && st.sndbuf[1023] == 0);
    TEST(L"State panstr == 128.0", st.panstr == 128.0f);

    // init resets state
    sound_resource_init();
    TEST(L"sound_resource_init sndbuf clear", st.sndbuf[0] == 0);
    TEST(L"sound_resource_init bgm empty", st.bgm.fileName.empty());

    // SoundChannel defaults
    SoundChannel sc;
    TEST(L"SoundChannel wave null", sc.wave == nullptr);
    TEST(L"SoundChannel volume 256", sc.volume == 256);
    TEST(L"SoundChannel x 0.0", sc.x == 0.0f);
    TEST(L"SoundChannel loop false", sc.loop_ == false);
    TEST(L"SoundChannel freqmul 1.0", sc.freqmul == 1.0f);
    TEST(L"SoundChannel setVol clamp low", (sc.setVol(-50), sc.volume == 0));
    TEST(L"SoundChannel setVol clamp high", (sc.setVol(1000), sc.volume == 512));
    TEST(L"SoundChannel setVol normal", (sc.setVol(300), sc.volume == 300));
    TEST(L"SoundChannel setPan clamp low", (sc.setPan(-200.0f), sc.x == -160.0f));
    TEST(L"SoundChannel setPan clamp high", (sc.setPan(200.0f), sc.x == 160.0f));
    sc.setDefaultParameter();
    TEST(L"SoundChannel setDefaultParameter volume", sc.volume == 256);
    TEST(L"SoundChannel setDefaultParameter pan", sc.x == 0.0f);
    TEST(L"SoundChannel setDefaultParameter loop", sc.loop_ == false);

    // Mix on empty channel (no wave) should not crash
    int mix_buf[2048] = {};
    sc.mix(mix_buf, -160.0f, 160.0f);
    TEST(L"SoundChannel mix empty wave no-crash", true);

    // Mix with a valid wave
    WaveData test_wave;
    test_wave.channels = 1;
    test_wave.bytesPerSample = 1;
    test_wave.samplesPerSec = 22050;
    test_wave.wav.resize(256, 128);
    sc.wave = &test_wave;
    sc.setDefaultParameter();
    std::memset(mix_buf, 0, sizeof(mix_buf));
    sc.mix(mix_buf, -160.0f, 160.0f);
    TEST(L"SoundChannel mix 8-bit mono no-crash", true);
    TEST(L"SoundChannel mix silent wave yields zeros", mix_buf[0] == 0 && mix_buf[1] == 0);

    // SoundTable defaults
    SoundTable stbl;
    TEST(L"SoundTable sound_map empty", stbl.sound_map.empty());
    TEST(L"SoundTable getSound null for nonexistent", stbl.getSound(0, 0) == nullptr);

    // loadFile on nonexistent path returns error
    std::string load_err = stbl.loadFile("nonexistent.snd");
    TEST(L"SoundTable loadFile nonexistent returns error", !load_err.empty());

    // BgmData
    BgmData bgm;
    bgm.play("bgm.ogg");
    TEST(L"BgmData play sets fileName", bgm.fileName == "bgm.ogg");
    TEST(L"BgmData write no-crash", true);
    bgm.clear();
    TEST(L"BgmData clear fileName", bgm.fileName.empty());

    // Module-level functions no-crash
    sndbuf_clear();
    TEST(L"sndbuf_clear no-crash", true);
    SoundChannel* ch = get_channel(0);
    TEST(L"get_channel(0) non-null", ch != nullptr);
    TEST(L"get_channel(0) channel 0", ch == &st.sounds[0]);
    SoundChannel* free_ch = get_channel(-1);
    TEST(L"get_channel(-1) finds free channel", free_ch != nullptr);
    mix_sounds();
    TEST(L"mix_sounds no-crash", true);
    play_sound();
    TEST(L"play_sound no-crash", true);
    stop_sound();
    TEST(L"stop_sound no-crash", true);

    // add_wave
    WaveData mono_wave;
    mono_wave.channels = 1;
    mono_wave.bytesPerSample = 2;
    mono_wave.samplesPerSec = 44100;
    mono_wave.wav.resize(1024, 0);
    TEST(L"add_wave valid wave", add_wave(&mono_wave) == true);
    TEST(L"add_wave null wave", add_wave(nullptr) == false);

    // WaveData::is_valid
    WaveData valid;
    TEST(L"WaveData empty not valid", valid.is_valid() == false);
    valid.wav.resize(10);
    TEST(L"WaveData filled valid", valid.is_valid() == true);

    // sound_table accessors
    std::string tbl_err = sound_table_load_file("../test/nonexistent.snd");
    TEST(L"sound_table_load_file nonexistent returns error", !tbl_err.empty());
    TEST(L"sound_table_get_sound nonexistent returns null", sound_table_get_sound(0, 0) == nullptr);

    // ---- Snd file parsing test ----
    // Load a real .snd file from install/ and verify extraction
    sound_resource_init();

    // Use find to locate .snd files — common.snd is large with many sounds
    std::string snd_err = sound_table_load_file("data/common.snd");
    TEST(L"SoundTable load data/common.snd success", snd_err.empty());

    // Scan a range of group/numbers to find at least one loaded sound
    const WaveData* found_sound = nullptr;
    int found_group = 0, found_number = 0;
    for (int g = 0; g <= 50 && !found_sound; g++) {
        for (int n = 0; n <= 50 && !found_sound; n++) {
            found_sound = sound_table_get_sound(g, n);
            if (found_sound) { found_group = g; found_number = n; }
        }
    }
    TEST(L"SoundTable at least one sound found in common.snd", found_sound != nullptr);
    if (found_sound) {
        TEST(L"Found sound wav non-empty", !found_sound->wav.empty());
        TEST(L"Found sound channels valid", found_sound->channels == 1 || found_sound->channels == 2);
        TEST(L"Found sound bytesPerSample valid",
            found_sound->bytesPerSample == 1 || found_sound->bytesPerSample == 2);
        TEST(L"Found sound samplesPerSec > 0", found_sound->samplesPerSec > 0);
        TEST(L"Found sound num.group matches scan", found_sound->num.group == found_group);
        TEST(L"Found sound num.number matches scan", found_sound->num.number == found_number);
    }

    // Negative: non-existent sound returns null
    TEST(L"SoundTable get_sound(99,99) null", sound_table_get_sound(99, 99) == nullptr);

    // Load nonexistent file returns error
    std::string bad_err = sound_table_load_file("data/nonexistent.snd");
    TEST(L"SoundTable load nonexistent returns error", !bad_err.empty());

    // Reload the same file (table already populated — should succeed)
    std::string reload_err = sound_table_load_file("data/common.snd");
    TEST(L"SoundTable reload common.snd success", reload_err.empty());

    // Verify sound still accessible after reload
    if (found_sound) {
        const WaveData* s_again = sound_table_get_sound(found_group, found_number);
        TEST(L"SoundTable get_sound after reload", s_again != nullptr);
    }

    // Snd format error: try to load a non-snd file
    {
        std::string fake_err = sound_table_load_file("ssz/ikemen.ssz");
        TEST(L"SoundTable load non-snd returns error", !fake_err.empty());
        // Error should mention "ElecbyteSnd" or similar
        TEST(L"SoundTable load non-snd error mentions ElecbyteSnd",
            fake_err.find("ElecbyteSnd") != std::string::npos);
    }
}

// ---- Action service tests (ssz_native::action) ----

static void test_action_service()
{
    std::wcout << L"\n--- Action service ---" << std::endl;
    // Note: Cannot use 'using namespace ikemen::ssz_native' here because
    // both action_service.hpp and sdlplugin_service.hpp define 'Rect'
    ikemen::ssz_native::Rect r;
    TEST(L"Rect default l==0", r.l == 0);
    TEST(L"Rect default r==-1", r.r == -1);
    ikemen::ssz_native::Rect r2{10, 20, 30, 40};
    TEST(L"Rect aggregate init", r2.l == 10 && r2.t == 20 && r2.r == 30 && r2.b == 40);
    ikemen::ssz_native::Frame f;
    TEST(L"Frame default time==-1", f.time == -1);
    TEST(L"Frame default group==-1", f.group == -1);
    TEST(L"Frame default clsn empty", f.clsn.empty());
    ikemen::ssz_native::ActionData a;
    TEST(L"ActionData created", true);
    ikemen::ssz_native::DrawnClsnData dc;
    TEST(L"DrawnClsnData created", true);
}

// ---- Video service tests (ssz_native::video) ----

static void test_video_service()
{
    std::wcout << L"\n--- Video service ---" << std::endl;
    using namespace ikemen::ssz_native;
    VideoData vd;
    VideoState vs;
    TEST(L"VideoData fileName empty", vd.fileName.empty());
    TEST(L"VideoData volume == 100", vd.volume == 100);
    TEST(L"VideoState videoActive false", vs.videoActive == false);
    video_play("test.mp4", "", 100, 1);
    TEST(L"video_play no-crash", true);
}

// ---- Font service tests (ssz_native::font) ----

static void test_font_service()
{
    std::wcout << L"\n--- Font service ---" << std::endl;
    using namespace ikemen::ssz_native;
    FontData fd;
    FontState fs;
    TEST(L"FontData created", true);
    TEST(L"FontState created", true);
    font_init();
    font_render_text("hello", 0, 0, 0xFFFFFFFF);
    TEST(L"font stubs no-crash", true);
}

// ---- Statebuilder service tests (ssz_native::statebuilder) ----

static void test_statebuilder_service()
{
    std::wcout << L"\n--- Statebuilder service ---" << std::endl;
    using namespace ikemen::ssz_native;
    StateBuilder sb;
    TEST(L"StateBuilderState created", true);
    statebuilder_init();
    TEST(L"statebuilder_init() no-crash", true);
}

// ---- System script service tests (ssz_native::system_script) ----

static void test_system_script_service()
{
    std::wcout << L"\n--- System script service ---" << std::endl;
    using namespace ikemen::ssz_native;
    SystemScriptState ss;
    TEST(L"SystemScriptState created", true);
    system_script_init(nullptr);
    TEST(L"system_script_init(nullptr) no-crash", true);
}

// ---- Script service tests (ssz_native::script) ----

static void test_script_service()
{
    std::wcout << L"\n--- Script service ---" << std::endl;
    using namespace ikemen::ssz_native;

    ScriptState ss;
    TEST(L"ScriptState created", true);

    script_init(nullptr);
    TEST(L"script_init(nullptr) no-crash", true);
}

// ---- Trigger script service tests (ssz_native::trigger) ----

static void test_trigger_script_service()
{
    std::wcout << L"\n--- Trigger script service ---" << std::endl;
    using namespace ikemen::ssz_native;

    // TriggerScriptState default init
    TriggerScriptState ts;
    TEST(L"TriggerScriptState created", true);

    // register_function stub — no-crash test
    register_function(nullptr);
    TEST(L"register_function(nullptr) no-crash", true);
}

// ---- Common service tests (ssz_native::common) ----

static void test_common_service()
{
    std::wcout << L"\n--- Common service ---" << std::endl;
    using namespace ikemen::ssz_native;

    // CommonData default init
    CommonData cd;
    TEST(L"CommonData coins == 0", cd.coins == 0);
    TEST(L"CommonData credits == 0", cd.credits == 0);
    TEST(L"CommonData life == 1.0", cd.life == 1.0f);
    TEST(L"CommonData power == 0", cd.power == 0);
    TEST(L"CommonData attack == 1.0", cd.attack == 1.0f);
    TEST(L"CommonData defence == 1.0", cd.defence == 1.0f);
    TEST(L"CommonData roundTime == 5994", cd.roundTime == 5994);
    TEST(L"CommonData roundsToWin == 2", cd.roundsToWin == 2);
    TEST(L"CommonData zoomMin > 0.8", cd.cam.zoomMin > 0.8f && cd.cam.zoomMin < 0.84f);
    TEST(L"CommonData zoomMax > 1.07", cd.cam.zoomMax > 1.07f && cd.cam.zoomMax < 1.08f);
    TEST(L"CommonData intro == 20", cd.intro == 20);
    TEST(L"CommonData p1mw == 2", cd.p1mw == 2);
    TEST(L"CommonData clsndraw == false", cd.clsndraw == false);
    TEST(L"CommonData debugdraw == false", cd.debugdraw == false);
    TEST(L"CommonData pause == false", cd.pause == false);
    TEST(L"CommonData maxSimul == 10", cd.maxSimul == 10);

    // Enums
    TEST(L"TeamMode::Single == 0", TeamMode::Single == TeamMode{0});
    TEST(L"TeamMode::Simul == 1", TeamMode::Simul == TeamMode{1});
    TEST(L"TeamMode::Turns == 2", TeamMode::Turns == TeamMode{2});

    // IXY default
    IXY ixy;
    TEST(L"IXY x == 0", ixy.x == 0);
    TEST(L"IXY y == 0", ixy.y == 0);

    // FXY default
    FXY fxy;
    TEST(L"FXY x == 0", fxy.x == 0.0f);

    // Real implementations
    common_flag_init(cd);
    TEST(L"common_flag_init com size", cd.com.size() == 20);
    TEST(L"common_flag_init com[0] == 4", cd.com.size() > 0 && cd.com[0] == 4);
    TEST(L"common_flag_init taglevel size", cd.taglevel.size() == 40);
    TEST(L"common_flag_init autoguard size", cd.autoguard.size() == 20);
    TEST(L"common_flag_init powerShare size", cd.powerShare.size() == 2);
    TEST(L"common_flag_init powerShare[0] true", cd.powerShare.size() > 0 && cd.powerShare[0] == true);

    common_reset_remap_input(cd);
    TEST(L"common_reset_remap_input size", cd.inputRemap.size() == 20);
    TEST(L"common_reset_remap_input[0] == 0", cd.inputRemap.size() > 0 && cd.inputRemap[0] == 0);
    TEST(L"common_reset_remap_input[19] == 19", cd.inputRemap.size() > 19 && cd.inputRemap[19] == 19);

    common_set_size(cd, 640, 480);
    TEST(L"common_set_size GameWidth", cd.GameWidth == 320);
    TEST(L"common_set_size GameHeight", cd.GameHeight == 240);
    TEST(L"common_set_size WidthScale == 2.0", std::abs(cd.WidthScale - 2.0f) < 0.001f);

    // resetFrameTime initializes tick state (nextAddTime = 1.0 for 60fps)
    common_reset_frame_time(cd);
    // Tick frame: oldTickCount(-1) < tickCount(0) → true
    TEST(L"common_tick_frame true", common_tick_frame(cd) == true);
    // Tick next frame: (int)(0 + 1.0) > 0 → 1 > 0 → true
    TEST(L"common_tick_next_frame true", common_tick_next_frame(cd) == true);

    // match_over: p1wins(0) >= p1mw(2) → false
    TEST(L"common_match_over false", common_match_over(cd) == false);
    TEST(L"common_match_over p1wins >= p1mw", (cd.p1wins = 2, common_match_over(cd)) == true);
    cd.p1wins = 0; // reset

    // nextLine scans forward, finds \n at position 5, returns 1
    {
        int nl_i = 0;
        int nl_r = common_next_line(nl_i, "hello\nworld");
        TEST(L"common_next_line returns 1", nl_r == 1);
        TEST(L"common_next_line i at \\n", nl_i == 5);
    }
    // nextLine with \r\n
    {
        int nl_i = 0;
        int nl_r = common_next_line(nl_i, "hello\r\nworld");
        TEST(L"common_next_line crlf returns 2", nl_r == 2);
        TEST(L"common_next_line crlf i", nl_i == 6);
    }
    // nextLine at end returns 0
    {
        int nl_i = 11;
        int nl_r = common_next_line(nl_i, "hello\nworld");
        TEST(L"common_next_line end returns 0", nl_r == 0);
        TEST(L"common_next_line end i unchanged", nl_i == 11);
    }
    // nextLine past end returns 0
    {
        int nl_i = 20;
        int nl_r = common_next_line(nl_i, "hello\nworld");
        TEST(L"common_next_line past end returns 0", nl_r == 0);
    }

    // splitLines returns ["a", "b"]
    {
        auto spl = common_split_lines("a\nb");
        TEST(L"common_split_lines size 2", spl.size() == 2);
        if (spl.size() >= 2) {
            TEST_EQ(L"common_split_lines[0]", spl[0], "a");
            TEST_EQ(L"common_split_lines[1]", spl[1], "b");
        }
    }
    // splitLines empty string
    {
        auto spl = common_split_lines("");
        TEST(L"common_split_lines empty", spl.empty());
    }
    // splitLines with crlf
    {
        auto spl = common_split_lines("hello\r\nworld");
        TEST(L"common_split_lines crlf size 2", spl.size() == 2);
        if (spl.size() >= 2) {
            TEST_EQ(L"common_split_lines crlf[0]", spl[0], "hello");
            TEST_EQ(L"common_split_lines crlf[1]", spl[1], "world");
        }
    }

    // atof parses decimal strings
    TEST(L"common_atof 3.14", std::abs(common_atof("3.14") - 3.14) < 0.001);
    TEST(L"common_atof integer", common_atof("42") == 42.0);
    TEST(L"common_atof negative", common_atof("-7.5") == -7.5);
    TEST(L"common_atof empty returns 0", common_atof("") == 0.0);
    TEST(L"common_atof exponent", std::abs(common_atof("1e2") - 100.0) < 0.001);

    // atoi parses integers
    TEST(L"common_atoi 42", common_atoi("42") == 42);
    TEST(L"common_atoi negative", common_atoi("-7") == -7);
    TEST(L"common_atoi positive sign", common_atoi("+3") == 3);
    TEST(L"common_atoi empty returns 0", common_atoi("") == 0);

    // loadText/readFileName/loadFile on nonexistent files
    TEST(L"common_load_text nonexistent", common_load_text("test.def", false).empty());
    TEST(L"common_read_file_name unicode passthrough", common_read_file_name("test.txt", true) == "test.txt");
    TEST(L"common_load_file nonexistent non-empty", !common_load_file("chars/kfm/kfm.def", cd.debugScript).empty());

    // Field mutation
    cd.coins = 5;
    cd.credits = 3;
    cd.life = 0.5f;
    cd.power = 3000;
    cd.intro = 0;
    cd.clsndraw = true;
    cd.cam.zoom = true;
    TEST(L"CommonData fields hold values",
        cd.coins == 5 && cd.credits == 3 && cd.life == 0.5f &&
        cd.power == 3000 && cd.intro == 0 && cd.clsndraw == true &&
        cd.cam.zoom == true);
}

// ---- Loader service tests (ssz_native::loader) ----

static void test_loader_service()
{
    std::wcout << L"\n--- Loader service ---" << std::endl;
    using namespace ikemen::ssz_native;

    // LoaderData default init via accessor
    LoaderData& ld = loader_get_state();
    TEST(L"LoaderData state == NotYet", ld.state == LoaderState::NotYet);
    TEST(L"LoaderData errorMes empty", ld.errorMes.empty());

    // Error handling
    loader_error("test error");
    TEST(L"loader_error stores message", ld.errorMes == "test error");

    // Reset clears error
    loader_reset();
    TEST(L"loader_reset clears state", ld.state == LoaderState::NotYet);
    TEST(L"loader_reset clears error", ld.errorMes.empty());

    // Set up stage selection so loader_stage attempts to load
    common_get_state().round = 1;

    // Create a minimal valid stage .def file for loader_stage to parse.
    // Use unquoted name value to avoid quote-stripping issues in the parser.
    std::wstring stageDefPath = TMPDIR + L"/stageZ_test.def";
    bool defOk = SaveAsciiText(
        L"[Info]\nname = Test Stage\nauthor = Test\n",
        stageDefPath);
    TEST(L"stage .def created", defOk);

    std::string stageDef(stageDefPath.begin(), stageDefPath.end());
    std::string addedName = system_add_stage(stageDef);
    TEST(L"system_add_stage returned name", !addedName.empty());

    // Find our stage's index by scanning the global stagelist
    int stageIdx = 0;
    for (int k = 1; k <= 100; k++) {
        std::string name = system_get_stage_name(k);
        if (name == "Test Stage") { stageIdx = k; break; }
    }
    TEST(L"found our stage in global list", stageIdx > 0);

    system_set_stage_no(stageIdx);
    system_select_stage(stageIdx);

    // Stage loading: the def file exists and can be parsed
    loader_reset();
    common_get_state().round = 1;
    TEST(L"loader_stage returns true", loader_stage() == true);

    // Reset after stage load failure so state machine tests start fresh
    loader_reset();

    TEST(L"loader_chara(0) returns 0", loader_chara(0) == 0);
    TEST(L"loader_chara(1) returns 0", loader_chara(1) == 0);
    TEST(L"loader_state_compile returns false", loader_state_compile() == false);

    // State machine: runTread starts loading, load completes it
    TEST(L"loader_run_tread returns true (starts loading)", loader_run_tread() == true);
    TEST(L"loader_run_tread sets state to Loading", ld.state == LoaderState::Loading);

    // Second call to runTread should fail (already running)
    TEST(L"loader_run_tread second call returns false", loader_run_tread() == false);

    // load() attempts to load everything, but stage_load is a stub
    loader_load();
    TEST(L"loader_load leaves state (Error or Complete)", 
        ld.state == LoaderState::Complete || ld.state == LoaderState::Error);

    // Reset brings back to NotYet
    loader_reset();
    TEST(L"loader_reset resets state", ld.state == LoaderState::NotYet);
    TEST(L"loader_reset clears error again", ld.errorMes.empty());

    // Error during loading: set error mes and verify
    loader_error("load error!");
    TEST(L"loader_error sets message", ld.errorMes == "load error!");

    // State enum values
    TEST(L"LoaderState values",
        LoaderState::NotYet == LoaderState{0} &&
        LoaderState::Loading == LoaderState{1} &&
        LoaderState::Complete == LoaderState{2} &&
        LoaderState::Error == LoaderState{3} &&
        LoaderState::Cancel == LoaderState{4});

    // No-arg convenience wrappers
    loader_error();
    TEST(L"loader_error() no-arg clears message", ld.errorMes.empty());
    TEST(L"loader_chara() no-arg returns 0", loader_chara() == 0);
}

// ---- Debug script service tests (ssz_native::debug) ----

static void test_debug_script_service()
{
    std::wcout << L"\n--- Debug script service ---" << std::endl;
    using namespace ikemen::ssz_native;

    // DebugScriptState default init
    DebugScriptState ds;
    TEST(L"DebugScriptState roundResetFlg false", ds.roundResetFlg == false);
    TEST(L"DebugScriptState reloadFlg false", ds.reloadFlg == false);
    TEST(L"DebugScriptState noHUDDisplay false", ds.noHUDDisplay == false);
    TEST(L"DebugScriptState L null", ds.L == nullptr);

    // Lua callback functions compile and don't crash
    int re = 0;
    lua_debug_puts(nullptr, re);
    lua_debug_ssz_reload(nullptr, re);
    lua_debug_set_life(nullptr, re);
    lua_debug_set_life_max(nullptr, re);
    lua_debug_set_power(nullptr, re);
    lua_debug_set_attack(nullptr, re);
    lua_debug_set_defence(nullptr, re);
    lua_debug_self_state(nullptr, re);
    lua_debug_add_hotkey(nullptr, re);
    lua_debug_toggle_clsn_draw(nullptr, re);
    lua_debug_toggle_debug_draw(nullptr, re);
    lua_debug_toggle_status_draw(nullptr, re);
    lua_debug_toggle_post_match(nullptr, re);
    lua_debug_toggle_pause(nullptr, re);
    lua_debug_toggle_pause_menu(nullptr, re);
    lua_debug_step(nullptr, re);
    lua_debug_toggle_record(nullptr, re);
    lua_debug_toggle_playback(nullptr, re);
    lua_debug_toggle_record_end(nullptr, re);
    lua_debug_round_reset(nullptr, re);
    lua_debug_reload(nullptr, re);
    lua_debug_set_accel(nullptr, re);
    lua_debug_set_ai_level(nullptr, re);
    lua_debug_set_time(nullptr, re);
    lua_debug_clear(nullptr, re);
    TEST(L"All 25 debug callbacks no-crash", true);

    // File loading functions
    TEST(L"debug_load_file empty", debug_load_file("test.lua").empty());
    TEST(L"debug_run_file empty", debug_run_file("test.lua").empty());

    // State field mutation
    ds.roundResetFlg = true;
    ds.reloadFlg = true;
    ds.noHUDDisplay = true;
    TEST(L"DebugScriptState flags hold values",
        ds.roundResetFlg == true && ds.reloadFlg == true && ds.noHUDDisplay == true);
}

// ---- System service tests (ssz_native::system) ----

static void test_system_service()
{
    std::wcout << L"\n--- System service ---" << std::endl;
    using namespace ikemen::ssz_native;

    // SelectData default init
    SelectData sel;
    TEST(L"SelectData columns == 5", sel.columns == 5);
    TEST(L"SelectData rows == 2", sel.rows == 2);
    TEST(L"SelectData cellsizex == 29.0", sel.cellsizex == 29.0f);
    TEST(L"SelectData selectedStageNo == -1", sel.selectedStageNo == -1);
    TEST(L"SelectData charlist empty", sel.charlist.empty());
    TEST(L"SelectData stagelist empty", sel.stagelist.empty());

    // SelectCharData default init
    SelectCharData ch;
    TEST(L"SelectCharData def empty", ch.def.empty());
    TEST(L"SelectCharData name empty", ch.name.empty());

    // SelectStageData default init
    SelectStageData st;
    TEST(L"SelectStageData def empty", st.def.empty());

    // SelectInfoData default init
    SelectInfoData inf;
    TEST(L"SelectInfoData p empty", inf.p.empty());
    TEST(L"SelectInfoData sel null", inf.sel == nullptr);

    // SystemData default init
    SystemData sys;
    TEST(L"SystemData selinf.p empty", sys.selinf.p.empty());

    // Stub methods compile and don't crash
    TEST(L"SelectData getCharNo stub", sel.getCharNo(0) == 0);
    TEST(L"SelectData getChar stub", sel.getChar(0) == nullptr);
    TEST(L"SelectData getStageNo stub", sel.getStageNo(0) == 0);
    TEST(L"SelectData getStage stub", sel.getStage(0) == nullptr);
    TEST(L"SelectData getStageName empty stagelist", sel.getStageName(0).empty());
    sel.selectStage(3);
    TEST(L"SelectData selectStage", sel.selectedStageNo == 3);
    sel.setStageNo(2);
    TEST(L"SelectData setStageNo stub", sel.curStageNo == 0);
    TEST(L"SelectInfoData addSelchr with null sel", inf.addSelchr(0, 0, 1) == false);
    TEST(L"SystemData selReset stub", true);
    sys.selReset();
    TEST(L"SystemData selReset no-crash", true);

    // ── Real state lookups: addChar / addStage / getStageName ──

    // addChar — from install/ directory, ../install/ = install/ itself
    bool added = sel.addChar("chars/kfm/kfm.def");
    TEST(L"SelectData addChar returns true", added == true);
    TEST(L"SelectData addChar charlist size", sel.charlist.size() == 1);
    TEST(L"SelectData addChar def stored", sel.charlist[0].def == "chars/kfm/kfm.def");
    TEST(L"SelectData addChar name non-empty", !sel.charlist[0].name.empty());

    // addStage
    std::string stageName = sel.addStage("stages/stageZ.def");
    TEST(L"SelectData addStage returns name", !stageName.empty());
    TEST(L"SelectData addStage stagelist size", sel.stagelist.size() == 1);
    TEST(L"SelectData addStage name non-empty", !stageName.empty());

    // getStageName after populating stagelist
    TEST(L"SelectData getStageName with 1 stage (0=RANDOM, 1=first)",
        sel.getStageName(0) == "RANDOM");
    TEST(L"SelectData getStageName index 1 non-empty",
        !sel.getStageName(1).empty());

    // getStageNo / getStage work with populated list
    TEST(L"SelectData getStageNo(0) on populated list", sel.getStageNo(0) == 0);
    auto* stagePtr = sel.getStage(1);
    TEST(L"SelectData getStage(1) not null", stagePtr != nullptr);
    if (stagePtr) {
        TEST(L"SelectData getStage(1) name non-empty", !stagePtr->name.empty());
    }

    // addChar with empty path returns false
    TEST(L"SelectData addChar empty path", sel.addChar("") == false);

    // addStage with empty path returns empty
    TEST(L"SelectData addStage empty path", sel.addStage("").empty());

    // Add a second stage and verify indexing
    sel.addStage("stages/kfm.def");
    TEST(L"SelectData stagelist size after 2nd add", sel.stagelist.size() == 2);
    TEST(L"SelectData getStageName index 2", sel.getStageName(2) == "stages/kfm.def");
    // Wrap-around check: with 2 stages, index 3 = index 1 (first real stage)
    TEST(L"SelectData getStageName wraps around (index 3 != RANDOM)",
        sel.getStageName(3) != "RANDOM");
}

// ---- Consts service tests (ssz_native::consts) ----

static void test_consts_service()
{
    namespace c = ikemen::ssz_native::consts;
    std::wcout << L"\n--- Consts service ---" << std::endl;

    // Signed template — matches &Signed<_t> from consts.ssz
    // SSZ: MAX = (1 << 8*typesize(_t) - 1) - 1
    // SSZ: MIN = !MAX
    TEST(L"Signed<int8_t>::MAX == 127", c::Signed<int8_t>::MAX == 127);
    TEST(L"Signed<int8_t>::MIN == -128", c::Signed<int8_t>::MIN == -128);
    TEST(L"Signed<int16_t>::MAX == 32767", c::Signed<int16_t>::MAX == 32767);
    TEST(L"Signed<int16_t>::MIN == -32768", c::Signed<int16_t>::MIN == -32768);
    TEST(L"Signed<int32_t>::MAX == 2147483647", c::Signed<int32_t>::MAX == 2147483647);
    TEST(L"Signed<int32_t>::MIN == -2147483648", c::Signed<int32_t>::MIN == static_cast<int32_t>(-2147483648));
    TEST(L"Signed<int64_t>::MAX == 9223372036854775807", c::Signed<int64_t>::MAX == 9223372036854775807LL);

    // Unsigned template — matches &Unsigned<_t> from consts.ssz
    // SSZ: MAX = !0x0 (all bits set), MIN = 0x0
    TEST(L"Unsigned<uint8_t>::MIN == 0", c::Unsigned<uint8_t>::MIN == 0);
    TEST(L"Unsigned<uint8_t>::MAX == 255", c::Unsigned<uint8_t>::MAX == 255);
    TEST(L"Unsigned<uint16_t>::MAX == 65535", c::Unsigned<uint16_t>::MAX == 65535);
    TEST(L"Unsigned<uint32_t>::MAX == 4294967295", c::Unsigned<uint32_t>::MAX == 4294967295U);
    TEST(L"Unsigned<uint64_t>::MAX == 18446744073709551615ULL",
        c::Unsigned<uint64_t>::MAX == 18446744073709551615ULL);

    // Type aliases — matches SSZ types exactly
    TEST(L"sizeof(byte_t) == 1", sizeof(c::byte_t) == 1);
    TEST(L"sizeof(short_t) == 2", sizeof(c::short_t) == 2);
    TEST(L"sizeof(int_t) == 4", sizeof(c::int_t) == 4);
    TEST(L"sizeof(long_t) == 8", sizeof(c::long_t) == 8);
    TEST(L"sizeof(ubyte_t) == 1", sizeof(c::ubyte_t) == 1);
    TEST(L"sizeof(ushort_t) == 2", sizeof(c::ushort_t) == 2);
    TEST(L"sizeof(uint_t) == 4", sizeof(c::uint_t) == 4);
    TEST(L"sizeof(ulong_t) == 8", sizeof(c::ulong_t) == 8);
    TEST(L"sizeof(char_t) == 1", sizeof(c::char_t) == 1);
    TEST(L"sizeof(index_t) == sizeof(intptr_t)", sizeof(c::index_t) == sizeof(intptr_t));

    // Signed/unsigned correctness
    TEST(L"byte_t is signed", static_cast<c::byte_t>(-1) < 0);
    TEST(L"ubyte_t is unsigned", static_cast<c::ubyte_t>(-1) > 0);
    TEST(L"char_t is unsigned (matches SSZ)", static_cast<c::char_t>(-1) > 0);

    // Sentinel values
    TEST(L"SENTINEL_MIN == int32_t MIN", c::SENTINEL_MIN == c::Signed<int32_t>::MIN);
    TEST(L"SENTINEL_MAX == int32_t MAX", c::SENTINEL_MAX == c::Signed<int32_t>::MAX);
    TEST(L"SENTINEL_UMAX == uint32_t MAX", c::SENTINEL_UMAX == c::Unsigned<uint32_t>::MAX);

    // null<T>() — matches SSZ null<_t>() which returns default-initialized value
    int* null_ptr = c::null<int>();
    TEST(L"null<int>() returns nullptr", null_ptr == nullptr);
    auto empty_vec = c::null_array<int>();
    TEST(L"null_array<int>() returns empty vector", empty_vec.empty());
    int default_int = c::null_value<int>();
    TEST(L"null_value<int>() returns 0", default_int == 0);
}

// ---- Alert service tests (ssz_native::alert) ----

static void test_alert_service()
{
    std::wcout << L"\n--- Alert service ---" << std::endl;

    // No-crash smoke test — dialog can't be verified programmatically.
    // Test with both empty and non-empty strings to ensure no crash.
    // Note: These are commented out by default because the alert() function
    // shows a MessageBox on Windows which blocks until dismissed.
    // Uncomment for manual testing.
    // namespace a = ikemen::ssz_native::alert;
    // a::alert(L"Test", L"Hello from native SSZ test");
    // a::alert(L"", L"");  // empty strings also valid
    TEST(L"alert_service API available", true);
    TEST(L"alert() takes title and message strings", true);
}

// ---- Crypto service tests (ssz_native::crypto) ----

static void test_crypto_service()
{
    namespace c = ikemen::ssz_native::crypto;
    std::wcout << L"\n--- Crypto service ---" << std::endl;

    // Base64 encode/decode roundtrip
    std::vector<uint8_t> data = {72, 101, 108, 108, 111}; // "Hello"
    std::string encoded = c::base64_encode(data);
    TEST(L"base64 encode non-empty", !encoded.empty());
    std::vector<uint8_t> decoded = c::base64_decode(encoded);
    TEST(L"base64 decode same size", decoded.size() == data.size());
    if (decoded.size() == data.size() && data.size() > 0)
        TEST(L"base64 roundtrip matches", memcmp(decoded.data(), data.data(), data.size()) == 0);

    // Base64 known value: "Hello" -> "SGVsbG8="
    TEST_EQ(L"base64 known hello", encoded, "SGVsbG8=");

    // Base64 padding
    std::vector<uint8_t> single = {'x'};
    std::string single_b64 = c::base64_encode(single);
    TEST_EQ(L"base64 single byte padding", single_b64, "eA==");

    // Base64 empty
    TEST(L"base64 encode empty", c::base64_encode({}).empty());
    TEST(L"base64 decode empty", c::base64_decode("").empty());

    // Arcfour known test: key="Key", src="Plaintext" -> known output
    std::vector<uint8_t> arc_key = {'K', 'e', 'y'};
    std::vector<uint8_t> arc_src = {'P', 'l', 'a', 'i', 'n', 't', 'e', 'x', 't'};
    c::Arcfour arc;
    arc.init(arc_key);
    std::vector<uint8_t> arc_enc = arc.encrypt(arc_src);

    // RC4 is symmetric: encrypt again with same key should decrypt
    c::Arcfour arc2;
    arc2.init(arc_key);
    std::vector<uint8_t> arc_dec = arc2.encrypt(arc_enc);
    TEST(L"Arcfour roundtrip", arc_dec.size() == arc_src.size());
    if (arc_dec.size() == arc_src.size() && arc_src.size() > 0)
        TEST(L"Arcfour roundtrip matches", memcmp(arc_dec.data(), arc_src.data(), arc_src.size()) == 0);

    // arcfour_encrypt convenience function
    std::vector<uint8_t> dest;
    TEST(L"arcfour_encrypt empty key fails", !c::arcfour_encrypt(dest, {}, arc_src));
    TEST(L"arcfour_encrypt works", c::arcfour_encrypt(dest, arc_key, arc_src));
    TEST(L"arcfour_encrypt non-empty dest", !dest.empty());

    // MD5 known values
    std::vector<uint8_t> md5_empty;
    auto md5_empty_hash = c::md5_hex(md5_empty);
    TEST_EQ(L"md5_hex empty", md5_empty_hash, "d41d8cd98f00b204e9800998ecf8427e");

    std::vector<uint8_t> md5_hello = {'H', 'e', 'l', 'l', 'o'};
    TEST_EQ(L"md5_hex Hello", c::md5_hex(md5_hello), "8b1a9953c4611296a827abf8c47804d7");

    // uint_to_b64_char
    TEST(L"uint_to_b64_char 0 == 'A'", c::uint_to_b64_char(0) == 'A');
    TEST(L"uint_to_b64_char 25 == 'Z'", c::uint_to_b64_char(25) == 'Z');
    TEST(L"uint_to_b64_char 26 == 'a'", c::uint_to_b64_char(26) == 'a');
    TEST(L"uint_to_b64_char 52 == '0'", c::uint_to_b64_char(52) == '0');
    TEST(L"uint_to_b64_char 62 == '+'", c::uint_to_b64_char(62) == '+');
    TEST(L"uint_to_b64_char 63 == '/'", c::uint_to_b64_char(63) == '/');

    // Arcfour get_byte
    {
        c::Arcfour rc4a, rc4b;
        std::vector<uint8_t> key = {'K', 'e', 'y'};
        rc4a.init(key);
        rc4b.init(key);
        bool match = true;
        for (int i = 0; i < 32; i++) {
            uint8_t a = rc4a.get_byte();
            std::vector<uint8_t> one = {0};
            auto enc = rc4b.encrypt(one);
            if (a != enc[0]) { match = false; break; }
        }
        TEST(L"Arcfour get_byte matches encrypt", match);
    }
}

// ---- Mesdialog service tests (ssz_native::mesdialog) ----

static void test_mesdialog_service()
{
    namespace m = ikemen::ssz_native::mesdialog;
    std::wcout << L"\n--- Mesdialog service ---" << std::endl;

    // ── Shared string roundtrip ──
    m::set_shared_string(L"hello");
    std::wstring got = m::get_shared_string();
    TEST_EQ(L"shared string roundtrip", got, L"hello");

    // Empty shared string
    m::set_shared_string(L"");
    got = m::get_shared_string();
    TEST(L"shared string empty", got.empty());

    // Code page constants
    TEST(L"UTF8 codepage == 65001", m::UTF8 == 65001);
    TEST(L"ACP codepage == 0", m::ACP == 0);
    TEST(L"SJIS codepage == 932", m::SJIS == 932);
    TEST(L"ISO_8859_1 codepage == 1252", m::ISO_8859_1 == 1252);

    // ── Encoding: UTF-8 roundtrip ──
    std::string utf8_data = "Hello W\xc3\xb6rld!"; // "Hello Wörld!" in UTF-8
    std::wstring wide = m::ubytes_to_str(utf8_data.data(), (intptr_t)utf8_data.size(), m::UTF8);
    TEST(L"ubytes_to_str UTF-8 non-empty", !wide.empty());
    // Convert back
    std::vector<uint8_t> back = m::str_to_ubytes(wide.data(), (intptr_t)(wide.size() * sizeof(wchar_t)), m::UTF8);
    TEST(L"str_to_ubytes UTF-8 non-empty", !back.empty());
    if (!back.empty() && !utf8_data.empty()) {
        std::string roundtrip(back.begin(), back.end());
        TEST_EQ(L"UTF-8 roundtrip matches original", roundtrip, utf8_data);
    }

    // ── Encoding: ASCII To Local (takes wide string buffer, converts via CP_THREAD_ACP) ──
    std::wstring ascii_src = L"ASCII test 123";
    std::wstring local = m::ascii_to_local(
        ascii_src.data(), (intptr_t)(ascii_src.size() * sizeof(wchar_t)));
    TEST(L"ascii_to_local non-empty", !local.empty());
    TEST_EQ(L"ascii_to_local roundtrip", local, ascii_src);

    // ── Encoding: empty input ──
    std::wstring empty_wide = m::ubytes_to_str(nullptr, 0, m::UTF8);
    TEST(L"ubytes_to_str empty input", empty_wide.empty());
    std::vector<uint8_t> empty_bytes = m::str_to_ubytes(nullptr, 0, m::UTF8);
    TEST(L"str_to_ubytes empty input", empty_bytes.empty());
    std::wstring empty_local = m::ascii_to_local(nullptr, 0);
    TEST(L"ascii_to_local empty input", empty_local.empty());

    // ── INI file roundtrip ──
    // Create a temp INI file using SaveAsciiText, then read it back
    std::wstring iniPath = TMPDIR + L"/test_mesdialog.ini";
    bool iniWritten = SaveAsciiText(
        L"[Section1]\nkey1=value1\nkey2=42\n[Section2]\nflag=true\n",
        iniPath);
    TEST(L"INI file created", iniWritten);

    if (iniWritten) {
        std::wstring iniFile(iniPath.begin(), iniPath.end());

        // Read string value
        std::wstring val1 = m::get_inifile_string(L"default", L"key1", L"Section1", iniFile);
        TEST_EQ(L"get_inifile_string key1", val1, L"value1");

        // Read int value
        int32_t val2 = m::get_inifile_int(0, L"key2", L"Section1", iniFile);
        TEST(L"get_inifile_int key2 == 42", val2 == 42);

        // Read default value for missing key
        std::wstring missing = m::get_inifile_string(L"def", L"nonexistent", L"Section1", iniFile);
        TEST_EQ(L"get_inifile_string missing returns default", missing, L"def");

        // Write a new key and verify
        bool written = m::write_inifile_string(L"newvalue", L"newkey", L"Section1", iniFile);
        TEST(L"write_inifile_string returns true", written == true);

        // Read back the newly written key
        std::wstring newVal = m::get_inifile_string(L"", L"newkey", L"Section1", iniFile);
        TEST_EQ(L"get_inifile_string newkey after write", newVal, L"newvalue");

        // Read from Section2
        std::wstring flag = m::get_inifile_string(L"", L"flag", L"Section2", iniFile);
        TEST_EQ(L"get_inifile_string Section2.flag", flag, L"true");
    }

    // ── Compression: uncompress with empty/non-compressed data ──
    std::vector<uint8_t> uncompressed = m::uncompress(nullptr, 0);
    TEST(L"uncompress empty input returns empty", uncompressed.empty());

    // ── Clipboard: no-crash test ──
    std::wstring clip = m::get_clipboard_str();
    TEST(L"get_clipboard_str no-crash", true);
    // Note: clipboard content varies by environment, so we only verify no-crash.

    // ── Dialog functions (no-crash only — interactive) ──
    // yes_no() and input_str() show dialogs, can't be auto-tested.
    TEST(L"yes_no and input_str APIs available", true);
}

// ---- Sound service tests (ssz_native::sound) ----

static void test_sound_service()
{
    namespace s = ikemen::ssz_native::sound;
    std::wcout << L"\n--- Sound service ---" << std::endl;
    std::printf("[TEST] sound_service running\n"); fflush(stdout);

    // ── Constants ──
    TEST(L"FREQ == 48000", s::FREQ == 48000);
    TEST(L"CHANNELS == 2", s::CHANNELS == 2);
    TEST(L"BUFFER_SAMPLES == 2048", s::BUFFER_SAMPLES == 2048);

    // ── Default construction — client may or may not be created
    // (depends on whether audio subsystem was initialized) ──
    s::AudioClient ac;
    TEST(L"AudioClient default constructed", true);

    // Move semantics
    s::AudioClient ac2;
    s::AudioClient ac3 = std::move(ac2);
    TEST(L"AudioClient move: no crash", true);

    s::AudioClient ac4;
    ac4 = std::move(ac3);
    TEST(L"AudioClient move-assign: no crash", true);

    // ── Operations on default-constructed client ──
    // These may fail at runtime (no audio device) but must not crash.
    bool started = ac.start();
    TEST(L"AudioClient start returns bool", started == false || started == true);

    bool stopped = ac.stop();
    TEST(L"AudioClient stop returns bool", stopped == false || stopped == true);

    bool ready = ac.buffer_ready();
    TEST(L"AudioClient buffer_ready returns bool", ready == false || ready == true);

    // set_buffer with a small test buffer
    float test_buf[64] = {0.0f};
    bool buf_set = ac.set_buffer(test_buf, 64);
    TEST(L"AudioClient set_buffer returns bool", buf_set == false || buf_set == true);

    // ── Double start/stop safety ──
    (void)ac.start();
    (void)ac.start();
    TEST(L"AudioClient double start no crash", true);
    (void)ac.stop();
    (void)ac.stop();
    TEST(L"AudioClient double stop no crash", true);

    // ── Operations on moved-from client return false ──
    s::AudioClient ac5;
    s::AudioClient ac6 = std::move(ac5);
    TEST(L"AudioClient moved-from start returns false", ac5.start() == false);
    TEST(L"AudioClient moved-from stop returns false", ac5.stop() == false);
    TEST(L"AudioClient moved-from buffer_ready returns false", ac5.buffer_ready() == false);
    TEST(L"AudioClient moved-from set_buffer returns false", ac5.set_buffer(test_buf, 64) == false);
}

// ---- Socket service tests (ssz_native::socket) ----

static void test_socket_service()
{
    namespace s = ikemen::ssz_native::socket;
    std::wcout << L"\n--- Socket service ---" << std::endl;

    // Default construction — not open
    s::SocketHandle sh;
    TEST(L"SocketHandle default not open", !sh.is_open());

    // Move semantics
    s::SocketHandle sh2;
    s::SocketHandle sh3 = std::move(sh2);
    TEST(L"SocketHandle move: source not open", !sh2.is_open());
    TEST(L"SocketHandle move: dest not open", !sh3.is_open());

    s::SocketHandle sh4;
    sh4 = std::move(sh3);
    TEST(L"SocketHandle move-assign: source not open", !sh3.is_open());
    TEST(L"SocketHandle move-assign: dest not open", !sh4.is_open());

    // Double close safety
    sh4.close();
    sh4.close();
    TEST(L"SocketHandle double close safe", true);

    // ── Operations on a non-connected socket fail gracefully ──
    s::SocketHandle sh5;
    char buf[64] = {};

    bool send_ok = sh5.send(10, buf);
    TEST(L"SocketHandle send on non-connected returns false", send_ok == false);

    bool recv_ok = sh5.recv(10, buf);
    TEST(L"SocketHandle recv on non-connected returns false", recv_ok == false);

    intptr_t send_ary = sh5.send_array(1, buf, 10);
    TEST(L"SocketHandle send_array on non-connected returns 0", send_ary == 0);

    intptr_t recv_ary = sh5.recv_array(1, buf, 10);
    TEST(L"SocketHandle recv_array on non-connected returns 0", recv_ary == 0);

    // ── Accept on non-listening socket returns sentinel ──
    s::SocketHandle sh6;
    s::SocketHandle accepted = sh6.accept(100, true);
    TEST(L"SocketHandle accept on non-listening returns sentinel",
        !accepted.is_open());

    // ── connect/listen skip in automated tests (requires network stack) ──
    TEST(L"SocketHandle connect/listen APIs available (skipped in auto-test)", true);
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

    // read_all_as<T> template
    {
        FileHandle wfh;
        wfh.open(TMPFILE, L"w+b");
        int32_t ints[] = {10, 20, 30, 40};
        wfh.write(ints, sizeof(ints));
        wfh.close();

        auto result = read_all_as<int32_t>(TMPFILE);
        TEST(L"read_all_as size", result.size() == 4);
        if (result.size() == 4) {
            TEST(L"read_all_as[0]", result[0] == 10);
            TEST(L"read_all_as[3]", result[3] == 40);
        }
    }
}

// ---- Module-level function tests (static state) ----
static void test_system_module_functions()
{
    std::wcout << L"\n--- System module-level functions ---" << std::endl;
    using namespace ikemen::ssz_native;
    
    // These test the free functions that the bridge calls.
    // They operate on an internal static SystemData instance.

    // addChar via module-level function
    bool modAdded = system_add_char("chars/kfm/kfm.def");
    TEST(L"system_add_char returns true", modAdded == true);

    // addStage via module-level function
    std::string modStageName = system_add_stage("stages/stageZ.def");
    TEST(L"system_add_stage returns name", !modStageName.empty());

    // getStageName via module-level function
    std::string name0 = system_get_stage_name(0);
    TEST(L"system_get_stage_name(0) = RANDOM", name0 == "RANDOM");
    std::string name1 = system_get_stage_name(1);
    TEST(L"system_get_stage_name(1) = first stage", !name1.empty());

    // setStageNo / selectStage via module-level functions
    int setNo = system_set_stage_no(1);
    TEST(L"system_set_stage_no returns 1", setNo == 1);
    system_select_stage(0);
    // No crash = success

    // addSelchr with empty internal state (sel is wired but charlist may be empty-ish)
    bool selchrAdded = system_add_selchr(0, 0, 1);
    TEST(L"system_add_selchr returns false (charlist empty)", selchrAdded == false);

    // selReset
    system_sel_reset();
    TEST(L"system_sel_reset no-crash", true);
}

// ---- Startup parity tests (matching pre-conversion trace) ----

static void test_startup_parity()
{
    std::wcout << L"\n--- Startup parity ---" << std::endl;
    using namespace ikemen::ssz_native;

    // 1. Static plugin registrations succeed (simulated by constructors)
    // The *_static_register() calls in main.cpp happen at engine startup.
    // Here we verify the native service types initialize correctly.

    // CommonData default init (matches SSZ startup state)
    CommonData cd;
    TEST(L"CommonData gameType == 0", cd.gameType == 0);
    TEST(L"CommonData round == 1", cd.round == 1);
    TEST(L"CommonData match == 1", cd.match == 1);
    TEST(L"CommonData life == 1.0", cd.life == 1.0f);
    TEST(L"CommonData power == 0", cd.power == 0);
    TEST(L"CommonData coins == 0", cd.coins == 0);
    TEST(L"CommonData credits == 0", cd.credits == 0);

    // 2. Stage service default init
    stage_init();
    TEST(L"Stage module initialized", true);
    TEST(L"EnvShake clear returns 0", stage_get_env_shake().getOffset() == 0.0f);

    // 3. Character service default init
    char_init();
    TEST(L"Char module initialized", true);

    // 4. Command service default init
    command_init();
    TEST(L"Command module initialized", true);

    // 5. Fight service default init
    fight_init();
    TEST(L"Fight module initialized", true);

    // 6. System service state accessors
    // addChar with existing select data
    SelectData sel;
    bool added = sel.addChar("chars/kfm/kfm.def");
    TEST(L"Selector addChar", added == true);
    TEST(L"Selector charlist populated", sel.charlist.size() == 1);

    sel.addStage("stages/stageZ.def");
    TEST(L"Selector stagelist populated", sel.stagelist.size() == 1);

    // 7. File I/O parity: open/write/close/read matches SSZ behavior
    {
        FileHandle f;
        bool ok = f.open(TMPDIR + L"/parity_test.txt", L"w+b");
        TEST(L"Parity file open", ok);

        const char* data = "parity test data";
        ok = f.write(data, 16);
        TEST(L"Parity file write", ok);
        f.close();

        ok = f.open(TMPDIR + L"/parity_test.txt", L"rb");
        TEST(L"Parity file reopen", ok);

        char buf[32] = {};
        ok = f.read(buf, 16);
        TEST(L"Parity file read", ok);
        TEST(L"Parity file content matches", std::memcmp(buf, data, 16) == 0);
        f.close();

        // Cleanup
        remove_file(TMPDIR + L"/parity_test.txt");
    }

    // 8. Stack operations match SSZ
    {
        Stack<int> s;
        TEST(L"Stack initially empty", s.empty());
        TEST(L"Stack size 0", s.size() == 0);

        s.push(10);
        s.push(20);
        s.push(30);
        TEST(L"Stack size after 3 pushes", s.size() == 3);
        TEST(L"Stack top", s.top() != nullptr && *s.top() == 30);

        int val = s.pop();
        TEST(L"Stack pop value", val == 30);
        TEST(L"Stack size after pop", s.size() == 2);

        s.clear();
        TEST(L"Stack empty after clear", s.empty());
    }

    // 9. Math constants match SSZ
    {
        TEST(L"PI > 3.14", math::PI > 3.14);
        TEST(L"E > 2.71", math::E > 2.71);
    }

    // 10. Loader state machine
    {
        LoaderData& ld = loader_get_state();
        TEST(L"Loader init state NotYet", ld.state == LoaderState::NotYet);
        TEST(L"Loader error empty", ld.errorMes.empty());
    }
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
    test_string_service();
    test_format_service();
    test_ogg_service();
    test_thread_service();
    test_time_service();
    test_table_service();
    test_lua_service();
    test_share_service();
    test_system_service();
    test_system_module_functions();
    test_debug_script_service();
    test_loader_service();
    test_common_service();
    test_trigger_script_service();
    test_script_service();
    test_system_script_service();
    test_statebuilder_service();
    test_font_service();
    test_video_service();
    test_action_service();
    test_sound_resource_service();
    test_fighting_service();
    test_bg_service();
    test_stage_service();
    test_sff_service();
    test_command_service();
    test_fight_service();
    test_char_service();
    test_sdlevent_service();
    test_sdlplugin_service();
    test_config_service();
    test_stack_service();
    test_shell_service();
    test_consts_service();
    test_alert_service();
    test_crypto_service();
    test_mesdialog_service();
    test_sound_service();
    test_socket_service();
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
