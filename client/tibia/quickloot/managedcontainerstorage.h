#pragma once

#include <cip_gen.h>

#include <cstdint>
#include <map>

namespace tibia {
namespace quickloot {

/// The server's quick-loot container assignments per object category and
/// whether loot falls back to the main backpack.
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
    /// Keyed by object category.
    using Assignments = std::map<int, Assignment>;

    explicit TManagedContainerStorage(QObject* parent = nullptr);
    ~TManagedContainerStorage() override;

    static TManagedContainerStorage* instance();

    /// Replaces the assignments; notifies only when they differ.
    void setManagedContainers(const Assignments& assignments);
    Assignments managedContainers() const { return m_assignments; }
    /// Stores the flag and always notifies.
    void setUseMainBackpackAsFallback(bool value);
    bool useMainBackpackAsFallback() const { return m_fallback; }

    Assignment assignment(int category) const;

private:
    Assignments m_assignments;
    bool m_fallback = false;
};

}
}
