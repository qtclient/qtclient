#include "monsterbonuseffectsdialogcontroller.h"

#include "cyclopedia/bestiarystorage.h"
#include "gamewindow/messagedialogcontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"

#include <client/const.h>
#include <tibia/world/game.h>
#include <tibia/world/localplayer.h>
#include <client/outfit.h>
#include <tibia/assets/things.h>

#include <QLocale>

#include <algorithm>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;
using cyclopedia::TBestiaryStorage;
using cyclopedia::TCharm;

QString englishNumber(const qulonglong value) { return QLocale(QLocale::English).toString(value); }

QColor outfitColor(const int index)
{
    const Color c = Outfit::getColor(index);
    return QColor(c.r(), c.g(), c.b());
}

/// The charm points (major) or minor charm echoes (minor) of the player.
quint64 charmResource(const bool minor)
{
    const LocalPlayerPtr& player = g_game.getLocalPlayer();
    if (!player)
        return 0;
    return player->getResourceBalance(minor ? Otc::RESOURCE_MINOR_CHARM : Otc::RESOURCE_CHARM);
}

class TQmlMonsterBonusEffect final : public cip::tibia::gamewindow::TQmlMonsterBonusEffect
{
public:
    TQmlMonsterBonusEffect(const TCharm& charm, QObject* parent)
        : cip::tibia::gamewindow::TQmlMonsterBonusEffect(parent)
    {
        m_id = charm.id;
        m_name = charm.name();
        m_description = charm.description();
        m_imageSource = cyclopedia::charmImageSource(charm.id);
        m_isMinor = charm.isMinor();
        const int nextCost = charm.nextGradeCost();
        m_resourceCostString = englishNumber(nextCost);
        m_notEnoughResource = charmResource(m_isMinor) < static_cast<quint64>(nextCost);
        m_clearCostString = englishNumber(charm.removeCost);
        m_notEnoughGold = TBestiaryStorage::goldBalance() < charm.removeCost;
        m_unlocked = charm.grade > 0;
        m_unlockGrade = static_cast<cip::tibia::qmlcomponents::enums::ECharmUnlockGrade>(charm.grade);
        m_assigned = charm.raceId != 0;
        m_assignedMonsterRaceID = charm.raceId;
        if (m_assigned) {
            const Outfit& outfit = g_things.getRaceData(charm.raceId).outfit;
            m_hasAppearanceInstance = outfit.getId() != 0;
            m_creatureOutfitId = outfit.getId();
            m_creatureHeadColor = outfitColor(outfit.getHead());
            m_creatureTorsoColor = outfitColor(outfit.getBody());
            m_creatureLegsColor = outfitColor(outfit.getLegs());
            m_creatureDetailColor = outfitColor(outfit.getFeet());
            m_creatureFirstAddOn = (outfit.getAddons() & 1) != 0;
            m_creatureSecondAddOn = (outfit.getAddons() & 2) != 0;
        }
        // CIP 0x100cfef80.
        if (charm.grade >= 3)
            m_upgradeButtonText = qtTrId("monster_bonus_effects_fully_unlocked_button_text");
        else if (charm.grade > 0)
            m_upgradeButtonText = qtTrId("monster_bonus_effects_upgrade_button_text")
                                      .arg(cyclopedia::charmPercent(charm.id, charm.grade + 1) / 100.0, 0, 'g');
        else
            m_upgradeButtonText = qtTrId("monster_bonus_effects_not_unlocked_button_text");
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

TMonsterBonusEffectsDialogController::TMonsterBonusEffectsDialogController(TBestiaryStorage& storage, QObject* parent)
    : cip::tibia::gamewindow::TMonsterBonusEffectsDialogController(parent)
    , m_storage(storage)
    , m_confirmDialog(new TMessageDialogController(this))
{
    m_temporarySelectedMonsterIndex = -1;
    m_resetCostString = QStringLiteral("0");
    m_confirmDialog->setYesNoButtons();
    m_confirmDialog->setCloseOnButtonClicked(true);
    connect(m_confirmDialog, &TMessageDialogController::acceptClicked, this,
            &TMonsterBonusEffectsDialogController::onConfirmDialogAccepted);
    connect(&m_storage, &TBestiaryStorage::charmsChanged, this, [this] {
        if (m_shown)
            refresh();
    });
    m_hooks.emplace_back("g_game", "onResourcesBalanceChange", [this](LuaObject*, const Args& args) {
        if (!m_shown)
            return std::any{};
        const auto* type = args.size() > 2 ? std::any_cast<Otc::ResourceTypes_t>(&args[2]) : nullptr;
        if (type
            && (*type == Otc::RESOURCE_BANK_BALANCE || *type == Otc::RESOURCE_GOLD_EQUIPPED || *type == Otc::RESOURCE_CHARM
                || *type == Otc::RESOURCE_MINOR_CHARM)) {
            rebuildBonusEffects();
            updatePlayerData();
        }
        return std::any{};
    });
}

TMonsterBonusEffectsDialogController::~TMonsterBonusEffectsDialogController() = default;

TMonsterBonusEffectsDialogController::Context TMonsterBonusEffectsDialogController::captureContext() const
{
    return Context{ m_selectedCharm, m_activeTab };
}

void TMonsterBonusEffectsDialogController::restoreContext(const Context& context)
{
    if (m_selectedCharm != context.charmId) {
        m_selectedCharm = context.charmId;
        Q_EMIT qmlSelectedBonusEffectIdChanged();
    }
    setActiveTab(context.activeTab);
}

void TMonsterBonusEffectsDialogController::selectCharm(const int charmId)
{
    const TCharm probe{ charmId };
    setActiveTab(probe.isMinor() ? 1 : 0);
    if (m_shown)
        refresh();
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
    const Outfit outfit = v != 0 ? g_things.getRaceData(v).outfit : Outfit();
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
    m_shown = true;
    refresh();
    // Crystal answers the bestiary request with the charm data too.
    g_game.requestBestiary();
}

void TMonsterBonusEffectsDialogController::hideDialogTab() { m_shown = false; }

void TMonsterBonusEffectsDialogController::refresh()
{
    rebuildBonusEffects();
    rebuildMonsterInfos();
    updatePlayerData();
    updateStoreTooltip();
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
    if (m_storage.hasCharmData())
        m_pendingCharm.reset();
}

void TMonsterBonusEffectsDialogController::rebuildBonusEffects()
{
    QList<TCharm> charms;
    const bool minorTab = m_activeTab == 1;
    for (const TCharm& charm : m_storage.charms()) {
        if (charm.isMinor() == minorTab)
            charms.append(charm);
    }
    std::stable_sort(charms.begin(), charms.end(), [](const TCharm& a, const TCharm& b) {
        if (a.grade != b.grade)
            return a.grade > b.grade;
        return a.name().compare(b.name(), Qt::CaseInsensitive) < 0;
    });
    QList<QObject*> previous = m_monsterBonusEffects;
    m_monsterBonusEffects.clear();
    for (const TCharm& charm : charms)
        m_monsterBonusEffects.append(new TQmlMonsterBonusEffect(charm, this));
    const int slots = m_storage.availableCharmSlots();
    m_remainingCharms = slots;
    m_canAssignMoreBonusEffects = slots != 0;
    m_numberOfUnlockedCharms = static_cast<int>(
        std::count_if(m_storage.charms().begin(), m_storage.charms().end(), [](const TCharm& charm) { return charm.grade > 0; }));
    Q_EMIT qmlMonsterBonusEffectsChanged();
    deleteAll(previous);
}

void TMonsterBonusEffectsDialogController::rebuildMonsterInfos()
{
    const bool minorTab = m_activeTab == 1;
    QList<QObject*> previous = m_monsterInfos;
    m_monsterInfos.clear();
    std::vector<std::pair<QString, int>> races;
    for (const uint16_t raceId : m_storage.charmableRaces()) {
        if (!minorTab && m_storage.isRaceOnlyAssignableForMinorCharms(raceId))
            continue;
        const QString name = QString::fromStdString(g_things.getRaceData(raceId).name);
        if (!name.contains(m_unassignedMonstersNameFilter, Qt::CaseInsensitive))
            continue;
        if (m_storage.assignedCharm(raceId, minorTab))
            continue;
        races.emplace_back(name, raceId);
    }
    std::sort(races.begin(), races.end(),
              [](const auto& a, const auto& b) { return a.first.compare(b.first, Qt::CaseInsensitive) < 0; });
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

void TMonsterBonusEffectsDialogController::updatePlayerData()
{
    const quint64 resetCost = m_storage.resetAllCharmsCost();
    m_resetCostString = m_storage.hasCharmData() ? englishNumber(resetCost) : QStringLiteral("0");
    m_notEnoughGoldReset = TBestiaryStorage::goldBalance() < resetCost;
    m_enableResetButton = std::any_of(m_storage.charms().begin(), m_storage.charms().end(),
                                      [](const TCharm& charm) { return charm.grade > 0; });
    Q_EMIT playerDataChanged();
}

void TMonsterBonusEffectsDialogController::updateStoreTooltip()
{
    const LocalPlayerPtr& player = g_game.getLocalPlayer();
    const int slots = m_storage.availableCharmSlots();
    QStringList lines;
    if (slots == 0)
        lines << qtTrId("monster_bonus_effects_no_more_remaining_unassigned_bonus_effects");
    else if (slots != 0xFF)
        lines << qtTrId("monster_bonus_effects_remaining_unassigned_bonus_effects").arg(slots);
    lines << (player && player->isPremium() ? qtTrId("monster_bonus_effects_no_cyclopedia_improved_charms")
                                            : qtTrId("monster_bonus_effects_no_premium_tooltip"));
    m_storeButtonTooltip = lines.join(u'\n');
    m_showStoreButtonTooltip = slots != 0xFF;
    Q_EMIT qmlStoreButtonTooltipChanged();
}

void TMonsterBonusEffectsDialogController::unlockBonusEffect(const int BonusEffectID)
{
    const TCharm* charm = m_storage.charm(BonusEffectID);
    if (!charm || charm->grade >= 3)
        return;
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
    const TCharm* charm = m_storage.charm(BonusEffectID);
    if (!charm || charm->raceId == 0)
        return;
    m_confirmAction = 2;
    m_confirmCharm = charm->id;
    m_confirmRace = 0;
    m_confirmDialog->showMessage(qtTrId("monsters_confirm_unassign_charm_caption"),
                                 qtTrId("monsters_confirm_unassign_charm").arg(charm->name(), englishNumber(charm->removeCost)));
}

void TMonsterBonusEffectsDialogController::assignBonusEffectToMonster(const int BonusEffectID, const int monsterRaceID)
{
    const TCharm* charm = m_storage.charm(BonusEffectID);
    if (!charm || monsterRaceID <= 0)
        return;
    m_confirmAction = 1;
    m_confirmCharm = charm->id;
    m_confirmRace = monsterRaceID;
    m_confirmDialog->showMessage(qtTrId("monsters_confirm_assign_charm_caption"), qtTrId("monsters_confirm_assign_charm").arg(charm->name()));
}

QString TMonsterBonusEffectsDialogController::findMonsterNameByRaceID(const int RaceID)
{
    return QString::fromStdString(g_things.getRaceData(RaceID).name);
}

void TMonsterBonusEffectsDialogController::onStoreButtonClicked()
{
    const LocalPlayerPtr& player = g_game.getLocalPlayer();
    const bool expansionOrPremium = m_storage.availableCharmSlots() == 0xFF || (player && player->isPremium());
    auto action = input::mutableClone(*input::makeAction(A::OPEN_STORE));
    action->parameter = expansionOrPremium ? 3 : 1;
    action->instance = expansionOrPremium ? 3 : -1;
    input::publish(action);
}

void TMonsterBonusEffectsDialogController::onResetAllCharmsClicked()
{
    m_confirmAction = 3;
    m_confirmCharm = 0;
    m_confirmRace = 0;
    m_confirmDialog->showMessage(qtTrId("monsters_confirm_clear_all_charms_caption"),
                                 qtTrId("monsters_confirm_clear_all_charms").arg(englishNumber(m_storage.resetAllCharmsCost())));
}

void TMonsterBonusEffectsDialogController::onActiveTabChanged(const int activeTabId)
{
    setActiveTab(activeTabId);
    refresh();
}

bool TMonsterBonusEffectsDialogController::isMonsterOnlyAssignableForMinorCharms(const int RaceID)
{
    return m_storage.isRaceOnlyAssignableForMinorCharms(RaceID);
}

void TMonsterBonusEffectsDialogController::onConfirmDialogAccepted()
{
    g_game.requestSendBuyCharmRune(static_cast<uint8_t>(m_confirmAction), static_cast<uint8_t>(m_confirmCharm),
                                   static_cast<uint16_t>(m_confirmRace));
}

}
