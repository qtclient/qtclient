#pragma once

#include "jsondocumentuploaderbase.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QTimer>
#include <QUrl>

#include <memory>

namespace tibia::shared {
class IClockService;
class TTibiaFileSystemHelper;
}

namespace tibia {
namespace game {

/// True when `document` has no integer `lastupdatetimestamp` or it lies more
/// than `maximumAgeSeconds` before `now`.
bool isCachedDocumentOlderThan(const QJsonDocument& document, const QDateTime& now, qint64 maximumAgeSeconds);
/// Stamps `document` with `now` as its `lastupdatetimestamp` (epoch seconds).
void stampLastUpdateTimestamp(QJsonDocument& document, const QDateTime& now);

/// The live numbers of the start screen: players online, streams and viewers.
struct TLiveInformation
{
    int playersOnline = 0;
    int twitchStreams = 0;
    int twitchViewers = 0;
    int gamingYouTubeStreams = 0;
    int gamingYouTubeViewers = 0;
};

/// CIP game::TLiveInformationRequestUploader: the live numbers from the disk
/// cache (onlinenumbers) while they are at most ten minutes old, else from the
/// client services (`{type:"cacheinfo"}`).
class TLiveInformationRequestUploader final : public shared::TJsonDocumentUploaderBase
{
    Q_OBJECT
public:
    TLiveInformationRequestUploader(const QUrl& url, std::shared_ptr<shared::TJsonDocumentHttpPostSender> sender,
                                    std::shared_ptr<shared::TTibiaFileSystemHelper> fileSystemHelper,
                                    std::shared_ptr<shared::IClockService> clock, QObject* parent = nullptr);
    ~TLiveInformationRequestUploader() override;

    const TLiveInformation& liveInformation() const { return m_liveInformation; }

    /// Reads the fields present as integers into `information`; missing or
    /// malformed fields keep their value.
    static void readLiveInformation(TLiveInformation& information, const QJsonDocument& document);
    static QJsonDocument liveInformationDocument(const TLiveInformation& information);

Q_SIGNALS:
    void newLiveInformationReceived();

public Q_SLOTS:
    void requestUpdate();
    void onFinished(bool UploadSuccessful, const QJsonDocument& ReplyFromServerJD);

private:
    std::shared_ptr<shared::TTibiaFileSystemHelper> m_fileSystemHelper;
    std::shared_ptr<shared::IClockService> m_clock;
    QUrl m_url;
    TLiveInformation m_liveInformation;
    /// Single shot; its timeout requests an update.
    QTimer m_updateTimer;
};

}
}
