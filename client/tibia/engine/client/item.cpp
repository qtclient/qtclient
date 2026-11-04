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

#include "item.h"

#include "animator.h"
#include "gameconfig.h"

#include "tibia/appearances/appearanceinstance.h"
#include "tibia/assets/thingtype.h"
#include "tibia/assets/things.h"
#include "worldmap/worldmapfield.h"
#include "framework/core/clock.h"

namespace {

Outfit& emptyPodiumOutfit()
{
    static Outfit empty;
    return empty;
}

}

#include <tibia/game/gamesession.h>


ItemPtr Item::create(const int id)
{
    const auto& item = std::make_shared<Item>();
    item->setId(id);

    return item;
}

void Item::draw(const Point& dest, const bool drawThings, LightSink* lightView)
{
    (void)dest; (void)drawThings; (void)lightView;
}


void Item::internalDraw(const int animationPhase, const Point& dest, const Color& color, const bool drawThings, const bool replaceColorShader, LightSink* lightView)
{
    (void)animationPhase; (void)dest; (void)color; (void)drawThings; (void)replaceColorShader; (void)lightView;
}


void Item::drawLight(const Point& dest, LightSink* lightView) {
    (void)dest; (void)lightView;
}


void Item::setPosition(const Position& position, const uint8_t stackPos)
{
    Thing::setPosition(position, stackPos);
}

int Item::getSubType()
{
    if (isSplash() || isFluidContainer())
        return m_countOrSubType;

    return g_game.getClientVersion() > 862 ? 0 : 1;
}

ItemPtr Item::clone()
{
    auto item = std::make_shared<Item>();
    *(item.get()) = *this;

    if (item->m_data) {
        item->m_data = nullptr;

        // Copy Vector
        item->getData()->attachedEffects = m_data->attachedEffects;
        item->getData()->attachedParticles = m_data->attachedParticles;
    }

    // Deep-copy CIP show-off so cloned items do not share look state.
    if (m_showOff) {
        ThingType* type = static_cast<Thing&>(*item).getThingType();
        item->m_showOff = std::make_shared<tibia::appearances::TShowOffSocketAppearanceInstance>(type);
        item->m_showOff->setLook(m_showOff->outfit(), m_showOff->direction(), m_showOff->drawsSocket());
    }

    return item;
}

void Item::patternsFor(ThingType* type, const int countOrSubType, int& patternX, int& patternY, int& patternZ)
{
    patternX = patternY = patternZ = 0;
    if (!type || type->isNull())
        return;

    const int numPatternX = type->getNumPatternX();
    const int numPatternY = type->getNumPatternY();

    if (type->isStackable() && numPatternX == 4 && numPatternY == 2) {
        if (countOrSubType <= 0) {
            patternX = 0;
            patternY = 0;
        } else if (countOrSubType < 5) {
            patternX = countOrSubType - 1;
            patternY = 0;
        } else if (countOrSubType < 10) {
            patternX = 0;
            patternY = 1;
        } else if (countOrSubType < 25) {
            patternX = 1;
            patternY = 1;
        } else if (countOrSubType < 50) {
            patternX = 2;
            patternY = 1;
        } else {
            patternX = 3;
            patternY = 1;
        }
        return;
    }

    if (!type->isSplash() && !type->isFluidContainer())
        return;

    int color = countOrSubType;
    if (g_game.getFeature(tibia::client::GameNewFluids)) {
        switch (countOrSubType) {
            case tibia::client::FluidNone:
                color = tibia::client::FluidTransparent;
                break;
            case tibia::client::FluidWater:
                color = tibia::client::FluidBlue;
                break;
            case tibia::client::FluidMana:
                color = tibia::client::FluidPurple;
                break;
            case tibia::client::FluidBeer:
            case tibia::client::FluidOil:
            case tibia::client::FluidMud:
            case tibia::client::FluidRum:
            case tibia::client::FluidTea:
            case tibia::client::FluidMead:
                color = tibia::client::FluidOrange;
                break;
            case tibia::client::FluidBlood:
            case tibia::client::FluidHealth:
                color = tibia::client::FluidRed;
                break;
            case tibia::client::FluidSlime:
                color = tibia::client::FluidGreen;
                break;
            case tibia::client::FluidLemonade:
            case tibia::client::FluidUrine:
            case tibia::client::FluidFruitJuice:
                color = tibia::client::FluidYellow;
                break;
            case tibia::client::FluidMilk:
            case tibia::client::FluidCoconutMilk:
                color = tibia::client::FluidWhite;
                break;
            case tibia::client::FluidWine:
                color = tibia::client::FluidPurple;
                break;
            case tibia::client::FluidInk:
                color = tibia::client::FluidBlack;
                break;
            case tibia::client::FluidCandy:
                color = tibia::client::FluidPink;
                break;
            case tibia::client::FluidChocolate:
                color = tibia::client::FluidBrown;
                break;
            default:
                color = tibia::client::FluidTransparent;
                break;
        }
    }
    patternX = (color % 4) % std::max(1, numPatternX);
    patternY = (color / 4) % std::max(1, numPatternY);
}

