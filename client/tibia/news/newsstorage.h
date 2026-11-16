#pragma once

#include <cip_gen.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector>

#include <optional>

namespace tibia {
namespace news {

/// One CIP `gamenews[]` row (compendium / clientservices type=news).
struct NewsItem {
    int id = 0;
    int index = 0;
    qint64 publishDate = 0;
    QString headline;
    QString message;
    QString category; ///< e.g. "GAME CONTENTS", "CLIENT FEATURES"
    QString type;     ///< REGULAR | RETURNER | GROUP HEADER
    int campaignId = 0;
    int groupHeaderId = 0;
    int unlockLevel = 0;
    bool isRead = true;
    qint64 dateFrom = 0;
    qint64 dateTo = 0;
    int specialEvent = 0;

    /// Reached by the character: group headers and entries without an unlock
    /// level from the start, others once the character reaches the level.
    bool unlocked = true;

    bool isGroupHeader() const
    {
        return type.compare(QLatin1String("GROUP HEADER"), Qt::CaseInsensitive) == 0;
    }

    /// CIP's category ids: 0 client features, 1 useful info, 2 player guide
    /// (game contents count as player guide), 3 major updates, 4 support.
    enum ECategory
    {
        ClientFeatures = 0,
        UsefulInfo = 1,
        PlayerGuide = 2,
        MajorUpdates = 3,
        Support = 4,
    };
    /// The category's id; an unknown category counts as useful info.
    ECategory categoryId() const;
    /// CIP's type ids: 0 regular, 3 returner, -1 group header.
    int typeId() const;
};

/// CIP TNewsStorage — holds portal `{gamenews:[…], categorycounts, …}` for NewsDialog.
class TNewsStorage final : public cip::tibia::news::TNewsStorage
{
    Q_OBJECT
public:
    explicit TNewsStorage(QObject* parent = nullptr);
    ~TNewsStorage() override;

    const QVector<NewsItem>& items() const { return m_items; }
    const QJsonObject& categoryCounts() const { return m_categoryCounts; }
    qint64 maxEditDate() const { return m_maxEditDate; }
    bool isReturner() const { return m_isReturner; }
    bool showRewardNews() const { return m_showRewardNews; }
    /// The character's level the entries unlock against, 0 before a character
    /// is known.
    uint16_t characterLevel() const { return m_characterLevel; }
    void setCharacterLevel(uint16_t level);

    /// The storage of the running client, null before one exists.
    static TNewsStorage* current();

    /// CIP: whether the storage holds any entry.
    bool hasEntries() const { return !m_items.isEmpty(); }
    /// CIP: the entry with `id`, if there is one.
    std::optional<NewsItem> entry(uint id) const;
    /// CIP: the entries of `category`, by id.
    QVector<NewsItem> entriesOfCategory(int category) const;
    /// CIP: among the unlocked entries, the category of the newest unread
    /// entry, else of the newest entry; useful info when there is none.
    int categoryOfNewestUnreadOrNewestEntry() const;
    /// CIP: whether an unlocked entry is unread.
    bool hasUnreadUnlockedEntry() const;
    /// CIP: marks the entry read or unread (group headers keep their state).
    void setEntryRead(uint id, bool read);

    /// Prefer CIP `gamenews`; also accepts legacy flat `newsitems`.
    void setFromNewsDocument(const QJsonObject& reply);
    void setItemsFromJson(const QJsonArray& newsitems);

Q_SIGNALS:
    void itemsChanged();

private:
    static NewsItem parseEntry(const QJsonObject& o, int fallbackIndex);
    void replaceItems(QVector<NewsItem> items);

    QVector<NewsItem> m_items;
    QJsonObject m_categoryCounts;
    qint64 m_maxEditDate = 0;
    bool m_isReturner = false;
    bool m_showRewardNews = false;
    uint16_t m_characterLevel = 0;
};

}
}
