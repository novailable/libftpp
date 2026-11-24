#include "message.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <iostream>
#include <limits>
#include <list>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

using namespace std::chrono_literals;

// ---------------------------------------------------------------- mini harness

static std::atomic<int> g_checks{0};
static std::atomic<int> g_failures{0};
static std::atomic<const char*> g_currentTest{"(none)"};
static std::atomic<long long> g_lastProgressMs{0};

static long long nowMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

#define CHECK(cond)                                                              \
    do {                                                                         \
        ++g_checks;                                                              \
        if (!(cond)) {                                                           \
            ++g_failures;                                                        \
            std::cerr << "  FAIL " << __FILE__ << ":" << __LINE__ << "  "       \
                      << #cond << "\n";                                          \
        }                                                                        \
    } while (0)

#define RUN(fn)                                  \
    do {                                         \
        std::cout << "[ RUN ] " #fn "\n";        \
        g_currentTest = #fn;                     \
        g_lastProgressMs = nowMs();              \
        fn();                                    \
    } while (0)

// Kills the process if a test stops making progress (infinite decode loop...).
static void startWatchdog(std::chrono::seconds limit)
{
    std::thread([limit] {
        for (;;)
        {
            std::this_thread::sleep_for(200ms);

            if (nowMs() - g_lastProgressMs.load() > limit.count() * 1000)
            {
                std::fprintf(stderr,
                    "\n  WATCHDOG: %s made no progress for %lld s\n",
                    g_currentTest.load(),
                    static_cast<long long>(limit.count()));
                std::_Exit(2);
            }
        }
    }).detach();
}

// ------------------------------------------------------------------- helpers

static constexpr std::uint32_t kMaxFrame = Message::maxSize;

// True only if f() throws exactly E (or a subclass). Any other exception,
// including std::bad_alloc / std::length_error, counts as "no".
template <class E, class F>
static bool throwsAs(F&& f)
{
    try { f(); }
    catch (const E&) { return true; }
    catch (...) { return false; }
    return false;
}

static void put32(std::vector<char>& v, std::uint32_t x)
{
    v.push_back(static_cast<char>(x >> 24));
    v.push_back(static_cast<char>(x >> 16));
    v.push_back(static_cast<char>(x >> 8));
    v.push_back(static_cast<char>(x));
}

static std::vector<char> header(std::uint32_t len, std::uint32_t type)
{
    std::vector<char> v;
    put32(v, len);
    put32(v, type);
    return v;
}

template <class T>
static std::vector<char> bytesOf(const T& v)
{
    std::vector<char> out(sizeof(T));
    std::memcpy(out.data(), &v, sizeof(T));
    return out;
}

static void append(std::vector<char>& a, const std::vector<char>& b)
{
    a.insert(a.end(), b.begin(), b.end());
}

static std::vector<char> bytesOf(const std::string& s)
{
    return std::vector<char>(s.begin(), s.end());
}

static std::vector<char> frameOf(std::uint32_t type, const std::vector<char>& payload)
{
    std::vector<char> f = header(static_cast<std::uint32_t>(4 + payload.size()), type);
    append(f, payload);
    return f;
}

static std::vector<char> payloadOf(const Message& m)
{
    const std::vector<char> e = m.encode();
    return std::vector<char>(e.begin() + 8, e.end());
}

// Builds a Message carrying exactly these payload bytes.
static Message fromPayload(Message::Type type, const std::vector<char>& payload)
{
    std::vector<char> f = frameOf(static_cast<std::uint32_t>(type), payload);
    std::optional<Message> m = Message::decode(f);
    if (!m)
    {
        std::fprintf(stderr, "  FATAL fromPayload: a well-formed frame did not decode (%s)\n",
                     g_currentTest.load());
        std::_Exit(3);
    }
    return *m;
}

// Pushes a message through the real wire format and back.
static Message viaWire(const Message& m)
{
    std::vector<char> buf = m.encode();
    std::optional<Message> out = Message::decode(buf);
    CHECK(out.has_value());
    CHECK(buf.empty());
    if (!out)
    {
        std::fprintf(stderr, "  FATAL viaWire: encode() output did not decode (%s)\n",
                     g_currentTest.load());
        std::_Exit(3);
    }
    return *out;
}

// True if nothing is left to read.
static bool exhausted(const Message& m)
{
    char c;
    return throwsAs<std::out_of_range>([&] { m >> c; });
}

struct Pod
{
    std::int32_t a;
    double       b;
    char         c;

    bool operator==(const Pod& o) const
    {
        return a == o.a && std::memcmp(&b, &o.b, sizeof(b)) == 0 && c == o.c;
    }
};

enum class Color : std::uint8_t { Red = 1, Blue = 7 };
enum Plain { Low = -5, High = 100000 };

// Equality that is bit-exact for floating point (so NaN and -0.0 are testable).
template <class T>
static bool same(const T& a, const T& b)
{
    if constexpr (std::is_floating_point<T>::value)
        return std::memcmp(&a, &b, sizeof(T)) == 0;
    else
        return a == b;
}

