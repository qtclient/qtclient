#pragma once

#include "declarations.h"

// No-op stand-in for the retired draw pool. Game logic still calls these.
class DrawPoolManager
{
public:
    void repaint(DrawPoolType) {}
    float getScaleFactor() const { return 1.f; }
    float getOpacity() const { return 1.f; }
};

inline DrawPoolManager g_drawPool;
