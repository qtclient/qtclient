#include "game/qmlblessingwithcharges.h"

namespace tibia::game {

namespace {

/// The cell of a blessing in the "blessings-icons" strip (0 for an unknown bit).
QString blessingImageSource(const int BlessingId)
{
    int index = 0;
    switch (BlessingId) {
        case 0x2: index = 8; break;
        case 0x4: index = 3; break;
        case 0x8: index = 5; break;
        case 0x10: index = 2; break;
        case 0x20: index = 4; break;
        case 0x40: index = 1; break;
        case 0x80: index = 6; break;
        case 0x100: index = 7; break;
        default: break;
    }
    return QStringLiteral("image://%1/%2").arg(QStringLiteral("blessings-icons")).arg(index);
}

}

TQmlBlessingWithCharges::TQmlBlessingWithCharges(const TBlessingDialogData::TBlessing& Blessing)
    : cip::tibia::game::TQmlBlessingWithCharges(nullptr)
    , m_blessing(Blessing)
{
}

int TQmlBlessingWithCharges::blessingId() const { return m_blessing.Id; }

QString TQmlBlessingWithCharges::blessingName() const
{
    switch (m_blessing.Id) {
        case 0x1: return qtTrId("blessing_adventurers_blessing");
        case 0x2: return qtTrId("blessing_twist_of_fate");
        case 0x4: return qtTrId("blessing_wisdom_of_solitude");
        case 0x8: return qtTrId("blessing_spark_of_the_phoenix");
        case 0x10: return qtTrId("blessing_fire_of_the_suns");
        case 0x20: return qtTrId("blessing_spritual_shielding");
        case 0x40: return qtTrId("blessing_embrace_of_tibia");
        case 0x80: return qtTrId("blessing_heart_of_the_mountain");
        case 0x100: return qtTrId("blessing_blood_of_the_mountain");
        default: return qtTrId("dummy_unknown");
    }
}

QString TQmlBlessingWithCharges::blessingImage() const { return blessingImageSource(m_blessing.Id); }

int TQmlBlessingWithCharges::blessingCharges() const { return m_blessing.Charges; }

int TQmlBlessingWithCharges::blessingChargesFromStore() const { return m_blessing.ChargesFromStore; }

}
