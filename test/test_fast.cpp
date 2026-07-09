// test_fast.cpp — Fast unit tests for foundation native SSZ modules.
//
// Tests only pure C++ native services that DON'T depend on SDL, OpenGL,
// audio, or video subsystems. Links only essential plugin objects + native
// service .o files — NO external libraries.
//
// Build: make test-fast        # → build/Debug/test_fast.exe
// Expected: ~120+ assertions, 0 failures

#include <stdint.h>
#include <string>
#include <vector>
#include <cstdio>
#include <cassert>
#include <cstring>
#include <iostream>
#include <cmath>
#include <limits>
#include <windows.h>
#include <setjmp.h>

#define SDL_MAIN_HANDLED
#include "sszdef.h"
#include "ssz_native/plugin_native_api.hpp"

// Foundation native service headers (no SDL-dependent modules, no Lua, no char)
#include "ssz_native/file_service.hpp"
#include "ssz_native/math_service.hpp"
#include "ssz_native/regex_service.hpp"
#include "ssz_native/socket_service.hpp"
#include "ssz_native/string_service.hpp"
#include "ssz_native/mesdialog_service.hpp"
#include "ssz_native/crypto_service.hpp"
#include "ssz_native/thread_service.hpp"
#include "ssz_native/time_service.hpp"
#include "ssz_native/shell_service.hpp"
#include "ssz_native/common_service.hpp"
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

// =========================================================================
// File I/O helpers using std::wstring API from plugin_native_api.hpp
// =========================================================================

static const std::wstring TMPDIR = L"__ikemen_test_tmp";
static const std::wstring TMPFILE = TMPDIR + L"/test.txt";
static const std::wstring TMPFILE2 = TMPDIR + L"/moved.txt";
static const std::wstring TMPFILE3 = TMPDIR + L"/copied.txt";

