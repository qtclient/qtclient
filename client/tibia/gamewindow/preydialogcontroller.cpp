#include "preydialogcontroller.h"

#include "input/gameactionhandlers.h"

#include <tibia/world/game.h>
#include <tibia/world/localplayer.h>
#include <client/outfit.h>
#include <tibia/assets/things.h>

#include <QDateTime>
#include <QLocale>

#include <algorithm>

namespace tibia::prey {

namespace {

qint64 now() { return QDateTime::currentSecsSinceEpoch(); }

}

TPreyData::TPreyData(const uint8_t preyId, QObject* parent) : cip::tibia::prey::TPreyData(parent), m_preyId(preyId) {}

void TPreyData::setState(const int state)
{
    m_state = state;
    Q_EMIT stateChanged();
}

void TPreyData::setBonus(const TPreyBonus& bonus)
{
    m_bonus = bonus;
    Q_EMIT bonusChanged();
}

void TPreyData::setLocked(const int unlockOption)
{
    if (unlockOption == 2) {
        setState(1);
        return;
    }
    m_unlockOption = unlockOption;
    Q_EMIT unlockOptionChanged();
    setState(0);
}

void TPreyData::setInactive() { setState(1); }

void TPreyData::setActive(const TPreyBonus& bonus, const TPreyMonster& monster, const int timeLeft)
{
    setBonus(bonus);
    m_monster = monster;
    Q_EMIT monsterInformationChanged();
    setTimeLeft(timeLeft);
    setState(bonus.type == 4 ? 1 : 2);
}

void TPreyData::setSelection(QList<TPreyMonster> list)
{
    const bool empty = list.isEmpty();
    m_monsterList = std::move(list);
    Q_EMIT monsterListChanged();
    setState(empty ? 1 : 3);
}

void TPreyData::setSelectionChangeMonster(const TPreyBonus& bonus, QList<TPreyMonster> list)
{
    const bool none = bonus.type == 4 || list.isEmpty();
    setBonus(bonus);
    m_monsterList = std::move(list);
    Q_EMIT monsterListChanged();
    setState(none ? 1 : 4);
}

void TPreyData::setListSelection(QList<uint16_t> races)
{
    const bool empty = races.isEmpty();
    m_raceList = std::move(races);
    Q_EMIT monsterRaceListChanged();
    setState(empty ? 1 : 5);
}

void TPreyData::setWildcardSelection(const TPreyBonus& bonus, QList<uint16_t> races)
{
    setBonus(bonus);
    m_raceList = std::move(races);
    Q_EMIT monsterRaceListChanged();
    setState(6);
}

void TPreyData::setTimeLeft(const int seconds)
{
    m_timeLeft = seconds;
    Q_EMIT timeLeftChanged();
}

void TPreyData::setNextFreeListReroll(const qint64 seconds)
{
    m_nextFreeListReroll = seconds > 0 ? now() + seconds : 0;
    Q_EMIT timeUntilFreeListRerollChanged();
}

void TPreyData::setAutomaticExtendType(const int type)
{
    m_automaticExtendType = type;
    Q_EMIT automaticExtendTypeChanged();
}

TPreyStorage::TPreyStorage(QObject* parent) : cip::tibia::prey::TPreyStorage(parent)
{
    for (int i = 0; i < SlotCount; ++i)
        m_slots[i] = new TPreyData(static_cast<uint8_t>(i), this);
    reset();
}

TPreyData* TPreyStorage::slot(const int preyId) const
{
    return preyId >= 0 && preyId < SlotCount ? m_slots[preyId] : nullptr;
}

void TPreyStorage::markReceived(const int preyId) { m_pendingSlots.erase(preyId); }

void TPreyStorage::reset()
{
    m_pendingSlots.clear();
    for (int i = 0; i < SlotCount; ++i)
        m_pendingSlots.insert(i);
}

void TPreyStorage::setPrices(const int listReroll, const int bonusReroll, const int choosePrey)
{
    const bool bonusPossible = bonusRerollPossible();
    const bool choosePossible = choosePreyPossible();
    m_listRerollPrice = listReroll;
    m_bonusRerollPrice = bonusReroll;
    m_choosePreyPrice = choosePrey;
    Q_EMIT listRerollPriceChanged();
    Q_EMIT bonusRerollPriceChanged();
    Q_EMIT choosePreyPriceChanged();
    if (bonusPossible != bonusRerollPossible())
        Q_EMIT bonusRerollPossibleChanged();
    if (choosePossible != choosePreyPossible())
        Q_EMIT choosePreyPossibleChanged();
}

void TPreyStorage::setPreyWildcards(const int amount)
{
    if (amount == m_preyWildcards)
        return;
    const bool bonusPossible = bonusRerollPossible();
    const bool choosePossible = choosePreyPossible();
    m_preyWildcards = amount;
    Q_EMIT preyWildcardAmountChanged();
    if (bonusPossible != bonusRerollPossible())
        Q_EMIT bonusRerollPossibleChanged();
    if (choosePossible != choosePreyPossible())
        Q_EMIT choosePreyPossibleChanged();
}

}

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;
using Button = TMessageDialogController::Button;
using EUnlockOption = cip::tibia::qmlcomponents::enums::EPreyUnlockOption;
using EExtendType = cip::tibia::qmlcomponents::enums::EPreyAutomaticExtendType;

