#ifndef POOL_HPP
#define POOL_HPP

#include <cstddef>
#include <stdexcept>
#include <vector>
#include <new>
#include <utility>

template <typename TType>
class   Pool
{
    private:
        size_t  _capacity = 0;
        std::vector<void*>  _blocks;
        std::vector<TType*> _free_slots;

        void    restore(TType* slot) noexcept
        {
            _free_slots.push_back(slot);
        }
        
    public:
        class   Object
        {
            friend class Pool;

            private:
                Pool*   _pool = nullptr;
                TType*  _slot = nullptr;

                Object(Pool* pool, TType* slot) noexcept
                    : _pool(pool), _slot(slot) {}

                void    _free() noexcept
                {
                    if (_pool != nullptr && _slot != nullptr)
                    {
                        _slot->~TType();
                        _pool->restore(_slot);
                    }
                    _pool = nullptr;
                    _slot = nullptr;
                }

            public:
                Object() noexcept = default;
                Object(const Object&) = delete;
                Object  &operator=(const Object&) = delete;
            
                Object(Object&& other) noexcept
                    : _pool(other._pool), _slot(other._slot)
                {
                    other._pool = nullptr;
                    other._slot = nullptr;
                }

                Object  &operator=(Object&& other) noexcept 
                {
                    if (this != &other)
                    {
                        _free();
                        _pool = other._pool;
                        _slot = other._slot;
                        other._pool = nullptr;
                        other._slot = nullptr;
                    }
                    return *this;
                }

                ~Object() { _free(); }

                TType*          operator->() noexcept { return _slot; }
                const TType*    operator->() const noexcept { return _slot; }
                TType&          operator*() noexcept { return *_slot; }
                const TType&    operator*() const noexcept { return *_slot; }

        };

        Pool() = default;
        Pool(const Pool&) = delete;
        Pool    &operator=(const Pool&) = delete;

        ~Pool()
        {
            for (void* block : _blocks)
                ::operator delete(block, std::align_val_t(alignof(TType)));
        }

        template<typename... TArgs>
        Object  acquire(TArgs&&... p_args)
        {
            if (_free_slots.empty())
                throw   std::runtime_error("Pool: no free object is available");
            TType*  slot = _free_slots.back();
            _free_slots.pop_back();

            try
            {
                new (static_cast<void*>(slot)) TType(std::forward<TArgs>(p_args)...);
            }
            catch (...)
            {
                _free_slots.push_back(slot);
                throw ;
            }
            return Object(this, slot);
        }

        void resize(const size_t& numberOfObjectStored)
        {
            if (numberOfObjectStored <= _capacity)
                return ;

            const size_t    extra = numberOfObjectStored - _capacity;
            void*   block = ::operator new(extra * sizeof(TType), std::align_val_t(alignof(TType)));

            try
            {
                _blocks.reserve(_blocks.size() + 1);
                _free_slots.reserve(numberOfObjectStored);
            }
            catch (...)
            {
                ::operator delete(block, std::align_val_t(alignof(TType)));
                throw ;
            }

            _blocks.push_back(block);
            TType*  slots = static_cast<TType*>(block);
            for (size_t i = extra; i > 0; --i)
                _free_slots.push_back(slots + (i - 1));
            _capacity = numberOfObjectStored;
        }

        size_t  capacity() const noexcept { return _capacity; }
        size_t  available() const noexcept { return _free_slots.size(); }
};

#endif
