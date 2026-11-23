#include "qmlcomponents/qmlcomponentsciptypes.h"

#include "qmlcomponents/outfitpalette.h"
#include "qmlcomponents/outfittypes.h"
#include "colorhelper.h"

#include <QColor>
#include <tibia/appearances/appearancetype.h>
#include <tibia/game/thingsession_forward.h>

#include <QDebug>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QHoverEvent>
#include <QJSEngine>
#include <QJsonDocument>
#include <QStandardPaths>

#include <algorithm>
#include <cmath>

namespace tibia::qmlcomponents {

/// The bar colour of a health fraction: dark red below 4 %, red below 10 %,
/// light red below 30 %, yellow below 60 %, light green below 95 %, else green.
QColor colorForHealthPercent(const float healthPercentNormalized)
{
    if (healthPercentNormalized < 0.04f)
        return QColor(0x60, 0x00, 0x00);
    if (healthPercentNormalized < 0.1f)
        return QColor(0xC0, 0x00, 0x00);
    if (healthPercentNormalized < 0.3f)
        return QColor(0xC0, 0x30, 0x30);
    if (healthPercentNormalized < 0.6f)
        return QColor(0xC0, 0xC0, 0x00);
    if (healthPercentNormalized < 0.95f)
        return QColor(0x60, 0xC0, 0x60);
    return QColor(0x00, 0xC0, 0x00);
}

namespace {

TAppearanceType* objectType(const int id)
{
    // The id is cut to 16 bit.
    if (!tibia::game::isValidDatId(static_cast<uint16_t>(id), ThingCategoryItem))
        return nullptr;
    return tibia::game::rawThingType(static_cast<uint16_t>(id), ThingCategoryItem);
}

} // namespace

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    (void)parent;
}

TOutfitQmlModel::TOutfitQmlModel(const uint16_t outfitId, const int head, const int torso, const int legs, const int detail,
                                 const bool firstAddOn, const bool secondAddOn, const bool isObjectOutfit, QObject* parent)
    : cip::tibia::qmlcomponents::TOutfitQmlModel(parent)
{
    m_outfitID = outfitId;
    m_colors = new TOutfitColorQmlType(head, torso, legs, detail, this);
    m_firstAddOn = firstAddOn;
    m_secondAddOn = secondAddOn;
    m_isObjectOutfit = isObjectOutfit;
}

QColor TColorHelperQmlService::createColorFromEightBitHSV(const int ColorValue)
{
    return tibia::shared::outfitColor(static_cast<uint8_t>(ColorValue));
}

QColor TColorHelperQmlService::createColorFromEightBitRGB(const int ColorValue)
{
    return tibia::shared::colorFromEightBit(static_cast<uint8_t>(ColorValue));
}

QColor TColorHelperQmlService::createColorForHealthPercent(const float HealthPercentNormalized)
{
    return colorForHealthPercent(HealthPercentNormalized);
}

QColor TColorHelperQmlService::createColorFromName(const QString& Name)
{
    return QColor::fromString(Name);
}

QString TAppearanceTypeHelperQmlService::getObjectAppearanceTypeNameForID(const int ID)
{
    if (TAppearanceType* type = objectType(ID))
        return QString::fromStdString(type->getName());
    return {};
}

QString TAppearanceTypeHelperQmlService::getObjectAppearanceTypeDescriptionForID(const int ID)
{
    if (TAppearanceType* type = objectType(ID))
        return QString::fromStdString(type->getDescription());
    return {};
}

int TAppearanceTypeHelperQmlService::getObjectAppearanceTypeIDByName(const QString& Name)
{
    int found = 0;
    tibia::appearances::TAppearancesManager::instance().forEachAppearanceType([&](const TAppearanceType& type) {
        if (!type.isItem() || type.name().compare(Name, Qt::CaseInsensitive) != 0)
            return true;
        found = static_cast<int>(type.getId());
        return false;
    });
    return found;
}

TQmlDynamicAbstractListModel::TQmlDynamicAbstractListModel(QObject* parent)
    : cip::tibia::qmlcomponents::TQmlDynamicAbstractListModel(parent)
{
}

int TQmlDynamicAbstractListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant TQmlDynamicAbstractListModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_rows.size()))
        return {};
    const int column = role - (Qt::UserRole + 1);
    const QVariantList& row = m_rows[static_cast<size_t>(index.row())];
    if (column < 0 || column >= row.size())
        return {};
    return row.at(column);
}

