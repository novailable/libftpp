#ifndef	WORKER_POOL_HPP
#define WORKER_POOL_HPP

#include <functional>
#include <cstddef>
#include <thread>
#include <memory>
#include <vector>

#include "thread_safe_queue.hpp"
#include "thread_safe_iostream.hpp"

class	WorkerPool
{
	private:
		using	state_type = uint32_t;
		using	Job = std::function<void()>;

		std::vector<std::thread>	_workers;
		ThreadSafeQueue<Job>	_jobs;
		
		static constexpr state_type	_stopbit = 1u << 31;	
		std::atomic<state_type>	_state{0};

		void	shutdown();
		void	addWorker();

	public:
		class	IJob
		{
			public:
				virtual ~IJob() = default;
				virtual void	execute() = 0;
		};
	
		WorkerPool(std::size_t threadCount = std::thread::hardware_concurrency());
		~WorkerPool();

		WorkerPool(const WorkerPool&) = delete;
		WorkerPool(WorkerPool&&) = delete;
		WorkerPool&	operator=(const WorkerPool&) = delete;
		WorkerPool&	operator=(WorkerPool&&) = delete;
	
		void	addJob(const Job& jobToExecute);
		void	addJob(std::shared_ptr<IJob>& jobToExecute);

		std::size_t	threadCount() const noexcept;
};

#endif

