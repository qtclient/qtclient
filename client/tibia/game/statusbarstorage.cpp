#include "statusbarstorage.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>

namespace tibia::game {

namespace {

const std::array<QLatin1StringView, 5> PositionNames = {
    QLatin1StringView("off"), QLatin1StringView("top"), QLatin1StringView("bottom"), QLatin1StringView("left"), QLatin1StringView("right"),
};
const std::array<QLatin1StringView, 4> StyleNames = {
    QLatin1StringView("default"), QLatin1StringView("compact"), QLatin1StringView("large"), QLatin1StringView("parallel"),
};
const std::array<QLatin1StringView, TStatusbarStorage::EntryCount> EntryKeys = {
    QLatin1StringView("showExperience"),   QLatin1StringView("showMagicLevel"),       QLatin1StringView("showAxeFighting"),
    QLatin1StringView("showClubFighting"), QLatin1StringView("showDistanceFighting"), QLatin1StringView("showFistFighting"),
    QLatin1StringView("showShielding"),    QLatin1StringView("showSwordFighting"),    QLatin1StringView("showFishing"),
};

template<size_t N>
int indexOf(const std::array<QLatin1StringView, N>& names, const QString& name)
{
    const auto it = std::find(names.begin(), names.end(), name);
    return it == names.end() ? 0 : static_cast<int>(it - names.begin());
}

}

TStatusbarStorage::TStatusbarStorage(QObject* parent)
    : cip::tibia::game::TStatusbarStorage(parent)
{
    m_shown.fill(false);
    m_shown[Experience] = true;
}

QString TStatusbarStorage::filePath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + QStringLiteral("/characterdata/%1/statusBarData.json").arg(m_characterId);
}

void TStatusbarStorage::loadForCharacter(const quint32 characterId)
{
    if (characterId == m_characterId)
        return;
    save();
    m_characterId = characterId;
    m_root = {};
    if (characterId != 0) {
        QFile file(filePath());
        if (file.open(QIODevice::ReadOnly))
            m_root = QJsonDocument::fromJson(file.readAll()).object();
    }
    m_position = indexOf(PositionNames, m_root.value(QLatin1String("position")).toString());
    m_style = indexOf(StyleNames, m_root.value(QLatin1String("style")).toString());
    for (int i = 0; i < EntryCount; ++i)
        m_shown[i] = m_root.value(EntryKeys[i]).toBool(i == Experience);
    m_dirty = false;
    Q_EMIT changed();
}

void TStatusbarStorage::save()
{
    if (!m_dirty || m_characterId == 0)
        return;
    m_root.insert(QLatin1String("position"), QString(PositionNames[m_position]));
    m_root.insert(QLatin1String("style"), QString(StyleNames[m_style]));
    for (int i = 0; i < EntryCount; ++i)
        m_root.insert(EntryKeys[i], m_shown[i]);
    const QString path = filePath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(m_root).toJson());
        if (file.commit())
            m_dirty = false;
    }
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
    m_dirty = true;
    Q_EMIT changed();
    Q_EMIT configurationChangedFromUserInput();
}

}
