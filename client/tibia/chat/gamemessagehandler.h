#pragma once

#include "shared/cip_typed.h"

#include "client/gamecore.h"

#include <client/declarations.h>
#include <client/position.h>

#include <QColor>
#include <QJsonObject>
#include <QString>

#include <vector>

namespace tibia::worldmap {
class TNumericalEffectStorage;
}

namespace tibia::chat {

class TTextStorage;
class TWhiteBlacklist;

/// Puts the server's talk and text messages on the map (CIP: the game message
/// handler, FUN_100996120 for talk, FUN_100997530 for text messages). The
/// console side follows with the chat window.
class TGameMessageHandler : public ::tibia::CipTyped
{
public:
    TGameMessageHandler(TTextStorage& texts, worldmap::TNumericalEffectStorage& effects);
    ~TGameMessageHandler();
    TGameMessageHandler(const TGameMessageHandler&) = delete;
    TGameMessageHandler& operator=(const TGameMessageHandler&) = delete;

    /// CIP handler +0x98: the same ignore list the console uses.
    void setIgnoreList(TWhiteBlacklist* list) { m_ignoreList = list; }

    /// Display options, by CIP option name (CIP: FUN_100995c70).
    void setOptions(const QJsonObject& options);

    /// CIP's overlay colour of a protocol message mode (FUN_100991040).
    static QColor overlayColor(int mode);

private:
    void onTalk(const QString& name, int mode, const QString& text, const Position& coordinate);
    void onTextMessage(int mode, const QString& text);
    void onNumericalEffect(const Position& coordinate, qint64 value, int color);
    void addSpeech(const QString& name, int mode, const QString& text, const Position& coordinate);

    TTextStorage& m_texts;
    worldmap::TNumericalEffectStorage& m_effects;
    TWhiteBlacklist* m_ignoreList = nullptr;
    std::vector<client::gamecore::Hook> m_hooks;

    bool m_showTextualEffects = true;
    bool m_showMessages = true;
    bool m_showPrivateMessages = true;
    bool m_showBoostedCreatureMessages = true;
    bool m_showOfflineTrainingMessages = true;
    bool m_showStoreMessages = true;
    bool m_showPotionMessages = true;
    bool m_showHotkeyUsageMessages = true;
    bool m_showLootMessages = true;
    bool m_showOwnSpells = true;
    bool m_showOthersSpells = true;
};

}
