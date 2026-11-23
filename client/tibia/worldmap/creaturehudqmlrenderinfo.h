#pragma once

#include "worldmap/creaturehudrenderprovider.h"

#include <cip_gen.h>

#include <QColor>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

namespace tibia {
namespace game {
class TPlayerStateDisplayInfoModel;
}
}

namespace tibia {
namespace worldmap {

/// The quest / modification icons under a creature's name (CIP:
/// TCreatureOtherIconModel, roles image, value, showValue).
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

/// What one CreatureHUD.qml shows (CIP: TCreatureHUDQmlRenderInfo): a copy of
/// the creature's HUD record, its properties computed from it. A new record
/// signals only the properties it changes.
class TCreatureHUDQmlRenderInfo : public cip::tibia::worldmap::TCreatureHUDQmlRenderInfo
{
    Q_OBJECT
    /// qtclient extension, not CipSoft: the HUD is the own character's (for pack QML overrides).
    Q_PROPERTY(bool isLocalPlayer READ isLocalPlayer NOTIFY isLocalPlayerChanged)
public:
    explicit TCreatureHUDQmlRenderInfo(QObject* parent = nullptr);
    ~TCreatureHUDQmlRenderInfo() override;

    void setRenderInfo(const TCreatureHUDRenderInfo& renderInfo);
    const TCreatureHUDRenderInfo& renderInfo() const { return m_renderInfo; }

    QString name() const override { return m_renderInfo.name; }
    double healthPercent() const override { return m_renderInfo.healthPercent / 100.0; }
    double manaPercent() const override { return m_renderInfo.manaPercent / 100.0; }
    double manaShieldPercent() const override { return m_renderInfo.manaShieldPercent / 100.0; }
    QColor healthColor() const override;
    float lightness() const override { return static_cast<float>(m_renderInfo.lightness / 255.0); }
    bool showCreatureName() const override { return m_renderInfo.showName; }
    bool showHealthBar() const override { return m_renderInfo.showHealthBar; }
    bool showManaBar() const override { return m_renderInfo.showManaBar; }
    bool showManaShieldBar() const override { return m_renderInfo.showManaShieldBar; }
    bool showBars() const override { return m_renderInfo.showBars; }
    bool showArcs() const override { return m_renderInfo.showArcs; }
    double arcDistance() const override { return m_renderInfo.arcDistance; }
    double arcOpacity() const override { return m_renderInfo.arcOpacity; }
    uint arcWidth() const override { return m_renderInfo.arcWidth; }
    double arcHeight() const override { return m_renderInfo.arcHeight; }
    bool showCombopointsAndSerene() const override { return m_renderInfo.showCombopointsAndSerene; }
    bool showCombopointsAndSereneLeft() const override { return m_renderInfo.showCombopointsAndSereneLeft; }
    int comboPoints() const override { return m_renderInfo.comboPoints; }
    bool isSerene() const override { return m_renderInfo.isSerene; }
    float scaleFactor() const override { return m_renderInfo.scaleFactor; }
    QStringList horizontalCreatureIcons() const override;
    QStringList verticalCreatureIcons() const override;
    bool isFiendishMonster() const override;
    bool isLeaderMonster() const override { return m_renderInfo.statusMarks.count(0) != 0; }
    bool isLeaderMinionMonster() const override { return m_renderInfo.statusMarks.count(1) != 0; }
    bool showSpecialConditions() const override { return m_renderInfo.showSpecialConditions; }
    bool isLocalPlayer() const { return m_renderInfo.isLocalPlayer; }

Q_SIGNALS:
    void isLocalPlayerChanged();

private:
    /// Fills the other-icon model from the record's icons.
    void updateOtherIcons();

    TCreatureHUDRenderInfo m_renderInfo;
    TCreatureOtherIconModel* m_otherIconModel = nullptr;
    game::TPlayerStateDisplayInfoModel* m_playerStateModel = nullptr;
};

}
}
