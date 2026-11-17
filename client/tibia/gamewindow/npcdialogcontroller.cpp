#include "npcdialogcontroller.h"
#include "colorhelper.h"

#include "chat/chatgameactionhandler.h"
#include "input/gameactionhandlers.h"
#include "config/clientoptions.h"

#include "trade/tradeciptypes.h"

#include "chat/chatchannel.h"
#include "gamewindow/chatcontroller.h"

#include <creatures/creaturestorage.h>
#include <client/outfit.h>

#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QQuickItem>
#include <QQuickWindow>
#include <tibia/game/gamenotify.h>
#include <client/staticdata.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/tgamenotify.h>

namespace tibia::gamewindow {

namespace {

QColor outfitColor(const int index)
{
    const QColor c = tibia::shared::outfitColor(index);
    return QColor(c.red(), c.green(), c.blue());
}

}

void TNpcDialogOutfit::setFromCreatureNamed(const QString& name)
{
    tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance();
    if (tibia::game::hasLocalPlayer() && storage && !name.isEmpty()) {
        const auto ar = tibia::game::awareRange();
    const AwareRange range{ ar.left, ar.top, ar.right, ar.bottom };
        for (const std::shared_ptr<tibia::creatures::TCreature>& creature :
             storage->spectatorsNear(tibia::game::localPlayerPosition(), false, range.left, range.right, range.top,
                                     range.bottom)) {
            if (creature && creature->name() == name) {
                setOutfit(creature->outfit());
                return;
            }
        }
    }
    m_outfitId = 0;
    m_addons = 0;
    m_isObject = false;
    m_colors.clear();
    Q_EMIT changed();
}

void TNpcDialogOutfit::setOutfit(const Outfit& outfit)
{
    m_isObject = outfit.getCategory() == ThingCategoryItem;
    m_outfitId = m_isObject ? outfit.getAuxId() : outfit.getId();
    m_addons = outfit.getAddons();
    m_colors = {
        { QStringLiteral("headColor"), outfitColor(outfit.getHead()) },
        { QStringLiteral("torsoColor"), outfitColor(outfit.getBody()) },
        { QStringLiteral("legsColor"), outfitColor(outfit.getLegs()) },
        { QStringLiteral("detailColor"), outfitColor(outfit.getFeet()) },
    };
    Q_EMIT changed();
}

TNpcDialogController::TNpcDialogController(trade::TNPCTradeStorage* storage, QObject* parent)
    : TDialogControllerWithFocusHandlingBase(QStringLiteral("NpcDialog.qml"), parent)
    , m_storage(storage)
    , m_trade(storage)
{
    m_tradeController = &m_trade;
    m_npcOutfit = &m_outfit;
    m_keywordButtonsModel = &m_keywordButtons;
    m_positionAndSize = QRect(-1, -1, 0, 0);
    connect(config::TClientOptions::instance(), &config::TClientOptions::npcDialogOptionsChanged, this,
            &TNpcDialogController::onDialogOptionsChanged);
    if (config::TClientOptions::instance()->npcDialogOptions().isValid())
        onDialogOptionsChanged();
    if (trade::TNPCTradeProtocolMessageHandler* messages = trade::TNPCTradeProtocolMessageHandler::instance()) {
        connect(messages, &trade::TNPCTradeProtocolMessageHandler::requestOpenNpcTrade, this, &TNpcDialogController::onRequestOpenNPCTrade);
        connect(messages, &trade::TNPCTradeProtocolMessageHandler::requestCloseNpcTrade, this, &TNpcDialogController::onRequestCloseNPCTrade);
    }
    // CIP / : while the trade has the keyboard the dialog's chat input lets go of it.
    m_tradeFocus.setActive(false);
    connect(&m_tradeFocus, &TUiFocusManager::focusChanged, this, [this] {
        m_tradeHasFocus = m_tradeFocus.hasFocus();
        setFocusAllowed(!m_tradeHasFocus);
        Q_EMIT tradeFocusChanged();
    });
    connect(this, &cip::tibia::gamewindow::TDialogControllerWithFocusHandlingBase::focusChanged, this,
            &TNpcDialogController::onFocusChanged);
    for (const input::EGameAction type : { input::EGameAction::CHAT_CHANNEL_SELECT_ALL, input::EGameAction::REQUEST_SELECT_ALL_TEXT,
                                           input::EGameAction::REQUEST_COPY_SELECTED_TEXT_TO_CLIPBOARD })
        m_actionListeners.push_back(input::addActionListener(type, [this](const input::IGameAction& action) { return handleGameAction(action); }));

    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameEnded, this, [this]() {
        m_talkPartners.clear();
    });
}

