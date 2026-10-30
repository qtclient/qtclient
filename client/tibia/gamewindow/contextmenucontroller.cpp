#include "contextmenucontroller.h"

#include "gamewindow/sidebarmanager.h"
#include "input/gameactionhandlers.h"
#include "input/gameactionmapper.h"

#include <tibia/world/creature.h>
#include <tibia/world/map.h>

#include <QCursor>
#include <QGuiApplication>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;

TContextMenuController* g_instance = nullptr;

/// The name an action names: its creature's, else its text.
QString nameOf(const input::TGameAction& action)
{
    if (action.creatureId) {
        if (const CreaturePtr creature = g_map.getCreatureById(action.creatureId))
            return QString::fromStdString(creature->getName());
    }
    return action.text;
}

const char* simpleLabel(const A type)
{
    switch (type) {
    case A::USE: return "context_use";
    case A::USE_WITH: return "context_use_with";
    case A::LOOK:
    case A::LOOK_AT_CREATURE: return "context_look";
    case A::INSPECT_OBJECT: return "context_inspect_object";
    case A::WRAP: return "context_wrap";
    case A::UNWRAP: return "context_unwrap";
    case A::ROTATE_OBJECT: return "context_rotate";
    case A::CONFIGURE_SHOW_OFF_SOCKET: return "context_configure_show_off_socket";
    case A::BROWSE_FIELD: return "context_browsefield";
    case A::OPEN: return "context_open";
    case A::OPEN_IN_NEW_WINDOW: return "context_open_new_window";
    case A::OPEN_PARENT_CONTAINER: return "context_open_parent_container";
    case A::MOVE_UP: return "context_move_up";
    case A::ATTACK_CREATURE: return "context_attack";
    case A::STOP_ATTACKING_CREATURE: return "context_stop_attack";
    case A::FOLLOW_CREATURE: return "context_follow";
    case A::STOP_FOLLOWING_CREATURE: return "context_stop_follow";
    case A::COPY_CREATURE_WITH_ID_NAME_TO_CLIPBOARD:
    case A::COPY_CREATURE_NAME_TO_CLIPBOARD: return "context_copy_name";
    case A::CLOSE_CHAT_CHANNEL: return "context_close_chat";
    case A::CLEAR_CHAT_CHANNEL: return "context_clear_chat";
    case A::CHAT_CHANNEL_SUBSCRIBE_TO_SERVER_LOG: return "context_show_server_messages";
    case A::CHAT_CHANNEL_UNSUBSCRIBE_SERVER_LOG: return "context_hide_server_messages";
    case A::CHAT_CHANNEL_MUTE: return "context_mute_chat_channel";
    case A::CHAT_CHANNEL_UNMUTE: return "context_unmute_chat_channel";
    case A::SHOW_CHAT_CHANNEL_AS_SECONDARY: return "context_show_as_secondary_chat";
    case A::HIDE_SECONDARY_CHAT_CHANNEL: return "context_hide_as_secondary_chat";
    case A::SAVE_CHAT_CHANNEL_TO_FILE: return "context_save_channel";
    case A::CHAT_CHANNEL_SELECT_ALL: return "context_select_all";
    case A::COPY_CHAT_ENTRY_TO_CLIPBOARD: return "context_copy_message";
    case A::INVITE_PLAYER_TO_PRIVATE_CHAT_CHANNEL: return "context_invite_to_channel";
    case A::MOUNT_PLAYER: return "context_mount";
    case A::UNMOUNT_PLAYER: return "context_dismount";
    case A::START_TALKING_TO_NPC: return "context_talk";
    case A::OPEN_PREY_DIALOG: return "context_open_prey_dialog";
    case A::OPEN_INVITE_TO_CHAT_CHANNEL_DIALOG: return "context_invite";
    case A::OPEN_EXCLUDE_FROM_CHAT_CHANNEL_DIALOG: return "context_exclude";
    case A::SHOW_DIALOG_CREATE_MINIMAP_MARKER: return "context_create_mark";
    case A::SHOW_DIALOG_EDIT_MINIMAP_MARKER: return "context_edit_mark";
    case A::DELETE_MINIMAP_MARKER: return "context_delete_mark";
    case A::REQUEST_OUTFITS_FROM_SERVER: return "context_select_outfit";
    case A::LEAVE_PARTY: return "context_leave_party";
    case A::OPEN_ADD_VIP_DIALOG: return "context_add_vip";
    case A::OPEN_ADD_VIP_GROUP_DIALOG: return "context_add_vip_group";
    case A::SORT_VIPS_BY_NAME: return "context_sort_vip_by_name";
    case A::SORT_VIPS_BY_ICON: return "context_sort_vip_by_icon";
    case A::SORT_VIPS_BY_STATUS: return "context_sort_vip_by_status";
    case A::HIDE_OFFLINE_VIPS: return "context_hide_offline_vips";
    case A::SHOW_OFFLINE_VIPS: return "context_show_offline_vips";
    case A::SHOW_VIPS_GROUPS: return "context_show_vip_groups";
    case A::HIDE_VIPS_GROUPS: return "context_hide_vip_groups";
    case A::COPY_TEXT_TO_CLIPBOARD: return "copy";
    case A::TRADE_ITEM: return "context_trade";
    case A::REPORT_STATEMENT: return "context_report_statement";
    case A::REPORT_NAME: return "context_report_name";
    case A::REPORT_BOT: return "context_report_bot";
    case A::REPORT_HIRELING_NAME: return "context_report_hireling_name";
    case A::REPORT_TEXT: return "context_report_text";
    case A::RECENTLY_TRADED: return "context_recently_traded";
    case A::GET_GUILD_MESSAGE: return "context_edit_guild_message";
    case A::SHOW_CHANNEL_JOIN_LEAVE_MESSAGES: return "context_show_join_leave_messages";
    case A::HIDE_CHANNEL_JOIN_LEAVE_MESSAGES: return "context_hide_join_leave_messages";
    case A::REPORT_BUG_MAP: return "context_report_coordinate";
    case A::REPORT_BUG_TYPO: return "context_report_typo";
    case A::BUY_HIRELING_NAME_OR_SEX_CHANGE: return "context_buy_hireling_name_or_sex_change";
    case A::OPEN_CYCLOPEDIA: return "context_open_in_cyclopedia";
    case A::OPEN_MANGE_LOOT_CONTAINER: return "managecontainer_caption";
    case A::LOOT_ADD_TO_BLACKWHITELIST: return "context_add_to_loot_blackwhitelist";
    case A::LOOT_REMOVE_FROM_BLACKWHITELIST: return "context_remove_from_loot_blackwhitelist";
    case A::STASH_STOW_TYPE: return "context_stash_stow_type";
    case A::STASH_STOW_CONTAINER_CONTENT: return "context_stash_stow_container_content";
    case A::STASH_OPEN_STOW_STACK_DIALOG: return "context_stash_open_stow_stack_dialog";
    case A::STASH_OPEN_RETRIEVE_DIALOG: return "context_stash_open_retrieve_dialog";
    case A::MARKET_BROWSE: return "context_trade_via_market";
    case A::GATHER_ALL: return "context_gather_all_coop_loot";
    case A::OPEN_WEAPON_PROFICIENCY_DIALOG: return "context_open_weapon_proficiency_dialog";
    default: return nullptr;
    }
}

