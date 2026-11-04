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
#include <framework/core/gameevents.h>

/// GameEvents object-scope publisher (`GameEvents::ObjectScope`). qtclient
/// controllers subscribe via `client::gamecore::Hook("object", …)`.
class GameObject : public std::enable_shared_from_this<GameObject>
{
public:
    virtual ~GameObject() = default;

    template<typename R, typename... T>
    R callLuaField(const std::string_view field, const T&... args)
    {
        const std::any out = g_gameEvents.publishObject(this, field, args...);
        const R* value = std::any_cast<R>(&out);
        return value ? *value : R();
    }
    template<typename... T>
    void callLuaField(const std::string_view field, const T&... args) { g_gameEvents.publishObject(this, field, args...); }

    GameObjectPtr asGameObject() { return shared_from_this(); }
    int getUseCount();

    template<typename T>
    std::shared_ptr<T> static_self_cast() { return std::static_pointer_cast<T>(shared_from_this()); }
};
