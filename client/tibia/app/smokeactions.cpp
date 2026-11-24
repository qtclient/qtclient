#include "app/smokeactions.h"

#include "game/gameactionrouter.h"
#include "input/gameaction.h"
#include "store/storestorage.h"

#include <QDebug>
#include <QMetaEnum>
#include <QTimer>

namespace tibia::app {

namespace {

using EGameAction = input::EGameAction;

/// The hint dialog confirms hints 1..14; hint 2 is the vocation selection,
/// which needs a choice and is never confirmed this way.
bool isConfirmableHint(const int hintId)
{
    return hintId >= 1 && hintId <= 14 && hintId != 2;
}

}

bool TSmokeActions::parse(const QStringList& specs, std::vector<TStep>& steps, QString& error)
{
    const QMetaEnum actions = QMetaEnum::fromType<EGameAction>();
    for (const QString& spec : specs) {
        const QStringList parts = spec.split(QLatin1Char(':'));
        if (parts.isEmpty() || parts.size() > 2 || parts.front().trimmed().isEmpty()) {
            error = QStringLiteral("--smoke-action '%1': expected <EGameAction name>[:<delay ms>]").arg(spec);
            return false;
        }
        bool known = false;
        const int value = actions.keyToValue(parts.front().trimmed().toUpper().toLatin1().constData(), &known);
        if (!known || value == int(EGameAction::UNKNOWN)) {
            error = QStringLiteral("--smoke-action '%1': unknown game action").arg(spec);
            return false;
        }
        TStep step;
        step.action = static_cast<EGameAction>(value);
        if (parts.size() == 2) {
            bool ok = false;
            step.delayMs = parts.back().trimmed().toInt(&ok);
            if (!ok || step.delayMs < 0) {
                error = QStringLiteral("--smoke-action '%1': the delay must be milliseconds >= 0").arg(spec);
                return false;
            }
        }
        steps.push_back(step);
    }
    return true;
}

TSmokeActions::TSmokeActions(std::shared_ptr<game::TGameActionRouter> router, std::vector<TStep> steps,
                             const bool confirmHints, const int confirmHintDelayMs, QObject* parent)
    : QObject(parent)
    , m_router(std::move(router))
    , m_steps(std::move(steps))
    , m_confirmHints(confirmHints)
    , m_confirmHintDelayMs(confirmHintDelayMs)
{
    // Every action the client's router passes on: the shown hint is what
    // CONFIRM_TUTORIAL_HINT confirms.
    connect(m_router->internalRouter(), &game::TInternalGameActionRouter::publishGameActionInternal, this,
            &TSmokeActions::onRoutedAction);
}

void TSmokeActions::start()
{
    if (!m_steps.empty())
        runStep(0);
}

void TSmokeActions::runStep(const std::size_t index)
{
    QTimer::singleShot(m_steps[index].delayMs, this, [this, index] {
        const EGameAction action = m_steps[index].action;
        if (action == EGameAction::CONFIRM_TUTORIAL_HINT) {
            confirmShownHint();
        } else if (action == EGameAction::OPEN_STORE) {
            // The store opens on its premium page (the analysers' store button).
            qInfo().noquote() << "smoke: game action OPEN_STORE (premium page)";
            publish(std::make_shared<input::TGameActionOpenStore>(
                store::TStoreOpenParameters{ .openAction = store::OPEN_PREMIUM_OR_BOOSTS, .premiumDetail = 0 }));
        } else {
            qInfo().noquote() << "smoke: game action" << QMetaEnum::fromType<EGameAction>().valueToKey(int(action));
            publish(std::make_shared<input::gameactions::TGameActionSimple>(action));
        }
        if (index + 1 < m_steps.size())
            runStep(index + 1);
    });
}

void TSmokeActions::publish(input::IGameActionPtr action)
{
    m_router->internalRouter()->handleGameActionInternal(std::move(action));
}

void TSmokeActions::onRoutedAction(const input::IGameActionPtr& action)
{
    if (!action)
        return;
    switch (action->gameActionType()) {
    case EGameAction::SHOW_TUTORIAL_HINT:
        if (const auto* hint = input::gameActionAs<input::TGameActionShowTutorialHint>(action)) {
            m_shownHintId = hint->tutorialHintId();
            if (m_confirmHints && isConfirmableHint(m_shownHintId)) {
                const int hintId = m_shownHintId;
                QTimer::singleShot(m_confirmHintDelayMs, this, [this, hintId] {
                    if (m_shownHintId == hintId)
                        confirmShownHint();
                });
            }
        }
        break;
    case EGameAction::CONFIRM_TUTORIAL_HINT:
        m_shownHintId = 0;
        break;
    default:
        break;
    }
}

void TSmokeActions::confirmShownHint()
{
    if (!isConfirmableHint(m_shownHintId)) {
        qInfo() << "smoke: no tutorial hint to confirm";
        return;
    }
    qInfo() << "smoke: confirm tutorial hint" << m_shownHintId;
    // The routed confirmation clears m_shownHintId (onRoutedAction).
    publish(std::make_shared<input::TGameActionTutorialHintConfirmed>(m_shownHintId));
}

}
