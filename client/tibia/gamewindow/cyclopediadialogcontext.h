#pragma once

#include "cyclopedia/bosstiarystorage.h"
#include "worldmap/coordinates.h"

#include <cip_gen.h>

#include <QString>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <stdexcept>
#include <variant>

namespace tibia {
namespace gamewindow {

/// What a Cyclopedia tab keeps when it is left for another one, so the back
/// button can show it again. Each tab captures and restores its own kind; a
/// tab handed another kind throws std::invalid_argument.

/// The item page: the category (-1 for a name search or a single type), the
/// name searched, the four filter buttons and the selected object type.
struct TItemInfoCyclopediaContext
{
    int category = -1;
    QString name;
    bool buttons[4] = {};
    uint16_t typeId = 0;
    bool operator==(const TItemInfoCyclopediaContext& other) const
    {
        return category == other.category && name == other.name
               && std::equal(std::begin(buttons), std::end(buttons), std::begin(other.buttons)) && typeId == other.typeId;
    }
};

/// The bestiary page: the view shown, the classification, the race search and the race.
struct TMonsterInfoCyclopediaContext
{
    cip::tibia::gamewindow::TMonsterDialogController::DialogState state =
        cip::tibia::gamewindow::TMonsterDialogController::MonsterClassification;
    QString classification;
    QString search;
    int raceId = 0;
    bool operator==(const TMonsterInfoCyclopediaContext&) const = default;
};

/// The charm page: the selected charm and the monster picked for it.
struct TMonsterBonusEffectCyclopediaContext
{
    int charmId = 0;
    int temporarySelectedMonsterIndex = 0;
    bool operator==(const TMonsterBonusEffectCyclopediaContext&) const = default;
};

/// The map page: the camera's top-left subfield (0, 0, 0 when the map had no
/// camera, which restores to the own character) and its zoom.
struct TMapInfoCyclopediaContext
{
    worldmap::TWorldMapSubfieldCoordinate topLeft;
    float zoom = 1.0f;
    bool operator==(const TMapInfoCyclopediaContext& other) const { return topLeft == other.topLeft && zoom == other.zoom; }
};

/// The houses page: the town and the state, type and sort filters, the
/// selected house, and the preview camera's centre and zoom.
struct THousesInfoCyclopediaContext
{
    QString town;
    int state = 5;
    int type = 0;
    int sortOrder = 0;
    uint houseId = 0;
    worldmap::TWorldMapSubfieldCoordinate cameraCenter;
    float cameraZoom = 1.0f;
    bool operator==(const THousesInfoCyclopediaContext& other) const
    {
        return town == other.town && state == other.state && type == other.type && sortOrder == other.sortOrder
               && houseId == other.houseId && cameraCenter == other.cameraCenter && cameraZoom == other.cameraZoom;
    }
};

/// The character page: the data type shown and the page of the deaths or kills.
struct TCharacterInfoCyclopediaContext
{
    int dataType = 0;
    qint64 page = 1;
    bool operator==(const TCharacterInfoCyclopediaContext&) const = default;
};

/// The bosstiary page: the filter and the page.
struct TBosstiaryCyclopediaContext
{
    cyclopedia::TBosstiaryFilter filter;
    int page = 1;
    bool operator==(const TBosstiaryCyclopediaContext&) const = default;
};

/// The boss slots page keeps nothing.
struct TBossSlotsCyclopediaContext
{
    bool operator==(const TBossSlotsCyclopediaContext&) const = default;
};

/// The magical archive: the spell the page opens on (0: the first one).
struct TMagicalArchiveCyclopediaContext
{
    uint16_t spellId = 0;
    bool operator==(const TMagicalArchiveCyclopediaContext&) const = default;
};

using TCyclopediaDialogContext =
    std::variant<TItemInfoCyclopediaContext, TMonsterInfoCyclopediaContext, TMonsterBonusEffectCyclopediaContext, TMapInfoCyclopediaContext,
                 THousesInfoCyclopediaContext, TCharacterInfoCyclopediaContext, TBosstiaryCyclopediaContext, TBossSlotsCyclopediaContext,
                 TMagicalArchiveCyclopediaContext>;

/// The tab's own kind of context, or std::invalid_argument.
template <class T>
const T& cyclopediaContextAs(const TCyclopediaDialogContext& context)
{
    const T* own = std::get_if<T>(&context);
    if (!own)
        throw std::invalid_argument("Invalid cyclopedia context type.");
    return *own;
}

}
}
