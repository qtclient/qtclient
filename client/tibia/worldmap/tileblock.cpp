/*
 * Copyright (c) 2024-2026 qtclient contributors
 * SPDX-License-Identifier: MIT
 */
#include "worldmap/tileblock.h"

#include "worldmap/worldmapstorage.h"

tibia::worldmap::TWorldMapField* TileBlock::create(const Position& pos)
{
    auto& slot = m_tiles[getTileIndex(pos)];
    return slot = tibia::worldmap::TWorldMapStorage::ensureStorage()->getOrCreateField(pos);
}

tibia::worldmap::TWorldMapField* TileBlock::getOrCreate(const Position& pos)
{
    auto& slot = m_tiles[getTileIndex(pos)];
    if (slot)
        return slot;
    return slot = tibia::worldmap::TWorldMapStorage::ensureStorage()->getOrCreateField(pos);
}
