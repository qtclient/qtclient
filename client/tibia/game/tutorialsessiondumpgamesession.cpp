#include "game/tutorialsessiondumpgamesession.h"

#include "game/gameactionrouter.h"
#include "input/gameaction.h"
#include "sessiondump/sessiondumpplayer.h"
#include "tutorial/sessiondumphints.h"
#include "tutorial/sessiondumphintsprocessor.h"
#include "tutorial/tutorialprogressuploader.h"
#include "tutorial/tutorialsessiondumpgameactionhandler.h"

#include "jsondocumenthttppostsender.h"

#include <QDebug>
#include <QEventLoop>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

#include <cstdint>
#include <exception>
#include <stdexcept>

namespace tibia::game {

TTutorialSessiondumpGameSession::TTutorialSessiondumpGameSession(QObject* parent)
    : TSessiondumpGameSession(parent)
{
}

TTutorialSessiondumpGameSession::~TTutorialSessiondumpGameSession() = default;

void TTutorialSessiondumpGameSession::initialize(const std::shared_ptr<const TGameSessionMetaInfo>& metaInfo)
{
    TSessiondumpGameSession::initialize(metaInfo);
    m_tutorialMetaInfo = std::dynamic_pointer_cast<const TTutorialSessiondumpGameSessionMetaInfo>(metaInfo);
    if (!m_tutorialMetaInfo)
        throw std::invalid_argument("wrong type of TGameSessionMetaInfo submitted");
    connect(this, &IGameSession::disconnected, this, &TTutorialSessiondumpGameSession::onDisconnected);

    TInternalGameActionRouter* router = gameActionRouter()->internalRouter();
    m_tutorialProgressUploader = std::make_shared<tutorial::TTutorialProgressUploader>(
        m_tutorialMetaInfo->tutorialProgressWebService, m_tutorialMetaInfo->clientSessionId, m_tutorialMetaInfo->characterName,
        std::make_shared<shared::TJsonDocumentHttpPostSender>());
    using tutorial::TTutorialSessiondumpGameActionHandler;
    m_tutorialSessiondumpGameActionHandler =
        std::make_shared<TTutorialSessiondumpGameActionHandler>(m_tutorialProgressUploader, m_tutorialMetaInfo->characterName);
    TTutorialSessiondumpGameActionHandler* tutorialActions = m_tutorialSessiondumpGameActionHandler.get();
    connect(router, &TInternalGameActionRouter::publishGameActionInternal, tutorialActions,
            &TTutorialSessiondumpGameActionHandler::handleGameAction);
    connect(tutorialActions, &TTutorialSessiondumpGameActionHandler::publishGameAction, router,
            &TInternalGameActionRouter::handleGameActionInternal);
}

void TTutorialSessiondumpGameSession::startSession()
{
    TSessiondumpGameSession::startSession();
    gameActionRouter()->handleGameAction(std::make_shared<input::TGameActionChangeTutorialModeEnabled>(true));
}

void TTutorialSessiondumpGameSession::startSessiondump(const sessiondump::TSessiondumpCatalog::TEntry& entry)
{
    TSessiondumpGameSession::startSessiondump(entry);

    QNetworkAccessManager network;
    QNetworkRequest request;
    request.setUrl(entry.hintsUrl);
    QNetworkReply* reply = network.get(request);
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    if (!reply->isFinished())
        loop.exec();
    const QJsonDocument document = QJsonDocument::fromJson(reply->readAll());
    reply->deleteLater();
    try {
        m_sessiondumpHints = std::make_shared<tutorial::TSessiondumpHints>(document);
    } catch (const std::exception& e) {
        // Host: a missing or malformed hints file leaves the tutorial without hints.
        qWarning() << "Invalid sessiondump hints" << entry.hintsUrl << e.what();
        return;
    }

    // Host: the hints processor reads the hints when it is made, so it is made
    // once they are loaded (CipSoft makes it in initialize).
    using tutorial::TSessiondumpHintsProcessor;
    m_sessiondumpHintsProcessor = std::make_shared<TSessiondumpHintsProcessor>(m_sessiondumpHints);
    TInternalGameActionRouter* router = gameActionRouter()->internalRouter();
    connect(router, &TInternalGameActionRouter::publishGameActionInternal, m_sessiondumpHintsProcessor.get(),
            &TSessiondumpHintsProcessor::handleGameAction);
    connect(m_sessiondumpHintsProcessor.get(), &TSessiondumpHintsProcessor::publishGameAction, router,
            &TInternalGameActionRouter::handleGameActionInternal);
}

void TTutorialSessiondumpGameSession::onSessiondumpTimerElapsed()
{
    if (m_sessiondumpHintsProcessor) {
        sessiondump::TSessiondumpPlayer* player = sessiondumpPlayer().get();
        player->setPauseAt(m_sessiondumpHintsProcessor->nextHintTimestamp());
        const int64_t progress = player->progress();
        if (m_sessiondumpHintsProcessor->hasHintToProcess(progress)) {
            Q_EMIT player->paused();
            m_sessiondumpHintsProcessor->processHints(progress);
        }
        if (!m_sessiondumpHintsProcessor->hasHintToProcess(progress) && !player->isFinishedLoading())
            player->resume();
    }
    TSessiondumpGameSession::onSessiondumpTimerElapsed();
}

void TTutorialSessiondumpGameSession::onDisconnected()
{
    gameActionRouter()->handleGameAction(std::make_shared<input::TGameActionChangeTutorialModeEnabled>(false));
}

}
