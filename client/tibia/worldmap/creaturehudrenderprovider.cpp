#include "worldmap/creaturehudrenderprovider.h"

#include "appearances/appearanceinstance.h"
#include "config/clientoptions.h"
#include "game/playerdata.h"
#include "protocol/protocolcodes.h"
#include "renderer/rendervolumeprovider.h"
#include "renderer/worldmaprenderprovider.h"
#include "worldmap/worldmapfield.h"
#include "worldmap/worldmapstorage.h"
#include "worldmap/worldmapviewport.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace tibia::worldmap {

namespace {

bool sameIcons(const std::vector<creatures::TCreatureIcon>& a, const std::vector<creatures::TCreatureIcon>& b)
{
    return std::equal(a.begin(), a.end(), b.begin(), b.end(), [](const auto& x, const auto& y) {
        return x.kind == y.kind && x.icon == y.icon && x.value == y.value && x.flag == y.flag;
    });
}

/// The creature icon a status mark shows; marks without one show none.
const std::map<uint8_t, uint8_t>& statusMarkIcons()
{
    static const std::map<uint8_t, uint8_t> icons{ { 2, 10 } };
    return icons;
}

/// The creature icons (kind 1, no number, flag set) of the status marks that have one.
std::vector<creatures::TCreatureIcon> statusMarkHudIcons(const std::set<uint8_t>& statusMarks)
{
    std::vector<creatures::TCreatureIcon> icons;
    for (const uint8_t mark : statusMarks) {
        const auto icon = statusMarkIcons().find(mark);
        if (icon == statusMarkIcons().end())
            continue;
        icons.push_back({ .kind = 1, .icon = icon->second, .value = 0, .flag = true });
    }
    return icons;
}

}

std::vector<creatures::TCreatureIcon> hudIconsWithStatusMarks(const creatures::TCreature& creature)
{
    std::vector<creatures::TCreatureIcon> icons = creature.hudIcons();
    const std::vector<creatures::TCreatureIcon> marks = statusMarkHudIcons(creature.extraIcons());
    icons.insert(icons.end(), marks.begin(), marks.end());
    return icons;
}

bool TCreatureHUDIconFlags::operator==(const TCreatureHUDIconFlags& other) const
{
    return summon == other.summon && npcCategory == other.npcCategory && guild == other.guild && party == other.party
        && skull == other.skull && sameIcons(icons, other.icons);
}

TCreatureHUDRenderProvider::TCreatureHUDRenderProvider(std::shared_ptr<TWorldMapStorage> worldMapStorage,
                                                       std::shared_ptr<creatures::TCreatureStorage> creatureStorage,
                                                       std::shared_ptr<TWorldMapViewport> worldMapViewport,
                                                       std::shared_ptr<qmlcomponents::TWorldMapRenderProvider> worldMapRenderProvider,
                                                       std::shared_ptr<game::TPlayerData> playerData,
                                                       config::TClientOptions* clientOptions, QObject* parent)
    : cip::tibia::worldmap::TCreatureHUDRenderProvider(parent)
    , m_worldMapStorage(std::move(worldMapStorage))
    , m_creatureStorage(std::move(creatureStorage))
    , m_worldMapViewport(std::move(worldMapViewport))
    , m_worldMapRenderProvider(std::move(worldMapRenderProvider))
    , m_playerData(std::move(playerData))
    , m_clientOptions(clientOptions)
    , m_playerStates(config::SpecialConditionsFilter::HUD)
{
    if (m_clientOptions) {
        connect(m_clientOptions, &config::TClientOptions::creatureHUDRenderProviderOptionsChanged, this,
                &TCreatureHUDRenderProvider::readOptions);
    }
    readOptions();
}

void TCreatureHUDRenderProvider::readOptions()
{
    if (!m_clientOptions)
        return;
    using O = config::EClientOption;
    m_playerHudEnabled = m_clientOptions->boolOption(O::playerHudEnabled);
    m_playerHudShowBars = m_clientOptions->boolOption(O::playerHudShowBars);
    m_playerHudShowArcs = m_clientOptions->boolOption(O::playerHudShowArcs);
    m_playerHudShowHarmonyLeft = m_clientOptions->boolOption(O::playerHudShowHarmonyLeft);
    m_playerHudArcDistance = m_clientOptions->floatOption(O::playerHudArcDistance);
    m_playerHudArcOpacity = m_clientOptions->floatOption(O::playerHudArcOpacity);
    m_playerHudArcHeight = m_clientOptions->floatOption(O::playerHudArcHeight);
    m_playerHudArcWidth = m_clientOptions->intOption(O::playerHudArcWidth);
    m_playerShowName = m_clientOptions->boolOption(O::playerShowName);
    m_playerShowHealth = m_clientOptions->boolOption(O::playerShowHealth);
    m_playerShowMana = m_clientOptions->boolOption(O::playerShowMana);
    m_playerShowHarmonySerene = m_clientOptions->boolOption(O::playerShowHarmonySerene);
    m_playerShowMarks = m_clientOptions->boolOption(O::playerShowMarks);
    m_creatureHudEnabled = m_clientOptions->boolOption(O::creatureHudEnabled);
    m_creatureShowName = m_clientOptions->boolOption(O::creatureShowName);
    m_creatureShowHealth = m_clientOptions->boolOption(O::creatureShowHealth);
    m_creatureShowMarks = m_clientOptions->boolOption(O::creatureShowMarks);
    m_creatureShowNpcIcon = m_clientOptions->boolOption(O::creatueShowNpcIcon);
}

