#include "creatures/battlelistrenderprovider.h"

#include "colorhelper.h"
#include "config/clientoptions.h"
#include "creatures/creaturestorage.h"
#include "protocol/protocolcodes.h"
#include "qmlcomponents/qmlcomponentsciptypes.h"
#include "worldmap/worldmapviewport.h"

#include <client/outfit.h>

#include <algorithm>
#include <cstdlib>

namespace tibia::creatures {

namespace {

/// An icon's image URL in a creature state flag provider (value 1 is image
/// 0); none for 0.
QString stateFlagUrl(const char* provider, const uint8_t value)
{
    if (value == 0)
        return {};
    return QStringLiteral("image://%1/%2").arg(QLatin1String(provider)).arg(value - 1);
}

/// The icon has a kind (neither 0 nor -1).
bool hasKind(const TCreatureIcon& icon)
{
    const auto kind = static_cast<int32_t>(icon.kind);
    return (kind >> 31) != kind;
}

QColor outfitColor(const int index)
{
    const QColor c = tibia::shared::outfitColor(index);
    return QColor(c.red(), c.green(), c.blue());
}

}

TBattleListDrawCommand& TBattleListDrawCommand::setCreatureId(const uint32_t id)
{
    creatureId = id;
    return *this;
}

TBattleListDrawCommand& TBattleListDrawCommand::setVisibleOrder(const uint order)
{
    visibleOrder = order;
    return *this;
}

TBattleListDrawCommand& TBattleListDrawCommand::setName(const QString& value)
{
    name = value;
    return *this;
}

TBattleListDrawCommand& TBattleListDrawCommand::setHealthPercent(const float percent)
{
    healthPercent = percent;
    healthColor = qmlcomponents::colorForHealthPercent(percent);
    return *this;
}

TBattleListDrawCommand& TBattleListDrawCommand::setManaPercent(const float percent)
{
    manaPercent = percent;
    return *this;
}

TBattleListDrawCommand& TBattleListDrawCommand::setShowMana(const bool show)
{
    showMana = show;
    return *this;
}

TBattleListDrawCommand& TBattleListDrawCommand::setOutfit(const Outfit& outfit)
{
    outfitIsObject = outfit.getCategory() == ThingCategoryItem;
    outfitId = outfitIsObject ? outfit.getAuxId() : outfit.getId();
    headColor = outfitColor(outfit.getHead());
    torsoColor = outfitColor(outfit.getBody());
    legsColor = outfitColor(outfit.getLegs());
    detailColor = outfitColor(outfit.getFeet());
    firstAddOn = (outfit.getAddons() & 1) != 0;
    secondAddOn = (outfit.getAddons() & 2) != 0;
    return *this;
}

TBattleListDrawCommand& TBattleListDrawCommand::setDistance(const uint value)
{
    distance = value;
    return *this;
}

TBattleListDrawCommand& TBattleListDrawCommand::setFrames(const std::vector<QColor>& colors)
{
    frames = colors;
    updateFrameColors();
    return *this;
}

void TBattleListDrawCommand::updateFrameColors()
{
    frameColors.clear();
    for (const QColor& color : frames)
        frameColors.append(color.name());
}

TBattleListDrawCommand& TBattleListDrawCommand::setTemporarySelected(const bool selected)
{
    temporarySelected = selected;
    return *this;
}

TBattleListDrawCommand& TBattleListDrawCommand::setTutorialMarkerId(const int markerId)
{
    tutorialMarkerId = markerId;
    return *this;
}

TBattleListDrawCommand& TBattleListDrawCommand::setStatus(const worldmap::TCreatureHUDIconFlags& flags)
{
    status = flags;
    updateStatusIcons();
    return *this;
}

void TBattleListDrawCommand::updateStatusIcons()
{
    // The HUD icons with a kind and an icon, then summon, guild, party and
    // skull. Empty URLs stay in: the QML row skips the zero-sized images.
    statusIcons.clear();
    for (const TCreatureIcon& icon : status.icons) {
        if (icon.icon != 0 && hasKind(icon))
            statusIcons.append(stateFlagUrl("creaturestateflags-othercreatureicons", icon.icon));
    }
    statusIcons.append(stateFlagUrl("creaturestateflags-summon", status.summon));
    statusIcons.append(stateFlagUrl("creaturestateflags-guild", status.guild));
    statusIcons.append(stateFlagUrl("creaturestateflags-party", status.party));
    statusIcons.append(stateFlagUrl("creaturestateflags-playerkiller", status.skull));
}

TBattleListDrawCommand& TBattleListDrawCommand::setInSharedXpDistance(const bool inDistance)
{
    inSharedXpDistance = inDistance;
    return *this;
}

TBattleListDrawCommand& TBattleListDrawCommand::setInViewport(const bool visible)
{
    inViewport = visible;
    return *this;
}

TBattleListDrawCommand& TBattleListDrawCommand::setFiendish(const bool value)
{
    fiendish = value;
    return *this;
}

TBattleListDrawCommand& TBattleListDrawCommand::setVocation(const int value)
{
    vocation = value;
    return *this;
}

TBattleListDrawCommand& TBattleListDrawCommand::setExtraIcons(std::set<uint8_t> icons)
{
    extraIcons = std::move(icons);
    return *this;
}

bool TBattleListDrawCommand::operator==(const TBattleListDrawCommand& other) const
{
    // CIP compares the outfit by pointer and the status (icons and marks) as
    // one record; qtclient carries both by value.
    return creatureId == other.creatureId && name == other.name && healthPercent == other.healthPercent
        && frameColors == other.frameColors && statusIcons == other.statusIcons && fiendish == other.fiendish
        && extraIcons == other.extraIcons && outfitId == other.outfitId && outfitIsObject == other.outfitIsObject
        && headColor == other.headColor && torsoColor == other.torsoColor && legsColor == other.legsColor
        && detailColor == other.detailColor && firstAddOn == other.firstAddOn && secondAddOn == other.secondAddOn
        && tutorialMarkerId == other.tutorialMarkerId && visibleOrder == other.visibleOrder && distance == other.distance
        && temporarySelected == other.temporarySelected && manaPercent == other.manaPercent && showMana == other.showMana
        && inSharedXpDistance == other.inSharedXpDistance && inViewport == other.inViewport && vocation == other.vocation;
}

// ---------------------------------------------------------------------------

TBattleListRenderProvider::TBattleListRenderProvider(std::shared_ptr<TCreatureStorage> creatureStorage,
                                                     std::shared_ptr<worldmap::TWorldMapViewport> worldMapViewport,
                                                     std::shared_ptr<game::TPlayerData> playerData,
                                                     config::TClientOptions* clientOptions, QObject* parent)
    : cip::tibia::creatures::TBattleListRenderProvider(parent)
    , m_creatureStorage(std::move(creatureStorage))
    , m_worldMapViewport(std::move(worldMapViewport))
    , m_clientOptions(clientOptions)
    , m_playerData(std::move(playerData))
{}

std::vector<TBattleListDrawCommand> TBattleListRenderProvider::drawCommands()
{
    std::vector<TBattleListDrawCommand> commands;
    const std::shared_ptr<TCreature> player = m_creatureStorage ? m_creatureStorage->player() : nullptr;
    if (!player || !m_worldMapViewport)
        return commands;
    const Position& playerPosition = player->position();
    const worldmap::TShearedWorldMapCuboid view = m_worldMapViewport->visibleArea();
    const std::map<TCreatureID, std::shared_ptr<TCreature>> creatures = m_creatureStorage->battleListCreatures(view);
    // The first list keeps the creatures' ages.
    if (m_listedCreatureIds.empty() && !creatures.empty()) {
        for (const auto& [id, creature] : creatures)
            m_listedCreatureIds.insert(id);
    }

    // The rows show the attack and follow frames when the map shows either the
    // target frame or the target highlight.
    const TCreatureMarkOptions mapMarks = TCreature::markOptions();
    TCreatureMarkOptions listMarks = mapMarks;
    if (m_clientOptions) {
        listMarks.targetFrame = m_clientOptions->boolOption(config::EClientOption::gameWindowShowTragetFrame)
            || m_clientOptions->boolOption(config::EClientOption::gameWindowShowTargetHighlight);
    }
    TCreature::setMarkOptions(listMarks);

    const TCreatureID temporarySelected = m_creatureStorage->temporarySelectedID();
    commands.reserve(creatures.size());
    for (const auto& [id, creature] : creatures) {
        const uint32_t creatureId = creature->creatureId();
        // A summon is the player's own (1) or another player's (2).
        uint8_t summon = 0;
        if (creature->creatureType() == Proto::CreatureTypeSummonOwn)
            summon = m_creatureStorage->playerID() == creature->masterId() ? 1 : 2;
        const Position& position = creature->position();
        const uint distance =
            static_cast<uint>(std::max(std::abs(position.x - playerPosition.x), std::abs(position.y - playerPosition.y)));
        int tutorialMarkerId = 0;
        if (const auto marker = m_tutorialMarkers.find(creatureId); marker != m_tutorialMarkers.end())
            tutorialMarkerId = marker->second;
        const std::vector<TCreatureIcon>& hudIcons = creature->hudIcons();
        const bool fiendish = std::any_of(hudIcons.begin(), hudIcons.end(),
                                          [](const TCreatureIcon& icon) { return icon.icon == 5 && hasKind(icon); });
        // A creature new to the list becomes the youngest.
        if (!m_listedCreatureIds.contains(creatureId)) {
            creature->renewUniqueId();
            m_listedCreatureIds.insert(creatureId);
        }

        worldmap::TCreatureHUDIconFlags status;
        status.guild = creature->emblem();
        status.skull = creature->skull();
        status.party = creature->shield();
        status.summon = summon;
        status.icons = worldmap::hudIconsWithStatusMarks(*creature);

        TBattleListDrawCommand command;
        command.setCreatureId(creatureId)
            .setVisibleOrder(static_cast<uint>(creature->uniqueId()))
            .setName(creature->name())
            .setHealthPercent(creature->healthRatio())
            .setManaPercent(creature->manaRatio())
            .setShowMana(creature->isPartyMember() ? creature->isPlayer() : false)
            .setOutfit(creature->outfit())
            .setDistance(distance)
            .setFrames(creature->marks())
            .setTemporarySelected(creatureId == temporarySelected)
            .setTutorialMarkerId(tutorialMarkerId)
            .setStatus(status)
            .setInSharedXpDistance(creature->showsStatus())
            .setInViewport(creature->isPlacedOnField()
                               ? view.containsSheared(worldmap::TWorldMapCoordinate(position))
                               : false)
            .setFiendish(fiendish)
            .setVocation(creature->vocation())
            .setExtraIcons(creature->extraIcons());
        commands.push_back(std::move(command));
    }
    TCreature::setMarkOptions(mapMarks);

    // Creatures that left the list are new to it when they come back.
    std::erase_if(m_listedCreatureIds, [&creatures](const uint32_t id) { return !creatures.contains(id); });
    return commands;
}

void TBattleListRenderProvider::setTutorialMarker(const uint32_t creatureId, const int markerId)
{
    m_tutorialMarkers[creatureId] = markerId;
}

void TBattleListRenderProvider::removeAllTutorialMarkers() { m_tutorialMarkers.clear(); }

}
