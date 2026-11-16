#include "managelootcontainerdialogcontroller.h"

#include "cyclopedia/iteminfostorage.h"
#include "gamewindow/marketcontroller.h"
#include "gamewindow/messagedialogcontroller.h"
#include "gamewindow/mousecursorcontroller.h"
#include "input/gameactionhandlers.h"
#include "quickloot/managedcontainerstorage.h"
#include "quickloot/quicklootciptypes.h"


#include <tibia/assets/thingtype.h>
#include <tibia/assets/things.h>

#include <QDebug>
#include <QPointer>

#include <algorithm>
#include <set>
#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/tgamenotify.h>
#include <tibia/game/thingsession_forward.h>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;

/// CIP category order from ctor .
constexpr int CategoryOrder[] = {
    31, 30, 1, 2, 3, 4, 24, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 27, 20, 21, 25,
};

/// CIP / empty sentinel.
constexpr uint16_t EmptyAppearance = 0x5ca9;


/// A managed container action for one object category; select actions also
/// carry the picked container.
void publishContainerAction(const A type, const int category, const input::TGameAction* target = nullptr)
{
    auto action = input::mutableClone(*input::makeAction(type));
    action->category = category;
    if (target) {
        action->position = target->position;
        action->stackPosition = target->stackPosition;
        action->objectClientId = target->objectClientId;
    }
    input::publish(action);
}

bool isSet(const uint16_t typeId) { return typeId != 0 && typeId != EmptyAppearance; }

QList<TMessageDialogButton*> yesNoButtons()
{
    return { new TMessageDialogButton(qtTrId("yes"), TMessageDialogController::Button::YES, QKeySequence(QStringLiteral("Return"))),
             new TMessageDialogButton(qtTrId("no"), TMessageDialogController::Button::NO, QKeySequence(QStringLiteral("Escape"))) };
}

ThingType* typeOf(const uint16_t typeId)
{
    const ThingTypePtr& type = tibia::game::thingType(typeId, ThingCategoryItem);
    return type && type->getId() != 0 ? type.get() : nullptr;
}

QString typeName(const uint16_t typeId)
{
    ThingType* type = typeOf(typeId);
    if (!type)
        return QStringLiteral("Unknown");
    const MarketData& market = type->getMarketData();
    const std::string& name = type->isMarketable() && !market.name.empty() ? market.name : type->getName();
    return name.empty() ? QStringLiteral("Unknown") : QString::fromStdString(name);
}

}

TLootContainerQMLInfo::TLootContainerQMLInfo(const int category, QObject* parent)
    : cip::tibia::gamewindow::TLootContainerQMLInfo(parent)
    , m_category(category)
{
    m_categoryName = TQmlObjectCategory::translate(category);
    // CIP : every category but Unassigned is premium-locked.
    m_isLockedByPremium = category != TManageLootContainerDialogController::UnassignedCategory;
}

void TLootContainerQMLInfo::setLootContainer(const uint16_t typeId)
{
    const uint16_t id = isSet(typeId) ? typeId : 0;
    const bool set = isSet(id);
    if (m_lootContainerAppearanceTypeID != id) {
        m_lootContainerAppearanceTypeID = id;
        Q_EMIT lootContainerAppearanceTypeIDChanged();
    }
    if (m_isLootContainerSet != set) {
        m_isLootContainerSet = set;
        Q_EMIT isLootContainerSetChanged();
    }
}

void TLootContainerQMLInfo::setObtainContainer(const uint16_t typeId)
{
    const uint16_t id = isSet(typeId) ? typeId : 0;
    const bool set = isSet(id);
    if (m_obtainContainerAppearanceTypeID != id) {
        m_obtainContainerAppearanceTypeID = id;
        Q_EMIT obtainContainerAppearanceTypeIDChanged();
    }
    if (m_isObtainContainerSet != set) {
        m_isObtainContainerSet = set;
        Q_EMIT isObtainContainerSetChanged();
    }
}

void TLootContainerQMLInfo::onSelectLootContainerClicked()
{
    Q_EMIT requestSelectLootContainer(m_category);
}

void TLootContainerQMLInfo::onClearLootContainerClicked()
{
    Q_EMIT requestClearLootContainer(m_category);
}

void TLootContainerQMLInfo::onOpenLootContainerClicked() { Q_EMIT requestOpenLootContainer(m_category); }

void TLootContainerQMLInfo::onSelectObtainContainerClicked() { Q_EMIT requestSelectObtainContainer(m_category); }

void TLootContainerQMLInfo::onClearObtainContainerClicked() { Q_EMIT requestClearObtainContainer(m_category); }

void TLootContainerQMLInfo::onOpenObtainContainerClicked() { Q_EMIT requestOpenObtainContainer(m_category); }

