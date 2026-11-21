#include "managedcontainerstorage.h"

namespace tibia::quickloot {

TManagedContainerStorage::TManagedContainerStorage(QObject* parent)
    : cip::tibia::quickloot::TManagedContainerStorage(parent)
{
}

TManagedContainerStorage::~TManagedContainerStorage() = default;

void TManagedContainerStorage::setManagedContainers(const Assignments& assignments)
{
    if (m_assignments == assignments)
        return;
    m_assignments = assignments;
    Q_EMIT managedContainersUpdated();
}

void TManagedContainerStorage::setUseMainBackpackAsFallback(const bool value)
{
    m_fallback = value;
    Q_EMIT useMainBackpackAsFallbackChanged();
}

TManagedContainerStorage::Assignment TManagedContainerStorage::assignment(const int category) const
{
    const auto it = m_assignments.find(category);
    return it == m_assignments.end() ? Assignment{} : it->second;
}

}
