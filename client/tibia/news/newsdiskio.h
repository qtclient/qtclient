#pragma once

#include "news/newsstorage.h"

#include <QObject>

#include <memory>
#include <optional>

class QThreadPool;

namespace tibia::shared {
class TTibiaFileSystemHelper;
}

namespace tibia {
namespace news {

/// CIP news::TNewsDiskIO: reads and writes the news cache (compendium.json)
/// through the file system helper.
class TNewsDiskIO : public QObject
{
    Q_OBJECT
public:
    explicit TNewsDiskIO(std::shared_ptr<shared::TTibiaFileSystemHelper> fileSystemHelper);
    ~TNewsDiskIO() override;

    /// Reports the cached news through loadingNewsDataSucceeded.
    virtual void loadNewsDataFromDiskIfPossible() = 0;
    virtual void saveNewsDataToDisk(const tibia::news::TNewsData& data) = 0;
    /// Waits for pending writes.
    virtual void waitForFinished() = 0;

Q_SIGNALS:
    void loadingNewsDataSucceeded(const tibia::news::TNewsData& NewsData);

protected:
    /// The cached document's news.
    std::optional<TNewsData> loadNewsData() const;
    void saveNewsData(const TNewsData& data) const;

    std::shared_ptr<shared::TTibiaFileSystemHelper> m_fileSystemHelper;
};

/// CIP news::TNewsDiskIOSynchronous: reads and writes on the calling thread.
class TNewsDiskIOSynchronous final : public TNewsDiskIO
{
    Q_OBJECT
public:
    using TNewsDiskIO::TNewsDiskIO;

    void loadNewsDataFromDiskIfPossible() override;
    void saveNewsDataToDisk(const tibia::news::TNewsData& data) override;
    void waitForFinished() override;
};

/// CIP news::TNewsDiskIOAsync: reads and writes on a one-thread pool; the
/// loaded news arrive on the owner's thread.
class TNewsDiskIOAsync final : public TNewsDiskIO
{
    Q_OBJECT
public:
    explicit TNewsDiskIOAsync(std::shared_ptr<shared::TTibiaFileSystemHelper> fileSystemHelper);
    ~TNewsDiskIOAsync() override;

    void loadNewsDataFromDiskIfPossible() override;
    void saveNewsDataToDisk(const tibia::news::TNewsData& data) override;
    void waitForFinished() override;

private:
    std::shared_ptr<QThreadPool> m_threadPool;
};

}
}
