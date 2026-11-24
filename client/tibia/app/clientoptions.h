#pragma once

#include <QJsonObject>
#include <QString>

namespace tibia {
namespace config {
class TClientOptions;
}
namespace app {

/// A JSON view of config::TClientOptions for host and pack code: load() is
/// the options as TClientOptions::toJson writes them; store and storeAll go
/// through its typed setters and save the file.
class ClientOptions
{
public:
    static ClientOptions load();
    static QString userFilePath();
    /// Sets one option (TClientOptions' typed setters, which announce it): a
    /// no-op when the option already holds `value`, else written and saved.
    static void store(const QString& name, const QJsonValue& value);
    /// Writes many settings at once (the options dialog's apply): every section
    /// of `changes` key by key, keeping the keys it does not name.
    /// The typed setters announce each change; the file is saved once.
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
