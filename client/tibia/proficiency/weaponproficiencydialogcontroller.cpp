#include "proficiency/weaponproficiencydialogcontroller.h"

#include "app/clientoptions.h"
#include "cyclopedia/iteminfostorage.h"
#include "gamewindow/messagedialogcontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "proficiency/proficiencyperks.h"

#include <tibia/world/game.h>
#include <client/item.h>
#include <tibia/world/localplayer.h>
#include <client/staticdata.h>
#include <tibia/assets/thingtype.h>
#include <tibia/assets/things.h>

#include <algorithm>
#include <array>

namespace tibia::proficiency {

namespace {

using Args = std::vector<std::any>;
using A = input::EGameAction;

constexpr int MaxProficiencyLevels = 7;
/// CIP's inspection reply type for INSPECT_PROFICIENCY (Crystal sends 0x02).
constexpr uint8_t ProficiencyInspection = 2;
constexpr int OneHandClothSlot = 6;
constexpr int TwoHandClothSlot = 0;
constexpr int MaxShapedPerks = 2;
constexpr int MaxRefineRank = 10;
constexpr int ShapingUnlockLevel = 3;
constexpr int FirstModifyDustCost = 250;
constexpr int SecondModifyDustCost = 1000;
/// CIP 0x100e26500's REQUEST_RESOURCE_BALANCE resources.
constexpr std::array<int, 2> DialogResources { 70, 73 };

int profession()
{
    const LocalPlayerPtr& player = g_game.getLocalPlayer();
    return player ? player->getVocation() % 10 : 0;
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

TWeaponObjectsModel::TWeaponObjectsModel(TProficiencyStorage* storage, QObject* parent)
    : cip::tibia::gamewindow::TAppearanceTypeListModel(parent)
    , m_storage(storage)
    , m_colorizeLootStyle(app::ClientOptions::load().options().value(QLatin1String("colorizeLootStyle")).toInt())
{
    connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged, this, [this](const QString& name) {
        if (name != QLatin1String("colorizeLootStyle"))
            return;
        m_colorizeLootStyle = app::ClientOptions::load().options().value(QLatin1String("colorizeLootStyle")).toInt();
        onLootValueImagesChanged();
    });
}

void TWeaponObjectsModel::onLootValueImagesChanged()
{
    if (!m_entries.empty())
        Q_EMIT dataChanged(index(0), index(static_cast<int>(m_entries.size()) - 1),
                           { LootvalueImageSourceUnderItem, LootvalueImageSourceOverItem });
}

// CIP 0x100d6acd0 roles 0x10a / 0x10b: the item price storage's loot value
// level 1..5 as image strip frame level-1, under the item for style 1 and
// over it for style 2 (FUN_100927c50 / FUN_100927d10).
QString TWeaponObjectsModel::lootValueImageSource(const uint16_t typeId, const int style) const
{
    const cyclopedia::TItemInfoStorage* prices = cyclopedia::TItemInfoStorage::instance();
    if (!prices || m_colorizeLootStyle != style)
        return {};
    const int level = prices->lootValueLevel(typeId);
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
    const TWeaponEntry& entry = m_entries[index.row()];
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
    for (const ThingTypePtr& type : g_things.getThingTypes(ThingCategoryItem)) {
        if (!type || type->getId() == 0 || !type->hasProficiency())
            continue;
        TWeaponEntry entry;
        entry.typeId = type->getId();
        entry.name = QString::fromStdString(type->getName());
        entry.weaponType = type->getAppearanceWeaponType();
        entry.minimumLevel = type->getMinimumLevel();
        entry.professions = type->getRestrictVocation();
        entry.hasClothSlot = type->isCloth();
        entry.clothSlot = type->getClothSlot();
        entries.push_back(std::move(entry));
    }
    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
    // CIP 0x100e22730 hands the character's item price storage to the model.
    if (cyclopedia::TItemInfoStorage* prices = cyclopedia::TItemInfoStorage::instance(); prices && !m_pricesConnection)
        m_pricesConnection = connect(prices, &cyclopedia::TItemInfoStorage::pricesChanged, this, &TWeaponObjectsModel::onLootValueImagesChanged);
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
#if !defined(Q_OS_WASM)
    // Wasm's POSIX collator logs and ignores both of these.
    m_collator.setNumericMode(true);
    m_collator.setCaseSensitivity(Qt::CaseInsensitive);
#endif
    setSortRole(TWeaponObjectsModel::ProficiencyXP);
    setSourceModel(model);
    sort(0);
}

void TWeaponObjectsProxy::clearFilters()
{
    beginFilterChange();
    m_name.clear();
    m_weaponTypes.clear();
    m_filterByLevel = m_filterByVocation = m_onlyOneHandWeapons = m_onlyTwoHandWeapons = false;
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
    Q_EMIT filterChanged();
}

void TWeaponObjectsProxy::setNameFilter(const QString& name)
{
    if (name == m_name)
        return;
    beginFilterChange();
    m_name = name;
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
}

void TWeaponObjectsProxy::setWeaponTypes(const std::set<int>& types)
{
    beginFilterChange();
    m_weaponTypes = types;
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
}

void TWeaponObjectsProxy::setFilter(const bool byLevel, const bool byVocation, const bool oneHand, const bool twoHand)
{
    beginFilterChange();
    m_filterByLevel = byLevel;
    m_filterByVocation = byVocation;
    m_onlyOneHandWeapons = oneHand;
    m_onlyTwoHandWeapons = twoHand;
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
    return source.isValid() ? m_model->entry(source.row()).typeId : 0;
}

bool TWeaponObjectsProxy::filterAcceptsRow(const int sourceRow, const QModelIndex&) const
{
    if (sourceRow < 0 || sourceRow >= m_model->size())
        return false;
    const TWeaponEntry& entry = m_model->entry(sourceRow);
    if (!m_name.isEmpty() && !entry.name.contains(m_name, Qt::CaseInsensitive))
        return false;
    if (!m_weaponTypes.empty() && !m_weaponTypes.contains(static_cast<int>(entry.weaponType)))
        return false;
    const LocalPlayerPtr& player = g_game.getLocalPlayer();
    if (m_filterByLevel && player && player->getLevel() < entry.minimumLevel)
        return false;
    if (m_filterByVocation && !vocationMatches(entry.professions))
        return false;
    if (m_onlyOneHandWeapons && !(entry.hasClothSlot && entry.clothSlot == OneHandClothSlot))
        return false;
    if (m_onlyTwoHandWeapons && !(entry.hasClothSlot && entry.clothSlot == TwoHandClothSlot))
        return false;
    return true;
}

bool TWeaponObjectsProxy::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    const TWeaponEntry& a = m_model->entry(left.row());
    const TWeaponEntry& b = m_model->entry(right.row());
    const uint32_t xpA = m_model->xp(left.row());
    const uint32_t xpB = m_model->xp(right.row());
    if (xpA != xpB)
        return xpA > xpB;
    return m_collator.compare(a.name, b.name) < 0;
}

TQmlWeaponTypes::TQmlWeaponTypes(const QString& text, std::set<int> types, QObject* parent)
    : cip::tibia::gamewindow::TQmlWeaponTypes(parent)
    , m_types(std::move(types))
{
    m_text = text;
    m_anyWeaponIdentifier = m_types.empty() ? 0 : *m_types.begin();
}

TWeaponProficiencyDialogController::TWeaponProficiencyDialogController(TProficiencyStorage* storage, QObject* parent)
    : TDialogController(QStringLiteral("WeaponProficiencyDialog.qml"), parent)
    , m_storage(storage)
    , m_model(storage)
    , m_proxy(&m_model)
    , m_confirm(new gamewindow::TMessageDialogController(this))
{
    m_weaponObjectsModel = &m_proxy;
    m_maxProficiencyLevels = MaxProficiencyLevels;
    m_selectedLevelIndex = -1;
    m_selectedPerkIndex = -1;
    // CIP 0x100e21c90's combo box entries, in its order.
    m_weaponTypes = {
        new TQmlWeaponTypes(qtTrId("market_category_axes"), { 2 }, this),
        new TQmlWeaponTypes(qtTrId("market_category_clubs"), { 3 }, this),
        new TQmlWeaponTypes(qtTrId("market_category_distanceweapons"), { 5, 6, 8 }, this),
        new TQmlWeaponTypes(qtTrId("market_category_fist_weapons"), { 4 }, this),
        new TQmlWeaponTypes(qtTrId("market_category_swords"), { 1 }, this),
        new TQmlWeaponTypes(qtTrId("market_category_wands"), { 7 }, this),
        new TQmlWeaponTypes(qtTrId("market_category_all_weapons"), {}, this),
    };
    m_weaponTypesIndex = static_cast<int>(m_weaponTypes.size()) - 1;

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
    connect(m_storage, &TProficiencyStorage::proficiencyDataChanged, this, [this](const uint typeId) {
        m_model.onObjectProficiencyXPChanged();
        if (typeId != m_currentTypeId)
            return;
        if (const TProficiencyData* server = m_storage->data(static_cast<uint16_t>(typeId)))
            m_local = *server;
        rebuildModel();
    });
    connect(m_storage, &TProficiencyStorage::proficiencyXPChanged, this, [this] {
        m_model.onObjectProficiencyXPChanged();
        Q_EMIT decorativeSourceChanged();
        Q_EMIT xpInformationChanged();
    });

    m_hooks.emplace_back("g_game", "onParseItemDetail", [this](LuaObject*, const Args& args) {
        const auto* data = args.empty() ? nullptr : std::any_cast<ItemInspectionData>(&args[0]);
        if (!data || data->inspectionType != ProficiencyInspection || !dialog()->isShown() || m_currentTypeId == 0)
            return std::any{};
        // CIP 0x100e23100.
        QString text = QString::fromStdString(data->name);
        text += QLatin1Char('\n');
        QStringList lines;
        for (const auto& [label, value] : data->descriptions)
            lines << QStringLiteral("%1: %2").arg(QString::fromStdString(label), QString::fromStdString(value));
        text += lines.join(QLatin1Char('\n'));
        m_currentInspectText = text;
        Q_EMIT currentInspectTextChanged();
        return std::any{};
    });
    m_hooks.emplace_back("object", "onStatesChange", [this](LuaObject* self, const Args&) {
        if (const LocalPlayerPtr& player = g_game.getLocalPlayer(); player && self == player.get()) {
            Q_EMIT canEditPerksChanged();
            updateSelectedPerk();
        }
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onResourcesBalanceChange", [this](LuaObject*, const Args& args) {
        const auto* type = args.size() > 2 ? std::any_cast<Otc::ResourceTypes_t>(&args[2]) : nullptr;
        if (type && *type == Otc::RESOURCE_FORGE_DUST) {
            Q_EMIT dustInformationChanged();
            updateSelectedPerk();
        }
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onGameEnd", [this](LuaObject*, const Args&) {
        m_onAccept = m_onDiscard = nullptr;
        m_confirm->dialog()->hide();
        if (dialog()->isShown())
            dialog()->close();
        m_currentTypeId = 0;
        m_local = {};
        m_pendingReset = false;
        m_currentInspectText.clear();
        for (QObject* level : std::exchange(m_proficiencyModel, {}))
            level->deleteLater();
        Q_EMIT proficiencyModelChanged();
        m_model.rebuild();
        return std::any{};
    });

    // CIP 0x100e26c10: the character info dialog passes the type, the button
    // bar and the context menus the object.
    input::setActionHandler(A::OPEN_WEAPON_PROFICIENCY_DIALOG, [this](const input::TGameAction& action) {
        uint16_t typeId = static_cast<uint16_t>(action.instance);
        if (typeId == 0 && action.object && action.object->isItem())
            typeId = static_cast<uint16_t>(action.object->getId());
        showDialog(typeId);
        return true;
    });
}

TWeaponProficiencyDialogController::~TWeaponProficiencyDialogController()
{
    input::setActionHandler(A::OPEN_WEAPON_PROFICIENCY_DIALOG, nullptr);
}

void TWeaponProficiencyDialogController::sendItemInfo(const uint16_t typeId) const
{
    g_game.sendWeaponProficiencyAction(Otc::WEAPON_PROFICIENCY_ITEM_INFO, typeId);
}

void TWeaponProficiencyDialogController::showDialog(const uint16_t typeId)
{
    m_onAccept = m_onDiscard = nullptr;
    m_confirm->dialog()->hide();
    if (m_model.isEmpty())
        m_model.rebuild();
    m_proxy.clearFilters();

    const ThingTypePtr type = typeId != 0 && g_things.isValidDatId(typeId, ThingCategoryItem)
        ? g_things.getThingType(typeId, ThingCategoryItem)
        : nullptr;
    if (type && type->getProficiencyId() != 0 && m_proxy.rowOf(typeId) != -1) {
        setFilter(false, vocationMatches(type->getRestrictVocation()), false, false);
        setWeaponTypeFilter(static_cast<int>(type->getAppearanceWeaponType()));
        select(typeId);
    } else {
        setWeaponTypeFilter(0);
        select(m_proxy.typeIdAt(0));
    }

    if (m_storage->listRequested()) {
        sendItemInfo(static_cast<uint16_t>(m_currentTypeId));
    } else {
        g_game.sendWeaponProficiencyAction(Otc::WEAPON_PROFICIENCY_LIST_INFO);
        m_storage->setListRequested(true);
    }
    for (const int resource : DialogResources) {
        auto action = input::mutableClone(*input::makeAction(A::REQUEST_RESOURCE_BALANCE));
        action->parameter = resource;
        input::publish(action);
    }
    dialog()->show();
}

void TWeaponProficiencyDialogController::select(const uint16_t typeId)
{
    if (typeId == 0 || m_proxy.rowOf(typeId) == -1)
        return;
    m_currentTypeId = typeId;
    Q_EMIT currentTypeIdChanged();
    m_currentInspectText.clear();
    Q_EMIT currentInspectTextChanged();
    m_selectedLevelIndex = -1;
    m_selectedPerkIndex = -1;

    g_game.inspectionObject(Otc::INSPECT_PROFICIENCY, typeId, 1);
    if (dialog()->isShown())
        sendItemInfo(typeId);

    const TProficiencyData* server = m_storage->data(typeId);
    m_local = server ? *server : TProficiencyData{};
    m_pendingReset = false;
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

bool TWeaponProficiencyDialogController::localDiffersFromServer() const
{
    const TProficiencyData* server = m_storage->data(static_cast<uint16_t>(m_currentTypeId));
    const TProficiencyData empty;
    const TProficiencyData& reference = server ? *server : empty;
    return flaggedPicks(m_local) != flaggedPicks(reference) || m_local.shaped != reference.shaped;
}

void TWeaponProficiencyDialogController::confirmUnsavedChanges(std::function<void()> next)
{
    if (m_currentTypeId != 0 && m_storage->data(static_cast<uint16_t>(m_currentTypeId)) && localDiffersFromServer()) {
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
    if (typeId == 0)
        return;
    const TProficiencyData* server = m_storage->data(typeId);
    if (m_pendingReset && server && !flaggedPicks(*server).empty())
        g_game.sendWeaponProficiencyAction(Otc::WEAPON_PROFICIENCY_RESET_PERKS, typeId);
    std::vector<uint8_t> levels;
    std::vector<uint8_t> perks;
    for (const TPerkPick& pick : flaggedPicks(m_local)) {
        levels.push_back(pick.level);
        perks.push_back(pick.perk);
    }
    g_game.sendWeaponProficiencyApply(typeId, levels, perks);
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
    m_proxy.setWeaponTypes(static_cast<TQmlWeaponTypes*>(m_weaponTypes[index])->types());
    m_weaponTypesIndex = index;
    Q_EMIT weaponTypeChanged();
}

void TWeaponProficiencyDialogController::setNameFilter(const QString& Name) { m_proxy.setNameFilter(Name); }

void TWeaponProficiencyDialogController::setFilter(const bool FilterByLevel, const bool FilterByVocation,
                                                   const bool OnlyOneHandWeapons, const bool OnlyTwoHandWeapons)
{
    m_proxy.setFilter(FilterByLevel, FilterByVocation, OnlyOneHandWeapons, OnlyTwoHandWeapons);
}

void TWeaponProficiencyDialogController::pickPerk(const uint LevelIndex, const uint PerkIndex)
{
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
    const LocalPlayerPtr& player = g_game.getLocalPlayer();
    const uint64_t dust = player ? player->getResourceBalance(Otc::RESOURCE_FORGE_DUST) : 0;
    const uint64_t cost = shaped == 0 ? FirstModifyDustCost : SecondModifyDustCost;
    return cost <= static_cast<uint16_t>(dust);
}

void TWeaponProficiencyDialogController::onModifyShapeClicked()
{
    const auto typeId = static_cast<uint16_t>(m_currentTypeId);
    if (!m_selectedPerkActionPossible || !g_game.isOnline() || typeId == 0)
        return;
    const auto level = static_cast<uint8_t>(m_selectedLevelIndex);
    const auto perk = static_cast<uint8_t>(m_selectedPerkIndex);
    if (!m_selectedPerkCanBeShaped) {
        m_onAccept = [this, typeId, level, perk] {
            onApplyClicked();
            g_game.sendWeaponProficiencySlotAction(Otc::WEAPON_PROFICIENCY_MODIFY_SLOT, typeId, level, perk);
        };
        m_onDiscard = nullptr;
        m_confirm->showMessage(qtTrId("weapon_proficiency_modify") + QLatin1Char('?'),
                               qtTrId("weapon_proficiency_confirm_modify").arg(modifiyDustCost()));
        return;
    }
    // OPEN_WEAPON_PROFICIENCY_SHAPE_DIALOG {typeId, level, perk, from the perk tree}.
    auto action = input::mutableClone(*input::makeAction(A::OPEN_WEAPON_PROFICIENCY_SHAPE_DIALOG));
    action->instance = typeId;
    action->parameter = level;
    action->category = perk;
    action->enable = true;
    input::publish(action);
}

void TWeaponProficiencyDialogController::closeDialog() { dialog()->close(); }

QString TWeaponProficiencyDialogController::currentTypeName() const
{
    if (m_currentTypeId != 0 && g_things.isValidDatId(static_cast<uint16_t>(m_currentTypeId), ThingCategoryItem)) {
        const ThingTypePtr type = g_things.getThingType(static_cast<uint16_t>(m_currentTypeId), ThingCategoryItem);
        if (type && !type->getName().empty())
            return QString::fromStdString(type->getName());
    }
    return qtTrId("weapon_proficiency_nothing_selected");
}

bool TWeaponProficiencyDialogController::currentIsUseRestricted() const
{
    if (m_currentTypeId == 0 || !g_things.isValidDatId(static_cast<uint16_t>(m_currentTypeId), ThingCategoryItem))
        return false;
    const ThingTypePtr type = g_things.getThingType(static_cast<uint16_t>(m_currentTypeId), ThingCategoryItem);
    const LocalPlayerPtr& player = g_game.getLocalPlayer();
    if (!type || !player)
        return false;
    return player->getLevel() < type->getMinimumLevel() || !vocationMatches(type->getRestrictVocation());
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
    const LocalPlayerPtr& player = g_game.getLocalPlayer();
    return player && (player->getStates() & Otc::IconPz) != 0;
}

bool TWeaponProficiencyDialogController::currentProficiencyChanged() const { return localDiffersFromServer(); }

bool TWeaponProficiencyDialogController::hasPickedPerks() const
{
    return std::ranges::any_of(m_local.picks, [](const TPerkPick& pick) { return pick.picked; });
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
