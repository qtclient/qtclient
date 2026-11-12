#pragma once

#include <QByteArray>
#include <QString>

/** Decode an lzma-alone buffer (5-byte props, 8-byte size, compressed payload). */
bool unpackLzmaAlone(const QByteArray& packed, QByteArray& unpacked, QString* error);
