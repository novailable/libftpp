#ifndef THREAD_SAFE_QUEUE_HPP
#define THREAD_SAFE_QUEUE_HPP

#include <cstddef>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <utility>

template <typename TType>
class	ThreadSafeQueue
{
	private:
		std::deque<TType>	_container;
		mutable	std::mutex			_mutex;

	public:
		ThreadSafeQueue()	= default;
		ThreadSafeQueue(const ThreadSafeQueue&) = delete;
		ThreadSafeQueue	&operator=(const ThreadSafeQueue&) = delete;
		~ThreadSafeQueue() {};

		void	push_back(const TType& newElement)
		{
			TType	copy(newElement);
			std::lock_guard<std::mutex>	lock(_mutex);
			_container.push_back(std::move(copy));
		}

		void	push_back(const TType&& newElement)
		{
			std::lock_guard<std::mutex>	lock(_mutex);
			_container.push_back(std::move(newElement));
		}
		
		void	push_front(const TType& newElement)
		{
			TType	copy(newElement);
			std::lock_guard<std::mutex>	lock(_mutex);
			_container.push_front(std::move(copy));
		}

		void	push_front(const TType&& newElement)
		{
			std::lock_guard<std::mutex>	lock(_mutex);
			_container.push_front(std::move(newElement));
		}

		TType	pop_back()
		{
			std::lock_guard<std::mutex>	lock(_mutex);
			if (_container.empty())
				throw	std::out_of_range("ThreadSafeQueue::pop_back: queue is empty");
			TType	data = std::move(_container.back());
			_container.pop_back();
			return	data;
		}

		TType	pop_front()
		{
			std::lock_guard<std::mutex>	lock(_mutex);
			if (_container.empty())
				throw	std::out_of_range("ThreadSafeQueue::pop_front: queue is empty");
			TType	data = std::move(_container.front());
			_container.pop_front();
			return	data;
		}

		size_t	size() const noexcept
		{
			std::lock_guard<std::mutex>	lock(_mutex);
			return	_container.size();
		}

		bool	empty() const noexcept
		{
			return size() == 0;
		}
};

#endif
