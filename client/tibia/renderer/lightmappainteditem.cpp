#include "lightmappainteditem.h"

#include "renderer/wasnglsl.h"
#include "renderer/worldmaprenderprovider.h"

#include <framework/core/graphicalapplication.h>

#include <QQuickWindow>
#include <QSGRenderNode>
#include <QSGRendererInterface>
#include <rhi/qrhi.h>
#include <rhi/qshader.h>

#include <cstring>

namespace tibia::qmlcomponents {

namespace {

QShader loadOverlayShader(const char* name)
{
    return tibia::renderer::loadStockShader(name);
}

/// Multiplies the classic light map into the framebuffer (CIP Blend mode multiply).
/// One QRhi update batch per prepare(), always submitted — otherwise the pool of 64
/// is exhausted when the map wrapper's layer grabs a second pass.
class MultiplyLightNode final : public QSGRenderNode
{
public:
    explicit MultiplyLightNode(QQuickWindow* window)
        : m_window(window)
    {}

    ~MultiplyLightNode() override { releaseResources(); }

    void setImage(const QImage& image)
    {
        if (image.size() == m_image.size() && image.constBits() == m_image.constBits())
            return;
        // Provider gives a fresh copy each frame; compare bytes only when size matches.
        if (image.size() == m_image.size() && !m_image.isNull() && image.sizeInBytes() == m_image.sizeInBytes()
            && std::memcmp(image.constBits(), m_image.constBits(), static_cast<size_t>(image.sizeInBytes())) == 0) {
            return;
        }
        m_image = image;
        m_imageDirty = true;
        markDirty(DirtyGeometry | DirtyMaterial);
    }

    void prepare() override
    {
        if (m_image.isNull() || !m_window)
            return;
        QSGRendererInterface* rif = m_window->rendererInterface();
        if (!rif || rif->graphicsApi() == QSGRendererInterface::Software)
            return;
        auto* rhi = static_cast<QRhi*>(rif->getResource(m_window, QSGRendererInterface::RhiResource));
        if (!rhi || !commandBuffer() || !renderTarget())
            return;
        if (!m_rhi) {
            m_rhi = rhi;
            if (!create()) {
                destroy();
                return;
            }
        }

        // Single batch for this prepare; always submit below (no early return after this).
        QRhiResourceUpdateBatch* batch = rhi->nextResourceUpdateBatch();
        if (!batch)
            return;

        if (m_imageDirty)
            uploadInto(batch);

        const QMatrix4x4 mvp = *projectionMatrix() * *matrix();
        std::memcpy(m_uboData, mvp.constData(), 64);
        batch->updateDynamicBuffer(m_ubo, 0, 64, m_uboData);

        ensurePipeline();
        commandBuffer()->resourceUpdate(batch);
    }

    void render(const RenderState* state) override
    {
        if (!m_pipeline || !m_texture || m_image.isNull())
            return;
        QRhiCommandBuffer* cb = commandBuffer();
        cb->setGraphicsPipeline(m_pipeline);
        const QSize size = renderTarget()->pixelSize();
        cb->setViewport(QRhiViewport(0, 0, static_cast<float>(size.width()), static_cast<float>(size.height())));
        if (state->scissorEnabled()) {
            const QRect clip = state->scissorRect();
            cb->setScissor(QRhiScissor(clip.x(), clip.y(), clip.width(), clip.height()));
        } else {
            cb->setScissor(QRhiScissor(0, 0, size.width(), size.height()));
        }
        cb->setShaderResources(m_bindings);
        const QRhiCommandBuffer::VertexInput input(m_vbo, 0);
        cb->setVertexInput(0, 1, &input);
        cb->draw(4);
    }

    void releaseResources() override { destroy(); }

