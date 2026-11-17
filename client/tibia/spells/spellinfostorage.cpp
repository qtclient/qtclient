#include "spellinfostorage.h"

#include "tibia/client/gamecore.h"

#include <QCoreApplication>

#include <algorithm>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace tibia::spells {

namespace {

/// CIP name→id (and display names).
int groupId(const QString& name)
{
    static const std::map<QString, int> ids = {
        { QStringLiteral("SPELLGROUP_ATTACK"), 1 },
        { QStringLiteral("SPELLGROUP_HEALING"), 2 },
        { QStringLiteral("SPELLGROUP_SUPPORT"), 3 },
        { QStringLiteral("SPELLGROUP_POWERSTRIKES"), 4 },
        { QStringLiteral("SPELLGROUP_CRIPPLING"), 6 },
        { QStringLiteral("SPELLGROUP_FOCUS"), 7 },
        { QStringLiteral("SPELLGROUP_ULTIMATESTRIKES"), 8 },
        { QStringLiteral("SPELLGROUP_GREATBEAMS"), 9 },
        { QStringLiteral("SPELLGROUP_BURSTS_OF_NATURE"), 10 },
        { QStringLiteral("SPELLGROUP_STANCE"), 11 },
    };
    const auto it = ids.find(name);
    return it == ids.end() ? 0 : it->second;
}

QStringList stringList(const QJsonValue& value)
{
    QStringList list;
    for (const QJsonValue& entry : value.toArray())
        list << entry.toString();
    return list;
}

/// The vocation names of `allowedVocations` as CIP vocation ids; other names
/// are left out.
std::set<uint8_t> vocationIds(const QJsonArray& names)
{
    static const std::map<QString, uint8_t> ids = {
        { QStringLiteral("Knight"), 1 }, { QStringLiteral("Paladin"), 2 }, { QStringLiteral("Sorcerer"), 3 },
        { QStringLiteral("Druid"), 4 },  { QStringLiteral("Monk"), 5 },
    };
    std::set<uint8_t> vocations;
    for (const QJsonValue& name : names) {
        const auto it = ids.find(name.toString());
        if (it != ids.end())
            vocations.insert(it->second);
    }
    return vocations;
}

/// A `damagetype` of spells.json (any case); damage over time counts as its element.
std::optional<uint8_t> damageTypeOf(const QString& name)
{
    static const std::map<std::string, uint8_t> types = {
        { "DAMAGE_HIT", 0 },          { "DAMAGE_LIFEDRAIN", 9 },      { "DAMAGE_MANADRAIN", 10 },
        { "DAMAGE_FIRE", 1 },         { "DAMAGE_EARTH", 2 },          { "DAMAGE_ENERGY", 3 },
        { "DAMAGE_ICE", 4 },          { "DAMAGE_HOLY", 5 },           { "DAMAGE_DEATH", 6 },
        { "DAMAGE_DROWNING", 8 },     { "DAMAGE_FIRE_DOT", 1 },       { "DAMAGE_EARTH_DOT", 2 },
        { "DAMAGE_ENERGY_DOT", 3 },   { "DAMAGE_ICE_DOT", 4 },        { "DAMAGE_HOLY_DOT", 5 },
        { "DAMAGE_DEATH_DOT", 6 },    { "DAMAGE_DROWNING_DOT", 8 },   { "DAMAGE_HEALING", 7 },
        { "DAMAGE_AGONY", 11 },
    };
    const auto it = types.find(name.toUpper().toStdString());
    return it == types.end() ? std::nullopt : std::optional<uint8_t>(it->second);
}

std::optional<int> scalingOf(const QString& name)
{
    static const std::map<std::string, int> scalings = {
        { "weaponskills", TSpellInfo::WeaponSkills }, { "magiclevel", TSpellInfo::MagicLevel },
        { "distanceskill", TSpellInfo::DistanceSkill }, { "weapon", TSpellInfo::Weapon },
        { "fistskill", TSpellInfo::FistSkill },        { "shieldingskill", TSpellInfo::ShieldingSkill },
    };
    const auto it = scalings.find(name.toStdString());
    return it == scalings.end() ? std::nullopt : std::optional<int>(it->second);
}

/// The groups, level, vocations and cooldowns both a spell and a rune's use
/// read from their object.
void readCastParameters(TSpellCastParameters& cast, const QJsonObject& object)
{
    cast.groupPrimary = uint8_t(groupId(object.value(QLatin1String("spellGroupPrimary")).toString()));
    cast.groupSecondary = uint8_t(groupId(object.value(QLatin1String("spellGroupSecondary")).toString()));
    cast.minimumLevel = uint16_t(object.value(QLatin1String("minimumCasterLevel")).toInt());
    cast.aggressive = object.value(QLatin1String("aggressive")).toBool();
    cast.vocations = vocationIds(object.value(QLatin1String("allowedVocations")).toArray());
    cast.cooldownSelf = qint64(object.value(QLatin1String("cooldownSelf")).toInt()) * 1000;
    cast.cooldownPrimaryGroup = qint64(object.value(QLatin1String("cooldownPrimaryGroup")).toInt()) * 1000;
    cast.cooldownSecondaryGroup = qint64(object.value(QLatin1String("cooldownSecondaryGroup")).toInt()) * 1000;
    cast.damageType = damageTypeOf(object.value(QLatin1String("damagetype")).toString());
}

}

