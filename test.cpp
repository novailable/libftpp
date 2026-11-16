#include <iostream>
#include <thread>
#include <unordered_map>
#include <atomic>

std::atomic<int> currentKey{-1};

void worker(const std::unordered_map<int, std::string>& tasks)
{
    for (const auto& [key, task] : tasks)
    {
        currentKey.store(key);

        // Process task
    }

    currentKey.store(-1); // Finished
}

int main()
{
    std::cout << "shit" << std::endl;
    std::unordered_map<int, std::string> tasks = {
        {1, "Task A"},
        {2, "Task B"},
        {3, "Task C"}
    };

    std::thread t(worker, std::cref(tasks));

    while (currentKey.load() != -1)
    {
        std::cout << "Worker processing key: "
                  << currentKey.load() << '\n';

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    t.join();
}