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

#include "client/const.h"
#include "client/position.h"

#include <framework/core/declarations.h>

#include <vector>

class LocalPlayerSessionThing;

/// Tip-internal GoPath / client walk-prediction (no CipSoft `T*` twin). Owns async
/// auto-walk scheduling and protocol-facing prediction; storage row holds armed dirs.
class LocalPlayerGoPath
{
public:
    explicit LocalPlayerGoPath(LocalPlayerSessionThing& owner);

    void unlockWalk();
    void stopPendingWalk();
    void lockWalk(uint16_t millis = 250);
    bool isWalkLocked();
    void stopAutoWalk(bool notifyServer = false);
    bool hasAutoWalkDirs();
    bool autoWalk(const Position& destination, bool retry = false,
                  tibia::client::AutoWalkPathMode pathMode = tibia::client::AutoWalkPathMinimap);
    bool canWalk(bool ignoreLock = false);

    void predictStep(tibia::client::Direction direction);
    void armGoPathDirs(const std::vector<tibia::client::Direction>& dirs);
    bool tryBeginNextGoPathStep();
    void resetPreWalk();
    bool isPreWalking();

    void cancelWalk(tibia::client::Direction direction = tibia::client::InvalidDirection);
    void onWalking();
    void onMovementTweenTerminated();
    void handleTeleport();
    void onConfirmedPosition(const Position& newPos);

    size_t getPreWalkingSize();
    bool isAutoWalking();
    void registerAdjustInvalidPosEvent();

private:
    bool retryAutoWalk();
    void cancelAdjustInvalidPosEvent();

    LocalPlayerSessionThing& m_owner;
    ScheduledEventPtr m_adjustInvalidPosEvent;
    ScheduledEventPtr m_autoWalkContinueEvent;
    /// Chunk continue deferred until the current step tween ends (so next GoPath can prewalk).
    Position m_pendingChunkContinue;
    bool m_knownCompletePath{ false };
};
