
#include "server.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

// --------------------------------------------------
// Constructor / Destructor
// --------------------------------------------------

Server::Server() : _fd(-1) {}

Server::~Server()
{
    try
    {
        stop();
    }
    catch (...)
    {
        // Destructors must not throw.
    }
}

// --------------------------------------------------
// Client lookup
// --------------------------------------------------

Server::Connection* Server::findClient(Id id)
{
    auto it = std::find_if(
        _clients.begin(),
        _clients.end(),
        [id](const Connection& con)
        {
            return con.id == id;
        }
    );

    if (it == _clients.end())
        return nullptr;

    return &(*it);
}

// --------------------------------------------------
// Start / Stop
// --------------------------------------------------

void Server::start(const size_t& p_port)
{
    stop();

    if (p_port > 65535)
        throw std::runtime_error("Server: invalid port");

    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);

    if (fd < 0)
        throw std::runtime_error("Server: socket creation failed");

    _fd = fd;

    int enable = 1;
    ::setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR,
                 &enable, sizeof(enable));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<uint16_t>(p_port));

    if (::bind(_fd, reinterpret_cast<sockaddr*>(&addr),
               sizeof(addr)) < 0 ||
        ::listen(_fd, SOMAXCONN) < 0)
    {
        const std::string error_msg =
            "Server: cannot listen on port " +
            std::to_string(p_port) + ": " + ::strerror(errno);

        try
        {
            _fd.close();
        }
        catch (...)
        {}

        throw std::runtime_error(error_msg);
    }

    _fd.setNBlock();

    _poll_fds.push_back({_fd, POLLIN, 0});
}

void Server::stop()
{
    // Destroying Connection objects closes their Fd objects.
    _clients.clear();

    _client_map.clear();
    _poll_fds.clear();

    if (_fd >= 0)
    {
        try
        {
            _fd.close();
        }
        catch (...)
        {}
    }
}

// --------------------------------------------------
// Actions
// --------------------------------------------------

void Server::defineAction(
    const Message::Type& messageType,
    const Action& action)
{
    _actions[messageType] = action;
}

// --------------------------------------------------
// Poll event management
// --------------------------------------------------

void Server::updatePollEvents(Connection& con)
{
    auto it = std::find_if(
        _poll_fds.begin() + 1,
        _poll_fds.end(),
        [&con](const pollfd& pfd)
        {
            return pfd.fd == static_cast<int>(con.fd);
        }
    );

    if (it == _poll_fds.end())
        return;

    it->events = POLLIN;

    if (!con.outbox.empty())
        it->events |= POLLOUT;
}

// --------------------------------------------------
// Sending
// --------------------------------------------------

void Server::queueSend(
    Connection& con,
    const std::vector<char>& data)
{
    if (!con.alive || data.empty())
        return;

    con.outbox.insert(
        con.outbox.end(),
        data.begin(),
        data.end()
    );

    updatePollEvents(con);
}

void Server::sendTo(const Message& message, Id clientID)
{
    Connection* con = findClient(clientID);

    if (!con || !con->alive)
        return;

    queueSend(*con, message.encode());
}

void Server::sendToArray(
    const Message& message,
    std::vector<Id> clientIDs)
{
    const std::vector<char> data = message.encode();

    for (Id id : clientIDs)
    {
        Connection* con = findClient(id);

        if (con && con->alive)
            queueSend(*con, data);
    }
}

void Server::sendToAll(const Message& message)
{
    const std::vector<char> data = message.encode();

    for (auto& con : _clients)
    {
        if (con.alive)
            queueSend(con, data);
    }
}

// --------------------------------------------------
// Reading
// --------------------------------------------------

void Server::read(Connection& con)
{
    char buf[4096];

    while (true)
    {
        const ssize_t count = ::recv(
            con.fd,
            buf,
            sizeof(buf),
            0
        );

        if (count > 0)
        {
            con.inbox.insert(
                con.inbox.end(),
                buf,
                buf + count
            );

            continue;
        }

        if (count == 0)
        {
            con.alive = false;
            break;
        }

        if (errno == EINTR)
            continue;

        if (errno == EAGAIN || errno == EWOULDBLOCK)
            break;

        con.alive = false;
        break;
    }
}

// --------------------------------------------------
// Writing
// --------------------------------------------------

