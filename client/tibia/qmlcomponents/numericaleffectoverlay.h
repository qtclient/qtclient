#pragma once

#include <cip_gen.h>

#include <client/declarations.h>
#include <client/position.h>

#include <QPointer>
#include <QQmlComponent>

#include <vector>

namespace tibia::worldmap {
class TNumericalEffectStorage;
}

namespace tibia::qmlcomponents {

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

    void setSources(TWorldMapRenderProvider* provider, const worldmap::TNumericalEffectStorage* effects);

private:
    struct Field
    {
        Position coordinate;
        TNumericEffectsItemController* controller = nullptr;
        QPointer<QQuickItem> item;
        bool seen = false;
    };

    /// CIP: FUN_1010cfbe0.
    void update();
    bool isShown(const Position& coordinate) const;
    Field create(const Position& coordinate);
    void remove(Field& field);
    void clear();

    QPointer<TWorldMapRenderProvider> m_provider;
    const worldmap::TNumericalEffectStorage* m_effects = nullptr;
    QMetaObject::Connection m_frameConnection;
    QQmlComponent* m_component = nullptr;
    std::vector<Field> m_fields;
};

}
