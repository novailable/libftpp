#ifndef PERSISTENT_WORKER_HPP
#define PERSISTENT_WORKER_HPP

#include <thread>
#include <vector>
#include <functional>
#include <memory>
#include <algorithm>
#include <exception>
#include <utility>

#include "thread_safe_iostream.hpp"

class   PersistentWorker
{
    private:
        using   Job = std::function<void()>;
        struct  task_t
        {
            std::string   name;
            std::shared_ptr<Job> job;
        };
        
        using Tasks = std::vector<task_t>;

        std::atomic<bool>   _running;
        std::atomic<std::shared_ptr<const Tasks>>  _tasks;
        std::thread _worker;
        
        void    workerJob();
        void    shutdown();
        Tasks::iterator find(std::shared_ptr<Tasks> tasks, const std::string &name);
    
    public:
        PersistentWorker();
        ~PersistentWorker();

        PersistentWorker(const PersistentWorker&) = delete;
        PersistentWorker&   operator=(const PersistentWorker&) = delete;
        PersistentWorker(PersistentWorker&&) = delete;
        PersistentWorker&   operator=(const PersistentWorker&&) = delete;

        void    addTask(const std::string& name, const Job& jobToExecute);
        void    removeTask(const std::string& name);
};

#endif
