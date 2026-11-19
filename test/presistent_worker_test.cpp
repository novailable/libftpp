#include "presistent_worker.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <random>
#include <set>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

// Change this one line if your class is spelled differently (e.g. PresistentWorker).
using Worker = PresistentWorker;

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

// Kills the process if a test stops making progress (deadlock, lost wakeup,
// destructor that never joins...). Uses C stdio so it still prints while a
// test has std::cerr redirected.
static void startWatchdog(std::chrono::seconds limit)
{
    std::thread([limit] {
        for (;;)
        {
            std::this_thread::sleep_for(200ms);

            if (nowMs() - g_lastProgressMs.load() > limit.count() * 1000)
            {
                std::fprintf(stderr,
                    "\n  WATCHDOG: %s made no progress for %lld s "
                    "(deadlock / lost wakeup / destructor never joined?)\n",
                    g_currentTest.load(),
                    static_cast<long long>(limit.count()));
                std::_Exit(2);
            }
        }
    }).detach();
}

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
        _data.append(s, static_cast<std::size_t>(n));
        return n;
    }

private:
    std::mutex _m;
    std::string _data;
};

// RAII: redirects std::cerr while alive. Do not CHECK() inside its scope,
// a failing CHECK would be swallowed too.
struct CerrCapture
{
    LockedBuf buf;
    std::streambuf* old;

    CerrCapture() : old(std::cerr.rdbuf(&buf)) {}
    ~CerrCapture() { std::cerr.rdbuf(old); }

    std::string str() { return buf.str(); }
};

static int countOccurrences(const std::string& hay, const std::string& needle)
{
    int n = 0;

    for (std::size_t p = hay.find(needle);
         p != std::string::npos;
         p = hay.find(needle, p + needle.size()))
        ++n;

    return n;
}

static std::string lineContaining(const std::string& text, const std::string& needle)
{
    const std::size_t p = text.find(needle);

    if (p == std::string::npos)
        return "";

    std::size_t b = text.rfind('\n', p);
    b = (b == std::string::npos) ? 0 : b + 1;

    std::size_t e = text.find('\n', p);
    if (e == std::string::npos)
        e = text.size();

    return text.substr(b, e - b);
}

// A job that blocks the worker mid-lap until released. Lets a test hold the
// worker inside a lap while it changes the task list.
// Declare it BEFORE the Worker so it outlives the worker.
struct Gate
{
    std::atomic<bool> entered{false};
    std::atomic<bool> released{false};
    std::atomic<int>  calls{0};

    std::function<void()> job()
    {
        return [this] {
            ++calls;
            entered = true;
            waitFor([this] { return released.load(); }, 10s);
        };
    }
};

// ------------------------------------------------------------------- execution

static void test_job_runs_repeatedly_in_a_loop()
{
    std::atomic<int> n{0};

    Worker w;
    w.addTask("loop", [&] { ++n; });

    CHECK(waitFor([&] { return n >= 5000; }));
}

static void test_all_jobs_run_on_one_background_thread()
{
    std::mutex m;
    std::set<std::thread::id> ids;
    std::atomic<int> a{0}, b{0}, c{0};

    auto rec = [&](std::atomic<int>& n) {
        return [&] {
            {
                std::lock_guard<std::mutex> lk(m);
                ids.insert(std::this_thread::get_id());
            }
            ++n;
        };
    };

    {
        Worker w;
        w.addTask("a", rec(a));
        w.addTask("b", rec(b));
        w.addTask("c", rec(c));

        CHECK(waitFor([&] { return a >= 50 && b >= 50 && c >= 50; }));
    }

    CHECK(ids.size() == 1);
    CHECK(ids.count(std::this_thread::get_id()) == 0);
}

static void test_tasks_run_in_insertion_order_every_lap()
{
    std::mutex m;
    std::vector<char> seq;

    auto rec = [&](char ch) {
        return [&, ch] {
            std::lock_guard<std::mutex> lk(m);
            if (seq.size() < 5000)
                seq.push_back(ch);
        };
    };

    Gate gate;
    Worker w;

    // Hold the worker inside a lap so A, B, C all land in the SAME next snapshot.
    w.addTask("gate", gate.job());
    CHECK(waitFor([&] { return gate.entered.load(); }));

    w.addTask("A", rec('A'));
    w.addTask("B", rec('B'));
    w.addTask("C", rec('C'));

    gate.released = true;

    CHECK(waitFor([&] {
        std::lock_guard<std::mutex> lk(m);
        return seq.size() >= 300;
    }));

    std::vector<char> copy;
    {
        std::lock_guard<std::mutex> lk(m);
        copy = seq;
    }

    bool ok = copy.size() >= 300;

    for (std::size_t i = 0; i < 300 && i < copy.size(); ++i)
        ok = ok && copy[i] == "ABC"[i % 3];

    CHECK(ok);
}

