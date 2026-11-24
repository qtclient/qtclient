#include "monsterbonuseffectsdialogcontroller.h"

#include "game/gameciptypes.h"
#include "colorhelper.h"

#include <utility>

#include "cyclopedia/bestiarystorage.h"
#include "cyclopedia/cyclopediaciptypes.h"
#include "game/gamesessionbase.h"
#include "gamewindow/messagedialogcontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "qmlcomponents/texthelper.h"

#include <client/const.h>
#include <client/outfit.h>
#include <tibia/assets/things.h>

#include <QLocale>

#include <algorithm>
#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/tgamenotify.h>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;
using cyclopedia::TBestiaryStorage;
using cyclopedia::TCharm;

QString englishNumber(const qulonglong value) { return QLocale(QLocale::English).toString(value); }

using ::tibia::shared::outfitColor;

/// The charm points (major) or minor charm echoes (minor) of the player.
quint64 charmResource(const bool minor)
{
    if (!tibia::game::hasLocalPlayer())
        return 0;
    return tibia::game::localPlayerResourceBalance(minor ? tibia::client::RESOURCE_MINOR_CHARM
                                                         : tibia::client::RESOURCE_CHARM);
}

class TQmlMonsterBonusEffect final : public cip::tibia::gamewindow::TQmlMonsterBonusEffect
{
public:
    TQmlMonsterBonusEffect(const TCharm& charm, const quint64 goldBalance, QObject* parent)
        : cip::tibia::gamewindow::TQmlMonsterBonusEffect(parent)
    {
        m_id = charm.id;
        m_name = charm.name();
        m_description = bonusEffectDescription(charm);
        m_imageSource = cyclopedia::charmImageSource(charm.id);
        m_isMinor = charm.isMinor();
        const int nextCost = charm.nextGradeCost();
        m_resourceCostString = qmlcomponents::thousandShortcutsMultiString(nextCost);
        m_notEnoughResource = charmResource(m_isMinor) < static_cast<quint64>(nextCost);
        m_clearCostString = qmlcomponents::thousandShortcutsMultiString(charm.removeCost);
        m_notEnoughGold = goldBalance < charm.removeCost;
        m_unlocked = charm.grade > 0;
        m_unlockGrade = static_cast<cip::tibia::qmlcomponents::enums::ECharmUnlockGrade>(charm.grade >= 1 && charm.grade <= 3 ? charm.grade : 0);
        m_assigned = charm.raceId != 0;
        m_assignedMonsterRaceID = charm.raceId;
        // Without an assigned creature the outfit colours read black.
        m_creatureHeadColor = QColor(Qt::black);
        m_creatureTorsoColor = QColor(Qt::black);
        m_creatureLegsColor = QColor(Qt::black);
        m_creatureDetailColor = QColor(Qt::black);
        if (m_assigned) {
            const Outfit& outfit = tibia::appearances::TAppearancesManager::instance().getRaceData(charm.raceId).outfit;
            m_hasAppearanceInstance = outfit.getId() != 0;
            m_creatureOutfitId = outfit.getId();
            m_creatureHeadColor = outfitColor(outfit.getHead());
            m_creatureTorsoColor = outfitColor(outfit.getBody());
            m_creatureLegsColor = outfitColor(outfit.getLegs());
            m_creatureDetailColor = outfitColor(outfit.getFeet());
            m_creatureFirstAddOn = (outfit.getAddons() & 1) != 0;
            m_creatureSecondAddOn = (outfit.getAddons() & 2) != 0;
        }
        // CIP .
        if (charm.grade >= 3)
            m_upgradeButtonText = qtTrId("monster_bonus_effects_fully_unlocked_button_text");
        else if (charm.grade > 0)
            m_upgradeButtonText = qtTrId("monster_bonus_effects_upgrade_button_text")
                                      .arg(cyclopedia::charmPercent(charm.id, charm.grade + 1) / 100.0, 0, 'g');
        else
            m_upgradeButtonText = qtTrId("monster_bonus_effects_not_unlocked_button_text");
    }

    /// A charm not unlocked yet describes its first grade; an unlocked one the
    /// description the charm message was read with (its current grade).
    static QString bonusEffectDescription(const TCharm& charm)
    {
        if (charm.grade == 0)
            return tableDescription(charm);
        return cyclopedia::charmDescription(charm.id, charm.grade);
    }

