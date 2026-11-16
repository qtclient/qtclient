#pragma once

#include <QString>
#include <QtGlobal>

namespace tibia {
namespace shared {

/// CIP `shared::TLauncherUtils`.
class TLauncherUtils
{
public:
    /// Whether an IPv4 interface (not loopback) reaches the internet: a TCP
    /// connection to 8.8.8.8:53, else 1.1.1.1:80, within 750 ms. Probed once.
    static bool hasLocalIPv4();
    /// Whether a TCP connection to `host`:`port` opens within `timeoutMs`.
    static bool canConnect(const QString& host, quint16 port, int timeoutMs);
};

}
}