QHash<int, QByteArray> TQmlDynamicAbstractListModel::roleNames() const
{
    return m_roles;
}

void TQmlDynamicAbstractListModel::setColumns(const QStringList& v)
{
    if (m_columns == v)
        return;
    beginResetModel();
    m_columns = v;
    rebuildRoles();
    endResetModel();
    Q_EMIT columnsChanged();
    Q_EMIT rowCountChanged();
}

QVariantMap TQmlDynamicAbstractListModel::rowData(const int Index)
{
    return rowData(index(Index, 0));
}

/// Every role of the row by its name.
QVariantMap TQmlDynamicAbstractListModel::rowData(const QModelIndex& Index)
{
    QVariantMap row;
    if (!Index.isValid())
        return row;
    const QHash<int, QByteArray> roles = roleNames();
    for (auto it = roles.cbegin(); it != roles.cend(); ++it)
        row[QString::fromUtf8(it.value())] = data(Index, it.key());
    return row;
}

/// One value per column, looked up by the column's name (invalid when missing).
void TQmlDynamicAbstractListModel::appendRow(const QVariantMap& ColumnData)
{
    QVariantList row;
    for (const QString& column : std::as_const(m_columns))
        row.append(ColumnData.value(column));
    const int first = static_cast<int>(m_rows.size());
    beginInsertRows({}, first, first);
    m_rows.push_back(row);
    endInsertRows();
    Q_EMIT rowCountChanged();
}

/// One value per column in order; columns past the array stay invalid.
void TQmlDynamicAbstractListModel::appendRowArray(const QVariantList& RowData)
{
    QVariantList row;
    for (qsizetype i = 0; i < m_columns.size(); ++i)
        row.append(i < RowData.size() ? RowData.at(i) : QVariant());
    const int first = static_cast<int>(m_rows.size());
    beginInsertRows({}, first, first);
    m_rows.push_back(row);
    endInsertRows();
    Q_EMIT rowCountChanged();
}

/// Overwrites the row's leading columns with the array's values; the rest keep theirs.
void TQmlDynamicAbstractListModel::overwriteRowArrayByIndex(const QVariantList& RowData, const int Index)
{
    if (Index < 0 || Index >= static_cast<int>(m_rows.size()))
        return;
    QVariantList& row = m_rows[static_cast<size_t>(Index)];
    for (qsizetype i = 0; i < m_columns.size() && i < RowData.size() && i < row.size(); ++i)
        row[i] = RowData.at(i);
    Q_EMIT dataChanged(index(Index, 0), index(Index, 0));
}

void TQmlDynamicAbstractListModel::removeRowByIndex(const int Index)
{
    if (Index < 0 || Index >= static_cast<int>(m_rows.size()))
        return;
    beginRemoveRows({}, Index, Index);
    m_rows.erase(m_rows.begin() + Index);
    endRemoveRows();
    Q_EMIT rowCountChanged();
}

/// Always reports the count, also when there was nothing to remove.
void TQmlDynamicAbstractListModel::clear()
{
    const int count = rowCount();
    if (count > 0) {
        beginRemoveRows({}, 0, count - 1);
        m_rows.clear();
        endRemoveRows();
    }
    Q_EMIT rowCountChanged();
}

void TQmlDynamicAbstractListModel::rebuildRoles()
{
    m_roles.clear();
    int role = Qt::UserRole + 1;
    for (const QString& column : m_columns)
        m_roles.insert(role++, column.toUtf8());
}

/// The value at a '/'-separated path of nested objects (empty parts skipped);
/// `fallback` when a part is missing.
QJsonValue jsonValueAtPath(const QJsonObject& root, const QString& path, const QJsonValue& fallback)
{
    const QStringList parts = path.split(u'/', Qt::SkipEmptyParts);
    QJsonObject object = root;
    for (qsizetype i = 0; i < parts.size(); ++i) {
        if (!object.contains(parts.at(i)))
            break;
        if (i == parts.size() - 1)
            return object.value(parts.at(i));
        object = object.value(parts.at(i)).toObject();
    }
    return fallback;
}

