
#ifndef FD_HPP
#define FD_HPP

class Fd
{
    private:
        int _fd;

    public:
        explicit Fd(int fd = -1);

        Fd(const Fd&) = delete;
        Fd& operator=(const Fd&) = delete;

        Fd(Fd&& other) noexcept;
        Fd& operator=(Fd&& other) noexcept;

        ~Fd();

        Fd& operator=(int fd);

        int get() const;

        operator int() const;

        void close();
        void setNBlock();
};

#endif
