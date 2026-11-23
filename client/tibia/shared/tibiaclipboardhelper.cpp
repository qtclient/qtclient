#include "tibiaclipboardhelper.h"

namespace tibia::shared {

TTibiaClipboardHelper::TTibiaClipboardHelper(std::shared_ptr<TClipboardHelper> clipboardHelper,
                                             std::shared_ptr<TTibiaUrlTools> urlTools)
    : m_clipboardHelper(std::move(clipboardHelper))
    , m_urlTools(std::move(urlTools))
{
}

void TTibiaClipboardHelper::setTextInClipboard(const QString& text)
{
    m_clipboardHelper->setTextInClipboard(m_urlTools->process(text));
}

void TTibiaClipboardHelper::setUnprocessedTextInClipboard(const QString& text)
{
    m_clipboardHelper->setTextInClipboard(text);
}

}
