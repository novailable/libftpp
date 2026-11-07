#ifndef STATE_MACHINE_HPP
#define STATE_MACHINE_HPP

#include <vector>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <stdexcept>
#include <functional>
#include <unordered_map>

template<typename TState, typename Hash = std::hash<TState>>
class   StateMachine
{
    public:
        using state_id = std::uint32_t;
        using action = std::function<void()>;

    private:

        struct  Transition
        {
            state_id            to;
            std::vector<action> actions;
        };

        struct  Node
        {
            bool                    alive = false;
            std::vector<action>     actions;
            std::vector<Transition> transitions;
        };

        std::vector<Node>   _nodes;
        std::unordered_map<TState, state_id, Hash>    _states;
        state_id    _current = 0;

        static void    _doAction(const std::vector<action> &actions)
        {
            for (const auto& act : actions)
                act();
        }

        state_id  _key(const TState& state) const
        {
            auto    it = _states.find(state);
            if (it == _states.end() || !_nodes[it->second].alive)
                throw   std::invalid_argument("StateMachine: no such state");
            return  it->second;
        }

        Node&   find_node(state_id id, bool alive = true)
        {
            if (id >= _nodes.size() || (alive && !_nodes[id].alive))
                throw   std::invalid_argument("StateMachine: no such state");
            return  _nodes[id];
        }

        Transition* _find(Node& from, state_id to)
        {
            for (auto& transit : from.transitions)
                if (transit.to == to)
                    return &transit;
            return nullptr;
        }

    public:
        void    addState(const TState &state)
        {
            auto    it = _states.find(state);
            if (it != _states.end())
			{
				Node&	nod = find_node(it->second, false);
				nod.alive = true;
				return ;
			}
            state_id    id = static_cast<state_id>(_nodes.size());
            _nodes.emplace_back();
            _nodes.back().alive = true;
            _states.emplace(state, id);
        }

        state_id    id(const TState& state) const { return _key(state); }
        state_id    current() const { return _current; }

        void    addTransition(const TState &startState, const TState &finalState,
                            action lambda)
        {
            state_id    from_id = _key(startState);
            state_id    to_id = _key(finalState);
            Node&       nod = find_node(from_id);
            find_node(to_id);
            if (Transition* transit = _find(nod, to_id))
                transit->actions.push_back(std::move(lambda));
            else
            {
                nod.transitions.push_back(Transition{to_id, {}});
                nod.transitions.back().actions.push_back(std::move(lambda));
            }
        }

        void    addAction(const TState &state, action lambda)
        {
            find_node(_key(state)).actions.push_back(std::move(lambda));
        }

        void    transitionTo(const TState &state)
        {
            state_id    target_id = _key(state);
            find_node(target_id);
            Transition* transit = _find(find_node(_current), target_id);
            if (!transit)
                throw   std::invalid_argument("StateMachine: transition not defined");
            _doAction(transit->actions);
            _current = target_id;
        }

        void    update()
        {
            if (_current >= _nodes.size() || !_nodes[_current].alive)
                throw   std::invalid_argument("StateMachine: no current state");
            _doAction(_nodes[_current].actions);
        }

        void    remove(const TState &state)
        {
            state_id	target_id = id(state);
            if (target_id == _current)
                throw   std::invalid_argument("StateMachine: cannot remove current state");

            for (auto& nod : _nodes)
            {
                for (auto it = nod.transitions.begin(); it != nod.transitions.end(); )
                    it = (it->to == target_id) ? nod.transitions.erase(it) : it + 1;
            }
            Node&   dead = _nodes[target_id];
            dead.alive = false;
            dead.actions.clear();
            dead.actions.shrink_to_fit();
            dead.transitions.clear();
            dead.transitions.shrink_to_fit();
        }
};

#endif