static bool setup()
{
    CreateDir(TMPDIR);
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

// =========================================================================
// Test cases
// =========================================================================

static void test_open_write_close_read()
{
    std::wcout << L"\n--- Open/Write/Close/Read ---" << std::endl;
    intptr_t fd = Open(L"w+b", TMPFILE);
    FILE* f = reinterpret_cast<FILE*>(fd);
    TEST(L"Open write", f != nullptr);
    if (!f) return;
    const char* data = "Hello, Ikemen!";
    bool ok = Write(14, data, f);
    TEST(L"Write", ok);
    FileClose(f);
    fd = Open(L"rb", TMPFILE);
    f = reinterpret_cast<FILE*>(fd);
    TEST(L"Open read", f != nullptr);
    if (!f) return;
    char buf[32] = {};
    ok = Read(14, buf, f);
    TEST(L"Read", ok);
    TEST(L"Read content matches", memcmp(buf, data, 14) == 0);
    FileClose(f);
}

static void test_seek()
{
    std::wcout << L"\n--- Seek ---" << std::endl;
    intptr_t fd = Open(L"w+b", TMPFILE);
    FILE* f = reinterpret_cast<FILE*>(fd);
    if (!f) return;
    Write(10, "0123456789", f);
    Seek(0, 0, f);
    char c; Read(1, &c, f);
    TEST_EQ(L"Seek SET 0 read", c, '0');
    Seek(0, 5, f); Read(1, &c, f);
    TEST_EQ(L"Seek SET 5 read", c, '5');
    Seek(1, 2, f); Read(1, &c, f);
    TEST_EQ(L"Seek CUR +2 read", c, '8');
    Seek(2, -3, f); Read(1, &c, f);
    TEST_EQ(L"Seek END -3 read", c, '7');
    FileClose(f);
}

static void test_write_read_ary()
{
    std::wcout << L"\n--- WriteAry/ReadAry ---" << std::endl;
    intptr_t fd = Open(L"w+b", TMPFILE);
    FILE* f = reinterpret_cast<FILE*>(fd);
    if (!f) return;
    int32_t src[] = {1, 2, 3, 4, 5};
    intptr_t total = sizeof(src);
    intptr_t written = WriteAry(sizeof(int32_t), src, total, f);
    TEST_INT(L"WriteAry count", 5, written);
    FileClose(f);
    fd = Open(L"rb", TMPFILE);
    f = reinterpret_cast<FILE*>(fd);
    if (!f) return;
    int32_t dst[5] = {};
    intptr_t read = ReadAry(sizeof(int32_t), dst, total, f);
    TEST_INT(L"ReadAry count", 5, read);
    for (int i = 0; i < 5; i++)
        TEST_INT((L"ReadAry element " + std::to_wstring(i)).c_str(), src[i], dst[i]);
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
    SaveAsciiText(L"delete me", TMPFILE);
    TEST(L"Delete existing file", Delete(TMPFILE));
    intptr_t fd = Open(L"rb", TMPFILE);
    TEST(L"Delete verified (file gone)", fd == 0);
}

static void test_move()
{
    std::wcout << L"\n--- Move ---" << std::endl;
    SaveAsciiText(L"move me", TMPFILE);
    Delete(TMPFILE2);
    TEST(L"Move", Move(TMPFILE2, TMPFILE));
    intptr_t fd = Open(L"rb", TMPFILE);
    TEST(L"Move source gone", fd == 0);
    fd = Open(L"rb", TMPFILE2);
    TEST(L"Move dest exists", fd != 0);
    if (fd) FileClose(reinterpret_cast<FILE*>(fd));
}

static void test_copy()
{
    std::wcout << L"\n--- Copy ---" << std::endl;
    SaveAsciiText(L"copy me", TMPFILE);
    Delete(TMPFILE3);
    TEST(L"Copy", Copy(false, TMPFILE3, TMPFILE));
    intptr_t f = Open(L"rb", TMPFILE);
    intptr_t g = Open(L"rb", TMPFILE3);
    TEST(L"Copy source exists", f != 0);
    TEST(L"Copy dest exists", g != 0);
    if (f) FileClose(reinterpret_cast<FILE*>(f));
    if (g) FileClose(reinterpret_cast<FILE*>(g));
    TEST(L"Copy no overwrite", !Copy(false, TMPFILE3, TMPFILE));
}

static void test_create_remove_dir()
{
    std::wcout << L"\n--- CreateDir/RemoveDir ---" << std::endl;
    const std::wstring dir = TMPDIR + L"/__subdir";
    RemoveDir(dir);
    bool ok = CreateDir(dir);
    TEST(L"CreateDir", ok);
    ok = CreateDir(dir);
    TEST(L"CreateDir existing false", !ok);
    ok = RemoveDir(dir);
    TEST(L"RemoveDir", ok);
    intptr_t fd = Open(L"w", dir + L"/nope.txt");
    TEST(L"RemoveDir verified", fd == 0);
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
    SaveAsciiText(L"a", TMPDIR + L"/alpha_find.txt");
    SaveAsciiText(L"b", TMPDIR + L"/beta_find.txt");
    SaveAsciiText(L"c", TMPDIR + L"/gamma_find.dat");
    auto files = Find(TMPDIR + L"/*_find.txt");
    TEST_INT(L"Find count", 2, files.size());
    Delete(TMPDIR + L"/alpha_find.txt");
    Delete(TMPDIR + L"/beta_find.txt");
    Delete(TMPDIR + L"/gamma_find.dat");
}

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
    TEST(L"Exp(0) == 1", Exp(0.0) == 1.0);
    TEST(L"Sqrt(4) == 2", Sqrt(4.0) == 2.0);
    TEST(L"Ceil(1.5) == 2", Ceil(1.5) == 2.0);
    TEST(L"Floor(1.5) == 1", Floor(1.5) == 1.0);
    TEST(L"IsFinite(0) true", IsFinite(0.0));
}

static void test_thread()
{
    std::wcout << L"\n--- Thread ---" << std::endl;
    ThreadDelay(0);
    ThreadDelay(1);
    TEST(L"ThreadDelay no crash", true);
}

