#pragma once

#include <cip_gen.h>

#include <client/declarations.h>

#include <vector>

namespace tibia::gamewindow {

/// A container's slots for container.qml's grid (CIP:
/// TAppearanceInstanceListModel, vtable 0x10262c5e8, data FUN_100e09a00):
/// its objects, then empty slots up to the minimum row count. Loot-value
/// frames and managed-container tooltips follow TItemInfoStorage /
/// container::managedContainerTooltip.
class TAppearanceInstanceListModel final : public cip::tibia::gamewindow::TAppearanceInstanceListModel
{
    Q_OBJECT
public:
    enum Role
    {
        TypeIdRole = Qt::UserRole + 1,
        CumulativeCountRole,
        TypeCountRole,
        CountStringRole,
        AlternativeSlotStringRole,
        LiquidTypeRole,
        HookDirectionRole,
        LootValueImageUnderItemRole,
        LootValueImageOverItemRole,
        ManagedContainerTooltipRole,
        UpgradeTierRole,
        DecoItemObjectIdRole,
    };

    explicit TAppearanceInstanceListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /// CIP FUN_100d6f290: every row goes and comes back.
    void setMinimumRows(int rows);
    /// CIP FUN_100d6f370: rows come and go at the end, and every row is
    /// signalled as changed.
    void setObjects(std::vector<ItemPtr> objects);
    const std::vector<ItemPtr>& objects() const { return m_objects; }
    /// Options showExpireInContainers and showExpireWhenUnused.
    void setExpireOptions(bool showExpire, bool whenUnused);
    /// The slots whose text counts down, as time passes.
    void refreshClocks();
    /// Re-read loot-value frames after prices or colorizeLootStyle change.
    void refreshLootValues();

private:
    std::vector<ItemPtr> m_objects;
    int m_minimumRows = 0;
    bool m_showExpire = true;
    bool m_showExpireWhenUnused = false;
};

}
