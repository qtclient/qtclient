#pragma once

#include "jsondocumentuploaderbase.h"

#include <QJsonDocument>
#include <QUrl>

#include <memory>

namespace tibia::shared {
class IClockService;
class TTibiaFileSystemHelper;
}

namespace tibia {
namespace game {

/// Today's boosted creature and boss (race ids).
struct TBoostedCreatureInformation
{
    int creatureRaceId = 0;
    int bossRaceId = 0;
};

/// CIP game::TBoostedCreatureRequestUploader: the boosted creature from the
/// disk cache (boostedcreature) until the last server save, else from the
/// client services (`{type:"boostedcreature"}`).
class TBoostedCreatureRequestUploader final : public shared::TJsonDocumentUploaderBase
{
    Q_OBJECT
public:
    TBoostedCreatureRequestUploader(const QUrl& url, std::shared_ptr<shared::TJsonDocumentHttpPostSender> sender,
                                    std::shared_ptr<shared::TTibiaFileSystemHelper> fileSystemHelper,
                                    std::shared_ptr<shared::IClockService> clock, QObject* parent = nullptr);
    ~TBoostedCreatureRequestUploader() override;

    const TBoostedCreatureInformation& boostedCreature() const { return m_boostedCreature; }

    /// Reads the race ids present as integers into `information`; missing or
    /// malformed fields keep their value.
    static void readBoostedCreature(TBoostedCreatureInformation& information, const QJsonDocument& document);
    static QJsonDocument boostedCreatureDocument(const TBoostedCreatureInformation& information);

Q_SIGNALS:
    void boostedCreatureReceived();

public Q_SLOTS:
    void requestUpdate();
    void onFinished(bool UploadSuccessful, const QJsonDocument& ReplyFromServerJD);

private:
    std::shared_ptr<shared::TTibiaFileSystemHelper> m_fileSystemHelper;
    std::shared_ptr<shared::IClockService> m_clock;
    QUrl m_url;
    TBoostedCreatureInformation m_boostedCreature;
};

}
}
