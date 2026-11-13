#include "thread_safe_iostream.hpp"

#include <cassert>

/// ThreadSafeIOStream::Guard

ThreadSafeIOStream::Guard::Guard(ThreadSafeIOStream& owner) : _owner(&owner) {}

ThreadSafeIOStream::Guard::Guard(Guard&& other) noexcept
	: _owner(other._owner)
{
	other._owner = nullptr;
}

ThreadSafeIOStream::Guard::~Guard()
{
	if (!_owner)
		return ;
	try { _owner->emit(); }
	catch (...) {}
}

ThreadSafeIOStream::Guard&	ThreadSafeIOStream::Guard::operator<<(std::ostream& (*func)(std::ostream&))
{
	func(_owner->_buf);
	return (*this);
}

ThreadSafeIOStream::Guard&	ThreadSafeIOStream::Guard::operator<<(std::ios& (*func)(std::ios&))
{
	func(_owner->_buf);
	return (*this);
}

ThreadSafeIOStream::Guard&	ThreadSafeIOStream::Guard::operator<<(std::ios_base& (*func)(std::ios_base&))
{
	func(_owner->_buf);
	return (*this);
}

/// ThreadSafeIOStream

ThreadSafeIOStream::ThreadSafeIOStream()
{
	setInStream(std::cin);
	setOutStream(std::cout);
}

ThreadSafeIOStream::ThreadSafeIOStream(std::istream &in, std::ostream &out)
{
	setInStream(in);
	setOutStream(out);
}

ThreadSafeIOStream::ThreadSafeIOStream(std::ostream &out)
{
	setOutStream(out);
}

void	ThreadSafeIOStream::emit()
{
	std::string	text = _buf.str();

	_buf.str("");	// drop the text, keep flags (hex, precision, ...)
	_buf.clear();	// reset error bits only
	if (text.empty())
		return ;
	safeWrite(_prefix + text);
}

void	ThreadSafeIOStream::write(const std::string& output)
{
	assert(_out_stream && "output stream is never null after construction");
	if (output.empty())
		return ;
	#ifdef __cpp_lib_syncbuf
		std::osyncstream(*_out_stream) << output << std::flush;
	#else
		_out_stream->write(output.data(), static_cast<std::streamsize>(output.size()));
		_out_stream->flush();
	#endif
}

void	ThreadSafeIOStream::safeWrite(const std::string& output)
{
	std::lock_guard<std::mutex>	lock(*_out_mutex);
	write(output);
}

void	ThreadSafeIOStream::setPrefix(const std::string& prefix)
{
	_prefix = prefix;
}

ThreadSafeIOStream::Guard	ThreadSafeIOStream::operator<<(std::ostream &(*func)(std::ostream &))
{
	Guard	guard(*this);
	guard << func;
	return (guard);
}

ThreadSafeIOStream::Guard	ThreadSafeIOStream::operator<<(std::ios_base &(*func)(std::ios_base &))
{
	Guard	guard(*this);
	guard << func;
	return (guard);
}

ThreadSafeIOStream::Guard	ThreadSafeIOStream::operator<<(std::ios &(*func)(std::ios &))
{
	Guard	guard(*this);
	guard << func;
	return (guard);
}

std::mutex*	ThreadSafeIOStream::streamMutex(std::ios *target, const std::ios *opposite)
{
	if (!target->rdbuf())
		throw std::invalid_argument("ThreadSafeIOStream: stream has no buffer");
	if (opposite && opposite->rdbuf() == target->rdbuf())
		throw std::invalid_argument("ThreadSafeIOStream: streams have same underlying buffer");
	return (&Registery::get(target->rdbuf()));
}

void	ThreadSafeIOStream::setInStream(std::istream &in)
{
	if (&in == _in_stream)
		return ;
	std::mutex	*mutex = streamMutex(&in, _out_stream);	// may throw: nothing changed yet
	_in_stream = &in;
	_in_mutex = mutex;
}

void	ThreadSafeIOStream::setOutStream(std::ostream &out)
{
	if (&out == _out_stream)
		return ;
	std::mutex	*mutex = streamMutex(&out, _in_stream);
	_out_stream = &out;
	_out_mutex = mutex;
}
