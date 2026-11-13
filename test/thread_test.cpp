#include "thread.hpp"
#include "thread_safe_iostream.hpp"

#include <atomic>
#include <chrono>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
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

// Redirects std::cout into a buffer for the lifetime of the object.
// Must be created BEFORE the threads that print are started.
class CoutCapture
{
	public:
		CoutCapture(bool cerr) : _cerr(cerr), _old(std::cerr.rdbuf(_buf.rdbuf())) {};
		CoutCapture() : _old(std::cout.rdbuf(_buf.rdbuf())) {}
		~CoutCapture() {
			if (_cerr)
				std::cerr.rdbuf(_old);
			else
				std::cout.rdbuf(_old); 
		}
		std::string str() const { return _buf.str(); }

	private:
		bool				_cerr = false;
		std::ostringstream	_buf;
		std::streambuf*		_old;
};

// ---------------------------------------------------------------- lifecycle
static void test_nothing_runs_before_start()
{
	std::atomic<bool> ran{false};
	{
		Thread t("idle", [&] { ran = true; });
		std::this_thread::sleep_for(std::chrono::milliseconds(30));
		CHECK(!ran);
	}
	CHECK(!ran);                       // destroyed without start: still never ran
}

static void test_start_runs_function()
{
	std::atomic<bool> ran{false};
	Thread t("worker", [&] { ran = true; });
	t.start();
	t.stop();
	CHECK(ran);
}

static void test_function_runs_on_another_thread()
{
	std::thread::id inside;
	Thread t("worker", [&] { inside = std::this_thread::get_id(); });
	t.start();
	t.stop();
	CHECK(inside != std::thread::id());
	CHECK(inside != std::this_thread::get_id());
}

static void test_stop_waits_for_function()
{
	std::atomic<bool> done{false};
	Thread t("slow", [&] {
		std::this_thread::sleep_for(std::chrono::milliseconds(80));
		done = true;
	});
	t.start();
	CHECK(!done);                      // still sleeping
	t.stop();
	CHECK(done);                       // stop() really joined
}

static void test_destructor_joins()
{
	std::atomic<bool> done{false};
	{
		Thread t("slow", [&] {
			std::this_thread::sleep_for(std::chrono::milliseconds(80));
			done = true;
		});
		t.start();
	}                                  // ~Thread must join, not terminate
	CHECK(done);
}

static void test_stop_is_idempotent()
{
	Thread t("worker", [] {});
	t.stop();                          // never started: no-op
	t.start();
	t.stop();
	t.stop();                          // already joined: no-op
	CHECK(true);
}

static void test_restart_after_stop()
{
	std::atomic<int> runs{0};
	Thread t("again", [&] { ++runs; });
	t.start();
	t.stop();
	t.start();
	t.stop();
	CHECK(runs == 2);
}

static void test_double_start_throws()
{
	std::atomic<bool> release{false};
	Thread t("busy", [&] { while (!release) std::this_thread::yield(); });
	t.start();

	bool threw = false;
	try { t.start(); }
	catch (const std::logic_error&) { threw = true; }
	CHECK(threw);

	release = true;
	t.stop();
}

static void test_empty_function_is_rejected()
{
	bool threw = false;
	try { Thread t("empty", std::function<void()>()); }
	catch (const std::invalid_argument&) { threw = true; }
	CHECK(threw);
}

static void test_stop_from_inside_throws()
{
	Thread* self = nullptr;
	std::atomic<bool> threw{false};
	std::atomic<bool> ready{false};
	Thread t("selfish", [&] {
		while (!ready) std::this_thread::yield();
		try { self->stop(); }
		catch (const std::logic_error&) { threw = true; }
	});
	self = &t;
	t.start();
	ready = true;
	t.stop();
	CHECK(threw);                      // would otherwise be a deadlock / system_error
}

// ---------------------------------------------------------------- prefix / output
static void test_name_becomes_prefix()
{
	CoutCapture cap;
	Thread t("worker", [] { threadSafeCout << "hello " << 42 << '\n'; });
	t.start();
	t.stop();
	CHECK(cap.str() == "[worker] hello 42\n");
}

static void test_prefix_is_per_thread()
{
	CoutCapture cap;
	Thread a("alpha", [] { threadSafeCout << "from a\n"; });
	Thread b("beta",  [] { threadSafeCout << "from b\n"; });
	a.start();
	a.stop();                          // sequential: output order is known
	b.start();
	b.stop();
	CHECK(cap.str() == "[alpha] from a\n[beta] from b\n");
}

static void test_many_named_threads_stay_whole()
{
	constexpr int N = 8, M = 500;
	CoutCapture cap;
	{
		std::vector<Thread*> ts;
		for (int id = 0; id < N; ++id)
			ts.push_back(new Thread("T" + std::to_string(id), [id] {
				for (int k = 0; k < M; ++k)
					threadSafeCout << "msg " << k << " id " << id << '\n';
			}));
		for (Thread* t : ts) t->start();
		for (Thread* t : ts) t->stop();
		for (Thread* t : ts) delete t;
	}

	std::vector<std::string> lines = splitLines(cap.str());
	CHECK(lines.size() == static_cast<size_t>(N * M));
	std::vector<int> next(N, 0);
	for (const std::string& l : lines)
	{
		int id = -1, k = -1, id2 = -1;
		bool ok = std::sscanf(l.c_str(), "[T%d] msg %d id %d", &id, &k, &id2) == 3
				&& id == id2 && id >= 0 && id < N;
		CHECK(ok);
		if (!ok)
			continue;
		CHECK(k == next[id]++);        // prefix matches the writer, order kept per thread
	}
	for (int id = 0; id < N; ++id)
		CHECK(next[id] == M);
}

static void test_exception_is_reported_not_fatal()
{
	{
		CoutCapture cap(true);
		Thread bad("bad", [] { throw std::runtime_error("boom"); });
		bad.start();
		bad.stop();
		CHECK(cap.str() == "[bad] exception: boom\n");
	}
	{
		CoutCapture cap;
		Thread ok("ok", [] { threadSafeCout << "still alive\n"; });
		ok.start();
		ok.stop();
		CHECK(cap.str().find("[ok] still alive\n") != std::string::npos);
	}
}

// ------------------------------------------------------------------------- main
int main()
{
	RUN(test_nothing_runs_before_start);
	RUN(test_start_runs_function);
	RUN(test_function_runs_on_another_thread);
	RUN(test_stop_waits_for_function);
	RUN(test_destructor_joins);
	RUN(test_stop_is_idempotent);
	RUN(test_restart_after_stop);
	RUN(test_double_start_throws);
	RUN(test_empty_function_is_rejected);
	RUN(test_stop_from_inside_throws);

	RUN(test_name_becomes_prefix);
	RUN(test_prefix_is_per_thread);
	RUN(test_many_named_threads_stay_whole);
	RUN(test_exception_is_reported_not_fatal);

	std::cout << "\n" << g_checks << " checks, " << g_failures << " failed\n";
	return g_failures == 0 ? 0 : 1;
}