constexpr int MaximumBonusGrade = 10;
/// CIP 0x100ee2320: the length of a prey, in seconds.
constexpr double PreyDuration = 7200.0;
/// CIP DAT_1017dbdc8: the free list reroll interval, in seconds.
constexpr double FreeListRerollInterval = 72000.0;
constexpr int FreeListRerollRefreshMs = 7000;
const QChar MultiStringSeparator(0x9c);

QString english(const qint64 value) { return QLocale(QLocale::English).toString(value); }

/// CIP 0x100bfbd70: the number and its " k"/"k", " kk"/"kk", ... forms as a
/// multi string.
QString numberVariants(const qint64 value)
{
    QStringList variants{ english(value) };
    for (const auto& [divisor, unit] : { std::pair<qint64, const char*>{ 1000, "k" }, { 1000000, "kk" }, { 1000000000, "kkk" } }) {
        if (value < divisor)
            break;
        const QString shortened = english(value / divisor);
        variants << shortened + u' ' + QLatin1String(unit) << shortened + QLatin1String(unit);
    }
    return variants.join(MultiStringSeparator);
}

uint64_t balance(const Otc::ResourceTypes_t type)
{
    const LocalPlayerPtr& player = g_game.getLocalPlayer();
    return player ? player->getResourceBalance(type) : 0;
}

uint64_t totalGold() { return balance(Otc::RESOURCE_BANK_BALANCE) + balance(Otc::RESOURCE_GOLD_EQUIPPED); }

int wildcardBalance() { return static_cast<int>(std::min<uint64_t>(balance(Otc::RESOURCE_PREY_WILDCARDS), INT_MAX)); }

QColor outfitColor(const int index)
{
    const Color c = Outfit::getColor(index);
    return QColor(c.r(), c.g(), c.b());
}

/// CIP 0x100b8f540.
QList<TMessageDialogButton*> cancelButton()
{
    return { new TMessageDialogButton(qtTrId("cancel"), Button::CANCEL, QKeySequence(QStringLiteral("Esc"))) };
}

/// CIP 0x100b8f3a0.
QList<TMessageDialogButton*> okButton()
{
    return { new TMessageDialogButton(qtTrId("ok"), Button::OK, QKeySequence(QStringLiteral("Return"))) };
}

void publishPreyAction(const A type, const uint preyId, const int parameter = 0)
{
    auto action = input::mutableClone(*input::makeAction(type));
    action->instance = static_cast<int>(preyId);
    action->parameter = parameter;
    input::publish(action);
}

/// CIP FUN_100ba94e0 store requests: open type 1 (premium) or 3 (an offer).
void openStore(const int openType, const int offer)
{
    auto action = input::mutableClone(*input::makeAction(A::OPEN_STORE));
    action->parameter = openType;
    action->instance = offer;
    input::publish(action);
}

/// CIP: "word word" -> "Word Word".
QString capitalizeWords(const QString& name)
{
    QStringList words = name.split(u' ');
    for (QString& word : words) {
        if (!word.isEmpty())
            word[0] = word[0].toUpper();
    }
    return words.join(u' ');
}

}

TListRerollEntryInfo::TListRerollEntryInfo(const prey::TPreyMonster& monster, QObject* parent)
    : cip::tibia::gamewindow::TListRerollEntryInfo(parent)
{
    m_monsterName = monster.name;
    m_monsterOutfitId = monster.outfitId;
    m_monsterHeadColor = outfitColor(monster.head);
    m_monsterTorsoColor = outfitColor(monster.torso);
    m_monsterLegsColor = outfitColor(monster.legs);
    m_monsterDetailColor = outfitColor(monster.detail);
    m_monsterFirstAddOn = monster.firstAddOn;
    m_monsterSecondAddOn = monster.secondAddOn;
}

