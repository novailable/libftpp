#include "client.hpp"

Client::Client() : _fd(-1) 
{}

Client::~Client()
{

}

void    Client::connect(const std::string& address, const size_t& port)
{
    addrinfo    hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo*   result = nullptr;
    const int status = ::getaddrinfo(address.c_str(), std::to_string(port).c_str(), &hints, &result);

    if (status != 0)
        throw   std::runtime_error("Client: cannot resolve " + address + ": " + ::gai_strerror(status));
    
    int fd = -1;
    for (addrinfo* info = result; info; info = info->ai_next)
    {
        fd = ::socket(info->ai_family, info->ai_socktype, info->ai_protocol);
        if (fd < 0)
            continue ;
        if (::connect(fd, info->ai_addr, info->ai_addrlen) == 0)
            break ;
        ::close(fd);
        fd = -1;
    }
    ::freeaddrinfo(result);

    if (fd < 0)
        throw   std::runtime_error("Client: cannot connect to " + address + ":" + std::to_string(port));
    
    ::fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
    _fd = fd;
}

void    Client::disconnect()
{

}

void    Client::defineAction(const Message::Type& messageType, 
                    const std::function<void(const Message& msg)>& action)
{
    _actions[messageType] = action;
}

void    Client::send(const Message& message)
{

}

void    Client::update()
{
    if (_fd < 0)
        return ;
    
    bool    close = false;
    char    buf[4096];
    for (;;)
    {
        const ssize_t   count = ::recv(_fd, buf, sizeof(buf), 0);
        if (count > 0)
        {
            _inbox.insert(_inbox.end(), buf, buf + count);
            continue;
        }
        if (count == 0)
        {
            close = true;
            break;
        }
        if (errno == EINTR)
            continue ;
        if (errno != EAGAIN && errno != EWOULDBLOCK)
            close = true;
        break;
    }

    std::vector<Message>    received;
    try
    {
        while ()
    }
}