/// Labels with a name argument.
const char* namedLabel(const A type)
{
    switch (type) {
    case A::INVITE_TO_INSPECT_PLAYER: return "context_invite_to_inspect_player";
    case A::ASK_TO_INSPECT_PLAYER: return "context_ask_to_inspect";
    case A::ALLOW_INSPECT_PLAYER: return "context_allow_inspect_player";
    case A::INSPECT_PLAYER: return "context_inspect_player";
    case A::REVOKE_INSPECT_PLAYER: return "context_revoke_inspect_player";
    case A::JOIN_AGGRESSION: return "context_join_aggression";
    case A::INVITE_TO_PARTY: return "context_invite_to_party";
    case A::JOIN_PARTY: return "context_join_party";
    case A::REVOKE_PARTY_INVITATION: return "context_revoke_party_invitation";
    case A::PASS_PARTY_LEADERSHIP: return "context_pass_party_leadership";
    case A::OPEN_MESSAGE_TO_CHAT_CHANNEL: return "context_message_to";
    case A::SELECT_CHAT_CHANNEL_AS_SECONDARY: return "context_select_as_secondary_chat";
    case A::ADD_NEW_VIP: return "context_add_player_to_vip";
    case A::REMOVE_VIP: return "context_remove_vip";
    case A::OPEN_EDIT_VIP_DIALOG: return "context_edit_vip";
    case A::EXIVA_ADD_CHARACTER_TO_WHITELIST: return "context_add_vip_to_exiva_whitelist";
    case A::EXIVA_REMOVE_CHARACTER_FROM_WHITELIST: return "context_remove_vip_from_exiva_whitelist";
    case A::REMOVE_VIP_GROUP: return "context_remove_vip_group";
    case A::OPEN_EDIT_VIP_GROUP_DIALOG: return "context_edit_vip_group";
    case A::IGNORE_NAME: return "context_ignore";
    case A::UNIGNORE_NAME: return "context_unignore";
    default: return nullptr;
    }
}

