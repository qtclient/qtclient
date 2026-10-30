#pragma once

#include "game/playerstatestracker.h"

#include <cip_gen.h>

#include <client/declarations.h>

#include <QHash>
#include <QJsonObject>
#include <QPointer>
#include <QQmlComponent>

namespace tibia::qmlcomponents {
class TWorldMapRenderProvider;
}

namespace tibia::worldmap {

/// The quest / modification icons under a creature's state flags (CIP:
/// TCreatureOtherIconModel, qt_static_metacall 0x101069650; roles image, value,
/// showValue).
class TCreatureOtherIconModel : public cip::TCreatureOtherIconModel
{
    Q_OBJECT
public:
    struct Icon
    {
        QString image;
        int value = 0;
        bool showValue = false;
        bool operator==(const Icon&) const = default;
    };

    using cip::TCreatureOtherIconModel::TCreatureOtherIconModel;

    void setIcons(const QList<Icon>& icons);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    QList<Icon> m_icons;
};

/// What one CreatureHUD.qml shows (CIP: TCreatureHUDQmlRenderInfo). Signals only
/// on change, so a frame with nothing new costs QML nothing.
class TCreatureHUDQmlRenderInfo : public cip::tibia::worldmap::TCreatureHUDQmlRenderInfo
{
    Q_OBJECT
public:
    explicit TCreatureHUDQmlRenderInfo(QObject* parent = nullptr);

    /// Display options, by CIP option name (clientoptions.json "options").
    void update(Creature& creature, float lightness, float scaleFactor, const QJsonObject& options);
    /// The own character's special conditions (CIP 0x101064c00: FUN_10101f1f0
    /// with the HUD filter); set before the HUD is created.
    void setPlayerStates(QObject* model) { m_playerStates = model; }
    /// specialConditionsOptions showInHUD, for the own character only.
    void setShowSpecialConditions(bool show);

private:
    void updateIcons(Creature& creature, const QJsonObject& options);

    TCreatureOtherIconModel* m_otherIcons = nullptr;
};

/// Keeps one CreatureHUD.qml per shown creature in the map's HUD overlay and
/// places it over its creature (CIP: TCreatureHUDOverlayController).
class TCreatureHUDOverlayController : public cip::tibia::worldmap::TCreatureHUDOverlayController
{
    Q_OBJECT
public:
    /// `overlay` is MapWindowPane's worldmapCreatureHUDOverlay: it covers the
    /// scaled map, in unscaled pixels.
    TCreatureHUDOverlayController(QQuickItem* overlay, qmlcomponents::TWorldMapRenderProvider* provider,
                                  QObject* parent = nullptr);
    ~TCreatureHUDOverlayController() override;

    void setRenderProvider(qmlcomponents::TWorldMapRenderProvider* provider);

public Q_SLOTS:
    void refreshCreatureHUDs() override;
    void update() override;

private:
    struct Hud
    {
        QPointer<QQuickItem> item;
        TCreatureHUDQmlRenderInfo* info = nullptr;
        bool seen = false;
    };

    Hud create();
    void clear();

    QPointer<QQuickItem> m_overlay;
    qmlcomponents::TWorldMapRenderProvider* m_provider = nullptr;
    QMetaObject::Connection m_frameConnection;
    QQmlComponent* m_component = nullptr;
    QHash<uint32_t, Hud> m_huds;
    QJsonObject m_options;
    game::TPlayerStatesTracker m_playerStates;
};

}
