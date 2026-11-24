#pragma once


#include <QString>

namespace tibia {
namespace shared {

/// CIP `shared::TTibiaUrlTools`: the chat message text processor. Every talk
/// and text message passes through process, so dotted host names cannot carry
/// look-alike characters.
class TTibiaUrlTools
{
public:
    /// Whether `text` holds a character outside the URL alphabet.
    static bool hasNonUrlCharacters(const QString& text);
    /// Whether `text` holds something dotted like a URL (weights such as "1.50 oz" excluded).
    static bool containsUrl(const QString& text);
    /// Replaces every character outside the URL alphabet with '?'.
    static void sanitize(QString& text);
    /// Sanitizes every dotted word sequence when `text` contains a URL.
    static QString process(const QString& text);
};

}
}