// ---- Math service tests ----
static void test_math_service()
{
    namespace m = ikemen::ssz_native::math;
    std::wcout << L"\n--- Math service ---" << std::endl;
    TEST(L"PI > 3.14", m::PI > 3.14);
    TEST(L"E > 2.71", m::E > 2.71);
    TEST(L"sin(0) == 0", m::sin(0.0) == 0.0);
    TEST(L"cos(0) == 1", m::cos(0.0) == 1.0);
    m::srand(12345); int32_t a = m::random();
    m::srand(12345); int32_t b = m::random();
    TEST(L"PRNG deterministic", a == b);
    m::srand(1); TEST_INT(L"PRNG Park-Miller", 16807, m::random());
    TEST_INT(L"PRNG seed=1 2nd", 282475249, m::random());
    for (int i = 0; i < 10; i++)
        TEST(L"rand(5,10) in range", m::rand(5, 10) >= 5 && m::rand(5, 10) <= 10);
    TEST(L"min(3,7)==3", m::min(3, 7) == 3);
    TEST(L"max(3,7)==7", m::max(3, 7) == 7);
    int val = 10; m::limMax(val, 7); TEST(L"limMax->7", val == 7);
    int x = 1, y = 2; m::swap(x, y); TEST(L"swap", x == 2 && y == 1);
}

// ---- String service tests ----
static void test_string_service()
{
    namespace s = ikemen::ssz_native::string_util;
    std::wcout << L"\n--- String service ---" << std::endl;
    TEST(L"equ same", s::equ(L"abc", L"abc"));
    TEST_EQ(L"trim spaces", s::trim(L"  hello  "), L"hello");
    TEST_INT(L"find start", 0, s::find(L"abc", L"abcdef"));
    TEST_INT(L"find not found", -1, s::find(L"xyz", L"abcdef"));
    auto parts = s::split(L",", L"a,b,c");
    TEST(L"split 3 parts", parts.size() == 3);
    std::vector<std::wstring> words = {L"a", L"b", L"c"};
    TEST_EQ(L"join", s::join(L",", words), L"a,b,c");
    auto lines = s::split_lines(L"line1\r\nline2\nline3");
    TEST(L"split_lines 3", lines.size() == 3);
    std::wstring orig = L"Hello, \u4e16\u754c!";
    auto utf8 = s::to_utf8(orig);
    TEST_EQ(L"utf8 roundtrip", s::from_utf8(utf8), orig);
    TEST_EQ(L"hex lower 255", s::to_hex_lower(255), L"ff");
    TEST_EQ(L"hex upper 255", s::to_hex_upper(255), L"FF");
    TEST_EQ(L"octal 8", s::to_octal(8), L"10");
    double d; TEST(L"s_to_number double", s::s_to_number(d, L"3.14"));
    auto ary = s::sv_to_ary<int32_t>(L",", L"1,2,3");
    TEST(L"sv_to_ary size", ary.size() == 3);
}

// ---- Stack service tests ----
static void test_stack_service()
{
    using namespace ikemen::ssz_native;
    std::wcout << L"\n--- Stack service ---" << std::endl;
    Stack<int> s;
    TEST(L"Stack initially empty", s.empty());
    s.push(42); s.push(100);
    TEST(L"Stack pop LIFO", s.pop() == 100 && s.pop() == 42);
    TEST(L"Stack empty after pops", s.empty());
    s.push(1); s.push(2); s.push(3); s.clear();
    TEST(L"Stack empty after clear", s.empty());
    Stack<int> s2;
    TEST(L"Stack top on empty nullptr", s2.top() == nullptr);
    s2.push(42); s2.push(99);
    const int* tp = s2.top();
    TEST(L"Stack top returns last", tp != nullptr && *tp == 99);
}

