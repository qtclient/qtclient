#pragma once

#include "client/gamecore.h"
#include "gamewindow/dialogmanager.h"

#include <cip_gen.h>

#include <QPoint>
#include <QString>

#include <vector>

namespace tibia::gamewindow {

class TVocationSelectionDialogController;

/// tutorial/TutorialHintDialog.qml (CIP: TTutorialHintDialogController).
class TTutorialHintDialogController final
    : public TDialogController<cip::tibia::gamewindow::TTutorialHintDialogController>
{
    Q_OBJECT
public:
    explicit TTutorialHintDialogController(QObject* parent = nullptr);

    /// Show a hint from a SHOW_TUTORIAL_HINT / ADD_HINT_AT_TUTORIAL_MARKER action.
    void showHint(const QString& caption, const QString& imagePath, const QString& buttonText,
                  int markerId = 0, bool bevel = true);
    /// Apply CIP caption-banner layout (no text caption; crest header + image CTA).
    void showBannerHint(const QString& captionImagePath, const QString& imagePath, const QString& buttonText,
                        int markerId = 0, bool bevel = true);
    /// Map a GameserverTutorialHint / SHOW_TUTORIAL_HINT id to dialog assets.
    /// Returns false when the id is unknown (caller may open vocation UI for id 2).
    bool showHintById(int hintId);

    Q_INVOKABLE void requestClose() override;
    Q_INVOKABLE QQuickItem* mapWindowPane() override;

private:
    void emitDisplay();
};

/// Owns tutorial hint + vocation-selection dialogs and wires tutorial game actions.
class TTutorialShell final : public QObject
{
    Q_OBJECT
public:
    explicit TTutorialShell(QObject* parent = nullptr);
    ~TTutorialShell() override;

    TTutorialHintDialogController* hint() const { return m_hint; }
    TVocationSelectionDialogController* vocation() const { return m_vocation; }

    /// GameserverTutorialHint (0xDC) / sessiondump SHOW_TUTORIAL_HINT id.
    void handleTutorialHintId(int hintId);

Q_SIGNALS:
    /// Tutorial cinematic should end (FINISHED_TUTORIAL / sessiondump end cue).
    void tutorialFinished();

private:
    TTutorialHintDialogController* m_hint = nullptr;
    TVocationSelectionDialogController* m_vocation = nullptr;
    std::vector<client::gamecore::Hook> m_hooks;
};

}
