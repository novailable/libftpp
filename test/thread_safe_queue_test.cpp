#include "thread_safe_queue.hpp"

#include <algorithm>
#include <atomic>
#include <iostream>
#include <random>
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

template <typename F>
static bool throwsOutOfRange(F fn)
{
	try { fn(); }
	catch (const std::out_of_range&) { return true; }
	catch (...) { return false; }
	return false;
}

// ------------------------------------------------------------ single thread
static void test_push_back_pop_front_is_fifo()
{
	ThreadSafeQueue<int> q;
	q.push_back(1);
	q.push_back(2);
	q.push_back(3);
	CHECK(q.pop_front() == 1);
	CHECK(q.pop_front() == 2);
	CHECK(q.pop_front() == 3);
}

static void test_push_front_pop_back_is_fifo()
{
	ThreadSafeQueue<int> q;
	q.push_front(1);
	q.push_front(2);
	q.push_front(3);
	CHECK(q.pop_back() == 1);
	CHECK(q.pop_back() == 2);
	CHECK(q.pop_back() == 3);
}

static void test_both_ends_are_lifo()
{
	ThreadSafeQueue<int> q;
	q.push_back(1);
	q.push_back(2);
	CHECK(q.pop_back() == 2);
	CHECK(q.pop_back() == 1);

	q.push_front(1);
	q.push_front(2);
	CHECK(q.pop_front() == 2);
	CHECK(q.pop_front() == 1);
}

static void test_mixed_ends()
{
	ThreadSafeQueue<std::string> q;
	q.push_back("b");
	q.push_front("a");
	q.push_back("c");                   // a b c
	CHECK(q.pop_front() == "a");
	CHECK(q.pop_back() == "c");
	CHECK(q.pop_back() == "b");
}

static void test_pop_on_empty_throws()
{
	ThreadSafeQueue<int> q;
	CHECK(throwsOutOfRange([&] { q.pop_front(); }));
	CHECK(throwsOutOfRange([&] { q.pop_back(); }));

	q.push_back(1);
	q.pop_front();
	CHECK(throwsOutOfRange([&] { q.pop_front(); }));   // empty again after use
	CHECK(throwsOutOfRange([&] { q.pop_back(); }));
	q.push_back(5);                                    // and still usable afterwards
	CHECK(q.pop_back() == 5);
}

static void test_size_and_empty()
{
	ThreadSafeQueue<int> q;
	CHECK(q.empty());
	CHECK(q.size() == 0);
	q.push_back(1);
	q.push_front(2);
	CHECK(!q.empty());
	CHECK(q.size() == 2);
	q.pop_back();
	CHECK(q.size() == 1);
	q.pop_front();
	CHECK(q.empty());
	CHECK(throwsOutOfRange([&] { q.pop_front(); }));
	CHECK(q.size() == 0);               // a failed pop leaves the counter alone
}

// If copying/moving the element out throws, it must stay in the queue.
struct ThrowsOnMove
{
	int v;
	explicit ThrowsOnMove(int x) : v(x) {}
	ThrowsOnMove(const ThrowsOnMove&) = default;
	ThrowsOnMove(ThrowsOnMove&&) { throw std::runtime_error("move failed"); }
};

static void test_throwing_move_keeps_element()
{
	ThreadSafeQueue<ThrowsOnMove> q;
	bool threw = false;
	try
	{ q.push_back(ThrowsOnMove(7));}
	catch (const std::runtime_error&) { threw = true; }
	try { q.pop_front(); }
	catch (const std::runtime_error&) { threw = true; }
	CHECK(threw);
	CHECK(q.size() == 1);               // not lost, and the counter agrees
}

