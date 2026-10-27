#ifndef MEMENTO_HPP
#define MEMENTO_HPP

#include "data_buffer.hpp"

class   Memento
{
    public:
        using Snapshot = DataBuffer;
        virtual ~Memento();
        Snapshot    save();
        void    load(const Memento::Snapshot &state);

    private:
        virtual void    _saveToSnapshot(Snapshot &snapshot) const = 0;
        virtual void    _loadFromSnapshot(Snapshot &snapshot) = 0;
};

#endif
