#ifndef MUTEX_REGISTERY_HPP
#define MUTEX_REGISTERY_HPP

#include <map>
#include <mutex>

// One mutex per key, created on first use and kept until program exit.
// std::map nodes never move, so the returned reference stays valid.
template <typename Key>
class MutexRegistery
{
	public:
		static std::mutex&	get(const Key& key)
		{
			static std::mutex					guard;
			static std::map<Key, std::mutex>	mutex_map;

			std::lock_guard<std::mutex>	lock(guard);
			return (mutex_map[key]);
		}
};

#endif
