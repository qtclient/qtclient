#include "characterinfodialogcontroller.h"

#include "skillwheel/skillwheelciptypes.h"

#include "cyclopedia/characterinfostorage.h"
#include "cyclopedia/combatstatsqmldata.h"
#include "cyclopedia/playermiscstatsprovider.h"
#include "cyclopedia/playerdefencestatsprovider.h"
#include "cyclopedia/playeroffencestatsprovider.h"
#include "game/skillswidgetentrydata.h"
#include "gamewindow/characterinfomodels.h"
#include "gamewindow/gamewindowciptypes.h"
#include "gamewindow/smoothfiltering.h"
#include "gamewindow/messagedialogcontroller.h"
#include "gamewindow/outfitdialogcontroller.h"
#include "gamewindow/remainingdialogs.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "qmlcomponents/texthelper.h"

#include <tibia/appearances/appearancetype.h>
#include <tibia/assets/things.h>
#include "game/playerskillstats.h"

#include <client/outfit.h>
#include <client/staticdata.h>

#include <QAbstractListModel>
#include <QKeySequence>
#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/thingsession_forward.h>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;
using Category = cip::tibia::gamewindow::TCharacterInfoDialogController::EDialogCategory;
using game::SkillType;

QString qstr(const std::string& text) { return QString::fromStdString(text); }

QColor outfitColor(const int index)
{
    const QColor c = Outfit::getColor(index);
    return QColor(c.red(), c.green(), c.blue());
}

QString formatDateTime(const uint32_t timestamp)
{
    static qmlcomponents::TTextHelperQmlService textHelper;
    return textHelper.formatDateTime(static_cast<int>(timestamp), true);
}

/// CIP: the data type sent for each of its data types (1..13).
tibia::client::CyclopediaCharacterInfoType_t protocolType(const int dataType)
{
    using T = TCharacterInfoDialogController;
    switch (dataType) {
        case T::GeneralStats: return tibia::client::CYCLOPEDIA_CHARACTERINFO_GENERALSTATS;
        case T::OffenceStats: return tibia::client::CYCLOPEDIA_CHARACTERINFO_OFFENCESTATS;
        case T::DefenceStats: return tibia::client::CYCLOPEDIA_CHARACTERINFO_DEFENCESTATS;
        case T::MiscStats: return tibia::client::CYCLOPEDIA_CHARACTERINFO_MISCSTATS;
        case T::RecentDeaths: return tibia::client::CYCLOPEDIA_CHARACTERINFO_RECENTDEATHS;
        case T::RecentKills: return tibia::client::CYCLOPEDIA_CHARACTERINFO_RECENTPVPKILLS;
        case T::Achievements: return tibia::client::CYCLOPEDIA_CHARACTERINFO_ACHIEVEMENTS;
        case T::ItemSummary: return tibia::client::CYCLOPEDIA_CHARACTERINFO_ITEMSUMMARY;
        case T::OutfitsMounts: return tibia::client::CYCLOPEDIA_CHARACTERINFO_OUTFITSMOUNTS;
        case T::StoreSummary: return tibia::client::CYCLOPEDIA_CHARACTERINFO_STORESUMMARY;
        case T::Inspection: return tibia::client::CYCLOPEDIA_CHARACTERINFO_INSPECTION;
        case T::Badges: return tibia::client::CYCLOPEDIA_CHARACTERINFO_BADGES;
        case T::Titles: return tibia::client::CYCLOPEDIA_CHARACTERINFO_TITLES;
        default: return tibia::client::CYCLOPEDIA_CHARACTERINFO_BASEINFORMATION;
    }
}

/// CIP 's table (data types 2..13).
constexpr Category CategoryOfDataType[] = { Category::OffenceStats, Category::DefenceStats, Category::MiscStats,
                                            Category::Deaths, Category::Kills, Category::Achievements,
                                            Category::Items, Category::OutfitsMountsSummonSkins, Category::StorePurchases,
                                            Category::InspectCharacter, Category::GeneralStats, Category::Titles };

/// CIP 's table (categories 1..13).
constexpr int DataTypeOfCategory[] = { 5, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 13 };

