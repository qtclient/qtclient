#pragma once

#include <QString>
#include <QUrl>

#include <memory>

namespace tibia {
namespace shared {
class TJsonDocumentUploaderBase;
}
namespace config {

/// CIP config::TClientOptionsUploader: posts the client options of a login to
/// its url, once per uploader.
class TClientOptionsUploader
{
public:
    explicit TClientOptionsUploader(QUrl Url);
    ~TClientOptionsUploader();

    /// CIP: the first call posts { dataType 4, clientID, account, config }
    /// (`ExportedOptions` is the options' base64 export); later calls do nothing.
    void upload(const QString& Account, const QString& ExportedOptions);

private:
    QUrl m_url;
    std::shared_ptr<shared::TJsonDocumentUploaderBase> m_pUploader;
    bool m_uploaded = false;
};

}
}