TSpellInfoStorage* TSpellInfoStorage::instance()
{
    static TSpellInfoStorage* storage = new TSpellInfoStorage;
    return storage;
}

/// CIP ships ``:/spells/<file>`` in the client binary. Our qtclient / local extract
/// places the same files next to CIP assets (`data/assets/…` or
/// ``Resources/assets/…``). Browser /play hydrates qtclient AppData
/// ``packages/<IdleOT|Tibia>.app/Contents/Resources/assets/``.
QString spellsAssetPath(const QString& fileName)
{
    const QString embedded = QStringLiteral(":/spells/") + fileName;
    if (QFileInfo::exists(embedded))
        return embedded;
    // CIP package: applicationDirPath../Resources/assets/<file>
    const QString cipAssets = QDir(QDir(QCoreApplication::applicationDirPath())
                                       .absoluteFilePath(QStringLiteral("..")))
                                  .filePath(QStringLiteral("Resources/assets"));
    QStringList dirs = {
        cipAssets,
        qEnvironmentVariable("QTCLIENT_DATA_DIR"),
        QDir::current().filePath(QStringLiteral("data/assets")),
        QDir::current().filePath(QStringLiteral("data")),
        QStringLiteral("/data"),
        QDir::current().filePath(QStringLiteral("Contents/Resources/assets")),
        QDir::current().filePath(QStringLiteral("Resources/assets")),
    };
    // Tip AppData Resources (resolveCipResourcesDir) — same tree loadAssets uses.
    const QString resources = tibia::client::gamecore::cipResourcesDir();
    if (!resources.isEmpty())
        dirs.prepend(QDir(resources).filePath(QStringLiteral("assets")));
    // Fallback when identity/AppData differs from resolveCipResourcesDir probes.
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!appData.isEmpty()) {
        for (const char* packageName : { "IdleOT.app", "Tibia.app" }) {
            dirs.append(QDir(appData).filePath(
                QLatin1String("packages/") + QLatin1String(packageName)
                + QLatin1String("/Contents/Resources/assets")));
        }
    }
    for (const QString& dir : dirs) {
        if (dir.isEmpty())
            continue;
        const QString direct = QDir(dir).filePath(fileName);
        if (QFileInfo::exists(direct))
            return direct;
        const QString assets = QDir(dir).filePath(QStringLiteral("assets/") + fileName);
        if (QFileInfo::exists(assets))
            return assets;
        const QString gameTables = QDir(dir).filePath(QStringLiteral("game-tables/") + fileName);
        if (QFileInfo::exists(gameTables))
            return gameTables;
    }
    return {};
}

QString spellsJsonPath()
{
    return spellsAssetPath(QStringLiteral("spells.json"));
}

