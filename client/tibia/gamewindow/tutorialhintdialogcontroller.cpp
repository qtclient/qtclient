#include "tutorialhintdialogcontroller.h"

#include "cyclopedia/cyclopediamapstaticdatastorage.h"
#include "gamewindow/battlelistcontroller.h"
#include "gamewindow/vocationselectiondialogcontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "qmlcomponents/tutorialhelper.h"
#include "worldmap/worldmaptutorialmarkeroverlaycontroller.h"

#include <QQuickItem>
#include <QUrl>
#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;

}

TTutorialHintDialogController::TTutorialHintDialogController(
    QQmlEngine* engine, QQuickItem* dialogParentItem, QQuickItem* mapWindowPane,
    std::shared_ptr<cyclopedia::TCyclopediaMapStaticDataStorage> CyclopediaMapStaticDataStorage, QObject* parent)
    : TDialogController(QStringLiteral("tutorial/TutorialHintDialog.qml"), engine, dialogParentItem, parent)
    , m_mapWindowPane(mapWindowPane)
    , m_cyclopediaMapStaticDataStorage(std::move(CyclopediaMapStaticDataStorage))
{
}

void TTutorialHintDialogController::connectToGameSession(std::shared_ptr<game::IGameSession> GameSession)
{
    m_gameSession = std::move(GameSession);
}

void TTutorialHintDialogController::handleGameAction(const input::IGameActionPtr& pGameAction)
{
    if (!pGameAction)
        return;
    if (const auto* marker = input::gameActionAs<input::TGameActionAddGUITutorialMarker>(pGameAction)) {
        if (marker->location() && marker->location()->gameActionContextType() == input::EGameActionContextType::LOCATION_TUTORIAL_HINT_DIALOG
            && m_tutorialMarkerID != marker->markerId()) {
            m_tutorialMarkerID = marker->markerId();
            Q_EMIT tutorialMarkerIdChanged();
        }
        return;
    }
    const auto* hint = input::gameActionAs<input::TGameActionShowTutorialHint>(pGameAction);
    if (!hint || hint->tutorialHintId() == 2)
        return;
    setTutorialHint(hint->tutorialHintId());
    if ((!m_caption.isEmpty() || !m_captionImagePath.isEmpty()) && !m_imagePath.isEmpty() && m_tutorialHintId >= 1
        && m_tutorialHintId <= 14) {
        dialog()->create();
        dialog()->show();
    }
}

void TTutorialHintDialogController::setTutorialHint(const int TutorialHintId)
{
    QString caption;
    QString captionImagePath;
    QString imagePath;
    QString mapImagePath;
    QString imageButtonText;
    bool hasBevelFrame = false;
    switch (TutorialHintId) {
        case 1:
            caption = qtTrId("tutorialhintdialog_caption_alternative_controls");
            imagePath = QStringLiteral("/images/tutorial/hint_01_alternativecontrols.png");
            break;
        case 2:
        case 12:
        default:
            break;
        case 3:
            caption = qtTrId("tutorialhintdialog_caption_combat_basics");
            imagePath = QStringLiteral("/images/tutorial/hint_03_vocationknight.png");
            break;
        case 4:
            caption = qtTrId("tutorialhintdialog_caption_combat_basics");
            imagePath = QStringLiteral("/images/tutorial/hint_04_vocationpaladin.png");
            break;
        case 5:
            caption = qtTrId("tutorialhintdialog_caption_combat_basics");
            imagePath = QStringLiteral("/images/tutorial/hint_05_vocationsorcerer.png");
            break;
        case 6:
            caption = qtTrId("tutorialhintdialog_caption_combat_basics");
            imagePath = QStringLiteral("/images/tutorial/hint_06_vocationdruid.png");
            break;
        case 7:
        case 8:
        case 9:
        case 10:
            caption = qtTrId("tutorialhintdialog_caption_arrival_main");
            imagePath = QStringLiteral("/images/tutorial/hint_07_arrivalmain.jpg");
            break;
        case 11:
            caption = qtTrId("tutorialhintdialog_caption_combat_basics");
            imagePath = QStringLiteral("/images/tutorial/hint_11_vocationmonk.png");
            break;
        case 13:
            // The welcome banner over the backdrop and the full map.
            captionImagePath = QStringLiteral("/images/tutorial/banner_tutorialpopup_welcometotibia.png");
            imagePath = QStringLiteral("/images/tutorial/backdrop_popup_welcometonewhaven.png");
            if (m_cyclopediaMapStaticDataStorage)
                mapImagePath = QUrl::fromLocalFile(m_cyclopediaMapStaticDataStorage->fullMapFileName()).url();
            imageButtonText = qtTrId("journey_on");
            hasBevelFrame = true;
            break;
        case 14:
            captionImagePath = QStringLiteral("/images/tutorial/banner_tutorialpopup_theworldawaits.png");
            imagePath = QStringLiteral("/images/tutorial/backdrop_popup_theworldawaits.jpg");
            imageButtonText = qtTrId("start");
            hasBevelFrame = true;
            break;
    }
    m_tutorialHintId = TutorialHintId;
    m_caption = caption;
    m_captionImagePath = captionImagePath;
    m_imagePath = imagePath;
    m_mapImagePath = mapImagePath;
    m_imageButtonText = imageButtonText;
    m_hasBevelFrame = hasBevelFrame;
    Q_EMIT displayDataChanged();
}

