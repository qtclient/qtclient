#include "spellinfostorage.h"

#include "tibia/client/gamecore.h"

#include <QCoreApplication>
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
        spell.damagetype = object.value(QLatin1String("damagetype")).toString();
        spell.mean = object.value(QLatin1String("mean")).toInt();
        spell.scaling = stringList(object.value(QLatin1String("scaling")));
        // CIP ESpellSource: Unknown=0, LevelUp=1, WheelOfDestiny=2, ThreeFoldPath=3.
        const QString source = object.value(QLatin1String("source")).toString();
        if (source.contains(QLatin1String("Wheel"), Qt::CaseInsensitive))
            spell.source = 2;
        else if (source.contains(QLatin1String("ThreeFold"), Qt::CaseInsensitive) || source.contains(QLatin1String("Threefold"), Qt::CaseInsensitive))
            spell.source = 3;
        else if (!source.isEmpty())
            spell.source = 1;
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
        if (spell.id <= 0)
            continue;
        if (!spell.words.isEmpty())
            m_byWords.emplace(spell.words.toLower(), spell.id);
        if (spell.runeObjectId)
            m_byRune.emplace(spell.runeObjectId, spell.id);
        m_spells.emplace(spell.id, std::move(spell));
    }
}

const TSpellInfo* TSpellInfoStorage::spell(const int spellId) const
{
    const auto it = m_spells.find(spellId);
    return it == m_spells.end() ? nullptr : &it->second;
}

const TSpellInfo* TSpellInfoStorage::spellOfText(const QString& text) const
{
    const QString words = text.trimmed().toLower();
    if (words.isEmpty())
        return nullptr;
    if (const auto it = m_byWords.find(words); it != m_byWords.end())
        return spell(it->second);
    const TSpellInfo* best = nullptr;
    for (const auto& [formula, id] : m_byWords) {
        if (words.size() > formula.size() && words.startsWith(formula) && words.at(formula.size()) == QLatin1Char(' ')
            && (!best || formula.size() > best->words.size()))
            best = spell(id);
    }
    return best;
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

}
