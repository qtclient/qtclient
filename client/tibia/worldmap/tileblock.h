/*
 * Copyright (c) 2024-2026 qtclient contributors
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "client/const.h"
#include "client/position.h"

#include <array>
#include <cstdint>

namespace tibia {
namespace worldmap {
class TWorldMapField;
}
}

class TileBlock
{
public:
    TileBlock() { m_tiles.fill(nullptr); }

    tibia::worldmap::TWorldMapField* create(const Position& pos);
    tibia::worldmap::TWorldMapField* getOrCreate(const Position& pos);
    tibia::worldmap::TWorldMapField* get(const Position& pos) { return m_tiles[getTileIndex(pos)]; }
    void assign(const Position& pos, tibia::worldmap::TWorldMapField* field) { m_tiles[getTileIndex(pos)] = field; }
    void remove(const Position& pos) { m_tiles[getTileIndex(pos)] = nullptr; }

    uint32_t getTileIndex(const Position& pos) { return ((pos.y % BLOCK_SIZE) * BLOCK_SIZE) + (pos.x % BLOCK_SIZE); }

    const std::array<tibia::worldmap::TWorldMapField*, BLOCK_SIZE * BLOCK_SIZE>& getTiles() const { return m_tiles; }

private:
    std::array<tibia::worldmap::TWorldMapField*, BLOCK_SIZE * BLOCK_SIZE> m_tiles;
};
