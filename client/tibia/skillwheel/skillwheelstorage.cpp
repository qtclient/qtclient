#include "skillwheelstorage.h"

#include <QDebug>
#include <QJsonArray>

#include <algorithm>
#include <numeric>
#include <tibia/game/gamesession.h>

namespace tibia::skillwheel {

namespace {

TSkillWheelStorage* s_liveInstance = nullptr;

bool hasBasic2(const TGem& gem) { return gem.quality >= static_cast<int>(EGemQuality::Regular); }
bool hasSupreme(const TGem& gem) { return gem.quality >= static_cast<int>(EGemQuality::Greater); }

/// The gems have the same mods.
bool sameMods(const TGem& a, const TGem& b)
{
    if (a.basic1 != b.basic1 || hasBasic2(a) != hasBasic2(b) || hasSupreme(a) != hasSupreme(b))
        return false;
    return (!hasBasic2(a) || a.basic2 == b.basic2) && (!hasSupreme(a) || a.supreme == b.supreme);
}

/// The vessel holds `gemId` in the current wheel.
bool isSocketed(const TWheel& wheel, const int gemId) { return std::ranges::find(wheel.gems, gemId) != wheel.gems.end(); }

TCodeGem codeGem(const TGem& gem)
{
    return { gem.basic1, hasBasic2(gem) ? gem.basic2 : -1, hasSupreme(gem) ? gem.supreme : -1 };
}

}

TSkillWheelStorage::TSkillWheelStorage(QObject* parent)
    : cip::tibia::skillwheel::TSkillWheelStorage(parent)
{
    s_liveInstance = this;
}

TSkillWheelStorage::~TSkillWheelStorage()
{
    if (s_liveInstance == this)
        s_liveInstance = nullptr;
}

TSkillWheelStorage* TSkillWheelStorage::liveInstance() { return s_liveInstance; }

QString TSkillWheelStorage::configurationName() const { return QStringLiteral("wheelOfDestiny"); }

/// Every preset as its name and its export code.
QJsonObject TSkillWheelStorage::toJson() const
{
    QJsonObject json;
    QJsonArray presets;
    for (const auto& [name, stored] : m_presets) {
        // A vessel whose gem is not in the inventory keeps the mods the preset was read with.
        TWheel wheel = stored;
        std::vector<TGem> gems = m_gemInventory;
        for (int vessel = 0; vessel < VesselCount; ++vessel) {
            const TCodeGem mods = wheel.gemMods[vessel];
            if ((wheel.gems[vessel] != NoGem && gem(wheel.gems[vessel])) || mods.basic1 < 0)
                continue;
            TGem kept;
            kept.id = -2 - vessel;
            kept.basic1 = mods.basic1;
            kept.basic2 = mods.basic2;
            kept.supreme = mods.supreme;
            kept.quality = mods.supreme >= 0 ? 2 : mods.basic2 >= 0 ? 1 : 0;
            gems.push_back(kept);
            wheel.gems[vessel] = kept.id;
        }
        QJsonObject preset;
        preset.insert(QLatin1String("name"), name);
        preset.insert(QLatin1String("exportString"), encodeWheelCode(wheel, gems, wheel.vocation - 1));
        presets.append(preset);
    }
    json.insert(QLatin1String("presets"), presets);
    return json;
}

/// A preset without a name or code, or with a code that is not a valid wheel, is skipped. The
/// presets read before are dropped (CIP has one storage per game session).
void TSkillWheelStorage::fromJson(const QJsonObject& json)
{
    m_presets.clear();
    for (const QJsonValue value : json.value(QLatin1String("presets")).toArray()) {
        const QJsonObject preset = value.toObject();
        const QString name = preset.value(QLatin1String("name")).toString();
        const QString code = preset.value(QLatin1String("exportString")).toString();
        if (name.isEmpty() || code.isEmpty()) {
            qDebug() << "Skipped Preset with insufficient data";
            continue;
        }
        const std::optional<TWheelCode> decoded = decodeWheelCode(code);
        if (!decoded || !isValidWheelCode(*decoded)) {
            qDebug() << "Skipped invalid Preset " << name;
            continue;
        }
        TWheel wheel;
        wheel.vocation = decoded->vocationIndex + 1;
        wheel.points = decoded->points;
        wheel.gemMods = decoded->gems;
        m_presets.insert_or_assign(name, wheel);
    }
}

void TSkillWheelStorage::setUnspentSkillPoints(const int points)
{
    m_unspentSkillPoints = points;
    Q_EMIT unspentSkillPointsChanged();
}

void TSkillWheelStorage::setPresets(const std::map<QString, TWheel>& presets)
{
    if (presets == m_presets)
        return;
    m_presets = presets;
    Q_EMIT configurationChangedFromUserInput();
}

void TSkillWheelStorage::addOrReplacePreset(const QString& name, const TWheel& wheel)
{
    if (name.isEmpty())
        return;
    std::map<QString, TWheel> presets = m_presets;
    presets.insert_or_assign(name, wheel);
    setPresets(presets);
}

std::optional<TWheel> TSkillWheelStorage::preset(const QString& name) const
{
    const auto it = m_presets.find(name);
    if (it == m_presets.end())
        return std::nullopt;
    return it->second;
}

void TSkillWheelStorage::removePreset(const QString& name)
{
    std::map<QString, TWheel> presets = m_presets;
    presets.erase(name);
    setPresets(presets);
}

bool TSkillWheelStorage::hasPreset(const QString& name) const { return m_presets.contains(name); }

bool TSkillWheelStorage::renamePreset(const QString& oldName, const QString& newName)
{
    const std::optional<TWheel> wheel = preset(oldName);
    if (!wheel)
        return false;
    removePreset(oldName);
    addOrReplacePreset(newName, *wheel);
    return true;
}

void TSkillWheelStorage::setCurrentSkillWheel(const TWheel& wheel) { setCurrentSkillWheel(std::optional<TWheel>(wheel)); }

void TSkillWheelStorage::setCurrentSkillWheel(const std::optional<TWheel>& wheel)
{
    if (wheel == m_currentSkillWheel)
        return;
    m_currentSkillWheel = wheel;
    Q_EMIT currentSkillWheelChanged();
}

void TSkillWheelStorage::resetCurrentSkillWheel()
{
    if (!m_currentSkillWheel)
        return;
    m_currentSkillWheel.reset();
    Q_EMIT currentSkillWheelChanged();
}

bool TSkillWheelStorage::isCurrentSkillWheelChanged() const { return m_serverSkillWheel != m_currentSkillWheel; }

void TSkillWheelStorage::setServerData(const int changeState, const QString& playerName, const bool ownCharacter, const int regularPoints,
                                       const int extraPoints, const std::vector<std::pair<int, int>>& promotionScrolls,
                                       const bool shrinesOfInsightPoints, const int huntingTaskPoints, const TWheel& wheel)
{
    m_changeState = changeState;
    m_playerName = playerName;
    m_ownCharacter = ownCharacter;
    m_regularPoints = regularPoints;
    m_extraPoints = extraPoints;
    m_promotionScrolls = promotionScrolls;
    m_shrinesOfInsightPoints = shrinesOfInsightPoints;
    m_huntingTaskPoints = huntingTaskPoints;
    m_serverSkillWheel = wheel;
    m_preview = false;
    Q_EMIT serverSkillWheelChanged();
}

void TSkillWheelStorage::setPreview(const QString& playerName, const int vocation)
{
    TWheel wheel;
    wheel.vocation = vocation;
    setServerData(0, playerName, true, 0, 0, {}, false, 0, wheel);
    m_preview = true;
}

void TSkillWheelStorage::clearServerData()
{
    m_changeState = 0;
    m_playerName.clear();
    m_ownCharacter = false;
    m_regularPoints = 0;
    m_extraPoints = 0;
    m_promotionScrolls.clear();
    m_preview = false;
    if (m_serverSkillWheel) {
        m_serverSkillWheel.reset();
        Q_EMIT serverSkillWheelChanged();
    }
    Q_EMIT serverSkillWheelChanged();
}

void TSkillWheelStorage::setGemInventory(std::vector<TGem> gems)
{
    const std::vector<TGem> previous = std::exchange(m_gemInventory, std::move(gems));
    if (m_currentSkillWheel) {
        for (int vessel = 0; vessel < VesselCount; ++vessel) {
            int& socketed = m_currentSkillWheel->gems[vessel];
            if (socketed == NoGem)
                continue;
            const auto now = std::ranges::find_if(m_gemInventory, [socketed](const TGem& g) { return g.id == socketed; });
            const auto before = std::ranges::find_if(previous, [socketed](const TGem& g) { return g.id == socketed; });
            if (now == m_gemInventory.end() || (before != previous.end() && !sameMods(*before, *now)))
                socketed = NoGem;
        }
    }
    Q_EMIT gemInventoryChanged();
}

std::optional<TGem> TSkillWheelStorage::gem(const int id) const
{
    const auto it = std::ranges::find_if(m_gemInventory, [id](const TGem& g) { return g.id == id; });
    if (it == m_gemInventory.end())
        return std::nullopt;
    return *it;
}

std::optional<TGem> TSkillWheelStorage::findGem(const TGem& gem) const
{
    std::optional<TGem> sameModsGem;
    for (const TGem& candidate : m_gemInventory) {
        if (!sameMods(candidate, gem))
            continue;
        if (candidate.id == gem.id)
            return candidate;
        if (!sameModsGem)
            sameModsGem = candidate;
    }
    return sameModsGem;
}

int TSkillWheelStorage::basicModGemCount(const int mod) const
{
    int count = 0;
    for (const TGem& gem : m_gemInventory) {
        count += gem.basic1 == mod ? 1 : 0;
        if (hasBasic2(gem))
            count += gem.basic2 == mod ? 1 : 0;
    }
    return count;
}

int TSkillWheelStorage::supremeModGemCount(const int mod) const
{
    return static_cast<int>(std::ranges::count_if(m_gemInventory, [mod](const TGem& g) { return hasSupreme(g) && g.supreme == mod; }));
}

bool TSkillWheelStorage::isBasicModActive(const int mod) const
{
    if (!m_currentSkillWheel)
        return false;
    return std::ranges::any_of(m_gemInventory, [&](const TGem& g) {
        return (g.basic1 == mod || (hasBasic2(g) && g.basic2 == mod)) && isSocketed(*m_currentSkillWheel, g.id);
    });
}

bool TSkillWheelStorage::isSupremeModActive(const int mod) const
{
    if (!m_currentSkillWheel)
        return false;
    return std::ranges::any_of(m_gemInventory,
                               [&](const TGem& g) { return hasSupreme(g) && g.supreme == mod && isSocketed(*m_currentSkillWheel, g.id); });
}

void TSkillWheelStorage::setModGrades(TGrades basicGrades, TGrades supremeGrades)
{
    m_basicGrades = std::move(basicGrades);
    m_supremeGrades = std::move(supremeGrades);
    Q_EMIT modGradesChanged();
}

void TSkillWheelStorage::setUnrevealedGems(const int quality, const int count)
{
    m_unrevealedGems[quality] = count;
    Q_EMIT unrevealedGemsChanged();
}

int TSkillWheelStorage::unrevealedGems(const int quality) const
{
    const auto it = m_unrevealedGems.find(quality);
    return it != m_unrevealedGems.end() ? it->second : 0;
}

void TSkillWheelStorage::setModList(const std::vector<TModListEntry>& modList)
{
    m_modList = modList;
    Q_EMIT modListChanged();
}

/// The supreme mods (of the current wheel's vocation when it is valid), then the basic mods.
void TSkillWheelStorage::updateModList()
{
    std::vector<TModListEntry> list;
    const auto grade = [](const TGrades& grades, const int mod) {
        const auto it = grades.find(mod);
        return it != grades.end() ? it->second : 0;
    };
    for (int mod = 0; mod < supremeModCount(); ++mod) {
        if (m_currentSkillWheel && isValid(m_currentSkillWheel->points) && !supremeModForVocation(mod, m_currentSkillWheel->vocation - 1))
            continue;
        list.push_back({ false, isSupremeModActive(mod), mod, supremeModGemCount(mod), grade(m_supremeGrades, mod) });
    }
    for (int mod = 0; mod < basicModCount(); ++mod) {
        if (!hasBasicMod(mod))
            continue;
        list.push_back({ true, isBasicModActive(mod), mod, basicModGemCount(mod), grade(m_basicGrades, mod) });
    }
    setModList(list);
}

void TSkillWheelStorage::setLesserFragments(const int fragments)
{
    m_lesserFragments = fragments;
    Q_EMIT lesserFragmentsChanged();
}

void TSkillWheelStorage::setGreaterFragments(const int fragments)
{
    m_greaterFragments = fragments;
    Q_EMIT greaterFragmentsChanged();
}

int TSkillWheelStorage::vocation() const { return m_serverSkillWheel ? std::clamp(m_serverSkillWheel->vocation, 1, VocationCount) : 1; }

TWheel TSkillWheelStorage::currentWheel() const
{
    if (m_currentSkillWheel)
        return *m_currentSkillWheel;
    TWheel wheel;
    wheel.vocation = vocation();
    return wheel;
}

TWheel TSkillWheelStorage::withInventoryGems(const TWheel& wheel) const
{
    TWheel result = wheel;
    for (int vessel = 0; vessel < VesselCount; ++vessel) {
        const TCodeGem mods = wheel.gemMods[vessel];
        if (result.gems[vessel] != NoGem && gem(result.gems[vessel]))
            continue;
        result.gems[vessel] = NoGem;
        if (mods.basic1 < 0)
            continue;
        TGem pattern;
        pattern.id = NoGem;
        pattern.basic1 = mods.basic1;
        pattern.basic2 = mods.basic2;
        pattern.supreme = mods.supreme;
        pattern.quality = mods.supreme >= 0 ? 2 : mods.basic2 >= 0 ? 1 : 0;
        if (const std::optional<TGem> found = findGem(pattern))
            result.gems[vessel] = found->id;
    }
    for (int vessel = 0; vessel < VesselCount; ++vessel)
        if (const std::optional<TGem> socketed = gem(result.gems[vessel]))
            result.gemMods[vessel] = codeGem(*socketed);
    return result;
}

int TSkillWheelStorage::modGradesPoints() const { return modGradePoints(m_basicGrades, m_supremeGrades); }

int TSkillWheelStorage::totalPoints() const { return m_regularPoints + m_extraPoints + modGradesPoints(); }

int TSkillWheelStorage::spentPoints() const
{
    const TWheel wheel = currentWheel();
    return std::accumulate(wheel.points.begin(), wheel.points.end(), 0);
}

uint64_t TSkillWheelStorage::gold() const
{
    return tibia::game::localPlayerResourceBalance(tibia::client::RESOURCE_BANK_BALANCE)
           + tibia::game::localPlayerResourceBalance(tibia::client::RESOURCE_GOLD_EQUIPPED);
}

}
