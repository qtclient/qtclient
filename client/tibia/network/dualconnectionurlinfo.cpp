#include "network/dualconnectionurlinfo.h"

#include <algorithm>
#include <cmath>

namespace tibia::network {

TDualConnectionUrlInfo::TDualConnectionUrlInfo(const QUrl& PrimaryUrl, const QUrl& SecondaryUrl)
{
    TUrlState& primary = m_urls[PrimaryUrl];
    primary = TUrlState{};
    primary.primary = true;
    TUrlState& secondary = m_urls[SecondaryUrl];
    secondary = TUrlState{};
    secondary.primary = false;
}

TDualConnectionUrlInfo::~TDualConnectionUrlInfo() = default;

void TDualConnectionUrlInfo::resetNextConnectTimes(const int64_t Now)
{
    for (auto& [url, state] : m_urls)
        state.nextConnectTime = Now + (state.primary ? 0 : 1);
}

void TDualConnectionUrlInfo::resetFailedConnectAttempts(const QUrl& Url, const int64_t NextConnectTime)
{
    const auto it = m_urls.find(Url);
    if (it == m_urls.end())
        return;
    it->second.failedConnectAttempts = 0;
    it->second.nextConnectTime = NextConnectTime;
}

QUrl TDualConnectionUrlInfo::nextUrlToConnect(const int64_t Now) const
{
    for (const auto& [url, state] : m_urls) {
        if (!state.connected && state.nextConnectTime <= Now)
            return url;
    }
    return QUrl();
}

EConnectionsUsed TDualConnectionUrlInfo::connectionsUsed() const
{
    bool primaryConnected = false;
    bool secondaryConnected = false;
    for (const auto& [url, state] : m_urls) {
        if (!state.connected)
            continue;
        if (state.primary)
            primaryConnected = true;
        else
            secondaryConnected = true;
    }
    return static_cast<EConnectionsUsed>((primaryConnected ? 2 : 0) | (secondaryConnected ? 1 : 0));
}

uint64_t TDualConnectionUrlInfo::failedConnectAttempts() const
{
    uint64_t attempts = 0;
    for (const auto& [url, state] : m_urls)
        attempts += state.failedConnectAttempts;
    return attempts;
}

bool TDualConnectionUrlInfo::isPrimaryUrl(const QUrl& Url) const
{
    const auto it = m_urls.find(Url);
    return it != m_urls.end() && it->second.primary;
}

void TDualConnectionUrlInfo::setConnected(const QUrl& Url)
{
    const auto it = m_urls.find(Url);
    if (it == m_urls.end())
        return;
    it->second.connected = true;
}

void TDualConnectionUrlInfo::setDisconnected(const QUrl& Url, const int64_t Now, const bool Backoff)
{
    const auto it = m_urls.find(Url);
    if (it == m_urls.end())
        return;
    TUrlState& state = it->second;
    const uint64_t attemptsBefore = state.failedConnectAttempts;
    state.failedConnectAttempts = attemptsBefore + 1;
    int64_t delay = 0;
    if (Backoff)
        delay = static_cast<int64_t>(std::exp2(static_cast<double>(std::min<uint64_t>(attemptsBefore, 4)))) * 1000;
    state.nextConnectTime = Now + delay;
    state.connected = false;
}

}
