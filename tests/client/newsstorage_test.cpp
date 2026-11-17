#include "news/newsstorage.h"

#include "iclockservice.h"

#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonObject>

#include <memory>

using namespace tibia::news;

namespace {

class TFixedClock final : public tibia::shared::IClockService
{
public:
    explicit TFixedClock(QDateTime now)
        : m_now(std::move(now))
    {}
    qint64 nowMillis() override { return m_now.toMSecsSinceEpoch(); }
    qint64 nowNanos() override { return nowMillis() * 1000000; }
    qint64 nowMicros() override { return nowMillis() * 1000; }
    QDateTime currentDateTime() override { return m_now; }

private:
    QDateTime m_now;
};

const QDateTime Now = QDateTime(QDate(2026, 11, 17), QTime(12, 0), QTimeZone::utc());

QJsonObject entryJson(const int id, const char* category, const char* type, const QDateTime& published,
                      const int unlockLevel = 0, const int groupHeader = 0, const bool read = false)
{
    QJsonObject object;
    object[QStringLiteral("id")] = id;
    object[QStringLiteral("category")] = QString::fromLatin1(category);
    object[QStringLiteral("type")] = QString::fromLatin1(type);
    object[QStringLiteral("headline")] = QStringLiteral("headline %1").arg(id);
    object[QStringLiteral("message")] = QStringLiteral("message %1").arg(id);
    object[QStringLiteral("publishdate")] = static_cast<int>(published.toSecsSinceEpoch());
    object[QStringLiteral("unlocklevel")] = unlockLevel;
    object[QStringLiteral("groupheaderid")] = groupHeader;
    object[QStringLiteral("isread")] = read;
    return object;
}

QJsonDocument newsDocument(const int maxEditDate, const QJsonArray& news, const QJsonArray& toDelete = {})
{
    QJsonObject object;
    object[QStringLiteral("maxeditdate")] = maxEditDate;
    object[QStringLiteral("gamenews")] = news;
    object[QStringLiteral("gamenewstodelete")] = toDelete;
    return QJsonDocument(object);
}

std::shared_ptr<TFixedClock> fixedClock() { return std::make_shared<TFixedClock>(Now); }

}

TEST(NewsStorage, EntryParseNeedsAnIdAndUnlocksWithoutLevel)
{
    EXPECT_FALSE(parseNewsEntry(QJsonObject{}));
    const std::optional<TNewsEntry> header =
        parseNewsEntry(entryJson(1, "PLAYER GUIDE", "GROUP HEADER", Now, 20));
    ASSERT_TRUE(header);
    EXPECT_EQ(header->type(), NEWS_TYPE_GROUP_HEADER);
    EXPECT_EQ(header->unlockLevel(), 0);
    EXPECT_TRUE(header->isUnlocked());
    const std::optional<TNewsEntry> leveled = parseNewsEntry(entryJson(2, "GAME CONTENTS", "REGULAR", Now, 8, 1));
    ASSERT_TRUE(leveled);
    EXPECT_EQ(leveled->category(), NEWS_CATEGORY_PLAYER_GUIDE);
    EXPECT_FALSE(leveled->isUnlocked());
    EXPECT_EQ(leveled->groupHeaderId(), std::optional<uint>(1));
}

TEST(NewsStorage, MergeKeepsReadStateUnlessRepublishedAndAutoReadsOldNews)
{
    TNewsStorage storage(fixedClock());
    int changes = 0;
    QObject::connect(&storage, &TNewsStorage::contentsChanged, [&changes] { ++changes; });
    int saves = 0;
    QObject::connect(&storage, &TNewsStorage::startSavingNewsDataToDisk, [&saves](const TNewsData&) { ++saves; });

    storage.mergeNewsData(parseNewsData(newsDocument(
        5, { entryJson(1, "USEFUL INFO", "REGULAR", Now.addDays(-1)), entryJson(2, "USEFUL INFO", "REGULAR", Now.addDays(-40)),
             entryJson(3, "PLAYER GUIDE", "REGULAR", Now.addDays(-40)) })));
    EXPECT_EQ(storage.version(), 5);
    EXPECT_EQ(changes, 1);
    EXPECT_EQ(saves, 1);
    EXPECT_FALSE(storage.entry(1)->isRead());
    EXPECT_TRUE(storage.entry(2)->isRead());   // older than 30 days
    EXPECT_FALSE(storage.entry(3)->isRead());  // player guide stays unread

    storage.setEntryRead(1, true);
    EXPECT_EQ(changes, 2);

    // Same entry again: stays read, nothing changed but it is saved.
    storage.mergeNewsData(parseNewsData(newsDocument(5, { entryJson(1, "USEFUL INFO", "REGULAR", Now.addDays(-1)) })));
    EXPECT_TRUE(storage.entry(1)->isRead());
    EXPECT_EQ(changes, 2);
    EXPECT_EQ(saves, 2);

    // Republished: unread again; deleted ids go.
    storage.mergeNewsData(parseNewsData(
        newsDocument(6, { entryJson(1, "USEFUL INFO", "REGULAR", Now.addSecs(-60)) }, QJsonArray{ 3 })));
    EXPECT_FALSE(storage.entry(1)->isRead());
    EXPECT_FALSE(storage.entry(3));
    EXPECT_EQ(changes, 3);
}

