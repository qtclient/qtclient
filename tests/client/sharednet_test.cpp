// CIP shared:: classes of the launcher's net slice (apps/qtclient/shared/src).
#include <gtest/gtest.h>

#include "colorhelper.h"
#include "crashreportsender.h"
#include "crashhandler.h"
#include "extent.h"
#include "highlighttimer.h"
#include "iclockservice.h"
#include "jsonhelper.h"
#include "paginationproxymodel.h"
#include "qtconnectionwrapper.h"
#include "ringbufferstopwatch.h"
#include "singlethreadedlowpriorityexecutor.h"
#include "synchronousexecutor.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringListModel>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QTimer>

#include <atomic>

using namespace tibia::shared;

namespace {

class TFakeClock : public IClockService
{
public:
    qint64 nanos = 0;
    qint64 nowMillis() override { return nanos / 1000000; }
    qint64 nowNanos() override { return nanos; }
    qint64 nowMicros() override { return nanos / 1000; }
    QDateTime currentDateTime() override { return {}; }
};

class TRecordingTask : public QRunnable
{
public:
    std::atomic<bool> ran { false };
    std::atomic<Qt::HANDLE> thread { nullptr };
    void run() override
    {
        thread = QThread::currentThreadId();
        ran = true;
    }
};

/// Spins the event loop until `done` or `timeoutMs`.
template<typename Predicate>
bool waitFor(Predicate done, const int timeoutMs = 2000)
{
    QEventLoop loop;
    QTimer deadline;
    deadline.setSingleShot(true);
    deadline.start(timeoutMs);
    while (!done() && deadline.isActive())
        loop.processEvents(QEventLoop::AllEvents, 10);
    return done();
}

}

TEST(Extent, IsTheOffsetBetweenTwoCoordinates)
{
    const TExtent extent(TCoordinate(1, 2, 3), TCoordinate(4, 7, 3));
    EXPECT_EQ(extent.x, 3);
    EXPECT_EQ(extent.y, 5);
    EXPECT_EQ(extent.z, 0);
    const TExtent sum = extent + TExtent(1, 1, 1);
    EXPECT_EQ(sum.x, 4);
    EXPECT_EQ(sum.z, 1);
    const TExtent difference = extent - TExtent(1, 1, 1);
    EXPECT_EQ(difference.y, 4);
    EXPECT_EQ(difference.z, -1);
}

TEST(ColorHelper, EightBitCubeAndOutfitPalette)
{
    EXPECT_EQ(colorFromEightBit(0), QColor(0, 0, 0));
    EXPECT_EQ(colorFromEightBit(215), QColor(255, 255, 255));
    EXPECT_EQ(colorFromEightBit(216), QColor(0, 0, 0));
    EXPECT_EQ(colorFromEightBit(36 + 6 * 2 + 3), QColor(51, 102, 153));
    EXPECT_EQ(eightBitFromColor(QColor(51, 102, 153)), 36 + 12 + 3);
    // Column 0 is the grey ramp, white on top.
    EXPECT_EQ(outfitColor(0), QColor(255, 255, 255));
    EXPECT_EQ(outfitColor(200), QColor(Qt::white));
    for (const int index : { 1, 18, 19, 77, 132 })
        EXPECT_EQ(outfitColorIndex(outfitColor(index)), index) << index;
}

TEST(SynchronousExecutor, RunsTheTaskAndReportsIt)
{
    TSynchronousExecutor executor;
    TRecordingTask task;
    task.setAutoDelete(false);
    QRunnable* executed = nullptr;
    QObject::connect(&executor, &TExecutor::executed, [&](QRunnable* t) { executed = t; });
    executor.start(&task);
    EXPECT_TRUE(task.ran);
    EXPECT_EQ(executed, &task);
}

TEST(SingleThreadedLowPriorityExecutor, RunsOnItsThreadAndReportsOnTheCallers)
{
    TSingleThreadedLowPriorityExecutor executor;
    TRecordingTask task;
    task.setAutoDelete(false);
    QRunnable* executed = nullptr;
    Qt::HANDLE reportedOn = nullptr;
    QObject::connect(&executor, &TExecutor::executed, [&](QRunnable* t) {
        executed = t;
        reportedOn = QThread::currentThreadId();
    });
    executor.start(&task);
    ASSERT_TRUE(waitFor([&] { return executed != nullptr; }));
    EXPECT_EQ(executed, &task);
    EXPECT_NE(task.thread.load(), QThread::currentThreadId());
    EXPECT_EQ(reportedOn, QThread::currentThreadId());
}

