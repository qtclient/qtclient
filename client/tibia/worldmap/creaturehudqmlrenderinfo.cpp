#include "worldmap/creaturehudqmlrenderinfo.h"

#include "game/playerstates.h"
#include "qmlcomponents/qmlcomponentsciptypes.h"

#include <algorithm>

namespace tibia::worldmap {

namespace {

/// One strip cell per value, none for 0.
QString stripIcon(const char* provider, const uint8_t value)
{
    return value ? QStringLiteral("image://creaturestateflags-%1/%2").arg(QLatin1String(provider)).arg(value - 1) : QString();
}

/// `color` darkened by `lightness` (0 black to 255 unchanged).
QColor darkened(const QColor& color, const int lightness)
{
    return QColor::fromRgb(color.red() * lightness / 0xff, color.green() * lightness / 0xff, color.blue() * lightness / 0xff, 0xff);
}

}

void TCreatureOtherIconModel::setIcons(const QList<Icon>& icons)
{
    if (icons == m_icons)
        return;
    beginResetModel();
    m_icons = icons;
    endResetModel();
}

int TCreatureOtherIconModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_icons.size());
}

QVariant TCreatureOtherIconModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() >= m_icons.size())
        return {};
    const Icon& icon = m_icons[index.row()];
    switch (role) {
        case Qt::UserRole + 1: return icon.image;
        case Qt::UserRole + 2: return icon.value;
        case Qt::UserRole + 3: return icon.showValue;
        default: return {};
    }
}

QHash<int, QByteArray> TCreatureOtherIconModel::roleNames() const
{
    return { { Qt::UserRole + 1, "image" }, { Qt::UserRole + 2, "value" }, { Qt::UserRole + 3, "showValue" } };
}

TCreatureHUDQmlRenderInfo::TCreatureHUDQmlRenderInfo(QObject* parent)
    : cip::tibia::worldmap::TCreatureHUDQmlRenderInfo(parent)
    , m_otherIconModel(new TCreatureOtherIconModel())
    , m_playerStateModel(new game::TPlayerStateDisplayInfoModel())
{
    m_creatureOtherIcons = m_otherIconModel;
    m_playerStates = m_playerStateModel;
}

TCreatureHUDQmlRenderInfo::~TCreatureHUDQmlRenderInfo()
{
    m_playerStates = nullptr;
    delete m_playerStateModel;
    m_creatureOtherIcons = nullptr;
    delete m_otherIconModel;
}

QColor TCreatureHUDQmlRenderInfo::healthColor() const
{
    // Grey while a floor above covers the creature; else the health colour,
    // full health without a health bar.
    QColor color(0xc0, 0xc0, 0xc0);
    if (!m_renderInfo.covered) {
        const float health = m_renderInfo.showHealthBar ? static_cast<float>(m_renderInfo.healthPercent / 100.0) : 1.f;
        color = qmlcomponents::colorForHealthPercent(health);
    }
    return darkened(color, m_renderInfo.lightness);
}

QStringList TCreatureHUDQmlRenderInfo::horizontalCreatureIcons() const
{
    const TCreatureHUDIconFlags& flags = m_renderInfo.iconFlags;
    QStringList icons;
    // An NPC's category is its only icon.
    if (flags.npcCategory) {
        icons.append(stripIcon("npc-categories", flags.npcCategory));
        return icons;
    }
    if (flags.party)
        icons.append(stripIcon("party", flags.party));
    if (flags.skull)
        icons.append(stripIcon("playerkiller", flags.skull));
    if (flags.summon)
        icons.append(stripIcon("summon", flags.summon));
    return icons;
}

QStringList TCreatureHUDQmlRenderInfo::verticalCreatureIcons() const
{
    QStringList icons;
    if (m_renderInfo.iconFlags.guild)
        icons.append(stripIcon("guild", m_renderInfo.iconFlags.guild));
    return icons;
}

