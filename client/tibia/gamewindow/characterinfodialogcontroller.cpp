#include "characterinfodialogcontroller.h"
#include "colorhelper.h"

#include "skillwheel/skillwheelciptypes.h"

#include "cyclopedia/characterinfostorage.h"
#include "game/gamesessionbase.h"
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
#include <tibia/game/tgamenotify.h>
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

/// CIP: TDeathQmlData. It keeps the death's time and formats it when read.
class TDeathQmlData final : public cip::tibia::gamewindow::TDeathQmlData
{
public:
    TDeathQmlData(const RecentDeathEntry& entry, QObject* parent)
        : cip::tibia::gamewindow::TDeathQmlData(parent)
        , m_time(entry.timestamp)
    {
        m_description = qstr(entry.cause);
    }

    QString timestamp() const override { return formatDateTime(m_time); }

private:
    uint32_t m_time = 0;
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

    /// No object in the slot.
    bool isEmpty() const { return m_appearanceID == 0; }
    bool isDualWield() const
    {
        TAppearanceType* type = tibia::game::rawThingType(static_cast<uint16_t>(m_appearanceID), ThingCategoryItem);
        return type && type->isDualWield();
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
    connect(m_errorMessage, &TMessageDialogController::acceptClicked, this, [this](bool) { onErrorMessageOkClicked(); });
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

bool TCharacterInfoDialogController::dataIsLoading() const
{
    return !characterInfoStorage() || !characterInfoStorage()->hasInfo(m_dataType);
}

void TCharacterInfoDialogController::showDialogTab() { showDialogTab(m_dataType); }

void TCharacterInfoDialogController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    if (m_characterInfoStorage)
        disconnect(m_characterInfoStorage.get(), nullptr, this, nullptr);
    m_dataType = Inspection;
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(GameSession.get());
    m_characterInfoStorage = session ? session->characterInfoStorage() : nullptr;
    if (!m_characterInfoStorage)
        return;
    connect(m_characterInfoStorage.get(), &cyclopedia::TCharacterInfoStorage::infoUpdated, this, [this](const int dataType) {
        if (m_shown)
            onCharacterInfoUpdated(dataType);
    });
    connect(m_characterInfoStorage.get(), &cyclopedia::TCharacterInfoStorage::errorReceived, this,
            &TCharacterInfoDialogController::onErrorReceived);
}

void TCharacterInfoDialogController::showDialogTab(const int dataType)
{
    // The records are fetched anew each time the page opens, the base
    // information first.
    m_shown = true;
    if (cyclopedia::TCharacterInfoStorage* storage = characterInfoStorage())
        storage->clearAll();
    tibia::game::requestSendCharacterInfo(m_characterId, tibia::client::CYCLOPEDIA_CHARACTERINFO_BASEINFORMATION);
    request(dataType, true);
}

void TCharacterInfoDialogController::hideDialogTab()
{
    m_shown = false;
    if (cyclopedia::TCharacterInfoStorage* storage = characterInfoStorage())
        storage->clearAll();
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
    if (characterInfoStorage() && characterInfoStorage()->hasInfo(dataType)) {
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

TCyclopediaDialogContext TCharacterInfoDialogController::captureContext() const
{
    return TCharacterInfoCyclopediaContext{ m_dataType, characterInfoStorage() ? characterInfoStorage()->currentPage(m_dataType) : 1 };
}

void TCharacterInfoDialogController::restoreContext(const TCyclopediaDialogContext& CyclopediaContext)
{
    const auto& context = cyclopediaContextAs<TCharacterInfoCyclopediaContext>(CyclopediaContext);
    request(context.dataType, false);
    sendRequest(context.dataType, static_cast<int>(context.page));
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

/// Explains a failed request; a missing character or one the player may not
/// view also tells the dialog.
void TCharacterInfoDialogController::onErrorReceived(int /*dataType*/, const int errorCode)
{
    if (!m_shown)
        return;
    m_errorType = errorCode;
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
    if (errorCode == 2)
        Q_EMIT noPermissionsToViewCharacter();
    else if (errorCode == 1)
        Q_EMIT errorLoadingCharacter();
}

void TCharacterInfoDialogController::onErrorMessageOkClicked()
{
    // The inspection was refused: the owner of the page shows another one.
    if (m_errorType == 3)
        Q_EMIT noPermissionsToInspectCharacter();
}

int TCharacterInfoDialogController::currentPage() const
{
    return characterInfoStorage() ? static_cast<int>(characterInfoStorage()->currentPage(m_dataType)) : 1;
}

int TCharacterInfoDialogController::numberOfPages() const
{
    return characterInfoStorage() ? static_cast<int>(characterInfoStorage()->pageCount(m_dataType)) : 1;
}

void TCharacterInfoDialogController::openStoreWithXpBoostCategory()
{
    auto action = std::make_shared<input::TGameActionOpenStore>(1, 1);
    Q_EMIT publishGameAction(action);
    Q_EMIT publishGameAction(input::makeGameAction(A::CLOSE_CYCLOPEDIA));
}

void TCharacterInfoDialogController::openBlessingsDialog() { tibia::game::requestBless(); }

void TCharacterInfoDialogController::openItemInformation(const int TypeID)
{
    auto action = std::make_shared<input::TGameActionOpenCyclopedia>(0);
    action->setTypeId(static_cast<uint16_t>(TypeID));
    Q_EMIT publishGameAction(action);
}

void TCharacterInfoDialogController::previousPaginatedPage()
{
    if (!characterInfoStorage())
        return;
    const qint64 page = characterInfoStorage()->currentPage(m_dataType);
    if (page > 1)
        sendRequest(m_dataType, static_cast<int>(page - 1));
}

void TCharacterInfoDialogController::nextPaginatedPage()
{
    const cyclopedia::TCharacterInfoStorage* storage = characterInfoStorage();
    if (!storage)
        return;
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
    auto action = std::make_shared<input::gameactions::TGameActionOpenWeaponProficiencyDialog>(static_cast<uint16_t>(static_cast<int>(AppearanceTypeID)));
    Q_EMIT publishGameAction(action);
}

/// Opens the outfit dialog in its character info mode over every outfit,
/// mount and summon skin of the shown character: the clicked one is selected,
/// the other kinds start on the first of their list (an outfit with its
/// add-ons), coloured like the appearances page.
void TCharacterInfoDialogController::openOutfitDialog(const int OutfitID, const bool IsOutfit, const bool IsMount,
                                                      const bool IsSummonSkin)
{
    if (!tibia::game::isOnline())
        return;
    Outfit current;
    current.setCategory(ThingCategoryCreature);
    if (IsOutfit)
        current.setId(static_cast<uint16_t>(OutfitID));
    else if (IsMount)
        current.setMount(static_cast<uint16_t>(OutfitID));
    else if (IsSummonSkin)
        current.setFamiliar(static_cast<uint16_t>(OutfitID));
    else
        return;
    if (const auto* model = dynamic_cast<TOutfitMountSummonSkinsListDataModelSortFilterProxy*>(m_outfitMountModel)) {
        const auto& colors = model->colors();
        current.setHead(colors[0]);
        current.setBody(colors[1]);
        current.setLegs(colors[2]);
        current.setFeet(colors[3]);
        current.setMountColors(colors[4], colors[5], colors[6], colors[7]);
    }

    using Entry = TCustomiseCharacterModel::Entry;
    QList<Entry> outfits;
    QList<Entry> mounts;
    QList<Entry> summons;
    if (const auto appearances = characterInfoStorage() ? characterInfoStorage()->outfitsMountsSummonSkins() : nullptr) {
        for (const auto& outfit : appearances->outfits) {
            if (IsOutfit && outfit.lookType == OutfitID)
                current.setAddons(static_cast<uint8_t>(outfit.addons));
            outfits.append({ .id = outfit.lookType, .name = outfit.name, .firstAddOn = (outfit.addons & 1) != 0,
                             .secondAddOn = (outfit.addons & 2) != 0, .type = static_cast<int>(outfit.source) });
        }
        for (const auto& mount : appearances->mounts)
            mounts.append({ .id = mount.lookType, .name = mount.name, .type = static_cast<int>(mount.source) });
        for (const auto& summon : appearances->summonSkins)
            summons.append({ .id = summon.lookType, .name = summon.name, .type = static_cast<int>(summon.source) });
    }
    if (!IsOutfit && !outfits.isEmpty()) {
        current.setId(static_cast<uint16_t>(outfits.first().id));
        current.setAddons(static_cast<uint8_t>((outfits.first().firstAddOn ? 1 : 0) | (outfits.first().secondAddOn ? 2 : 0)));
    }
    if (!IsMount && !mounts.isEmpty())
        current.setMount(static_cast<uint16_t>(mounts.first().id));
    if (!IsSummonSkin && !summons.isEmpty())
        current.setFamiliar(static_cast<uint16_t>(summons.first().id));

    if (!m_outfitDialog)
        m_outfitDialog = new TSelectOutfitDialogController(this);
        connect(m_outfitDialog, &TSelectOutfitDialogController::publishGameAction, this, &TCharacterInfoDialogController::publishGameAction);
    m_outfitDialog->openCharacterInfoSelection(current, std::move(outfits), std::move(mounts), std::move(summons), IsOutfit, IsMount,
                                               IsSummonSkin);
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
    if (!characterInfoStorage())
        return;
    const cyclopedia::TCharacterBaseInformation information = characterInfoStorage()->baseInformation();
    replace(m_characterBaseInfo, static_cast<QObject*>(new TCharacterBaseQmlInfo(information, this)));
    Q_EMIT dataModelChanged();
}

/// CIP: the offence stats object over the storage's provider.
void TCharacterInfoDialogController::updateOffenceStats()
{
    if (auto provider = characterInfoStorage() ? characterInfoStorage()->offenceStats() : nullptr)
        replace(m_offenceStats, static_cast<QObject*>(new cyclopedia::TOffenceStatsQmlData(std::move(provider), this)));
    Q_EMIT dataModelChanged();
}

/// CIP: the defence stats object over the storage's provider.
void TCharacterInfoDialogController::updateDefenceStats()
{
    if (auto provider = characterInfoStorage() ? characterInfoStorage()->defenceStats() : nullptr)
        replace(m_defenceStats, static_cast<QObject*>(new cyclopedia::TDefenceStatsQmlData(std::move(provider), this)));
    Q_EMIT dataModelChanged();
}

/// CIP: the misc stats object over the storage's provider and the live skill stats.
void TCharacterInfoDialogController::updateMiscStats()
{
    if (auto provider = characterInfoStorage() ? characterInfoStorage()->miscStats() : nullptr)
        replace(m_miscStats, static_cast<QObject*>(new cyclopedia::TMiscStatsQmlData(std::move(provider), this)));
    Q_EMIT dataModelChanged();
}

/// CIP: one death object per entry.
void TCharacterInfoDialogController::updateDeaths()
{
    QList<QObject*> deaths;
    if (const cyclopedia::TCharacterInfoStorage* storage = characterInfoStorage())
        for (const RecentDeathEntry& entry : storage->recentDeaths())
            deaths.append(new TDeathQmlData(entry, this));
    replace(m_characterDeaths, std::move(deaths));
    Q_EMIT dataModelChanged();
}

/// CIP: one kill object per entry.
void TCharacterInfoDialogController::updateKills()
{
    QList<QObject*> kills;
    if (const cyclopedia::TCharacterInfoStorage* storage = characterInfoStorage())
        for (const RecentPvPKillEntry& entry : storage->recentPvpKills())
            kills.append(new TKillQmlData(entry, this));
    replace(m_characterKills, std::move(kills));
    Q_EMIT dataModelChanged();
}

/// CIP: the store purchases object, built from the summary as it is.
void TCharacterInfoDialogController::updateStorePurchases()
{
    if (const auto summary = characterInfoStorage() ? characterInfoStorage()->storeSummary() : nullptr)
        replace(m_storePurchases, static_cast<QObject*>(new TStorePurchasesQmlData(*summary, this)));
    Q_EMIT dataModelChanged();
}

/// The general stats page's account infos, from the badges record.
void TCharacterInfoDialogController::updateAccountInfos()
{
    if (const auto infos = characterInfoStorage() ? characterInfoStorage()->accountInfos() : nullptr)
        replace(m_accountInfos, static_cast<QObject*>(new TAccountInfoQmlData(*infos, this)));
    Q_EMIT dataModelChanged();
}

/// The ten equipment slots and the inspected character (slot 99), built only
/// when the storage holds an inspection.
void TCharacterInfoDialogController::updateInspection()
{
    const cyclopedia::TCharacterInfoStorage* storage = characterInfoStorage();
    if (!storage || !storage->hasInfo(Inspection))
        return;
    const CyclopediaCharacterInspection inspection = storage->inspection();
    QList<QObject*> objects;
    for (int slot = 1; slot <= InspectSlots; ++slot)
        objects.append(new TInspectObjectQmlData(slot, this));
    auto* player = new TInspectObjectQmlData(InspectPlayerSlot, this);
    objects.append(player);
    for (const InspectionInventoryItem& entry : inspection.inventoryItems) {
        if (entry.slot >= 1 && entry.slot <= InspectSlots)
            static_cast<TInspectObjectQmlData*>(objects[entry.slot - 1])->setItem(entry);
    }
    player->setPlayer(inspection);
    replace(m_inspectPlayerList, std::move(objects));
    Q_EMIT dataModelChanged();
}

/// A dual-wield weapon in the left hand with the right hand empty.
bool TCharacterInfoDialogController::hasDualWielding() const
{
    bool leftHandDualWield = false;
    bool rightHandEmpty = false;
    for (QObject* object : m_inspectPlayerList) {
        const auto* data = static_cast<const TInspectObjectQmlData*>(object);
        if (data->slotID() == InspectLeftHandSlot && !data->isEmpty() && data->isDualWield())
            leftHandDualWield = true;
        if (data->slotID() == InspectRightHandSlot && data->isEmpty())
            rightHandEmpty = true;
    }
    return leftHandDualWield && rightHandEmpty;
}

/// The left hand's dual-wield weapon, 0 without one.
int TCharacterInfoDialogController::dualWieldingObjectID() const
{
    for (QObject* object : m_inspectPlayerList) {
        const auto* data = static_cast<const TInspectObjectQmlData*>(object);
        if (data->slotID() == InspectLeftHandSlot && !data->isEmpty() && data->isDualWield())
            return static_cast<int>(data->appearanceID());
    }
    return 0;
}

void TCharacterInfoDialogController::hookEvents()
{
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onInspectionState", [this](GameObject*, const std::vector<std::any>& args) {
        m_inspectionState = client::gamecore::argInt(args, 1);
        Q_EMIT inspectionStateChanged();
        return std::any{};
    });
}

}
