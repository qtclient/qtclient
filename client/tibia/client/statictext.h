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

#include <tibia/schedule/schedule.h>
#include <QColor>
#include <deque>
#include <memory>
#include <string>
#include <QPoint>
#include <QSize>

#include "declarations.h"
#include <tibia/game/gameobject.h>

 // @bindclass
class StaticText final : public GameObject
{
public:
    StaticText();
    std::string getName() { return m_name; }
    std::string getText() { return m_text; }
    bool hasText() { return !m_text.empty(); }

    tibia::client::MessageMode getMessageMode() const { return m_mode; }
    std::string getFirstMessage() { return m_messages.empty() ? "" : m_messages[0].first; }

    bool isYell() const { return m_mode == tibia::client::MessageYell || m_mode == tibia::client::MessageMonsterYell || m_mode == tibia::client::MessageBarkLoud; }

    void setText(std::string_view text);
    void setFont(std::string_view fontName);
    bool addMessage(std::string_view name, tibia::client::MessageMode mode, std::string_view text);

    std::shared_ptr<StaticText> asStaticText() { return std::static_pointer_cast<StaticText>(shared_from_this()); }

    void setColor(const QColor& color) { m_color = color; }
    QColor getColor() { return m_color; }

    Position getPosition() const { return m_position; }
    void setPosition(const Position& position) { m_position = position; }

private:
    void update();
    void scheduleUpdate();
    void compose();

    std::deque<std::pair<std::string, int64_t>> m_messages;
    std::string m_name;
    tibia::client::MessageMode m_mode{ tibia::client::MessageNone };
    QColor m_color{ QColor(Qt::white) };
    std::string m_text;
    tibia::schedule::TScheduledTaskPtr m_updateEvent;
    Position m_position;
};
