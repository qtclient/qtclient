#include "imbuingstorage.h"

#include <tibia/game/tgamenotify.h>

#include <algorithm>

namespace tibia::imbuing {

TImbuingStorage* TImbuingStorage::s_instance = nullptr;

TImbuingStorage::TImbuingStorage(QObject* parent)
    : cip::tibia::imbuing::TImbuingStorage(parent)
{
    s_instance = this;
    // The handler's dead message closes the dialog; so does leaving the game.
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::death, this,
            [this](int, int) { Q_EMIT requestCloseImbuingDialog(); });
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameEnded, this,
            [this]() { Q_EMIT requestCloseImbuingDialog(); });
}

TImbuingStorage::~TImbuingStorage()
{
    if (s_instance == this)
        s_instance = nullptr;
}

TImbuingStorage* TImbuingStorage::instance()
{
    return s_instance;
}

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
