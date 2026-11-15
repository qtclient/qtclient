#include "managedcontainerstorage.h"

#include <tibia/game/tgamenotify.h>

namespace tibia::quickloot {

namespace {

TManagedContainerStorage* g_instance = nullptr;

}

TManagedContainerStorage::TManagedContainerStorage(QObject* parent)
    : cip::tibia::quickloot::TManagedContainerStorage(parent)
{
    g_instance = this;
    // Host: this storage outlives a game session, so a session's end clears it.
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameEnded, this, [this]() {
        m_fallback = false;
        m_assignments.clear();
        Q_EMIT useMainBackpackAsFallbackChanged();
        Q_EMIT managedContainersUpdated();
    });
}

TManagedContainerStorage::~TManagedContainerStorage()
{
    if (g_instance == this)
        g_instance = nullptr;
}

TManagedContainerStorage* TManagedContainerStorage::instance() { return g_instance; }

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
