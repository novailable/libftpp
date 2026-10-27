
#ifndef OBSERVER_HPP
#define OBSERVER_HPP

#include <map>
#include <vector>
#include <functional>

template <typename TEvent>
class Observer
{
    private:
        std::map<TEvent, std::vector<std::function<void()>>> _observer;

    public:
        void subscribe(const TEvent& event, const std::function<void()>& callback)
        {
            _observer[event].push_back(callback);
        }

        void notify(const TEvent& event)
        {
            auto it = _observer.find(event);

            if (it == _observer.end())
                return;

            for (const auto& callback : it->second)
                callback();
        }
};

#endif
