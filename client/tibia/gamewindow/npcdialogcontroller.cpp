#include "npcdialogcontroller.h"
#include "game/gamesessionbase.h"
#include "colorhelper.h"

#include "chat/chatgameactionhandler.h"
#include "input/gameactionhandlers.h"
#include "config/clientoptions.h"

#include "trade/tradeciptypes.h"

#include "iclockservice.h"

#include "chat/chatchannel.h"
#include "gamewindow/chatcontroller.h"

#include <creatures/creaturestorage.h>
#include <client/outfit.h>

#include <QGuiApplication>
#include <QMouseEvent>
#include <QQuickItem>
#include <QQuickWindow>
#include <tibia/game/gamenotify.h>
#include <client/staticdata.h>
#include "game/currentgamesession.h"
#include "game/gamesessionbase.h"
#include "game/mapsession.h"
#include "protocol/clientmessages.h"
#include <tibia/game/tgamenotify.h>

namespace tibia::gamewindow {

namespace {

using ::tibia::shared::outfitColor;

}

void TNpcDialogOutfit::setFromCreatureNamed(const tibia::game::TGameSessionBase* session, const QString& name)
{
    tibia::creatures::TCreatureStorage* storage = session ? session->creatureStorage().get() : nullptr;
    if (session && session->hasLocalPlayer() && storage && !name.isEmpty()) {
        const auto ar = tibia::game::awareRange();
    const AwareRange range{ ar.left, ar.top, ar.right, ar.bottom };
        for (const std::shared_ptr<tibia::creatures::TCreature>& creature :
             storage->spectatorsNear(session->localPlayerPosition(), false, range.left, range.right, range.top,
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

TNpcDialogController::TNpcDialogController(config::TClientOptions* ClientOptions,
                                           std::shared_ptr<input::TGameActionMapper> GameActionMapper,
                                           TChatController* ChatController, input::TEventFilterManager* EventFilterManager,
                                           QQmlEngine* Engine, QQuickItem* DialogParentItem, QObject* parent)
    : TDialogControllerWithFocusHandlingBase(QStringLiteral("NpcDialog.qml"), Engine, DialogParentItem, parent)
    , m_trade(DialogParentItem, Engine)
    , m_chat(std::shared_ptr<config::TClientOptions>(std::shared_ptr<void>(), ClientOptions), GameActionMapper, 2, this)
    , m_filter(&m_chat, this)
    , m_chatControl(ChatController)
    , m_clientOptions(ClientOptions)
    , m_pGameActionMapper(std::move(GameActionMapper))
    , m_pEventFilterManager(EventFilterManager)
{
    connect(&m_chat, &TChatChannelController::publishGameAction, this, &TNpcDialogController::publishGameAction);
    m_chatChannelModel = QVariant::fromValue<QObject*>(&m_filter);
    m_tradeController = &m_trade;
    m_npcOutfit = &m_outfit;
    m_keywordButtonsModel = &m_keywordButtons;
    // CIP: the chat controller knows the dialog (its chat input yields to the dialog's).
    m_chatController = ChatController;
    if (ChatController)
        ChatController->setNpcDialog(this);
    m_positionAndSize = QRect(-1, -1, 0, 0);
    connect(m_clientOptions, &config::TClientOptions::npcDialogOptionsChanged, this, &TNpcDialogController::onNpcDialogOptionsChanged);
    if (m_clientOptions->npcDialogOptions().isValid())
        onNpcDialogOptionsChanged();
    // CIP / : while the trade has the keyboard the dialog's chat input lets go of it.
    m_tradeFocus.setActive(false);
    connect(&m_tradeFocus, &TUiFocusManager::focusChanged, this, [this] {
        setFocusAllowed(!m_tradeFocus.hasFocus());
        Q_EMIT tradeFocusChanged();
    });
    connect(this, &cip::tibia::gamewindow::TDialogControllerWithFocusHandlingBase::focusChanged, this,
            &TNpcDialogController::onFocusChanged);
    for (const input::EGameAction type : { input::EGameAction::CHAT_CHANNEL_SELECT_ALL, input::EGameAction::REQUEST_SELECT_ALL_TEXT,
                                           input::EGameAction::REQUEST_COPY_SELECTED_TEXT_TO_CLIPBOARD })
        m_gameActions.on(type, [this](const input::IGameAction& action) { handleGameAction(action); });
}

void TNpcDialogController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    dialog()->close();
    for (const QMetaObject::Connection& connection : m_gameSessionConnections)
        disconnect(connection);
    m_gameSessionConnections.clear();
    m_talkPartners.clear();
    m_gameSession = GameSession;
    if (!m_gameSession)
        m_chat.resetChannel();
    m_trade.connectToGameSession(m_gameSession);
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    m_storage = session ? session->npcTradeStorage().get() : nullptr;
    m_chatStorage = session ? session->chatChannelStorage().get() : nullptr;
    if (!session)
        return;
    chat::TChatStorage* talk = session->chatStorage().get();
    m_gameSessionConnections.push_back(connect(talk, &chat::TChatStorage::npcTalkPartnersChanged, this,
                                               [this, talk] { onNpcChatWindow(talk->npcTalkPartners(), talk->keywordButtons()); }));
    m_gameSessionConnections.push_back(connect(talk, &chat::TChatStorage::keywordButtonsChanged, this,
                                               [this, talk] { m_keywordButtons.setItems(talk->keywordButtons()); }));
    chat::TChatChannelStorage* channels = session->chatChannelStorage().get();
    m_gameSessionConnections.push_back(connect(channels, &chat::TChatChannelStorage::channelAdded, this, &TNpcDialogController::bindNpcChannel));
    trade::TNPCTradeProtocolMessageHandler* npcTrade = session->npcTradeProtocolMessageHandler().get();
    m_gameSessionConnections.push_back(
        connect(npcTrade, &trade::TNPCTradeProtocolMessageHandler::requestOpenNpcTrade, this, &TNpcDialogController::onRequestOpenNPCTrade));
    m_gameSessionConnections.push_back(
        connect(npcTrade, &trade::TNPCTradeProtocolMessageHandler::requestCloseNpcTrade, this, &TNpcDialogController::onRequestCloseNPCTrade));
    bindNpcChannel();
}

tibia::creatures::TCreatureStorage* TNpcDialogController::creatureStorage() const
{
    const auto* session = dynamic_cast<const tibia::game::TGameSessionBase*>(m_gameSession.get());
    return session ? session->creatureStorage().get() : nullptr;
}

QString TNpcDialogController::partnerNamesOf(const std::vector<uint32_t>& npcIds) const
{
    QStringList names;
    tibia::creatures::TCreatureStorage* storage = creatureStorage();
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
    if (m_external)
        return;
    m_talkPartners = npcIds;
    m_keywordButtons.setItems(buttons);
    if (npcIds.empty()) {
        closeButtonPressed();
        return;
    }
    if (tibia::creatures::TCreatureStorage* storage = creatureStorage()) {
        if (const std::shared_ptr<tibia::creatures::TCreature> row = storage->creatureAt(npcIds.front()))
            m_outfit.setOutfit(row->outfit());
    }
    setTalkPartnerNames(partnerNamesOf(npcIds));
    postTalkingTo(m_talkPartnerNames);
    showDialog();
}

void TNpcDialogController::showDialog()
{
    if (dialog()->item())
        return;
    if (m_gameSession) {
        if (const std::shared_ptr<shared::IClockService> clock = m_gameSession->clockService())
            m_filter.setMinimumTime(clock->currentDateTime().addMSecs(-500));
    }
    dialog()->create();
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
    resetConversation();
    if (m_chatControl)
        m_chatControl->setNpcDialog(nullptr);
}

void TNpcDialogController::closeButtonPressed()
{
    if (m_external) {
        const std::function<void()> close = m_external->close;
        closeExternalConversation();
        if (close)
            close();
        return;
    }
    dialog()->close();
    if (const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get()); session && session->chatGameActionHandler())
        session->chatGameActionHandler()->emitSendCloseNPCChannel();
    m_trade.onRequestCloseNpcTrade();
    resetConversation();
}

void TNpcDialogController::resetConversation()
{
    setShowTrade(false);
    m_outfit.setOutfit(Outfit());
    m_filter.clearMinimumTime();
    m_keywordButtons.setItems({});
}

void TNpcDialogController::onFocusChanged()
{
    if (!hasFocus()) {
        Q_EMIT publishGameAction(std::make_shared<input::TGameActionSwitchChatMode>(m_savedChatMode));
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
        if (!dialog()->isShown() || !channel)
            return false;
        selectAllInChannelOfType(static_cast<int>(channel->chatChannelIdentifier().type));
        return channel->chatChannelIdentifier().type == chat::TChatChannelIdentifier::Npc;
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
        m_chat.selectAllSince(m_filter.minimumTime());
        return;
    }
    m_chat.startNewSelection(0, 0);
    QMetaObject::invokeMethod(item, "selectAllChatInput");
}

void TNpcDialogController::selectAllInChannelOfType(const int channelType)
{
    if (channelType == chat::TChatChannelIdentifier::Npc)
        m_chat.selectAllSince(m_filter.minimumTime());
}

int TNpcDialogController::chatMode() const
{
    return m_chatControl ? m_chatControl->chatMode() : 0;
}

void TNpcDialogController::copySelectedText()
{
    QQuickItem* item = dialog()->item();
    if (!item)
        return;
    const QString selected = item->property("currentChatInputSelectedText").toString();
    if (selected.isEmpty()) {
        if (m_chat.hasSelection()) {
            Q_EMIT publishGameAction(std::make_shared<input::TGameActionCopyTextToClipboard>(m_chat.selectedText()));
        }
        return;
    }
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionCopyTextToClipboard>(selected));
}

QString TNpcDialogController::sendChatMessage(const QString& Message, const bool EnforceWritesToLocalChat)
{
    // CIP : the chat controller's send with the conversation pane's channel.
    if (chat::TChatChannel* channel = m_chat.channel(); m_chatControl && (channel || EnforceWritesToLocalChat))
        return m_chatControl->sendChatMessageTo(channel, Message, EnforceWritesToLocalChat);
    const QString text = Message.trimmed();
    if (text.isEmpty())
        return {};
    if (!sayToExternalNpc(text))
        if (const auto* session = dynamic_cast<const tibia::game::TGameSessionBase*>(m_gameSession.get()); session)
            tibia::protocol::gameclient::sendTalk(12, 0, "", text.toStdString()); // CIP chat message mode NpcTo
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

bool TNpcDialogController::sayToExternalNpc(const QString& text)
{
    if (!m_external)
        return false;
    if (m_external->say)
        m_external->say(text);
    return true;
}

void TNpcDialogController::onRequestOpenNPCTrade()
{
    if (!m_external && m_talkPartners.empty()) {
        const QString names = m_storage ? m_storage->npcName() : QString();
        setTalkPartnerNames(names);
        m_outfit.setFromCreatureNamed(dynamic_cast<const tibia::game::TGameSessionBase*>(m_gameSession.get()), names);
        m_keywordButtons.setItems({});
    }
    setShowTrade(true);
    dialog()->show();
}

void TNpcDialogController::onRequestCloseNPCTrade()
{
    setShowTrade(false);
    if (!m_external && m_talkPartners.empty())
        dialog()->close();
}

void TNpcDialogController::bindNpcChannel()
{
    if (m_chatStorage)
        m_chat.setChannel(m_chatStorage->find({ chat::TChatChannelIdentifier::Npc }));
}

void TNpcDialogController::openExternalConversation(TExternalNpcConversation conversation)
{
    setTalkPartnerNames(conversation.name);
    m_outfit.setOutfit(conversation.outfit);
    m_keywordButtons.setItems(conversation.keywordButtons);
    m_external = std::move(conversation);
    dialog()->show();
}

void TNpcDialogController::closeExternalConversation()
{
    if (!m_external)
        return;
    m_external.reset();
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
    config::TClientOptions* options = m_clientOptions;
    QRect rect = options->npcDialogOptions();
    rect.moveTo(x, y);
    options->setNpcDialogOptions(rect);
}

void TNpcDialogController::onNpcDialogOptionsChanged()
{
    const QRect rect = m_clientOptions->npcDialogOptions();
    if (rect == m_positionAndSize)
        return;
    m_positionAndSize = rect;
    Q_EMIT dialogOptionsChanged();
}

void TNpcDialogController::onSizeChanged(const int NewDialogWidth, const int NewContentHeight)
{
    if (!dialog()->item())
        return;
    config::TClientOptions* options = m_clientOptions;
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
    m_tradeFocus.setActive(m_showTrade);
    Q_EMIT shownChanged(true);
}

void TNpcDialogController::afterHide()
{
    Q_EMIT shownChanged(false);
}

}