/// CIP : the general stats' two blocks.
constexpr SkillType FirstBlock[] = { SkillType::Level,     SkillType::Experience, SkillType::ExperienceGain,
                                     SkillType::HitPoints, SkillType::Mana,       SkillType::SoulPoints,
                                     SkillType::Capacity,  SkillType::Speed,      SkillType::Food,
                                     SkillType::Stamina,   SkillType::OfflineTraining };
constexpr SkillType SecondBlock[] = { SkillType::MagicLevel, SkillType::Fist,     SkillType::Club,      SkillType::Sword,
                                      SkillType::Axe,        SkillType::Distance, SkillType::Shielding, SkillType::Fishing };

/// CIP: TCharacterBaseQmlInfo.
class TCharacterBaseQmlInfo final : public cip::tibia::gamewindow::TCharacterBaseQmlInfo
{
public:
    TCharacterBaseQmlInfo(const cyclopedia::TCharacterBaseInformation& information, QObject* parent)
        : cip::tibia::gamewindow::TCharacterBaseQmlInfo(parent)
    {
        m_name = information.name;
        m_vocation = information.vocation;
        m_level = information.level;
        const Outfit& outfit = information.outfit;
        m_outfitID = outfit.getId();
        m_headColor = outfitColor(outfit.getHead());
        m_torsoColor = outfitColor(outfit.getBody());
        m_legsColor = outfitColor(outfit.getLegs());
        m_detailColor = outfitColor(outfit.getFeet());
        m_firstAddon = (outfit.getAddons() & 1) != 0;
        m_secondAddon = (outfit.getAddons() & 2) != 0;
        m_isOwnCharacter = information.ownCharacter;
        m_title = information.title;
    }
};

/// CIP: TAccountInfoQmlData ; badges list at +0x170 via .
class TAccountInfoQmlData final : public cip::tibia::gamewindow::TAccountInfoQmlData
{
public:
    TAccountInfoQmlData(const cyclopedia::TAccountInfos& infos, QObject* parent)
        : cip::tibia::gamewindow::TAccountInfoQmlData(parent)
    {
        m_canShowAccountInfos = infos.canShowAccountInfos;
        m_isOnline = infos.online;
        m_isPremium = infos.premium;
        m_loyaltyTitle = infos.loyaltyTitle;
        std::vector<AccountBadgeEntry> badges;
        badges.reserve(infos.badges.size());
        for (const friends::TAccountBadge& badge : infos.badges)
            badges.push_back({ badge.id, badge.name });
        m_accountBadges = new TAccountBadgesModel(std::move(badges), this);
    }
};

/// CIP: TDeathQmlData .
class TDeathQmlData final : public cip::tibia::gamewindow::TDeathQmlData
{
public:
    TDeathQmlData(const RecentDeathEntry& entry, QObject* parent)
        : cip::tibia::gamewindow::TDeathQmlData(parent)
    {
        m_timestamp = formatDateTime(entry.timestamp);
        m_description = qstr(entry.cause);
    }
};

/// CIP: TKillQmlData .
class TKillQmlData final : public cip::tibia::gamewindow::TKillQmlData
{
public:
    TKillQmlData(const RecentPvPKillEntry& entry, QObject* parent)
        : cip::tibia::gamewindow::TKillQmlData(parent)
    {
        static const char* const Status[] = { "charinfo_kill_justified", "charinfo_kill_unjustified", "charinfo_kill_guildwar",
                                              "charinfo_kill_assisted", "charinfo_kill_arena" };
        m_timestamp = formatDateTime(entry.timestamp);
        m_description = qstr(entry.description);
        m_unjustified = qtTrId(entry.status < std::size(Status) ? Status[entry.status] : "dummy_unknown");
        m_unjustifiedCode = entry.status;
    }
};

/// The store summary's list models (CIP vtables : name/count
/// rows, : house item rows), filled once like CIP .
class TStoreSummaryListModel final : public QAbstractListModel
{
public:
    TStoreSummaryListModel(QList<QByteArray> roles, QList<QVariantList> rows, QObject* parent)
        : QAbstractListModel(parent)
        , m_roles(std::move(roles))
        , m_rows(std::move(rows))
    {}

