// Contract test for the Qt kit's network layer on the web, in a real browser:
// QNetworkAccessManager over fetch and QWebSocket over the browser WebSocket
// (qtclient/Qt5), with JSPI event delivery (QWasmSuspendResumeControl). It
// talks to the endpoints run.mjs serves on the page's own origin.
//
// It guards what the launcher and the game rely on: downloads streamed in
// chunks, POSTs, HTTP errors, aborts, redirects, a nested event loop inside a
// reply handler, and a binary WebSocket session with a server close.

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>
#include <QWebSocket>

#include <emscripten.h>
#include <emscripten/val.h>

#include <string>

namespace {

int g_failures = 0;
std::string g_log;

void note(const QString& line)
{
    g_log += line.toStdString() + "\n";
    qInfo().noquote() << line;
}

void check(bool ok, const QString& what)
{
    if (!ok) {
        ++g_failures;
        note(QStringLiteral("FAIL ") + what);
    }
}

QString origin()
{
    return QString::fromStdString(emscripten::val::global("location")["origin"].as<std::string>());
}

QUrl at(const QString& path)
{
    return QUrl(origin() + path);
}

// Run a nested event loop until the reply finishes (or a timeout).
bool waitFinished(QNetworkReply* reply, int timeoutMs = 20000)
{
    if (reply->isFinished())
        return true;
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);
    loop.exec();
    return reply->isFinished();
}

QByteArray pattern(qsizetype size)
{
    QByteArray bytes(size, Qt::Uninitialized);
    for (qsizetype i = 0; i < size; ++i)
        bytes[i] = char((i * 31 + 7) & 0xff);
    return bytes;
}

void testGet(QNetworkAccessManager& nam)
{
    QNetworkReply* reply = nam.get(QNetworkRequest(at(QStringLiteral("/text"))));
    check(waitFinished(reply), QStringLiteral("GET /text finishes"));
    check(reply->error() == QNetworkReply::NoError, QStringLiteral("GET /text: no error"));
    check(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200,
          QStringLiteral("GET /text: status 200"));
    check(reply->rawHeader("X-Test") == "1", QStringLiteral("GET /text: response header"));
    check(reply->readAll() == "hello", QStringLiteral("GET /text: body"));
    reply->deleteLater();
}

