#pragma once

#include "tibiaurltools.h"

#include "clipboardhelper.h"

#include <QString>

#include <memory>

namespace tibia {
namespace shared {

/// CIP `shared::TTibiaClipboardHelper`: the game client's clipboard writer. Text
/// copied from the game goes through the URL tools first, so dotted host names
/// in it cannot carry look-alike characters.
class TTibiaClipboardHelper
{
public:
    TTibiaClipboardHelper(std::shared_ptr<TClipboardHelper> clipboardHelper, std::shared_ptr<TTibiaUrlTools> urlTools);
    virtual ~TTibiaClipboardHelper() = default;

    /// Puts the processed `text` into the clipboard.
    virtual void setTextInClipboard(const QString& text);
    /// Puts `text` into the clipboard as it is.
    virtual void setUnprocessedTextInClipboard(const QString& text);

private:
    std::shared_ptr<TClipboardHelper> m_clipboardHelper;
    std::shared_ptr<TTibiaUrlTools> m_urlTools;
};

}
}