void Item::updatePatterns()
{
    m_numPatternX = m_numPatternY =
        m_numPatternZ = 0;

    // Avoid crashes with invalid items
    if (!isValid())
        return;

    ThingType* type = getThingType();
    if (isHangable()) {
        const int numPatternX = getNumPatternX();
        const auto& tile = getTile();
        if (tile) {
            if (tile->mustHookSouth())
                m_numPatternX = numPatternX >= 2 ? 1 : 0;
            else if (tile->mustHookEast())
                m_numPatternX = numPatternX >= 3 ? 2 : 0;
        }
        return;
    }

    if (isStackable() || isSplash() || isFluidContainer()) {
        int x = 0;
        int y = 0;
        int z = 0;
        patternsFor(type, m_countOrSubType, x, y, z);
        m_numPatternX = static_cast<uint8_t>(x);
        m_numPatternY = static_cast<uint8_t>(y);
        m_numPatternZ = static_cast<uint8_t>(z);
        return;
    }

    const int numPatternX = getNumPatternX();
    const int numPatternY = getNumPatternY();
    m_numPatternX = m_position.x % std::max<int>(1, numPatternX);
    m_numPatternY = m_position.y % std::max<int>(1, numPatternY);
    m_numPatternZ = m_position.z % std::max<int>(1, getNumPatternZ());
}

int Item::calculateAnimationPhase()
{
    if (!hasAnimationPhases() || !canAnimate()) return 0;

    if (getIdleAnimator()) return getIdleAnimator()->getPhase();

    if (m_async) {
        return (g_clock.millis() % (g_gameConfig.getItemTicksPerFrame() * getAnimationPhases())) / g_gameConfig.getItemTicksPerFrame();
    }

    if (g_clock.millis() - m_lastPhase >= g_gameConfig.getItemTicksPerFrame()) {
        m_phase = (m_phase + 1) % getAnimationPhases();
        m_lastPhase = g_clock.millis();
    }

    return m_phase;
}

int Item::peekAnimationPhase()
{
    if (!hasAnimationPhases() || !canAnimate())
        return 0;

    if (const auto& animator = getIdleAnimator())
        return animator->peekPhase();

    if (m_async) {
        const int phases = std::max(1, getAnimationPhases());
        const int ticks = std::max<int>(1, g_gameConfig.getItemTicksPerFrame());
        return (g_clock.millis() % (static_cast<long long>(ticks) * phases)) / ticks;
    }

    const int phases = std::max(1, getAnimationPhases());
    if (g_clock.millis() - m_lastPhase >= g_gameConfig.getItemTicksPerFrame())
        return (m_phase + 1) % phases;
    return m_phase;
}

void Item::setId(uint32_t id)
{
    if (!g_things.isValidDatId(id, ThingCategoryItem))
        id = 0;


    m_clientId = id;

    if (!isPodium())
        clearPodiumOutfit();

    // Shader example on only items that can be marketed.
    /*
    if (isMarketable()) {
        m_shaderId = g_shaders.getShader("Outfit - Outline")->getId();
    }
    */
}

std::shared_ptr<tibia::appearances::TShowOffSocketAppearanceInstance> Item::showOffSocket()
{
    if (!m_showOff) {
        ThingType* type = static_cast<Thing&>(*this).getThingType();
        m_showOff = std::make_shared<tibia::appearances::TShowOffSocketAppearanceInstance>(type);
    }
    return m_showOff;
}

std::shared_ptr<tibia::appearances::TShowOffSocketAppearanceInstance> Item::showOffSocket() const
{
    return m_showOff;
}

void Item::setPodiumOutfit(const Outfit& outfit)
{
    Outfit look = outfit;
    if (look.isInvalid())
        look.setCategory(look.getAuxId() > 0 ? ThingCategoryItem : ThingCategoryCreature);
    const auto socket = showOffSocket();
    socket->setLook(look, socket->direction(), socket->drawsSocket());
}

void Item::clearPodiumOutfit()
{
    m_showOff.reset();
}

void Item::setPodiumDirection(const uint8_t direction)
{
    const auto socket = showOffSocket();
    socket->setLook(socket->outfit(), direction, socket->drawsSocket());
}

void Item::setPodiumVisible(const bool visible)
{
    const auto socket = showOffSocket();
    socket->setLook(socket->outfit(), socket->direction(), visible);
}

const Outfit& Item::getPodiumOutfit() const
{
    return m_showOff ? m_showOff->outfit() : emptyPodiumOutfit();
}

uint8_t Item::getPodiumDirection() const
{
    return m_showOff ? m_showOff->direction() : uint8_t(2);
}

bool Item::isPodiumVisible() const
{
    return m_showOff ? m_showOff->drawsSocket() : true;
}

bool Item::hasPodiumOutfit() const
{
    return m_showOff && m_showOff->hasFigure();
}

ThingType* Item::getThingType() const {
    return g_things.getRawThingType(m_clientId, ThingCategoryItem);
}


void Item::setDurationTime(uint32_t duration)
{
    m_duration = duration;
    if (m_decaying)
        m_durationEnd = g_clock.millis() + static_cast<int64_t>(m_duration) * 1000;
}

void Item::setDecaying(bool decaying)
{
    if (m_decaying == decaying) return;
    m_decaying = decaying;

    if (decaying) {
        m_durationEnd = g_clock.millis() + static_cast<int64_t>(m_duration) * 1000;
    } else {
        const int64_t remaining_ms = m_durationEnd - g_clock.millis();
        m_duration = static_cast<uint32_t>(std::max<int64_t>(0, (remaining_ms + 999) / 1000));
        m_durationEnd = 0;
    }
}

uint32_t Item::getDurationTime() const
{
    if (m_decaying) {
        const auto remaining = m_durationEnd - g_clock.millis();
        return remaining > 0 ? static_cast<uint32_t>(remaining / 1000) : 0;
    }
    return m_duration;
}

int Item::getClothSlot()
{
    auto* type = getThingType();
    return type ? type->getClothSlot() : 0;
}

/* vim: set ts=4 sw=4 et :*/