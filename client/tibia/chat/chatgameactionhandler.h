#pragma once

#include "input/gameaction.h"
#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QColor>
#include <QObject>
#include <QString>

#include <functional>

namespace tibia {
namespace chat {

struct TChatEntry;
struct TChatChannelIdentifier;
class TChatChannelStorage;

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

/// The chat's game action candidates (CIP: chat::TChatGameActionProvider):
/// v2 for a channel tab (or the empty tab bar), v3 for an entry of a channel.
class TChatGameActionProvider final : public ::tibia::CipTyped
{
public:
    /// The name actions of a player entry (CIP +0x18, the generic chat
    /// provider): message, invite, VIP, ignore and report name.
    using NameActions = std::function<void(::tibia::input::TGameActionList& actions, const QString& name)>;
    /// The console option that shows join / leave messages (CIP +0x28, option 0x1b).
    using ShowJoinLeaveMessages = std::function<bool()>;

    TChatGameActionProvider(TChatChannelStorage& storage, NameActions nameActions, ShowJoinLeaveMessages showJoinLeaveMessages);

    /// v2: the tab's actions; without a channel, every open channel to pick as
    /// the secondary one, by name.
    ::tibia::input::TGameActionList gameActions(const TChatChannelIdentifier* identifier) const;
    /// v3: an entry's actions. `entryText` is the line as the pane shows it,
    /// `selectedText` the selection, `secondaryShown` whether the entry is in
    /// the secondary pane.
    ::tibia::input::TGameActionList entryGameActions(const QString& entryText, const TChatEntry& entry, const QString& selectedText,
                                                     const QString& ownName, bool secondaryShown) const;

    /// The channel a TGameActionChatChannel names.
    static TChatChannelIdentifier identifierOf(const ::tibia::input::TGameAction& action);

private:
    TChatChannelStorage& m_storage;
    NameActions m_nameActions;
    ShowJoinLeaveMessages m_showJoinLeaveMessages;
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