void TPreyBestiarySelectionModel::setRaces(const QList<uint16_t>& races)
{
    beginResetModel();
    m_entries.clear();
    m_entries.reserve(races.size());
    for (const uint16_t race : races)
        m_entries.append({ .id = race, .name = capitalizeWords(QString::fromStdString(g_things.getRaceData(race).name)) });
    endResetModel();
    Q_EMIT rowCountChanged();
}

int TPreyBestiarySelectionModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QVariant TPreyBestiarySelectionModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() >= m_entries.size())
        return {};
    const Entry& entry = m_entries[index.row()];
    switch (role) {
        case IdRole: return entry.id;
        case NameRole: return entry.name;
        default: return {};
    }
}

QHash<int, QByteArray> TPreyBestiarySelectionModel::roleNames() const { return { { IdRole, "id" }, { NameRole, "name" } }; }

void TPreyBestiarySelectionProxyModel::setNameFilter(const QString& filter)
{
    beginFilterChange();
    m_nameFilter = filter;
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
}

bool TPreyBestiarySelectionProxyModel::filterAcceptsRow(const int sourceRow, const QModelIndex& sourceParent) const
{
    const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    return index.data(TPreyBestiarySelectionModel::NameRole).toString().contains(m_nameFilter, Qt::CaseInsensitive);
}

bool TPreyBestiarySelectionProxyModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    return left.data(TPreyBestiarySelectionModel::NameRole).toString().toLower()
           < right.data(TPreyBestiarySelectionModel::NameRole).toString().toLower();
}

TPreyRenderInfo::TPreyRenderInfo(prey::TPreyData* data, QObject* parent) : cip::tibia::gamewindow::TPreyRenderInfo(parent), m_data(data)
{
    m_bestiaryProxy.setSourceModel(&m_bestiary);
    m_bestiaryProxy.sort(0);
    connect(data, &prey::TPreyData::stateChanged, this, &TPreyRenderInfo::stateChanged);
    connect(data, &prey::TPreyData::unlockOptionChanged, this, &TPreyRenderInfo::unlockOptionChanged);
    connect(data, &prey::TPreyData::timeLeftChanged, this, &TPreyRenderInfo::timeLeftChanged);
    connect(data, &prey::TPreyData::timeUntilFreeListRerollChanged, this, &TPreyRenderInfo::timeUntilFreeListRerollChanged);
    connect(data, &prey::TPreyData::automaticExtendTypeChanged, this, &TPreyRenderInfo::automaticExtendTypeChanged);
    connect(data, &prey::TPreyData::bonusChanged, this, &TPreyRenderInfo::bonusChanged);
    connect(data, &prey::TPreyData::monsterInformationChanged, this, &TPreyRenderInfo::monsterChanged);
    connect(data, &prey::TPreyData::monsterListChanged, this, &TPreyRenderInfo::onMonsterListChanged);
    connect(data, &prey::TPreyData::monsterRaceListChanged, this, &TPreyRenderInfo::onMonsterRaceListChanged);
    connect(data, &prey::TPreyData::huntingTaskRacesChanged, this, &TPreyRenderInfo::onMonsterRaceListChanged);
    onMonsterListChanged();
    onMonsterRaceListChanged();
}

void TPreyRenderInfo::onMonsterListChanged()
{
    const QList<QObject*> old = m_listRerollData;
    m_listRerollData.clear();
    for (const prey::TPreyMonster& monster : m_data->monsterList())
        m_listRerollData.append(new TListRerollEntryInfo(monster, this));
    Q_EMIT listRerollDataChanged();
    for (QObject* entry : old)
        entry->deleteLater();
}

void TPreyRenderInfo::onMonsterRaceListChanged()
{
    m_bestiary.setRaces(m_data->raceList());
    Q_EMIT bestiarySelectionDataChanged();
}

uint TPreyRenderInfo::preyId() const { return m_data->preyId(); }
bool TPreyRenderInfo::isLocked() const { return m_data->state() == 0; }

bool TPreyRenderInfo::isInactive() const
{
    const int state = m_data->state();
    return state == 1 || state == 3 || state == 5;
}

bool TPreyRenderInfo::isActive() const { return m_data->state() == 2; }

bool TPreyRenderInfo::isSelecting() const
{
    const int state = m_data->state();
    return state >= 3 && state <= 6;
}

