#pragma once

#include "exivaoptions/exivaoptionsstorage.h"

#include <cip_gen.h>

#include <QString>

#include <memory>
#include <vector>

namespace tibia::protobuf::protocol {
class GameserverMessageExivaSuppressed;
class GameserverMessageUpdateExivaOptions;
}

namespace tibia {
namespace exivaoptions {

/// Fills the exiva options storage from the server's messages and sends the
/// client's exiva options.
class TExivaOptionsProtocolMessageHandler final : public cip::tibia::exivaoptions::TExivaOptionsProtocolMessageHandler
{
    Q_OBJECT
public:
    /// The names that join or leave each whitelist with an options update.
    struct WhitelistChanges
    {
        std::vector<QString> addedCharacters;
        std::vector<QString> removedCharacters;
        std::vector<QString> addedGuilds;
        std::vector<QString> removedGuilds;
    };

    /// The handler keeps the session's exiva options storage.
    explicit TExivaOptionsProtocolMessageHandler(std::shared_ptr<TExivaOptionsStorage> storage);

    /// A suppressed exiva only flashes the chat's exiva button.
    void handleExivaSuppressedMessage(const ::tibia::protobuf::protocol::GameserverMessageExivaSuppressed& message);
    /// The flags replace the stored ones; the listed names join or leave the whitelists.
    void handleUpdateExivaOptionsMessage(const ::tibia::protobuf::protocol::GameserverMessageUpdateExivaOptions& message);

    /// The six flags and the whitelist changes go to the server.
    virtual void sendSetExivaOptions(const TExivaOptionsStorage::Options& options, const WhitelistChanges& changes) const;

private:
    std::shared_ptr<TExivaOptionsStorage> m_storage;
};

}
}