template <class T>
static bool roundTrip(const T& v)
{
    Message m(1);
    m << v;
    const Message r = viaWire(m);

    T got{};
    r >> got;
    return same(got, v) && exhausted(r);
}

// ------------------------------------------------------- random message script
//
// The same seed generates the same sequence of values, so one run can write
// them (w != nullptr) and a second run can read them back and compare
// (r != nullptr). Returns the number of mismatches when reading.

static int runScript(std::uint32_t seed, int nOps, std::size_t maxLen,
                     Message* w, const Message* r)
{
    std::mt19937 rng(seed);
    int bad = 0;

    auto step = [&](const auto& v) {
        using T = std::decay_t<decltype(v)>;
        if (w)
            *w << v;
        else
        {
            T g{};
            *r >> g;
            if (!same(g, v))
                ++bad;
        }
    };
    auto u32 = [&] { return static_cast<std::uint32_t>(rng()); };
    auto u64 = [&] {
        const std::uint64_t hi = u32();
        const std::uint64_t lo = u32();
        return (hi << 32) | lo;
    };
    auto str = [&] {
        std::string s;
        const std::size_t n = u32() % (maxLen + 1);
        for (std::size_t i = 0; i < n; ++i)
            s.push_back(static_cast<char>(u32()));
        return s;
    };

    for (int i = 0; i < nOps; ++i)
    {
        switch (u32() % 10)
        {
            case 0: step(static_cast<std::int32_t>(u32())); break;
            case 1: step(static_cast<std::int64_t>(u64())); break;
            case 2: { const std::uint64_t bits = u64(); double d; std::memcpy(&d, &bits, 8); step(d); break; }
            case 3: step(static_cast<char>(u32())); break;
            case 4: step(str()); break;
            case 5: {
                std::vector<std::int32_t> v;
                const std::size_t n = u32() % (maxLen + 1);
                for (std::size_t k = 0; k < n; ++k) v.push_back(static_cast<std::int32_t>(u32()));
                step(v);
                break;
            }
            case 6: {
                std::vector<std::string> v;
                const std::size_t n = u32() % 5;
                for (std::size_t k = 0; k < n; ++k) v.push_back(str());
                step(v);
                break;
            }
            case 7: {
                std::vector<std::vector<std::uint16_t>> v;
                const std::size_t n = u32() % 4;
                for (std::size_t k = 0; k < n; ++k)
                {
                    std::vector<std::uint16_t> inner;
                    const std::size_t m = u32() % (maxLen + 1);
                    for (std::size_t j = 0; j < m; ++j) inner.push_back(static_cast<std::uint16_t>(u32()));
                    v.push_back(inner);
                }
                step(v);
                break;
            }
            case 8: {
                Pod p{};
                p.a = static_cast<std::int32_t>(u32());
                const std::uint64_t bits = u64();
                std::memcpy(&p.b, &bits, 8);
                p.c = static_cast<char>(u32());
                step(p);
                break;
            }
            default: {
                std::deque<std::int64_t> v;
                const std::size_t n = u32() % (maxLen + 1);
                for (std::size_t k = 0; k < n; ++k) v.push_back(static_cast<std::int64_t>(u64()));
                step(v);
                break;
            }
        }
    }
    return bad;
}

// ------------------------------------------------------------------ type / API

static void test_type_and_construction()
{
    static_assert(std::is_same<Message::Type, int>::value, "Type must be int");
    static_assert(std::is_default_constructible<Message>::value, "");
    static_assert(std::is_copy_constructible<Message>::value, "");
    static_assert(std::is_move_constructible<Message>::value, "");

    CHECK(Message().type() == 0);
    CHECK(Message(1).type() == 1);

    // Type survives the wire, including negatives and the extremes.
    const int types[] = {0, 1, -1, 42, -42, INT_MAX, INT_MIN, 0x01020304, -2};
    bool ok = true;
    for (int t : types)
        ok = ok && viaWire(Message(t)).type() == t;
    CHECK(ok);

    // Message with no payload at all.
    Message empty(9);
    CHECK(empty.encode().size() == 8);
    const Message r = viaWire(empty);
    CHECK(r.type() == 9);
    CHECK(exhausted(r));
}

static void test_wire_format_exact_bytes()
{
    {
        Message m(0x01020304);
        const std::int32_t  a = 0x0A0B0C0D;
        const std::uint8_t  b = 0xFF;
        const std::uint16_t c = 0x1234;
        m << a << b << c;

        std::vector<char> expected;
        put32(expected, 4 + 4 + 1 + 2);   // frame length = type + payload
        put32(expected, 0x01020304);      // type, big endian
        append(expected, bytesOf(a));
        append(expected, bytesOf(b));
        append(expected, bytesOf(c));

        CHECK(m.encode() == expected);

        const std::vector<char> e = m.encode();
        CHECK(e[0] == 0 && e[1] == 0 && e[2] == 0 && e[3] == 11);
        CHECK(e[4] == 1 && e[5] == 2 && e[6] == 3 && e[7] == 4);
    }

    {   // negative type: two's complement, big endian
        const std::vector<char> e = Message(-2).encode();
        CHECK(e.size() == 8);
        CHECK(e[3] == 4);
        CHECK(static_cast<unsigned char>(e[4]) == 0xFF && static_cast<unsigned char>(e[5]) == 0xFF &&
              static_cast<unsigned char>(e[6]) == 0xFF && static_cast<unsigned char>(e[7]) == 0xFE);
    }

    {   // encode() is const and repeatable
        Message m(5);
        m << 1 << 2;
        CHECK(m.encode() == m.encode());
    }
}

