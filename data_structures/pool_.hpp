#pragma once

#include <vector>

template <typename TType>
class Pool
{
    private:
		size_t	_capacity = 0;
		std::vector<*void>	_blocks;
		std::vector<TType>	_frees;

    public:
        class   Object
        {
            private:
				friend class Pool;
				
				O
        }
        void    resize(const size_t& numberOfObjectStored)
        {
			if (numberOfObjectStored <= _capacity)
			{
				_capacity = numberOfObjectStored;
				return ;
			}
	
			int	extra = numberOfObjectStored - _capacity;
			void	*block = ::operator new(extra * sizeof(TType), std::align_val_t(alignof(TType));
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
        
        TType* operator -> ()
}