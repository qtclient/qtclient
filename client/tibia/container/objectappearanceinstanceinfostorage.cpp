#include "container/objectappearanceinstanceinfostorage.h"

namespace tibia::container {

bool TObjectAppearanceInstanceInfoStorage::TCompare::operator()(const TObjectAppearanceInstanceTypeID& left,
                                                                const TObjectAppearanceInstanceTypeID& right) const
{
    if (left.typeId != right.typeId)
        return left.typeId < right.typeId;
    if (left.liquid != right.liquid)
        return static_cast<int32_t>(left.liquid) < static_cast<int32_t>(right.liquid);
    return left.tier < right.tier;
}

TObjectAppearanceInstanceInfoStorage::TObjectAppearanceInstanceInfoStorage(QObject* parent)
    : cip::tibia::container::TObjectAppearanceInstanceInfoStorage(parent)
{
    // A zero-interval single shot: any number of changes in one turn of the
    // event loop are announced once.
    m_emitTimer.setInterval(0);
    m_emitTimer.setSingleShot(true);
    connect(&m_emitTimer, &QTimer::timeout, this, &TObjectAppearanceInstanceInfoStorage::onAsyncEmitObjectInfosChanged);
}

void TObjectAppearanceInstanceInfoStorage::onAsyncEmitObjectInfosChanged()
{
    Q_EMIT objectInfosChanged();
}

void TObjectAppearanceInstanceInfoStorage::resetCounts()
{
    for (auto it = m_infos.begin(); it != m_infos.end();) {
        it->second.count = 0;
        if (it->second.name.isEmpty())
            it = m_infos.erase(it);
        else
            ++it;
    }
}

void TObjectAppearanceInstanceInfoStorage::setCount(const TObjectAppearanceInstanceTypeID& instance, const int count)
{
    TInfo& info = m_infos[instance];
    if (info.count == count)
        return;
    info.count = count;
    m_emitTimer.start();
}

int TObjectAppearanceInstanceInfoStorage::count(const TObjectAppearanceInstanceTypeID& instance) const
{
    const auto it = m_infos.find(instance);
    return it == m_infos.end() ? 0 : it->second.count;
}

void TObjectAppearanceInstanceInfoStorage::setName(const TObjectAppearanceInstanceTypeID& instance, const QString& name)
{
    TInfo& info = m_infos[instance];
    if (info.name == name)
        return;
    info.name = name;
    m_emitTimer.start();
}

QString TObjectAppearanceInstanceInfoStorage::name(const TObjectAppearanceInstanceTypeID& instance) const
{
    const auto it = m_infos.find(instance);
    return it == m_infos.end() ? QString() : it->second.name;
}

TObjectAppearanceInstanceInfoStorage::TInfo TObjectAppearanceInstanceInfoStorage::info(const TObjectAppearanceInstanceTypeID& instance) const
{
    const auto it = m_infos.find(instance);
    return it == m_infos.end() ? TInfo{} : it->second;
}

} // namespace tibia::container
