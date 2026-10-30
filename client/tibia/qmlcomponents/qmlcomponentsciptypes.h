#pragma once

// CIP-named qmlcomponents surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

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

class TAppearanceTypeHelperQmlService final : public cip::tibia::qmlcomponents::TAppearanceTypeHelperQmlService
{
    Q_OBJECT
public:
    using cip::tibia::qmlcomponents::TAppearanceTypeHelperQmlService::TAppearanceTypeHelperQmlService;
};

class TCachedOutlineTextTextureManager final : public cip::tibia::qmlcomponents::TCachedOutlineTextTextureManager
{
    Q_OBJECT
public:
    using cip::tibia::qmlcomponents::TCachedOutlineTextTextureManager::TCachedOutlineTextTextureManager;
};

class TColorHelperQmlService final : public cip::tibia::qmlcomponents::TColorHelperQmlService
{
    Q_OBJECT
public:
    using cip::tibia::qmlcomponents::TColorHelperQmlService::TColorHelperQmlService;
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

class TQmlDynamicAbstractListModel final : public cip::tibia::qmlcomponents::TQmlDynamicAbstractListModel
{
    Q_OBJECT
public:
    using cip::tibia::qmlcomponents::TQmlDynamicAbstractListModel::TQmlDynamicAbstractListModel;
};

class TQmlIncubatorWithCallbackOnReady final : public ::tibia::CipTyped
{
public:
    TQmlIncubatorWithCallbackOnReady() = default;
};

class TQmlJsonSettingsService final : public cip::tibia::qmlcomponents::TQmlJsonSettingsService
{
    Q_OBJECT
public:
    using cip::tibia::qmlcomponents::TQmlJsonSettingsService::TQmlJsonSettingsService;
};

class TQmlSortFilterProxyModel final : public cip::tibia::qmlcomponents::TQmlSortFilterProxyModel
{
    Q_OBJECT
public:
    using cip::tibia::qmlcomponents::TQmlSortFilterProxyModel::TQmlSortFilterProxyModel;
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
