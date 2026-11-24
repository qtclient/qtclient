#pragma once

#include <QSGRendererInterface>
#include <QString>
#include <QStringList>

#include <memory>

class QTranslator;

class QQmlEngine;

namespace tibia {
namespace app {

/// CIP renderer index (option rendererIndex) to the Qt graphics API. Index 0
/// and unknown indices are QSGRendererInterface::Unknown (Qt picks).
QSGRendererInterface::GraphicsApi graphicsApiForRenderer(int rendererIndex);
/// The CIP renderer index of a Qt graphics API (0 for APIs CIP has no renderer for).
int rendererIndexForGraphicsApi(QSGRendererInterface::GraphicsApi api);
/// Accepts the CIP index or a name: auto, software, d3d11, vulkan, opengl, metal, d3d12.
int parseRendererIndex(const QString& value, int fallback);
/// CIP: the renderers this machine can run, probed once the application
/// exists: auto always, Metal, OpenGL and Vulkan when Qt can start them
/// (Vulkan 1.0; MoltenVK without Metal heaps). Host: Direct3D 11 / 12 on
/// Windows; the browser has WebGL without a probe.
void probeSupportedRenderers();
/// CIP: whether the probe found the renderer.
bool isRendererSupported(int rendererIndex);
/// CIP: the renderer index the process started its graphics with (0 auto).
void setActiveGraphicsEngine(int rendererIndex);
int activeGraphicsEngine();
/// CIP: the process renders with the software renderer (index 3).
bool isSoftwareGraphicsEngineActive();
/// Host: --renderer on the command line (or QTCLIENT_RENDERER), -1 when none.
int rendererFromCommandLine();
/// CIP: Qt 5 builds installed QML modules next to the binary; the folder is
/// removed so they cannot clash with Qt 6's (install directories only).
void removeOldQtQmlFolder();

/// After QGuiApplication exists: point VK loader at MoltenVK ICD in the qtclient bundle (macOS).
void applyBundledMoltenVkIcd();

/// Mounts CIP's art pack (graphics_resources.rcc, resource set "graphics")
/// at qrc:. Returns the mounted file, empty when none of dirs has it.
QString mountGraphicsResources(const QStringList& dirs);

/// Installs CIP's client strings (client.en.qm) on the application (the
/// client-less start; the game application loads them with its translation
/// loader).
void installTranslations();
/// Host: the translator CIP's client strings are loaded into; it shows the
/// application name where the strings name the client "Tibia".
std::unique_ptr<QTranslator> createClientTranslator();
/// Host: where the client strings are (CIP: the application directory;
/// qtclient bundles client.en.qm in its resources).
QString clientTranslationDirectory();

/// CIP QML asks for Verdana. Desktop has it as a system font; the browser does
/// not, so load the same faces from `<data>/fonts` when the family is missing.
void installUiFonts(const QStringList& dirs);

/// Makes CIP's PreferDefaultHinting keep the same horizontal advances on the
/// browser font engine as on the desktop. No effect outside wasm.
void alignTextHinting(QQmlEngine* engine);

/// QML engine with CipSoft base URL, qmlcomponents types, and pack interceptors.
std::unique_ptr<QQmlEngine> createEngine();

}
}
