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
#include "framework/graphics/declarations.h"

#include <memory>

static const std::vector<AttachedEffectPtr> EMPTY_ATTACHED_EFFECTS;

#pragma pack(push, 1)
class AttachableObject : public std::enable_shared_from_this<AttachableObject>
{
public:
    AttachableObject() = default;
    virtual ~AttachableObject();

    virtual bool isThing() { return false; }

    template<typename T>
    std::shared_ptr<T> static_self_cast() { return std::static_pointer_cast<T>(shared_from_this()); }

    template<typename T>
    std::shared_ptr<T> static_self_cast() const { return std::static_pointer_cast<T>(shared_from_this()); }

    void attachEffect(const AttachedEffectPtr& obj);
    void clearAttachedEffects(bool ignoreLuaEvent = false);
    void clearTemporaryAttachedEffects();
    void clearPermanentAttachedEffects();
    bool detachEffectById(uint16_t id);
    bool detachEffect(const AttachedEffectPtr& obj);
    AttachedEffectPtr getAttachedEffectById(uint16_t id);

    virtual void onStartAttachEffect(const AttachedEffectPtr& /*effect*/) {};
    virtual void onDispatcherAttachEffect(const AttachedEffectPtr& /*effect*/) {};
    virtual void onStartDetachEffect(const AttachedEffectPtr& /*effect*/) {};

    bool isOwnerHidden() { return m_ownerHidden > 0; }

    const std::vector<AttachedEffectPtr>& getAttachedEffects() { return m_data ? m_data->attachedEffects : EMPTY_ATTACHED_EFFECTS; };

    void attachParticleEffect(const std::string& name);
    void clearAttachedParticlesEffect();
    bool detachParticleEffectByName(const std::string& name);
    void updateAndAttachParticlesEffects(std::vector<std::string>& newElements);

    bool hasAttachedEffects()  const { return m_data && !m_data->attachedEffects.empty(); };
    bool hasAttachedParticles()  const { return m_data && !m_data->attachedParticles.empty(); };

protected:
    struct Data
    {
        std::vector<AttachedEffectPtr> attachedEffects;
        std::vector<std::string> attachedParticles;
    };

    void drawAttachedEffect(const Point& originalDest, const Point& dest, LightSink* lightView, bool isOnTop);
    void drawAttachedLightEffect(const Point& dest, LightSink* lightView);

    void onDetachEffect(const AttachedEffectPtr& effect, bool callEvent = true);
    void drawAttachedParticlesEffect(const Point& dest);

    auto getData() {
        if (!m_data)
            m_data = std::make_shared<Data>();
        return m_data;
    }

    std::shared_ptr<Data> m_data;

    uint8_t m_ownerHidden{ 0 };
};
#pragma pack(pop)
