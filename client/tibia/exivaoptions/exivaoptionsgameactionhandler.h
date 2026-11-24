#pragma once

#include "exivaoptions/exivaoptionsstorage.h"
#include "input/gameactionptr.h"

#include <cip_gen.h>

#include <QString>

#include <memory>

namespace tibia {
namespace input {
class TGameActionUpdateExivaOptions;
}
namespace exivaoptions {

class TExivaOptionsProtocolMessageHandler;

/// The exiva game actions: toggle "allow everyone", update the options from
/// the dialog, add / remove a whitelisted character. Each one sends the
/// stored (or the dialog's) options through the protocol message handler.
class TExivaOptionsGameActionHandler final : public cip::tibia::exivaoptions::TExivaOptionsGameActionHandler
{
    Q_OBJECT
public:
    TExivaOptionsGameActionHandler(std::shared_ptr<TExivaOptionsProtocolMessageHandler> protocolMessageHandler,
                                   std::shared_ptr<TExivaOptionsStorage> storage);

public Q_SLOTS:
    /// The router's game actions (connected to publishGameActionInternal).
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction);

private:
    /// The stored options as the protocol sends them.
    TExivaOptionsStorage::Options storedOptions() const;
    /// (EXIVA_TOGGLE_ALLOW_EVERYONE): the stored options with "allow
    /// everyone" flipped, and a status line about it.
    void toggleAllowEveryone();
    /// (EXIVA_UPADATE_OPTIONS): the action's options with the names that join
    /// or leave each whitelist compared to the stored ones.
    void updateOptions(const input::TGameActionUpdateExivaOptions& action);
    /// (EXIVA_ADD_CHARACTER_TO_WHITELIST): the stored options with `name` as
    /// the only added character.
    void addCharacterToWhitelist(const QString& name);
    /// (EXIVA_REMOVE_CHARACTER_FROM_WHITELIST): the stored options with `name`
    /// as the only removed character.
    void removeCharacterFromWhitelist(const QString& name);

    std::shared_ptr<TExivaOptionsProtocolMessageHandler> m_protocolMessageHandler;
    std::shared_ptr<TExivaOptionsStorage> m_storage;
};

}
}