// ---- Crypto service tests ----
static void test_crypto_service()
{
    namespace c = ikemen::ssz_native::crypto;
    std::wcout << L"\n--- Crypto service ---" << std::endl;
    std::vector<uint8_t> data = {72, 101, 108, 108, 111};
    std::string encoded = c::base64_encode(data);
    auto decoded = c::base64_decode(encoded);
    TEST(L"base64 roundtrip", decoded == data);
    TEST_EQ(L"base64 known", encoded, "SGVsbG8=");
    TEST(L"base64 encode empty", c::base64_encode({}).empty());
    std::vector<uint8_t> key = {'K', 'e', 'y'};
    std::vector<uint8_t> src = {'P', 'l', 'a', 'i', 'n', 't', 'e', 'x', 't'};
    c::Arcfour arc; arc.init(key); auto enc = arc.encrypt(src);
    c::Arcfour arc2; arc2.init(key); auto dec = arc2.encrypt(enc);
    TEST(L"Arcfour roundtrip", dec == src);
    TEST_EQ(L"md5 empty", c::md5_hex({}), "d41d8cd98f00b204e9800998ecf8427e");
    std::vector<uint8_t> hello = {'H', 'e', 'l', 'l', 'o'};
    TEST_EQ(L"md5 Hello", c::md5_hex(hello), "8b1a9953c4611296a827abf8c47804d7");
}

// ---- Time service tests ----
static void test_time_service()
{
    namespace t = ikemen::ssz_native::time_util;
    std::wcout << L"\n--- Time service ---" << std::endl;
    TEST(L"tick_count > 0", t::tick_count() > 0);
    int64_t ut = t::unix_time();
    TEST(L"unix_time > 1e9", ut > 1000000000);
    TEST(L"unix_time in 2026 range", ut > 1700000000 && ut < 2000000000);
}

// ---- Config service tests ----
static void test_config_service()
{
    std::wcout << L"\n--- Config service ---" << std::endl;
    using namespace ikemen::ssz_native;
    ConfigData cfg;
    TEST(L"Config Width 640", cfg.Width == 640);
    TEST(L"Config Height 480", cfg.Height == 480);
    TEST(L"Config GameSpeed 60", cfg.GameSpeed == 60);
    ConfigData saved = make_default_config();
    saved.Width = 800; saved.Height = 600;
    bool saved_ok = config_save("test_config_fast.ini", saved);
    TEST(L"config_save succeeds", saved_ok);
    if (saved_ok) {
        ConfigData loaded;
        TEST(L"config_load succeeds", config_load("test_config_fast.ini", loaded));
        TEST(L"config roundtrip Width", loaded.Width == 800);
        std::remove("test_config_fast.ini");
    }
    TEST(L"config_load nonexistent fails", !config_load("nonexistent_config.ini", cfg));
}

// ---- Consts service tests ----
static void test_consts_service()
{
    namespace c = ikemen::ssz_native::consts;
    std::wcout << L"\n--- Consts service ---" << std::endl;
    TEST(L"Signed<int8_t>::MAX==127", c::Signed<int8_t>::MAX == 127);
    TEST(L"Signed<int8_t>::MIN==-128", c::Signed<int8_t>::MIN == -128);
    TEST(L"Unsigned<uint8_t>::MAX==255", c::Unsigned<uint8_t>::MAX == 255);
    TEST(L"sizeof(byte_t)==1", sizeof(c::byte_t) == 1);
    TEST(L"sizeof(int_t)==4", sizeof(c::int_t) == 4);
    TEST(L"null<int>()==nullptr", c::null<int>() == nullptr);
    TEST(L"null_array<int> empty", c::null_array<int>().empty());
    TEST(L"null_value<int>()==0", c::null_value<int>() == 0);
}

// ---- Shell service tests ----
static void test_shell_service()
{
    namespace s = ikemen::ssz_native::shell;
    std::wcout << L"\n--- Shell service ---" << std::endl;
    s::open(L"", L"", L"", false, false);
    TEST(L"shell::open no-crash", true);
    TEST(L"shell::move_to_trash nonexistent", !s::move_to_trash(L"__nonexistent__"));
}

// =========================================================================
// Main
// =========================================================================

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
    test_stack_service();
    test_crypto_service();
    test_time_service();
    test_config_service();
    test_consts_service();
    test_shell_service();

    cleanup();

    std::wcout << L"\n=== " << g_tests << L" tests, " << g_fails << L" failures ===" << std::endl;

    if (g_fails > 0) {
        std::wcerr << L"Failures detected." << std::endl;
    } else {
        RemoveDir(TMPDIR);
    }

    return g_fails > 0 ? 1 : 0;
}
