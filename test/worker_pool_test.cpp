
#include "worker_pool.hpp"

#include <atomic>
#include <chrono>
#include <ctime>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

// ---------------------------------------------------------------- mini harness

static std::atomic<int> g_checks{0};
static std::atomic<int> g_failures{0};

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
        fn();                                    \
    } while (0)

// ------------------------------------------------------------------- helpers

// Polls until pred() is true or the time limit expires.
template <class Pred>
static bool waitFor(
    Pred pred,
    std::chrono::milliseconds limit = 10s)
{
    const auto end = std::chrono::steady_clock::now() + limit;

    while (!pred())
    {
        if (std::chrono::steady_clock::now() > end)
            return false;

        std::this_thread::sleep_for(1ms);
    }

    return true;
}

// ------------------------------------------------------------------- execution

static void test_every_job_runs_exactly_once()
{
    constexpr int N = 5000;

    std::vector<std::atomic<int>> hits(N);
    for (auto& h : hits)
        h = 0;

    std::atomic<int> done{0};

    {
        WorkerPool pool(4);

        for (int i = 0; i < N; ++i)
        {
            pool.addJob([&, i] {
                ++hits[i];
                ++done;
            });
        }

        CHECK(waitFor([&] { return done == N; }));
    }

    int wrong = 0;

    for (auto& h : hits)
        wrong += (h != 1);

    CHECK(wrong == 0);
}

static void test_jobs_run_in_parallel()
{
    std::atomic<int> arrived{0};
    std::atomic<int> done{0};
    std::atomic<bool> timedOut{false};

    std::mutex m;
    std::set<std::thread::id> ids;

    {
        WorkerPool pool(4);

        CHECK(pool.threadCount() == 4);

        for (int i = 0; i < 4; ++i)
        {
            pool.addJob([&] {
                {
                    std::lock_guard<std::mutex> lk(m);
                    ids.insert(std::this_thread::get_id());
                }

                ++arrived;

                if (!waitFor([&] { return arrived == 4; }, 5s))
                    timedOut = true;

                ++done;
            });
        }

        CHECK(waitFor([&] { return done == 4; }));
    }

    CHECK(!timedOut);
    CHECK(ids.size() == 4);
}

static void test_single_worker_runs_jobs_in_fifo_order()
{
    std::mutex m;
    std::vector<int> order;
    std::atomic<int> done{0};

    {
        WorkerPool pool(1);

        for (int i = 0; i < 200; ++i)
        {
            pool.addJob([&, i] {
                {
                    std::lock_guard<std::mutex> lk(m);
                    order.push_back(i);
                }

                ++done;
            });
        }

        CHECK(waitFor([&] { return done == 200; }));
    }

    bool sorted = order.size() == 200;

    for (size_t i = 0; i < order.size(); ++i)
        sorted = sorted && order[i] == static_cast<int>(i);

    CHECK(sorted);
}

// Thread-safe streambuf used to capture whatever the worker writes to std::cerr.
class LockedBuf : public std::streambuf
{
public:
    std::string str()
    {
        std::lock_guard<std::mutex> lk(_m);
        return _data;
    }

protected:
    int_type overflow(int_type c) override
    {
        if (traits_type::eq_int_type(c, traits_type::eof()))
            return traits_type::not_eof(c);

        std::lock_guard<std::mutex> lk(_m);
        _data.push_back(traits_type::to_char_type(c));
        return c;
    }

    std::streamsize xsputn(const char* s, std::streamsize n) override
    {
        std::lock_guard<std::mutex> lk(_m);
        _data.append(s, static_cast<size_t>(n));
        return n;
    }

private:
    std::mutex _m;
    std::string _data;
};

struct CerrCapture
{
    LockedBuf buf;
    std::streambuf* old;

    CerrCapture() : old(std::cerr.rdbuf(&buf)) {}
    ~CerrCapture() { std::cerr.rdbuf(old); }

    std::string str() { return buf.str(); }
};

static void test_throwing_jobs_do_not_kill_workers()
{
    std::atomic<int> ran{0};
    CerrCapture cap;
    {
        WorkerPool pool(1);

        pool.addJob([] { throw std::runtime_error("boom"); });
        pool.addJob([] { throw 42; });
        pool.addJob([&] { ++ran; });
        pool.addJob([] { throw std::logic_error("again"); });
        pool.addJob([&] { ++ran; });

        CHECK(waitFor([&] { return ran == 2; }));
    }
    std::string log;
    log = cap.str();
    CHECK((log.find("boom") > 0) & (log.find("42") > 0) & (log.find("again") > 0));
    CHECK(ran == 2);
}

// ------------------------------------------------------------------- lifecycle

static void test_destructor_drains_backlog()
{
    std::atomic<int> n{0};

    {
        WorkerPool pool(2);

        for (int i = 0; i < 300; ++i)
        {
            pool.addJob([&] {
                std::this_thread::sleep_for(1ms);
                ++n;
            });
        }
    }

    CHECK(n == 300);
}

static void test_destructor_waits_for_running_job()
{
    std::atomic<bool> started{false};
    std::atomic<bool> finished{false};

    {
        WorkerPool pool(1);

        pool.addJob([&] {
            started = true;
            std::this_thread::sleep_for(200ms);
            finished = true;
        });

        CHECK(waitFor([&] { return started.load(); }));
    }

    CHECK(finished);
}

struct CountJob : WorkerPool::IJobs
{
    explicit CountJob(
        std::atomic<int>& c,
        std::atomic<int>& destroyed)
        : counter(c), destroyedCount(destroyed) {}

