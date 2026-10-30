#pragma once

#include <cip_gen.h>

#include <algorithm>
#include <map>

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>

namespace tibia::config {

/// TSidebarPanelsManager's panel types (sidebars.json names "minimap",
/// "inventoryStatus", "hitpointManabar", "buttons"; CIP: FUN_1009fc600).
enum class SidebarPanelType { Minimap = 0, InventoryStatus = 1, HitpointManabar = 2, Buttons = 3 };

/// sidebars.json's sidebarWidgetsMangerOptions: one widget list per sidebar,
/// the right sidebars (ids 0..) first, then the left ones (ids 100..).
struct SidebarWidgetsManagerOptions
{
    int leftSidebarCount = 0;
    QJsonArray openWidgetsOrderPerSidebar;

    int rightSidebarCount() const { return std::max<int>(1, static_cast<int>(openWidgetsOrderPerSidebar.size()) - leftSidebarCount); }
    bool operator==(const SidebarWidgetsManagerOptions&) const = default;
};

/// sidebars.json's sidebarPanelsOptions (CIP: FUN_1009e8b50 / FUN_1009e8c80).
struct SidebarPanelsOptions
{
    /// Minimap zoom exponent, -1 to 2; stored as round(zoom * 100).
    float minimapZoomLevel = 0;
    bool controlButtonsVisible = false;
    bool minimizeInventory = false;
    bool chaseModeEnabled = true;

    bool operator==(const SidebarPanelsOptions&) const = default;
};

/// One entry of sidebars.json's containersOptions, keyed by container id
/// (CIP: FUN_100a14ae0, read by FUN_100a14ac0 / FUN_100a14ad0).
struct ContainerWidgetOptions
{
    int contentHeight = 0;
    bool contentMaximized = true;

    bool operator==(const ContainerWidgetOptions&) const = default;
};
using ContainersOptions = std::map<int, ContainerWidgetOptions>;

/// A character's sidebar configuration (CIP: TSidebarOptions), kept in
/// <app data>/characterdata/<character id>/sidebars.json. Only the sections
/// the client uses are parsed; the rest of the file is written back as read.
/// Like CIP's TCharacterConfigurationManager, changes are written when the
/// session ends, not as they happen: CIP's onConfigurationChanged
/// (0x100a11a80) only marks the file dirty, and save (0x100a12350) runs when
/// the next character loads (0x100a13250) and on destruction, so a killed
/// client loses them.
class TSidebarOptions final : public cip::tibia::config::TSidebarOptions
{
    Q_OBJECT
public:
    explicit TSidebarOptions(QObject* parent = nullptr);

    /// Reads the character's file (saving the previous character's first).
    void loadForCharacter(quint32 characterId);
    /// Writes the file if anything changed since it was read.
    void save();

    const SidebarPanelsOptions& sidebarPanelsOptions() const { return m_panels; }
    void setSidebarPanelsOptions(const SidebarPanelsOptions& options);

    /// sidebarPanelsMangerOptions.panelsOrder; empty is the default order.
    const QList<SidebarPanelType>& panelsOrder() const { return m_panelsOrder; }
    void setPanelsOrder(const QList<SidebarPanelType>& order);

    const SidebarWidgetsManagerOptions& sidebarWidgetsManagerOptions() const { return m_widgetsManager; }
    void setSidebarWidgetsManagerOptions(const SidebarWidgetsManagerOptions& options);

    /// Signals optionsForAllContainersChanged when a character's file is read.
    const ContainersOptions& containersOptions() const { return m_containers; }
    void setContainersOptions(const ContainersOptions& options);

    /// A top-level section its widget parses itself (battleListsOptions,
    /// skillsWidgetOptions, ...), as read from the file.
    QJsonValue section(const QString& key) const { return m_root.value(key); }
    void setSection(const QString& key, const QJsonValue& value);

private:
    QString filePath() const;
    void assign(const SidebarPanelsOptions& options);
    void changed();

    quint32 m_characterId = 0;
    QJsonObject m_root;
    SidebarPanelsOptions m_panels;
    QList<SidebarPanelType> m_panelsOrder;
    SidebarWidgetsManagerOptions m_widgetsManager;
    ContainersOptions m_containers;
    bool m_dirty = false;
};

}
