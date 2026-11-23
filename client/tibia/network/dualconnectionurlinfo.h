#pragma once

#include <QUrl>

#include <cstdint>
#include <map>

namespace tibia {
namespace network {

/// Which of the game server connections carry the session: bit 0 a
/// secondary URL's connection, bit 1 the primary URL's.
enum class EConnectionsUsed : int
{
    None = 0,
    Secondary = 1,
    Primary = 2,
    PrimaryAndSecondary = 3,
};

/// CIP network::TDualConnectionUrlInfo: the game server URLs a dual
/// connection uses (one primary, one secondary), each with its connection
/// state, the time it may be connected to next and its failed attempts in a
/// row. Times are the game clock's milliseconds.
class TDualConnectionUrlInfo
{
public:
    TDualConnectionUrlInfo(const QUrl& PrimaryUrl, const QUrl& SecondaryUrl);
    virtual ~TDualConnectionUrlInfo();

    /// Every URL may be connected to again: the primary at `Now`, the
    /// secondaries one millisecond later.
    virtual void resetNextConnectTimes(int64_t Now);
    /// `Url` may be connected to at `NextConnectTime`; its failed attempts are forgotten.
    virtual void resetFailedConnectAttempts(const QUrl& Url, int64_t NextConnectTime);
    /// The first URL (in URL order) that is not connected and may be
    /// connected to at `Now`; an empty URL when there is none.
    virtual QUrl nextUrlToConnect(int64_t Now) const;
    /// Which URLs are connected.
    virtual EConnectionsUsed connectionsUsed() const;
    /// The failed attempts of all URLs together.
    virtual uint64_t failedConnectAttempts() const;
    /// Whether `Url` is the primary URL (false for an unknown one).
    virtual bool isPrimaryUrl(const QUrl& Url) const;
    /// `Url` is connected now.
    virtual void setConnected(const QUrl& Url);
    /// `Url` lost its connection (or failed to connect) at `Now`: one more
    /// failed attempt; with `Backoff` it may be connected to again after
    /// 2^min(attempts before, 4) seconds, else right away.
    virtual void setDisconnected(const QUrl& Url, int64_t Now, bool Backoff);

private:
    struct TUrlState
    {
        bool primary = false;
        int64_t nextConnectTime = 0;
        uint64_t failedConnectAttempts = 0;
        bool connected = false;
    };

    std::map<QUrl, TUrlState> m_urls;
};

}
}
