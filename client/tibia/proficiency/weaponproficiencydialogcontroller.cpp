#include "proficiency/weaponproficiencydialogcontroller.h"
#include "game/inspectedobject.h"

#include "platform/platform.h"

#include "config/clientoptions.h"
#include "analysers/itempricestorage.h"
#include "exaltation/exaltationstorage.h"
#include "game/gameciptypes.h"
#include "game/gamesessionbase.h"
#include "game/playerdata.h"
#include "gamewindow/messagedialogcontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "proficiency/proficiencyperks.h"

#include <client/staticdata.h>
#include <tibia/appearances/appearancetype.h>
#include <tibia/assets/things.h>

#include <algorithm>
#include <array>
#include <tibia/game/gamenotify.h>
#include "game/currentgamesession.h"
#include "protocol/clientmessages.h"
#include <tibia/game/tgamenotify.h>
#include <tibia/appearances/appearanceinstance.h>

namespace tibia::proficiency {

namespace {

using Args = std::vector<std::any>;
using A = input::EGameAction;

constexpr int MaxProficiencyLevels = 7;
/// CIP's inspection reply type for INSPECT_PROFICIENCY.
constexpr uint8_t ProficiencyInspection = 2;
constexpr int MaxShapedPerks = 2;
constexpr int MaxRefineRank = 10;
constexpr int ShapingUnlockLevel = 3;
constexpr int FirstModifyDustCost = 250;
constexpr int SecondModifyDustCost = 1000;
/// CIP 's REQUEST_RESOURCE_BALANCE resources.
constexpr std::array<int, 2> DialogResources { 70, 73 };

/// The current session's player data while the session has a character.
const tibia::game::TPlayerData* localPlayerData()
{
    const tibia::game::TGameSessionBase* session = tibia::game::currentGameSession();
    return session && session->hasLocalPlayer() ? session->playerData().get() : nullptr;
}

bool sessionIsOnline()
{
    const tibia::game::TGameSessionBase* session = tibia::game::currentGameSession();
    return session && session->isOnline();
}

bool sessionCanPerformGameAction()
{
    const tibia::game::TGameSessionBase* session = tibia::game::currentGameSession();
    return session && session->canPerformGameAction();
}

uint64_t localPlayerResource(const tibia::client::ResourceTypes_t type)
{
    const tibia::game::TPlayerData* data = localPlayerData();
    return data ? data->resourceBalance(type) : 0;
}

int profession()
{
    // The classic vocation id counts promoted vocations from 11; its last digit is the profession.
    const tibia::game::TPlayerData* data = localPlayerData();
    return data ? (data->vocation() + (data->promoted() ? 10 : 0)) % 10 : 0;
}

bool vocationMatches(const std::vector<uint32_t>& professions)
{
    if (professions.empty())
        return true;
    const int own = profession();
    return std::ranges::any_of(professions, [own](const uint32_t p) { return static_cast<int>(p) == own; });
}

std::vector<TPerkPick> flaggedPicks(const TProficiencyData& data)
{
    std::vector<TPerkPick> picks;
    std::ranges::copy_if(data.picks, std::back_inserter(picks), [](const TPerkPick& pick) { return pick.picked; });
    return picks;
}

}

TWeaponObjectsModel::TWeaponObjectsModel(config::TClientOptions* ClientOptions, TProficiencyStorage* storage, QObject* parent)
    : cip::tibia::gamewindow::TAppearanceTypeListModel(parent)
    , m_clientOptions(ClientOptions)
    , m_storage(storage)
{
    if (m_clientOptions)
        connect(m_clientOptions, &config::TClientOptions::appearanceTypeListModelOptionsChanged, this,
                &TWeaponObjectsModel::onItemPriceVisualizationChanged);
}

void TWeaponObjectsModel::onItemPriceVisualizationChanged()
{
    if (!m_entries.empty())
        Q_EMIT dataChanged(index(0), index(static_cast<int>(m_entries.size()) - 1),
                           { LootvalueImageSourceUnderItem, LootvalueImageSourceOverItem });
}

// CIP roles 0x10a / 0x10b: the item price storage's loot value
// level 1..5 as image strip frame level-1, under the item for style 1 and
// over it for style 2 .
QString TWeaponObjectsModel::lootValueImageSource(const uint16_t typeId, const int style) const
{
    if (!m_clientOptions || m_clientOptions->colorizeLootStyle() != style)
        return {};
    const int level = analysers::TItemPriceStorage::instance()->lootValueLevel(typeId);
    if (level == 0)
        return {};
    return QStringLiteral("image://%1/%2")
        .arg(style == 1 ? QStringLiteral("containerslot-glowingborders") : QStringLiteral("containerslot-coloredges"))
        .arg(level - 1);
}

int TWeaponObjectsModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QVariant TWeaponObjectsModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() >= static_cast<int>(m_entries.size()))
        return {};
    const gamewindow::TAppearanceTypeEntry& entry = m_entries[index.row()].type;
    switch (role) {
    case TypeID:
    case DisplayTypeID:
        return entry.typeId;
    case CumulativeCount:
    case TypeCount:
        return 0;
    case CountString:
        return QString();
    case LootvalueImageSourceUnderItem:
        return lootValueImageSource(entry.typeId, 1);
    case LootvalueImageSourceOverItem:
        return lootValueImageSource(entry.typeId, 2);
    case TypeName:
        return entry.name.isEmpty() ? qtTrId("dummy_unknown") : entry.name;
    case LiquideType:
    case HookDirection:
    case UpgradeTier:
    case DecoItemObjectID:
        return 0;
    case ProficiencyXP:
        return m_storage->xp(entry.typeId);
    case ProficiencyLevel:
        return m_storage->decorationLevel(entry.typeId);
    case HasProficiencyMastery:
        return m_storage->hasMastery(entry.typeId);
    default:
        return {};
    }
}

