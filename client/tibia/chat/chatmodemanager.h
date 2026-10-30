#pragma once

#include <QObject>

namespace tibia::chat {

/// Whether typing goes to the chat or to the hotkeys (CIP: TChatModeManager,
/// vtable slots 12-19). The mode starts from chatOptions.chatModeOn.
class TChatModeManager final : public QObject
{
    Q_OBJECT
public:
    /// tibia::chat::EChatMode.
    enum EChatMode : int
    {
        CHAT_OFF = 0,
        CHAT_ON = 1,
        CHAT_TEMPORARY_ON = 2,
    };

    explicit TChatModeManager(QObject* parent = nullptr);

    /// v12.
    int chatMode() const { return m_mode; }
    /// v13.
    void setChatMode(int mode);
    /// v14: the Chat On/Off button.
    void onChatModeButtonClicked();
    /// v15: a sent message ends a temporary on.
    void onMessageSent();
    /// v16: clicking the input turns the chat on.
    void onChatInputClicked();
    /// v17 (SWITCH_CHAT_MODE, v18): temporary on only from off.
    void switchChatMode(int mode);
    /// v19.
    void onGameSessionDisconnected();

Q_SIGNALS:
    void chatModeChanged();

private:
    int m_mode = CHAT_OFF;
};

}