/// The first of `types` among the actions (CIP 0x100b7eef0's modifier search).
A firstPresent(const std::vector<A>& types, const input::TGameActionList& actions)
{
    for (const A type : types) {
        for (const input::TGameActionPtr& action : actions) {
            if (action->type == type)
                return type;
        }
    }
    return A::UNKNOWN;
}

}

TContextMenuEntryItem::TContextMenuEntryItem(const QString& text, const QString& shortcut, const bool separator, QObject* parent)
    : cip::tibia::gamewindow::TContextMenuEntryItem(parent)
{
    m_text = text;
    m_shortcutString = shortcut;
    m_styleType = QStringLiteral("ContextMenu");
    m_isSeparator = separator;
}

TContextMenuController::TContextMenuController(QQmlEngine* engine, QQuickItem* root, QObject* parent)
    : cip::tibia::gamewindow::TContextMenuController(parent)
    , m_root(root)
{
    g_instance = this;

    // CIP 0x100b7e720.
    m_shield = createQuickItem(engine, QStringLiteral("TibiaMouseShield.qml"));
    if (m_shield) {
        m_shield->setParentItem(root);
        m_shield->setVisible(false);
        m_shield->setZ(m_shield->z() + 1);
        connect(m_shield, SIGNAL(anyButtonClicked()), this, SLOT(onRequestClose()));
        m_menu = createQuickItem(engine, QStringLiteral("ContextMenuTemplate.qml"));
        if (m_menu) {
            m_menu->setParentItem(m_shield->findChild<QQuickItem*>(QStringLiteral("placeholder")));
            m_menu->setProperty("controller", QVariant::fromValue<QObject*>(this));
            connect(m_menu, &QQuickItem::heightChanged, this, &TContextMenuController::onCalculatePosition);
            connect(m_menu, &QQuickItem::widthChanged, this, &TContextMenuController::onCalculatePosition);
        }
    }
    if (engine)
        engine->rootContext()->setContextProperty(QStringLiteral("tibiaContextMenuController"), this);

    m_showTimer.setSingleShot(true);
    connect(&m_showTimer, &QTimer::timeout, this, &TContextMenuController::onAsyncShow);
    connect(&m_discardKeys, &input::TDiscardAllKeyboardInputAndSignalEscapePressedEventFilter::escapePressed, this,
            &TContextMenuController::onRequestClose);

    input::setActionHandler(A::CHANGE_CONTEXT_MENU_ENABLED, [this](const input::TGameAction& action) {
        setEnabled(action.enable);
        return true;
    });
}

