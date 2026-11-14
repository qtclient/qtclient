#pragma once

#include "appearances/appearanceinstance.h"

#include <cip_gen.h>

#include <client/position.h>

#include <functional>
#include <unordered_map>
#include <vector>

namespace tibia::effects {

/// The graphical effects shown on the map, in their order of arrival. The
/// storage holds a limited number of effects; an effect leaves it when its
/// animation finishes, when the server removes it, or when its field leaves
/// the view.
class TEffectStorage final : public cip::tibia::effects::TEffectStorage
{
public:
    using TEffectPtr = appearances::TAppearanceInstancePtr;

    /// The game session's storage (an empty one before a session exists).
    static TEffectStorage& instance();

    explicit TEffectStorage(QObject* parent = nullptr);
    ~TEffectStorage() override;

    /// The `maximumNumberOfEffects` setting: 400, at most 800.
    static size_t maximumEffects();
    /// Effects held, numerical ones included.
    size_t count() const;
    /// Another effect fits.
    bool hasRoom() const { return count() < maximumEffects(); }

    /// Add a started effect at its field; false when the storage is full.
    bool add(const TEffectPtr& effect);
    /// Remove the effects of `typeId` at `field` (the server's remove message).
    void remove(uint16_t typeId, const Position& field);
    /// Advance every effect to `nowMs`, then drop the finished ones and those
    /// whose field is out of view.
    void update(qint64 nowMs);
    void clear();

    /// The missiles flying on floor `z`, in their order of arrival.
    const std::vector<TEffectPtr>& missilesOn(uint8_t z) const;
    /// The effects at `field`, in their order of arrival.
    const std::vector<TEffectPtr>& effectsAt(const Position& field) const;
    bool hasEffectsAt(const Position& field) const { return m_byField.contains(field); }

    /// Counts the numerical effects, which share the limit.
    void setNumericalCount(std::function<size_t()> count) { m_numericalCount = std::move(count); }

private:
    void erase(const std::function<bool(const TEffectPtr&)>& predicate);

    std::vector<TEffectPtr> m_effects;
    std::unordered_map<Position, std::vector<TEffectPtr>, Position::Hasher> m_byField;
    std::unordered_map<uint8_t, std::vector<TEffectPtr>> m_missilesByFloor;
    std::function<size_t()> m_numericalCount;
};

}