    StateFlags changedStates() const override { return BlendState; }
    RenderingFlags flags() const override { return BoundedRectRendering | DepthAwareRendering; }
    QRectF rect() const override
    {
        return QRectF(0, 0, m_image.isNull() ? 0 : m_image.width(), m_image.isNull() ? 0 : m_image.height());
    }

private:
    bool create()
    {
        m_vertex = loadOverlayShader("lightmap_overlay.vert");
        m_fragment = loadOverlayShader("lightmap_overlay.frag");
        if (!m_vertex.isValid() || !m_fragment.isValid())
            return false;
        m_ubo = m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 64);
        if (!m_ubo->create())
            return false;
        m_vbo = m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, 4 * 4 * sizeof(float));
        if (!m_vbo->create())
            return false;
        m_sampler = m_rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None, QRhiSampler::ClampToEdge,
                                      QRhiSampler::ClampToEdge);
        return m_sampler->create();
    }

    void uploadInto(QRhiResourceUpdateBatch* batch)
    {
        m_imageDirty = false;
        const QSize size = m_image.size();
        if (!m_texture || m_texture->pixelSize() != size) {
            delete m_texture;
            m_texture = m_rhi->newTexture(QRhiTexture::RGBA8, size, 1, QRhiTexture::Flags());
            if (!m_texture->create()) {
                delete m_texture;
                m_texture = nullptr;
                m_imageDirty = true;
                return;
            }
            m_pipeline = nullptr; // bindings changed
            m_format.clear();
        }
        // CIP paints ARGB32_Premultiplied. convertToFormat(RGBA8888) unpremultiplies.
        QImage rgba(size, QImage::Format_RGBA8888);
        for (int y = 0; y < size.height(); ++y) {
            const auto* src = reinterpret_cast<const QRgb*>(m_image.constScanLine(y));
            auto* dst = reinterpret_cast<QRgb*>(rgba.scanLine(y));
            for (int x = 0; x < size.width(); ++x)
                dst[x] = qRgba(qRed(src[x]), qGreen(src[x]), qBlue(src[x]), qAlpha(src[x]));
        }
        batch->uploadTexture(m_texture, rgba);

        const float w = float(size.width());
        const float h = float(size.height());
        const float verts[] = {
            0, 0, 0, 0, w, 0, 1, 0, 0, h, 0, 1, w, h, 1, 1,
        };
        batch->updateDynamicBuffer(m_vbo, 0, sizeof(verts), verts);

        if (!m_bindings)
            m_bindings = m_rhi->newShaderResourceBindings();
        m_bindings->setBindings({ QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage, m_ubo),
                                  QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, m_texture,
                                                                            m_sampler) });
        m_bindings->create();
    }

    void ensurePipeline()
    {
        if (m_pipelineFailed || !m_texture || !m_bindings || !renderTarget())
            return;
        QRhiRenderPassDescriptor* targetPass = renderTarget()->renderPassDescriptor();
        const QVector<quint32> format = targetPass->serializedFormat();
        const int samples = renderTarget()->sampleCount();
        if (m_pipeline && format == m_format && samples == m_samples)
            return;
        delete m_pipeline;
        m_pipeline = m_rhi->newGraphicsPipeline();
        QRhiGraphicsPipeline::TargetBlend multiply;
        multiply.enable = true;
        multiply.srcColor = QRhiGraphicsPipeline::DstColor;
        multiply.dstColor = QRhiGraphicsPipeline::Zero;
        multiply.srcAlpha = QRhiGraphicsPipeline::Zero;
        multiply.dstAlpha = QRhiGraphicsPipeline::One;
        m_pipeline->setTargetBlends({ multiply });
        m_pipeline->setTopology(QRhiGraphicsPipeline::TriangleStrip);
        m_pipeline->setShaderStages({ { QRhiShaderStage::Vertex, m_vertex }, { QRhiShaderStage::Fragment, m_fragment } });
        QRhiVertexInputLayout layout;
        layout.setBindings({ { 4 * sizeof(float) } });
        layout.setAttributes({ { 0, 0, QRhiVertexInputAttribute::Float2, 0 },
                              { 0, 1, QRhiVertexInputAttribute::Float2, 2 * sizeof(float) } });
        m_pipeline->setVertexInputLayout(layout);
        m_pipeline->setShaderResourceBindings(m_bindings);
        m_pipeline->setRenderPassDescriptor(targetPass);
        m_pipeline->setSampleCount(samples);
        m_pipeline->setDepthTest(false);
        m_pipeline->setDepthWrite(false);
        if (!m_pipeline->create()) {
            delete m_pipeline;
            m_pipeline = nullptr;
            // Do not retry every frame — WebGL link failures spam the console.
            m_pipelineFailed = true;
            return;
        }
        m_format = format;
        m_samples = samples;
    }

    void destroy()
    {
        delete m_pipeline;
        m_pipeline = nullptr;
        delete m_bindings;
        m_bindings = nullptr;
        delete m_sampler;
        m_sampler = nullptr;
        delete m_texture;
        m_texture = nullptr;
        delete m_vbo;
        m_vbo = nullptr;
        delete m_ubo;
        m_ubo = nullptr;
        m_rhi = nullptr;
        m_format.clear();
        m_imageDirty = true;
        m_pipelineFailed = false;
    }

    QQuickWindow* m_window = nullptr;
    QRhi* m_rhi = nullptr;
    QImage m_image;
    bool m_imageDirty = false;
    bool m_pipelineFailed = false;
    QShader m_vertex;
    QShader m_fragment;
    QRhiBuffer* m_ubo = nullptr;
    QRhiBuffer* m_vbo = nullptr;
    QRhiTexture* m_texture = nullptr;
    QRhiSampler* m_sampler = nullptr;
    QRhiShaderResourceBindings* m_bindings = nullptr;
    QRhiGraphicsPipeline* m_pipeline = nullptr;
    QVector<quint32> m_format;
    int m_samples = 0;
    float m_uboData[16]{};
};

}

TLightmapPaintedItem::TLightmapPaintedItem(QQuickItem* parent)
    : cip::tibia::qmlcomponents::TLightmapPaintedItem(parent)
{
    setTransformOrigin(TopLeft);
    setFlag(ItemHasContents);
}

void TLightmapPaintedItem::setRenderProvider(TWorldMapRenderProvider* provider)
{
    disconnect(m_frameConnection);
    m_provider = provider;
    if (provider)
        m_frameConnection = connect(provider, &TWorldMapRenderProvider::overlaysAdvanced, this, &TLightmapPaintedItem::takeFrame);
    takeFrame();
}

void TLightmapPaintedItem::takeFrame()
{
    const TWorldMapRenderProvider::Frame* frame = m_provider ? &m_provider->frame() : nullptr;
    if (!frame || !frame->valid || frame->light.isNull()) {
        m_image = QImage();
        update();
        return;
    }
    m_image = frame->light.copy();
    // Logical pixels. The map pane's scale pass enlarges this with the sprites.
    setPosition(QPointF(-frame->camera.x(), -frame->camera.y()));
    setSize(m_image.size());
    update();
}

QSGNode* TLightmapPaintedItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*)
{
    if (m_image.isNull()) {
        delete oldNode;
        return nullptr;
    }
    auto* node = static_cast<MultiplyLightNode*>(oldNode);
    if (!node)
        node = new MultiplyLightNode(window());
    node->setImage(m_image);
    return node;
}

}