    int rowCount(const QModelIndex& parent) const override { return parent.isValid() ? 0 : static_cast<int>(m_rows.size()); }
    QVariant data(const QModelIndex& index, const int role) const override
    {
        const int column = role - Qt::UserRole;
        if (!index.isValid() || index.row() >= m_rows.size() || column < 0 || column >= m_rows[index.row()].size())
            return {};
        return m_rows[index.row()][column];
    }
    QHash<int, QByteArray> roleNames() const override
    {
        QHash<int, QByteArray> names;
        for (int i = 0; i < m_roles.size(); ++i)
            names.insert(Qt::UserRole + i, m_roles[i]);
        return names;
    }

private:
    QList<QByteArray> m_roles;
    QList<QVariantList> m_rows;
};

/// CIP : hours (unbounded) and minutes, two digits each.
QString formatXpBoostTime(const uint32_t seconds)
{
    return QStringLiteral("%1:%2").arg(seconds / 3600, 2, 10, QLatin1Char('0')).arg((seconds / 60) % 60, 2, 10, QLatin1Char('0'));
}

/// CIP: TStorePurchasesQmlData over the 0xDA store summary
/// (CIP reader case 8).
class TStorePurchasesQmlData final : public cip::tibia::gamewindow::TStorePurchasesQmlData
{
public:
    TStorePurchasesQmlData(const cyclopedia::TStoreSummary& summary, QObject* parent)
        : cip::tibia::gamewindow::TStorePurchasesQmlData(parent)
    {
        m_remainingStoreXpBoost = formatXpBoostTime(summary.storeXpBoostTime);
        m_remainingDailyRewardXpBoost = formatXpBoostTime(summary.dailyRewardXpBoostTime);
        QList<QVariantList> blessings;
        for (const auto& [name, count] : summary.blessings)
            blessings.append({ name, count });
        m_storeBlessings = new TStoreSummaryListModel({ "name", "count" }, std::move(blessings), this);
        m_preySlots = summary.preySlots;
        m_preyWildcards = summary.preyWildcards;
        m_purchasedPermanentWeeklyTaskExpansion = summary.permanentWeeklyTaskExpansion;
        m_collectionTokens = summary.instantRewardAccesses;
        m_hasCharmExpansion = summary.charmExpansion;
        m_hirelings = summary.hirelings;
        for (const QString& job : summary.hirelingJobs)
            m_hirelingJobs.append(job);
        QList<QVariantList> hirelingOutfits;
        for (const auto& [name, count] : summary.hirelingOutfits)
            hirelingOutfits.append({ name, count });
        m_hirelingOutfits = new TStoreSummaryListModel({ "name", "count" }, std::move(hirelingOutfits), this);
        QList<QVariantList> houseItems;
        for (const auto& [id, name, count] : summary.houseItems)
            houseItems.append({ static_cast<int>(id), name, count });
        m_houseItems = new TStoreSummaryListModel({ "appearanceTypeID", "name", "count" }, std::move(houseItems), this);
    }
};

/// CIP : the inventory slots' background images (1..10); the
/// player's entry (99) has none.
const char* const InspectSlotBackgrounds[] = {
    nullptr,
    "/images/inventory-head.png",
    "/images/inventory-neck.png",
    "/images/inventory-back.png",
    "/images/inventory-torso.png",
    "/images/inventory-right-hand.png",
    "/images/inventory-left-hand.png",
    "/images/inventory-legs.png",
    "/images/inventory-feet.png",
    "/images/inventory-finger.png",
    "/images/inventory-hip.png",
};
constexpr int InspectSlots = 10;
constexpr int InspectPlayerSlot = 99;
constexpr int InspectRightHandSlot = 5;
constexpr int InspectLeftHandSlot = 6;

/// CIP: TInspectObjectQMLData of the inspection page. The ctor 
/// keeps only the slot; set is the fill of one entry built by
/// the converter .
class TInspectObjectQmlData final : public cip::tibia::gamewindow::TInspectObjectQMLData
{
public:
    using Descriptions = std::vector<std::pair<std::string, std::string>>;

    TInspectObjectQmlData(const int slot, QObject* parent)
        : cip::tibia::gamewindow::TInspectObjectQMLData(parent)
    {
        m_slotID = slot;
        if (slot > 0 && slot < static_cast<int>(std::size(InspectSlotBackgrounds)))
            m_backgroundImageUrl = QString::fromLatin1(InspectSlotBackgrounds[slot]);
    }

