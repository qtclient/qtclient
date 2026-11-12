#include "sessiondumphintsprocessor.h"

#include "tutorial/tutorialciptypes.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLoggingCategory>

#include <algorithm>
#include <memory>

Q_LOGGING_CATEGORY(lcHints, "tibia.tutorial.hints")

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
        qCWarning(lcHints).noquote() << "cannot open" << hintsPath;
        return false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        qCWarning(lcHints).noquote() << "invalid hints JSON" << hintsPath;
        return false;
    }
    m_actionsByTs = doc.object().value(QLatin1String("actions")).toObject();
    qCInfo(lcHints).noquote() << "loaded" << m_actionsByTs.size() << "hint buckets from" << hintsPath;
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
    qCInfo(lcHints) << "fire" << timestampMs << "actions" << bucket.size();
    for (const QJsonValue& value : bucket) {
        if (value.isObject())
            dispatchAction(value.toObject());
    }
}

void TSessiondumpHintsProcessor::dispatchAction(const QJsonObject& action)
{
    const QString type = action.value(QLatin1String("type")).toString();
    if (type == QLatin1String("TUTORIAL_PROGRESS")) {
        auto a = input::mutableClone(*input::makeAction(A::TUTORIAL_PROGRESS));
        a->text = action.value(QLatin1String("tutorialprogress")).toString();
        input::publish(a);
        return;
    }
    if (type == QLatin1String("SKIP_TO")) {
        auto a = input::mutableClone(*input::makeAction(A::SESSIONDUMP_SKIP_TO));
        a->parameter = action.value(QLatin1String("skiptotimestamp")).toInt();
        // Do not treat 0 as "end" — that is a no-op / restart cue in CIP sidecars.
        a->text = QStringLiteral("seek");
        input::publish(a);
        return;
    }
    if (type == QLatin1String("CONDITION")) {
        const QString condition = action.value(QLatin1String("conditiontype")).toString();
        const QJsonObject data = action.value(QLatin1String("conditiondata")).toObject();
        if (condition == QLatin1String("SHOW_TUTORIAL_HINT") || data.contains(QLatin1String("showtutorialhintid"))) {
            auto a = input::mutableClone(*input::makeAction(A::SHOW_TUTORIAL_HINT));
            a->parameter = data.value(QLatin1String("showtutorialhintid")).toInt();
            input::publish(a);
            return;
        }
        // Text-hint conditions: surface as status-line style tutorial progress until overlays land.
        const QJsonObject textHint = data.value(QLatin1String("texthint")).toObject();
        if (!textHint.isEmpty()) {
            auto a = input::mutableClone(*input::makeAction(A::TUTORIAL_PROGRESS));
            a->text = textHint.value(QLatin1String("text")).toString();
            a->statement = condition;
            input::publish(a);
        }
        qCInfo(lcHints).noquote() << "condition" << condition << "(overlay deferred)";
        return;
    }
    if (type == QLatin1String("SWITCH_CHANNEL") || type == QLatin1String("CLEAR_OLDEST_MESSAGE")
        || type == QLatin1String("FINISHED_TUTORIAL")) {
        if (type == QLatin1String("FINISHED_TUTORIAL")) {
            input::publish(input::makeAction(A::FINISHED_TUTORIAL_FOR_CHARACTER));
            return;
        }
        qCInfo(lcHints).noquote() << "action" << type;
        return;
    }
    qCInfo(lcHints).noquote() << "unhandled hint action" << type;
}

}
