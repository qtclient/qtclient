#pragma once

// CIP-named qmlcomponents surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QAbstractListModel>
#include <QColor>
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

namespace tibia::qmlcomponents {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


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

class TCachedOutlineTextTextureManager final : public cip::tibia::qmlcomponents::TCachedOutlineTextTextureManager
{
    Q_OBJECT
public:
    using cip::tibia::qmlcomponents::TCachedOutlineTextTextureManager::TCachedOutlineTextTextureManager;
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

class TLenshelpManager final : public cip::tibia::qmlcomponents::TLenshelpManager
{
    Q_OBJECT
public:
    using cip::tibia::qmlcomponents::TLenshelpManager::TLenshelpManager;
};

class TLenshelpQuickItem final : public cip::tibia::qmlcomponents::TLenshelpQuickItem
{
    Q_OBJECT
public:
    using cip::tibia::qmlcomponents::TLenshelpQuickItem::TLenshelpQuickItem;
};

class TNotMouseMoveEventFilter final : public cip::tibia::qmlcomponents::TNotMouseMoveEventFilter
{
    Q_OBJECT
public:
    using cip::tibia::qmlcomponents::TNotMouseMoveEventFilter::TNotMouseMoveEventFilter;
};

class TOffscreenOutlineTextToImageService final : public cip::tibia::qmlcomponents::TOffscreenOutlineTextToImageService
{
    Q_OBJECT
public:
    using cip::tibia::qmlcomponents::TOffscreenOutlineTextToImageService::TOffscreenOutlineTextToImageService;
};

class TOutfitQmlModel final : public cip::tibia::qmlcomponents::TOutfitQmlModel
{
    Q_OBJECT
public:
    using cip::tibia::qmlcomponents::TOutfitQmlModel::TOutfitQmlModel;
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
    QVariantMap mapAt(int row) const;

    QHash<int, QByteArray> m_roles;
    std::vector<QVariantMap> m_rows;
};

class TQmlIncubatorWithCallbackOnReady final : public ::tibia::CipTyped
{
public:
    TQmlIncubatorWithCallbackOnReady() = default;
};

/// Debounced JSON key/value store (CIP TQmlJsonSettingsService).
class TQmlJsonSettingsService final : public cip::tibia::qmlcomponents::TQmlJsonSettingsService
{
    Q_OBJECT
public:
    explicit TQmlJsonSettingsService(QObject* parent = nullptr);

    Q_INVOKABLE void save(const QString& Setting, const QJsonValue& Value) override;
    Q_INVOKABLE QJsonValue load(const QString& Setting, const QJsonValue& Default) override;

private Q_SLOTS:
    void onSaveJsonTimerTimeout();
    void onReloadSettingsFromDisk();

private:
    QString filePath() const;
    void scheduleSave();

    QJsonObject m_root;
    QTimer m_saveTimer;
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

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    void refreshCount();
    QVariantMap rowMap(const QModelIndex& index) const;
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

} // namespace tibia::qmlcomponents
