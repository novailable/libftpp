#ifndef THREAD_HPP
#define THREAD_HPP

#include <thread>
#include <functional>
#include <string>

#include "thread_safe_iostream.hpp"

class	Thread
{
	private:
		std::string _name;
		std::thread	_thread;
		std::function<void()>	_func;

	public:
		Thread(const std::string& name, std::function<void()> functToExecute);
		~Thread();

		Thread(const Thread&) = delete;
		Thread	operator=(const Thread&) = delete;
		void	start();
		void	stop();
};

#endif