QHash<int, QByteArray> TWeaponObjectsModel::roleNames() const
{
    return {
        { TypeID, "typeID" },
        { DisplayTypeID, "displayTypeID" },
        { CumulativeCount, "cumulativeCount" },
        { TypeCount, "typeCount" },
        { CountString, "countString" },
        { TypeName, "typeName" },
        { LiquideType, "liquideType" },
        { HookDirection, "hookDirection" },
        { LootvalueImageSourceUnderItem, "lootvalueImageSourceUnderItem" },
        { LootvalueImageSourceOverItem, "lootvalueImageSourceOverItem" },
        { UpgradeTier, "upgradeTier" },
        { DecoItemObjectID, "decoItemObjectID" },
        { ProficiencyXP, "proficiencyXP" },
        { ProficiencyLevel, "proficiencyLevel" },
        { HasProficiencyMastery, "hasProficiencyMastery" },
    };
}

void TWeaponObjectsModel::rebuild()
{
    std::vector<TWeaponEntry> entries;
    for (const TAppearanceTypePtr& type : tibia::appearances::TAppearancesManager::instance().getProficiencyObjectTypes()) {
        TWeaponEntry entry;
        entry.type.typeId = type->getId();
        entry.type.name = QString::fromStdString(type->getName());
        entry.type.requiredLevel = static_cast<int>(type->getMinimumLevel());
        entry.type.professions = type->restrictVocationMask();
        entry.type.hasClothSlot = type->isCloth();
        entry.type.clothSlot = type->getClothSlot();
        entry.weaponType = type->getAppearanceWeaponType();
        entries.push_back(std::move(entry));
    }
    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
    // CIP hands the character's item price storage to the model.
    if (!m_pricesConnection) {
        analysers::TItemPriceStorage* prices = analysers::TItemPriceStorage::instance();
        m_pricesConnection = connect(prices, &analysers::TItemPriceStorage::configurationChangedFromUserInput, this,
                                     &TWeaponObjectsModel::onItemPriceVisualizationChanged);
        connect(prices, &analysers::TItemPriceStorage::marketPricesAvailable, this, &TWeaponObjectsModel::onItemPriceVisualizationChanged);
    }
}

void TWeaponObjectsModel::onObjectProficiencyXPChanged()
{
    if (m_entries.empty())
        return;
    Q_EMIT dataChanged(index(0), index(static_cast<int>(m_entries.size()) - 1),
                       { ProficiencyXP, ProficiencyLevel, HasProficiencyMastery });
}

TWeaponObjectsProxy::TWeaponObjectsProxy(TWeaponObjectsModel* model, QObject* parent)
    : cip::tibia::gamewindow::TAppearanceTypeListModelSortFilterProxy(parent)
    , m_model(model)
{
    m_typeIdEnumValue = TWeaponObjectsModel::TypeID;
    tibia::platform::configureNaturalCollator(m_collator);
    setSortRole(TWeaponObjectsModel::ProficiencyXP);
    setSourceModel(model);
    sort(0);
}

void TWeaponObjectsProxy::resetFilters()
{
    beginFilterChange();
    m_weaponTypes.clear();
    m_name.clear();
    m_filter = {};
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
    Q_EMIT filterChanged();
}

void TWeaponObjectsProxy::setName(const QString& name)
{
    if (name == m_name)
        return;
    beginFilterChange();
    m_name = name;
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
    Q_EMIT filterChanged();
}

