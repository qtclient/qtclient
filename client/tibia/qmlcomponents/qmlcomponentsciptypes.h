#pragma once

// CIP-named qmlcomponents surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QAbstractListModel>
#include <QColor>
#include <QFileSystemWatcher>
#include <QJsonDocument>
#include <QHash>
#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QSortFilterProxyModel>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>

#include <vector>

namespace tibia {
namespace qmlcomponents {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);

/// The bar colour of a health fraction (0..1).
QColor colorForHealthPercent(float healthPercentNormalized);


class TAppearanceInstanceInfoQmlType final : public cip::tibia::qmlcomponents::TAppearanceInstanceInfoQmlType
{
    Q_OBJECT
public:
    using cip::tibia::qmlcomponents::TAppearanceInstanceInfoQmlType::TAppearanceInstanceInfoQmlType;
};

class TAppearanceInstanceRendererBase final : public cip::tibia::qmlcomponents::TAppearanceInstanceRendererBase
{
    Q_OBJECT
public:
    using cip::tibia::qmlcomponents::TAppearanceInstanceRendererBase::TAppearanceInstanceRendererBase;
};

/// QML singleton AppearanceTypeHelper — item name / description / id lookup.
class TAppearanceTypeHelperQmlService final : public cip::tibia::qmlcomponents::TAppearanceTypeHelperQmlService
{
    Q_OBJECT
public:
    using cip::tibia::qmlcomponents::TAppearanceTypeHelperQmlService::TAppearanceTypeHelperQmlService;

    Q_INVOKABLE QString getObjectAppearanceTypeNameForID(int ID) override;
    Q_INVOKABLE QString getObjectAppearanceTypeDescriptionForID(int ID) override;
    Q_INVOKABLE int getObjectAppearanceTypeIDByName(const QString& Name) override;
};

/// QML singleton ColorHelper — outfit HSV palette, RGB 6³, health, named colors.
class TColorHelperQmlService final : public cip::tibia::qmlcomponents::TColorHelperQmlService
{
    Q_OBJECT
public:
    using cip::tibia::qmlcomponents::TColorHelperQmlService::TColorHelperQmlService;

    Q_INVOKABLE QColor createColorFromEightBitHSV(int ColorValue) override;
    Q_INVOKABLE QColor createColorFromEightBitRGB(int ColorValue) override;
    Q_INVOKABLE QColor createColorForHealthPercent(float HealthPercentNormalized) override;
    Q_INVOKABLE QColor createColorFromName(const QString& Name) override;
};

class TGlobalConstantsService final : public cip::tibia::qmlcomponents::TGlobalConstantsService
{
    Q_OBJECT
public:
    using cip::tibia::qmlcomponents::TGlobalConstantsService::TGlobalConstantsService;
};

/// QML OutfitModel: an outfit (or an object shown as one), its colours and
/// addons.
class TOutfitQmlModel final : public cip::tibia::qmlcomponents::TOutfitQmlModel
{
    Q_OBJECT
public:
    using cip::tibia::qmlcomponents::TOutfitQmlModel::TOutfitQmlModel;
    /// An outfit's id, its four palette colours (owned by the model), its
    /// addons and whether the id is an object's.
    TOutfitQmlModel(uint16_t outfitId, int head, int torso, int legs, int detail, bool firstAddOn, bool secondAddOn, bool isObjectOutfit,
                    QObject* parent = nullptr);
};

class TQSGScenegraphDrawCommandsNode final : public ::tibia::CipTyped
{
public:
    TQSGScenegraphDrawCommandsNode() = default;
};

/// Dynamic column list model for QML (tutorial loot table, etc.).
class TQmlDynamicAbstractListModel : public cip::tibia::qmlcomponents::TQmlDynamicAbstractListModel
{
    Q_OBJECT
public:
    explicit TQmlDynamicAbstractListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const override { return static_cast<int>(m_rows.size()); }
    void setColumns(const QStringList& v) override;

