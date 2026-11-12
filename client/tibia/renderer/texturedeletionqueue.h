#pragma once

/// Tip renderer façade for the OTC batched GL texture-deletion queue (Wave 2 hub peel).
/// Call sites prefer this over engine `Texture` TU-local `g_pendingDeletions*`.
/// No CipSoft contract twin — do not invent a fake CipSoft `T*` name.

#include <cstddef>
#include <cstdint>

namespace tibia {
namespace renderer {

/// Queue a texture for GL/backend teardown. Safe from any thread; actual delete
/// runs on the main/GL thread via `flushTextureDeletions()`.
void enqueueTextureDeletion(uint32_t glId, uint32_t uniqueId, bool smooth);

/// Drain the pending queue with one batched `glDeleteTextures` (or backend
/// destroy). Call exclusively where an active GL/RHI context exists.
void flushTextureDeletions();

size_t deletedTextureCount();
size_t textureDeletionBatchCount();

}
}
