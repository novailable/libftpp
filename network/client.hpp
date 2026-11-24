#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <cstddef>
#include <functional>
#include <map>
#include <vector>
#include <string>

#include <cerrno>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>

#include <stdexcept>
#include <string>
#include <utility>

#include "message.hpp"
#include "fd.hpp"


class   Client
{
    private:
        Fd  _fd;
        std::vector<char>   _inbox;
        std::map <Message::Type, std::function<void(const Message& msg)>> _actions;

        bool    sendAll(const std::vector<char>& data);

    public:
        Client();
        ~Client();

        Client(const Client&) = delete;
        Client& operator=(const Client&) = delete;

        void    connect(const std::string& address, const size_t& port);
        void    disconnect();
        void    defineAction(const Message::Type& messageType, 
                            const std::function<void(const Message& msg)>& action);
        void    send(const Message& message);
        void    update();
};

#endif