// ------------------------------------------------------------- multi thread
// P producers, C consumers: every value delivered exactly once, and values from
// one producer reach any single consumer in the order they were pushed.
static void test_producers_consumers_fifo()
{
	constexpr int P = 4, C = 4, K = 20000;
	ThreadSafeQueue<int> q;
	std::atomic<int> consumed{0};
	std::vector<std::vector<int>> got(C);

	std::vector<std::thread> ts;
	for (int p = 0; p < P; ++p)
		ts.emplace_back([&, p] {
			for (int i = 0; i < K; ++i)
				q.push_back(p * K + i);
		});
	for (int c = 0; c < C; ++c)
		ts.emplace_back([&, c] {
			while (consumed < P * K)
			{
				try { got[c].push_back(q.pop_front()); ++consumed; }
				catch (const std::out_of_range&) { std::this_thread::yield(); }
			}
		});
	for (auto& t : ts)
		t.join();

	std::vector<int> all;
	for (const auto& g : got)
	{
		all.insert(all.end(), g.begin(), g.end());
		std::vector<int> last(P, -1);
		for (int v : g)
		{
			int p = v / K;
			CHECK(v > last[p]);         // per-producer order kept
			last[p] = v;
		}
	}
	std::sort(all.begin(), all.end());
	bool exact = all.size() == static_cast<size_t>(P * K);
	for (int i = 0; exact && i < P * K; ++i)
		exact = all[i] == i;
	
	CHECK(exact);                       // no loss, no duplicates
	CHECK(q.empty());
}

// All four operations at once, plus an observer reading size()/empty() lock-free.
static void test_all_operations_concurrently()
{
	constexpr int T = 8, N = 40000;
	ThreadSafeQueue<long> q;
	std::atomic<long> pushedSum{0}, poppedSum{0};
	std::atomic<long> pushedCnt{0}, poppedCnt{0}, emptyHits{0};
	std::atomic<bool> done{false};

	std::thread observer([&] {
		size_t maxSeen = 0;
		while (!done)
		{
			maxSeen = std::max(maxSeen, q.size());
			(void)q.empty();
		}
		CHECK(maxSeen <= static_cast<size_t>(T) * N);
	});

	runThreads(T, [&](int id) {
		std::mt19937 rng(static_cast<unsigned>(id) + 1);
		for (int i = 0; i < N; ++i)
		{
			long v = static_cast<long>(id) * N + i + 1;
			switch (rng() % 4)
			{
				case 0: q.push_back(v);  pushedSum += v; ++pushedCnt; break;
				case 1: q.push_front(v); pushedSum += v; ++pushedCnt; break;
				case 2:
					try { poppedSum += q.pop_back(); ++poppedCnt; }
					catch (const std::out_of_range&) { ++emptyHits; }
					break;
				default:
					try { poppedSum += q.pop_front(); ++poppedCnt; }
					catch (const std::out_of_range&) { ++emptyHits; }
					break;
			}
		}
	});
	done = true;
	observer.join();

	CHECK(q.size() == static_cast<size_t>(pushedCnt - poppedCnt));   // mirror matches the truth
	long drained = 0, drainedCnt = 0;
	while (!q.empty())
	{
		drained += q.pop_front();
		++drainedCnt;
	}
	CHECK(pushedCnt == poppedCnt + drainedCnt);
	CHECK(pushedSum == poppedSum + drained);                         // conservation
	CHECK(throwsOutOfRange([&] { q.pop_front(); }));
}

// A worker handing results back through the queue: the data written before push
// must be fully visible after pop (the mutex's acquire/release at work).
struct Message
{
	int id;
	std::string text;
	std::vector<int> data;
};

static void test_data_is_visible_after_pop()
{
	constexpr int N = 20000;
	ThreadSafeQueue<Message> q;
	std::atomic<bool> ok{true};

	std::thread producer([&] {
		for (int i = 0; i < N; ++i)
			q.push_back(Message{i, "msg" + std::to_string(i), {i, i + 1, i + 2}});
	});
	std::thread consumer([&] {
		int next = 0;
		while (next < N)
		{
			try
			{
				Message m = q.pop_front();
				if (m.id != next || m.text != "msg" + std::to_string(next)
					|| m.data != std::vector<int>{next, next + 1, next + 2})
					ok = false;
				++next;
			}
			catch (const std::out_of_range&) { std::this_thread::yield(); }
		}
	});
	producer.join();
	consumer.join();
	CHECK(ok);
}

// ------------------------------------------------------------------------- main
int main()
{
	RUN(test_push_back_pop_front_is_fifo);
	RUN(test_push_front_pop_back_is_fifo);
	RUN(test_both_ends_are_lifo);
	RUN(test_mixed_ends);
	RUN(test_pop_on_empty_throws);
	RUN(test_size_and_empty);
	RUN(test_throwing_move_keeps_element);

	RUN(test_producers_consumers_fifo);
	RUN(test_all_operations_concurrently);
	RUN(test_data_is_visible_after_pop);

	std::cout << "\n" << g_checks << " checks, " << g_failures << " failed\n";
	return g_failures == 0 ? 0 : 1;
}