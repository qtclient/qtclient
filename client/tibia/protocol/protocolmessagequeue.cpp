#include "protocol/protocolmessagequeue.h"

#include "protocol/protocolciptypes.h"

#include <protocol.pb.h>

#include <exception>

namespace tibia::protocol {

namespace {

/// Marks a drain as running for its lifetime; acquired() is false when one already runs.
class TReentrancyScope
{
public:
    explicit TReentrancyScope(bool& active) : m_active(active), m_acquired(!active) { m_active = true; }
    ~TReentrancyScope()
    {
        if (m_acquired)
            m_active = false;
    }
    TReentrancyScope(const TReentrancyScope&) = delete;
    TReentrancyScope& operator=(const TReentrancyScope&) = delete;

    bool acquired() const { return m_acquired; }

private:
    bool& m_active;
    bool m_acquired;
};

/// Filled with the default (no delay) the first time a queue is built without one.
std::shared_ptr<TProtocolMessageQueueConfiguration> defaultConfiguration;

} // namespace

TProtocolMessageQueue::TProtocolMessageQueue(QObject* parent)
    : TProtocolMessageQueue(defaultConfiguration, parent)
{
}

TProtocolMessageQueue::TProtocolMessageQueue(std::shared_ptr<TProtocolMessageQueueConfiguration>& configuration,
                                             QObject* parent)
    : cip::tibia::protocol::TProtocolMessageQueue(parent)
{
    if (!configuration)
        configuration = std::make_shared<TProtocolMessageQueueConfiguration>();
    m_configuration = configuration;

    m_gameserverMessageTimer.setSingleShot(true);
    m_gameserverMessageTimer.setInterval(m_configuration->gameserverMessageDelay);
    connect(&m_gameserverMessageTimer, &QTimer::timeout, this, &TProtocolMessageQueue::fireEmitSignalForNewProtocolMessage);
    m_gameclientMessageTimer.setSingleShot(true);
    m_gameclientMessageTimer.setInterval(m_configuration->gameclientMessageDelay);
    connect(&m_gameclientMessageTimer, &QTimer::timeout, this,
            &TProtocolMessageQueue::fireEmitSignalForNewClientProtocolMessage);

    registerServerMessages();
}

TProtocolMessageQueue::~TProtocolMessageQueue() = default;

void TProtocolMessageQueue::enqueueGameserverMessage(const std::shared_ptr<GameserverMessage>& message)
{
    m_gameserverMessages.push_back(message);
    if (m_gameserverMessageTimer.interval() != 0) {
        if (!m_gameserverMessageTimer.isActive())
            m_gameserverMessageTimer.start();
        return;
    }
    fireEmitSignalForNewProtocolMessage();
}

void TProtocolMessageQueue::enqueueGameclientMessage(const std::shared_ptr<GameclientMessage>& message)
{
    // Host side: the connection writes classic wire bytes, so encode them here.
    const auto* translator = clientMessageTranslator();
    enqueueGameclientMessage(message, translator && message ? translator->translateToClassic(*message) : QByteArray());
}

void TProtocolMessageQueue::enqueueGameclientMessage(const std::shared_ptr<GameclientMessage>& message,
                                                     const QByteArray& wire)
{
    ++m_clientCount;
    m_gameclientMessages.emplace_back(message, wire);
    Q_EMIT sendMessage(message);
    if (m_gameclientMessageTimer.interval() == 0) {
        fireEmitSignalForNewClientProtocolMessage();
        return;
    }
    if (!m_gameclientMessageTimer.isActive())
        m_gameclientMessageTimer.start();
}

void TProtocolMessageQueue::enqueueServer(std::unique_ptr<GameserverMessage> message)
{
    if (!message)
        return;
    ++m_serverCount;
    enqueueGameserverMessage(std::shared_ptr<GameserverMessage>(std::move(message)));
}

void TProtocolMessageQueue::enqueueClient(std::unique_ptr<GameclientMessage> message, const QByteArray& wire)
{
    enqueueGameclientMessage(std::shared_ptr<GameclientMessage>(std::move(message)), wire);
}

void TProtocolMessageQueue::prepareAndEnqueueGameclientMessage(std::shared_ptr<GameclientMessage> pGameclientMessage,
                                                               const int GameclientMessageType)
{
    pGameclientMessage->set_type(GameclientMessageType);
    enqueueGameclientMessage(pGameclientMessage);
}

void TProtocolMessageQueue::handleGameAction(std::shared_ptr<const ::tibia::input::IGameAction>)
{
}

void TProtocolMessageQueue::fireEmitSignalForNewProtocolMessage()
{
    TReentrancyScope scope(m_gameserverMessageGuard.active);
    if (!scope.acquired()) {
        if (!m_gameserverMessageTimer.isActive())
            m_gameserverMessageTimer.start();
        return;
    }
    while (!m_gameserverMessages.empty()) {
        std::shared_ptr<GameserverMessage> message = std::move(m_gameserverMessages.front());
        m_gameserverMessages.pop_front();
        Q_EMIT receivedMessage(message);
        dispatchGameserverMessage(message);
    }
}

void TProtocolMessageQueue::fireEmitSignalForNewClientProtocolMessage()
{
    TReentrancyScope scope(m_gameclientMessageGuard.active);
    if (!scope.acquired()) {
        if (!m_gameclientMessageTimer.isActive())
            m_gameclientMessageTimer.start();
        return;
    }
    while (!m_gameclientMessages.empty()) {
        auto [message, wire] = std::move(m_gameclientMessages.front());
        m_gameclientMessages.pop_front();
        Q_EMIT clientMessageReadyToProcess(message);
        const int opcode = message && message->has_type() ? message->type()
                                                          : (wire.isEmpty() ? -1 : int(uint8_t(wire[0])));
        Q_EMIT clientMessageEnqueued(opcode, wire);
    }
}

void TProtocolMessageQueue::dispatchGameserverMessage(const std::shared_ptr<GameserverMessage>& message)
{
    m_lastGameserverMessage = message;
    const auto handler = m_gameserverMessageHandlers.find(message->type());
    if (handler != m_gameserverMessageHandlers.end()) {
        // A handler that rejects the message (CIP's "unhandled enum value" /
        // invalid mode throws) drops that message only: a server sending a value
        // CipSoft's never does must not take the whole client down.
        try {
            handler->second(*message);
        } catch (const std::exception&) {
        }
    }
    Q_EMIT serverMessageEnqueued(message->has_type() ? message->type() : -1);
}

} // namespace tibia::protocol
