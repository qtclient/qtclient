#include "spells/spellsciptypes.h"

#include "protocol/protocolciptypes.h"
#include "spells/spellinfostorage.h"

#include <protocol.pb.h>

namespace tibia::spells {

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    (void)parent;
}

void TSpellsProtocolMessageHandler::handleSpellDataModificationMessage(const ::tibia::protobuf::protocol::GameserverMessageSpellDataModification& message)
{
    std::unordered_map<uint16_t, std::vector<TSpellInfoStorage::TSpellModification>> modifications;
    for (const auto& entry : message.entries())
        modifications[static_cast<uint16_t>(entry.spell_id())].push_back({ entry.mod(), entry.value() });
    TSpellInfoStorage::instance()->setSpellModifications(std::move(modifications));
}

} // namespace tibia::spells
