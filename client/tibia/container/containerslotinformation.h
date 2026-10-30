#pragma once

#include <cip_gen.h>

#include <client/declarations.h>

#include <QString>

namespace tibia::container {

/// What one inventory or container slot shows (CIP: the info 0x100a362c0..
/// 0x100a36390 build, assigned by 0x100a35750).
struct TSlotInfo
{
    uint slotId = 0;
    ItemPtr object;
    QString text;
    bool acceptsDrop = true;
    bool isUpdatable = true;
    int tutorialMarkerId = 0;
    QString managedContainerTooltip;
};

/// The slot text (CIP: TContainerRenderProvider::v2 0x100b0c7b0): with
/// `showExpire` (option 0x79 showExpireInInventory) the charges or the time
/// left, then a stack's count above one, then a quiver's ammunition.
/// `whenUnused` is option 0x7a showExpireWhenUnused.
QString slotText(const ItemPtr& item, bool showExpire, bool whenUnused);
/// CIP 0x100962970: charges, long and short, unless a brand-new item hides them.
QString chargesText(const ItemPtr& item, bool whenUnused);
/// CIP 0x100962eb0: the time left, long and short.
QString clockText(const ItemPtr& item, bool whenUnused);
/// CIP 0x100960950: 9999, then "10k" up to "64k", then "65k+".
QString ammoText(uint total);
/// CIP 0x10095f080 ("%1min %2s", "%1d %2h %3min") and 0x10095f0d0 ("1h05").
QString durationLongText(qint64 seconds);
QString durationShortText(qint64 seconds);
/// CIP 0x100a363f0: the loot and obtain categories a managed container is
/// set for ("loot_categories_tooltip_caption" then the sorted names joined
/// by <br>, a blank line, the same for obtain); empty for other items.
QString managedContainerTooltip(const ItemPtr& item);
/// Whether the item counts down (its text changes as time passes).
bool hasClock(const ItemPtr& item);

/// One slot of a container or the inventory, for ContainerSlot.qml (CIP:
/// container::TContainerSlotQmlInformation, ctor 0x100a355a0).
class TContainerSlotQmlInformation final : public cip::tibia::container::TContainerSlotQmlInformation
{
    Q_OBJECT
public:
    explicit TContainerSlotQmlInformation(QObject* parent = nullptr);

    const ItemPtr& object() const { return m_object; }
    void setBackgroundImage(const QString& image);
    /// CIP 0x100a35750: takes the info, signalling what changed.
    void assign(const TSlotInfo& info);
    /// CIP 0x100a35bb0: a counting-down item's text, as time passes.
    void setText(const QString& text);

private:
    ItemPtr m_object;
};

}
