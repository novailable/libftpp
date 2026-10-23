#pragma once

#include <vector>
#include <cstddef>
#include <new>
#include <utility>
#include <stdexcept>

template <typename TType>
class Pool
{
    private:
		size_t	_capacity = 0;
		std::vector<*void>	_blocks;
		std::vector<TType>	_frees;

		void	giveBack(TType* slot) noexcept
		{
			return _free.push_back(slot);
		}

    public:
        class   Object
        {
            private:
				friend class Pool;
				
				Pool*	_pool;
				TType*	_slot;
				
				Object(Pool* pool, TType* slot) noexcept : _pool(pool), _slot(slot){}

				void	release() noexcept
				{
					if (_pool != nullptr && _slot != nullptr)
					{
						_slot->~TType();
						_pool->giveBack(_slot);
					}
					_pool = nullptr;
					_slot = nullptr;
				}
			
			public:
				Object() noexcept : _pool(nullptr), _slot(nullptr) {}
				
				Object(const Object&) = delete;
				Object	&operator=(const Object&) = delete;

				Object(Object&& other) noexcept : _pool(other._pool), _slot(other._slot)
				{
					other._pool = nullptr;
					other._slot = nullptr;
				}

				Object	&operator=(Object&& other) const
				{
					if (this != &other)
					{
						release();
						_pool = other._pool;
						_slot = other._slot;
						other._pool = nullptr;
						other._slot = nullptr;
					}
					return *this;
				}

				~Object() { release(); }

			TType*	operator->() noexcept { return _slot;}
			const TType*	operator->() noexcept { return _slot;}
			TType&	operator*() noexcept { return _slot;}
			const TType&	operator->() noexcept { return _slot;}
		};
	
		Pool() = default;
		Pool(const Pool&) = delete;
		Pool	&operator=(const Pool&) = delete;

		~Pool()
		{
			for (void* block : _blocks)
				::operator delete(block, std::align_val_t(alignof(TType)));
		}
        void    resize(const size_t& numberOfObjectStored)
        {
			if (numberOfObjectStored <= _capacity)
			{
				_capacity = numberOfObjectStored;
				return ;
			}
	
			int	extra = numberOfObjectStored - _capacity;
			void	*block = ::operator new(extra * sizeof(TType), std::align_val_t(alignof(TType)));
			try
			{
				_blocks.push_back(block);
				_free.reserve(_free.size() + extra);
			}
			catch(...)
			{
				::operator delete(block, std::align_val_t(alignof(TType)));
				throw;
			}
			
			TType*	slots = static_cast<TType*>(block);
			for (size_t i = extra; i > 0; --i)
				_free.pushback(slots + (i - 1));
			_capacity = numberOfObjectStored;
		}
        
		template<typename... TArgs>
		Object	acquire(TArgs&&... args)
		{
			if (_free.empty())
				throw std::runtime_error("Pool: no free object available");
			
			TType*	slot = _free.back();
			_free.pop_back();

			try
			{
				new (static_cast<void*>(slot)) TType(std::forward<TArgs>(args)...);
			}
			catch (...)
			{
				_free.push_back(slot);
				throw;
			}
			return Object(this, slot);
		}
		
		size_t	capacity() const noexcept { return _capacity; }
		size_t	available() const noexcept { return _free.size(); }
};