    ~CountJob() override
    {
        ++destroyedCount;
    }

    void execute() override
    {
        ++counter;
    }

    std::atomic<int>& counter;
    std::atomic<int>& destroyedCount;
};

static void test_ijobs_overload_and_lifetime()
{
    std::atomic<int> ran{0};
    std::atomic<int> destroyed{0};

    {
        WorkerPool pool(2);

        for (int i = 0; i < 100; ++i)
        {
            pool.addJob(
                std::make_shared<CountJob>(ran, destroyed)
            );
        }

        CHECK(waitFor([&] { return ran == 100; }));
    }

    CHECK(ran == 100);
    CHECK(destroyed == 100);
}

// ------------------------------------------------------------------- validation

static void test_invalid_arguments()
{
    WorkerPool pool(1);

    bool threw = false;

    try
    {
        pool.addJob(std::function<void()>{});
    }
    catch (const std::invalid_argument&)
    {
        threw = true;
    }

    CHECK(threw);

    threw = false;

    try
    {
        pool.addJob(std::shared_ptr<WorkerPool::IJobs>{});
    }
    catch (const std::invalid_argument&)
    {
        threw = true;
    }

    CHECK(threw);
}

// ------------------------------------------------------------------- concurrency

static void test_many_producers()
{
    constexpr int P = 6;
    constexpr int PER = 3000;

    std::atomic<int> n{0};

    {
        WorkerPool pool(4);
        std::vector<std::thread> producers;

        for (int p = 0; p < P; ++p)
        {
            producers.emplace_back([&] {
                for (int i = 0; i < PER; ++i)
                    pool.addJob([&] { ++n; });
            });
        }

        for (auto& t : producers)
            t.join();

        CHECK(waitFor([&] { return n == P * PER; }));
    }

    CHECK(n == P * PER);
}

static void test_jobs_can_submit_jobs()
{
    std::atomic<int> leaves{0};

    {
        WorkerPool pool(3);

        for (int i = 0; i < 20; ++i)
        {
            pool.addJob([&] {
                for (int j = 0; j < 10; ++j)
                    pool.addJob([&] { ++leaves; });
            });
        }

        CHECK(waitFor([&] { return leaves == 200; }));
    }

    CHECK(leaves == 200);
}

static void test_idle_pool_is_asleep_and_wakes_later()
{
    std::atomic<int> n{0};

    WorkerPool pool(4);

    pool.addJob([&] { ++n; });

    CHECK(waitFor([&] { return n == 1; }));

    const std::clock_t cpu0 = std::clock();

    std::this_thread::sleep_for(300ms);

    const double cpuMs =
        1000.0 * static_cast<double>(std::clock() - cpu0)
        / CLOCKS_PER_SEC;

    CHECK(cpuMs < 100.0);

    pool.addJob([&] { ++n; });

    CHECK(waitFor([&] { return n == 2; }));
}

static void test_no_lost_wakeup_one_job_at_a_time()
{
    std::atomic<int> n{0};

    WorkerPool pool(3);

    constexpr int total = 1000;
    constexpr int barWidth = 40;

    for (int i = 1; i <= total; ++i)
    {
        pool.addJob([&] { ++n; });

        if (!waitFor([&] { return n.load() == i; }, 5s))
        {
            std::cout << '\n';
            CHECK(false && "job stuck: lost wakeup");
            return;
        }

        // Update the progress bar every 100 jobs.
        if (i % 100 == 0 || i == total)
        {
            const int progress = (i * 100) / total;
            const int pos = (barWidth * i) / total;

            std::cout << '\r' << '[';

            for (int j = 0; j < barWidth; ++j)
			{
                std::cout << (j < pos ? '=' : '-');
			}

            std::cout << "] "
                      << progress << "% "
                      << i << "/" << total
                      << std::flush;
        }

        if (i % 5000 == 0)
            std::this_thread::sleep_for(20ms);
    }

    std::cout << '\n';
    CHECK(n == total);
}

static void test_construct_and_destroy_repeatedly()
{
    for (int i = 0; i < 300; ++i)
    {
        WorkerPool pool(4);

        if (i % 3 == 1)
            std::this_thread::sleep_for(1ms);

        if (i % 3 == 2)
            pool.addJob([] {});
    }

    CHECK(true);
}

static void test_zero_threads_becomes_one()
{
    std::atomic<int> n{0};

    WorkerPool pool(0);

    CHECK(pool.threadCount() == 1);

    pool.addJob([&] { ++n; });

    CHECK(waitFor([&] { return n == 1; }));
}

// ------------------------------------------------------------------------- main

int main()
{
    RUN(test_every_job_runs_exactly_once);
    RUN(test_jobs_run_in_parallel);
    RUN(test_single_worker_runs_jobs_in_fifo_order);
    RUN(test_throwing_jobs_do_not_kill_workers);

    RUN(test_destructor_drains_backlog);
    RUN(test_destructor_waits_for_running_job);
    RUN(test_ijobs_overload_and_lifetime);
    RUN(test_invalid_arguments);

    RUN(test_many_producers);
    RUN(test_jobs_can_submit_jobs);
    RUN(test_idle_pool_is_asleep_and_wakes_later);
    RUN(test_no_lost_wakeup_one_job_at_a_time);
    RUN(test_construct_and_destroy_repeatedly);
    RUN(test_zero_threads_becomes_one);

    std::cout << "\n"
              << g_checks << " checks, "
              << g_failures << " failed\n";

    return g_failures == 0 ? 0 : 1;
}
