#include "shared/sharedciptypes.h"

#include <QClipboard>
#include <QCryptographicHash>
#include <QGuiApplication>

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
    TClipboardHelper tClipboardHelper;
    THashHelper tHashHelper;
    TCoordinate tCoordinate;
    TExtent tExtent;
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

void TClipboardHelper::setText(const QString& text) const
{
    if (QClipboard* clip = QGuiApplication::clipboard())
        clip->setText(text);
}

QString TClipboardHelper::text() const
{
    if (QClipboard* clip = QGuiApplication::clipboard())
        return clip->text();
    return {};
}

QByteArray THashHelper::sha256(const QByteArray& data)
{
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256);
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