TSpellInfoStorage::TSpellInfoStorage()
{
    const QString path = spellsJsonPath();
    QFile file(path);
    if (path.isEmpty() || !file.open(QIODevice::ReadOnly)) {
        qWarning("spells.json missing (make extract → data/assets/spells.json): spells unknown");
        return;
    }
    for (const QJsonValue& value : QJsonDocument::fromJson(file.readAll()).array()) {
        const QJsonObject object = value.toObject();
        TSpellInfo spell;
        spell.id = object.value(QLatin1String("spellid")).toInt();
        spell.name = object.value(QLatin1String("name")).toString();
        spell.words = object.value(QLatin1String("formulaWithoutParams")).toString();
        spell.parameterHints = object.value(QLatin1String("parameterHints")).toString();
        spell.iconIndex = object.value(QLatin1String("iconIndex")).toInt();
        spell.minimumLevel = object.value(QLatin1String("minimumCasterLevel")).toInt();
        spell.mana = object.value(QLatin1String("castCostMana")).toInt();
        spell.soul = object.value(QLatin1String("castCostSoulPoints")).toInt();
        spell.premium = object.value(QLatin1String("premium")).toBool();
        spell.aggressive = object.value(QLatin1String("aggressive")).toBool();
        spell.crossHairTarget = object.value(QLatin1String("crossHairTarget")).toBool();
        spell.aimAtTarget = object.value(QLatin1String("aimattarget")).toBool();
        spell.isRune = object.value(QLatin1String("isRune")).toBool();
        spell.isRuneCreatable = object.value(QLatin1String("isRuneCreatable")).toBool();
        spell.description = object.value(QLatin1String("description")).toString();
        // CIP's record.
        spell.type = !spell.isRune ? TSpellInfo::Instant : spell.isRuneCreatable ? TSpellInfo::CreatableRune : TSpellInfo::Rune;
        spell.autoUnlock = object.value(QLatin1String("autoUnlock")).toBool();
        for (const QJsonValue& name : object.value(QLatin1String("scaling")).toArray()) {
            if (const std::optional<int> scaling = scalingOf(name.toString()))
                spell.scalings.insert(*scaling);
        }
        readCastParameters(spell.cast, object);
        spell.cast.onlyPromoted = object.value(QLatin1String("onlyPromoted")).toBool();
        spell.cast.mana = object.value(QLatin1String("castCostMana")).toInt();
        spell.cast.soul = uint8_t(object.value(QLatin1String("castCostSoulPoints")).toInt());
        if (spell.isRune) {
            const QJsonObject params = object.value(QLatin1String("runeParams")).toObject();
            TRuneParameters rune;
            rune.objectId = uint16_t(params.value(QLatin1String("runeType")).toInt());
            rune.useMagicLevel = params.value(QLatin1String("useMagicLevel")).toInt();
            rune.amount = uint8_t(params.value(QLatin1String("amount")).toInt());
            readCastParameters(rune.cast, params);
            rune.cast.mana = params.value(QLatin1String("useMana")).toInt();
            rune.cast.mean = params.value(QLatin1String("mean")).toInt();
            spell.rune = std::move(rune);
        } else {
            spell.cast.mean = object.value(QLatin1String("mean")).toInt();
        }
        spell.damagetype = object.value(QLatin1String("damagetype")).toString();
        spell.mean = object.value(QLatin1String("mean")).toInt();
        spell.scaling = stringList(object.value(QLatin1String("scaling")));
        // CIP ESpellSource: lower-cased "wheelofdestiny" 2, "threefoldpath" 3, else level up 1.
        const QString source = object.value(QLatin1String("source")).toString().toLower();
        spell.source = source == QLatin1String("wheelofdestiny") ? 2 : source == QLatin1String("threefoldpath") ? 3 : 1;
        QJsonObject cooldowns = object;
        if (const QJsonObject rune = object.value(QLatin1String("runeParams")).toObject(); !rune.isEmpty()) {
            spell.runeObjectId = rune.value(QLatin1String("runeType")).toInt();
            spell.runeAmount = rune.value(QLatin1String("amount")).toInt();
            spell.minimumMagicLevel = rune.value(QLatin1String("useMagicLevel")).toInt();
            spell.runeVocations = stringList(rune.value(QLatin1String("allowedVocations")));
            if (spell.mean == 0)
                spell.mean = rune.value(QLatin1String("mean")).toInt();
            if (spell.damagetype.isEmpty())
                spell.damagetype = rune.value(QLatin1String("damagetype")).toString();
            cooldowns = rune;
        }
        spell.groupPrimary = groupId(cooldowns.value(QLatin1String("spellGroupPrimary")).toString());
        spell.groupSecondary = groupId(cooldowns.value(QLatin1String("spellGroupSecondary")).toString());
        spell.cooldownSelf = cooldowns.value(QLatin1String("cooldownSelf")).toInt();
        spell.cooldownPrimaryGroup = cooldowns.value(QLatin1String("cooldownPrimaryGroup")).toInt();
        spell.cooldownSecondaryGroup = cooldowns.value(QLatin1String("cooldownSecondaryGroup")).toInt();
        spell.vocations = stringList(object.value(QLatin1String("allowedVocations")));
        if (spell.vocations.isEmpty())
            spell.vocations = stringList(cooldowns.value(QLatin1String("allowedVocations")));
        if (spell.rune)
            m_byRune[spell.rune->objectId] = spell.id;
        m_spells[spell.id] = std::move(spell);
    }
}

const TSpellInfo* TSpellInfoStorage::spell(const int spellId) const
{
    const auto it = m_spells.find(spellId);
    return it == m_spells.end() ? nullptr : &it->second;
}

int TSpellInfoStorage::spellCount() const { return int(m_spells.size()); }

TSpellInfo TSpellInfoStorage::spellInfoAt(const int spellId) const
{
    TSpellInfo spell;
    if (const TSpellInfo* record = this->spell(spellId))
        spell = *record;
    applySpellModifications(spell);
    return spell;
}

