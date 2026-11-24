
#include "fd.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <stdexcept>
#include <utility>

Fd::Fd(int fd) : _fd(fd) {}

Fd::Fd(Fd&& other) noexcept : _fd(other._fd)
{
    other._fd = -1;
}

Fd& Fd::operator=(Fd&& other) noexcept
{
    if (this != &other)
    {
        if (_fd >= 0)
            ::close(_fd);

        _fd = other._fd;
        other._fd = -1;
    }

    return *this;
}

Fd::~Fd()
{
    if (_fd >= 0)
        ::close(_fd);
}

Fd& Fd::operator=(int fd)
{
    if (_fd != fd)
    {
        if (_fd >= 0)
            ::close(_fd);

        _fd = fd;
    }

    return *this;
}

int Fd::get() const
{
    return _fd;
}

Fd::operator int() const
{
    return _fd;
}

void Fd::close()
{
    if (_fd >= 0)
    {
        int fd = _fd;
        _fd = -1;

        if (::close(fd) < 0 && errno != EINTR)
            throw std::runtime_error("Fd: closing failed");
    }
}

void Fd::setNBlock()
{
    if (_fd < 0)
        throw std::runtime_error("Fd: no file descriptor");

    int flags = ::fcntl(_fd, F_GETFL, 0);

    if (flags < 0 ||
        ::fcntl(_fd, F_SETFL, flags | O_NONBLOCK) < 0)
        throw std::runtime_error("Fd: failed to set non-blocking");
}