bool TPreyRenderInfo::isGridSelecting() const
{
    const int state = m_data->state();
    return state == 3 || state == 4;
}

bool TPreyRenderInfo::isBestiarySelecting() const
{
    const int state = m_data->state();
    return state == 5 || state == 6;
}

EUnlockOption TPreyRenderInfo::unlockOption() const
{
    switch (m_data->unlockOption()) {
        case 0: return EUnlockOption::UNLOCK_OPTION_PREMIUM_OR_STORE;
        case 1: return EUnlockOption::UNLOCK_OPTION_STORE;
        default: return EUnlockOption::UNLOCK_OPTION_NONE;
    }
}

/// CIP 0x100c63620 (0x10095e960: "HH:MM").
QString TPreyRenderInfo::timeLeft() const
{
    const int seconds = m_data->timeLeft();
    if (seconds < 1)
        return {};
    if (seconds < 60)
        return qtTrId("time_less_than_one_minute");
    return QStringLiteral("%1:%2").arg(seconds / 3600, 2, 10, QChar(u'0')).arg((seconds / 60) % 60, 2, 10, QChar(u'0'));
}

double TPreyRenderInfo::timeLeftProgress() const { return m_data->timeLeft() / PreyDuration; }

namespace {

const char* bonusImageName(const int type)
{
    switch (type) {
        case 0: return "damage-boost";
        case 1: return "damage-reduction";
        case 2: return "improved-xp";
        case 3: return "improved-loot";
        case 4: return "none";
        default: return nullptr;
    }
}

}

QString TPreyRenderInfo::bonusIcon() const
{
    const char* name = bonusImageName(m_data->bonus().type);
    return name ? QStringLiteral("/images/prey-bonus-%1.png").arg(QLatin1String(name)) : QString();
}

QString TPreyRenderInfo::bonusIconSmall() const
{
    const char* name = bonusImageName(m_data->bonus().type);
    return name ? QStringLiteral("/images/prey-bonus-%1-small.png").arg(QLatin1String(name)) : QString();
}

QString TPreyRenderInfo::bonusValueText() const
{
    if (m_data->bonus().type == 4)
        return QStringLiteral("+??%");
    return QStringLiteral("+%1%").arg(m_data->bonus().value);
}

uint TPreyRenderInfo::bonusGrade() const { return m_data->bonus().grade; }
uint TPreyRenderInfo::maximumBonusGrade() const { return MaximumBonusGrade; }

/// CIP 0x100c63b60: a filled star per grade, then empty ones.
QString TPreyRenderInfo::bonusGradeText() const
{
    QString text;
    const int grade = m_data->bonus().grade;
    for (int i = 0; i < MaximumBonusGrade; ++i)
        text.append(i < grade ? QChar(0x2605) : QChar(0x2606));
    return text;
}

/// CIP 0x100c63cc0.
QString TPreyRenderInfo::bonusTypeText() const
{
    switch (m_data->bonus().type) {
        case 0: return qtTrId("prey_bonus_type_damage_boost");
        case 1: return qtTrId("prey_bonus_type_damage_reduction");
        case 2: return qtTrId("prey_bonus_type_bonus_xp");
        case 3: return qtTrId("prey_bonus_type_improved_loot");
        default: return qtTrId("prey_bonus_type_name_none");
    }
}

/// CIP 0x100c63d10.
QString TPreyRenderInfo::preyDescription() const
{
    const int value = m_data->bonus().value;
    switch (m_data->bonus().type) {
        case 0: return qtTrId("prey_bonus_description_damage_boost").arg(value);
        case 1: return qtTrId("prey_bonus_description_damage_reduction").arg(value);
        case 2: return qtTrId("prey_bonus_description_bonus_xp").arg(value);
        case 3: return qtTrId("prey_bonus_description_improved_loot").arg(value);
        default: return qtTrId("prey_bonus_description_name_none");
    }
}

QString TPreyRenderInfo::monsterName() const { return m_data->monster().name; }
uint TPreyRenderInfo::monsterOutfitId() const { return m_data->monster().outfitId; }
QColor TPreyRenderInfo::monsterHeadColor() const { return outfitColor(m_data->monster().head); }
QColor TPreyRenderInfo::monsterTorsoColor() const { return outfitColor(m_data->monster().torso); }
QColor TPreyRenderInfo::monsterLegsColor() const { return outfitColor(m_data->monster().legs); }
QColor TPreyRenderInfo::monsterDetailColor() const { return outfitColor(m_data->monster().detail); }
bool TPreyRenderInfo::monsterFirstAddOn() const { return m_data->monster().firstAddOn; }
bool TPreyRenderInfo::monsterSecondAddOn() const { return m_data->monster().secondAddOn; }

