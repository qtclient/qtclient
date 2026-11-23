#include "minimap/minimapmarkerqmlrenderinfo.h"

namespace tibia::minimap {

TMinimapMarkerQmlRenderInfo::TMinimapMarkerQmlRenderInfo(QObject* parent)
    : cip::tibia::minimap::TMinimapMarkerQmlRenderInfo(parent)
{
}

QString TMinimapMarkerQmlRenderInfo::tooltiptext() const
{
    return m_marker.description;
}

QString TMinimapMarkerQmlRenderInfo::symbol() const
{
    return QStringLiteral("image://%1/%2").arg(QStringLiteral("minimap-markers")).arg(m_marker.icon);
}

QDateTime TMinimapMarkerQmlRenderInfo::lastModifiedTimestamp() const
{
    return m_marker.creationTime;
}

void TMinimapMarkerQmlRenderInfo::setMarker(const TMinimapMarker& marker)
{
    const TMinimapMarker previous = m_marker;
    m_marker = marker;
    if (previous.description != m_marker.description)
        Q_EMIT tooltiptextChanged();
    if (previous.icon != m_marker.icon)
        Q_EMIT symbolChanged();
    if (previous.creationTime != m_marker.creationTime)
        Q_EMIT lastModifiedTimestampChanged();
}

}
