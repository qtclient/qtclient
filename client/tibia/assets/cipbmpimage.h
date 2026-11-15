#pragma once

#include <QByteArray>
#include <QImage>

namespace tibia::assets {

/// Decodes a CIP catalog image (LZMA-compressed BMP) into RGBA8888; null on failure.
QImage decodeCipBmpImage(const QByteArray& fileData);

}