TManageLootContainerDialogController::TManageLootContainerDialogController(QObject* parent)
    : TDialogController(QStringLiteral("ManageLootContainerDialog.qml"), parent)
    , m_itemInfo(cyclopedia::TItemInfoStorage::instance())
    , m_containers(quickloot::TManagedContainerStorage::instance())
    , m_listModel([](uint16_t, uint8_t) { return 1; }, this)
    , m_listProxy(&m_listModel, this)
{
    m_listItems = &m_listProxy;
    m_confirm = new TMessageDialogController(this);
    m_confirm->setCloseOnButtonClicked(true);
    connect(m_confirm, &cip::tibia::gamewindow::TMessageDialogController::acceptClicked, this,
            [this](bool) { onClearConfirmed(); });

    for (const int category : CategoryOrder) {
        auto* row = new TLootContainerQMLInfo(category, this);
        m_lootContainers.append(row);
        connect(row, &TLootContainerQMLInfo::requestSelectLootContainer, this, [this](const int c) { startContainerPick(c, true); });
        connect(row, &TLootContainerQMLInfo::requestClearLootContainer, this,
                [](const int c) { publishContainerAction(A::LOOT_CONTAINER_CLEAR, c); });
        connect(row, &TLootContainerQMLInfo::requestOpenLootContainer, this,
                [](const int c) { publishContainerAction(A::LOOT_CONTAINER_OPEN, c); });
        connect(row, &TLootContainerQMLInfo::requestSelectObtainContainer, this, [this](const int c) { startContainerPick(c, false); });
        connect(row, &TLootContainerQMLInfo::requestClearObtainContainer, this,
                [](const int c) { publishContainerAction(A::OBTAIN_CONTAINER_CLEAR, c); });
        connect(row, &TLootContainerQMLInfo::requestOpenObtainContainer, this,
                [](const int c) { publishContainerAction(A::OBTAIN_CONTAINER_OPEN, c); });
    }

    if (m_itemInfo) {
        m_blackWhiteListType = m_itemInfo->lootListType();
        connect(m_itemInfo, &cyclopedia::TItemInfoStorage::lootListChanged, this, [this] {
            if (!m_itemInfo)
                return;
            if (m_blackWhiteListType != m_itemInfo->lootListType()) {
                m_blackWhiteListType = m_itemInfo->lootListType();
                Q_EMIT lootBlackWhitelistChanged();
            }
            rebuildListItems();
        });
        connect(m_itemInfo, &cyclopedia::TItemInfoStorage::premiumChanged, this, &TManageLootContainerDialogController::onPremiumTimeChanged);
        connect(m_itemInfo, &cyclopedia::TItemInfoStorage::characterChanged, this, [this] {
            if (!m_itemInfo)
                return;
            m_blackWhiteListType = m_itemInfo->lootListType();
            Q_EMIT lootBlackWhitelistChanged();
            rebuildListItems();
            syncPremium();
        });
    }
    if (m_containers) {
        connect(m_containers, &quickloot::TManagedContainerStorage::managedContainersUpdated, this,
                &TManageLootContainerDialogController::onLootContainersUpdated);
        connect(m_containers, &quickloot::TManagedContainerStorage::useMainBackpackAsFallbackChanged, this,
                &TManageLootContainerDialogController::syncFallback);
        syncFallback();
    }

    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameStarted, this, [this]() {
        syncPremium();
        rebuildListItems();
    });
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameEnded, this, [this]() {
        cancelPick();
        dialog()->close();
    });

    rebuildListItems();
    syncPremium();
    onLootContainersUpdated();
    installHandlers();
}

TManageLootContainerDialogController::~TManageLootContainerDialogController()
{
    uninstallHandlers();
}

void TManageLootContainerDialogController::installHandlers()
{
    const auto handle = [this](const input::TGameAction& action) { return handleGameAction(action); };
    for (const A type : { A::OPEN_MANAGE_LOOT_CONTAINER, A::OPEN_MANAGE_LOOT_CONTAINER_STASH })
        input::setActionHandler(type, handle);
}

void TManageLootContainerDialogController::uninstallHandlers()
{
    for (const A type : { A::OPEN_MANAGE_LOOT_CONTAINER, A::OPEN_MANAGE_LOOT_CONTAINER_STASH })
        input::setActionHandler(type, nullptr);
}

bool TManageLootContainerDialogController::handleGameAction(const input::TGameAction& action)
{
    switch (action.type) {
    case A::OPEN_MANAGE_LOOT_CONTAINER:
        m_scrollOnShow = false;
        showDialog();
        return true;
    case A::OPEN_MANAGE_LOOT_CONTAINER_STASH:
        // CIP case 0xfc: show then scrollDown.
        m_scrollOnShow = true;
        showDialog();
        return true;
    default:
        return false;
    }
}

