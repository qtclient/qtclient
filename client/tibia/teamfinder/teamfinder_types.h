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

#include <cstdint>
#include <string>
#include <vector>

// Team Finder (S 0x2C leader / S 0x2D member list) — Crystal sendLeaderTeamFinder / sendTeamFinderList.
// Leaf wire PODs only (no Outfit / staticdata hub). Tip + packs share these global names.

struct TeamFinderMember
{
    uint32_t guid{ 0 };
    std::string name;
    uint16_t level{ 0 };
    uint8_t vocation{ 0 };
    uint8_t status{ 0 };
};

struct TeamFinderLeaderData
{
    bool reset{ false };
    uint16_t minLevel{ 0 };
    uint16_t maxLevel{ 0 };
    uint8_t vocationIDs{ 0 };
    uint16_t teamSlots{ 0 };
    uint16_t freeSlots{ 0 };
    uint32_t timestamp{ 0 };
    uint8_t teamType{ 0 };
    uint16_t bossID{ 0 };
    uint16_t huntType{ 0 };
    uint16_t huntArea{ 0 };
    uint16_t questID{ 0 };
    std::vector<TeamFinderMember> members;
};

struct TeamFinderListEntry
{
    uint32_t leaderGuid{ 0 };
    std::string leaderName;
    uint16_t minLevel{ 0 };
    uint16_t maxLevel{ 0 };
    uint8_t vocationIDs{ 0 };
    uint16_t teamSlots{ 0 };
    uint16_t membersCount{ 0 };
    uint32_t timestamp{ 0 };
    uint8_t teamType{ 0 };
    uint16_t bossID{ 0 };
    uint16_t huntType{ 0 };
    uint16_t huntArea{ 0 };
    uint16_t questID{ 0 };
    uint8_t status{ 0 };
};

struct TeamFinderAssemble
{
    uint16_t minLevel{ 0 };
    uint16_t maxLevel{ 0 };
    uint8_t vocationIDs{ 0 };
    uint16_t teamSlots{ 0 };
    uint16_t freeSlots{ 0 };
    bool partyBool{ false };
    uint32_t timestamp{ 0 };
    uint8_t teamType{ 0 };
    uint16_t bossID{ 0 };
    uint16_t huntType{ 0 };
    uint16_t huntArea{ 0 };
    uint16_t questID{ 0 };
};
