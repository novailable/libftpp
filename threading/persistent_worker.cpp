#include "persistent_worker.hpp"

PersistentWorker::PersistentWorker() 
    :   _running{true},
        _tasks{std::make_shared<const Tasks>()},
        _worker(&PersistentWorker::workerJob, this)
{
    threadSafeCerr.setPrefix("PersistentWorker::");
}

PersistentWorker::~PersistentWorker()
{
    shutdown();
}

void    PersistentWorker::shutdown()
{
    _running.store(false, std::memory_order_release);
    _tasks.store(std::make_shared<const Tasks>(), std::memory_order_release);
    _tasks.notify_all();
    if (_worker.joinable())
        _worker.join();
}

PersistentWorker::Tasks::iterator   PersistentWorker::find(std::shared_ptr<Tasks> tasks, const std::string& name)
{
    return  std::find_if(tasks->begin(), tasks->end(), [&](const task_t& task) {
        return task.name == name;
    });
}

void    PersistentWorker::workerJob()
{
    while (_running.load(std::memory_order_acquire))
    {
        auto tasks = _tasks.load(std::memory_order_acquire);
        if (tasks->empty())
        {
            _tasks.wait(tasks, std::memory_order_acquire);
            continue ;
        }

        for (const task_t& task : *tasks)
        {
            if (!_running.load(std::memory_order_relaxed))
                break ;
            auto    job = task.job;
            if (job && *job)
            {
                try
                {
                    (*job)();
                }
                catch(const std::exception& e)
                {
                    threadSafeCerr << task.name << ": " << e.what() << std::endl;
                }
                catch(...)
                {
                    threadSafeCerr << task.name << ": throw an exception" << std::endl;
                }  
            }
        }
        std::this_thread::yield();
    }
}

void    PersistentWorker::addTask(const std::string& name, const Job& jobToExecute)
{
    if (name.empty())
    {
        threadSafeCerr << "addTask: empty task's name" << std::endl;
        return ;
    }
    auto    new_job = std::make_shared<Job>(jobToExecute);
    auto    old_tasks = _tasks.load(std::memory_order_acquire);
    std::shared_ptr<const Tasks>    new_tasks;
    do
    {
        auto    copy = std::make_shared<Tasks>(*old_tasks);
        auto    it = std::find_if(copy->begin(), copy->end(), 
                    [&](const task_t& task) {return task.name == name;});
        if (it != copy->end())
            it->job = new_job;
        else
            copy->push_back(task_t{name, new_job});
        
        new_tasks = std::move(copy);
    }
    while (!_tasks.compare_exchange_weak(old_tasks, new_tasks, 
        std::memory_order_acq_rel, std::memory_order_acquire));
    _tasks.notify_one();

}

void    PersistentWorker::removeTask(const std::string& name)
{
    auto    old_tasks = _tasks.load(std::memory_order_acquire);
    std::shared_ptr<const Tasks>    new_tasks;

    do
    {
        auto    copy = std::make_shared<Tasks>(*old_tasks);
        auto    it = std::find_if(copy->begin(), copy->end(),
                    [&](const task_t& task) {return task.name == name;});
        if (it == copy->end())
        {
            threadSafeCerr << name << " is not found in tasks" << std::endl;
            return ;
        }

        copy->erase(it);
        new_tasks = copy;        
    } while (!_tasks.compare_exchange_weak(old_tasks, new_tasks,
            std::memory_order_release, std::memory_order_acquire));
}
