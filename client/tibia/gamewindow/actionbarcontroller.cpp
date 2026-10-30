#include "actionbarcontroller.h"

#include "app/clientoptions.h"
#include "gamewindow/actionbarmanager.h"
#include "gamewindow/actionbuttoncontroller.h"
#include "gamewindow/draganddropcontroller.h"
#include "spells/spellinfostorage.h"

#include <client/item.h>
#include <tibia/assets/thingtype.h>

#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>

#include <algorithm>

namespace tibia::gamewindow {

namespace {

using Setting = config::TActionSetting;
using P = EActionBarPosition;

struct TPositionOptions
{
    const char* locked;
    const char* showGroup;
    std::array<const char*, 3> showBar;
};

/// Options 0x0-0xe.
const TPositionOptions& optionsOf(const P position)
{
    static const TPositionOptions bottom{ "actionBarBottomLocked", "actionBarsShowBottom", { "actionBarShowBottom1", "actionBarShowBottom2", "actionBarShowBottom3" } };
    static const TPositionOptions left{ "actionBarLeftLocked", "actionBarsShowLeft", { "actionBarShowLeft1", "actionBarShowLeft2", "actionBarShowLeft3" } };
    static const TPositionOptions right{ "actionBarRightLocked", "actionBarsShowRight", { "actionBarShowRight1", "actionBarShowRight2", "actionBarShowRight3" } };
    switch (position) {
    case P::ActionBarPositionLeft:
        return left;
    case P::ActionBarPositionRight:
        return right;
    default:
        return bottom;
    }
}

bool option(const QJsonObject& options, const char* name, const bool fallback)
{
    return options.value(QLatin1String(name)).toBool(fallback);
}

}

TActionBarController::TActionBarController(TActionBarManager* manager, const int actionBarId, const EActionBarPosition position, QObject* buttonsDisplayOptions,
                                           QQuickItem* gameWindowItem, QQuickItem* mapWindowPane, QObject* parent)
    : cip::tibia::gamewindow::TActionBarController(parent)
    , m_manager(manager)
{
    m_actionBarId = actionBarId;
    m_position = position;
    m_vertical = position != P::ActionBarPositionBottom;
    m_buttonsDisplayOptions = buttonsDisplayOptions;
    m_gameWindowQuickItem = gameWindowItem;
    m_mapWindowPane = mapWindowPane;
    for (int id = 1; id <= ButtonCount; ++id) {
        auto* button = new TActionButtonController(this, id, -1, this);
        m_buttons.push_back(button);
        m_actionButtonModel << button;
    }
}

TActionButtonController* TActionBarController::button(const int buttonId) const
{
    return buttonId >= 1 && buttonId <= int(m_buttons.size()) ? m_buttons[size_t(buttonId - 1)] : nullptr;
}

void TActionBarController::createItem(QQmlEngine* engine, QQuickItem* container)
{
    if (m_item || !engine || !container)
        return;
    QQmlComponent component(engine, QUrl(QStringLiteral("qrc:/qt/qml/qmlcomponents/qml/ActionBar.qml")));
    QObject* object = component.beginCreate(engine->rootContext());
    auto* item = qobject_cast<QQuickItem*>(object);
    if (!item) {
        qWarning().noquote() << "ActionBar.qml:" << component.errorString();
        delete object;
        return;
    }
    item->setProperty("controller", QVariant::fromValue<QObject*>(this));
    item->setParentItem(container);
    item->setParent(container);
    component.completeCreate();
    m_item = item;
}

void TActionBarController::destroyItem()
{
    if (m_item)
        m_item->deleteLater();
    m_item = nullptr;
    m_numVisibleSlots = 0;
}

void TActionBarController::applyMappings(const bool chatOn)
{
    const config::THotkeyOptions* hotkeys = config::THotkeyOptions::instance();
    for (TActionButtonController* button : m_buttons) {
        const int id = int(button->actionButtonID());
        button->setSetting(hotkeys->mapping(m_actionBarId, id), hotkeys->buttonKeys(m_actionBarId, id, chatOn));
        button->setHotkeys(hotkeys->allButtonKeys(m_actionBarId, id, chatOn));
    }
    // CIP FUN_100c80df0.
    const int first = hotkeys->firstVisibleButton(m_actionBarId);
    if (first != m_firstVisibleButtonId) {
        m_firstVisibleButtonId = first;
        Q_EMIT firstVisibleButtonIdChanged();
    }
}

void TActionBarController::setFirstVisibleButtonId(const int v)
{
    if (v == m_firstVisibleButtonId)
        return;
    m_firstVisibleButtonId = v;
    if (m_firstVisibleCanBeSaved)
        config::THotkeyOptions::instance()->setFirstVisibleButton(m_actionBarId, v);
    Q_EMIT firstVisibleButtonIdChanged();
}

bool TActionBarController::insertSpell(const int spellId, const bool newlyUnlocked)
{
    const int first = std::max(0, m_firstVisibleButtonId);
    const int end = std::min(first + m_numVisibleSlots, int(m_buttons.size()));
    for (int index = first; index < end; ++index) {
        TActionButtonController* button = m_buttons[size_t(index)];
        if (!button->isShownEmpty())
            continue;
        const spells::TSpellInfo* spell = spells::TSpellInfoStorage::instance()->spell(spellId);
        if (!spell)
            return false;
        Setting setting;
        setting.kind = Setting::TEXT;
        setting.text = spell->words;
        setting.sendAutomatically = true;
        assign(index + 1, -1, setting);
        button->setIsNewlyUnlocked(newlyUnlocked);
        Q_EMIT button->startInsertAnimation();
        return true;
    }
    return false;
}

void TActionBarController::refreshObjects()
{
    for (TActionButtonController* button : m_buttons)
        button->refreshObject();
}

bool TActionBarController::refreshCooldowns(const qint64 now)
{
    bool running = false;
    for (TActionButtonController* button : m_buttons)
        running = button->refreshCooldown(now) || running;
    return running;
}

void TActionBarController::setLocked(const bool locked)
{
    if (locked == m_locked)
        return;
    m_locked = locked;
    Q_EMIT lockedChanged();
}

Setting TActionBarController::settingAt(const int buttonId, const int multiActionIndex) const
{
    const Setting setting = config::THotkeyOptions::instance()->mapping(m_actionBarId, buttonId);
    if (multiActionIndex < 0)
        return setting;
    if (setting.kind == Setting::MULTI_ACTION && multiActionIndex < int(setting.multiActions.size()))
        return setting.multiActions[size_t(multiActionIndex)];
    return {};
}

void TActionBarController::assign(const int buttonId, const int multiActionIndex, const Setting& setting)
{
    config::THotkeyOptions* hotkeys = config::THotkeyOptions::instance();
    if (multiActionIndex < 0) {
        hotkeys->setMapping(m_actionBarId, buttonId, setting);
        return;
    }
    Setting multi = hotkeys->mapping(m_actionBarId, buttonId);
    if (multi.kind != Setting::MULTI_ACTION)
        return;
    if (int(multi.multiActions.size()) <= multiActionIndex)
        multi.multiActions.resize(size_t(multiActionIndex + 1));
    multi.multiActions[size_t(multiActionIndex)] = setting;
    hotkeys->setMapping(m_actionBarId, buttonId, multi);
}

void TActionBarController::openAssignDialog(const EAssignKind kind, const int buttonId, const int multiActionIndex)
{
    m_manager->openAssignDialog(this, kind, buttonId, multiActionIndex);
}

void TActionBarController::editHotkey(const int buttonId)
{
    m_manager->editHotkey(this, buttonId);
}

void TActionBarController::onDropped(const uint ButtonId, const int MultiActionIndex)
{
    TDragAndDropController* dnd = TDragAndDropController::instance();
    if (!dnd)
        return;
    const input::TGameActionPtr move = dnd->takeAction();
    if (!move || !move->object || !move->object->isItem())
        return;
    const ItemPtr item = move->object->static_self_cast<Item>();
    m_manager->assignDroppedObject(this, int(ButtonId), MultiActionIndex, item, dnd->modifiers().testFlag(Qt::AltModifier));
}

void TActionBarController::onSwapActions(const uint SourceActionBarId, const uint SourceButtonId, const int SourceMultiActionIndex,
                                         const uint DestinationActionBarId, const uint DestinationButtonId, const int DestinationMultiActionIndex)
{
    if (SourceActionBarId == DestinationActionBarId && SourceButtonId == DestinationButtonId && SourceMultiActionIndex == DestinationMultiActionIndex)
        return;
    TActionBarController* source = m_manager->bar(int(SourceActionBarId));
    TActionBarController* destination = m_manager->bar(int(DestinationActionBarId));
    if (!source || !destination)
        return;
    const Setting from = source->settingAt(int(SourceButtonId), SourceMultiActionIndex);
    const Setting to = destination->settingAt(int(DestinationButtonId), DestinationMultiActionIndex);
    if (DestinationMultiActionIndex >= 0 && from.kind == Setting::MULTI_ACTION)
        return;
    destination->assign(int(DestinationButtonId), DestinationMultiActionIndex, from);
    source->assign(int(SourceButtonId), SourceMultiActionIndex, to);
}

void TActionBarController::onSpellIconDropped(const uint ButtonId, const int SpellEnumId, const int MultiActionIndex)
{
    const spells::TSpellInfo* spell = spells::TSpellInfoStorage::instance()->spell(SpellEnumId);
    if (!spell)
        return;
    if (!spell->parameterHints.isEmpty()) {
        m_manager->openSpellDialogFor(this, int(ButtonId), MultiActionIndex, SpellEnumId);
        return;
    }
    Setting setting;
    setting.kind = Setting::TEXT;
    setting.text = spell->words;
    setting.sendAutomatically = true;
    assign(int(ButtonId), MultiActionIndex, setting);
}

void TActionBarController::onSpellIconDropped(const uint ButtonId, const int SpellEnumId)
{
    onSpellIconDropped(ButtonId, SpellEnumId, -1);
}

void TActionBarController::closeOpenMultiActionButtons()
{
    for (TActionButtonController* button : m_buttons)
        if (button->multiActionExpanded())
            button->cip::tibia::gamewindow::TActionButtonController::setMultiActionExpanded(false);
}

void TActionBarController::setNumVisibleSlots(const int Amount)
{
    m_numVisibleSlots = Amount;
}

TActionBarGroup::TActionBarGroup(TActionBarManager* manager, const EActionBarPosition position, QQuickItem* container, QQmlEngine* engine,
                                 QQuickItem* gameWindowItem, QQuickItem* mapWindowPane, QObject* buttonsDisplayOptions, QObject* parent)
    : cip::tibia::gamewindow::TActionBarGroup(parent)
    , m_position(position)
    , m_container(container)
    , m_engine(engine)
{
    if (container) {
        m_barContainer = container->findChild<QQuickItem*>(QStringLiteral("actionBarContainer"));
        container->setProperty("controller", QVariant::fromValue<QObject*>(this));
    }
    for (const int id : barIds(position))
        m_bars.push_back(new TActionBarController(manager, id, position, buttonsDisplayOptions, gameWindowItem, mapWindowPane, this));
    applyOptions();
}

TActionBarGroup::~TActionBarGroup()
{
    if (m_container)
        m_container->setProperty("controller", QVariant::fromValue<QObject*>(nullptr));
    for (TActionBarController* bar : m_bars)
        bar->destroyItem();
}

std::array<int, 3> TActionBarGroup::barIds(const EActionBarPosition position)
{
    switch (position) {
    case P::ActionBarPositionLeft:
        return { 4, 5, 6 };
    case P::ActionBarPositionRight:
        return { 7, 8, 9 };
    default:
        return { 1, 2, 3 };
    }
}

void TActionBarGroup::applyOptions()
{
    const QJsonObject options = app::ClientOptions::load().options();
    const TPositionOptions& names = optionsOf(m_position);
    const bool groupShown = option(options, names.showGroup, true);
    uint count = 0;
    for (size_t i = 0; i < m_bars.size(); ++i) {
        TActionBarController* bar = m_bars[i];
        const bool shown = groupShown && option(options, names.showBar[i], true);
        if (shown) {
            ++count;
            if (!bar->item())
                bar->createItem(m_engine, m_barContainer);
        } else {
            bar->destroyItem();
        }
    }
    if (count != m_actionBarCount) {
        m_actionBarCount = count;
        Q_EMIT numberOfActionBarsChanged();
    }
    const bool locked = option(options, names.locked, false);
    if (locked != m_locked) {
        m_locked = locked;
        Q_EMIT lockedChanged();
    }
    for (TActionBarController* bar : m_bars)
        bar->setLocked(locked);
}

void TActionBarGroup::onActionBarsOptionsChanged()
{
    applyOptions();
}

void TActionBarGroup::onLockButtonClicked()
{
    app::ClientOptions::store(QLatin1String(optionsOf(m_position).locked), !m_locked);
}

void TActionBarGroup::setFirstVisibleButtonIdCanBeSaved(const bool CanBeSaved)
{
    for (TActionBarController* bar : m_bars)
        bar->setFirstVisibleButtonIdCanBeSaved(CanBeSaved);
}

}