TContextMenuController::~TContextMenuController()
{
    setKeyboardBlocked(false);
    input::setActionHandler(A::CHANGE_CONTEXT_MENU_ENABLED, nullptr);
    if (g_instance == this)
        g_instance = nullptr;
    delete m_menu;
    delete m_shield;
    qDeleteAll(m_entries);
}

TContextMenuController* TContextMenuController::instance()
{
    return g_instance;
}

QString TContextMenuController::labelOf(const input::TGameAction& action, QString& shortcut)
{
    if (const char* id = simpleLabel(action.type)) {
        if (action.type == A::BUY_HIRELING_NAME_OR_SEX_CHANGE)
            shortcut = qtTrId("context_store_dialog");
        return qtTrId(id);
    }
    if (const char* id = namedLabel(action.type))
        return qtTrId(id).arg(nameOf(action));
    switch (action.type) {
    case A::QUICK_LOOT:
        return qtTrId(action.position.x != UINT16_MAX ? "context_loot_all" : "context_loot");
    case A::SET_SHARED_EXPERIENCE:
        return qtTrId(action.enable ? "context_shared_exp_on" : "context_shared_exp_off");
    case A::SET_PLAYER_MOVEMENT_INTENT:
        return QStringLiteral("Walk here");
    case A::GOD_TELEPORT:
        return QStringLiteral("Teleport here");
    case A::CYCLOPEDIA_MAP_SELECT_ITEM:
        return QStringLiteral("Select");
    default:
        return QStringLiteral("Unknown Game Action (%1)").arg(static_cast<int>(action.type));
    }
}

void TContextMenuController::showGameActions(const input::TGameActionList& actions, const int context, const input::TGameActionMapper& mapper)
{
    clear();
    const A shift = firstPresent(mapper.ruleTypes(context, Qt::LeftButton, Qt::ShiftModifier), actions);
    const A ctrl = firstPresent(mapper.ruleTypes(context, Qt::LeftButton, Qt::ControlModifier), actions);
    const A alt = firstPresent(mapper.ruleTypes(context, Qt::LeftButton, Qt::AltModifier), actions);
    for (const input::TGameActionPtr& action : actions) {
        if (action->type == A::CONTEXT_MENU_SEPARATOR) {
            addSeparator();
            continue;
        }
        QString shortcut;
        const QString label = labelOf(*action, shortcut);
        if (action->type == shift)
            shortcut = qtTrId("context_keymodifier_shift");
        if (action->type == ctrl)
            shortcut = qtTrId("context_keymodifier_ctrl");
        if (action->type == alt)
            shortcut = qtTrId("context_keymodifier_alt");
        TContextMenuEntryItem* entry = addEntry(label, shortcut);
        connect(entry, &TContextMenuEntryItem::triggered, this, [action] { input::publish(action); });
    }
    std::erase_if(m_extensions, [](const auto& extension) { return extension.first.isNull(); });
    for (const auto& extension : std::vector(m_extensions))
        extension.second(actions, context, *this);
    requestShow();
}

void TContextMenuController::addGameActionsExtension(QObject* owner, GameActionsExtension extension)
{
    if (owner && extension)
        m_extensions.emplace_back(owner, std::move(extension));
}

