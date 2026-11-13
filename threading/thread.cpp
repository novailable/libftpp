#include "thread.hpp"

Thread::Thread(const std::string& name, std::function<void()> functToExecute)
	: _name(name), _func(std::move(functToExecute))
{
	if (!_func)
		throw	std::invalid_argument("Thread: function must not be empty");
}

Thread::~Thread()
{
	try { stop(); } catch (...) {}
}

void	Thread::start()
{
	if (_thread.joinable())
		throw	std::logic_error("Thread: " + _name + " is already started");
	_thread = std::thread([this]() {
		threadSafeCout.setPrefix("[" + _name + "] ");
		threadSafeCerr.setPrefix("[" + _name + "] ");
		try
		{
			_func();
		}
		catch(const std::exception& e)
		{
			threadSafeCerr << "exception: " << e.what() << std::endl;
		}
		catch(...)
		{
			threadSafeCerr << "Thread: unknown error" << std::endl;
		}
	}); 
}

void	Thread::stop()
{
	if (!_thread.joinable())
		return ;
		// throw	std::logic_error("Thread: " + _name + " isn't start yet");
	if (_thread.get_id() == std::this_thread::get_id())
		throw	std::logic_error("Thread: " + _name + " cannot stop itself");
	_thread.join();
}