    /// The charm table's description at the grade after the charm's own,
    /// capped to the last grade.
    static QString tableDescription(const TCharm& charm)
    {
        return cyclopedia::charmDescription(charm.id, std::min(charm.grade, 2) + 1);
    }
};

class TQmlMonsterInfo final : public cip::tibia::gamewindow::TQmlMonsterInfo
{
public:
    TQmlMonsterInfo(const int id, const QString& name, QObject* parent)
        : cip::tibia::gamewindow::TQmlMonsterInfo(parent)
    {
        m_id = id;
        m_name = name;
    }
};

void deleteAll(QList<QObject*>& objects)
{
    for (QObject* object : objects)
        object->deleteLater();
    objects.clear();
}

}

TMonsterBonusEffectsDialogController::TMonsterBonusEffectsDialogController(QObject* parent)
    : cip::tibia::gamewindow::TMonsterBonusEffectsDialogController(parent)
    , m_confirmDialog(new TMessageDialogController(this))
{
    m_temporarySelectedMonsterIndex = -1;
    m_confirmDialog->setYesNoButtons();
    m_confirmDialog->setCloseOnButtonClicked(true);
    connect(m_confirmDialog, &TMessageDialogController::acceptClicked, this,
            &TMonsterBonusEffectsDialogController::onConfirmDialogAccepted);
    // Host: the player's gold and charm points are the local player's
    // resource balances (CipSoft: the player's and the charm storage's signals).
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::resourcesBalanceChanged, this,
            [this](qint64, qint64, int type) {
        if (type == static_cast<int>(tibia::client::RESOURCE_BANK_BALANCE) || type == static_cast<int>(tibia::client::RESOURCE_GOLD_EQUIPPED)) {
            onPlayerDataChanged();
        } else if (type == static_cast<int>(tibia::client::RESOURCE_CHARM) || type == static_cast<int>(tibia::client::RESOURCE_MINOR_CHARM)) {
            // The charm points: the list's resource flags.
            if (m_bonusEffectStorage)
                onMonsterBonusEffectsChanged();
        }
    });
}

void TMonsterBonusEffectsDialogController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    for (const QMetaObject::Connection& connection : m_sessionConnections)
        disconnect(connection);
    m_sessionConnections.clear();
    m_confirmDialog->dialog()->close();
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(GameSession.get());
    m_bonusEffectStorage = session ? session->monsterBonusEffectStorage() : nullptr;
    m_monsterRaceStorage = session ? session->monsterRaceStorage() : nullptr;
    if (!m_bonusEffectStorage || !m_monsterRaceStorage)
        return;
    using BonusEffects = cip::tibia::cyclopedia::TMonsterBonusEffectStorage;
    using Races = cip::tibia::cyclopedia::TMonsterRaceStorage;
    const BonusEffects* bonusEffects = m_bonusEffectStorage.get();
    m_sessionConnections.push_back(connect(bonusEffects, &BonusEffects::bonusEffectsChanged, this,
                                           &TMonsterBonusEffectsDialogController::onMonsterBonusEffectsChangedFromStorage));
    // The charm points and echoes: the list's resource flags.
    for (const auto signal : { &BonusEffects::availableCharmPointsChanged, &BonusEffects::unlockedCharmPointsChanged,
                               &BonusEffects::availableMinorCharmEchoesChanged, &BonusEffects::unlockedMinorCharmEchoesChanged }) {
        m_sessionConnections.push_back(connect(bonusEffects, signal, this, [this] {
            if (m_bonusEffectStorage)
                onMonsterBonusEffectsChanged();
        }));
    }
    m_sessionConnections.push_back(connect(bonusEffects, &BonusEffects::remainingNumberOfAssignableBonusEffectsChanged, this,
                                           &TMonsterBonusEffectsDialogController::onRemainingNumberOfAssignableBonusEffectsChanged));
    m_sessionConnections.push_back(connect(m_monsterRaceStorage.get(), &Races::monsterRacesUnlockedForCharmsChanged, this,
                                           &TMonsterBonusEffectsDialogController::rebuildMonsterInfos));
    m_sessionConnections.push_back(connect(bonusEffects, &BonusEffects::resetAllCharmCostChanged, this,
                                           &TMonsterBonusEffectsDialogController::playerDataChanged));
}

