#include "managelootcontainerdialogcontroller.h"

#include "cyclopedia/iteminfostorage.h"
#include "gamewindow/marketcontroller.h"
#include "gamewindow/messagedialogcontroller.h"
#include "gamewindow/mousecursorcontroller.h"
#include "input/gameactionhandlers.h"
#include "quickloot/managedcontainerstorage.h"

#include <tibia/world/container.h>
#include <tibia/world/game.h>
#include <client/item.h>
#include <tibia/world/localplayer.h>
#include <tibia/assets/thingtype.h>
#include <tibia/assets/things.h>

#include <QDebug>
#include <QPointer>

#include <algorithm>
#include <set>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;

/// CIP category order from ctor 0x100d27e70.
constexpr int CategoryOrder[] = {
    31, 30, 1, 2, 3, 4, 24, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 27, 20, 21, 25,
};

/// CIP 0x100d27d50 / 0x100d27d70 empty sentinel.
constexpr uint16_t EmptyAppearance = 0x5ca9;

constexpr uint8_t ActionSelectLoot = 0;
constexpr uint8_t ActionClearLoot = 1;
constexpr uint8_t ActionOpenLoot = 2;
constexpr uint8_t ActionSelectObtain = 4;
constexpr uint8_t ActionClearObtain = 5;
constexpr uint8_t ActionOpenObtain = 6;

bool isSet(const uint16_t typeId) { return typeId != 0 && typeId != EmptyAppearance; }

QList<TMessageDialogButton*> yesNoButtons()
{
    return { new TMessageDialogButton(qtTrId("yes"), TMessageDialogController::Button::YES, QKeySequence(QStringLiteral("Return"))),
             new TMessageDialogButton(qtTrId("no"), TMessageDialogController::Button::NO, QKeySequence(QStringLiteral("Escape"))) };
}

