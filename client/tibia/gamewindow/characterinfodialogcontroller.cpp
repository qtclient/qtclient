#include "characterinfodialogcontroller.h"
#include "colorhelper.h"

#include "cyclopedia/combatstatsqmldata.h"
#include "cyclopedia/playermiscstatsprovider.h"
#include "cyclopedia/playerdefencestatsprovider.h"
#include "cyclopedia/playeroffencestatsprovider.h"
#include "game/skillswidgetentrydata.h"
#include "gamewindow/characterinfomodels.h"
#include "gamewindow/gamewindowciptypes.h"
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

template <class T>
const T* arg(const std::vector<std::any>& args, const size_t index)
{
    return index < args.size() ? std::any_cast<T>(&args[index]) : nullptr;
}

QString qstr(const std::string& text) { return QString::fromStdString(text); }

QColor outfitColor(const int index)
{
    const QColor c = tibia::shared::outfitColor(index);
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
    TCharacterBaseQmlInfo(const std::vector<std::any>& args, QObject* parent)
        : cip::tibia::gamewindow::TCharacterBaseQmlInfo(parent)
    {
        using client::gamecore::argBool;
        using client::gamecore::argInt;
        using client::gamecore::argString;
        m_name = argString(args, 0);
        m_vocation = argString(args, 1);
        m_level = static_cast<int>(argInt(args, 2));
        if (const auto* outfit = arg<Outfit>(args, 3)) {
            m_outfitID = outfit->getId();
            m_headColor = outfitColor(outfit->getHead());
            m_torsoColor = outfitColor(outfit->getBody());
            m_legsColor = outfitColor(outfit->getLegs());
            m_detailColor = outfitColor(outfit->getFeet());
            m_firstAddon = (outfit->getAddons() & 1) != 0;
            m_secondAddon = (outfit->getAddons() & 2) != 0;
        }
        m_isOwnCharacter = argBool(args, 4);
        m_title = argString(args, 5);
    }
};

