#pragma once

#include "appearances/appearanceinstance.h"
#include "effects/numericaleffect.h"

#include <cip_gen.h>

#include <client/position.h>

#include <deque>
#include <functional>
#include <map>
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
    /// The active game session's storage (instance() falls back to an empty one).
    static void setInstance(TEffectStorage* storage);

    /// `maximumEffects`: how many effects, numbers included, the storage holds.
    explicit TEffectStorage(size_t maximumEffects = maximumEffectsSetting(), QObject* parent = nullptr);
    ~TEffectStorage() override;

    /// The `maximumNumberOfEffects` setting: 400, at most 800.
    static size_t maximumEffectsSetting();
    size_t maximumEffects() const { return m_maximumEffects; }
    /// Effects held, numerical ones included.
    size_t count() const;
    /// Another effect fits.
    bool hasRoom() const { return count() < maximumEffects(); }

    /// Add a started effect at its field; false when the storage is full.
    bool add(const TEffectPtr& effect);
    /// Remove the effects of `typeId` at `field` (the server's remove message).
    void remove(uint16_t typeId, const Position& field);
    /// Advance every effect to `nowMs`, then drop the finished ones.
    void update(qint64 nowMs);
    /// Drop the effects whose field is outside the map area `inArea` accepts
    /// (the view moved).
    void removeOutside(const std::function<bool(const Position&)>& inArea);
    void clear();

    /// The effect at `index` in the order of arrival.
    TEffectPtr effectAt(size_t index) const { return m_effects[index]; }
    /// Every effect, in the order of arrival.
    std::vector<TEffectPtr> effects() const { return m_effects; }
    /// The missiles flying on floor `z`, in their order of arrival.
    const std::vector<TEffectPtr>& missilesOn(uint8_t z) const;
    /// The effects at `field`, in their order of arrival.
    const std::vector<TEffectPtr>& effectsAt(const Position& field) const;
    bool hasEffectsAt(const Position& field) const { return m_byField.contains(field); }

    /// Adds an effect and keeps it until stopTrackedEffects (the tutorial's field markers).
    void trackEffect(const TEffectPtr& effect);
    /// Stops the tracked effects and drops the finished effects.
    void stopTrackedEffects();

    /// Adds a rising number, or adds its value to a number of the same colour
    /// started on the same field just before; false when merged or full.
    bool addNumericalEffect(const effects::TNumericalEffectPtr& effect);
    /// The numbers rising on `field`, oldest first.
    const std::deque<effects::TNumericalEffectPtr>& numericalEffectsAt(const Position& field) const;
    /// The fields of floors `minZ`..`maxZ` with rising numbers.
    std::vector<Position> numericalEffectFields(int minZ, int maxZ) const;
    /// Advances every number to `nowMs`.
    void updateNumericalEffects(qint64 nowMs);
    size_t numericalEffectCount() const;

private:
    struct FieldOrder
    {
        bool operator()(const Position& a, const Position& b) const
        {
            if (a.x != b.x)
                return a.x < b.x;
            if (a.y != b.y)
                return a.y < b.y;
            return a.z < b.z;
        }
    };

    void erase(const std::function<bool(const TEffectPtr&)>& predicate);
    void removeFinished();
    void removeFinishedNumericalEffects();

    std::vector<TEffectPtr> m_effects;
    size_t m_maximumEffects = 0;
    std::unordered_map<Position, std::vector<TEffectPtr>, Position::Hasher> m_byField;
    std::unordered_map<uint8_t, std::vector<TEffectPtr>> m_missilesByFloor;
    std::vector<TEffectPtr> m_trackedEffects;
    std::map<Position, std::deque<effects::TNumericalEffectPtr>, FieldOrder> m_numericalEffects;
};

}
