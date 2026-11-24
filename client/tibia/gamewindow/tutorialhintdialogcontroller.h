#pragma once

#include "input/gameactiondispatch.h"
#include "gamewindow/dialogmanager.h"

#include <cip_gen.h>

#include <QPoint>
#include <QPointer>
#include <QQuickItem>
#include <QString>

#include <memory>
#include <vector>

namespace tibia {
namespace cyclopedia {
class TCyclopediaMapStaticDataStorage;
}
namespace game {
class IGameSession;
}
namespace gamewindow {

class TVocationSelectionDialogController;

/// tutorial/TutorialHintDialog.qml (CIP: TTutorialHintDialogController).
class TTutorialHintDialogController final
    : public TDialogController<cip::tibia::gamewindow::TTutorialHintDialogController>
{
    Q_OBJECT
public:
    /// The dialog is made with `engine` in `dialogParentItem`; `mapWindowPane`
    /// is the game window's map pane the hint is placed over.
    TTutorialHintDialogController(QQmlEngine* engine, QQuickItem* dialogParentItem, QQuickItem* mapWindowPane,
                                  std::shared_ptr<cyclopedia::TCyclopediaMapStaticDataStorage> CyclopediaMapStaticDataStorage,
                                  QObject* parent = nullptr);

    /// CIP: the dialog keeps the game session.
    void connectToGameSession(std::shared_ptr<game::IGameSession> GameSession);
    /// CIP: a GUI tutorial marker on the hint dialog becomes the dialog's marker; a tutorial hint
    /// (other than 2, the vocation selection's) fills the dialog, which is shown when the hint has a
    /// caption (or caption image) and an image and its id is 1..14.
    void handleGameAction(const input::IGameActionPtr& pGameAction);

    Q_INVOKABLE void requestClose() override;
    Q_INVOKABLE QQuickItem* mapWindowPane() override;

    /// The player's position on the tutorial island's map image (its
    /// world position minus 31616, 30720); (0, 0) without a player.
    QPoint youAreHereCoordianteWithinImage() const override;

    /// A shown hint (a caption or caption image, an image and a hint id 1..14)
    /// is confirmed; the hint data is cleared and the dialog closes.
    void closeDialog();

private:
    /// CIP: the hint's caption (or caption and background banners), image, map image, button text
    /// and bevel frame; an unknown hint leaves them empty.
    void setTutorialHint(int TutorialHintId);

    QPointer<QQuickItem> m_mapWindowPane;
    std::shared_ptr<cyclopedia::TCyclopediaMapStaticDataStorage> m_cyclopediaMapStaticDataStorage;
    /// The hint shown (CONFIRM_TUTORIAL_HINT carries it).
    int m_tutorialHintId = 0;
    std::shared_ptr<game::IGameSession> m_gameSession;
};

/// Host: owns the tutorial hint and vocation selection dialogs (CipSoft: the game client's) and the
/// tutorial marker / finished-tutorial glue of the router's actions.
class TTutorialShell final : public QObject
{
    Q_OBJECT
public:
    /// The dialogs are made with `engine` in `dialogParentItem` and placed over
    /// `mapWindowPane`, the game window's map pane.
    TTutorialShell(QQmlEngine* engine, QQuickItem* dialogParentItem, QQuickItem* mapWindowPane,
                   std::shared_ptr<cyclopedia::TCyclopediaMapStaticDataStorage> CyclopediaMapStaticDataStorage,
                   QObject* parent = nullptr);
    ~TTutorialShell() override;

    TTutorialHintDialogController* hint() const { return m_hint; }
    TVocationSelectionDialogController* vocation() const { return m_vocation; }

Q_SIGNALS:
    /// Tutorial cinematic should end (FINISHED_TUTORIAL / sessiondump end cue).
    void tutorialFinished();

private:
    TTutorialHintDialogController* m_hint = nullptr;
    TVocationSelectionDialogController* m_vocation = nullptr;

public Q_SLOTS:
    /// The router's game actions: the hint and vocation selection dialogs get each (CIP: both are
    /// connected to publishGameActionInternal), then the markers and the finished tutorial.
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction);

private:
    ::tibia::input::TGameActionDispatch m_gameActions;
};

}
}
