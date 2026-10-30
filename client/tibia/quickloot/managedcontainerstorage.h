#pragma once

#include "client/gamecore.h"

#include <cip_gen.h>

#include <cstdint>
#include <map>
#include <tuple>
#include <vector>

namespace tibia::quickloot {

/// Server-driven quick-loot container assignments (CIP: TManagedContainerStorage,
/// vtable 0x10263c688 family, update 0x100f74e60, fallback 0x100f74fb0).
class TManagedContainerStorage final : public cip::tibia::quickloot::TManagedContainerStorage
{
    Q_OBJECT
public:
    struct Assignment
    {
        uint16_t lootContainerId = 0;
        uint16_t obtainContainerId = 0;
        bool operator==(const Assignment& other) const = default;
    };

    explicit TManagedContainerStorage(QObject* parent = nullptr);
    ~TManagedContainerStorage() override;

    static TManagedContainerStorage* instance();

    bool useMainBackpackAsFallback() const { return m_fallback; }
    /// CIP 0x100f74fb0 + LOOT_CONTAINER_FALLBACK (action 3).
    void setUseMainBackpackAsFallback(bool value);

    Assignment assignment(uint8_t category) const;
    const std::map<uint8_t, Assignment>& assignments() const { return m_assignments; }

private:
    void onContainers(bool fallback, const std::vector<std::tuple<uint8_t, uint16_t, uint16_t>>& list);

    bool m_fallback = false;
    std::map<uint8_t, Assignment> m_assignments;
    std::vector<client::gamecore::Hook> m_hooks;
};

}