QList<TCharm> TMonsterBonusEffectsDialogController::charms() const
{
    QList<TCharm> charms;
    if (!m_bonusEffectStorage)
        return charms;
    for (const auto& [id, effect] : m_bonusEffectStorage->bonusEffects()) {
        TCharm& charm = charms.emplace_back();
        charm.id = effect.id;
        charm.grade = effect.tier;
        charm.raceId = effect.raceId;
        charm.removeCost = effect.removeCost;
    }
    return charms;
}

std::optional<TCharm> TMonsterBonusEffectsDialogController::charm(const int charmId) const
{
    for (const TCharm& charm : charms()) {
        if (charm.id == charmId)
            return charm;
    }
    return std::nullopt;
}

TMonsterBonusEffectsDialogController::~TMonsterBonusEffectsDialogController()
{
    // CIP: without its session the bonus effect list is rebuilt empty.
    QList<QObject*> previous = std::exchange(m_monsterBonusEffects, {});
    Q_EMIT qmlMonsterBonusEffectsChanged();
    deleteAll(previous);
}

TCyclopediaDialogContext TMonsterBonusEffectsDialogController::captureContext() const
{
    return TMonsterBonusEffectCyclopediaContext{ m_selectedCharm, m_temporarySelectedMonsterIndex };
}

void TMonsterBonusEffectsDialogController::restoreContext(const TCyclopediaDialogContext& CyclopediaContext)
{
    const auto& context = cyclopediaContextAs<TMonsterBonusEffectCyclopediaContext>(CyclopediaContext);
    if (m_selectedCharm != context.charmId) {
        m_selectedCharm = context.charmId;
        Q_EMIT qmlSelectedBonusEffectIdChanged();
    }
    if (m_temporarySelectedMonsterIndex == context.temporarySelectedMonsterIndex)
        return;
    m_temporarySelectedMonsterIndex = context.temporarySelectedMonsterIndex;
    Q_EMIT qmlTemporarySelectedMonsterIndexChanged();
}

void TMonsterBonusEffectsDialogController::selectCharm(const int charmId)
{
    const TCharm probe{ charmId };
    setActiveTab(probe.isMinor() ? 1 : 0);
    onMonsterBonusEffectsChangedFromStorage();
    rebuildMonsterInfos();
    m_pendingCharm = charmId;
}

int TMonsterBonusEffectsDialogController::selectedBonusEffectId() const
{
    for (int i = 0; i < m_monsterBonusEffects.size(); ++i) {
        const auto* effect = qobject_cast<const cip::tibia::gamewindow::TQmlMonsterBonusEffect*>(m_monsterBonusEffects[i]);
        if (effect && effect->id() == m_selectedCharm)
            return i;
    }
    return 0;
}

void TMonsterBonusEffectsDialogController::setSelectedBonusEffectFromIndex(const int BonusEffectIndex)
{
    if (BonusEffectIndex < 0 || BonusEffectIndex >= m_monsterBonusEffects.size())
        return;
    const auto* effect = qobject_cast<const cip::tibia::gamewindow::TQmlMonsterBonusEffect*>(m_monsterBonusEffects[BonusEffectIndex]);
    if (!effect || effect->id() == m_selectedCharm)
        return;
    m_selectedCharm = effect->id();
    Q_EMIT qmlSelectedBonusEffectIdChanged();
}

void TMonsterBonusEffectsDialogController::setTemporarySelectedMonsterRaceID(const int v)
{
    if (m_temporarySelectedMonsterRaceID == v)
        return;
    m_temporarySelectedMonsterRaceID = v;
    const Outfit outfit = v != 0 ? tibia::appearances::TAppearancesManager::instance().getRaceData(v).outfit : Outfit();
    m_hasTemporarySelectedMonsterAppearanceInstance = v != 0 && outfit.getId() != 0;
    m_temporarySelectedMonsterOutfitId = outfit.getId();
    m_temporarySelectedMonsterHeadColor = outfitColor(outfit.getHead());
    m_temporarySelectedMonsterTorsoColor = outfitColor(outfit.getBody());
    m_temporarySelectedMonsterLegsColor = outfitColor(outfit.getLegs());
    m_temporarySelectedMonsterDetailColor = outfitColor(outfit.getFeet());
    m_temporarySelectedMonsterFirstAddOn = (outfit.getAddons() & 1) != 0;
    m_temporarySelectedMonsterSecondAddOn = (outfit.getAddons() & 2) != 0;
    Q_EMIT qmlTemporarySelectedMonsterOutfitChanged();
}

