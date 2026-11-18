#include "newsdiskio.h"

#include "shared/tibiafilesystemhelper.h"

#include <QThreadPool>

namespace tibia::news {

TNewsDiskIO::TNewsDiskIO(std::shared_ptr<shared::TTibiaFileSystemHelper> fileSystemHelper)
    : QObject(nullptr)
    , m_fileSystemHelper(std::move(fileSystemHelper))
{}

TNewsDiskIO::~TNewsDiskIO() = default;

std::optional<TNewsData> TNewsDiskIO::loadNewsData() const { return parseNewsData(m_fileSystemHelper->readCompendium()); }

void TNewsDiskIO::saveNewsData(const TNewsData& data) const { m_fileSystemHelper->writeCompendium(newsDataToJson(data)); }

void TNewsDiskIOSynchronous::loadNewsDataFromDiskIfPossible()
{
    if (const std::optional<TNewsData> data = loadNewsData())
        Q_EMIT loadingNewsDataSucceeded(*data);
}

void TNewsDiskIOSynchronous::saveNewsDataToDisk(const TNewsData& data) { saveNewsData(data); }

void TNewsDiskIOSynchronous::waitForFinished() {}

TNewsDiskIOAsync::TNewsDiskIOAsync(std::shared_ptr<shared::TTibiaFileSystemHelper> fileSystemHelper)
    : TNewsDiskIO(std::move(fileSystemHelper))
    , m_threadPool(std::make_shared<QThreadPool>())
{
    m_threadPool->setMaxThreadCount(1);
}

TNewsDiskIOAsync::~TNewsDiskIOAsync()
{
    // qtclient: the workers read this object's helper.
    m_threadPool->waitForDone();
}

void TNewsDiskIOAsync::loadNewsDataFromDiskIfPossible()
{
    const auto finish = [this](std::optional<TNewsData> data) {
        if (data)
            Q_EMIT loadingNewsDataSucceeded(*data);
    };
    m_threadPool->start([this, finish] {
        std::optional<TNewsData> data = loadNewsData();
        QMetaObject::invokeMethod(this, [finish, data = std::move(data)] { finish(data); }, Qt::QueuedConnection);
    });
}

void TNewsDiskIOAsync::saveNewsDataToDisk(const TNewsData& data)
{
    m_threadPool->start([this, data] { saveNewsData(data); });
}

void TNewsDiskIOAsync::waitForFinished() { m_threadPool->waitForDone(); }

}
