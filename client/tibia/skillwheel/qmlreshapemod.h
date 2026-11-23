#pragma once

#include "skillwheel/skillwheeldata.h"

#include <cip_gen.h>

namespace tibia {
namespace skillwheel {

/// One side of the gem atelier's reshape dialog (CIP: TQmlReshapeMod): a gem's mods, the mod the
/// reshape is about (0 first basic, 1 second basic, 2 supreme), the vocation index its effect text
/// is for and the mod grades.
class TQmlReshapeMod final : public cip::tibia::skillwheel::TQmlReshapeMod
{
    Q_OBJECT
public:
    TQmlReshapeMod(const TGemMods& mods, int modIdx, int vocationIndex, const TGrades& basicGrades, const TGrades& supremeGrades,
                   QObject* parent = nullptr);

    /// The mod's grade, 0 for a mod the gem does not have.
    int modGrade() const override;
    /// The mod's icon, empty for a mod the gem does not have.
    QUrl imageSource() const override;
    /// A basic mod's effects at its grade; a supreme mod's effect, after its name for augmentations
    /// and revelation masteries. Empty for a mod the gem does not have.
    QString effectString() const override;
    bool isSupremeMod() const override;

private:
    TGemMods m_mods;
    int m_modIdx = 0;
    int m_vocationIndex = 0;
    TGrades m_basicGrades;
    TGrades m_supremeGrades;
};

}
}