void TMonsterBonusEffectsDialogController::setUnassignedMonstersNameFilter(const QString& v)
{
    if (m_unassignedMonstersNameFilter == v)
        return;
    m_unassignedMonstersNameFilter = v;
    Q_EMIT qmlUnassignedMonstersNameFilterChanged();
    rebuildMonsterInfos();
}

void TMonsterBonusEffectsDialogController::showDialogTab()
{
    if (!m_bonusEffectStorage)
        return;
    // Crystal answers the bestiary request with the charm data too.
    tibia::game::requestBestiary();
}

void TMonsterBonusEffectsDialogController::hideDialogTab() { m_selectedCharm = -1; }

void TMonsterBonusEffectsDialogController::onMonsterBonusEffectsChangedFromStorage()
{
    onMonsterBonusEffectsChanged();
    rebuildMonsterInfos();
    if (!m_pendingCharm)
        return;
    const int charmId = *m_pendingCharm;
    for (int i = 0; i < m_monsterBonusEffects.size(); ++i) {
        const auto* effect = qobject_cast<const cip::tibia::gamewindow::TQmlMonsterBonusEffect*>(m_monsterBonusEffects[i]);
        if (effect && effect->id() == charmId) {
            setSelectedBonusEffectFromIndex(i);
            break;
        }
    }
    m_pendingCharm.reset();
}

void TMonsterBonusEffectsDialogController::onMonsterBonusEffectsChanged()
{
    QList<TCharm> charms;
    const bool minorTab = m_activeTab == 1;
    const QList<TCharm> all = this->charms();
    if (m_bonusEffectStorage)
        m_goldBalance = TBestiaryStorage::goldBalance();
    for (const TCharm& charm : all) {
        if (charm.isMinor() == minorTab)
            charms.append(charm);
    }
    // CIP: higher unlock grade first (grades outside 1..3 count as 0), then by name.
    const auto grade = [](const TCharm& charm) { return charm.grade >= 1 && charm.grade <= 3 ? charm.grade : 0; };
    std::sort(charms.begin(), charms.end(), [&grade](const TCharm& a, const TCharm& b) {
        if (grade(a) != grade(b))
            return grade(a) > grade(b);
        return a.name().compare(b.name()) < 0;
    });
    QList<QObject*> previous = m_monsterBonusEffects;
    m_monsterBonusEffects.clear();
    for (const TCharm& charm : charms)
        m_monsterBonusEffects.append(new TQmlMonsterBonusEffect(charm, m_goldBalance, this));
    m_numberOfUnlockedCharms = static_cast<int>(std::count_if(all.begin(), all.end(), [](const TCharm& charm) { return charm.grade > 0; }));
    Q_EMIT qmlMonsterBonusEffectsChanged();
    Q_EMIT qmlStoreButtonTooltipChanged();
    Q_EMIT playerDataChanged();
    deleteAll(previous);
}

void TMonsterBonusEffectsDialogController::rebuildMonsterInfos()
{
    const bool minorTab = m_activeTab == 1;
    QList<QObject*> previous = m_monsterInfos;
    m_monsterInfos.clear();
    std::vector<std::pair<QString, int>> races;
    if (m_monsterRaceStorage) {
        // The tab's race list, by the name filter.
        const QList<TCharm> all = charms();
        const std::vector<cyclopedia::TMonsterRace>& tabRaces =
            minorTab ? m_monsterRaceStorage->racesUnlockedForMinorCharms() : m_monsterRaceStorage->racesUnlockedForCharms();
        for (const cyclopedia::TMonsterRace& race : tabRaces) {
            const int raceId = race.raceId;
            const QString name = QString::fromStdString(tibia::appearances::TAppearancesManager::instance().getRaceData(raceId).name);
            if (!name.contains(m_unassignedMonstersNameFilter, Qt::CaseInsensitive))
                continue;
            // The unlocked charms on the race; one of the tab's kind keeps it out.
            const bool assigned = std::any_of(all.begin(), all.end(), [raceId, minorTab](const TCharm& charm) {
                return charm.grade != 0 && charm.raceId == raceId && charm.isMinor() == minorTab;
            });
            if (assigned)
                continue;
            races.emplace_back(name, raceId);
        }
    }
    std::sort(races.begin(), races.end(),
              [](const auto& a, const auto& b) { return a.first.compare(b.first) < 0; });
    for (const auto& [name, raceId] : races)
        m_monsterInfos.append(new TQmlMonsterInfo(raceId, name, this));
    Q_EMIT qmlMonsterInfosChanged();
    deleteAll(previous);
    if (m_temporarySelectedMonsterRaceID != 0) {
        const int raceId = m_temporarySelectedMonsterRaceID;
        m_temporarySelectedMonsterRaceID = 0;
        setTemporarySelectedMonsterRaceID(raceId);
    }
}