std::optional<TSpellInfo> TSpellInfoStorage::spellInfo(const int spellId) const
{
    const TSpellInfo* record = spell(spellId);
    if (!record)
        return std::nullopt;
    TSpellInfo spell = *record;
    applySpellModifications(spell);
    return spell;
}

QString TSpellInfoStorage::spellFormula(const int spellId) const
{
    if (!spell(spellId))
        return {};
    const TSpellInfo spell = spellInfoAt(spellId);
    if (spell.parameterHints.isEmpty())
        return spell.words;
    return spell.words + QLatin1Char(' ') + spell.parameterHints.trimmed();
}

std::optional<TSpellCastParameters> TSpellInfoStorage::castParameters(const int spellId) const
{
    const std::optional<TSpellInfo> spell = spellInfo(spellId);
    if (!spell)
        return std::nullopt;
    return spell->rune ? spell->rune->cast : spell->cast;
}

std::optional<TSpellInfo> TSpellInfoStorage::runeSpellInfo(const int objectId) const
{
    const auto it = m_byRune.find(objectId);
    if (it == m_byRune.end())
        return std::nullopt;
    return spellInfo(it->second);
}

bool TSpellInfoStorage::isCrossHairTarget(const int spellId) const
{
    const TSpellInfo* spell = this->spell(spellId);
    return spell && spell->crossHairTarget;
}

bool TSpellInfoStorage::isAimAtTarget(const int spellId) const
{
    const TSpellInfo* spell = this->spell(spellId);
    return spell && spell->aimAtTarget;
}

void TSpellInfoStorage::applySpellModifications(TSpellInfo& spell) const
{
    const std::vector<TSpellModification>* modifications = spellModifications(uint16_t(spell.id));
    if (!modifications)
        return;
    // The value is signed: a modification can lower the cost or cooldown.
    for (const TSpellModification& modification : *modifications) {
        const int value = int(modification.value);
        switch (modification.type) {
        case 0: spell.cast.mana += value; break;
        case 1: spell.cast.cooldownSelf += value; break;
        case 2: spell.cast.cooldownPrimaryGroup += value; break;
        case 3: spell.cast.cooldownSecondaryGroup += value; break;
        default: break;
        }
    }
}

TSpellInfoStorage::TSpellText TSpellInfoStorage::parseSpellText(QString text) const
{
    text = text.trimmed();
    TSpellText result;
    result.parameters = text;
    qsizetype bestLength = 0;
    for (const auto& [id, spell] : m_spells) {
        const qsizetype length = spell.words.size();
        if (bestLength >= length || !text.startsWith(spell.words, Qt::CaseInsensitive))
            continue;
        const QString parameters = text.mid(length).trimmed();
        if (!spell.parameterHints.isEmpty() || parameters.isEmpty()) {
            result.spell = &spell;
            result.parameters = parameters;
            bestLength = length;
        }
    }
    return result;
}

const TSpellInfo* TSpellInfoStorage::spellOfText(const QString& text) const { return parseSpellText(text).spell; }

int TSpellInfoStorage::spellIdOfText(const QString& text) const
{
    const TSpellInfo* spell = parseSpellText(text).spell;
    return spell ? spell->id : 0;
}

void TSpellInfoStorage::setSpellModifications(std::unordered_map<uint16_t, std::vector<TSpellModification>> modifications)
{
    m_modifications = std::move(modifications);
    Q_EMIT spellModificationsApplied();
}

void TSpellInfoStorage::clearSpellModifications() { m_modifications.clear(); }

const std::vector<TSpellInfoStorage::TSpellModification>* TSpellInfoStorage::spellModifications(const uint16_t spellId) const
{
    const auto it = m_modifications.find(spellId);
    return it == m_modifications.end() ? nullptr : &it->second;
}

const TSpellInfo* TSpellInfoStorage::runeSpellOfObject(const int objectId) const
{
    const auto it = m_byRune.find(objectId);
    return it == m_byRune.end() ? nullptr : spell(it->second);
}

QString TSpellInfoStorage::iconUrl(const TSpellInfo& spell)
{
    return QStringLiteral("image://spell-icons-32x32/%1").arg(spell.iconIndex);
}

void TSpellInfoStorage::setUnlockedSpells(const std::vector<uint16_t>& spellIds)
{
    if (&m_unlockedSpells != &spellIds)
        m_unlockedSpells.assign(spellIds.begin(), spellIds.end());
}

void TSpellInfoStorage::clearUnlockedSpells()
{
    m_unlockedSpells.clear();
}

bool TSpellInfoStorage::isSpellUnlocked(const uint16_t spellId) const
{
    return std::find(m_unlockedSpells.begin(), m_unlockedSpells.end(), spellId) != m_unlockedSpells.end();
}

}