static void test_many_tasks_all_run()
{
    constexpr int N = 400;

    std::vector<std::atomic<int>> hits(N);
    for (auto& h : hits)
        h = 0;

    Worker w;

    for (int i = 0; i < N; ++i)
        w.addTask("task" + std::to_string(i), [&hits, i] { ++hits[i]; });

    const bool all = waitFor([&] {
        for (auto& h : hits)
            if (h == 0)
                return false;
        return true;
    }, 15s);

    CHECK(all);
}

static void test_same_name_replaces_and_never_duplicates()
{
    std::atomic<int> lap{0}, oldCalls{0}, newCalls{0};

    Worker w;
    w.addTask("lap", [&] { ++lap; });          // one tick per lap
    w.addTask("x", [&] { ++oldCalls; });

    CHECK(waitFor([&] { return oldCalls >= 10; }));

    w.addTask("x", [&] { ++newCalls; });       // replace

    CHECK(waitFor([&] { return newCalls >= 10; }));

    std::this_thread::sleep_for(50ms);         // let any in-flight old call land

    const int oldFrozen = oldCalls;
    const int lap0 = lap;
    const int new0 = newCalls;

    std::this_thread::sleep_for(200ms);

    const int dLap = lap - lap0;
    const int dNew = newCalls - new0;

    CHECK(oldCalls == oldFrozen);              // old job is gone
    CHECK(dLap > 0);
    CHECK(std::abs(dLap - dNew) <= 2);         // exactly one "x" per lap, not two
}

// ------------------------------------------------------------ list modification

static void test_removed_task_stops_running_others_continue()
{
    std::atomic<int> a{0}, b{0};

    Worker w;
    w.addTask("a", [&] { ++a; });
    w.addTask("b", [&] { ++b; });

    CHECK(waitFor([&] { return a >= 50 && b >= 50; }));

    w.removeTask("a");
    std::this_thread::sleep_for(100ms);

    const int aFrozen = a;
    const int b0 = b;

    std::this_thread::sleep_for(200ms);

    CHECK(a == aFrozen);
    CHECK(b > b0);
}

static void test_remove_unknown_task_is_harmless()
{
    std::atomic<int> n{0};
    bool ok = false;

    {
        CerrCapture cap;        // the worker may log "not found"; keep output clean
        Worker w;

        w.removeTask("nope");   // empty list
        w.addTask("real", [&] { ++n; });
        w.removeTask("nope");   // non-empty list
        w.removeTask("nope");

        ok = waitFor([&] { return n >= 100; });
    }

    CHECK(ok);
}

static void test_add_empty_job_is_harmless()
{
    std::atomic<int> n{0};
    bool ok = false;

    {
        CerrCapture cap;        // the worker may log "empty task"
        Worker w;

        w.addTask("empty", std::function<void()>{});
        w.addTask("real", [&] { ++n; });

        ok = waitFor([&] { return n >= 100; });
        w.removeTask("empty");
    }

    CHECK(ok);
}

static void test_updates_apply_only_between_laps()
{
    std::atomic<int> late{0}, victim{0};

    Gate gate;
    Worker w;

    w.addTask("gate", gate.job());
    CHECK(waitFor([&] { return gate.entered.load(); }));

    // Worker is stuck inside lap #1. A new task must not run yet...
    w.addTask("late", [&] { ++late; });
    std::this_thread::sleep_for(150ms);
    CHECK(late == 0);

    // ...and a task added then removed during the same lap must never run.
    w.addTask("victim", [&] { ++victim; });
    w.removeTask("victim");

    gate.released = true;

    CHECK(waitFor([&] { return late >= 10; }));
    CHECK(victim == 0);
}

static void test_add_remove_never_block_on_a_slow_job()
{
    Gate gate;
    Worker w;

    w.addTask("gate", gate.job());
    CHECK(waitFor([&] { return gate.entered.load(); }));

    const auto t0 = std::chrono::steady_clock::now();

    for (int i = 0; i < 1000; ++i)
    {
        const std::string name = "t" + std::to_string(i % 10);
        w.addTask(name, [] {});
        w.removeTask(name);
    }

    const auto elapsed = std::chrono::steady_clock::now() - t0;

    CHECK(elapsed < 2s);       // the control thread never waits for the worker
    CHECK(!gate.released);     // ...and the worker really was stuck the whole time

    gate.released = true;
}

