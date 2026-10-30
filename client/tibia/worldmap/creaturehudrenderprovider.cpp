#include "worldmap/creaturehudrenderprovider.h"

#include "renderer/worldmaprenderprovider.h"

namespace tibia::worldmap {

TCreatureHUDRenderProvider::TCreatureHUDRenderProvider(QObject* parent)
    : cip::tibia::worldmap::TCreatureHUDRenderProvider(parent)
{
}

void TCreatureHUDRenderProvider::setRecords(QVector<Record> records)
{
    m_records = std::move(records);
}

void TCreatureHUDRenderProvider::syncFrom(const qmlcomponents::TWorldMapRenderProvider& provider)
{
    QVector<Record> next;
    const auto& creatures = provider.frame().creatures;
    next.reserve(static_cast<int>(creatures.size()));
    for (const auto& hud : creatures)
        next.push_back({ hud.creature, hud.point });
    m_records = std::move(next);
}

}
