#pragma once

#include <QJsonObject>
#include <QString>

namespace tibia::shared {

/// CIP `shared::TClipboardHelper`: system clipboard text plus the
/// base64-of-CBOR encoding the client uses to share JSON objects as text.
class TClipboardHelper
{
public:
    virtual ~TClipboardHelper() = default;

    virtual void setTextInClipboard(const QString& text);
    virtual QString getTextFromClipboard();
    virtual QJsonObject getBase64StringAsJsonObject();
    virtual void setJsonObjectAsBase64String(const QJsonObject& jsonObject);
    virtual QJsonObject convertBase64StringAsJsonObject(const QString& base64String);
    virtual QString convertJsonObjectToBase64String(const QJsonObject& jsonObject);
};

}
