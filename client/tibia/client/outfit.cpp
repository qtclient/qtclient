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

#include "outfit.h"
#include "colorhelper.h"

void Outfit::resetClothes()
{
    setHead(0);
    setBody(0);
    setLegs(0);
    setFeet(0);
    setMount(0);
    setFamiliar(0);
}

void Outfit::setHead(const uint8_t head) {
    if (m_head == head)
        return;

    m_head = head;
    m_headColor = tibia::shared::outfitColor(head);
}
void Outfit::setBody(const uint8_t body) {
    if (m_body == body)
        return;

    m_body = body;
    m_bodyColor = tibia::shared::outfitColor(body);
}
void Outfit::setLegs(const uint8_t legs) {
    if (m_legs == legs)
        return;

    m_legs = legs;
    m_legsColor = tibia::shared::outfitColor(legs);
}
void Outfit::setFeet(const uint8_t feet) {
    if (m_feet == feet)
        return;

    m_feet = feet;
    m_feetColor = tibia::shared::outfitColor(feet);
}