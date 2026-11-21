#include "imbuingstorage.h"

#include <algorithm>

namespace tibia::imbuing {

TImbuingStorage::TImbuingStorage(QObject* parent)
    : cip::tibia::imbuing::TImbuingStorage(parent)
{
}

TImbuingStorage::~TImbuingStorage() = default;

void TImbuingStorage::setImbuingData(const uint32_t mode, const bool scrollAvailable, const uint16_t itemTypeId,
                                     const std::optional<uint8_t> itemTier, std::vector<TImbuementSlot> slots,
                                     std::vector<TImbuement> imbuements, std::vector<TImbuementSource> ownedObjects)
{
    m_mode = mode;
    m_scrollAvailable = scrollAvailable;
    m_itemTypeId = itemTypeId;
    m_itemTier = itemTier;
    m_slots = std::move(slots);
    m_imbuements = std::move(imbuements);
    m_ownedObjects = std::move(ownedObjects);
    Q_EMIT imbuingDataChanged();
}

void TImbuingStorage::addOwned(const uint16_t typeId, const uint32_t count)
{
    const auto it = std::find_if(m_ownedObjects.begin(), m_ownedObjects.end(),
                                 [typeId](const TImbuementSource& owned) { return owned.objectTypeId == typeId; });
    if (it != m_ownedObjects.end())
        it->count += count;
    else
        m_ownedObjects.push_back({ typeId, count, {} });
    Q_EMIT imbuingDataChanged();
}

}
