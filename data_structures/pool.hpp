#ifndef POOL_HPP
#define POOL_HPP

#include <cstddef>
#include <new>
#include <stdexcept>
#include <utility>
#include <vector>

/*
** Pool<TType>
**
** Owns raw, pre-allocated memory for TType objects. Objects are constructed
** on acquire() (placement new) and destroyed when the Pool::Object handle
** goes out of scope; the memory itself stays in the pool and is reused.
**
** Rule: a Pool::Object must not outlive the Pool that produced it.
*/
template <typename TType>
class Pool
{
public:
	class Object
	{
		private:
			friend class Pool;

			Object(Pool* p_pool, TType* p_slot) noexcept : _pool(p_pool), _slot(p_slot) {}

			void release() noexcept
			{
				if (_pool != nullptr && _slot != nullptr)
				{
					_slot->~TType();
					_pool->giveBack(_slot);
				}
				_pool = nullptr;
				_slot = nullptr;
			}

			Pool*  _pool;
			TType* _slot;
	
	public:
		Object() noexcept : _pool(nullptr), _slot(nullptr) {}

		Object(const Object&) = delete;
		Object& operator=(const Object&) = delete;

		Object(Object&& p_other) noexcept : _pool(p_other._pool), _slot(p_other._slot)
		{
			p_other._pool = nullptr;
			p_other._slot = nullptr;
		}

		Object& operator=(Object&& p_other) noexcept
		{
			if (this != &p_other)
			{
				release();
				_pool = p_other._pool;
				_slot = p_other._slot;
				p_other._pool = nullptr;
				p_other._slot = nullptr;
			}
			return *this;
		}

		~Object() { release(); }

		TType* operator->() noexcept { return _slot; }
		const TType* operator->() const noexcept { return _slot; }
		TType& operator*() noexcept { return *_slot; }
		const TType& operator*() const noexcept { return *_slot; }

	};

	Pool() = default;
	Pool(const Pool&) = delete;
	Pool& operator=(const Pool&) = delete;

	~Pool()
	{
		for (void* block : _blocks)
			::operator delete(block, std::align_val_t(alignof(TType)));
	}

	/*
	** Makes the pool able to hold numberOfObjectStored objects in total.
	** Growing never moves existing slots, so live Pool::Object stay valid.
	** Asking for less than the current capacity is a no-op.
	*/
	void resize(const size_t& numberOfObjectStored)
	{
		if (numberOfObjectStored <= _capacity)
			return;

		const size_t extra = numberOfObjectStored - _capacity;
		void* block = ::operator new(extra * sizeof(TType), std::align_val_t(alignof(TType)));

		try
		{
			_blocks.push_back(block);
			_free.reserve(_free.size() + extra);
		}
		catch (...)
		{
			::operator delete(block, std::align_val_t(alignof(TType)));
			throw;
		}

		TType* slots = static_cast<TType*>(block);
		for (size_t i = extra; i > 0; --i) // reversed so the lowest address is acquired first
			_free.push_back(slots + (i - 1));
		_capacity = numberOfObjectStored;
	}

	/*
	** Constructs a TType in a free slot with the given arguments and hands
	** it back wrapped in a Pool::Object. Throws if the pool is exhausted.
	*/
	template <typename... TArgs>
	Object acquire(TArgs&&... p_args)
	{
		if (_free.empty())
			throw std::runtime_error("Pool: no free object available");

		TType* slot = _free.back();
		_free.pop_back();

		try
		{
			new (static_cast<void*>(slot)) TType(std::forward<TArgs>(p_args)...);
		}
		catch (...)
		{
			_free.push_back(slot);
			throw;
		}
		return Object(this, slot);
	}

	size_t capacity() const noexcept { return _capacity; }
	size_t available() const noexcept { return _free.size(); }

private:
	void giveBack(TType* p_slot) noexcept { _free.push_back(p_slot); } // capacity reserved in resize()

	std::vector<void*>  _blocks;
	std::vector<TType*> _free;
	size_t              _capacity = 0;
};

#endif