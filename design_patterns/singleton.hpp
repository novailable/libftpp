#ifndef SINGLETON_HPP
#define SINGLETON_HPP

#include <memory>
#include <stdexcept>

template<typename TType>
class   Singleton
{
    private:
        static std::unique_ptr<TType>   _instance;

        Singleton() = delete;
        Singleton(const Singleton&) = delete;
        Singleton   &operator=(const Singleton&) = delete;

    public:
        
        static TType*  instance()
        {
            if (!_instance)
                throw   std::runtime_error("Singleton: instance is not instantiate yet");
            return (_instance.get());
        }

        template<typename... TArgs>
        void    instantiate(TArgs&&... p_args)
        {
            if (instance)
                throw   std::runtime_error("Singleton: instance is already instantiated");
            _instance.reset(new TType(std::forward<TArgs>(p_args)...));

        }
};

template<typename TType>
std::unique_ptr<TType>  Singleton<TType>::_instance = nullptr;

#endif
