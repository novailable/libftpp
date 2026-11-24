
#ifndef SERVER_HPP
#define SERVER_HPP

#include <functional>
#include <map>
#include <poll.h>
#include <vector>

#include "thread_safe_iostream.hpp"
#include "message.hpp"
#include "fd.hpp"

class Server
{
    private:
        using Id = long long;
        using Action = std::function<void(Id&, const Message&)>;

        struct Connection
        {
            Id                  id;
            Fd                  fd;
            std::vector<char>   inbox;
            std::vector<char>   outbox;
            bool                alive;
        };

        Fd                              _fd;
        Id                              _currentID = 0;
        std::vector<pollfd>             _poll_fds;
        std::vector<Connection>         _clients;
        std::map<int, Id>               _client_map;
        std::map<Message::Type, Action> _actions;

        Connection* findClient(Id id);

        void read(Connection& con);
        void write(Connection& con);
        void queueSend(Connection& con, const std::vector<char>& data);

        void remove_client(Id id);
        void updatePollEvents(Connection& con);

    public:
        Server();
        ~Server();

        Server(const Server&) = delete;
        Server& operator=(const Server&) = delete;

        void start(const size_t& p_port);
        void stop();

        void defineAction(
            const Message::Type& messageType,
            const Action& action
        );

        void sendTo(const Message& message, Id clientID);
        void sendToArray(const Message& message, std::vector<Id> clientIDs);
        void sendToAll(const Message& message);

        void update();
};

#endif