static void test_job_can_modify_the_task_list_from_inside()
{
    std::atomic<int> selfRuns{0}, childRuns{0};

    Worker w;

    w.addTask("self", [&] {
        if (++selfRuns == 5)
        {
            w.removeTask("self");                       // destroys the running job's list entry
            w.addTask("child", [&] { ++childRuns; });
        }
    });

    CHECK(waitFor([&] { return childRuns >= 10; }));

    std::this_thread::sleep_for(50ms);
    CHECK(selfRuns == 5);
}

// ------------------------------------------------------------------- exceptions

static void test_throwing_jobs_do_not_kill_worker_and_message_is_reported()
{
    std::atomic<int> survivor{0};
    std::atomic<int> runtimeCalls{0}, logicCalls{0}, intCalls{0};

    bool alive = false;
    std::string log;

    {
        CerrCapture cap;
        {
            Worker w;

            // Each thrower throws only on its first 3 calls so stderr is not flooded,
            // then behaves. They must keep being called after throwing.
            w.addTask("thrower_runtime", [&] {
                if (runtimeCalls++ < 3) throw std::runtime_error("boom-runtime-7731");
            });
            w.addTask("thrower_logic", [&] {
                if (logicCalls++ < 3) throw std::logic_error("boom-logic-4412");
            });
            w.addTask("thrower_int", [&] {
                if (intCalls++ < 3) throw 42;
            });
            w.addTask("survivor", [&] { ++survivor; });

            alive = waitFor([&] {
                return survivor >= 1000 && runtimeCalls >= 10
                    && logicCalls >= 10 && intCalls >= 10;
            });
        }                       // worker destroyed => all output has been written
        log = cap.str();
    }

    CHECK(alive);
    CHECK(!log.empty() && "nothing captured: threadSafeCerr must write through std::cerr");

    // std::exception messages: what() text is reported, attributed to the right task.
    const std::string l1 = lineContaining(log, "boom-runtime-7731");
    CHECK(!l1.empty());
    CHECK(l1.find("thrower_runtime") != std::string::npos);
    CHECK(countOccurrences(log, "boom-runtime-7731") == 3);

    const std::string l2 = lineContaining(log, "boom-logic-4412");
    CHECK(!l2.empty());
    CHECK(l2.find("thrower_logic") != std::string::npos);
    CHECK(countOccurrences(log, "boom-logic-4412") == 3);

    // Non-std exception: no what(), but the task name must still be reported, once per throw.
    CHECK(countOccurrences(log, "thrower_int") == 3);
}

// ------------------------------------------------------------------- lifecycle

static void test_destructor_stops_idle_worker()
{
    for (int i = 0; i < 200; ++i)
    {
        Worker w;                          // never gets a task: worker is asleep
        if (i % 2)
            std::this_thread::sleep_for(1ms);
    }

    CHECK(true);   // reaching here means no destructor hung (watchdog would have fired)
}

static void test_destructor_waits_for_running_job_and_nothing_runs_after()
{
    struct S
    {
        std::atomic<bool> inJob{false};
        std::atomic<int> calls{0};
        std::atomic<int> exits{0};
    };

    auto s = std::make_shared<S>();

    {
        Worker w;

        w.addTask("slow", [s] {
            s->inJob = true;
            ++s->calls;
            std::this_thread::sleep_for(150ms);
            s->inJob = false;
            ++s->exits;
        });

        CHECK(waitFor([&] { return s->inJob.load(); }));
    }

    CHECK(!s->inJob);                    // destructor returned only after the job finished
    CHECK(s->calls == s->exits);

    const int c = s->calls;
    std::this_thread::sleep_for(400ms);
    CHECK(s->calls == c);                // worker thread is really gone
}

static void test_construct_and_destroy_repeatedly()
{
    for (int i = 0; i < 300; ++i)
    {
        Worker w;
        // std::atomic<int> n{0};

        switch (i % 5)
        {
            case 1: std::this_thread::sleep_for(1ms); break;
            case 2: w.addTask("t", [] {}); break;
            case 3: w.addTask("t", [] {}); w.removeTask("t"); break;
            case 4: w.addTask("t", [] {}); std::this_thread::sleep_for(2ms); break;
            default: break;
        }
    }

    CHECK(true);
}

