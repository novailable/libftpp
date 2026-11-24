#include "client.hpp"

Client::Client() {}

Client::~Client()
{
    disconnect();
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
    
    for (addrinfo* info = result; info; info = info->ai_next)
    {
        _fd = ::socket(info->ai_family, info->ai_socktype, info->ai_protocol);
        if (_fd < 0)
            continue ;
        if (::connect(_fd, info->ai_addr, info->ai_addrlen) == 0)
            break ;
        _fd.close();
    }
    ::freeaddrinfo(result);

    if (_fd < 0)
        throw   std::runtime_error("Client: cannot connect to " + address + ":" + std::to_string(port));
    
    _fd.setNBlock();
    _inbox.clear();
}

void    Client::disconnect()
{
    _fd.close();
    _inbox.clear();
}

void    Client::defineAction(const Message::Type& messageType, 
                    const std::function<void(const Message& msg)>& action)
{
    _actions[messageType] = action;
}

bool    Client::sendAll(const std::vector<char>& data)
{
    std::size_t sent = 0;
    while (sent < data.size())
    {
        const ssize_t count = ::send(_fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
        if (count > 0)
        {
            sent += static_cast<std::size_t>(count);
            continue ;
        }
        if (count < 0 && errno == EINTR)
            continue ;
        if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            pollfd  poll_fd{_fd, POLLOUT, 0};
            if (::poll(&poll_fd, 1, 5000) <= 0)
                return false;
            continue ;
        }
        return false;
    }
    return true;
}
 
void    Client::send(const Message& message)
{
    if (_fd < 0)
        throw std::runtime_error("Client: not connected");
    
    if (!sendAll(message.encode()))
    {
        disconnect();
    }
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
        while (auto msg = Message::decode(_inbox))
            received.push_back(std::move(*msg));
    }
    catch (const std::runtime_error&)
    {
        close = true;
    }

    for (const Message& msg : received)
    {
        auto it = _actions.find(msg.type());
        if (it != _actions.end() && it->second)
            it->second(msg);
    }

    if (close)
        disconnect();
}