QString TNpcDialogController::partnerNamesOf(const std::vector<uint32_t>& npcIds)
{
    QStringList names;
    tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance();
    for (const uint32_t id : npcIds) {
        const std::shared_ptr<tibia::creatures::TCreature> row = storage ? storage->creatureAt(id) : nullptr;
        names << (row ? row->name() : qtTrId("npc_dialog_talk_partner_name_fallback"));
    }
    if (names.size() <= 1)
        return names.isEmpty() ? qtTrId("npc_dialog_talk_partner_name_fallback") : names.front();
    const QString last = names.takeLast();
    return names.join(QStringLiteral(", ")) + qtTrId("connecting_and") + last;
}

void TNpcDialogController::onNpcChatWindow(const std::vector<uint32_t>& npcIds, std::vector<TNpcKeywordButtonModel::Button> buttons)
{
    if (m_remote)
        return;
    m_talkPartners = npcIds;
    m_keywordButtons.setItems(buttons);
    if (npcIds.empty()) {
        setShowTrade(false);
        dialog()->close();
        return;
    }
    if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
        if (const std::shared_ptr<tibia::creatures::TCreature> row = storage->creatureAt(npcIds.front()))
            m_outfit.setOutfit(row->outfit());
    }
    setTalkPartnerNames(partnerNamesOf(npcIds));
    postTalkingTo(m_talkPartnerNames);
    dialog()->show();
}

void TNpcDialogController::postTalkingTo(const QString& names)
{
    // CIP: a system line in the NPCs channel (opened if needed) names the talk partners.
    if (!m_chatStorage || names.isEmpty())
        return;
    if (chat::TChatChannel* channel = m_chatStorage->open({ chat::TChatChannelIdentifier::Npc }, false, false))
        channel->addEntry(chat::TClientChatEntryFactory::systemEntry(QColor(240, 240, 240), qtTrId("npc_dialog_talking_to").arg(names)));
}

TNpcDialogController::~TNpcDialogController()
{
    for (const int listener : m_actionListeners)
        input::removeActionListener(listener);
    resetConversation();
    qApp->removeEventFilter(this);
    if (m_chatControl)
        m_chatControl->setNpcDialog(nullptr);
}

void TNpcDialogController::closeButtonPressed()
{
    if (m_remote) {
        const std::function<void()> close = m_remote->close;
        closeRemoteSession();
        if (close)
            close();
        return;
    }
    m_trade.onRequestCloseNpcTrade();
    dialog()->close();
    resetConversation();
}

void TNpcDialogController::resetConversation()
{
    setShowTrade(false);
    m_outfit.setOutfit(Outfit());
    m_keywordButtons.setItems({});
}

void TNpcDialogController::onFocusChanged()
{
    if (!hasFocus()) {
        input::publish(std::make_shared<input::TGameActionSwitchChatMode>(m_savedChatMode));
    } else if (m_chatControl) {
        m_savedChatMode = m_chatControl->chatMode();
    }
}

bool TNpcDialogController::handleGameAction(const input::IGameAction& action)
{
    switch (action.gameActionType()) {
    case input::EGameAction::PRESS_ENTER_IN_CHAT:
        if (!hasFocus())
            return false;
        pressEnter();
        return true;
    case input::EGameAction::CHAT_CHANNEL_SELECT_ALL: {
        // The NPCs channel's select-all marks the conversation.
        const auto* channel = input::gameActionAs<input::TGameActionChatChannel>(&action);
        if (!dialog()->isShown() || !channel || channel->chatChannelIdentifier().type != chat::TChatChannelIdentifier::Npc || !m_chat)
            return false;
        m_chat->selectAll();
        return true;
    }
    case input::EGameAction::SET_CURRENT_CHAT_CHANNEL_INPUT_TEXT: {
        const auto* text = input::gameActionAs<input::TGameActionSetCurrentChatChannelInputText>(&action);
        if (!dialog()->isShown() || !text)
            return false;
        setInputText(text->text(), text->sendAutomatically(), text->isSpell(), text->spellCastOptionalTargetType());
        return true;
    }
    case input::EGameAction::REQUEST_SELECT_ALL_TEXT:
        if (!hasFocus())
            return false;
        selectAllText();
        return true;
    case input::EGameAction::REQUEST_COPY_SELECTED_TEXT_TO_CLIPBOARD:
        if (!hasFocus())
            return false;
        copySelectedText();
        return true;
    default:
        return false;
    }
}

