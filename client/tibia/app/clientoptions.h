#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>

namespace tibia::app {

/// TClientOptions' change notification (CIP controllers' onOptionChanged).
class ClientOptionsNotifier final : public QObject
{
    Q_OBJECT
public:
    static ClientOptionsNotifier* instance();

Q_SIGNALS:
    /// ClientOptions::store changed the option `name`.
    void optionChanged(const QString& name);
};

/// conf/clientoptions.json, CIP's options file (TClientOptions). Defaults are
/// CIP's own file (cip/conf, embedded at :/conf); the user's copy lives in the
/// app data dir and overrides them key by key.
class ClientOptions
{
public:
    static ClientOptions load();
    static QString userFilePath();
    /// Sets one option in the user's file (TClientOptions' setters, vtable 0x80).
    static void store(const QString& name, const QJsonValue& value);
    /// Writes many settings at once (the options dialog's apply): every section
    /// of `changes` key by key, keeping the keys it does not name.
    /// Emits optionChanged for each changed option, and once per other changed section.
    static void storeAll(const QJsonObject& changes);

    const QJsonObject& root() const { return m_root; }
    /// The "options" section, keyed by CIP's option names (cip_options.h).
    QJsonObject options() const { return m_root.value(QLatin1String("options")).toObject(); }

    /// Option 0x5c: 0 auto, 3 software, 6 D3D11, 7 Vulkan, 8 OpenGL, 9 Metal, 10 D3D12.
    int rendererIndex() const;
    /// Option 0x5e.
    bool vSyncEnabled() const;

private:
    QJsonObject m_root;
};

}