static void test_chaining_and_const_reading()
{
    Message m(7);
    CHECK(&(m << 1) == &m);
    CHECK(&(m << std::string("s") << 2.5 << 'c') == &m);

    const Message& cm = m;
    int i = 0; std::string s; double d = 0; char c = 0;
    CHECK(&(cm >> i) == &cm);
    cm >> s >> d >> c;
    CHECK(i == 1 && s == "s" && d == 2.5 && c == 'c');
    CHECK(exhausted(cm));
}

// ------------------------------------------------------------- scalar round trips

static void test_scalar_roundtrips()
{
    CHECK(roundTrip<std::int8_t>(-128));
    CHECK(roundTrip<std::int8_t>(127));
    CHECK(roundTrip<std::uint8_t>(255));
    CHECK(roundTrip<std::int16_t>(-32768));
    CHECK(roundTrip<std::uint16_t>(65535));
    CHECK(roundTrip<std::int32_t>(INT32_MIN));
    CHECK(roundTrip<std::int32_t>(INT32_MAX));
    CHECK(roundTrip<std::uint32_t>(UINT32_MAX));
    CHECK(roundTrip<std::int64_t>(INT64_MIN));
    CHECK(roundTrip<std::int64_t>(INT64_MAX));
    CHECK(roundTrip<std::uint64_t>(UINT64_MAX));
    CHECK(roundTrip<std::size_t>(std::size_t(-1)));
    CHECK(roundTrip<bool>(true));
    CHECK(roundTrip<bool>(false));
    CHECK(roundTrip<char>('z'));
    CHECK(roundTrip<char>('\0'));
    CHECK(roundTrip<signed char>(-1));
    CHECK(roundTrip<wchar_t>(L'€'));
    CHECK(roundTrip<char16_t>(u'€'));
    CHECK(roundTrip<char32_t>(U'\U0001F600'));

    // Floating point must be bit exact.
    const double dspecial[] = {
        0.0, -0.0, 1.0, -1.0, 3.141592653589793, 1e-320 /*denormal*/,
        std::numeric_limits<double>::min(), std::numeric_limits<double>::max(),
        std::numeric_limits<double>::lowest(), std::numeric_limits<double>::epsilon(),
        std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::signaling_NaN(),
    };
    bool ok = true;
    for (double d : dspecial) ok = ok && roundTrip(d);
    CHECK(ok);

    const float fspecial[] = {
        0.0f, -0.0f, 1.5f, -2.25f, std::numeric_limits<float>::min(), std::numeric_limits<float>::max(),
        std::numeric_limits<float>::denorm_min(), std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::quiet_NaN(),
    };
    ok = true;
    for (float f : fspecial) ok = ok && roundTrip(f);
    CHECK(ok);

    CHECK(roundTrip(Color::Blue));
    CHECK(roundTrip(Color::Red));
    CHECK(roundTrip(Low));
    CHECK(roundTrip(High));
}

static void test_struct_and_array_roundtrips()
{
    Pod p{};
    p.a = -7; p.b = 1.5; p.c = 'q';
    CHECK(roundTrip(p));
    CHECK(roundTrip(std::array<int, 3>{1, 2, 3}));
    CHECK(roundTrip(std::array<Pod, 2>{p, p}));
    CHECK(roundTrip(std::array<std::array<int, 2>, 2>{{{1, 2}, {3, 4}}}));
    CHECK(roundTrip(std::array<double, 2>{0.5, -0.5}));
}

// -------------------------------------------------------------- sequence round trips

static void test_string_roundtrips()
{
    CHECK(roundTrip(std::string()));
    CHECK(roundTrip(std::string("x")));
    CHECK(roundTrip(std::string("hello world")));
    CHECK(roundTrip(std::string("line1\nline2\r\n\ttab")));
    CHECK(roundTrip(std::string("a\0b\0c", 5)));
    CHECK(roundTrip(std::string(1000, '\0')));
    CHECK(roundTrip(std::string("\xff\xfe\x80 not utf8 \xc3")));

    std::string allBytes;
    for (int i = 0; i < 256; ++i)
        allBytes.push_back(static_cast<char>(i));
    CHECK(roundTrip(allBytes));

    CHECK(roundTrip(std::string(8 * 1024 * 1024, 'x')));
    CHECK(roundTrip(std::wstring(L"wide € string")));
    CHECK(roundTrip(std::u16string(u"utf16 €")));
    CHECK(roundTrip(std::u32string(U"utf32 \U0001F600")));
}

