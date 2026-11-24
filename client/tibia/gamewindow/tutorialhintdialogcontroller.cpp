#include "tutorialhintdialogcontroller.h"

#include "cyclopedia/cyclopediamapstaticdatastorage.h"
#include "gamewindow/battlelistcontroller.h"
#include "gamewindow/vocationselectiondialogcontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "qmlcomponents/tutorialhelper.h"
#include "worldmap/worldmaptutorialmarkeroverlaycontroller.h"

#include <QQuickItem>
#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;
using Args = std::vector<std::any>;

struct HintAssets
{
    QString captionId;
    QString captionImagePath;
    QString imagePath;
    QString buttonTextId;
    bool bevel = true;
};

/// CIP hint-id → assets (strings + `/images/tutorial/hint_*` from client.strings).
/// Id 2 opens VocationSelectionPopup instead (handled by TTutorialShell).
const HintAssets* hintAssets(const int id)
{
    static const HintAssets alternative{ QStringLiteral("tutorialhintdialog_caption_alternative_controls"),
                                         {},
                                         QStringLiteral("/images/tutorial/hint_01_alternativecontrols.png"),
                                         {},
                                         true };
    static const HintAssets knight{ QStringLiteral("tutorialhintdialog_caption_combat_basics"),
                                    {},
                                    QStringLiteral("/images/tutorial/hint_03_vocationknight.png"),
                                    {},
                                    true };
    static const HintAssets paladin{ QStringLiteral("tutorialhintdialog_caption_combat_basics"),
                                     {},
                                     QStringLiteral("/images/tutorial/hint_04_vocationpaladin.png"),
                                     {},
                                     true };
    static const HintAssets sorcerer{ QStringLiteral("tutorialhintdialog_caption_combat_basics"),
                                      {},
                                      QStringLiteral("/images/tutorial/hint_05_vocationsorcerer.png"),
                                      {},
                                      true };
    static const HintAssets druid{ QStringLiteral("tutorialhintdialog_caption_combat_basics"),
                                   {},
                                   QStringLiteral("/images/tutorial/hint_06_vocationdruid.png"),
                                   {},
                                   true };
    static const HintAssets monk{ QStringLiteral("tutorialhintdialog_caption_combat_basics"),
                                  {},
                                  QStringLiteral("/images/tutorial/hint_11_vocationmonk.png"),
                                  {},
                                  true };
    static const HintAssets arrival{ QStringLiteral("tutorialhintdialog_caption_arrival_main"),
                                     {},
                                     QStringLiteral("/images/tutorial/hint_07_arrivalmain.jpg"),
                                     {},
                                     true };
    static const HintAssets welcome{ {},
                                     QStringLiteral("/images/tutorial/banner_tutorialpopup_welcometotibia.png"),
                                     QStringLiteral("/images/tutorial/backdrop_popup_welcometonewhaven.png"),
                                     QStringLiteral("start"),
                                     true };
    static const HintAssets worldAwaits{ {},
                                         QStringLiteral("/images/tutorial/banner_tutorialpopup_theworldawaits.png"),
                                         QStringLiteral("/images/tutorial/backdrop_popup_theworldawaits.jpg"),
                                         QStringLiteral("start"),
                                         true };

    switch (id) {
        case 1:
            return &alternative;
        case 3:
            return &knight;
        case 4:
            return &paladin;
        case 5:
            return &sorcerer;
        case 6:
            return &druid;
        case 7:
        case 13: // Crystal Newhaven→Targuna ferry still sends 13; CIP asset is hint_07.
            return &arrival;
        case 11:
            return &monk;
        case 14:
            return &welcome;
        case 15:
            return &worldAwaits;
        default:
            return nullptr;
    }
}

}

TTutorialHintDialogController::TTutorialHintDialogController(
    QQmlEngine* engine, QQuickItem* dialogParentItem, QQuickItem* mapWindowPane,
    std::shared_ptr<cyclopedia::TCyclopediaMapStaticDataStorage> CyclopediaMapStaticDataStorage, QObject* parent)
    : TDialogController(QStringLiteral("tutorial/TutorialHintDialog.qml"), engine, dialogParentItem, parent)
    , m_mapWindowPane(mapWindowPane)
    , m_cyclopediaMapStaticDataStorage(std::move(CyclopediaMapStaticDataStorage))
{
}