void TManageLootContainerDialogController::setUseMainBackpackAsFallback(const bool v)
{
    // The checkbox only asks the server; the stored flag follows its answer.
    if (m_containers ? m_containers->useMainBackpackAsFallback() == v : !v)
        return;
    auto action = input::mutableClone(*input::makeAction(A::LOOT_CONTAINER_FALLBACK));
    action->enable = v;
    input::publish(action);
}

void TManageLootContainerDialogController::setFilterString(const QString& v)
{
    if (::cip::unchanged(m_filterString, v))
        return;
    m_filterString = v;
    applyListFilter();
    Q_EMIT filterStringChanged();
}

void TManageLootContainerDialogController::showDialog()
{
    syncPremium();
    syncFallback();
    if (m_itemInfo) {
        const int listType = m_itemInfo->lootListType();
        if (m_blackWhiteListType != listType) {
            m_blackWhiteListType = listType;
            Q_EMIT lootBlackWhitelistChanged();
        }
    }
    rebuildListItems();
    onLootContainersUpdated();
    const bool stashScroll = m_scrollOnShow;
    qInfo().nospace() << "tibia.loot: show ManageLootContainerDialog stashScroll=" << stashScroll
                      << " (action " << (stashScroll ? 252 : 251) << ')';
    dialog()->show();
    if (m_scrollOnShow) {
        m_scrollOnShow = false;
        Q_EMIT scrollDown();
    }
}

void TManageLootContainerDialogController::closeDialog()
{
    cancelPick();
    dialog()->close();
}

void TManageLootContainerDialogController::closeButtonClicked() { closeDialog(); }

void TManageLootContainerDialogController::getPremiumClicked()
{
    // The store, opened on its premium page.
    auto action = input::mutableClone(*input::makeAction(A::OPEN_STORE));
    action->parameter = 1;
    input::publish(action);
}

void TManageLootContainerDialogController::requestClearItemBlackWhitelist()
{
    if (!m_itemInfo)
        return;
    const bool white = m_itemInfo->lootListType() == cyclopedia::TItemInfoStorage::Whitelist;
    m_confirm->setButtons(yesNoButtons());
    m_confirm->showMessage(qtTrId(white ? "managecontainer_confirm_clear_whitelist_caption" : "managecontainer_confirm_clear_blacklist_caption"),
                           qtTrId(white ? "managecontainer_confirm_clear_whitelist_text" : "managecontainer_confirm_clear_blacklist_text"));
}

void TManageLootContainerDialogController::onClearConfirmed()
{
    if (!m_itemInfo)
        return;
    std::set<uint16_t> black = m_itemInfo->lootList(cyclopedia::TItemInfoStorage::Blacklist);
    std::set<uint16_t> white = m_itemInfo->lootList(cyclopedia::TItemInfoStorage::Whitelist);
    if (m_itemInfo->lootListType() == cyclopedia::TItemInfoStorage::Whitelist)
        white.clear();
    else
        black.clear();
    m_itemInfo->replaceLootLists(m_itemInfo->lootListType(), std::move(black), std::move(white));
}

void TManageLootContainerDialogController::requestLootBlackWhitelistSwitch(const int NewListMode)
{
    if (!m_itemInfo)
        return;
    // CIP radio BindingGroup re-enters when syncing `checked` from controllerChecked;
    // skip no-op switches so Qt 6 does not report a controllerChecked binding loop.
    const int next = NewListMode == cyclopedia::TItemInfoStorage::Whitelist ? cyclopedia::TItemInfoStorage::Whitelist
                                                                            : cyclopedia::TItemInfoStorage::Blacklist;
    if (m_itemInfo->lootListType() == next && m_blackWhiteListType == next)
        return;
    m_itemInfo->setLootListType(next);
    m_blackWhiteListType = m_itemInfo->lootListType();
    Q_EMIT lootBlackWhitelistChanged();
    rebuildListItems();
}

void TManageLootContainerDialogController::requestAddItemBlackWhitelist()
{
    // Items are put on the list from the cyclopedia's item page.
    auto action = input::mutableClone(*input::makeAction(A::OPEN_CYCLOPEDIA));
    action->parameter = 0;
    input::publish(action);
}

void TManageLootContainerDialogController::removeItemFromList(const int AppearanceTypeIDRaw)
{
    if (m_itemInfo)
        m_itemInfo->setLootListed(static_cast<uint16_t>(std::clamp(AppearanceTypeIDRaw, 0, 0xffff)), false);
}

void TManageLootContainerDialogController::showItemDetails(const int AppearanceTypeIDRaw)
{
    auto action = input::makeAction(A::OPEN_CYCLOPEDIA);
    auto mutableAction = std::const_pointer_cast<input::TGameAction>(action);
    mutableAction->parameter = 0;
    mutableAction->instance = std::clamp(AppearanceTypeIDRaw, 0, 0xffff);
    input::publish(action);
}

