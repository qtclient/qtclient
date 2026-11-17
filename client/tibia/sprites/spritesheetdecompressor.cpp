#include "sprites/spritesheetdecompressor.h"

#include "readonlymemoryblob.h"

namespace tibia::sprites {

TSpritesheetDecompressor::TSpritesheetDecompressor(QObject* parent)
    : QObject(parent)
{
}

TSpritesheetPointer TSpritesheetDecompressor::decode(const std::shared_ptr<tibia::shared::TReadOnlyMemoryBlob>& data)
{
    QImage image;
    image = QImage::fromData(data->getData());
    image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    return std::make_shared<QImage>(std::move(image));
}

}
