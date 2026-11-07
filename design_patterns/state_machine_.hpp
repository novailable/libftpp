#ifndef STATE_MACHINE_HPP
#define STATE_MACHINE_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

// Note: do not add/remove states or actions from inside an action callback.
template<typename TState, typename Hash = std::hash<TState>>
class StateMachine
{
    public:
        using StateId = std::uint32_t;
        using Action  = std::function<void()>;

    private:
        struct Edge
        {
            StateId             to;
            std::vector<Action> actions;
        };

        struct Node
        {
            bool                alive = false;
            std::vector<Action> actions;
            std::vector<Edge>   edges;      // few edges per state: linear scan beats hashing
        };

        std::vector<Node>                           _nodes;     // indexed by StateId, never reused
        std::unordered_map<TState, StateId, Hash>   _ids;
        StateId                                     _current = 0;

        static void _run(const std::vector<Action>& actions)
        {
            for (const auto& act : actions)
                act();
        }

        StateId _key(const TState& state) const
        {
            auto it = _ids.find(state);
            if (it == _ids.end())
                throw std::logic_error("StateMachine: no such state");
            return it->second;
        }

        Node& find_node(StateId id)
        {
            if (id >= _nodes.size() || !_nodes[id].alive)
                throw std::logic_error("StateMachine: no such state");
            return _nodes[id];
        }

        Edge* _findEdge(Node& from, StateId to)
        {
            for (auto& e : from.edges)
                if (e.to == to)
                    return &e;
            return nullptr;
        }

    public:
        // Returns the id of the state (existing id if already added).
        // The first state added becomes the initial state.
        StateId addState(const TState& state)
        {
            auto it = _ids.find(state);
            if (it != _ids.end())
                return it->second;
            StateId id = static_cast<StateId>(_nodes.size());   // monotonic: no id collision after remove()
            _nodes.emplace_back();
            _nodes.back().alive = true;
            _ids.emplace(state, id);
            return id;
        }

        StateId id(const TState& state) const { return _key(state); }
        StateId current() const               { return _current; }

        void addTransition(StateId from, StateId to, Action fn)
        {
            Node& n = find_node(from);
            find_node(to);                                          // validate target
            if (Edge* e = _findEdge(n, to))
                e->actions.push_back(std::move(fn));
            else
                n.edges.push_back(Edge{to, {}}), n.edges.back().actions.push_back(std::move(fn));
        }

        void addTransition(const TState& from, const TState& to, Action fn)
        {
            addTransition(_key(from), _key(to), std::move(fn));
        }

        void addAction(StateId state, Action fn)
        {
            find_node(state).actions.push_back(std::move(fn));
        }

        void addAction(const TState& state, Action fn)
        {
            addAction(_key(state), std::move(fn));
        }

        // Hot-path overload: no hashing.
        void transitionTo(StateId target)
        {
            find_node(target);
            Edge* e = _findEdge(find_node(_current), target);
            if (!e)
                throw std::logic_error("StateMachine: transition not defined");
            _run(e->actions);
            _current = target;
        }

        void transitionTo(const TState& state)
        {
            transitionTo(_key(state));
        }

        // A state with no actions is a no-op.
        void update()
        {
            if (_current >= _nodes.size() || !_nodes[_current].alive)
                throw std::logic_error("StateMachine: no current state");
            _run(_nodes[_current].actions);
        }

        void remove(const TState& state)
        {
            StateId key = _key(state);
            if (key == _current)
                throw std::logic_error("StateMachine: cannot remove the current state");

            for (auto& n : _nodes)
            {
                for (auto it = n.edges.begin(); it != n.edges.end(); )
                    it = (it->to == key) ? n.edges.erase(it) : it + 1;
            }
            Node& dead = _nodes[key];
            dead.alive = false;
            dead.actions.clear();
            dead.actions.shrink_to_fit();
            dead.edges.clear();
            dead.edges.shrink_to_fit();
            _ids.erase(state);
        }
};

#endif