/// CIP: TAccountInfoQmlData ; badges list at +0x170 via .
class TAccountInfoQmlData final : public cip::tibia::gamewindow::TAccountInfoQmlData
{
public:
    TAccountInfoQmlData(const std::vector<std::any>& args, QObject* parent)
        : cip::tibia::gamewindow::TAccountInfoQmlData(parent)
    {
        using client::gamecore::argInt;
        using Wire = std::vector<std::tuple<uint32_t, std::string>>;
        m_canShowAccountInfos = argInt(args, 0) != 0;
        m_isOnline = argInt(args, 1) != 0;
        m_isPremium = argInt(args, 2) != 0;
        m_loyaltyTitle = client::gamecore::argString(args, 3);
        std::vector<AccountBadgeEntry> badges;
        if (const auto* wire = args.size() > 4 ? std::any_cast<Wire>(&args[4]) : nullptr) {
            badges.reserve(wire->size());
            for (const auto& [id, name] : *wire)
                badges.push_back({ id, QString::fromStdString(name) });
        }
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

/// CIP : the hireling job names by job id.
QString hirelingJobName(const int job)
{
    switch (job) {
        case 0: return qtTrId("charinfo_hireling_job_banker");
        case 1: return qtTrId("charinfo_hireling_job_cook");
        case 2: return qtTrId("charinfo_hireling_job_steward");
        case 3: return qtTrId("charinfo_hireling_job_trader");
        default: return {};
    }
}

/// CIP: TStorePurchasesQmlData over the 0xDA store summary
/// (CIP reader case 8).
class TStorePurchasesQmlData final : public cip::tibia::gamewindow::TStorePurchasesQmlData
{
public:
    TStorePurchasesQmlData(const std::vector<std::any>& args, QObject* parent)
        : cip::tibia::gamewindow::TStorePurchasesQmlData(parent)
    {
        using client::gamecore::argBool;
        using client::gamecore::argInt;
        m_remainingStoreXpBoost = formatXpBoostTime(static_cast<uint32_t>(argInt(args, 0)));
        m_remainingDailyRewardXpBoost = formatXpBoostTime(static_cast<uint32_t>(argInt(args, 1)));
        QList<QVariantList> blessings;
        if (const auto* list = arg<std::vector<std::tuple<std::string, uint8_t>>>(args, 2))
            for (const auto& [name, count] : *list)
                blessings.append({ qstr(name), static_cast<int>(count) });
        m_storeBlessings = new TStoreSummaryListModel({ "name", "count" }, std::move(blessings), this);
        m_preySlots = static_cast<int>(argInt(args, 3));
        m_preyWildcards = static_cast<int>(argInt(args, 4));
        m_purchasedPermanentWeeklyTaskExpansion = argBool(args, 5);
        m_collectionTokens = static_cast<int>(argInt(args, 6));
        m_hasCharmExpansion = argBool(args, 7);
        m_hirelings = static_cast<int>(argInt(args, 8));
        if (const auto* jobs = arg<std::vector<uint16_t>>(args, 9))
            for (const uint16_t job : *jobs)
                m_hirelingJobs.append(hirelingJobName(job));
        QList<QVariantList> hirelingOutfits;
        if (const auto* list = arg<std::vector<std::tuple<std::string, uint8_t>>>(args, 11))
            for (const auto& [name, count] : *list)
                hirelingOutfits.append({ qstr(name), static_cast<int>(count) });
        m_hirelingOutfits = new TStoreSummaryListModel({ "name", "count" }, std::move(hirelingOutfits), this);
        QList<QVariantList> houseItems;
        if (const auto* list = arg<std::vector<std::tuple<uint16_t, std::string, uint8_t>>>(args, 10))
            for (const auto& [id, name, count] : *list)
                houseItems.append({ static_cast<int>(id), qstr(name), static_cast<int>(count) });
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

/// The offence half of the older combined combat-stats record.
cyclopedia::CombatStats combinedOffence(const CyclopediaCharacterCombatStats& data)
{
    cyclopedia::CombatStats offence;
    offence.attackDamageType = data.weaponElement;
    offence.convertedDamage = data.weaponElementDamage / 100.0;
    offence.convertedDamageType = data.weaponElementType;
    offence.attackValue = data.weaponMaxHitChance;
    return offence;
}

/// The defence half of the older combined combat-stats record (mitigation
/// as its second argument).
cyclopedia::CombatStats combinedDefence(const CyclopediaCharacterCombatStats& data, const std::vector<std::any>& args)
{
    cyclopedia::CombatStats defence;
    defence.defence = data.defense;
    defence.armor = data.armor;
    if (const auto* mitigation = args.size() > 1 ? std::any_cast<double>(&args[1]) : nullptr)
        defence.mitigation = *mitigation;
    else
        defence.mitigation = static_cast<double>(client::gamecore::argInt(args, 1));
    if (const game::TPlayerSkillStats* skillStats = game::TPlayerSkillStats::liveInstance())
        defence.mantra = skillStats->combatStats().mantra;
    return defence;
}


}

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
    new TQmlTargetWorld({}, {}, false, false, this);
    new TQmlWorldTransferCondition(false, {}, this);
    new TSpritesheetDataObject({}, this);
    new TSimpleNameAndCountModel(this);
    new TDialogControllerWithFocusHandlingBase(this);
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

bool TCharacterInfoDialogController::dataIsLoading() const { return !m_data.contains(m_dataType); }

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
    m_data.clear();
    tibia::game::requestSendCharacterInfo(m_characterId, tibia::client::CYCLOPEDIA_CHARACTERINFO_BASEINFORMATION);
    request(dataType, true);
}

void TCharacterInfoDialogController::hideDialogTab()
{
    m_shown = false;
    m_data.clear();
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
    if (m_data.contains(dataType)) {
        onCharacterInfoUpdated(dataType);
    } else {
        if (dataType == RecentDeaths || dataType == RecentKills)
            m_page[dataType - RecentDeaths] = 1;
        sendRequest(dataType, 1);
        // The general stats show the account infos of the badges answer.
        if (dataType == GeneralStats) {
            m_data.erase(Badges);
            tibia::game::requestSendCharacterInfo(m_characterId, tibia::client::CYCLOPEDIA_CHARACTERINFO_BADGES);
        }
    }
    Q_EMIT dialogCategoryChanged();
    Q_EMIT dataIsLoadingChanged();
}

void TCharacterInfoDialogController::sendRequest(const int dataType, const int page)
{
    // 23 entries per page.
    tibia::game::requestSendCharacterInfo(m_characterId, protocolType(dataType), 23, static_cast<uint16_t>(page));
}

void TCharacterInfoDialogController::store(const int dataType, const Args& args)
{
    if (!m_shown)
        return;
    m_data[dataType] = args;
    onCharacterInfoUpdated(dataType);
}

void TCharacterInfoDialogController::onCharacterInfoUpdated(const int dataType)
{
    Q_EMIT dataIsLoadingChanged();
    const auto it = m_data.find(dataType);
    if (it == m_data.end())
        return;
    const Args args = it->second;
    if (dataType == BaseInformation) {
        updateBaseInfo(args);
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
        case OffenceStats: updateOffenceStats(args); break;
        case DefenceStats: updateDefenceStats(args); break;
        case MiscStats: updateMiscStats(args); break;
        case RecentDeaths: updateDeaths(args); break;
        case RecentKills: updateKills(args); break;
        case Achievements: updateAchievements(args); break;
        case ItemSummary: updateItemSummary(args); break;
        case OutfitsMounts: updateOutfitsMounts(args); break;
        case StoreSummary: updateStorePurchases(args); break;
        case Inspection: updateInspection(args); break;
        case Titles: updateTitles(args); break;
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

void TCharacterInfoDialogController::onErrorReceived(const int errorCode)
{
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
    const int index = m_dataType - RecentDeaths;
    if (m_page[index] > 1) {
        --m_page[index];
        sendRequest(m_dataType, m_page[index]);
    }
}

void TCharacterInfoDialogController::nextPaginatedPage()
{
    if (m_dataType != RecentDeaths && m_dataType != RecentKills)
        return;
    const int index = m_dataType - RecentDeaths;
    if (m_page[index] < m_pages[index]) {
        ++m_page[index];
        sendRequest(m_dataType, m_page[index]);
    }
}

void TCharacterInfoDialogController::openSkillWheelDialog()
{
    // The shown character's wheel; the own one when no character is named.
    uint32_t characterId = m_characterId;
    if (characterId == 0)
        characterId = tibia::game::hasLocalPlayer() ? tibia::game::localPlayerId() : 0;
    input::publish(std::make_shared<input::TGameActionCreature>(A::REQUEST_SKILL_WHEEL, characterId));
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
void TCharacterInfoDialogController::updateBaseInfo(const Args& args)
{
    replace(m_characterBaseInfo, static_cast<QObject*>(new TCharacterBaseQmlInfo(args, this)));
    Q_EMIT dataModelChanged();
}

/// CIP: the offence stats object from the received record.
void TCharacterInfoDialogController::updateOffenceStats(const Args& args)
{
    if (const auto* data = arg<CyclopediaCharacterOffenceStats>(args, 0))
        replace(m_offenceStats, static_cast<QObject*>(new cyclopedia::TOffenceStatsQmlData(std::make_shared<cyclopedia::TPlayerOffenceStatsProvider>(*data), this)));
    else if (const auto* combined = arg<CyclopediaCharacterCombatStats>(args, 0))
        replace(m_offenceStats, static_cast<QObject*>(new cyclopedia::TOffenceStatsQmlData(combinedOffence(*combined), this)));
    Q_EMIT dataModelChanged();
}

/// CIP: the defence stats object from the received record.
void TCharacterInfoDialogController::updateDefenceStats(const Args& args)
{
    if (const auto* data = arg<CyclopediaCharacterDefenceStats>(args, 0))
        replace(m_defenceStats, static_cast<QObject*>(new cyclopedia::TDefenceStatsQmlData(std::make_shared<cyclopedia::TPlayerDefenceStatsProvider>(*data), this)));
    else if (const auto* combined = arg<CyclopediaCharacterCombatStats>(args, 0))
        replace(m_defenceStats, static_cast<QObject*>(new cyclopedia::TDefenceStatsQmlData(combinedDefence(*combined, args), this)));
    Q_EMIT dataModelChanged();
}

/// CIP: the misc stats object from the record and the live skill stats.
void TCharacterInfoDialogController::updateMiscStats(const Args& args)
{
    if (const auto* data = arg<CyclopediaCharacterMiscStats>(args, 0))
        replace(m_miscStats, static_cast<QObject*>(new cyclopedia::TMiscStatsQmlData(std::make_shared<cyclopedia::TPlayerMiscStatsProvider>(*data), this)));
    Q_EMIT dataModelChanged();
}

/// CIP: one death object per entry.
void TCharacterInfoDialogController::updateDeaths(const Args& args)
{
    QList<QObject*> deaths;
    if (const auto* data = arg<CyclopediaCharacterRecentDeaths>(args, 0)) {
        for (const RecentDeathEntry& entry : data->entries)
            deaths.append(new TDeathQmlData(entry, this));
    }
    replace(m_characterDeaths, std::move(deaths));
    m_currentPage = m_page[0];
    m_numberOfPages = m_pages[0];
    Q_EMIT dataModelChanged();
}

/// CIP: one kill object per entry.
void TCharacterInfoDialogController::updateKills(const Args& args)
{
    QList<QObject*> kills;
    if (const auto* data = arg<CyclopediaCharacterRecentPvPKills>(args, 0)) {
        for (const RecentPvPKillEntry& entry : data->entries)
            kills.append(new TKillQmlData(entry, this));
    }
    replace(m_characterKills, std::move(kills));
    m_currentPage = m_page[1];
    m_numberOfPages = m_pages[1];
    Q_EMIT dataModelChanged();
}

/// CIP: the store purchases object, built from the summary as it is.
void TCharacterInfoDialogController::updateStorePurchases(const Args& args)
{
    replace(m_storePurchases, static_cast<QObject*>(new TStorePurchasesQmlData(args, this)));
    Q_EMIT dataModelChanged();
}

/// The general stats page's account infos, from the badges record.
void TCharacterInfoDialogController::updateAccountInfos()
{
    if (const auto it = m_data.find(Badges); it != m_data.end())
        replace(m_accountInfos, static_cast<QObject*>(new TAccountInfoQmlData(it->second, this)));
    Q_EMIT dataModelChanged();
}

/// Slots 1..10 then the player (99), filled from the entries.
void TCharacterInfoDialogController::updateInspection(const Args& args)
{
    const auto* data = arg<CyclopediaCharacterInspection>(args, 0);
    if (!data)
        return;
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
    using client::gamecore::argInt;
    hookModelEvents();
    const auto storeAs = [this](const int dataType) {
        return [this, dataType](GameObject*, const Args& args) {
            store(dataType, args);
            return std::any{};
        };
    };

    m_hooks.emplace_back(tibia::game::kNotifyScope, "onInspectionState", [this](GameObject*, const Args& args) {
        m_inspectionState = client::gamecore::argInt(args, 1);
        Q_EMIT inspectionStateChanged();
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onCyclopediaCharacterBaseInformation", storeAs(BaseInformation));
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onParseCyclopediaCharacterGeneralStats", storeAs(GeneralStats));
    // The badges answer only feeds the general stats page's account infos.
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onParseCyclopediaCharacterBadges", [this](GameObject*, const Args& args) {
        if (!m_shown)
            return std::any{};
        m_data[Badges] = args;
        if (m_dataType == GeneralStats)
            updateAccountInfos();
        return std::any{};
    });
    // The older combined combat-stats record fills both the offence and the
    // defence page.
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onParseCyclopediaCharacterCombatStats", [this](GameObject*, const Args& args) {
        store(OffenceStats, args);
        store(DefenceStats, args);
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onCyclopediaCharacterOffenceStats", storeAs(OffenceStats));
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onCyclopediaCharacterDefenceStats", storeAs(DefenceStats));
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onCyclopediaCharacterMiscStats", storeAs(MiscStats));
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onCyclopediaCharacterPagination", [this](GameObject*, const Args& args) {
        const int type = static_cast<int>(argInt(args, 0));
        const int index = type == tibia::client::CYCLOPEDIA_CHARACTERINFO_RECENTDEATHS ? 0 : type == tibia::client::CYCLOPEDIA_CHARACTERINFO_RECENTPVPKILLS ? 1 : -1;
        if (index < 0)
            return std::any{};
        m_page[index] = std::max<int>(1, static_cast<int>(argInt(args, 1)));
        m_pages[index] = std::max<int>(1, static_cast<int>(argInt(args, 2)));
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onCyclopediaCharacterRecentDeaths", storeAs(RecentDeaths));
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onCyclopediaCharacterRecentKills", storeAs(RecentKills));
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onParseCyclopediaStoreSummary", storeAs(StoreSummary));
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onParseCyclopediaCharacterInspection", storeAs(Inspection));
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onCyclopediaCharacterInfoError", [this](GameObject*, const Args& args) {
        if (m_shown && static_cast<int>(argInt(args, 0)) == protocolType(m_dataType))
            onErrorReceived(static_cast<int>(argInt(args, 1)));
        return std::any{};
    });
}

}
