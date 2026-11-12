#include "graphics.h"

#include "glutil.h"
#include "texturemanager.h"
#include "render/irenderbackend.h"
#include <framework/core/logger.h>

Graphics g_graphics;

Graphics::Graphics() = default;
Graphics::~Graphics() = default;

void Graphics::selectEngine() {}

void Graphics::init()
{
    g_textures.init();
    m_gl = false;
    m_ok = true;
}

bool Graphics::initEngine(const std::string&) { return false; }

bool Graphics::initGL()
{
#if defined(OPENGL_ES) || defined(__EMSCRIPTEN__) || defined(__ANDROID__)
    m_gl = true;
    return true;
#else
    // Desktop host draws through Qt Quick (Metal/Vulkan/D3D); engine GL is not loaded.
    return false;
#endif
}

void Graphics::setDeviceInfo(std::string vendor, std::string renderer, std::string version, const int maxTextureSize)
{
    m_vendor = std::move(vendor);
    m_renderer = std::move(renderer);
    m_version = std::move(version);
    if (maxTextureSize > 0)
        m_maxTextureSize = maxTextureSize;
}

std::vector<std::string> Graphics::getAvailableBackends() const { return {}; }

std::string Graphics::getBackendName(const std::string& engine) const { return engine; }

void Graphics::terminate()
{
    if (m_backend) {
        m_backend->shutdown();
        m_backend.reset();
    }
    g_textures.terminate();
    m_ok = false;
}

void Graphics::resize(const Size& size) { m_viewportSize = size; }
