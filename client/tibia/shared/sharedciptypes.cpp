#include "shared/sharedciptypes.h"

namespace tibia::shared {

namespace {
struct CipTypedKeep {
    TExperienceGainChangeTracker tExperienceGainChangeTracker;
    TIODeviceReader tIODeviceReader;
    TIODeviceWriter tIODeviceWriter;
    TImagestripResponse tImagestripResponse;
    TNullExperienceGainChangeTracker tNullExperienceGainChangeTracker;
    TNullExperiencePerHourProvider tNullExperiencePerHourProvider;
    TTibiaClipboardHelper tTibiaClipboardHelper;
    TTibiaFileSystemHelper tTibiaFileSystemHelper;
    QTextBrowserWithNetwork qTextBrowserWithNetwork;
};
CipTypedKeep& typedKeep() { static CipTypedKeep keep; return keep; }
} // namespace

void QTextBrowserWithNetwork::onDownloadFailed(QUrl Url, QNetworkReply::NetworkError Code, QString ErrorString)
{
    Q_UNUSED(Url);
    Q_UNUSED(Code);
    Q_UNUSED(ErrorString);
}

void QTextBrowserWithNetwork::onDownloadFinished(QUrl Url, QByteArray Data)
{
    Q_UNUSED(Url);
    Q_UNUSED(Data);
}

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    (void)typedKeep();
    // CipSoft network wrapper surfaces — live under the product owner for moc.
    new QNetworkAccessManagerWrapper(parent);
    new QNetworkReplyWrapper(parent);
}

} // namespace tibia::shared
