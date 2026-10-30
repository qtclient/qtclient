#pragma once

#include "declarations.h"
#include <framework/luaengine/luaobject.h>

#include <memory>

// The asio accept loop left with Connection. Nothing in the tibia client opens
// a listening socket.
class Server final : public LuaObject
{
public:
    explicit Server(int) {}
    static std::shared_ptr<Server> create(int) { return nullptr; }
    bool isOpen() const { return false; }
    void close() {}
    void acceptNext() {}
};
