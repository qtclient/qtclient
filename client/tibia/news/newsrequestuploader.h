#pragma once

#include "jsondocumentuploaderbase.h"

#include <QJsonDocument>
#include <QString>
#include <QUrl>

#include <map>
#include <memory>
#include <tuple>

namespace tibia::shared {
class IClockService;
class TTibiaFileSystemHelper;
}

namespace tibia {
namespace news {

class TNewsStorage;

/// The parameters of a news query; queries with the same parameters share a
/// retry count.
struct TNewsQueryParameters
{
    bool returner = false;
    bool highPotential = false;
    uint maxKnownEditDate = 0;

    bool operator<(const TNewsQueryParameters& other) const
    {
        return std::tie(returner, highPotential, maxKnownEditDate)
            < std::tie(other.returner, other.highPotential, other.maxKnownEditDate);
    }
};

/// CIP news::TNewsRequestUploader: queries `{type:"news"}` from the client
/// services and merges the answer into the news storage.
class TNewsRequestUploader final : public shared::TJsonDocumentUploaderBase
{
    Q_OBJECT
public:
    TNewsRequestUploader(const QUrl& url, std::shared_ptr<TNewsStorage> storage,
                         std::shared_ptr<shared::TJsonDocumentHttpPostSender> sender,
                         std::shared_ptr<shared::TTibiaFileSystemHelper> fileSystemHelper,
                         std::shared_ptr<shared::IClockService> clock, QObject* parent = nullptr);
    ~TNewsRequestUploader() override;

    /// Queries the news, at most three times per parameters unless forced (a
    /// forced query asks for every entry).
    void requestNews(const TNewsQueryParameters& parameters, bool force);
    /// Queries the news with the storage's parameters once the cache is stale.
    void requestUpdate();

Q_SIGNALS:
    void newsQueryFailed(const QString& ErrorMessage);

public Q_SLOTS:
    void onFinished(bool UploadSuccessful, const QJsonDocument& ReplyFromServerJD);

private:
    TNewsQueryParameters storageParameters() const;

    std::shared_ptr<shared::TTibiaFileSystemHelper> m_fileSystemHelper;
    std::shared_ptr<shared::IClockService> m_clock;
    QUrl m_url;
    std::shared_ptr<TNewsStorage> m_storage;
    std::map<TNewsQueryParameters, uint> m_retries;
    /// True between requestNews upload and the matching finished (shared sender).
    bool m_pending = false;
};

/// The query document of a news request.
QJsonDocument newsQueryDocument(const TNewsQueryParameters& parameters);

/// CIP news::TNewsViewsUploader: reports `{type:"newsviewed", viewedid}` when a
/// news entry is opened.
class TNewsViewsUploader final : public shared::TJsonDocumentUploaderBase
{
    Q_OBJECT
public:
    TNewsViewsUploader(const QUrl& url, std::shared_ptr<shared::TJsonDocumentHttpPostSender> sender,
                       QObject* parent = nullptr);
    ~TNewsViewsUploader() override;

    void reportNewsViewed(uint id);

private:
    QUrl m_url;
};

/// The document of a news view report.
QJsonDocument newsViewedDocument(uint id);

}
}
