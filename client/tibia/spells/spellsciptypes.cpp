#include "spells/spellsciptypes.h"

#include "protocol/protocolciptypes.h"
#include "spells/spellinfostorage.h"

#include <protocol.pb.h>

namespace tibia::spells {

namespace {

/// The server's spell data modifications, grouped by spell, replace the
/// storage's whole set.
void handleSpellDataModificationMessage(const ::tibia::protobuf::protocol::GameserverMessageSpellDataModification& message)
{
    std::unordered_map<uint16_t, std::vector<TSpellInfoStorage::TSpellModification>> modifications;
    for (const auto& entry : message.entries())
        modifications[static_cast<uint16_t>(entry.spell_id())].push_back({ entry.mod(), entry.value() });
    TSpellInfoStorage::instance()->setSpellModifications(std::move(modifications));
}

} // namespace

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    (void)parent;
}

void TSpellsProtocolMessageHandler::onServerMessageEnqueued(const int opcode)
{
    using namespace ::tibia::protobuf::protocol;
    if (opcode != 96)
        return;
    const auto* shadow = protocol::nativeServerMessage(opcode);
    if (shadow && shadow->HasExtension(GameserverMessageSpellDataModification::gameserver_message_spell_data_modification))
        handleSpellDataModificationMessage(
            shadow->GetExtension(GameserverMessageSpellDataModification::gameserver_message_spell_data_modification));
}

} // namespace tibia::spells
