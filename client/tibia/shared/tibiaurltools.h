#pragma once

#include "shared/cip_typed.h"

#include <QString>

namespace tibia::shared {

/// CIP shared::TTibiaUrlTools (vtable 0x10267ace0): the chat message text processor. Every talk and text message
/// passes through process() (TChatProtocolMessageHandler +0xa8, FUN_100996120 / FUN_100997530) so dotted
/// host names cannot carry look-alike characters.
class TTibiaUrlTools : public ::tibia::CipTyped
{
public:
    /// CIP 0x10102ad10: whether `text` holds a character outside the URL alphabet.
    static bool hasNonUrlCharacters(const QString& text);
    /// CIP 0x10102ad70: whether `text` holds something dotted like a URL (weights such as "1.50 oz" excluded).
    static bool containsUrl(const QString& text);
    /// CIP 0x10102afd0: replaces every character outside the URL alphabet with '?'.
    static void sanitize(QString& text);
    /// CIP 0x10102b070: sanitizes every dotted word sequence when `text` contains a URL.
    static QString process(const QString& text);
};

} // namespace tibia::shared