void TMonsterBonusEffectsDialogController::onPlayerDataChanged()
{
    Q_EMIT playerDataChanged();
    if (!m_bonusEffectStorage)
        return;
    const quint64 gold = TBestiaryStorage::goldBalance();
    if (m_goldBalance == gold)
        return;
    m_goldBalance = gold;
    onMonsterBonusEffectsChanged();
    Q_EMIT playerDataChanged();
}

void TMonsterBonusEffectsDialogController::onRemainingNumberOfAssignableBonusEffectsChanged()
{
    Q_EMIT qmlStoreButtonTooltipChanged();
    Q_EMIT qmlMonsterBonusEffectsChanged();
}

QString TMonsterBonusEffectsDialogController::resetCostString() const
{
    if (!m_bonusEffectStorage)
        return QStringLiteral("0");
    return englishNumber(m_bonusEffectStorage->resetAllCharmCost());
}

bool TMonsterBonusEffectsDialogController::notEnoughGoldReset() const
{
    if (!m_bonusEffectStorage)
        return true;
    return m_goldBalance < m_bonusEffectStorage->resetAllCharmCost();
}

bool TMonsterBonusEffectsDialogController::enableResetButton() const
{
    if (!m_bonusEffectStorage)
        return false;
    const QList<TCharm> all = charms();
    return std::any_of(all.begin(), all.end(), [](const TCharm& charm) { return charm.grade > 0; });
}

bool TMonsterBonusEffectsDialogController::canAssignMoreBonusEffects() const
{
    return m_bonusEffectStorage && m_bonusEffectStorage->remainingNumberOfAssignableBonusEffects() != 0;
}

int TMonsterBonusEffectsDialogController::remainingCharms() const
{
    return m_bonusEffectStorage ? m_bonusEffectStorage->remainingNumberOfAssignableBonusEffects() : 0;
}

void TMonsterBonusEffectsDialogController::setPremiumTimeStorage(game::TPremiumTimeStorage* storage)
{
    if (m_premiumTimeStorage)
        disconnect(m_premiumTimeStorage.data(), nullptr, this, nullptr);
    m_premiumTimeStorage = storage;
    if (storage)
        connect(storage, &game::TPremiumTimeStorage::premiumTimeChanged, this, &TMonsterBonusEffectsDialogController::qmlStoreButtonTooltipChanged);
}

bool TMonsterBonusEffectsDialogController::isPremium() const { return m_premiumTimeStorage && m_premiumTimeStorage->isPremium(); }

QString TMonsterBonusEffectsDialogController::storeButtonTooltip() const
{
    // No session or unlimited slots (0xFF): no tooltip.
    if (!m_bonusEffectStorage)
        return {};
    const int slots = m_bonusEffectStorage->remainingNumberOfAssignableBonusEffects();
    QStringList lines;
    if (slots != 0xFF) {
        if (slots == 0)
            lines << qtTrId("monster_bonus_effects_no_more_remaining_unassigned_bonus_effects");
        else
            lines << qtTrId("monster_bonus_effects_remaining_unassigned_bonus_effects").arg(slots);
        lines << (isPremium() ? qtTrId("monster_bonus_effects_no_cyclopedia_improved_charms")
                                                      : qtTrId("monster_bonus_effects_no_premium_tooltip"));
    }
    return lines.join(u'\n');
}

bool TMonsterBonusEffectsDialogController::showStoreButtonTooltip() const
{
    if (!m_bonusEffectStorage)
        return true;
    return m_bonusEffectStorage->remainingNumberOfAssignableBonusEffects() != 0xFF;
}

