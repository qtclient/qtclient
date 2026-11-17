/*
 * Copyright (c) 2024-2026 qtclient contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once

#include "global.h"

#ifndef USE_PRECOMPILED_HEADERS
#include <memory>
#endif

enum FrameGroupType : uint8_t;
enum ThingCategory : uint8_t;
enum StaticDataCategory : uint8_t;
enum ThingAttr : uint8_t;
enum ThingFlagAttr : uint64_t;
enum STACK_PRIORITY : uint8_t;
enum PLAYER_ACTION : uint8_t;
enum ITEM_CATEGORY : uint8_t;
enum SpriteMask : uint8_t;

// core
class LightView;
class LightSink;
class AnimatedText;
class StaticText;
class ItemType;
class TileBlock;
class AttachedEffect;


using LightViewPtr = std::unique_ptr<LightView>;

namespace tibia::appearances {
class TAppearanceType;
}
using TAppearanceType = tibia::appearances::TAppearanceType;

namespace tibia {
namespace worldmap {
class TWorldMapField;
}
}
using WorldMapFieldPtr = tibia::worldmap::TWorldMapField*;

using AnimatedTextPtr = std::shared_ptr<AnimatedText>;
using StaticTextPtr = std::shared_ptr<StaticText>;
using TAppearanceTypePtr = TAppearanceType*;
using ItemTypePtr = std::shared_ptr<ItemType>;
using AttachedEffectPtr = std::shared_ptr<AttachedEffect>;


using TAppearanceTypeList = std::vector<TAppearanceTypePtr>;
using ItemTypeList = std::vector<ItemTypePtr>;

using WorldMapFieldList = std::vector<tibia::worldmap::TWorldMapField*>;


// ui
class UIItem;
class UIEffect;
class UIMissile;
class UICreature;
class UIGraph;
class UIMap;
class UIMinimap;
class UIProgressRect;
class UIProgressArc;
class UIConditionsPanel;
class UIMapAnchorLayout;
class UIPositionAnchor;
class UISprite;

using UIItemPtr = std::shared_ptr<UIItem>;
using UIEffectPtr = std::shared_ptr<UIEffect>;
using UIMissilePtr = std::shared_ptr<UIMissile>;
using UICreaturePtr = std::shared_ptr<UICreature>;
using UIGraphPtr = std::shared_ptr<UIGraph>;
using UISpritePtr = std::shared_ptr<UISprite>;
using UIMapPtr = std::shared_ptr<UIMap>;
using UIMinimapPtr = std::shared_ptr<UIMinimap>;
using UIProgressRectPtr = std::shared_ptr<UIProgressRect>;
using UIProgressArcPtr = std::shared_ptr<UIProgressArc>;
using UIConditionsPanelPtr = std::shared_ptr<UIConditionsPanel>;
using UIMapAnchorLayoutPtr = std::shared_ptr<UIMapAnchorLayout>;
using UIPositionAnchorPtr = std::shared_ptr<UIPositionAnchor>;
