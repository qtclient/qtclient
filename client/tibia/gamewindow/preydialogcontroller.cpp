#include "preydialogcontroller.h"
#include "colorhelper.h"

#include <utility>
#include "config/clientoptions.h"

#include "qmlcomponents/texthelper.h"

#include "game/gamesessionbase.h"
#include "input/gameactionhandlers.h"
#include "prey/preyciptypes.h"

#include <client/outfit.h>
#include <tibia/assets/things.h>

#include <QDateTime>
#include <QLocale>

#include <algorithm>
#include <tibia/game/gamesession.h>
#include <tibia/game/thingsession_forward.h>
#include <tibia/game/tgamenotify.h>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;
using Button = TMessageDialogController::Button;
using EUnlockOption = cip::tibia::qmlcomponents::enums::EPreyUnlockOption;
using EExtendType = cip::tibia::qmlcomponents::enums::EPreyAutomaticExtendType;

constexpr int MaximumBonusGrade = 10;
/// CIP : the length of a prey, in seconds.
constexpr double PreyDuration = 7200.0;
/// CIP DAT_1017dbdc8: the free list reroll interval, in seconds.
constexpr double FreeListRerollInterval = 72000.0;
constexpr int FreeListRerollRefreshMs = 7000;
const QChar MultiStringSeparator(0x9c);

QString english(const qint64 value) { return QLocale(QLocale::English).toString(value); }

uint64_t balance(const tibia::client::ResourceTypes_t type)
{
    return tibia::game::hasLocalPlayer() ? tibia::game::localPlayerResourceBalance(type) : 0;
}

uint64_t totalGold() { return balance(tibia::client::RESOURCE_BANK_BALANCE) + balance(tibia::client::RESOURCE_GOLD_EQUIPPED); }

int wildcardBalance() { return static_cast<int>(std::min<uint64_t>(balance(tibia::client::RESOURCE_PREY_WILDCARDS), INT_MAX)); }

using ::tibia::shared::outfitColor;

/// CIP .
QList<TMessageDialogButton*> cancelButton()
{
    return { new TMessageDialogButton(qtTrId("cancel"), Button::CANCEL, QKeySequence(QStringLiteral("Esc"))) };
}

/// CIP .
QList<TMessageDialogButton*> okButton()
{
    return { new TMessageDialogButton(qtTrId("ok"), Button::OK, QKeySequence(QStringLiteral("Return"))) };
}

input::IGameActionPtr preyAction(const A type, const uint preyId)
{
    return std::make_shared<input::TGameActionPreySystem>(static_cast<uint8_t>(preyId), type);
}

/// CIP store requests: the premium page or a useful thing.
input::IGameActionPtr openStoreAction(store::TStoreOpenParameters parameters)
{
    return std::make_shared<input::TGameActionOpenStore>(std::move(parameters));
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

TPreyBestiarySelectionModel::TPreyBestiarySelectionModel(QObject* parent)
    : cip::TPreyBestiarySelectionModel(parent)
{
    connect(this, &QAbstractItemModel::rowsInserted, this, &TPreyBestiarySelectionModel::rowCountChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &TPreyBestiarySelectionModel::rowCountChanged);
}

void TPreyBestiarySelectionModel::setRaces(const QList<uint16_t>& races)
{
    beginResetModel();
    m_entries.clear();
    m_entries.reserve(races.size());
    for (const uint16_t race : races)
        m_entries.append({ .id = race, .name = capitalizeWords(QString::fromStdString(tibia::game::raceData(race).name)) });
    m_count = static_cast<int>(m_entries.size());
    endResetModel();
    Q_EMIT rowCountChanged();
}

int TPreyBestiarySelectionModel::count() const
{
    return m_count;
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

/// CIP (: "HH:MM").
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

/// CIP : a filled star per grade, then empty ones.
QString TPreyRenderInfo::bonusGradeText() const
{
    QString text;
    const int grade = m_data->bonus().grade;
    for (int i = 0; i < MaximumBonusGrade; ++i)
        text.append(i < grade ? QChar(0x2605) : QChar(0x2606));
    return text;
}

/// CIP .
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

/// CIP .
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

/// CIP .
bool TPreyRenderInfo::hasFreeListReroll() const { return secondsToFreeListReroll() == 0; }

/// CIP (; "HH:MM" from minutes).
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

/// CIP .
double TPreyRenderInfo::timeToFreeListRerollProgress() const { return secondsToFreeListReroll() / FreeListRerollInterval; }

EExtendType TPreyRenderInfo::automaticPreyExtendType() const
{
    switch (m_data->automaticExtendType()) {
        case 2: return EExtendType::LOCK_PREY;
        case 1: return EExtendType::BONUS_REROLL;
        default: return EExtendType::NOT_EXTENDING;
    }
}

/// CIP .
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

/// CIP .
void TPreyRenderInfo::confirmGridPreyCreatureClicked(const int Index)
{
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionSelectPreyMonsterFromList>(static_cast<uint8_t>(preyId()), static_cast<uint8_t>(Index)));
}

/// CIP .
void TPreyRenderInfo::confirmBestiaryPreyCreatureClicked(const int MonsterRaceID)
{
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionSelectPreyMonsterFromBestiary>(static_cast<uint8_t>(preyId()), static_cast<uint16_t>(MonsterRaceID)));
}

