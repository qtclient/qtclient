#pragma once

#include <cip_gen.h>

#include <QString>

namespace tibia {
namespace container {

/// A count as Qt length variants (U+009C separated): the English-grouped
/// number, the plain number when it differs, then "Nk" from 1000 and "Nkk"
/// from 1000000.
QString countVariants(uint count);

/// An object type and count shown in an item slot (CIP:
/// container::TQMLItemSlotData). The displayed type starts as the type and
/// can be swapped; the loot value frame images are set by the owner.
class TQMLItemSlotData final : public cip::tibia::container::TQMLItemSlotData
{
    Q_OBJECT
public:
    TQMLItemSlotData(uint16_t typeId, uint32_t count, const QString& name, QObject* parent = nullptr);

    int typeID() const override { return m_typeId; }
    int displayTypeID() const override { return m_displayTypeId; }
    /// The count capped at 255.
    int cumulativeCount() const override;
    QString countString() const override;
    /// "<count>x <name>", empty without a name.
    QString tooltipString() const override;
    QString lootvalueImageSourceUnderItem() const override { return m_lootvalueUnder; }
    QString lootvalueImageSourceOverItem() const override { return m_lootvalueOver; }

    const QString& name() const { return m_name; }
    uint32_t count() const { return m_count; }
    /// Changes the count; signals countChanged when it changes.
    void setCount(uint32_t count);
    /// Shows another type in the slot; signals typeIDchanged when it changes.
    void setDisplayTypeID(uint16_t typeId);
    /// Both loot value frame images; signals lootvalueCategoryChanged when either changes.
    void setLootvalueImageSources(const QString& underItem, const QString& overItem);

private:
    uint16_t m_typeId = 0;
    uint16_t m_displayTypeId = 0;
    uint32_t m_count = 0;
    QString m_name;
    QString m_lootvalueUnder;
    QString m_lootvalueOver;
};

}
}
