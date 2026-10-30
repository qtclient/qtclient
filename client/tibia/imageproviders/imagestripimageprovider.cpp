#include "imagestripimageprovider.h"

#include "optimizedborderimageprovider.h"

#include <QCoreApplication>
#include <QMetaObject>
#include <QQmlEngine>
#include <QQuickImageResponse>
#include <QQuickTextureFactory>
#include <QThread>
#include <QTimer>

namespace tibia::imageproviders {

namespace {

int g_spellIconCount = 0;

class StripImageResponse final : public QQuickImageResponse
{
public:
    explicit StripImageResponse(QImage image) : m_image(std::move(image))
    {
        // requestImageResponse runs on the scene-graph thread. Keep this QObject
        // on the app thread so finished() does not create children under
        // QQmlEngine from QSGRenderThread.
        if (QCoreApplication* app = QCoreApplication::instance()) {
            moveToThread(app->thread());
            QMetaObject::invokeMethod(this, [this] { Q_EMIT finished(); }, Qt::QueuedConnection);
        } else {
            QTimer::singleShot(0, this, [this] { Q_EMIT finished(); });
        }
    }

    QQuickTextureFactory* textureFactory() const override { return QQuickTextureFactory::textureFactoryForImage(m_image); }

private:
    QImage m_image;
};

}

void TImagestrip::load() const
{
    std::call_once(m_loaded, [this] {
        m_image = QImage(m_file);
        if (m_image.isNull())
            qWarning("image strip %s failed to load", qPrintable(m_file));
        m_cellHeight = m_image.height();
        if (m_cellWidth <= 0)
            m_cellWidth = m_image.height();
    });
}

QImage TImagestrip::cell(const QString& id) const
{
    load();
    bool ok = false;
    const uint index = id.toUInt(&ok);
    if (ok && m_cellWidth > 0 && (qint64(index) + 1) * m_cellWidth <= m_image.width())
        return m_image.copy(int(index) * m_cellWidth, 0, m_cellWidth, m_cellHeight);
    QImage empty(std::max(m_cellWidth, 0), m_cellHeight, QImage::Format_ARGB32_Premultiplied);
    empty.fill(Qt::transparent);
    return empty;
}

int TImagestrip::cellCount() const
{
    load();
    return m_cellWidth > 0 ? m_image.width() / m_cellWidth : 0;
}

QImage TImagestripImageProvider::requestImage(const QString& id, QSize* size, const QSize&)
{
    QImage image = m_strip.cell(id);
    if (size)
        *size = image.size();
    return image;
}

QQuickImageResponse* TImagestripRichTextImageProvider::requestImageResponse(const QString& id, const QSize&)
{
    return new StripImageResponse(m_strip.cell(id));
}

void registerImageProviders(QQmlEngine* engine)
{
    struct Strip { const char* name; const char* file; int cellWidth; };
    static constexpr Strip strips[] = {
        { "creaturestateflags-npc-categories", ":/images/creatures/hud/flags/npc-categories.png", 0 },
        { "creaturestateflags-summon", ":/images/creatures/hud/flags/summon.png", 0 },
        { "creaturestateflags-playerkiller", ":/images/creatures/hud/flags/playerkiller.png", 0 },
        { "creaturestateflags-party", ":/images/creatures/hud/flags/party.png", 0 },
        { "creaturestateflags-guild", ":/images/creatures/hud/flags/guild.png", 0 },
        { "creaturestateflags-otherplayericons", ":/images/creatures/hud/flags/quests.png", 0 },
        { "creaturestateflags-othercreatureicons", ":/images/creatures/hud/flags/modifications.png", 0 },
        { "playerstate-flags", ":/images/creatures/player-state-flags.png", 0 },
        { "playerstate-flags-client", ":/images/creatures/player-state-flags-client.png", 0 },
        { "playerstate-flags-rottenblood", ":/images/creatures/player-state-flags-rotten-blood.png", 0 },
        { "playerstate-playerkiller-flags", ":/images/creatures/player-state-playerkiller-flags.png", 0 },
        { "playerstate-guildwar-flag", ":/images/creatures/player-state-guildwar-flag.png", 0 },
        { "minimap-markers", ":/images/minimap/markers.png", 0 },
        { "vip-icons", ":/images/creatures/vip-icons.png", 0 },
        { "spellgroup-icons-20x20", ":/images/spells/spellgroup-icons-20x20.png", 0 },
        { "premium-icons-20x20", ":/images/premium/premium-icons-20x20.png", 0 },
        { "premium-icons-32x32", ":/images/premium/premium-icons-32x32.png", 0 },
        { "imbuement-icons", ":/images/imbuing/imbuement-icons.png", 0 },
        { "tutorial-mouse-hint-icons", ":/images/tutorial/tutorial-mouse-hint-icons.png", 0 },
        { "blessings-icons", ":/images/blessings/blessings-icons.png", 0 },
        { "friendbadge-icons", ":/images/social/friend-badge-icons.png", 0 },
        { "spell-icons-20x20", ":/images/spells/spell-icons-20x20.png", 0 },
        { "spell-icons-32x32", ":/images/spells/spell-icons-32x32.png", 0 },
        { "passiveability-icons-32x32", ":/images/passiveabilities/passiveability-icons-32x32.png", 0 },
        { "action-bar-cooldown", ":/images/spells/action-bar-cooldown.png", 0 },
        { "store-button-animation", ":/animations/button-store-animation.png", 106 },
        { "purchasecomplete-idle-animation", ":/animations/animation-purchasecomplete-idle.png", 108 },
        { "purchasecomplete-pressed-animation", ":/animations/animation-purchasecomplete-pressed.png", 108 },
        { "dragonheader-animation", ":/animations/animation-dragonheader.png", 192 },
        { "fire-bowl-animation", ":/animations/animation-fire-bowl.png", 32 },
        { "resting-area-bonuses-levels", ":/images/dailyreward/icon-restingareabonuseslevels.png", 0 },
        { "monster-bonus-effects", ":/images/monster-bonus-effects.png", 0 },
        { "monster-classifications", ":/images/monster-classifications.png", 0 },
        { "map-layers", ":/images/automap-indicator-maplayers.png", 14 },
        { "skills-icons", ":/images/icons-skills.png", 0 },
        { "containerslot-glowingborders", ":/images/containerslot-glowingborders.png", 0 },
        { "containerslot-coloredges", ":/images/containerslot-coloredges.png", 0 },
        { "charactertrade-categories", ":/images/icons-charactertrade-highlights.png", 0 },
        { "upgrade-tiers", ":/images/tiers-strip.png", 9 },
        { "upgrade-tiers-big", ":/images/tiers-strip-big.png", 18 },
        { "icons-skillwheel-mediumperks", ":/images/skillwheel/icons-skillwheel-mediumperks.png", 0 },
        { "icons-skillwheel-smallperks", ":/images/skillwheel/icons-skillwheel-smallperks.png", 0 },
        { "icons-skillwheel-largeperks", ":/images/skillwheel/icons-skillwheel-largeperks.png", 0 },
        { "icons-skillwheel-gemvariants", ":/images/skillwheel/icons-gematelier-gemvariants.png", 0 },
        { "icons-skillwheel-sockets", ":/images/skillwheel/icons-skillwheel-sockets.png", 0 },
        { "icons-skillwheel-gem-domainaffinity", ":/images/skillwheel/icons-gematelier-domainaffinity.png", 0 },
        { "icons-skillwheel-gem-basicmods", ":/images/skillwheel/icons-skillwheel-basicmods.png", 0 },
        { "icons-skillwheel-gem-suprememods", ":/images/skillwheel/icons-skillwheel-suprememods.png", 0 },
        { "backdrop-mod-grade", ":/images/skillwheel/backdrop_modgrades.png", 0 },
        { "overlay-mod-grade-potential", ":/images/skillwheel/icons-modgrades-potential.png", 0 },
        { "backdrop-grades-circle-bottom", ":/images/skillwheel/backdrop_grades_circle_bottom_anim.png", 0 },
        { "backdrop-grades-circle-mid", ":/images/skillwheel/backdrop_grades_circle_mid_anim.png", 0 },
        { "backdrop-grades-circle-top", ":/images/skillwheel/backdrop_grades_circle_top_anim.png", 0 },
        { "backdrop-grades-line", ":/images/skillwheel/backdrop_grades_line_anim.png", 4 },
        { "vocation-icons", ":/images/skin/classic/icons-vocations.png", 0 },
        { "proficiency-icons", ":/images/proficiency/icons-weaponmastery.png", 0 },
        { "proficiency-icons-critdamage-elements", ":/images/proficiency/icons-weaponmastery-criticalExtraDamageElement.png", 0 },
        { "proficiency-icons-critchance-elements", ":/images/proficiency/icons-weaponmastery-criticalHitChanceElement.png", 0 },
        { "proficiency-icons-monsterclasses", ":/images/proficiency/icons-weaponmastery-damagaAgainstBestiary.png", 0 },
        { "proficiency-icons-elemental-pierce", ":/images/proficiency/icons-weaponmastery-elementalPiercing.png", 0 },
        { "proficiency-icons-homing-missile", ":/images/proficiency/icons-weaponmastery-homingMissile.png", 0 },
        { "proficiency-icons-skilllevel-autoattack", ":/images/proficiency/icons-weaponmastery-gainExtraDamageAutoAttack.png", 0 },
        { "proficiency-icons-skilllevel-spells", ":/images/proficiency/icons-weaponmastery-gainExtraDamageSpells.png", 0 },
        { "proficiency-icons-skilllevel-healing", ":/images/proficiency/icons-weaponmastery-gainExtraHealingSpells.png", 0 },
        { "proficiency-icons-skills", ":/images/proficiency/icons-weaponmastery-offensiveBonusSkill.png", 0 },
        { "proficiency-icons-specialized-magic-level", ":/images/proficiency/icon-weaponsmastery-specializedMagicLevel.png", 0 },
        { "backdrop-weaponmastery-highlight", ":/images/proficiency/backdrop_weaponmastery_highlight.png", 0 },
        { "backdrop-weaponmastery-shaped-highlight", ":/images/proficiency/backdrop_weaponmastery_manipulate_highlight.png", 0 },
        { "icons-npcdialog", ":/images/icons-npcdialog.png", 0 },
    };
    for (const Strip& strip : strips) {
        auto* provider = new TImagestripImageProvider(QString::fromLatin1(strip.file), strip.cellWidth);
        if (qstrcmp(strip.name, "spell-icons-20x20") == 0)
            g_spellIconCount = provider->strip().cellCount();
        engine->addImageProvider(QLatin1String(strip.name), provider);
    }
    engine->addImageProvider(QStringLiteral("store-description-icons"),
                             new TImagestripRichTextImageProvider(QStringLiteral(":/images/store-icons-inline.png")));
    engine->addImageProvider(QLatin1String(qmlcomponents::OptimizedBorderImageProvider::Id), new qmlcomponents::OptimizedBorderImageProvider);
}

int spellIconCount() { return g_spellIconCount; }

}
