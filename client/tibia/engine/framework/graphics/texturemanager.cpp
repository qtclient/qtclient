/*
 * Copyright (c) 2024-2026 qtclient contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "texturemanager.h"

#include "animatedtexture.h"
#include "image.h"
#include "texture.h"
#include "framework/core/clock.h"
#include "framework/core/eventdispatcher.h"
#include "framework/core/resourcemanager.h"

#include <algorithm>

#ifdef FRAMEWORK_NET
#include <framework/net/protocolhttp.h>
#endif

TextureManager g_textures;

namespace {

TexturePtr textureFromImage(const ImagePtr& image)
{
    if (!image)
        return nullptr;

    if (image->isAnimated()) {
        const auto& frames = image->getAnimation();
        std::vector<ImagePtr> frameImages;
        std::vector<uint16_t> framesDelay;
        frameImages.reserve(frames.size());
        framesDelay.reserve(frames.size());
        for (const auto& frame : frames) {
            framesDelay.push_back(static_cast<uint16_t>(std::clamp(frame.delay, 0, 65535)));
            frameImages.push_back(frame.image);
        }

        const auto& animatedTexture = std::make_shared<AnimatedTexture>(
            frameImages.front()->getSize(), frameImages, framesDelay, image->animationLoopCount());
        return animatedTexture;
    }

    return std::make_shared<Texture>(image, false, false);
}

} // namespace

void TextureManager::init() { m_emptyTexture = std::make_shared<Texture>(); }

void TextureManager::terminate()
{
    if (m_liveReloadEvent) {
        m_liveReloadEvent->cancel();
        m_liveReloadEvent = nullptr;
    }
    m_textures.clear();
    m_animatedTextures.clear();
    m_matrixCache.objects.clear();
    m_emptyTexture = nullptr;
}

void TextureManager::poll()
{
    // update only every 16msec, this allows upto 60 fps for animated textures
    static ticks_t lastUpdate = 0;

    const ticks_t now = g_clock.millis();
    if (now - lastUpdate < 16)
        return;

    lastUpdate = now;

    std::shared_lock l(m_mutex);
    for (const auto& animatedTexture : m_animatedTextures)
        animatedTexture->update();
}

void TextureManager::clearCache()
{
    std::unique_lock l(m_mutex);
    m_animatedTextures.clear();
    m_textures.clear();
}

void TextureManager::liveReload()
{
    if (m_liveReloadEvent)
        return;

    m_liveReloadEvent = tibia::schedule::every([this] {
        std::shared_lock l(m_mutex);
        for (const auto& [fileName, tex] : m_textures) {
            const auto& path = tibia::files::data().guessFilePath(fileName, "png");
            if (tex->getTime() >= tibia::files::data().getFileTime(path))
                continue;

            const auto& image = Image::load(path);
            if (!image)
                continue;
            tex->uploadPixels(image, tex->hasMipmaps());
            tex->setTime(stdext::time());
        }
    }, 1000);
}

TexturePtr TextureManager::getTexture(const std::string& fileName, const bool smooth)
{
    TexturePtr texture;

    // before must resolve filename to full path
    const auto& filePath = tibia::files::data().resolvePath(fileName);

    // check if the texture is already loaded
    {
        std::shared_lock l(m_mutex);
        const auto it = m_textures.find(filePath);
        if (it != m_textures.end()) {
            texture = it->second;
        }
    }

#ifdef FRAMEWORK_NET
    // load texture from "virtual directory"
    if (filePath.substr(0, 11) == "/downloads/") {
        std::string _filePath = filePath;
        const auto& fileDownload = g_http.getFile(_filePath.erase(0, 11));
        if (fileDownload) {
            std::stringstream fin(fileDownload->response);
            texture = loadTexture(fin);
        }
    }
#endif

    // texture not found, load it
    if (!texture) {
        try {
            if (filePath.ends_with(".bmp.lzma") || filePath.ends_with(".BMP.LZMA")) {
                // CIP catalog images (subarea masks, etc.) — do not append .png.
                if (const auto& image = Image::loadCipBmp(filePath)) {
                    texture = std::make_shared<Texture>(image);
                } else {
                    g_logger.error("Unable to load CIP texture '{}'", fileName);
                }
            } else {
                texture = textureFromImage(Image::load(filePath));
            }
        } catch (const stdext::exception& e) {
            g_logger.error("Unable to load texture '{}': {}", fileName, e.what());;
        }

        if (texture) {
            texture->m_source = filePath;
            texture->setTime(stdext::time());
            texture->allowAtlasCache();
            std::unique_lock l(m_mutex);
            if (texture->isAnimatedTexture())
                texture = m_animatedTextures.emplace_back(std::static_pointer_cast<AnimatedTexture>(texture));
            m_textures[filePath] = texture;
        }
    }

    if (texture) {
        texture->m_lastTimeUsage.restart();
        if (texture->isSmooth() != smooth) {
            texture->setSmooth(smooth);
        }
    }

    return texture;
}

TexturePtr TextureManager::loadTexture(std::stringstream& file)
{
    const std::string buffer = file.str();
    return textureFromImage(Image::loadPNG(buffer.data(), buffer.size()));
}

Matrix3 toMatrix(const Size& size, const bool upsideDown) {
    if (upsideDown) {
        return { 1.0f / size.width(), 0.0f,                                                  0.0f,
                      0.0f,                 -1.0f / size.height(),                                0.0f,
                      0.0f,                  size.height() / static_cast<float>(size.height()), 1.0f };
    }

    return { 1.0f / size.width(), 0.0f, 0.0f,
        0.0f, 1.0f / size.height(), 0.0f,
        0.0f, 0.0f, 1.0f };
}

const Matrix3* TextureManager::getMatrixById(uint16_t id) {
    return id < m_matrixCache.objects.size() ? m_matrixCache.objects[id].get() : nullptr;
}

uint16_t TextureManager::getMatrixId(const Size& size, bool upsidedown) {
    const uint64_t hash = (static_cast<uint64_t>(size.height()) << 33) | (static_cast<uint64_t>(size.width()) << 1) | (upsidedown ? 1 : 0);
    auto it = m_matrixCache.indexMap.find(hash);
    if (it != m_matrixCache.indexMap.end()) {
        return it->second;
    }

    const auto id = m_matrixCache.objects.size();
    m_matrixCache.indexMap[hash] = id;
    m_matrixCache.objects.emplace_back(std::make_unique<Matrix3>(toMatrix(size, upsidedown)));
    return id;
}
