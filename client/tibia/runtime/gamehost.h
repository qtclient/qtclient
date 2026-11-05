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

#include <QObject>
#include <QString>
#include <QStringList>

namespace tibia {
namespace network {
class ExtendedOpcodeRouter;
}
}
namespace tibia {
namespace qmlcomponents {
class TWorldMapRenderProvider;
}
}

namespace tibia {
namespace runtime {

/// Pack-facing host façade for the live game client.
/// Tip `client::TGameClient` owns a concrete subclass and forwards the signals
/// packs connect to. Prefer this over `#include <client/gameclient.h>`.
/// CIP base is already a QObject, so this is composition (not MI).
class TGameHost : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    ~TGameHost() override = default;

    /// Frame-duration ring average (IDLEOT_PERF / FPS peels). 0 when unavailable.
    virtual qint64 averageFrameNs() const = 0;
    /// World-map build phase costs from the last advance (ns). 0 when unavailable.
    virtual qint64 worldmapBuildNs() const = 0;
    virtual qint64 worldmapTilesNs() const = 0;
    virtual qint64 worldmapLightNs() const = 0;
    virtual qint64 worldmapRaycastNs() const = 0;
    virtual qint64 worldmapRaycastGatherNs() const = 0;
    virtual qint64 worldmapRaycastRoofsNs() const = 0;

    virtual void toggleCharacterSelectionForSwitch() = 0;
    virtual bool requestCharacterSwitch(const QString& characterName) = 0;
    /// Account character names (login list); empty before login.
    virtual QStringList characterNames() const = 0;
    /// Live session or sessiondump logout.
    virtual void requestLogout() = 0;

    virtual network::ExtendedOpcodeRouter* opcodeRouter() const = 0;
    /// Opaque to packs that only forward the pointer (worldview wire).
    virtual qmlcomponents::TWorldMapRenderProvider* worldMapRenderProvider() const = 0;

Q_SIGNALS:
    void mainWindowInitialized();
    void worldLeft();
    void characterSwitchRequested(const QString& characterName);
    void characterSelectionClosed();
    void characterSelectionRequested();
};

}
}
