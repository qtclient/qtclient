#include "newsstorage.h"

#include "iclockservice.h"
#include "jsonhelper.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonValue>
#include <QTimeZone>

#include <limits>

namespace tibia::news {

namespace {

constexpr int IntMax = std::numeric_limits<int>::max();

/// CIP: entries published more than this many days ago count as read.
constexpr int AutoReadAfterDays = 30;

/// CIP: the server save; cached documents expire with it.
const QTime ServerSaveTime(10, 0);

TNewsStorage* g_currentNewsStorage = nullptr;

}

QTimeZone serverTimeZone()
{
    static const QTimeZone zone = [] {
#if QT_CONFIG(timezone)
        QTimeZone berlin(QByteArrayLiteral("Europe/Berlin"));
        // qtclient: a build may lack the zone database.
        if (berlin.isValid())
            return berlin;
#endif
        // qtclient: wasm Qt has no named zones; Berlin's standard time.
        return QTimeZone::fromSecondsAheadOfUtc(3600);
    }();
    return zone;
}

int newsCategoryFromName(const QString& name, const int fallback)
{
    if (name.compare(QLatin1String("MAJOR UPDATES"), Qt::CaseSensitive) == 0)
        return NEWS_CATEGORY_MAJOR_UPDATES;
    if (name.compare(QLatin1String("GAME CONTENTS"), Qt::CaseSensitive) == 0
        || name.compare(QLatin1String("PLAYER GUIDE"), Qt::CaseSensitive) == 0)
        return NEWS_CATEGORY_PLAYER_GUIDE;
    if (name.compare(QLatin1String("CLIENT FEATURES"), Qt::CaseSensitive) == 0)
        return NEWS_CATEGORY_CLIENT_FEATURES;
    if (name.compare(QLatin1String("USEFUL INFO"), Qt::CaseSensitive) == 0)
        return NEWS_CATEGORY_USEFUL_INFO;
    if (name.compare(QLatin1String("SUPPORT"), Qt::CaseSensitive) == 0)
        return NEWS_CATEGORY_SUPPORT;
    return fallback;
}

int newsTypeFromName(const QString& name, const int fallback)
{
    if (name.compare(QLatin1String("REGULAR"), Qt::CaseSensitive) == 0)
        return NEWS_TYPE_REGULAR;
    if (name.compare(QLatin1String("RETURNER"), Qt::CaseSensitive) == 0)
        return NEWS_TYPE_RETURNER;
    return name.compare(QLatin1String("GROUP HEADER"), Qt::CaseSensitive) == 0 ? NEWS_TYPE_GROUP_HEADER : fallback;
}

QString newsCategoryName(const int category)
{
    switch (category) {
        case NEWS_CATEGORY_CLIENT_FEATURES:
            return QStringLiteral("CLIENT FEATURES");
        case NEWS_CATEGORY_USEFUL_INFO:
            return QStringLiteral("USEFUL INFO");
        case NEWS_CATEGORY_PLAYER_GUIDE:
            return QStringLiteral("PLAYER GUIDE");
        case NEWS_CATEGORY_MAJOR_UPDATES:
            return QStringLiteral("MAJOR UPDATES");
        case NEWS_CATEGORY_SUPPORT:
            return QStringLiteral("SUPPORT");
        default:
            // CIP throws; on WASM that escapes application.exec and freezes /play.
            return QStringLiteral("USEFUL INFO");
    }
}

QString newsTypeName(const int type)
{
    switch (type) {
        case NEWS_TYPE_GROUP_HEADER:
            return QStringLiteral("GROUP HEADER");
        case NEWS_TYPE_RETURNER:
            return QStringLiteral("RETURNER");
        case NEWS_TYPE_REGULAR:
            return QStringLiteral("REGULAR");
        default:
            return QStringLiteral("REGULAR");
    }
}

TNewsEntry::TNewsEntry(const uint id, const int category, const int type, QString headline, QString message,
                       QDateTime publishDate, const ushort unlockLevel, const int campaignId, const bool read,
                       const std::optional<uint> groupHeaderId)
    : m_id(id)
    , m_category(category)
    , m_type(type)
    , m_headline(std::move(headline))
    , m_message(std::move(message))
    , m_publishDate(std::move(publishDate))
    , m_unlockLevel(unlockLevel)
    , m_campaignId(campaignId)
    , m_read(read)
    , m_groupHeaderId(groupHeaderId)
{
    if (m_type == NEWS_TYPE_GROUP_HEADER)
        m_unlockLevel = 0;
    else if (unlockLevel != 0)
        return;
    m_unlocked = true;
}

bool TNewsEntry::setRead(const bool read)
{
    const bool changed = m_read != read;
    if (changed)
        m_read = read;
    return changed;
}

bool TNewsEntry::setUnlocked(const bool unlocked)
{
    const bool changed = m_unlocked != unlocked;
    if (changed)
        m_unlocked = unlocked;
    return changed;
}

bool TNewsEntry::operator==(const TNewsEntry& other) const
{
    return m_id == other.m_id && m_category == other.m_category && m_type == other.m_type
        && m_headline == other.m_headline && m_message == other.m_message && m_publishDate == other.m_publishDate
        && m_unlockLevel == other.m_unlockLevel && m_unlocked == other.m_unlocked && m_campaignId == other.m_campaignId
        && m_read == other.m_read && m_groupHeaderId == other.m_groupHeaderId;
}

std::optional<TNewsEntry> parseNewsEntry(const QJsonObject& object)
{
    if (!object.contains(QStringLiteral("id")))
        return std::nullopt;
    const int groupHeaderId = shared::jsonInt(object, QStringLiteral("groupheaderid"), 0, IntMax, 0);
    const int id = shared::jsonInt(object, QStringLiteral("id"), 0, IntMax, 0);
    const int category = newsCategoryFromName(object[QStringLiteral("category")].toString(), NEWS_CATEGORY_USEFUL_INFO);
    const int type = newsTypeFromName(object[QStringLiteral("type")].toString(), NEWS_TYPE_REGULAR);
    QString headline = shared::jsonString(object, QStringLiteral("headline"));
    QString message = shared::jsonString(object, QStringLiteral("message"));
    QDateTime publishDate =
        QDateTime::fromSecsSinceEpoch(object[QStringLiteral("publishdate")].toInt(), QTimeZone::UTC);
    const int unlockLevel = shared::jsonInt(object, QStringLiteral("unlocklevel"), 0, IntMax, 0);
    const int campaignId = object[QStringLiteral("campaignid")].toInt();
    const bool read = shared::jsonBool(object, QStringLiteral("isread"), false);
    return TNewsEntry(static_cast<uint>(id), category, type, std::move(headline), std::move(message), std::move(publishDate),
                      static_cast<ushort>(unlockLevel), campaignId, read,
                      groupHeaderId != 0 ? std::optional<uint>(static_cast<uint>(groupHeaderId)) : std::nullopt);
}

QJsonObject newsEntryToJson(const TNewsEntry& entry)
{
    QJsonObject object;
    object[QStringLiteral("id")] = static_cast<int>(entry.id());
    object[QStringLiteral("category")] = newsCategoryName(entry.category());
    object[QStringLiteral("type")] = newsTypeName(entry.type());
    object[QStringLiteral("headline")] = entry.headline();
    object[QStringLiteral("message")] = entry.message();
    object[QStringLiteral("publishdate")] = static_cast<int>(entry.publishDate().toSecsSinceEpoch());
    object[QStringLiteral("unlocklevel")] = static_cast<int>(entry.unlockLevel());
    object[QStringLiteral("campaignid")] = entry.campaignId();
    object[QStringLiteral("isread")] = entry.isRead();
    object[QStringLiteral("groupheaderid")] = static_cast<int>(entry.groupHeaderId().value_or(0));
    return object;
}

TNewsData parseNewsData(const QJsonDocument& document)
{
    TNewsData data;
    const QJsonObject object = document.object();
    data.maxEditDate = object[QStringLiteral("maxeditdate")].toInt(0);
    data.isReturner = object[QStringLiteral("isreturner")].toBool(false);
    data.showRewardNews = object[QStringLiteral("showrewardnews")].toBool(false);
    data.lastUpdateTimestamp = object[QStringLiteral("lastupdatetimestamp")].toInt(0);
    const QJsonArray news = object[QStringLiteral("gamenews")].toArray();
    for (const QJsonValue& value : news) {
        if (std::optional<TNewsEntry> entry = parseNewsEntry(value.toObject()))
            data.gameNews.push_back(std::move(*entry));
    }
    const QJsonArray toDelete = object[QStringLiteral("gamenewstodelete")].toArray();
    for (const QJsonValue& value : toDelete)
        data.gameNewsToDelete.insert(static_cast<uint>(value.toInt(0)));
    return data;
}

QJsonDocument newsDataToJson(const TNewsData& data)
{
    QJsonArray news;
    for (const TNewsEntry& entry : data.gameNews)
        news.append(newsEntryToJson(entry));
    QJsonObject object;
    object[QStringLiteral("maxeditdate")] = data.maxEditDate;
    object[QStringLiteral("isreturner")] = data.isReturner.value_or(false);
    object[QStringLiteral("showrewardnews")] = data.showRewardNews.value_or(false);
    object[QStringLiteral("lastupdatetimestamp")] = data.lastUpdateTimestamp;
    object[QStringLiteral("gamenews")] = news;
    return QJsonDocument(object);
}

bool isCachedDocumentOutdated(const QJsonDocument& document, const QDateTime& now)
{
    const QJsonObject object = document.object();
    const QJsonValue timestamp = object.value(QStringLiteral("lastupdatetimestamp"));
    if (!timestamp.isDouble())
        return true;
    const int lastUpdate = object[QStringLiteral("lastupdatetimestamp")].toInt(0);
    QDateTime serverSave = now.toTimeZone(serverTimeZone());
    if (serverSave.time() < ServerSaveTime)
        serverSave = serverSave.addDays(-1);
    serverSave.setTime(ServerSaveTime);
    return lastUpdate < serverSave.toSecsSinceEpoch();
}

TNewsStorage::TNewsStorage(std::shared_ptr<shared::IClockService> clock, QObject* parent)
    : QObject(parent)
    , m_clock(std::move(clock))
{
    g_currentNewsStorage = this;
}

TNewsStorage::~TNewsStorage()
{
    if (g_currentNewsStorage == this)
        g_currentNewsStorage = nullptr;
}

TNewsStorage* TNewsStorage::current() { return g_currentNewsStorage; }

void TNewsStorage::setReturnerInformation(const bool returner, const bool showRewardNews)
{
    const bool known = m_returnerInformation;
    const bool changed = m_isReturner != returner || m_showRewardNews != showRewardNews;
    m_isReturner = returner;
    m_showRewardNews = showRewardNews;
    m_returnerInformation = true;
    if (known && changed)
        Q_EMIT returnerInformationChanged();
}

std::optional<TNewsEntry> TNewsStorage::entry(const uint id) const
{
    const auto it = m_entries.find(id);
    if (it == m_entries.end())
        return std::nullopt;
    return it->second;
}

std::optional<TNewsEntry> TNewsStorage::firstEntryOfType(const int type) const
{
    std::optional<TNewsEntry> withoutCampaign;
    for (const auto& [id, entry] : m_entries) {
        if (entry.type() != type)
            continue;
        if (type == NEWS_TYPE_RETURNER && !entry.hasCampaign()) {
            withoutCampaign = entry;
            continue;
        }
        return entry;
    }
    if (type == NEWS_TYPE_RETURNER)
        return withoutCampaign;
    return std::nullopt;
}

std::vector<TNewsEntry> TNewsStorage::entriesOfCategory(const int category) const
{
    std::vector<TNewsEntry> entries;
    for (const auto& [id, entry] : m_entries) {
        if (entry.category() == category)
            entries.push_back(entry);
    }
    return entries;
}

int TNewsStorage::categoryOfNewestUnreadOrNewestEntry() const
{
    QDateTime newest = QDateTime::fromSecsSinceEpoch(0);
    QDateTime newestUnread = QDateTime::fromSecsSinceEpoch(0);
    if (m_entries.empty())
        return NEWS_CATEGORY_PLAYER_GUIDE;
    int newestCategory = NEWS_CATEGORY_PLAYER_GUIDE;
    int newestUnreadCategory = NEWS_CATEGORY_PLAYER_GUIDE;
    bool anyUnread = false;
    for (const auto& [id, entry] : m_entries) {
        if (!entry.isUnlocked())
            continue;
        if (!entry.isRead()) {
            anyUnread = true;
            if (entry.publishDate() > newestUnread) {
                newestUnreadCategory = entry.category();
                newestUnread = entry.publishDate();
            }
        }
        if (entry.publishDate() > newest) {
            newestCategory = entry.category();
            newest = entry.publishDate();
        }
    }
    return anyUnread ? newestUnreadCategory : newestCategory;
}

bool TNewsStorage::hasUnreadUnlockedEntry() const
{
    for (const auto& [id, entry] : m_entries) {
        if (!entry.isRead() && entry.isUnlocked())
            return true;
    }
    return false;
}

void TNewsStorage::setEntryRead(const uint id, const bool read)
{
    const auto it = m_entries.find(id);
    if (it == m_entries.end())
        return;
    TNewsEntry& entry = it->second;
    if (entry.type() == NEWS_TYPE_GROUP_HEADER)
        return;
    if (!entry.setRead(read))
        return;
    if (entry.groupHeaderId())
        updateGroupHeaderReadStates();
    Q_EMIT contentsChanged();
}

bool TNewsStorage::updateUnlockedStates()
{
    bool changed = false;
    for (auto& [id, entry] : m_entries) {
        bool unlocked = true;
        if (m_accountLevel < entry.unlockLevel())
            unlocked = entry.unlockLevel() <= m_characterLevel;
        changed |= entry.setUnlocked(unlocked);
    }
    if (changed)
        updateGroupHeaderReadStates();
    return changed;
}

void TNewsStorage::raiseAccountLevel(const ushort level)
{
    if (level <= m_accountLevel)
        return;
    m_accountLevel = level;
    if (updateUnlockedStates())
        Q_EMIT contentsChanged();
}

void TNewsStorage::raiseCharacterLevel(const ushort level)
{
    if (level <= m_characterLevel)
        return;
    m_characterLevel = level;
    updateUnlockedStates();
    Q_EMIT contentsChanged();
}

void TNewsStorage::resetLevels()
{
    if (m_accountLevel == 0 && m_characterLevel == 0)
        return;
    m_accountLevel = 0;
    m_characterLevel = 0;
    updateUnlockedStates();
    Q_EMIT contentsChanged();
}

void TNewsStorage::updateGroupHeaderReadStates()
{
    for (auto& [headerId, header] : m_entries) {
        if (header.type() != NEWS_TYPE_GROUP_HEADER)
            continue;
        bool unreadEntry = false;
        for (const auto& [id, entry] : m_entries) {
            if (!entry.isRead() && entry.isUnlocked() && entry.groupHeaderId().value_or(0) == header.id()) {
                unreadEntry = true;
                break;
            }
        }
        header.setRead(!unreadEntry);
    }
}

bool TNewsStorage::markReadIfOld(TNewsEntry& entry, shared::IClockService& clock)
{
    const QDateTime threshold = clock.currentDateTime().addDays(-AutoReadAfterDays);
    if (entry.category() == NEWS_CATEGORY_PLAYER_GUIDE || entry.type() == NEWS_TYPE_RETURNER)
        return false;
    if (entry.publishDate() >= threshold || entry.isRead())
        return false;
    entry.setRead(true);
    return true;
}

void TNewsStorage::insertOrAssign(const TNewsEntry& entry) { m_entries.insert_or_assign(entry.id(), entry); }

void TNewsStorage::mergeNewsData(const TNewsData& data)
{
    bool changed = m_version != data.maxEditDate;
    m_version = data.maxEditDate;
    m_lastUpdateTimestamp = data.lastUpdateTimestamp;
    for (TNewsEntry entry : data.gameNews) {
        bool entryChanged = true;
        if (const auto old = m_entries.find(entry.id()); old != m_entries.end()) {
            // A republished entry is unread again.
            entry.setRead(old->second.isRead());
            if (old->second.publishDate() != entry.publishDate())
                entry.setRead(false);
            entryChanged = !(entry == old->second);
        }
        changed |= entryChanged;
        changed |= markReadIfOld(entry, *m_clock);
        insertOrAssign(entry);
    }
    for (const uint id : data.gameNewsToDelete)
        changed |= m_entries.erase(id) != 0;
    if (updateUnlockedStates() || changed) {
        updateGroupHeaderReadStates();
        saveToDisk();
        Q_EMIT contentsChanged();
        return;
    }
    saveToDisk();
}

void TNewsStorage::saveToDisk()
{
    TNewsData data;
    data.maxEditDate = m_version;
    data.lastUpdateTimestamp = m_lastUpdateTimestamp;
    data.isReturner = m_isReturner;
    data.showRewardNews = m_showRewardNews;
    data.gameNews.reserve(m_entries.size());
    for (const auto& [id, entry] : m_entries)
        data.gameNews.push_back(entry);
    Q_EMIT startSavingNewsDataToDisk(data);
}

void TNewsStorage::loadFromDisk(const TNewsData& data)
{
    if (!m_entries.empty()) {
        qDebug() << "Skipped updating non empty NewsStorage form disk";
        return;
    }
    m_version = data.maxEditDate;
    m_lastUpdateTimestamp = data.lastUpdateTimestamp;
    if (data.isReturner || data.showRewardNews) {
        m_isReturner = data.isReturner.value_or(false);
        m_showRewardNews = data.showRewardNews.value_or(false);
        m_returnerInformation = true;
    }
    for (TNewsEntry entry : data.gameNews) {
        markReadIfOld(entry, *m_clock);
        insertOrAssign(entry);
    }
    updateGroupHeaderReadStates();
    updateUnlockedStates();
    Q_EMIT contentsChanged();
}

}
