/*
 * Copyright (c) 2024-2026 qtclient contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once

#include "declarations.h"
#include "client/eightbitcolor.h"

#include "clockservice.h"
#include <QColor>
#include <deque>
#include <memory>
#include <string>
#include <QPoint>
#include <QSize>
#include <tibia/game/gameobject.h>

// @bindclass
class AnimatedText final : public GameObject
{
public:
    AnimatedText();
    AnimatedText(const std::string_view text, const int color) : AnimatedText() {
        setText(text);
        setColor(color);
    }
    void onAppear();

    std::shared_ptr<AnimatedText> asAnimatedText() { return std::static_pointer_cast<AnimatedText>(shared_from_this()); }

    void setColor(const int color) { m_color = tibia::client::colorFromEightBit(color); }
    void setText(const std::string_view text) { m_text = std::string(text); }
    void setOffset(const QPoint& offset) { m_offset = offset; }

    QColor getColor() { return m_color; }
    QPoint getOffset() { return m_offset; }
    tibia::shared::TFrameStopWatch getTimer() const { return m_animationTimer; }
    std::string getText() const { return m_text; }

    bool merge(const AnimatedTextPtr& other);
    Position getPosition() const { return m_position; }
    void setPosition(const Position& position) { m_position = position; }

private:
    QColor m_color{ QColor(Qt::white) };
    tibia::shared::TFrameStopWatch m_animationTimer;
    std::string m_text;
    QPoint m_offset;
    Position m_position;
};