    void setItem(const InspectionInventoryItem& entry)
    {
        setDetails(qstr(entry.name), entry.descriptions);
        if (entry.typeId != 0) {
            m_appearanceID = entry.typeId;
            m_objectCount = static_cast<uint>(std::max<uint16_t>(1, entry.count));
            // CIP : the count on the sprite from 2 on.
            if (m_objectCount > 1)
                m_textOnSprite = QString::number(m_objectCount);
            m_objectUpgradeTier = entry.tier;
            if (entry.fluidOrSplash)
                m_liquidType = static_cast<decltype(m_liquidType)>(entry.fluidOrSubtype);
            if (entry.hookSouth)
                m_hookDirection = static_cast<decltype(m_hookDirection)>(1);
            else if (entry.hookEast)
                m_hookDirection = static_cast<decltype(m_hookDirection)>(2);
            m_hasProficiency = entry.hasProficiency;
        }
        for (const uint16_t icon : entry.imbuements)
            m_imbumentIcons << QStringLiteral("image://%1/%2").arg(QStringLiteral("imbuement-icons")).arg(icon);
    }

    void setPlayer(const CyclopediaCharacterInspection& data)
    {
        setDetails(qstr(data.playerName), data.playerDescriptions);
        m_appearanceID = data.outfit.getId();
        m_objectCount = 1;
        m_headColor = outfitColor(data.outfit.getHead());
        m_torsoColor = outfitColor(data.outfit.getBody());
        m_legsColor = outfitColor(data.outfit.getLegs());
        m_detailColor = outfitColor(data.outfit.getFeet());
        m_firstAddOn = (data.outfit.getAddons() & 1) != 0;
        m_secondAddOn = (data.outfit.getAddons() & 2) != 0;
    }

private:
    /// CIP (HTML rows, the first without top padding) and
    /// (clipboard text).
    void setDetails(const QString& name, const Descriptions& descriptions)
    {
        static const QString Row = QStringLiteral("<tr>  <td width=\"150\" style=\"text-align:right; padding-right:%3px; padding-top:%4px\">%1:</td>"
                                                  "  <td style=\"padding-top:%4px\">%2</td></tr>");
        setObjectName(name);
        m_objectInformation = QStringLiteral("<table width=\"100%\">");
        m_clipboardReadyText = qtTrId("inspect_inspecting").arg(name) + QLatin1Char('\n');
        bool first = true;
        for (const auto& [label, value] : descriptions) {
            const QString key = qstr(label);
            const QString text = qstr(value);
            m_objectInformation += Row.arg(key, text).arg(5).arg(first ? 0 : 5);
            m_clipboardReadyText += QStringLiteral("%1: %2\n").arg(key, text);
            first = false;
        }
        m_objectInformation += QStringLiteral("</table>");
    }
};



}

bool TCharacterInfoDialogController::clientSettingSmoothFiltering() const { return smoothFilteringEnabled(); }

TCharacterInfoDialogController::TCharacterInfoDialogController(QObject* parent)
    : cip::tibia::gamewindow::TCharacterInfoDialogController(parent)
    , m_errorMessage(new TMessageDialogController(this))
{
    m_achievementsController = new TAchievementInfoController(this);
    m_errorMessage->setCloseOnButtonClicked(true);
    // CIP character-trade / transfer QML carriers + dialog controllers.
    auto* character = new TQMLCharacterData(this);
    character->setCharacter({}, {}, 0, {});
    new TAccountCharactersModel(this);
    new TQmlChildTransaction({}, {}, TQmlChildTransaction::TibiaCoins, 0, this);
    new TQmlPossibleMainChar({}, {}, {}, 0, this);
    new TQmlTargetWorld({}, false, false, 0, this);
    new TQmlWorldTransferCondition(false, {}, this);
    new TSpritesheetDataObject({}, this);
    new TSimpleNameAndCountModel(this);
    new TCharacterTradeDialogController(this);
    new TCharacterTradeDuePaymentDialogController(this);
    new TCharacterWorldTransferDialogController(this);
    new TMainCharacterChangeDialogController(this);
    new TMarketCoinTransactionDetailsDialogController(this);
    hookEvents();
}

TCharacterInfoDialogController::~TCharacterInfoDialogController() = default;

