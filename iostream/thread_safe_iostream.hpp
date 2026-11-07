#ifndef THREAD_SAFE_IOSTREAM_HPP
#define THREAD_SAFE_IOSTREAM_HPP

#include <iostream>
#include <sstream>
#include <mutex>
#include <string>
#include <limits>
#include <stdexcept>
#include <ios>
#include <streambuf>
#include <type_traits>
#include <version>

#if __has_include(<syncstream>)
	#include <syncstream>
#endif

#include "mutex_registery.hpp"

// Rules:
//  - one ThreadSafeIOStream per thread (_buf is not locked). threadSafeCout is thread_local.
//  - never use the same wrapper inside the arguments of its own << chain.
//  - streams must outlive the wrapper.
//  - lock order is always: input mutex, then output mutex.
class ThreadSafeIOStream
{
	private:
		using	Registery = MutexRegistery<const std::streambuf*>;

		std::istream*	_in_stream	= nullptr;
		std::ostream*	_out_stream	= nullptr;
		std::mutex*		_in_mutex	= nullptr;
		std::mutex*		_out_mutex	= nullptr;

		std::string			_prefix;
		std::ostringstream	_buf;		// text + persistent format flags

		void				emit();
		void				write(const std::string& output);		// caller holds _out_mutex
		void				safeWrite(const std::string& output);	// locks _out_mutex
		static std::mutex*	streamMutex(std::ios *target, const std::ios *opposite);

	public:
		class	Guard
		{
			private:
				ThreadSafeIOStream*	_owner;

			public:
				explicit Guard(ThreadSafeIOStream& owner);
				Guard(Guard&& other) noexcept;
				~Guard();

				Guard(const Guard&) = delete;
				Guard&	operator=(const Guard&) = delete;
				Guard&	operator=(Guard&&) = delete;

				template <typename T>
				Guard&	operator<<(const T& value)
				{
					_owner->_buf << value;
					return (*this);
				}

				Guard&	operator<<(std::ostream& (*func)(std::ostream&));
				Guard&	operator<<(std::ios& (*func)(std::ios&));
				Guard&	operator<<(std::ios_base& (*func)(std::ios_base&));
		};

		explicit ThreadSafeIOStream(std::istream &in = std::cin,
									std::ostream &out = std::cout);
		~ThreadSafeIOStream() = default;

		ThreadSafeIOStream(const ThreadSafeIOStream &) = delete;
		ThreadSafeIOStream&	operator=(const ThreadSafeIOStream &) = delete;

		void	setPrefix(const std::string &prefix);
		void	setInStream(std::istream &in);
		void	setOutStream(std::ostream &out);

		Guard	operator<<(std::ostream &(*func)(std::ostream &));
		Guard	operator<<(std::ios_base &(*func)(std::ios_base &));
		Guard	operator<<(std::ios &(*func)(std::ios &));

		template <typename T>
		Guard	operator<<(const T &value)
		{
			Guard	guard(*this);
			guard << value;
			return (guard);
		}

		// in, then out: reading can flush a tied output stream (cin.tie() == &cout)
		template <typename T>
		ThreadSafeIOStream&	operator>>(T &value)
		{
			std::lock_guard<std::mutex>	inlock(*_in_mutex);
			std::lock_guard<std::mutex>	outlock(*_out_mutex);
			*_in_stream >> value;
			return (*this);
		}

		// Throws std::ios_base::failure on EOF, std::invalid_argument on bad input.
		template <typename T>
		void	prompt(const std::string &question, T &dest)
		{
			std::lock_guard<std::mutex>	inlock(*_in_mutex);
			std::lock_guard<std::mutex>	outlock(*_out_mutex);

			write(_prefix + question);

			if constexpr (std::is_same_v<T, std::string>)
			{
				if (!std::getline(*_in_stream, dest))
					throw std::ios_base::failure("input closed (EOF)");
				if (!dest.empty() && dest.back() == '\r')
					dest.pop_back();
			}
			else
			{
				*_in_stream >> dest;

				if (_in_stream->eof() && _in_stream->fail())
					throw std::ios_base::failure("input closed (EOF)");

				if (_in_stream->fail())
				{
					_in_stream->clear();
					_in_stream->ignore(std::numeric_limits<std::streamsize>::max(), '\n');
					throw std::invalid_argument("invalid input");
				}

				if (_in_stream->good())
					_in_stream->ignore(std::numeric_limits<std::streamsize>::max(), '\n');
			}
		}
};

inline thread_local	ThreadSafeIOStream threadSafeCout;

#endif