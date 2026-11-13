#include "worker_pool.hpp"

WorkerPool::WorkerPool(std::size_t threadCount)
{
	threadCount = threadCount == 0 ? 1 : threadCount;
	try
	{
		_workers.reserve(threadCount);
		for (std::size_t i = 0; i < threadCount; ++i)
		{
			_workers.emplace_back([this]{addWorker();});
		}
	}
	catch (...)
	{
		shutdown();
		throw;
	}
}

WorkerPool::~WorkerPool()
{
	shutdown();
}

void	WorkerPool::addWorker()
{
	while (1)
	{
		state_type	state = _state.load(std::memory_order_acquire);
		while (1)
		{
			state_type	job_count = (state & ~_stopbit);
			if (job_count > 0)
			{
				if (_state.compare_exchange_weak(state, state - 1,
					std::memory_order_acquire, std::memory_order_acquire))
					break ;
				continue;
			}
			if (state & _stopbit)
				return ;
			
			_state.wait(state, std::memory_order_acquire);
			state = _state.load(std::memory_order_acquire);
		}
		Job job = _jobs.pop_front();
		try
		{
			job();
		}
		catch(const std::exception& e)
		{
			threadSafeCerr << "WorkerPool: job: " << e.what() << std::endl;
		}
		catch(...)
		{
			threadSafeCerr << "WorkerPool: job: thrown an exception" << std::endl; 
		}
	}
}

void	WorkerPool::shutdown()
{
	_state.fetch_or(_stopbit, std::memory_order_release);
	_state.notify_all();
	for (auto& worker : _workers)
		if (worker.joinable())
			worker.join();
}

void	WorkerPool::addJob(const Job& jobToExecute)
{
	if (!jobToExecute)
		throw	std::invalid_argument("WorkerPool::addJob: empty job");
	if (_state.load(std::memory_order_relaxed) & _stopbit)
		throw	std::runtime_error("WorkerPool::addJob: pool is shutting down");
	try
	{
		_jobs.push_back(jobToExecute);
		_state.fetch_add(1, std::memory_order_release);
		_state.notify_one();
	}
	catch(...)
	{
		shutdown();
		throw;
	}
	
}

void	WorkerPool::addJob(std::shared_ptr<WorkerPool::IJobs> jobToExecute)
{
	if (!jobToExecute)
		throw	std::invalid_argument("WorkerPool::addJob: empty job");
	addJob(Job([job = std::move(jobToExecute)]{job->execute();}));
}

std::size_t	WorkerPool::threadCount() const noexcept
{
	return _workers.size();
}