void TWeaponObjectsProxy::setTypeIds(const std::set<int>& weaponTypes)
{
    if (weaponTypes == m_weaponTypes)
        return;
    beginFilterChange();
    m_weaponTypes = weaponTypes;
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
    Q_EMIT filterChanged();
}

void TWeaponObjectsProxy::setFilter(const gamewindow::TAppearanceTypeFilter& filter)
{
    if (filter == m_filter)
        return;
    beginFilterChange();
    m_filter = filter;
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
    Q_EMIT filterChanged();
}

int TWeaponObjectsProxy::rowOf(const uint16_t typeId) const
{
    for (int row = 0; row < rowCount(); ++row) {
        if (typeIdAt(row) == typeId)
            return row;
    }
    return -1;
}

uint16_t TWeaponObjectsProxy::typeIdAt(const int row) const
{
    const QModelIndex source = mapToSource(index(row, 0));
    return source.isValid() ? m_model->entry(source.row()).type.typeId : 0;
}

/// The name (case-insensitive part), the weapon types and the filter buttons
/// for the player's level and profession.
bool TWeaponObjectsProxy::filterAcceptsRow(const int sourceRow, const QModelIndex&) const
{
    if (sourceRow < 0 || sourceRow >= m_model->size() || !localPlayerData())
        return false;
    const TWeaponEntry& entry = m_model->entry(sourceRow);
    bool accepted = m_name.isEmpty() || entry.type.name.indexOf(m_name, 0, Qt::CaseInsensitive) != -1;
    if (!m_weaponTypes.empty())
        accepted = accepted && m_weaponTypes.contains(static_cast<int>(entry.weaponType));
    return m_filter.accepts(entry.type, std::nullopt, localPlayerData()->level(), profession()) && accepted;
}

bool TWeaponObjectsProxy::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    const uint32_t xpA = m_model->xp(left.row());
    const uint32_t xpB = m_model->xp(right.row());
    if (xpA != xpB)
        return xpA > xpB;
    return m_collator.compare(m_model->entry(left.row()).type.name, m_model->entry(right.row()).type.name) < 0;
}

TQmlWeaponTypes::TQmlWeaponTypes(const QString& text, std::set<int> types, QObject* parent)
    : cip::tibia::gamewindow::TQmlWeaponTypes(parent)
    , m_types(std::move(types))
{
    m_text = text;
}

int TQmlWeaponTypes::anyWeaponIdentifier() const { return m_types.empty() ? 0 : *m_types.begin(); }

TWeaponProficiencyDialogController::TWeaponProficiencyDialogController(config::TClientOptions* ClientOptions, QQmlEngine* engine,
                                                                       QQuickItem* dialogParentItem, QObject* parent)
    : TDialogController(QStringLiteral("WeaponProficiencyDialog.qml"), engine, dialogParentItem, parent)
    // Host: CIP reads the proficiency storage from the game session; qtclient's
    // is the game client's, alive before any controller.
    , m_storage(TProficiencyStorage::instance())
    , m_model(ClientOptions, m_storage)
    , m_proxy(&m_model)
    , m_confirm(new gamewindow::TMessageDialogController(this))
{
    m_weaponObjectsModel = &m_proxy;
    m_selectedLevelIndex = -1;
    m_selectedPerkIndex = -1;
    // The combo box entries, in CipSoft's order.
    m_weaponTypes = {
        new TQmlWeaponTypes(qtTrId("market_category_axes"), { 2 }, this),
        new TQmlWeaponTypes(qtTrId("market_category_clubs"), { 3 }, this),
        new TQmlWeaponTypes(qtTrId("market_category_distanceweapons"), { 5, 6, 8 }, this),
        new TQmlWeaponTypes(qtTrId("market_category_fist_weapons"), { 4 }, this),
        new TQmlWeaponTypes(qtTrId("market_category_swords"), { 1 }, this),
        new TQmlWeaponTypes(qtTrId("market_category_wands"), { 7 }, this),
        new TQmlWeaponTypes(qtTrId("market_category_all_weapons"), {}, this),
    };
    // Weapon type 0 selects "all weapons".
    setWeaponTypeFilter(0);

    m_confirm->setYesNoButtons();
    m_confirm->setCloseOnButtonClicked(true);
    m_confirm->setMessageDialogTextFormat(gamewindow::TMessageDialogController::RICH_TEXT);
    connect(m_confirm, &gamewindow::TMessageDialogController::acceptClicked, this, [this](bool) {
        m_onDiscard = nullptr;
        if (auto accept = std::exchange(m_onAccept, nullptr))
            accept();
    });
    connect(m_confirm, &gamewindow::TMessageDialogController::discardClicked, this, [this](bool) {
        m_onAccept = nullptr;
        if (auto discard = std::exchange(m_onDiscard, nullptr))
            discard();
    });

    const auto onSourceChanged = [this] {
        Q_EMIT decorativeSourceChanged();
        Q_EMIT xpInformationChanged();
    };
    connect(this, &TWeaponProficiencyDialogController::currentTypeIdChanged, this, onSourceChanged);
    connect(this, &TWeaponProficiencyDialogController::proficiencyModelChanged, this, onSourceChanged);

    // CIP : the character info dialog passes the type, the button
    // bar and the context menus the object.
    m_gameActions.on(A::OPEN_WEAPON_PROFICIENCY_DIALOG, [this](const input::gameactions::TGameActionOpenWeaponProficiencyDialog& action) {
        showDialog(action.typeId());
        return true;
    });
}

