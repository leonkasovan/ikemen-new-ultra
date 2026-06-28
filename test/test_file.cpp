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
#include "ssz_native/socket_service.hpp"
#include "ssz_native/sound_service.hpp"
#include "ssz_native/string_service.hpp"
#include "ssz_native/ogg_service.hpp"
#include "ssz_native/mesdialog_service.hpp"
#include "ssz_native/crypto_service.hpp"

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
}

// ---- Ogg service tests (ssz_native::ogg) ----

static void test_ogg_service()
{
    namespace o = ikemen::ssz_native::ogg;
    std::wcout << L"\n--- Ogg service ---" << std::endl;

    // Default construction — decoder created
    o::OggVorbisHandle ov;
    TEST(L"OggVorbisHandle constructed", ov.is_valid());

    // Move semantics
    o::OggVorbisHandle ov2;
    o::OggVorbisHandle ov3 = std::move(ov2);
    TEST(L"OggVorbisHandle move: source invalid", !ov2.is_valid());
    TEST(L"OggVorbisHandle move: dest valid", ov3.is_valid());

    o::OggVorbisHandle ov4;
    ov4 = std::move(ov3);
    TEST(L"OggVorbisHandle move-assign: source invalid", !ov3.is_valid());
    TEST(L"OggVorbisHandle move-assign: dest valid", ov4.is_valid());

    // Self-move-assignment safety
    o::OggVorbisHandle ov5;
    ov5 = std::move(ov5);
    TEST(L"OggVorbisHandle self-move safe", ov5.is_valid());

    // Can't test open/read without a real .ogg file, but verify no-crash
    // on a valid-but-non-opened decoder handle
    o::OggVorbisHandle ov6;
    ov6.clear();
    ov6.pcm_total();
    ov6.channels();
    ov6.rate();
    ov6.seek(0.0);
    int16_t buf2[16];
    ov6.read(buf2, 16);
    TEST(L"OggVorbisHandle clear/pcm/rate/read/seek no crash on non-opened handle", true);

    // Operations on a moved-from (null) handle must not crash
    o::OggVorbisHandle ov7;
    o::OggVorbisHandle ov8 = std::move(ov7);
    ov7.clear();
    ov7.pcm_total();
    ov7.read(buf2, 16);
    TEST(L"OggVorbisHandle operations no crash on null handle", true);
}

// ---- Alert service tests (ssz_native::alert) ----

static void test_alert_service()
{
    // No-crash smoke test — dialog can't be verified programmatically.
    // ikemen::ssz_native::alert::alert(L"test", L"test");
    TEST(L"alert_service header compiles", true);
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
}

// ---- Mesdialog service tests (ssz_native::mesdialog) ----

static void test_mesdialog_service()
{
    namespace m = ikemen::ssz_native::mesdialog;
    std::wcout << L"\n--- Mesdialog service ---" << std::endl;

    // Shared string roundtrip
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
}

// ---- Sound service tests (ssz_native::sound) ----

static void test_sound_service()
{
    namespace s = ikemen::ssz_native::sound;
    std::wcout << L"\n--- Sound service ---" << std::endl;

    // Default construction — client created
    s::AudioClient ac;
    TEST(L"AudioClient default constructed", true);

    // Move semantics
    s::AudioClient ac2;
    s::AudioClient ac3 = std::move(ac2);
    TEST(L"AudioClient move: no crash", true);

    s::AudioClient ac4;
    ac4 = std::move(ac3);
    TEST(L"AudioClient move-assign: no crash", true);

    // Start/stop on default-constructed client (may fail at runtime but must not crash)
    (void)ac.start();
    (void)ac.stop();
    (void)ac.buffer_ready();
    TEST(L"AudioClient start/stop/buffer_ready no crash", true);
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
    // (can't open a real socket in unit test, but verify moves work)
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
    test_string_service();
    test_ogg_service();
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
