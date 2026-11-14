#include "qmlcomponents/qmlcomponentsciptypes.h"

#include "qmlcomponents/outfitpalette.h"

#include <framework/util/color.h>
#include <tibia/assets/thingtype.h>
#include <tibia/game/thingsession_forward.h>

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

namespace {

QColor qcolorFromFramework(const Color& c)
{
    return QColor::fromRgbF(c.rF(), c.gF(), c.bF(), c.aF());
}

/// Same thresholds as TCreature::informationColorForHealthPercent (0..100).
QColor healthColor(const uint8_t healthPercent)
{
    static constexpr Color
        COLOR1(0x00, 0xBC, 0x00),
        COLOR2(0x50, 0xA1, 0x50),
        COLOR3(0xA1, 0xA1, 0x00),
        COLOR4(0xBF, 0x0A, 0x0A),
        COLOR5(0x91, 0x0F, 0x0F),
        COLOR6(0x85, 0x0C, 0x0C);
    if (healthPercent > 92)
        return qcolorFromFramework(COLOR1);
    if (healthPercent > 60)
        return qcolorFromFramework(COLOR2);
    if (healthPercent > 30)
        return qcolorFromFramework(COLOR3);
    if (healthPercent > 8)
        return qcolorFromFramework(COLOR4);
    if (healthPercent > 3)
        return qcolorFromFramework(COLOR5);
    return qcolorFromFramework(COLOR6);
}

ThingType* objectType(const int id)
{
    if (id <= 0 || !tibia::game::isValidDatId(static_cast<uint16_t>(id), ThingCategoryItem))
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

QColor TColorHelperQmlService::createColorFromEightBitHSV(const int ColorValue)
{
    return outfitpalette::color(ColorValue);
}

QColor TColorHelperQmlService::createColorFromEightBitRGB(const int ColorValue)
{
    return qcolorFromFramework(Color::from8bit(ColorValue));
}

QColor TColorHelperQmlService::createColorForHealthPercent(const float HealthPercentNormalized)
{
    const float clamped = std::clamp(HealthPercentNormalized, 0.f, 1.f);
    return healthColor(static_cast<uint8_t>(std::lround(clamped * 100.f)));
}

QColor TColorHelperQmlService::createColorFromName(const QString& Name)
{
    return QColor(Name);
}

QString TAppearanceTypeHelperQmlService::getObjectAppearanceTypeNameForID(const int ID)
{
    if (ThingType* type = objectType(ID))
        return QString::fromStdString(type->getName());
    return {};
}

QString TAppearanceTypeHelperQmlService::getObjectAppearanceTypeDescriptionForID(const int ID)
{
    if (ThingType* type = objectType(ID))
        return QString::fromStdString(type->getDescription());
    return {};
}

int TAppearanceTypeHelperQmlService::getObjectAppearanceTypeIDByName(const QString& Name)
{
    if (Name.isEmpty())
        return 0;
    const QString needle = Name.trimmed();
    for (const ThingTypePtr& type : tibia::game::thingTypes(ThingCategoryItem)) {
        if (!type || type->getId() == 0)
            continue;
        if (QString::fromStdString(type->getName()).compare(needle, Qt::CaseInsensitive) == 0)
            return static_cast<int>(type->getId());
    }
    return 0;
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
    const auto it = m_roles.constFind(role);
    if (it == m_roles.cend())
        return {};
    return m_rows[static_cast<size_t>(index.row())].value(QString::fromUtf8(it.value()));
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
    return mapAt(Index);
}

QVariantMap TQmlDynamicAbstractListModel::rowData(const QModelIndex& Index)
{
    return Index.isValid() ? mapAt(Index.row()) : QVariantMap{};
}

void TQmlDynamicAbstractListModel::appendRow(const QVariantMap& ColumnData)
{
    const int row = static_cast<int>(m_rows.size());
    beginInsertRows({}, row, row);
    m_rows.push_back(ColumnData);
    m_count = static_cast<int>(m_rows.size());
    endInsertRows();
    Q_EMIT rowCountChanged();
}

void TQmlDynamicAbstractListModel::appendRowArray(const QVariantList& RowData)
{
    QVariantMap map;
    for (int i = 0; i < m_columns.size() && i < RowData.size(); ++i)
        map.insert(m_columns.at(i), RowData.at(i));
    appendRow(map);
}

void TQmlDynamicAbstractListModel::overwriteRowArrayByIndex(const QVariantList& RowData, const int Index)
{
    if (Index < 0 || Index >= static_cast<int>(m_rows.size()))
        return;
    QVariantMap map;
    for (int i = 0; i < m_columns.size() && i < RowData.size(); ++i)
        map.insert(m_columns.at(i), RowData.at(i));
    m_rows[static_cast<size_t>(Index)] = map;
    const QModelIndex idx = index(Index, 0);
    Q_EMIT dataChanged(idx, idx);
}

void TQmlDynamicAbstractListModel::removeRowByIndex(const int Index)
{
    if (Index < 0 || Index >= static_cast<int>(m_rows.size()))
        return;
    beginRemoveRows({}, Index, Index);
    m_rows.erase(m_rows.begin() + Index);
    m_count = static_cast<int>(m_rows.size());
    endRemoveRows();
    Q_EMIT rowCountChanged();
}

void TQmlDynamicAbstractListModel::clear()
{
    if (m_rows.empty())
        return;
    beginResetModel();
    m_rows.clear();
    m_count = 0;
    endResetModel();
    Q_EMIT rowCountChanged();
}

void TQmlDynamicAbstractListModel::rebuildRoles()
{
    m_roles.clear();
    int role = Qt::UserRole + 1;
    for (const QString& column : m_columns)
        m_roles.insert(role++, column.toUtf8());
}

QVariantMap TQmlDynamicAbstractListModel::mapAt(const int row) const
{
    if (row < 0 || row >= static_cast<int>(m_rows.size()))
        return {};
    return m_rows[static_cast<size_t>(row)];
}

TQmlJsonSettingsService::TQmlJsonSettingsService(QObject* parent)
    : cip::tibia::qmlcomponents::TQmlJsonSettingsService(parent)
{
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(250);
    connect(&m_saveTimer, &QTimer::timeout, this, &TQmlJsonSettingsService::onSaveJsonTimerTimeout);
    onReloadSettingsFromDisk();
}

void TQmlJsonSettingsService::save(const QString& Setting, const QJsonValue& Value)
{
    if (Setting.isEmpty())
        return;
    m_root.insert(Setting, Value);
    scheduleSave();
    Q_EMIT settingsChanged();
}

QJsonValue TQmlJsonSettingsService::load(const QString& Setting, const QJsonValue& Default)
{
    if (Setting.isEmpty() || !m_root.contains(Setting))
        return Default;
    return m_root.value(Setting);
}

void TQmlJsonSettingsService::onSaveJsonTimerTimeout()
{
    QFile file(filePath());
    QDir().mkpath(QFileInfo(file).absolutePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    file.write(QJsonDocument(m_root).toJson(QJsonDocument::Compact));
}

void TQmlJsonSettingsService::onReloadSettingsFromDisk()
{
    QFile file(filePath());
    if (file.open(QIODevice::ReadOnly)) {
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        m_root = doc.isObject() ? doc.object() : QJsonObject{};
    } else {
        m_root = {};
    }
    Q_EMIT settingsReloaded();
}

QString TQmlJsonSettingsService::filePath() const
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath(QStringLiteral("qml-json-settings.json"));
}

void TQmlJsonSettingsService::scheduleSave()
{
    m_saveTimer.start();
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
    cip::tibia::qmlcomponents::TQmlSortFilterProxyModel::setFilterCallback(v);
    invalidateFilter();
    refreshCount();
}

void TQmlSortFilterProxyModel::setSortCallback(const QJSValue& v)
{
    cip::tibia::qmlcomponents::TQmlSortFilterProxyModel::setSortCallback(v);
    invalidate();
    refreshCount();
}

QVariantMap TQmlSortFilterProxyModel::get(const int index)
{
    return rowMap(this->index(index, 0));
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

QVariantMap TQmlSortFilterProxyModel::rowMap(const QModelIndex& index) const
{
    QVariantMap row;
    if (!index.isValid())
        return row;
    const QAbstractItemModel* model = index.model();
    if (!model)
        return row;
    const QHash<int, QByteArray> roles = model->roleNames();
    for (auto it = roles.cbegin(); it != roles.cend(); ++it)
        row.insert(QString::fromUtf8(it.value()), model->data(index, it.key()));
    return row;
}

void TLenshelpQuickItem::hoverEnterEvent(QHoverEvent* event)
{
    cip::tibia::qmlcomponents::TLenshelpQuickItem::hoverEnterEvent(event);
}

void TLenshelpQuickItem::hoverLeaveEvent(QHoverEvent* event)
{
    cip::tibia::qmlcomponents::TLenshelpQuickItem::hoverLeaveEvent(event);
}

bool TNotMouseMoveEventFilter::eventFilter(QObject* watched, QEvent* event)
{
    return cip::tibia::qmlcomponents::TNotMouseMoveEventFilter::eventFilter(watched, event);
}

} // namespace tibia::qmlcomponents