// ------------------------------------------------------------------- concurrency

static void test_concurrent_adds_lose_nothing()
{
    constexpr int T = 8;
    constexpr int PER = 60;
    constexpr int N = T * PER;

    std::vector<std::atomic<int>> hits(N);
    for (auto& h : hits)
        h = 0;

    Worker w;
    std::atomic<bool> go{false};
    std::vector<std::thread> producers;

    for (int t = 0; t < T; ++t)
    {
        producers.emplace_back([&, t] {
            while (!go)
                std::this_thread::yield();

            for (int i = 0; i < PER; ++i)
            {
                const int id = t * PER + i;
                w.addTask("t" + std::to_string(id), [&hits, id] { ++hits[id]; });
            }
        });
    }

    go = true;

    for (auto& th : producers)
        th.join();

    waitFor([&] {
        for (auto& h : hits)
            if (h == 0)
                return false;
        return true;
    }, 15s);

    int missing = 0;
    for (auto& h : hits)
        missing += (h == 0);

    CHECK(missing == 0);       // a lost CAS update would leave a task that never runs
}

static void test_concurrent_add_remove_churn_keeps_worker_healthy()
{
    constexpr int T = 6;
    constexpr int ITER = 4000;
    constexpr int K = 16;

    std::vector<std::atomic<int>> cnt(K);
    for (auto& c : cnt)
        c = 0;

    std::atomic<int> heartbeat{0};
    bool beating = false;
    bool frozen = true;

    {
        CerrCapture cap;       // removing unknown names may log; discard it
        Worker w;

        w.addTask("heartbeat", [&] { ++heartbeat; });
        CHECK(waitFor([&] { return heartbeat >= 1; }));

        const int h0 = heartbeat;
        std::vector<std::thread> ts;

        for (int t = 0; t < T; ++t)
        {
            ts.emplace_back([&, t] {
                for (int i = 0; i < ITER; ++i)
                {
                    const int k = (t * 7 + i) % K;
                    w.addTask("c" + std::to_string(k), [&cnt, k] { ++cnt[k]; });

                    if (i % 3 == 0)
                        w.removeTask("c" + std::to_string((k + 5) % K));
                }
            });
        }

        for (auto& th : ts)
            th.join();

        beating = waitFor([&] { return heartbeat > h0 + 100; });

        for (int k = 0; k < K; ++k)
            w.removeTask("c" + std::to_string(k));

        std::this_thread::sleep_for(100ms);

        std::vector<int> snap;
        for (auto& c : cnt)
            snap.push_back(c);

        std::this_thread::sleep_for(150ms);

        for (int k = 0; k < K; ++k)
            frozen = frozen && (cnt[k] == snap[k]);

        const int h1 = heartbeat;
        std::this_thread::sleep_for(100ms);
        beating = beating && heartbeat > h1;
    }

    CHECK(beating);            // the permanent task survived every concurrent update
    CHECK(frozen);             // every churned task really is gone after being removed
}

static void test_mutable_job_state_survives_updates()
{
    std::atomic<int> rewinds{0}, calls{0}, noise{0};
    bool ran = false;

    {
        CerrCapture cap;
        Worker w;

        // Counts its own calls in captured state. If the job is COPIED on a list
        // update, the copy carries a stale n and the count jumps backwards.
        w.addTask("stateful", [&, n = 0]() mutable {
            ++n;
            if (n != calls.load() + 1)
                ++rewinds;
            calls.store(n);
        });

        std::thread noisy([&] {
            for (int i = 0; i < 3000; ++i)
            {
                const std::string name = "n" + std::to_string(i % 8);
                w.addTask(name, [&] { ++noise; });

                if (i % 2)
                    w.removeTask(name);
            }
        });

        noisy.join();
        ran = waitFor([&] { return calls >= 1000; });
    }

    CHECK(ran);
    CHECK(rewinds == 0);
}

static void test_no_lost_wakeup_add_remove_cycles()
{
    constexpr int total = 5000;

    Worker w;

    for (int i = 1; i <= total; ++i)
    {
        // Fresh counter per cycle so a straggler from the previous cycle can't fake success.
        auto ran = std::make_shared<std::atomic<int>>(0);

        w.addTask("cycle", [ran] { ++*ran; });

        if (!waitFor([&] { return ran->load() >= 1; }, 5s))
        {
            CHECK(false && "task never ran: lost wakeup");
            return;
        }

        w.removeTask("cycle");     // list empty again -> worker goes back to sleep

        if (i % 500 == 0)
            std::this_thread::sleep_for(5ms);   // let it really fall asleep now and then
    }

    CHECK(true);
}

