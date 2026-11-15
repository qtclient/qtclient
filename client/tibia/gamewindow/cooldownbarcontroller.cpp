#include "cooldownbarcontroller.h"

#include "app/clientoptions.h"
#include "imageproviders/imagestripimageprovider.h"
#include "spells/spellinfostorage.h"

#include <QQmlEngine>
#include <QQuickItem>
#include <QVariantList>

#include <algorithm>

namespace tibia::gamewindow {

namespace {

constexpr auto CooldownBarEnabled = "cooldownBarEnabled";

/// The spell groups on the bar, in order (conjure, group 5, is left out).
constexpr int BarGroups[] = { 1, 2, 3, 4, 6, 7, 8, 9, 10, 11 };

/// Crystal's SpellGroup_t numbers great beams and bursts of nature the other
/// way round than the client's spell groups.
int serverGroup(const int group)
{
    if (group == 9)
        return 10;
    if (group == 10)
        return 9;
    return group;
}

/// image://spellgroup-icons-20x20/<group - 1>; no icon for group 0.
QString spellGroupIconUrl(const int group)
{
    if (group == 0)
        return {};
    return QStringLiteral("image://%1/%2").arg(QLatin1String("spellgroup-icons-20x20")).arg(group - 1);
}

QString spellGroupName(const int group)
{
    switch (group) {
        case 1: return qtTrId("spell_attack");
        case 2: return qtTrId("spell_healing");
        case 3: return qtTrId("spell_support");
        case 4: return qtTrId("spellcooldown_spell_special");
        case 5: return qtTrId("spellcooldown_spell_conjure");
        case 6: return qtTrId("spellcooldown_spell_crippling");
        case 7: return qtTrId("spellcooldown_spell_focus");
        case 8: return qtTrId("spellcooldown_spell_ultimate_strikes");
        case 9: return qtTrId("spellcooldown_spell_great_beams");
        case 10: return qtTrId("spellcooldown_spell_bursts_of_nature");
        case 11: return qtTrId("spellcooldown_spell_stance");
        default: return {};
    }
}

QString spellIconUrl(const int iconIndex)
{
    return QStringLiteral("image://%1/%2").arg(QLatin1String("spell-icons-20x20")).arg(iconIndex);
}

bool isValidSpellIcon(const int iconIndex)
{
    return iconIndex >= 0 && iconIndex < imageproviders::spellIconCount();
}

/// The spell's own cooldown from the spell table, in milliseconds.
int spellCooldownMs(const spells::TSpellInfo& spell)
{
    return spell.cooldownSelf * 1000;
}

/// QML owns the items: they live as long as the list it shows.
QObject* qmlOwned(TCooldownProgressDataForQML* data)
{
    QQmlEngine::setObjectOwnership(data, QQmlEngine::JavaScriptOwnership);
    return data;
}

}

TCooldownProgressDataForQML::TCooldownProgressDataForQML(QString sourceUrl, const int remainingMs, const int totalMs, QString tooltip)
    : cip::tibia::gamewindow::TCooldownProgressDataForQML(nullptr)
{
    m_sourceURL = std::move(sourceUrl);
    m_remainingDurationMilliseconds = remainingMs;
    m_totalDurationMilliseconds = totalMs;
    m_tooltip = std::move(tooltip);
}

TCooldownBarController::TCooldownBarController(QQuickItem* gameWindowItem, QObject* parent)
    : cip::tibia::gamewindow::TCooldownBarController(parent)
{
    if (gameWindowItem)
        m_bar = gameWindowItem->findChild<QQuickItem*>(QLatin1String("cooldownBar"));
    if (!m_bar)
        qWarning("gamewindow.qml has no cooldownBar");

    showIdleSpellGroups();

    applyVisibility();
    connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged, this, [this](const QString& name) {
        if (name == QLatin1String(CooldownBarEnabled))
            applyVisibility();
    });

    m_nextFrame.setSingleShot(true);
    m_nextFrame.setInterval(0);
    connect(&m_nextFrame, &QTimer::timeout, this, [this] { update(game::TCooldownStorage::now()); });
    if (game::TCooldownStorage* storage = game::TCooldownStorage::instance())
        connect(storage, &game::TCooldownStorage::spellOrSpellGroupCooldownChanged, this, &TCooldownBarController::markDirty);
}

void TCooldownBarController::showIdleSpellGroups()
{
    QVariantList groups;
    for (const int group : BarGroups)
        groups.append(QVariant::fromValue(qmlOwned(new TCooldownProgressDataForQML(spellGroupIconUrl(group), 0, 1, spellGroupName(group)))));
    if (m_bar)
        m_bar->setProperty("spellGroupCooldowns", groups);
}

QString TCooldownBarController::spellTooltip(const int spellId)
{
    const spells::TSpellInfo* spell = spells::TSpellInfoStorage::instance()->spell(spellId);
    if (!spell)
        return {};
    return qtTrId("spellcooldown_individual_spell").arg(spell->name).arg(spellCooldownMs(*spell) / 1000);
}

void TCooldownBarController::update(const qint64 now)
{
    game::TCooldownStorage* storage = game::TCooldownStorage::instance();
    if (!m_dirty || !m_bar || !storage)
        return;

    QVariantList groups;
    for (const int group : BarGroups) {
        const game::TCooldown cooldown = storage->groupCooldown(serverGroup(group));
        const qint64 remaining = std::max<qint64>(0, cooldown.end - now);
        const qint64 total = std::max<qint64>(1, cooldown.duration);
        groups.append(QVariant::fromValue(
            qmlOwned(new TCooldownProgressDataForQML(spellGroupIconUrl(group), int(remaining), int(total), spellGroupName(group)))));
    }
    m_bar->setProperty("spellGroupCooldowns", groups);

    QVariantList spellCooldowns;
    spells::TSpellInfoStorage* spellInfo = spells::TSpellInfoStorage::instance();
    for (const auto& [spellId, cooldown] : storage->spellCooldowns()) {
        const qint64 remaining = cooldown.end - now;
        const spells::TSpellInfo* spell = spellInfo->spell(spellId);
        if (remaining <= 0 || !spell || !isValidSpellIcon(spell->iconIndex))
            continue;
        spellCooldowns.append(QVariant::fromValue(qmlOwned(new TCooldownProgressDataForQML(
            spellIconUrl(spell->iconIndex), int(remaining), spellCooldownMs(*spell), spellTooltip(spellId)))));
    }
    m_bar->setProperty("spellCooldowns", spellCooldowns);
    m_dirty = false;
}

void TCooldownBarController::markDirty()
{
    m_dirty = true;
    if (!m_nextFrame.isActive())
        m_nextFrame.start();
}

void TCooldownBarController::applyVisibility()
{
    if (!m_bar)
        return;
    m_bar->setVisible(app::ClientOptions::load().options().value(QLatin1String(CooldownBarEnabled)).toBool(true));
}

}
