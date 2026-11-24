#include "spells/spellsciptypes.h"

#include "protocol/protocolciptypes.h"
#include "spells/spellinfostorage.h"

#include <protocol.pb.h>

#include <stdexcept>

namespace tibia::spells {

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    (void)parent;
}

TSpellsProtocolMessageHandler::TSpellsProtocolMessageHandler(TSpellInfoStorage* spellInfoStorage)
    : cip::tibia::spells::TSpellsProtocolMessageHandler(nullptr)
    , m_spellInfoStorage(spellInfoStorage)
{
}

uint32_t spellModificationType(const uint32_t protocolValue)
{
    if (protocolValue > 3)
        throw std::runtime_error("unhandled enum value");
    return protocolValue;
}

void TSpellsProtocolMessageHandler::handleSpellDataModificationMessage(const ::tibia::protobuf::protocol::GameserverMessageSpellDataModification& message)
{
    std::unordered_map<uint16_t, std::vector<TSpellInfoStorage::TSpellModification>> modifications;
    for (const auto& entry : message.entries())
        modifications[static_cast<uint16_t>(entry.spell_id())].push_back({ spellModificationType(entry.mod()), entry.value() });
    m_spellInfoStorage->setSpellModifications(std::move(modifications));
}

void TSpellsProtocolMessageHandler::handleUnlockedSpellsMessage(const ::tibia::protobuf::protocol::GameserverMessageUnlockedSpells& message)
{
    for (const uint32_t spellId : message.spell_ids())
        Q_EMIT receivedUnlockedSpell(static_cast<int>(spellId));
}

} // namespace tibia::spells
