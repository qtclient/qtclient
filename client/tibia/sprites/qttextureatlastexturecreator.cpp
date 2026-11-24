#include "sprites/qttextureatlastexturecreator.h"

#include "sprites/spritesheetmanager.h"

#include <QImage>
#include <QPointer>
#include <QQuickWindow>
#include <QRunnable>
#include <QSGTexture>

namespace tibia::sprites {

TQtTextureAtlasTextureCreator::TQtTextureAtlasTextureCreator(QQuickWindow* window)
    : m_window(window)
{
    registerSpritesheetMetaTypes();
}

std::shared_ptr<QSGTexture> TQtTextureAtlasTextureCreator::createTextureFromImage(const QImage& image) const
{
    if (!m_window->isSceneGraphInitialized())
        return {};
    QSGTexture* texture = m_window->createTextureFromImage(image, QQuickWindow::TextureCanUseAtlas);
    if (!texture)
        return {};
    return std::shared_ptr<QSGTexture>(texture, [window = QPointer<QQuickWindow>(m_window), rhi = m_window->rhi()](QSGTexture* created) {
        deleteOnRenderThread(window, rhi, created);
    });
}

std::shared_ptr<QSGTexture> TQtTextureAtlasTextureCreator::createNonAtlasTextureFromImage(const QImage& image) const
{
    if (!m_window->isSceneGraphInitialized())
        return {};
    QSGTexture* texture = m_window->createTextureFromImage(image);
    if (!texture)
        return {};
    return std::shared_ptr<QSGTexture>(texture, [window = QPointer<QQuickWindow>(m_window), rhi = m_window->rhi()](QSGTexture* created) {
        deleteOnRenderThread(window, rhi, created);
    });
}

void TQtTextureAtlasTextureCreator::deleteOnRenderThread(const QPointer<QQuickWindow>& window, QRhi* rhi,
                                                         QSGTexture* texture)
{
    // Host: a texture let go while its window is being destroyed went with the
    // window's QRhi.
    if (!window)
        return;
    QQuickWindow* target = window.data();
    target->scheduleRenderJob(QRunnable::create([target, rhi, texture] {
                                  if (target->rhi() == rhi && texture)
                                      delete texture;
                              }),
                              QQuickWindow::BeforeSynchronizingStage);
}

} // namespace tibia::sprites