/// CIP .
void TPreyRenderInfo::unlockPermanentlyClicked() { Q_EMIT publishGameAction(openStoreAction({.openAction = store::OPEN_USEFUL_THING, .usefulThing = 0})); }

/// CIP .
void TPreyRenderInfo::unlockWithPermiumClicked() { Q_EMIT publishGameAction(openStoreAction({.openAction = store::OPEN_PREMIUM_OR_BOOSTS, .premiumDetail = 0})); }

/// CIP .
void TPreyRenderInfo::autoExtendPreydClicked(const EExtendType QMLAutoExtendType)
{
    int type = 0;
    if (QMLAutoExtendType == EExtendType::BONUS_REROLL)
        type = 1;
    else if (QMLAutoExtendType == EExtendType::LOCK_PREY)
        type = 2;
    Q_EMIT requestChangeAutoExtendType(preyId(), type);
}

/// A new filter rebuilds the race list before the proxy filters it.
void TPreyRenderInfo::setMonsterRacesFilter(const QString& NameFilter)
{
    if (m_monsterRacesFilter == NameFilter)
        return;
    m_monsterRacesFilter = NameFilter;
    onMonsterRaceListChanged();
    if (m_bestiaryProxy.nameFilter() != m_monsterRacesFilter)
        m_bestiaryProxy.setNameFilter(m_monsterRacesFilter);
}

bool TPreyPreyPageController::clientSettingSmoothFiltering() const
{
    return m_clientOptions && m_clientOptions->antialiasingMode() == 1;
}

TPreyPreyPageController::TPreyPreyPageController(config::TClientOptions* ClientOptions, QQmlEngine*, QQuickItem*, QObject* parent)
    : cip::tibia::gamewindow::TPreyPreyPageController(parent)
    , m_clientOptions(ClientOptions)
    , m_bonusReroll(new TMessageDialogController(this))
    , m_listReroll(new TMessageDialogController(this))
    , m_choosePrey(new TMessageDialogController(this))
    , m_autoReroll(new TMessageDialogController(this))
    , m_lockPrey(new TMessageDialogController(this))
{
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
    connect(&m_freeRerollTimer, &QTimer::timeout, this, &TPreyPreyPageController::onAdvanceTimeUntilFreeListReroll);
}

/// Every slot's free list reroll countdown is shown anew.
void TPreyPreyPageController::onAdvanceTimeUntilFreeListReroll()
{
    for (QObject* info : std::as_const(m_renderInfos))
        Q_EMIT static_cast<TPreyRenderInfo*>(info)->timeUntilFreeListRerollChanged();
}

