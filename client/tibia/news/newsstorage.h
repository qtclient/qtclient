#pragma once

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTimeZone>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <vector>

namespace tibia::shared {
class IClockService;
}

namespace tibia {
namespace news {

/// CIP news categories (the dialog's tabs).
enum ENewsCategory : int
{
    NEWS_CATEGORY_CLIENT_FEATURES = 0,
    NEWS_CATEGORY_USEFUL_INFO = 1,
    NEWS_CATEGORY_PLAYER_GUIDE = 2,
    NEWS_CATEGORY_MAJOR_UPDATES = 3,
    NEWS_CATEGORY_SUPPORT = 4,
};

/// CIP news types.
enum ENewsType : int
{
    NEWS_TYPE_GROUP_HEADER = -1,
    NEWS_TYPE_REGULAR = 0,
    NEWS_TYPE_RETURNER = 3,
};

/// The category of a `category` name; game contents count as player guide.
int newsCategoryFromName(const QString& name, int fallback);
/// The type of a `type` name.
int newsTypeFromName(const QString& name, int fallback);
/// The server's time zone (CIP: Europe/Berlin).
QTimeZone serverTimeZone();

/// The `category` name of a category; unknown values log and return USEFUL INFO
/// (CIP throws — that freezes WASM /play when it escapes the event loop).
QString newsCategoryName(int category);
/// The `type` name of a type; unknown values log and return REGULAR.
QString newsTypeName(int type);

/// CIP news::TNewsEntry: one `gamenews` row. A group header never needs a
/// level; an entry without an unlock level starts unlocked.
class TNewsEntry
{
public:
    TNewsEntry(uint id, int category, int type, QString headline, QString message, QDateTime publishDate,
               ushort unlockLevel, int campaignId, bool read, std::optional<uint> groupHeaderId);

    uint id() const { return m_id; }
    int category() const { return m_category; }
    int type() const { return m_type; }
    const QString& headline() const { return m_headline; }
    const QString& message() const { return m_message; }
    QDateTime publishDate() const { return m_publishDate; }
    ushort unlockLevel() const { return m_unlockLevel; }
    bool isUnlocked() const { return m_unlocked; }
    int campaignId() const { return m_campaignId; }
    bool hasCampaign() const { return m_campaignId != 0; }
    bool isRead() const { return m_read; }
    std::optional<uint> groupHeaderId() const { return m_groupHeaderId; }

    /// True when the read state changed.
    bool setRead(bool read);
    /// True when the unlocked state changed.
    bool setUnlocked(bool unlocked);

    bool operator==(const TNewsEntry& other) const;

private:
    uint m_id;
    int m_category;
    int m_type;
    QString m_headline;
    QString m_message;
    QDateTime m_publishDate;
    ushort m_unlockLevel;
    bool m_unlocked = false;
    int m_campaignId;
    bool m_read;
    std::optional<uint> m_groupHeaderId;
};

/// A `gamenews` object; empty when it lacks its key field.
std::optional<TNewsEntry> parseNewsEntry(const QJsonObject& object);
/// The `gamenews` object of an entry, as the disk cache stores it.
QJsonObject newsEntryToJson(const TNewsEntry& entry);

/// CIP news::TNewsData: a news document (server answer or disk cache).
struct TNewsData
{
    int maxEditDate = 0;
    std::vector<TNewsEntry> gameNews;
    std::set<uint> gameNewsToDelete;
    qint64 lastUpdateTimestamp = 0;
    std::optional<bool> isReturner;
    std::optional<bool> showRewardNews;
};

TNewsData parseNewsData(const QJsonDocument& document);
/// The disk cache document (without gamenewstodelete).
QJsonDocument newsDataToJson(const TNewsData& data);

/// Whether a cached document (`lastupdatetimestamp`) predates the last server
/// save before `now`; a document without the timestamp is outdated.
bool isCachedDocumentOutdated(const QJsonDocument& document, const QDateTime& now);

/// CIP news::TNewsStorage: the news entries by id, their read and unlocked
/// states, the account's returner information and the levels entries unlock
/// against (the account's highest character level and the playing character's).
class TNewsStorage final : public QObject
{
    Q_OBJECT
public:
    explicit TNewsStorage(std::shared_ptr<shared::IClockService> clock, QObject* parent = nullptr);
    ~TNewsStorage() override;

    /// qtclient host accessor: the storage of the running client, null before one exists.
    static TNewsStorage* current();

    /// The server's maxeditdate the entries are from.
    int version() const { return m_version; }
    bool isReturner() const { return m_returnerInformation && m_isReturner; }
    bool showRewardNews() const { return m_returnerInformation && m_showRewardNews; }
    /// Remembers the account's returner information; a change after the first
    /// one emits returnerInformationChanged.
    void setReturnerInformation(bool returner, bool showRewardNews);

    bool hasEntries() const { return !m_entries.empty(); }
    const std::map<uint, TNewsEntry>& entries() const { return m_entries; }
    /// The entry with `id`, if there is one.
    std::optional<TNewsEntry> entry(uint id) const;
    /// The first entry of `type` by id; for returner news the first with a
    /// campaign, else the last without one.
    std::optional<TNewsEntry> firstEntryOfType(int type) const;
    /// The entries of `category`, by id.
    std::vector<TNewsEntry> entriesOfCategory(int category) const;
    /// Among the unlocked entries, the category of the newest unread entry,
    /// else of the newest entry; player guide when there is none.
    int categoryOfNewestUnreadOrNewestEntry() const;
    /// Whether an unlocked entry is unread.
    bool hasUnreadUnlockedEntry() const;
    /// Marks an entry read or unread (group headers keep theirs; they follow
    /// their entries).
    void setEntryRead(uint id, bool read);

    /// The account's highest character level only grows until resetLevels.
    void raiseAccountLevel(ushort level);
    /// The playing character's level only grows until resetLevels.
    void raiseCharacterLevel(ushort level);
    ushort characterLevel() const { return m_characterLevel; }
    void resetLevels();

    /// Merges a server answer: entries keep their read state unless their
    /// publish date changed, old entries count as read, deleted ids go; then
    /// saves and reports a change.
    void mergeNewsData(const TNewsData& data);
    /// Hands the storage to the disk cache.
    void saveToDisk();

public Q_SLOTS:
    /// The disk cache's entries, when the storage is still empty.
    void loadFromDisk(const tibia::news::TNewsData& data);

Q_SIGNALS:
    void contentsChanged();
    void returnerInformationChanged();
    void startSavingNewsDataToDisk(const tibia::news::TNewsData& FileContent);

private:
    /// Unlocks the entries against the levels; true when any changed.
    bool updateUnlockedStates();
    /// A group header is read when none of its unlocked entries is unread.
    void updateGroupHeaderReadStates();
    /// Entries older than 30 days count as read (not player guide or returner
    /// news); true when the entry changed.
    static bool markReadIfOld(TNewsEntry& entry, shared::IClockService& clock);
    void insertOrAssign(const TNewsEntry& entry);

    std::shared_ptr<shared::IClockService> m_clock;
    int m_version = 0;
    qint64 m_lastUpdateTimestamp = 0;
    bool m_returnerInformation = false;
    bool m_isReturner = false;
    bool m_showRewardNews = false;
    std::map<uint, TNewsEntry> m_entries;
    ushort m_accountLevel = 0;
    ushort m_characterLevel = 0;
};

}
}

Q_DECLARE_METATYPE(tibia::news::TNewsData)