    Q_INVOKABLE QVariantMap rowData(int Index) override;
    Q_INVOKABLE QVariantMap rowData(const QModelIndex& Index) override;
    Q_INVOKABLE void appendRow(const QVariantMap& ColumnData) override;
    Q_INVOKABLE void appendRowArray(const QVariantList& RowData) override;
    Q_INVOKABLE void overwriteRowArrayByIndex(const QVariantList& RowData, int Index) override;
    Q_INVOKABLE void removeRowByIndex(int Index) override;
    Q_INVOKABLE void clear() override;

private:
    void rebuildRoles();

    QHash<int, QByteArray> m_roles;
    /// One value per column, in column order.
    std::vector<QVariantList> m_rows;
};

QJsonValue jsonValueAtPath(const QJsonObject& root, const QString& path, const QJsonValue& fallback);
QJsonObject jsonObjectWithValueAtPath(const QJsonObject& root, const QString& path, const QJsonValue& value);

/// QML JsonSettings: settings in one JSON file, addressed by '/'-separated
/// paths; saved 500 ms after the last change, reloaded when the file changes.
class TQmlJsonSettingsService final : public cip::tibia::qmlcomponents::TQmlJsonSettingsService
{
    Q_OBJECT
public:
    /// Host: QML creates it without a path; it uses the app data location.
    explicit TQmlJsonSettingsService(QObject* parent = nullptr);
    TQmlJsonSettingsService(const QString& settingsFilePath, QObject* parent);

    Q_INVOKABLE void save(const QString& Setting, const QJsonValue& Value) override;
    Q_INVOKABLE QJsonValue load(const QString& Setting, const QJsonValue& Default) override;

private Q_SLOTS:
    void onSaveJsonTimerTimeout();
    void onReloadSettingsFromDisk();

private:
    bool m_settingsValid = false;
    QJsonDocument m_settings;
    QString m_settingsFilePath;
    QTimer m_saveJsonTimer;
    QFileSystemWatcher m_fileSystemWatcher;
};

/// Sort/filter proxy with JS callbacks + row `get(index)` for QML.
class TQmlSortFilterProxyModel : public cip::tibia::qmlcomponents::TQmlSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit TQmlSortFilterProxyModel(QObject* parent = nullptr);

    void setFilterCallback(const QJSValue& v) override;
    void setSortCallback(const QJSValue& v) override;

    Q_INVOKABLE QVariantMap get(int index) override;
    void invalidateFilter(QSortFilterProxyModel::Direction direction = QSortFilterProxyModel::Direction::Rows);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    void refreshCount();
};

class TScenegraphDrawCommandsRendererQuickItem final : public cip::tibia::qmlcomponents::TScenegraphDrawCommandsRendererQuickItem
{
    Q_OBJECT
public:
    using cip::tibia::qmlcomponents::TScenegraphDrawCommandsRendererQuickItem::TScenegraphDrawCommandsRendererQuickItem;
};

class TSimpleTextureMaterial final : public ::tibia::CipTyped
{
public:
    TSimpleTextureMaterial() = default;
};

class TSimpleTextureShader final : public ::tibia::CipTyped
{
public:
    TSimpleTextureShader() = default;
};

class TSimpleTextureShaderScenegraphNode final : public ::tibia::CipTyped
{
public:
    TSimpleTextureShaderScenegraphNode() = default;
};

class TTargetSelectionQuickItem final : public cip::tibia::qmlcomponents::TTargetSelectionQuickItem
{
    Q_OBJECT
public:
    using cip::tibia::qmlcomponents::TTargetSelectionQuickItem::TTargetSelectionQuickItem;
};

class TTextureAtlasViewer final : public cip::tibia::qmlcomponents::TTextureAtlasViewer
{
    Q_OBJECT
public:
    using cip::tibia::qmlcomponents::TTextureAtlasViewer::TTextureAtlasViewer;
};

class TTextureAtlasViewerController final : public cip::tibia::qmlcomponents::TTextureAtlasViewerController
{
    Q_OBJECT
public:
    using cip::tibia::qmlcomponents::TTextureAtlasViewerController::TTextureAtlasViewerController;
};

/// CIP QML type name (product camera type: `TWorldMapCamera` in minimaprendererquickitem.h).
using TWorldMapCameraQmlType = cip::tibia::qmlcomponents::TWorldMapCameraQmlType;

}
}