TEST(RingBufferStopWatch, KeepsTheLastDurations)
{
    auto clock = std::make_shared<TFakeClock>();
    TRingBufferStopWatch watch(3, clock);
    EXPECT_EQ(watch.elapsed(), 0);
    for (const qint64 duration : { 10, 20, 30, 40 }) {
        watch.start();
        clock->nanos += duration;
        EXPECT_EQ(watch.elapsed(), duration);
        watch.stop();
    }
    EXPECT_EQ(watch.count(), 3u);
    EXPECT_EQ(watch.capacity(), 3u);
    EXPECT_EQ(watch.minimum(), 20);
    EXPECT_EQ(watch.maximum(), 40);
    EXPECT_EQ(watch.sum(), 90);
    EXPECT_EQ(watch.average(), 30);
    EXPECT_EQ(watch.elapsed(), 40);
    watch.reset();
    EXPECT_EQ(watch.count(), 0u);
    EXPECT_FALSE(watch.isRunning());
    EXPECT_EQ(watch.average(), 0);
}

TEST(HighlightTimer, ProtectedHighlightNeedsForce)
{
    THighlightTimer timer(1000);
    int changes = 0;
    QObject::connect(&timer, &THighlightTimer::changed, [&] { ++changes; });
    timer.start(true);
    EXPECT_TRUE(timer.isHighlighted());
    timer.stop(false);
    EXPECT_TRUE(timer.isHighlighted());
    timer.stop(true);
    EXPECT_FALSE(timer.isHighlighted());
    timer.start(false);
    timer.stop(false);
    EXPECT_FALSE(timer.isHighlighted());
    EXPECT_EQ(changes, 4);
    EXPECT_EQ(timer.interval(), 1000);
}

TEST(QtConnectionWrapper, ForwardsOnlyTheFirstSignalUntilReset)
{
    TQtConnectionWrapper wrapper(0, true);
    QObject source;
    wrapper.connectTo(&source, &QObject::objectNameChanged);
    int forwarded = 0;
    QObject::connect(&wrapper, &TQtConnectionWrapper::signal, [&] { ++forwarded; });
    source.setObjectName(QStringLiteral("1"));
    source.setObjectName(QStringLiteral("2"));
    EXPECT_EQ(forwarded, 1);
    wrapper.resetEmitCount();
    source.setObjectName(QStringLiteral("3"));
    EXPECT_EQ(forwarded, 2);
    wrapper.disconnectAll();
    wrapper.resetEmitCount();
    source.setObjectName(QStringLiteral("4"));
    EXPECT_EQ(forwarded, 2);
}

TEST(PaginationProxyModel, ShowsOnePageAndClampsWhenRowsGo)
{
    QStringListModel source(QStringList { "a", "b", "c", "d", "e" });
    TPaginationProxyModel pages(2);
    pages.setSourceModel(&source);
    EXPECT_EQ(pages.numberOfPages(), 3);
    EXPECT_EQ(pages.rowCount(), 2);
    EXPECT_FALSE(pages.hasPreviousPage());
    EXPECT_TRUE(pages.hasNextPage());
    pages.setCurrentPage(3);
    EXPECT_EQ(pages.rowCount(), 1);
    EXPECT_EQ(pages.index(0, 0).data().toString(), QStringLiteral("e"));
    EXPECT_EQ(pages.rowData(pages.index(0, 0)).value(QStringLiteral("display")).toString(), QStringLiteral("e"));
    source.removeRows(3, 2);
    EXPECT_EQ(pages.currentPage(), 2);
    EXPECT_EQ(pages.numberOfPages(), 2);
    source.removeRows(0, 3);
    EXPECT_EQ(pages.currentPage(), 1);
    EXPECT_EQ(pages.numberOfPages(), 1);
    pages.setEntriesPerPage(0);
    EXPECT_EQ(pages.entriesPerPage(), 1);
}

TEST(JsonHelper, ReadsTypedMembersWithFallbacks)
{
    const QJsonObject object { { "b", true }, { "i", 50 }, { "d", 2.5 }, { "s", "x" }, { "o", QJsonObject { { "k", 1 } } },
        { "a", QJsonArray { 1 } }, { "t", 60 } };
    EXPECT_TRUE(jsonBool(object, "b", false));
    EXPECT_TRUE(jsonBool(object, "missing", true));
    EXPECT_EQ(jsonInt(object, "i", 0, 40, 7), 40);
    EXPECT_EQ(jsonInt(object, "missing", 10, 40, 7), 10);
    EXPECT_DOUBLE_EQ(jsonDouble(object, "d", 0.0, 10.0, 1.0), 2.5);
    EXPECT_DOUBLE_EQ(jsonDouble(object, "s", 0.0, 10.0, 1.0), 1.0);
    EXPECT_EQ(jsonString(object, "s"), QStringLiteral("x"));
    EXPECT_TRUE(jsonString(object, "i").isEmpty());
    EXPECT_EQ(jsonObject(object, "o").value("k").toInt(), 1);
    EXPECT_EQ(jsonArray(object, "a").size(), 1);
    EXPECT_EQ(jsonDateTime(object, "t").toSecsSinceEpoch(), 60);
}