TCharacterInfoDialogController::EDialogCategory TCharacterInfoDialogController::dialogCategory() const
{
    const int index = m_dataType - 2;
    if (index >= 0 && index < static_cast<int>(std::size(CategoryOfDataType)))
        return CategoryOfDataType[index];
    return Category::GeneralStats;
}

bool TCharacterInfoDialogController::dataIsLoading() const { return !cyclopedia::TCharacterInfoStorage::instance()->hasInfo(m_dataType); }

void TCharacterInfoDialogController::showDialogTab()
{
    const int dataType = m_nextDataType > BaseInformation ? m_nextDataType : GeneralStats;
    m_nextDataType = GeneralStats;
    showDialogTab(dataType);
}

void TCharacterInfoDialogController::showDialogTab(const int dataType)
{
    // The records are fetched anew each time the page opens, the base
    // information first.
    m_shown = true;
    cyclopedia::TCharacterInfoStorage::instance()->clearAll();
    tibia::game::requestSendCharacterInfo(m_characterId, tibia::client::CYCLOPEDIA_CHARACTERINFO_BASEINFORMATION);
    request(dataType, true);
}

void TCharacterInfoDialogController::hideDialogTab()
{
    m_shown = false;
    cyclopedia::TCharacterInfoStorage::instance()->clearAll();
    if (m_outfitDialog) {
        m_outfitDialog->deleteLater();
        m_outfitDialog = nullptr;
    }
    clearModels({ GeneralStats, OffenceStats, DefenceStats, MiscStats, RecentDeaths, RecentKills, Achievements, ItemSummary,
                  OutfitsMounts, StoreSummary, Inspection });
}

void TCharacterInfoDialogController::setDialogCategory(const EDialogCategory Category)
{
    // A category without a page of its own (the main menus) shows the general stats.
    const int index = static_cast<int>(Category) - 1;
    const bool known = index >= 0 && index < static_cast<int>(std::size(DataTypeOfCategory));
    request(known ? DataTypeOfCategory[index] : GeneralStats, false);
}

/// A new page (or `force`) shows the record already received, otherwise asks
/// the server for its first page.
void TCharacterInfoDialogController::request(const int dataType, const bool force)
{
    if (!force && dataType == m_dataType)
        return;
    m_dataType = dataType;
    if (cyclopedia::TCharacterInfoStorage::instance()->hasInfo(dataType)) {
        onCharacterInfoUpdated(dataType);
    } else {
        sendRequest(dataType, 1);
        // The general stats show the account infos of the badges answer.
        if (dataType == GeneralStats)
            tibia::game::requestSendCharacterInfo(m_characterId, tibia::client::CYCLOPEDIA_CHARACTERINFO_BADGES);
    }
    Q_EMIT dialogCategoryChanged();
    Q_EMIT dataIsLoadingChanged();
}

void TCharacterInfoDialogController::sendRequest(const int dataType, const int page)
{
    // 23 entries per page.
    tibia::game::requestSendCharacterInfo(m_characterId, protocolType(dataType), 23, static_cast<uint16_t>(page));
}

void TCharacterInfoDialogController::onCharacterInfoUpdated(const int dataType)
{
    Q_EMIT dataIsLoadingChanged();
    if (dataType == BaseInformation) {
        updateBaseInfo();
        if (m_dataType != BaseInformation)
            return;
    } else {
        clearModels({ dataType });
        if (m_dataType != dataType)
            return;
    }
    switch (dataType) {
        case GeneralStats:
            rebuildStatsBlocks();
            updateAccountInfos();
            break;
        case OffenceStats: updateOffenceStats(); break;
        case DefenceStats: updateDefenceStats(); break;
        case MiscStats: updateMiscStats(); break;
        case RecentDeaths: updateDeaths(); break;
        case RecentKills: updateKills(); break;
        case Achievements: updateAchievements(); break;
        case ItemSummary: updateItemSummary(); break;
        case OutfitsMounts: updateOutfitsMounts(); break;
        case StoreSummary: updateStorePurchases(); break;
        case Inspection: updateInspection(); break;
        case Titles: updateTitles(); break;
        default: break;
    }
}

