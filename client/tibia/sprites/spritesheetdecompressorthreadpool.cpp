#include "sprites/spritesheetdecompressorthreadpool.h"

#include <QPointer>
#include <QThreadPool>

namespace tibia::sprites {

void TSpritesheetDecompressorThreadpool::decompress(const TSpritesheetID& spritesheetId,
                                                    const std::shared_ptr<tibia::shared::TReadOnlyMemoryBlob>& data)
{
    // CIP watches a QFuture of the decode and emits from its finished signal on
    // this object's thread; posting the result back does the same.
    const QPointer<TSpritesheetDecompressorThreadpool> self(this);
    QThreadPool::globalInstance()->start([self, spritesheetId, data] {
        TSpritesheetPointer spritesheet = decode(data);
        if (!self)
            return;
        QMetaObject::invokeMethod(self.data(), [self, spritesheetId, spritesheet] {
            if (self)
                Q_EMIT self->decompressingSpritesheetFinished(spritesheetId, spritesheet);
        }, Qt::QueuedConnection);
    });
}

}