ThingType* typeOf(const uint16_t typeId)
{
    const ThingTypePtr& type = g_things.getThingType(typeId, ThingCategoryItem);
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
    // CIP 0x100d27d40: every category but Unassigned is premium-locked.
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
        connect(row, &TLootContainerQMLInfo::requestClearLootContainer, this, [this](const int c) {
            sendContainerAction(ActionClearLoot, static_cast<uint8_t>(c));
        });
        connect(row, &TLootContainerQMLInfo::requestOpenLootContainer, this, [this](const int c) {
            if (m_containers)
                openAssignedContainer(static_cast<uint8_t>(c), m_containers->assignment(static_cast<uint8_t>(c)).lootContainerId,
                                      ActionOpenLoot);
        });
        connect(row, &TLootContainerQMLInfo::requestSelectObtainContainer, this, [this](const int c) { startContainerPick(c, false); });
        connect(row, &TLootContainerQMLInfo::requestClearObtainContainer, this, [this](const int c) {
            sendContainerAction(ActionClearObtain, static_cast<uint8_t>(c));
        });
        connect(row, &TLootContainerQMLInfo::requestOpenObtainContainer, this, [this](const int c) {
            if (m_containers)
                openAssignedContainer(static_cast<uint8_t>(c), m_containers->assignment(static_cast<uint8_t>(c)).obtainContainerId,
                                      ActionOpenObtain);
        });
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

    m_hooks.emplace_back("g_game", "onGameStart", [this](LuaObject*, const std::vector<std::any>&) {
        syncPremium();
        rebuildListItems();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onGameEnd", [this](LuaObject*, const std::vector<std::any>&) {
        cancelPick();
        dialog()->close();
        return std::any{};
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
    for (const A type : { A::OPEN_MANGE_LOOT_CONTAINER, A::OPEN_MANGE_LOOT_CONTAINER_STASH, A::QUICK_LOOT, A::QUICK_LOOT_AREA_AT_PLAYER,
                          A::LOOT_ADD_TO_BLACKWHITELIST, A::LOOT_REMOVE_FROM_BLACKWHITELIST, A::LOOT_CONTAINER_FALLBACK,
                          A::LOOT_CONTAINER_SELECT, A::LOOT_CONTAINER_CLEAR, A::LOOT_CONTAINER_OPEN, A::OBTAIN_CONTAINER_SELECT,
                          A::OBTAIN_CONTAINER_CLEAR, A::OBTAIN_CONTAINER_OPEN })
        input::setActionHandler(type, handle);
}

void TManageLootContainerDialogController::uninstallHandlers()
{
    for (const A type : { A::OPEN_MANGE_LOOT_CONTAINER, A::OPEN_MANGE_LOOT_CONTAINER_STASH, A::QUICK_LOOT, A::QUICK_LOOT_AREA_AT_PLAYER,
                          A::LOOT_ADD_TO_BLACKWHITELIST, A::LOOT_REMOVE_FROM_BLACKWHITELIST, A::LOOT_CONTAINER_FALLBACK,
                          A::LOOT_CONTAINER_SELECT, A::LOOT_CONTAINER_CLEAR, A::LOOT_CONTAINER_OPEN, A::OBTAIN_CONTAINER_SELECT,
                          A::OBTAIN_CONTAINER_CLEAR, A::OBTAIN_CONTAINER_OPEN })
        input::setActionHandler(type, nullptr);
}

bool TManageLootContainerDialogController::handleGameAction(const input::TGameAction& action)
{
    switch (action.type) {
    case A::OPEN_MANGE_LOOT_CONTAINER:
        m_scrollOnShow = false;
        showDialog();
        return true;
    case A::OPEN_MANGE_LOOT_CONTAINER_STASH:
        // CIP 0x100d2bc10 case 0xfc: show then scrollDown.
        m_scrollOnShow = true;
        showDialog();
        return true;
    case A::QUICK_LOOT:
        if (action.object && action.object->isItem()) {
            const ItemPtr item = action.object->static_self_cast<Item>();
            g_game.sendQuickLoot(0, item);
        }
        return true;
    case A::QUICK_LOOT_AREA_AT_PLAYER:
        g_game.sendQuickLoot(2, nullptr);
        return true;
    case A::LOOT_ADD_TO_BLACKWHITELIST:
        if (m_itemInfo && action.object && action.object->isItem())
            m_itemInfo->setLootListed(action.object->getId(), true);
        return true;
    case A::LOOT_REMOVE_FROM_BLACKWHITELIST:
        if (m_itemInfo && action.object && action.object->isItem())
            m_itemInfo->setLootListed(action.object->getId(), false);
        return true;
    case A::LOOT_CONTAINER_FALLBACK:
        if (m_containers)
            m_containers->setUseMainBackpackAsFallback(action.enable);
        return true;
    case A::LOOT_CONTAINER_SELECT:
        startContainerPick(action.parameter >= 0 ? action.parameter : action.category, true);
        return true;
    case A::OBTAIN_CONTAINER_SELECT:
        startContainerPick(action.parameter >= 0 ? action.parameter : action.category, false);
        return true;
    case A::LOOT_CONTAINER_CLEAR:
        sendContainerAction(ActionClearLoot, static_cast<uint8_t>(action.parameter >= 0 ? action.parameter : action.category));
        return true;
    case A::OBTAIN_CONTAINER_CLEAR:
        sendContainerAction(ActionClearObtain, static_cast<uint8_t>(action.parameter >= 0 ? action.parameter : action.category));
        return true;
    case A::LOOT_CONTAINER_OPEN:
        if (m_containers)
            openAssignedContainer(static_cast<uint8_t>(action.parameter >= 0 ? action.parameter : action.category),
                                  m_containers->assignment(static_cast<uint8_t>(action.parameter >= 0 ? action.parameter : action.category))
                                      .lootContainerId,
                                  ActionOpenLoot);
        return true;
    case A::OBTAIN_CONTAINER_OPEN:
        if (m_containers)
            openAssignedContainer(static_cast<uint8_t>(action.parameter >= 0 ? action.parameter : action.category),
                                  m_containers->assignment(static_cast<uint8_t>(action.parameter >= 0 ? action.parameter : action.category))
                                      .obtainContainerId,
                                  ActionOpenObtain);
        return true;
    default:
        return false;
    }
}

void TManageLootContainerDialogController::setUseMainBackpackAsFallback(const bool v)
{
    if (m_containers)
        m_containers->setUseMainBackpackAsFallback(v);
    else
        TDialogController::setUseMainBackpackAsFallback(v);
}

void TManageLootContainerDialogController::setFilterString(const QString& v)
{
    if (::cip::unchanged(m_filterString, v))
        return;
    m_filterString = v;
    m_listProxy.setNameFilter(v, false, false, false, false, {}, false);
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
    input::publish(input::makeAction(A::OPEN_URL_GET_PREMIUM));
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

void TManageLootContainerDialogController::requestAddItemBlackWhitelist() { startListItemPick(); }

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
        const auto assignment = m_containers->assignment(static_cast<uint8_t>(row->category()));
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
    m_listProxy.setNameFilter(m_filterString, false, false, false, false, {}, false);
}

void TManageLootContainerDialogController::syncPremium()
{
    const bool premium = g_game.getLocalPlayer() && g_game.getLocalPlayer()->isPremium();
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
    if (!m_isPremium && category != UnassignedCategory) {
        qInfo().nospace() << "tibia.loot: startContainerPick blocked (premium) category=" << category;
        return;
    }
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

void TManageLootContainerDialogController::startListItemPick()
{
    qInfo() << "tibia.loot: startListItemPick";
    m_pick = PickMode::ListItem;
    m_pickCategory = 0;
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
    if (target && target->object && target->object->isItem()) {
        const ItemPtr item = target->object->static_self_cast<Item>();
        if (mode == PickMode::ListItem) {
            if (m_itemInfo)
                m_itemInfo->setLootListed(item->getId(), true);
        } else if (item->isContainer()) {
            sendContainerAction(mode == PickMode::LootContainer ? ActionSelectLoot : ActionSelectObtain, static_cast<uint8_t>(category),
                                item);
        } else {
            m_confirm->setButtons({ new TMessageDialogButton(qtTrId("ok"), TMessageDialogController::Button::OK,
                                                             QKeySequence(QStringLiteral("Return"))) });
            m_confirm->showMessage(qtTrId("managecontainer_invalid_container_caption"), qtTrId("managecontainer_invalid_container_text"));
        }
    }
    if (g_game.isOnline())
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

void TManageLootContainerDialogController::sendContainerAction(const uint8_t action, const uint8_t category, const ItemPtr& item)
{
    if (!g_game.isOnline())
        return;
    // Proto::ClientLootContainer = 144
    qInfo().nospace() << "tibia.loot: ClientLootContainer(144) action=" << int(action) << " category=" << int(category)
                      << " itemId=" << (item ? int(item->getId()) : 0);
    g_game.sendContainerAction(action, category);
    if (item)
        g_game.openContainerQuickLoot(action, category, item->getPosition(), item->getId(), item->getStackPos(), false);
    else
        g_game.openContainerQuickLoot(action, category, {}, 0, 0, false);
}

void TManageLootContainerDialogController::openAssignedContainer(const uint8_t category, const uint16_t typeId, const uint8_t action)
{
    if (!isSet(typeId) || !g_game.isOnline())
        return;
    for (const auto& entry : g_game.getContainers()) {
        const ContainerPtr& container = entry.second;
        if (container && container->getContainerItem() && container->getContainerItem()->getId() == typeId)
            return;
    }
    g_game.openContainerQuickLoot(action, category, {}, 0, 0, false);
}

}
