#pragma once

#include "declarations.h"

#include <string>

// Text storage without the bitmap-font atlas. Draw is a no-op.
class CachedText
{
public:
    void draw(const Rect&, const Color&) {}
    void wrapText(int) {}
    void setFont(const auto&) {}
    void setText(const std::string_view text) { m_text = std::string{ text }; }
    void setAlign(const Fw::AlignmentFlag align) { m_align = align; }

    Size getTextSize() const { return {}; }
    std::string getText() const { return m_text; }
    bool hasText() const { return !m_text.empty(); }
    const std::string& getFont() const { return m_font; }
    Fw::AlignmentFlag getAlign() const { return m_align; }

private:
    std::string m_text;
    std::string m_font;
    Fw::AlignmentFlag m_align{ Fw::AlignTopLeft };
};