int TWeaponProficiencyDialogController::maxProficiencyLevels() const { return MaxProficiencyLevels; }

void TWeaponProficiencyDialogController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    closeDialog();
    m_gameSessionConnections.disconnectAll();
    m_gameSession = GameSession;
    // Host: qtclient's weapons list reads the appearances and the proficiency
    // storage itself.
    m_model.rebuild();
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    if (!session)
        return;
    // Host: CIP hands the storage to the weapons model, which follows it
    // itself; qtclient's model is refreshed from the storage's XP changes.
    m_gameSessionConnections.add(connect(m_storage, &TProficiencyStorage::proficiencyXPChanged, &m_model,
                                         &TWeaponObjectsModel::onObjectProficiencyXPChanged));
    m_gameSessionConnections.add(connect(m_storage, &TProficiencyStorage::currentProficiencyProgressChanged, this,
                                         &TWeaponProficiencyDialogController::xpInformationChanged));
    m_gameSessionConnections.add(connect(m_storage, &TProficiencyStorage::proficiencyDataChanged, this,
                                         [this](const uint typeId) { onProficiencyDataChanged(static_cast<uint16_t>(typeId)); }));
    m_gameSessionConnections.add(connect(session->gameProtocolMessageHandler().get(), &game::TGameProtocolMessageHandler::inspectObjectDataReceived,
                                         this, [this](const std::vector<gamewindow::TInspectObjectData>& list,
                                                      const game::EInspectionListWindow window) {
                                             onInspectObjectDataReceived(game::toItemInspectionData(list, window));
                                         }));
    m_gameSessionConnections.add(
        connect(session->playerData().get(), &game::TPlayerData::playerDataChanged, this, &TWeaponProficiencyDialogController::onPlayerDataChanged));
    m_gameSessionConnections.add(connect(session->exaltationStorage().get(), &exaltation::TExaltationStorage::exaltedCurrencyBalanceChanged, this,
                                         &TWeaponProficiencyDialogController::onExaltedCurrencyBalanceChanged));
}

TWeaponProficiencyDialogController::~TWeaponProficiencyDialogController()
{
}

void TWeaponProficiencyDialogController::sendItemInfo(const uint16_t typeId) const
{
    if (sessionCanPerformGameAction())
        tibia::protocol::gameclient::sendWeaponProficiencyAction(tibia::client::WEAPON_PROFICIENCY_ITEM_INFO, typeId);
}

void TWeaponProficiencyDialogController::showDialog(const uint16_t typeId)
{
    m_onAccept = m_onDiscard = nullptr;
    m_confirm->dialog()->close();
    if (m_model.isEmpty())
        m_model.rebuild();
    m_proxy.resetFilters();

    // The asked weapon when it is a proficiency weapon not already shown,
    // otherwise the list's first entry with every weapon type.
    const TAppearanceTypePtr type = typeId != 0 && tibia::appearances::TAppearancesManager::instance().isValidDatId(typeId, ThingCategoryItem)
        ? tibia::appearances::TAppearanceInstanceFactory::installed()->appearanceType(ThingCategoryItem, typeId)
        : nullptr;
    const bool selected = typeId != 0 && m_gameSession && select(typeId);
    if (!selected || !type) {
        select(m_proxy.typeIdAt(0));
        setWeaponTypeFilter(0);
    } else {
        setFilter(false, vocationMatches(type->getRestrictVocation()), false, false);
        setWeaponTypeFilter(static_cast<int>(type->getAppearanceWeaponType()));
    }

    if (sessionIsOnline()) {
        if (m_storage->listRequested()) {
            sendItemInfo(static_cast<uint16_t>(m_currentTypeId));
        } else {
            if (sessionCanPerformGameAction())
                tibia::protocol::gameclient::sendWeaponProficiencyAction(tibia::client::WEAPON_PROFICIENCY_LIST_INFO);
            m_storage->setListRequested(true);
        }
    }
    for (const int resource : DialogResources) {
        auto action = std::make_shared<input::TGameActionRequestResourceBalance>(resource);
        Q_EMIT publishGameAction(action);
    }
    if (!dialog()->item()) {
        dialog()->create();
        dialog()->show();
    }
}

