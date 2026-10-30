#pragma once

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QColor>
#include <QObject>
#include <QString>

namespace tibia::chat {

struct TChatEntry;

/// CIP chat::TChatGameActionHandler — game-action → console / protocol bridge.
/// Live dispatch is TChatController::handleAction (FUN_100ba1070); this type is
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
/// for a channel (vtable 0x100b02320). Menu order lives in TChatController;
/// constructing this at menu build sites matches CIP ownership.
class TChatGameActionProvider final : public ::tibia::CipTyped
{
public:
    TChatGameActionProvider() = default;
};

/// CIP chat::TClientChatEntryFactory — builds TChatEntry values
/// (FUN_100b087c0 system / speaker / loot factories). Live path: TChatChannelStorage::add*.
class TClientChatEntryFactory final : public ::tibia::CipTyped
{
public:
    TClientChatEntryFactory() = default;

    /// FUN_100b087c0 / FUN_1009915d0: system line.
    static TChatEntry systemEntry(const QString& text, const QColor& color);
};

} // namespace tibia::chat