void TManageLootContainerDialogController::onPremiumTimeChanged() { syncPremium(); }

void TManageLootContainerDialogController::onLootContainersUpdated()
{
    if (!m_containers)
        return;
    for (QObject* object : m_lootContainers) {
        auto* row = qobject_cast<TLootContainerQMLInfo*>(object);
        if (!row)
            continue;
        const auto assignment = m_containers->assignment(static_cast<int>(row->category()));
        row->setLootContainer(assignment.lootContainerId);
        row->setObtainContainer(assignment.obtainContainerId);
    }
}

void TManageLootContainerDialogController::rebuildListItems()
{
    if (!m_itemInfo)
        return;
    std::vector<TAppearanceTypeEntry> entries;
    for (const uint16_t typeId : m_itemInfo->lootList(m_itemInfo->lootListType())) {
        TAppearanceTypeEntry entry;
        entry.typeId = typeId;
        entry.name = typeName(typeId);
        if (ThingType* type = typeOf(typeId))
            entry.marketCategory = static_cast<int>(type->getMarketData().category);
        entries.push_back(std::move(entry));
    }
    std::ranges::sort(entries, [](const TAppearanceTypeEntry& a, const TAppearanceTypeEntry& b) { return a.name < b.name; });
    m_listModel.reset(std::move(entries));
    applyListFilter();
}

void TManageLootContainerDialogController::applyListFilter()
{
    // Appearance proxy's empty name+category rejects every row (Item Info "pick a
    // category" behaviour). Loot lists must show all entries until the player types.
    if (m_filterString.isEmpty()) {
        m_listProxy.setCategoryFilter(TAppearanceTypeListModelSortFilterProxy::AllCategories, false, false, false, false, {},
                                      false);
        return;
    }
    m_listProxy.setNameFilter(m_filterString, false, false, false, false, {}, false);
}

void TManageLootContainerDialogController::syncPremium()
{
    const bool premium = tibia::game::hasLocalPlayer() && tibia::game::localPlayerIsPremium();
    if (m_isPremium == premium)
        return;
    m_isPremium = premium;
    Q_EMIT premiumStatusChanged();
}

void TManageLootContainerDialogController::syncFallback()
{
    const bool fallback = m_containers && m_containers->useMainBackpackAsFallback();
    if (m_useMainBackpackAsFallback == fallback)
        return;
    m_useMainBackpackAsFallback = fallback;
    Q_EMIT useMainBackpackAsFallbackChanged();
}

void TManageLootContainerDialogController::startContainerPick(const int category, const bool lootColumn)
{
    qInfo().nospace() << "tibia.loot: startContainerPick category=" << category << " lootColumn=" << lootColumn;
    m_pick = lootColumn ? PickMode::LootContainer : PickMode::ObtainContainer;
    m_pickCategory = category;
    if (TMouseCursorController* cursor = TMouseCursorController::instance())
        cursor->setTargetSelection(true);
    dialog()->hide();
    QPointer<TManageLootContainerDialogController> self(this);
    input::startTargetSelection([self](const input::TGameActionPtr& target) {
        if (TMouseCursorController* cursor = TMouseCursorController::instance())
            cursor->setTargetSelection(false);
        if (self)
            self->finishPick(target);
    });
}

void TManageLootContainerDialogController::finishPick(const input::TGameActionPtr& target)
{
    const PickMode mode = m_pick;
    const int category = m_pickCategory;
    m_pick = PickMode::None;
    m_pickCategory = 0;
    if (mode == PickMode::None)
        return;
    if (target && target->objectClientId) {
        const uint16_t typeId = target->objectClientId;
        ThingType* type = tibia::game::rawThingType(typeId, ThingCategoryItem);
        if (type && type->isContainer()) {
            publishContainerAction(mode == PickMode::LootContainer ? A::LOOT_CONTAINER_SELECT : A::OBTAIN_CONTAINER_SELECT, category,
                                   target.get());
        } else {
            m_confirm->setButtons({ new TMessageDialogButton(qtTrId("ok"), TMessageDialogController::Button::OK,
                                                             QKeySequence(QStringLiteral("Return"))) });
            m_confirm->showMessage(qtTrId("managecontainer_invalid_container_caption"), qtTrId("managecontainer_invalid_container_text"));
        }
    }
    if (tibia::game::isOnline())
        dialog()->show();
}

void TManageLootContainerDialogController::cancelPick()
{
    if (m_pick == PickMode::None)
        return;
    m_pick = PickMode::None;
    m_pickCategory = 0;
    if (input::isSelectingTarget())
        input::finishTargetSelection(nullptr);
    if (TMouseCursorController* cursor = TMouseCursorController::instance())
        cursor->setTargetSelection(false);
}

}