void TWeaponProficiencyDialogController::sendInspectObject(const uint16_t typeId) const
{
    if (sessionCanPerformGameAction())
        tibia::protocol::gameclient::sendInspectionObject(tibia::client::INSPECT_PROFICIENCY, typeId, 1);
}

bool TWeaponProficiencyDialogController::select(const uint16_t typeId)
{
    if (!m_model.contains(typeId) || typeId == m_currentTypeId)
        return false;
    m_currentTypeId = typeId;
    Q_EMIT currentTypeIdChanged();
    m_currentInspectText.clear();
    Q_EMIT currentInspectTextChanged();
    if (m_selectedLevelIndex != -1 || m_selectedPerkIndex != -1) {
        m_selectedLevelIndex = -1;
        m_selectedPerkIndex = -1;
        updateSelectedPerk();
    }

    if (m_gameSession) {
        sendInspectObject(typeId);
        if (dialog()->item())
            sendItemInfo(typeId);
        // A pending reset stays pending: the next apply still sends it.
        const TProficiencyData* server = m_storage->data(typeId);
        m_local = server ? *server : TProficiencyData{};
    }
    rebuildModel();
    return true;
}

void TWeaponProficiencyDialogController::onInspectObjectDataReceived(const ItemInspectionData& data)
{
    if (data.inspectionType != ProficiencyInspection)
        return;
    QString text = QString::fromStdString(data.name);
    text += QLatin1Char('\n');
    bool first = true;
    for (const auto& [label, value] : data.descriptions) {
        if (!first)
            text += QLatin1Char('\n');
        first = false;
        text += QStringLiteral("%1: %2").arg(QString::fromStdString(label), QString::fromStdString(value));
    }
    m_currentInspectText = text;
    Q_EMIT currentInspectTextChanged();
}

void TWeaponProficiencyDialogController::onPlayerDataChanged()
{
    Q_EMIT canEditPerksChanged();
    updateSelectedPerk();
}

void TWeaponProficiencyDialogController::onExaltedCurrencyBalanceChanged()
{
    Q_EMIT dustInformationChanged();
    updateSelectedPerk();
}

void TWeaponProficiencyDialogController::onProficiencyDataChanged(const uint16_t typeId)
{
    if (typeId != m_currentTypeId)
        return;
    const TProficiencyData* server = m_storage->data(typeId);
    m_local = server ? *server : TProficiencyData{};
    rebuildModel();
}

void TWeaponProficiencyDialogController::rebuildModel()
{
    const QList<QObject*> old = m_proficiencyModel;
    m_proficiencyModel.clear();
    const auto typeId = static_cast<uint16_t>(m_currentTypeId);
    const TProficiencyData* server = m_storage->data(typeId);
    if (const TProficiencyInfo* table = typeId != 0 ? m_storage->info(typeId) : nullptr) {
        const int tableLevels = std::min<int>(static_cast<int>(table->levels.size()), 9);
        for (int i = 0; i < static_cast<int>(table->levels.size()); ++i) {
            const auto local = std::ranges::find_if(m_local.picks, [i](const TPerkPick& pick) { return pick.level == i; });
            const TPerkPick* localPick = local == m_local.picks.end() ? nullptr : &*local;
            const bool serverPicked = server && server->hasPick(i);
            m_proficiencyModel << new TQMLProficiencyLevelInformation(i, tableLevels, typeId, table->levels[i], localPick,
                                                                      m_local.shaped, serverPicked, this);
        }
    }
    Q_EMIT proficiencyModelChanged();
    Q_EMIT dustInformationChanged();
    for (QObject* level : old)
        level->deleteLater();
    updateSelectedPerk();
}

bool TWeaponProficiencyDialogController::picksEqual(const std::vector<TPerkPick>& a, const std::vector<TPerkPick>& b)
{
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i].level != b[i].level || a[i].picked != b[i].picked)
            return false;
        if (a[i].picked && b[i].picked && a[i].perk != b[i].perk)
            return false;
    }
    return true;
}

