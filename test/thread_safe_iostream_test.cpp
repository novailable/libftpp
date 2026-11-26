#include "thread_safe_iostream.hpp"

#include <algorithm>
#include <atomic>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

// ---------------------------------------------------------------- mini harness
static std::atomic<int> g_checks{0};
static std::atomic<int> g_failures{0};

#define CHECK(cond)                                                              \
	do {                                                                         \
		++g_checks;                                                              \
		if (!(cond)) {                                                           \
			++g_failures;                                                        \
			std::cerr << "  FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond  \
					  << "\n";                                                   \
		}                                                                        \
	} while (0)

#define RUN(fn)                                  \
	do {                                         \
		std::cout << "[ RUN ] " #fn "\n";       \
		fn();                                    \
	} while (0)

// ------------------------------------------------------------------- helpers
static std::vector<std::string> splitLines(const std::string& text)
{
	std::vector<std::string> lines;
	std::istringstream iss(text);
	std::string l;
	while (std::getline(iss, l))
		lines.push_back(l);
	return lines;
}

static std::string payload(int id) { return std::string(80, static_cast<char>('a' + id)); }

static std::string expectedLine(int id, int k)
{
	return "[T" + std::to_string(id) + "] msg " + std::to_string(k) + " " + payload(id);
}

// Runs fn(thread_index) on n threads released at the same instant.
template <typename F>
static void runThreads(int n, F fn)
{
	std::atomic<bool> go{false};
	std::vector<std::thread> ts;
	for (int i = 0; i < n; ++i)
		ts.emplace_back([&, i] {
			while (!go)
				std::this_thread::yield();
			fn(i);
		});
	go = true;
	for (auto& t : ts)
		t.join();
}

// prompt() throws; this turns it into a bool for the tests that only care about success.
template <typename T>
static bool ask(ThreadSafeIOStream& tio, const std::string& question, T& dest)
{
	try
	{
		tio.prompt(question, dest);
		return true;
	}
	catch (const std::invalid_argument&) { return false; }
	catch (const std::ios_base::failure&) { return false; }
}

// Writes `count` lines, each built from several << pieces.
static void writerWork(ThreadSafeIOStream& tio, int id, int count)
{
	tio.setPrefix("[T" + std::to_string(id) + "] ");
	for (int k = 0; k < count; ++k)
		tio << "msg " << k << ' ' << payload(id) << '\n';
}

// Every line must be intact, carry exactly one prefix, and each thread's
// lines must appear in the order that thread wrote them.
static void verifyThreadLines(const std::vector<std::string>& lines, int nThreads, int perThread)
{
	CHECK(lines.size() == static_cast<size_t>(nThreads * perThread));
	std::vector<int> next(nThreads, 0);
	for (const std::string& l : lines)
	{
		bool ok = l.size() > 3 && l[0] == '[' && l[1] == 'T' && l[2] >= '0' && l[2] < '0' + nThreads;
		CHECK(ok);
		if (!ok)
			continue;
		int id = l[2] - '0';
		int k = next[id]++;
		CHECK(l == expectedLine(id, k));
	}
	for (int id = 0; id < nThreads; ++id)
		CHECK(next[id] == perThread);
}

// --------------------------------------------------------- output: single thread
static void test_prefix_and_basic_output()
{
	std::istringstream in;
	std::ostringstream out;
	ThreadSafeIOStream tio(in, out);

	tio << "no prefix yet\n";
	CHECK(out.str() == "no prefix yet\n");

	out.str("");
	tio.setPrefix("[A] ");
	tio << "hello " << 42 << ' ' << 3.5 << '\n';
	CHECK(out.str() == "[A] hello 42 3.5\n");
}

static void test_prefix_once_per_statement()
{
	std::istringstream in;
	std::ostringstream out;
	ThreadSafeIOStream tio(in, out);
	tio.setPrefix("[A] ");

	tio << "a\nb\nc\n";                // one statement -> one prefix
	CHECK(out.str() == "[A] a\nb\nc\n");
}

static void test_each_statement_is_emitted_immediately()
{
	std::istringstream in;
	std::ostringstream out;
	ThreadSafeIOStream tio(in, out);
	tio.setPrefix("[A] ");

	tio << "abc";                      // no newline needed, the statement ends at ';'
	CHECK(out.str() == "[A] abc");
	tio << "def\n";
	CHECK(out.str() == "[A] abc[A] def\n");
}

static void test_endl()
{
	std::istringstream in;
	std::ostringstream out;
	ThreadSafeIOStream tio(in, out);
	tio.setPrefix("[A] ");
	tio << "x" << std::endl;
	CHECK(out.str() == "[A] x\n");
}

static void test_formatting_manipulators()
{
	std::istringstream in;
	std::ostringstream out;
	ThreadSafeIOStream tio(in, out);
	tio.setPrefix("[F] ");

	tio << std::hex << 255 << ' ' << std::dec << 255 << '\n';
	tio << std::setw(5) << std::setfill('0') << 42 << std::setfill(' ') << '\n';
	tio << std::boolalpha << true << ' ' << false << '\n';
	tio << std::fixed << std::setprecision(2) << 3.14159 << '\n';

	CHECK(out.str() == "[F] ff 255\n[F] 00042\n[F] true false\n[F] 3.14\n");
}

static void test_format_persists_across_statements()
{
	std::istringstream in;
	std::ostringstream out;
	ThreadSafeIOStream tio(in, out);

	tio << std::hex;                   // statement 1: prints nothing
	CHECK(out.str().empty());
	tio << 255 << std::endl;           // statement 2: still hex
	tio << 255 << '\n';
	tio << std::dec << 255 << '\n';    // back to decimal, and it sticks
	tio << 255 << '\n';
	CHECK(out.str() == "ff\nff\n255\n255\n");
}

static void test_format_does_not_leak_between_wrappers()
{
	std::istringstream in;
	std::ostringstream out;
	ThreadSafeIOStream a(in, out);
	ThreadSafeIOStream b(in, out);

	a << std::hex;
	a << 255 << '\n';
	b << 255 << '\n';
	CHECK(out.str() == "ff\n255\n");
}

static void test_set_out_stream_switches_target()
{
	std::istringstream in;
	std::ostringstream a, b;
	ThreadSafeIOStream tio(in, a);

	tio << "to a\n";
	tio.setOutStream(b);
	tio << "to b\n";
	CHECK(a.str() == "to a\n");
	CHECK(b.str() == "to b\n");
}

static void test_invalid_streams_are_rejected()
{
	{
		std::stringstream ss;
		bool threw = false;
		try { ThreadSafeIOStream tio(ss, ss); }
		catch (const std::invalid_argument&) { threw = true; }
		CHECK(threw);                  // same buffer on both sides
	}
	{
		std::istringstream in;
		std::ostream noBuf(nullptr);
		bool threw = false;
		try { ThreadSafeIOStream tio(in, noBuf); }
		catch (const std::invalid_argument&) { threw = true; }
		CHECK(threw);                  // stream without a buffer
	}
	{
		std::istringstream in;
		std::ostringstream out;
		ThreadSafeIOStream tio(in, out);
		bool threw = false;
		try { tio.setOutStream(out); tio.setInStream(in); }
		catch (...) { threw = true; }
		CHECK(!threw);                 // re-setting the same streams is harmless
	}
}

// ----------------------------------------------------------- output: multi-thread
static void test_concurrent_output_lines()
{
	constexpr int N = 8, M = 300;
	std::istringstream dummy;
	std::ostringstream out;            // NOT thread-safe on its own

	runThreads(N, [&](int id) {
		ThreadSafeIOStream tio(dummy, out);   // same out -> same mutex
		writerWork(tio, id, M);
	});

	verifyThreadLines(splitLines(out.str()), N, M);
}

// One << statement containing several lines must be written as one unit.
static void test_multiline_chunk_is_atomic_across_threads()
{
	constexpr int N = 8, M = 300;
	std::istringstream dummy;
	std::ostringstream out;

	runThreads(N, [&](int id) {
		ThreadSafeIOStream tio(dummy, out);
		tio.setPrefix("[T" + std::to_string(id) + "] ");
		for (int k = 0; k < M; ++k)
			tio << "first " << k << "\nsecond " << k << "\n";
	});

	std::vector<std::string> lines = splitLines(out.str());
	CHECK(lines.size() == static_cast<size_t>(N * M * 2));
	int firsts = 0;
	for (size_t i = 0; i < lines.size(); ++i)
	{
		size_t pos = lines[i].find("] first ");
		if (pos == std::string::npos)
			continue;
		++firsts;
		std::string k = lines[i].substr(pos + 8);
		CHECK(i + 1 < lines.size() && lines[i + 1] == "second " + k);
	}
	CHECK(firsts == N * M);
}

static void test_format_isolated_per_thread()
{
	constexpr int N = 8, M = 1000;
	std::istringstream dummy;
	std::ostringstream out;

	runThreads(N, [&](int id) {
		ThreadSafeIOStream tio(dummy, out);
		tio.setPrefix("[T" + std::to_string(id) + "] ");
		if (id % 2 == 0)
			tio << std::hex;
		for (int k = 0; k < M; ++k)
			tio << 255 << std::endl;
	});

	std::vector<std::string> lines = splitLines(out.str());
	CHECK(lines.size() == static_cast<size_t>(N * M));
	for (const std::string& l : lines)
	{
		int id = l.size() > 3 ? l[2] - '0' : -1;
		CHECK(l.substr(5) == (id % 2 == 0 ? "ff" : "255"));
	}
}

// Two different outputs must not block each other, and must not mix.
static void test_independent_streams()
{
	constexpr int N = 20000;
	std::istringstream d1, d2;
	std::ostringstream o1, o2;

	std::thread a([&] { ThreadSafeIOStream tio(d1, o1); for (int i = 0; i < N; ++i) tio << "A" << i << '\n'; });
	std::thread b([&] { ThreadSafeIOStream tio(d2, o2); for (int i = 0; i < N; ++i) tio << "B" << i << '\n'; });
	a.join();
	b.join();

	CHECK(splitLines(o1.str()).size() == static_cast<size_t>(N));
	CHECK(splitLines(o2.str()).size() == static_cast<size_t>(N));
	CHECK(o1.str().find('B') == std::string::npos);
	CHECK(o2.str().find('A') == std::string::npos);
}

// The real thread_local threadSafeCout, with std::cout redirected to a buffer.
// Redirect BEFORE any thread touches threadSafeCout, and don't use it from main.
static void test_global_threadSafeCout()
{
	constexpr int N = 4, M = 200;
	std::ostringstream captured;
	std::streambuf* old = std::cout.rdbuf(captured.rdbuf());

	{
		std::vector<std::thread> ts;
		for (int id = 0; id < N; ++id)
			ts.emplace_back([id] { writerWork(threadSafeCout, id, M); });
		ts.emplace_back([] { threadSafeCout << "noprefix\n"; }); // fresh thread: empty prefix
		for (auto& t : ts)
			t.join();
	}
	std::cout.rdbuf(old);

	std::vector<std::string> lines = splitLines(captured.str());
	auto it = std::find(lines.begin(), lines.end(), "noprefix");
	CHECK(it != lines.end());
	if (it != lines.end())
		lines.erase(it);
	verifyThreadLines(lines, N, M);
}

// ------------------------------------------------------------------------ input
static void test_prompt_int()
{
	std::istringstream in("42\n");
	std::ostringstream out;
	ThreadSafeIOStream tio(in, out);
	tio.setPrefix("[P] ");

	int n = 0;
	CHECK(ask(tio, "Age: ", n));
	CHECK(n == 42);
	CHECK(out.str() == "[P] Age: ");   // prompt text, no newline
}

static void test_prompt_string_is_raw()
{
	std::istringstream in("John\n  \nWick\n  spaced   out \n");
	std::ostringstream out;
	ThreadSafeIOStream tio(in, out);

	std::string a, b, c, d;
	CHECK(ask(tio, "1: ", a));
	CHECK(ask(tio, "2: ", b));
	CHECK(ask(tio, "3: ", c));
	CHECK(ask(tio, "4: ", d));
	CHECK(a == "John");
	CHECK(b == "  ");                  // blank-ish answer kept as typed
	CHECK(c == "Wick");
	CHECK(d == "  spaced   out ");     // nothing trimmed or collapsed
}

static void test_prompt_invalid_input_throws_and_recovers()
{
	std::istringstream in("abc\n42\n");
	std::ostringstream out;
	ThreadSafeIOStream tio(in, out);

	int n = 0;
	bool threw = false;
	try { tio.prompt("n: ", n); }
	catch (const std::invalid_argument&) { threw = true; }
	CHECK(threw);                      // "abc" is not an int, the line is dropped

	CHECK(ask(tio, "n: ", n));         // next prompt starts clean
	CHECK(n == 42);
}

static void test_prompt_eof_throws()
{
	{
		std::istringstream in("");
		std::ostringstream out;
		ThreadSafeIOStream tio(in, out);
		int n = 0;
		std::string s;
		bool intEof = false, strEof = false;
		try { tio.prompt("n: ", n); } catch (const std::ios_base::failure&) { intEof = true; }
		try { tio.prompt("s: ", s); } catch (const std::ios_base::failure&) { strEof = true; }
		CHECK(intEof);
		CHECK(strEof);
	}
	{
		std::istringstream in("5");    // no trailing newline is still a valid value
		std::ostringstream out;
		ThreadSafeIOStream tio(in, out);
		int n = 0;
		CHECK(ask(tio, "n: ", n));
		CHECK(n == 5);
	}
}

static void test_prompt_crlf()
{
	std::istringstream in("hello\r\n7\r\n");
	std::ostringstream out;
	ThreadSafeIOStream tio(in, out);

	std::string s;
	int n = 0;
	CHECK(ask(tio, "s: ", s));
	CHECK(s == "hello");               // '\r' is a terminator, not content
	CHECK(ask(tio, "n: ", n));
	CHECK(n == 7);
}

static void test_prompt_int_then_string_stays_in_sync()
{
	std::istringstream in("5 trailing junk\nhello world\n");
	std::ostringstream out;
	ThreadSafeIOStream tio(in, out);

	int n = 0;
	std::string s;
	CHECK(ask(tio, "n: ", n));
	CHECK(ask(tio, "s: ", s));
	CHECK(n == 5);
	CHECK(s == "hello world");         // not "" and not " trailing junk"
}

static void test_operator_extract()
{
	std::istringstream in("7 8\n");
	std::ostringstream out;
	ThreadSafeIOStream tio(in, out);

	int a = 0, b = 0;
	tio >> a >> b;
	CHECK(a == 7);
	CHECK(b == 8);
}

// Many threads reading from one stream: every value delivered exactly once.
static void test_concurrent_prompt_no_loss_no_duplicates()
{
	constexpr int N = 20000, T = 12;
	std::string src;
	for (int i = 0; i < N; ++i)
		src += std::to_string(i) + "\n";

	std::istringstream in(src);
	std::ostringstream out;
	std::vector<std::vector<int>> got(T);
	std::atomic<int> eofs{0};

	runThreads(T, [&](int id) {
		ThreadSafeIOStream tio(in, out);   // same in -> same input mutex
		for (;;)
		{
			int v = 0;
			try { tio.prompt("?", v); }
			catch (const std::ios_base::failure&) { ++eofs; break; }
			got[id].push_back(v);
		}
	});

	std::set<int> seen;
	size_t total = 0;
	for (const auto& g : got)
		for (int v : g) { seen.insert(v); ++total; }

	CHECK(total == static_cast<size_t>(N));
	CHECK(seen.size() == static_cast<size_t>(N));
	CHECK(eofs == T);                          // every thread sees EOF once
	CHECK(out.str() == std::string(N + T, '?'));   // one '?' per attempt
}

// Readers and writers at once on the same streams.
static void test_mixed_read_write()
{
	constexpr int T = 8, N = 3000;
	std::string src;
	for (int i = 0; i < T * N; ++i)
		src += std::to_string(i) + "\n";

	std::istringstream in(src);
	std::ostringstream out;
	std::atomic<long> sum{0}, count{0};

	runThreads(T, [&](int id) {
		ThreadSafeIOStream tio(in, out);
		tio.setPrefix("[T" + std::to_string(id) + "] ");
		for (int j = 0; j < N; ++j)
		{
			int v = 0;
			tio >> v;
			sum += v;
			++count;
			tio << "log " << j << '\n';
		}
	});

	CHECK(count == T * N);
	CHECK(sum == static_cast<long>(T * N) * (T * N - 1) / 2);
	std::vector<std::string> lines = splitLines(out.str());
	CHECK(lines.size() == static_cast<size_t>(T * N));
	for (const std::string& l : lines)
	{
		int id = -1, j = -1;
		CHECK(std::sscanf(l.c_str(), "[T%d] log %d", &id, &j) == 2);
	}
}

// ------------------------------------------------------------------------- main
int main()
{
	RUN(test_prefix_and_basic_output);
	RUN(test_prefix_once_per_statement);
	RUN(test_each_statement_is_emitted_immediately);
	RUN(test_endl);
	RUN(test_formatting_manipulators);
	RUN(test_format_persists_across_statements);
	RUN(test_format_does_not_leak_between_wrappers);
	RUN(test_set_out_stream_switches_target);
	RUN(test_invalid_streams_are_rejected);

	RUN(test_concurrent_output_lines);
	RUN(test_multiline_chunk_is_atomic_across_threads);
	RUN(test_format_isolated_per_thread);
	RUN(test_independent_streams);
	RUN(test_global_threadSafeCout);

	RUN(test_prompt_int);
	RUN(test_prompt_string_is_raw);
	RUN(test_prompt_invalid_input_throws_and_recovers);
	RUN(test_prompt_eof_throws);
	RUN(test_prompt_crlf);
	RUN(test_prompt_int_then_string_stays_in_sync);
	RUN(test_operator_extract);
	RUN(test_concurrent_prompt_no_loss_no_duplicates);
	RUN(test_mixed_read_write);

	std::cout << "\n" << g_checks << " checks, " << g_failures << " failed\n";
	return g_failures == 0 ? 0 : 1;
}
