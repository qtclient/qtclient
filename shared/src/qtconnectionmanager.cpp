#include "qtconnectionmanager.h"

namespace tibia::shared {

TQtConnectionManager::~TQtConnectionManager()
{
    disconnectAll();
}

void TQtConnectionManager::add(const QMetaObject::Connection& connection)
{
    m_connections.push_back(connection);
}

void TQtConnectionManager::disconnectAll()
{
    for (const QMetaObject::Connection& connection : m_connections)
        QObject::disconnect(connection);
    m_connections.clear();
}

}