static void test_sequence_roundtrips()
{
    CHECK(roundTrip(std::vector<int>{}));
    CHECK(roundTrip(std::vector<int>{1, -2, 3, INT_MAX, INT_MIN}));
    CHECK(roundTrip(std::vector<double>{0.5, -0.0, 1e300}));
    CHECK(roundTrip(std::vector<char>(3 * 1024 * 1024, 'c')));
    CHECK(roundTrip(std::vector<std::uint64_t>(100000, 0xDEADBEEFCAFEBABEull)));
    CHECK(roundTrip(std::vector<bool>{true, false, true, true, false}));
    CHECK(roundTrip(std::vector<bool>{}));

    CHECK(roundTrip(std::vector<std::string>{}));
    CHECK(roundTrip(std::vector<std::string>{"a", "", "ccc", std::string("n\0l", 3)}));
    CHECK(roundTrip(std::vector<std::vector<int>>{{1}, {}, {2, 3}, {}}));
    CHECK(roundTrip(std::vector<std::vector<std::string>>{{"x", "y"}, {}, {"z"}}));
    CHECK(roundTrip(std::vector<std::vector<std::vector<int>>>{{{1}, {}}, {}, {{2, 3}}}));

    CHECK(roundTrip(std::deque<int>{9, 8, 7}));
    CHECK(roundTrip(std::deque<std::string>{"d", "e"}));
    CHECK(roundTrip(std::list<char>{'a', 'b', 'c'}));
    CHECK(roundTrip(std::list<std::string>{"l1", "l2"}));

    Pod p{};
    p.a = 5; p.b = 2.5; p.c = 'x';
    CHECK(roundTrip(std::vector<Pod>{p, p, p}));
    CHECK(roundTrip(std::vector<std::array<int, 2>>{{1, 2}, {3, 4}}));

    // Many small elements through the element-by-element path.
    std::vector<std::string> many;
    for (int i = 0; i < 20000; ++i)
        many.push_back(std::to_string(i));
    CHECK(roundTrip(many));
}

static void test_sequence_wire_layout()
{
    // std::string = uint64 length, then raw bytes
    {
        Message m(1);
        m << std::string("hi");
        std::vector<char> expected = bytesOf(std::uint64_t(2));
        append(expected, bytesOf(std::string("hi")));
        CHECK(payloadOf(m) == expected);
    }

    // empty string / empty vector = just a zero length
    {
        Message a(1), b(1);
        a << std::string();
        b << std::vector<int>{};
        CHECK(payloadOf(a) == bytesOf(std::uint64_t(0)));
        CHECK(payloadOf(b) == bytesOf(std::uint64_t(0)));
    }

    // vector<int> = uint64 count, then the raw elements
    {
        Message m(1);
        m << std::vector<std::int32_t>{1, 2, 3};
        std::vector<char> expected = bytesOf(std::uint64_t(3));
        for (std::int32_t v : {1, 2, 3}) append(expected, bytesOf(v));
        CHECK(payloadOf(m) == expected);
    }

    // vector<string> = count, then each string (length + bytes)
    {
        Message m(1);
        m << std::vector<std::string>{"a", "bc"};
        std::vector<char> expected = bytesOf(std::uint64_t(2));
        append(expected, bytesOf(std::uint64_t(1)));  append(expected, bytesOf(std::string("a")));
        append(expected, bytesOf(std::uint64_t(2)));  append(expected, bytesOf(std::string("bc")));
        CHECK(payloadOf(m) == expected);
    }

    // The hand-written "size_t length, then one char at a time" style is
    // byte-for-byte the same as operator<< on a std::string.
    if constexpr (sizeof(std::size_t) == 8)
    {
        const std::string text = "manual";

        Message manual(2);
        manual << text.size();
        for (char c : text) manual << c;

        Message tmpl(2);
        tmpl << text;
        CHECK(payloadOf(manual) == payloadOf(tmpl));

        // read the manual one with the template...
        std::string got;
        manual >> got;
        CHECK(got == text);

        // ...and the template one by hand
        std::size_t len = 0;
        tmpl >> len;
        std::string byHand;
        for (std::size_t i = 0; i < len; ++i) { char c; tmpl >> c; byHand.push_back(c); }
        CHECK(len == text.size() && byHand == text);
    }
}

static void test_mixed_sequence_order()
{
    Message m(4);
    m << 42 << std::string("mid") << 3.5 << std::vector<int>{1, 2} << 'z' << Color::Blue;

    const Message r = viaWire(m);
    int i = 0; std::string s; double d = 0; std::vector<int> v; char c = 0; Color col = Color::Red;
    r >> i >> s >> d >> v >> c >> col;

    CHECK(i == 42 && s == "mid" && d == 3.5 && (v == std::vector<int>{1, 2}) && c == 'z' && col == Color::Blue);
    CHECK(exhausted(r));
}

static void test_reading_into_prefilled_targets_replaces()
{
    std::string s = "old content that is much longer than the new one";
    std::vector<int> v{9, 9, 9, 9, 9};
    std::vector<std::string> vs{"x", "y", "z"};
    std::deque<int> dq{1, 2, 3};
    std::list<char> ls{'a', 'b'};

    Message m(1);
    m << std::string("new") << std::vector<int>{1} << std::vector<std::string>{}
      << std::deque<int>{7} << std::list<char>{};

    m >> s >> v >> vs >> dq >> ls;

    CHECK(s == "new");
    CHECK((v == std::vector<int>{1}));
    CHECK(vs.empty());
    CHECK((dq == std::deque<int>{7}));
    CHECK(ls.empty());
}

