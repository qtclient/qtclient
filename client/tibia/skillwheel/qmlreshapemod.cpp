#include "skillwheel/qmlreshapemod.h"

namespace tibia::skillwheel {

namespace {

/// A mod's grade, 0 for a mod without one.
int gradeOf(const TGrades& grades, const int mod)
{
    const auto it = grades.find(mod);
    return it != grades.end() ? it->second : 0;
}

}

TQmlReshapeMod::TQmlReshapeMod(const TGemMods& mods, const int modIdx, const int vocationIndex, const TGrades& basicGrades,
                               const TGrades& supremeGrades, QObject* parent)
    : cip::tibia::skillwheel::TQmlReshapeMod(parent)
    , m_mods(mods)
    , m_modIdx(modIdx)
    , m_vocationIndex(vocationIndex)
    , m_basicGrades(basicGrades)
    , m_supremeGrades(supremeGrades)
{}

int TQmlReshapeMod::modGrade() const
{
    switch (m_modIdx) {
        case 2: return m_mods.supreme ? gradeOf(m_supremeGrades, *m_mods.supreme) : 0;
        case 1: return m_mods.basic2 ? gradeOf(m_basicGrades, *m_mods.basic2) : 0;
        case 0: return gradeOf(m_basicGrades, m_mods.basic1);
        default: return 0;
    }
}

QUrl TQmlReshapeMod::imageSource() const
{
    switch (m_modIdx) {
        case 2: return m_mods.supreme ? supremeModImage(*m_mods.supreme) : QUrl();
        case 1: return m_mods.basic2 ? basicModImage(*m_mods.basic2) : QUrl();
        case 0: return basicModImage(m_mods.basic1);
        default: return QUrl();
    }
}

QString TQmlReshapeMod::effectString() const
{
    if (m_modIdx == 2) {
        if (!m_mods.supreme)
            return QString();
        const int mod = *m_mods.supreme;
        const QString name = supremeModName(mod, false, false);
        const QString effect = supremeModEffect(mod, gradeOf(m_supremeGrades, mod));
        return showsModName(mod) ? name + u'\n' + effect : effect;
    }
    int mod = 0;
    if (m_modIdx == 1) {
        if (!m_mods.basic2)
            return QString();
        mod = *m_mods.basic2;
    } else if (m_modIdx == 0) {
        mod = m_mods.basic1;
    } else {
        return QString();
    }
    return basicModEffect(mod, m_vocationIndex, gradeOf(m_basicGrades, mod));
}

bool TQmlReshapeMod::isSupremeMod() const { return m_modIdx == 2; }

}