bool TCreatureHUDQmlRenderInfo::isFiendishMonster() const
{
    const auto& icons = m_renderInfo.iconFlags.icons;
    // Creature icon 5 (not a player icon) marks a fiendish monster.
    return std::any_of(icons.begin(), icons.end(), [](const creatures::TCreatureIcon& icon) { return icon.icon == 5 && icon.kind != 0; });
}

void TCreatureHUDQmlRenderInfo::updateOtherIcons()
{
    QList<TCreatureOtherIconModel::Icon> icons;
    for (const creatures::TCreatureIcon& icon : m_renderInfo.iconFlags.icons) {
        if (icon.icon == 0)
            continue;
        // Kind 0 is a player icon (the quest strip), any other a creature icon.
        const QString image = stripIcon(icon.kind == 0 ? "otherplayericons" : "othercreatureicons", icon.icon);
        // A flagged icon never shows its value; a value of 0 shows only on a player icon.
        const bool showValue = !icon.flag && (icon.value != 0 || icon.kind == 0);
        icons.append({ image, icon.value, showValue });
    }
    m_otherIconModel->setIcons(icons);
}

void TCreatureHUDQmlRenderInfo::setRenderInfo(const TCreatureHUDRenderInfo& renderInfo)
{
    const TCreatureHUDRenderInfo old = m_renderInfo;
    m_renderInfo = renderInfo;
    const TCreatureHUDRenderInfo& now = m_renderInfo;

    if (old.name != now.name)
        Q_EMIT nameChanged();
    if (old.healthPercent != now.healthPercent)
        Q_EMIT healthPercentChanged();
    if (old.manaPercent != now.manaPercent)
        Q_EMIT manaPercentChanged();
    if (old.manaShieldPercent != now.manaShieldPercent)
        Q_EMIT manaShieldPercentChanged();
    if (old.healthPercent != now.healthPercent || old.covered != now.covered || old.lightness != now.lightness)
        Q_EMIT healthColorChanged();
    if (old.showName != now.showName)
        Q_EMIT showCreatureNameChanged();
    if (old.showHealthBar != now.showHealthBar) {
        Q_EMIT healthPercentChanged();
        Q_EMIT healthColorChanged();
        Q_EMIT showHealthBarChanged();
    }
    if (old.showManaBar != now.showManaBar) {
        Q_EMIT manaPercentChanged();
        Q_EMIT showManaBarChanged();
    }
    if (old.showManaShieldBar != now.showManaShieldBar)
        Q_EMIT showManaShieldBarChanged();
    if (old.showBars != now.showBars)
        Q_EMIT showBarsChanged();
    if (old.playerStates != now.playerStates)
        m_playerStateModel->setEntries(now.playerStates);
    if (old.showSpecialConditions != now.showSpecialConditions)
        Q_EMIT showSpecialConditionsChanged();
    if (old.showArcs != now.showArcs || old.showCombopointsAndSereneLeft != now.showCombopointsAndSereneLeft
        || old.arcDistance != now.arcDistance || old.arcOpacity != now.arcOpacity || old.arcWidth != now.arcWidth
        || old.arcHeight != now.arcHeight)
        Q_EMIT showArcsChanged();
    if (old.showCombopointsAndSerene != now.showCombopointsAndSerene || old.comboPoints != now.comboPoints
        || old.isSerene != now.isSerene)
        Q_EMIT comboPointsOrSereneStateChanged();
    if (old.scaleFactor != now.scaleFactor)
        Q_EMIT scaleFactorChanged();
    if (!(old.iconFlags == now.iconFlags)) {
        updateOtherIcons();
        Q_EMIT horizontalCreatureIconsChanged();
        Q_EMIT verticalCreatureIconsChanged();
        Q_EMIT fiendishMonsterChanged();
    }
    if (old.statusMarks != now.statusMarks)
        Q_EMIT statusEffectsChanged();
    if (old.isLocalPlayer != now.isLocalPlayer)
        Q_EMIT isLocalPlayerChanged();
}

}
