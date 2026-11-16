#include "newsstorage.h"

#include <QJsonValue>

#include <algorithm>

namespace tibia::news {

namespace {

QString normalizeCategory(QString category)
{
    category = category.trimmed().toUpper();
    if (category.isEmpty())
        return QStringLiteral("USEFUL INFO");
    if (category == QLatin1String("MAJOR UPDATE") || category == QLatin1String("UPDATE"))
        return QStringLiteral("MAJOR UPDATES");
    if (category == QLatin1String("CLIENT") || category == QLatin1String("CLIENT FEATURE"))
        return QStringLiteral("CLIENT FEATURES");
    if (category == QLatin1String("GAME") || category == QLatin1String("GAME CONTENT"))
        return QStringLiteral("GAME CONTENTS");
    if (category == QLatin1String("GUIDE") || category == QLatin1String("PLAYERGUIDE"))
        return QStringLiteral("PLAYER GUIDE");
    if (category == QLatin1String("USEFUL") || category == QLatin1String("INFO"))
        return QStringLiteral("USEFUL INFO");
    return category;
}

QString normalizeType(QString type)
{
    type = type.trimmed().toUpper();
    if (type == QLatin1String("RETURNER"))
        return QStringLiteral("RETURNER");
    if (type == QLatin1String("GROUP HEADER") || type == QLatin1String("GROUPHEADER"))
        return QStringLiteral("GROUP HEADER");
    return QStringLiteral("REGULAR");
}

}

namespace {

TNewsStorage* g_currentNewsStorage = nullptr;

}

TNewsStorage::TNewsStorage(QObject* parent)
    : cip::tibia::news::TNewsStorage(parent)
{
    g_currentNewsStorage = this;
}

TNewsStorage::~TNewsStorage()
{
    if (g_currentNewsStorage == this)
        g_currentNewsStorage = nullptr;
}

TNewsStorage* TNewsStorage::current() { return g_currentNewsStorage; }

void TNewsStorage::setCharacterLevel(const uint16_t level)
{
    if (m_characterLevel == level)
        return;
    m_characterLevel = level;
    // Entries unlock once the character reaches their level.
    for (NewsItem& item : m_items) {
        if (!item.unlocked && item.unlockLevel <= level)
            item.unlocked = true;
    }
    Q_EMIT contentsChanged();
}

NewsItem::ECategory NewsItem::categoryId() const
{
    if (category == QLatin1String("MAJOR UPDATES"))
        return MajorUpdates;
    if (category == QLatin1String("GAME CONTENTS") || category == QLatin1String("PLAYER GUIDE"))
        return PlayerGuide;
    if (category == QLatin1String("CLIENT FEATURES"))
        return ClientFeatures;
    if (category == QLatin1String("SUPPORT"))
        return Support;
    return UsefulInfo;
}

int NewsItem::typeId() const
{
    if (type == QLatin1String("RETURNER"))
        return 3;
    if (type == QLatin1String("GROUP HEADER"))
        return -1;
    return 0;
}

std::optional<NewsItem> TNewsStorage::entry(const uint id) const
{
    for (const NewsItem& item : m_items) {
        if (static_cast<uint>(item.id) == id)
            return item;
    }
    return std::nullopt;
}

QVector<NewsItem> TNewsStorage::entriesOfCategory(const int category) const
{
    QVector<NewsItem> entries;
    for (const NewsItem& item : m_items) {
        if (item.categoryId() == category)
            entries.push_back(item);
    }
    std::stable_sort(entries.begin(), entries.end(), [](const NewsItem& a, const NewsItem& b) {
        return static_cast<uint>(a.id) < static_cast<uint>(b.id);
    });
    return entries;
}

int TNewsStorage::categoryOfNewestUnreadOrNewestEntry() const
{
    int newestCategory = NewsItem::UsefulInfo;
    int newestUnreadCategory = NewsItem::UsefulInfo;
    bool anyUnread = false;
    qint64 newest = 0;
    qint64 newestUnread = 0;
    for (const NewsItem& item : m_items) {
        if (!item.unlocked)
            continue;
        if (!item.isRead) {
            anyUnread = true;
            if (item.publishDate > newestUnread) {
                newestUnreadCategory = item.categoryId();
                newestUnread = item.publishDate;
            }
        }
        if (item.publishDate > newest) {
            newestCategory = item.categoryId();
            newest = item.publishDate;
        }
    }
    return anyUnread ? newestUnreadCategory : newestCategory;
}

bool TNewsStorage::hasUnreadUnlockedEntry() const
{
    return std::any_of(m_items.begin(), m_items.end(), [](const NewsItem& item) { return !item.isRead && item.unlocked; });
}

void TNewsStorage::setEntryRead(const uint id, const bool read)
{
    for (NewsItem& item : m_items) {
        if (static_cast<uint>(item.id) != id)
            continue;
        if (item.typeId() == -1 || item.isRead == read)
            return;
        item.isRead = read;
        Q_EMIT contentsChanged();
        return;
    }
}

NewsItem TNewsStorage::parseEntry(const QJsonObject& o, const int fallbackIndex)
{
    NewsItem item;
    const QString headline = o.value(QStringLiteral("headline")).toString();
    const QString title = o.value(QStringLiteral("title")).toString();
    item.headline = !headline.isEmpty() ? headline : title;
    item.message = o.value(QStringLiteral("message")).toString();
    if (item.message.isEmpty())
        item.message = o.value(QStringLiteral("body")).toString();
    if (item.message.isEmpty())
        item.message = o.value(QStringLiteral("content")).toString();
    if (item.message.isEmpty())
        item.message = item.headline;
    item.id = o.value(QStringLiteral("id")).toInt(fallbackIndex + 1);
    item.index = o.value(QStringLiteral("index")).toInt(fallbackIndex + 1);
    item.publishDate = o.value(QStringLiteral("publishdate")).toInteger();
    if (item.publishDate == 0)
        item.publishDate = o.value(QStringLiteral("date")).toInteger();
    item.category = normalizeCategory(o.value(QStringLiteral("category")).toString());
    item.type = normalizeType(o.value(QStringLiteral("type")).toString());
    item.campaignId = o.value(QStringLiteral("campaignid")).toInt();
    item.groupHeaderId = o.value(QStringLiteral("groupheaderid")).toInt();
    item.unlockLevel = o.value(QStringLiteral("unlocklevel")).toInt();
    item.isRead = o.contains(QStringLiteral("isread")) ? o.value(QStringLiteral("isread")).toBool() : true;
    item.dateFrom = o.value(QStringLiteral("datefrom")).toInteger();
    item.dateTo = o.value(QStringLiteral("dateto")).toInteger();
    item.specialEvent = o.value(QStringLiteral("specialevent")).toInt();
    // Group headers and entries without an unlock level start unlocked.
    item.unlocked = item.isGroupHeader() || item.unlockLevel == 0;
    return item;
}

void TNewsStorage::replaceItems(QVector<NewsItem> items)
{
    m_items = std::move(items);
    Q_EMIT contentsChanged();
    Q_EMIT itemsChanged();
    Q_EMIT returnerInformationChanged();
}

void TNewsStorage::setFromNewsDocument(const QJsonObject& reply)
{
    m_categoryCounts = reply.value(QStringLiteral("categorycounts")).toObject();
    m_maxEditDate = reply.value(QStringLiteral("maxeditdate")).toInteger();
    m_isReturner = reply.value(QStringLiteral("isreturner")).toBool();
    m_showRewardNews = reply.value(QStringLiteral("showrewardnews")).toBool();

    QJsonArray rows = reply.value(QStringLiteral("gamenews")).toArray();
    if (rows.isEmpty())
        rows = reply.value(QStringLiteral("newsitems")).toArray();

    QVector<NewsItem> parsed;
    parsed.reserve(rows.size());
    int index = 0;
    for (const QJsonValue& value : rows) {
        NewsItem item = parseEntry(value.toObject(), index);
        if (item.unlockLevel <= m_characterLevel)
            item.unlocked = true;
        if (!item.headline.isEmpty())
            parsed.push_back(std::move(item));
        ++index;
    }
    if (m_maxEditDate == 0) {
        for (const NewsItem& item : parsed)
            m_maxEditDate = std::max(m_maxEditDate, item.publishDate);
    }
    replaceItems(std::move(parsed));
}

void TNewsStorage::setItemsFromJson(const QJsonArray& newsitems)
{
    QJsonObject wrap;
    wrap.insert(QStringLiteral("newsitems"), newsitems);
    setFromNewsDocument(wrap);
}

}
