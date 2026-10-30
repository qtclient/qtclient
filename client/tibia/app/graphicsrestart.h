#pragma once

#include <QString>

namespace tibia::app {

/// CipSoft keys under conf/clientoptions.json "options" for CIP's graphics-engine
/// restart / keep-renderer flow (keep_renderer_after_restart).
namespace graphicsrestart {

constexpr auto PendingConfirmKey = "graphicsEnginePendingConfirm";
constexpr auto PreviousRendererKey = "graphicsEnginePreviousRendererIndex";
constexpr auto PreviousVSyncKey = "graphicsEnginePreviousVSync";
constexpr auto PreviousFreetypeKey = "graphicsEnginePreviousFreetype";

/// CIP renderer display name for keep_renderer_after_restart's %1.
QString rendererDisplayName(int rendererIndex);

bool pendingConfirm();
/// Snapshot the live graphics trio before Apply overwrites them, and mark that
/// the next process start should ask keep_renderer_after_restart.
void markPendingConfirm(int previousRenderer, bool previousVSync, bool previousFreetype);
void clearPendingConfirm();
/// Restore the previous renderer/vsync/freetype and clear the flag.
void revertPending();

}

}