void TTutorialHintDialogController::requestClose()
{
    closeDialog();
}

void TTutorialHintDialogController::closeDialog()
{
    if ((!m_caption.isEmpty() || !m_captionImagePath.isEmpty()) && !m_imagePath.isEmpty() && m_tutorialHintId >= 1
        && m_tutorialHintId <= 14)
        Q_EMIT publishGameAction(std::make_shared<input::TGameActionTutorialHintConfirmed>(m_tutorialHintId));
    m_tutorialHintId = 0;
    m_caption.clear();
    m_captionImagePath.clear();
    m_imagePath.clear();
    m_mapImagePath.clear();
    m_imageButtonText.clear();
    m_hasBevelFrame = false;
    m_tutorialMarkerID = 0;
    dialog()->close();
}

QPoint TTutorialHintDialogController::youAreHereCoordianteWithinImage() const
{
    if (!tibia::game::hasLocalPlayer())
        return {};
    const Position position = tibia::game::localPlayerPosition();
    return { int(position.x) - 0x7b80, int(position.y) - 0x7800 };
}

QQuickItem* TTutorialHintDialogController::mapWindowPane() { return m_mapWindowPane; }

TTutorialShell::TTutorialShell(QQmlEngine* engine, QQuickItem* dialogParentItem, QQuickItem* mapWindowPane,
                               std::shared_ptr<cyclopedia::TCyclopediaMapStaticDataStorage> CyclopediaMapStaticDataStorage,
                               QObject* parent)
    : QObject(parent)
    , m_hint(new TTutorialHintDialogController(engine, dialogParentItem, mapWindowPane, std::move(CyclopediaMapStaticDataStorage), this))
    , m_vocation(new TVocationSelectionDialogController(engine, dialogParentItem, mapWindowPane, this))
{
    m_gameActions.on(A::ADD_GUI_TUTORIAL_MARKER, [](const input::TGameActionAddGUITutorialMarker& action) {
        if (worldmap::TWorldMapTutorialMarkerOverlayController* overlay =
                worldmap::TWorldMapTutorialMarkerOverlayController::instance())
            overlay->addGuiMarker(action);
        if (qmlcomponents::TTutorialHelperQmlService* helper = qmlcomponents::TTutorialHelperQmlService::instance()) {
            if (action.markerId() > 0) {
                if (auto* item = helper->findTutorialMarkerQuickItemWithID(action.markerId()))
                    item->setVisible(true);
            }
        }
        return true;
    });
    m_gameActions.on(A::REMOVE_ALL_TUTORIAL_MARKERS, [](const input::IGameAction&) {
        if (worldmap::TWorldMapTutorialMarkerOverlayController* overlay =
                worldmap::TWorldMapTutorialMarkerOverlayController::instance())
            overlay->removeAllMarkers();
        return true;
    });
    m_gameActions.on(A::FINISHED_TUTORIAL_FOR_CHARACTER, [this](const input::IGameAction&) {
        Q_EMIT tutorialFinished();
        return true;
    });
}

TTutorialShell::~TTutorialShell()
{
}

void TTutorialShell::handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction)
{
    m_hint->handleGameAction(pGameAction);
    m_vocation->handleGameAction(pGameAction);
    m_gameActions.handle(pGameAction);
}

}