void TNpcDialogController::selectAllText()
{
    QQuickItem* item = dialog()->item();
    if (!item)
        return;
    if (item->property("currentChatInput").toString().isEmpty()) {
        if (m_chat)
            m_chat->selectAll();
        return;
    }
    if (m_chat)
        m_chat->startNewSelection(0, 0);
    QMetaObject::invokeMethod(item, "selectAllChatInput");
}

void TNpcDialogController::copySelectedText()
{
    QQuickItem* item = dialog()->item();
    if (!item)
        return;
    const QString selected = item->property("currentChatInputSelectedText").toString();
    if (selected.isEmpty()) {
        if (m_chat && m_chat->hasSelection()) {
            input::publish(std::make_shared<input::TGameActionCopyTextToClipboard>(m_chat->selectedText()));
        }
        return;
    }
    input::publish(std::make_shared<input::TGameActionCopyTextToClipboard>(selected));
}

QString TNpcDialogController::sendChatMessage(const QString& Message, const bool EnforceWritesToLocalChat)
{
    // CIP : the chat controller's send with the conversation pane's channel.
    if (chat::TChatChannel* channel = m_chat ? m_chat->channel() : nullptr; m_chatControl && (channel || EnforceWritesToLocalChat))
        return m_chatControl->sendChatMessageTo(channel, Message, EnforceWritesToLocalChat);
    const QString text = Message.trimmed();
    if (text.isEmpty())
        return {};
    if (!sayToRemoteNpc(text))
        tibia::game::talkChannel(tibia::client::MessageNpcTo, 0, text.toStdString());
    // As TChatController::sayInChannel: the server does not echo NPC-channel lines.
    if (m_chatStorage)
        m_chatStorage->addNpcEntry(chat::consoleColor(12), text, m_chatStorage->ownName());
    return {};
}

QString TNpcDialogController::sendChatMessage(const QString& Message)
{
    return sendChatMessage(Message, false);
}

void TNpcDialogController::sendKeyword(const QString& Keyword)
{
    // CIP : sends and clears the input.
    sendChatMessage(Keyword);
    if (QQuickItem* item = dialog()->item())
        item->setProperty("currentChatInput", QString());
}

void TNpcDialogController::pressEnter()
{
    QQuickItem* item = dialog()->item();
    if (!item)
        return;
    if (m_chatControl)
        m_chatControl->onMessageSent();
    const QString rest = sendChatMessage(item->property("currentChatInput").toString(), false);
    item->setProperty("currentChatInput", rest);
}

void TNpcDialogController::setInputText(const QString& text, const bool send, const bool isSpell, int)
{
    QQuickItem* item = dialog()->item();
    if (!item)
        return;
    if (!send) {
        item->setProperty("currentChatInput", text);
        return;
    }
    sendChatMessage(text, isSpell);
    item->setProperty("currentChatInput", QString());
}

bool TNpcDialogController::sayToRemoteNpc(const QString& text)
{
    if (!m_remote)
        return false;
    if (m_remote->say)
        m_remote->say(text);
    return true;
}

void TNpcDialogController::onRequestOpenNPCTrade()
{
    if (!m_remote && m_talkPartners.empty()) {
        const QString names = m_storage ? m_storage->npcName() : QString();
        setTalkPartnerNames(names);
        m_outfit.setFromCreatureNamed(names);
        m_keywordButtons.setItems({});
    }
    setShowTrade(true);
    dialog()->show();
}

void TNpcDialogController::onRequestCloseNPCTrade()
{
    setShowTrade(false);
    if (!m_remote && m_talkPartners.empty())
        dialog()->close();
}

void TNpcDialogController::setChatController(TChatController* controller)
{
    if (m_chatControl || !controller)
        return;
    m_chatControl = controller;
    m_chatController = controller;
    controller->setNpcDialog(this);
    chat::TChatChannelStorage* storage = &controller->storage();
    m_chatStorage = storage;
    chat::TChatStorage* talk = &controller->chatStorage();
    connect(talk, &chat::TChatStorage::npcTalkPartnersChanged, this,
            [this, talk] { onNpcChatWindow(talk->npcTalkPartners(), talk->keywordButtons()); });
    connect(talk, &chat::TChatStorage::keywordButtonsChanged, this, [this, talk] { m_keywordButtons.setItems(talk->keywordButtons()); });
    m_chat = new TChatChannelController(this);
    m_chatChannelModel = QVariant::fromValue<QObject*>(m_chat);
    connect(storage, &chat::TChatChannelStorage::channelAdded, this, &TNpcDialogController::bindNpcChannel);
    connect(storage, &chat::TChatChannelStorage::channelsReset, this, &TNpcDialogController::bindNpcChannel);
    bindNpcChannel();
}