/// `root` with `value` at a '/'-separated path, creating the objects on the way
/// (a part holding no object becomes one).
QJsonObject jsonObjectWithValueAtPath(const QJsonObject& root, const QString& path, const QJsonValue& value)
{
    const QStringList parts = path.split(u'/', Qt::SkipEmptyParts);
    if (parts.isEmpty())
        return root;
    std::vector<QJsonObject> objects{ root };
    for (qsizetype i = 0; i + 1 < parts.size(); ++i) {
        QJsonObject& top = objects.back();
        if (!top.contains(parts.at(i)))
            top[parts.at(i)] = QJsonObject();
        objects.push_back(top[parts.at(i)].toObject());
    }
    objects.back()[parts.last()] = value;
    QJsonObject result = objects.back();
    objects.pop_back();
    for (qsizetype i = parts.size() - 2; i >= 0; --i) {
        QJsonObject parent = objects.back();
        objects.pop_back();
        parent[parts.at(i)] = result;
        result = parent;
    }
    return result;
}

TQmlJsonSettingsService::TQmlJsonSettingsService(QObject* parent)
    : TQmlJsonSettingsService(QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
                                  .filePath(QStringLiteral("qml-json-settings.json")),
                              parent)
{
}

TQmlJsonSettingsService::TQmlJsonSettingsService(const QString& settingsFilePath, QObject* parent)
    : cip::tibia::qmlcomponents::TQmlJsonSettingsService(parent)
    , m_settingsFilePath(settingsFilePath)
{
    onReloadSettingsFromDisk();
    m_saveJsonTimer.setInterval(500);
    m_saveJsonTimer.setSingleShot(true);
    connect(&m_saveJsonTimer, &QTimer::timeout, this, &TQmlJsonSettingsService::onSaveJsonTimerTimeout);
    m_fileSystemWatcher.addPath(m_settingsFilePath);
    connect(&m_fileSystemWatcher, &QFileSystemWatcher::fileChanged, this, &TQmlJsonSettingsService::onReloadSettingsFromDisk);
}

/// Ignored until the settings were read; a change restarts the save timer.
void TQmlJsonSettingsService::save(const QString& Setting, const QJsonValue& Value)
{
    if (!m_settingsValid)
        return;
    const QJsonObject changed = jsonObjectWithValueAtPath(m_settings.object(), Setting, Value);
    if (changed == m_settings.object())
        return;
    m_settings.setObject(changed);
    Q_EMIT settingsChanged();
    if (m_saveJsonTimer.isActive())
        m_saveJsonTimer.stop();
    m_saveJsonTimer.start();
}

QJsonValue TQmlJsonSettingsService::load(const QString& Setting, const QJsonValue& Default)
{
    if (!m_settingsValid)
        return Default;
    return jsonValueAtPath(m_settings.object(), Setting, Default);
}

/// The watcher stops watching while the file is written, so writing does not reload.
void TQmlJsonSettingsService::onSaveJsonTimerTimeout()
{
    const QStringList watched = m_fileSystemWatcher.files();
    if (!watched.isEmpty())
        m_fileSystemWatcher.removePaths(watched);
    // Host: written through QFile (CipSoft goes through its file-system service).
    QFile file(m_settingsFilePath);
    QDir().mkpath(QFileInfo(file).absolutePath());
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        file.write(m_settings.toJson(QJsonDocument::Indented));
    if (!watched.isEmpty())
        m_fileSystemWatcher.addPaths(watched);
}

/// A missing file starts empty settings; a file that cannot be read or parsed
/// (or is empty) blocks saving until the next reload.
void TQmlJsonSettingsService::onReloadSettingsFromDisk()
{
    qDebug() << "reloading" << m_settingsFilePath;
    const QJsonDocument previous = m_settings;
    if (!QFileInfo(m_settingsFilePath).exists()) {
        qDebug() << m_settingsFilePath << "could not be found. creating new settings file";
        m_settings = QJsonDocument();
        m_settingsValid = true;
    } else {
        QFile file(m_settingsFilePath);
        QJsonParseError error{};
        error.error = QJsonParseError::NoError;
        QJsonDocument read;
        const bool opened = file.open(QIODevice::ReadOnly);
        if (opened)
            read = QJsonDocument::fromJson(file.readAll(), &error);
        m_settings = read;
        m_settingsValid = opened && error.error == QJsonParseError::NoError && !m_settings.isEmpty();
        if (!m_settingsValid)
            qDebug() << m_settingsFilePath << "could not be read or parsed";
    }
    Q_EMIT settingsReloaded();
    if (previous != m_settings)
        Q_EMIT settingsChanged();
}

