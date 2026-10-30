#pragma once

#include <cip_gen.h>

#include <QString>

namespace tibia::container {

/// CIP 0x100a3c5e0: a count as Qt length variants (U+009C separated): the
/// English-grouped number, the plain number, then "Nk" from 1000 and "Nkk"
/// from 1000000.
QString countVariants(uint count);

/// An object type and count shown in an item slot (CIP:
/// container::TQMLItemSlotData, ctor 0x100a3c500, metacall 0x1007f0450).
class TQMLItemSlotData final : public cip::tibia::container::TQMLItemSlotData
{
    Q_OBJECT
public:
    TQMLItemSlotData(int typeId, uint count, const QString& name, QObject* parent = nullptr);

    uint count() const { return m_count; }
    /// CIP 0x100a3cc80.
    void setCount(uint count);

    /// CIP 0x100a3c5a0: capped at 255.
    int cumulativeCount() const override;
    /// CIP 0x100a3c5c0.
    QString countString() const override;
    /// CIP 0x100a3ca90: "<count>x <name>", empty without a name.
    QString tooltipString() const override;

private:
    uint m_count = 0;
    QString m_name;
};

}
