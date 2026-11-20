#include "sessiondumphintsprocessor.h"

#include "tutorial/tutorialciptypes.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>

#include <algorithm>
#include <memory>

namespace tibia::tutorial {

namespace {

using A = input::EGameAction;

}

QString hintsPathForDump(const QString& dumpPath)
{
    QFileInfo info(dumpPath);
    if (!info.exists())
        return {};
    const QString sibling = info.path() + QLatin1Char('/') + info.completeBaseName() + QStringLiteral(".hints");
    return QFileInfo::exists(sibling) ? sibling : QString{};
}

TSessiondumpHintsProcessor::TSessiondumpHintsProcessor(QObject* parent)
    : cip::tibia::tutorial::TSessiondumpHintsProcessor(parent)
{
    tutorial::ownProductSurface(this);
}

TSessiondumpHintsProcessor::~TSessiondumpHintsProcessor() { stop(); }

bool TSessiondumpHintsProcessor::load(const QString& hintsPath)
{
    stop();
    m_actionsByTs = {};
    if (hintsPath.isEmpty())
        return false;
    QFile file(hintsPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return false;
    }
    m_actionsByTs = doc.object().value(QLatin1String("actions")).toObject();
    return !m_actionsByTs.isEmpty();
}

void TSessiondumpHintsProcessor::start()
{
    stop();
    if (m_actionsByTs.isEmpty())
        return;
    m_running = true;
    for (auto it = m_actionsByTs.begin(); it != m_actionsByTs.end(); ++it) {
        bool ok = false;
        const qint64 ts = it.key().toLongLong(&ok);
        if (!ok)
            continue;
        auto* timer = new QTimer(this);
        timer->setSingleShot(true);
        timer->setInterval(static_cast<int>(std::max<qint64>(0, ts)));
        connect(timer, &QTimer::timeout, this, [this, ts] { fireBucket(ts); });
        m_timers.append(timer);
        timer->start();
    }
}

void TSessiondumpHintsProcessor::stop()
{
    m_running = false;
    for (QTimer* timer : m_timers) {
        timer->stop();
        timer->deleteLater();
    }
    m_timers.clear();
}

void TSessiondumpHintsProcessor::fireBucket(const qint64 timestampMs)
{
    if (!m_running)
        return;
    const QJsonArray bucket = m_actionsByTs.value(QString::number(timestampMs)).toArray();
    for (const QJsonValue& value : bucket) {
        if (value.isObject())
            dispatchAction(value.toObject());
    }
}

void TSessiondumpHintsProcessor::dispatchAction(const QJsonObject& action)
{
    const QString type = action.value(QLatin1String("type")).toString();
    if (type == QLatin1String("TUTORIAL_PROGRESS")) {
        Q_EMIT publishGameAction(std::make_shared<input::TGameActionTutorialProgress>(action.value(QLatin1String("tutorialprogress")).toString()));
        return;
    }
    if (type == QLatin1String("SKIP_TO")) {
        // 0 is a no-op / restart cue in CIP sidecars, not the end.
        Q_EMIT publishGameAction(std::make_shared<input::TGameActionSessiondumpSkipTo>(
            static_cast<int64_t>(action.value(QLatin1String("skiptotimestamp")).toDouble())));
        return;
    }
    if (type == QLatin1String("CONDITION")) {
        const QString condition = action.value(QLatin1String("conditiontype")).toString();
        const QJsonObject data = action.value(QLatin1String("conditiondata")).toObject();
        if (condition == QLatin1String("SHOW_TUTORIAL_HINT") || data.contains(QLatin1String("showtutorialhintid"))) {
            Q_EMIT publishGameAction(std::make_shared<input::TGameActionShowTutorialHint>(data.value(QLatin1String("showtutorialhintid")).toInt()));
            return;
        }
        // Text-hint conditions: surface as status-line style tutorial progress until overlays land.
        const QJsonObject textHint = data.value(QLatin1String("texthint")).toObject();
        if (!textHint.isEmpty()) {
            Q_EMIT publishGameAction(std::make_shared<input::TGameActionTutorialProgress>(textHint.value(QLatin1String("text")).toString()));
        }
        return;
    }
    if (type == QLatin1String("SWITCH_CHANNEL") || type == QLatin1String("CLEAR_OLDEST_MESSAGE")
        || type == QLatin1String("FINISHED_TUTORIAL")) {
        if (type == QLatin1String("FINISHED_TUTORIAL")) {
            Q_EMIT publishGameAction(std::make_shared<input::TGameActionFinishedTutorialForCharacter>(QString()));
            return;
        }
        return;
    }
}

}