/// The session's prey storage: its prices and wildcards.
void TPreyPreyPageController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    closeMessages();
    m_gameSessionConnections.disconnectAll();
    m_gameSession = GameSession;
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    m_storage = session ? session->preyStorage().get() : nullptr;
    if (!m_storage)
        return;
    prey::TPreyStorage* storage = m_storage;
    const auto add = [this](const QMetaObject::Connection& connection) { m_gameSessionConnections.add(connection); };
    add(connect(storage, &prey::TPreyStorage::listRerollPriceChanged, this, [this] {
        Q_EMIT listRerollPriceChanged();
        Q_EMIT listRerollTooExpensiveChanged();
    }));
    add(connect(storage, &prey::TPreyStorage::bonusRerollPriceChanged, this, &TPreyPreyPageController::bonusRerollPriceChanged));
    add(connect(storage, &prey::TPreyStorage::bonusRerollPossibleChanged, this, &TPreyPreyPageController::bonusRerollTooExpensiveChanged));
    add(connect(storage, &prey::TPreyStorage::choosePreyPriceChanged, this, &TPreyPreyPageController::choosePreyPriceChanged));
    add(connect(storage, &prey::TPreyStorage::choosePreyPossibleChanged, this, &TPreyPreyPageController::choosePreyTooExpensiveChanged));
    add(connect(storage, &prey::TPreyStorage::preyWildcardAmountChanged, this, &TPreyPreyPageController::preyWildcardAmountChanged));
}

QString TPreyPreyPageController::preyWildcards() const { return english(m_storage->preyWildcards()); }
QString TPreyPreyPageController::listRerollPrice() const { return qmlcomponents::thousandShortcutsMultiString(m_storage->listRerollPrice()); }
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
        connect(info, &TPreyRenderInfo::publishGameAction, this, &TPreyPreyPageController::publishGameAction);
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

void TPreyPreyPageController::hideDialogTab()
{
    m_freeRerollTimer.stop();
    // CIP: the slot render infos go and the list change is announced (deferred).
    const QList<QObject*> old = std::exchange(m_renderInfos, {});
    for (QObject* info : old)
        info->deleteLater();
    if (!m_preyListTimer.isActive())
        m_preyListTimer.start(0);
}

TPreyPreyPageController::~TPreyPreyPageController()
{
    m_freeRerollTimer.stop();
    qDeleteAll(std::exchange(m_renderInfos, {}));
}

/// Asks to pay `price` wildcards; when the storage says it cannot be paid,
/// only Cancel.
void TPreyPreyPageController::askToUseWildcards(TMessageDialogController* box, const int price, const bool possible)
{
    const int wildcards = m_storage->preyWildcards();
    const QString currency = qtTrId("currency_view_preywildcard_tooltip");
    QString text;
    if (!possible) {
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
    askToUseWildcards(m_bonusReroll, m_storage->bonusRerollPrice(), m_storage->bonusRerollPossible());
}

void TPreyPreyPageController::onRequestListReroll(const uint preyId)
{
    m_pendingPreyId = preyId;
    const prey::TPreyData* data = m_storage->slot(static_cast<int>(preyId));
    const uint64_t gold = totalGold();
    const auto price = static_cast<uint64_t>(m_storage->listRerollPrice());
    QString text;
    if (data && data->nextFreeListReroll() <= QDateTime::currentSecsSinceEpoch()) {
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
    askToUseWildcards(m_choosePrey, m_storage->choosePreyPrice(), m_storage->choosePreyPossible());
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
        Q_EMIT publishGameAction(std::make_shared<input::TGameActionChangeAutoPreyExtendType>(static_cast<uint8_t>(preyId), 0));
    }
}

void TPreyPreyPageController::onConfirmBonusReroll() { Q_EMIT publishGameAction(preyAction(A::PREY_REQUEST_BONUS_REROLL, m_pendingPreyId)); }
void TPreyPreyPageController::onConfirmListReroll() { Q_EMIT publishGameAction(preyAction(A::PREY_REQUEST_LIST_REROLL, m_pendingPreyId)); }
void TPreyPreyPageController::onConfirmChoosePrey() { Q_EMIT publishGameAction(preyAction(A::PREY_REQUEST_FREEZE_AND_SHOW_BESTIARY_MONSTERS, m_pendingPreyId)); }

void TPreyPreyPageController::onConfirmEnableAutoRerollBonus()
{
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionChangeAutoPreyExtendType>(static_cast<uint8_t>(m_pendingPreyId), 1));
}

void TPreyPreyPageController::onConfirmEnableLockPrey() { Q_EMIT publishGameAction(std::make_shared<input::TGameActionChangeAutoPreyExtendType>(static_cast<uint8_t>(m_pendingPreyId), 2)); }

