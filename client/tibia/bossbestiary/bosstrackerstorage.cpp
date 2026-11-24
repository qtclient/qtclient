#include "bossbestiary/bosstrackerstorage.h"

#include "qmlcomponents/qmlcomponentsciptypes.h"

#include <client/outfit.h>

#include <QDateTime>
#include <QQmlEngine>

#include <algorithm>

namespace tibia::bossbestiary {

TBossTrackerEntry::TBossTrackerEntry(const int raceId, QString name, const Outfit& outfit, const qint64 cooldownUntil)
    : raceId(raceId)
    , name(std::move(name))
    , cooldownUntil(cooldownUntil)
{
    // An object outfit shows the object's appearance.
    const bool objectOutfit = outfit.getId() == 0 && outfit.getAuxId() != 0;
    const auto outfitId = static_cast<uint16_t>(objectOutfit ? outfit.getAuxId() : outfit.getId());
    auto* model = new qmlcomponents::TOutfitQmlModel(outfitId, outfit.getHead(), outfit.getBody(), outfit.getLegs(), outfit.getFeet(),
                                                     (outfit.getAddons() & 1) != 0, (outfit.getAddons() & 2) != 0, objectOutfit, nullptr);
    QQmlEngine::setObjectOwnership(model, QQmlEngine::CppOwnership);
    this->outfit.reset(model);
}

TBossTrackerEntry::TBossTrackerEntry(const TBossTrackerEntry& other)
    : raceId(other.raceId)
    , name(other.name)
    , cooldownUntil(other.cooldownUntil)
{
    auto* model = new qmlcomponents::TOutfitQmlModel(nullptr);
    QQmlEngine::setObjectOwnership(model, QQmlEngine::CppOwnership);
    outfit.reset(model);
    if (other.outfit)
        model->copyFrom(*other.outfit);
}

TBossTrackerEntry& TBossTrackerEntry::operator=(const TBossTrackerEntry& other)
{
    raceId = other.raceId;
    name = other.name;
    if (!outfit) {
        auto* model = new qmlcomponents::TOutfitQmlModel(nullptr);
        QQmlEngine::setObjectOwnership(model, QQmlEngine::CppOwnership);
        outfit.reset(model);
    }
    if (other.outfit && other.outfit != outfit)
        outfit->copyFrom(*other.outfit);
    cooldownUntil = other.cooldownUntil;
    return *this;
}

bool TBossTrackerEntry::hasCooldown() const { return cooldownUntil > 0; }

bool TBossTrackerEntry::cooldownExpired() const
{
    return hasCooldown() && cooldownUntil <= QDateTime::currentSecsSinceEpoch();
}

bool TBossTrackerEntry::operator==(const TBossTrackerEntry& other) const
{
    return raceId == other.raceId && name == other.name && outfit == other.outfit && cooldownUntil == other.cooldownUntil;
}

bool TBossTrackerEntry::operator!=(const TBossTrackerEntry& other) const
{
    return !(*this == other);
}

TBossTrackerStorage::TBossTrackerStorage(QObject* parent)
    : cip::tibia::bossbestiary::TBossTrackerStorage(parent)
{
}

void TBossTrackerStorage::setEntries(std::vector<TBossTrackerEntry> entries)
{
    if (entries == m_entries)
        return;
    m_entries = std::move(entries);
    Q_EMIT bossEntriesChanged();
}

qint64 TBossTrackerStorage::cooldownUntil(const int raceId) const
{
    const auto it = std::ranges::find(m_entries, raceId, &TBossTrackerEntry::raceId);
    return it == m_entries.end() ? 0 : it->cooldownUntil;
}

} // namespace tibia::bossbestiary
