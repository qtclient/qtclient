/*
 * Copyright (c) 2024-2026 qtclient contributors
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "client/const.h"
#include "client/position.h"

#include <memory>
#include <vector>

struct PathFindResult
{
    tibia::client::PathFindResult status = tibia::client::PathFindResultNoWay;
    std::vector<tibia::client::Direction> path;
    int complexity = 0;
    Position start;
    Position destination;
};
using PathFindResult_ptr = std::shared_ptr<PathFindResult>;

struct Node
{
    float cost;      // Indexed8 / ground tile cost (CIP TNavigationNode[4])
    float totalCost; // f = g + h (CIP [6]; heap key)
    float pathCost;  // g from start (CIP [3])
    Position pos;
    Node* prev;
    int distance;
    int unseen;
};
