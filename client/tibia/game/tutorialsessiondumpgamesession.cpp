#include "game/tutorialsessiondumpgamesession.h"

#include "creatures/creaturestorage.h"
#include "effects/effectgameactionhandler.h"
#include "effects/effectstorage.h"
#include "game/gameactionrouter.h"
#include "input/gameaction.h"
#include "protocol/protocolmessagequeue.h"
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

TTutorialSessiondumpGameSession::TTutorialSessiondumpGameSession(
    std::shared_ptr<shared::TTibiaFileSystemHelper> tibiaFileSystemHelper,
    std::shared_ptr<appearances::TAppearancesManager> appearancesManager,
    std::shared_ptr<appearances::TAppearanceInstanceFactory> appearanceInstanceFactory,
    std::shared_ptr<config::TClientOptions> clientOptions, std::shared_ptr<shared::TTibiaClipboardHelper> clipboardHelper,
    std::shared_ptr<shared::TTibiaUrlTools> urlTools, std::shared_ptr<shared::IClockService> clientClockService,
    std::shared_ptr<analysers::TItemPriceStorage> itemPriceStorage, std::shared_ptr<sound::TSoundController> soundController,
    std::shared_ptr<input::TKeyboardShortcutHandler> keyboardShortcutHandler, spells::TSpellInfoStorage* spellInfoStorage,
    TPassiveAbilityInfoStorage* passiveAbilityInfoStorage, proficiency::TProficiencyStorage* proficiencyStorage,
    std::shared_ptr<gamewindow::TDialogStackController> dialogStackController, QObject* parent)
    : TSessiondumpGameSession(std::move(tibiaFileSystemHelper), std::move(appearancesManager), std::move(appearanceInstanceFactory),
                       std::move(clientOptions), std::move(clipboardHelper), std::move(urlTools),
                       std::move(clientClockService), std::move(itemPriceStorage), std::move(soundController),
                       std::move(keyboardShortcutHandler), spellInfoStorage, passiveAbilityInfoStorage, proficiencyStorage,
                       std::move(dialogStackController), parent)
{
}

TTutorialSessiondumpGameSession::~TTutorialSessiondumpGameSession() = default;

void TTutorialSessiondumpGameSession::initialize(const std::shared_ptr<const TGameSessionMetaInfo>& metaInfo)
{
    TSessiondumpGameSession::initialize(metaInfo);
    // The tutorial loads no character's configuration.
    m_configurationCharacterId = 0;
    m_tutorialMetaInfo = std::dynamic_pointer_cast<const TTutorialSessiondumpGameSessionMetaInfo>(metaInfo);
    if (!m_tutorialMetaInfo)
        throw std::invalid_argument("wrong type of TGameSessionMetaInfo submitted");
    connect(protocolMessageQueue().get(), &protocol::TProtocolMessageQueue::receivedPlayerDataBasicMessage, this,
            &TTutorialSessiondumpGameSession::onPlayerDataBasicReceived);
    connect(this, &IGameSession::disconnected, this, &TTutorialSessiondumpGameSession::onDisconnected);

    TInternalGameActionRouter* router = gameActionRouter()->internalRouter();
    // The hints processor over the hints the sessiondump's start loaded,
    // linked both ways with the router. Host: a missing or malformed hints
    // file leaves the tutorial without hints.
    if (m_sessiondumpHints) {
        using tutorial::TSessiondumpHintsProcessor;
        m_sessiondumpHintsProcessor = std::make_shared<TSessiondumpHintsProcessor>(m_sessiondumpHints);
        connect(router, &TInternalGameActionRouter::publishGameActionInternal, m_sessiondumpHintsProcessor.get(),
                &TSessiondumpHintsProcessor::handleGameAction);
        connect(m_sessiondumpHintsProcessor.get(), &TSessiondumpHintsProcessor::publishGameAction, router,
                &TInternalGameActionRouter::handleGameActionInternal);
    }
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

void TTutorialSessiondumpGameSession::createGameActionProvidersAndHandlers()
{
    TSessiondumpGameSession::createGameActionProvidersAndHandlers();
    m_effectGameActionHandler = std::make_shared<effects::TEffectGameActionHandler>(*effectStorage());
    connect(gameActionRouter()->internalRouter(), &TInternalGameActionRouter::publishGameActionInternal,
            m_effectGameActionHandler.get(), &effects::TEffectGameActionHandler::handleGameAction);
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
    }
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

void TTutorialSessiondumpGameSession::onSessiondumpFinished()
{
    Q_EMIT disconnected();
}

void TTutorialSessiondumpGameSession::onPlayerDataBasicReceived()
{
    const std::shared_ptr<creatures::TCreature> player = creatureStorage()->player();
    if (!player)
        return;
    player->setName(m_tutorialMetaInfo->characterName);
    // Host: a missing or malformed hints file leaves the dump's outfit.
    if (m_sessiondumpHints)
        player->setOutfitAppearance(m_sessiondumpHints->createOutfitInstance(m_tutorialMetaInfo->isMale));
}

void TTutorialSessiondumpGameSession::onDisconnected()
{
    gameActionRouter()->handleGameAction(std::make_shared<input::TGameActionChangeTutorialModeEnabled>(false));
}

}
