#include "npcdialogcontroller.h"

#include "chat/chatchannel.h"
#include "gamewindow/chatcontroller.h"

#include <tibia/world/creature.h>
#include <tibia/world/game.h>
#include <tibia/world/localplayer.h>
#include <tibia/world/map.h>
#include <client/outfit.h>

#include <QGuiApplication>
#include <QMouseEvent>
#include <QQuickItem>
#include <QQuickWindow>

namespace tibia::gamewindow {

namespace {

QColor outfitColor(const int index)
{
    const Color c = Outfit::getColor(index);
    return QColor(c.r(), c.g(), c.b());
}

}

void TNpcDialogOutfit::setFromCreatureNamed(const QString& name)
{
    const LocalPlayerPtr& player = g_game.getLocalPlayer();
    if (player && !name.isEmpty()) {
        const std::string wanted = name.toStdString();
        for (const CreaturePtr& creature : g_map.getSpectators(player->getPosition(), false)) {
            if (creature && creature->getName() == wanted) {
                setOutfit(creature->getOutfit());
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
    : TDialogController(QStringLiteral("NpcDialog.qml"), parent)
    , m_storage(storage)
    , m_trade(storage)
{
    m_tradeController = &m_trade;
    m_npcOutfit = &m_outfit;
    m_keywordButtonsModel = &m_keywordButtons;
    m_positionAndSize = QRect(-1, -1, 0, 0);
    connect(storage, &trade::TNPCTradeStorage::requestOpenNpcTrade, this, &TNpcDialogController::onRequestOpenNPCTrade);
    connect(storage, &trade::TNPCTradeStorage::requestCloseNpcTrade, this, &TNpcDialogController::onRequestCloseNPCTrade);
    // CIP 0x100e3e550 / 0x100e47c80: while the trade has the keyboard the dialog's chat input lets go of it.
    m_tradeFocus.setActive(false);
    connect(&m_tradeFocus, &TUiFocusManager::focusChanged, this, [this] {
        m_tradeHasFocus = m_tradeFocus.hasFocus();
        if (TDialogStackController* stack = TDialogStackController::instance()) {
            if (m_tradeHasFocus)
                stack->dialogReleasedFocus(dialog());
            else if (m_hasFocus)
                stack->dialogTookFocus(dialog());
        }
        Q_EMIT tradeFocusChanged();
    });

    using Args = std::vector<std::any>;
    m_hooks.emplace_back("g_game", "onNpcChatWindow", [this](LuaObject*, const Args& args) {
        const auto* data = args.empty() ? nullptr : std::any_cast<NpcChatWindowData>(&args[0]);
        if (!data)
            return std::any{};
        std::vector<TNpcKeywordButtonModel::Button> buttons;
        buttons.reserve(data->buttons.size());
        for (const NpcButton& button : data->buttons)
            buttons.push_back({ button.id, QString::fromStdString(button.text) });
        onNpcChatWindow(data->npcIds, std::move(buttons));
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onNpcChatWindowClose", [this](LuaObject*, const Args&) {
        onNpcChatWindow({}, {});
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onGameEnd", [this](LuaObject*, const Args&) {
        m_talkPartners.clear();
        return std::any{};
    });
}

QString TNpcDialogController::partnerNamesOf(const std::vector<uint32_t>& npcIds)
{
    QStringList names;
    for (const uint32_t id : npcIds) {
        const CreaturePtr creature = g_map.getCreatureById(id);
        names << (creature ? QString::fromStdString(creature->getName()) : qtTrId("npc_dialog_talk_partner_name_fallback"));
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
    m_keywordButtons.setButtons(std::move(buttons));
    if (npcIds.empty()) {
        setShowTrade(false);
        dialog()->close();
        return;
    }
    if (const CreaturePtr creature = g_map.getCreatureById(npcIds.front()))
        m_outfit.setOutfit(creature->getOutfit());
    setTalkPartnerNames(partnerNamesOf(npcIds));
    dialog()->show();
}

TNpcDialogController::~TNpcDialogController()
{
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
}

QString TNpcDialogController::sendChatMessage(const QString& Message, const bool EnforceWritesToLocalChat)
{
    // CIP 0x100e40830: the chat controller's send with the conversation pane's channel.
    if (chat::TChatChannel* channel = m_chat ? m_chat->channel() : nullptr; m_chatControl && (channel || EnforceWritesToLocalChat))
        return m_chatControl->sendChatMessageTo(channel, Message, EnforceWritesToLocalChat);
    const QString text = Message.trimmed();
    if (text.isEmpty())
        return {};
    if (!sayToRemoteNpc(text))
        g_game.talkChannel(Otc::MessageNpcTo, 0, text.toStdString());
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
    // CIP 0x100e41660: sends and clears the input.
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
        m_keywordButtons.setButtons({});
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
    m_keywordButtons.setButtons(session.keywordButtons);
    m_remote = std::move(session);
    dialog()->show();
}

void TNpcDialogController::closeRemoteSession()
{
    if (!m_remote)
        return;
    m_remote.reset();
    m_keywordButtons.setButtons({});
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
    m_positionAndSize.moveTo(x, y);
}

void TNpcDialogController::onSizeChanged(const int NewDialogWidth, const int NewContentHeight)
{
    m_positionAndSize.setSize(QSize(NewDialogWidth, NewContentHeight));
}

void TNpcDialogController::onTradeRequestTakeFocus()
{
    m_tradeFocus.takeFocus();
}

void TNpcDialogController::onTradeRequestReleaseFocus()
{
    m_tradeFocus.releaseFocus();
}

void TNpcDialogController::takeFocus()
{
    if (m_hasFocus)
        return;
    m_hasFocus = true;
    if (TDialogStackController* stack = TDialogStackController::instance())
        stack->dialogTookFocus(dialog());
    Q_EMIT focusChanged();
}

void TNpcDialogController::releaseFocus()
{
    if (!m_hasFocus)
        return;
    m_hasFocus = false;
    if (TDialogStackController* stack = TDialogStackController::instance())
        stack->dialogReleasedFocus(dialog());
    Q_EMIT focusChanged();
}

void TNpcDialogController::afterShow()
{
    qApp->installEventFilter(this);
    takeFocus();
    // CIP 0x100e41580 / 0x100e41630: the trade pane is the trade focus's click area.
    if (QQuickItem* item = dialog()->item()) {
        m_tradeFocus.setClickArea(item->findChild<QQuickItem*>(QStringLiteral("npcTrade")));
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
    // CIP 0x100e41600.
    m_tradeFocus.setActive(false);
    releaseFocus();
    Q_EMIT shownChanged(false);
}

bool TNpcDialogController::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress && qobject_cast<QQuickWindow*>(watched)) {
        if (QQuickItem* item = dialog()->item()) {
            const auto* press = static_cast<QMouseEvent*>(event);
            if (item->contains(item->mapFromScene(press->scenePosition())))
                takeFocus();
            else
                releaseFocus();
        }
    }
    return TDialogController::eventFilter(watched, event);
}

}