TQmlSortFilterProxyModel::TQmlSortFilterProxyModel(QObject* parent)
    : cip::tibia::qmlcomponents::TQmlSortFilterProxyModel(parent)
{
    connect(this, &QAbstractItemModel::rowsInserted, this, [this] { refreshCount(); });
    connect(this, &QAbstractItemModel::rowsRemoved, this, [this] { refreshCount(); });
    connect(this, &QAbstractItemModel::modelReset, this, [this] { refreshCount(); });
    connect(this, &QAbstractItemModel::layoutChanged, this, [this] { refreshCount(); });
}

void TQmlSortFilterProxyModel::setFilterCallback(const QJSValue& v)
{
    beginFilterChange();
    m_filterCallback = v;
    endFilterChange(QSortFilterProxyModel::Direction::Both);
    Q_EMIT callbackChanged();
    refreshCount();
}

/// Re-sorts the first column ascending with the new callback.
void TQmlSortFilterProxyModel::setSortCallback(const QJSValue& v)
{
    m_sortCallback = v;
    sort(0, Qt::AscendingOrder);
    Q_EMIT callbackChanged();
    refreshCount();
}

void TQmlSortFilterProxyModel::invalidateFilter(const QSortFilterProxyModel::Direction direction)
{
    beginFilterChange();
    endFilterChange(direction);
}

/// Every role of the proxy row by its name; empty outside the rows.
QVariantMap TQmlSortFilterProxyModel::get(const int index)
{
    QVariantMap row;
    if (index < 0 || index >= rowCount())
        return row;
    const QHash<int, QByteArray> roles = roleNames();
    const QModelIndex proxyIndex = this->index(index, 0);
    for (auto it = roles.cbegin(); it != roles.cend(); ++it)
        row[QString::fromUtf8(it.value())] = data(proxyIndex, it.key());
    return row;
}

bool TQmlSortFilterProxyModel::filterAcceptsRow(const int sourceRow, const QModelIndex& sourceParent) const
{
    if (!m_filterCallback.isCallable())
        return QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);
    QAbstractItemModel* source = sourceModel();
    if (!source)
        return false;
    QVariantMap sourceRowMap;
    const QModelIndex src = source->index(sourceRow, 0, sourceParent);
    const QHash<int, QByteArray> roles = source->roleNames();
    for (auto it = roles.cbegin(); it != roles.cend(); ++it)
        sourceRowMap.insert(QString::fromUtf8(it.value()), source->data(src, it.key()));
    if (QJSEngine* engine = qjsEngine(const_cast<TQmlSortFilterProxyModel*>(this))) {
        const QJSValue arg = engine->toScriptValue(sourceRowMap);
        const QJSValue out = const_cast<QJSValue&>(m_filterCallback).call(QJSValueList{ arg });
        if (out.isBool())
            return out.toBool();
        if (out.isNumber())
            return out.toNumber() != 0;
        return !out.isNull() && !out.isUndefined() && out.toBool();
    }
    return true;
}

bool TQmlSortFilterProxyModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    if (!m_sortCallback.isCallable())
        return QSortFilterProxyModel::lessThan(left, right);
    if (QJSEngine* engine = qjsEngine(const_cast<TQmlSortFilterProxyModel*>(this))) {
        QVariantMap leftMap;
        QVariantMap rightMap;
        if (QAbstractItemModel* source = sourceModel()) {
            const QHash<int, QByteArray> roles = source->roleNames();
            for (auto it = roles.cbegin(); it != roles.cend(); ++it) {
                leftMap.insert(QString::fromUtf8(it.value()), source->data(left, it.key()));
                rightMap.insert(QString::fromUtf8(it.value()), source->data(right, it.key()));
            }
        }
        const QJSValue a = engine->toScriptValue(leftMap);
        const QJSValue b = engine->toScriptValue(rightMap);
        const QJSValue out = const_cast<QJSValue&>(m_sortCallback).call(QJSValueList{ a, b });
        if (out.isNumber())
            return out.toNumber() < 0;
        if (out.isBool())
            return out.toBool();
    }
    return QSortFilterProxyModel::lessThan(left, right);
}

void TQmlSortFilterProxyModel::refreshCount()
{
    const int next = rowCount();
    if (m_count == next)
        return;
    m_count = next;
    Q_EMIT rowCountChanged();
}


} // namespace tibia::qmlcomponents