std::vector<TCreatureHUDRenderInfo> TCreatureHUDRenderProvider::creatureHUDRenderInfos() const
{
    std::vector<TCreatureHUDRenderInfo> infos;
    if (!m_worldMapStorage || !m_worldMapViewport)
        return infos;
    std::vector<std::shared_ptr<creatures::TCreature>> creatures = m_worldMapStorage->creaturesIn(m_worldMapViewport->visibleArea());
    appendPlayer(creatures);

    infos.reserve(creatures.size());
    const int firstVisibleFloor =
        renderer::TRenderVolumeProvider::scanFirstVisibleFloor(*m_worldMapStorage, m_worldMapViewport->centerField());
    for (const std::shared_ptr<creatures::TCreature>& creature : creatures) {
        const bool local = creature->isLocalPlayer();
        const bool shown = local
            ? m_playerHudEnabled
                && (m_playerShowName || m_playerShowHealth || m_playerShowMana || m_playerShowHarmonySerene || m_playerShowMarks)
            : m_creatureHudEnabled && (m_creatureShowName || m_creatureShowHealth || m_creatureShowMarks || m_creatureShowNpcIcon);
        if (!shown)
            continue;
        // qtclient places a HUD only on a creature the frame draws.
        TWorldMapSubfieldCoordinate anchor;
        if (!frameAnchor(creature->creatureId(), anchor))
            continue;
        infos.push_back(creatureHUDRenderInfo(*creature, firstVisibleFloor));
    }
    return infos;
}

void TCreatureHUDRenderProvider::appendPlayer(std::vector<std::shared_ptr<creatures::TCreature>>& creatures) const
{
    if (std::any_of(creatures.begin(), creatures.end(), [](const auto& creature) { return creature->isLocalPlayer(); }))
        return;
    if (!m_creatureStorage)
        return;
    if (std::shared_ptr<creatures::TCreature> player = m_creatureStorage->player())
        creatures.push_back(std::move(player));
}

