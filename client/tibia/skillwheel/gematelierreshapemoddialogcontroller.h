#pragma once

#include "gamewindow/dialogmanager.h"
#include "skillwheel/qmlreshapemod.h"

#include <cip_gen.h>

#include <memory>

namespace tibia {
namespace skillwheel {

/// GemAtelierGemReshapeDialog.qml (CIP: TGemAtelierReshapeModDialogController): the mod a reshape
/// attempt is about, as the gem has it and as the server rolled it. Keeping the current mod emits
/// cancelReshape, choosing the new one confirmReshape; either closes the dialog.
class TGemAtelierReshapeModDialogController final
    : public gamewindow::TDialogController<cip::tibia::gamewindow::TGemAtelierReshapeModDialogController>
{
    Q_OBJECT
public:
    TGemAtelierReshapeModDialogController(QQmlEngine* engine, QQuickItem* dialogParentItem, QObject* parent = nullptr);
    ~TGemAtelierReshapeModDialogController() override;

    QObject* currentMod() const override { return m_current.get(); }
    QObject* reshapedMod() const override { return m_reshaped.get(); }

    /// The current and the reshaped mod `modIdx` of the gem; emits dataChanged.
    void setData(const TGemMods& currentMods, const TGemMods& reshapedMods, int modIdx, int vocationIndex,
                 const TGrades& basicGrades, const TGrades& supremeGrades);

    void requestCancel() override;
    void chooseNewMod() override;
    QString getModGradeImageSource(int ModGrade, bool IsFilled) override;
    QString getModGradeImageSource(int ModGrade) override { return getModGradeImageSource(ModGrade, true); }
    QString getModGradeHighlightImageSource(int ModGrade) override { return modGradeHighlightImage(ModGrade); }
    /// Makes and shows the dialog unless it is already there.
    void showDialog() override;
    /// Emits cancelReshape and closes the dialog.
    void closeDialog() override;

private:
    std::unique_ptr<TQmlReshapeMod> m_current;
    std::unique_ptr<TQmlReshapeMod> m_reshaped;
};

}
}
