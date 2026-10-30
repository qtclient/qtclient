#include "tutorialhintdialogcontroller.h"

#include "gamewindow/vocationselectiondialogcontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"

#include <QLoggingCategory>
#include <QQuickItem>

namespace tibia::gamewindow {

namespace {

Q_LOGGING_CATEGORY(lcTutorial, "tibia.tutorial")

using A = input::EGameAction;
using Args = std::vector<std::any>;

QQuickItem* mapWindowPaneItem()
{
    TDialogStackController* stack = TDialogStackController::instance();
    if (!stack || !stack->dialogPlaceholder())
        return nullptr;
    QQuickItem* root = stack->dialogPlaceholder();
    while (root->parentItem())
        root = root->parentItem();
    if (QQuickItem* pane = root->findChild<QQuickItem*>(QStringLiteral("mapWindowPane")))
        return pane;
    return stack->dialogPlaceholder()->parentItem();
}

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

TTutorialHintDialogController::TTutorialHintDialogController(QObject* parent)
    : TDialogController(QStringLiteral("tutorial/TutorialHintDialog.qml"), parent)
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
    dialog()->close();
    input::publish(input::makeAction(A::CONFIRM_TUTORIAL_HINT));
}

QQuickItem* TTutorialHintDialogController::mapWindowPane() { return mapWindowPaneItem(); }

TTutorialShell::TTutorialShell(QObject* parent)
    : QObject(parent)
    , m_hint(new TTutorialHintDialogController(this))
    , m_vocation(new TVocationSelectionDialogController(this))
{
    m_hooks.emplace_back("g_game", "onTutorialHint", [this](LuaObject*, const Args& args) {
        if (!args.empty())
            handleTutorialHintId(static_cast<int>(client::gamecore::argInt(args, 0)));
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onGameEnd", [this](LuaObject*, const Args&) {
        m_hint->dialog()->destroy();
        m_vocation->dialog()->destroy();
        return std::any{};
    });

    input::setActionHandler(A::SHOW_TUTORIAL_HINT, [this](const input::TGameAction& action) {
        if (action.parameter > 0)
            handleTutorialHintId(action.parameter);
        else if (!action.filePath.isEmpty() || !action.text.isEmpty())
            m_hint->showHint(action.text, action.filePath, action.statement, 0, true);
        return true;
    });
    input::setActionHandler(A::ADD_HINT_AT_TUTORIAL_MARKER, [this](const input::TGameAction& action) {
        m_hint->showHint(action.text, action.filePath, action.statement, action.parameter, true);
        return true;
    });
    input::setActionHandler(A::ADD_MOUSE_TRAIL_HINT_AT_TUTORIAL_MARKER, [this](const input::TGameAction& action) {
        m_hint->showHint(action.text, action.filePath, action.statement, action.parameter, false);
        return true;
    });
    input::setActionHandler(A::CONFIRM_TUTORIAL_HINT, [this](const input::TGameAction&) {
        m_hint->dialog()->close();
        return true;
    });
    input::setActionHandler(A::ADD_GUI_TUTORIAL_MARKER, [](const input::TGameAction& action) {
        qCInfo(lcTutorial) << "ADD_GUI_TUTORIAL_MARKER" << action.parameter << action.text;
        return true;
    });
    input::setActionHandler(A::REMOVE_ALL_TUTORIAL_MARKERS, [](const input::TGameAction&) {
        qCInfo(lcTutorial) << "REMOVE_ALL_TUTORIAL_MARKERS";
        return true;
    });
    input::setActionHandler(A::FINISHED_TUTORIAL_FOR_CHARACTER, [this](const input::TGameAction&) {
        qCInfo(lcTutorial) << "FINISHED_TUTORIAL_FOR_CHARACTER";
        Q_EMIT tutorialFinished();
        return true;
    });
    input::setActionHandler(A::TUTORIAL_PROGRESS, [](const input::TGameAction& action) {
        qCInfo(lcTutorial) << "TUTORIAL_PROGRESS" << action.parameter << action.text;
        return true;
    });
    input::setActionHandler(A::SESSIONDUMP_PAUSE, [](const input::TGameAction&) {
        qCInfo(lcTutorial) << "SESSIONDUMP_PAUSE (PacketPlayer has no pause yet)";
        return true;
    });
    input::setActionHandler(A::SESSIONDUMP_RESUME, [](const input::TGameAction&) {
        qCInfo(lcTutorial) << "SESSIONDUMP_RESUME (PacketPlayer has no resume yet)";
        return true;
    });
    input::setActionHandler(A::SESSIONDUMP_SKIP_TO, [this](const input::TGameAction& action) {
        qCInfo(lcTutorial) << "SESSIONDUMP_SKIP_TO" << action.parameter << action.text;
        // Finish only on an explicit end cue — CIP `.hints` use SKIP_TO 0 as a
        // no-op / restart marker at TutorialStart, not "end cinematic".
        if (action.text == QLatin1String("end") || action.text == QLatin1String("finish"))
            Q_EMIT tutorialFinished();
        return true;
    });
    input::setActionHandler(A::SESSIONDUMP_START, [](const input::TGameAction& action) {
        qCInfo(lcTutorial) << "SESSIONDUMP_START" << action.text;
        return true;
    });
}

TTutorialShell::~TTutorialShell()
{
    input::setActionHandler(A::SHOW_TUTORIAL_HINT, nullptr);
    input::setActionHandler(A::ADD_HINT_AT_TUTORIAL_MARKER, nullptr);
    input::setActionHandler(A::ADD_MOUSE_TRAIL_HINT_AT_TUTORIAL_MARKER, nullptr);
    input::setActionHandler(A::CONFIRM_TUTORIAL_HINT, nullptr);
    input::setActionHandler(A::ADD_GUI_TUTORIAL_MARKER, nullptr);
    input::setActionHandler(A::REMOVE_ALL_TUTORIAL_MARKERS, nullptr);
    input::setActionHandler(A::FINISHED_TUTORIAL_FOR_CHARACTER, nullptr);
    input::setActionHandler(A::TUTORIAL_PROGRESS, nullptr);
    input::setActionHandler(A::SESSIONDUMP_PAUSE, nullptr);
    input::setActionHandler(A::SESSIONDUMP_RESUME, nullptr);
    input::setActionHandler(A::SESSIONDUMP_SKIP_TO, nullptr);
    input::setActionHandler(A::SESSIONDUMP_START, nullptr);
}

void TTutorialShell::handleTutorialHintId(const int hintId)
{
    qCInfo(lcTutorial) << "tutorial hint id" << hintId;
    if (hintId == 2) {
        m_vocation->showDialog();
        return;
    }
    if (!m_hint->showHintById(hintId))
        qCInfo(lcTutorial) << "unknown tutorial hint id" << hintId;
}

}
