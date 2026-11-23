#include "network/unencryptedrawmessagestream.h"

namespace tibia::network {

TUnencryptedRawMessageStream::TUnencryptedRawMessageStream()
    : QBuffer(static_cast<QObject*>(nullptr))
{
    open(QIODevice::ReadWrite);
}

TUnencryptedRawMessageStream::~TUnencryptedRawMessageStream() = default;

void TUnencryptedRawMessageStream::appendData(const QByteArray& Data)
{
    const qint64 position = pos();
    seek(buffer().size());
    write(Data);
    seek(position);
}

void TUnencryptedRawMessageStream::removeReadData()
{
    QByteArray remaining = buffer();
    remaining.remove(0, pos());
    buffer().swap(remaining);
    seek(0);
}

QByteArray TUnencryptedRawMessageStream::readAllData()
{
    return readAll();
}

}
