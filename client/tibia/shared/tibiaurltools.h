#pragma once

#include "shared/cip_typed.h"

#include <QString>

namespace tibia {
namespace shared {

/// CIP shared::TTibiaUrlTools (vtable): the chat message text processor. Every talk and text message
/// passes through process (TChatProtocolMessageHandler +0xa8,) so dotted
/// host names cannot carry look-alike characters.
class TTibiaUrlTools : public ::tibia::CipTyped
{
public:
    /// CIP : whether `text` holds a character outside the URL alphabet.
    static bool hasNonUrlCharacters(const QString& text);
    /// CIP : whether `text` holds something dotted like a URL (weights such as "1.50 oz" excluded).
    static bool containsUrl(const QString& text);
    /// CIP : replaces every character outside the URL alphabet with '?'.
    static void sanitize(QString& text);
    /// CIP : sanitizes every dotted word sequence when `text` contains a URL.
    static QString process(const QString& text);
};

}
}
