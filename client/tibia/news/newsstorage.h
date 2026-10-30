#pragma once

#include <cip_gen.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector>

namespace tibia::news {

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

    bool isGroupHeader() const
    {
        return type.compare(QLatin1String("GROUP HEADER"), Qt::CaseInsensitive) == 0;
    }
};

/// CIP TNewsStorage — holds portal `{gamenews:[…], categorycounts, …}` for NewsDialog.
class TNewsStorage final : public cip::tibia::news::TNewsStorage
{
    Q_OBJECT
public:
    explicit TNewsStorage(QObject* parent = nullptr);

    const QVector<NewsItem>& items() const { return m_items; }
    const QJsonObject& categoryCounts() const { return m_categoryCounts; }
    qint64 maxEditDate() const { return m_maxEditDate; }
    bool isReturner() const { return m_isReturner; }
    bool showRewardNews() const { return m_showRewardNews; }

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
};

}
