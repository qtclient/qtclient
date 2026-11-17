#include "statusbarstorage.h"

#include <QJsonDocument>

#include <stdexcept>

namespace tibia::game {

namespace {

/// The JSON keys of the show* flags, by Entry.
const std::array<QLatin1StringView, TStatusbarStorage::EntryCount> EntryKeys = {
    QLatin1StringView("showExperience"),   QLatin1StringView("showMagicLevel"),       QLatin1StringView("showAxeFighting"),
    QLatin1StringView("showClubFighting"), QLatin1StringView("showDistanceFighting"), QLatin1StringView("showFistFighting"),
    QLatin1StringView("showShielding"),    QLatin1StringView("showSwordFighting"),    QLatin1StringView("showFishing"),
};

/// The flags in the order the configuration lists them.
constexpr std::array<TStatusbarStorage::Entry, TStatusbarStorage::EntryCount> StoredEntries = {
    TStatusbarStorage::Experience,   TStatusbarStorage::MagicLevel,    TStatusbarStorage::AxeFighting,
    TStatusbarStorage::ClubFighting, TStatusbarStorage::FistFighting,  TStatusbarStorage::SwordFighting,
    TStatusbarStorage::Shielding,    TStatusbarStorage::DistanceFighting, TStatusbarStorage::Fishing,
};

QString positionToString(const int position)
{
    switch (position) {
        case TStatusbarStorage::Off: return QStringLiteral("off");
        case TStatusbarStorage::Top: return QStringLiteral("top");
        case TStatusbarStorage::Bottom: return QStringLiteral("bottom");
        case TStatusbarStorage::Left: return QStringLiteral("left");
        case TStatusbarStorage::Right: return QStringLiteral("right");
        default: throw std::runtime_error("Unhandled enum value");
    }
}

int positionFromString(const QString& name)
{
    if (name == QLatin1String("top"))
        return TStatusbarStorage::Top;
    if (name == QLatin1String("bottom"))
        return TStatusbarStorage::Bottom;
    if (name == QLatin1String("left"))
        return TStatusbarStorage::Left;
    return name == QLatin1String("right") ? TStatusbarStorage::Right : TStatusbarStorage::Off;
}

QString styleToString(const int style)
{
    switch (style) {
        case TStatusbarStorage::Default: return QStringLiteral("default");
        case TStatusbarStorage::Compact: return QStringLiteral("compact");
        case TStatusbarStorage::Large: return QStringLiteral("large");
        case TStatusbarStorage::Parallel: return QStringLiteral("parallel");
        default: throw std::runtime_error("Unhandled enum value");
    }
}

int styleFromString(const QString& name)
{
    if (name == QLatin1String("default"))
        return TStatusbarStorage::Default;
    if (name == QLatin1String("compact"))
        return TStatusbarStorage::Compact;
    if (name == QLatin1String("large"))
        return TStatusbarStorage::Large;
    return name == QLatin1String("parallel") ? TStatusbarStorage::Parallel : TStatusbarStorage::Default;
}

}

TStatusbarStorage::TStatusbarStorage(QObject* parent)
    : cip::tibia::game::TStatusbarStorage(parent)
{
    m_shown.fill(false);
    m_shown[Experience] = true;
}

QString TStatusbarStorage::configurationName() const
{
    return QStringLiteral("statusBarData");
}

QJsonObject TStatusbarStorage::toJson() const
{
    QJsonObject json;
    json.insert(QLatin1String("position"), positionToString(m_position));
    json.insert(QLatin1String("style"), styleToString(m_style));
    for (const Entry entry : StoredEntries)
        json.insert(EntryKeys[entry], m_shown[entry]);
    return json;
}

void TStatusbarStorage::fromJson(const QJsonObject& json)
{
    if (const QJsonValue value = json.value(QLatin1String("position")); value.isString()) {
        m_position = positionFromString(value.toString());
        Q_EMIT configurationChangedFromUserInput();
    }
    if (const QJsonValue value = json.value(QLatin1String("style")); value.isString()) {
        m_style = styleFromString(value.toString());
        Q_EMIT configurationChangedFromUserInput();
    }
    for (const Entry entry : StoredEntries) {
        if (const QJsonValue value = json.value(EntryKeys[entry]); value.isBool()) {
            m_shown[entry] = value.toBool();
            Q_EMIT configurationChangedFromUserInput();
        }
    }
}

void TStatusbarStorage::resetConfiguration()
{
    m_position = Off;
    m_style = Default;
    m_shown.fill(false);
    m_shown[Experience] = true;
    Q_EMIT changed();
}

void TStatusbarStorage::setPosition(const int position)
{
    if (position == m_position || position < Off || position > Right)
        return;
    m_position = position;
    modified();
}

void TStatusbarStorage::setStyle(const int style)
{
    if (style == m_style || style < Default || style > Parallel)
        return;
    m_style = style;
    modified();
}

void TStatusbarStorage::setShown(const Entry entry, const bool shown)
{
    if (m_shown[entry] == shown)
        return;
    m_shown[entry] = shown;
    modified();
}

void TStatusbarStorage::modified()
{
    Q_EMIT changed();
    Q_EMIT configurationChangedFromUserInput();
}

}
