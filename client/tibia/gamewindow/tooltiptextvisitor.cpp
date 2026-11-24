#include "gamewindow/tooltiptextvisitor.h"

#include "cyclopedia/cyclopediamapstaticdatastorage.h"
#include "cyclopedia/staticcontentinformation.h"

#include <QtGlobal>

namespace tibia::gamewindow {

QString TTooltipTextVisitor::operator()(const worldmap::TPassageMapAreaData& data) const
{
    QString names;
    if (m_staticData) {
        // The passage data lists the subareas behind it (qtclient's
        // field name is raceIds).
        for (const quint16 subAreaId : data.raceIds) {
            if (!names.isEmpty())
                names.append(QStringLiteral(", "));
            names.append(m_staticData->subAreaName(subAreaId));
        }
    }
    return qtTrId("passage_tooltip").arg(names);
}

QString TTooltipTextVisitor::operator()(const worldmap::TViewpointMapAreaData&) const
{
    return qtTrId("viewpoint_tooltip");
}

QString TTooltipTextVisitor::operator()(const worldmap::TActiveRaidMapAreaData&) const
{
    return qtTrId("active_raid_tooltip");
}

QString TTooltipTextVisitor::operator()(const worldmap::TPlayerPositionMapAreaData&) const
{
    return qtTrId("player_position_tooltip");
}

QString TTooltipTextVisitor::operator()(const worldmap::THouseMarkerWorldMapAreaData& data) const
{
    const char* listing = "";
    switch (data.listing) {
    case 0:
        listing = "house_owned_tooltip";
        break;
    case 1:
        listing = "house_auctioned_tooltip";
        break;
    case 2:
        listing = "house_not_auctionable_tooltip";
        break;
    default:
        break;
    }
    return QStringLiteral("%1\n%2").arg(cyclopedia::staticHouse(data.houseId).name).arg(qtTrId(listing));
}

}
