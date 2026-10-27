#include "memento.hpp"

Memento::~Memento() {}

Memento::Snapshot    Memento::save()
{
    Snapshot    state;
    _saveToSnapshot(state);
    return  state;  
}

void    Memento::load(const Snapshot &state)
{
    Snapshot    copy(state);
    copy.rewind();
    _loadFromSnapshot(copy);
}
