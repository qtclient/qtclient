#include "sprites/spritesheetdecompressorthreadpool.h"

#include <QException>
#include <QFutureWatcher>
#include <QPromise>
#include <QThreadPool>

namespace tibia::sprites {

void TSpritesheetDecompressorThreadpool::decompress(const TSpritesheetID& spritesheetId,
                                                    const std::shared_ptr<tibia::shared::TReadOnlyMemoryBlob>& data)
{
    // The decode runs on the thread pool; its future's watcher reports the
    // image on this object's thread and goes. A decode that failed holds a
    // TSpritesheetDecompressorConcurrentException, which reading the result
    // throws again.
    // Host: decompress may be called from the render thread; the watcher
    // lives on this object's thread (a parent on another thread is refused).
    auto* watcher = new QFutureWatcher<TSpritesheetPointer>();
    watcher->moveToThread(thread());
    connect(watcher, &QFutureWatcher<TSpritesheetPointer>::finished, this, [this, watcher, spritesheetId] {
        const TSpritesheetPointer spritesheet = watcher->future().result();
        Q_EMIT decompressingSpritesheetFinished(spritesheetId, spritesheet);
        watcher->deleteLater();
    });
    auto promise = std::make_shared<QPromise<TSpritesheetPointer>>();
    watcher->setFuture(promise->future());
    QThreadPool::globalInstance()->start([promise, data] {
        promise->start();
        try {
            promise->addResult(decode(data));
        } catch (const QException& exception) {
            promise->setException(exception);
        }
        promise->finish();
    });
}

}
