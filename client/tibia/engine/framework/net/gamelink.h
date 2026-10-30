#pragma once

#include <cstdint>
#include <string_view>

class Protocol;

/// Socket, XTEA, sequence and checksum live in client/tibia/network.
/// gamecore calls this table; the tibia executable installs it at startup.
namespace gamelink {

struct Hooks
{
    void (*connect)(Protocol* proto, const char* host, uint16_t port, bool gameWorld);
    void (*disconnect)(Protocol* proto);
    bool (*isConnected)(Protocol* proto);
    bool (*isConnecting)(Protocol* proto);
    void (*write)(Protocol* proto, const uint8_t* data, uint16_t size, bool raw);
    void (*generateXtea)(Protocol* proto, uint32_t key[4]);
    void (*setXtea)(Protocol* proto, const uint32_t key[4]);
    void (*enableXtea)(Protocol* proto);
    void (*enableChecksum)(Protocol* proto);
    void (*enableSequenced)(Protocol* proto);
    void (*setRecordFile)(Protocol* proto, const char* file);
    int64_t (*millisSinceRead)(Protocol* proto);
};

void install(const Hooks& hooks);
bool installed();

void connect(Protocol* proto, std::string_view host, uint16_t port, bool gameWorld);
void disconnect(Protocol* proto);
bool isConnected(Protocol* proto);
bool isConnecting(Protocol* proto);
void write(Protocol* proto, const uint8_t* data, uint16_t size, bool raw);
void generateXtea(Protocol* proto, uint32_t key[4]);
void setXtea(Protocol* proto, const uint32_t key[4]);
void enableXtea(Protocol* proto);
void enableChecksum(Protocol* proto);
void enableSequenced(Protocol* proto);
void setRecordFile(Protocol* proto, std::string_view file);
int64_t millisSinceRead(Protocol* proto);

}