qint64 TPreyRenderInfo::secondsToFreeListReroll() const
{
    return std::max<qint64>(m_data->nextFreeListReroll() - QDateTime::currentSecsSinceEpoch(), 0);
}

/// CIP 0x100c64640.
bool TPreyRenderInfo::hasFreeListReroll() const { return secondsToFreeListReroll() == 0; }

/// CIP 0x100c64670 (0x1009610a0; 0x10095e080 "HH:MM" from minutes).
QString TPreyRenderInfo::timeToFreeListReroll() const
{
    const qint64 seconds = secondsToFreeListReroll();
    if (seconds == 0)
        return qtTrId("prey_list_reroll_is_free") + MultiStringSeparator + qtTrId("prey_list_reroll_is_free_short");
    if (seconds > 60) {
        const qint64 minutes = seconds / 60;
        return QStringLiteral("%1:%2").arg(minutes / 60, 2, 10, QChar(u'0')).arg(minutes % 60, 2, 10, QChar(u'0'));
    }
    return qtTrId("time_less_than_one_minute") + MultiStringSeparator + qtTrId("time_less_than_one_minute_short");
}

/// CIP 0x100c646b0.
double TPreyRenderInfo::timeToFreeListRerollProgress() const { return secondsToFreeListReroll() / FreeListRerollInterval; }

EExtendType TPreyRenderInfo::automaticPreyExtendType() const
{
    switch (m_data->automaticExtendType()) {
        case 2: return EExtendType::LOCK_PREY;
        case 1: return EExtendType::BONUS_REROLL;
        default: return EExtendType::NOT_EXTENDING;
    }
}

/// CIP 0x100c64320.
QString TPreyRenderInfo::automaticExtendTypeText() const
{
    const QString format = QStringLiteral("%1: %2");
    switch (m_data->automaticExtendType()) {
        case 2: return format.arg(qtTrId("prey_lock_prey"), qtTrId("enabled"));
        case 1: return format.arg(qtTrId("prey_auto_reroll"), qtTrId("enabled"));
        default: return format.arg(qtTrId("prey_auto_extend"), qtTrId("disabled"));
    }
}

void TPreyRenderInfo::rerollPreyBonusClicked() { Q_EMIT requestBonusReroll(preyId()); }
void TPreyRenderInfo::listRerollClicked() { Q_EMIT requestListReroll(preyId()); }
void TPreyRenderInfo::choosePreyClicked() { Q_EMIT requestChoosePrey(preyId()); }

/// CIP 0x100c64cf0.
void TPreyRenderInfo::confirmGridPreyCreatureClicked(const int Index)
{
    publishPreyAction(A::PREY_SELECT_MONSTER_FROM_LIST, preyId(), static_cast<uint8_t>(Index));
}

/// CIP 0x100c64e50.
void TPreyRenderInfo::confirmBestiaryPreyCreatureClicked(const int MonsterRaceID)
{
    publishPreyAction(A::PREY_SELECT_MONSTER_FROM_BESTIARY, preyId(), static_cast<uint16_t>(MonsterRaceID));
}

/// CIP 0x100c64fb0.
void TPreyRenderInfo::unlockPermanentlyClicked() { openStore(3, 0); }

/// CIP 0x100c65150.
void TPreyRenderInfo::unlockWithPermiumClicked() { openStore(1, -1); }

/// CIP 0x100c652f0.
void TPreyRenderInfo::autoExtendPreydClicked(const EExtendType QMLAutoExtendType)
{
    int type = 0;
    if (QMLAutoExtendType == EExtendType::BONUS_REROLL)
        type = 1;
    else if (QMLAutoExtendType == EExtendType::LOCK_PREY)
        type = 2;
    Q_EMIT requestChangeAutoExtendType(preyId(), type);
}

void TPreyRenderInfo::setMonsterRacesFilter(const QString& NameFilter) { m_bestiaryProxy.setNameFilter(NameFilter); }