TCreatureHUDRenderInfo TCreatureHUDRenderProvider::creatureHUDRenderInfo(const creatures::TCreature& creature,
                                                                         const int firstVisibleFloor) const
{
    TCreatureHUDRenderInfo info;
    info.creatureId = creature.creatureId();
    info.name = creature.name();
    info.showName = m_creatureShowName;
    info.isLocalPlayer = creature.isLocalPlayer();
    TWorldMapSubfieldCoordinate anchor;
    frameAnchor(info.creatureId, anchor);
    info.anchor = adjustedAnchor(anchor, creature);
    info.covered = !isUncovered(m_worldMapStorage, TWorldMapCoordinate(creature.position()), firstVisibleFloor);
    info.lightness = lightnessAt(anchor);
    info.statusMarks = creature.extraIcons();

    bool showMarks = false;
    switch (creature.creatureType()) {
        case Proto::CreatureTypePlayer:
            if (!info.isLocalPlayer) {
                info.showHealthBar = m_creatureShowHealth;
                info.healthPercent = creature.healthPercent();
                showMarks = m_creatureShowMarks;
            } else {
                info.showBars = m_playerHudShowBars;
                info.showArcs = m_playerHudShowArcs;
                info.arcOpacity = m_playerHudArcOpacity;
                info.arcDistance = m_playerHudArcDistance;
                info.arcWidth = static_cast<uint16_t>(m_playerHudArcWidth);
                info.arcHeight = m_playerHudArcHeight;
                info.showName = m_playerShowName;
                info.showHealthBar = m_playerShowHealth;
                info.showManaBar = m_playerShowMana;
                if (m_playerData) {
                    info.healthPercent = m_playerData->healthPercent();
                    info.manaPercent = m_playerData->manaPercent();
                    info.manaShieldPercent = m_playerData->manaShieldPercent();
                    info.showManaShieldBar = m_playerData->magicShieldVocation();
                    // Vocation 5 is the monk.
                    info.showCombopointsAndSerene = m_playerShowHarmonySerene && m_playerData->vocation() == 5;
                    info.comboPoints = m_playerData->harmony();
                    info.isSerene = m_playerData->serene();
                }
                // The own character's HUD stays readable in the dark.
                info.lightness = std::max<uint8_t>(info.lightness, 0x66);
                info.showCombopointsAndSereneLeft = m_playerHudShowHarmonyLeft;
                showMarks = m_playerShowMarks;
                info.showSpecialConditions = m_playerStates.options().showInHUD;
                info.playerStates = m_playerStates.model()->entries();
            }
            if (showMarks) {
                info.iconFlags.guild = creature.emblem();
                info.iconFlags.party = creature.shield();
                info.iconFlags.skull = creature.skull();
                info.iconFlags.icons = hudIconsWithStatusMarks(creature);
            }
            break;
        case Proto::CreatureTypeMonster:
            info.showHealthBar = m_creatureShowHealth;
            info.healthPercent = creature.healthPercent();
            if (m_creatureShowMarks)
                info.iconFlags.icons = hudIconsWithStatusMarks(creature);
            break;
        case Proto::CreatureTypeNpc:
            if (m_creatureShowNpcIcon)
                info.iconFlags.npcCategory = creature.icon();
            break;
        case Proto::CreatureTypeSummonOwn:
            info.showHealthBar = m_creatureShowHealth;
            info.healthPercent = creature.healthPercent();
            if (m_creatureShowMarks) {
                // 1 the player's own summon, 2 another player's.
                const creatures::TCreatureID player = m_creatureStorage ? m_creatureStorage->playerID() : 0;
                info.iconFlags.summon = player != creature.masterId() ? 2 : 1;
                info.iconFlags.party = creature.shield();
            }
            break;
        default:
            break;
    }
    return info;
}

bool TCreatureHUDRenderProvider::isUncovered(const std::shared_ptr<TWorldMapStorage>& worldMapStorage, const TWorldMapCoordinate& position,
                                             const int firstVisibleFloor)
{
    if (!worldMapStorage)
        return true;
    for (int floor = position.z; firstVisibleFloor < floor;) {
        --floor;
        const TWorldMapCoordinate above = TWorldMapCoordinate(position.x, position.y, floor).shiftedTowardsFloor(position.z);
        if (const std::shared_ptr<TWorldMapField> field = worldMapStorage->field(above); field && field->hasBank())
            return false;
    }
    return true;
}

TWorldMapSubfieldCoordinate TCreatureHUDRenderProvider::adjustedAnchor(const TWorldMapSubfieldCoordinate& anchor,
                                                                       const creatures::TCreature& creature)
{
    const appearances::TAppearanceInstancePtr outfit = creature.outfitAppearance();
    const bool isOutfit = outfit && outfit->category() == ThingCategoryCreature;
    const uint16_t outfitId = isOutfit ? outfit->id() : 0;
    TWorldMapSubfieldCoordinate adjusted = anchor;
    // These outfits stand taller than their field: the HUD moves up and left.
    if (isOutfit && (outfitId == 1102 || outfitId == 1103 || outfitId == 1173 || outfitId == 1174)) {
        adjusted.subfieldY += (creature.isMounted() ? 0 : 5) - 17;
        adjusted.subfieldX -= 5;
    }
    if (isOutfit && creature.isMounted() && (outfitId == 1173 || outfitId == 1174))
        adjusted.subfieldY -= 5;
    return adjusted;
}

bool TCreatureHUDRenderProvider::frameAnchor(const creatures::TCreatureID creatureId, TWorldMapSubfieldCoordinate& anchor) const
{
    if (!m_worldMapRenderProvider)
        return false;
    const auto& frame = m_worldMapRenderProvider->frame();
    if (!frame.valid)
        return false;
    for (const auto& hud : frame.creatures) {
        if (hud.creatureId == creatureId) {
            anchor = TWorldMapSubfieldCoordinate(hud.point);
            return true;
        }
    }
    return false;
}

uint8_t TCreatureHUDRenderProvider::lightnessAt(const TWorldMapSubfieldCoordinate& anchor) const
{
    if (!m_worldMapRenderProvider)
        return 0xff;
    constexpr int HalfField = qmlcomponents::TWorldMapRenderProvider::FieldSize / 2;
    const float lightness = m_worldMapRenderProvider->lightnessAt(anchor.toMapPixel() + QPoint(HalfField, HalfField));
    return static_cast<uint8_t>(std::lround(std::clamp(lightness, 0.f, 1.f) * 255.f));
}

}
