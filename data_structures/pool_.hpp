#pragma once

template<typename TType>
class Pool
{

    private:
        Object  _block;

    public:
        class Object
        {
            public:
                TType*  operator->() const
                {
                    return 
                }
                
        };

        void    resize(const size_t& numberOfObjectStored)
        {
            
        }
        template<typename... TArgs>
        Object  acquire(TArgs&& p_args)
        {

        }
};
