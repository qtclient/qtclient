#pragma once

#include "container/containerciptypes.h"

#include <cip_gen.h>

#include <QString>

namespace tibia {
namespace container {

/// What one inventory or container slot shows (CIP: the info ..
/// build, assigned by). Appearance fields only — no ItemPtr.
struct TSlotInfo
{
    uint slotId = 0;
    TInventorySlot appearance;
    QString text;
    bool acceptsDrop = true;
    bool isUpdatable = true;
    int tutorialMarkerId = 0;
    QString managedContainerTooltip;
};

/// The slot text (CIP: TContainerRenderProvider::v2): with
/// `showExpire` (option 0x79 showExpireInInventory) the charges or the time
/// left, then a stack's count above one, then a quiver's ammunition.
/// `whenUnused` is option 0x7a showExpireWhenUnused.
QString slotText(const TInventorySlot& slot, bool showExpire, bool whenUnused);
/// CIP : charges, long and short, unless a brand-new item hides them.
QString chargesText(const TInventorySlot& slot, bool whenUnused);
/// CIP : the time left, long and short.
QString clockText(const TInventorySlot& slot, bool whenUnused);
/// CIP : 9999, then "10k" up to "64k", then "65k+".
QString ammoText(uint total);
/// CIP ("%1min %2s", "%1d %2h %3min") and ("1h05").
QString durationLongText(qint64 seconds);
QString durationShortText(qint64 seconds);
/// CIP : the loot and obtain categories a managed container is
/// set for ("loot_categories_tooltip_caption" then the sorted names joined
/// by <br>, a blank line, the same for obtain); empty for other items.
QString managedContainerTooltip(const TInventorySlot& slot);
/// Whether the item counts down (its text changes as time passes).
bool hasClock(const TInventorySlot& slot);

/// One slot of a container or the inventory, for ContainerSlot.qml (CIP:
/// container::TContainerSlotQmlInformation, ctor).
class TContainerSlotQmlInformation final : public cip::tibia::container::TContainerSlotQmlInformation
{
    Q_OBJECT
public:
    explicit TContainerSlotQmlInformation(QObject* parent = nullptr);

    const TInventorySlot& appearance() const { return m_appearance; }
    bool hasClock() const { return m_appearance.hasClock; }
    void setBackgroundImage(const QString& image);
    /// CIP : takes the info, signalling what changed.
    void assign(const TSlotInfo& info);
    /// CIP : a counting-down item's text, as time passes.
    void setText(const QString& text);

private:
    TInventorySlot m_appearance;
};

}
}