TPreyPreyPageController::TPreyPreyPageController(prey::TPreyStorage* storage, QObject* parent)
    : cip::tibia::gamewindow::TPreyPreyPageController(parent)
    , m_storage(storage)
    , m_bonusReroll(new TMessageDialogController(this))
    , m_listReroll(new TMessageDialogController(this))
    , m_choosePrey(new TMessageDialogController(this))
    , m_autoReroll(new TMessageDialogController(this))
    , m_lockPrey(new TMessageDialogController(this))
{
    m_clientSettingSmoothFiltering = true;
    for (TMessageDialogController* box : { m_bonusReroll, m_listReroll, m_choosePrey, m_autoReroll, m_lockPrey })
        box->setCloseOnButtonClicked(true);
    connect(m_bonusReroll, &TMessageDialogController::acceptClicked, this, &TPreyPreyPageController::onConfirmBonusReroll);
    connect(m_listReroll, &TMessageDialogController::acceptClicked, this, &TPreyPreyPageController::onConfirmListReroll);
    connect(m_choosePrey, &TMessageDialogController::acceptClicked, this, &TPreyPreyPageController::onConfirmChoosePrey);
    connect(m_autoReroll, &TMessageDialogController::acceptClicked, this, &TPreyPreyPageController::onConfirmEnableAutoRerollBonus);
    connect(m_lockPrey, &TMessageDialogController::acceptClicked, this, &TPreyPreyPageController::onConfirmEnableLockPrey);

    m_preyListTimer.setSingleShot(true);
    connect(&m_preyListTimer, &QTimer::timeout, this, &TPreyPreyPageController::preyListChanged);
    m_freeRerollTimer.setInterval(FreeListRerollRefreshMs);
    connect(&m_freeRerollTimer, &QTimer::timeout, this, [this] {
        for (QObject* info : std::as_const(m_renderInfos))
            Q_EMIT static_cast<TPreyRenderInfo*>(info)->timeUntilFreeListRerollChanged();
    });

    connect(storage, &prey::TPreyStorage::listRerollPriceChanged, this, [this] {
        Q_EMIT listRerollPriceChanged();
        Q_EMIT listRerollTooExpensiveChanged();
    });
    connect(storage, &prey::TPreyStorage::bonusRerollPriceChanged, this, &TPreyPreyPageController::bonusRerollPriceChanged);
    connect(storage, &prey::TPreyStorage::bonusRerollPossibleChanged, this, &TPreyPreyPageController::bonusRerollTooExpensiveChanged);
    connect(storage, &prey::TPreyStorage::choosePreyPriceChanged, this, &TPreyPreyPageController::choosePreyPriceChanged);
    connect(storage, &prey::TPreyStorage::choosePreyPossibleChanged, this, &TPreyPreyPageController::choosePreyTooExpensiveChanged);
    connect(storage, &prey::TPreyStorage::preyWildcardAmountChanged, this, &TPreyPreyPageController::preyWildcardAmountChanged);
}

QString TPreyPreyPageController::preyWildcards() const { return english(m_storage->preyWildcards()); }
QString TPreyPreyPageController::listRerollPrice() const { return numberVariants(m_storage->listRerollPrice()); }
bool TPreyPreyPageController::listRerollTooExpensive() const { return totalGold() < static_cast<uint64_t>(m_storage->listRerollPrice()); }
QString TPreyPreyPageController::bonusRerollPrice() const { return english(m_storage->bonusRerollPrice()); }
bool TPreyPreyPageController::bonusRerollTooExpensive() const { return !m_storage->bonusRerollPossible(); }
QString TPreyPreyPageController::choosePreyPrice() const { return english(m_storage->choosePreyPrice()); }
bool TPreyPreyPageController::choosePreyTooExpensive() const { return !m_storage->choosePreyPossible(); }

void TPreyPreyPageController::emitAll()
{
    Q_EMIT preyWildcardAmountChanged();
    Q_EMIT listRerollPriceChanged();
    Q_EMIT listRerollTooExpensiveChanged();
    Q_EMIT bonusRerollPriceChanged();
    Q_EMIT bonusRerollTooExpensiveChanged();
    Q_EMIT choosePreyPriceChanged();
    Q_EMIT choosePreyTooExpensiveChanged();
}

void TPreyPreyPageController::onBalanceChanged() { Q_EMIT listRerollTooExpensiveChanged(); }

void TPreyPreyPageController::closeMessages()
{
    for (TMessageDialogController* box : { m_bonusReroll, m_listReroll, m_choosePrey, m_autoReroll, m_lockPrey })
        box->dialog()->close();
}