bool TWeaponProficiencyDialogController::localDiffersFromServer() const
{
    if (!sessionIsOnline() || m_currentTypeId == 0)
        return false;
    const TProficiencyData* server = m_storage->data(static_cast<uint16_t>(m_currentTypeId));
    return server && !picksEqual(server->picks, m_local.picks);
}

void TWeaponProficiencyDialogController::confirmUnsavedChanges(std::function<void()> next)
{
    if (localDiffersFromServer()) {
        m_onAccept = [this, next] {
            onApplyClicked();
            next();
        };
        m_onDiscard = std::move(next);
        m_confirm->showMessage(qtTrId("weapon_proficiency_confirm_perk_changes_caption"),
                               qtTrId("weapon_proficiency_confirm_perk_changes"));
        return;
    }
    next();
}

void TWeaponProficiencyDialogController::weaponObjectSelected(const uint RawTypeId)
{
    if (RawTypeId == m_currentTypeId)
        return;
    const auto typeId = static_cast<uint16_t>(RawTypeId);
    confirmUnsavedChanges([this, typeId] { select(typeId); });
}

void TWeaponProficiencyDialogController::requestClose()
{
    confirmUnsavedChanges([this] { closeDialog(); });
}

void TWeaponProficiencyDialogController::onApplyClicked()
{
    const auto typeId = static_cast<uint16_t>(m_currentTypeId);
    const TProficiencyData* server = m_storage->data(typeId);
    if (m_pendingReset && sessionIsOnline() && server && !flaggedPicks(*server).empty())
        if (sessionCanPerformGameAction())
            tibia::protocol::gameclient::sendWeaponProficiencyAction(tibia::client::WEAPON_PROFICIENCY_RESET_PERKS, typeId);
    std::vector<uint8_t> levels;
    std::vector<uint8_t> perks;
    for (const TPerkPick& pick : flaggedPicks(m_local)) {
        levels.push_back(pick.level);
        perks.push_back(pick.perk);
    }
    if (sessionCanPerformGameAction())
        tibia::protocol::gameclient::sendWeaponProficiencyApply(typeId, levels, perks);
    m_pendingReset = false;
}

void TWeaponProficiencyDialogController::onOkClicked()
{
    onApplyClicked();
    closeDialog();
}

void TWeaponProficiencyDialogController::setWeaponTypeFilter(const int RawWeaponType)
{
    int index = static_cast<int>(m_weaponTypes.size()) - 1;
    for (int i = 0; i < m_weaponTypes.size(); ++i) {
        const auto* entry = static_cast<TQmlWeaponTypes*>(m_weaponTypes[i]);
        if (entry->anyWeaponIdentifier() == RawWeaponType || entry->types().contains(RawWeaponType)) {
            index = i;
            break;
        }
    }
    m_proxy.setTypeIds(static_cast<TQmlWeaponTypes*>(m_weaponTypes[index])->types());
    m_weaponTypesIndex = index;
    Q_EMIT weaponTypeChanged();
}

void TWeaponProficiencyDialogController::setNameFilter(const QString& Name) { m_proxy.setName(Name); }

void TWeaponProficiencyDialogController::setFilter(const bool FilterByLevel, const bool FilterByVocation,
                                                   const bool OnlyOneHandWeapons, const bool OnlyTwoHandWeapons)
{
    gamewindow::TAppearanceTypeFilter filter;
    filter.filterByLevel = FilterByLevel;
    filter.filterByVocation = FilterByVocation;
    filter.onlyOneHandWeapons = OnlyOneHandWeapons;
    filter.onlyTwoHandWeapons = OnlyTwoHandWeapons;
    m_proxy.setFilter(filter);
}

void TWeaponProficiencyDialogController::pickPerk(const uint LevelIndex, const uint PerkIndex)
{
    if (!sessionIsOnline())
        return;
    m_local.pick(static_cast<int>(LevelIndex), static_cast<int>(PerkIndex));
    rebuildModel();
}

void TWeaponProficiencyDialogController::resetPerks()
{
    m_pendingReset = true;
    m_local.picks.clear();
    rebuildModel();
}

void TWeaponProficiencyDialogController::selectPerk(const int LevelIndex, const int PerkIndex)
{
    if (m_selectedLevelIndex == LevelIndex && m_selectedPerkIndex == PerkIndex)
        return;
    m_selectedLevelIndex = LevelIndex;
    m_selectedPerkIndex = PerkIndex;
    updateSelectedPerk();
}

