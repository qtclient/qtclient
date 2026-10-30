#include "chatmodemanager.h"

#include "app/clientoptions.h"

#include <QJsonObject>

namespace tibia::chat {

TChatModeManager::TChatModeManager(QObject* parent)
    : QObject(parent)
{
    const QJsonObject options = app::ClientOptions::load().root().value(QLatin1String("chatOptions")).toObject();
    m_mode = options.value(QLatin1String("chatModeOn")).toBool(false) ? CHAT_ON : CHAT_OFF;
}

void TChatModeManager::setChatMode(const int mode)
{
    if (m_mode == mode)
        return;
    const bool wasOn = m_mode == CHAT_ON;
    m_mode = mode;
    if (wasOn != (mode == CHAT_ON)) {
        QJsonObject options = app::ClientOptions::load().root().value(QLatin1String("chatOptions")).toObject();
        options.insert(QLatin1String("chatModeOn"), mode == CHAT_ON);
        app::ClientOptions::storeAll({ { QStringLiteral("chatOptions"), options } });
    }
    Q_EMIT chatModeChanged();
}

void TChatModeManager::onChatModeButtonClicked()
{
    setChatMode(m_mode != CHAT_OFF ? CHAT_OFF : CHAT_ON);
}

void TChatModeManager::onMessageSent()
{
    if (m_mode == CHAT_TEMPORARY_ON)
        setChatMode(CHAT_OFF);
}

void TChatModeManager::onChatInputClicked()
{
    if (m_mode == CHAT_OFF)
        setChatMode(CHAT_ON);
}

void TChatModeManager::switchChatMode(const int mode)
{
    switch (mode) {
    case CHAT_TEMPORARY_ON:
        if (m_mode == CHAT_OFF)
            setChatMode(CHAT_TEMPORARY_ON);
        break;
    case CHAT_ON:
        setChatMode(CHAT_ON);
        break;
    case CHAT_OFF:
        setChatMode(CHAT_OFF);
        break;
    default:
        break;
    }
}

void TChatModeManager::onGameSessionDisconnected()
{
    if (m_mode == CHAT_TEMPORARY_ON)
        setChatMode(CHAT_OFF);
}

}