TEST(CrashReportData, RoundTripsAndAnonymizes)
{
    TCrashReportData annotations;
    annotations["Character"] = QStringLiteral("Knight");
    annotations["Version"] = QStringLiteral("1.0");
    annotations.anonymize();
    EXPECT_EQ(annotations["Character"], QStringLiteral("xxx"));
    const TCrashReportData parsed(annotations.toJson());
    EXPECT_EQ(parsed.values(), annotations.values());
    TCrashReportData none;
    none.anonymize();
    EXPECT_TRUE(none.empty());
    EXPECT_EQ(TCrashHandler::annotationsFileName(QStringLiteral("/tmp/a/b.dmp")), QStringLiteral("b.dmp.json"));
}

TEST(CrashHandler, WritesABreakpadMinidumpWithItsSideFiles)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    TCrashHandler& handler = TCrashHandler::instance();
    handler.initialize(QStringLiteral("Tibia"), QStringLiteral("15.11"), directory.path());
    handler.setAnnotation(QStringLiteral("Character"), QStringLiteral("Knight"));
    handler.addLogMessage(QStringLiteral("hello"));
    handler.setSessiondumpProvider([] { return QByteArray("session"); });
    if (!handler.writeMinidump())
        GTEST_SKIP() << "no Breakpad on this host";
    const QStringList dumps = QDir(directory.path()).entryList({ QStringLiteral("*.dmp") }, QDir::Files);
    ASSERT_EQ(dumps.size(), 1);
    QFile dump(QDir(directory.path()).filePath(dumps.first()));
    ASSERT_TRUE(dump.open(QIODevice::ReadOnly));
    EXPECT_EQ(dump.read(4), QByteArray("MDMP"));
    QFile annotations(QDir(directory.path()).filePath(dumps.first() + QStringLiteral(".json")));
    ASSERT_TRUE(annotations.open(QIODevice::ReadOnly));
    const TCrashReportData parsed(annotations.readAll());
    EXPECT_EQ(parsed.at(QStringLiteral("ProductName")), QStringLiteral("Tibia"));
    EXPECT_EQ(parsed.at(QStringLiteral("Version")), QStringLiteral("15.11"));
    EXPECT_EQ(parsed.at(QStringLiteral("Character")), QStringLiteral("Knight"));
    EXPECT_TRUE(parsed.count(QStringLiteral("Timestamp")));
    EXPECT_TRUE(parsed.at(QStringLiteral("LogMessages")).contains(QStringLiteral("hello")));
    QFile session(QDir(directory.path()).filePath(dumps.first() + QStringLiteral(".session")));
    ASSERT_TRUE(session.open(QIODevice::ReadOnly));
    EXPECT_EQ(session.readAll(), QByteArray("session"));
}

TEST(CrashReportSender, PostsCipSoftsMultipartForm)
{
    QTcpServer server;
    ASSERT_TRUE(server.listen(QHostAddress::LocalHost));
    QByteArray request;
    QObject::connect(&server, &QTcpServer::newConnection, [&] {
        QTcpSocket* socket = server.nextPendingConnection();
        QObject::connect(socket, &QTcpSocket::readyRead, [&request, socket] {
            request += socket->readAll();
            const qsizetype headerEnd = request.indexOf("\r\n\r\n");
            if (headerEnd < 0)
                return;
            const QByteArray headers = request.left(headerEnd).toLower();
            const qsizetype at = headers.indexOf("content-length:");
            const qsizetype length = headers.mid(at + 15, headers.indexOf("\r\n", at) - at - 15).trimmed().toLongLong();
            if (request.size() >= headerEnd + 4 + length)
                socket->write("HTTP/1.1 201 Created\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
        });
    });
    auto annotations = std::make_shared<TCrashReportData>();
    (*annotations)["Comment"] = QStringLiteral("it broke");
    (*annotations)["ReportType"] = QStringLiteral("Full");
    TCrashReportSender sender(QStringLiteral("/dumps/abc.dmp"), QStringLiteral("/dumps/abc.dmp.session"), annotations);
    const QUrl url(QStringLiteral("http://127.0.0.1:%1/clientservices/crashreceiver.php").arg(server.serverPort()));
    sender.send(QByteArray("MDMP-dump"), QByteArray("session-dump"), url, true);
    ASSERT_TRUE(waitFor([&] { return request.contains("--\r\n"); }, 5000));
    EXPECT_TRUE(request.startsWith("POST /clientservices/crashreceiver.php"));
    EXPECT_TRUE(request.toLower().contains(QByteArray("content-disposition: form-data; name=\"minidump\"; filename=\"abc.dmp\"").toLower()));
    EXPECT_TRUE(request.contains("MDMP-dump"));
    EXPECT_TRUE(request.toLower().contains(QByteArray("content-disposition: form-data; name=\"sessiondump\"; filename=\"abc.dmp.session\"").toLower()));
    EXPECT_TRUE(request.toLower().contains(QByteArray("content-disposition: form-data; name=\"Comment\"").toLower()));
    EXPECT_TRUE(request.contains("it broke"));
    EXPECT_TRUE(request.toLower().contains(QByteArray("content-disposition: form-data; name=\"ReportType\"").toLower()));
    EXPECT_TRUE(request.toLower().contains("content-type: multipart/form-data"));
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
