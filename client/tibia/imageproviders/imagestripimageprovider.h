#pragma once

#include <QImage>
#include <QQuickAsyncImageProvider>
#include <QQuickImageProvider>

#include <mutex>

class QQmlEngine;

namespace tibia::imageproviders {

/// A horizontal strip of equally wide cells; image://<name>/<index> is cell <index>.
/// Cells are as wide as the image is high unless a cell width is given.
/// An index outside the strip yields a transparent cell.
class TImagestrip
{
public:
    explicit TImagestrip(QString file, int cellWidth = 0) : m_file(std::move(file)), m_cellWidth(cellWidth) {}

    QImage cell(const QString& id) const;
    int cellCount() const;

private:
    void load() const;

    QString m_file;
    mutable int m_cellWidth;
    mutable int m_cellHeight = 0;
    mutable QImage m_image;
    mutable std::once_flag m_loaded;
};

class TImagestripImageProvider final : public QQuickImageProvider
{
public:
    explicit TImagestripImageProvider(QString file, int cellWidth = 0)
        : QQuickImageProvider(Image), m_strip(std::move(file), cellWidth) {}

    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;
    const TImagestrip& strip() const { return m_strip; }

private:
    TImagestrip m_strip;
};

/// Same strip, answered asynchronously; rich text (<img src="image://…">) needs it.
class TImagestripRichTextImageProvider final : public QQuickAsyncImageProvider
{
public:
    explicit TImagestripRichTextImageProvider(QString file) : m_strip(std::move(file)) {}

    QQuickImageResponse* requestImageResponse(const QString& id, const QSize& requestedSize) override;

private:
    TImagestrip m_strip;
};

/// Registers every CIP image provider on the engine (CIP 0x10091f5b0).
void registerImageProviders(QQmlEngine* engine);

/// Number of cells in spell-icons-20x20 (CIP DAT_1077b0738).
int spellIconCount();

}