void TWeaponProficiencyDialogController::updateSelectedPerk()
{
    m_selectedPerkCanBeShaped = false;
    m_selectedPerkTooltip.clear();
    m_selectedPerkGuiHelp.clear();
    m_selectedPerkActionPossible = true;
    QStringList reasons;
    const auto refuse = [this, &reasons](const QString& reason) {
        m_selectedPerkActionPossible = false;
        reasons << reason;
    };

    if (!canEditPerks())
        refuse(qtTrId("weapon_proficiency_adjust_not_possibile_tooltip"));
    const auto typeId = static_cast<uint16_t>(m_currentTypeId);
    if (typeId == 0) {
        refuse(qtTrId("weapon_proficiency_nothing_selected"));
    } else {
        const TProficiencyInfo* table = m_storage->info(typeId);
        const int levelIndex = m_selectedLevelIndex;
        const int perkIndex = m_selectedPerkIndex;
        if (!table || levelIndex < 0 || levelIndex >= static_cast<int>(table->levels.size()) || perkIndex < 0
            || perkIndex >= static_cast<int>(table->levels[levelIndex].perks.size())) {
            refuse(qtTrId("weapon_proficiency_no_perk_selected"));
        } else if (std::ranges::any_of(m_local.shaped, [levelIndex, perkIndex](const TShapedSlot& slot) {
                       return slot.level == levelIndex && slot.perk == perkIndex;
                   })) {
            m_selectedPerkCanBeShaped = true;
        } else if (m_local.shaped.size() >= MaxShapedPerks) {
            refuse(qtTrId("weapon_proficiency_max_shaped_perks_reached").arg(MaxShapedPerks));
        } else {
            const int level = m_storage->level(typeId);
            if (level <= levelIndex)
                refuse(qtTrId("weapon_proficiency_perk_level_not_unlocked"));
            if (level < ShapingUnlockLevel)
                refuse(qtTrId("weapon_proficiency_perk_level_3_not_unlocked").arg(ShapingUnlockLevel));
            if (m_local.shaped.size() == 1 && !m_storage->hasMastery(typeId))
                refuse(qtTrId("weapon_proficiency_mastery_needed"));
            if (!hasEnoughDust())
                refuse(qtTrId("weapon_proficiency_not_enough_dust"));
        }
    }

    if (!reasons.isEmpty()) {
        m_selectedPerkTooltip = qtTrId("weapon_proficiency_action_not_possible_because") + QStringLiteral("<br>\n");
        for (const QString& reason : reasons)
            m_selectedPerkTooltip += (qtTrId("bullet_point") + QStringLiteral("<br/>")).arg(reason);
        if (m_selectedPerkTooltip.endsWith(QStringLiteral("<br/>"), Qt::CaseInsensitive))
            m_selectedPerkTooltip.chop(5);
    }
    m_selectedPerkGuiHelp = qtTrId("weapon_proficiency_shaping_general_information").arg(MaxShapedPerks).arg(MaxRefineRank);
    Q_EMIT selectedPerkChanged();
}

bool TWeaponProficiencyDialogController::hasEnoughDust() const
{
    const size_t shaped = m_local.shaped.size();
    if (m_currentTypeId == 0 || shaped >= 2)
        return true;
    const uint64_t dust = localPlayerResource(tibia::client::RESOURCE_FORGE_DUST);
    const uint64_t cost = shaped == 0 ? FirstModifyDustCost : SecondModifyDustCost;
    return cost <= static_cast<uint16_t>(dust);
}

void TWeaponProficiencyDialogController::onModifyShapeClicked()
{
    const auto typeId = static_cast<uint16_t>(m_currentTypeId);
    if (!m_selectedPerkActionPossible || !sessionIsOnline() || typeId == 0)
        return;
    const auto level = static_cast<uint8_t>(m_selectedLevelIndex);
    const auto perk = static_cast<uint8_t>(m_selectedPerkIndex);
    if (!m_selectedPerkCanBeShaped) {
        m_onAccept = [this, typeId, level, perk] {
            onApplyClicked();
            if (sessionCanPerformGameAction())
                tibia::protocol::gameclient::sendWeaponProficiencySlotAction(tibia::client::WEAPON_PROFICIENCY_MODIFY_SLOT, typeId, level, perk);
        };
        m_onDiscard = nullptr;
        m_confirm->showMessage(qtTrId("weapon_proficiency_modify") + QLatin1Char('?'),
                               qtTrId("weapon_proficiency_confirm_modify").arg(modifiyDustCost()));
        return;
    }
    // OPEN_WEAPON_PROFICIENCY_SHAPE_DIALOG {typeId, level, perk, from the perk tree}.
    auto action = std::make_shared<input::gameactions::TGameActionOpenWeaponProficiencyShapeDialog>(static_cast<uint16_t>(typeId), static_cast<uint8_t>(level), static_cast<uint8_t>(perk), true);
    Q_EMIT publishGameAction(action);
}

