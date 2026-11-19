#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>

#include <functional>
#include <map>
#include <cstddef>
#include <vector>
#include <string>

#include "message.hpp"


class   Client
{
    private:
        int _fd;
        std::vector<char>   _inbox;
        std::map <Message::Type, std::function<void(const Message& msg)>> _actions;
        
    public:
        Client();
        ~Client();
        void    connect(const std::string& address, const size_t& port);
        void    disconnect();
        void    defineAction(const Message::Type& messageType, 
                            const std::function<void(const Message& msg)>& action);
        void    send(const Message& message);
        void    update();
};

#endif