void TCharacterInfoDialogController::clearModels(const std::set<int>& dataTypes)
{
    for (const int dataType : dataTypes) {
        switch (dataType) {
            case GeneralStats:
                replace(m_characterStatsFirstBlock, {});
                replace(m_characterStatsSecondBlock, {});
                replace(m_accountInfos, static_cast<QObject*>(nullptr));
                break;
            case OffenceStats: replace(m_offenceStats, static_cast<QObject*>(nullptr)); break;
            case DefenceStats: replace(m_defenceStats, static_cast<QObject*>(nullptr)); break;
            case MiscStats: replace(m_miscStats, static_cast<QObject*>(nullptr)); break;
            case RecentDeaths: replace(m_characterDeaths, {}); break;
            case RecentKills: replace(m_characterKills, {}); break;
            case Achievements:
                if (auto* controller = dynamic_cast<TAchievementInfoController*>(m_achievementsController))
                    controller->reset();
                break;
            case ItemSummary: replace(m_itemSummary, static_cast<QObject*>(nullptr)); break;
            case OutfitsMounts: replace(m_outfitMountModel, static_cast<QObject*>(nullptr)); break;
            case StoreSummary: replace(m_storePurchases, static_cast<QObject*>(nullptr)); break;
            case Inspection: replace(m_inspectPlayerList, {}); break;
            case Titles: replace(m_titlesInfo, static_cast<QObject*>(nullptr)); break;
            default: break;
        }
    }
    Q_EMIT dataModelChanged();
}

void TCharacterInfoDialogController::clearAllModels()
{
    clearModels({ GeneralStats, OffenceStats, DefenceStats, MiscStats, RecentDeaths, RecentKills, Achievements, ItemSummary,
                  OutfitsMounts, StoreSummary, Inspection });
}

void TCharacterInfoDialogController::onErrorReceived(const int dataType, const int errorCode)
{
    if (!m_shown || dataType != m_dataType)
        return;
    // CIP .
    const char* text = nullptr;
    switch (errorCode) {
        case 1: text = "charinfo_error_no_data"; break;
        case 2: text = "charinfo_error_no_permission_for_character"; break;
        case 3: text = "charinfo_error_no_permission_for_inspect"; break;
        default: return;
    }
    m_errorMessage->setButtons({ new TMessageDialogButton(qtTrId("ok"), TMessageDialogController::Button::OK,
                                                          QKeySequence(QStringLiteral("Return"))) });
    m_errorMessage->showMessage(qtTrId("error"), qtTrId(text));
    if (errorCode == 1)
        Q_EMIT errorLoadingCharacter();
    else if (errorCode == 2)
        Q_EMIT noPermissionsToViewCharacter();
    else
        Q_EMIT noPermissionsToInspectCharacter();
}

void TCharacterInfoDialogController::openStoreWithXpBoostCategory()
{
    auto action = input::mutableClone(*input::makeAction(A::OPEN_STORE));
    action->parameter = 1;
    action->instance = 1;
    input::publish(action);
    input::publish(input::makeAction(A::CLOSE_CYCLOPEDIA));
}

void TCharacterInfoDialogController::openBlessingsDialog() { tibia::game::requestBless(); }

void TCharacterInfoDialogController::openItemInformation(const int TypeID)
{
    auto action = input::mutableClone(*input::makeAction(A::OPEN_CYCLOPEDIA));
    action->parameter = 0;
    action->instance = TypeID;
    input::publish(action);
}

void TCharacterInfoDialogController::previousPaginatedPage()
{
    if (m_dataType != RecentDeaths && m_dataType != RecentKills)
        return;
    const qint64 page = cyclopedia::TCharacterInfoStorage::instance()->currentPage(m_dataType);
    if (page > 1)
        sendRequest(m_dataType, static_cast<int>(page - 1));
}

void TCharacterInfoDialogController::nextPaginatedPage()
{
    if (m_dataType != RecentDeaths && m_dataType != RecentKills)
        return;
    const cyclopedia::TCharacterInfoStorage* storage = cyclopedia::TCharacterInfoStorage::instance();
    const qint64 page = storage->currentPage(m_dataType);
    if (page < storage->pageCount(m_dataType))
        sendRequest(m_dataType, static_cast<int>(page + 1));
}

void TCharacterInfoDialogController::openSkillWheelDialog()
{
    // The shown character's wheel; the own one when no character is named.
    uint32_t characterId = m_characterId;
    if (characterId == 0)
        characterId = tibia::game::hasLocalPlayer() ? tibia::game::localPlayerId() : 0;
    skillwheel::TSkillWheelProtocolMessageHandler::requestCharacterSkillWheel(characterId);
}

