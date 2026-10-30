#pragma once

#include <QSGRendererInterface>
#include <QString>
#include <QStringList>

#include <memory>

class QQmlEngine;

namespace tibia::app {

class ClientOptions;

/// CIP renderer index (option rendererIndex) to the Qt graphics API. Index 0
/// and unknown indices are QSGRendererInterface::Unknown (Qt picks).
QSGRendererInterface::GraphicsApi graphicsApiForRenderer(int rendererIndex);
/// Accepts the CIP index or a name: auto, software, d3d11, vulkan, opengl, metal, d3d12.
int parseRendererIndex(const QString& value, int fallback);
bool isRendererSupported(int rendererIndex);

/// What CIP's startup (FUN_1008e8e80) does before QApplication exists:
/// Quick Controls style, graphics API, render loop, scene graph atlas and
/// surface format. rendererOverride < 0 keeps the options' renderer.
void configureProcess(const ClientOptions& options, int rendererOverride);

/// Mounts CIP's art pack (graphics_resources.rcc, resource set "graphics")
/// at qrc:/. Returns the mounted file, empty when none of dirs has it.
QString mountGraphicsResources(const QStringList& dirs);

/// Installs CIP's client strings (client.en.qm) on the application.
void installTranslations();

/// CIP QML asks for Verdana. Desktop has it as a system font; the browser does
/// not, so load the same faces from `<data>/fonts` when the family is missing.
void installUiFonts(const QStringList& dirs);

/// Makes CIP's PreferDefaultHinting keep the same horizontal advances on the
/// browser font engine as on the desktop. No effect outside wasm.
void alignTextHinting(QQmlEngine* engine);

/// QML engine with CIP's base URL, the qmlcomponents types and the client/overrides interceptor.
std::unique_ptr<QQmlEngine> createEngine();

}