void TMonsterBonusEffectsDialogController::unlockBonusEffect(const int BonusEffectID)
{
    // The charm becomes the selected one (no notify) and the confirmation's.
    const std::optional<TCharm> charm = this->charm(BonusEffectID);
    if (!charm)
        return;
    m_selectedCharm = charm->id;
    m_confirmAction = 0;
    m_confirmCharm = charm->id;
    m_confirmRace = 0;
    const QString cost = englishNumber(charm->nextGradeCost());
    if (charm->grade == 0) {
        m_confirmDialog->showMessage(qtTrId("monsters_confirm_unlock_charm_caption"),
                                     qtTrId(charm->isMinor() ? "monsters_confirm_unlock_minor_charm" : "monsters_confirm_unlock_major_charm")
                                         .arg(charm->name(), cost));
    } else {
        m_confirmDialog->showMessage(qtTrId("monsters_confirm_upgrade_charm_caption"),
                                     qtTrId(charm->isMinor() ? "monsters_confirm_upgrade_minor_charm" : "monsters_confirm_upgrade_major_charm")
                                         .arg(charm->name(), cost));
    }
}

void TMonsterBonusEffectsDialogController::clearBonusEffect(const int BonusEffectID)
{
    const std::optional<TCharm> charm = this->charm(BonusEffectID);
    if (!charm)
        return;
    m_selectedCharm = charm->id;
    m_confirmAction = 2;
    m_confirmCharm = charm->id;
    m_confirmRace = 0;
    m_confirmDialog->showMessage(qtTrId("monsters_confirm_unassign_charm_caption"),
                                 qtTrId("monsters_confirm_unassign_charm").arg(charm->name(), englishNumber(charm->removeCost)));
}

void TMonsterBonusEffectsDialogController::assignBonusEffectToMonster(const int BonusEffectID, const int monsterRaceID)
{
    const std::optional<TCharm> charm = this->charm(BonusEffectID);
    if (!charm)
        return;
    m_selectedCharm = charm->id;
    m_confirmAction = 1;
    m_confirmCharm = charm->id;
    m_confirmRace = monsterRaceID;
    m_confirmDialog->showMessage(qtTrId("monsters_confirm_assign_charm_caption"), qtTrId("monsters_confirm_assign_charm").arg(charm->name()));
}

QString TMonsterBonusEffectsDialogController::findMonsterNameByRaceID(const int RaceID)
{
    return QString::fromStdString(tibia::appearances::TAppearancesManager::instance().getRaceData(RaceID).name);
}

void TMonsterBonusEffectsDialogController::onStoreButtonClicked()
{
    const bool expansionOrPremium = (m_bonusEffectStorage && m_bonusEffectStorage->remainingNumberOfAssignableBonusEffects() == 0xFF) || isPremium();
    auto action = std::make_shared<input::TGameActionOpenStore>(expansionOrPremium ? store::TStoreOpenParameters{.openAction = store::OPEN_USEFUL_THING, .usefulThing = 3}
                                                                          : store::TStoreOpenParameters{.openAction = store::OPEN_PREMIUM_OR_BOOSTS, .premiumDetail = -1});
    Q_EMIT publishGameAction(action);
}

void TMonsterBonusEffectsDialogController::onResetAllCharmsClicked()
{
    m_confirmAction = 3;
    m_confirmCharm = 0;
    m_confirmRace = 0;
    m_confirmDialog->showMessage(qtTrId("monsters_confirm_clear_all_charms_caption"),
                                 qtTrId("monsters_confirm_clear_all_charms").arg(resetCostString()));
}

void TMonsterBonusEffectsDialogController::onActiveTabChanged(const int activeTabId)
{
    setActiveTab(activeTabId);
    onMonsterBonusEffectsChangedFromStorage();
}

bool TMonsterBonusEffectsDialogController::isMonsterOnlyAssignableForMinorCharms(const int RaceID)
{
    // Not in the list of the races for major charms.
    if (!m_monsterRaceStorage)
        return true;
    const std::vector<cyclopedia::TMonsterRace>& races = m_monsterRaceStorage->racesUnlockedForCharms();
    return std::none_of(races.begin(), races.end(), [RaceID](const cyclopedia::TMonsterRace& race) { return race.raceId == RaceID; });
}

void TMonsterBonusEffectsDialogController::onConfirmDialogAccepted()
{
    tibia::game::requestSendBuyCharmRune(static_cast<uint8_t>(m_confirmAction), static_cast<uint8_t>(m_confirmCharm),
                                   static_cast<uint16_t>(m_confirmRace));
}

}
