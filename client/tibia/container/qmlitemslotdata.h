#pragma once

#include <cip_gen.h>

#include <QString>

namespace tibia {
namespace container {

/// CIP : a count as Qt length variants (U+009C separated): the
/// English-grouped number, the plain number, then "Nk" from 1000 and "Nkk"
/// from 1000000.
QString countVariants(uint count);

/// An object type and count shown in an item slot (CIP:
/// container::TQMLItemSlotData, ctor , metacall).
class TQMLItemSlotData final : public cip::tibia::container::TQMLItemSlotData
{
    Q_OBJECT
public:
    TQMLItemSlotData(int typeId, uint count, const QString& name, QObject* parent = nullptr);

    uint count() const { return m_count; }
    /// CIP .
    void setCount(uint count);

    /// CIP : capped at 255.
    int cumulativeCount() const override;
    /// CIP .
    QString countString() const override;
    /// CIP : "<count>x <name>", empty without a name.
    QString tooltipString() const override;

private:
    uint m_count = 0;
    QString m_name;
};

}
}
