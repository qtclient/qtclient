#pragma once

// Handwritten classes QML instantiates instead of the generated cip:: bases
// (included by the generated cip_registration.cpp).

#include "qmlcomponents/outlinetext.h"
#include "renderer/renderdriver.h"

CIP_IMPLEMENT(::cip::tibia::qmlcomponents::TRenderDriver, ::tibia::qmlcomponents::TRenderDriver);
CIP_IMPLEMENT(::cip::tibia::qmlcomponents::TOutlineTextQuickItem, ::tibia::qmlcomponents::TOutlineTextQuickItem);
CIP_IMPLEMENT(::cip::tibia::qmlcomponents::TCachedOutlineTextQuickItem, ::tibia::qmlcomponents::TCachedOutlineTextQuickItem);

#include "qmlcomponents/outfittypes.h"
CIP_IMPLEMENT(::cip::tibia::qmlcomponents::TOutfitColorQmlType, ::tibia::qmlcomponents::TOutfitColorQmlType);
CIP_IMPLEMENT(::cip::tibia::qmlcomponents::TOutfitPresetQmlType, ::tibia::qmlcomponents::TOutfitPresetQmlType);
CIP_IMPLEMENT(::cip::tibia::qmlcomponents::TTibiaObjectCreatorQmlService, ::tibia::qmlcomponents::TTibiaObjectCreatorQmlService);
CIP_IMPLEMENT(::cip::tibia::qmlcomponents::TClipboardHelperQmlService, ::tibia::qmlcomponents::TClipboardHelperQmlService);
CIP_IMPLEMENT(::cip::tibia::qmlcomponents::TAbstractItemModelHelper, ::tibia::qmlcomponents::TAbstractItemModelHelper);

#include "qmlcomponents/texthelper.h"
CIP_IMPLEMENT(::cip::tibia::qmlcomponents::TTextHelperQmlService, ::tibia::qmlcomponents::TTextHelperQmlService);

#include "qmlcomponents/tooltiphelper.h"
CIP_IMPLEMENT(::cip::tibia::qmlcomponents::TTooltipHelperQmlService, ::tibia::qmlcomponents::TTooltipHelperQmlService);

#include "qmlcomponents/tutorialhelper.h"
CIP_IMPLEMENT(::cip::tibia::qmlcomponents::TTutorialHelperQmlService, ::tibia::qmlcomponents::TTutorialHelperQmlService);
CIP_IMPLEMENT(::cip::tibia::qmlcomponents::TTutorialMarkerQuickItem, ::tibia::qmlcomponents::TTutorialMarkerQuickItem);

#ifdef TIBIA_WITH_GAMECORE
#include "qmlcomponents/soundhelper.h"
CIP_IMPLEMENT(::cip::tibia::qmlcomponents::TSoundHelperService, ::tibia::qmlcomponents::TSoundHelperService);

#include "qmlcomponents/minimapquickitem.h"
#include "qmlcomponents/minimaprendererquickitem.h"
#include "qmlcomponents/numericaleffectoverlay.h"
#include "qmlcomponents/singleobjectrenderer.h"
#include "qmlcomponents/speechbubbleoverlay.h"
#include "qmlcomponents/worldmapcameraviewport.h"
#include "qmlcomponents/worldmapqmltypes.h"
#include "renderer/lightmappainteditem.h"
#include "renderer/worldmapquickitem.h"

CIP_IMPLEMENT(::cip::tibia::qmlcomponents::TMinimapQuickItem, ::tibia::qmlcomponents::TMinimapQuickItem);
CIP_IMPLEMENT(::cip::tibia::qmlcomponents::TMinimapRendererQuickItem, ::tibia::qmlcomponents::TMinimapRendererQuickItem);
CIP_IMPLEMENT(::cip::tibia::qmlcomponents::TWorldMapCameraViewportQmlType, ::tibia::qmlcomponents::TWorldMapCameraViewport);
CIP_IMPLEMENT(::cip::tibia::qmlcomponents::TWorldMapCoordinateQmlType, ::tibia::qmlcomponents::TWorldMapCoordinateQmlType);
CIP_IMPLEMENT(::cip::tibia::qmlcomponents::TWorldMapCuboidQmlType, ::tibia::qmlcomponents::TWorldMapCuboidQmlType);
CIP_IMPLEMENT(::cip::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType,
              ::tibia::qmlcomponents::TWorldMapSubfieldCoordinateQmlType);
CIP_IMPLEMENT(::cip::tibia::qmlcomponents::TNumericalEffectOverlayQuickItem, ::tibia::qmlcomponents::TNumericalEffectOverlayQuickItem);
CIP_IMPLEMENT(::cip::tibia::qmlcomponents::TSingleObjectAppearanceInstanceRenderer, ::tibia::qmlcomponents::TSingleObjectAppearanceInstanceRenderer);
CIP_IMPLEMENT(::cip::tibia::qmlcomponents::TSpeechBubbleOverlayQuickItem, ::tibia::qmlcomponents::TSpeechBubbleOverlayQuickItem);
CIP_IMPLEMENT(::cip::tibia::qmlcomponents::TWorldMapQuickItem, ::tibia::qmlcomponents::TWorldMapQuickItem);
CIP_IMPLEMENT(::cip::tibia::qmlcomponents::TLightmapPaintedItem, ::tibia::qmlcomponents::TLightmapPaintedItem);

#include "qmlcomponents/appearanceinstancerenderer.h"
CIP_IMPLEMENT(::cip::tibia::qmlcomponents::TAppearanceInstanceRenderer, ::tibia::qmlcomponents::TAppearanceInstanceRenderer);
CIP_IMPLEMENT(::cip::tibia::qmlcomponents::TObjectAppearanceInstanceInfoQmlType, ::tibia::qmlcomponents::TObjectAppearanceInstanceInfoQmlType);
CIP_IMPLEMENT(::cip::tibia::qmlcomponents::TOutfitAppearanceInstanceQmlType, ::tibia::qmlcomponents::TOutfitAppearanceInstanceQmlType);

#include "store/dynamicallyloadedimage.h"
CIP_IMPLEMENT(::cip::tibia::qmlcomponents::TDynamicallyLoadedImageQuickItem, ::tibia::qmlcomponents::TDynamicallyLoadedImageQuickItem);
#endif