// ------------------------------------------------------------------- read errors

static void test_read_past_end_throws_out_of_range()
{
    {   // empty message: every kind of read throws
        const Message m(1);
        int i; double d; char c; std::string s; std::vector<int> v; std::vector<std::string> vs;
        CHECK(throwsAs<std::out_of_range>([&] { m >> i; }));
        CHECK(throwsAs<std::out_of_range>([&] { m >> d; }));
        CHECK(throwsAs<std::out_of_range>([&] { m >> c; }));
        CHECK(throwsAs<std::out_of_range>([&] { m >> s; }));
        CHECK(throwsAs<std::out_of_range>([&] { m >> v; }));
        CHECK(throwsAs<std::out_of_range>([&] { m >> vs; }));
    }

    {   // exact fit succeeds, one more byte fails
        Message m(1);
        m << std::int32_t(5);
        std::int32_t x = 0;
        CHECK(!throwsAs<std::out_of_range>([&] { m >> x; }));
        CHECK(x == 5);
        CHECK(exhausted(m));
    }

    {   // one byte short of a double
        const Message m = fromPayload(1, std::vector<char>(7, 'a'));
        double d;
        CHECK(throwsAs<std::out_of_range>([&] { m >> d; }));
    }

    {   // a length prefix cut in half
        const Message m = fromPayload(1, std::vector<char>(5, 0));
        std::string s;
        CHECK(throwsAs<std::out_of_range>([&] { m >> s; }));
    }
}

static void test_failed_read_restores_position()
{
    {   // scalar: 3 bytes cannot be an int, but are still there afterwards
        const Message m = fromPayload(1, std::vector<char>{'a', 'b', 'c'});
        int i;
        CHECK(throwsAs<std::out_of_range>([&] { m >> i; }));
        char a, b, c;
        m >> a >> b >> c;
        CHECK(a == 'a' && b == 'b' && c == 'c');
    }

    {   // string whose length is fine but data is short
        Message m(1);
        m << std::uint64_t(10) << 'a' << 'b' << 'c';
        std::string s = "untouched?";
        CHECK(throwsAs<std::out_of_range>([&] { m >> s; }));

        std::uint64_t len = 0; char a, b, c;
        m >> len >> a >> b >> c;
        CHECK(len == 10 && a == 'a' && b == 'b' && c == 'c');
    }

    {   // vector<string> that claims 3 elements but holds 2
        Message m(1);
        m << std::uint64_t(3) << std::string("a") << std::string("b");
        std::vector<std::string> v;
        CHECK(throwsAs<std::out_of_range>([&] { m >> v; }));

        std::uint64_t n = 0; std::string a, b;
        m >> n >> a >> b;
        CHECK(n == 3 && a == "a" && b == "b");
        CHECK(exhausted(m));
    }

    {   // nested: the inner vector is the broken one
        Message m(1);
        m << std::uint64_t(2) << std::vector<int>{1, 2} << std::uint64_t(1000);
        std::vector<std::vector<int>> v;
        CHECK(throwsAs<std::out_of_range>([&] { m >> v; }));

        std::uint64_t n = 0; std::vector<int> first;
        m >> n >> first;
        CHECK(n == 2 && (first == std::vector<int>{1, 2}));
    }

    {   // after a failed read, a correct read of the same data still works
        Message m(1);
        m << std::string("ok");
        std::vector<std::string> wrong;              // wrong type: "ok" as vector<string>
        CHECK(throwsAs<std::out_of_range>([&] { m >> wrong; }));
        std::string right;
        m >> right;
        CHECK(right == "ok");
    }
}

static void test_hostile_length_prefixes()
{
    // A length prefix must never be trusted: it must give out_of_range, never
    // bad_alloc / length_error / a crash, and must leave the message unread.
    const std::uint64_t counts[] = {
        17, 1000, 0xFFFFFFFFull, 1ull << 32, 1ull << 40, 1ull << 62, 1ull << 63, ~0ull,
    };

    bool ok = true;
    for (std::uint64_t count : counts)
    {
        Message m(1);
        m << count << std::uint64_t(0) << std::uint64_t(0);   // 16 bytes of filler

        auto check = [&](auto tag) {
            using T = decltype(tag);
            T v;
            const bool threw = throwsAs<std::out_of_range>([&] { m >> v; });
            std::uint64_t again = 0;
            m >> again;                                       // position was restored
            m = fromPayload(1, payloadOf(m));                 // rewind for the next type
            return threw && again == count;
        };

        ok = ok && check(std::string{});
        ok = ok && check(std::vector<int>{});
        ok = ok && check(std::vector<char>{});
        ok = ok && check(std::vector<std::string>{});
        ok = ok && check(std::vector<std::vector<int>>{});
        ok = ok && check(std::deque<int>{});
        ok = ok && check(std::list<char>{});
        ok = ok && check(std::wstring{});
    }
    CHECK(ok);

    {   // count * sizeof(element) would overflow a 64 bit multiplication
        Message m(1);
        m << (~0ull / 4 + 2) << std::uint64_t(0);
        std::vector<std::int32_t> v;
        CHECK(throwsAs<std::out_of_range>([&] { m >> v; }));
    }

    {   // 5 ints do not fit in 16 bytes although 5 <= 16
        Message m(1);
        m << std::uint64_t(5) << std::uint64_t(0) << std::uint64_t(0);
        std::vector<std::int32_t> v;
        CHECK(throwsAs<std::out_of_range>([&] { m >> v; }));
    }

    {   // inner prefix hostile inside an otherwise fine outer vector
        Message m(1);
        m << std::uint64_t(2) << std::uint64_t(1) << std::int32_t(7) << (1ull << 62);
        std::vector<std::vector<int>> v;
        CHECK(throwsAs<std::out_of_range>([&] { m >> v; }));
    }
}

