/*
 * Copyright (c) 2024-2026 qtclient contributors
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "client/position.h"

/// Engine-side listener for the fields the server describes (the Qt client's
/// minimap). Called on the parsing thread, once per field object change.
class MapObserver
{
public:
    virtual ~MapObserver() = default;
    /// An item or creature was added to or removed from the field, or the
    /// field was cleaned by a description. Not called for fields dropped
    /// because they left the aware range.
    virtual void onTileChanged(const Position& pos) = 0;
    /// A map description or move message is fully parsed (central position set).
    /// `fullDescription`: the whole window of the central floor was described
    /// (a map or central-floor description, or a floor change), not a step.
    virtual void onWindowChanged(bool fullDescription) = 0;
};
