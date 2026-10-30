#include "creaturehud.h"

#include "app/clientoptions.h"
#include "renderer/mapscale.h"
#include "renderer/worldmaprenderprovider.h"

#include <tibia/world/world.h>
#include <client/protocolcodes.h>

#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>

#include <cmath>

namespace tibia::worldmap {

namespace {

using Provider = qmlcomponents::TWorldMapRenderProvider;

template <class T, class Owner, class Signal>
void assign(Owner* owner, T& member, const T& value, Signal signal)
{
    if (member == value)
        return;
    member = value;
    Q_EMIT(owner->*signal)();
}

bool option(const QJsonObject& options, const char* name, const bool fallback = true)
{
    return options.value(QLatin1String(name)).toBool(fallback);
}

double numberOption(const QJsonObject& options, const char* name, const double fallback)
{
    return options.value(QLatin1String(name)).toDouble(fallback);
}

double fraction(const uint32_t value, const uint32_t max)
{
    return max ? std::min(1.0, static_cast<double>(value) / max) : 0.0;
}

/// FUN_100925970 and siblings: one strip cell per value, none for 0.
QString stripIcon(const char* provider, const uint8_t value)
{
    return value ? QStringLiteral("image://creaturestateflags-%1/%2").arg(QLatin1String(provider)).arg(value - 1) : QString();
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
    , m_otherIcons(new TCreatureOtherIconModel(this))
{
    m_lightness = 1.f;
    m_scaleFactor = 1.f;
    m_creatureOtherIcons = m_otherIcons;
}

/// FUN_101067510 (horizontal), FUN_101067820 (vertical), the other-icon model
/// and FUN_101067a00 (fiendish), all read from the state flags at info +0x98.
/// An NPC category is the only horizontal icon; otherwise party, skull, summon.
/// Guild emblems are the vertical list. Other icons map kind 0 to the quest
/// strip and any other kind to the modification strip (FUN_100b13360).
/// The flags are built per creature type (CIP 0x101064c00): players get guild,
/// party, skull and other icons; monsters other icons; NPCs their category;
/// summons the summon flag and party. All but the NPC category (gated on
/// creatueShowNpcIcon) hang on the marks option, playerShowMarks for the own
/// character and creatureShowMarks for everyone else.
void TCreatureHUDQmlRenderInfo::updateIcons(Creature& creature, const QJsonObject& options)
{
    using Info = cip::tibia::worldmap::TCreatureHUDQmlRenderInfo;

    enum class Kind { Player, Monster, Npc, Summon };
    const bool summonType = creature.getType() == Proto::CreatureTypeSummonOwn || creature.getType() == Proto::CreatureTypeSummonOther;
    const Kind kind = creature.isNpc() ? Kind::Npc : summonType ? Kind::Summon : creature.isPlayer() ? Kind::Player : Kind::Monster;
    const bool marks = kind == Kind::Npc ? option(options, "creatueShowNpcIcon")
                                         : option(options, creature.isLocalPlayer() ? "playerShowMarks" : "creatureShowMarks");

    uint8_t summon = 0, npcCategory = 0, guild = 0, party = 0, skull = 0;
    bool withOthers = false;
    if (marks) {
        switch (kind) {
            case Kind::Player:
                guild = creature.getEmblem();
                party = creature.getShield();
                skull = creature.getSkull();
                withOthers = true;
                break;
            case Kind::Monster: withOthers = true; break;
            case Kind::Npc: npcCategory = creature.getIcon(); break;
            case Kind::Summon:
                summon = creature.getType() == Proto::CreatureTypeSummonOwn ? 1 : 2;
                party = creature.getShield();
                break;
        }
    }

    QStringList horizontal;
    if (npcCategory) {
        horizontal.append(stripIcon("npc-categories", npcCategory));
    } else {
        for (const QString& icon : { stripIcon("party", party), stripIcon("playerkiller", skull), stripIcon("summon", summon) }) {
            if (!icon.isEmpty())
                horizontal.append(icon);
        }
    }
    assign(this, m_horizontalCreatureIcons, horizontal, &Info::horizontalCreatureIconsChanged);

    QStringList vertical;
    if (const QString emblem = stripIcon("guild", guild); !emblem.isEmpty())
        vertical.append(emblem);
    assign(this, m_verticalCreatureIcons, vertical, &Info::verticalCreatureIconsChanged);

    QList<TCreatureOtherIconModel::Icon> others;
    bool fiendish = false;
    if (withOthers) {
        for (const auto& [icon, category, count] : creature.getIcons()) {
            others.append({ stripIcon(category == 0 ? "otherplayericons" : "othercreatureicons", icon), count, count > 0 });
            fiendish |= category != 0 && icon == 5;
        }
    }
    m_otherIcons->setIcons(others);
    assign(this, m_isFiendishMonster, fiendish, &Info::fiendishMonsterChanged);

    // CIP FUN_101067ab0 / FUN_101067b00: status-mark types 0/1. On product/Crystal
    // the Echo Raid glow wire (0x8B type 15) carries the same: 0=warden/leader,
    // 1=empowered/minion (see Monster::getEchoGlow / Creature::getEchoRaidColor).
    const int8_t echo = creature.getEchoRaidColor();
    const bool leader = echo == 0;
    const bool minion = echo == 1;
    assign(this, m_isLeaderMonster, leader, &Info::statusEffectsChanged);
    assign(this, m_isLeaderMinionMonster, minion, &Info::statusEffectsChanged);
}

void TCreatureHUDQmlRenderInfo::setShowSpecialConditions(const bool show)
{
    assign(this, m_showSpecialConditions, show, &cip::tibia::worldmap::TCreatureHUDQmlRenderInfo::showSpecialConditionsChanged);
}

void TCreatureHUDQmlRenderInfo::update(Creature& creature, float lightness, const float scaleFactor, const QJsonObject& options)
{
    using Info = cip::tibia::worldmap::TCreatureHUDQmlRenderInfo;
    const bool local = creature.isLocalPlayer();
    // CIP keeps the own character's HUD readable in the dark (at least 0x66).
    if (local)
        lightness = std::max(lightness, 0x66 / 255.f);

    assign(this, m_name, QString::fromStdString(creature.getName()), &Info::nameChanged);
    assign(this, m_healthPercent, creature.getHealthPercent() / 100.0, &Info::healthPercentChanged);

    const Color& base = creature.getInformationColor();
    const QColor color = QColor::fromRgbF(base.rF() * lightness, base.gF() * lightness, base.bF() * lightness);
    if (m_healthColor != color || m_lightness != lightness) {
        m_healthColor = color;
        m_lightness = lightness;
        Q_EMIT healthColorChanged();
    }
    assign(this, m_scaleFactor, scaleFactor, &Info::scaleFactorChanged);

    // CIP StatusBarController: vocation % 10 == 5 is Monk.
    constexpr int MonkProfession = 5;

    bool showName, showHealth, showMana = false, showManaShield = false, showBars = true, showArcs = false;
    bool showCombo = false, showComboLeft = false, serene = false;
    int combo = 0;
    if (local) {
        auto& player = static_cast<LocalPlayer&>(creature);
        const bool hud = option(options, "playerHudEnabled");
        showName = hud && option(options, "playerShowName");
        showHealth = hud && option(options, "playerShowHealth");
        showMana = hud && option(options, "playerShowMana");
        showManaShield = showMana && player.getMaxManaShield() > 0;
        showBars = option(options, "playerHudShowBars");
        showArcs = hud && option(options, "playerHudShowArcs");
        assign(this, m_manaPercent, fraction(player.getMana(), player.getMaxMana()), &Info::manaPercentChanged);
        assign(this, m_manaShieldPercent, fraction(player.getManaShield(), player.getMaxManaShield()), &Info::manaShieldPercentChanged);

        const double distance = numberOption(options, "playerHudArcDistance", 0.25);
        const double height = numberOption(options, "playerHudArcHeight", 0.21);
        const double opacity = numberOption(options, "playerHudArcOpacity", 1.0);
        const auto width = static_cast<uint>(numberOption(options, "playerHudArcWidth", 6));
        if (m_arcDistance != distance || m_arcHeight != height || m_arcOpacity != opacity || m_arcWidth != width) {
            m_arcDistance = distance;
            m_arcHeight = height;
            m_arcOpacity = opacity;
            m_arcWidth = width;
            Q_EMIT showArcsChanged();
        }

        const bool monk = player.getVocation() % 10 == MonkProfession;
        showCombo = hud && monk && option(options, "playerShowHarmonySerene");
        showComboLeft = showCombo && option(options, "playerHudShowHarmonyLeft");
        combo = showCombo ? static_cast<int>(player.getHarmony()) : 0;
        serene = showCombo && player.isSerene();
    } else {
        const bool hud = option(options, "creatureHudEnabled");
        showName = hud && option(options, "creatureShowName");
        // NPCs have shown no health since 11.00.
        showHealth = hud && option(options, "creatureShowHealth") && !(creature.isNpc() && tibia::world::clientVersion() >= 1100);
    }
    assign(this, m_showCreatureName, showName, &Info::showCreatureNameChanged);
    assign(this, m_showHealthBar, showHealth, &Info::showHealthBarChanged);
    if (m_showManaBar != showMana || m_showManaShieldBar != showManaShield) {
        m_showManaBar = showMana;
        m_showManaShieldBar = showManaShield;
        Q_EMIT showManaBarChanged();
    }
    assign(this, m_showBars, showBars, &Info::showBarsChanged);
    assign(this, m_showArcs, showArcs, &Info::showArcsChanged);
    if (m_showCombopointsAndSerene != showCombo || m_comboPoints != combo || m_isSerene != serene) {
        m_showCombopointsAndSerene = showCombo;
        m_comboPoints = combo;
        m_isSerene = serene;
        Q_EMIT comboPointsOrSereneStateChanged();
    }
    // Left placement shares the arcs notify (CIP contract).
    if (m_showCombopointsAndSereneLeft != showComboLeft) {
        m_showCombopointsAndSereneLeft = showComboLeft;
        Q_EMIT showArcsChanged();
    }
    updateIcons(creature, options);
}

TCreatureHUDOverlayController::TCreatureHUDOverlayController(QQuickItem* overlay, Provider* provider, QObject* parent)
    : cip::tibia::worldmap::TCreatureHUDOverlayController(parent)
    , m_overlay(overlay)
    , m_options(app::ClientOptions::load().options())
    , m_playerStates(config::SpecialConditionsFilter::HUD)
{
    if (QQmlEngine* engine = qmlEngine(overlay)) {
        m_component = new QQmlComponent(engine, QUrl(QStringLiteral("qrc:/qt/qml/qmlcomponents/qml/CreatureHUD.qml")),
                                        QQmlComponent::PreferSynchronous, this);
        if (m_component->isError())
            qWarning().noquote() << "CreatureHUD:" << m_component->errorString();
    }
    connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged, this,
            [this] { m_options = app::ClientOptions::load().options(); });
    setRenderProvider(provider);
}

TCreatureHUDOverlayController::~TCreatureHUDOverlayController()
{
    clear();
}

void TCreatureHUDOverlayController::setRenderProvider(Provider* provider)
{
    disconnect(m_frameConnection);
    m_provider = provider;
    if (provider)
        m_frameConnection = connect(provider, &Provider::overlaysAdvanced, this, &TCreatureHUDOverlayController::update);
    else
        clear();
}

void TCreatureHUDOverlayController::refreshCreatureHUDs()
{
    update();
}

TCreatureHUDOverlayController::Hud TCreatureHUDOverlayController::create()
{
    Hud hud;
    if (!m_component || !m_component->isReady())
        return hud;
    QObject* object = m_component->beginCreate(qmlContext(m_overlay));
    hud.item = qobject_cast<QQuickItem*>(object);
    if (!hud.item) {
        delete object;
        return hud;
    }
    hud.info = new TCreatureHUDQmlRenderInfo(hud.item);
    hud.info->setPlayerStates(m_playerStates.model());
    hud.item->setParentItem(m_overlay);
    hud.item->setProperty("creatureHUDInfo", QVariant::fromValue<QObject*>(hud.info));
    m_component->completeCreate();
    return hud;
}

void TCreatureHUDOverlayController::update()
{
    if (!m_overlay || !m_provider)
        return;
    const Provider::Frame& frame = m_provider->frame();
    if (!frame.valid) {
        clear();
        return;
    }

    // Overlay matches MapWindowPane cover scale (uniform, square pixels).
    const int MapWidth = frame.visibleWidth * Provider::FieldSize;
    const int MapHeight = frame.visibleHeight * Provider::FieldSize;
    const qmlcomponents::MapCoverScale cover = qmlcomponents::MapCoverScale::placed(
        m_overlay->width(), m_overlay->height(), MapWidth, MapHeight, frame.viewScale);
    const float hudScale = static_cast<float>(cover.scale);
    const QRect visible(frame.camera, QSize(MapWidth, MapHeight));

    for (Hud& hud : m_huds)
        hud.seen = false;

    for (const Provider::CreatureHud& record : frame.creatures) {
        const QRect field(record.point, QSize(Provider::FieldSize, Provider::FieldSize));
        if (!visible.intersects(field))
            continue;
        const uint32_t id = record.creature->getId();
        auto it = m_huds.find(id);
        if (it == m_huds.end()) {
            Hud hud = create();
            if (!hud.item)
                continue;
            it = m_huds.insert(id, hud);
        }
        Hud& hud = *it;
        hud.seen = true;

        const QPoint center = record.point + QPoint(Provider::FieldSize / 2, Provider::FieldSize / 2);
        hud.info->update(*record.creature, m_provider->lightnessAt(center), hudScale, m_options);
        hud.info->setShowSpecialConditions(record.creature->isLocalPlayer() && m_playerStates.options().showInHUD);

        // FUN_101066030: centred on the scaled field, standing on its top edge.
        const QPoint camera = frame.camera;
        const QPointF mapped = cover.toPane(record.point.x() - camera.x(), record.point.y() - camera.y());
        const double width = cover.scale * Provider::FieldSize;
        const QPointF position(std::floor(mapped.x() + width * 0.5 - 16 + 0.5), std::floor(mapped.y()));
        if (hud.item->position() != position)
            hud.item->setPosition(position);
        hud.item->setZ(record.creature->isLocalPlayer() ? 9999 : 0);
    }

    for (auto it = m_huds.begin(); it != m_huds.end();) {
        if (it->seen) {
            ++it;
            continue;
        }
        if (it->item)
            it->item->deleteLater();
        it = m_huds.erase(it);
    }
}

void TCreatureHUDOverlayController::clear()
{
    for (Hud& hud : m_huds) {
        if (hud.item)
            hud.item->deleteLater();
    }
    m_huds.clear();
}

}
