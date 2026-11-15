#pragma once

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QColor>
#include <QObject>
#include <QString>

namespace tibia {
namespace chat {

struct TChatEntry;

/// CIP chat::TChatGameActionHandler — game-action → console / protocol bridge.
/// Live dispatch is TChatController::handleAction ; this type is
/// the CIP-named handler owned beside the controller for the same lifetime.
class TChatGameActionHandler final : public cip::tibia::chat::TChatGameActionHandler
{
    Q_OBJECT
public:
    using cip::tibia::chat::TChatGameActionHandler::TChatGameActionHandler;
};

/// CIP chat::TChatStorage — keywordButtons / npcTalkPartners signals (contract).
/// NPC keyword UI lives on TNpcDialogController; this is the CIP storage
/// surface owned by TChatController for that session lifetime.
class TChatStorage final : public cip::tibia::chat::TChatStorage
{
    Q_OBJECT
public:
    using cip::tibia::chat::TChatStorage::TChatStorage;
};

/// CIP chat::TChatGameActionProvider — context-menu / hotkey action candidates
/// for a channel (vtable). Menu order lives in TChatController;
/// constructing this at menu build sites matches CIP ownership.
class TChatGameActionProvider final : public ::tibia::CipTyped
{
public:
    TChatGameActionProvider() = default;
};

/// Builds chat entries (CIP: chat::TClientChatEntryFactory). Every entry
/// carries the time it was made.
class TClientChatEntryFactory final
{
public:
    /// A system line.
    static TChatEntry systemEntry(const QColor& color, const QString& text);
    /// A speaker without level (an NPC).
    static TChatEntry speakerEntry(const QColor& color, const QString& name, const QString& text);
    /// A player with level.
    static TChatEntry speakerWithLevelEntry(const QColor& color, const QString& name, const QString& text, int level,
                                            bool traded, uint32_t statementId);
    /// A loot line.
    static TChatEntry lootEntry(const QColor& color, const QString& text);
    /// v2: "the channel was closed", red.
    static TChatEntry channelClosedEntry();
    /// v3: the Team Finder channel's closing line, red.
    static TChatEntry teamFinderChannelClosedEntry();
    /// v4: the help channel's introduction, white.
    static TChatEntry helpChannelIntroductionEntry();
    /// v5: the advertising channels' introduction, white.
    static TChatEntry advertisingChannelIntroductionEntry();
};

}
}