void TTutorialHintDialogController::showHint(const QString& caption, const QString& imagePath, const QString& buttonText,
                                             const int markerId, const bool bevel)
{
    m_caption = caption;
    m_captionImagePath.clear();
    m_imagePath = imagePath;
    m_mapImagePath.clear();
    m_imageButtonText = buttonText;
    m_hasBevelFrame = bevel;
    m_youAreHereCoordianteWithinImage = QPoint();
    if (m_tutorialMarkerID != markerId) {
        m_tutorialMarkerID = markerId;
        Q_EMIT tutorialMarkerIdChanged();
    }
    emitDisplay();
    dialog()->show();
}

void TTutorialHintDialogController::showBannerHint(const QString& captionImagePath, const QString& imagePath,
                                                   const QString& buttonText, const int markerId, const bool bevel)
{
    m_caption.clear();
    m_captionImagePath = captionImagePath;
    m_imagePath = imagePath;
    m_mapImagePath.clear();
    m_imageButtonText = buttonText;
    m_hasBevelFrame = bevel;
    m_youAreHereCoordianteWithinImage = QPoint();
    if (m_tutorialMarkerID != markerId) {
        m_tutorialMarkerID = markerId;
        Q_EMIT tutorialMarkerIdChanged();
    }
    emitDisplay();
    dialog()->show();
}

bool TTutorialHintDialogController::showHintById(const int hintId)
{
    const HintAssets* assets = hintAssets(hintId);
    if (!assets)
        return false;
    m_tutorialHintId = hintId;
    const QString button = assets->buttonTextId.isEmpty() ? QString() : qtTrId(assets->buttonTextId.toUtf8().constData());
    if (!assets->captionImagePath.isEmpty()) {
        showBannerHint(assets->captionImagePath, assets->imagePath, button, 0, assets->bevel);
        return true;
    }
    showHint(qtTrId(assets->captionId.toUtf8().constData()), assets->imagePath, button, 0, assets->bevel);
    return true;
}

void TTutorialHintDialogController::emitDisplay() { Q_EMIT displayDataChanged(); }

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
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onTutorialHint", [this](GameObject*, const Args& args) {
        if (!args.empty())
            handleTutorialHintId(static_cast<int>(client::gamecore::argInt(args, 0)));
        return std::any{};
    });

    m_gameActions.on(A::SHOW_TUTORIAL_HINT, [this](const input::TGameActionShowTutorialHint& action) {
        if (action.tutorialHintId() > 0)
            handleTutorialHintId(action.tutorialHintId());
        return true;
    });
    m_gameActions.on(A::CONFIRM_TUTORIAL_HINT, [this](const input::IGameAction&) {
        m_hint->dialog()->close();
        return true;
    });
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
    m_gameActions.on(A::TUTORIAL_PROGRESS, [](const input::TGameActionTutorialProgress&) {
        return true;
    });
    m_gameActions.on(A::SESSIONDUMP_PAUSE, [](const input::IGameAction&) {
        return true;
    });
    m_gameActions.on(A::SESSIONDUMP_RESUME, [](const input::IGameAction&) {
        return true;
    });
    m_gameActions.on(A::SESSIONDUMP_SKIP_TO, [](const input::TGameActionSessiondumpSkipTo&) {
        // CIP `.hints` use SKIP_TO 0 as a no-op / restart marker at TutorialStart.
        return true;
    });
    m_gameActions.on(A::SESSIONDUMP_START, [](const input::TGameActionSessiondumpStart&) {
        return true;
    });
}

TTutorialShell::~TTutorialShell()
{
}

void TTutorialShell::handleTutorialHintId(const int hintId)
{
    if (hintId == 2) {
        m_vocation->handleGameAction(std::make_shared<input::TGameActionShowTutorialHint>(hintId));
        return;
    }
    m_hint->showHintById(hintId);
}

}