static void test_failed_read_leaves_target_untouched()
{
    // A read that fails must not clobber the variable it was reading into.
    {   // bulk path: 5 ints claimed, only 16 bytes present
        Message m(1);
        m << std::uint64_t(5) << std::uint64_t(0) << std::uint64_t(0);
        std::vector<std::int32_t> v{1, 2, 3};
        CHECK(throwsAs<std::out_of_range>([&] { m >> v; }));
        CHECK((v == std::vector<std::int32_t>{1, 2, 3}));
    }

    {   // element-by-element path: claims 3 strings, holds 2
        Message m(1);
        m << std::uint64_t(3) << std::string("a") << std::string("b");
        std::vector<std::string> v{"keep", "me"};
        CHECK(throwsAs<std::out_of_range>([&] { m >> v; }));
        CHECK((v == std::vector<std::string>{"keep", "me"}));
    }

    {   // string whose data is short
        Message m(1);
        m << std::uint64_t(10) << 'a';
        std::string s = "keep";
        CHECK(throwsAs<std::out_of_range>([&] { m >> s; }));
        CHECK(s == "keep");
    }
}

// --------------------------------------------------------------- copy and move

static void test_copy_and_move()
{
    Message a(5);
    a << 1 << std::string("x");
    const std::vector<char> original = a.encode();

    Message b = a;                       // copy
    CHECK(b.type() == 5);
    CHECK(b.encode() == original);

    b << 2;                              // writing to the copy...
    CHECK(a.encode() == original);       // ...does not touch the original
    CHECK(b.encode() != original);

    int i = 0; std::string s;            // reading the original does not disturb the copy
    a >> i >> s;
    Message c = a;
    CHECK(c.encode() == original);

    Message d(9);
    d = b;                               // copy assignment
    CHECK(d.type() == 5 && d.encode() == b.encode());

    const std::vector<char> beforeMove = b.encode();
    Message e = std::move(b);            // move
    CHECK(e.type() == 5 && e.encode() == beforeMove);
}

// ------------------------------------------------------------- frame decoding

static void test_decode_incomplete_frames_wait()
{
    bool ok = true;
    for (std::uint32_t seed = 0; seed < 150; ++seed)
    {
        Message m(static_cast<int>(seed) - 50);
        runScript(seed, 1 + static_cast<int>(seed % 6), 20, &m, nullptr);
        const std::vector<char> bytes = m.encode();

        std::vector<char> buf;
        std::optional<Message> got;
        for (std::size_t i = 0; i < bytes.size(); ++i)
        {
            buf.push_back(bytes[i]);
            const std::vector<char> before = buf;
            std::optional<Message> o = Message::decode(buf);

            if (i + 1 < bytes.size())
                ok = ok && !o.has_value() && buf == before;   // incomplete: nothing consumed
            else
                got = std::move(o);
        }

        ok = ok && got.has_value() && buf.empty() && got->encode() == bytes
                && got->type() == m.type();
    }
    CHECK(ok);

    std::vector<char> empty;
    CHECK(!Message::decode(empty).has_value());
}

static void test_decode_multiple_frames_and_trailing()
{
    std::vector<Message> sent;
    std::vector<char> stream;
    for (std::uint32_t seed = 0; seed < 100; ++seed)
    {
        Message m(static_cast<int>(seed));
        runScript(seed, 1 + static_cast<int>(seed % 5), 15, &m, nullptr);
        append(stream, m.encode());
        sent.push_back(m);
    }

    // half of one more frame at the end
    Message tail(999);
    tail << std::string("this frame is never completed");
    const std::vector<char> tailBytes = tail.encode();
    const std::size_t half = tailBytes.size() / 2;
    stream.insert(stream.end(), tailBytes.begin(), tailBytes.begin() + static_cast<std::ptrdiff_t>(half));

    bool ok = true;
    for (const Message& m : sent)
    {
        std::optional<Message> o = Message::decode(stream);
        ok = ok && o.has_value() && o->type() == m.type() && o->encode() == m.encode();
    }
    CHECK(ok);

    CHECK(!Message::decode(stream).has_value());
    CHECK(stream.size() == half);                         // the partial frame is kept

    stream.insert(stream.end(), tailBytes.begin() + static_cast<std::ptrdiff_t>(half), tailBytes.end());
    std::optional<Message> last = Message::decode(stream);
    CHECK(last.has_value() && last->type() == 999);
    CHECK(stream.empty());
}