void testPost(QNetworkAccessManager& nam)
{
    QNetworkRequest request(at(QStringLiteral("/echo")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    const QByteArray body = R"({"type":"login","n":1})";
    QNetworkReply* reply = nam.post(request, body);
    check(waitFinished(reply), QStringLiteral("POST /echo finishes"));
    check(reply->error() == QNetworkReply::NoError, QStringLiteral("POST /echo: no error"));
    check(reply->header(QNetworkRequest::ContentTypeHeader).toString() == QStringLiteral("application/json"),
          QStringLiteral("POST /echo: content type round trip"));
    check(reply->readAll() == body, QStringLiteral("POST /echo: body round trip"));
    reply->deleteLater();
}

void testStreamed(QNetworkAccessManager& nam)
{
    // 3 MiB in several server writes: readyRead more than once, bytes in order.
    QNetworkReply* reply = nam.get(QNetworkRequest(at(QStringLiteral("/big"))));
    QByteArray got;
    int readyReads = 0;
    QObject::connect(reply, &QNetworkReply::readyRead, reply, [&] {
        ++readyReads;
        got += reply->readAll();
    });
    check(waitFinished(reply), QStringLiteral("GET /big finishes"));
    got += reply->readAll();
    check(reply->error() == QNetworkReply::NoError, QStringLiteral("GET /big: no error"));
    check(got == pattern(3 << 20), QStringLiteral("GET /big: 3 MiB intact"));
    check(readyReads > 1, QStringLiteral("GET /big: streamed (readyRead %1x)").arg(readyReads));
    reply->deleteLater();
}

void testHttpError(QNetworkAccessManager& nam)
{
    QNetworkReply* reply = nam.get(QNetworkRequest(at(QStringLiteral("/missing"))));
    check(waitFinished(reply), QStringLiteral("GET /missing finishes"));
    check(reply->error() == QNetworkReply::ContentNotFoundError, QStringLiteral("GET /missing: ContentNotFoundError"));
    check(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 404,
          QStringLiteral("GET /missing: status 404"));
    reply->deleteLater();
}

void testRedirect(QNetworkAccessManager& nam)
{
    QNetworkReply* reply = nam.get(QNetworkRequest(at(QStringLiteral("/redirect"))));
    check(waitFinished(reply), QStringLiteral("GET /redirect finishes"));
    check(reply->error() == QNetworkReply::NoError, QStringLiteral("GET /redirect: no error"));
    check(reply->readAll() == "hello", QStringLiteral("GET /redirect: followed to /text"));
    reply->deleteLater();
}

void testAbort(QNetworkAccessManager& nam)
{
    QNetworkReply* reply = nam.get(QNetworkRequest(at(QStringLiteral("/slow"))));
    QTimer::singleShot(100, reply, &QNetworkReply::abort);
    QElapsedTimer timer;
    timer.start();
    check(waitFinished(reply, 4000), QStringLiteral("abort finishes the reply"));
    check(timer.elapsed() < 3000, QStringLiteral("abort is prompt"));
    check(reply->error() == QNetworkReply::OperationCanceledError, QStringLiteral("abort: OperationCanceledError"));
    reply->deleteLater();
}

void testNestedLoop(QNetworkAccessManager& nam)
{
    // A handler that runs its own event loop (a modal wait) while other
    // replies' events are queued: the nested loop delivers them, and the
    // outer delivery must cope with the queue it left behind.
    QNetworkReply* first = nam.get(QNetworkRequest(at(QStringLiteral("/text"))));
    QNetworkReply* other = nam.get(QNetworkRequest(at(QStringLiteral("/big"))));
    bool innerOk = false;
    QObject::connect(first, &QNetworkReply::finished, first, [&] {
        QNetworkReply* inner = nam.get(QNetworkRequest(at(QStringLiteral("/text"))));
        innerOk = waitFinished(inner) && inner->readAll() == "hello";
        inner->deleteLater();
    });
    check(waitFinished(first), QStringLiteral("nested: outer reply finishes"));
    check(waitFinished(other), QStringLiteral("nested: concurrent reply finishes"));
    check(innerOk, QStringLiteral("nested: reply inside a handler's event loop"));
    check(other->readAll().size() == (3 << 20), QStringLiteral("nested: concurrent reply intact"));
    first->deleteLater();
    other->deleteLater();
}

void testWebSocket()
{
    QUrl url = at(QStringLiteral("/ws"));
    url.setScheme(url.scheme() == QStringLiteral("https") ? QStringLiteral("wss") : QStringLiteral("ws"));
    QWebSocket socket;
    QEventLoop loop;
    QList<QByteArray> echoed;
    QString text;
    bool connected = false;
    bool disconnected = false;
    const QByteArray big = pattern(100'000);
    QObject::connect(&socket, &QWebSocket::connected, &loop, [&] {
        connected = true;
        socket.sendBinaryMessage(QByteArray("\x01\x02\x03", 3));
        socket.sendBinaryMessage(big);
        socket.sendTextMessage(QStringLiteral("ping"));
    });
    QObject::connect(&socket, &QWebSocket::binaryMessageReceived, &loop, [&](const QByteArray& message) {
        echoed << message;
    });
    QObject::connect(&socket, &QWebSocket::textMessageReceived, &loop, [&](const QString& message) {
        text = message;
        // The server closes on "close" with 4000 "bye".
        socket.sendTextMessage(QStringLiteral("close"));
    });
    QObject::connect(&socket, &QWebSocket::disconnected, &loop, [&] {
        disconnected = true;
        loop.quit();
    });
    QTimer::singleShot(20000, &loop, &QEventLoop::quit);
    socket.open(url);
    loop.exec();
    check(connected, QStringLiteral("websocket: connected"));
    check(echoed.size() == 2 && echoed.value(0) == QByteArray("\x01\x02\x03", 3) && echoed.value(1) == big,
          QStringLiteral("websocket: binary frames echoed in order"));
    check(text == QStringLiteral("ping"), QStringLiteral("websocket: text frame echoed"));
    check(disconnected, QStringLiteral("websocket: disconnected on server close"));
    check(socket.closeCode() == QWebSocketProtocol::CloseCode(4000), QStringLiteral("websocket: close code 4000 (got %1)").arg(int(socket.closeCode())));
    check(socket.closeReason() == QStringLiteral("bye"), QStringLiteral("websocket: close reason"));
}

void testWebSocketRefused()
{
    // The server refuses the upgrade: an error and a disconnect, never a hang.
    QUrl url = at(QStringLiteral("/no-ws"));
    url.setScheme(url.scheme() == QStringLiteral("https") ? QStringLiteral("wss") : QStringLiteral("ws"));
    QWebSocket socket;
    QEventLoop loop;
    bool errored = false;
    QObject::connect(&socket, &QWebSocket::errorOccurred, &loop, [&] { errored = true; });
    QObject::connect(&socket, &QWebSocket::disconnected, &loop, &QEventLoop::quit);
    QTimer::singleShot(10000, &loop, &QEventLoop::quit);
    socket.open(url);
    loop.exec();
    check(errored, QStringLiteral("websocket: refused upgrade reports an error"));
    check(socket.state() == QAbstractSocket::UnconnectedState, QStringLiteral("websocket: refused ends unconnected"));
}

// One statement per field: a comma inside EM_ASM code splits macro arguments.
void report(int failures, const std::string& log)
{
    EM_ASM({
        var result = {};
        result.failures = $0;
        result.log = UTF8ToString($1);
        globalThis.qtNetworkContract = result;
    }, failures, log.c_str());
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QTimer::singleShot(0, &app, [] {
        QNetworkAccessManager nam;
        testGet(nam);
        testPost(nam);
        testStreamed(nam);
        testHttpError(nam);
        testRedirect(nam);
        testAbort(nam);
        testNestedLoop(nam);
        testWebSocket();
        testWebSocketRefused();
        note(g_failures ? QStringLiteral("contract: %1 failure(s)").arg(g_failures) : QStringLiteral("contract: ok"));
        report(g_failures, g_log);
    });
    return app.exec();
}