void TCharacterInfoDialogController::openWeaponProficiencyDialog(const uint AppearanceTypeID)
{
    auto action = input::mutableClone(*input::makeAction(A::OPEN_WEAPON_PROFICIENCY_DIALOG));
    action->instance = static_cast<int>(AppearanceTypeID);
    input::publish(action);
}

void TCharacterInfoDialogController::openOutfitDialog(const int OutfitID, const bool IsOutfit, const bool IsMount,
                                                      const bool IsSummonSkin)
{
    if (!m_outfitDialog)
        m_outfitDialog = new TSelectOutfitDialogController(this);
    m_outfitDialog->openCharacterInfoSelection(OutfitID, IsOutfit, IsMount, IsSummonSkin);
}

void TCharacterInfoDialogController::rebuildStatsBlocks()
{
    const bool xpBoostPurchasable = tibia::game::isOnline() && tibia::game::localPlayerIsStoreExpBoostPurchasable();
    QList<QObject*> first;
    for (const SkillType type : FirstBlock) {
        auto* data = new game::TSkillsWidgetEntryData(this);
        data->assign(game::buildSkillsWidgetEntry(type));
        first.append(data);
        if (type == SkillType::ExperienceGain) {
            game::SkillsWidgetEntry separator;
            separator.separatorType = game::SkillsWidgetEntry::Separator::XP_BOOST_BUTTON;
            separator.showButton = xpBoostPurchasable;
            auto* button = new game::TSkillsWidgetEntryData(this);
            button->assign(separator);
            first.append(button);
        }
    }
    QList<QObject*> second;
    for (const SkillType type : SecondBlock) {
        auto* data = new game::TSkillsWidgetEntryData(this);
        data->assign(game::buildSkillsWidgetEntry(type));
        second.append(data);
    }
    replace(m_characterStatsFirstBlock, std::move(first));
    replace(m_characterStatsSecondBlock, std::move(second));
}

template <class T>
void TCharacterInfoDialogController::replace(T*& member, T* object)
{
    if (member)
        member->deleteLater();
    member = object;
}

void TCharacterInfoDialogController::replace(QList<QObject*>& member, QList<QObject*> objects)
{
    for (QObject* object : member)
        object->deleteLater();
    member = std::move(objects);
}

/// CIP: the base information object (name, vocation, level, outfit, title).
void TCharacterInfoDialogController::updateBaseInfo()
{
    const cyclopedia::TCharacterBaseInformation information = cyclopedia::TCharacterInfoStorage::instance()->baseInformation();
    replace(m_characterBaseInfo, static_cast<QObject*>(new TCharacterBaseQmlInfo(information, this)));
    Q_EMIT dataModelChanged();
}

/// CIP: the offence stats object over the storage's provider.
void TCharacterInfoDialogController::updateOffenceStats()
{
    if (auto provider = cyclopedia::TCharacterInfoStorage::instance()->offenceStats())
        replace(m_offenceStats, static_cast<QObject*>(new cyclopedia::TOffenceStatsQmlData(std::move(provider), this)));
    Q_EMIT dataModelChanged();
}

/// CIP: the defence stats object over the storage's provider.
void TCharacterInfoDialogController::updateDefenceStats()
{
    if (auto provider = cyclopedia::TCharacterInfoStorage::instance()->defenceStats())
        replace(m_defenceStats, static_cast<QObject*>(new cyclopedia::TDefenceStatsQmlData(std::move(provider), this)));
    Q_EMIT dataModelChanged();
}

/// CIP: the misc stats object over the storage's provider and the live skill stats.
void TCharacterInfoDialogController::updateMiscStats()
{
    if (auto provider = cyclopedia::TCharacterInfoStorage::instance()->miscStats())
        replace(m_miscStats, static_cast<QObject*>(new cyclopedia::TMiscStatsQmlData(std::move(provider), this)));
    Q_EMIT dataModelChanged();
}

