#include "clipboardhelper.h"

#include <QCborMap>
#include <QCborParserError>
#include <QCborValue>
#include <QClipboard>
#include <QGuiApplication>
#include <QJsonValue>

namespace tibia::shared {

void TClipboardHelper::setTextInClipboard(const QString& text)
{
    QGuiApplication::clipboard()->setText(text);
}

QString TClipboardHelper::getTextFromClipboard()
{
    return QGuiApplication::clipboard()->text();
}

QJsonObject TClipboardHelper::getBase64StringAsJsonObject()
{
    // CIP round-trips the clipboard text through local 8-bit and UTF-8.
    const QByteArray local = getTextFromClipboard().toLocal8Bit();
    return convertBase64StringAsJsonObject(QString::fromUtf8(local));
}

void TClipboardHelper::setJsonObjectAsBase64String(const QJsonObject& jsonObject)
{
    setTextInClipboard(convertJsonObjectToBase64String(jsonObject));
}

QJsonObject TClipboardHelper::convertBase64StringAsJsonObject(const QString& base64String)
{
    QJsonObject result;
    const QByteArray local = base64String.toLocal8Bit();
    if (base64String.isEmpty())
        return result;
    const QByteArray cbor = QByteArray::fromBase64(local, QByteArray::OmitTrailingEquals);
    if (cbor.isEmpty())
        return result;
    QCborParserError error;
    const QCborValue value = QCborValue::fromCbor(cbor, &error);
    if (error.error == QCborError::NoError)
        result = value.toMap().toJsonObject();
    return result;
}

QString TClipboardHelper::convertJsonObjectToBase64String(const QJsonObject& jsonObject)
{
    const QByteArray cbor = QCborValue::fromJsonValue(QJsonValue(jsonObject)).toCbor();
    return QString::fromLocal8Bit(cbor.toBase64(QByteArray::OmitTrailingEquals));
}

}