void TWeaponProficiencyDialogController::closeDialog()
{
    m_onAccept = m_onDiscard = nullptr;
    m_confirm->dialog()->close();
    dialog()->close();
    // The next opening starts without a weapon and without local choices.
    m_currentTypeId = 0;
    m_local = {};
    m_pendingReset = false;
}

QString TWeaponProficiencyDialogController::currentTypeName() const
{
    QString name = qtTrId("weapon_proficiency_nothing_selected");
    if (m_currentTypeId != 0 && tibia::appearances::TAppearancesManager::instance().isValidDatId(static_cast<uint16_t>(m_currentTypeId), ThingCategoryItem)) {
        const TAppearanceTypePtr type = tibia::appearances::TAppearanceInstanceFactory::installed()->appearanceType(ThingCategoryItem, static_cast<uint16_t>(m_currentTypeId));
        if (type)
            name = QString::fromStdString(type->getName());
    }
    return name;
}

bool TWeaponProficiencyDialogController::currentIsUseRestricted() const
{
    // CIP: restricted while nothing is selected or there is no session.
    if (m_currentTypeId == 0 || !tibia::appearances::TAppearancesManager::instance().isValidDatId(static_cast<uint16_t>(m_currentTypeId), ThingCategoryItem))
        return true;
    const TAppearanceTypePtr type = tibia::appearances::TAppearanceInstanceFactory::installed()->appearanceType(ThingCategoryItem, static_cast<uint16_t>(m_currentTypeId));
    if (!type || !localPlayerData())
        return true;
    return localPlayerData()->level() < type->getMinimumLevel() || !vocationMatches(type->getRestrictVocation());
}

QString TWeaponProficiencyDialogController::currentDecorationSource() const
{
    const int level = m_storage->decorationLevel(static_cast<uint16_t>(m_currentTypeId));
    if (level < 8)
        return QStringLiteral("/images/proficiency/icon-masterylevel-%1.png").arg(level);
    return QStringLiteral("/images/proficiency/icon-masterylevel-0.png");
}

QString TWeaponProficiencyDialogController::currentDecorationLevelSource() const
{
    const auto typeId = static_cast<uint16_t>(m_currentTypeId);
    const int level = m_storage->decorationLevel(typeId);
    if (level == 0)
        return {};
    return QStringLiteral("/images/proficiency/icon-masterylevel-%1-%2.png")
        .arg(level)
        .arg(m_storage->hasMastery(typeId) ? QStringLiteral("gold") : QStringLiteral("silver"));
}

uint TWeaponProficiencyDialogController::currentProficiencyXP() const
{
    return m_storage->xp(static_cast<uint16_t>(m_currentTypeId));
}

uint TWeaponProficiencyDialogController::nextLevelProficiencyXP() const
{
    const auto typeId = static_cast<uint16_t>(m_currentTypeId);
    if (typeId == 0)
        return 0;
    const int count = m_storage->levelCount(typeId);
    const int current = m_storage->level(typeId);
    const int next = current + (current < count ? 1 : 0);
    const int last = std::max(count - 1, 0);
    const int index = (next <= last || next > 8) ? next : last + 2;
    return TProficiencyStorage::threshold(index, typeId);
}

uint TWeaponProficiencyDialogController::masteryProficiencyXP() const
{
    const auto typeId = static_cast<uint16_t>(m_currentTypeId);
    if (typeId == 0 || !m_storage->info(typeId))
        return 90000000;
    const int count = m_storage->levelCount(typeId);
    const int index = (count >= 1 && count <= 8) ? count + 1 : count;
    return TProficiencyStorage::threshold(index, typeId, true);
}

bool TWeaponProficiencyDialogController::canEditPerks() const
{
    const tibia::game::TPlayerData* data = tibia::game::currentPlayerData();
    return sessionIsOnline() && data && (data->states().flags.to_ullong() & tibia::client::IconPz) != 0;
}

bool TWeaponProficiencyDialogController::currentProficiencyChanged() const { return localDiffersFromServer(); }

bool TWeaponProficiencyDialogController::hasPickedPerks() const
{
    return m_currentTypeId != 0 && std::ranges::any_of(m_local.picks, [](const TPerkPick& pick) { return pick.picked; });
}

QString TWeaponProficiencyDialogController::modifiyDustCost() const
{
    switch (m_local.shaped.size()) {
    case 0: return QString::number(FirstModifyDustCost);
    case 1: return QString::number(SecondModifyDustCost);
    default: return QStringLiteral("-");
    }
}

bool TWeaponProficiencyDialogController::hasEnoughDustToModify() const { return hasEnoughDust(); }

}