void TContextMenuController::showContextMenuForTextInputQmlItem(QObject* textInput)
{
    clear();
    if (!textInput)
        return;
    const QPointer<QObject> field(textInput);
    const bool hasSelection = textInput->property("hasSelection").toBool();
    const bool plain = textInput->property("echoMode").toInt() == 0;
    const bool readOnly = textInput->property("readOnly").toBool();

    const auto removeSelection = [field] {
        if (!field)
            return;
        const int start = field->property("selectionStart").toInt();
        const int end = field->property("selectionEnd").toInt();
        QMetaObject::invokeMethod(field, "remove", Q_ARG(int, start), Q_ARG(int, end));
        QMetaObject::invokeMethod(field, "deselect");
    };
    const auto copySelection = [field] {
        if (!field)
            return;
        input::publish(std::make_shared<input::TGameActionCopyTextToClipboard>(
            field->property("selectedText").toString()));
    };

    if (hasSelection && plain && !readOnly) {
        connect(addEntry(qtTrId("cut_text")), &TContextMenuEntryItem::triggered, this, [=] {
            const QString text = field ? field->property("selectedText").toString() : QString();
            removeSelection();
            input::publish(std::make_shared<input::TGameActionCopyTextToClipboard>(text));
        });
    }
    bool copyShown = false;
    if (hasSelection && plain) {
        connect(addEntry(qtTrId("copy")), &TContextMenuEntryItem::triggered, this, copySelection);
        copyShown = true;
    }
    if (textInput->property("canPaste").toBool()) {
        connect(addEntry(qtTrId("paste_text")), &TContextMenuEntryItem::triggered, this, [field] {
            if (!field)
                return;
            QMetaObject::invokeMethod(field, "paste");
            QMetaObject::invokeMethod(field, "deselect");
        });
        addSeparator();
    } else if (copyShown) {
        addSeparator();
    }
    if (hasSelection && !readOnly) {
        connect(addEntry(qtTrId("delete_text")), &TContextMenuEntryItem::triggered, this, removeSelection);
        addSeparator();
    }
    connect(addEntry(qtTrId("select_all_text")), &TContextMenuEntryItem::triggered, this, [field] {
        if (field)
            QMetaObject::invokeMethod(field, "selectAll");
    });
    requestShow();
}

void TContextMenuController::clear()
{
    for (QObject* entry : std::as_const(m_entries))
        entry->deleteLater();
    m_entries.clear();
    Q_EMIT entriesChanged();
}

void TContextMenuController::addSeparator()
{
    if (m_entries.isEmpty() || m_entries.last()->property("isSeparator").toBool())
        return;
    m_entries.append(new TContextMenuEntryItem({}, {}, true, this));
}

TContextMenuEntryItem* TContextMenuController::addEntry(const QString& text, const QString& shortcut)
{
    auto* entry = new TContextMenuEntryItem(text, shortcut, false, this);
    m_entries.append(entry);
    connect(entry, &TContextMenuEntryItem::triggered, this, &TContextMenuController::onRequestClose);
    return entry;
}

void TContextMenuController::requestShow()
{
    if (!m_menu || !m_enabled || m_entries.isEmpty())
        return;
    Q_EMIT entriesChanged();
    m_showTimer.start();
}

void TContextMenuController::onRequestClose()
{
    if (m_shield)
        m_shield->setVisible(false);
    setKeyboardBlocked(false);
    Q_EMIT contextMenuClosed();
}

void TContextMenuController::onAsyncShow()
{
    onCalculatePosition();
    setKeyboardBlocked(true);
    if (m_shield)
        m_shield->setVisible(true);
}

void TContextMenuController::onCalculatePosition()
{
    if (!m_menu || !m_root || !m_menu->window())
        return;
    const QPoint cursor = m_menu->window()->mapFromGlobal(QCursor::pos());
    const QRect area = m_root->mapRectToScene(m_root->boundingRect()).toRect();
    const int width = qRound(m_menu->width());
    const int height = qRound(m_menu->height());
    int x = cursor.x();
    if (area.left() + area.width() - 1 < x + width)
        x -= width;
    int y = cursor.y();
    if (area.top() + area.height() - 1 < y + height)
        y -= height;
    const QPointF position(std::max(x, area.left()), std::max(y, area.top()));
    if (!qFuzzyCompare(position.x(), m_menu->x()) || !qFuzzyCompare(position.y(), m_menu->y()))
        m_menu->setPosition(position);
}

void TContextMenuController::setKeyboardBlocked(const bool blocked)
{
    if (blocked == m_keyboardBlocked)
        return;
    m_keyboardBlocked = blocked;
    if (blocked)
        qApp->installEventFilter(&m_discardKeys);
    else
        qApp->removeEventFilter(&m_discardKeys);
}

}
