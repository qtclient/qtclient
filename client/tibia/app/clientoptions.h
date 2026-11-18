#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>

namespace tibia {
namespace config {
class TClientOptions;
}
namespace app {

/// TClientOptions' change notification (CIP controllers' onOptionChanged).
class ClientOptionsNotifier final : public QObject
{
    Q_OBJECT
public:
    static ClientOptionsNotifier* instance();

Q_SIGNALS:
    /// ClientOptions::store changed the option `name`, or a typed
    /// TClientOptions setter changed it (the client window: its section name).
    void optionChanged(const QString& name);

private:
    /// Forwards TClientOptions' typed changes and saves the file after them.
    ClientOptionsNotifier();

    bool m_saveScheduled = false;
};

/// A JSON view of config::TClientOptions for host and pack code: load() is
/// the options as TClientOptions::toJson writes them; store and storeAll go
/// through its typed setters and save the file.
class ClientOptions
{
public:
    static ClientOptions load();
    /// The stored clientoptions.json only, for main() before QGuiApplication
    /// (renderer, vsync, atlas). It leaves config::TClientOptions alone: that
    /// singleton loads once the app exists, under the pack's AppData identity
    /// and with an event dispatcher for its option timers.
    static ClientOptions readStoredFile();
    static QString userFilePath();
    /// Sets one option (TClientOptions' typed setters): a no-op when the
    /// option already holds `value`, else written and announced by optionChanged.
    static void store(const QString& name, const QJsonValue& value);
    /// Writes many settings at once (the options dialog's apply): every section
    /// of `changes` key by key, keeping the keys it does not name.
    /// Emits optionChanged for each changed option, and once per other changed section.
    static void storeAll(const QJsonObject& changes);
    /// The options dialog's apply: whatever `pending` (a working copy of the
    /// client's options) holds differently, written as storeAll writes.
    static void storeChanged(const config::TClientOptions& pending);

    const QJsonObject& root() const { return m_root; }
    /// The "options" section, keyed by CIP's option names (cip_options.h).
    QJsonObject options() const { return m_root.value(QLatin1String("options")).toObject(); }

    /// Option rendererIndex: 0 auto, 3 software, 6 D3D11, 7 Vulkan, 8 OpenGL, 9 Metal, 10 D3D12.
    int rendererIndex() const;
    /// Option vSyncEnabled.
    bool vSyncEnabled() const;

private:
    QJsonObject m_root;
};

}
}
