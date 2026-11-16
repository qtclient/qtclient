#pragma once

#include <QMetaObject>
#include <QObject>

#include <vector>

namespace tibia {
namespace shared {

/// CIP `shared::TQtConnectionManager`: owns Qt connections and disconnects
/// them together (at the latest when it is destroyed).
class TQtConnectionManager
{
public:
    TQtConnectionManager() = default;
    virtual ~TQtConnectionManager();

    void add(const QMetaObject::Connection& connection);
    /// Disconnects and forgets every connection.
    void disconnectAll();

private:
    std::vector<QMetaObject::Connection> m_connections;
};

}
}