void TNpcDialogController::bindNpcChannel()
{
    if (m_chat && m_chatStorage)
        m_chat->setChannel(m_chatStorage->find({ chat::TChatChannelIdentifier::Npc }));
}

void TNpcDialogController::openRemoteSession(TRemoteNpcSession session)
{
    setTalkPartnerNames(session.name);
    m_outfit.setOutfit(session.outfit);
    m_keywordButtons.setItems(session.keywordButtons);
    m_remote = std::move(session);
    dialog()->show();
}

void TNpcDialogController::closeRemoteSession()
{
    if (!m_remote)
        return;
    m_remote.reset();
    m_keywordButtons.setItems({});
    if (m_showTrade)
        m_trade.onRequestCloseNpcTrade();
    if (dialog()->item())
        onRequestCloseNPCTrade();
}

void TNpcDialogController::setTalkPartnerNames(const QString& names)
{
    if (names == m_talkPartnerNames)
        return;
    m_talkPartnerNames = names;
    Q_EMIT talkPartnersChanged();
}

void TNpcDialogController::setShowTrade(const bool show)
{
    if (show == m_showTrade)
        return;
    m_showTrade = show;
    m_tradeFocus.setActive(show);
    Q_EMIT showTradeChanged();
}

void TNpcDialogController::onPositionChanged(const int x, const int y)
{
    // CIP: the client options keep the dialog's rectangle.
    config::TClientOptions* options = config::TClientOptions::instance();
    QRect rect = options->npcDialogOptions();
    rect.moveTo(x, y);
    options->setNpcDialogOptions(rect);
}

void TNpcDialogController::onDialogOptionsChanged()
{
    const QRect rect = config::TClientOptions::instance()->npcDialogOptions();
    if (rect == m_positionAndSize)
        return;
    m_positionAndSize = rect;
    Q_EMIT dialogOptionsChanged();
}

void TNpcDialogController::onSizeChanged(const int NewDialogWidth, const int NewContentHeight)
{
    if (!dialog()->item())
        return;
    config::TClientOptions* options = config::TClientOptions::instance();
    QRect rect = options->npcDialogOptions();
    rect.setSize(QSize(NewDialogWidth, NewContentHeight));
    options->setNpcDialogOptions(rect);
}

void TNpcDialogController::onTradeRequestTakeFocus()
{
    m_tradeFocus.takeFocus();
}

void TNpcDialogController::onTradeRequestReleaseFocus()
{
    m_tradeFocus.releaseFocus();
}

void TNpcDialogController::afterCreate()
{
    TDialogControllerWithFocusHandlingBase::afterCreate();
    // CIP: the trade pane is the trade focus's click area.
    if (QQuickItem* item = dialog()->item())
        m_tradeFocus.setClickArea(item->findChild<QQuickItem*>(QStringLiteral("npcTrade")));
}

void TNpcDialogController::beforeDestroy()
{
    TDialogControllerWithFocusHandlingBase::beforeDestroy();
    m_tradeFocus.setClickArea(nullptr);
}

void TNpcDialogController::beforeHide()
{
    m_tradeFocus.releaseFocus();
    m_tradeFocus.setActive(false);
    TDialogControllerWithFocusHandlingBase::beforeHide();
}

void TNpcDialogController::afterShow()
{
    // product remote sessions: Escape closes (eventFilter).
    qApp->installEventFilter(this);
    if (QQuickItem* item = dialog()->item()) {
        // product: ChatFab owns chat on/off — hide CIP mode button in the NPC input.
        for (QQuickItem* child : item->findChildren<QQuickItem*>(Qt::FindChildrenRecursively)) {
            if (child->property("showChatModeButton").isValid())
                child->setProperty("showChatModeButton", false);
        }
    }
    m_tradeFocus.setActive(m_showTrade);
    Q_EMIT shownChanged(true);
}

void TNpcDialogController::afterHide()
{
    qApp->removeEventFilter(this);
    Q_EMIT shownChanged(false);
}

bool TNpcDialogController::eventFilter(QObject* watched, QEvent* event)
{
    // product remote sessions: ESC closes (CIP in-world NPC keeps Escape for game hotkeys;
    // NpcDialog.qml's onCancelPressedFunction is never reached for that reason).
    if (m_remote && dialog()->isShown() && event->type() == QEvent::KeyPress) {
        const auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Escape && !key->isAutoRepeat()) {
            closeButtonPressed();
            return true;
        }
    }
    return TDialogControllerWithFocusHandlingBase::eventFilter(watched, event);
}

}