void TPreyPreyPageController::showDialogTab()
{
    m_bonusReroll->dialog()->close();
    m_listReroll->dialog()->close();

    const QList<QObject*> old = m_renderInfos;
    m_renderInfos.clear();
    for (prey::TPreyData* data : m_storage->slots()) {
        auto* info = new TPreyRenderInfo(data, this);
        connect(info, &TPreyRenderInfo::requestBonusReroll, this, &TPreyPreyPageController::onRequestBonusReroll);
        connect(info, &TPreyRenderInfo::requestListReroll, this, &TPreyPreyPageController::onRequestListReroll);
        connect(info, &TPreyRenderInfo::requestChoosePrey, this, &TPreyPreyPageController::onRequestChoosePrey);
        connect(info, &TPreyRenderInfo::requestChangeAutoExtendType, this, &TPreyPreyPageController::onRequestChangeAutoExtendType);
        m_renderInfos.append(info);
    }
    for (QObject* info : old)
        info->deleteLater();
    m_preyListTimer.start(0);

    emitAll();
    m_freeRerollTimer.start();
    for (QObject* info : std::as_const(m_renderInfos))
        Q_EMIT static_cast<TPreyRenderInfo*>(info)->timeUntilFreeListRerollChanged();
}

void TPreyPreyPageController::hideDialogTab() { m_freeRerollTimer.stop(); }

void TPreyPreyPageController::askToUseWildcards(TMessageDialogController* box, const int price)
{
    const int wildcards = m_storage->preyWildcards();
    const QString currency = qtTrId("currency_view_preywildcard_tooltip");
    QString text;
    if (wildcards < price) {
        text = qtTrId("prey_confirm_use_prey_wildcard_not_possible").arg(currency);
        box->setButtons(cancelButton());
    } else {
        if (wildcards == price)
            text = qtTrId("prey_confirm_use_prey_wildcard_last").arg(english(price), currency);
        else
            text = qtTrId("prey_confirm_use_prey_wildcard").arg(english(price), english(wildcards), currency);
        box->setYesNoButtons();
    }
    box->showMessage(qtTrId("prey_confirm_using_prey_wildcard_caption"), text);
}

void TPreyPreyPageController::onRequestBonusReroll(const uint preyId)
{
    m_pendingPreyId = preyId;
    askToUseWildcards(m_bonusReroll, m_storage->bonusRerollPrice());
}

void TPreyPreyPageController::onRequestListReroll(const uint preyId)
{
    m_pendingPreyId = preyId;
    const prey::TPreyData* data = m_storage->slot(static_cast<int>(preyId));
    if (!data)
        return;
    const uint64_t gold = totalGold();
    const auto price = static_cast<uint64_t>(m_storage->listRerollPrice());
    QString text;
    if (data->nextFreeListReroll() <= QDateTime::currentSecsSinceEpoch()) {
        text = qtTrId("prey_confirm_list_reroll_free");
        m_listReroll->setYesNoButtons();
    } else if (gold >= price) {
        text = qtTrId("prey_confirm_list_reroll_normal").arg(english(price), english(gold));
        m_listReroll->setYesNoButtons();
    } else {
        text = qtTrId("prey_confirm_list_reroll_not_possible").arg(english(gold), english(price));
        m_listReroll->setButtons(cancelButton());
    }
    m_listReroll->showMessage(qtTrId("prey_confirm_list_reroll_caption"), text);
}

void TPreyPreyPageController::onRequestChoosePrey(const uint preyId)
{
    m_pendingPreyId = preyId;
    askToUseWildcards(m_choosePrey, m_storage->choosePreyPrice());
}

void TPreyPreyPageController::onRequestChangeAutoExtendType(const uint preyId, const int type)
{
    const prey::TPreyData* data = m_storage->slot(static_cast<int>(preyId));
    if (!data || data->automaticExtendType() == type)
        return;
    m_pendingPreyId = preyId;
    const QString currency = qtTrId("currency_view_preywildcard_tooltip");
    const QString caption = qtTrId("prey_confirm_using_prey_wildcard_caption");
    if (type == 2) {
        m_lockPrey->setYesNoButtons();
        m_lockPrey->showMessage(caption, qtTrId("prey_confirm_enable_auto_prey_extension_description")
                                             .arg(qtTrId("prey_lock_prey"), english(m_storage->choosePreyPrice()), currency));
    } else if (type == 1) {
        m_autoReroll->setYesNoButtons();
        m_autoReroll->showMessage(caption, qtTrId("prey_confirm_enable_auto_prey_extension_description")
                                               .arg(qtTrId("prey_auto_reroll"), english(m_storage->bonusRerollPrice()), currency));
    } else {
        publishPreyAction(A::PREY_REQUEST_CHANGE_AUTO_EXPAND_TYPE, preyId, 0);
    }
}