static void test_decode_rejects_malformed_lengths()
{
    // Valid frame has len >= 4 (the type field).
    for (std::uint32_t len : {0u, 1u, 2u, 3u})
    {
        std::vector<char> buf = header(len, 7);
        append(buf, std::vector<char>(16, 'x'));
        const std::vector<char> before = buf;
        CHECK(throwsAs<std::runtime_error>([&] { Message::decode(buf); }));
        CHECK(buf == before);                              // nothing consumed on error
    }

    {   // len = 4 is the smallest valid frame: type only, empty payload
        std::vector<char> buf = header(4, 77);
        std::optional<Message> o = Message::decode(buf);
        CHECK(o.has_value() && o->type() == 77 && exhausted(*o));
        CHECK(buf.empty());
    }

    {   // only 4 bytes of a bad header: the length alone is enough to reject
        std::vector<char> buf;
        put32(buf, 2);
        CHECK(throwsAs<std::runtime_error>([&] { Message::decode(buf); }));
    }

    {   // absurdly large claimed length
        for (std::uint32_t len : {kMaxFrame + 1, 0x7FFFFFFFu, 0x80000000u, 0xFFFFFFFFu})
        {
            std::vector<char> buf = header(len, 1);
            const std::vector<char> before = buf;
            CHECK(throwsAs<std::runtime_error>([&] { Message::decode(buf); }));
            CHECK(buf == before);
        }
    }

    {   // a bad frame after good ones: the good ones are still delivered first
        Message good(1);
        good << 1;
        std::vector<char> buf = good.encode();
        append(buf, good.encode());
        append(buf, header(1, 0));                          // malformed
        CHECK(Message::decode(buf).has_value());
        CHECK(Message::decode(buf).has_value());
        CHECK(throwsAs<std::runtime_error>([&] { Message::decode(buf); }));
    }
}

static void test_decode_frame_limit_boundaries()
{
    {   // exactly the limit, header only: just "not complete yet", and no allocation
        std::vector<char> buf = header(kMaxFrame, 1);
        const std::vector<char> before = buf;
        std::optional<Message> o;
        CHECK(!throwsAs<std::exception>([&] { o = Message::decode(buf); }));
        CHECK(!o.has_value());
        CHECK(buf == before);
    }

    {   // a complete frame of exactly the maximum size decodes
        Message m(3);
        m << std::vector<char>(kMaxFrame - 4 - 8, 'm');     // payload = 8 (count) + data
        std::vector<char> buf = m.encode();
        CHECK(buf.size() == std::size_t(kMaxFrame) + 4);
        std::optional<Message> o = Message::decode(buf);
        CHECK(o.has_value() && o->type() == 3);
        CHECK(buf.empty());
    }

    {   // encode() does not enforce the limit, so a sender can build a frame the
        // receiver must refuse (this is what a Server does with such a client).
        Message m(3);
        m << std::vector<char>(std::size_t(kMaxFrame) + 1, 'o');
        std::vector<char> buf = m.encode();
        CHECK(throwsAs<std::runtime_error>([&] { Message::decode(buf); }));
    }
}

static void test_truncated_payload_never_crashes()
{
    // Cut a valid payload at every possible length. Reading the full structure
    // must throw out_of_range every time (never crash, never any other exception).
    bool ok = true;
    int cuts = 0;

    for (std::uint32_t seed = 0; seed < 300; ++seed)
    {
        const int nOps = 1 + static_cast<int>(seed % 6);

        Message m(3);
        runScript(seed, nOps, 12, &m, nullptr);
        const std::vector<char> payload = payloadOf(m);

        {   // sanity: the complete payload reads back without mismatches
            const Message full = fromPayload(3, payload);
            ok = ok && runScript(seed, nOps, 12, nullptr, &full) == 0 && exhausted(full);
        }

        for (std::size_t k = 0; k < payload.size(); ++k)
        {
            const Message t = fromPayload(3, std::vector<char>(payload.begin(), payload.begin() + static_cast<std::ptrdiff_t>(k)));
            ok = ok && throwsAs<std::out_of_range>([&] { runScript(seed, nOps, 12, nullptr, &t); });
            ++cuts;
        }
    }
    CHECK(ok);
    CHECK(cuts > 1000);
}

// ------------------------------------------------------------------------ fuzz

static void test_fuzz_roundtrip()
{
    int mismatches = 0;
    int notExhausted = 0;

    for (std::uint32_t seed = 0; seed < 4000; ++seed)
    {
        const int nOps = 1 + static_cast<int>(seed % 25);
        const std::size_t maxLen = (seed % 7 == 0) ? 600 : 20;

        Message m(static_cast<int>(seed * 2654435761u));
        runScript(seed, nOps, maxLen, &m, nullptr);

        const Message r = viaWire(m);
        mismatches += runScript(seed, nOps, maxLen, nullptr, &r);
        notExhausted += exhausted(r) ? 0 : 1;

        if (r.type() != m.type())
            ++mismatches;
    }

    CHECK(mismatches == 0);
    CHECK(notExhausted == 0);
}