void Server::write(Connection& con)
{
    while (!con.outbox.empty())
    {
        const ssize_t count = ::send(
            con.fd,
            con.outbox.data(),
            con.outbox.size(),
            MSG_NOSIGNAL
        );

        if (count > 0)
        {
            con.outbox.erase(
                con.outbox.begin(),
                con.outbox.begin() + count
            );

            continue;
        }

        if (count < 0 && errno == EINTR)
            continue;

        if (count < 0 &&
            (errno == EAGAIN || errno == EWOULDBLOCK))
            break;

        con.alive = false;
        break;
    }

    if (con.alive)
        updatePollEvents(con);
}

// --------------------------------------------------
// Remove client
// --------------------------------------------------

void Server::remove_client(Id id)
{
    auto it = std::find_if(
        _clients.begin(),
        _clients.end(),
        [id](const Connection& con)
        {
            return con.id == id;
        }
    );

    if (it == _clients.end())
        return;

    const int fd = it->fd;

    _client_map.erase(fd);

    _poll_fds.erase(
        std::remove_if(
            _poll_fds.begin() + 1,
            _poll_fds.end(),
            [fd](const pollfd& pfd)
            {
                return pfd.fd == fd;
            }
        ),
        _poll_fds.end()
    );

    // Erasing the Connection destroys its Fd,
    // which closes the socket automatically.
    _clients.erase(it);
}

// --------------------------------------------------
// Update
// --------------------------------------------------

void Server::update()
{
    if (_fd < 0)
        return;

    const int result = ::poll(
        _poll_fds.data(),
        static_cast<nfds_t>(_poll_fds.size()),
        1000
    );

    if (result < 0)
    {
        if (errno == EINTR)
            return;

        throw std::runtime_error("Server: poll failed");
    }

    if (result == 0)
        return;

    std::vector<std::pair<Id, Message>> received;
    std::vector<Id> dead_clients;

    // Number of descriptors that were actually monitored
    // by this poll() call. Newly accepted clients are excluded.
    const std::size_t polled_count = _poll_fds.size();

    // ----------------------------------------------
    // 1. Accept new clients
    // ----------------------------------------------

    if (_poll_fds[0].revents & POLLIN)
    {
        while (true)
        {
            const int fd = ::accept(_fd, nullptr, nullptr);

            if (fd < 0)
            {
                if (errno == EINTR)
                    continue;

                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;

                break;
            }

            Connection con{
                ++_currentID,
                Fd(fd),
                {},
                {},
                true
            };

            con.fd.setNBlock();

            const Id id = con.id;

            _client_map[fd] = id;
            _clients.push_back(std::move(con));

            _poll_fds.push_back({fd, POLLIN, 0});
        }
    }

    // ----------------------------------------------
    // 2. Process ready clients
    // ----------------------------------------------

    for (std::size_t i = 1; i < polled_count; ++i)
    {
        const pollfd poll_obj = _poll_fds[i];

        if (poll_obj.revents == 0)
            continue;

        auto map_it = _client_map.find(poll_obj.fd);

        if (map_it == _client_map.end())
            continue;

        const Id id = map_it->second;

        Connection* con = findClient(id);

        if (!con || !con->alive)
            continue;

        // Read incoming data.
        if (poll_obj.revents & POLLIN)
            read(*con);

        // Write pending outgoing data.
        if (con->alive && (poll_obj.revents & POLLOUT))
            write(*con);

        // Handle socket errors and disconnections.
        if (poll_obj.revents & (POLLERR | POLLHUP | POLLNVAL))
            con->alive = false;

        // Decode complete messages.
        try
        {
            while (auto msg = Message::decode(con->inbox))
            {
                received.emplace_back(
                    con->id,
                    std::move(*msg)
                );
            }
        }
        catch (const std::runtime_error&)
        {
            con->alive = false;
        }

        if (!con->alive)
            dead_clients.push_back(id);
    }

    // ----------------------------------------------
    // 3. Remove disconnected clients
    // ----------------------------------------------

    for (Id id : dead_clients)
        remove_client(id);

    // ----------------------------------------------
    // 4. Dispatch received messages
    // ----------------------------------------------

    for (auto& [id, msg] : received)
    {
        auto it = _actions.find(msg.type());

        if (it != _actions.end() && it->second)
        {
            Id clientID = id;
            it->second(clientID, msg);
        }
    }
}