void TPreyPreyPageController::onConfirmBonusReroll() { publishPreyAction(A::PREY_REQUEST_BONUS_REROLL, m_pendingPreyId); }
void TPreyPreyPageController::onConfirmListReroll() { publishPreyAction(A::PREY_REQUEST_LIST_REROLL, m_pendingPreyId); }
void TPreyPreyPageController::onConfirmChoosePrey() { publishPreyAction(A::PREY_REQUEST_FREEZE_AND_SHOW_BESTIARY_MONSTERS, m_pendingPreyId); }

void TPreyPreyPageController::onConfirmEnableAutoRerollBonus()
{
    publishPreyAction(A::PREY_REQUEST_CHANGE_AUTO_EXPAND_TYPE, m_pendingPreyId, 1);
}

void TPreyPreyPageController::onConfirmEnableLockPrey() { publishPreyAction(A::PREY_REQUEST_CHANGE_AUTO_EXPAND_TYPE, m_pendingPreyId, 2); }

/// CIP 0x100c618e0.
void TPreyPreyPageController::getMorePreyWildcardsClicked() { openStore(3, 1); }

TPreyDialogController::TPreyDialogController(QObject* parent)
    : TDialogController(QStringLiteral("prey/PreyDialog.qml"), parent)
    , m_storage(new prey::TPreyStorage(this))
    , m_page(new TPreyPreyPageController(m_storage, this))
    , m_notOnMain(new TMessageDialogController(this))
    , m_info(new TMessageDialogController(this))
{
    m_preyPageController = m_page;
    m_notOnMain->setCloseOnButtonClicked(true);
    m_info->setCloseOnButtonClicked(true);
    connect(m_storage, &prey::TPreyStorage::preyWildcardAmountChanged, this, &TPreyDialogController::preyWildcardAmountChanged);
}

TPreyDialogController::~TPreyDialogController() = default;

int TPreyDialogController::preyWildcards() const { return m_storage->preyWildcards(); }

void TPreyDialogController::onBalanceChanged()
{
    m_storage->setPreyWildcards(wildcardBalance());
    m_page->onBalanceChanged();
}

void TPreyDialogController::clear()
{
    closeDialog();
    m_page->closeMessages();
    m_notOnMain->dialog()->close();
    m_info->dialog()->close();
    m_storage->reset();
}

void TPreyDialogController::showDialog()
{
    closeDialog();
    const LocalPlayerPtr& player = g_game.getLocalPlayer();
    if (!player || !player->hasReachedMain()) {
        m_notOnMain->setButtons(okButton());
        m_notOnMain->showMessage(qtTrId("prey_need_to_reach_main_caption"), qtTrId("prey_need_to_reach_main_text"));
        return;
    }
    if (!m_storage->allDataReceived()) {
        auto action = input::mutableClone(*input::makeAction(A::SET_STATUS_LINE));
        action->text = qtTrId("prey_information_not_available");
        input::publish(action);
        return;
    }
    m_storage->setPreyWildcards(wildcardBalance());
    dialog()->show();
    requestTabSwitch(PreyCreatures);
    for (const int resource : { 0, 1, 10, 0x32 }) {
        auto action = input::mutableClone(*input::makeAction(A::REQUEST_RESOURCE_BALANCE));
        action->parameter = resource;
        input::publish(action);
    }
}

void TPreyDialogController::closeDialog()
{
    if (m_activeTab == PreyCreatures)
        m_page->hideDialogTab();
    m_activeTab = NoTabInitialState;
    Q_EMIT activeTabChanged();
    dialog()->close();
}

void TPreyDialogController::closeButtonClicked() { closeDialog(); }

/// CIP 0x100c5cc20.
void TPreyDialogController::getMorePreyWildcardsClicked() { openStore(3, 1); }

void TPreyDialogController::requestClose() { closeDialog(); }

void TPreyDialogController::requestTabSwitch(const EDialogTab NewTab)
{
    if (m_activeTab == NewTab)
        return;
    if (m_activeTab == PreyCreatures)
        m_page->hideDialogTab();
    m_activeTab = NoTabInitialState;
    if (NewTab == PreyCreatures) {
        m_page->showDialogTab();
        m_activeTab = PreyCreatures;
    }
    Q_EMIT activeTabChanged();
}

void TPreyDialogController::onRequestShowMessageDialog(const QString& Message)
{
    m_info->setButtons(okButton());
    m_info->showMessage(qtTrId("info"), Message);
}

}
