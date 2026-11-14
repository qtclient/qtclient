#include "effects/effectstorage.h"

#include "config/clientsettings.h"
#include "worldmap/worldmapfield.h"
#include "worldmap/worldmapstorage.h"

#include <tibia/game/mapsession.h>

#include <algorithm>

namespace tibia::effects {

namespace {

appearances::TEffectInstance* effectOf(const TEffectStorage::TEffectPtr& appearance)
{
    return dynamic_cast<appearances::TEffectInstance*>(appearance.get());
}

void notifyField(const Position& field)
{
    if (worldmap::TWorldMapField* tile = worldmap::fieldAt(field))
        tile->effectsChanged();
}

TEffectStorage* g_current = nullptr;

}

TEffectStorage& TEffectStorage::instance()
{
    static TEffectStorage fallback;
    return g_current ? *g_current : fallback;
}

TEffectStorage::TEffectStorage(QObject* parent)
    : cip::tibia::effects::TEffectStorage(parent)
{
    static bool fallbackMade = false;
    // The first one is the fallback; later ones belong to a game session.
    if (fallbackMade)
        g_current = this;
    fallbackMade = true;
}

TEffectStorage::~TEffectStorage()
{
    if (g_current == this)
        g_current = nullptr;
}

size_t TEffectStorage::maximumEffects()
{
    static const qint64 maximum = config::clientSettingAtMost(QStringLiteral("GRAPHICS"),
                                                               QStringLiteral("maximumNumberOfEffects"), 400, 800);
    return static_cast<size_t>(std::max<qint64>(0, maximum));
}

size_t TEffectStorage::count() const
{
    return m_effects.size() + (m_numericalCount ? m_numericalCount() : 0);
}

bool TEffectStorage::add(const TEffectPtr& effect)
{
    appearances::TEffectInstance* instance = effectOf(effect);
    if (!instance || !hasRoom())
        return false;
    m_effects.push_back(effect);
    if (dynamic_cast<appearances::TMissileEffectInstance*>(instance)) {
        m_missilesByFloor[instance->field().z].push_back(effect);
        return true;
    }
    m_byField[instance->field()].push_back(effect);
    notifyField(instance->field());
    return true;
}

void TEffectStorage::erase(const std::function<bool(const TEffectPtr&)>& predicate)
{
    std::vector<Position> changed;
    std::erase_if(m_effects, [&](const TEffectPtr& effect) {
        if (!predicate(effect))
            return false;
        if (appearances::TEffectInstance* instance = effectOf(effect);
            instance && dynamic_cast<appearances::TMissileEffectInstance*>(instance)) {
            std::erase(m_missilesByFloor[instance->field().z], effect);
        } else if (instance) {
            const auto it = m_byField.find(instance->field());
            if (it != m_byField.end()) {
                std::erase(it->second, effect);
                if (it->second.empty())
                    m_byField.erase(it);
            }
            changed.push_back(instance->field());
        }
        return true;
    });
    for (const Position& field : changed)
        notifyField(field);
}

void TEffectStorage::remove(const uint16_t typeId, const Position& field)
{
    erase([&](const TEffectPtr& effect) {
        const appearances::TEffectInstance* instance = effectOf(effect);
        return instance && instance->field() == field && effect->id() == typeId;
    });
}

void TEffectStorage::update(const qint64 nowMs)
{
    for (const TEffectPtr& effect : m_effects) {
        if (appearances::TEffectInstance* instance = effectOf(effect))
            instance->update(nowMs);
    }
    erase([](const TEffectPtr& effect) {
        const appearances::TEffectInstance* instance = effectOf(effect);
        if (!instance || instance->isFinished())
            return true;
        const Position& field = instance->field();
        return !tibia::game::isAwareOf({ field.x, field.y, field.z });
    });
}

void TEffectStorage::clear()
{
    erase([](const TEffectPtr&) { return true; });
}

const std::vector<TEffectStorage::TEffectPtr>& TEffectStorage::missilesOn(const uint8_t z) const
{
    static const std::vector<TEffectPtr> none;
    const auto it = m_missilesByFloor.find(z);
    return it == m_missilesByFloor.end() ? none : it->second;
}

const std::vector<TEffectStorage::TEffectPtr>& TEffectStorage::effectsAt(const Position& field) const
{
    static const std::vector<TEffectPtr> none;
    const auto it = m_byField.find(field);
    return it == m_byField.end() ? none : it->second;
}

}