static void test_idle_worker_is_asleep_and_wakes_later()
{
    std::atomic<int> n{0};

    Worker w;
    w.addTask("one", [&] { ++n; });

    CHECK(waitFor([&] { return n >= 1; }));

    w.removeTask("one");
    std::this_thread::sleep_for(100ms);        // let the lap finish and the worker sleep

    const std::clock_t cpu0 = std::clock();

    std::this_thread::sleep_for(300ms);

    const double cpuMs =
        1000.0 * static_cast<double>(std::clock() - cpu0)
        / CLOCKS_PER_SEC;

    CHECK(cpuMs < 100.0);                      // an empty worker must not spin

    const int n0 = n;
    w.addTask("again", [&] { ++n; });

    CHECK(waitFor([&] { return n > n0; }));
}

static void test_random_operation_soup_then_quiesce()
{
    constexpr int NAMES = 20;
    constexpr int T = 6;
    constexpr int OPS = 6000;

    std::vector<std::atomic<int>> cnt(NAMES);
    for (auto& c : cnt)
        c = 0;

    std::atomic<int> finalRuns{0};
    bool frozen = true;
    bool finalOk = false;

    {
        CerrCapture cap;       // lots of "not found" / "empty task" noise
        Worker w;
        std::vector<std::thread> ts;

        for (int t = 0; t < T; ++t)
        {
            ts.emplace_back([&, t] {
                std::mt19937 rng(1234u + static_cast<unsigned>(t));

                for (int i = 0; i < OPS; ++i)
                {
                    const int k = static_cast<int>(rng() % NAMES);
                    const int op = static_cast<int>(rng() % 10);
                    const std::string name = "s" + std::to_string(k);

                    if (op < 5)
                        w.addTask(name, [&cnt, k] { ++cnt[k]; });
                    else if (op < 9)
                        w.removeTask(name);
                    else
                        w.addTask(name, std::function<void()>{});
                }
            });
        }

        for (auto& th : ts)
            th.join();

        for (int k = 0; k < NAMES; ++k)
            w.removeTask("s" + std::to_string(k));

        std::this_thread::sleep_for(150ms);

        std::vector<int> snap;
        for (auto& c : cnt)
            snap.push_back(c);

        std::this_thread::sleep_for(150ms);

        for (int k = 0; k < NAMES; ++k)
            frozen = frozen && (cnt[k] == snap[k]);

        w.addTask("final", [&] { ++finalRuns; });
        finalOk = waitFor([&] { return finalRuns >= 1; });
    }

    CHECK(frozen);             // nothing the soup left behind is still running
    CHECK(finalOk);            // and the worker is still alive and responsive
}

// ------------------------------------------------------------------------- main

int main()
{
    startWatchdog(60s);

    RUN(test_job_runs_repeatedly_in_a_loop);
    RUN(test_all_jobs_run_on_one_background_thread);
    RUN(test_tasks_run_in_insertion_order_every_lap);
    RUN(test_many_tasks_all_run);
    RUN(test_same_name_replaces_and_never_duplicates);

    RUN(test_removed_task_stops_running_others_continue);
    RUN(test_remove_unknown_task_is_harmless);
    RUN(test_add_empty_job_is_harmless);
    RUN(test_updates_apply_only_between_laps);
    RUN(test_add_remove_never_block_on_a_slow_job);
    RUN(test_job_can_modify_the_task_list_from_inside);

    RUN(test_throwing_jobs_do_not_kill_worker_and_message_is_reported);

    RUN(test_destructor_stops_idle_worker);
    RUN(test_destructor_waits_for_running_job_and_nothing_runs_after);
    RUN(test_construct_and_destroy_repeatedly);

    RUN(test_concurrent_adds_lose_nothing);
    RUN(test_concurrent_add_remove_churn_keeps_worker_healthy);
    RUN(test_mutable_job_state_survives_updates);
    RUN(test_no_lost_wakeup_add_remove_cycles);
    RUN(test_idle_worker_is_asleep_and_wakes_later);
    RUN(test_random_operation_soup_then_quiesce);

    std::cout << "\n"
              << g_checks << " checks, "
              << g_failures << " failed\n";

    return g_failures == 0 ? 0 : 1;
}
