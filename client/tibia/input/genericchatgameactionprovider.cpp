#include "input/genericchatgameactionprovider.h"

#include "chat/chatchannel.h"
#include "chat/chatprotocolmessagehandler.h"
#include "creatures/vipstorage.h"

#include <memory>

namespace tibia::input {

TGenericChatGameActionProvider::TGenericChatGameActionProvider(const chat::TChatChannelStorage* chatChannelStorage,
                                                               const creatures::TVipStorage* vipStorage)
    : m_chatChannelStorage(chatChannelStorage)
    , m_vipStorage(vipStorage)
{
}

void TGenericChatGameActionProvider::setWhiteBlacklist(const chat::TWhiteBlacklist* whiteBlacklist)
{
    m_whiteBlacklist = whiteBlacklist;
}

void TGenericChatGameActionProvider::appendGameActions(TGameActionList& actions, const QString& name) const
{
    actions.push_back(std::make_shared<TGameActionOpenMessageToChatChannel>(name));
    if (const chat::TChatChannel* ownChannel = m_chatChannelStorage ? m_chatChannelStorage->ownChannel() : nullptr)
        actions.push_back(std::make_shared<TGameActionInvitePlayerToPrivateChatChannel>(ownChannel->identifier().id, name));
    actions.push_back(std::make_shared<TGameActionReportCharacter>(EGameAction::REPORT_NAME, name));
    if (m_whiteBlacklist) {
        if (m_whiteBlacklist->isOnBlacklist(name))
            actions.push_back(std::make_shared<TGameActionUnignoreName>(name));
        else
            actions.push_back(std::make_shared<TGameActionIgnoreName>(name));
    }
    if (!m_vipStorage || !m_vipStorage->vipByName(name))
        actions.push_back(std::make_shared<TGameActionAddNewVip>(name));
}

}
