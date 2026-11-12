#include "renderer/texturedeletionqueue.h"

#include <atomic>
#include <mutex>
#include <vector>

namespace tibia {
namespace renderer {
namespace {

struct PendingTextureDeletion
{
    uint32_t id;       // OpenGL name; 0 when the texture never reached a GL context
    uint32_t uniqueId; // process-wide identity, which every texture has on every backend
    bool smooth;
};

// Tip-owned queue (formerly engine Texture TU `g_pendingDeletions` /
// `g_pendingDeletionMutex`). Renamed off the OTC `g_*` hub scan.
std::mutex pendingDeletionMutex;
std::vector<PendingTextureDeletion> pendingDeletions;

// Tip-infra diagnostics — atomic counts beside the queue; retirement rides on
// the peel above. Kept as `g_*` names intentionally (catalogued qtclient-infra).
std::atomic_size_t g_deletedTextures{ 0 };
std::atomic_size_t g_deletionBatches{ 0 };

} // namespace

void enqueueTextureDeletion(const uint32_t glId, const uint32_t uniqueId, const bool smooth)
{
    std::scoped_lock lock(pendingDeletionMutex);
    pendingDeletions.emplace_back(glId, uniqueId, smooth);
}

void flushTextureDeletions()
{
    // Tip host never inits OTC Soft Graphics / GL backends — drain is a queue
    // clear (Wave 3 R2). Framework Texture TU still owns real GL destroy if
    // ticket-9b3-framework-graphics is revived on a GL host.
    std::scoped_lock lock(pendingDeletionMutex);
    if (pendingDeletions.empty())
        return;

    g_deletionBatches.fetch_add(1, std::memory_order_relaxed);
    pendingDeletions.clear();
}

size_t deletedTextureCount()
{
    return g_deletedTextures.load(std::memory_order_relaxed);
}

size_t textureDeletionBatchCount()
{
    return g_deletionBatches.load(std::memory_order_relaxed);
}

}
}
