#ifndef PRESISTENT_WORKER_HPP
#define PRESISTENT_WORKER_HPP

#include <thread>
#include <vector>
#include <functional>
#include <memory>

#include "thread_safe_iostream.hpp"

class   PresistentWorker
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
        PresistentWorker();
        ~PresistentWorker();

        PresistentWorker(const PresistentWorker&) = delete;
        PresistentWorker&   operator=(const PresistentWorker&) = delete;
        PresistentWorker(PresistentWorker&&) = delete;
        PresistentWorker&   operator=(const PresistentWorker&&) = delete;

        void    addTask(const std::string& name, const Job& jobToExecute);
        void    removeTask(const std::string& name);
};

#endif
