#pragma once

#include <cip_gen.h>

#include <client/declarations.h>
#include <client/position.h>

#include <QPointer>
#include <QQmlComponent>

#include <vector>

#include <memory>

namespace tibia {
namespace effects {
class TNumericalEffectRenderProvider;
}
namespace game {
class IGameSession;
}
namespace renderer {
class TRenderVolumeProvider;
class TWorldMapCamera;
}
namespace worldmap {
class TWorldMapStorage;
}
}

namespace tibia {
namespace qmlcomponents {

class TWorldMapRenderProvider;

/// The numbers rising over one field (CIP: qmlcomponents::TNumericEffectsItemController).
class TNumericEffectsItemController : public cip::tibia::qmlcomponents::TNumericEffectsItemController
{
    Q_OBJECT
public:
    struct Item
    {
        QString text;
        QColor color;
        double percent = 0;
    };

    using cip::tibia::qmlcomponents::TNumericEffectsItemController::TNumericEffectsItemController;

    /// Always emits itemsChanged: the numbers move every frame.
    void setItems(std::vector<Item> items);

    QString getTextForIndex(int index) override;
    QColor getColorForIndex(int index) override;
    double getEffectPercentForIndex(int index) override;

private:
    std::vector<Item> m_items;
};

/// Damage, healing, mana and experience numbers over the map (CIP:
/// qmlcomponents::TNumericalEffectOverlayQuickItem): one
/// NumericalEffectsTemplate.qml per field with numbers.
class TNumericalEffectOverlayQuickItem : public cip::tibia::qmlcomponents::TNumericalEffectOverlayQuickItem
{
    Q_OBJECT
public:
    explicit TNumericalEffectOverlayQuickItem(QQuickItem* parent = nullptr);
    ~TNumericalEffectOverlayQuickItem() override;

    /// The session's render volume provider, world map camera, numerical
    /// effect render provider and world map storage (all unset without a
    /// session), and a new NumericalEffectsTemplate.qml component; an invalid
    /// template throws. The overlay is enabled when all four are set.
    /// Host: the session's render provider supplies the frame and pixels and
    /// drives the per-frame update.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

protected:
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

private:
    struct Field
    {
        Position coordinate;
        TNumericEffectsItemController* controller = nullptr;
        QPointer<QQuickItem> item;
        bool seen = false;
    };

    /// One item per field with rising numbers of the render volume (the
    /// numerical effect render provider's groups), placed over its field.
    void update();
    Field create(const Position& coordinate);
    void remove(Field& field);
    void clear();

    std::unique_ptr<QQmlComponent> m_component;
    bool m_enabled = false;
    std::shared_ptr<effects::TNumericalEffectRenderProvider> m_numericalEffectRenderProvider;
    std::shared_ptr<renderer::TRenderVolumeProvider> m_renderVolumeProvider;
    std::shared_ptr<renderer::TWorldMapCamera> m_worldMapCamera;
    std::shared_ptr<worldmap::TWorldMapStorage> m_worldMapStorage;
    QPointer<TWorldMapRenderProvider> m_provider;
    QMetaObject::Connection m_frameConnection;
    std::vector<Field> m_fields;
};

}
}