TEST(NewsStorage, LevelsUnlockEntriesAndGroupHeadersFollowTheirEntries)
{
    TNewsStorage storage(fixedClock());
    storage.mergeNewsData(parseNewsData(newsDocument(
        1, { entryJson(10, "PLAYER GUIDE", "GROUP HEADER", Now), entryJson(11, "PLAYER GUIDE", "REGULAR", Now, 8, 10) })));
    EXPECT_FALSE(storage.entry(11)->isUnlocked());
    // No unlocked unread entry: the header counts as read.
    EXPECT_TRUE(storage.entry(10)->isRead());
    EXPECT_FALSE(storage.hasUnreadUnlockedEntry());

    storage.raiseCharacterLevel(8);
    EXPECT_TRUE(storage.entry(11)->isUnlocked());
    EXPECT_FALSE(storage.entry(10)->isRead());
    EXPECT_TRUE(storage.hasUnreadUnlockedEntry());
    EXPECT_EQ(storage.categoryOfNewestUnreadOrNewestEntry(), NEWS_CATEGORY_PLAYER_GUIDE);

    storage.setEntryRead(11, true);
    EXPECT_TRUE(storage.entry(10)->isRead());
    // Headers keep their state against direct changes.
    storage.setEntryRead(10, false);
    EXPECT_TRUE(storage.entry(10)->isRead());

    storage.resetLevels();
    EXPECT_FALSE(storage.entry(11)->isUnlocked());
}

TEST(NewsStorage, ReturnerEntriesPreferACampaign)
{
    TNewsStorage storage(fixedClock());
    QJsonObject withCampaign = entryJson(3, "USEFUL INFO", "RETURNER", Now);
    withCampaign[QStringLiteral("campaignid")] = 7;
    storage.mergeNewsData(parseNewsData(newsDocument(
        1, { entryJson(1, "USEFUL INFO", "RETURNER", Now), entryJson(2, "USEFUL INFO", "RETURNER", Now), withCampaign })));
    ASSERT_TRUE(storage.firstEntryOfType(NEWS_TYPE_RETURNER));
    EXPECT_EQ(storage.firstEntryOfType(NEWS_TYPE_RETURNER)->id(), 3u);
    EXPECT_FALSE(storage.firstEntryOfType(NEWS_TYPE_GROUP_HEADER));
}

TEST(NewsStorage, ReturnerInformationReportsLaterChangesOnly)
{
    TNewsStorage storage(fixedClock());
    int changes = 0;
    QObject::connect(&storage, &TNewsStorage::returnerInformationChanged, [&changes] { ++changes; });
    storage.setReturnerInformation(true, false);
    EXPECT_EQ(changes, 0);
    storage.setReturnerInformation(true, false);
    EXPECT_EQ(changes, 0);
    storage.setReturnerInformation(true, true);
    EXPECT_EQ(changes, 1);
    EXPECT_TRUE(storage.showRewardNews());
}

TEST(NewsStorage, DiskDocumentRoundTripsAndExpiresWithTheServerSave)
{
    TNewsData data = parseNewsData(newsDocument(4, { entryJson(1, "SUPPORT", "REGULAR", Now, 0, 0, true) }));
    data.lastUpdateTimestamp = Now.toSecsSinceEpoch();
    const QJsonDocument document = newsDataToJson(data);
    const TNewsData back = parseNewsData(document);
    ASSERT_EQ(back.gameNews.size(), 1u);
    EXPECT_EQ(back.gameNews.front(), data.gameNews.front());
    EXPECT_EQ(back.maxEditDate, 4);
    EXPECT_FALSE(isCachedDocumentOutdated(document, Now));
    EXPECT_TRUE(isCachedDocumentOutdated(document, Now.addDays(1)));
    EXPECT_TRUE(isCachedDocumentOutdated(QJsonDocument(QJsonObject{}), Now));

    TNewsStorage storage(fixedClock());
    storage.loadFromDisk(back);
    EXPECT_TRUE(storage.entry(1)->isRead());
    // A non-empty storage ignores the disk.
    storage.loadFromDisk(parseNewsData(newsDocument(9, {})));
    EXPECT_EQ(storage.version(), 4);
}