/// CIP .
void TPreyPreyPageController::getMorePreyWildcardsClicked() { Q_EMIT publishGameAction(openStoreAction({.openAction = store::OPEN_USEFUL_THING, .usefulThing = 1})); }

TPreyDialogController::TPreyDialogController(config::TClientOptions* ClientOptions, QQmlEngine* Engine, QQuickItem* DialogParentItem,
                                             QObject* parent)
    : TDialogController(QStringLiteral("prey/PreyDialog.qml"), Engine, DialogParentItem, parent)
    , m_page(new TPreyPreyPageController(ClientOptions, Engine, DialogParentItem, this))
    , m_notOnMain(new TMessageDialogController(this))
    , m_info(new TMessageDialogController(this))
{
    m_preyPageController = m_page;
    connect(m_page, &TPreyPreyPageController::publishGameAction, this, &TPreyDialogController::publishGameAction);
    m_notOnMain->setCloseOnButtonClicked(true);
    m_info->setCloseOnButtonClicked(true);
    // Host: the gold and wildcard balances arrive on the game notify bus.
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::resourcesBalanceChanged, this,
            &TPreyDialogController::onBalanceChanged);
}

TPreyDialogController::~TPreyDialogController() = default;

/// The session's prey storage and handler: the dialog and its messages close,
/// the creatures page follows the session.
void TPreyDialogController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    m_gameSession = GameSession;
    m_notOnMain->dialog()->close();
    m_info->dialog()->close();
    closeDialog();
    m_gameSessionConnections.disconnectAll();
    m_storage = nullptr;
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    if (!session)
        return;
    m_storage = session->preyStorage().get();
    m_page->connectToGameSession(m_gameSession);
    m_gameSessionConnections.add(
        connect(m_storage, &prey::TPreyStorage::preyWildcardAmountChanged, this, &TPreyDialogController::preyWildcardAmountChanged));
    m_gameSessionConnections.add(connect(session->preyProtocolMessageHandler().get(), &prey::TPreyProtocolMessageHandler::requestShowMessageDialog,
                                         this, &TPreyDialogController::onRequestShowMessageDialog));
}

int TPreyDialogController::preyWildcards() const { return m_storage ? m_storage->preyWildcards() : 0; }

void TPreyDialogController::onBalanceChanged()
{
    if (!m_storage)
        return;
    m_storage->setPreyWildcards(wildcardBalance());
    m_page->onBalanceChanged();
}

void TPreyDialogController::showDialog()
{
    closeDialog();
    if (!tibia::game::hasLocalPlayer() || !tibia::game::localPlayerHasReachedMain()) {
        m_notOnMain->setButtons(okButton());
        m_notOnMain->showMessage(qtTrId("prey_need_to_reach_main_caption"), qtTrId("prey_need_to_reach_main_text"));
        return;
    }
    if (!m_storage || !m_storage->allDataReceived()) {
        auto action = std::make_shared<input::TGameActionSetStatusLine>(qtTrId("prey_information_not_available"));
        Q_EMIT publishGameAction(action);
        return;
    }
    m_storage->setPreyWildcards(wildcardBalance());
    dialog()->show();
    requestTabSwitch(PreyCreatures);
    for (const int resource : { 0, 1, 10, 0x32 }) {
        auto action = std::make_shared<input::TGameActionRequestResourceBalance>(resource);
        Q_EMIT publishGameAction(action);
    }
}

void TPreyDialogController::handleGameAction(const input::IGameActionPtr& pGameAction)
{
    if (pGameAction && pGameAction->gameActionType() == A::OPEN_PREY_DIALOG)
        showDialog();
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

/// CIP .
void TPreyDialogController::getMorePreyWildcardsClicked() { Q_EMIT publishGameAction(openStoreAction({.openAction = store::OPEN_USEFUL_THING, .usefulThing = 1})); }

void TPreyDialogController::requestClose() { closeDialog(); }

/// CIP: the creatures tab's page is hidden and shown again even when the
/// tab does not change.
void TPreyDialogController::requestTabSwitch(const EDialogTab NewTab)
{
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
