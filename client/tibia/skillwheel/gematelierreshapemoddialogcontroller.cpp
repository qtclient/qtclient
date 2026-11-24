#include "skillwheel/gematelierreshapemoddialogcontroller.h"

namespace tibia::skillwheel {

TGemAtelierReshapeModDialogController::TGemAtelierReshapeModDialogController(QQmlEngine* engine, QQuickItem* dialogParentItem,
                                                                             QObject* parent)
    : TDialogController(QStringLiteral("GemAtelierGemReshapeDialog.qml"), engine, dialogParentItem, parent)
{}

TGemAtelierReshapeModDialogController::~TGemAtelierReshapeModDialogController()
{
    Q_EMIT cancelReshape();
    dialog()->close();
}

void TGemAtelierReshapeModDialogController::setData(const TGemMods& currentMods, const TGemMods& reshapedMods, const int modIdx,
                                                    const int vocationIndex, const TGrades& basicGrades, const TGrades& supremeGrades)
{
    m_current = std::make_unique<TQmlReshapeMod>(currentMods, modIdx, vocationIndex, basicGrades, supremeGrades);
    m_reshaped = std::make_unique<TQmlReshapeMod>(reshapedMods, modIdx, vocationIndex, basicGrades, supremeGrades);
    Q_EMIT dataChanged();
}

void TGemAtelierReshapeModDialogController::requestCancel()
{
    Q_EMIT cancelReshape();
    closeDialog();
}

void TGemAtelierReshapeModDialogController::chooseNewMod()
{
    Q_EMIT confirmReshape();
    closeDialog();
}

/// A filled grade shows its backdrop, an unfilled one the empty backdrop.
QString TGemAtelierReshapeModDialogController::getModGradeImageSource(const int ModGrade, const bool IsFilled)
{
    return IsFilled ? modGradeImage(ModGrade) : modGradeImage(8);
}

void TGemAtelierReshapeModDialogController::showDialog()
{
    if (dialog()->item())
        return;
    dialog()->create();
    dialog()->show();
}

void TGemAtelierReshapeModDialogController::closeDialog()
{
    Q_EMIT cancelReshape();
    dialog()->close();
}

}
