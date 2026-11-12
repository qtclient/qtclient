#include "bossdifficultyselectiondialogcontroller.h"

#include <tibia/game/gamesession.h>

#include <string>
#include <tibia/game/gamenotify.h>
#include <tibia/game/tgamenotify.h>

namespace tibia::gamewindow {

namespace {

using Args = std::vector<std::any>;

enum EBossDifficultyAction : uint8_t { Start = 0, Cancel = 1, Preview = 2 };

QStringList stringList(const Args& args, const size_t index)
{
    QStringList out;
    if (const auto* list = index < args.size() ? std::any_cast<std::vector<std::string>>(&args[index]) : nullptr) {
        out.reserve(static_cast<qsizetype>(list->size()));
        for (const std::string& s : *list)
            out.append(QString::fromStdString(s));
    }
    return out;
}

}

TBossDifficultySelectionDialogController::TBossDifficultySelectionDialogController(QObject* parent)
    : TDialogController(QStringLiteral("BossDifficultySelectionDialog.qml"), parent)
{
    m_previewTimer.setSingleShot(true);
    m_previewTimer.setInterval(500);
    connect(&m_previewTimer, &QTimer::timeout, this, [this] {
        tibia::game::sendBossDifficultyAction(Preview, static_cast<uint16_t>(m_selectedDifficulty));
    });

    m_hooks.emplace_back(tibia::game::kNotifyScope, "onBossDifficultySelection", [this](GameObject*, const Args& args) {
        open(args);
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onBossDifficultyUpdate", [this](GameObject*, const Args& args) {
        update(args);
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onBossDifficultyClose", [this](GameObject*, const Args&) {
        m_previewTimer.stop();
        dialog()->close();
        return std::any{};
    });
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameEnded, this, [this]() {
        m_previewTimer.stop();
        dialog()->destroy();
    });
}

// b1/6de0cd49cfeaeccc
void TBossDifficultySelectionDialogController::open(const Args& args)
{
    using client::gamecore::argBool;
    using client::gamecore::argInt;
    const int lowest = static_cast<int>(argInt(args, 0));
    const bool active = argBool(args, 1);
    const int raceId = static_cast<int>(argInt(args, 2));
    const int personalHighest = static_cast<int>(argInt(args, 5));
    // Crystal: bad luck percent * 1000.
    const float badLuck = static_cast<float>(argInt(args, 6)) / 1000.f;
    const bool sameConstants = dialog()->isShown() && lowest == m_lowestDifficulty && active == m_isActiveUser && raceId == m_raceId
        && personalHighest == m_personalHighestDifficulty && badLuck == m_currentBadLuckDropBonus;
    m_lowestDifficulty = lowest;
    m_isActiveUser = active;
    m_raceId = raceId;
    m_personalHighestDifficulty = personalHighest;
    m_currentBadLuckDropBonus = badLuck;
    if (!m_previewTimer.isActive())
        m_selectedDifficulty = static_cast<int>(argInt(args, 3));
    m_highestDifficultyOfGroup = static_cast<int>(argInt(args, 4));
    m_playerNames = stringList(args, 7);
    m_conModifiers = stringList(args, 8);
    m_proModifiers = stringList(args, 9);
    if (!sameConstants)
        dialog()->destroy();
    Q_EMIT selectionDataUpdated();
    dialog()->show();
}

// b1/74b4a8636dbdb36a
void TBossDifficultySelectionDialogController::update(const Args& args)
{
    if (!m_isActiveUser)
        m_selectedDifficulty = static_cast<int>(client::gamecore::argInt(args, 0));
    m_conModifiers = stringList(args, 1);
    m_proModifiers = stringList(args, 2);
    Q_EMIT selectionDataUpdated();
}

// b1/5fe5d1a7e2202ce7
void TBossDifficultySelectionDialogController::setSelectedDifficulty(const int v)
{
    if (v == m_selectedDifficulty)
        return;
    m_selectedDifficulty = v;
    m_previewTimer.start();
    Q_EMIT selectionDataUpdated();
}

// b1/8d23692530c9ba76
void TBossDifficultySelectionDialogController::onCancelClicked()
{
    m_previewTimer.stop();
    tibia::game::sendBossDifficultyAction(Cancel);
}

// b1/340334120542aca2
void TBossDifficultySelectionDialogController::onStartClicked()
{
    m_previewTimer.stop();
    tibia::game::sendBossDifficultyAction(Start, static_cast<uint16_t>(m_selectedDifficulty));
}

}