/// CIP: one death object per entry.
void TCharacterInfoDialogController::updateDeaths()
{
    const cyclopedia::TCharacterInfoStorage* storage = cyclopedia::TCharacterInfoStorage::instance();
    QList<QObject*> deaths;
    for (const RecentDeathEntry& entry : storage->recentDeaths())
        deaths.append(new TDeathQmlData(entry, this));
    replace(m_characterDeaths, std::move(deaths));
    m_currentPage = static_cast<int>(storage->currentPage(RecentDeaths));
    m_numberOfPages = static_cast<int>(storage->pageCount(RecentDeaths));
    Q_EMIT dataModelChanged();
}

/// CIP: one kill object per entry.
void TCharacterInfoDialogController::updateKills()
{
    const cyclopedia::TCharacterInfoStorage* storage = cyclopedia::TCharacterInfoStorage::instance();
    QList<QObject*> kills;
    for (const RecentPvPKillEntry& entry : storage->recentPvpKills())
        kills.append(new TKillQmlData(entry, this));
    replace(m_characterKills, std::move(kills));
    m_currentPage = static_cast<int>(storage->currentPage(RecentKills));
    m_numberOfPages = static_cast<int>(storage->pageCount(RecentKills));
    Q_EMIT dataModelChanged();
}

/// CIP: the store purchases object, built from the summary as it is.
void TCharacterInfoDialogController::updateStorePurchases()
{
    if (const auto summary = cyclopedia::TCharacterInfoStorage::instance()->storeSummary())
        replace(m_storePurchases, static_cast<QObject*>(new TStorePurchasesQmlData(*summary, this)));
    Q_EMIT dataModelChanged();
}

/// The general stats page's account infos, from the badges record.
void TCharacterInfoDialogController::updateAccountInfos()
{
    if (const auto infos = cyclopedia::TCharacterInfoStorage::instance()->accountInfos())
        replace(m_accountInfos, static_cast<QObject*>(new TAccountInfoQmlData(*infos, this)));
    Q_EMIT dataModelChanged();
}

/// Slots 1..10 then the player (99), filled from the entries.
void TCharacterInfoDialogController::updateInspection()
{
    const CyclopediaCharacterInspection inspection = cyclopedia::TCharacterInfoStorage::instance()->inspection();
    const CyclopediaCharacterInspection* data = &inspection;
    QList<QObject*> objects;
    for (int slot = 1; slot <= InspectSlots; ++slot)
        objects.append(new TInspectObjectQmlData(slot, this));
    auto* player = new TInspectObjectQmlData(InspectPlayerSlot, this);
    objects.append(player);
    bool rightHandEmpty = true;
    uint16_t leftHandTypeId = 0;
    bool leftHandDualWield = false;
    for (const InspectionInventoryItem& entry : data->inventoryItems) {
        if (entry.slot < 1 || entry.slot > InspectSlots)
            continue;
        static_cast<TInspectObjectQmlData*>(objects[entry.slot - 1])->setItem(entry);
        if (entry.slot == InspectRightHandSlot && entry.typeId != 0)
            rightHandEmpty = false;
        if (entry.slot == InspectLeftHandSlot && entry.typeId != 0) {
            leftHandTypeId = entry.typeId;
            if (TAppearanceType* type = tibia::game::rawThingType(entry.typeId, ThingCategoryItem))
                leftHandDualWield = type->isDualWield();
        }
    }
    player->setPlayer(*data);
    m_hasDualWielding = leftHandDualWield && rightHandEmpty;
    m_dualWieldingObjectID = leftHandDualWield ? static_cast<int>(leftHandTypeId) : 0;
    replace(m_inspectPlayerList, std::move(objects));
    Q_EMIT dataModelChanged();
}

void TCharacterInfoDialogController::hookEvents()
{
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onInspectionState", [this](GameObject*, const std::vector<std::any>& args) {
        m_inspectionState = client::gamecore::argInt(args, 1);
        Q_EMIT inspectionStateChanged();
        return std::any{};
    });
    cyclopedia::TCharacterInfoStorage* storage = cyclopedia::TCharacterInfoStorage::instance();
    connect(storage, &cyclopedia::TCharacterInfoStorage::infoUpdated, this, [this](const int dataType) {
        if (m_shown)
            onCharacterInfoUpdated(dataType);
    });
    connect(storage, &cyclopedia::TCharacterInfoStorage::errorReceived, this, &TCharacterInfoDialogController::onErrorReceived);
}

}