static void test_fuzz_garbage_frames()
{
    // Random bytes into decode: either nothing, a message, or runtime_error.
    // Whatever comes out must be safely readable (out_of_range at worst).
    std::mt19937 rng(0xC0FFEE);
    int unexpected = 0;
    int decoded = 0;

    for (int iter = 0; iter < 60000; ++iter)
    {
        std::vector<char> buf(rng() % 48);
        for (char& c : buf)
            c = static_cast<char>(rng());

        if (iter % 2 == 0 && buf.size() >= 4)   // steer half of them towards valid-looking lengths
        {
            buf[0] = 0; buf[1] = 0; buf[2] = 0;
            buf[3] = static_cast<char>(4 + rng() % 40);
        }

        try
        {
            for (int guard = 0; guard < 64; ++guard)
            {
                std::optional<Message> o = Message::decode(buf);
                if (!o)
                    break;
                ++decoded;

                try
                {
                    int x; double d; std::string s; std::vector<int> v; std::vector<std::string> vs;
                    *o >> x >> d >> s >> v >> vs;
                }
                catch (const std::out_of_range&) {}
            }
        }
        catch (const std::runtime_error&) {}
        catch (...) { ++unexpected; }
    }

    CHECK(unexpected == 0);
    CHECK(decoded > 1000);                      // the fuzz really reached the decoder
}

static void test_fuzz_bitflipped_valid_frames()
{
    // Take real frames, flip bits anywhere (header, counts, data). Decoding and
    // reading must stay inside the documented exceptions.
    std::mt19937 rng(0xBADF00D);
    int unexpected = 0;
    int reachedReader = 0;

    for (std::uint32_t iter = 0; iter < 8000; ++iter)
    {
        const int nOps = 1 + static_cast<int>(iter % 8);

        Message m(1);
        runScript(iter, nOps, 20, &m, nullptr);
        std::vector<char> bytes = m.encode();

        const int flips = 1 + static_cast<int>(rng() % 3);
        for (int f = 0; f < flips; ++f)
            bytes[rng() % bytes.size()] ^= static_cast<char>(1u << (rng() % 8));

        try
        {
            std::optional<Message> o = Message::decode(bytes);
            if (o)
            {
                ++reachedReader;
                try { runScript(iter, nOps, 20, nullptr, &*o); }
                catch (const std::out_of_range&) {}
            }
        }
        catch (const std::runtime_error&) {}
        catch (...) { ++unexpected; }
    }

    CHECK(unexpected == 0);
    CHECK(reachedReader > 1000);
}

// ---------------------------------------------------- things that must not compile
//
// Each block must FAIL to compile. Run for example:
//   g++ -std=c++17 -fsyntax-only -DCOMPILE_FAIL_LITERAL message_test.cpp    (must error)
// The normal build (no -D) must compile and is what runs the tests.

#if defined(COMPILE_FAIL_LITERAL)
static void compileFail() { Message m(1); m << "text"; }                  // array of char
#elif defined(COMPILE_FAIL_POINTER)
static void compileFail() { Message m(1); const char* p = "x"; m << p; }  // pointer value is meaningless
#elif defined(COMPILE_FAIL_NONTRIVIAL)
struct HasString { std::string s; };
static void compileFail() { Message m(1); HasString h; m << h; }          // neither trivial nor a sequence
#elif defined(COMPILE_FAIL_READ_POINTER)
static void compileFail() { Message m(1); int* p = nullptr; m >> p; }
#endif

// ------------------------------------------------------------------------- main

int main()
{
    startWatchdog(120s);

    RUN(test_type_and_construction);
    RUN(test_wire_format_exact_bytes);
    RUN(test_chaining_and_const_reading);

    RUN(test_scalar_roundtrips);
    RUN(test_struct_and_array_roundtrips);
    RUN(test_string_roundtrips);
    RUN(test_sequence_roundtrips);
    RUN(test_sequence_wire_layout);
    RUN(test_mixed_sequence_order);
    RUN(test_reading_into_prefilled_targets_replaces);

    RUN(test_read_past_end_throws_out_of_range);
    RUN(test_failed_read_restores_position);
    RUN(test_hostile_length_prefixes);
    RUN(test_failed_read_leaves_target_untouched);
    RUN(test_copy_and_move);

    RUN(test_decode_incomplete_frames_wait);
    RUN(test_decode_multiple_frames_and_trailing);
    RUN(test_decode_rejects_malformed_lengths);
    RUN(test_decode_frame_limit_boundaries);
    RUN(test_truncated_payload_never_crashes);

    RUN(test_fuzz_roundtrip);
    RUN(test_fuzz_garbage_frames);
    RUN(test_fuzz_bitflipped_valid_frames);

    std::cout << "\n"
              << g_checks << " checks, "
              << g_failures << " failed\n";

    return g_failures == 0 ? 0 : 1;
}
