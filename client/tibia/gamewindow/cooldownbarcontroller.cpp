#include "cooldownbarcontroller.h"

#include "app/clientoptions.h"
#include "spells/spellinfostorage.h"

#include <QQmlEngine>
#include <QQuickItem>

#include <algorithm>

namespace tibia::gamewindow {

namespace {

constexpr auto CooldownBarEnabled = "cooldownBarEnabled";
constexpr int CooldownTickMs = 100;

/// CIP FUN_100c0aef0: groups shown on the bar (conjure / id 5 is omitted).
constexpr int BarGroups[] = { 1, 2, 3, 4, 6, 7, 8, 9, 10, 11 };

/// Crystal SpellGroup_t swaps great beams / bursts of nature vs CIP ids.
int swapGreatBeamsBursts(const int id)
{
    if (id == 9)
        return 10;
    if (id == 10)
        return 9;
    return id;
}

int remainingMs(const game::TCooldown& cooldown, const qint64 now)
{
    if (cooldown.end <= now)
        return 0;
    return int(std::min<qint64>(cooldown.end - now, cooldown.duration > 0 ? cooldown.duration : cooldown.end - now));
}

int totalMs(const game::TCooldown& cooldown)
{
    return cooldown.duration > 0 ? int(cooldown.duration) : 1;
}

void clearModels(QVariantList& models)
{
    for (const QVariant& value : models)
        if (QObject* object = value.value<QObject*>())
            delete object;
    models.clear();
}

}

TCooldownProgressDataForQML::TCooldownProgressDataForQML(QString sourceUrl, const int remainingMs, const int totalMs,
                                                       QString tooltip, QObject* parent)
    : cip::tibia::gamewindow::TCooldownProgressDataForQML(parent)
{
    m_sourceURL = std::move(sourceUrl);
    m_remainingDurationMilliseconds = remainingMs;
    m_totalDurationMilliseconds = totalMs > 0 ? totalMs : 1;
    m_tooltip = std::move(tooltip);
}

TCooldownBarController::~TCooldownBarController()
{
    clearModels(m_groupModels);
    clearModels(m_spellModels);
}

TCooldownBarController::TCooldownBarController(QQuickItem* gameWindowItem, QObject* parent)
    : cip::tibia::gamewindow::TCooldownBarController(parent)
{
    if (gameWindowItem)
        m_bar = gameWindowItem->findChild<QQuickItem*>(QLatin1String("cooldownBar"));
    if (!m_bar)
        qWarning("gamewindow.qml has no cooldownBar");

    applyVisibility();
    rebuildIdleGroups();
    if (m_bar)
        m_bar->setProperty("spellCooldowns", QVariantList{});

    connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged, this, [this](const QString& name) {
        if (name == QLatin1String(CooldownBarEnabled))
            applyVisibility();
    });

    m_tick.setInterval(CooldownTickMs);
    connect(&m_tick, &QTimer::timeout, this, &TCooldownBarController::refresh);

    if (game::TCooldownStorage* storage = game::TCooldownStorage::instance()) {
        connect(storage, &game::TCooldownStorage::spellOrSpellGroupCooldownChanged, this, &TCooldownBarController::refresh);
        connect(storage, &game::TCooldownStorage::multiUseCooldownChanged, this, &TCooldownBarController::refresh);
    }
}

void TCooldownBarController::applyVisibility()
{
    if (!m_bar)
        return;
    // CIP FUN_100b63d50: option 0x77.
    m_bar->setVisible(app::ClientOptions::load().options().value(QLatin1String(CooldownBarEnabled)).toBool(true));
}

QString TCooldownBarController::groupTooltip(const int cipGroup)
{
    // CIP 0x10102bcc0 / skillwheel spellGroupName (CIP ids).
    switch (cipGroup) {
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

QString TCooldownBarController::groupIconUrl(const int cipGroup)
{
    if (cipGroup <= 0)
        return {};
    // CIP FUN_100926790 / FUN_1009267b0: image://spellgroup-icons-20x20/<id-1>.
    return QStringLiteral("image://spellgroup-icons-20x20/%1").arg(cipGroup - 1);
}

void TCooldownBarController::rebuildIdleGroups()
{
    clearModels(m_groupModels);
    m_groupModels.reserve(std::size(BarGroups));
    for (const int group : BarGroups) {
        auto* data = new TCooldownProgressDataForQML(groupIconUrl(group), 0, 1, groupTooltip(group));
        QQmlEngine::setObjectOwnership(data, QQmlEngine::CppOwnership);
        m_groupModels.append(QVariant::fromValue(data));
    }
    if (m_bar)
        m_bar->setProperty("spellGroupCooldowns", m_groupModels);
}

void TCooldownBarController::refresh()
{
    game::TCooldownStorage* storage = game::TCooldownStorage::instance();
    if (!m_bar || !storage) {
        m_tick.stop();
        return;
    }

    const qint64 now = game::TCooldownStorage::now();
    clearModels(m_groupModels);
    m_groupModels.reserve(std::size(BarGroups));
    bool running = false;
    for (const int cipGroup : BarGroups) {
        const game::TCooldown cooldown = storage->groupCooldown(swapGreatBeamsBursts(cipGroup));
        const int remaining = remainingMs(cooldown, now);
        running = running || remaining > 0;
        auto* data = new TCooldownProgressDataForQML(groupIconUrl(cipGroup), remaining, remaining > 0 ? totalMs(cooldown) : 1,
                                                   groupTooltip(cipGroup));
        QQmlEngine::setObjectOwnership(data, QQmlEngine::CppOwnership);
        m_groupModels.append(QVariant::fromValue(data));
    }
    m_bar->setProperty("spellGroupCooldowns", m_groupModels);

    clearModels(m_spellModels);
    spells::TSpellInfoStorage* spellsInfo = spells::TSpellInfoStorage::instance();
    for (const auto& [spellId, cooldown] : storage->spellCooldowns()) {
        const int remaining = remainingMs(cooldown, now);
        if (remaining <= 0)
            continue;
        const spells::TSpellInfo* spell = spellsInfo->spell(spellId);
        if (!spell || spell->iconIndex < 0)
            continue;
        running = true;
        // CIP FUN_100c0bc40: spellcooldown_individual_spell %1 name %2 seconds.
        const QString tip = qtTrId("spellcooldown_individual_spell").arg(spell->name).arg((remaining + 999) / 1000);
        auto* data = new TCooldownProgressDataForQML(QStringLiteral("image://spell-icons-20x20/%1").arg(spell->iconIndex), remaining,
                                                   totalMs(cooldown), tip);
        QQmlEngine::setObjectOwnership(data, QQmlEngine::CppOwnership);
        m_spellModels.append(QVariant::fromValue(data));
    }
    m_bar->setProperty("spellCooldowns", m_spellModels);

    if (running && !m_tick.isActive())
        m_tick.start();
    else if (!running && m_tick.isActive())
        m_tick.stop();
}

}
